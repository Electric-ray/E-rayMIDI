// license:BSD-3-Clause
//
// リバーブ・コーラス・バリエーション（システムエフェクト）のパラメータの番地（issue #35）。
//
// 種類ごとのパラメータの並び・名前・範囲・表示は、インサーションと同じ表（xg/fx_params.h）を使う。
// 同じ種類なら、LCD の編集画面に出るものは同じ。番地だけがシステムエフェクトの形に替わる:
//   リバーブ       パラメータ 1-10 は 02 01 02-0B、11-16 は 02 01 10-15（どれも 1 バイト）
//   コーラス       パラメータ 1-10 は 02 01 22-2B。11-16（02 01 30-35）は firmware が書いても
//                  受け付けない（RAM に場所が無い）ので扱わない
//   バリエーション パラメータ 1-10 は 02 01 42-55（どれも 2 バイト）、11-16 は 02 01 70-75
// 範囲はシステムエフェクトの番地に書いて読み返して確かめた（tools/fxsweep/sysfx_check.cpp）。
// リバーブ 196・コーラス 154・バリエーション 1256 のパラメータで、書いた下限と上限がそのまま返った。
// 返らなかったのはリバーブとコーラスのパラメータ 10（Dry/Wet）だけで、書いても 0 のまま。
// システムエフェクトは戻り量で混ぜるので Dry/Wet が無い。だから扱わない

#ifndef S_MU2000_XG_SYSFX_H
#define S_MU2000_XG_SYSFX_H

#pragma once

#include "xg/fx_params.h"

namespace xg {

enum class sysfx : u8 { reverb, chorus, variation };

// 種類の番地（02 01 の後ろ）
constexpr u8 sysfx_type_lo(sysfx which)
{
	return which == sysfx::reverb ? 0x00 : which == sysfx::chorus ? 0x20 : 0x40;
}

// インサーションの表の 1 行を、システムエフェクトの番地（02 01 の後ろ）へ読み替える。
// size にバイト数。その塊に無いパラメータなら -1
inline int sysfx_addr(sysfx which, const fx_param &p, int &size)
{
	// インサーションの番地から「パラメータの番号」（1-16）
	int n = 0;
	if (p.addr >= 0x02 && p.addr <= 0x0b)      n = p.addr - 0x02 + 1;
	else if (p.addr >= 0x30 && p.addr <= 0x43) n = (p.addr - 0x30) / 2 + 1;
	else if (p.addr >= 0x20 && p.addr <= 0x25) n = p.addr - 0x20 + 11;
	else return -1;

	switch (which) {
	case sysfx::reverb:
		if (p.size != 1 || n == 10)             // 10 は Dry/Wet。システムエフェクトには無い（書いても 0 のまま）
			return -1;
		size = 1;
		return n <= 10 ? 0x02 + (n - 1) : 0x10 + (n - 11);
	case sysfx::chorus:
		if (p.size != 1 || n >= 10)             // 10 は Dry/Wet（無い）、11-16 は受け付けない
			return -1;
		size = 1;
		return 0x22 + (n - 1);
	case sysfx::variation:
		if (n <= 10) {
			size = 2;
			return 0x42 + 2 * (n - 1);
		}
		size = 1;
		return 0x70 + (n - 11);
	}
	return -1;
}

} // namespace xg

#endif // S_MU2000_XG_SYSFX_H
