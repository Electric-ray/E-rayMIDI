// android_compat.h
// NDK libc++ 에 없는 C++23 기능의 폴리필
// CMakeLists.txt 에서 -include 플래그로 강제 포함됩니다.
#pragma once

// std::unreachable() – C++23, NDK 28 libc++ 미지원
// Clang 내장 __builtin_unreachable() 으로 대체
#if __cplusplus >= 202002L
#include <utility>
namespace std {
    [[noreturn]] inline void unreachable() {
        __builtin_unreachable();
    }
}
#endif
