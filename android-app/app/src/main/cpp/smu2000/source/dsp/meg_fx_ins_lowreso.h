// license:BSD-3-Clause
// S-MU2000: インサーション 1: LOW RESO, D.SCRATCH
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_LOWRESO_H
#define S_MU2000_DSP_MEG_FX_INS_LOWRESO_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_lowreso : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xf000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; s_r39 = 0; s_r3a = 0; s_r3b = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * s_r30;
		const float m2 = lfo[12];   // m05
		const float r3 = s_r30;   // r20
		// 0c1
		const float p4 = k[1] * s_r20 + p1;
		const float r5 = s_r20;   // r21
		// 0c2
		const float p6 = k[2] * s_r21 + p4;
		// 0c3
		const float p7 = k[3] * s_r22 + p6;
		const float r8 = s_r22;   // r23
		// 0c4
		const float p9 = sat((k[4] * s_r23 + p7) * 2.0f);
		const float r10 = w24(p9);   // r22
		// 0c5
		const float p11 = k[5] * m2;
		const int32_t i12 = idx_of(p11);
		// 0c6
		// 0c7
		const float t13 = tv_index(p11);   // t1
		// 0c8
		// 0c9
		const float p14 = k[9] * s_r31;
		const float r15 = s_r31;   // r2c
		const float q16 = ram[at(9, i12)];
		// 0ca
		const float p17 = k[10] * s_r2c + p14;
		const float r18 = s_r2c;   // r2d
		// 0cb
		const float p19 = k[11] * s_r2d + p17;
		const float m20 = q16;   // m14
		// 0cc
		const float p21 = k[12] * s_r2e + p19;
		const float r22 = s_r2e;   // r2f
		const float q23 = ram[at(12, i12 + 1)];
		// 0cd
		const float p24 = sat((k[13] * s_r2f + p21) * 2.0f);
		const float r25 = w24(p24);   // r2e
		// 0ce
		const float m26 = q23;   // m15
		// 0cf
		const float q27 = ram[at(15)];
		// 0d0
		const float m28 = lfo[13];   // m06
		// 0d1
		const float p29 = t13 * m20 - m20;
		const float m30 = q27;   // m01
		// 0d2
		const float p31 = t13 * m26 - p29;
		const float r32 = w24(p31);   // r01
		const float q33 = ram[at(18)];
		// 0d3
		const float p34 = k[19] * m2;
		const int32_t i35 = idx_of(p34);
		// 0d4
		const float p36 = k[20] * s_m12;
		const float m37 = q33;   // m02
		// 0d5
		const float p38 = k[21] * r10 + p36;
		const float t39 = tv_index(p34);   // t1
		// 0d6
		const float p40 = sat((k[22] * m30 + p38) * 16.0f);
		const float r41 = w24(p40);   // r09
		// 0d7
		const float p42 = k[23];
		// 0d8
		const float p43 = k[24] * m28 + p42;
		const float q44 = ram[at(24, i35)];
		// 0d9
		const float p45 = sat(k[25] * s_r24 + p43);
		// 0da
		const float p46 = k[26];
		const float m47 = q44;   // m14
		// 0db
		const float p48 = k[27] * m28 + p46;
		const float t49 = tv_plain(p45);   // t2
		const float q50 = ram[at(27, i35 + 1)];
		// 0dc
		const float p51 = sat(k[28] * s_r24 + p48);
		// 0dd
		const float p52 = k[29] * r25;
		const float m53 = q50;   // m15
		// 0de
		const float p54 = sat((k[30] * m37 + p52) * 16.0f);
		const float r55 = w24(p54);   // r25
		const float t56 = tv_plain(p51);   // t3
		// 0df
		const float p57 = t39 * m47 - m47;
		// 0e0
		const float p58 = t39 * m53 - p57;
		const float m59 = lfo[14];   // m07
		const float r60 = w24(p58);   // r02
		// 0e1
		const float p61 = t49 * s_r26;
		const float r62 = w24(p61);   // r07
		// 0e2
		const float p63 = t56 * s_r27;
		const float r64 = w24(p63);   // r08
		// 0e3
		const float p65 = k[35];
		// 0e4
		const float p66 = k[36] * m59 + p65;
		// 0e5
		const float p67 = sat(k[37] * s_r24 + p66);
		// 0e6
		const float p68 = k[38];
		// 0e7
		const float p69 = k[39] * m59 + p68;
		const float t70 = tv_plain(p67);   // t2
		// 0e8
		const float p71 = sat(k[40] * s_r24 + p69);
		// 0e9
		const float p72 = k[41] * s_r26;
		// 0ea
		const float p73 = sat((k[42] * r32 + p72) * 2.0f);
		const float r74 = w24(p73);   // r05
		const float t75 = tv_plain(p71);   // t3
		// 0eb
		const float p76 = k[43] * s_r27;
		// 0ec
		const float p77 = sat((k[44] * r60 + p76) * 2.0f);
		const float r78 = w24(p77);   // r06
		// 0ed
		const float p79 = k[45];
		// 0ee
		const float p80 = sat(k[46] * r41 + p79);
		const int32_t i81 = idx_of(p80);
		// 0ef
		const float p82 = k[47] * r32;
		const float r83 = r32;   // r39
		// 0f0
		const float p84 = k[48] * s_r39 + p82;
		const float m85 = lfo[15];   // m12
		const float t86 = tv_index(p80);   // t1
		// 0f1
		const float p87 = sat((k[49] * s_r38 + p84) * 2.0f);
		const float r88 = w24(p87);   // r38
		// 0f2
		const float p89 = t70 * r74;
		const float r90 = w24(p89);   // r05
		// 0f3
		const float p91 = t75 * r78;
		const float r92 = w24(p91);   // r06
		const float q93 = tab(51, i81);
		// 0f4
		const float p94 = k[52] * r60;
		const float r95 = r60;   // r3b
		// 0f5
		const float p96 = k[53] * s_r3b + p94;
		const float m97 = q93;   // m14
		// 0f6
		const float p98 = sat((k[54] * s_r3a + p96) * 2.0f);
		const float r99 = w24(p98);   // r3a
		const float q100 = tab(54, i81 + 1);
		// 0f7
		const float p101 = k[55] * r62;
		// 0f8
		const float p102 = sat((k[56] * r88 + p101) * 2.0f);
		const float m103 = q100;   // m15
		const float w104 = p102;
		// 0f9
		const float q105 = tab(57, i81);
		// 0fa
		const float p106 = k[58] * r64;
		// 0fb
		const float p107 = sat((k[59] * r99 + p106) * 2.0f);
		const float m108 = q105;   // m03
		const float w109 = p107;
		// 0fc
		const float p110 = t86 * m97 - m97;
		ram[at(60)] = w104;
		// 0fd
		const float p111 = t86 * m103 - p110;
		const float r112 = w24(p111);   // r03
		// 0fe
		const float p113 = k[62];
		// 0ff
		const float p114 = sat(k[63] * r55 + p113);
		const int32_t i115 = idx_of(p114);
		ram[at(63)] = w109;
		// 100
		const float p116 = k[64] * in[0];
		// 101
		const float p117 = k[65] * r32 + p116;
		const float t118 = tv_index(p114);   // t1
		// 102
		const float p119 = sat((k[66] * r90 + p117) * 4.0f);
		const float r120 = w24(p119);   // r05
		const float q121 = tab(66, i115);
		// 103
		const float p122 = k[67] * in[1];
		// 104
		const float p123 = k[68] * r60 + p122;
		const float m124 = q121;   // m14
		// 105
		const float p125 = sat((k[69] * r92 + p123) * 4.0f);
		const float r126 = w24(p125);   // r06
		const float q127 = tab(69, i115 + 1);
		// 106
		const float p128 = k[70] * r112;
		// 107
		const float p129 = sat((k[71] * m108 + p128) * 2.0f);
		const float m130 = q127;   // m15
		const float r131 = w24(p129);   // r28
		const float w132 = p129;
		// 108
		const float p133 = k[72] * s_r28;
		const float r134 = s_r28;   // r29
		const float q135 = tab(72, i115);
		// 109
		const float p136 = k[73] * s_r29 + p133;
		// 10a
		const float p137 = k[74] * s_r2a + p136;
		const float m138 = q135;   // m04
		const float r139 = s_r2a;   // r2b
		// 10b
		const float p140 = k[75] * s_r2b + p137;
		ram[at(75)] = w132;
		// 10c
		const float p141 = sat((k[76] * r131 + p140) * 2.0f);
		const float r142 = w24(p141);   // r2a
		// 10d
		const float p143 = t118 * m124 - m124;
		// 10e
		const float p144 = t118 * m130 - p143;
		const float r145 = w24(p144);   // r04
		// 10f
		const float p146 = sat((k[79] * in[0]) * 2.0f);
		const float r147 = w24(p146);   // r30
		// 110
		const float p148 = sat(k[80] * r120);
		const float m149 = w24(p148);   // m28
		// 111
		const float p150 = sat((k[81] * in[1]) * 2.0f);
		const float r151 = w24(p150);   // r31
		// 112
		const float p152 = k[82] * r145;
		// 113
		const float p153 = sat((k[83] * m138 + p152) * 2.0f);
		const float r154 = w24(p153);   // r34
		const float w155 = p153;
		// 114
		const float p156 = sat(k[84] * r126);
		const float m157 = w24(p156);   // m29
		// 115
		const float p158 = k[85] * s_r34;
		const float r159 = s_r34;   // r35
		// 116
		const float p160 = k[86] * s_r35 + p158;
		// 117
		const float p161 = k[87] * s_r36 + p160;
		const float r162 = s_r36;   // r37
		ram[at(87)] = w155;
		// 118
		const float p163 = k[88] * s_r37 + p161;
		// 119
		const float p164 = sat((k[89] * r154 + p163) * 2.0f);
		const float r165 = w24(p164);   // r36
		// 11a
		// 11b
		const float p166 = sat((k[91] * r142) * 2.0f);
		const float r167 = w24(p166);   // r26
		// 11c
		// 11d
		const float p168 = sat((k[93] * r165) * 2.0f);
		const float r169 = w24(p168);   // r27
		// 11e
		const float p170 = k[94];
		// 11f
		const float p171 = sat(k[95] * r142 + p170);
		const float r172 = w24(p171);   // r24
		out[0] = m149;   // m28
		out[1] = m157;   // m29
		s_p = p171;
		s_r30 = r147;
		s_r20 = r3;
		s_r21 = r5;
		s_r22 = r10;
		s_r23 = r8;
		s_r31 = r151;
		s_r2c = r15;
		s_r2d = r18;
		s_r2e = r25;
		s_r2f = r22;
		s_m12 = m85;
		s_r24 = r172;
		s_r26 = r167;
		s_r27 = r169;
		s_r39 = r83;
		s_r38 = r88;
		s_r3b = r95;
		s_r3a = r99;
		s_r28 = r131;
		s_r29 = r134;
		s_r2a = r142;
		s_r2b = r139;
		s_r34 = r154;
		s_r35 = r159;
		s_r36 = r165;
		s_r37 = r162;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r20 = 0.0f;
	float s_r21 = 0.0f;
	float s_r22 = 0.0f;
	float s_r23 = 0.0f;
	float s_r24 = 0.0f;
	float s_r26 = 0.0f;
	float s_r27 = 0.0f;
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

#endif // S_MU2000_DSP_MEG_FX_INS_LOWRESO_H
