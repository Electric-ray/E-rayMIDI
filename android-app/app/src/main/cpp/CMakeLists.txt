cmake_minimum_required(VERSION 3.22.1)
project("nuked-sc55-android" CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------------------------
# jcmoyer/Nuked-SC55 backend source files
# We include only the backend/ directory (no SDL, no rtmidi, no renderer).
# ---------------------------------------------------------------------------
set(NUKED_BACKEND ${CMAKE_CURRENT_SOURCE_DIR}/nuked-sc55/src/backend)

set(NUKED_BACKEND_SRCS
    ${NUKED_BACKEND}/config.cpp
    ${NUKED_BACKEND}/diagnostics.cpp
    ${NUKED_BACKEND}/emu.cpp
    ${NUKED_BACKEND}/file_hashing.cpp
    ${NUKED_BACKEND}/file_io.cpp
    ${NUKED_BACKEND}/lcd.cpp
    ${NUKED_BACKEND}/mcu.cpp
    ${NUKED_BACKEND}/mcu_interrupt.cpp
    ${NUKED_BACKEND}/mcu_opcodes.cpp
    ${NUKED_BACKEND}/mcu_timer.cpp
    ${NUKED_BACKEND}/pcm.cpp
    ${NUKED_BACKEND}/rom.cpp
    ${NUKED_BACKEND}/rom_io.cpp
    ${NUKED_BACKEND}/standard_romsets.cpp
    ${NUKED_BACKEND}/submcu.cpp
    # SHA-256 (C file, compiled as C)
    ${NUKED_BACKEND}/sha/sha224-256.c
)

# ---------------------------------------------------------------------------
# SC55Bridge JNI shared library
# ---------------------------------------------------------------------------
add_library(
    nuked-sc55-jni
    SHARED
    SC55Bridge.cpp
    ${NUKED_BACKEND_SRCS}
)

target_include_directories(nuked-sc55-jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/nuked-sc55/src
    ${CMAKE_CURRENT_SOURCE_DIR}/nuked-sc55/src/backend
)

# Aggressive optimization for MCU emulation performance on ARM (LG Velvet)
target_compile_options(nuked-sc55-jni PRIVATE
    -O3
    -ffast-math
    -fomit-frame-pointer
    # Android compat: std::unreachable polyfill (C++23, not in NDK28 libc++)
    -include ${CMAKE_CURRENT_SOURCE_DIR}/android_compat.h
)

# Android-specific defines to suppress platform-specific code in the backend
target_compile_definitions(nuked-sc55-jni PRIVATE
    ANDROID_BUILD=1
    # Disable file_io platform detection (we handle ROM loading ourselves via std::filesystem)
    _POSIX_C_SOURCE=200809L
)

# std::filesystem requires libc++fs on older NDK, but NDK r22+ bundles it in libc++
# With NDK r25 + c++_shared, std::filesystem is available directly.
set_target_properties(nuked-sc55-jni PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# ---------------------------------------------------------------------------
# Android system libraries
# ---------------------------------------------------------------------------
find_library(log-lib     log)
find_library(aaudio-lib  aaudio)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)

target_link_libraries(nuked-sc55-jni
    ${log-lib}
    ${aaudio-lib}
    ${android-lib}
    ${jnigraphics-lib}
)

# ---------------------------------------------------------------------------
# TinySoundFont (SF2 사운드폰트 재생) JNI 공유 라이브러리
# nuked-sc55-jni와 완전히 독립적인 두 번째 .so — 서로 다른 재생 엔진이므로
# 심볼 충돌 없이 나란히 빌드/링크된다.
# ---------------------------------------------------------------------------
add_library(
    soundfont-jni
    SHARED
    TsfBridge.cpp
)

target_include_directories(soundfont-jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tsf
)

target_compile_options(soundfont-jni PRIVATE
    -O3
    -ffast-math
)

target_link_libraries(soundfont-jni
    ${log-lib}
    ${aaudio-lib}
)

# ---------------------------------------------------------------------------
# munt(mt32emu) — MT-32/CM-32L 신시사이저 코어 + JNI 공유 라이브러리
# munt-android에서 이식(통합작업순서.md Phase 2). nuked-sc55-jni /
# soundfont-jni와 완전히 독립적인 세 번째 .so — 심볼 충돌 없이 나란히
# 빌드/링크된다. 소스 목록은 munt-android 원본 CMakeLists.txt와 동일
# (SamplerateAdapter/SoxrAdapter는 외부 라이브러리(soxr/libsamplerate)가
# 필요해 원본에서도 빌드 대상에서 제외되어 있었음 — 내부 리샘플러만 사용).
# ---------------------------------------------------------------------------
set(MT32EMU_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/munt/mt32emu)

set(MT32EMU_SRCS
    ${MT32EMU_ROOT}/Analog.cpp
    ${MT32EMU_ROOT}/BReverbModel.cpp
    ${MT32EMU_ROOT}/Display.cpp
    ${MT32EMU_ROOT}/File.cpp
    ${MT32EMU_ROOT}/FileStream.cpp
    ${MT32EMU_ROOT}/LA32FloatWaveGenerator.cpp
    ${MT32EMU_ROOT}/LA32Ramp.cpp
    ${MT32EMU_ROOT}/LA32WaveGenerator.cpp
    ${MT32EMU_ROOT}/MidiStreamParser.cpp
    ${MT32EMU_ROOT}/Part.cpp
    ${MT32EMU_ROOT}/Partial.cpp
    ${MT32EMU_ROOT}/PartialManager.cpp
    ${MT32EMU_ROOT}/Poly.cpp
    ${MT32EMU_ROOT}/ROMInfo.cpp
    ${MT32EMU_ROOT}/SampleRateConverter.cpp
    ${MT32EMU_ROOT}/Synth.cpp
    ${MT32EMU_ROOT}/Tables.cpp
    ${MT32EMU_ROOT}/TVA.cpp
    ${MT32EMU_ROOT}/TVF.cpp
    ${MT32EMU_ROOT}/TVP.cpp
    ${MT32EMU_ROOT}/VersionTagging.cpp
    ${MT32EMU_ROOT}/sha1/sha1.cpp
    ${MT32EMU_ROOT}/srchelper/InternalResampler.cpp
    ${MT32EMU_ROOT}/srchelper/srctools/src/FIRResampler.cpp
    ${MT32EMU_ROOT}/srchelper/srctools/src/IIR2xResampler.cpp
    ${MT32EMU_ROOT}/srchelper/srctools/src/LinearResampler.cpp
    ${MT32EMU_ROOT}/srchelper/srctools/src/ResamplerModel.cpp
    ${MT32EMU_ROOT}/srchelper/srctools/src/SincResampler.cpp
)

set(MT32EMU_DEFINES
    MT32EMU_EXPORTS_TYPE=0
    MT32EMU_WITH_INTERNAL_RESAMPLER=1
)

add_library(mt32emu STATIC ${MT32EMU_SRCS})

target_include_directories(mt32emu PUBLIC
    ${MT32EMU_ROOT}
    ${MT32EMU_ROOT}/sha1
    ${MT32EMU_ROOT}/srchelper
    ${MT32EMU_ROOT}/srchelper/srctools/include
)

target_compile_definitions(mt32emu PRIVATE ${MT32EMU_DEFINES})

add_library(
    munt-jni
    SHARED
    MuntBridge.cpp
)

target_compile_definitions(munt-jni PRIVATE ${MT32EMU_DEFINES})

target_link_libraries(munt-jni
    mt32emu
    ${log-lib}
    ${aaudio-lib}
)
