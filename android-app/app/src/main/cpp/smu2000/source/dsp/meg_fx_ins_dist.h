// license:BSD-3-Clause
// S-MU2000: インサーション 1: DISTORTION, CMP+DT, OVERDRIVE, AMP SIM, HM ENHNCER, COMPRESSOR, NOISE GATE
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m29 m28 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_DIST_H
#define S_MU2000_DSP_MEG_FX_INS_DIST_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_dist : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x29, 0x28 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r3d = 0; s_r3e = 0; s_r3f = 0; }

	// in: 入口（m29 m28）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat((k[0] * s_m12) * 16.0f);
		const float r2 = w24(p1);   // r01
		// 0c1
		const float p3 = sat((k[1] * in[0]) * 2.0f);
		const float m4 = in[0];   // m03
		const float r5 = w24(p3);   // r23
		// 0c2
		const float p6 = k[2] * s_r23;
		const float r7 = s_r23;   // r24
		// 0c3
		const float p8 = k[3] * s_r24 + p6;
		// 0c4
		const float p9 = k[4] * s_r25 + p8;
		const float r10 = s_r25;   // r26
		// 0c5
		const float p11 = k[5] * s_r26 + p9;
		// 0c6
		const float p12 = sat((k[6] * r5 + p11) * 2.0f);
		const float r13 = w24(p12);   // r25
		// 0c7
		const float p14 = sat((k[7] * r2) * 16.0f);
		const float r15 = w24(p14);   // r01
		// 0c8
		const float p16 = k[8] * in[1];
		const float m17 = in[1];   // m02
		// 0c9
		const float p18 = sat((k[9] * in[0] + p16) * 2.0f);
		const float r19 = w24(p18);   // r33
		// 0ca
		const float p20 = sat((k[10] * r15) * 16.0f);
		const float r21 = w24(p20);   // r01
		// 0cb
		const float p22 = k[11] * s_r33;
		const float r23 = s_r33;   // r20
		// 0cc
		const float p24 = k[12] * s_r20 + p22;
		// 0cd
		const float p25 = k[13] * s_r21 + p24;
		const float r26 = s_r21;   // r22
		// 0ce
		const float p27 = k[14] * s_r22 + p25;
		// 0cf
		const float p28 = sat((k[15] * r19 + p27) * 2.0f);
		const float r29 = w24(p28);   // r21
		// 0d0
		const float p30 = sat((k[16] * r21) * 16.0f);
		const float r31 = w24(p30);   // r02
		// 0d1
		const float p32 = sat((k[17] * r13) * 2.0f);
		const float r33 = w24(p32);   // r35
		// 0d2
		const float p34 = sat((k[18] * r29) * 2.0f);
		const float r35 = w24(p34);   // r34
		// 0d3
		const float p36 = sat((k[19] * r31) * 16.0f);
		const float r37 = w24(p36);   // r03
		// 0d4
		const float p38 = k[20] * r33;
		// 0d5
		const float p39 = satabs((k[21] * r35 + p38) * 16.0f);
		const float m40 = w24(p39);   // m01
		// 0d6
		const float p41 = satpos(k[22] * s_r27 - p39);
		const float r42 = w24(p41);   // r36
		// 0d7
		const float p43 = sat((k[23] * r37) * 16.0f);
		const float r44 = w24(p43);   // r04
		// 0d8
		const float p45 = k[24] * s_r3d;
		const float r46 = s_r3d;   // r3e
		// 0d9
		const float p47 = k[25] * s_r3e + p45;
		// 0da
		const float p48 = sat(k[26] * s_r3f + p47);
		const float r49 = w24(p48);   // r3f
		// 0db
		const float p50 = sat((k[27] * r44) * 16.0f);
		const float r51 = w24(p50);   // r05
		// 0dc
		const float p52 = sat(m40 + r42);
		const float r53 = w24(p52);   // r27
		const float t54 = tv_plain(p48);   // t1
		// 0dd
		const float p55 = k[29] * s_r28;
		// 0de
		const float p56 = k[30] * s_r28 + (p55 * (1.0f / 32768.0f));
		// 0df
		const float p57 = m40 - p56;
		// 0e0
		const float p58 = satpos(k[32] * r53 - p57);
		// 0e1
		const float p59 = sat(m40 + p58);
		const float r60 = w24(p59);   // r28
		// 0e2
		const float p61 = sat((k[34] * r51) * 16.0f);
		const float r62 = w24(p61);   // r01
		// 0e3
		const float p63 = k[35] * r21;
		// 0e4
		const float p64 = k[36] * r31 + p63;
		// 0e5
		const float p65 = k[37] * r37 + p64;
		// 0e6
		const float p66 = k[38] * r44 + p65;
		// 0e7
		const float p67 = k[39] * r51 + p66;
		// 0e8
		const float p68 = sat(k[40] * r62 + p67);
		const float r69 = w24(p68);   // r01
		// 0e9
		const float p70 = sat(t54 * r35);
		const float r71 = w24(p70);   // r39
		// 0ea
		const float p72 = sat(t54 * r33);
		const float r73 = w24(p72);   // r3a
		// 0eb
		const float p74 = sat(k[43] * r69);
		const float r75 = w24(p74);   // r2b
		const float t76 = tv_plain(p70);   // t2
		// 0ec
		const float p77 = sat(k[44] * r60);
		const float r78 = w24(p77);   // r38
		const float t79 = tv_plain(p72);   // t3
		// 0ed
		const float p80 = k[45] * s_r2b;
		const float r81 = s_r2b;   // r2c
		// 0ee
		const float p82 = k[46] * s_r2c + p80;
		// 0ef
		const float p83 = k[47] * s_r2d + p82;
		// 0f0
		const float p84 = k[48] * s_r2e + p83;
		// 0f1
		const float p85 = sat((k[49] * r75 + p84) * 2.0f);
		const float r86 = w24(p85);   // r2d
		// 0f2
		const float p87 = k[50] * s_r2d;
		const float r88 = s_r2d;   // r2e
		// 0f3
		const float p89 = k[51] * s_r2e + p87;
		// 0f4
		const float p90 = k[52] * s_r2f + p89;
		// 0f5
		const float p91 = k[53] * s_r30 + p90;
		// 0f6
		const float p92 = sat((k[54] * r86 + p91) * 2.0f);
		const float r93 = w24(p92);   // r2f
		// 0f7
		const float p94 = k[55] * s_r2f;
		const float r95 = s_r2f;   // r30
		// 0f8
		const float p96 = k[56] * s_r30 + p94;
		// 0f9
		const float p97 = k[57] * s_r31 + p96;
		const float r98 = s_r31;   // r32
		// 0fa
		const float p99 = k[58] * s_r32 + p97;
		// 0fb
		const float p100 = sat((k[59] * r93 + p99) * 16.0f);
		const float r101 = w24(p100);   // r31
		// 0fc
		const float p102 = sat(t76 * r35);
		const float r103 = w24(p102);   // r3b
		// 0fd
		const float p104 = k[61] * r71;
		// 0fe
		const float p105 = k[62] * r35 + p104;
		// 0ff
		const float p106 = sat((k[63] * r103 + p105) * 16.0f);
		const float r107 = w24(p106);   // r29
		// 100
		const float p108 = k[64] * s_r29;
		// 101
		const float p109 = k[65] * s_m12 + p108;
		// 102
		const float p110 = sat((k[66] * r107 + p109) * 2.0f);
		const float m111 = w24(p110);   // m12
		// 103
		const float p112 = sat((k[67] * r101 + s_m14) * 4.0f);
		const float m113 = w24(p112);   // m28
		// 104
		const float p114 = k[68] * r101 + s_m15;
		// 105
		const float p115 = sat((k[69] * s_m13 + p114) * 4.0f);
		const float m116 = w24(p115);   // m29
		// 106
		const float p117 = sat(t79 * r33);
		const float r118 = w24(p117);   // r3c
		// 107
		const float p119 = k[71] * r73;
		// 108
		const float p120 = k[72] * r33 + p119;
		// 109
		const float p121 = sat((k[73] * r118 + p120) * 16.0f);
		const float r122 = w24(p121);   // r2a
		// 10a
		const float p123 = k[74] * s_r2a;
		// 10b
		const float p124 = k[75] * s_m13 + p123;
		// 10c
		const float p125 = sat((k[76] * r122 + p124) * 2.0f);
		const float m126 = w24(p125);   // m13
		// 10d
		const float p127 = satpos(k[77] + r78);
		const float r128 = w24(p127);   // r01
		// 10e
		const float p129 = satpos(k[78] + r78);
		const float r130 = w24(p129);   // r02
		// 10f
		const float p131 = satpos(k[79] + r78);
		const float r132 = w24(p131);   // r03
		// 110
		const float p133 = satpos(k[80] + r78);
		const float r134 = w24(p133);   // r04
		// 111
		const float p135 = satpos(k[81] + r78);
		const float r136 = w24(p135);   // r05
		// 112
		const float p137 = satpos(k[82] + r78);
		const float r138 = w24(p137);   // r06
		// 113
		const float p139 = satpos(k[83] + r78);
		const float r140 = w24(p139);   // r39
		// 114
		const float p141 = satpos(k[84] + r78);
		const float r142 = w24(p141);   // r3a
		// 115
		const float p143 = (k[85] * r128) * 2.0f;
		// 116
		const float p144 = (k[86] * r132 + p143) * 2.0f;
		// 117
		const float p145 = (k[87] * r134 + p144) * 2.0f;
		// 118
		const float p146 = (k[88] * r130 + p145) * 2.0f;
		// 119
		const float p147 = (k[89] * r136 + p146) * 2.0f;
		// 11a
		const float p148 = (k[90] * r138 + p147) * 4.0f;
		// 11b
		const float p149 = k[91] * r140 + p148;
		// 11c
		const float p150 = k[92] * r142 + p149;
		// 11d
		const float p151 = satpos(k[93] + p150);
		const float r152 = w24(p151);   // r3d
		// 11e
		const float p153 = sat(k[94] * m17);
		const float m154 = w24(p153);   // m14
		// 11f
		const float p155 = sat(k[95] * m4);
		const float m156 = w24(p155);   // m15
		out[0] = m113;   // m28
		out[1] = m116;   // m29
		s_p = p155;
		s_m12 = m111;
		s_r23 = r5;
		s_r24 = r7;
		s_r25 = r13;
		s_r26 = r10;
		s_r33 = r19;
		s_r20 = r23;
		s_r21 = r29;
		s_r22 = r26;
		s_r27 = r53;
		s_r3d = r152;
		s_r3e = r46;
		s_r3f = r49;
		s_r28 = r60;
		s_r2b = r75;
		s_r2c = r81;
		s_r2d = r86;
		s_r2e = r88;
		s_r2f = r93;
		s_r30 = r95;
		s_r31 = r101;
		s_r32 = r98;
		s_r29 = r107;
		s_m14 = m154;
		s_m15 = m156;
		s_m13 = m126;
		s_r2a = r122;
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
	float s_r29 = 0.0f;
	float s_r2a = 0.0f;
	float s_r2b = 0.0f;
	float s_r2c = 0.0f;
	float s_r2d = 0.0f;
	float s_r2e = 0.0f;
	float s_r2f = 0.0f;
	float s_r30 = 0.0f;
	float s_r31 = 0.0f;
	float s_r32 = 0.0f;
	float s_r33 = 0.0f;
	float s_r3d = 0.0f;
	float s_r3e = 0.0f;
	float s_r3f = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_DIST_H
