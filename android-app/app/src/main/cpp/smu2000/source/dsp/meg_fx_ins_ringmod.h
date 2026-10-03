// license:BSD-3-Clause
// S-MU2000: インサーション 1: RING MOD
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_RINGMOD_H
#define S_MU2000_DSP_MEG_FX_INS_RINGMOD_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_ringmod : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x2000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; s_r39 = 0; s_r3a = 0; s_r3b = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * s_r26;
		const int32_t i2 = idx_of(p1);
		// 0c1
		const float p3 = sat((k[1] * in[0]) * 2.0f);
		const float r4 = w24(p3);   // r20
		// 0c2
		const float p5 = k[2] * s_r20;
		const float t6 = tv_index(p1);   // t1
		// 0c3
		const float p7 = k[3] * s_r21 + p5;
		const float q8 = tab(3, i2);
		// 0c4
		const float p9 = sat((k[4] * r4 + p7) * 2.0f);
		const float r10 = w24(p9);   // r21
		// 0c5
		const float p11 = k[5] * s_r21;
		const float m12 = q8;   // m01
		// 0c6
		const float p13 = k[6] * s_r22 + p11;
		const float q14 = tab(6, i2 + 1);
		// 0c7
		const float p15 = sat((k[7] * r10 + p13) * 4.0f);
		const float r16 = w24(p15);   // r22
		// 0c8
		const float p17 = s_r26;
		const float m18 = q14;   // m02
		// 0c9
		const float p19 = k[9] + p17;
		const float r20 = w24(p19);   // r26
		// 0ca
		const float p21 = t6 * m12 - m12;
		// 0cb
		const float p22 = sat(t6 * m18 - p21);
		const float w23 = p22;
		// 0cc
		const float p24 = sat((k[12] * in[1]) * 2.0f);
		const float r25 = w24(p24);   // r23
		// 0cd
		const float p26 = k[13] * s_r23;
		// 0ce
		const float p27 = k[14] * s_r24 + p26;
		// 0cf
		const float p28 = sat((k[15] * r25 + p27) * 2.0f);
		const float r29 = w24(p28);   // r24
		ram[at(15)] = w23;
		// 0d0
		const float p30 = k[16] * s_r24;
		const float m31 = lfo[13];   // m03
		// 0d1
		const float p32 = k[17] * s_r25 + p30;
		// 0d2
		const float p33 = sat((k[18] * r29 + p32) * 4.0f);
		const float r34 = w24(p33);   // r25
		// 0d3
		const float p35 = sat(k[19] * m31);
		const float r36 = w24(p35);   // r2a
		// 0d4
		const float p37 = k[20] * s_r2a;
		// 0d5
		const float p38 = k[21] * s_r2b + p37;
		// 0d6
		const float p39 = sat(k[22] * r36 + p38);
		const float r40 = w24(p39);   // r2b
		// 0d7
		const float p41 = sat((k[23] * r16) * 2.0f);
		const float r42 = w24(p41);   // r2c
		// 0d8
		const float p43 = k[24] * s_r2c;
		const float r44 = s_r2c;   // r2d
		// 0d9
		const float p45 = k[25] * s_r2d + p43;
		// 0da
		const float p46 = k[26] * s_r2e + p45;
		// 0db
		const float p47 = k[27] * s_r2f + p46;
		// 0dc
		const float p48 = sat((k[28] * r42 + p47) * 4.0f);
		const float r49 = w24(p48);   // r2e
		// 0dd
		const float p50 = k[29] * s_r2e;
		const float r51 = s_r2e;   // r2f
		// 0de
		const float p52 = k[30] * s_r2f + p50;
		// 0df
		const float p53 = k[31] * s_r30 + p52;
		// 0e0
		const float p54 = k[32] * s_r31 + p53;
		// 0e1
		const float p55 = sat((k[33] * r49 + p54) * 4.0f);
		const float r56 = w24(p55);   // r30
		// 0e2
		const float p57 = k[34] * s_r30;
		const float r58 = s_r30;   // r31
		// 0e3
		const float p59 = k[35] * s_r31 + p57;
		// 0e4
		const float p60 = k[36] * s_r32 + p59;
		const float r61 = s_r32;   // r33
		// 0e5
		const float p62 = k[37] * s_r33 + p60;
		// 0e6
		const float p63 = sat((k[38] * r56 + p62) * 16.0f);
		const float r64 = w24(p63);   // r32
		// 0e7
		const float p65 = sat(k[39] * r40);
		const int32_t i66 = idx_of(p65);
		// 0e8
		// 0e9
		const float t67 = tv_index(p65);   // t2
		// 0ea
		const float p68 = sat((k[42] * r34) * 2.0f);
		const float r69 = w24(p68);   // r34
		const float q70 = ram[at(42, i66)];
		// 0eb
		const float p71 = k[43] * s_r34;
		const float r72 = s_r34;   // r35
		// 0ec
		const float p73 = k[44] * s_r35 + p71;
		const float m74 = q70;   // m01
		// 0ed
		const float p75 = k[45] * s_r36 + p73;
		const float q76 = ram[at(45, i66 + 1)];
		// 0ee
		const float p77 = k[46] * s_r37 + p75;
		// 0ef
		const float p78 = sat((k[47] * r69 + p77) * 4.0f);
		const float m79 = q76;   // m02
		const float r80 = w24(p78);   // r36
		// 0f0
		const float p81 = k[48] * s_r36;
		const float r82 = s_r36;   // r37
		// 0f1
		const float p83 = k[49] * s_r37 + p81;
		// 0f2
		const float p84 = k[50] * s_r38 + p83;
		// 0f3
		const float p85 = k[51] * s_r39 + p84;
		// 0f4
		const float p86 = sat((k[52] * r80 + p85) * 4.0f);
		const float r87 = w24(p86);   // r38
		// 0f5
		const float p88 = k[53] * s_r38;
		const float r89 = s_r38;   // r39
		// 0f6
		const float p90 = k[54] * s_r39 + p88;
		// 0f7
		const float p91 = k[55] * s_r3a + p90;
		const float r92 = s_r3a;   // r3b
		// 0f8
		const float p93 = k[56] * s_r3b + p91;
		// 0f9
		const float p94 = sat((k[57] * r87 + p93) * 16.0f);
		const float r95 = w24(p94);   // r3a
		// 0fa
		const float p96 = t67 * m74 - m74;
		// 0fb
		const float p97 = sat(t67 * m79 - p96);
		const float r98 = w24(p97);   // r28
		// 0fc
		const float p99 = k[60] * s_r28;
		// 0fd
		const float p100 = k[61] * s_r29 + p99;
		// 0fe
		const float p101 = sat(k[62] * r98 + p100);
		const float r102 = w24(p101);   // r29
		// 0ff
		// 100
		const float t103 = tv_plain(p101);   // t3
		// 101
		const float p104 = sat(t103 * r64);
		const float m105 = w24(p104);   // m01
		// 102
		const float p106 = sat(t103 * r95);
		const float m107 = w24(p106);   // m02
		// 103
		const float p108 = k[67] * r102;
		const float r109 = w24(p108);   // r03
		// 104
		// 105
		// 106
		const float p110 = k[70] * m105 + r109;
		// 107
		const float p111 = sat((k[71] * r42 + p110) * 4.0f);
		const float m112 = w24(p111);   // m28
		// 108
		const float p113 = k[72] * m107 + r109;
		// 109
		const float p114 = sat((k[73] * r69 + p113) * 4.0f);
		const float m115 = w24(p114);   // m29
		// 10a
		// 10b
		// 10c
		// 10d
		// 10e
		// 10f
		// 110
		// 111
		// 112
		// 113
		// 114
		// 115
		// 116
		// 117
		// 118
		// 119
		// 11a
		// 11b
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m112;   // m28
		out[1] = m115;   // m29
		s_p = p114;
		s_r26 = r20;
		s_r20 = r4;
		s_r21 = r10;
		s_r22 = r16;
		s_r23 = r25;
		s_r24 = r29;
		s_r25 = r34;
		s_r2a = r36;
		s_r2b = r40;
		s_r2c = r42;
		s_r2d = r44;
		s_r2e = r49;
		s_r2f = r51;
		s_r30 = r56;
		s_r31 = r58;
		s_r32 = r64;
		s_r33 = r61;
		s_r34 = r69;
		s_r35 = r72;
		s_r36 = r80;
		s_r37 = r82;
		s_r38 = r87;
		s_r39 = r89;
		s_r3a = r95;
		s_r3b = r92;
		s_r28 = r98;
		s_r29 = r102;
	}

private:
	float s_m00 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r20 = 0.0f;
	float s_r21 = 0.0f;
	float s_r22 = 0.0f;
	float s_r23 = 0.0f;
	float s_r24 = 0.0f;
	float s_r25 = 0.0f;
	float s_r26 = 0.0f;
	float s_r28 = 0.0f;
	float s_r29 = 0.0f;
	float s_r2a = 0.0f;
	float s_r2b = 0.0f;
	float s_r2c = 0.0f;
	float s_r2d = 0.0f;
	float s_r2e = 0.0f;
	float s_r2f = 0.0f;
	float s_r30 = 0.0f;
	float s_r31 = 0.0f;
	float s_r32 = 0.0f;
	float s_r33 = 0.0f;
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
	float s_r36 = 0.0f;
	float s_r37 = 0.0f;
	float s_r38 = 0.0f;
	float s_r39 = 0.0f;
	float s_r3a = 0.0f;
	float s_r3b = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_RINGMOD_H
