// license:BSD-3-Clause
// S-MU2000: バリエーション: AUTO PAN 2
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_AUTOPAN2_H
#define S_MU2000_DSP_MEG_FX_VAR_AUTOPAN2_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_autopan2 : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; s_r5d = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * s_m18;
		const int32_t i2 = idx_of(p1);
		// 121
		const float p3 = sat((k[1] * in[0]) * 2.0f);
		const float r4 = w24(p3);   // r4e
		// 122
		const float p5 = k[2] * s_r4e;
		const float r6 = s_r4e;   // r4f
		const float t7 = tv_index(p1);   // t0
		// 123
		const float p8 = k[3] * s_r4f + p5;
		const float q9 = ram[at(3, i2)];
		// 124
		const float p10 = k[4] * s_r50 + p8;
		// 125
		const float p11 = k[5] * s_r51 + p10;
		const float m12 = q9;   // m05
		// 126
		const float p13 = sat((k[6] * r4 + p11) * 2.0f);
		const float r14 = w24(p13);   // r50
		const float q15 = ram[at(6, i2 + 1)];
		// 127
		const float p16 = k[7] * s_r50;
		const float r17 = s_r50;   // r51
		// 128
		const float p18 = k[8] * s_r51 + p16;
		const float m19 = q15;   // m06
		// 129
		const float p20 = k[9] * s_r52 + p18;
		// 12a
		const float p21 = k[10] * s_r53 + p20;
		// 12b
		const float p22 = sat((k[11] * r14 + p21) * 4.0f);
		const float r23 = w24(p22);   // r52
		// 12c
		const float p24 = k[12] * s_r52;
		const float r25 = s_r52;   // r53
		// 12d
		const float p26 = k[13] * s_r53 + p24;
		// 12e
		const float p27 = k[14] * s_r54 + p26;
		const float r28 = s_r54;   // r55
		// 12f
		const float p29 = k[15] * s_r55 + p27;
		// 130
		const float p30 = sat((k[16] * r23 + p29) * 16.0f);
		const float r31 = w24(p30);   // r54
		// 131
		// 132
		const float p32 = k[18] * s_m19;
		const int32_t i33 = idx_of(p32);
		// 133
		const float p34 = sat((k[19] * in[1]) * 2.0f);
		const float r35 = w24(p34);   // r56
		// 134
		const float p36 = k[20] * s_r56;
		const float r37 = s_r56;   // r57
		const float t38 = tv_index(p32);   // t1
		// 135
		const float p39 = k[21] * s_r57 + p36;
		const float q40 = ram[at(21, i33)];
		// 136
		const float p41 = k[22] * s_r58 + p39;
		// 137
		const float p42 = k[23] * s_r59 + p41;
		const float m43 = q40;   // m03
		// 138
		const float p44 = sat((k[24] * r35 + p42) * 2.0f);
		const float r45 = w24(p44);   // r58
		const float q46 = ram[at(24, i33 + 1)];
		// 139
		const float p47 = k[25] * s_r58;
		const float r48 = s_r58;   // r59
		// 13a
		const float p49 = k[26] * s_r59 + p47;
		const float m50 = q46;   // m04
		// 13b
		const float p51 = k[27] * s_r5a + p49;
		// 13c
		const float p52 = k[28] * s_r5b + p51;
		// 13d
		const float p53 = sat((k[29] * r45 + p52) * 4.0f);
		const float r54 = w24(p53);   // r5a
		// 13e
		const float p55 = k[30] * s_r5a;
		const float r56 = s_r5a;   // r5b
		// 13f
		const float p57 = k[31] * s_r5b + p55;
		// 140
		const float p58 = k[32] * s_r5c + p57;
		const float r59 = s_r5c;   // r5d
		// 141
		const float p60 = k[33] * s_r5d + p58;
		// 142
		const float p61 = sat((k[34] * r54 + p60) * 16.0f);
		const float r62 = w24(p61);   // r5c
		// 143
		const float p63 = k[35];
		// 144
		const float p64 = s_r44 - p63;
		const float r65 = w24(p64);   // r01
		// 145
		const float p66 = t7 * m12 - m12;
		// 146
		const float p67 = sat(t7 * m19 - p66);
		const float r68 = w24(p67);   // r08
		// 147
		const float p69 = k[39] * r65;
		const int32_t i70 = idx_of(p69);
		const float t71 = k[39];   // t2（定数）
		// 148
		// 149
		const float p72 = k[41];
		const float t73 = tv_index(p69);   // t0
		// 14a
		const float p74 = s_r44 - p72;
		const float r75 = w24(p74);   // r01
		const float q76 = tab(42, i70);
		// 14b
		const float p77 = t38 * m43 - m43;
		// 14c
		const float p78 = sat(t38 * m50 - p77);
		const float m79 = q76;   // m01
		const float r80 = w24(p78);   // r09
		// 14d
		const float p81 = t71 * r75;
		const int32_t i82 = idx_of(p81);
		const float q83 = tab(45, i70 + 1);
		// 14e
		// 14f
		const float p84 = k[47];
		const float m85 = q83;   // m02
		const float t86 = tv_index(p81);   // t1
		// 150
		const float p87 = s_r44 - p84;
		const float r88 = w24(p87);   // r01
		const float q89 = tab(48, i82);
		// 151
		const float p90 = t73 * m79 - m79;
		// 152
		const float p91 = sat(t73 * m85 - p90);
		const float m92 = q89;   // m01
		const float r93 = w24(p91);   // r45
		// 153
		const float p94 = t71 * r88;
		const int32_t i95 = idx_of(p94);
		const float q96 = tab(51, i82 + 1);
		// 154
		// 155
		const float p97 = k[53];
		const float m98 = q96;   // m02
		const float t99 = tv_index(p94);   // t0
		// 156
		const float p100 = s_r44 - p97;
		const float r101 = w24(p100);   // r01
		const float q102 = tab(54, i95);
		// 157
		const float p103 = t86 * m92 - m92;
		// 158
		const float p104 = sat(t86 * m98 - p103);
		const float m105 = q102;   // m01
		const float r106 = w24(p104);   // r46
		// 159
		const float p107 = t71 * r101;
		const int32_t i108 = idx_of(p107);
		const float q109 = tab(57, i95 + 1);
		// 15a
		const float p110 = k[58];
		// 15b
		const float p111 = sat(k[59] + (p110 * (1.0f / 32768.0f)));
		const float m112 = q109;   // m02
		const float t113 = tv_index(p107);   // t1
		// 15c
		const float p114 = s_r44 + p111;
		const float r115 = w24(p114);   // r44
		const float q116 = tab(60, i108);
		// 15d
		const float p117 = t99 * m105 - m105;
		// 15e
		const float p118 = sat(t99 * m112 - p117);
		const float m119 = q116;   // m01
		const float r120 = w24(p118);   // r47
		// 15f
		const float p121 = s_m16;
		const float q122 = tab(63, i108 + 1);
		// 160
		const float p123 = s_m17;
		// 161
		const float p124 = k[65] * r62;
		const float m125 = q122;   // m02
		const float t126 = tv_plain(p121);   // t2
		// 162
		const float p127 = sat(k[66] * r31 + p124);
		const float w128 = p127;
		const float t129 = tv_plain(p123);   // t7
		// 163
		const float p130 = t113 * m119 - m119;
		// 164
		const float p131 = sat(t113 * m125 - p130);
		const float r132 = w24(p131);   // r48
		// 165
		const float p133 = k[69] * r93;
		const float t134 = k[69];   // t0（定数）
		ram[at(69)] = w128;
		// 166
		const float p135 = sat(k[70] * r120 + p133);
		const float r136 = w24(p135);   // r02
		const float t137 = k[70];   // t1（定数）
		// 167
		const float p138 = t134 * r106;
		// 168
		const float p139 = sat(t137 * r132 + p138);
		const float r140 = w24(p139);   // r03
		// 169
		const float p141 = (k[73] * r136) * 2.0f;
		// 16a
		const float p142 = satpos(k[74] + p141);
		const float r143 = w24(p142);   // r02
		// 16b
		const float p144 = (k[75] * r140) * 2.0f;
		// 16c
		const float p145 = satpos(k[76] + p144);
		const float r146 = w24(p145);   // r03
		// 16d
		// 16e
		const float p147 = satpos((k[78] * r143) * 16.0f);
		const float r148 = w24(p147);   // r02
		const float t149 = k[78];   // t0（定数）
		// 16f
		const float p150 = satpos((t149 * r146) * 16.0f);
		const float r151 = w24(p150);   // r03
		// 170
		// 171
		const float p152 = satpos((t149 * r148) * 16.0f);
		const float m153 = w24(p152);   // m18
		// 172
		const float p154 = satpos((t149 * r151) * 16.0f);
		const float m155 = w24(p154);   // m19
		// 173
		const float p156 = sat(k[83] * r62);
		const float w157 = p156;
		// 174
		const float p158 = k[84] * m153;
		const float m159 = w24(p158);   // m16
		// 175
		const float p160 = k[85] * m155;
		const float m161 = w24(p160);   // m17
		// 176
		// 177
		ram[at(87)] = w157;
		// 178
		const float p162 = sat(t126 * r68 + r68);
		const float r163 = w24(p162);   // r08
		// 179
		const float p164 = sat(t129 * r80 + r80);
		const float r165 = w24(p164);   // r09
		// 17a
		// 17b
		const float p166 = k[91] * r163;
		// 17c
		const float p167 = sat((k[92] * r31 + p166) * 4.0f);
		const float m168 = w24(p167);   // m2c
		// 17d
		const float p169 = k[93] * r165;
		// 17e
		const float p170 = sat((k[94] * r62 + p169) * 4.0f);
		const float m171 = w24(p170);   // m2d
		// 17f
		out[0] = m168;   // m2c
		out[1] = m171;   // m2d
		s_p = p170;
		s_m18 = m153;
		s_r4e = r4;
		s_r4f = r6;
		s_r50 = r14;
		s_r51 = r17;
		s_r52 = r23;
		s_r53 = r25;
		s_r54 = r31;
		s_r55 = r28;
		s_m19 = m155;
		s_r56 = r35;
		s_r57 = r37;
		s_r58 = r45;
		s_r59 = r48;
		s_r5a = r54;
		s_r5b = r56;
		s_r5c = r62;
		s_r5d = r59;
		s_r44 = r115;
		s_m16 = m159;
		s_m17 = m161;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_AUTOPAN2_H
