// license:BSD-3-Clause
// S-MU2000: インサーション 1: DUAL ROTR1, DUAL ROTR2
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m29 m28 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_DUAL_ROTARY_H
#define S_MU2000_DSP_MEG_FX_INS_DUAL_ROTARY_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_dual_rotary : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x3f000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x29, 0x28 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; s_r39 = 0; s_r3a = 0; s_r3b = 0; s_r3c = 0; s_r3d = 0; s_r3e = 0; s_r3f = 0; }

	// in: 入口（m29 m28）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * s_m12;
		const float m2 = lfo[12];   // m34
		// 0c1
		const float p3 = k[1] + p1;
		const float r4 = w24(p3);   // r09
		// 0c2
		const float p5 = sat(k[2] * s_r38);
		const int32_t i6 = idx_of(p5);
		// 0c3
		const float p7 = sat(k[3] * s_m12);
		// 0c4
		const float p8 = sat(k[4] * s_r39);
		const int32_t j9 = idx_of(p8);
		const float t10 = tv_index(p5);   // t1
		// 0c5
		const float p11 = k[5] * s_m12;
		const float r12 = w24(p11);   // r38
		const float t13 = tv_plain(p7);   // t3
		// 0c6
		const float p14 = k[6] * r4 + p11;
		const float r15 = w24(p14);   // r39
		const float t16 = tv_index(p8);   // t2
		const float q17 = ram[at(6, i6)];
		// 0c7
		const float p18 = k[7] * s_r30;
		// 0c8
		const float p19 = k[8] * s_r31 + p18;
		const float m20 = q17;   // m01
		// 0c9
		const float p21 = k[9] * s_r32 + p19;
		const float q22 = ram[at(9, i6 + 1)];
		// 0ca
		const float p23 = k[10] * s_r34 + p21;
		// 0cb
		const float p24 = k[11] * s_r35 + p23;
		const float m25 = q22;   // m02
		// 0cc
		const float p26 = sat(k[12] * s_r36 + p24);
		const float r27 = w24(p26);   // r2e
		const float q28 = ram[at(12, j9)];
		// 0cd
		const float p29 = t10 * m20 - m20;
		// 0ce
		const float p30 = sat(t10 * m25 - p29);
		const float m31 = q28;   // m03
		const float r32 = w24(p30);   // r01
		// 0cf
		const float p33 = sat(k[15] * s_r3a);
		const int32_t i34 = idx_of(p33);
		const float q35 = ram[at(15, j9 + 1)];
		// 0d0
		const float p36 = sat(k[16] * s_r3b);
		const float m37 = lfo[13];   // m35
		const int32_t j38 = idx_of(p36);
		// 0d1
		const float p39 = k[17] * s_m13;
		const float m40 = q35;   // m04
		const float r41 = w24(p39);   // r3a
		const float t42 = tv_index(p33);   // t1
		// 0d2
		const float p43 = k[18] * s_m14 + p39;
		const float r44 = w24(p43);   // r3b
		const float t45 = tv_index(p36);   // t5
		const float q46 = ram[at(18, i34)];
		// 0d3
		const float p47 = k[19] * s_r30;
		// 0d4
		const float p48 = k[20] * s_r31 + p47;
		const float m49 = q46;   // m05
		// 0d5
		const float p50 = k[21] * s_r33 + p48;
		const float q51 = ram[at(21, i34 + 1)];
		// 0d6
		const float p52 = k[22] * s_r34 + p50;
		// 0d7
		const float p53 = k[23] * s_r35 + p52;
		const float m54 = q51;   // m06
		// 0d8
		const float p55 = sat(k[24] * s_r37 + p53);
		const float r56 = w24(p55);   // r2f
		const float q57 = ram[at(24, j38)];
		// 0d9
		const float p58 = sat(k[25] * r4);
		// 0da
		const float p59 = t16 * m31 - m31;
		const float m60 = q57;   // m07
		// 0db
		const float p61 = sat(t16 * m40 - p59);
		const float r62 = w24(p61);   // r02
		const float t63 = tv_plain(p58);   // t2
		const float q64 = ram[at(27, j38 + 1)];
		// 0dc
		const float p65 = k[28] * m2;
		// 0dd
		const float p66 = k[29] + p65;
		const float m67 = q64;   // m08
		const float r68 = w24(p66);   // r09
		// 0de
		const float p69 = sat(k[30] * s_r3c);
		const int32_t i70 = idx_of(p69);
		// 0df
		const float p71 = t42 * m49 - m49;
		// 0e0
		const float p72 = sat(t42 * m54 - p71);
		const float m73 = lfo[14];   // m36
		const float r74 = w24(p72);   // r03
		const float t75 = tv_index(p69);   // t1
		// 0e1
		const float p76 = t13 * r32 + r32;
		const float r77 = w24(p76);   // r30
		const float q78 = ram[at(33, i70)];
		// 0e2
		const float p79 = t45 * m60 - m60;
		// 0e3
		const float p80 = sat(t45 * m67 - p79);
		const float m81 = q78;   // m01
		const float r82 = w24(p80);   // r04
		// 0e4
		const float p83 = sat(k[36] * s_r3d);
		const int32_t j84 = idx_of(p83);
		const float q85 = ram[at(36, i70 + 1)];
		// 0e5
		const float p86 = t63 * r62 + r62;
		const float r87 = w24(p86);   // r31
		// 0e6
		const float p88 = k[38] * in[0];
		const float m89 = q85;   // m02
		const float t90 = tv_index(p83);   // t2
		// 0e7
		const float p91 = sat((k[39] * in[1] + p88) * 2.0f);
		const float r92 = w24(p91);   // r20
		const float q93 = ram[at(39, j84)];
		// 0e8
		const float p94 = k[40] * s_r20;
		const float r95 = s_r20;   // r08
		// 0e9
		const float p96 = k[41] * s_r21 + p94;
		const float m97 = q93;   // m03
		// 0ea
		const float p98 = k[42] * s_r22 + p96;
		const float q99 = ram[at(42, j84 + 1)];
		// 0eb
		const float p100 = k[43] * s_r23 + p98;
		// 0ec
		const float p101 = sat((k[44] * r92 + p100) * 2.0f);
		const float m102 = q99;   // m04
		const float r103 = w24(p101);   // r22
		// 0ed
		const float p104 = k[45] * s_r22;
		const float r105 = s_r22;   // r23
		// 0ee
		const float p106 = k[46] * s_r23 + p104;
		// 0ef
		const float p107 = k[47] * s_r24 + p106;
		// 0f0
		const float p108 = k[48] * s_r25 + p107;
		const float m109 = lfo[15];   // m12
		// 0f1
		const float p110 = sat((k[49] * r103 + p108) * 2.0f);
		const float r111 = w24(p110);   // r24
		// 0f2
		const float p112 = k[50] * s_r24;
		const float r113 = s_r24;   // r25
		// 0f3
		const float p114 = k[51] * s_r26 + p112;
		// 0f4
		const float p115 = sat((k[52] * r111 + p114) * 16.0f);
		const float r116 = w24(p115);   // r26
		// 0f5
		const float p117 = k[53] * m2;
		const float r118 = w24(p117);   // r3c
		// 0f6
		const float p119 = k[54] * r68;
		const float r120 = w24(p119);   // r3d
		// 0f7
		const float p121 = t75 * m81 - m81;
		// 0f8
		const float p122 = sat(t75 * m89 - p121);
		const float r123 = w24(p122);   // r05
		// 0f9
		const float p124 = t90 * m97 - m97;
		// 0fa
		const float p125 = sat(t90 * m102 - p124);
		const float r126 = w24(p125);   // r06
		// 0fb
		const float p127 = sat((k[59] * r116) * 2.0f);
		const float w128 = p127;
		// 0fc
		const float p129 = sat(k[60] * s_r3e);
		const int32_t i130 = idx_of(p129);
		// 0fd
		const float p131 = sat(k[61] * s_r3f);
		const int32_t j132 = idx_of(p131);
		// 0fe
		const float p133 = k[62] * r95;
		const float r134 = r95;   // r21
		const float t135 = tv_index(p129);   // t1
		// 0ff
		const float p136 = k[63] * s_r21 + p133;
		const float t137 = tv_index(p131);   // t5
		ram[at(63)] = w128;
		// 100
		const float p138 = k[64] * s_r28 + p136;
		const float m139 = lfo[16];   // m13
		// 101
		const float p140 = k[65] * s_r29 + p138;
		// 102
		const float p141 = sat((k[66] * r92 + p140) * 2.0f);
		const float r142 = w24(p141);   // r28
		const float q143 = ram[at(66, i130)];
		// 103
		const float p144 = k[67] * s_r28;
		const float r145 = s_r28;   // r29
		// 104
		const float p146 = k[68] * s_r29 + p144;
		const float m147 = q143;   // m05
		// 105
		const float p148 = k[69] * s_r2a + p146;
		const float q149 = ram[at(69, i130 + 1)];
		// 106
		const float p150 = k[70] * s_r2b + p148;
		// 107
		const float p151 = sat((k[71] * r142 + p150) * 2.0f);
		const float m152 = q149;   // m06
		const float r153 = w24(p151);   // r2a
		// 108
		const float p154 = k[72] * s_r2a;
		const float r155 = s_r2a;   // r2b
		const float q156 = ram[at(72, j132)];
		// 109
		const float p157 = k[73] * s_r2c + p154;
		// 10a
		const float p158 = sat((k[74] * r153 + p157) * 16.0f);
		const float m159 = q156;   // m07
		const float r160 = w24(p158);   // r2c
		// 10b
		const float p161 = k[75] * m37;
		const float r162 = w24(p161);   // r3e
		const float q163 = ram[at(75, j132 + 1)];
		// 10c
		const float p164 = k[76] * m73;
		const float r165 = w24(p164);   // r3f
		// 10d
		const float p166 = t135 * m147 - m147;
		const float m167 = q163;   // m08
		// 10e
		const float p168 = sat(t135 * m152 - p166);
		const float r169 = w24(p168);   // r07
		// 10f
		const float p170 = sat((k[79] * r160) * 2.0f);
		const float w171 = p170;
		// 110
		const float p172 = t137 * m159 - m159;
		const float m173 = lfo[17];   // m14
		// 111
		const float p174 = sat(t137 * m167 - p172);
		const float r175 = w24(p174);   // r08
		ram[at(81)] = w171;
		// 112
		const float p176 = sat(k[82] * m139);
		// 113
		const float p177 = sat(k[83] * m173);
		// 114
		const float p178 = sat(k[84] * m2);
		const float t179 = tv_plain(p176);   // t1
		// 115
		const float p180 = sat(k[85] * r68);
		const float t181 = tv_plain(p177);   // t2
		// 116
		const float p182 = sat(k[86] * m37);
		const float t183 = tv_plain(p178);   // t3
		// 117
		const float p184 = sat(k[87] * m73);
		const float t185 = tv_plain(p180);   // t5
		// 118
		const float p186 = t179 * r74 + r74;
		const float r187 = w24(p186);   // r32
		const float t188 = tv_plain(p182);   // t1
		// 119
		const float p189 = t181 * r82 + r82;
		const float r190 = w24(p189);   // r33
		const float t191 = tv_plain(p184);   // t2
		// 11a
		const float p192 = sat((k[90] * r27) * 4.0f);
		const float m193 = w24(p192);   // m28
		// 11b
		const float p194 = sat((k[91] * r56) * 4.0f);
		const float m195 = w24(p194);   // m29
		// 11c
		const float p196 = t183 * r123 + r123;
		const float r197 = w24(p196);   // r34
		// 11d
		const float p198 = t185 * r126 + r126;
		const float r199 = w24(p198);   // r35
		// 11e
		const float p200 = t188 * r169 + r169;
		const float r201 = w24(p200);   // r36
		// 11f
		const float p202 = t191 * r175 + r175;
		const float r203 = w24(p202);   // r37
		out[0] = m193;   // m28
		out[1] = m195;   // m29
		s_p = p202;
		s_m12 = m109;
		s_r38 = r12;
		s_r39 = r15;
		s_r30 = r77;
		s_r31 = r87;
		s_r32 = r187;
		s_r34 = r197;
		s_r35 = r199;
		s_r36 = r201;
		s_r3a = r41;
		s_r3b = r44;
		s_m13 = m139;
		s_m14 = m173;
		s_r33 = r190;
		s_r37 = r203;
		s_r3c = r118;
		s_r3d = r120;
		s_r20 = r92;
		s_r21 = r134;
		s_r22 = r103;
		s_r23 = r105;
		s_r24 = r111;
		s_r25 = r113;
		s_r26 = r116;
		s_r3e = r162;
		s_r3f = r165;
		s_r28 = r142;
		s_r29 = r145;
		s_r2a = r153;
		s_r2b = r155;
		s_r2c = r160;
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
	float s_r28 = 0.0f;
	float s_r29 = 0.0f;
	float s_r2a = 0.0f;
	float s_r2b = 0.0f;
	float s_r2c = 0.0f;
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

#endif // S_MU2000_DSP_MEG_FX_INS_DUAL_ROTARY_H
