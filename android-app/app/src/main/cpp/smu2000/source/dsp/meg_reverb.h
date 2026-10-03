// license:BSD-3-Clause
// S-MU2000: MU2000 のリバーブ（18 種類）を、MEG と同じ作りのまま C++ の float で鳴らす。
//
// firmware はリバーブの 18 種類すべてに**同じ形のプログラム**を置き、種類とパラメータの違いは
// 係数（命令ごとの定数）と遅延の長さ（番地表）だけで出している（doc/native-dsp.md の「MEG と同じ作りのリバーブ」）。
// ここでは形を C++ で書き起こし、係数と番地は firmware が MEG に書いた値をそのまま読む。
// だから種類もパラメータも実機どおりに効き、違うのは次の細部だけ:
//   - 24bit への詰めと、書き込みごとのディザ（1 LSB に満たない雑音）を省く
//   - 遅延メモリを 16bit の浮動小数点に詰めない（仮数 12bit ぶんの丸めが無い）
// 遅延メモリは MEG と同じ 1 本の環状の窓で、番地も同じ式（番地表の値 - サンプル番号）で引く。
// 読み書きの対応を決め打ちしないので、PLATE のように遅延の並びが違う種類もそのまま同じ結線になる。
//
// 値の目盛りは MEG のレジスタの 24bit を 1.0 とする（入出力とも m24 / m25 と同じ目盛り）。

#ifndef S_MU2000_DSP_MEG_REVERB_H
#define S_MU2000_DSP_MEG_REVERB_H

#include <algorithm>
#include <cstdint>
#include <vector>

namespace smu2000::dsp {

class meg_reverb
{
public:
	// 区画 0 の窓の長さ（番地の mask + 1）。firmware のリバーブは 64k 語の窓を使う
	void resize(uint32_t window)
	{
		if (m_ram.size() != window) {
			m_ram.assign(window, 0.0f);
			m_mask = window - 1;
		}
	}

	// firmware が MEG に書いた係数（命令ごとの定数、1.15 の符号付き）と番地表を読む。
	// 書き換わったら呼び直す（重くない。命令ごとに割り算 1 回）
	void configure(const int16_t *cst, const uint16_t *off)
	{
		for (int pc = 0; pc != NPC; pc++)
			m_k[pc] = float(cst[pc]) * (1.0f / 32768.0f);
		for (int i = 0; i != NPC / 3; i++)
			m_off[i] = off[i];
	}

	void reset()
	{
		std::fill(m_ram.begin(), m_ram.end(), 0.0f);
		m_s = {};
	}

	// 鳴らし始めに、MEG の遅延メモリの中身を写す（meg_fx_common.h の load_ram と同じ）
	void load_ram(const uint16_t *revram, uint32_t base, uint32_t counter)
	{
		m_n = counter - 1;
		for (uint32_t i = 0; i != m_ram.size(); i++) {
			const uint16_t v = revram[(base + i) & 0x3ffff];
			const uint32_t e = (v >> 12) & 15, s = (v >> 11) & 1, m = v & 0x7ff;
			uint32_t vb = e ? (m | 0x800) << (e - 1) : m;
			if (s)
				vb ^= e ? (0xffffffffu << (e - 1)) : 0xffffffffu;
			m_ram[i] = float(int32_t(vb)) * (1.0f / 8388608.0f);
		}
	}

	// l, r: リバーブへの送り（MEG の m24 / m25）。ol, orr: 戻り（MEG が m24 / m25 に書く値）
	void process(float l, float r, float &ol, float &orr)
	{
		const float *k = m_k;
		state &s = m_s;
		m_n++;

		// ---- 初期反射: 入口の遅延線から 5 タップずつ ----
		const float el0 = rd(0x00), el1 = rd(0x03), el2 = rd(0x06), el3 = rd(0x09), el4 = rd(0x0c);
		const float er0 = rd(0x0f), er1 = rd(0x12), er2 = rd(0x15), er3 = rd(0x18), er4 = rd(0x1b);
		const float erl = sat(k[0x19] * el0 + k[0x1a] * el1 + k[0x1b] * el2 + k[0x1c] * el3 + k[0x1d] * el4);
		const float err = sat(k[0x1e] * er0 + k[0x1f] * er1 + k[0x20] * er2 + k[0x21] * er3 + k[0x22] * er4);

		// ---- 拡散の入口: 左右のいちばん早いタップを混ぜて 1 次のフィルタ ----
		const float ap1 = rd(0x1e), ap2 = rd(0x21);
		const float t24 = rd(0x24), t27 = rd(0x27), t2a = rd(0x2a), t2d = rd(0x2d);
		const float d10 = sat(2.0f * (k[0x23] * el4 + k[0x24] * er4));
		const float d11 = sat(4.0f * (k[0x25] * s.r11 + k[0x26] * s.r10 + k[0x27] * d10));
		s.r10 = d10;
		s.r11 = d11;

		// ---- 入口: 送りに 1 次のハイパスとローパス（左） ----
		const float a0 = sat(2.0f * (k[0x28] * r + k[0x29] * l));
		const float b0 = sat(k[0x2a] * s.b0 + k[0x2b] * s.a0 + k[0x2c] * a0);
		const float c0 = sat(2.0f * (k[0x2d] * s.c0 + k[0x2e] * s.b0 + k[0x2f] * b0));
		s.a0 = a0; s.b0 = b0; s.c0 = c0;
		const float t30 = rd(0x30);

		// ---- 拡散: オールパス 2 段 ----
		wr(0x33, sat(k[0x31] * d11 + k[0x30] * ap1));
		const float x1 = sat(2.0f * (k[0x33] * d11 + k[0x32] * ap1));
		wr(0x36, sat(k[0x34] * el4 + c0));                       // 左の入口の遅延線へ

		// ---- 入口（右） ----
		const float a1 = sat(2.0f * (k[0x35] * r));
		const float b1 = sat(k[0x36] * s.b1 + k[0x37] * s.a1 + k[0x38] * a1);
		const float t39 = rd(0x39);
		const float c1 = sat(2.0f * (k[0x39] * s.c1 + k[0x3a] * s.b1 + k[0x3b] * b1));
		s.a1 = a1; s.b1 = b1; s.c1 = c1;
		const float t3c = rd(0x3c);
		wr(0x3f, sat(k[0x3d] * x1 + k[0x3c] * ap2));
		const float dif = sat(2.0f * (k[0x3f] * x1 + k[0x3e] * ap2));   // 後部の網へ入る音
		wr(0x42, sat(k[0x40] * er4 + c1));                       // 右の入口の遅延線へ

		// ---- 出口（左）: 初期反射 + 後部の 6 本から 1 タップずつ ----
		const float outl = sat(erl + k[0x41] * t24 + k[0x42] * t27 + k[0x43] * t2a + k[0x44] * t2d
		                       + k[0x45] * t30 + k[0x46] * t39);
		const float t45 = rd(0x45), t48 = rd(0x48), t4b = rd(0x4b), t4e = rd(0x4e), t51 = rd(0x51);
		const float o54 = rd(0x54), o57 = rd(0x57), o5a = rd(0x5a), o5d = rd(0x5d);
		// ---- 出口（右） ----
		const float outr = sat(err + k[0x60] * t3c + k[0x61] * t45 + k[0x62] * t48 + k[0x63] * t4b
		                       + k[0x64] * t4e + k[0x65] * t51);
		const float f1 = rd(0x60), f2 = rd(0x63), f3 = rd(0x66), f4 = rd(0x6c);

		// ---- 出口のオールパス 2 段ずつ ----
		wr(0x69, sat(k[0x67] * outl + k[0x66] * o54));
		const float ol1 = sat(2.0f * (k[0x69] * outl + k[0x68] * o54));
		const float or1 = sat(2.0f * (k[0x6b] * outr + k[0x6a] * o57));
		wr(0x6f, sat(k[0x6d] * outr + k[0x6c] * o57));
		wr(0x72, sat(k[0x6f] * ol1 + k[0x6e] * o5a));
		const float ol2 = sat(2.0f * (k[0x71] * ol1 + k[0x70] * o5a));
		wr(0x75, sat(k[0x73] * or1 + k[0x72] * o5d));
		const float or2 = sat(2.0f * (k[0x75] * or1 + k[0x74] * o5d));

		// ---- 後部の 6 本: 読んだ音に 2 タップの FIR と 1 極の減衰をかけ、拡散の音と足して書き戻す ----
		const float g1 = sat(2.0f * (k[0x76] * f1 + k[0x77] * s.f1 + k[0x78] * s.g1));
		const float f5 = rd(0x78);
		const float g2 = sat(2.0f * (k[0x79] * f2 + k[0x7a] * s.f2 + k[0x7b] * s.g2));
		const float f6 = rd(0x7b);
		const float w1 = sat(k[0x7c] * dif + g1);
		const float g3 = sat(2.0f * (k[0x7d] * s.f3 + k[0x7e] * f3 + k[0x7f] * s.g3));
		wr(0x7e, w1);
		const float w2 = sat(k[0x80] * dif + g2);
		const float g4 = sat(2.0f * (k[0x81] * f4 + k[0x82] * s.f4 + k[0x83] * s.g4));
		const float w3 = sat(k[0x84] * dif + g3);
		wr(0x84, w2);
		const float g5 = sat(2.0f * (k[0x85] * f5 + k[0x86] * s.f5 + k[0x87] * s.g5));
		wr(0x87, w3);
		const float w4 = sat(k[0x88] * dif + g4);
		const float g6 = sat(2.0f * (k[0x89] * f6 + k[0x8a] * s.f6 + k[0x8b] * s.g6));
		wr(0x8a, w4);
		const float w5 = sat(k[0x8c] * dif + g5);
		ol = sat(4.0f * (k[0x8d] * ol2));
		orr = sat(4.0f * (k[0x8e] * or2));
		wr(0x90, w5);
		wr(0x93, sat(k[0x91] * dif + g6));
		s.f1 = f1; s.f2 = f2; s.f3 = f3; s.f4 = f4; s.f5 = f5; s.f6 = f6;
		s.g1 = g1; s.g2 = g2; s.g3 = g3; s.g4 = g4; s.g5 = g5; s.g6 = g6;
	}

private:
	static constexpr int NPC = 0x9c;     // 区画 0 の命令の数（リバーブはこの中）

	// 1 サンプル前の値を持つもの
	struct state {
		float r10, r11;                   // 拡散の入口の 1 次フィルタ
		float a0, b0, c0, a1, b1, c1;     // 入口のハイパス・ローパス（左・右）
		float f1, f2, f3, f4, f5, f6;     // 後部の 6 本の、前のサンプルで読んだ値（FIR の 2 タップ目）
		float g1, g2, g3, g4, g5, g6;     // 後部の 6 本の減衰の 1 極
	};

	// p の段の飽和（±2^38 = レジスタの ±1.0）
	static float sat(float v) { return std::clamp(v, -1.0f, 1.0f); }

	uint32_t addr(int pc) const { return (uint32_t(m_off[pc / 3]) - m_n) & m_mask; }
	float rd(int pc) const { return m_ram[addr(pc)]; }
	void wr(int pc, float v) { m_ram[addr(pc)] = v; }

	float m_k[NPC] = {};
	uint16_t m_off[NPC / 3] = {};
	std::vector<float> m_ram;
	uint32_t m_mask = 0;
	uint32_t m_n = 0;                    // サンプル番号（番地を引くのに使う。MEG の m_sample_counter と同じ向き）
	state m_s = {};
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_REVERB_H
