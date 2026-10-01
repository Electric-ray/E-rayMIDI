// license:BSD-3-Clause
//
// 音色の記録（ROM の 84 バイト）から、SWP30 のスロットのレジスタを組み立てる。
// **firmware を走らせずに音を出す**ための最初の部品（doc/native-engine.md の段 2）。
//
// 番地と式はすべて firmware を読んで決めた（同 6.2-6.6）。分かっていない所は
// 「まだ分からない」と書いて、実機を鳴らして測った値をそのまま置いてある。
// ここに入っているのは**式だけ**で、ROM の中身は持たない（実行時に読むだけ）。

#ifndef S_MU2000_XG_NATIVE_VOICE_H
#define S_MU2000_XG_NATIVE_VOICE_H

#pragma once

#include "compat/mamecompat.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace xg {
namespace nv {

// ROM の中の番地（MU2000 EX firmware v2.01）
constexpr u32 SET_TABLE  = 0x200AF0;   // 波形の組 → 波形の並びの中の位置（16bit を 503 個）
constexpr u32 SET_COUNT  = 0x1F8;
constexpr u32 WAVE_BASE  = 0x1F55A0;   // 波形の記録（16 バイトずつ）
constexpr u32 ATTACK_TAB = 0x1F4DB8;   // アタックの速さ（128 バイト）
// **EG のつまみ（CC72/73/75）で速さの目盛りをどう動かすか**（6.157）。
// `tools/native/egtab.py` で 128 段ぜんぶ測って、3 音色で突き合わせた。
//
//   つまみ <= 64 : 目盛り = min(63, 素の目盛り + (65 - つまみ) / 2)
//   つまみ >  64 : 目盛り = min(素の目盛り, 表[つまみ - 64])
//
// 上の向き（遅くする側）は音色ごとの目盛りからの足し算だが、**下の向き
// （速くする側）は音色によらない**。3 音色（Strings1・GrandPno・Flute）の
// どれも、つまみ 72 で目盛り 28、つまみ 127 で 1 になる。
// 表は ROM にそのまま入っていた（頭が 63 ＝「変えない」）
constexpr u32 EG_RATE_CC = 0x1E54A4;    // つまみ 64-127 → 目盛り（64 バイト）

inline int eg_rate_cc(const u8 *rom, int base, int cc)
{
	// 実機（`0x127288`）はつまみ 64 でも 63 で頭打ちにする
	if (!rom || cc < 0 || cc == 64)
		return base > 63 ? 63 : base;
	const int c = cc > 127 ? 127 : cc;
	if (c < 64) {
		const int v = base + (65 - c) / 2;
		return v > 63 ? 63 : v;
	}
	const int cap = int(rom[EG_RATE_CC + u32(c - 64)]);
	return cap < base ? cap : base;
}

// **減衰のつまみ（CC75）は足し算**（立ち上がりと違って表を使わない）。
// 3 音色とも同じずれで、下は 0 で止まる（実機の値がそこで飽和する）。
//
//   つまみ <= 64 : 目盛り + (67 - つまみ) / 4
//   つまみ >  64 : 目盛り - (つまみ - 64) × 7 / 16
//
// 1/4 と 7/16 は `tools/native/egtab.py` で 128 段ぜんぶ測って合わせた
inline int eg_rate_cc_add(int base, int cc)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	// **頭打ちは鍵の補正を足したあと**（6.212）。実機（`0x127338`）は
	// `v += 声の塊 +109` してから 0-63 に収めるので、ここで先に収めると
	// byte75 が 47 より大きい音色で CC75 を下げたときにずれる
	return c < 64 ? base + (67 - c) / 4
	              : base - (c - 64) * 7 / 16;
}

// **立ち上がりのつまみ（CC73）は減衰 1（0x07）も動かす**（6.157）。
// 実機の `0x1272F4` は遅くする側では何もせず、速くする側だけ
//
//   目盛り = min(素, ((127 - つまみ) >> 2) + 2)
//
// とする。前に測って合わせた `素 - (つまみ-68)/4` は、
// 素が 16 のときだけこれと同じになる（6.171）
inline int eg_dec1_cc(int base, int cc)
{
	if (cc <= 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	const int cap = ((127 - c) >> 2) + 2;
	return cap < base ? cap : base;
}
constexpr u32 DECAY_TAB  = 0x1F4E38;   // 減衰の速さ（128 バイト）
// **離しは別の表**（6.212）。途中までは減衰の表と同じ並びだが、
// 速い側（目盛り 48 以上）だけ違う（減衰 95 98 100 102 104 104 108 108 112… /
// 離し 95 95 96 96 96 96 101 101 104…）。DECAY_TAB を引いていたので、CC72 を
// 下げ切ったときだけこちらの離しが速すぎた（Strings で 0xEC 対 0xE5）
constexpr u32 REL_TAB    = 0x1F4EB8;   // 離しの速さ（128 バイト）
constexpr u32 VEL_CURVE  = 0x1E5E5E;   // 強さの曲線（128 バイトの行が並ぶ。行 0 はそのまま）
// **つまみの割り当て「音量」の表**（6.195。実機 0x12A782）。
// |深さ - 64| を索引に引いて、値と掛けて 8 びったものを
// パートの目盛り（0-128）に足す。中身は 3 × 索引
constexpr u32 AMP_ASSIGN_TAB = 0x1E6899;

constexpr u32 LEVEL_TAB  = 0x1E6798;   // 0-127 → 減衰（128 バイトの行が並ぶ。行 1 が 0x1E6818）
constexpr u32 SLOT_TABLE = 0x1F4F58;   // スロット番号 → レジスタの先頭（4 バイト × 64）
constexpr u32 CUTOFF_TAB = 0x1E5B58;   // フィルタの切る高さ（16bit。索引は記録の byte37）

inline int s8(u8 v) { return v >= 128 ? int(v) - 256 : int(v); }
inline u16 rd16(const u8 *rom, u32 a) { return u16(rom[a] << 8 | rom[a + 1]); }
inline u32 rd32(const u8 *rom, u32 a)
{ return u32(rom[a]) << 24 | u32(rom[a + 1]) << 16 | u32(rom[a + 2]) << 8 | rom[a + 3]; }

// 音色の記録の 84 バイト（要素 1 つぶん）。rec は xg::voice_rom::lookup の戻り値
inline const u8 *element(const u8 *rom, u32 rec, int index = 0)
{
	return rom + rec + 12 + u32(index) * 84;
}
// 記録の先頭のバイトは**要素のビットマスク**（1/3/7/15 ＝ 1〜4 要素）。
// 数ではないので、立っているビットを数える
inline int element_count(const u8 *rom, u32 rec)
{
	int n = 0;
	for (int i = 0; i < 4; i++)
		if (rom[rec] & (1 << i))
			n++;
	return n;
}

// その要素が、この鍵と強さで鳴るか（byte4,5 が鍵の範囲、byte6,7 が強さの範囲）
inline bool element_active(const u8 *elem, int note, int vel)
{
	return note >= elem[4] && note <= elem[5] && vel >= elem[6] && vel <= elem[7];
}

// 波形の組の番号（7bit が 2 つ）
inline int wave_set(const u8 *elem) { return (elem[2] << 7) | (elem[3] & 0x7f); }

// その鍵で使う波形の記録（16 バイト）。無ければ nullptr
inline const u8 *wave_entry(const u8 *rom, int setno, int note)
{
	if (setno < 0 || setno >= int(SET_COUNT))
		return nullptr;
	u32 s = WAVE_BASE + rd16(rom, SET_TABLE + u32(setno) * 2);
	for (int i = 0; i < 80; i++) {
		if (rom[s + 3] >= note || rom[s + 3] == 0x7f)
			return rom + s;
		s += 16;
	}
	return nullptr;
}

// 波形の記録の中身
struct wave_info {
	int level;         // この波形ぶんの減衰（0.375dB 目盛り。多段サンプルで段ごとに違う）
	int base_key;      // もとの音程（半音）
	int fine_cents;    // その細かい調整（セント。引く）
	int key_max;       // この記録を使う鍵の上限
	u32 pre_loop;      // ループ前のサンプル数（レジスタ 0x12/0x13）
	u32 loop_len;      // ループの長さ（0x14/0x15）
	u32 format_addr;   // 形式＋波形 ROM の番地（0x16/0x17）
};

inline wave_info read_wave(const u8 *e)
{
	wave_info w{};
	w.level      = e[0];
	w.base_key   = e[1];
	w.fine_cents = e[2] >= 128 ? int(e[2]) - 256 : int(e[2]);
	w.key_max    = e[3];
	w.pre_loop   = u32(e[4]) << 24 | u32(e[5]) << 16 | u32(e[6]) << 8 | e[7];
	w.loop_len   = u32(e[8]) << 24 | u32(e[9]) << 16 | u32(e[10]) << 8 | e[11];
	w.format_addr = u32(e[12]) << 24 | u32(e[13]) << 16 | u32(e[14]) << 8 | e[15];
	return w;
}

// **逆向きに鳴らすサンプル**（`0x14/0x15` の bit31）は、2 つの数を
// **入れ替えて**書く（doc/native-engine.md の 6.233）。チップは後ろから
// 読むので、「ループまでの数」と「ループの長さ」の役割が入れ替わる
// （swp30.cpp の「Sample addressing, pitching and looping」の説明）。
// 旗（上の 8bit）はそれぞれの側に残る。
//
// 記録のまま書くと、逆向きのまま長さ 1 で止まるので、**音程は合っているのに
// ざらついた音**になる。実機と突き合わせて見つかったのは 2 つ:
//   StandKit#・StandKit の鍵 47/48/50（Mid Tom L/H・High Tom）
//     記録 pre=0x5302 loop=0x000001 → 実機 pre=0x000001 loop=0x5302
//   AnalogKit の鍵 28
//     記録 pre=0x5125 loop=0x002174 → 実機 pre=0x002174 loop=0x5125
// 2 つ目は数がどちらも大きいので、「入れ替え」であって「1 を書く」ではない
inline void wave_backwards_swap(u32 &pre, u32 &loop)
{
	if (!(loop & 0x80000000))
		return;
	const u32 a = pre & 0xffffff, b = loop & 0xffffff;
	pre  = (pre  & 0xff000000) | b;
	loop = (loop & 0xff000000) | a;
}

// 音程のレジスタ（0x11）。1 オクターブ = 1024、細かい調整はセント（**足す**）。
// 実測（鍵 0-127・18 区画）と ±0.7 目盛りで合う
// 鍵の追従率（記録の byte19）。**表は ROM の `0x1E5E58` に 6 個**
// （`100, 50, 20, 10, 5, 0`。そのすぐ後ろが強さの曲線 `0x1E5E5E`）。
//
// 前は 4 個の表を `byte19 & 3` で引いていたので、**byte19 が 4 の要素を
// 100、5 を 50 と読んでいた**。実機が引く記録を全部当たると byte19 は
// 0-5 で、4 か 5 の要素が 16 個ある（GM では Goblins・MelodTom・FretNoiz の
// 第 2 要素。ほかは効果音バンク）。5 は「鍵でまったく動かない」＝ 打楽器や
// 効果音の音（6.112）
constexpr u32 KEY_FOLLOW_TAB = 0x1E5E58;
constexpr int KEY_FOLLOW_N   = 6;

inline int key_follow(const u8 *rom, const u8 *elem)
{
	const int i = int(elem[19]);
	if (!rom || i < 0 || i >= KEY_FOLLOW_N)
		return 100;
	return int(rom[KEY_FOLLOW_TAB + u32(i)]);
}

// **鍵の追従の支点**（byte20）。ほとんどの要素は 60（中央のド）だが、
// Bottle の 75 や Applause の 57 のように別の鍵を支点にするものがある。
// 支点が 60 でないと、追従が 100 でない音色では鍵 60 でも値がずれる（6.96）
inline int key_pivot(const u8 *elem) { return elem[20]; }

// 要素を**遅らせて鳴らす**段（byte72）。
//
//   遅れ = 441 * 2^(n-1) - 82   サンプル
//
// **実機の 1 つ目の押鍵から 2 つ目の押鍵まで**を測り直した（6.176）。
// MusicBox（n=3）が 1682、SynStrings 1/2 と FrenchHorn（n=2）が 800 で、
// 鍵 48・60・72 のどれでも同じ。前の `- 130` は 48 サンプル短かった。
//
// **48 サンプルでも聞こえる**。FrenchHorn は 2 つの要素が 9 目盛りだけ
// ずれていて、そのうなりで音ができている。片方が 48 サンプルずれると
// うなりの位相が 0.2 秒ずれて、波形が丸ごと合わなくなる
inline u32 elem_delay(const u8 *elem)
{
	const int n = elem[72] & 0x7f;
	if (n <= 0)
		return 0;
	return u32(441 * (1 << (n < 8 ? n - 1 : 7)) - 82);
}

// **波形を選ぶときの鍵**。実機は「その要素が実際に出す高さ」で選ぶので、
// 粗調（byte17）だけでなく**鍵の追従（byte19）と支点（byte20）**も入る。
//
//   選ぶ鍵 = 支点 + (鍵 - 支点) * 追従 / 100 + (byte17 - 64)
//
// * GtHarmonics（31）は byte17 が 12 半音下、追従 100 で、鍵 84 のときに
//   鍵 72 のぶんの波形を鳴らす（6.93）
// * Rain（96）は byte17 が +24、**追従 20**、支点 60。鍵 84 なら
//   60 + 24*20/100 + 24 = 88 で、上限鍵 96 の記録に入る。粗調だけで
//   数えると 108 になって、1 つ先の記録（上限鍵 108）を取ってしまう（6.110）
inline int wave_note(const u8 *rom, const u8 *elem, int note)
{
	const int piv = key_pivot(elem);
	const int n = piv + (note - piv) * key_follow(rom, elem) / 100 + int(elem[17]) - 64;
	return n < 0 ? 0 : (n > 127 ? 127 : n);
}

// **鍵が 0-127 からはみ出した半音数**（6.141）。`wave_note` が丸めているぶん
inline int note_overflow(const u8 *rom, const u8 *elem, int note)
{
	const int piv = key_pivot(elem);
	const int n = piv + (note - piv) * key_follow(rom, elem) / 100 + int(elem[17]) - 64;
	return n > 127 ? n - 127 : (n < 0 ? n : 0);
}

// 要素ぶんの音程のずらし（セント）。byte17 が半音、byte18 がセント
inline int elem_tune(const u8 *elem)
{
	return (int(elem[17]) - 64) * 100 + (int(elem[18]) - 64);
}

// **ポルタメントの速さ**（doc/native-engine.md の 6.41）。
// ROM の表 0x1E6698（16bit・128 語）を CC5 で直に引く。目盛りが 2 通りある:
//   CC5 24-127 … 表 ÷ 128 = 10ms あたりのセント
//   CC5  0-23  … 表 × 2   = 10ms あたりのセント（256 倍の目盛り）
// 返すのは**セント × 256**（そのまま足し引きできる細かさ）
constexpr u32 PORTA_TAB = 0x1E6698;
constexpr u32 PORTA_TICK = 441;            // firmware は 10ms ごとに足す

inline int porta_step(const u8 *rom, int cc5)
{
	if (!rom || cc5 < 0 || cc5 > 127)
		return 0;
	const int raw = int(rd16(rom, PORTA_TAB + u32(cc5) * 2));
	return cc5 < 24 ? raw * 512 : raw * 2;
}

// **滑りの残りをセントに直す**（6.197）。`glide` はセント × 256 で、
// 上へ滑るときは負。C++ の `/` は 0 の側へ切り捨てるので、
// そのままだと**上へ滑るときだけ 1 セント高め**になる。
// 実機は下へ落とす（CC5=32 の滑りで、レジスタが 1 だけ高かった）
inline int glide_cents(int glide)
{
	return glide >= 0 ? glide / 256 : -((-glide + 255) / 256);
}

inline u16 pitch_reg(const wave_info &w, int note, int follow = 100,
                     int cents_extra = 0, int pivot = 60)
{
	// 整数で計算する（firmware と同じ丸めになる。0 の側へ切り捨て）。
	// **鍵の追従は要素の支点（byte20）を軸にする**（波形の基準鍵ではない）。
	// 追従が 100 のときは同じ式になるが、50 や 20 の音色では基準鍵との
	// ずれぶん食い違う（Woodblock で 749 セント、TaikoDrum で 1700 セント。
	// どちらも 50 * (60 - 基準鍵) でぴったり）。
	// 支点はほとんどの要素で 60 なので長らく定数で足りていたが、Bottle（75）
	// と Applause（57）だけ違っていて、鍵 60 でも値がずれていた（6.96）
	const int cents = (note - pivot) * follow + (pivot - w.base_key) * 100
	                + w.fine_cents + cents_extra;
	const int v = cents * 1024 / 1200;
	// ビット 14 は波形の**形式**で決まる（形式 3 のときだけ立つ。402 組で確かめた）
	const u16 flag = ((w.format_addr >> 30) & 3) == 3 ? 0x4000 : 0;
	return u16((v & 0x3fff) | flag);
}


// ---- コントローラ（doc/native-engine.md の 6.14）
//
// 実機が何を書くかは `nativeplay --ccwatch` で見た:
//   CC7・CC11 → レジスタ 0x09 の下位バイト（減衰）
//   CC10      → レジスタ 0x32（上が左・下が右の減衰）
//   ベンド    → レジスタ 0x11（音程）
//   CC1       → レジスタ 0x0a の下位バイト（LFO の深さ）

// 音量（CC7）・表現（CC11）の減衰。level→減衰の表（0.375dB 目盛り）を 2 倍すると
// レジスタ 0x09 の目盛り（0.1875dB）になる。cc>=8 で実測との差は 0.375dB 以内
// **音量と表現は掛けてから一度だけ減衰に直す**（実機の 0x12A404 がそうしている）。
// firmware はパートの塊の +0x12E-0x130 に `((CC7+1) * (CC11+1)) >> 7` を
// 線形のまま持っていて（`nativeplay --ccbyte 7` と `--ccbyte 11` で確かめた。
// CC7 だけ振ると cc+1、CC11 だけ振ると 101*(cc+1)/128 でぴったり）、
// それを音の level に掛けてから減衰に直す。
// 前は CC7 と CC11 を別々に減衰へ直して足していたので、実機とずれていた
inline int vol_gain(int vol, int expr)
{
	const int v = vol < 0 ? 100 : (vol > 127 ? 127 : vol);
	const int e = expr < 0 ? 127 : (expr > 127 ? 127 : expr);
	// **音量 0 は素通しで 0**。式どおりなら ((0+1)*(127+1))>>7 = 1 になるが、
	// 実機のパートの塊 +0x12F は CC7=0 で 0 になる（実測）
	if (v == 0)
		return 0;
	return ((v + 1) * (e + 1)) >> 7;        // 0-128
}

// **音量の目盛りに掛ける**（実機の `0x12A4AA`）。パートの塊の +0x12F が
// この線形の値（0-128）で、実測で `((音量+1) * (エクスプレッション+1)) >> 7`
// そのもの（CC7 と CC11 を 0-127 まで振って 256 点すべて一致）。
// 実機は**目盛りに掛けてから** 1 回だけ減衰の表を引く（6.101）。
// 掛けた結果が 0 になったら 1（`0x12A4C0`）。掛ける値が 0 なら目盛りごと 0
inline int level_with_gain(int level, int gain)
{
	if (gain <= 0 || level <= 0)
		return 0;
	int v = (level * (gain > 128 ? 128 : gain)) >> 7;
	if (v <= 0)
		v = 1;
	return v > 128 ? 128 : v;
}

// その逆。写し取ったときの目盛りから、掛ける前の目盛りを取り戻す。
// `(A * gain) >> 7 == l` になる A は幅を持つので**真ん中**を取る
// （写し取ったときの値はそのまま戻り、ほかの音量でのずれがいちばん小さい）
inline int level_without_gain(int l, int gain)
{
	if (gain <= 0)
		return 0;
	if (gain >= 128 || l <= 0)
		return l < 0 ? 0 : l;
	const int lo = (l * 128 + gain - 1) / gain;
	const int hi = ((l + 1) * 128 - 1) / gain;
	const int a = (lo + (hi < lo ? lo : hi)) / 2;
	return a > 128 ? 128 : a;
}

// その線形の値（0-128）を減衰に直す
// つまみ 1 つぶんの、目盛りへの足し分（6.195）
inline int amp_assign(const u8 *rom, int depth, int value)
{
	const int d = depth - 64;
	if (!d || !value)
		return 0;
	const int t = int(rom[AMP_ASSIGN_TAB + u32(d < 0 ? -d : d)]);
	const int v = int(u16(t * value) >> 8);
	return d < 0 ? -v : v;
}

inline int gain_att(const u8 *rom, int gain)
{
	if (gain <= 0)
		return 255;
	return 2 * int(rom[LEVEL_TAB + u32(std::min(128, gain) - 1)]);
}

inline int cc_vol_att(const u8 *rom, int cc)
{
	if (cc <= 0)
		return 255;
	return 2 * int(rom[LEVEL_TAB + u32(std::min(127, cc) - 1)]);
}

// パン（CC10）の減衰。中央で左右とも -3dB になる cos 則。
// 右側は pan_att(128 - cc10)。
//
// **ROM に表がある**（`0x1BBAD0` の 128 バイト。6.100）。cos の式で出すと
// 14 点で 1 ずれていた（丸め方の違い）。この表だと CC 0-127 の左右 128 点が
// 1 つ残らず実機と合う。`0x1E6B88` にも同じ曲線の**切り捨て**版があって、
// 送りの表（6.99）と同じ組になっている
constexpr u32 PAN_ATT_TAB = 0x1BBAD0;

inline int pan_att(const u8 *rom, int x)
{
	if (x <= 0)
		return 0;
	if (x >= 127)
		return 255;
	if (rom)
		return int(rom[PAN_ATT_TAB + u32(x)]);
	const double c = std::cos(double(x) / 127.0 * 1.5707963267948966);
	const int v = int(std::lround(-20.0 * std::log10(c) / 0.375));
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// 明るさ（CC74）→ レジスタ 0x00 の下 12bit（切る高さ）。
// 実測（`nativeplay --ccfilter`）は **16 × (値 - 64)** でまっすぐ動き、1984 で頭打ち
constexpr int CUTOFF_MAX = 1984;
inline int bright_shift(int cc) { return 16 * (cc - 64); }

// 共振（CC71）→ レジスタ 0x04 の上 5bit。実測は **2 きざみで 1 段**
// （64 まで 0、67 で 1、127 で 31）
inline int reso_shift(int cc) { return (cc - 64) / 2; }

// 送り（CC91 リバーブ・CC93 コーラス）→ レジスタ 0x33・0x34 の下位（減衰）。
// 実測は **16 + 送りの表** で、音色によらない（GrandPno・Strings・Flute で同じ）。
// 使うのは差ぶんだけなので、下駄の 16 は要らない。
//
// **表は `LEVEL_TAB` ではない**（6.99）。音量の表（0x1E6798）は同じ曲線を
// 切り捨てで持っていて、送りの表（0x1B99B9）は四捨五入で持っている。
// 1 きざみずつ違うので、CC91 を振ると 1 ずれた値を書いていた。
// 実測 49 点（CC 1-127）が 0x1B99B9 と 1 つ残らず合う
constexpr u32 SEND_TAB = 0x1B99B9;      // 0-127 → 送りの減衰（127 バイト。番号 0 が CC1）

inline int send_att(const u8 *rom, int cc)
{
	if (cc <= 0)
		return 255;
	return int(rom[SEND_TAB + u32(std::min(127, cc) - 1)]);
}

// **ビブラートのつまみ**（08 pp 15 速さ・16 深さ ＝ NRPN 01 08/09）。
// どちらもレジスタ `0x0a` を動かす（上位が速さ、下位が深さ）。
// `tools/native/egtab.py` で 128 段ぜんぶ測り、2 音色で突き合わせた
// （doc/native-engine.md の 6.162）。
//
//   速さ: つまみ < 64 なら min(素の目盛り, 表)、> 64 なら max(素の目盛り, 表)
//         （表の 63 と 0 は「基準を残す」印になる）
//   深さ: 素の深さに表の値を足す
constexpr u8 VIB_RATE_TAB[128] = {
	  0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   2,   2,   2,   2,   3,
	  3,   4,   4,   5,   6,   7,   8,   9,   9,  10,  10,  10,  11,  11,  12,  12,
	 13,  13,  14,  14,  14,  15,  15,  15,  17,  17,  17,  17,  19,  19,  19,  21,
	 21,  21,  23,  23,  23,  25,  25,  25,  27,  27,  29,  29,  29,  29,  29,  63,
	 63,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  38,  38,  38,  38,  38,
	 38,  38,  38,  38,  38,  38,  38,  38,  38,  46,  46,  46,  46,  46,  46,  46,
	 46,  46,  46,  46,  46,  46,  53,  53,  53,  53,  53,  53,  53,  53,  63,  63,
	 63,  63,  63,  63,  63,  63,  63,  63,  63,  63,  63,  63,  63,  63,  63,  63,
};

constexpr u8 VIB_DEPTH_TAB[128] = {
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,  29,  40,  52,  69,  86,  86,  86,  86,  86, 145, 145, 145, 145, 145, 150,
	150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 160, 160, 160, 160,
	160, 160, 160, 160, 160, 160, 160, 160, 163, 163, 163, 163, 163, 163, 163, 163,
	163, 163, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 173, 173, 173, 173,
};

inline int vib_rate(int base, int cc)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	const int t = int(VIB_RATE_TAB[c]);
	const int v = c < 64 ? (t < base ? t : base) : (t > base ? t : base);
	return v < 0 ? 0 : (v > 63 ? 63 : v);
}

// **足さずに、すぐ上の `vib_rate` と同じ「大小で選ぶ」形**（6.221）。
// 音色自身が深さを持つもの（ChiffLead は 6）で、つまみを 64 から動かすと
// 実機は音色自身のぶんを**捨てて**つまみの表の値にする。足していたので
// 6 段ぶん深すぎた。実機で測った値（ChiffLead・自身 6）:
//
//   つまみ 0-63 → 00（表が 0。小さいほう）   64 → 06（音色自身）
//   65 → 1D、70 → 56、80 → 96、96 → A0、127 → AD（どれも表の値。大きいほう）
//
// 表の 0-64 は 0 なので、下側は「つまみが勝って 0」になる。
// 自身が 0 の音色（GrandPno）はどちらの式でも同じ値になるため、
// **自身が 0 でない音色でしか出ない**差だった
inline int vib_depth(int base, int cc)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	const int t = int(VIB_DEPTH_TAB[c]);
	const int v = c < 64 ? (t < base ? t : base) : (t > base ? t : base);
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// **ビブラートの遅れのつまみ**（08 pp 17 ＝ NRPN 01 0A）。20ms の目盛り。
// firmware を 128 段ぜんぶ測って作った（doc/native-engine.md の 6.232）。
// 測り方: 08 pp 17 だけを変えた MIDI を `--native-engine` 無しで鳴らし、
// 押鍵から `0x0a` の深さが初めて増えるまでを数える
// （押鍵からの時刻 = 5.2ms + 20ms × 目盛り。当てはめの外れは最大 0.15 目盛り）。
//   下（0-63）は Shakuhachi（自身 33 目盛り）で「小さいほう」が勝つ側から、
//   上（65-127）は Strings（自身 0）で「大きいほう」が勝つ側から読んだ
constexpr u8 VIB_DLY_TAB[128] = {
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
	  2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,
	  4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   8,   8,   8,
	  0,   9,   9,  10,  10,  10,  11,  11,  11,  12,  12,  13,  14,  15,  16,  18,
	 18,  19,  19,  19,  20,  21,  22,  23,  24,  26,  28,  30,  33,  35,  38,  38,
	 39,  40,  41,  41,  42,  43,  44,  44,  45,  48,  51,  55,  59,  63,  68,  72,
	 77,  81,  87,  93,  99, 107, 114, 123, 131, 138, 149, 159, 168, 183, 195, 206,
};

// **速さ・深さと同じ「大小で選ぶ」形**（6.232）。つまみが 64 なら音色のまま。
// 64 より下なら小さいほう（＝つまみで遅れを詰められる）、上なら大きいほう。
// 実機で確かめた例（Shakuhachi は自身 33 目盛り ＝ 665ms）:
//   つまみ 0 → 0ms、16 → 25ms、32 → 45ms、48 → 85ms、63 → 165ms（どれも表が勝つ）
//   つまみ 64 → 665ms、80 → 665ms（表は 18 目盛りなので自身が勝つ）
inline int vib_delay(int base, int cc)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	const int t = int(VIB_DLY_TAB[c]);
	const int v = c < 64 ? (t < base ? t : base) : (t > base ? t : base);
	return v < 0 ? 0 : v;
}

// モジュレーション（CC1）→ レジスタ 0x0a の下位（LFO の深さ）に足す。
// 実測は 10 段で、**音色によらない**（GrandPno・Strings・SawLead で同じ）。
// 0x0a の上位は LFO の型と刻みなので触らない
inline int mod_depth(int cc)
{
	static const u8 STEP[10] = { 0, 9, 17, 26, 35, 43, 52, 60, 72, 84 };
	static const u8 EDGE[9]  = { 13, 26, 39, 52, 64, 77, 90, 103, 116 };
	int i = 0;
	while (i < 9 && cc >= int(EDGE[i]))
		i++;
	return int(STEP[i]);
}


// ピッチベンド → セント。firmware は 2 回とも 0 の側へ切り捨てる
// （ベンド幅 2 半音・目一杯で 167 目盛り。実測と一致）
inline int bend_cents(int bend14, int range_semitones)
{
	return (bend14 - 8192) * range_semitones * 100 / 8192;
}

// 0..255 に収める
inline int clamp_att(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// 組み立てたスロットのレジスタ。write が立っている所だけ書く
struct slot_regs {
	u16 v[0x40];
	u64 write;         // ビット n が立っていればレジスタ n を書く

	slot_regs() { std::memset(v, 0, sizeof(v)); write = 0; }
	void set(int reg, u16 value) { v[reg] = value; write |= u64(1) << reg; }
};

// **フィルタの第 2 段（レジスタ 0x02。上 4bit の 8 がハイパス）**。実機（0x12AFCE）は要素の
// byte82（ドラムは記録の byte20）を 16 倍し、**パートの HPF（0A pp 20）の 24 ×（値 − 64）を足して**、
// 0-0x7FF に収める。GrandPno（byte82 = 0）と DistGtr（0x180）で HPF を 4 通り振り、ドラムの
// パートでも振って、実機の書いた値と全部一致した（0x60 → 0x300、0x7F → 0x5E8・0x768、低いと 0）
inline u16 filter2_reg(int byte82, int hpf = 64)
{
	return u16(0x8000 | u16(std::clamp(byte82 * 16 + 24 * (hpf - 64), 0, 0x7ff)));
}

// 分かっていない所に置く値。**実機を鳴らして測った、素直な音色のときの値**で、
// これは「式が分かっていない」という印でもある（doc/native-engine.md の 6.6）
struct defaults {
	u16 filter1 = 0x1000 | 0x7ff;   // 開き切り
	u16 bypass  = 0xdcff;           // 上位は「前の値からの変わり方」で決まる（0x127F10）
	u16 filter2 = 0x8000;
	u16 post    = 0x5010;           // ここは定数だと分かっている
	u16 filter2p = 0x0000;
	u16 lfo_amp = 0xfa00;
	u16 lfo     = 0x5f00;
	u16 r0b     = 0x7f00;
	u16 r10     = 0x4000;
	// ミキサ（パート 1・音量 100・パン中央・リバーブ送り 40 のときの実測）。
	// **0x32-0x37 だけ**。0x38-0x3d は「入力 0x40 から先」＝ MEG の戻りや A/D の
	// ぶんで、声のスロットのものではない。ここを書くと残響の混ざり方が変わる
	u16 mix[6] = { 0x0808, 0x182b, 0xffff, 0x4d00, 0x4800, 0x4400 };
	// 声ごとの IIR（パートの EQ）。素通しのときの実測
	u16 iir[6] = { 0xe05d, 0x1fa3, 0x2000, 0x0257, 0xfda9, 0x2000 };
};

// **ベロシティ感度**（XG の 08 pp 0C 深さ・0D ずらし。どちらも既定 64）。
// 実測（Strings1・強さ 100 と 40）で
//
//   効く強さ = clamp(強さ * 深さ / 64 + (ずらし - 64) * 2, 1, 127)
//
// 深さ 16 で 100 -> 25、32 で 50、48 で 75、80 以上で頭打ち。
// ずらし 32 で 100 -> 36、48 で 68、72 で 116、強さ 40 のときは 32 で 1
inline int vel_sense(int vel, int depth, int offset)
{
	const int d = depth  < 0 ? 64 : (depth  > 127 ? 127 : depth);
	const int o = offset < 0 ? 64 : (offset > 127 ? 127 : offset);
	int v = vel * d / 64 + (o - 64) * 2;
	if (v < 1) v = 1;
	return v > 127 ? 127 : v;
}

// 強さから、音量レジスタに足す減衰を出す（firmware の 0x128DA0）。
//
//   減衰 = 表2[0x1E6798 + 表1[0x1E5E5E + 曲線*128 + 強さ]]
//
// **曲線は要素の byte68 で選ぶ**（6.109）。表は 7 行しかない
// （0x1E5E5E から 0x1E61DE まで ＝ 128 × 7。行 7 の位置は別の表で、
// 値が曲線になっていない）。実機が引く記録 290 件・要素 451 個を当たると
// byte68 は**すべて 0-6**。念のため範囲外は行 0 に倒す。
// 行 0 は素通し、行 1 は少し丸い曲線。Bottle(76) と SoundTrk(97) が行 1 で、
// 実機のボイスの塊 +119 が強さ 40/100/127 で 22/4/0（行 0 なら 26/5/0）
constexpr int VEL_CURVE_ROWS = 7;

inline int velocity_att(const u8 *rom, int vel, int curve = 0)
{
	const u32 c = u32(curve >= 0 && curve < VEL_CURVE_ROWS ? curve : 0);
	const int i = rom[VEL_CURVE + c * 128 + u32(vel & 0x7f)];
	return rom[LEVEL_TAB + u32(i & 0x7f)];
}

// **サンプル＆ホールドの音程**（6.206。実機 0x1299E2）。
// `elem[9] == 2` の要素だけ。符号つきの乱数を半分にして byte14 を掛け、
// 4 で割ったものを `0x11` に**そのまま足す**（セントには直さない）
inline bool sh_lfo(const u8 *elem) { return int(elem[9]) >= 2; }
inline int  sh_ticks(const u8 *elem)
{
	const int n = 0x40 - int(elem[11]);
	return n < 1 ? 1 : n;
}
inline int  sh_pitch_off(int rnd, const u8 *elem)
{
	return ((s8(u8(rnd)) >> 1) * int(elem[14])) >> 2;
}
// 鍵を押すたびに LFO の初めの位相を乱数で決める要素（実機 0x12AD2E）
inline bool lfo_rnd_phase(const u8 *elem) { return elem[10] == 0; }

// その要素の強さの曲線の行（byte68）
inline int vel_curve_of(const u8 *elem) { return int(elem[68]); }

// **強さの利き幅**（6.204。実機 0x128E10）。
// 曲線を引く**前に強さを上へ寄せる**。
// byte36 が大きいほど弱い打鍵が持ち上げられて、利きが浅くなる。
// （2230 要素中 269 が 0 以外。S.Strngs は 1 で、
// 強さ 1 の減衰が 96 ではなく 48）
inline int vel_shift(const u8 *elem, int vel)
{
	const int d = int(elem[36]);
	if (!d)
		return vel;
	const int v = vel + (((127 - vel) * (d * 36)) >> 8);
	return v > 127 ? 127 : v;
}

inline int rd16s(const u8 *rom, u32 a)
{
	const int v = int(rd16(rom, a));
	return v >= 0x8000 ? v - 0x10000 : v;
}

// ---- **音程の包絡線（ピッチ EG）**（doc/native-engine.md の 6.68）
//
// スロットの `0x10` は「行き先の音程のずれ」、`0x0b` の上位バイトは「その速さ」で、
// 近づけるのはチップの仕事（swp30.cpp の `peg_step`）。firmware は
// **キーオンの直前に初めの高さを書き、キーオンの直後に行き先を書く**だけ。
//
// 実機の `0x12BAC8`-`0x12BBA0` を起こした:
//
//   速さ = 表 0x1E6C94[ clamp(byte26 + ((64 - パート[26]) >> 2), 0, 63) ]
//   初めの高さ = 速さが 127（＝即到達）なら byte31、そうでなければ byte30
//   行き先     = byte31
//   セント = 段（byte21）で目盛りを変えた (高さ - 64) × 75
//   レジスタ = 0x4000 | (表 0x1E6D14[セント] & 0x3fff)
//
// Violin(40) は byte30=18・byte21=0 → (18-64)×75>>2 = -862 セント → -183。
// 実機の `0x10` は `7f49`（= 0x4000 | (-183 & 0x3fff)）でぴったり合った。
// Trumpet・BrssSec・MuteTrp・Piccolo・PanFlute・Clarinet・Recorder でも合う
constexpr u32 PEG_RATE_TAB  = 0x1E6C94;   // 速さの目盛り → レジスタ（64 語）
constexpr u32 CENT_PITCH_TAB = 0x1E6D14;  // セント → 音程の目盛り（0-4800）

// 16bit に切り詰める（実機の EXTS.W）
inline int s16v(int v) { return int(s16(u16(v))); }

// セント → 音程の目盛り（1 オクターブ = 256）。実機の `0x12B860`
// **表は 0-4800 セントしか無い**。それを超えるぶんは
// オクターブ（1200 セント = 256）で数えて、余りだけ表を引く（6.185）。
// 頭打ちにしていたので、Rain の強さ 127 だけ `0x10` が 32 低かった
inline int cents_to_pitch(const u8 *rom, int cents)
{
	if (!rom || !cents)
		return 0;
	const bool neg = cents < 0;
	const int c = neg ? -cents : cents;
	const int v = c <= 4800
	            ? int(rd16s(rom, CENT_PITCH_TAB + u32(c) * 2))
	            : (c / 1200) * 256
	              + int(rd16s(rom, CENT_PITCH_TAB + u32(c % 1200) * 2));
	return neg ? -v : v;
}

// 高さに掛かる**強さの効き**（実機の `0x12BD40`）。byte22 が 64 なら 0
inline int peg_vel_depth(const u8 *elem, int vel)
{
	const int d = int(elem[22]) - 64;
	if (!d)
		return 0;
	const int x = 36 * (d > 0 ? d : -d);
	const int m = d > 0 ? (0x80 - (vel & 0x7f)) : (vel & 0x7f);
	return int((u32(s16v(x * m)) * 2 & 0xffff) >> 8);
}

// 高さ（0-127、64 が中央）→ セント。実機の `0x12BBAE`。
// byte21 が目盛り: 0 なら 18.75・1 なら 37.5・2 なら 75・3 なら 150 セント刻み
inline int peg_cents(const u8 *elem, int level, int vel)
{
	const int d = level - 64;
	if (!d)
		return 0;
	const int k = peg_vel_depth(elem, vel);
	int m;
	if (d > 0) {
		m = d + 1;                                   // 実機は正の側だけ 1 を足す
		m -= int((u32(s16v(m * k)) & 0xffff) >> 8);
	} else {
		m = d + int((u32(-s16v(d * k)) & 0xffff) >> 8);
		m = -m;
	}
	int v = s16v(m * 75);
	switch (elem[21]) {
	case 0: v = s16v(v) >> 1; v = s16v(v) >> 1; break;
	case 1: v = s16v(v) >> 1; break;
	case 3: v = s16v(v) << 1; break;
	default: break;
	}
	return d > 0 ? v : -v;
}

// **パートの初めの高さ**（08 pp 69）のずらし。要素の高さ（0-127）に足すのではなく、
// **目盛りを ±半オクターブ（byte21 = 1）に固定して別に音程へ直し、足す**。
// SquareLd（素の高さ 0 ＝ -256）で +63 が +256、GrandPno（素 64）で +63 が +512 と、
// どちらも実機の書いた 0x10 に一致（6.214）
inline int part_peg_cents(int value)
{
	if (value == 64)
		return 0;
	u8 unit[84] = {};
	unit[21] = 1;                 // 目盛り: ±半オクターブ
	unit[22] = 64;                // 強さは効かせない
	return peg_cents(unit, value, 100);
}

// 速さの**鍵追従**（実機の `0x12BC72`）。byte24 が深さ、byte25 が折れ点。
// SquareLd（byte24=62・byte25=60）は鍵 60 で 63（即到達）、鍵 72 で 61、
// 鍵 84 で 60 になり、実機とぴったり合った
inline int peg_rate_key_adj(const u8 *elem, int note)
{
	const int d = int(elem[24]) - 64;
	if (!d)
		return 0;
	return s16v((note - int(elem[25])) * (d * 16)) >> 8;   // 算術シフト（下へ丸める）
}

// 速さの**強さ追従**（実機の `0x12BCB2`）。byte23 が深さ
inline int peg_rate_vel_adj(const u8 *elem, int vel)
{
	const int d = int(elem[23]) - 64;
	if (!d)
		return 0;
	const int a = d * 16;
	const int m = a >= 0 ? a * (vel & 0x7f) : (0x80 - (vel & 0x7f)) * (-a);
	return int((u32(s16v(m)) & 0xffff) >> 8);
}

// 速さのレジスタ（`0x0b` の上位バイト）。実機の `0x12BCF0`。
// 途中で何度も符号つき 1 バイトに切り詰めている
// 段ごとの生の速さ（段 0 は byte26、段 1 は byte27、段 2 は byte28、離しは byte29）
inline int peg_rate_raw(const u8 *elem, int stage)
{
	return int(elem[26 + (stage < 0 ? 0 : (stage > 3 ? 3 : stage))]);
}

// 段ごとの行き先の高さ（段 0 は byte31、段 1 は byte32、段 2 は byte33、離しは byte34）
inline int peg_level_of(const u8 *elem, int stage)
{
	return int(elem[31 + (stage < 0 ? 0 : (stage > 3 ? 3 : stage))]);
}

// **立ち上がりのつまみ（CC73 / 08 pp 1B）も、パートの速さとまったく同じだけ
// 目盛りを動かす**（6.171）。GrandPno・Strings1・NylonGt を 128 段測って
//   目盛り = 素 + ((64 - つまみ) >> 2)       （算術シフト。下へ丸める）
// だった。つまみ 65-68 で -1、69-72 で -2 …… と 4 段ごとに 1 目盛り。
// 下げる側も同じ式で、NylonGt（素の目盛り 54）は つまみ 48 で 58、
// つまみ 32 で 62 と、4 段ごとに 1 目盛りずつ遅くなる
inline int peg_rate_idx_of(const u8 *elem, int raw, int note, int vel, int part_rate = 64,
                           int cc_atk = 64)
{
	int r = raw + (int(s8(u8(64 - part_rate))) >> 2)
	            + (int(s8(u8(64 - cc_atk))) >> 2);
	if (s8(u8(r)) > 63) r = 63;
	if (s8(u8(r)) < 0)  r = 0;
	r += peg_rate_key_adj(elem, note);
	if (s8(u8(r)) < 0)  r = 0;
	if (s8(u8(r)) >= 63)
		return 63;
	r += peg_rate_vel_adj(elem, vel);
	if (s8(u8(r)) > 62) r = 62;
	return r;
}

inline int peg_rate_idx(const u8 *elem, int note, int vel, int part_rate = 64,
                        int cc_atk = 64)
{
	return peg_rate_idx_of(elem, int(elem[26]), note, vel, part_rate, cc_atk);
}

inline int peg_rate_reg(const u8 *rom, const u8 *elem, int note = 60, int vel = 100,
                        int part_rate = 64, int cc_atk = 64)
{
	return rd16s(rom, PEG_RATE_TAB
	                  + u32(peg_rate_idx(elem, note, vel, part_rate, cc_atk)) * 2);
}

// 段 stage の速さのレジスタ
inline int peg_rate_reg_stage(const u8 *rom, const u8 *elem, int stage, int note, int vel,
                              int part_rate = 64, int cc_atk = 64)
{
	const int i = peg_rate_idx_of(elem, peg_rate_raw(elem, stage), note, vel,
	                              part_rate, cc_atk);
	return rd16s(rom, PEG_RATE_TAB + u32(i) * 2);
}

// 速さの素を直に渡す版（6.205。離しの段で使う）
inline int peg_rate_reg_raw(const u8 *rom, const u8 *elem, int raw, int note, int vel,
                            int part_rate = 64, int cc_atk = 64)
{
	const int i = peg_rate_idx_of(elem, raw, note, vel, part_rate, cc_atk);
	return rd16s(rom, PEG_RATE_TAB + u32(i) * 2);
}

// 離しの速さの頭打ちの表（パートの塊 +0x65 が 64 より上のとき）
constexpr u32 PEG_REL_TAB = 0x1E54E4;

// ---- **遅れて掛かるビブラート**（6.175）
//
// 弦・木管・金管は、鍵を押してすぐには揺れない。実機は 20ms ごとに回る
// 仕事の中で、音ごとのカウンタを「遅れのぶん待ってから、1 歩ずつ」
// 上げていき、その値を表で引いて `0x0a` の下位に書き直す。
//
//   遅れ（20ms の目盛り） = byte12 ? 3 × byte12 / 4 + 3 : 0
//   止まる所              = byte14
//   1 歩                  = byte13 ? max(1, byte14 / byte13 - 1) : max(1, byte14)
//   レジスタ              = 表C[ 表B[カウンタ] ]
//
// 実機の `0x127CBC`（遅れの式）・`0x129940`（1 歩進める）・`0x129E04`
// （表引き）から起こして、GM の揺れを持つ 41 音色で確かめた
// （遅れ 21/23・止まる所 20/23・1 歩 18/18 が一致。外れるのは
// AltoSax・Oboe・Piccolo の 3 つだけで、そこはまだ分からない）。
//
// **`byte14 × 3` は近道だった**。実機は表引きで、byte14 が 5 以上だと
// 1 ずれる（6 のとき 18 ではなく 17）。Violin がそれ
constexpr u32 VIB_CAP_TAB = 0x1E6370;   // パートの深さ → 頭打ち
constexpr u32 VIB_CNT_TAB = 0x1E63F0;   // カウンタ → 目盛り（＝カウンタ × 2）
constexpr u32 VIB_REG_TAB = 0x1E6596;   // 目盛り → レジスタ
constexpr u32 VIB_TICK    = 882;        // 20ms

// **つまみの割り当て「LFO の音程」**（6.198。実機 0x129D62）。
// つまみごとの `値 × 深さ / 128` を足して 127 で頭打ちし、
// XG モードなら表 `0x1E62F0`（それ以外は VIB_CAP_TAB）を引いてから
// VIB_REG_TAB でレジスタの値にする。
//
// **既定の深さ（10）なら、上の 10 段の表と完全に同じ**になる
//（CC1 = 13→09、26→17、64→43、100→60、127→84 を確かめた）。
// 深さ 64・CC1 = 100 では 0xad で、実機と 1 ビットも違わない
constexpr u32 PMOD_CAP_XG = 0x1E62F0;

// **つまみの割り当て「LFO の音量」**（6.199。実機 0x129B34）。
// つまみごとの `値 × 深さ / 1024` を足して **15 で頭打ち**し、
// **byte16 を下駄**にしてから 2 倍する。つまみが全部既定なら
// `byte16 × 2` になって、これまでの `vib_amp_depth` と同じ
inline int amod_reg(int sum, int base)
{
	int n = sum < 0 ? 0 : (sum > 15 ? 15 : sum);
	if (n < base)
		n = base;
	return (n * 2) & 0x7f;
}

inline int pmod_reg(const u8 *rom, int sum, bool xg = true)
{
	const int s = sum < 0 ? 0 : (sum > 127 ? 127 : sum);
	const u32 tab = xg ? PMOD_CAP_XG : VIB_CAP_TAB;
	return int(rom[VIB_REG_TAB + u32(rom[tab + u32(s)])]);
}

// **LFO はフィルタの切る高さも揺らす**（6.189）。音程と音量の LFO は
// チップが持っているが、**フィルタぶんは firmware が自前で勘定している**
// （ボイスの塊 +76 が 15bit の位相、+78 が歩幅、+106 が深さ、+80 が
// 切る高さへ足すぶん）。10ms ごとに
//
//   足すぶん = 三角(位相) x 深さ / 512      （位相を進めるのは**そのあと**）
//
// Violin（byte15 = 2）で実機と 1 ビットも違わないことを確かめた
constexpr u32 LFO_STEP_TAB = 0x1E6516;  // 速さ 0-63 → 10ms あたりの歩幅
constexpr u32 LFO_FDEP_TAB = 0x1E62E0;  // byte15 → 深さ

inline u16 lfo_step(const u8 *rom, int rate)
{ return rd16(rom, LFO_STEP_TAB + u32(rate & 0x3f) * 2); }

// 深さ。**byte15 を索引にして表を引くだけ**（実機 0x129C94）。
// Violin(2)→4・Viola(1)→2・TubulBel(2)→4 が実機と一致。
// byte13 ではないことは AltoSax（byte13=3・byte15=0）で確かめた
// （揺れがまったく無かった）
// **つまみのぶんが索引に乗る**（6.191。実機 0x129C56）。
// つまみの合計を 22 で頭打ちしてから
//
//   0 → 0、1 → 4、それ以上 → ((n + 1) >> 1) + 4
//
// に折りたたみ、**byte15 を下駄にする**。つまみが全部既定なら
// 索引は byte15 そのものになる
inline int lfo_fdep_index(const u8 *elem, int extra)
{
	int n = extra < 0 ? 0 : (extra > 22 ? 22 : extra);
	n = n == 0 ? 0 : (n == 1 ? 4 : ((n + 1) >> 1) + 4);
	const int b15 = int(elem[15] & 0x7f);
	return n < b15 ? b15 : n;
}

inline int lfo_fdepth(const u8 *rom, const u8 *elem, int extra = 0)
{ return int(rom[LFO_FDEP_TAB + u32(lfo_fdep_index(elem, extra) & 0x7f)]); }

// 位相（15bit）→ 波（-0x2000 〜 +0x2000）。実機 0x12A10C。
// 型 0（byte9 が 0）はのこぎりを半分にしたもの
inline int lfo_fwave(u32 phase, bool tri)
{
	const int p = int(phase & 0x7fff);
	if (!tri)
		return (p <= 0x3fff ? p : p - 0x8000) >> 1;
	if (p <= 0x1fff)
		return p;
	if (p <= 0x5fff)
		return 0x4000 - p;
	return p - 0x8000;
}

// 切る高さへ足すぶん。**負のときは絶対値で掛けてから符号を戻す**
inline int lfo_fcut(int wave, int depth)
{
	if (wave >= 0)
		return int((u32(wave) * u32(depth)) >> 9);
	return -int((u32(-wave) * u32(depth)) >> 9);
}

inline u32 lfo_next(u32 phase, u16 step)
{
	const u32 p = (phase + step) & 0xffff;
	return p >= 0x8000 ? p - 0x8000 : p;
}

inline int vib_ramp_reg(const u8 *rom, int counter)
{
	const int c = counter < 0 ? 0 : (counter > 63 ? 63 : counter);
	return int(rom[VIB_REG_TAB + u32(rom[VIB_CNT_TAB + u32(c)])]);
}

// **遅れて掛かるビブラートの、せり上がりの途中の深さ**（レジスタ 0x0a の下位 8bit。doc の 6.217）。
// 遅れが明けてから 20ms ごとに 2 本を進め、大きいほうが効く（目盛り = 下位 7bit、bit7 なら 8 倍 で比べる）:
//   音色のぶん  = 表[c1]。c1 は 0 から音色の刻み（vib_ramp_step）で目標（byte14）まで。
//                 Vib Depth（dpt）が 64 より下なら、1 段ごとに 14 目盛り引く
//   Depth のぶん = 表[c2] を VIB_DEPTH_TAB[dpt] で止めたもの。c2 は 0 から 5 ずつ（表の 63 より先も引く）
// 表[c] = VIB_REG_TAB[VIB_CNT_TAB[c]]。firmware の 0x0a と 20ms ごとに、Depth 64 以下と 69 以上で一致する。
// 65-68 は行き着く値が合い、出だし 100ms ほどの上がり方だけが違う
inline int vib_ramp_value(const u8 *rom, int dpt, int c1, int c2)
{
	auto units = [](int d) { return (d & 0x80) ? (d & 0x7f) * 8 : (d & 0x7f); };
	auto table = [&](int c) {
		const int k = c < 0 ? 0 : (c > 127 ? 127 : c);
		return int(rom[VIB_REG_TAB + u32(rom[VIB_CNT_TAB + u32(k)])]);
	};
	if (dpt < 0)
		dpt = 64;                      // つまみに触れていない
	if (dpt > 127)
		dpt = 127;
	int own = table(c1);
	if (dpt < 64) {
		const int u = units(own) - (64 - dpt) * 14;
		own = u <= 0 ? 0 : (u < 128 ? u : (0x80 | (u / 8 > 127 ? 127 : u / 8)));
	}
	const int ptop = VIB_DEPTH_TAB[dpt];
	int pv = 0;
	if (ptop) {
		pv = table(c2);
		if (units(pv) >= units(ptop))
			pv = ptop;
	}
	return units(pv) > units(own) ? pv : own;
}

// Depth のぶんのカウンタが止まる所（これより先は表を引いても同じ）
constexpr int VIB_PART_CNT_END = 127;

inline int vib_delay_ticks(const u8 *elem)
{
	return elem[12] ? (3 * int(elem[12])) / 4 + 3 : 0;
}

inline int vib_ramp_target(const u8 *elem) { return int(elem[14]); }

inline int vib_ramp_step(const u8 *elem)
{
	const int t = int(elem[14]);
	if (!elem[13])
		return t > 1 ? t : 1;
	const int v = t / int(elem[13]) - 1;
	return v > 1 ? v : 1;
}

// **音量側の揺れ**（`0x05` の下位）も同じ遅れで待つ。
// GM で遅れを持つ音色は byte16 がどれも 1（深さ 2）なので、
// せり上がるのか一っ飛びなのかは分からない。測れた範囲では
// 遅れが明けた瞬間に 0 から byte16 × 2 へ一つ飛ぶ
inline int vib_amp_depth(const u8 *elem) { return (int(elem[16]) * 2) & 0x7f; }

// ---- **パートの EQ**（レジスタ `0x20`-`0x2a` の偶数番。6.181）
//
// 実機（`0x12C10E`）は表を 2 つ引くだけ。低域・高域それぞれ
// 3 ワード連続で取って、`0x20`/`0x22`/`0x24` と `0x26`/`0x28`/`0x2a` へ入れる。
//
//   低域の索引 = 96 × (周波数 - 4)  + 3 × (ゲイン >> 2)
//   高域の索引 = 96 × (周波数 - 28) + 3 × (ゲイン >> 2)
//
// XG の番地は 08 pp 72 が低域のゲイン、73 が高域のゲイン、
// **76 が低域の周波数、77 が高域の周波数**（74、75 は効かない）。
// 既定はゲイン 64・低域 12・高域 54 で、XG の仕様と合う
constexpr u32 EQ_LOW_TAB  = 0x1EDD98;
constexpr u32 EQ_HIGH_TAB = 0x1F0298;

inline u32 eq_index(int freq, int gain, int lo, int hi)
{
	const int f = freq < lo ? lo : (freq > hi ? hi : freq);
	const int g = gain < 0 ? 0 : (gain > 127 ? 127 : gain);
	return u32(96 * (f - lo) + 3 * (g >> 2));
}

// 6 つの係数を入れる。レジスタは `0x20` から 1 つ飛ばし
inline void eq_set(const u8 *rom, slot_regs &r, int lo_gain, int hi_gain,
                   int lo_freq, int hi_freq)
{
	if (!rom)
		return;
	const u32 li = eq_index(lo_freq, lo_gain, 4, 40);
	const u32 hi = eq_index(hi_freq, hi_gain, 28, 58);
	for (u32 i = 0; i < 3; i++) {
		r.set(0x20 + i * 2, rd16(rom, EQ_LOW_TAB  + (li + i) * 2));
		r.set(0x26 + i * 2, rd16(rom, EQ_HIGH_TAB + (hi + i) * 2));
	}
}

// その音がせり上がりを持つか（持たないものは押した瞬間の値のまま）
inline bool vib_ramps(const u8 *elem)
{
	return elem[9] < 2 && (elem[14] || elem[16]) && (elem[12] || elem[13]);
}

// `SMU2000_NO_PEG` を立てると音程の包絡線をやめる（比べるための逃げ道）
inline bool peg_on()
{
	static const bool on = std::getenv("SMU2000_NO_PEG") == nullptr;
	return on;
}

// `0x10` に書く値（セントを渡す）。**ビット 14 は byte10 で決まる**
// （実機の `0x12AD2E`。byte10 が 0 の音色は立てない。PanFlute・BirdTweet）
inline u16 peg_reg(const u8 *rom, int cents, const u8 *elem = nullptr)
{
	const u16 flag = (!elem || elem[10]) ? 0x4000 : 0;
	if (!peg_on())
		return flag;
	return u16(flag | (u16(cents_to_pitch(rom, cents)) & 0x3fff));
}

// ---- **フィルタの包絡線**（doc/native-engine.md の 6.63）
//
// 実機は firmware のソフトでこれを動かしていて、10ms ごとに
// 切る高さへ足す値を作り直す。折れ線で、状態は 3 つ:
//
//   累算  段の中でいまどこまで来たか（`[音+66]`）
//   目標  その段の行き先（`[音+68]`）
//   増分  1 段あたりの足し引き（`[音+70]`。0x8000 なら「すぐ次の段」）
//
// 10ms ごとに 累算 += 増分 して、向きに応じて目標を越えたら次の段へ。
// 切る高さに足す値は **累算 >> 2**。
//
// 段は要素のバイトで決まる（要素 + 2 を基準に読んでいるので、ここでは
// 要素そのものの番号で書く）:
//
//   はじめの累算 = 目標(byte55)
//   段 1: 目標 = 目標(byte56)、速さ = byte51
//   段 2: 目標 = 目標(byte57)、速さ = byte52
//
// Kitayama（0,72,5）鍵 60・強さ 100 の実機の値で全部合わせた（6.63）
constexpr u32 FENV_INC_TAB = 0x1E5C58;   // 速さ → 増分（16bit 符号つき × 64）

// 0 の側へ丸める >>8（実機は符号で分けている）
inline int sh8(int v) { return v >= 0 ? (v >> 8) : -((-v) >> 8); }

// **包絡線の深さ**（実機の `[音+93]`。`0x128ADC`）。
// 強さの表を byte8 で選び、byte46 の深さと掛け合わせる。
//   深さ = ((36 × (byte46 - 64)) × (0x80 - 表[強さ]) × 2) >> 8
// 表は byte8 が 0 なら 0x1E5D58、そうでなければ 0x1E5DD8。
// GrandPno（byte46=70・byte8=1・強さ 100）で 111、
// Kitayama（byte46=71・byte8=0）で 72。どちらも実機の値と一致した。
// **パートの塊 +210 が 0 でないときの枝はまだ起こしていない**
// （そこは深さがもう一段変わる。既定の音色では 0）
constexpr u32 FENV_VEL_TAB0 = 0x1E5D58;
constexpr u32 FENV_VEL_TAB1 = 0x1E5DD8;

inline int fenv_depth(const u8 *rom, const u8 *elem, int vel)
{
	if (!rom || !elem)
		return 0;
	const int d = int(elem[46]) - 64;
	if (d < 0)
		return 0;                    // 負の枝はまだ起こしていない
	const u32 tab = elem[8] ? FENV_VEL_TAB1 : FENV_VEL_TAB0;
	const int t = rom[tab + u32(vel & 0x7f)];
	const int v = (36 * d) * (0x80 - t);
	return int((u32(v) * 2 & 0xffff) >> 8);
}

// レベルのバイト → 目標
inline int fenv_target(const u8 *rom, const u8 *elem, int level, int vel)
{
	const int x = (level - 64) * 2;
	return (x - sh8(x * fenv_depth(rom, elem, vel))) * 64;
}

// 速さへの足し込み。鍵のぶん（byte48 が深さ・byte49 が基準鍵）と
// 強さのぶん（byte47 が深さ）
inline int fenv_key_adj(const u8 *elem, int note)
{
	const int d = int(elem[48]) - 64;
	return d ? sh8((note - int(elem[49])) * (d * 16)) : 0;
}
inline int fenv_vel_adj(const u8 *elem, int vel)
{
	const int d = int(elem[47]) - 64;
	if (!d)
		return 0;
	const int a = d * 16;
	return sh8(a >= 0 ? a * vel : -((-a) * (0x80 - vel)));
}

// 速さ → 増分。63 以上は「すぐ次の段」の印
constexpr int FENV_NEXT = 0x8000;
inline int fenv_inc(const u8 *rom, int rate)
{
	if (rate >= 63)
		return FENV_NEXT;
	if (rate < 0)
		rate = 0;
	return rd16s(rom, FENV_INC_TAB + u32(rate) * 2);
}

// 音色ごとの下駄。firmware は「音色の音量 → 表」と、鍵ごとの足し込みで作る。
// 式そのものはまだ解けていないので、**1 回だけ実機に鳴らしてもらって校正する**（下）。
// 校正しないときの当て値（実測の中央値。5〜19 の幅がある）
constexpr int VOICE_ATT_TYPICAL = 12;

constexpr u32 LEVEL_CURVE = 0x23CED0;   // 鍵による音量の曲線（128 バイトの行が並ぶ）

// **鍵による切る高さのずれ**（実機の 0x12C1E4 → 0x12C20E）。
// 音量の鍵曲線とまったく同じ仕掛けで、記録の byte38 が 0xFF なら
// ROM の曲線表（LEVEL_CURVE）を byte44,byte45 が指す行で引き、
// **その符号つきの値を 32 倍**して 12bit の切る高さに足す。
// 32 倍なので、鍵を上げ下げすると 32 きざみの階段になる（実測と一致）。
// 実測（`nativeplay --keycut`）と Strngs2・GrandPno・DrawOrg で
// 差が完全に一定になった（doc/native-engine.md の 6.56）
inline int cutoff_key_curve(const u8 *rom, const u8 *elem, int note)
{
	if (!rom || !elem || elem[38] != 0xff)
		return 0;        // 折れ線の枝はまだ起こしていない
	const u32 row = (u32(elem[44]) << 8 | elem[45]) * 128;
	const u32 a = LEVEL_CURVE + row + u32(note & 0x7f);
	return s8(rom[a]) * 32;
}


// 音量の鍵による増減。記録の byte60 が 0xFF のときは ROM の曲線表を引く
// （byte66,byte67 が行の番号）。符号付きで、鍵ごとに ±10 ほど動く
inline int level_key_curve(const u8 *rom, const u8 *elem, int note)
{
	if (elem[60] != 0xff)
		return 0;                       // 折れ線の形はまだ入れていない
	const u32 idx = u32(elem[66]) << 8 | elem[67];
	const u32 a = LEVEL_CURVE + idx * 128 + u32(note & 0x7f);
	if (a >= 0x400000)
		return 0;
	return int(s8(rom[a]));
}

// 減衰 → 音量の目盛り（表を逆に引く）。同じ減衰になる目盛りが 3-4 段
// 並ぶので、**いちばん上（音量が大きい側）**を返す。真ん中を返していた
// ときは、鍵の曲線を足したあとで表の段を 1 つ踏み外していた
// （Strings の鍵 48 が 2 段ぶん静かになっていた）
inline int level_from_att(const u8 *rom, int att)
{
	int lo = -1, hi = -1;
	for (int i = 0; i < 128; i++)
		if (rom[LEVEL_TAB + 0x80 + i] == att) {
			if (lo < 0) lo = i;
			hi = i;
		}
	return lo < 0 ? 64 : hi;
}

// **校正**: 1 回だけ実機（firmware）に鳴らしてもらった減衰から、その音色の
// 「素の音量」を出す。これがあれば、ほかの鍵・強さの減衰は式で出せる
inline int wave_level(const u8 *rom, const u8 *elem, int note)
{
	const u8 *we = wave_entry(rom, wave_set(elem), wave_note(rom, elem, note));
	return we ? int(we[0]) : 0;
}

// 鍵の曲線が音量の目盛りに効く倍率は **2 倍**。実機（`0x12C1D8`）は表を
// 引いた値を 1 ビット左へ寄せ、**符号付き 1 バイト**にして持つ（`setup[8]`)。
// それを目盛りに足す（`0x12AC0E`）。
//
// 前は 6/4 = 1.5 倍にしていた。当時は写し取った減衰から目盛りを逆に引いて
// いたので、表の段の幅にずれが埋もれて 1.5 倍がいちばん「マシ」に見えた。
// 実機の目盛り（ボイスの塊 +118。6.101）を直に読めるようにしたら、Bottle の
// 鍵 36/42/48/60 が 1/19/39/65 で、曲線の差 -32/-23/-13/0 のちょうど 2 倍と
// 分かった（1.5 倍だと鍵 36 で 40 段ぶん明るすぎた ＝ 35dB 違っていた）
inline int level_curve_scaled(const u8 *rom, const u8 *elem, int note)
{
	const int v = level_key_curve(rom, elem, note) * 2;
	return int(s8(u8(v)));                  // 実機は 1 バイトに詰めて持つ
}

// 写し取ったときのつまみの位置（既定のパート: 音量 100・エクスプレッション 127）
constexpr int VOL_GAIN_DEF = ((100 + 1) * (127 + 1)) >> 7;      // = 101

// **実機のボイスの塊**。0x94 バイトずつ並んでいて、番号はスロットの番号と
// 同じ（和音を鳴らして +32 の読み先を見た: 424384 / 424418 / 4244AC）。
// +118 が「掛ける前の音量の目盛り」（0x12AC2C が書く。6.101）
constexpr u32 VBLK_BASE   = 0x424364;
constexpr u32 VBLK_STRIDE = 0x94;
constexpr u32 VBLK_LEVEL  = 118;

inline int fw_voice_level(const u8 *ram, int slot)
{
	if (!ram || slot < 0 || slot >= 64)
		return -1;
	const u32 a = VBLK_BASE + u32(slot) * VBLK_STRIDE + VBLK_LEVEL - 0x400000;
	const int v = int(ram[a]);
	return (v >= 1 && v <= 128) ? v : -1;
}

inline int calibrate_level(const u8 *rom, const u8 *elem, int att_ref, int note_ref,
                           int vel_ref, int gain_ref = VOL_GAIN_DEF)
{
	const int rest = att_ref / 2 - velocity_att(rom, vel_shift(elem, vel_ref), vel_curve_of(elem))
	               - wave_level(rom, elem, note_ref);
	// **つまみのぶんを割り戻す**。base_level が持つのは「掛ける前の目盛り」で、
	// 鳴らすときに `level_with_gain` でそのときの音量を掛け直す（6.101）。
	//
	// 表は同じ減衰が 3-4 段つづくので、逆に引くと目盛りは**幅**でしか分から
	// ない。掛ける前の目盛りに直すと幅はさらに広がるので、その**真ん中**を
	// 取る（端を取ると、音量を上げ下げしたときに片側へ 1 段ずれる）
	int lo = -1, hi = -1;
	for (int i = 0; i < 128; i++)
		if (int(rom[LEVEL_TAB + 0x80 + i]) == rest) {
			if (lo < 0) lo = i;
			hi = i;
		}
	int l;
	if (lo < 0) {
		l = level_without_gain(64, gain_ref);
	} else {
		const int g = gain_ref <= 0 ? 1 : (gain_ref > 128 ? 128 : gain_ref);
		const int alo = (lo * 128 + g - 1) / g;
		int ahi = ((hi + 1) * 128 - 1) / g;
		if (ahi > 128) ahi = 128;
		l = ahi < alo ? alo : (alo + ahi) / 2;
	}
	return l - level_curve_scaled(rom, elem, note_ref);
}

// 実機のボイスの塊から取った目盛りを base_level に直す（逆引きが要らない道）
inline int base_level_from_fw(const u8 *rom, const u8 *elem, int fw_level, int note_ref)
{
	return fw_level - level_curve_scaled(rom, elem, note_ref);
}

// **掛ける前の音量の目盛りは ROM から出せる**（6.113）。実機（`0x12ABE0`）は
//
//   目盛り = clamp((音色の記録[1] * 要素[59]) / 99 + 鍵の曲線 * 2, 0, 128)
//
// （firmware の番号では要素[57]。こちらの要素の番号は実機より 2 大きい）。
// Bottle 65・PickBass 106・GrandPno 108・Strings1 97・Flute 97 が、実機の
// ボイスの塊 +118 とそのまま一致する。
//
// **写し取りで逆に引くのをやめた理由**: 目盛りは 0-128 で頭打ちになるので、
// 張り付く鍵（PickBass の鍵 36 など）で写し取ると本当の値が取れない。
// ROM から出せば、どの鍵で写し取っても同じ答えになる
inline int voice_raw_level(const u8 *rom, u32 rec, const u8 *elem)
{
	if (!rom || !rec)
		return 64;
	// **切り捨て**。前は四捨五入（+49）にしていたが、それは減衰の表から
	// 逆に引いた値で当てていたので外していた。実機のボイスの塊 +118 を
	// 直に読むと（`tools/native/levelprobe.py`）、GM の 128 音色 112 件で
	// **切り捨てが 111 件合い、四捨五入は 61 件しか合わない**（6.158）
	return int(rom[rec + 1]) * int(elem[59]) / 99;
}

// 掛ける前の音量の目盛り（鍵の曲線まで入れたもの）。
// `adj` は写し取りで見つかったずれ（普通は 0）
inline int volume_level(const u8 *rom, u32 rec, const u8 *elem, int note, int adj = 0)
{
	int l = voice_raw_level(rom, rec, elem) + adj + level_curve_scaled(rom, elem, note);
	if (l < 0) l = 0;
	return l > 128 ? 128 : l;
}

// 目盛りに乗らない側の減衰（強さと、波形の段ぶん）。
// 波形の記録の先頭のバイトが、その段ぶんの減衰。多段サンプルの音色では
// 段の変わり目で 1.5dB ほど動くので、これを入れないと段ごとにずれる
inline int volume_rest(const u8 *rom, const u8 *elem, int note, int vel)
{
	return velocity_att(rom, vel_shift(elem, vel), vel_curve_of(elem))
	     + wave_level(rom, elem, note);
}

// 目盛り・残り・そのときの音量から、0x09 に入れる減衰。
// 実機（`0x12A538`-`0x12A55A`）は **127 で頭打ちにしてから 2 倍**する
// `add` は**つまみの割り当ての音量**（6.200）。実機は音量を掛けた**あと**の
// 目盛りの索引に足して 0-128 で頭打ちにする（音量の側ではない）
inline int volume_att_from(const u8 *rom, int level, int rest, int gain,
                           int add = 0)
{
	int l = level_with_gain(level, gain);
	if (add) {
		l += add;
		if (l < 0) l = 0;
		if (l > 128) l = 128;
	}
	int a = int(rom[LEVEL_TAB + 0x80 + u32(l)]) + rest;
	if (a > 127) a = 127;
	if (a < 0) a = 0;
	return a * 2;
}

// 校正した素の音量から、その鍵・強さの減衰（0x09 に入れる値）
inline int volume_att(const u8 *rom, u32 rec, const u8 *elem, int note, int vel,
                      int gain = VOL_GAIN_DEF, int adj = 0)
{
	return volume_att_from(rom, volume_level(rom, rec, elem, note, adj),
	                       volume_rest(rom, elem, note, vel), gain);
}

// 減衰・離しの速さに乗る、鍵による補正（firmware の 0x12ADD0）
inline int rate_key_corr(const u8 *elem, int note)
{
	int c = (note - int(elem[71])) * (int(elem[70]) - 64) * 16;
	if (c < 0)
		c += 0xff;
	return c >> 8;
}

// 減衰の表の目盛りを 0-127 に収める
inline int clamp_idx(int i) { return i < 0 ? 0 : (i > 127 ? 127 : i); }

inline int rate_scale(int raw, int corr)
{
	int v = raw + corr;
	if (v <= 0) v = 1;
	if (v > 63) v = 63;
	return v * 2;
}

// **減衰 2 だけは下限が 0**（実機の `0x127338`。減衰 1 の `0x1272F4` は 1）。
// 表の頭は 1,1,2,2,… なので、0 と 1 で値が変わる。byte75 が 0 の音色
// （Trumpet・BrssSec・SquareLd）で実機は 1、こちらは 2 になっていた
inline int rate_scale2(int raw, int corr)
{
	int v = raw + corr;
	if (v < 0) v = 0;
	if (v > 63) v = 63;
	return v * 2;
}

// ---- **共振**（レジスタ `0x04`）。実機の `0x12806A` と `0x12810A`
//
//   目減り = (18 × |byte81 - 64| × (byte81>64 ? 0x80-強さ : 強さ)) & 0xffff >> 8
//   値     = max(byte35 - 目減り, 0)
//   パート（+25）の下駄を足して、>>1 して 5bit に収める
//
// 18 音色 × 強さ 30/100/127 の 54 通りで実機と一致した（EPiano1 は強さで
// 要素が切り替わる音色で、鳴っている側の要素で計算すれば合う）
inline int reso_vel_drop(const u8 *elem, int vel)
{
	const int d = int(elem[81]) - 64;
	if (!d)
		return 0;
	const int x = 18 * (d > 0 ? d : -d);
	const int m = d > 0 ? (0x80 - (vel & 0x7f)) : (vel & 0x7f);
	return int((u32(x * m) & 0xffff) >> 8);
}

// **1 ビット落とす前の値**。切る高さの頭打ちはこちらで見る（6.183）
inline int reso_raw(const u8 *elem, int vel, int part_res = 64)
{
	int v = int(elem[35]) - reso_vel_drop(elem, vel);
	if (v < 0)
		v = 0;
	const int p = part_res - 64;
	int r = p >= 0 ? (p >= v ? p : v) : p + v;
	return r < 0 ? 0 : r;
}

inline int reso_level(const u8 *elem, int vel, int part_res = 64)
{
	return (reso_raw(elem, vel, part_res) >> 1) & 31;
}

// 鍵を離すときに 0x09 へ入れる値。
// 上位のビット 15 が「離せ」の印で、残りが離しの速さ（swp30.cpp の release_glo_w）。
// 速さは減衰と同じ表を **byte76** で引き、鍵の補正も同じだけ乗る
// （実機が離すときに書く値と、GrandPno の鍵 60 で一致する: 0xBE1E）
// **離しのつまみ（CC72 / 08 pp 1C）**（6.170）。実機の `0x127384`。
// 下げる側（64 未満）は
//
//   素が 55 より大きければ**動かない**
//   そうでなければ min(55, 素 + (65 - つまみ) / 2)
//
// `tools/native/reltab.py` で Strings1 と GrandPno を 128 段測った値と
// 合う（どちらも素が 55 以下なので、頭打ちは見えていない）。
//
// **上げる側（64 より大きい）**は別の表（`0x1E54E4`）を
//
//   min(素, 表[(つまみ - 64) + ボイスの塊 +112])
//
// で引く。音色ごとに動きはじめる所が違うのは **素の目盛りが
// 違うから**：Strings1 は byte76 = 29 で つまみ 90、GrandPno は 31 で 86。
// どちらも +112 = 3 で説明がつく
constexpr u32 REL_RATE_CC = 0x1E54E4;   // 離しのつまみ（上げる側）の表
constexpr int REL_RATE_OFF = 1;         // ボイスの塊 +112
// **+112 は 1**（6.212）。前は 3 としていたが、実機のワーク RAM を
// 読むと 1 だった（声の塊 +112 そのもの）。3 だとつまみ 88 以上で
// 目盛りが 1 つ分速くなる（Strings も GrandPno も）

// **立ち上がりの表の引き方**（実機の `0x1272DE`）。
// 目盛りを 2 倍する前に **0 は 4 に直す**。
//
// そのあと実機は **126 を 255 に読み替えている**が、
// それを入れると `0x06` が 0x7800 になって実機（0x7700）と違った
// （keylevel・retrig・dense で 57 本）。255 は表の索引ではなく、
// 呼ぶ側が別に見ている印らしいのでここでは 126 のままにする
inline int attack_idx(int rate)
{
	const int r = (rate <= 0 ? 4 : rate) * 2;
	return r > 126 ? 126 : r;
}

inline int rel_rate_cc(const u8 *rom, int base, int cc)
{
	if (cc < 0 || cc == 64)
		return base;
	const int c = cc > 127 ? 127 : cc;
	if (c < 64) {
		if (base > 55)
			return base;            // 素が速ければ下げる側は効かない
		const int v = base + (65 - c) / 2;
		return v > 55 ? 55 : v;
	}
	if (!rom)
		return base;
	const int t = int(rom[REL_RATE_CC + u32(c - 64 + REL_RATE_OFF)]);
	return t < base ? t : base;
}

inline u16 release_reg(const u8 *rom, const u8 *elem, int note, int att,
                       int cc_rel = 64)
{
	const int r = rom[REL_TAB + rate_scale(
	                  rel_rate_cc(rom, int(elem[76]), cc_rel), rate_key_corr(elem, note))];
	return u16(((0x80 | (r & 0x7f)) << 8) | (att & 0xff));
}

// **ダンパーを踏んだとき、離している最中の音に入れる値**（6.164）。
// 実機は「ペダルが拾った」形で、離しをやめて**減衰 2 の速さに戻す**
// （8 音色で、レジスタ `0x08` の上位と 1 ビット違わず同じ値だった）。
// ビット 15 の「離せ」の印は立てたまま
inline u16 damper_hold_reg(const u8 *rom, const u8 *elem, int note, int att,
                           int cc_dec = 64, int adj = 0)
{
	const int corr = rate_key_corr(elem, note);
	const int r = rom[DECAY_TAB + clamp_idx(
	                  rate_scale2(eg_rate_cc_add(int(elem[75]), cc_dec), corr) + adj)];
	return u16(((0x80 | (r & 0x7f)) << 8) | (att & 0xff));
}

// **音色の写し取り**。式が分かっていないレジスタ（フィルタ・素通しの量など）は、
// 起動のときに firmware へ 1 音だけ鳴らしてもらって、そのときの値を覚えておく。
// 鍵や強さで動かないものが多いので、これだけで実機にかなり近くなる。
// 覚えるのは**利用者の ROM から起こした値**で、配らない（起動のたびに作る）
// フィルタの包絡線の 1 段。firmware はこれをソフトで動かして、鳴っている間
// 0x00・0x01・0x04 を 10ms ごとに書き直す（doc/native-engine.md の 6.17）
struct fstep {
	u32 at;            // 鳴らし始めてからのサンプル数（rel なら離してから）
	u8  reg;
	u16 v;
	// **離したあとの段**。実機はフィルタを離しのあいだも動かし続ける。
	// 写し取りの元にした音が短いと、録れる段のほとんどがこちら側になる。
	// 押してからの並びと離してからの並びを分けて持ち、鳴らすときも
	// それぞれの時刻から流す（doc/native-engine.md の 6.57）
	u8  rel = 0;
};

struct voice_cal {
	bool have = false;
	// **合成の写し**（`default_cal`。写し取りをしていない）。つまみの差では
	// なく、その場で式を組み直す目印（doc/native-engine.md の 6.154）
	bool synth = false;
	int  base_level = 64;      // 校正した素の音量
	int  cal_vel = 100;        // 写し取ったときの強さ（強さを変えるときの基準）
	// 写し取ったときの鍵。レジスタ 0x00（切る高さ）は鍵でも動くので、
	// ここからの差ぶんだけずらす（doc/native-engine.md の 6.56）
	int  cal_note = 60;
	// 写し取ったときのコントローラの位置。ここからの差ぶんだけ動かす
	int  cal_vol = 100, cal_expr = 127, cal_pan = 64, cal_mod = 0;
	int  cal_rev = 40, cal_cho = 0;      // 写し取ったときの送り（CC91・CC93）
	int  cal_bri = 64, cal_res = 64;     // 写し取ったときの明るさ・共振（CC74・CC71）
	// **写し取ったときのパートの「経路」**（素通しの量・バリエーション送り・
	// パートの EQ・インサーションの掛かり先）をまとめた印。
	// ここが違うと、写し取った 0x20-0x2b・0x32-0x37 はそのまま使えない
	u32  cal_ctx = 0;
	// **減衰の表の目盛りのずれ**（写し取ったときの実機の値と、こちらの式の差）。
	// 減衰は鍵で変わるので写し取った値をそのまま使えないが、ずれは鍵に
	// よらないとみて、式で出した目盛りにこれを足す。これでパート側の
	// EG の設定（CC75 など）も、こちらの式の小さなずれも一緒に吸収できる。
	// **表の目盛りそのもの**で持つ（実機は奇数の目盛りも使うので、
	// rate_scale の「2 倍」の単位では足りない）
	int  dec_adj[2] = { 0, 0 };
	u16  reg[0x40] = {};       // 基準の鍵・強さでの値
	u64  mask = 0;             // 覚えているレジスタ

	bool has(int r) const { return (mask & (u64(1) << r)) != 0; }
	void set(int r, u16 v) { reg[r] = v; mask |= u64(1) << r; }

	// 写し取った音で、firmware がフィルタをどう動かしたか。
	// あとの音でも同じように動かす（鍵と強さは変わるが、形は近い）
	std::vector<fstep> filter_env;

	// そのスロットが鳴らしていた波形の番地（0x16/0x17）
	u32 wave_addr() const { return u32(reg[0x16]) << 16 | reg[0x17]; }
};

// **写し取りの代わりに置く「既定のつまみでの写し」**（段 4。`SMU2000_NOCAL`）。
//
// 写し取りが持っているのは、突き詰めると 2 つだけになった:
//   * **つまみがどこにあったか**（cal_vol・cal_pan・cal_rev …）。鳴らすときは
//     そこからの**差**でレジスタを動かすので、基準の位置さえ分かればよい
//   * **式で出せない所の値**（0x20-0x2b のパート EQ・0x32-0x37 のミキサ）
//
// `defaults` は「パート 1・音量 100・パン中央・リバーブ送り 40」で測った値
// なので、基準のつまみもそこに合わせれば辻褄が合う。**写し取りを 1 音も
// せずに鳴らせる**ようになる。まだ式で出せない所は defaults のままなので、
// つまみを既定から動かした曲では、そのぶんだけ実機と離れる
inline voice_cal default_cal(const defaults &d = defaults())
{
	voice_cal c;
	c.have = false;                // build_note の丸写しはしない（式を使う）
	c.synth = true;
	c.base_level = 0;              // 目盛りは ROM から出す（6.113）
	c.cal_vel = 100;
	c.cal_note = 60;
	c.cal_vol = 100;
	c.cal_expr = 127;
	c.cal_pan = 64;
	c.cal_mod = 0;
	c.cal_rev = 40;
	c.cal_cho = 0;
	c.cal_bri = 64;
	c.cal_res = 64;
	c.dec_adj[0] = c.dec_adj[1] = 0;
	// つまみの差を乗せる元になる値だけ入れておく（パン・送り・ミキサ）
	for (int i = 0; i < 6; i++)
		c.set(0x32 + i, d.mix[i]);
	return c;
}

// 要素と、写し取ったスロットを**波形の番地で**結び付ける。
// 要素の並びとスロットの並びが同じとは限らないので、順番では当てにならない
// used には「もう使った写し取り」の印を立てる。同じ波形を鳴らす要素が
// 2 つあるとき（重ねの音色ではよくある）、両方が同じ写し取りを掴むと
// 片方の音量が丸ごと違ってしまう
inline const voice_cal *match_cal(const std::vector<voice_cal> &cals, u32 want, u32 *used = nullptr)
{
	for (size_t i = 0; i < cals.size(); i++) {
		if (used && (*used & (u32(1) << i)))
			continue;
		const voice_cal &c = cals[i];
		if (c.has(0x16) && c.has(0x17) && c.wave_addr() == want) {
			if (used)
				*used |= u32(1) << i;
			return &c;
		}
	}
	return nullptr;
}

// **フィルタの包絡線の初めの値**（実機の `0x1288E4`-`0x12895C`）。
// 立ち上がりが最速（byte50 が 63）の音色は**いきなり段 0 の行き先から
// 始まる**。そうでなければ byte54（既定は 64 ＝ ずれ 0）から始めて、
// byte50 の速さで段 0 の行き先へ登る。
// GrandPno（byte50=63）は 0x400、Flute（byte50=62）は 0 で実機と一致した
// **立ち上がりのつまみはフィルタの包絡線の速さにも効く**（6.171）。
// 実機の `0x128A32` をそのまま起こしたもの。つまみ → 目盛りの表が
// `0x1E5CD8` に 128 バイトある。
//
//   つまみ 64        素の目盛りのまま
//   つまみ 65-127    min(素, 表[つまみ])      表[64]=63 … 表[127]=0
//   つまみ 0-63      max(素, 表[つまみ])      表[63]=18 … 表[0]=63
//
// **下げる側は、段 0 の行き先（byte55）が初めの高さ（byte54）より下がる
// 音色では効かない**（実機は `CMP/HS` で振り分けて、つまみを 64 に戻す）。
// 10ms ごとの `0x00` の伸びを増分の表（0x1E5C58）で引き戻して、
// Strings1（素 63）・Reed Organ（素 40）・Bird Tweet（素 5）の 3 つで
// 表と 1 つ残らず一致した（`tools/native/fenvrate.py`）
constexpr u32 FENV_ATK_CC = 0x1E5CD8;   // つまみ → 立ち上がりの目盛り（128 バイト）

// **速さにつまみを掛ける**（6.205。実機 `0x128A5A`）。
// つまみが 64 なら素通し、64 より上なら表で頭打ち、
// 下なら表で底上げする。**段 0・段 2・離しで同じ表**を使う
inline int fenv_rate_cc(const u8 *rom, int base, int cc)
{
	if (!rom || cc < 0)
		return base;
	const int c = cc & 0x7f;
	if (c == 64)
		return base;
	const int t = int(rom[FENV_ATK_CC + u32(c)]);
	return c > 64 ? (t < base ? t : base) : (t > base ? t : base);
}

inline int fenv_atk_rate(const u8 *rom, const u8 *elem, int cc_atk = 64)
{
	const int base = int(elem[50]);
	if (!rom)
		return base;
	int cc = cc_atk & 0x7f;
	if (cc < 64 && int(elem[55]) < int(elem[54]))
		cc = 64;                        // 下がる包絡線には効かない
	return fenv_rate_cc(rom, base, cc);
}

// **すぐ段 0 の行き先まで行くか**。実機は目盛りが 63 以上なら増分に
// 0x8000（＝すぐ次の段）を入れる。**鍵の補正を足したあともう一度見る**
// ので、つまみで 63 を割っていても鍵の補正で戻ることがある
inline bool fenv_atk_instant(const u8 *rom, const u8 *elem, int cc_atk = 64,
                             int note = 60)
{
	const int r = fenv_atk_rate(rom, elem, cc_atk);
	if (r >= 63)
		return true;
	int k = r + fenv_key_adj(elem, note);
	if (k < 0)
		k = 0;
	return k >= 63;
}

// **初めの高さ**。すぐ行き先まで行くなら段 0 の行き先（byte55）、
// そうでなければ byte54 から登る。つまみで 63 を割ると、ここが切り替わる
inline int fenv_start_level(const u8 *elem, bool instant)
{
	return instant ? elem[55] : elem[54];
}

// **ソフトペダル（CC67）はフィルタの包絡線の強さを 32 下げる**（6.182）。
// 実機は踏んでいるあいだ、深さ（ボイスの塊 +93）を「強さ - 32」で作り直す。
// 音量・共振・減衰・立ち上がりはまったく動かない
inline int soft_vel(int vel, bool soft)
{
	if (!soft)
		return vel;
	const int v = vel - 32;
	return v < 0 ? 0 : v;
}

inline int fenv_init(const u8 *rom, const u8 *elem, int vel, int cc_atk = 64,
                     int note = 60)
{
	return fenv_target(rom, elem,
	                   fenv_start_level(elem,
	                                    fenv_atk_instant(rom, elem, cc_atk, note)),
	                   vel);
}

// **鍵を押した瞬間の `0x00`**（実機の `0x12AC98`）。
//   表 0x1E5B58[byte37] ＋ 鍵の追従 を 0-0xFFF に収め、
//   そこへ包絡線の初めの値（>>2）を足して下 11bit を取る
// 包絡線の今の値（facc）を渡すと、そのときの `0x00` を返す。
// 実機は 10ms ごとにこれを書き直している
// 鍵を押した瞬間の `0x00` を**実機と同じ式で出す**。
//
// **2026-09-20 から既定で入**（6.116）。以前は「写し取りの無いスロットは
// フィルタの包絡線が動かないので試し曲が 0.36dB 明るくなる」ので切って
// いたが、そのあとの直し（送り・パン・音量・鍵の曲線・強さの曲線・
// 鍵の追従）で前提が変わり、入れたほうが良くなった:
//
//   `native の口` のいちばん悪い値   切 -0.41dB -> 入 **-0.05dB**
//   SoundTrk の鍵 84                切 -10dB   -> 入 **+0.25dB**
//
// `SMU2000_CUT_EXACT=0` で前の道に戻せる
// **フィルタ側の LFO を切る**（`SMU2000_FLFO=0`。6.189）
inline bool flfo_on()
{
	static const bool on = [] {
		const char *e = std::getenv("SMU2000_FLFO");
		return !e || (e[0] != '0' || e[1]);
	}();
	return on;
}

inline bool cut_exact()
{
	static const bool on = [] {
		const char *e = std::getenv("SMU2000_CUT_EXACT");
		return !e || (e[0] != '0' || e[1]);
	}();
	return on;
}

// `cap` を false にすると、**共振が浅いときの頭打ち（0x7C0）を掛けない**
// 値を返す。明るさのつまみ（CC74）は**頭打ちの前**に効くので、
// つまみを下げる曲では素の値から引かないと 7 ずれる（6.167）
inline u16 cutoff_of(const u8 *rom, const u8 *elem, int note, int vel, int facc,
                     bool cap = true)
{
	int cut = int(rd16(rom, CUTOFF_TAB + u32(elem[37]) * 2))
	        + cutoff_key_curve(rom, elem, note);
	cut = cut < 0 ? 0 : (cut > 0xfff ? 0xfff : cut);
	cut += facc >> 2;
	// **0x800 は下駄**。実機（`0x127E84`）は「0x800 以下なら 1」＝閉じ切りに
	// してから 0xFFF で頭打ちにし、下 11bit を取る。SynBrass1 は表 0x6d4 に
	// 鍵の追従 +96、包絡線 -224 で 0x654 ＝ 下駄より下なので、実機は 1 を書く
	if (cut <= 0x800) cut = 1;
	if (cut > 0xfff) cut = 0xfff;
	cut &= 0x7ff;
	// そのうえで `0x12E79C` が「**共振が 4 未満なら 0x7C0 で頭打ち**」を掛ける
	// （EPiano1 は強さ 100 で共振 0 → 0x7C0、強さ 127 で共振 4 → 0x7FF）
	if (cap && reso_level(elem, vel) < 4 && cut > CUTOFF_MAX)
		cut = CUTOFF_MAX;
	return u16(0x1000 | u16(cut));
}

// `fvel` はフィルタの包絡線の深さだけに使う強さ（ソフトペダルで下がる）。
// 共振の頭打ちは素の強さのまま
inline u16 cutoff_keyon(const u8 *rom, const u8 *elem, int note, int vel,
                        bool cap = true, int cc_atk = 64, int fvel = -1)
{
	return cutoff_of(rom, elem, note, vel,
	                 fenv_init(rom, elem, fvel < 0 ? vel : fvel, cc_atk, note), cap);
}

// 共振が浅ければ頭打ちを掛ける（つまみを効かせたあとに使う）
// **頭打ちの判定にはつまみを入れた共振を使う**（6.183）。
// CC71 を上げると実機は頭打ちを外すのに、素の値で見ていたので
// SquareLead の CC71 = 96 だけ 7 だけ暗かった
inline u16 cutoff_cap(u16 v, const u8 *elem, int vel, int part_res = 64)
{
	int cut = int(v & 0xfff);
	if (reso_level(elem, vel, part_res) < 4 && cut > CUTOFF_MAX)
		cut = CUTOFF_MAX;
	return u16((v & 0xf000) | u16(cut));
}

// ---- **音色そのものが持つパン**（レジスタ `0x32`）。実機の `0x12AF40` と `0x12B794`
//
//   位置 = clamp(CC10 + 表 0x1E68DC[byte69] - 64, 0, 127)
//          （byte69 が 15 のときだけ鍵で 0x1E68EB を引く）
//   左 = 表 0x1E6B90[パート[14]] + 表 0x1E6C11[位置]
//   右 = 表 0x1E6B90[0x80-パート[14]] + 表 0x1E6C11[0x80-位置]
//   レジスタ = (左 << 8) | 右   （どちらも 255 で頭打ち）
//
// Warm Pad は 2 つの要素が byte69=2 と 12 で、表を引くと 13 と 115。
// 実機は片方に `083c`、もう片方に `3c08` を書いていて、式と一致する
constexpr u32 PAN_SEL_TAB   = 0x1E68DC;   // byte69 → パンの位置（16 個）
constexpr u32 PAN_SEL_KEY   = 0x1E68EB;   // byte69 が 15 のとき、鍵で引く
constexpr u32 PAN_BASE_TAB  = 0x1E6B90;   // パートのパン → 下駄（中央で 8 ＝ -3dB）
constexpr u32 PAN_CURVE_TAB = 0x1E6C11;   // パンの位置 → 減衰（0-128）

inline int elem_pan(const u8 *rom, const u8 *elem, int note)
{
	const int i = int(elem[69]);
	return i == 15 ? int(rom[PAN_SEL_KEY + u32(note & 0x7f)])
	               : int(rom[PAN_SEL_TAB + u32(i & 0xf)]);
}

// パンの位置（0-127）
inline int voice_pan_pos(const u8 *rom, const u8 *elem, int note, int cc10 = 64)
{
	const int p = cc10 + elem_pan(rom, elem, note) - 64;
	return p < 0 ? 0 : (p > 127 ? 127 : p);
}

// **送りはパンで目減りする**（実機の `0x12C3F8`）。真ん中で 16 を足し、
// 左右に振るほど減る（表 0x1F2198）。Warm Pad は位置 13 で 4 なので
// 既定の `2b` から 12 減って `1f`。実機と一致した
constexpr u32 PAN_SEND_TAB = 0x1F2198;

inline int pan_send_adj(const u8 *rom, int pan_pos)
{
	return int(rom[PAN_SEND_TAB + u32(pan_pos & 0x7f)])
	     - int(rom[PAN_SEND_TAB + 64]);
}

// **Rnd（パンの値が 0）のときのレジスタ**（doc/native-engine.md の 6.147）。
// 実機は要素を 1 つ鳴らすたびに 8bit の乱数を進めて、その上位 7bit を
// パンの位置にする。**音色が持っているパンの寄りは無視される**（実測）。
// 位置 r に対して 左 = 下駄[r]、右 = 下駄[128-r] そのもの
// Rnd のときに送りが目減りするぶん（表を位置 0 で引くので、写し取った
// ときの位置ぶんがそのまま減る）
inline int pan_send_drop(const u8 *rom, int pan_pos)
{
	return rom ? int(rom[PAN_SEND_TAB + u32(pan_pos & 0x7f)]) : 0;
}

inline u16 pan_rnd_reg(const u8 *rom, int r)
{
	if (!rom)
		return 0x0808;
	const int q = r < 0 ? 0 : (r > 127 ? 127 : r);
	int l = int(rom[PAN_BASE_TAB + u32(q)]);
	int rr = int(rom[PAN_BASE_TAB + u32(0x80 - q)]);
	if (l > 255) l = 255;
	if (rr > 255) rr = 255;
	return u16((l << 8) | rr);
}

// **インサーションを通るときの `0x32`**（6.184）。
// パートのパン（CC10）は**まったく見ない**で、
// 音色（打）自身のパンだけを左に入れ、右は 0
inline u16 ins_pan_reg(const u8 *rom, int pan_pos)
{
	if (!rom)
		return 0;
	const int p = pan_pos < 0 ? 0 : (pan_pos > 127 ? 127 : pan_pos);
	int l = int(rom[PAN_CURVE_TAB + u32(p)]);
	if (l > 255) l = 255;
	return u16(l << 8);
}

inline u16 voice_pan_reg(const u8 *rom, const u8 *elem, int note,
                         int cc10 = 64, int part_pan = 64)
{
	const int p = voice_pan_pos(rom, elem, note, cc10);
	const int q = part_pan & 0x7f;
	int l = int(rom[PAN_BASE_TAB + u32(q)]) + int(rom[PAN_CURVE_TAB + u32(p)]);
	int r = int(rom[PAN_BASE_TAB + u32(0x80 - q)])
	      + int(rom[PAN_CURVE_TAB + u32(0x80 - p)]);
	if (l > 255) l = 255;
	if (r > 255) r = 255;
	return u16((l << 8) | r);
}

// ---- **ドラムの 1 打**（doc/native-engine.md の 6.86・6.87）
//
// ドラムの記録は 42 バイトで、旋律の要素（84 バイト）とは別の並び。
// 波形の記録（16 バイト）が +26 にそのまま埋まっている。
//   +1  音程の微調（セント。64 が中央）   +10 減衰 2 の行き先
//   +11 切る高さ（表の索引）              +12 共振（>> 2）
//   +13 立ち上がりの速さ                  +14 減衰 1 の速さ
//   +15 減衰 2 の速さ                      +20 フィルタの第 2 係数
//   +26 音程の基準（半音。引く）           +27/+28 GM/XG の半音のずらし
//   +30..+41 ループ前・ループ長・形式と番地
constexpr u32 DRUM_PITCH_TAB = 0x1E9298;   // セント → 音程の目盛り（1200 = 1024）
constexpr u32 DRUM_KIT_TABLE = 0x292250;   // キット → 鍵ごとのずれの表（4 バイト）
constexpr u32 DRUM_RECORDS   = 0x283dd0;   // ずれの元になる番地
constexpr u32 PART_KIT       = 0x110;      // パートの塊の中の、キットの番号

// キットの番号と鍵から、ドラムの 1 打の記録（42 バイト）。無ければ nullptr。
// 実機の `0x134DB8`
inline const u8 *drum_record(const u8 *rom, int kit, int note)
{
	if (!rom || (kit & 0x80))
		return nullptr;                 // bit7 が立つキットは別の道（未対応）
	const u32 base = rd32(rom, DRUM_KIT_TABLE + u32(kit & 0x7f) * 4);
	if (base < 0x200000 || base > 0x2ffff0)
		return nullptr;
	const u16 off = rd16(rom, base + u32(note & 0x7f) * 2);
	return off == 0xffff ? nullptr : rom + DRUM_RECORDS + off;
}

// **その記録に波形が埋まっているか**（doc/native-engine.md の 6.234）。
// ふつうの打は +24/+25 が `FFFF` で、波形の記録（16 バイト）が +26 にそのまま
// 入っている。**SFX キット**（MSB 126。キット番号 47 など）の打はそこが
// `FFFF` でない 16bit の索引で、+26 から先は 0 のまま。波形はその索引の先に
// あるが、**どの表を引くのかはまだ解けていない**（波形の組の番号でも、
// 間隔の決まった要素の並びでもなかった）。
// 埋まっていない記録を式で組むと、波形の番地が 0 になって雑音が鳴るので、
// こういう打は firmware に回す
inline bool drum_rec_has_wave(const u8 *rec)
{
	return rec && rd16(rec, 24) == 0xffff;
}

// **波形の埋まっていない打は、旋律の音色記録から組む**（6.234）。
// +24/+25 は `(b24 << 7) | (b25 & 0x7f)` の索引で、`0x283B50` の 4 バイトの表を
// 引くと音色記録（`VOICES` からの距離の半分。`VOICE_TABLE` と同じ形）が出る。
// 実機はその記録の要素を押した鍵と強さで選び、**波形も音程も「鍵 64」として**
// 組む（押した鍵は音程に効かない。SFX は鍵で高さを変えない音だから）。
//   波形: `wave_entry(組, wave_note(要素, 64))` … キット 47・48 の 57 打すべてで一致
//   音程: `pitch_reg(波形, 64, 追従, 粗調+微調, 支点)` … 73 スロット中 68 が一致
//         （鍵 60 だと 2 つしか合わない。余りが「追従率 × 4」の形で出て 64 と分かった）
// 表は GROUP_XG（0x283950）から 0x80 おきに並ぶ表の続きにある。
// 実機の 0x134BE6 に仮の見張りを付けて引数を覗き、要素を指していると分かって解けた
constexpr u32 SFX_VOICE_TABLE = 0x283B50;
constexpr u32 SFX_VOICES      = 0x200ee0;   // xg::voice_rom::VOICES と同じ
constexpr u32 SFX_VOICES_END  = 0x23cece;   // xg::voice_rom::VOICES_END と同じ
constexpr int SFX_NOTE        = 64;         // 波形と音程を決める鍵（固定）

// **SFX の打のドラムセットアップ**（6.236）。実機は打ごとの値を要素のバイトに足してから
// 旋律の道で組む。firmware で 0-127 を振って合わせた: 切る高さの索引（byte37）に値 − 64、
// 共振（byte35）に (値 − 64) >> 1、立ち上がり・減衰 1・減衰 2 の速さ（byte73・74・75）に値 − 64。
// e は要素（84 バイト）の写し
constexpr int ELEM_BYTES = 84;
inline void sfx_setup_apply(u8 *e, int cut, int reso, int atk, int dec1, int dec2)
{
	auto add = [&](int i, int d, int hi) { e[i] = u8(std::clamp(int(e[i]) + d, 0, hi)); };
	add(37, cut - 64, 127);
	add(35, (reso - 64) >> 1, 63);
	add(73, atk - 64, 63);
	add(74, dec1 - 64, 63);
	add(75, dec2 - 64, 63);
}

inline u32 sfx_voice_record(const u8 *rom, const u8 *drec)
{
	if (!rom || !drec || drec[24] == 0xff)
		return 0;
	const u32 idx = (u32(drec[24]) << 7) | (drec[25] & 0x7f);
	const u32 slot = SFX_VOICE_TABLE + idx * 4;
	const u32 rec = SFX_VOICES + rd32(rom, slot) * 2;
	return (rec >= SFX_VOICES && rec + 16 <= SFX_VOICES_END) ? rec : 0;
}

// `SMU2000_DRUM_EXACT=1` で、ドラムを写し取りではなく式で組む
inline bool drum_exact()
{
	static const bool on = std::getenv("SMU2000_DRUM_EXACT") != nullptr;
	return on;
}

inline int drum_cents(const u8 *rec, int coarse = 64, int fine = 64, bool xg = true)
{
	const int semi = coarse + (xg ? s8(rec[28]) : int(rec[27])) - int(rec[26]);
	return semi * 100 + (int(rec[1]) - 64) + (fine - 64);
}

inline u16 drum_pitch_reg(const u8 *rom, const u8 *rec, int cents)
{
	int c = cents < 0 ? -cents : cents;
	if (c > 9600)
		c = 9600;
	const int t = int(rd16(rom, DRUM_PITCH_TAB + u32(c) * 2) & 0x3fff);
	const u32 addr = u32(rec[38]) << 24 | u32(rec[39]) << 16 | u32(rec[40]) << 8 | rec[41];
	const bool fmt3 = ((addr >> 30) & 3) == 3;
	// **音程には上下の頭打ちがある**（6.236。打の粗調を 0-127 と振って確かめた）。
	// 下は -0x1FFF（TknoKtLo の鍵 38 で `2001`）、上は 0x1BFF（7 オクターブ。SLatinKt の鍵 36 で
	// `1bff`）、**形式 3 の波形は上が 0x7FF**（2 オクターブ。China Kit の鍵 38 で `47ff`）
	const int sv = std::clamp(cents < 0 ? -t : t, -0x1fff, fmt3 ? 0x7ff : 0x1bff);
	u16 v = u16(sv & 0x3fff);
	if (fmt3)
		v = u16(v | 0x4000);
	return v;
}

// ドラムの 1 打のレジスタを、記録だけから組む（写し取りを使わない）。
// 4 打 × 全鍵 58 個で実機と一致したものだけを入れてある
// **ドラムの「音量」（セットアップの 02、NRPN 16）は
// `0x09` ではなく、**立ち上がりの目盛り**を動かす**（6.180）。
//
//   目盛り = clamp(rec[13] + 音量 - 64, 0, 127)
//   0x06    = ATTACK_TAB[目盛り] << 8 | (目盛り >= 126 ? 0x00 : 0x7e)
//
// 既定の 64 でちょうど rec[13] そのものになる。
// Standard Kit の鍵 36（rec[13] = 122）・鍵 38・42（127）を
// 音量 0-127 の全段で確かめた（`tools/native/drumlvl.py`）。
// `0x07`・`0x08`・`0x09` は音量で動かない
// **ドラムの NRPN は記録のバイトをずらすだけ**（6.180）。
// rec[11] 切る高さ・[12] 共振・[13] 立ち上がり・[14][15] 減衰で、
// どれも `記録 + 値 - 64` を 0-127 に収めてからいつもの道を通る
inline int drum_rec_idx(const u8 *rec, int i, int v)
{
	const int x = int(rec[i] & 0x7f) + (v < 0 ? 64 : v) - 64;
	return x < 0 ? 0 : (x > 127 ? 127 : x);
}

inline int drum_atk_idx(const u8 *rec, int atk) { return drum_rec_idx(rec, 13, atk); }

inline int drum_cut_idx(const u8 *rec, int cut) { return drum_rec_idx(rec, 11, cut); }

// **減衰 1 と減衰 2 は別の値**（3n rr 0E・0F）。NRPN 17 は両方に同じ値を入れる
inline slot_regs drum_note(const u8 *rom, const u8 *rec, int att,
                           const defaults &d = defaults(),
                           int coarse = 64, int fine = 64, int atk = 64,
                           int cut = 64, int reso = 64, int dec1 = 64, int dec2 = 64)
{
	slot_regs r;
	if (!rom || !rec)
		return r;
	{
		// **共振が浅いと切る高さは 0x7C0 で頭打ち**（旋律と同じ。6.167）
		int c0 = int(rd16(rom, CUTOFF_TAB + u32(drum_cut_idx(rec, cut)) * 2) & 0x7ff);
		if ((rec[12] >> 2) < 4 && c0 > CUTOFF_MAX)
			c0 = CUTOFF_MAX;
		r.set(0x00, u16(0x1000 | u16(c0)));
	}
	r.set(0x01, 0xffff);
	r.set(0x02, filter2_reg(rec[20]));
	r.set(0x03, d.post);
	r.set(0x04, u16(u16(drum_rec_idx(rec, 12, reso) >> 2) << 11));
	r.set(0x05, d.lfo_amp);
	// **速さの表は 2 倍しない**（旋律は rate_scale で 2 倍する）
	{
		const int ai = drum_atk_idx(rec, atk);
		r.set(0x06, u16(u16(rom[ATTACK_TAB + u32(ai)]) << 8
		                | (ai >= 126 ? 0x00 : 0x7e)));
	}
	r.set(0x07, u16(u16(rom[DECAY_TAB + u32(drum_rec_idx(rec, 14, dec1))]) << 8 | 0x04));
	r.set(0x08, u16(u16(rom[DECAY_TAB + u32(drum_rec_idx(rec, 15, dec2))]) << 8
	                | u16(((0x7f - int(rec[10])) * 2) & 0xff)));
	r.set(0x09, u16(att & 0xff));
	r.set(0x0a, 0x7000);
	r.set(0x0b, 0x0000);
	r.set(0x10, 0x0000);
	r.set(0x11, drum_pitch_reg(rom, rec, drum_cents(rec, coarse, fine)));
	const wave_info w = read_wave(rec + 26);
	{
		// 逆向きのサンプルは 2 つの数を入れ替える（6.233）。
		// StandKit# のタム 3 つと AnalogKit の鍵 28 がこれ
		u32 pre = w.pre_loop, loop = w.loop_len;
		wave_backwards_swap(pre, loop);
		r.set(0x12, u16(pre >> 16));
		r.set(0x13, u16(pre));
		r.set(0x14, u16(loop >> 16));
		r.set(0x15, u16(loop));
	}
	r.set(0x16, u16(w.format_addr >> 16));
	r.set(0x17, u16(w.format_addr));
	for (int i = 0; i < 6; i++)
		r.set(0x20 + i * 2, d.iir[i]);
	for (int i = 0; i < 6; i++)
		r.set(0x32 + i, d.mix[i]);
	return r;
}

// ---- **ドラムの音量・パン・送り**（doc/native-engine.md の 6.155）
//
// 写し取りを捨てる（段 4）ための最後の 3 本。`tools/native/drumprobe.py` で
// 1 キット 57 打 × 強さ 3 通りを実機と突き合わせて出した。
//
// 元になる値は**ドラムセットアップ**（`3n rr pp`。ワーク RAM）:
//   +02 音量  +04 パン  +05 リバーブ送り  +06 コーラス送り
// キットを選ぶと firmware が ROM の記録（42 バイトの +2/+4/+5/+6）から
// ここへ写すので、曲が `3n rr pp` で上書きしていてもそのまま読める。

// **ドラムは強さの曲線が 1**（旋律は要素の byte68 で選ぶ）。
// 強さ 40/100/127 で 22/4/0。曲線 0 なら 26/5/0 で合わない
constexpr int DRUM_VEL_CURVE = 1;

// `0x09` に入れる減衰。**1 キット 57 打 × 強さ 3 通りで、実機と完全に一致**。
//
//   目盛り = clamp((音量 + 記録[+29] + 1) × つまみ >> 7, 1, 128)
//   減衰   = 2 × clamp(表[0x80 + 目盛り] + 強さの減衰(曲線 1), 0, 127)
//
// **記録の +29**（波形の記録の 4 バイト目。符号つき）が効く。45 打はここが 0
// なので気づかず、残り 12 打だけ外していた（6.160）。実機は
// `0x12A436`-`0x12A540` でこれを組んでいる
inline int drum_att(const u8 *rom, const u8 *rec, int level, int vel,
                    int gain = VOL_GAIN_DEF)
{
	if (!rom)
		return 0x40;
	// **打の音量が 0 なら減衰をいっぱいにする**（実機は `0xFE`。StandKit の鍵 38 で確かめた。6.236）
	if (level <= 0)
		return 0xfe;
	const int adj = rec ? int(s8(rec[29])) : 0;
	int l = (level + adj + 1) * (gain > 128 ? 128 : (gain < 0 ? 0 : gain)) >> 7;
	if (l < 1) l = 1;
	if (l > 128) l = 128;
	int a = int(rom[LEVEL_TAB + 0x80 + u32(l)])
	      + velocity_att(rom, vel, DRUM_VEL_CURVE);
	if (a > 127) a = 127;
	if (a < 0) a = 0;
	return a * 2;
}

// `0x32`（パン）。**57 打すべて実機と一致**。旋律の `voice_pan_reg` と同じ形で、
// 音色のパンの代わりにドラムセットアップのパンを使う
inline u16 drum_pan_reg(const u8 *rom, int pan, int part_pan = 64)
{
	if (!rom)
		return 0x0808;
	const int p = pan < 0 ? 64 : (pan > 127 ? 127 : pan);
	const int q = (part_pan < 0 ? 64 : part_pan) & 0x7f;
	int l = int(rom[PAN_BASE_TAB + u32(q)]) + int(rom[PAN_CURVE_TAB + u32(p)]);
	int r = int(rom[PAN_BASE_TAB + u32(0x80 - q)])
	      + int(rom[PAN_CURVE_TAB + u32(0x80 - p)]);
	if (l > 255) l = 255;
	if (r > 255) r = 255;
	return u16((l << 8) | r);
}

// `0x33`・`0x34` の下位（送り）。**ドラム 57 打すべて実機と一致**。
// パートの送り（CC91/CC93）と**打ごとの送り**を掛け合わせてから表を引き、
// 真ん中で 16 の下駄、パンで振るぶん目減りする。
// **旋律にも使える**（打ごとの送りを 127 にすれば掛け算が消える）
inline int send_level_att(const u8 *rom, int part_send, int extra_send, int pan)
{
	if (!rom)
		return 0xff;
	const int ps = part_send < 0 ? 40 : (part_send > 127 ? 127 : part_send);
	const int ds = extra_send < 0 ? 127 : (extra_send > 127 ? 127 : extra_send);
	const int eff = (ps * ds) / 127;
	const int v = 16 + send_att(rom, eff) + pan_send_adj(rom, pan < 0 ? 64 : pan);
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// 1 音ぶんのレジスタを作る。att は 0x09 に入れる減衰（0-255。小さいほど大きい音）
inline slot_regs build_note(const u8 *rom, const u8 *elem, int note, int att,
                            const voice_cal *cal = nullptr,
                            const defaults &d = defaults(), int cents_extra = 0,
                            int vel = 100, int cc_atk = 64, int cc_dec = 64,
                            int cc_vrate = 64, int cc_vdep = 64, int wnote = -1,
                            int knote = -1, bool soft = false,
                            int part_peg_init = 64, int part_peg_atk = 64)
{
	slot_regs r;
	// **移調・ノートシフト・粗調は「鍵の曲線」には効かない**（6.172）。
	// 波形と音程はずらした鍵、切る高さ・減衰の速さ・音程の包絡線は
	// **押した鍵そのもの**で引く。GrandPno を -24 半音、SquareLd を
	// -3 半音、粗調（RPN 2）・ノートシフト（08 pp 08）・
	// マスター移調（00 00 06）の 3 通りで確かめた。どれも同じ
	const int kn = knote < 0 ? note : knote;
	// **CC84 で滑り出す音は、波形を「滑り出す鍵」で選ぶ**（6.168）
	const u8 *we = wave_entry(rom, wave_set(elem),
	                          wave_note(rom, elem, wnote < 0 ? note : wnote));
	if (!we)
		return r;
	const wave_info w = read_wave(we);

	// --- フィルタ。切る高さは ROM の表（0x1E5B58）を byte37 で引く。
	// 実機はここに鍵と強さの倍率を掛ける（`0x127FA4`）が、その係数がまだ分からない。
	// 倍率 1 として表を引くだけでも、開き切りよりはずっと実機に近い
	// 実機（0x12AC98）は表を引いた値に**鍵の追従**（0x12C1E4）を足して
	// 0-0xFFF に収める。鍵の追従を入れていなかったので、Flute のように
	// 曲線を持つ音色で鍵を押した瞬間の値がずれていた（6.71）
	// **鍵を押した瞬間の値そのもの**は `cutoff_keyon` が出せる（14 音色 ×
	// 鍵 5 通り × 強さ 3 通りで実機と完全に一致）。**既定で入**（6.116）。
	// `SMU2000_CUT_EXACT=0` で写し取り前提の前の道に戻せる
	r.set(0x00, cut_exact()
	            ? cutoff_keyon(rom, elem, kn, vel, true, cc_atk,
	                           soft_vel(vel, soft))
	            : u16(0x1000 | (rd16(rom, CUTOFF_TAB + u32(elem[37]) * 2) & 0x7ff)));
	// **鍵を押した瞬間の 0x01 は 0xFFFF**（実機は毎回そう書いて、最初の
	// 包絡線の目で本当の値に置き換える）。14 音色を実機と突き合わせて
	// 確かめた（doc/native-engine.md の 6.67）
	r.set(0x01, 0xffff);
	// フィルタの第 2 係数。実機（0x12AFCE）は **byte82 を 16 倍**して
	// 0x800 の下駄を履かせ、0x800-0xFFF に収めてから下 11bit を取る。
	// つまり素直に byte82 * 16 で、0x7FF で頭打ち（DistGtr の 0x180、
	// Kitayama の 0x570 が実機と一致した）
	r.set(0x02, filter2_reg(elem[82]));
	r.set(0x03, d.post);
	// フィルタの第 2 パラメータ（共振）。byte35 から強さぶんを引いて（byte81）、
	// 1 ビット落として 5bit にする（0x12806A）。18 音色 × 強さ 3 通りで一致
	r.set(0x04, u16(reso_level(elem, vel) << 11));
	// LFO の深さ（音量側）。実機（0x129B34）は byte16 を 2 倍して下位に置くが、
	// **遅れ（byte12）と byte13 がどちらも 0 のときだけ**使う（0x127D18）。
	// Vibes（byte12=0・byte13=0・byte16=2）は 4、Koto（byte12=48）は 0
	// **打鍵のときの上位は `0xFA00`**。鳴っている途中の書き直し
	//（0x12E60C）だけが `0xAA00` になる（6.199）
	r.set(0x05, u16((d.lfo_amp & 0xff00)
	                | u16((elem[12] || elem[13]) ? 0 : ((elem[16] * 2) & 0x7f))));
	// LFO の型と刻み。上位は byte11 に**byte9 が 0 でなければ** 0x40 を足したもの
	// （Rain は byte9=0 で `2d`）。下位は**音程の深さ = byte14 × 3**
	// （PanFlute の byte14=1 で 3、ChiffLead・TnklBell・Helicopter の 2 で 6）
	// 深さは `0x05` と同じく、**遅れ（byte12）と byte13 がどちらも 0 のとき**だけ。
	//
	// そのうえで **byte9 が 2 だと音程の深さは 0** になる（6.95）。音色の記録
	// 全部（962 件）で byte12・byte13 が 0 かつ byte14 が 0 でない要素は
	// BirdTweet（byte9=2・byte14=6）と Choral（byte9=2・byte14=1）の 2 つだけ
	// で、実機はどちらも 0 を書く。byte10=0 の組（JumpBrss・StdiumOr）は
	// ちゃんと深さを書くので、効いているのは byte10 ではなく byte9 のほう。
	// **音量側（0x05）は 0 にならない**（Choral の byte16=13 → 26 が一致）
	// **ビブラートのつまみ**（08 pp 15・16）で速さも深さも動く（6.162）。
	// 止まっている音色（遅れ byte12・byte13 があるもの、byte9 が 2）は
	// つまみを回しても動かない。**素の深さが 0 でも、つまみでは動く**
	// （SquareLd は素が 0 で、つまみ 96 のとき実機は 0xa0）
	// **遅れを持つ音色は 0 から始めて、20ms ごとにせり上げる**（6.175）。
	// byte9 が 2 以上の音色はそもそも揺れない
	const bool vgate = (elem[12] || elem[13] || elem[9] >= 2);
	// **bit7（8 倍の目盛り）は落とさない**（6.234）。byte14 が 62・63 の要素は表の値が
	// 0x96 で、実機もそのまま下位に書く（SFX Kit1 の鍵 28・29 で `0a=xx96`）。
	// `& 0x7f` で 0x16 にしていた。旋律の記録 1353 個のうち変わるのは 9 要素だけ
	const int plfo0 = vgate ? 0 : vib_ramp_reg(rom, elem[14]);
	const int lrate = vib_rate(int(elem[11] & 0x3f), cc_vrate);
	const int plfo  = vgate ? 0 : vib_depth(plfo0, cc_vdep);
	r.set(0x0a, u16(((((elem[9] ? 0x40 : 0) | (lrate & 0x3f)) << 8))
	                | u16(plfo & 0xff)));
	// 音程の包絡線。速さが 127（即到達）のときだけ初めの高さは byte31 を使う
	// **パートのピッチ EG**（XG の 08 pp 69・6A ＝ ワーク RAM の +0x62・+0x63。6.214）。
	// アタックの時間は、素の速さの目盛りを**立ち上がりのつまみと同じ表**（eg_rate_cc）で
	// 動かす: 64 より上は表の値で頭打ち（遅く）、下は足す（速く。63 で止まる）。
	// だから素が即到達（63）の音色では、下げても何も変わらない。
	// 初めの高さは素の byte30 にずらし量を足す
	const int praw = part_peg_atk == 64 ? int(elem[26]) : eg_rate_cc(rom, int(elem[26]), part_peg_atk);
	const int prate = peg_rate_reg_raw(rom, elem, praw, kn, vel, 64, cc_atk);
	r.set(0x0b, u16(prate << 8));
	r.set(0x10, peg_reg(rom, prate == 127 ? peg_cents(elem, elem[31], vel)
	                                      : peg_cents(elem, elem[30], vel) + part_peg_cents(part_peg_init),
	                   elem));

	// --- 包絡線（doc/native-engine.md の 6.3・6.4）
	//
	// 減衰の速さは鍵で動く。firmware の 0x12ADD0 と 0x1272F4 がやっているのは
	//   補正 = ((鍵 - 折れ点) * ((depth - 64) * 16)) >> 8     （負は 0 の側へ）
	//   目盛り = clamp(記録の値 + 補正, 1, 63) * 2
	// で、その目盛りで ROM の表を引いたものがレジスタの上位バイトになる。
	// 深さは byte70、折れ点の鍵は byte71（鍵 36・60・84 で確かめた）。
	const int corr = rate_key_corr(elem, kn);
	// **立ち上がりのつまみ（CC73）で目盛りが動く**（6.157）
	const int arate = eg_rate_cc(rom, int(elem[73]), cc_atk);
	const u8 atk = rom[ATTACK_TAB + u32(attack_idx(arate))];
	// 写し取りがあれば、そのときのずれを表の目盛りに足す（上の dec_adj を見よ）
	const int a1 = cal && cal->have ? cal->dec_adj[0] : 0;
	const int a2 = cal && cal->have ? cal->dec_adj[1] : 0;
	const u8 dc1 = rom[DECAY_TAB  + clamp_idx(
	                   rate_scale(eg_dec1_cc(int(elem[74]), cc_atk), corr) + a1)];
	// **減衰のつまみ（CC75）で目盛りが動く**（6.157）
	const u8 dc2 = rom[DECAY_TAB  + clamp_idx(
	                   rate_scale2(eg_rate_cc_add(int(elem[75]), cc_dec), corr) + a2)];
	// はじめの音量。アタックが最速（63）のときだけ 0 で、あとは 0x7e
	r.set(0x06, u16(atk << 8 | (arate >= 0x3f ? 0x00 : 0x7e)));
	r.set(0x07, u16(dc1 << 8 | (((0x7f - elem[77]) * 2) & 0xff)));
	r.set(0x08, u16(dc2 << 8 | (((0x7f - elem[78]) * 2) & 0xff)));
	r.set(0x09, u16(att & 0xff));

	// --- 音程と波形（6.2）
	// 要素の byte17 は**半音単位の粗調**、byte18 は**セント単位の離調**（どちらも 64 が中央）。
	// 離調は重ねの音色で 2 つの層をずらすのに使う。入れないと層がぴったり重なって
	// 打ち消し合わず、3dB ほど大きくなる（doc/native-engine.md の 6.18）
	// **鍵が 0-127 からはみ出したら、はみ出したぶんを引く**（6.141）。
	// 実機は粗調（byte17）を足した鍵を 0-127 に収めてから波形も音程も出す
	// （`wave_note` は丸めているのに、音程だけ丸めていなかった）。
	// Shakuhachi の第 2 要素は粗調 +12 半音・支点 53 なので、鍵 120 で
	// 53+67+12 = 132 ＝ 5 半音はみ出す。実機との差はちょうど 500 セントだった
	r.set(0x11, pitch_reg(w, note, key_follow(rom, elem),
	                      cents_extra + elem_tune(elem) - note_overflow(rom, elem, note) * 100,
	                      key_pivot(elem)));
	// **鳴らし始める位置をずらす**（実機の `0x12A9C8`）。要素の byte79 が
	// 128 サンプル単位、byte80 が 1 サンプル単位の下駄で、ループ前の長さから
	// 引く。Oboe(7→896)・Clarinet(2→256)・Bagpipe(8→1024) で実機と一致した。
	// 入れていなかったので、その 3 音色は波形がまるで合っていなかった
	{
		u32 pre = w.pre_loop, loop = w.loop_len;
		// 逆向きなら 2 つの数を入れ替える（6.233）。**下駄を引く前**に入れ替えて、
		// 下駄は 0x12/0x13 に行くほうから引く。旋律の音色で逆向きのものは
		// 見つかっていないので、この組み合わせは実機で確かめられていない
		wave_backwards_swap(pre, loop);
		const u32 skip = u32(elem[79]) * 128 + elem[80];
		if (pre > skip)
			pre -= skip;
		else
			pre = 0;
		r.set(0x12, u16(pre >> 16));
		r.set(0x13, u16(pre));
		r.set(0x14, u16(loop >> 16));
		r.set(0x15, u16(loop));
	}
	r.set(0x16, u16(w.format_addr >> 16));
	r.set(0x17, u16(w.format_addr));

	// --- 声の EQ とミキサ
	for (int i = 0; i < 6; i++)
		r.set(0x20 + i * 2, d.iir[i]);
	for (int i = 0; i < 6; i++)
		r.set(0x32 + i, d.mix[i]);
	// **音色そのものが持つパン**（byte69）。写し取りがあれば下で上書きされる
	r.set(0x32, voice_pan_reg(rom, elem, note));
	// 送りはそのパンのぶん目減りする
	{
		const int adj = pan_send_adj(rom, voice_pan_pos(rom, elem, note));
		for (int i = 0; i < 2; i++) {
			// **切ってある送り（0xff）はそのまま**。実機も頭打ちなので、
			// ここでパンのぶん引くと切ったはずの送りが開いてしまう
			if ((d.mix[1 + i] & 0xff) >= 0xff)
				continue;
			int v = int(d.mix[1 + i] & 0xff) + adj;
			v = v < 0 ? 0 : (v > 255 ? 255 : v);
			r.set(0x33 + i, u16((d.mix[1 + i] & 0xff00) | u16(v)));
		}
	}

	// --- 写し取った値で上書き。式が分かっていない所だけ
	//
	// **0x06（立ち上がり）もここに入れる。** 入れていなかったので、
	// パート側の EG の設定（CC73 など）が native の音に一切効いていなかった。
	// CC73 を全パートに送る曲では全部の音の立ち上がりが狂う（実測で
	// firmware 417e に対しこちらは 387e ＝ ずっと遅い）。
	// 立ち上がりは鍵でも強さでも変わらないと測ってあるので（nativeplay
	// --egwatch を鍵 36-96・強さ 1-127 で確認）、写し取った値をそのまま使える。
	// 0x07・0x08（減衰）は鍵で変わるので、ここには入れられない（宿題）
	if (cal && cal->have) {
		// 0x20-0x2b は**偶数番だけ**でよい（奇数番と 0x30・0x31 は実機の
		// firmware も一度も書かない。記録を追って確かめた）
		// **0x0b・0x10 はもう写し取らない**。音程の包絡線を式で出すようになった
		// （写し取りは包絡線が終わったあとの値を拾うので、入れると出だしの
		//  しゃくりが丸ごと消えていた。doc/native-engine.md の 6.68）
		// **`0x01` も写し取りで上書きしない**（6.210）。押鍵の瞬間は
		// 実機はどの音色でも `0xFFFF`（6.67）なのに、写し取りは
		// そのあとの 10ms の目の値（`0xDCFF` など）を拾っていた。
		// 下位 8bit しかチップは見ていないので音は変わらないが、物差しが濁る
		static const int COPY[] = { 0x00, 0x06, 0x0a,
		                            0x20, 0x22, 0x24, 0x26, 0x28, 0x2a,
		                            0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };
		for (int i : COPY) {
			// **`0x00` を式で出せるときは写し取りで上書きしない**（6.124）。
			// 写し取りは鍵 1 つ・強さ 1 つぶんしか無いので、**強さの違う音**の
			// 切る高さが出せない（写し取りが強さ 100 なら、強さ 127 の音は
			// 実機より暗いままだった。keylevel の強さ 127 の音が全部そう）
			if (i == 0x00 && cut_exact())
				continue;
			if (cal->has(i))
				r.set(i, cal->reg[i]);
		}
	}
	return r;
}

} // namespace nv
} // namespace xg

#endif // S_MU2000_XG_NATIVE_VOICE_H
