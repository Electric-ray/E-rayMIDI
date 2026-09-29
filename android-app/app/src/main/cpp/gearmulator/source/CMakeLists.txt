# gearmulator/source/CMakeLists.txt
#
# dsp56300/gearmulator 저장소에서 88lib(88emu 엔진 코어)를 구동하는 데
# 필요한 최소 서브셋만 뽑아 독립적으로 빌드하기 위한 CMakeLists.
#
# 원본 저장소의 source/CMakeLists.txt + source/ronaldo/CMakeLists.txt에서
# 88lib 경로만 따라가며 필요한 add_subdirectory 호출들을 그대로 가져왔다.
# JUCE 플러그인/GUI, 다른 신스(Osirus/Vavra/...), freetype/lunasvg/RmlUi,
# 네트워크 브리지(MCP 서버) 등은 전부 제외했다 — 이 프로젝트(E-rayMIDI)는
# JNI 라이브러리 하나(88lib)만 필요하기 때문이다.
#
# upstream과 최대한 같은 상대 경로 구조(source/ronaldo/..., source/cpu/...,
# source/framework/...)를 유지해서, 나중에 upstream이 갱신되었을 때 이
# 서브셋만 다시 복사해오기 쉽게 했다.

cmake_minimum_required(VERSION 3.22.1)
project(gearmulator_min VERSION 2.2.26 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# [E-rayMIDI 벤더 패치] nuked-sc55-jni는 MCU 에뮬레이션 소스를 같은 타겟에
# 넣고 target_compile_options로 -O3를 강제하지만, 88lib는 여러 개의 별도
# CMake 타겟(h8500/sh2/mcs96/custom_chips/hardwareLib/...)으로 나뉘어 있어
# 그 방식을 그대로 쓸 수 없다. 대신 이 디렉터리 스코프의 CMAKE_*_FLAGS에
# -O3를 강제로 얹어서, 아래에서 add_subdirectory되는 모든 타겟(=88lib가
# 필요로 하는 CPU 에뮬레이션/JIT 코드 전부)이 Debug 빌드(AGP가 기본으로 주는
# -O0)에서도 항상 최적화되게 한다. -O3는 커맨드라인에서 뒤에 오는 플래그가
# 우선하므로 Debug 기본 플래그(-O0 -g)를 덮어쓴다. 이게 없으면 사이클
# 단위로 명령어를 해석하는 H8500/SH2/MCS96 인터프리터가 Debug 빌드에서
# 수십 배 느려져 실기에서 심한 버벅임/언더런으로 나타난다.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -DNDEBUG")

# ----------------- asmjit (커스텀 칩 JIT 백엔드: xp/lsp/mt32reverb가 사용) -----
set(ASMJIT_STATIC TRUE)
set(ASMJIT_NO_INSTALL TRUE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(cpu/dsp56300/source/asmjit)

# ----------------- dsp56kBase (synthLib가 logging.cpp만 사용) -----------------
add_subdirectory(cpu/dsp56300/source/vtuneSdk)
add_subdirectory(cpu/dsp56300/source/dsp56kBase)
# dsp56kBase/CMakeLists.txt는 BUILD_TESTING 가드 없이 테스트 실행파일 5개를
# 무조건 만든다. 그중 일부가 Android NDK에 없는 -lrt를 링크해서 "ninja all"
# 단계(AGP의 externalNativeBuild가 기본으로 돌리는 타깃)에서 링크 실패가 난다.
# 우리는 dsp56kBase "라이브러리"만 필요하므로 이 테스트 타깃들을 빌드 대상에서
# 제외한다 (업스트림 CMakeLists.txt 자체는 건드리지 않는다).
foreach(_dsp56kBaseTest IN ITEMS sharedAudioBufferTest sharedAudioReducerTest ringBufferTest semaphoreTest profilerTest)
	if(TARGET ${_dsp56kBaseTest})
		set_target_properties(${_dsp56kBaseTest} PROPERTIES EXCLUDE_FROM_ALL TRUE)
	endif()
endforeach()

# ----------------- 공통 베이스 라이브러리 --------------------------------------
add_subdirectory(framework/baseLib)
add_subdirectory(framework/synthLib)
add_subdirectory(3rdparty/libresample)

# ----------------- CPU 코어 (88emu가 필요로 하는 것만) --------------------------
add_subdirectory(cpu/common)   # emucommon (INTERFACE)
add_subdirectory(cpu/h8500)    # SC-55/SC-88 계열 MCU
add_subdirectory(cpu/sh2)      # SC-8850 등 상위 기종 MCU
add_subdirectory(cpu/mcs96)    # MT-32/CM 계열 MCU
add_subdirectory(cpu/mc68k)    # 68kEmu — hardwareLib가 사용
# upstream source/CMakeLists.txt와 동일: 68kEmu 자신의 CMakeLists.txt는 include
# 경로를 스스로 노출하지 않으므로("#include "mc68k/logging.h"" 형태로 참조됨),
# 상위에서 cpu/ 디렉터리 자체를 PUBLIC include로 추가해줘야 한다.
target_include_directories(68kEmu PUBLIC ${CMAKE_CURRENT_LIST_DIR}/cpu)

add_subdirectory(framework/hardwareLib)

# ----------------- custom-chip JIT 백엔드 감지 (source/ronaldo/CMakeLists.txt 원본과 동일) ---
option(CHIPS_FORCE_NO_JIT "Run the custom-chip DSPs on their interpreters even where a JIT back end exists" OFF)
if(CHIPS_FORCE_NO_JIT)
	add_compile_definitions(CHIPS_FORCE_NO_JIT=1)
	set(CHIPS_JIT_ASMJIT FALSE)
else()
	include(CheckCXXSourceCompiles)
	set(CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/ronaldo/custom_chips)
	check_cxx_source_compiles("
		#include \"jitHost.h\"
		#if !CHIPS_JIT_ASMJIT
		#error no asmjit back end on this host
		#endif
		int main() { return 0; }" CHIPS_JIT_ASMJIT)
	unset(CMAKE_REQUIRED_INCLUDES)
endif()
message(STATUS "[gearmulator-min] Custom-chip JIT through asmjit: ${CHIPS_JIT_ASMJIT}")

# ----------------- 커스텀 칩(음원 보드 구성 요소) -------------------------------
add_subdirectory(ronaldo/custom_chips/lsp)
add_subdirectory(ronaldo/custom_chips/xp)
add_subdirectory(ronaldo/custom_chips/gp)
add_subdirectory(ronaldo/custom_chips/lp)
add_subdirectory(ronaldo/custom_chips/rcc)
add_subdirectory(ronaldo/custom_chips/la32)
add_subdirectory(ronaldo/custom_chips/mt32reverb)

# ----------------- 88lib 본체 --------------------------------------------------
set(CMAKE_PROJECT_VERSION "erayMIDI-vendor")
add_subdirectory(ronaldo/88emu/88lib)
