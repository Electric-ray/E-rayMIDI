// license:BSD-3-Clause
// copyright-holders:Sandro Ronco
//
// MAME の hd44780.cpp から、レジスタとアドレスカウンタの扱いをそのまま取った。
// 表示の組み立ても MAME と同じ形にしてある（hd44780.h の説明を参照）。

#include "hd44780.h"

#include <cstring>

void hd44780_device::reset()
{
	std::memset(m_ddram, 0x20, sizeof(m_ddram));
	std::memset(m_cgram, 0, sizeof(m_cgram));
	m_ac = 0;
	m_active_ram = DDRAM;
	m_direction = 1;
	m_disp_shift = 0;
	m_num_line = 1;
	m_char_size = 8;
	m_data_len = 8;
	m_shift_on = false;
	m_display_on = m_cursor_on = m_blink_on = false;
	m_nibble = false;
	m_ir = m_dr = 0;
	m_busy_until = 0;
	m_owned[0] = m_owned[1] = 0;
	m_cg_owned = 0;
	std::memcpy(m_fw, m_ddram, sizeof(m_fw));
	std::memcpy(m_cg_fw, m_cgram, sizeof(m_cg_fw));
	set_busy(410);      // 電源投入直後はしばらく忙しい
}

void hd44780_device::set_owned(u32 i, bool on)
{
	if (i >= 0x80)
		return;
	const u64 bit = 1ull << (i & 63);
	u64 &w = m_owned[i >> 6];
	if (on) {
		if (!(w & bit))
			m_fw[i] = m_ddram[i];
		w |= bit;
	} else if (w & bit) {
		m_ddram[i] = m_fw[i];
		w &= ~bit;
	}
}

void hd44780_device::set_cg_owned(u32 i, bool on)
{
	if (i >= 0x40)
		return;
	const u64 bit = 1ull << i;
	if (on) {
		if (!(m_cg_owned & bit))
			m_cg_fw[i] = m_cgram[i];
		m_cg_owned |= bit;
	} else if (m_cg_owned & bit) {
		m_cgram[i] = m_cg_fw[i];
		m_cg_owned &= ~bit;
	}
}

void hd44780_device::clear_cg_owned()
{
	if (!m_cg_owned)
		return;
	for (u32 i = 0; i < 0x40; i++)
		set_cg_owned(i, false);
}

void hd44780_device::clear_owned()
{
	if (!m_owned[0] && !m_owned[1])
		return;
	for (u32 i = 0; i < 0x80; i++)
		set_owned(i, false);
}

void hd44780_device::correct_ac()
{
	if (m_active_ram == DDRAM) {
		const int max_ac = (m_num_line == 1) ? 0x4f : 0x67;
		if (m_ac > max_ac)
			m_ac -= max_ac + 1;
		else if (m_ac < 0)
			m_ac = max_ac;
		else if (m_num_line == 2 && m_ac > 0x27 && m_ac < 0x40)
			m_ac = 0x40 + (m_ac - 0x28);
	} else
		m_ac &= 0x3f;
}

void hd44780_device::update_ac(int direction)
{
	if (m_active_ram == DDRAM && m_num_line == 2 && direction == -1 && m_ac == 0x40)
		m_ac = 0x27;
	else
		m_ac += direction;
	correct_ac();
}

void hd44780_device::shift_display(int direction)
{
	m_disp_shift += direction;
	if (m_disp_shift == 0x50)
		m_disp_shift = 0;
	else if (m_disp_shift == -1)
		m_disp_shift = 0x4f;
}

void hd44780_device::control_w(u8 data)
{
	if (m_data_len == 4) {
		if (m_nibble) { m_ir = data & 0xf0; return; }
		m_ir |= (data >> 4) & 0x0f;
	} else
		m_ir = data;

	if (BIT(m_ir, 7)) {                 // DDRAM アドレス設定
		m_active_ram = DDRAM;
		m_ac = m_ir & 0x7f;
		correct_ac();
		set_busy(10);
		return;
	}
	if (BIT(m_ir, 6)) {                 // CGRAM アドレス設定
		m_active_ram = CGRAM;
		m_ac = m_ir & 0x3f;
		set_busy(10);
		return;
	}
	if (BIT(m_ir, 5)) {                 // ファンクションセット
		if (BIT(m_ir, 3))
			m_char_size = 8;            // 2 行のときは 5x10 が使えない
		else
			m_char_size = BIT(m_ir, 2) ? 10 : 8;
		m_data_len = BIT(m_ir, 4) ? 8 : 4;
		m_num_line = BIT(m_ir, 3) + 1;
		correct_ac();
		set_busy(10);
		return;
	}
	if (BIT(m_ir, 4)) {                 // カーソル/表示のシフト
		const int direction = BIT(m_ir, 2) ? +1 : -1;
		if (BIT(m_ir, 3))
			shift_display(direction);
		else
			update_ac(direction);
		set_busy(10);
	} else if (BIT(m_ir, 3)) {          // 表示のオンオフ
		m_display_on = BIT(m_ir, 2);
		m_cursor_on  = BIT(m_ir, 1);
		m_blink_on   = BIT(m_ir, 0);
		set_busy(10);
	} else if (BIT(m_ir, 2)) {          // エントリモード
		m_direction = BIT(m_ir, 1) ? +1 : -1;
		m_shift_on  = BIT(m_ir, 0);
		set_busy(10);
	} else if (BIT(m_ir, 1)) {          // ホーム
		m_ac = 0;
		m_active_ram = DDRAM;
		m_direction = 1;
		m_disp_shift = 0;
		set_busy(410);
	} else if (BIT(m_ir, 0)) {          // 表示クリア
		m_ac = 0;
		m_active_ram = DDRAM;
		m_direction = 1;
		m_disp_shift = 0;
		std::memset(m_ddram, 0x20, sizeof(m_ddram));
		// **画面を消すのは firmware の持ち物を越える**（6.188）
		std::memset(m_fw, 0x20, sizeof(m_fw));
		m_owned[0] = m_owned[1] = 0;
		set_busy(410);
	}
}

u8 hd44780_device::control_r() const
{
	if (m_data_len == 4) {
		if (m_nibble)
			return u8((busy() ? 0x80 : 0) | (m_ac & 0x70));
		return u8((m_ac << 4) & 0xf0);
	}
	return u8((busy() ? 0x80 : 0) | (m_ac & 0x7f));
}

void hd44780_device::data_w(u8 data)
{
	if (m_data_len == 4) {
		if (m_nibble) { m_dr = data & 0xf0; return; }
		m_dr |= (data >> 4) & 0x0f;
	} else
		m_dr = data;

	if (m_active_ram == DDRAM) {
		// **native の持ち物のマスは表示を変えない**（6.188）
		m_fw[m_ac] = m_dr;
		if (!owned(u32(m_ac))) {
			if (m_ddram[m_ac] != m_dr)
				note_change(false, m_ac, m_ddram[m_ac], m_dr);
			m_ddram[m_ac] = m_dr;
		}
	} else {
		m_cg_fw[m_ac] = m_dr;
		if (!cg_owned(u32(m_ac))) {
			if (m_cgram[m_ac] != m_dr)
				note_change(true, m_ac, m_cgram[m_ac], m_dr);
			m_cgram[m_ac] = m_dr;
		}
	}

	set_busy(10);
	update_ac(m_direction);
	if (m_shift_on)
		shift_display(m_direction);
}

u8 hd44780_device::data_r()
{
	// firmware が読み返すのは、firmware が書いた値（6.188）
	u8 data = (m_active_ram == DDRAM) ? m_fw[m_ac] : m_cg_fw[m_ac];

	if (m_data_len == 4) {
		if (m_nibble)
			return data & 0xf0;
		data = u8((data << 4) & 0xf0);
	}

	set_busy(10);
	update_ac(m_direction);
	return data;
}


// MAME の hd44780_base_device::render() と同じ。カーソルの点滅は追っていない
// （音には関わらず、こちらは時計を持たないため）
const u8 *hd44780_device::render()
{
	std::memset(m_render_buf, 0, sizeof(m_render_buf));
	if (!m_display_on || !m_cgrom)
		return m_render_buf;

	const int line_size = 80 / m_num_line;
	for (int line = 0; line < m_num_line; line++) {
		for (int pos = 0; pos < line_size; pos++) {
			const u16 char_pos = u16(line * 0x40 + ((pos + m_disp_shift) % line_size));

			const u8 *src;
			if (m_ddram[char_pos] < 0x10) {
				// 0x00-0x0f は利用者が作った字。CGRAM から引く
				if (m_char_size == 8)
					src = m_cgram + (m_ddram[char_pos] & 0x07) * 8;
				else
					src = m_cgram + ((m_ddram[char_pos] >> 1) & 0x03) * 16;
			} else {
				src = m_cgrom + m_ddram[char_pos] * 0x10;
			}

			u8 *dest = m_render_buf + 16 * (line * line_size + pos);
			std::memcpy(dest, src, size_t(m_char_size));

			if (char_pos == m_ac && m_cursor_on)
				dest[m_char_size - 1] = 0x1f;
		}
	}
	return m_render_buf;
}

// 状態の保存と復元。字の絵（m_cgrom）は ROM なので入れない
void hd44780_device::state(state_io &s)
{
	s.tag("lcd");
	s.v(m_now); s.v(m_busy_until);
	s.arr(m_render_buf); s.arr(m_ddram); s.arr(m_cgram);
	// **native の持ち物**（版 11 から。6.188）。古い記録には無いので、
	// そのときは「誰も持っていない」に戻す
	if (s.version() >= 11) {
		s.arr(m_fw); s.v(m_owned[0]); s.v(m_owned[1]);
	} else {
		std::memcpy(m_fw, m_ddram, sizeof(m_fw));
		m_owned[0] = m_owned[1] = 0;
	}
	if (s.version() >= 12) {
		s.arr(m_cg_fw); s.v(m_cg_owned);
	} else {
		std::memcpy(m_cg_fw, m_cgram, sizeof(m_cg_fw));
		m_cg_owned = 0;
	}
	s.v(m_ac); s.v(m_active_ram); s.v(m_direction); s.v(m_disp_shift);
	s.v(m_num_line); s.v(m_char_size); s.v(m_data_len);
	s.v(m_shift_on); s.v(m_display_on); s.v(m_cursor_on); s.v(m_blink_on);
	s.v(m_nibble); s.v(m_ir); s.v(m_dr);
}
