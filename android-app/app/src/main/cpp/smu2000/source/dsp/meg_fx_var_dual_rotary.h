// license:BSD-3-Clause
// S-MU2000: バリエーション: DUAL ROTR1, DUAL ROTR2
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2d m2c / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_DUAL_ROTARY_H
#define S_MU2000_DSP_MEG_FX_VAR_DUAL_ROTARY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_dual_rotary : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xfc0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2d, 0x2c };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; s_r5d = 0; s_r5e = 0; s_r5f = 0; s_r60 = 0; s_r61 = 0; s_r62 = 0; s_r63 = 0; }

	// in: 入口（m2d m2c）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * s_m16;
		const float m2 = lfo[18];   // m34
		// 121
		const float p3 = k[1] + p1;
		const float r4 = w24(p3);   // r09
		// 122
		const float p5 = sat(k[2] * s_r5c);
		const int32_t i6 = idx_of(p5);
		// 123
		const float p7 = sat(k[3] * s_m16);
		// 124
		const float p8 = sat(k[4] * s_r5d);
		const int32_t j9 = idx_of(p8);
		const float t10 = tv_index(p5);   // t1
		// 125
		const float p11 = k[5] * s_m16;
		const float r12 = w24(p11);   // r5c
		const float t13 = tv_plain(p7);   // t3
		// 126
		const float p14 = k[6] * r4 + p11;
		const float r15 = w24(p14);   // r5d
		const float t16 = tv_index(p8);   // t2
		const float q17 = ram[at(6, i6)];
		// 127
		const float p18 = k[7] * s_r54;
		// 128
		const float p19 = k[8] * s_r55 + p18;
		const float m20 = q17;   // m01
		// 129
		const float p21 = k[9] * s_r56 + p19;
		const float q22 = ram[at(9, i6 + 1)];
		// 12a
		const float p23 = k[10] * s_r58 + p21;
		// 12b
		const float p24 = k[11] * s_r59 + p23;
		const float m25 = q22;   // m02
		// 12c
		const float p26 = sat(k[12] * s_r5a + p24);
		const float r27 = w24(p26);   // r52
		const float q28 = ram[at(12, j9)];
		// 12d
		const float p29 = t10 * m20 - m20;
		// 12e
		const float p30 = sat(t10 * m25 - p29);
		const float m31 = q28;   // m03
		const float r32 = w24(p30);   // r01
		// 12f
		const float p33 = sat(k[15] * s_r5e);
		const int32_t i34 = idx_of(p33);
		const float q35 = ram[at(15, j9 + 1)];
		// 130
		const float p36 = sat(k[16] * s_r5f);
		const float m37 = lfo[19];   // m35
		const int32_t j38 = idx_of(p36);
		// 131
		const float p39 = k[17] * s_m17;
		const float m40 = q35;   // m04
		const float r41 = w24(p39);   // r5e
		const float t42 = tv_index(p33);   // t1
		// 132
		const float p43 = k[18] * s_m18 + p39;
		const float r44 = w24(p43);   // r5f
		const float t45 = tv_index(p36);   // t7
		const float q46 = ram[at(18, i34)];
		// 133
		const float p47 = k[19] * s_r54;
		// 134
		const float p48 = k[20] * s_r55 + p47;
		const float m49 = q46;   // m05
		// 135
		const float p50 = k[21] * s_r57 + p48;
		const float q51 = ram[at(21, i34 + 1)];
		// 136
		const float p52 = k[22] * s_r58 + p50;
		// 137
		const float p53 = k[23] * s_r59 + p52;
		const float m54 = q51;   // m06
		// 138
		const float p55 = sat(k[24] * s_r5b + p53);
		const float r56 = w24(p55);   // r53
		const float q57 = ram[at(24, j38)];
		// 139
		const float p58 = sat(k[25] * r4);
		// 13a
		const float p59 = t16 * m31 - m31;
		const float m60 = q57;   // m07
		// 13b
		const float p61 = sat(t16 * m40 - p59);
		const float r62 = w24(p61);   // r02
		const float t63 = tv_plain(p58);   // t2
		const float q64 = ram[at(27, j38 + 1)];
		// 13c
		const float p65 = k[28] * m2;
		// 13d
		const float p66 = k[29] + p65;
		const float m67 = q64;   // m08
		const float r68 = w24(p66);   // r09
		// 13e
		const float p69 = sat(k[30] * s_r60);
		const int32_t i70 = idx_of(p69);
		// 13f
		const float p71 = t42 * m49 - m49;
		// 140
		const float p72 = sat(t42 * m54 - p71);
		const float m73 = lfo[20];   // m36
		const float r74 = w24(p72);   // r03
		const float t75 = tv_index(p69);   // t1
		// 141
		const float p76 = t13 * r32 + r32;
		const float r77 = w24(p76);   // r54
		const float q78 = ram[at(33, i70)];
		// 142
		const float p79 = t45 * m60 - m60;
		// 143
		const float p80 = sat(t45 * m67 - p79);
		const float m81 = q78;   // m01
		const float r82 = w24(p80);   // r04
		// 144
		const float p83 = sat(k[36] * s_r61);
		const int32_t j84 = idx_of(p83);
		const float q85 = ram[at(36, i70 + 1)];
		// 145
		const float p86 = t63 * r62 + r62;
		const float r87 = w24(p86);   // r55
		// 146
		const float p88 = k[38] * in[0];
		const float m89 = q85;   // m02
		const float t90 = tv_index(p83);   // t2
		// 147
		const float p91 = sat((k[39] * in[1] + p88) * 2.0f);
		const float r92 = w24(p91);   // r44
		const float q93 = ram[at(39, j84)];
		// 148
		const float p94 = k[40] * s_r44;
		const float r95 = s_r44;   // r08
		// 149
		const float p96 = k[41] * s_r45 + p94;
		const float m97 = q93;   // m03
		// 14a
		const float p98 = k[42] * s_r46 + p96;
		const float q99 = ram[at(42, j84 + 1)];
		// 14b
		const float p100 = k[43] * s_r47 + p98;
		// 14c
		const float p101 = sat((k[44] * r92 + p100) * 2.0f);
		const float m102 = q99;   // m04
		const float r103 = w24(p101);   // r46
		// 14d
		const float p104 = k[45] * s_r46;
		const float r105 = s_r46;   // r47
		// 14e
		const float p106 = k[46] * s_r47 + p104;
		// 14f
		const float p107 = k[47] * s_r48 + p106;
		// 150
		const float p108 = k[48] * s_r49 + p107;
		const float m109 = lfo[21];   // m16
		// 151
		const float p110 = sat((k[49] * r103 + p108) * 2.0f);
		const float r111 = w24(p110);   // r48
		// 152
		const float p112 = k[50] * s_r48;
		const float r113 = s_r48;   // r49
		// 153
		const float p114 = k[51] * s_r4a + p112;
		// 154
		const float p115 = sat((k[52] * r111 + p114) * 16.0f);
		const float r116 = w24(p115);   // r4a
		// 155
		const float p117 = k[53] * m2;
		const float r118 = w24(p117);   // r60
		// 156
		const float p119 = k[54] * r68;
		const float r120 = w24(p119);   // r61
		// 157
		const float p121 = t75 * m81 - m81;
		// 158
		const float p122 = sat(t75 * m89 - p121);
		const float r123 = w24(p122);   // r05
		// 159
		const float p124 = t90 * m97 - m97;
		// 15a
		const float p125 = sat(t90 * m102 - p124);
		const float r126 = w24(p125);   // r06
		// 15b
		const float p127 = sat((k[59] * r116) * 2.0f);
		const float w128 = p127;
		// 15c
		const float p129 = sat(k[60] * s_r62);
		const int32_t i130 = idx_of(p129);
		// 15d
		const float p131 = sat(k[61] * s_r63);
		const int32_t j132 = idx_of(p131);
		// 15e
		const float p133 = k[62] * r95;
		const float r134 = r95;   // r45
		const float t135 = tv_index(p129);   // t1
		// 15f
		const float p136 = k[63] * s_r45 + p133;
		const float t137 = tv_index(p131);   // t7
		ram[at(63)] = w128;
		// 160
		const float p138 = k[64] * s_r4c + p136;
		const float m139 = lfo[22];   // m17
		// 161
		const float p140 = k[65] * s_r4d + p138;
		// 162
		const float p141 = sat((k[66] * r92 + p140) * 2.0f);
		const float r142 = w24(p141);   // r4c
		const float q143 = ram[at(66, i130)];
		// 163
		const float p144 = k[67] * s_r4c;
		const float r145 = s_r4c;   // r4d
		// 164
		const float p146 = k[68] * s_r4d + p144;
		const float m147 = q143;   // m05
		// 165
		const float p148 = k[69] * s_r4e + p146;
		const float q149 = ram[at(69, i130 + 1)];
		// 166
		const float p150 = k[70] * s_r4f + p148;
		// 167
		const float p151 = sat((k[71] * r142 + p150) * 2.0f);
		const float m152 = q149;   // m06
		const float r153 = w24(p151);   // r4e
		// 168
		const float p154 = k[72] * s_r4e;
		const float r155 = s_r4e;   // r4f
		const float q156 = ram[at(72, j132)];
		// 169
		const float p157 = k[73] * s_r50 + p154;
		// 16a
		const float p158 = sat((k[74] * r153 + p157) * 16.0f);
		const float m159 = q156;   // m07
		const float r160 = w24(p158);   // r50
		// 16b
		const float p161 = k[75] * m37;
		const float r162 = w24(p161);   // r62
		const float q163 = ram[at(75, j132 + 1)];
		// 16c
		const float p164 = k[76] * m73;
		const float r165 = w24(p164);   // r63
		// 16d
		const float p166 = t135 * m147 - m147;
		const float m167 = q163;   // m08
		// 16e
		const float p168 = sat(t135 * m152 - p166);
		const float r169 = w24(p168);   // r07
		// 16f
		const float p170 = sat((k[79] * r160) * 2.0f);
		const float w171 = p170;
		// 170
		const float p172 = t137 * m159 - m159;
		const float m173 = lfo[23];   // m18
		// 171
		const float p174 = sat(t137 * m167 - p172);
		const float r175 = w24(p174);   // r08
		ram[at(81)] = w171;
		// 172
		const float p176 = sat(k[82] * m139);
		// 173
		const float p177 = sat(k[83] * m173);
		// 174
		const float p178 = sat(k[84] * m2);
		const float t179 = tv_plain(p176);   // t1
		// 175
		const float p180 = sat(k[85] * r68);
		const float t181 = tv_plain(p177);   // t2
		// 176
		const float p182 = sat(k[86] * m37);
		const float t183 = tv_plain(p178);   // t3
		// 177
		const float p184 = sat(k[87] * m73);
		const float t185 = tv_plain(p180);   // t7
		// 178
		const float p186 = t179 * r74 + r74;
		const float r187 = w24(p186);   // r56
		const float t188 = tv_plain(p182);   // t1
		// 179
		const float p189 = t181 * r82 + r82;
		const float r190 = w24(p189);   // r57
		const float t191 = tv_plain(p184);   // t2
		// 17a
		const float p192 = sat((k[90] * r27) * 4.0f);
		const float m193 = w24(p192);   // m2c
		// 17b
		const float p194 = sat((k[91] * r56) * 4.0f);
		const float m195 = w24(p194);   // m2d
		// 17c
		const float p196 = t183 * r123 + r123;
		const float r197 = w24(p196);   // r58
		// 17d
		const float p198 = t185 * r126 + r126;
		const float r199 = w24(p198);   // r59
		// 17e
		const float p200 = t188 * r169 + r169;
		const float r201 = w24(p200);   // r5a
		// 17f
		const float p202 = t191 * r175 + r175;
		const float r203 = w24(p202);   // r5b
		out[0] = m193;   // m2c
		out[1] = m195;   // m2d
		s_p = p202;
		s_m16 = m109;
		s_r5c = r12;
		s_r5d = r15;
		s_r54 = r77;
		s_r55 = r87;
		s_r56 = r187;
		s_r58 = r197;
		s_r59 = r199;
		s_r5a = r201;
		s_r5e = r41;
		s_r5f = r44;
		s_m17 = m139;
		s_m18 = m173;
		s_r57 = r190;
		s_r5b = r203;
		s_r60 = r118;
		s_r61 = r120;
		s_r44 = r92;
		s_r45 = r134;
		s_r46 = r103;
		s_r47 = r105;
		s_r48 = r111;
		s_r49 = r113;
		s_r4a = r116;
		s_r62 = r162;
		s_r63 = r165;
		s_r4c = r142;
		s_r4d = r145;
		s_r4e = r153;
		s_r4f = r155;
		s_r50 = r160;
	}

private:
	float s_m00 = 0.0f;
	float s_m16 = 0.0f;
	float s_m17 = 0.0f;
	float s_m18 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r44 = 0.0f;
	float s_r45 = 0.0f;
	float s_r46 = 0.0f;
	float s_r47 = 0.0f;
	float s_r48 = 0.0f;
	float s_r49 = 0.0f;
	float s_r4a = 0.0f;
	float s_r4c = 0.0f;
	float s_r4d = 0.0f;
	float s_r4e = 0.0f;
	float s_r4f = 0.0f;
	float s_r50 = 0.0f;
	float s_r54 = 0.0f;
	float s_r55 = 0.0f;
	float s_r56 = 0.0f;
	float s_r57 = 0.0f;
	float s_r58 = 0.0f;
	float s_r59 = 0.0f;
	float s_r5a = 0.0f;
	float s_r5b = 0.0f;
	float s_r5c = 0.0f;
	float s_r5d = 0.0f;
	float s_r5e = 0.0f;
	float s_r5f = 0.0f;
	float s_r60 = 0.0f;
	float s_r61 = 0.0f;
	float s_r62 = 0.0f;
	float s_r63 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_DUAL_ROTARY_H
