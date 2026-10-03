// license:BSD-3-Clause
// S-MU2000: インサーション 1: LO-FI
//
// **tools/meg_fx/emit_dyn.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。このプログラムは分岐（条件つきで
// 先の命令を飛ばす）を持つので、レジスタを変数として持ち、3 命令遅れの書き込みを「書くかどうか」の印つきで回す。
// 係数と番地は firmware が MEG に書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29

#ifndef S_MU2000_DSP_MEG_FX_INS_LOFI_H
#define S_MU2000_DSP_MEG_FX_INS_LOFI_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_lofi : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); IX = 0; IX2 = 0; M12 = 0.0f; M13 = 0.0f; M15 = 0.0f; M28 = 0.0f; M29 = 0.0f; R01 = 0.0f; R02 = 0.0f; R03 = 0.0f; R04 = 0.0f; R20 = 0.0f; R21 = 0.0f; R22 = 0.0f; R23 = 0.0f; R24 = 0.0f; R25 = 0.0f; R26 = 0.0f; R27 = 0.0f; R28 = 0.0f; R29 = 0.0f; R2a = 0.0f; R2b = 0.0f; R2c = 0.0f; R2d = 0.0f; R2e = 0.0f; R2f = 0.0f; R30 = 0.0f; R31 = 0.0f; R32 = 0.0f; R33 = 0.0f; R34 = 0.0f; R35 = 0.0f; R36 = 0.0f; R37 = 0.0f; R38 = 0.0f; R39 = 0.0f; R3a = 0.0f; R3b = 0.0f; R3c = 0.0f; RRD = 0.0f; RWR = 0.0f; T0 = 0.0f; fn = false; fz = false; p = 0.0f; }

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
		M28 = in[0];
		M29 = in[1];
		// 0c0
		if (skip && 192 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[0] + R25);
			pr0 = w24(p); er0 = true;
			tv0 = tv_plain(p);
		} else
			tv0 = tv_plain(p);
		// 0c1
		if (skip && 193 >= skip) skip = 0;
		if (!skip) {
			p = k[1] + 0.0f;
			pm1 = w24(p); em1 = true;
			tv1 = tv_plain(p);
		} else
			tv1 = tv_plain(p);
		// 0c2
		if (skip && 194 >= skip) skip = 0;
		if (!skip) {
			p = k[2] + 0.0f;
			tv2 = tv_plain(p);
		} else
			tv2 = tv_plain(p);
		// 0c3
		if (skip && 195 >= skip) skip = 0;
		if (er0) R25 = pr0;
		if (!skip) {
			p = R25 - p;
			fn = p < 0.0f; fz = p == 0.0f;
			tv3 = tv_plain(p);
		} else
			tv3 = tv_plain(p);
		// 0c4
		if (skip && 196 >= skip) skip = 0;
		if (em1) M15 = pm1;
		if (!skip) {
			p = sat((k[4] * R2c + 0.0f) * 4.0f);
			pr4 = w24(p); er4 = true;
			tv4 = tv_plain(p);
			T0 = k[4];
		} else
			tv4 = tv_plain(p);
		// 0c5
		if (skip && 197 >= skip) skip = 0;
		if (!skip) {
			p = sat((T0 * R38 + 0.0f) * 4.0f);
			pr5 = w24(p); er5 = true;
			tv5 = tv_plain(p);
		} else
			tv5 = tv_plain(p);
		// 0c6
		if (skip && 198 >= skip) skip = 0;
		if (!skip) {
			p = k[6] * M28 + 0.0f;
			tv6 = tv_plain(p);
		} else
			tv6 = tv_plain(p);
		// 0c7
		if (skip && 199 >= skip) skip = 0;
		if (er4) R03 = pr4;
		if (!skip) {
			p = sat(k[7] * M29 + p);
			pr7 = w24(p); er7 = true;
			tv7 = tv_plain(p);
		} else
			tv7 = tv_plain(p);
		// 0c8
		if (skip && 200 >= skip) skip = 0;
		if (er5) R04 = pr5;
		if (!skip) {
			p = k[8] * R21 + 0.0f;
			tv8 = tv_plain(p);
		} else
			tv8 = tv_plain(p);
		// 0c9
		if (skip && 201 >= skip) skip = 0;
		if (!skip) {
			p = k[9] * R20 + p;
			tv9 = tv_plain(p);
		} else
			tv9 = tv_plain(p);
		// 0ca
		if (skip && 202 >= skip) skip = 0;
		if (er7) R20 = pr7;
		if (!skip) {
			p = sat((k[10] * R20 + p) * 4.0f);
			pr10 = w24(p); er10 = true;
			tv10 = tv_plain(p);
		} else
			tv10 = tv_plain(p);
		// 0cb
		if (skip && 203 >= skip) skip = 0;
		if (!skip) {
			p = k[11] * R22 + 0.0f;
			tv11 = tv_plain(p);
		} else
			tv11 = tv_plain(p);
		// 0cc
		if (skip && 204 >= skip) skip = 0;
		if (!skip) {
			p = k[12] * R21 + p;
			pr12 = R21; er12 = true;
			tv12 = tv_plain(p);
		} else
			tv12 = tv_plain(p);
		// 0cd
		if (skip && 205 >= skip) skip = 0;
		if (er10) R21 = pr10;
		if (!skip) {
			p = k[13] * R21 + p;
			tv13 = tv_plain(p);
		} else
			tv13 = tv_plain(p);
		// 0ce
		if (skip && 206 >= skip) skip = 0;
		if (!skip) {
			p = k[14] * R23 + p;
			pr14 = R23; er14 = true;
			tv14 = tv_plain(p);
		} else
			tv14 = tv_plain(p);
		// 0cf
		if (skip && 207 >= skip) skip = 0;
		if (er12) R22 = pr12;
		if (!skip) {
			p = sat((k[15] * R24 + p) * 2.0f);
			pr15 = w24(p); er15 = true;
			tv15 = tv_plain(p);
		} else
			tv15 = tv_plain(p);
		// 0d0
		if (skip && 208 >= skip) skip = 0;
		if (!skip) {
			p = k[16] * M29 + 0.0f;
			pr16 = w24(p); er16 = true;
			tv16 = tv_plain(p);
		} else
			tv16 = tv_plain(p);
		// 0d1
		if (skip && 209 >= skip) skip = 0;
		if (er14) R24 = pr14;
		if (!skip) {
			p = k[17] * R2e + 0.0f;
			tv17 = tv_plain(p);
		} else
			tv17 = tv_plain(p);
		// 0d2
		if (skip && 210 >= skip) skip = 0;
		if (er15) R23 = pr15;
		if (!skip) {
			p = k[18] * R2d + p;
			tv18 = tv_plain(p);
		} else
			tv18 = tv_plain(p);
		// 0d3
		if (skip && 211 >= skip) skip = 0;
		if (er16) R2d = pr16;
		if (!skip) {
			p = sat((k[19] * R2d + p) * 4.0f);
			pr19 = w24(p); er19 = true;
			tv19 = tv_plain(p);
		} else
			tv19 = tv_plain(p);
		// 0d4
		if (skip && 212 >= skip) skip = 0;
		if (!skip) {
			p = k[20] * R2f + 0.0f;
			tv20 = tv_plain(p);
		} else
			tv20 = tv_plain(p);
		// 0d5
		if (skip && 213 >= skip) skip = 0;
		if (!skip) {
			p = k[21] * R2e + p;
			pr21 = R2e; er21 = true;
			tv21 = tv_plain(p);
		} else
			tv21 = tv_plain(p);
		// 0d6
		if (skip && 214 >= skip) skip = 0;
		if (er19) R2e = pr19;
		if (!skip) {
			p = k[22] * R2e + p;
			tv22 = tv_plain(p);
		} else
			tv22 = tv_plain(p);
		// 0d7
		if (skip && 215 >= skip) skip = 0;
		if (!skip) {
			p = k[23] * R30 + p;
			pr23 = R30; er23 = true;
			tv23 = tv_plain(p);
		} else
			tv23 = tv_plain(p);
		// 0d8
		if (skip && 216 >= skip) skip = 0;
		if (er21) R2f = pr21;
		if (!skip) {
			p = sat((k[24] * R31 + p) * 2.0f);
			pr24 = w24(p); er24 = true;
			tv24 = tv_plain(p);
		} else
			tv24 = tv_plain(p);
		// 0d9
		if (skip && 217 >= skip) skip = 0;
		if (!skip && fn && 221 > 217) skip = 221;
		tv25 = tv_plain(p);
		// 0da
		if (skip && 218 >= skip) skip = 0;
		if (er23) R31 = pr23;
		if (!skip) {
			p = R23 + 0.0f;
			pm26 = w24(p); em26 = true;
			tv26 = tv_plain(p);
		} else
			tv26 = tv_plain(p);
		// 0db
		if (skip && 219 >= skip) skip = 0;
		if (er24) R30 = pr24;
		if (!skip) {
			p = R30 + 0.0f;
			pm27 = w24(p); em27 = true;
			tv27 = tv_plain(p);
		} else
			tv27 = tv_plain(p);
		// 0dc
		if (skip && 220 >= skip) skip = 0;
		if (!skip) {
			p = k[28] + 0.0f;
			pr28 = w24(p); er28 = true;
			tv28 = tv_plain(p);
		} else
			tv28 = tv_plain(p);
		// 0dd
		if (skip && 221 >= skip) skip = 0;
		if (em26) M12 = pm26;
		if (!skip) {
			p = sat(k[29] * R39 + M12);
			pr29 = w24(p); er29 = true;
			tv29 = tv_plain(p);
		} else
			tv29 = tv_plain(p);
		// 0de
		if (skip && 222 >= skip) skip = 0;
		if (em27) M13 = pm27;
		if (!skip) {
			p = sat(k[30] * R3b + M13);
			pr30 = w24(p); er30 = true;
			tv30 = tv_plain(p);
		} else
			tv30 = tv_plain(p);
		// 0df
		if (skip && 223 >= skip) skip = 0;
		if (er28) R25 = pr28;
		if (!skip) {
			p = k[31] * M28 + 0.0f;
			tv31 = tv_plain(p);
		} else
			tv31 = tv_plain(p);
		// 0e0
		if (skip && 224 >= skip) skip = 0;
		if (er29) R01 = pr29;
		if (!skip) {
			p = sat((k[32] * R03 + p) * 4.0f);
			pm32 = w24(p); em32 = true;
			tv32 = tv_plain(p);
		} else
			tv32 = tv_plain(p);
		// 0e1
		if (skip && 225 >= skip) skip = 0;
		if (er30) R02 = pr30;
		if (!skip) {
			p = sat(R01 - M15);
			fn = p < 0.0f; fz = p == 0.0f;
			tv33 = tv_plain(p);
		} else
			tv33 = tv_plain(p);
		// 0e2
		if (skip && 226 >= skip) skip = 0;
		if (!skip) {
			p = k[34] * M29 + 0.0f;
			tv34 = tv_plain(p);
		} else
			tv34 = tv_plain(p);
		// 0e3
		if (skip && 227 >= skip) skip = 0;
		if (em32) M28 = pm32;
		if (!skip) {
			p = k[35] * R04 + p;
			tv35 = tv_plain(p);
		} else
			tv35 = tv_plain(p);
		// 0e4
		if (skip && 228 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[36] * R03 + p) * 4.0f);
			pm36 = w24(p); em36 = true;
			tv36 = tv_plain(p);
		} else
			tv36 = tv_plain(p);
		// 0e5
		if (skip && 229 >= skip) skip = 0;
		if (!skip && fn && 232 > 229) skip = 232;
		tv37 = tv_plain(p);
		// 0e6
		if (skip && 230 >= skip) skip = 0;
		if (!skip) {
			p = M15 + 0.0f;
			pr38 = w24(p); er38 = true;
			tv38 = tv_plain(p);
		} else
			tv38 = tv_plain(p);
		// 0e7
		if (skip && 231 >= skip) skip = 0;
		if (em36) M29 = pm36;
		if (!skip && true && 236 > 231) skip = 236;
		tv39 = tv_plain(p);
		// 0e8
		if (skip && 232 >= skip) skip = 0;
		if (!skip) {
			p = sat(R01 + M15);
			fn = p < 0.0f; fz = p == 0.0f;
			tv40 = tv_plain(p);
		} else
			tv40 = tv_plain(p);
		// 0e9
		if (skip && 233 >= skip) skip = 0;
		if (er38) R01 = pr38;
		if (!skip) {
			tv41 = tv_plain(p);
		} else
			tv41 = tv_plain(p);
		// 0ea
		if (skip && 234 >= skip) skip = 0;
		if (!skip && !fn && 236 > 234) skip = 236;
		tv42 = tv_plain(p);
		// 0eb
		if (skip && 235 >= skip) skip = 0;
		if (!skip) {
			p = k[43] * M15 + 0.0f;
			pr43 = w24(p); er43 = true;
			tv43 = tv_plain(p);
		} else
			tv43 = tv_plain(p);
		// 0ec
		if (skip && 236 >= skip) skip = 0;
		if (!skip) {
			p = sat(R02 - M15);
			fn = p < 0.0f; fz = p == 0.0f;
			tv44 = tv_plain(p);
		} else
			tv44 = tv_plain(p);
		// 0ed
		if (skip && 237 >= skip) skip = 0;
		if (!skip) {
			tv45 = tv_plain(p);
		} else
			tv45 = tv_plain(p);
		// 0ee
		if (skip && 238 >= skip) skip = 0;
		if (er43) R01 = pr43;
		if (!skip && fn && 241 > 238) skip = 241;
		tv46 = tv_plain(p);
		// 0ef
		if (skip && 239 >= skip) skip = 0;
		if (!skip) {
			p = M15 + 0.0f;
			pr47 = w24(p); er47 = true;
			tv47 = tv_plain(p);
		} else
			tv47 = tv_plain(p);
		// 0f0
		if (skip && 240 >= skip) skip = 0;
		if (!skip && true && 245 > 240) skip = 245;
		tv48 = tv_plain(p);
		// 0f1
		if (skip && 241 >= skip) skip = 0;
		if (!skip) {
			p = sat(R02 + M15);
			fn = p < 0.0f; fz = p == 0.0f;
			tv49 = tv_plain(p);
		} else
			tv49 = tv_plain(p);
		// 0f2
		if (skip && 242 >= skip) skip = 0;
		if (er47) R02 = pr47;
		if (!skip) {
			tv50 = tv_plain(p);
		} else
			tv50 = tv_plain(p);
		// 0f3
		if (skip && 243 >= skip) skip = 0;
		if (!skip && !fn && 245 > 243) skip = 245;
		tv51 = tv_plain(p);
		// 0f4
		if (skip && 244 >= skip) skip = 0;
		if (!skip) {
			p = k[52] * M15 + 0.0f;
			pr52 = w24(p); er52 = true;
			tv52 = tv_plain(p);
		} else
			tv52 = tv_plain(p);
		// 0f5
		if (skip && 245 >= skip) skip = 0;
		if (!skip) {
			p = R01 + 0.0f;
			tv53 = tv_plain(p);
		} else
			tv53 = tv_plain(p);
		// 0f6
		if (skip && 246 >= skip) skip = 0;
		if (!skip) {
			p = sat(R3a + p);
			pr54 = w24(p); er54 = true;
			tv54 = tv_plain(p);
		} else
			tv54 = tv_plain(p);
		// 0f7
		if (skip && 247 >= skip) skip = 0;
		if (er52) R02 = pr52;
		if (!skip) {
			p = R02 + 0.0f;
			tv55 = tv_plain(p);
		} else
			tv55 = tv_plain(p);
		// 0f8
		if (skip && 248 >= skip) skip = 0;
		if (!skip) {
			p = sat(R3c + p);
			pr56 = w24(p); er56 = true;
			tv56 = tv_plain(p);
		} else
			tv56 = tv_plain(p);
		// 0f9
		if (skip && 249 >= skip) skip = 0;
		if (er54) R3a = pr54;
		if (!skip) {
			p = sat((k[57] * R3a + 0.0f) * 16.0f);
			pr57 = w24(p); er57 = true;
			tv57 = tv_plain(p);
			T0 = k[57];
		} else
			tv57 = tv_plain(p);
		// 0fa
		if (skip && 250 >= skip) skip = 0;
		if (!skip) {
			p = R01 + 0.0f;
			tv58 = tv_plain(p);
		} else
			tv58 = tv_plain(p);
		// 0fb
		if (skip && 251 >= skip) skip = 0;
		if (er56) R3c = pr56;
		if (!skip) {
			p = sat(R39 + p);
			pr59 = w24(p); er59 = true;
			tv59 = tv_plain(p);
		} else
			tv59 = tv_plain(p);
		// 0fc
		if (skip && 252 >= skip) skip = 0;
		if (er57) R01 = pr57;
		if (!skip) {
			p = sat((T0 * R3c + 0.0f) * 16.0f);
			pr60 = w24(p); er60 = true;
			tv60 = tv_plain(p);
		} else
			tv60 = tv_plain(p);
		// 0fd
		if (skip && 253 >= skip) skip = 0;
		if (!skip) {
			p = R02 + 0.0f;
			tv61 = tv_plain(p);
		} else
			tv61 = tv_plain(p);
		// 0fe
		if (skip && 254 >= skip) skip = 0;
		if (er59) R39 = pr59;
		if (!skip) {
			p = sat(R3b + p);
			pr62 = w24(p); er62 = true;
			tv62 = tv_plain(p);
		} else
			tv62 = tv_plain(p);
		// 0ff
		if (skip && 255 >= skip) skip = 0;
		if (er60) R02 = pr60;
		if (!skip) {
			p = sat((k[63] * R01 + 0.0f) * 16.0f);
			pr63 = w24(p); er63 = true;
			tv63 = tv_plain(p);
		} else
			tv63 = tv_plain(p);
		// 100
		if (skip && 256 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[64] * R02 + 0.0f) * 16.0f);
			pr64 = w24(p); er64 = true;
			tv64 = tv_plain(p);
		} else
			tv64 = tv_plain(p);
		// 101
		if (skip && 257 >= skip) skip = 0;
		if (er62) R3b = pr62;
		if (!skip) {
			tv65 = tv_plain(p);
		} else
			tv65 = tv_plain(p);
		// 102
		if (skip && 258 >= skip) skip = 0;
		if (er63) R01 = pr63;
		if (!skip) {
			p = sat((k[66] * R01 + 0.0f) * 16.0f);
			pr66 = w24(p); er66 = true;
			tv66 = tv_plain(p);
			T0 = k[66];
		} else
			tv66 = tv_plain(p);
		// 103
		if (skip && 259 >= skip) skip = 0;
		if (er64) R02 = pr64;
		if (!skip) {
			p = k[67] * R27 + 0.0f;
			tv67 = tv_plain(p);
		} else
			tv67 = tv_plain(p);
		// 104
		if (skip && 260 >= skip) skip = 0;
		if (!skip) {
			p = k[68] * R26 + p;
			tv68 = tv_plain(p);
		} else
			tv68 = tv_plain(p);
		// 105
		if (skip && 261 >= skip) skip = 0;
		if (er66) R26 = pr66;
		if (!skip) {
			p = sat((k[69] * R26 + p) * 2.0f);
			pr69 = w24(p); er69 = true;
			tv69 = tv_plain(p);
		} else
			tv69 = tv_plain(p);
		// 106
		if (skip && 262 >= skip) skip = 0;
		if (!skip) {
			p = k[70] * R28 + 0.0f;
			tv70 = tv_plain(p);
		} else
			tv70 = tv_plain(p);
		// 107
		if (skip && 263 >= skip) skip = 0;
		if (!skip) {
			p = k[71] * R27 + p;
			pr71 = R27; er71 = true;
			tv71 = tv_plain(p);
		} else
			tv71 = tv_plain(p);
		// 108
		if (skip && 264 >= skip) skip = 0;
		if (er69) R27 = pr69;
		if (!skip) {
			p = k[72] * R27 + p;
			tv72 = tv_plain(p);
		} else
			tv72 = tv_plain(p);
		// 109
		if (skip && 265 >= skip) skip = 0;
		if (!skip) {
			p = k[73] * R29 + p;
			tv73 = tv_plain(p);
		} else
			tv73 = tv_plain(p);
		// 10a
		if (skip && 266 >= skip) skip = 0;
		if (er71) R28 = pr71;
		if (!skip) {
			p = sat((k[74] * R2a + p) * 4.0f);
			pr74 = w24(p); er74 = true;
			tv74 = tv_plain(p);
		} else
			tv74 = tv_plain(p);
		// 10b
		if (skip && 267 >= skip) skip = 0;
		if (!skip) {
			p = k[75] * R2b + 0.0f;
			tv75 = tv_plain(p);
		} else
			tv75 = tv_plain(p);
		// 10c
		if (skip && 268 >= skip) skip = 0;
		if (!skip) {
			p = k[76] * R29 + p;
			pr76 = R29; er76 = true;
			tv76 = tv_plain(p);
		} else
			tv76 = tv_plain(p);
		// 10d
		if (skip && 269 >= skip) skip = 0;
		if (er74) R29 = pr74;
		if (!skip) {
			p = sat((k[77] * R29 + p) * 4.0f);
			pr77 = w24(p); er77 = true;
			tv77 = tv_plain(p);
		} else
			tv77 = tv_plain(p);
		// 10e
		if (skip && 270 >= skip) skip = 0;
		if (!skip) {
			p = k[78] * R2c + 0.0f;
			tv78 = tv_plain(p);
		} else
			tv78 = tv_plain(p);
		// 10f
		if (skip && 271 >= skip) skip = 0;
		if (er76) R2a = pr76;
		if (!skip) {
			p = k[79] * R2b + p;
			tv79 = tv_plain(p);
		} else
			tv79 = tv_plain(p);
		// 110
		if (skip && 272 >= skip) skip = 0;
		if (er77) R2b = pr77;
		if (!skip) {
			p = sat((k[80] * R2b + p) * 4.0f);
			pr80 = w24(p); er80 = true;
			tv80 = tv_plain(p);
		} else
			tv80 = tv_plain(p);
		// 111
		if (skip && 273 >= skip) skip = 0;
		if (!skip) {
			p = sat((T0 * R02 + 0.0f) * 16.0f);
			pr81 = w24(p); er81 = true;
			tv81 = tv_plain(p);
		} else
			tv81 = tv_plain(p);
		// 112
		if (skip && 274 >= skip) skip = 0;
		if (!skip) {
			p = k[82] * R33 + 0.0f;
			tv82 = tv_plain(p);
		} else
			tv82 = tv_plain(p);
		// 113
		if (skip && 275 >= skip) skip = 0;
		if (er80) R2c = pr80;
		if (!skip) {
			p = k[83] * R32 + p;
			tv83 = tv_plain(p);
		} else
			tv83 = tv_plain(p);
		// 114
		if (skip && 276 >= skip) skip = 0;
		if (er81) R32 = pr81;
		if (!skip) {
			p = sat((k[84] * R32 + p) * 2.0f);
			pr84 = w24(p); er84 = true;
			tv84 = tv_plain(p);
		} else
			tv84 = tv_plain(p);
		// 115
		if (skip && 277 >= skip) skip = 0;
		if (!skip) {
			p = k[85] * R34 + 0.0f;
			tv85 = tv_plain(p);
		} else
			tv85 = tv_plain(p);
		// 116
		if (skip && 278 >= skip) skip = 0;
		if (!skip) {
			p = k[86] * R33 + p;
			pr86 = R33; er86 = true;
			tv86 = tv_plain(p);
		} else
			tv86 = tv_plain(p);
		// 117
		if (skip && 279 >= skip) skip = 0;
		if (er84) R33 = pr84;
		if (!skip) {
			p = k[87] * R33 + p;
			tv87 = tv_plain(p);
		} else
			tv87 = tv_plain(p);
		// 118
		if (skip && 280 >= skip) skip = 0;
		if (!skip) {
			p = k[88] * R35 + p;
			tv88 = tv_plain(p);
		} else
			tv88 = tv_plain(p);
		// 119
		if (skip && 281 >= skip) skip = 0;
		if (er86) R34 = pr86;
		if (!skip) {
			p = sat((k[89] * R36 + p) * 4.0f);
			pr89 = w24(p); er89 = true;
			tv89 = tv_plain(p);
		} else
			tv89 = tv_plain(p);
		// 11a
		if (skip && 282 >= skip) skip = 0;
		if (!skip) {
			p = k[90] * R37 + 0.0f;
			tv90 = tv_plain(p);
		} else
			tv90 = tv_plain(p);
		// 11b
		if (skip && 283 >= skip) skip = 0;
		if (!skip) {
			p = k[91] * R35 + p;
			pr91 = R35; er91 = true;
			tv91 = tv_plain(p);
		} else
			tv91 = tv_plain(p);
		// 11c
		if (skip && 284 >= skip) skip = 0;
		if (er89) R35 = pr89;
		if (!skip) {
			p = sat((k[92] * R35 + p) * 4.0f);
			pr92 = w24(p); er92 = true;
			tv92 = tv_plain(p);
		} else
			tv92 = tv_plain(p);
		// 11d
		if (skip && 285 >= skip) skip = 0;
		if (!skip) {
			p = k[93] * R38 + 0.0f;
			tv93 = tv_plain(p);
		} else
			tv93 = tv_plain(p);
		// 11e
		if (skip && 286 >= skip) skip = 0;
		if (er91) R36 = pr91;
		if (!skip) {
			p = k[94] * R37 + p;
			tv94 = tv_plain(p);
		} else
			tv94 = tv_plain(p);
		// 11f
		if (skip && 287 >= skip) skip = 0;
		if (er92) R37 = pr92;
		if (!skip) {
			p = sat((k[95] * R37 + p) * 4.0f);
			pr95 = w24(p); er95 = true;
			tv95 = tv_plain(p);
		} else
			tv95 = tv_plain(p);
		if (er95) R38 = pr95;
		out[0] = M28;
		out[1] = M29;
	}

private:
	int32_t IX = 0;
	int32_t IX2 = 0;
	float M12 = 0.0f;
	float M13 = 0.0f;
	float M15 = 0.0f;
	float M28 = 0.0f;
	float M29 = 0.0f;
	float R01 = 0.0f;
	float R02 = 0.0f;
	float R03 = 0.0f;
	float R04 = 0.0f;
	float R20 = 0.0f;
	float R21 = 0.0f;
	float R22 = 0.0f;
	float R23 = 0.0f;
	float R24 = 0.0f;
	float R25 = 0.0f;
	float R26 = 0.0f;
	float R27 = 0.0f;
	float R28 = 0.0f;
	float R29 = 0.0f;
	float R2a = 0.0f;
	float R2b = 0.0f;
	float R2c = 0.0f;
	float R2d = 0.0f;
	float R2e = 0.0f;
	float R2f = 0.0f;
	float R30 = 0.0f;
	float R31 = 0.0f;
	float R32 = 0.0f;
	float R33 = 0.0f;
	float R34 = 0.0f;
	float R35 = 0.0f;
	float R36 = 0.0f;
	float R37 = 0.0f;
	float R38 = 0.0f;
	float R39 = 0.0f;
	float R3a = 0.0f;
	float R3b = 0.0f;
	float R3c = 0.0f;
	float RRD = 0.0f;
	float RWR = 0.0f;
	float T0 = 0.0f;
	bool fn = false;
	bool fz = false;
	float p = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_LOFI_H
