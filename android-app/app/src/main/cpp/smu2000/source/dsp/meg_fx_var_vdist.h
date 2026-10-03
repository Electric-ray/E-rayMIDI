// license:BSD-3-Clause
// S-MU2000: バリエーション: V DT HARD, V DT H+DLY, V DT SOFT, V DT S+DLY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_VDIST_H
#define S_MU2000_DSP_MEG_FX_VAR_VDIST_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_vdist : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float q1 = ram[at(0)];
		// 121
		const float p2 = k[1] * in[0];
		// 122
		const float p3 = sat(k[2] * in[1] + p2);
		const float m4 = q1;   // m01
		const float r5 = w24(p3);   // r44
		// 123
		const float p6 = k[3] * s_r45;
		const float q7 = ram[at(3)];
		// 124
		const float p8 = k[4] * s_r44 + p6;
		// 125
		const float p9 = sat((k[5] * r5 + p8) * 2.0f);
		const float m10 = q7;   // m02
		const float r11 = w24(p9);   // r45
		// 126
		const float p12 = k[6] * s_r46;
		const float q13 = ram[at(6)];
		// 127
		const float p14 = k[7] * s_r45 + p12;
		const float r15 = s_r45;   // r46
		// 128
		const float p16 = k[8] * r11 + p14;
		const float m17 = q13;   // m03
		// 129
		const float p18 = k[9] * s_r47 + p16;
		const float q19 = ram[at(9)];
		// 12a
		const float p20 = sat((k[10] * s_r48 + p18) * 4.0f);
		const float r21 = w24(p20);   // r47
		// 12b
		const float p22 = k[11] * s_r49;
		const float m23 = q19;   // m04
		// 12c
		const float p24 = k[12] * s_r47 + p22;
		const float r25 = s_r47;   // r48
		// 12d
		const float p26 = sat((k[13] * r21 + p24) * 2.0f);
		const float r27 = w24(p26);   // r49
		// 12e
		// 12f
		// 130
		const float p28 = sat((k[16] * r27) * 16.0f);
		// 131
		const float p29 = sat((p28) * 16.0f);
		// 132
		const float p30 = sat((p29) * 16.0f);
		const float r31 = w24(p30);   // r02
		// 133
		const float p32 = sat(k[19] + p30);
		const float r33 = w24(p32);   // r03
		// 134
		const float p34 = k[20] * s_r4b;
		const float r35 = w24(p34);   // r4c
		// 135
		const float p36 = k[21] * s_r4e;
		const float t37 = tv_plain(p32);   // t1
		// 136
		const float p38 = sat((k[22] * s_r4c + p36) * 2.0f);
		const float r39 = s_r4c;   // r4d
		// 137
		const float p40 = k[23] * r35 + p38;
		// 138
		const float p41 = k[24] * s_r4d + p40;
		// 139
		const float p42 = sat(k[25] * s_r4f + p41);
		const float r43 = w24(p42);   // r4e
		// 13a
		const float p44 = k[26] * s_r4f;
		// 13b
		const float p45 = k[27] * s_r4e + p44;
		const float r46 = s_r4e;   // r4f
		// 13c
		const float p47 = k[28] * r43 + p45;
		// 13d
		const float p48 = k[29] * s_r50 + p47;
		// 13e
		const float p49 = sat((k[30] * s_r51 + p48) * 4.0f);
		const float r50 = w24(p49);   // r50
		// 13f
		const float p51 = k[31] * s_r52;
		// 140
		const float p52 = k[32] * s_r50 + p51;
		const float r53 = s_r50;   // r51
		// 141
		const float p54 = sat((k[33] * r50 + p52) * 2.0f);
		const float r55 = w24(p54);   // r52
		// 142
		const float p56 = k[34] * s_r53;
		// 143
		const float p57 = k[35] * s_r52 + p56;
		// 144
		const float p58 = sat((k[36] * r55 + p57) * 4.0f);
		const float r59 = w24(p58);   // r53
		// 145
		// 146
		const float p60 = k[38] * s_r53;
		// 147
		const float p61 = sat((k[39] * r59 + p60) * 16.0f);
		// 148
		const float p62 = sat(k[40] * s_r54 + p61);
		const float r63 = w24(p62);   // r54
		// 149
		const float p64 = k[41] * s_r55;
		// 14a
		const float p65 = k[42] * s_r54 + p64;
		// 14b
		const float p66 = sat((k[43] * r63 + p65) * 4.0f);
		const float r67 = w24(p66);   // r55
		// 14c
		const float p68 = k[44] * s_r56;
		// 14d
		const float p69 = k[45] * s_r55 + p68;
		const float r70 = s_r55;   // r56
		// 14e
		const float p71 = k[46] * r67 + p69;
		// 14f
		const float p72 = k[47] * s_r57 + p71;
		const float r73 = s_r57;   // r58
		// 150
		const float p74 = sat((k[48] * s_r58 + p72) * 4.0f);
		const float r75 = w24(p74);   // r57
		// 151
		const float p76 = k[49] * s_r59;
		// 152
		const float p77 = k[50] * s_r57 + p76;
		const float r78 = s_r57;   // r58
		// 153
		const float p79 = sat((k[51] * r75 + p77) * 4.0f);
		const float r80 = w24(p79);   // r59
		// 154
		const float p81 = sat(t37 * r33);
		const float r82 = w24(p81);   // r03
		// 155
		// 156
		const float p83 = k[54] * s_r4a - s_r4a;
		const float t84 = k[54];   // t2（定数）
		// 157
		const float p85 = t84 * r82 - p83;
		const float r86 = w24(p85);   // r4a
		// 158
		const float p87 = r82 - p85;
		const float r88 = w24(p87);   // r03
		// 159
		const float p89 = sat((k[57] * r80) * 2.0f);
		const float r90 = w24(p89);   // r05
		// 15a
		const float p91 = sat(k[58] * r31 - r31);
		const float t92 = k[58];   // t2（定数）
		// 15b
		const float p93 = sat(t92 * r88 - p91);
		const float r94 = w24(p93);   // r03
		// 15c
		const float p95 = k[60] * r90;
		const float t96 = k[60];   // t2（定数）
		// 15d
		const float p97 = sat((k[61] * in[0] + p95) * 4.0f);
		const float r98 = w24(p97);   // r06
		// 15e
		const float p99 = sat((k[62] * r94) * 16.0f);
		const float r100 = w24(p99);   // r4b
		// 15f
		const float p101 = t96 * r90;
		// 160
		const float p102 = sat((k[64] * in[1] + p101) * 4.0f);
		const float r103 = w24(p102);   // r07
		// 161
		const float p104 = k[65] * r100;
		// 162
		const float p105 = sat(k[66] + p104);
		const float r106 = w24(p105);   // r01
		// 163
		// 164
		const float t107 = tv_plain(p105);   // t1
		// 165
		const float p108 = sat(t107 * r106);
		const float r109 = w24(p108);   // r02
		// 166
		// 167
		// 168
		const float p110 = sat(t107 * r109);
		const float r111 = w24(p110);   // r03
		// 169
		const float p112 = k[73] * r106;
		// 16a
		const float p113 = k[74] * r109 + p112;
		// 16b
		const float p114 = sat((k[75] * r111 + p113) * 2.0f);
		const float r115 = w24(p114);   // r4b
		// 16c
		const float p116 = k[76] * m4;
		const float m117 = m4;   // m16
		// 16d
		const float p118 = k[77] * s_m16 + p116;
		// 16e
		const float p119 = sat((k[78] * s_r5a + p118) * 2.0f);
		const float r120 = w24(p119);   // r5a
		// 16f
		const float p121 = k[79] * r98;
		// 170
		const float p122 = (k[80] * r103 + p121) * 2.0f;
		// 171
		const float p123 = sat(k[81] * r120 + p122);
		const float w124 = p123;
		// 172
		const float p125 = k[82] * m23;
		// 173
		const float p126 = sat(k[83] * m17 + p125);
		const float r127 = w24(p126);   // r01
		// 174
		const float p128 = k[84] * m17;
		ram[at(84)] = w124;
		// 175
		const float p129 = sat(k[85] * m10 + p128);
		const float r130 = w24(p129);   // r02
		// 176
		// 177
		const float p131 = k[87] * r98;
		// 178
		const float p132 = sat((k[88] * r127 + p131) * 4.0f);
		const float m133 = w24(p132);   // m2c
		// 179
		const float p134 = k[89] * r103;
		// 17a
		const float p135 = sat((k[90] * r130 + p134) * 4.0f);
		const float m136 = w24(p135);   // m2d
		// 17b
		// 17c
		// 17d
		// 17e
		// 17f
		out[0] = m133;   // m2c
		out[1] = m136;   // m2d
		s_p = p135;
		s_r45 = r11;
		s_r44 = r5;
		s_r46 = r15;
		s_r47 = r21;
		s_r48 = r25;
		s_r49 = r27;
		s_r4b = r115;
		s_r4e = r43;
		s_r4c = r35;
		s_r4d = r39;
		s_r4f = r46;
		s_r50 = r50;
		s_r51 = r53;
		s_r52 = r55;
		s_r53 = r59;
		s_r54 = r63;
		s_r55 = r67;
		s_r56 = r70;
		s_r57 = r75;
		s_r58 = r78;
		s_r59 = r80;
		s_r4a = r86;
		s_m16 = m117;
		s_r5a = r120;
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

#endif // S_MU2000_DSP_MEG_FX_VAR_VDIST_H
