// license:BSD-3-Clause
//
// パラメータの層（doc/params.md）。音色・エフェクト・パートの値を名前で読み書きする。
//
// **値は MU2000 の firmware に聞く。** XG の問い合わせとダンプ要求に、firmware は
// MIDI OUT から答える。ここはその返事を読んで「写し」を持つ。画面が送った値を
// 覚えておくのではないので、パネルで変えたものも曲が変えたものも写しに入る。
//
// Windows にも画面にも依存しない。バイトを受け取り、バイトを返すだけ。
// 音源には触らない（送るバイトは呼んだ側が MIDI IN へ流す）。
// 1 本の糸から使うこと（画面の糸）。

#ifndef S_MU2000_XG_MODEL_H
#define S_MU2000_XG_MODEL_H

#pragma once

#include "compat/mamecompat.h"

#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xg {

// ---- 定義表

enum class area : u8 {
	system,     // 00 00 xx
	effect,     // 02 01 xx（リバーブ・コーラス・バリエーション）/ 03 nn xx（インサーション）
	part,       // 08 pp xx。pp がパート番号（0 から）
};

enum class coding : u8 {
	byte7,      // 1 バイト 7bit。2 バイトなら MSB, LSB（値 = MSB × 128 + LSB）
	nibble,     // 1 バイトに 4bit ずつ（マスタチューン、デチューン）
};

enum class view : u8 {
	raw,        // そのまま
	plus1,      // 1 から数える（プログラム番号）
	center,     // 真ん中を 0 として ±（ノートシフトなど）
	pan,        // 0 がランダム、1-127 で L63 ... C ... R63
	choice,     // 名前の表
	part_off,   // 0-63 がパート 1-64、64-65 が AD1・AD2、127 が OFF
};

struct param {
	const char *key;      // "part.volume"
	const char *label;    // "Volume"
	area  where;
	u8    hi, mid, lo;    // part のときの mid は使わない（パート番号が入る）
	u8    size;           // SysEx のデータのバイト数
	coding enc;
	int   min, max;       // 値の範囲
	int   special;        // 範囲の外で許される値（OFF の 127 など）。無ければ -1
	int   def;            // XG の初期値（資料の値。実測ではない）
	view  how;
	int   center;         // view::center のとき
	const char *const *choices;   // view::choice のとき。min から順に
};

const std::vector<param> &params();
const param *find(const std::string &key);

// 番地を 1 つの数に畳む（hi << 14 | mid << 7 | lo）
constexpr u32 pack(u8 hi, u8 mid, u8 lo) { return u32(hi) << 14 | u32(mid) << 7 | lo; }
u32 address(const param &p, int part = 0);

bool valid(const param &p, int value);
// バンクのように、プログラムを書いたときに初めて効くもの
bool applies_on_program(const param &p);
std::string format(const param &p, int value);

// ---- SysEx。デバイス番号は 0（firmware の既定の「全部」で受ける）

std::vector<u8> param_change(const param &p, int part, int value);
std::vector<u8> param_request(const param &p, int part = 0);
std::vector<u8> dump_request(u32 addr);
inline std::vector<u8> part_dump_request(int part) { return dump_request(pack(0x08, u8(part), 0)); }

// ---- 写し

class model
{
public:
	// MIDI OUT から来たバイトを 1 つずつ。XG のパラメータチェンジと一括ダンプを
	// 読んで写しに入れる。関係ないものは読み捨てる
	void feed(u8 b);

	// 写しにある値。まだ一度も読んでいなければ false（**決め打ちの初期値は返さない**）
	bool get(const param &p, int part, int &value) const;

	// 定義表に無い番地をそのまま読み書きする（エフェクトの種類ごとのパラメータ。xg/fx_params.h）。
	// size バイトを 7bit ずつ（上の桁が先）。set_raw は set と同じく写しを書き換えて送るバイトを返す
	bool get_raw(u32 addr, int size, int &value) const;
	std::vector<u8> set_raw(u32 addr, int size, int value);

	// 書く。写しをすぐ書き換え、送るバイト（パラメータチェンジ）を返す。
	// 書いた番地は少しのあいだ、返事で上書きしない。つまみを回している最中に、
	// 書く前に頼んだ読み返しが届いて古い値へ戻って見えるのを防ぐ
	std::vector<u8> set(const param &p, int part, int value);

	// 書いた（set した）ことを知らせる先。画面で値を触ったのを、プラグインがホストの
	// オートメーションへ伝えるのに使う。写しに入ってきた値（load・feed）では呼ばない
	using edit_listener = std::function<void(const param &p, int part, int value)>;
	void set_edit_listener(edit_listener f) { m_edit = std::move(f); }
	// set_raw（定義表に無い番地。インサーションのパラメータなど）で書いたときの知らせ先
	using raw_listener = std::function<void(u32 addr, int size, int value)>;
	void set_raw_listener(raw_listener f) { m_edit_raw = std::move(f); }

	// ワーク RAM から写した塊を入れる（画面はこれで値を得る。xg/ram.h）。
	// 書いた直後の値は、feed と同じく少しの間は上書きしない。now_ms は音源の時計
	void load(u32 addr, const u8 *data, size_t n, u64 now_ms)
	{
		m_now = now_ms;
		store(addr, data, n);
		m_accepted++;
	}

	// 塊の読み返しを頼む。同じものが並んでいれば足さない
	void want_dump(u32 addr);
	void want_part(int part) { want_dump(pack(0x08, u8(part), 0)); }

	// ときどき呼ぶ。送る問い合わせがあればそのバイトを返す。
	// **返事が戻るまで次は頼まない**。戻らなければ now_ms で時間を見て 1 回だけ頼み直す。
	// now_ms は音源の時刻で数えること（音が止まっている間は返事も来ないので）
	std::vector<u8> poll(u64 now_ms);
	bool busy() const { return m_waiting || !m_queue.empty(); }

	// 読んだメッセージの数（試験用）
	u64 accepted() const { return m_accepted; }
	u64 rejected() const { return m_rejected; }   // チェックサム違いなど

	// 写しを全部忘れる（音源を起動し直したとき）
	void forget();
	// 1 つだけ忘れる（試験で、次に読んだ値が firmware から来たものだと確かめる）
	void forget(const param &p, int part);

private:
	void on_sysex();
	void store(u32 addr, const u8 *data, size_t n);

	static constexpr u64 PIN_MS = 500;       // 書いた番地を返事で上書きしない時間

	std::unordered_map<u32, u8> m_bytes;     // 番地 → 7bit の中身
	std::unordered_map<u32, u64> m_pinned;   // 番地 → 書いた時刻（poll に渡された時刻で数える）
	u64 m_now = 0;
	std::vector<u8> m_msg;
	bool m_in = false;

	std::deque<u32> m_queue;
	bool m_waiting = false;
	u32  m_wait_addr = 0;
	u64  m_sent_ms = 0;
	int  m_tries = 0;

	u64 m_accepted = 0, m_rejected = 0;
	edit_listener m_edit;
	raw_listener m_edit_raw;
};

} // namespace xg

#endif // S_MU2000_XG_MODEL_H
