// license:BSD-3-Clause
// S-MU2000: バリエーション: STREO DT, STREO OD, STREO AMP
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_STEREO_DIST_H
#define S_MU2000_DSP_MEG_FX_VAR_STEREO_DIST_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_stereo_dist : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * in[0];
		// 121
		const float p2 = sat(k[1] * in[1] + p1);
		const float r3 = w24(p2);   // r44
		// 122
		const float p4 = k[2] * s_r44;
		// 123
		const float p5 = k[3] * s_r45 + p4;
		// 124
		const float p6 = sat((k[4] * r3 + p5) * 2.0f);
		const float r7 = w24(p6);   // r45
		// 125
		const float p8 = sat(k[5] * s_r45);
		// 126
		const float p9 = k[6] * s_r46 + p8;
		// 127
		const float p10 = sat((k[7] * r7 + p9) * 2.0f);
		const float r11 = w24(p10);   // r46
		// 128
		const float p12 = k[8] * in[1];
		// 129
		const float p13 = sat(k[9] * in[0] + p12);
		const float r14 = w24(p13);   // r4f
		// 12a
		const float p15 = k[10] * s_r4f;
		// 12b
		const float p16 = k[11] * s_r50 + p15;
		// 12c
		const float p17 = sat((k[12] * r14 + p16) * 2.0f);
		const float r18 = w24(p17);   // r50
		// 12d
		const float p19 = sat(k[13] * s_r50);
		// 12e
		const float p20 = k[14] * s_r51 + p19;
		// 12f
		const float p21 = sat((k[15] * r18 + p20) * 2.0f);
		const float r22 = w24(p21);   // r51
		// 130
		// 131
		const float p23 = sat((k[17] * r11) * 16.0f);
		const float r24 = w24(p23);   // r01
		// 132
		const float p25 = sat((k[18] * r22) * 16.0f);
		const float r26 = w24(p25);   // r02
		// 133
		// 134
		const float p27 = sat((k[20] * r24) * 16.0f);
		const float r28 = w24(p27);   // r01
		// 135
		const float p29 = sat((k[21] * r26) * 16.0f);
		const float r30 = w24(p29);   // r02
		// 136
		// 137
		const float p31 = sat((k[23] * r28) * 16.0f);
		const float r32 = w24(p31);   // r01
		// 138
		const float p33 = sat((k[24] * r30) * 16.0f);
		const float r34 = w24(p33);   // r02
		// 139
		// 13a
		const float p35 = sat((k[26] * r32) * 16.0f);
		const float r36 = w24(p35);   // r03
		// 13b
		const float p37 = sat((k[27] * r34) * 16.0f);
		const float r38 = w24(p37);   // r04
		// 13c
		// 13d
		const float p39 = sat((k[29] * r36) * 16.0f);
		const float r40 = w24(p39);   // r05
		// 13e
		const float p41 = sat((k[30] * r38) * 16.0f);
		const float r42 = w24(p41);   // r06
		// 13f
		// 140
		const float p43 = sat((k[32] * r40) * 16.0f);
		const float r44 = w24(p43);   // r07
		// 141
		const float p45 = sat((k[33] * r42) * 16.0f);
		const float r46 = w24(p45);   // r08
		// 142
		// 143
		const float p47 = sat((k[35] * r44) * 16.0f);
		const float m48 = w24(p47);   // m01
		// 144
		const float p49 = sat((k[36] * r46) * 16.0f);
		const float m50 = w24(p49);   // m02
		// 145
		// 146
		const float p51 = sat((k[38] * m48) * 16.0f);
		const float m52 = w24(p51);   // m03
		// 147
		const float p53 = sat((k[39] * m50) * 16.0f);
		const float m54 = w24(p53);   // m04
		// 148
		const float p55 = k[40] * r32;
		// 149
		const float p56 = k[41] * r36 + p55;
		// 14a
		const float p57 = k[42] * r40 + p56;
		// 14b
		const float p58 = k[43] * r44 + p57;
		// 14c
		const float p59 = k[44] * m48 + p58;
		// 14d
		const float p60 = sat(k[45] * m52 + p59);
		const float r61 = w24(p60);   // r01
		// 14e
		const float p62 = k[46] * r34;
		// 14f
		const float p63 = k[47] * r38 + p62;
		// 150
		const float p64 = k[48] * r42 + p63;
		// 151
		const float p65 = k[49] * r46 + p64;
		// 152
		const float p66 = k[50] * m50 + p65;
		// 153
		const float p67 = sat(k[51] * m54 + p66);
		const float r68 = w24(p67);   // r02
		// 154
		const float p69 = sat(k[52] * r61);
		const float r70 = w24(p69);   // r47
		// 155
		const float p71 = k[53] * s_r48;
		// 156
		const float p72 = k[54] * s_r47 + p71;
		const float r73 = s_r47;   // r48
		// 157
		const float p74 = k[55] * s_r4a + p72;
		// 158
		const float p75 = k[56] * s_r49 + p74;
		// 159
		const float p76 = sat((k[57] * r70 + p75) * 2.0f);
		const float r77 = w24(p76);   // r49
		// 15a
		const float p78 = k[58] * s_r49;
		const float r79 = s_r49;   // r4a
		// 15b
		const float p80 = k[59] * s_r4a + p78;
		// 15c
		const float p81 = k[60] * s_r4c + p80;
		// 15d
		const float p82 = k[61] * s_r4b + p81;
		// 15e
		const float p83 = sat((k[62] * r77 + p82) * 2.0f);
		const float r84 = w24(p83);   // r4b
		// 15f
		const float p85 = k[63] * s_r4b;
		const float r86 = s_r4b;   // r4c
		// 160
		const float p87 = k[64] * s_r4c + p85;
		// 161
		const float p88 = k[65] * s_r4d + p87;
		const float r89 = s_r4d;   // r4e
		// 162
		const float p90 = k[66] * s_r4e + p88;
		// 163
		const float p91 = sat((k[67] * r84 + p90) * 16.0f);
		const float r92 = w24(p91);   // r4d
		// 164
		const float p93 = sat(k[68] * r68);
		const float r94 = w24(p93);   // r52
		// 165
		const float p95 = k[69] * s_r53;
		// 166
		const float p96 = k[70] * s_r52 + p95;
		const float r97 = s_r52;   // r53
		// 167
		const float p98 = k[71] * s_r55 + p96;
		// 168
		const float p99 = k[72] * s_r54 + p98;
		// 169
		const float p100 = sat((k[73] * r94 + p99) * 2.0f);
		const float r101 = w24(p100);   // r54
		// 16a
		const float p102 = k[74] * s_r54;
		const float r103 = s_r54;   // r55
		// 16b
		const float p104 = k[75] * s_r55 + p102;
		// 16c
		const float p105 = k[76] * s_r57 + p104;
		// 16d
		const float p106 = k[77] * s_r56 + p105;
		// 16e
		const float p107 = sat((k[78] * r101 + p106) * 2.0f);
		const float r108 = w24(p107);   // r56
		// 16f
		const float p109 = k[79] * s_r56;
		const float r110 = s_r56;   // r57
		// 170
		const float p111 = k[80] * s_r57 + p109;
		// 171
		const float p112 = k[81] * s_r58 + p111;
		const float r113 = s_r58;   // r59
		// 172
		const float p114 = k[82] * s_r59 + p112;
		// 173
		const float p115 = sat((k[83] * r108 + p114) * 16.0f);
		const float r116 = w24(p115);   // r58
		// 174
		const float p117 = k[84] * in[0];
		// 175
		const float p118 = sat((k[85] * r92 + p117) * 4.0f);
		const float m119 = w24(p118);   // m2c
		// 176
		const float p120 = k[86] * in[1];
		// 177
		const float p121 = sat((k[87] * r116 + p120) * 4.0f);
		const float m122 = w24(p121);   // m2d
		// 178
		// 179
		// 17a
		// 17b
		// 17c
		// 17d
		// 17e
		// 17f
		out[0] = m119;   // m2c
		out[1] = m122;   // m2d
		s_p = p121;
		s_r44 = r3;
		s_r45 = r7;
		s_r46 = r11;
		s_r4f = r14;
		s_r50 = r18;
		s_r51 = r22;
		s_r48 = r73;
		s_r47 = r70;
		s_r4a = r79;
		s_r49 = r77;
		s_r4c = r86;
		s_r4b = r84;
		s_r4d = r92;
		s_r4e = r89;
		s_r53 = r97;
		s_r52 = r94;
		s_r55 = r103;
		s_r54 = r101;
		s_r57 = r110;
		s_r56 = r108;
		s_r58 = r116;
		s_r59 = r113;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_STEREO_DIST_H
