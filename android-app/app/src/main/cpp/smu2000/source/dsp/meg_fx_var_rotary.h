// license:BSD-3-Clause
// S-MU2000: バリエーション: ROTARY SP, TREMOLO, AUTO PAN, 2WAY ROTRY, AMBIENCE
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_ROTARY_H
#define S_MU2000_DSP_MEG_FX_VAR_ROTARY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_rotary : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x3c0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat(k[0] * s_r54);
		const float m2 = lfo[18];   // m34
		const int32_t i3 = idx_of(p1);
		// 121
		const float p4 = sat((k[1] * s_r57) * 16.0f);
		// 122
		const float p5 = k[2] * s_r58 + p4;
		const float t6 = tv_index(p1);   // t1
		// 123
		const float p7 = satpos(k[3] + p5);
		const float r8 = w24(p7);   // r08
		const float q9 = ram[at(3, i3)];
		// 124
		const float p10 = sat((k[4] * s_r59) * 16.0f);
		// 125
		const float p11 = k[5] * s_r5a + p10;
		const float m12 = q9;   // m01
		// 126
		const float p13 = satpos(k[6] + p11);
		const float r14 = w24(p13);   // r09
		const float q15 = ram[at(6, i3 + 1)];
		// 127
		const float p16 = sat(k[7] * r8);
		const int32_t i17 = idx_of(p16);
		// 128
		const float p18 = sat((k[8] + m2) * 2.0f);
		const float m19 = q15;   // m02
		const float r20 = w24(p18);   // r05
		// 129
		const float p21 = k[9] * s_r54;
		const float t22 = tv_index(p16);   // t3
		// 12a
		const float p23 = t6 * m12 - m12;
		// 12b
		const float p24 = sat(t6 * m19 - p23);
		const float r25 = w24(p24);   // r01
		const float t26 = tv_plain(p21);   // t1
		// 12c
		const float p27 = sat((k[12] * r20) * 2.0f);
		const float r28 = w24(p27);   // r05
		const float q29 = ram[at(12, i17)];
		// 12d
		const float p30 = sat(k[13] * r14);
		const int32_t i31 = idx_of(p30);
		// 12e
		const float p32 = sat(t26 * r25 + r25);
		const float m33 = q29;   // m05
		const float r34 = w24(p32);   // r01
		const float t35 = tv_plain(p27);   // t1
		// 12f
		const float p36 = sat((k[15] * s_r55) * 16.0f);
		const float t37 = tv_index(p30);   // t7
		const float q38 = ram[at(15, i17 + 1)];
		// 130
		const float p39 = k[16] * s_r56 + p36;
		const float m40 = lfo[19];   // m35
		// 131
		const float p41 = satpos(k[17] + p39);
		const float m42 = q38;   // m06
		const float r43 = w24(p41);   // r07
		// 132
		const float p44 = sat(k[18] * r8);
		const float q45 = ram[at(18, i31)];
		// 133
		const float p46 = t22 * m33 - m33;
		// 134
		const float p47 = sat(t22 * m42 - p46);
		const float m48 = q45;   // m07
		const float r49 = w24(p47);   // r03
		const float t50 = tv_plain(p44);   // t3
		// 135
		const float p51 = sat(k[21] * r43);
		const int32_t i52 = idx_of(p51);
		const float q53 = ram[at(21, i31 + 1)];
		// 136
		const float p54 = sat(t35 * r28);
		const float r55 = w24(p54);   // r06
		// 137
		const float p56 = k[23] * in[0];
		const float m57 = q53;   // m08
		const float t58 = tv_index(p51);   // t2
		// 138
		const float p59 = sat((k[24] * in[1] + p56) * 2.0f);
		const float r60 = w24(p59);   // r44
		const float q61 = ram[at(24, i52)];
		// 139
		const float p62 = k[25] * s_r44;
		const float r63 = s_r44;   // r45
		// 13a
		const float p64 = k[26] * s_r45 + p62;
		const float m65 = q61;   // m03
		// 13b
		const float p66 = k[27] * s_r46 + p64;
		const float q67 = ram[at(27, i52 + 1)];
		// 13c
		const float p68 = k[28] * s_r47 + p66;
		// 13d
		const float p69 = sat((k[29] * r60 + p68) * 2.0f);
		const float m70 = q67;   // m04
		const float r71 = w24(p69);   // r46
		// 13e
		const float p72 = k[30] * s_r46;
		const float r73 = s_r46;   // r47
		// 13f
		const float p74 = k[31] * s_r47 + p72;
		// 140
		const float p75 = k[32] * s_r48 + p74;
		const float m76 = lfo[20];   // m36
		// 141
		const float p77 = k[33] * s_r49 + p75;
		// 142
		const float p78 = sat((k[34] * r71 + p77) * 2.0f);
		const float r79 = w24(p78);   // r48
		// 143
		const float p80 = k[35] * s_r48;
		const float r81 = s_r48;   // r49
		// 144
		const float p82 = k[36] * s_r49 + p80;
		// 145
		const float p83 = k[37] * s_r4a + p82;
		const float r84 = s_r4a;   // r4b
		// 146
		const float p85 = k[38] * s_r4b + p83;
		// 147
		const float p86 = sat((k[39] * r79 + p85) * 16.0f);
		const float r87 = w24(p86);   // r4a
		// 148
		const float p88 = t35 * r55;
		const float r89 = w24(p88);   // r06
		// 149
		const float p90 = k[41] * in[1];
		// 14a
		const float p91 = sat((k[42] * in[0] + p90) * 2.0f);
		const float r92 = w24(p91);   // r4c
		// 14b
		const float p93 = k[43] * s_r4c;
		const float r94 = s_r4c;   // r4d
		// 14c
		const float p95 = k[44] * s_r4d + p93;
		// 14d
		const float p96 = k[45] * s_r4e + p95;
		// 14e
		const float p97 = k[46] * s_r4f + p96;
		// 14f
		const float p98 = sat((k[47] * r92 + p97) * 2.0f);
		const float r99 = w24(p98);   // r4e
		// 150
		const float p100 = k[48] * s_r4e;
		const float m101 = lfo[21];   // m37
		const float r102 = s_r4e;   // r4f
		// 151
		const float p103 = k[49] * s_r4f + p100;
		// 152
		const float p104 = k[50] * s_r50 + p103;
		// 153
		const float p105 = k[51] * s_r51 + p104;
		// 154
		const float p106 = sat((k[52] * r99 + p105) * 2.0f);
		const float r107 = w24(p106);   // r50
		// 155
		const float p108 = k[53] * s_r50;
		const float r109 = s_r50;   // r51
		// 156
		const float p110 = k[54] * s_r51 + p108;
		// 157
		const float p111 = k[55] * s_r52 + p110;
		const float r112 = s_r52;   // r53
		// 158
		const float p113 = k[56] * s_r53 + p111;
		// 159
		const float p114 = sat((k[57] * r107 + p113) * 16.0f);
		const float r115 = w24(p114);   // r52
		// 15a
		const float p116 = sat(t50 * r49 + r49);
		const float r117 = w24(p116);   // r03
		// 15b
		const float p118 = k[59] * r87;
		// 15c
		const float p119 = sat(k[60] * r115 + p118);
		const float w120 = p119;
		// 15d
		const float p121 = sat(k[61] * r14);
		// 15e
		const float p122 = t37 * m48 - m48;
		// 15f
		const float p123 = sat(t37 * m57 - p122);
		const float r124 = w24(p123);   // r04
		const float t125 = tv_plain(p121);   // t7
		ram[at(63)] = w120;
		// 160
		const float p126 = sat(k[64] * r115);
		const float w127 = p126;
		// 161
		const float p128 = k[65] * r43;
		// 162
		const float p129 = t58 * m65 - m65;
		ram[at(66)] = w127;
		// 163
		const float p130 = sat(t58 * m70 - p129);
		const float r131 = w24(p130);   // r02
		const float t132 = tv_plain(p128);   // t2
		// 164
		const float p133 = sat(t125 * r124 + r124);
		const float r134 = w24(p133);   // r04
		// 165
		const float p135 = k[69] * r34;
		// 166
		const float p136 = k[70] * r117 + p135;
		// 167
		const float p137 = sat(k[71] * r134 + p136);
		const float r138 = w24(p137);   // r01
		// 168
		const float p139 = sat(t132 * r131 + r131);
		const float r140 = w24(p139);   // r02
		// 169
		const float p141 = k[73] * r117;
		// 16a
		const float p142 = k[74] * r134 + p141;
		// 16b
		const float p143 = sat(k[75] * r140 + p142);
		const float r144 = w24(p143);   // r02
		// 16c
		const float p145 = sat((k[76] * r28) * 16.0f);
		// 16d
		const float p146 = k[77] * r89 + p145;
		// 16e
		const float p147 = satpos(k[78] + p146);
		const float r148 = w24(p147);   // r54
		// 16f
		const float p149 = sat((k[79] + m40) * 2.0f);
		const float r150 = w24(p149);   // r07
		// 170
		const float p151 = sat((k[80] + m76) * 2.0f);
		const float r152 = w24(p151);   // r08
		// 171
		const float p153 = sat((k[81] + m101) * 2.0f);
		const float r154 = w24(p153);   // r09
		// 172
		const float p155 = sat((k[82] * r150) * 2.0f);
		const float r156 = w24(p155);   // r55
		// 173
		const float p157 = sat((k[83] * r152) * 2.0f);
		const float r158 = w24(p157);   // r57
		// 174
		const float p159 = sat((k[84] * r154) * 2.0f);
		const float r160 = w24(p159);   // r59
		const float t161 = tv_plain(p155);   // t2
		// 175
		const float p162 = k[85] * r87;
		const float t163 = tv_plain(p157);   // t3
		// 176
		const float p164 = sat((k[86] * r138 + p162) * 4.0f);
		const float m165 = w24(p164);   // m2c
		const float t166 = tv_plain(p159);   // t7
		// 177
		const float p167 = k[87] * r115;
		// 178
		const float p168 = sat((k[88] * r144 + p167) * 4.0f);
		const float m169 = w24(p168);   // m2d
		// 179
		const float p170 = sat(t161 * r156);
		const float r171 = w24(p170);   // r07
		// 17a
		const float p172 = sat(t163 * r158);
		const float r173 = w24(p172);   // r08
		// 17b
		const float p174 = sat(t166 * r160);
		const float r175 = w24(p174);   // r09
		// 17c
		const float p176 = sat(t161 * r171);
		const float r177 = w24(p176);   // r56
		// 17d
		const float p178 = sat(t163 * r173);
		const float r179 = w24(p178);   // r58
		// 17e
		const float p180 = sat(t166 * r175);
		const float r181 = w24(p180);   // r5a
		// 17f
		out[0] = m165;   // m2c
		out[1] = m169;   // m2d
		s_p = p180;
		s_r54 = r148;
		s_r57 = r158;
		s_r58 = r179;
		s_r59 = r160;
		s_r5a = r181;
		s_r55 = r156;
		s_r56 = r177;
		s_r44 = r60;
		s_r45 = r63;
		s_r46 = r71;
		s_r47 = r73;
		s_r48 = r79;
		s_r49 = r81;
		s_r4a = r87;
		s_r4b = r84;
		s_r4c = r92;
		s_r4d = r94;
		s_r4e = r99;
		s_r4f = r102;
		s_r50 = r107;
		s_r51 = r109;
		s_r52 = r115;
		s_r53 = r112;
	}

private:
	float s_m00 = 0.0f;
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
	float s_r54 = 0.0f;
	float s_r55 = 0.0f;
	float s_r56 = 0.0f;
	float s_r57 = 0.0f;
	float s_r58 = 0.0f;
	float s_r59 = 0.0f;
	float s_r5a = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_ROTARY_H
