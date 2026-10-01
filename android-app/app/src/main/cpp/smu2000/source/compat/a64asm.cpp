// license:BSD-3-Clause
//
// S-MU2000: selftest for the aarch64 emitter (a64asm.h). Builds a tiny
// function out of each primitive, runs it on the host CPU and compares the
// result with the plain C++ equivalent, so an encoding mistake cannot reach
// the JIT unnoticed. Wired into the verify tool next to the MEG JIT selftest.

#include "a64asm.h"
#include "exec_mem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __aarch64__

using namespace a64;

namespace {

// Assemble, copy to executable memory, run as u32(u8*) or u64(u8*), free.
template <typename T>
T run(const emitter &a, void *scratch = nullptr)
{
	const size_t bytes = a.code.size() * sizeof(u32);
	void *buf = exec_mem::alloc_rw(bytes + 16);
	if (!buf)
		return T(1);   // allocation failure: the caller sees a mismatch
	std::memcpy(buf, a.code.data(), bytes);
	if (!exec_mem::make_executable(buf, bytes + 16)) {
		exec_mem::free_mem(buf, bytes + 16);
		return T(1);
	}
	if (std::getenv("A64_DEBUG")) {
		std::fprintf(stderr, "run %zu insns:", a.code.size());
		for (u32 w : a.code) std::fprintf(stderr, " %08x", w);
		std::fprintf(stderr, "\n");
	}
	const T r = reinterpret_cast<T (*)(void *)>(buf)(scratch);
	exec_mem::free_mem(buf, bytes + 16);
	return r;
}

u32 run32(const emitter &a, void *scratch = nullptr) { return run<u32>(a, scratch); }
u64 run64(const emitter &a, void *scratch = nullptr) { return run<u64>(a, scratch); }

// call targets for the X17 tests
int add7(int v) { return v + 7; }
int twice(int v) { return add7(add7(v)); }

} // namespace

// check helpers: on mismatch, dump the snippet's machine code plus the got
// and want values. `a` is the snippet's emitter in the enclosing block.
#define CHECK32(want) do { u32 got_ = run32(a); if (got_ != u32(want)) { \
	std::fprintf(stderr, "CHECK32 (%zu insns):", a.code.size()); \
	for (u32 w_ : a.code) std::fprintf(stderr, " %08x", w_); \
	std::fprintf(stderr, " | got %08x want %08x\n", got_, u32(want)); \
	bad++; } } while (0)
#define CHECK64(want) do { u64 got_ = run64(a); if (got_ != u64(want)) { \
	std::fprintf(stderr, "CHECK64 (%zu insns):", a.code.size()); \
	for (u32 w_ : a.code) std::fprintf(stderr, " %08x", w_); \
	std::fprintf(stderr, " | got %016llx want %016llx\n", (unsigned long long)got_, (unsigned long long)u64(want)); \
	bad++; } } while (0)
#define CHECK32S(want, scr) do { u32 got_ = run32(a, scr); if (got_ != u32(want)) { \
	std::fprintf(stderr, "CHECK32S (%zu insns):", a.code.size()); \
	for (u32 w_ : a.code) std::fprintf(stderr, " %08x", w_); \
	std::fprintf(stderr, " | got %08x want %08x\n", got_, u32(want)); \
	bad++; } } while (0)

u64 a64::selftest()
{
	if (sizeof(void *) != 8)
		return ~u64(0);
	u64 bad = 0;

	// ---- data processing: register forms ----------------------------------
	{
		emitter a;
		a.mov_imm32(W1, 0x12345678);
		a.mov_imm32(W2, 0x9abcdef0);
		a.add_reg(W0, W1, W2);        a.ret();
		CHECK32(0x12345678u + 0x9abcdef0u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x000000ff);
		a.mov_imm32(W2, 0x000000ab);
		a.add_reg(W0, W1, W2, 0, 4);   // W0 = W1 + (W2 << 4): LSL, amount 4
		a.ret();
		CHECK32(0xffu + (0xabu << 4));
	}
	{
		emitter a;                    // ADDS: 0xffffffff + 1 carries out
		a.mov_imm32(W1, 0xffffffff);
		a.mov_imm32(W2, 1);
		a.adds_reg(W0, W1, W2);
		a.cset(W0, CS);               // carry flag
		a.ret();
		CHECK32(1);
	}
	{
		emitter a;                    // SUBS: borrow (C = 0)
		a.mov_imm32(W1, 5);
		a.mov_imm32(W2, 7);
		a.subs_reg(W0, W1, W2);
		a.cset(W0, CC);
		a.ret();
		CHECK32(1);
	}
	{
		emitter a;                    // SUB plain
		a.mov_imm32(W1, 0x9abcdef0);
		a.mov_imm32(W2, 0x12345678);
		a.sub_reg(W0, W1, W2);
		a.ret();
		CHECK32(0x9abcdef0u - 0x12345678u);
	}
	{
		emitter a;                    // logical register forms
		a.mov_imm32(W1, 0xf0f0f0f0);
		a.mov_imm32(W2, 0x0f0f1234);
		a.and_reg(W0, W1, W2);        a.ret();
		CHECK32((0xf0f0f0f0u & 0x0f0f1234u));
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xf0f0f0f0);
		a.mov_imm32(W2, 0x0f0f1234);
		a.orr_reg(W0, W1, W2);        a.ret();
		CHECK32((0xf0f0f0f0u | 0x0f0f1234u));
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xf0f0f0f0);
		a.mov_imm32(W2, 0x0f0f1234);
		a.eor_reg(W0, W1, W2);        a.ret();
		CHECK32((0xf0f0f0f0u ^ 0x0f0f1234u));
	}
	{
		emitter a;                    // ANDS + condition: identical operands give a
		// NONZERO result, so Z stays clear and EQ is false
		a.mov_imm32(W1, 0x1234);
		a.mov_imm32(W2, 0x1234);
		a.ands_reg(W0, W1, W2);
		a.cset(W0, EQ);
		a.ret();
		CHECK32(0);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xf0f0f0f0);
		a.mov_imm32(W2, 0x0f0f1234);
		a.bic_reg(W0, W1, W2);        a.ret();
		CHECK32((0xf0f0f0f0u & ~0x0f0f1234u));
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xf0f0f0f0);
		a.mov_imm32(W2, 0x0f0f1234);
		a.orn_reg(W0, W1, W2);        a.ret();
		CHECK32((0xf0f0f0f0u | ~0x0f0f1234u));
	}

	// ---- immediate forms ---------------------------------------------------
	{
		emitter a;
		a.mov_imm32(W1, 0x12345000);
		a.add_imm(W0, W1, 4095);      // max imm12
		a.ret();
		CHECK32(0x12345000u + 4095u);
	}
	{
		emitter a;                    // imm12 shifted by 12
		a.mov_imm32(W1, 0x12345000);
		a.add_imm(W0, W1, 1, 1);      // W0 = W1 + 4096
		a.ret();
		CHECK32(0x12345000u + 0x1000u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x12345678);
		a.sub_imm(W0, W1, 4095);      a.ret();
		CHECK32(0x12345678u - 4095u);
	}
	{
		emitter a;                    // ADDS imm -> zero result sets Z
		a.mov_imm32(W1, 0xfffffffd);
		a.adds_imm(W0, W1, 3);
		a.cset(W0, EQ);
		a.ret();
		CHECK32(1);
	}
	{
		emitter a;                    // SUBS imm: small - big wraps, borrow (CC) set
		a.mov_imm32(W1, 3);
		a.subs_imm(W0, W1, 0xffb);
		a.cset(W0, CC);
		a.ret();
		CHECK32(1);
	}

	// ---- move wide ----------------------------------------------------------
	{
		emitter a;                    // movz + movk full 32-bit
		a.movz(W0, 0x1234, 0);
		a.movk(W0, 0x5678, 1);
		a.ret();
		CHECK32(0x56781234u);
	}
	{
		emitter a;                    // movn: inverted low half
		a.movn(W0, 0x1234, 0);
		a.ret();
		CHECK32(~0x1234u);
	}
	{
		emitter a;                    // movn into high half
		a.movn(W0, 0x0000, 1);        // W0 = ~0x00000000 -> 0xffffffff
		a.ret();
		CHECK32(0xffffffffu);
	}
	{
		emitter a;
		a.mov_imm32(W0, 0xffffffff);  // movn path
		a.ret();
		CHECK32(0xffffffffu);
	}
	{
		emitter a;
		a.mov_imm32(W0, 0x1234abcd);  // movz+movk path
		a.ret();
		CHECK32(0x1234abcdu);
	}
	{
		emitter a;                    // mov_imm32 must cover bit 31 without signing trouble
		a.mov_imm32(W0, 0x80000001);
		a.ret();
		CHECK32(0x80000001u);
	}

	// ---- moves, extends, neg -----------------------------------------------
	{
		emitter a;
		a.mov_imm32(W1, 0xdeadbeef);
		a.mov_reg(W0, W1);            a.ret();
		CHECK32(0xdeadbeefu);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x12345678);
		a.mvn_reg(W0, W1);            a.ret();
		CHECK32(~0x12345678u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x00000080);
		a.sxtb(W0, W1);               a.ret();
		CHECK32(u32(s32(s8(0x80))));
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x00008000);
		a.sxth(W0, W1);               a.ret();
		CHECK32(u32(s32(s16(0x8000))));
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xffffff80);
		a.uxtb(W0, W1);               a.ret();
		CHECK32(0x80u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xffff8000);
		a.uxth(W0, W1);               a.ret();
		CHECK32(0x8000u);
	}
	{
		emitter a;                    // sxtw: 0x80000000 -> 0xffffffff80000000
		a.mov_imm32(W1, 0x80000000);
		a.sxtw64(X0, W1);             a.ret();
		CHECK64(u64(s64(s32(0x80000000))));
	}
	{
		emitter a;                    // orr xd, wzr, wn zero-extends into the x register
		a.mov_imm32(W1, 0x80000000);
		a.mov_to64(X0, W1);           a.ret();
		CHECK64(0x80000000u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 7);
		a.neg_reg(W0, W1);            a.ret();
		CHECK32(u32(0u - 7u));
	}

	// ---- shifts -------------------------------------------------------------
	{
		const u32 shift_cases[][3] = {   // value, shift, kind marker
			{ 0x80000001, 4, 0 }, { 0x80000001, 4, 1 }, { 0x80000001, 4, 2 },
			{ 0x12345678, 0, 1 }, { 0x12345678, 31, 0 }, { 0x12345678, 31, 1 },
			{ 0x12345678, 12, 3 },
		};
		for (const auto &c : shift_cases) {
			emitter a;
			a.mov_imm32(W1, c[0]);
			a.mov_imm32(W2, c[1]);
			if (c[2] == 0) a.lslv(W0, W1, W2);
			else if (c[2] == 1) a.lsrv(W0, W1, W2);
			else if (c[2] == 2) a.asrv(W0, W1, W2);
			else a.rorv(W0, W1, W2);
			a.ret();
			u32 want;
			const u32 v = c[0];
			const int s = int(c[1] & 31);   // aarch64 uses shift mod 32
			if (c[2] == 0)      want = v << s;
			else if (c[2] == 1) want = v >> s;                         // s in 0..31: no C UB
			else if (c[2] == 2) want = u32(s32(v) >> s);
			else                want = s ? (v >> s) | (v << (32 - s)) : v;
			CHECK32(want);
		}
	}
	{
		emitter a;                    // register shifts wrap mod 32: shift by 32
		// is a no-op (unlike the immediate shifts, which saturate)
		a.mov_imm32(W1, 0x12345678);
		a.mov_imm32(W2, 32);
		a.lsrv(W0, W1, W2);           a.ret();
		CHECK32(0x12345678u);
	}
	{
		emitter a;                    // immediate shifts
		a.mov_imm32(W1, 0x0000000f);
		a.lsl_imm(W0, W1, 4);         a.ret();
		CHECK32(0xf0u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x80000000);
		a.sar_imm(W0, W1, 31);        a.ret();
		CHECK32(0xffffffffu);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x80000001);
		a.lsr_imm(W0, W1, 1);         a.ret();
		CHECK32(0x40000000u);
	}

	// ---- multiply / divide / bit scans --------------------------------------
	// The product goes to W3 and only then to the result register: MUL/MNEG are
	// MADD/MSUB with Ra = WZR, and a test that wrote the result straight to W0
	// would pass even if Ra were left pointing at W0 (which is what the JIT's
	// scratch register holds).
	{
		emitter a;
		a.mov_imm32(W0, 0xdeadbeef);
		a.mov_imm32(W1, 0x12345678);
		a.mov_imm32(W2, 0x9abcdef0);
		a.mul(W3, W1, W2);
		a.mov_reg(W0, W3);            a.ret();
		CHECK32(0x12345678u * 0x9abcdef0u);
	}
	{
		emitter a;
		a.mov_imm32(W0, 0xdeadbeef);
		a.mov_imm32(W1, 100);
		a.mov_imm32(W2, 7);
		a.mneg(W3, W1, W2);
		a.mov_reg(W0, W3);            a.ret();
		CHECK32(u32(0u - 100u * 7u));
	}
	{
		emitter a;                    // smull: (-2) * 3 = -6, visible in 64 bits
		a.mov_imm32(W1, u32(-2));
		a.mov_imm32(W2, 3);
		a.smull64(X0, W1, W2);        a.ret();
		CHECK64(u64(s64(-2) * s64(3)));
	}
	{
		emitter a;                    // umull: 0xffffffff * 2 crosses into bit 32
		a.mov_imm32(W1, 0xffffffff);
		a.mov_imm32(W2, 2);
		a.umull64(X0, W1, W2);        a.ret();
		CHECK64(u64(0xffffffffu) * 2u);
	}
	{
		emitter a;                    // sdiv rounds toward zero
		a.mov_imm32(W1, u32(-7));
		a.mov_imm32(W2, 2);
		a.sdiv(W0, W1, W2);           a.ret();
		CHECK32(u32(-7 / 2));
	}
	{
		emitter a;                    // sdiv by zero: 0 by definition
		a.mov_imm32(W1, 1234);
		a.mov_imm32(W2, 0);
		a.sdiv(W0, W1, W2);           a.ret();
		CHECK32(0);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0xffffffff);
		a.mov_imm32(W2, 16);
		a.udiv(W0, W1, W2);           a.ret();
		CHECK32(0xffffffffu / 16u);
	}
	{
		emitter a;                    // rbit + clz = count trailing zeros
		a.mov_imm32(W1, 0x00800000);
		a.rbit(W2, W1);
		a.clz(W0, W2);                a.ret();
		u32 want = 0, v = 0x00800000;
		while (!(v & 1)) { v >>= 1; want++; }
		CHECK32(want);
	}

	// ---- conditions ----------------------------------------------------------
	{
		struct { u32 a, b; u8 cond; u32 want; } cs[] = {
			{ 5, 5, EQ, 1 }, { 5, 6, EQ, 0 }, { 5, 6, NE, 1 },
			{ 5, 5, CS, 1 }, { 3, 5, CS, 0 },   // 3-5 borrows -> C=0
			{ 5, 3, CC, 0 }, { 3, 5, CC, 1 },
			{ 7, 3, HI, 1 }, { 3, 7, HI, 0 }, { 7, 7, HI, 0 },
			{ 3, 7, LS, 1 }, { 7, 3, LS, 0 },
			{ 3, u32(-1), GE, 1 }, { 3, u32(-1), LT, 0 },   // signed: 3 > -1
			{ u32(-1), 3, GT, 0 }, { 3, 3, GT, 0 },
			{ 3, 3, LE, 1 }, { 5, 3, LE, 0 },
		};
		for (const auto &c : cs) {
			emitter a;
			a.mov_imm32(W1, c.a);
			a.mov_imm32(W2, c.b);
			a.cmp_reg(W1, W2);
			a.cset(W0, c.cond);
			a.ret();
			CHECK32(c.want);
		}
	}
	{
		emitter a;                    // cmp against an immediate (must fit imm12)
		a.mov_imm32(W1, 0x234);
		a.cmp_imm(W1, 0x234);
		a.cset(W0, EQ);               a.ret();
		CHECK32(1);
	}

	// ---- memory: immediate offsets -------------------------------------------
	{
		u8 scratch[32] = {};
		emitter a;
		a.mov_imm32(W1, 0x11);
		a.strb(W1, X0, 3);            // byte offset 3 (imm12 unscaled)
		a.mov_imm32(W2, 0x2233);
		a.strh(W2, X0, 6);            // halfword at 6 (imm = 3 units of 2)
		a.mov_imm32(W3, 0x44556677);
		a.str_w(W3, X0, 8);           // word at 8 (imm = 2 units of 4)
		a.ldrb(W4, X0, 3);
		a.ldrh(W5, X0, 6);
		a.ldr_w(W6, X0, 8);
		a.add_reg(W0, W4, W5);
		a.add_reg(W0, W0, W6);
		a.ret();
		CHECK32S(0x11u + 0x2233u + 0x44556677u, scratch);
		if (scratch[3] != 0x11 || scratch[6] != 0x33 || scratch[7] != 0x22) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; } // strb at 3, strh little-endian at 6
		if (std::memcmp(scratch + 8, "\x77\x66\x55\x44", 4) != 0) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}
	{
		u8 scratch[32] = {};
		emitter a;                    // 64-bit store/load round trip
		a.mov_imm64(X1, u64(0x1122334455667788ull));
		a.str_x(X1, X0, 8);
		a.ldr_x(X0, X0, 8);
		a.ret();
		if (run64(a, scratch) != u64(0x1122334455667788ull)) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
		if (std::memcmp(scratch + 8, "\x88\x77\x66\x55\x44\x33\x22\x11", 8) != 0) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}

	// ---- memory: register offsets --------------------------------------------
	{
		u8 scratch[32] = {};
		emitter a;
		a.mov_imm32(W1, 0x5a);
		a.mov_imm32(W2, 5);           // byte index (x2 = 5)
		a.strb_r(W1, X0, X2);
		a.mov_imm32(W2, 10);
		a.mov_imm32(W1, 0x3344);
		a.strh_r(W1, X0, X2);
		a.mov_imm32(W2, 20);
		a.mov_imm32(W1, 0x778899aa);
		a.str_w_r(W1, X0, X2);
		a.mov_imm32(W2, 5);
		a.ldrb_r(W3, X0, X2);
		a.mov_imm32(W2, 10);
		a.ldrh_r(W4, X0, X2);
		a.mov_imm32(W2, 20);
		a.ldr_w_r(W5, X0, X2);
		a.add_reg(W0, W3, W4);
		a.add_reg(W0, W0, W5);
		a.ret();
		CHECK32S(0x5au + 0x3344u + 0x778899aau, scratch);
		if (scratch[5] != 0x5a || scratch[10] != 0x44 || scratch[11] != 0x33) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; } // strh is little-endian: low byte first
		if (std::memcmp(scratch + 20, "\xaa\x99\x88\x77", 4) != 0) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}

	// ---- memory: pre/post index ----------------------------------------------
	{
		u8 scratch[32] = {};
		emitter a;                    // two post-increment stores walk the buffer
		a.mov_imm32(W1, 0x11111111);
		a.str_w_post(W1, X0, 4);      // store at 0, X0 += 4
		a.mov_imm32(W1, 0x22222222);
		a.str_w_post(W1, X0, 4);      // store at 4, X0 += 4
		a.mov_imm32(W2, 0x33333333);
		a.str_w_pre(W2, X0, 4);       // X0 += 4 (now base+12), store at 12
		a.sub_imm64(X0, X0, 12);      // back to base (64-bit: keep the pointer intact)
		a.ldr_w_post(W3, X0, 4);      // load 0x11111111, X0 = 4
		a.ldr_w(W0, X0, 0);           // load 0x22222222
		a.add_reg(W0, W0, W3);
		a.ret();
		CHECK32S(0x11111111u + 0x22222222u, scratch);
		if (std::memcmp(scratch, "\x11\x11\x11\x11\x22\x22\x22\x22\x00\x00\x00\x00\x33\x33\x33\x33", 16) != 0) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}

	// ---- branches and patches -------------------------------------------------
	{
		emitter a;                    // forward B.cond: 5 == 5, so B.EQ is taken
		// and skips the overwrite
		a.movz(W0, 7, 0);
		a.mov_imm32(W1, 5);
		a.mov_imm32(W2, 5);
		a.cmp_reg(W1, W2);
		const size_t skip = a.b_cond(EQ);
		a.movz(W0, 9, 0);             // must be skipped when 5 == 5
		a.patch(skip);
		a.ret();
		CHECK32(7);
	}
	{
		emitter a;                    // backward CB(N)Z: count a loop down
		a.movz(W2, 5, 0);
		a.movz(W0, 0, 0);
		const size_t loop = a.here();
		a.add_imm(W0, W0, 3);
		a.subs_imm(W2, W2, 1);
		const size_t again = a.cbnz_w(W2);
		a.patch_to(again, loop);
		a.ret();
		CHECK32(15);
	}
	{
		emitter a;                    // CBZ taken on zero skips the overwrite
		a.movz(W0, 1, 0);
		a.movz(W1, 0, 0);
		const size_t skip = a.cbz_w(W1);
		a.movz(W0, 2, 0);
		a.patch(skip);
		a.ret();
		CHECK32(1);
	}
	{
		emitter a;                    // CBZ not taken on nonzero
		a.movz(W0, 1, 0);
		a.movz(W1, 3, 0);
		const size_t skip = a.cbz_w(W1);
		a.movz(W0, 2, 0);
		a.patch(skip);
		a.ret();
		CHECK32(2);
	}
	{
		emitter a;                    // TBZ / TBNZ on bit 5
		a.mov_imm32(W1, 0x20);        // bit 5 set
		const size_t zero = a.tbz(W1, 5);
		a.movz(W0, 1, 0);
		a.patch(zero);
		a.ret();
		CHECK32(1);     // bit set -> TBZ not taken -> w0 = 1
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x10);        // bit 5 clear
		const size_t zero = a.tbnz(W1, 5);
		a.movz(W0, 1, 0);
		a.patch(zero);
		a.ret();
		CHECK32(1);     // bit clear -> TBNZ not taken -> w0 = 1
	}

	// ---- calls through X17 ----------------------------------------------------
	{
		emitter a;                    // BLR through a 64-bit constant. The snippet
		// needs its own frame: BLR writes x30, so a plain ret afterwards would
		// jump to the stale address and loop forever
		a.emit(0xA9BF7BFDu);          // stp x29, x30, [sp, #-16]!
		a.emit(0x910003FDu);          // mov x29, sp
		a.movz(W0, 5, 0);
		a.mov_imm64_x17(reinterpret_cast<u64>(&add7));
		a.blr_x17();
		a.emit(0xA8C17BFDu);          // ldp x29, x30, [sp], #16
		a.ret();
		CHECK32(12);
	}
	{
		emitter a;                    // BR as a tail call: since we never BLR, x30
		// still holds run()'s return address and the callee returns straight
		// there. No frame may be pushed: sp must stay balanced for run()'s own
		// epilogue
		a.movz(W0, 100, 0);
		a.mov_imm64_x17(reinterpret_cast<u64>(&twice));
		a.br_x17();
		a.ret();                      // never reached; twice returns for us
		CHECK32(114);
	}
	{
		emitter a;                    // mov_imm64 into a callee-saved register
		a.mov_imm64(X19, u64(0xfedcba9876543210ull));
		a.mov_to64(X0, W19);
		a.ret();
		CHECK32(0x76543210u);
	}

	// ---- rev16 / rev32 ----------------------------------------------------------
	{
		emitter a;
		a.mov_imm32(W1, 0x12345678);
		a.rev32(W0, W1);              a.ret();
		CHECK32(0x78563412u);
	}
	{
		emitter a;
		a.mov_imm32(W1, 0x12345678);
		a.rev16(W0, W1);              a.ret();
		CHECK32(0x34127856u);
	}

	// ---- register-offset memory with extend option ------------------------------
	{
		u8 scratch[32] = {};
		emitter a;
		a.mov_imm32(W1, 0x77);
		a.mov_imm32(W2, 9);           // byte index, zero-extended
		a.strb_x(W1, X0, W2, false);
		a.ldrb_x(W0, X0, W2, false);
		a.ret();
		CHECK32S(0x77u, scratch);
		if (scratch[9] != 0x77) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}
	{
		u8 scratch[32] = {};
		emitter a;
		a.mov_imm32(W1, 0x2233);
		a.mov_imm32(W2, 12);
		a.strh_x(W1, X0, W2, false);
		a.ldrh_x(W0, X0, W2, false);
		a.ret();
		CHECK32S(0x2233u, scratch);
		if (scratch[12] != 0x33 || scratch[13] != 0x22) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}
	{
		u8 scratch[32] = {};
		emitter a;
		a.mov_imm32(W1, 0x44556677);
		a.mov_imm32(W2, 16);
		a.str_w_x(W1, X0, W2, false);
		a.ldr_w_x(W0, X0, W2, false);
		a.ret();
		CHECK32S(0x44556677u, scratch);
	}

	// ---- stack pairs --------------------------------------------------------------
	{
		emitter a;                    // stp/ldp round trip through the red zone-free frame
		a.emit(0xA9BF7BFDu);          // stp x29, x30, [sp, #-16]!
		a.emit(0x910003FDu);          // mov x29, sp
		a.stp_x(X19, X20, X31, -16, true);   // stp x19, x20, [sp, #-16]!
		a.mov_imm64(X19, u64(0x0102030405060708ull));
		a.mov_imm64(X20, u64(0x1122334455667788ull));
		a.ldp_x(X19, X20, X31, 16, true);    // ldp x19, x20, [sp], #16 (restores the saved pair)
		a.mov_imm64(X0, u64(0x0102030405060708ull));  // the stp'd x19 value survived the trip
		a.ldp_x(X29, X30, X31, 16, true);    // ldp x29, x30, [sp], #16 (sp back where run() expects)
		a.ret();
		CHECK32(0x05060708u);
	}

	// ---- 64-bit pointer arithmetic ------------------------------------------------
	{
		emitter a;
		a.mov_imm64(X1, u64(0x0000ffff00000010ull));
		a.add_imm64(X0, X1, 0x10);
		a.ret();
		CHECK64(0x0000ffff00000010u + 0x10u);
	}

	// ---- big-offset forms + imm12 fixup --------------------------------------------
	{
		emitter a;                   // emit with placeholder offset, then repair the field
		u8 scratch[16384] = {};      // 16380 needs the full imm12 range (4095 words)
		a.mov_imm64(X1, reinterpret_cast<u64>(scratch));
		a.mov_imm32(W2, 0x5a5a);
		const size_t st = a.str_w_big(W2, X1, 0);
		a.fix12(st, 16380 / 4);
		const size_t ld = a.ldr_w_big(W0, X1, 0);
		a.fix12(ld, 16380 / 4);
		a.ret();
		CHECK32(0x5a5au);
	}

	// ---- additions for the SH2 backend ---------------------------------------------
	{
		emitter a;                       // mov_x copies a full 64-bit pointer
		a.mov_imm64(X1, u64(0x1122334455667788ull));
		a.mov_x(X0, X1);
		a.ret();
		CHECK64(0x1122334455667788ull);
	}
	{
		emitter a;                       // cbz_x on a nonzero pointer falls through
		a.mov_imm64(X1, u64(0x1234));
		const size_t skip = a.cbz_x(X1);
		a.movz(W0, 42, 0);
		a.patch(skip);
		a.ret();
		CHECK32(42u);
	}
	{
		emitter a;                       // tst_imm sees the low bit
		a.mov_imm32(W1, 0x80000001);
		a.tst_imm(W1, 1);
		a.cset(W0, NE);
		a.ret();
		CHECK32(1u);
	}
	{
		emitter a;                       // tst_imm over a 2-bit mask (both bits clear -> EQ)
		a.mov_imm32(W1, 0x80000001);
		a.tst_imm(W1, 0x300);
		a.cset(W0, EQ);
		a.ret();
		CHECK32(1u);
	}
	{
		emitter a;                       // and_imm / or_imm with encodable masks
		a.mov_imm32(W1, 0x80000001);
		a.and_imm(W0, W1, 0xfffe);
		a.or_imm(W0, W0, 0x300);
		a.ret();
		CHECK32((0x80000001u & 0xfffeu) | 0x300u);
	}
	{
		emitter a;                       // wrapping-run mask (0x80000001 itself is encodable)
		a.mov_imm32(W1, 0x1234);
		a.and_imm(W0, W1, 0x80000001);
		a.ret();
		CHECK32(0x1234u & 0x80000001u);
	}
	{
		emitter a;                       // generic blr through x16 (x17 is covered above)
		a.movz(W0, 5, 0);
		a.mov_imm64(X16, reinterpret_cast<u64>(&add7));
		a.stp_x(X29, X30, X31, -16, true);   // frame: BLR writes x30
		a.blr(X16);
		a.ldp_x(X29, X30, X31, 16, true);
		a.ret();
		CHECK32(12u);
	}

	// ---- 64-bit data processing (the MEG JIT's p accumulator lives here) ----
	{
		emitter a;                       // add_x / sub_x round trip through the upper word
		a.mov_imm64(X1, u64(0x0123456789abcdefull));
		a.mov_imm32(X2, 0x1111);
		a.add_x(X0, X1, X2);
		a.sub_x(X0, X0, X2);
		a.ret();
		CHECK64(0x0123456789abcdefull);
	}
	{
		emitter a;                       // neg_x then mul_x
		a.mov_imm32(X1, 9);
		a.neg_x(X2, X1);
		a.mov_imm32(X3, 7);
		a.mul_x(X0, X2, X3);             // low 64 bits of (-9) * 7
		a.ret();
		CHECK64(u64(-63));
	}
	{
		emitter a;                       // and_x / orr_x / eor_x
		a.mov_imm64(X1, u64(0x00000000ffff0000ull));
		a.mov_imm32(X2, 0x00ff00ff);
		a.and_x(X3, X1, X2);
		a.orr_x(X4, X3, X2);
		a.eor_x(X0, X4, X1);
		a.ret();
		CHECK64((((0xffff0000ull & 0x00ff00ffull) | 0x00ff00ffull) ^ 0xffff0000ull));
	}
	{
		emitter a;                       // 64-bit immediate shifts: lsr / asr / lsl
		a.mov_imm64(X1, u64(0x0000000400000080ull));
		a.lsr_imm_x(X2, X1, 8);          // 0x0000000004000000
		a.lsl_imm_x(X3, X2, 4);          // 0x0000000040000000
		a.mov_imm64(X4, u64(0x8000000000000000ull));
		a.asr_imm_x(X5, X4, 62);         // -2^63 >> 62 = -2, sign-filled
		a.add_x(X0, X3, X5);
		a.ret();
		CHECK64(0x0000000040000000ull + u64(-2));
	}
	{
		emitter a;                       // 64-bit variable shifts: asrv fills, lsrv does not
		a.mov_imm64(X1, u64(0x8000000000000100ull));
		a.mov_imm32(X2, 8);
		a.asrv_x(X3, X1, X2);
		a.lsrv_x(X4, X1, X2);
		a.eor_x(X0, X3, X4);
		a.ret();
		CHECK64(u64(s64(0x8000000000000100ull) >> 8) ^ (0x8000000000000100ull >> 8));
	}
	{
		emitter a;                       // 64-bit variable lslv
		a.mov_imm32(X1, 3);
		a.mov_imm32(X2, 40);
		a.lslv_x(X0, X1, X2);
		a.ret();
		CHECK64(3ull << 40);
	}
	{
		emitter a;                       // cmp_x + csel_x, tst_x + csel_x
		a.mov_imm32(X1, 3);
		a.mov_imm32(X2, 9);
		a.cmp_x(X1, X2);
		a.csel_x(X3, X2, X1, LT);        // X1 < X2 -> X2
		a.tst_x(X1, X1);
		a.csel_x(X0, X1, X3, EQ);        // X1 != 0 -> X3
		a.ret();
		CHECK64(9u);
	}
	{
		emitter a;                       // cneg_x takes the magnitude under a condition
		a.mov_imm32(X1, 6);
		a.neg_x(X2, X1);
		a.cmp_x(X2, WZR);
		a.cneg_x(X0, X2, LT);            // negative -> negate it back
		a.ret();
		CHECK64(6u);
	}
	{
		emitter a;                       // eor_imm over a run of ones
		a.mov_imm32(X1, 0x12345678);
		a.eor_imm(X0, X1, 0x7ffffff);
		a.ret();
		CHECK32(0x12345678u ^ 0x7ffffffu);
	}
	{
		emitter a;                       // ror_imm: 32-bit rotate (rol by 16 == ror by 16)
		a.mov_imm32(W0, 0x12345678);
		a.ror_imm(W0, W0, 16);
		a.ret();
		CHECK32(0x56781234u);
	}
	{
		u8 scratch[16] = {};
		scratch[0] = 0xcd; scratch[1] = 0xab;                       // 0xabcd as s16
		scratch[4] = 0x78; scratch[5] = 0x56; scratch[6] = 0x34; scratch[7] = 0x12;
		emitter a;                       // ldrsh + sxtw64 and ldrsw: both sign-extend to 64
		a.ldrsh(W1, X0, 0);
		a.sxtw64(X1, W1);
		a.ldrsw(X2, X0, 4);
		a.add_x(X0, X1, X2);
		a.ret();
		const s64 want = s64(s16(0xabcd)) + s64(0x12345678);
		CHECK32S(u32(want), scratch);
	}

	// ---- immediate forms that need X16, and scaled register offsets ----------
	{
		emitter a;                       // tst_reg: Z set when the AND is zero
		a.mov_imm32(W1, 0x0f0f0f0f);
		a.mov_imm32(W2, 0xf0f0f0f0);
		a.tst_reg(W1, W2);
		a.cset(W0, EQ);
		a.ret();
		CHECK32(1);
	}
	{
		emitter a;                       // add_imm_any: all three encoding paths
		a.mov_imm32(W1, 0x1000);
		a.add_imm_any(W2, W1, 0x2345);   // plain imm12
		a.add_imm_any(W3, W2, 0x1000);   // imm12 shifted left by 12
		a.add_imm_any(W4, W3, 0x3fc00);  // neither: materialized in X16
		a.mov_reg(W0, W4);
		a.ret();
		CHECK32(0x1000u + 0x2345u + 0x1000u + 0x3fc00u);
	}
	{
		emitter a;                       // logical immediate, encodable and not
		a.mov_imm32(W1, 0x12345678);
		a.and_imm_any(W2, W1, 0x3ff);
		a.and_imm_any(W3, W2, 0xf0f0f0f0);   // alternating: only via X16
		a.or_imm_any(W4, W3, 0x800);
		a.or_imm_any(W5, W4, 0x0f0f0f0f);
		a.eor_imm_any(W6, W5, 0x7ffffff);
		a.eor_imm_any(W0, W6, 0xff00ff00);
		a.ret();
		CHECK32(((((0x12345678u & 0x3ffu) & 0xf0f0f0f0u) | 0x800u) | 0x0f0f0f0fu) ^ 0x7ffffffu ^ 0xff00ff00u);
	}
	{
		emitter a;                       // cmp_imm_any: a constant past imm12
		a.mov_imm32(W1, 0x3fc00);
		a.cmp_imm_any(W1, 0x3fc00);
		a.cset(W0, EQ);
		a.ret();
		CHECK32(1);
	}
	{
		emitter a;                       // add_off: all three encoding paths
		a.mov_imm32(X1, 0x1000);
		a.add_off(X2, X1, 0x123);        // plain imm12
		a.add_off(X3, X2, 0x2000);       // imm12 shifted left by 12
		a.add_off(X4, X3, 5164);         // neither: materialized in X16
		a.mov_x(X0, X4);
		a.ret();
		CHECK64(0x1000u + 0x123u + 0x2000u + 5164u);
	}
	{
		u8 scratch[64] = {};
		emitter a;                       // register offsets scaled by the access size
		a.mov_imm32(W2, 5);
		a.mov_imm32(W1, 0x11223344);
		a.str_w_sr(W1, X0, X2);          // word at 20 (5 * 4)
		a.mov_imm32(W1, 0x3344);
		a.strh_sr(W1, X0, X2);           // halfword at 10 (5 * 2)
		a.mov_imm32(W1, 0x5a);
		a.strb_sr(W1, X0, X2);           // byte at 5
		a.ldr_w_sr(W0, X0, X2);
		a.ret();
		CHECK32S(0x11223344u, scratch);
		if (std::memcmp(scratch + 20, "\x44\x33\x22\x11", 4) != 0) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
		if (scratch[5] != 0x5a) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
		if (scratch[10] != 0x44 || scratch[11] != 0x33) { std::fprintf(stderr, "RAWFAIL %d\n", __LINE__); bad++; }
	}

	return bad;
}

#else // !__aarch64__

// x86-64 builds link verify too; the emitter is unused there, so the selftest
// is an empty stub (verify prints it as "0 mismatches")
u64 a64::selftest() { return 0; }

#endif // __aarch64__
