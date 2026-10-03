// license:BSD-3-Clause
// S-MU2000: バリエーション: MULTI COMP
//
// **tools/meg_fx/emit_dyn.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。このプログラムは分岐（条件つきで
// 先の命令を飛ばす）を持つので、レジスタを変数として持ち、3 命令遅れの書き込みを「書くかどうか」の印つきで回す。
// 係数と番地は firmware が MEG に書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d

#ifndef S_MU2000_DSP_MEG_FX_VAR_MULTICOMP_H
#define S_MU2000_DSP_MEG_FX_VAR_MULTICOMP_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_multicomp : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); IX = 0; IX2 = 0; M01 = 0.0f; M02 = 0.0f; M03 = 0.0f; M04 = 0.0f; M05 = 0.0f; M06 = 0.0f; M07 = 0.0f; M16 = 0.0f; M17 = 0.0f; M18 = 0.0f; M19 = 0.0f; M2c = 0.0f; M2d = 0.0f; R01 = 0.0f; R02 = 0.0f; R03 = 0.0f; R04 = 0.0f; R05 = 0.0f; R06 = 0.0f; R07 = 0.0f; R08 = 0.0f; R09 = 0.0f; R44 = 0.0f; R45 = 0.0f; R46 = 0.0f; R47 = 0.0f; R4c = 0.0f; R4d = 0.0f; R4e = 0.0f; R4f = 0.0f; R50 = 0.0f; R51 = 0.0f; R58 = 0.0f; R59 = 0.0f; RRD = 0.0f; RWR = 0.0f; T1 = 0.0f; T2 = 0.0f; T3 = 0.0f; T7 = 0.0f; fn = false; fz = false; p = 0.0f; }

	// in: 入口。out: 出口。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		float tv0;
		int32_t pi0;
		bool ei0 = false;
		float tv1;
		float pr1;
		bool er1 = false;
		float tv2;
		float tv3;
		float pq3;
		bool eq3 = false;
		float tv4;
		float pr4;
		bool er4 = false;
		float tv5;
		float pm5;
		bool em5 = false;
		float pr5;
		bool er5 = false;
		float tv6;
		float pq6;
		bool eq6 = false;
		float tv7;
		float tv8;
		float pm8;
		bool em8 = false;
		float pr8;
		bool er8 = false;
		float tv9;
		int32_t pi9;
		bool ei9 = false;
		float tv10;
		float pr10;
		bool er10 = false;
		float tv11;
		float pm11;
		bool em11 = false;
		float tv12;
		float pq12;
		bool eq12 = false;
		float tv13;
		float pr13;
		bool er13 = false;
		float tv14;
		float pm14;
		bool em14 = false;
		float pr14;
		bool er14 = false;
		float tv15;
		float pq15;
		bool eq15 = false;
		float tv16;
		float pr16;
		bool er16 = false;
		float tv17;
		float pm17;
		bool em17 = false;
		float tv18;
		float pm18;
		bool em18 = false;
		float tv19;
		float tv20;
		float pm20;
		bool em20 = false;
		float tv21;
		float tv22;
		float pr22;
		bool er22 = false;
		float tv23;
		float pr23;
		bool er23 = false;
		float tv24;
		float tv25;
		float pr25;
		bool er25 = false;
		float tv26;
		float pr26;
		bool er26 = false;
		float tv27;
		float tv28;
		float pm28;
		bool em28 = false;
		float tv29;
		float pr29;
		bool er29 = false;
		float tv30;
		float pr30;
		bool er30 = false;
		float tv31;
		float pr31;
		bool er31 = false;
		float tv32;
		float tv33;
		float pm33;
		bool em33 = false;
		float tv34;
		float pm34;
		bool em34 = false;
		float tv35;
		float tv36;
		float pm36;
		bool em36 = false;
		float tv37;
		float tv38;
		float pm38;
		bool em38 = false;
		float tv39;
		float tv40;
		float pr40;
		bool er40 = false;
		float tv41;
		float tv42;
		float tv43;
		float pm43;
		bool em43 = false;
		float tv44;
		float tv45;
		float pr45;
		bool er45 = false;
		float tv46;
		float tv47;
		float tv48;
		int32_t pi48;
		bool ei48 = false;
		float tv49;
		float tv50;
		float pm50;
		bool em50 = false;
		float tv51;
		float tv52;
		float pr52;
		bool er52 = false;
		float tv53;
		float tv54;
		float pq54;
		bool eq54 = false;
		float tv55;
		float pm55;
		bool em55 = false;
		float tv56;
		float pm56;
		bool em56 = false;
		float tv57;
		float pr57;
		bool er57 = false;
		float pq57;
		bool eq57 = false;
		float tv58;
		float tv59;
		float pm59;
		bool em59 = false;
		float pr59;
		bool er59 = false;
		float tv60;
		float tv61;
		float tv62;
		int32_t pi62;
		bool ei62 = false;
		float tv63;
		float tv64;
		float tv65;
		float pr65;
		bool er65 = false;
		float tv66;
		float pq66;
		bool eq66 = false;
		float tv67;
		float tv68;
		float pm68;
		bool em68 = false;
		float tv69;
		int32_t pi69;
		bool ei69 = false;
		float pq69;
		bool eq69 = false;
		float tv70;
		float tv71;
		float pm71;
		bool em71 = false;
		float tv72;
		float pm72;
		bool em72 = false;
		float pq72;
		bool eq72 = false;
		float tv73;
		float pr73;
		bool er73 = false;
		float tv74;
		float pm74;
		bool em74 = false;
		float tv75;
		float pq75;
		bool eq75 = false;
		float tv76;
		float tv77;
		float pm77;
		bool em77 = false;
		float tv78;
		float pm78;
		bool em78 = false;
		float tv79;
		float pr79;
		bool er79 = false;
		float tv80;
		float tv81;
		float tv82;
		float tv83;
		float pm83;
		bool em83 = false;
		float tv84;
		float pr84;
		bool er84 = false;
		float tv85;
		float tv86;
		float pm86;
		bool em86 = false;
		float tv87;
		float pr87;
		bool er87 = false;
		float tv88;
		float pr88;
		bool er88 = false;
		float tv89;
		float pr89;
		bool er89 = false;
		float tv90;
		float tv91;
		float tv92;
		float pr92;
		bool er92 = false;
		float tv93;
		float tv94;
		float tv95;
		float pr95;
		bool er95 = false;
		int skip = 0;
		M2c = in[0];
		M2d = in[1];
		// 120
		p = k[0] * R58 + 0.0f;
		pi0 = idx_of(p); ei0 = true;
		tv0 = tv_index(p);
		// 121
		p = sat((k[1] * M2c + 0.0f) * 4.0f);
		pr1 = w24(p); er1 = true;
		tv1 = tv_plain(p);
		// 122
		p = k[2] * R45 + 0.0f;
		tv2 = tv_plain(p);
		T1 = tv0;
		// 123
		if (ei0) IX = pi0;
		p = k[3] * R44 + p;
		tv3 = tv_plain(p);
		pq3 = tab(3, IX); eq3 = true;
		// 124
		if (er1) R44 = pr1;
		p = sat((k[4] * R44 + p) * 2.0f);
		pr4 = w24(p); er4 = true;
		tv4 = tv_plain(p);
		// 125
		if (eq3) RRD = pq3;
		p = sat((k[5] * M2d + 0.0f) * 4.0f);
		pm5 = RRD; em5 = true;
		pr5 = w24(p); er5 = true;
		tv5 = tv_plain(p);
		// 126
		p = k[6] * R47 + 0.0f;
		tv6 = tv_plain(p);
		pq6 = tab(6, IX + 1); eq6 = true;
		// 127
		if (er4) R45 = pr4;
		p = k[7] * R46 + p;
		tv7 = tv_plain(p);
		// 128
		if (em5) M01 = pm5;
		if (er5) R46 = pr5;
		if (eq6) RRD = pq6;
		p = sat((k[8] * R46 + p) * 2.0f);
		pm8 = RRD; em8 = true;
		pr8 = w24(p); er8 = true;
		tv8 = tv_plain(p);
		// 129
		p = k[9] * R59 + 0.0f;
		pi9 = idx_of(p); ei9 = true;
		tv9 = tv_index(p);
		// 12a
		p = sat(R45 - M16);
		pr10 = w24(p); er10 = true;
		tv10 = tv_plain(p);
		// 12b
		if (em8) M02 = pm8;
		if (er8) R47 = pr8;
		p = satabs((k[11] * M18 + 0.0f) * 16.0f);
		pm11 = w24(p); em11 = true;
		tv11 = tv_plain(p);
		T2 = tv9;
		// 12c
		if (ei9) IX = pi9;
		p = T1 * M01 - M01;
		tv12 = tv_plain(p);
		pq12 = tab(12, IX); eq12 = true;
		// 12d
		if (er10) R01 = pr10;
		p = sat(T1 * M02 - p);
		pr13 = w24(p); er13 = true;
		tv13 = tv_plain(p);
		// 12e
		if (em11) M07 = pm11;
		if (eq12) RRD = pq12;
		p = sat(R47 - M18);
		pm14 = RRD; em14 = true;
		pr14 = w24(p); er14 = true;
		tv14 = tv_plain(p);
		// 12f
		p = k[15] * R58 + 0.0f;
		tv15 = tv_plain(p);
		pq15 = tab(15, IX + 1); eq15 = true;
		// 130
		if (er13) R05 = pr13;
		p = sat((k[16] * R05 + p) * 2.0f);
		pr16 = w24(p); er16 = true;
		tv16 = tv_plain(p);
		// 131
		if (em14) M03 = pm14;
		if (er14) R03 = pr14;
		if (eq15) RRD = pq15;
		pm17 = RRD; em17 = true;
		tv17 = tv_plain(p);
		// 132
		p = satabs((k[18] * M19 + 0.0f) * 16.0f);
		pm18 = w24(p); em18 = true;
		tv18 = tv_plain(p);
		// 133
		if (er16) R07 = pr16;
		p = k[19] * M2c + 0.0f;
		tv19 = tv_plain(p);
		// 134
		if (em17) M04 = pm17;
		p = sat((k[20] * R07 + p) * 4.0f);
		pm20 = w24(p); em20 = true;
		tv20 = tv_plain(p);
		// 135
		if (em18) M02 = pm18;
		p = T2 * M03 - M03;
		tv21 = tv_plain(p);
		// 136
		p = sat(T2 * M04 - p);
		pr22 = w24(p); er22 = true;
		tv22 = tv_plain(p);
		// 137
		if (em20) M2c = pm20;
		p = sat(R01 - M17);
		pr23 = w24(p); er23 = true;
		tv23 = tv_plain(p);
		// 138
		p = k[24] * R59 + 0.0f;
		tv24 = tv_plain(p);
		// 139
		if (er22) R06 = pr22;
		p = sat((k[25] * R06 + p) * 2.0f);
		pr25 = w24(p); er25 = true;
		tv25 = tv_plain(p);
		// 13a
		if (er23) R02 = pr23;
		p = sat(R03 - M19);
		pr26 = w24(p); er26 = true;
		tv26 = tv_plain(p);
		// 13b
		p = k[27] * M2d + 0.0f;
		tv27 = tv_plain(p);
		// 13c
		if (er25) R08 = pr25;
		p = sat((k[28] * R08 + p) * 4.0f);
		pm28 = w24(p); em28 = true;
		tv28 = tv_plain(p);
		// 13d
		if (er26) R04 = pr26;
		p = satabs((k[29] * M16 + 0.0f) * 16.0f);
		pr29 = w24(p); er29 = true;
		tv29 = tv_plain(p);
		// 13e
		p = satabs((k[30] * M17 + 0.0f) * 16.0f);
		pr30 = w24(p); er30 = true;
		tv30 = tv_plain(p);
		// 13f
		if (em28) M2d = pm28;
		p = satabs((k[31] * R02 + 0.0f) * 16.0f);
		pr31 = w24(p); er31 = true;
		tv31 = tv_plain(p);
		// 140
		if (er29) R05 = pr29;
		p = satpos(R05 - M07);
		tv32 = tv_plain(p);
		// 141
		if (er30) R06 = pr30;
		p = sat(M07 + p);
		pm33 = w24(p); em33 = true;
		tv33 = tv_plain(p);
		// 142
		if (er31) R07 = pr31;
		p = satabs((k[34] * R04 + 0.0f) * 16.0f);
		pm34 = w24(p); em34 = true;
		tv34 = tv_plain(p);
		// 143
		p = k[35] * R4c + 0.0f;
		tv35 = tv_plain(p);
		// 144
		if (em33) M01 = pm33;
		p = k[36] * M01 - p;
		fn = p < 0.0f; fz = p == 0.0f;
		pm36 = w24(p); em36 = true;
		tv36 = tv_plain(p);
		// 145
		if (em34) M03 = pm34;
		p = satpos(R06 - M02);
		tv37 = tv_plain(p);
		T1 = k[37];
		// 146
		p = sat(M02 + p);
		pm38 = w24(p); em38 = true;
		tv38 = tv_plain(p);
		T2 = k[38];
		// 147
		if (em36) M01 = pm36;
		p = (fn ? T1 : k[39]) * R4f + R4f;
		tv39 = tv_plain(p);
		// 148
		p = sat((fn ? T2 : k[40]) + p);
		pr40 = w24(p); er40 = true;
		tv40 = tv_plain(p);
		// 149
		if (em38) M02 = pm38;
		p = k[41] * R4f + 0.0f;
		tv41 = tv_plain(p);
		// 14a
		p = satpos(R07 - M03);
		tv42 = tv_plain(p);
		// 14b
		if (er40) R4f = pr40;
		p = sat(M03 + p);
		pm43 = w24(p); em43 = true;
		tv43 = tv_plain(p);
		T3 = tv41;
		// 14c
		p = sat((k[44] * R4c + 0.0f) * 2.0f);
		tv44 = tv_plain(p);
		// 14d
		p = sat((fn ? T3 : k[45]) * M01 + p);
		pr45 = w24(p); er45 = true;
		tv45 = tv_plain(p);
		// 14e
		if (em43) M03 = pm43;
		tv46 = tv_plain(p);
		// 14f
		p = k[47] + 0.0f;
		tv47 = tv_plain(p);
		// 150
		if (er45) R4c = pr45;
		p = k[48] * R4c + p;
		pi48 = idx_of(p); ei48 = true;
		tv48 = tv_index(p);
		// 151
		p = k[49] * R4d + 0.0f;
		tv49 = tv_plain(p);
		// 152
		p = k[50] * M02 - p;
		fn = p < 0.0f; fz = p == 0.0f;
		pm50 = w24(p); em50 = true;
		tv50 = tv_plain(p);
		T7 = tv48;
		// 153
		if (ei48) IX = pi48;
		p = (fn ? T1 : k[51]) * R50 + R50;
		tv51 = tv_plain(p);
		// 154
		p = sat((fn ? T2 : k[52]) + p);
		pr52 = w24(p); er52 = true;
		tv52 = tv_plain(p);
		// 155
		if (em50) M02 = pm50;
		p = k[53] * R50 + 0.0f;
		tv53 = tv_plain(p);
		// 156
		p = k[54] * R4e + 0.0f;
		tv54 = tv_plain(p);
		pq54 = tab(54, IX); eq54 = true;
		// 157
		if (er52) R50 = pr52;
		p = k[55] * M03 - p;
		fn = p < 0.0f; fz = p == 0.0f;
		pm55 = w24(p); em55 = true;
		tv55 = tv_plain(p);
		T3 = tv53;
		// 158
		if (eq54) RRD = pq54;
		p = sat((k[56] * R4d + 0.0f) * 2.0f);
		pm56 = RRD; em56 = true;
		tv56 = tv_plain(p);
		// 159
		p = sat((fn ? T3 : k[57]) * M02 + p);
		pr57 = w24(p); er57 = true;
		tv57 = tv_plain(p);
		pq57 = tab(57, IX + 1); eq57 = true;
		// 15a
		if (em55) M03 = pm55;
		p = (fn ? T1 : k[58]) * R51 + R51;
		tv58 = tv_plain(p);
		// 15b
		if (em56) M01 = pm56;
		if (eq57) RRD = pq57;
		p = sat((fn ? T2 : k[59]) + p);
		pm59 = RRD; em59 = true;
		pr59 = w24(p); er59 = true;
		tv59 = tv_plain(p);
		// 15c
		if (er57) R4d = pr57;
		p = k[60] * R51 + 0.0f;
		tv60 = tv_plain(p);
		// 15d
		p = k[61] + 0.0f;
		tv61 = tv_plain(p);
		// 15e
		if (em59) M02 = pm59;
		if (er59) R51 = pr59;
		p = k[62] * R4d + p;
		pi62 = idx_of(p); ei62 = true;
		tv62 = tv_index(p);
		T3 = tv60;
		// 15f
		tv63 = tv_plain(p);
		// 160
		p = sat((k[64] * R4e + 0.0f) * 2.0f);
		tv64 = tv_plain(p);
		T2 = tv62;
		// 161
		if (ei62) IX = pi62;
		p = sat((fn ? T3 : k[65]) * M03 + p);
		pr65 = w24(p); er65 = true;
		tv65 = tv_plain(p);
		// 162
		p = T7 * M01 - M01;
		tv66 = tv_plain(p);
		pq66 = tab(66, IX); eq66 = true;
		// 163
		p = sat(T7 * M02 - p);
		tv67 = tv_plain(p);
		// 164
		if (er65) R4e = pr65;
		if (eq66) RRD = pq66;
		p = k[68] + 0.0f;
		pm68 = RRD; em68 = true;
		tv68 = tv_plain(p);
		// 165
		p = k[69] * R4e + p;
		pi69 = idx_of(p); ei69 = true;
		tv69 = tv_index(p);
		T1 = tv67;
		pq69 = tab(69, IX + 1); eq69 = true;
		// 166
		tv70 = tv_plain(p);
		// 167
		if (em68) M03 = pm68;
		if (eq69) RRD = pq69;
		p = sat((k[71] * M16 + 0.0f) * 2.0f);
		pm71 = RRD; em71 = true;
		tv71 = tv_plain(p);
		T3 = tv69;
		// 168
		if (ei69) IX2 = pi69;
		p = sat(k[72] * R01 + p);
		pm72 = w24(p); em72 = true;
		tv72 = tv_plain(p);
		pq72 = tab(72, IX2); eq72 = true;
		// 169
		p = T1 * M16 + 0.0f;
		pr73 = w24(p); er73 = true;
		tv73 = tv_plain(p);
		// 16a
		if (em71) M04 = pm71;
		if (eq72) RRD = pq72;
		p = T2 * M03 - M03;
		pm74 = RRD; em74 = true;
		tv74 = tv_plain(p);
		// 16b
		if (em72) M16 = pm72;
		p = sat(T2 * M04 - p);
		tv75 = tv_plain(p);
		pq75 = tab(75, IX2 + 1); eq75 = true;
		// 16c
		if (er73) R01 = pr73;
		tv76 = tv_plain(p);
		// 16d
		if (em74) M05 = pm74;
		if (eq75) RRD = pq75;
		p = sat((k[77] * M18 + 0.0f) * 2.0f);
		pm77 = RRD; em77 = true;
		tv77 = tv_plain(p);
		T2 = tv75;
		// 16e
		p = sat(k[78] * R03 + p);
		pm78 = w24(p); em78 = true;
		tv78 = tv_plain(p);
		// 16f
		p = T1 * M18 + 0.0f;
		pr79 = w24(p); er79 = true;
		tv79 = tv_plain(p);
		// 170
		if (em77) M06 = pm77;
		p = T3 * M05 - M05;
		tv80 = tv_plain(p);
		// 171
		if (em78) M18 = pm78;
		p = sat(T3 * M06 - p);
		tv81 = tv_plain(p);
		// 172
		if (er79) R07 = pr79;
		p = sat((k[82] * M17 + 0.0f) * 2.0f);
		tv82 = tv_plain(p);
		// 173
		p = sat(k[83] * R02 + p);
		pm83 = w24(p); em83 = true;
		tv83 = tv_plain(p);
		T3 = tv81;
		// 174
		p = T2 * M17 + 0.0f;
		pr84 = w24(p); er84 = true;
		tv84 = tv_plain(p);
		// 175
		p = sat((k[85] * M19 + 0.0f) * 2.0f);
		tv85 = tv_plain(p);
		// 176
		if (em83) M17 = pm83;
		p = sat(k[86] * R04 + p);
		pm86 = w24(p); em86 = true;
		tv86 = tv_plain(p);
		// 177
		if (er84) R03 = pr84;
		p = T2 * M19 + 0.0f;
		pr87 = w24(p); er87 = true;
		tv87 = tv_plain(p);
		// 178
		p = T3 * R02 + 0.0f;
		pr88 = w24(p); er88 = true;
		tv88 = tv_plain(p);
		// 179
		if (em86) M19 = pm86;
		p = T3 * R04 + 0.0f;
		pr89 = w24(p); er89 = true;
		tv89 = tv_plain(p);
		// 17a
		if (er87) R08 = pr87;
		p = k[90] * R01 + 0.0f;
		tv90 = tv_plain(p);
		// 17b
		if (er88) R05 = pr88;
		p = k[91] * R03 + p;
		tv91 = tv_plain(p);
		// 17c
		if (er89) R09 = pr89;
		p = sat((k[92] * R05 + p) * 16.0f);
		pr92 = w24(p); er92 = true;
		tv92 = tv_plain(p);
		// 17d
		p = k[93] * R07 + 0.0f;
		tv93 = tv_plain(p);
		// 17e
		p = k[94] * R08 + p;
		tv94 = tv_plain(p);
		// 17f
		if (er92) R58 = pr92;
		p = sat((k[95] * R09 + p) * 16.0f);
		pr95 = w24(p); er95 = true;
		tv95 = tv_plain(p);
		if (er95) R59 = pr95;
		out[0] = M2c;
		out[1] = M2d;
	}

private:
	int32_t IX = 0;
	int32_t IX2 = 0;
	float M01 = 0.0f;
	float M02 = 0.0f;
	float M03 = 0.0f;
	float M04 = 0.0f;
	float M05 = 0.0f;
	float M06 = 0.0f;
	float M07 = 0.0f;
	float M16 = 0.0f;
	float M17 = 0.0f;
	float M18 = 0.0f;
	float M19 = 0.0f;
	float M2c = 0.0f;
	float M2d = 0.0f;
	float R01 = 0.0f;
	float R02 = 0.0f;
	float R03 = 0.0f;
	float R04 = 0.0f;
	float R05 = 0.0f;
	float R06 = 0.0f;
	float R07 = 0.0f;
	float R08 = 0.0f;
	float R09 = 0.0f;
	float R44 = 0.0f;
	float R45 = 0.0f;
	float R46 = 0.0f;
	float R47 = 0.0f;
	float R4c = 0.0f;
	float R4d = 0.0f;
	float R4e = 0.0f;
	float R4f = 0.0f;
	float R50 = 0.0f;
	float R51 = 0.0f;
	float R58 = 0.0f;
	float R59 = 0.0f;
	float RRD = 0.0f;
	float RWR = 0.0f;
	float T1 = 0.0f;
	float T2 = 0.0f;
	float T3 = 0.0f;
	float T7 = 0.0f;
	bool fn = false;
	bool fz = false;
	float p = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_MULTICOMP_H
