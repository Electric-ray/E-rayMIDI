// license:BSD-3-Clause
//
// XG のエフェクトの種類（MSB/LSB）と、その値を、C++ のエフェクト（dsp::blocks.h）へ割り当てる。
// 軽量モードの入り口。doc/native-dsp.md を見よ。
//
// **実機（MEG）の再現ではない。** 種類ごとの系統（残響・ディレイ・揺れ・歪み・EQ・ダイナミクス・
// ローファイ）に合わせて、似た掛かり方の C++ の作りを当てているだけで、同じ音にはならない。
// 正しさが要るときは今までどおり MEG を回す（既定はそちら）。
//
// 値は XG の生の数（0-127 など）で渡す。意味（秒・ミリ秒・Hz・dB）は xg/fx_params.h の表から引く。

#ifndef S_MU2000_DSP_FX_NATIVE_H
#define S_MU2000_DSP_FX_NATIVE_H

#pragma once

#include "blocks.h"
#include "blocks2.h"
#include "reverb.h"
#include "xg/fx_params.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace smu2000::dsp {

// 生の値を、表を見て物理量に直す（"2.5" → 2.5、"0.1" → 0.1、数でない表示は def）
inline float fx_value(const xg::fx_param &p, int raw, float def = 0.0f)
{
	const int v = raw < int(p.lo) ? int(p.lo) : (raw > int(p.hi) ? int(p.hi) : raw);
	switch (p.fmt) {
	case xg::fx_fmt::tenths:
		return float(v) / 10.0f;
	case xg::fx_fmt::table: {
		if (!p.texts || v < int(p.lo))
			return float(v);
		const char *t = p.texts[v - int(p.lo)];
		if (!t || !*t)
			return def;
		char *end = nullptr;
		float f = std::strtof(t, &end);
		if (end == t)
			return def;                 // "off" や "D63>W" のような表示
		if (*end == 'k' || *end == 'K')
			f *= 1000.0f;               // "6.3k" は 6300
		return f;
	}
	default:
		return float(v);
	}
}

// 1 つのエフェクト（リバーブ・コーラス・バリエーション・インサーション 1-4）
class fx_slot
{
public:
	enum class kind { none, thru, reverb, early, delay, mod, rotary, drive, eq, wah, dyn, lofi,
	                  ring, slice, isolator, reso, cancel, enhancer, pitch, talk, chain };

	void set_rate(float rate)
	{
		m_rate = rate;
		m_rev.set_rate(rate);
		m_er.set_rate(rate);
		m_dly.set_rate(rate);
		m_mod.set_rate(rate);
		m_rot.set_rate(rate);
		m_drv.set_rate(rate);
		m_eq.set_rate(rate);
		m_wah.set_rate(rate);
		m_dyn.set_rate(rate);
		m_lofi.set_rate(rate);
		m_ring.set_rate(rate);
		m_slice.set_rate(rate);
		m_iso.set_rate(rate);
		m_reso.set_rate(rate);
		m_cancel.set_rate(rate);
		m_enh.set_rate(rate);
		m_pitch.set_rate(rate);
		m_talk.set_rate(rate);
	}

	kind current() const { return m_kind; }
	int  type() const { return m_type; }

	// 種類と、その種類のパラメータ（生の値。並びは xg/fx_params.h の順）
	void set(int type, const int *raw, int count)
	{
		const bool new_type = type != m_type;
		m_type = type;
		m_kind = kind_of(type >> 7);
		apply(raw, count);
		if (new_type)
			reset();
	}

	void reset()
	{
		m_rev.reset();
		m_er.reset();
		m_dly.reset();
		m_mod.reset();
		m_rot.reset();
		m_drv.reset();
		m_eq.reset();
		m_wah.reset();
		m_dyn.reset();
		m_lofi.reset();
		m_ring.reset();
		m_slice.reset();
		m_iso.reset();
		m_reso.reset();
		m_cancel.reset();
		m_enh.reset();
		m_pitch.reset();
		m_talk.reset();
	}

	// インサーションに掛かっているか。インサーションでは、そのパートの乾いた音は
	// ミキサの乾いた出口に出ない（MEG の中を通ってから戻る）。だから残響やディレイのように
	// 「濡れた音しか出さない」作りのものは、ここで Dry/Wet のぶんだけ乾いた音を足す。
	// 足さないと、ディレイを掛けたパートの直の音が丸ごと消える
	void set_insertion(bool on) { m_insertion = on; }

	void process(float l, float r, float &ol, float &orr)
	{
		// 入り口が黙ったままなら、中身が落ち着いたところで回すのをやめる（軽くするため）。
		// 尾（リバーブなど）が消えるまでは回し続ける
		if (l == 0.0f && r == 0.0f) {
			if (m_quiet > QUIET_LIMIT) {
				ol = orr = 0.0f;
				return;
			}
			m_quiet++;
		} else {
			m_quiet = 0;
		}

		switch (m_kind) {
		case kind::reverb: m_rev.process(l, r, ol, orr); break;
		case kind::early:  m_er.process(l, r, ol, orr); break;
		case kind::delay:  m_dly.process(l, r, ol, orr); break;
		case kind::mod:    m_mod.process(l, r, ol, orr); break;
		case kind::rotary: m_rot.process(l, r, ol, orr); break;
		case kind::drive:  m_drv.process(l, r, ol, orr); break;
		case kind::eq:     m_eq.process(l, r, ol, orr); break;
		case kind::wah:    m_wah.process(l, r, ol, orr); break;
		case kind::dyn:    m_dyn.process(l, r, ol, orr); break;
		case kind::lofi:   m_lofi.process(l, r, ol, orr); break;
		case kind::ring:   m_ring.process(l, r, ol, orr); break;
		case kind::slice:  m_slice.process(l, r, ol, orr); break;
		case kind::isolator: m_iso.process(l, r, ol, orr); break;
		case kind::reso:   m_reso.process(l, r, ol, orr); break;
		case kind::cancel: m_cancel.process(l, r, ol, orr); break;
		case kind::enhancer: m_enh.process(l, r, ol, orr); break;
		case kind::pitch:  m_pitch.process(l, r, ol, orr); break;
		case kind::talk:   m_talk.process(l, r, ol, orr); break;
		case kind::chain: {
			// 組み合わせの種類（CMP+DT+DLY など）は、順につないで通す
			float a = l, b = r, x = 0.0f, y = 0.0f;
			if (m_chain_comp) { m_dyn.process(a, b, x, y); a = x; b = y; }
			if (m_chain_wah)  { m_wah.process(a, b, x, y); a = x; b = y; }
			if (m_chain_drive){ m_drv.process(a, b, x, y); a = x; b = y; }
			if (m_chain_rotary){ m_rot.process(a, b, x, y); a = x; b = y; }
			if (m_chain_delay) {
				// ディレイは「足す」形（実機も直の音は減らない）
				m_dly.process(a, b, x, y);
				a += x * m_chain_delay_mix;
				b += y * m_chain_delay_mix;
			}
			ol = a * m_chain_level;
			orr = b * m_chain_level;
			break;
		}
		case kind::thru:   ol = l; orr = r; break;
		default:           ol = orr = 0.0f; break;
		}

		// 濡れた音しか出さない作りのものを、インサーションで使うとき。
		// Dry/Wet のぶんだけ濡れた音を弱めて、乾いた音を足す
		if (m_insertion && m_dry_mix > 0.0f) {
			// 濡れた音は Dry/Wet の割合そのまま。乾いた音は、ディレイ・残響では
			// Dry/Wet を下げても小さくならない（実測でディレイ 6 種が実機と 0.1dB 差）。
			// AMBIENCE だけは乾いた音も割合で下がる
			const float w = 1.0f - m_dry_mix;
			const float d = m_dry_full ? 1.0f : m_dry_mix;
			ol  = ol * w + l * d;
			orr = orr * w + r * d;
		}
	}

	// 種類の系統。xg::fx_categories() の分け方に合わせてある
	static kind kind_of(int msb)
	{
		if (msb == 0x00) return kind::none;
		if (msb == 0x40) return kind::thru;
		// 形が決まっているもの
		if (msb == 0x70 || msb == 0x71) return kind::ring;
		if (msb == 0x72) return kind::slice;
		if (msb == 0x73) return kind::isolator;
		if (msb == 0x74) return kind::reso;
		if (msb == 0x55 || msb == 0x14) return kind::cancel;
		if (msb == 0x51) return kind::enhancer;
		if (msb == 0x50) return kind::pitch;
		if (msb == 0x5d) return kind::talk;
		if (msb == 0x58) return kind::early;                 // AMBIENCE は短い残響として
		if (msb == 0x5f || msb == 0x60 || msb == 0x61) return kind::chain;
		if ((msb >= 0x01 && msb <= 0x04) || (msb >= 0x10 && msb <= 0x14)) return kind::reverb;
		if (msb >= 0x09 && msb <= 0x0b) return kind::early;
		if ((msb >= 0x05 && msb <= 0x08) || msb == 0x15 || msb == 0x16) return kind::delay;
		if (msb == 0x41 || msb == 0x42 || msb == 0x57 || msb == 0x43 || msb == 0x44 ||
		    msb == 0x48 || msb == 0x68 || msb == 0x6b || msb == 0x6c || msb == 0x6e ||
		    msb == 0x6f) return kind::mod;
		if (msb == 0x45 || msb == 0x46 || msb == 0x47 || msb == 0x56 || msb == 0x63) return kind::rotary;
		if (msb == 0x49 || msb == 0x4a || msb == 0x4b || msb == 0x62) return kind::drive;
		if (msb == 0x4c || msb == 0x4d || msb == 0x73) return kind::eq;
		if (msb == 0x4e || msb == 0x52 || msb == 0x6d) return kind::wah;
		if (msb == 0x53 || msb == 0x54 || msb == 0x69) return kind::dyn;
		if (msb == 0x5e || msb == 0x72 || msb == 0x75 || msb == 0x76) return kind::lofi;
		return kind::mod;
	}

private:
	// 名前でパラメータを引く（無ければ def）
	float par(const char *label, float def) const
	{
		if (!m_def)
			return def;
		for (int i = 0; i < m_def->count && i < MAX_PAR; i++)
			if (!std::strcmp(m_def->params[i].label, label))
				return fx_value(m_def->params[i], m_raw[i], def);
		return def;
	}

	// 生の値のまま（0-127 など）
	float raw_of(const char *label, int def) const
	{
		if (m_def)
			for (int i = 0; i < m_def->count && i < MAX_PAR; i++)
				if (!std::strcmp(m_def->params[i].label, label))
					return float(m_raw[i]);
		return float(def);
	}

	// EQ のゲイン（XG は 52-76 で ±12dB、64 が 0）
	float gain_db(const char *label) const
	{
		return clampf(raw_of(label, 64) - 64.0f, -12.0f, 12.0f);
	}

	// Dry/Wet（1-127。64 で半々、127 が全部 wet）
	float wet_of(float def) const
	{
		if (!has("Dry/Wet"))
			return def;
		return clampf(raw_of("Dry/Wet", 127) / 127.0f, 0.0f, 1.0f);
	}

	bool has(const char *label) const
	{
		if (!m_def)
			return false;
		for (int i = 0; i < m_def->count; i++)
			if (!std::strcmp(m_def->params[i].label, label))
				return true;
		return false;
	}

	void apply(const int *raw, int count)
	{
		m_def = xg::fx_find(m_type);
		for (int i = 0; i < MAX_PAR; i++)
			m_raw[i] = i < count ? raw[i] : 0;

		// 濡れた音しか出さない作り（残響・初期反射・ディレイ）は、インサーションのとき
		// Dry/Wet のぶんの乾いた音を足す。ほかの種類は中で混ぜてある
		m_dry_mix = (m_kind == kind::reverb || m_kind == kind::early || m_kind == kind::delay)
		            ? 1.0f - wet_of(0.5f) : 0.0f;
		m_dry_full = (m_type >> 7) != 0x58;

		switch (m_kind) {
		case kind::reverb: {
			reverb::params p;
			p.time        = par("ReverbTime", 2.0f);
			p.predelay_ms = par("InitDelay", 20.0f);
			p.hpf_hz      = par("HPF Cutoff", 80.0f);
			// High Damp（1-10）は高い音の減り方。表の LPF Cutoff（Hz）と合わせて使う
			p.damp_hz     = par("LPF Cutoff", 8000.0f) * clampf(par("High Damp", 5.0f) / 5.0f, 0.3f, 2.0f);
			p.diffusion   = clampf(par("Diffusion", 7.0f) / 10.0f, 0.0f, 1.0f);
			p.er_level    = clampf(par("Er/Rev", 5.0f) / 10.0f, 0.0f, 1.0f);
			p.width       = 1.0f;
			m_rev.set_params(p);
			break;
		}
		case kind::early: {
			early_ref::params p;
			p.room     = clampf(par("Room Size", 5.0f) / 10.0f, 0.1f, 1.0f);
			p.liveness = clampf(par("Liveness", 5.0f) / 10.0f, 0.0f, 1.0f);
			// 切るまでの長さ。GATE REVERB には Gate Time が無く、InitDelay は
			// 頭の隙間なので、そこを使うと 10ms になって尾が 12dB 足りなかった。
			// 部屋の大きさから決める
			p.time_ms  = has("Gate Time") ? par("Gate Time", 200.0f)
			             : has("Room Size") ? clampf(80.0f + raw_of("Room Size", 10) * 22.0f, 60.0f, 400.0f)
			             : par("InitDelay", 200.0f);
			p.diffuse  = clampf(par("Diffusion", 7.0f) / 10.0f, 0.0f, 1.0f);
			p.gate     = (m_type >> 7) == 0x0a;
			p.reverse  = (m_type >> 7) == 0x0b;
			// AMBIENCE（0x58）は初期反射で代わりをしている。そのままだと 4dB 大きい
			p.level    = 1.0f;
			m_er.set_params(p);
			break;
		}
		case kind::delay: {
			delay_fx::params p;
			p.l_ms = par("LchDelay", par("Lch Delay", par("DelayTime", 300.0f)));
			p.r_ms = par("RchDelay", par("Rch Delay", p.l_ms * 1.3f));
			p.c_ms = par("CchDelay", (p.l_ms + p.r_ms) * 0.5f);
			p.fb_ms = par("FB Delay", par("FBDelay1", p.l_ms));
			// FB Level は 1-127 で 64 が 0（表 T7 の -63%..+63%）。
			// ECHO は左右で別々に持つ
			const float fb_raw = has("FB Level") ? raw_of("FB Level", 64)
			                     : (raw_of("Lch FBLevl", 64) + raw_of("Rch FBLevl", 64)) * 0.5f;
			p.feedback = clampf((fb_raw - 64.0f) / 64.0f, -0.95f, 0.95f);
			// ECHO の 2 本目
			if (has("LchDelay2")) {
				p.l2_ms = par("LchDelay2", 0.0f);
				p.r2_ms = par("RchDelay2", p.l2_ms);
				p.level2 = clampf(raw_of("Delay2Lvl", 0) / 127.0f, 0.0f, 1.0f);
			}
			// 真ん中の音は LCR ディレイだけ。ほかの種類で足すと尾が倍になる
			p.c_level = has("Cch Level") ? clampf(raw_of("Cch Level", 100) / 127.0f, 0.0f, 1.0f) : 0.0f;
			p.hpf_hz = par("HPF Cutoff", 60.0f);
			p.lpf_hz = par("LPF Cutoff", 8000.0f) * clampf(par("High Damp", 5.0f) / 5.0f, 0.3f, 2.0f);
			// クロスディレイ（0x08）と T.CRS DLY（0x16）は左右を入れ替えて戻す
			p.cross = (m_type >> 7) == 0x08 || (m_type >> 7) == 0x16;
			if (p.cross) {
				p.l_ms = par("L~R Delay", par("L~R Dly", p.l_ms));
				p.r_ms = par("R~L Delay", par("R~L Dly", p.r_ms));
				p.fb_ms = (p.l_ms + p.r_ms) * 0.5f;
			}
			m_dly.set_params(p);
			break;
		}
		case kind::mod: {
			mod_fx::params p;
			const int msb = m_type >> 7;
			p.rate_hz = par("LFO Freq", 0.6f);
			p.depth = clampf(raw_of("LFO Depth", raw_of("Mod Depth", 40)) / 127.0f, 0.0f, 1.0f);
			// ModDlyOfst は 1-127 の生の値。0.1ms きざみとみて 0.1-12.7ms にする
			p.delay_ms = has("DelayOfst") ? par("DelayOfst", 10.0f)
			             : has("ModDlyOfst") ? clampf(raw_of("ModDlyOfst", 40) * 0.1f, 0.1f, 20.0f)
			             : 10.0f;
			// 戻す量。実機のフランジャーはここまで共振しないので、7 割にしてある
			p.feedback = clampf((raw_of("FB Level", raw_of("Mod FB", 64)) / 64.0f - 1.0f) * 0.4f, -0.8f, 0.8f);
			p.phase_deg = par("LFO Phase", par("PhaseShift", 90.0f));
			p.stages = par("Stage", 6.0f);
			p.dry_wet = has("Mod Mix") ? clampf(raw_of("Mod Mix", 40) / 127.0f, 0.0f, 1.0f) : wet_of(0.3f);
			if (msb == 0x43 || msb == 0x44 || msb == 0x68 || msb == 0x6b || msb == 0x6e || msb == 0x74)
				p.type = mod_fx::kind::flanger;
			else if (msb == 0x48 || msb == 0x6c || msb == 0x6f)
				p.type = mod_fx::kind::phaser;
			else if (msb == 0x42)
				p.type = mod_fx::kind::celeste;
			else if (msb == 0x57 || msb == 0x62)
				p.type = mod_fx::kind::ensemble;
			else
				p.type = mod_fx::kind::chorus;
			m_mod.set_params(p);
			break;
		}
		case kind::rotary: {
			rotary_fx::params p;
			const int msb = m_type >> 7;
			p.speed_hz = par("LFO Freq", par("RotorSpd", 5.0f));
			p.depth = clampf(raw_of("LFO Depth", raw_of("Mic Angle", 60)) / 127.0f, 0.0f, 1.0f);
			p.drive = clampf(raw_of("Drive", 0) / 127.0f, 0.0f, 1.0f);
			p.type = msb == 0x46 ? rotary_fx::kind::tremolo
			       : msb == 0x47 ? rotary_fx::kind::auto_pan
			                     : rotary_fx::kind::rotary;
			m_rot.set_params(p);
			break;
		}
		case kind::drive: {
			drive_fx::params p;
			p.drive = clampf(raw_of("Drive", raw_of("Dist Drive", 60)) / 127.0f, 0.0f, 1.0f);
			p.edge = clampf(raw_of("Edge", 64) / 127.0f, 0.0f, 1.0f);
			// 出口の大きさは、同じ曲を MEG と鳴らして rms を合わせた
			p.out_level = clampf(raw_of("OutputLvl", raw_of("DistOutLvl", 64)) / 49.0f, 0.0f, 2.6f);
			p.lpf_hz = par("LPF Cutoff", 4000.0f);
			p.eq_low_db = gain_db("EQ LowGain");
			p.eq_low_hz = par("EQ LowFreq", 200.0f);
			p.eq_mid_db = gain_db("EQ MidGain");
			p.eq_mid_hz = par("EQ MidFreq", 1400.0f);
			p.eq_mid_q = std::max(0.1f, par("EQ MidWidt", 10.0f) / 10.0f);
			p.dry_wet = wet_of(1.0f);
			m_drv.set_params(p);
			break;
		}
		case kind::eq: {
			eq_fx::params p;
			p.low_hz = par("EQ LowFreq", par("Low Freq", 200.0f));
			p.low_db = has("EQ LowGain") ? gain_db("EQ LowGain") : gain_db("Low Gain");
			p.mid_hz = par("EQ MidFreq", par("Mid Freq", 1000.0f));
			p.mid_db = has("EQ MidGain") ? gain_db("EQ MidGain") : gain_db("Mid Gain");
			p.mid_q = std::max(0.1f, par("EQ MidWidt", par("Mid Width", 10.0f)) / 10.0f);
			p.high_hz = par("EQHighFreq", par("High Freq", 6000.0f));
			p.high_db = has("EQHighGain") ? gain_db("EQHighGain") : gain_db("High Gain");
			p.three_band = has("EQ MidGain") || has("Mid Gain");
			m_eq.set_params(p);
			break;
		}
		case kind::wah: {
			wah_fx::params p;
			p.by_envelope = has("Sensitivty");
			p.rate_hz = par("LFO Freq", 1.0f);
			p.depth = clampf(raw_of("LFO Depth", 64) / 127.0f, 0.0f, 1.0f);
			p.sens = clampf(raw_of("Sensitivty", 64) / 127.0f, 0.0f, 1.0f);
			// CutoffFreq は揺れの真ん中。そこから下 1/2・上 4 倍まで振る
			const float center = clampf(par("CutoffFreq", 800.0f), 100.0f, 6000.0f);
			p.low_hz = center * 0.8f;
			p.high_hz = clampf(center * 5.0f, 500.0f, 14000.0f);
			p.resonance = clampf(par("Resonance", 30.0f) / 20.0f, 0.5f, 3.0f);
			p.dry_wet = wet_of(1.0f);
			m_wah.set_params(p);
			break;
		}
		case kind::dyn: {
			dyn_fx::params p;
			p.gate = (m_type >> 7) == 0x54;
			p.threshold_db = par("Threshold", par("ThreshLevel", -20.0f));
			p.ratio = std::max(1.0f, par("Ratio", 4.0f));
			p.attack_ms = par("Attack", par("AttackTime", 5.0f));
			p.release_ms = par("Release", par("RelesTime", 100.0f));
			p.out_level = clampf(raw_of("OutputLvl", 64) / 64.0f, 0.0f, 4.0f);
			m_dyn.set_params(p);
			break;
		}
		case kind::ring: {
			ring_fx::params p;
			// 掛ける波の高さ（粗いほうと細かいほう）
			// FreqCourse は表に Hz が入っている。Freq Fine は細かい足し引き
			p.freq_hz = clampf(par("FreqCourse", 400.0f) + par("Freq Fine", 0.0f), 10.0f, 8000.0f);
			p.depth = 1.0f;                       // 混ぜ具合は Dry/Wet のほうで決まる
			p.dry_wet = wet_of(1.0f);
			p.by_envelope = (m_type >> 7) == 0x70;
			p.sens = clampf(raw_of("Sensitivty", 64) / 127.0f, 0.0f, 1.0f);
			m_ring.set_params(p);
			break;
		}
		case kind::slice: {
			slice_fx::params p;
			// DivideType は 1 拍を何分割するか。テンポは分からないので 120 とみなす
			// DivideType は 1 拍の分け方。テンポは分からないので 120 とみなす
			p.rate_hz = clampf(2.0f * std::max(1.0f, raw_of("DivideType", 4)), 0.5f, 24.0f);
			p.duty = clampf(par("Gate Time", 50.0f) / 100.0f, 0.05f, 0.95f);
			p.depth = wet_of(1.0f);               // 切る深さは Dry/Wet に従う
			m_slice.set_params(p);
			break;
		}
		case kind::isolator: {
			isolator_fx::params p;
			p.low_hz = 200.0f;
			p.high_hz = 2000.0f;
			// Level は 0-127（64 で素通し）、Mute は 0/1
			p.low_gain  = raw_of("Low Mute", 0)  > 0 ? 0.0f : clampf(raw_of("Low Level", 64) / 64.0f, 0.0f, 2.0f);
			p.mid_gain  = raw_of("Mid Mute", 0)  > 0 ? 0.0f : clampf(raw_of("Mid Level", 64) / 64.0f, 0.0f, 2.0f);
			p.high_gain = raw_of("High Mute", 0) > 0 ? 0.0f : clampf(raw_of("HighLevel", 64) / 64.0f, 0.0f, 2.0f);
			m_iso.set_params(p);
			break;
		}
		case kind::reso: {
			reso_fx::params p;
			// LOW RESO は低いところだけを共振させて残す種類。実機の出音もほぼ低音だけ
			// 切る高さのパラメータは無い。実機は素通しに対して -11.7dB で、
			// 250Hz あたりから上が落ちる形だったので、そこに合わせる
			p.cutoff_hz = clampf(250.0f + raw_of("Resoltn", 0) * 10.0f, 100.0f, 600.0f);
			// 共振を強くすると 250Hz が持ち上がりすぎる。実機は山を作らずに、
			// 全体が小さくなる形（素通しに対して -11.7dB）だった
			p.resonance = clampf(0.7f + raw_of("Mod FB", 64) / 128.0f, 0.5f, 2.0f);
			p.level = 0.4f;
			p.dry_wet = wet_of(1.0f);
			m_reso.set_params(p);
			break;
		}
		case kind::cancel: {
			cancel_fx::params p;
			p.low_hz = par("CrsoverFrq", 120.0f);
			if (p.low_hz < 40.0f || p.low_hz > 1000.0f)
				p.low_hz = 120.0f;
			p.high_hz = 8000.0f;
			m_cancel.set_params(p);
			break;
		}
		case kind::enhancer: {
			enhancer_fx::params p;
			p.hpf_hz = par("HPF Cutoff", 2000.0f);
			p.drive = clampf(raw_of("Drive", 64) / 127.0f, 0.0f, 1.0f);
			p.mix = clampf(raw_of("Mix Level", 40) / 127.0f, 0.0f, 1.0f);
			m_enh.set_params(p);
			break;
		}
		case kind::pitch: {
			pitch_fx::params p;
			// Pitch は半音（64 が 0）、Fine はセント
			p.cents = (raw_of("Pitch", 64) - 64.0f) * 100.0f + (raw_of("Fine 1", 64) - 64.0f);
			p.dry_wet = wet_of(0.5f);
			m_pitch.set_params(p);
			break;
		}
		case kind::talk: {
			talk_fx::params p;
			p.vowel = clampf(raw_of("Vowel", 0) / 25.0f, 0.0f, 4.0f);
			p.rate_hz = clampf(par("Move Speed", 0.0f), 0.0f, 10.0f);
			p.drive = clampf(raw_of("Drive", 40) / 127.0f, 0.0f, 1.0f);
			p.dry_wet = wet_of(1.0f);
			m_talk.set_params(p);
			break;
		}
		case kind::chain: {
			// どれを通すかは種類で決まる（0x5f 歪み+ディレイ / 0x60 コンプ+歪み+ディレイ /
			// 0x61 ワウ+歪み+ディレイ）
			const int msb = m_type >> 7;
			m_chain_comp = msb == 0x60;
			m_chain_wah = msb == 0x61;
			m_chain_drive = true;
			m_chain_rotary = false;
			m_chain_delay = true;
			m_chain_delay_mix = clampf(raw_of("Delay Mix", 64) / 127.0f, 0.0f, 1.0f);
			// ワウを通す組（0x61）は、そのままだと 4.5dB 大きい
			m_chain_level = m_chain_wah ? 0.6f : 1.0f;

			dyn_fx::params c;
			c.threshold_db = par("Threshold", -20.0f);
			c.ratio = std::max(1.0f, par("Ratio", 4.0f));
			c.attack_ms = par("Attack", 5.0f);
			c.release_ms = par("Release", 100.0f);
			m_dyn.set_params(c);

			wah_fx::params w;
			w.by_envelope = true;
			w.sens = clampf(raw_of("Sensitivty", 64) / 127.0f, 0.0f, 1.0f);
			const float center = clampf(par("CutoffFreq", 800.0f), 100.0f, 6000.0f);
			w.low_hz = center * 0.8f;
			w.high_hz = clampf(center * 5.0f, 500.0f, 14000.0f);
			w.resonance = clampf(par("Resonance", 30.0f) / 20.0f, 0.5f, 3.0f);
			m_wah.set_params(w);

			drive_fx::params d;
			d.drive = clampf(raw_of("Dist Drive", 60) / 127.0f, 0.0f, 1.0f);
			d.edge = 0.5f;
			d.out_level = clampf(raw_of("DistOutLvl", 64) / 49.0f, 0.0f, 2.6f);
			d.lpf_hz = par("LPF Cutoff", 4000.0f);
			d.eq_low_db = gain_db("DT LowGain");
			d.eq_mid_db = gain_db("DT MidGain");
			d.dry_wet = 1.0f;
			m_drv.set_params(d);

			delay_fx::params dl;
			dl.l_ms = par("LchDelay", par("Delay", 300.0f));
			dl.r_ms = par("RchDelay", dl.l_ms);
			dl.c_ms = dl.l_ms;
			dl.fb_ms = par("FB Delay", dl.l_ms);
			dl.feedback = clampf((raw_of("FB Level", raw_of("DelayFBLvl", 64)) / 64.0f - 1.0f) * 1.2f, -0.9f, 0.9f);
			dl.c_level = 0.0f;
			m_dly.set_params(dl);
			break;
		}
		case kind::lofi: {
			lofi_fx::params p;
			p.bits = clampf(par("WordLength", par("Bit Assign", 8.0f)), 1.0f, 16.0f);
			// SmplFreq は落とし先の周波数（44.1k など）。何サンプルに 1 回にするかへ直す
			const float hz = par("SmplFreq", 11025.0f);
			p.rate_div = hz > 100.0f ? clampf(44100.0f / hz, 1.0f, 64.0f) : std::max(1.0f, hz);
			p.noise = clampf(raw_of("NoiseLevel", 0) / 127.0f, 0.0f, 0.2f);
			p.lpf_hz = par("LPF Cutoff", 8000.0f);
			m_lofi.set_params(p);
			break;
		}
		default:
			break;
		}
	}

	static constexpr int MAX_PAR = 16;

	// 入り口が黙ってから回し続けるサンプル数（44100Hz で 4 秒ぶん。いちばん長い尾より長く）
	static constexpr int QUIET_LIMIT = 44100 * 4;
	int m_quiet = 0;

	kind m_kind = kind::none;
	int  m_type = 0;
	int  m_raw[MAX_PAR] = {};
	const xg::fx_def *m_def = nullptr;
	float m_rate = 44100.0f;

	bool  m_insertion = false;
	float m_dry_mix = 0.0f;       // インサーションのとき、足す乾いた音の量
	bool  m_dry_full = true;      // 乾いた音を Dry/Wet で下げないか

	// 組み合わせの種類で、どれを通すか
	bool m_chain_comp = false, m_chain_wah = false, m_chain_drive = true;
	bool m_chain_rotary = false, m_chain_delay = true;
	float m_chain_delay_mix = 0.5f;
	float m_chain_level = 1.0f;

	reverb    m_rev;
	early_ref m_er;
	delay_fx  m_dly;
	mod_fx    m_mod;
	rotary_fx m_rot;
	drive_fx  m_drv;
	eq_fx     m_eq;
	wah_fx    m_wah;
	dyn_fx    m_dyn;
	lofi_fx   m_lofi;
	ring_fx     m_ring;
	slice_fx    m_slice;
	isolator_fx m_iso;
	reso_fx     m_reso;
	cancel_fx   m_cancel;
	enhancer_fx m_enh;
	pitch_fx    m_pitch;
	talk_fx     m_talk;
};

// 7 つの口（リバーブ・コーラス・バリエーション・インサーション 1-4）をまとめたもの
class native_fx
{
public:
	enum slot_id { REVERB = 0, CHORUS = 1, VARIATION = 2, INS1 = 3, INS2 = 4, INS3 = 5, INS4 = 6, SLOTS = 7 };

	void set_rate(float rate)
	{
		for (auto &s : m_slot)
			s.set_rate(rate);
		m_meq.set_rate(rate);
	}

	master_eq &meq() { return m_meq; }

	void set(slot_id id, int type, const int *raw, int count)
	{
		m_slot[id].set_insertion(id >= INS1);
		m_slot[id].set(type, raw, count);
	}
	void reset() { for (auto &s : m_slot) s.reset(); }

	fx_slot &slot(slot_id id) { return m_slot[id]; }

	void process(slot_id id, float l, float r, float &ol, float &orr)
	{
		// インサーションは音がそこを通るので、種類が無い・分からないときは素通し。
		// 送り（リバーブなど）は、種類が無ければ何も出さない
		if (id >= INS1 && m_slot[id].current() == fx_slot::kind::none) {
			ol = l;
			orr = r;
			return;
		}
		m_slot[id].process(l, r, ol, orr);
	}

	// 送りに対する戻りの量（リバーブ・コーラス・バリエーション）
	void set_return(slot_id id, float gain) { m_return[id] = gain; }
	float ret(slot_id id) const { return m_return[id]; }

private:
	fx_slot   m_slot[SLOTS];
	master_eq m_meq;
	// 送りに対する戻りの量。インサーションは、送りの目盛りが乾いた音と違うので実測で合わせた
	// （THRU を掛けて、MEG のときと同じ大きさになる値。doc/native-dsp.md）
	float   m_return[SLOTS] = { 0.6f, 0.6f, 0.6f, 0.31f, 0.31f, 0.31f, 0.31f };
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_FX_NATIVE_H
