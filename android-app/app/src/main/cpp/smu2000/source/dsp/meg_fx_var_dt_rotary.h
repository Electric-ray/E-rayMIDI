// license:BSD-3-Clause
// S-MU2000: バリエーション: DT +RTRY, OD +RTRY, AMP+RTRY, DT +2RTRY, OD +2RTRY, AMP+2RTRY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_DT_ROTARY_H
#define S_MU2000_DSP_MEG_FX_VAR_DT_ROTARY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_dt_rotary : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x3c0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat(k[0] * s_m16);
		const float m2 = lfo[18];   // m16
		const int32_t i3 = idx_of(p1);
		// 121
		const float p4 = sat(k[1] * s_m17);
		const int32_t j5 = idx_of(p4);
		// 122
		const float p6 = k[2] * in[0];
		const float t7 = tv_index(p1);   // t1
		// 123
		const float p8 = sat(k[3] * in[1] + p6);
		const float r9 = w24(p8);   // r44
		const float t10 = tv_index(p4);   // t2
		const float q11 = ram[at(3, i3)];
		// 124
		const float p12 = k[4] * s_r44;
		// 125
		const float p13 = k[5] * s_r45 + p12;
		const float m14 = q11;   // m01
		// 126
		const float p15 = sat((k[6] * r9 + p13) * 2.0f);
		const float r16 = w24(p15);   // r45
		const float q17 = ram[at(6, i3 + 1)];
		// 127
		const float p18 = sat(k[7] * s_r45);
		// 128
		const float p19 = k[8] * s_r46 + p18;
		const float m20 = q17;   // m02
		// 129
		const float p21 = sat((k[9] * r16 + p19) * 2.0f);
		const float r22 = w24(p21);   // r46
		const float q23 = ram[at(9, j5)];
		// 12a
		const float p24 = t7 * m14 - m14;
		// 12b
		const float p25 = sat(t7 * m20 - p24);
		const float m26 = q23;   // m03
		const float r27 = w24(p25);   // r07
		// 12c
		const float p28 = sat(k[12] * s_m18);
		const int32_t i29 = idx_of(p28);
		const float q30 = ram[at(12, j5 + 1)];
		// 12d
		const float p31 = sat(k[13] * s_m19);
		const int32_t j32 = idx_of(p31);
		// 12e
		const float p33 = sat(k[14] * m2);
		const float m34 = q30;   // m04
		const float t35 = tv_index(p28);   // t3
		// 12f
		const float p36 = sat((k[15] * r22) * 16.0f);
		const float r37 = w24(p36);   // r01
		const float t38 = tv_index(p31);   // t7
		const float q39 = ram[at(15, i29)];
		// 130
		const float m40 = lfo[19];   // m17
		const float t41 = tv_plain(p33);   // t1
		// 131
		const float p42 = t10 * m26 - m26;
		const float m43 = q39;   // m05
		// 132
		const float p44 = sat(t10 * m34 - p42);
		const float r45 = w24(p44);   // r08
		const float q46 = ram[at(18, i29 + 1)];
		// 133
		const float p47 = sat((k[19] * r37) * 16.0f);
		const float r48 = w24(p47);   // r01
		// 134
		const float p49 = k[20] * s_r50;
		const float m50 = q46;   // m06
		// 135
		const float p51 = k[21] * s_r4f + p49;
		const float r52 = s_r4f;   // r50
		const float q53 = ram[at(21, j32)];
		// 136
		const float p54 = k[22] * s_r52 + p51;
		// 137
		const float p55 = k[23] * s_r51 + p54;
		const float m56 = q53;   // m07
		// 138
		const float p57 = sat((k[24] * s_r5b + p55) * 2.0f);
		const float r58 = w24(p57);   // r51
		const float q59 = ram[at(24, j32 + 1)];
		// 139
		const float p60 = sat((k[25] * r48) * 16.0f);
		const float r61 = w24(p60);   // r01
		// 13a
		const float p62 = k[26] * s_r51;
		const float m63 = q59;   // m08
		const float r64 = s_r51;   // r52
		// 13b
		const float p65 = k[27] * s_r52 + p62;
		// 13c
		const float p66 = k[28] * s_r54 + p65;
		// 13d
		const float p67 = k[29] * s_r53 + p66;
		const float r68 = s_r53;   // r54
		// 13e
		const float p69 = sat((k[30] * r58 + p67) * 16.0f);
		const float r70 = w24(p69);   // r53
		// 13f
		const float p71 = sat((k[31] * r61) * 16.0f);
		const float r72 = w24(p71);   // r02
		// 140
		const float p73 = t35 * m43 - m43;
		const float m74 = lfo[20];   // m18
		// 141
		const float p75 = sat(t35 * m50 - p73);
		const float r76 = w24(p75);   // r09
		// 142
		const float p77 = sat((k[34] * r72) * 16.0f);
		const float r78 = w24(p77);   // r03
		// 143
		const float p79 = t38 * m56 - m56;
		// 144
		const float p80 = sat(t38 * m63 - p79);
		const float m81 = w24(p80);   // m09
		// 145
		const float p82 = sat((k[37] * r78) * 16.0f);
		const float r83 = w24(p82);   // r04
		// 146
		const float p84 = k[38] * s_r56;
		// 147
		const float p85 = k[39] * s_r55 + p84;
		const float r86 = s_r55;   // r56
		// 148
		const float p87 = k[40] * s_r58 + p85;
		// 149
		const float p88 = k[41] * s_r57 + p87;
		// 14a
		const float p89 = sat((k[42] * s_r5c + p88) * 2.0f);
		const float r90 = w24(p89);   // r57
		// 14b
		const float p91 = sat((k[43] * r83) * 16.0f);
		const float r92 = w24(p91);   // r05
		// 14c
		const float p93 = k[44] * s_r57;
		const float r94 = s_r57;   // r58
		// 14d
		const float p95 = k[45] * s_r58 + p93;
		// 14e
		const float p96 = k[46] * s_r5a + p95;
		// 14f
		const float p97 = k[47] * s_r59 + p96;
		const float r98 = s_r59;   // r5a
		// 150
		const float p99 = sat((k[48] * r90 + p97) * 16.0f);
		const float m100 = lfo[21];   // m19
		const float r101 = w24(p99);   // r59
		// 151
		const float p102 = sat((k[49] * r92) * 16.0f);
		const float r103 = w24(p102);   // r06
		// 152
		const float p104 = sat(k[50] * m40);
		// 153
		const float p105 = sat(k[51] * m74);
		// 154
		const float p106 = k[52] * r70;
		const float t107 = tv_plain(p104);   // t2
		// 155
		const float p108 = sat(k[53] * r101 + p106);
		const float w109 = p108;
		const float t110 = tv_plain(p105);   // t3
		// 156
		const float p111 = k[54] * r61;
		// 157
		const float p112 = k[55] * r72 + p111;
		// 158
		const float p113 = k[56] * r78 + p112;
		// 159
		const float p114 = k[57] * r83 + p113;
		ram[at(57)] = w109;
		// 15a
		const float p115 = k[58] * r92 + p114;
		// 15b
		const float p116 = sat(k[59] * r103 + p115);
		const float r117 = w24(p116);   // r01
		// 15c
		const float p118 = sat(k[60] * r101);
		const float w119 = p118;
		// 15d
		const float p120 = sat(k[61] * m100);
		// 15e
		const float p121 = sat(k[62] * r117);
		const float r122 = w24(p121);   // r47
		// 15f
		const float p123 = k[63] * s_r48;
		const float t124 = tv_plain(p120);   // t7
		ram[at(63)] = w119;
		// 160
		const float p125 = k[64] * s_r47 + p123;
		const float r126 = s_r47;   // r48
		// 161
		const float p127 = k[65] * s_r4a + p125;
		// 162
		const float p128 = k[66] * s_r49 + p127;
		// 163
		const float p129 = sat((k[67] * r122 + p128) * 2.0f);
		const float r130 = w24(p129);   // r49
		// 164
		const float p131 = k[68] * s_r49;
		const float r132 = s_r49;   // r4a
		// 165
		const float p133 = k[69] * s_r4a + p131;
		// 166
		const float p134 = k[70] * s_r4c + p133;
		// 167
		const float p135 = k[71] * s_r4b + p134;
		// 168
		const float p136 = sat((k[72] * r130 + p135) * 2.0f);
		const float r137 = w24(p136);   // r4b
		// 169
		const float p138 = k[73] * s_r4b;
		const float r139 = s_r4b;   // r4c
		// 16a
		const float p140 = k[74] * s_r4c + p138;
		// 16b
		const float p141 = k[75] * s_r4d + p140;
		const float r142 = s_r4d;   // r4e
		// 16c
		const float p143 = k[76] * s_r4e + p141;
		// 16d
		const float p144 = sat((k[77] * r137 + p143) * 16.0f);
		const float r145 = w24(p144);   // r4d
		// 16e
		const float p146 = t41 * r27 + r27;
		const float r147 = w24(p146);   // r01
		// 16f
		const float p148 = t107 * r45 + r45;
		const float r149 = w24(p148);   // r02
		// 170
		const float p150 = t110 * r76 + r76;
		const float r151 = w24(p150);   // r03
		// 171
		const float p152 = t124 * m81 + m81;
		const float r153 = w24(p152);   // r04
		// 172
		const float p154 = s_r5b;
		const float r155 = w24(p154);   // r4f
		// 173
		const float p156 = s_r5c;
		const float r157 = w24(p156);   // r55
		// 174
		const float p158 = k[84] * in[0];
		// 175
		const float p159 = sat((k[85] * r145 + p158) * 4.0f);
		const float r160 = w24(p159);   // r5b
		// 176
		const float p161 = k[86] * in[1];
		// 177
		const float p162 = sat((k[87] * r145 + p161) * 4.0f);
		const float r163 = w24(p162);   // r5c
		// 178
		const float p164 = k[88] * r147;
		// 179
		const float p165 = sat(k[89] * r151 + p164);
		// 17a
		const float p166 = sat(k[90] * r153 + p165);
		// 17b
		const float p167 = sat((k[91] * r70 + p166) * 4.0f);
		const float m168 = w24(p167);   // m2c
		// 17c
		const float p169 = k[92] * r149;
		// 17d
		const float p170 = sat(k[93] * r153 + p169);
		// 17e
		const float p171 = sat(k[94] * r151 + p170);
		// 17f
		const float p172 = sat((k[95] * r101 + p171) * 4.0f);
		const float m173 = w24(p172);   // m2d
		out[0] = m168;   // m2c
		out[1] = m173;   // m2d
		s_p = p172;
		s_m16 = m2;
		s_m17 = m40;
		s_r44 = r9;
		s_r45 = r16;
		s_r46 = r22;
		s_m18 = m74;
		s_m19 = m100;
		s_r50 = r52;
		s_r4f = r155;
		s_r52 = r64;
		s_r51 = r58;
		s_r5b = r160;
		s_r54 = r68;
		s_r53 = r70;
		s_r56 = r86;
		s_r55 = r157;
		s_r58 = r94;
		s_r57 = r90;
		s_r5c = r163;
		s_r5a = r98;
		s_r59 = r101;
		s_r48 = r126;
		s_r47 = r122;
		s_r4a = r132;
		s_r49 = r130;
		s_r4c = r139;
		s_r4b = r137;
		s_r4d = r145;
		s_r4e = r142;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_DT_ROTARY_H
