// license:BSD-3-Clause
// S-MU2000: インサーション 1: AUTO WAH, A-WAH+DT, A-WAH+OD, TOUCH WAH1, TOUCH WAH2, T-WAH+DIST, T-WAH+ODRV
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_AUTOWAH_H
#define S_MU2000_DSP_MEG_FX_INS_AUTOWAH_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_autowah : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x1000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m12 = 0; s_m13 = 0; s_m14 = 0; s_m15 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; s_r36 = 0; s_r37 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = sat((k[0] * in[0]) * 2.0f);
		const float r2 = w24(p1);   // r33
		// 0c1
		const float p3 = k[1] * s_r33;
		// 0c2
		const float p4 = k[2] * s_r34 + p3;
		// 0c3
		const float p5 = sat((k[3] * r2 + p4) * 2.0f);
		const float r6 = w24(p5);   // r34
		// 0c4
		const float p7 = k[4] * s_r34;
		const float m8 = lfo[12];   // m34
		// 0c5
		const float p9 = k[5] * s_r20 + p7;
		// 0c6
		const float p10 = sat((k[6] * r6 + p9) * 4.0f);
		const float r11 = w24(p10);   // r20
		// 0c7
		const float p12 = sat((k[7] * in[1]) * 2.0f);
		const float r13 = w24(p12);   // r21
		// 0c8
		const float p14 = k[8] * s_r21;
		// 0c9
		const float p15 = k[9] * s_r35 + p14;
		// 0ca
		const float p16 = sat((k[10] * r13 + p15) * 2.0f);
		const float r17 = w24(p16);   // r35
		// 0cb
		const float p18 = k[11] * s_r35;
		// 0cc
		const float p19 = k[12] * s_r22 + p18;
		// 0cd
		const float p20 = sat((k[13] * r17 + p19) * 4.0f);
		const float r21 = w24(p20);   // r22
		// 0ce
		const float p22 = k[14] * s_m14;
		const float m23 = s_m14;   // m05
		// 0cf
		const float p24 = sat(k[15] * s_m15 + p22);
		const float m25 = s_m15;   // m06
		const float r26 = w24(p24);   // r28
		// 0d0
		const float p27 = k[16] * s_r28;
		// 0d1
		const float p28 = k[17] * s_r29 + p27;
		// 0d2
		const float p29 = sat((k[18] * r26 + p28) * 2.0f);
		const float r30 = w24(p29);   // r29
		// 0d3
		const float p31 = sat(k[19] * s_r29);
		// 0d4
		const float p32 = k[20] * s_r2a + p31;
		// 0d5
		const float p33 = sat((k[21] * r30 + p32) * 2.0f);
		const float r34 = w24(p33);   // r2a
		// 0d6
		const float p35 = k[22] * r11;
		// 0d7
		const float p36 = sat(k[23] * r21 + p35);
		const float r37 = w24(p36);   // r23
		// 0d8
		const float p38 = sat((k[24] * r34) * 16.0f);
		const float r39 = w24(p38);   // r02
		// 0d9
		const float p40 = k[25] * s_r23;
		// 0da
		const float p41 = k[26] * s_r24 + p40;
		// 0db
		const float p42 = sat((k[27] * r37 + p41) * 2.0f);
		const float r43 = w24(p42);   // r24
		// 0dc
		const float p44 = sat((k[28] * r39) * 16.0f);
		const float r45 = w24(p44);   // r02
		// 0dd
		const float p46 = k[29] * s_m12 - r11;
		// 0de
		const float p47 = sat(k[30] * s_r26 - p46);
		const float m48 = w24(p47);   // m02
		// 0df
		const float p49 = satabs((k[31] * r43) * 16.0f);
		const float m50 = w24(p49);   // m01
		// 0e0
		const float p51 = sat((k[32] * r45) * 16.0f);
		const float r52 = w24(p51);   // r02
		// 0e1
		const float p53 = k[33] * s_m13 - r21;
		// 0e2
		const float p54 = sat(k[34] * s_r27 - p53);
		const float m55 = w24(p54);   // m03
		// 0e3
		const float p56 = satpos(k[35] * s_r25 - m50);
		// 0e4
		const float p57 = sat(k[36] * m50 + p56);
		const float r58 = w24(p57);   // r25
		// 0e5
		const float p59 = k[37] * s_r36;
		// 0e6
		const float p60 = k[38] * s_r36 + (p59 * (1.0f / 32768.0f));
		// 0e7
		const float p61 = k[39] * m50 - p60;
		// 0e8
		const float p62 = satpos(k[40] * r58 - p61);
		// 0e9
		const float p63 = sat(k[41] * m50 + p62);
		const float r64 = w24(p63);   // r36
		// 0ea
		const float p65 = k[42] * s_r36;
		// 0eb
		const float p66 = k[43] * s_r37 + p65;
		// 0ec
		const float p67 = sat(k[44] * r64 + p66);
		const float r68 = w24(p67);   // r37
		// 0ed
		const float p69 = sat((k[45] * r52) * 16.0f);
		const float r70 = w24(p69);   // r03
		// 0ee
		const float p71 = k[46];
		// 0ef
		const float p72 = k[47] * m8 + p71;
		// 0f0
		const float p73 = sat(k[48] * r68 + p72);
		const float r74 = w24(p73);   // r01
		// 0f1
		const float p75 = sat((k[49] * r70) * 16.0f);
		const float r76 = w24(p75);   // r04
		// 0f2
		const float t77 = tv_plain(p73);   // t1
		// 0f3
		const float p78 = sat(t77 * r74);
		const float r79 = w24(p78);   // r01
		// 0f4
		const float p80 = sat((k[52] * r76) * 16.0f);
		const float r81 = w24(p80);   // r05
		// 0f5
		const float t82 = tv_plain(p78);   // t1
		// 0f6
		const float p83 = t82 * r79;
		// 0f7
		const float p84 = sat(k[55] + p83);
		// 0f8
		const float p85 = sat((k[56] * r81) * 16.0f);
		const float m86 = w24(p85);   // m04
		// 0f9
		const float t87 = tv_plain(p84);   // t1
		// 0fa
		const float p88 = sat(t87 * m48 + s_r26);
		const float r89 = w24(p88);   // r26
		// 0fb
		const float p90 = sat(t87 * m55 + s_r27);
		const float r91 = w24(p90);   // r27
		// 0fc
		const float p92 = sat((k[60] * m86) * 16.0f);
		const float r93 = w24(p92);   // r02
		// 0fd
		const float p94 = k[61] * r52;
		// 0fe
		const float p95 = k[62] * r70 + p94;
		// 0ff
		const float p96 = k[63] * r76 + p95;
		// 100
		const float p97 = k[64] * r81 + p96;
		// 101
		const float p98 = k[65] * m86 + p97;
		// 102
		const float p99 = sat(k[66] * r93 + p98);
		const float r100 = w24(p99);   // r02
		// 103
		const float p101 = sat(t87 * r89 + s_m12);
		const float m102 = w24(p101);   // m12
		// 104
		const float p103 = sat(t87 * r91 + s_m13);
		const float m104 = w24(p103);   // m13
		// 105
		const float p105 = sat(k[69] * r100);
		const float r106 = w24(p105);   // r2b
		// 106
		const float p107 = k[70] * s_r2c;
		// 107
		const float p108 = k[71] * s_r2b + p107;
		const float r109 = s_r2b;   // r2c
		// 108
		const float p110 = k[72] * s_r2e + p108;
		// 109
		const float p111 = k[73] * s_r2d + p110;
		// 10a
		const float p112 = sat((k[74] * r106 + p111) * 2.0f);
		const float r113 = w24(p112);   // r2d
		// 10b
		const float p114 = k[75] * s_r2d;
		const float r115 = s_r2d;   // r2e
		// 10c
		const float p116 = k[76] * s_r2e + p114;
		// 10d
		const float p117 = k[77] * s_r30 + p116;
		// 10e
		const float p118 = k[78] * s_r2f + p117;
		// 10f
		const float p119 = sat((k[79] * r113 + p118) * 2.0f);
		const float r120 = w24(p119);   // r2f
		// 110
		const float p121 = k[80] * s_r2f;
		const float r122 = s_r2f;   // r30
		// 111
		const float p123 = k[81] * s_r30 + p121;
		// 112
		const float p124 = k[82] * s_r31 + p123;
		const float r125 = s_r31;   // r32
		// 113
		const float p126 = k[83] * s_r32 + p124;
		// 114
		const float p127 = sat((k[84] * r120 + p126) * 16.0f);
		const float r128 = w24(p127);   // r31
		// 115
		const float p129 = k[85] * r11;
		// 116
		const float p130 = sat((k[86] * r89 + p129) * 4.0f);
		const float m131 = w24(p130);   // m14
		// 117
		const float p132 = k[87] * r21;
		// 118
		const float p133 = sat((k[88] * r91 + p132) * 4.0f);
		const float m134 = w24(p133);   // m15
		// 119
		const float p135 = k[89] * m23;
		// 11a
		const float p136 = sat((k[90] * r128 + p135) * 4.0f);
		const float m137 = w24(p136);   // m28
		// 11b
		const float p138 = k[91] * m25;
		// 11c
		const float p139 = sat((k[92] * r128 + p138) * 4.0f);
		const float m140 = w24(p139);   // m29
		// 11d
		// 11e
		// 11f
		out[0] = m137;   // m28
		out[1] = m140;   // m29
		s_p = p139;
		s_r33 = r2;
		s_r34 = r6;
		s_r20 = r11;
		s_r21 = r13;
		s_r35 = r17;
		s_r22 = r21;
		s_m14 = m131;
		s_m15 = m134;
		s_r28 = r26;
		s_r29 = r30;
		s_r2a = r34;
		s_r23 = r37;
		s_r24 = r43;
		s_m12 = m102;
		s_r26 = r89;
		s_m13 = m104;
		s_r27 = r91;
		s_r25 = r58;
		s_r36 = r64;
		s_r37 = r68;
		s_r2c = r109;
		s_r2b = r106;
		s_r2e = r115;
		s_r2d = r113;
		s_r30 = r122;
		s_r2f = r120;
		s_r31 = r128;
		s_r32 = r125;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_AUTOWAH_H
