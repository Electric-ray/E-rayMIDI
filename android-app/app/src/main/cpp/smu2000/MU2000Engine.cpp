// MU2000Engine.cpp
#include "MU2000Engine.h"
#include "source/mu2000.h"
#include "lcd_cgrom_fallback.h"

namespace erayMidi {

MU2000Engine::MU2000Engine() : m_mu(std::make_unique<mu2000>()) {}
MU2000Engine::~MU2000Engine() = default;

bool MU2000Engine::loadProgram(const std::string& path) {
    return m_mu->load_program(path);
}

bool MU2000Engine::loadWave(const std::string& dir) {
    return m_mu->load_wave(dir);
}

bool MU2000Engine::loadSintab(const std::string& path) {
    if (path.empty()) return true; // 선택 사항
    return m_mu->load_sintab(path);
}

// hd44780_device::render()는 문자 ROM(CGROM)이 없으면 전부 0인 이미지를 돌려준다. 즉 폰트가 없으면
// 소리는 나도 LCD가 완전히 빈 화면이 된다 (레벨미터 글자도 CGROM 위에서 합성되기 때문).
// 그래서 진짜 hd44780u_b04.bin을 못 읽으면 내장 5x8 폰트로 CGROM을 채운다.
// mu2000::set_lcd_font()가 이 위에 레벨미터/PAN 글자(0x7f~)를 덮어쓴다.
bool MU2000Engine::loadLcdFont(const std::string& path) {
    if (!path.empty() && m_mu->load_lcd_font(path)) return true;
    auto rom = std::make_shared<std::vector<uint8_t>>(0x1000, uint8_t{0});
    for (int ch = 0x10; ch <= 0xFF; ++ch)
        for (int row = 0; row < 8; ++row)
            (*rom)[static_cast<size_t>(ch) * 16 + row] = kFallbackCgrom[ch - 0x10][row] & 0x1F;
    m_mu->set_lcd_font(std::move(rom));
    return true;   // 외부 폰트가 없는 것은 오류가 아니다
}

void MU2000Engine::setThreaded(bool on) {
    m_mu->set_threaded(on);
}

void MU2000Engine::reset() {
    m_mu->reset();
}

static inline int16_t clampS16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

void MU2000Engine::renderSample(int16_t& left, int16_t& right) {
    int32_t l = 0, r = 0;
    m_mu->run_sample(l, r);
    // mu2000.h 주석: 출력은 MAME 내부 스케일(전체 진폭 = DAC_FULL_SCALE = 1<<17).
    // 16bit PCM으로 만들려면 >> 2 해야 한다("16bit にするときは >> 2").
    left = clampS16(l >> 2);
    right = clampS16(r >> 2);
}

void MU2000Engine::midiInByte(uint8_t b) {
    m_mu->midi_in(b, 0);
}

bool MU2000Engine::isBootReady() const {
    return m_mu->midi_ready(0);
}

void MU2000Engine::lcdSnapshot(Lcd2000Raw& out) {
    hd44780_device& lcd = m_mu->lcd();
    const uint8_t* img = m_mu->lcd_render();
    const int cols = lcd.line_size();   // 2행 x 40칸, 창에 보이는 건 왼쪽 24칸
    for (int row = 0; row < Lcd2000Raw::kRows; ++row)
        for (int col = 0; col < Lcd2000Raw::kCols; ++col)
            for (int y = 0; y < Lcd2000Raw::kCellH; ++y)
                out.dots[(row * Lcd2000Raw::kCols + col) * Lcd2000Raw::kCellH + y] =
                    img[16 * (row * cols + col) + y];
    out.lcdOn = lcd.display_on();
    out.contrast = m_mu->lcd_contrast();
    out.valid = true;
}

const char* MU2000Engine::lastError() const {
    return m_mu->error().c_str();
}

} // namespace erayMidi
