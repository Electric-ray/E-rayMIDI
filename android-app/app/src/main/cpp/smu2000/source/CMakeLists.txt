# smu2000/source/CMakeLists.txt
#
# tarboh/S-MU2000 (YAMAHA MU2000, SH7042+SWP30x2 LLE 에뮬레이터)의 코어를
# 최소 서브셋으로 벤더링해서 독립적으로 빌드하기 위한 CMakeLists.
#
# 원본 Makefile의 SRCS(= OBJS) 목록 + src/mu2000.cpp(메인 클래스, Makefile에서
# 모든 실행파일 타겟에 별도로 얹는 방식)를 그대로 따왔다. VST3/VSTi 래퍼,
# live/render/gui 등 데스크톱 전용 프런트엔드는 전부 제외했다 — 이 프로젝트는
# JNI 라이브러리 하나(mu2000 클래스)만 필요하기 때문이다.
#
# upstream과 동일한 상대 경로 구조(compat/, mame/cpu/, mame/sound/, ...)를
# 유지해서, 나중에 upstream이 갱신되면 같은 서브셋만 다시 복사해오면 된다.
#
# 고정 커밋: d44b0891cb9550567db818c269122184be6fe158

cmake_minimum_required(VERSION 3.22.1)
project(smu2000_core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 88emu(gearmulator) 이식 때 배운 교훈: Debug 빌드 기본값(-O0)으로는 사이클
# 단위 CPU 인터프리터가 실기에서 버벅일 정도로 느려진다. 원본 Makefile도
# 항상 -O3를 쓰므로("音を作るのは重いので最適化を上げる"), 이 디렉터리
# 스코프 전체에 무조건 강제한다.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wno-inconsistent-missing-override")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -DNDEBUG")

add_library(smu2000_core STATIC
    compat/a64asm.cpp
    compat/compat.cpp
    mame/cpu/sh.cpp
    mame/cpu/sh2.cpp
    mame/cpu/sh2_jit.cpp
    mame/cpu/sh7042.cpp
    mame/cpu/sh_adc.cpp
    mame/cpu/sh_bsc.cpp
    mame/cpu/sh_cmt.cpp
    mame/cpu/sh_dmac.cpp
    mame/cpu/sh_intc.cpp
    mame/cpu/sh_mtu.cpp
    mame/cpu/sh_port.cpp
    mame/cpu/sh_sci.cpp
    mame/machine/sci4.cpp
    mame/sound/swp30.cpp
    mame/sound/swp30_jit.cpp
    mame/video/hd44780.cpp
    mu2000.cpp
    smartmedia.cpp
)

# 모든 로컬 include가 "mame/cpu/sh.h"처럼 src/ 루트 기준 상대경로인 것과,
# "emu.h"/"a64asm.h"처럼 compat/ 폴더 기준 파일명만 쓰는 것이 섞여 있다
# (원본 Makefile: CXXFLAGS += -I src -I src/compat). 그대로 재현한다.
target_include_directories(smu2000_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
)

find_package(Threads REQUIRED)
target_link_libraries(smu2000_core PUBLIC Threads::Threads)

set_property(TARGET smu2000_core PROPERTY POSITION_INDEPENDENT_CODE ON)
