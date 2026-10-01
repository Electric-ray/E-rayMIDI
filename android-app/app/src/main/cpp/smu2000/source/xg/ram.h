// license:BSD-3-Clause
//
// XG の値が firmware のワーク RAM（0x400000-0x43ffff）のどこにあるか。
//
// 画面は MU2000 に問い合わせずに、ここを読んで値を出す（doc/params.md の「RAM から読む」）。
// 問い合わせ（ダンプ要求）は MIDI IN に入るので、LCD の受信マークが点きっぱなしになる。
// RAM なら何も流さずに、曲・SysEx・パネル操作のどれで変わった値も見える。
//
// 番地は、1 項目ずつパラメータチェンジで書いて RAM の変わった所を探して決めた。
// 塊の中の並びは XG の番地そのまま。xgtest.exe が全項目を一括ダンプと突き合わせる。

#ifndef S_MU2000_XG_RAM_H
#define S_MU2000_XG_RAM_H

#pragma once

#include "xg/model.h"

namespace xg {
namespace ram {

// ワーク RAM の先頭（0x400000）からの位置
constexpr u32 SYSTEM   = 0x226c1;   // 00 00 00-06
// 00 00 00-03（マスターチューン）。4 バイトの下 4bit をつないだ 12bit の値で、
// 0x400 が 0 セント、1 きざみ 0.1 セント（doc/native-engine.md の 6.136）
// **10ms 割り込みの印**（doc/native-engine.md の 6.145）。firmware は
// 10ms ごとにここを 0/1 で裏返す（pc=0x12989A）。音を鳴らしていなくても
// 動くので、写し取りが無いときでも 10ms 格子の位相をここから学べる。
// 隣の +0x3E941 は 10ms ごとに 1 増える数、0x408008 は 16bit の数
constexpr u32 TICK_MARK     = 0x3e948;
// **パンの Rnd（乱数）の種**（doc/native-engine.md の 6.147）。
// パートのパンが 0（画面では Rnd）のとき、実機は要素を 1 つ鳴らすたびに
//   x = (0xB3 * x + 0x11) & 0xFF
// と進めて、`x >> 1` をパンの位置（0-127）に使う。native もここを読んで
// 進めて書き戻すので、実機モードと行き来しても列が途切れない
constexpr u32 PAN_RND       = 0x3e94c;
// **液晶のメーターの元**（doc/native-engine.md の 6.148）。パート 1-16 の
// 「今いちばん大きい音の目盛り」が 1 バイトずつ並ぶ。firmware は 25ms ごとに
// ここを 0x43E240 へ写し、さらになまして 0x43E282 に置き、液晶へ描く。
//
// **ここは書いてはいけない。** firmware の音の管理が使っている場所で、
// 外から書くと演奏が壊れる（試験が 5 件崩れた）。native の口では
// firmware が演奏画面を描き直さないので、書いても誰も読まない。
// メーターは native が液晶へ直に描く（mu2000::draw_meter）
constexpr u32 METER_SRC     = 0x2dd8;
constexpr u32 SYS_TUNE      = SYSTEM + 0;
constexpr u32 SYS_VOLUME    = SYSTEM + 4;   // 00 00 04（マスター音量）
// **パートの音量の目盛り**（0-128）。実機はここを音量の目盛りに掛ける
// （`0x12A4AA`）。音量・エクスプレッション・マスター音量だけでなく、
// **インサーションを通すと下がる**（LO-FI を掛けたパートで 101 -> 80）。
// だから式で作らず、実機が持っている値を読む（doc/native-engine.md の 6.114）
constexpr u32 PART_GAIN = 0x12f;
constexpr u32 SYS_TRANSPOSE = SYSTEM + 6;   // 00 00 06（64 が 0 半音）
// **ドラムセットアップ**（XG の `3n rr nn`）。SysEx を書いて、書かれた番地を
// 見て並びを割り出した（`3n` が組 0-3、`rr` が鍵 13-91、`nn` がパラメータ 0-22）:
//   0x30 24 00 -> 4228F2   0x30 24 02 -> 4228F4   0x30 25 02 -> 42290B（+23）
//   0x30 26 02 -> 422922   0x31 24 02 -> 42300D（+1817 = 23*79）
//   0x30 0D 02 -> 4226E3
constexpr u32 DRUM_SETUP       = 0x226e1;   // 組 0・鍵 13・パラメータ 0
constexpr u32 DRUM_SETUP_PARAM = 23;
constexpr u32 DRUM_SETUP_NOTES = 79;
constexpr int DRUM_SETUP_NOTE0 = 13;
constexpr int DRUM_SETUP_SETS  = 4;

inline u32 drum_setup(int set, int note, int param)
{
	return DRUM_SETUP + u32(set) * DRUM_SETUP_PARAM * DRUM_SETUP_NOTES
	     + u32(note - DRUM_SETUP_NOTE0) * DRUM_SETUP_PARAM + u32(param);
}

// XG の番地（3n rr pp の pp）→ 上の並びの番号（0-22）。無い番地は -1。
// 00-0F はそのまま、あとは 20・21・24・25・50・60・61（xgtest --drumprobe で割り出した）
inline int drum_setup_index(int addr)
{
	if (addr >= 0 && addr < 16)
		return addr;
	switch (addr) {
	case 0x20: return 16;   // EQ 低音の量
	case 0x21: return 17;   // EQ 高音の量
	case 0x24: return 18;   // EQ 低音の周波数
	case 0x25: return 19;   // EQ 高音の周波数
	case 0x50: return 20;   // HPF
	case 0x60: return 21;   // 強さで音程
	case 0x61: return 22;   // 強さで切る高さ
	default:   return -1;
	}
}
// その番号の既定値（リセットした直後のワーク RAM。EQ の周波数のほかは 64）
inline int drum_setup_default(int index)
{
	return index == 18 ? 0x0c : index == 19 ? 0x36 : 64;
}

constexpr u32 VOICE_MODE = 0x226bc; // 音色の引き方（1 が XG）。xg/voices.h の lookup に渡す
constexpr u32 VOICE_SET  = 0x226de; // 音色の組の選び方（MU2000 の音色なら 1）
constexpr u32 EFFECT   = 0x0cad8;   // 02 01 00 から。下の EFFECTS の並び
constexpr u32 EFFECT_SIZE = 0x16b;  // 02 01 00 からマスター EQ の終わりまで
// パートの塊は PART_STRIDE ずつ並ぶが、**並びは XG のパート番号の順ではない**。
// 口ごとに「10 番目のパート（ch10）が先頭、残りが 1-9, 11-16」の固定の順（ドラムかどうかに
// よらない。パートを DRUM にしても並びは変わらなかった）
constexpr u32 PARTS    = 0x28d64;   // 並びの先頭（パート 10 の塊）
constexpr u32 PART_STRIDE = 0x134;

// XG のパート番号（0-31）から、塊の先頭
constexpr u32 part_base(int part)
{
	const int port = part / 16, k = part % 16;
	const int slot = k == 9 ? 0 : k < 9 ? k + 1 : k;
	return PARTS + u32(port * 16 + slot) * PART_STRIDE;
}
constexpr u32 PART_XG_SIZE = 0x29;  // 08 pp 00-28
// パートの EQ（08 pp 72-77）は塊の +0x6A から。XG の番地から 8 引いた所
// **スケールチューニング**（XG の 08 pp 41-4C ＝ C から B まで 12 個。
// 64 が 0 セント）。ワーク RAM では +0x3A から 12 バイト。
// 実機が書くところを見て突き止めた（doc/native-engine.md の 6.127）
constexpr u32 PART_SCALE_XG  = 0x41;
constexpr u32 PART_SCALE_RAM = 0x3a;
constexpr u32 PART_SCALE_SIZE = 12;
// **08 pp 41-6E**（スケールチューニング・アフタータッチ・AC1・AC2・ポルタメント・ピッチ EG・
// ベロシティの範囲）。ワーク RAM では塊の +0x3A から、XG の番地の順にそのまま 46 バイト。
// 1 つずつ書いて RAM の変わった所で確かめた（41 → +3A、4D → +46、59 → +52、67 → +60、6E → +67）
constexpr u32 PART_EXT_XG   = 0x41;
constexpr u32 PART_EXT_RAM  = 0x3a;
constexpr u32 PART_EXT_SIZE = 0x2e;
// **HPF の切る高さ**（0A pp 20。パートの塊の番地は 08 ではなく 0A）。塊の +0x78
constexpr u32 PART_HPF_HI  = 0x0a;
constexpr u32 PART_HPF_XG  = 0x20;
constexpr u32 PART_HPF_RAM = 0x78;
constexpr u32 PART_EQ_XG   = 0x72;
constexpr u32 PART_EQ_RAM  = 0x6a;
constexpr u32 PART_EQ_SIZE = 6;
// 低域のゲイン・高域のゲイン・低域の周波数・高域の周波数（6.181）
constexpr u32 PART_EQ_LGAIN = 0x6a;
constexpr u32 PART_EQ_HGAIN = 0x6b;
constexpr u32 PART_EQ_LFREQ = 0x6e;
constexpr u32 PART_EQ_HFREQ = 0x6f;

// パートの塊の中の、XG に番地の無い演奏中の値
constexpr u32 PART_MOD  = 0x7d;     // CC1
constexpr u32 PART_EXP  = 0x7e;     // CC11
// **RPN の行き先**（doc/native-engine.md の 6.125）。XG の 08 pp のならびとは
// 別の場所に入る。実機が書くところを見て突き止めた
constexpr u32 PART_COARSE = 0xc9;   // RPN 2（粗調）。符号つきの半音（実機の式にある）
constexpr u32 PART_FINE   = 0xcc;   // RPN 1（微調）。16bit 符号つき、8192 で 100 セント
constexpr u32 PART_BEND = 0x80;     // ピッチベンドの MSB の半分（0x20 が真ん中）
constexpr u32 PART_HOLD = 0xd9;     // CC64。0 か 1
constexpr u32 PART_VOICE = 0xf8;    // 選んでいる音色の記録を指す値（ROM の中。xg/voices.h）
constexpr u32 PART_COPY = 0x100;    // 画面へ写す長さ（上の全部を含む）

// エフェクトの塊。xg は XG の番地の先頭、ram はワーク RAM での先頭
struct block { u8 hi, mid, lo; u32 size; u32 ram; };

// リバーブ・コーラス・バリエーションは**詰めて並ぶ**（XG の番地そのままではない）。
// 1 つずつ書いて RAM の変わった所で確かめた:
//   リバーブ       02 01 00-0D が +0x00、10-15（パラメータ 11-16）が +0x0E
//   コーラス       02 01 20-2E が +0x00。30-35 は書いても RAM に入らない（firmware が捨てる）
//   バリエーション 02 01 40-41（種類）が +0x00、パラメータ 1-10（42-55）は +0x02 から
//                  16bit の数で 10 個（VAR_WIDE）、56-60 が +0x16、70-75 が +0x21
constexpr block EFFECTS[] = {
	{ 0x02, 0x01, 0x00, 0x0e, 0x0cad8 },   // リバーブ
	{ 0x02, 0x01, 0x10, 0x06, 0x0cae6 },
	{ 0x02, 0x01, 0x20, 0x0f, 0x0caec },   // コーラス
	{ 0x02, 0x01, 0x40, 0x02, 0x0cb02 },   // バリエーション
	{ 0x02, 0x01, 0x56, 0x0b, 0x0cb18 },
	{ 0x02, 0x01, 0x70, 0x06, 0x0cb23 },
	// インサーションは 03 0n 00-11 がそのまま並び、そのあとに 20-25（パラメータ 11-16）が
	// 詰めて続く。1 つずつ書いて RAM の変わった所で確かめた。
	// パラメータ 1-10 の 2 バイトの番地（30-43）は、+0x18 から 16bit の数で 10 個並ぶ
	// （7bit ずつではないので、この表には入れない。INS_WIDE を見よ）
	{ 0x03, 0x00, 0x00, 0x12, 0x0cb7e },   // インサーション 1
	{ 0x03, 0x00, 0x20, 0x06, 0x0cb90 },
	{ 0x03, 0x01, 0x00, 0x12, 0x0cbaa },   // インサーション 2
	{ 0x03, 0x01, 0x20, 0x06, 0x0cbbc },
	{ 0x03, 0x02, 0x00, 0x12, 0x0cbd6 },   // インサーション 3
	{ 0x03, 0x02, 0x20, 0x06, 0x0cbe8 },
	{ 0x03, 0x03, 0x00, 0x12, 0x0cc02 },   // インサーション 4
	{ 0x03, 0x03, 0x20, 0x06, 0x0cc14 },
	{ 0x02, 0x40, 0x00, 0x15, 0x0cc2e },   // マスター EQ（02 40 00-14）
};

// インサーション n（0-3）の塊の先頭と、そこからパラメータ 1-10 の 16bit の数（上位バイトが先）の位置
constexpr u32 INS_BLOCK[4] = { 0x0cb7e, 0x0cbaa, 0x0cbd6, 0x0cc02 };
// **その塊の中の「掛かり先のパート」**（XG の 03 0n 0C。0x7f なら掛けない）
constexpr u32 INS_PART = 0x0c;
// バリエーションの塊（02 01 40-5B）。+0x1a が繋ぎ方（0 ＝ インサーション）、
// +0x1b が掛かり先のパート（doc/native-engine.md の 6.161）
constexpr u32 VAR_BLOCK = 0x0cb02;
constexpr u32 VAR_CONNECT = 0x1a;
constexpr u32 VAR_PART    = 0x1b;
constexpr u32 INS_WIDE = 0x18;
// バリエーションのパラメータ 1-10（02 01 42-55）。塊の +0x02 から 16bit の数（上位バイトが先）
constexpr u32 VAR_WIDE = 0x02;

// XG の番地から、ワーク RAM での位置。無ければ false
inline bool locate(u32 addr, u32 &off)
{
	const u8 hi = u8(addr >> 14), mid = u8((addr >> 7) & 0x7f), lo = u8(addr & 0x7f);
	if (hi == 0x00 && mid == 0x00 && lo < 7) {
		off = SYSTEM + lo;
		return true;
	}
	if (hi == 0x08 && mid < 32 && lo < PART_XG_SIZE) {
		off = part_base(mid) + lo;
		return true;
	}
	if (hi == 0x08 && mid < 32 && lo >= PART_EXT_XG && lo < PART_EXT_XG + PART_EXT_SIZE) {
		off = part_base(mid) + PART_EXT_RAM + (lo - PART_EXT_XG);
		return true;
	}
	if (hi == PART_HPF_HI && mid < 32 && lo == PART_HPF_XG) {
		off = part_base(mid) + PART_HPF_RAM;
		return true;
	}
	if (hi == 0x08 && mid < 32 && lo >= PART_EQ_XG && lo < PART_EQ_XG + PART_EQ_SIZE) {
		off = part_base(mid) + PART_EQ_RAM + (lo - PART_EQ_XG);
		return true;
	}
	for (const block &b : EFFECTS) {
		if (hi == b.hi && mid == b.mid && lo >= b.lo && lo < b.lo + b.size) {
			off = b.ram + (lo - b.lo);
			return true;
		}
	}
	return false;
}

} // namespace ram
} // namespace xg

#endif // S_MU2000_XG_RAM_H
