// license:BSD-3-Clause
// S-MU2000: インサーション 1: ISOLATOR
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_ISOLATOR_H
#define S_MU2000_DSP_MEG_FX_INS_ISOLATOR_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_isolator : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; s_r39 = 0; s_r3a = 0; s_r3b = 0; s_r3c = 0; s_r3d = 0; s_r3e = 0; s_r3f = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0];
		const float r2 = w24(p1);   // r03
		// 0c1
		const float p3 = (k[1] * s_m12) * 2.0f;
		const float m4 = s_m12;   // m05
		// 0c2
		const float p5 = s_r20 + p3;
		// 0c3
		const float p6 = sat((k[3] * s_m12 + p5) * 2.0f);
		const float r7 = w24(p6);   // r01
		// 0c4
		const float p8 = (k[4] * s_m14) * 2.0f;
		const float m9 = s_m14;   // m06
		// 0c5
		const float p10 = s_r30 + p8;
		// 0c6
		const float p11 = sat((k[6] * s_m14 + p10) * 2.0f);
		const float r12 = w24(p11);   // r02
		// 0c7
		const float p13 = (k[7] * s_m13) * 2.0f;
		const float m14 = s_m13;   // m07
		// 0c8
		const float p15 = s_r29 + p13;
		// 0c9
		const float p16 = sat((k[9] * s_m13 + p15) * 2.0f);
		const float m17 = w24(p16);   // m03
		// 0ca
		const float p18 = (k[10] * s_m15) * 2.0f;
		const float m19 = s_m15;   // m08
		// 0cb
		const float p20 = s_r39 + p18;
		// 0cc
		const float p21 = sat((k[12] * s_m15 + p20) * 2.0f);
		const float m22 = w24(p21);   // m04
		// 0cd
		const float p23 = sat(k[13] * r7 + in[0]);
		const float r24 = w24(p23);   // r04
		// 0ce
		const float p25 = sat(k[14] * r12 + in[1]);
		const float r26 = w24(p25);   // r05
		// 0cf
		const float p27 = sat(k[15] * r2);
		const int32_t i28 = idx_of(p27);
		// 0d0
		const float p29 = sat(k[16] * r24);
		const float r30 = w24(p29);   // r21
		// 0d1
		const float p31 = k[17] * s_r22;
		const float t32 = tv_index(p27);   // t2
		// 0d2
		const float p33 = k[18] * s_r21 + p31;
		const float r34 = s_r21;   // r22
		const float q35 = ram[at(18, i28)];
		// 0d3
		const float p36 = k[19] * r30 + p33;
		// 0d4
		const float p37 = k[20] * s_r23 + p36;
		const float m38 = q35;   // m01
		const float r39 = s_r23;   // r24
		// 0d5
		const float p40 = sat((k[21] * s_r24 + p37) * 2.0f);
		const float r41 = w24(p40);   // r23
		const float q42 = ram[at(21, i28 + 1)];
		// 0d6
		const float p43 = sat(k[22] * r30 + s_m12);
		const float m44 = w24(p43);   // m12
		const float r45 = w24(p43);   // r04
		// 0d7
		const float p46 = sat(k[23] * r26);
		const float m47 = q42;   // m02
		const float r48 = w24(p46);   // r31
		// 0d8
		const float p49 = k[24] * s_r32;
		// 0d9
		const float p50 = k[25] * s_r31 + p49;
		const float r51 = s_r31;   // r32
		// 0da
		const float p52 = k[26] * r48 + p50;
		// 0db
		const float p53 = k[27] * s_r33 + p52;
		const float r54 = s_r33;   // r34
		// 0dc
		const float p55 = sat((k[28] * s_r34 + p53) * 2.0f);
		const float r56 = w24(p55);   // r33
		// 0dd
		const float p57 = sat(k[29] * r48 + s_m14);
		const float m58 = w24(p57);   // m14
		const float r59 = w24(p57);   // r05
		// 0de
		const float p60 = sat(r45 + m4);
		const float m61 = w24(p60);   // m05
		// 0df
		const float p62 = sat(k[31] * m17 + r41);
		const float r63 = w24(p62);   // r06
		// 0e0
		const float p64 = sat(k[32] * m22 + r56);
		const float r65 = w24(p64);   // r07
		// 0e1
		const float p66 = sat(r59 + m9);
		const float m67 = w24(p66);   // m06
		// 0e2
		const float p68 = sat(k[34] * m61 + s_r20);
		const float r69 = w24(p68);   // r20
		// 0e3
		const float p70 = sat(k[35] * r2);
		const int32_t i71 = idx_of(p70);
		// 0e4
		const float p72 = s_r20;
		// 0e5
		const float p73 = sat(r69 + p72);
		const float r74 = w24(p73);   // r25
		const float t75 = tv_index(p70);   // t3
		// 0e6
		const float p76 = k[38] * s_r26;
		// 0e7
		const float p77 = k[39] * s_r25 + p76;
		const float r78 = s_r25;   // r26
		const float q79 = ram[at(39, i71)];
		// 0e8
		const float p80 = k[40] * r74 + p77;
		// 0e9
		const float p81 = k[41] * s_r27 + p80;
		const float m82 = q79;   // m03
		const float r83 = s_r27;   // r28
		// 0ea
		const float p84 = sat((k[42] * s_r28 + p81) * 2.0f);
		const float r85 = w24(p84);   // r27
		const float w86 = p84;
		const float q87 = ram[at(42, i71 + 1)];
		// 0eb
		const float p88 = sat(k[43] * m67 + s_r30);
		const float r89 = w24(p88);   // r30
		// 0ec
		const float p90 = sat(k[44] * in[0]);
		const float m91 = w24(p90);   // m05
		// 0ed
		const float p92 = s_r30;
		const float m93 = q87;   // m04
		ram[at(45)] = w86;
		// 0ee
		const float p94 = sat(r89 + p92);
		const float r95 = w24(p94);   // r35
		// 0ef
		const float p96 = k[47] * s_r38;
		// 0f0
		const float p97 = k[48] * s_r35 + p96;
		const float r98 = s_r35;   // r38
		// 0f1
		const float p99 = k[49] * r95 + p97;
		// 0f2
		const float p100 = k[50] * s_r37 + p99;
		const float r101 = s_r37;   // r36
		// 0f3
		const float p102 = sat((k[51] * s_r36 + p100) * 2.0f);
		const float r103 = w24(p102);   // r37
		const float w104 = p102;
		// 0f4
		const float p105 = sat(k[52] * r63);
		const float r106 = w24(p105);   // r2a
		// 0f5
		const float p107 = k[53] * s_r2b;
		// 0f6
		const float p108 = k[54] * s_r2a + p107;
		const float r109 = s_r2a;   // r2b
		ram[at(54)] = w104;
		// 0f7
		const float p110 = k[55] * r106 + p108;
		// 0f8
		const float p111 = k[56] * s_r2c + p110;
		const float r112 = s_r2c;   // r2d
		// 0f9
		const float p113 = sat((k[57] * s_r2d + p111) * 2.0f);
		const float r114 = w24(p113);   // r2c
		// 0fa
		const float p115 = sat(k[58] * r65);
		const float r116 = w24(p115);   // r3a
		// 0fb
		const float p117 = k[59] * s_r3b;
		// 0fc
		const float p118 = k[60] * s_r3a + p117;
		const float r119 = s_r3a;   // r3b
		// 0fd
		const float p120 = k[61] * r116 + p118;
		// 0fe
		const float p121 = k[62] * s_r3c + p120;
		const float r122 = s_r3c;   // r3d
		// 0ff
		const float p123 = sat((k[63] * s_r3d + p121) * 2.0f);
		const float r124 = w24(p123);   // r3c
		// 100
		const float p125 = sat(k[64] * r106 + s_m13);
		const float m126 = w24(p125);   // m13
		const float r127 = w24(p125);   // r06
		// 101
		const float p128 = sat(k[65] * r116 + s_m15);
		const float m129 = w24(p128);   // m15
		const float r130 = w24(p128);   // r07
		// 102
		const float p131 = t32 * m38 - m38;
		// 103
		const float p132 = sat(t32 * m47 - p131);
		const float r133 = w24(p132);   // r08
		// 104
		const float p134 = sat(r127 + m14);
		const float m135 = w24(p134);   // m07
		// 105
		const float p136 = sat(r130 + m19);
		const float m137 = w24(p136);   // m08
		// 106
		const float p138 = t75 * m82 - m82;
		// 107
		const float p139 = sat(t75 * m93 - p138);
		const float r140 = w24(p139);   // r09
		// 108
		const float p141 = sat(k[72] * m135 + s_r29);
		const float r142 = w24(p141);   // r29
		// 109
		const float p143 = sat(k[73] * in[1]);
		const float m144 = w24(p143);   // m06
		// 10a
		const float p145 = s_r29;
		// 10b
		const float p146 = sat(r142 + p145);
		const float r147 = w24(p146);   // r06
		// 10c
		const float p148 = sat(k[76] * m137 + s_r39);
		const float r149 = w24(p148);   // r39
		// 10d
		// 10e
		const float p150 = s_r39;
		// 10f
		const float p151 = sat(r149 + p150);
		const float r152 = w24(p151);   // r07
		// 110
		const float p153 = k[80] * r147;
		// 111
		const float p154 = k[81] * s_r2e + p153;
		// 112
		const float p155 = sat((k[82] * s_r2f + p154) * 2.0f);
		const float r156 = w24(p155);   // r2e
		// 113
		const float p157 = k[83] * s_r2e + p155;
		const float r158 = s_r2e;   // r2f
		// 114
		const float p159 = sat(k[84] * s_r2f + p157);
		const float r160 = w24(p159);   // r06
		// 115
		const float p161 = k[85] * r152;
		// 116
		const float p162 = k[86] * s_r3e + p161;
		// 117
		const float p163 = sat((k[87] * s_r3f + p162) * 2.0f);
		const float r164 = w24(p163);   // r3e
		// 118
		const float p165 = k[88] * s_r3e + p163;
		const float r166 = s_r3e;   // r3f
		// 119
		const float p167 = sat(k[89] * s_r3f + p165);
		const float r168 = w24(p167);   // r07
		// 11a
		const float p169 = k[90] * r133 + m91;
		// 11b
		const float p170 = (k[91] * r160 + p169) * 2.0f;
		// 11c
		const float p171 = sat((k[92] * r114 + p170) * 16.0f);
		const float m172 = w24(p171);   // m28
		// 11d
		const float p173 = k[93] * r140 + m144;
		// 11e
		const float p174 = (k[94] * r168 + p173) * 2.0f;
		// 11f
		const float p175 = sat((k[95] * r124 + p174) * 16.0f);
		const float m176 = w24(p175);   // m29
		out[0] = m172;   // m28
		out[1] = m176;   // m29
		s_p = p175;
		s_m12 = m44;
		s_r20 = r69;
		s_m14 = m58;
		s_r30 = r89;
		s_m13 = m126;
		s_r29 = r142;
		s_m15 = m129;
		s_r39 = r149;
		s_r22 = r34;
		s_r21 = r30;
		s_r23 = r41;
		s_r24 = r39;
		s_r32 = r51;
		s_r31 = r48;
		s_r33 = r56;
		s_r34 = r54;
		s_r26 = r78;
		s_r25 = r74;
		s_r27 = r85;
		s_r28 = r83;
		s_r38 = r98;
		s_r35 = r95;
		s_r37 = r103;
		s_r36 = r101;
		s_r2b = r109;
		s_r2a = r106;
		s_r2c = r114;
		s_r2d = r112;
		s_r3b = r119;
		s_r3a = r116;
		s_r3c = r124;
		s_r3d = r122;
		s_r2e = r156;
		s_r2f = r158;
		s_r3e = r164;
		s_r3f = r166;
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
	float s_r34 = 0.0f;
	float s_r35 = 0.0f;
	float s_r36 = 0.0f;
	float s_r37 = 0.0f;
	float s_r38 = 0.0f;
	float s_r39 = 0.0f;
	float s_r3a = 0.0f;
	float s_r3b = 0.0f;
	float s_r3c = 0.0f;
	float s_r3d = 0.0f;
	float s_r3e = 0.0f;
	float s_r3f = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_ISOLATOR_H
