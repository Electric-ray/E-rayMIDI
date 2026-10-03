// license:BSD-3-Clause
// S-MU2000: バリエーション: AUTO WAH, A-WAH+DT, A-WAH+OD, TOUCH WAH1, TOUCH WAH2, T-WAH+DIST, T-WAH+ODRV
//
// **tools/meg_fx/emit.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。係数と番地は firmware が MEG に
// 書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d


#ifndef S_MU2000_DSP_MEG_FX_VAR_AUTOWAH_H
#define S_MU2000_DSP_MEG_FX_VAR_AUTOWAH_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_autowah : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x40000;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); s_m00 = 0; s_m16 = 0; s_m17 = 0; s_m18 = 0; s_m19 = 0; s_p = 0; s_r00 = 0; s_r44 = 0; s_r45 = 0; s_r46 = 0; s_r47 = 0; s_r48 = 0; s_r49 = 0; s_r4a = 0; s_r4b = 0; s_r4c = 0; s_r4d = 0; s_r4e = 0; s_r4f = 0; s_r50 = 0; s_r51 = 0; s_r52 = 0; s_r53 = 0; s_r54 = 0; s_r55 = 0; s_r56 = 0; s_r57 = 0; s_r58 = 0; s_r59 = 0; s_r5a = 0; s_r5b = 0; }

	// in: 入口（m2c m2d）。out: 出口（m2c m2d）。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		// 120
		const float p1 = sat((k[0] * in[0]) * 2.0f);
		const float r2 = w24(p1);   // r57
		// 121
		const float p3 = k[1] * s_r57;
		// 122
		const float p4 = k[2] * s_r58 + p3;
		// 123
		const float p5 = sat((k[3] * r2 + p4) * 2.0f);
		const float r6 = w24(p5);   // r58
		// 124
		const float p7 = k[4] * s_r58;
		const float m8 = lfo[18];   // m34
		// 125
		const float p9 = k[5] * s_r44 + p7;
		// 126
		const float p10 = sat((k[6] * r6 + p9) * 4.0f);
		const float r11 = w24(p10);   // r44
		// 127
		const float p12 = sat((k[7] * in[1]) * 2.0f);
		const float r13 = w24(p12);   // r45
		// 128
		const float p14 = k[8] * s_r45;
		// 129
		const float p15 = k[9] * s_r59 + p14;
		// 12a
		const float p16 = sat((k[10] * r13 + p15) * 2.0f);
		const float r17 = w24(p16);   // r59
		// 12b
		const float p18 = k[11] * s_r59;
		// 12c
		const float p19 = k[12] * s_r46 + p18;
		// 12d
		const float p20 = sat((k[13] * r17 + p19) * 4.0f);
		const float r21 = w24(p20);   // r46
		// 12e
		const float p22 = k[14] * s_m18;
		const float m23 = s_m18;   // m05
		// 12f
		const float p24 = sat(k[15] * s_m19 + p22);
		const float m25 = s_m19;   // m06
		const float r26 = w24(p24);   // r4c
		// 130
		const float p27 = k[16] * s_r4c;
		// 131
		const float p28 = k[17] * s_r4d + p27;
		// 132
		const float p29 = sat((k[18] * r26 + p28) * 2.0f);
		const float r30 = w24(p29);   // r4d
		// 133
		const float p31 = sat(k[19] * s_r4d);
		// 134
		const float p32 = k[20] * s_r4e + p31;
		// 135
		const float p33 = sat((k[21] * r30 + p32) * 2.0f);
		const float r34 = w24(p33);   // r4e
		// 136
		const float p35 = k[22] * r11;
		// 137
		const float p36 = sat(k[23] * r21 + p35);
		const float r37 = w24(p36);   // r47
		// 138
		const float p38 = sat((k[24] * r34) * 16.0f);
		const float r39 = w24(p38);   // r02
		// 139
		const float p40 = k[25] * s_r47;
		// 13a
		const float p41 = k[26] * s_r48 + p40;
		// 13b
		const float p42 = sat((k[27] * r37 + p41) * 2.0f);
		const float r43 = w24(p42);   // r48
		// 13c
		const float p44 = sat((k[28] * r39) * 16.0f);
		const float r45 = w24(p44);   // r02
		// 13d
		const float p46 = k[29] * s_m16 - r11;
		// 13e
		const float p47 = sat(k[30] * s_r4a - p46);
		const float m48 = w24(p47);   // m02
		// 13f
		const float p49 = satabs((k[31] * r43) * 16.0f);
		const float m50 = w24(p49);   // m01
		// 140
		const float p51 = sat((k[32] * r45) * 16.0f);
		const float r52 = w24(p51);   // r02
		// 141
		const float p53 = k[33] * s_m17 - r21;
		// 142
		const float p54 = sat(k[34] * s_r4b - p53);
		const float m55 = w24(p54);   // m03
		// 143
		const float p56 = satpos(k[35] * s_r49 - m50);
		// 144
		const float p57 = sat(k[36] * m50 + p56);
		const float r58 = w24(p57);   // r49
		// 145
		const float p59 = k[37] * s_r5a;
		// 146
		const float p60 = k[38] * s_r5a + (p59 * (1.0f / 32768.0f));
		// 147
		const float p61 = k[39] * m50 - p60;
		// 148
		const float p62 = satpos(k[40] * r58 - p61);
		// 149
		const float p63 = sat(k[41] * m50 + p62);
		const float r64 = w24(p63);   // r5a
		// 14a
		const float p65 = k[42] * s_r5a;
		// 14b
		const float p66 = k[43] * s_r5b + p65;
		// 14c
		const float p67 = sat(k[44] * r64 + p66);
		const float r68 = w24(p67);   // r5b
		// 14d
		const float p69 = sat((k[45] * r52) * 16.0f);
		const float r70 = w24(p69);   // r03
		// 14e
		const float p71 = k[46];
		// 14f
		const float p72 = k[47] * m8 + p71;
		// 150
		const float p73 = sat(k[48] * r68 + p72);
		const float r74 = w24(p73);   // r01
		// 151
		const float p75 = sat((k[49] * r70) * 16.0f);
		const float r76 = w24(p75);   // r04
		// 152
		const float t77 = tv_plain(p73);   // t1
		// 153
		const float p78 = sat(t77 * r74);
		const float r79 = w24(p78);   // r01
		// 154
		const float p80 = sat((k[52] * r76) * 16.0f);
		const float r81 = w24(p80);   // r05
		// 155
		const float t82 = tv_plain(p78);   // t1
		// 156
		const float p83 = t82 * r79;
		// 157
		const float p84 = sat(k[55] + p83);
		// 158
		const float p85 = sat((k[56] * r81) * 16.0f);
		const float m86 = w24(p85);   // m04
		// 159
		const float t87 = tv_plain(p84);   // t1
		// 15a
		const float p88 = sat(t87 * m48 + s_r4a);
		const float r89 = w24(p88);   // r4a
		// 15b
		const float p90 = sat(t87 * m55 + s_r4b);
		const float r91 = w24(p90);   // r4b
		// 15c
		const float p92 = sat((k[60] * m86) * 16.0f);
		const float r93 = w24(p92);   // r02
		// 15d
		const float p94 = k[61] * r52;
		// 15e
		const float p95 = k[62] * r70 + p94;
		// 15f
		const float p96 = k[63] * r76 + p95;
		// 160
		const float p97 = k[64] * r81 + p96;
		// 161
		const float p98 = k[65] * m86 + p97;
		// 162
		const float p99 = sat(k[66] * r93 + p98);
		const float r100 = w24(p99);   // r02
		// 163
		const float p101 = sat(t87 * r89 + s_m16);
		const float m102 = w24(p101);   // m16
		// 164
		const float p103 = sat(t87 * r91 + s_m17);
		const float m104 = w24(p103);   // m17
		// 165
		const float p105 = sat(k[69] * r100);
		const float r106 = w24(p105);   // r4f
		// 166
		const float p107 = k[70] * s_r50;
		// 167
		const float p108 = k[71] * s_r4f + p107;
		const float r109 = s_r4f;   // r50
		// 168
		const float p110 = k[72] * s_r52 + p108;
		// 169
		const float p111 = k[73] * s_r51 + p110;
		// 16a
		const float p112 = sat((k[74] * r106 + p111) * 2.0f);
		const float r113 = w24(p112);   // r51
		// 16b
		const float p114 = k[75] * s_r51;
		const float r115 = s_r51;   // r52
		// 16c
		const float p116 = k[76] * s_r52 + p114;
		// 16d
		const float p117 = k[77] * s_r54 + p116;
		// 16e
		const float p118 = k[78] * s_r53 + p117;
		// 16f
		const float p119 = sat((k[79] * r113 + p118) * 2.0f);
		const float r120 = w24(p119);   // r53
		// 170
		const float p121 = k[80] * s_r53;
		const float r122 = s_r53;   // r54
		// 171
		const float p123 = k[81] * s_r54 + p121;
		// 172
		const float p124 = k[82] * s_r55 + p123;
		const float r125 = s_r55;   // r56
		// 173
		const float p126 = k[83] * s_r56 + p124;
		// 174
		const float p127 = sat((k[84] * r120 + p126) * 16.0f);
		const float r128 = w24(p127);   // r55
		// 175
		const float p129 = k[85] * r11;
		// 176
		const float p130 = sat((k[86] * r89 + p129) * 4.0f);
		const float m131 = w24(p130);   // m18
		// 177
		const float p132 = k[87] * r21;
		// 178
		const float p133 = sat((k[88] * r91 + p132) * 4.0f);
		const float m134 = w24(p133);   // m19
		// 179
		const float p135 = k[89] * m23;
		// 17a
		const float p136 = sat((k[90] * r128 + p135) * 4.0f);
		const float m137 = w24(p136);   // m2c
		// 17b
		const float p138 = k[91] * m25;
		// 17c
		const float p139 = sat((k[92] * r128 + p138) * 4.0f);
		const float m140 = w24(p139);   // m2d
		// 17d
		// 17e
		// 17f
		out[0] = m137;   // m2c
		out[1] = m140;   // m2d
		s_p = p139;
		s_r57 = r2;
		s_r58 = r6;
		s_r44 = r11;
		s_r45 = r13;
		s_r59 = r17;
		s_r46 = r21;
		s_m18 = m131;
		s_m19 = m134;
		s_r4c = r26;
		s_r4d = r30;
		s_r4e = r34;
		s_r47 = r37;
		s_r48 = r43;
		s_m16 = m102;
		s_r4a = r89;
		s_m17 = m104;
		s_r4b = r91;
		s_r49 = r58;
		s_r5a = r64;
		s_r5b = r68;
		s_r50 = r109;
		s_r4f = r106;
		s_r52 = r115;
		s_r51 = r113;
		s_r54 = r122;
		s_r53 = r120;
		s_r55 = r128;
		s_r56 = r125;
	}

private:
	float s_m00 = 0.0f;
	float s_m16 = 0.0f;
	float s_m17 = 0.0f;
	float s_m18 = 0.0f;
	float s_m19 = 0.0f;
	float s_p = 0.0f;
	float s_r00 = 0.0f;
	float s_r44 = 0.0f;
	float s_r45 = 0.0f;
	float s_r46 = 0.0f;
	float s_r47 = 0.0f;
	float s_r48 = 0.0f;
	float s_r49 = 0.0f;
	float s_r4a = 0.0f;
	float s_r4b = 0.0f;
	float s_r4c = 0.0f;
	float s_r4d = 0.0f;
	float s_r4e = 0.0f;
	float s_r4f = 0.0f;
	float s_r50 = 0.0f;
	float s_r51 = 0.0f;
	float s_r52 = 0.0f;
	float s_r53 = 0.0f;
	float s_r54 = 0.0f;
	float s_r55 = 0.0f;
	float s_r56 = 0.0f;
	float s_r57 = 0.0f;
	float s_r58 = 0.0f;
	float s_r59 = 0.0f;
	float s_r5a = 0.0f;
	float s_r5b = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_AUTOWAH_H
