// license:BSD-3-Clause
// S-MU2000: MEG と同じ作りのエフェクト（src/dsp/meg_fx_*.h）が共通で使うもの。
//
// firmware がエフェクトに置く MEG のプログラムの形を float の C++ に書き起こしたものは、どれもこの型から作る。
// 係数（命令ごとの定数）と番地表は、firmware が MEG に書いた値を configure() で読む。
// 値の目盛りは MEG のレジスタの 24bit を 1.0 とする。p（積和の途中）も同じ目盛りで、p の 2^38 が 1.0 にあたる。
// 遅延メモリは MEG と同じ環状の窓で、番地も同じ式（番地表の値 + idx - サンプル番号）で引く。
//
// MEG と違うのは次の細部だけ（どれも 1 LSB 前後の話）:
//   - レジスタへ入れるときの 24bit への切り捨てと、書き込みごとのディザを省く
//   - 遅延メモリを 16bit の浮動小数点に詰めない

#ifndef S_MU2000_DSP_MEG_FX_COMMON_H
#define S_MU2000_DSP_MEG_FX_COMMON_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace smu2000::dsp {

template<int N>
class meg_fx_base
{
public:
	// 区画の窓の長さ（番地の mask + 1）
	void resize(uint32_t window)
	{
		if (m_ram.size() != window) {
			m_ram.assign(window, 0.0f);
			m_mask = window - 1;
		}
	}

	// cst / off: MEG の命令ごとの定数（0x180）と番地表（0x80）。base: このエフェクトの先頭の命令の番地
	void configure(const int16_t *cst, const uint16_t *off, int base)
	{
		for (int i = 0; i != N; i++) {
			const int16_t c = cst[base + i];
			k[i] = float(c) * (1.0f / 32768.0f);
			kx[i] = float(m1_expand(c)) * (1.0f / 32768.0f);
			m_off[i] = off[(base + i) / 3];
		}
	}

protected:
	void reset_ram() { std::fill(m_ram.begin(), m_ram.end(), 0.0f); }

public:
	// SWP30 のリバーブ RAM（0x40000 語）。絶対番地の読み出しが使う
	void set_table(const uint16_t *ram) { m_tab = ram; }

	// 鳴らし始めに、MEG のこの区画の遅延メモリの中身を写す（切り替えたときに MEG に残っていたものから始める）。
	// counter: MEG のサンプル番号。base: 区画の窓の先頭の番地
	void load_ram(const uint16_t *revram, uint32_t base, uint32_t counter)
	{
		// C++ の番地は (番地表 - m_n) & mask、MEG は ((番地表 - counter) & mask) + base。m_n を counter にそろえる
		m_n = counter - 1;          // process() の頭で 1 進む
		for (uint32_t i = 0; i != m_ram.size(); i++)
			m_ram[i] = revram_value(revram[(base + i) & 0x3ffff]);
	}

protected:

	// p の段の飽和（±2^38 = ±1.0）。正の上限は 2^38 - 1 で、レジスタに入れると 0x7fffff になる。
	// ちょうど 1.0 にすると、レジスタへ入れるときの 24bit の折り返しで -1.0 に化ける（歪み系が壊れる）
	static constexpr float PMAX = 1.0f - 1.0f / 8388608.0f;
	static float sat(float v) { return std::clamp(v, -1.0f, PMAX); }
	static float satpos(float v) { return std::clamp(v, 0.0f, PMAX); }
	static float satabs(float v) { return std::min(std::fabs(v), PMAX); }
	// レジスタへ入れるとき（24bit で折り返す。飽和させた p ならそのまま）
	// MEG は 24bit へ 0 の側に切り捨てる（meg_pack24）。位相を積み上げていくレジスタは、この切り捨てで
	// 少しずつ遅れるので、同じように切り捨てないと再生の位置（D.TURNTBL など）がずれていく
	static float w24(float v)
	{
		if (v >= 1.0f || v < -1.0f)
			v -= 2.0f * std::floor((v + 1.0f) * 0.5f);
		return float(int32_t(v * 8388608.0f)) * (1.0f / 8388608.0f);
	}
	// idx（p >> 23）
	static int32_t idx_of(float p) { return int32_t(std::floor(double(p) * 32768.0)); }
	// t の値。idx を書く命令では端数（(p >> 8) & 0x7fff）、ほかは p >> 23 を 16bit で止めたもの
	static float tv_index(float p) { return float(int64_t(std::floor(double(p) * 1073741824.0)) & 0x7fff) * (1.0f / 32768.0f); }
	static float tv_plain(float p) { return std::clamp(std::floor(p * 32768.0f), -32768.0f, 32767.0f) * (1.0f / 32768.0f); }
	static float expand_t(float t) { return float(m1_expand(int16_t(t * 32768.0f))) * (1.0f / 32768.0f); }
	// ビットの積（歪み・ローファイが使う）
	static float and38(float a, float b)
	{
		const int64_t x = int64_t(double(a) * 274877906944.0), y = int64_t(double(b) * 274877906944.0);
		return float(double(x & y) * (1.0 / 274877906944.0));
	}
	float noise()
	{
		m_seed = m_seed * 1664525u + 1013904223u;
		return float(int32_t(m_seed) >> 8) * (1.0f / 8388608.0f);
	}

	static int16_t m1_expand(int16_t v)
	{
		if (v < 0)
			return 0;
		const uint32_t s = uint32_t(v) >> 12;
		const int32_t m = 0x1000 | (v & 0xfff);
		return int16_t(s == 5 ? m : s < 5 ? (m >> (5 - s)) : (m << (s - 5)));
	}

	uint32_t at(int rel, int32_t add = 0) const { return (uint32_t(m_off[rel]) + uint32_t(add) - m_n) & m_mask; }

	// MEG の遅延メモリの 1 語（16bit の浮動小数点）をほどく（meg_state::revram_decode と同じ）
	static float revram_value(uint16_t v)
	{
		const uint32_t e = (v >> 12) & 15, s = (v >> 11) & 1, m = v & 0x7ff;
		uint32_t vb = e ? (m | 0x800) << (e - 1) : m;
		if (s)
			vb ^= e ? (0xffffffffu << (e - 1)) : 0xffffffffu;
		return float(int32_t(vb)) * (1.0f / 8388608.0f);
	}

	// 絶対番地の読み出し（bit 0x23）。firmware が遅延メモリ（SWP30 のリバーブ RAM）に置いた表を読む
	float tab(int rel, int32_t add = 0) const
	{
		return m_tab ? revram_value(m_tab[(uint32_t(m_off[rel]) + uint32_t(add)) & 0x3ffff]) : 0.0f;
	}

	float k[N] = {};         // 定数（1.15）
	float kx[N] = {};        // 定数を m1_expand したもの
	uint16_t m_off[N] = {};  // 命令ごとの番地表の値
	std::vector<float> m_ram;
	float *ram_ptr() { return m_ram.data(); }
	uint32_t m_mask = 0;
	uint32_t m_n = 0;
	uint32_t m_seed = 1;
	const uint16_t *m_tab = nullptr;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_COMMON_H
