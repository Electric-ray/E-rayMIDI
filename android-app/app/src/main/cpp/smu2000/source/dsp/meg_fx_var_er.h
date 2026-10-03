// license:BSD-3-Clause
// S-MU2000: バリエーション: ER 1, ER 2, GATE REV, REVRS GATE, KARAOKE 1, KARAOKE 2, KARAOKE 3
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_ER_H
#define S_MU2000_DSP_MEG_FX_VAR_ER_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_er : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4f = 0; s_r50 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * in[0];
		const float q2 = ram[at(0)];
		// 121
		const float p3 = sat((k[1] * in[1] + p1) * 2.0f);
		const float r4 = w24(p3);   // r44
		// 122
		const float p5 = k[2] * s_r45;
		const float m6 = q2;   // m01
		// 123
		const float p7 = k[3] * s_r44 + p5;
		const float r8 = s_r44;   // r45
		const float q9 = ram[at(3)];
		// 124
		const float p10 = k[4] * r4 + p7;
		// 125
		const float p11 = k[5] * s_r46 + p10;
		const float m12 = q9;   // m02
		const float r13 = s_r46;   // r47
		// 126
		const float p14 = sat((k[6] * s_r47 + p11) * 2.0f);
		const float r15 = w24(p14);   // r46
		const float q16 = ram[at(6)];
		// 127
		const float p17 = k[7] * s_r47;
		// 128
		const float p18 = k[8] * s_r46 + p17;
		const float m19 = q16;   // m03
		// 129
		const float p20 = k[9] * r15 + p18;
		const float q21 = ram[at(9)];
		// 12a
		const float p22 = k[10] * s_r48 + p20;
		const float r23 = s_r48;   // r49
		// 12b
		const float p24 = sat((k[11] * s_r49 + p22) * 2.0f);
		const float m25 = q21;   // m04
		const float r26 = w24(p24);   // r48
		// 12c
		const float p27 = k[12] * s_r49;
		const float q28 = ram[at(12)];
		// 12d
		const float p29 = k[13] * s_r48 + p27;
		// 12e
		const float p30 = k[14] * r26 + p29;
		const float m31 = q28;   // m05
		// 12f
		const float p32 = k[15] * s_r4a + p30;
		const float r33 = s_r4a;   // r4b
		const float q34 = ram[at(15)];
		// 130
		const float p35 = sat((k[16] * s_r4b + p32) * 4.0f);
		const float r36 = w24(p35);   // r4a
		// 131
		const float m37 = q34;   // m06
		// 132
		const float q38 = ram[at(18)];
		// 133
		// 134
		const float m39 = q38;   // m07
		// 135
		const float p40 = k[21] * m6;
		const float q41 = ram[at(21)];
		// 136
		const float p42 = k[22] * m12 + p40;
		// 137
		const float p43 = k[23] * m19 + p42;
		const float m44 = q41;   // m08
		// 138
		const float p45 = k[24] * m25 + p43;
		const float q46 = ram[at(24)];
		// 139
		const float p47 = k[25] * m31 + p45;
		// 13a
		const float p48 = k[26] * m37 + p47;
		const float m49 = q46;   // m09
		// 13b
		const float p50 = k[27] * m39 + p48;
		const float q51 = ram[at(27)];
		// 13c
		const float p52 = k[28] * m44 + p50;
		// 13d
		const float p53 = sat(k[29] * m49 + p52);
		const float m54 = q51;   // m01
		const float r55 = w24(p53);   // r4d
		const float w56 = p53;
		// 13e
		const float q57 = ram[at(30)];
		// 13f
		const float p58 = k[31] * r36;
		// 140
		const float p59 = sat((k[32] * m54 + p58) * 2.0f);
		const float m60 = q57;   // m02
		const float r61 = w24(p59);   // r01
		// 141
		const float p62 = k[33] * r36;
		ram[at(33)] = w56;
		// 142
		const float p63 = sat(k[34] * m54 + p62);
		const float w64 = p63;
		// 143
		const float p65 = k[35] * r61;
		// 144
		const float p66 = sat((k[36] * m60 + p65) * 2.0f);
		const float r67 = w24(p66);   // r01
		const float q68 = ram[at(36)];
		// 145
		const float p69 = k[37] * r61;
		// 146
		const float p70 = sat(k[38] * m60 + p69);
		const float m71 = q68;   // m03
		const float w72 = p70;
		// 147
		ram[at(39)] = w64;
		// 148
		const float p73 = k[40] * s_m16;
		// 149
		const float p74 = k[41] * m71 + p73;
		const float m75 = m71;   // m16
		// 14a
		const float p76 = sat(k[42] * s_r4c + p74);
		const float r77 = w24(p76);   // r4c
		const float q78 = ram[at(42)];
		// 14b
		// 14c
		const float m79 = q78;   // m01
		// 14d
		const float q80 = ram[at(45)];
		// 14e
		// 14f
		const float m81 = q80;   // m02
		// 150
		const float q82 = ram[at(48)];
		// 151
		// 152
		const float m83 = q82;   // m03
		// 153
		const float q84 = ram[at(51)];
		// 154
		// 155
		const float m85 = q84;   // m04
		// 156
		const float q86 = ram[at(54)];
		// 157
		// 158
		const float m87 = q86;   // m05
		// 159
		const float q88 = ram[at(57)];
		// 15a
		// 15b
		const float m89 = q88;   // m06
		// 15c
		const float q90 = ram[at(60)];
		// 15d
		// 15e
		const float m91 = q90;   // m07
		// 15f
		const float p92 = k[63] * m79;
		const float q93 = ram[at(63)];
		// 160
		const float p94 = k[64] * m81 + p92;
		// 161
		const float p95 = k[65] * m83 + p94;
		const float m96 = q93;   // m08
		// 162
		const float p97 = k[66] * m85 + p95;
		const float q98 = ram[at(66)];
		// 163
		const float p99 = k[67] * m87 + p97;
		// 164
		const float p100 = k[68] * m89 + p99;
		const float m101 = q98;   // m09
		// 165
		const float p102 = k[69] * m91 + p100;
		ram[at(69)] = w72;
		// 166
		const float p103 = k[70] * m96 + p102;
		// 167
		const float p104 = sat(k[71] * m101 + p103);
		const float r105 = w24(p104);   // r4e
		const float w106 = p104;
		// 168
		const float p107 = k[72] * in[0];
		const float q108 = ram[at(72)];
		// 169
		const float p109 = sat((k[73] * s_r4f + p107) * 4.0f);
		const float m110 = w24(p109);   // m2c
		// 16a
		const float p111 = k[74] * in[1];
		const float m112 = q108;   // m01
		// 16b
		const float p113 = sat((k[75] * s_r50 + p111) * 4.0f);
		const float m114 = w24(p113);   // m2d
		const float q115 = ram[at(75)];
		// 16c
		const float p116 = k[76] * r55;
		// 16d
		const float p117 = sat(k[77] * m112 + p116);
		const float m118 = q115;   // m02
		const float r119 = w24(p117);   // r02
		// 16e
		ram[at(78)] = w106;
		// 16f
		const float p120 = k[79] * r105;
		// 170
		const float p121 = sat(k[80] * m118 + p120);
		const float r122 = w24(p121);   // r03
		// 171
		const float q123 = ram[at(81)];
		// 172
		// 173
		const float m124 = q123;   // m03
		// 174
		const float p125 = k[84] * r67;
		const float q126 = ram[at(84)];
		// 175
		const float p127 = sat(k[85] * r77 + p125);
		const float w128 = p127;
		// 176
		const float p129 = k[86] * m124;
		const float m130 = q126;   // m04
		// 177
		const float p131 = sat(k[87] * r119 + p129);
		const float w132 = p131;
		ram[at(87)] = w128;
		// 178
		const float p133 = k[88] * m124;
		// 179
		const float p134 = sat((k[89] * r119 + p133) * 2.0f);
		const float r135 = w24(p134);   // r4f
		// 17a
		const float p136 = k[90] * m130;
		ram[at(90)] = w132;
		// 17b
		const float p137 = sat(k[91] * r122 + p136);
		const float w138 = p137;
		// 17c
		const float p139 = k[92] * m130;
		// 17d
		const float p140 = sat((k[93] * r122 + p139) * 2.0f);
		const float r141 = w24(p140);   // r50
		ram[at(93)] = w138;
		// 17e
		// 17f
		out[0] = m110;   // m2c
		out[1] = m114;   // m2d
		s_p = p140;
		s_r45 = r8;
		s_r44 = r4;
		s_r46 = r15;
		s_r47 = r13;
		s_r48 = r26;
		s_r49 = r23;
		s_r4a = r36;
		s_r4b = r33;
		s_m16 = m75;
		s_r4c = r77;
		s_r4f = r135;
		s_r50 = r141;
	}

private:
	float s_m00 = 0.0f;
	float s_m16 = 0.0f;
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
	float s_r4f = 0.0f;
	float s_r50 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_ER_H
