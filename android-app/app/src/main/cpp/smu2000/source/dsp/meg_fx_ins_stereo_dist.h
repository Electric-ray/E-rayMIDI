// license:BSD-3-Clause
// S-MU2000: インサーション 1: STREO DT, STREO OD, STREO AMP
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29


#ifndef S_MU2000_DSP_MEG_FX_INS_STEREO_DIST_H
#define S_MU2000_DSP_MEG_FX_INS_STEREO_DIST_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_stereo_dist : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_p = 0; s_r00 = 0; s_r20 = 0; s_r21 = 0; s_r22 = 0; s_r23 = 0; s_r24 = 0; s_r25 = 0; s_r26 = 0; s_r27 = 0; s_r28 = 0; s_r29 = 0; s_r2a = 0; s_r2b = 0; s_r2c = 0; s_r2d = 0; s_r2e = 0; s_r2f = 0; s_r30 = 0; s_r31 = 0; s_r32 = 0; s_r33 = 0; s_r34 = 0; s_r35 = 0; }

	// in: 入口（m28 m29）。out: 出口（m28 m29）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 0c0
		const float p1 = k[0] * in[0];
		// 0c1
		const float p2 = sat(k[1] * in[1] + p1);
		const float r3 = w24(p2);   // r20
		// 0c2
		const float p4 = k[2] * s_r20;
		// 0c3
		const float p5 = k[3] * s_r21 + p4;
		// 0c4
		const float p6 = sat((k[4] * r3 + p5) * 2.0f);
		const float r7 = w24(p6);   // r21
		// 0c5
		const float p8 = sat(k[5] * s_r21);
		// 0c6
		const float p9 = k[6] * s_r22 + p8;
		// 0c7
		const float p10 = sat((k[7] * r7 + p9) * 2.0f);
		const float r11 = w24(p10);   // r22
		// 0c8
		const float p12 = k[8] * in[1];
		// 0c9
		const float p13 = sat(k[9] * in[0] + p12);
		const float r14 = w24(p13);   // r2b
		// 0ca
		const float p15 = k[10] * s_r2b;
		// 0cb
		const float p16 = k[11] * s_r2c + p15;
		// 0cc
		const float p17 = sat((k[12] * r14 + p16) * 2.0f);
		const float r18 = w24(p17);   // r2c
		// 0cd
		const float p19 = sat(k[13] * s_r2c);
		// 0ce
		const float p20 = k[14] * s_r2d + p19;
		// 0cf
		const float p21 = sat((k[15] * r18 + p20) * 2.0f);
		const float r22 = w24(p21);   // r2d
		// 0d0
		// 0d1
		const float p23 = sat((k[17] * r11) * 16.0f);
		const float r24 = w24(p23);   // r01
		// 0d2
		const float p25 = sat((k[18] * r22) * 16.0f);
		const float r26 = w24(p25);   // r02
		// 0d3
		// 0d4
		const float p27 = sat((k[20] * r24) * 16.0f);
		const float r28 = w24(p27);   // r01
		// 0d5
		const float p29 = sat((k[21] * r26) * 16.0f);
		const float r30 = w24(p29);   // r02
		// 0d6
		// 0d7
		const float p31 = sat((k[23] * r28) * 16.0f);
		const float r32 = w24(p31);   // r01
		// 0d8
		const float p33 = sat((k[24] * r30) * 16.0f);
		const float r34 = w24(p33);   // r02
		// 0d9
		// 0da
		const float p35 = sat((k[26] * r32) * 16.0f);
		const float r36 = w24(p35);   // r03
		// 0db
		const float p37 = sat((k[27] * r34) * 16.0f);
		const float r38 = w24(p37);   // r04
		// 0dc
		// 0dd
		const float p39 = sat((k[29] * r36) * 16.0f);
		const float r40 = w24(p39);   // r05
		// 0de
		const float p41 = sat((k[30] * r38) * 16.0f);
		const float r42 = w24(p41);   // r06
		// 0df
		// 0e0
		const float p43 = sat((k[32] * r40) * 16.0f);
		const float r44 = w24(p43);   // r07
		// 0e1
		const float p45 = sat((k[33] * r42) * 16.0f);
		const float r46 = w24(p45);   // r08
		// 0e2
		// 0e3
		const float p47 = sat((k[35] * r44) * 16.0f);
		const float m48 = w24(p47);   // m01
		// 0e4
		const float p49 = sat((k[36] * r46) * 16.0f);
		const float m50 = w24(p49);   // m02
		// 0e5
		// 0e6
		const float p51 = sat((k[38] * m48) * 16.0f);
		const float m52 = w24(p51);   // m03
		// 0e7
		const float p53 = sat((k[39] * m50) * 16.0f);
		const float m54 = w24(p53);   // m04
		// 0e8
		const float p55 = k[40] * r32;
		// 0e9
		const float p56 = k[41] * r36 + p55;
		// 0ea
		const float p57 = k[42] * r40 + p56;
		// 0eb
		const float p58 = k[43] * r44 + p57;
		// 0ec
		const float p59 = k[44] * m48 + p58;
		// 0ed
		const float p60 = sat(k[45] * m52 + p59);
		const float r61 = w24(p60);   // r01
		// 0ee
		const float p62 = k[46] * r34;
		// 0ef
		const float p63 = k[47] * r38 + p62;
		// 0f0
		const float p64 = k[48] * r42 + p63;
		// 0f1
		const float p65 = k[49] * r46 + p64;
		// 0f2
		const float p66 = k[50] * m50 + p65;
		// 0f3
		const float p67 = sat(k[51] * m54 + p66);
		const float r68 = w24(p67);   // r02
		// 0f4
		const float p69 = sat(k[52] * r61);
		const float r70 = w24(p69);   // r23
		// 0f5
		const float p71 = k[53] * s_r24;
		// 0f6
		const float p72 = k[54] * s_r23 + p71;
		const float r73 = s_r23;   // r24
		// 0f7
		const float p74 = k[55] * s_r26 + p72;
		// 0f8
		const float p75 = k[56] * s_r25 + p74;
		// 0f9
		const float p76 = sat((k[57] * r70 + p75) * 2.0f);
		const float r77 = w24(p76);   // r25
		// 0fa
		const float p78 = k[58] * s_r25;
		const float r79 = s_r25;   // r26
		// 0fb
		const float p80 = k[59] * s_r26 + p78;
		// 0fc
		const float p81 = k[60] * s_r28 + p80;
		// 0fd
		const float p82 = k[61] * s_r27 + p81;
		// 0fe
		const float p83 = sat((k[62] * r77 + p82) * 2.0f);
		const float r84 = w24(p83);   // r27
		// 0ff
		const float p85 = k[63] * s_r27;
		const float r86 = s_r27;   // r28
		// 100
		const float p87 = k[64] * s_r28 + p85;
		// 101
		const float p88 = k[65] * s_r29 + p87;
		const float r89 = s_r29;   // r2a
		// 102
		const float p90 = k[66] * s_r2a + p88;
		// 103
		const float p91 = sat((k[67] * r84 + p90) * 16.0f);
		const float r92 = w24(p91);   // r29
		// 104
		const float p93 = sat(k[68] * r68);
		const float r94 = w24(p93);   // r2e
		// 105
		const float p95 = k[69] * s_r2f;
		// 106
		const float p96 = k[70] * s_r2e + p95;
		const float r97 = s_r2e;   // r2f
		// 107
		const float p98 = k[71] * s_r31 + p96;
		// 108
		const float p99 = k[72] * s_r30 + p98;
		// 109
		const float p100 = sat((k[73] * r94 + p99) * 2.0f);
		const float r101 = w24(p100);   // r30
		// 10a
		const float p102 = k[74] * s_r30;
		const float r103 = s_r30;   // r31
		// 10b
		const float p104 = k[75] * s_r31 + p102;
		// 10c
		const float p105 = k[76] * s_r33 + p104;
		// 10d
		const float p106 = k[77] * s_r32 + p105;
		// 10e
		const float p107 = sat((k[78] * r101 + p106) * 2.0f);
		const float r108 = w24(p107);   // r32
		// 10f
		const float p109 = k[79] * s_r32;
		const float r110 = s_r32;   // r33
		// 110
		const float p111 = k[80] * s_r33 + p109;
		// 111
		const float p112 = k[81] * s_r34 + p111;
		const float r113 = s_r34;   // r35
		// 112
		const float p114 = k[82] * s_r35 + p112;
		// 113
		const float p115 = sat((k[83] * r108 + p114) * 16.0f);
		const float r116 = w24(p115);   // r34
		// 114
		const float p117 = k[84] * in[0];
		// 115
		const float p118 = sat((k[85] * r92 + p117) * 4.0f);
		const float m119 = w24(p118);   // m28
		// 116
		const float p120 = k[86] * in[1];
		// 117
		const float p121 = sat((k[87] * r116 + p120) * 4.0f);
		const float m122 = w24(p121);   // m29
		// 118
		// 119
		// 11a
		// 11b
		// 11c
		// 11d
		// 11e
		// 11f
		out[0] = m119;   // m28
		out[1] = m122;   // m29
		s_p = p121;
		s_r20 = r3;
		s_r21 = r7;
		s_r22 = r11;
		s_r2b = r14;
		s_r2c = r18;
		s_r2d = r22;
		s_r24 = r73;
		s_r23 = r70;
		s_r26 = r79;
		s_r25 = r77;
		s_r28 = r86;
		s_r27 = r84;
		s_r29 = r92;
		s_r2a = r89;
		s_r2f = r97;
		s_r2e = r94;
		s_r31 = r103;
		s_r30 = r101;
		s_r33 = r110;
		s_r32 = r108;
		s_r34 = r116;
		s_r35 = r113;
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
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_STEREO_DIST_H
