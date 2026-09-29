cmake_minimum_required(VERSION 3.10)
project(dsp56kBase)

add_library(dsp56kBase)

set(SOURCES
bitfield.h
buildconfig.h
conditionvariable.cpp conditionvariable.h
dspassert.cpp dspassert.h
fastmath.h
logging.cpp logging.h
mmuarray.h
mmuhelper.cpp mmuhelper.h
mutex.cpp mutex.h
profiler.cpp profiler.h
ringbuffer.h
semaphore.h
sharedaudiobuffer.h
sharedaudioreducer.h
threadtools.cpp threadtools.h
trigger.cpp trigger.h
win8sync.cpp win8sync.h
)

target_sources(dsp56kBase PRIVATE ${SOURCES})
source_group("source" FILES ${SOURCES})

# Profiler integrations, see profiler.cpp. vtuneSdk is only added for these platforms, Superluminal is Windows only.
if(WIN32 OR (UNIX AND NOT APPLE))
	target_link_libraries(dsp56kBase PRIVATE vtuneSdk)
	target_compile_definitions(dsp56kBase PRIVATE DSP56K_USE_VTUNE_JIT_PROFILING_API)
endif()

if(WIN32)
	target_compile_definitions(dsp56kBase PRIVATE DSP56K_USE_SUPERLUMINAL_PROFILING_API)
endif()

if(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /constexpr:steps10000000")
elseif(UNIX AND NOT APPLE)
	if(NOT ANDROID)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-loop-limit=10000000")
	endif()
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	# [E-rayMIDI 벤더 패치] librt는 Android NDK(bionic libc)에 없다 —
	# 원래 코드가 요구하는 기능(POSIX 실시간 확장, 예: clock_gettime 등)은
	# bionic에서 이미 libc에 통합되어 있어 별도 링크가 필요 없다.
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(dsp56kBase PUBLIC Threads::Threads rt)
elseif(ANDROID)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(dsp56kBase PUBLIC Threads::Threads)
endif()

target_include_directories(dsp56kBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# [E-rayMIDI 벤더 패치] 아래 테스트 실행파일들은 원래 가드 없이 무조건
# 빌드되는데, 일부가 Android NDK에 없는 -lrt를 링크해서 실패한다. 게다가
# Android Gradle Plugin은 EXCLUDE_FROM_ALL 여부와 무관하게 CMake File API로
# 찾은 타겟을 전부 강제로 빌드하므로, 아예 BUILD_TESTING일 때만 타겟 자체가
# 생성되도록 감쌌다 (BUILD_TESTING은 이 프로젝트에서 켠 적이 없어 기본 OFF).
if(BUILD_TESTING)

add_executable(sharedAudioBufferTest sharedaudiobuffer_test.cpp)
target_link_libraries(sharedAudioBufferTest PRIVATE dsp56kBase)
add_test(NAME sharedAudioBufferTest COMMAND sharedAudioBufferTest)

add_executable(sharedAudioReducerTest sharedaudioreducer_test.cpp)
target_link_libraries(sharedAudioReducerTest PRIVATE dsp56kBase)
add_test(NAME sharedAudioReducerTest COMMAND sharedAudioReducerTest)

add_executable(ringBufferTest ringbuffer_test.cpp)
target_link_libraries(ringBufferTest PRIVATE dsp56kBase)
add_test(NAME ringBufferTest COMMAND ringBufferTest)

add_executable(semaphoreTest semaphore_test.cpp)
target_link_libraries(semaphoreTest PRIVATE dsp56kBase)
add_test(NAME semaphoreTest COMMAND semaphoreTest)

add_executable(profilerTest profiler_test.cpp)
target_link_libraries(profilerTest PRIVATE dsp56kBase)
add_test(NAME profilerTest COMMAND profilerTest)

endif()
