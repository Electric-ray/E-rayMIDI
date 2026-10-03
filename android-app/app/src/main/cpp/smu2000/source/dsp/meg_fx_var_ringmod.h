// license:BSD-3-Clause
// S-MU2000: バリエーション: RING MOD
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_RINGMOD_H
#define S_MU2000_DSP_MEG_FX_VAR_RINGMOD_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_ringmod : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x80000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; s_r5c = 0; s_r5d = 0; s_r5e = 0; s_r5f = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * s_r4a;
		const int32_t i2 = idx_of(p1);
		// 121
		const float p3 = sat((k[1] * in[0]) * 2.0f);
		const float r4 = w24(p3);   // r44
		// 122
		const float p5 = k[2] * s_r44;
		const float t6 = tv_index(p1);   // t1
		// 123
		const float p7 = k[3] * s_r45 + p5;
		const float q8 = tab(3, i2);
		// 124
		const float p9 = sat((k[4] * r4 + p7) * 2.0f);
		const float r10 = w24(p9);   // r45
		// 125
		const float p11 = k[5] * s_r45;
		const float m12 = q8;   // m01
		// 126
		const float p13 = k[6] * s_r46 + p11;
		const float q14 = tab(6, i2 + 1);
		// 127
		const float p15 = sat((k[7] * r10 + p13) * 4.0f);
		const float r16 = w24(p15);   // r46
		// 128
		const float p17 = s_r4a;
		const float m18 = q14;   // m02
		// 129
		const float p19 = k[9] + p17;
		const float r20 = w24(p19);   // r4a
		// 12a
		const float p21 = t6 * m12 - m12;
		// 12b
		const float p22 = sat(t6 * m18 - p21);
		const float w23 = p22;
		// 12c
		const float p24 = sat((k[12] * in[1]) * 2.0f);
		const float r25 = w24(p24);   // r47
		// 12d
		const float p26 = k[13] * s_r47;
		// 12e
		const float p27 = k[14] * s_r48 + p26;
		// 12f
		const float p28 = sat((k[15] * r25 + p27) * 2.0f);
		const float r29 = w24(p28);   // r48
		ram[at(15)] = w23;
		// 130
		const float p30 = k[16] * s_r48;
		const float m31 = lfo[19];   // m03
		// 131
		const float p32 = k[17] * s_r49 + p30;
		// 132
		const float p33 = sat((k[18] * r29 + p32) * 4.0f);
		const float r34 = w24(p33);   // r49
		// 133
		const float p35 = sat(k[19] * m31);
		const float r36 = w24(p35);   // r4e
		// 134
		const float p37 = k[20] * s_r4e;
		// 135
		const float p38 = k[21] * s_r4f + p37;
		// 136
		const float p39 = sat(k[22] * r36 + p38);
		const float r40 = w24(p39);   // r4f
		// 137
		const float p41 = sat((k[23] * r16) * 2.0f);
		const float r42 = w24(p41);   // r50
		// 138
		const float p43 = k[24] * s_r50;
		const float r44 = s_r50;   // r51
		// 139
		const float p45 = k[25] * s_r51 + p43;
		// 13a
		const float p46 = k[26] * s_r52 + p45;
		// 13b
		const float p47 = k[27] * s_r53 + p46;
		// 13c
		const float p48 = sat((k[28] * r42 + p47) * 4.0f);
		const float r49 = w24(p48);   // r52
		// 13d
		const float p50 = k[29] * s_r52;
		const float r51 = s_r52;   // r53
		// 13e
		const float p52 = k[30] * s_r53 + p50;
		// 13f
		const float p53 = k[31] * s_r54 + p52;
		// 140
		const float p54 = k[32] * s_r55 + p53;
		// 141
		const float p55 = sat((k[33] * r49 + p54) * 4.0f);
		const float r56 = w24(p55);   // r54
		// 142
		const float p57 = k[34] * s_r54;
		const float r58 = s_r54;   // r55
		// 143
		const float p59 = k[35] * s_r55 + p57;
		// 144
		const float p60 = k[36] * s_r56 + p59;
		const float r61 = s_r56;   // r57
		// 145
		const float p62 = k[37] * s_r57 + p60;
		// 146
		const float p63 = sat((k[38] * r56 + p62) * 16.0f);
		const float r64 = w24(p63);   // r56
		// 147
		const float p65 = sat(k[39] * r40);
		const int32_t i66 = idx_of(p65);
		// 148
		// 149
		const float t67 = tv_index(p65);   // t2
		// 14a
		const float p68 = sat((k[42] * r34) * 2.0f);
		const float r69 = w24(p68);   // r58
		const float q70 = ram[at(42, i66)];
		// 14b
		const float p71 = k[43] * s_r58;
		const float r72 = s_r58;   // r59
		// 14c
		const float p73 = k[44] * s_r59 + p71;
		const float m74 = q70;   // m01
		// 14d
		const float p75 = k[45] * s_r5a + p73;
		const float q76 = ram[at(45, i66 + 1)];
		// 14e
		const float p77 = k[46] * s_r5b + p75;
		// 14f
		const float p78 = sat((k[47] * r69 + p77) * 4.0f);
		const float m79 = q76;   // m02
		const float r80 = w24(p78);   // r5a
		// 150
		const float p81 = k[48] * s_r5a;
		const float r82 = s_r5a;   // r5b
		// 151
		const float p83 = k[49] * s_r5b + p81;
		// 152
		const float p84 = k[50] * s_r5c + p83;
		// 153
		const float p85 = k[51] * s_r5d + p84;
		// 154
		const float p86 = sat((k[52] * r80 + p85) * 4.0f);
		const float r87 = w24(p86);   // r5c
		// 155
		const float p88 = k[53] * s_r5c;
		const float r89 = s_r5c;   // r5d
		// 156
		const float p90 = k[54] * s_r5d + p88;
		// 157
		const float p91 = k[55] * s_r5e + p90;
		const float r92 = s_r5e;   // r5f
		// 158
		const float p93 = k[56] * s_r5f + p91;
		// 159
		const float p94 = sat((k[57] * r87 + p93) * 16.0f);
		const float r95 = w24(p94);   // r5e
		// 15a
		const float p96 = t67 * m74 - m74;
		// 15b
		const float p97 = sat(t67 * m79 - p96);
		const float r98 = w24(p97);   // r4c
		// 15c
		const float p99 = k[60] * s_r4c;
		// 15d
		const float p100 = k[61] * s_r4d + p99;
		// 15e
		const float p101 = sat(k[62] * r98 + p100);
		const float r102 = w24(p101);   // r4d
		// 15f
		// 160
		const float t103 = tv_plain(p101);   // t3
		// 161
		const float p104 = sat(t103 * r64);
		const float m105 = w24(p104);   // m01
		// 162
		const float p106 = sat(t103 * r95);
		const float m107 = w24(p106);   // m02
		// 163
		const float p108 = k[67] * r102;
		const float r109 = w24(p108);   // r03
		// 164
		// 165
		// 166
		const float p110 = k[70] * m105 + r109;
		// 167
		const float p111 = sat((k[71] * r42 + p110) * 4.0f);
		const float m112 = w24(p111);   // m2c
		// 168
		const float p113 = k[72] * m107 + r109;
		// 169
		const float p114 = sat((k[73] * r69 + p113) * 4.0f);
		const float m115 = w24(p114);   // m2d
		// 16a
		// 16b
		// 16c
		// 16d
		// 16e
		// 16f
		// 170
		// 171
		// 172
		// 173
		// 174
		// 175
		// 176
		// 177
		// 178
		// 179
		// 17a
		// 17b
		// 17c
		// 17d
		// 17e
		// 17f
		out[0] = m112;   // m2c
		out[1] = m115;   // m2d
		s_p = p114;
		s_r4a = r20;
		s_r44 = r4;
		s_r45 = r10;
		s_r46 = r16;
		s_r47 = r25;
		s_r48 = r29;
		s_r49 = r34;
		s_r4e = r36;
		s_r4f = r40;
		s_r50 = r42;
		s_r51 = r44;
		s_r52 = r49;
		s_r53 = r51;
		s_r54 = r56;
		s_r55 = r58;
		s_r56 = r64;
		s_r57 = r61;
		s_r58 = r69;
		s_r59 = r72;
		s_r5a = r80;
		s_r5b = r82;
		s_r5c = r87;
		s_r5d = r89;
		s_r5e = r95;
		s_r5f = r92;
		s_r4c = r98;
		s_r4d = r102;
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

#endif // S_MU2000_DSP_MEG_FX_VAR_RINGMOD_H
