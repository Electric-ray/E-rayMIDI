// license:BSD-3-Clause
//
// XG のエフェクトの種別の名前（MSB × 128 + LSB）。エフェクトの面と一覧の窓が使う。
//
// **種類の一覧は、利用者のプログラム ROM から実行時に読む**（set_fx_type_rom）。
// firmware がパネルの LCD で種類を選ばせるときの表で、リバーブ 19・コーラス 21・
// バリエーション（とインサーション）118 個。番号と LCD の名前（10 文字）が並んでいる。
// ROM を読めなければ、下の少ない表（XG の基本の種類）で出す。
//
// firmware が受け付けるかは、エミュレータで表の全部を書いて RAM に残るかで確かめた。
// バリエーションとインサーション 1-4 は表の全部を受け付ける。

#ifndef S_MU2000_XG_FX_TYPES_H
#define S_MU2000_XG_FX_TYPES_H

#pragma once

#include "compat/mamecompat.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace xg {

struct fx_type { const char *name; u8 msb, lsb; };

// ROM が無いときの表
inline constexpr fx_type REV_BASIC[] = {
	{ "NO EFFECT", 0x00, 0x00 }, { "HALL 1", 0x01, 0x00 }, { "HALL 2", 0x01, 0x01 },
	{ "ROOM 1", 0x02, 0x00 },    { "ROOM 2", 0x02, 0x01 }, { "ROOM 3", 0x02, 0x02 },
	{ "STAGE 1", 0x03, 0x00 },   { "STAGE 2", 0x03, 0x01 }, { "PLATE", 0x04, 0x00 },
	{ "WHITE ROOM", 0x10, 0x00 },{ "TUNNEL", 0x11, 0x00 },  { "BASEMENT", 0x13, 0x00 },
};
inline constexpr fx_type CHO_BASIC[] = {
	{ "NO EFFECT", 0x00, 0x00 }, { "CHORUS 1", 0x41, 0x00 }, { "CHORUS 2", 0x41, 0x01 },
	{ "CHORUS 3", 0x41, 0x02 },  { "CELESTE 1", 0x42, 0x00 },{ "CELESTE 2", 0x42, 0x01 },
	{ "CELESTE 3", 0x42, 0x02 }, { "FLANGER 1", 0x43, 0x00 },{ "FLANGER 2", 0x43, 0x01 },
	{ "FLANGER 3", 0x43, 0x02 },
};
// GATE REVERB と REVERSE GATE の番号は firmware の LCD の種類の表示で確かめた
inline constexpr fx_type INS_BASIC[] = {
	{ "NO EFFECT", 0x00, 0x00 },   { "HALL 1", 0x01, 0x00 },     { "ROOM 1", 0x02, 0x00 },
	{ "STAGE 1", 0x03, 0x00 },     { "PLATE", 0x04, 0x00 },      { "DELAY LCR", 0x05, 0x00 },
	{ "DELAY L,R", 0x06, 0x00 },   { "ECHO", 0x07, 0x00 },       { "CROSS DELAY", 0x08, 0x00 },
	{ "ER 1", 0x09, 0x00 },        { "GATE REVERB", 0x0a, 0x00 },{ "REVERSE GATE", 0x0b, 0x00 },
	{ "THRU", 0x40, 0x00 },        { "CHORUS 1", 0x41, 0x00 },   { "CELESTE 1", 0x42, 0x00 },
	{ "FLANGER 1", 0x43, 0x00 },   { "SYMPHONIC", 0x44, 0x00 },  { "ROTARY SP", 0x45, 0x00 },
	{ "TREMOLO", 0x46, 0x00 },     { "AUTO PAN", 0x47, 0x00 },   { "PHASER 1", 0x48, 0x00 },
	{ "DISTORTION", 0x49, 0x00 },  { "OVERDRIVE", 0x4a, 0x00 },  { "AMP SIM", 0x4b, 0x00 },
	{ "3BAND EQ", 0x4c, 0x00 },    { "2BAND EQ", 0x4d, 0x00 },   { "AUTO WAH", 0x4e, 0x00 },
};

namespace detail {

struct fx_tables {
	std::vector<fx_type> rev, cho, ins;
	std::deque<std::string> names;          // fx_type::name の持ち主
	bool from_rom = false;
};

inline fx_tables &tables()
{
	static fx_tables t = [] {
		fx_tables b;
		b.rev.assign(std::begin(REV_BASIC), std::end(REV_BASIC));
		b.cho.assign(std::begin(CHO_BASIC), std::end(CHO_BASIC));
		b.ins.assign(std::begin(INS_BASIC), std::end(INS_BASIC));
		return b;
	}();
	return t;
}

} // namespace detail

// プログラム ROM から種類の表を読む。番地は MU2000 EX（firmware v2.01）で調べたもの。
// 版が違って形が合わなければ何もしない（false）
inline bool set_fx_type_rom(const std::vector<u8> &rom)
{
	constexpr u32 REV_CODES = 0x2aea33, CHO_CODES = 0x2aea59, VAR_CODES = 0x2aea83;   // (MSB, LSB) の並び
	constexpr u32 NAMES = 0x2aeb6f;                                                  // 10 文字ずつ、同じ順
	constexpr int NREV = 19, NCHO = 21, NVAR = 118;
	if (rom.size() < NAMES + 10 * (NREV + NCHO + NVAR))
		return false;
	auto name_at = [&](int i) { return std::string(reinterpret_cast<const char *>(rom.data() + NAMES + 10 * i), 10); };
	// 形の確かめ: 3 つの表とも NO EFFECT（00 00）で始まり、バリエーションの最後は THRU（40 00）
	if (name_at(0).rfind("NO EFFECT", 0) || name_at(NREV).rfind("NO EFFECT", 0) || name_at(NREV + NCHO).rfind("NO EFFECT", 0) ||
	    name_at(NREV + NCHO + NVAR - 1).rfind("THRU", 0) || rom[REV_CODES] || rom[REV_CODES + 1] ||
	    rom[VAR_CODES + 2 * (NVAR - 1)] != 0x40)
		return false;
	detail::fx_tables &t = detail::tables();
	t.rev.clear(); t.cho.clear(); t.ins.clear(); t.names.clear();
	int k = 0;
	auto load = [&](std::vector<fx_type> &out, u32 codes, int n) {
		for (int i = 0; i < n; i++, k++) {
			std::string s = name_at(k);
			while (!s.empty() && (s.back() == ' ' || s.back() == 0))
				s.pop_back();
			t.names.push_back(s);
			out.push_back({ t.names.back().c_str(), rom[codes + 2 * i], rom[codes + 2 * i + 1] });
		}
	};
	load(t.rev, REV_CODES, NREV);
	load(t.cho, CHO_CODES, NCHO);
	load(t.ins, VAR_CODES, NVAR);
	t.from_rom = true;
	return true;
}

// リバーブ・コーラス・バリエーション（インサーションも同じ）に置ける種類
inline const std::vector<fx_type> &rev_types() { return detail::tables().rev; }
inline const std::vector<fx_type> &cho_types() { return detail::tables().cho; }
inline const std::vector<fx_type> &ins_types() { return detail::tables().ins; }

// 種別の値から名前。表に無ければ「TYPE 49-01」のように番号で
inline std::string fx_name(int value)
{
	const int msb = value >> 7, lsb = value & 0x7f;
	for (const auto *list : { &ins_types(), &rev_types(), &cho_types() })
		for (const fx_type &t : *list)
			if (t.msb == msb && t.lsb == lsb)
				return t.name;
	char buf[16];
	std::snprintf(buf, sizeof(buf), "TYPE %02X-%02X", msb, lsb);
	return buf;
}

// 種類を選ぶ品書きの分け方（画面の都合で決めたもの）。MSB の系統ごと
struct fx_category { const char *name; std::vector<u8> msbs; };
inline const std::vector<fx_category> &fx_categories()
{
	static const std::vector<fx_category> c = {
		{ "リバーブ",                 { 0x01, 0x02, 0x03, 0x04, 0x10, 0x11, 0x12, 0x13 } },
		{ "初期反射・ゲート",         { 0x09, 0x0a, 0x0b } },
		{ "ディレイ・エコー",         { 0x05, 0x06, 0x07, 0x08, 0x15, 0x16 } },
		{ "カラオケ",                 { 0x14 } },
		{ "コーラス・セレステ",       { 0x41, 0x42, 0x57 } },
		{ "フランジャー・フェイザー", { 0x43, 0x44, 0x48, 0x68, 0x6b, 0x6c, 0x6e, 0x6f } },
		{ "回転・トレモロ・パン",     { 0x45, 0x46, 0x47, 0x56, 0x63 } },
		{ "歪み・アンプ",             { 0x49, 0x4a, 0x4b, 0x62 } },
		{ "EQ・ワウ・フィルタ",       { 0x4c, 0x4d, 0x4e, 0x52, 0x6d } },
		{ "コンプ・ゲート",           { 0x53, 0x54, 0x69 } },
		{ "組み合わせ",               { 0x5f, 0x60, 0x61 } },
		{ "ローファイ・テクノ",       { 0x5e, 0x74, 0x75, 0x76, 0x73, 0x72 } },
		{ "ピッチ・その他",           { 0x50, 0x51, 0x55, 0x58, 0x5d, 0x70, 0x71 } },
	};
	return c;
}

} // namespace xg

#endif // S_MU2000_XG_FX_TYPES_H
