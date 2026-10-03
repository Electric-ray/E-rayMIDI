// license:BSD-3-Clause
// S-MU2000: インサーション 1: TALK MOD
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_TALKMOD_H
#define S_MU2000_DSP_MEG_FX_INS_TALKMOD_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_talkmod : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * in[0];
		// 0c1
		const float p2 = sat((k[1] * in[1] + p1) * 2.0f);
		const float r3 = w24(p2);   // r20
		// 0c2
		const float p4 = k[2] * s_r21;
		// 0c3
		const float p5 = k[3] * s_r20 + p4;
		const float r6 = s_r20;   // r21
		// 0c4
		const float p7 = k[4] * r3 + p5;
		// 0c5
		const float p8 = k[5] * s_r22 + p7;
		const float r9 = s_r22;   // r23
		// 0c6
		const float p10 = sat((k[6] * s_r23 + p8) * 2.0f);
		const float r11 = w24(p10);   // r22
		// 0c7
		const float p12 = satpos(k[7] + s_m14);
		const float r13 = w24(p12);   // r02
		// 0c8
		const float p14 = satpos(k[8] + s_m14);
		const float r15 = w24(p14);   // r03
		// 0c9
		const float p16 = sat((k[9] * r11) * 2.0f);
		const float r17 = w24(p16);   // r3f
		// 0ca
		const float p18 = satpos(k[10] + s_m14);
		const float r19 = w24(p18);   // r04
		// 0cb
		const float p20 = satpos(k[11] + s_m14);
		const float r21 = w24(p20);   // r05
		// 0cc
		const float p22 = satabs((k[12] * r17) * 16.0f);
		const float r23 = w24(p22);   // r01
		// 0cd
		const float p24 = satpos(k[13] + s_m14);
		const float r25 = w24(p24);   // r06
		// 0ce
		const float p26 = satpos(k[14] + s_m14);
		const float r27 = w24(p26);   // r07
		// 0cf
		const float p28 = satpos(k[15] * s_m12 - r23);
		// 0d0
		const float p29 = sat(k[16] * r23 + p28);
		const float m30 = w24(p29);   // m12
		// 0d1
		const float p31 = k[17] * s_m13;
		// 0d2
		const float p32 = k[18] * s_m13 + (p31 * (1.0f / 32768.0f));
		// 0d3
		const float p33 = k[19] * r23 - p32;
		// 0d4
		const float p34 = satpos(k[20] * m30 - p33);
		// 0d5
		const float p35 = sat(k[21] * r23 + p34);
		const float m36 = w24(p35);   // m13
		// 0d6
		const float p37 = satpos(k[22] + s_m14);
		const float r38 = w24(p37);   // r08
		// 0d7
		const float p39 = satpos(k[23] + s_m14);
		const float r40 = w24(p39);   // r09
		// 0d8
		const float p41 = sat(k[24] * m36);
		const float m42 = w24(p41);   // m14
		// 0d9
		const float p43 = (k[25] * r13) * 2.0f;
		// 0da
		const float p44 = (k[26] * r19 + p43) * 2.0f;
		// 0db
		const float p45 = (k[27] * r21 + p44) * 2.0f;
		// 0dc
		const float p46 = (k[28] * r15 + p45) * 2.0f;
		// 0dd
		const float p47 = (k[29] * r25 + p46) * 2.0f;
		// 0de
		const float p48 = (k[30] * r27 + p47) * 4.0f;
		// 0df
		const float p49 = k[31] * r38 + p48;
		// 0e0
		const float p50 = k[32] * r40 + p49;
		// 0e1
		const float p51 = satpos(k[33] + p50);
		const float r52 = w24(p51);   // r24
		// 0e2
		const float p53 = k[34] * s_r25;
		// 0e3
		const float p54 = k[35] * s_r24 + p53;
		// 0e4
		const float p55 = sat(k[36] * r52 + p54);
		const float r56 = w24(p55);   // r25
		// 0e5
		const float p57 = k[37] * s_r27;
		// 0e6
		const float p58 = k[38] * in[0] + p57;
		const float t59 = tv_plain(p55);   // t1
		// 0e7
		const float p60 = sat((k[39] * in[1] + p58) * 2.0f);
		const float r61 = w24(p60);   // r03
		// 0e8
		const float p62 = sat(t59 * r17);
		// 0e9
		// 0ea
		const float t63 = tv_plain(p62);   // t1
		// 0eb
		const float p64 = sat(t63 * r17);
		const float r65 = w24(p64);   // r01
		// 0ec
		// 0ed
		const float p66 = k[45] * r17;
		// 0ee
		const float p67 = sat((k[46] * r65 + p66) * 16.0f);
		const float r68 = w24(p67);   // r26
		// 0ef
		const float p69 = k[47] * s_r27;
		// 0f0
		const float p70 = k[48] * s_r26 + p69;
		// 0f1
		const float p71 = sat((k[49] * r68 + p70) * 2.0f);
		const float r72 = w24(p71);   // r27
		// 0f2
		const float p73 = s_r29;
		// 0f3
		const float p74 = k[51] * s_r28 + p73;
		// 0f4
		const float p75 = sat(k[52] * r61 - p74);
		const float r76 = w24(p75);   // r01
		// 0f5
		const float p77 = s_r2d;
		// 0f6
		const float p78 = k[54] * s_r2c + p77;
		// 0f7
		const float p79 = sat(k[55] * s_r30 - p78);
		const float r80 = w24(p79);   // r02
		// 0f8
		const float p81 = k[56] * s_r28;
		// 0f9
		const float p82 = sat(k[57] * r76 + p81);
		const float r83 = w24(p82);   // r28
		const float t84 = k[57];   // t1（定数）
		// 0fa
		const float p85 = k[58] * s_r2c;
		// 0fb
		const float p86 = sat(k[59] * r80 + p85);
		const float r87 = w24(p86);   // r2c
		const float t88 = k[59];   // t2（定数）
		// 0fc
		const float p89 = k[60] * s_r29;
		// 0fd
		const float p90 = sat(t84 * r83 + p89);
		const float r91 = w24(p90);   // r29
		// 0fe
		const float p92 = r76;
		// 0ff
		const float p93 = k[63] * r83 + p92;
		// 100
		const float p94 = sat(r91 + p93);
		const float r95 = w24(p94);   // r01
		// 101
		const float p96 = k[65] * s_r2d;
		// 102
		const float p97 = sat(t88 * r87 + p96);
		const float r98 = w24(p97);   // r2d
		// 103
		const float p99 = r80;
		// 104
		const float p100 = k[68] * r87 + p99;
		// 105
		const float p101 = sat(r98 + p100);
		const float r102 = w24(p101);   // r02
		// 106
		const float p103 = s_r2b;
		// 107
		const float p104 = k[71] * s_r2a + p103;
		// 108
		const float p105 = sat(k[72] * r95 - p104);
		const float r106 = w24(p105);   // r01
		// 109
		const float p107 = s_r2f;
		// 10a
		const float p108 = k[74] * s_r2e + p107;
		// 10b
		const float p109 = sat(k[75] * r102 - p108);
		const float r110 = w24(p109);   // r02
		// 10c
		const float p111 = k[76] * s_r2a;
		// 10d
		const float p112 = sat(k[77] * r106 + p111);
		const float r113 = w24(p112);   // r2a
		const float t114 = k[77];   // t1（定数）
		// 10e
		const float p115 = k[78] * s_r2e;
		// 10f
		const float p116 = sat(k[79] * r110 + p115);
		const float r117 = w24(p116);   // r2e
		const float t118 = k[79];   // t2（定数）
		// 110
		const float p119 = k[80] * s_r2b;
		// 111
		const float p120 = sat(t114 * r113 + p119);
		const float r121 = w24(p120);   // r2b
		// 112
		const float p122 = r106;
		// 113
		const float p123 = k[83] * r113 + p122;
		// 114
		const float p124 = sat(r121 + p123);
		const float r125 = w24(p124);   // r30
		// 115
		const float p126 = k[85] * s_r2f;
		// 116
		const float p127 = sat(t118 * r117 + p126);
		const float r128 = w24(p127);   // r2f
		// 117
		const float p129 = r110;
		// 118
		const float p130 = k[88] * r117 + p129;
		// 119
		const float p131 = sat(r128 + p130);
		const float r132 = w24(p131);   // r02
		// 11a
		// 11b
		// 11c
		// 11d
		// 11e
		const float p133 = sat((k[94] * r132) * 4.0f);
		const float m134 = w24(p133);   // m28
		// 11f
		const float p135 = sat((k[95] * r132) * 4.0f);
		const float m136 = w24(p135);   // m29
		out[0] = m134;   // m28
		out[1] = m136;   // m29
		s_p = p135;
		s_r21 = r6;
		s_r20 = r3;
		s_r22 = r11;
		s_r23 = r9;
		s_m14 = m42;
		s_m12 = m30;
		s_m13 = m36;
		s_r25 = r56;
		s_r24 = r52;
		s_r27 = r72;
		s_r26 = r68;
		s_r29 = r91;
		s_r28 = r83;
		s_r2d = r98;
		s_r2c = r87;
		s_r30 = r125;
		s_r2b = r121;
		s_r2a = r113;
		s_r2f = r128;
		s_r2e = r117;
	}

private:
	float s_m00 = 0.0f;
	float s_m12 = 0.0f;
	float s_m13 = 0.0f;
	float s_m14 = 0.0f;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_TALKMOD_H
