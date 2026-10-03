// Lcd2000Renderer.cpp  (layout/shape logic follows tarboh/S-MU2000 src/ui/panel.cpp, BSD-3-Clause)
#include "Lcd2000Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace erayMidi {
namespace {

// ---- geometry (px) --------------------------------------------------------------------------
constexpr int    D = 8;                       // 위쪽 면의 점 간격
constexpr int    kW = 932, kH = 248;
constexpr int    kX0 = 22;                    // 2.7점 + 여백
constexpr int    kY0 = 4;
constexpr int    kSy = 166;                   // 아래 면의 윗변 = y0 + 16*D + 목금 띠(34px)
constexpr int    TOP_COLS = 17, COLS = 24, ROWS = 2, CELL_W = 5, CELL_H = 8;
constexpr double LOW_DOT = 0.92;              // 아래 면의 점은 위쪽보다 조금 작다
constexpr double DOT_GAP = 0.10;

enum { LOW_PART = 0, LOW_BANK, LOW_ICON, LOW_VOL, LOW_EXP, LOW_PAN, LOW_REV, LOW_CHO, LOW_VAR, LOW_KEY, LOW_MODE };
// 실제 기기 사진에서 잰 값 (위쪽 면의 점 간격이 단위)
constexpr double kLowX[11] = {0, 11.96, 30.5, 48.5, 56.1, 62.2, 70.3, 78.3, 86.1, 92.5, 102.9};
constexpr double kLowW[11] = {10.12, 15.64, 15, 4, 4, 7.2, 6.8, 6.7, 6.8, 8.1, 1.3};
constexpr double kModeY[4] = {-2.2, 0.6, 3.4, 6.2};
constexpr double PI = 3.14159265358979;

// ---- color ----------------------------------------------------------------------------------
struct Rgb { double r, g, b; };
constexpr Rgb kBack{150, 205, 45}, kGhost0{140, 194, 44}, kDot0{18, 22, 14}, kFaint2{147, 202, 45};
Rgb mix(Rgb a, Rgb b, double t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
uint32_t pack(Rgb c) {
    auto q = [](double v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0, 255.0))); };
    return 0xFF000000u | (q(c.b) << 16) | (q(c.g) << 8) | q(c.r);   // RGBA_8888, little endian
}
struct Ink { Rgb dot, ghost, faint; };
Ink palette(int c) {
    if (c == 2) return {kDot0, kGhost0, kFaint2};
    c = std::clamp(c, 1, 8);
    const double ghostA = c == 1 ? 0.35 : 0.06 * (8 - c) / 6.0;
    const double dotA = c == 1 ? 1.0 : 1.0 - 0.08 * (c - 2);
    return {mix(kBack, kDot0, dotA), mix(kBack, kDot0, ghostA), mix(kBack, kDot0, ghostA * 0.3)};
}

// ---- tiny 3x5 font for the printed labels under the window ------------------------------------
const char* glyph(char ch) {
    switch (ch) {
        case 'A': return "010101111101101"; case 'B': return "110101110101110";
        case 'C': return "011100100100011"; case 'E': return "111100110100111";
        case 'F': return "111100110100100"; case 'G': return "011100101101011";
        case 'H': return "101101111101101"; case 'I': return "111010010010111";
        case 'K': return "101101110101101"; case 'L': return "100100100100111";
        case 'M': return "101111111101101"; case 'N': return "110101101101101";
        case 'O': return "010101101101010"; case 'P': return "110101110100100";
        case 'R': return "110101110101101"; case 'S': return "011100010001110";
        case 'T': return "111010010010010"; case 'V': return "101101101101010";
        case 'X': return "101101010101101"; case 'Y': return "101101010010010";
        case '/': return "001001010100100"; case '#': return "101111101111101";
        default:  return nullptr;
    }
}

class Painter {
public:
    Painter(uint32_t* px, int w, int h, const Lcd2000Raw& raw)
        : m_px(px), m_w(w), m_h(h), m_raw(raw), m_ink(palette(raw.contrast)) {}

    void render();

private:
    uint32_t* m_px;
    int m_w, m_h;
    const Lcd2000Raw& m_raw;
    Ink m_ink;

    Rgb dotC() const { return m_ink.dot; }
    Rgb inkOf(bool on) const { return on ? m_ink.dot : m_ink.ghost; }

    void rect(int l, int t, int r, int b, Rgb c) {
        l = std::max(l, 0); t = std::max(t, 0); r = std::min(r, m_w); b = std::min(b, m_h);
        const uint32_t v = pack(c);
        for (int y = t; y < b; ++y)
            for (int x = l; x < r; ++x) m_px[static_cast<size_t>(y) * m_w + x] = v;
    }

    // 점 하나. w x h 는 점 간격, 오른쪽/아래에 gp 만큼 틈. 틈이 정수가 아니면 가장자리 한 줄을 배경과 섞는다.
    void dotbox(int l, int t, int w, int h, Rgb ink, double gp) {
        const int gi = static_cast<int>(gp);
        const double fr = gp - gi;
        const bool part = fr > 0.05;
        const int sw = w - gi - (part ? 1 : 0), sh = h - gi - (part ? 1 : 0);
        if (sw < 1 || sh < 1) { rect(l, t, l + std::max(1, w), t + std::max(1, h), ink); return; }
        rect(l, t, l + sw, t + sh, ink);
        if (!part) return;
        const Rgb edge = mix(ink, kBack, fr);
        rect(l + sw, t, l + sw + 1, t + sh, edge);
        rect(l, t + sh, l + sw, t + sh + 1, edge);
        rect(l + sw, t + sh, l + sw + 1, t + sh + 1, mix(ink, kBack, 1.0 - (1.0 - fr) * (1.0 - fr)));
    }

    bool bit(int v, int n) const { return (v >> n) & 1; }
    const uint8_t* cellAt(int row, int col) const { return m_raw.dots + (row * COLS + col) * CELL_H; }

    // 한 칸(5x8 점). p = 점 간격(소수 가능)
    void cell(int row, int col, double px, double py, double p) {
        const uint8_t* c = cellAt(row, col);
        auto at = [](double v) { return static_cast<int>(std::lround(v)); };
        for (int y = 0; y < CELL_H; ++y)
            for (int x = 0; x < CELL_W; ++x) {
                const int l = at(px + x * p), t = at(py + y * p);
                dotbox(l, t, at(px + (x + 1) * p) - l, at(py + (y + 1) * p) - t,
                       (m_raw.lcdOn && bit(c[y], 4 - x)) ? m_ink.dot : m_ink.ghost, DOT_GAP * p);
            }
    }

    // 23번째 칸의 제어 비트. 열 A-D = bit3-bit0, 행 0-15 = 위 칸의 0-7 + 아래 칸의 0-7
    enum { CA = 0, CB = 1, CC = 2, CD = 3 };
    bool ctl(int col, int row) const {
        if (!m_raw.lcdOn) return false;
        const int v = m_raw.dots[((row / 8) * COLS + TOP_COLS + 6) * CELL_H + (row % 8)];
        return bit(v, 3 - col);
    }

    int lx(int which) const { return kX0 + static_cast<int>(std::lround(kLowX[which] * D)); }
    int lw(int which) const { return static_cast<int>(std::lround(kLowW[which] * D)); }

    // ---- 다각형 (가장자리 부드럽게) ----
    struct Pt { double x, y; };
    static Pt polar(double cx, double cy, double r, double deg) {
        const double a = deg * PI / 180.0;
        return {cx + r * std::sin(a), cy - r * std::cos(a)};
    }
    void poly(std::initializer_list<Pt> ps, bool on) { fill(std::vector<Pt>(ps), inkOf(on)); }
    void polyC(std::initializer_list<Pt> ps, Rgb c) { fill(std::vector<Pt>(ps), c); }
    void ring(double cx, double cy, double r0, double r1, double a0, double a1, bool on) {
        const int n = std::max(8, std::min(180, static_cast<int>((a1 - a0) / 2)));
        std::vector<Pt> v;
        for (int i = 0; i <= n; ++i) v.push_back(polar(cx, cy, r1, a0 + (a1 - a0) * i / n));
        for (int i = n; i >= 0; --i) v.push_back(polar(cx, cy, r0, a0 + (a1 - a0) * i / n));
        fill(v, inkOf(on));
    }
    void fill(const std::vector<Pt>& p, Rgb c);

    void text(double x, double y, const char* s, int scale, Rgb c, bool centered) {
        int n = 0;
        while (s[n]) ++n;
        const int w = n * 4 * scale - scale;
        int ox = static_cast<int>(std::lround(x)) - (centered ? w / 2 : 0);
        const int oy = static_cast<int>(std::lround(y));
        for (int i = 0; i < n; ++i, ox += 4 * scale) {
            const char* g = glyph(s[i]);
            if (!g) continue;
            for (int r = 0; r < 5; ++r)
                for (int q = 0; q < 3; ++q)
                    if (g[r * 3 + q] == '1')
                        rect(ox + q * scale, oy + r * scale, ox + (q + 1) * scale, oy + (r + 1) * scale, c);
        }
    }

    void seven(double x, double y, double w, double h, double t, unsigned seg);
    void fan(int which, const bool* on8);
    void drawUpperFace();
    void drawLowerFace();
    void drawSegments();
    void drawLabels();
};

void Painter::fill(const std::vector<Pt>& p, Rgb c) {
    const size_t n = p.size();
    if (n < 3) return;
    double minx = p[0].x, maxx = p[0].x, miny = p[0].y, maxy = p[0].y;
    for (const Pt& q : p) { minx = std::min(minx, q.x); maxx = std::max(maxx, q.x); miny = std::min(miny, q.y); maxy = std::max(maxy, q.y); }
    const int x0 = std::max(0, static_cast<int>(std::floor(minx))), y0 = std::max(0, static_cast<int>(std::floor(miny)));
    const int x1 = std::min(m_w, static_cast<int>(std::ceil(maxx))), y1 = std::min(m_h, static_cast<int>(std::ceil(maxy)));
    const int sw = x1 - x0, sh = y1 - y0;
    if (sw <= 0 || sh <= 0) return;
    constexpr int SS = 4;
    std::vector<float> cov(static_cast<size_t>(sw) * sh, 0.0f);
    std::vector<double> xs;
    for (int r = 0; r < sh * SS; ++r) {
        const double ys = y0 + (r + 0.5) / SS;
        xs.clear();
        for (size_t i = 0; i < n; ++i) {
            const Pt& a = p[i];
            const Pt& b = p[(i + 1) % n];
            if ((a.y <= ys && b.y > ys) || (b.y <= ys && a.y > ys))
                xs.push_back(a.x + (ys - a.y) * (b.x - a.x) / (b.y - a.y));
        }
        std::sort(xs.begin(), xs.end());
        float* row = &cov[static_cast<size_t>(r / SS) * sw];
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            const double a = xs[i] - x0, b = xs[i + 1] - x0;
            for (int col = std::max(0, static_cast<int>(std::floor(a))); col < std::min(sw, static_cast<int>(std::ceil(b))); ++col) {
                const double o = std::min(b, col + 1.0) - std::max(a, static_cast<double>(col));
                if (o > 0) row[col] += static_cast<float>(o / SS);
            }
        }
    }
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            const double a = std::min(1.0f, cov[static_cast<size_t>(y) * sw + x]);
            if (a <= 0) continue;
            uint32_t& d = m_px[static_cast<size_t>(y0 + y) * m_w + (x0 + x)];
            auto ch = [&](int sh2, double v) {
                return static_cast<uint32_t>(std::lround(v * a + ((d >> sh2) & 0xff) * (1 - a))) << sh2;
            };
            d = 0xFF000000u | ch(16, c.b) | ch(8, c.g) | ch(0, c.r);
        }
}

void Painter::seven(double x, double y, double w, double h, double t, unsigned seg) {
    const double k = std::max(0.5, 0.1 * D);                // 세그먼트 사이 틈
    const double ym = y + h / 2, ht = t / 2;
    poly({{x + k, y}, {x + w - k, y}, {x + w - t - k, y + t}, {x + t + k, y + t}}, bit(seg, 0));                         // a
    poly({{x + w, y + k}, {x + w, ym - ht - k}, {x + w - ht, ym - k}, {x + w - t, ym - ht - k}, {x + w - t, y + t + k}}, bit(seg, 1));  // b
    poly({{x + w, ym + ht + k}, {x + w, y + h - k}, {x + w - t, y + h - t - k}, {x + w - t, ym + ht + k}, {x + w - ht, ym + k}}, bit(seg, 2)); // c
    poly({{x + t + k, y + h - t}, {x + w - t - k, y + h - t}, {x + w - k, y + h}, {x + k, y + h}}, bit(seg, 3));          // d
    poly({{x, ym + ht + k}, {x + ht, ym + k}, {x + t, ym + ht + k}, {x + t, y + h - t - k}, {x, y + h - k}}, bit(seg, 4)); // e
    poly({{x, y + k}, {x + t, y + t + k}, {x + t, ym - ht - k}, {x + ht, ym - k}, {x, ym - ht - k}}, bit(seg, 5));         // f
    poly({{x + ht + k, ym}, {x + t + k, ym - ht}, {x + w - t - k, ym - ht}, {x + w - ht - k, ym}, {x + w - t - k, ym + ht}, {x + t + k, ym + ht}}, bit(seg, 6)); // g
}

void Painter::fan(int which, const bool* on8) {
    const double dv = LOW_DOT * D;
    const double cx = lx(which) + lw(which) / 2.0, cy = kSy + 8.4 * dv;
    for (int k = 0; k < 8; ++k) {
        const double r = (1.45 + 1.0 * k) * D;
        const double t = std::max(0.42 * D, 1.0) / 2;
        ring(cx, cy, r - t, r + t, -22.5, 22.5, on8[k]);
    }
}

void Painter::drawUpperFace() {
    // 0-8 칸: 레벨미터(1칸 = 2개), 9-16 칸: 문자 8자. 행 사이는 비우지 않는다
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < TOP_COLS; ++col)
            cell(row, col, kX0 + col * (CELL_W + 1) * D, kY0 + row * CELL_H * D, D);
}

void Painter::drawLowerFace() {
    const double ld = LOW_DOT * D;
    for (int i = 0; i < 2; ++i) cell(0, TOP_COLS + i, lx(LOW_PART) + i * (CELL_W + 1) * ld, kSy, ld);
    for (int i = 0; i < 3; ++i) cell(1, TOP_COLS + i, lx(LOW_BANK) + i * (CELL_W + 1) * ld, kSy, ld);

    // 악기 모양: 20-22 칸의 두 행이 한 장의 그림 (점이 가로로 길다)
    const int ix = lx(LOW_ICON), iw = lw(LOW_ICON);
    const int seg_h = 8 * D;
    const int first = TOP_COLS + 3, last = COLS - 1;
    const int nx = (last - first) * CELL_W, ny = ROWS * CELL_H;
    for (int row = 0; row < ROWS; ++row)
        for (int col = first; col < last; ++col) {
            const uint8_t* c = cellAt(row, col);
            for (int y = 0; y < CELL_H; ++y) {
                const int yy = row * CELL_H + y;
                const int top = kSy + yy * seg_h / ny, bot = kSy + (yy + 1) * seg_h / ny;
                for (int x = 0; x < CELL_W; ++x) {
                    const int xx = (col - first) * CELL_W + x;
                    const int left = ix + xx * iw / nx, right = ix + (xx + 1) * iw / nx;
                    dotbox(left, top, std::max(1, right - left), std::max(1, bot - top),
                           (m_raw.lcdOn && bit(c[y], 4 - x)) ? m_ink.dot : m_ink.faint, DOT_GAP * D);
                }
            }
        }
}

void Painter::drawSegments() {
    const double dd = D, dv = LOW_DOT * dd;
    const double px = 1.0;
    auto thick = [&](double t) { return std::max(t, px); };
    const int sy = kSy;

    // VOL / EXP: 행 0 의 19번째 칸에 레벨미터와 같은 형태로 들어 있다 (왼쪽 2점 = VOL, 오른쪽 2점 = EXP)
    {
        const uint8_t* c = cellAt(0, TOP_COLS + 2);
        const double want = 8.3 * dv / 8;
        const double pitch = std::max(1.0, std::round(want / px)) * px;
        const int bar = std::max(static_cast<int>(std::lround(px)), static_cast<int>(std::lround(thick(0.48 * pitch))));
        const double start = std::round((sy - 0.5 * dv + 4 * (want - pitch)) / px) * px;
        for (int y = 0; y < CELL_H; ++y) {
            const int top = static_cast<int>(std::lround(start + y * pitch)), bot = top + bar;
            const bool vol = m_raw.lcdOn && (bit(c[y], 4) || bit(c[y], 3));
            const bool exp = m_raw.lcdOn && (bit(c[y], 1) || bit(c[y], 0));
            rect(lx(LOW_VOL), top, lx(LOW_VOL) + lw(LOW_VOL), bot, inkOf(vol));
            rect(lx(LOW_EXP), top, lx(LOW_EXP) + lw(LOW_EXP), bot, inkOf(exp));
        }
    }

    // PAN: 열린 원호 안에서 바늘이 45도 간격 7곳으로 움직인다 (D15 왼쪽 아래, D12 위, D9 오른쪽 아래)
    {
        const double cx = lx(LOW_PAN) + lw(LOW_PAN) / 2.0, cy = sy + 3.75 * dv;
        const double r = 3.6 * dd;
        ring(cx, cy, r - thick(0.25 * dd), r, -124.0, 124.0, m_raw.lcdOn);
        const double r0 = 0.29 * r, r1 = 0.72 * r, ht = thick(0.36 * dd) / 2;
        for (int k = 0; k < 7; ++k) {
            const bool on = ctl(CD, 15 - k);
            const double a = -135.0 + 45.0 * k;
            const Pt p0 = polar(cx, cy, r0, a), p1 = polar(cx, cy, r1, a);
            const double nx = std::cos(a * PI / 180.0) * ht, ny = std::sin(a * PI / 180.0) * ht;
            poly({{p0.x - nx, p0.y - ny}, {p1.x - nx, p1.y - ny}, {p1.x + nx, p1.y + ny}, {p0.x + nx, p0.y + ny}}, on);
        }
    }

    // REV / CHO / VAR 센드량 부채꼴
    {
        bool rev[8], cho[8], var[8];
        for (int k = 0; k < 8; ++k) { rev[k] = ctl(CA, 15 - k); cho[k] = ctl(CB, 15 - k); var[k] = ctl(CC, 15 - k); }
        fan(LOW_REV, rev); fan(LOW_CHO, cho); fan(LOW_VAR, var);
    }

    // KEY(노트 시프트): 부호 + 7세그 두 자리
    {
        const double t = thick(0.4 * dd);
        const double dh = 5.4 * dv, dy = sy + 2.2 * dv, dw = 2.6 * dd;
        const double kx = lx(LOW_KEY);
        const double sw = 2.4 * dd, cy = dy + dh / 2, vh = 0.62 * dh;
        const double vx = kx + sw / 2, cut = std::max(px, 0.3 * dd);
        poly({{kx, cy - t / 2}, {kx + sw, cy - t / 2}, {kx + sw, cy + t / 2}, {kx, cy + t / 2}}, ctl(CB, 0));
        const bool plus = ctl(CA, 0);
        poly({{vx - t / 2, cy - vh / 2}, {vx + t / 2, cy - vh / 2}, {vx + t / 2, cy - t / 2 - cut}, {vx - t / 2, cy - t / 2 - cut}}, plus);
        poly({{vx - t / 2, cy + t / 2 + cut}, {vx + t / 2, cy + t / 2 + cut}, {vx + t / 2, cy + vh / 2}, {vx - t / 2, cy + vh / 2}}, plus);

        unsigned ten = 0;
        if (ctl(CA, 1)) ten |= (1u << 0) | (1u << 3) | (1u << 4) | (1u << 6);   // 십의 자리는 a/d/e/g 한 묶음
        if (ctl(CB, 1)) ten |= 1u << 1;
        if (ctl(CA, 7)) ten |= 1u << 2;
        if (ctl(CB, 4)) ten |= 1u << 5;
        seven(kx + sw + 0.2 * dd, dy, dw, dh, t, ten);

        unsigned one = 0;
        if (ctl(CB, 6)) one |= 1u << 0;
        if (ctl(CA, 6)) one |= 1u << 1;
        if (ctl(CA, 3)) one |= 1u << 2;
        if (ctl(CA, 2)) one |= 1u << 3;
        if (ctl(CB, 2)) one |= 1u << 4;
        if (ctl(CB, 7)) one |= 1u << 5;
        if (ctl(CB, 3)) one |= 1u << 6;
        seven(kx + sw + 0.2 * dd + dw + 0.5 * dd, dy, dw, dh, t, one);
    }

    // 맨 오른쪽 모드 표시 ▶ (위 하나는 이름이 없다. 나머지가 XG / GS / PERFORM)
    {
        const double mx = lx(LOW_MODE);
        const double th = 1.45 * dv, tw = th * 0.9;
        const bool mode[4] = {false, ctl(CB, 5), ctl(CA, 4), ctl(CA, 5)};
        for (int k = 0; k < 4; ++k) {
            const double cy = sy + kModeY[k] * dv;
            poly({{mx, cy - th / 2}, {mx + tw, cy}, {mx, cy + th / 2}}, mode[k]);
        }
    }

    // 아래 면 위에 뜨는 ▼ 커서: 지금 만지고 있는 항목
    {
        const int cur_y = sy - std::max(2, static_cast<int>(std::lround(1.6 * LOW_DOT * D)));
        const int hw = std::max(2, D);
        struct { int at; bool on; } cur[] = {
            {LOW_VOL, ctl(CC, 3)}, {LOW_EXP, ctl(CC, 4)}, {LOW_PAN, ctl(CC, 5)}, {LOW_REV, ctl(CC, 6)},
            {LOW_CHO, ctl(CC, 7)}, {LOW_VAR, ctl(CD, 6)}, {LOW_KEY, ctl(CD, 7)},
        };
        for (const auto& c : cur) {
            if (!c.on) continue;
            const int cx = lx(c.at) + lw(c.at) / 2;
            polyC({{double(cx - hw), double(cur_y - hw)}, {double(cx + hw), double(cur_y - hw)}, {double(cx), double(cur_y)}}, m_ink.dot);
        }
        // 뱅크 / 프로그램 번호 커서는 악기 모양 위
        const int ix = lx(LOW_ICON);
        const int tops[2] = {ix + (3 + 5) * D / 2, ix + (11 + 13) * D / 2};
        const bool ton[2] = {ctl(CC, 1), ctl(CC, 0)};
        for (int k = 0; k < 2; ++k) {
            if (!ton[k]) continue;
            polyC({{double(tops[k] - hw), double(cur_y - hw)}, {double(tops[k] + hw), double(cur_y - hw)}, {double(tops[k]), double(cur_y)}}, m_ink.dot);
        }
    }
}

// 실물에는 창 아래/옆에 인쇄돼 있는 이름표
void Painter::drawLabels() {
    const Rgb c = mix(kBack, kDot0, 0.7);
    const int y = kSy + 66;
    struct { int at; const char* s; } cols[] = {
        {LOW_PART, "PART"}, {LOW_ICON, "BANK/PGM#"}, {LOW_VOL, "VOL"}, {LOW_EXP, "EXP"}, {LOW_PAN, "PAN"},
        {LOW_REV, "REV"}, {LOW_CHO, "CHO"}, {LOW_VAR, "VAR"}, {LOW_KEY, "KEY"},
    };
    for (const auto& l : cols) text(lx(l.at) + lw(l.at) / 2.0, y, l.s, 2, c, true);
    const double dv = LOW_DOT * D;
    const char* modes[3] = {"XG", "GS", "PERFORM"};
    for (int k = 1; k < 4; ++k) {
        const double th = 1.45 * dv;
        text(lx(LOW_MODE) + th * 0.9 + 4, kSy + kModeY[k] * dv - 5, modes[k - 1], 2, c, false);
    }
}

void Painter::render() {
    std::fill(m_px, m_px + static_cast<size_t>(m_w) * m_h, pack(kBack));
    drawUpperFace();
    drawLowerFace();
    drawSegments();
    drawLabels();
}

} // namespace

bool lcd2000Equal(const Lcd2000Raw& a, const Lcd2000Raw& b) {
    return a.valid == b.valid && a.lcdOn == b.lcdOn && a.contrast == b.contrast &&
           std::memcmp(a.dots, b.dots, sizeof(a.dots)) == 0;
}

void lcd2000FrameSize(int* width, int* height) {
    *width = kW;
    *height = kH;
}

void lcd2000Render(const Lcd2000Raw& raw, uint32_t* dst) {
    if (!dst) return;
    Painter(dst, kW, kH, raw).render();
}

} // namespace erayMidi
