// license:BSD-3-Clause
// S-MU2000: バリエーション: SLICE
//
// **tools/meg_fx/emit_dyn.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。このプログラムは分岐（条件つきで
// 先の命令を飛ばす）を持つので、レジスタを変数として持ち、3 命令遅れの書き込みを「書くかどうか」の印つきで回す。
// 係数と番地は firmware が MEG に書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d

#ifndef S_MU2000_DSP_MEG_FX_VAR_SLICE_H
#define S_MU2000_DSP_MEG_FX_VAR_SLICE_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_slice : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); IX = 0; IX2 = 0; M01 = 0.0f; M02 = 0.0f; M03 = 0.0f; M04 = 0.0f; M07 = 0.0f; M08 = 0.0f; M2c = 0.0f; M2d = 0.0f; R01 = 0.0f; R02 = 0.0f; R03 = 0.0f; R04 = 0.0f; R07 = 0.0f; R44 = 0.0f; R45 = 0.0f; R46 = 0.0f; R47 = 0.0f; R48 = 0.0f; R49 = 0.0f; R4a = 0.0f; R4d = 0.0f; R4e = 0.0f; R4f = 0.0f; R50 = 0.0f; R58 = 0.0f; R59 = 0.0f; R5a = 0.0f; R5b = 0.0f; R5c = 0.0f; R5d = 0.0f; RRD = 0.0f; RWR = 0.0f; T1 = 0.0f; T2 = 0.0f; T3 = 0.0f; T7 = 0.0f; fn = false; fz = false; p = 0.0f; }

	// in: 入口。out: 出口。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		float tv0;
		int32_t pi0;
		bool ei0 = false;
		float tv1;
		float tv2;
		float tv3;
		float pq3;
		bool eq3 = false;
		float tv4;
		float tv5;
		float pm5;
		bool em5 = false;
		float tv6;
		int32_t pi6;
		bool ei6 = false;
		float pq6;
		bool eq6 = false;
		float tv7;
		float tv8;
		float pm8;
		bool em8 = false;
		float tv9;
		float pr9;
		bool er9 = false;
		float pq9;
		bool eq9 = false;
		float tv10;
		float tv11;
		float pm11;
		bool em11 = false;
		float pr11;
		bool er11 = false;
		float tv12;
		int32_t pi12;
		bool ei12 = false;
		float pq12;
		bool eq12 = false;
		float tv13;
		float tv14;
		float pm14;
		bool em14 = false;
		float tv15;
		float pr15;
		bool er15 = false;
		float pq15;
		bool eq15 = false;
		float tv16;
		float tv17;
		float pm17;
		bool em17 = false;
		float pr17;
		bool er17 = false;
		float tv18;
		int32_t pi18;
		bool ei18 = false;
		float pq18;
		bool eq18 = false;
		float tv19;
		float tv20;
		float pm20;
		bool em20 = false;
		float tv21;
		float pr21;
		bool er21 = false;
		float pq21;
		bool eq21 = false;
		float tv22;
		float tv23;
		float pm23;
		bool em23 = false;
		float pr23;
		bool er23 = false;
		float tv24;
		float pq24;
		bool eq24 = false;
		float tv25;
		float pm25;
		bool em25 = false;
		float tv26;
		float pm26;
		bool em26 = false;
		float tv27;
		float tv28;
		float tv29;
		float pr29;
		bool er29 = false;
		float tv30;
		float tv31;
		float tv32;
		float pm32;
		bool em32 = false;
		float tv33;
		float tv34;
		float tv35;
		float tv36;
		float pr36;
		bool er36 = false;
		float tv37;
		float tv38;
		float tv39;
		float pr39;
		bool er39 = false;
		float tv40;
		float tv41;
		float pr41;
		bool er41 = false;
		float tv42;
		float pr42;
		bool er42 = false;
		float tv43;
		float pr43;
		bool er43 = false;
		float tv44;
		float tv45;
		float tv46;
		float pr46;
		bool er46 = false;
		float tv47;
		float tv48;
		float tv49;
		float pr49;
		bool er49 = false;
		float tv50;
		float pr50;
		bool er50 = false;
		float tv51;
		float tv52;
		float tv53;
		float pr53;
		bool er53 = false;
		float tv54;
		float pr54;
		bool er54 = false;
		float tv55;
		float tv56;
		float tv57;
		float pr57;
		bool er57 = false;
		float tv58;
		float tv59;
		float tv60;
		float pr60;
		bool er60 = false;
		float tv61;
		float pr61;
		bool er61 = false;
		float tv62;
		float tv63;
		float tv64;
		float pr64;
		bool er64 = false;
		float tv65;
		float pm65;
		bool em65 = false;
		float tv66;
		float pm66;
		bool em66 = false;
		float tv67;
		float tv68;
		float pr68;
		bool er68 = false;
		float tv69;
		float pr69;
		bool er69 = false;
		float tv70;
		float pr70;
		bool er70 = false;
		float tv71;
		float pr71;
		bool er71 = false;
		float tv72;
		float tv73;
		float pr73;
		bool er73 = false;
		float tv74;
		float tv75;
		float pr75;
		bool er75 = false;
		float tv76;
		float pm76;
		bool em76 = false;
		float tv77;
		float tv78;
		float pm78;
		bool em78 = false;
		float tv79;
		float pm79;
		bool em79 = false;
		float tv80;
		float tv81;
		float pm81;
		bool em81 = false;
		float tv82;
		float pm82;
		bool em82 = false;
		float tv83;
		float tv84;
		float pm84;
		bool em84 = false;
		float tv85;
		float tv86;
		float tv87;
		float pm87;
		bool em87 = false;
		float tv88;
		float pm88;
		bool em88 = false;
		float tv89;
		float tv90;
		float pr90;
		bool er90 = false;
		float tv91;
		float tv92;
		float tv93;
		float pm93;
		bool em93 = false;
		float tv94;
		float tv95;
		float pm95;
		bool em95 = false;
		int skip = 0;
		M2c = in[0];
		M2d = in[1];
		// 120
		if (skip && 288 >= skip) skip = 0;
		if (!skip) {
			p = k[0] * R4a + 0.0f;
			pi0 = idx_of(p); ei0 = true;
			tv0 = tv_index(p);
			T2 = k[0];
		} else
			tv0 = tv_plain(p);
		// 121
		if (skip && 289 >= skip) skip = 0;
		if (!skip) {
			tv1 = tv_plain(p);
		} else
			tv1 = tv_plain(p);
		// 122
		if (skip && 290 >= skip) skip = 0;
		if (!skip) {
			tv2 = tv_plain(p);
			T7 = tv0;
		} else
			tv2 = tv_plain(p);
		// 123
		if (skip && 291 >= skip) skip = 0;
		if (ei0) IX = pi0;
		if (!skip) {
			tv3 = tv_plain(p);
			pq3 = tab(3, IX); eq3 = true;
		} else
			tv3 = tv_plain(p);
		// 124
		if (skip && 292 >= skip) skip = 0;
		if (!skip) {
			tv4 = tv_plain(p);
		} else
			tv4 = tv_plain(p);
		// 125
		if (skip && 293 >= skip) skip = 0;
		if (eq3) RRD = pq3;
		if (!skip) {
			pm5 = RRD; em5 = true;
			tv5 = tv_plain(p);
		} else
			tv5 = tv_plain(p);
		// 126
		if (skip && 294 >= skip) skip = 0;
		if (!skip) {
			p = T2 * R4a + 0.0f;
			pi6 = idx_of(p); ei6 = true;
			tv6 = tv_index(p);
			pq6 = tab(6, IX + 1); eq6 = true;
		} else
			tv6 = tv_plain(p);
		// 127
		if (skip && 295 >= skip) skip = 0;
		if (!skip) {
			tv7 = tv_plain(p);
		} else
			tv7 = tv_plain(p);
		// 128
		if (skip && 296 >= skip) skip = 0;
		if (em5) M01 = pm5;
		if (eq6) RRD = pq6;
		if (!skip) {
			p = k[8] + 0.0f;
			pm8 = RRD; em8 = true;
			tv8 = tv_plain(p);
			T1 = tv6;
		} else
			tv8 = tv_plain(p);
		// 129
		if (skip && 297 >= skip) skip = 0;
		if (ei6) IX = pi6;
		if (!skip) {
			p = R4a - p;
			pr9 = w24(p); er9 = true;
			tv9 = tv_plain(p);
			pq9 = tab(9, IX); eq9 = true;
		} else
			tv9 = tv_plain(p);
		// 12a
		if (skip && 298 >= skip) skip = 0;
		if (!skip) {
			p = T7 * M01 - M01;
			tv10 = tv_plain(p);
		} else
			tv10 = tv_plain(p);
		// 12b
		if (skip && 299 >= skip) skip = 0;
		if (em8) M02 = pm8;
		if (eq9) RRD = pq9;
		if (!skip) {
			p = sat(T7 * M02 - p);
			pm11 = RRD; em11 = true;
			pr11 = w24(p); er11 = true;
			tv11 = tv_plain(p);
		} else
			tv11 = tv_plain(p);
		// 12c
		if (skip && 300 >= skip) skip = 0;
		if (er9) R07 = pr9;
		if (!skip) {
			p = T2 * R07 + 0.0f;
			pi12 = idx_of(p); ei12 = true;
			tv12 = tv_index(p);
			pq12 = tab(12, IX + 1); eq12 = true;
		} else
			tv12 = tv_plain(p);
		// 12d
		if (skip && 301 >= skip) skip = 0;
		if (!skip) {
			tv13 = tv_plain(p);
		} else
			tv13 = tv_plain(p);
		// 12e
		if (skip && 302 >= skip) skip = 0;
		if (em11) M01 = pm11;
		if (er11) R4d = pr11;
		if (eq12) RRD = pq12;
		if (!skip) {
			p = k[14] + 0.0f;
			pm14 = RRD; em14 = true;
			tv14 = tv_plain(p);
			T7 = tv12;
		} else
			tv14 = tv_plain(p);
		// 12f
		if (skip && 303 >= skip) skip = 0;
		if (ei12) IX = pi12;
		if (!skip) {
			p = R4a - p;
			pr15 = w24(p); er15 = true;
			tv15 = tv_plain(p);
			pq15 = tab(15, IX); eq15 = true;
		} else
			tv15 = tv_plain(p);
		// 130
		if (skip && 304 >= skip) skip = 0;
		if (!skip) {
			p = T1 * M01 - M01;
			tv16 = tv_plain(p);
		} else
			tv16 = tv_plain(p);
		// 131
		if (skip && 305 >= skip) skip = 0;
		if (em14) M02 = pm14;
		if (eq15) RRD = pq15;
		if (!skip) {
			p = sat(T1 * M02 - p);
			pm17 = RRD; em17 = true;
			pr17 = w24(p); er17 = true;
			tv17 = tv_plain(p);
		} else
			tv17 = tv_plain(p);
		// 132
		if (skip && 306 >= skip) skip = 0;
		if (er15) R07 = pr15;
		if (!skip) {
			p = T2 * R07 + 0.0f;
			pi18 = idx_of(p); ei18 = true;
			tv18 = tv_index(p);
			pq18 = tab(18, IX + 1); eq18 = true;
		} else
			tv18 = tv_plain(p);
		// 133
		if (skip && 307 >= skip) skip = 0;
		if (!skip) {
			p = k[19] + 0.0f;
			tv19 = tv_plain(p);
		} else
			tv19 = tv_plain(p);
		// 134
		if (skip && 308 >= skip) skip = 0;
		if (em17) M01 = pm17;
		if (er17) R4e = pr17;
		if (eq18) RRD = pq18;
		if (!skip) {
			p = sat(k[20] + (p * (1.0f / 32768.0f)));
			pm20 = RRD; em20 = true;
			tv20 = tv_plain(p);
			T1 = tv18;
		} else
			tv20 = tv_plain(p);
		// 135
		if (skip && 309 >= skip) skip = 0;
		if (ei18) IX = pi18;
		if (!skip) {
			p = R4a + p;
			pr21 = w24(p); er21 = true;
			tv21 = tv_plain(p);
			pq21 = tab(21, IX); eq21 = true;
		} else
			tv21 = tv_plain(p);
		// 136
		if (skip && 310 >= skip) skip = 0;
		if (!skip) {
			p = T7 * M01 - M01;
			tv22 = tv_plain(p);
		} else
			tv22 = tv_plain(p);
		// 137
		if (skip && 311 >= skip) skip = 0;
		if (em20) M02 = pm20;
		if (eq21) RRD = pq21;
		if (!skip) {
			p = sat(T7 * M02 - p);
			pm23 = RRD; em23 = true;
			pr23 = w24(p); er23 = true;
			tv23 = tv_plain(p);
		} else
			tv23 = tv_plain(p);
		// 138
		if (skip && 312 >= skip) skip = 0;
		if (er21) R4a = pr21;
		if (!skip) {
			tv24 = tv_plain(p);
			pq24 = tab(24, IX + 1); eq24 = true;
		} else
			tv24 = tv_plain(p);
		// 139
		if (skip && 313 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[25] * R4e + 0.0f);
			pm25 = w24(p); em25 = true;
			tv25 = tv_plain(p);
		} else
			tv25 = tv_plain(p);
		// 13a
		if (skip && 314 >= skip) skip = 0;
		if (em23) M01 = pm23;
		if (er23) R4f = pr23;
		if (eq24) RRD = pq24;
		if (!skip) {
			pm26 = RRD; em26 = true;
			tv26 = tv_plain(p);
		} else
			tv26 = tv_plain(p);
		// 13b
		if (skip && 315 >= skip) skip = 0;
		if (!skip) {
			tv27 = tv_plain(p);
		} else
			tv27 = tv_plain(p);
		// 13c
		if (skip && 316 >= skip) skip = 0;
		if (em25) M07 = pm25;
		if (!skip) {
			p = T1 * M01 - M01;
			tv28 = tv_plain(p);
		} else
			tv28 = tv_plain(p);
		// 13d
		if (skip && 317 >= skip) skip = 0;
		if (em26) M02 = pm26;
		if (!skip) {
			p = sat(T1 * M02 - p);
			pr29 = w24(p); er29 = true;
			tv29 = tv_plain(p);
		} else
			tv29 = tv_plain(p);
		// 13e
		if (skip && 318 >= skip) skip = 0;
		if (!skip) {
			p = M07 + 0.0f;
			tv30 = tv_plain(p);
		} else
			tv30 = tv_plain(p);
		// 13f
		if (skip && 319 >= skip) skip = 0;
		if (!skip) {
			p = 0.0f - p;
			tv31 = tv_plain(p);
		} else
			tv31 = tv_plain(p);
		// 140
		if (skip && 320 >= skip) skip = 0;
		if (er29) R50 = pr29;
		if (!skip) {
			p = sat(k[32] + p);
			pm32 = w24(p); em32 = true;
			tv32 = tv_plain(p);
		} else
			tv32 = tv_plain(p);
		// 141
		if (skip && 321 >= skip) skip = 0;
		if (!skip) {
			tv33 = tv_plain(p);
		} else
			tv33 = tv_plain(p);
		// 142
		if (skip && 322 >= skip) skip = 0;
		if (!skip) {
			tv34 = tv_plain(p);
		} else
			tv34 = tv_plain(p);
		// 143
		if (skip && 323 >= skip) skip = 0;
		if (em32) M08 = pm32;
		if (!skip) {
			p = k[35] * R4f + 0.0f;
			tv35 = tv_plain(p);
		} else
			tv35 = tv_plain(p);
		// 144
		if (skip && 324 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[36] + p);
			pr36 = w24(p); er36 = true;
			tv36 = tv_plain(p);
		} else
			tv36 = tv_plain(p);
		// 145
		if (skip && 325 >= skip) skip = 0;
		if (!skip) {
			p = k[37] * R5c + 0.0f;
			tv37 = tv_plain(p);
		} else
			tv37 = tv_plain(p);
		// 146
		if (skip && 326 >= skip) skip = 0;
		if (!skip) {
			p = k[38] * R5d + p;
			tv38 = tv_plain(p);
		} else
			tv38 = tv_plain(p);
		// 147
		if (skip && 327 >= skip) skip = 0;
		if (er36) R5c = pr36;
		if (!skip) {
			p = sat((k[39] * R5c + p) * 2.0f);
			pr39 = w24(p); er39 = true;
			tv39 = tv_plain(p);
		} else
			tv39 = tv_plain(p);
		// 148
		if (skip && 328 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[40] * R4d + 0.0f);
			tv40 = tv_plain(p);
		} else
			tv40 = tv_plain(p);
		// 149
		if (skip && 329 >= skip) skip = 0;
		if (!skip) {
			p = satpos(k[41] * R50 + p);
			pr41 = w24(p); er41 = true;
			tv41 = tv_plain(p);
			T2 = tv39;
		} else
			tv41 = tv_plain(p);
		// 14a
		if (skip && 330 >= skip) skip = 0;
		if (er39) R5d = pr39;
		if (!skip) {
			p = sat(k[42] + p);
			pr42 = w24(p); er42 = true;
			tv42 = tv_plain(p);
		} else
			tv42 = tv_plain(p);
		// 14b
		if (skip && 331 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[43] * M2c + 0.0f) * 2.0f);
			pr43 = w24(p); er43 = true;
			tv43 = tv_plain(p);
		} else
			tv43 = tv_plain(p);
		// 14c
		if (skip && 332 >= skip) skip = 0;
		if (er41) R01 = pr41;
		if (!skip) {
			p = k[44] * R44 + 0.0f;
			tv44 = tv_plain(p);
			T1 = tv42;
		} else
			tv44 = tv_plain(p);
		// 14d
		if (skip && 333 >= skip) skip = 0;
		if (er42) R02 = pr42;
		if (!skip) {
			p = k[45] * R45 + p;
			tv45 = tv_plain(p);
		} else
			tv45 = tv_plain(p);
		// 14e
		if (skip && 334 >= skip) skip = 0;
		if (er43) R44 = pr43;
		if (!skip) {
			p = sat((k[46] * R44 + p) * 2.0f);
			pr46 = w24(p); er46 = true;
			tv46 = tv_plain(p);
		} else
			tv46 = tv_plain(p);
		// 14f
		if (skip && 335 >= skip) skip = 0;
		if (!skip) {
			p = k[47] * R45 + 0.0f;
			tv47 = tv_plain(p);
		} else
			tv47 = tv_plain(p);
		// 150
		if (skip && 336 >= skip) skip = 0;
		if (!skip) {
			p = k[48] * R46 + p;
			tv48 = tv_plain(p);
		} else
			tv48 = tv_plain(p);
		// 151
		if (skip && 337 >= skip) skip = 0;
		if (er46) R45 = pr46;
		if (!skip) {
			p = sat((k[49] * R45 + p) * 4.0f);
			pr49 = w24(p); er49 = true;
			tv49 = tv_plain(p);
		} else
			tv49 = tv_plain(p);
		// 152
		if (skip && 338 >= skip) skip = 0;
		if (!skip) {
			p = T1 * M07 + 0.0f;
			pr50 = w24(p); er50 = true;
			tv50 = tv_plain(p);
		} else
			tv50 = tv_plain(p);
		// 153
		if (skip && 339 >= skip) skip = 0;
		if (!skip) {
			p = k[51] * R58 + 0.0f;
			tv51 = tv_plain(p);
		} else
			tv51 = tv_plain(p);
		// 154
		if (skip && 340 >= skip) skip = 0;
		if (er49) R46 = pr49;
		if (!skip) {
			p = k[52] * R59 + p;
			tv52 = tv_plain(p);
		} else
			tv52 = tv_plain(p);
		// 155
		if (skip && 341 >= skip) skip = 0;
		if (er50) R58 = pr50;
		if (!skip) {
			p = sat((k[53] * R58 + p) * 2.0f);
			pr53 = w24(p); er53 = true;
			tv53 = tv_plain(p);
		} else
			tv53 = tv_plain(p);
		// 156
		if (skip && 342 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[54] * M2d + 0.0f) * 2.0f);
			pr54 = w24(p); er54 = true;
			tv54 = tv_plain(p);
		} else
			tv54 = tv_plain(p);
		// 157
		if (skip && 343 >= skip) skip = 0;
		if (!skip) {
			p = k[55] * R47 + 0.0f;
			tv55 = tv_plain(p);
			T3 = tv53;
		} else
			tv55 = tv_plain(p);
		// 158
		if (skip && 344 >= skip) skip = 0;
		if (er53) R59 = pr53;
		if (!skip) {
			p = k[56] * R48 + p;
			tv56 = tv_plain(p);
		} else
			tv56 = tv_plain(p);
		// 159
		if (skip && 345 >= skip) skip = 0;
		if (er54) R47 = pr54;
		if (!skip) {
			p = sat((k[57] * R47 + p) * 2.0f);
			pr57 = w24(p); er57 = true;
			tv57 = tv_plain(p);
		} else
			tv57 = tv_plain(p);
		// 15a
		if (skip && 346 >= skip) skip = 0;
		if (!skip) {
			p = k[58] * R48 + 0.0f;
			tv58 = tv_plain(p);
		} else
			tv58 = tv_plain(p);
		// 15b
		if (skip && 347 >= skip) skip = 0;
		if (!skip) {
			p = k[59] * R49 + p;
			tv59 = tv_plain(p);
		} else
			tv59 = tv_plain(p);
		// 15c
		if (skip && 348 >= skip) skip = 0;
		if (er57) R48 = pr57;
		if (!skip) {
			p = sat((k[60] * R48 + p) * 4.0f);
			pr60 = w24(p); er60 = true;
			tv60 = tv_plain(p);
		} else
			tv60 = tv_plain(p);
		// 15d
		if (skip && 349 >= skip) skip = 0;
		if (!skip) {
			p = T1 * M08 + 0.0f;
			pr61 = w24(p); er61 = true;
			tv61 = tv_plain(p);
		} else
			tv61 = tv_plain(p);
		// 15e
		if (skip && 350 >= skip) skip = 0;
		if (!skip) {
			p = k[62] * R5a + 0.0f;
			tv62 = tv_plain(p);
		} else
			tv62 = tv_plain(p);
		// 15f
		if (skip && 351 >= skip) skip = 0;
		if (er60) R49 = pr60;
		if (!skip) {
			p = k[63] * R5b + p;
			tv63 = tv_plain(p);
		} else
			tv63 = tv_plain(p);
		// 160
		if (skip && 352 >= skip) skip = 0;
		if (er61) R5a = pr61;
		if (!skip) {
			p = sat((k[64] * R5a + p) * 2.0f);
			pr64 = w24(p); er64 = true;
			tv64 = tv_plain(p);
		} else
			tv64 = tv_plain(p);
		// 161
		if (skip && 353 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[65] * R46 + 0.0f) * 2.0f);
			pm65 = w24(p); em65 = true;
			tv65 = tv_plain(p);
		} else
			tv65 = tv_plain(p);
		// 162
		if (skip && 354 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[66] * R49 + 0.0f) * 2.0f);
			pm66 = w24(p); em66 = true;
			tv66 = tv_plain(p);
			T7 = tv64;
		} else
			tv66 = tv_plain(p);
		// 163
		if (skip && 355 >= skip) skip = 0;
		if (er64) R5b = pr64;
		if (!skip) {
			tv67 = tv_plain(p);
		} else
			tv67 = tv_plain(p);
		// 164
		if (skip && 356 >= skip) skip = 0;
		if (em65) M03 = pm65;
		if (!skip) {
			p = sat(T3 * M03 + 0.0f);
			pr68 = w24(p); er68 = true;
			tv68 = tv_plain(p);
		} else
			tv68 = tv_plain(p);
		// 165
		if (skip && 357 >= skip) skip = 0;
		if (em66) M04 = pm66;
		if (!skip) {
			p = sat(T7 * M03 + 0.0f);
			pr69 = w24(p); er69 = true;
			tv69 = tv_plain(p);
		} else
			tv69 = tv_plain(p);
		// 166
		if (skip && 358 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * M04 + 0.0f);
			pr70 = w24(p); er70 = true;
			tv70 = tv_plain(p);
		} else
			tv70 = tv_plain(p);
		// 167
		if (skip && 359 >= skip) skip = 0;
		if (er68) R01 = pr68;
		if (!skip) {
			p = sat(T7 * M04 + 0.0f);
			pr71 = w24(p); er71 = true;
			tv71 = tv_plain(p);
		} else
			tv71 = tv_plain(p);
		// 168
		if (skip && 360 >= skip) skip = 0;
		if (er69) R02 = pr69;
		if (!skip) {
			p = k[72] * R01 + 0.0f;
			tv72 = tv_plain(p);
		} else
			tv72 = tv_plain(p);
		// 169
		if (skip && 361 >= skip) skip = 0;
		if (er70) R03 = pr70;
		if (!skip) {
			p = sat((k[73] * R02 + p) * 16.0f);
			pr73 = w24(p); er73 = true;
			tv73 = tv_plain(p);
		} else
			tv73 = tv_plain(p);
		// 16a
		if (skip && 362 >= skip) skip = 0;
		if (er71) R04 = pr71;
		if (!skip) {
			p = k[74] * R03 + 0.0f;
			tv74 = tv_plain(p);
		} else
			tv74 = tv_plain(p);
		// 16b
		if (skip && 363 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[75] * R04 + p) * 16.0f);
			pr75 = w24(p); er75 = true;
			tv75 = tv_plain(p);
		} else
			tv75 = tv_plain(p);
		// 16c
		if (skip && 364 >= skip) skip = 0;
		if (er73) R01 = pr73;
		if (!skip) {
			p = sat(T2 * R01 + 0.0f);
			pm76 = w24(p); em76 = true;
			tv76 = tv_plain(p);
		} else
			tv76 = tv_plain(p);
		// 16d
		if (skip && 365 >= skip) skip = 0;
		if (!skip) {
			tv77 = tv_plain(p);
		} else
			tv77 = tv_plain(p);
		// 16e
		if (skip && 366 >= skip) skip = 0;
		if (er75) R02 = pr75;
		if (!skip) {
			p = sat(T2 * R02 + 0.0f);
			pm78 = w24(p); em78 = true;
			tv78 = tv_plain(p);
		} else
			tv78 = tv_plain(p);
		// 16f
		if (skip && 367 >= skip) skip = 0;
		if (em76) M01 = pm76;
		if (!skip) {
			p = sat((k[79] * M01 + 0.0f) * 16.0f);
			pm79 = w24(p); em79 = true;
			tv79 = tv_plain(p);
		} else
			tv79 = tv_plain(p);
		// 170
		if (skip && 368 >= skip) skip = 0;
		if (!skip) {
			tv80 = tv_plain(p);
		} else
			tv80 = tv_plain(p);
		// 171
		if (skip && 369 >= skip) skip = 0;
		if (em78) M02 = pm78;
		if (!skip) {
			p = sat((k[81] * M02 + 0.0f) * 16.0f);
			pm81 = w24(p); em81 = true;
			tv81 = tv_plain(p);
		} else
			tv81 = tv_plain(p);
		// 172
		if (skip && 370 >= skip) skip = 0;
		if (em79) M01 = pm79;
		if (!skip) {
			p = sat((k[82] * M01 + 0.0f) * 16.0f);
			pm82 = w24(p); em82 = true;
			tv82 = tv_plain(p);
		} else
			tv82 = tv_plain(p);
		// 173
		if (skip && 371 >= skip) skip = 0;
		if (!skip) {
			tv83 = tv_plain(p);
		} else
			tv83 = tv_plain(p);
		// 174
		if (skip && 372 >= skip) skip = 0;
		if (em81) M02 = pm81;
		if (!skip) {
			p = sat((k[84] * M02 + 0.0f) * 16.0f);
			pm84 = w24(p); em84 = true;
			tv84 = tv_plain(p);
		} else
			tv84 = tv_plain(p);
		// 175
		if (skip && 373 >= skip) skip = 0;
		if (em82) M01 = pm82;
		if (!skip) {
			tv85 = tv_plain(p);
		} else
			tv85 = tv_plain(p);
		// 176
		if (skip && 374 >= skip) skip = 0;
		if (!skip) {
			tv86 = tv_plain(p);
		} else
			tv86 = tv_plain(p);
		// 177
		if (skip && 375 >= skip) skip = 0;
		if (em84) M02 = pm84;
		if (!skip) {
			p = sat(k[87] * M01 + 0.0f);
			pm87 = w24(p); em87 = true;
			tv87 = tv_plain(p);
		} else
			tv87 = tv_plain(p);
		// 178
		if (skip && 376 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[88] * M02 + 0.0f);
			pm88 = w24(p); em88 = true;
			tv88 = tv_plain(p);
		} else
			tv88 = tv_plain(p);
		// 179
		if (skip && 377 >= skip) skip = 0;
		if (!skip && true && 380 > 377) skip = 380;
		tv89 = tv_plain(p);
		// 17a
		if (skip && 378 >= skip) skip = 0;
		if (em87) M01 = pm87;
		if (!skip) {
			p = k[90] + 0.0f;
			pr90 = w24(p); er90 = true;
			tv90 = tv_plain(p);
		} else
			tv90 = tv_plain(p);
		// 17b
		if (skip && 379 >= skip) skip = 0;
		if (em88) M02 = pm88;
		if (!skip && true && 256 > 379) skip = 256;
		tv91 = tv_plain(p);
		// 17c
		if (skip && 380 >= skip) skip = 0;
		if (!skip) {
			p = k[92] * M01 + 0.0f;
			tv92 = tv_plain(p);
		} else
			tv92 = tv_plain(p);
		// 17d
		if (skip && 381 >= skip) skip = 0;
		if (er90) R4a = pr90;
		if (!skip) {
			p = sat((k[93] * M03 + p) * 4.0f);
			pm93 = w24(p); em93 = true;
			tv93 = tv_plain(p);
		} else
			tv93 = tv_plain(p);
		// 17e
		if (skip && 382 >= skip) skip = 0;
		if (!skip) {
			p = k[94] * M02 + 0.0f;
			tv94 = tv_plain(p);
		} else
			tv94 = tv_plain(p);
		// 17f
		if (skip && 383 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[95] * M04 + p) * 4.0f);
			pm95 = w24(p); em95 = true;
			tv95 = tv_plain(p);
		} else
			tv95 = tv_plain(p);
		if (em93) M2c = pm93;
		if (em95) M2d = pm95;
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
	float M07 = 0.0f;
	float M08 = 0.0f;
	float M2c = 0.0f;
	float M2d = 0.0f;
	float R01 = 0.0f;
	float R02 = 0.0f;
	float R03 = 0.0f;
	float R04 = 0.0f;
	float R07 = 0.0f;
	float R44 = 0.0f;
	float R45 = 0.0f;
	float R46 = 0.0f;
	float R47 = 0.0f;
	float R48 = 0.0f;
	float R49 = 0.0f;
	float R4a = 0.0f;
	float R4d = 0.0f;
	float R4e = 0.0f;
	float R4f = 0.0f;
	float R50 = 0.0f;
	float R58 = 0.0f;
	float R59 = 0.0f;
	float R5a = 0.0f;
	float R5b = 0.0f;
	float R5c = 0.0f;
	float R5d = 0.0f;
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

#endif // S_MU2000_DSP_MEG_FX_VAR_SLICE_H
