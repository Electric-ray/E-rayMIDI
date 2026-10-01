// license:BSD-3-Clause
//
// SmartMedia（NAND フラッシュのカード）。MU2000 の前面のカードの差し込み口。
//
// firmware は 0xD00000 の制御の留め金と 0xC00000 のデータの口で、NAND の命令をそのまま送る
// （エミュで読み書きを記録して決めた）。留め金のビットは:
//   bit 5 カードの電源 / bit 1 選ばない / bit 0 選ぶ / bit 3 命令の留め（CLE） / bit 2 番地の留め（ALE）
// 差し込まれているかは CPU のポート A の bit 19（PA19、1 で差し込み）で見る。
//
// 中身は NAND の生の並び（1 ページ 512 バイト + 予備 16 バイト、32 ページで 1 ブロック）。
// 新しく作るカードには、店で売っている SmartMedia と同じ物理の書式（先頭のブロックの CIS）だけを入れておく。
// 論理の並び（ブロックの番地の割り当てと FAT）は firmware の書式化（UTIL → CARD → Format）が書く。
// 忙しい印・差し込み・書き込み禁止は CPU のポート A の PA18 / PA19 / PA20（mu2000.cpp）。
//
// 命令は NAND の公開された命令の組（読む 00/01/50、書く 80+10、消す 60+D0、状態 70、名乗り 90、リセット FF）だけ。

#ifndef S_MU2000_SMARTMEDIA_H
#define S_MU2000_SMARTMEDIA_H

#pragma once

#include "compat/mamecompat.h"

#include <algorithm>
#include <string>
#include <vector>

class state_io;

namespace smu2000 {

class smartmedia
{
public:
	static constexpr u32 PAGE = 512, SPARE = 16, PAGES_PER_BLOCK = 32;

	// megabytes は 16 / 32 / 64 / 128。中身は全部 0xFF（書式化されていない）
	bool create(u32 megabytes);
	// 生の並びのファイルを読む / 書く。大きさから容量を決める
	bool load(const std::string &path, std::string &err);
	bool save(const std::string &path, std::string &err) const;
	void eject() { m_data.clear(); m_pages = 0; m_dirty_blocks.clear(); m_dirty = false; }

	bool inserted() const { return m_pages != 0; }
	bool dirty() const { return m_dirty; }
	void clear_dirty() { m_dirty = false; std::fill(m_dirty_blocks.begin(), m_dirty_blocks.end(), u8(0)); }

	// 書き換えたブロックだけをファイルへ書き戻す。音の糸を長く止めないように、
	// take_dirty_blocks（機械を止めて写す、1 ブロック 17KB）と write_blocks（止めずに書く）に分けてある
	struct block { u32 index; std::vector<u8> bytes; };
	void take_dirty_blocks(std::vector<block> &out);
	static bool write_blocks(const std::string &path, const std::vector<block> &blocks, std::string &err);
	bool write_protected = false;
	u32 megabytes() const { return m_pages * PAGE / (1024 * 1024); }

	// firmware の口
	void control_w(u8 v);
	void data_w(u8 v);
	u8 data_r();

	// 状態の保存。**カードの中身は入れない**（外の記憶なので、ファイルで持つ）
	void state(state_io &s);

	const std::vector<u8> &raw() const { return m_data; }

private:
	enum class mode : u8 { idle, read, read_id, status, program, erase };

	u32 page_bytes() const { return PAGE + SPARE; }
	void mark_dirty(u32 page);
	u32 address_cycles() const { return m_pages > 65536 ? 4 : 3; }   // 32MB までは 3 回、64MB からは 4 回
	void command(u8 c);
	void address(u8 a);

	std::vector<u8> m_data;
	u32 m_pages = 0;
	u8  m_device_code = 0;
	bool m_dirty = false;
	std::vector<u8> m_dirty_blocks;   // ブロックごとの書き換えた印

	// 留め金と命令の途中の状態
	u8   m_ctrl = 0;
	mode m_mode = mode::idle;
	u8   m_pointer = 0;           // 00（ページの頭）/ 01（後ろ半分）/ 50（予備）
	u8   m_addr_count = 0;
	u32  m_column = 0;            // 1 ページの中の位置（0-527）
	u32  m_page = 0;
	u32  m_id_pos = 0;
	u8   m_last_cmd = 0;
	std::vector<u8> m_buf;        // 書く命令のあいだ溜める 528 バイト
};

} // namespace smu2000

#endif // S_MU2000_SMARTMEDIA_H
