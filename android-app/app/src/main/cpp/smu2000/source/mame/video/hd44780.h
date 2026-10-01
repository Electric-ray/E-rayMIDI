// license:BSD-3-Clause
// copyright-holders:Sandro Ronco
//
// Hitachi HD44780 LCD コントローラ。
// MAME の src/devices/video/hd44780.* から、firmware が触る部分だけを取った。
//
// **ビジーフラグは音を出すのにも要る**。MU2000 の firmware は LCD にコマンドを
// 送るたびにビジーが立つのを見ており、常に「空いている」と返すと初期化の途中で
// 先へ進まなくなる。
//
// 表示の組み立て（render）は MAME と同じ形にしてある。文字の絵は CGROM から
// 引き、0x00-0x0f だけは CGRAM から引く。SVG のレイアウトは持たない。
//
// MAME はビジーの計測に emu_timer を使っていたが、こちらは CPU のサイクル数で
// 数える。LCD の発振は 270kHz、命令は 10 サイクル（37us）、クリアと
// ホームだけ 410 サイクル（1.52ms）。

#ifndef S_MU2000_HD44780_H
#define S_MU2000_HD44780_H

#pragma once

#include "state.h"
#include "../../compat/mamecompat.h"

class hd44780_device
{
public:
	// 状態の保存と復元（src/state.h）
	void state(state_io &s);

	// cpu_hz: ビジーの残り時間を数えるための CPU 側の周波数
	hd44780_device(u32 cpu_hz = 28000000, u32 lcd_hz = 270000)
		: m_cpu_hz(cpu_hz), m_lcd_hz(lcd_hz) {}

	void reset();

	// 現在の CPU サイクル。読み書きの前に入れておく
	void set_now(u64 cycles) { m_now = cycles; }

	void control_w(u8 data);
	u8   control_r() const;
	void data_w(u8 data);
	u8   data_r();

	bool busy() const { return m_now < m_busy_until; }

	// 表示内容。生の DDRAM
	const u8 *ddram() const { return m_ddram; }
	// 利用者が作った字（0x00-0x07）の絵。1 文字 8 バイト
	const u8 *cgram() const { return m_cgram; }

	// **外から 1 マス書き替える**。native の口で液晶のメーターを自前で
	// 描くのに使う（doc/native-engine.md の 6.148）。firmware の手順を
	// 通らないので、表示の状態（カーソルなど）は何も変えない
	void poke_ddram(u32 i, u8 v) { if (i < 0x80) m_ddram[i] = v; }

	// **そのマスを native の持ち物にする**（doc/native-engine.md の 6.188）。
	// 持ち物のあいだ、firmware が書いても**表示は変わらない**。書いた値は
	// 「firmware の思っている画面」(`fw_ddram`) にだけ入る。
	//
	// 手放すときに、その控えを表示へ写し戻す。firmware は自分の持っている
	// 画面の控えと**違うマスしか送らない**ので、写し戻さないと、こちらが
	// 置いた字が二度と消えない
	void set_owned(u32 i, bool on);
	void clear_owned();
	bool owned(u32 i) const
	{ return i < 0x80 && ((m_owned[i >> 6] >> (i & 63)) & 1); }

	// firmware が「いま画面にはこれが出ている」と思っている中身。
	// 持ち物にしていないマスでは DDRAM と同じ
	const u8 *fw_ddram() const { return m_fw; }

	// **外字（CGRAM）も同じように取る**（6.190）。音色の絵は
	// native が先に書くので、取らないと firmware の古い絵と交互になる
	void poke_cgram(u32 i, u8 v) { if (i < 0x40) m_cgram[i] = v; }
	void set_cg_owned(u32 i, bool on);
	void clear_cg_owned();
	bool cg_owned(u32 i) const
	{ return i < 0x40 && ((m_cg_owned >> i) & 1); }

	// **firmware が表示を変えた書き込みの記録**。native の口で点滅を受け持つのに
	// 使う（mu2000::blink_*）。持ち物のマスへの書き込みと、同じ値の書き直しは
	// 入らない。溜まりすぎたら古いものから捨てず、新しいものを捨てる
	struct change { u8 cg, addr, before, after; };
	static constexpr int CHANGE_MAX = 64;
	int changes(const change *&out) const { out = m_changes; return m_n_changes; }
	void clear_changes() { m_n_changes = 0; }

	// 文字の絵。HD44780U B04 の CGROM 4KB（1 文字 16 バイト、下位 5bit が絵）
	void set_cgrom(const u8 *rom, size_t size)
	{ m_cgrom = (rom && size >= 0x1000) ? rom : nullptr; }

	// 画面を組み立てる。MAME と同じ並びで、80 マス × 16 バイトを返す。
	// マス (行, 桁) は render()[16 * (行 * 桁数 + 桁)]、各バイトの下位 5bit が
	// 1 行ぶんの点。左端が bit4
	static constexpr int RENDER_SIZE = 80 * 16;
	const u8 *render();

	int  lines() const     { return m_num_line; }
	int  line_size() const { return 80 / m_num_line; }
	int  char_size() const { return m_char_size; }
	bool display_on() const { return m_display_on; }

private:
	void set_busy(u16 lcd_cycles)
	{
		m_busy_until = m_now + u64(lcd_cycles) * m_cpu_hz / m_lcd_hz;
	}
	void correct_ac();
	void update_ac(int direction);
	void shift_display(int direction);

	enum { DDRAM, CGRAM };

	u32 m_cpu_hz = 0, m_lcd_hz = 0;
	u64 m_now = 0, m_busy_until = 0;

	const u8 *m_cgrom = nullptr;
	u8  m_render_buf[RENDER_SIZE] = {};
	u8  m_ddram[0x80] = {};
	u8  m_fw[0x80] = {};        // firmware の思っている画面（6.188）
	u64 m_owned[2] = {};        // native の持ち物のマス
	u8  m_cg_fw[0x40] = {};     // firmware の思っている外字（6.190）
	u64 m_cg_owned = 0;
	u8  m_cgram[0x40] = {};
	int m_ac = 0;
	int m_active_ram = DDRAM;
	int m_direction = 1;
	int m_disp_shift = 0;
	int m_num_line = 1;
	int m_char_size = 8;
	int m_data_len = 8;
	bool m_shift_on = false;
	bool m_display_on = false, m_cursor_on = false, m_blink_on = false;
	bool m_nibble = false;      // 4bit 接続のときの上位/下位
	u8  m_ir = 0, m_dr = 0;
	change m_changes[CHANGE_MAX] = {};
	int m_n_changes = 0;
	void note_change(bool cg, int addr, u8 before, u8 after)
	{
		if (m_n_changes < CHANGE_MAX)
			m_changes[m_n_changes++] = change{ u8(cg), u8(addr), before, after };
	}
};

#endif // S_MU2000_HD44780_H
