// license:BSD-3-Clause
//
// 音色の名前と楽器の絵を、利用者のプログラム ROM から読む（doc/pc-editor.md）。
//
// 名前も絵もこのリポジトリには入れない。実行時に ROM の中を見るだけ。
// 番地は MU2000 EX（firmware v2.01）で調べたもの。違う版の ROM では ok() が false になり、
// 画面は GM の名前に戻る。
//
// 調べ方（doc/pc-editor.md の「音色の名前と絵」）:
//   * パートの塊の +0xF8 に、firmware が選んだ音色の記録を指す値がある（ROM の中）。
//     記録は「要素の印 1 バイト・1 バイト・名前 10 文字」で始まる
//   * ドラムキットはその値が 0。プログラム → キットの番号の表と、名前 8 文字＋4 バイトの並びを引く
//   * 楽器の絵は 16×16 ドットを 16 ワードで持つ。通常の音色はプログラム番号 → 絵の番号の表で決まる。
//     ドラムキットは全キットで 1 つの絵（DRUM_ICON）

#ifndef S_MU2000_XG_VOICES_H
#define S_MU2000_XG_VOICES_H

#pragma once

#include "compat/mamecompat.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace xg {

class voice_rom
{
public:
	static constexpr u32 PART_VOICE = 0xf8;      // パートの塊の中の、音色の記録を指す値

	explicit voice_rom(std::shared_ptr<const std::vector<u8>> rom) : m_rom(std::move(rom))
	{
		// 版の確かめ。どれか 1 つでも違えば使わない
		m_ok = m_rom && m_rom->size() == 0x400000 &&
		       !std::memcmp(at(VOICES + 2), "GrandPno", 8) &&
		       !std::memcmp(at(KIT_NAMES), "StandKit", 8) &&
		       !std::memcmp(at(SFX_NAMES), "SFXKit 1", 8) &&
		       word(ICONS) == 0xfffc;
	}

	bool ok() const { return m_ok; }
	// ROM の中身（音色の要素の記録を読むとき。xg/native_voice.h の element）
	const u8 *data() const { return m_ok ? m_rom->data() : nullptr; }
	// パートの塊（ワーク RAM の写し）から、選んでいる音色の記録の番地。無ければ 0
	u32 voice_record(const u8 *part_ram) const { return m_ok ? record(part_ram) : 0; }

	// 名前。part_ram はパートの塊（+0xF8 まで含む）。分からなければ空
	std::string name(const u8 *part_ram, int msb, int prog) const
	{
		if (!m_ok)
			return {};
		if (msb == 127 || msb == 126) {
			const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
			const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
			const u8 idx = (*m_rom)[map + (prog & 0x7f)];
			std::string s(reinterpret_cast<const char *>(at(names + idx * 12)), 8);
			return trim(s);
		}
		const u32 rec = record(part_ram);
		if (!rec)
			return {};
		return trim(std::string(reinterpret_cast<const char *>(at(rec + 2)), 10));
	}

	// バンクとプログラムから音色の記録を引く。firmware の 0x134AA8 と同じ手順。
	// mode は RAM の 0x4226BC、set は 0x4226DE（xg::ram::VOICE_MODE / VOICE_SET）。
	// 引けなければ 0（ドラム、MSB 16 の特別な組など）
	// **その LSB を実機が受け付けるか**（6.202）。組の表で
	// **LSB 0 と同じ組**になる LSB（0 以外）は丸ごと無視され、
	// 前のバンクのままになる（番号はそのまま効く）
	bool lsb_ok(int mode, int set, int msb, int lsb) const
	{
		if (!m_ok || mode != 1)
			return true;
		msb &= 0x7f; lsb &= 0x7f;
		if (lsb == 0)
			return true;
		const u8 kind = byte(GROUP_XG + u32(msb));
		u32 tab;
		if (kind == 0)
			tab = set == 0 ? GROUP_LSB0 : GROUP_LSB1;
		else if (kind == 77)
			tab = GROUP_LSB77;
		else if (kind == 0xc9)
			tab = set == 0 ? GROUP_LSBC9_0 : GROUP_LSBC9_1;
		else
			return true;              // LSB を見ない組
		return byte(tab + u32(lsb)) != byte(tab);
	}

	u32 lookup(int mode, int set, int msb, int lsb, int prog) const
	{
		if (!m_ok)
			return 0;
		msb &= 0x7f; lsb &= 0x7f; prog &= 0x7f;
		u32 group = 76;
		if (mode == 0) {
			const u8 g = byte(GROUP_GM + msb);
			group = g == 0xff ? 76 : g;
		} else if (mode == 1) {
			if (msb == 16 && lsb < 2)
				return 0;                        // firmware は別の関数へ行く。まだ真似していない
			const u8 kind = byte(GROUP_XG + msb);
			if (kind == 0)
				group = byte((set == 0 ? GROUP_LSB0 : GROUP_LSB1) + lsb);
			else if (kind == 77)
				group = byte(GROUP_LSB77 + lsb);
			else if (kind == 0xc9)
				group = byte((set == 0 ? GROUP_LSBC9_0 : GROUP_LSBC9_1) + lsb);
			else
				group = kind;
		}
		const u32 slot = VOICE_TABLE + group * 512 + u32(prog) * 4;
		if (slot + 4 > m_rom->size())
			return 0;
		const u32 off = u32(byte(slot)) << 24 | u32(byte(slot + 1)) << 16 | u32(byte(slot + 2)) << 8 | byte(slot + 3);
		const u32 rec = VOICES + off * 2;
		return rec >= VOICES && rec + 16 <= VOICES_END ? rec : 0;
	}

	// **ドラムの 1 打の記録**を引く（firmware の 0x134DB8）。
	//   キットの番号 = パートの塊の +0x110
	//   表の先頭     = rd32(0x292250 + キット * 4)
	//   ずれ         = rd16(表の先頭 + 鍵 * 2)   （0xFFFF なら鳴らない）
	//   記録         = 0x283DD0 + ずれ
	// 実機が鳴らした値と突き合わせて確かめた（Standard Kit はキット番号 36 で、
	// 鍵 36→284196・鍵 38→28C2DE・鍵 42→28C332）。
	// **記録の並びは旋律の要素（84 バイト）とは別**で、まだ解いていない
	// （フィルタの第 2 係数が +20 にある。旋律は +80。doc の 6.85）
	u32 drum_record(int kit, int note) const
	{
		// **bit7 が立っているときは別の道**（0x134DF0）。あちらは**ドラム
		// セットアップ**の引き方で（4 組 × 79 鍵、DRAM の 0x01051A28 から
		// 42 バイトずつ）、1 打の記録ではない。SFX キットはこの道を
		// キット番号 47・48 で通る（doc/native-engine.md の 6.234）
		if (!m_ok || (kit & 0x80))
			return 0;
		const u32 base = rd32(DRUM_KIT_TABLE + u32(kit & 0x7f) * 4);
		if (base < 0x200000 || base + 256 > m_rom->size())
			return 0;
		const u16 off = word(base + u32(note & 0x7f) * 2);
		return off == 0xffff ? 0 : DRUM_RECORDS + off;
	}

	// 記録の名前（lookup の戻り値から）
	std::string record_name(u32 rec) const
	{
		if (!m_ok || rec < VOICES || rec + 12 > VOICES_END)
			return {};
		return trim(std::string(reinterpret_cast<const char *>(at(rec + 2)), 10));
	}

	// ドラムキットの名前。無いキット（SilenKit を指すもの）は空
	std::string kit_name(int msb, int prog) const
	{
		if (!m_ok || (msb != 127 && msb != 126))
			return {};
		const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
		const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
		const u8 idx = byte(map + (prog & 0x7f));
		const std::string s = trim(std::string(reinterpret_cast<const char *>(at(names + idx * 12)), 8));
		return s == "SilenKit" ? std::string() : s;
	}

	// **ドラムの鍵ごとの楽器名**（12 文字。LCD のドラムセットアップの画面に出る名前）。
	// キットの記録（名前 8 文字の後ろの 4 バイト）が、鍵 13-91 の名前の表（12 文字 × 79）を指している。
	// バンク 127 の 32 キットとバンク 126（SFX）の 9 キットすべてで引ける。音の無い鍵は
	// 「************」なので空を返す。キットでない（msb が 126・127 でない）ときも空
	std::string drum_key_name(int msb, int prog, int key) const
	{
		if (!m_ok || (msb != 127 && msb != 126) || key < DRUM_KEY0 || key >= DRUM_KEY0 + DRUM_KEYS)
			return {};
		const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
		const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
		const u32 table = rd32(names + u32(byte(map + u32(prog & 0x7f))) * 12 + 8);
		if (table < 0x200000 || table + DRUM_KEYS * 12 > m_rom->size())
			return {};
		std::string s(reinterpret_cast<const char *>(at(table + u32(key - DRUM_KEY0) * 12)), 12);
		for (char c : s)
			if (c < 0x20 || c > 0x7e)
				return {};
		if (s == "************")
			return {};
		return trim(s);
	}

	// 楽器の絵。16 行、各行 16 ビット（上の桁が左）。無ければ false
	// **演奏画面に出る 8 文字**（6.190）。記録の名前（10 文字）の
	// 頭 8 をそのまま。ドラム（MSB 126・127）はキットの名前 8 文字。
	// 引けなければ空（呼ぶ側が firmware に任せる）
	std::string screen_name(u32 rec, int msb, int prog) const
	{
		if (!m_ok)
			return {};
		if (msb == 127 || msb == 126) {
			const u32 map = msb == 127 ? KIT_MAP : SFX_MAP;
			const u32 names = msb == 127 ? KIT_NAMES : SFX_NAMES;
			const u8 idx = (*m_rom)[map + u32(prog & 0x7f)];
			return std::string(
			    reinterpret_cast<const char *>(at(names + u32(idx) * 12)), 8);
		}
		if (rec < VOICES || rec + 16 > VOICES_END)
			return {};
		return std::string(reinterpret_cast<const char *>(at(rec + 2)), 8);
	}

	// **楽器の絵**を記録から直に引く（6.190）。
	// `icon` はパートの塊から記録を読み直すが、native の口では
	// その塊を firmware が書くまで遅れるので、こちらの記録を使う
	bool icon_of(u32 rec, int msb, int prog, u16 rows[16]) const
	{
		if (!m_ok)
			return false;
		if (msb == 127 || msb == 126) {
			if (word(DRUM_ICON) != 0x000e)
				return false;
			for (int y = 0; y < 16; y++)
				rows[y] = word(DRUM_ICON + u32(y) * 2) & 0xfffe;
			return true;
		}
		if (rec < VOICES || rec + 16 > VOICES_END)
			return false;
		int index;
		if (!std::memcmp(at(rec + 2), "Silence", 7))
			index = 56;
		else if (msb == 64)
			index = 57;
		else
			index = (*m_rom)[ICON_OF_PROGRAM + u32(prog & 0x7f)];
		const u32 p = ICONS + u32(index) * 32;
		for (int y = 0; y < 16; y++)
			rows[y] = word(p + u32(y) * 2);
		return true;
	}

	bool icon(const u8 *part_ram, int msb, int prog, u16 rows[16]) const
	{
		if (!m_ok)
			return false;
		if (msb == 127 || msb == 126) {
			// ドラムキットはどのキットも同じ絵。LCD は右端の 1 列を出さないので、そこは消す
			if (word(DRUM_ICON) != 0x000e)
				return false;                              // 版が違う
			for (int y = 0; y < 16; y++)
				rows[y] = word(DRUM_ICON + u32(y) * 2) & 0xfffe;
			return true;
		}
		const u32 rec = record(part_ram);
		if (!rec)
			return false;
		int index;
		if (!std::memcmp(at(rec + 2), "Silence", 7))
			index = 56;
		else if (msb == 64)
			index = 57;                                // 効果音
		else
			index = (*m_rom)[ICON_OF_PROGRAM + (prog & 0x7f)];
		const u32 p = ICONS + u32(index) * 32;
		for (int y = 0; y < 16; y++)
			rows[y] = word(p + u32(y) * 2);
		return true;
	}

private:
	static constexpr u32 VOICES          = 0x200ee0;   // 音色の記録の並び
	static constexpr u32 VOICES_END      = 0x23cece;
	static constexpr u32 KIT_MAP         = 0x299fcc;   // バンク 127: プログラム → キットの番号
	static constexpr u32 KIT_NAMES       = 0x299dc0;   //   名前 8 文字 + 4 バイト
	static constexpr u32 SFX_MAP         = 0x29be58;   // バンク 126
	static constexpr u32 SFX_NAMES       = 0x29bdec;
	static constexpr int DRUM_KEY0 = 13, DRUM_KEYS = 79;   // 鍵ごとの名前の表の範囲
	static constexpr u32 ICON_OF_PROGRAM = 0x1cd044;   // プログラム → 絵の番号
	static constexpr u32 ICONS           = 0x1bbf70;   // 絵。16 ワードずつ
	// ドラムキットの絵。プログラム → 絵の番号の表のすぐ後ろに、起動のときの動く絵のコマが並んでいて、
	// その 2 コマ目。キットを選んだときに firmware が LCD の CGRAM に書いた絵と突き合わせて見つけた
	static constexpr u32 DRUM_ICON       = 0x1cd0e4;

	// バンク → 音色の組（firmware の 0x134AA8 が引く表）
	static constexpr u32 GROUP_GM       = 0x283d50;   // GM モード: MSB → 組
	static constexpr u32 GROUP_XG       = 0x283950;   // XG モード: MSB → 組（0 / 77 / 0xC9 は LSB で引き直す）
	static constexpr u32 GROUP_LSB0     = 0x2839d0;
	static constexpr u32 GROUP_LSB1     = 0x283a50;
	static constexpr u32 GROUP_LSB77    = 0x283ad0;
	static constexpr u32 GROUP_LSBC9_0  = 0x292640;
	static constexpr u32 GROUP_LSBC9_1  = 0x2926c0;
	static constexpr u32 VOICE_TABLE    = 0x267f50;   // 組 × 128 プログラム。値の 2 倍が VOICES からの距離
	// ドラム（firmware の 0x134DB8）
	static constexpr u32 DRUM_KIT_TABLE = 0x292250;   // キット → 鍵ごとのずれの表（4 バイト）
	static constexpr u32 DRUM_RECORDS   = 0x283dd0;   // ずれの元になる番地

	u8 byte(u32 a) const { return (*m_rom)[a]; }
	const u8 *at(u32 a) const { return m_rom->data() + a; }
	u16 word(u32 a) const { return u16((*m_rom)[a] << 8 | (*m_rom)[a + 1]); }
	u32 rd32(u32 a) const
	{
		return u32((*m_rom)[a]) << 24 | u32((*m_rom)[a + 1]) << 16 |
		       u32((*m_rom)[a + 2]) << 8 | (*m_rom)[a + 3];
	}

	u32 record(const u8 *part_ram) const
	{
		const u32 r = u32(part_ram[PART_VOICE]) << 24 | u32(part_ram[PART_VOICE + 1]) << 16 |
		              u32(part_ram[PART_VOICE + 2]) << 8 | part_ram[PART_VOICE + 3];
		if (r < VOICES || r + 16 > VOICES_END)
			return 0;
		return r;
	}

	static std::string trim(std::string s)
	{
		while (!s.empty() && (s.back() == ' ' || s.back() == 0))
			s.pop_back();
		return s;
	}

	std::shared_ptr<const std::vector<u8>> m_rom;
	bool m_ok = false;
};

} // namespace xg

#endif // S_MU2000_XG_VOICES_H
