// license:BSD-3-Clause
// S-MU2000: バリエーション: HALL 1, HALL 2, HALL M, HALL L, ROOM 1, ROOM 2, ROOM 3, ROOM S, ROOM M, ROOM L, STAGE 1, STAGE 2, PLATE, GM PLATE, WHITE ROOM, TUNNEL, CANYON, BASEMENT
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_REVERB_H
#define S_MU2000_DSP_MEG_FX_VAR_REVERB_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_reverb : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; }

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
		const float q8 = ram[at(3)];
		// 124
		const float p9 = sat(k[4] * r4 + p7);
		const float r10 = w24(p9);   // r45
		// 125
		const float p11 = k[5] * s_r46;
		const float m12 = q8;   // m02
		// 126
		const float p13 = k[6] * s_r45 + p11;
		const float q14 = ram[at(6)];
		// 127
		const float p15 = sat((k[7] * r10 + p13) * 2.0f);
		const float r16 = w24(p15);   // r46
		// 128
		const float p17 = sat((k[8] * m6) * 2.0f);
		const float m18 = q14;   // m03
		const float r19 = w24(p17);   // r47
		// 129
		const float p20 = k[9] * s_r48;
		const float q21 = ram[at(9)];
		// 12a
		const float p22 = k[10] * s_r47 + p20;
		// 12b
		const float p23 = sat((k[11] * r19 + p22) * 4.0f);
		const float m24 = q21;   // m04
		const float r25 = w24(p23);   // r48
		// 12c
		const float q26 = ram[at(12)];
		// 12d
		const float p27 = k[13] * m12;
		// 12e
		const float p28 = sat((k[14] * r25 + p27) * 2.0f);
		const float m29 = q26;   // m05
		const float r30 = w24(p28);   // r01
		// 12f
		const float p31 = k[15] * m12;
		const float q32 = ram[at(15)];
		// 130
		const float p33 = sat(k[16] * r25 + p31);
		const float w34 = p33;
		// 131
		const float p35 = k[17] * m18;
		const float m36 = q32;   // m06
		// 132
		const float p37 = sat(k[18] * r30 + p35);
		const float w38 = p37;
		ram[at(18)] = w34;
		// 133
		const float p39 = k[19] * m18;
		// 134
		const float p40 = sat((k[20] * r30 + p39) * 2.0f);
		const float m41 = w24(p40);   // m09
		// 135
		const float p42 = k[21] * m24;
		const float m43 = m24;   // m16
		ram[at(21)] = w38;
		// 136
		const float p44 = k[22] * s_m16 + p42;
		// 137
		const float p45 = sat((k[23] * s_r49 + p44) * 2.0f);
		const float r46 = w24(p45);   // r49
		// 138
		const float p47 = k[24] * m29;
		const float m48 = m29;   // m17
		const float q49 = ram[at(24)];
		// 139
		const float p50 = k[25] * s_m17 + p47;
		// 13a
		const float p51 = sat((k[26] * s_r4a + p50) * 2.0f);
		const float m52 = q49;   // m07
		const float r53 = w24(p51);   // r4a
		// 13b
		const float p54 = sat(k[27] * m41 + r46);
		const float w55 = p54;
		const float q56 = ram[at(27)];
		// 13c
		const float p57 = k[28] * m36;
		const float m58 = m36;   // m18
		// 13d
		const float p59 = k[29] * s_m18 + p57;
		const float m60 = q56;   // m02
		// 13e
		const float p61 = sat((k[30] * s_r4b + p59) * 2.0f);
		const float r62 = w24(p61);   // r4b
		ram[at(30)] = w55;
		// 13f
		const float p63 = sat(k[31] * m41 + r53);
		const float w64 = p63;
		// 140
		const float p65 = k[32] * m52;
		const float m66 = m52;   // m19
		// 141
		const float p67 = k[33] * s_m19 + p65;
		ram[at(33)] = w64;
		// 142
		const float p68 = sat((k[34] * s_r4c + p67) * 2.0f);
		const float r69 = w24(p68);   // r4c
		// 143
		const float p70 = sat(k[35] * m41 + r62);
		const float w71 = p70;
		// 144
		const float q72 = ram[at(36)];
		// 145
		// 146
		const float m73 = q72;   // m03
		// 147
		ram[at(39)] = w71;
		// 148
		// 149
		// 14a
		const float q74 = ram[at(42)];
		// 14b
		// 14c
		const float m75 = q74;   // m04
		// 14d
		const float q76 = ram[at(45)];
		// 14e
		// 14f
		const float m77 = q76;   // m05
		// 150
		const float q78 = ram[at(48)];
		// 151
		const float p79 = k[49] * m60;
		// 152
		const float p80 = k[50] * m73 + p79;
		const float m81 = q78;   // m06
		// 153
		const float p82 = k[51] * m75 + p80;
		const float q83 = ram[at(51)];
		// 154
		const float p84 = k[52] * m77 + p82;
		// 155
		const float p85 = sat(k[53] * m81 + p84);
		const float m86 = q83;   // m02
		const float r87 = w24(p85);   // r02
		// 156
		const float q88 = ram[at(54)];
		// 157
		// 158
		const float p89 = k[56] * r87;
		const float m90 = q88;   // m03
		// 159
		const float p91 = sat((k[57] * m86 + p89) * 2.0f);
		const float r92 = w24(p91);   // r02
		const float q93 = ram[at(57)];
		// 15a
		const float p94 = k[58] * r87;
		// 15b
		const float p95 = sat(k[59] * m86 + p94);
		const float m96 = q93;   // m04
		const float w97 = p95;
		// 15c
		const float p98 = k[60] * r92;
		const float q99 = ram[at(60)];
		// 15d
		const float p100 = sat((k[61] * m90 + p98) * 2.0f);
		const float r101 = w24(p100);   // r02
		// 15e
		const float p102 = k[62] * r92;
		const float m103 = q99;   // m05
		// 15f
		const float p104 = sat(k[63] * m90 + p102);
		const float w105 = p104;
		ram[at(63)] = w97;
		// 160
		// 161
		// 162
		ram[at(66)] = w105;
		// 163
		// 164
		// 165
		const float q106 = ram[at(69)];
		// 166
		// 167
		const float m107 = q106;   // m06
		// 168
		const float q108 = ram[at(72)];
		// 169
		// 16a
		const float m109 = q108;   // m07
		// 16b
		const float q110 = ram[at(75)];
		// 16c
		const float p111 = k[76] * m96;
		// 16d
		const float p112 = k[77] * m103 + p111;
		const float m113 = q110;   // m08
		// 16e
		const float p114 = k[78] * m107 + p112;
		const float q115 = ram[at(78)];
		// 16f
		const float p116 = k[79] * m109 + p114;
		// 170
		const float p117 = sat(k[80] * m113 + p116);
		const float m118 = q115;   // m02
		const float r119 = w24(p117);   // r03
		// 171
		const float q120 = ram[at(81)];
		// 172
		const float p121 = sat(k[82] * m41 + r69);
		const float w122 = p121;
		// 173
		const float p123 = sat(k[83] * m6 + r16);
		const float m124 = q120;   // m03
		const float w125 = p123;
		// 174
		const float p126 = k[84] * r119;
		ram[at(84)] = w122;
		// 175
		const float p127 = sat((k[85] * m118 + p126) * 2.0f);
		const float r128 = w24(p127);   // r03
		// 176
		const float p129 = k[86] * r119;
		// 177
		const float p130 = sat(k[87] * m118 + p129);
		const float w131 = p130;
		ram[at(87)] = w125;
		// 178
		const float p132 = k[88] * r128;
		// 179
		const float p133 = sat((k[89] * m124 + p132) * 2.0f);
		const float r134 = w24(p133);   // r03
		// 17a
		const float p135 = k[90] * r128;
		ram[at(90)] = w131;
		// 17b
		const float p136 = sat(k[91] * m124 + p135);
		const float w137 = p136;
		// 17c
		const float p138 = k[92] * in[0];
		// 17d
		const float p139 = sat((k[93] * r101 + p138) * 4.0f);
		const float m140 = w24(p139);   // m2c
		ram[at(93)] = w137;
		// 17e
		const float p141 = k[94] * in[1];
		// 17f
		const float p142 = sat((k[95] * r134 + p141) * 4.0f);
		const float m143 = w24(p142);   // m2d
		out[0] = m140;   // m2c
		out[1] = m143;   // m2d
		s_p = p142;
		s_r45 = r10;
		s_r44 = r4;
		s_r46 = r16;
		s_r48 = r25;
		s_r47 = r19;
		s_m16 = m43;
		s_r49 = r46;
		s_m17 = m48;
		s_r4a = r53;
		s_m18 = m58;
		s_r4b = r62;
		s_m19 = m66;
		s_r4c = r69;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_REVERB_H
