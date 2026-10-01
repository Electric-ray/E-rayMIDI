// license:BSD-3-Clause
//
// C++ で書いたエフェクトの中身（軽量モード。doc/native-dsp.md）。
// XG の種類ごとに、掛かり方を似せた作りにしてある。**実機（MEG）と同じ音にはならない。**
//
//   early_ref  初期反射・ゲートリバーブ
//   delay_fx   ディレイ（LCR・L,R・クロス・エコー）
//   mod_fx     コーラス・セレステ・フランジャー・シンフォニック・アンサンブル・フェイザー
//   rotary_fx  回転スピーカー・トレモロ・オートパン
//   drive_fx   歪み・オーバードライブ・アンプ
//   eq_fx      2/3 バンド EQ
//   wah_fx     オートワウ（揺らし／音量追従）
//   dyn_fx     コンプレッサ・ノイズゲート
//   lofi_fx    ローファイ（ビット落とし・間引き）
//
// どれも 1 サンプルずつ。入り口と出口はステレオの float（±1 くらい）。

#ifndef S_MU2000_DSP_BLOCKS_H
#define S_MU2000_DSP_BLOCKS_H

#pragma once

#include "util.h"

namespace smu2000::dsp {

// ---- 初期反射とゲートリバーブ -------------------------------------------------
class early_ref
{
public:
	struct params {
		float room     = 0.6f;    // 部屋の大きさ（反射の間隔）
		float liveness = 0.5f;    // 減り方（0 でぱたっと止まる＝ゲート）
		float time_ms  = 200.0f;  // 全体の長さ
		float diffuse  = 0.6f;
		bool  gate     = false;   // 途中で切る（GATE REVERB）
		bool  reverse  = false;   // だんだん大きくなる（REVERSE GATE）
		float level    = 1.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		m_line.resize(size_t(rate * 0.5f));
		m_line.clear();
		set_params(m_p);
	}

	void set_params(const params &p)
	{
		m_p = p;
		const float span = clampf(m_p.time_ms, 10.0f, 400.0f) * 0.001f;
		for (int i = 0; i < TAPS; i++) {
			// 反射は前ほど詰まって、あとほど散らばる
			const float u = float(i + 1) / float(TAPS);
			const float t = span * std::pow(u, 0.8f) * (0.6f + 0.8f * m_p.room);
			m_tap[i] = t * m_rate;
			float g = std::pow(0.15f + 0.85f * m_p.liveness, u * 4.0f);
			if (m_p.gate) {
				// 切るまでは減らさず、切り際は 2 つぶんかけて落とす。
				// 真っ二つに切ると、実機より尾が 12dB 小さくなっていた
				g = u < 0.85f ? 1.0f : clampf((1.0f - u) / 0.15f, 0.0f, 1.0f);
				g = g * g;
			}
			if (m_p.reverse)
				g = u * u;                      // だんだん大きく
			m_gain[i] = g * (i % 2 ? -1.0f : 1.0f) * (0.5f + 0.5f * m_p.diffuse);
		}
	}

	void reset() { m_line.clear(); }

	void process(float l, float r, float &ol, float &orr)
	{
		const float in = (l + r) * 0.5f;
		m_line.push(in);
		float a = 0.0f, b = 0.0f;
		for (int i = 0; i < TAPS; i++) {
			const float v = m_line.tapf(m_tap[i]) * m_gain[i];
			if (i & 1)
				b += v;
			else
				a += v;
		}
		ol = a * m_p.level;
		orr = b * m_p.level;
	}

private:
	static constexpr int TAPS = 12;
	params m_p;
	float m_rate = 44100.0f;
	delay_line m_line;
	float m_tap[TAPS] = {};
	float m_gain[TAPS] = {};
};

// ---- ディレイ ---------------------------------------------------------------
class delay_fx
{
public:
	struct params {
		float l_ms = 300.0f, r_ms = 400.0f, c_ms = 200.0f;  // 左・右・真ん中の遅れ
		float fb_ms = 350.0f;      // 戻す線の遅れ
		float feedback = 0.35f;    // 戻す量（-1..1。負なら逆相）
		float c_level = 0.8f;      // 真ん中の音量（LCR 用）
		float hpf_hz = 60.0f, lpf_hz = 8000.0f;
		bool  cross = false;       // 左右を入れ替えて戻す
		float level = 1.0f;
		// 2 本目の組（ECHO の LchDelay2/RchDelay2）。level2 が 0 なら使わない
		float l2_ms = 0.0f, r2_ms = 0.0f, level2 = 0.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		const size_t n = size_t(rate * 2.0f) + 4;     // 最大 2 秒
		m_l.resize(n);
		m_r.resize(n);
		m_l.clear();
		m_r.clear();
		set_params(m_p);
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_hp[0].set(m_p.hpf_hz, m_rate);
		m_hp[1].set(m_p.hpf_hz, m_rate);
		m_lp[0].set(m_p.lpf_hz, m_rate);
		m_lp[1].set(m_p.lpf_hz, m_rate);
	}

	void reset()
	{
		m_l.clear();
		m_r.clear();
		for (int i = 0; i < 2; i++) { m_hp[i].clear(); m_lp[i].clear(); }
	}

	void process(float l, float r, float &ol, float &orr)
	{
		const float dl = m_l.tapf(ms(m_p.l_ms));
		const float dr = m_r.tapf(ms(m_p.r_ms));
		const float dc = (m_l.tapf(ms(m_p.c_ms)) + m_r.tapf(ms(m_p.c_ms))) * 0.5f * m_p.c_level;
		const float fl = m_l.tapf(ms(m_p.fb_ms));
		const float fr = m_r.tapf(ms(m_p.fb_ms));

		// 戻すのは、出ている音（左右と真ん中）をまとめたもの。実機の LCR ディレイは
		// 尾がはっきり残るので、1 本だけ戻すと足りない
		// クロスディレイは左右を入れ替えて戻す。ほかは左右をまとめて戻す
		// （まとめないと、片側だけに音がある曲で尾が細くなる）
		const float mix = (fl + fr) * 0.5f + dc * 0.5f * (m_p.c_level > 0.0f ? 1.0f : 0.0f);
		float bl = m_lp[0].lp(m_hp[0].hp(m_p.cross ? fr : mix)) * m_p.feedback;
		float br = m_lp[1].lp(m_hp[1].hp(m_p.cross ? fl : mix)) * m_p.feedback;
		bl = clampf(bl, -4.0f, 4.0f);
		br = clampf(br, -4.0f, 4.0f);

		m_l.push(l + bl);
		m_r.push(r + br);

		float el = 0.0f, er = 0.0f;
		if (m_p.level2 > 0.0f) {
			el = m_l.tapf(ms(m_p.l2_ms)) * m_p.level2;
			er = m_r.tapf(ms(m_p.r2_ms)) * m_p.level2;
		}
		ol = (dl + dc + el) * m_p.level;
		orr = (dr + dc + er) * m_p.level;
	}

private:
	float ms(float v) const { return clampf(v, 0.1f, 1990.0f) * 0.001f * m_rate; }

	params m_p;
	float m_rate = 44100.0f;
	delay_line m_l, m_r;
	one_pole m_hp[2], m_lp[2];
};

// ---- コーラス・フランジャー・フェイザーなど ------------------------------------
class mod_fx
{
public:
	enum class kind { chorus, flanger, celeste, symphonic, phaser, ensemble };

	struct params {
		kind  type = kind::chorus;
		float rate_hz = 0.5f;
		float depth = 0.3f;        // 0-1
		float delay_ms = 12.0f;    // 真ん中の遅れ
		float feedback = 0.0f;     // フランジャー用（-1..1）
		float phase_deg = 90.0f;   // 左右のずれ
		float stages = 6.0f;       // フェイザーの段数
		float dry_wet = 0.5f;      // 1 で揺れた音だけ
		float level = 1.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		m_l.resize(size_t(rate * 0.05f) + 4);       // 最大 50ms
		m_r.resize(size_t(rate * 0.05f) + 4);
		m_l.clear();
		m_r.clear();
		set_params(m_p);
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_lfo.set(m_p.rate_hz, m_rate);
	}

	void reset()
	{
		m_l.clear();
		m_r.clear();
		m_lfo.clear();
		for (auto &a : m_ap) { a.z = 0.0f; }
		m_fb_l = m_fb_r = 0.0f;
	}

	void process(float l, float r, float &ol, float &orr)
	{
		m_lfo.advance();
		const float ph = m_p.phase_deg / 360.0f;

		if (m_p.type == kind::phaser) {
			// オールパスを重ねて、位相をずらしたものと足す
			const int n = std::clamp(int(m_p.stages), 2, 12);
			const float mod = 0.5f + 0.5f * m_lfo.sine();
			const float f = 200.0f * std::pow(40.0f, mod * clampf(m_p.depth, 0.05f, 1.0f));
			const float w = std::tan(PI_F * clampf(f, 20.0f, m_rate * 0.45f) / m_rate);
			const float a = (w - 1.0f) / (w + 1.0f);
			float xl = l + m_fb_l * m_p.feedback;
			float xr = r + m_fb_r * m_p.feedback;
			for (int i = 0; i < n; i++) {
				xl = ap(m_ap[i * 2], a, xl);
				xr = ap(m_ap[i * 2 + 1], a, xr);
			}
			m_fb_l = xl;
			m_fb_r = xr;
			ol = (l + xl) * 0.5f * m_p.level;
			orr = (r + xr) * 0.5f * m_p.level;
			return;
		}

		const float base = clampf(m_p.delay_ms, 0.2f, 40.0f) * 0.001f * m_rate;
		const float swing = base * clampf(m_p.depth, 0.0f, 1.0f) * 0.9f;

		float dl = base + swing * m_lfo.sine();
		float dr = base + swing * m_lfo.sine_at(ph);
		if (m_p.type == kind::ensemble) {
			// 3 本を少しずつずらして重ねる（アンサンブル・シンフォニック）
			const float d2 = base * 1.3f + swing * m_lfo.sine_at(0.33f);
			const float d3 = base * 0.7f + swing * m_lfo.sine_at(0.66f);
			m_l.push(l);
			m_r.push(r);
			const float a = (m_l.tapf(dl) + m_l.tapf(d2) + m_l.tapf(d3)) / 3.0f;
			const float b = (m_r.tapf(dr) + m_r.tapf(d2) + m_r.tapf(d3)) / 3.0f;
			ol = (a * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
			orr = (b * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
			return;
		}

		const float wl = m_l.tapf(dl);
		const float wr = m_r.tapf(dr);
		m_l.push(clampf(l + wl * m_p.feedback, -4.0f, 4.0f));
		m_r.push(clampf(r + wr * m_p.feedback, -4.0f, 4.0f));
		ol = (wl * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
		orr = (wr * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
	}

private:
	struct ap_state { float z = 0.0f; };
	static float ap(ap_state &s, float a, float x)
	{
		const float y = a * x + s.z;
		s.z = x - a * y;
		return y;
	}

	params m_p;
	float m_rate = 44100.0f;
	delay_line m_l, m_r;
	lfo m_lfo;
	ap_state m_ap[24];
	float m_fb_l = 0.0f, m_fb_r = 0.0f;
};

// ---- 回転スピーカー・トレモロ・オートパン ---------------------------------------
class rotary_fx
{
public:
	enum class kind { rotary, tremolo, auto_pan };

	struct params {
		kind  type = kind::rotary;
		float speed_hz = 5.0f;
		float depth = 0.7f;       // 0-1
		float drive = 0.0f;       // 回転スピーカーの歪み
		float level = 1.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		m_line.resize(size_t(rate * 0.01f) + 4);
		m_line.clear();
		m_split.set(800.0f, rate);
		set_params(m_p);
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_lfo.set(m_p.speed_hz, m_rate);
		m_lfo_bass.set(m_p.speed_hz * 0.8f, m_rate);
	}

	void reset()
	{
		m_line.clear();
		m_lfo.clear();
		m_lfo_bass.clear();
		m_split.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		m_lfo.advance();
		m_lfo_bass.advance();
		const float d = clampf(m_p.depth, 0.0f, 1.0f);

		if (m_p.type == kind::tremolo) {
			const float g = 1.0f - d * 0.35f * (1.0f - m_lfo.sine());
			ol = l * g * m_p.level;
			orr = r * g * m_p.level;
			return;
		}
		if (m_p.type == kind::auto_pan) {
			const float s = m_lfo.sine() * d;
			ol = l * (0.5f + 0.5f * (1.0f - s)) * m_p.level;
			orr = r * (0.5f + 0.5f * (1.0f + s)) * m_p.level;
			return;
		}

		// 回転スピーカー: 高い音（ホーン）と低い音（ローター）を別の速さで回し、
		// 音量と遅れ（ドップラー）を揺らす
		const float in = (l + r) * 0.5f;
		const float bass = m_split.lp(in);
		const float horn = in - bass;
		m_line.push(horn);

		const float sw = m_lfo.sine(), sw2 = m_lfo.sine_at(0.25f);
		const float dop = (0.6f + 0.4f * d) * (1.0f + sw) * 0.0015f * m_rate;
		const float hl = m_line.tapf(dop) * (1.0f - d * 0.45f * (1.0f - sw));
		const float hr = m_line.tapf(dop * 0.7f) * (1.0f - d * 0.45f * (1.0f + sw));
		const float bl = bass * (1.0f - d * 0.25f * (1.0f - sw2));
		const float br = bass * (1.0f - d * 0.25f * (1.0f + sw2));

		float a = hl + bl, b = hr + br;
		if (m_p.drive > 0.01f) {
			const float g = 1.0f + m_p.drive * 8.0f;
			a = soft_clip(a * g) * (1.0f / (1.0f + m_p.drive * 2.0f));
			b = soft_clip(b * g) * (1.0f / (1.0f + m_p.drive * 2.0f));
		}
		ol = a * m_p.level * 1.7f;      // 分けたぶんの戻し（実機と rms を合わせた）
		orr = b * m_p.level * 1.7f;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	delay_line m_line;
	lfo m_lfo, m_lfo_bass;
	one_pole m_split;
};

// ---- 歪み・アンプ -------------------------------------------------------------
class drive_fx
{
public:
	struct params {
		float drive = 0.5f;        // 0-1
		float edge = 0.5f;         // 0 でまろやか、1 でざらつく
		float out_level = 0.5f;
		float lpf_hz = 4000.0f;    // キャビネットの丸め
		float eq_low_db = 0.0f, eq_mid_db = 0.0f;
		float eq_low_hz = 200.0f, eq_mid_hz = 1400.0f, eq_mid_q = 1.0f;
		float dry_wet = 1.0f;      // 1 で全部歪んだ音
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		set_params(m_p);
		reset();
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_low.low_shelf(clampf(m_p.eq_low_hz, 32.0f, 2000.0f), m_p.eq_low_db, m_rate);
		m_mid.peak(clampf(m_p.eq_mid_hz, 100.0f, 10000.0f), m_p.eq_mid_q, m_p.eq_mid_db, m_rate);
		m_cab.set(clampf(m_p.lpf_hz, 500.0f, 16000.0f), m_rate);
		m_dc.set(m_rate);
	}

	void reset()
	{
		m_low.clear();
		m_mid.clear();
		m_cab.clear();
		m_dc.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		const float in = (l + r) * 0.5f;
		const float d = clampf(m_p.drive, 0.0f, 1.0f);
		const float g = 1.0f + d * 60.0f;
		float x = m_mid.process(m_low.process(in)) * g;
		// edge が高いほど頭を角ばらせる
		const float soft = soft_clip(x);
		const float hard = hard_clip(x * 0.7f);
		x = soft * (1.0f - m_p.edge) + hard * m_p.edge;
		// 潰すと振幅が全振幅に張り付くので、持ち上げたぶんを戻す
		x *= 1.0f / (1.0f + d * 12.0f);
		x = m_dc.process(m_cab.lp(x));
		const float wet = x * m_p.out_level;
		ol = orr = wet * m_p.dry_wet + in * (1.0f - m_p.dry_wet);
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	biquad m_low, m_mid;
	one_pole m_cab;
	dc_block m_dc;
};

// ---- EQ ---------------------------------------------------------------------
class eq_fx
{
public:
	struct params {
		float low_hz = 200.0f, low_db = 0.0f;
		float mid_hz = 1000.0f, mid_db = 0.0f, mid_q = 1.0f;
		float high_hz = 6000.0f, high_db = 0.0f;
		bool  three_band = true;
		float level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		for (int ch = 0; ch < 2; ch++) {
			m_low[ch].low_shelf(m_p.low_hz, m_p.low_db, m_rate);
			m_mid[ch].peak(m_p.mid_hz, m_p.mid_q, m_p.mid_db, m_rate);
			m_high[ch].high_shelf(m_p.high_hz, m_p.high_db, m_rate);
		}
	}

	void reset()
	{
		for (int ch = 0; ch < 2; ch++) { m_low[ch].clear(); m_mid[ch].clear(); m_high[ch].clear(); }
	}

	void process(float l, float r, float &ol, float &orr)
	{
		float a = m_low[0].process(l), b = m_low[1].process(r);
		if (m_p.three_band) {
			a = m_mid[0].process(a);
			b = m_mid[1].process(b);
		}
		ol = m_high[0].process(a) * m_p.level;
		orr = m_high[1].process(b) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	biquad m_low[2], m_mid[2], m_high[2];
};

// ---- ワウ -------------------------------------------------------------------
class wah_fx
{
public:
	struct params {
		bool  by_envelope = true;  // 音量で動かす（false なら揺らしで動かす）
		float rate_hz = 1.0f;
		float depth = 0.6f;
		float sens = 0.5f;
		float low_hz = 300.0f, high_hz = 3000.0f;
		float resonance = 3.0f;
		float dry_wet = 1.0f;
		float level = 1.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		m_env.set(5.0f, 120.0f, rate);
		set_params(m_p);
		reset();
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_lfo.set(m_p.rate_hz, m_rate);
	}

	void reset()
	{
		m_lfo.clear();
		m_env.clear();
		for (auto &f : m_bp)
			f.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		const float in = (l + r) * 0.5f;
		float u;
		if (m_p.by_envelope) {
			const float e = m_env.process(in) * (1.0f + m_p.sens * 8.0f);
			u = clampf(e, 0.0f, 1.0f);
		} else {
			m_lfo.advance();
			u = 0.5f + 0.5f * m_lfo.sine() * clampf(m_p.depth, 0.0f, 1.0f);
		}
		const float f = m_p.low_hz * std::pow(std::max(1.01f, m_p.high_hz / m_p.low_hz), u);
		m_bp[0].band_pass(f, m_p.resonance, m_rate);
		m_bp[1] = m_bp[0];
		const float k = 0.6f + 0.7f * m_p.resonance;      // 帯だけ取り出すと痩せるぶん
		const float wl = m_bp[0].process(l) * k, wr = m_bp[1].process(r) * k;
		ol = (wl * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
		orr = (wr * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	lfo m_lfo;
	follower m_env;
	biquad m_bp[2];
};

// ---- コンプレッサ・ゲート -------------------------------------------------------
class dyn_fx
{
public:
	struct params {
		bool  gate = false;
		float threshold_db = -20.0f;
		float ratio = 4.0f;
		float attack_ms = 5.0f, release_ms = 100.0f;
		float out_level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		m_env.set(m_p.attack_ms, m_p.release_ms, m_rate);
		m_thresh = db_to_lin(m_p.threshold_db);
		// 潰したぶんの持ち上げ（全振幅のところで元に戻る量）
		// 実機と同じ大きさになるところを実測で選んだ（全部戻すと大きすぎる）
		m_makeup = m_p.gate ? 1.0f
		                    : clampf(std::pow(std::pow(std::max(1e-4f, m_thresh), 1.0f / std::max(1.0f, m_p.ratio) - 1.0f), 0.75f),
		                             1.0f, 10.0f);
	}

	void reset() { m_env.clear(); }

	void process(float l, float r, float &ol, float &orr)
	{
		const float e = m_env.process(std::max(std::fabs(l), std::fabs(r)));
		float g = 1.0f;
		if (m_p.gate) {
			g = e >= m_thresh ? 1.0f : e / std::max(1e-6f, m_thresh);
			g = g * g;                          // 下は急に落とす
		} else if (e > m_thresh && m_thresh > 0.0f) {
			const float over = e / m_thresh;
			g = std::pow(over, 1.0f / std::max(1.0f, m_p.ratio)) / over;
		}
		// 持ち上げは「入っている音の大きさ」に応じて掛ける。いつでも掛けると、
		// 音を離したあとの尾まで持ち上がって実機より 12dB 大きくなっていた
		const float boost = 1.0f + (m_makeup - 1.0f) * clampf(e / std::max(1e-6f, m_thresh), 0.0f, 1.0f);
		ol = l * g * m_p.out_level * boost;
		orr = r * g * m_p.out_level * boost;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	follower m_env;
	float m_thresh = 0.1f, m_makeup = 1.0f;
};

// ---- ローファイ ---------------------------------------------------------------
class lofi_fx
{
public:
	struct params {
		float bits = 8.0f;         // 落とし先のビット数
		float rate_div = 4.0f;     // 何サンプルに 1 回だけ更新するか
		float noise = 0.0f;        // 混ぜる雑音
		float lpf_hz = 8000.0f;
		float level = 1.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		set_params(m_p);
		reset();
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_lp[0].set(m_p.lpf_hz, m_rate);
		m_lp[1].set(m_p.lpf_hz, m_rate);
	}

	void reset()
	{
		m_count = 0;
		m_hold_l = m_hold_r = 0.0f;
		m_lp[0].clear();
		m_lp[1].clear();
		m_rand = 22222;
	}

	void process(float l, float r, float &ol, float &orr)
	{
		const int div = std::max(1, int(m_p.rate_div));
		if (m_count <= 0) {
			m_count = div;
			const float steps = std::pow(2.0f, clampf(m_p.bits, 1.0f, 16.0f) - 1.0f);
			m_hold_l = std::round(clampf(l, -1.0f, 1.0f) * steps) / steps;
			m_hold_r = std::round(clampf(r, -1.0f, 1.0f) * steps) / steps;
		}
		m_count--;
		float a = m_hold_l, b = m_hold_r;
		if (m_p.noise > 0.0f) {
			a += noise() * m_p.noise;
			b += noise() * m_p.noise;
		}
		ol = m_lp[0].lp(a) * m_p.level;
		orr = m_lp[1].lp(b) * m_p.level;
	}

private:
	float noise()
	{
		m_rand = m_rand * 1103515245u + 12345u;
		return float(int32_t(m_rand >> 8) & 0xffff) / 32768.0f - 1.0f;
	}

	params m_p;
	float m_rate = 44100.0f;
	one_pole m_lp[2];
	int m_count = 0;
	float m_hold_l = 0.0f, m_hold_r = 0.0f;
	uint32_t m_rand = 22222;
};

// ---- マスター EQ（5 帯。02 40 00-14） -------------------------------------------
class master_eq
{
public:
	void set_rate(float rate) { m_rate = rate; reset(); }

	// XG の生の値: gain 52-76（64 が 0dB）、freq は表の番号、q 1-120（7 が 1.0）、
	// shape は帯 1 と 5 だけ（0 シェルフ / 1 ピーク）
	void set_raw(const int gain[5], const int freq[5], const int q[5], int shape1, int shape5)
	{
		for (int i = 0; i < 5; i++) {
			const float db = clampf(float(gain[i] - 64), -12.0f, 12.0f);
			const float hz = xg_eq_hz(freq[i]);
			const float qq = std::max(0.1f, float(q[i]) / 7.0f);
			for (int ch = 0; ch < 2; ch++) {
				if (i == 0 && !shape1)
					m_f[i][ch].low_shelf(hz, db, m_rate);
				else if (i == 4 && !shape5)
					m_f[i][ch].high_shelf(hz, db, m_rate);
				else
					m_f[i][ch].peak(hz, qq, db, m_rate);
			}
			m_flat[i] = std::fabs(db) < 0.05f;
		}
	}

	void reset()
	{
		for (auto &band : m_f)
			for (auto &f : band)
				f.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		for (int i = 0; i < 5; i++) {
			if (m_flat[i])
				continue;
			l = m_f[i][0].process(l);
			r = m_f[i][1].process(r);
		}
		ol = l;
		orr = r;
	}

private:
	float m_rate = 44100.0f;
	biquad m_f[5][2];
	bool m_flat[5] = { true, true, true, true, true };
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_BLOCKS_H
