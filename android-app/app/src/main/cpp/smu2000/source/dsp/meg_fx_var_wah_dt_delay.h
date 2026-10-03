// license:BSD-3-Clause
// S-MU2000: バリエーション: WAH+DT+DLY, WAH+OD+DLY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_WAH_DT_DELAY_H
#define S_MU2000_DSP_MEG_FX_VAR_WAH_DT_DELAY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_wah_dt_delay : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x40000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat((k[0] * in[0]) * 2.0f);
		const float r2 = w24(p1);   // r44
		// 121
		const float p3 = k[1] * s_r45;
		// 122
		const float p4 = k[2] * s_r44 + p3;
		// 123
		const float p5 = sat((k[3] * r2 + p4) * 2.0f);
		const float r6 = w24(p5);   // r45
		// 124
		const float p7 = k[4] * s_r46;
		const float m8 = lfo[18];   // m34
		// 125
		const float p9 = k[5] * s_r45 + p7;
		// 126
		const float p10 = sat((k[6] * r6 + p9) * 4.0f);
		const float r11 = w24(p10);   // r46
		// 127
		const float p12 = sat((k[7] * in[1]) * 2.0f);
		const float r13 = w24(p12);   // r47
		// 128
		const float p14 = k[8] * s_r48;
		// 129
		const float p15 = k[9] * s_r47 + p14;
		// 12a
		const float p16 = sat((k[10] * r13 + p15) * 2.0f);
		const float r17 = w24(p16);   // r48
		// 12b
		const float p18 = k[11] * s_r49;
		// 12c
		const float p19 = k[12] * s_r48 + p18;
		// 12d
		const float p20 = sat((k[13] * r17 + p19) * 4.0f);
		const float r21 = w24(p20);   // r49
		// 12e
		const float p22 = k[14] * s_m18;
		// 12f
		const float p23 = sat(k[15] * s_m19 + p22);
		const float r24 = w24(p23);   // r50
		// 130
		const float p25 = k[16] * s_r51;
		// 131
		const float p26 = k[17] * s_r50 + p25;
		// 132
		const float p27 = sat((k[18] * r24 + p26) * 2.0f);
		const float r28 = w24(p27);   // r51
		// 133
		const float p29 = k[19] * s_r52;
		// 134
		const float p30 = k[20] * s_r51 + p29;
		// 135
		const float p31 = sat((k[21] * r28 + p30) * 2.0f);
		const float r32 = w24(p31);   // r52
		// 136
		const float p33 = k[22] * r11;
		// 137
		const float p34 = sat(k[23] * r21 + p33);
		const float r35 = w24(p34);   // r4a
		// 138
		const float p36 = sat((k[24] * r32) * 16.0f);
		const float r37 = w24(p36);   // r02
		// 139
		const float p38 = k[25] * s_r4a;
		// 13a
		const float p39 = k[26] * r35 + p38;
		// 13b
		const float p40 = sat((k[27] * s_r4b + p39) * 2.0f);
		const float r41 = w24(p40);   // r4b
		// 13c
		const float p42 = sat((k[28] * r37) * 16.0f);
		const float r43 = w24(p42);   // r02
		// 13d
		const float p44 = k[29] * s_m16 - r35;
		// 13e
		const float p45 = sat(k[30] * s_r4f - p44);
		const float m46 = w24(p45);   // m02
		// 13f
		const float p47 = satabs((k[31] * r41) * 16.0f);
		const float m48 = w24(p47);   // m01
		// 140
		const float p49 = sat((k[32] * r43) * 16.0f);
		const float r50 = w24(p49);   // r02
		// 141
		const float p51 = k[33] * s_r58;
		// 142
		const float p52 = sat((k[34] * s_m18 + p51) * 4.0f);
		const float m53 = w24(p52);   // m05
		// 143
		const float p54 = k[35] * s_r58;
		// 144
		const float p55 = sat((k[36] * s_m19 + p54) * 4.0f);
		const float m56 = w24(p55);   // m06
		// 145
		const float p57 = sat((k[37] * r50) * 16.0f);
		const float r58 = w24(p57);   // r03
		// 146
		const float p59 = satpos(k[38] * s_r4c - m48);
		// 147
		const float p60 = sat(k[39] * m48 + p59);
		const float r61 = w24(p60);   // r4c
		// 148
		const float p62 = k[40] * s_r4d;
		// 149
		const float p63 = k[41] * s_r4d + (p62 * (1.0f / 32768.0f));
		// 14a
		const float p64 = k[42] * m48 - p63;
		const float q65 = ram[at(42)];
		// 14b
		const float p66 = satpos(k[43] * r61 - p64);
		// 14c
		const float p67 = sat(k[44] * m48 + p66);
		const float m68 = q65;   // m09
		const float r69 = w24(p67);   // r4d
		// 14d
		const float p70 = sat((k[45] * r58) * 16.0f);
		const float r71 = w24(p70);   // r04
		const float q72 = ram[at(45)];
		// 14e
		const float p73 = k[46] * s_r4d;
		// 14f
		const float p74 = k[47] * r69 + p73;
		const float m75 = q72;   // m08
		// 150
		const float p76 = sat(k[48] * s_r4e + p74);
		const float r77 = w24(p76);   // r4e
		const float q78 = ram[at(48)];
		// 151
		const float p79 = sat((k[49] * r71) * 16.0f);
		const float r80 = w24(p79);   // r05
		// 152
		const float p81 = k[50];
		const float m82 = q78;   // m07
		// 153
		const float p83 = k[51] * m8 + p81;
		// 154
		const float p84 = sat(k[52] * r77 + p83);
		const float r85 = w24(p84);   // r01
		// 155
		const float p86 = sat((k[53] * r80) * 16.0f);
		const float m87 = w24(p86);   // m04
		// 156
		const float p88 = k[54] * m68;
		const float m89 = m68;   // m17
		const float t90 = tv_plain(p84);   // t1
		const float q91 = ram[at(54)];
		// 157
		const float p92 = k[55] * s_m17 + p88;
		// 158
		const float p93 = sat((k[56] * s_r59 + p92) * 2.0f);
		const float m94 = q91;   // m09
		const float r95 = w24(p93);   // r59
		// 159
		const float p96 = sat(t90 * r85);
		const float r97 = w24(p96);   // r01
		// 15a
		const float p98 = k[58] * m53;
		// 15b
		const float p99 = (k[59] * m56 + p98) * 2.0f;
		const float t100 = tv_plain(p96);   // t1
		// 15c
		const float p101 = sat(k[60] * r95 + p99);
		const float w102 = p101;
		// 15d
		const float p103 = t100 * r97;
		// 15e
		const float p104 = sat(k[62] + p103);
		// 15f
		const float p105 = sat((k[63] * m87) * 16.0f);
		const float r106 = w24(p105);   // r02
		ram[at(63)] = w102;
		// 160
		const float p107 = k[64] * r50;
		const float t108 = tv_plain(p104);   // t1
		// 161
		const float p109 = k[65] * r58 + p107;
		// 162
		const float p110 = k[66] * r71 + p109;
		// 163
		const float p111 = k[67] * r80 + p110;
		// 164
		const float p112 = k[68] * m87 + p111;
		// 165
		const float p113 = sat(k[69] * r106 + p112);
		const float r114 = w24(p113);   // r02
		// 166
		const float p115 = sat(t108 * m46 + s_r4f);
		const float r116 = w24(p115);   // r4f
		// 167
		const float p117 = k[71] * m94;
		// 168
		const float p118 = sat(k[72] * m82 + p117);
		const float r119 = w24(p118);   // r03
		// 169
		const float p120 = k[73] * m75;
		// 16a
		const float p121 = sat(k[74] * m82 + p120);
		const float r122 = w24(p121);   // r04
		// 16b
		const float p123 = sat(t108 * r116 + s_m16);
		const float m124 = w24(p123);   // m16
		// 16c
		const float p125 = sat(k[76] * r114);
		const float r126 = w24(p125);   // r53
		// 16d
		const float p127 = k[77] * s_r54;
		// 16e
		const float p128 = k[78] * s_r53 + p127;
		// 16f
		const float p129 = sat((k[79] * r126 + p128) * 2.0f);
		const float r130 = w24(p129);   // r54
		// 170
		const float p131 = k[80] * s_r55;
		// 171
		const float p132 = k[81] * s_r54 + p131;
		const float r133 = s_r54;   // r55
		// 172
		const float p134 = k[82] * r130 + p132;
		// 173
		const float p135 = k[83] * s_r56 + p134;
		const float r136 = s_r56;   // r57
		// 174
		const float p137 = sat((k[84] * s_r57 + p135) * 2.0f);
		const float r138 = w24(p137);   // r56
		// 175
		const float p139 = k[85] * s_r58;
		// 176
		const float p140 = k[86] * s_r56 + p139;
		// 177
		const float p141 = sat((k[87] * r138 + p140) * 4.0f);
		const float r142 = w24(p141);   // r58
		// 178
		const float p143 = k[88] * r11;
		// 179
		const float p144 = sat((k[89] * r116 + p143) * 4.0f);
		const float m145 = w24(p144);   // m18
		// 17a
		const float p146 = k[90] * r21;
		// 17b
		const float p147 = sat((k[91] * r116 + p146) * 4.0f);
		const float m148 = w24(p147);   // m19
		// 17c
		const float p149 = k[92] * m53;
		// 17d
		const float p150 = sat((k[93] * r119 + p149) * 4.0f);
		const float m151 = w24(p150);   // m2c
		// 17e
		const float p152 = k[94] * m56;
		// 17f
		const float p153 = sat((k[95] * r122 + p152) * 4.0f);
		const float m154 = w24(p153);   // m2d
		out[0] = m151;   // m2c
		out[1] = m154;   // m2d
		s_p = p153;
		s_r45 = r6;
		s_r44 = r2;
		s_r46 = r11;
		s_r48 = r17;
		s_r47 = r13;
		s_r49 = r21;
		s_m18 = m145;
		s_m19 = m148;
		s_r51 = r28;
		s_r50 = r24;
		s_r52 = r32;
		s_r4a = r35;
		s_r4b = r41;
		s_m16 = m124;
		s_r4f = r116;
		s_r58 = r142;
		s_r4c = r61;
		s_r4d = r69;
		s_r4e = r77;
		s_m17 = m89;
		s_r59 = r95;
		s_r54 = r130;
		s_r53 = r126;
		s_r55 = r133;
		s_r56 = r138;
		s_r57 = r136;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_WAH_DT_DELAY_H
