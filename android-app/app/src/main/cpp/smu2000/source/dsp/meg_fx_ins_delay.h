// license:BSD-3-Clause
// S-MU2000: インサーション 1: DELAY LCR, DELAY L,R, ECHO, CROSSDELAY, T.DELAY, T.ECHO, T.CRS DLY, CHORUS 1, CHORUS 2, CHORUS 4, GM CHORUS1, GM CHORUS2, GM CHORUS3, GM CHORUS4, FB CHORUS, CELESTE 1, CELESTE 2, CELESTE 3, CELESTE 4, FLANGER 1, FLANGER 2, FLANGER 3, GM FLANGER, SYMPHONIC, T.FLANGER, THRU
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_DELAY_H
#define S_MU2000_DSP_MEG_FX_INS_DELAY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_delay : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x3f000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * s_m12;
		const float m2 = lfo[12];   // m34
		// 0c1
		const float p3 = satpos(k[1] * s_r34 + p1);
		const int32_t i4 = idx_of(p3);
		// 0c2
		const float p5 = sat((k[2] * s_r37) * 16.0f);
		// 0c3
		const float p6 = k[3] * s_r38 + p5;
		const float t7 = tv_index(p3);   // t1
		// 0c4
		const float p8 = satpos(k[4] + p6);
		const float r9 = w24(p8);   // r04
		// 0c5
		const float p10 = sat((k[5] * s_r35) * 16.0f);
		// 0c6
		const float p11 = k[6] * s_r36 + p10;
		const float q12 = ram[at(6, i4)];
		// 0c7
		const float p13 = satpos(k[7] + p11);
		const float r14 = w24(p13);   // r03
		// 0c8
		const float p15 = k[8] * s_m14;
		const float m16 = q12;   // m01
		// 0c9
		const float p17 = satpos(k[9] * r9 + p15);
		const int32_t i18 = idx_of(p17);
		const float q19 = ram[at(9, i4 + 1)];
		// 0ca
		const float p20 = sat((k[10] * in[0]) * 2.0f);
		const float r21 = w24(p20);   // r20
		// 0cb
		const float p22 = k[11] * s_r20;
		const float m23 = q19;   // m02
		const float r24 = s_r20;   // r21
		const float t25 = tv_index(p17);   // t3
		// 0cc
		const float p26 = k[12] * s_r21 + p22;
		const float q27 = ram[at(12, i18)];
		// 0cd
		const float p28 = k[13] * s_r22 + p26;
		// 0ce
		const float p29 = k[14] * s_r23 + p28;
		const float m30 = q27;   // m05
		// 0cf
		const float p31 = sat((k[15] * r21 + p29) * 2.0f);
		const float r32 = w24(p31);   // r22
		const float q33 = ram[at(15, i18 + 1)];
		// 0d0
		const float p34 = k[16] * s_r22;
		const float m35 = lfo[13];   // m35
		const float r36 = s_r22;   // r23
		// 0d1
		const float p37 = k[17] * s_r23 + p34;
		const float m38 = q33;   // m06
		// 0d2
		const float p39 = k[18] * s_r24 + p37;
		const float q40 = ram[at(18, i18)];
		// 0d3
		const float p41 = k[19] * s_r25 + p39;
		// 0d4
		const float p42 = sat((k[20] * r32 + p41) * 2.0f);
		const float m43 = q40;   // m07
		const float r44 = w24(p42);   // r24
		// 0d5
		const float p45 = k[21] * s_r24;
		const float r46 = s_r24;   // r25
		const float q47 = ram[at(21, i18 + 1)];
		// 0d6
		const float p48 = k[22] * s_r25 + p45;
		// 0d7
		const float p49 = k[23] * s_r26 + p48;
		const float m50 = q47;   // m08
		const float r51 = s_r26;   // r27
		// 0d8
		const float p52 = k[24] * s_r27 + p49;
		// 0d9
		const float p53 = sat((k[25] * r44 + p52) * 16.0f);
		const float r54 = w24(p53);   // r26
		// 0da
		const float p55 = k[26] * s_m13;
		// 0db
		const float p56 = satpos(k[27] * r14 + p55);
		const int32_t i57 = idx_of(p56);
		// 0dc
		const float p58 = sat((k[28] + m2) * 2.0f);
		const float r59 = w24(p58);   // r01
		// 0dd
		const float p60 = sat((k[29] * in[1]) * 2.0f);
		const float r61 = w24(p60);   // r28
		const float t62 = tv_index(p56);   // t2
		// 0de
		const float p63 = k[30] * s_r28;
		const float r64 = s_r28;   // r29
		const float q65 = ram[at(30, i57)];
		// 0df
		const float p66 = k[31] * s_r29 + p63;
		// 0e0
		const float p67 = k[32] * s_r2a + p66;
		const float m68 = q65;   // m03
		// 0e1
		const float p69 = k[33] * s_r2b + p67;
		const float m70 = lfo[14];   // m36
		const float q71 = ram[at(33, i57 + 1)];
		// 0e2
		const float p72 = sat((k[34] * r61 + p69) * 2.0f);
		const float r73 = w24(p72);   // r2a
		// 0e3
		const float p74 = k[35] * s_r2a;
		const float m75 = q71;   // m04
		const float r76 = s_r2a;   // r2b
		// 0e4
		const float p77 = k[36] * s_r2b + p74;
		// 0e5
		const float p78 = k[37] * s_r2c + p77;
		// 0e6
		const float p79 = k[38] * s_r2d + p78;
		// 0e7
		const float p80 = sat((k[39] * r73 + p79) * 2.0f);
		const float r81 = w24(p80);   // r2c
		// 0e8
		const float p82 = k[40] * s_r2c;
		const float r83 = s_r2c;   // r2d
		// 0e9
		const float p84 = k[41] * s_r2d + p82;
		// 0ea
		const float p85 = k[42] * s_r2e + p84;
		const float r86 = s_r2e;   // r2f
		// 0eb
		const float p87 = k[43] * s_r2f + p85;
		// 0ec
		const float p88 = sat((k[44] * r81 + p87) * 16.0f);
		const float r89 = w24(p88);   // r2e
		// 0ed
		const float p90 = t7 * m16 - m16;
		// 0ee
		const float p91 = sat(t7 * m23 - p90);
		const float r92 = w24(p91);   // r30
		// 0ef
		const float p93 = k[47] * s_r30;
		// 0f0
		const float p94 = k[48] * s_r31 + p93;
		const float m95 = lfo[15];   // m12
		// 0f1
		const float p96 = sat((k[49] * r92 + p94) * 2.0f);
		const float r97 = w24(p96);   // r31
		// 0f2
		const float p98 = t62 * m68 - m68;
		// 0f3
		const float p99 = sat(t62 * m75 - p98);
		const float r100 = w24(p99);   // r32
		// 0f4
		const float p101 = k[52] * s_r32;
		// 0f5
		const float p102 = k[53] * s_r33 + p101;
		// 0f6
		const float p103 = sat((k[54] * r100 + p102) * 2.0f);
		const float r104 = w24(p103);   // r33
		// 0f7
		const float p105 = k[55] * r54;
		// 0f8
		const float p106 = k[56] * r89 + p105;
		// 0f9
		const float p107 = k[57] * r97 + p106;
		// 0fa
		const float p108 = sat(k[58] * r104 + p107);
		const float w109 = p108;
		// 0fb
		const float p110 = k[59] * r89;
		// 0fc
		const float p111 = k[60] * r97 + p110;
		ram[at(60)] = w109;
		// 0fd
		const float p112 = sat(k[61] * r104 + p111);
		const float w113 = p112;
		// 0fe
		const float p114 = t25 * m30 - m30;
		// 0ff
		const float p115 = sat(t25 * m38 - p114);
		const float r116 = w24(p115);   // r05
		ram[at(63)] = w113;
		// 100
		const float p117 = t25 * m43 - m43;
		const float m118 = lfo[16];   // m13
		// 101
		const float p119 = sat(t25 * m50 - p117);
		const float r120 = w24(p119);   // r06
		// 102
		const float p121 = sat((k[66] + m35) * 2.0f);
		const float r122 = w24(p121);   // r03
		// 103
		const float p123 = sat((k[67] + m70) * 2.0f);
		const float r124 = w24(p123);   // r04
		// 104
		const float p125 = sat((k[68] * r59) * 2.0f);
		const float r126 = w24(p125);   // r01
		// 105
		const float p127 = sat((k[69] * r122) * 2.0f);
		const float r128 = w24(p127);   // r35
		// 106
		const float p129 = sat((k[70] * r124) * 2.0f);
		const float r130 = w24(p129);   // r37
		const float t131 = tv_plain(p125);   // t1
		// 107
		const float p132 = k[71] * r92;
		const float t133 = tv_plain(p127);   // t2
		// 108
		const float p134 = k[72] * r116 + p132;
		const float t135 = tv_plain(p129);   // t3
		// 109
		const float p136 = sat(k[73] * r120 + p134);
		const float r137 = w24(p136);   // r07
		// 10a
		const float p138 = sat(t131 * r126);
		const float r139 = w24(p138);   // r02
		// 10b
		const float p140 = sat(t133 * r128);
		const float r141 = w24(p140);   // r03
		// 10c
		const float p142 = sat(t135 * r130);
		const float r143 = w24(p142);   // r04
		// 10d
		const float p144 = sat(t131 * r139);
		const float r145 = w24(p144);   // r02
		// 10e
		const float p146 = sat(t133 * r141);
		const float r147 = w24(p146);   // r36
		// 10f
		const float p148 = sat(t135 * r143);
		const float r149 = w24(p148);   // r38
		// 110
		const float p150 = sat((k[80] * r126) * 16.0f);
		const float m151 = lfo[17];   // m14
		// 111
		const float p152 = k[81] * r145 + p150;
		// 112
		const float p153 = satpos(k[82] + p152);
		const float r154 = w24(p153);   // r34
		// 113
		const float p155 = k[83] * r100;
		// 114
		const float p156 = k[84] * r120 + p155;
		// 115
		const float p157 = sat(k[85] * r116 + p156);
		const float r158 = w24(p157);   // r08
		// 116
		const float p159 = k[86] * r54;
		// 117
		const float p160 = k[87] * r89 + p159;
		// 118
		const float p161 = sat((k[88] * r137 + p160) * 4.0f);
		const float m162 = w24(p161);   // m28
		// 119
		const float p163 = k[89] * r89;
		// 11a
		const float p164 = k[90] * r54 + p163;
		// 11b
		const float p165 = sat((k[91] * r158 + p164) * 4.0f);
		const float m166 = w24(p165);   // m29
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m162;   // m28
		out[1] = m166;   // m29
		s_p = p165;
		s_m12 = m95;
		s_r34 = r154;
		s_r37 = r130;
		s_r38 = r149;
		s_r35 = r128;
		s_r36 = r147;
		s_m14 = m151;
		s_r20 = r21;
		s_r21 = r24;
		s_r22 = r32;
		s_r23 = r36;
		s_r24 = r44;
		s_r25 = r46;
		s_r26 = r54;
		s_r27 = r51;
		s_m13 = m118;
		s_r28 = r61;
		s_r29 = r64;
		s_r2a = r73;
		s_r2b = r76;
		s_r2c = r81;
		s_r2d = r83;
		s_r2e = r89;
		s_r2f = r86;
		s_r30 = r92;
		s_r31 = r97;
		s_r32 = r100;
		s_r33 = r104;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
	float s_m13 = 0.0f;
	float s_m14 = 0.0f;
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
	float s_r30 = 0.0f;
	float s_r31 = 0.0f;
	float s_r32 = 0.0f;
	float s_r33 = 0.0f;
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
	float s_r36 = 0.0f;
	float s_r37 = 0.0f;
	float s_r38 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_DELAY_H
