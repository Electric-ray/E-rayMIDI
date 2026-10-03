// GearmulatorEngine.h
//
// 88emu(emu88 C API, vendor/gearmulator/source/ronaldo/88emu/88lib)를 감싸는
// 얇은 C++ 어댑터. GearmulatorBridge.cpp(JNI)가 AAudio/스레딩을 맡고, 이
// 파일은 emu88_* 호출만 책임진다 — 개발계획서 §20의 MidiEngine 계약을 따른다.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ronaldo/88emu/88lib/c_interface.h"
#include "Lcd88Renderer.h"

namespace erayMidi {

class GearmulatorEngine final {
public:
    explicit GearmulatorEngine(emu88_device_id deviceId);
    ~GearmulatorEngine();

    void addRomPath(const std::string& path);
    void setOutputSampleRate(double sampleRate); // 0 = 디바이스 네이티브 레이트(리샘플링 없음)

    bool init();      // ROM 로드 + 펌웨어 부팅까지 블로킹 — 반드시 백그라운드 스레드에서 호출
    void reset();      // 소프트 리셋 (All Notes/Sound Off 방식, GS/GM 리셋 아님)
    void hardReset();  // 디바이스 팩토리 리셋 SysEx/커맨드
    void sendMidi(uint8_t status, uint8_t data1, uint8_t data2);
    void sendSysEx(const uint8_t* data, size_t length);
    void renderInt16(int16_t* interleavedOut, int frames); // AAudio 콜백에서 직접 호출
    void shutdown();

    bool isOpen() const;
    uint32_t actualSampleRate() const;
    const char* deviceName() const;
    emu88_device_id deviceId() const { return m_deviceId; }

    // 실제 기기 디스플레이/패널 상태 (MainActivity의 LCD 뷰/LED 패널이 사용).
    // 문자 LCD가 없는 기종(SC-55 등 그래픽 LCD)에서는 빈 문자열을 돌려준다
    // (c_interface.h: "on a graphic display like the SC-8850's").
    std::string getDisplayText(unsigned screen = 0) const;
    bool isDisplayOn(unsigned screen = 0) const;
    uint32_t getPanelLeds() const;

    // 실제 LCD의 원시 내용(Character: DDRAM/CGRAM, Graphic: 도트 그리드). 렌더 스레드에서만 호출할 것
    // (emu88_context는 스레드 세이프하지 않다). out.look은 호출자가 채운다.
    bool getDisplayRaw(unsigned screen, Lcd88Raw& out) const;
    // 이 기종의 LCD가 어떤 모양으로 그려져야 하는지(유리 색/배율).
    Lcd88Look lcdLook() const;

private:
    emu88_context m_context = nullptr;
    emu88_device_id m_deviceId;
    double m_outputSampleRate = 0.0;
};

// "sc55", "sc88pro", "mt32new" 등 88emuPlayer CLI ID와 동일한 문자열 <-> emu88_device_id.
bool resolveDeviceId(const std::string& cliId, emu88_device_id* outId);
const char* deviceCliId(emu88_device_id id);

} // namespace erayMidi
