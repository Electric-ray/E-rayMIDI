// MU2000Bridge.cpp
// JNI bridge: S-MU2000 (tarboh/S-MU2000, SH7042+SWP30x2 LLE) + AAudio
//
// 다른 엔진들과 동일한 구조: 전용 렌더 스레드가 계속 run_sample()을 호출해
// 락프리 링버퍼를 채우고, AAudio 콜백은 pop만 한다.
//
// S-MU2000 고유의 차이점 (GearmulatorBridge.cpp와 비교):
//   - MIDI가 3바이트 패킹이 아니라 "1바이트씩"이다. 스트림 파싱(SysEx 경계
//     포함)은 SH7042 펌웨어가 알아서 하므로, 브리지는 그냥 바이트를 순서대로
//     큐에 쌓았다가 렌더 스레드에서 midi_in()에 하나씩 흘려보내면 된다.
//   - 블로킹 "부팅 함수"가 없다. reset() 직후 바로 run_sample() 루프를
//     시작하면 그 자체가 부팅 과정이다(=별도 boot 단계 불필요, 렌더 스레드가
//     시작하자마자 링버퍼를 채우기 시작한다). midi_ready()가 true가 되기
//     전까지 흘려보낸 MIDI는 펌웨어가 무시하므로, Kotlin 쪽에서 이 상태를
//     보고 "부팅 중" 안내만 해주면 된다.
//   - 출력이 44100Hz 고정이다(88lib처럼 내부 리샘플러가 없음). AAudio가
//     44100Hz를 못 받아주는 극히 드문 기기에서는 리샘플링 없이 그대로
//     재생되어 피치가 살짝 달라질 수 있다 — 초기 이식에서는 감수한다.

#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include <cstring>
#include <cstdint>

#include "smu2000/MU2000Engine.h"

#define TAG "MU2000Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using erayMidi::MU2000Engine;

// ---------------------------------------------------------------------------
// 오디오 링버퍼 (다른 엔진들과 동일한 락프리 구조)
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
static void ring_push(int16_t l, int16_t r) {
    int h = s_ringHead.load(std::memory_order_relaxed);
    int nh = (h + 1) % RING_FRAMES;
    if (nh == s_ringTail.load(std::memory_order_acquire)) return; // full, drop
    s_ring[h] = {l, r};
    s_ringHead.store(nh, std::memory_order_release);
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
// MIDI 바이트 큐 (SysEx 포함 전부 낱개 바이트로 — mu2000의 스트림 파서와 그대로 맞음)
// ---------------------------------------------------------------------------
static std::deque<uint8_t> g_midiQ;
static std::mutex          g_midiMtx;

static void drainMidiQueue(MU2000Engine& engine) {
    std::deque<uint8_t> local;
    {
        std::lock_guard<std::mutex> lk(g_midiMtx);
        local.swap(g_midiQ); // 락은 이 swap 동안만
    }
    for (uint8_t b : local) {
        engine.midiInByte(b);
    }
}

// ---------------------------------------------------------------------------
// 전역 상태
// ---------------------------------------------------------------------------
static std::unique_ptr<MU2000Engine> s_engine;
static AAudioStream*     s_stream = nullptr;
static std::thread       s_renderThread;
static std::atomic<bool> s_renderThreadRunning{false};
static std::atomic<bool> s_bootDone{false}; // midi_ready(0)
static std::atomic<bool> s_running{false};
static bool              s_initialized = false;
static uint32_t          s_actualRate = 44100;

static aaudio_data_callback_result_t audioCallback(
        AAudioStream*, void*, void* audioData, int32_t numFrames)
{
    auto* out = static_cast<int16_t*>(audioData);
    for (int i = 0; i < numFrames; ++i) {
        StereoS16 f{};
        if (!ring_pop(f)) f = {0, 0};
        out[i * 2 + 0] = f.l;
        out[i * 2 + 1] = f.r;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static aaudio_result_t openStream(AAudioStream** stream, int32_t rate) {
    AAudioStreamBuilder* builder = nullptr;
    AAudio_createStreamBuilder(&builder);
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, rate);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(builder, audioCallback, nullptr);
    aaudio_result_t res = AAudioStreamBuilder_openStream(builder, stream);
    AAudioStreamBuilder_delete(builder);
    return res;
}

// ---------------------------------------------------------------------------
// nativeInit()에서 저장해 두는 ROM 경로들 (렌더 스레드 시작 시점에 실제 로딩)
// ---------------------------------------------------------------------------
static std::string s_programPath;
static std::string s_waveDir;
static std::string s_sintabPath;
static std::string s_lcdFontPath;

// ---------------------------------------------------------------------------
// 렌더 스레드: ROM 로딩 + reset() 자체가 "부팅 준비"이고, 그 뒤 run_sample()을
// 계속 호출하는 것 자체가 부팅 과정이다(S-MU2000은 블로킹 boot 함수가 없음).
// midi_ready()가 true가 되기 전에 들어온 MIDI는 펌웨어가 그냥 무시하므로,
// 드레인 자체는 부팅 상태와 무관하게 항상 수행한다.
// ---------------------------------------------------------------------------
static void renderLoop(int32_t /*desiredRate*/) {
    if (!s_engine->loadProgram(s_programPath)) {
        LOGE("프로그램 ROM 로딩 실패: %s (%s)", s_programPath.c_str(), s_engine->lastError());
        return;
    }
    if (!s_engine->loadWave(s_waveDir)) {
        LOGE("웨이브 ROM 로딩 실패: %s (%s)", s_waveDir.c_str(), s_engine->lastError());
        return;
    }
    if (!s_sintabPath.empty() && !s_engine->loadSintab(s_sintabPath)) {
        LOGE("sintab 로딩 실패(치명적이지 않음, 계속 진행): %s", s_engine->lastError());
    }
    if (!s_lcdFontPath.empty() && !s_engine->loadLcdFont(s_lcdFontPath)) {
        LOGE("LCD 폰트 로딩 실패(치명적이지 않음, 계속 진행): %s", s_engine->lastError());
    }

    // 초기 이식: 안전하게 단일 스레드로 시작 (SWP30 두 개를 별도 스레드로
    // 돌리는 옵션은 big.LITTLE 코어 배치에서 스핀웨이트 지연이 커질 위험이
    // 있어 보류 — 실기 CPU 여유를 보고 나중에 켤지 결정).
    s_engine->setThreaded(false);
    s_engine->reset();
    LOGI("render thread: reset 완료, 부팅+렌더 루프 시작");

    while (s_renderThreadRunning.load(std::memory_order_relaxed)) {
        drainMidiQueue(*s_engine); // 부팅 전엔 펌웨어가 그냥 무시함

        int16_t l, r;
        s_engine->renderSample(l, r);
        ring_push(l, r);

        if (!s_bootDone.load(std::memory_order_relaxed) && s_engine->isBootReady()) {
            s_bootDone.store(true, std::memory_order_release);
            LOGI("render thread: 부팅 완료 (midi_ready)");
        }

        // 링버퍼가 이미 충분히 차 있으면 잠깐 쉰다 (다른 엔진들과 동일한
        // 생산자/소비자 속도 조절 — run_sample은 시간 개념이 없는 pull
        // 방식이라, 여기서 안 쉬면 CPU를 낭비하며 버퍼만 계속 채운다).
        if (ring_size() >= (RING_FRAMES * 3) / 4) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeInit(
        JNIEnv* env, jobject, jstring jProgramPath, jstring jWaveDir,
        jstring jSintabPath, jstring jLcdFontPath)
{
    auto toStd = [&](jstring js) -> std::string {
        if (!js) return "";
        const char* c = env->GetStringUTFChars(js, nullptr);
        std::string s(c);
        env->ReleaseStringUTFChars(js, c);
        return s;
    };
    s_programPath = toStd(jProgramPath);
    s_waveDir     = toStd(jWaveDir);
    s_sintabPath  = toStd(jSintabPath);
    s_lcdFontPath = toStd(jLcdFontPath);

    s_engine = std::make_unique<MU2000Engine>();
    s_initialized = true;
    s_bootDone.store(false);
    LOGI("nativeInit: program=%s wave=%s", s_programPath.c_str(), s_waveDir.c_str());
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeStart(JNIEnv*, jobject)
{
    if (!s_initialized || !s_engine || s_running.load()) return;

    // S-MU2000은 44100Hz 고정 출력(내부 리샘플러 없음) — 우선 그대로 연다.
    static const int32_t kTryRates[] = {44100, 48000};
    aaudio_result_t res = AAUDIO_ERROR_INVALID_STATE;
    for (int32_t rate : kTryRates) {
        res = openStream(&s_stream, rate);
        if (res == AAUDIO_OK) break;
    }
    if (res != AAUDIO_OK) {
        LOGE("AAudio open 실패: %s", AAudio_convertResultToText(res));
        return;
    }
    s_actualRate = AAudioStream_getSampleRate(s_stream);
    if (s_actualRate != 44100) {
        LOGE("경고: AAudio가 44100Hz를 못 받고 %u Hz로 열림 — 리샘플러가 없어 "
             "피치가 살짝 달라질 수 있음", s_actualRate);
    }

    ring_reset();
    s_running = true;
    s_bootDone.store(false);
    s_renderThreadRunning = true;
    s_renderThread = std::thread(renderLoop, s_actualRate);
    AAudioStream_requestStart(s_stream);
    LOGI("nativeStart: AAudio @ %u Hz, 렌더 스레드 시작", s_actualRate);
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeStop(JNIEnv*, jobject)
{
    if (!s_running.load()) return;
    s_running = false;
    s_renderThreadRunning = false;
    if (s_stream) AAudioStream_requestStop(s_stream);
    if (s_renderThread.joinable()) s_renderThread.join();
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeTerm(JNIEnv*, jobject)
{
    if (!s_initialized) return;
    if (s_running.load()) {
        s_running = false;
        s_renderThreadRunning = false;
        if (s_stream) AAudioStream_requestStop(s_stream);
        if (s_renderThread.joinable()) s_renderThread.join();
    }
    if (s_stream) { AAudioStream_close(s_stream); s_stream = nullptr; }
    s_engine.reset();
    { std::lock_guard<std::mutex> lk(g_midiMtx); g_midiQ.clear(); }
    ring_reset();
    s_initialized = false;
    s_bootDone.store(false);
    LOGI("nativeTerm");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeSendMidi(JNIEnv* env, jobject, jbyteArray data, jint len)
{
    if (!s_initialized || len <= 0) return;
    jbyte* buf = env->GetByteArrayElements(data, nullptr);
    std::lock_guard<std::mutex> lk(g_midiMtx);
    for (jint i = 0; i < len; ++i) g_midiQ.push_back(static_cast<uint8_t>(buf[i]));
    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);
}

// S-MU2000에는 88lib의 emu88_play_silence 같은 "장치 고유 전체음소거" API가
// 없다 — 표준 MIDI CC(All Sound Off/All Notes Off)를 16채널에 흘려보낸다.
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeAllSoundOff(JNIEnv*, jobject)
{
    if (!s_initialized) return;
    std::lock_guard<std::mutex> lk(g_midiMtx);
    for (int ch = 0; ch < 16; ++ch) {
        g_midiQ.push_back(0xB0 | ch); g_midiQ.push_back(120); g_midiQ.push_back(0); // All Sound Off
        g_midiQ.push_back(0xB0 | ch); g_midiQ.push_back(123); g_midiQ.push_back(0); // All Notes Off
    }
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeIsBootDone(JNIEnv*, jobject)
{
    return s_bootDone.load() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeGetSampleRate(JNIEnv*, jobject)
{
    return (jint)s_actualRate;
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_MU2000Engine_nativeGetLastError(JNIEnv* env, jobject)
{
    if (!s_engine) return env->NewStringUTF("");
    return env->NewStringUTF(s_engine->lastError());
}

} // extern "C"
