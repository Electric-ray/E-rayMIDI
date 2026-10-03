// license:BSD-3-Clause
// S-MU2000: インサーション 1: D.TURNTBL
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_TURNTABLE_H
#define S_MU2000_DSP_MEG_FX_INS_TURNTABLE_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_turntable : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x1f000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; s_r38 = 0; s_r39 = 0; s_r3a = 0; s_r3b = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * in[0];
		const float m2 = lfo[12];   // m03
		const float r3 = w24(p1);   // r20
		const float q4 = ram[at(0)];
		// 0c1
		const float p5 = k[1] * s_r20;
		// 0c2
		const float p6 = k[2] * s_r21 + p5;
		const float m7 = q4;   // m01
		// 0c3
		const float p8 = k[3] * s_r22 + p6;
		const float r9 = s_r22;   // r23
		// 0c4
		const float p10 = k[4] * s_r23 + p8;
		// 0c5
		const float p11 = sat((k[5] * r3 + p10) * 2.0f);
		const float r12 = w24(p11);   // r22
		// 0c6
		const float p13 = k[6];
		// 0c7
		const float p14 = (k[7] * s_r3b + p13) * 2.0f;
		const float r15 = w24(p14);   // r01
		// 0c8
		const float p16 = k[8];
		// 0c9
		const float p17 = (k[9] * m7 + p16) * 2.0f;
		const float r18 = w24(p17);   // r02
		const float w19 = p17;
		// 0ca
		const float p20 = k[10] * r3;
		const float r21 = r3;   // r24
		// 0cb
		const float p22 = k[11] * s_r24 + p20;
		// 0cc
		const float p23 = sat((k[12] * s_r25 + p22) * 2.0f);
		const float r24 = w24(p23);   // r25
		ram[at(12)] = w19;
		// 0cd
		const float p25 = k[13] * in[1];
		const float r26 = w24(p25);   // r2a
		// 0ce
		const float p27 = k[14] * s_r2a;
		// 0cf
		const float p28 = k[15] * s_r2b + p27;
		// 0d0
		const float p29 = k[16] * s_r2c + p28;
		const float m30 = lfo[13];   // m04
		const float r31 = s_r2c;   // r2d
		// 0d1
		const float p32 = k[17] * s_r2d + p29;
		// 0d2
		const float p33 = sat((k[18] * r26 + p32) * 2.0f);
		const float r34 = w24(p33);   // r2c
		// 0d3
		const float p35 = k[19];
		// 0d4
		const float p36 = (k[20] * s_r3a + p35) * 16.0f;
		const float r37 = w24(p36);   // r3a
		// 0d5
		const float p38 = k[21] * r26;
		const float r39 = r26;   // r2e
		// 0d6
		const float p40 = k[22] * s_r2e + p38;
		// 0d7
		const float p41 = sat((k[23] * s_r2f + p40) * 2.0f);
		const float r42 = w24(p41);   // r2f
		// 0d8
		const float p43 = k[24] * r37;
		const float r44 = w24(p43);   // r08
		// 0d9
		// 0da
		// 0db
		// 0dc
		// 0dd
		// 0de
		const float p45 = k[30];
		// 0df
		const float p46 = sat(k[31] * m2 + p45);
		// 0e0
		const float p47 = k[32] * r15;
		const float m48 = lfo[14];   // m03
		// 0e1
		const float p49 = k[33] * r18 + p47;
		const float r50 = w24(p49);   // r34
		const float t51 = tv_plain(p46);   // t2
		// 0e2
		const float p52 = k[34] * s_r34;
		// 0e3
		const float p53 = k[35] * s_r35 + p52;
		// 0e4
		const float p54 = sat((k[36] * r50 + p53) * 2.0f);
		const float r55 = w24(p54);   // r35
		const float q56 = ram[at(36)];
		// 0e5
		const float p57 = t51 * r44;
		const float r58 = w24(p57);   // r03
		// 0e6
		const float p59 = k[38];
		const float m60 = q56;   // m01
		// 0e7
		const float p61 = sat(k[39] * m30 + p59);
		const float q62 = ram[at(39)];
		// 0e8
		const float p63 = k[40] * m48;
		// 0e9
		const float p64 = sat((k[41] * r12 + p63) * 16.0f);
		const float m65 = q62;   // m02
		const float r66 = w24(p64);   // r04
		const float t67 = tv_plain(p61);   // t2
		// 0ea
		const float p68 = k[42] * m48;
		const float q69 = ram[at(42)];
		// 0eb
		const float p70 = sat((k[43] * r34 + p68) * 16.0f);
		const float r71 = w24(p70);   // r05
		// 0ec
		const float p72 = t67 * r58;
		const float m73 = q69;   // m14
		const float w74 = p72;
		// 0ed
		const float p75 = k[45];
		const float q76 = ram[at(45)];
		// 0ee
		const float p77 = sat(k[46] * r66 + p75);
		const int32_t i78 = idx_of(p77);
		// 0ef
		const float m79 = q76;   // m15
		// 0f0
		const float p80 = k[48] * r24;
		const float m81 = lfo[15];   // m06
		const float t82 = tv_index(p77);   // t3
		ram[at(48)] = w74;
		// 0f1
		const float p83 = k[49] * m60 + p80;
		// 0f2
		const float p84 = k[50] * m73 + p83;
		// 0f3
		const float p85 = k[51] * s_r28 + p84;
		const float q86 = tab(51, i78);
		// 0f4
		const float p87 = sat((k[52] * in[0] + p85) * 4.0f);
		const float r88 = w24(p87);   // r06
		// 0f5
		const float p89 = k[53] * r42;
		const float m90 = q86;   // m03
		// 0f6
		const float p91 = k[54] * m65 + p89;
		const float q92 = tab(54, i78 + 1);
		// 0f7
		const float p93 = k[55] * m79 + p91;
		// 0f8
		const float p94 = k[56] * s_r32 + p93;
		const float m95 = q92;   // m04
		// 0f9
		const float p96 = sat((k[57] * in[1] + p94) * 4.0f);
		const float r97 = w24(p96);   // r07
		// 0fa
		const float p98 = t82 * m90 - m90;
		// 0fb
		const float p99 = t82 * m95 - p98;
		const float r100 = w24(p99);   // r04
		// 0fc
		const float p101 = k[60];
		// 0fd
		const float p102 = sat(k[61] * r71 + p101);
		const int32_t i103 = idx_of(p102);
		// 0fe
		const float p104 = sat(k[62] * r88);
		const float m105 = w24(p104);   // m28
		// 0ff
		const float p106 = sat(k[63] * r97);
		const float m107 = w24(p106);   // m29
		const float t108 = tv_index(p102);   // t3
		// 100
		const float p109 = k[64] * r100;
		const float m110 = lfo[16];   // m05
		const float r111 = w24(p109);   // r26
		// 101
		const float p112 = k[65] * s_r26;
		const float r113 = s_r26;   // r27
		// 102
		const float p114 = k[66] * s_r27 + p112;
		const float q115 = tab(66, i103);
		// 103
		const float p116 = k[67] * s_r28 + p114;
		const float r117 = s_r28;   // r29
		// 104
		const float p118 = k[68] * s_r29 + p116;
		const float m119 = q115;   // m03
		// 105
		const float p120 = sat((k[69] * r111 + p118) * 2.0f);
		const float r121 = w24(p120);   // r28
		const float q122 = tab(69, i103 + 1);
		// 106
		const float p123 = k[70];
		// 107
		const float p124 = sat(k[71] * m81 + p123);
		const float m125 = q122;   // m04
		// 108
		const float p126 = k[72];
		// 109
		const float p127 = sat(k[73] * m110 + p126);
		const float t128 = tv_plain(p124);   // t2
		// 10a
		const float p129 = t108 * m119 - m119;
		// 10b
		const float p130 = t108 * m125 - p129;
		const float r131 = w24(p130);   // r05
		const float t132 = tv_plain(p127);   // t3
		// 10c
		const float p133 = t128 * r55;
		const float w134 = p133;
		// 10d
		// 10e
		const float p135 = t132 * r55;
		const float w136 = p135;
		ram[at(78)] = w134;
		// 10f
		const float p137 = k[79] * r131;
		const float r138 = w24(p137);   // r30
		// 110
		const float p139 = k[80] * s_r30;
		const float r140 = s_r30;   // r31
		// 111
		const float p141 = k[81] * s_r31 + p139;
		ram[at(81)] = w136;
		// 112
		const float p142 = k[82] * s_r32 + p141;
		const float r143 = s_r32;   // r33
		// 113
		const float p144 = k[83] * s_r33 + p142;
		// 114
		const float p145 = sat((k[84] * r138 + p144) * 2.0f);
		const float r146 = w24(p145);   // r32
		// 115
		// 116
		// 117
		const float p147 = (k[87] * r15) * 2.0f;
		const float r148 = w24(p147);   // r36
		// 118
		const float p149 = (k[88] * s_r36) * 2.0f;
		const float r150 = w24(p149);   // r37
		// 119
		const float p151 = (k[89] * s_r37) * 2.0f;
		const float r152 = w24(p151);   // r38
		// 11a
		const float p153 = (k[90] * s_r38) * 2.0f;
		const float r154 = w24(p153);   // r39
		// 11b
		const float p155 = (k[91] * s_r39) * 2.0f;
		const float r156 = w24(p155);   // r3b
		// 11c
		// 11d
		const float p157 = (k[93] * m73) * 2.0f;
		const float r158 = w24(p157);   // r21
		// 11e
		const float p159 = (k[94] * m79) * 2.0f;
		const float r160 = w24(p159);   // r2b
		// 11f
		out[0] = m105;   // m28
		out[1] = m107;   // m29
		s_p = p159;
		s_r20 = r3;
		s_r21 = r158;
		s_r22 = r12;
		s_r23 = r9;
		s_r3b = r156;
		s_r24 = r21;
		s_r25 = r24;
		s_r2a = r26;
		s_r2b = r160;
		s_r2c = r34;
		s_r2d = r31;
		s_r3a = r37;
		s_r2e = r39;
		s_r2f = r42;
		s_r34 = r50;
		s_r35 = r55;
		s_r28 = r121;
		s_r32 = r146;
		s_r26 = r111;
		s_r27 = r113;
		s_r29 = r117;
		s_r30 = r138;
		s_r31 = r140;
		s_r33 = r143;
		s_r36 = r148;
		s_r37 = r150;
		s_r38 = r152;
		s_r39 = r154;
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
	float s_r37 = 0.0f;
	float s_r38 = 0.0f;
	float s_r39 = 0.0f;
	float s_r3a = 0.0f;
	float s_r3b = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_TURNTABLE_H
