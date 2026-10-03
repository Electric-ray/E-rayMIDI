// Lcd88Renderer.h
//
// 88emu가 펌웨어 대신 "그려주지 않는" LCD를 앱에서 그리기 위한 렌더러.
//
//   emu88_get_display_raw() (c_interface.h)가 주는 원시 데이터
//     - Character : HD44780 DDRAM 80B + CGRAM 64B  (SC-55 / SC-88 계열)
//     - Graphic   : 도트 그리드 width x height     (SC-8850, MT-32/CM 계열)
//   를 RGBA_8888(리틀엔디언 uint32 = A<<24|B<<16|G<<8|R) 픽셀로 합성한다.
//
// Character 계열의 글자 배치는 88emuplayer/ui/panel.cpp(= Nuked-SC55 LCD_Render와 동일)를
// 따른다. 이 파일은 안드로이드/JNI와 무관한 순수 C++이며, 오디오 스레드가 아닌
// 곳(Kotlin 백그라운드 스레드)에서 호출하는 것을 전제로 한다.
#pragma once

#include <cstdint>

namespace erayMidi {

// 같은 raw 데이터라도 기종에 따라 유리/도트 색과 배율이 다르다.
enum class Lcd88Look : int {
    Sc55Glass = 0,  // Character형 (741x268, 주황 유리 + 검은 도트)
    Sc8850    = 1,  // Graphic형, 주황 유리, 도트 4x4 (160x64 -> 640x256)
    CmGreen   = 2,  // Graphic형, MT-32 / CM-32L / CM-32P의 연두색 유리
};

struct Lcd88Raw {
    static constexpr int kMonoMax = 16384;  // SC-8850 160x64 = 10240이 최대

    int  type = 0;              // 0 없음, 1 Character, 2 Graphic (emu88_display_info.type)
    int  width = 0, height = 0; // Graphic일 때 도트 수
    bool displayOn = false;
    bool powered = true;
    uint16_t leds = 0;
    Lcd88Look look = Lcd88Look::Sc55Glass;
    uint8_t dd[80] = {};
    uint8_t cg[64] = {};
    int     monoLen = 0;
    uint8_t mono[kMonoMax] = {};
};

// 화면에 영향을 주는 내용(revision/leds 제외)이 같은지.
bool lcd88Equal(const Lcd88Raw& a, const Lcd88Raw& b);

// 결과 비트맵 크기. 그릴 것이 없으면 false.
bool lcd88FrameSize(const Lcd88Raw& raw, int* width, int* height);

// dst는 width*height 개의 uint32_t (lcd88FrameSize로 얻은 크기, stride = width).
void lcd88Render(const Lcd88Raw& raw, uint32_t* dst);

} // namespace erayMidi
