// license:BSD-3-Clause
// S-MU2000: インサーション 1: AUTO PAN 2
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_AUTOPAN2_H
#define S_MU2000_DSP_MEG_FX_INS_AUTOPAN2_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_autopan2 : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; s_r39 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * s_m14;
		const int32_t i2 = idx_of(p1);
		// 0c1
		const float p3 = sat((k[1] * in[0]) * 2.0f);
		const float r4 = w24(p3);   // r2a
		// 0c2
		const float p5 = k[2] * s_r2a;
		const float r6 = s_r2a;   // r2b
		const float t7 = tv_index(p1);   // t0
		// 0c3
		const float p8 = k[3] * s_r2b + p5;
		const float q9 = ram[at(3, i2)];
		// 0c4
		const float p10 = k[4] * s_r2c + p8;
		// 0c5
		const float p11 = k[5] * s_r2d + p10;
		const float m12 = q9;   // m05
		// 0c6
		const float p13 = sat((k[6] * r4 + p11) * 2.0f);
		const float r14 = w24(p13);   // r2c
		const float q15 = ram[at(6, i2 + 1)];
		// 0c7
		const float p16 = k[7] * s_r2c;
		const float r17 = s_r2c;   // r2d
		// 0c8
		const float p18 = k[8] * s_r2d + p16;
		const float m19 = q15;   // m06
		// 0c9
		const float p20 = k[9] * s_r2e + p18;
		// 0ca
		const float p21 = k[10] * s_r2f + p20;
		// 0cb
		const float p22 = sat((k[11] * r14 + p21) * 4.0f);
		const float r23 = w24(p22);   // r2e
		// 0cc
		const float p24 = k[12] * s_r2e;
		const float r25 = s_r2e;   // r2f
		// 0cd
		const float p26 = k[13] * s_r2f + p24;
		// 0ce
		const float p27 = k[14] * s_r30 + p26;
		const float r28 = s_r30;   // r31
		// 0cf
		const float p29 = k[15] * s_r31 + p27;
		// 0d0
		const float p30 = sat((k[16] * r23 + p29) * 16.0f);
		const float r31 = w24(p30);   // r30
		// 0d1
		// 0d2
		const float p32 = k[18] * s_m15;
		const int32_t i33 = idx_of(p32);
		// 0d3
		const float p34 = sat((k[19] * in[1]) * 2.0f);
		const float r35 = w24(p34);   // r32
		// 0d4
		const float p36 = k[20] * s_r32;
		const float r37 = s_r32;   // r33
		const float t38 = tv_index(p32);   // t1
		// 0d5
		const float p39 = k[21] * s_r33 + p36;
		const float q40 = ram[at(21, i33)];
		// 0d6
		const float p41 = k[22] * s_r34 + p39;
		// 0d7
		const float p42 = k[23] * s_r35 + p41;
		const float m43 = q40;   // m03
		// 0d8
		const float p44 = sat((k[24] * r35 + p42) * 2.0f);
		const float r45 = w24(p44);   // r34
		const float q46 = ram[at(24, i33 + 1)];
		// 0d9
		const float p47 = k[25] * s_r34;
		const float r48 = s_r34;   // r35
		// 0da
		const float p49 = k[26] * s_r35 + p47;
		const float m50 = q46;   // m04
		// 0db
		const float p51 = k[27] * s_r36 + p49;
		// 0dc
		const float p52 = k[28] * s_r37 + p51;
		// 0dd
		const float p53 = sat((k[29] * r45 + p52) * 4.0f);
		const float r54 = w24(p53);   // r36
		// 0de
		const float p55 = k[30] * s_r36;
		const float r56 = s_r36;   // r37
		// 0df
		const float p57 = k[31] * s_r37 + p55;
		// 0e0
		const float p58 = k[32] * s_r38 + p57;
		const float r59 = s_r38;   // r39
		// 0e1
		const float p60 = k[33] * s_r39 + p58;
		// 0e2
		const float p61 = sat((k[34] * r54 + p60) * 16.0f);
		const float r62 = w24(p61);   // r38
		// 0e3
		const float p63 = k[35];
		// 0e4
		const float p64 = s_r20 - p63;
		const float r65 = w24(p64);   // r01
		// 0e5
		const float p66 = t7 * m12 - m12;
		// 0e6
		const float p67 = sat(t7 * m19 - p66);
		const float r68 = w24(p67);   // r08
		// 0e7
		const float p69 = k[39] * r65;
		const int32_t i70 = idx_of(p69);
		const float t71 = k[39];   // t2（定数）
		// 0e8
		// 0e9
		const float p72 = k[41];
		const float t73 = tv_index(p69);   // t0
		// 0ea
		const float p74 = s_r20 - p72;
		const float r75 = w24(p74);   // r01
		const float q76 = tab(42, i70);
		// 0eb
		const float p77 = t38 * m43 - m43;
		// 0ec
		const float p78 = sat(t38 * m50 - p77);
		const float m79 = q76;   // m01
		const float r80 = w24(p78);   // r09
		// 0ed
		const float p81 = t71 * r75;
		const int32_t i82 = idx_of(p81);
		const float q83 = tab(45, i70 + 1);
		// 0ee
		// 0ef
		const float p84 = k[47];
		const float m85 = q83;   // m02
		const float t86 = tv_index(p81);   // t1
		// 0f0
		const float p87 = s_r20 - p84;
		const float r88 = w24(p87);   // r01
		const float q89 = tab(48, i82);
		// 0f1
		const float p90 = t73 * m79 - m79;
		// 0f2
		const float p91 = sat(t73 * m85 - p90);
		const float m92 = q89;   // m01
		const float r93 = w24(p91);   // r21
		// 0f3
		const float p94 = t71 * r88;
		const int32_t i95 = idx_of(p94);
		const float q96 = tab(51, i82 + 1);
		// 0f4
		// 0f5
		const float p97 = k[53];
		const float m98 = q96;   // m02
		const float t99 = tv_index(p94);   // t0
		// 0f6
		const float p100 = s_r20 - p97;
		const float r101 = w24(p100);   // r01
		const float q102 = tab(54, i95);
		// 0f7
		const float p103 = t86 * m92 - m92;
		// 0f8
		const float p104 = sat(t86 * m98 - p103);
		const float m105 = q102;   // m01
		const float r106 = w24(p104);   // r22
		// 0f9
		const float p107 = t71 * r101;
		const int32_t i108 = idx_of(p107);
		const float q109 = tab(57, i95 + 1);
		// 0fa
		const float p110 = k[58];
		// 0fb
		const float p111 = sat(k[59] + (p110 * (1.0f / 32768.0f)));
		const float m112 = q109;   // m02
		const float t113 = tv_index(p107);   // t1
		// 0fc
		const float p114 = s_r20 + p111;
		const float r115 = w24(p114);   // r20
		const float q116 = tab(60, i108);
		// 0fd
		const float p117 = t99 * m105 - m105;
		// 0fe
		const float p118 = sat(t99 * m112 - p117);
		const float m119 = q116;   // m01
		const float r120 = w24(p118);   // r23
		// 0ff
		const float p121 = s_m12;
		const float q122 = tab(63, i108 + 1);
		// 100
		const float p123 = s_m13;
		// 101
		const float p124 = k[65] * r62;
		const float m125 = q122;   // m02
		const float t126 = tv_plain(p121);   // t2
		// 102
		const float p127 = sat(k[66] * r31 + p124);
		const float w128 = p127;
		const float t129 = tv_plain(p123);   // t5
		// 103
		const float p130 = t113 * m119 - m119;
		// 104
		const float p131 = sat(t113 * m125 - p130);
		const float r132 = w24(p131);   // r24
		// 105
		const float p133 = k[69] * r93;
		const float t134 = k[69];   // t0（定数）
		ram[at(69)] = w128;
		// 106
		const float p135 = sat(k[70] * r120 + p133);
		const float r136 = w24(p135);   // r02
		const float t137 = k[70];   // t1（定数）
		// 107
		const float p138 = t134 * r106;
		// 108
		const float p139 = sat(t137 * r132 + p138);
		const float r140 = w24(p139);   // r03
		// 109
		const float p141 = (k[73] * r136) * 2.0f;
		// 10a
		const float p142 = satpos(k[74] + p141);
		const float r143 = w24(p142);   // r02
		// 10b
		const float p144 = (k[75] * r140) * 2.0f;
		// 10c
		const float p145 = satpos(k[76] + p144);
		const float r146 = w24(p145);   // r03
		// 10d
		// 10e
		const float p147 = satpos((k[78] * r143) * 16.0f);
		const float r148 = w24(p147);   // r02
		const float t149 = k[78];   // t0（定数）
		// 10f
		const float p150 = satpos((t149 * r146) * 16.0f);
		const float r151 = w24(p150);   // r03
		// 110
		// 111
		const float p152 = satpos((t149 * r148) * 16.0f);
		const float m153 = w24(p152);   // m14
		// 112
		const float p154 = satpos((t149 * r151) * 16.0f);
		const float m155 = w24(p154);   // m15
		// 113
		const float p156 = sat(k[83] * r62);
		const float w157 = p156;
		// 114
		const float p158 = k[84] * m153;
		const float m159 = w24(p158);   // m12
		// 115
		const float p160 = k[85] * m155;
		const float m161 = w24(p160);   // m13
		// 116
		// 117
		ram[at(87)] = w157;
		// 118
		const float p162 = sat(t126 * r68 + r68);
		const float r163 = w24(p162);   // r08
		// 119
		const float p164 = sat(t129 * r80 + r80);
		const float r165 = w24(p164);   // r09
		// 11a
		// 11b
		const float p166 = k[91] * r163;
		// 11c
		const float p167 = sat((k[92] * r31 + p166) * 4.0f);
		const float m168 = w24(p167);   // m28
		// 11d
		const float p169 = k[93] * r165;
		// 11e
		const float p170 = sat((k[94] * r62 + p169) * 4.0f);
		const float m171 = w24(p170);   // m29
		// 11f
		out[0] = m168;   // m28
		out[1] = m171;   // m29
		s_p = p170;
		s_m14 = m153;
		s_r2a = r4;
		s_r2b = r6;
		s_r2c = r14;
		s_r2d = r17;
		s_r2e = r23;
		s_r2f = r25;
		s_r30 = r31;
		s_r31 = r28;
		s_m15 = m155;
		s_r32 = r35;
		s_r33 = r37;
		s_r34 = r45;
		s_r35 = r48;
		s_r36 = r54;
		s_r37 = r56;
		s_r38 = r62;
		s_r39 = r59;
		s_r20 = r115;
		s_m12 = m159;
		s_m13 = m161;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_AUTOPAN2_H
