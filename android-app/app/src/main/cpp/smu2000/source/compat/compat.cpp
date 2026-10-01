// license:BSD-3-Clause
//
// 互換層の実体。ログと、移植の突き合わせ用の命令追跡。

#include "mamecompat.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace smu2000 {

bool g_verbose = false;

// 命令の追跡。MAME の debugger_instruction_hook に相当する。
// MAME 側にも同じものを入れてあるので、突き合わせて最初に食い違う命令を探せる。
//
//   g_pc_hash  ブロック（65536 命令）ごとに畳んだ値。どこで食い違うかを安く探す
//   g_pc_trace 生の PC 列。g_pc_skip で頭を飛ばし、g_pc_trace_left 命令ぶん出す
std::FILE *g_pc_trace = nullptr;
u64        g_pc_trace_left = 0;
u64        g_pc_skip = 0;
std::FILE *g_pc_hash = nullptr;
std::FILE *g_port_trace = nullptr;
u64        g_pc_cycles = 0;   // 追跡に添えるサイクル数
std::FILE *g_upd_trace = nullptr;

static u64 s_count = 0, s_h = 0;

void pc_hash(u32 pc, u64 regs)
{
	s_h = (s_h * 1000003 ^ pc) * 1000003 ^ regs;
	if (!(++s_count & 0xffff))
		std::fprintf(g_pc_hash, "%llu %016llx\n",
		             (unsigned long long)s_count, (unsigned long long)s_h);
}

// ---- どの番地で回っているかの数え上げ
//
// SMU2000_PCPROF に「出す件数」を入れると入る。JIT のブロックに入るたびに
// 1 つ数えるだけなので、速さへの影響はほぼ無い。詰まっている輪を探すためのもの

u32 *g_pc_prof = nullptr;
u64 g_pc_prof_why[5] = {};
u64 g_slow_mem[2] = {};
static std::vector<u32> s_pc_prof;
static int s_pc_prof_top = 0;

void pc_prof_start()
{
	const char *e = std::getenv("SMU2000_PCPROF");
	if (!e || !*e)
		return;
	s_pc_prof_top = std::atoi(e);
	if (s_pc_prof_top <= 0)
		s_pc_prof_top = 30;
	s_pc_prof.assign(0x400000 / 0x40, 0);
	g_pc_prof = s_pc_prof.data();
}

void pc_prof_report()
{
	if (!g_pc_prof)
		return;
	std::vector<u32> idx;
	u64 total = 0;
	for (u32 i = 0; i < s_pc_prof.size(); i++)
		if (s_pc_prof[i]) {
			idx.push_back(i);
			total += s_pc_prof[i];
		}
	std::sort(idx.begin(), idx.end(),
	          [](u32 a, u32 b) { return s_pc_prof[a] > s_pc_prof[b]; });
	std::printf("ブロックに入った回数 合計 %llu（上位 %d）\n",
	            (unsigned long long)total, s_pc_prof_top);
	std::printf("  JIT を抜けた訳: 遅延枠 %llu / 割り込みの印 %llu / 番地が外 %llu\n",
	            (unsigned long long)g_pc_prof_why[0], (unsigned long long)g_pc_prof_why[1],
	            (unsigned long long)g_pc_prof_why[2]);
	std::printf("  入り直し %llu 回で %llu 命令 = 1 回あたり %.1f 命令\n",
	            (unsigned long long)g_pc_prof_why[3], (unsigned long long)g_pc_prof_why[4],
	            g_pc_prof_why[3] ? double(g_pc_prof_why[4]) / double(g_pc_prof_why[3]) : 0.0);
	std::printf("  JIT の速い道から外れたメモリ: 読み %llu / 書き %llu\n",
	            (unsigned long long)g_slow_mem[0], (unsigned long long)g_slow_mem[1]);
	for (int k = 0; k < s_pc_prof_top && k < int(idx.size()); k++)
		std::printf("  %08x  %10u  %5.1f%%\n", idx[k] * 0x40,
		            s_pc_prof[idx[k]], 100.0 * s_pc_prof[idx[k]] / double(total));
}

void pc_trace(u32 pc, const char *regs)
{
	if (g_pc_skip) {
		g_pc_skip--;
		return;
	}
	if (!g_pc_trace_left) {
		g_pc_trace = nullptr;
		return;
	}
	g_pc_trace_left--;
	std::fprintf(g_pc_trace, "%08X C=%llu%s\n", pc, (unsigned long long)g_pc_cycles, regs);
}

} // namespace smu2000
