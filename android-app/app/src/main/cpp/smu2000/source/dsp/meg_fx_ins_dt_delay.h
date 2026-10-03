// license:BSD-3-Clause
// S-MU2000: インサーション 1: DT+DELAY, OD+DELAY, CMP+DT+DLY, CMP+OD+DLY
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_DT_DELAY_H
#define S_MU2000_DSP_MEG_FX_INS_DT_DELAY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_dt_delay : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat((k[0] * s_m12) * 16.0f);
		const float r2 = w24(p1);   // r01
		const float q3 = ram[at(0)];
		// 0c1
		const float p4 = k[1] * in[0];
		// 0c2
		const float p5 = sat((k[2] * in[1] + p4) * 2.0f);
		const float m6 = q3;   // m06
		const float r7 = w24(p5);   // r20
		// 0c3
		const float p8 = k[3] * s_r21;
		const float q9 = ram[at(3)];
		// 0c4
		const float p10 = k[4] * s_r20 + p8;
		const float r11 = s_r20;   // r21
		// 0c5
		const float p12 = k[5] * r7 + p10;
		const float m13 = q9;   // m07
		// 0c6
		const float p14 = k[6] * s_r22 + p12;
		const float r15 = s_r22;   // r23
		const float q16 = ram[at(6)];
		// 0c7
		const float p17 = sat((k[7] * s_r23 + p14) * 2.0f);
		const float r18 = w24(p17);   // r22
		// 0c8
		const float p19 = sat((k[8] * r2) * 16.0f);
		const float m20 = q16;   // m08
		const float r21 = w24(p19);   // r01
		// 0c9
		const float q22 = ram[at(9)];
		// 0ca
		const float p23 = sat((k[10] * r18) * 2.0f);
		const float r24 = w24(p23);   // r09
		// 0cb
		const float p25 = sat((k[11] * r21) * 16.0f);
		const float m26 = q22;   // m09
		const float r27 = w24(p25);   // r01
		// 0cc
		const float p28 = k[12] * s_r26;
		const float r29 = s_r26;   // r27
		// 0cd
		const float p30 = k[13] * s_r27 + p28;
		// 0ce
		const float p31 = sat(k[14] * s_r28 + p30);
		const float r32 = w24(p31);   // r28
		// 0cf
		const float p33 = sat((k[15] * r27) * 16.0f);
		const float r34 = w24(p33);   // r02
		// 0d0
		const float p35 = satabs((k[16] * r24) * 16.0f);
		const float m36 = w24(p35);   // m01
		const float t37 = tv_plain(p31);   // t1
		// 0d1
		const float p38 = sat(t37 * r24);
		const float r39 = w24(p38);   // r08
		// 0d2
		const float p40 = sat((k[18] * r34) * 16.0f);
		const float r41 = w24(p40);   // r03
		// 0d3
		const float p42 = satpos(k[19] * s_r24 - m36);
		const float t43 = tv_plain(p38);   // t1
		// 0d4
		const float p44 = sat(k[20] * m36 + p42);
		const float r45 = w24(p44);   // r24
		// 0d5
		const float p46 = sat((k[21] * r41) * 16.0f);
		const float r47 = w24(p46);   // r04
		// 0d6
		const float p48 = sat(t43 * r24);
		const float r49 = w24(p48);   // r07
		// 0d7
		const float p50 = k[23] * r24;
		// 0d8
		const float p51 = k[24] * r39 + p50;
		// 0d9
		const float p52 = sat((k[25] * r49 + p51) * 16.0f);
		const float r53 = w24(p52);   // r29
		// 0da
		const float p54 = sat((k[26] * r47) * 16.0f);
		const float r55 = w24(p54);   // r05
		// 0db
		const float p56 = k[27] * s_r29;
		// 0dc
		const float p57 = k[28] * r53 + p56;
		// 0dd
		const float p58 = sat((k[29] * s_m12 + p57) * 2.0f);
		const float m59 = w24(p58);   // m12
		// 0de
		const float p60 = k[30] * s_r25;
		// 0df
		const float p61 = k[31] * s_r25 + (p60 * (1.0f / 32768.0f));
		// 0e0
		const float p62 = k[32] * m36 - p61;
		// 0e1
		const float p63 = satpos(k[33] * r45 - p62);
		// 0e2
		const float p64 = sat(k[34] * m36 + p63);
		const float r65 = w24(p64);   // r25
		// 0e3
		const float p66 = sat((k[35] * r55) * 16.0f);
		const float r67 = w24(p66);   // r01
		// 0e4
		const float p68 = k[36] * r27;
		// 0e5
		const float p69 = k[37] * r34 + p68;
		// 0e6
		const float p70 = k[38] * r41 + p69;
		// 0e7
		const float p71 = k[39] * r47 + p70;
		// 0e8
		const float p72 = k[40] * r55 + p71;
		// 0e9
		const float p73 = sat(k[41] * r67 + p72);
		const float r74 = w24(p73);   // r01
		// 0ea
		const float p75 = sat(k[42] * r65);
		const float r76 = w24(p75);   // r3f
		// 0eb
		// 0ec
		const float p77 = sat(k[44] * r74);
		const float r78 = w24(p77);   // r2a
		// 0ed
		const float p79 = k[45] * s_r2b;
		// 0ee
		const float p80 = k[46] * s_r2a + p79;
		const float r81 = s_r2a;   // r2b
		// 0ef
		const float p82 = k[47] * r78 + p80;
		// 0f0
		const float p83 = k[48] * s_r2c + p82;
		// 0f1
		const float p84 = sat((k[49] * s_r2d + p83) * 2.0f);
		const float r85 = w24(p84);   // r2c
		// 0f2
		const float p86 = k[50] * s_r2d;
		// 0f3
		const float p87 = k[51] * s_r2c + p86;
		const float r88 = s_r2c;   // r2d
		// 0f4
		const float p89 = k[52] * r85 + p87;
		// 0f5
		const float p90 = k[53] * s_r2e + p89;
		// 0f6
		const float p91 = sat((k[54] * s_r2f + p90) * 2.0f);
		const float r92 = w24(p91);   // r2e
		// 0f7
		const float p93 = k[55] * s_r2f;
		// 0f8
		const float p94 = k[56] * s_r2e + p93;
		const float r95 = s_r2e;   // r2f
		// 0f9
		const float p96 = k[57] * r92 + p94;
		// 0fa
		const float p97 = k[58] * s_r30 + p96;
		const float r98 = s_r30;   // r31
		// 0fb
		const float p99 = sat((k[59] * s_r31 + p97) * 16.0f);
		const float r100 = w24(p99);   // r30
		// 0fc
		const float p101 = k[60] * m6;
		const float m102 = m6;   // m13
		// 0fd
		const float p103 = k[61] * s_m13 + p101;
		// 0fe
		const float p104 = sat((k[62] * s_r32 + p103) * 2.0f);
		const float r105 = w24(p104);   // r32
		// 0ff
		const float p106 = k[63] * in[0];
		// 100
		const float p107 = sat((k[64] * r100 + p106) * 4.0f);
		const float r108 = w24(p107);   // r01
		// 101
		const float p109 = k[65] * in[1];
		// 102
		const float p110 = sat((k[66] * r100 + p109) * 4.0f);
		const float r111 = w24(p110);   // r02
		// 103
		const float p112 = k[67] * m26;
		// 104
		const float p113 = sat(k[68] * m20 + p112);
		const float r114 = w24(p113);   // r03
		// 105
		const float p115 = k[69] * m20;
		// 106
		const float p116 = sat(k[70] * m13 + p115);
		const float r117 = w24(p116);   // r04
		// 107
		const float p118 = k[71] * r108;
		// 108
		const float p119 = (k[72] * r111 + p118) * 2.0f;
		// 109
		const float p120 = sat(k[73] * r105 + p119);
		const float w121 = p120;
		// 10a
		const float p122 = k[74] * r108;
		// 10b
		const float p123 = sat((k[75] * r114 + p122) * 4.0f);
		const float m124 = w24(p123);   // m28
		ram[at(75)] = w121;
		// 10c
		const float p125 = k[76] * r111;
		// 10d
		const float p126 = sat((k[77] * r117 + p125) * 4.0f);
		const float m127 = w24(p126);   // m29
		// 10e
		const float p128 = satpos(k[78] + r76);
		const float r129 = w24(p128);   // r01
		// 10f
		const float p130 = satpos(k[79] + r76);
		const float r131 = w24(p130);   // r02
		// 110
		const float p132 = satpos(k[80] + r76);
		const float r133 = w24(p132);   // r03
		// 111
		const float p134 = satpos(k[81] + r76);
		const float r135 = w24(p134);   // r04
		// 112
		const float p136 = satpos(k[82] + r76);
		const float r137 = w24(p136);   // r05
		// 113
		const float p138 = satpos(k[83] + r76);
		const float r139 = w24(p138);   // r06
		// 114
		const float p140 = satpos(k[84] + r76);
		const float r141 = w24(p140);   // r07
		// 115
		const float p142 = satpos(k[85] + r76);
		const float r143 = w24(p142);   // r08
		// 116
		const float p144 = (k[86] * r129) * 2.0f;
		// 117
		const float p145 = (k[87] * r133 + p144) * 2.0f;
		// 118
		const float p146 = (k[88] * r135 + p145) * 2.0f;
		// 119
		const float p147 = (k[89] * r131 + p146) * 2.0f;
		// 11a
		const float p148 = (k[90] * r137 + p147) * 2.0f;
		// 11b
		const float p149 = (k[91] * r139 + p148) * 4.0f;
		// 11c
		const float p150 = k[92] * r141 + p149;
		// 11d
		const float p151 = k[93] * r143 + p150;
		// 11e
		const float p152 = satpos(k[94] + p151);
		const float r153 = w24(p152);   // r26
		// 11f
		out[0] = m124;   // m28
		out[1] = m127;   // m29
		s_p = p152;
		s_m12 = m59;
		s_r21 = r11;
		s_r20 = r7;
		s_r22 = r18;
		s_r23 = r15;
		s_r26 = r153;
		s_r27 = r29;
		s_r28 = r32;
		s_r24 = r45;
		s_r29 = r53;
		s_r25 = r65;
		s_r2b = r81;
		s_r2a = r78;
		s_r2c = r85;
		s_r2d = r88;
		s_r2e = r92;
		s_r2f = r95;
		s_r30 = r100;
		s_r31 = r98;
		s_m13 = m102;
		s_r32 = r105;
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
	float s_r30 = 0.0f;
	float s_r31 = 0.0f;
	float s_r32 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_DT_DELAY_H
