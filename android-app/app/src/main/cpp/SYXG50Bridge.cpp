// SYXG50Bridge.cpp  v2.0
// JNI bridge: madaha(Rust, S-YXG50 에뮬레이션) + AAudio
//
// 구조는 SC55Bridge.cpp와 동일한 "전용 렌더 스레드 → 락프리 링버퍼 →
// AAudio 콜백은 pop만" 패턴이다 (v1.0에서는 AAudio 콜백이 madaha_render_i16을
// 직접 호출했는데, 노트/채널이 많아 프레임당 렌더 비용이 튀는 순간 AAudio의
// 엄격한 콜백 데드라인을 놓쳐 끊김+노이즈가 났다 — 실기기에서 확인됨).
// 렌더 스레드는 데드라인이 없어 순간적으로 느려져도 링버퍼가 흡수한다.
//
//   MIDI 이벤트  : Kotlin 스레드 → 큐(뮤텍스) → MIDI 스레드 → madaha_send_*
//   오디오 렌더  : 렌더 스레드가 계속 madaha_render_i16 → 링버퍼 push
//   AAudio 콜백  : 링버퍼 pop만 (데드라인 안에서 절대 무거운 일 안 함)

#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>

#include <mutex>
#include <thread>
#include <chrono>
#include <deque>
#include <vector>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <string>

#define TAG "SYXG50Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// madaha_core(Rust) FFI 선언 — src/ffi.rs와 정확히 일치해야 한다.
// ---------------------------------------------------------------------------
extern "C" {
    int32_t madaha_init(const char* bin_path, const char* wave_path,
                         uint32_t sample_rate, uint16_t max_polyphony);
    void    madaha_destroy(void);
    int32_t madaha_is_ready(void);
    void    madaha_send_midi(uint32_t packed);
    void    madaha_send_sysex(const uint8_t* data, int32_t len);
    void    madaha_all_notes_off(void);
    void    madaha_render_i16(int16_t* out, int32_t frames);
    int32_t madaha_get_last_error(char* buf, int32_t buf_len);
}

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
static void ring_push(const StereoS16& f) {
    int h = s_ringHead.load(std::memory_order_relaxed);
    int nh = (h + 1) % RING_FRAMES;
    if (nh == s_ringTail.load(std::memory_order_acquire)) return; // full, drop
    s_ring[h] = f;
    s_ringHead.store(nh, std::memory_order_release);
}
static bool ring_pop(StereoS16& f) {
    int t = s_ringTail.load(std::memory_order_relaxed);
    if (t == s_ringHead.load(std::memory_order_acquire)) return false; // empty
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

static AAudioStream*      s_stream = nullptr;
static std::atomic<bool>  s_initialized{false};
static std::atomic<bool>  s_running{false};
static std::thread        s_midiThread;
static std::atomic<bool>  s_midiThreadRunning{false};
static std::thread        s_renderThread;
static std::atomic<bool>  s_renderThreadRunning{false};
static constexpr int      kChannels = 2;
static constexpr int      kFramesBurst = 512;
// 렌더 스레드가 한 번에 madaha_render_i16을 호출하는 프레임 수. 너무 작으면
// 호출 오버헤드가 커지고, 너무 크면 backpressure 반응이 느려진다.
static constexpr int      kRenderChunk = 256;
// 16의 배수, 32~2048. 렌더가 AAudio 콜백과 완전히 분리된(v2.0) 뒤로는
// poly_replicant(150%) 여유분까지 감안해도 안정적이라 원래 값(128)으로 복원.
static constexpr uint16_t kMaxPolyphony = 128;
static std::atomic<int>   s_actualSampleRate{44100};

// LED 패널용: 채널별 현재 활성 노트 수 / 뱅크·프로그램. nativeSendMidi가
// 큐에 넣기 직전에 peek해서 갱신한다(가벼운 정수 연산이라 렌더/MIDI 스레드
// 부담과 무관 — UI 표시 목적이라 지연 처리해도 상관없다).
static std::atomic<int>     s_channelActive[16];
static std::atomic<uint8_t> s_channelBankMsb[16];
static std::atomic<uint8_t> s_channelBankLsb[16];
static std::atomic<uint8_t> s_channelProgram[16];

// ---------------------------------------------------------------------------
// MIDI 처리 스레드 — 여기서만 madaha_send_midi/madaha_send_sysex를 호출한다.
// ---------------------------------------------------------------------------
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
                madaha_send_sysex(ev.sysex->data(), (int32_t)ev.sysex->size());
            } else {
                madaha_send_midi(ev.packed);
            }
            local.pop_front();
        }
    }
}

// ---------------------------------------------------------------------------
// 렌더 스레드: 계속 madaha_render_i16을 호출해 링버퍼를 채운다.
// AAudio의 엄격한 콜백 데드라인과 완전히 분리되어 있어, 노트/채널이 많아
// 순간적으로 렌더가 느려져도(리버브 등 항상-on 이펙트 비용 포함) 링버퍼가
// 그 지터를 흡수한다 — 콜백은 그냥 pop만 하므로 절대 데드라인을 놓치지 않는다.
// ---------------------------------------------------------------------------
static void renderLoop() {
    std::vector<int16_t> buf(kRenderChunk * kChannels);
    constexpr int kHighWater = (RING_FRAMES * 3) / 4;
    while (s_renderThreadRunning.load(std::memory_order_relaxed)) {
        // 링버퍼가 이미 충분히 차 있으면 더 만들지 않고 잠깐 쉰다
        // (렌더 스레드가 AAudio 소비 속도보다 빠를 때 CPU를 아낀다).
        while (ring_size() >= kHighWater && s_renderThreadRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        madaha_render_i16(buf.data(), kRenderChunk);
        for (int i = 0; i < kRenderChunk; ++i) {
            ring_push({buf[i * 2], buf[i * 2 + 1]});
        }
    }
}

// ---------------------------------------------------------------------------
// AAudio 데이터 콜백: 링버퍼에서 pop만 한다 (데드라인 안에서 절대 무거운
// 작업을 하지 않는다 — v1.0의 끊김+노이즈 원인이 바로 이 지점이었다).
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
        ring_pop(f); // underrun 시 무음(초기 fill-up 구간 등 드묾)
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
Java_com_example_nukedsc55_SYXG50Engine_nativeInit(
        JNIEnv* env, jobject, jstring jBinPath, jstring jWavePath)
{
    if (s_initialized.load()) return JNI_TRUE;

    const char* cBin = env->GetStringUTFChars(jBinPath, nullptr);
    const char* cWave = env->GetStringUTFChars(jWavePath, nullptr);
    std::string binPath(cBin);
    std::string wavePath(cWave);
    env->ReleaseStringUTFChars(jBinPath, cBin);
    env->ReleaseStringUTFChars(jWavePath, cWave);

    LOGI("nativeInit: bin=%s wave=%s", binPath.c_str(), wavePath.c_str());

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
        aaudio_result_t r = AAudioStreamBuilder_openStream(b, st);
        AAudioStreamBuilder_delete(b);
        return r;
    };

    aaudio_result_t res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, AAUDIO_UNSPECIFIED);
    if (res != AAUDIO_OK) {
        LOGW("SHARED@native 실패(%s), EXCLUSIVE 시도", AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_EXCLUSIVE, AAUDIO_UNSPECIFIED);
    }
    if (res != AAUDIO_OK) {
        LOGW("EXCLUSIVE@native 실패(%s), 48kHz SHARED 시도", AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, 48000);
    }
    if (res != AAUDIO_OK) {
        LOGE("AAudio open 실패: %s", AAudio_convertResultToText(res));
        return JNI_FALSE;
    }

    int32_t actualRate = AAudioStream_getSampleRate(s_stream);
    s_actualSampleRate.store(actualRate);

    int32_t burst = AAudioStream_getFramesPerBurst(s_stream);
    int32_t capacity = AAudioStream_getBufferCapacityInFrames(s_stream);
    int32_t targetBuffer = burst * 4;
    if (targetBuffer > capacity) targetBuffer = capacity;
    if (targetBuffer > 0) AAudioStream_setBufferSizeInFrames(s_stream, targetBuffer);
    LOGI("AAudio burst=%d capacity=%d bufferSize->%d", burst, capacity, targetBuffer);

    if (!madaha_init(binPath.c_str(), wavePath.c_str(), (uint32_t)actualRate, kMaxPolyphony)) {
        char err[256] = {0};
        madaha_get_last_error(err, sizeof(err));
        LOGE("madaha_init 실패: %s", err);
        AAudioStream_close(s_stream); s_stream = nullptr;
        return JNI_FALSE;
    }

    LOGI("AAudio opened @ %d Hz, madaha_core 초기화 완료", actualRate);
    for (int ch = 0; ch < 16; ++ch) {
        s_channelActive[ch].store(0);
        s_channelBankMsb[ch].store(0);
        s_channelBankLsb[ch].store(0);
        s_channelProgram[ch].store(0);
    }
    s_initialized.store(true);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeStart(JNIEnv*, jobject)
{
    if (!s_initialized.load() || s_running.load()) return;
    s_running.store(true);
    ring_reset();
    s_midiThreadRunning.store(true);
    s_midiThread = std::thread(midiThreadLoop);
    s_renderThreadRunning.store(true);
    s_renderThread = std::thread(renderLoop);
    // 렌더 스레드가 링버퍼를 어느 정도 채울 때까지 짧게 대기 — 스트림 시작
    // 직후 몇 콜백이 무음으로 시작되는 것을 방지(진짜 언더런은 아니지만
    // 첫 순간 딸깍거림을 줄여준다).
    for (int i = 0; i < 20 && ring_size() < kFramesBurst * 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    AAudioStream_requestStart(s_stream);
    LOGI("nativeStart: AAudio + 렌더 스레드 + MIDI 스레드 시작 (ring=%d)", ring_size());
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeStop(JNIEnv*, jobject)
{
    if (!s_running.load()) return;
    s_running.store(false);
    if (s_stream) AAudioStream_requestStop(s_stream);
    s_midiThreadRunning.store(false);
    if (s_midiThread.joinable()) s_midiThread.join();
    s_renderThreadRunning.store(false);
    if (s_renderThread.joinable()) s_renderThread.join();
    LOGI("nativeStop");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeTerm(JNIEnv*, jobject)
{
    if (!s_initialized.load()) return;
    if (s_running.load()) {
        s_running.store(false);
        if (s_stream) AAudioStream_requestStop(s_stream);
        s_midiThreadRunning.store(false);
        if (s_midiThread.joinable()) s_midiThread.join();
        s_renderThreadRunning.store(false);
        if (s_renderThread.joinable()) s_renderThread.join();
    }
    if (s_stream) { AAudioStream_close(s_stream); s_stream = nullptr; }
    madaha_destroy();
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
    ring_reset();
    s_initialized.store(false);
    LOGI("nativeTerm");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeSendMidi(JNIEnv*, jobject, jint packed)
{
    if (!s_initialized.load()) return;
    // LED 패널용 상태를 큐잉과 별개로 즉시 peek-갱신 (UI 표시 목적, 지연 무관).
    uint8_t status = (uint8_t)(packed & 0xFF);
    uint8_t d1 = (uint8_t)((packed >> 8) & 0xFF);
    uint8_t d2 = (uint8_t)((packed >> 16) & 0xFF);
    uint8_t ty = status & 0xF0;
    uint8_t ch = status & 0x0F;
    switch (ty) {
        case 0x90:
            if (d2 > 0) s_channelActive[ch].fetch_add(1);
            else if (s_channelActive[ch].load() > 0) s_channelActive[ch].fetch_sub(1);
            break;
        case 0x80:
            if (s_channelActive[ch].load() > 0) s_channelActive[ch].fetch_sub(1);
            break;
        case 0xB0:
            if (d1 == 0) s_channelBankMsb[ch].store(d2);
            else if (d1 == 32) s_channelBankLsb[ch].store(d2);
            break;
        case 0xC0:
            s_channelProgram[ch].store(d1);
            break;
        default: break;
    }
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({false, (uint32_t)packed, nullptr});
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeSendSysEx(JNIEnv* env, jobject, jbyteArray data, jint len)
{
    if (!s_initialized.load() || len <= 0) return;
    jbyte* buf = env->GetByteArrayElements(data, nullptr);
    auto v = std::make_shared<std::vector<uint8_t>>((uint8_t*)buf, (uint8_t*)buf + len);
    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({true, 0, v});
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeIsReady(JNIEnv*, jobject)
{
    return (s_initialized.load() && madaha_is_ready()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeGetSampleRate(JNIEnv*, jobject)
{
    return (jint)s_actualSampleRate.load();
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeGetVersion(JNIEnv* env, jobject)
{
    return env->NewStringUTF("madaha(S-YXG50) bridge v2.0 (dedicated render thread + ring buffer)");
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeGetLastError(JNIEnv* env, jobject)
{
    char err[256] = {0};
    madaha_get_last_error(err, sizeof(err));
    return env->NewStringUTF(err);
}

// LED 패널용: 비트 i = 채널 i에 지금 소리 나는 노트가 있는지 여부.
JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeGetChannelActiveMask(JNIEnv*, jobject)
{
    if (!s_initialized.load()) return 0;
    jint mask = 0;
    for (int ch = 0; ch < 16; ++ch) {
        if (s_channelActive[ch].load() > 0) mask |= (1 << ch);
    }
    return mask;
}

// LED 패널용: 채널의 현재 뱅크/프로그램을 짧은 텍스트로. S-YXG50 ROM에는
// SoundFont와 달리 사람이 읽는 악기명 테이블이 없어서(TBL 포맷 한계),
// 숫자로 표시한다(사용자가 뱅크/프로그램 번호로 어떤 악기인지 유추 가능).
JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SYXG50Engine_nativeGetChannelProgramText(JNIEnv* env, jobject, jint channel)
{
    if (!s_initialized.load() || channel < 0 || channel > 15) return env->NewStringUTF("---");
    char buf[24];
    snprintf(buf, sizeof(buf), "B%d.%d P%d",
             s_channelBankMsb[channel].load(), s_channelBankLsb[channel].load(),
             s_channelProgram[channel].load());
    return env->NewStringUTF(buf);
}

} // extern "C"
