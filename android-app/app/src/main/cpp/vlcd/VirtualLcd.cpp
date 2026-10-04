// VirtualLcd.cpp
//
// "LCD가 없는" 엔진(SoundFont/FluidSynth, SC-8820 등)과 1줄 문자 LCD(Munt MT-32)를 위한 가상 LCD.
// 앱의 다른 LCD(88emu, S-MU2000)와 똑같은 렌더러를 재사용한다:
//
//   - 가상 패널(renderPanel): MU2000 LCD(Lcd2000Renderer)와 같은 모양. 호출자(Kotlin)가 MIDI에서 추적한
//     파라미터(파트별 레벨/볼륨/팬/리버브/코러스, 음색명 ...)를 받아 2x24칸 도트 이미지(384B)로
//     "구성"한 뒤 Lcd2000Renderer로 그린다 — 위 면의 레벨미터와 음색명, 아래 면의 파트 번호/프로그램,
//     VOL/EXP 막대, PAN 바늘, REV/CHO 부채꼴, KEY 7세그가 실기처럼 움직인다.
//   - 문자 LCD(renderText): 20문자 1줄을 120x9 도트로 만들어 88emu MT-32/CM 화면(연두색 유리)으로 그린다.
//
// 순수 계산 + JNI 비트맵 복사뿐이라 오디오 스레드와 무관하다 (Kotlin LCD 백그라운드 스레드에서 호출).
#include <jni.h>
#include <android/bitmap.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "Lcd2000Renderer.h"
#include "Lcd88Renderer.h"
#include "lcd_cgrom_fallback.h"   // kFallbackCgrom[240][10] : HD44780 5x8 문자 (코드 0x10~0xFF)

using erayMidi::Lcd2000Raw;
using erayMidi::Lcd88Raw;
using erayMidi::Lcd88Look;

namespace {

// ---- 가상 패널 파라미터 배열 (Kotlin VirtualPanelState와 같은 순서) --------------------------------
enum {
    P_LCD_ON = 0, P_CONTRAST, P_PART /*1..16*/, P_BANK, P_PROG /*0..127*/, P_VOL, P_EXP, P_PAN,
    P_REV, P_CHO, P_VAR, P_KEY /*-24..24*/, P_MODE /*0 없음,1 XG,2 GS,3 PERFORM*/, P_DRUM,
    P_METER = 16,   // 16..31 : 채널별 레벨 0..127
    P_COUNT = 32
};

constexpr int COLS = 24;

// 5x8 문자 한 칸을 (row, col)에 쓴다. 코드 1은 ▶ (MU2000 펌웨어가 CGRAM으로 쓰는 글자)
void putGlyph(Lcd2000Raw& r, int row, int col, int code) {
    static const uint8_t kArrow[8] = {0x10, 0x18, 0x1C, 0x1E, 0x1C, 0x18, 0x10, 0x00};
    uint8_t* dst = &r.dots[(row * COLS + col) * Lcd2000Raw::kCellH];
    const uint8_t* g = nullptr;
    if (code == 1) g = kArrow;
    else if (code >= 0x20 && code <= 0xFF) g = kFallbackCgrom[code - 0x10];
    for (int y = 0; y < 8; ++y) dst[y] = g ? (g[y] & 0x1F) : 0;
}

void putText(Lcd2000Raw& r, int row, int col, const char* s, int n) {
    for (int i = 0; i < n; ++i) putGlyph(r, row, col + i, static_cast<unsigned char>(s[i]));
}

// 23번째 칸의 제어 비트: 열 A-D = 비트3-0, 행 0-15 (위 칸 0-7, 아래 칸 8-15)
void ctl(Lcd2000Raw& r, int col, int row) {
    r.dots[((row / 8) * COLS + 23) * Lcd2000Raw::kCellH + (row % 8)] |= static_cast<uint8_t>(1u << (3 - col));
}
enum { CA = 0, CB = 1, CC = 2, CD = 3 };

// 한 점 켜기: 아래 면 악기 모양(20~22칸, 15x16 점)
void iconDot(Lcd2000Raw& r, int x, int y) {
    if (x < 0 || x >= 15 || y < 0 || y >= 16) return;
    const int col = 20 + x / 5, row = y / 8;
    r.dots[(row * COLS + col) * Lcd2000Raw::kCellH + (y % 8)] |= static_cast<uint8_t>(1u << (4 - x % 5));
}

void drawIcon(Lcd2000Raw& r, bool drum) {
    if (drum) {
        for (int x = 3; x <= 11; ++x) for (int y = 7; y <= 12; ++y) iconDot(r, x, y);   // 북통
        for (int x = 4; x <= 10; ++x) iconDot(r, x, 6);
        for (int i = 0; i < 4; ++i) { iconDot(r, 1 + i, 1 + i); iconDot(r, 13 - i, 1 + i); }   // 스틱
    } else {
        for (int x = 0; x <= 14; ++x) { iconDot(r, x, 2); iconDot(r, x, 13); }              // 건반 틀
        for (int y = 2; y <= 13; ++y) { iconDot(r, 0, y); iconDot(r, 14, y); }
        const int blacks[4] = {2, 5, 8, 11};
        for (int b : blacks) for (int y = 3; y <= 8; ++y) { iconDot(r, b, y); iconDot(r, b + 1, y); } // 검은 건반
        for (int y = 9; y <= 12; ++y) { iconDot(r, 4, y); iconDot(r, 7, y); iconDot(r, 10, y); }       // 흰 건반 구분선
    }
}

// 7세그 숫자(a..g = 비트0..6)
const uint8_t kSeg[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void fillPanel(const jint* p, const char* name, int nameLen, Lcd2000Raw& r) {
    std::memset(r.dots, 0, sizeof(r.dots));
    r.valid = true;
    r.lcdOn = p[P_LCD_ON] != 0;
    r.contrast = p[P_CONTRAST] > 0 ? p[P_CONTRAST] : 2;

    // 위 면 0~8칸: 레벨미터 18개 (1칸 = 2개, 점 폭 2), 16채널 + 끝의 2개는 바닥 한 점만
    for (int bar = 0; bar < 18; ++bar) {
        const int lvl = bar < 16 ? std::clamp(p[P_METER + bar], 0, 127) : 0;
        const int h = 1 + static_cast<int>(std::lround(lvl / 127.0 * 15.0));   // 1..16
        const uint8_t mask = (bar % 2 == 0) ? 0x18 : 0x03;
        const int col = bar / 2;
        for (int y = 0; y < h; ++y) {
            const int row = y < 8 ? 1 : 0, line = 7 - (y % 8);
            r.dots[(row * COLS + col) * Lcd2000Raw::kCellH + line] |= mask;
        }
    }

    // 위 면 9~16칸: 윗줄 = 음색명 8자, 아랫줄 = ▶뱅크▶프로그램
    char buf[9];
    std::memset(buf, ' ', 8);
    std::memcpy(buf, name, std::min(nameLen, 8));
    putText(r, 0, 9, buf, 8);
    char sub[9];
    std::snprintf(sub, sizeof(sub), "\x01%03d\x01%03d", std::clamp(p[P_BANK], 0, 999), std::clamp(p[P_PROG] + 1, 0, 999));
    putText(r, 1, 9, sub, 8);

    // 아래 면: 파트 번호 "01", 프로그램 "001", 악기 모양
    char part[3];
    std::snprintf(part, sizeof(part), "%02d", std::clamp(p[P_PART], 0, 99));
    putText(r, 0, 17, part, 2);
    char pg[4];
    std::snprintf(pg, sizeof(pg), "%03d", std::clamp(p[P_PROG] + 1, 0, 999));
    putText(r, 1, 17, pg, 3);
    drawIcon(r, p[P_DRUM] != 0);

    // VOL / EXP 막대 (행 0의 19번째 칸): 8단계, 아래에서부터 쌓임. 왼쪽 2점 = VOL, 오른쪽 2점 = EXP
    const int vol = std::clamp(static_cast<int>(std::lround(p[P_VOL] / 127.0 * 8.0)), 0, 8);
    const int exp = std::clamp(static_cast<int>(std::lround(p[P_EXP] / 127.0 * 8.0)), 0, 8);
    for (int y = 0; y < 8; ++y) {
        uint8_t v = 0;
        if (7 - y < vol) v |= 0x18;
        if (7 - y < exp) v |= 0x03;
        r.dots[(0 * COLS + 19) * Lcd2000Raw::kCellH + y] |= v;
    }

    // PAN 바늘: 7곳 (0 = 왼쪽 끝 ... 3 = 중앙 ... 6 = 오른쪽 끝)
    const int pan = std::clamp(static_cast<int>(std::lround(p[P_PAN] / 127.0 * 6.0)), 0, 6);
    ctl(r, CD, 15 - pan);

    // REV / CHO / VAR 센드량 부채꼴 (8단계)
    const int rev = std::clamp(static_cast<int>(std::ceil(p[P_REV] / 127.0 * 8.0)), 0, 8);
    const int cho = std::clamp(static_cast<int>(std::ceil(p[P_CHO] / 127.0 * 8.0)), 0, 8);
    const int var = std::clamp(static_cast<int>(std::ceil(p[P_VAR] / 127.0 * 8.0)), 0, 8);
    for (int k = 0; k < 8; ++k) {
        if (k < rev) ctl(r, CA, 15 - k);
        if (k < cho) ctl(r, CB, 15 - k);
        if (k < var) ctl(r, CC, 15 - k);
    }

    // KEY: 부호 + 7세그 두 자리
    const int key = std::clamp(p[P_KEY], -24, 24);
    ctl(r, CB, 0);                 // 가로 막대 (+ 도 - 도 있다)
    if (key >= 0) ctl(r, CA, 0);   // 세로 막대 = 플러스
    const int ak = std::abs(key), tens = ak / 10, ones = ak % 10;
    if (tens == 1) { ctl(r, CB, 1); ctl(r, CA, 7); }
    else if (tens == 2) { ctl(r, CA, 1); ctl(r, CB, 1); }
    const uint8_t s = kSeg[ones];
    if (s & 0x01) ctl(r, CB, 6);   // a
    if (s & 0x02) ctl(r, CA, 6);   // b
    if (s & 0x04) ctl(r, CA, 3);   // c
    if (s & 0x08) ctl(r, CA, 2);   // d
    if (s & 0x10) ctl(r, CB, 2);   // e
    if (s & 0x20) ctl(r, CB, 7);   // f
    if (s & 0x40) ctl(r, CB, 3);   // g

    // 모드 표시 ▶ (XG / GS / PERFORM)
    switch (p[P_MODE]) {
        case 1: ctl(r, CB, 5); break;
        case 2: ctl(r, CA, 4); break;
        case 3: ctl(r, CA, 5); break;
        default: break;
    }
}

bool copyToBitmap(JNIEnv* env, jobject bitmap, const uint32_t* px, int w, int h) {
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) return false;
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return false;
    if (static_cast<int>(info.width) != w || static_cast<int>(info.height) != h) return false;
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) return false;
    auto* dst = static_cast<uint8_t*>(pixels);
    for (int y = 0; y < h; ++y)
        std::memcpy(dst + static_cast<size_t>(y) * info.stride, px + static_cast<size_t>(y) * w, static_cast<size_t>(w) * 4);
    AndroidBitmap_unlockPixels(env, bitmap);
    return true;
}

std::mutex g_mtx;                 // 정적 작업 버퍼 보호 (호출자는 보통 스레드 하나)
std::vector<uint32_t> g_scratch;
Lcd2000Raw g_panel;
Lcd88Raw g_text;

} // namespace

extern "C" {

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_VirtualLcd_panelSize(JNIEnv*, jobject) {
    int w = 0, h = 0;
    erayMidi::lcd2000FrameSize(&w, &h);
    return (jint)((w << 16) | h);
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_VirtualLcd_renderPanel(JNIEnv* env, jobject, jobject bitmap, jintArray params, jbyteArray name) {
    if (!bitmap || !params || env->GetArrayLength(params) < P_COUNT) return JNI_FALSE;
    jint p[P_COUNT];
    env->GetIntArrayRegion(params, 0, P_COUNT, p);
    char nm[16] = {};
    int nameLen = 0;
    if (name) {
        nameLen = std::min<int>(env->GetArrayLength(name), 8);
        env->GetByteArrayRegion(name, 0, nameLen, reinterpret_cast<jbyte*>(nm));
    }
    std::lock_guard<std::mutex> lk(g_mtx);
    fillPanel(p, nm, nameLen, g_panel);
    int w = 0, h = 0;
    erayMidi::lcd2000FrameSize(&w, &h);
    g_scratch.resize(static_cast<size_t>(w) * h);
    erayMidi::lcd2000Render(g_panel, g_scratch.data());
    return copyToBitmap(env, bitmap, g_scratch.data(), w, h) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_VirtualLcd_textSize(JNIEnv*, jobject) {
    Lcd88Raw r;
    r.type = 2; r.width = 120; r.height = 9; r.monoLen = 120 * 9; r.look = Lcd88Look::CmGreen;
    int w = 0, h = 0;
    if (!erayMidi::lcd88FrameSize(r, &w, &h)) return 0;
    return (jint)((w << 16) | h);
}

// codes: 문자 코드(유니코드 포인트) 최대 20개. 0x20~0x7E는 5x8 폰트, 0x2588(█)은 가득 찬 칸, 그 외는 공백
JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_VirtualLcd_renderText(JNIEnv* env, jobject, jobject bitmap, jintArray codes) {
    if (!bitmap || !codes) return JNI_FALSE;
    const int n = std::min<int>(env->GetArrayLength(codes), 20);
    jint cs[20] = {};
    env->GetIntArrayRegion(codes, 0, n, cs);

    std::lock_guard<std::mutex> lk(g_mtx);
    Lcd88Raw& r = g_text;
    r.type = 2; r.width = 120; r.height = 9; r.monoLen = 120 * 9;
    r.displayOn = true; r.powered = true; r.look = Lcd88Look::CmGreen;
    std::memset(r.mono, 0, static_cast<size_t>(r.monoLen));
    for (int i = 0; i < n; ++i) {
        const int c = cs[i];
        for (int y = 0; y < 8; ++y) {
            uint8_t row = 0;
            if (c == 0x2588 || c == 1) row = 0x1F;
            else if (c >= 0x20 && c <= 0x7E) row = kFallbackCgrom[c - 0x10][y] & 0x1F;
            for (int x = 0; x < 5; ++x)
                if (row & (1u << (4 - x))) r.mono[y * 120 + i * 6 + x] = 1;
        }
    }
    int w = 0, h = 0;
    if (!erayMidi::lcd88FrameSize(r, &w, &h)) return JNI_FALSE;
    g_scratch.resize(static_cast<size_t>(w) * h);
    erayMidi::lcd88Render(r, g_scratch.data());
    return copyToBitmap(env, bitmap, g_scratch.data(), w, h) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
