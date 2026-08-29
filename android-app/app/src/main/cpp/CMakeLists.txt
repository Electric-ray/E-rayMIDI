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
# madaha(Rust) — S-YXG50 신시사이저 코어 + JNI 공유 라이브러리
# nuked-sc55-jni / soundfont-jni / munt-jni와 완전히 독립적인 네 번째 .so.
#
# madaha_core는 CMake가 아니라 cargo-ndk로 빌드되는 Rust staticlib이다.
# ANDROID_ABI(CMake가 AGP로부터 자동으로 받는 변수, 예: "arm64-v8a")를
# cargo-ndk가 그대로 이해하는 타겟 이름이라 별도 매핑이 필요 없다.
#
# 사전 준비 (로컬 1회):
#   rustup target add \
#       aarch64-linux-android armv7-linux-androideabi \
#       x86_64-linux-android i686-linux-android
#   cargo install cargo-ndk
#   (edition 2024는 rustc 1.85+ stable에 정식 지원 — nightly 불필요, 확인됨)
#   LLVM 설치(winget install LLVM.LLVM) — opl3-rs가 bindgen으로 libclang 필요
# ---------------------------------------------------------------------------
set(MADAHA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/madaha)
set(MADAHA_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/madaha-target)

# cargo-ndk가 만드는 스테이징 경로는 <target-dir>/<rust-triple>/release/*.a 이다.
# ANDROID_ABI → rust triple 매핑 (BYPRODUCTS/IMPORTED_LOCATION 지정용).
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(MADAHA_RUST_TRIPLE "aarch64-linux-android")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(MADAHA_RUST_TRIPLE "armv7-linux-androideabi")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(MADAHA_RUST_TRIPLE "x86_64-linux-android")
elseif(ANDROID_ABI STREQUAL "x86")
    set(MADAHA_RUST_TRIPLE "i686-linux-android")
else()
    message(FATAL_ERROR "madaha_core: unsupported ANDROID_ABI ${ANDROID_ABI}")
endif()

set(MADAHA_CORE_LIB ${MADAHA_TARGET_DIR}/${MADAHA_RUST_TRIPLE}/release/libmadaha_core.a)

# BYPRODUCTS가 핵심: add_custom_target 자체는 OUTPUT이 없는 phony 타겟이라,
# 이게 없으면 Ninja는 이 커맨드가 libmadaha_core.a를 만든다는 걸 전혀 몰라서
# "no known rule to make it"으로 그래프 구성 단계에서부터 실패한다.
# BYPRODUCTS로 알려주면 (1) ninja가 이 파일에 대한 생성 규칙으로 인식하고,
# (2) 그러면서도 ALL 타겟이라 매 빌드마다 커맨드 자체는 계속 실행됨
# (오래 안 걸림 — cargo 자체가 증분 빌드라 소스 변경 없으면 즉시 끝난다).
add_custom_target(
    build_madaha_core ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ANDROID_NDK_HOME=${CMAKE_ANDROID_NDK}
        "LIBCLANG_PATH=C:/Program Files/LLVM/bin"
        cargo ndk
            -t ${ANDROID_ABI}
            --platform 24
            build --target-dir ${MADAHA_TARGET_DIR} --release --no-default-features --lib
    BYPRODUCTS ${MADAHA_CORE_LIB}
    WORKING_DIRECTORY ${MADAHA_ROOT}
    COMMENT "cargo-ndk: madaha_core staticlib (${ANDROID_ABI})"
    VERBATIM
)

add_library(madaha_core STATIC IMPORTED)
set_target_properties(madaha_core PROPERTIES IMPORTED_LOCATION ${MADAHA_CORE_LIB})
add_dependencies(madaha_core build_madaha_core)

add_library(
    syxg50-jni
    SHARED
    SYXG50Bridge.cpp
)

# NOTE: IMPORTED 라이브러리에 건 순서 의존성만으로는 부족할 때가 있어,
# 실제 빌드 타겟인 syxg50-jni에도 직접 건다 (AGP가 명시적 타겟 목록으로
# ninja를 호출해도 build_madaha_core가 먼저 실행됨을 보장).
add_dependencies(syxg50-jni build_madaha_core)

# 실제 렌더링은 madaha_core(Rust, 이미 Cargo release 프로파일로 opt-level=3)가
# 하지만, 이 브리지 자체(링버퍼 push/pop 루프, AAudio 콜백)도 다른 세 엔진과
# 일관되게 최적화한다 (코드 최적화 리뷰로 확인 — 여기만 빠져 있었음).
target_compile_options(syxg50-jni PRIVATE -O3 -fomit-frame-pointer)

target_link_libraries(syxg50-jni
    madaha_core
    ${log-lib}
    ${aaudio-lib}
    # Rust staticlib이 참조하는 libc/libm/pthread/dl은 NDK의 libc/libm에
    # 이미 포함되어 있어 별도 링크가 필요 없다.
)
