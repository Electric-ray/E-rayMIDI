// license:BSD-3-Clause
// S-MU2000: バリエーション: LO-FI
//
// **tools/meg_fx/emit_dyn.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。このプログラムは分岐（条件つきで
// 先の命令を飛ばす）を持つので、レジスタを変数として持ち、3 命令遅れの書き込みを「書くかどうか」の印つきで回す。
// 係数と番地は firmware が MEG に書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m2c m2d / 出口: m2c m2d

#ifndef S_MU2000_DSP_MEG_FX_VAR_LOFI_H
#define S_MU2000_DSP_MEG_FX_VAR_LOFI_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_var_lofi : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x2c, 0x2d };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x2c, 0x2d };  // 出口のレジスタ

	void reset() { reset_ram(); IX = 0; IX2 = 0; M16 = 0.0f; M17 = 0.0f; M19 = 0.0f; M2c = 0.0f; M2d = 0.0f; R01 = 0.0f; R02 = 0.0f; R03 = 0.0f; R04 = 0.0f; R44 = 0.0f; R45 = 0.0f; R46 = 0.0f; R47 = 0.0f; R48 = 0.0f; R49 = 0.0f; R4a = 0.0f; R4b = 0.0f; R4c = 0.0f; R4d = 0.0f; R4e = 0.0f; R4f = 0.0f; R50 = 0.0f; R51 = 0.0f; R52 = 0.0f; R53 = 0.0f; R54 = 0.0f; R55 = 0.0f; R56 = 0.0f; R57 = 0.0f; R58 = 0.0f; R59 = 0.0f; R5a = 0.0f; R5b = 0.0f; R5c = 0.0f; R5d = 0.0f; R5e = 0.0f; R5f = 0.0f; R60 = 0.0f; RRD = 0.0f; RWR = 0.0f; T0 = 0.0f; fn = false; fz = false; p = 0.0f; }

	// in: 入口。out: 出口。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		float tv0;
		float pr0;
		bool er0 = false;
		float tv1;
		float pm1;
		bool em1 = false;
		float tv2;
		float tv3;
		float tv4;
		float pr4;
		bool er4 = false;
		float tv5;
		float pr5;
		bool er5 = false;
		float tv6;
		float tv7;
		float pr7;
		bool er7 = false;
		float tv8;
		float tv9;
		float tv10;
		float pr10;
		bool er10 = false;
		float tv11;
		float tv12;
		float pr12;
		bool er12 = false;
		float tv13;
		float tv14;
		float pr14;
		bool er14 = false;
		float tv15;
		float pr15;
		bool er15 = false;
		float tv16;
		float pr16;
		bool er16 = false;
		float tv17;
		float tv18;
		float tv19;
		float pr19;
		bool er19 = false;
		float tv20;
		float tv21;
		float pr21;
		bool er21 = false;
		float tv22;
		float tv23;
		float pr23;
		bool er23 = false;
		float tv24;
		float pr24;
		bool er24 = false;
		float tv25;
		float tv26;
		float pm26;
		bool em26 = false;
		float tv27;
		float pm27;
		bool em27 = false;
		float tv28;
		float pr28;
		bool er28 = false;
		float tv29;
		float pr29;
		bool er29 = false;
		float tv30;
		float pr30;
		bool er30 = false;
		float tv31;
		float tv32;
		float pm32;
		bool em32 = false;
		float tv33;
		float tv34;
		float tv35;
		float tv36;
		float pm36;
		bool em36 = false;
		float tv37;
		float tv38;
		float pr38;
		bool er38 = false;
		float tv39;
		float tv40;
		float tv41;
		float tv42;
		float tv43;
		float pr43;
		bool er43 = false;
		float tv44;
		float tv45;
		float tv46;
		float tv47;
		float pr47;
		bool er47 = false;
		float tv48;
		float tv49;
		float tv50;
		float tv51;
		float tv52;
		float pr52;
		bool er52 = false;
		float tv53;
		float tv54;
		float pr54;
		bool er54 = false;
		float tv55;
		float tv56;
		float pr56;
		bool er56 = false;
		float tv57;
		float pr57;
		bool er57 = false;
		float tv58;
		float tv59;
		float pr59;
		bool er59 = false;
		float tv60;
		float pr60;
		bool er60 = false;
		float tv61;
		float tv62;
		float pr62;
		bool er62 = false;
		float tv63;
		float pr63;
		bool er63 = false;
		float tv64;
		float pr64;
		bool er64 = false;
		float tv65;
		float tv66;
		float pr66;
		bool er66 = false;
		float tv67;
		float tv68;
		float tv69;
		float pr69;
		bool er69 = false;
		float tv70;
		float tv71;
		float pr71;
		bool er71 = false;
		float tv72;
		float tv73;
		float tv74;
		float pr74;
		bool er74 = false;
		float tv75;
		float tv76;
		float pr76;
		bool er76 = false;
		float tv77;
		float pr77;
		bool er77 = false;
		float tv78;
		float tv79;
		float tv80;
		float pr80;
		bool er80 = false;
		float tv81;
		float pr81;
		bool er81 = false;
		float tv82;
		float tv83;
		float tv84;
		float pr84;
		bool er84 = false;
		float tv85;
		float tv86;
		float pr86;
		bool er86 = false;
		float tv87;
		float tv88;
		float tv89;
		float pr89;
		bool er89 = false;
		float tv90;
		float tv91;
		float pr91;
		bool er91 = false;
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
		if (skip && 288 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[0] + R49);
			pr0 = w24(p); er0 = true;
			tv0 = tv_plain(p);
		} else
			tv0 = tv_plain(p);
		// 121
		if (skip && 289 >= skip) skip = 0;
		if (!skip) {
			p = k[1] + 0.0f;
			pm1 = w24(p); em1 = true;
			tv1 = tv_plain(p);
		} else
			tv1 = tv_plain(p);
		// 122
		if (skip && 290 >= skip) skip = 0;
		if (!skip) {
			p = k[2] + 0.0f;
			tv2 = tv_plain(p);
		} else
			tv2 = tv_plain(p);
		// 123
		if (skip && 291 >= skip) skip = 0;
		if (er0) R49 = pr0;
		if (!skip) {
			p = R49 - p;
			fn = p < 0.0f; fz = p == 0.0f;
			tv3 = tv_plain(p);
		} else
			tv3 = tv_plain(p);
		// 124
		if (skip && 292 >= skip) skip = 0;
		if (em1) M19 = pm1;
		if (!skip) {
			p = sat((k[4] * R50 + 0.0f) * 4.0f);
			pr4 = w24(p); er4 = true;
			tv4 = tv_plain(p);
			T0 = k[4];
		} else
			tv4 = tv_plain(p);
		// 125
		if (skip && 293 >= skip) skip = 0;
		if (!skip) {
			p = sat((T0 * R5c + 0.0f) * 4.0f);
			pr5 = w24(p); er5 = true;
			tv5 = tv_plain(p);
		} else
			tv5 = tv_plain(p);
		// 126
		if (skip && 294 >= skip) skip = 0;
		if (!skip) {
			p = k[6] * M2c + 0.0f;
			tv6 = tv_plain(p);
		} else
			tv6 = tv_plain(p);
		// 127
		if (skip && 295 >= skip) skip = 0;
		if (er4) R03 = pr4;
		if (!skip) {
			p = sat(k[7] * M2d + p);
			pr7 = w24(p); er7 = true;
			tv7 = tv_plain(p);
		} else
			tv7 = tv_plain(p);
		// 128
		if (skip && 296 >= skip) skip = 0;
		if (er5) R04 = pr5;
		if (!skip) {
			p = k[8] * R45 + 0.0f;
			tv8 = tv_plain(p);
		} else
			tv8 = tv_plain(p);
		// 129
		if (skip && 297 >= skip) skip = 0;
		if (!skip) {
			p = k[9] * R44 + p;
			tv9 = tv_plain(p);
		} else
			tv9 = tv_plain(p);
		// 12a
		if (skip && 298 >= skip) skip = 0;
		if (er7) R44 = pr7;
		if (!skip) {
			p = sat((k[10] * R44 + p) * 4.0f);
			pr10 = w24(p); er10 = true;
			tv10 = tv_plain(p);
		} else
			tv10 = tv_plain(p);
		// 12b
		if (skip && 299 >= skip) skip = 0;
		if (!skip) {
			p = k[11] * R46 + 0.0f;
			tv11 = tv_plain(p);
		} else
			tv11 = tv_plain(p);
		// 12c
		if (skip && 300 >= skip) skip = 0;
		if (!skip) {
			p = k[12] * R45 + p;
			pr12 = R45; er12 = true;
			tv12 = tv_plain(p);
		} else
			tv12 = tv_plain(p);
		// 12d
		if (skip && 301 >= skip) skip = 0;
		if (er10) R45 = pr10;
		if (!skip) {
			p = k[13] * R45 + p;
			tv13 = tv_plain(p);
		} else
			tv13 = tv_plain(p);
		// 12e
		if (skip && 302 >= skip) skip = 0;
		if (!skip) {
			p = k[14] * R47 + p;
			pr14 = R47; er14 = true;
			tv14 = tv_plain(p);
		} else
			tv14 = tv_plain(p);
		// 12f
		if (skip && 303 >= skip) skip = 0;
		if (er12) R46 = pr12;
		if (!skip) {
			p = sat((k[15] * R48 + p) * 2.0f);
			pr15 = w24(p); er15 = true;
			tv15 = tv_plain(p);
		} else
			tv15 = tv_plain(p);
		// 130
		if (skip && 304 >= skip) skip = 0;
		if (!skip) {
			p = k[16] * M2d + 0.0f;
			pr16 = w24(p); er16 = true;
			tv16 = tv_plain(p);
		} else
			tv16 = tv_plain(p);
		// 131
		if (skip && 305 >= skip) skip = 0;
		if (er14) R48 = pr14;
		if (!skip) {
			p = k[17] * R52 + 0.0f;
			tv17 = tv_plain(p);
		} else
			tv17 = tv_plain(p);
		// 132
		if (skip && 306 >= skip) skip = 0;
		if (er15) R47 = pr15;
		if (!skip) {
			p = k[18] * R51 + p;
			tv18 = tv_plain(p);
		} else
			tv18 = tv_plain(p);
		// 133
		if (skip && 307 >= skip) skip = 0;
		if (er16) R51 = pr16;
		if (!skip) {
			p = sat((k[19] * R51 + p) * 4.0f);
			pr19 = w24(p); er19 = true;
			tv19 = tv_plain(p);
		} else
			tv19 = tv_plain(p);
		// 134
		if (skip && 308 >= skip) skip = 0;
		if (!skip) {
			p = k[20] * R53 + 0.0f;
			tv20 = tv_plain(p);
		} else
			tv20 = tv_plain(p);
		// 135
		if (skip && 309 >= skip) skip = 0;
		if (!skip) {
			p = k[21] * R52 + p;
			pr21 = R52; er21 = true;
			tv21 = tv_plain(p);
		} else
			tv21 = tv_plain(p);
		// 136
		if (skip && 310 >= skip) skip = 0;
		if (er19) R52 = pr19;
		if (!skip) {
			p = k[22] * R52 + p;
			tv22 = tv_plain(p);
		} else
			tv22 = tv_plain(p);
		// 137
		if (skip && 311 >= skip) skip = 0;
		if (!skip) {
			p = k[23] * R54 + p;
			pr23 = R54; er23 = true;
			tv23 = tv_plain(p);
		} else
			tv23 = tv_plain(p);
		// 138
		if (skip && 312 >= skip) skip = 0;
		if (er21) R53 = pr21;
		if (!skip) {
			p = sat((k[24] * R55 + p) * 2.0f);
			pr24 = w24(p); er24 = true;
			tv24 = tv_plain(p);
		} else
			tv24 = tv_plain(p);
		// 139
		if (skip && 313 >= skip) skip = 0;
		if (!skip && fn && 317 > 313) skip = 317;
		tv25 = tv_plain(p);
		// 13a
		if (skip && 314 >= skip) skip = 0;
		if (er23) R55 = pr23;
		if (!skip) {
			p = R47 + 0.0f;
			pm26 = w24(p); em26 = true;
			tv26 = tv_plain(p);
		} else
			tv26 = tv_plain(p);
		// 13b
		if (skip && 315 >= skip) skip = 0;
		if (er24) R54 = pr24;
		if (!skip) {
			p = R54 + 0.0f;
			pm27 = w24(p); em27 = true;
			tv27 = tv_plain(p);
		} else
			tv27 = tv_plain(p);
		// 13c
		if (skip && 316 >= skip) skip = 0;
		if (!skip) {
			p = k[28] + 0.0f;
			pr28 = w24(p); er28 = true;
			tv28 = tv_plain(p);
		} else
			tv28 = tv_plain(p);
		// 13d
		if (skip && 317 >= skip) skip = 0;
		if (em26) M16 = pm26;
		if (!skip) {
			p = sat(k[29] * R5d + M16);
			pr29 = w24(p); er29 = true;
			tv29 = tv_plain(p);
		} else
			tv29 = tv_plain(p);
		// 13e
		if (skip && 318 >= skip) skip = 0;
		if (em27) M17 = pm27;
		if (!skip) {
			p = sat(k[30] * R5f + M17);
			pr30 = w24(p); er30 = true;
			tv30 = tv_plain(p);
		} else
			tv30 = tv_plain(p);
		// 13f
		if (skip && 319 >= skip) skip = 0;
		if (er28) R49 = pr28;
		if (!skip) {
			p = k[31] * M2c + 0.0f;
			tv31 = tv_plain(p);
		} else
			tv31 = tv_plain(p);
		// 140
		if (skip && 320 >= skip) skip = 0;
		if (er29) R01 = pr29;
		if (!skip) {
			p = sat((k[32] * R03 + p) * 4.0f);
			pm32 = w24(p); em32 = true;
			tv32 = tv_plain(p);
		} else
			tv32 = tv_plain(p);
		// 141
		if (skip && 321 >= skip) skip = 0;
		if (er30) R02 = pr30;
		if (!skip) {
			p = sat(R01 - M19);
			fn = p < 0.0f; fz = p == 0.0f;
			tv33 = tv_plain(p);
		} else
			tv33 = tv_plain(p);
		// 142
		if (skip && 322 >= skip) skip = 0;
		if (!skip) {
			p = k[34] * M2d + 0.0f;
			tv34 = tv_plain(p);
		} else
			tv34 = tv_plain(p);
		// 143
		if (skip && 323 >= skip) skip = 0;
		if (em32) M2c = pm32;
		if (!skip) {
			p = k[35] * R04 + p;
			tv35 = tv_plain(p);
		} else
			tv35 = tv_plain(p);
		// 144
		if (skip && 324 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[36] * R03 + p) * 4.0f);
			pm36 = w24(p); em36 = true;
			tv36 = tv_plain(p);
		} else
			tv36 = tv_plain(p);
		// 145
		if (skip && 325 >= skip) skip = 0;
		if (!skip && fn && 328 > 325) skip = 328;
		tv37 = tv_plain(p);
		// 146
		if (skip && 326 >= skip) skip = 0;
		if (!skip) {
			p = M19 + 0.0f;
			pr38 = w24(p); er38 = true;
			tv38 = tv_plain(p);
		} else
			tv38 = tv_plain(p);
		// 147
		if (skip && 327 >= skip) skip = 0;
		if (em36) M2d = pm36;
		if (!skip && true && 332 > 327) skip = 332;
		tv39 = tv_plain(p);
		// 148
		if (skip && 328 >= skip) skip = 0;
		if (!skip) {
			p = sat(R01 + M19);
			fn = p < 0.0f; fz = p == 0.0f;
			tv40 = tv_plain(p);
		} else
			tv40 = tv_plain(p);
		// 149
		if (skip && 329 >= skip) skip = 0;
		if (er38) R01 = pr38;
		if (!skip) {
			tv41 = tv_plain(p);
		} else
			tv41 = tv_plain(p);
		// 14a
		if (skip && 330 >= skip) skip = 0;
		if (!skip && !fn && 332 > 330) skip = 332;
		tv42 = tv_plain(p);
		// 14b
		if (skip && 331 >= skip) skip = 0;
		if (!skip) {
			p = k[43] * M19 + 0.0f;
			pr43 = w24(p); er43 = true;
			tv43 = tv_plain(p);
		} else
			tv43 = tv_plain(p);
		// 14c
		if (skip && 332 >= skip) skip = 0;
		if (!skip) {
			p = sat(R02 - M19);
			fn = p < 0.0f; fz = p == 0.0f;
			tv44 = tv_plain(p);
		} else
			tv44 = tv_plain(p);
		// 14d
		if (skip && 333 >= skip) skip = 0;
		if (!skip) {
			tv45 = tv_plain(p);
		} else
			tv45 = tv_plain(p);
		// 14e
		if (skip && 334 >= skip) skip = 0;
		if (er43) R01 = pr43;
		if (!skip && fn && 337 > 334) skip = 337;
		tv46 = tv_plain(p);
		// 14f
		if (skip && 335 >= skip) skip = 0;
		if (!skip) {
			p = M19 + 0.0f;
			pr47 = w24(p); er47 = true;
			tv47 = tv_plain(p);
		} else
			tv47 = tv_plain(p);
		// 150
		if (skip && 336 >= skip) skip = 0;
		if (!skip && true && 341 > 336) skip = 341;
		tv48 = tv_plain(p);
		// 151
		if (skip && 337 >= skip) skip = 0;
		if (!skip) {
			p = sat(R02 + M19);
			fn = p < 0.0f; fz = p == 0.0f;
			tv49 = tv_plain(p);
		} else
			tv49 = tv_plain(p);
		// 152
		if (skip && 338 >= skip) skip = 0;
		if (er47) R02 = pr47;
		if (!skip) {
			tv50 = tv_plain(p);
		} else
			tv50 = tv_plain(p);
		// 153
		if (skip && 339 >= skip) skip = 0;
		if (!skip && !fn && 341 > 339) skip = 341;
		tv51 = tv_plain(p);
		// 154
		if (skip && 340 >= skip) skip = 0;
		if (!skip) {
			p = k[52] * M19 + 0.0f;
			pr52 = w24(p); er52 = true;
			tv52 = tv_plain(p);
		} else
			tv52 = tv_plain(p);
		// 155
		if (skip && 341 >= skip) skip = 0;
		if (!skip) {
			p = R01 + 0.0f;
			tv53 = tv_plain(p);
		} else
			tv53 = tv_plain(p);
		// 156
		if (skip && 342 >= skip) skip = 0;
		if (!skip) {
			p = sat(R5e + p);
			pr54 = w24(p); er54 = true;
			tv54 = tv_plain(p);
		} else
			tv54 = tv_plain(p);
		// 157
		if (skip && 343 >= skip) skip = 0;
		if (er52) R02 = pr52;
		if (!skip) {
			p = R02 + 0.0f;
			tv55 = tv_plain(p);
		} else
			tv55 = tv_plain(p);
		// 158
		if (skip && 344 >= skip) skip = 0;
		if (!skip) {
			p = sat(R60 + p);
			pr56 = w24(p); er56 = true;
			tv56 = tv_plain(p);
		} else
			tv56 = tv_plain(p);
		// 159
		if (skip && 345 >= skip) skip = 0;
		if (er54) R5e = pr54;
		if (!skip) {
			p = sat((k[57] * R5e + 0.0f) * 16.0f);
			pr57 = w24(p); er57 = true;
			tv57 = tv_plain(p);
			T0 = k[57];
		} else
			tv57 = tv_plain(p);
		// 15a
		if (skip && 346 >= skip) skip = 0;
		if (!skip) {
			p = R01 + 0.0f;
			tv58 = tv_plain(p);
		} else
			tv58 = tv_plain(p);
		// 15b
		if (skip && 347 >= skip) skip = 0;
		if (er56) R60 = pr56;
		if (!skip) {
			p = sat(R5d + p);
			pr59 = w24(p); er59 = true;
			tv59 = tv_plain(p);
		} else
			tv59 = tv_plain(p);
		// 15c
		if (skip && 348 >= skip) skip = 0;
		if (er57) R01 = pr57;
		if (!skip) {
			p = sat((T0 * R60 + 0.0f) * 16.0f);
			pr60 = w24(p); er60 = true;
			tv60 = tv_plain(p);
		} else
			tv60 = tv_plain(p);
		// 15d
		if (skip && 349 >= skip) skip = 0;
		if (!skip) {
			p = R02 + 0.0f;
			tv61 = tv_plain(p);
		} else
			tv61 = tv_plain(p);
		// 15e
		if (skip && 350 >= skip) skip = 0;
		if (er59) R5d = pr59;
		if (!skip) {
			p = sat(R5f + p);
			pr62 = w24(p); er62 = true;
			tv62 = tv_plain(p);
		} else
			tv62 = tv_plain(p);
		// 15f
		if (skip && 351 >= skip) skip = 0;
		if (er60) R02 = pr60;
		if (!skip) {
			p = sat((k[63] * R01 + 0.0f) * 16.0f);
			pr63 = w24(p); er63 = true;
			tv63 = tv_plain(p);
		} else
			tv63 = tv_plain(p);
		// 160
		if (skip && 352 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[64] * R02 + 0.0f) * 16.0f);
			pr64 = w24(p); er64 = true;
			tv64 = tv_plain(p);
		} else
			tv64 = tv_plain(p);
		// 161
		if (skip && 353 >= skip) skip = 0;
		if (er62) R5f = pr62;
		if (!skip) {
			tv65 = tv_plain(p);
		} else
			tv65 = tv_plain(p);
		// 162
		if (skip && 354 >= skip) skip = 0;
		if (er63) R01 = pr63;
		if (!skip) {
			p = sat((k[66] * R01 + 0.0f) * 16.0f);
			pr66 = w24(p); er66 = true;
			tv66 = tv_plain(p);
			T0 = k[66];
		} else
			tv66 = tv_plain(p);
		// 163
		if (skip && 355 >= skip) skip = 0;
		if (er64) R02 = pr64;
		if (!skip) {
			p = k[67] * R4b + 0.0f;
			tv67 = tv_plain(p);
		} else
			tv67 = tv_plain(p);
		// 164
		if (skip && 356 >= skip) skip = 0;
		if (!skip) {
			p = k[68] * R4a + p;
			tv68 = tv_plain(p);
		} else
			tv68 = tv_plain(p);
		// 165
		if (skip && 357 >= skip) skip = 0;
		if (er66) R4a = pr66;
		if (!skip) {
			p = sat((k[69] * R4a + p) * 2.0f);
			pr69 = w24(p); er69 = true;
			tv69 = tv_plain(p);
		} else
			tv69 = tv_plain(p);
		// 166
		if (skip && 358 >= skip) skip = 0;
		if (!skip) {
			p = k[70] * R4c + 0.0f;
			tv70 = tv_plain(p);
		} else
			tv70 = tv_plain(p);
		// 167
		if (skip && 359 >= skip) skip = 0;
		if (!skip) {
			p = k[71] * R4b + p;
			pr71 = R4b; er71 = true;
			tv71 = tv_plain(p);
		} else
			tv71 = tv_plain(p);
		// 168
		if (skip && 360 >= skip) skip = 0;
		if (er69) R4b = pr69;
		if (!skip) {
			p = k[72] * R4b + p;
			tv72 = tv_plain(p);
		} else
			tv72 = tv_plain(p);
		// 169
		if (skip && 361 >= skip) skip = 0;
		if (!skip) {
			p = k[73] * R4d + p;
			tv73 = tv_plain(p);
		} else
			tv73 = tv_plain(p);
		// 16a
		if (skip && 362 >= skip) skip = 0;
		if (er71) R4c = pr71;
		if (!skip) {
			p = sat((k[74] * R4e + p) * 4.0f);
			pr74 = w24(p); er74 = true;
			tv74 = tv_plain(p);
		} else
			tv74 = tv_plain(p);
		// 16b
		if (skip && 363 >= skip) skip = 0;
		if (!skip) {
			p = k[75] * R4f + 0.0f;
			tv75 = tv_plain(p);
		} else
			tv75 = tv_plain(p);
		// 16c
		if (skip && 364 >= skip) skip = 0;
		if (!skip) {
			p = k[76] * R4d + p;
			pr76 = R4d; er76 = true;
			tv76 = tv_plain(p);
		} else
			tv76 = tv_plain(p);
		// 16d
		if (skip && 365 >= skip) skip = 0;
		if (er74) R4d = pr74;
		if (!skip) {
			p = sat((k[77] * R4d + p) * 4.0f);
			pr77 = w24(p); er77 = true;
			tv77 = tv_plain(p);
		} else
			tv77 = tv_plain(p);
		// 16e
		if (skip && 366 >= skip) skip = 0;
		if (!skip) {
			p = k[78] * R50 + 0.0f;
			tv78 = tv_plain(p);
		} else
			tv78 = tv_plain(p);
		// 16f
		if (skip && 367 >= skip) skip = 0;
		if (er76) R4e = pr76;
		if (!skip) {
			p = k[79] * R4f + p;
			tv79 = tv_plain(p);
		} else
			tv79 = tv_plain(p);
		// 170
		if (skip && 368 >= skip) skip = 0;
		if (er77) R4f = pr77;
		if (!skip) {
			p = sat((k[80] * R4f + p) * 4.0f);
			pr80 = w24(p); er80 = true;
			tv80 = tv_plain(p);
		} else
			tv80 = tv_plain(p);
		// 171
		if (skip && 369 >= skip) skip = 0;
		if (!skip) {
			p = sat((T0 * R02 + 0.0f) * 16.0f);
			pr81 = w24(p); er81 = true;
			tv81 = tv_plain(p);
		} else
			tv81 = tv_plain(p);
		// 172
		if (skip && 370 >= skip) skip = 0;
		if (!skip) {
			p = k[82] * R57 + 0.0f;
			tv82 = tv_plain(p);
		} else
			tv82 = tv_plain(p);
		// 173
		if (skip && 371 >= skip) skip = 0;
		if (er80) R50 = pr80;
		if (!skip) {
			p = k[83] * R56 + p;
			tv83 = tv_plain(p);
		} else
			tv83 = tv_plain(p);
		// 174
		if (skip && 372 >= skip) skip = 0;
		if (er81) R56 = pr81;
		if (!skip) {
			p = sat((k[84] * R56 + p) * 2.0f);
			pr84 = w24(p); er84 = true;
			tv84 = tv_plain(p);
		} else
			tv84 = tv_plain(p);
		// 175
		if (skip && 373 >= skip) skip = 0;
		if (!skip) {
			p = k[85] * R58 + 0.0f;
			tv85 = tv_plain(p);
		} else
			tv85 = tv_plain(p);
		// 176
		if (skip && 374 >= skip) skip = 0;
		if (!skip) {
			p = k[86] * R57 + p;
			pr86 = R57; er86 = true;
			tv86 = tv_plain(p);
		} else
			tv86 = tv_plain(p);
		// 177
		if (skip && 375 >= skip) skip = 0;
		if (er84) R57 = pr84;
		if (!skip) {
			p = k[87] * R57 + p;
			tv87 = tv_plain(p);
		} else
			tv87 = tv_plain(p);
		// 178
		if (skip && 376 >= skip) skip = 0;
		if (!skip) {
			p = k[88] * R59 + p;
			tv88 = tv_plain(p);
		} else
			tv88 = tv_plain(p);
		// 179
		if (skip && 377 >= skip) skip = 0;
		if (er86) R58 = pr86;
		if (!skip) {
			p = sat((k[89] * R5a + p) * 4.0f);
			pr89 = w24(p); er89 = true;
			tv89 = tv_plain(p);
		} else
			tv89 = tv_plain(p);
		// 17a
		if (skip && 378 >= skip) skip = 0;
		if (!skip) {
			p = k[90] * R5b + 0.0f;
			tv90 = tv_plain(p);
		} else
			tv90 = tv_plain(p);
		// 17b
		if (skip && 379 >= skip) skip = 0;
		if (!skip) {
			p = k[91] * R59 + p;
			pr91 = R59; er91 = true;
			tv91 = tv_plain(p);
		} else
			tv91 = tv_plain(p);
		// 17c
		if (skip && 380 >= skip) skip = 0;
		if (er89) R59 = pr89;
		if (!skip) {
			p = sat((k[92] * R59 + p) * 4.0f);
			pr92 = w24(p); er92 = true;
			tv92 = tv_plain(p);
		} else
			tv92 = tv_plain(p);
		// 17d
		if (skip && 381 >= skip) skip = 0;
		if (!skip) {
			p = k[93] * R5c + 0.0f;
			tv93 = tv_plain(p);
		} else
			tv93 = tv_plain(p);
		// 17e
		if (skip && 382 >= skip) skip = 0;
		if (er91) R5a = pr91;
		if (!skip) {
			p = k[94] * R5b + p;
			tv94 = tv_plain(p);
		} else
			tv94 = tv_plain(p);
		// 17f
		if (skip && 383 >= skip) skip = 0;
		if (er92) R5b = pr92;
		if (!skip) {
			p = sat((k[95] * R5b + p) * 4.0f);
			pr95 = w24(p); er95 = true;
			tv95 = tv_plain(p);
		} else
			tv95 = tv_plain(p);
		if (er95) R5c = pr95;
		out[0] = M2c;
		out[1] = M2d;
	}

private:
	int32_t IX = 0;
	int32_t IX2 = 0;
	float M16 = 0.0f;
	float M17 = 0.0f;
	float M19 = 0.0f;
	float M2c = 0.0f;
	float M2d = 0.0f;
	float R01 = 0.0f;
	float R02 = 0.0f;
	float R03 = 0.0f;
	float R04 = 0.0f;
	float R44 = 0.0f;
	float R45 = 0.0f;
	float R46 = 0.0f;
	float R47 = 0.0f;
	float R48 = 0.0f;
	float R49 = 0.0f;
	float R4a = 0.0f;
	float R4b = 0.0f;
	float R4c = 0.0f;
	float R4d = 0.0f;
	float R4e = 0.0f;
	float R4f = 0.0f;
	float R50 = 0.0f;
	float R51 = 0.0f;
	float R52 = 0.0f;
	float R53 = 0.0f;
	float R54 = 0.0f;
	float R55 = 0.0f;
	float R56 = 0.0f;
	float R57 = 0.0f;
	float R58 = 0.0f;
	float R59 = 0.0f;
	float R5a = 0.0f;
	float R5b = 0.0f;
	float R5c = 0.0f;
	float R5d = 0.0f;
	float R5e = 0.0f;
	float R5f = 0.0f;
	float R60 = 0.0f;
	float RRD = 0.0f;
	float RWR = 0.0f;
	float T0 = 0.0f;
	bool fn = false;
	bool fz = false;
	float p = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_VAR_LOFI_H
