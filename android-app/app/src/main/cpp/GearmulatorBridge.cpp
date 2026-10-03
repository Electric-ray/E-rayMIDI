// GearmulatorBridge.cpp
// JNI bridge: 88emu (dsp56300/gearmulator 88lib) + AAudio
//
// 다른 엔진들(SC55/Munt/SoundFont/S-YXG50)과 동일한 구조로 통일했다:
//   MIDI 이벤트  : Kotlin 스레드 → 큐(뮤텍스) → 렌더 스레드에서 드레인
//   오디오 렌더  : 렌더 스레드가 계속 emu88_render_bit16s() → 링버퍼(락프리) push
//   AAudio 콜백  : 링버퍼 pop만 (무거운 연산 절대 안 함)
//
// SC55Bridge/FluidBridge와 다른 점 하나: 88lib의 emu88_context는 스레드
// 세이프하지 않다("a context is not thread-safe; use it from one thread at
// a time" — c_interface.h). FluidSynth는 스레드 세이프해서 MIDI 처리
// 스레드와 렌더 스레드를 완전히 분리했지만, 88lib는 그럴 수 없어서
// SC55Bridge의 mcuLoop처럼 "MIDI 드레인 + 렌더"를 같은 스레드(렌더 스레드)
// 안에서 순서대로 처리한다 — 그래도 AAudio 콜백과는 링버퍼로 완전히
// 분리되므로, 렌더가 순간적으로 느려져도(리버브 등) AAudio 데드라인을
// 놓치지 않는다.
//
// 리샘플링은 emu88_set_stereo_output_samplerate()에 AAudio가 실제로 내준
// 레이트를 넘기면 88lib 내부에서 처리해준다 — 별도 수동 리샘플러 불필요.

#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>
#include <android/bitmap.h>

#include "AAudioRecover.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <chrono>
#include <cstring>
#include <cstdint>

#include "gearmulator/GearmulatorEngine.h"

#define TAG "GearmulatorBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using erayMidi::GearmulatorEngine;
using erayMidi::Lcd88Raw;

// ---------------------------------------------------------------------------
// 오디오 링버퍼 (렌더 스레드 → AAudio 콜백, 락프리 — 다른 엔진들과 동일한 구조)
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
// MIDI 이벤트 큐 (SC55Bridge.cpp와 동일한 mutex+deque 패턴)
// ---------------------------------------------------------------------------
struct MidiEv {
    bool isSysex;
    uint32_t packed;
    std::shared_ptr<std::vector<uint8_t>> sysex;
};
static std::deque<MidiEv>    g_evQ;
static std::mutex            g_evMtx;
static std::atomic<uint64_t> g_midiCount{0};
static std::atomic<uint64_t> g_sysexCount{0};

static void drainMidiQueue(GearmulatorEngine& engine) {
    std::deque<MidiEv> local;
    {
        std::lock_guard<std::mutex> lk(g_evMtx);
        local.swap(g_evQ); // 락은 이 swap 동안만 (마이크로초 단위)
    }
    while (!local.empty()) {
        const MidiEv& ev = local.front();
        if (ev.isSysex && ev.sysex) {
            engine.sendSysEx(ev.sysex->data(), ev.sysex->size());
        } else {
            uint8_t st = ev.packed & 0xFF;
            uint8_t d1 = (ev.packed >> 8) & 0xFF;
            uint8_t d2 = (ev.packed >> 16) & 0xFF;
            engine.sendMidi(st, d1, d2);
        }
        local.pop_front();
    }
}

// ---------------------------------------------------------------------------
// 전역 상태
// ---------------------------------------------------------------------------
static std::unique_ptr<GearmulatorEngine> s_engine;
static AAudioStream*     s_stream = nullptr;
static eray::AAudioRecover s_recover;   // 출력 경로 변경(블루투스 통화 등)으로 끊긴 스트림 자동 복구
static std::thread       s_renderThread;
static std::atomic<bool> s_renderThreadRunning{false};
static std::atomic<bool> s_bootDone{false};
static std::atomic<bool> s_running{false};
static bool              s_initialized = false;
static uint32_t          s_actualRate = 44100; // AAudio가 실제로 내준 레이트
static constexpr int     kRenderChunk = 256;    // 렌더 스레드가 한 번에 emu88_render_bit16s에 요청하는 프레임 수

// ---------------------------------------------------------------------------
// LCD 스냅샷. 88lib 컨텍스트는 스레드 세이프하지 않으므로, 원시 LCD 내용(DDRAM/CGRAM 또는
// 도트 그리드, 수십~수천 바이트)만 렌더 스레드가 ~30Hz로 복사해 두고, 픽셀 합성은
// JNI(nativeGetLcdFrame, Kotlin 백그라운드 스레드)에서 한다 — 오디오 렌더 스레드에는
// 비트맵 합성 비용을 얹지 않는다. 내용이 바뀐 경우에만 s_lcdSeq를 올려서 UI가
// 변경 없는 프레임을 건너뛸 수 있게 한다.
// ---------------------------------------------------------------------------
static std::mutex            s_lcdMtx;
static Lcd88Raw              s_lcdRaw;          // 소비자(JNI)용 최신 스냅샷
static Lcd88Raw              s_lcdTmp;          // 렌더 스레드 전용 작업 버퍼
static std::atomic<uint64_t> s_lcdSeq{0};
static constexpr int         kLcdIntervalMs = 33;

// 렌더 스레드에서만 호출
static void pollLcd() {
    Lcd88Raw& tmp = s_lcdTmp;
    if (!s_engine->getDisplayRaw(0, tmp)) { tmp.type = 0; tmp.monoLen = 0; }
    tmp.look = s_engine->lcdLook();
    std::lock_guard<std::mutex> lk(s_lcdMtx);
    if (!erayMidi::lcd88Equal(tmp, s_lcdRaw)) {
        s_lcdRaw = tmp;
        s_lcdSeq.fetch_add(1, std::memory_order_release);
    }
}

static void clearLcd() {
    std::lock_guard<std::mutex> lk(s_lcdMtx);
    s_lcdRaw.type = 0;
    s_lcdRaw.monoLen = 0;
    s_lcdSeq.fetch_add(1, std::memory_order_release);
}

// AAudio 콜백: 링버퍼에서 pop만 한다. 여기서 emu88_render_bit16s를 직접
// 부르던 이전 버전이 버벅임의 원인이었을 가능성이 높다 — 노트가 몰릴 때
// 렌더링이 콜백의 엄격한 데드라인을 넘기면 끊김+노이즈가 남는다.
static aaudio_data_callback_result_t audioCallback(
        AAudioStream*, void*, void* audioData, int32_t numFrames)
{
    auto* out = static_cast<int16_t*>(audioData);
    for (int i = 0; i < numFrames; ++i) {
        StereoS16 f{};
        if (!ring_pop(f)) f = {0, 0}; // 언더런: 무음 (렌더 스레드가 아직 못 채움)
        out[i * 2 + 0] = f.l;
        out[i * 2 + 1] = f.r;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

// AAudio 스트림을 지정된 샘플레이트로 열어본다.
static aaudio_result_t openStream(AAudioStream** stream, aaudio_sharing_mode_t mode, int32_t rate) {
    AAudioStreamBuilder* builder = nullptr;
    AAudio_createStreamBuilder(&builder);
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, rate);
    AAudioStreamBuilder_setSharingMode(builder, mode);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(builder, audioCallback, nullptr);
    AAudioStreamBuilder_setErrorCallback(builder, eray::AAudioRecover::onError, &s_recover);
    aaudio_result_t res = AAudioStreamBuilder_openStream(builder, stream);
    AAudioStreamBuilder_delete(builder);
    return res;
}

// ---------------------------------------------------------------------------
// 렌더 스레드: 부팅(블로킹) 후 계속 emu88_render_bit16s()를 청크 단위로 호출해
// 링버퍼를 채운다. 88lib의 emu88_context는 스레드 세이프하지 않으므로
// (c_interface.h: "use it from one thread at a time"), MIDI 드레인도 반드시
// 이 스레드에서만 한다 — 렌더 호출 "직전에" 반영되어야 하는 순서 요구사항과도
// 맞다("Everything played is delivered with the next frame rendered").
// ---------------------------------------------------------------------------
static void renderLoop(uint32_t desiredRate) {
    LOGI("render thread: opening synth (device=%s)...", s_engine->deviceName());
    if (!s_engine->init()) {
        LOGE("render thread: emu88_open_synth 실패 (ROM 누락/손상 가능성)");
        return;
    }
    uint32_t nativeRate = s_engine->actualSampleRate();
    LOGI("render thread: 부팅 완료. 디바이스 네이티브 레이트=%u Hz, AAudio 요청 레이트=%u Hz",
         nativeRate, desiredRate);
    s_bootDone.store(true, std::memory_order_release);

    std::vector<int16_t> buf(kRenderChunk * 2);
    constexpr int kHighWater = (RING_FRAMES * 3) / 4;
    auto lastLcdPoll = std::chrono::steady_clock::now() - std::chrono::milliseconds(kLcdIntervalMs);

    while (s_renderThreadRunning.load(std::memory_order_relaxed)) {
        // 링버퍼가 이미 충분히 차 있으면 잠깐 쉰다 (AAudio 콜백이 소비하는
        // 속도에 맞춰 렌더 속도를 자연스럽게 조절 — 다른 엔진들과 동일).
        while (ring_size() >= kHighWater && s_renderThreadRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        drainMidiQueue(*s_engine);
        s_engine->renderInt16(buf.data(), kRenderChunk);
        ring_push_block(buf.data(), kRenderChunk);

        const auto now = std::chrono::steady_clock::now();
        if (now - lastLcdPoll >= std::chrono::milliseconds(kLcdIntervalMs)) {
            lastLcdPoll = now;
            pollLcd();
        }
    }
}

extern "C" {

// nativeInit: ROM 검색 경로 등록만 수행 (빠름)
JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeInit(
        JNIEnv* env, jobject, jstring jRomDir, jstring jDeviceCliId)
{
    const char* cDir = env->GetStringUTFChars(jRomDir, nullptr);
    std::string romDir(cDir);
    env->ReleaseStringUTFChars(jRomDir, cDir);

    const char* cDevId = env->GetStringUTFChars(jDeviceCliId, nullptr);
    std::string deviceCliId(cDevId);
    env->ReleaseStringUTFChars(jDeviceCliId, cDevId);

    emu88_device_id deviceId;
    if (!erayMidi::resolveDeviceId(deviceCliId, &deviceId)) {
        LOGE("nativeInit: 알 수 없는 device id '%s'", deviceCliId.c_str());
        return JNI_FALSE;
    }

    s_engine = std::make_unique<GearmulatorEngine>(deviceId);
    s_engine->addRomPath(romDir);
    LOGI("nativeInit: device=%s romDir=%s", s_engine->deviceName(), romDir.c_str());
    s_initialized = true;
    s_bootDone.store(false);
    return JNI_TRUE;
}

// nativeStart: AAudio 오픈(가능하면 디바이스 네이티브 레이트로) + 렌더 스레드 시작
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeStart(JNIEnv*, jobject)
{
    if (!s_initialized || !s_engine || s_running.load()) return;

    // SC55Engine과 같은 원칙: 가능하면 리샘플링 없이 디바이스 고유 레이트로 연다.
    // emu88_get_device_samplerate()는 open 전이라 0을 반환하므로, 흔한 두 값
    // (66207Hz: SC-55/88 계열, 32000Hz: MT-32 계열)을 먼저 시도하고 안 되면
    // AAudio 흔한 기본값 + 88lib 내부 리샘플러로 폴백한다.
    static const int32_t kTryRates[] = {66207, 32000, 48000, 44100};
    aaudio_result_t res = AAUDIO_ERROR_INVALID_STATE;
    for (int32_t rate : kTryRates) {
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, rate);
        if (res == AAUDIO_OK) break;
    }
    if (res != AAUDIO_OK) {
        LOGE("AAudio open 실패: %s", AAudio_convertResultToText(res));
        return;
    }

    s_actualRate = AAudioStream_getSampleRate(s_stream);
    // 88lib에 우리가 실제로 받은 레이트를 알려준다. 그 값이 디바이스 고유 레이트와
    // 같으면 내부적으로 리샘플링을 건너뛰고, 다르면 88lib가 알아서 리샘플링한다.
    s_engine->setOutputSampleRate((double)s_actualRate);

    ring_reset();
    s_running = true;
    s_bootDone.store(false);
    s_renderThreadRunning = true;
    s_renderThread = std::thread(renderLoop, s_actualRate);
    AAudioStream_requestStart(s_stream);
    s_recover.arm(&s_stream, [](AAudioStream** o) {
        return openStream(o, AAUDIO_SHARING_MODE_SHARED, (int32_t)s_actualRate);   // 같은 레이트로 (피치 유지)
    });
    LOGI("nativeStart: AAudio @ %u Hz, 렌더 스레드 시작", s_actualRate);
}

// 출력 경로가 바뀐 뒤(통화 종료 등) Kotlin에서 "스트림을 다시 열어라" 요청
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeRestartAudio(JNIEnv*, jobject)
{
    if (s_running.load()) s_recover.requestRestart();
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeStop(JNIEnv*, jobject)
{
    if (!s_running.load()) return;
    s_recover.disarm();
    s_running = false;
    s_renderThreadRunning = false;
    if (s_stream) AAudioStream_requestStop(s_stream);
    if (s_renderThread.joinable()) s_renderThread.join();
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeTerm(JNIEnv*, jobject)
{
    if (!s_initialized) return;
    s_recover.disarm();
    if (s_running.load()) {
        s_running = false;
        s_renderThreadRunning = false;
        if (s_stream) AAudioStream_requestStop(s_stream);
        if (s_renderThread.joinable()) s_renderThread.join();
    }
    if (s_stream) { AAudioStream_close(s_stream); s_stream = nullptr; }
    if (s_engine) s_engine->shutdown();
    s_engine.reset();
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
    ring_reset();
    clearLcd();
    s_initialized = false;
    s_bootDone.store(false);
    LOGI("nativeTerm");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeSendMidi(JNIEnv*, jobject, jint packed)
{
    if (!s_initialized) return;
    ++g_midiCount;
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({false, (uint32_t)packed, nullptr});
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeSendSysEx(JNIEnv* env, jobject, jbyteArray data, jint len)
{
    if (!s_initialized || len <= 0) return;
    jbyte* buf = env->GetByteArrayElements(data, nullptr);
    auto v = std::make_shared<std::vector<uint8_t>>((uint8_t*)buf, (uint8_t*)buf + len);
    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);
    ++g_sysexCount;
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({true, 0, v});
}

// 소프트 리셋 (All Sound Off류) — SC55Engine 정책과 동일하게, 자동/전환 경로에서는
// 이것만 쓰고 GS/MT 전체 리셋은 사용자가 명시적으로 누르는 버튼에서만 호출한다.
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeResetSynth(JNIEnv*, jobject)
{
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
    if (!s_initialized || !s_bootDone.load() || !s_engine) return;
    s_engine->reset();
}

// 하드 리셋 (디바이스 고유 GS/MT 전체 리셋) — 수동 리셋 버튼 전용.
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeHardReset(JNIEnv*, jobject)
{
    if (!s_initialized || !s_bootDone.load() || !s_engine) return;
    s_engine->hardReset();
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeIsBootDone(JNIEnv*, jobject)
{
    return s_bootDone.load() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetSampleRate(JNIEnv*, jobject)
{
    return (jint)s_actualRate;
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetVersion(JNIEnv* env, jobject)
{
    return env->NewStringUTF(emu88_get_library_version_string());
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetStats(JNIEnv* env, jobject)
{
    size_t qsz; { std::lock_guard<std::mutex> lk(g_evMtx); qsz = g_evQ.size(); }
    char buf[128];
    snprintf(buf, sizeof(buf), "midi=%llu sx=%llu q=%zu ring=%d boot=%d",
             (unsigned long long)g_midiCount.load(),
             (unsigned long long)g_sysexCount.load(),
             qsz, ring_size(), (int)s_bootDone.load());
    return env->NewStringUTF(buf);
}

// ---------------------------------------------------------------------------
// ROM 유틸 — 프로세스 전역(88lib 문서 "process-wide" 참고), 컨텍스트/엔진
// 인스턴스와 무관하게 아무 때나 호출 가능하다. SC55Engine처럼 "정확한 파일명
// 목록"을 하드코딩하지 않고, 88lib가 내용 기반으로 식별한 결과를 그대로 쓴다.
// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeAddRomPath(JNIEnv* env, jobject, jstring jPath)
{
    const char* cPath = env->GetStringUTFChars(jPath, nullptr);
    emu88_add_rom_path(cPath);
    env->ReleaseStringUTFChars(jPath, cPath);
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeIsDeviceAvailable(JNIEnv* env, jobject, jstring jDeviceCliId)
{
    const char* cDevId = env->GetStringUTFChars(jDeviceCliId, nullptr);
    emu88_device_id deviceId;
    bool resolved = erayMidi::resolveDeviceId(cDevId, &deviceId);
    env->ReleaseStringUTFChars(jDeviceCliId, cDevId);
    if (!resolved) return JNI_FALSE;
    return emu88_is_device_available(deviceId) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeDescribeRoms(JNIEnv* env, jobject, jstring jDeviceCliId)
{
    const char* cDevId = env->GetStringUTFChars(jDeviceCliId, nullptr);
    emu88_device_id deviceId;
    bool resolved = erayMidi::resolveDeviceId(cDevId, &deviceId);
    env->ReleaseStringUTFChars(jDeviceCliId, cDevId);
    if (!resolved) return env->NewStringUTF("(알 수 없는 device id)");

    // 필요한 실제 길이를 먼저 물어본 뒤(snprintf 규약과 동일) 정확한 크기로 다시 채운다.
    size_t needed = emu88_describe_device_roms(deviceId, nullptr, 0);
    std::vector<char> buf(needed + 1, 0);
    emu88_describe_device_roms(deviceId, buf.data(), buf.size());
    return env->NewStringUTF(buf.data());
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeRescanRoms(JNIEnv*, jobject)
{
    emu88_rescan_roms();
}

// ---------------------------------------------------------------------------
// 실제 기기 디스플레이/패널 상태 — MuntEngine.getLcdText()와 동일한 목적.
// emu88_get_display_text()는 문자 LCD(MT-32/CM-32L류)에서만 텍스트를 주고,
// SC-55/SC-8850처럼 그래픽 LCD인 기종은 "0 while ... on a graphic display"
// (c_interface.h)라서 빈 문자열이 돌아온다 — 그 경우 Kotlin 쪽에서 LED
// 패널로 자동 대체한다.
// ---------------------------------------------------------------------------
JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetDisplayText(JNIEnv* env, jobject, jint screen)
{
    if (!s_initialized || !s_engine || !s_bootDone.load()) return env->NewStringUTF("");
    std::string text = s_engine->getDisplayText((unsigned)screen);
    return env->NewStringUTF(text.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeIsDisplayOn(JNIEnv*, jobject, jint screen)
{
    if (!s_initialized || !s_engine || !s_bootDone.load()) return JNI_FALSE;
    return s_engine->isDisplayOn((unsigned)screen) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetPanelLeds(JNIEnv*, jobject)
{
    if (!s_initialized || !s_engine || !s_bootDone.load()) return 0;
    return (jint)s_engine->getPanelLeds();
}

// ---------------------------------------------------------------------------
// 실제 기기 LCD 프레임 (SC-55/SC-88: 741x268 주황 유리 / SC-8850: 640x256 / MT-32·CM: 연두색 도트).
//   nativeGetLcdSize : (width << 16) | height, 이 기종에 LCD가 없으면 0
//   nativeGetLcdSeq  : 화면 내용이 바뀔 때마다 증가 — 같으면 그리지 않아도 된다
//   nativeGetLcdFrame: RGBA_8888 비트맵(크기가 nativeGetLcdSize와 정확히 같아야 함)에 합성
// 모두 s_lcdMtx로 보호된 복사본만 읽으므로 어느 스레드에서 불러도 안전하다.
// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetLcdSize(JNIEnv*, jobject)
{
    int w = 0, h = 0;
    std::lock_guard<std::mutex> lk(s_lcdMtx);
    if (!erayMidi::lcd88FrameSize(s_lcdRaw, &w, &h)) return 0;
    return (jint)((w << 16) | h);
}

JNIEXPORT jlong JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetLcdSeq(JNIEnv*, jobject)
{
    return (jlong)s_lcdSeq.load(std::memory_order_acquire);
}

static std::mutex            s_frameMtx;        // nativeGetLcdFrame 호출 직렬화(작업 버퍼 공유)
static Lcd88Raw              s_frameCopy;
static std::vector<uint32_t> s_frameScratch;

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_GearmulatorEngine_nativeGetLcdFrame(JNIEnv* env, jobject, jobject bitmap)
{
    std::lock_guard<std::mutex> fl(s_frameMtx);
    {
        std::lock_guard<std::mutex> lk(s_lcdMtx);
        s_frameCopy = s_lcdRaw;
    }
    int w = 0, h = 0;
    if (!erayMidi::lcd88FrameSize(s_frameCopy, &w, &h)) return JNI_FALSE;

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) return JNI_FALSE;
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return JNI_FALSE;
    if ((int)info.width != w || (int)info.height != h) return JNI_FALSE;

    s_frameScratch.resize((size_t)w * (size_t)h);
    erayMidi::lcd88Render(s_frameCopy, s_frameScratch.data());

    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) return JNI_FALSE;
    auto* dst = static_cast<uint8_t*>(pixels);
    for (int y = 0; y < h; ++y)
        memcpy(dst + (size_t)y * info.stride, &s_frameScratch[(size_t)y * w], (size_t)w * sizeof(uint32_t));
    AndroidBitmap_unlockPixels(env, bitmap);
    return JNI_TRUE;
}

} // extern "C"
