// Lcd2000Renderer.h
//
// YAMAHA MU2000(S-MU2000 LLE)의 LCD를 RGBA 비트맵으로 합성한다.
//
// mu2000::lcd_render()가 주는 HD44780 도트(2행 x 24칸 x 8줄, 각 바이트의 하위 5비트)를
// 입력으로 받는다. 이 도트는 글자 그림만이 아니라 실제 유리판의 "면"별로 의미가 다르다:
//   - 위의 면   : 0~16칸. 0~8칸 = 레벨미터(18개), 9~16칸 = 문자 8자(음색명 / 번호)
//   - 아래의 면 : 17~19칸 = 파트 번호 "01" / 뱅크 "A01", 20~22칸 = 악기 모양(두 행이 한 장의 그림)
//   - 23칸     : 그림이 아니라 64개의 제어 비트. 고정 모양 세그먼트(VOL/EXP 막대, PAN 바늘,
//                REV/CHO/VAR 부채꼴, KEY 7세그, XG/GS/PERFORM 표시, 커서)를 켜고 끈다
// 배치와 모양은 tarboh/S-MU2000 의 src/ui/panel.cpp (draw_lcd_body) 를 따랐다 (BSD-3-Clause).
// 안드로이드/JNI와 무관한 순수 C++이며 Kotlin 백그라운드 스레드에서 호출하는 것을 전제로 한다.
#pragma once

#include <cstdint>

namespace erayMidi {

struct Lcd2000Raw {
    static constexpr int kRows = 2, kCols = 24, kCellH = 8;
    bool    valid = false;      // 아직 한 번도 채워지지 않았으면 false
    bool    lcdOn = false;
    int     contrast = 2;       // UTIL > SYS 의 Contrast 1..8 (작을수록 진함)
    uint8_t dots[kRows * kCols * kCellH] = {};
};

bool lcd2000Equal(const Lcd2000Raw& a, const Lcd2000Raw& b);
void lcd2000FrameSize(int* width, int* height);
// dst는 width*height 개의 uint32_t (RGBA_8888, stride = width).
void lcd2000Render(const Lcd2000Raw& raw, uint32_t* dst);

} // namespace erayMidi
