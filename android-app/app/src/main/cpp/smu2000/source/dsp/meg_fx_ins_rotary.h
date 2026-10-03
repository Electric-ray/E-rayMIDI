// license:BSD-3-Clause
// S-MU2000: インサーション 1: ROTARY SP, TREMOLO, AUTO PAN, 2WAY ROTRY, AMBIENCE
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_ROTARY_H
#define S_MU2000_DSP_MEG_FX_INS_ROTARY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_rotary : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0xf000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat(k[0] * s_r30);
		const float m2 = lfo[12];   // m34
		const int32_t i3 = idx_of(p1);
		// 0c1
		const float p4 = sat((k[1] * s_r33) * 16.0f);
		// 0c2
		const float p5 = k[2] * s_r34 + p4;
		const float t6 = tv_index(p1);   // t1
		// 0c3
		const float p7 = satpos(k[3] + p5);
		const float r8 = w24(p7);   // r08
		const float q9 = ram[at(3, i3)];
		// 0c4
		const float p10 = sat((k[4] * s_r35) * 16.0f);
		// 0c5
		const float p11 = k[5] * s_r36 + p10;
		const float m12 = q9;   // m01
		// 0c6
		const float p13 = satpos(k[6] + p11);
		const float r14 = w24(p13);   // r09
		const float q15 = ram[at(6, i3 + 1)];
		// 0c7
		const float p16 = sat(k[7] * r8);
		const int32_t i17 = idx_of(p16);
		// 0c8
		const float p18 = sat((k[8] + m2) * 2.0f);
		const float m19 = q15;   // m02
		const float r20 = w24(p18);   // r05
		// 0c9
		const float p21 = k[9] * s_r30;
		const float t22 = tv_index(p16);   // t3
		// 0ca
		const float p23 = t6 * m12 - m12;
		// 0cb
		const float p24 = sat(t6 * m19 - p23);
		const float r25 = w24(p24);   // r01
		const float t26 = tv_plain(p21);   // t1
		// 0cc
		const float p27 = sat((k[12] * r20) * 2.0f);
		const float r28 = w24(p27);   // r05
		const float q29 = ram[at(12, i17)];
		// 0cd
		const float p30 = sat(k[13] * r14);
		const int32_t i31 = idx_of(p30);
		// 0ce
		const float p32 = sat(t26 * r25 + r25);
		const float m33 = q29;   // m05
		const float r34 = w24(p32);   // r01
		const float t35 = tv_plain(p27);   // t1
		// 0cf
		const float p36 = sat((k[15] * s_r31) * 16.0f);
		const float t37 = tv_index(p30);   // t5
		const float q38 = ram[at(15, i17 + 1)];
		// 0d0
		const float p39 = k[16] * s_r32 + p36;
		const float m40 = lfo[13];   // m35
		// 0d1
		const float p41 = satpos(k[17] + p39);
		const float m42 = q38;   // m06
		const float r43 = w24(p41);   // r07
		// 0d2
		const float p44 = sat(k[18] * r8);
		const float q45 = ram[at(18, i31)];
		// 0d3
		const float p46 = t22 * m33 - m33;
		// 0d4
		const float p47 = sat(t22 * m42 - p46);
		const float m48 = q45;   // m07
		const float r49 = w24(p47);   // r03
		const float t50 = tv_plain(p44);   // t3
		// 0d5
		const float p51 = sat(k[21] * r43);
		const int32_t i52 = idx_of(p51);
		const float q53 = ram[at(21, i31 + 1)];
		// 0d6
		const float p54 = sat(t35 * r28);
		const float r55 = w24(p54);   // r06
		// 0d7
		const float p56 = k[23] * in[0];
		const float m57 = q53;   // m08
		const float t58 = tv_index(p51);   // t2
		// 0d8
		const float p59 = sat((k[24] * in[1] + p56) * 2.0f);
		const float r60 = w24(p59);   // r20
		const float q61 = ram[at(24, i52)];
		// 0d9
		const float p62 = k[25] * s_r20;
		const float r63 = s_r20;   // r21
		// 0da
		const float p64 = k[26] * s_r21 + p62;
		const float m65 = q61;   // m03
		// 0db
		const float p66 = k[27] * s_r22 + p64;
		const float q67 = ram[at(27, i52 + 1)];
		// 0dc
		const float p68 = k[28] * s_r23 + p66;
		// 0dd
		const float p69 = sat((k[29] * r60 + p68) * 2.0f);
		const float m70 = q67;   // m04
		const float r71 = w24(p69);   // r22
		// 0de
		const float p72 = k[30] * s_r22;
		const float r73 = s_r22;   // r23
		// 0df
		const float p74 = k[31] * s_r23 + p72;
		// 0e0
		const float p75 = k[32] * s_r24 + p74;
		const float m76 = lfo[14];   // m36
		// 0e1
		const float p77 = k[33] * s_r25 + p75;
		// 0e2
		const float p78 = sat((k[34] * r71 + p77) * 2.0f);
		const float r79 = w24(p78);   // r24
		// 0e3
		const float p80 = k[35] * s_r24;
		const float r81 = s_r24;   // r25
		// 0e4
		const float p82 = k[36] * s_r25 + p80;
		// 0e5
		const float p83 = k[37] * s_r26 + p82;
		const float r84 = s_r26;   // r27
		// 0e6
		const float p85 = k[38] * s_r27 + p83;
		// 0e7
		const float p86 = sat((k[39] * r79 + p85) * 16.0f);
		const float r87 = w24(p86);   // r26
		// 0e8
		const float p88 = t35 * r55;
		const float r89 = w24(p88);   // r06
		// 0e9
		const float p90 = k[41] * in[1];
		// 0ea
		const float p91 = sat((k[42] * in[0] + p90) * 2.0f);
		const float r92 = w24(p91);   // r28
		// 0eb
		const float p93 = k[43] * s_r28;
		const float r94 = s_r28;   // r29
		// 0ec
		const float p95 = k[44] * s_r29 + p93;
		// 0ed
		const float p96 = k[45] * s_r2a + p95;
		// 0ee
		const float p97 = k[46] * s_r2b + p96;
		// 0ef
		const float p98 = sat((k[47] * r92 + p97) * 2.0f);
		const float r99 = w24(p98);   // r2a
		// 0f0
		const float p100 = k[48] * s_r2a;
		const float m101 = lfo[15];   // m37
		const float r102 = s_r2a;   // r2b
		// 0f1
		const float p103 = k[49] * s_r2b + p100;
		// 0f2
		const float p104 = k[50] * s_r2c + p103;
		// 0f3
		const float p105 = k[51] * s_r2d + p104;
		// 0f4
		const float p106 = sat((k[52] * r99 + p105) * 2.0f);
		const float r107 = w24(p106);   // r2c
		// 0f5
		const float p108 = k[53] * s_r2c;
		const float r109 = s_r2c;   // r2d
		// 0f6
		const float p110 = k[54] * s_r2d + p108;
		// 0f7
		const float p111 = k[55] * s_r2e + p110;
		const float r112 = s_r2e;   // r2f
		// 0f8
		const float p113 = k[56] * s_r2f + p111;
		// 0f9
		const float p114 = sat((k[57] * r107 + p113) * 16.0f);
		const float r115 = w24(p114);   // r2e
		// 0fa
		const float p116 = sat(t50 * r49 + r49);
		const float r117 = w24(p116);   // r03
		// 0fb
		const float p118 = k[59] * r87;
		// 0fc
		const float p119 = sat(k[60] * r115 + p118);
		const float w120 = p119;
		// 0fd
		const float p121 = sat(k[61] * r14);
		// 0fe
		const float p122 = t37 * m48 - m48;
		// 0ff
		const float p123 = sat(t37 * m57 - p122);
		const float r124 = w24(p123);   // r04
		const float t125 = tv_plain(p121);   // t5
		ram[at(63)] = w120;
		// 100
		const float p126 = sat(k[64] * r115);
		const float w127 = p126;
		// 101
		const float p128 = k[65] * r43;
		// 102
		const float p129 = t58 * m65 - m65;
		ram[at(66)] = w127;
		// 103
		const float p130 = sat(t58 * m70 - p129);
		const float r131 = w24(p130);   // r02
		const float t132 = tv_plain(p128);   // t2
		// 104
		const float p133 = sat(t125 * r124 + r124);
		const float r134 = w24(p133);   // r04
		// 105
		const float p135 = k[69] * r34;
		// 106
		const float p136 = k[70] * r117 + p135;
		// 107
		const float p137 = sat(k[71] * r134 + p136);
		const float r138 = w24(p137);   // r01
		// 108
		const float p139 = sat(t132 * r131 + r131);
		const float r140 = w24(p139);   // r02
		// 109
		const float p141 = k[73] * r117;
		// 10a
		const float p142 = k[74] * r134 + p141;
		// 10b
		const float p143 = sat(k[75] * r140 + p142);
		const float r144 = w24(p143);   // r02
		// 10c
		const float p145 = sat((k[76] * r28) * 16.0f);
		// 10d
		const float p146 = k[77] * r89 + p145;
		// 10e
		const float p147 = satpos(k[78] + p146);
		const float r148 = w24(p147);   // r30
		// 10f
		const float p149 = sat((k[79] + m40) * 2.0f);
		const float r150 = w24(p149);   // r07
		// 110
		const float p151 = sat((k[80] + m76) * 2.0f);
		const float r152 = w24(p151);   // r08
		// 111
		const float p153 = sat((k[81] + m101) * 2.0f);
		const float r154 = w24(p153);   // r09
		// 112
		const float p155 = sat((k[82] * r150) * 2.0f);
		const float r156 = w24(p155);   // r31
		// 113
		const float p157 = sat((k[83] * r152) * 2.0f);
		const float r158 = w24(p157);   // r33
		// 114
		const float p159 = sat((k[84] * r154) * 2.0f);
		const float r160 = w24(p159);   // r35
		const float t161 = tv_plain(p155);   // t2
		// 115
		const float p162 = k[85] * r87;
		const float t163 = tv_plain(p157);   // t3
		// 116
		const float p164 = sat((k[86] * r138 + p162) * 4.0f);
		const float m165 = w24(p164);   // m28
		const float t166 = tv_plain(p159);   // t5
		// 117
		const float p167 = k[87] * r115;
		// 118
		const float p168 = sat((k[88] * r144 + p167) * 4.0f);
		const float m169 = w24(p168);   // m29
		// 119
		const float p170 = sat(t161 * r156);
		const float r171 = w24(p170);   // r07
		// 11a
		const float p172 = sat(t163 * r158);
		const float r173 = w24(p172);   // r08
		// 11b
		const float p174 = sat(t166 * r160);
		const float r175 = w24(p174);   // r09
		// 11c
		const float p176 = sat(t161 * r171);
		const float r177 = w24(p176);   // r32
		// 11d
		const float p178 = sat(t163 * r173);
		const float r179 = w24(p178);   // r34
		// 11e
		const float p180 = sat(t166 * r175);
		const float r181 = w24(p180);   // r36
		// 11f
		out[0] = m165;   // m28
		out[1] = m169;   // m29
		s_p = p180;
		s_r30 = r148;
		s_r33 = r158;
		s_r34 = r179;
		s_r35 = r160;
		s_r36 = r181;
		s_r31 = r156;
		s_r32 = r177;
		s_r20 = r60;
		s_r21 = r63;
		s_r22 = r71;
		s_r23 = r73;
		s_r24 = r79;
		s_r25 = r81;
		s_r26 = r87;
		s_r27 = r84;
		s_r28 = r92;
		s_r29 = r94;
		s_r2a = r99;
		s_r2b = r102;
		s_r2c = r107;
		s_r2d = r109;
		s_r2e = r115;
		s_r2f = r112;
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
	float s_r30 = 0.0f;
	float s_r31 = 0.0f;
	float s_r32 = 0.0f;
	float s_r33 = 0.0f;
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
	float s_r36 = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_ROTARY_H
