// license:BSD-3-Clause

#include "xg/model.h"

#include <algorithm>
#include <cstdio>

namespace xg {

namespace {

constexpr const char *const MONO_POLY[]  = { "MONO", "POLY" };
constexpr const char *const KEY_ASSIGN[] = { "SINGLE", "MULTI" };
constexpr const char *const PART_MODE[]  = { "NORMAL", "DRUM", "DRUMS1", "DRUMS2", "DRUMS3", "DRUMS4" };
constexpr const char *const CONNECT[]    = { "INSERTION", "SYSTEM" };
constexpr const char *const EQ_TYPE[]    = { "FLAT", "JAZZ", "POPS", "ROCK", "CONCERT" };
constexpr const char *const EQ_SHAPE[]   = { "SHELF", "PEAK" };
constexpr const char *const OFF_ON[]     = { "OFF", "ON" };

// 番地は XG の決まりから。**マルチパートの 41 バイトは、firmware が返した一括ダンプと
// 1 バイトずつ突き合わせて並びを確かめた**（doc/params.md）。範囲と読み返しは
// xgtest.exe が firmware に確かめる。def は資料の値で、確かめていない。
//
// firmware に確かめて分かった決まり:
//   バンク       SysEx で書いても、次にプログラムを書くまで効かない（CC0/32 → PC と同じ）
//   エレメント   全パートの合計に枠がある。既定の 2 × 32 パートの上に 32 は載らない
//   キーアサイン 0-1 まで。資料にある 2（INST）は受け付けない
//
//   key                    label          area       hi    mid   lo    n  coding          min   max   sp   def   view            ctr   choices
const std::vector<param> TABLE = {
	{ "system.master_tune",   "Master Tune", area::system, 0x00, 0x00, 0x00, 4, coding::nibble, 0x000, 0x7ff, -1, 0x400, view::center, 0x400, nullptr },
	{ "system.master_volume", "Master Vol",  area::system, 0x00, 0x00, 0x04, 1, coding::byte7,  0,     127,   -1, 127,   view::raw,    0,     nullptr },
	{ "system.transpose",     "Transpose",   area::system, 0x00, 0x00, 0x06, 1, coding::byte7,  0x28,  0x58,  -1, 0x40,  view::center, 0x40,  nullptr },

	// ここの番地は doc/effects.md で音を測って確かめてある
	{ "reverb.type",          "Rev Type",    area::effect, 0x02, 0x01, 0x00, 2, coding::byte7,  0,     0x3fff, -1, 0x80,  view::raw,    0,     nullptr },
	{ "reverb.return",        "Rev Return",  area::effect, 0x02, 0x01, 0x0c, 1, coding::byte7,  0,     127,   -1, 64,    view::raw,    0,     nullptr },
	{ "reverb.pan",           "Rev Pan",     area::effect, 0x02, 0x01, 0x0d, 1, coding::byte7,  1,     127,   -1, 64,    view::pan,    0,     nullptr },
	{ "chorus.type",          "Cho Type",    area::effect, 0x02, 0x01, 0x20, 2, coding::byte7,  0,     0x3fff, -1, 0x2080, view::raw,   0,     nullptr },
	{ "chorus.return",        "Cho Return",  area::effect, 0x02, 0x01, 0x2c, 1, coding::byte7,  0,     127,   -1, 64,    view::raw,    0,     nullptr },
	{ "chorus.pan",           "Cho Pan",     area::effect, 0x02, 0x01, 0x2d, 1, coding::byte7,  1,     127,   -1, 64,    view::pan,    0,     nullptr },
	{ "chorus.to_reverb",     "Cho>Rev",     area::effect, 0x02, 0x01, 0x2e, 1, coding::byte7,  0,     127,   -1, 0,     view::raw,    0,     nullptr },
	{ "variation.type",       "Var Type",    area::effect, 0x02, 0x01, 0x40, 2, coding::byte7,  0,     0x3fff, -1, 0x280, view::raw,    0,     nullptr },
	{ "variation.return",     "Var Return",  area::effect, 0x02, 0x01, 0x56, 1, coding::byte7,  0,     127,   -1, 64,    view::raw,    0,     nullptr },
	{ "variation.pan",        "Var Pan",     area::effect, 0x02, 0x01, 0x57, 1, coding::byte7,  1,     127,   -1, 64,    view::pan,    0,     nullptr },
	{ "variation.to_reverb",  "Var>Rev",     area::effect, 0x02, 0x01, 0x58, 1, coding::byte7,  0,     127,   -1, 0,     view::raw,    0,     nullptr },
	{ "variation.to_chorus",  "Var>Cho",     area::effect, 0x02, 0x01, 0x59, 1, coding::byte7,  0,     127,   -1, 0,     view::raw,    0,     nullptr },
	{ "variation.connect",    "Var Connect", area::effect, 0x02, 0x01, 0x5a, 1, coding::byte7,  0,     1,     -1, 0,     view::choice, 0,     CONNECT },
	{ "variation.part",       "Var Part",    area::effect, 0x02, 0x01, 0x5b, 1, coding::byte7,  0,     65,   127, 127,   view::part_off, 0,   nullptr },
	{ "insertion1.type",      "Ins1 Type",   area::effect, 0x03, 0x00, 0x00, 2, coding::byte7,  0,     0x3fff, -1, 0,     view::raw,    0,     nullptr },
	{ "insertion1.part",      "Ins1 Part",   area::effect, 0x03, 0x00, 0x0c, 1, coding::byte7,  0,     65,   127, 127,   view::part_off, 0,   nullptr },
	{ "insertion2.type",      "Ins2 Type",   area::effect, 0x03, 0x01, 0x00, 2, coding::byte7,  0,     0x3fff, -1, 0,     view::raw,    0,     nullptr },
	{ "insertion2.part",      "Ins2 Part",   area::effect, 0x03, 0x01, 0x0c, 1, coding::byte7,  0,     65,   127, 127,   view::part_off, 0,   nullptr },
	{ "insertion3.type",      "Ins3 Type",   area::effect, 0x03, 0x02, 0x00, 2, coding::byte7,  0,     0x3fff, -1, 0,     view::raw,    0,     nullptr },
	{ "insertion3.part",      "Ins3 Part",   area::effect, 0x03, 0x02, 0x0c, 1, coding::byte7,  0,     65,   127, 127,   view::part_off, 0,   nullptr },
	{ "insertion4.type",      "Ins4 Type",   area::effect, 0x03, 0x03, 0x00, 2, coding::byte7,  0,     0x3fff, -1, 0,     view::raw,    0,     nullptr },
	{ "insertion4.part",      "Ins4 Part",   area::effect, 0x03, 0x03, 0x0c, 1, coding::byte7,  0,     65,   127, 127,   view::part_off, 0,   nullptr },

	// マスター EQ（02 40 00-14）。範囲は firmware に 0 と 127 を書いて読み返した値。
	// 初期値は MU2000 を起動した直後の値
	{ "master_eq.type",       "EQ Type",     area::effect, 0x02, 0x40, 0x00, 1, coding::byte7,  0,    4,    -1, 0,    view::choice, 0,    EQ_TYPE },
	{ "master_eq.gain1",      "EQ Gain1",    area::effect, 0x02, 0x40, 0x01, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "master_eq.freq1",      "EQ Freq1",    area::effect, 0x02, 0x40, 0x02, 1, coding::byte7,  4,    40,   -1, 12,   view::raw,    0,    nullptr },
	{ "master_eq.q1",         "EQ Q1",       area::effect, 0x02, 0x40, 0x03, 1, coding::byte7,  1,    120,  -1, 7,    view::raw,    0,    nullptr },
	{ "master_eq.shape1",     "EQ Shape1",   area::effect, 0x02, 0x40, 0x04, 1, coding::byte7,  0,    1,    -1, 0,    view::choice, 0,    EQ_SHAPE },
	{ "master_eq.gain2",      "EQ Gain2",    area::effect, 0x02, 0x40, 0x05, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "master_eq.freq2",      "EQ Freq2",    area::effect, 0x02, 0x40, 0x06, 1, coding::byte7,  14,   54,   -1, 28,   view::raw,    0,    nullptr },
	{ "master_eq.q2",         "EQ Q2",       area::effect, 0x02, 0x40, 0x07, 1, coding::byte7,  1,    120,  -1, 7,    view::raw,    0,    nullptr },
	{ "master_eq.gain3",      "EQ Gain3",    area::effect, 0x02, 0x40, 0x09, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "master_eq.freq3",      "EQ Freq3",    area::effect, 0x02, 0x40, 0x0a, 1, coding::byte7,  14,   54,   -1, 34,   view::raw,    0,    nullptr },
	{ "master_eq.q3",         "EQ Q3",       area::effect, 0x02, 0x40, 0x0b, 1, coding::byte7,  1,    120,  -1, 7,    view::raw,    0,    nullptr },
	{ "master_eq.gain4",      "EQ Gain4",    area::effect, 0x02, 0x40, 0x0d, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "master_eq.freq4",      "EQ Freq4",    area::effect, 0x02, 0x40, 0x0e, 1, coding::byte7,  14,   54,   -1, 46,   view::raw,    0,    nullptr },
	{ "master_eq.q4",         "EQ Q4",       area::effect, 0x02, 0x40, 0x0f, 1, coding::byte7,  1,    120,  -1, 7,    view::raw,    0,    nullptr },
	{ "master_eq.gain5",      "EQ Gain5",    area::effect, 0x02, 0x40, 0x11, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "master_eq.freq5",      "EQ Freq5",    area::effect, 0x02, 0x40, 0x12, 1, coding::byte7,  28,   58,   -1, 52,   view::raw,    0,    nullptr },
	{ "master_eq.q5",         "EQ Q5",       area::effect, 0x02, 0x40, 0x13, 1, coding::byte7,  1,    120,  -1, 7,    view::raw,    0,    nullptr },
	{ "master_eq.shape5",     "EQ Shape5",   area::effect, 0x02, 0x40, 0x14, 1, coding::byte7,  0,    1,    -1, 0,    view::choice, 0,    EQ_SHAPE },

	// マルチパート（08 pp 00-28、41 バイト）
	{ "part.element_reserve", "Elem Rsv",    area::part, 0x08, 0, 0x00, 1, coding::byte7,  0,    32,   -1, 2,    view::raw,    0,    nullptr },
	{ "part.bank_msb",        "Bank MSB",    area::part, 0x08, 0, 0x01, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.bank_lsb",        "Bank LSB",    area::part, 0x08, 0, 0x02, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.program",         "Program",     area::part, 0x08, 0, 0x03, 1, coding::byte7,  0,    127,  -1, 0,    view::plus1,  0,    nullptr },
	{ "part.rcv_channel",     "Rcv Ch",      area::part, 0x08, 0, 0x04, 1, coding::byte7,  0,    63,  127, 0,    view::raw,    0,    nullptr },
	{ "part.mono_poly",       "Mono/Poly",   area::part, 0x08, 0, 0x05, 1, coding::byte7,  0,    1,    -1, 1,    view::choice, 0,    MONO_POLY },
	{ "part.key_assign",      "Key Assign",  area::part, 0x08, 0, 0x06, 1, coding::byte7,  0,    1,    -1, 1,    view::choice, 0,    KEY_ASSIGN },
	{ "part.mode",            "Part Mode",   area::part, 0x08, 0, 0x07, 1, coding::byte7,  0,    5,    -1, 0,    view::choice, 0,    PART_MODE },
	{ "part.note_shift",      "Note Shift",  area::part, 0x08, 0, 0x08, 1, coding::byte7,  0x28, 0x58, -1, 0x40, view::center, 0x40, nullptr },
	{ "part.detune",          "Detune",      area::part, 0x08, 0, 0x09, 2, coding::nibble, 0x00, 0xff, -1, 0x80, view::center, 0x80, nullptr },
	{ "part.volume",          "Volume",      area::part, 0x08, 0, 0x0b, 1, coding::byte7,  0,    127,  -1, 100,  view::raw,    0,    nullptr },
	{ "part.vel_depth",       "Vel Depth",   area::part, 0x08, 0, 0x0c, 1, coding::byte7,  0,    127,  -1, 64,   view::raw,    0,    nullptr },
	{ "part.vel_offset",      "Vel Offset",  area::part, 0x08, 0, 0x0d, 1, coding::byte7,  0,    127,  -1, 64,   view::raw,    0,    nullptr },
	{ "part.pan",             "Pan",         area::part, 0x08, 0, 0x0e, 1, coding::byte7,  0,    127,  -1, 64,   view::pan,    0,    nullptr },
	{ "part.note_low",        "Note Low",    area::part, 0x08, 0, 0x0f, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.note_high",       "Note High",   area::part, 0x08, 0, 0x10, 1, coding::byte7,  0,    127,  -1, 127,  view::raw,    0,    nullptr },
	{ "part.dry_level",       "Dry Level",   area::part, 0x08, 0, 0x11, 1, coding::byte7,  0,    127,  -1, 127,  view::raw,    0,    nullptr },
	{ "part.chorus_send",     "Cho Send",    area::part, 0x08, 0, 0x12, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.reverb_send",     "Rev Send",    area::part, 0x08, 0, 0x13, 1, coding::byte7,  0,    127,  -1, 40,   view::raw,    0,    nullptr },
	{ "part.variation_send",  "Var Send",    area::part, 0x08, 0, 0x14, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.vib_rate",        "Vib Rate",    area::part, 0x08, 0, 0x15, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.vib_depth",       "Vib Depth",   area::part, 0x08, 0, 0x16, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.vib_delay",       "Vib Delay",   area::part, 0x08, 0, 0x17, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.cutoff",          "Cutoff",      area::part, 0x08, 0, 0x18, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.resonance",       "Resonance",   area::part, 0x08, 0, 0x19, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.attack",          "Attack",      area::part, 0x08, 0, 0x1a, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.decay",           "Decay",       area::part, 0x08, 0, 0x1b, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.release",         "Release",     area::part, 0x08, 0, 0x1c, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.mw_pitch",        "MW Pitch",    area::part, 0x08, 0, 0x1d, 1, coding::byte7,  0x28, 0x58, -1, 0x40, view::center, 0x40, nullptr },
	{ "part.mw_filter",       "MW Filter",   area::part, 0x08, 0, 0x1e, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.mw_amp",          "MW Amp",      area::part, 0x08, 0, 0x1f, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.mw_lfo_pmod",     "MW LFO PM",   area::part, 0x08, 0, 0x20, 1, coding::byte7,  0,    127,  -1, 10,   view::raw,    0,    nullptr },
	{ "part.mw_lfo_fmod",     "MW LFO FM",   area::part, 0x08, 0, 0x21, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.mw_lfo_amod",     "MW LFO AM",   area::part, 0x08, 0, 0x22, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.bend_pitch",      "PB Pitch",    area::part, 0x08, 0, 0x23, 1, coding::byte7,  0x28, 0x58, -1, 0x42, view::center, 0x40, nullptr },
	{ "part.bend_filter",     "PB Filter",   area::part, 0x08, 0, 0x24, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.bend_amp",        "PB Amp",      area::part, 0x08, 0, 0x25, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.bend_lfo_pmod",   "PB LFO PM",   area::part, 0x08, 0, 0x26, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.bend_lfo_fmod",   "PB LFO FM",   area::part, 0x08, 0, 0x27, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.bend_lfo_amod",   "PB LFO AM",   area::part, 0x08, 0, 0x28, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	// アフタータッチ（チャンネル 4D-52・ポリ 53-58）と AC1（59-5F）・AC2（60-66）の効き方。
	// 音程は ±24 半音（28-58）、ほかは 0-127。firmware が範囲の外を切り詰めるのを確かめた。
	// MU2000 の液晶のメニュー（Part Edit）に出るのは AC1 の CC 番号・フィルタ・音量だけだが、
	// 番地はどれも問い合わせに答え、書いた値が読み返せる
	{ "part.cat_pitch",       "CAT Pitch",   area::part, 0x08, 0, 0x4d, 1, coding::byte7,  0x28, 0x58, -1, 0x40, view::center, 0x40, nullptr },
	{ "part.cat_filter",      "CAT Filter",  area::part, 0x08, 0, 0x4e, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.cat_amp",         "CAT Amp",     area::part, 0x08, 0, 0x4f, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.cat_lfo_pmod",    "CAT LFO PM",  area::part, 0x08, 0, 0x50, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.cat_lfo_fmod",    "CAT LFO FM",  area::part, 0x08, 0, 0x51, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.cat_lfo_amod",    "CAT LFO AM",  area::part, 0x08, 0, 0x52, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.pat_pitch",       "PAT Pitch",   area::part, 0x08, 0, 0x53, 1, coding::byte7,  0x28, 0x58, -1, 0x40, view::center, 0x40, nullptr },
	{ "part.pat_filter",      "PAT Filter",  area::part, 0x08, 0, 0x54, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.pat_amp",         "PAT Amp",     area::part, 0x08, 0, 0x55, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.pat_lfo_pmod",    "PAT LFO PM",  area::part, 0x08, 0, 0x56, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.pat_lfo_fmod",    "PAT LFO FM",  area::part, 0x08, 0, 0x57, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.pat_lfo_amod",    "PAT LFO AM",  area::part, 0x08, 0, 0x58, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.ac1_cc",          "AC1 CC No",   area::part, 0x08, 0, 0x59, 1, coding::byte7,  0,    95,   -1, 16,   view::raw,    0,    nullptr },
	{ "part.ac1_pitch",       "AC1 Pitch",   area::part, 0x08, 0, 0x5a, 1, coding::byte7,  0x28, 0x58, -1, 0x40, view::center, 0x40, nullptr },
	{ "part.ac1_filter",      "AC1 Filter",  area::part, 0x08, 0, 0x5b, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.ac1_amp",         "AC1 Amp",     area::part, 0x08, 0, 0x5c, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.ac1_lfo_pmod",    "AC1 LFO PM",  area::part, 0x08, 0, 0x5d, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.ac1_lfo_fmod",    "AC1 LFO FM",  area::part, 0x08, 0, 0x5e, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.ac1_lfo_amod",    "AC1 LFO AM",  area::part, 0x08, 0, 0x5f, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.ac2_cc",          "AC2 CC No",   area::part, 0x08, 0, 0x60, 1, coding::byte7,  0,    95,   -1, 17,   view::raw,    0,    nullptr },
	{ "part.ac2_pitch",       "AC2 Pitch",   area::part, 0x08, 0, 0x61, 1, coding::byte7,  0x28, 0x58, -1, 0x40, view::center, 0x40, nullptr },
	{ "part.ac2_filter",      "AC2 Filter",  area::part, 0x08, 0, 0x62, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.ac2_amp",         "AC2 Amp",     area::part, 0x08, 0, 0x63, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.ac2_lfo_pmod",    "AC2 LFO PM",  area::part, 0x08, 0, 0x64, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.ac2_lfo_fmod",    "AC2 LFO FM",  area::part, 0x08, 0, 0x65, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.ac2_lfo_amod",    "AC2 LFO AM",  area::part, 0x08, 0, 0x66, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	// ベロシティの範囲（08 pp 6D-6E）。この強さの間だけ鳴る
	{ "part.vel_limit_low",   "Vel Lim Lo",  area::part, 0x08, 0, 0x6d, 1, coding::byte7,  1,    127,  -1, 1,    view::raw,    0,    nullptr },
	{ "part.vel_limit_high",  "Vel Lim Hi",  area::part, 0x08, 0, 0x6e, 1, coding::byte7,  1,    127,  -1, 127,  view::raw,    0,    nullptr },
	// ポルタメントとピッチ EG（08 pp 67-6C）。firmware の液晶のメニュー（Part Edit）の
	// 表にこの番地があり、問い合わせにも答える。ワーク RAM では塊の +0x60 から
	{ "part.porta_switch",    "Porta Sw",    area::part, 0x08, 0, 0x67, 1, coding::byte7,  0,    1,    -1, 0,    view::choice, 0,    OFF_ON },
	{ "part.porta_time",      "Porta Time",  area::part, 0x08, 0, 0x68, 1, coding::byte7,  0,    127,  -1, 0,    view::raw,    0,    nullptr },
	{ "part.peg_init_level",  "PEG Init",    area::part, 0x08, 0, 0x69, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.peg_attack_time", "PEG Attack",  area::part, 0x08, 0, 0x6a, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.peg_rel_level",   "PEG Rel Lv",  area::part, 0x08, 0, 0x6b, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	{ "part.peg_rel_time",    "PEG Rel Tm",  area::part, 0x08, 0, 0x6c, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	// HPF の切る高さ。**番地は 0A pp 20**（08 pp ではない）。液晶のメニューの表から見つけ、
	// 7F にすると 150Hz より下が 45dB 下がるのを確かめた。ワーク RAM では塊の +0x78
	{ "part.hpf_cutoff",      "HPF Cutoff",  area::part, 0x0a, 0, 0x20, 1, coding::byte7,  0,    127,  -1, 64,   view::center, 64,   nullptr },
	// パートの EQ（08 pp 72-77）。周波数の範囲は firmware が切り詰めた値。ゲインは firmware が
	// 0-127 をそのまま受けるが、XG の決まりの ±12dB（52-76）に留める
	{ "part.eq_bass_gain",    "EQ Bass G",   area::part, 0x08, 0, 0x72, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "part.eq_treble_gain",  "EQ Treb G",   area::part, 0x08, 0, 0x73, 1, coding::byte7,  52,   76,   -1, 64,   view::center, 64,   nullptr },
	{ "part.eq_bass_freq",    "EQ Bass F",   area::part, 0x08, 0, 0x76, 1, coding::byte7,  4,    40,   -1, 12,   view::raw,    0,    nullptr },
	{ "part.eq_treble_freq",  "EQ Treb F",   area::part, 0x08, 0, 0x77, 1, coding::byte7,  28,   58,   -1, 54,   view::raw,    0,    nullptr },
};

u8 checksum(const u8 *p, size_t n)
{
	u32 sum = 0;
	for (size_t i = 0; i < n; i++)
		sum += p[i];
	return u8((0x80 - (sum & 0x7f)) & 0x7f);
}

// 値を SysEx のデータのバイトに崩す
void encode(const param &p, int value, u8 *out)
{
	for (int i = 0; i < p.size; i++) {
		const int shift = (p.size - 1 - i) * (p.enc == coding::nibble ? 4 : 7);
		out[i] = u8((value >> shift) & (p.enc == coding::nibble ? 0x0f : 0x7f));
	}
}

} // namespace


const std::vector<param> &params() { return TABLE; }

const param *find(const std::string &key)
{
	for (const param &p : TABLE)
		if (key == p.key)
			return &p;
	return nullptr;
}

u32 address(const param &p, int part)
{
	return pack(p.hi, p.where == area::part ? u8(part) : p.mid, p.lo);
}

bool valid(const param &p, int value)
{
	return (value >= p.min && value <= p.max) || (p.special >= 0 && value == p.special);
}

std::string format(const param &p, int value)
{
	char buf[48];
	switch (p.how) {
	case view::plus1:
		std::snprintf(buf, sizeof(buf), "%d", value + 1);
		break;
	case view::center:
		std::snprintf(buf, sizeof(buf), "%+d", value - p.center);
		break;
	case view::pan:
		if (value == 0)       std::snprintf(buf, sizeof(buf), "Rnd");
		else if (value == 64) std::snprintf(buf, sizeof(buf), "C");
		else if (value < 64)  std::snprintf(buf, sizeof(buf), "L%d", 64 - value);
		else                  std::snprintf(buf, sizeof(buf), "R%d", value - 64);
		break;
	case view::choice:
		if (p.choices && value >= p.min && value <= p.max)
			return p.choices[value - p.min];
		std::snprintf(buf, sizeof(buf), "%d", value);
		break;
	case view::part_off:
		if (value == p.special) std::snprintf(buf, sizeof(buf), "OFF");
		else                    std::snprintf(buf, sizeof(buf), "Part %d", value + 1);
		break;
	default:
		std::snprintf(buf, sizeof(buf), "%d", value);
		break;
	}
	return buf;
}


std::vector<u8> param_change(const param &p, int part, int value)
{
	const u32 a = address(p, part);
	std::vector<u8> m = { 0xf0, 0x43, 0x10, 0x4c, u8(a >> 14), u8((a >> 7) & 0x7f), u8(a & 0x7f) };
	u8 data[8] = {};
	encode(p, value, data);
	m.insert(m.end(), data, data + p.size);
	m.push_back(0xf7);
	return m;
}

std::vector<u8> param_request(const param &p, int part)
{
	const u32 a = address(p, part);
	return { 0xf0, 0x43, 0x30, 0x4c, u8(a >> 14), u8((a >> 7) & 0x7f), u8(a & 0x7f), 0xf7 };
}

std::vector<u8> dump_request(u32 a)
{
	return { 0xf0, 0x43, 0x20, 0x4c, u8(a >> 14), u8((a >> 7) & 0x7f), u8(a & 0x7f), 0xf7 };
}


// ---- 写し

void model::feed(u8 b)
{
	if (b >= 0xf8)
		return;                         // リアルタイムは SysEx の途中にも来うる
	if (b == 0xf0) {
		m_msg.clear();
		m_msg.push_back(b);
		m_in = true;
		return;
	}
	if (!m_in)
		return;
	if (b & 0x80 && b != 0xf7) {        // 途中で別のメッセージが始まった。捨てる
		m_in = false;
		m_rejected++;
		return;
	}
	m_msg.push_back(b);
	if (b == 0xf7) {
		m_in = false;
		on_sysex();
	} else if (m_msg.size() > 8192) {
		m_in = false;
		m_rejected++;
	}
}

void model::on_sysex()
{
	const std::vector<u8> &m = m_msg;
	// F0 43 xn 4C ...
	if (m.size() < 8 || m[1] != 0x43 || m[3] != 0x4c)
		return;
	const u8 kind = m[2] & 0xf0;
	if (kind == 0x10) {
		// パラメータチェンジ: F0 43 1n 4C hh mm ll data... F7
		const u32 a = pack(m[4], m[5], m[6]);
		store(a, m.data() + 7, m.size() - 8);
	} else if (kind == 0x00) {
		// 一括ダンプ: F0 43 0n 4C cntH cntL hh mm ll data... sum F7
		if (m.size() < 11)
			return;
		const size_t count = size_t(m[4]) << 7 | m[5];
		if (m.size() != count + 11) {
			m_rejected++;
			return;
		}
		// チェックサムは「数・番地・データ・チェックサム」を足して下位 7bit が 0
		if (checksum(m.data() + 4, count + 5) != m[count + 9]) {
			m_rejected++;
			return;
		}
		store(pack(m[6], m[7], m[8]), m.data() + 9, count);
	} else {
		return;                          // 問い合わせなど。こちらが読むものではない
	}
	m_accepted++;
}

void model::store(u32 addr, const u8 *data, size_t n)
{
	// 番地は 7bit ずつの 3 桁。ダンプは lo の桁を超えて続かない前提で、lo を進める
	const u8 hi = u8(addr >> 14), mid = u8((addr >> 7) & 0x7f);
	for (size_t i = 0; i < n; i++) {
		const u32 lo = (addr & 0x7f) + u32(i);
		if (lo > 0x7f)
			break;
		const u32 a = pack(hi, mid, u8(lo));
		if (!m_pinned.empty()) {
			const auto pin = m_pinned.find(a);
			if (pin != m_pinned.end()) {
				if (m_now - pin->second < PIN_MS)
					continue;                // 書いたばかり。こちらの値が新しい
				m_pinned.erase(pin);
			}
		}
		m_bytes[a] = data[i] & 0x7f;
	}
	if (m_waiting && addr == m_wait_addr) {
		m_waiting = false;
		m_tries = 0;
	}
}

bool model::get(const param &p, int part, int &value) const
{
	const u32 a = address(p, part);
	int v = 0;
	for (int i = 0; i < p.size; i++) {
		const auto it = m_bytes.find(a + u32(i));
		if (it == m_bytes.end())
			return false;
		v = (v << (p.enc == coding::nibble ? 4 : 7)) | (it->second & (p.enc == coding::nibble ? 0x0f : 0x7f));
	}
	value = v;
	return true;
}

bool model::get_raw(u32 addr, int size, int &value) const
{
	int v = 0;
	for (int i = 0; i < size; i++) {
		const auto it = m_bytes.find(addr + u32(i));
		if (it == m_bytes.end())
			return false;
		v = v << 7 | (it->second & 0x7f);
	}
	value = v;
	return true;
}

std::vector<u8> model::set_raw(u32 addr, int size, int value)
{
	std::vector<u8> m = { 0xf0, 0x43, 0x10, 0x4c, u8(addr >> 14), u8((addr >> 7) & 0x7f), u8(addr & 0x7f) };
	for (int i = 0; i < size; i++) {
		const u8 b = u8((value >> (7 * (size - 1 - i))) & 0x7f);
		m_bytes[addr + u32(i)] = b;
		m_pinned[addr + u32(i)] = m_now;
		m.push_back(b);
	}
	m.push_back(0xf7);
	if (m_edit_raw)
		m_edit_raw(addr, size, value);
	return m;
}

bool applies_on_program(const param &p)
{
	return p.where == area::part && p.hi == 0x08 && (p.lo == 0x01 || p.lo == 0x02);
}

std::vector<u8> model::set(const param &p, int part, int value)
{
	const u32 a = address(p, part);
	u8 data[8] = {};
	encode(p, value, data);
	for (int i = 0; i < p.size; i++) {
		m_bytes[a + u32(i)] = data[i];
		m_pinned[a + u32(i)] = m_now;
	}
	std::vector<u8> out = param_change(p, part, value);
	if (m_edit)
		m_edit(p, part, value);
	// バンクはプログラムを書くまで効かない。写しに今のプログラムがあれば続けて送る。
	// 無ければバンクだけ送る（次にプログラムを選んだときに効く）
	int prog = 0;
	if (applies_on_program(p)) {
		const param *pp = find("part.program");
		if (pp && get(*pp, part, prog)) {
			const std::vector<u8> more = param_change(*pp, part, prog);
			out.insert(out.end(), more.begin(), more.end());
		}
	}
	return out;
}

void model::want_dump(u32 addr)
{
	if (m_waiting && m_wait_addr == addr)
		return;
	if (std::find(m_queue.begin(), m_queue.end(), addr) == m_queue.end())
		m_queue.push_back(addr);
}

std::vector<u8> model::poll(u64 now_ms)
{
	m_now = now_ms;
	// 頼んだ返事がまだ。塊 1 つは 31250bps の線で 20ms もかからないが、
	// firmware が手を離せないこともあるので 400ms 待ってから 1 回だけ頼み直す
	if (m_waiting) {
		if (now_ms - m_sent_ms < 400)
			return {};
		if (m_tries >= 2) {
			m_waiting = false;           // 諦める。次へ
			m_tries = 0;
		} else {
			m_sent_ms = now_ms;
			m_tries++;
			return dump_request(m_wait_addr);
		}
	}
	if (m_queue.empty())
		return {};
	m_wait_addr = m_queue.front();
	m_queue.pop_front();
	m_waiting = true;
	m_sent_ms = now_ms;
	m_tries = 1;
	return dump_request(m_wait_addr);
}

void model::forget(const param &p, int part)
{
	const u32 a = address(p, part);
	for (int i = 0; i < p.size; i++) {
		m_bytes.erase(a + u32(i));
		m_pinned.erase(a + u32(i));
	}
}

void model::forget()
{
	m_bytes.clear();
	m_pinned.clear();
	m_queue.clear();
	m_waiting = false;
	m_in = false;
	m_tries = 0;
}

} // namespace xg
