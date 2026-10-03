// license:BSD-3-Clause
// S-MU2000: インサーション 1: ER 1, ER 2, GATE REV, REVRS GATE, KARAOKE 1, KARAOKE 2, KARAOKE 3
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_ER_H
#define S_MU2000_DSP_MEG_FX_INS_ER_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_er : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r2b = 0; s_r2c = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * in[0];
		const float q2 = ram[at(0)];
		// 0c1
		const float p3 = sat((k[1] * in[1] + p1) * 2.0f);
		const float r4 = w24(p3);   // r20
		// 0c2
		const float p5 = k[2] * s_r21;
		const float m6 = q2;   // m01
		// 0c3
		const float p7 = k[3] * s_r20 + p5;
		const float r8 = s_r20;   // r21
		const float q9 = ram[at(3)];
		// 0c4
		const float p10 = k[4] * r4 + p7;
		// 0c5
		const float p11 = k[5] * s_r22 + p10;
		const float m12 = q9;   // m02
		const float r13 = s_r22;   // r23
		// 0c6
		const float p14 = sat((k[6] * s_r23 + p11) * 2.0f);
		const float r15 = w24(p14);   // r22
		const float q16 = ram[at(6)];
		// 0c7
		const float p17 = k[7] * s_r23;
		// 0c8
		const float p18 = k[8] * s_r22 + p17;
		const float m19 = q16;   // m03
		// 0c9
		const float p20 = k[9] * r15 + p18;
		const float q21 = ram[at(9)];
		// 0ca
		const float p22 = k[10] * s_r24 + p20;
		const float r23 = s_r24;   // r25
		// 0cb
		const float p24 = sat((k[11] * s_r25 + p22) * 2.0f);
		const float m25 = q21;   // m04
		const float r26 = w24(p24);   // r24
		// 0cc
		const float p27 = k[12] * s_r25;
		const float q28 = ram[at(12)];
		// 0cd
		const float p29 = k[13] * s_r24 + p27;
		// 0ce
		const float p30 = k[14] * r26 + p29;
		const float m31 = q28;   // m05
		// 0cf
		const float p32 = k[15] * s_r26 + p30;
		const float r33 = s_r26;   // r27
		const float q34 = ram[at(15)];
		// 0d0
		const float p35 = sat((k[16] * s_r27 + p32) * 4.0f);
		const float r36 = w24(p35);   // r26
		// 0d1
		const float m37 = q34;   // m06
		// 0d2
		const float q38 = ram[at(18)];
		// 0d3
		// 0d4
		const float m39 = q38;   // m07
		// 0d5
		const float p40 = k[21] * m6;
		const float q41 = ram[at(21)];
		// 0d6
		const float p42 = k[22] * m12 + p40;
		// 0d7
		const float p43 = k[23] * m19 + p42;
		const float m44 = q41;   // m08
		// 0d8
		const float p45 = k[24] * m25 + p43;
		const float q46 = ram[at(24)];
		// 0d9
		const float p47 = k[25] * m31 + p45;
		// 0da
		const float p48 = k[26] * m37 + p47;
		const float m49 = q46;   // m09
		// 0db
		const float p50 = k[27] * m39 + p48;
		const float q51 = ram[at(27)];
		// 0dc
		const float p52 = k[28] * m44 + p50;
		// 0dd
		const float p53 = sat(k[29] * m49 + p52);
		const float m54 = q51;   // m01
		const float r55 = w24(p53);   // r29
		const float w56 = p53;
		// 0de
		const float q57 = ram[at(30)];
		// 0df
		const float p58 = k[31] * r36;
		// 0e0
		const float p59 = sat((k[32] * m54 + p58) * 2.0f);
		const float m60 = q57;   // m02
		const float r61 = w24(p59);   // r01
		// 0e1
		const float p62 = k[33] * r36;
		ram[at(33)] = w56;
		// 0e2
		const float p63 = sat(k[34] * m54 + p62);
		const float w64 = p63;
		// 0e3
		const float p65 = k[35] * r61;
		// 0e4
		const float p66 = sat((k[36] * m60 + p65) * 2.0f);
		const float r67 = w24(p66);   // r01
		const float q68 = ram[at(36)];
		// 0e5
		const float p69 = k[37] * r61;
		// 0e6
		const float p70 = sat(k[38] * m60 + p69);
		const float m71 = q68;   // m03
		const float w72 = p70;
		// 0e7
		ram[at(39)] = w64;
		// 0e8
		const float p73 = k[40] * s_m12;
		// 0e9
		const float p74 = k[41] * m71 + p73;
		const float m75 = m71;   // m12
		// 0ea
		const float p76 = sat(k[42] * s_r28 + p74);
		const float r77 = w24(p76);   // r28
		const float q78 = ram[at(42)];
		// 0eb
		// 0ec
		const float m79 = q78;   // m01
		// 0ed
		const float q80 = ram[at(45)];
		// 0ee
		// 0ef
		const float m81 = q80;   // m02
		// 0f0
		const float q82 = ram[at(48)];
		// 0f1
		// 0f2
		const float m83 = q82;   // m03
		// 0f3
		const float q84 = ram[at(51)];
		// 0f4
		// 0f5
		const float m85 = q84;   // m04
		// 0f6
		const float q86 = ram[at(54)];
		// 0f7
		// 0f8
		const float m87 = q86;   // m05
		// 0f9
		const float q88 = ram[at(57)];
		// 0fa
		// 0fb
		const float m89 = q88;   // m06
		// 0fc
		const float q90 = ram[at(60)];
		// 0fd
		// 0fe
		const float m91 = q90;   // m07
		// 0ff
		const float p92 = k[63] * m79;
		const float q93 = ram[at(63)];
		// 100
		const float p94 = k[64] * m81 + p92;
		// 101
		const float p95 = k[65] * m83 + p94;
		const float m96 = q93;   // m08
		// 102
		const float p97 = k[66] * m85 + p95;
		const float q98 = ram[at(66)];
		// 103
		const float p99 = k[67] * m87 + p97;
		// 104
		const float p100 = k[68] * m89 + p99;
		const float m101 = q98;   // m09
		// 105
		const float p102 = k[69] * m91 + p100;
		ram[at(69)] = w72;
		// 106
		const float p103 = k[70] * m96 + p102;
		// 107
		const float p104 = sat(k[71] * m101 + p103);
		const float r105 = w24(p104);   // r2a
		const float w106 = p104;
		// 108
		const float p107 = k[72] * in[0];
		const float q108 = ram[at(72)];
		// 109
		const float p109 = sat((k[73] * s_r2b + p107) * 4.0f);
		const float m110 = w24(p109);   // m28
		// 10a
		const float p111 = k[74] * in[1];
		const float m112 = q108;   // m01
		// 10b
		const float p113 = sat((k[75] * s_r2c + p111) * 4.0f);
		const float m114 = w24(p113);   // m29
		const float q115 = ram[at(75)];
		// 10c
		const float p116 = k[76] * r55;
		// 10d
		const float p117 = sat(k[77] * m112 + p116);
		const float m118 = q115;   // m02
		const float r119 = w24(p117);   // r02
		// 10e
		ram[at(78)] = w106;
		// 10f
		const float p120 = k[79] * r105;
		// 110
		const float p121 = sat(k[80] * m118 + p120);
		const float r122 = w24(p121);   // r03
		// 111
		const float q123 = ram[at(81)];
		// 112
		// 113
		const float m124 = q123;   // m03
		// 114
		const float p125 = k[84] * r67;
		const float q126 = ram[at(84)];
		// 115
		const float p127 = sat(k[85] * r77 + p125);
		const float w128 = p127;
		// 116
		const float p129 = k[86] * m124;
		const float m130 = q126;   // m04
		// 117
		const float p131 = sat(k[87] * r119 + p129);
		const float w132 = p131;
		ram[at(87)] = w128;
		// 118
		const float p133 = k[88] * m124;
		// 119
		const float p134 = sat((k[89] * r119 + p133) * 2.0f);
		const float r135 = w24(p134);   // r2b
		// 11a
		const float p136 = k[90] * m130;
		ram[at(90)] = w132;
		// 11b
		const float p137 = sat(k[91] * r122 + p136);
		const float w138 = p137;
		// 11c
		const float p139 = k[92] * m130;
		// 11d
		const float p140 = sat((k[93] * r122 + p139) * 2.0f);
		const float r141 = w24(p140);   // r2c
		ram[at(93)] = w138;
		// 11e
		// 11f
		out[0] = m110;   // m28
		out[1] = m114;   // m29
		s_p = p140;
		s_r21 = r8;
		s_r20 = r4;
		s_r22 = r15;
		s_r23 = r13;
		s_r24 = r26;
		s_r25 = r23;
		s_r26 = r36;
		s_r27 = r33;
		s_m12 = m75;
		s_r28 = r77;
		s_r2b = r135;
		s_r2c = r141;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
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
	float s_r2b = 0.0f;
	float s_r2c = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_ER_H
