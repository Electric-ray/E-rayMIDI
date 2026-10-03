// FluidBridge.cpp
// JNI bridge: FluidSynth(SF2 사운드폰트) + AAudio
//
// 원래 TinySoundFont(tsf.h)를 쓰던 걸 FluidSynth로 교체했다. 구조는
// SYXG50Bridge.cpp/SC55Bridge.cpp와 동일한 "전용 렌더 스레드 → 링버퍼 →
// AAudio 콜백은 pop만" 패턴 — AAudio 콜백 안에서 직접 무거운 연산을 하면
// 노트가 많을 때 데드라인을 놓쳐 끊김+노이즈가 난다는 게 이미 다른 엔진들
// 통합하면서 실기기로 확인된 사실이라, 처음부터 이 구조로 간다.
//
//   MIDI 이벤트  : Kotlin 스레드 → 큐(뮤텍스) → MIDI 스레드 → fluid_synth_*
//   오디오 렌더  : 렌더 스레드가 계속 fluid_synth_write_s16 → 링버퍼 push
//   AAudio 콜백  : 링버퍼 pop만

#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>
#include "AAudioRecover.h"
#include <fluidsynth.h>

#include <mutex>
#include <thread>
#include <chrono>
#include <deque>
#include <vector>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <string>

#define TAG "FluidBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// 오디오 링버퍼 (렌더 스레드 → AAudio 콜백, 락프리)
// ---------------------------------------------------------------------------
static constexpr int RING_FRAMES = 16384;
struct StereoS16 { int16_t l, r; };
static StereoS16         s_ring[RING_FRAMES];
static std::atomic<int>  s_ringHead{0};
static std::atomic<int>  s_ringTail{0};

static inline int ring_size() {
    return (s_ringHead.load(std::memory_order_acquire) -
            s_ringTail.load(std::memory_order_acquire) + RING_FRAMES) % RING_FRAMES;
}
static void ring_push_block(const int16_t* interleaved, int frames) {
    for (int i = 0; i < frames; ++i) {
        int h = s_ringHead.load(std::memory_order_relaxed);
        int nh = (h + 1) % RING_FRAMES;
        if (nh == s_ringTail.load(std::memory_order_acquire)) return; // full, drop
        s_ring[h] = {interleaved[i * 2], interleaved[i * 2 + 1]};
        s_ringHead.store(nh, std::memory_order_release);
    }
}
static bool ring_pop(StereoS16& f) {
    int t = s_ringTail.load(std::memory_order_relaxed);
    if (t == s_ringHead.load(std::memory_order_acquire)) return false;
    f = s_ring[t];
    s_ringTail.store((t + 1) % RING_FRAMES, std::memory_order_release);
    return true;
}
static void ring_reset() {
    s_ringHead.store(0, std::memory_order_relaxed);
    s_ringTail.store(0, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// MIDI 이벤트 큐 (Kotlin 스레드 -> MIDI 처리 스레드)
// ---------------------------------------------------------------------------
struct MidiEv {
    bool isSysex;
    uint32_t packed;
    std::shared_ptr<std::vector<uint8_t>> sysex;
};
static std::deque<MidiEv> g_evQ;
static std::mutex         g_evMtx;

static fluid_settings_t*  s_settings = nullptr;
static fluid_synth_t*     s_synth = nullptr;
static int                s_sfontId = -1;
static AAudioStream*      s_stream = nullptr;
static int32_t            s_openRate = 48000;    // 스트림이 실제로 열린 레이트 (FluidSynth 레이트와 같다)
static eray::AAudioRecover s_recover;            // 출력 경로 변경(블루투스 통화 등)으로 끊긴 스트림 자동 복구
static std::atomic<bool>  s_initialized{false};
static std::atomic<bool>  s_running{false};
static std::thread        s_midiThread;
static std::atomic<bool>  s_midiThreadRunning{false};
static std::thread        s_renderThread;
static std::atomic<bool>  s_renderThreadRunning{false};
static constexpr int      kChannels = 2;
static constexpr int      kFramesBurst = 512;
static constexpr int      kRenderChunk = 256;
static constexpr int      kPolyphony = 128;
static std::atomic<int>   s_actualSampleRate{44100};

// LED 패널용: 채널별 현재 활성 노트 수.
static std::atomic<int>   s_channelActive[16];

// ---------------------------------------------------------------------------
// MIDI 처리 스레드 — 여기서만 fluid_synth_* 이벤트 함수를 호출한다.
// ---------------------------------------------------------------------------
static void handleMidiPacked(uint32_t packed) {
    uint8_t status = (uint8_t)(packed & 0xFF);
    uint8_t d1 = (uint8_t)((packed >> 8) & 0xFF);
    uint8_t d2 = (uint8_t)((packed >> 16) & 0xFF);
    uint8_t ty = status & 0xF0;
    uint8_t ch = status & 0x0F;
    switch (ty) {
        case 0x80:
            fluid_synth_noteoff(s_synth, ch, d1);
            if (s_channelActive[ch].load() > 0) s_channelActive[ch].fetch_sub(1);
            break;
        case 0x90:
            if (d2 > 0) {
                fluid_synth_noteon(s_synth, ch, d1, d2);
                s_channelActive[ch].fetch_add(1);
            } else {
                fluid_synth_noteoff(s_synth, ch, d1);
                if (s_channelActive[ch].load() > 0) s_channelActive[ch].fetch_sub(1);
            }
            break;
        case 0xA0:
            fluid_synth_key_pressure(s_synth, ch, d1, d2);
            break;
        case 0xB0:
            fluid_synth_cc(s_synth, ch, d1, d2);
            break;
        case 0xC0:
            fluid_synth_program_change(s_synth, ch, d1);
            break;
        case 0xD0:
            fluid_synth_channel_pressure(s_synth, ch, d1);
            break;
        case 0xE0:
            fluid_synth_pitch_bend(s_synth, ch, (d1 & 0x7F) | ((d2 & 0x7F) << 7));
            break;
        default: break;
    }
}

static void midiThreadLoop() {
    while (s_midiThreadRunning.load(std::memory_order_relaxed)) {
        std::deque<MidiEv> local;
        {
            std::lock_guard<std::mutex> lk(g_evMtx);
            local.swap(g_evQ);
        }
        if (local.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        while (!local.empty()) {
            const MidiEv& ev = local.front();
            if (ev.isSysex && ev.sysex) {
                // FluidSynth의 fluid_synth_sysex()는 0xF0/0xF7 프레이밍 없이
                // 제조사ID부터 시작하는 페이로드를 기대한다 — 있으면 벗겨낸다.
                const uint8_t* p = ev.sysex->data();
                int n = (int)ev.sysex->size();
                if (n > 0 && p[0] == 0xF0) { p++; n--; }
                if (n > 0 && p[n - 1] == 0xF7) n--;
                if (n > 0) {
                    fluid_synth_sysex(s_synth, reinterpret_cast<const char*>(p), n,
                                       nullptr, nullptr, nullptr, 0);
                }
            } else {
                handleMidiPacked(ev.packed);
            }
            local.pop_front();
        }
    }
}

// ---------------------------------------------------------------------------
// 렌더 스레드: fluid_synth_write_s16을 인터리브 모드로 계속 호출해
// 링버퍼를 채운다. AAudio 콜백의 엄격한 데드라인과 완전히 분리되어 있어
// 순간적으로 렌더가 느려져도(리버브 등) 링버퍼가 지터를 흡수한다.
// ---------------------------------------------------------------------------
static void renderLoop() {
    std::vector<int16_t> buf(kRenderChunk * kChannels);
    constexpr int kHighWater = (RING_FRAMES * 3) / 4;
    while (s_renderThreadRunning.load(std::memory_order_relaxed)) {
        while (ring_size() >= kHighWater && s_renderThreadRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        // 인터리브 트릭: L/R 둘 다 같은 버퍼를 가리키되 offset/increment로
        // L=짝수 인덱스, R=홀수 인덱스에 쓰게 한다 (fluid_synth_write_s16 문서 패턴).
        fluid_synth_write_s16(s_synth, kRenderChunk,
                               buf.data(), 0, 2,
                               buf.data(), 1, 2);
        ring_push_block(buf.data(), kRenderChunk);
    }
}

// ---------------------------------------------------------------------------
// AAudio 데이터 콜백: 링버퍼에서 pop만 한다.
// ---------------------------------------------------------------------------
static aaudio_data_callback_result_t audioCallback(
        AAudioStream*, void*, void* audioData, int32_t numFrames)
{
    auto* out = static_cast<int16_t*>(audioData);
    if (!s_initialized.load(std::memory_order_relaxed)) {
        std::memset(audioData, 0, (size_t)numFrames * kChannels * sizeof(int16_t));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    for (int i = 0; i < numFrames; ++i) {
        StereoS16 f{0, 0};
        ring_pop(f);
        out[i * 2 + 0] = f.l;
        out[i * 2 + 1] = f.r;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

// ---------------------------------------------------------------------------
// JNI exports
// ---------------------------------------------------------------------------
extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeInit(JNIEnv* env, jobject, jstring jSf2Path)
{
    if (s_initialized.load()) return JNI_TRUE;

    const char* cPath = env->GetStringUTFChars(jSf2Path, nullptr);
    std::string sf2Path(cPath);
    env->ReleaseStringUTFChars(jSf2Path, cPath);
    LOGI("nativeInit: sf2=%s", sf2Path.c_str());

    auto openStream = [](AAudioStream** st, aaudio_sharing_mode_t mode, int32_t rate) {
        AAudioStreamBuilder* b = nullptr;
        AAudio_createStreamBuilder(&b);
        AAudioStreamBuilder_setDirection           (b, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setPerformanceMode     (b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode         (b, mode);
        AAudioStreamBuilder_setSampleRate          (b, rate);
        AAudioStreamBuilder_setChannelCount        (b, kChannels);
        AAudioStreamBuilder_setFormat              (b, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setFramesPerDataCallback(b, kFramesBurst);
        AAudioStreamBuilder_setDataCallback        (b, audioCallback, nullptr);
        AAudioStreamBuilder_setErrorCallback       (b, eray::AAudioRecover::onError, &s_recover);
        aaudio_result_t r = AAudioStreamBuilder_openStream(b, st);
        AAudioStreamBuilder_delete(b);
        return r;
    };

    aaudio_result_t res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, AAUDIO_UNSPECIFIED);
    if (res != AAUDIO_OK) {
        LOGW("SHARED 실패(%s), EXCLUSIVE 시도", AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_EXCLUSIVE, AAUDIO_UNSPECIFIED);
    }
    if (res != AAUDIO_OK) {
        LOGW("EXCLUSIVE 실패(%s), 48kHz SHARED 시도", AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, 48000);
    }
    if (res != AAUDIO_OK) {
        LOGE("AAudio open 실패: %s", AAudio_convertResultToText(res));
        return JNI_FALSE;
    }

    int32_t actualRate = AAudioStream_getSampleRate(s_stream);
    s_openRate = actualRate;
    s_actualSampleRate.store(actualRate);

    int32_t burst = AAudioStream_getFramesPerBurst(s_stream);
    int32_t capacity = AAudioStream_getBufferCapacityInFrames(s_stream);
    int32_t targetBuffer = burst * 4;
    if (targetBuffer > capacity) targetBuffer = capacity;
    if (targetBuffer > 0) AAudioStream_setBufferSizeInFrames(s_stream, targetBuffer);

    s_settings = new_fluid_settings();
    fluid_settings_setnum(s_settings, "synth.sample-rate", actualRate);
    fluid_settings_setint(s_settings, "synth.polyphony", kPolyphony);
    fluid_settings_setint(s_settings, "synth.midi-channels", 16);
    // 헤드룸: FluidSynth 기본 gain(0.2)은 안전하지만, 다른 엔진들과의 체감
    // 볼륨 균형 및 하드 클리핑 방지를 위해 소폭 조정.
    fluid_settings_setnum(s_settings, "synth.gain", 0.6);

    s_synth = new_fluid_synth(s_settings);
    if (!s_synth) {
        LOGE("new_fluid_synth 실패");
        AAudioStream_close(s_stream); s_stream = nullptr;
        delete_fluid_settings(s_settings); s_settings = nullptr;
        return JNI_FALSE;
    }

    s_sfontId = fluid_synth_sfload(s_synth, sf2Path.c_str(), 1);
    if (s_sfontId == FLUID_FAILED) {
        LOGE("fluid_synth_sfload 실패: %s", sf2Path.c_str());
        delete_fluid_synth(s_synth); s_synth = nullptr;
        delete_fluid_settings(s_settings); s_settings = nullptr;
        AAudioStream_close(s_stream); s_stream = nullptr;
        return JNI_FALSE;
    }

    for (int ch = 0; ch < 16; ++ch) s_channelActive[ch].store(0);
    LOGI("AAudio opened @ %d Hz, FluidSynth 초기화 완료 (sfontId=%d)", actualRate, s_sfontId);
    s_initialized.store(true);
    return JNI_TRUE;
}

// AAudioRecover: 출력 경로가 바뀌어(블루투스 통화 후 A2DP 복귀 등) 스트림이 끊겼을 때,
// 처음 열렸던 레이트 그대로 다시 연다. 엔진 내부 레이트/리샘플 설정이 그대로 유효하도록 같은 레이트를 고집한다.
static aaudio_result_t reopenAudioStream(AAudioStream** out) {
    aaudio_result_t r = AAUDIO_ERROR_UNAVAILABLE;
    for (aaudio_sharing_mode_t mode : {AAUDIO_SHARING_MODE_SHARED, AAUDIO_SHARING_MODE_EXCLUSIVE}) {
        AAudioStreamBuilder* b = nullptr;
        AAudio_createStreamBuilder(&b);
        AAudioStreamBuilder_setDirection           (b, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setPerformanceMode     (b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode         (b, mode);
        AAudioStreamBuilder_setSampleRate          (b, s_openRate);
        AAudioStreamBuilder_setChannelCount        (b, kChannels);
        AAudioStreamBuilder_setFormat              (b, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setFramesPerDataCallback(b, kFramesBurst);
        AAudioStreamBuilder_setDataCallback        (b, audioCallback, nullptr);
        AAudioStreamBuilder_setErrorCallback       (b, eray::AAudioRecover::onError, &s_recover);
        r = AAudioStreamBuilder_openStream(b, out);
        AAudioStreamBuilder_delete(b);
        if (r == AAUDIO_OK) return r;
    }
    return r;
}
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeStart(JNIEnv*, jobject)
{
    if (!s_initialized.load() || s_running.load()) return;
    s_running.store(true);
    ring_reset();
    s_midiThreadRunning.store(true);
    s_midiThread = std::thread(midiThreadLoop);
    s_renderThreadRunning.store(true);
    s_renderThread = std::thread(renderLoop);
    for (int i = 0; i < 20 && ring_size() < kFramesBurst * 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    AAudioStream_requestStart(s_stream);
    s_recover.arm(&s_stream, [](AAudioStream** o) { return reopenAudioStream(o); });
    LOGI("nativeStart: AAudio + 렌더 스레드 + MIDI 스레드 시작 (ring=%d)", ring_size());
}

// 출력 경로가 바뀐 뒤(통화 종료 등) Kotlin에서 "스트림을 다시 열어라" 요청
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeRestartAudio(JNIEnv*, jobject)
{
    if (s_running.load()) s_recover.requestRestart();
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeStop(JNIEnv*, jobject)
{
    if (!s_running.load()) return;
    s_recover.disarm();
    s_running.store(false);
    if (s_stream) AAudioStream_requestStop(s_stream);
    s_midiThreadRunning.store(false);
    if (s_midiThread.joinable()) s_midiThread.join();
    s_renderThreadRunning.store(false);
    if (s_renderThread.joinable()) s_renderThread.join();
    LOGI("nativeStop");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeTerm(JNIEnv*, jobject)
{
    if (!s_initialized.load()) return;
    s_recover.disarm();
    if (s_running.load()) {
        s_running.store(false);
        if (s_stream) AAudioStream_requestStop(s_stream);
        s_midiThreadRunning.store(false);
        if (s_midiThread.joinable()) s_midiThread.join();
        s_renderThreadRunning.store(false);
        if (s_renderThread.joinable()) s_renderThread.join();
    }
    if (s_stream) { AAudioStream_close(s_stream); s_stream = nullptr; }
    if (s_synth) { delete_fluid_synth(s_synth); s_synth = nullptr; }
    if (s_settings) { delete_fluid_settings(s_settings); s_settings = nullptr; }
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
    ring_reset();
    s_initialized.store(false);
    LOGI("nativeTerm");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeSendMidi(JNIEnv*, jobject, jint packed)
{
    if (!s_initialized.load()) return;
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({false, (uint32_t)packed, nullptr});
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeSendSysEx(JNIEnv* env, jobject, jbyteArray data, jint len)
{
    if (!s_initialized.load() || len <= 0) return;
    jbyte* buf = env->GetByteArrayElements(data, nullptr);
    auto v = std::make_shared<std::vector<uint8_t>>((uint8_t*)buf, (uint8_t*)buf + len);
    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({true, 0, v});
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeIsReady(JNIEnv*, jobject)
{
    return (s_initialized.load() && s_synth != nullptr) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetActiveVoices(JNIEnv*, jobject)
{
    if (!s_initialized.load() || !s_synth) return 0;
    return fluid_synth_get_active_voice_count(s_synth);
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetPresetCount(JNIEnv*, jobject)
{
    if (!s_initialized.load() || !s_synth) return 0;
    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id(s_synth, s_sfontId);
    if (!sfont) return 0;
    int count = 0;
    fluid_sfont_iteration_start(sfont);
    while (fluid_sfont_iteration_next(sfont) != nullptr) count++;
    return count;
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetChannelPresetName(JNIEnv* env, jobject, jint channel)
{
    if (!s_initialized.load() || !s_synth || channel < 0 || channel > 15) {
        return env->NewStringUTF("");
    }
    fluid_preset_t* preset = fluid_synth_get_channel_preset(s_synth, channel);
    if (!preset) return env->NewStringUTF("");
    const char* name = fluid_preset_get_name(preset);
    return env->NewStringUTF(name ? name : "");
}

// LED 패널용: 비트 i = 채널 i에 지금 소리 나는 노트가 있는지 여부.
JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetChannelActiveMask(JNIEnv*, jobject)
{
    if (!s_initialized.load()) return 0;
    jint mask = 0;
    for (int ch = 0; ch < 16; ++ch) {
        if (s_channelActive[ch].load() > 0) mask |= (1 << ch);
    }
    return mask;
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetVersion(JNIEnv* env, jobject)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "FluidSynth %s bridge (dedicated render thread + ring buffer)",
             FLUIDSYNTH_VERSION);
    return env->NewStringUTF(buf);
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetSampleRate(JNIEnv*, jobject)
{
    return (jint)s_actualSampleRate.load();
}

} // extern "C"
