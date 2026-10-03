// license:BSD-3-Clause
// S-MU2000: バリエーション: DELAY LCR, DELAY L,R, ECHO, CROSSDELAY, T.DELAY, T.ECHO, T.CRS DLY, CHORUS 1, CHORUS 2, CHORUS 4, GM CHORUS1, GM CHORUS2, GM CHORUS3, GM CHORUS4, FB CHORUS, CELESTE 1, CELESTE 2, CELESTE 3, CELESTE 4, FLANGER 1, FLANGER 2, FLANGER 3, GM FLANGER, SYMPHONIC, T.FLANGER, THRU
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_DELAY_H
#define S_MU2000_DSP_MEG_FX_VAR_DELAY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_delay : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xfc0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * s_m16;
		const float m2 = lfo[18];   // m34
		// 121
		const float p3 = satpos(k[1] * s_r58 + p1);
		const int32_t i4 = idx_of(p3);
		// 122
		const float p5 = sat((k[2] * s_r5b) * 16.0f);
		// 123
		const float p6 = k[3] * s_r5c + p5;
		const float t7 = tv_index(p3);   // t1
		// 124
		const float p8 = satpos(k[4] + p6);
		const float r9 = w24(p8);   // r04
		// 125
		const float p10 = sat((k[5] * s_r59) * 16.0f);
		// 126
		const float p11 = k[6] * s_r5a + p10;
		const float q12 = ram[at(6, i4)];
		// 127
		const float p13 = satpos(k[7] + p11);
		const float r14 = w24(p13);   // r03
		// 128
		const float p15 = k[8] * s_m18;
		const float m16 = q12;   // m01
		// 129
		const float p17 = satpos(k[9] * r9 + p15);
		const int32_t i18 = idx_of(p17);
		const float q19 = ram[at(9, i4 + 1)];
		// 12a
		const float p20 = sat((k[10] * in[0]) * 2.0f);
		const float r21 = w24(p20);   // r44
		// 12b
		const float p22 = k[11] * s_r44;
		const float m23 = q19;   // m02
		const float r24 = s_r44;   // r45
		const float t25 = tv_index(p17);   // t3
		// 12c
		const float p26 = k[12] * s_r45 + p22;
		const float q27 = ram[at(12, i18)];
		// 12d
		const float p28 = k[13] * s_r46 + p26;
		// 12e
		const float p29 = k[14] * s_r47 + p28;
		const float m30 = q27;   // m05
		// 12f
		const float p31 = sat((k[15] * r21 + p29) * 2.0f);
		const float r32 = w24(p31);   // r46
		const float q33 = ram[at(15, i18 + 1)];
		// 130
		const float p34 = k[16] * s_r46;
		const float m35 = lfo[19];   // m35
		const float r36 = s_r46;   // r47
		// 131
		const float p37 = k[17] * s_r47 + p34;
		const float m38 = q33;   // m06
		// 132
		const float p39 = k[18] * s_r48 + p37;
		const float q40 = ram[at(18, i18)];
		// 133
		const float p41 = k[19] * s_r49 + p39;
		// 134
		const float p42 = sat((k[20] * r32 + p41) * 2.0f);
		const float m43 = q40;   // m07
		const float r44 = w24(p42);   // r48
		// 135
		const float p45 = k[21] * s_r48;
		const float r46 = s_r48;   // r49
		const float q47 = ram[at(21, i18 + 1)];
		// 136
		const float p48 = k[22] * s_r49 + p45;
		// 137
		const float p49 = k[23] * s_r4a + p48;
		const float m50 = q47;   // m08
		const float r51 = s_r4a;   // r4b
		// 138
		const float p52 = k[24] * s_r4b + p49;
		// 139
		const float p53 = sat((k[25] * r44 + p52) * 16.0f);
		const float r54 = w24(p53);   // r4a
		// 13a
		const float p55 = k[26] * s_m17;
		// 13b
		const float p56 = satpos(k[27] * r14 + p55);
		const int32_t i57 = idx_of(p56);
		// 13c
		const float p58 = sat((k[28] + m2) * 2.0f);
		const float r59 = w24(p58);   // r01
		// 13d
		const float p60 = sat((k[29] * in[1]) * 2.0f);
		const float r61 = w24(p60);   // r4c
		const float t62 = tv_index(p56);   // t2
		// 13e
		const float p63 = k[30] * s_r4c;
		const float r64 = s_r4c;   // r4d
		const float q65 = ram[at(30, i57)];
		// 13f
		const float p66 = k[31] * s_r4d + p63;
		// 140
		const float p67 = k[32] * s_r4e + p66;
		const float m68 = q65;   // m03
		// 141
		const float p69 = k[33] * s_r4f + p67;
		const float m70 = lfo[20];   // m36
		const float q71 = ram[at(33, i57 + 1)];
		// 142
		const float p72 = sat((k[34] * r61 + p69) * 2.0f);
		const float r73 = w24(p72);   // r4e
		// 143
		const float p74 = k[35] * s_r4e;
		const float m75 = q71;   // m04
		const float r76 = s_r4e;   // r4f
		// 144
		const float p77 = k[36] * s_r4f + p74;
		// 145
		const float p78 = k[37] * s_r50 + p77;
		// 146
		const float p79 = k[38] * s_r51 + p78;
		// 147
		const float p80 = sat((k[39] * r73 + p79) * 2.0f);
		const float r81 = w24(p80);   // r50
		// 148
		const float p82 = k[40] * s_r50;
		const float r83 = s_r50;   // r51
		// 149
		const float p84 = k[41] * s_r51 + p82;
		// 14a
		const float p85 = k[42] * s_r52 + p84;
		const float r86 = s_r52;   // r53
		// 14b
		const float p87 = k[43] * s_r53 + p85;
		// 14c
		const float p88 = sat((k[44] * r81 + p87) * 16.0f);
		const float r89 = w24(p88);   // r52
		// 14d
		const float p90 = t7 * m16 - m16;
		// 14e
		const float p91 = sat(t7 * m23 - p90);
		const float r92 = w24(p91);   // r54
		// 14f
		const float p93 = k[47] * s_r54;
		// 150
		const float p94 = k[48] * s_r55 + p93;
		const float m95 = lfo[21];   // m16
		// 151
		const float p96 = sat((k[49] * r92 + p94) * 2.0f);
		const float r97 = w24(p96);   // r55
		// 152
		const float p98 = t62 * m68 - m68;
		// 153
		const float p99 = sat(t62 * m75 - p98);
		const float r100 = w24(p99);   // r56
		// 154
		const float p101 = k[52] * s_r56;
		// 155
		const float p102 = k[53] * s_r57 + p101;
		// 156
		const float p103 = sat((k[54] * r100 + p102) * 2.0f);
		const float r104 = w24(p103);   // r57
		// 157
		const float p105 = k[55] * r54;
		// 158
		const float p106 = k[56] * r89 + p105;
		// 159
		const float p107 = k[57] * r97 + p106;
		// 15a
		const float p108 = sat(k[58] * r104 + p107);
		const float w109 = p108;
		// 15b
		const float p110 = k[59] * r89;
		// 15c
		const float p111 = k[60] * r97 + p110;
		ram[at(60)] = w109;
		// 15d
		const float p112 = sat(k[61] * r104 + p111);
		const float w113 = p112;
		// 15e
		const float p114 = t25 * m30 - m30;
		// 15f
		const float p115 = sat(t25 * m38 - p114);
		const float r116 = w24(p115);   // r05
		ram[at(63)] = w113;
		// 160
		const float p117 = t25 * m43 - m43;
		const float m118 = lfo[22];   // m17
		// 161
		const float p119 = sat(t25 * m50 - p117);
		const float r120 = w24(p119);   // r06
		// 162
		const float p121 = sat((k[66] + m35) * 2.0f);
		const float r122 = w24(p121);   // r03
		// 163
		const float p123 = sat((k[67] + m70) * 2.0f);
		const float r124 = w24(p123);   // r04
		// 164
		const float p125 = sat((k[68] * r59) * 2.0f);
		const float r126 = w24(p125);   // r01
		// 165
		const float p127 = sat((k[69] * r122) * 2.0f);
		const float r128 = w24(p127);   // r59
		// 166
		const float p129 = sat((k[70] * r124) * 2.0f);
		const float r130 = w24(p129);   // r5b
		const float t131 = tv_plain(p125);   // t1
		// 167
		const float p132 = k[71] * r92;
		const float t133 = tv_plain(p127);   // t2
		// 168
		const float p134 = k[72] * r116 + p132;
		const float t135 = tv_plain(p129);   // t3
		// 169
		const float p136 = sat(k[73] * r120 + p134);
		const float r137 = w24(p136);   // r07
		// 16a
		const float p138 = sat(t131 * r126);
		const float r139 = w24(p138);   // r02
		// 16b
		const float p140 = sat(t133 * r128);
		const float r141 = w24(p140);   // r03
		// 16c
		const float p142 = sat(t135 * r130);
		const float r143 = w24(p142);   // r04
		// 16d
		const float p144 = sat(t131 * r139);
		const float r145 = w24(p144);   // r02
		// 16e
		const float p146 = sat(t133 * r141);
		const float r147 = w24(p146);   // r5a
		// 16f
		const float p148 = sat(t135 * r143);
		const float r149 = w24(p148);   // r5c
		// 170
		const float p150 = sat((k[80] * r126) * 16.0f);
		const float m151 = lfo[23];   // m18
		// 171
		const float p152 = k[81] * r145 + p150;
		// 172
		const float p153 = satpos(k[82] + p152);
		const float r154 = w24(p153);   // r58
		// 173
		const float p155 = k[83] * r100;
		// 174
		const float p156 = k[84] * r120 + p155;
		// 175
		const float p157 = sat(k[85] * r116 + p156);
		const float r158 = w24(p157);   // r08
		// 176
		const float p159 = k[86] * r54;
		// 177
		const float p160 = k[87] * r89 + p159;
		// 178
		const float p161 = sat((k[88] * r137 + p160) * 4.0f);
		const float m162 = w24(p161);   // m2c
		// 179
		const float p163 = k[89] * r89;
		// 17a
		const float p164 = k[90] * r54 + p163;
		// 17b
		const float p165 = sat((k[91] * r158 + p164) * 4.0f);
		const float m166 = w24(p165);   // m2d
		// 17c
		// 17d
		// 17e
		// 17f
		out[0] = m162;   // m2c
		out[1] = m166;   // m2d
		s_p = p165;
		s_m16 = m95;
		s_r58 = r154;
		s_r5b = r130;
		s_r5c = r149;
		s_r59 = r128;
		s_r5a = r147;
		s_m18 = m151;
		s_r44 = r21;
		s_r45 = r24;
		s_r46 = r32;
		s_r47 = r36;
		s_r48 = r44;
		s_r49 = r46;
		s_r4a = r54;
		s_r4b = r51;
		s_m17 = m118;
		s_r4c = r61;
		s_r4d = r64;
		s_r4e = r73;
		s_r4f = r76;
		s_r50 = r81;
		s_r51 = r83;
		s_r52 = r89;
		s_r53 = r86;
		s_r54 = r92;
		s_r55 = r97;
		s_r56 = r100;
		s_r57 = r104;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_DELAY_H
