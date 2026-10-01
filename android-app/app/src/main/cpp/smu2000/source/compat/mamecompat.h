// license:BSD-3-Clause
//
// MAME 依存を肩代わりする最小限の層。
//
// MAME のデバイス実装（swp30.cpp など）は BSD-3-Clause なので流用できるが、
// device_t / address_space / sound_stream といった MAME 本体の仕組みに依存している。
// ここではそれらのうち **実際に使われている部分だけ** を用意する。
//
// 方針:
//   - アルゴリズム本体には手を触れない
//   - セーブステート、デバッガ、逆アセンブラは作らない（ソフトシンセに要らない）
//   - メモリ空間はフラットな配列。バンク切り替えもハンドラも無い
//   - 動的再コンパイラ(DRC)は使わない。インタプリタ経路だけを使う

#ifndef S_MU2000_MAMECOMPAT_H
#define S_MU2000_MAMECOMPAT_H

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---- MAME の整数型 ---------------------------------------------------------

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using offs_t = std::uint32_t;

// MAME のソースは uintN_t も混ぜて使うので、素の名前でも引けるようにする
using std::uint8_t;  using std::uint16_t; using std::uint32_t; using std::uint64_t;
using std::int8_t;   using std::int16_t;  using std::int32_t;  using std::int64_t;

// MAME のソースに散っている属性・注釈。中身は要らない
#define ATTR_COLD
#define ATTR_FORCE_INLINE inline

// ---- セーブステートとログ ---------------------------------------------------
//
// save_item は swp30.cpp だけで 100 箇所あるが、すべて初期化関数の中の登録処理。
// セーブステートを作らないので、まるごと消してよい。

#define NAME(x) x
#define save_item(...)      do {} while(0)
// state_add(...) はデバッガに見せるレジスタの登録。
// .formatstr("%08X").callimport() のように連ねて呼ばれるので、受け流す器を返す
struct state_entry_dummy
{
	template <typename... A> state_entry_dummy &formatstr(A &&...)  { return *this; }
	template <typename... A> state_entry_dummy &callimport(A &&...) { return *this; }
	template <typename... A> state_entry_dummy &callexport(A &&...) { return *this; }
	template <typename... A> state_entry_dummy &noshow(A &&...)     { return *this; }
	template <typename... A> state_entry_dummy &mask(A &&...)       { return *this; }
};
#define state_add(...)      (::state_entry_dummy{})

// デバッガのフック。デバッガを持たないので何もしない
// MAME はここでデバッガに命令を見せていた。こちらは PC の追跡にだけ使う
#define debugger_instruction_hook(pc) \
	do { if (::smu2000::g_pc_hash)  ::smu2000::pc_hash(pc, regs_hash()); \
	     if (::smu2000::g_pc_trace) { \
	         ::smu2000::g_pc_cycles = total_cycles(); \
	         ::smu2000::pc_trace(pc, regs_text()); } } while(0)
#define debugger_exception_hook(...)   do {} while(0)
#define debugger_wait_hook(...)        do {} while(0)
#define debugger_privilege_hook(...)   do {} while(0)
// 割り込みを受け取ったことをデバッガに知らせる。ベクタはそのまま返す
#define standard_irq_callback(irqline, pc) (0)

namespace smu2000 {
extern bool g_verbose;   // 既定では黙る。デバッグ時だけ true にする

// 移植の突き合わせ用。MAME の debugger の trace と同じものを出す
extern std::FILE *g_pc_trace;      // 生の PC 列
extern u64        g_pc_trace_left;
extern u64        g_pc_skip;       // 頭を飛ばす命令数
extern std::FILE *g_pc_hash;       // ブロックごとに畳んだ値
extern std::FILE *g_port_trace;    // ポート E（LCD 用）の出入り
extern u64        g_pc_cycles;     // 追跡に添えるサイクル数
extern std::FILE *g_upd_trace;     // 周辺を進めた時刻と次の予定
void pc_hash(u32 pc, u64 regs);        // 畳み込みだけ。安い
void pc_trace(u32 pc, const char *regs);

// どの番地で回っているかを数える（SMU2000_PCPROF=<出す件数> で入る）。
// JIT のブロックに入るたびに 1 つ数えるので、詰まっている輪がすぐ分かる。
// 追跡と違って JIT を止めないので、速さを測りながら使える
extern u32 *g_pc_prof;                 // 0x40 ごとの数え上げ。ROM 4MB ぶん
extern u64 g_pc_prof_why[5];           // 0 遅延枠 / 1 割り込みの印 / 2 番地が外 / 3 ブロックに入った / 4 進んだ命令数
extern u64 g_slow_mem[2];              // JIT の速い道から外れたメモリ操作（0 読み / 1 書き）
void pc_prof_start();                  // 環境変数を見て用意する。無ければ何もしない
void pc_prof_report();                 // 多い順に出す
}

// MAME の logerror は書式を自前で組み立てるので %s に std::string を渡せる。
// 素の fprintf に流すと壊れるため（swp30 の describe() がこれ）、
// util::string_format を通す
#define logerror(...)                        \
	do {                                     \
		if (::smu2000::g_verbose)              \
			::smu2000::log_fmt(__VA_ARGS__);     \
	} while(0)

// ---- メモリ空間 -------------------------------------------------------------
//
// MAME の memory_access<AddrBits, DataWidth, AddrShift, Endian>::cache の代わり。
// 相手はどれも素の配列（波形 ROM、リバーブ RAM、MEG のプログラム）なので、
// 範囲外を折り返すだけの単純な実装で足りる。
//
// AddrShift の意味は MAME と同じ:
//    -1 : アドレス 1 = 16bit ワード 1 個
//    -2 : アドレス 1 = 32bit ワード 1 個
//    -3 : アドレス 1 = 64bit ワード 1 個

template <int AddrBits, int DataWidth, int AddrShift>
class flat_space
{
public:
	void set(const void *base, size_t bytes)
	{
		// S-MU2000: 空のときは 8 バイトの 0 を指す。こうしておくと読みの側で
		// 「入っているか」を見なくてよくなる（read_dword は 1 声につき 2〜3 回呼ばれる）
		static const u8 s_zero[8] = {};
		m_base  = base ? reinterpret_cast<const u8 *>(base) : s_zero;
		m_bytes = base ? bytes : 0;
		m_mask  = m_bytes ? (m_bytes - 1) : 0;   // 2 の冪でない場合は下の wrap() で丸める
		m_pow2  = m_bytes && !(m_bytes & (m_bytes - 1));
	}

	u16 read_word(offs_t addr) const
	{
		return read_at<u16>(offset_of(addr, 2));
	}

	// S-MU2000: 声 1 つにつき 2〜3 回呼ばれる、いちばん熱い読み。
	// 相手（波形 ROM 32MB、リバーブ RAM 512KB）はどちらも 2 の冪なので、
	// そちらを枝の無い道にしてある。2 の冪でない領域は下の遅い道へ落とす
	u32 read_dword(offs_t addr) const
	{
		if (u32(addr - m_ov_from) < m_ov_units) {
			u32 v;
			std::memcpy(&v, m_ov + (size_t(addr - m_ov_from) << (-AddrShift)), 4);
			return v;
		}
		if (m_pow2) {
			u32 v;
			std::memcpy(&v, m_base + ((size_t(addr) << (-AddrShift)) & m_mask & ~size_t(3)), 4);
			return v;
		}
		return read_at<u32>(offset_of(addr, 4));
	}

	// S-MU2000: 番地 from から units 個ぶんを、別の書ける領域に差し替える（SWP30 のサンプリング RAM）。
	// 読み出し（read_dword）と書き込み（write_dword）の両方がこちらを使う
	void set_overlay(u8 *base, offs_t from, size_t units)
	{
		m_ov = base;
		m_ov_from = from;
		m_ov_units = base ? u32(units) : 0;
	}

	u64 read_qword(offs_t addr) const
	{
		return read_at<u64>(offset_of(addr, 8));
	}

	// 書き込みは可変な領域（リバーブ RAM）でのみ使う
	void set_writable(void *base, size_t bytes)
	{
		set(base, bytes);
		m_write = reinterpret_cast<u8 *>(base);
	}

	void write_word(offs_t addr, u16 data)
	{
		if (m_write) std::memcpy(m_write + offset_of(addr, 2), &data, 2);
	}

	void write_dword(offs_t addr, u32 data)
	{
		if (u32(addr - m_ov_from) < m_ov_units) {
			std::memcpy(m_ov + (size_t(addr - m_ov_from) << (-AddrShift)), &data, 4);
			return;
		}
		if (m_write) std::memcpy(m_write + offset_of(addr, 4), &data, 4);
	}

private:
	// AddrShift 分だけ左シフトしてバイト単位にし、領域内へ折り返す
	size_t offset_of(offs_t addr, size_t elem) const
	{
		size_t byte = size_t(addr) << (-AddrShift);
		return wrap(byte, elem);
	}

	size_t wrap(size_t byte, size_t elem) const
	{
		if (!m_bytes) return 0;
		if (m_pow2)   return byte & m_mask & ~(elem - 1);
		return (byte % m_bytes) & ~(elem - 1);
	}

	template <typename T> T read_at(size_t byte) const
	{
		T v = 0;
		if (m_base) std::memcpy(&v, m_base + byte, sizeof(T));
		return v;
	}

	const u8 *m_base  = nullptr;
	u8       *m_write = nullptr;
	size_t    m_bytes = 0;
	size_t    m_mask  = 0;
	bool      m_pow2  = false;
	u8       *m_ov    = nullptr;
	offs_t    m_ov_from  = 0;
	u32       m_ov_units = 0;
};

// ---- MAME の小物 ------------------------------------------------------------

// BIT(value, n) : n ビット目を取り出す。BIT(value, n, len) : n から len ビット
constexpr u32 BIT(u64 v, int n)          { return u32((v >> n) & 1); }
constexpr u64 BIT(u64 v, int n, int len) { return (v >> n) & ((u64(1) << len) - 1); }

namespace util {
// util::string_format は printf 形式。ログ用途にしか使われていない。
// MAME 版は std::string を %s に直接渡せるので、そこだけ真似ておく
template <typename T> inline T          fmt_arg(T v)                  { return v; }
inline                       const char *fmt_arg(const std::string &v) { return v.c_str(); }

template <typename... A>
inline std::string string_format(const char *fmt, A... args)
{
	char buf[1024];
	std::snprintf(buf, sizeof(buf), fmt, fmt_arg(args)...);
	return std::string(buf);
}
inline std::string string_format(const char *fmt) { return std::string(fmt); }
inline std::string string_format(const std::string &fmt) { return fmt; }
template <typename... A>
inline std::string string_format(const std::string &fmt, A... args)
{
	return string_format(fmt.c_str(), args...);
}

// util::stream_format は書式化して stream に流す。swp30 の describe() が
// これで発音中のサンプル位置や形式を組み立てているので、空実装にはできない
template <typename S, typename... A>
inline void stream_format(S &out, const char *fmt, A... args)
{
	out << string_format(fmt, args...);
}

// util::sext(value, bits) : bits ビットの符号付き値として符号拡張する
// util::make_bitmask<T>(n) : 下位 n ビットが立ったマスク
template <typename T> constexpr T make_bitmask(unsigned n)
{
	return T(n >= (sizeof(T) * 8) ? T(~T(0)) : ((T(1) << n) - 1));
}

// util::sext(value, width) : width ビットの符号付き値として符号拡張する。
// 元の型のまま扱うのが肝で、u64 に広げてから畳むと上位ビットが残る
template <typename T, typename U>
constexpr std::make_signed_t<T> sext(T value, U width) noexcept
{
	return std::make_signed_t<T>(value << (8 * sizeof(value) - width))
	       >> (8 * sizeof(value) - width);
}
}

namespace smu2000 {
template <typename... A>
inline void log_fmt(A &&...args)
{
	if (!g_verbose) return;
	std::fputs(util::string_format(std::forward<A>(args)...).c_str(), stderr);
}
}

// ---- MAME のデバイス生成まわり ----------------------------------------------
//
// MAME のデバイスは machine_config に登録して生成する。ここでは実体を直接
// 作るので、コンストラクタの引数を受け流すためだけの空の型を用意する。
// sh7042 は内蔵周辺（SCI, タイマ, ポート…）を required_device で持つので、
// それも「ただのポインタ」に置き換える。

// MAME のデバイス基底。周辺が実際に使うのは clock() と machine() だけだった
// （save_item と logerror はマクロで無効化済み）。
class running_machine;
class emu_timer;

class machine_config;

// デバイス種別。tag での検索をしないので中身は空でよい
struct device_type_dummy {};
using device_type = const device_type_dummy *;

class device_t
{
public:
	device_t() = default;
	// MAME の device_t(mconfig, type, tag, owner, clock)。
	// 使うのは clock だけ。tag での結線はしないので owner も型も捨てる
	device_t(const machine_config &, device_type, const char *, device_t *, u32 clock)
		: m_clock(clock) {}
	virtual ~device_t() = default;

	u32 clock() const { return m_clock; }
	void set_clock(u32 c) { m_clock = c; }

	running_machine &machine() const { return *m_machine; }
	void set_machine(running_machine *m) { m_machine = m; }

	// MAME が呼ぶ初期化。実体側は素直に呼べばよい
	// MAME はスケジューラが icount を直に触るための口。こちらは CPU が自分で持つ
	void set_icountptr(int &) {}

	virtual void device_start() {}
	virtual void device_reset() {}
	// サブデバイスの生成。こちらは実体を直に作るので中身は使わない
	virtual void device_add_mconfig(machine_config &) {}

	// MAME の timer_alloc(FUNC(cb), this)。呼び出し側の running_machine に預ける。
	// 定義は running_machine の完成後（このファイル下部）：machine().make_timer は
	// 非依存式なので Clang はクラス定義時点で running_machine の完成を要求する
	// （GCC は遅延検査）。MSVC/GCC 動作は不変。
	//
	// The body is written below, once running_machine is a complete type. Putting
	// it here makes **Clang reject it as member access into an incomplete type**:
	// the machine() call does not depend on the template arguments, so unlike GCC
	// it is checked at definition time rather than deferred to instantiation.
	template <typename T, typename U>
	emu_timer *timer_alloc(void (T::*cb)(s32), const char *, U *obj);

private:
	u32 m_clock = 0;
	running_machine *m_machine = nullptr;
};

struct address_map_constructor {};
struct address_map {};

// required_device<T> / optional_device<T> はサブデバイスへの参照。
// 実体はこちらで作って set() で結び付ける。
template <typename T>
class required_device
{
public:
	template <typename... A> required_device(A &&...) {}
	T *operator->() const { return m_target; }
	T &operator*()  const { return *m_target; }
	operator T *()  const { return m_target; }
	T *target() const { return m_target; }
	// MAME は tag 解決前でも lookup() で実体を取れる。こちらは同じもの
	T *lookup() const { return m_target; }
	bool found() const { return m_target != nullptr; }
	void set(T *p) { m_target = p; }
	// MAME は tag（文字列）で結線するが、こちらは実体を直接渡す。
	// MAME 側のソースは set_tag(*this) や set_tag(m_intc) の形で呼ぶので、
	// デバイスそのものと finder の両方を受ける
	template <typename U> void set_tag(U &&u)
	{
		using B = std::remove_cv_t<std::remove_reference_t<U>>;
		if constexpr (std::is_base_of_v<T, B>)
			m_target = &u;
		else if constexpr (std::is_pointer_v<B>)
			m_target = u;
		else if constexpr (requires { u.lookup(); })
			m_target = u.lookup();
		// それ以外（tag 文字列）は結線に使わないので捨てる
	}
private:
	T *m_target = nullptr;
};
template <typename T> using optional_device = required_device<T>;

// required_device_array<T,N> は同種のサブデバイスをまとめて持つもの
template <typename T, int N>
class required_device_array
{
public:
	template <typename... A> required_device_array(A &&...) {}
	required_device<T> &operator[](int i) { return m_devs[i]; }
	const required_device<T> &operator[](int i) const { return m_devs[i]; }
private:
	std::array<required_device<T>, N> m_devs;
};
template <typename T, int N> using optional_device_array = required_device_array<T, N>;

// MAME のコールバック（devcb）。外部との入出力を関数で受ける。
// MU2000 では LED ラッチやスイッチ走査、SCI の送信線がこれを通る。
//
// 実際の呼ばれ方を数えて型を決めた:
//   読み  cb()                      引数なし
//   書き  cb(offset, data, mem_mask) ポート
//   線    cb(state)                  SCI の TX / CLK
//
// 未接続のときに返す値は MAME 同様に構築時に決める（ポートは 0xffff など）。
template <typename R, typename... A>
class devcb_base
{
public:
	using fn = std::function<R(A...)>;
	// void を配列やメンバに持てないので、その場合だけ char で場所取りする
	using def_t = std::conditional_t<std::is_void_v<R>, char, R>;

	devcb_base() = default;
	template <typename O> explicit devcb_base(O &) {}
	template <typename O> devcb_base(O &, def_t d) : m_default(d) {}

	// MAME は cb.bind().set(...) の形で繋ぐ。取り込んだソースは
	//   auto read_porte() { return m_read_port16[2].bind(); }
	// のように auto で受けるので、参照を返すとコピーされて設定が捨てられる。
	// 元を指す小さな仲介を返す
	class binder
	{
	public:
		explicit binder(devcb_base &t) : m_t(&t) {}
		template <typename F> binder &set(F &&f) { m_t->set(std::forward<F>(f)); return *this; }
		template <typename V> binder &set_constant(V v)
		{
			m_t->set([v]() { return R(v); });
			return *this;
		}
		template <typename... X> binder &append(X &&...)     { return *this; }
		template <typename... X> binder &set_inputline(X &&...) { return *this; }
		binder &bind() { return *this; }
	private:
		devcb_base *m_t;
	};

	binder bind() { return binder(*this); }
	// MAME の書き手は (offset, data, mem_mask) を全部受けるものと、
	// data だけ受けるものの両方が許される。後者はここで合わせる
	template <typename F> devcb_base &set(F &&f)
	{
		if constexpr (std::is_invocable_v<F &, A...>)
			m_fn = std::forward<F>(f);
		else if constexpr (sizeof...(A) == 3)
			m_fn = [g = std::forward<F>(f)](offs_t, auto data, auto) mutable { g(data); };
		else
			static_assert(std::is_invocable_v<F &, A...>, "devcb に繋げない形の関数");
		return *this;
	}
	template <typename F> devcb_base &operator=(F &&f) { return set(std::forward<F>(f)); }
	void resolve() {}
	template <typename... X> void resolve_safe(X &&...) {}
	bool isnull() const { return !m_fn; }

	R operator()(A... a) const
	{
		if constexpr (std::is_void_v<R>) { if (m_fn) m_fn(a...); }
		else                             { return m_fn ? m_fn(a...) : m_default; }
	}

	// 同じ型を N 個並べたもの（ポートのように複数ある場合）。
	// MAME は array(*this) / array(*this, 既定値) で作る
	template <int N> class array
	{
	public:
		template <typename O> explicit array(O &) {}
		template <typename O> array(O &, def_t d) { for (auto &e : m_a) e.set_default(d); }
		devcb_base       &operator[](int i)       { return m_a[i]; }
		const devcb_base &operator[](int i) const { return m_a[i]; }
		constexpr int size() const { return N; }
	private:
		std::array<devcb_base, N> m_a;
	};

	void set_default(def_t d) { m_default = d; }

private:
	fn    m_fn;
	def_t m_default{};
};

using devcb_read8   = devcb_base<u8>;
using devcb_read16  = devcb_base<u16>;
using devcb_read32  = devcb_base<u32>;
using devcb_write8  = devcb_base<void, offs_t, u8,  u8>;
using devcb_write16 = devcb_base<void, offs_t, u16, u16>;
using devcb_write32 = devcb_base<void, offs_t, u32, u32>;
using devcb_read_line  = devcb_base<int>;
using devcb_write_line = devcb_base<void, int>;

// 時間。周辺のタイマが型として使うだけで、実際の刻みは呼び出し側が管理する
struct attotime
{
	// 内部はサンプルではなく「秒」
	double t = 0.0;
	bool   m_never = false;

	static attotime from_ticks(u64 n, u32 hz) { attotime a; a.t = double(n) / double(hz); return a; }

	// MAME では関数ではなく定数。書き方をそろえる
	static const attotime never;
	static const attotime zero;

	u64    as_ticks(u32 hz) const { return u64(t * double(hz)); }
	double as_double() const      { return t; }
	bool   is_never() const       { return m_never; }
	bool   is_zero() const        { return !m_never && t == 0.0; }

	bool operator==(const attotime &o) const { return m_never == o.m_never && t == o.t; }
	bool operator!=(const attotime &o) const { return !(*this == o); }
	bool operator<(const attotime &o) const
	{
		if (m_never || o.m_never) return !m_never && o.m_never;
		return t < o.t;
	}
	attotime operator+(const attotime &o) const { attotime a; a.t = t + o.t; return a; }
	attotime operator-(const attotime &o) const { attotime a; a.t = t - o.t; return a; }
	// 「この周期 × 周波数」で比を出すのに使われる
	attotime operator*(double k) const { attotime a; a.t = t * k; return a; }
	attotime operator/(double k) const { attotime a; a.t = t / k; return a; }
};

inline const attotime attotime::never = [] { attotime a; a.m_never = true; return a; }();
inline const attotime attotime::zero  = attotime();

// MAME のタイマ。MAME ではスケジューラが鳴らしていた。
// こちらは実行ループが「次に鳴る時刻」を見て CPU を区切り、時が来たら鳴らす。
// 時刻は CPU のサイクル数で持つ（アト秒の丸めを持ち込まないため）。
class running_machine;

class emu_timer
{
public:
	emu_timer(running_machine &m, std::function<void(s32)> cb)
		: m_machine(&m), m_cb(std::move(cb)) {}

	void adjust(const attotime &when, s32 param = 0);
	void enable(bool on) { if (!on) m_expire = ~u64(0); }

	bool  scheduled() const { return m_expire != ~u64(0); }
	u64   expire_cycles() const { return m_expire; }
	void  fire() { m_expire = ~u64(0); if (m_cb) m_cb(m_param); }

	// 状態の保存と復元。呼び先（m_cb）は組み立て直せるので写さない
	template <typename IO> void state_sync(IO &s) { s.v(m_expire); s.v(m_param); }

private:
	running_machine *m_machine;
	std::function<void(s32)> m_cb;
	u64 m_expire = ~u64(0);      // ~0 は「予定なし」
	s32 m_param = 0;
};

// 割り込み線の番号。NMI だけ使う
enum { INPUT_LINE_NMI = -1, INPUT_LINE_IRQ0 = 0 };

// MAME の machine_config はデバイスの木を組み立てる器。
// こちらは tag の木を持たず、生成した順に所有するだけ。
// device_start() をこの順に呼べばよい
class machine_config
{
public:
	template <typename T, typename F, typename... A>
	T &make(F &finder, A &&...args)
	{
		auto owned = std::make_unique<T>(*this, "", nullptr, std::forward<A>(args)...);
		T &dev = *owned;
		m_devices.push_back(std::move(owned));
		finder.set(&dev);
		return dev;
	}

	std::vector<std::unique_ptr<device_t>> m_devices;   // 生成順
};

// MAME はデバイス種別をグローバルな定数として持ち、生成時に渡す。
// SH_CMT(config, m_cmt, *this, m_intc, 144, 148) のように呼ばれるので、
// 種別そのものを「生成する関数」にしておくと、取り込んだソースを直さずに済む
template <typename T>
class device_creator : public device_type_dummy
{
public:
	template <typename F, typename... A>
	T &operator()(machine_config &config, F &finder, A &&...args) const
	{
		return config.make<T>(finder, std::forward<A>(args)...);
	}
	// device_t のコンストラクタには種別として自分を渡す
	operator const device_type_dummy *() const { return this; }
};

// こちらは tag での検索をしないので中身は要らないが、名前は参照されるので用意する。
// ヘッダ側の DECLARE で実体を作り、実装側の DEFINE は空にしておく
#define DECLARE_DEVICE_TYPE(Type, Class)  inline const device_creator<Class> Type;
#define DECLARE_DEVICE_TYPE_NS(Type, Class, ...) DECLARE_DEVICE_TYPE(Type, Class)
#define DEFINE_DEVICE_TYPE(...)
#define DEFINE_DEVICE_TYPE_NS(...)

// MAME の FUNC(x) は「関数と名前」を並べて渡すためのもの
#define FUNC(x) &x, #x

// MAME の finder。tag での結線はしないので、名前だけ通ればよい
struct finder_base { static constexpr const char *DUMMY_TAG = nullptr; };

// COMBINE_DATA: mem_mask の立っているビットだけ data で置き換える
#define COMBINE_DATA(ptr) 	(*(ptr) = (*(ptr) & ~mem_mask) | (data & mem_mask))
#define TIMER_CALLBACK_MEMBER(name) void name(s32 param)

// ---- 割り込み線とエラー -----------------------------------------------------

enum line_state { CLEAR_LINE = 0, ASSERT_LINE, HOLD_LINE };

[[noreturn]] inline void fatalerror_impl(const char *msg)
{
	std::fprintf(stderr, "fatal: %s\n", msg);
	std::abort();
}
#define fatalerror(...) do { char b[256]; std::snprintf(b, sizeof(b), __VA_ARGS__); fatalerror_impl(b); } while(0)

// ---- 乱数 -------------------------------------------------------------------
//
// swp30 が machine() を使うのは乱数のためだけ（LFO のランダム波形と MEG のノイズ）。
// MAME と**同じ数列**でないと波形が一致しないので、実装をそのまま写す。
//   src/emu/machine.cpp : running_machine::rand(), 初期シード 0x9d14abd7

class running_machine
{
public:
	u32 rand()
	{
		m_rand_seed = 1664525 * m_rand_seed + 1013904223;
		// 下位ビットは周期が短くよく使われるので 16bit 回転して返す
		return (m_rand_seed >> 16) | (m_rand_seed << 16);
	}

	void reset_seed() { m_rand_seed = 0x9d14abd7; }

	// MAME はログに「どこから呼ばれたか」を添えていた。こちらは持たない
	const char *describe_context() const { return ""; }

	// MAME はデバッガの覗き見で副作用を止める。こちらにデバッガはない
	bool side_effects_disabled() const { return false; }

	// ---- 時計。CPU のサイクル数で持つ。実行ループが進める

	void set_clock_hz(u32 hz) { m_hz = hz; }
	u32  clock_hz() const     { return m_hz; }
	void set_cycles(u64 c)    { m_cycles = c; }
	u64  cycles() const       { return m_cycles; }
	attotime time() const     { attotime a; a.t = double(m_cycles) / m_hz; return a; }

	emu_timer *make_timer(std::function<void(s32)> cb)
	{
		m_timers.push_back(std::make_unique<emu_timer>(*this, std::move(cb)));
		return m_timers.back().get();
	}

	// 次に鳴るタイマのサイクル数。予定がなければ ~0。
	// 実行ループが毎周ここを通るので覚えた値を返す形も試したが、
	// 速くならないうえに音が変わった（出力がビット単位で一致しなくなった）。
	// 数え直す方が安い。タイマは 8 本しかない
	u64 next_timer_cycles() const
	{
		u64 best = ~u64(0);
		for (const auto &t : m_timers)
			if (t->scheduled() && t->expire_cycles() < best)
				best = t->expire_cycles();
		return best;
	}


	// now までに来ているタイマを鳴らす
	void run_timers(u64 now)
	{
		for (int guard = 0; guard < 64; guard++) {
			emu_timer *due = nullptr;
			for (const auto &t : m_timers)
				if (t->scheduled() && t->expire_cycles() <= now &&
				    (!due || t->expire_cycles() < due->expire_cycles()))
					due = t.get();
			if (!due)
				return;
			m_cycles = due->expire_cycles();
			due->fire();
		}
	}

	// 状態の保存と復元。時計とタイマの予定を写す。
	// タイマは生まれた順に並んでいるので、番号で対応が取れる
	template <typename IO> void state_sync(IO &s)
	{
		s.tag("mach");
		s.v(m_rand_seed);
		s.v(m_cycles);
		u32 n = u32(m_timers.size());
		s.v(n);
		if (n != m_timers.size())
			return;                       // 数が違う。読み手が食い違いを見る
		for (auto &t : m_timers)
			t->state_sync(s);
	}

private:
	u32 m_rand_seed = 0x9d14abd7;
	u32 m_hz = 28000000;
	u64 m_cycles = 0;
	std::vector<std::unique_ptr<emu_timer>> m_timers;
};

// device_t::timer_alloc の定義（宣言は device_t 内、上のコメント参照）。
// running_machine が完成したここで初めて machine().make_timer が正当になる。
template <typename T, typename U>
inline emu_timer *device_t::timer_alloc(void (T::*cb)(s32), const char *, U *obj)
{
	T *self = static_cast<T *>(obj);
	return machine().make_timer([self, cb](s32 p) { (self->*cb)(p); });
}

inline void emu_timer::adjust(const attotime &when, s32 param)
{
	m_param = param;
	if (when.is_never())
		m_expire = ~u64(0);
	else
		m_expire = m_machine->cycles() + u64(when.as_double() * m_machine->clock_hz());
}

// ---- MAME の型名に合わせる --------------------------------------------------
//
// swp30.h は内部の構造体でも memory_access<...>::cache を引数の型に使っている。
// ここで同じ名前を用意しておけば、**ヘッダの中身に一切手を入れずに** 解決できる。

enum endianness_t { ENDIANNESS_LITTLE, ENDIANNESS_BIG };

template <int AddrBits, int DataWidth, int AddrShift, endianness_t Endian>
struct memory_access
{
	using cache    = flat_space<AddrBits, DataWidth, AddrShift>;
	using specific = flat_space<AddrBits, DataWidth, AddrShift>;
};

// required_region_ptr<u16> の代わり。ROM を指すだけ
template <typename T>
class region_ptr
{
public:
	void set(const T *base, size_t count) { m_base = base; m_count = count; }
	const T &operator[](size_t i) const
	{
		static const T zero = T();
		return (m_base && i < m_count) ? m_base[i] : zero;
	}
	const T *target() const { return m_base; }
	size_t count() const { return m_base ? m_count : 0; }
	explicit operator bool() const { return m_base != nullptr; }
private:
	const T *m_base = nullptr;
	size_t   m_count = 0;
};

// ---- 音声バッファ -----------------------------------------------------------
//
// MAME の sound_stream の代わり。swp30.cpp が使うのは get / put_int_clamp の 2 つだけ。

// swp30 は 1 サンプルにつき 1 回 sound_stream_update を呼ばれる作りで、
// index には常に 0 が渡る。出力は DAC 4 本 + 外部シリアル 16 本の計 20 本。
class sound_buffer
{
public:
	static constexpr int OUTPUTS = 20;   // 0-3 = DAC, 4-19 = MELO
	static constexpr int INPUTS  = 16;   // MELI（MU2000 では未使用）

	void reset(int inputs, int outputs, int samples)
	{
		m_samples = samples;
		m_in.assign(size_t(inputs) * samples, 0.0f);
		m_out.assign(size_t(outputs) * samples, 0.0f);
		m_inputs  = inputs;
		m_outputs = outputs;
	}

	int samples() const { return m_samples; }

	// 入力（外部シリアル入力。MU2000 では使わないので常に 0）
	float get(int channel, int index) const
	{
		if (channel >= m_inputs || index >= m_samples) return 0.0f;
		return m_in[size_t(channel) * m_samples + index];
	}

	// 出力。value / scale を -1.0〜+1.0 に収めて書き込む
	void put_int_clamp(int channel, int index, s32 value, s32 scale)
	{
		if (channel >= m_outputs || index >= m_samples) return;
		s32 v = std::clamp<s32>(value, -scale, scale - 1);
		m_out[size_t(channel) * m_samples + index] = float(v) / float(scale);
	}

	const float *output(int channel) const
	{
		return m_out.data() + size_t(channel) * m_samples;
	}

private:
	std::vector<float> m_in, m_out;
	int m_samples = 0, m_inputs = 0, m_outputs = 0;
};

#endif // S_MU2000_MAMECOMPAT_H
