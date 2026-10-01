// license:BSD-3-Clause
//
// S-MU2000: MEG のプログラムをその場で x86-64 の機械語にする（JIT）。
//
// firmware が MEG に書いたプログラムを、プログラムか番地の割り当てが変わるたびに訳し直す。
// 訳した機械語は meg_state::run_program() と**ビット単位で同じこと**をする:
//
//   * 命令ごとの判定（ALU の種類・読み先・書き先・メモリ操作）は訳すときに決める
//   * 3 命令遅れの書き込み・2 命令遅れのメモリポートと t の値は、遅れの輪のどの枠かが
//     訳すときに決まるので、反映する命令の位置に直に置く。サンプルを跨ぐ分
//     （頭の 3 命令が読む枠と、終わりの 3 命令が書く枠）だけ輪そのものを使う
//   * 定数・番地表・LFO は firmware が動かしている最中にも書くので、実行時に読む
//   * 乱数（ディザ）は同じ順に同じ回数だけ引く
//
// 分岐（bit 0x3f）を含むプログラム（LO-FI と DYNA 系）は、遅れの輪を毎命令で読み書きする形で訳す。
// 訳した物はどこにも保存しない（firmware 由来のものを配らない。実行時に作って捨てる）。
//
// x86-64（Windows・macOS・Linux）と arm64 で使う。
// そのほかでは build() が false を返し、今までどおり解釈実行する。

#include "swp30.h"

// JIT を使うか:
//   第一段階 = x86-64 と arm64（MinGW の __x86_64__ と MSVC の _M_X64 の両方。これで MSVC x64 も JIT を使う）
//   第二段階 = x86-32 の移植（Phase 5）。SMU_JIT32_PORT_MEG をビルドで定義した時だけ JIT を有効にし、
//              SMU2000_MEG_JIT32 が建つ。マクロを切った Win32 は今までどおり解釈実行専用（動きは不変）
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86)) && !defined(SMU_JIT32_NO_MEG)
	#define SMU_JIT32_PORT_MEG        // win32 は既定で JIT 有効（最適化しない）。解除は SMU_JIT32_NO_MEG
#endif
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
#define SMU2000_MEG_JIT 1
#elif defined(_WIN32) && (defined(__i386__) || defined(_M_IX86)) && defined(SMU_JIT32_PORT_MEG)
#define SMU2000_MEG_JIT 1
#define SMU2000_MEG_JIT32 1
#else
#define SMU2000_MEG_JIT 0
#endif

#include <cstdlib>
#include <cstring>
#include <vector>

#if SMU2000_MEG_JIT
#include "compat/exec_mem.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#ifdef __aarch64__
#include "a64asm.h"
#else
#include "x64asm.h"
#endif
#endif

namespace {

// 機械語部品（x86-64 と x86-32 の両方）。64bit 命令はこの 3 つには無い（全て 32bit 命令）
// encode/decode が余りに使う scratch: x64 は R11 / R8、32bit は ESI（どちらの呼び所も ESI は空き）
#if SMU2000_MEG_JIT && !defined(__aarch64__)

using namespace x64asm;
using meg_asm = assembler;
using meg_arg_t = x64_arg0_t;

#if SMU_X64ASM_MODE == 32
	#define RENC ESI   // encode の余り scratch（x64 では R11）
	#define RDEC ESI   // decode の余り scratch（x64 では R8）
#else
	#define RENC R11
	#define RDEC R8
#endif

// meg_state::revram_encode と同じことをする。入力 eax（u32）、出力 eax（u16）。rcx rdx r11 を壊す。
// 分岐を使わない。符号は音の値しだいで読めないので、分岐にすると予測の外れで遅くなる
void emit_revram_encode(assembler &a)
{
	a.and32i(RAX, 0x7ffffff);
	a.mov32(RDX, RAX);
	a.shl32(RDX, 5);
	a.sar32(RDX, 31);                                    // bit 26 が立っていれば -1
	a.mov32(RCX, RDX);
	a.and32i(RCX, 0x7ffffff);
	a.xor32(RAX, RCX);
	a.and32i(RDX, 1);                                    // s
	// e は bit 11〜25 のうち一番上の 1 の位置 - 10。無ければ e = 0 で m = v（v < 0x800）
	a.mov32(RCX, RAX);
	a.or32ri(RCX, 0x400);
	a.bsr32(RCX, RCX);
	a.sub32ri(RCX, 10);                                  // e
#if SMU_X64ASM_MODE == 32
	// 32bit では setcc の先が ESI（modrm rm=6）だと REX が無く AH を壊す（SIL は REX 必須、EAX の上 8bit を壊すと仮数が出る）。
	// 分岐を使わず算術で e!=0 を作る（x64 側は従来通り）
	a.mov32(RENC, RCX);                                  // e
	a.sub32ri(RENC, 1);
	a.shr32(RENC, 31);                                   // e == 0 なら 1
	a.xor32ri(RENC, 1);                                  // e != 0
	a.sub32(RCX, RENC);                                  // e ? e - 1 : 0
	a.add32(RENC, RCX);                                  // e
#else
	a.xor32(RENC, RENC);
	a.test32(RCX, RCX);
	a.setcc(0x95, RENC);                                 // e != 0
	a.sub32(RCX, RENC);                                  // e ? e - 1 : 0
	a.add32(RENC, RCX);                                  // e
#endif
	a.shr32cl(RAX);
	a.and32i(RAX, 0x7ff);
	a.shl32(RENC, 12);
	a.or32(RAX, RENC);
	a.shl32(RDX, 11);
	a.or32(RAX, RDX);
}

// meg_state::m1_expand と同じことをする。入力 eax（下の 16bit が s16）、出力 rax（0〜0x7ffc）。rcx を壊す
void emit_m1_expand(assembler &a)
{
	a.test32ri(RAX, 0x8000);
	const size_t neg = a.jcc_fwd(0x85);
	a.mov32(RCX, RAX);
	a.shr32(RCX, 12);
	a.and32i(RCX, 7);                                    // s
	a.and32i(RAX, 0xfff);
	a.or32ri(RAX, 0x1000);
	a.cmp32ri(RCX, 5);
	const size_t done1 = a.jcc_fwd(0x84);                // s == 5
	const size_t less = a.jcc_fwd(0x82);                 // s < 5（jb）
	a.sub32ri(RCX, 5);
	a.shl32cl(RAX);
	const size_t done2 = a.jmp_fwd();
	a.patch(less);
	a.neg32(RCX);
	a.add32ri(RCX, 5);
	a.shr32cl(RAX);
	const size_t done3 = a.jmp_fwd();
	a.patch(neg);
	a.xor32(RAX, RAX);
	a.patch(done1);
	a.patch(done2);
	a.patch(done3);
}

// meg_state::revram_decode と同じことをする。入力 eax（u16）、出力 eax。rcx rdx と余り scratch を壊す。分岐を使わない
void emit_revram_decode(assembler &a)
{
	a.mov32(RDEC, RAX);                                  // v
	a.mov32(RCX, RAX);
	a.shr32(RCX, 12);                                    // e
	a.and32i(RAX, 0x7ff);                                // m
	a.xor32(RDX, RDX);
	a.test32(RCX, RCX);
	a.setcc(0x95, RDX);                                  // e != 0
	a.sub32(RCX, RDX);                                   // e ? e - 1 : 0
	a.shl32(RDX, 11);
	a.or32(RAX, RDX);                                    // e ? m | 0x800 : m
	a.shl32cl(RAX);
	a.imm32(RDX, 0xffffffff);
	a.shl32cl(RDX);                                      // 反転の範囲
	a.mov32(RCX, RDEC);
	a.shl32(RCX, 20);
	a.sar32(RCX, 31);                                    // s ? -1 : 0
	a.and32(RDX, RCX);
	a.xor32(RAX, RDX);
}

#elif defined(__aarch64__)

using namespace a64;
using meg_asm = emitter;
using meg_arg_t = u32;

// The three helpers work on the same scratch registers the JIT uses, so one can
// be dropped into a block exactly as it stands: HA carries the value in and
// out, HC/HD/HM are scratch. The W forms are used throughout because the JIT's
// x86 counterpart also works on the low 32 bits and the W writes clear the top
// half of the X register, which is what the callers expect to read back.
enum : u8 { HA = X2, HC = X3, HD = X4, HE = X5, HM = X7 };

// Same as meg_state::revram_encode. Input HA (u32), result HA (low 16 bits).
// HC and HD are clobbered.
void emit_revram_encode(emitter &a)
{
	a.and_imm(HA, HA, 0x7ffffff);
	a.eor_reg(HD, HD, HD);                               // s
	a.tst_imm(HA, 0x4000000);
	const size_t pos = a.b_cond(EQ);
	a.eor_imm(HA, HA, 0x7ffffff);
	a.mov_imm32(HD, 1);
	a.patch(pos);
	// e is the highest set bit minus 10 when the value reaches bit 11; below
	// that e = 0 and m is the value itself
	a.lsr_imm(HC, HA, 11);
	const size_t small = a.cbz_w(HC);
	a.clz(HC, HA);
	a.mov_imm32(HM, 31);
	a.sub_reg(HC, HM, HC);                               // bsr(v)
	a.sub_imm(HC, HC, 11);                               // e - 1
	a.lsrv(HA, HA, HC);
	a.and_imm(HA, HA, 0x7ff);
	a.add_imm(HC, HC, 1);                                // e
	a.lsl_imm(HC, HC, 12);
	a.orr_reg(HA, HA, HC);
	a.patch(small);
	a.lsl_imm(HD, HD, 11);
	a.orr_reg(HA, HA, HD);
}

// Same as meg_state::m1_expand. Input HA (a sign-extended s16 in the low half),
// result HA (0..0x7ffc). HC is clobbered.
void emit_m1_expand(emitter &a)
{
	a.tst_imm(HA, 0x8000);
	const size_t neg = a.b_cond(NE);
	a.lsr_imm(HC, HA, 12);
	a.and_imm(HC, HC, 7);                                // s
	a.and_imm(HA, HA, 0xfff);
	a.or_imm(HA, HA, 0x1000);
	a.cmp_imm(HC, 5);
	const size_t done1 = a.b_cond(EQ);
	const size_t less = a.b_cond(LO);
	a.sub_imm(HC, HC, 5);
	a.lslv(HA, HA, HC);
	const size_t done2 = a.b();
	a.patch(less);
	a.neg_reg(HC, HC);
	a.add_imm(HC, HC, 5);
	a.lsrv(HA, HA, HC);
	const size_t done3 = a.b();
	a.patch(neg);
	a.eor_reg(HA, HA, HA);
	a.patch(done1);
	a.patch(done2);
	a.patch(done3);
}

// Same as meg_state::revram_decode. Input HA (u16), result HA.
//
// The C++ version inverts every bit when e = 0, so that a negative value
// round-trips through revram_encode (doc/upstream.md #18). The inversion mask
// is therefore built at run time rather than being one constant per branch.
// HC, HD and HE are clobbered.
void emit_revram_decode(emitter &a)
{
	a.mov_reg(HE, HA);                                   // v
	a.lsr_imm(HC, HA, 12);                               // e
	a.and_imm(HA, HA, 0x7ff);                            // m
	a.cmp_imm(HC, 0);
	a.cset(HD, NE);                                      // e != 0
	a.sub_reg(HC, HC, HD);                               // e ? e - 1 : 0
	a.lsl_imm(HD, HD, 11);
	a.orr_reg(HA, HA, HD);                               // e ? m | 0x800 : m
	a.lslv(HA, HA, HC);
	a.mov_imm32(HD, 0xffffffff);
	a.lslv(HD, HD, HC);                                  // the inversion mask
	a.lsl_imm(HC, HE, 20);
	a.sar_imm(HC, HC, 31);                               // s ? -1 : 0
	a.and_reg(HD, HD, HC);
	a.eor_reg(HA, HA, HD);
}

#endif

} // namespace


struct swp30_device::meg_jit {
	using fn_t = void (*)(meg_state *, swp30_device *, u16 *);

	// 訳した機械語 1 本
	struct code {
		fn_t fn = nullptr;
		void *buf = nullptr;
		size_t buf_size = 0;
		u32 d3 = 0, d2 = 0;
		// S-MU2000: 訳したときのリバーブ RAM の区画の有効・無効（0x80e）。
		// 無効な区画への出し入れは訳すときに省くので、変わったら訳し直す
		u16 revram_enable = 0;
		~code()
		{
#if SMU2000_MEG_JIT
			if (buf)
				exec_mem::free_mem(buf, buf_size);
#endif
		}
	};

	// gen: 定数を実行時に読む版（いつでも使える）。spec: 今の定数を焼き込んだ版。
	// 定数は firmware がエフェクトを組むときにまとめて書き、そのあとはほとんど変わらない。
	// 変わってから STABLE サンプルのあいだ動かなければ spec を作り、次に変わった瞬間に gen へ戻す
	code gen, spec;
	const meg_state::op *ops = nullptr;
	u32 spec_const_gen = 0;
	u32 seen_const_gen = 0;
	u32 stable = 0;
	bool spec_tried = false;
	static constexpr u32 STABLE = 8192;

#if SMU2000_MEG_JIT
	static u32 call_lfo(meg_state *ms, u32 lfo) { return ms->get_lfo(int(lfo)); }
#endif

	bool build(code &c, meg_state &ms, const meg_state::op *ops, swp30_device &swp, bool bake);
};

void swp30_device::meg_jit_delete(meg_jit *j)
{
	delete j;
}

namespace {

// ---- The tuning switches, read once at startup
//
// File scope on purpose rather than function-local statics: a function-local
// static is only *initialised* once, but every read of it still costs a guard
// check, and meg_jit_run() is called once per audio sample (44100 times a
// second per MEG program). build() is only reached per program compiled, but it
// is the same idea
const bool g_meg_bake = [] {
	const char *e = std::getenv("SMU2000_MEG_BAKE");
	return !(e && e[0] == '0');
}();
const bool g_meg_check = [] {
	const char *e = std::getenv("SMU2000_MEG_JIT_CHECK");
	return e && e[0] != '0';
}();

// How many op codes of the block to emit and to step, for bisecting a
// divergence against the interpreter (SMU2000_MEG_JIT_UPTO). It only means
// anything together with g_meg_check, which runs the interpreter for the same
// number of steps, so without that switch this is the whole block: a stray
// variable must not be able to silently truncate a compiled program
constexpr u32 MEG_OPS = 0x180;
u32 meg_jit_upto()
{
	static const u32 upto = [] {
		if (!g_meg_check)
			return MEG_OPS;
		const char *e = std::getenv("SMU2000_MEG_JIT_UPTO");
		return e ? u32(std::atoi(e)) : MEG_OPS;
	}();
	return upto;
}

} // namespace

bool swp30_device::meg_jit_enabled()
{
#if SMU2000_MEG_JIT
	static const bool on = [] {
		const char *e = std::getenv("SMU2000_MEG_JIT");
		return !(e && e[0] == '0');
	}();
	return on;
#else
	return false;
#endif
}

void swp30_device::meg_jit_rebuild()
{
	if (!meg_jit_enabled()) {
		m_jit.reset();
		return;
	}
	if (!m_jit)
		m_jit.reset(new meg_jit);
	meg_jit &j = *m_jit;
	j.ops = m_meg_ops.data();
	if (!j.build(j.gen, *m_meg, j.ops, *this, false))
		j.gen.fn = nullptr;
	j.gen.revram_enable = m_revram_enable;
	// プログラムか番地が変わったので、焼き込んだ版は作り直す
	j.spec.fn = nullptr;
	j.spec_tried = false;
	j.seen_const_gen = m_meg_const_gen;
	j.stable = 0;
}

// 訳した物を使えなくする（次に meg_jit_rebuild() を呼ぶまで解釈実行で回る）
void swp30_device::meg_jit_invalidate()
{
	if (m_jit) {
		m_jit->gen.fn = nullptr;
		m_jit->spec.fn = nullptr;
	}
}

bool swp30_device::meg_jit_run()
{
	meg_jit *j = m_jit.get();
	if (!j || !j->gen.fn)
		return false;

	// S-MU2000: 区画の有効・無効が訳したときと変わっていたら、訳し直すまで解釈実行で回す。
	// revram_enable_w からも作り直しを頼んでいるが、取りこぼしても正しく鳴るように二重にしておく
	if (j->gen.revram_enable != m_revram_enable) {
		meg_jit_invalidate();
		m_meg_jit_wait = 1;
		return false;
	}

	meg_jit::code *c = &j->gen;
	if (g_meg_bake) {
		const u32 cg = m_meg_const_gen;
		if (cg != j->seen_const_gen) {
			j->seen_const_gen = cg;
			j->stable = 0;
			j->spec_tried = false;
		} else if (j->stable < meg_jit::STABLE)
			j->stable++;
		if (j->spec.fn && j->spec_const_gen == cg)
			c = &j->spec;
		else if (j->stable >= meg_jit::STABLE && !j->spec_tried) {
			j->spec_tried = true;
			if (j->build(j->spec, *m_meg, j->ops, *this, true)) {
				j->spec_const_gen = cg;
				c = &j->spec;
			} else
				j->spec.fn = nullptr;
		}
	}

	if (c->d3 != m_meg->m_delay_3 || c->d2 != m_meg->m_delay_2)
		return false;

	if (g_meg_check) {
		// DEBUG: run the block through the JIT, then through the interpreter from
		// the same starting state, and report the first field that differs.
		static int shown = 0;
		meg_state before(*m_meg);
		std::vector<u16> ram0(m_reverb_ram);
		u32 seed0 = m_rand_seed;
		bool fn0 = m_meg_flag_n, fz0 = m_meg_flag_z;
		c->fn(m_meg, this, m_reverb_ram.data());
		const meg_state jit(*m_meg);
		const std::vector<u16> ramj(m_reverb_ram);
		const u32 seedj = m_rand_seed;
		const bool fnj = m_meg_flag_n, fzj = m_meg_flag_z;
		*m_meg = before;
		m_reverb_ram = ram0;
		m_rand_seed = seed0;
		m_meg_flag_n = fn0;
		m_meg_flag_z = fz0;
		if (const u32 upto = meg_jit_upto(); upto != MEG_OPS) {
			for (u32 i = 0; i != upto; i++)
				m_meg->step();
		} else
			m_meg->run_program(m_meg_ops.data());
		const u8 *jb = reinterpret_cast<const u8 *>(&jit);
		const u8 *ib = reinterpret_cast<const u8 *>(m_meg);
		// run_program() subtracts the 0x180 icount itself, so the icount (and the
		// retval next to it, which only the helper-call paths write) is expected
		// to differ here; stop short of it
		const size_t end = size_t(reinterpret_cast<const u8 *>(&m_meg->m_icount) - ib);
		size_t first = end;
		for (size_t i = 0; i < end; i++)
			if (jb[i] != ib[i]) { first = i; break; }
		size_t rambad = 0, ramfirst = 0;
		for (size_t i = 0; i < ramj.size(); i++)
			if (ramj[i] != m_reverb_ram[i]) { if (!rambad) ramfirst = i; rambad++; }
		if ((first != end || rambad || seedj != m_rand_seed || fnj != m_meg_flag_n || fzj != m_meg_flag_z) && shown < 40) {
			shown++;
			std::fprintf(stderr, "MEGCHECK sample %d state@%zu/%zu ram %zu (first %zu) seed %s flags %s%s\n",
				shown, first, end, rambad, ramfirst,
				seedj == m_rand_seed ? "ok" : "BAD", fnj == m_meg_flag_n ? "ok" : "BAD", fzj == m_meg_flag_z ? "ok" : "BAD");
			if (shown == 1) {
				for (int b = 0; b < 0x40; b += 8) {
					std::fprintf(stderr, "  m[%02x..] jit", b);
					for (int i = 0; i < 8; i++) std::fprintf(stderr, " %8d", jit.m_m[b + i]);
					std::fprintf(stderr, "\n         interp");
					for (int i = 0; i < 8; i++) std::fprintf(stderr, " %8d", m_meg->m_m[b + i]);
					std::fprintf(stderr, "\n");
				}
				std::fprintf(stderr, "  jit    m32 %d m33 %d m48 %d m49 %d mwv %d,%d,%d mwr %d,%d,%d\n",
					jit.m_m[32], jit.m_m[33], jit.m_m[48], jit.m_m[49],
					jit.m_mw_value[0], jit.m_mw_value[1], jit.m_mw_value[2], jit.m_mw_reg[0], jit.m_mw_reg[1], jit.m_mw_reg[2]);
				std::fprintf(stderr, "  interp m32 %d m33 %d m48 %d m49 %d mwv %d,%d,%d mwr %d,%d,%d\n",
					m_meg->m_m[32], m_meg->m_m[33], m_meg->m_m[48], m_meg->m_m[49],
					m_meg->m_mw_value[0], m_meg->m_mw_value[1], m_meg->m_mw_value[2], m_meg->m_mw_reg[0], m_meg->m_mw_reg[1], m_meg->m_mw_reg[2]);
				for (u32 k = 0; k != 0x180; k++) {
					const meg_state::op &o = m_meg_ops[k];
					if (o.dm == 32 || o.dm == 33 || o.dm == 48 || o.dm == 49)
						std::fprintf(stderr, "  ops[%u] dm=%d dm_src=%d mmode=%d asel=%d rop=%d shift=%d clamp=%d sm=%d sr=%d alu=%d\n",
							k, o.dm, o.dm_src, o.mmode, o.asel, o.rop, o.shift, o.clamp, o.sm, o.sr, o.alu);
				}
				std::fprintf(stderr, "  before m32 %d m33 %d m48 %d m49 %d p %lld mwv %d,%d,%d mwr %d,%d,%d d3 %d d2 %d\n",
					before.m_m[32], before.m_m[33], before.m_m[48], before.m_m[49], (long long)before.m_p,
					before.m_mw_value[0], before.m_mw_value[1], before.m_mw_value[2], before.m_mw_reg[0], before.m_mw_reg[1], before.m_mw_reg[2], before.m_delay_3, before.m_delay_2);
			}
			for (int i = 0; i < 0x40; i++)
				if (jit.m_m[i] != m_meg->m_m[i]) std::fprintf(stderr, "  m[%d] jit %d interp %d\n", i, jit.m_m[i], m_meg->m_m[i]);
			for (int i = 0; i < 0x80; i++)
				if (jit.m_r[i] != m_meg->m_r[i]) std::fprintf(stderr, "  r[%d] jit %d interp %d\n", i, jit.m_r[i], m_meg->m_r[i]);
			for (int i = 0; i < 8; i++)
				if (jit.m_t[i] != m_meg->m_t[i]) std::fprintf(stderr, "  t[%d] jit %d interp %d\n", i, jit.m_t[i], m_meg->m_t[i]);
			if (jit.m_p != m_meg->m_p) std::fprintf(stderr, "  p jit %lld interp %lld\n", (long long)jit.m_p, (long long)m_meg->m_p);
			if (jit.m_ram_index != m_meg->m_ram_index) std::fprintf(stderr, "  ix jit %d interp %d\n", jit.m_ram_index, m_meg->m_ram_index);
			if (jit.m_ram_read != m_meg->m_ram_read) std::fprintf(stderr, "  rr jit %d interp %d\n", jit.m_ram_read, m_meg->m_ram_read);
			if (jit.m_ram_write != m_meg->m_ram_write) std::fprintf(stderr, "  rw jit %d interp %d\n", jit.m_ram_write, m_meg->m_ram_write);
			if (jit.m_delay_3 != m_meg->m_delay_3 || jit.m_delay_2 != m_meg->m_delay_2) std::fprintf(stderr, "  d3/d2 jit %d,%d interp %d,%d\n", jit.m_delay_3, jit.m_delay_2, m_meg->m_delay_3, m_meg->m_delay_2);
			for (int i = 0; i < 3; i++) {
				if (jit.m_mw_value[i] != m_meg->m_mw_value[i] || jit.m_mw_reg[i] != m_meg->m_mw_reg[i]) std::fprintf(stderr, "  mw[%d] jit %d/%d interp %d/%d\n", i, jit.m_mw_value[i], jit.m_mw_reg[i], m_meg->m_mw_value[i], m_meg->m_mw_reg[i]);
				if (jit.m_rw_value[i] != m_meg->m_rw_value[i] || jit.m_rw_reg[i] != m_meg->m_rw_reg[i]) std::fprintf(stderr, "  rw[%d] jit %d/%d interp %d/%d\n", i, jit.m_rw_value[i], jit.m_rw_reg[i], m_meg->m_rw_value[i], m_meg->m_rw_reg[i]);
				if (jit.m_index_value[i] != m_meg->m_index_value[i] || jit.m_index_active[i] != m_meg->m_index_active[i]) std::fprintf(stderr, "  ixv[%d] jit %d/%d interp %d/%d\n", i, jit.m_index_value[i], jit.m_index_active[i], m_meg->m_index_value[i], m_meg->m_index_active[i]);
				if (jit.m_memw_value[i] != m_meg->m_memw_value[i] || jit.m_memw_active[i] != m_meg->m_memw_active[i]) std::fprintf(stderr, "  memw[%d] jit %d/%d interp %d/%d\n", i, jit.m_memw_value[i], jit.m_memw_active[i], m_meg->m_memw_value[i], m_meg->m_memw_active[i]);
				if (jit.m_memr_value[i] != m_meg->m_memr_value[i] || jit.m_memr_active[i] != m_meg->m_memr_active[i]) std::fprintf(stderr, "  memr[%d] jit %d/%d interp %d/%d\n", i, jit.m_memr_value[i], jit.m_memr_active[i], m_meg->m_memr_value[i], m_meg->m_memr_active[i]);
			}
			for (int i = 0; i < 2; i++)
				if (jit.m_t_value[i] != m_meg->m_t_value[i]) std::fprintf(stderr, "  tv[%d] jit %d interp %d\n", i, jit.m_t_value[i], m_meg->m_t_value[i]);
		}
	}
	c->fn(m_meg, this, m_reverb_ram.data());
	m_meg->m_pc = 0;
	m_meg->m_icount -= 0x180;
	return true;
}

// 機械語にしたリバーブ RAM の詰め方・戻し方を、meg_state の関数と全部の入力で突き合わせる。
// 食い違った入力の数を返す（JIT が無い環境では 0）。make test の verify から呼ぶ
u64 swp30_device::meg_jit_selftest()
{
	// x86-32: cdecl で呼び、入口で eax に引数を移す。m1_expand も入出力とも 32bit に収まるので 3 つとも同じ形
#if SMU2000_MEG_JIT
	u64 bad = 0;
	for (int which = 0; which < 3; which++) {
		meg_asm a;
#if defined(__aarch64__)
		a.mov_reg(HA, W0);                              // the value arrives in w0
#elif SMU_X64ASM_MODE == 32
		a.push(ESI);                                     // RENC/RDEC は ESI。cdecl では callee-saved なのでセルフテストのスタブでは守る
		a.load32(RAX, mem{ RSP, NOREG, 1, 8 });          // cdecl: 引数（push ぶんずれた [esp+8]）
#else
		a.mov32(RAX, ARG0);
#endif
		if (which == 0) emit_revram_encode(a); else if (which == 1) emit_revram_decode(a); else emit_m1_expand(a);
#if defined(__aarch64__)
		a.mov_reg(W0, HA);                              // and the result goes back in w0
#elif SMU_X64ASM_MODE == 32
		a.pop(ESI);
#endif
		a.ret();
		// RW buffer, made executable after the copy (the code is bytes on x86
		// and 32-bit instructions on arm64, hence code[0] rather than a literal)
		const size_t bytes = a.code.size() * sizeof(a.code[0]);
		void *buf = exec_mem::alloc_rw(bytes);
		if (!buf)
			return ~u64(0);
		std::memcpy(buf, a.code.data(), bytes);
		if (!exec_mem::make_executable(buf, bytes)) {
			exec_mem::free_mem(buf, bytes);
			return ~u64(0);
		}
		const auto fn = reinterpret_cast<u32 (*)(meg_arg_t)>(buf);
		if (which == 0) {
			for (u32 v = 0; v < 0x8000000; v++)        // encode は下の 27bit しか見ない
				if ((fn(v) & 0xffff) != meg_state::revram_encode(v))
					bad++;
			for (u32 v : { 0xffffffffu, 0x80000000u, 0xf8000001u })
				if ((fn(v) & 0xffff) != meg_state::revram_encode(v))
					bad++;
		} else if (which == 1) {
			for (u32 v = 0; v < 0x10000; v++)
				if (fn(v) != meg_state::revram_decode(u16(v)))
					bad++;
		} else {
#if SMU_X64ASM_MODE == 32
			// m1_expand の入出力は 32bit に収まる（出力は 0〜0x7ffc）。MSVC x86 の cdecl: 引数 esp+4、戻り eax
			for (s32 v = -0x8000; v < 0x8000; v++)
				if (s32(fn(u32(v))) != s32(meg_state::m1_expand(s16(v))))
					bad++;
#else
			// 呼ぶ側は loads16 で 64bit に符号拡張した値を渡す。出力は 64bit のまま使う
			const auto fn64 = reinterpret_cast<s64 (*)(s64)>(buf);
			for (s32 v = -0x8000; v < 0x8000; v++)
				if (fn64(v) != s64(meg_state::m1_expand(s16(v))))
					bad++;
#endif
		}
		exec_mem::free_mem(buf, bytes);
	}
	return bad;
#else
	return 0;
#endif
}

// build(): JIT を使わなければ空（解釈実行）。使うなら x86-64 / x86-32 共用の 1 本
// （命令の出し分けは中の #if SMU_X64ASM_MODE == 32 で行う。x86-64 の出す機械語は従来と 1 バイトも同じ）
#if !SMU2000_MEG_JIT

bool swp30_device::meg_jit::build(code &, meg_state &, const meg_state::op *, swp30_device &, bool)
{
	return false;
}

#elif !defined(__aarch64__)	// x86（x64 と SMU2000_MEG_JIT32 の両モード。SMU_X64ASM_MODE で切り替える）

bool swp30_device::meg_jit::build(code &cd, meg_state &ms, const meg_state::op *ops, swp30_device &swp, bool bake)
{
	fn_t &fn = cd.fn;
	void *&buf = cd.buf;
	size_t &buf_size = cd.buf_size;
	u32 &d3 = cd.d3, &d2 = cd.d2;
	fn = nullptr;
	// 分岐（前へ飛ばすだけ）のあるプログラムは、遅れの輪を解釈実行と同じく毎命令で読み書きする形で訳す。
	// 飛ばされた命令は、その命令が輪に入れるはずだった書き込みを消し、t の値だけを入れる（run_program と同じ）
	bool branchy = false;
	for (u32 pc = 0; pc != 0x180; pc++)
		if (ops[pc].jump)
			branchy = true;
	if (swp.m_reverb_ram.size() < 0x40000)
		return false;

	d3 = ms.m_delay_3;
	d2 = ms.m_delay_2;
	const auto slot3 = [&](u32 k) { return (d3 + k) % 3; };
	const auto slot2 = [&](u32 k) { return (d2 + k) % 2; };

	// 要素の位置（meg_state と swp30_device の中）
	const auto off = [](const void *base, const void *field) { return s32(intptr_t(field) - intptr_t(base)); };
	const s32 o_m        = off(&ms, ms.m_m.data());
	const s32 o_r        = off(&ms, ms.m_r.data());
	const s32 o_t        = off(&ms, ms.m_t.data());
	const s32 o_p        = off(&ms, &ms.m_p);
	const s32 o_const    = off(&ms, ms.m_const.data());
	const s32 o_offset   = off(&ms, ms.m_offset.data());
	const s32 o_mw_value = off(&ms, ms.m_mw_value.data());
	const s32 o_mw_reg   = off(&ms, ms.m_mw_reg.data());
	const s32 o_rw_value = off(&ms, ms.m_rw_value.data());
	const s32 o_rw_reg   = off(&ms, ms.m_rw_reg.data());
	const s32 o_ix_value = off(&ms, ms.m_index_value.data());
	const s32 o_ix_act   = off(&ms, ms.m_index_active.data());
	const s32 o_memw_val = off(&ms, ms.m_memw_value.data());
	const s32 o_memr_val = off(&ms, ms.m_memr_value.data());
	const s32 o_t_value  = off(&ms, ms.m_t_value.data());
	const s32 o_memw_act = off(&ms, ms.m_memw_active.data());
	const s32 o_memr_act = off(&ms, ms.m_memr_active.data());
	const s32 o_ram_read = off(&ms, &ms.m_ram_read);
	const s32 o_ram_write = off(&ms, &ms.m_ram_write);
	const s32 o_ram_index = off(&ms, &ms.m_ram_index);
	const s32 o_sample   = off(&ms, &ms.m_sample_counter);
	const s32 o_lfo      = off(&ms, ms.m_lfo.data());
	const s32 o_lfo_counter = off(&ms, ms.m_lfo_counter.data());
	// get_lfo を機械語にするのは、sin 表が 1/4 周期ぶん（0x8000 個）そろっているときだけ
	const u16 *sintab = swp.m_sintab.count() >= 0x8000 ? swp.m_sintab.target() : nullptr;
	const s32 o_seed     = off(&swp, &swp.m_rand_seed);
	const s32 o_flag_n   = off(&swp, &swp.m_meg_flag_n);
	const s32 o_flag_z   = off(&swp, &swp.m_meg_flag_z);
	const s32 o_ix2_value = off(&swp, swp.m_meg_ix2_value.data());   // 2 つ目の idx（doc/upstream.md の 32）
	const s32 o_ix2_act   = off(&swp, swp.m_meg_ix2_act.data());
	const s32 o_ram_index2 = off(&swp, &swp.m_meg_ram_index2);
	const s32 o_skip     = off(&swp, &swp.m_meg_jit_skip);

	if (sizeof(ms.m_mw_reg[0]) != 1 || sizeof(ms.m_index_active[0]) != 1 || sizeof(ms.m_memw_active[0]) != 1 ||
	    sizeof(swp.m_meg_flag_n) != 1 || sizeof(ms.m_t_value[0]) != 2 || sizeof(ms.m_const[0]) != 2 ||
	    sizeof(ms.m_offset[0]) != 2 || sizeof(ms.m_m[0]) != 4 || sizeof(ms.m_r[0]) != 4)
		return false;

	// t の値（2 命令遅れ）を書いておく必要がある命令: 2 つ後に t を p から書く命令があるか、終わりの 2 つ
	bool need_tval[0x180] = {};
	for (u32 k = 0; k != 0x180; k++) {
		if (k >= 0x17e)
			need_tval[k] = true;
		if (k + 2 < 0x180 && ops[k + 2].t_write && ops[k + 2].t_from_p)
			need_tval[k] = true;
	}

	// 3 命令遅れの書き込みを、書いた命令の場所で直に入れてよいレジスタを調べる。
	// 読む命令の 1 つ前か 2 つ前（サンプルを跨いでも）に同じレジスタへの書き込みが無ければ、
	// どの時点で読んでも見える値は変わらない。終わりの 3 命令（0x17d-0x17f）が書くレジスタは、
	// 次のサンプルの頭で輪から入るので対象にしない
	bool early_r[128] = {}, early_m[128] = {};
	{
		bool bad_r[128] = {}, bad_m[128] = {};
		const auto reads = [&](const meg_state::op &o, bool m, u32 x) {
			if (!x)
				return false;
			bool rd = false;
			if (o.alu && (o.mmode == 2 || o.mmode == 3) && (o.m2_from_m != 0) == m && (m ? o.sm : o.sr) == x)
				rd = true;
			if (o.alu && !m && o.asel == 1 && o.sr == x)
				rd = true;
			if (o.alu && m && o.asel == 2 && o.sm == x)
				rd = true;
			if (!m && o.dr && o.dr_from_r && o.sr == x)
				rd = true;
			if (m && o.dm && o.dm_src == 7 && o.sm == x)
				rd = true;
			return rd;
		};
		for (u32 j = 0; j != 0x180; j++)
			for (u32 back = 1; back <= 2; back++) {
				const meg_state::op &w = ops[(j + 0x180 - back) % 0x180];
				if (w.dr && reads(ops[j], false, w.dr))
					bad_r[w.dr] = true;
				if (w.dm && reads(ops[j], true, w.dm))
					bad_m[w.dm] = true;
			}
		for (u32 k = 0x17d; k != 0x180; k++) {
			if (ops[k].dr) bad_r[ops[k].dr] = true;
			if (ops[k].dm) bad_m[ops[k].dm] = true;
		}
		static const bool early_on = [] {
			const char *e = std::getenv("SMU2000_MEG_EARLY");
			return !(e && e[0] == '0');
		}();
		for (u32 x = 1; x != 128; x++) {
			early_r[x] = early_on && !branchy && !bad_r[x];
			early_m[x] = early_on && !branchy && !bad_m[x];
		}
	}

	// 前倒しで書いた書き込みでも、輪の枠の最後の値になるもの（同じ枠へ後で書く命令が無いもの）は枠にも書く。
	// 読まれはしないが、状態の保存の中身を解釈実行と同じにしておく
	bool last_slot_r[0x180] = {}, last_slot_m[0x180] = {};
	for (u32 c = 0; c != 3; c++) {
		for (int k = 0x17f; k >= 0; k--)
			if (u32(k) % 3 == c && ops[k].dr) { last_slot_r[k] = true; break; }
		for (int k = 0x17f; k >= 0; k--)
			if (u32(k) % 3 == c && ops[k].dm) { last_slot_m[k] = true; break; }
	}

	// 何を出したかの内訳（SMU2000_MEG_JIT_STATS=1 のときだけ出す。どこを削ると効くかを見るため）
	struct jit_stats {
		u32 alu = 0, skipped = 0, clamp = 0, shift = 0, latch = 0;
		u32 wr_m = 0, wr_r = 0, early = 0, mem_r = 0, mem_w = 0, lfo = 0, tw = 0, chain = 0;
		u32 mul = 0, mul_pow2 = 0, dm_src[8] = {};
	} st;

	assembler a;
#if SMU_X64ASM_MODE == 32
	// 32bit: ebx=ms, edi=swp, ebp=ram（callee-saved の 3 つを pins）。eax ecx edx esi は scratch。
	// p(s64)・seed・sample・一時的な acc 退避は esp 基準の置き場。限界値は全部即値（load_p_limits 不要）
	const u8 MS = EBX, SWP = EDI, RAM = EBP;
	const s32 F_PLO = 0, F_PHI = 4, F_SEED = 8, F_SC = 12, F_ALO = 16, F_AHI = 20;
	[[maybe_unused]] const s32 LFO_SLOT_BASE = 24;                    // x86-32 では巻き上げをしない（使わない）
	const auto M = [&](s32 disp) { return mem{MS, NOREG, 1, disp}; };
	const auto FM = [&](s32 disp) { return mem{RSP, NOREG, 1, disp}; };
	[[maybe_unused]] const auto load_p_limits = [&]() {};

	// 入口（cdecl）。push4(16) + subrsp(24) = 40 → ms/swp/ram は esp+44/48/52（retaddr を含む引数は押し出し分ずれる）
	a.push(RBX); a.push(RSI); a.push(RDI); a.push(RBP);
	a.subrsp(24);                                    // 置き場 6 個ぶん
	a.load32(MS, mem{RSP, NOREG, 1, 44});
	a.load32(SWP, mem{RSP, NOREG, 1, 48});
	a.load32(RAM, mem{RSP, NOREG, 1, 52});
	a.load32(RAX, M(o_p)); a.store32(FM(F_PLO), RAX);
	a.load32(RAX, M(o_p + 4)); a.store32(FM(F_PHI), RAX);
	a.load32(RAX, M(o_sample)); a.store32(FM(F_SC), RAX);
	a.load32(RAX, mem{SWP, NOREG, 1, o_seed}); a.store32(FM(F_SEED), RAX);
	if (branchy)
		a.store32i(mem{SWP, NOREG, 1, o_skip}, 0);

	// p を 24bit に詰める（meg_pack24）。入出力 acc=(eax,edx)→eax
	const auto pack24 = [&]() {
		a.mov32(RCX, RAX); a.mov32(RSI, RDX);         // (ecx,esi) = p
		a.sar64(RCX, RSI, 63);                        // 負なら -1
		a.and32i(RCX, 0x7fff); a.xor32(RSI, RSI);     // 負なら 0x7fff（hi は 0）
		a.add64(RAX, RDX, RCX, RSI);                  // p += (負?0x7fff:0)
		a.sar64(RAX, RDX, 15);
		// 1 つだけはみ出したときは限界に止める（cf/ zf を壊さぬよう源レジスタは cmp 前に用意）
		a.imm32(RCX, 0x7fffff); a.xor32(RSI, RSI);      // K_MAX ペア
		a.cmp64i(RAX, RDX, 0x800000);                   // ==0x800000?
		a.cmovcc64(0x44, RAX, RDX, RCX, RSI);
		a.imm32(RCX, u32(s32(-0x800000))); a.imm32(RSI, 0xffffffffu);   // K_MIN ペア
		a.cmp64i(RAX, RDX, u32(s32(-0x800001)));        // ==-0x800001?
		a.cmovcc64(0x44, RAX, RDX, RCX, RSI);
		a.shl32(RAX, 8); a.sar32(RAX, 8);             // 24bit の符号拡張
	};
	// 乱数を 1 つ引く（swp30_device::rand）。出力 eax
	const auto rnd = [&]() {
		a.load32(RAX, FM(F_SEED));
		a.imul32i(RAX, RAX, 1664525);
		a.add32i(RAX, 1013904223);
		a.store32(FM(F_SEED), RAX);
		a.rol32(RAX, 16);
	};
	// p に雑音を足して詰める（dm の 6 番、dr の p）。出力 eax（acc=(eax,edx)）
	const auto p_packed = [&](bool noise) {
		if (noise) {
			rnd();
			a.and32i(RAX, 0x07e0);                    // 雑音（正）
			a.xor32(RSI, RSI);                        // 0（cf を壊さぬよう先に）
			a.mov32(RCX, RAX);                        // 雑音 lo
			a.load32(RAX, FM(F_PLO));                 // p lo
			a.add32(RAX, RCX);                        // lo 和。cf = 上がり
			a.load32(RDX, FM(F_PHI));                 // p hi（mov なので cf 保持）
			a.adc32(RDX, RSI);                        // cf を hi へ
		} else {
			a.load32(RAX, FM(F_PLO)); a.load32(RDX, FM(F_PHI));
		}
		pack24();
	};
	// p を acc=(eax,edx) へ／acc を n 右（算術）。結果の下 32bit は eax（この先は 32bit に収まる値）
	const auto AccFromP = [&]() { a.load32(RAX, FM(F_PLO)); a.load32(RDX, FM(F_PHI)); };
	const auto ShrAcc = [&](u8 n) { a.sar64(RAX, RDX, n); };
#else
	const u8 MS = RBX, SWP = R12, P = R13, SC = R14, RAM = R15, SEED = RSI, K_MAX = RDI, K_MIN = RBP;
	// LFO の値を 1 サンプルに 1 回だけ作って置いておく枠（24 個 × 4 バイト。下の emit_lfo）
	const s32 LFO_SLOT_BASE = 48;
	const s32 FRAME = 152;                               // 影 32 + 置き場 16 + LFO 96 + 揃え 8
	const auto FM = [&](s32 disp) { return mem{RSP, NOREG, 1, disp}; };
	const u8 P_MAX = R9, P_MIN = R10;                    // p の飽和の限界。r9 r10 は呼ぶ先で壊れるので、呼んだあと積み直す
	const auto load_p_limits = [&]() {
		a.imm64(P_MAX, 0x3fffffffff);
		a.imm64(P_MIN, u64(s64(-0x4000000000)));
	};   // SEED: 乱数の種を回しているあいだ持つ
	const auto M = [&](s32 disp) { return mem{MS, NOREG, 1, disp}; };

	// 入口（引数は ARG0 = ms、ARG1 = swp、ARG2 = リバーブ RAM。Windows x64 でも SysVでも）
	a.push(RBX); a.push(R12); a.push(R13); a.push(R14); a.push(R15); a.push(RSI); a.push(RDI); a.push(RBP);
	a.subrsp(FRAME);                                 // 影 32 + 置き場 16 + LFO 96 + 揃え 8。rsp は 16 の倍数
	a.mov64(MS, ARG0);
	a.mov64(SWP, ARG1);
	a.mov64(RAM, ARG2);
	a.load64(P, M(o_p));
	a.load32(SC, M(o_sample));
	a.load32(SEED, mem{SWP, NOREG, 1, o_seed});
	a.imm64(K_MAX, 0x7fffff);                            // pack24 の限界（即値を毎回積まないため）
	a.imm64(K_MIN, u64(s64(-0x800000)));
	load_p_limits();

	if (branchy)
		a.store32i(mem{SWP, NOREG, 1, o_skip}, 0);

	// p を 24bit に詰める（meg_pack24）。入力 rax、出力 eax
	const auto pack24 = [&]() {
		// meg_pack24 と同じく 0 の側へ切り捨てる（負なら 0x7fff を足してから右へ）
		a.mov64(RCX, RAX);
		a.sar64(RCX, 63);
		a.and32i(RCX, 0x7fff);
		a.add64(RAX, RCX);
		a.sar64(RAX, 15);
		// 1 つだけはみ出したときは限界に止め、ほかは 24bit で折り返す（meg_pack24 と同じ）
		a.cmp64ri(RAX, 0x800000);
		a.cmove64(RAX, K_MAX);
		a.cmp64ri(RAX, u32(s32(-0x800001)));
		a.cmove64(RAX, K_MIN);
		a.shl32(RAX, 8);
		a.sar32(RAX, 8);
	};
	// 乱数を 1 つ引く（swp30_device::rand）。出力 eax
	const auto rnd = [&]() {
		a.imul32i(RAX, SEED, 1664525);
		a.add32i(RAX, 1013904223);
		a.mov32(SEED, RAX);
		a.rol32(RAX, 16);
	};
	// p に雑音を足して詰める（dm の 6 番、dr の p）。出力 eax
	const auto p_packed = [&](bool noise) {
		if (noise) {
			rnd();
			a.and32i(RAX, 0x07e0);                    // 雑音（正の小さな値）
			a.add64(RAX, P);                          // そのまま p を足す（写しを 2 つ省く）
		} else
			a.mov64(RAX, P);
		pack24();
	};
	// p を acc(rax) へ／acc を n 右（算術）
	const auto AccFromP = [&]() { a.mov64(RAX, P); };
	const auto ShrAcc = [&](u8 n) { a.sar64(RAX, n); };
#endif

	// **LFO の値はプログラムを回している間ずっと変わらない**。数えるのを進める
	// lfo_step() はプログラムを回し終えた後に呼ばれ、設定（m_lfo）を書くのは CPU、
	// つまりサンプルとサンプルの合間だけ。なのに前は「使う命令ごとに」毎回 30 命令
	// ほどかけて作り直していた。使う番号ごとに頭で 1 回だけ作って置き場に入れ、
	// 使う命令は読むだけにする（出力はまったく同じ）。出力 eax。rcx rdx r8 を壊す
	const auto emit_lfo = [&](u32 idx) {
		static constexpr u32 lfo_offsets[16] = {
			0x00000, 0x02aaa, 0x04000, 0x05555, 0x08000, 0x0aaaa, 0x0c000, 0x0d555,
			0x10000, 0x12aaa, 0x14000, 0x15555, 0x18000, 0x1aaaa, 0x1c000, 0x1d555,
		};
		a.load32(RAX, M(o_lfo_counter + 4 * s32(idx)));
		a.shr32(RAX, 5);
		a.loadu16(RDX, M(o_lfo + 2 * s32(idx)));
		a.mov32(RCX, RDX);
		a.shr32(RCX, 8);
		a.and32i(RCX, 3);
		a.shl32cl(RAX);
		a.mov32(RCX, RDX);
		a.shr32(RCX, 12);
#if SMU_X64ASM_MODE == 32
		a.imm32(RSI, u32(uintptr_t(lfo_offsets)));
		a.load32(RCX, mem{RSI, RCX, 4, 0});
#else
		a.imm64(R8, u64(uintptr_t(lfo_offsets)));
		a.load32(RCX, mem{R8, RCX, 4, 0});
#endif
		a.add32(RAX, RCX);
		a.and32i(RAX, 0x1ffff);
		a.shr32(RDX, 10);
		a.and32i(RDX, 3);
		std::vector<size_t> done;
		a.test32(RDX, RDX);
		const size_t not_sine = a.jcc_fwd(0x85);
		{   // sine
			a.mov32(RCX, RAX);
			a.and32i(RCX, 0x7fff);
			a.test32ri(RAX, 0x8000);
			const size_t no_rev = a.jcc_fwd(0x84);
			a.xor32ri(RCX, 0x7fff);
			a.patch(no_rev);
#if SMU_X64ASM_MODE == 32
			a.imm32(RSI, u32(uintptr_t(sintab)));
			a.mov32(RDX, RAX);
			a.loadu16(RAX, mem{RSI, RCX, 2, 0});
#else
			a.imm64(R8, u64(uintptr_t(sintab)));
			a.mov32(RDX, RAX);
			a.loadu16(RAX, mem{R8, RCX, 2, 0});
#endif
			a.test32ri(RDX, 0x10000);
			const size_t no_neg = a.jcc_fwd(0x84);
			a.xor32ri(RAX, 0xffff);
			a.patch(no_neg);
			done.push_back(a.jmp_fwd());
		}
		a.patch(not_sine);
		a.cmp32ri(RDX, 1);
		const size_t not_tri = a.jcc_fwd(0x85);
		{   // tri
			a.add32ri(RAX, 0x8000);
			a.and32i(RAX, 0x1ffff);
			a.test32ri(RAX, 0x10000);
			const size_t no_fold = a.jcc_fwd(0x84);
			a.xor32ri(RAX, 0x1ffff);
			a.patch(no_fold);
			done.push_back(a.jmp_fwd());
		}
		a.patch(not_tri);
		a.cmp32ri(RDX, 2);
		const size_t not_up = a.jcc_fwd(0x85);
		a.shr32(RAX, 1);                                     // saw up
		done.push_back(a.jmp_fwd());
		a.patch(not_up);
		a.xor32ri(RAX, 0x1ffff);                             // saw down
		a.shr32(RAX, 1);
		for (size_t d : done) a.patch(d);
		a.shl32(RAX, 7);
	};

	// 2 回以上使う番号だけ頭で作る（1 回だけなら、置いて読む手間のぶん損になる）
	bool lfo_hoist[0x18] = {};
	s32 lfo_slot[0x18] = {};
	{
		u32 uses[0x18] = {};
		if (sintab)
			for (u32 k = 0; k != 0x180; k++)
				if (ops[k].dm && ops[k].dm_src <= 3 && ops[k].lfo < 0x18)
					uses[ops[k].lfo]++;
#if SMU_X64ASM_MODE != 32
		u32 n = 0;
		for (u32 i = 0; i != 0x18; i++)
			if (uses[i] >= 2) {
				lfo_hoist[i] = true;
				lfo_slot[i] = LFO_SLOT_BASE + s32(4 * n);
				n++;
				emit_lfo(i);
				a.store32(FM(lfo_slot[i]), RAX);
			}
#endif
	}

	// 命令ごとに出したバイト数を、どの仕事のぶんかで数える（統計のときだけ）
	size_t sz_ring = 0, sz_alu = 0, sz_dm = 0, sz_dr = 0, sz_ix = 0, sz_t = 0, sz_mark = a.code.size();
	for (u32 k = 0; k != 0x180; k++) {
		const meg_state::op &o = ops[k];
		sz_mark = a.code.size();

		// ---- 反映（遅れて入る書き込み）----
		if (k < 3 || branchy) {
			const u32 s = slot3(k);
			// m
			a.loadu8(RAX, M(o_mw_reg + s));
			a.test32(RAX, RAX);
			size_t j1 = a.jz_fwd();
			a.load32(RCX, M(o_mw_value + 4 * s));
			a.store32(mem{MS, RAX, 4, o_m}, RCX);
			a.patch(j1);
			// r
			a.loadu8(RAX, M(o_rw_reg + s));
			a.test32(RAX, RAX);
			size_t j2 = a.jz_fwd();
			a.load32(RCX, M(o_rw_value + 4 * s));
			a.store32(mem{MS, RAX, 4, o_r}, RCX);
			a.patch(j2);
			// index
			a.loadu8(RAX, M(o_ix_act + s));
			a.test32(RAX, RAX);
			size_t j3 = a.jz_fwd();
			a.load32(RCX, M(o_ix_value + 4 * s));
			a.store32(M(o_ram_index), RCX);
			a.patch(j3);
			// 2 つ目の index
			a.loadu8(RAX, mem{SWP, NOREG, 1, s32(o_ix2_act + s)});
			a.test32(RAX, RAX);
			size_t j4 = a.jz_fwd();
			a.load32(RCX, mem{SWP, NOREG, 1, o_ix2_value + 4 * s32(s)});
			a.store32(mem{SWP, NOREG, 1, o_ram_index2}, RCX);
			a.patch(j4);
		} else {
			const meg_state::op &w = ops[k - 3];
			const u32 s = slot3(k);
			if (w.dm && !early_m[w.dm]) {
				a.load32(RCX, M(o_mw_value + 4 * s));
				a.store32(M(o_m + 4 * w.dm), RCX);
			}
			if (w.dr && !early_r[w.dr]) {
				a.load32(RCX, M(o_rw_value + 4 * s));
				a.store32(M(o_r + 4 * w.dr), RCX);
			}
			if (w.index) {
				a.load32(RCX, M(o_ix_value + 4 * s));
				a.store32(M(o_ram_index), RCX);
			}
			if (w.index2) {
				a.load32(RCX, mem{SWP, NOREG, 1, o_ix2_value + 4 * s32(s)});
				a.store32(mem{SWP, NOREG, 1, o_ram_index2}, RCX);
			}
		}
		if (k < 2 || branchy) {
			const u32 s = slot2(k);
			a.loadu8(RAX, M(o_memw_act + s));
			a.test32(RAX, RAX);
			size_t j1 = a.jz_fwd();
			a.load32(RCX, M(o_memw_val + 4 * s));
			a.store32(M(o_ram_write), RCX);
			a.store8i(M(o_memw_act + s), 0);
			a.patch(j1);
			a.loadu8(RAX, M(o_memr_act + s));
			a.test32(RAX, RAX);
			size_t j2 = a.jz_fwd();
			a.load32(RCX, M(o_memr_val + 4 * s));
			a.store32(M(o_ram_read), RCX);
			a.store8i(M(o_memr_act + s), 0);
			a.patch(j2);
		} else {
			const meg_state::op &w = ops[k - 2];
			const u32 s = slot2(k);
			if (w.memw) {
				a.load32(RCX, M(o_memw_val + 4 * s));
				a.store32(M(o_ram_write), RCX);
			}
			if (w.memop == 2 || w.memop == 3) {
				a.load32(RCX, M(o_memr_val + 4 * s));
				a.store32(M(o_ram_read), RCX);
			}
		}

		// ---- 分岐と、飛ばされた命令 ----
		size_t skip_jump = 0, jump_done = 0;
		if (branchy) {
			a.cmp32i_mem(mem{SWP, NOREG, 1, o_skip}, k);
			skip_jump = a.jcc_fwd(0x87);                             // ja: 飛ばす位置がこの命令より後
			if (o.jump) {
				// meg_cond を機械語で
				size_t no_jump = 0;
				if (o.cond & 8) {
					a.loadu8(RAX, mem{SWP, NOREG, 1, o_flag_n});
					if (!(o.cond & 4))
						a.xor32ri(RAX, 1);
					if (o.cond & 2) {
						a.loadu8(RCX, mem{SWP, NOREG, 1, o_flag_z});
						a.or32(RAX, RCX);
					}
					a.test32(RAX, RAX);
					no_jump = a.jz_fwd();
				}
				if (o.target > k)
					a.store32i(mem{SWP, NOREG, 1, o_skip}, o.target);
				if (no_jump)
					a.patch(no_jump);
				jump_done = a.jmp_fwd();                             // 分岐の命令そのものも、飛ばされた命令と同じ後始末をする
			}
		}
		if (!(branchy && o.jump)) {

		// ---- ALU ----
		// bake のときは定数（と、そこから決まる m1）を焼き込む。係数 0 の掛け算は省き、
		// 「p = 0 * x + p」のように p が変わらない命令は丸ごと省く（結果はビット単位で同じ）
		bool alu_skip = false;
		if (o.alu && bake && !o.m1_from_t && o.mmode != 3) {
			s64 c = ms.m_const[k];
			if (o.m1_expand)
				c = meg_state::m1_expand(s16(c));
			const bool m_zero = o.mmode == 0 || c == 0;
			if (m_zero && o.asel == 0 && o.rop == 0 && o.shift == 0 && o.clamp == 0 && !o.latch)
				alu_skip = true;                                 // p はもう 42bit に収まっている
			else if (m_zero) {
				a.xor32(RAX, RAX);
#if SMU_X64ASM_MODE == 32
				a.xor32(RDX, RDX);                         // acc=(eax,edx)。hi も 0
#endif
			}
			else if (o.mmode == 1)
#if SMU_X64ASM_MODE == 32
				{ a.imm32(RAX, u32(u64(c) << (8 + 15))); a.imm32(RDX, u32(c >> 9)); }   // hi は算術右シフト（c<0 でも符号を保つ）
#else
				a.imm64(RAX, u64(c << (8 + 15)));
#endif
			else {
#if SMU_X64ASM_MODE == 32
				// acc=(eax,edx)。積の hi は imul64i の制約で esp 不可。hi を esi に置いて掛け、eax,edx に戻す
				a.load32(RAX, o.m2_from_m ? M(o_m + 4 * o.sm) : M(o_r + 4 * o.sr));  // lo（movsx は使わない＝32bit で読む）
				a.mov32(RSI, RAX); a.sar32(RSI, 31);                                 // hi = 符号
				a.imul64i(RAX, RSI, u32(s32(c)));                                    // 結果 eax(lo), esi(hi)
				a.mov32(RDX, RSI);
#else
				a.loads32(RAX, o.m2_from_m ? M(o_m + 4 * o.sm) : M(o_r + 4 * o.sr));
				// **2 の冪の係数は掛け算をやめてずらす**。imul は 3 サイクル、
				// shl は 1 サイクル。曲の中の係数の 1〜2 割がこれに当たる。
				// 符号を反転するぶん（neg）を足しても掛け算より短い
				{
					const s64 ac = c < 0 ? -c : c;
					int sh = -1;
					if (ac && !(ac & (ac - 1)))
						for (sh = 0; (s64(1) << sh) != ac; sh++)
							;
					if (sh < 0)
						a.imul64i(RAX, RAX, u32(s32(c)));
					else {
						if (sh)
							a.shl64(RAX, u8(sh));
						if (c < 0)
							a.neg64(RAX);
					}
				}
#endif
			}
		}
		sz_ring += a.code.size() - sz_mark;
		sz_mark = a.code.size();
		if (o.alu)
			st.alu++;
		if (o.alu && alu_skip)
			st.skipped++;
		if (o.alu && !alu_skip) {
			if (o.clamp) st.clamp++;
			if (o.shift) st.shift++;
			if (o.latch) st.latch++;
			// asel==0 は「前の p をそのまま使う」＝ p の鎖が切れない命令
			if (o.asel == 0) st.chain++;
		}
		if (o.dm) st.wr_m++;
		if (o.dr) st.wr_r++;
		if (o.dm && early_m[o.dm]) st.early++;
		if (o.dr && early_r[o.dr]) st.early++;
		if (o.memw) st.mem_w++;
		if (o.memop == 2 || o.memop == 3) st.mem_r++;
		if (o.dm && o.dm_src <= 3) st.lfo++;
		if (o.dm) st.dm_src[o.dm_src & 7]++;
		if (o.alu && !alu_skip && !o.m1_from_t && o.mmode == 2) {
			const s64 c = o.m1_expand ? meg_state::m1_expand(s16(ms.m_const[k])) : s64(ms.m_const[k]);
			st.mul++;
			const s64 ac = c < 0 ? -c : c;
			if (ac && !(ac & (ac - 1))) st.mul_pow2++;
		}
		if (o.t_write) st.tw++;
		if (o.alu && !alu_skip) {
			if (!(bake && !o.m1_from_t && o.mmode != 3)) {
#if SMU_X64ASM_MODE == 32
			// 掛ける値 m1 を eax に。acc は (eax,edx)。mmode==2 だけ hi を esi に置く（imul64 の制約）
			if (o.m1_from_t == 2) {
				// 印（負）が立っていれば t、そうでなければ定数（meg_state::step と同じ、doc/upstream.md の 29）
				a.loads16(ECX, M(o_t + 2 * o.t));                  // t（負のとき使う）
				a.loads16(RAX, M(o_const + 2 * s32(k)));           // 定数
				a.loadu8(RDX, mem{SWP, NOREG, 1, o_flag_n});
				a.test32(RDX, RDX);
				a.cmovcc64(0x44, RAX, RDX, RCX, RDX);              // zf（flag_n==0）なら定数 ecx を選択（hi は元 acc_hi のまま）
			} else if (o.m1_from_t)
				a.loads16(RAX, M(o_t + 2 * o.t));
			else
				a.loads16(RAX, M(o_const + 2 * s32(k)));
			if (o.m1_expand)
				emit_m1_expand(a);                           // 0〜0x7ffc。ecx を壊す
			switch (o.mmode) {
			case 0:
				a.xor32(RAX, RAX); a.xor32(RDX, RDX);
				break;
			case 1:
				a.mov32(RDX, RAX); a.sar32(RDX, 31); a.shl64(RAX, RDX, 8 + 15);   // m1 の符号を hi に（m1_expand 済みなら 0）
				break;
			case 2:
				// acc=(eax,esi)。掛ける相手を ecx:ebx に置く（ebx は MS を一時的に預ける）
				if (o.m1_expand) a.xor32(RSI, RSI);
				else { a.mov32(RSI, RAX); a.sar32(RSI, 31); }
				a.load32(ECX, o.m2_from_m ? M(o_m + 4 * o.sm) : M(o_r + 4 * o.sr));
				a.push(RBX);
				a.mov32(RBX, RCX); a.sar32(RBX, 31);         // 相手の hi
				a.imul64(RAX, RSI, ECX, RBX);
				a.mov32(RDX, RSI);
				a.pop(RBX);
				break;
			default:
				// mmode==3: m/r の値を 15bit 左（m1 は使わない。x64 と同じくロードし直す）
				a.load32(RAX, o.m2_from_m ? M(o_m + 4 * o.sm) : M(o_r + 4 * o.sr));
				a.mov32(RDX, RAX); a.sar32(RDX, 31); a.shl64(RAX, RDX, 15);
				break;
			}
#else
			if (o.m1_from_t == 2) {
				// 印（負）が立っていれば t、そうでなければ定数（meg_state::step と同じ、doc/upstream.md の 29）
				a.loads16(RAX, M(o_t + 2 * o.t));
				a.loads16(RCX, M(o_const + 2 * s32(k)));
				a.loadu8(RDX, mem{SWP, NOREG, 1, o_flag_n});
				a.test32(RDX, RDX);
				a.cmove64(RAX, RCX);
			} else if (o.m1_from_t)
				a.loads16(RAX, M(o_t + 2 * o.t));
			else
				a.loads16(RAX, M(o_const + 2 * s32(k)));
			if (o.m1_expand)
				emit_m1_expand(a);                           // meg_state::m1_expand を機械語で
			switch (o.mmode) {
			case 0:
				a.xor32(RAX, RAX);
				break;
			case 1:
				a.shl64(RAX, 8 + 15);
				break;
			case 2:
				a.loads32(RCX, o.m2_from_m ? M(o_m + 4 * o.sm) : M(o_r + 4 * o.sr));
				a.imul64(RAX, RCX);
				break;
			default:
				a.loads32(RAX, o.m2_from_m ? M(o_m + 4 * o.sm) : M(o_r + 4 * o.sr));
				a.shl64(RAX, 15);
				break;
			}
#endif
			}
			// 足す相手を置くレジスタ。x86-64 で asel==0（p そのもの）のときは
			// **p の居るレジスタを直に使い**、rcx への写しを 1 つ省く
#if SMU_X64ASM_MODE != 32
			u8 B = RCX;
#endif
			switch (o.asel) {
#if SMU_X64ASM_MODE == 32
			// acc=(eax,edx)。相手 b を (ecx,esi) に
			case 0: a.load32(RCX, FM(F_PLO)); a.load32(RSI, FM(F_PHI)); break;
			case 1: a.load32(RCX, M(o_r + 4 * o.sr)); a.mov32(RSI, RCX); a.sar32(RSI, 31); a.shl64(RCX, RSI, 15); break;
			case 2: a.load32(RCX, M(o_m + 4 * o.sm)); a.mov32(RSI, RCX); a.sar32(RSI, 31); a.shl64(RCX, RSI, 15); break;
			case 3: a.load32(RCX, FM(F_PLO)); a.load32(RSI, FM(F_PHI)); a.sar64(RCX, RSI, 15); break;
			default: a.xor32(RCX, RCX); a.xor32(RSI, RSI); break;
#else
			case 0: B = P; break;                          // 写さずに p のレジスタを使う
			case 1: a.loads32(RCX, M(o_r + 4 * o.sr)); a.shl64(RCX, 15); break;
			case 2: a.loads32(RCX, M(o_m + 4 * o.sm)); a.shl64(RCX, 15); break;
			case 3: a.mov64(RCX, P); a.sar64(RCX, 15); break;
			default: a.xor32(RCX, RCX); break;
#endif
			}
			switch (o.rop) {
			case 0:
#if SMU_X64ASM_MODE == 32
				a.add32(RAX, RCX); a.adc32(RDX, RSI);
#else
				a.add64(RAX, B);
#endif
				break;
			case 1:
#if SMU_X64ASM_MODE == 32
				a.sub32(RAX, RCX); a.sbb32(RDX, RSI);
#else
				a.sub64(RAX, B);
#endif
				break;
			case 2:
#if SMU_X64ASM_MODE == 32
				// a + |b|。acc=(eax,edx) を一時スロットへ退避し、|b| を eax:edx に作って足す
				a.store32(FM(F_ALO), RAX); a.store32(FM(F_AHI), RDX);
				a.mov32(RAX, RCX); a.mov32(RDX, RSI);          // (eax,edx) = b
				a.neg64(RAX, RDX);                              // -b（sf = その符号）
				a.cmovs64(RAX, RDX, RCX, RSI);                  // b が正なら b を戻す → |b|
				a.add32rm(RAX, FM(F_ALO));                       // + a_lo
				a.load32(RCX, FM(F_AHI));                        // a_hi（mov なので cf は保持）
				a.adc32(RDX, RCX);
#else
				a.mov64(RDX, B);
				a.neg64(RDX);
				a.cmovs64(RDX, B);
				a.add64(RAX, RDX);
#endif
				break;
			default:
#if SMU_X64ASM_MODE == 32
				a.and32(RAX, RCX); a.and32(RDX, RSI);
#else
				a.and64(RAX, B);
#endif
				break;
			}
			if (o.shift)
#if SMU_X64ASM_MODE == 32
				a.shl64(RAX, RDX, o.shift);
#else
				a.shl64(RAX, o.shift);
#endif
			if (o.clamp == 0) {
#if SMU_X64ASM_MODE == 32
				a.shl64(RAX, RDX, 22);
				a.sar64(RAX, RDX, 22);
#else
				a.shl64(RAX, 22);
				a.sar64(RAX, 22);
#endif
			}
			switch (o.clamp) {
			case 0: break;
			case 1:
#if SMU_X64ASM_MODE == 32
				// 飽和（acc=(eax,edx) は ±2^41 収まり。hi の値だけで判定できる）
				// P_MIN=-2^38 の hi=0xffffffc0。lo の tie（0）は hi==0xffffffc0 の時必ず acc>=P_MIN
				a.cmp32ri(RDX, 0xffffffc0u);
				a.imm32(RCX, 0u); a.imm32(RSI, 0xffffffc0u);
				a.cmovcc64(0x4c, RAX, RDX, RCX, RSI);          // cmovl → P_MIN
				// P_MAX=+2^38-1 の hi=0x3f。lo の tie（0xffffffff）は hi==0x3f の時必ず acc<=P_MAX
				a.cmp32ri(RDX, 0x3f);
				a.imm32(RCX, 0xffffffffu); a.imm32(RSI, 0x3f);
				a.cmovcc64(0x4f, RAX, RDX, RCX, RSI);          // cmovg → P_MAX
#else
				a.cmp64(RAX, P_MIN);
				a.cmovl64(RAX, P_MIN);
				a.cmp64(RAX, P_MAX);
				a.cmovg64(RAX, P_MAX);
#endif
				break;
			case 2:
#if SMU_X64ASM_MODE == 32
				a.xor32(RCX, RCX); a.xor32(RSI, RSI);
				a.test32(RDX, RDX);                             // 符号は hi の bit31
				a.cmovcc64(0x48, RAX, RDX, RCX, RSI);          // cmovs（負）→ 0
				a.cmp32ri(RDX, 0x3f);
				a.imm32(RCX, 0xffffffffu); a.imm32(RSI, 0x3f);
				a.cmovcc64(0x4f, RAX, RDX, RCX, RSI);          // cmovg → P_MAX
#else
				a.xor32(RCX, RCX);
				a.cmp64(RAX, RCX);
				a.cmovl64(RAX, RCX);
				a.cmp64(RAX, P_MAX);
				a.cmovg64(RAX, P_MAX);
#endif
				break;
			default:
#if SMU_X64ASM_MODE == 32
				a.mov32(RCX, RAX); a.mov32(RSI, RDX);
				a.neg64(RCX, RSI);                              // -acc
				a.cmovs64(RCX, RSI, RAX, RDX);                  // 正なら元のまま → |acc|
				a.mov32(RAX, RCX); a.mov32(RDX, RSI);
				a.cmp32ri(RDX, 0x3f);
				a.imm32(RCX, 0xffffffffu); a.imm32(RSI, 0x3f);
				a.cmovcc64(0x4f, RAX, RDX, RCX, RSI);          // cmovg → P_MAX
#else
				a.mov64(RDX, RAX);
				a.neg64(RDX);
				a.cmovs64(RDX, RAX);
				a.mov64(RAX, RDX);
				a.cmp64(RAX, P_MAX);
				a.cmovg64(RAX, P_MAX);
#endif
				break;
			}
#if SMU_X64ASM_MODE == 32
			a.store32(FM(F_PLO), RAX); a.store32(FM(F_PHI), RDX);   // p を置き場へ
			if (o.latch) {
				a.test32(RDX, RDX);                             // 符号（hi の bit31。test32 は of=0）
				a.setl_mem(mem{SWP, NOREG, 1, o_flag_n});
				a.test64(RAX, RDX, RAX, RDX);                   // ゼロ
				a.sete_mem(mem{SWP, NOREG, 1, o_flag_z});
			}
#else
			a.mov64(P, RAX);
			if (o.latch) {
				a.test64(P, P);
				a.setl_mem(mem{SWP, NOREG, 1, o_flag_n});
				a.test64(P, P);
				a.sete_mem(mem{SWP, NOREG, 1, o_flag_z});
			}
#endif
		}

		sz_alu += a.code.size() - sz_mark;
		sz_mark = a.code.size();

		// ---- dm ----
		if (o.dm) {
			switch (o.dm_src) {
			case 0: case 1: case 2: case 3:
				if (sintab && o.lfo < 0x18 && lfo_hoist[o.lfo]) {
					a.load32(RAX, FM(lfo_slot[o.lfo]));   // 頭で 1 回だけ作っておいた値
				} else if (sintab && o.lfo < 0x18) {
					emit_lfo(o.lfo);                      // 1 回しか使わない番号はその場で
				} else {
#if SMU_X64ASM_MODE == 32
					// cdecl: 引数を後ろから積む（lfo, ms）。p/seed は置き場にあって callee-saved を壊さない
					a.imm32(RDX, o.lfo);
					a.push(RDX);
					a.push(MS);
					a.call_abs(reinterpret_cast<void *>(&meg_jit::call_lfo));
					a.addrsp(8);
#else
					// sin 表が無いときは補助関数を呼ぶ。引数は規約に合わせて ARG0 = ms、ARG1 = LFO の番号。
					// 以前は Windows x64 の RCX/RDX に決め打ちで、SysV（macOS・Linux の x86-64）では
					// 引数が渡らず落ちていた（Rosetta で exit 139。PR #21 の注記）。
					// SysV では rsi・rdi は呼ぶ先が壊してよいので、そこに置いている SEED と K_MAX を
					// 積んで戻す（2 つで 16 バイトなので揃えは崩れない）。Windows x64 では呼ぶ先が守るうえ、
					// 呼ぶ先が影の 32 バイトに書くので積まない
					if constexpr (sysv_abi) {
						a.push(SEED);
						a.push(K_MAX);
					}
					a.mov64(ARG0, MS);
					a.imm32(ARG1, o.lfo);
					a.call_abs(reinterpret_cast<void *>(&meg_jit::call_lfo));
					if constexpr (sysv_abi) {
						a.pop(K_MAX);
						a.pop(SEED);
					}
					load_p_limits();
#endif
				}
				break;
			case 4:
				a.load32(RAX, M(o_ram_read));
				break;
			case 5:
				rnd();
				a.shl32(RAX, 8);
				a.sar32(RAX, 8);
				break;
			case 6:
				p_packed(!o.no_noise);
				break;
			default:
				a.load32(RAX, M(o_m + 4 * o.sm));
				break;
			}
			if (k < 0x17d && early_m[o.dm]) {
				a.store32(M(o_m + 4 * o.dm), RAX);
				if (last_slot_m[k])
					a.store32(M(o_mw_value + 4 * slot3(k)), RAX);
			} else
				a.store32(M(o_mw_value + 4 * slot3(k)), RAX);
		}
		if (k >= 0x17d || branchy)
			a.store8i(M(o_mw_reg + slot3(k)), o.dm);

		sz_dm += a.code.size() - sz_mark;
		sz_mark = a.code.size();

		// ---- dr ----
		if (o.dr) {
			if (o.dr_from_r)
				a.load32(RAX, M(o_r + 4 * o.sr));
			else
				p_packed(!o.no_noise);
			if (k < 0x17d && early_r[o.dr]) {
				a.store32(M(o_r + 4 * o.dr), RAX);
				if (last_slot_r[k])
					a.store32(M(o_rw_value + 4 * slot3(k)), RAX);
			} else
				a.store32(M(o_rw_value + 4 * slot3(k)), RAX);
		}
		if (k >= 0x17d || branchy)
			a.store8i(M(o_rw_reg + slot3(k)), o.dr);

		sz_dr += a.code.size() - sz_mark;
		sz_mark = a.code.size();

		// ---- メモリへの書き値 ----
		if (o.memw) {
			AccFromP();
			ShrAcc(15);
			a.store32(M(o_memw_val + 4 * slot2(k)), RAX);
		}
		if (k >= 0x17e || branchy)
			a.store8i(M(o_memw_act + slot2(k)), o.memw ? 1 : 0);

		// ---- index ----
		if (o.index) {
			AccFromP();
			ShrAcc(15 + 8);
			a.store32(M(o_ix_value + 4 * slot3(k)), RAX);
		}
		if (k >= 0x17d || branchy)
			a.store8i(M(o_ix_act + slot3(k)), o.index ? 1 : 0);
		if (o.index2) {
			AccFromP();
			ShrAcc(15 + 8);
			a.store32(mem{SWP, NOREG, 1, o_ix2_value + 4 * s32(slot3(k))}, RAX);
		}
		if (k >= 0x17d || branchy)
			a.store8i(mem{SWP, NOREG, 1, o_ix2_act + s32(slot3(k))}, o.index2 ? 1 : 0);

		sz_ix += a.code.size() - sz_mark;
		sz_mark = a.code.size();

		// ---- t ----
		if (o.t_write) {
			if (o.t_from_p)
				a.loadu16(RAX, M(o_t_value + 2 * slot2(k)));
			else if (bake)
				a.imm32(RAX, u16(ms.m_const[k]));
			else
				a.loadu16(RAX, M(o_const + 2 * s32(k)));
			a.store16(M(o_t + 2 * o.t), RAX);
		}
		if (need_tval[k]) {
			AccFromP();
			if (o.index || o.index2) {
				ShrAcc(8);
				a.and32i(RAX, 0x7fff);
			} else {
				ShrAcc(15 + 8);
#if SMU_X64ASM_MODE == 32
				// t は ±0x8000 に飽和。値は eax（edx は符号拡張）
				a.jlt64i(RAX, RDX, u32(s32(-0x8000)), [&] { a.imm32(RAX, u32(s32(-0x8000))); a.imm32(RDX, 0xffffffffu); });
				a.jgt64i(RAX, RDX, 0x7fff, [&] { a.imm32(RAX, 0x7fff); a.xor32(RDX, RDX); });
#else
				a.imm64(RCX, u64(s64(-0x8000)));
				a.cmp64(RAX, RCX);
				a.cmovl64(RAX, RCX);
				a.imm64(RCX, 0x7fff);
				a.cmp64(RAX, RCX);
				a.cmovg64(RAX, RCX);
#endif
			}
			a.store16(M(o_t_value + 2 * slot2(k)), RAX);
		}

		sz_t += a.code.size() - sz_mark;
		sz_mark = a.code.size();

		// ---- メモリ操作 ----
		size_t table_done = 0;
		if (o.memop >= 2 && o.mem_table) {
			// bit 0x23 の読み出しはリバーブ RAM の絶対番地（meg_state::step、doc/upstream.md の 24）
			a.loadu16(RAX, M(o_offset + 2 * s32(o.offset_index)));
			if (o.mem_use_index) {
				a.load32(RCX, M(o_ram_index));
				a.add32(RAX, RCX);
			}
			if (o.mem_use_index2) {
				a.load32(RCX, mem{SWP, NOREG, 1, o_ram_index2});
				a.add32(RAX, RCX);
			}
			if (o.memop == 3)
				a.add32i(RAX, 1);
			a.and32i(RAX, 0x3ffff);
			a.loadu16(RAX, mem{RAM, RAX, 2, 0});
			emit_revram_decode(a);
			a.store32(M(o_memr_val + 4 * slot2(k)), RAX);
			table_done = a.jmp_fwd();
		}
		// S-MU2000: 無効な区画（0x80e）への出し入れは、訳すときに省く。
		// 書き込みは落ち、読み出しは 0 になる。有効・無効が変われば訳し直す（meg_jit_run）
		if (o.memop && BIT(swp.m_revram_enable, o.region)) {
			if (o.memop != 1)
				a.store32i(M(o_memr_val + 4 * slot2(k)), 0);
		} else if (o.memop) {
			a.loadu16(RAX, M(o_offset + 2 * s32(o.offset_index)));
			if (o.mem_use_index) {
				a.load32(RCX, M(o_ram_index));
				a.add32(RAX, RCX);
			}
			if (o.mem_use_index2) {
				a.load32(RCX, mem{SWP, NOREG, 1, o_ram_index2});
				a.add32(RAX, RCX);
			}
#if SMU_X64ASM_MODE == 32
			a.load32(RCX, FM(F_SC));
			a.sub32(RAX, RCX);
#else
			a.sub32(RAX, SC);
#endif
			if (o.memop == 3)
				a.add32i(RAX, 1);
			a.and32i(RAX, o.addr_mask);
			a.add32i(RAX, o.addr_base);
			a.and32i(RAX, 0x3ffff);
			if (o.memop == 1) {
				// meg_state::revram_encode を機械語で（関数は呼ばない）。番地を取っておく
#if SMU_X64ASM_MODE == 32
				// encode は eax ecx edx esi を全部壊すので番地は置き場へ退避
				a.store32(FM(F_ALO), RAX);
				a.load32(RAX, M(o_ram_write));
				emit_revram_encode(a);
				a.load32(RCX, FM(F_ALO));
				a.store16(mem{RAM, RCX, 2, 0}, RAX);
#else
				a.mov64(R8, RAX);
				a.load32(RAX, M(o_ram_write));
				emit_revram_encode(a);
				a.store16(mem{RAM, R8, 2, 0}, RAX);
#endif
			} else {
				// meg_state::revram_decode を機械語で（関数は呼ばない）
				a.loadu16(RAX, mem{RAM, RAX, 2, 0});
				emit_revram_decode(a);
				a.store32(M(o_memr_val + 4 * slot2(k)), RAX);
			}
		}
		if (table_done)
			a.patch(table_done);
		if (k >= 0x17e || branchy)
			a.store8i(M(o_memr_act + slot2(k)), (o.memop == 2 || o.memop == 3) ? 1 : 0);
		}   // !(branchy && o.jump)

		if (branchy) {
			const size_t normal_done = o.jump ? 0 : a.jmp_fwd();
			a.patch(skip_jump);
			if (jump_done)
				a.patch(jump_done);
			// 飛ばされた命令（と分岐の命令）: 輪に入れる書き込みを消し、t の値を入れる
			if (o.jump && o.t_write) {
				// 分岐の命令も t を書く（meg_state::step と同じ、doc/upstream.md の 31）
				if (o.t_from_p)
					a.loadu16(RAX, M(o_t_value + 2 * slot2(k)));
				else
					a.loadu16(RAX, M(o_const + 2 * s32(k)));
				a.store16(M(o_t + 2 * o.t), RAX);
			}
			a.store8i(M(o_mw_reg + slot3(k)), 0);
			a.store8i(M(o_rw_reg + slot3(k)), 0);
			a.store8i(M(o_memw_act + slot2(k)), 0);
			a.store8i(M(o_ix_act + slot3(k)), 0);
			a.store8i(mem{SWP, NOREG, 1, o_ix2_act + s32(slot3(k))}, 0);
			if (need_tval[k]) {
				AccFromP();
				ShrAcc(15 + 8);
#if SMU_X64ASM_MODE == 32
				a.jlt64i(RAX, RDX, u32(s32(-0x8000)), [&] { a.imm32(RAX, u32(s32(-0x8000))); a.imm32(RDX, 0xffffffffu); });
				a.jgt64i(RAX, RDX, 0x7fff, [&] { a.imm32(RAX, 0x7fff); a.xor32(RDX, RDX); });
#else
				a.imm64(RCX, u64(s64(-0x8000)));
				a.cmp64(RAX, RCX);
				a.cmovl64(RAX, RCX);
				a.imm64(RCX, 0x7fff);
				a.cmp64(RAX, RCX);
				a.cmovg64(RAX, RCX);
#endif
				a.store16(M(o_t_value + 2 * slot2(k)), RAX);
			}
			if (normal_done)
				a.patch(normal_done);
		}
	}

	// 出口
#if SMU_X64ASM_MODE == 32
	a.load32(RAX, FM(F_PLO)); a.store32(M(o_p), RAX);
	a.load32(RAX, FM(F_PHI)); a.store32(M(o_p + 4), RAX);
	a.load32(RAX, FM(F_SEED)); a.store32(mem{SWP, NOREG, 1, o_seed}, RAX);
	a.addrsp(24);
	a.pop(RBP); a.pop(RDI); a.pop(RSI); a.pop(RBX);
#else
	a.store64(M(o_p), P);
	a.store32(mem{SWP, NOREG, 1, o_seed}, SEED);
	a.addrsp(FRAME);
	a.pop(RBP); a.pop(RDI); a.pop(RSI); a.pop(R15); a.pop(R14); a.pop(R13); a.pop(R12); a.pop(RBX);
#endif
	a.ret();

	if (a.code.size() > buf_size) {
		if (buf)
			exec_mem::free_mem(buf, buf_size);
		buf_size = (a.code.size() + 0xffff) & ~size_t(0xffff);
		buf = exec_mem::alloc_rw(buf_size);
		if (!buf) {
			buf_size = 0;
			return false;
		}
	}
	// make writable, copy, make executable again. Needed on rebuilds, which arrive still executable
	if (!exec_mem::make_writable(buf, buf_size))
		return false;
	std::memcpy(buf, a.code.data(), a.code.size());
	if (!exec_mem::make_executable(buf, buf_size))
		return false;
	fn = reinterpret_cast<fn_t>(buf);
	{
		static const bool stats = std::getenv("SMU2000_MEG_JIT_STATS") != nullptr;
		if (stats)
			std::fprintf(stderr,
			             "meg-jit: %zu バイト / ALU %u（省いた %u）/ p の鎖 %u / 詰め %u / ずらし %u"
			             " / 印 %u / m 書き %u / r 書き %u（前倒し %u）/ RAM 読み %u 書き %u / LFO %u / t 書き %u%s\n",
			             a.code.size(), st.alu, st.skipped, st.chain, st.clamp, st.shift, st.latch,
			             st.wr_m, st.wr_r, st.early, st.mem_r, st.mem_w, st.lfo, st.tw,
			             branchy ? " / 分岐あり" : "");
		if (stats)
			std::fprintf(stderr, "        バイトの内訳: 遅れの後始末 %zu / ALU %zu / dm %zu / dr %zu"
			             " / index %zu / t %zu / メモリ %zu\n",
			             sz_ring, sz_alu, sz_dm, sz_dr, sz_ix, sz_t,
			             a.code.size() - sz_ring - sz_alu - sz_dm - sz_dr - sz_ix - sz_t);
		if (stats)
			std::fprintf(stderr, "        掛け算 %u（2 の冪 %u）/ dm の出どころ %u %u %u %u %u %u %u %u\n",
			             st.mul, st.mul_pow2, st.dm_src[0], st.dm_src[1], st.dm_src[2], st.dm_src[3],
			             st.dm_src[4], st.dm_src[5], st.dm_src[6], st.dm_src[7]);
	}
#if SMU_X64ASM_MODE == 32
	// x86-32 の JIT が本当に効いている証明（環境変数で 1 回だけ）。ビルドは meg_jit_run からしか来ない
	static bool done32 = false;
	if (!done32 && std::getenv("SMU2000_MEG_JIT32_LOG")) {
		done32 = true;
		std::fprintf(stderr, "meg-jit32: x86-32 build() emitted %zu bytes\n", a.code.size());
	}
	if (const char *dp = std::getenv("SMU2000_MEG_JIT32_DUMP")) {   // ALU を含む大きめのプログラムを 1 本
		static int dumped = 0;
		if (dumped < 1 && a.code.size() > 4000) {
			dumped++;
			if (FILE *f = std::fopen(dp, "wb")) { std::fwrite(a.code.data(), 1, a.code.size(), f); std::fclose(f); }
		}
	}
	if (const char *ds = std::getenv("SMU2000_MEG_JIT32_DUMPSAMPLE")) {   // 分岐サンプル周辺の全プログラムを番号付きで
		static int n = 0;
		if (n < 40 && a.code.size() > 1500) {
			char nm[512]; std::snprintf(nm, sizeof nm, "%s_%02d_%ld_%zu.bin", ds, n, (long)ms.m_sample_counter, a.code.size());
			if (FILE *f = std::fopen(nm, "wb")) { std::fwrite(a.code.data(), 1, a.code.size(), f); std::fclose(f); }
			n++;
		}
	}
#endif
	return true;
}

#else // __aarch64__

// The arm64 MEG JIT. Same shape as the x86-64 version above (which carries the
// comments explaining what each section computes); only the notes where the
// two differ are repeated here.
bool swp30_device::meg_jit::build(code &cd, meg_state &ms, const meg_state::op *ops, swp30_device &swp, bool bake)
{
	using namespace a64;

	// The baked-constant variant of the x86-64 backend has no arm64 equivalent
	// yet, so asking for it here just fails and the caller keeps the general
	// version (see meg_jit_run)
	if (bake)
		return false;

	cd.fn = nullptr;
	// Programs that jump (LO-FI and DYNA types) are compiled too, by reading and
	// writing the delay ring on every op rather than committing three ops later,
	// exactly as the x86-64 backend above does. An instruction that was jumped
	// over clears the write it would have left in the ring and stores only its t
	// value (meg_state::run_program does the same)
	bool branchy = false;
	for (u32 pc = 0; pc != 0x180; pc++)
		if (ops[pc].jump)
			branchy = true;
	if (swp.m_reverb_ram.size() < 0x40000)
		return false;

	cd.d3 = ms.m_delay_3;
	cd.d2 = ms.m_delay_2;
	const auto slot3 = [&](u32 k) { return (cd.d3 + k) % 3; };
	const auto slot2 = [&](u32 k) { return (cd.d2 + k) % 2; };

	// element positions (inside meg_state and swp30_device)
	const auto off = [](const void *base, const void *field) { return s32(intptr_t(field) - intptr_t(base)); };
	const s32 o_m        = off(&ms, ms.m_m.data());
	const s32 o_r        = off(&ms, ms.m_r.data());
	const s32 o_t        = off(&ms, ms.m_t.data());
	const s32 o_p        = off(&ms, &ms.m_p);
	const s32 o_const    = off(&ms, ms.m_const.data());
	const s32 o_offset   = off(&ms, ms.m_offset.data());
	const s32 o_mw_value = off(&ms, ms.m_mw_value.data());
	const s32 o_mw_reg   = off(&ms, ms.m_mw_reg.data());
	const s32 o_rw_value = off(&ms, ms.m_rw_value.data());
	const s32 o_rw_reg   = off(&ms, ms.m_rw_reg.data());
	const s32 o_ix_value = off(&ms, ms.m_index_value.data());
	const s32 o_ix_act   = off(&ms, ms.m_index_active.data());
	const s32 o_memw_val = off(&ms, ms.m_memw_value.data());
	const s32 o_memr_val = off(&ms, ms.m_memr_value.data());
	const s32 o_t_value  = off(&ms, ms.m_t_value.data());
	const s32 o_memw_act = off(&ms, ms.m_memw_active.data());
	const s32 o_memr_act = off(&ms, ms.m_memr_active.data());
	const s32 o_ram_read = off(&ms, &ms.m_ram_read);
	const s32 o_ram_write = off(&ms, &ms.m_ram_write);
	const s32 o_ram_index = off(&ms, &ms.m_ram_index);
	const s32 o_sample   = off(&ms, &ms.m_sample_counter);
	const s32 o_lfo      = off(&ms, ms.m_lfo.data());
	const s32 o_lfo_counter = off(&ms, ms.m_lfo_counter.data());
	const s32 o_seed     = off(&swp, &swp.m_rand_seed);
	const s32 o_flag_n   = off(&swp, &swp.m_meg_flag_n);
	const s32 o_flag_z   = off(&swp, &swp.m_meg_flag_z);
	// The second idx (doc/upstream.md #32) lives on swp30_device rather than in
	// meg_state, so that the layout the saved state is packed from does not move
	const s32 o_ix2_value  = off(&swp, swp.m_meg_ix2_value.data());
	const s32 o_ix2_act    = off(&swp, swp.m_meg_ix2_act.data());
	const s32 o_ram_index2 = off(&swp, &swp.m_meg_ram_index2);
	const s32 o_skip     = off(&swp, &swp.m_meg_jit_skip);

	if (sizeof(ms.m_mw_reg[0]) != 1 || sizeof(ms.m_index_active[0]) != 1 || sizeof(ms.m_memw_active[0]) != 1 ||
	    sizeof(swp.m_meg_flag_n) != 1 || sizeof(ms.m_t_value[0]) != 2 || sizeof(ms.m_const[0]) != 2 ||
	    sizeof(ms.m_offset[0]) != 2 || sizeof(ms.m_m[0]) != 4 || sizeof(ms.m_r[0]) != 4)
		return false;
	// The LFO goes inline only with a full quarter-wave table (0x8000
	// entries), same rule as the x86-64 backend; otherwise call the helper.
	const u16 *sintab = swp.m_sintab.count() >= 0x8000 ? swp.m_sintab.target() : nullptr;

	// instructions that must leave a t value in the ring (2 later one is read), plus the tail
	bool need_tval[0x180] = {};
	for (u32 k = 0; k != 0x180; k++) {
		if (k >= 0x17e)
			need_tval[k] = true;
		if (k + 2 < 0x180 && ops[k + 2].t_write && ops[k + 2].t_from_p)
			need_tval[k] = true;
	}

	// A write delayed by three ops can be committed where it is written when
	// no reader in between can see the difference: no write to the same
	// register one or two ops ahead of any read (wrapping across samples),
	// and nothing written by the tail that the next sample takes from the
	// ring. Same analysis as the x86-64 backend (SMU2000_MEG_EARLY).
	bool early_r[128] = {}, early_m[128] = {};
	{
		bool bad_r[128] = {}, bad_m[128] = {};
		const auto reads = [&](const meg_state::op &o, bool m, u32 x) {
			if (!x)
				return false;
			bool rd = false;
			if (o.alu && (o.mmode == 2 || o.mmode == 3) && (o.m2_from_m != 0) == m && (m ? o.sm : o.sr) == x)
				rd = true;
			if (o.alu && !m && o.asel == 1 && o.sr == x)
				rd = true;
			if (o.alu && m && o.asel == 2 && o.sm == x)
				rd = true;
			if (!m && o.dr && o.dr_from_r && o.sr == x)
				rd = true;
			if (m && o.dm && o.dm_src == 7 && o.sm == x)
				rd = true;
			return rd;
		};
		for (u32 j = 0; j != 0x180; j++)
			for (u32 back = 1; back <= 2; back++) {
				const meg_state::op &w = ops[(j + 0x180 - back) % 0x180];
				if (w.dr && reads(ops[j], false, w.dr))
					bad_r[w.dr] = true;
				if (w.dm && reads(ops[j], true, w.dm))
					bad_m[w.dm] = true;
			}
		for (u32 k = 0x17d; k != 0x180; k++) {
			if (ops[k].dr) bad_r[ops[k].dr] = true;
			if (ops[k].dm) bad_m[ops[k].dm] = true;
		}
		static const bool early_on = [] {
			const char *e = std::getenv("SMU2000_MEG_EARLY");
			return !(e && e[0] == '0');
		}();
		for (u32 x = 1; x != 128; x++) {
			early_r[x] = early_on && !branchy && !bad_r[x];
			early_m[x] = early_on && !branchy && !bad_m[x];
		}
	}

	// Even a write committed early leaves its ring slot holding the last
	// value written to it, so a saved state reads back exactly what the
	// interpreter would leave there.
	bool last_slot_r[0x180] = {}, last_slot_m[0x180] = {};
	for (u32 c = 0; c != 3; c++) {
		for (int k = 0x17f; k >= 0; k--)
			if (u32(k) % 3 == c && ops[k].dr) { last_slot_r[k] = true; break; }
		for (int k = 0x17f; k >= 0; k--)
			if (u32(k) % 3 == c && ops[k].dm) { last_slot_m[k] = true; break; }
	}

	emitter a;
	// Block state in callee-saved registers (rbx/r12-r15 in the x86 version).
	// A carries the value a program instruction computes, E holds an address
	// across the revram helpers, T takes a materialized struct address; A, C, D,
	// E and T are all caller-saved and hold nothing across a helper call.
	// SEED keeps the rand() state across the block the way the x86 backend
	// keeps it in rsi: every dithered write would otherwise load and store it.
	// CB addresses the constant region (m_const/m_offset/m_lfo/m_t and the t
	// ring): those tables sit ~12KB into meg_state, past what the halfword
	// imm12 form reaches, so every access through MS pays an add_off first.
	// m_m/m_r and the delay rings stay within reach and keep using MS.
	// KLO/KMN/PMX hold the saturation limits the x86 backend keeps in
	// registers (its rdi/rbp/r9/r10): -0x800001 and -0x800000 for pack24 and
	// 0x3fffffffff for the clamp. Only three callee-saved registers are left,
	// so P_MIN (-0x4000000000, two instructions) stays materialized.
	const u8 MS = X19, SWP = X20, P = X21, SC = X22, RAM = X23, SEED = X24, CB = X25;
	const u8 KLO = X26, KMN = X27, PMX = X28;
	const u8 A = HA, C = HC, D = HD, E = HE, T = X6;

	// [base + disp] with disp known at build time. Each size reaches further
	// than the plain imm12 (the field is scaled by the access size), so a disp
	// past that has its address materialized in T first.
	const auto ldb   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 4095)  a.ldrb(rt, base, d);  else { a.add_off(T, base, d); a.ldrb(rt, T, 0); } };
	const auto stb   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 4095)  a.strb(rt, base, d);  else { a.add_off(T, base, d); a.strb(rt, T, 0); } };
	const auto ldh   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 8190)  a.ldrh(rt, base, d);  else { a.add_off(T, base, d); a.ldrh(rt, T, 0); } };
	const auto ldh_s = [&](u32 rt, u32 base, s32 d) {
		if (d >= 0 && d <= 8190)
			a.ldrsh(rt, base, d);
		else {
			a.add_off(T, base, d);
			a.ldrsh(rt, T, 0);
		}
		// LDRSH Wt sign-extends to 32 bits; the MEG ALU consumes a signed
		// 64-bit m1 value, so extend the result through the top half of Xt.
		a.sxtw64(rt, rt);
	};
	const auto sth   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 8190)  a.strh(rt, base, d);  else { a.add_off(T, base, d); a.strh(rt, T, 0); } };
	const auto ldw   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 16380) a.ldr_w(rt, base, d); else { a.add_off(T, base, d); a.ldr_w(rt, T, 0); } };
	const auto ldw_s = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 16380) a.ldrsw(rt, base, d); else { a.add_off(T, base, d); a.ldrsw(rt, T, 0); } };
	const auto stw   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 16380) a.str_w(rt, base, d); else { a.add_off(T, base, d); a.str_w(rt, T, 0); } };
	const auto ldx   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 32760) a.ldr_x(rt, base, d); else { a.add_off(T, base, d); a.ldr_x(rt, T, 0); } };
	const auto stx   = [&](u32 rt, u32 base, s32 d) { if (d >= 0 && d <= 32760) a.str_x(rt, base, d); else { a.add_off(T, base, d); a.str_x(rt, T, 0); } };
	// [CB + (d - o_const)]: the constant region, addressed from o_const. The
	// call sites keep passing absolute struct offsets; the else branch is the
	// old MS-based path and only guards against the struct layout drifting
	// past the imm12 reach unnoticed.
	const s32 o_cbias = o_const;
	const auto cdh   = [&](u32 rt, s32 d) { const s32 r = d - o_cbias; if (r >= 0 && r <= 8190)  a.ldrh(rt, CB, r);  else { a.add_off(T, MS, d); a.ldrh(rt, T, 0); } };
	const auto cdh_s = [&](u32 rt, s32 d) {
		const s32 r = d - o_cbias;
		if (r >= 0 && r <= 8190)
			a.ldrsh(rt, CB, r);
		else {
			a.add_off(T, MS, d);
			a.ldrsh(rt, T, 0);
		}
		a.sxtw64(rt, rt);
	};
	const auto csth   = [&](u32 rt, s32 d) { const s32 r = d - o_cbias; if (r >= 0 && r <= 8190)  a.strh(rt, CB, r);  else { a.add_off(T, MS, d); a.strh(rt, T, 0); } };

	// entry: x0 = ms, x1 = swp, x2 = the reverb RAM. x30 is saved because the
	// LFO is fetched with BLR, and x19-x25 because they carry the block state
	// (x29 is untouched by both the JIT and the helpers, so its save slot
	// goes to x25 instead).
	a.stp_x(X19, X20, X31, -16, true);
	a.stp_x(X21, X22, X31, -16, true);
	a.stp_x(X23, X24, X31, -16, true);
	a.stp_x(X25, X30, X31, -16, true);
	a.stp_x(X26, X27, X31, -16, true);
	a.stp_x(X28, X29, X31, -16, true);   // x29 is never touched; it only pads the pair
	a.mov_x(MS, X0);
	a.mov_x(SWP, X1);
	a.mov_x(RAM, X2);
	ldx(P, MS, o_p);
	ldw(SC, MS, o_sample);
	ldw(SEED, SWP, o_seed);
	a.add_off(CB, MS, u32(o_const));
	a.mov_imm64(KMN, u64(s64(-0x800000)));
	a.sub_imm64(KLO, KMN, 1);            // -0x800001, one past the low limit
	a.mov_imm64(PMX, 0x3fffffffff);
	if (branchy)
		stw(WZR, SWP, o_skip);

	// pack p into 24 bits (meg_state::meg_pack24). Input A, output A
	const auto pack24 = [&]() {
		// Same rounding as meg_pack24: toward zero, so a negative value has
		// 0x7fff added before the shift
		a.asr_imm_x(C, A, 63);
		a.and_imm(C, C, 0x7fff);
		a.add_x(A, A, C);
		a.asr_imm_x(A, A, 15);
		// One past either limit stops **at** the limit; everything else wraps
		// into 24 bits (the interpreter does the same, doc/upstream.md #23)
		a.mov_imm64(C, 0x800000);
		a.cmp_x(A, C);
		a.mov_imm64(D, 0x7fffff);
		a.csel_x(A, D, A, EQ);
		a.cmp_x(A, KLO);
		a.csel_x(A, KMN, A, EQ);
		a.lsl_imm(A, A, 8);
		a.sar_imm(A, A, 8);
	};
	// one step of swp30_device::rand. Output A; the state lives in SEED
	const auto rnd = [&]() {
		a.mov_reg(A, SEED);
		a.mov_imm32(T, 1664525);
		a.mul(A, A, T);
		a.mov_imm32(D, 1013904223);
		a.add_reg(A, A, D);
		a.mov_reg(SEED, A);
		a.ror_imm(A, A, 16);
	};
	// p plus noise, packed (dm 6, and dr's p). Output A
	const auto p_packed = [&](bool noise) {
		if (noise) {
			rnd();
			a.and_imm(A, A, 0x07e0);
			a.mov_x(D, A);
			a.mov_x(A, P);
			a.add_x(A, A, D);
		} else
			a.mov_x(A, P);
		pack24();
	};

	// DEBUG: stop the block early, to bisect against the interpreter. Read once
	// (see meg_jit_upto); ignored unless the check above is on
	const u32 upto = meg_jit_upto();
	for (u32 k = 0; k != upto; k++) {
		const meg_state::op &o = ops[k];

		// ---- delayed writes, applied ----
		if (k < 3 || branchy) {
			const u32 s = slot3(k);
			// m: the slot's register number is the index, and it may be 0 (= none)
			ldb(A, MS, o_mw_reg + s);
			a.cmp_reg(A, WZR);
			const size_t j1 = a.b_cond(EQ);
			ldw(C, MS, o_mw_value + 4 * s);
			a.add_off(T, MS, o_m);
			a.str_w_sr(C, T, A);
			a.patch(j1);
			// r
			ldb(A, MS, o_rw_reg + s);
			a.cmp_reg(A, WZR);
			const size_t j2 = a.b_cond(EQ);
			ldw(C, MS, o_rw_value + 4 * s);
			a.add_off(T, MS, o_r);
			a.str_w_sr(C, T, A);
			a.patch(j2);
			// index
			ldb(A, MS, o_ix_act + s);
			a.cmp_reg(A, WZR);
			const size_t j3 = a.b_cond(EQ);
			ldw(C, MS, o_ix_value + 4 * s);
			stw(C, MS, o_ram_index);
			a.patch(j3);
			// the second index, which gets its own carry (doc/upstream.md #32)
			ldb(A, SWP, o_ix2_act + s);
			a.cmp_reg(A, WZR);
			const size_t j4 = a.b_cond(EQ);
			ldw(C, SWP, o_ix2_value + 4 * s);
			stw(C, SWP, o_ram_index2);
			a.patch(j4);
		} else {
			const meg_state::op &w = ops[k - 3];
			const u32 s = slot3(k);
			if (w.dm && !early_m[w.dm]) {
				ldw(C, MS, o_mw_value + 4 * s);
				stw(C, MS, o_m + 4 * w.dm);
			}
			if (w.dr && !early_r[w.dr]) {
				ldw(C, MS, o_rw_value + 4 * s);
				stw(C, MS, o_r + 4 * w.dr);
			}
			if (w.index) {
				ldw(C, MS, o_ix_value + 4 * s);
				stw(C, MS, o_ram_index);
			}
			if (w.index2) {
				ldw(C, SWP, o_ix2_value + 4 * s);
				stw(C, SWP, o_ram_index2);
			}
		}
		if (k < 2 || branchy) {
			const u32 s = slot2(k);
			ldb(A, MS, o_memw_act + s);
			a.cmp_reg(A, WZR);
			const size_t j1 = a.b_cond(EQ);
			ldw(C, MS, o_memw_val + 4 * s);
			stw(C, MS, o_ram_write);
			stb(WZR, MS, o_memw_act + s);
			a.patch(j1);
			ldb(A, MS, o_memr_act + s);
			a.cmp_reg(A, WZR);
			const size_t j2 = a.b_cond(EQ);
			ldw(C, MS, o_memr_val + 4 * s);
			stw(C, MS, o_ram_read);
			stb(WZR, MS, o_memr_act + s);
			a.patch(j2);
		} else {
			const meg_state::op &w = ops[k - 2];
			const u32 s = slot2(k);
			if (w.memw) {
				ldw(C, MS, o_memw_val + 4 * s);
				stw(C, MS, o_ram_write);
			}
			if (w.memop == 2 || w.memop == 3) {
				ldw(C, MS, o_memr_val + 4 * s);
				stw(C, MS, o_ram_read);
			}
		}

		// ---- branch, and the instruction it jumped over ----
		size_t skip_jump = 0, jump_done = 0;
		if (branchy) {
			ldw(A, SWP, o_skip);
			a.mov_imm32(C, k);
			a.cmp_reg(A, C);
			skip_jump = a.b_cond(HI);            // the skip target is past this instruction
			if (o.jump) {
				// meg_cond in machine code: 1 when the condition holds. N is the
				// sign flag, Z the zero flag; bit 2 of cond chooses which, and
				// bit 1 ORs Z in
				size_t no_jump = 0;
				if (o.cond & 8) {
					ldb(A, SWP, o_flag_n);
					if (!(o.cond & 4))
						a.eor_imm(A, A, 1);
					if (o.cond & 2) {
						ldb(C, SWP, o_flag_z);
						a.orr_reg(A, A, C);
					}
					a.cmp_reg(A, WZR);
					no_jump = a.b_cond(EQ);
				}
				if (o.target > k) {
					a.mov_imm32(C, o.target);
					stw(C, SWP, o_skip);
				}
				if (no_jump)
					a.patch(no_jump);
				jump_done = a.b();               // the jump instruction does the same clean-up
			}
		}
		if (!(branchy && o.jump)) {

		// ---- ALU ----
		if (o.alu) {
			// the interpreter widens the s16 constant (or t) to s64, so the load
			// has to sign-extend all the way
			if (o.m1_from_t == 2) {
				// MULTI COMP follows its envelope: t when the last latch was
				// negative, the constant otherwise (meg_state::step, doc/upstream.md #29)
				cdh_s(A, o_t + 2 * o.t);
				cdh_s(C, o_const + 2 * s32(k));
				ldb(D, SWP, o_flag_n);
				a.cmp_reg(D, WZR);
				a.csel_x(A, A, C, NE);
			} else if (o.m1_from_t)
				cdh_s(A, o_t + 2 * o.t);
			else
				cdh_s(A, o_const + 2 * s32(k));
			if (o.m1_expand)
				emit_m1_expand(a);
			switch (o.mmode) {
			case 0:
				// mmode 0 (no multiplier) still goes through the shift and the
				// saturation below, with p as the only source: V DT HARD/SOFT
				// distort that way (doc/upstream.md #21)
				a.eor_reg(A, A, A);
				break;
			case 1:
				a.lsl_imm_x(A, A, 8 + 15);
				break;
			case 2:
				ldw_s(C, MS, o.m2_from_m ? o_m + 4 * o.sm : o_r + 4 * o.sr);
				a.mul_x(A, A, C);
				break;
			default:
				ldw_s(A, MS, o.m2_from_m ? o_m + 4 * o.sm : o_r + 4 * o.sr);
				a.lsl_imm_x(A, A, 15);
				break;
			}
			switch (o.asel) {
			case 0: a.mov_x(C, P); break;
			case 1: ldw_s(C, MS, o_r + 4 * o.sr); a.lsl_imm_x(C, C, 15); break;
			case 2: ldw_s(C, MS, o_m + 4 * o.sm); a.lsl_imm_x(C, C, 15); break;
			case 3: a.mov_x(C, P); a.asr_imm_x(C, C, 15); break;
			default: a.eor_reg(C, C, C); break;
			}
			switch (o.rop) {
			case 0: a.add_x(A, A, C); break;
			case 1: a.sub_x(A, A, C); break;
			case 2:
				// |a|: NEG does not set the flags here (unlike x86), so compare
				a.mov_x(D, C);
				a.neg_x(D, D);
				a.cmp_x(D, X31);
				a.csel_x(D, C, D, MI);
				a.add_x(A, A, D);
				break;
			default: a.and_x(A, A, C); break;
			}
			if (o.shift)
				a.lsl_imm_x(A, A, o.shift);
			// Only an unstaturated operation wraps at 42 bits: one that clamps
			// has to reach the limit first, or a strong distortion flips sign and
			// gains harmonics the machine does not have (doc/upstream.md #20)
			if (o.clamp == 0) {
				a.lsl_imm_x(A, A, 22);
				a.asr_imm_x(A, A, 22);
			}
			switch (o.clamp) {
			case 0: break;
			case 1:
				a.mov_imm64(C, u64(s64(-0x4000000000)));
				a.cmp_x(A, C);
				a.csel_x(A, C, A, LT);
				a.cmp_x(A, PMX);
				a.csel_x(A, PMX, A, GT);
				break;
			case 2:
				a.eor_reg(C, C, C);
				a.cmp_x(A, C);
				a.csel_x(A, C, A, LT);
				a.cmp_x(A, PMX);
				a.csel_x(A, PMX, A, GT);
				break;
			default:
				a.mov_x(D, A);
				a.neg_x(D, D);
				a.cmp_x(D, X31);
				a.csel_x(D, A, D, MI);
				a.mov_x(A, D);
				a.cmp_x(A, PMX);
				a.csel_x(A, PMX, A, GT);
				break;
			}
			a.mov_x(P, A);
			if (o.latch) {
				a.cmp_x(P, X31);
				a.cset(C, MI);
				stb(C, SWP, o_flag_n);
				a.cmp_x(P, X31);
				a.cset(C, EQ);
				stb(C, SWP, o_flag_z);
			}
		}

		// ---- dm ----
		if (o.dm) {
			switch (o.dm_src) {
			case 0: case 1: case 2: case 3:
				if (sintab && o.lfo < 0x18) {
					// meg_state::get_lfo in machine code, no call (same shape
					// as the x86-64 backend). Output A. C, D and E are scratch.
					static constexpr u32 lfo_offsets[16] = {
						0x00000, 0x02aaa, 0x04000, 0x05555, 0x08000, 0x0aaaa, 0x0c000, 0x0d555,
						0x10000, 0x12aaa, 0x14000, 0x15555, 0x18000, 0x1aaaa, 0x1c000, 0x1d555,
					};
					ldw(A, MS, o_lfo_counter + 4 * s32(o.lfo));
					a.lsr_imm(A, A, 5);
					cdh(D, o_lfo + 2 * s32(o.lfo));
					a.mov_reg(C, D);
					a.lsr_imm(C, C, 8);
					a.and_imm(C, C, 3);
					a.lslv(A, A, C);
					a.mov_reg(C, D);
					a.lsr_imm(C, C, 12);
					a.mov_imm64(E, u64(uintptr_t(lfo_offsets)));
					a.ldr_w_sr(C, E, C);
					a.add_reg(A, A, C);
					a.and_imm(A, A, 0x1ffff);
					a.lsr_imm(D, D, 10);
					a.and_imm(D, D, 3);
					std::vector<size_t> done;
					a.cmp_imm(D, 0);
					const size_t not_sine = a.b_cond(NE);
					{   // sine
						a.mov_reg(C, A);
						a.and_imm(C, C, 0x7fff);
						a.tst_imm(A, 0x8000);
						const size_t no_rev = a.b_cond(EQ);
						a.eor_imm(C, C, 0x7fff);
						a.patch(no_rev);
						a.mov_imm64(E, u64(uintptr_t(sintab)));
						a.mov_reg(D, A);
						a.ldrh_sr(A, E, C);
						a.tst_imm(D, 0x10000);
						const size_t no_neg = a.b_cond(EQ);
						a.eor_imm(A, A, 0xffff);
						a.patch(no_neg);
						done.push_back(a.b());
					}
					a.patch(not_sine);
					a.cmp_imm(D, 1);
					const size_t not_tri = a.b_cond(NE);
					{   // tri
						a.add_imm(A, A, 8, 1);               // +0x8000 via imm12 << 12
						a.and_imm(A, A, 0x1ffff);
						a.tst_imm(A, 0x10000);
						const size_t no_fold = a.b_cond(EQ);
						a.eor_imm(A, A, 0x1ffff);
						a.patch(no_fold);
						done.push_back(a.b());
					}
					a.patch(not_tri);
					a.cmp_imm(D, 2);
					const size_t not_up = a.b_cond(NE);
					a.lsr_imm(A, A, 1);                              // saw up
					done.push_back(a.b());
					a.patch(not_up);
					a.eor_imm(A, A, 0x1ffff);                        // saw down
					a.lsr_imm(A, A, 1);
					for (size_t d : done) a.patch(d);
					a.lsl_imm(A, A, 7);
				} else {
					a.mov_x(X0, MS);
					a.mov_imm32(X1, o.lfo);
					a.mov_imm64(X17, u64(uintptr_t(&meg_jit::call_lfo)));
					a.blr(X17);
					a.mov_reg(A, W0);                        // the result comes back in w0
				}
				break;
			case 4:
				ldw(A, MS, o_ram_read);
				break;
			case 5:
				rnd();
				a.lsl_imm(A, A, 8);
				a.sar_imm(A, A, 8);
				break;
			case 6:
				p_packed(!o.no_noise);
				break;
			default:
				ldw(A, MS, o_m + 4 * o.sm);
				break;
			}
			if (k < 0x17d && early_m[o.dm]) {
				stw(A, MS, o_m + 4 * o.dm);
				if (last_slot_m[k])
					stw(A, MS, o_mw_value + 4 * slot3(k));
			} else
				stw(A, MS, o_mw_value + 4 * slot3(k));
		}
		if (k >= 0x17d || branchy) {
			a.mov_imm32(C, o.dm);
			stb(C, MS, o_mw_reg + slot3(k));
		}

		// ---- dr ----
		if (o.dr) {
			if (o.dr_from_r)
				ldw(A, MS, o_r + 4 * o.sr);
			else
				p_packed(!o.no_noise);
			if (k < 0x17d && early_r[o.dr]) {
				stw(A, MS, o_r + 4 * o.dr);
				if (last_slot_r[k])
					stw(A, MS, o_rw_value + 4 * slot3(k));
			} else
				stw(A, MS, o_rw_value + 4 * slot3(k));
		}
		if (k >= 0x17d || branchy) {
			a.mov_imm32(C, o.dr);
			stb(C, MS, o_rw_reg + slot3(k));
		}

		// ---- value to write to memory ----
		if (o.memw) {
			a.mov_x(A, P);
			a.asr_imm_x(A, A, 15);
			stw(A, MS, o_memw_val + 4 * slot2(k));
		}
		if (k >= 0x17e || branchy) {
			a.mov_imm32(C, o.memw ? 1 : 0);
			stb(C, MS, o_memw_act + slot2(k));
		}

		// ---- index ----
		if (o.index) {
			a.mov_x(A, P);
			a.asr_imm_x(A, A, 15 + 8);
			stw(A, MS, o_ix_value + 4 * slot3(k));
		}
		// An instruction that sets both idx and a memory write carries the second
		// index too; a read marked with bit 0x22 adds it
		if (o.index2) {
			a.mov_x(A, P);
			a.asr_imm_x(A, A, 15 + 8);
			stw(A, SWP, o_ix2_value + 4 * slot3(k));
		}
		if (k >= 0x17d || branchy) {
			a.mov_imm32(C, o.index ? 1 : 0);
			stb(C, MS, o_ix_act + slot3(k));
			a.mov_imm32(C, o.index2 ? 1 : 0);
			stb(C, SWP, o_ix2_act + slot3(k));
		}

		// ---- t ----
		if (o.t_write) {
			if (o.t_from_p)
				cdh(A, o_t_value + 2 * slot2(k));
			else
				cdh(A, o_const + 2 * s32(k));
			csth(A, o_t + 2 * o.t);
		}
		if (need_tval[k]) {
			a.mov_x(A, P);
			if (o.index || o.index2) {
				a.asr_imm_x(A, A, 8);
				a.and_imm(A, A, 0x7fff);
			} else {
				a.asr_imm_x(A, A, 15 + 8);
				a.mov_imm64(C, u64(s64(-0x8000)));
				a.cmp_x(A, C);
				a.csel_x(A, C, A, LT);
				a.mov_imm64(C, 0x7fff);
				a.cmp_x(A, C);
				a.csel_x(A, C, A, GT);
			}
			csth(A, o_t_value + 2 * slot2(k));
		}

		// ---- memory operation ----
		size_t table_done = 0;
		if (o.memop >= 2 && o.mem_table) {
			// A read marked with bit 0x23 addresses the reverb RAM absolutely: the
			// firmware writes the wave or curve table straight into it, so the
			// sample counter must not be subtracted (meg_state::step, doc/upstream.md #24)
			cdh(A, o_offset + 2 * s32(o.offset_index));
			if (o.mem_use_index) {
				ldw(C, MS, o_ram_index);
				a.add_reg(A, A, C);
			}
			if (o.mem_use_index2) {
				ldw(C, SWP, o_ram_index2);
				a.add_reg(A, A, C);
			}
			if (o.memop == 3)
				a.add_imm(A, A, 1);
			a.and_imm_any(A, A, 0x3ffff);
			a.ldrh_sr(A, RAM, A);
			emit_revram_decode(a);
			stw(A, MS, o_memr_val + 4 * slot2(k));
			table_done = a.b();
		}
		// S-MU2000: 無効な区画（0x80e）への出し入れは、訳すときに省く（x86 側と同じ）
		if (o.memop && BIT(swp.m_revram_enable, o.region)) {
			if (o.memop != 1) {
				a.mov_imm32(A, 0);
				stw(A, MS, o_memr_val + 4 * slot2(k));
			}
		} else if (o.memop) {
			cdh(A, o_offset + 2 * s32(o.offset_index));
			if (o.mem_use_index) {
				ldw(C, MS, o_ram_index);
				a.add_reg(A, A, C);
			}
			if (o.mem_use_index2) {
				ldw(C, SWP, o_ram_index2);
				a.add_reg(A, A, C);
			}
			a.sub_reg(A, A, SC);
			if (o.memop == 3)
				a.add_imm(A, A, 1);
			a.and_imm_any(A, A, o.addr_mask);
			a.add_imm_any(A, A, o.addr_base);
			a.and_imm(A, A, 0x3ffff);
			if (o.memop == 1) {
				// meg_state::revram_encode written out in machine code, with the
				// address parked in E (the helper clobbers C and D)
				a.mov_x(E, A);
				ldw(A, MS, o_ram_write);
				emit_revram_encode(a);
				a.strh_sr(A, RAM, E);
			} else {
				// meg_state::revram_decode written out in machine code
				a.ldrh_sr(A, RAM, A);
				emit_revram_decode(a);
				stw(A, MS, o_memr_val + 4 * slot2(k));
			}
		}
		if (table_done)
			a.patch(table_done);
		if (k >= 0x17e || branchy) {
			a.mov_imm32(C, (o.memop == 2 || o.memop == 3) ? 1 : 0);
			stb(C, MS, o_memr_act + slot2(k));
		}
		}   // !(branchy && o.jump)

		if (branchy) {
			const size_t normal_done = o.jump ? 0 : a.b();
			a.patch(skip_jump);
			if (jump_done)
				a.patch(jump_done);
			// the instruction that was jumped over (and the jump itself): drop the
			// write it would have left in the ring and store only its t value
			if (o.jump && o.t_write) {
				if (o.t_from_p)
					cdh(A, o_t_value + 2 * slot2(k));
				else
					cdh(A, o_const + 2 * s32(k));
				csth(A, o_t + 2 * o.t);
			}
			stb(WZR, MS, o_mw_reg + slot3(k));
			stb(WZR, MS, o_rw_reg + slot3(k));
			stb(WZR, MS, o_memw_act + slot2(k));
			stb(WZR, MS, o_ix_act + slot3(k));
			stb(WZR, SWP, o_ix2_act + slot3(k));
			if (need_tval[k]) {
				a.mov_x(A, P);
				a.asr_imm_x(A, A, 15 + 8);
				a.mov_imm64(C, u64(s64(-0x8000)));
				a.cmp_x(A, C);
				a.csel_x(A, C, A, LT);
				a.mov_imm64(C, 0x7fff);
				a.cmp_x(A, C);
				a.csel_x(A, C, A, GT);
				csth(A, o_t_value + 2 * slot2(k));
			}
			if (normal_done)
				a.patch(normal_done);
		}
	}

	// exit
	stx(P, MS, o_p);
	stw(SEED, SWP, o_seed);
	a.ldp_x(X28, X29, X31, 16, true);
	a.ldp_x(X26, X27, X31, 16, true);
	a.ldp_x(X25, X30, X31, 16, true);
	a.ldp_x(X23, X24, X31, 16, true);
	a.ldp_x(X21, X22, X31, 16, true);
	a.ldp_x(X19, X20, X31, 16, true);
	a.ret();

	// a.code is a vector of 32-bit instructions, so every size here has to
	// count four bytes each (the x86 version's vector is bytes)
	const size_t bytes = a.code.size() * sizeof(a.code[0]);
	if (bytes > cd.buf_size) {
		if (cd.buf)
			exec_mem::free_mem(cd.buf, cd.buf_size);
		cd.buf_size = (bytes + 0xffff) & ~size_t(0xffff);
		cd.buf = exec_mem::alloc_rw(cd.buf_size);
		if (!cd.buf) {
			cd.buf_size = 0;
			return false;
		}
	}
	// make writable, copy, make executable again. Needed on rebuilds, which arrive still executable
	if (!exec_mem::make_writable(cd.buf, cd.buf_size))
		return false;
	std::memcpy(cd.buf, a.code.data(), bytes);
	if (!exec_mem::make_executable(cd.buf, cd.buf_size))
		return false;
	cd.fn = reinterpret_cast<fn_t>(cd.buf);
	return true;
}

#endif
