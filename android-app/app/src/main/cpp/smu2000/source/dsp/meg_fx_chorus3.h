// license:BSD-3-Clause
// S-MU2000: コーラス（CHORUS 3）
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m26 m27 / 出口: m26 m27


#ifndef S_MU2000_DSP_MEG_FX_CHORUS3_H
#define S_MU2000_DSP_MEG_FX_CHORUS3_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_chorus3 : public meg_fx_base<40>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xf00;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x26, 0x27 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x26, 0x27 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m10 = 0; s_m11 = 0; s_p = 0; s_r00 = 0; s_r18 = 0; s_r19 = 0; s_r1a = 0; s_r1b = 0; s_r1c = 0; s_r1d = 0; s_t4 = 0; }

	// in: 入口（m26 m27）。out: 出口（m26 m27）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 098
		const float p1 = sat(k[0] * lfo[8]);
		const float m2 = lfo[9];   // m07
		const int32_t i3 = idx_of(p1);
		// 099
		const float p4 = sat((k[1] * in[0]) * 2.0f);
		const float r5 = w24(p4);   // r18
		// 09a
		const float p6 = k[2] * s_r18;
		const float t7 = tv_index(p1);   // t1
		// 09b
		const float p8 = k[3] * s_r19 + p6;
		// 09c
		const float p9 = sat((k[4] * r5 + p8) * 2.0f);
		const float r10 = w24(p9);   // r19
		const float q11 = ram[at(4, i3)];
		// 09d
		const float p12 = k[5] * s_r19;
		// 09e
		const float p13 = k[6] * s_r1a + p12;
		const float m14 = q11;   // m01
		// 09f
		const float p15 = sat((k[7] * r10 + p13) * 4.0f);
		const float r16 = w24(p15);   // r1a
		const float q17 = ram[at(7, i3 + 1)];
		// 0a0
		const float p18 = sat(k[8] * m2);
		const float m19 = lfo[10];   // m08
		const int32_t i20 = idx_of(p18);
		// 0a1
		const float p21 = sat((k[9] * in[1]) * 2.0f);
		const float m22 = q17;   // m02
		const float r23 = w24(p21);   // r1b
		// 0a2
		const float p24 = k[10] * s_r1b;
		const float t25 = tv_index(p18);   // t2
		// 0a3
		const float p26 = k[11] * s_r1c + p24;
		// 0a4
		const float p27 = sat((k[12] * r23 + p26) * 2.0f);
		const float r28 = w24(p27);   // r1c
		// 0a5
		const float p29 = k[13] * s_r1c;
		const float q30 = ram[at(13, i20)];
		// 0a6
		const float p31 = k[14] * s_r1d + p29;
		// 0a7
		const float p32 = sat((k[15] * r28 + p31) * 4.0f);
		const float m33 = q30;   // m03
		const float r34 = w24(p32);   // r1d
		// 0a8
		const float p35 = sat(k[16] * m19);
		const int32_t i36 = idx_of(p35);
		const float q37 = ram[at(16, i20 + 1)];
		// 0a9
		const float p38 = t7 * m14 - m14;
		// 0aa
		const float p39 = sat(t7 * m22 - p38);
		const float m40 = q37;   // m04
		const float r41 = w24(p39);   // r01
		const float t42 = tv_index(p35);   // t1
		// 0ab
		const float q43 = ram[at(19, i36)];
		// 0ac
		const float p44 = t25 * m33 - m33;
		// 0ad
		const float p45 = sat(t25 * m40 - p44);
		const float m46 = q43;   // m05
		const float r47 = w24(p45);   // r03
		// 0ae
		const float p48 = k[22] * r16;
		const float q49 = ram[at(22, i36 + 1)];
		// 0af
		const float p50 = k[23] * r34 + p48;
		// 0b0
		const float p51 = k[24] * r41 + p50;
		const float m52 = q49;   // m06
		// 0b1
		const float p53 = sat(k[25] * r47 + p51);
		const float m54 = lfo[11];   // m09
		const float w55 = p53;
		// 0b2
		const float p56 = t42 * m46 - m46;
		// 0b3
		const float p57 = sat(t42 * m52 - p56);
		const float r58 = w24(p57);   // r04
		// 0b4
		const float p59 = sat(k[28] * m54);
		const int32_t i60 = idx_of(p59);
		ram[at(28)] = w55;
		// 0b5
		const float p61 = s_t4 * s_m10 - s_m10;
		// 0b6
		const float p62 = sat(s_t4 * s_m11 - p61);
		const float r63 = w24(p62);   // r02
		const float t64 = tv_index(p59);   // t4
		// 0b7
		const float p65 = k[31] * r41;
		const float q66 = ram[at(31, i60)];
		// 0b8
		const float p67 = sat((k[32] * r47 + p65) * 4.0f);
		const float m68 = w24(p67);   // m26
		// 0b9
		const float p69 = k[33] * r34;
		const float m70 = q66;   // m10
		// 0ba
		const float p71 = k[34] * r58 + p69;
		const float q72 = ram[at(34, i60 + 1)];
		// 0bb
		const float p73 = sat(k[35] * r63 + p71);
		const float w74 = p73;
		// 0bc
		const float m75 = q72;   // m11
		// 0bd
		ram[at(37)] = w74;
		// 0be
		const float p76 = k[38] * r58;
		// 0bf
		const float p77 = sat((k[39] * r63 + p76) * 4.0f);
		const float m78 = w24(p77);   // m27
		out[0] = m68;   // m26
		out[1] = m78;   // m27
		s_p = p77;
		s_r18 = r5;
		s_r19 = r10;
		s_r1a = r16;
		s_r1b = r23;
		s_r1c = r28;
		s_r1d = r34;
		s_t4 = t64;
		s_m10 = m70;
		s_m11 = m75;
	}

private:
	float s_m00 = 0.0f;
	float s_m10 = 0.0f;
	float s_m11 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r18 = 0.0f;
	float s_r19 = 0.0f;
	float s_r1a = 0.0f;
	float s_r1b = 0.0f;
	float s_r1c = 0.0f;
	float s_r1d = 0.0f;
	float s_t4 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_CHORUS3_H
