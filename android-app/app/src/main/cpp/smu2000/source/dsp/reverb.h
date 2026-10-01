// license:BSD-3-Clause
//
// C++ で書いたリバーブ（軽量モードの試作。doc/native-dsp.md）。
//
// **これは MEG（実機のエフェクト DSP）の置き換えではなく、別物の近似**。
// 音の正しさが要るときは今までどおり MEG を回す。こちらは「実機の再現より軽さが欲しい」
// 場面のための選択肢で、掛かり方は似せてあるが同じ音にはならない。
//
// 作り: 前遅れ → 初期反射（4 点） → 8 本の遅延を行列で混ぜる網（FDN）。
// 網の各段に 1 次のローパス（高い音ほど早く減る）と 1 次のハイパス（低い音の溜まりを抜く）。
// 行列は Householder（全部の遅延を等しく混ぜる）で、掛け算は足し算と引き算だけで済む。
//
//   rv.set_rate(44100);
//   rv.set_params(p);           // p は下の params
//   rv.process(l, r, ol, or);   // 1 サンプルずつ
//
// 係数の意味は XG のリバーブのパラメータに寄せてある（time が RevTime など）。
// 実機と突き合わせた結果は doc/native-dsp.md。

#ifndef S_MU2000_DSP_REVERB_H
#define S_MU2000_DSP_REVERB_H

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace smu2000::dsp {

class reverb
{
public:
	struct params {
		float time        = 2.0f;    // 秒。-60dB まで落ちるまでの長さ（RevTime）
		float predelay_ms = 20.0f;   // 前遅れ（InitialDelay）
		float damp_hz     = 6000.0f; // これより上を早く減らす（HighDamp / LPF）
		float hpf_hz      = 80.0f;   // これより下を抜く（HPF）
		float diffusion   = 0.7f;    // 反射の散らばり（0-1）
		float er_level    = 0.35f;   // 初期反射の量（Rev/Er Balance）
		float width       = 1.0f;    // 左右の広がり（0 で真ん中）
	};

	void set_rate(float rate)
	{
		m_rate = rate > 1.0f ? rate : 44100.0f;
		// 遅延の長さは互いに素に近い素数（同じ周期が重ならないように）。
		// 44100Hz を基準に決めてあるので、ほかの周波数では比で伸ばす
		static const int BASE[TAPS] = { 1687, 1801, 2113, 2437, 2689, 2903, 3217, 3491 };
		const float k = m_rate / 44100.0f;
		for (int i = 0; i < TAPS; i++) {
			const int n = std::max(16, int(BASE[i] * k));
			m_line[i].assign(size_t(n), 0.0f);
			m_pos[i] = 0;
		}
		m_pre.assign(size_t(m_rate * 0.25f) + 2, 0.0f);   // 前遅れは最大 250ms
		m_pre_pos = 0;
		reset();
		set_params(m_p);
	}

	void set_params(const params &p)
	{
		m_p = p;
		// 各遅延の 1 周ぶんの減衰。-60dB / time 秒
		for (int i = 0; i < TAPS; i++) {
			const float len_s = float(m_line[i].size()) / m_rate;
			// 網の中のフィルタ（高い音を減らす・低い音を抜く）でも減るので、
			// 狙いの長さを少し伸ばしておく（実機の RT60 に合わせた）
			const float t = std::max(0.05f, m_p.time) * 1.35f;
			m_gain[i] = std::pow(10.0f, -3.0f * len_s / t);   // 10^(-60dB/20 * len/time)
		}
		m_damp = coef_lp(std::clamp(m_p.damp_hz, 500.0f, m_rate * 0.45f));
		m_hpf  = coef_lp(std::clamp(m_p.hpf_hz, 20.0f, 1000.0f));
		m_pre_len = std::min(m_pre.size() - 1,
		                     size_t(m_rate * std::clamp(m_p.predelay_ms, 0.0f, 200.0f) * 0.001f));
		m_diff = std::clamp(m_p.diffusion, 0.0f, 0.95f);
	}

	void reset()
	{
		for (auto &l : m_line)
			std::fill(l.begin(), l.end(), 0.0f);
		std::fill(m_pre.begin(), m_pre.end(), 0.0f);
		std::fill(m_lp.begin(), m_lp.end(), 0.0f);
		std::fill(m_hp.begin(), m_hp.end(), 0.0f);
		for (auto &a : m_ap)
			std::fill(a.begin(), a.end(), 0.0f);
		std::fill(m_ap_pos.begin(), m_ap_pos.end(), 0);
	}

	// 1 サンプル。入り口は送り（モノラルでもよい。その場合は同じ値を 2 つ渡す）
	void process(float in_l, float in_r, float &out_l, float &out_r)
	{
		const float in = (in_l + in_r) * 0.5f;

		// ---- 前遅れ
		m_pre[m_pre_pos] = in;
		size_t rd = m_pre_pos >= m_pre_len ? m_pre_pos - m_pre_len : m_pre_pos + m_pre.size() - m_pre_len;
		const float pre = m_pre[rd];
		if (++m_pre_pos >= m_pre.size())
			m_pre_pos = 0;

		// ---- 初期反射（前遅れの線から 4 点つまむ）
		float er = 0.0f;
		static const float ER_T[4] = { 0.0071f, 0.0117f, 0.0191f, 0.0269f };  // 秒
		static const float ER_G[4] = { 0.70f, 0.52f, 0.38f, 0.28f };
		for (int i = 0; i < 4; i++) {
			const size_t d = std::min(m_pre.size() - 1, size_t(m_rate * ER_T[i]));
			const size_t at = m_pre_pos >= d ? m_pre_pos - d : m_pre_pos + m_pre.size() - d;
			er += m_pre[at] * ER_G[i];
		}

		// ---- 散らす（オールパス 2 段）
		float x = pre;
		for (int i = 0; i < AP; i++)
			x = allpass(i, x);

		// ---- 網（FDN）。遅延から読み、混ぜて、書き戻す
		float v[TAPS];
		for (int i = 0; i < TAPS; i++)
			v[i] = m_line[i][m_pos[i]];

		// Householder: y = v - (2/N) * sum(v)
		float sum = 0.0f;
		for (int i = 0; i < TAPS; i++)
			sum += v[i];
		const float corr = sum * (2.0f / float(TAPS));

		for (int i = 0; i < TAPS; i++) {
			float y = (v[i] - corr) * m_gain[i] + x * 0.4f;
			// 高い音を早く減らす
			m_lp[i] += m_damp * (y - m_lp[i]);
			y = m_lp[i];
			// 低い音の溜まりを抜く
			m_hp[i] += m_hpf * (y - m_hp[i]);
			y -= m_hp[i];
			m_line[i][m_pos[i]] = y;
			if (size_t(++m_pos[i]) >= m_line[i].size())
				m_pos[i] = 0;
		}

		// ---- 取り出し。左右で違う遅延をまぜて広がりを出す
		const float l = (v[0] + v[2] - v[5] + v[7]) * 0.5f;
		const float r = (v[1] - v[3] + v[4] + v[6]) * 0.5f;
		const float mid = (l + r) * 0.5f, side = (l - r) * 0.5f * m_p.width;
		const float erg = m_p.er_level * 0.3f;
		out_l = mid + side + er * erg;
		out_r = mid - side + er * erg;
	}

private:
	static constexpr int TAPS = 8;
	static constexpr int AP = 2;

	float coef_lp(float hz) const
	{
		// 1 次のローパスの係数（1 - exp(-2πf/fs)）
		return 1.0f - std::exp(-2.0f * 3.14159265f * hz / m_rate);
	}

	float allpass(int i, float x)
	{
		static const int LEN[AP] = { 227, 353 };
		auto &line = m_ap[i];
		if (line.size() < 4)
			line.assign(size_t(LEN[i] * m_rate / 44100.0f) + 1, 0.0f);
		const float d = line[m_ap_pos[i]];
		const float y = -m_diff * x + d;
		line[m_ap_pos[i]] = x + m_diff * y;
		if (size_t(++m_ap_pos[i]) >= line.size())
			m_ap_pos[i] = 0;
		return y;
	}

	params m_p;
	float  m_rate = 44100.0f;
	std::array<std::vector<float>, TAPS> m_line;
	std::array<int, TAPS>   m_pos{};
	std::array<float, TAPS> m_gain{};
	std::array<float, TAPS> m_lp{};
	std::array<float, TAPS> m_hp{};
	std::vector<float> m_pre;
	size_t m_pre_pos = 0, m_pre_len = 0;
	std::array<std::vector<float>, AP> m_ap;
	std::array<int, AP> m_ap_pos{};
	float m_damp = 0.3f, m_hpf = 0.01f, m_diff = 0.7f;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_REVERB_H
