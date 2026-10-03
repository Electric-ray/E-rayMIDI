// license:BSD-3-Clause
// S-MU2000: バリエーション: CHORUS 3
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_CHORUS3_H
#define S_MU2000_DSP_MEG_FX_VAR_CHORUS3_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_chorus3 : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xfc0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat(k[0] * s_m16);
		const float m2 = lfo[18];   // m34
		const int32_t i3 = idx_of(p1);
		// 121
		const float p4 = satpos(k[1] * s_r58 + p1);
		// 122
		const float p5 = sat((k[2] * s_r5b) * 16.0f);
		const float t6 = tv_index(p1);   // t1
		// 123
		const float p7 = k[3] * s_r5c + p5;
		// 124
		const float p8 = satpos(k[4] + p7);
		const float r9 = w24(p8);   // r04
		// 125
		const float p10 = sat((k[5] * s_r59) * 16.0f);
		// 126
		const float p11 = k[6] * s_r5a + p10;
		const float q12 = ram[at(6, i3)];
		// 127
		const float p13 = satpos(k[7] + p11);
		const float r14 = w24(p13);   // r03
		// 128
		const float p15 = sat(k[8] * s_m18);
		const float m16 = q12;   // m01
		const int32_t i17 = idx_of(p15);
		// 129
		const float p18 = sat((k[9] * in[0]) * 2.0f);
		const float r19 = w24(p18);   // r44
		const float q20 = ram[at(9, i3 + 1)];
		// 12a
		const float p21 = k[10] * s_r44;
		const float r22 = s_r44;   // r45
		const float t23 = tv_index(p15);   // t3
		// 12b
		const float p24 = k[11] * s_r45 + p21;
		const float m25 = q20;   // m02
		// 12c
		const float p26 = k[12] * s_r46 + p24;
		const float q27 = ram[at(12, i17)];
		// 12d
		const float p28 = k[13] * s_r47 + p26;
		// 12e
		const float p29 = sat((k[14] * r19 + p28) * 2.0f);
		const float m30 = q27;   // m05
		const float r31 = w24(p29);   // r46
		// 12f
		const float p32 = sat(k[15] * s_m19);
		const int32_t i33 = idx_of(p32);
		const float q34 = ram[at(15, i17 + 1)];
		// 130
		const float p35 = k[16] * s_r46;
		const float m36 = lfo[19];   // m35
		const float r37 = s_r46;   // r47
		// 131
		const float p38 = k[17] * s_r47 + p35;
		const float m39 = q34;   // m06
		const float t40 = tv_index(p32);   // t7
		// 132
		const float p41 = k[18] * s_r48 + p38;
		const float q42 = ram[at(18, i33)];
		// 133
		const float p43 = k[19] * s_r49 + p41;
		// 134
		const float p44 = sat((k[20] * r31 + p43) * 2.0f);
		const float m45 = q42;   // m07
		const float r46 = w24(p44);   // r48
		// 135
		const float p47 = k[21] * s_r48;
		const float r48 = s_r48;   // r49
		const float q49 = ram[at(21, i33 + 1)];
		// 136
		const float p50 = k[22] * s_r49 + p47;
		// 137
		const float p51 = k[23] * s_r4a + p50;
		const float m52 = q49;   // m08
		const float r53 = s_r4a;   // r4b
		// 138
		const float p54 = k[24] * s_r4b + p51;
		// 139
		const float p55 = sat((k[25] * r46 + p54) * 16.0f);
		const float r56 = w24(p55);   // r4a
		// 13a
		const float p57 = sat(k[26] * s_m17);
		const int32_t i58 = idx_of(p57);
		// 13b
		const float p59 = satpos(k[27] * r14 + p57);
		// 13c
		const float p60 = sat((k[28] + m2) * 2.0f);
		const float r61 = w24(p60);   // r01
		const float t62 = tv_index(p57);   // t2
		// 13d
		const float p63 = sat((k[29] * in[1]) * 2.0f);
		const float r64 = w24(p63);   // r4c
		// 13e
		const float p65 = k[30] * s_r4c;
		const float r66 = s_r4c;   // r4d
		const float q67 = ram[at(30, i58)];
		// 13f
		const float p68 = k[31] * s_r4d + p65;
		// 140
		const float p69 = k[32] * s_r4e + p68;
		const float m70 = q67;   // m03
		// 141
		const float p71 = k[33] * s_r4f + p69;
		const float m72 = lfo[20];   // m19
		const float q73 = ram[at(33, i58 + 1)];
		// 142
		const float p74 = sat((k[34] * r64 + p71) * 2.0f);
		const float r75 = w24(p74);   // r4e
		// 143
		const float p76 = k[35] * s_r4e;
		const float m77 = q73;   // m04
		const float r78 = s_r4e;   // r4f
		// 144
		const float p79 = k[36] * s_r4f + p76;
		// 145
		const float p80 = k[37] * s_r50 + p79;
		// 146
		const float p81 = k[38] * s_r51 + p80;
		// 147
		const float p82 = sat((k[39] * r75 + p81) * 2.0f);
		const float r83 = w24(p82);   // r50
		// 148
		const float p84 = k[40] * s_r50;
		const float r85 = s_r50;   // r51
		// 149
		const float p86 = k[41] * s_r51 + p84;
		// 14a
		const float p87 = k[42] * s_r52 + p86;
		const float r88 = s_r52;   // r53
		// 14b
		const float p89 = k[43] * s_r53 + p87;
		// 14c
		const float p90 = sat((k[44] * r83 + p89) * 16.0f);
		const float r91 = w24(p90);   // r52
		// 14d
		const float p92 = t6 * m16 - m16;
		// 14e
		const float p93 = sat(t6 * m25 - p92);
		const float r94 = w24(p93);   // r54
		// 14f
		const float p95 = t23 * m30 - m30;
		// 150
		const float p96 = sat(t23 * m39 - p95);
		const float m97 = lfo[21];   // m16
		const float r98 = w24(p96);   // r05
		// 151
		// 152
		const float p99 = t62 * m70 - m70;
		// 153
		const float p100 = sat(t62 * m77 - p99);
		const float r101 = w24(p100);   // r56
		// 154
		const float p102 = t40 * m45 - m45;
		// 155
		const float p103 = sat(t40 * m52 - p102);
		const float r104 = w24(p103);   // r06
		// 156
		// 157
		const float p105 = k[55] * r56;
		// 158
		const float p106 = k[56] * r91 + p105;
		// 159
		const float p107 = k[57] * r94 + p106;
		// 15a
		const float p108 = sat(k[58] * r98 + p107);
		const float w109 = p108;
		// 15b
		const float p110 = k[59] * r91;
		// 15c
		const float p111 = k[60] * r104 + p110;
		ram[at(60)] = w109;
		// 15d
		const float p112 = sat(k[61] * r101 + p111);
		const float w113 = p112;
		// 15e
		// 15f
		ram[at(63)] = w113;
		// 160
		const float m114 = lfo[22];   // m17
		// 161
		// 162
		const float p115 = sat((k[66] + m36) * 2.0f);
		const float r116 = w24(p115);   // r03
		// 163
		const float p117 = sat((k[67] + m72) * 2.0f);
		const float r118 = w24(p117);   // r04
		// 164
		const float p119 = sat((k[68] * r61) * 2.0f);
		const float r120 = w24(p119);   // r01
		// 165
		const float p121 = sat((k[69] * r116) * 2.0f);
		const float r122 = w24(p121);   // r59
		// 166
		const float p123 = sat((k[70] * r118) * 2.0f);
		const float r124 = w24(p123);   // r5b
		const float t125 = tv_plain(p119);   // t1
		// 167
		const float p126 = k[71] * r94;
		const float t127 = tv_plain(p121);   // t2
		// 168
		const float p128 = k[72] * r98 + p126;
		const float t129 = tv_plain(p123);   // t3
		// 169
		const float p130 = sat(k[73] * r104 + p128);
		const float r131 = w24(p130);   // r07
		// 16a
		const float p132 = sat(t125 * r120);
		const float r133 = w24(p132);   // r02
		// 16b
		const float p134 = sat(t127 * r122);
		const float r135 = w24(p134);   // r03
		// 16c
		const float p136 = sat(t129 * r124);
		const float r137 = w24(p136);   // r04
		// 16d
		const float p138 = sat(t125 * r133);
		const float r139 = w24(p138);   // r02
		// 16e
		const float p140 = sat(t127 * r135);
		const float r141 = w24(p140);   // r5a
		// 16f
		const float p142 = sat(t129 * r137);
		const float r143 = w24(p142);   // r5c
		// 170
		const float p144 = sat((k[80] * r120) * 16.0f);
		const float m145 = lfo[23];   // m18
		// 171
		const float p146 = k[81] * r139 + p144;
		// 172
		const float p147 = satpos(k[82] + p146);
		const float r148 = w24(p147);   // r58
		// 173
		const float p149 = k[83] * r101;
		// 174
		const float p150 = k[84] * r104 + p149;
		// 175
		const float p151 = sat(k[85] * r98 + p150);
		const float r152 = w24(p151);   // r08
		// 176
		const float p153 = k[86] * r56;
		// 177
		const float p154 = k[87] * r91 + p153;
		// 178
		const float p155 = sat((k[88] * r131 + p154) * 4.0f);
		const float m156 = w24(p155);   // m2c
		// 179
		const float p157 = k[89] * r91;
		// 17a
		const float p158 = k[90] * r56 + p157;
		// 17b
		const float p159 = sat((k[91] * r152 + p158) * 4.0f);
		const float m160 = w24(p159);   // m2d
		// 17c
		// 17d
		// 17e
		// 17f
		out[0] = m156;   // m2c
		out[1] = m160;   // m2d
		s_p = p159;
		s_m16 = m97;
		s_r58 = r148;
		s_r5b = r124;
		s_r5c = r143;
		s_r59 = r122;
		s_r5a = r141;
		s_m18 = m145;
		s_r44 = r19;
		s_r45 = r22;
		s_r46 = r31;
		s_r47 = r37;
		s_m19 = m72;
		s_r48 = r46;
		s_r49 = r48;
		s_r4a = r56;
		s_r4b = r53;
		s_m17 = m114;
		s_r4c = r64;
		s_r4d = r66;
		s_r4e = r75;
		s_r4f = r78;
		s_r50 = r83;
		s_r51 = r85;
		s_r52 = r91;
		s_r53 = r88;
	}

private:
	float s_m00 = 0.0f;
	float s_m16 = 0.0f;
	float s_m17 = 0.0f;
	float s_m18 = 0.0f;
	float s_m19 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r44 = 0.0f;
	float s_r45 = 0.0f;
	float s_r46 = 0.0f;
	float s_r47 = 0.0f;
	float s_r48 = 0.0f;
	float s_r49 = 0.0f;
	float s_r4a = 0.0f;
	float s_r4b = 0.0f;
	float s_r4c = 0.0f;
	float s_r4d = 0.0f;
	float s_r4e = 0.0f;
	float s_r4f = 0.0f;
	float s_r50 = 0.0f;
	float s_r51 = 0.0f;
	float s_r52 = 0.0f;
	float s_r53 = 0.0f;
	float s_r58 = 0.0f;
	float s_r59 = 0.0f;
	float s_r5a = 0.0f;
	float s_r5b = 0.0f;
	float s_r5c = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_CHORUS3_H
