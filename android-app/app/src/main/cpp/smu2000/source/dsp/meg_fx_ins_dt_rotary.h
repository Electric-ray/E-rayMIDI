// license:BSD-3-Clause
// S-MU2000: インサーション 1: DT +RTRY, OD +RTRY, AMP+RTRY, DT +2RTRY, OD +2RTRY, AMP+2RTRY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_DT_ROTARY_H
#define S_MU2000_DSP_MEG_FX_INS_DT_ROTARY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_dt_rotary : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xf000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat(k[0] * s_m12);
		const float m2 = lfo[12];   // m12
		const int32_t i3 = idx_of(p1);
		// 0c1
		const float p4 = sat(k[1] * s_m13);
		const int32_t j5 = idx_of(p4);
		// 0c2
		const float p6 = k[2] * in[0];
		const float t7 = tv_index(p1);   // t1
		// 0c3
		const float p8 = sat(k[3] * in[1] + p6);
		const float r9 = w24(p8);   // r20
		const float t10 = tv_index(p4);   // t2
		const float q11 = ram[at(3, i3)];
		// 0c4
		const float p12 = k[4] * s_r20;
		// 0c5
		const float p13 = k[5] * s_r21 + p12;
		const float m14 = q11;   // m01
		// 0c6
		const float p15 = sat((k[6] * r9 + p13) * 2.0f);
		const float r16 = w24(p15);   // r21
		const float q17 = ram[at(6, i3 + 1)];
		// 0c7
		const float p18 = sat(k[7] * s_r21);
		// 0c8
		const float p19 = k[8] * s_r22 + p18;
		const float m20 = q17;   // m02
		// 0c9
		const float p21 = sat((k[9] * r16 + p19) * 2.0f);
		const float r22 = w24(p21);   // r22
		const float q23 = ram[at(9, j5)];
		// 0ca
		const float p24 = t7 * m14 - m14;
		// 0cb
		const float p25 = sat(t7 * m20 - p24);
		const float m26 = q23;   // m03
		const float r27 = w24(p25);   // r07
		// 0cc
		const float p28 = sat(k[12] * s_m14);
		const int32_t i29 = idx_of(p28);
		const float q30 = ram[at(12, j5 + 1)];
		// 0cd
		const float p31 = sat(k[13] * s_m15);
		const int32_t j32 = idx_of(p31);
		// 0ce
		const float p33 = sat(k[14] * m2);
		const float m34 = q30;   // m04
		const float t35 = tv_index(p28);   // t3
		// 0cf
		const float p36 = sat((k[15] * r22) * 16.0f);
		const float r37 = w24(p36);   // r01
		const float t38 = tv_index(p31);   // t5
		const float q39 = ram[at(15, i29)];
		// 0d0
		const float m40 = lfo[13];   // m13
		const float t41 = tv_plain(p33);   // t1
		// 0d1
		const float p42 = t10 * m26 - m26;
		const float m43 = q39;   // m05
		// 0d2
		const float p44 = sat(t10 * m34 - p42);
		const float r45 = w24(p44);   // r08
		const float q46 = ram[at(18, i29 + 1)];
		// 0d3
		const float p47 = sat((k[19] * r37) * 16.0f);
		const float r48 = w24(p47);   // r01
		// 0d4
		const float p49 = k[20] * s_r2c;
		const float m50 = q46;   // m06
		// 0d5
		const float p51 = k[21] * s_r2b + p49;
		const float r52 = s_r2b;   // r2c
		const float q53 = ram[at(21, j32)];
		// 0d6
		const float p54 = k[22] * s_r2e + p51;
		// 0d7
		const float p55 = k[23] * s_r2d + p54;
		const float m56 = q53;   // m07
		// 0d8
		const float p57 = sat((k[24] * s_r37 + p55) * 2.0f);
		const float r58 = w24(p57);   // r2d
		const float q59 = ram[at(24, j32 + 1)];
		// 0d9
		const float p60 = sat((k[25] * r48) * 16.0f);
		const float r61 = w24(p60);   // r01
		// 0da
		const float p62 = k[26] * s_r2d;
		const float m63 = q59;   // m08
		const float r64 = s_r2d;   // r2e
		// 0db
		const float p65 = k[27] * s_r2e + p62;
		// 0dc
		const float p66 = k[28] * s_r30 + p65;
		// 0dd
		const float p67 = k[29] * s_r2f + p66;
		const float r68 = s_r2f;   // r30
		// 0de
		const float p69 = sat((k[30] * r58 + p67) * 16.0f);
		const float r70 = w24(p69);   // r2f
		// 0df
		const float p71 = sat((k[31] * r61) * 16.0f);
		const float r72 = w24(p71);   // r02
		// 0e0
		const float p73 = t35 * m43 - m43;
		const float m74 = lfo[14];   // m14
		// 0e1
		const float p75 = sat(t35 * m50 - p73);
		const float r76 = w24(p75);   // r09
		// 0e2
		const float p77 = sat((k[34] * r72) * 16.0f);
		const float r78 = w24(p77);   // r03
		// 0e3
		const float p79 = t38 * m56 - m56;
		// 0e4
		const float p80 = sat(t38 * m63 - p79);
		const float m81 = w24(p80);   // m09
		// 0e5
		const float p82 = sat((k[37] * r78) * 16.0f);
		const float r83 = w24(p82);   // r04
		// 0e6
		const float p84 = k[38] * s_r32;
		// 0e7
		const float p85 = k[39] * s_r31 + p84;
		const float r86 = s_r31;   // r32
		// 0e8
		const float p87 = k[40] * s_r34 + p85;
		// 0e9
		const float p88 = k[41] * s_r33 + p87;
		// 0ea
		const float p89 = sat((k[42] * s_r38 + p88) * 2.0f);
		const float r90 = w24(p89);   // r33
		// 0eb
		const float p91 = sat((k[43] * r83) * 16.0f);
		const float r92 = w24(p91);   // r05
		// 0ec
		const float p93 = k[44] * s_r33;
		const float r94 = s_r33;   // r34
		// 0ed
		const float p95 = k[45] * s_r34 + p93;
		// 0ee
		const float p96 = k[46] * s_r36 + p95;
		// 0ef
		const float p97 = k[47] * s_r35 + p96;
		const float r98 = s_r35;   // r36
		// 0f0
		const float p99 = sat((k[48] * r90 + p97) * 16.0f);
		const float m100 = lfo[15];   // m15
		const float r101 = w24(p99);   // r35
		// 0f1
		const float p102 = sat((k[49] * r92) * 16.0f);
		const float r103 = w24(p102);   // r06
		// 0f2
		const float p104 = sat(k[50] * m40);
		// 0f3
		const float p105 = sat(k[51] * m74);
		// 0f4
		const float p106 = k[52] * r70;
		const float t107 = tv_plain(p104);   // t2
		// 0f5
		const float p108 = sat(k[53] * r101 + p106);
		const float w109 = p108;
		const float t110 = tv_plain(p105);   // t3
		// 0f6
		const float p111 = k[54] * r61;
		// 0f7
		const float p112 = k[55] * r72 + p111;
		// 0f8
		const float p113 = k[56] * r78 + p112;
		// 0f9
		const float p114 = k[57] * r83 + p113;
		ram[at(57)] = w109;
		// 0fa
		const float p115 = k[58] * r92 + p114;
		// 0fb
		const float p116 = sat(k[59] * r103 + p115);
		const float r117 = w24(p116);   // r01
		// 0fc
		const float p118 = sat(k[60] * r101);
		const float w119 = p118;
		// 0fd
		const float p120 = sat(k[61] * m100);
		// 0fe
		const float p121 = sat(k[62] * r117);
		const float r122 = w24(p121);   // r23
		// 0ff
		const float p123 = k[63] * s_r24;
		const float t124 = tv_plain(p120);   // t5
		ram[at(63)] = w119;
		// 100
		const float p125 = k[64] * s_r23 + p123;
		const float r126 = s_r23;   // r24
		// 101
		const float p127 = k[65] * s_r26 + p125;
		// 102
		const float p128 = k[66] * s_r25 + p127;
		// 103
		const float p129 = sat((k[67] * r122 + p128) * 2.0f);
		const float r130 = w24(p129);   // r25
		// 104
		const float p131 = k[68] * s_r25;
		const float r132 = s_r25;   // r26
		// 105
		const float p133 = k[69] * s_r26 + p131;
		// 106
		const float p134 = k[70] * s_r28 + p133;
		// 107
		const float p135 = k[71] * s_r27 + p134;
		// 108
		const float p136 = sat((k[72] * r130 + p135) * 2.0f);
		const float r137 = w24(p136);   // r27
		// 109
		const float p138 = k[73] * s_r27;
		const float r139 = s_r27;   // r28
		// 10a
		const float p140 = k[74] * s_r28 + p138;
		// 10b
		const float p141 = k[75] * s_r29 + p140;
		const float r142 = s_r29;   // r2a
		// 10c
		const float p143 = k[76] * s_r2a + p141;
		// 10d
		const float p144 = sat((k[77] * r137 + p143) * 16.0f);
		const float r145 = w24(p144);   // r29
		// 10e
		const float p146 = t41 * r27 + r27;
		const float r147 = w24(p146);   // r01
		// 10f
		const float p148 = t107 * r45 + r45;
		const float r149 = w24(p148);   // r02
		// 110
		const float p150 = t110 * r76 + r76;
		const float r151 = w24(p150);   // r03
		// 111
		const float p152 = t124 * m81 + m81;
		const float r153 = w24(p152);   // r04
		// 112
		const float p154 = s_r37;
		const float r155 = w24(p154);   // r2b
		// 113
		const float p156 = s_r38;
		const float r157 = w24(p156);   // r31
		// 114
		const float p158 = k[84] * in[0];
		// 115
		const float p159 = sat((k[85] * r145 + p158) * 4.0f);
		const float r160 = w24(p159);   // r37
		// 116
		const float p161 = k[86] * in[1];
		// 117
		const float p162 = sat((k[87] * r145 + p161) * 4.0f);
		const float r163 = w24(p162);   // r38
		// 118
		const float p164 = k[88] * r147;
		// 119
		const float p165 = sat(k[89] * r151 + p164);
		// 11a
		const float p166 = sat(k[90] * r153 + p165);
		// 11b
		const float p167 = sat((k[91] * r70 + p166) * 4.0f);
		const float m168 = w24(p167);   // m28
		// 11c
		const float p169 = k[92] * r149;
		// 11d
		const float p170 = sat(k[93] * r153 + p169);
		// 11e
		const float p171 = sat(k[94] * r151 + p170);
		// 11f
		const float p172 = sat((k[95] * r101 + p171) * 4.0f);
		const float m173 = w24(p172);   // m29
		out[0] = m168;   // m28
		out[1] = m173;   // m29
		s_p = p172;
		s_m12 = m2;
		s_m13 = m40;
		s_r20 = r9;
		s_r21 = r16;
		s_r22 = r22;
		s_m14 = m74;
		s_m15 = m100;
		s_r2c = r52;
		s_r2b = r155;
		s_r2e = r64;
		s_r2d = r58;
		s_r37 = r160;
		s_r30 = r68;
		s_r2f = r70;
		s_r32 = r86;
		s_r31 = r157;
		s_r34 = r94;
		s_r33 = r90;
		s_r38 = r163;
		s_r36 = r98;
		s_r35 = r101;
		s_r24 = r126;
		s_r23 = r122;
		s_r26 = r132;
		s_r25 = r130;
		s_r28 = r139;
		s_r27 = r137;
		s_r29 = r145;
		s_r2a = r142;
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
	float s_r36 = 0.0f;
	float s_r37 = 0.0f;
	float s_r38 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_DT_ROTARY_H
