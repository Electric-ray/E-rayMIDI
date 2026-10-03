// license:BSD-3-Clause
// S-MU2000: インサーション 1: V DT HARD, V DT H+DLY, V DT SOFT, V DT S+DLY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_VDIST_H
#define S_MU2000_DSP_MEG_FX_INS_VDIST_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_vdist : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float q1 = ram[at(0)];
		// 0c1
		const float p2 = k[1] * in[0];
		// 0c2
		const float p3 = sat(k[2] * in[1] + p2);
		const float m4 = q1;   // m01
		const float r5 = w24(p3);   // r20
		// 0c3
		const float p6 = k[3] * s_r21;
		const float q7 = ram[at(3)];
		// 0c4
		const float p8 = k[4] * s_r20 + p6;
		// 0c5
		const float p9 = sat((k[5] * r5 + p8) * 2.0f);
		const float m10 = q7;   // m02
		const float r11 = w24(p9);   // r21
		// 0c6
		const float p12 = k[6] * s_r22;
		const float q13 = ram[at(6)];
		// 0c7
		const float p14 = k[7] * s_r21 + p12;
		const float r15 = s_r21;   // r22
		// 0c8
		const float p16 = k[8] * r11 + p14;
		const float m17 = q13;   // m03
		// 0c9
		const float p18 = k[9] * s_r23 + p16;
		const float q19 = ram[at(9)];
		// 0ca
		const float p20 = sat((k[10] * s_r24 + p18) * 4.0f);
		const float r21 = w24(p20);   // r23
		// 0cb
		const float p22 = k[11] * s_r25;
		const float m23 = q19;   // m04
		// 0cc
		const float p24 = k[12] * s_r23 + p22;
		const float r25 = s_r23;   // r24
		// 0cd
		const float p26 = sat((k[13] * r21 + p24) * 2.0f);
		const float r27 = w24(p26);   // r25
		// 0ce
		// 0cf
		// 0d0
		const float p28 = sat((k[16] * r27) * 16.0f);
		// 0d1
		const float p29 = sat((p28) * 16.0f);
		// 0d2
		const float p30 = sat((p29) * 16.0f);
		const float r31 = w24(p30);   // r02
		// 0d3
		const float p32 = sat(k[19] + p30);
		const float r33 = w24(p32);   // r03
		// 0d4
		const float p34 = k[20] * s_r27;
		const float r35 = w24(p34);   // r28
		// 0d5
		const float p36 = k[21] * s_r2a;
		const float t37 = tv_plain(p32);   // t1
		// 0d6
		const float p38 = sat((k[22] * s_r28 + p36) * 2.0f);
		const float r39 = s_r28;   // r29
		// 0d7
		const float p40 = k[23] * r35 + p38;
		// 0d8
		const float p41 = k[24] * s_r29 + p40;
		// 0d9
		const float p42 = sat(k[25] * s_r2b + p41);
		const float r43 = w24(p42);   // r2a
		// 0da
		const float p44 = k[26] * s_r2b;
		// 0db
		const float p45 = k[27] * s_r2a + p44;
		const float r46 = s_r2a;   // r2b
		// 0dc
		const float p47 = k[28] * r43 + p45;
		// 0dd
		const float p48 = k[29] * s_r2c + p47;
		// 0de
		const float p49 = sat((k[30] * s_r2d + p48) * 4.0f);
		const float r50 = w24(p49);   // r2c
		// 0df
		const float p51 = k[31] * s_r2e;
		// 0e0
		const float p52 = k[32] * s_r2c + p51;
		const float r53 = s_r2c;   // r2d
		// 0e1
		const float p54 = sat((k[33] * r50 + p52) * 2.0f);
		const float r55 = w24(p54);   // r2e
		// 0e2
		const float p56 = k[34] * s_r2f;
		// 0e3
		const float p57 = k[35] * s_r2e + p56;
		// 0e4
		const float p58 = sat((k[36] * r55 + p57) * 4.0f);
		const float r59 = w24(p58);   // r2f
		// 0e5
		// 0e6
		const float p60 = k[38] * s_r2f;
		// 0e7
		const float p61 = sat((k[39] * r59 + p60) * 16.0f);
		// 0e8
		const float p62 = sat(k[40] * s_r30 + p61);
		const float r63 = w24(p62);   // r30
		// 0e9
		const float p64 = k[41] * s_r31;
		// 0ea
		const float p65 = k[42] * s_r30 + p64;
		// 0eb
		const float p66 = sat((k[43] * r63 + p65) * 4.0f);
		const float r67 = w24(p66);   // r31
		// 0ec
		const float p68 = k[44] * s_r32;
		// 0ed
		const float p69 = k[45] * s_r31 + p68;
		const float r70 = s_r31;   // r32
		// 0ee
		const float p71 = k[46] * r67 + p69;
		// 0ef
		const float p72 = k[47] * s_r33 + p71;
		const float r73 = s_r33;   // r34
		// 0f0
		const float p74 = sat((k[48] * s_r34 + p72) * 4.0f);
		const float r75 = w24(p74);   // r33
		// 0f1
		const float p76 = k[49] * s_r35;
		// 0f2
		const float p77 = k[50] * s_r33 + p76;
		const float r78 = s_r33;   // r34
		// 0f3
		const float p79 = sat((k[51] * r75 + p77) * 4.0f);
		const float r80 = w24(p79);   // r35
		// 0f4
		const float p81 = sat(t37 * r33);
		const float r82 = w24(p81);   // r03
		// 0f5
		// 0f6
		const float p83 = k[54] * s_r26 - s_r26;
		const float t84 = k[54];   // t2（定数）
		// 0f7
		const float p85 = t84 * r82 - p83;
		const float r86 = w24(p85);   // r26
		// 0f8
		const float p87 = r82 - p85;
		const float r88 = w24(p87);   // r03
		// 0f9
		const float p89 = sat((k[57] * r80) * 2.0f);
		const float r90 = w24(p89);   // r05
		// 0fa
		const float p91 = sat(k[58] * r31 - r31);
		const float t92 = k[58];   // t2（定数）
		// 0fb
		const float p93 = sat(t92 * r88 - p91);
		const float r94 = w24(p93);   // r03
		// 0fc
		const float p95 = k[60] * r90;
		const float t96 = k[60];   // t2（定数）
		// 0fd
		const float p97 = sat((k[61] * in[0] + p95) * 4.0f);
		const float r98 = w24(p97);   // r06
		// 0fe
		const float p99 = sat((k[62] * r94) * 16.0f);
		const float r100 = w24(p99);   // r27
		// 0ff
		const float p101 = t96 * r90;
		// 100
		const float p102 = sat((k[64] * in[1] + p101) * 4.0f);
		const float r103 = w24(p102);   // r07
		// 101
		const float p104 = k[65] * r100;
		// 102
		const float p105 = sat(k[66] + p104);
		const float r106 = w24(p105);   // r01
		// 103
		// 104
		const float t107 = tv_plain(p105);   // t1
		// 105
		const float p108 = sat(t107 * r106);
		const float r109 = w24(p108);   // r02
		// 106
		// 107
		// 108
		const float p110 = sat(t107 * r109);
		const float r111 = w24(p110);   // r03
		// 109
		const float p112 = k[73] * r106;
		// 10a
		const float p113 = k[74] * r109 + p112;
		// 10b
		const float p114 = sat((k[75] * r111 + p113) * 2.0f);
		const float r115 = w24(p114);   // r27
		// 10c
		const float p116 = k[76] * m4;
		const float m117 = m4;   // m12
		// 10d
		const float p118 = k[77] * s_m12 + p116;
		// 10e
		const float p119 = sat((k[78] * s_r36 + p118) * 2.0f);
		const float r120 = w24(p119);   // r36
		// 10f
		const float p121 = k[79] * r98;
		// 110
		const float p122 = (k[80] * r103 + p121) * 2.0f;
		// 111
		const float p123 = sat(k[81] * r120 + p122);
		const float w124 = p123;
		// 112
		const float p125 = k[82] * m23;
		// 113
		const float p126 = sat(k[83] * m17 + p125);
		const float r127 = w24(p126);   // r01
		// 114
		const float p128 = k[84] * m17;
		ram[at(84)] = w124;
		// 115
		const float p129 = sat(k[85] * m10 + p128);
		const float r130 = w24(p129);   // r02
		// 116
		// 117
		const float p131 = k[87] * r98;
		// 118
		const float p132 = sat((k[88] * r127 + p131) * 4.0f);
		const float m133 = w24(p132);   // m28
		// 119
		const float p134 = k[89] * r103;
		// 11a
		const float p135 = sat((k[90] * r130 + p134) * 4.0f);
		const float m136 = w24(p135);   // m29
		// 11b
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m133;   // m28
		out[1] = m136;   // m29
		s_p = p135;
		s_r21 = r11;
		s_r20 = r5;
		s_r22 = r15;
		s_r23 = r21;
		s_r24 = r25;
		s_r25 = r27;
		s_r27 = r115;
		s_r2a = r43;
		s_r28 = r35;
		s_r29 = r39;
		s_r2b = r46;
		s_r2c = r50;
		s_r2d = r53;
		s_r2e = r55;
		s_r2f = r59;
		s_r30 = r63;
		s_r31 = r67;
		s_r32 = r70;
		s_r33 = r75;
		s_r34 = r78;
		s_r35 = r80;
		s_r26 = r86;
		s_m12 = m117;
		s_r36 = r120;
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
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
	float s_r36 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_VDIST_H
