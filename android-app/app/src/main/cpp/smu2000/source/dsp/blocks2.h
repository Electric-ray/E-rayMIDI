// license:BSD-3-Clause
//
// C++ で書いたエフェクトの続き（軽量モード。doc/native-dsp.md）。
// blocks.h が系統ごとの作りなのに対して、こちらは**種類ごとに形が決まっているもの**を置く。
//
//   ring_fx      リングモジュレータ（RING MOD・DYNA RING）
//   slice_fx     刻んで切る（SLICE）
//   isolator_fx  3 つの帯を別々に上げ下げ・消す（ISOLATOR）
//   reso_fx      共振するローパス（LOW RESO）
//   cancel_fx    真ん中の音を消す（VOIC CANCL・KARAOKE）
//   enhancer_fx  倍音を足す（HM ENHNCER）
//   pitch_fx     音程を変える（PITCH CNG。遅延を読み替える簡単な作り）
//   talk_fx      しゃべるような響き（TALK MOD。2 つの山で母音を作る）
//
// どれも **実機の再現ではない**。掛かり方を似せた別物。

#ifndef S_MU2000_DSP_BLOCKS2_H
#define S_MU2000_DSP_BLOCKS2_H

#pragma once

#include "blocks.h"

namespace smu2000::dsp {

// ---- リングモジュレータ --------------------------------------------------------
class ring_fx
{
public:
	struct params {
		float freq_hz = 400.0f;   // 掛ける波の高さ
		float depth = 1.0f;       // 0 で素通し、1 で全部
		float dry_wet = 1.0f;
		bool  by_envelope = false; // DYNA RING: 音量で高さが動く
		float sens = 0.5f;
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
		m_osc.set(m_p.freq_hz, m_rate);
	}

	void reset()
	{
		m_osc.clear();
		m_env.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		if (m_p.by_envelope) {
			const float e = clampf(m_env.process((l + r) * 0.5f) * (1.0f + m_p.sens * 8.0f), 0.0f, 1.0f);
			m_osc.set(m_p.freq_hz * (0.5f + 1.5f * e), m_rate);
		}
		m_osc.advance();
		const float c = m_osc.sine();
		const float wl = l * c, wr = r * c;
		const float d = m_p.depth;
		ol = ((wl * d + l * (1.0f - d)) * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
		orr = ((wr * d + r * (1.0f - d)) * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	lfo m_osc;
	follower m_env;
};

// ---- 刻んで切る（SLICE） -------------------------------------------------------
class slice_fx
{
public:
	struct params {
		float rate_hz = 4.0f;     // 1 秒に何回切るか
		float duty = 0.5f;        // 鳴らしている割合
		float depth = 1.0f;       // 1 で完全に切る
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
		m_lfo.set(m_p.rate_hz, m_rate);
		m_smooth.set(400.0f, m_rate);      // 切り替えの角を丸める
	}

	void reset()
	{
		m_lfo.clear();
		m_smooth.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		m_lfo.advance();
		const float open = m_lfo.phase() < clampf(m_p.duty, 0.05f, 0.95f) ? 1.0f : 0.0f;
		const float g = 1.0f - m_p.depth + m_p.depth * m_smooth.lp(open);
		ol = l * g * m_p.level;
		orr = r * g * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	lfo m_lfo;
	one_pole m_smooth;
};

// ---- アイソレータ（3 つの帯を別々に） -------------------------------------------
class isolator_fx
{
public:
	struct params {
		float low_hz = 200.0f, high_hz = 2000.0f;   // 分ける境目
		float low_gain = 1.0f, mid_gain = 1.0f, high_gain = 1.0f;
		float level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		for (int ch = 0; ch < 2; ch++) {
			m_lo[ch].set(m_p.low_hz, m_rate);
			m_hi[ch].set(m_p.high_hz, m_rate);
		}
	}

	void reset()
	{
		for (int ch = 0; ch < 2; ch++) { m_lo[ch].clear(); m_hi[ch].clear(); }
	}

	void process(float l, float r, float &ol, float &orr)
	{
		ol = split(0, l);
		orr = split(1, r);
	}

private:
	float split(int ch, float x)
	{
		const float low = m_lo[ch].lp(x);
		const float high = m_hi[ch].hp(x);
		const float mid = x - low - high;
		return (low * m_p.low_gain + mid * m_p.mid_gain + high * m_p.high_gain) * m_p.level;
	}

	params m_p;
	float m_rate = 44100.0f;
	one_pole m_lo[2], m_hi[2];
};

// ---- 共振するローパス（LOW RESO） ----------------------------------------------
class reso_fx
{
public:
	struct params {
		float cutoff_hz = 400.0f;
		float resonance = 6.0f;
		float dry_wet = 1.0f;
		float level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		for (int ch = 0; ch < 2; ch++)
			m_f[ch].low_pass(m_p.cutoff_hz, m_p.resonance, m_rate);
	}

	void reset() { m_f[0].clear(); m_f[1].clear(); }

	void process(float l, float r, float &ol, float &orr)
	{
		const float wl = m_f[0].process(l), wr = m_f[1].process(r);
		ol = (wl * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
		orr = (wr * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	biquad m_f[2];
};

// ---- 真ん中の音を消す（VOIC CANCL・KARAOKE） -----------------------------------
class cancel_fx
{
public:
	struct params {
		float low_hz = 120.0f;     // ここより下は消さない（ベースが抜けないように）
		float high_hz = 8000.0f;   // ここより上も消さない
		float level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		m_lo.set(m_p.low_hz, m_rate);
		m_hi.set(m_p.high_hz, m_rate);
	}

	void reset() { m_lo.clear(); m_hi.clear(); }

	void process(float l, float r, float &ol, float &orr)
	{
		const float mid = (l + r) * 0.5f;
		const float keep = m_lo.lp(mid) + m_hi.hp(mid);   // 低いところと高いところは残す
		const float side = (l - r) * 0.5f;
		ol = (side + keep) * m_p.level;
		orr = (-side + keep) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	one_pole m_lo, m_hi;
};

// ---- 倍音を足す（HM ENHNCER） --------------------------------------------------
class enhancer_fx
{
public:
	struct params {
		float hpf_hz = 2000.0f;   // ここより上から倍音を作る
		float drive = 0.5f;
		float mix = 0.3f;
		float level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		m_hp[0].set(m_p.hpf_hz, m_rate);
		m_hp[1].set(m_p.hpf_hz, m_rate);
	}

	void reset() { m_hp[0].clear(); m_hp[1].clear(); }

	void process(float l, float r, float &ol, float &orr)
	{
		const float g = 1.0f + m_p.drive * 12.0f;
		const float el = soft_clip(m_hp[0].hp(l) * g) * m_p.mix;
		const float er = soft_clip(m_hp[1].hp(r) * g) * m_p.mix;
		ol = (l + el) * m_p.level;
		orr = (r + er) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	one_pole m_hp[2];
};

// ---- 音程を変える（PITCH CNG） --------------------------------------------------
//
// 遅延の線を、少しずつずれた速さで 2 か所から読み、行き止まりで入れ替えながら混ぜる
// （いわゆるクロスフェードの音程変え）。荒い作りだが、掛かり方は似る
class pitch_fx
{
public:
	struct params {
		float cents = 0.0f;       // 変える量（セント）
		float dry_wet = 0.5f;
		float level = 1.0f;
	};

	void set_rate(float rate)
	{
		m_rate = rate;
		m_line[0].resize(size_t(rate * 0.1f));
		m_line[1].resize(size_t(rate * 0.1f));
		set_params(m_p);
		reset();
	}

	void set_params(const params &p)
	{
		m_p = p;
		m_ratio = std::pow(2.0f, clampf(m_p.cents, -1200.0f, 1200.0f) / 1200.0f);
		m_win = m_rate * 0.05f;               // 50ms の窓
	}

	void reset()
	{
		m_line[0].clear();
		m_line[1].clear();
		m_phase = 0.0f;
	}

	void process(float l, float r, float &ol, float &orr)
	{
		m_line[0].push(l);
		m_line[1].push(r);
		// 読み出し位置は、1 サンプルにつき (1 - ratio) ずつずれる
		m_phase += (1.0f - m_ratio);
		if (m_phase < 0.0f)
			m_phase += m_win;
		if (m_phase >= m_win)
			m_phase -= m_win;
		const float d1 = m_phase;
		const float d2 = m_phase + m_win * 0.5f >= m_win ? m_phase - m_win * 0.5f : m_phase + m_win * 0.5f;
		// 窓の端では音量を落として、継ぎ目を目立たなくする
		const float g1 = 0.5f - 0.5f * std::cos(2.0f * PI_F * d1 / m_win);
		const float g2 = 0.5f - 0.5f * std::cos(2.0f * PI_F * d2 / m_win);
		const float wl = m_line[0].tapf(d1) * g1 + m_line[0].tapf(d2) * g2;
		const float wr = m_line[1].tapf(d1) * g1 + m_line[1].tapf(d2) * g2;
		ol = (wl * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
		orr = (wr * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	delay_line m_line[2];
	float m_ratio = 1.0f, m_win = 2205.0f, m_phase = 0.0f;
};

// ---- しゃべるような響き（TALK MOD） ---------------------------------------------
class talk_fx
{
public:
	struct params {
		float vowel = 0.0f;        // 0:a 1:i 2:u 3:e 4:o のあいだを動く
		float rate_hz = 0.0f;      // 0 より大きいと自分で動く
		float drive = 0.3f;
		float dry_wet = 1.0f;
		float level = 1.0f;
	};

	void set_rate(float rate) { m_rate = rate; set_params(m_p); reset(); }

	void set_params(const params &p)
	{
		m_p = p;
		m_lfo.set(m_p.rate_hz, m_rate);
	}

	void reset()
	{
		m_lfo.clear();
		for (auto &f : m_f)
			f.clear();
	}

	void process(float l, float r, float &ol, float &orr)
	{
		float v = m_p.vowel;
		if (m_p.rate_hz > 0.0f) {
			m_lfo.advance();
			v = 2.0f + 2.0f * m_lfo.sine();
		}
		// 母音ごとの 2 つの山（だいたいの値）
		static const float F1[5] = { 800.0f, 350.0f, 350.0f, 550.0f, 450.0f };
		static const float F2[5] = { 1200.0f, 2300.0f, 900.0f, 1800.0f, 800.0f };
		const int i = int(clampf(v, 0.0f, 4.0f));
		const int j = std::min(4, i + 1);
		const float t = clampf(v, 0.0f, 4.0f) - float(i);
		const float f1 = F1[i] * (1.0f - t) + F1[j] * t;
		const float f2 = F2[i] * (1.0f - t) + F2[j] * t;
		m_f[0].band_pass(f1, 4.0f, m_rate);
		m_f[1].band_pass(f2, 6.0f, m_rate);
		const float in = (l + r) * 0.5f * (1.0f + m_p.drive * 4.0f);
		const float w = (m_f[0].process(in) * 1.2f + m_f[1].process(in) * 0.8f);
		ol = (w * m_p.dry_wet + l * (1.0f - m_p.dry_wet)) * m_p.level;
		orr = (w * m_p.dry_wet + r * (1.0f - m_p.dry_wet)) * m_p.level;
	}

private:
	params m_p;
	float m_rate = 44100.0f;
	lfo m_lfo;
	biquad m_f[2];
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_BLOCKS2_H
