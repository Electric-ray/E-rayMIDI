// GearmulatorEngine.cpp
#include "GearmulatorEngine.h"

namespace erayMidi {

GearmulatorEngine::GearmulatorEngine(emu88_device_id deviceId)
    : m_deviceId(deviceId) {
    m_context = emu88_create_context();
}

GearmulatorEngine::~GearmulatorEngine() {
    shutdown();
    emu88_free_context(m_context);
    m_context = nullptr;
}

void GearmulatorEngine::addRomPath(const std::string& path) {
    emu88_add_rom_path(path.c_str());
}

void GearmulatorEngine::setOutputSampleRate(double sampleRate) {
    m_outputSampleRate = sampleRate;
    if (m_context) {
        emu88_set_stereo_output_samplerate(m_context, sampleRate);
    }
}

bool GearmulatorEngine::init() {
    if (!m_context) return false;
    if (emu88_select_device(m_context, m_deviceId) != EMU88_RC_OK) return false;

    emu88_set_boot_flags(m_context, EMU88_BOOT_DEFAULT);
    emu88_set_stereo_output_samplerate(m_context, m_outputSampleRate);

    // 블로킹: 실제 펌웨어를 부팅까지 돌린다(에뮬레이션 시간 기준 몇 초).
    // 반드시 UI/오디오 콜백이 아닌 별도 부팅 스레드에서 호출할 것.
    return emu88_open_synth(m_context) == EMU88_RC_OK;
}

void GearmulatorEngine::reset() {
    // All Sound Off류 — 소리만 끊고 패치/파라미터는 그대로 둔다.
    if (m_context) emu88_play_silence(m_context);
}

void GearmulatorEngine::hardReset() {
    // 디바이스 고유의 완전 리셋(Sound Canvas 계열: GS Reset / MT-32 계열: 전체 파라미터 리셋).
    // MT-32 계열은 리셋 후 돌아오기까지 시간이 걸린다(emu88 문서 참고).
    if (m_context) emu88_play_device_reset(m_context);
}

void GearmulatorEngine::sendMidi(uint8_t status, uint8_t data1, uint8_t data2) {
    if (!m_context) return;
    const uint32_t msg = static_cast<uint32_t>(status)
                        | (static_cast<uint32_t>(data1) << 8)
                        | (static_cast<uint32_t>(data2) << 16);
    emu88_play_msg(m_context, msg);
}

void GearmulatorEngine::sendSysEx(const uint8_t* data, size_t length) {
    if (!m_context || !data || length == 0) return;
    emu88_play_sysex(m_context, data, static_cast<uint32_t>(length));
}

void GearmulatorEngine::renderInt16(int16_t* interleavedOut, int frames) {
    if (!m_context || frames <= 0 || !interleavedOut) return;
    emu88_render_bit16s(m_context, interleavedOut, static_cast<uint32_t>(frames));
}

void GearmulatorEngine::shutdown() {
    if (m_context) emu88_close_synth(m_context);
}

bool GearmulatorEngine::isOpen() const {
    return m_context && emu88_is_open(m_context);
}

uint32_t GearmulatorEngine::actualSampleRate() const {
    return m_context ? emu88_get_actual_stereo_output_samplerate(m_context) : 0;
}

const char* GearmulatorEngine::deviceName() const {
    return emu88_get_device_name(m_deviceId);
}

std::string GearmulatorEngine::getDisplayText(unsigned screen) const {
    if (!m_context) return {};
    size_t needed = emu88_get_display_text(m_context, screen, nullptr, 0);
    if (needed == 0) return {};
    std::string s(needed, '\0');
    emu88_get_display_text(m_context, screen, s.data(), s.size() + 1);
    return s;
}

bool GearmulatorEngine::isDisplayOn(unsigned screen) const {
    return m_context && emu88_is_display_on(m_context, screen);
}

uint32_t GearmulatorEngine::getPanelLeds() const {
    return m_context ? emu88_get_panel_leds(m_context) : 0;
}

namespace {
struct DeviceEntry { const char* cliId; emu88_device_id id; };
constexpr DeviceEntry kDeviceTable[] = {
    {"mt32old",  EMU88_DEVICE_MT32_OLD},
    {"mt32new",  EMU88_DEVICE_MT32_NEW},
    {"cm32l",    EMU88_DEVICE_CM32L},
    {"cm32ln",   EMU88_DEVICE_CM32LN},
    {"cm32p",    EMU88_DEVICE_CM32P},
    {"cm64",     EMU88_DEVICE_CM64},
    {"sc55",     EMU88_DEVICE_SC55},
    {"sc55mk2",  EMU88_DEVICE_SC55MK2},
    {"sc55st",   EMU88_DEVICE_SC55ST},
    {"cm300",    EMU88_DEVICE_CM300},
    {"sc155",    EMU88_DEVICE_SC155},
    {"sc155mk2", EMU88_DEVICE_SC155MK2},
    {"scc1a",    EMU88_DEVICE_SCC1A},
    {"scb55",    EMU88_DEVICE_SCB55},
    {"rlp3237",  EMU88_DEVICE_RLP3237},
    {"sc88",     EMU88_DEVICE_SC88},
    {"sc88vl",   EMU88_DEVICE_SC88VL},
    {"xpgs",     EMU88_DEVICE_XPGS},
    {"sc88pro",  EMU88_DEVICE_SC88PRO},
    {"vegspro",  EMU88_DEVICE_VEGSPRO},
    {"sc8820",   EMU88_DEVICE_SC8820},
    {"sc8850",   EMU88_DEVICE_SC8850},
    {"nu10b",    EMU88_DEVICE_NU10B},
    {"miig5",    EMU88_DEVICE_MIIG5},
};
} // namespace

bool resolveDeviceId(const std::string& cliId, emu88_device_id* outId) {
    for (const auto& e : kDeviceTable) {
        if (cliId == e.cliId) { *outId = e.id; return true; }
    }
    return false;
}

const char* deviceCliId(emu88_device_id id) {
    for (const auto& e : kDeviceTable) {
        if (e.id == id) return e.cliId;
    }
    return "unknown";
}

} // namespace erayMidi
