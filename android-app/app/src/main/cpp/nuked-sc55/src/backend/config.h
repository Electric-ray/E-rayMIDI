// config.h  (Android NDK 빌드용 - CMake 자동 생성 대체)
// config.h.in 의 @VAR@ 치환값을 직접 채운 버전입니다.
#pragma once

// ASIO는 Windows 전용 – Android에서는 비활성화
#define NUKED_ENABLE_ASIO 0

#define NUKED_VERSION       "1.0.0-android"
#define NUKED_SOURCE        "jcmoyer/Nuked-SC55 (android port)"
#define NUKED_VERSION_MAJOR 1
#define NUKED_VERSION_MINOR 0
#define NUKED_VERSION_PATCH 0

#include <cstdio>
void Cfg_WriteVersionInfo(FILE* file);
