// license:BSD-3-Clause
// S-MU2000: インサーション 1: 3-BAND EQ, 2-BAND EQ, PITCH CNG1, PITCH CNG2, VOIC CANCL, ENS DETUNE
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_EQ_H
#define S_MU2000_DSP_MEG_FX_INS_EQ_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_eq : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x30000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = (s_m12) * 2.0f;
		const float r2 = w24(p1);   // r06
		// 0c1
		const float p3 = satpos(k[1] * s_m12);
		const int32_t i4 = idx_of(p3);
		// 0c2
		const float p5 = (s_m13) * 2.0f;
		const float r6 = w24(p5);   // r09
		// 0c3
		const float p7 = k[3] * r2;
		const float t8 = tv_index(p3);   // t1
		// 0c4
		const float p9 = satpos(k[4] + p7);
		const float r10 = w24(p9);   // r05
		// 0c5
		const float p11 = satabs(r2);
		const float r12 = w24(p11);   // r06
		// 0c6
		const float p13 = satpos(k[6] + p11);
		const float r14 = w24(p13);   // r07
		const float q15 = ram[at(6, i4)];
		// 0c7
		const float p16 = satpos(k[7] * r10);
		const int32_t i17 = idx_of(p16);
		// 0c8
		const float p18 = satpos(k[8] + r12);
		const float m19 = q15;   // m01
		const float r20 = w24(p18);   // r06
		// 0c9
		const float p21 = k[9] * r6;
		const float t22 = tv_index(p16);   // t2
		const float q23 = ram[at(9, i4 + 1)];
		// 0ca
		const float p24 = satpos(k[10] + p21);
		const float r25 = w24(p24);   // r08
		// 0cb
		const float p26 = satabs(r6);
		const float m27 = q23;   // m02
		const float r28 = w24(p26);   // r09
		// 0cc
		const float p29 = satpos(k[12] + p26);
		const float r30 = w24(p29);   // r3f
		const float q31 = ram[at(12, i17)];
		// 0cd
		const float p32 = satpos(k[13] * r25);
		const int32_t i33 = idx_of(p32);
		// 0ce
		const float p34 = satpos(k[14] + r28);
		const float m35 = q31;   // m03
		const float r36 = w24(p34);   // r09
		// 0cf
		const float p37 = sat((k[15] * r20) * 16.0f);
		const float t38 = tv_index(p32);   // t5
		const float q39 = ram[at(15, i17 + 1)];
		// 0d0
		const float p40 = t8 * m19 - m19;
		// 0d1
		const float p41 = sat(t8 * m27 - p40);
		const float m42 = q39;   // m04
		const float r43 = w24(p41);   // r01
		const float t44 = tv_plain(p37);   // t1
		// 0d2
		const float p45 = sat((k[18] * r14) * 16.0f);
		const float q46 = ram[at(18, i33)];
		// 0d3
		const float p47 = t22 * m35 - m35;
		// 0d4
		const float p48 = sat(t22 * m42 - p47);
		const float m49 = q46;   // m07
		const float r50 = w24(p48);   // r02
		const float t51 = tv_plain(p45);   // t2
		// 0d5
		const float p52 = satpos(k[21] * s_m13);
		const int32_t i53 = idx_of(p52);
		const float q54 = ram[at(21, i33 + 1)];
		// 0d6
		const float p55 = sat((k[22] * in[0]) * 2.0f);
		const float r56 = w24(p55);   // r20
		// 0d7
		const float p57 = k[23] * s_r20;
		const float m58 = q54;   // m08
		const float r59 = s_r20;   // r21
		const float t60 = tv_index(p52);   // t3
		// 0d8
		const float p61 = k[24] * s_r21 + p57;
		const float q62 = ram[at(24, i53)];
		// 0d9
		const float p63 = k[25] * s_r22 + p61;
		// 0da
		const float p64 = k[26] * s_r23 + p63;
		const float m65 = q62;   // m05
		// 0db
		const float p66 = sat((k[27] * r56 + p64) * 2.0f);
		const float r67 = w24(p66);   // r22
		const float q68 = ram[at(27, i53 + 1)];
		// 0dc
		const float p69 = k[28] * s_r22;
		const float r70 = s_r22;   // r23
		// 0dd
		const float p71 = k[29] * s_r23 + p69;
		const float m72 = q68;   // m06
		// 0de
		const float p73 = k[30] * s_r24 + p71;
		// 0df
		const float p74 = k[31] * s_r25 + p73;
		// 0e0
		const float p75 = sat((k[32] * r67 + p74) * 2.0f);
		const float r76 = w24(p75);   // r24
		// 0e1
		const float p77 = k[33] * s_r24;
		const float r78 = s_r24;   // r25
		// 0e2
		const float p79 = k[34] * s_r25 + p77;
		// 0e3
		const float p80 = k[35] * s_r26 + p79;
		const float r81 = s_r26;   // r27
		// 0e4
		const float p82 = k[36] * s_r27 + p80;
		// 0e5
		const float p83 = sat((k[37] * r76 + p82) * 16.0f);
		const float r84 = w24(p83);   // r26
		// 0e6
		const float p85 = sat((k[38] * r30) * 16.0f);
		// 0e7
		const float p86 = t38 * m49 - m49;
		// 0e8
		const float p87 = sat(t38 * m58 - p86);
		const float r88 = w24(p87);   // r04
		const float t89 = tv_plain(p85);   // t5
		// 0e9
		const float p90 = sat((k[41] * r36) * 16.0f);
		// 0ea
		const float p91 = t60 * m65 - m65;
		// 0eb
		const float p92 = sat(t60 * m72 - p91);
		const float r93 = w24(p92);   // r03
		const float t94 = tv_plain(p90);   // t3
		// 0ec
		const float p95 = k[44] * in[1];
		// 0ed
		const float p96 = sat((k[45] * in[0] + p95) * 2.0f);
		const float r97 = w24(p96);   // r28
		// 0ee
		const float p98 = k[46] * s_r28;
		const float r99 = s_r28;   // r29
		// 0ef
		const float p100 = k[47] * s_r29 + p98;
		// 0f0
		const float p101 = k[48] * s_r2a + p100;
		// 0f1
		const float p102 = k[49] * s_r2b + p101;
		// 0f2
		const float p103 = sat((k[50] * r97 + p102) * 2.0f);
		const float r104 = w24(p103);   // r2a
		// 0f3
		const float p105 = k[51] * s_r2a;
		const float r106 = s_r2a;   // r2b
		// 0f4
		const float p107 = k[52] * s_r2b + p105;
		// 0f5
		const float p108 = k[53] * s_r2c + p107;
		// 0f6
		const float p109 = k[54] * s_r2d + p108;
		// 0f7
		const float p110 = sat((k[55] * r104 + p109) * 2.0f);
		const float r111 = w24(p110);   // r2c
		// 0f8
		const float p112 = k[56] * s_r2c;
		const float r113 = s_r2c;   // r2d
		// 0f9
		const float p114 = k[57] * s_r2d + p112;
		// 0fa
		const float p115 = k[58] * s_r2e + p114;
		const float r116 = s_r2e;   // r2f
		// 0fb
		const float p117 = k[59] * s_r2f + p115;
		// 0fc
		const float p118 = sat((k[60] * r111 + p117) * 16.0f);
		const float r119 = w24(p118);   // r2e
		// 0fd
		const float p120 = t51 * r50 - r50;
		// 0fe
		const float p121 = sat(t44 * r43 - p120);
		const float r122 = w24(p121);   // r01
		// 0ff
		const float p123 = t89 * r88 - r88;
		// 100
		const float p124 = sat(t94 * r93 - p123);
		const float r125 = w24(p124);   // r03
		// 101
		const float p126 = k[65] * r84;
		// 102
		const float p127 = k[66] * r119 + p126;
		// 103
		const float p128 = k[67] * r122 + p127;
		// 104
		const float p129 = sat(k[68] * r125 + p128);
		const float w130 = p129;
		// 105
		const float p131 = k[69] * r119;
		// 106
		const float p132 = k[70] * r125 + p131;
		// 107
		const float p133 = sat(k[71] * r122 + p132);
		const float w134 = p133;
		// 108
		const float p135 = k[72] * r122;
		ram[at(72)] = w130;
		// 109
		const float p136 = sat(k[73] * r125 + p135);
		const float r137 = w24(p136);   // r02
		// 10a
		const float p138 = k[74] * r125;
		// 10b
		const float p139 = sat(k[75] * r122 + p138);
		const float r140 = w24(p139);   // r04
		ram[at(75)] = w134;
		// 10c
		const float p141 = k[76] * in[0];
		// 10d
		const float p142 = k[77] * r84 + p141;
		// 10e
		const float p143 = k[78] * r119 + p142;
		const float m144 = lfo[16];   // m12
		// 10f
		const float p145 = sat((k[79] * r137 + p143) * 4.0f);
		const float m146 = w24(p145);   // m28
		// 110
		const float p147 = k[80] * in[1];
		// 111
		const float p148 = k[81] * r119 + p147;
		// 112
		const float p149 = k[82] * r84 + p148;
		// 113
		const float p150 = sat((k[83] * r140 + p149) * 4.0f);
		const float m151 = w24(p150);   // m29
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
		const float m152 = lfo[17];   // m13
		out[0] = m146;   // m28
		out[1] = m151;   // m29
		s_p = p150;
		s_m12 = m144;
		s_m13 = m152;
		s_r20 = r56;
		s_r21 = r59;
		s_r22 = r67;
		s_r23 = r70;
		s_r24 = r76;
		s_r25 = r78;
		s_r26 = r84;
		s_r27 = r81;
		s_r28 = r97;
		s_r29 = r99;
		s_r2a = r104;
		s_r2b = r106;
		s_r2c = r111;
		s_r2d = r113;
		s_r2e = r119;
		s_r2f = r116;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
	float s_m13 = 0.0f;
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

#endif // S_MU2000_DSP_MEG_FX_INS_EQ_H
