// license:BSD-3-Clause
// S-MU2000: バリエーション: DT+DELAY, OD+DELAY, CMP+DT+DLY, CMP+OD+DLY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_DT_DELAY_H
#define S_MU2000_DSP_MEG_FX_VAR_DT_DELAY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_dt_delay : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat((k[0] * s_m16) * 16.0f);
		const float r2 = w24(p1);   // r01
		const float q3 = ram[at(0)];
		// 121
		const float p4 = k[1] * in[0];
		// 122
		const float p5 = sat((k[2] * in[1] + p4) * 2.0f);
		const float m6 = q3;   // m06
		const float r7 = w24(p5);   // r44
		// 123
		const float p8 = k[3] * s_r45;
		const float q9 = ram[at(3)];
		// 124
		const float p10 = k[4] * s_r44 + p8;
		const float r11 = s_r44;   // r45
		// 125
		const float p12 = k[5] * r7 + p10;
		const float m13 = q9;   // m07
		// 126
		const float p14 = k[6] * s_r46 + p12;
		const float r15 = s_r46;   // r47
		const float q16 = ram[at(6)];
		// 127
		const float p17 = sat((k[7] * s_r47 + p14) * 2.0f);
		const float r18 = w24(p17);   // r46
		// 128
		const float p19 = sat((k[8] * r2) * 16.0f);
		const float m20 = q16;   // m08
		const float r21 = w24(p19);   // r01
		// 129
		const float q22 = ram[at(9)];
		// 12a
		const float p23 = sat((k[10] * r18) * 2.0f);
		const float r24 = w24(p23);   // r09
		// 12b
		const float p25 = sat((k[11] * r21) * 16.0f);
		const float m26 = q22;   // m09
		const float r27 = w24(p25);   // r01
		// 12c
		const float p28 = k[12] * s_r4a;
		const float r29 = s_r4a;   // r4b
		// 12d
		const float p30 = k[13] * s_r4b + p28;
		// 12e
		const float p31 = sat(k[14] * s_r4c + p30);
		const float r32 = w24(p31);   // r4c
		// 12f
		const float p33 = sat((k[15] * r27) * 16.0f);
		const float r34 = w24(p33);   // r02
		// 130
		const float p35 = satabs((k[16] * r24) * 16.0f);
		const float m36 = w24(p35);   // m01
		const float t37 = tv_plain(p31);   // t1
		// 131
		const float p38 = sat(t37 * r24);
		const float r39 = w24(p38);   // r08
		// 132
		const float p40 = sat((k[18] * r34) * 16.0f);
		const float r41 = w24(p40);   // r03
		// 133
		const float p42 = satpos(k[19] * s_r48 - m36);
		const float t43 = tv_plain(p38);   // t1
		// 134
		const float p44 = sat(k[20] * m36 + p42);
		const float r45 = w24(p44);   // r48
		// 135
		const float p46 = sat((k[21] * r41) * 16.0f);
		const float r47 = w24(p46);   // r04
		// 136
		const float p48 = sat(t43 * r24);
		const float r49 = w24(p48);   // r07
		// 137
		const float p50 = k[23] * r24;
		// 138
		const float p51 = k[24] * r39 + p50;
		// 139
		const float p52 = sat((k[25] * r49 + p51) * 16.0f);
		const float r53 = w24(p52);   // r4d
		// 13a
		const float p54 = sat((k[26] * r47) * 16.0f);
		const float r55 = w24(p54);   // r05
		// 13b
		const float p56 = k[27] * s_r4d;
		// 13c
		const float p57 = k[28] * r53 + p56;
		// 13d
		const float p58 = sat((k[29] * s_m16 + p57) * 2.0f);
		const float m59 = w24(p58);   // m16
		// 13e
		const float p60 = k[30] * s_r49;
		// 13f
		const float p61 = k[31] * s_r49 + (p60 * (1.0f / 32768.0f));
		// 140
		const float p62 = k[32] * m36 - p61;
		// 141
		const float p63 = satpos(k[33] * r45 - p62);
		// 142
		const float p64 = sat(k[34] * m36 + p63);
		const float r65 = w24(p64);   // r49
		// 143
		const float p66 = sat((k[35] * r55) * 16.0f);
		const float r67 = w24(p66);   // r01
		// 144
		const float p68 = k[36] * r27;
		// 145
		const float p69 = k[37] * r34 + p68;
		// 146
		const float p70 = k[38] * r41 + p69;
		// 147
		const float p71 = k[39] * r47 + p70;
		// 148
		const float p72 = k[40] * r55 + p71;
		// 149
		const float p73 = sat(k[41] * r67 + p72);
		const float r74 = w24(p73);   // r01
		// 14a
		const float p75 = sat(k[42] * r65);
		const float r76 = w24(p75);   // r63
		// 14b
		// 14c
		const float p77 = sat(k[44] * r74);
		const float r78 = w24(p77);   // r4e
		// 14d
		const float p79 = k[45] * s_r4f;
		// 14e
		const float p80 = k[46] * s_r4e + p79;
		const float r81 = s_r4e;   // r4f
		// 14f
		const float p82 = k[47] * r78 + p80;
		// 150
		const float p83 = k[48] * s_r50 + p82;
		// 151
		const float p84 = sat((k[49] * s_r51 + p83) * 2.0f);
		const float r85 = w24(p84);   // r50
		// 152
		const float p86 = k[50] * s_r51;
		// 153
		const float p87 = k[51] * s_r50 + p86;
		const float r88 = s_r50;   // r51
		// 154
		const float p89 = k[52] * r85 + p87;
		// 155
		const float p90 = k[53] * s_r52 + p89;
		// 156
		const float p91 = sat((k[54] * s_r53 + p90) * 2.0f);
		const float r92 = w24(p91);   // r52
		// 157
		const float p93 = k[55] * s_r53;
		// 158
		const float p94 = k[56] * s_r52 + p93;
		const float r95 = s_r52;   // r53
		// 159
		const float p96 = k[57] * r92 + p94;
		// 15a
		const float p97 = k[58] * s_r54 + p96;
		const float r98 = s_r54;   // r55
		// 15b
		const float p99 = sat((k[59] * s_r55 + p97) * 16.0f);
		const float r100 = w24(p99);   // r54
		// 15c
		const float p101 = k[60] * m6;
		const float m102 = m6;   // m17
		// 15d
		const float p103 = k[61] * s_m17 + p101;
		// 15e
		const float p104 = sat((k[62] * s_r56 + p103) * 2.0f);
		const float r105 = w24(p104);   // r56
		// 15f
		const float p106 = k[63] * in[0];
		// 160
		const float p107 = sat((k[64] * r100 + p106) * 4.0f);
		const float r108 = w24(p107);   // r01
		// 161
		const float p109 = k[65] * in[1];
		// 162
		const float p110 = sat((k[66] * r100 + p109) * 4.0f);
		const float r111 = w24(p110);   // r02
		// 163
		const float p112 = k[67] * m26;
		// 164
		const float p113 = sat(k[68] * m20 + p112);
		const float r114 = w24(p113);   // r03
		// 165
		const float p115 = k[69] * m20;
		// 166
		const float p116 = sat(k[70] * m13 + p115);
		const float r117 = w24(p116);   // r04
		// 167
		const float p118 = k[71] * r108;
		// 168
		const float p119 = (k[72] * r111 + p118) * 2.0f;
		// 169
		const float p120 = sat(k[73] * r105 + p119);
		const float w121 = p120;
		// 16a
		const float p122 = k[74] * r108;
		// 16b
		const float p123 = sat((k[75] * r114 + p122) * 4.0f);
		const float m124 = w24(p123);   // m2c
		ram[at(75)] = w121;
		// 16c
		const float p125 = k[76] * r111;
		// 16d
		const float p126 = sat((k[77] * r117 + p125) * 4.0f);
		const float m127 = w24(p126);   // m2d
		// 16e
		const float p128 = satpos(k[78] + r76);
		const float r129 = w24(p128);   // r01
		// 16f
		const float p130 = satpos(k[79] + r76);
		const float r131 = w24(p130);   // r02
		// 170
		const float p132 = satpos(k[80] + r76);
		const float r133 = w24(p132);   // r03
		// 171
		const float p134 = satpos(k[81] + r76);
		const float r135 = w24(p134);   // r04
		// 172
		const float p136 = satpos(k[82] + r76);
		const float r137 = w24(p136);   // r05
		// 173
		const float p138 = satpos(k[83] + r76);
		const float r139 = w24(p138);   // r06
		// 174
		const float p140 = satpos(k[84] + r76);
		const float r141 = w24(p140);   // r07
		// 175
		const float p142 = satpos(k[85] + r76);
		const float r143 = w24(p142);   // r08
		// 176
		const float p144 = (k[86] * r129) * 2.0f;
		// 177
		const float p145 = (k[87] * r133 + p144) * 2.0f;
		// 178
		const float p146 = (k[88] * r135 + p145) * 2.0f;
		// 179
		const float p147 = (k[89] * r131 + p146) * 2.0f;
		// 17a
		const float p148 = (k[90] * r137 + p147) * 2.0f;
		// 17b
		const float p149 = (k[91] * r139 + p148) * 4.0f;
		// 17c
		const float p150 = k[92] * r141 + p149;
		// 17d
		const float p151 = k[93] * r143 + p150;
		// 17e
		const float p152 = satpos(k[94] + p151);
		const float r153 = w24(p152);   // r4a
		// 17f
		out[0] = m124;   // m2c
		out[1] = m127;   // m2d
		s_p = p152;
		s_m16 = m59;
		s_r45 = r11;
		s_r44 = r7;
		s_r46 = r18;
		s_r47 = r15;
		s_r4a = r153;
		s_r4b = r29;
		s_r4c = r32;
		s_r48 = r45;
		s_r4d = r53;
		s_r49 = r65;
		s_r4f = r81;
		s_r4e = r78;
		s_r50 = r85;
		s_r51 = r88;
		s_r52 = r92;
		s_r53 = r95;
		s_r54 = r100;
		s_r55 = r98;
		s_m17 = m102;
		s_r56 = r105;
	}

private:
	float s_m00 = 0.0f;
	float s_m16 = 0.0f;
	float s_m17 = 0.0f;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_DT_DELAY_H
