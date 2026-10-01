// license:BSD-3-Clause
//
// 状態の保存と復元。DAW のプロジェクトに音色やエフェクトを覚えさせるため、
// **機械まるごと**（CPU・RAM・SWP30・LCD・タイマ）を書き出して読み戻す。
//
// 書くのと読むのを**同じ 1 本の関数**でやる。別々に書くと、片方だけ直して
// ずれるのがいちばん怖いので、その道を塞いでおく。
//
//   void なにか::state(state_io &s) { s.v(m_a); s.v(m_b); s.arr(m_c); }
//
// 目印（tag）を挟んでおくと、読むときに食い違いをその場で見つけられる。
//
// 正しさの確かめ方は「保存して戻した続きの音が、保存せず走り続けた音と
// 1 バイトも違わないこと」。tools/state_test.py がそれをやる。

#ifndef S_MU2000_STATE_H
#define S_MU2000_STATE_H

#pragma once

#include "compat/mamecompat.h"

#include <cstring>
#include <string>
#include <vector>

class state_io
{
public:
	// 書く側
	explicit state_io(std::vector<u8> &out) : m_out(&out) {}
	// 読む側
	state_io(const u8 *p, size_t n) : m_in(p), m_len(n) {}

	bool writing() const { return m_out != nullptr; }
	// 状態の形の版。読むときは先頭で読んだ版、書くときは今の版を入れておく
	u32 version() const { return m_version; }
	void set_version(u32 v) { m_version = v; }
	bool ok() const      { return m_ok; }
	const std::string &error() const { return m_err; }

	// 生のバイト列。書くときは足し、読むときは埋める
	void raw(void *p, size_t n)
	{
		if (!m_ok)
			return;
		if (m_out) {
			const u8 *b = static_cast<const u8 *>(p);
			m_out->insert(m_out->end(), b, b + n);
		} else {
			if (m_at + n > m_len) {
				fail("足りない");
				return;
			}
			std::memcpy(p, m_in + m_at, n);
			m_at += n;
		}
	}

	// 値ひとつ。配列でもそのまま渡せる
	template <typename T> void v(T &x)
	{
		static_assert(std::is_trivially_copyable<T>::value, "そのまま写せない型");
		raw(&x, sizeof(T));
	}

	// std::array / 生の配列
	template <typename T, size_t N> void arr(T (&a)[N])
	{
		static_assert(std::is_trivially_copyable<T>::value, "そのまま写せない型");
		raw(a, sizeof(a));
	}
	template <typename A> void stdarr(A &a)
	{
		static_assert(std::is_trivially_copyable<typename A::value_type>::value,
		              "そのまま写せない型");
		raw(a.data(), a.size() * sizeof(typename A::value_type));
	}

	// 大きさの決まっている入れ物（RAM など）
	void mem(void *p, size_t n) { raw(p, n); }

	// 目印。読むときに食い違えばそこで止める
	void tag(const char *name)
	{
		char buf[8] = {};
		std::strncpy(buf, name, sizeof(buf) - 1);
		char got[8];
		std::memcpy(got, buf, sizeof(got));
		raw(got, sizeof(got));
		if (!m_out && m_ok && std::memcmp(got, buf, sizeof(buf)) != 0) {
			char m[64];
			std::snprintf(m, sizeof(m), "目印が違う（%s のところ）", name);
			fail(m);
		}
	}

private:
	void fail(const char *why)
	{
		m_ok = false;
		if (m_err.empty())
			m_err = why;
	}

	std::vector<u8> *m_out = nullptr;
	const u8 *m_in = nullptr;
	size_t m_len = 0, m_at = 0;
	bool m_ok = true;
	u32 m_version = 0;
	std::string m_err;
};

// ---- 詰め込み
//
// 状態はそのままだと 6MB ほどある。中身はほとんど 0 の続きなので、
// **同じバイトの続きだけを縮める**単純な方法で十分に小さくなる。
// DAW のプロジェクトに埋め込むので、小さいに越したことはない。
//
//   0x00 nn b      b が nn+4 個（nn は 0-251、つまり 4-255 個）
//   0x00 0xfc b    b が 1 個（0x00 そのものを出すとき）
//   それ以外        そのまま 1 バイト

inline std::vector<u8> state_pack(const std::vector<u8> &in)
{
	std::vector<u8> out;
	out.reserve(in.size() / 4 + 64);
	size_t i = 0;
	while (i < in.size()) {
		size_t run = 1;
		while (run < 255 && i + run < in.size() && in[i + run] == in[i])
			run++;
		if (run >= 4) {
			out.push_back(0x00);
			out.push_back(u8(run - 4));
			out.push_back(in[i]);
			i += run;
		} else if (in[i] == 0x00) {
			out.push_back(0x00);
			out.push_back(0xfc);
			out.push_back(0x00);
			i++;
		} else {
			out.push_back(in[i]);
			i++;
		}
	}
	return out;
}

inline bool state_unpack(const u8 *p, size_t n, std::vector<u8> &out)
{
	out.clear();
	size_t i = 0;
	while (i < n) {
		if (p[i] != 0x00) {
			out.push_back(p[i]);
			i++;
			continue;
		}
		if (i + 3 > n)
			return false;
		const u8 cnt = p[i + 1], val = p[i + 2];
		i += 3;
		if (cnt == 0xfc)
			out.push_back(val);
		else
			out.insert(out.end(), size_t(cnt) + 4, val);
	}
	return true;
}

#endif // S_MU2000_STATE_H
