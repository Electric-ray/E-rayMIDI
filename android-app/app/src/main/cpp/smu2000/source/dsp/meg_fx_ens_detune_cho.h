// license:BSD-3-Clause
// S-MU2000: コーラスの口の ENS DETUNE
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m26 m27 / 出口: m26 m27


#ifndef S_MU2000_DSP_MEG_FX_ENS_DETUNE_CHO_H
#define S_MU2000_DSP_MEG_FX_ENS_DETUNE_CHO_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ens_detune_cho : public meg_fx_base<40>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x300;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x26, 0x27 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x26, 0x27 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r18 = 0; s_r19 = 0; s_r1a = 0; s_r1b = 0; s_r1c = 0; s_r1d = 0; s_r1e = 0; s_r1f = 0; }

	// in: 入口（m26 m27）。out: 出口（m26 m27）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 098
		const float p1 = sat((k[0] * s_r1c) * 16.0f);
		const float m2 = lfo[9];   // m09
		// 099
		const float p3 = sat((k[1] * s_r1d) * 16.0f);
		// 09a
		const float p4 = (lfo[8]) * 2.0f;
		const float r5 = w24(p4);   // r01
		const float t6 = tv_plain(p1);   // t1
		// 09b
		const float p7 = satpos(k[3] * lfo[8]);
		const int32_t i8 = idx_of(p7);
		const float t9 = tv_plain(p3);   // t2
		// 09c
		const float p10 = t9 * s_r19 - s_r19;
		// 09d
		const float p11 = sat(t6 * s_r18 - p10);
		const float r12 = w24(p11);   // r08
		const float t13 = tv_index(p7);   // t1
		// 09e
		const float p14 = sat((k[6] * s_r1e) * 16.0f);
		// 09f
		const float p15 = sat((k[7] * s_r1f) * 16.0f);
		const float q16 = ram[at(7, i8)];
		// 0a0
		const float p17 = (m2) * 2.0f;
		const float r18 = w24(p17);   // r04
		const float t19 = tv_plain(p14);   // t3
		// 0a1
		const float p20 = satpos(k[9] * m2);
		const float m21 = q16;   // m01
		const int32_t i22 = idx_of(p20);
		const float t23 = tv_plain(p15);   // t4
		// 0a2
		const float p24 = t23 * s_r1b - s_r1b;
		const float q25 = ram[at(10, i8 + 1)];
		// 0a3
		const float p26 = sat(t19 * s_r1a - p24);
		const float r27 = w24(p26);   // r09
		const float t28 = tv_index(p20);   // t3
		// 0a4
		const float p29 = k[12] * in[0];
		const float m30 = q25;   // m02
		// 0a5
		const float p31 = (k[13] * in[1] + p29) * 2.0f;
		const float q32 = ram[at(13, i22)];
		// 0a6
		const float p33 = sat(k[14] * r12 + p31);
		const float w34 = p33;
		// 0a7
		const float p35 = k[15] * r5;
		const float m36 = q32;   // m05
		// 0a8
		const float p37 = satpos(k[16] + p35);
		const float r38 = w24(p37);   // r02
		const float q39 = ram[at(16, i22 + 1)];
		// 0a9
		const float p40 = (k[17] * in[1]) * 2.0f;
		// 0aa
		const float p41 = sat(k[18] * r27 + p40);
		const float w42 = p41;
		// 0ab
		const float p43 = k[19] * r18;
		const float m44 = q39;   // m06
		ram[at(19)] = w34;
		// 0ac
		const float p45 = satpos(k[20] + p43);
		const float r46 = w24(p45);   // r05
		// 0ad
		const float p47 = satpos(k[21] * r38);
		const int32_t i48 = idx_of(p47);
		// 0ae
		const float p49 = satabs(r5);
		const float r50 = w24(p49);   // r03
		ram[at(22)] = w42;
		// 0af
		const float p51 = satpos(k[23] + p49);
		const float r52 = w24(p51);   // r1c
		const float t53 = tv_index(p47);   // t2
		// 0b0
		const float p54 = satabs(r18);
		const float r55 = w24(p54);   // r06
		// 0b1
		const float p56 = satpos(k[25] + p54);
		const float r57 = w24(p56);   // r1e
		const float q58 = ram[at(25, i48)];
		// 0b2
		const float p59 = satpos(k[26] * r46);
		const int32_t i60 = idx_of(p59);
		// 0b3
		const float p61 = satpos(k[27] + r50);
		const float m62 = q58;   // m03
		const float r63 = w24(p61);   // r1d
		// 0b4
		const float p64 = satpos(k[28] + r55);
		const float r65 = w24(p64);   // r1f
		const float t66 = tv_index(p59);   // t4
		const float q67 = ram[at(28, i48 + 1)];
		// 0b5
		const float p68 = sat((k[29] * r12) * 4.0f);
		const float m69 = w24(p68);   // m26
		// 0b6
		const float p70 = k[30] * r12;
		const float m71 = q67;   // m04
		// 0b7
		const float p72 = sat((k[31] * r27 + p70) * 4.0f);
		const float m73 = w24(p72);   // m27
		const float q74 = ram[at(31, i60)];
		// 0b8
		const float p75 = t13 * m21 - m21;
		// 0b9
		const float p76 = sat(t13 * m30 - p75);
		const float m77 = q74;   // m07
		const float r78 = w24(p76);   // r18
		// 0ba
		const float p79 = t53 * m62 - m62;
		const float q80 = ram[at(34, i60 + 1)];
		// 0bb
		const float p81 = sat(t53 * m71 - p79);
		const float r82 = w24(p81);   // r19
		// 0bc
		const float p83 = t28 * m36 - m36;
		const float m84 = q80;   // m08
		// 0bd
		const float p85 = sat(t28 * m44 - p83);
		const float r86 = w24(p85);   // r1a
		// 0be
		const float p87 = t66 * m77 - m77;
		// 0bf
		const float p88 = sat(t66 * m84 - p87);
		const float r89 = w24(p88);   // r1b
		out[0] = m69;   // m26
		out[1] = m73;   // m27
		s_p = p88;
		s_r1c = r52;
		s_r1d = r63;
		s_r19 = r82;
		s_r18 = r78;
		s_r1e = r57;
		s_r1f = r65;
		s_r1b = r89;
		s_r1a = r86;
	}

private:
	float s_m00 = 0.0f;
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

#endif // S_MU2000_DSP_MEG_FX_ENS_DETUNE_CHO_H
