// license:BSD-3-Clause
// S-MU2000: コーラスの口の PHASER 1
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m26 m27 / 出口: m26 m27


#ifndef S_MU2000_DSP_MEG_FX_PHASER_CHO_H
#define S_MU2000_DSP_MEG_FX_PHASER_CHO_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_phaser_cho : public meg_fx_base<40>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x200;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x26, 0x27 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x26, 0x27 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m10 = 0; s_m11 = 0; s_p = 0; s_r00 = 0; s_r18 = 0; s_r19 = 0; s_r1a = 0; s_r1b = 0; s_r1c = 0; s_r1d = 0; s_r1e = 0; s_r1f = 0; }

	// in: 入口（m26 m27）。out: 出口（m26 m27）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 098
		const float p1 = k[0] * in[0];
		const float m2 = lfo[9];   // m01
		// 099
		const float p3 = sat((k[1] * in[1] + p1) * 2.0f);
		const float r4 = w24(p3);   // r18
		// 09a
		const float p5 = k[2] * s_r18;
		// 09b
		const float p6 = k[3] * s_r19 + p5;
		// 09c
		const float p7 = sat((k[4] * r4 + p6) * 2.0f);
		const float r8 = w24(p7);   // r19
		// 09d
		const float p9 = k[5] * s_r19;
		// 09e
		const float p10 = k[6] * s_r1a + p9;
		// 09f
		const float p11 = sat((k[7] * r8 + p10) * 4.0f);
		const float r12 = w24(p11);   // r1a
		// 0a0
		const float p13 = k[8] * m2;
		// 0a1
		const float p14 = sat(k[9] + p13);
		const float r15 = w24(p14);   // r01
		// 0a2
		const float p16 = k[10] * s_r1d;
		// 0a3
		const float p17 = k[11] * s_r1e + p16;
		const float t18 = tv_plain(p14);   // t1
		// 0a4
		const float p19 = sat((k[12] * s_r1f + p17) * 2.0f);
		const float r20 = w24(p19);   // r02
		// 0a5
		const float p21 = sat(t18 * r15);
		const float r22 = w24(p21);   // r01
		// 0a6
		const float p23 = k[14] * s_r1e;
		// 0a7
		const float p24 = k[15] * s_r1f + p23;
		const float t25 = tv_plain(p21);   // t1
		// 0a8
		const float p26 = sat((k[16] * s_m11 + p24) * 2.0f);
		const float r27 = w24(p26);   // r03
		// 0a9
		const float p28 = k[17];
		// 0aa
		const float p29 = sat(t25 * r22 - p28);
		// 0ab
		const float p30 = (k[19] * r12) * 2.0f;
		// 0ac
		const float p31 = sat(k[20] * r27 + p30);
		const float r32 = w24(p31);   // r1b
		const float t33 = tv_plain(p29);   // t1
		// 0ad
		const float p34 = s_r1b;
		// 0ae
		const float p35 = t33 * s_r1c - p34;
		const float r36 = s_r1c;   // r01
		// 0af
		const float p37 = sat(t33 * r32 - p35);
		const float r38 = w24(p37);   // r1c
		// 0b0
		const float p39 = k[24] * r20;
		// 0b1
		const float p40 = sat((k[25] * r27 + p39) * 4.0f);
		const float m41 = w24(p40);   // m26
		// 0b2
		const float p42 = t33 * s_m10 - r36;
		// 0b3
		const float p43 = sat(t33 * r38 - p42);
		const float m44 = w24(p43);   // m10
		// 0b4
		const float p45 = sat((k[28] * r27) * 4.0f);
		const float m46 = w24(p45);   // m27
		// 0b5
		const float p47 = t33 * s_r1d - s_m10;
		// 0b6
		const float p48 = sat(t33 * m44 - p47);
		const float r49 = w24(p48);   // r1d
		// 0b7
		const float p50 = s_r1d;
		// 0b8
		const float p51 = t33 * s_r1e - p50;
		// 0b9
		const float p52 = sat(t33 * r49 - p51);
		const float r53 = w24(p52);   // r1e
		// 0ba
		const float p54 = s_r1e;
		// 0bb
		const float p55 = t33 * s_r1f - p54;
		// 0bc
		const float p56 = sat(t33 * r53 - p55);
		const float r57 = w24(p56);   // r1f
		// 0bd
		const float p58 = s_r1f;
		// 0be
		const float p59 = t33 * s_m11 - p58;
		// 0bf
		const float p60 = sat(t33 * r57 - p59);
		const float m61 = w24(p60);   // m11
		out[0] = m41;   // m26
		out[1] = m46;   // m27
		s_p = p60;
		s_r18 = r4;
		s_r19 = r8;
		s_r1a = r12;
		s_r1d = r49;
		s_r1e = r53;
		s_r1f = r57;
		s_m11 = m61;
		s_r1b = r32;
		s_r1c = r38;
		s_m10 = m44;
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
	float s_r1e = 0.0f;
	float s_r1f = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_PHASER_CHO_H
