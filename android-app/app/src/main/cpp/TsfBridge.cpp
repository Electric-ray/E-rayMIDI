// TsfBridge.cpp  v1.1
// JNI bridge: TinySoundFont (schellingb/TinySoundFont) + AAudio
//
// v1.1: MIDI 이벤트 드레인과 오디오 렌더링을 분리된 스레드로 나눔.
// v1.0에서는 둘 다 AAudio 콜백(실시간 스레드)에서 처리했는데, GS 초기화
// 구간처럼 짧은 시간에 SysEx/CC가 몰리면 그 순간 드레인 처리가 오래 걸려
// 실시간 우선순위 스레드가 CPU를 오래 붙들고, 마찬가지로 실시간 우선순위인
// RTP 수신 스레드가 밀리는 문제가 있었다. 그 결과 실제 패킷 유실이 늘고,
// Note Off 유실 → 노트가 안 꺼짐 → 동시 발음 수 증가 → 렌더링 부하 증가 →
// 더 밀림 → BY 발동, 이라는 악순환이 생겼다(실사용 테스트로 확인됨).
// 수정: 렌더링은 실시간 콜백에 남기고, MIDI 이벤트 처리는 일반 우선순위
// 백그라운드 스레드로 옮긴다. 둘 다 짧은 뮤텍스로만 동기화(tsf.h가 문서로
// 명시한 스레드 안전 패턴)해서, 이벤트 버스트가 있어도 임계구역 자체는
// 마이크로초 단위라 렌더 스레드를 실질적으로 막지 않는다.

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

#define TSF_IMPLEMENTATION
#include "tsf.h"

#define TAG "TsfBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// MIDI 이벤트 큐 (Kotlin 스레드 -> MIDI 처리 스레드)
// ---------------------------------------------------------------------------
struct MidiEv {
    bool isSysex;
    uint32_t packed;          // status | (d1<<8) | (d2<<16)
    std::shared_ptr<std::vector<uint8_t>> sysex;
};
static std::deque<MidiEv> g_evQ;
static std::mutex         g_evMtx;

// ---------------------------------------------------------------------------
// TSF 인스턴스 상태 보호용 뮤텍스: MIDI 처리 스레드(tsf_channel_* 호출)와
// 오디오 콜백 스레드(tsf_render_short)가 동시에 tsf 내부 상태를 건드리지
// 않도록 짧게만 잠근다. 각 임계구역은 단일 채널 이벤트 처리/한 버퍼 렌더링
// 정도라 보유 시간이 짧다.
// ---------------------------------------------------------------------------
static std::mutex         g_tsfMtx;

// ---------------------------------------------------------------------------
// TSF 인스턴스 + AAudio 스트림
// ---------------------------------------------------------------------------
static tsf*               s_tsf = nullptr;
static AAudioStream*      s_stream = nullptr;
static std::atomic<bool>  s_initialized{false};
static std::atomic<bool>  s_running{false};
static std::thread        s_midiThread;
static std::atomic<bool>  s_midiThreadRunning{false};
static constexpr int      kChannels    = 2;
static constexpr int      kFramesBurst = 512;
static constexpr int      kMaxVoices   = 256;
// 많은 무료 GM 사운드폰트가 SC-55 에뮬레이션 출력보다 크게 마스터링되어
// 있어서, 동일 볼륨에서 체감 음량이 커지는 경우가 많다. 기본으로 살짝
// 낮춰서 청감상 비슷한 수준으로 맞춘다 (필요시 추후 UI 슬라이더로 노출 가능).
static constexpr float    kDefaultGainDb = -6.0f;

// 채널별 현재 프로그램(악기) 번호 캐시 — 드럼채널(9) 여부 판단 및 상태 조회용
static int s_channelProgram[16] = {0};
static int s_channelBank[16]    = {0};
// IEngine 공통 계약용: nativeInit에서 AAudio가 실제로 열어준 레이트를 기억해둘 (TSF는 고정 네이티브 레이트가 없으므로 디바이스가 부여한 값을 그대로 보고한다).
static std::atomic<int> s_actualSampleRate{44100};

static void applyDefaultChannelSetup() {
    // GM 기본값: 채널 10(0-idx 9)은 드럼킷(뱅크128), 나머지는 뱅크0/프리셋0.
    // 사운드폰트에 뱅크128이 없는 경우 tsf_channel_set_bank_preset이 실패하므로
    // flag_mididrums 방식(tsf_channel_set_presetnumber)으로 한 번 더 시도한다.
    // 이 함수는 항상 g_tsfMtx를 잡은 상태에서 호출되어야 한다.
    for (int ch = 0; ch < 16; ++ch) {
        bool isDrum = (ch == 9);
        if (isDrum) {
            if (!tsf_channel_set_bank_preset(s_tsf, ch, 128, 0)) {
                tsf_channel_set_presetnumber(s_tsf, ch, 0, 1);
            }
        } else {
            tsf_channel_set_bank_preset(s_tsf, ch, 0, 0);
        }
        s_channelProgram[ch] = 0;
        s_channelBank[ch] = isDrum ? 128 : 0;
    }
}

// ---------------------------------------------------------------------------
// MIDI 이벤트 1개 처리 (MIDI 처리 스레드에서만 호출, g_tsfMtx 보유 중)
// ---------------------------------------------------------------------------
static void processOneEvent(const MidiEv& ev) {
    if (ev.isSysex && ev.sysex) {
        auto& v = *ev.sysex;
        if (v.size() >= 5 && v[0] == 0xF0 && v[1] == 0x7E && v[3] == 0x09) {
            // Universal Non-realtime SysEx: GM System On(01)/Off(02)
            // -> 채널 기본 상태로 재초기화 (안전한 리셋 동작)
            LOGI("GM SysEx 수신 (cmd=%02X) -> 채널 기본값 재설정", v[4]);
            tsf_reset(s_tsf);
            applyDefaultChannelSetup();
        }
        // GS/XG 전용 SysEx(리듬 파트 재배정, 리버브/코러스 레벨 등)는 TSF가
        // 표현할 수 있는 개념이 아니므로 의도적으로 무시한다. 이 때문에 GS
        // 전용 편성(표준 채널10 이외의 리듬 채널 등)을 쓰는 곡은 일부 악기가
        // 다르게 들릴 수 있는데, 이는 SoundFont(GM) 모드의 근본적 한계다.
        return;
    }

    uint32_t p  = ev.packed;
    uint8_t  st = p & 0xFF;
    uint8_t  d1 = (p >> 8)  & 0xFF;
    uint8_t  d2 = (p >> 16) & 0xFF;
    uint8_t  ch = st & 0x0F;
    uint8_t  type = st & 0xF0;

    switch (type) {
        case 0x80: // Note Off
            tsf_channel_note_off(s_tsf, ch, d1);
            break;
        case 0x90: // Note On (velocity 0 = Note Off, 흔한 관례)
            if (d2 == 0) tsf_channel_note_off(s_tsf, ch, d1);
            else         tsf_channel_note_on(s_tsf, ch, d1, d2 / 127.0f);
            break;
        case 0xB0: // Control Change — tsf가 뱅크/서스테인/볼륨/팬/RPN 대부분을 처리
            tsf_channel_midi_control(s_tsf, ch, d1, d2);
            if (d1 == 0 || d1 == 32) s_channelBank[ch] = tsf_channel_get_preset_bank(s_tsf, ch);
            break;
        case 0xC0: // Program Change
        {
            bool isDrum = (ch == 9);
            tsf_channel_set_presetnumber(s_tsf, ch, d1, isDrum ? 1 : 0);
            s_channelProgram[ch] = d1;
            break;
        }
        case 0xE0: // Pitch Bend (d1=LSB, d2=MSB)
        {
            int bend = (d1 & 0x7F) | ((d2 & 0x7F) << 7);
            tsf_channel_set_pitchwheel(s_tsf, ch, bend);
            break;
        }
        case 0xA0: // Poly Aftertouch — TSF 미지원, 무시
        case 0xD0: // Channel Aftertouch — TSF 미지원, 무시
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// MIDI 처리 스레드: 오디오 콜백과 완전히 분리된 일반 우선순위 스레드.
// 여기서 큐를 비우고 tsf_channel_* 함수들을 호출한다. 이벤트가 아무리
// 몰려도(SysEx/CC 버스트 등) 이 스레드가 오래 걸릴 뿐, 실시간 오디오
// 콜백이나 RTP 수신 스레드의 CPU 스케줄링을 방해하지 않는다.
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
            {
                std::lock_guard<std::mutex> lk(g_tsfMtx);
                if (s_tsf) processOneEvent(local.front());
            }
            local.pop_front();
        }
    }
}

// ---------------------------------------------------------------------------
// AAudio 데이터 콜백: 렌더링만 수행 (MIDI 드레인은 별도 스레드로 이동됨)
// ---------------------------------------------------------------------------
static aaudio_data_callback_result_t audioCallback(
        AAudioStream*, void*, void* audioData, int32_t numFrames)
{
    if (!s_initialized.load(std::memory_order_relaxed) || !s_tsf) {
        std::memset(audioData, 0, (size_t)numFrames * kChannels * sizeof(int16_t));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    std::lock_guard<std::mutex> lk(g_tsfMtx);
    tsf_render_short(s_tsf, static_cast<short*>(audioData), numFrames, 0);
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
    s_tsf = tsf_load_filename(sf2Path.c_str());
    if (!s_tsf) {
        LOGE("tsf_load_filename 실패: %s", sf2Path.c_str());
        return JNI_FALSE;
    }
    // 렌더링 스레드(오디오 콜백)에서 note_on 시 voice 배열이 실시간 재할당되지
    // 않도록 미리 넉넉히 확보 (tsf.h 문서가 권장하는 스레드 안전 패턴).
    tsf_set_max_voices(s_tsf, kMaxVoices);

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

    // rate=0(AAUDIO_UNSPECIFIED)으로 열어서 디바이스가 선호하는 네이티브 레이트를
    // 그대로 받는다. TSF는 SC-55와 달리 "고정 네이티브 레이트" 제약이 없는 순수
    // 신디사이저라 어떤 레이트든 tsf_set_output()에 그대로 넘기면 되고, 별도의
    // 리샘플링 계층이 필요 없다.
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
        tsf_close(s_tsf); s_tsf = nullptr;
        return JNI_FALSE;
    }

    int32_t actualRate = AAudioStream_getSampleRate(s_stream);
    s_actualSampleRate.store(actualRate);
    tsf_set_output(s_tsf, TSF_STEREO_INTERLEAVED, actualRate, kDefaultGainDb);
    applyDefaultChannelSetup();

    LOGI("AAudio opened @ %d Hz, presets=%d, gain=%.1fdB", actualRate, tsf_get_presetcount(s_tsf), kDefaultGainDb);
    s_initialized.store(true);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeStart(JNIEnv*, jobject)
{
    if (!s_initialized.load() || s_running.load()) return;
    s_running.store(true);
    s_midiThreadRunning.store(true);
    s_midiThread = std::thread(midiThreadLoop);
    AAudioStream_requestStart(s_stream);
    LOGI("nativeStart: AAudio + MIDI thread started");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeStop(JNIEnv*, jobject)
{
    if (!s_running.load()) return;
    s_running.store(false);
    if (s_stream) AAudioStream_requestStop(s_stream);
    s_midiThreadRunning.store(false);
    if (s_midiThread.joinable()) s_midiThread.join();
    LOGI("nativeStop");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeTerm(JNIEnv*, jobject)
{
    if (!s_initialized.load()) return;
    if (s_running.load()) {
        s_running.store(false);
        if (s_stream) AAudioStream_requestStop(s_stream);
        s_midiThreadRunning.store(false);
        if (s_midiThread.joinable()) s_midiThread.join();
    }
    if (s_stream) { AAudioStream_close(s_stream); s_stream = nullptr; }
    if (s_tsf) { tsf_close(s_tsf); s_tsf = nullptr; }
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
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
    return s_initialized.load() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetActiveVoices(JNIEnv*, jobject)
{
    if (!s_initialized.load() || !s_tsf) return 0;
    std::lock_guard<std::mutex> lk(g_tsfMtx);
    return (jint)tsf_active_voice_count(s_tsf);
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetPresetCount(JNIEnv*, jobject)
{
    if (!s_initialized.load() || !s_tsf) return 0;
    return (jint)tsf_get_presetcount(s_tsf);
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetChannelPresetName(JNIEnv* env, jobject, jint channel)
{
    if (!s_initialized.load() || !s_tsf || channel < 0 || channel > 15) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lk(g_tsfMtx);
    int presetIdx = tsf_channel_get_preset_index(s_tsf, channel);
    const char* name = tsf_get_presetname(s_tsf, presetIdx);
    return env->NewStringUTF(name ? name : "");
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetVersion(JNIEnv* env, jobject)
{
    return env->NewStringUTF("TinySoundFont bridge v1.1 (separate MIDI thread, -6dB default gain)");
}

// IEngine 공통 계약용 accessor (통합작업순서.md Phase 1)
JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SoundFontEngine_nativeGetSampleRate(JNIEnv*, jobject)
{
    return (jint)s_actualSampleRate.load();
}

} // extern "C"
