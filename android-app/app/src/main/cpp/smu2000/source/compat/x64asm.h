// license:BSD-3-Clause
//
// S-MU2000: 小さな x86 の組み立て器。MEG の JIT（swp30_jit.cpp）と SH2 の JIT（sh2_jit.cpp）で使う。
// デュアルモード（CPU32_LEDGER.md の Phase 2 で追加）:
//   * x86-64（_M_X64 / __x86_64__）: 従来どおり。出す機械語は従来と 1 バイトも変えない。
//   * x86-32（_M_IX86 / __i386__）: cdecl。REX は絶対に出さない（R8..R15 は無い＝ enums からも無い）。
//     64bit の命令は (lo,hi) のレジスタ組（ペア）を明示で受ける形になる（Phase 3/5 が使う一覧と
//     約束事は CPU32_LEDGER.md 「Phase 2 result」を読むこと）。
//   * それ以外の環境では使えない（#error）。
// 注意: Windows x64 の呼び出し規約（rcx,rdx,r8,r9＋影領域）は 32bit では cdecl（[esp+4]..、影なし）。
//       32bit モードで i386 と x64 をまたいで共有するコマンドは呼ぶ側で#ifで分けること。

#ifndef S_MU2000_X64ASM_H
#define S_MU2000_X64ASM_H

#pragma once

#include "mamecompat.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
	#define SMU_X64ASM_MODE 64
#elif defined(__i386__) || defined(_M_IX86)
	#define SMU_X64ASM_MODE 32
#else
	#error "x64asm.h: x86-64 か x86-32 でだけ使うこと"
#endif

namespace x64asm {

#if SMU_X64ASM_MODE == 64

enum : u8 { RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15, NOREG = 0xff };

// Callee argument registers 1-4. Windows x64: rcx/rdx/r8/r9; SysV: rdi/rsi/rdx/rcx
#ifdef _WIN32
constexpr u8 ARG0 = RCX, ARG1 = RDX, ARG2 = R8, ARG3 = R9;
constexpr bool sysv_abi = false;
#else
constexpr u8 ARG0 = RDI, ARG1 = RSI, ARG2 = RDX, ARG3 = RCX;
constexpr bool sysv_abi = true;
#endif

// Type to receive the first argument on the C++ side (for the selftest's function
// pointers; the return value is rax under either ABI, so it does not differ)
#ifdef _WIN32
using x64_arg0_t = u32;   // ecx; the caller knows the callee ignores the upper 32 bits
#else
using x64_arg0_t = u64;   // rdi
#endif

struct mem {
	u8 base;
	u8 index = NOREG;
	u8 scale = 1;
	s32 disp = 0;
};

class assembler
{
public:
	std::vector<u8> code;

	void byte(u8 b) { code.push_back(b); }
	void d32(u32 v) { for (int i = 0; i < 4; i++) byte(u8(v >> (8 * i))); }
	void d64(u64 v) { for (int i = 0; i < 8; i++) byte(u8(v >> (8 * i))); }

	// 命令の本体。prefix（0 なら無し）、REX.W、命令バイト列、ModRM の reg 欄、相手
	void rr(u8 prefix, bool w, std::initializer_list<u8> opc, u8 reg, u8 rm)
	{
		if (prefix) byte(prefix);
		u8 rex = 0x40 | (w ? 8 : 0) | ((reg >> 3) & 1) << 2 | ((rm >> 3) & 1);
		if (rex != 0x40) byte(rex);
		for (u8 o : opc) byte(o);
		byte(u8(0xc0 | ((reg & 7) << 3) | (rm & 7)));
	}
	void rm(u8 prefix, bool w, std::initializer_list<u8> opc, u8 reg, const mem &m)
	{
		if (prefix) byte(prefix);
		const u8 x = m.index == NOREG ? 0 : (m.index >> 3) & 1;
		u8 rex = 0x40 | (w ? 8 : 0) | ((reg >> 3) & 1) << 2 | x << 1 | ((m.base >> 3) & 1);
		if (rex != 0x40) byte(rex);
		for (u8 o : opc) byte(o);
		// いつも disp32 の形にする（長さが決まっていて楽）
		if (m.index == NOREG && (m.base & 7) != RSP) {
			byte(u8(0x80 | ((reg & 7) << 3) | (m.base & 7)));
		} else {
			byte(u8(0x80 | ((reg & 7) << 3) | 4));
			const u8 ss = m.scale == 1 ? 0 : m.scale == 2 ? 1 : m.scale == 4 ? 2 : 3;
			const u8 idx = m.index == NOREG ? 4 : (m.index & 7);
			byte(u8((ss << 6) | (idx << 3) | (m.base & 7)));
		}
		d32(u32(m.disp));
	}

	void mov64(u8 d, u8 s)            { rr(0, true, {0x8b}, d, s); }
	void load64(u8 d, const mem &m)   { rm(0, true, {0x8b}, d, m); }
	void store64(const mem &m, u8 s)  { rm(0, true, {0x89}, s, m); }
	void load32(u8 d, const mem &m)   { rm(0, false, {0x8b}, d, m); }
	void store32(const mem &m, u8 s)  { rm(0, false, {0x89}, s, m); }
	void loads32(u8 d, const mem &m)  { rm(0, true, {0x63}, d, m); }          // movsxd
	void loads16(u8 d, const mem &m)  { rm(0, true, {0x0f, 0xbf}, d, m); }    // movsx r64, m16
	void loadu16(u8 d, const mem &m)  { rm(0, false, {0x0f, 0xb7}, d, m); }   // movzx r32, m16
	void loadu8(u8 d, const mem &m)   { rm(0, false, {0x0f, 0xb6}, d, m); }   // movzx r32, m8
	void store16(const mem &m, u8 s)  { rm(0x66, false, {0x89}, s, m); }
	void store8i(const mem &m, u8 v)  { rm(0, false, {0xc6}, 0, m); byte(v); }
	void imm64(u8 d, u64 v)
	{
		byte(u8(0x48 | ((d >> 3) & 1)));
		byte(u8(0xb8 | (d & 7)));
		d64(v);
	}
	void imm32(u8 d, u32 v)
	{
		if (d >= 8) byte(0x41);
		byte(u8(0xb8 | (d & 7)));
		d32(v);
	}
	void add64(u8 d, u8 s) { rr(0, true, {0x01}, s, d); }
	void sub64(u8 d, u8 s) { rr(0, true, {0x29}, s, d); }
	void and64(u8 d, u8 s) { rr(0, true, {0x21}, s, d); }
	void cmp64(u8 a, u8 b) { rr(0, true, {0x39}, b, a); }          // cmp a, b
	void test64(u8 a, u8 b) { rr(0, true, {0x85}, b, a); }
	void test32(u8 a, u8 b) { rr(0, false, {0x85}, b, a); }
	void xor32(u8 d, u8 s) { rr(0, false, {0x31}, s, d); }
	void add32(u8 d, u8 s) { rr(0, false, {0x01}, s, d); }
	void sub32(u8 d, u8 s) { rr(0, false, {0x29}, s, d); }
	void and32(u8 d, u8 s) { rr(0, false, {0x21}, s, d); }
	void imul64(u8 d, u8 s) { rr(0, true, {0x0f, 0xaf}, d, s); }
	void imul32i(u8 d, u8 s, u32 v) { rr(0, false, {0x69}, d, s); d32(v); }
	void imul64i(u8 d, u8 s, u32 v) { rr(0, true, {0x69}, d, s); d32(v); }
	void add32i(u8 d, u32 v) { rr(0, false, {0x81}, 0, d); d32(v); }
	void and32i(u8 d, u32 v) { rr(0, false, {0x81}, 4, d); d32(v); }
	void shl64(u8 d, u8 n) { rr(0, true, {0xc1}, 4, d); byte(n); }
	void sar64(u8 d, u8 n) { rr(0, true, {0xc1}, 7, d); byte(n); }
	void shl32(u8 d, u8 n) { rr(0, false, {0xc1}, 4, d); byte(n); }
	void sar32(u8 d, u8 n) { rr(0, false, {0xc1}, 7, d); byte(n); }
	void rol32(u8 d, u8 n) { rr(0, false, {0xc1}, 0, d); byte(n); }
	void neg64(u8 d) { rr(0, true, {0xf7}, 3, d); }
	void cmovl64(u8 d, u8 s) { rr(0, true, {0x0f, 0x4c}, d, s); }
	void cmovg64(u8 d, u8 s) { rr(0, true, {0x0f, 0x4f}, d, s); }
	void cmovs64(u8 d, u8 s) { rr(0, true, {0x0f, 0x48}, d, s); }
	void cmove64(u8 d, u8 s) { rr(0, true, {0x0f, 0x44}, d, s); }
	void cmp64ri(u8 d, u32 v) { rr(0, true, {0x81}, 7, d); d32(v); }
	void setl_mem(const mem &m) { rm(0, false, {0x0f, 0x9c}, 0, m); }
	void sete_mem(const mem &m) { rm(0, false, {0x0f, 0x94}, 0, m); }
	void call_reg(u8 r) { rr(0, false, {0xff}, 2, r); }
	void push(u8 r) { if (r >= 8) byte(0x41); byte(u8(0x50 | (r & 7))); }
	void pop(u8 r)  { if (r >= 8) byte(0x41); byte(u8(0x58 | (r & 7))); }
	void subrsp(u32 v) { rr(0, true, {0x81}, 5, RSP); d32(v); }
	void addrsp(u32 v) { rr(0, true, {0x81}, 0, RSP); d32(v); }
	void ret() { byte(0xc3); }
	// jz で先へ飛ぶ。飛び先は後で patch() で埋める
	size_t jz_fwd() { byte(0x0f); byte(0x84); d32(0); return code.size(); }
	void patch(size_t at) { const u32 rel = u32(code.size() - at); std::memcpy(&code[at - 4], &rel, 4); }
	void call_abs(void *fn) { imm64(RAX, u64(uintptr_t(fn))); call_reg(RAX); }

	// ---- SH2 の JIT で足したもの ----
	void store32i(const mem &m, u32 v) { rm(0, false, {0xc7}, 0, m); d32(v); }   // mov dword [m], imm32
	void cmp32i_mem(const mem &m, u32 v) { rm(0, false, {0x81}, 7, m); d32(v); } // cmp dword [m], imm32
	void sub32i_mem(const mem &m, u32 v) { rm(0, false, {0x81}, 5, m); d32(v); } // sub dword [m], imm32
	// 条件分岐（cc は 0x84 = je、0x85 = jne、0x8e = jle など、0F の後ろのバイト）。飛び先は後で patch() で埋める
	size_t jcc_fwd(u8 cc) { byte(0x0f); byte(cc); d32(0); return code.size(); }
	size_t jmp_fwd() { byte(0xe9); d32(0); return code.size(); }
	// at（jcc_fwd / jmp_fwd の戻り値）の飛び先を target にする
	void patch_to(size_t at, size_t target) { const u32 rel = u32(target - at); std::memcpy(&code[at - 4], &rel, 4); }
	void mov32(u8 d, u8 s) { rr(0, false, {0x8b}, d, s); }
	// r32 と [m] の演算（結果は r32 か [m]）
	void add32rm(u8 d, const mem &m) { rm(0, false, {0x03}, d, m); }
	void cmp32rm(u8 d, const mem &m) { rm(0, false, {0x3b}, d, m); }     // cmp r, [m]
	void test32rm(u8 d, const mem &m) { rm(0, false, {0x85}, d, m); }
	void add32mr(const mem &m, u8 s) { rm(0, false, {0x01}, s, m); }     // add [m], r
	void sub32mr(const mem &m, u8 s) { rm(0, false, {0x29}, s, m); }
	void and32mr(const mem &m, u8 s) { rm(0, false, {0x21}, s, m); }
	void or32mr(const mem &m, u8 s)  { rm(0, false, {0x09}, s, m); }
	void xor32mr(const mem &m, u8 s) { rm(0, false, {0x31}, s, m); }
	void add32i_mem(const mem &m, u32 v) { rm(0, false, {0x81}, 0, m); d32(v); }
	void or32i_mem(const mem &m, u32 v)  { rm(0, false, {0x81}, 1, m); d32(v); }
	void and32i_mem(const mem &m, u32 v) { rm(0, false, {0x81}, 4, m); d32(v); }
	void xor32i_mem(const mem &m, u32 v) { rm(0, false, {0x81}, 6, m); d32(v); }
	void test32i_mem(const mem &m, u32 v) { rm(0, false, {0xf7}, 0, m); d32(v); }
	void shl32i_mem(const mem &m, u8 n) { rm(0, false, {0xc1}, 4, m); byte(n); }
	void shr32i_mem(const mem &m, u8 n) { rm(0, false, {0xc1}, 5, m); byte(n); }
	void loads8_32(u8 d, const mem &m)  { rm(0, false, {0x0f, 0xbe}, d, m); }  // movsx r32, byte [m]
	void loads16_32(u8 d, const mem &m) { rm(0, false, {0x0f, 0xbf}, d, m); }  // movsx r32, word [m]
	void store8(const mem &m, u8 s) { rm(0, false, {0x88}, s, m); }            // s は AL/CL/DL/BL か R8B 以上
	void movzx8(u8 d, u8 s)  { rr(0, false, {0x0f, 0xb6}, d, s); }
	void movzx16(u8 d, u8 s) { rr(0, false, {0x0f, 0xb7}, d, s); }
	void movsx8(u8 d, u8 s)  { rr(0, false, {0x0f, 0xbe}, d, s); }
	void movsx16(u8 d, u8 s) { rr(0, false, {0x0f, 0xbf}, d, s); }
	void imul32(u8 d, u8 s) { rr(0, false, {0x0f, 0xaf}, d, s); }
	void add32ri(u8 d, u32 v) { rr(0, false, {0x81}, 0, d); d32(v); }
	void sub32ri(u8 d, u32 v) { rr(0, false, {0x81}, 5, d); d32(v); }
	void cmp32ri(u8 d, u32 v) { rr(0, false, {0x81}, 7, d); d32(v); }
	void test32ri(u8 d, u32 v) { rr(0, false, {0xf7}, 0, d); d32(v); }
	void shr32(u8 d, u8 n) { rr(0, false, {0xc1}, 5, d); byte(n); }
	void neg32(u8 d) { rr(0, false, {0xf7}, 3, d); }
	void not32(u8 d) { rr(0, false, {0xf7}, 2, d); }
	void bswap32(u8 r) { if (r >= 8) byte(0x41); byte(0x0f); byte(u8(0xc8 | (r & 7))); }
	// setcc al など（cc は 0x94 = sete、0x9d = setge、0x9f = setg、0x93 = setae、0x97 = seta）
	void setcc(u8 cc, u8 r) { rr(0, false, {0x0f, cc}, 0, r); }
	void lea32(u8 d, const mem &m) { rm(0, false, {0x8d}, d, m); }
	void or32(u8 d, u8 s) { rr(0, false, {0x09}, s, d); }
	void or32ri(u8 d, u32 v)  { rr(0, false, {0x81}, 1, d); d32(v); }
	void xor32ri(u8 d, u32 v) { rr(0, false, {0x81}, 6, d); d32(v); }
	void shl32cl(u8 d) { rr(0, false, {0xd3}, 4, d); }
	void shr32cl(u8 d) { rr(0, false, {0xd3}, 5, d); }
	void bsr32(u8 d, u8 s) { rr(0, false, {0x0f, 0xbd}, d, s); }   // s が 0 のときは使わないこと
};

#else // ------------------------------------------------ x86-32 (cdecl) ----------------

// 32bit には R8..R15 が無い。EAX..EDI の 0〜7 だけ（NOREG だけは 0xff のまま）
enum : u8 { RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI, NOREG = 0xff };
// 同じ値の別名（Phase 3/5 で 32bit 名で書けるように）
inline constexpr u8 EAX = RAX, ECX = RCX, EDX = RDX, EBX = RBX, ESP = RSP, EBP = RBP, ESI = RSI, EDI = RDI;

// cdecl の 1 番目の引数はSTACK なので C++ 側の型はただの u32（x64 版と同じ名前でおける）
using x64_arg0_t = u32;

struct mem {
	u8 base;
	u8 index = NOREG;
	u8 scale = 1;
	s32 disp = 0;
};

[[noreturn]] inline void x32_bad_reg() { std::abort(); }   // 訳し bugs の早期停止（emit 時）

class assembler
{
public:
	std::vector<u8> code;

	// 8 以上のレジスタ番号を渡したら止まる（R8..R15 は 32bit には無い）
	static void chk(u8 r) { if (r >= 8) x32_bad_reg(); }

	void byte(u8 b) { code.push_back(b); }
	void d32(u32 v) { for (int i = 0; i < 4; i++) byte(u8(v >> (8 * i))); }

	// x64 版と同じ形だが REX を絶対出さない（w 無しコマンドだけ）
	void reg_modrm(u8 reg, u8 rm) { byte(u8(0xc0 | ((reg & 7) << 3) | (rm & 7))); }
	void rr(u8 prefix, std::initializer_list<u8> opc, u8 reg, u8 rm)
	{
		chk(reg); chk(rm);
		if (prefix) byte(prefix);
		for (u8 o : opc) byte(o);
		reg_modrm(reg, rm);
	}
	void rm(u8 prefix, std::initializer_list<u8> opc, u8 reg, const mem &m)
	{
		chk(reg); chk(m.base); if (m.index != NOREG) chk(m.index);
		if (prefix) byte(prefix);
		for (u8 o : opc) byte(o);
		// いつも disp32 の形にする（長さが決まっていて楽）
		if (m.index == NOREG && (m.base & 7) != RSP) {
			byte(u8(0x80 | ((reg & 7) << 3) | (m.base & 7)));
		} else {
			byte(u8(0x80 | ((reg & 7) << 3) | 4));
			const u8 ss = m.scale == 1 ? 0 : m.scale == 2 ? 1 : m.scale == 4 ? 2 : 3;
			const u8 idx = m.index == NOREG ? 4 : (m.index & 7);
			byte(u8((ss << 6) | (idx << 3) | (m.base & 7)));
		}
		d32(u32(m.disp));
	}
	static mem off(const mem &m, s32 d) { mem r = m; r.disp += d; return r; }

	// ---- そのまま使える 32bit 命令（x64 版と同じ形） ----
	void mov32(u8 d, u8 s)            { rr(0, {0x8b}, d, s); }
	void load32(u8 d, const mem &m)   { rm(0, {0x8b}, d, m); }
	void store32(const mem &m, u8 s)  { rm(0, {0x89}, s, m); }
	void loadu16(u8 d, const mem &m)  { rm(0, {0x0f, 0xb7}, d, m); }   // movzx r32, m16
	void loadu8(u8 d, const mem &m)   { rm(0, {0x0f, 0xb6}, d, m); }   // movzx r32, m8
	void loads16(u8 d, const mem &m)  { rm(0, {0x0f, 0xbf}, d, m); }   // movsx r32, m16（64bit 拡張では無い）
	void store16(const mem &m, u8 s)  { rm(0x66, {0x89}, s, m); }
	void store8(const mem &m, u8 s)   { rm(0, {0x88}, s, m); }
	void store8i(const mem &m, u8 v)  { rm(0, {0xc6}, 0, m); byte(v); }
	void store32i(const mem &m, u32 v) { rm(0, {0xc7}, 0, m); d32(v); }
	void imm32(u8 d, u32 v)           { byte(u8(0xb8 | d)); d32(v); }
	void add32(u8 d, u8 s) { rr(0, {0x01}, s, d); }
	void sub32(u8 d, u8 s) { rr(0, {0x29}, s, d); }
	void and32(u8 d, u8 s) { rr(0, {0x21}, s, d); }
	void or32(u8 d, u8 s)  { rr(0, {0x09}, s, d); }
	void xor32(u8 d, u8 s) { rr(0, {0x31}, s, d); }
	void adc32(u8 d, u8 s) { rr(0, {0x11}, s, d); }
	void sbb32(u8 d, u8 s) { rr(0, {0x19}, s, d); }
	void test32(u8 a, u8 b) { rr(0, {0x85}, b, a); }
	void imul32(u8 d, u8 s) { rr(0, {0x0f, 0xaf}, d, s); }
	void imul32i(u8 d, u8 s, u32 v) { rr(0, {0x69}, d, s); d32(v); }
	void add32i(u8 d, u32 v)  { rr(0, {0x81}, 0, d); d32(v); }
	void sub32ri(u8 d, u32 v) { rr(0, {0x81}, 5, d); d32(v); }
	void and32i(u8 d, u32 v)  { rr(0, {0x81}, 4, d); d32(v); }
	void or32ri(u8 d, u32 v)  { rr(0, {0x81}, 1, d); d32(v); }
	void xor32ri(u8 d, u32 v) { rr(0, {0x81}, 6, d); d32(v); }
	void add32ri(u8 d, u32 v) { rr(0, {0x81}, 0, d); d32(v); }
	void cmp32ri(u8 d, u32 v) { rr(0, {0x81}, 7, d); d32(v); }
	void test32ri(u8 d, u32 v) { rr(0, {0xf7}, 0, d); d32(v); }
	void shl32(u8 d, u8 n) { rr(0, {0xc1}, 4, d); byte(n); }
	void shr32(u8 d, u8 n) { rr(0, {0xc1}, 5, d); byte(n); }
	void sar32(u8 d, u8 n) { rr(0, {0xc1}, 7, d); byte(n); }
	void rol32(u8 d, u8 n) { rr(0, {0xc1}, 0, d); byte(n); }
	void shl32cl(u8 d) { rr(0, {0xd3}, 4, d); }
	void shr32cl(u8 d) { rr(0, {0xd3}, 5, d); }
	void neg32(u8 d) { rr(0, {0xf7}, 3, d); }
	void not32(u8 d) { rr(0, {0xf7}, 2, d); }
	void bswap32(u8 r) { byte(0x0f); byte(u8(0xc8 | r)); }
	void bsr32(u8 d, u8 s) { rr(0, {0x0f, 0xbd}, d, s); }
	void movzx8(u8 d, u8 s)  { rr(0, {0x0f, 0xb6}, d, s); }
	void movzx16(u8 d, u8 s) { rr(0, {0x0f, 0xb7}, d, s); }
	void movsx8(u8 d, u8 s)  { rr(0, {0x0f, 0xbe}, d, s); }
	void movsx16(u8 d, u8 s) { rr(0, {0x0f, 0xbf}, d, s); }
	void loads8_32(u8 d, const mem &m)  { rm(0, {0x0f, 0xbe}, d, m); }
	void loads16_32(u8 d, const mem &m) { rm(0, {0x0f, 0xbf}, d, m); }
	void lea32(u8 d, const mem &m) { rm(0, {0x8d}, d, m); }
	// setcc al など（cc は 0x94 = sete、0x9d = setge、0x9f = setg、0x93 = setae、0x97 = seta）
	// 注意: 32bit（REX 無し）で書き込める 8bit レジスタは AL/CL/DL/BL の 4 つだけ。
	// r=4..7 は AH/CH/DH/BH を壊す（SIL 等を意図しても暗黙に AH になる）ので禁止
	void setcc(u8 cc, u8 r) { if (r >= 4) abort(); rr(0, {0x0f, cc}, 0, r); }
	void setl_mem(const mem &m) { rm(0, {0x0f, 0x9c}, 0, m); }
	void sete_mem(const mem &m) { rm(0, {0x0f, 0x94}, 0, m); }
	// r32 と [m] の演算（x64 版と同じ）
	void add32rm(u8 d, const mem &m) { rm(0, {0x03}, d, m); }
	void cmp32rm(u8 d, const mem &m) { rm(0, {0x3b}, d, m); }
	void test32rm(u8 d, const mem &m) { rm(0, {0x85}, d, m); }
	void add32mr(const mem &m, u8 s) { rm(0, {0x01}, s, m); }
	void sub32mr(const mem &m, u8 s) { rm(0, {0x29}, s, m); }
	void and32mr(const mem &m, u8 s) { rm(0, {0x21}, s, m); }
	void or32mr(const mem &m, u8 s)  { rm(0, {0x09}, s, m); }
	void xor32mr(const mem &m, u8 s) { rm(0, {0x31}, s, m); }
	void add32i_mem(const mem &m, u32 v) { rm(0, {0x81}, 0, m); d32(v); }
	void or32i_mem(const mem &m, u32 v)  { rm(0, {0x81}, 1, m); d32(v); }
	void and32i_mem(const mem &m, u32 v) { rm(0, {0x81}, 4, m); d32(v); }
	void xor32i_mem(const mem &m, u32 v) { rm(0, {0x81}, 6, m); d32(v); }
	void test32i_mem(const mem &m, u32 v) { rm(0, {0xf7}, 0, m); d32(v); }
	void shl32i_mem(const mem &m, u8 n) { rm(0, {0xc1}, 4, m); byte(n); }
	void shr32i_mem(const mem &m, u8 n) { rm(0, {0xc1}, 5, m); byte(n); }
	void cmp32i_mem(const mem &m, u32 v) { rm(0, {0x81}, 7, m); d32(v); }
	void sub32i_mem(const mem &m, u32 v) { rm(0, {0x81}, 5, m); d32(v); }
	// shld/shrd（64bit シフトの部品）。x64 側には無かったもの。imm8 形は 0F A4（shld）/ 0F AC（shrd）
	void shld(u8 d, u8 s, u8 n) { chk(d); chk(s); byte(0x0f); byte(0xa4); reg_modrm(s, d); byte(n); }
	void shrd(u8 d, u8 s, u8 n) { chk(d); chk(s); byte(0x0f); byte(0xac); reg_modrm(s, d); byte(n); }

	void call_reg(u8 r) { rr(0, {0xff}, 2, r); }
	void push(u8 r) { chk(r); byte(u8(0x50 | r)); }
	void pop(u8 r)  { chk(r); byte(u8(0x58 | r)); }
	// cdecl: 影領域は無い。 esp の加減算だけ
	void subrsp(u32 v) { rr(0, {0x81}, 5, RSP); d32(v); }
	void addrsp(u32 v) { rr(0, {0x81}, 0, RSP); d32(v); }
	void ret() { byte(0xc3); }
	size_t jcc_fwd(u8 cc) { byte(0x0f); byte(cc); d32(0); return code.size(); }   // cc は 0F の後ろのバイト
	size_t jz_fwd() { return jcc_fwd(0x84); }
	size_t jmp_fwd() { byte(0xe9); d32(0); return code.size(); }
	void patch(size_t at) { const u32 rel = u32(code.size() - at); std::memcpy(&code[at - 4], &rel, 4); }
	void patch_to(size_t at, size_t target) { const u32 rel = u32(target - at); std::memcpy(&code[at - 4], &rel, 4); }
	// アドレスを EAX に積んでから呼ぶ（B8 id / FF D0）。EAX を壊す（x64 版の call_abs と同じ約束）
	void call_abs(void *fn) { imm32(RAX, u32(uintptr_t(fn))); call_reg(RAX); }

	// ---- 64bit: (lo,hi) のレジスタ組で受ける形。hi が抜けた呼び出し（mov64(d,s,dhi,shi) など）は
	// 呼ぶ側で決める。lo が値の下 32bit、hi が上 32bit（符号含む）。小端（lo が低いアドレス）----

	// (d,dhi) = (s,shi)
	void mov64(u8 d, u8 dhi, u8 s, u8 shi) { mov32(d, s); mov32(dhi, shi); }
	// [m] の 8 バイトを (d,dhi) へ。lo を [m]、hi を [m+4] から
	void load64(u8 d, u8 dhi, const mem &m) { load32(d, m); load32(dhi, off(m, 4)); }
	void store64(const mem &m, u8 s, u8 shi) { store32(m, s); store32(off(m, 4), shi); }
	// (d,dhi) += (s,shi) / -= 。CF は hi 側の adc/sbb のもの（呼び出し側で当てにしないこと）
	void add64(u8 d, u8 dhi, u8 s, u8 shi) { add32(d, s); adc32(dhi, shi); }
	void sub64(u8 d, u8 dhi, u8 s, u8 shi) { sub32(d, s); sbb32(dhi, shi); }
	void and64(u8 d, u8 dhi, u8 s, u8 shi) { and32(d, s); and32(dhi, shi); }   // 後のフラグは不定
	// (d,dhi) の組を imm32 にする（imm64 は 32bit に無い。定数は全部 32bit に収まる前提:
	// アドレスも MEG の定数も収まる）。収まっていなければ止まる
	void mov_imm64(u8 d, u8 dhi, u64 v)
	{
		if (v > u64(0x7fffffff) && v < u64(0xffffffff80000000ull))
			x32_bad_reg();                                // 32bit（符号なし範囲外）に収まらない
		imm32(d, u32(v));
		imm32(dhi, v & 0x80000000ull ? 0xffffffffu : 0u); // 符号拡張
	}
	// 左論理シフト n（0..63）。n<32: lo<<n（下は 0）、hi<<n に lo の溢れを足す（shld は hi を空いた上へ足す形）。
	// n>=32: hi←lo<<(n-32)、lo←0
	void shl64(u8 d, u8 dhi, u8 n)
	{
		if (n >= 64) { x32_bad_reg(); }
		if (n == 0) return;
		if (n < 32) { shld(dhi, d, n); shl32(d, n); }
		else { mov32(dhi, d); if (n > 32) shl32(dhi, u8(n - 32)); xor32(d, d); }
	}
	// 算術右シフト n（0..63）。n<32: lo←(hi:lo)>>n、hi←hi>>n（算術）。n>=32: lo←hi>>(n-32)（算術）、hi←符号
	void sar64(u8 d, u8 dhi, u8 n)
	{
		if (n >= 64) { x32_bad_reg(); }
		if (n == 0) return;
		if (n < 32) { shrd(d, dhi, n); sar32(dhi, n); }
		else { mov32(d, dhi); if (n > 32) sar32(d, u8(n - 32)); sar32(dhi, 31); }
	}
	// (d,dhi) = -(d,dhi)。hi = ~hi + 1 - CF（= lo の借り）なので sbb は -1 を引く。
	// SF は hi の結果から出る（負数判定 cmov/j は hi を見る）
	void neg64(u8 d, u8 dhi) { neg32(d); not32(dhi); rr(0, {0x83}, 3, dhi); byte(0xff); }   // sbb hi,-1
	// (d,dhi) = (s,shi) を同じ条件で両方移動。
	// cmovs64: neg64 の後（SF＝値の符号）や通常の cmp32/test32 の後で使う。
	// cmove64: cmp64/cmp64i の後で使える（ZF はラベルで正しい）。
	// cmovl64/cmovg64: 単純な cmp32 の後だけ。cmp64 の後では使えない（jlt64/jgt64 を使う）
	void cmovcc64(u8 cc, u8 d, u8 dhi, u8 s, u8 shi) { byte(0x0f); rr(0, {cc}, d, s); byte(0x0f); rr(0, {cc}, dhi, shi); }
	void cmovl64(u8 d, u8 dhi, u8 s, u8 shi) { cmovcc64(0x4c, d, dhi, s, shi); }
	void cmovg64(u8 d, u8 dhi, u8 s, u8 shi) { cmovcc64(0x4f, d, dhi, s, shi); }
	void cmovs64(u8 d, u8 dhi, u8 s, u8 shi) { cmovcc64(0x48, d, dhi, s, shi); }
	void cmove64(u8 d, u8 dhi, u8 s, u8 shi) { cmovcc64(0x44, d, dhi, s, shi); }

	// ---- 64bit 比較の約束事（重要）----
	// cmp64/cmp64i/test64 は hi→(違えば jne)→lo の cmp 2 連射。飛び先（ラベル）に落ちた後の
	// フラグは: ZF,CF は常に 64bit の意味で正しい（je/jne/ja/jb/jae/jbe はそのまま使える）。
	// SF,OF は「hi が違った時だけ」正しい（hi が同じ時は lo 引き算のフラグになる）。
	//   → jl/jg/jle/jge（と cmovl/cmovg）は cmp64 の後では使えない。符号比較は下の
	//     jlt64/jgt64（分岐の形。MSVC が 32bit で出す列と形が同じ）を使うこと。
	//     cmovs64/cmove64 は「SF/ZF のだけ見る源（neg64 や通常の cmp32）」の後なら使える
	void cmp64(u8 a, u8 ahi, u8 b, u8 bhi)
	{
		rr(0, {0x39}, bhi, ahi);                          // cmp ahi,bhi
		const size_t j = jcc_fwd(0x85);                   // jne（hi が違う＝そこで決まり）
		rr(0, {0x39}, b, a);                              // cmp alo,blo
		patch(j);
	}
	void cmp64i(u8 a, u8 ahi, u32 bi)                     // b = (bi, bi>>31)（符号拡張の即値）
	{
		if (bi & 0x80000000u)
			cmp32ri(ahi, 0xffffffffu);
		else
			test32(ahi, ahi);                             // hi と 0（je/jne/符号 jcc は cmp と同じになる）
		const size_t j = jcc_fwd(0x85);
		cmp32ri(a, bi);
		patch(j);
	}
	// (a,ahi) と (b,bhi) を符号で比べて「小さい」ときだけ body() を出す（上の約束事の分岐形）
	template <class F>
	void jlt64(u8 a, u8 ahi, u8 b, u8 bhi, F body)
	{
		rr(0, {0x39}, bhi, ahi);
		const size_t end = jcc_fwd(0x8f);                 // jg: hi が勝ってる＝小さくない
		const size_t to = jcc_fwd(0x8c);                  // jl: hi が負ける＝小さい
		rr(0, {0x39}, b, a);
		const size_t end2 = jcc_fwd(0x83);                // jae: hi 同じで lo が負けない＝小さくない
		patch(to);
		body();
		patch(end);
		patch(end2);
	}
	template <class F>
	void jgt64(u8 a, u8 ahi, u8 b, u8 bhi, F body)        // 「大きい」ときだけ body()
	{
		rr(0, {0x39}, bhi, ahi);
		const size_t end = jcc_fwd(0x8c);                 // jl
		const size_t to = jcc_fwd(0x8f);                  // jg
		rr(0, {0x39}, b, a);
		const size_t end2 = jcc_fwd(0x86);                // jbe（rel32）
		patch(to);
		body();
		patch(end);
		patch(end2);
	}
	template <class F>
	void jlt64i(u8 a, u8 ahi, u32 bi, F body)             // b = (bi, bi>>31) と比べて小さいとき body()
	{
		if (bi & 0x80000000u)
			cmp32ri(ahi, 0xffffffffu);
		else
			test32(ahi, ahi);
		const size_t end = jcc_fwd(0x8f);
		const size_t to = jcc_fwd(0x8c);
		cmp32ri(a, bi);
		const size_t end2 = jcc_fwd(0x83);
		patch(to);
		body();
		patch(end);
		patch(end2);
	}
	template <class F>
	void jgt64i(u8 a, u8 ahi, u32 bi, F body)             // b = (bi, bi>>31) と比べて大きいとき body()
	{
		if (bi & 0x80000000u)
			cmp32ri(ahi, 0xffffffffu);
		else
			test32(ahi, ahi);
		const size_t end = jcc_fwd(0x8c);
		const size_t to = jcc_fwd(0x8f);
		cmp32ri(a, bi);
		const size_t end2 = jcc_fwd(0x86);                // jbe（rel32）
		patch(to);
		body();
		patch(end);
		patch(end2);
	}
	// (a,ahi) & (b,bhi)（b が (0,0) なら「値が 0 でないか」）。ZF は常に正しい（je/jne）。
	// SF は hi が違った（0 でなかった）時だけ x64 の test と同じ。符号（flag_n 等）を見たい時は
	// test64 ではなく test32(ahi,ahi) の後で setcc を使うこと（bit31 hi が値の符号そのもの）
	void test64(u8 a, u8 ahi, u8 b, u8 bhi)
	{
		test32(bhi, ahi);                                 // hi & bhi
		const size_t j = jcc_fwd(0x85);                   // jne（hi が 0 でなければ終わり）
		test32(b, a);                                     // lo & blo
		patch(j);
	}
	// (d,dhi) に (s,shi) を掛けた下 64bit（a=(d,dhi)、b=(s,shi)）。
	// d は RAX でなければならない（RAX を使う全幅 mul を使う）。EDX は必ず壊れる。
	// s,shi,dhi は RAX/RDX/ESP 以外のこと（dhi は s,shi と重なってよい）。
	// a*b = a_lo*b_lo + (a_lo*b_hi + a_hi*b_lo)・2^32 (mod 2^64)。
	//   lo = a_lo*b_lo の下、hi = (a_lo*b_lo の上) + (a_lo*b_hi の下) + (a_hi*b_lo の下)（mod 2^32）
	// 注: lo 同士の全幅掛けは符号無しの mul（F7 /4）。F7 /5 と 69 /r は辺を「符号有り」で見るので
	//     lo の bit31 が立った時に上 32bit が狂う（MEG の m<<23 系は実際に立つ）
	void mul_full(u8 s) { chk(s); byte(0xf7); reg_modrm(4, s); }          // edx:eax = eax * s（符号無し）
	void imul64(u8 d, u8 dhi, u8 s, u8 shi)
	{
		if (d != RAX || dhi == RAX || dhi == RDX || dhi == RSP) x32_bad_reg();
		if (s == RAX || s == RDX || s == RSP || shi == RAX || shi == RDX || shi == RSP) x32_bad_reg();
		push(RAX);                    // a_lo を退避
		mov32(RDX, dhi);              // a_hi
		imul32(RDX, s);               // a_hi * b_lo（下だけ。切り捨ては符号不変）
		imul32(RAX, shi);             // a_lo * b_hi（下だけ。a_lo はまだスタックにある）
		add32(RDX, RAX);              // 交差項の和
		push(RDX);                    // [esp+0]=交差項, [esp+4]=a_lo
		load32(RAX, mem{ RSP, NOREG, 1, 4 });   // a_lo
		mul_full(s);                  // a_lo * b_lo（全幅・符号無し）edx:eax
		mov32(dhi, RDX);              // a_lo*b_lo の上
		add32rm(dhi, mem{ RSP, NOREG, 1, 0 }); // + 交差項
		addrsp(8);
	}
	// (d,dhi) に符号拡張 imm32 を掛けた下 64bit。d は RAX。EDX と ECX を壊す。dhi は RAX/RDX/ESP 以外
	// （ECX が生きている時は呼ぶ側で push すること。MEG の呼び出し所では死んでいる）
	//   lo = a_lo*c の下、hi = (a_lo*c の上) + (a_hi*c の下) - (c<0 なら a_lo)（mod 2^32）
	//   ※ c<0 のとき b_hi=-1 の交差項 -a_lo が効く。これを落とすと a>0,c<0 で巨大な正になる
	void imul64i(u8 d, u8 dhi, u32 v)
	{
		if (d != RAX || dhi == RAX || dhi == RDX || dhi == RSP) x32_bad_reg();
		push(RAX);                    // a_lo
		mov32(RCX, dhi);              // a_hi
		imul32i(RCX, RCX, v);         // a_hi * c（下だけ）
		push(RCX);                    // [esp+0]=a_hi*c の下, [esp+4]=a_lo
		imm32(RCX, v);                // c
		load32(RAX, mem{ RSP, NOREG, 1, 4 });   // a_lo
		mul_full(RCX);                // a_lo * c（全幅・符号無し）edx:eax
		mov32(dhi, RDX);              // a_lo*c の上
		add32rm(dhi, mem{ RSP, NOREG, 1, 0 }); // + a_hi*c の下
		if (v & 0x80000000)
			load32(RCX, mem{ RSP, NOREG, 1, 4 }), sub32(dhi, RCX);   // - a_lo（b_hi=-1 の交差項）
		addrsp(8);
	}
};

#endif // SMU_X64ASM_MODE

} // namespace x64asm

#endif
