// license:BSD-3-Clause
// S-MU2000: バリエーション: PHASER 1, PHASER 2, T.PHASER
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_PHASER_H
#define S_MU2000_DSP_MEG_FX_VAR_PHASER_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_phaser : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x40000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0];
		// 121
		const float p2 = k[1] * s_m16 - (p1 * (1.0f / 32768.0f));
		// 122
		const float p3 = k[2] - p2;
		const float m4 = w24(p3);   // m16
		// 123
		const float p5 = sat((k[3] * in[0]) * 2.0f);
		const float r6 = w24(p5);   // r44
		// 124
		const float p7 = k[4] * s_r44;
		const float m8 = lfo[18];   // m01
		// 125
		const float p9 = k[5] * s_r45 + p7;
		// 126
		const float p10 = sat((k[6] * r6 + p9) * 2.0f);
		const float r11 = w24(p10);   // r45
		// 127
		const float p12 = k[7] * s_r45;
		// 128
		const float p13 = k[8] * s_r46 + p12;
		// 129
		const float p14 = sat((k[9] * r11 + p13) * 4.0f);
		const float r15 = w24(p14);   // r46
		// 12a
		const float p16 = k[10] + m8;
		const float r17 = w24(p16);   // r04
		// 12b
		const float p18 = k[11] + m8;
		const float r19 = w24(p18);   // r05
		// 12c
		const float p20 = sat((k[12] * in[1]) * 2.0f);
		const float r21 = w24(p20);   // r47
		// 12d
		const float p22 = k[13] * s_r47;
		// 12e
		const float p23 = k[14] * s_r48 + p22;
		// 12f
		const float p24 = sat((k[15] * r21 + p23) * 2.0f);
		const float r25 = w24(p24);   // r48
		// 130
		const float p26 = k[16] * s_r48;
		// 131
		const float p27 = k[17] * s_r49 + p26;
		// 132
		const float p28 = sat((k[18] * r25 + p27) * 4.0f);
		const float r29 = w24(p28);   // r49
		// 133
		const float p30 = satabs(k[19] * r17);
		// 134
		const float p31 = sat(k[20] + p30);
		const float r32 = w24(p31);   // r04
		// 135
		const float p33 = satabs(k[21] * r19);
		// 136
		const float p34 = sat(k[22] + p33);
		const float r35 = w24(p34);   // r05
		const float t36 = tv_plain(p31);   // t1
		// 137
		const float p37 = k[23] * s_r4c;
		// 138
		const float p38 = k[24] * s_r4d + p37;
		const float t39 = tv_plain(p34);   // t2
		// 139
		const float p40 = k[25] * s_r4e + p38;
		// 13a
		const float p41 = k[26] * s_r4f + p40;
		// 13b
		const float p42 = sat((k[27] * s_r50 + p41) * 2.0f);
		const float r43 = w24(p42);   // r02
		// 13c
		const float p44 = sat(t36 * r32);
		const float r45 = w24(p44);   // r04
		// 13d
		const float p46 = sat(t39 * r35);
		const float r47 = w24(p46);   // r05
		// 13e
		const float p48 = k[30] * s_r53;
		const float t49 = tv_plain(p44);   // t1
		// 13f
		const float p50 = k[31] * s_r54 + p48;
		const float t51 = tv_plain(p46);   // t2
		// 140
		const float p52 = k[32] * s_r55 + p50;
		// 141
		const float p53 = k[33] * s_r56 + p52;
		// 142
		const float p54 = sat((k[34] * s_r57 + p53) * 2.0f);
		const float r55 = w24(p54);   // r03
		// 143
		const float p56 = k[35];
		// 144
		const float p57 = sat(t49 * r45 - p56);
		// 145
		const float p58 = k[37] * r43;
		// 146
		const float p59 = k[38] * r55 + p58;
		const float t60 = tv_plain(p57);   // t1
		// 147
		const float p61 = k[39] * s_r52 + p59;
		// 148
		const float p62 = sat((k[40] * s_r56 + p61) * 2.0f);
		const float r63 = w24(p62);   // r01
		// 149
		const float p64 = k[41];
		// 14a
		const float p65 = sat(t51 * r47 - p64);
		// 14b
		const float p66 = k[43] * r15;
		// 14c
		const float p67 = (k[44] * r29 + p66) * 2.0f;
		const float t68 = tv_plain(p65);   // t2
		// 14d
		const float p69 = k[45] * r43 + p67;
		// 14e
		const float p70 = sat(k[46] * r55 + p69);
		const float r71 = w24(p70);   // r4a
		// 14f
		const float p72 = s_r4a;
		// 150
		const float p73 = t60 * s_r4b - p72;
		// 151
		const float p74 = sat(t60 * r71 - p73);
		const float r75 = w24(p74);   // r4b
		// 152
		const float p76 = s_r4b;
		// 153
		const float p77 = t60 * s_r4c - p76;
		// 154
		const float p78 = sat(t60 * r75 - p77);
		const float r79 = w24(p78);   // r4c
		// 155
		const float p80 = s_r4c;
		// 156
		const float p81 = t60 * s_r4d - p80;
		// 157
		const float p82 = sat(t60 * r79 - p81);
		const float r83 = w24(p82);   // r4d
		// 158
		const float p84 = s_r4d;
		// 159
		const float p85 = t60 * s_r4e - p84;
		// 15a
		const float p86 = sat(t60 * r83 - p85);
		const float r87 = w24(p86);   // r4e
		// 15b
		const float p88 = s_r4e;
		// 15c
		const float p89 = t60 * s_r4f - p88;
		// 15d
		const float p90 = sat(t60 * r87 - p89);
		const float r91 = w24(p90);   // r4f
		// 15e
		const float p92 = s_r4f;
		// 15f
		const float p93 = t60 * s_r50 - p92;
		// 160
		const float p94 = sat(t60 * r91 - p93);
		const float r95 = w24(p94);   // r50
		// 161
		const float p96 = (k[65] * r29) * 2.0f;
		// 162
		const float p97 = k[66] * r55 + p96;
		// 163
		const float p98 = sat(k[67] * r43 + p97);
		const float r99 = w24(p98);   // r51
		// 164
		const float p100 = s_r51;
		// 165
		const float p101 = t68 * s_r52 - p100;
		// 166
		const float p102 = sat(t68 * r99 - p101);
		const float r103 = w24(p102);   // r52
		// 167
		const float p104 = s_r52;
		// 168
		const float p105 = t68 * s_r53 - p104;
		// 169
		const float p106 = sat(t68 * r103 - p105);
		const float r107 = w24(p106);   // r53
		// 16a
		const float p108 = s_r53;
		// 16b
		const float p109 = t68 * s_r54 - p108;
		// 16c
		const float p110 = sat(t68 * r107 - p109);
		const float r111 = w24(p110);   // r54
		// 16d
		const float p112 = s_r54;
		// 16e
		const float p113 = t68 * s_r55 - p112;
		// 16f
		const float p114 = sat(t68 * r111 - p113);
		const float r115 = w24(p114);   // r55
		// 170
		const float p116 = s_r55;
		// 171
		const float p117 = t68 * s_r56 - p116;
		// 172
		const float p118 = sat(t68 * r115 - p117);
		const float r119 = w24(p118);   // r56
		// 173
		const float p120 = s_r56;
		// 174
		const float p121 = t68 * s_r57 - p120;
		// 175
		const float p122 = sat(t68 * r119 - p121);
		const float r123 = w24(p122);   // r57
		// 176
		const float p124 = k[86] * r15;
		// 177
		const float p125 = sat((k[87] * r63 + p124) * 4.0f);
		const float m126 = w24(p125);   // m2c
		// 178
		const float p127 = k[88] * r29;
		// 179
		const float p128 = sat((k[89] * r55 + p127) * 4.0f);
		const float m129 = w24(p128);   // m2d
		// 17a
		// 17b
		// 17c
		// 17d
		// 17e
		// 17f
		out[0] = m126;   // m2c
		out[1] = m129;   // m2d
		s_p = p128;
		s_m16 = m4;
		s_r44 = r6;
		s_r45 = r11;
		s_r46 = r15;
		s_r47 = r21;
		s_r48 = r25;
		s_r49 = r29;
		s_r4c = r79;
		s_r4d = r83;
		s_r4e = r87;
		s_r4f = r91;
		s_r50 = r95;
		s_r53 = r107;
		s_r54 = r111;
		s_r55 = r115;
		s_r56 = r119;
		s_r57 = r123;
		s_r52 = r103;
		s_r4a = r71;
		s_r4b = r75;
		s_r51 = r99;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_PHASER_H
