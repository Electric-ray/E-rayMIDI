// license:BSD-3-Clause
// S-MU2000: インサーション 1: AMP SIM 2
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_AMPSIM2_H
#define S_MU2000_DSP_MEG_FX_INS_AMPSIM2_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_ampsim2 : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * s_r27;
		const int32_t i2 = idx_of(p1);
		// 0c1
		const float p3 = k[1] * in[0];
		// 0c2
		const float p4 = sat(k[2] * in[1] + p3);
		const float r5 = w24(p4);   // r21
		const float t6 = tv_index(p1);   // t1
		// 0c3
		const float p7 = k[3] * s_r21;
		const float r8 = s_r21;   // r22
		const float q9 = tab(3, i2);
		// 0c4
		const float p10 = k[4] * s_r22 + p7;
		// 0c5
		const float p11 = k[5] * s_r23 + p10;
		const float m12 = q9;   // m01
		// 0c6
		const float p13 = k[6] * s_r24 + p11;
		const float q14 = tab(6, i2 + 1);
		// 0c7
		const float p15 = sat((k[7] * r5 + p13) * 4.0f);
		const float r16 = w24(p15);   // r23
		// 0c8
		const float p17 = k[8] * s_r23;
		const float m18 = q14;   // m02
		const float r19 = s_r23;   // r24
		// 0c9
		const float p20 = k[9] * s_r25 + p17;
		// 0ca
		const float p21 = sat((k[10] * r16 + p20) * 2.0f);
		const float r22 = w24(p21);   // r25
		// 0cb
		const float p23 = t6 * m12 - m12;
		// 0cc
		const float p24 = sat(t6 * m18 - p23);
		const float r25 = w24(p24);   // r01
		// 0cd
		const float p26 = sat((k[13] * r22) * 16.0f);
		// 0ce
		const float p27 = sat((p26) * 16.0f);
		// 0cf
		const float p28 = sat((p27) * 16.0f);
		const float r29 = w24(p28);   // r02
		// 0d0
		const float p30 = sat(k[16] + p28);
		const float r31 = w24(p30);   // r03
		// 0d1
		const float p32 = sat(k[17] * r25);
		const float r33 = w24(p32);   // r28
		// 0d2
		const float p34 = k[18] * s_r28;
		const float t35 = tv_plain(p30);   // t2
		// 0d3
		const float p36 = k[19] * s_r20 + p34;
		// 0d4
		const float p37 = sat((k[20] * r33 + p36) * 4.0f);
		const float r38 = w24(p37);   // r20
		// 0d5
		const float p39 = k[21] * s_r20;
		const float r40 = s_r20;   // r29
		// 0d6
		const float p41 = k[22] * s_r29 + p39;
		// 0d7
		const float p42 = k[23] * s_r2a + p41;
		// 0d8
		const float p43 = k[24] * s_r2b + p42;
		// 0d9
		const float p44 = sat((k[25] * r38 + p43) * 2.0f);
		const float r45 = w24(p44);   // r2a
		// 0da
		const float p46 = k[26] * s_r2a;
		const float r47 = s_r2a;   // r2b
		// 0db
		const float p48 = k[27] * s_r2b + p46;
		// 0dc
		const float p49 = k[28] * s_r2c + p48;
		// 0dd
		const float p50 = k[29] * s_r2d + p49;
		// 0de
		const float p51 = sat((k[30] * r45 + p50) * 4.0f);
		const float r52 = w24(p51);   // r2c
		// 0df
		const float p53 = k[31] * s_r2c;
		const float r54 = s_r2c;   // r2d
		// 0e0
		const float p55 = k[32] * s_r2d + p53;
		// 0e1
		const float p56 = k[33] * s_r2e + p55;
		const float r57 = s_r2e;   // r2f
		// 0e2
		const float p58 = k[34] * s_r2f + p56;
		// 0e3
		const float p59 = sat((k[35] * r52 + p58) * 16.0f);
		const float r60 = w24(p59);   // r2e
		// 0e4
		const float p61 = sat(t35 * r31);
		const float r62 = w24(p61);   // r03
		// 0e5
		const float p63 = k[37] * in[0];
		const float m64 = w24(p63);   // m01
		// 0e6
		const float p65 = k[38] * in[1];
		const float m66 = w24(p65);   // m02
		// 0e7
		const float p67 = k[39] * s_r26 - s_r26;
		const float t68 = k[39];   // t3（定数）
		// 0e8
		const float p69 = t68 * r62 - p67;
		const float r70 = w24(p69);   // r26
		// 0e9
		const float p71 = r62 - p69;
		const float r72 = w24(p71);   // r03
		// 0ea
		const float p73 = sat((k[42] * r60) * 16.0f);
		const float r74 = w24(p73);   // r05
		// 0eb
		const float p75 = sat(k[43] * r29 - r29);
		const float t76 = k[43];   // t3（定数）
		// 0ec
		const float p77 = sat(t76 * r72 - p75);
		const float r78 = w24(p77);   // r03
		// 0ed
		const float p79 = sat((k[45] * r74 + m64) * 4.0f);
		const float m80 = w24(p79);   // m28
		const float t81 = k[45];   // t3（定数）
		// 0ee
		const float p82 = sat((t81 * r74 + m66) * 4.0f);
		const float m83 = w24(p82);   // m29
		// 0ef
		const float p84 = sat((k[47] * r78) * 16.0f);
		const float r85 = w24(p84);   // r27
		// 0f0
		// 0f1
		// 0f2
		// 0f3
		// 0f4
		// 0f5
		// 0f6
		// 0f7
		// 0f8
		// 0f9
		// 0fa
		// 0fb
		// 0fc
		// 0fd
		// 0fe
		// 0ff
		// 100
		// 101
		// 102
		// 103
		// 104
		// 105
		// 106
		// 107
		// 108
		// 109
		// 10a
		// 10b
		// 10c
		// 10d
		// 10e
		// 10f
		// 110
		// 111
		// 112
		// 113
		// 114
		// 115
		// 116
		// 117
		// 118
		// 119
		// 11a
		// 11b
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m80;   // m28
		out[1] = m83;   // m29
		s_p = p84;
		s_r27 = r85;
		s_r21 = r5;
		s_r22 = r8;
		s_r23 = r16;
		s_r24 = r19;
		s_r25 = r22;
		s_r28 = r33;
		s_r20 = r38;
		s_r29 = r40;
		s_r2a = r45;
		s_r2b = r47;
		s_r2c = r52;
		s_r2d = r54;
		s_r2e = r60;
		s_r2f = r57;
		s_r26 = r70;
	}

private:
	float s_m00 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r20 = 0.0f;
	float s_r21 = 0.0f;
	float s_r22 = 0.0f;
	float s_r23 = 0.0f;
	float s_r24 = 0.0f;
	float s_r25 = 0.0f;
	float s_r26 = 0.0f;
	float s_r27 = 0.0f;
	float s_r28 = 0.0f;
	float s_r29 = 0.0f;
	float s_r2a = 0.0f;
	float s_r2b = 0.0f;
	float s_r2c = 0.0f;
	float s_r2d = 0.0f;
	float s_r2e = 0.0f;
	float s_r2f = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_AMPSIM2_H
