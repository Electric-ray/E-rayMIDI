// license:BSD-3-Clause
// S-MU2000: インサーション 1: PHASER 1, PHASER 2, T.PHASER
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_PHASER_H
#define S_MU2000_DSP_MEG_FX_INS_PHASER_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_phaser : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x1000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0];
		// 0c1
		const float p2 = k[1] * s_m12 - (p1 * (1.0f / 32768.0f));
		// 0c2
		const float p3 = k[2] - p2;
		const float m4 = w24(p3);   // m12
		// 0c3
		const float p5 = sat((k[3] * in[0]) * 2.0f);
		const float r6 = w24(p5);   // r20
		// 0c4
		const float p7 = k[4] * s_r20;
		const float m8 = lfo[12];   // m01
		// 0c5
		const float p9 = k[5] * s_r21 + p7;
		// 0c6
		const float p10 = sat((k[6] * r6 + p9) * 2.0f);
		const float r11 = w24(p10);   // r21
		// 0c7
		const float p12 = k[7] * s_r21;
		// 0c8
		const float p13 = k[8] * s_r22 + p12;
		// 0c9
		const float p14 = sat((k[9] * r11 + p13) * 4.0f);
		const float r15 = w24(p14);   // r22
		// 0ca
		const float p16 = k[10] + m8;
		const float r17 = w24(p16);   // r04
		// 0cb
		const float p18 = k[11] + m8;
		const float r19 = w24(p18);   // r05
		// 0cc
		const float p20 = sat((k[12] * in[1]) * 2.0f);
		const float r21 = w24(p20);   // r23
		// 0cd
		const float p22 = k[13] * s_r23;
		// 0ce
		const float p23 = k[14] * s_r24 + p22;
		// 0cf
		const float p24 = sat((k[15] * r21 + p23) * 2.0f);
		const float r25 = w24(p24);   // r24
		// 0d0
		const float p26 = k[16] * s_r24;
		// 0d1
		const float p27 = k[17] * s_r25 + p26;
		// 0d2
		const float p28 = sat((k[18] * r25 + p27) * 4.0f);
		const float r29 = w24(p28);   // r25
		// 0d3
		const float p30 = satabs(k[19] * r17);
		// 0d4
		const float p31 = sat(k[20] + p30);
		const float r32 = w24(p31);   // r04
		// 0d5
		const float p33 = satabs(k[21] * r19);
		// 0d6
		const float p34 = sat(k[22] + p33);
		const float r35 = w24(p34);   // r05
		const float t36 = tv_plain(p31);   // t1
		// 0d7
		const float p37 = k[23] * s_r28;
		// 0d8
		const float p38 = k[24] * s_r29 + p37;
		const float t39 = tv_plain(p34);   // t2
		// 0d9
		const float p40 = k[25] * s_r2a + p38;
		// 0da
		const float p41 = k[26] * s_r2b + p40;
		// 0db
		const float p42 = sat((k[27] * s_r2c + p41) * 2.0f);
		const float r43 = w24(p42);   // r02
		// 0dc
		const float p44 = sat(t36 * r32);
		const float r45 = w24(p44);   // r04
		// 0dd
		const float p46 = sat(t39 * r35);
		const float r47 = w24(p46);   // r05
		// 0de
		const float p48 = k[30] * s_r2f;
		const float t49 = tv_plain(p44);   // t1
		// 0df
		const float p50 = k[31] * s_r30 + p48;
		const float t51 = tv_plain(p46);   // t2
		// 0e0
		const float p52 = k[32] * s_r31 + p50;
		// 0e1
		const float p53 = k[33] * s_r32 + p52;
		// 0e2
		const float p54 = sat((k[34] * s_r33 + p53) * 2.0f);
		const float r55 = w24(p54);   // r03
		// 0e3
		const float p56 = k[35];
		// 0e4
		const float p57 = sat(t49 * r45 - p56);
		// 0e5
		const float p58 = k[37] * r43;
		// 0e6
		const float p59 = k[38] * r55 + p58;
		const float t60 = tv_plain(p57);   // t1
		// 0e7
		const float p61 = k[39] * s_r2e + p59;
		// 0e8
		const float p62 = sat((k[40] * s_r32 + p61) * 2.0f);
		const float r63 = w24(p62);   // r01
		// 0e9
		const float p64 = k[41];
		// 0ea
		const float p65 = sat(t51 * r47 - p64);
		// 0eb
		const float p66 = k[43] * r15;
		// 0ec
		const float p67 = (k[44] * r29 + p66) * 2.0f;
		const float t68 = tv_plain(p65);   // t2
		// 0ed
		const float p69 = k[45] * r43 + p67;
		// 0ee
		const float p70 = sat(k[46] * r55 + p69);
		const float r71 = w24(p70);   // r26
		// 0ef
		const float p72 = s_r26;
		// 0f0
		const float p73 = t60 * s_r27 - p72;
		// 0f1
		const float p74 = sat(t60 * r71 - p73);
		const float r75 = w24(p74);   // r27
		// 0f2
		const float p76 = s_r27;
		// 0f3
		const float p77 = t60 * s_r28 - p76;
		// 0f4
		const float p78 = sat(t60 * r75 - p77);
		const float r79 = w24(p78);   // r28
		// 0f5
		const float p80 = s_r28;
		// 0f6
		const float p81 = t60 * s_r29 - p80;
		// 0f7
		const float p82 = sat(t60 * r79 - p81);
		const float r83 = w24(p82);   // r29
		// 0f8
		const float p84 = s_r29;
		// 0f9
		const float p85 = t60 * s_r2a - p84;
		// 0fa
		const float p86 = sat(t60 * r83 - p85);
		const float r87 = w24(p86);   // r2a
		// 0fb
		const float p88 = s_r2a;
		// 0fc
		const float p89 = t60 * s_r2b - p88;
		// 0fd
		const float p90 = sat(t60 * r87 - p89);
		const float r91 = w24(p90);   // r2b
		// 0fe
		const float p92 = s_r2b;
		// 0ff
		const float p93 = t60 * s_r2c - p92;
		// 100
		const float p94 = sat(t60 * r91 - p93);
		const float r95 = w24(p94);   // r2c
		// 101
		const float p96 = (k[65] * r29) * 2.0f;
		// 102
		const float p97 = k[66] * r55 + p96;
		// 103
		const float p98 = sat(k[67] * r43 + p97);
		const float r99 = w24(p98);   // r2d
		// 104
		const float p100 = s_r2d;
		// 105
		const float p101 = t68 * s_r2e - p100;
		// 106
		const float p102 = sat(t68 * r99 - p101);
		const float r103 = w24(p102);   // r2e
		// 107
		const float p104 = s_r2e;
		// 108
		const float p105 = t68 * s_r2f - p104;
		// 109
		const float p106 = sat(t68 * r103 - p105);
		const float r107 = w24(p106);   // r2f
		// 10a
		const float p108 = s_r2f;
		// 10b
		const float p109 = t68 * s_r30 - p108;
		// 10c
		const float p110 = sat(t68 * r107 - p109);
		const float r111 = w24(p110);   // r30
		// 10d
		const float p112 = s_r30;
		// 10e
		const float p113 = t68 * s_r31 - p112;
		// 10f
		const float p114 = sat(t68 * r111 - p113);
		const float r115 = w24(p114);   // r31
		// 110
		const float p116 = s_r31;
		// 111
		const float p117 = t68 * s_r32 - p116;
		// 112
		const float p118 = sat(t68 * r115 - p117);
		const float r119 = w24(p118);   // r32
		// 113
		const float p120 = s_r32;
		// 114
		const float p121 = t68 * s_r33 - p120;
		// 115
		const float p122 = sat(t68 * r119 - p121);
		const float r123 = w24(p122);   // r33
		// 116
		const float p124 = k[86] * r15;
		// 117
		const float p125 = sat((k[87] * r63 + p124) * 4.0f);
		const float m126 = w24(p125);   // m28
		// 118
		const float p127 = k[88] * r29;
		// 119
		const float p128 = sat((k[89] * r55 + p127) * 4.0f);
		const float m129 = w24(p128);   // m29
		// 11a
		// 11b
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m126;   // m28
		out[1] = m129;   // m29
		s_p = p128;
		s_m12 = m4;
		s_r20 = r6;
		s_r21 = r11;
		s_r22 = r15;
		s_r23 = r21;
		s_r24 = r25;
		s_r25 = r29;
		s_r28 = r79;
		s_r29 = r83;
		s_r2a = r87;
		s_r2b = r91;
		s_r2c = r95;
		s_r2f = r107;
		s_r30 = r111;
		s_r31 = r115;
		s_r32 = r119;
		s_r33 = r123;
		s_r2e = r103;
		s_r26 = r71;
		s_r27 = r75;
		s_r2d = r99;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_PHASER_H
