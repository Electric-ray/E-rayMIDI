// license:BSD-3-Clause
// S-MU2000: インサーション 1: HALL 1, HALL 2, HALL M, HALL L, ROOM 1, ROOM 2, ROOM 3, ROOM S, ROOM M, ROOM L, STAGE 1, STAGE 2, PLATE, GM PLATE, WHITE ROOM, TUNNEL, CANYON, BASEMENT
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_REVERB_H
#define S_MU2000_DSP_MEG_FX_INS_REVERB_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_reverb : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; }

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
		const float q8 = ram[at(3)];
		// 0c4
		const float p9 = sat(k[4] * r4 + p7);
		const float r10 = w24(p9);   // r21
		// 0c5
		const float p11 = k[5] * s_r22;
		const float m12 = q8;   // m02
		// 0c6
		const float p13 = k[6] * s_r21 + p11;
		const float q14 = ram[at(6)];
		// 0c7
		const float p15 = sat((k[7] * r10 + p13) * 2.0f);
		const float r16 = w24(p15);   // r22
		// 0c8
		const float p17 = sat((k[8] * m6) * 2.0f);
		const float m18 = q14;   // m03
		const float r19 = w24(p17);   // r23
		// 0c9
		const float p20 = k[9] * s_r24;
		const float q21 = ram[at(9)];
		// 0ca
		const float p22 = k[10] * s_r23 + p20;
		// 0cb
		const float p23 = sat((k[11] * r19 + p22) * 4.0f);
		const float m24 = q21;   // m04
		const float r25 = w24(p23);   // r24
		// 0cc
		const float q26 = ram[at(12)];
		// 0cd
		const float p27 = k[13] * m12;
		// 0ce
		const float p28 = sat((k[14] * r25 + p27) * 2.0f);
		const float m29 = q26;   // m05
		const float r30 = w24(p28);   // r01
		// 0cf
		const float p31 = k[15] * m12;
		const float q32 = ram[at(15)];
		// 0d0
		const float p33 = sat(k[16] * r25 + p31);
		const float w34 = p33;
		// 0d1
		const float p35 = k[17] * m18;
		const float m36 = q32;   // m06
		// 0d2
		const float p37 = sat(k[18] * r30 + p35);
		const float w38 = p37;
		ram[at(18)] = w34;
		// 0d3
		const float p39 = k[19] * m18;
		// 0d4
		const float p40 = sat((k[20] * r30 + p39) * 2.0f);
		const float m41 = w24(p40);   // m09
		// 0d5
		const float p42 = k[21] * m24;
		const float m43 = m24;   // m12
		ram[at(21)] = w38;
		// 0d6
		const float p44 = k[22] * s_m12 + p42;
		// 0d7
		const float p45 = sat((k[23] * s_r25 + p44) * 2.0f);
		const float r46 = w24(p45);   // r25
		// 0d8
		const float p47 = k[24] * m29;
		const float m48 = m29;   // m13
		const float q49 = ram[at(24)];
		// 0d9
		const float p50 = k[25] * s_m13 + p47;
		// 0da
		const float p51 = sat((k[26] * s_r26 + p50) * 2.0f);
		const float m52 = q49;   // m07
		const float r53 = w24(p51);   // r26
		// 0db
		const float p54 = sat(k[27] * m41 + r46);
		const float w55 = p54;
		const float q56 = ram[at(27)];
		// 0dc
		const float p57 = k[28] * m36;
		const float m58 = m36;   // m14
		// 0dd
		const float p59 = k[29] * s_m14 + p57;
		const float m60 = q56;   // m02
		// 0de
		const float p61 = sat((k[30] * s_r27 + p59) * 2.0f);
		const float r62 = w24(p61);   // r27
		ram[at(30)] = w55;
		// 0df
		const float p63 = sat(k[31] * m41 + r53);
		const float w64 = p63;
		// 0e0
		const float p65 = k[32] * m52;
		const float m66 = m52;   // m15
		// 0e1
		const float p67 = k[33] * s_m15 + p65;
		ram[at(33)] = w64;
		// 0e2
		const float p68 = sat((k[34] * s_r28 + p67) * 2.0f);
		const float r69 = w24(p68);   // r28
		// 0e3
		const float p70 = sat(k[35] * m41 + r62);
		const float w71 = p70;
		// 0e4
		const float q72 = ram[at(36)];
		// 0e5
		// 0e6
		const float m73 = q72;   // m03
		// 0e7
		ram[at(39)] = w71;
		// 0e8
		// 0e9
		// 0ea
		const float q74 = ram[at(42)];
		// 0eb
		// 0ec
		const float m75 = q74;   // m04
		// 0ed
		const float q76 = ram[at(45)];
		// 0ee
		// 0ef
		const float m77 = q76;   // m05
		// 0f0
		const float q78 = ram[at(48)];
		// 0f1
		const float p79 = k[49] * m60;
		// 0f2
		const float p80 = k[50] * m73 + p79;
		const float m81 = q78;   // m06
		// 0f3
		const float p82 = k[51] * m75 + p80;
		const float q83 = ram[at(51)];
		// 0f4
		const float p84 = k[52] * m77 + p82;
		// 0f5
		const float p85 = sat(k[53] * m81 + p84);
		const float m86 = q83;   // m02
		const float r87 = w24(p85);   // r02
		// 0f6
		const float q88 = ram[at(54)];
		// 0f7
		// 0f8
		const float p89 = k[56] * r87;
		const float m90 = q88;   // m03
		// 0f9
		const float p91 = sat((k[57] * m86 + p89) * 2.0f);
		const float r92 = w24(p91);   // r02
		const float q93 = ram[at(57)];
		// 0fa
		const float p94 = k[58] * r87;
		// 0fb
		const float p95 = sat(k[59] * m86 + p94);
		const float m96 = q93;   // m04
		const float w97 = p95;
		// 0fc
		const float p98 = k[60] * r92;
		const float q99 = ram[at(60)];
		// 0fd
		const float p100 = sat((k[61] * m90 + p98) * 2.0f);
		const float r101 = w24(p100);   // r02
		// 0fe
		const float p102 = k[62] * r92;
		const float m103 = q99;   // m05
		// 0ff
		const float p104 = sat(k[63] * m90 + p102);
		const float w105 = p104;
		ram[at(63)] = w97;
		// 100
		// 101
		// 102
		ram[at(66)] = w105;
		// 103
		// 104
		// 105
		const float q106 = ram[at(69)];
		// 106
		// 107
		const float m107 = q106;   // m06
		// 108
		const float q108 = ram[at(72)];
		// 109
		// 10a
		const float m109 = q108;   // m07
		// 10b
		const float q110 = ram[at(75)];
		// 10c
		const float p111 = k[76] * m96;
		// 10d
		const float p112 = k[77] * m103 + p111;
		const float m113 = q110;   // m08
		// 10e
		const float p114 = k[78] * m107 + p112;
		const float q115 = ram[at(78)];
		// 10f
		const float p116 = k[79] * m109 + p114;
		// 110
		const float p117 = sat(k[80] * m113 + p116);
		const float m118 = q115;   // m02
		const float r119 = w24(p117);   // r03
		// 111
		const float q120 = ram[at(81)];
		// 112
		const float p121 = sat(k[82] * m41 + r69);
		const float w122 = p121;
		// 113
		const float p123 = sat(k[83] * m6 + r16);
		const float m124 = q120;   // m03
		const float w125 = p123;
		// 114
		const float p126 = k[84] * r119;
		ram[at(84)] = w122;
		// 115
		const float p127 = sat((k[85] * m118 + p126) * 2.0f);
		const float r128 = w24(p127);   // r03
		// 116
		const float p129 = k[86] * r119;
		// 117
		const float p130 = sat(k[87] * m118 + p129);
		const float w131 = p130;
		ram[at(87)] = w125;
		// 118
		const float p132 = k[88] * r128;
		// 119
		const float p133 = sat((k[89] * m124 + p132) * 2.0f);
		const float r134 = w24(p133);   // r03
		// 11a
		const float p135 = k[90] * r128;
		ram[at(90)] = w131;
		// 11b
		const float p136 = sat(k[91] * m124 + p135);
		const float w137 = p136;
		// 11c
		const float p138 = k[92] * in[0];
		// 11d
		const float p139 = sat((k[93] * r101 + p138) * 4.0f);
		const float m140 = w24(p139);   // m28
		ram[at(93)] = w137;
		// 11e
		const float p141 = k[94] * in[1];
		// 11f
		const float p142 = sat((k[95] * r134 + p141) * 4.0f);
		const float m143 = w24(p142);   // m29
		out[0] = m140;   // m28
		out[1] = m143;   // m29
		s_p = p142;
		s_r21 = r10;
		s_r20 = r4;
		s_r22 = r16;
		s_r24 = r25;
		s_r23 = r19;
		s_m12 = m43;
		s_r25 = r46;
		s_m13 = m48;
		s_r26 = r53;
		s_m14 = m58;
		s_r27 = r62;
		s_m15 = m66;
		s_r28 = r69;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
	float s_m13 = 0.0f;
	float s_m14 = 0.0f;
	float s_m15 = 0.0f;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_REVERB_H
