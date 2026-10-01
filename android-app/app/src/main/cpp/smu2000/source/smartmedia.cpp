// license:BSD-3-Clause

#include "smartmedia.h"
#include "state.h"

#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace smu2000 {

namespace {

constexpr u8 MAKER_TOSHIBA = 0x98;

// 名前は UTF-8。Windows の fopen は ANSI のコードページで読むので、日本語の名前は wide で開く
std::FILE *open_file(const std::string &path, const char *mode)
{
#ifdef _WIN32
	const int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
	if (n > 0) {
		std::wstring w(size_t(n), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
		std::wstring m;
		for (const char *c = mode; *c; c++)
			m += wchar_t(*c);
		return _wfopen(w.c_str(), m.c_str());
	}
#endif
	return std::fopen(path.c_str(), mode);
}

// 3.3V の SmartMedia の名乗りの装置番号
u8 device_code_for(u32 megabytes)
{
	switch (megabytes) {
	case 16:  return 0x73;
	case 32:  return 0x75;
	case 64:  return 0x76;
	case 128: return 0x79;
	default:  return 0;
	}
}

// SmartMedia の ECC（256 バイトに 3 バイト、22bit のハミング符号）。
// 列の偶奇 6bit と、奇数個の 1 を持つバイトの位置の排他的論理和から作り、反転して入れる
void ecc256(const u8 *d, u8 out[3])
{
	u8 reg1 = 0, reg2 = 0, reg3 = 0;
	for (int j = 0; j < 256; j++) {
		const u8 b = d[j];
		auto bit = [b](int k) { return (b >> k) & 1; };
		const u8 cp = u8((bit(0) ^ bit(2) ^ bit(4) ^ bit(6)) |
		                 ((bit(1) ^ bit(3) ^ bit(5) ^ bit(7)) << 1) |
		                 ((bit(0) ^ bit(1) ^ bit(4) ^ bit(5)) << 2) |
		                 ((bit(2) ^ bit(3) ^ bit(6) ^ bit(7)) << 3) |
		                 ((bit(0) ^ bit(1) ^ bit(2) ^ bit(3)) << 4) |
		                 ((bit(4) ^ bit(5) ^ bit(6) ^ bit(7)) << 5));
		int par = 0;
		for (int k = 0; k < 8; k++)
			par ^= bit(k);
		reg1 ^= cp;
		if (par) {
			reg3 ^= u8(j);
			reg2 ^= u8(~j);
		}
	}
	u8 t1 = 0, t2 = 0, a = 0x80, bm = 0x80;
	for (int i = 0; i < 4; i++) {
		if (reg3 & a) t1 |= bm;
		bm >>= 1;
		if (reg2 & a) t1 |= bm;
		bm >>= 1;
		a >>= 1;
	}
	bm = 0x80;
	for (int i = 0; i < 4; i++) {
		if (reg3 & a) t2 |= bm;
		bm >>= 1;
		if (reg2 & a) t2 |= bm;
		bm >>= 1;
		a >>= 1;
	}
	const u8 c0 = u8(~t1), c1 = u8(~t2);
	// SmartMedia の並びは、行の偶奇の 2 バイトが入れ替わる
	out[0] = c1;
	out[1] = c0;
	out[2] = u8(((~reg1) << 2) | 0x03);
}

} // namespace

bool smartmedia::create(u32 megabytes)
{
	const u8 code = device_code_for(megabytes);
	if (!code)
		return false;
	m_pages = megabytes * 1024 * 1024 / PAGE;
	m_device_code = code;
	m_data.assign(size_t(m_pages) * page_bytes(), 0xff);
	m_dirty_blocks.assign(m_pages / PAGES_PER_BLOCK, 1);
	// 物理の書式（SSFDC）。店で売っている SmartMedia は最初から、先頭の良いブロックに CIS が書いてある。
	// MU2000 の書式化はこれを探してから FAT を書くので、CIS が無いと「Bad Card!」になる。
	// firmware が見るのは CIS の頭の 10 バイトだけ。予備の領域はブロックの番地を 0000 にし、ECC を入れる
	static const u8 cis_head[10] = { 0x01, 0x03, 0xd9, 0x01, 0xff, 0x18, 0x02, 0xdf, 0x01, 0x20 };
	for (u32 pg = 0; pg < 2; pg++) {
		u8 *p = m_data.data() + size_t(pg) * page_bytes();
		std::fill(p, p + PAGE, u8(0xff));
		std::copy(cis_head, cis_head + 10, p);
		u8 *sp = p + PAGE;
		u8 e1[3], e2[3];
		ecc256(p, e1);
		ecc256(p + 256, e2);
		sp[6] = sp[7] = 0x00;
		sp[11] = sp[12] = 0x00;
		sp[8] = e2[0]; sp[9] = e2[1]; sp[10] = e2[2];
		sp[13] = e1[0]; sp[14] = e1[1]; sp[15] = e1[2];
	}
	// CIS のブロックの残りのページも、番地は 0000
	for (u32 pg = 2; pg < PAGES_PER_BLOCK; pg++) {
		u8 *sp = m_data.data() + size_t(pg) * page_bytes() + PAGE;
		sp[6] = sp[7] = sp[11] = sp[12] = 0x00;
	}
	m_dirty = true;
	return true;
}

bool smartmedia::load(const std::string &path, std::string &err)
{
	std::FILE *f = open_file(path, "rb");
	if (!f) {
		err = "カードのファイルを開けない: " + path;
		return false;
	}
	std::fseek(f, 0, SEEK_END);
	const long size = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	u32 mb = 0;
	for (u32 m : { 16u, 32u, 64u, 128u })
		if (size_t(size) == size_t(m) * 1024 * 1024 / PAGE * (PAGE + SPARE))
			mb = m;
	if (!mb) {
		std::fclose(f);
		err = "カードのファイルの大きさが 16/32/64/128MB の SmartMedia と合わない: " + path;
		return false;
	}
	create(mb);
	const size_t got = std::fread(m_data.data(), 1, m_data.size(), f);
	std::fclose(f);
	if (got != m_data.size()) {
		eject();
		err = "カードのファイルを読み切れない: " + path;
		return false;
	}
	clear_dirty();
	return true;
}

void smartmedia::mark_dirty(u32 page)
{
	const u32 b = page / PAGES_PER_BLOCK;
	if (b < m_dirty_blocks.size())
		m_dirty_blocks[b] = 1;
	m_dirty = true;
}

void smartmedia::take_dirty_blocks(std::vector<block> &out)
{
	out.clear();
	const size_t bytes = size_t(PAGES_PER_BLOCK) * page_bytes();
	for (u32 b = 0; b < m_dirty_blocks.size(); b++) {
		if (!m_dirty_blocks[b])
			continue;
		const auto from = m_data.begin() + ptrdiff_t(size_t(b) * bytes);
		out.push_back({ b, std::vector<u8>(from, from + ptrdiff_t(bytes)) });
		m_dirty_blocks[b] = 0;
	}
	m_dirty = false;
}

bool smartmedia::write_blocks(const std::string &path, const std::vector<block> &blocks, std::string &err)
{
	if (blocks.empty())
		return true;
	std::FILE *f = open_file(path, "r+b");
	if (!f) {
		err = "カードのファイルに書き戻せない: " + path;
		return false;
	}
	bool ok = true;
	for (const block &b : blocks) {
		const long at = long(b.index) * long(PAGES_PER_BLOCK) * long(PAGE + SPARE);
		if (std::fseek(f, at, SEEK_SET) != 0 || std::fwrite(b.bytes.data(), 1, b.bytes.size(), f) != b.bytes.size())
			ok = false;
	}
	if (std::fclose(f) != 0)
		ok = false;
	if (!ok)
		err = "カードのファイルに書き戻し切れない: " + path;
	return ok;
}

bool smartmedia::save(const std::string &path, std::string &err) const
{
	std::FILE *f = open_file(path, "wb");
	if (!f) {
		err = "カードのファイルを書けない: " + path;
		return false;
	}
	const size_t put = std::fwrite(m_data.data(), 1, m_data.size(), f);
	std::fclose(f);
	if (put != m_data.size()) {
		err = "カードのファイルを書き切れない: " + path;
		return false;
	}
	return true;
}

void smartmedia::control_w(u8 v)
{
	m_ctrl = v;
}

void smartmedia::data_w(u8 v)
{
	if (!inserted() || !(m_ctrl & 0x01))
		return;
	if (m_ctrl & 0x08) {
		command(v);
	} else if (m_ctrl & 0x04) {
		address(v);
	} else if (m_mode == mode::program) {
		if (m_column < page_bytes())
			m_buf[m_column++] = v;
	}
}

u8 smartmedia::data_r()
{
	if (!inserted() || !(m_ctrl & 0x01))
		return 0xff;
	switch (m_mode) {
	case mode::read_id: {
		const u8 id[2] = { MAKER_TOSHIBA, m_device_code };
		return id[m_id_pos++ % 2];
	}
	case mode::status:
		// bit 7 は書き込みを禁じていない、bit 6 は準備ができている、bit 0 は失敗
		return u8((write_protected ? 0x00 : 0x80) | 0x40);
	case mode::read: {
		if (m_page >= m_pages)
			return 0xff;
		const u8 v = m_data[size_t(m_page) * page_bytes() + m_column];
		m_column++;
		if (m_column >= page_bytes()) {
			// 次のページへ続けて読む。予備だけを読む命令（50）なら次の予備から
			m_page++;
			m_column = (m_pointer == 0x50) ? PAGE : 0;
			if (m_pointer == 0x01)
				m_pointer = 0x00;
		}
		return v;
	}
	default:
		return 0xff;
	}
}

void smartmedia::command(u8 c)
{
	m_last_cmd = c;
	switch (c) {
	case 0xff:                                   // リセット
		m_mode = mode::idle;
		m_pointer = 0x00;
		break;
	case 0x00: case 0x01: case 0x50:             // 読む（ページの頭 / 後ろ半分 / 予備）
		m_pointer = c;
		m_mode = mode::read;
		m_addr_count = 0;
		break;
	case 0x80:                                   // 書く中身を受け取り始める
		m_mode = mode::program;
		m_addr_count = 0;
		m_buf.assign(page_bytes(), 0xff);
		m_column = (m_pointer == 0x01) ? 256 : (m_pointer == 0x50) ? PAGE : 0;
		break;
	case 0x10:                                   // 書く。NAND は 1 を 0 にしかできない
		if (m_mode == mode::program && !write_protected && m_page < m_pages) {
			const size_t base = size_t(m_page) * page_bytes();
			for (u32 i = 0; i < page_bytes(); i++)
				m_data[base + i] &= m_buf[i];
			mark_dirty(m_page);
		}
		if (m_pointer == 0x01)
			m_pointer = 0x00;
		m_mode = mode::status;
		break;
	case 0x60:                                   // 消すブロックの番地を受け取り始める
		m_mode = mode::erase;
		m_addr_count = 0;
		break;
	case 0xd0:                                   // 消す
		if (m_mode == mode::erase && !write_protected) {
			const u32 block = m_page / PAGES_PER_BLOCK;
			const size_t base = size_t(block) * PAGES_PER_BLOCK * page_bytes();
			if (base < m_data.size())
				std::fill(m_data.begin() + base, m_data.begin() + std::min(m_data.size(), base + size_t(PAGES_PER_BLOCK) * page_bytes()), u8(0xff));
			mark_dirty(m_page);
		}
		m_mode = mode::status;
		break;
	case 0x70:                                   // 状態
		m_mode = mode::status;
		break;
	case 0x90:                                   // 名乗り
		m_mode = mode::read_id;
		m_id_pos = 0;
		break;
	default:
		break;
	}
}

void smartmedia::address(u8 a)
{
	switch (m_mode) {
	case mode::read:
	case mode::program:
		if (m_addr_count == 0) {
			m_column = a + ((m_pointer == 0x01) ? 256 : (m_pointer == 0x50) ? PAGE : 0);
			if (m_column >= page_bytes())
				m_column = page_bytes() - 1;
			m_page = 0;
		} else {
			const u32 shift = 8 * (m_addr_count - 1);
			m_page = (m_page & ~(u32(0xff) << shift)) | (u32(a) << shift);
		}
		m_addr_count++;
		break;
	case mode::erase: {
		// 消すときは列の番地が無く、ページの番地だけ（32MB までは 2 回、64MB からは 3 回）
		const u32 shift = 8 * m_addr_count;
		if (m_addr_count == 0)
			m_page = 0;
		m_page = (m_page & ~(u32(0xff) << shift)) | (u32(a) << shift);
		m_addr_count++;
		break;
	}
	default:
		break;
	}
}

void smartmedia::state(state_io &s)
{
	s.v(m_ctrl);
	u8 md = u8(m_mode);
	s.v(md);
	m_mode = mode(md);
	s.v(m_pointer); s.v(m_addr_count); s.v(m_column); s.v(m_page); s.v(m_id_pos); s.v(m_last_cmd);
	u32 n = u32(m_buf.size());
	s.v(n);
	if (!s.writing())
		m_buf.resize(std::min<u32>(n, 4096));
	if (!m_buf.empty())
		s.mem(m_buf.data(), m_buf.size());
}

} // namespace smu2000
