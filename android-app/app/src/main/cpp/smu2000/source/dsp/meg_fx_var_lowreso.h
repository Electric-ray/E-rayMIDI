// license:BSD-3-Clause
// S-MU2000: バリエーション: LOW RESO, D.SCRATCH
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_LOWRESO_H
#define S_MU2000_DSP_MEG_FX_VAR_LOWRESO_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_lowreso : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x3c0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; s_r5d = 0; s_r5e = 0; s_r5f = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * s_r54;
		const float m2 = lfo[18];   // m05
		const float r3 = s_r54;   // r44
		// 121
		const float p4 = k[1] * s_r44 + p1;
		const float r5 = s_r44;   // r45
		// 122
		const float p6 = k[2] * s_r45 + p4;
		// 123
		const float p7 = k[3] * s_r46 + p6;
		const float r8 = s_r46;   // r47
		// 124
		const float p9 = sat((k[4] * s_r47 + p7) * 2.0f);
		const float r10 = w24(p9);   // r46
		// 125
		const float p11 = k[5] * m2;
		const int32_t i12 = idx_of(p11);
		// 126
		// 127
		const float t13 = tv_index(p11);   // t1
		// 128
		// 129
		const float p14 = k[9] * s_r55;
		const float r15 = s_r55;   // r50
		const float q16 = ram[at(9, i12)];
		// 12a
		const float p17 = k[10] * s_r50 + p14;
		const float r18 = s_r50;   // r51
		// 12b
		const float p19 = k[11] * s_r51 + p17;
		const float m20 = q16;   // m18
		// 12c
		const float p21 = k[12] * s_r52 + p19;
		const float r22 = s_r52;   // r53
		const float q23 = ram[at(12, i12 + 1)];
		// 12d
		const float p24 = sat((k[13] * s_r53 + p21) * 2.0f);
		const float r25 = w24(p24);   // r52
		// 12e
		const float m26 = q23;   // m19
		// 12f
		const float q27 = ram[at(15)];
		// 130
		const float m28 = lfo[19];   // m06
		// 131
		const float p29 = t13 * m20 - m20;
		const float m30 = q27;   // m01
		// 132
		const float p31 = t13 * m26 - p29;
		const float r32 = w24(p31);   // r01
		const float q33 = ram[at(18)];
		// 133
		const float p34 = k[19] * m2;
		const int32_t i35 = idx_of(p34);
		// 134
		const float p36 = k[20] * s_m16;
		const float m37 = q33;   // m02
		// 135
		const float p38 = k[21] * r10 + p36;
		const float t39 = tv_index(p34);   // t1
		// 136
		const float p40 = sat((k[22] * m30 + p38) * 16.0f);
		const float r41 = w24(p40);   // r09
		// 137
		const float p42 = k[23];
		// 138
		const float p43 = k[24] * m28 + p42;
		const float q44 = ram[at(24, i35)];
		// 139
		const float p45 = sat(k[25] * s_r48 + p43);
		// 13a
		const float p46 = k[26];
		const float m47 = q44;   // m18
		// 13b
		const float p48 = k[27] * m28 + p46;
		const float t49 = tv_plain(p45);   // t2
		const float q50 = ram[at(27, i35 + 1)];
		// 13c
		const float p51 = sat(k[28] * s_r48 + p48);
		// 13d
		const float p52 = k[29] * r25;
		const float m53 = q50;   // m19
		// 13e
		const float p54 = sat((k[30] * m37 + p52) * 16.0f);
		const float r55 = w24(p54);   // r49
		const float t56 = tv_plain(p51);   // t3
		// 13f
		const float p57 = t39 * m47 - m47;
		// 140
		const float p58 = t39 * m53 - p57;
		const float m59 = lfo[20];   // m07
		const float r60 = w24(p58);   // r02
		// 141
		const float p61 = t49 * s_r4a;
		const float r62 = w24(p61);   // r07
		// 142
		const float p63 = t56 * s_r4b;
		const float r64 = w24(p63);   // r08
		// 143
		const float p65 = k[35];
		// 144
		const float p66 = k[36] * m59 + p65;
		// 145
		const float p67 = sat(k[37] * s_r48 + p66);
		// 146
		const float p68 = k[38];
		// 147
		const float p69 = k[39] * m59 + p68;
		const float t70 = tv_plain(p67);   // t2
		// 148
		const float p71 = sat(k[40] * s_r48 + p69);
		// 149
		const float p72 = k[41] * s_r4a;
		// 14a
		const float p73 = sat((k[42] * r32 + p72) * 2.0f);
		const float r74 = w24(p73);   // r05
		const float t75 = tv_plain(p71);   // t3
		// 14b
		const float p76 = k[43] * s_r4b;
		// 14c
		const float p77 = sat((k[44] * r60 + p76) * 2.0f);
		const float r78 = w24(p77);   // r06
		// 14d
		const float p79 = k[45];
		// 14e
		const float p80 = sat(k[46] * r41 + p79);
		const int32_t i81 = idx_of(p80);
		// 14f
		const float p82 = k[47] * r32;
		const float r83 = r32;   // r5d
		// 150
		const float p84 = k[48] * s_r5d + p82;
		const float m85 = lfo[21];   // m16
		const float t86 = tv_index(p80);   // t1
		// 151
		const float p87 = sat((k[49] * s_r5c + p84) * 2.0f);
		const float r88 = w24(p87);   // r5c
		// 152
		const float p89 = t70 * r74;
		const float r90 = w24(p89);   // r05
		// 153
		const float p91 = t75 * r78;
		const float r92 = w24(p91);   // r06
		const float q93 = tab(51, i81);
		// 154
		const float p94 = k[52] * r60;
		const float r95 = r60;   // r5f
		// 155
		const float p96 = k[53] * s_r5f + p94;
		const float m97 = q93;   // m18
		// 156
		const float p98 = sat((k[54] * s_r5e + p96) * 2.0f);
		const float r99 = w24(p98);   // r5e
		const float q100 = tab(54, i81 + 1);
		// 157
		const float p101 = k[55] * r62;
		// 158
		const float p102 = sat((k[56] * r88 + p101) * 2.0f);
		const float m103 = q100;   // m19
		const float w104 = p102;
		// 159
		const float q105 = tab(57, i81);
		// 15a
		const float p106 = k[58] * r64;
		// 15b
		const float p107 = sat((k[59] * r99 + p106) * 2.0f);
		const float m108 = q105;   // m03
		const float w109 = p107;
		// 15c
		const float p110 = t86 * m97 - m97;
		ram[at(60)] = w104;
		// 15d
		const float p111 = t86 * m103 - p110;
		const float r112 = w24(p111);   // r03
		// 15e
		const float p113 = k[62];
		// 15f
		const float p114 = sat(k[63] * r55 + p113);
		const int32_t i115 = idx_of(p114);
		ram[at(63)] = w109;
		// 160
		const float p116 = k[64] * in[0];
		// 161
		const float p117 = k[65] * r32 + p116;
		const float t118 = tv_index(p114);   // t1
		// 162
		const float p119 = sat((k[66] * r90 + p117) * 4.0f);
		const float r120 = w24(p119);   // r05
		const float q121 = tab(66, i115);
		// 163
		const float p122 = k[67] * in[1];
		// 164
		const float p123 = k[68] * r60 + p122;
		const float m124 = q121;   // m18
		// 165
		const float p125 = sat((k[69] * r92 + p123) * 4.0f);
		const float r126 = w24(p125);   // r06
		const float q127 = tab(69, i115 + 1);
		// 166
		const float p128 = k[70] * r112;
		// 167
		const float p129 = sat((k[71] * m108 + p128) * 2.0f);
		const float m130 = q127;   // m19
		const float r131 = w24(p129);   // r4c
		const float w132 = p129;
		// 168
		const float p133 = k[72] * s_r4c;
		const float r134 = s_r4c;   // r4d
		const float q135 = tab(72, i115);
		// 169
		const float p136 = k[73] * s_r4d + p133;
		// 16a
		const float p137 = k[74] * s_r4e + p136;
		const float m138 = q135;   // m04
		const float r139 = s_r4e;   // r4f
		// 16b
		const float p140 = k[75] * s_r4f + p137;
		ram[at(75)] = w132;
		// 16c
		const float p141 = sat((k[76] * r131 + p140) * 2.0f);
		const float r142 = w24(p141);   // r4e
		// 16d
		const float p143 = t118 * m124 - m124;
		// 16e
		const float p144 = t118 * m130 - p143;
		const float r145 = w24(p144);   // r04
		// 16f
		const float p146 = sat((k[79] * in[0]) * 2.0f);
		const float r147 = w24(p146);   // r54
		// 170
		const float p148 = sat(k[80] * r120);
		const float m149 = w24(p148);   // m2c
		// 171
		const float p150 = sat((k[81] * in[1]) * 2.0f);
		const float r151 = w24(p150);   // r55
		// 172
		const float p152 = k[82] * r145;
		// 173
		const float p153 = sat((k[83] * m138 + p152) * 2.0f);
		const float r154 = w24(p153);   // r58
		const float w155 = p153;
		// 174
		const float p156 = sat(k[84] * r126);
		const float m157 = w24(p156);   // m2d
		// 175
		const float p158 = k[85] * s_r58;
		const float r159 = s_r58;   // r59
		// 176
		const float p160 = k[86] * s_r59 + p158;
		// 177
		const float p161 = k[87] * s_r5a + p160;
		const float r162 = s_r5a;   // r5b
		ram[at(87)] = w155;
		// 178
		const float p163 = k[88] * s_r5b + p161;
		// 179
		const float p164 = sat((k[89] * r154 + p163) * 2.0f);
		const float r165 = w24(p164);   // r5a
		// 17a
		// 17b
		const float p166 = sat((k[91] * r142) * 2.0f);
		const float r167 = w24(p166);   // r4a
		// 17c
		// 17d
		const float p168 = sat((k[93] * r165) * 2.0f);
		const float r169 = w24(p168);   // r4b
		// 17e
		const float p170 = k[94];
		// 17f
		const float p171 = sat(k[95] * r142 + p170);
		const float r172 = w24(p171);   // r48
		out[0] = m149;   // m2c
		out[1] = m157;   // m2d
		s_p = p171;
		s_r54 = r147;
		s_r44 = r3;
		s_r45 = r5;
		s_r46 = r10;
		s_r47 = r8;
		s_r55 = r151;
		s_r50 = r15;
		s_r51 = r18;
		s_r52 = r25;
		s_r53 = r22;
		s_m16 = m85;
		s_r48 = r172;
		s_r4a = r167;
		s_r4b = r169;
		s_r5d = r83;
		s_r5c = r88;
		s_r5f = r95;
		s_r5e = r99;
		s_r4c = r131;
		s_r4d = r134;
		s_r4e = r142;
		s_r4f = r139;
		s_r58 = r154;
		s_r59 = r159;
		s_r5a = r165;
		s_r5b = r162;
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
	float s_r58 = 0.0f;
	float s_r59 = 0.0f;
	float s_r5a = 0.0f;
	float s_r5b = 0.0f;
	float s_r5c = 0.0f;
	float s_r5d = 0.0f;
	float s_r5e = 0.0f;
	float s_r5f = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_LOWRESO_H
