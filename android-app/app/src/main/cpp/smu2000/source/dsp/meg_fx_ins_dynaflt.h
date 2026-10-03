// license:BSD-3-Clause
// S-MU2000: インサーション 1: DYNA FLT
//
// **tools/meg_fx/emit_dyn.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。このプログラムは分岐（条件つきで
// 先の命令を飛ばす）を持つので、レジスタを変数として持ち、3 命令遅れの書き込みを「書くかどうか」の印つきで回す。
// 係数と番地は firmware が MEG に書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29

#ifndef S_MU2000_DSP_MEG_FX_INS_DYNAFLT_H
#define S_MU2000_DSP_MEG_FX_INS_DYNAFLT_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_dynaflt : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); IX = 0; IX2 = 0; M01 = 0.0f; M02 = 0.0f; M03 = 0.0f; M04 = 0.0f; M05 = 0.0f; M06 = 0.0f; M07 = 0.0f; M08 = 0.0f; M09 = 0.0f; M12 = 0.0f; M13 = 0.0f; M28 = 0.0f; M29 = 0.0f; R01 = 0.0f; R03 = 0.0f; R05 = 0.0f; R06 = 0.0f; R20 = 0.0f; R21 = 0.0f; R22 = 0.0f; R23 = 0.0f; R24 = 0.0f; R25 = 0.0f; R28 = 0.0f; R29 = 0.0f; R2a = 0.0f; R2c = 0.0f; R2d = 0.0f; R2e = 0.0f; R2f = 0.0f; R30 = 0.0f; R31 = 0.0f; R32 = 0.0f; R33 = 0.0f; R34 = 0.0f; R35 = 0.0f; R36 = 0.0f; R37 = 0.0f; R38 = 0.0f; R39 = 0.0f; R3a = 0.0f; R3b = 0.0f; RRD = 0.0f; RWR = 0.0f; T0 = 0.0f; T1 = 0.0f; T2 = 0.0f; T3 = 0.0f; T5 = 0.0f; fn = false; fz = false; p = 0.0f; }

	// in: 入口。out: 出口。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		float tv0;
		float pr0;
		bool er0 = false;
		float tv1;
		float tv2;
		float tv3;
		float pr3;
		bool er3 = false;
		float tv4;
		float tv5;
		float tv6;
		float pr6;
		bool er6 = false;
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
		float tv13;
		float pr13;
		bool er13 = false;
		float tv14;
		float tv15;
		float pm15;
		bool em15 = false;
		float tv16;
		float tv17;
		float pm17;
		bool em17 = false;
		float tv18;
		float tv19;
		float tv20;
		float pr20;
		bool er20 = false;
		float tv21;
		float tv22;
		float pm22;
		bool em22 = false;
		float tv23;
		float tv24;
		float pm24;
		bool em24 = false;
		float tv25;
		float tv26;
		float tv27;
		float tv28;
		float pm28;
		bool em28 = false;
		float tv29;
		float pr29;
		bool er29 = false;
		float tv30;
		float tv31;
		float tv32;
		float tv33;
		float tv34;
		float tv35;
		float tv36;
		float pr36;
		bool er36 = false;
		float tv37;
		float tv38;
		float tv39;
		float tv40;
		float pr40;
		bool er40 = false;
		float tv41;
		float pr41;
		bool er41 = false;
		float tv42;
		float pm42;
		bool em42 = false;
		float tv43;
		float tv44;
		float pr44;
		bool er44 = false;
		float tv45;
		float pm45;
		bool em45 = false;
		float tv46;
		float pr46;
		bool er46 = false;
		float tv47;
		float tv48;
		float tv49;
		float pm49;
		bool em49 = false;
		float tv50;
		float tv51;
		float pr51;
		bool er51 = false;
		float tv52;
		float tv53;
		float pr53;
		bool er53 = false;
		float tv54;
		float pr54;
		bool er54 = false;
		float tv55;
		float pm55;
		bool em55 = false;
		float tv56;
		float tv57;
		float pr57;
		bool er57 = false;
		float tv58;
		float tv59;
		float pm59;
		bool em59 = false;
		float tv60;
		float pr60;
		bool er60 = false;
		float tv61;
		float tv62;
		float pm62;
		bool em62 = false;
		float tv63;
		float pr63;
		bool er63 = false;
		float tv64;
		float pr64;
		bool er64 = false;
		float tv65;
		float tv66;
		float pm66;
		bool em66 = false;
		float tv67;
		float tv68;
		float pm68;
		bool em68 = false;
		float tv69;
		float tv70;
		float pm70;
		bool em70 = false;
		float tv71;
		float pm71;
		bool em71 = false;
		float tv72;
		float tv73;
		float tv74;
		float tv75;
		float pm75;
		bool em75 = false;
		float tv76;
		float pm76;
		bool em76 = false;
		float tv77;
		float pr77;
		bool er77 = false;
		float tv78;
		float pm78;
		bool em78 = false;
		float pr78;
		bool er78 = false;
		float tv79;
		float pr79;
		bool er79 = false;
		float tv80;
		float pm80;
		bool em80 = false;
		float pr80;
		bool er80 = false;
		float tv81;
		float tv82;
		float pm82;
		bool em82 = false;
		float pr82;
		bool er82 = false;
		float tv83;
		float tv84;
		float pm84;
		bool em84 = false;
		float pr84;
		bool er84 = false;
		float tv85;
		float tv86;
		float pr86;
		bool er86 = false;
		float tv87;
		float pr87;
		bool er87 = false;
		float tv88;
		float tv89;
		float pr89;
		bool er89 = false;
		float tv90;
		float tv91;
		float pr91;
		bool er91 = false;
		float tv92;
		float tv93;
		float pr93;
		bool er93 = false;
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
			p = sat((k[0] * M28 + 0.0f) * 2.0f);
			pr0 = w24(p); er0 = true;
			tv0 = tv_plain(p);
		} else
			tv0 = tv_plain(p);
		// 0c1
		if (skip && 193 >= skip) skip = 0;
		if (!skip) {
			p = k[1] * R20 + 0.0f;
			tv1 = tv_plain(p);
		} else
			tv1 = tv_plain(p);
		// 0c2
		if (skip && 194 >= skip) skip = 0;
		if (!skip) {
			p = k[2] * R21 + p;
			tv2 = tv_plain(p);
		} else
			tv2 = tv_plain(p);
		// 0c3
		if (skip && 195 >= skip) skip = 0;
		if (er0) R20 = pr0;
		if (!skip) {
			p = sat((k[3] * R20 + p) * 2.0f);
			pr3 = w24(p); er3 = true;
			tv3 = tv_plain(p);
		} else
			tv3 = tv_plain(p);
		// 0c4
		if (skip && 196 >= skip) skip = 0;
		if (!skip) {
			p = k[4] * R21 + 0.0f;
			tv4 = tv_plain(p);
		} else
			tv4 = tv_plain(p);
		// 0c5
		if (skip && 197 >= skip) skip = 0;
		if (!skip) {
			p = k[5] * R22 + p;
			tv5 = tv_plain(p);
		} else
			tv5 = tv_plain(p);
		// 0c6
		if (skip && 198 >= skip) skip = 0;
		if (er3) R21 = pr3;
		if (!skip) {
			p = sat((k[6] * R21 + p) * 4.0f);
			pr6 = w24(p); er6 = true;
			tv6 = tv_plain(p);
		} else
			tv6 = tv_plain(p);
		// 0c7
		if (skip && 199 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[7] * M29 + 0.0f) * 2.0f);
			pr7 = w24(p); er7 = true;
			tv7 = tv_plain(p);
		} else
			tv7 = tv_plain(p);
		// 0c8
		if (skip && 200 >= skip) skip = 0;
		if (!skip) {
			p = k[8] * R23 + 0.0f;
			tv8 = tv_plain(p);
		} else
			tv8 = tv_plain(p);
		// 0c9
		if (skip && 201 >= skip) skip = 0;
		if (er6) R22 = pr6;
		if (!skip) {
			p = k[9] * R24 + p;
			tv9 = tv_plain(p);
		} else
			tv9 = tv_plain(p);
		// 0ca
		if (skip && 202 >= skip) skip = 0;
		if (er7) R23 = pr7;
		if (!skip) {
			p = sat((k[10] * R23 + p) * 2.0f);
			pr10 = w24(p); er10 = true;
			tv10 = tv_plain(p);
		} else
			tv10 = tv_plain(p);
		// 0cb
		if (skip && 203 >= skip) skip = 0;
		if (!skip) {
			p = k[11] * R24 + 0.0f;
			tv11 = tv_plain(p);
		} else
			tv11 = tv_plain(p);
		// 0cc
		if (skip && 204 >= skip) skip = 0;
		if (!skip) {
			p = k[12] * R25 + p;
			tv12 = tv_plain(p);
		} else
			tv12 = tv_plain(p);
		// 0cd
		if (skip && 205 >= skip) skip = 0;
		if (er10) R24 = pr10;
		if (!skip) {
			p = sat((k[13] * R24 + p) * 4.0f);
			pr13 = w24(p); er13 = true;
			tv13 = tv_plain(p);
		} else
			tv13 = tv_plain(p);
		// 0ce
		if (skip && 206 >= skip) skip = 0;
		if (!skip) {
			p = k[14] * R22 + 0.0f;
			tv14 = tv_plain(p);
			T0 = k[14];
		} else
			tv14 = tv_plain(p);
		// 0cf
		if (skip && 207 >= skip) skip = 0;
		if (!skip) {
			p = satabs((T0 * R22 + p) * 16.0f);
			pm15 = w24(p); em15 = true;
			tv15 = tv_plain(p);
		} else
			tv15 = tv_plain(p);
		// 0d0
		if (skip && 208 >= skip) skip = 0;
		if (er13) R25 = pr13;
		if (!skip) {
			p = T0 * R25 + 0.0f;
			tv16 = tv_plain(p);
		} else
			tv16 = tv_plain(p);
		// 0d1
		if (skip && 209 >= skip) skip = 0;
		if (!skip) {
			p = satabs((T0 * R25 + p) * 16.0f);
			pm17 = w24(p); em17 = true;
			tv17 = tv_plain(p);
		} else
			tv17 = tv_plain(p);
		// 0d2
		if (skip && 210 >= skip) skip = 0;
		if (em15) M01 = pm15;
		if (!skip) {
			p = M01 - p;
			fn = p < 0.0f; fz = p == 0.0f;
			tv18 = tv_plain(p);
		} else
			tv18 = tv_plain(p);
		// 0d3
		if (skip && 211 >= skip) skip = 0;
		if (!skip) {
			p = k[19] + 0.0f;
			tv19 = tv_plain(p);
		} else
			tv19 = tv_plain(p);
		// 0d4
		if (skip && 212 >= skip) skip = 0;
		if (em17) M02 = pm17;
		if (!skip) {
			p = sat(k[20] + (p * (1.0f / 32768.0f)));
			pr20 = w24(p); er20 = true;
			tv20 = tv_plain(p);
		} else
			tv20 = tv_plain(p);
		// 0d5
		if (skip && 213 >= skip) skip = 0;
		if (!skip && !fn && 215 > 213) skip = 215;
		tv21 = tv_plain(p);
		// 0d6
		if (skip && 214 >= skip) skip = 0;
		if (!skip) {
			p = M02 + 0.0f;
			pm22 = w24(p); em22 = true;
			tv22 = tv_plain(p);
		} else
			tv22 = tv_plain(p);
		// 0d7
		if (skip && 215 >= skip) skip = 0;
		if (er20) R01 = pr20;
		if (!skip) {
			p = sat(k[23] * R30 + M12);
			tv23 = tv_plain(p);
		} else
			tv23 = tv_plain(p);
		// 0d8
		if (skip && 216 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[24] * R32 + p);
			pm24 = w24(p); em24 = true;
			tv24 = tv_plain(p);
		} else
			tv24 = tv_plain(p);
		// 0d9
		if (skip && 217 >= skip) skip = 0;
		if (em22) M01 = pm22;
		if (!skip) {
			p = k[25] - M01;
			fn = p < 0.0f; fz = p == 0.0f;
			tv25 = tv_plain(p);
		} else
			tv25 = tv_plain(p);
		// 0da
		if (skip && 218 >= skip) skip = 0;
		if (!skip) {
			tv26 = tv_plain(p);
		} else
			tv26 = tv_plain(p);
		// 0db
		if (skip && 219 >= skip) skip = 0;
		if (em24) M12 = pm24;
		if (!skip && (fn || fz) && 221 > 219) skip = 221;
		tv27 = tv_plain(p);
		// 0dc
		if (skip && 220 >= skip) skip = 0;
		if (!skip) {
			p = k[28] + 0.0f;
			pm28 = w24(p); em28 = true;
			tv28 = tv_plain(p);
		} else
			tv28 = tv_plain(p);
		// 0dd
		if (skip && 221 >= skip) skip = 0;
		if (!skip) {
			p = k[29] + 0.0f;
			pr29 = w24(p); er29 = true;
			tv29 = tv_plain(p);
		} else
			tv29 = tv_plain(p);
		// 0de
		if (skip && 222 >= skip) skip = 0;
		if (!skip) {
			p = k[30] * R28 + 0.0f;
			tv30 = tv_plain(p);
			T0 = k[30];
		} else
			tv30 = tv_plain(p);
		// 0df
		if (skip && 223 >= skip) skip = 0;
		if (em28) M01 = pm28;
		if (!skip) {
			p = sat(T0 * M01 - p);
			fn = p < 0.0f; fz = p == 0.0f;
			tv31 = tv_plain(p);
		} else
			tv31 = tv_plain(p);
		// 0e0
		if (skip && 224 >= skip) skip = 0;
		if (er29) R2a = pr29;
		if (!skip) {
			tv32 = tv_plain(p);
			T2 = k[32];
		} else
			tv32 = tv_plain(p);
		// 0e1
		if (skip && 225 >= skip) skip = 0;
		if (!skip && (!fn || fz) && 233 > 225) skip = 233;
		T1 = tv31;
		tv33 = tv_plain(p);
		// 0e2
		if (skip && 226 >= skip) skip = 0;
		if (!skip) {
			p = k[34] * R29 + 0.0f;
			tv34 = tv_plain(p);
		} else
			tv34 = tv_plain(p);
		// 0e3
		if (skip && 227 >= skip) skip = 0;
		if (!skip) {
			p = k[35] * R29 + (p * (1.0f / 32768.0f));
			tv35 = tv_plain(p);
		} else
			tv35 = tv_plain(p);
		// 0e4
		if (skip && 228 >= skip) skip = 0;
		if (!skip) {
			p = sat(R29 + p);
			pr36 = w24(p); er36 = true;
			tv36 = tv_plain(p);
		} else
			tv36 = tv_plain(p);
		// 0e5
		if (skip && 229 >= skip) skip = 0;
		if (!skip) {
			tv37 = tv_plain(p);
		} else
			tv37 = tv_plain(p);
		// 0e6
		if (skip && 230 >= skip) skip = 0;
		if (!skip) {
			tv38 = tv_plain(p);
		} else
			tv38 = tv_plain(p);
		// 0e7
		if (skip && 231 >= skip) skip = 0;
		if (er36) R2a = pr36;
		if (!skip) {
			p = k[39] * R2a + 0.0f;
			tv39 = tv_plain(p);
		} else
			tv39 = tv_plain(p);
		// 0e8
		if (skip && 232 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[40] * R2a + (p * (1.0f / 32768.0f)));
			pr40 = w24(p); er40 = true;
			tv40 = tv_plain(p);
		} else
			tv40 = tv_plain(p);
		// 0e9
		if (skip && 233 >= skip) skip = 0;
		if (!skip) {
			p = T2 * R22 - R22;
			pr41 = R22; er41 = true;
			tv41 = tv_plain(p);
		} else
			tv41 = tv_plain(p);
		// 0ea
		if (skip && 234 >= skip) skip = 0;
		if (!skip) {
			p = T2 * R2e - p;
			pm42 = w24(p); em42 = true;
			tv42 = tv_plain(p);
		} else
			tv42 = tv_plain(p);
		// 0eb
		if (skip && 235 >= skip) skip = 0;
		if (er40) R01 = pr40;
		if (!skip) {
			p = T1 * R01 + 0.0f;
			tv43 = tv_plain(p);
		} else
			tv43 = tv_plain(p);
		// 0ec
		if (skip && 236 >= skip) skip = 0;
		if (er41) R2e = pr41;
		if (!skip) {
			p = sat(R28 + p);
			pr44 = w24(p); er44 = true;
			tv44 = tv_plain(p);
		} else
			tv44 = tv_plain(p);
		// 0ed
		if (skip && 237 >= skip) skip = 0;
		if (em42) M08 = pm42;
		if (!skip) {
			p = k[45] + 0.0f;
			pm45 = w24(p); em45 = true;
			tv45 = tv_plain(p);
			T5 = k[45];
		} else
			tv45 = tv_plain(p);
		// 0ee
		if (skip && 238 >= skip) skip = 0;
		if (!skip) {
			p = R2a + 0.0f;
			pr46 = w24(p); er46 = true;
			tv46 = tv_plain(p);
		} else
			tv46 = tv_plain(p);
		// 0ef
		if (skip && 239 >= skip) skip = 0;
		if (er44) R28 = pr44;
		if (!skip) {
			tv47 = tv_plain(p);
		} else
			tv47 = tv_plain(p);
		// 0f0
		if (skip && 240 >= skip) skip = 0;
		if (em45) M05 = pm45;
		if (!skip) {
			p = sat(k[48] * R37 + M13);
			tv48 = tv_plain(p);
		} else
			tv48 = tv_plain(p);
		// 0f1
		if (skip && 241 >= skip) skip = 0;
		if (er46) R29 = pr46;
		if (!skip) {
			p = sat(k[49] * R39 + p);
			pm49 = w24(p); em49 = true;
			tv49 = tv_plain(p);
		} else
			tv49 = tv_plain(p);
		// 0f2
		if (skip && 242 >= skip) skip = 0;
		if (!skip) {
			p = satpos((k[50] * R28 + 0.0f) * 4.0f);
			tv50 = tv_plain(p);
		} else
			tv50 = tv_plain(p);
		// 0f3
		if (skip && 243 >= skip) skip = 0;
		if (!skip) {
			p = satpos(k[51] + p);
			pr51 = w24(p); er51 = true;
			tv51 = tv_plain(p);
		} else
			tv51 = tv_plain(p);
		// 0f4
		if (skip && 244 >= skip) skip = 0;
		if (em49) M13 = pm49;
		if (!skip) {
			p = 0.0f - p;
			tv52 = tv_plain(p);
		} else
			tv52 = tv_plain(p);
		// 0f5
		if (skip && 245 >= skip) skip = 0;
		if (!skip) {
			p = satpos(k[53] + p);
			pr53 = w24(p); er53 = true;
			tv53 = tv_plain(p);
		} else
			tv53 = tv_plain(p);
		// 0f6
		if (skip && 246 >= skip) skip = 0;
		if (er51) R05 = pr51;
		if (!skip) {
			p = T2 * R25 - R25;
			pr54 = R25; er54 = true;
			tv54 = tv_plain(p);
		} else
			tv54 = tv_plain(p);
		// 0f7
		if (skip && 247 >= skip) skip = 0;
		if (!skip) {
			p = T2 * R35 - p;
			pm55 = w24(p); em55 = true;
			tv55 = tv_plain(p);
		} else
			tv55 = tv_plain(p);
		// 0f8
		if (skip && 248 >= skip) skip = 0;
		if (er53) R06 = pr53;
		if (!skip) {
			p = k[56] * R05 + 0.0f;
			tv56 = tv_plain(p);
		} else
			tv56 = tv_plain(p);
		// 0f9
		if (skip && 249 >= skip) skip = 0;
		if (er54) R35 = pr54;
		if (!skip) {
			p = sat(k[57] * R06 + p);
			pr57 = w24(p); er57 = true;
			tv57 = tv_plain(p);
		} else
			tv57 = tv_plain(p);
		// 0fa
		if (skip && 250 >= skip) skip = 0;
		if (em55) M09 = pm55;
		if (!skip) {
			p = sat(k[58] * R2f + M12);
			tv58 = tv_plain(p);
		} else
			tv58 = tv_plain(p);
		// 0fb
		if (skip && 251 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[59] * R31 + p);
			pm59 = w24(p); em59 = true;
			tv59 = tv_plain(p);
			T0 = tv57;
		} else
			tv59 = tv_plain(p);
		// 0fc
		if (skip && 252 >= skip) skip = 0;
		if (er57) R2c = pr57;
		if (!skip) {
			p = sat(k[60] * R2c - M05);
			pr60 = w24(p); er60 = true;
			tv60 = tv_plain(p);
		} else
			tv60 = tv_plain(p);
		// 0fd
		if (skip && 253 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[61] * R36 + M13);
			tv61 = tv_plain(p);
		} else
			tv61 = tv_plain(p);
		// 0fe
		if (skip && 254 >= skip) skip = 0;
		if (em59) M12 = pm59;
		if (!skip) {
			p = sat(k[62] * R38 + p);
			pm62 = w24(p); em62 = true;
			tv62 = tv_plain(p);
		} else
			tv62 = tv_plain(p);
		// 0ff
		if (skip && 255 >= skip) skip = 0;
		if (er60) R06 = pr60;
		if (!skip) {
			p = sat(k[63] * R06 + M05);
			pr63 = w24(p); er63 = true;
			tv63 = tv_plain(p);
		} else
			tv63 = tv_plain(p);
		// 100
		if (skip && 256 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[64] - p) * 2.0f);
			pr64 = w24(p); er64 = true;
			tv64 = tv_plain(p);
		} else
			tv64 = tv_plain(p);
		// 101
		if (skip && 257 >= skip) skip = 0;
		if (em62) M13 = pm62;
		if (!skip) {
			p = sat(k[65] * R33 + M12);
			tv65 = tv_plain(p);
		} else
			tv65 = tv_plain(p);
		// 102
		if (skip && 258 >= skip) skip = 0;
		if (er63) R2d = pr63;
		if (!skip) {
			p = sat((k[66] * R34 + p) * 4.0f);
			pm66 = w24(p); em66 = true;
			tv66 = tv_plain(p);
		} else
			tv66 = tv_plain(p);
		// 103
		if (skip && 259 >= skip) skip = 0;
		if (er64) R03 = pr64;
		if (!skip) {
			p = sat(k[67] * R03 + 0.0f);
			tv67 = tv_plain(p);
		} else
			tv67 = tv_plain(p);
		// 104
		if (skip && 260 >= skip) skip = 0;
		if (!skip) {
			p = sat(T0 * R2c + 0.0f);
			pm68 = w24(p); em68 = true;
			tv68 = tv_plain(p);
		} else
			tv68 = tv_plain(p);
		// 105
		if (skip && 261 >= skip) skip = 0;
		if (em66) M28 = pm66;
		if (!skip) {
			p = sat(k[69] * R3a + M13);
			tv69 = tv_plain(p);
			T0 = tv67;
		} else
			tv69 = tv_plain(p);
		// 106
		if (skip && 262 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[70] * R3b + p) * 4.0f);
			pm70 = w24(p); em70 = true;
			tv70 = tv_plain(p);
		} else
			tv70 = tv_plain(p);
		// 107
		if (skip && 263 >= skip) skip = 0;
		if (em68) M01 = pm68;
		if (!skip) {
			p = satpos(T0 * M01 + R2c);
			pm71 = w24(p); em71 = true;
			tv71 = tv_plain(p);
		} else
			tv71 = tv_plain(p);
		// 108
		if (skip && 264 >= skip) skip = 0;
		if (!skip) {
			p = sat(R2d - p);
			tv72 = tv_plain(p);
		} else
			tv72 = tv_plain(p);
		// 109
		if (skip && 265 >= skip) skip = 0;
		if (em70) M29 = pm70;
		if (!skip) {
			tv73 = tv_plain(p);
			T3 = tv71;
		} else
			tv73 = tv_plain(p);
		// 10a
		if (skip && 266 >= skip) skip = 0;
		if (em71) M04 = pm71;
		if (!skip) {
			p = sat(M04 + p);
			tv74 = tv_plain(p);
		} else
			tv74 = tv_plain(p);
		// 10b
		if (skip && 267 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[75] * R22 + 0.0f);
			pm75 = w24(p); em75 = true;
			tv75 = tv_plain(p);
		} else
			tv75 = tv_plain(p);
		// 10c
		if (skip && 268 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[76] * R25 + 0.0f);
			pm76 = w24(p); em76 = true;
			tv76 = tv_plain(p);
			T0 = tv74;
		} else
			tv76 = tv_plain(p);
		// 10d
		if (skip && 269 >= skip) skip = 0;
		if (!skip) {
			p = sat(T0 * R2f + M08);
			pr77 = w24(p); er77 = true;
			tv77 = tv_plain(p);
		} else
			tv77 = tv_plain(p);
		// 10e
		if (skip && 270 >= skip) skip = 0;
		if (em75) M12 = pm75;
		if (!skip) {
			p = sat(T5 * R31 + p);
			pm78 = w24(p); em78 = true;
			pr78 = w24(p); er78 = true;
			tv78 = tv_plain(p);
		} else
			tv78 = tv_plain(p);
		// 10f
		if (skip && 271 >= skip) skip = 0;
		if (em76) M13 = pm76;
		if (!skip) {
			p = sat(T0 * R36 + M09);
			pr79 = w24(p); er79 = true;
			tv79 = tv_plain(p);
		} else
			tv79 = tv_plain(p);
		// 110
		if (skip && 272 >= skip) skip = 0;
		if (er77) R30 = pr77;
		if (!skip) {
			p = sat(T5 * R38 + p);
			pm80 = w24(p); em80 = true;
			pr80 = w24(p); er80 = true;
			tv80 = tv_plain(p);
		} else
			tv80 = tv_plain(p);
		// 111
		if (skip && 273 >= skip) skip = 0;
		if (em78) M02 = pm78;
		if (er78) R32 = pr78;
		if (!skip) {
			p = k[81] * R2f + 0.0f;
			tv81 = tv_plain(p);
			T0 = k[81];
		} else
			tv81 = tv_plain(p);
		// 112
		if (skip && 274 >= skip) skip = 0;
		if (er79) R37 = pr79;
		if (!skip) {
			p = sat((T3 * M02 + p) * 4.0f);
			pm82 = w24(p); em82 = true;
			pr82 = w24(p); er82 = true;
			tv82 = tv_plain(p);
		} else
			tv82 = tv_plain(p);
		// 113
		if (skip && 275 >= skip) skip = 0;
		if (em80) M06 = pm80;
		if (er80) R39 = pr80;
		if (!skip) {
			p = T0 * R36 + 0.0f;
			tv83 = tv_plain(p);
		} else
			tv83 = tv_plain(p);
		// 114
		if (skip && 276 >= skip) skip = 0;
		if (!skip) {
			p = sat((T3 * M06 + p) * 4.0f);
			pm84 = w24(p); em84 = true;
			pr84 = w24(p); er84 = true;
			tv84 = tv_plain(p);
		} else
			tv84 = tv_plain(p);
		// 115
		if (skip && 277 >= skip) skip = 0;
		if (em82) M03 = pm82;
		if (er82) R2f = pr82;
		if (!skip) {
			p = satpos((k[85] * M01 + R2c) * 4.0f);
			tv85 = tv_plain(p);
		} else
			tv85 = tv_plain(p);
		// 116
		if (skip && 278 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * M03 + R31);
			pr86 = w24(p); er86 = true;
			tv86 = tv_plain(p);
		} else
			tv86 = tv_plain(p);
		// 117
		if (skip && 279 >= skip) skip = 0;
		if (em84) M07 = pm84;
		if (er84) R36 = pr84;
		if (!skip) {
			p = sat(T3 * M07 + R38);
			pr87 = w24(p); er87 = true;
			tv87 = tv_plain(p);
			T3 = tv85;
		} else
			tv87 = tv_plain(p);
		// 118
		if (skip && 280 >= skip) skip = 0;
		if (!skip) {
			p = T3 * R33 - R33;
			tv88 = tv_plain(p);
		} else
			tv88 = tv_plain(p);
		// 119
		if (skip && 281 >= skip) skip = 0;
		if (er86) R31 = pr86;
		if (!skip) {
			p = sat(T3 * R31 - p);
			pr89 = w24(p); er89 = true;
			tv89 = tv_plain(p);
		} else
			tv89 = tv_plain(p);
		// 11a
		if (skip && 282 >= skip) skip = 0;
		if (er87) R38 = pr87;
		if (!skip) {
			p = T3 * R3a - R3a;
			tv90 = tv_plain(p);
		} else
			tv90 = tv_plain(p);
		// 11b
		if (skip && 283 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * R38 - p);
			pr91 = w24(p); er91 = true;
			tv91 = tv_plain(p);
		} else
			tv91 = tv_plain(p);
		// 11c
		if (skip && 284 >= skip) skip = 0;
		if (er89) R33 = pr89;
		if (!skip) {
			p = T3 * R34 - R34;
			tv92 = tv_plain(p);
		} else
			tv92 = tv_plain(p);
		// 11d
		if (skip && 285 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * R33 - p);
			pr93 = w24(p); er93 = true;
			tv93 = tv_plain(p);
		} else
			tv93 = tv_plain(p);
		// 11e
		if (skip && 286 >= skip) skip = 0;
		if (er91) R3a = pr91;
		if (!skip) {
			p = T3 * R3b - R3b;
			tv94 = tv_plain(p);
		} else
			tv94 = tv_plain(p);
		// 11f
		if (skip && 287 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * R3a - p);
			pr95 = w24(p); er95 = true;
			tv95 = tv_plain(p);
		} else
			tv95 = tv_plain(p);
		if (er93) R34 = pr93;
		if (er95) R3b = pr95;
		out[0] = M28;
		out[1] = M29;
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
	float M08 = 0.0f;
	float M09 = 0.0f;
	float M12 = 0.0f;
	float M13 = 0.0f;
	float M28 = 0.0f;
	float M29 = 0.0f;
	float R01 = 0.0f;
	float R03 = 0.0f;
	float R05 = 0.0f;
	float R06 = 0.0f;
	float R20 = 0.0f;
	float R21 = 0.0f;
	float R22 = 0.0f;
	float R23 = 0.0f;
	float R24 = 0.0f;
	float R25 = 0.0f;
	float R28 = 0.0f;
	float R29 = 0.0f;
	float R2a = 0.0f;
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
	float RRD = 0.0f;
	float RWR = 0.0f;
	float T0 = 0.0f;
	float T1 = 0.0f;
	float T2 = 0.0f;
	float T3 = 0.0f;
	float T5 = 0.0f;
	bool fn = false;
	bool fz = false;
	float p = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_DYNAFLT_H
