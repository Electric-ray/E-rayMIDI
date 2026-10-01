// license:BSD-3-Clause
//
// S-MU2000: a small aarch64 (arm64) machine-code emitter. Used by the SH2 JIT
// (sh2_jit.cpp) on arm64 builds, the same role x64asm.h plays on x86-64.
// Only the primitives the JITs need are here; encoding is done by composing
// the architectural bit fields, so each helper mirrors one instruction form.
//
// Register numbers are the architectural 0..30 (w0..w30 / x0..x30), with 31
// meaning ZR or SP depending on the instruction - every helper here takes ZR.
//
// The JIT holds block state in callee-saved registers and calls C helpers, so
// everything emitted must be a leaf of its own frame; no stack forms are
// provided. The selftest below executes each primitive on the host CPU and
// compares against the plain C++ equivalent, so an encoding mistake cannot
// reach the JIT unnoticed.

#ifndef S_MU2000_A64ASM_H
#define S_MU2000_A64ASM_H

#include "mamecompat.h"

#include <cstddef>
#include <vector>

#ifdef _MSC_VER
// MSVC で x86 ビルドするとこのヘッダも通る（verify.cpp が自テスト用に常時 include する）。
// __builtin_popcount は GCC/Clang 専用なので __popcnt で受ける
#include <intrin.h>
#define __builtin_popcount(x) __popcnt(x)
#endif

namespace a64 {

// Architectural register numbers. The JIT pins block state to x19-x22, which
// are callee-saved in the Apple C ABI, so helpers called from a block cannot
// disturb them.
enum : u8 {
	W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14, W15,
	W16, W17, W18, W19, W20, W21, W22, W23, W24, W25, W26, W27, W28, W29, W30, WZR = 31,
};

// 64-bit register numbers share the encoding; the instruction class selects
// the width. X17 is the intra-procedure-call scratch (see mov_imm64_x17).
enum : u8 { X0 = 0, X1 = 1, X2 = 2, X3 = 3, X4 = 4, X5 = 5, X6 = 6, X7 = 7, X8 = 8, X9 = 9,
             X16 = 16, X17 = 17, X19 = 19, X20 = 20, X21 = 21, X22 = 22, X23 = 23, X24 = 24, X25 = 25, X26 = 26, X27 = 27, X28 = 28, X29 = 29, X30 = 30, X31 = 31 };

// Condition codes for B.cond / CSET, plus the aliases CMP/HS and CMP/LO use.
enum : u8 { EQ, NE, CS, HS = CS, CC, LO = CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL };

struct emitter {
	std::vector<u32> code;

	void emit(u32 insn) { code.push_back(insn); }
	size_t here() const { return code.size(); }          // instruction index

	// ---- data processing, shifted register (bit 21 = 0) ----
	// add/sub with optional shifted third operand: Wd = Wn OP Wm << shift
	void add_reg(u32 rd, u32 rn, u32 rm, u32 shift = 0, u32 amount = 0)  { emit(0x0B000000u | rm << 16 | shift << 22 | amount << 10 | rn << 5 | rd); }
	void adds_reg(u32 rd, u32 rn, u32 rm, u32 shift = 0, u32 amount = 0) { emit(0x2B000000u | rm << 16 | shift << 22 | amount << 10 | rn << 5 | rd); }
	void sub_reg(u32 rd, u32 rn, u32 rm, u32 shift = 0, u32 amount = 0)  { emit(0x4B000000u | rm << 16 | shift << 22 | amount << 10 | rn << 5 | rd); }
	void subs_reg(u32 rd, u32 rn, u32 rm, u32 shift = 0, u32 amount = 0) { emit(0x6B000000u | rm << 16 | shift << 22 | amount << 10 | rn << 5 | rd); }

	// ---- logical, shifted register (class bits 28..24 = 01010, N = bit 21) ----
	void and_reg(u32 rd, u32 rn, u32 rm)  { emit(0x0A000000u | rm << 16 | rn << 5 | rd); }
	void orr_reg(u32 rd, u32 rn, u32 rm)  { emit(0x2A000000u | rm << 16 | rn << 5 | rd); }
	void eor_reg(u32 rd, u32 rn, u32 rm)  { emit(0x4A000000u | rm << 16 | rn << 5 | rd); }
	void ands_reg(u32 rd, u32 rn, u32 rm) { emit(0x6A000000u | rm << 16 | rn << 5 | rd); }
	void bic_reg(u32 rd, u32 rn, u32 rm)  { emit(0x0A200000u | rm << 16 | rn << 5 | rd); }
	void orn_reg(u32 rd, u32 rn, u32 rm)  { emit(0x2A200000u | rm << 16 | rn << 5 | rd); }

	// ---- immediate forms (imm12, optionally shifted by 12) ----
	void add_imm(u32 rd, u32 rn, u32 imm, u32 shift12 = 0)  { emit(0x11000000u | shift12 << 22 | imm << 10 | rn << 5 | rd); }
	void adds_imm(u32 rd, u32 rn, u32 imm)                  { emit(0x31000000u | imm << 10 | rn << 5 | rd); }
	void sub_imm(u32 rd, u32 rn, u32 imm)                   { emit(0x51000000u | imm << 10 | rn << 5 | rd); }
	void subs_imm(u32 rd, u32 rn, u32 imm)                  { emit(0x71000000u | imm << 10 | rn << 5 | rd); }

	// 64-bit forms, for pointer arithmetic on x registers
	void add_imm64(u32 rd, u32 rn, u32 imm) { emit(0x91000000u | imm << 10 | rn << 5 | rd); }
	void sub_imm64(u32 rd, u32 rn, u32 imm) { emit(0xD1000000u | imm << 10 | rn << 5 | rd); }

	// Xd = Xn + an arbitrary byte offset. Struct fields sit past the 4095 the
	// plain imm12 form reaches, so this also tries the imm12-shifted-left-by-12
	// form and, failing that, materializes the offset in X16.
	void add_off(u32 xd, u32 xn, u32 off)
	{
		if (off <= 0xfff) {
			add_imm64(xd, xn, off);
			return;
		}
		if ((off & 0xfff) == 0 && (off >> 12) <= 0xfff) {
			emit(0x91000000u | 1u << 22 | (off >> 12) << 10 | xn << 5 | xd);
			return;
		}
		mov_imm64(X16, off);
		add_x(xd, xn, X16);
	}

	// ---- move wide (movn/movz/movk), 16-bit chunk at shift 16*hw ----
	void movz(u32 rd, u32 imm16, u32 hw) { emit(0x52800000u | hw << 21 | imm16 << 5 | rd); }
	void movn(u32 rd, u32 imm16, u32 hw) { emit(0x12800000u | hw << 21 | imm16 << 5 | rd); }
	void movk(u32 rd, u32 imm16, u32 hw) { emit(0x72800000u | hw << 21 | imm16 << 5 | rd); }

	// 32-bit wide moves expressed with movn/movz/movk
	void mov_imm32(u32 rd, u32 v)
	{
		const u32 lo = v & 0xffff, hi = v >> 16;
		if (hi == 0xffff)      movn(rd, ~lo & 0xffff, 0);
		else if (hi == 0)      movz(rd, lo, 0);
		else { movz(rd, lo, 0); movk(rd, hi, 1); }
	}

	// 64-bit constant into an x register. Only the nonzero 16-bit chunks are
	// emitted (a movz for the highest one, movk for the rest), so small
	// values take one instruction instead of four: the MEG JIT materializes
	// constants like 0x800000 per sample, and helper-call addresses only need
	// three chunks on macOS (high chunk is zero).
	void mov_imm64(u32 rd, u64 v)
	{
		int top = -1;
		for (int hw = 3; hw >= 0; hw--)
			if ((v >> (hw * 16)) & 0xffff) { top = hw; break; }
		if (top < 0) { emit(0xD2800000u | 0 << 21 | 0 << 5 | rd); return; }   // movz xd, #0
		emit(0xD2800000u | u32(top) << 21 | u32((v >> (top * 16)) & 0xffff) << 5 | rd);
		for (int hw = top - 1; hw >= 0; hw--) {
			const u32 chunk = u32((v >> (hw * 16)) & 0xffff);
			if (chunk) emit(0xF2800000u | u32(hw) << 21 | chunk << 5 | rd);
		}
	}

	// ---- register moves and extends ----
	void mov_reg(u32 rd, u32 rm)            { orr_reg(rd, WZR, rm); }                       // Wd = Wm
	void mvn_reg(u32 rd, u32 rm)            { emit(0x2A2003E0u | rm << 16 | rd); }          // Wd = ~Wm
	void neg_reg(u32 rd, u32 rn)            { emit(0x4B0003E0u | rn << 16 | rd); }          // Wd = 0 - Wn (SUB with Rn = ZR)
	void sxtb(u32 rd, u32 rn)               { emit(0x13001C00u | rn << 5 | rd); }           // sign-extend byte
	void sxth(u32 rd, u32 rn)               { emit(0x13003C00u | rn << 5 | rd); }
	void uxtb(u32 rd, u32 rn)               { emit(0x53001C00u | rn << 5 | rd); }           // zero-extend byte
	void uxth(u32 rd, u32 rn)               { emit(0x53003C00u | rn << 5 | rd); }
	void sxtw64(u32 xd, u32 wn)             { emit(0x93407C00u | wn << 5 | xd); }           // Xd = sext(Wn)
	void mov_to64(u32 xd, u32 wn)           { emit(0x2A0003E0u | wn << 16 | xd); }          // ORR Wd, WZR, Wn: zero-extends into Xd
	void mov_x(u32 xd, u32 xm)              { emit(0xAA0003E0u | xm << 16 | xd); }          // Xd = Xm

	// ---- variable shifts by register, and rotate-extract helpers ----
	void lslv(u32 rd, u32 rn, u32 rm) { emit(0x1AC02000u | rm << 16 | rn << 5 | rd); }
	void lsrv(u32 rd, u32 rn, u32 rm) { emit(0x1AC02400u | rm << 16 | rn << 5 | rd); }
	void asrv(u32 rd, u32 rn, u32 rm) { emit(0x1AC02800u | rm << 16 | rn << 5 | rd); }
	void rorv(u32 rd, u32 rn, u32 rm) { emit(0x1AC02C00u | rm << 16 | rn << 5 | rd); }

	// immediate shifts expressed with (U/S)BFM
	void lsl_imm(u32 rd, u32 rn, u32 amt) { emit(0x53000000u | ((32 - amt) & 31) << 16 | (31 - amt) << 10 | rn << 5 | rd); }
	void lsr_imm(u32 rd, u32 rn, u32 amt) { emit(0x53000000u | amt << 16 | 31 << 10 | rn << 5 | rd); }
	void sar_imm(u32 rd, u32 rn, u32 amt) { emit(0x13000000u | amt << 16 | 31 << 10 | rn << 5 | rd); }

	// insert low bits of Wn into Wd at bit 0 (used to merge a T bit into SR)
	void bfi0(u32 rd, u32 rn) { emit(0x53000000u | 31 << 16 | rn << 5 | rd); }      // BFI Wd, Wn, #0, #1

	// ---- 64-bit data processing (X registers) ----
	// The MEG JIT keeps its 64-bit p accumulator and its clamps in x registers,
	// so the arithmetic and the conditional selects need the X forms.
	void add_x(u32 rd, u32 rn, u32 rm)  { emit(0x8B000000u | rm << 16 | rn << 5 | rd); }
	void sub_x(u32 rd, u32 rn, u32 rm)  { emit(0xCB000000u | rm << 16 | rn << 5 | rd); }
	void and_x(u32 rd, u32 rn, u32 rm)  { emit(0x8A000000u | rm << 16 | rn << 5 | rd); }
	void orr_x(u32 rd, u32 rn, u32 rm)  { emit(0xAA000000u | rm << 16 | rn << 5 | rd); }
	void eor_x(u32 rd, u32 rn, u32 rm)  { emit(0xCA000000u | rm << 16 | rn << 5 | rd); }
	void neg_x(u32 rd, u32 rn)          { emit(0xCB0003E0u | rn << 16 | rd); }          // SUB Xd, XZR, Xn
	void cmp_x(u32 rn, u32 rm)          { emit(0xEB00001Fu | rm << 16 | rn << 5); }      // SUBS XZR, Xn, Xm
	void tst_x(u32 rn, u32 rm)          { emit(0xEA00001Fu | rm << 16 | rn << 5); }      // ANDS XZR, Xn, Xm
	void mul_x(u32 rd, u32 rn, u32 rm)  { emit(0x9B007C00u | rm << 16 | rn << 5 | rd); }  // MADD, Ra = XZR
	// 64-bit immediate shifts (UBFM/SBFM, same shape as the 32-bit helpers)
	void lsl_imm_x(u32 rd, u32 rn, u32 amt) { emit(0xD3400000u | ((64 - amt) & 63) << 16 | (63 - amt) << 10 | rn << 5 | rd); }
	void lsr_imm_x(u32 rd, u32 rn, u32 amt) { emit(0xD3400000u | amt << 16 | 63 << 10 | rn << 5 | rd); }
	void asr_imm_x(u32 rd, u32 rn, u32 amt) { emit(0x93400000u | amt << 16 | 63 << 10 | rn << 5 | rd); }
	void lslv_x(u32 rd, u32 rn, u32 rm) { emit(0x9AC02000u | rm << 16 | rn << 5 | rd); }
	void lsrv_x(u32 rd, u32 rn, u32 rm) { emit(0x9AC02400u | rm << 16 | rn << 5 | rd); }
	void asrv_x(u32 rd, u32 rn, u32 rm) { emit(0x9AC02800u | rm << 16 | rn << 5 | rd); }
	// Conditional select. "cond" is the arm64 condition that picks Rn; CSNEG is
	// what turns a value into its magnitude (cneg_x) and CSEL is the cmov shape.
	void csel_x(u32 rd, u32 rn, u32 rm, u32 cond)  { emit(0x9A800000u | rm << 16 | cond << 12 | rn << 5 | rd); }
	void csneg_x(u32 rd, u32 rn, u32 rm, u32 cond) { emit(0xDA800400u | rm << 16 | cond << 12 | rn << 5 | rd); }
	// Xd = cond ? -Xn : Xn. CSNEG negates its *false* operand (Xm), so the alias
	// CNEG - which negates its single source - inverts the condition.
	void cneg_x(u32 rd, u32 rn, u32 cond) { csneg_x(rd, rn, rn, cond ^ 1); }

	// ---- multiply / divide ----
	// MADD/MSUB with Ra = WZR: the whole product, nothing added
	void mul(u32 rd, u32 rn, u32 rm)  { emit(0x1B007C00u | rm << 16 | rn << 5 | rd); }        // MUL (MADD, Ra = WZR)
	void mneg(u32 rd, u32 rn, u32 rm) { emit(0x1B00FC00u | rm << 16 | rn << 5 | rd); }        // MNEG (MSUB, Ra = WZR)
	void smull64(u32 xd, u32 wn, u32 wm) { emit(0x9B207C00u | wm << 16 | wn << 5 | xd); }     // SMULL Xd, Wn, Wm
	void umull64(u32 xd, u32 wn, u32 wm) { emit(0x9BA07C00u | wm << 16 | wn << 5 | xd); }
	void sdiv(u32 rd, u32 rn, u32 rm) { emit(0x1AC00C00u | rm << 16 | rn << 5 | rd); }        // no flags; divide by zero gives 0
	void udiv(u32 rd, u32 rn, u32 rm) { emit(0x1AC00800u | rm << 16 | rn << 5 | rd); }

	// bit scans (kept for symmetry with x64asm.h; not used by the JIT today)
	// data-processing (1 source): source register sits in the Rn field
	void rbit(u32 rd, u32 rm) { emit(0x5AC00000u | rm << 5 | rd); }
	void clz(u32 rd, u32 rn)  { emit(0x5AC01000u | rn << 5 | rd); }

	// ---- condition set / compare aliases ----
	void cmp_reg(u32 rn, u32 rm) { subs_reg(WZR, rn, rm); }
	// CMP Wn, #imm. The immediate may sit in imm12 or, when it is a multiple of
	// 4096, in imm12 shifted left by 12. Anything else is not encodable: the
	// caller has to hold it in a register and use cmp_reg, so emit a trap rather
	// than silently comparing against a different value.
	void cmp_imm(u32 rn, u32 imm)
	{
		if (imm <= 0xfff) {
			emit(0x71000000u | imm << 10 | rn << 5 | WZR);
			return;
		}
		if ((imm & 0xfff) == 0 && (imm >> 12) <= 0xfff) {
			emit(0x71400000u | (imm >> 12) << 10 | rn << 5 | WZR);
			return;
		}
		emit(0);                                            // UDF: not encodable
	}
	void cset(u32 rd, u32 cond) { emit(0x1A800400u | (cond ^ 1) << 12 | WZR << 16 | WZR << 5 | rd); }

	// ---- logical immediate (bitmask) forms ------------------------------------------
	// A logical immediate is a run of '1' bits (possibly wrapping) replicated
	// across an element of size esize and rotated. encode_bitmask finds (esize,
	// len, rot) for a value; returns false when the value is not encodable
	// (e.g. 0xfffffcff has two separate zero bits) - callers then fall back to
	// a register-held mask.
	static bool encode_bitmask(u32 v, u32 &nn, u32 &imms, u32 &immr)
	{
		if (v == 0 || v == 0xffffffffu)
			return false;
		for (u32 esize = 2; esize <= 32; esize <<= 1) {
			const u32 mask = esize == 32 ? 0xffffffffu : (1u << esize) - 1;
			if (esize < 32 && (((v >> esize) ^ v) & mask))
				continue;                                   // pattern does not repeat
			const u32 elem = v & mask;
			const u32 len = __builtin_popcount(elem);
			if (len == 0 || len == esize)
				continue;                                   // all zeros / all ones: try a larger element
			// The run of ones may wrap around bit 0, so search the rotation that
			// brings it down to bit 0 (that rotation is exactly immr).
			for (u32 rot = 0; rot < esize; rot++) {
				const u32 rolled = rot == 0 ? elem
				                : esize == 32 ? ((elem << rot) | (elem >> (32 - rot)))
				                              : (((elem << rot) | (elem >> (esize - rot))) & mask);
				if (rolled != (1u << len) - 1)
					continue;
				nn = 0;
				imms = 2 * (32 - esize) + len - 1;
				immr = rot;
				return true;
			}
		}
		return false;
	}

	void and_imm(u32 rd, u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		const bool ok = encode_bitmask(v, nn, imms, immr);
		if (!ok) { emit(0); return; }                       // UDF: callers must pass encodable masks
		emit(0x12000000u | nn << 22 | immr << 16 | imms << 10 | rn << 5 | rd);
	}
	void eor_imm(u32 rd, u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		const bool ok = encode_bitmask(v, nn, imms, immr);
		if (!ok) { emit(0); return; }
		emit(0x52000000u | nn << 22 | immr << 16 | imms << 10 | rn << 5 | rd);
	}
	void or_imm(u32 rd, u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		const bool ok = encode_bitmask(v, nn, imms, immr);
		if (!ok) { emit(0); return; }
		emit(0x32000000u | nn << 22 | immr << 16 | imms << 10 | rn << 5 | rd);
	}
	// TST Wn, #v (ANDS with Rd = ZR)
	void tst_imm(u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		const bool ok = encode_bitmask(v, nn, imms, immr);
		if (!ok) { emit(0); return; }
		emit(0x72000000u | nn << 22 | immr << 16 | imms << 10 | rn << 5 | WZR);
	}
	// TST Wn, Wm
	void tst_reg(u32 rn, u32 rm) { emit(0x6A00001Fu | rm << 16 | rn << 5); }

	// Immediate forms for constants that are known at emit time but not
	// necessarily encodable (register values of the MEG program). X16 holds the
	// constant when the immediate form does not apply.
	void add_imm_any(u32 rd, u32 rn, u32 v)
	{
		if (v <= 0xfff) {
			add_imm(rd, rn, v);
			return;
		}
		if ((v & 0xfff) == 0 && (v >> 12) <= 0xfff) {
			emit(0x11000000u | 1u << 22 | (v >> 12) << 10 | rn << 5 | rd);
			return;
		}
		mov_imm32(X16, v);
		add_reg(rd, rn, X16);
	}
	void and_imm_any(u32 rd, u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		if (encode_bitmask(v, nn, imms, immr)) { and_imm(rd, rn, v); return; }
		mov_imm32(X16, v);
		and_reg(rd, rn, X16);
	}
	void or_imm_any(u32 rd, u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		if (encode_bitmask(v, nn, imms, immr)) { or_imm(rd, rn, v); return; }
		mov_imm32(X16, v);
		orr_reg(rd, rn, X16);
	}
	void eor_imm_any(u32 rd, u32 rn, u32 v)
	{
		u32 nn, imms, immr;
		if (encode_bitmask(v, nn, imms, immr)) { eor_imm(rd, rn, v); return; }
		mov_imm32(X16, v);
		eor_reg(rd, rn, X16);
	}
	void cmp_imm_any(u32 rn, u32 v)
	{
		if (v <= 0xfff || ((v & 0xfff) == 0 && (v >> 12) <= 0xfff)) { cmp_imm(rn, v); return; }
		mov_imm32(X16, v);
		cmp_reg(rn, X16);
	}

	// ---- memory: unsigned-offset (imm12) forms. Offsets are in BYTES; the
	// helper divides by the access size as the encoding requires ----
	void ldrb(u32 rt, u32 rn, u32 off = 0)  { emit(0x39400000u | off << 10 | rn << 5 | rt); }
	void ldrh(u32 rt, u32 rn, u32 off = 0)  { emit(0x79400000u | off / 2 << 10 | rn << 5 | rt); }
	void ldrsh(u32 rt, u32 rn, u32 off = 0) { emit(0x79C00000u | off / 2 << 10 | rn << 5 | rt); }   // signed halfword
	void ldr_w(u32 rt, u32 rn, u32 off = 0) { emit(0xB9400000u | off / 4 << 10 | rn << 5 | rt); }
	void ldrsw(u32 rt, u32 rn, u32 off = 0) { emit(0xB9800000u | off / 4 << 10 | rn << 5 | rt); }   // Xt = sext32(word)
	void ldr_x(u32 rt, u32 rn, u32 off = 0) { emit(0xF9400000u | off / 8 << 10 | rn << 5 | rt); }
	void strb(u32 rt, u32 rn, u32 off = 0)  { emit(0x39000000u | off << 10 | rn << 5 | rt); }
	void strh(u32 rt, u32 rn, u32 off = 0)  { emit(0x79000000u | off / 2 << 10 | rn << 5 | rt); }
	void str_w(u32 rt, u32 rn, u32 off = 0) { emit(0xB9000000u | off / 4 << 10 | rn << 5 | rt); }
	void str_x(u32 rt, u32 rn, u32 off = 0) { emit(0xF9000000u | off / 8 << 10 | rn << 5 | rt); }

	// Large fixed offsets (struct fields beyond imm12's reach). The *_big forms
	// take a byte offset, scale it here, and return the instruction index; when
	// the offset is only known after emitting, pass 0 and repair the field with
	// fix12(at, off / size). Offsets must stay below size * 4096 - the field is
	// silently truncated otherwise.
	size_t ldr_w_big(u32 rt, u32 rn, u32 bytes) { const size_t t = here(); emit(0xB9400000u | bytes / 4 << 10 | rn << 5 | rt); return t; }
	size_t ldr_x_big(u32 rt, u32 rn, u32 bytes) { const size_t t = here(); emit(0xF9400000u | bytes / 8 << 10 | rn << 5 | rt); return t; }
	size_t str_w_big(u32 rt, u32 rn, u32 bytes) { const size_t t = here(); emit(0xB9000000u | bytes / 4 << 10 | rn << 5 | rt); return t; }
	size_t str_x_big(u32 rt, u32 rn, u32 bytes) { const size_t t = here(); emit(0xF9000000u | bytes / 8 << 10 | rn << 5 | rt); return t; }
	void fix12(size_t at, u32 scaled) { code[at] = (code[at] & ~0x3ffc00u) | (scaled & 0xfff) << 10; }

	// ---- memory: register offset [Xn, Xm], option LSL, no shift ----
	void ldrb_r(u32 rt, u32 rn, u32 xm)  { emit(0x38606800u | xm << 16 | rn << 5 | rt); }
	void ldrh_r(u32 rt, u32 rn, u32 xm)  { emit(0x78606800u | xm << 16 | rn << 5 | rt); }
	void ldr_w_r(u32 rt, u32 rn, u32 xm) { emit(0xB8606800u | xm << 16 | rn << 5 | rt); }
	void ldr_x_r(u32 rt, u32 rn, u32 xm) { emit(0xF8606800u | xm << 16 | rn << 5 | rt); }
	void strb_r(u32 rt, u32 rn, u32 xm)  { emit(0x38206800u | xm << 16 | rn << 5 | rt); }
	void strh_r(u32 rt, u32 rn, u32 xm)  { emit(0x78206800u | xm << 16 | rn << 5 | rt); }
	void str_w_r(u32 rt, u32 rn, u32 xm) { emit(0xB8206800u | xm << 16 | rn << 5 | rt); }
	void str_x_r(u32 rt, u32 rn, u32 xm) { emit(0xF8206800u | xm << 16 | rn << 5 | rt); }

	// ---- memory: post-index [Xn], #imm9 (option 01: writeback) ----
	void ldrb_post(u32 rt, u32 rn, s32 imm9)  { emit(0x38400400u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }
	void strb_post(u32 rt, u32 rn, s32 imm9)  { emit(0x38000400u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }
	void ldrh_post(u32 rt, u32 rn, s32 imm9)  { emit(0x78400400u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }
	void strh_post(u32 rt, u32 rn, s32 imm9)  { emit(0x78000400u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }
	void ldr_w_post(u32 rt, u32 rn, s32 imm9) { emit(0xB8400400u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }
	void str_w_post(u32 rt, u32 rn, s32 imm9) { emit(0xB8000400u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }

	// ---- memory: pre-index [Xn, #imm9]! ----
	void ldr_w_pre(u32 rt, u32 rn, s32 imm9) { emit(0xB8400C00u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }
	void str_w_pre(u32 rt, u32 rn, s32 imm9) { emit(0xB8000C00u | u32(imm9 & 0x1ff) << 12 | rn << 5 | rt); }

	// ---- byte order (big-endian data on a little-endian host) ----
	void rev16(u32 rd, u32 rn) { emit(0x5AC00400u | rn << 5 | rd); }   // swap bytes within each 16-bit half
	void rev32(u32 rd, u32 rn) { emit(0x5AC00800u | rn << 5 | rd); }   // full 32-bit byte reverse
	// EXTR Wd, Wn, Wn, #amt = ROR Wd, Wn, #amt (no immediate rotate in arm64)
	void ror_imm(u32 rd, u32 rn, u32 amt) { emit(0x13800000u | rn << 16 | amt << 10 | rn << 5 | rd); }

	// ---- register-offset memory with index extend option ----
	// option 2 = UXTW, 6 = SXTW; index is zero/sign-extended from the 32-bit
	// register before use, never shifted (the SH2 uses byte addressing)
	void ldrb_x(u32 rt, u32 xn, u32 wm, bool sxtw)  { emit(0x38600800u | (sxtw ? 6u : 2u) << 13 | wm << 16 | xn << 5 | rt); }
	void ldrh_x(u32 rt, u32 xn, u32 wm, bool sxtw)  { emit(0x78600800u | (sxtw ? 6u : 2u) << 13 | wm << 16 | xn << 5 | rt); }
	void ldr_w_x(u32 rt, u32 xn, u32 wm, bool sxtw) { emit(0xB8600800u | (sxtw ? 6u : 2u) << 13 | wm << 16 | xn << 5 | rt); }
	void strb_x(u32 rt, u32 xn, u32 wm, bool sxtw)  { emit(0x38200800u | (sxtw ? 6u : 2u) << 13 | wm << 16 | xn << 5 | rt); }
	void strh_x(u32 rt, u32 xn, u32 wm, bool sxtw)  { emit(0x78200800u | (sxtw ? 6u : 2u) << 13 | wm << 16 | xn << 5 | rt); }
	void str_w_x(u32 rt, u32 xn, u32 wm, bool sxtw) { emit(0xB8200800u | (sxtw ? 6u : 2u) << 13 | wm << 16 | xn << 5 | rt); }
	// LDR Xt, [Xn, Wm, UXTW #3]: pointer tables indexed by a 32-bit word offset
	void ldr_x_uxtw3(u32 rt, u32 xn, u32 wm) { emit(0xF8600800u | 2u << 13 | 1u << 12 | wm << 16 | xn << 5 | rt); }

	// ---- memory: register offset [Xn, Xm, LSL #log2(size)] ----
	// option 011 with S = 1 scales the index by the access size, which is the
	// shape the JITs use for a[i] (S = 0 would add the index unscaled).
	void ldrb_sr(u32 rt, u32 rn, u32 xm)  { emit(0x38607800u | xm << 16 | rn << 5 | rt); }
	void ldrh_sr(u32 rt, u32 rn, u32 xm)  { emit(0x78607800u | xm << 16 | rn << 5 | rt); }
	void ldr_w_sr(u32 rt, u32 rn, u32 xm) { emit(0xB8607800u | xm << 16 | rn << 5 | rt); }
	void ldr_x_sr(u32 rt, u32 rn, u32 xm) { emit(0xF8607800u | xm << 16 | rn << 5 | rt); }
	void strb_sr(u32 rt, u32 rn, u32 xm)  { emit(0x38207800u | xm << 16 | rn << 5 | rt); }
	void strh_sr(u32 rt, u32 rn, u32 xm)  { emit(0x78207800u | xm << 16 | rn << 5 | rt); }
	void str_w_sr(u32 rt, u32 rn, u32 xm) { emit(0xB8207800u | xm << 16 | rn << 5 | rt); }
	void str_x_sr(u32 rt, u32 rn, u32 xm) { emit(0xF8207800u | xm << 16 | rn << 5 | rt); }

	// ---- stack pairs (Apple ABI prologue/epilogue shapes used by the JIT) ----
	// imm7 is in BYTES and must be a multiple of 8; pre/post select the
	// writeback addressing modes. Bits: mode 25-23 (010 signed, 011 pre,
	// 001 post), L = 22 (load), imm7 = 21-15, Rt2 = 14-10, Rn = 9-5, Rt = 4-0
	void stp_x(u32 rt1, u32 rt2, u32 rn, s32 imm7_bytes, bool pre)  { emit((pre ? 0xA9800000u : 0xA9000000u) | u32(imm7_bytes / 8 & 0x7f) << 15 | rt2 << 10 | rn << 5 | rt1); }
	void ldp_x(u32 rt1, u32 rt2, u32 rn, s32 imm7_bytes, bool post) { emit((post ? 0xA8C00000u : 0xA9C00000u) | u32(imm7_bytes / 8 & 0x7f) << 15 | rt2 << 10 | rn << 5 | rt1); }

	// ---- branches and returns ----
	// Forward references return a patch token (the placeholder instruction's
	// index); patch() fills in "here", patch_to() an explicit target.
	size_t b_cond(u32 cond) { const size_t t = here(); emit(0x54000000u | cond); return t; }
	size_t b()              { const size_t t = here(); emit(0x14000000u); return t; }
	size_t cbz_w(u32 rt)    { const size_t t = here(); emit(0x34000000u | rt); return t; }
	size_t cbnz_w(u32 rt)   { const size_t t = here(); emit(0x35000000u | rt); return t; }
	size_t cbz_x(u32 rt)    { const size_t t = here(); emit(0xB4000000u | rt); return t; }
	size_t tbz(u32 rt, u32 bit)  { const size_t t = here(); emit(0x36000000u | bit << 19 | rt); return t; }
	size_t tbnz(u32 rt, u32 bit) { const size_t t = here(); emit(0x37000000u | bit << 19 | rt); return t; }

	void patch_to(size_t token, size_t target)
	{
		const s32 off = s32(target) - s32(token);
		const u32 w = code[token];
		if ((w & 0xfe000000u) == 0x54000000u)          // B.cond, imm19
			code[token] = (w & ~0x00ffffe0u) | (u32(off & 0x7ffff) << 5);
		else if ((w & 0x7e000000u) == 0x34000000u)     // CBZ/CBNZ, imm19
			code[token] = (w & ~0x00ffffe0u) | (u32(off & 0x7ffff) << 5);
		else if ((w & 0x7e000000u) == 0x36000000u)     // TBZ/TBNZ, imm14 at bits 18-5
			code[token] = (w & ~0x0007ffe0u) | (u32(off & 0x3fff) << 5);
		else                                           // B, imm26
			code[token] = (w & ~0x03ffffffu) | u32(off & 0x3ffffff);
	}
	void patch(size_t token) { patch_to(token, here()); }

	// Calls from generated code go through X17: load the absolute address with
	// mov_imm64, then BLR (call) or BR (tail-jump). Plain absolute moves beat
	// ADRP+ADD here: no fixups, no page-size assumptions, and the JIT calls
	// helpers rarely enough that the two extra instructions do not matter.
	void mov_imm64_x17(u64 v) { mov_imm64(X17, v); }
	void blr(u32 rn) { emit(0xD63F0000u | rn << 5); }
	void br(u32 rn)  { emit(0xD61F0000u | rn << 5); }
	void blr_x17() { blr(X17); }
	void br_x17()  { br(X17); }
	void ret()     { emit(0xD65F03C0u); }
};

// ---- selftest ---------------------------------------------------------------
// Executes each primitive on the host CPU and compares against the plain C++
// equivalent. Returns the number of mismatches; 0 means every encoding used by
// the JIT is correct. Runs once, from verify.

u64 selftest();

} // namespace a64

#endif
