// Lcd88Renderer.cpp
//
// Character 계열(SC-55 / SC-88)의 글자 배치와 크기는 88emuplayer/ui/panel.cpp
// (Nuked-SC55의 LCD_Render와 같은 좌표)를 그대로 옮긴 것이다. 차이는 두 가지:
//  - JUCE 오버레이가 아니라 불투명 RGBA 비트맵으로 직접 합성한다
//    (배경 유리/베젤은 lcd88_back.h, 켜진 도트는 검정, 꺼진 도트는 어두운 주황).
//  - 모든 픽셀의 알파를 명시적으로 0xFF로 쓴다.
#include "Lcd88Renderer.h"

#include <algorithm>
#include <cstring>

#include "lcd88_font.h"
#include "lcd88_back.h"

namespace erayMidi {
namespace {

constexpr int kCharW = 741;
constexpr int kCharH = 268;

// RGBA_8888 in memory (R,G,B,A bytes) == little-endian uint32 A<<24 | B<<16 | G<<8 | R.
constexpr uint32_t rgba(uint32_t r, uint32_t g, uint32_t b) {
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}
constexpr uint32_t kBlack   = rgba(0x00, 0x00, 0x00);
constexpr uint32_t kGlass55 = rgba(0xFF, 0x6F, 0x0F);  // lcd88_back_palette[0]
constexpr uint32_t kUnlit55 = rgba(0xC8, 0x50, 0x00);  // Nuked-SC55 lcd.color2
constexpr uint32_t kGlassCm = rgba(0x51, 0xBE, 0x03);  // CM-32P/CM-32L 백라이트
constexpr uint32_t kUnlitCm = rgba(0x00, 0xB5, 0x78);

static_assert(Lcd88Raw::kMonoMax >= 160 * 64, "mono buffer too small for the SC-8850");

struct Canvas {
    uint32_t* px;
    int w, h;
};

inline void put(const Canvas& c, int row, int col, uint32_t v) {
    if (row < 0 || row >= c.h || col < 0 || col >= c.w) return;
    c.px[static_cast<size_t>(row) * c.w + col] = v;
}

inline void fillRect(const Canvas& c, int row, int col, int rows, int cols, uint32_t v) {
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            put(c, row + i, col + j, v);
}

// 0x10 이상은 CGROM, 0x00..0x0F는 CGRAM(8글자 x 8줄)
const uint8_t* glyph(uint8_t ch, const uint8_t* cg) {
    return ch >= 16 ? lcd88_font[ch - 16] : &cg[(ch & 7) * 8];
}

// 5x7 문자 셀, 6픽셀 피치, 도트 하나는 5x5 블록 (LCD_FontRenderStandard)
void drawChar(const Canvas& c, int row, int col, uint8_t ch, const uint8_t* cg) {
    const uint8_t* f = glyph(ch, cg);
    for (int i = 0; i < 7; ++i)
        for (int j = 0; j < 5; ++j)
            fillRect(c, row + i * 6, col + j * 6, 5, 5, (f[i] & (1u << (4 - j))) ? kBlack : kUnlit55);
}

// 레벨미터 세그먼트: 8줄, width열, 도트 하나는 9x24 블록 (LCD_FontRenderLevel)
void drawLevel(const Canvas& c, int row, int col, uint8_t ch, const uint8_t* cg, int width) {
    const uint8_t* f = glyph(ch, cg);
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < width; ++j)
            fillRect(c, row + i * 11, col + j * 26, 9, 24, (f[i] & (1u << (4 - j))) ? kBlack : kUnlit55);
}

// 12x11 "L" / "R" 글리프, 위치(row, col)
const char* const kLrGlyph[2][12] = {
    {"11000000000", "11000000000", "11000000000", "11000000000",
     "11000000000", "11000000000", "11000000000", "11000000000",
     "11000000000", "11000000000", "11111111111", "11111111111"},
    {"11111111100", "11111111110", "11000000110", "11000000110",
     "11000000110", "11111111110", "11111111100", "11000001100",
     "11000000110", "11000000110", "11000000011", "11000000011"},
};
const int kLrPos[2][2] = {{70, 264}, {232, 264}};

void drawLr(const Canvas& c, uint8_t ch, const uint8_t* cg) {
    // DDRAM 58번 칸의 bit0이 L/R 램프
    const uint32_t col = (glyph(ch, cg)[0] & 1) ? kBlack : kUnlit55;
    for (int g = 0; g < 2; ++g)
        for (int i = 0; i < 12; ++i)
            for (int j = 0; j < 11; ++j)
                if (kLrGlyph[g][i][j] == '1')
                    put(c, i + kLrPos[g][0], j + kLrPos[g][1], col);
}

struct TextField { int ddStart, count, row, col; };
constexpr TextField kFields[] = {
    // 위쪽 줄: 3자(맵/변주) + 16자(악기명)
    {0, 3, 11, 34}, {3, 16, 11, 153},
    // 3자리 숫자 6개, 한 줄에 2개
    {40, 3, 75, 34}, {43, 3, 75, 153},
    {49, 3, 139, 34}, {46, 3, 139, 153},
    {52, 3, 203, 34}, {55, 3, 203, 153},
};

void renderCharacter(const Lcd88Raw& r, uint32_t* dst) {
    const Canvas c{dst, kCharW, kCharH};
    const size_t count = static_cast<size_t>(kCharW) * kCharH;

    // 전원이 끊긴 유리는 어둡다 (대기 모드 기종). 켜져 있지만 표시만 꺼진 경우는 백라이트만 보인다.
    if (!r.powered) {
        std::fill(dst, dst + count, kBlack);
        return;
    }
    for (size_t i = 0; i < count; ++i)
        dst[i] = lcd88_back_palette[lcd88_back_data[i]];
    if (!r.displayOn) return;

    for (const auto& tf : kFields)
        for (int i = 0; i < tf.count; ++i)
            drawChar(c, tf.row, tf.col + i * 35, r.dd[tf.ddStart + i], r.cg);

    drawLr(c, r.dd[58], r.cg);

    // 레벨미터 2채널 x 4세그먼트(마지막 세그먼트는 1열 폭의 피크 마커)
    const int levelDd[2] = {20, 60};
    for (int ch = 0; ch < 2; ++ch)
        for (int seg = 0; seg < 4; ++seg)
            drawLevel(c, 71 + ch * 88, 293 + seg * 130, r.dd[levelDd[ch] + seg], r.cg, seg == 3 ? 1 : 5);
}

// ---- Graphic ----------------------------------------------------------------

struct GraphicStyle {
    uint32_t glass, off, on;
    int pitch, dot;
    int cellW, cellH;      // 0이면 모든 픽셀이 도트. 아니면 셀의 마지막 열/행은 간격
    int texW, texH;        // 0이면 width*pitch x height*pitch
    int originX, originY;
};

GraphicStyle styleFor(const Lcd88Raw& r) {
    if (r.look == Lcd88Look::CmGreen) {
        // 8px 피치에 6px 도트. CM-32P(96x18 도트)와 CM-32L/MT-32(120x9)의 창 크기가 다르다.
        if (r.width == 96) return {kGlassCm, kUnlitCm, kBlack, 8, 6, 6, 9, 800, 150, 25, 11};
        if (r.width == 120) return {kGlassCm, kUnlitCm, kBlack, 8, 6, 6, 9, 1000, 92, 20, 10};
        return {kGlassCm, kUnlitCm, kBlack, 8, 6, 6, 9, 0, 0, 0, 0};
    }
    // SC-8850: 주황 유리 위에 켜진 도트만, 가장자리까지 꽉 채움
    return {kGlass55, kGlass55, kBlack, 4, 4, 0, 0, 0, 0, 0, 0};
}

void renderGraphic(const Lcd88Raw& r, uint32_t* dst, int texW, int texH) {
    const GraphicStyle s = styleFor(r);
    const Canvas c{dst, texW, texH};
    const size_t count = static_cast<size_t>(texW) * texH;

    if (!r.powered) {
        std::fill(dst, dst + count, kBlack);
        return;
    }
    std::fill(dst, dst + count, s.glass);

    const auto isDot = [&](int x, int y) {
        return s.cellW == 0 || ((x % s.cellW) < s.cellW - 1 && (y % s.cellH) < s.cellH - 1);
    };
    const auto dot = [&](int x, int y, uint32_t color) {
        fillRect(c, s.originY + y * s.pitch, s.originX + x * s.pitch, s.dot, s.dot, color);
    };

    if (s.off != s.glass)
        for (int y = 0; y < r.height; ++y)
            for (int x = 0; x < r.width; ++x)
                if (isDot(x, y)) dot(x, y, s.off);

    if (!r.displayOn) return;
    for (int y = 0; y < r.height; ++y)
        for (int x = 0; x < r.width; ++x)
            if (r.mono[static_cast<size_t>(y) * r.width + x]) dot(x, y, s.on);
}

} // namespace

bool lcd88Equal(const Lcd88Raw& a, const Lcd88Raw& b) {
    if (a.type != b.type || a.width != b.width || a.height != b.height || a.displayOn != b.displayOn ||
        a.powered != b.powered || a.look != b.look || a.monoLen != b.monoLen)
        return false;
    if (a.type == 1 && (std::memcmp(a.dd, b.dd, sizeof(a.dd)) != 0 || std::memcmp(a.cg, b.cg, sizeof(a.cg)) != 0))
        return false;
    if (a.monoLen > 0 && std::memcmp(a.mono, b.mono, static_cast<size_t>(a.monoLen)) != 0)
        return false;
    return true;
}

bool lcd88FrameSize(const Lcd88Raw& r, int* width, int* height) {
    if (r.type == 1) {
        *width = kCharW;
        *height = kCharH;
        return true;
    }
    if (r.type == 2 && r.width > 0 && r.height > 0 && r.monoLen == r.width * r.height &&
        r.monoLen <= Lcd88Raw::kMonoMax) {
        const GraphicStyle s = styleFor(r);
        *width = s.texW ? s.texW : r.width * s.pitch;
        *height = s.texH ? s.texH : r.height * s.pitch;
        return true;
    }
    return false;
}

void lcd88Render(const Lcd88Raw& r, uint32_t* dst) {
    int w = 0, h = 0;
    if (!dst || !lcd88FrameSize(r, &w, &h)) return;
    if (r.type == 1) renderCharacter(r, dst);
    else renderGraphic(r, dst, w, h);
}

} // namespace erayMidi
