// license:BSD-3-Clause
// S-MU2000: インサーション 1: V-FLANGER
//
// **tools/meg_fx/emit_dyn.py が作ったもの。手で直さない。** firmware がこのエフェクトに置く MEG のプログラムの形を、
// float の C++ に書き起こした（doc/native-dsp.md の「MEG と同じ作りのエフェクト」）。このプログラムは分岐（条件つきで
// 先の命令を飛ばす）を持つので、レジスタを変数として持ち、3 命令遅れの書き込みを「書くかどうか」の印つきで回す。
// 係数と番地は firmware が MEG に書いた値を configure() で読む。値の目盛りはレジスタの 24bit を 1.0（p も同じ）。
// 入口: m28 m29 / 出口: m28 m29

#ifndef S_MU2000_DSP_MEG_FX_INS_VFLANGER_H
#define S_MU2000_DSP_MEG_FX_INS_VFLANGER_H

#include "meg_fx_common.h"

namespace smu2000::dsp {

class meg_fx_ins_vflanger : public meg_fx_base<96>
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0x0;   // 使う LFO（ビット = 番号）
	static constexpr uint8_t IN_REGS[N_IN] = { 0x28, 0x29 };    // 入口のレジスタ（m の番号）
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x28, 0x29 };  // 出口のレジスタ

	void reset() { reset_ram(); IX = 0; IX2 = 0; M01 = 0.0f; M02 = 0.0f; M03 = 0.0f; M04 = 0.0f; M05 = 0.0f; M06 = 0.0f; M07 = 0.0f; M12 = 0.0f; M13 = 0.0f; M28 = 0.0f; M29 = 0.0f; R03 = 0.0f; R04 = 0.0f; R05 = 0.0f; R06 = 0.0f; R20 = 0.0f; R21 = 0.0f; R22 = 0.0f; R23 = 0.0f; R24 = 0.0f; R25 = 0.0f; R26 = 0.0f; R27 = 0.0f; R29 = 0.0f; R2a = 0.0f; R2b = 0.0f; R2c = 0.0f; R2d = 0.0f; R2e = 0.0f; R2f = 0.0f; R30 = 0.0f; R32 = 0.0f; R33 = 0.0f; R34 = 0.0f; R35 = 0.0f; R38 = 0.0f; R39 = 0.0f; R3a = 0.0f; R3b = 0.0f; RRD = 0.0f; RWR = 0.0f; T1 = 0.0f; T2 = 0.0f; T3 = 0.0f; T5 = 0.0f; fn = false; fz = false; p = 0.0f; }

	// in: 入口。out: 出口。lfo: MEG の LFO 24 本の今の値
	void process(const float *in, float *out, const float *lfo)
	{
		m_n++;
		float *const ram = m_ram.data();
		float tv0;
		float pm0;
		bool em0 = false;
		int32_t pi0;
		bool ei0 = false;
		float tv1;
		int32_t pi1;
		bool ei1 = false;
		float tv2;
		float pm2;
		bool em2 = false;
		float tv3;
		float pq3;
		bool eq3 = false;
		float tv4;
		float pr4;
		bool er4 = false;
		float tv5;
		float pm5;
		bool em5 = false;
		float tv6;
		float pq6;
		bool eq6 = false;
		float tv7;
		float pr7;
		bool er7 = false;
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
		float tv12;
		float pr12;
		bool er12 = false;
		float pq12;
		bool eq12 = false;
		float tv13;
		float tv14;
		float pm14;
		bool em14 = false;
		float pr14;
		bool er14 = false;
		float tv15;
		float pr15;
		bool er15 = false;
		float tv16;
		float pr16;
		bool er16 = false;
		float tv17;
		float pr17;
		bool er17 = false;
		float tv18;
		float tv19;
		float tv20;
		float pr20;
		bool er20 = false;
		float tv21;
		float tv22;
		float pr22;
		bool er22 = false;
		float tv23;
		float tv24;
		float tv25;
		float pr25;
		bool er25 = false;
		float tv26;
		float tv27;
		float pr27;
		bool er27 = false;
		float tv28;
		float pr28;
		bool er28 = false;
		float tv29;
		float tv30;
		float pr30;
		bool er30 = false;
		float tv31;
		float pr31;
		bool er31 = false;
		float tv32;
		float tv33;
		float pr33;
		bool er33 = false;
		float tv34;
		float tv35;
		float tv36;
		float pr36;
		bool er36 = false;
		float tv37;
		float pr37;
		bool er37 = false;
		float tv38;
		float tv39;
		float pr39;
		bool er39 = false;
		float tv40;
		float tv41;
		float tv42;
		float pr42;
		bool er42 = false;
		float tv43;
		float tv44;
		float pr44;
		bool er44 = false;
		float tv45;
		float tv46;
		float pr46;
		bool er46 = false;
		float tv47;
		float tv48;
		float pr48;
		bool er48 = false;
		float tv49;
		float pm49;
		bool em49 = false;
		float tv50;
		float tv51;
		float pm51;
		bool em51 = false;
		float tv52;
		float tv53;
		float pm53;
		bool em53 = false;
		float tv54;
		float pm54;
		bool em54 = false;
		float tv55;
		int32_t pi55;
		bool ei55 = false;
		float tv56;
		int32_t pi56;
		bool ei56 = false;
		float tv57;
		float tv58;
		float tv59;
		float tv60;
		float pm60;
		bool em60 = false;
		float pq60;
		bool eq60 = false;
		float tv61;
		float pm61;
		bool em61 = false;
		float tv62;
		float pm62;
		bool em62 = false;
		float tv63;
		float pr63;
		bool er63 = false;
		float pq63;
		bool eq63 = false;
		float tv64;
		float tv65;
		float pm65;
		bool em65 = false;
		float tv66;
		float pr66;
		bool er66 = false;
		float tv67;
		float pr67;
		bool er67 = false;
		float tv68;
		float pm68;
		bool em68 = false;
		float tv69;
		float pm69;
		bool em69 = false;
		float pq69;
		bool eq69 = false;
		float tv70;
		float tv71;
		float pm71;
		bool em71 = false;
		float pr71;
		bool er71 = false;
		float tv72;
		float pm72;
		bool em72 = false;
		float pq72;
		bool eq72 = false;
		float tv73;
		float tv74;
		float pm74;
		bool em74 = false;
		float tv75;
		float pr75;
		bool er75 = false;
		float tv76;
		float pm76;
		bool em76 = false;
		float tv77;
		float tv78;
		float tv79;
		float pr79;
		bool er79 = false;
		float tv80;
		float tv81;
		float pr81;
		bool er81 = false;
		float tv82;
		float tv83;
		float tv84;
		float tv85;
		float tv86;
		float pw86;
		bool ew86 = false;
		float tv87;
		float tv88;
		float tv89;
		float tv90;
		float pw90;
		bool ew90 = false;
		float tv91;
		float tv92;
		float pm92;
		bool em92 = false;
		float tv93;
		float tv94;
		float tv95;
		float pm95;
		bool em95 = false;
		int skip = 0;
		M28 = in[0];
		M29 = in[1];
		// 0c0
		if (skip && 192 >= skip) skip = 0;
		if (!skip) {
			p = k[0] * R38 + 0.0f;
			pm0 = noise(); em0 = true;
			pi0 = idx_of(p); ei0 = true;
			tv0 = tv_index(p);
		} else
			tv0 = tv_plain(p);
		// 0c1
		if (skip && 193 >= skip) skip = 0;
		if (!skip) {
			p = k[1] * R39 + 0.0f;
			pi1 = idx_of(p); ei1 = true;
			tv1 = tv_index(p);
		} else
			tv1 = tv_plain(p);
		// 0c2
		if (skip && 194 >= skip) skip = 0;
		if (!skip) {
			p = k[2] + 0.0f;
			pm2 = w24(p); em2 = true;
			tv2 = tv_plain(p);
			T2 = tv0;
		} else
			tv2 = tv_plain(p);
		// 0c3
		if (skip && 195 >= skip) skip = 0;
		if (em0) M01 = pm0;
		if (ei0) IX = pi0;
		if (!skip) {
			tv3 = tv_plain(p);
			T3 = tv1;
			pq3 = ram[at(3, IX)]; eq3 = true;
		} else
			tv3 = tv_plain(p);
		// 0c4
		if (skip && 196 >= skip) skip = 0;
		if (ei1) IX2 = pi1;
		if (!skip) {
			p = k[4] * M28 + 0.0f;
			pr4 = w24(p); er4 = true;
			tv4 = tv_plain(p);
		} else
			tv4 = tv_plain(p);
		// 0c5
		if (skip && 197 >= skip) skip = 0;
		if (em2) M06 = pm2;
		if (eq3) RRD = pq3;
		if (!skip) {
			p = k[5] * R21 + 0.0f;
			pm5 = RRD; em5 = true;
			tv5 = tv_plain(p);
		} else
			tv5 = tv_plain(p);
		// 0c6
		if (skip && 198 >= skip) skip = 0;
		if (!skip) {
			p = k[6] * R20 + p;
			tv6 = tv_plain(p);
			pq6 = ram[at(6, IX + 1)]; eq6 = true;
		} else
			tv6 = tv_plain(p);
		// 0c7
		if (skip && 199 >= skip) skip = 0;
		if (er4) R20 = pr4;
		if (!skip) {
			p = sat((k[7] * R20 + p) * 2.0f);
			pr7 = w24(p); er7 = true;
			tv7 = tv_plain(p);
		} else
			tv7 = tv_plain(p);
		// 0c8
		if (skip && 200 >= skip) skip = 0;
		if (em5) M02 = pm5;
		if (eq6) RRD = pq6;
		if (!skip) {
			p = k[8] * R22 + 0.0f;
			pm8 = RRD; em8 = true;
			tv8 = tv_plain(p);
		} else
			tv8 = tv_plain(p);
		// 0c9
		if (skip && 201 >= skip) skip = 0;
		if (!skip) {
			p = k[9] * R21 + p;
			pr9 = R21; er9 = true;
			tv9 = tv_plain(p);
			pq9 = ram[at(9, IX2)]; eq9 = true;
		} else
			tv9 = tv_plain(p);
		// 0ca
		if (skip && 202 >= skip) skip = 0;
		if (er7) R21 = pr7;
		if (!skip) {
			p = k[10] * R21 + p;
			tv10 = tv_plain(p);
		} else
			tv10 = tv_plain(p);
		// 0cb
		if (skip && 203 >= skip) skip = 0;
		if (em8) M03 = pm8;
		if (eq9) RRD = pq9;
		if (!skip) {
			p = k[11] * R23 + p;
			pm11 = RRD; em11 = true;
			tv11 = tv_plain(p);
		} else
			tv11 = tv_plain(p);
		// 0cc
		if (skip && 204 >= skip) skip = 0;
		if (er9) R22 = pr9;
		if (!skip) {
			p = sat((k[12] * R24 + p) * 4.0f);
			pr12 = w24(p); er12 = true;
			tv12 = tv_plain(p);
			pq12 = ram[at(12, IX2 + 1)]; eq12 = true;
		} else
			tv12 = tv_plain(p);
		// 0cd
		if (skip && 205 >= skip) skip = 0;
		if (!skip) {
			p = k[13] * R25 + 0.0f;
			tv13 = tv_plain(p);
		} else
			tv13 = tv_plain(p);
		// 0ce
		if (skip && 206 >= skip) skip = 0;
		if (em11) M04 = pm11;
		if (eq12) RRD = pq12;
		if (!skip) {
			p = k[14] * R23 + p;
			pm14 = RRD; em14 = true;
			pr14 = R23; er14 = true;
			tv14 = tv_plain(p);
		} else
			tv14 = tv_plain(p);
		// 0cf
		if (skip && 207 >= skip) skip = 0;
		if (er12) R23 = pr12;
		if (!skip) {
			p = sat((k[15] * R23 + p) * 16.0f);
			pr15 = w24(p); er15 = true;
			tv15 = tv_plain(p);
		} else
			tv15 = tv_plain(p);
		// 0d0
		if (skip && 208 >= skip) skip = 0;
		if (!skip) {
			p = k[16] * M06 + R32;
			fn = p < 0.0f; fz = p == 0.0f;
			pr16 = w24(p); er16 = true;
			tv16 = tv_plain(p);
		} else
			tv16 = tv_plain(p);
		// 0d1
		if (skip && 209 >= skip) skip = 0;
		if (em14) M05 = pm14;
		if (er14) R24 = pr14;
		if (!skip) {
			p = k[17] * M29 + 0.0f;
			pr17 = w24(p); er17 = true;
			tv17 = tv_plain(p);
		} else
			tv17 = tv_plain(p);
		// 0d2
		if (skip && 210 >= skip) skip = 0;
		if (er15) R25 = pr15;
		if (!skip) {
			p = k[18] * R2a + 0.0f;
			tv18 = tv_plain(p);
		} else
			tv18 = tv_plain(p);
		// 0d3
		if (skip && 211 >= skip) skip = 0;
		if (er16) R32 = pr16;
		if (!skip) {
			p = k[19] * R29 + p;
			tv19 = tv_plain(p);
		} else
			tv19 = tv_plain(p);
		// 0d4
		if (skip && 212 >= skip) skip = 0;
		if (er17) R29 = pr17;
		if (!skip) {
			p = sat((k[20] * R29 + p) * 2.0f);
			pr20 = w24(p); er20 = true;
			tv20 = tv_plain(p);
		} else
			tv20 = tv_plain(p);
		// 0d5
		if (skip && 213 >= skip) skip = 0;
		if (!skip) {
			p = k[21] * R2b + 0.0f;
			tv21 = tv_plain(p);
		} else
			tv21 = tv_plain(p);
		// 0d6
		if (skip && 214 >= skip) skip = 0;
		if (!skip) {
			p = k[22] * R2a + p;
			pr22 = R2a; er22 = true;
			tv22 = tv_plain(p);
		} else
			tv22 = tv_plain(p);
		// 0d7
		if (skip && 215 >= skip) skip = 0;
		if (er20) R2a = pr20;
		if (!skip) {
			p = k[23] * R2a + p;
			tv23 = tv_plain(p);
		} else
			tv23 = tv_plain(p);
		// 0d8
		if (skip && 216 >= skip) skip = 0;
		if (!skip) {
			p = k[24] * R2c + p;
			tv24 = tv_plain(p);
		} else
			tv24 = tv_plain(p);
		// 0d9
		if (skip && 217 >= skip) skip = 0;
		if (er22) R2b = pr22;
		if (!skip) {
			p = sat((k[25] * R2d + p) * 4.0f);
			pr25 = w24(p); er25 = true;
			tv25 = tv_plain(p);
		} else
			tv25 = tv_plain(p);
		// 0da
		if (skip && 218 >= skip) skip = 0;
		if (!skip) {
			p = k[26] * R2e + 0.0f;
			tv26 = tv_plain(p);
		} else
			tv26 = tv_plain(p);
		// 0db
		if (skip && 219 >= skip) skip = 0;
		if (!skip) {
			p = k[27] * R2c + p;
			pr27 = R2c; er27 = true;
			tv27 = tv_plain(p);
		} else
			tv27 = tv_plain(p);
		// 0dc
		if (skip && 220 >= skip) skip = 0;
		if (er25) R2c = pr25;
		if (!skip) {
			p = sat((k[28] * R2c + p) * 16.0f);
			pr28 = w24(p); er28 = true;
			tv28 = tv_plain(p);
		} else
			tv28 = tv_plain(p);
		// 0dd
		if (skip && 221 >= skip) skip = 0;
		if (!skip && true && 223 > 221) skip = 223;
		tv29 = tv_plain(p);
		// 0de
		if (skip && 222 >= skip) skip = 0;
		if (er27) R2d = pr27;
		if (!skip) {
			p = M01 + 0.0f;
			pr30 = w24(p); er30 = true;
			tv30 = tv_plain(p);
		} else
			tv30 = tv_plain(p);
		// 0df
		if (skip && 223 >= skip) skip = 0;
		if (er28) R2e = pr28;
		if (!skip) {
			p = sat((k[31] + std::fabs(R32)) * 2.0f);
			pr31 = w24(p); er31 = true;
			tv31 = tv_plain(p);
		} else
			tv31 = tv_plain(p);
		// 0e0
		if (skip && 224 >= skip) skip = 0;
		if (!skip) {
			p = R32 + 0.0f;
			tv32 = tv_plain(p);
		} else
			tv32 = tv_plain(p);
		// 0e1
		if (skip && 225 >= skip) skip = 0;
		if (er30) R3a = pr30;
		if (!skip) {
			p = k[33] + p;
			pr33 = w24(p); er33 = true;
			tv33 = tv_plain(p);
		} else
			tv33 = tv_plain(p);
		// 0e2
		if (skip && 226 >= skip) skip = 0;
		if (er31) R03 = pr31;
		if (!skip) {
			p = R33 - p;
			fn = p < 0.0f; fz = p == 0.0f;
			tv34 = tv_plain(p);
		} else
			tv34 = tv_plain(p);
		// 0e3
		if (skip && 227 >= skip) skip = 0;
		if (!skip) {
			p = k[35] * R3a + 0.0f;
			tv35 = tv_plain(p);
		} else
			tv35 = tv_plain(p);
		// 0e4
		if (skip && 228 >= skip) skip = 0;
		if (er33) R33 = pr33;
		if (!skip) {
			p = sat(k[36] * R03 + p);
			pr36 = w24(p); er36 = true;
			tv36 = tv_plain(p);
		} else
			tv36 = tv_plain(p);
		// 0e5
		if (skip && 229 >= skip) skip = 0;
		if (!skip) {
			p = sat((k[37] + std::fabs(R33)) * 2.0f);
			pr37 = w24(p); er37 = true;
			tv37 = tv_plain(p);
		} else
			tv37 = tv_plain(p);
		// 0e6
		if (skip && 230 >= skip) skip = 0;
		if (!skip && true && 232 > 230) skip = 232;
		tv38 = tv_plain(p);
		// 0e7
		if (skip && 231 >= skip) skip = 0;
		if (er36) R03 = pr36;
		if (!skip) {
			p = M01 + 0.0f;
			pr39 = w24(p); er39 = true;
			tv39 = tv_plain(p);
		} else
			tv39 = tv_plain(p);
		// 0e8
		if (skip && 232 >= skip) skip = 0;
		if (er37) R04 = pr37;
		if (!skip) {
			p = k[40] * R2e + 0.0f;
			tv40 = tv_plain(p);
		} else
			tv40 = tv_plain(p);
		// 0e9
		if (skip && 233 >= skip) skip = 0;
		if (!skip) {
			p = k[41] * R25 + p;
			tv41 = tv_plain(p);
		} else
			tv41 = tv_plain(p);
		// 0ea
		if (skip && 234 >= skip) skip = 0;
		if (er39) R3b = pr39;
		if (!skip) {
			p = sat(k[42] * R27 + p);
			pr42 = w24(p); er42 = true;
			tv42 = tv_plain(p);
		} else
			tv42 = tv_plain(p);
		// 0eb
		if (skip && 235 >= skip) skip = 0;
		if (!skip) {
			p = k[43] * R3b + 0.0f;
			tv43 = tv_plain(p);
		} else
			tv43 = tv_plain(p);
		// 0ec
		if (skip && 236 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[44] * R04 + p);
			pr44 = w24(p); er44 = true;
			tv44 = tv_plain(p);
		} else
			tv44 = tv_plain(p);
		// 0ed
		if (skip && 237 >= skip) skip = 0;
		if (er42) R05 = pr42;
		if (!skip) {
			p = k[45] * R34 - R34;
			tv45 = tv_plain(p);
			T1 = k[45];
		} else
			tv45 = tv_plain(p);
		// 0ee
		if (skip && 238 >= skip) skip = 0;
		if (!skip) {
			p = sat(T1 * R03 - p);
			pr46 = w24(p); er46 = true;
			tv46 = tv_plain(p);
		} else
			tv46 = tv_plain(p);
		// 0ef
		if (skip && 239 >= skip) skip = 0;
		if (er44) R04 = pr44;
		if (!skip) {
			p = T1 * R35 - R35;
			tv47 = tv_plain(p);
		} else
			tv47 = tv_plain(p);
		// 0f0
		if (skip && 240 >= skip) skip = 0;
		if (!skip) {
			p = sat(T1 * R04 - p);
			pr48 = w24(p); er48 = true;
			tv48 = tv_plain(p);
		} else
			tv48 = tv_plain(p);
		// 0f1
		if (skip && 241 >= skip) skip = 0;
		if (er46) R34 = pr46;
		if (!skip) {
			p = k[49] + 0.0f;
			pm49 = w24(p); em49 = true;
			tv49 = tv_plain(p);
		} else
			tv49 = tv_plain(p);
		// 0f2
		if (skip && 242 >= skip) skip = 0;
		if (!skip) {
			p = T2 * M02 - M02;
			tv50 = tv_plain(p);
		} else
			tv50 = tv_plain(p);
		// 0f3
		if (skip && 243 >= skip) skip = 0;
		if (er48) R35 = pr48;
		if (!skip) {
			p = sat(T2 * M03 - p);
			pm51 = w24(p); em51 = true;
			tv51 = tv_plain(p);
		} else
			tv51 = tv_plain(p);
		// 0f4
		if (skip && 244 >= skip) skip = 0;
		if (em49) M06 = pm49;
		if (!skip) {
			p = T3 * M04 - M04;
			tv52 = tv_plain(p);
		} else
			tv52 = tv_plain(p);
		// 0f5
		if (skip && 245 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * M05 - p);
			pm53 = w24(p); em53 = true;
			tv53 = tv_plain(p);
		} else
			tv53 = tv_plain(p);
		// 0f6
		if (skip && 246 >= skip) skip = 0;
		if (em51) M02 = pm51;
		if (!skip) {
			p = sat(k[54] * M02 - R26);
			pm54 = w24(p); em54 = true;
			tv54 = tv_plain(p);
		} else
			tv54 = tv_plain(p);
		// 0f7
		if (skip && 247 >= skip) skip = 0;
		if (!skip) {
			p = satpos(k[55] * R34 + M06);
			pi55 = idx_of(p); ei55 = true;
			tv55 = tv_index(p);
			T1 = k[55];
		} else
			tv55 = tv_plain(p);
		// 0f8
		if (skip && 248 >= skip) skip = 0;
		if (em53) M04 = pm53;
		if (!skip) {
			p = satpos(T1 * R35 + M06);
			pi56 = idx_of(p); ei56 = true;
			tv56 = tv_index(p);
		} else
			tv56 = tv_plain(p);
		// 0f9
		if (skip && 249 >= skip) skip = 0;
		if (em54) M02 = pm54;
		if (!skip) {
			tv57 = tv_plain(p);
			T2 = tv55;
		} else
			tv57 = tv_plain(p);
		// 0fa
		if (skip && 250 >= skip) skip = 0;
		if (ei55) IX = pi55;
		if (!skip) {
			tv58 = tv_plain(p);
			T3 = tv56;
		} else
			tv58 = tv_plain(p);
		// 0fb
		if (skip && 251 >= skip) skip = 0;
		if (ei56) IX2 = pi56;
		if (!skip) {
			tv59 = tv_plain(p);
		} else
			tv59 = tv_plain(p);
		// 0fc
		if (skip && 252 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[60] * M04 - R2f);
			pm60 = w24(p); em60 = true;
			tv60 = tv_plain(p);
			pq60 = tab(60, IX); eq60 = true;
		} else
			tv60 = tv_plain(p);
		// 0fd
		if (skip && 253 >= skip) skip = 0;
		if (!skip) {
			p = k[61] + 0.0f;
			pm61 = w24(p); em61 = true;
			tv61 = tv_plain(p);
		} else
			tv61 = tv_plain(p);
		// 0fe
		if (skip && 254 >= skip) skip = 0;
		if (eq60) RRD = pq60;
		if (!skip) {
			p = k[62] * R2e + 0.0f;
			pm62 = RRD; em62 = true;
			tv62 = tv_plain(p);
		} else
			tv62 = tv_plain(p);
		// 0ff
		if (skip && 255 >= skip) skip = 0;
		if (em60) M04 = pm60;
		if (!skip) {
			p = sat(k[63] * R30 + p);
			pr63 = w24(p); er63 = true;
			tv63 = tv_plain(p);
			pq63 = tab(63, IX + 1); eq63 = true;
		} else
			tv63 = tv_plain(p);
		// 100
		if (skip && 256 >= skip) skip = 0;
		if (em61) M03 = pm61;
		if (!skip) {
			p = sat(k[64] * R34 + M03);
			tv64 = tv_plain(p);
		} else
			tv64 = tv_plain(p);
		// 101
		if (skip && 257 >= skip) skip = 0;
		if (em62) M06 = pm62;
		if (eq63) RRD = pq63;
		if (!skip) {
			p = sat(k[65] * R35 + M03);
			pm65 = RRD; em65 = true;
			tv65 = tv_plain(p);
		} else
			tv65 = tv_plain(p);
		// 102
		if (skip && 258 >= skip) skip = 0;
		if (er63) R06 = pr63;
		if (!skip) {
			p = sat(k[66] * M02 + R26);
			pr66 = w24(p); er66 = true;
			tv66 = tv_plain(p);
			T1 = tv64;
		} else
			tv66 = tv_plain(p);
		// 103
		if (skip && 259 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[67] * M04 + R2f);
			pr67 = w24(p); er67 = true;
			tv67 = tv_plain(p);
			T5 = tv65;
		} else
			tv67 = tv_plain(p);
		// 104
		if (skip && 260 >= skip) skip = 0;
		if (em65) M07 = pm65;
		if (!skip) {
			p = sat(T1 * M12 - R26);
			pm68 = w24(p); em68 = true;
			tv68 = tv_plain(p);
		} else
			tv68 = tv_plain(p);
		// 105
		if (skip && 261 >= skip) skip = 0;
		if (er66) R26 = pr66;
		if (!skip) {
			p = sat(T5 * M13 - R2f);
			pm69 = w24(p); em69 = true;
			tv69 = tv_plain(p);
			pq69 = tab(69, IX2); eq69 = true;
		} else
			tv69 = tv_plain(p);
		// 106
		if (skip && 262 >= skip) skip = 0;
		if (er67) R2f = pr67;
		if (!skip) {
			p = T2 * M06 - M06;
			tv70 = tv_plain(p);
		} else
			tv70 = tv_plain(p);
		// 107
		if (skip && 263 >= skip) skip = 0;
		if (em68) M02 = pm68;
		if (eq69) RRD = pq69;
		if (!skip) {
			p = sat(T2 * M07 - p);
			pm71 = RRD; em71 = true;
			pr71 = w24(p); er71 = true;
			tv71 = tv_plain(p);
		} else
			tv71 = tv_plain(p);
		// 108
		if (skip && 264 >= skip) skip = 0;
		if (em69) M04 = pm69;
		if (!skip) {
			p = sat(T1 * R26 + M02);
			pm72 = w24(p); em72 = true;
			tv72 = tv_plain(p);
			pq72 = tab(72, IX2 + 1); eq72 = true;
		} else
			tv72 = tv_plain(p);
		// 109
		if (skip && 265 >= skip) skip = 0;
		if (!skip) {
			p = k[73] * R27 + 0.0f;
			tv73 = tv_plain(p);
		} else
			tv73 = tv_plain(p);
		// 10a
		if (skip && 266 >= skip) skip = 0;
		if (em71) M06 = pm71;
		if (er71) R38 = pr71;
		if (eq72) RRD = pq72;
		if (!skip) {
			p = k[74] * M12 + p;
			pm74 = RRD; em74 = true;
			tv74 = tv_plain(p);
		} else
			tv74 = tv_plain(p);
		// 10b
		if (skip && 267 >= skip) skip = 0;
		if (em72) M12 = pm72;
		if (!skip) {
			p = sat(k[75] * M12 + p);
			pr75 = w24(p); er75 = true;
			tv75 = tv_plain(p);
		} else
			tv75 = tv_plain(p);
		// 10c
		if (skip && 268 >= skip) skip = 0;
		if (!skip) {
			p = sat(T5 * R2f + M04);
			pm76 = w24(p); em76 = true;
			tv76 = tv_plain(p);
		} else
			tv76 = tv_plain(p);
		// 10d
		if (skip && 269 >= skip) skip = 0;
		if (em74) M07 = pm74;
		if (!skip) {
			p = k[77] * R30 + 0.0f;
			tv77 = tv_plain(p);
		} else
			tv77 = tv_plain(p);
		// 10e
		if (skip && 270 >= skip) skip = 0;
		if (er75) R27 = pr75;
		if (!skip) {
			p = k[78] * M13 + p;
			tv78 = tv_plain(p);
		} else
			tv78 = tv_plain(p);
		// 10f
		if (skip && 271 >= skip) skip = 0;
		if (em76) M13 = pm76;
		if (!skip) {
			p = sat(k[79] * M13 + p);
			pr79 = w24(p); er79 = true;
			tv79 = tv_plain(p);
		} else
			tv79 = tv_plain(p);
		// 110
		if (skip && 272 >= skip) skip = 0;
		if (!skip) {
			p = T3 * M06 - M06;
			tv80 = tv_plain(p);
		} else
			tv80 = tv_plain(p);
		// 111
		if (skip && 273 >= skip) skip = 0;
		if (!skip) {
			p = sat(T3 * M07 - p);
			pr81 = w24(p); er81 = true;
			tv81 = tv_plain(p);
		} else
			tv81 = tv_plain(p);
		// 112
		if (skip && 274 >= skip) skip = 0;
		if (er79) R30 = pr79;
		if (!skip) {
			p = sat(k[82] + std::fabs(R05));
			tv82 = tv_plain(p);
		} else
			tv82 = tv_plain(p);
		// 113
		if (skip && 275 >= skip) skip = 0;
		if (!skip) {
			p = sat(k[83] + std::fabs(R06));
			tv83 = tv_plain(p);
		} else
			tv83 = tv_plain(p);
		// 114
		if (skip && 276 >= skip) skip = 0;
		if (er81) R39 = pr81;
		if (!skip) {
			tv84 = tv_plain(p);
			T1 = tv82;
		} else
			tv84 = tv_plain(p);
		// 115
		if (skip && 277 >= skip) skip = 0;
		if (!skip) {
			p = (R05 + 0.0f) * 2.0f;
			tv85 = tv_plain(p);
			T2 = tv83;
		} else
			tv85 = tv_plain(p);
		// 116
		if (skip && 278 >= skip) skip = 0;
		if (!skip) {
			p = sat(T1 * R05 - p);
			pw86 = p; ew86 = true;
			tv86 = tv_plain(p);
		} else
			tv86 = tv_plain(p);
		// 117
		if (skip && 279 >= skip) skip = 0;
		if (!skip) {
			tv87 = tv_plain(p);
		} else
			tv87 = tv_plain(p);
		// 118
		if (skip && 280 >= skip) skip = 0;
		if (ew86) RWR = pw86;
		if (!skip) {
			tv88 = tv_plain(p);
		} else
			tv88 = tv_plain(p);
		// 119
		if (skip && 281 >= skip) skip = 0;
		if (!skip) {
			p = (R06 + 0.0f) * 2.0f;
			tv89 = tv_plain(p);
		} else
			tv89 = tv_plain(p);
		// 11a
		if (skip && 282 >= skip) skip = 0;
		if (!skip) {
			p = sat(T2 * R06 - p);
			pw90 = p; ew90 = true;
			tv90 = tv_plain(p);
			ram[at(90)] = RWR;
		} else
			tv90 = tv_plain(p);
		// 11b
		if (skip && 283 >= skip) skip = 0;
		if (!skip) {
			p = k[91] * R27 + 0.0f;
			tv91 = tv_plain(p);
			T1 = k[91];
		} else
			tv91 = tv_plain(p);
		// 11c
		if (skip && 284 >= skip) skip = 0;
		if (ew90) RWR = pw90;
		if (!skip) {
			p = sat((k[92] * R25 + p) * 4.0f);
			pm92 = w24(p); em92 = true;
			tv92 = tv_plain(p);
			T2 = k[92];
		} else
			tv92 = tv_plain(p);
		// 11d
		if (skip && 285 >= skip) skip = 0;
		if (!skip) {
			p = k[93] * R27 + 0.0f;
			tv93 = tv_plain(p);
			ram[at(93)] = RWR;
		} else
			tv93 = tv_plain(p);
		// 11e
		if (skip && 286 >= skip) skip = 0;
		if (!skip) {
			p = T1 * R30 + p;
			tv94 = tv_plain(p);
		} else
			tv94 = tv_plain(p);
		// 11f
		if (skip && 287 >= skip) skip = 0;
		if (em92) M28 = pm92;
		if (!skip) {
			p = sat((T2 * R2e + p) * 4.0f);
			pm95 = w24(p); em95 = true;
			tv95 = tv_plain(p);
		} else
			tv95 = tv_plain(p);
		if (em95) M29 = pm95;
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
	float M12 = 0.0f;
	float M13 = 0.0f;
	float M28 = 0.0f;
	float M29 = 0.0f;
	float R03 = 0.0f;
	float R04 = 0.0f;
	float R05 = 0.0f;
	float R06 = 0.0f;
	float R20 = 0.0f;
	float R21 = 0.0f;
	float R22 = 0.0f;
	float R23 = 0.0f;
	float R24 = 0.0f;
	float R25 = 0.0f;
	float R26 = 0.0f;
	float R27 = 0.0f;
	float R29 = 0.0f;
	float R2a = 0.0f;
	float R2b = 0.0f;
	float R2c = 0.0f;
	float R2d = 0.0f;
	float R2e = 0.0f;
	float R2f = 0.0f;
	float R30 = 0.0f;
	float R32 = 0.0f;
	float R33 = 0.0f;
	float R34 = 0.0f;
	float R35 = 0.0f;
	float R38 = 0.0f;
	float R39 = 0.0f;
	float R3a = 0.0f;
	float R3b = 0.0f;
	float RRD = 0.0f;
	float RWR = 0.0f;
	float T1 = 0.0f;
	float T2 = 0.0f;
	float T3 = 0.0f;
	float T5 = 0.0f;
	bool fn = false;
	bool fz = false;
	float p = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_INS_VFLANGER_H
