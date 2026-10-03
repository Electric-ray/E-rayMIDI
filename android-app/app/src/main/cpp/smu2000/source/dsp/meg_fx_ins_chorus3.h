// license:BSD-3-Clause
// S-MU2000: インサーション 1: CHORUS 3
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_CHORUS3_H
#define S_MU2000_DSP_MEG_FX_INS_CHORUS3_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_chorus3 : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x3f000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat(k[0] * s_m12);
		const float m2 = lfo[12];   // m34
		const int32_t i3 = idx_of(p1);
		// 0c1
		const float p4 = satpos(k[1] * s_r34 + p1);
		// 0c2
		const float p5 = sat((k[2] * s_r37) * 16.0f);
		const float t6 = tv_index(p1);   // t1
		// 0c3
		const float p7 = k[3] * s_r38 + p5;
		// 0c4
		const float p8 = satpos(k[4] + p7);
		const float r9 = w24(p8);   // r04
		// 0c5
		const float p10 = sat((k[5] * s_r35) * 16.0f);
		// 0c6
		const float p11 = k[6] * s_r36 + p10;
		const float q12 = ram[at(6, i3)];
		// 0c7
		const float p13 = satpos(k[7] + p11);
		const float r14 = w24(p13);   // r03
		// 0c8
		const float p15 = sat(k[8] * s_m14);
		const float m16 = q12;   // m01
		const int32_t i17 = idx_of(p15);
		// 0c9
		const float p18 = sat((k[9] * in[0]) * 2.0f);
		const float r19 = w24(p18);   // r20
		const float q20 = ram[at(9, i3 + 1)];
		// 0ca
		const float p21 = k[10] * s_r20;
		const float r22 = s_r20;   // r21
		const float t23 = tv_index(p15);   // t3
		// 0cb
		const float p24 = k[11] * s_r21 + p21;
		const float m25 = q20;   // m02
		// 0cc
		const float p26 = k[12] * s_r22 + p24;
		const float q27 = ram[at(12, i17)];
		// 0cd
		const float p28 = k[13] * s_r23 + p26;
		// 0ce
		const float p29 = sat((k[14] * r19 + p28) * 2.0f);
		const float m30 = q27;   // m05
		const float r31 = w24(p29);   // r22
		// 0cf
		const float p32 = sat(k[15] * s_m15);
		const int32_t i33 = idx_of(p32);
		const float q34 = ram[at(15, i17 + 1)];
		// 0d0
		const float p35 = k[16] * s_r22;
		const float m36 = lfo[13];   // m35
		const float r37 = s_r22;   // r23
		// 0d1
		const float p38 = k[17] * s_r23 + p35;
		const float m39 = q34;   // m06
		const float t40 = tv_index(p32);   // t5
		// 0d2
		const float p41 = k[18] * s_r24 + p38;
		const float q42 = ram[at(18, i33)];
		// 0d3
		const float p43 = k[19] * s_r25 + p41;
		// 0d4
		const float p44 = sat((k[20] * r31 + p43) * 2.0f);
		const float m45 = q42;   // m07
		const float r46 = w24(p44);   // r24
		// 0d5
		const float p47 = k[21] * s_r24;
		const float r48 = s_r24;   // r25
		const float q49 = ram[at(21, i33 + 1)];
		// 0d6
		const float p50 = k[22] * s_r25 + p47;
		// 0d7
		const float p51 = k[23] * s_r26 + p50;
		const float m52 = q49;   // m08
		const float r53 = s_r26;   // r27
		// 0d8
		const float p54 = k[24] * s_r27 + p51;
		// 0d9
		const float p55 = sat((k[25] * r46 + p54) * 16.0f);
		const float r56 = w24(p55);   // r26
		// 0da
		const float p57 = sat(k[26] * s_m13);
		const int32_t i58 = idx_of(p57);
		// 0db
		const float p59 = satpos(k[27] * r14 + p57);
		// 0dc
		const float p60 = sat((k[28] + m2) * 2.0f);
		const float r61 = w24(p60);   // r01
		const float t62 = tv_index(p57);   // t2
		// 0dd
		const float p63 = sat((k[29] * in[1]) * 2.0f);
		const float r64 = w24(p63);   // r28
		// 0de
		const float p65 = k[30] * s_r28;
		const float r66 = s_r28;   // r29
		const float q67 = ram[at(30, i58)];
		// 0df
		const float p68 = k[31] * s_r29 + p65;
		// 0e0
		const float p69 = k[32] * s_r2a + p68;
		const float m70 = q67;   // m03
		// 0e1
		const float p71 = k[33] * s_r2b + p69;
		const float m72 = lfo[14];   // m15
		const float q73 = ram[at(33, i58 + 1)];
		// 0e2
		const float p74 = sat((k[34] * r64 + p71) * 2.0f);
		const float r75 = w24(p74);   // r2a
		// 0e3
		const float p76 = k[35] * s_r2a;
		const float m77 = q73;   // m04
		const float r78 = s_r2a;   // r2b
		// 0e4
		const float p79 = k[36] * s_r2b + p76;
		// 0e5
		const float p80 = k[37] * s_r2c + p79;
		// 0e6
		const float p81 = k[38] * s_r2d + p80;
		// 0e7
		const float p82 = sat((k[39] * r75 + p81) * 2.0f);
		const float r83 = w24(p82);   // r2c
		// 0e8
		const float p84 = k[40] * s_r2c;
		const float r85 = s_r2c;   // r2d
		// 0e9
		const float p86 = k[41] * s_r2d + p84;
		// 0ea
		const float p87 = k[42] * s_r2e + p86;
		const float r88 = s_r2e;   // r2f
		// 0eb
		const float p89 = k[43] * s_r2f + p87;
		// 0ec
		const float p90 = sat((k[44] * r83 + p89) * 16.0f);
		const float r91 = w24(p90);   // r2e
		// 0ed
		const float p92 = t6 * m16 - m16;
		// 0ee
		const float p93 = sat(t6 * m25 - p92);
		const float r94 = w24(p93);   // r30
		// 0ef
		const float p95 = t23 * m30 - m30;
		// 0f0
		const float p96 = sat(t23 * m39 - p95);
		const float m97 = lfo[15];   // m12
		const float r98 = w24(p96);   // r05
		// 0f1
		// 0f2
		const float p99 = t62 * m70 - m70;
		// 0f3
		const float p100 = sat(t62 * m77 - p99);
		const float r101 = w24(p100);   // r32
		// 0f4
		const float p102 = t40 * m45 - m45;
		// 0f5
		const float p103 = sat(t40 * m52 - p102);
		const float r104 = w24(p103);   // r06
		// 0f6
		// 0f7
		const float p105 = k[55] * r56;
		// 0f8
		const float p106 = k[56] * r91 + p105;
		// 0f9
		const float p107 = k[57] * r94 + p106;
		// 0fa
		const float p108 = sat(k[58] * r98 + p107);
		const float w109 = p108;
		// 0fb
		const float p110 = k[59] * r91;
		// 0fc
		const float p111 = k[60] * r104 + p110;
		ram[at(60)] = w109;
		// 0fd
		const float p112 = sat(k[61] * r101 + p111);
		const float w113 = p112;
		// 0fe
		// 0ff
		ram[at(63)] = w113;
		// 100
		const float m114 = lfo[16];   // m13
		// 101
		// 102
		const float p115 = sat((k[66] + m36) * 2.0f);
		const float r116 = w24(p115);   // r03
		// 103
		const float p117 = sat((k[67] + m72) * 2.0f);
		const float r118 = w24(p117);   // r04
		// 104
		const float p119 = sat((k[68] * r61) * 2.0f);
		const float r120 = w24(p119);   // r01
		// 105
		const float p121 = sat((k[69] * r116) * 2.0f);
		const float r122 = w24(p121);   // r35
		// 106
		const float p123 = sat((k[70] * r118) * 2.0f);
		const float r124 = w24(p123);   // r37
		const float t125 = tv_plain(p119);   // t1
		// 107
		const float p126 = k[71] * r94;
		const float t127 = tv_plain(p121);   // t2
		// 108
		const float p128 = k[72] * r98 + p126;
		const float t129 = tv_plain(p123);   // t3
		// 109
		const float p130 = sat(k[73] * r104 + p128);
		const float r131 = w24(p130);   // r07
		// 10a
		const float p132 = sat(t125 * r120);
		const float r133 = w24(p132);   // r02
		// 10b
		const float p134 = sat(t127 * r122);
		const float r135 = w24(p134);   // r03
		// 10c
		const float p136 = sat(t129 * r124);
		const float r137 = w24(p136);   // r04
		// 10d
		const float p138 = sat(t125 * r133);
		const float r139 = w24(p138);   // r02
		// 10e
		const float p140 = sat(t127 * r135);
		const float r141 = w24(p140);   // r36
		// 10f
		const float p142 = sat(t129 * r137);
		const float r143 = w24(p142);   // r38
		// 110
		const float p144 = sat((k[80] * r120) * 16.0f);
		const float m145 = lfo[17];   // m14
		// 111
		const float p146 = k[81] * r139 + p144;
		// 112
		const float p147 = satpos(k[82] + p146);
		const float r148 = w24(p147);   // r34
		// 113
		const float p149 = k[83] * r101;
		// 114
		const float p150 = k[84] * r104 + p149;
		// 115
		const float p151 = sat(k[85] * r98 + p150);
		const float r152 = w24(p151);   // r08
		// 116
		const float p153 = k[86] * r56;
		// 117
		const float p154 = k[87] * r91 + p153;
		// 118
		const float p155 = sat((k[88] * r131 + p154) * 4.0f);
		const float m156 = w24(p155);   // m28
		// 119
		const float p157 = k[89] * r91;
		// 11a
		const float p158 = k[90] * r56 + p157;
		// 11b
		const float p159 = sat((k[91] * r152 + p158) * 4.0f);
		const float m160 = w24(p159);   // m29
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m156;   // m28
		out[1] = m160;   // m29
		s_p = p159;
		s_m12 = m97;
		s_r34 = r148;
		s_r37 = r124;
		s_r38 = r143;
		s_r35 = r122;
		s_r36 = r141;
		s_m14 = m145;
		s_r20 = r19;
		s_r21 = r22;
		s_r22 = r31;
		s_r23 = r37;
		s_m15 = m72;
		s_r24 = r46;
		s_r25 = r48;
		s_r26 = r56;
		s_r27 = r53;
		s_m13 = m114;
		s_r28 = r64;
		s_r29 = r66;
		s_r2a = r75;
		s_r2b = r78;
		s_r2c = r83;
		s_r2d = r85;
		s_r2e = r91;
		s_r2f = r88;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
	float s_m13 = 0.0f;
	float s_m14 = 0.0f;
	float s_m15 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r20 = 0.0f;
	float s_r21 = 0.0f;
	float s_r22 = 0.0f;
	float s_r23 = 0.0f;
	float s_r24 = 0.0f;
	float s_r25 = 0.0f;
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
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
	float s_r36 = 0.0f;
	float s_r37 = 0.0f;
	float s_r38 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_CHORUS3_H
