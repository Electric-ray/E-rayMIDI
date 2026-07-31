/**
 * MuntBridge.cpp — JNI: mt32emu(MT-32/CM-32L) + AAudio
 *
 * Ported verbatim from munt-android (MuntBridge.cpp v3) into the
 * nuked-sc55-android integrated project. Internal logic (MIDI event queue,
 * AAudio callback, slice-based draining) is UNCHANGED from the original —
 * only the JNI package qualifier was updated (com.example.muntforandroid ->
 * com.example.nukedsc55) and one native accessor (nativeGetSampleRate) was
 * added so the Kotlin-side IEngine contract can query the engine's native
 * sample rate uniformly across SC-55 / munt / SoundFont.
 *
 * [버그수정] MIDI 이벤트 큐 도입 (원본 munt-android에서 이미 반영됨):
 *   - sendMidi/sendSysEx: audio mutex 없이 g_evMtx 큐에 push (즉시 반환)
 *   - aaCallback: render 전에 큐 전체 drain → Note On+Off 타이밍 정상화
 *
 * 샘플레이트 전략 (통합작업순서.md Phase 2 항목 확정):
 *   SC-55(66207Hz)와 munt(32000Hz)는 서로 다른 네이티브 레이트를 요구하지만,
 *   두 엔진은 MainActivity에서 상호 배타적으로만 동작한다(동시에 재생되지
 *   않음 — 엔진 전환 시 이전 엔진을 완전히 stop()/destroy() 한 뒤 다음
 *   엔진을 init() 한다). 따라서 공통 리샘플링 레이어나 AAudio 스트림 공유는
 *   불필요 — 각 엔진이 자신의 AAudio 스트림을 독립적으로 열고 닫는 현재
 *   구조(SC55Bridge.cpp / TsfBridge.cpp와 동일한 패턴)를 그대로 유지한다.
 */
#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <deque>
#include <vector>
#include <memory>
#include <sched.h>

#include "munt/mt32emu/mt32emu.h"

#define TAG  "MuntBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ── 진단용 ReportHandler ─────────────────────────────────────────────────
class DiagReportHandler : public MT32Emu::ReportHandler3 {
public:
    void onNoteOnIgnored(MT32Emu::Bit32u partialsNeeded, MT32Emu::Bit32u partialsFree) override {
        __android_log_print(ANDROID_LOG_WARN, "MuntBridge",
            "⚠️ NoteOn IGNORED: 필요=%u 남은파셜=%u (32파셜 한계 초과)",
            partialsNeeded, partialsFree);
    }
    void onPlayingPolySilenced(MT32Emu::Bit32u partialsNeeded, MT32Emu::Bit32u partialsFree) override {
        __android_log_print(ANDROID_LOG_WARN, "MuntBridge",
            "⚠️ Poly SILENCED: 필요=%u 남은파셜=%u (재생 중 음 강제종료)",
            partialsNeeded, partialsFree);
    }
    void onPolyStateChanged(MT32Emu::Bit8u partNum) override {}
    void onProgramChanged(MT32Emu::Bit8u partNum, const char* groupName, const char* patchName) override {
        LOGI("ProgramChange: ch%d → %s / %s", partNum+1, groupName, patchName);
    }
    void showLCDMessage(const char* message) override {
        LOGI("MT-32 LCD: %s", message);
    }
    void printDebug(const char* fmt, va_list list) override {
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, list);
        __android_log_print(ANDROID_LOG_DEBUG, "MuntBridge", "mt32emu: %s", buf);
    }
};
static DiagReportHandler g_reportHandler;

// ── ROM / Synth ───────────────────────────────────────────────────────────
static uint8_t* g_ctrlData = nullptr; static size_t g_ctrlLen = 0;
static uint8_t* g_pcmData  = nullptr; static size_t g_pcmLen  = 0;
static const MT32Emu::ROMImage* g_ctrlImg = nullptr;
static const MT32Emu::ROMImage* g_pcmImg  = nullptr;
static MT32Emu::Synth* g_synth = nullptr;
static std::mutex       g_mtx;   // synth render 전용

// ── MIDI 이벤트 큐 ────────────────────────────────────────────────────────
struct MidiEv {
    bool isSysex;
    uint32_t msg;
    std::shared_ptr<std::vector<uint8_t>> sysex;
};
static std::deque<MidiEv> g_evQ;
static std::mutex         g_evMtx;

// ── 통계 ─────────────────────────────────────────────────────────────────
static std::atomic<uint64_t> g_midiCount{0};
static std::atomic<uint64_t> g_sysexCount{0};
static std::atomic<int>      g_logCount{0};

// ── AAudio ────────────────────────────────────────────────────────────────
#define SAMPLE_RATE 32000
static AAudioStream* g_aaStream = nullptr;

#define MIDI_SLICE_FRAMES 160

static aaudio_data_callback_result_t aaCallback(
        AAudioStream*, void*, void* audioData, int32_t numFrames) {

    // FIX (외부 리뷰로 확인된 문제): SCHED_FIFO는 SC55Bridge.cpp에서 이미 제거된 패턴이다 —
    // Android/Linux 커널이 SCHED_FIFO 스레드를 주기적으로 강제 throttle해서 오히려 주기적인
    // 오디오 끑김(stutter)을 유발한다는 것이 SC-55 쪽에서 검증된 사실이고, AAudio의
    // LOW_LATENCY 퍼포먼스 모드가 이미 적절한 스케쥴링을 요청하므로 직접 SCHED_FIFO를 건드릴
    // 필요가 없다. Munt에도 동일하게 적용.

    auto* buf = static_cast<int16_t*>(audioData);
    if (!g_synth) {
        memset(buf, 0, numFrames * 2 * sizeof(int16_t));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    // FIX (외부 리뷰로 확인된 버그, SC55Bridge.cpp와 동일한 패턴): 이전에는 g_evMtx를
    // 큐 전체를 드레인하는 동안 계속 잡고 있어서, MIDI가 한꿼번에 많이 쌓이면(한 RTP
    // 패킷에 여러 MIDI 명령이 들어있는 흔한 경우) 이 락이 오래 잡혀 있어 nativeSendMidi/
    // nativeSendSysEx를 호출하는 RTP/USB 수신 스레드가 멈춘다. 수정: 락 안에서는 큐를
    // 로컬 변수로 swap만 하고(O(1), 마이크로초 단위), 실제 playMsg/playSysex 처리는 락 없이 한다.
    std::deque<MidiEv> localQ;
    {
        std::lock_guard<std::mutex> qlk(g_evMtx);
        localQ.swap(g_evQ);
    }

    std::lock_guard<std::mutex> lk(g_mtx);

    int32_t offset = 0;
    while (offset < numFrames) {
        while (!localQ.empty()) {
            auto& ev = localQ.front();
            if (ev.isSysex && ev.sysex)
                g_synth->playSysex(
                    (const MT32Emu::Bit8u*)ev.sysex->data(),
                    (MT32Emu::Bit32u)ev.sysex->size());
            else
                g_synth->playMsg((MT32Emu::Bit32u)ev.msg);
            localQ.pop_front();
        }
        int32_t toRender = std::min(MIDI_SLICE_FRAMES, numFrames - offset);
        g_synth->render(buf + offset * 2, (MT32Emu::Bit32u)toRender);
        offset += toRender;
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static bool startAAudio() {
    // FIX (외부 리뷰 제안): 960프레임(32kHz에서 30ms)은 SC-55의 kFramesBurst=512와 비교해
    // 응답성이 느렸다 — MIDI가 들어와도 콜백이 처리될 때까지 최대 30ms 걸릴 수 있음.
    // 480(15ms)으로 줄임 — 너무 작게 줄이면 콜백 횟수가 늘어 CPU 부담이 커질 수 있어
    // 중간값으로 보수적으로 택함.
    constexpr int32_t CB_FRAMES  = 480;
    constexpr int32_t BUF_FRAMES = CB_FRAMES * 6;

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        LOGE("builder 생성 실패"); return false;
    }
    AAudioStreamBuilder_setDirection      (builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate     (builder, SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount   (builder, 2);
    AAudioStreamBuilder_setFormat         (builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode    (builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setDataCallback   (builder, aaCallback, nullptr);
    AAudioStreamBuilder_setFramesPerDataCallback(builder, CB_FRAMES);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, BUF_FRAMES);

    aaudio_result_t r = AAudioStreamBuilder_openStream(builder, &g_aaStream);
    if (r != AAUDIO_OK) {
        LOGE("EXCLUSIVE 실패(%d), SHARED 재시도", r);
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        r = AAudioStreamBuilder_openStream(builder, &g_aaStream);
    }
    AAudioStreamBuilder_delete(builder);
    if (r != AAUDIO_OK) { LOGE("스트림 열기 실패: %d", r); return false; }

    int32_t rate  = AAudioStream_getSampleRate(g_aaStream);
    int32_t burst = AAudioStream_getFramesPerBurst(g_aaStream);
    int32_t cap   = AAudioStream_getBufferCapacityInFrames(g_aaStream);
    LOGI("AAudio: %dHz cb=%dfr(%.1fms) slice=%dfr(%.1fms) cap=%dfr burst=%dfr sharing=%d",
         rate, CB_FRAMES, CB_FRAMES*1000.0/SAMPLE_RATE,
         MIDI_SLICE_FRAMES, MIDI_SLICE_FRAMES*1000.0/SAMPLE_RATE,
         cap, burst, (int)AAudioStream_getSharingMode(g_aaStream));
    if (rate != SAMPLE_RATE)
        LOGI("⚠️ %dHz→%dHz OS 리샘플 중", SAMPLE_RATE, rate);

    r = AAudioStream_requestStart(g_aaStream);
    if (r != AAUDIO_OK) { LOGE("start 실패: %d", r); return false; }
    return true;
}

static void stopAAudio() {
    if (g_aaStream) {
        AAudioStream_requestStop(g_aaStream);
        AAudioStream_close(g_aaStream);
        g_aaStream = nullptr;
    }
}

static void freeRom() {
    if (g_ctrlImg){MT32Emu::ROMImage::freeROMImage(g_ctrlImg);g_ctrlImg=nullptr;}
    if (g_pcmImg) {MT32Emu::ROMImage::freeROMImage(g_pcmImg); g_pcmImg=nullptr;}
    delete[] g_ctrlData; g_ctrlData=nullptr; g_ctrlLen=0;
    delete[] g_pcmData;  g_pcmData=nullptr;  g_pcmLen=0;
}

extern "C" {

// ── nativeInit: ROM 로드 + synth open + AAudio 시작 (원본과 동일하게 한번에) ──
JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeInit(
        JNIEnv* env, jobject, jbyteArray ctrlRom, jbyteArray pcmRom) {
    stopAAudio();
    { std::lock_guard<std::mutex> lk(g_mtx);
      if (g_synth) { g_synth->close(); delete g_synth; g_synth=nullptr; } }
    { std::lock_guard<std::mutex> qlk(g_evMtx); g_evQ.clear(); }
    freeRom();
    g_midiCount=0; g_sysexCount=0; g_logCount=0;

    jsize cLen=env->GetArrayLength(ctrlRom), pLen=env->GetArrayLength(pcmRom);
    jbyte *cTmp=env->GetByteArrayElements(ctrlRom,nullptr),
          *pTmp=env->GetByteArrayElements(pcmRom,nullptr);
    g_ctrlData=new uint8_t[(size_t)cLen]; memcpy(g_ctrlData,cTmp,(size_t)cLen); g_ctrlLen=(size_t)cLen;
    g_pcmData =new uint8_t[(size_t)pLen]; memcpy(g_pcmData, pTmp,(size_t)pLen); g_pcmLen=(size_t)pLen;
    env->ReleaseByteArrayElements(ctrlRom,cTmp,JNI_ABORT);
    env->ReleaseByteArrayElements(pcmRom, pTmp,JNI_ABORT);

    MT32Emu::ArrayFile cf((const MT32Emu::Bit8u*)g_ctrlData,g_ctrlLen);
    MT32Emu::ArrayFile pf((const MT32Emu::Bit8u*)g_pcmData, g_pcmLen);
    g_ctrlImg=MT32Emu::ROMImage::makeROMImage(&cf);
    g_pcmImg =MT32Emu::ROMImage::makeROMImage(&pf);
    if (!g_ctrlImg||!g_pcmImg) { LOGE("ROM 실패"); freeRom(); return JNI_FALSE; }

    const MT32Emu::ROMInfo* ci=g_ctrlImg->getROMInfo();
    const MT32Emu::ROMInfo* pi=g_pcmImg->getROMInfo();
    LOGI("ROM ctrl=%s pcm=%s", ci?ci->shortName:"?", pi?pi->shortName:"?");

    { std::lock_guard<std::mutex> lk(g_mtx);
      g_synth = new MT32Emu::Synth(&g_reportHandler);
      g_synth->setReportHandler3(&g_reportHandler);
      // 파셜 수 32→64: 폴리포니 오버플로우(음 누락/강제종료) 완화 (munt-android 원본 결정 유지)
      if (!g_synth->open(*g_ctrlImg, *g_pcmImg, 64)) {
          LOGE("Synth::open 실패"); delete g_synth; g_synth=nullptr; freeRom(); return JNI_FALSE; } }

    if (!startAAudio()) {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_synth->close(); delete g_synth; g_synth=nullptr; freeRom(); return JNI_FALSE; }

    LOGI("nativeInit OK — MIDI 큐 방식 활성화");
    return JNI_TRUE;
}

// nativeStart/nativeStop: nativeInit()이 이미 오디오까지 시작하므로 여기서는
// 다른 엔진(SC55Engine/SoundFontEngine)과 동일한 Kotlin측 호출 순서
// (nativeInit → nativeStart)를 맞추기 위한 얇은 no-op/제어용 래퍼.
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeStart(JNIEnv*, jobject) {
    // no-op: 오디오 스트림은 nativeInit()에서 이미 시작됨
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeStop(JNIEnv*, jobject) {
    stopAAudio();
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeTerm(JNIEnv*, jobject) {
    stopAAudio();
    { std::lock_guard<std::mutex> qlk(g_evMtx); g_evQ.clear(); }
    { std::lock_guard<std::mutex> lk(g_mtx);
      if (g_synth) { g_synth->close(); delete g_synth; g_synth=nullptr; } }
    freeRom();
    LOGI("nativeTerm OK");
}

// ── nativeSendMidi: 큐에 push (non-blocking) ─────────────────────────────
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeSendMidi(JNIEnv*,jobject,jint packed) {
    uint32_t u = (uint32_t)packed;
    ++g_midiCount;
    std::lock_guard<std::mutex> qlk(g_evMtx);
    g_evQ.push_back({false, u, nullptr});
}

// ── nativeSendSysEx: 큐에 push (non-blocking) ────────────────────────────
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeSendSysEx(JNIEnv* env,jobject,jbyteArray data,jint len) {
    ++g_sysexCount;
    jbyte* buf=env->GetByteArrayElements(data,nullptr);
    auto sysexVec = std::make_shared<std::vector<uint8_t>>(
        (uint8_t*)buf, (uint8_t*)buf + len);
    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);

    std::lock_guard<std::mutex> qlk(g_evMtx);
    g_evQ.push_back({true, 0, sysexVec});
}

// ── nativeResetSynth ──────────────────────────────────────────────────────
// GS Reset 정책 분리 (통합작업순서.md Phase 2): SC-55mk2 경로에서는 절대
// PostSystemReset(GS_RESET)을 호출하지 않기로 확정되어 있으나, 이는 SC-55
// 전용 결정이며 여기 MT-32 Master Reset SysEx에는 적용되지 않는다 —
// mt32emu 코어가 자체적으로 필요로 하는 리셋 방식이고 SC55Bridge.cpp와는
// 완전히 독립된 코드 경로이므로 공용 로직에 절대 섞지 않는다.
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeResetSynth(JNIEnv*,jobject) {
    std::lock_guard<std::mutex> qlk(g_evMtx);
    g_evQ.clear();
    if (!g_synth) return;

    // FIX (외부 리뷰로 확인된 버그): MT-32 표준 관습상 Part1-8은 MIDI 채널 2-9,
    // Rhythm은 채널 10이고 채널 1은 미사용이다. 이전 코드는 "ch==8"(=채널 9, 실제로 Part8이
    // 있는 유효한 채널)를 스킵하고 있어서 Part8이 All Notes Off를 안 받았다 —
    // 스킵해야 할 건 미사용 채널인 "ch==0"(=채널 1)이다.
    for (int ch = 0; ch < 10; ch++) {
        if (ch == 0) continue;
        g_evQ.push_back({false, (uint32_t)(0xB0|ch)|(123u<<8)|(0u<<16), nullptr});
        g_evQ.push_back({false, (uint32_t)(0xB0|ch)|(120u<<8)|(0u<<16), nullptr});
    }

    static const uint8_t kMT32Reset[] = {
        0xF0, 0x41, 0x10, 0x16, 0x12,
        0x7F, 0x00, 0x00, 0x01, 0x00,
        0xF7
    };
    auto sysex = std::make_shared<std::vector<uint8_t>>(
        kMT32Reset, kMT32Reset + sizeof(kMT32Reset));
    g_evQ.push_back({true, 0, sysex});

    LOGI("nativeResetSynth: All Notes Off + MT-32 Master Reset");
}

// ── nativeGetSampleRate: IEngine 공통 계약용 신규 accessor ───────────────
JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeGetSampleRate(JNIEnv*, jobject) {
    return SAMPLE_RATE;
}

// ── nativeGetStats ────────────────────────────────────────────────────────
JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_MuntEngine_nativeGetStats(JNIEnv* env,jobject) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (!g_synth) return env->NewStringUTF("partStates:0\nnames:\nmidi:0\nsysex:0\nactive:0\n");
    char buf[1024];
    MT32Emu::Bit32u states=g_synth->getPartStates();
    char names[512]={0};
    for(int i=0;i<9;i++){
        const char* n=g_synth->getPatchName((MT32Emu::Bit8u)i);
        if(i>0) strcat(names,",");
        strncat(names,n?n:"---",20);
    }
    size_t qsz;
    { std::lock_guard<std::mutex> qlk(g_evMtx); qsz=g_evQ.size(); }
    snprintf(buf,sizeof(buf),
        "partStates:%u\nnames:%s\nmidi:%llu\nsysex:%llu\nactive:%d\nqsz:%zu\n",
        (unsigned)states, names,
        (unsigned long long)g_midiCount.load(),
        (unsigned long long)g_sysexCount.load(),
        g_synth->isActive()?1:0, qsz);
    return env->NewStringUTF(buf);
}

} // extern "C"
