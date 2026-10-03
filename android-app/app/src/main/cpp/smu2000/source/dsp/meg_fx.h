// license:BSD-3-Clause
// S-MU2000: 軽量モードの口（リバーブ・コーラス・バリエーション・インサーション 1）を、firmware が置いた
// MEG のプログラムの形に合わせて、MEG と同じ作りの C++ で鳴らす（doc/native-dsp.md）。
//
// プログラムが変わったら identify() で形を見分ける（係数と番地を除いた命令語の FNV-1a）。
// 知っている形（meg_fx_list.h）なら、その形の C++ で鳴らし、戻りは MEG と同じレジスタ（m24/m25 など）に書く。
// 知らない形なら active() が偽になり、呼ぶ側は前の試作（系統ごとの近似）で鳴らす。

#ifndef S_MU2000_DSP_MEG_FX_H
#define S_MU2000_DSP_MEG_FX_H

#include "meg_reverb.h"

#include <cstdint>
#include <memory>

namespace smu2000::dsp {

// 形ごとの C++ を同じ口で呼ぶためのもの
class meg_fx_iface
{
public:
	virtual ~meg_fx_iface() = default;
	virtual void resize(uint32_t window) = 0;
	virtual void configure(const int16_t *cst, const uint16_t *off, int base) = 0;
	virtual void set_table(const uint16_t *ram) = 0;
	virtual void load_ram(const uint16_t *revram, uint32_t base, uint32_t counter) = 0;
	virtual void reset() = 0;
	virtual void process(const float *in, float *out, const float *lfo) = 0;
	virtual uint32_t lfo_used() const = 0;
	virtual int n_in() const = 0;
	virtual int n_out() const = 0;
	virtual int in_reg(int i) const = 0;
	virtual int out_reg(int i) const = 0;
};

template<class T>
class meg_fx_impl final : public meg_fx_iface
{
public:
	void resize(uint32_t window) override { m_t.resize(window); }
	void configure(const int16_t *cst, const uint16_t *off, int base) override { m_t.configure(cst, off, base); }
	void set_table(const uint16_t *ram) override { m_t.set_table(ram); }
	void load_ram(const uint16_t *revram, uint32_t base, uint32_t counter) override { m_t.load_ram(revram, base, counter); }
	void reset() override { m_t.reset(); }
	void process(const float *in, float *out, const float *lfo) override { m_t.process(in, out, lfo); }
	uint32_t lfo_used() const override { return T::LFO_USED; }
	int n_in() const override { return T::N_IN; }
	int n_out() const override { return T::N_OUT; }
	int in_reg(int i) const override { return T::IN_REGS[i]; }
	int out_reg(int i) const override { return T::OUT_REGS[i]; }
private:
	T m_t;
};

template<class T>
meg_fx_iface *make_meg_fx() { return new meg_fx_impl<T>; }

// リバーブ（手で書き起こしたもの）を同じ口に合わせる
class meg_fx_reverb
{
public:
	static constexpr int N_IN = 2, N_OUT = 2;
	static constexpr uint32_t LFO_USED = 0;
	static constexpr uint8_t IN_REGS[N_IN] = { 0x24, 0x25 };
	static constexpr uint8_t OUT_REGS[N_OUT] = { 0x24, 0x25 };
	void resize(uint32_t window) { m_rev.resize(window); }
	void configure(const int16_t *cst, const uint16_t *off, int) { m_rev.configure(cst, off); }
	void set_table(const uint16_t *) {}
	void load_ram(const uint16_t *revram, uint32_t base, uint32_t counter) { m_rev.load_ram(revram, base, counter); }
	void reset() { m_rev.reset(); }
	void process(const float *in, float *out, const float *) { m_rev.process(in[0], in[1], out[0], out[1]); }
private:
	meg_reverb m_rev;
};

struct meg_fx_entry {
	uint64_t hash;          // 命令語の FNV-1a（係数と番地を除く）
	int len;                // 命令の数
	meg_fx_iface *(*make)();
};

} // namespace smu2000::dsp

#include "meg_fx_list.h"

namespace smu2000::dsp {

class meg_fx_slot
{
public:
	// program: MEG の命令 0x180 語。lo, hi: この口の命令の範囲
	void identify(const uint64_t *program, int lo, int hi)
	{
		uint64_t h = 0xcbf29ce484222325ull;
		for (int pc = lo; pc != hi; pc++)
			for (int i = 0; i != 8; i++)
				h = (h ^ ((program[pc] >> (8 * i)) & 0xff)) * 0x100000001b3ull;
		const meg_fx_entry *found = nullptr;
		for (const meg_fx_entry &e : MEG_FX_LIST)
			if (e.hash == h && e.len == hi - lo)
				found = &e;
		if (found != m_entry || lo != m_lo) {
			m_fx.reset(found ? found->make() : nullptr);
			m_entry = found;
			m_fresh = true;
		}
		m_lo = lo;
	}

	bool active() const { return m_fx != nullptr; }
	// 作ったばかり（遅延メモリをまだ MEG から写していない）か。写したら偽にする
	bool fresh() const { return m_fresh; }
	void mark_loaded() { m_fresh = false; }
	meg_fx_iface &fx() { return *m_fx; }
	int lo() const { return m_lo; }

	void reset()
	{
		if (m_fx)
			m_fx->reset();
	}

private:
	const meg_fx_entry *m_entry = nullptr;
	std::unique_ptr<meg_fx_iface> m_fx;
	int m_lo = 0;
	bool m_fresh = false;
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_H
