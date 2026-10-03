// license:BSD-3-Clause
// S-MU2000: インサーション 1: WAH+DT+DLY, WAH+OD+DLY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_WAH_DT_DELAY_H
#define S_MU2000_DSP_MEG_FX_INS_WAH_DT_DELAY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_wah_dt_delay : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x1000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat((k[0] * in[0]) * 2.0f);
		const float r2 = w24(p1);   // r20
		// 0c1
		const float p3 = k[1] * s_r21;
		// 0c2
		const float p4 = k[2] * s_r20 + p3;
		// 0c3
		const float p5 = sat((k[3] * r2 + p4) * 2.0f);
		const float r6 = w24(p5);   // r21
		// 0c4
		const float p7 = k[4] * s_r22;
		const float m8 = lfo[12];   // m34
		// 0c5
		const float p9 = k[5] * s_r21 + p7;
		// 0c6
		const float p10 = sat((k[6] * r6 + p9) * 4.0f);
		const float r11 = w24(p10);   // r22
		// 0c7
		const float p12 = sat((k[7] * in[1]) * 2.0f);
		const float r13 = w24(p12);   // r23
		// 0c8
		const float p14 = k[8] * s_r24;
		// 0c9
		const float p15 = k[9] * s_r23 + p14;
		// 0ca
		const float p16 = sat((k[10] * r13 + p15) * 2.0f);
		const float r17 = w24(p16);   // r24
		// 0cb
		const float p18 = k[11] * s_r25;
		// 0cc
		const float p19 = k[12] * s_r24 + p18;
		// 0cd
		const float p20 = sat((k[13] * r17 + p19) * 4.0f);
		const float r21 = w24(p20);   // r25
		// 0ce
		const float p22 = k[14] * s_m14;
		// 0cf
		const float p23 = sat(k[15] * s_m15 + p22);
		const float r24 = w24(p23);   // r2c
		// 0d0
		const float p25 = k[16] * s_r2d;
		// 0d1
		const float p26 = k[17] * s_r2c + p25;
		// 0d2
		const float p27 = sat((k[18] * r24 + p26) * 2.0f);
		const float r28 = w24(p27);   // r2d
		// 0d3
		const float p29 = k[19] * s_r2e;
		// 0d4
		const float p30 = k[20] * s_r2d + p29;
		// 0d5
		const float p31 = sat((k[21] * r28 + p30) * 2.0f);
		const float r32 = w24(p31);   // r2e
		// 0d6
		const float p33 = k[22] * r11;
		// 0d7
		const float p34 = sat(k[23] * r21 + p33);
		const float r35 = w24(p34);   // r26
		// 0d8
		const float p36 = sat((k[24] * r32) * 16.0f);
		const float r37 = w24(p36);   // r02
		// 0d9
		const float p38 = k[25] * s_r26;
		// 0da
		const float p39 = k[26] * r35 + p38;
		// 0db
		const float p40 = sat((k[27] * s_r27 + p39) * 2.0f);
		const float r41 = w24(p40);   // r27
		// 0dc
		const float p42 = sat((k[28] * r37) * 16.0f);
		const float r43 = w24(p42);   // r02
		// 0dd
		const float p44 = k[29] * s_m12 - r35;
		// 0de
		const float p45 = sat(k[30] * s_r2b - p44);
		const float m46 = w24(p45);   // m02
		// 0df
		const float p47 = satabs((k[31] * r41) * 16.0f);
		const float m48 = w24(p47);   // m01
		// 0e0
		const float p49 = sat((k[32] * r43) * 16.0f);
		const float r50 = w24(p49);   // r02
		// 0e1
		const float p51 = k[33] * s_r34;
		// 0e2
		const float p52 = sat((k[34] * s_m14 + p51) * 4.0f);
		const float m53 = w24(p52);   // m05
		// 0e3
		const float p54 = k[35] * s_r34;
		// 0e4
		const float p55 = sat((k[36] * s_m15 + p54) * 4.0f);
		const float m56 = w24(p55);   // m06
		// 0e5
		const float p57 = sat((k[37] * r50) * 16.0f);
		const float r58 = w24(p57);   // r03
		// 0e6
		const float p59 = satpos(k[38] * s_r28 - m48);
		// 0e7
		const float p60 = sat(k[39] * m48 + p59);
		const float r61 = w24(p60);   // r28
		// 0e8
		const float p62 = k[40] * s_r29;
		// 0e9
		const float p63 = k[41] * s_r29 + (p62 * (1.0f / 32768.0f));
		// 0ea
		const float p64 = k[42] * m48 - p63;
		const float q65 = ram[at(42)];
		// 0eb
		const float p66 = satpos(k[43] * r61 - p64);
		// 0ec
		const float p67 = sat(k[44] * m48 + p66);
		const float m68 = q65;   // m09
		const float r69 = w24(p67);   // r29
		// 0ed
		const float p70 = sat((k[45] * r58) * 16.0f);
		const float r71 = w24(p70);   // r04
		const float q72 = ram[at(45)];
		// 0ee
		const float p73 = k[46] * s_r29;
		// 0ef
		const float p74 = k[47] * r69 + p73;
		const float m75 = q72;   // m08
		// 0f0
		const float p76 = sat(k[48] * s_r2a + p74);
		const float r77 = w24(p76);   // r2a
		const float q78 = ram[at(48)];
		// 0f1
		const float p79 = sat((k[49] * r71) * 16.0f);
		const float r80 = w24(p79);   // r05
		// 0f2
		const float p81 = k[50];
		const float m82 = q78;   // m07
		// 0f3
		const float p83 = k[51] * m8 + p81;
		// 0f4
		const float p84 = sat(k[52] * r77 + p83);
		const float r85 = w24(p84);   // r01
		// 0f5
		const float p86 = sat((k[53] * r80) * 16.0f);
		const float m87 = w24(p86);   // m04
		// 0f6
		const float p88 = k[54] * m68;
		const float m89 = m68;   // m13
		const float t90 = tv_plain(p84);   // t1
		const float q91 = ram[at(54)];
		// 0f7
		const float p92 = k[55] * s_m13 + p88;
		// 0f8
		const float p93 = sat((k[56] * s_r35 + p92) * 2.0f);
		const float m94 = q91;   // m09
		const float r95 = w24(p93);   // r35
		// 0f9
		const float p96 = sat(t90 * r85);
		const float r97 = w24(p96);   // r01
		// 0fa
		const float p98 = k[58] * m53;
		// 0fb
		const float p99 = (k[59] * m56 + p98) * 2.0f;
		const float t100 = tv_plain(p96);   // t1
		// 0fc
		const float p101 = sat(k[60] * r95 + p99);
		const float w102 = p101;
		// 0fd
		const float p103 = t100 * r97;
		// 0fe
		const float p104 = sat(k[62] + p103);
		// 0ff
		const float p105 = sat((k[63] * m87) * 16.0f);
		const float r106 = w24(p105);   // r02
		ram[at(63)] = w102;
		// 100
		const float p107 = k[64] * r50;
		const float t108 = tv_plain(p104);   // t1
		// 101
		const float p109 = k[65] * r58 + p107;
		// 102
		const float p110 = k[66] * r71 + p109;
		// 103
		const float p111 = k[67] * r80 + p110;
		// 104
		const float p112 = k[68] * m87 + p111;
		// 105
		const float p113 = sat(k[69] * r106 + p112);
		const float r114 = w24(p113);   // r02
		// 106
		const float p115 = sat(t108 * m46 + s_r2b);
		const float r116 = w24(p115);   // r2b
		// 107
		const float p117 = k[71] * m94;
		// 108
		const float p118 = sat(k[72] * m82 + p117);
		const float r119 = w24(p118);   // r03
		// 109
		const float p120 = k[73] * m75;
		// 10a
		const float p121 = sat(k[74] * m82 + p120);
		const float r122 = w24(p121);   // r04
		// 10b
		const float p123 = sat(t108 * r116 + s_m12);
		const float m124 = w24(p123);   // m12
		// 10c
		const float p125 = sat(k[76] * r114);
		const float r126 = w24(p125);   // r2f
		// 10d
		const float p127 = k[77] * s_r30;
		// 10e
		const float p128 = k[78] * s_r2f + p127;
		// 10f
		const float p129 = sat((k[79] * r126 + p128) * 2.0f);
		const float r130 = w24(p129);   // r30
		// 110
		const float p131 = k[80] * s_r31;
		// 111
		const float p132 = k[81] * s_r30 + p131;
		const float r133 = s_r30;   // r31
		// 112
		const float p134 = k[82] * r130 + p132;
		// 113
		const float p135 = k[83] * s_r32 + p134;
		const float r136 = s_r32;   // r33
		// 114
		const float p137 = sat((k[84] * s_r33 + p135) * 2.0f);
		const float r138 = w24(p137);   // r32
		// 115
		const float p139 = k[85] * s_r34;
		// 116
		const float p140 = k[86] * s_r32 + p139;
		// 117
		const float p141 = sat((k[87] * r138 + p140) * 4.0f);
		const float r142 = w24(p141);   // r34
		// 118
		const float p143 = k[88] * r11;
		// 119
		const float p144 = sat((k[89] * r116 + p143) * 4.0f);
		const float m145 = w24(p144);   // m14
		// 11a
		const float p146 = k[90] * r21;
		// 11b
		const float p147 = sat((k[91] * r116 + p146) * 4.0f);
		const float m148 = w24(p147);   // m15
		// 11c
		const float p149 = k[92] * m53;
		// 11d
		const float p150 = sat((k[93] * r119 + p149) * 4.0f);
		const float m151 = w24(p150);   // m28
		// 11e
		const float p152 = k[94] * m56;
		// 11f
		const float p153 = sat((k[95] * r122 + p152) * 4.0f);
		const float m154 = w24(p153);   // m29
		out[0] = m151;   // m28
		out[1] = m154;   // m29
		s_p = p153;
		s_r21 = r6;
		s_r20 = r2;
		s_r22 = r11;
		s_r24 = r17;
		s_r23 = r13;
		s_r25 = r21;
		s_m14 = m145;
		s_m15 = m148;
		s_r2d = r28;
		s_r2c = r24;
		s_r2e = r32;
		s_r26 = r35;
		s_r27 = r41;
		s_m12 = m124;
		s_r2b = r116;
		s_r34 = r142;
		s_r28 = r61;
		s_r29 = r69;
		s_r2a = r77;
		s_m13 = m89;
		s_r35 = r95;
		s_r30 = r130;
		s_r2f = r126;
		s_r31 = r133;
		s_r32 = r138;
		s_r33 = r136;
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
	float s_r30 = 0.0f;
	float s_r31 = 0.0f;
	float s_r32 = 0.0f;
	float s_r33 = 0.0f;
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_WAH_DT_DELAY_H
