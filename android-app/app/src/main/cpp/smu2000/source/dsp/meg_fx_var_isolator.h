// license:BSD-3-Clause
// S-MU2000: バリエーション: ISOLATOR
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_ISOLATOR_H
#define S_MU2000_DSP_MEG_FX_VAR_ISOLATOR_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_isolator : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; s_r5d = 0; s_r5e = 0; s_r5f = 0; s_r60 = 0; s_r61 = 0; s_r62 = 0; s_r63 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0];
		const float r2 = w24(p1);   // r03
		// 121
		const float p3 = (k[1] * s_m16) * 2.0f;
		const float m4 = s_m16;   // m05
		// 122
		const float p5 = s_r44 + p3;
		// 123
		const float p6 = sat((k[3] * s_m16 + p5) * 2.0f);
		const float r7 = w24(p6);   // r01
		// 124
		const float p8 = (k[4] * s_m18) * 2.0f;
		const float m9 = s_m18;   // m06
		// 125
		const float p10 = s_r54 + p8;
		// 126
		const float p11 = sat((k[6] * s_m18 + p10) * 2.0f);
		const float r12 = w24(p11);   // r02
		// 127
		const float p13 = (k[7] * s_m17) * 2.0f;
		const float m14 = s_m17;   // m07
		// 128
		const float p15 = s_r4d + p13;
		// 129
		const float p16 = sat((k[9] * s_m17 + p15) * 2.0f);
		const float m17 = w24(p16);   // m03
		// 12a
		const float p18 = (k[10] * s_m19) * 2.0f;
		const float m19 = s_m19;   // m08
		// 12b
		const float p20 = s_r5d + p18;
		// 12c
		const float p21 = sat((k[12] * s_m19 + p20) * 2.0f);
		const float m22 = w24(p21);   // m04
		// 12d
		const float p23 = sat(k[13] * r7 + in[0]);
		const float r24 = w24(p23);   // r04
		// 12e
		const float p25 = sat(k[14] * r12 + in[1]);
		const float r26 = w24(p25);   // r05
		// 12f
		const float p27 = sat(k[15] * r2);
		const int32_t i28 = idx_of(p27);
		// 130
		const float p29 = sat(k[16] * r24);
		const float r30 = w24(p29);   // r45
		// 131
		const float p31 = k[17] * s_r46;
		const float t32 = tv_index(p27);   // t2
		// 132
		const float p33 = k[18] * s_r45 + p31;
		const float r34 = s_r45;   // r46
		const float q35 = ram[at(18, i28)];
		// 133
		const float p36 = k[19] * r30 + p33;
		// 134
		const float p37 = k[20] * s_r47 + p36;
		const float m38 = q35;   // m01
		const float r39 = s_r47;   // r48
		// 135
		const float p40 = sat((k[21] * s_r48 + p37) * 2.0f);
		const float r41 = w24(p40);   // r47
		const float q42 = ram[at(21, i28 + 1)];
		// 136
		const float p43 = sat(k[22] * r30 + s_m16);
		const float m44 = w24(p43);   // m16
		const float r45 = w24(p43);   // r04
		// 137
		const float p46 = sat(k[23] * r26);
		const float m47 = q42;   // m02
		const float r48 = w24(p46);   // r55
		// 138
		const float p49 = k[24] * s_r56;
		// 139
		const float p50 = k[25] * s_r55 + p49;
		const float r51 = s_r55;   // r56
		// 13a
		const float p52 = k[26] * r48 + p50;
		// 13b
		const float p53 = k[27] * s_r57 + p52;
		const float r54 = s_r57;   // r58
		// 13c
		const float p55 = sat((k[28] * s_r58 + p53) * 2.0f);
		const float r56 = w24(p55);   // r57
		// 13d
		const float p57 = sat(k[29] * r48 + s_m18);
		const float m58 = w24(p57);   // m18
		const float r59 = w24(p57);   // r05
		// 13e
		const float p60 = sat(r45 + m4);
		const float m61 = w24(p60);   // m05
		// 13f
		const float p62 = sat(k[31] * m17 + r41);
		const float r63 = w24(p62);   // r06
		// 140
		const float p64 = sat(k[32] * m22 + r56);
		const float r65 = w24(p64);   // r07
		// 141
		const float p66 = sat(r59 + m9);
		const float m67 = w24(p66);   // m06
		// 142
		const float p68 = sat(k[34] * m61 + s_r44);
		const float r69 = w24(p68);   // r44
		// 143
		const float p70 = sat(k[35] * r2);
		const int32_t i71 = idx_of(p70);
		// 144
		const float p72 = s_r44;
		// 145
		const float p73 = sat(r69 + p72);
		const float r74 = w24(p73);   // r49
		const float t75 = tv_index(p70);   // t3
		// 146
		const float p76 = k[38] * s_r4a;
		// 147
		const float p77 = k[39] * s_r49 + p76;
		const float r78 = s_r49;   // r4a
		const float q79 = ram[at(39, i71)];
		// 148
		const float p80 = k[40] * r74 + p77;
		// 149
		const float p81 = k[41] * s_r4b + p80;
		const float m82 = q79;   // m03
		const float r83 = s_r4b;   // r4c
		// 14a
		const float p84 = sat((k[42] * s_r4c + p81) * 2.0f);
		const float r85 = w24(p84);   // r4b
		const float w86 = p84;
		const float q87 = ram[at(42, i71 + 1)];
		// 14b
		const float p88 = sat(k[43] * m67 + s_r54);
		const float r89 = w24(p88);   // r54
		// 14c
		const float p90 = sat(k[44] * in[0]);
		const float m91 = w24(p90);   // m05
		// 14d
		const float p92 = s_r54;
		const float m93 = q87;   // m04
		ram[at(45)] = w86;
		// 14e
		const float p94 = sat(r89 + p92);
		const float r95 = w24(p94);   // r59
		// 14f
		const float p96 = k[47] * s_r5c;
		// 150
		const float p97 = k[48] * s_r59 + p96;
		const float r98 = s_r59;   // r5c
		// 151
		const float p99 = k[49] * r95 + p97;
		// 152
		const float p100 = k[50] * s_r5b + p99;
		const float r101 = s_r5b;   // r5a
		// 153
		const float p102 = sat((k[51] * s_r5a + p100) * 2.0f);
		const float r103 = w24(p102);   // r5b
		const float w104 = p102;
		// 154
		const float p105 = sat(k[52] * r63);
		const float r106 = w24(p105);   // r4e
		// 155
		const float p107 = k[53] * s_r4f;
		// 156
		const float p108 = k[54] * s_r4e + p107;
		const float r109 = s_r4e;   // r4f
		ram[at(54)] = w104;
		// 157
		const float p110 = k[55] * r106 + p108;
		// 158
		const float p111 = k[56] * s_r50 + p110;
		const float r112 = s_r50;   // r51
		// 159
		const float p113 = sat((k[57] * s_r51 + p111) * 2.0f);
		const float r114 = w24(p113);   // r50
		// 15a
		const float p115 = sat(k[58] * r65);
		const float r116 = w24(p115);   // r5e
		// 15b
		const float p117 = k[59] * s_r5f;
		// 15c
		const float p118 = k[60] * s_r5e + p117;
		const float r119 = s_r5e;   // r5f
		// 15d
		const float p120 = k[61] * r116 + p118;
		// 15e
		const float p121 = k[62] * s_r60 + p120;
		const float r122 = s_r60;   // r61
		// 15f
		const float p123 = sat((k[63] * s_r61 + p121) * 2.0f);
		const float r124 = w24(p123);   // r60
		// 160
		const float p125 = sat(k[64] * r106 + s_m17);
		const float m126 = w24(p125);   // m17
		const float r127 = w24(p125);   // r06
		// 161
		const float p128 = sat(k[65] * r116 + s_m19);
		const float m129 = w24(p128);   // m19
		const float r130 = w24(p128);   // r07
		// 162
		const float p131 = t32 * m38 - m38;
		// 163
		const float p132 = sat(t32 * m47 - p131);
		const float r133 = w24(p132);   // r08
		// 164
		const float p134 = sat(r127 + m14);
		const float m135 = w24(p134);   // m07
		// 165
		const float p136 = sat(r130 + m19);
		const float m137 = w24(p136);   // m08
		// 166
		const float p138 = t75 * m82 - m82;
		// 167
		const float p139 = sat(t75 * m93 - p138);
		const float r140 = w24(p139);   // r09
		// 168
		const float p141 = sat(k[72] * m135 + s_r4d);
		const float r142 = w24(p141);   // r4d
		// 169
		const float p143 = sat(k[73] * in[1]);
		const float m144 = w24(p143);   // m06
		// 16a
		const float p145 = s_r4d;
		// 16b
		const float p146 = sat(r142 + p145);
		const float r147 = w24(p146);   // r06
		// 16c
		const float p148 = sat(k[76] * m137 + s_r5d);
		const float r149 = w24(p148);   // r5d
		// 16d
		// 16e
		const float p150 = s_r5d;
		// 16f
		const float p151 = sat(r149 + p150);
		const float r152 = w24(p151);   // r07
		// 170
		const float p153 = k[80] * r147;
		// 171
		const float p154 = k[81] * s_r52 + p153;
		// 172
		const float p155 = sat((k[82] * s_r53 + p154) * 2.0f);
		const float r156 = w24(p155);   // r52
		// 173
		const float p157 = k[83] * s_r52 + p155;
		const float r158 = s_r52;   // r53
		// 174
		const float p159 = sat(k[84] * s_r53 + p157);
		const float r160 = w24(p159);   // r06
		// 175
		const float p161 = k[85] * r152;
		// 176
		const float p162 = k[86] * s_r62 + p161;
		// 177
		const float p163 = sat((k[87] * s_r63 + p162) * 2.0f);
		const float r164 = w24(p163);   // r62
		// 178
		const float p165 = k[88] * s_r62 + p163;
		const float r166 = s_r62;   // r63
		// 179
		const float p167 = sat(k[89] * s_r63 + p165);
		const float r168 = w24(p167);   // r07
		// 17a
		const float p169 = k[90] * r133 + m91;
		// 17b
		const float p170 = (k[91] * r160 + p169) * 2.0f;
		// 17c
		const float p171 = sat((k[92] * r114 + p170) * 16.0f);
		const float m172 = w24(p171);   // m2c
		// 17d
		const float p173 = k[93] * r140 + m144;
		// 17e
		const float p174 = (k[94] * r168 + p173) * 2.0f;
		// 17f
		const float p175 = sat((k[95] * r124 + p174) * 16.0f);
		const float m176 = w24(p175);   // m2d
		out[0] = m172;   // m2c
		out[1] = m176;   // m2d
		s_p = p175;
		s_m16 = m44;
		s_r44 = r69;
		s_m18 = m58;
		s_r54 = r89;
		s_m17 = m126;
		s_r4d = r142;
		s_m19 = m129;
		s_r5d = r149;
		s_r46 = r34;
		s_r45 = r30;
		s_r47 = r41;
		s_r48 = r39;
		s_r56 = r51;
		s_r55 = r48;
		s_r57 = r56;
		s_r58 = r54;
		s_r4a = r78;
		s_r49 = r74;
		s_r4b = r85;
		s_r4c = r83;
		s_r5c = r98;
		s_r59 = r95;
		s_r5b = r103;
		s_r5a = r101;
		s_r4f = r109;
		s_r4e = r106;
		s_r50 = r114;
		s_r51 = r112;
		s_r5f = r119;
		s_r5e = r116;
		s_r60 = r124;
		s_r61 = r122;
		s_r52 = r156;
		s_r53 = r158;
		s_r62 = r164;
		s_r63 = r166;
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

#endif // S_MU2000_DSP_MEG_FX_VAR_ISOLATOR_H
