// license:BSD-3-Clause
// S-MU2000: バリエーション: D.TURNTBL
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_TURNTABLE_H
#define S_MU2000_DSP_MEG_FX_VAR_TURNTABLE_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_turntable : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x7c0000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; s_r5d = 0; s_r5e = 0; s_r5f = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * in[0];
		const float m2 = lfo[18];   // m03
		const float r3 = w24(p1);   // r44
		const float q4 = ram[at(0)];
		// 121
		const float p5 = k[1] * s_r44;
		// 122
		const float p6 = k[2] * s_r45 + p5;
		const float m7 = q4;   // m01
		// 123
		const float p8 = k[3] * s_r46 + p6;
		const float r9 = s_r46;   // r47
		// 124
		const float p10 = k[4] * s_r47 + p8;
		// 125
		const float p11 = sat((k[5] * r3 + p10) * 2.0f);
		const float r12 = w24(p11);   // r46
		// 126
		const float p13 = k[6];
		// 127
		const float p14 = (k[7] * s_r5f + p13) * 2.0f;
		const float r15 = w24(p14);   // r01
		// 128
		const float p16 = k[8];
		// 129
		const float p17 = (k[9] * m7 + p16) * 2.0f;
		const float r18 = w24(p17);   // r02
		const float w19 = p17;
		// 12a
		const float p20 = k[10] * r3;
		const float r21 = r3;   // r48
		// 12b
		const float p22 = k[11] * s_r48 + p20;
		// 12c
		const float p23 = sat((k[12] * s_r49 + p22) * 2.0f);
		const float r24 = w24(p23);   // r49
		ram[at(12)] = w19;
		// 12d
		const float p25 = k[13] * in[1];
		const float r26 = w24(p25);   // r4e
		// 12e
		const float p27 = k[14] * s_r4e;
		// 12f
		const float p28 = k[15] * s_r4f + p27;
		// 130
		const float p29 = k[16] * s_r50 + p28;
		const float m30 = lfo[19];   // m04
		const float r31 = s_r50;   // r51
		// 131
		const float p32 = k[17] * s_r51 + p29;
		// 132
		const float p33 = sat((k[18] * r26 + p32) * 2.0f);
		const float r34 = w24(p33);   // r50
		// 133
		const float p35 = k[19];
		// 134
		const float p36 = (k[20] * s_r5e + p35) * 16.0f;
		const float r37 = w24(p36);   // r5e
		// 135
		const float p38 = k[21] * r26;
		const float r39 = r26;   // r52
		// 136
		const float p40 = k[22] * s_r52 + p38;
		// 137
		const float p41 = sat((k[23] * s_r53 + p40) * 2.0f);
		const float r42 = w24(p41);   // r53
		// 138
		const float p43 = k[24] * r37;
		const float r44 = w24(p43);   // r08
		// 139
		// 13a
		// 13b
		// 13c
		// 13d
		// 13e
		const float p45 = k[30];
		// 13f
		const float p46 = sat(k[31] * m2 + p45);
		// 140
		const float p47 = k[32] * r15;
		const float m48 = lfo[20];   // m03
		// 141
		const float p49 = k[33] * r18 + p47;
		const float r50 = w24(p49);   // r58
		const float t51 = tv_plain(p46);   // t2
		// 142
		const float p52 = k[34] * s_r58;
		// 143
		const float p53 = k[35] * s_r59 + p52;
		// 144
		const float p54 = sat((k[36] * r50 + p53) * 2.0f);
		const float r55 = w24(p54);   // r59
		const float q56 = ram[at(36)];
		// 145
		const float p57 = t51 * r44;
		const float r58 = w24(p57);   // r03
		// 146
		const float p59 = k[38];
		const float m60 = q56;   // m01
		// 147
		const float p61 = sat(k[39] * m30 + p59);
		const float q62 = ram[at(39)];
		// 148
		const float p63 = k[40] * m48;
		// 149
		const float p64 = sat((k[41] * r12 + p63) * 16.0f);
		const float m65 = q62;   // m02
		const float r66 = w24(p64);   // r04
		const float t67 = tv_plain(p61);   // t2
		// 14a
		const float p68 = k[42] * m48;
		const float q69 = ram[at(42)];
		// 14b
		const float p70 = sat((k[43] * r34 + p68) * 16.0f);
		const float r71 = w24(p70);   // r05
		// 14c
		const float p72 = t67 * r58;
		const float m73 = q69;   // m18
		const float w74 = p72;
		// 14d
		const float p75 = k[45];
		const float q76 = ram[at(45)];
		// 14e
		const float p77 = sat(k[46] * r66 + p75);
		const int32_t i78 = idx_of(p77);
		// 14f
		const float m79 = q76;   // m19
		// 150
		const float p80 = k[48] * r24;
		const float m81 = lfo[21];   // m06
		const float t82 = tv_index(p77);   // t3
		ram[at(48)] = w74;
		// 151
		const float p83 = k[49] * m60 + p80;
		// 152
		const float p84 = k[50] * m73 + p83;
		// 153
		const float p85 = k[51] * s_r4c + p84;
		const float q86 = tab(51, i78);
		// 154
		const float p87 = sat((k[52] * in[0] + p85) * 4.0f);
		const float r88 = w24(p87);   // r06
		// 155
		const float p89 = k[53] * r42;
		const float m90 = q86;   // m03
		// 156
		const float p91 = k[54] * m65 + p89;
		const float q92 = tab(54, i78 + 1);
		// 157
		const float p93 = k[55] * m79 + p91;
		// 158
		const float p94 = k[56] * s_r56 + p93;
		const float m95 = q92;   // m04
		// 159
		const float p96 = sat((k[57] * in[1] + p94) * 4.0f);
		const float r97 = w24(p96);   // r07
		// 15a
		const float p98 = t82 * m90 - m90;
		// 15b
		const float p99 = t82 * m95 - p98;
		const float r100 = w24(p99);   // r04
		// 15c
		const float p101 = k[60];
		// 15d
		const float p102 = sat(k[61] * r71 + p101);
		const int32_t i103 = idx_of(p102);
		// 15e
		const float p104 = sat(k[62] * r88);
		const float m105 = w24(p104);   // m2c
		// 15f
		const float p106 = sat(k[63] * r97);
		const float m107 = w24(p106);   // m2d
		const float t108 = tv_index(p102);   // t3
		// 160
		const float p109 = k[64] * r100;
		const float m110 = lfo[22];   // m05
		const float r111 = w24(p109);   // r4a
		// 161
		const float p112 = k[65] * s_r4a;
		const float r113 = s_r4a;   // r4b
		// 162
		const float p114 = k[66] * s_r4b + p112;
		const float q115 = tab(66, i103);
		// 163
		const float p116 = k[67] * s_r4c + p114;
		const float r117 = s_r4c;   // r4d
		// 164
		const float p118 = k[68] * s_r4d + p116;
		const float m119 = q115;   // m03
		// 165
		const float p120 = sat((k[69] * r111 + p118) * 2.0f);
		const float r121 = w24(p120);   // r4c
		const float q122 = tab(69, i103 + 1);
		// 166
		const float p123 = k[70];
		// 167
		const float p124 = sat(k[71] * m81 + p123);
		const float m125 = q122;   // m04
		// 168
		const float p126 = k[72];
		// 169
		const float p127 = sat(k[73] * m110 + p126);
		const float t128 = tv_plain(p124);   // t2
		// 16a
		const float p129 = t108 * m119 - m119;
		// 16b
		const float p130 = t108 * m125 - p129;
		const float r131 = w24(p130);   // r05
		const float t132 = tv_plain(p127);   // t3
		// 16c
		const float p133 = t128 * r55;
		const float w134 = p133;
		// 16d
		// 16e
		const float p135 = t132 * r55;
		const float w136 = p135;
		ram[at(78)] = w134;
		// 16f
		const float p137 = k[79] * r131;
		const float r138 = w24(p137);   // r54
		// 170
		const float p139 = k[80] * s_r54;
		const float r140 = s_r54;   // r55
		// 171
		const float p141 = k[81] * s_r55 + p139;
		ram[at(81)] = w136;
		// 172
		const float p142 = k[82] * s_r56 + p141;
		const float r143 = s_r56;   // r57
		// 173
		const float p144 = k[83] * s_r57 + p142;
		// 174
		const float p145 = sat((k[84] * r138 + p144) * 2.0f);
		const float r146 = w24(p145);   // r56
		// 175
		// 176
		// 177
		const float p147 = (k[87] * r15) * 2.0f;
		const float r148 = w24(p147);   // r5a
		// 178
		const float p149 = (k[88] * s_r5a) * 2.0f;
		const float r150 = w24(p149);   // r5b
		// 179
		const float p151 = (k[89] * s_r5b) * 2.0f;
		const float r152 = w24(p151);   // r5c
		// 17a
		const float p153 = (k[90] * s_r5c) * 2.0f;
		const float r154 = w24(p153);   // r5d
		// 17b
		const float p155 = (k[91] * s_r5d) * 2.0f;
		const float r156 = w24(p155);   // r5f
		// 17c
		// 17d
		const float p157 = (k[93] * m73) * 2.0f;
		const float r158 = w24(p157);   // r45
		// 17e
		const float p159 = (k[94] * m79) * 2.0f;
		const float r160 = w24(p159);   // r4f
		// 17f
		out[0] = m105;   // m2c
		out[1] = m107;   // m2d
		s_p = p159;
		s_r44 = r3;
		s_r45 = r158;
		s_r46 = r12;
		s_r47 = r9;
		s_r5f = r156;
		s_r48 = r21;
		s_r49 = r24;
		s_r4e = r26;
		s_r4f = r160;
		s_r50 = r34;
		s_r51 = r31;
		s_r5e = r37;
		s_r52 = r39;
		s_r53 = r42;
		s_r58 = r50;
		s_r59 = r55;
		s_r4c = r121;
		s_r56 = r146;
		s_r4a = r111;
		s_r4b = r113;
		s_r4d = r117;
		s_r54 = r138;
		s_r55 = r140;
		s_r57 = r143;
		s_r5a = r148;
		s_r5b = r150;
		s_r5c = r152;
		s_r5d = r154;
	}

private:
	float s_m00 = 0.0f;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_TURNTABLE_H
