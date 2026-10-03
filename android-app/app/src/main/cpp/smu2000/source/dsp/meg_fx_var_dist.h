// license:BSD-3-Clause
// S-MU2000: バリエーション: DISTORTION, CMP+DT, OVERDRIVE, AMP SIM, HM ENHNCER, COMPRESSOR, NOISE GATE
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2d m2c / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_DIST_H
#define S_MU2000_DSP_MEG_FX_VAR_DIST_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_dist : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2d, 0x2c };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r61 = 0; s_r62 = 0; s_r63 = 0; }

	// in: 入口（m2d m2c）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat((k[0] * s_m16) * 16.0f);
		const float r2 = w24(p1);   // r01
		// 121
		const float p3 = sat((k[1] * in[0]) * 2.0f);
		const float m4 = in[0];   // m03
		const float r5 = w24(p3);   // r47
		// 122
		const float p6 = k[2] * s_r47;
		const float r7 = s_r47;   // r48
		// 123
		const float p8 = k[3] * s_r48 + p6;
		// 124
		const float p9 = k[4] * s_r49 + p8;
		const float r10 = s_r49;   // r4a
		// 125
		const float p11 = k[5] * s_r4a + p9;
		// 126
		const float p12 = sat((k[6] * r5 + p11) * 2.0f);
		const float r13 = w24(p12);   // r49
		// 127
		const float p14 = sat((k[7] * r2) * 16.0f);
		const float r15 = w24(p14);   // r01
		// 128
		const float p16 = k[8] * in[1];
		const float m17 = in[1];   // m02
		// 129
		const float p18 = sat((k[9] * in[0] + p16) * 2.0f);
		const float r19 = w24(p18);   // r57
		// 12a
		const float p20 = sat((k[10] * r15) * 16.0f);
		const float r21 = w24(p20);   // r01
		// 12b
		const float p22 = k[11] * s_r57;
		const float r23 = s_r57;   // r44
		// 12c
		const float p24 = k[12] * s_r44 + p22;
		// 12d
		const float p25 = k[13] * s_r45 + p24;
		const float r26 = s_r45;   // r46
		// 12e
		const float p27 = k[14] * s_r46 + p25;
		// 12f
		const float p28 = sat((k[15] * r19 + p27) * 2.0f);
		const float r29 = w24(p28);   // r45
		// 130
		const float p30 = sat((k[16] * r21) * 16.0f);
		const float r31 = w24(p30);   // r02
		// 131
		const float p32 = sat((k[17] * r13) * 2.0f);
		const float r33 = w24(p32);   // r59
		// 132
		const float p34 = sat((k[18] * r29) * 2.0f);
		const float r35 = w24(p34);   // r58
		// 133
		const float p36 = sat((k[19] * r31) * 16.0f);
		const float r37 = w24(p36);   // r03
		// 134
		const float p38 = k[20] * r33;
		// 135
		const float p39 = satabs((k[21] * r35 + p38) * 16.0f);
		const float m40 = w24(p39);   // m01
		// 136
		const float p41 = satpos(k[22] * s_r4b - p39);
		const float r42 = w24(p41);   // r5a
		// 137
		const float p43 = sat((k[23] * r37) * 16.0f);
		const float r44 = w24(p43);   // r04
		// 138
		const float p45 = k[24] * s_r61;
		const float r46 = s_r61;   // r62
		// 139
		const float p47 = k[25] * s_r62 + p45;
		// 13a
		const float p48 = sat(k[26] * s_r63 + p47);
		const float r49 = w24(p48);   // r63
		// 13b
		const float p50 = sat((k[27] * r44) * 16.0f);
		const float r51 = w24(p50);   // r05
		// 13c
		const float p52 = sat(m40 + r42);
		const float r53 = w24(p52);   // r4b
		const float t54 = tv_plain(p48);   // t1
		// 13d
		const float p55 = k[29] * s_r4c;
		// 13e
		const float p56 = k[30] * s_r4c + (p55 * (1.0f / 32768.0f));
		// 13f
		const float p57 = m40 - p56;
		// 140
		const float p58 = satpos(k[32] * r53 - p57);
		// 141
		const float p59 = sat(m40 + p58);
		const float r60 = w24(p59);   // r4c
		// 142
		const float p61 = sat((k[34] * r51) * 16.0f);
		const float r62 = w24(p61);   // r01
		// 143
		const float p63 = k[35] * r21;
		// 144
		const float p64 = k[36] * r31 + p63;
		// 145
		const float p65 = k[37] * r37 + p64;
		// 146
		const float p66 = k[38] * r44 + p65;
		// 147
		const float p67 = k[39] * r51 + p66;
		// 148
		const float p68 = sat(k[40] * r62 + p67);
		const float r69 = w24(p68);   // r01
		// 149
		const float p70 = sat(t54 * r35);
		const float r71 = w24(p70);   // r5d
		// 14a
		const float p72 = sat(t54 * r33);
		const float r73 = w24(p72);   // r5e
		// 14b
		const float p74 = sat(k[43] * r69);
		const float r75 = w24(p74);   // r4f
		const float t76 = tv_plain(p70);   // t2
		// 14c
		const float p77 = sat(k[44] * r60);
		const float r78 = w24(p77);   // r5c
		const float t79 = tv_plain(p72);   // t3
		// 14d
		const float p80 = k[45] * s_r4f;
		const float r81 = s_r4f;   // r50
		// 14e
		const float p82 = k[46] * s_r50 + p80;
		// 14f
		const float p83 = k[47] * s_r51 + p82;
		// 150
		const float p84 = k[48] * s_r52 + p83;
		// 151
		const float p85 = sat((k[49] * r75 + p84) * 2.0f);
		const float r86 = w24(p85);   // r51
		// 152
		const float p87 = k[50] * s_r51;
		const float r88 = s_r51;   // r52
		// 153
		const float p89 = k[51] * s_r52 + p87;
		// 154
		const float p90 = k[52] * s_r53 + p89;
		// 155
		const float p91 = k[53] * s_r54 + p90;
		// 156
		const float p92 = sat((k[54] * r86 + p91) * 2.0f);
		const float r93 = w24(p92);   // r53
		// 157
		const float p94 = k[55] * s_r53;
		const float r95 = s_r53;   // r54
		// 158
		const float p96 = k[56] * s_r54 + p94;
		// 159
		const float p97 = k[57] * s_r55 + p96;
		const float r98 = s_r55;   // r56
		// 15a
		const float p99 = k[58] * s_r56 + p97;
		// 15b
		const float p100 = sat((k[59] * r93 + p99) * 16.0f);
		const float r101 = w24(p100);   // r55
		// 15c
		const float p102 = sat(t76 * r35);
		const float r103 = w24(p102);   // r5f
		// 15d
		const float p104 = k[61] * r71;
		// 15e
		const float p105 = k[62] * r35 + p104;
		// 15f
		const float p106 = sat((k[63] * r103 + p105) * 16.0f);
		const float r107 = w24(p106);   // r4d
		// 160
		const float p108 = k[64] * s_r4d;
		// 161
		const float p109 = k[65] * s_m16 + p108;
		// 162
		const float p110 = sat((k[66] * r107 + p109) * 2.0f);
		const float m111 = w24(p110);   // m16
		// 163
		const float p112 = sat((k[67] * r101 + s_m18) * 4.0f);
		const float m113 = w24(p112);   // m2c
		// 164
		const float p114 = k[68] * r101 + s_m19;
		// 165
		const float p115 = sat((k[69] * s_m17 + p114) * 4.0f);
		const float m116 = w24(p115);   // m2d
		// 166
		const float p117 = sat(t79 * r33);
		const float r118 = w24(p117);   // r60
		// 167
		const float p119 = k[71] * r73;
		// 168
		const float p120 = k[72] * r33 + p119;
		// 169
		const float p121 = sat((k[73] * r118 + p120) * 16.0f);
		const float r122 = w24(p121);   // r4e
		// 16a
		const float p123 = k[74] * s_r4e;
		// 16b
		const float p124 = k[75] * s_m17 + p123;
		// 16c
		const float p125 = sat((k[76] * r122 + p124) * 2.0f);
		const float m126 = w24(p125);   // m17
		// 16d
		const float p127 = satpos(k[77] + r78);
		const float r128 = w24(p127);   // r01
		// 16e
		const float p129 = satpos(k[78] + r78);
		const float r130 = w24(p129);   // r02
		// 16f
		const float p131 = satpos(k[79] + r78);
		const float r132 = w24(p131);   // r03
		// 170
		const float p133 = satpos(k[80] + r78);
		const float r134 = w24(p133);   // r04
		// 171
		const float p135 = satpos(k[81] + r78);
		const float r136 = w24(p135);   // r05
		// 172
		const float p137 = satpos(k[82] + r78);
		const float r138 = w24(p137);   // r06
		// 173
		const float p139 = satpos(k[83] + r78);
		const float r140 = w24(p139);   // r5d
		// 174
		const float p141 = satpos(k[84] + r78);
		const float r142 = w24(p141);   // r5e
		// 175
		const float p143 = (k[85] * r128) * 2.0f;
		// 176
		const float p144 = (k[86] * r132 + p143) * 2.0f;
		// 177
		const float p145 = (k[87] * r134 + p144) * 2.0f;
		// 178
		const float p146 = (k[88] * r130 + p145) * 2.0f;
		// 179
		const float p147 = (k[89] * r136 + p146) * 2.0f;
		// 17a
		const float p148 = (k[90] * r138 + p147) * 4.0f;
		// 17b
		const float p149 = k[91] * r140 + p148;
		// 17c
		const float p150 = k[92] * r142 + p149;
		// 17d
		const float p151 = satpos(k[93] + p150);
		const float r152 = w24(p151);   // r61
		// 17e
		const float p153 = sat(k[94] * m17);
		const float m154 = w24(p153);   // m18
		// 17f
		const float p155 = sat(k[95] * m4);
		const float m156 = w24(p155);   // m19
		out[0] = m113;   // m2c
		out[1] = m116;   // m2d
		s_p = p155;
		s_m16 = m111;
		s_r47 = r5;
		s_r48 = r7;
		s_r49 = r13;
		s_r4a = r10;
		s_r57 = r19;
		s_r44 = r23;
		s_r45 = r29;
		s_r46 = r26;
		s_r4b = r53;
		s_r61 = r152;
		s_r62 = r46;
		s_r63 = r49;
		s_r4c = r60;
		s_r4f = r75;
		s_r50 = r81;
		s_r51 = r86;
		s_r52 = r88;
		s_r53 = r93;
		s_r54 = r95;
		s_r55 = r101;
		s_r56 = r98;
		s_r4d = r107;
		s_m18 = m154;
		s_m19 = m156;
		s_m17 = m126;
		s_r4e = r122;
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
	float s_r61 = 0.0f;
	float s_r62 = 0.0f;
	float s_r63 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_DIST_H
