// license:BSD-3-Clause
// S-MU2000: バリエーション: AMP SIM 2
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_AMPSIM2_H
#define S_MU2000_DSP_MEG_FX_VAR_AMPSIM2_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_ampsim2 : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = k[0] * s_r4b;
		const int32_t i2 = idx_of(p1);
		// 121
		const float p3 = k[1] * in[0];
		// 122
		const float p4 = sat(k[2] * in[1] + p3);
		const float r5 = w24(p4);   // r45
		const float t6 = tv_index(p1);   // t1
		// 123
		const float p7 = k[3] * s_r45;
		const float r8 = s_r45;   // r46
		const float q9 = tab(3, i2);
		// 124
		const float p10 = k[4] * s_r46 + p7;
		// 125
		const float p11 = k[5] * s_r47 + p10;
		const float m12 = q9;   // m01
		// 126
		const float p13 = k[6] * s_r48 + p11;
		const float q14 = tab(6, i2 + 1);
		// 127
		const float p15 = sat((k[7] * r5 + p13) * 4.0f);
		const float r16 = w24(p15);   // r47
		// 128
		const float p17 = k[8] * s_r47;
		const float m18 = q14;   // m02
		const float r19 = s_r47;   // r48
		// 129
		const float p20 = k[9] * s_r49 + p17;
		// 12a
		const float p21 = sat((k[10] * r16 + p20) * 2.0f);
		const float r22 = w24(p21);   // r49
		// 12b
		const float p23 = t6 * m12 - m12;
		// 12c
		const float p24 = sat(t6 * m18 - p23);
		const float r25 = w24(p24);   // r01
		// 12d
		const float p26 = sat((k[13] * r22) * 16.0f);
		// 12e
		const float p27 = sat((p26) * 16.0f);
		// 12f
		const float p28 = sat((p27) * 16.0f);
		const float r29 = w24(p28);   // r02
		// 130
		const float p30 = sat(k[16] + p28);
		const float r31 = w24(p30);   // r03
		// 131
		const float p32 = sat(k[17] * r25);
		const float r33 = w24(p32);   // r4c
		// 132
		const float p34 = k[18] * s_r4c;
		const float t35 = tv_plain(p30);   // t2
		// 133
		const float p36 = k[19] * s_r44 + p34;
		// 134
		const float p37 = sat((k[20] * r33 + p36) * 4.0f);
		const float r38 = w24(p37);   // r44
		// 135
		const float p39 = k[21] * s_r44;
		const float r40 = s_r44;   // r4d
		// 136
		const float p41 = k[22] * s_r4d + p39;
		// 137
		const float p42 = k[23] * s_r4e + p41;
		// 138
		const float p43 = k[24] * s_r4f + p42;
		// 139
		const float p44 = sat((k[25] * r38 + p43) * 2.0f);
		const float r45 = w24(p44);   // r4e
		// 13a
		const float p46 = k[26] * s_r4e;
		const float r47 = s_r4e;   // r4f
		// 13b
		const float p48 = k[27] * s_r4f + p46;
		// 13c
		const float p49 = k[28] * s_r50 + p48;
		// 13d
		const float p50 = k[29] * s_r51 + p49;
		// 13e
		const float p51 = sat((k[30] * r45 + p50) * 4.0f);
		const float r52 = w24(p51);   // r50
		// 13f
		const float p53 = k[31] * s_r50;
		const float r54 = s_r50;   // r51
		// 140
		const float p55 = k[32] * s_r51 + p53;
		// 141
		const float p56 = k[33] * s_r52 + p55;
		const float r57 = s_r52;   // r53
		// 142
		const float p58 = k[34] * s_r53 + p56;
		// 143
		const float p59 = sat((k[35] * r52 + p58) * 16.0f);
		const float r60 = w24(p59);   // r52
		// 144
		const float p61 = sat(t35 * r31);
		const float r62 = w24(p61);   // r03
		// 145
		const float p63 = k[37] * in[0];
		const float m64 = w24(p63);   // m01
		// 146
		const float p65 = k[38] * in[1];
		const float m66 = w24(p65);   // m02
		// 147
		const float p67 = k[39] * s_r4a - s_r4a;
		const float t68 = k[39];   // t3（定数）
		// 148
		const float p69 = t68 * r62 - p67;
		const float r70 = w24(p69);   // r4a
		// 149
		const float p71 = r62 - p69;
		const float r72 = w24(p71);   // r03
		// 14a
		const float p73 = sat((k[42] * r60) * 16.0f);
		const float r74 = w24(p73);   // r05
		// 14b
		const float p75 = sat(k[43] * r29 - r29);
		const float t76 = k[43];   // t3（定数）
		// 14c
		const float p77 = sat(t76 * r72 - p75);
		const float r78 = w24(p77);   // r03
		// 14d
		const float p79 = sat((k[45] * r74 + m64) * 4.0f);
		const float m80 = w24(p79);   // m2c
		const float t81 = k[45];   // t3（定数）
		// 14e
		const float p82 = sat((t81 * r74 + m66) * 4.0f);
		const float m83 = w24(p82);   // m2d
		// 14f
		const float p84 = sat((k[47] * r78) * 16.0f);
		const float r85 = w24(p84);   // r4b
		// 150
		// 151
		// 152
		// 153
		// 154
		// 155
		// 156
		// 157
		// 158
		// 159
		// 15a
		// 15b
		// 15c
		// 15d
		// 15e
		// 15f
		// 160
		// 161
		// 162
		// 163
		// 164
		// 165
		// 166
		// 167
		// 168
		// 169
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
		out[0] = m80;   // m2c
		out[1] = m83;   // m2d
		s_p = p84;
		s_r4b = r85;
		s_r45 = r5;
		s_r46 = r8;
		s_r47 = r16;
		s_r48 = r19;
		s_r49 = r22;
		s_r4c = r33;
		s_r44 = r38;
		s_r4d = r40;
		s_r4e = r45;
		s_r4f = r47;
		s_r50 = r52;
		s_r51 = r54;
		s_r52 = r60;
		s_r53 = r57;
		s_r4a = r70;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_AMPSIM2_H
