// license:BSD-3-Clause
//
// S-MU2000: SH-2 の命令をその場で x86-64 の機械語にする（JIT）。
//
// プログラム ROM（0x000000-0x3FFFFF）の上の命令を、分岐までのひと続き（ブロック）ごとに訳す。
// 訳した機械語は sh2_device::execute_run() の 1 周と**同じ順に同じこと**をする:
//
//   1. pc を進める（遅延スロットの命令なら m_delay を見て分岐先へ）
//   2. 命令を実行する（よく使う命令は機械語で直に、ほかは解釈実行の execute_one を呼ぶ）
//   3. m_test_irq が立っていて m_delay が 0 なら割り込みを確かめる
//   4. icount を 1 減らし、0 以下なら戻る
//
// メモリはプログラム ROM とワーク RAM だけを直に読み書きし（mem_bus の fast() と同じ範囲）、
// ほかは解釈実行と同じ関数を通す。周辺に触った命令のあとで、周辺が割り込みを起こして pc が
// 変わっていたり m_test_irq が立っていたりすれば、ブロックを抜けて execute_run に戻る。
// 訳すのは m_delay が 0 のときに入る所だけで、それ以外（RAM の上のコード、遅延スロットから
// 始まる所、PC の追跡中）は今までどおり解釈実行する。遅延スロットの命令は、pc が実行時に
// 決まるので、いつも解釈実行の関数を呼ぶ。
// ROM は書き換わらないので、訳した物は捨て直さない（リセットのときだけ捨てる）。
// 訳した物はどこにも保存しない（firmware 由来のものを配らない。実行時に作って捨てる）。
//
// x86-64 と x86-32 と arm64 で使う（Windows・macOS・Linux）。
// SMU2000_SH2_JIT=0 では使わない。
// SMU2000_SH2_JIT=1 では命令を機械語で書かず、全部 execute_one を呼ぶ（食い違いを探すとき用）。

#include "sh2.h"

// JIT を使うか:
//   x86-64 と arm64（MinGW の __x86_64__ と MSVC の _M_X64 の両方。これで MSVC x64 も JIT を使う）
//   x86-32 の移植（Phase 3 で完了）。32bit では SMU_JIT32_PORT_SH2 を自分で define して
//              JIT を自動で有効にする（SMU2000_SH2_JIT32 が建つ）。止めるには SMU_JIT32_NO_SH2 を定義。
//              実行時の SMU2000_SH2_JIT=0 は引き続く解釈実行専用の逃げ道（A/B 検証用）
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86)) && !defined(SMU_JIT32_NO_SH2)
	#define SMU_JIT32_PORT_SH2
#endif
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
#define SMU2000_SH2_JIT 1
#elif defined(_WIN32) && (defined(__i386__) || defined(_M_IX86)) && defined(SMU_JIT32_PORT_SH2)
#define SMU2000_SH2_JIT 1
#define SMU2000_SH2_JIT32 1
#else
#define SMU2000_SH2_JIT 0
#endif
#include "compat/exec_mem.h"
#if SMU2000_SH2_JIT
#ifdef __aarch64__
#include "a64asm.h"
#else
#include "x64asm.h"
#if SMU_X64ASM_MODE == 32
	// x86-32 の割当: ebx=cpu rsi=状態 ebp=ROM edi=RAM（全部 callee 保存＝ヘルパ呼び出しを跨ぐ）
	// 傷物（scratch）は eax/ecx/edx。ecx は mwrite の書き込み値を兼務（x64 の r8 の役）
	#define JIT_ROM RBP
	#define JIT_RAM RDI
	#define JIT_VAL RCX
#else
	#define JIT_ROM R12
	#define JIT_RAM R13
	#define JIT_VAL R8
#endif
#endif
#endif

#include <array>
#include <cstddef>
#include <cstdlib>
#include <memory>

struct sh2_device::jit {
	// 入口の関数（x64: rcx = cpu、rdx = 状態、r8 = ROM、r9 = RAM ／ x86-32: cdecl で [esp+4,8,12,16]）。
	// arm64: x0 = cpu、x1 = 状態、x2 = ROM、x3 = RAM）。
	// 状態はどちらの規約でも rbp に置く。rbp は両方の規約で保つべきレジスタなので、
	// ブロックの中で呼ぶ先（呼ぶ先が壊してよいのは rax・rcx・rdx・rsi・rdi・r8〜r11）と衝突しない。
	// arm64 版では x19-x26 に置く（全部呼ばれる側が保存するレジスタなので衝突しない）
	using enter_t = void (*)(sh2_device *, internal_sh2_state *, const u8 *rom, u8 *ram);
	using code_t = void *;

	static constexpr u32 ROM_END   = 0x400000;   // ここより下だけ訳す
	static constexpr int MAX_INSNS = 48;         // 1 ブロックの命令数の上限（遅延スロットは別）
	static constexpr size_t BUF_SIZE = 16 * 1024 * 1024;

	void *buf = nullptr;
	size_t used = 0;
	std::array<std::unique_ptr<code_t[]>, (ROM_END >> 12)> pages;
	enter_t enter = nullptr;      // 置き場の頭に作る
	void *next_block = nullptr;   // ブロックの終わりから飛ぶ先（置き場の頭に作る）
	code_t entry = nullptr;       // enter が最初に飛ぶブロック
	size_t base_used = 0;         // 入口と飛ぶ先の大きさ（捨てても残す）

	~jit()
	{
#if SMU2000_SH2_JIT
		if (buf)
			exec_mem::free_mem(buf, BUF_SIZE);
#endif
	}

	code_t *slot(u32 pc)
	{
		auto &p = pages[pc >> 12];
		if (!p) {
			p.reset(new code_t[0x800]);
			std::fill(p.get(), p.get() + 0x800, nullptr);
		}
		return &p[(pc & 0xfff) >> 1];
	}

	void flush()
	{
		for (auto &p : pages)
			p.reset();
		used = base_used;
	}

	bool init(sh2_device &cpu);
	code_t compile(sh2_device &cpu, u32 pc);
};

void sh2_device::jit_delete(jit *j)
{
	delete j;
}

bool sh2_device::jit_enabled()
{
#if SMU2000_SH2_JIT
	static const bool on = [] {
		const char *e = std::getenv("SMU2000_SH2_JIT");
		return !(e && e[0] == '0');
	}();
	return on;
#else
	return false;
#endif
}

// 命令を機械語で書くか。SMU2000_SH2_JIT=1 なら全部 execute_one を呼ぶ
[[maybe_unused]] static bool native_enabled()
{
	static const bool on = [] {
		const char *e = std::getenv("SMU2000_SH2_JIT");
		return !(e && e[0] == '1');
	}();
	return on;
}

void sh2_device::jit_flush()
{
	if (m_jit)
		m_jit->flush();
}

void sh2_device::jit_exec(sh2_device *c, u32 opcode)
{
	c->execute_one(u16(opcode));
}

// 食い違いを探す用。SH2_JIT_TRACE=サイクル,命令数,ファイル で、そのサイクルから 1 命令ごとに PC とレジスタを書く
// （解釈実行の道でも同じ形で書くので、SMU2000_SH2_JIT=0 の追跡と突き合わせられる）
static std::FILE *g_jt_file = nullptr;
static u64 g_jt_from = 0, g_jt_left = 0;
// SH2_JIT_HASH=step,file writes the same lines as the trace above through a
// rolling FNV-1a and emits one line every step, so a long boot can be bisected
// without materialising a multi-gigabyte trace. In both modes the traced
// instruction's own address is recorded after an '@' when the caller is
// compiled code (the interpreter, which has no compiled address, omits it).
static bool g_jt_hashmode = false;
static u64 g_jt_hash_step = 0, g_jt_hash_n = 0, g_jt_hash = 1469598103934665603ull;
bool sh2_device::jit_trace_on()
{
	static const bool on = [] {
		char path[512] = {};
		if (const char *e = std::getenv("SH2_JIT_TRACE")) {
			unsigned long long from = 0, count = 0;
			if (std::sscanf(e, "%llu,%llu,%511s", &from, &count, path) != 3) return false;
			g_jt_file = std::fopen(path, "w");
			g_jt_from = from; g_jt_left = count;
			return g_jt_file != nullptr;
		}
		if (const char *h = std::getenv("SH2_JIT_HASH")) {
			unsigned long long step = 100000;
			if (std::sscanf(h, "%llu,%511s", &step, path) != 2) return false;
			g_jt_file = std::fopen(path, "w");
			g_jt_hashmode = true; g_jt_hash_step = step ? step : 1; g_jt_left = ~u64(0);
			return g_jt_file != nullptr;
		}
		return false;
	}();
	return on;
}
void sh2_device::jit_trace(sh2_device *c, u32 at)
{
	const u64 cyc = c->total_cycles();
	if (cyc < g_jt_from || !g_jt_left) return;
	g_jt_left--;
	char buf[512];
	const int n = at == 0xffffffffu
		? std::snprintf(buf, sizeof buf, "%08X C=%llu%s ic=%d ti=%u dl=%08X pi=%u il=%d\n", c->m_sh2_state->pc, (unsigned long long)cyc, c->regs_text(),
		                c->m_sh2_state->icount, c->m_test_irq, c->m_sh2_state->m_delay, c->m_sh2_state->pending_irq, c->m_sh2_state->internal_irq_level)
		: std::snprintf(buf, sizeof buf, "%08X@%08X C=%llu%s ic=%d ti=%u dl=%08X pi=%u il=%d\n", c->m_sh2_state->pc, at, (unsigned long long)cyc, c->regs_text(),
		                c->m_sh2_state->icount, c->m_test_irq, c->m_sh2_state->m_delay, c->m_sh2_state->pending_irq, c->m_sh2_state->internal_irq_level);
	if (g_jt_hashmode) {
		for (int i = 0; i < n; i++) { g_jt_hash ^= u8(buf[i]); g_jt_hash *= 1099511628211ull; }
		if (++g_jt_hash_n >= g_jt_hash_step) {
			g_jt_hash_n = 0;
			std::fprintf(g_jt_file, "%llu %016llx\n", (unsigned long long)cyc, (unsigned long long)g_jt_hash);
			std::fflush(g_jt_file);
		}
		return;
	}
	std::fwrite(buf, 1, size_t(n), g_jt_file);
	if (!g_jt_left) std::fflush(g_jt_file);
}

void sh2_device::jit_irq(sh2_device *c)
{
	c->check_pending_irq("mame_sh2_execute");
	c->m_test_irq = 0;
}

u32 sh2_device::jit_rb(sh2_device *c, u32 a) { if (smu2000::g_pc_prof) smu2000::g_slow_mem[0]++; return c->read_byte(a); }
u32 sh2_device::jit_rw(sh2_device *c, u32 a) { if (smu2000::g_pc_prof) smu2000::g_slow_mem[0]++; return c->read_word(a); }
u32 sh2_device::jit_rl(sh2_device *c, u32 a) { if (smu2000::g_pc_prof) smu2000::g_slow_mem[0]++; return c->read_long(a); }
void sh2_device::jit_wb(sh2_device *c, u32 a, u32 v) { if (smu2000::g_pc_prof) smu2000::g_slow_mem[1]++; c->write_byte(a, u8(v)); }
void sh2_device::jit_ww(sh2_device *c, u32 a, u32 v) { if (smu2000::g_pc_prof) smu2000::g_slow_mem[1]++; c->write_word(a, u16(v)); }
void sh2_device::jit_wl(sh2_device *c, u32 a, u32 v) { if (smu2000::g_pc_prof) smu2000::g_slow_mem[1]++; c->write_long(a, v); }

bool sh2_device::jit_run()
{
	// 割り込みの印が外（周辺）で立っているときは、解釈実行で 1 命令進めて確かめさせる。
	// ブロックの中では、周辺に触らない命令のあとで印を見ないので
	if (m_sh2_state->m_delay || m_test_irq || smu2000::g_pc_trace || smu2000::g_pc_hash) {
		if (smu2000::g_pc_prof)
			smu2000::g_pc_prof_why[m_sh2_state->m_delay ? 0 : 1]++;
		return false;
	}
	const u32 pc = m_sh2_state->pc;
	if (pc >= jit::ROM_END - 0x100 || (pc & 1)) {
		if (smu2000::g_pc_prof)
			smu2000::g_pc_prof_why[2]++;
		return false;
	}
	if (!m_jit)
		m_jit.reset(new jit);
	jit::code_t code = *m_jit->slot(pc);
	if (!code) {
		// 訳している途中で置き場が一杯になると全部捨てるので、置き場は訳した後に引き直す
		code = m_jit->compile(*this, pc);
		if (!code)
			return false;
		*m_jit->slot(pc) = code;
	}
	if (smu2000::g_pc_prof) {
		smu2000::g_pc_prof[(pc & 0x3fffff) >> 6]++;
		smu2000::g_pc_prof_why[3]++;
	}
	m_jit->entry = code;
	const int before = m_sh2_state->icount;
	m_jit->enter(this, m_sh2_state, m_program->hot_rom(), m_program->hot_ram());
	if (smu2000::g_pc_prof)
		smu2000::g_pc_prof_why[4] += u64(before - m_sh2_state->icount);
	return true;
}

namespace {

// 命令の種類。execute_one の振り分けと同じ表から引く
enum class kind { normal, delayed, ends };

[[maybe_unused]] kind classify(u16 op)
{
	switch (op >> 12) {
	case 0x0:
		switch (op & 0x3f) {
		case 0x03: case 0x0b: case 0x23: case 0x2b:          // BSRF, RTS, BRAF, RTE
			return kind::delayed;
		case 0x1b:                                            // SLEEP
		case 0x00: case 0x01: case 0x10: case 0x11: case 0x13: case 0x20: case 0x21:
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x38: case 0x39: case 0x3a: case 0x3b:
			return kind::ends;                                // ILLEGAL
		}
		return kind::normal;
	case 0x2:
		return (op & 15) == 3 ? kind::ends : kind::normal;
	case 0x3:
		return ((op & 15) == 1 || (op & 15) == 9) ? kind::ends : kind::normal;
	case 0x4:
		switch (op & 0x3f) {
		case 0x0b: case 0x2b:                                 // JSR, JMP
			return kind::delayed;
		case 0x0c: case 0x0d: case 0x14: case 0x1c: case 0x1d: case 0x2c: case 0x2d:
			return kind::ends;
		}
		return (op & 0x3f) >= 0x30 && (op & 0x3f) != 0x3f ? kind::ends : kind::normal;
	case 0x8:
		switch ((op >> 8) & 15) {
		case 0xd: case 0xf:                                   // BTS, BFS
			return kind::delayed;
		case 0x9: case 0xb:                                   // BT, BF
		case 0x2: case 0x3: case 0x6: case 0x7: case 0xa: case 0xc: case 0xe:
			return kind::ends;
		}
		return kind::normal;
	case 0xa: case 0xb:                                       // BRA, BSR
		return kind::delayed;
	case 0xc:
		return ((op >> 8) & 15) == 3 ? kind::ends : kind::normal;   // TRAPA
	case 0xf:
		return kind::ends;
	}
	return kind::normal;
}

} // namespace

// JIT を切った所（SMU2000_SH2_JIT=0 の環境）は解釈実行専用。
// ここから下が命令生成（x86-64 と x86-32 は SMU_X64ASM_MODE で #if で分ける。出す機械語だけ違い、
// ブロックを組み立てる流れは共通）
#if !SMU2000_SH2_JIT

bool sh2_device::jit::init(sh2_device &)
{
	return false;
}

sh2_device::jit::code_t sh2_device::jit::compile(sh2_device &, u32)
{
	return nullptr;
}

#elif !defined(__aarch64__)	// x86（x64 と SMU2000_SH2_JIT32 の両モード。SMU_X64ASM_MODE で切り替える）

// 置き場の頭に、入口（enter）とブロックの終わりから飛ぶ先（next_block）を作る
bool sh2_device::jit::init(sh2_device &cpu)
{
	using namespace x64asm;		// RWX buffer: code runs in place as soon as it is written (rebuilds overwrite it, so no protect swapping)
	buf = exec_mem::alloc_rwx(BUF_SIZE);
	if (!buf)
		return false;
#if SMU_X64ASM_MODE == 32
	if (sizeof(pages[0]) != 4)
#else
	if (sizeof(pages[0]) != 8)
#endif
		return false;
	const internal_sh2_state *st = cpu.m_sh2_state;
#if SMU_X64ASM_MODE == 32
	const auto S = [st](const void *f) { return mem{ RSI, NOREG, 1, s32(intptr_t(f) - intptr_t(st)) }; };   // 32bit は状態=rsi（rbp=ROM 基）
#else
	const auto S = [st](const void *f) { return mem{ RBP, NOREG, 1, s32(intptr_t(f) - intptr_t(st)) }; };
#endif
	const mem C_test { RBX, NOREG, 1, s32(intptr_t(&cpu.m_test_irq) - intptr_t(&cpu)) };

	assembler a;
	// enter: ebx rsi + ROM/RAM 基を保存して entry へ飛ぶ
#if SMU_X64ASM_MODE == 32
	// x86-32 cdecl: 呼び出し側が積んだ引数 cpu/ROM... は [esp+4,8,12,16]。4 積んだ後は +16 ずれる
	a.push(RBX); a.push(RSI); a.push(RBP); a.push(RDI);
	a.load32(RBX, mem{ RSP, NOREG, 1, 20 });   // cpu
	a.load32(RSI, mem{ RSP, NOREG, 1, 24 });   // 状態
	a.load32(RBP, mem{ RSP, NOREG, 1, 28 });   // ROM
	a.load32(RDI, mem{ RSP, NOREG, 1, 32 });   // RAM
	a.imm32(RAX, u32(uintptr_t(&entry)));
	a.load32(RAX, mem{ RAX, NOREG, 1, 0 });
	a.rr(0, {0xff}, 4, RAX);             // jmp eax
#else
	// enter: rbx rbp r12 r13 を保って、entry へ飛ぶ
	a.push(RBX); a.push(RBP); a.push(R12); a.push(R13);
	a.subrsp(40);                        // 影 32 + 詰め物 8。rsp は 16 の倍数になる
	a.mov64(RBX, ARG0);
	a.mov64(RBP, ARG1);                  // 状態は rbp（SysV のヘルパ呼び出しが rdi・rsi・rdx・rcx を壊す）
	a.mov64(R12, ARG2);
	a.mov64(R13, ARG3);
	a.imm64(RAX, u64(uintptr_t(&entry)));
	a.load64(RAX, mem{ RAX, NOREG, 1, 0 });
	a.rr(0, false, {0xff}, 4, RAX);      // jmp rax
#endif

	// next_block: jit_run が見ることを見て、次のブロックが訳してあればそこへ飛ぶ。無ければ戻る
	const size_t next = a.code.size();
	std::vector<size_t> to_exit;
	a.cmp32i_mem(S(&st->icount), 0);
	to_exit.push_back(a.jcc_fwd(0x8e));                     // jle
	a.load32(RAX, S(&st->m_delay));
	a.test32(RAX, RAX);
	to_exit.push_back(a.jcc_fwd(0x85));
	a.load32(RAX, C_test);
	a.test32(RAX, RAX);
	to_exit.push_back(a.jcc_fwd(0x85));
	a.load32(RAX, S(&st->pc));
	a.cmp32ri(RAX, ROM_END - 0x100);
	to_exit.push_back(a.jcc_fwd(0x83));                     // jae
	a.test32ri(RAX, 1);
	to_exit.push_back(a.jcc_fwd(0x85));
	a.mov32(RCX, RAX);
	a.shr32(RCX, 12);
#if SMU_X64ASM_MODE == 32
	a.imm32(RDX, u32(uintptr_t(pages.data())));
	a.load32(RDX, mem{ RDX, RCX, 4, 0 });
	a.test32(RDX, RDX);
	to_exit.push_back(a.jcc_fwd(0x84));
	a.and32i(RAX, 0xfff);
	a.shr32(RAX, 1);
	a.load32(RAX, mem{ RDX, RAX, 4, 0 });
	a.test32(RAX, RAX);
	to_exit.push_back(a.jcc_fwd(0x84));
	a.rr(0, {0xff}, 4, RAX);             // jmp eax
#else
	a.imm64(RDX, u64(uintptr_t(pages.data())));
	a.load64(RDX, mem{ RDX, RCX, 8, 0 });
	a.test64(RDX, RDX);
	to_exit.push_back(a.jcc_fwd(0x84));
	a.and32i(RAX, 0xfff);
	a.shr32(RAX, 1);
	a.load64(RAX, mem{ RDX, RAX, 8, 0 });
	a.test64(RAX, RAX);
	to_exit.push_back(a.jcc_fwd(0x84));
	a.rr(0, false, {0xff}, 4, RAX);      // jmp rax
#endif
	for (size_t p : to_exit)
		a.patch(p);
#if SMU_X64ASM_MODE == 32
	a.pop(RDI); a.pop(RBP); a.pop(RSI); a.pop(RBX);   // 引き数は呼び出し側が取る（cdecl）
#else
	a.addrsp(40);
	a.pop(R13); a.pop(R12); a.pop(RBP); a.pop(RBX);
#endif
	a.ret();

#if SMU_X64ASM_MODE == 32
	// x86-32 ポートが生きている証明用: SMU2000_SH2_JIT32_LOG=1 で 1 回だけ印を書く
	if (std::getenv("SMU2000_SH2_JIT32_LOG"))
		std::fprintf(stderr, "sh2-jit32: x86-32 prologue emitted (%zu bytes, enter=%p)\n", a.code.size(), (void *)buf);
#endif
	std::memcpy(buf, a.code.data(), a.code.size());
	enter = reinterpret_cast<enter_t>(buf);
	next_block = static_cast<u8 *>(buf) + next;
	base_used = used = a.code.size();
	return true;
}

sh2_device::jit::code_t sh2_device::jit::compile(sh2_device &cpu, u32 pc)
{
	using namespace x64asm;
	if (!buf && !init(cpu))
		return nullptr;

	// 直に読み書きする領域（mem_bus の fast() と同じ）。番地を折り返さない設定のときだけ訳す
	const mem_bus &bus = *cpu.m_program;
	if (cpu.m_am != 0xffffffff || cpu.m_decrypted_program != cpu.m_program || !bus.hot_rom() || !bus.hot_ram())
		return nullptr;
	const u32 rom_end   = bus.hot_rom_end();      // 含む
	const u32 ram_start = bus.hot_ram_start();
	const u32 ram_len   = bus.hot_ram_len();      // 端 - 始め
	if (rom_end < 0xffff || rom_end >= 0x40000000 || ram_len < 0xffff || ram_start <= rom_end)
		return nullptr;

	const internal_sh2_state *st = cpu.m_sh2_state;
#if SMU_X64ASM_MODE == 32
	const auto S = [st](const void *f) { return mem{ RSI, NOREG, 1, s32(intptr_t(f) - intptr_t(st)) }; };   // 32bit は状態=rsi（rbp=ROM 基）
#else
	const auto S = [st](const void *f) { return mem{ RBP, NOREG, 1, s32(intptr_t(f) - intptr_t(st)) }; };
#endif
	const mem S_pc = S(&st->pc), S_delay = S(&st->m_delay), S_icount = S(&st->icount), S_ea = S(&st->ea);
	const mem S_sr = S(&st->sr), S_pr = S(&st->pr), S_gbr = S(&st->gbr), S_vbr = S(&st->vbr);
	const mem S_mach = S(&st->mach), S_macl = S(&st->macl);
	const auto R = [&](int n) { return S(&st->r[n]); };
	const mem C_test { RBX, NOREG, 1, s32(intptr_t(&cpu.m_test_irq) - intptr_t(&cpu)) };
	if (sizeof(st->pc) != 4 || sizeof(st->icount) != 4 || sizeof(cpu.m_test_irq) != 4 || sizeof(st->r[0]) != 4)
		return nullptr;

	assembler a;
	// ブロックの中では rbx = cpu、rsi = 状態、ROM/RAM 基 = JIT_ROM/JIT_RAM（enter が入れる）

	std::vector<size_t> to_finish, to_ret;

	const auto call = [&](void *fn) { a.call_abs(fn); };
	// Helper-call arguments. The address sits in edx and the value in r8d by internal
	// convention. Under Windows x64 that is already the argument order (only rcx is
	// missing); under SysV they have to move to rsi and rdx
#if SMU_X64ASM_MODE == 32
	constexpr bool sysv = false;   // x86-32 は cdecl（引数は-stack）。引数レジスタの出し分けは不要
#else
	constexpr bool sysv = sysv_abi;
#endif
	const auto setT = [&]() {           // al の 0/1 を T へ
		a.movzx8(RAX, RAX);
		a.and32i_mem(S_sr, ~u32(SH_T));
		a.or32mr(S_sr, RAX);
	};
	// 読む。番地は edx、値は eax（sz バイトを並べた値。符号は広げない）
	const auto mread = [&](int sz) {
		std::vector<size_t> to_slow, to_done;
		if (sz > 1) {
			a.test32ri(RDX, 1);
			to_slow.push_back(a.jcc_fwd(0x85));
		}
		a.cmp32ri(RDX, rom_end + 1 - u32(sz));
		const size_t not_rom = a.jcc_fwd(0x87);                // ja
		const mem mr{ JIT_ROM, RDX, 1, 0 };
		if (sz == 1) a.loadu8(RAX, mr);
		else if (sz == 2) { a.loadu16(RAX, mr); a.bswap32(RAX); a.shr32(RAX, 16); }
		else { a.load32(RAX, mr); a.bswap32(RAX); }
		to_done.push_back(a.jmp_fwd());
		a.patch(not_rom);
		a.lea32(RAX, mem{ RDX, NOREG, 1, -s32(ram_start) });
		a.cmp32ri(RAX, ram_len + 1 - u32(sz));
		to_slow.push_back(a.jcc_fwd(0x87));
		const mem mw{ JIT_RAM, RAX, 1, 0 };
		if (sz == 1) a.loadu8(RAX, mw);
		else if (sz == 2) { a.loadu16(RAX, mw); a.bswap32(RAX); a.shr32(RAX, 16); }
		else { a.load32(RAX, mw); a.bswap32(RAX); }
		to_done.push_back(a.jmp_fwd());
		for (size_t p : to_slow) a.patch(p);
#if SMU_X64ASM_MODE == 32
		a.push(RDX); a.push(RBX);        // cdecl: jit_r?(c, a)
		call(sz == 1 ? reinterpret_cast<void *>(&sh2_device::jit_rb) :
		     sz == 2 ? reinterpret_cast<void *>(&sh2_device::jit_rw) : reinterpret_cast<void *>(&sh2_device::jit_rl));
		a.addrsp(8);
#else
		if constexpr (sysv)
			a.mov64(ARG1, RDX);        // address
		a.mov64(ARG0, RBX);
		call(sz == 1 ? reinterpret_cast<void *>(&sh2_device::jit_rb) :
		     sz == 2 ? reinterpret_cast<void *>(&sh2_device::jit_rw) : reinterpret_cast<void *>(&sh2_device::jit_rl));
#endif
		for (size_t p : to_done) a.patch(p);
	};
	// 書く。番地は edx、値は r8d
	// 書く。番地は edx、値は r8d（x86-32 では ecx。速い道で edx を潰したので遅い道は先に積む）
	const auto mwrite = [&](int sz) {
		std::vector<size_t> to_slow;
#if SMU_X64ASM_MODE == 32
		if (sz > 1) {
			a.test32ri(RDX, 1);
			to_slow.push_back(a.jcc_fwd(0x85));
		}
		a.lea32(RAX, mem{ RDX, NOREG, 1, -s32(ram_start) });
		a.cmp32ri(RAX, ram_len + 1 - u32(sz));
		to_slow.push_back(a.jcc_fwd(0x87));
		const mem mw{ JIT_RAM, RAX, 1, 0 };
		if (sz == 1) a.store8(mw, JIT_VAL);
		else if (sz == 2) { a.mov32(RDX, JIT_VAL); a.bswap32(RDX); a.shr32(RDX, 16); a.store16(mw, RDX); }
		else { a.mov32(RDX, JIT_VAL); a.bswap32(RDX); a.store32(mw, RDX); }
		const size_t done = a.jmp_fwd();
		for (size_t p : to_slow) a.patch(p);
		a.push(JIT_VAL); a.push(RDX); a.push(RBX);   // cdecl: jit_w?(c, a, v)
		call(sz == 1 ? reinterpret_cast<void *>(&sh2_device::jit_wb) :
		     sz == 2 ? reinterpret_cast<void *>(&sh2_device::jit_ww) : reinterpret_cast<void *>(&sh2_device::jit_wl));
		a.addrsp(12);
		a.patch(done);
#else
		if (sz > 1) {
			a.test32ri(RDX, 1);
			to_slow.push_back(a.jcc_fwd(0x85));
		}
		a.lea32(RAX, mem{ RDX, NOREG, 1, -s32(ram_start) });
		a.cmp32ri(RAX, ram_len + 1 - u32(sz));
		to_slow.push_back(a.jcc_fwd(0x87));
		const mem mw{ R13, RAX, 1, 0 };
		if (sz == 1) a.store8(mw, R8);
		else if (sz == 2) { a.mov32(RCX, R8); a.bswap32(RCX); a.shr32(RCX, 16); a.store16(mw, RCX); }
		else { a.mov32(RCX, R8); a.bswap32(RCX); a.store32(mw, RCX); }
		const size_t done = a.jmp_fwd();
		for (size_t p : to_slow) a.patch(p);
		if constexpr (sysv) {
			a.mov64(ARG1, RDX);        // address
			a.mov64(ARG2, R8);         // value
		}
		a.mov64(ARG0, RBX);
		call(sz == 1 ? reinterpret_cast<void *>(&sh2_device::jit_wb) :
		     sz == 2 ? reinterpret_cast<void *>(&sh2_device::jit_ww) : reinterpret_cast<void *>(&sh2_device::jit_wl));
		a.patch(done);
#endif
	};
	const auto sext8 = [](u32 v) { return u32(s32(s8(v))); };

	// 解釈実行と同じことを機械語で書く。書けない命令は none を返す
	enum res { none, pure, memop, delayed, ends };
	const auto native = [&](u16 op, u32 at) -> res {
		const int n = (op >> 8) & 15, m = (op >> 4) & 15;
		const u32 pcv = at + 2;                                  // 実行中の pc
		const auto rd_ea = [&](int base, u32 disp, int sz) {    // ea = r[base] + disp で読む
			a.load32(RDX, R(base));
			if (disp) a.add32ri(RDX, disp);
			a.store32(S_ea, RDX);
			mread(sz);
		};
		const auto wr_ea = [&](int base, u32 disp, int src, int sz) {   // ea = r[base] + disp へ r[src] を書く
			a.load32(RDX, R(base));
			if (disp) a.add32ri(RDX, disp);
			a.store32(S_ea, RDX);
			a.load32(JIT_VAL, R(src));
			mwrite(sz);
		};
		const auto to_r = [&](int d, int sz) {                  // eax を符号拡張して r[d] へ
			if (sz == 1) a.movsx8(RAX, RAX);
			else if (sz == 2) a.movsx16(RAX, RAX);
			a.store32(R(d), RAX);
		};
		const auto cmp_t = [&](u8 cc) { a.load32(RAX, R(n)); a.cmp32rm(RAX, R(m)); a.setcc(cc, RAX); setT(); return pure; };
		const auto copy = [&](const mem &from, const mem &to) { a.load32(RAX, from); a.store32(to, RAX); return pure; };
		const auto branch_to = [&](u32 target, bool delay) {
			if (delay) { a.store32i(S_delay, target); a.store32i(S_ea, target); a.sub32i_mem(S_icount, 1); }
			else { a.store32i(S_pc, target); a.store32i(S_ea, target); a.sub32i_mem(S_icount, 2); }
		};

		switch (op >> 12) {
		case 0x0:
			switch (op & 0x3f) {
			case 0x04: case 0x14: case 0x24: case 0x34:
			case 0x05: case 0x15: case 0x25: case 0x35:
			case 0x06: case 0x16: case 0x26: case 0x36: {       // MOV.x Rm,@(R0,Rn)
				const int sz = 1 << ((op & 15) - 4);
				a.load32(RDX, R(n)); a.add32rm(RDX, R(0)); a.store32(S_ea, RDX);
				a.load32(JIT_VAL, R(m));
				mwrite(sz);
				return memop;
			}
			case 0x0c: case 0x1c: case 0x2c: case 0x3c:
			case 0x0d: case 0x1d: case 0x2d: case 0x3d:
			case 0x0e: case 0x1e: case 0x2e: case 0x3e: {       // MOV.x @(R0,Rm),Rn
				const int sz = 1 << ((op & 15) - 12);
				a.load32(RDX, R(m)); a.add32rm(RDX, R(0)); a.store32(S_ea, RDX);
				mread(sz);
				to_r(n, sz);
				return memop;
			}
			case 0x07: case 0x17: case 0x27: case 0x37:          // MUL.L
				a.load32(RAX, R(n)); a.load32(RCX, R(m)); a.imul32(RAX, RCX); a.store32(S_macl, RAX);
				a.sub32i_mem(S_icount, 1);
				return pure;
			case 0x08: a.and32i_mem(S_sr, ~u32(SH_T)); return pure;                 // CLRT
			case 0x18: a.or32i_mem(S_sr, SH_T); return pure;                        // SETT
			case 0x19: a.and32i_mem(S_sr, ~u32(SH_M | SH_Q | SH_T)); return pure;   // DIV0U
			case 0x09: return pure;                                                  // NOP
			case 0x28: a.store32i(S_mach, 0); a.store32i(S_macl, 0); return pure;   // CLRMAC
			case 0x02: return copy(S_sr, R(n));                                      // STC SR,Rn
			case 0x12: return copy(S_gbr, R(n));
			case 0x22: return copy(S_vbr, R(n));
			case 0x0a: return copy(S_mach, R(n));                                    // STS x,Rn
			case 0x1a: return copy(S_macl, R(n));
			case 0x2a: return copy(S_pr, R(n));
			case 0x29:                                                               // MOVT
				a.load32(RAX, S_sr); a.and32i(RAX, SH_T); a.store32(R(n), RAX);
				return pure;
			case 0x0b:                                                               // RTS
				a.load32(RAX, S_pr); a.store32(S_delay, RAX); a.store32(S_ea, RAX); a.sub32i_mem(S_icount, 1);
				return delayed;
			}
			return none;
		case 0x1:                                                                    // MOV.L Rm,@(disp,Rn)
			wr_ea(n, (op & 15) * 4, m, 4);
			return memop;
		case 0x2:
			switch (op & 15) {
			case 0: case 1: case 2: {                                                // MOV.x Rm,@Rn
				const int sz = 1 << (op & 15);
				wr_ea(n, 0, m, sz);
				return memop;
			}
			case 4: case 5: case 6: {                                                // MOV.x Rm,@-Rn
				const int sz = 1 << ((op & 15) - 4);
				a.load32(JIT_VAL, R(m));
				a.sub32i_mem(R(n), sz);
				a.load32(RDX, R(n));
				mwrite(sz);
				return memop;
			}
			case 8: a.load32(RAX, R(n)); a.test32rm(RAX, R(m)); a.setcc(0x94, RAX); setT(); return pure;   // TST
			case 9: a.load32(RAX, R(m)); a.and32mr(R(n), RAX); return pure;
			case 10: a.load32(RAX, R(m)); a.xor32mr(R(n), RAX); return pure;
			case 11: a.load32(RAX, R(m)); a.or32mr(R(n), RAX); return pure;
			case 14: a.loadu16(RAX, R(n)); a.loadu16(RCX, R(m)); a.imul32(RAX, RCX); a.store32(S_macl, RAX); return pure;       // MULU
			case 15: a.loads16_32(RAX, R(n)); a.loads16_32(RCX, R(m)); a.imul32(RAX, RCX); a.store32(S_macl, RAX); return pure; // MULS
			}
			return none;
		case 0x3:
			switch (op & 15) {
			case 0: return cmp_t(0x94);          // CMP/EQ
			case 2: return cmp_t(0x93);          // CMP/HS
			case 3: return cmp_t(0x9d);          // CMP/GE
			case 6: return cmp_t(0x97);          // CMP/HI
			case 7: return cmp_t(0x9f);          // CMP/GT
			case 8: a.load32(RAX, R(m)); a.sub32mr(R(n), RAX); return pure;
			case 12: a.load32(RAX, R(m)); a.add32mr(R(n), RAX); return pure;
			}
			return none;
		case 0x4:
			switch (op & 0x3f) {
			case 0x00: case 0x20:                                                    // SHLL / SHAL
				a.load32(RAX, R(n)); a.mov32(RCX, RAX); a.shr32(RCX, 31); a.shl32(RAX, 1); a.store32(R(n), RAX);
				a.and32i_mem(S_sr, ~u32(SH_T)); a.or32mr(S_sr, RCX);
				return pure;
			case 0x01: case 0x21:                                                    // SHLR / SHAR
				a.load32(RAX, R(n)); a.mov32(RCX, RAX); a.and32i(RCX, 1);
				if ((op & 0x3f) == 0x01) a.shr32(RAX, 1); else a.sar32(RAX, 1);
				a.store32(R(n), RAX);
				a.and32i_mem(S_sr, ~u32(SH_T)); a.or32mr(S_sr, RCX);
				return pure;
			case 0x08: a.shl32i_mem(R(n), 2); return pure;
			case 0x18: a.shl32i_mem(R(n), 8); return pure;
			case 0x28: a.shl32i_mem(R(n), 16); return pure;
			case 0x09: a.shr32i_mem(R(n), 2); return pure;
			case 0x19: a.shr32i_mem(R(n), 8); return pure;
			case 0x29: a.shr32i_mem(R(n), 16); return pure;
			case 0x10: a.sub32i_mem(R(n), 1); a.setcc(0x94, RAX); setT(); return pure;   // DT
			case 0x11: a.cmp32i_mem(R(n), 0); a.setcc(0x9d, RAX); setT(); return pure;   // CMP/PZ
			case 0x15: a.cmp32i_mem(R(n), 0); a.setcc(0x9f, RAX); setT(); return pure;   // CMP/PL
			case 0x0a: return copy(R(n), S_mach);                                    // LDS Rn,x
			case 0x1a: return copy(R(n), S_macl);
			case 0x2a: return copy(R(n), S_pr);
			case 0x1e: return copy(R(n), S_gbr);                                     // LDC Rn,GBR / VBR
			case 0x2e: return copy(R(n), S_vbr);
			case 0x02: case 0x12: case 0x22: {                                       // STS.L x,@-Rn
				const mem src = (op & 0x3f) == 0x02 ? S_mach : (op & 0x3f) == 0x12 ? S_macl : S_pr;
				a.sub32i_mem(R(n), 4); a.load32(RDX, R(n)); a.store32(S_ea, RDX);
				a.load32(JIT_VAL, src);
				mwrite(4);
				return memop;
			}
			case 0x06: case 0x16: case 0x26: {                                       // LDS.L @Rn+,x
				const mem dst = (op & 0x3f) == 0x06 ? S_mach : (op & 0x3f) == 0x16 ? S_macl : S_pr;
				a.load32(RDX, R(n)); a.store32(S_ea, RDX);
				mread(4);
				a.store32(dst, RAX);
				a.add32i_mem(R(n), 4);
				return memop;
			}
			case 0x0b:                                                               // JSR
				a.store32i(S_pr, pcv + 2);
				[[fallthrough]];
			case 0x2b:                                                               // JMP
				a.load32(RAX, R(n)); a.store32(S_delay, RAX); a.store32(S_ea, RAX); a.sub32i_mem(S_icount, 1);
				return delayed;
			}
			return none;
		case 0x5:                                                                    // MOV.L @(disp,Rm),Rn
			rd_ea(m, (op & 15) * 4, 4);
			to_r(n, 4);
			return memop;
		case 0x6:
			switch (op & 15) {
			case 0: case 1: case 2: {                                                // MOV.x @Rm,Rn
				const int sz = 1 << (op & 15);
				rd_ea(m, 0, sz);
				to_r(n, sz);
				return memop;
			}
			case 3: return copy(R(m), R(n));
			case 4: case 5: case 6: {                                                // MOV.x @Rm+,Rn
				const int sz = 1 << ((op & 15) - 4);
				a.load32(RDX, R(m));
				mread(sz);
				to_r(n, sz);
				if (n != m) a.add32i_mem(R(m), sz);
				return memop;
			}
			case 7: a.load32(RAX, R(m)); a.not32(RAX); a.store32(R(n), RAX); return pure;
			case 11: a.load32(RAX, R(m)); a.neg32(RAX); a.store32(R(n), RAX); return pure;
			case 12: a.loadu8(RAX, R(m)); a.store32(R(n), RAX); return pure;
			case 13: a.loadu16(RAX, R(m)); a.store32(R(n), RAX); return pure;
			case 14: a.loads8_32(RAX, R(m)); a.store32(R(n), RAX); return pure;
			case 15: a.loads16_32(RAX, R(m)); a.store32(R(n), RAX); return pure;
			}
			return none;
		case 0x7:                                                                    // ADD #imm,Rn
			a.add32i_mem(R(n), sext8(op & 0xff));
			return pure;
		case 0x8: {
			const u32 target = pcv + u32(s32(s8(op & 0xff)) * 2) + 2;
			switch (n) {
			case 0: wr_ea(m, op & 15, 0, 1); return memop;                          // MOV.B R0,@(disp,Rm)
			case 1: wr_ea(m, (op & 15) * 2, 0, 2); return memop;
			case 4: rd_ea(m, op & 15, 1); to_r(0, 1); return memop;                 // MOV.B @(disp,Rm),R0
			case 5: rd_ea(m, (op & 15) * 2, 2); to_r(0, 2); return memop;
			case 8: a.cmp32i_mem(R(0), sext8(op & 0xff)); a.setcc(0x94, RAX); setT(); return pure;   // CMP/EQ #imm
			case 9: case 11: case 13: case 15: {                                    // BT / BF / BT/S / BF/S
				a.test32i_mem(S_sr, SH_T);
				const size_t skip = a.jcc_fwd((n == 9 || n == 13) ? 0x84 : 0x85);
				branch_to(target, n >= 13);
				a.patch(skip);
				return n >= 13 ? delayed : ends;
			}
			}
			return none;
		}
		case 0x9: {                                                                  // MOV.W @(disp,PC),Rn
			const u32 ea = pcv + (op & 0xff) * 2 + 2;
			if (ea + 1 > rom_end)
				return none;
			a.store32i(S_ea, ea);
			a.store32i(R(n), u32(s32(s16(cpu.m_program->read_word(ea)))));
			return pure;
		}
		case 0xa:                                                                    // BRA
			branch_to(pcv + u32(util::sext(op & 0xfff, 12) * 2) + 2, true);
			return delayed;
		case 0xb:                                                                    // BSR
			a.store32i(S_pr, pcv + 2);
			branch_to(pcv + u32(util::sext(op & 0xfff, 12) * 2) + 2, true);
			return delayed;
		case 0xc: {
			const u32 d = op & 0xff;
			switch (n) {
			case 0: case 1: case 2: {                                                // MOV.x R0,@(disp,GBR)
				const int sz = 1 << n;
				a.load32(RDX, S_gbr); a.add32ri(RDX, d * sz); a.store32(S_ea, RDX);
				a.load32(JIT_VAL, R(0));
				mwrite(sz);
				return memop;
			}
			case 4: case 5: case 6: {                                                // MOV.x @(disp,GBR),R0
				const int sz = 1 << (n - 4);
				a.load32(RDX, S_gbr); a.add32ri(RDX, d * sz); a.store32(S_ea, RDX);
				mread(sz);
				to_r(0, sz);
				return memop;
			}
			case 7: {                                                                // MOVA
				const u32 ea = ((pcv + 2) & ~3u) + d * 4;
				a.store32i(S_ea, ea); a.store32i(R(0), ea);
				return pure;
			}
			case 8: a.test32i_mem(R(0), d); a.setcc(0x94, RAX); setT(); return pure;   // TST #imm,R0
			case 9: a.and32i_mem(R(0), d); return pure;
			case 10: a.xor32i_mem(R(0), d); return pure;
			case 11: a.or32i_mem(R(0), d); return pure;
			}
			return none;
		}
		case 0xd: {                                                                  // MOV.L @(disp,PC),Rn
			const u32 ea = ((pcv + 2) & ~3u) + (op & 0xff) * 4;
			if (ea + 3 > rom_end)
				return none;
			a.store32i(S_ea, ea);
			a.store32i(R(n), cpu.m_program->read_dword(ea));
			return pure;
		}
		case 0xe:                                                                    // MOV #imm,Rn
			a.store32i(R(n), sext8(op & 0xff));
			return pure;
		}
		return none;
	};

	// pc を書くのを遅らせる。レジスタだけの命令（pure）は pc を見ないので、続いているあいだは書かず、
	// 周辺に触る命令・解釈実行・ブロックを抜ける所の手前でだけ、そのとき正しい pc を書く
	static const bool lazy_pc = [] {
		const char *e = std::getenv("SMU2000_SH2_LAZYPC");
		return !(e && e[0] == '0');
	}();
	static const bool slot_native = [] {
		const char *e = std::getenv("SMU2000_SH2_SLOTNATIVE");
		return !(e && e[0] == '0');
	}();
	// Same as the arm64 side: these two are functions with a function-local
	// static, so they are read once per compile rather than inside the loop,
	// which would ask for them two or three times per instruction compiled
	const bool trace       = jit_trace_on();
	const bool emit_native = native_enabled();
	bool pc_stale = false;              // メモリの pc が古い
	u32 stale_pc = 0;                   // そのとき正しい pc
	std::vector<std::pair<size_t, u32>> stale_rets;   // pc を書いてから ret へ行く出口
	const auto store_pc_at = [&](size_t pos, u32 v) {
		assembler t;
		t.store32i(S_pc, v);
		a.code.insert(a.code.begin() + std::ptrdiff_t(pos), t.code.begin(), t.code.end());
	};

	bool slot = false;
	for (int i = 0; ; i++) {
		const u32 at = pc + 2 * u32(i);
		const u16 op = cpu.m_decrypted_program->read_word(at);
		const kind k = classify(op);

		if (trace) {
			if (pc_stale) { a.store32i(S_pc, stale_pc); pc_stale = false; }
#if SMU_X64ASM_MODE == 32
			a.push(RBX);                 // cdecl: jit_trace(c)
			call(reinterpret_cast<void *>(&sh2_device::jit_trace));
			a.addrsp(4);
#else
			a.mov64(ARG0, RBX);
			a.imm32(ARG1, at);
			call(reinterpret_cast<void *>(&sh2_device::jit_trace));
#endif
		}

		// 1. pc を進める
		if (slot) {
			a.load32(RAX, S_delay);
			a.test32(RAX, RAX);
			const size_t no_delay = a.jcc_fwd(0x84);          // je
			a.store32(S_pc, RAX);
			a.store32i(S_delay, 0);
			const size_t done = a.jmp_fwd();
			a.patch(no_delay);
			a.store32i(S_pc, at + 2);
			a.patch(done);
		} else if (!lazy_pc || trace)
			a.store32i(S_pc, at + 2);

		// 2. 実行する
		res r = none;
		const size_t op_begin = a.code.size();
		// 遅延スロットの命令も、pc を使わない普通の命令なら機械語で書く（解釈実行でも pc は見ない）。
		// pc を使うのは MOV.W/MOV.L @(disp,PC) と MOVA だけ（分岐はスロットに来ない）
		const bool pc_rel = (op >> 12) == 0x9 || (op >> 12) == 0xd || (op >> 8) == 0xc7;
		if (emit_native && (!slot || (slot_native && k == kind::normal && !pc_rel)))
			r = native(op, at);
		if (r == none) {
			if (!slot && lazy_pc && !trace)
				a.store32i(S_pc, at + 2);
#if SMU_X64ASM_MODE == 32
			a.imm32(RDX, op);
			a.push(RDX); a.push(RBX);    // cdecl: jit_exec(c, op)
			call(reinterpret_cast<void *>(&sh2_device::jit_exec));
			a.addrsp(8);
#else
			a.mov64(ARG0, RBX);
			a.imm32(ARG1, op);
			call(reinterpret_cast<void *>(&sh2_device::jit_exec));
#endif
			r = k == kind::delayed ? delayed : k == kind::ends ? ends : memop;
			pc_stale = false;
		} else if (!slot && lazy_pc && !trace) {
			if (r == pure) {
				pc_stale = true;
				stale_pc = at + 2;
			} else {
				store_pc_at(op_begin, at + 2);                // 命令の頭に差し込む（中の飛び先は相対なのでずれない）
				pc_stale = false;
			}
		}
		if (slot)
			pc_stale = false;

		// 分岐した・止まる命令・遅延スロットの後はブロックを終える
		if (slot || r == ends || (r != delayed && (i + 1 >= MAX_INSNS || at + 4 >= ROM_END)))
			break;

		// 3. 4. 途中の確かめ。周辺に触りうる命令は、割り込みの印か思わぬ pc の変化があれば終わりの処理へ
		if (r == memop) {
			a.load32(RAX, C_test);
			a.test32(RAX, RAX);
			to_finish.push_back(a.jcc_fwd(0x85));             // jne
			a.cmp32i_mem(S_pc, at + 2);
			to_finish.push_back(a.jcc_fwd(0x85));             // jne
		}
		a.sub32i_mem(S_icount, 1);
		if (pc_stale)
			stale_rets.emplace_back(a.jcc_fwd(0x8e), stale_pc);   // jle（pc を書いてから ret へ）
		else
			to_ret.push_back(a.jcc_fwd(0x8e));                // jle

		slot = r == delayed;
		if (slot && at + 4 >= ROM_END)
			return nullptr;
	}

	// 最後の命令のあとで pc がまだ古ければ書く（終わりの処理と next_block が pc を見る）
	if (pc_stale)
		a.store32i(S_pc, stale_pc);

	// 終わりの処理: 3. と 4.
	const size_t finish = a.code.size();
	for (size_t p : to_finish)
		a.patch_to(p, finish);
	a.load32(RAX, C_test);
	a.test32(RAX, RAX);
	const size_t no_irq1 = a.jcc_fwd(0x84);
	a.load32(RAX, S_delay);
	a.test32(RAX, RAX);
	const size_t no_irq2 = a.jcc_fwd(0x85);
#if SMU_X64ASM_MODE == 32
	a.push(RBX);                     // cdecl: jit_irq(c)
	call(reinterpret_cast<void *>(&sh2_device::jit_irq));
	a.addrsp(4);
#else
	a.mov64(ARG0, RBX);
	call(reinterpret_cast<void *>(&sh2_device::jit_irq));
#endif
	a.patch(no_irq1);
	a.patch(no_irq2);
	a.sub32i_mem(S_icount, 1);

	const size_t ret = a.code.size();
	for (size_t p : to_ret)
		a.patch_to(p, ret);
#if SMU_X64ASM_MODE == 32
	a.imm32(RAX, u32(uintptr_t(next_block)));
	a.rr(0, {0xff}, 4, RAX);             // jmp eax
#else
	a.imm64(RAX, u64(uintptr_t(next_block)));
	a.rr(0, false, {0xff}, 4, RAX);      // jmp rax
#endif
	// pc が古いまま抜ける所: 書いてから ret へ
	for (const auto &[pos, v] : stale_rets) {
		a.patch_to(pos, a.code.size());
		a.store32i(S_pc, v);
		const size_t j = a.jmp_fwd();
		a.patch_to(j, ret);
	}

	if (used + a.code.size() > BUF_SIZE) {
		flush();
		// 捨てたので、呼び出し元の置き場も消えている。次の呼び出しで訳し直す
		return nullptr;
	}
	u8 *dst = static_cast<u8 *>(buf) + used;
	std::memcpy(dst, a.code.data(), a.code.size());
	used += a.code.size();
	return dst;
}

#elif defined(__aarch64__)

// arm64 backend. Block state lives in callee-saved registers, so helper calls
// cannot disturb it: x19 = cpu, x20 = state, x21 = ROM, x22 = RAM. The Apple
// C ABI passes arguments in x0-x7 and clobbers x0-x17 freely, which the
// helpers below are written against.
bool sh2_device::jit::init(sh2_device &cpu)
{
	using namespace a64;
	buf = exec_mem::alloc_rwx(BUF_SIZE);
	if (!buf)
		return false;
	if (sizeof(pages[0]) != 8)
		return false;
	const internal_sh2_state *st = cpu.m_sh2_state;
	const auto S = [st](const void *f) { return u32(intptr_t(f) - intptr_t(st)); };
	const u32 C_test = u32(intptr_t(&cpu.m_test_irq) - intptr_t(&cpu));
	// memory offsets go into the scaled imm12 field, so they must be 4-aligned
	if ((S(&st->pc) | S(&st->m_delay) | S(&st->icount) | C_test) & 3)
		return false;

	emitter a;
	// enter: preserve x19-x22 (the block registers plus x29/x30 need saving:
	// helper calls inside a block go through BLR, which overwrites x30 with
	// their return address, so the exit RET needs x30 restored from the stack).
	a.stp_x(X19, X20, X31, -16, true);
	a.stp_x(X21, X22, X31, -16, true);
	a.stp_x(X29, X30, X31, -16, true);
	a.mov_x(X19, X0);
	a.mov_x(X20, X1);
	a.mov_x(X21, X2);
	a.mov_x(X22, X3);
	a.mov_imm64(X17, u64(uintptr_t(&entry)));
	a.ldr_x(X16, X17, 0);
	a.br(X16);

	// next_block: the same checks jit_run makes; jump to the next compiled block, or return.
	// It calls nothing, so w0/w1 are free scratch (w16 is taken by the pc and is
	// clobbered by mov_imm64(X16, ...) below; x18 is reserved by the platform).
	const size_t next = a.code.size();
	std::vector<size_t> to_exit;
	a.ldr_w_big(W16, X20, S(&st->icount));
	a.cmp_imm(W16, 0);
	to_exit.push_back(a.b_cond(LE));
	a.ldr_w_big(W16, X20, S(&st->m_delay));
	to_exit.push_back(a.cbnz_w(W16));
	a.ldr_w_big(W16, X19, C_test);
	to_exit.push_back(a.cbnz_w(W16));
	a.ldr_w_big(W16, X20, S(&st->pc));
	a.mov_imm32(W17, ROM_END - 0x100);     // 0x3fff00 fits neither imm12 form: hold it in x17
	a.cmp_reg(W16, W17);
	to_exit.push_back(a.b_cond(HS));
	a.tst_imm(W16, 1);
	to_exit.push_back(a.b_cond(NE));
	a.mov_x(X0, X16);
	a.lsr_imm(W0, W0, 12);                 // w0 = pc >> 12
	a.and_imm(W1, W16, 0xfff);
	a.lsr_imm(W1, W1, 1);                  // w1 = (pc & 0xfff) >> 1
	a.mov_imm64(X16, u64(uintptr_t(pages.data())));
	a.ldr_x_uxtw3(X16, X16, W0);           // x16 = pages[pc >> 12]
	to_exit.push_back(a.cbz_x(X16));
	a.ldr_x_uxtw3(X16, X16, W1);           // x16 = pages[pc >> 12][(pc & 0xfff) >> 1]
	to_exit.push_back(a.cbz_x(X16));
	a.br(X16);
	for (size_t p : to_exit)
		a.patch_to(p, a.code.size());
	a.ldp_x(X29, X30, X31, 16, true);
	a.ldp_x(X21, X22, X31, 16, true);
	a.ldp_x(X19, X20, X31, 16, true);
	a.ret();

	exec_mem::copy_code(buf, a.code.data(), a.code.size() * 4);
	enter = reinterpret_cast<enter_t>(buf);
	next_block = static_cast<u8 *>(buf) + next * 4;
	base_used = used = a.code.size() * 4;
	return true;
}

// Compile one block. The skeleton follows the x86-64 version (see the comment at
// the top of this file and jit_run); only the emitted instructions differ.
// Inside a block x19 = cpu, x20 = state, x21 = ROM, x22 = RAM (set by enter);
// x0-x17 are scratch between helper calls and the helper call arguments.
sh2_device::jit::code_t sh2_device::jit::compile(sh2_device &cpu, u32 pc)
{
	using namespace a64;
	if (!buf && !init(cpu))
		return nullptr;

	// 直に読み書きする領域（mem_bus の fast() と同じ）。番地を折り返さない設定のときだけ訳す
	const mem_bus &bus = *cpu.m_program;
	if (cpu.m_am != 0xffffffff || cpu.m_decrypted_program != cpu.m_program || !bus.hot_rom() || !bus.hot_ram())
		return nullptr;
	const u32 rom_end   = bus.hot_rom_end();      // 含む
	const u32 ram_start = bus.hot_ram_start();
	const u32 ram_len   = bus.hot_ram_len();      // 端 - 始め
	if (rom_end < 0xffff || rom_end >= 0x40000000 || ram_len < 0xffff || ram_start <= rom_end)
		return nullptr;

	const internal_sh2_state *st = cpu.m_sh2_state;
	const auto S = [st](const void *f) -> u32 { return u32(intptr_t(f) - intptr_t(st)); };
	const u32 S_pc = S(&st->pc), S_delay = S(&st->m_delay), S_icount = S(&st->icount), S_ea = S(&st->ea);
	const u32 S_sr = S(&st->sr), S_pr = S(&st->pr), S_gbr = S(&st->gbr), S_vbr = S(&st->vbr);
	const u32 S_mach = S(&st->mach), S_macl = S(&st->macl);
	const auto R = [&](int n) -> u32 { return S(&st->r[n]); };
	const u32 C_test = u32(intptr_t(&cpu.m_test_irq) - intptr_t(&cpu));
	if (sizeof(st->pc) != 4 || sizeof(st->icount) != 4 || sizeof(cpu.m_test_irq) != 4 || sizeof(st->r[0]) != 4)
		return nullptr;
	// scaled imm12 fields: all state offsets the emitted code touches must be 4-aligned
	for (u32 o : {S_pc, S_delay, S_icount, S_ea, S_sr, S_pr, S_gbr, S_vbr, S_mach, S_macl, C_test})
		if (o & 3)
			return nullptr;

	emitter a;

	std::vector<size_t> to_finish, to_ret;

	// Helper calls: Apple C ABI, arguments in x0-x2
	const auto call = [&](void *fn) {
		a.mov_imm64_x17(u64(uintptr_t(fn)));
		a.blr_x17();
	};
	// Register roles by internal convention: w0 = loaded value / ALU result (also
	// the helper return value), w1 = address, w2 = value to store, w3 = scratch
	// for the T bit, w16/w17 = spare
	const auto sr_and = [&](u32 mask) {           // sr &= mask (from a register: the mask is arbitrary)
		a.ldr_w_big(W16, X20, S_sr);
		a.mov_imm32(W17, mask);
		a.and_reg(W16, W16, W17);
		a.str_w_big(W16, X20, S_sr);
	};
	const auto sr_or = [&](u32 mask) {            // sr |= mask
		a.ldr_w_big(W16, X20, S_sr);
		a.mov_imm32(W17, mask);
		a.orr_reg(W16, W16, W17);
		a.str_w_big(W16, X20, S_sr);
	};
	const auto mergeT = [&](u32 wbit) {           // T bit of sr := bit 0 of wbit
		a.ldr_w_big(W16, X20, S_sr);
		a.and_imm(W16, W16, ~u32(SH_T));
		a.orr_reg(W16, W16, wbit);
		a.str_w_big(W16, X20, S_sr);
	};
	const auto setT = [&]() {                     // T bit from bit 0 of w0
		a.and_imm(W17, W0, 1);
		mergeT(W17);
	};
	const auto dec_icount = [&](u32 k) {
		a.ldr_w_big(W16, X20, S_icount);
		a.sub_imm(W16, W16, k);
		a.str_w_big(W16, X20, S_icount);
	};
	// Read. Address in w1, value into w0 (sz bytes in big-endian order, no sign
	// extension). ROM and work RAM are read inline, anything else goes through
	// the interpreter helpers. Both regions are flat byte arrays (membus.h), so
	// the address indexes them directly ([Xn, Wm, UXTW]).
	const auto mread = [&](int sz) {
		std::vector<size_t> to_ram, to_slow, to_done;
		if (sz > 1) {
			a.tst_imm(W1, 1);
			to_slow.push_back(a.b_cond(NE));               // unaligned: use the helper
		}
		a.mov_imm32(W16, rom_end + 1 - u32(sz));
		a.cmp_reg(W1, W16);
		to_ram.push_back(a.b_cond(HI));                    // unsigned above rom_end
		if (sz == 1) a.ldrb_x(W0, X21, W1, false);
		else if (sz == 2) { a.ldrh_x(W0, X21, W1, false); a.rev16(W0, W0); }
		else { a.ldr_w_x(W0, X21, W1, false); a.rev32(W0, W0); }
		to_done.push_back(a.b());
		for (size_t p : to_ram) a.patch(p);
		a.mov_imm32(W16, ram_start);
		a.sub_reg(W17, W1, W16);                           // w17 = address - start of RAM
		a.mov_imm32(W16, ram_len + 1 - u32(sz));
		a.cmp_reg(W17, W16);
		to_slow.push_back(a.b_cond(HI));
		if (sz == 1) a.ldrb_x(W0, X22, W17, false);
		else if (sz == 2) { a.ldrh_x(W0, X22, W17, false); a.rev16(W0, W0); }
		else { a.ldr_w_x(W0, X22, W17, false); a.rev32(W0, W0); }
		to_done.push_back(a.b());
		for (size_t p : to_slow) a.patch(p);
		a.mov_x(X0, X19);                                  // cpu (address already in w1)
		call(sz == 1 ? reinterpret_cast<void *>(&sh2_device::jit_rb) :
		     sz == 2 ? reinterpret_cast<void *>(&sh2_device::jit_rw) : reinterpret_cast<void *>(&sh2_device::jit_rl));
		for (size_t p : to_done) a.patch(p);
	};
	// Write. Address in w1, value in w2
	const auto mwrite = [&](int sz) {
		std::vector<size_t> to_slow;
		if (sz > 1) {
			a.tst_imm(W1, 1);
			to_slow.push_back(a.b_cond(NE));
		}
		a.mov_imm32(W16, ram_start);
		a.sub_reg(W17, W1, W16);                           // w17 = address - start of RAM
		a.mov_imm32(W16, ram_len + 1 - u32(sz));
		a.cmp_reg(W17, W16);
		to_slow.push_back(a.b_cond(HI));
		if (sz == 1) a.strb_x(W2, X22, W17, false);
		else if (sz == 2) { a.rev16(W3, W2); a.strh_x(W3, X22, W17, false); }
		else { a.rev32(W3, W2); a.str_w_x(W3, X22, W17, false); }
		const size_t done = a.b();
		for (size_t p : to_slow) a.patch(p);
		a.mov_x(X0, X19);                                  // cpu (address in w1, value in w2)
		call(sz == 1 ? reinterpret_cast<void *>(&sh2_device::jit_wb) :
		     sz == 2 ? reinterpret_cast<void *>(&sh2_device::jit_ww) : reinterpret_cast<void *>(&sh2_device::jit_wl));
		a.patch(done);
	};
	const auto sext8 = [](u32 v) { return u32(s32(s8(v))); };

	// 解釈実行と同じことを機械語で書く。書けない命令は none を返す
	enum res { none, pure, memop, delayed, ends };
	const auto native = [&](u16 op, u32 at) -> res {
		const int n = (op >> 8) & 15, m = (op >> 4) & 15;
		const u32 pcv = at + 2;                                  // 実行中の pc
		const auto rd_ea = [&](int base, u32 disp, int sz) {    // ea = r[base] + disp を読む
			a.ldr_w_big(W1, X20, R(base));
			if (disp) { a.add_imm(W1, W1, disp); }
			a.str_w_big(W1, X20, S_ea);
			mread(sz);
		};
		const auto wr_ea = [&](int base, u32 disp, int src, int sz) {   // ea = r[base] + disp へ r[src] を書く
			a.ldr_w_big(W1, X20, R(base));
			if (disp) { a.add_imm(W1, W1, disp); }
			a.str_w_big(W1, X20, S_ea);
			a.ldr_w_big(W2, X20, R(src));
			mwrite(sz);
		};
		const auto to_r = [&](int d, int sz) {                   // w0 を符号拡張して r[d] へ
			if (sz == 1) a.sxtb(W0, W0);
			else if (sz == 2) a.sxth(W0, W0);
			a.str_w_big(W0, X20, R(d));
		};
		const auto cmp_t = [&](u32 cond) {
			a.ldr_w_big(W16, X20, R(n));
			a.ldr_w_big(W17, X20, R(m));
			a.cmp_reg(W16, W17);
			a.cset(W0, cond);
			setT();
			return pure;
		};	const auto copy = [&](u32 from, u32 to) { a.ldr_w_big(W16, X20, from); a.str_w_big(W16, X20, to); return pure; };
		const auto branch_to = [&](u32 target, bool delay) {
			a.mov_imm32(W16, target);
			a.str_w_big(W16, X20, delay ? S_delay : S_pc);
			a.str_w_big(W16, X20, S_ea);
			dec_icount(delay ? 1 : 2);
		};

		switch (op >> 12) {
		case 0x0:
			switch (op & 0x3f) {
			case 0x04: case 0x14: case 0x24: case 0x34:
			case 0x05: case 0x15: case 0x25: case 0x35:
			case 0x06: case 0x16: case 0x26: case 0x36: {       // MOV.x Rm,@(R0,Rn)
				const int sz = 1 << ((op & 15) - 4);
				a.ldr_w_big(W1, X20, R(n));
				a.ldr_w_big(W16, X20, R(0));
				a.add_reg(W1, W1, W16);
				a.str_w_big(W1, X20, S_ea);
				a.ldr_w_big(W2, X20, R(m));
				mwrite(sz);
				return memop;
			}
			case 0x0c: case 0x1c: case 0x2c: case 0x3c:
			case 0x0d: case 0x1d: case 0x2d: case 0x3d:
			case 0x0e: case 0x1e: case 0x2e: case 0x3e: {       // MOV.x @(R0,Rm),Rn
				const int sz = 1 << ((op & 15) - 12);
				a.ldr_w_big(W1, X20, R(m));
				a.ldr_w_big(W16, X20, R(0));
				a.add_reg(W1, W1, W16);
				a.str_w_big(W1, X20, S_ea);
				mread(sz);
				to_r(n, sz);
				return memop;
			}
			case 0x07: case 0x17: case 0x27: case 0x37:          // MUL.L
				a.ldr_w_big(W16, X20, R(n));
				a.ldr_w_big(W17, X20, R(m));
				a.mul(W16, W16, W17);
				a.str_w_big(W16, X20, S_macl);
				dec_icount(1);
				return pure;
			case 0x08: sr_and(~u32(SH_T)); return pure;                  // CLRT
			case 0x18: sr_or(SH_T); return pure;                         // SETT
			case 0x19: sr_and(~u32(SH_M | SH_Q | SH_T)); return pure;    // DIV0U
			case 0x09: return pure;                                      // NOP
			case 0x28:                                                   // CLRMAC
				a.str_w_big(WZR, X20, S_mach);
				a.str_w_big(WZR, X20, S_macl);
				return pure;
			case 0x02: return copy(S_sr, R(n));                          // STC SR,Rn
			case 0x12: return copy(S_gbr, R(n));
			case 0x22: return copy(S_vbr, R(n));
			case 0x0a: return copy(S_mach, R(n));                        // STS x,Rn
			case 0x1a: return copy(S_macl, R(n));
			case 0x2a: return copy(S_pr, R(n));
			case 0x29:                                                   // MOVT
				a.ldr_w_big(W16, X20, S_sr);
				a.and_imm(W16, W16, SH_T);
				a.str_w_big(W16, X20, R(n));
				return pure;
			case 0x0b:                                                   // RTS
				a.ldr_w_big(W16, X20, S_pr);
				a.str_w_big(W16, X20, S_delay);
				a.str_w_big(W16, X20, S_ea);
				dec_icount(1);
				return delayed;
			}
			return none;
		case 0x1:                                                        // MOV.L Rm,@(disp,Rn)
			wr_ea(n, (op & 15) * 4, m, 4);
			return memop;
		case 0x2:
			switch (op & 15) {
			case 0: case 1: case 2: {                                // MOV.x Rm,@Rn
				const int sz = 1 << (op & 15);
				wr_ea(n, 0, m, sz);
				return memop;
			}
			case 4: case 5: case 6: {                                // MOV.x Rm,@-Rn
				const int sz = 1 << ((op & 15) - 4);
				a.ldr_w_big(W2, X20, R(m));
				a.ldr_w_big(W16, X20, R(n));
				a.sub_imm(W16, W16, u32(sz));
				a.str_w_big(W16, X20, R(n));
				a.mov_reg(W1, W16);
				mwrite(sz);
				return memop;
			}
			case 8:                                                  // TST Rm,Rn
				a.ldr_w_big(W16, X20, R(n));
				a.ldr_w_big(W17, X20, R(m));
				a.ands_reg(W16, W16, W17);
				a.cset(W0, EQ);
				setT();
				return pure;
			case 9:                                                  // AND Rm,Rn
				a.ldr_w_big(W16, X20, R(m));
				a.ldr_w_big(W17, X20, R(n));
				a.and_reg(W17, W17, W16);
				a.str_w_big(W17, X20, R(n));
				return pure;
			case 10:                                                 // XOR Rm,Rn
				a.ldr_w_big(W16, X20, R(m));
				a.ldr_w_big(W17, X20, R(n));
				a.eor_reg(W17, W17, W16);
				a.str_w_big(W17, X20, R(n));
				return pure;
			case 11:                                                 // OR Rm,Rn
				a.ldr_w_big(W16, X20, R(m));
				a.ldr_w_big(W17, X20, R(n));
				a.orr_reg(W17, W17, W16);
				a.str_w_big(W17, X20, R(n));
				return pure;
			case 14:                                                 // MULU
				a.ldr_w_big(W16, X20, R(n));
				a.uxth(W16, W16);
				a.ldr_w_big(W17, X20, R(m));
				a.uxth(W17, W17);
				a.mul(W16, W16, W17);
				a.str_w_big(W16, X20, S_macl);
				return pure;
			case 15:                                                 // MULS
				a.ldr_w_big(W16, X20, R(n));
				a.sxth(W16, W16);
				a.ldr_w_big(W17, X20, R(m));
				a.sxth(W17, W17);
				a.mul(W16, W16, W17);
				a.str_w_big(W16, X20, S_macl);
				return pure;
			}
			return none;
		case 0x3:
			switch (op & 15) {
			case 0: return cmp_t(EQ);            // CMP/EQ
			case 2: return cmp_t(CS);            // CMP/HS
			case 3: return cmp_t(GE);            // CMP/GE
			case 6: return cmp_t(HI);            // CMP/HI
			case 7: return cmp_t(GT);            // CMP/GT
			case 8:                              // SUB Rm,Rn
				a.ldr_w_big(W16, X20, R(m));
				a.ldr_w_big(W17, X20, R(n));
				a.sub_reg(W17, W17, W16);
				a.str_w_big(W17, X20, R(n));
				return pure;
			case 12:                             // ADD Rm,Rn
				a.ldr_w_big(W16, X20, R(m));
				a.ldr_w_big(W17, X20, R(n));
				a.add_reg(W17, W17, W16);
				a.str_w_big(W17, X20, R(n));
				return pure;
			}
			return none;
		case 0x4:
			switch (op & 0x3f) {
			case 0x00: case 0x20:                                                    // SHLL / SHAL
				a.ldr_w_big(W16, X20, R(n));
				a.lsr_imm(W3, W16, 31);                  // bit shifted out to the left
				a.lsl_imm(W16, W16, 1);
				a.str_w_big(W16, X20, R(n));
				mergeT(W3);
				return pure;
			case 0x01: case 0x21:                                                    // SHLR / SHAR
				a.ldr_w_big(W16, X20, R(n));
				a.and_imm(W3, W16, 1);                   // bit shifted out to the right
				if ((op & 0x3f) == 0x01) a.lsr_imm(W16, W16, 1); else a.sar_imm(W16, W16, 1);
				a.str_w_big(W16, X20, R(n));
				mergeT(W3);
				return pure;
			case 0x08: case 0x18: case 0x28: {                                       // SHLL2/8/16
				const u32 sh = (op & 0x3f) == 0x08 ? 2 : (op & 0x3f) == 0x18 ? 8 : 16;
				a.ldr_w_big(W16, X20, R(n));
				a.lsl_imm(W16, W16, sh);
				a.str_w_big(W16, X20, R(n));
				return pure;
			}
			case 0x09: case 0x19: case 0x29: {                                       // SHLR2/8/16
				const u32 sh = (op & 0x3f) == 0x09 ? 2 : (op & 0x3f) == 0x19 ? 8 : 16;
				a.ldr_w_big(W16, X20, R(n));
				a.lsr_imm(W16, W16, sh);
				a.str_w_big(W16, X20, R(n));
				return pure;
			}
			case 0x10:                                                               // DT
				a.ldr_w_big(W16, X20, R(n));
				a.subs_imm(W16, W16, 1);
				a.str_w_big(W16, X20, R(n));
				a.cset(W0, EQ);
				setT();
				return pure;
			case 0x11:                                                               // CMP/PZ
				a.ldr_w_big(W16, X20, R(n));
				a.cmp_imm(W16, 0);
				a.cset(W0, GE);
				setT();
				return pure;
			case 0x15:                                                               // CMP/PL
				a.ldr_w_big(W16, X20, R(n));
				a.cmp_imm(W16, 0);
				a.cset(W0, GT);
				setT();
				return pure;
			case 0x0a: return copy(R(n), S_mach);                                    // LDS Rn,x
			case 0x1a: return copy(R(n), S_macl);
			case 0x2a: return copy(R(n), S_pr);
			case 0x1e: return copy(R(n), S_gbr);                                     // LDC Rn,GBR / VBR
			case 0x2e: return copy(R(n), S_vbr);
			case 0x02: case 0x12: case 0x22: {                                       // STS.L x,@-Rn
				const u32 src = (op & 0x3f) == 0x02 ? S_mach : (op & 0x3f) == 0x12 ? S_macl : S_pr;
				a.ldr_w_big(W16, X20, R(n));
				a.sub_imm(W16, W16, 4);
				a.str_w_big(W16, X20, R(n));
				a.mov_reg(W1, W16);
				a.str_w_big(W1, X20, S_ea);
				a.ldr_w_big(W2, X20, src);
				mwrite(4);
				return memop;
			}
			case 0x06: case 0x16: case 0x26: {                                       // LDS.L @Rn+,x
				const u32 dst = (op & 0x3f) == 0x06 ? S_mach : (op & 0x3f) == 0x16 ? S_macl : S_pr;
				a.ldr_w_big(W1, X20, R(n));
				a.str_w_big(W1, X20, S_ea);
				mread(4);
				a.str_w_big(W0, X20, dst);
				a.ldr_w_big(W16, X20, R(n));
				a.add_imm(W16, W16, 4);
				a.str_w_big(W16, X20, R(n));
				return memop;
			}
			case 0x0b:                                                               // JSR
				a.mov_imm32(W16, pcv + 2);
				a.str_w_big(W16, X20, S_pr);
				[[fallthrough]];
			case 0x2b:                                                               // JMP
				a.ldr_w_big(W16, X20, R(n));
				a.str_w_big(W16, X20, S_delay);
				a.str_w_big(W16, X20, S_ea);
				dec_icount(1);
				return delayed;
			}
			return none;
		case 0x5:                                                                    // MOV.L @(disp,Rm),Rn
			rd_ea(m, (op & 15) * 4, 4);
			to_r(n, 4);
			return memop;
		case 0x6:
			switch (op & 15) {
			case 0: case 1: case 2: {                                                // MOV.x @Rm,Rn
				const int sz = 1 << (op & 15);
				rd_ea(m, 0, sz);
				to_r(n, sz);
				return memop;
			}
			case 3: return copy(R(m), R(n));
			case 4: case 5: case 6: {                                                // MOV.x @Rm+,Rn
				const int sz = 1 << ((op & 15) - 4);
				a.ldr_w_big(W1, X20, R(m));
				mread(sz);
				to_r(n, sz);
				if (n != m) {
					a.ldr_w_big(W16, X20, R(m));
					a.add_imm(W16, W16, u32(sz));
					a.str_w_big(W16, X20, R(m));
				}
				return memop;
			}
			case 7:                                                                  // NOT
				a.ldr_w_big(W16, X20, R(m));
				a.mvn_reg(W16, W16);
				a.str_w_big(W16, X20, R(n));
				return pure;
			case 11:                                                                 // NEG
				a.ldr_w_big(W16, X20, R(m));
				a.neg_reg(W16, W16);
				a.str_w_big(W16, X20, R(n));
				return pure;
			case 12:                                                                 // EXTU.B
				a.ldrb(W16, X20, R(m));
				a.str_w_big(W16, X20, R(n));
				return pure;
			case 13:                                                                 // EXTU.W
				a.ldrh(W16, X20, R(m));
				a.str_w_big(W16, X20, R(n));
				return pure;
			case 14:                                                                 // EXTS.B
				a.ldrb(W16, X20, R(m));
				a.sxtb(W16, W16);
				a.str_w_big(W16, X20, R(n));
				return pure;
			case 15:                                                                 // EXTS.W
				a.ldrh(W16, X20, R(m));
				a.sxth(W16, W16);
				a.str_w_big(W16, X20, R(n));
				return pure;
			}
			return none;
		case 0x7: {                                                                  // ADD #imm,Rn
			a.ldr_w_big(W16, X20, R(n));
			const s32 imm = s32(s8(op & 0xff));
			if (imm >= 0) a.add_imm(W16, W16, u32(imm)); else a.sub_imm(W16, W16, u32(-imm));
			a.str_w_big(W16, X20, R(n));
			return pure;
		}
		case 0x8: {
			const u32 target = pcv + u32(s32(s8(op & 0xff)) * 2) + 2;
			switch (n) {
			case 0: wr_ea(m, op & 15, 0, 1); return memop;                          // MOV.B R0,@(disp,Rm)
			case 1: wr_ea(m, (op & 15) * 2, 0, 2); return memop;
			case 4: rd_ea(m, op & 15, 1); to_r(0, 1); return memop;                 // MOV.B @(disp,Rm),R0
			case 5: rd_ea(m, (op & 15) * 2, 2); to_r(0, 2); return memop;
			case 8:                                                                  // CMP/EQ #imm,R0
				a.ldr_w_big(W16, X20, R(0));
				a.mov_imm32(W17, sext8(op & 0xff));
				a.cmp_reg(W16, W17);
				a.cset(W0, EQ);
				setT();
				return pure;
			case 9: case 11: case 13: case 15: {                                    // BT / BF / BT/S / BF/S
				a.ldr_w_big(W16, X20, S_sr);
				a.tst_imm(W16, SH_T);
				const size_t skip = a.b_cond((n == 9 || n == 13) ? EQ : NE);
				branch_to(target, n >= 13);
				a.patch(skip);
				return n >= 13 ? delayed : ends;
			}
			}
			return none;
		}
		case 0x9: {                                                                  // MOV.W @(disp,PC),Rn
			const u32 ea = pcv + (op & 0xff) * 2 + 2;
			if (ea + 1 > rom_end)
				return none;
			a.mov_imm32(W16, ea);
			a.str_w_big(W16, X20, S_ea);
			a.mov_imm32(W16, u32(s32(s16(cpu.m_program->read_word(ea)))));
			a.str_w_big(W16, X20, R(n));
			return pure;
		}
		case 0xa:                                                                    // BRA
			branch_to(pcv + u32(util::sext(op & 0xfff, 12) * 2) + 2, true);
			return delayed;
		case 0xb:                                                                    // BSR
			a.mov_imm32(W16, pcv + 2);
			a.str_w_big(W16, X20, S_pr);
			branch_to(pcv + u32(util::sext(op & 0xfff, 12) * 2) + 2, true);
			return delayed;
		case 0xc: {
			const u32 d = op & 0xff;
			switch (n) {
			case 0: case 1: case 2: {                                                // MOV.x R0,@(disp,GBR)
				const int sz = 1 << n;
				a.ldr_w_big(W1, X20, S_gbr);
				a.add_imm(W1, W1, d * u32(sz));
				a.str_w_big(W1, X20, S_ea);
				a.ldr_w_big(W2, X20, R(0));
				mwrite(sz);
				return memop;
			}
			case 4: case 5: case 6: {                                                // MOV.x @(disp,GBR),R0
				const int sz = 1 << (n - 4);
				a.ldr_w_big(W1, X20, S_gbr);
				a.add_imm(W1, W1, d * u32(sz));
				a.str_w_big(W1, X20, S_ea);
				mread(sz);
				to_r(0, sz);
				return memop;
			}
			case 7: {                                                                // MOVA
				const u32 ea = ((pcv + 2) & ~3u) + d * 4;
				a.mov_imm32(W16, ea);
				a.str_w_big(W16, X20, S_ea);
				a.str_w_big(W16, X20, R(0));
				return pure;
			}
			case 8:                                                                  // TST #imm,R0
				a.ldr_w_big(W16, X20, R(0));
				a.mov_imm32(W17, d);
				a.ands_reg(W16, W16, W17);
				a.cset(W0, EQ);
				setT();
				return pure;
			case 9:                                                                  // AND #imm,R0
				a.ldr_w_big(W16, X20, R(0));
				a.mov_imm32(W17, d);
				a.and_reg(W16, W16, W17);
				a.str_w_big(W16, X20, R(0));
				return pure;
			case 10:                                                                 // XOR #imm,R0
				a.ldr_w_big(W16, X20, R(0));
				a.mov_imm32(W17, d);
				a.eor_reg(W16, W16, W17);
				a.str_w_big(W16, X20, R(0));
				return pure;
			case 11:                                                                 // OR #imm,R0
				a.ldr_w_big(W16, X20, R(0));
				a.mov_imm32(W17, d);
				a.orr_reg(W16, W16, W17);
				a.str_w_big(W16, X20, R(0));
				return pure;
			}
			return none;
		}
		case 0xd: {                                                                  // MOV.L @(disp,PC),Rn
			const u32 ea = ((pcv + 2) & ~3u) + (op & 0xff) * 4;
			if (ea + 3 > rom_end)
				return none;
			a.mov_imm32(W16, ea);
			a.str_w_big(W16, X20, S_ea);
			a.mov_imm32(W16, cpu.m_program->read_dword(ea));
			a.str_w_big(W16, X20, R(n));
			return pure;
		}
		case 0xe:                                                                    // MOV #imm,Rn
			a.mov_imm32(W16, sext8(op & 0xff));
			a.str_w_big(W16, X20, R(n));
			return pure;
		}
		return none;
	};

	// Writing the pc can wait: an instruction that only touches registers (pure)
	// never reads it, so it is written just before something that does -- a
	// peripheral-touching instruction, the interpreter, or leaving the block
	static const bool lazy_pc = [] {
		const char *e = std::getenv("SMU2000_SH2_LAZYPC");
		return !(e && e[0] == '0');
	}();
	static const bool slot_native = [] {
		const char *e = std::getenv("SMU2000_SH2_SLOTNATIVE");
		return !(e && e[0] == '0');
	}();
	// The two that are functions rather than flags of this function are read
	// once here instead of inside the loop below. Each read is a call to a
	// function with a function-local static (a guard check on every call), and
	// the loop asks for them two or three times per instruction compiled --
	// which is once per compiled block, not once per instruction executed, but
	// it is still the compile path that dominates boot
	const bool trace       = jit_trace_on();
	const bool emit_native = native_enabled();
	bool pc_stale = false;              // the pc in the state is behind
	u32 stale_pc = 0;                   // where it should point
	std::vector<std::pair<size_t, u32>> stale_rets;   // exits that write it first
	// Insert a pc store at an already-emitted position. Every branch fixed up so
	// far points before it, so the positions recorded so far do not move
	const auto store_pc_at = [&](size_t pos, u32 v) {
		emitter t;
		t.mov_imm32(W16, v);
		t.str_w_big(W16, X20, S_pc);
		a.code.insert(a.code.begin() + std::ptrdiff_t(pos), t.code.begin(), t.code.end());
	};

	bool slot = false;
	for (int i = 0; ; i++) {
		const u32 at = pc + 2 * u32(i);
		const u16 op = cpu.m_decrypted_program->read_word(at);
		const kind k = classify(op);

		if (trace) {
			if (pc_stale) {
				a.mov_imm32(W16, stale_pc);
				a.str_w_big(W16, X20, S_pc);
				pc_stale = false;
			}
			a.mov_x(X0, X19);
			a.mov_imm32(W1, at);
			call(reinterpret_cast<void *>(&sh2_device::jit_trace));
		}

		// 1. pc を進める
		if (slot) {
			a.ldr_w_big(W16, X20, S_delay);
			a.cmp_imm(W16, 0);
			const size_t no_delay = a.b_cond(EQ);
			a.str_w_big(W16, X20, S_pc);
			a.str_w_big(WZR, X20, S_delay);
			const size_t done = a.b();
			a.patch(no_delay);
			a.mov_imm32(W16, at + 2);
			a.str_w_big(W16, X20, S_pc);
			a.patch(done);
		} else if (!lazy_pc || trace) {
			a.mov_imm32(W16, at + 2);
			a.str_w_big(W16, X20, S_pc);
		}

		// 2. 実行する
		res r = none;
		const size_t op_begin = a.code.size();
		// A delay-slot instruction can be compiled as well, as long as it does not
		// read the pc: only MOV.W/MOV.L @(disp,PC) and MOVA do, and no branch lands
		// in a slot
		const bool pc_rel = (op >> 12) == 0x9 || (op >> 12) == 0xd || (op >> 8) == 0xc7;
		if (emit_native && (!slot || (slot_native && k == kind::normal && !pc_rel)))
			r = native(op, at);
		if (r == none) {
			if (!slot && lazy_pc && !trace) {
				a.mov_imm32(W16, at + 2);
				a.str_w_big(W16, X20, S_pc);
			}
			a.mov_x(X0, X19);
			a.movz(W1, op, 0);
			call(reinterpret_cast<void *>(&sh2_device::jit_exec));
			r = k == kind::delayed ? delayed : k == kind::ends ? ends : memop;
			pc_stale = false;
		} else if (!slot && lazy_pc && !trace) {
			if (r == pure) {
				pc_stale = true;
				stale_pc = at + 2;
			} else {
				store_pc_at(op_begin, at + 2);     // at the head of this instruction
				pc_stale = false;
			}
		}
		if (slot)
			pc_stale = false;

		// 分岐した・止まる命令・遅延スロットの後はブロックを終える
		if (slot || r == ends || (r != delayed && (i + 1 >= MAX_INSNS || at + 4 >= ROM_END)))
			break;

		// 3. 4. 途中の確かめ。周辺に触りうる命令は、割り込みの印か思わぬ pc の変化があれば終わりの処理へ
		if (r == memop) {
			a.ldr_w_big(W16, X19, C_test);
			to_finish.push_back(a.cbnz_w(W16));
			a.ldr_w_big(W16, X20, S_pc);
			a.mov_imm32(W17, at + 2);
			a.cmp_reg(W16, W17);
			to_finish.push_back(a.b_cond(NE));
		}
		a.ldr_w_big(W16, X20, S_icount);
		a.subs_imm(W16, W16, 1);
		a.str_w_big(W16, X20, S_icount);
		if (pc_stale)
			stale_rets.emplace_back(a.b_cond(LE), stale_pc);   // write the pc, then exit
		else
			to_ret.push_back(a.b_cond(LE));

		slot = r == delayed;
		if (slot && at + 4 >= ROM_END)
			return nullptr;
	}

	// The last instruction may have left the pc unwritten, and both the finish
	// section and next_block read it
	if (pc_stale) {
		a.mov_imm32(W16, stale_pc);
		a.str_w_big(W16, X20, S_pc);
	}

	// 終わりの処理: 3. と 4.
	const size_t finish = a.code.size();
	for (size_t p : to_finish)
		a.patch_to(p, finish);
	a.ldr_w_big(W16, X19, C_test);
	const size_t no_irq1 = a.cbz_w(W16);
	a.ldr_w_big(W16, X20, S_delay);
	const size_t no_irq2 = a.cbnz_w(W16);
	a.mov_x(X0, X19);
	call(reinterpret_cast<void *>(&sh2_device::jit_irq));
	a.patch(no_irq1);
	a.patch(no_irq2);
	dec_icount(1);

	const size_t ret = a.code.size();
	for (size_t p : to_ret)
		a.patch_to(p, ret);
	a.mov_imm64_x17(u64(uintptr_t(next_block)));
	a.br_x17();

	// Exits that were taken with a stale pc: write it, then take the same jump
	for (const auto &[pos, v] : stale_rets) {
		a.patch_to(pos, a.code.size());
		a.mov_imm32(W16, v);
		a.str_w_big(W16, X20, S_pc);
		const size_t j = a.b();
		a.patch_to(j, ret);
	}

	if (used + a.code.size() * 4 > BUF_SIZE) {
		flush();
		// 捨てたので、呼び出し元の置き場も消えている。次の呼び出しで訳し直す
		return nullptr;
	}
	u8 *dst = static_cast<u8 *>(buf) + used;
	exec_mem::copy_code(dst, a.code.data(), a.code.size() * 4);
	used += a.code.size() * 4;
	return dst;
}

#endif
