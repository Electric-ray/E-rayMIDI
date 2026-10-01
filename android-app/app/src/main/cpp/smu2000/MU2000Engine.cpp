// MU2000Engine.cpp
#include "MU2000Engine.h"
#include "source/mu2000.h"

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

bool MU2000Engine::loadLcdFont(const std::string& path) {
    if (path.empty()) return true; // 선택 사항 (대체 폰트로 자동 대체됨)
    return m_mu->load_lcd_font(path);
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

const char* MU2000Engine::lastError() const {
    return m_mu->error().c_str();
}

} // namespace erayMidi
