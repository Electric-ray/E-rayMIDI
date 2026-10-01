// license:BSD-3-Clause
//
// C++ で書くエフェクト（軽量モード。doc/native-dsp.md）の土台。
// 遅延の線、1 次のフィルタ、双 2 次のフィルタ、揺らし（LFO）、歪ませ方など、
// どのエフェクトでも使う小物だけを置く。全部 float で、1 サンプルずつ処理する。
//
// **実機（MEG）の再現ではない。** 掛かり方を似せた別物で、正しさが要るときは MEG を回す。

#ifndef S_MU2000_DSP_UTIL_H
#define S_MU2000_DSP_UTIL_H

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace smu2000::dsp {

constexpr float PI_F = 3.14159265358979f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// XG の EQ の周波数の表（値 0-60 → Hz）。ui/eq_curve.h と同じもの
inline float xg_eq_hz(int index)
{
	static const int HZ[61] = {
		20, 22, 25, 28, 32, 36, 40, 45, 50, 56, 63, 70, 80, 90, 100, 110, 125, 140, 160, 180,
		200, 225, 250, 280, 315, 355, 400, 450, 500, 560, 630, 700, 800, 900, 1000, 1100, 1200, 1400, 1600, 1800,
		2000, 2200, 2500, 2800, 3200, 3600, 4000, 4500, 5000, 5600, 6300, 7000, 8000, 9000, 10000, 11000, 12000, 14000, 16000, 18000,
		20000 };
	return float(HZ[index < 0 ? 0 : (index > 60 ? 60 : index)]);
}

// dB を倍率に
inline float db_to_lin(float db) { return std::pow(10.0f, db / 20.0f); }

// ---- 遅延の線（読み出しは小数点つき。揺らすときに要る）
class delay_line
{
public:
	void resize(size_t n)
	{
		m_buf.assign(std::max<size_t>(n, 4), 0.0f);
		m_pos = 0;
	}
	void clear() { std::fill(m_buf.begin(), m_buf.end(), 0.0f); }
	size_t size() const { return m_buf.size(); }

	void push(float v)
	{
		m_buf[m_pos] = v;
		if (++m_pos >= m_buf.size())
			m_pos = 0;
	}

	// d サンプル前（整数）
	float tap(size_t d) const
	{
		d = std::min(d, m_buf.size() - 1);
		const size_t at = m_pos >= d + 1 ? m_pos - d - 1 : m_pos + m_buf.size() - d - 1;
		return m_buf[at];
	}

	// d サンプル前（小数。線形に混ぜる）
	float tapf(float d) const
	{
		d = clampf(d, 0.0f, float(m_buf.size() - 2));
		const size_t i = size_t(d);
		const float f = d - float(i);
		return m_buf_at(i) * (1.0f - f) + m_buf_at(i + 1) * f;
	}

private:
	float m_buf_at(size_t d) const
	{
		const size_t at = m_pos >= d + 1 ? m_pos - d - 1 : m_pos + m_buf.size() - d - 1;
		return m_buf[at];
	}
	std::vector<float> m_buf;
	size_t m_pos = 0;
};

// ---- 1 次のローパス／ハイパス
class one_pole
{
public:
	void set(float hz, float rate)
	{
		m_a = 1.0f - std::exp(-2.0f * PI_F * clampf(hz, 1.0f, rate * 0.49f) / rate);
	}
	void clear() { m_z = 0.0f; }
	float lp(float x) { m_z += m_a * (x - m_z); return m_z; }
	float hp(float x) { m_z += m_a * (x - m_z); return x - m_z; }

private:
	float m_a = 0.5f, m_z = 0.0f;
};

// ---- 双 2 次（RBJ の式）。ピーク・シェルフ・バンドパス・ローパス
class biquad
{
public:
	void clear() { m_x1 = m_x2 = m_y1 = m_y2 = 0.0f; }

	void peak(float hz, float q, float gain_db, float rate)
	{
		const float A = std::pow(10.0f, gain_db / 40.0f);
		const float w = 2.0f * PI_F * clampf(hz, 10.0f, rate * 0.49f) / rate;
		const float alpha = std::sin(w) / (2.0f * std::max(0.1f, q));
		set(1 + alpha * A, -2 * std::cos(w), 1 - alpha * A,
		    1 + alpha / A, -2 * std::cos(w), 1 - alpha / A);
	}

	void low_shelf(float hz, float gain_db, float rate)
	{
		const float A = std::pow(10.0f, gain_db / 40.0f);
		const float w = 2.0f * PI_F * clampf(hz, 10.0f, rate * 0.49f) / rate;
		const float c = std::cos(w), s = std::sin(w);
		const float beta = std::sqrt(A) / 0.707f;
		const float b0 = A * ((A + 1) - (A - 1) * c + beta * s);
		const float b1 = 2 * A * ((A - 1) - (A + 1) * c);
		const float b2 = A * ((A + 1) - (A - 1) * c - beta * s);
		const float a0 = (A + 1) + (A - 1) * c + beta * s;
		const float a1 = -2 * ((A - 1) + (A + 1) * c);
		const float a2 = (A + 1) + (A - 1) * c - beta * s;
		set(b0, b1, b2, a0, a1, a2);
	}

	void high_shelf(float hz, float gain_db, float rate)
	{
		const float A = std::pow(10.0f, gain_db / 40.0f);
		const float w = 2.0f * PI_F * clampf(hz, 10.0f, rate * 0.49f) / rate;
		const float c = std::cos(w), s = std::sin(w);
		const float beta = std::sqrt(A) / 0.707f;
		const float b0 = A * ((A + 1) + (A - 1) * c + beta * s);
		const float b1 = -2 * A * ((A - 1) + (A + 1) * c);
		const float b2 = A * ((A + 1) + (A - 1) * c - beta * s);
		const float a0 = (A + 1) - (A - 1) * c + beta * s;
		const float a1 = 2 * ((A - 1) - (A + 1) * c);
		const float a2 = (A + 1) - (A - 1) * c - beta * s;
		set(b0, b1, b2, a0, a1, a2);
	}

	void low_pass(float hz, float q, float rate)
	{
		const float w = 2.0f * PI_F * clampf(hz, 10.0f, rate * 0.49f) / rate;
		const float alpha = std::sin(w) / (2.0f * std::max(0.1f, q));
		const float c = std::cos(w);
		set((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + alpha, -2 * c, 1 - alpha);
	}

	void band_pass(float hz, float q, float rate)
	{
		const float w = 2.0f * PI_F * clampf(hz, 10.0f, rate * 0.49f) / rate;
		const float alpha = std::sin(w) / (2.0f * std::max(0.1f, q));
		const float c = std::cos(w);
		set(alpha, 0.0f, -alpha, 1 + alpha, -2 * c, 1 - alpha);
	}

	float process(float x)
	{
		const float y = m_b0 * x + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
		m_x2 = m_x1; m_x1 = x;
		m_y2 = m_y1; m_y1 = y;
		return y;
	}

private:
	void set(float b0, float b1, float b2, float a0, float a1, float a2)
	{
		const float k = 1.0f / a0;
		m_b0 = b0 * k; m_b1 = b1 * k; m_b2 = b2 * k;
		m_a1 = a1 * k; m_a2 = a2 * k;
	}
	float m_b0 = 1, m_b1 = 0, m_b2 = 0, m_a1 = 0, m_a2 = 0;
	float m_x1 = 0, m_x2 = 0, m_y1 = 0, m_y2 = 0;
};

// ---- 揺らし（LFO）。正弦と三角
class lfo
{
public:
	void set(float hz, float rate) { m_step = clampf(hz, 0.0f, 100.0f) / rate; }
	void set_phase(float p) { m_phase = p - std::floor(p); }
	void clear() { m_phase = 0.0f; }

	void advance()
	{
		m_phase += m_step;
		if (m_phase >= 1.0f)
			m_phase -= 1.0f;
	}
	float sine() const { return sine_of(m_phase); }
	float sine_at(float offset) const { return sine_of(m_phase + offset); }

	// 0-1 の位相から正弦。表を線形につないで引く（std::sin は 1 サンプルに何度も呼ぶには重い）
	static float sine_of(float p)
	{
		static const std::vector<float> TAB = [] {
			std::vector<float> t(SINE_N + 1);
			for (int i = 0; i <= SINE_N; i++)
				t[size_t(i)] = std::sin(2.0f * PI_F * float(i) / float(SINE_N));
			return t;
		}();
		p -= std::floor(p);
		const float x = p * float(SINE_N);
		const int i = int(x);
		const float f = x - float(i);
		return TAB[size_t(i)] * (1.0f - f) + TAB[size_t(i + 1)] * f;
	}
	float phase() const { return m_phase; }

private:
	static constexpr int SINE_N = 1024;
	float m_phase = 0.0f, m_step = 0.0f;
};

// ---- 直流を抜く
class dc_block
{
public:
	void set(float rate) { m_r = 1.0f - 20.0f / rate; }
	void clear() { m_x1 = m_y1 = 0.0f; }
	float process(float x)
	{
		const float y = x - m_x1 + m_r * m_y1;
		m_x1 = x;
		m_y1 = y;
		return y;
	}

private:
	float m_r = 0.999f, m_x1 = 0.0f, m_y1 = 0.0f;
};

// ---- 歪ませ方
inline float soft_clip(float x)
{
	// tanh に近い形を、割り算 1 回で
	return x / (1.0f + std::fabs(x));
}

inline float hard_clip(float x, float lim = 1.0f)
{
	return clampf(x, -lim, lim);
}

// 包絡線を追う（コンプ・ワウ用）
class follower
{
public:
	void set(float attack_ms, float release_ms, float rate)
	{
		m_att = 1.0f - std::exp(-1.0f / (std::max(0.1f, attack_ms) * 0.001f * rate));
		m_rel = 1.0f - std::exp(-1.0f / (std::max(1.0f, release_ms) * 0.001f * rate));
	}
	void clear() { m_env = 0.0f; }
	float process(float x)
	{
		const float a = std::fabs(x);
		m_env += (a > m_env ? m_att : m_rel) * (a - m_env);
		return m_env;
	}
	float value() const { return m_env; }

private:
	float m_att = 0.01f, m_rel = 0.001f, m_env = 0.0f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_UTIL_H
