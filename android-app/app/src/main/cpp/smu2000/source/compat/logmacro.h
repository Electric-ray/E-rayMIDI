// license:BSD-3-Clause
//
// MAME の src/emu/logmacro.h の代わり。
// 取り込んだソースは VERBOSE を定義してからこれを読み、LOGMASKED / LOG で書く。
//
// MAME 上流は sh_sci.cpp で VERBOSE=(LOG_DATA|LOG_RATE) を有効にしている。
// そのままだと MIDI のバイトが全部流れて音源として使い物にならないので、
// コンパイル時のマスクに加えて実行時の smu2000::g_verbose でも止める。
// つまり既定では黙り、デバッグ時だけ出る。

#include "mamecompat.h"

#ifndef VERBOSE
#define VERBOSE 0
#endif

#ifndef LOG_GENERAL
#define LOG_GENERAL (1U << 0)
#endif

#ifndef LOG_OUTPUT_FUNC
#define LOG_OUTPUT_FUNC [] (auto &&...a) { ::smu2000::log_fmt(std::forward<decltype(a)>(a)...); }
#endif

#define LOGMASKED(mask, ...) 	do { if (VERBOSE & (mask)) (LOG_OUTPUT_FUNC)(__VA_ARGS__); } while (false)

#define LOG(...) LOGMASKED(LOG_GENERAL, __VA_ARGS__)
