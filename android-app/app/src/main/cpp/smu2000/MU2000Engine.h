// MU2000Engine.h
//
// tarboh/S-MU2000 (SH7042 + SWP30x2, YAMAHA MU2000 LLE 에뮬레이터)의
// mu2000 클래스를 감싸는 얇은 C++ 어댑터. MU2000Bridge.cpp(JNI)가
// AAudio/스레딩을 맡고, 이 파일은 mu2000 클래스 호출만 책임진다.
//
// 88emu(GearmulatorEngine)와의 핵심 차이:
//   - MIDI가 3바이트 패킹이 아니라 "1바이트씩" 들어간다(midi_in(u8, port)).
//     스트림 파싱은 SH7042 펌웨어 내부가 하므로 SysEx도 그냥 바이트를
//     그대로 흘려보내면 된다 — 별도 SysEx 경계 처리가 필요 없다.
//   - 블로킹 "부팅 함수"가 없다. reset() 자체는 즉시 반환하고, 그 뒤
//     run_sample()을 계속 호출하는 것 자체가 부팅 과정이다. 펌웨어가
//     부팅을 마쳤다는 신호는 midi_ready() — 이걸 기다리지 않고 MIDI를
//     흘려보내면 곡 시작부의 리셋/음색 지정이 다 버려진다(업스트림 주석).
//   - 출력이 44100Hz 고정이다(SWP30 하드웨어 고유 레이트, 88lib처럼 내부
//     리샘플러가 없다) — AAudio가 44100Hz를 못 받으면 직접 리샘플링해야
//     한다.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

class mu2000;

namespace erayMidi {

class MU2000Engine final {
public:
    MU2000Engine();
    ~MU2000Engine();

    // ROM 로딩. 프로그램 ROM은 정확한 파일 경로, 웨이브 ROM은
    // xv364a0.ic49/xv365a0.ic50/xw848a0.ic53/xw849a0.ic54 4개 파일이 있는
    // 디렉터리, sintab/lcd_font는 없어도(경로가 비어있으면) 동작한다.
    bool loadProgram(const std::string& path);
    bool loadWave(const std::string& dir);
    bool loadSintab(const std::string& path); // 실패해도 치명적이지 않음
    bool loadLcdFont(const std::string& path); // 없으면 대체 폰트 사용

    void setThreaded(bool on); // 초기 이식은 안전하게 false로 시작 권장
    void reset();

    // 렌더 스레드에서 계속 호출 — 부팅 진행 + 오디오 렌더링을 겸한다.
    void renderSample(int16_t& left, int16_t& right);

    // MIDI 입력 (1바이트씩, port 0 = DIN A 고정)
    void midiInByte(uint8_t b);

    bool isBootReady() const;       // midi_ready(0) — 이게 true여야 MIDI를 흘려보내도 안전
    const char* lastError() const;

private:
    std::unique_ptr<mu2000> m_mu;
};

} // namespace erayMidi
