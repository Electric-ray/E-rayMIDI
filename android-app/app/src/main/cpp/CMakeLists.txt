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
    -flto=thin
    # Android compat: std::unreachable polyfill (C++23, not in NDK28 libc++)
    -include ${CMAKE_CURRENT_SOURCE_DIR}/android_compat.h
)
target_link_options(nuked-sc55-jni PRIVATE -flto=thin)

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
# FluidSynth (SF2 사운드폰트 재생) JNI 공유 라이브러리
# 원래 TinySoundFont(헤더 하나짜리 미니 신스)를 쓰다가, 정식 FluidSynth로
# 교체했다(더 정확한 SF2 해석/이펙트/보이스 관리). FluidSynth 공식 GitHub
# Release의 Android 프리빌트 바이너리를 그대로 가져다 쓴다 — 예전엔 glib를
# 직접 크로스컴파일해야 해서 매우 번거로웠지만, 2.5.0부터 glib 없이(C++11
# OSAL) 빌드된 공식 Android 배포본이 나와서 훨씬 간단해졌다.
# (다운로드: https://github.com/FluidSynth/fluidsynth/releases, asset:
#  fluidsynth-v2.6.0-android24.zip → app/src/main/cpp/fluidsynth/ 에 압축 해제)
# nuked-sc55-jni와 완전히 독립적인 두 번째 .so — 서로 다른 재생 엔진이므로
# 심볼 충돌 없이 나란히 빌드/링크된다.
# ---------------------------------------------------------------------------
set(FLUIDSYNTH_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/fluidsynth)
set(FLUIDSYNTH_LIB_DIR ${FLUIDSYNTH_ROOT}/lib/${ANDROID_ABI})

# FluidSynth 공식 문서(Using prebuilt libraries on Android)에 나온 패턴:
# libfluidsynth.so 자체와, 릴리즈 zip에 같이 들어있는 의존 라이브러리들을
# 전부 IMPORTED SHARED로 선언해야 런타임에 심볼 누락으로 죽지 않는다.
foreach(FS_LIB IN ITEMS
    fluidsynth fluidsynth-assetloader
    FLAC oboe ogg opus sndfile vorbis vorbisenc vorbisfile
)
    add_library(lib${FS_LIB} SHARED IMPORTED)
    set_target_properties(lib${FS_LIB} PROPERTIES
        IMPORTED_LOCATION ${FLUIDSYNTH_LIB_DIR}/lib${FS_LIB}.so
    )
endforeach()

add_library(
    soundfont-jni
    SHARED
    FluidBridge.cpp
)

target_include_directories(soundfont-jni PRIVATE
    ${FLUIDSYNTH_ROOT}/include
)

target_compile_options(soundfont-jni PRIVATE
    -O3
)

target_link_libraries(soundfont-jni
    libfluidsynth
    libfluidsynth-assetloader
    libFLAC
    liboboe
    libogg
    libopus
    libsndfile
    libvorbis
    libvorbisenc
    libvorbisfile
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

# BUGFIX(코드 최적화 리뷰로 발견): 다른 세 엔진(nuked-sc55-jni/soundfont-jni,
# 그리고 Cargo release 프로파일을 쓰는 madaha_core)은 전부 target_compile_options로
# -O3를 명시했는데 mt32emu/munt-jni만 빠져 있었다. build.gradle.kts에도
# CMAKE_BUILD_TYPE을 명시적으로 지정하지 않아서, 우리가 계속 테스트해온
# assembleDebug 빌드에서는 이 타겟만 AGP 기본값(Debug → 최적화 없는 -O0)으로
# 빌드되고 있었을 가능성이 높다 — 지금까지 MT-32 성능/게인 테스트가 전부
# 최적화 안 된 코드 기준이었다는 뜻이라 실제 배포 빌드에서는 여유가 훨씬
# 클 것으로 예상된다. 다른 엔진과 동일하게 명시적으로 강제한다.
target_compile_options(mt32emu PRIVATE -O3 -fomit-frame-pointer -flto=thin)

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

target_compile_options(munt-jni PRIVATE -O3 -fomit-frame-pointer -flto=thin)
target_compile_definitions(munt-jni PRIVATE ${MT32EMU_DEFINES})
target_link_options(munt-jni PRIVATE -flto=thin)

target_link_libraries(munt-jni
    mt32emu
    ${log-lib}
    ${aaudio-lib}
)

# ---------------------------------------------------------------------------
# Gearmulator 88emu — Roland Sound Canvas / MT-32 계열 통합 엔진 + JNI 공유 라이브러리
# 88emu_88emu_통합_개발계획서 Phase 4. dsp56300/gearmulator 저장소에서 88lib
# 구동에 필요한 최소 서브셋만 vendoring했다(gearmulator/source/ 아래, upstream과
# 동일한 상대 경로 구조 유지 — 나중에 다시 복사해오기 쉽게).
# nuked-sc55-jni / soundfont-jni / munt-jni / syxg50-jni와 완전히 독립적인
# 다섯 번째 .so — 심볼 충돌 없이 나란히 빌드/링크된다.
# ---------------------------------------------------------------------------
set(GEARMULATOR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/gearmulator)

add_subdirectory(${GEARMULATOR_ROOT}/source ${CMAKE_CURRENT_BINARY_DIR}/gearmulator-min)

add_library(
    gearmulator-jni
    SHARED
    GearmulatorBridge.cpp
    ${GEARMULATOR_ROOT}/GearmulatorEngine.cpp
    ${GEARMULATOR_ROOT}/Lcd88Renderer.cpp
)

target_include_directories(gearmulator-jni PRIVATE
    ${GEARMULATOR_ROOT}
    ${GEARMULATOR_ROOT}/source
)

target_compile_options(gearmulator-jni PRIVATE -O3 -fomit-frame-pointer -flto=thin)
target_link_options(gearmulator-jni PRIVATE -flto=thin)

target_link_libraries(gearmulator-jni
    88lib
    ${log-lib}
    ${aaudio-lib}
    ${jnigraphics-lib}
)

# ---------------------------------------------------------------------------
# S-MU2000 — YAMAHA MU2000(SH7042+SWP30x2) LLE 엔진 + JNI 공유 라이브러리
# tarboh/S-MU2000 저장소에서 mu2000 클래스 구동에 필요한 최소 서브셋만
# vendoring했다(smu2000/source/ 아래, upstream과 동일한 상대 경로 구조).
# 고정 커밋: d44b0891cb9550567db818c269122184be6fe158
# ---------------------------------------------------------------------------
set(SMU2000_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/smu2000)

add_subdirectory(${SMU2000_ROOT}/source ${CMAKE_CURRENT_BINARY_DIR}/smu2000-core)

add_library(
    mu2000-jni
    SHARED
    MU2000Bridge.cpp
    ${SMU2000_ROOT}/MU2000Engine.cpp
    ${SMU2000_ROOT}/Lcd2000Renderer.cpp
)

target_include_directories(mu2000-jni PRIVATE
    ${SMU2000_ROOT}
)

target_compile_options(mu2000-jni PRIVATE -O3 -fomit-frame-pointer)

target_link_libraries(mu2000-jni
    smu2000_core
    ${log-lib}
    ${aaudio-lib}
    ${jnigraphics-lib}
)

# ---------------------------------------------------------------------------
# vlcd-jni — 가상 LCD (v1.9+)
# LCD가 없는 엔진(SoundFont/FluidSynth, SC-8820)과 MT-32 문자 LCD(Munt)를 앱의 다른 LCD
# (88emu, S-MU2000)와 같은 모양으로 그린다. 두 렌더러를 그대로 재사용하는 순수 계산 라이브러리.
# ---------------------------------------------------------------------------
add_library(
    vlcd-jni
    SHARED
    vlcd/VirtualLcd.cpp
    ${SMU2000_ROOT}/Lcd2000Renderer.cpp
    ${GEARMULATOR_ROOT}/Lcd88Renderer.cpp
)

target_include_directories(vlcd-jni PRIVATE
    ${SMU2000_ROOT}
    ${GEARMULATOR_ROOT}
)

target_compile_options(vlcd-jni PRIVATE -O3)

target_link_libraries(vlcd-jni
    ${log-lib}
    ${jnigraphics-lib}
)
