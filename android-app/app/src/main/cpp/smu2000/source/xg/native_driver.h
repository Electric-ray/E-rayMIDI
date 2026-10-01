// license:BSD-3-Clause
//
// **firmware を走らせずに音を鳴らす口**（doc/native-engine.md の段 2）。
//
// 考え方はこう。
//
//   * 起動と、音色を選ぶところ（プログラムチェンジ・SysEx）は firmware に任せる。
//     そこは曲の頭で数回しか起きないので、重さに効かない
//   * **その音色の 1 音目も firmware に鳴らさせて、スロットに書かれた値を写し取る**
//     （voice_cal）。式が分かっていない所（フィルタ・素通しの量など）はこれで埋まる
//   * 2 音目からは CPU を止めたまま、この口が式でレジスタを作って鳴らす
//
// 鍵と強さで動くもの（音程・波形・包絡線・音量）は式で出すので、写し取りは
// 音色あたり 1 回で足りる。覚えるのは利用者の ROM から起こした値で、配らない。

#ifndef S_MU2000_XG_NATIVE_DRIVER_H
#define S_MU2000_XG_NATIVE_DRIVER_H

#pragma once

#include "xg/native_voice.h"
#include "xg/ram.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#include <vector>

namespace xg {

class native_driver
{
public:
	static constexpr int PARTS = 64;
	// **声のスロットは 2 つの SWP30 を合わせた 128**。0-63 がマスタ、64-127 が
	// スレーブ。実機の firmware はマスタから使い、あふれるとスレーブへ回す。
	// 前はマスタの 64 だけで、65 声目から先は古い声を奪っていた（画面の
	// 発音数でスレーブがいつも 0 だった）。firmware がスレーブの声に書く値は
	// マスタと 1 つも違わない（同じ音をそれぞれで鳴らして全レジスタを比べた）。
	// レジスタの番地は `スロット * 64 + reg` のままで、0x1000 から上がスレーブ
	static constexpr int SLOTS = 128;
	static constexpr int CHIP_SLOTS = 64;
	// スレーブの声の出口（0x35-0x37）の値。書き込みの最後（mu2000 の m_poke）で使う
	static u16 slave_mixer(u16 master) { return slave_route(master); }

	// スロットの印（128 ビット）。32 ビットの Windows でも作れるように 64 ビット 2 つで持つ
	struct slot_bits
	{
		u64 w[2] = { 0, 0 };
		static slot_bits of(int i)
		{
			slot_bits b;
			b.w[i >> 6] = u64(1) << (i & 63);
			return b;
		}
		bool test(int i) const { return (w[i >> 6] >> (i & 63)) & 1; }
		void set(int i) { w[i >> 6] |= u64(1) << (i & 63); }
		explicit operator bool() const { return (w[0] | w[1]) != 0; }
		slot_bits &operator|=(const slot_bits &o)
		{
			w[0] |= o.w[0];
			w[1] |= o.w[1];
			return *this;
		}
	};

	// **フィルタの段を流す時刻の補正**（サンプル）。
	// 段の時刻は firmware に鳴らさせた音から録るが、録るときの時計と
	// 流すときの時計で、数え始めの位置が少しずれる（録るのは run_cycles の
	// 中、流すのは tick の中で、同じサンプルでも順番が違う）。
	// 実測で決めた: 乾いた音の 2 音目を実機と突き合わせて、-3 で
	// **1 ビットも違わなくなる**（-2 だと 99.9%、0 だと 99.8%）。
	// doc/native-engine.md の 6.63
	static constexpr int EG_LAG = -3;

	// **フィルタの包絡線は式で動かす**（doc/native-engine.md の 6.63）。
	// 写し取った録画の代わりに、要素のバイトから折れ線を組み立てる。
	// SMU2000_NO_FENV を立てると、前の「録画を流す」やり方に戻る
	static bool fenv_on()
	{
		static const bool on = std::getenv("SMU2000_NO_FENV") == nullptr;
		return on;
	}

	// **実機がスロットを空けたら書くのをやめる**（6.207）。
	// SMU2000_NO_SLOTFREE を立てると、前の「包絡線が底まで追う」やり方に戻る
	static bool slotfree_on()
	{
		static const bool on = std::getenv("SMU2000_NO_SLOTFREE") == nullptr;
		return on;
	}

	// SMU2000_NATIVE_DEBUG が立っていれば、鳴らすたびに値を出す（調べもの用）
	// **ドラムの打だけを見る調べ用**（`SMU2000_DRUM_DEBUG=1`）。打ごとに、受けたとき・鍵を押したとき・
	// 押して 441 サンプル（10ms）後にチップでその声が鳴っているかを 1 行ずつ出す。
	// SMU2000_NATIVE_DEBUG は量が多くて gui の音が途切れるので、こちらは打のことだけ
	static bool drum_debug_on()
	{
		static const bool on = std::getenv("SMU2000_DRUM_DEBUG") != nullptr;
		return on;
	}

	static bool debug_on()
	{
		static const bool on = std::getenv("SMU2000_NATIVE_DEBUG") != nullptr;
		return on;
	}

	// SMU2000_VIB_TRACE が立っていれば、**遅れて掛かるビブラート**の
	// せり上がりを出す（`--trace-swp` は native の口の書き込みを拾わないので、
	// firmware の道と時刻を突き合わせるのに要る。6.232）
	static bool vib_trace()
	{
		static const bool on = std::getenv("SMU2000_VIB_TRACE") != nullptr;
		return on;
	}

	// スロット 1 つの使われ方
	struct slot_use {
		bool on = false;
		// **鍵をもう押したか**。要素を書き終えてから押すので、混んでいると押すのが
		// 数十 ms 遅れる（write_done）。その間に届いた離しは off_wait に預けて、
		// 押した直後に効かせる（実機は MIDI を順に処理するので、押しを終えてから
		// 離しを読む）。先に離していたので、ゲートの短いドラムが混んだ所で
		// 鳴らなかった（離した印の付いたスロットへ遅れて鍵が押されていた）
		bool keyed = false;
		bool off_wait = false;
		u32 tpos = 0;                   // フィルタの包絡線の、つぎに書く段
		u64 tstart = 0;                 // 鳴らし始めた時刻
		bool held = false;              // ダンパーで離しを待たせている
		bool sost = false;              // ソステヌート（CC66）で離しを待たせている
		// **離しの最中**（on は落ちたが、まだ鳴り終わっていない）。
		// 実機はこの間もつまみの動きを反映するので、こちらも追う必要がある。
		// 追わないと、曲の終わりの CC7 のフェードアウトで、離したばかりの
		// 長い音（ストリングスなど）だけが元の音量のまま鳴り続ける
		bool rel = false;
		u64  rel_at = 0;                // 離した時刻
		u32  rpos = 0;                  // 離してからの段の、つぎに書く位置
		int  rel_att = 0;               // 離しのときに書いた減衰（戻さないための下限）
		// **Rnd のパンで当たった位置**（0-127。-1 は Rnd ではない）。
		// 鳴らし始めに 1 度引いて、そのあとは動かさない（6.147）
		// **つまみの差を乗せる元の値**（パン・リバーブ送り・コーラス送り）。
		// 写し取りがあればその値、無ければ式で組んだ値。ここを持たずに
		// `cal->reg[]` を見ていたので、合成の写しでは音色のパンが消えていた
		// ダンパーで拾われた（6.164）。ペダルを離したら離し直す
		bool caught = false;
		u16  base32 = 0, base33 = 0, base34 = 0;
		int  rnd_pan = -1;
		int  rnd_drop = 0;              // Rnd のときの送りの目減り
		int  vel = 0;                   // 押した強さ（液晶のメーター用）
		// **キーアサインがシングルで切られた音**。離しの速さが 0xD9 になる
		bool single_cut = false;
		// **オルタネートグループで切った打の、止めを刺す時刻**（0 は無し）。
		// 実機は 0xD9 で離したあと 1010 サンプル（23ms）で 0xF0 を書く
		u64  alt_kill = 0;
		int part = -1, note = -1, att = 0;
		// **MIDI で押された鍵**。note のほうは XG のノートシフト（08 pp 08）を
		// 足した「鳴らす鍵」なので、離すときの照合はこちらで見る
		int keynote = -1;
		// **音量の目盛り**（つまみを掛ける前）と、目盛りに乗らない側の減衰。
		// 実機は目盛りに音量を掛けてから 1 回だけ表を引くので、CC7・CC11 が
		// 動いたらこの 2 つから作り直す（doc/native-engine.md の 6.101）
		int lvl0 = 0, arest = 0;
		const u8 *elem = nullptr;
		const u8 *wave = nullptr;       // ベンドで音程を作り直すのに要る
		const nv::voice_cal *cal = nullptr;
		u16 lfo = 0;                    // いま鳴らしている 0x0a（モジュレーションを足す前）
		u16 cut = 0;                    // いま鳴らしている 0x00（明るさを足す前）
		u16 drum_rel = 0;
		// **ポルタメント**。glide は「まだ残っている音程のずれ」（セント × 256。
		// 前の鍵の側が正にも負にもなる）。10ms ごとに step ずつ 0 へ寄せる
		// **フィルタの包絡線**（doc/native-engine.md の 6.63）。
		// 写し取った録画の代わりに、こちらで式から動かす
		int facc = 0, ftgt = 0, finc = 0, fstage = 0, fadj = 0, fvel = 100;
		bool hard = false;              // オールサウンドオフで切った（離しを最速に）
		u32 inst = 0;                   // 何回目の押しか（同じ鍵を重ねたとき用）
		u64 fnext = 0;                  // つぎに 1 段進める時刻
		// **サンプル＆ホールドの音程**（6.206）
		u64 sh_next = ~u64(0);          // つぎの 20ms の刻み
		int sh_cnt = 0;                 // 残りの刻み（0 なら無し）
		int sh_off = 0;                 // いまのずらし
		// **音程の包絡線の行き先**。実機はキーオンの直後にこれを書いて、
		// あとはチップに任せる（doc/native-engine.md の 6.68）。
		// 0xffff は「書くものが無い」の印
		u16 peg_tgt = 0xffff;
		// **遅れて掛かるビブラート**（6.175）
		int vcnt = 0;          // いまのカウンタ
		int vtgt = 0;          // 止まる所
		int vstep = 1;         // 1 歩
		int vdly = 0;          // 遅れの残り（20ms の目盛り）
		// **SFX キットの打**（6.234）。要素を持って旋律の道で組んだが、離しは
		// ドラムの決まり（3n rr 09 が立っていなければ鳴りきる）に従う
		bool sfx = false;
		// SFX の打の要素の写し。**ドラムセットアップの値を足してある**（6.236）。
		// elem はここを指す（スロットが鳴っている間ずっと使うので、スロットに持たせる）
		std::array<u8, nv::ELEM_BYTES> sfx_el{};
		u16 vhi = 0;           // `0x0a` の上位（型と刻み）
		u16 ahi = 0;           // `0x05` の上位
		int vamp = 0;          // 遅れが明けたあとの、音量側の揺れ
		int fdvel = 100;       // フィルタの包絡線の深さだけに使う強さ（6.182）
		// **フィルタ側の LFO**（6.189）。firmware が自前で持っている
		// 15bit の位相と、そこから作った「切る高さへ足すぶん」
		u32 lph = 0;           // 位相（0-0x7fff）
		u16 lstep = 0;         // 10ms あたりの歩幅
		int lfdep = 0;         // 深さ（遅れが明けるまで 0）
		int lfull = 0;         // 遅れが明けたあとの深さ
		int lcut = 0;          // 切る高さへ足すぶん
		bool ltri = true;      // 三角波か（byte9 が 0 でなければ）
		bool lrun = false;     // 遅れが明けて、位相を進める段階に入ったか
		// **つまみの割り当てで音程を書き直す印**（6.195）。
		// 実機は受けた瞬間ではなく、**次の 10ms の刻み**で書く
		bool pdirty = false;
		int vfull = 0;         // つまみまで入れた、せり上がり切った深さ（今は使わない。6.217）
		// **Vib Depth のぶんのせり上がり**（6.217）。0 から 20ms ごとに 5 ずつ
		int vcnt2 = 0;
		int vdep = 64;         // 押したときの Vib Depth（08 pp 16）
		bool vramp = false;    // 遅れて掛かるビブラートを式で動かしている（録画の 0x0a は流さない）
		u64 vnext = ~u64(0);   // つぎに進める時刻
		// **音程の包絡線の段**（0 が押した直後の段。3 で終わり）。
		// チップが行き先に着いたら次の段を張る（doc の 6.80）
		int pstage = 3;
		int pvel = 100;
		u64 pnext = 0;              // つぎに着いたか見る時刻
		s32 glide = 0, glide_step = 0;
		u64 glide_next = 0;
		u64 age = 0;
	};

	// SWP30 へ 1 レジスタ書く口。0x1000 から上はスレーブ（スロット 64-127）
	using poke_fn = std::function<void(u32 reg, u16 value)>;

	void set_poke(poke_fn f) { m_poke = std::move(f); }
	// **チップの「音程の包絡線が着いた」印を覗く**。実機の firmware も
	// 内部レジスタ 4 の bit14 で同じものを見ている
	using peek_fn = std::function<bool(int)>;
	void set_peg_peek(peek_fn f) { m_peg_peek = std::move(f); }
	// **そのスロットがまだ鳴っているか**をチップに聞く（6.151）。
	// オルタネートグループで切る相手を選ぶのに使う
	void set_slot_peek(peek_fn f) { m_slot_peek = std::move(f); }
	// **実機がまだそのスロットを持っているか**（6.207）。離した音の
	// 尾をどこまで追うかはこれで決める
	void set_slot_held(peek_fn f) { m_slot_held = std::move(f); }
	void set_rom(const u8 *rom) { m_rom = rom; }
	// **そのスロット（0-63 マスタ、64-127 スレーブ）を今 native が鳴らしているなら、そのパート**。
	// 鳴らしていなければ -1（画面のスペクトラムが声をパートに振り分けるのに使う）
	int slot_part(int i) const
	{
		if (i < 0 || i >= SLOTS)
			return -1;
		const slot_use &s = m_slot[size_t(i)];
		return (s.on || s.rel) ? s.part : -1;
	}
	// ワーク RAM（firmware が音色を選んだ結果を読む）
	void set_ram(u8 *ram) { m_ram = ram; m_ramw = ram; }

	void reset()
	{
		m_cal.clear();
		m_drum.clear();
		for (auto &a : m_drum_touch)
			a.fill(0);
		for (auto &a : m_pat)
			a.fill(0);
		for (auto &a : m_asn)
			a.fill(0);
		m_asn_have.fill(0);
		for (auto &s : m_slot)
			s = slot_use();
		for (auto &c : m_cc)
			c = part_cc();
		for (auto &s : m_seen)
			s = ram_seen();
		for (auto &r : m_recsel)
			r = 0;
		for (u64 &t : m_fw_touch)
			t = 0;
		for (auto &d : m_recsel_drum)
			d = -1;
		m_clock = 0;
		m_alt_kill_next = ~u64(0);
		m_traj = false;
		m_rec = false;
		m_traj_next = 0;
		m_pend.clear();
		m_busy = 0;
		m_bend_due.fill(0);
		m_bend_next = ~u64(0);
		m_age = 0;
	}

	// **リセット（XG システムオン・GM システムオン・GS リセット）**（6.136）。
	// パートの状態を既定に戻し、鳴っている音を切る。**写し取りの覚えは
	// 消さない**（音色の記録＋経路ごとに覚えているので、そのまま使える）。
	// 入れるまでは、リセットのあとも native が古い音色・古いつまみで
	// 鳴らしていた（曲の途中でリセットを入れる曲は珍しくない）
	void reset_parts()
	{
		for (int p = 0; p < PARTS; p++) {
			all_off(p, true);
			m_cc[p] = part_cc();
		}
		// **ドラムのセットアップも既定に戻る**（XG・GM のシステムオンで、打ごとに
		// 書いた切る高さが消えるのを firmware で確かめた。2026-09-28）
		clear_drum_setup(-1);
		m_pend.clear();
		m_bend_due.fill(0);
		m_bend_next = ~u64(0);
	}

	// 写し取りの覚え先の鍵。**音色の記録（下 32bit）＋パートの経路（上 32bit）**。
	// 経路が違えば別物として覚えるので、つまみを行き来しても取り直しは 1 度で済む
	u64 cal_key(u32 rec, int part) const { return u64(rec) | (u64(part_ctx(part)) << 32); }

	// 覚えておく写し取りの上限。ふだんは音色の数だけなので数十で足りるが、
	// DAW がつまみを掃くと経路の印がそのぶん増えるので、天井を付けておく。
	// 溢れたら覚えないだけ（その音は firmware が鳴らす）
	static constexpr size_t CAL_MAX = 512;

	// firmware に鳴らさせた 1 音から写し取る。鍵は写しに入っている経路から組む
	void learn(u32 rec, std::vector<nv::voice_cal> cals)
	{
		if (cals.empty() || m_cal.size() >= CAL_MAX)
			return;
		const u64 k = u64(rec) | (u64(cals[0].cal_ctx) << 32);
		if (m_cal.find(k) == m_cal.end())
			m_cal[k] = std::move(cals);
	}

	// ドラムは音ごとに中身が違うので、**鍵ごと**に覚える。
	// 同じ音を何度も叩くので、これだけで打楽器のほとんどが native になる
	void learn_drum(u64 key, std::vector<nv::voice_cal> cals)
	{
		if (cals.empty() || m_drum.size() >= CAL_MAX)
			return;
		if (m_drum.find(key) == m_drum.end())
			m_drum[key] = std::move(cals);
	}

	// ドラムのパートか。XG の「パートモード」（08 pp 07。0 が普通、1 以上がドラム）を見る。
	// 「記録が引けない＝ドラム」では、音色を選び終える前の旋律パートまで拾ってしまう
	bool is_drum(int part) const
	{
		if (part >= 0 && part < PARTS && m_recsel_drum[part] >= 0)
			return m_recsel_drum[part] != 0;
		if (!m_ram || part < 0 || part >= PARTS)
			return false;
		return m_ram[ram::part_base(part) + 0x07] != 0;
	}

	// そのパート・鍵のドラムの 1 打の記録（42 バイト）。無ければ nullptr
	const u8 *drum_rec_of(int part, int note) const
	{
		if (!m_ram || !m_rom || part < 0 || part >= PARTS)
			return nullptr;
		return nv::drum_record(m_rom, int(m_ram[ram::part_base(part) + nv::PART_KIT]), note);
	}

	// 写し取ったものを取っておく・戻す（voicecache.h）
	const std::unordered_map<u64, std::vector<nv::voice_cal>> &cal_map() const { return m_cal; }
	const std::unordered_map<u64, std::vector<nv::voice_cal>> &drum_map() const { return m_drum; }
	size_t cal_count() const { return m_cal.size() + m_drum.size(); }
	int peak_slots() const { return m_peak; }
	// 写し取りの記録が足りないまま組んだ音の数（6.219）
	u32 cal_missing() const { return m_cal_missing; }

	// **firmware が最近触ったスロット**を覚える。firmware はこちらの使用中を
	// 知らないので、避けないと「firmware が自分の音の続きを書く」ときに
	// こちらの音が壊れる（doc/native-engine.md の 6.47）。
	// 呼ぶのは mu2000 のバス書き込みの所（firmware の書き込みだけが通る）
	// `base` はそのチップの先頭スロット（マスタ 0、スレーブ 64）
	void mark_fw_slots(u64 mask, int base = 0)
	{
		for (int i = 0; i < CHIP_SLOTS; i++)
			if ((mask >> i) & 1)
				m_fw_touch[base + i] = m_clock + 1;   // 0 は「触っていない」
	}

	// **firmware が声のレジスタに書いたスロットも、その firmware のものとみなす**
	//（6.220）。鍵を押した瞬間の印だけだと、firmware の音が 2 秒より長く伸びる
	// ときに印が切れ、こちらがそのスロットを取ってしまう。すると firmware は
	// 自分の音の続き（フィルタの包絡線 0x00・0x01・0x04 など）を書き続けるので、
	// こちらの音が途中で化ける。書いている間は避け続ければ、それが起きない。
	// **毎サンプル書き替わる MEG の戻り（0x0e・0x0f・0x38-0x3f）は除く**こと。
	// それを含めると全スロットが firmware のものになってしまう（呼ぶ側で除く）
	void mark_fw_slot(u32 slot)
	{
		if (slot < SLOTS)
			m_fw_touch[slot] = m_clock + 1;
	}

	// **踏まれたスロットは諦める**（6.220）。firmware が、こちらが鳴らしている
	// スロットに自分の音を置いてしまったときに呼ばれる。そこはもう firmware の
	// 包絡線が走っているので、こちらが書き続けても**二重に書いた音**にしかならない。
	// こちらの音は手放して（キーオフは送らない。送ると firmware の音が切れる）、
	// 以後そのスロットは firmware のものとして避ける。
	// `SMU2000_NO_YIELD=1` で、この譲りを止めて前のままにできる
	void yield_slot(u32 slot)
	{
		static const bool off = std::getenv("SMU2000_NO_YIELD") != nullptr;
		if (off || slot >= SLOTS)
			return;
		slot_use &u = m_slot[slot];
		if (!u.on && !u.rel)
			return;
		u.on = u.held = u.sost = u.rel = false;
		u.cal = nullptr;
		u.elem = nullptr;
		m_fw_touch[slot] = m_clock + 1;
	}

	// **包絡線の格子の位相**。実機の包絡線は 441 サンプルの全体共通の格子で
	// 進む（doc/native-engine.md の 6.60）。その位相は起動から決まっているので、
	// native の口が始まる前に firmware が書いた 0x00 の時刻から拾っておく。
	// native の口が始まったあとは firmware の時間が遅れるので、拾い直さない
	// **10ms タイマの位相を実機から学ぶ**（6.118）。firmware の 10ms 割り込みは
	// 世界共通なので、包絡線も滑りも「鍵を押した時刻」ではなくこの格子に乗る。
	//
	// **いちばん早いものを取ってはいけない**。`0x00` はタイマだけでなく
	// **鍵を押したときにも**書かれる。そちらは好きな時刻に来るので、
	// 1 回でも早いものが混じると位相がそこに居着いてしまう（`--bootcache`
	// では正しく、実際に起動させると 141 サンプルずれていた。6.118）。
	// タイマの書き込みは 1 か所に集まり、鍵のぶんは散らばるので、
	// **いちばん数の多い位相**を取る
	// **10ms の印から学べたら、それだけを信じる**（6.209）。
	// 写し取りの最中の `0x00` は鍵を押したときにも書かれるうえ、
	// native の口では firmware の時間が伸びているので目が合わない。
	// 混ぜていたせいで、写し取りをする道だけ位相が
	// 218 サンプル（半目）ずれていた
	void set_eg_phase(u32 sample)
	{
		// **一度決まったらもう動かさない**（6.209）。数え続けていたので、
		// 写し取りの最中に firmware が書いた`0x00`（firmware の時間は
		// native の口では伸びている）があとから追い越して、
		// 写し取りをする道だけ位相が242 → 24 にずれていた。
		// ベンドも包絡線も滑りもこの格子に乗るので、影響は広い
		if (m_eg_have)
			return;
		const u32 p = sample % FENV_TICK;
		if (m_eg_hits[p] == 0xffff)
			return;
		const u16 n = ++m_eg_hits[p];
		if (n > m_eg_best) {
			m_eg_best = n;
			m_eg_phase = p;
			// 数が溜まるまでは信じない（鍵のぶんだけで決めないように）
			if (n >= EG_PHASE_MIN)
				m_eg_have = true;
		}
	}
	static constexpr u16 EG_PHASE_MIN = 16;
	// **フィルタの包絡線も 10ms 格子**（6.133）。6.121 のときは
	// keylevel が 100% → 99% と落ちたので切っていたが、そのあとの直し
	// （6.123 の鍵の追従の二重掛け・6.124 の写し取りの上書き）で前提が
	// 変わり、いまは落ちるところが無く rpn が 98% → 99% になる。
	// firmware の 10ms 割り込みが包絡線も動かしている以上こちらが正しい形。
	// `SMU2000_EG_GRID=0` で前の道（写し取りの at0 を鍵の時刻に足す）に戻せる
	static bool eg_grid()
	{
		static const bool on = [] {
			const char *e = std::getenv("SMU2000_EG_GRID");
			return !e || (e[0] != '0' || e[1]);
		}();
		return on;
	}
	// **滑りは実機の 10ms 格子に乗せる**（6.121）。`SMU2000_PORTA_GRID=0` で
	// 前の道（写し取りの相対値を鍵の時刻に足す）に戻せる
	static bool peg_grid()
	{
		static const bool on = [] {
			const char *e = std::getenv("SMU2000_PEG_GRID");
			return !e || (e[0] != '0' || e[1]);
		}();
		return on;
	}
	static bool porta_grid()
	{
		static const bool on = [] {
			const char *e = std::getenv("SMU2000_PORTA_GRID");
			return !e || (e[0] != '0' || e[1]);
		}();
		return on;
	}
	// x 以降でいちばん早い格子の目
	u64 eg_after(u64 x) const
	{
		const u64 base = x - (x % FENV_TICK) + m_eg_phase;
		return base >= x ? base : base + FENV_TICK;
	}

	// そのスロットを firmware がまだ使っていそうか
	bool fw_recent(int slot) const
	{
		const u64 t = m_fw_touch[slot];
		return t && m_clock + 1 - t < FW_KEEP;
	}

	// いまこちらが鳴らしているスロットの印。firmware が写し取りのために
	// 鳴らすとき、ここと重なっていないかを見るのに使う
	slot_bits slot_mask() const
	{
		slot_bits m;
		for (int i = 0; i < SLOTS; i++)
			if (m_slot[i].on)
				m.set(i);
		return m;
	}

	// 写し取りの最中は、段が後から増えるので毎サンプル見る
	void set_recording(bool on) { m_rec = on; m_traj_next = 0; }

	// 写し取ったものを、あとから直せるように渡す（フィルタの包絡線の追記用）
	std::vector<nv::voice_cal> *cals_of(u32 rec, int part)
	{
		const auto it = m_cal.find(cal_key(rec, part));
		return it == m_cal.end() ? nullptr : &it->second;
	}
	// **覚えたときの経路で引く**。写し取りを覚えてからフィルタの動きを
	// 録り始めるまでに、そのパートの経路が変わっていることがある。
	// いまの経路で引くと見つからず、録りが丸ごと落ちていた
	std::vector<nv::voice_cal> *cals_of_ctx(u32 rec, u32 ctx)
	{
		const auto it = m_cal.find(u64(rec) | (u64(ctx) << 32));
		return it == m_cal.end() ? nullptr : &it->second;
	}
	// その写し取りを捨てて、つぎの音で取り直させる。
	// **まだその写しを指しているスロットの指し先を外してから**消すこと。
	// 外さずに消すと、離しの最中のスロットが消えた中身を読みに行って落ちる
	void drop_cal(u32 rec, u32 ctx)
	{
		const auto it = m_cal.find(u64(rec) | (u64(ctx) << 32));
		if (it == m_cal.end())
			return;
		const nv::voice_cal *first = it->second.data();
		const nv::voice_cal *last  = first + it->second.size();
		for (slot_use &s : m_slot)
			if (s.cal >= first && s.cal < last) {
				s.cal = nullptr;
				s.rel = false;
			}
		m_cal.erase(it);
	}
	std::vector<nv::voice_cal> *drum_cals_of(u64 key)
	{
		const auto it = m_drum.find(key);
		return it == m_drum.end() ? nullptr : &it->second;
	}

	// フィルタの包絡線を流し、遅らせた要素を鳴らす。1 サンプルに 1 回呼ぶ
	void tick(u64 clock)
	{
		m_clock = clock;
		if (!m_pend.empty()) {
			// **同じ時刻のものは 1 回で押す**。実機も要素をまとめて
			// 押すので、要素ごとに分けると合図が 2 回になってしまう
			size_t w = 0;
			slot_bits now;
			for (size_t i = 0; i < m_pend.size(); i++) {
				if (m_pend[i].at <= clock)
					now |= m_pend[i].mask;
				else
					m_pend[w++] = m_pend[i];
			}
			m_pend.resize(w);
			if (now)
				key_on(now);
		}
		if (drum_debug_on() && !m_drum_watch.empty())
			drum_watch_check(clock);
		// **オルタネートグループで切った打に止めを刺す**（6.151）
		if (clock >= m_alt_kill_next) {
			u64 next3 = ~u64(0);
			for (int i = 0; i < SLOTS; i++) {
				slot_use &s = m_slot[i];
				if (!s.alt_kill)
					continue;
				if (s.alt_kill <= clock) {
					s.alt_kill = 0;
					m_poke(u32(i) * 64 + 9,
					       u16(0xf000 | u16(note_att(s, s.part) & 0xff)));
					s.on = false;
					s.rel = false;     // 音は消えたのでスロットを空ける
				} else if (s.alt_kill < next3) {
					next3 = s.alt_kill;
				}
			}
			m_alt_kill_next = next3;
		}
		// **格子に乗せたベンド**（6.125）
		if (clock >= m_bend_next) {
			u64 next2 = ~u64(0);
			for (int p = 0; p < PARTS; p++) {
				if (!m_bend_due[p])
					continue;
				if (m_bend_due[p] <= clock) {
					m_bend_due[p] = 0;
					apply_bend(p);
				} else if (m_bend_due[p] < next2) {
					next2 = m_bend_due[p];
				}
			}
			m_bend_next = next2;
		}
		if (!m_traj)
			return;
		// **つぎの段の時刻まで何もしない**。ここを毎サンプル 64 スロット見ていると、
		// SH-2 を止めた意味が薄れるくらい重かった。
		// 写し取りの最中だけは、段が後から増えるので毎回見る
		if (!m_rec && clock < m_traj_next)
			return;
		u64 next = ~u64(0);
		int live = 0;
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			// **サンプル＆ホールドの音程**（6.206）は、押していても
			// 離していても同じに回る。実機はスロットを使い回すまで
			// 止めないので、ここで止めると乱数の列がずれる
			// 止め時は実測で決めた。実機は離してから 145ms 後は引くが、
			// 675ms 後には止めている（スロットを空けている）
			if (s.on || (s.rel && clock - s.rel_at <= SH_TAIL))
				if (sh_tick(i, clock, next))
					live++;          // 見回りを止めない
			// **写し取りが無くても包絡線は動かす**（`SMU2000_CUT_EXACT=1` のとき）。
			// 式だけで `0x00` を出せるようになったので、録画は要らない（6.72）
			if (!s.cal && !(nv::cut_exact() && fenv_on() && s.elem)
			    && !(s.on && s.pstage < 3 && s.elem && m_peg_peek))
				continue;
			// **離しの最中もフィルタを動かす**。実機は離しのあいだも
			// 0x00・0x01・0x04 を書き続ける（doc/native-engine.md の 6.57）
			if (!s.on) {
				// **鳴り終わった声には書かない**（6.201）
				if (!rel_follow(s, clock))
					continue;
				// **離しの最中も包絡線を式で動かす**
				if (fenv_on() && s.elem) {
					bool moved = false;
					while (clock >= s.fnext) {
						fenv_step(s);
						lfo_tick(s);     // 離しのあいだも揺れる（6.192）
						s.fnext += FENV_TICK;
						moved = true;
					}
					if (moved) {
						s.cut = fenv_cut(s);
						m_poke(u32(i) * 64 + 0x00, cut_with_cc(s, s.cut));
						if (s.pdirty && s.wave) {
							s.pdirty = false;
							m_poke(u32(i) * 64 + 0x11, pitch_of(s));
						}
					}
					if (s.finc || s.lfdep) {
						live++;
						if (s.fnext < next)
							next = s.fnext;
					}
				}
				if (!s.cal)
					continue;
				const std::vector<nv::fstep> &re = s.cal->filter_env;
				while (s.rpos < re.size()) {
					if (!re[s.rpos].rel) { s.rpos++; continue; }
					if (u64(s64(s.rel_at + re[s.rpos].at) + EG_LAG) > clock)
						break;
					// **式で出せるときだけ録画を捨てる**。ドラムは要素を持たない
					// ので式が動かない。捨てるとフィルタの包絡線が丸ごと消える
					if (s.elem && ((fenv_on() && re[s.rpos].reg == 0x00)
					               || re[s.rpos].reg == 0x04
					               || (s.vramp && re[s.rpos].reg == 0x0a))) {   // 6.217
						s.rpos++;
						continue;
					}
					u16 v = re[s.rpos].v;
					if (re[s.rpos].reg == 0x0a) {
						s.lfo = v;
						v = lfo_reg(v, *s.cal, s.part, s.keynote);
					} else if (re[s.rpos].reg == 0x00) {
						s.cut = v;
						v = cutoff_reg(v, *s.cal, s.part, s.elem, s.keynote,
						                   s.lcut + assign_cut(s.part, s.keynote));
					} else if (re[s.rpos].reg == 0x05) {
					// **LFO の音量の割り当てを乗せる**（6.199）。
					// 録画はつまみが既定のときの値なので、
					// そのまま流すと割り当てが消える
					v = u16((v & 0xff00)
					      | u16(nv::amod_reg(assign_amod(s.part, s.keynote),
					                        int(v & 0x7f) / 2)));
				} else if (re[s.rpos].reg == 0x04) {
						v = reso_reg(v, *s.cal, s.part);
					}
					m_poke(u32(i) * 64 + re[s.rpos].reg, v);
					s.rpos++;
				}
				while (s.rpos < re.size() && !re[s.rpos].rel)
					s.rpos++;
				if (s.rpos < re.size()) {
					live++;
					if (u64(s64(s.rel_at + re[s.rpos].at) + EG_LAG) < next)
						next = u64(s64(s.rel_at + re[s.rpos].at) + EG_LAG);
				}
				continue;
			}
			live++;
			// **音程の包絡線の段**。チップが行き先に着いていたら次の段を張る
			if (s.pstage < 3 && s.elem && m_peg_peek) {
				while (clock >= s.pnext) {
					if (m_peg_peek(i))
						peg_advance(i);
					s.pnext += FENV_TICK;
					if (s.pstage >= 3)
						break;
				}
				if (s.pstage < 3 && s.pnext < next)
					next = s.pnext;
			}
			// **フィルタの包絡線を式で動かす**（録画の代わり）
			if (fenv_on() && s.elem) {
				bool moved = false;
				while (clock >= s.fnext) {
					fenv_step(s);
					// **フィルタ側の LFO** も同じ 10ms の刻み
					//（6.189）。実機は**足すぶんを先に作ってから
					// 位相を進める**（だから 1 刻み遅れる）。
					// 遅れ（ビブラートの vdly）のあいだは位相も止まる
					lfo_tick(s);
					s.fnext += FENV_TICK;
					moved = true;
				}
				if (moved) {
					s.cut = fenv_cut(s);
					m_poke(u32(i) * 64 + 0x00, cut_with_cc(s, s.cut));
					// **つまみの割り当ての音程もこの刻み**（6.195）
					if (s.pdirty && s.wave) {
						s.pdirty = false;
						m_poke(u32(i) * 64 + 0x11, pitch_of(s));
					}
				}
				if (s.fnext < next)
					next = s.fnext;
			}
			// **サンプル＆ホールドの音程**（6.206）。20ms ごとに刻みを
			// 削って、なくなったら乱数を引いて音程をずらす
			// **遅れて掛かるビブラート**（6.175）。20ms ごとに
			// 遅れを 1 づつ削って、無くなったら深さを 1 歩ずつ上げる
			// 6.217: Vib Depth のぶんも同じ刻みで別にせり上げ、大きいほうを書く（写し取りの録画は使わない。
			// 録画は写し取った音が短いと、せり上がりの途中で切れて、そこで止まっていた）
			auto vib_moving = [&](const slot_use &x) {
				return x.vdly > 0 || x.vcnt < x.vtgt || (x.vramp && x.vcnt2 < nv::VIB_PART_CNT_END);
			};
			if (vib_moving(s)) {
				bool movp = false, mova = false;
				while (clock >= s.vnext && vib_moving(s)) {
					if (s.vdly > 0) {
						s.vdly--;
						if (!s.vdly && s.vamp > 0)
							mova = true;     // 遅れが明けた
					} else {
						// **フィルタ側の LFO が掛かり出すのは、
						// せり上げの 1 歩目と同じ刻み**（6.189）。
						// 遅れが 0 になった刻みではまだ掛からない
						s.lrun = true;
						s.lfdep = s.lfull;
						s.vcnt += s.vstep;
						if (s.vcnt > s.vtgt)
							s.vcnt = s.vtgt;
						s.vcnt2 = std::min(nv::VIB_PART_CNT_END, s.vcnt2 + 5);
						movp = true;
					}
					s.vnext += nv::VIB_TICK;
				}
				if (mova)
					m_poke(u32(i) * 64 + 0x05,
					       u16(0xaa00 | u16(nv::amod_reg(
					           assign_amod(s.part, s.keynote), s.vamp / 2))));
				if (movp) {
					// 音色のぶんと Vib Depth のぶんの大きいほう（6.217）。モジュレーションとの大きいほうは
					// lfo_reg が取る（6.215）。今の値は s.lfo に置く（つまみが動いたときの元になる）
					s.lfo = u16(s.vhi | u16(nv::vib_ramp_value(m_rom, s.vdep, s.vcnt, s.vcnt2)));
					m_poke(u32(i) * 64 + 0x0a, s.cal ? lfo_reg(s.lfo, *s.cal, s.part, s.keynote) : s.lfo);
					if (vib_trace())
						std::printf("VIB %.1f ms  スロット %2d  0a = %04x%c",
						            double(clock - s.tstart) / 44.1, int(i), s.lfo, 10);
					// 実機はこの刻みでも切る高さを作り直す
					//（6.189。位相は進めない）
					if (s.lstep && s.elem && fenv_on()) {
						s.lcut = nv::lfo_fcut(
						    nv::lfo_fwave(s.lph, s.ltri), s.lfdep);
						m_poke(u32(i) * 64 + 0x00,
						       cut_with_cc(s, s.cut));
					}
				}
				if (vib_moving(s) && s.vnext < next)
					next = s.vnext;
			}
			// ポルタメント: 10ms ごとに残りのずれを step だけ 0 へ寄せて、
			// 音程のレジスタを書き直す（6.41）
			if (s.glide && s.elem && s.wave) {
				bool moved = false;
				while (s.glide && s.glide_next <= clock) {
					if (s.glide > 0)
						s.glide = s.glide > s.glide_step ? s.glide - s.glide_step : 0;
					else
						s.glide = -s.glide > s.glide_step ? s.glide + s.glide_step : 0;
					s.glide_next += nv::PORTA_TICK;
					moved = true;
				}
				// **動いたときだけ書く**。前は段の輪が回るたびに書いていて、
				// 1 音の滑りで 0x11 を 26000 回以上書いていた（6.82）
				if (moved)
					m_poke(u32(i) * 64 + 0x11, pitch_of(s));
			}
			if (s.glide && s.glide_next < next)
				next = s.glide_next;
			if (!s.cal || s.tpos >= s.cal->filter_env.size())
				continue;
			const std::vector<nv::fstep> &fe = s.cal->filter_env;
			while (s.tpos < fe.size() && !fe[s.tpos].rel &&
			       u64(s64(s.tstart + fe[s.tpos].at) + EG_LAG) <= clock) {
				u16 v = fe[s.tpos].v;
				// **式で出せるときだけ録画を捨てる**（上の但し書きを見よ）
				if (s.elem && ((fenv_on() && fe[s.tpos].reg == 0x00)
				               || fe[s.tpos].reg == 0x04
				               || (s.vramp && fe[s.tpos].reg == 0x0a))) {   // 6.217
					s.tpos++;
					continue;
				}
				if (fe[s.tpos].reg == 0x0a) {      // 深さにモジュレーションを足す
					s.lfo = v;
					v = lfo_reg(v, *s.cal, s.part, s.keynote);
				} else if (fe[s.tpos].reg == 0x00) {   // 切る高さに明るさを足す
					s.cut = v;
					v = cutoff_reg(v, *s.cal, s.part, s.elem, s.keynote,
						                   s.lcut + assign_cut(s.part, s.keynote));
				} else if (fe[s.tpos].reg == 0x05) {
					// **LFO の音量の割り当てを乗せる**（6.199）。
					// 録画はつまみが既定のときの値なので、
					// そのまま流すと割り当てが消える
					v = u16((v & 0xff00)
					      | u16(nv::amod_reg(assign_amod(s.part, s.keynote),
					                        int(v & 0x7f) / 2)));
				} else if (fe[s.tpos].reg == 0x04) {
					v = reso_reg(v, *s.cal, s.part);
				}
				m_poke(u32(i) * 64 + fe[s.tpos].reg, v);
				s.tpos++;
			}
			// 離しの段に行き当たったら、押してからの並びはそこで終わり
			while (s.tpos < fe.size() && fe[s.tpos].rel)
				s.tpos++;
			if (s.tpos < fe.size() &&
			    u64(s64(s.tstart + fe[s.tpos].at) + EG_LAG) < next)
				next = u64(s64(s.tstart + fe[s.tpos].at) + EG_LAG);
		}
		m_traj = live > 0;
		m_traj_next = next;
	}

	// **その鍵のドラムセットアップの印**（XG の `3n rr nn`）。
	// 音の高さ・音量・パン・送りなどが全部ここに入る。式は起こせていないので、
	// EG のつまみ（6.14）と同じく**値が変わったら写し取り直す**。
	// 組は 4 つあってパートモードで選ばれるが、どれが使われるか見分けるより
	// 4 組ぶん混ぜるほうが確実（1 鍵あたり 44 バイト）
	u32 drum_ctx(int note) const
	{
		if (!m_ram || note < ram::DRUM_SETUP_NOTE0
		    || note >= ram::DRUM_SETUP_NOTE0 + int(ram::DRUM_SETUP_NOTES))
			return 0;
		u32 h = 2166136261u;
		for (int s = 0; s < ram::DRUM_SETUP_SETS; s++)
			// **23 個ぜんぶ混ぜる**。前は 0-10 までしか見ていなかったので、
			// 打ごとのフィルタ（0B・0C）や EG（0D-0F）を動かしても
			// 取り直しが走らず、古い音のままだった
			for (int p = 0; p < int(ram::DRUM_SETUP_PARAM); p++) {
				h ^= m_ram[ram::drum_setup(s, note, p)];
				h *= 16777619u;
			}
		// **マスター音量**（00 00 04）。旋律の声は目盛りに掛け直せるが、
		// ドラムは写し取った減衰をそのまま使う道なので追えない。
		// 印に混ぜて、変わったら取り直させる
		h ^= m_ram[ram::SYS_VOLUME];
		h *= 16777619u;
		return h;
	}

	// ドラムの覚え先の鍵（バンクとプログラムと音の高さ）
	u64 drum_key(int part, int note) const
	{
		if (!m_ram)
			return 0;
		const u8 *p = m_ram + ram::part_base(part);
		return u64(p[1]) << 24 | u64(p[2]) << 16 | u64(p[3]) << 8 | u64(note & 0x7f) |
		       (u64(part_ctx(part) ^ drum_ctx(note)) << 32);
	}
	bool drum_known(int part, int note) const
	{
		return m_drum.find(drum_key(part, note)) != m_drum.end();
	}

	// **音色を自分で決める**（xg::voice_rom::lookup。旋律系のバンク 640 音色で
	// firmware と食い違い 0 だった）。0 を渡すと、またワーク RAM を見る
	void set_record(int part, u32 rec, int drum)
	{
		if (part < 0 || part >= PARTS)
			return;
		m_recsel[part] = rec;
		m_recsel_drum[part] = s8(drum);
		// **音色を替えると firmware がつまみを音色の既定値で上書きする**
		// （XG の決まり）。実測: 曲が CC91=40 を送っていても、そのあとの
		// プログラムチェンジでパートの塊 +0x13 が 33 や 31 になっていた。
		// こちらが CC の生値を握ったままだと、送りの差分が丸ごと狂う。
		// -1 に戻して、firmware が処理し終えたあと sync_cc() で読み直す
		forget_cc(part);
	}

	// **XG のパートの設定（08 pp ll）を自分にも効かせる**。番地はワーク RAM の
	// パートの塊の並びと同じ。ここが無いと、つまみを CC ではなく SysEx で
	// 決める曲で、firmware がその SysEx を処理し終えるまで（native の口では
	// 1 秒以上かかる）古い値のまま鳴ってしまう
	void set_part_param(int part, u8 addr, u8 dd)
	{
		if (part < 0 || part >= PARTS)
			return;
		part_cc &p = m_cc[part];
		// **m_seen は触らない**。ワーク RAM はまだ firmware が書き替えて
		// いないので、ここで「見た」ことにすると、次の同期で古い値を
		// 取り込み直してしまう
		switch (addr) {
		case 0x0b: p.vol = dd; break;
		case 0x0e: p.pan = dd; break;
		case 0x12: p.cho = dd; break;
		case 0x13: p.rev = dd; break;
		case 0x18: p.bri = dd; break;
		case 0x15: p.vrate = dd; break;
		case 0x16: p.vdep = dd; break;
		case 0x17: p.vdly = dd; break;
		case 0x19: p.res = dd; break;
		case 0x1a: p.atk = dd; break;
		case 0x1b: p.dec = dd; break;
		case 0x1c: p.rel = dd; break;
		default: break;
		}
		// **つまみの割り当て**（6.195）。ワーク RAM を読むと
		// native の口では 100ms 遅れるので、こちらでも覚える。
		// XG の番地 0x1D-0x28 はそのまま、0x4D-0x66 は 7 引いた所
		if ((addr >= 0x1d && addr <= 0x28) || (addr >= 0x4d && addr <= 0x66))
			set_assign(part, addr <= 0x28 ? u32(addr) : u32(addr) - 7, dd);
	}

	// **こちらが見ている割り当てのバイト**（6.203）。つまみ 6 つ×
	// 行き先 6 つ（音程・切る高さ・音量・LFO の音程・フィルタ・音量）の 36 を全部
	static constexpr u32 ASN_BASE[6] = {
		0x1d,            // モジュレーション
		0x23,            // ベンド
		0x46,            // チャンネルアフタータッチ
		0x4c,            // PAT
		0x53,            // AC1
		0x5a,            // AC2
	};
	static constexpr int ASN_N = 36;

	static int asn_index(u32 off)
	{
		for (int b = 0; b < 6; b++)
			if (off >= ASN_BASE[b] && off < ASN_BASE[b] + 6)
				return b * 6 + int(off - ASN_BASE[b]);
		return -1;
	}

	int asn_byte(int part, u32 off) const
	{
		if (part >= 0 && part < PARTS) {
			const int k = asn_index(off);
			if (k >= 0 && ((m_asn_have[part] >> k) & 1))
				return int(m_asn[part][size_t(k)]);
		}
		return (m_ram && part >= 0 && part < PARTS)
		     ? int(m_ram[ram::part_base(part) + off]) : 64;
	}

	void set_assign(int part, u32 off, int v)
	{
		if (part < 0 || part >= PARTS)
			return;
		const int k = asn_index(off);
		if (k < 0)
			return;
		m_asn[part][size_t(k)] = u8(v & 0x7f);
		m_asn_have[part] |= u64(1) << k;
		refresh_lfo_depth(part);
		refresh_assign_pitch(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
		refresh_pmod(part);
	}

	// そのパートの「こちらが覚えているつまみ」を捨てて、ワーク RAM から
	// 読み直させる（firmware が書き替えたかもしれないとき）
	void forget_cc(int part)
	{
		if (part < 0 || part >= PARTS)
			return;
		part_cc &p = m_cc[part];
		p.vol = p.expr = p.pan = p.mod = -1;
		p.rev = p.cho = p.bri = p.res = -1;
	}

	// パートの音色の記録。自分で引けていればそれを、そうでなければワーク RAM を読む
	u32 record_of(int part) const
	{
		if (part >= 0 && part < PARTS && m_recsel[part])
			return m_recsel[part];
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u8 *p = m_ram + ram::part_base(part);
		const u32 r = u32(p[ram::PART_VOICE]) << 24 | u32(p[ram::PART_VOICE + 1]) << 16 |
		              u32(p[ram::PART_VOICE + 2]) << 8 | p[ram::PART_VOICE + 3];
		return (r >= 0x200ee0 && r + 16 <= 0x23cece) ? r : 0;
	}


	// ---- コントローラ（doc/native-engine.md の 6.14）
	//
	// これを native 側で持つと、DAW の自動演奏でつまみが動いても SH-2 が起きない。
	// 実機と同じレジスタを、実機と同じ式で書く

	// パートごとの、いまのつまみの位置
	struct part_cc {
		// -1 は「まだ動かされていない＝写し取ったときのまま」
		int vol = -1, expr = -1, pan = -1;     // CC7 / CC11 / CC10
		int mod = -1;                          // CC1（モジュレーション）
		int rev = -1, cho = -1;                // CC91 / CC93（送り）
		int bri = -1, res = -1;                // CC74 / CC71（明るさ・共振）
		// EG のつまみ（CC73 立ち上がり・CC75 減衰・CC72 離し。08 pp 1A/1B/1C）
		int atk = -1, dec = -1, rel = -1;
		// ビブラート（08 pp 15 速さ・16 深さ・17 遅れ ＝ NRPN 01 08/09/0A）
		int vrate = -1, vdep = -1, vdly = -1;
		int var = -1;                          // CC94（バリエーション送り）
		// ポルタメント（CC5 速さ・CC65 入切・CC84 で滑り出す鍵を指定）。
		// last は最後に押した鍵で、つぎの音はここから滑る
		int porta_time = 0, porta_src = -1, last = -1;
		bool porta_on = false;
		// **こちらでさばけない CC が既定から外れている**印（ビットごとに 1 つ）。
		// 立っている間、そのパートの音は firmware に鳴らしてもらう。
		// 黙って無視すると、ポルタメントや EG の設定が効かない音になる
		u32 unknown = 0;
		int bend = 8192, range = 2;            // ピッチベンドと、その幅（半音）
		bool mono = false;                     // CC126 モノ / CC127 ポリ
		bool damper = false;
		bool sost_on = false;          // CC66（ソステヌート）                   // CC64
		bool soft = false;             // CC67（ソフトペダル。6.182）
		// **NRPN の控え**（6.180）。ドラムのセットアップを
		// 「触った」かどうかを知るためだけに見ている
		int nrpn_msb = -1, nrpn_lsb = -1;
		bool rpn_last = false;         // 最後に書いたのが RPN なら true
		// **フィルタ側 LFO の深さに乗るつまみ**（6.191）。
		// 値はこちらで覚える（ワーク RAM を見ると 100ms 遅れる）
		int chpress = 0;               // チャンネルアフタータッチ
		int ac1 = 0, ac2 = 0;          // AC1・AC2 の値
	};

	// firmware を回したあとに、パートの音量・表現・パンをワーク RAM から取り直す。
	// SysEx やパネルで変えられた場合も、これで追い付く
	// ワーク RAM のその値を、こちらの控えに取り込むか決める。
	//
	// 前は「こちらが触っていない（-1）ものだけ拾う」だった。それだと
	// **firmware が裏で書き替えたとき**に気づけない。実際、音色を替えると
	// firmware はパートのつまみを音色の既定値で上書きする（XG の決まり）。
	// 曲が CC91=40 を送っていても、そのあとのプログラムチェンジで
	// パートの塊 +0x13 は 33 になっていた。こちらが 40 を握ったままだと
	// 送りの差分が丸ごと狂う（実測でリバーブ送りが 5 段ずれた）。
	//
	// そこで**前に見た RAM の値**を覚えておき、RAM が動いていたら
	// 「firmware が書き替えた」とみなして取り込む。動いていなければ
	// こちらの値（まだ firmware が処理していない新しい CC）を残す
	void take_ram(int &mine, u8 &seen, u8 now)
	{
		if (mine < 0 || now != seen)
			mine = now;
		seen = now;
	}

	void sync_cc()
	{
		if (!m_ram)
			return;
		for (int p = 0; p < PARTS; p++) {
			const u8 *b = m_ram + ram::part_base(p);
			ram_seen &s = m_seen[p];
			take_ram(m_cc[p].vol,  s.vol,  b[0x0b]);
			take_ram(m_cc[p].expr, s.expr, b[ram::PART_EXP]);
			take_ram(m_cc[p].pan,  s.pan,  b[0x0e]);
			take_ram(m_cc[p].mod,  s.mod,  b[ram::PART_MOD]);
			take_ram(m_cc[p].rev,  s.rev,  b[0x13]);
			take_ram(m_cc[p].cho,  s.cho,  b[0x12]);
			take_ram(m_cc[p].bri,  s.bri,  b[0x18]);
			take_ram(m_cc[p].res,  s.res,  b[0x19]);
			take_ram(m_cc[p].atk,  s.atk,  b[0x1a]);
			take_ram(m_cc[p].dec,  s.dec,  b[0x1b]);
			take_ram(m_cc[p].rel,  s.rel,  b[0x1c]);
			take_ram(m_cc[p].vrate, s.vrate, b[0x15]);
			take_ram(m_cc[p].vdep,  s.vdep,  b[0x16]);
			take_ram(m_cc[p].vdly,  s.vdly,  b[0x17]);
			// ベンド幅（08 pp 23。64 が 0 半音）。RPN でも SysEx でもここに入る
			const int r2 = int(b[0x23]) - 64;
			m_cc[p].range = r2 < 0 ? 0 : (r2 > 24 ? 24 : r2);
		}
	}

	// **パートの「経路」の印**。素通しの量（08 pp 11）・バリエーション送り（14）・
	// パートの EQ（+0x6A-0x6F）・インサーション 4 つの掛かり先を混ぜる。
	// 写し取りはこの経路ごとの値なので、違う経路では使い回せない
	u32 part_ctx(int part) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		u32 h = 2166136261u;
		auto mix = [&h](u8 x) { h ^= x; h *= 16777619u; };
		const u8 *b = m_ram + ram::part_base(part);
		mix(b[0x11]);
		mix(b[0x14]);
		// **ビブラート（08 pp 15 速さ・16 深さ・17 遅れ ＝ CC76・77・78）**。
		// 速さと深さは式が出た（6.162）が、遅れ（+0x17）はまだなので、
		// 写し取りの道では 3 つとも印に混ぜたままにしておく
		mix(b[0x15]);
		mix(b[0x16]);
		mix(b[0x17]);
		// **ノートシフト**（08 pp 08）と**マスター移調**（00 00 06）。
		// 写し取りは移したあとの鍵で取る（波形の番地もその鍵で決まる）ので、
		// 移し方が変わったら取り直す
		mix(b[0x08]);
		if (m_ram)
			mix(m_ram[ram::SYS_TRANSPOSE]);
		// EG のつまみ（CC73 アタック +0x1a・CC75 ディケイ +0x1b・CC72 リリース +0x1c）。
		// この 3 つは式が起こせていない（CC73 は 0x06 だけでなく 0x00・0x07・0x0b も
		// 動かす多目標のつまみだった）。**式の代わりに写し取り直す**：
		// ここに混ぜておくと、つまみが動いた時点で写し取りが別物になり、
		// 次の 1 音だけ firmware が鳴らして取り直す。以後はまた native
		mix(b[0x1a]);
		mix(b[0x1b]);
		mix(b[0x1c]);
		// バリエーション送り（CC94）は口の側で覚えたものを使う
		mix(u8(m_cc[part].var < 0 ? 0 : m_cc[part].var));
		for (int i = 0; i < 6; i++)
			mix(b[ram::PART_EQ_RAM + i]);
		for (int n = 0; n < 4; n++)
			mix(m_ram[ram::INS_BLOCK[n] + 0x0c]);
		return h ? h : 1;
	}

	// 写し取ったときのつまみの位置（ワーク RAM から）
	int part_vol(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + 0x0b]) : 100; }
	int part_expr(int part) const { return m_ram ? int(m_ram[ram::part_base(part) + ram::PART_EXP]) : 127; }
	int part_pan(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + 0x0e]) : 64; }
	int part_mod(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + ram::PART_MOD]) : 0; }
	int part_rev(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + 0x13]) : 40; }
	// パートの共振つまみ（CC71 / 08 pp 19）。こちらが握っていればその値
	int res_knob(int part) const
	{ return m_cc[part].res >= 0 ? m_cc[part].res
	       : (m_ram ? int(m_ram[ram::part_base(part) + 0x19]) : 64); }
	// **そのパートの音量の目盛り**（0-128）。実機はパートの塊 +0x12F に持ち、
	// 音量の目盛りに掛ける（`0x12A4AA`）。中身は
	//
	//   ((音量+1)*(エクスプレッション+1))>>7 に、マスター音量が同じ形で掛かり、
	//   **インサーションを通すとさらに下がる**（LO-FI で 101 -> 80）
	//
	// なので式では作れない。**実機の値を読んで、こちらが動かしたぶんだけ
	// 比で直す**（つまみを動かしても firmware は 100ms 以内に追いつくが、
	// その間も正しい値を出せる）。6.114
	// **つまみの割り当て「音量」の合計**（6.195）。
	// 実機（0x12A700）はベンド・モジュレーション・アフタータッチ・
	// AC1・AC2 を同じ形で足している（PAT はこの組に無い）
	int assign_amp(int part, int note = -1) const
	{
		if (!m_ram || !m_rom || part < 0 || part >= PARTS)
			return 0;
		const u8 *b = m_ram + ram::part_base(part);
		const part_cc &c = m_cc[part];
		int sum = 0;
		sum += nv::amp_assign(m_rom, asn_byte(part, 0x1f),
		                      c.mod >= 0 ? c.mod : int(b[ram::PART_MOD]));
		sum += nv::amp_assign(m_rom, asn_byte(part, 0x48), c.chpress);
		sum += nv::amp_assign(m_rom, asn_byte(part, 0x55), c.ac1);
		sum += nv::amp_assign(m_rom, asn_byte(part, 0x5c), c.ac2);
		// **ベンドは中央からの離れを 64 で割った値**（6.200。実機 0x12A6F6）。
		// 値の符号は呼ぶ側、深さの符号は表の側が持つ
		const int bv = (c.bend - 0x2000) >> 6;
		if (bv) {
			const int q = nv::amp_assign(m_rom, asn_byte(part, 0x25),
			                             bv < 0 ? -bv : bv);
			sum += bv < 0 ? -q : q;
		}
		// **PAT だけは鍵ごと**（実機 0x12A4D4。鍵は 36-97 だけ）
		if (note >= 36 && note < 98)
			sum += nv::amp_assign(m_rom, asn_byte(part, 0x4e),
			                      int(m_pat[size_t(part)][size_t(note)]));
		return sum;
	}

	// **つまみの割り当て「切る高さ」**（6.196。実機 0x1281BA）。
	//
	//   足すぶん = ((深さ - 64) × 値) >> 2
	//
	// ベンドだけは中央からの離れを使って `(深さ - 64) × 離れ >> 8`。
	// **PAT だけは鍵ごと**
	int assign_cut(int part, int note) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u8 *b = m_ram + ram::part_base(part);
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) {
			return (d == 64 || !v) ? 0 : ((d - 64) * v) >> 2;
		};
		int sum = 0;
		sum += term(asn_byte(part, 0x1e),
		            c.mod >= 0 ? c.mod : int(b[ram::PART_MOD]));
		sum += term(asn_byte(part, 0x47), c.chpress);
		sum += term(asn_byte(part, 0x54), c.ac1);
		sum += term(asn_byte(part, 0x5b), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, 0x4d),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = asn_byte(part, 0x24);
		if (pb != 64) {
			const int v = c.bend - 0x2000;
			if (v)
				sum += ((pb - 64) * v) >> 8;
		}
		return sum;
	}

	int vol_gain_of(int part, int vol, int expr) const
	{
		int g = nv::vol_gain(vol, expr);
		if (m_ram) {
			g = (g * (int(m_ram[ram::SYS_VOLUME]) + 1)) >> 7;
			// RAM の値と、RAM のつまみから作った値の比で直す
			const int g_ram = int(m_ram[ram::part_base(part) + ram::PART_GAIN]);
			int g_calc = nv::vol_gain(part_vol(part), part_expr(part));
			g_calc = (g_calc * (int(m_ram[ram::SYS_VOLUME]) + 1)) >> 7;
			if (g_calc > 0)
				g = g * g_ram / g_calc;
		}
		// **つまみの割り当てはここでは足さない**（6.200）。実機は音量を
		// 掛けたあとの目盛りの索引の側に足す（volume_att_from の `add`）
		return g < 0 ? 0 : (g > 128 ? 128 : g);
	}

	// **ベロシティ感度**（08 pp 0C 深さ・0D ずらし）を掛けた強さ
	int part_vel(int part, int vel) const
	{
		if (!m_ram)
			return vel;
		const u8 *b = m_ram + ram::part_base(part);
		return nv::vel_sense(vel, int(b[0x0c]), int(b[0x0d]));
	}

	// **ノートシフト**（08 pp 08。64 が 0 半音、±24 まで）。実機は鍵を移して
	// から音色を選ぶので、要素の鍵域も波形の選び方も移した鍵で決まる
	// **スケールチューニング**（6.127）。音名ごとに音程をずらす
	// （XG の 08 pp 41-4C。ワーク RAM では +0x3A から 12 個、64 が 0 セント）
	int part_scale_cents(int part, int note) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u32 i = u32(((note % 12) + 12) % 12);
		return int(m_ram[ram::part_base(part) + ram::PART_SCALE_RAM + i]) - 64;
	}

	// **マスターチューン**（6.136）。全部のパートに効く。
	// 00 00 00-03 の 4 バイトの下 4bit をつないだ 12bit（0x400 が 0 セント、
	// 1 きざみ 0.1 セント）
	int master_tune_tenths() const
	{
		if (!m_ram)
			return 0;
		const u8 *b = m_ram + ram::SYS_TUNE;
		const int v = ((b[0] & 0xf) << 12) | ((b[1] & 0xf) << 8)
		            | ((b[2] & 0xf) << 4) | (b[3] & 0xf);
		return (v & 0xfff) - 0x400;
	}

	// **RPN 1（微調）**（6.125）。パートの塊 +0xCC に「14bit の値 − 8192」が
	// 入る（8192 で 100 セント）。音程のレジスタにだけ出る
	int part_fine_cents(int part) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u8 *b = m_ram + ram::part_base(part);
		const int fine = int(s16(u16(u16(b[ram::PART_FINE]) << 8 | b[ram::PART_FINE + 1])));
		// マスターチューン（全部のパートに効く）も一緒に足す
		return fine * 100 / 8192 + master_tune_tenths() / 10;
	}

	int part_shift(int part) const
	{
		if (!m_ram)
			return 0;
		// 実機（`0x128D46`）は
		//   鍵 + (パートの塊[8] - 64) + (マスター移調 - 64) + パートの塊[0xC9]
		// を 0-127 に収める。`0x128D60` が読むのは `0x4226C7` ＝ SYSTEM + 6。
		// 最後の `パートの塊[0xC9]` が何なのかはまだ分かっていないので入れて
		// いない（既定では 0 のはずだが、確かめていない）
		int v = int(m_ram[ram::part_base(part) + 0x08]) - 64;
		v += int(m_ram[ram::SYS_TRANSPOSE]) - 64;
		// **+0xC9 は RPN 2（粗調）**（6.125）。実機の式に入っているのに
		// 何なのか分からず外していた。RPN 2 を送ると符号つきの半音がここに入る
		v += int(s8(m_ram[ram::part_base(part) + ram::PART_COARSE]));
		return v;
	}
	int part_cho(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + 0x12]) : 0; }

	// **パートの EQ**を式で入れる（6.181）。写し取りのときは
	// 写した値（`d.iir`）のままで、こちらは使わない
	void apply_part_eq(nv::slot_regs &r, int part) const
	{
		if (!m_ram || !m_rom || part < 0 || part >= PARTS)
			return;
		const u8 *b = m_ram + ram::part_base(part);
		nv::eq_set(m_rom, r, int(b[ram::PART_EQ_LGAIN]), int(b[ram::PART_EQ_HGAIN]),
		           int(b[ram::PART_EQ_LFREQ]), int(b[ram::PART_EQ_HFREQ]));
	}
	// **ドラムの EQ はパートの EQ ではなく打ごとの EQ（3n rr 20・21・24・25）**。
	// 実機はパートの EQ（08 pp 72-77）をドラムに掛けない（firmware で確かめた。
	// パートの低音 +12dB でもレジスタは既定のまま、打の低音 +12dB だと動く）
	void apply_drum_eq(nv::slot_regs &r, int part, int note) const
	{
		if (!m_rom)
			return;
		nv::eq_set(m_rom, r, drum_live(part, note, 0x20), drum_live(part, note, 0x21),
		           drum_live(part, note, 0x24), drum_live(part, note, 0x25));
	}
	// **ドラムの HPF はパートの HPF と打ごとの HPF（3n rr 50）の足し算**
	// （パート 100 ＋ 打 84 が、どちらか一方の 120 と同じレジスタになった）
	int drum_hpf(int part, int note) const { return part_hpf(part) + drum_live(part, note, 0x50) - 64; }
	int part_bri(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + 0x18]) : 64; }
	// パートの塊の 1 バイト（無ければ 64）
	int part_ram(int part, u32 off) const { return m_ram ? int(m_ram[ram::part_base(part) + off]) : 64; }
	// パートの HPF（0A pp 20。64 が音色のまま）
	int part_hpf(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + ram::PART_HPF_RAM]) : 64; }
	int part_res(int part) const  { return m_ram ? int(m_ram[ram::part_base(part) + 0x19]) : 64; }

	// ---- つまみの割り当て（doc/native-engine.md の 6.43）
	//
	// XG の「モジュレーション・ベンド・アフタータッチ・AC1・AC2 が音の何を
	// どれだけ動かすか」は、パートの塊に**6 つ組**（音程・フィルタ・音量・
	// LFO の PMOD/FMOD/AMOD）で並んでいる。位置は `nativeplay --xgmap` で
	// XG のアドレスを 1 つずつ書いて見つけた（08 pp 4D → +0x46 など）。
	//
	// **既定のままなら、そのつまみは SWP30 のレジスタを 1 つも動かさない**
	// （`nativeplay --at` で確かめた）。だから既定のあいだは firmware に
	// 任せる必要がない。既定から外れているときだけ任せる
	static constexpr u32 MW_BLOCK  = 0x1d;   // モジュレーション（CC1）
	static constexpr u32 PB_BLOCK  = 0x23;   // ベンド（+0x23 は幅なので別扱い）
	static constexpr u32 AT_BLOCK  = 0x46;   // アフタータッチ（08 pp 4D-52）
	static constexpr u32 PAT_BLOCK = 0x4c;   // 鍵ごとのアフタータッチ
	static constexpr u32 AC1_NUM   = 0x52;   // AC1 の CC 番号（既定 16）
	static constexpr u32 AC1_BLOCK = 0x53;
	static constexpr u32 AC2_NUM   = 0x59;   // AC2 の CC 番号（既定 17）
	static constexpr u32 AC2_BLOCK = 0x5a;

	// その 6 つ組が既定（＝音に何も起きない）か。既定は 64,64,64,0,0,0。
	//
	// **5 番目（LFO のフィルタ変調の深さ）だけはこちらで鳴らせる**
	//（6.192）。そこは 6.191 で式が分かったので、それだけが
	// 既定から外れているなら firmware に渡さなくてよい
	bool assign_idle(int part, u32 off, bool mine = false) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return false;                  // 分からないときは任せる側に倒す
		const u8 *b = m_ram + ram::part_base(part) + off;
		// **6 つ組は全部こちらで鳴らせる**（6.191-6.199）。
		// 音程・切る高さ・音量・LFO の音程・LFO のフィルタ・LFO の音量
		if (mine)
			return true;
		(void)b;
		return b[0] == 64 && b[1] == 64 && b[2] == 64 && !b[3] && !b[4] && !b[5];
	}

	// モジュレーションの割り当ては既定が 64,64,64,**10**,0,0（LFO の音程が 10）。
	// ここが動いていると、こちらの CC1 の式（6.14 の 10 段の表）が合わない。
	// **フィルタ変調の深さ（5 番目）だけはこちらで鳴らせる**（6.192）
	bool mod_idle(int part) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return false;
		const u8 *b = m_ram + ram::part_base(part) + MW_BLOCK;
		// **6 つ組は全部こちらで鳴らせる**（6.199）
		(void)b;
		return true;
	}

	// ベンドは +0x23 が幅（RPN で普通に動く。こちらも読んでいる）なので、
	// 音程以外の 5 つだけを見る
	bool bend_idle(int part) const
	{
		// **ベンドの 6 つ組も全部こちらで鳴らせる**（6.201）。
		// 音程は `pitch_of`、切る高さは `assign_cut`、音量は
		// `assign_amp`、LFO の 3 つは `assign_pmod` / `lfo_fdep_extra` /
		// `assign_amod` が見ている
		(void)part;
		return true;
	}

	// アフタータッチ（触れた強さ）。**割り当てが既定なら音に何も起きない**
	void aftertouch(int part, bool poly)
	{
		if (part < 0 || part >= PARTS)
			return;
		const u32 bit = poly ? 30u : 31u;
		if (assign_idle(part, poly ? PAT_BLOCK : AT_BLOCK, true))
			m_cc[part].unknown &= ~(1u << bit);
		else
			m_cc[part].unknown |= 1u << bit;
	}

	// AC1・AC2（好きな CC を割り当てられるつまみ）。番号が合っていて割り当てが
	// 既定から外れていれば、native では何も起きないので firmware に任せる
	void assignable(int part, int cc, int value)
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return;
		const u8 *pb = m_ram + ram::part_base(part);
		const u32 num[2] = { AC1_NUM, AC2_NUM };
		const u32 blk[2] = { AC1_BLOCK, AC2_BLOCK };
		for (int k = 0; k < 2; k++) {
			if (cc != int(pb[num[k]]))
				continue;
			// **値も覚える**（6.191）。フィルタ側 LFO の深さに乗る
			(k ? m_cc[part].ac2 : m_cc[part].ac1) = value & 0x7f;
			refresh_lfo_depth(part);
			refresh_assign_pitch(part);
			refresh_assign_amp(part);
			refresh_assign_amod(part);
			const u32 bit = k ? 28u : 29u;
			if (value && !assign_idle(part, blk[k], true))
				m_cc[part].unknown |= 1u << bit;
			else
				m_cc[part].unknown &= ~(1u << bit);
		}
	}


	// その CC を native でさばけるか（実際にさばく前に決める）
	static bool handles_cc(int cc)
	{
		return cc == 0x07 || cc == 0x0b || cc == 0x0a || cc == 0x40 || cc == 0x01 ||
		       cc == 0x49 || cc == 0x4b || cc == 0x48 ||      // EG（73・75・72。6.157）
		       cc == 0x5b || cc == 0x5d || cc == 0x4a || cc == 0x47 ||
		       cc == 0x05 || cc == 0x41 || cc == 0x54 ||
		       cc == 0x7e || cc == 0x7f || cc == 0x79;
	}

	// CC を受ける。native でさばけたら true（firmware にも短く回す）
	bool control(int part, int cc, int value)
	{
		if (part < 0 || part >= PARTS)
			return false;
		part_cc &p = m_cc[part];
		switch (cc) {
		case 0x07: p.vol = value; break;
		case 0x0b: p.expr = value; break;
		case 0x0a: p.pan = value; break;
		case 0x49: p.atk = value; break;       // CC73 立ち上がり（6.157）
		case 0x4b: p.dec = value; break;       // CC75 減衰
		case 0x48: p.rel = value; break;       // CC72 離し
		case 0x01:
			p.mod = value;
			// モジュレーションの割り当てが動いていると、こちらの式が合わない
			if (value && !mod_idle(part))
				p.unknown |= 1u << 27;
			else
				p.unknown &= ~(1u << 27);
			// **割り当てのぶんを鳴っている音に効かせる**（6.199）。
			// 0x0a は apply_cc が書き直すので、残りをここで
			refresh_lfo_depth(part);
			refresh_assign_pitch(part);
			refresh_assign_amp(part);
			refresh_assign_amod(part);
			break;
		case 0x5b: p.rev = value; break;
		case 0x5d: p.cho = value; break;
		case 0x4a: p.bri = value; break;
		case 0x47: p.res = value; break;
		// **モノ / ポリ**（6.125）。モノのパートは、つぎの鍵を押すと
		// 前の音を**離す**（実機は古いスロットへ離しの `0x09` を書く）。
		// 入れるまでは前の音が鳴り続けて、重なったぶん 0.8dB 大きかった
		// **モノ・ポリの切り替えも鍵を離す**（6.208）。MIDI の決めで
		// CC124-127 はどれもオールノートオフを兼ねる。
		// 実機で確かめたところ、4 つとも CC123 と同じだけ音が止まる
		case 0x7e: p.mono = true; all_off(part); return true;
		case 0x7f: p.mono = false; all_off(part); return true;
		case 0x05: p.porta_time = value; return true;     // ポルタメントの速さ
		case 0x41: p.porta_on = value >= 64; return true; // ポルタメント 入切
		case 0x54: p.porta_src = value & 0x7f; return true;   // 滑り出す鍵を指定
		case 0x40:                             // ダンパー
		{
			const bool was = p.damper;
			p.damper = value >= 64;
			if (!p.damper)
				release_held(part);
			else if (!was)
				damper_catch(part);    // 離し中の音を拾う（6.164）
			return true;
		}
		case 0x42:                             // ソステヌート
			// ダンパーと違って、**踏んだ時点で鳴っている音だけ**を待たせる。
			// あとから押した鍵は普通に離れる
			if (value >= 64) {
				p.sost_on = true;
				for (int i = 0; i < SLOTS; i++) {
					slot_use &s2 = m_slot[i];
					if (s2.on && s2.part == part)
						s2.sost = true;
				}
			} else {
				p.sost_on = false;
				release_sost(part);
			}
			return true;
		// **CC121 コントローラリセット**（6.126）。ベンド・モジュレーション・
		// エクスプレッション・ダンパー・ポルタメントを既定へ戻す。
		// 音量とパンと送りは**戻らない**（XG も MIDI もそう決まっている）。
		// 入れるまではベンドが残って、リセット後の音が半音ずれていた
		case 0x79:
			p.bend = 8192;
			p.mod = 0;
			p.expr = 127;
			p.damper = false;
			p.sost_on = false;
			p.porta_on = false;
			p.porta_src = -1;
			p.unknown &= ~((1u << 26) | (1u << 27));
			release_held(part);
			release_sost(part);
			apply_bend(part);
			apply_cc(part);
			return false;
		// **CC67 ソフトペダル**（6.182）。踏むと、このあと押す音の
		// フィルタの包絡線が「強さ - 32」の深さになる。
		// 鳴っている音はそのまま（実機も書き直さない）
		case 0x43:
			p.soft = value >= 64;
			return false;
		// **NRPN を控える**（6.180）。ドラムのセットアップを
		// 「触った」かどうかがワーク RAM から見えないので、
		// こちらで MIDI を見て印を立てる
		case 0x63: p.nrpn_msb = value; p.rpn_last = false; return false;
		case 0x62: p.nrpn_lsb = value; p.rpn_last = false; return false;
		case 0x65:
		case 0x64: p.rpn_last = true; return false;
		case 0x06:
			if (!p.rpn_last && p.nrpn_lsb >= 0) {
				const int a = drum_nrpn_addr(p.nrpn_msb);
				if (a >= 0)
					mark_drum_setup(drum_set_of(part), p.nrpn_lsb, a, value);
				// **NRPN 17 は減衰 1 と減衰 2 を両方動かす**（firmware で確かめた。
				// SysEx の 0E は減衰 1 だけ、0F は減衰 2 だけ）
				if (a == 0x0e)
					mark_drum_setup(drum_set_of(part), p.nrpn_lsb, 0x0f, value);
			}
			return false;
		case 0x78:                             // CC120 オールサウンドオフ
			all_off(part, true);
			return false;
		case 0x7b:                             // CC123 オールノートオフ
			all_off(part);
			return false;
		case 0x7c:                             // CC124 オムニオフ
		case 0x7d:                             // CC125 オムニオン
			all_off(part);
			return false;
		default: {
			// バリエーション送り。ワーク RAM には出てこない（掛かり先が
			// パートに繋がっていないと firmware が何も書かない）ので、
			// **口の側で覚えて経路の印に混ぜる**。送りの値は写し取った
			// ミキサのレジスタに入っているので、値が変われば取り直せばよい
			if (cc == 0x5e) {
				p.var = value;
				return false;                  // firmware にも見せる（写し取りのため）
			}
			// 知らない CC は AC1・AC2 に割り当てられているかもしれない
			assignable(part, cc, value);
			return false;                      // 知らない CC は firmware に任せる
		}
		}
		apply_cc(part);
		return true;
	}

	void bend(int part, int value14)
	{
		if (part < 0 || part >= PARTS)
			return;
		m_cc[part].bend = value14;
		// （下で格子に乗せて流す）
		// ベンドの割り当て（音程以外）が動いていると、こちらの式が合わない
		if (value14 != 8192 && !bend_idle(part))
			m_cc[part].unknown |= 1u << 26;
		else
			m_cc[part].unknown &= ~(1u << 26);
		// **ベンドは実機の 10ms 格子で効く**（6.125）。firmware は CC を
		// 受けた瞬間ではなく、つぎの 10ms 割り込みでレジスタを書き直す
		// （実測で `0x11` の書き込みがいつも位相 304 に乗る）。こちらは
		// その場で書いていたので、大きく曲げる曲で 5ms ぶん先走っていた
		bend_assign(part);          // 割り当てはその場で（6.201）
		if (m_eg_have && bend_grid()) {
			const u64 due = eg_after(m_clock);
			m_bend_due[part] = due;
			if (due < m_bend_next)
				m_bend_next = due;
		} else {
			apply_bend(part);
		}
	}

	static bool bend_grid()
	{
		static const bool on = [] {
			const char *e = std::getenv("SMU2000_BEND_GRID");
			return !e || (e[0] != '0' || e[1]);
		}();
		return on;
	}

	void set_bend_range(int part, int semitones)
	{
		if (part >= 0 && part < PARTS)
			m_cc[part].range = semitones;
	}

	void reset_cc(int part)
	{
		if (part >= 0 && part < PARTS)
			m_cc[part] = part_cc();
	}

	const part_cc &cc_of(int part) const { return m_cc[part]; }

private:
	// **離した音を追ってよいか**（6.201）。
	// 実機は声が鳴り終わった時点でスロットを空け、以後は
	// 何も書かない。こちらだけが書き続けると、鳴り続けている
	// 尾にベンドや LFO が掛かって実機と全く違う音になる
	bool rel_follow(const slot_use &s, u64 now = ~u64(0)) const
	{
		if (s.on)
			return true;
		if (now == ~u64(0))
			now = m_clock;
		if (!s.rel || now - s.rel_at > REL_FOLLOW)
			return false;
		const int ch = int(&s - m_slot.data());
		// 実機がスロットを空けたら、そこで書くのをやめる（6.207）
		if (m_slot_held && slotfree_on() && !m_slot_held(ch))
			return false;
		if (!m_slot_peek)
			return true;
		return m_slot_peek(ch);
	}

	// いま鳴っているスロットに、つまみの動きを反映する
	void apply_cc(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.cal)
				continue;
			// **離しの最中の音も追う**。0x09 の下位は「素の減衰」で、
			// 坂の位置（swp30 の m_envelope_level）とは別に持たれている
			// （swp30.cpp の envelope_block: 出る値は level + (glo & 0xff) << 6）。
			// つまり書き直しても坂は引き直しにならないので、安心して追える。
			// 追わないと、曲の終わりの CC7 のフェードアウトで離したばかりの
			// 長い音だけが元の音量のまま鳴り続ける
			if (!s.on) {
				// **ドラムには離しの段が無い**（要素を持たない）ので、
				// 追わずにそのまま鳴らしきらせる（6.139）
				if (!s.elem || !rel_follow(s))
					continue;
				m_poke(u32(i) * 64 + 9, release_of(s, part, s.keynote));
				continue;
			}
			m_poke(u32(i) * 64 + 9, u16(note_att(s, part)));
			if (s.cal->has(0x32))
				m_poke(u32(i) * 64 + 0x32,
				       s.cal->synth && ins_routed(part) ? u16(0)
				       : (s.rnd_pan < 0 && s.cal->synth ? exact_pan(s, part)
				                                        : pan_reg(*s.cal, part, s.rnd_pan, s.base32)));
			if (s.lfo)
				m_poke(u32(i) * 64 + 0x0a,
				       lfo_reg(s.lfo, *s.cal, part, s.keynote));
			if (s.cal->has(0x33))
				m_poke(u32(i) * 64 + 0x33,
				       s.cal->synth
				       ? exact_send(s, part, false, s.base33)
				       : send_reg(*s.cal, 0x33, false, m_cc[part].rev, s.cal->cal_rev,
				                  s.rnd_drop, s.base33));
			if (s.cal->has(0x34))
				m_poke(u32(i) * 64 + 0x34,
				       s.cal->synth
				       ? (ins_routed(part)
				          ? u16((exact_send(s, part, true, s.base34) & 0xff00) | 0x10)
				          : exact_send(s, part, true, s.base34))
				       : send_reg(*s.cal, 0x34, true, m_cc[part].cho, s.cal->cal_cho,
				                  s.rnd_drop, s.base34));
			if (s.cut)
				m_poke(u32(i) * 64 + 0x00,
				       cutoff_reg(s.cut, *s.cal, part, s.elem, s.keynote));
			if (s.cal->has(0x04))
				m_poke(u32(i) * 64 + 0x04,
				       s.cal->synth && s.elem
				       ? u16(u16(nv::reso_level(s.elem, s.fvel, res_knob(part))) << 11)
				       : reso_reg(s.cal->reg[0x04], *s.cal, part));
		}
	}

	// **包絡線の段を 1 つ進める**（実機の 0x128766）。
	// 累算を目標にきっちり合わせてから、つぎの段の目標と増分を決める
	void fenv_next(slot_use &s)
	{
		const u8 *e = s.elem;
		if (!e || !m_rom) {
			s.finc = 0;
			return;
		}
		s.facc = s.ftgt;
		if (s.fstage >= 9) {             // 離しの段は進めない
			s.finc = 0;
			return;
		}
		s.fstage++;
		const int adj = s.fadj;
		int rate = -1, lvl = -1;
		if (s.fstage == 1) {
			if (e[55] != e[56]) { rate = int(e[51]) + adj; lvl = e[56]; }
			else                  s.fstage = 2;
		}
		if (rate < 0 && s.fstage == 2) {
			// **段 2 の速さには EG ディケイ（CC75）が掛かる**（6.205）。
			// 実機は段 1 には 0x40 を渡していて、そこだけ掛からない
			if (e[56] != e[57]) {
				rate = nv::fenv_rate_cc(m_rom, int(e[52]), m_cc[s.part].dec)
				     + adj;
				lvl = e[57];
			} else {
				s.fstage = 3;
			}
		}
		if (rate < 0) {              // もう段が無い
			s.finc = 0;
			return;
		}
		if (rate < 0) rate = 0;
		if (rate > 63) rate = 63;
		s.ftgt = nv::fenv_target(m_rom, e, lvl, s.fdvel);
		s.finc = nv::fenv_inc(m_rom, rate);
		// 下る向きなら増分の符号を反転する（実機の 0x128BA4）
		if (s.facc > s.ftgt && s.finc != nv::FENV_NEXT)
			s.finc = -s.finc;
	}

	// 鍵を押したときに包絡線を張る
	void fenv_start(slot_use &s, int vel)
	{
		if (!s.elem || !m_rom)
			return;
		s.fvel = vel;
		s.fdvel = nv::soft_vel(vel, m_cc[s.part].soft);
		const int kadj = nv::fenv_key_adj(s.elem, s.keynote);
		s.fadj = kadj + nv::fenv_vel_adj(s.elem, vel);
		s.ftgt = nv::fenv_target(m_rom, s.elem, s.elem[55], s.fdvel);
		// **立ち上がりの段**。byte50 が 63（即到達）なら段 0 の行き先から
		// 始まり、そうでなければ byte54 から byte50 の速さで登る（6.71）。
		// **立ち上がりのつまみでこの速さも動く**（6.171）。実機は
		// 「目盛り 63 以上」を**鍵の補正を足す前と足したあとの 2 回**見る
		const int atk = m_cc[s.part].atk;
		const int a50 = nv::fenv_atk_rate(m_rom, s.elem, atk);
		s.facc = nv::cut_exact()
		       ? nv::fenv_init(m_rom, s.elem, s.fdvel, atk, s.keynote) : s.ftgt;
		s.finc = 0;
		s.fstage = 0;
		// **押鍵のときにもう段 0 の行き先に居るか**。
		// ここで段を進めた音は、押した直後の 1 目で値が動かない
		const bool at_tgt = (s.facc == s.ftgt);
		if (at_tgt) {
			fenv_next(s);
		} else if (nv::fenv_atk_instant(m_rom, s.elem, atk, s.keynote)) {
			s.finc = nv::FENV_NEXT;
		} else {
			int rate = a50 + s.fadj;
			rate = rate < 0 ? 0 : (rate > 62 ? 62 : rate);
			s.finc = nv::fenv_inc(m_rom, rate);
			if (s.facc > s.ftgt && s.finc != nv::FENV_NEXT)
				s.finc = -s.finc;
		}
		// **格子の目は録画の 1 段目から取る**。録画の時刻は firmware が
		// 実際に書いた時刻なので、そこが格子の目そのもの。
		// 位相を別に測るより、これがいちばん近い（実測で確かめた）
		u32 at0 = FENV_TICK;
		if (s.cal)
		for (const nv::fstep &e : s.cal->filter_env)
			if (e.reg == 0x00 && !e.rel) { at0 = e.at; break; }
		// **実機の 10ms 割り込みは世界共通**（6.118）。鍵を押したあと最初に
		// 来る目が 1 目め。写し取りの at0 は写し取った音の鍵からの相対なので、
		// そのまま足すと鍵ごとに位相がずれる（実機の位相は 304、こちらは
		// 曲ごとに 86-308 とばらばらだった）。位相をまだ学べていない間だけ at0 を使う。
		//
		// **1 目飛ばすのは、押鍵のときにもう行き先に居る音だけ**（6.177）。
		// GrandPno（byte50 = 63 で即到達）は実機も 1 目めで値を動かさないが、
		// Crystal（byte50 = 62）は 1 目めから動く。一律に飛ばしていたので、
		// そういう音色だけ包絡線が丸ごと 10ms 遅れていた
		const u32 skip = at_tgt ? FENV_TICK : 0;
		// **遅れて鳴る要素は、その要素が鳴り出してから数える**
		//（6.189）。レジスタは要素をまとめて先に書くが、実機の
		// 10ms 刻みは**その要素が実際に打たれてから**始まる。
		// 書いた時刻から数えると、1 目ぶん早く進んでいた
		const u64 dly = s.elem ? nv::elem_delay(s.elem) : 0;
		s.fnext = (m_eg_have && eg_grid())
		        ? eg_after(u64(s64(s.tstart + dly) + EG_LAG)) + skip
		        : u64(s64(s.tstart + dly + at0 + skip) + EG_LAG);
	}

	// **サンプル＆ホールドの音程の1 刻み**（6.206）。
	// 20ms ごとに刻みを削って、なくなったら乱数を引いて音程をずらす。
	// **押している間も離しの間も同じに回る**（止めると乱数の列がずれる）
	bool sh_tick(int i, u64 clock, u64 &next)
	{
		slot_use &s = m_slot[i];
		if (s.sh_cnt <= 0 || !s.elem)
			return false;
		bool drew = false;
		while (clock >= s.sh_next) {
			if (--s.sh_cnt <= 0) {
				s.sh_off = nv::sh_pitch_off(rnd_next(), s.elem);
				s.sh_cnt = nv::sh_ticks(s.elem);
				drew = true;
			}
			s.sh_next += 2 * FENV_TICK;
		}
		if (drew && s.wave)
			m_poke(u32(i) * 64 + 0x11, pitch_of(s));
		if (s.sh_next < next)
			next = s.sh_next;
		return true;
	}

	// **フィルタ側 LFO の 1 刻み**（6.189・6.192）。押しているあいだも
	// 離しのあいだも同じに回る。入れ忘れていたときは、離したとたん
	// 揺れが止まって、切る高さが古いぶんだけずれたまま固まっていた
	void lfo_tick(slot_use &s)
	{
		if (!s.lstep)
			return;
		s.lcut = nv::lfo_fcut(nv::lfo_fwave(s.lph, s.ltri), s.lfdep);
		// **位相が進むのは深さが 0 でないあいだだけ**。
		// 実機（0x1298D0）は深さが 0 なら位相を進める係を呼ばない
		if (s.lfdep)
			s.lph = nv::lfo_next(s.lph, s.lstep);
	}

	// **離しの段**。鍵を離すと、実機はもう 1 段張って 0 へ向かう。
	// 速さは byte53、行き先は byte58（段 1 が byte51/byte56、
	// 段 2 が byte52/byte57 と並んでいるので、その次）。
	// 実測（GrandPno）で増分 -28 ＝ INC_TAB[13]、byte53(13) と一致
	void fenv_release(slot_use &s)
	{
		const u8 *e = s.elem;
		if (!e || !m_rom || !fenv_on())
			return;
		// **離しの速さには EG リリース（CC72）が掛かる**（6.205）
		int rate = nv::fenv_rate_cc(m_rom, int(e[53]), m_cc[s.part].rel)
		         + s.fadj;
		if (rate < 0) rate = 0;
		if (rate > 63) rate = 63;
		s.fstage = 9;                    // もう段を進めない印
		s.ftgt = nv::fenv_target(m_rom, e, e[58], s.fdvel);
		s.finc = nv::fenv_inc(m_rom, rate);
		if (s.facc > s.ftgt && s.finc != nv::FENV_NEXT)
			s.finc = -s.finc;
	}

	// 10ms ぶん進める
	void fenv_step(slot_use &s)
	{
		if (s.finc == nv::FENV_NEXT) {
			fenv_next(s);
			return;
		}
		if (!s.finc)
			return;
		s.facc += s.finc;
		if ((s.finc > 0 && s.facc >= s.ftgt) || (s.finc < 0 && s.facc <= s.ftgt))
			fenv_next(s);
	}

	// いまの切る高さ（写し取った鍵を押した時点の値を基準に、包絡線の差ぶんを足す）
	// 写し取りがあれば CC74 の差ぶん、無ければ 64 からの差ぶんを乗せる
	u16 cut_with_cc(const slot_use &s, u16 base) const
	{
		// **式で出した値には、写し取りとの差を重ねてはいけない**（6.123）。
		// `cutoff_of` は鍵の追従（`cutoff_key_curve`）をもう含んでいるのに、
		// `cutoff_reg` は「いまの鍵 − 写し取った鍵」ぶんをさらに足すので、
		// **鍵の追従が二重に掛かる**。Rain の第 2 要素は鍵 60・84 で
		// 0x100 ぶん明るくなっていた（鍵 36 で写し取るので、そこだけ合う）。
		// 明るさ（CC71）は写し取りとの差ではなく、そのまま足す
		const int add = s.lcut + assign_cut(s.part, s.keynote);
		if (s.cal && !nv::cut_exact())
			return cutoff_reg(base, *s.cal, s.part, s.elem, s.keynote, add);
		return cut_plain(base, s.part, s.elem, s.fvel, add);
	}

	// 式で出した `0x00` に、明るさ（CC71）だけを足す
	// **明るさのつまみは頭打ちの前に効く**（6.167）。`base` は頭打ちを
	// 掛けていない値を渡すこと
	u16 cut_plain(u16 base, int part, const u8 *elem, int vel,
	              int lfo = 0) const
	{
		const int now = m_cc[part].bri;
		int v = int(base & 0xfff);
		if (now >= 0 && now != 64)
			v += nv::bright_shift(now);
		// **頭は 0x7ff**（6.212）。実機は0x800 の下騄を履いたまま
		// 0xfff で止めるので、こちらでは0x7ff で止めるのと同じ。
		// 0xfff で止めていたので、明るさを上げて共振も深いと（`cutoff_cap` が
		// 通らない）`0xa3f` のような 12 ビットの値を書いて、音が丸ごと壊れていた
		v = v < 0 ? 0 : (v > 0x7ff ? 0x7ff : v);
		// **LFO の揺れはここ**（6.189）。実機（0x127E82）も
		// 鍵の追従を足して頭打ちしたあと、頭打ちの前に足す。
		//
		// **頭は 0x7ff**。実機は 0x800 の下駄を履いたまま
		// 0xfff で止めてから `& 0x7ff` で面だけ取るので、
		// こちらでは 0x7ff で止めるのと同じ。これをしないと
		// TubulBel（もう 0x7ff）で 0x802 を書いてしまい、
		// 上の面が変わって音が丸ごと壊れた。
		// 下は 1（実機は 0x800 以下になると 1 にする）
		if (lfo) {
			v += lfo;
			v = v <= 0 ? 1 : (v > 0x7ff ? 0x7ff : v);
		}
		return nv::cutoff_cap(u16((base & 0xf000) | u16(v)), elem, vel,
		                      res_knob(part));
	}

	u16 fenv_cut(const slot_use &s) const
	{
		// 式だけで出す道（写し取りが無いときは必ずこちら）。
		// **頭打ちは掛けずに返す**（明るさのつまみのあとで掛ける。6.167）
		if (!s.cal || nv::cut_exact())
			return nv::cutoff_of(m_rom, s.elem, s.keynote, s.fvel, s.facc, false);
		const u16 base = s.cal->reg[0x00];
		const int init = nv::fenv_target(m_rom, s.elem, s.elem[55], s.fvel) >> 2;
		int v = int(base & 0xfff) - init + (s.facc >> 2);
		v = v < 0 ? 0 : (v > 0xfff ? 0xfff : v);
		return u16((base & 0xf000) | u16(v));
	}

	// そのスロットの、いまの音程レジスタ（ベンドと滑りの残りを入れて作る）
	// **つまみの割り当て「音程」**（6.195）。実機（0x12BE70）は
	//
	//   セント = (値 × (深さ - 64) × 0xC947) >> 16
	//
	// を足す。`0xC947 / 65536 = 0.78624` なので、値が 127 のとき
	// ほぼ「(深さ - 64) × 100 セント」＝XG の ±24 半音になる。
	// **PAT だけは鍵ごと**（パートの塊の +0x82 から 62 鍵分。6.191）
	int assign_cents(int part, int note) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u8 *b = m_ram + ram::part_base(part);
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) {
			if (d == 64 || !v)
				return 0;
			return int(s16(u16((u32(v) * u32(d - 64) * 0xc947u) >> 16)));
		};
		int sum = 0;
		sum += term(asn_byte(part, MW_BLOCK),
		            c.mod >= 0 ? c.mod : int(b[ram::PART_MOD]));
		sum += term(asn_byte(part, AT_BLOCK), c.chpress);
		sum += term(asn_byte(part, AC1_BLOCK), c.ac1);
		sum += term(asn_byte(part, AC2_BLOCK), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, PAT_BLOCK),
			            int(m_pat[size_t(part)][size_t(note)]));
		return sum;
	}

	u16 pitch_of(const slot_use &s) const
	{
		const part_cc &pc = m_cc[s.part];
		return nv::pitch_reg(nv::read_wave(s.wave), s.note, nv::key_follow(m_rom, s.elem),
		                     nv::bend_cents(pc.bend, pc.range) + nv::elem_tune(s.elem)
		                     + part_fine_cents(s.part)
		                     + part_scale_cents(s.part, s.note) + nv::glide_cents(s.glide)
		                     + assign_cents(s.part, s.keynote) + s.sh_off,
		                     nv::key_pivot(s.elem));
	}

	void apply_bend(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem || !s.wave)
				continue;
			// **離している音も曲げる**（6.201）。戻す時刻と
			// 離す時刻が重なると、曲がったまま鳴り続けていた
			if (!rel_follow(s))
				continue;
			m_poke(u32(i) * 64 + 0x11, pitch_of(s));
		}
	}

	// **ベンドの割り当て（音程以外）はその場で**（6.201）。
	// 実機は音程だけを 10ms の格子に乗せて、こちらはすぐに書く。
	// 切る高さはフィルタの 10ms の刻みが毎回 `assign_cut` を乗せるのでここには無い
	void bend_assign(int part)
	{
		if (bend_asn_plain(part))
			return;
		refresh_lfo_depth(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
		refresh_pmod(part);
	}

	// つまみが動いたら、鳴っている音の**LFO の音程の深さ**を書き直す（6.201）。
	// CC は `apply_cc` がやっているので、ここはベンド専用
	void refresh_pmod(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.cal || !s.lfo)
				continue;
			if (!rel_follow(s))
				continue;
			m_poke(u32(i) * 64 + 0x0a,
			       lfo_reg(s.lfo, *s.cal, part, s.keynote));
		}
	}

	// ベンドの割り当て（音程以外の 5 つ）が全部既定か
	bool bend_asn_plain(int part) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return true;
		const u8 *b = m_ram + ram::part_base(part) + PB_BLOCK;
		return b[1] == 64 && b[2] == 64 && !b[3] && !b[4] && !b[5];
	}

	// モノのパートで、いま鳴っている別の鍵を離す。
	// **付け替えの離しは 0xD9**（キーアサインがシングルのときと同じ。6.208）。
	// 音色の離しの速さを使っていたので、弦のような尾の長い
	// 音色でこちらだけ前の鍵が鳴り続けていた（48・0・80・10・56 の
	// 5 音色で確かめて、どれも 0xD9）
	void mono_cut(int part, int except)
	{
		int keys[SLOTS];
		int n = 0;
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (!s.on || s.part != part || s.keynote == except)
				continue;
			s.single_cut = true;
			bool seen = false;
			for (int k = 0; k < n; k++)
				if (keys[k] == s.keynote)
					seen = true;
			if (!seen)
				keys[n++] = s.keynote;
		}
		for (int k = 0; k < n; k++)
			note_off(part, keys[k], true);
	}

	// 実機は 0xD9 で離してから **1010 サンプル**（23ms）後に 0xF0 を書く。
	// 4 回の開閉でどれも同じ間隔だった
	static constexpr u64 ALT_KILL_DELAY = 1010;

	// 同じオルタネートグループで鳴っている打を止める（6.151）
	int alt_cut(int part, int except, int grp)
	{
		int wrote = 0;
		int keys[SLOTS];
		int n = 0;
		for (int i = 0; i < SLOTS; i++) {
			const slot_use &s = m_slot[i];
			if (s.part != part || s.keynote == except)
				continue;
			// 離してから長い音は追わない（実機も書かない。6.199）
			if (!rel_follow(s))
				continue;
			if (s.alt_kill)
				continue;
			// **鳴り終わった打は切らない**。実機もそうで、切ると書き込みの
			// ぶん打鍵が 1 サンプル遅れてしまう（6.151）
			if (m_slot_peek && !m_slot_peek(i))
				continue;
			if (drum_alt_group(part, s.keynote) != grp)
				continue;
			bool seen = false;
			for (int k = 0; k < n; k++)
				if (keys[k] == s.keynote)
					seen = true;
			if (!seen)
				keys[n++] = s.keynote;
		}
		for (int k = 0; k < n; k++)
			for (int i = 0; i < SLOTS; i++) {
				slot_use &s = m_slot[i];
				if (s.part != part || s.keynote != keys[k] || (!s.on && !s.rel))
					continue;
				if (s.alt_kill)        // もう切ってある打は二度切らない
					continue;
				if (m_slot_peek && !m_slot_peek(i))
					continue;
				s.alt_kill = m_clock + ALT_KILL_DELAY;
				if (s.alt_kill < m_alt_kill_next)
					m_alt_kill_next = s.alt_kill;
				s.single_cut = true;
				s.on = false;
				s.rel = true;
				s.rel_at = m_clock;
				// **ドラムは要素を持たない**ので、普通の離しの式は使えない。
				// 速さだけ与えて、音量はそのときの値にする
				m_poke(u32(i) * 64 + 9,
				       s.elem ? release_of(s, part, s.keynote)
				              : u16(SINGLE_CUT_RATE | u16(note_att(s, part) & 0xff)));
				wrote++;
			}
		return wrote;
	}

	// ダンパーを離したとき、待たせていた音を切る
	void release_held(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.on && s.held && s.part == part) {
				s.held = false;
				note_off(part, s.keynote, true);
			} else if (s.caught && s.part == part && !s.on && s.rel && s.elem) {
				// **ペダルで拾っていた音を離し直す**（6.164）
				s.caught = false;
				m_poke(u32(i) * 64 + 9, release_of(s, part, s.keynote));
			}
		}
	}

	// ソステヌートを離したとき、待たせていた音を切る
	void release_sost(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.sost && s.part == part) {
				s.sost = false;
				if (s.on)
					note_off(part, s.keynote);
			}
		}
	}

	// そのスロットの、いまのつまみでの減衰。
	// **掛けてから一度だけ減衰に直す**（実機の `0x12A4AA`。6.101）。
	// 触られていない側は写し取ったときの値のまま
	int note_att(const slot_use &s, int part) const
	{
		const part_cc &p = m_cc[part];
		const nv::voice_cal *c = s.cal;
		const int asn = assign_amp(part, s.keynote);
		if (s.sfx && s.lvl0 == 0)
			return 0xfe;                 // 打の音量で目盛りが 0（sfx_level）
		if (!m_rom || (p.vol < 0 && p.expr < 0 && !asn))
			return nv::clamp_att(s.att);
		const int vol  = p.vol  >= 0 ? p.vol  : (c ? c->cal_vol  : 100);
		const int expr = p.expr >= 0 ? p.expr : (c ? c->cal_expr : 127);
		if (s.lvl0 > 0)
			return nv::clamp_att(nv::volume_att_from(m_rom, s.lvl0, s.arest,
			                                         vol_gain_of(part, vol, expr),
			                                         asn));
		// **ドラムには目盛りが無い**（要素を持たず、写し取った減衰をそのまま
		// 使う道）。そこは今までどおり、減衰の差ぶんで動かす
		int a = s.att;
		if (c) {
			a += nv::gain_att(m_rom, nv::vol_gain(vol, expr))
			   - nv::gain_att(m_rom, nv::vol_gain(c->cal_vol, c->cal_expr));
		}
		return nv::clamp_att(a);
	}

	// フィルタのレジスタ。下 12bit が切る高さで、明るさ（CC74）のぶんをずらす
	// elem と note を渡すのは、**鍵による切る高さのずれ**を入れるため。
	// 写し取りは音色あたり 1 音なので、写した鍵と違う鍵ではここがずれる
	// （利用者の曲で、食い違いの大半がこれだった）
	u16 cutoff_reg(u16 base, const nv::voice_cal &c, int part,
	               const u8 *elem = nullptr, int note = -1, int lfo = 0) const
	{
		const int now = m_cc[part].bri;
		int d = 0;
		if (elem && note >= 0)
			d = nv::cutoff_key_curve(m_rom, elem, note)
			  - nv::cutoff_key_curve(m_rom, elem, c.cal_note);
		if (d == 0 && lfo == 0 && (now < 0 || now == c.cal_bri))
			return base;
		int v = int(base & 0xfff) + d + lfo;
		if (now >= 0)
			v += nv::bright_shift(now) - nv::bright_shift(c.cal_bri);
		v = v < 0 ? 0 : (v > nv::CUTOFF_MAX ? nv::CUTOFF_MAX : v);
		return u16((base & 0xf000) | u16(v));
	}

	// 共振のレジスタ。上 5bit が共振で、CC71 のぶんをずらす
	u16 reso_reg(u16 base, const nv::voice_cal &c, int part) const
	{
		const int now = m_cc[part].res;
		if (now < 0 || now == c.cal_res)
			return base;
		int v = int(base >> 11) + nv::reso_shift(now) - nv::reso_shift(c.cal_res);
		v = v < 0 ? 0 : (v > 31 ? 31 : v);
		return u16((base & 0x07ff) | u16(v << 11));
	}

	// 送りのレジスタ。下位が減衰で、写し取ったときからの差ぶんだけ動かす。
	// 写し取ったときに切れていた（0xff）送りは差が取れないので、
	// **もう一方の送りから下駄を借りる**（どちらもパートの同じ下駄に乗っている）
	// 0x32-0x37 は 1 つで 2 本ぶんの送りを持つ。リバーブは 0x33 の**下位**、
	// コーラスは 0x34 の**上位**（nativeplay --ccwatch で確かめた）
	// `drop` は**パンの Rnd で目減りするぶん**（6.147）。Rnd のときは送りの
	// 表を位置 0 で引くので、写し取ったとき（音色の持つパンの位置）のぶんだけ減る
	u16 send_reg(const nv::voice_cal &c, int which, bool hi, int now, int was,
	             int drop, u16 base) const
	{
		if (drop == 0 && (now < 0 || now == was))
			return base;
		const int cur = hi ? (base >> 8) : (base & 0xff);
		// 写し取ったときに切れていた（0xff）送りは差が取れない。
		// 下駄は 16（CC91=127・CC93=127 のどちらも 16 になる）
		const int v = (now < 0 || now == was)
		            ? cur - drop
		            : ((cur >= 0xff && was <= 0)
		               ? 16 + nv::send_att(m_rom, now) - drop
		               : cur + nv::send_att(m_rom, now) - nv::send_att(m_rom, was) - drop);
		const int w = nv::clamp_att(v);
		return u16(hi ? ((w << 8) | (base & 0xff)) : ((base & 0xff00) | w));
	}

	// **つまみの割り当て「LFO の音程」の合計**（6.198）。
	// 実機（0x12A034）はベンド・モジュレーション・AT・AC1・AC2 を
	// `値 × 深さ / 128` で足し、そこに**鍵ごとの PAT**を加える
	// `bend_now` に 0x2000 を渡すと、ベンドを中央に置いたときの合計を返す
	//（6.201。`lfo_reg` の「素の値」用）
	int assign_pmod(int part, int note, int mod_now, int bend_now = -1) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) { return (d && v) ? (d * v) / 128 : 0; };
		int sum = 0;
		sum += term(asn_byte(part, 0x20), mod_now < 0 ? 0 : mod_now);
		sum += term(asn_byte(part, 0x49), c.chpress);
		sum += term(asn_byte(part, 0x56), c.ac1);
		sum += term(asn_byte(part, 0x5d), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, 0x4f),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = asn_byte(part, 0x26);
		if (pb) {
			int v = bend_now < 0 ? c.bend : bend_now;
			if (v < 0)
				v += 63;
			v = (v >> 6) - 128;
			if (v)
				sum += (v < 0 ? -v : v) * pb / 128;
		}
		return sum;
	}

	// **つまみの割り当て「LFO の音量」の合計**（6.199）。
	// どのつまみも `値 × 深さ / 1024`。PAT だけは鍵ごと
	int assign_amod(int part, int note) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u8 *b = m_ram + ram::part_base(part);
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) { return (d && v) ? (d * v) / 1024 : 0; };
		int sum = 0;
		sum += term(asn_byte(part, 0x22),
		            c.mod >= 0 ? c.mod : int(b[ram::PART_MOD]));
		sum += term(asn_byte(part, 0x4b), c.chpress);
		sum += term(asn_byte(part, 0x58), c.ac1);
		sum += term(asn_byte(part, 0x5f), c.ac2);
		if (note >= 36 && note < 98)
			sum += term(asn_byte(part, 0x51),
			            int(m_pat[size_t(part)][size_t(note)]));
		// **ベンドだけ分母が 256**（6.201。実機 0x129F40）。中央からの
		// 離れを 256 で割った ±32 を、符号を捨てて深さと掛ける
		const int pb = s8(u8(asn_byte(part, 0x28)));
		if (pb) {
			int v = c.bend - 0x2000;
			if (v < 0)
				v += 0xff;
			v >>= 8;
			if (v < 0)
				v = -v;
			sum += (pb * v) / 256;
		}
		return sum;
	}

	// つまみが動いたら、鳴っている音の**LFO の音量**を書き直す（6.199）
	void refresh_assign_amod(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem)
				continue;
			// 離してから長い音は追わない（実機も書かない。6.199）
			if (!rel_follow(s))
				continue;
			const int base = s.lrun ? (s.vamp / 2) : 0;
			m_poke(u32(i) * 64 + 0x05,
			       u16(0xaa00 | u16(nv::amod_reg(assign_amod(part, s.keynote),
			                                     base))));
		}
	}

	// LFO のレジスタ。下位が深さで、つまみのぶん（6.198）と音色自身の深さの**大きいほう**（6.215）。
	// 実機は 表[max(つまみの合計の頭打ち, 音色自身の目盛り)] で、足さない。表（VIB_REG_TAB）は
	// 単調なので、レジスタの値どうしの max と同じ。base は音色自身の値（ビブラートのつまみ・
	// 遅れてせり上がる分を含む）。写し取りの値がちょうどつまみのぶんなら、音色自身は
	// それ以下で分からないので 0 と見る（写し取りをつまみを上げたまま取ったときだけ）
	u16 lfo_reg(u16 base, const nv::voice_cal &c, int part, int note = 60) const
	{
		if (!m_rom)
			return base;
		const int now = m_cc[part].mod;
		const int sum = assign_pmod(part, note, now < 0 ? c.cal_mod : now);
		const int was = nv::pmod_reg(m_rom, assign_pmod(part, note, c.cal_mod, 0x2000));
		int own = int(base & 0xff);
		if (was > 0 && own == was)
			own = 0;
		const int wheel = nv::pmod_reg(m_rom, sum);
		return u16((base & 0xff00) | (own > wheel ? own : wheel));
	}

public:
	// **液晶のメーター**（doc/native-engine.md の 6.148）。実機はパートごとに
	// 「いちばん大きい音の目盛り」を持っていて、演奏画面がそれを棒にして描く。
	// native の口では firmware が音を持たないので、そこがずっと 0 になり
	// **メーターが動かない**（利用者からの報告）。鳴らしている音から作り直す。
	//
	// 離したあとも少しの間は残す（打楽器のような短い音でも、25ms おきの
	// 見回りで拾えるように）
	static constexpr u64 METER_TAIL = 44100 / 4;

	// **フィルタ側 LFO の深さに乗るつまみの合計**（6.191）。
	// 実機は 0x129E30（パートごとの控え）と 0x129C00（鍵ごとの PAT）。
	// どれも **深さ × 値 / 1024**。ベンドだけは中央からの離れを
	// 256 で割ってから乗せる（向きは見ない）。
	//
	// **深さはワーク RAM から読む**（割り当ては曲の途中では稀なので
	// 100ms 遅れても痛くない）が、**値はこちらの控え**を使う
	int lfo_fdep_extra(int part, int note) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return 0;
		const u8 *b = m_ram + ram::part_base(part);
		const part_cc &c = m_cc[part];
		auto term = [](int d, int v) { return (d && v) ? (d * v) / 1024 : 0; };
		int sum = 0;
		sum += term(s8(u8(asn_byte(part, 0x21))),
		            c.mod >= 0 ? c.mod : int(b[ram::PART_MOD]));
		sum += term(s8(u8(asn_byte(part, 0x4a))), c.chpress);
		sum += term(s8(u8(asn_byte(part, 0x57))), c.ac1);
		sum += term(s8(u8(asn_byte(part, 0x5e))), c.ac2);
		// **鍵ごとの PAT**。実機はパートの塊の +0x82 から 62 鍵分
		//（鍵 36-97）を持っていて、+0x50 の深さと掛ける
		if (note >= 36 && note < 98)
			sum += term(s8(u8(asn_byte(part, 0x50))),
			            int(m_pat[size_t(part)][size_t(note)]));
		const int pb = s8(u8(asn_byte(part, 0x27)));
		if (pb) {
			int v = c.bend - 0x2000;
			if (v < 0)
				v += 0xff;
			v >>= 8;
			if (v < 0)
				v = -v;
			sum += (pb * v) / 256;
		}
		return sum;
	}

	// つまみが動いたら、鳴っている音の**音量**を書き直す（6.195）
	void refresh_assign_amp(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem)
				continue;
			if (s.on)
				m_poke(u32(i) * 64 + 9, u16(note_att(s, part)));
			else if (rel_follow(s))
				m_poke(u32(i) * 64 + 9, release_of(s, part, s.keynote));
		}
	}

	// つまみが動いたら、鳴っている音の**音程**を書き直す（6.195）
	void refresh_assign_pitch(int part)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || !s.elem || !s.wave)
				continue;
			// **離した音も追う**（6.200）。押している音だけにすると
			// 割り当てを戻すところの残差が 16.9% → 47.9% に悪くなった。
			// 実機も離しの最中は音程を見直しているらしい
			if (!rel_follow(s))
				continue;
			s.pdirty = true;
			m_traj = true;
			m_traj_next = 0;
		}
	}

	// つまみが動いたら、鳴っている音の深さを作り直す
	void refresh_lfo_depth(int part)
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return;
		for (slot_use &s : m_slot) {
			if (s.part != part || !s.elem || !s.lstep)
				continue;
			// 離してから長い音は追わない（実機も書かない。6.199）
			if (!rel_follow(s))
				continue;
			s.lfull = nv::lfo_fdepth(m_rom, s.elem,
			                         lfo_fdep_extra(part, s.note));
			if (s.lrun)
				s.lfdep = s.lfull;
		}
	}

	// **鍵ごとのアフタータッチ**（6.191）。値を覚えて、
	// 鳴っている音のフィルタ側 LFO の深さを作り直す
	void poly_at(int part, int note, int value)
	{
		if (part < 0 || part >= PARTS || note < 0 || note > 127)
			return;
		m_pat[size_t(part)][size_t(note)] = u8(value & 0x7f);
		refresh_lfo_depth(part);
		refresh_assign_pitch(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
		// **調べ用**（`SMU2000_PAT_DBG=1`）。索引に乗るぶんと深さを出す
		if (std::getenv("SMU2000_PAT_DBG"))
			std::fprintf(stderr, "PAT part=%d note=%d v=%d extra=%d depth=%02x\n",
			             part, note, value, lfo_fdep_extra(part, note),
			             m_ram ? int(m_ram[ram::part_base(part) + 0x50]) : -1);
	}

	// **チャンネルアフタータッチ**（6.191）
	void chan_press(int part, int value)
	{
		if (part < 0 || part >= PARTS)
			return;
		m_cc[part].chpress = value & 0x7f;
		refresh_lfo_depth(part);
		refresh_assign_pitch(part);
		refresh_assign_amp(part);
		refresh_assign_amod(part);
	}

	// **そのパートをその強さで鳴らしたときの目盛り**（6.188）。
	// firmware が持っている音（写し取りの 1 音目）にも使う
	int part_meter(int part, int vel) const { return meter_of(part, vel); }

	void fill_meter(u8 *dst, int n) const
	{
		for (int i = 0; i < n; i++)
			dst[i] = 0;
		if (!m_rom)
			return;
		for (const slot_use &s : m_slot) {
			if (s.part < 0 || s.part >= n || s.vel <= 0)
				continue;
			if (!s.on && !(s.rel && m_clock - s.rel_at < METER_TAIL))
				continue;
			const int v = meter_of(s.part, s.vel);
			if (v > int(dst[s.part]))
				dst[s.part] = u8(v);
		}
	}

private:
	// 目盛り = 強さ x パートの目盛り / 128。
	// **パートの目盛りはワーク RAM から取る**（PART_GAIN。音量・
	// エクスプレッション・マスター音量・インサーションの損まで畳んである。
	// 6.114）。実機との差は 1 以内（実測 15 通り）
	int meter_of(int part, int vel) const
	{
		if (!m_ram)
			return 0;
		const int g = int(m_ram[ram::part_base(part) + ram::PART_GAIN]) - 1;
		if (g <= 0)
			return 0;
		const int v = (vel * g) >> 7;
		return v > 127 ? 127 : v;
	}

	// **Rnd（パン 0）かどうか**。パートのパンの値がそのまま 0 のとき
	bool pan_is_rnd(int part) const { return m_cc[part].pan == 0; }

	// **Rnd の乱数を 1 つ進める**（6.147）。種はワーク RAM にあって、
	// 実機の firmware と同じ場所・同じ式なので、実機モードと行き来しても
	// 列が途切れない。要素 1 つにつき 1 回進む
	// **乱数を 1 つ進める**。種はワーク RAM にあって、実機（0x12B06C）と
	// 同じ式なので、実機モードと行き来しても列が途切れない。
	// **引く回数と順が実機と同じで無いと値がずれる**
	int rnd_next()
	{
		if (!m_ramw || !m_ram)
			return 128;
		u8 &x = m_ramw[ram::PAN_RND];
		x = u8(0xb3 * x + 0x11);
		return int(x);
	}

	// パンの Rnd（6.147）。要素 1 つにつき 1 回進む
	int pan_rnd_draw() { return rnd_next() >> 1; }

	// パンのレジスタ（写し取った値からの差ぶんで動かす）
	// **そのパートはインサーションを通るか**（6.161）。バリエーションを
	// インサーションとして使っている場合と、インサーション 1-4 の掛かり先。
	// 通るパートは、スロットのミキサ（0x32・0x34-0x37）が丸ごと別の値になる
	bool ins_routed(int part) const { return ins_target(part) >= 0; }

	// **どのエフェクトに挿さっているか**。0 がバリエーション（インサーション接続）、
	// 1-4 がインサーション 1-4、-1 は通らない
	int ins_target(int part) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return -1;
		if (m_ram[ram::VAR_BLOCK + ram::VAR_CONNECT] == 0
		    && int(m_ram[ram::VAR_BLOCK + ram::VAR_PART]) == part)
			return 0;
		for (int n = 0; n < 4; n++)
			if (int(m_ram[ram::INS_BLOCK[n] + ram::INS_PART]) == part)
				return n + 1;
		return -1;
	}

	// インサーションを通るときのミキサ。**実機の値をそのまま置く**（6.161）。
	// 0x35-0x37 は**挿さっている先で違う**（3 つとも同じ値）。前はどれも 0x4000 に
	// していたが、それはバリエーションの値で、インサーション 1-4 では音が
	// 挿した先へ行かず、別の出口へ出ていた。firmware に 5 通り鳴らさせて測った
	// （マスタの声。スレーブの声は write_slot で置き換える）
	void ins_mixer(nv::slot_regs &r, int pan_pos, int target) const
	{
		static const u16 ROUTE[5] = { 0x4000, 0x1000, 0x0001, 0x0002, 0x0004 };
		// **パンは音色（打）自身の分だけ残る**（6.184）。
		// 真ん中の音色なら 0 なので、lofi・ins2 では見えていなかった
		r.set(0x32, nv::ins_pan_reg(m_rom, pan_pos));
		r.set(0x34, u16((r.v[0x34] & 0xff00) | 0x10));
		const u16 v = ROUTE[target < 0 || target > 4 ? 0 : target];
		r.set(0x35, v);
		r.set(0x36, v);
		r.set(0x37, v);
	}

	// **スレーブの声の出口（0x35-0x37）**。スレーブの声は自分の DAC には出ず、
	// MELO の線でマスタのミキサへ入るので、同じ行き先でも値が違う。firmware が
	// 同じ音をマスタとスレーブで鳴らしたときの値（素通し・バリエーション・
	// インサーション 1-4。パートの Dry Level では変わらない）。
	// 前はマスタの値のまま書いていて、スレーブに置いた声は**どこにも出ていなかった**。
	// **書き込みの最後（mu2000 の m_poke）で置き換える**。途中の write_slot で
	// 置き換えていたときは、自分で書くドラムの道が漏れていて、インサーションを掛けた
	// ドラムがスレーブに置かれると別のインサーションへ流れていた
	static u16 slave_route(u16 master)
	{
		switch (master) {
		case 0x4d00: return 0x000f;       // 素通し
		case 0x4800: return 0x000c;
		case 0x4400: return 0x000a;
		case 0x4000: return 0x0008;       // バリエーション
		case 0x1000: return 0x0010;       // インサーション 1
		case 0x0001: return 0x1000;       // インサーション 2
		case 0x0002: return 0x2000;       // インサーション 3
		case 0x0004: return 0x4000;       // インサーション 4
		default:     return master;
		}
	}

	// **合成の写しのときは、つまみを織り込んだ値をその場で組み直す**（6.154）。
	// 写し取りが無いので「基準からの差」ではなく絶対値で出す。
	// パンは `PAN_BASE[CC10] + PAN_CURVE[音色（打）のパン]`（6.155）
	u16 exact_pan(const slot_use &s, int part) const
	{
		const int q = m_cc[part].pan < 0 ? 64 : m_cc[part].pan;
		if (s.elem && s.sfx)
			return nv::voice_pan_reg(m_rom, s.elem, s.note, sfx_pan(part, s.keynote), q);
		if (s.elem)
			return nv::voice_pan_reg(m_rom, s.elem, s.note, 64, q);
		const int dp = drum_setup_of(part, s.keynote, 0x04);
		return nv::drum_pan_reg(m_rom, dp < 0 ? 64 : dp, q);
	}

	// 送り（0x33・0x34）。上位はそのまま、下位を組み直す
	u16 exact_send(const slot_use &s, int part, bool cho, u16 base) const
	{
		const int now = cho ? m_cc[part].cho : m_cc[part].rev;
		int extra = 127;
		int pan = 64;
		if (s.elem && s.sfx) {
			const int d = drum_setup_of(part, s.keynote, cho ? 0x06 : 0x05);
			extra = d < 0 ? 127 : d;
			pan = nv::voice_pan_pos(m_rom, s.elem, s.note, sfx_pan(part, s.keynote));
		} else if (s.elem) {
			pan = nv::voice_pan_pos(m_rom, s.elem, s.note);
		} else {
			const int d = drum_setup_of(part, s.keynote, cho ? 0x06 : 0x05);
			const int dp = drum_setup_of(part, s.keynote, 0x04);
			extra = d < 0 ? 127 : d;
			pan = dp < 0 ? 64 : dp;
		}
		// **パンが Rnd のときは、送りの表を位置 0 で引く**（6.147）。
		// 当たった位置ではなく 0 なので、音色（打）の寄りは効かない
		if (s.rnd_pan >= 0)
			pan = 0;
		// **リバーブは 0x33 の下位、コーラスは 0x34 の上位**（`send_reg` と同じ）。
		// ここを下位で書いていたので、コーラスを使う曲で送りが丸ごと狂っていた
		const int v = nv::send_level_att(m_rom, now < 0 ? (cho ? 0 : 40) : now,
		                                 extra, pan);
		return cho ? u16(u16(v) << 8 | (base & 0x00ff))
		           : u16((base & 0xff00) | u16(v));
	}

	u16 pan_reg(const nv::voice_cal &c, int part, int rnd, u16 base) const
	{
		// Rnd のときは**音色のパンの寄りを無視して**、当たった位置そのもの
		// （実機もそうしている。6.147）
		if (rnd >= 0)
			return nv::pan_rnd_reg(m_rom, rnd);
		const int now = m_cc[part].pan, was = c.cal_pan;
		if (now < 0 || now == was)
			return base;
		const int l = nv::clamp_att((base >> 8) + nv::pan_att(m_rom, now) - nv::pan_att(m_rom, was));
		const int r = nv::clamp_att((base & 0xff) + nv::pan_att(m_rom, 128 - now)
		                            - nv::pan_att(m_rom, 128 - was));
		return u16(l << 8 | r);
	}

public:
	// そのパートの音色をもう写し取ってあるか（CC を firmware にどれだけ
	// 見せるかの目安。まだなら 1 音目は firmware が鳴らすので、CC も効かせてもらう）
	bool part_learned(int part) const
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return false;
		if (is_drum(part))
			return !m_drum.empty();
		const u32 rec = record_of(part);
		return rec && m_cal.find(cal_key(rec, part)) != m_cal.end();
	}

	// そのパートは firmware に任せきりか（知らない CC が効いている）。
	// このパートでは写し取りをしても使い道が無いので、やらない
	bool delegated(int part) const
	{ return part >= 0 && part < PARTS && m_cc[part].unknown != 0; }

	// **キーアサインがシングルか**（XG の 08 pp 06。0 がシングル、1 がマルチ）
	bool key_assign_single(int part) const
	{
		return m_ram && part >= 0 && part < PARTS
		    && m_ram[ram::part_base(part) + 0x06] == 0;
	}

	// そのパートが使うドラムの組（0-3）。パートモードから決まる
	int drum_set_of(int part) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return -1;
		const int mode = int(m_ram[ram::part_base(part) + 0x07]);
		const int set = mode >= 2 ? mode - 2 : 0;
		return set < ram::DRUM_SETUP_SETS ? set : -1;
	}

	// **打ごとのセットアップの今の値**（切る高さ・共振・EG・EQ・HPF。`addr` は 3n rr pp の pp）。
	// MIDI（SysEx・NRPN）で書かれた値の控えを先に見て、無ければワーク RAM。
	// native の口では firmware を 100ms につき 5ms しか回さないので、打つ時点では
	// RAM がまだ古いことがある（6.180）。
	// **実機は SysEx で書いても NRPN で書いても次の打から変わる**（2026-09-28 に
	// firmware で確かめた。6.180 の「SysEx では計算し直さない」は番号の取り違えから出た誤り）
	int drum_live(int part, int note, int addr) const
	{
		const int set = drum_set_of(part);
		const int idx = ram::drum_setup_index(addr);
		if (set < 0 || note < 0 || note > 127 || idx < 0)
			return idx < 0 ? 64 : ram::drum_setup_default(idx);
		if ((m_drum_touch[size_t(set)][size_t(note)] >> idx) & 1)
			return int(m_drum_val[size_t(set)][size_t(note)][size_t(idx)]);
		if (!m_ram || note < ram::DRUM_SETUP_NOTE0
		    || note >= ram::DRUM_SETUP_NOTE0 + int(ram::DRUM_SETUP_NOTES))
			return ram::drum_setup_default(idx);
		return int(m_ram[ram::drum_setup(set, note, idx)]);
	}

	// **ドラムのセットアップを触った**（3n rr pp の SysEx と、
	// NRPN 14-1F）。`set` は 3n の n、`addr` は pp
	void mark_drum_setup(int set, int note, int addr, int value)
	{
		mark_drum_setup_index(set, note, ram::drum_setup_index(addr), value);
	}
	// 同じく、ワーク RAM の並びの番号（0-22）で
	void mark_drum_setup_index(int set, int note, int idx, int value)
	{
		if (set < 0 || set >= ram::DRUM_SETUP_SETS || note < 0 || note > 127
		    || idx < 0 || idx >= int(ram::DRUM_SETUP_PARAM))
			return;
		m_drum_touch[size_t(set)][size_t(note)] |= 1u << idx;
		m_drum_val[size_t(set)][size_t(note)][size_t(idx)] = u8(value & 0x7f);
	}

	// ---- **SFX の打のドラムセットアップ**（6.236）。firmware で 0-127 を振って合わせた
	// 要素のバイトに足す: 切る高さの索引（byte37）に値 − 64、共振（byte35）に (値 − 64) >> 1、
	// 立ち上がり・減衰 1・減衰 2 の速さ（byte73・74・75）に値 − 64
	void sfx_setup_elem(int part, int note, u8 *e) const
	{
		nv::sfx_setup_apply(e, drum_live(part, note, 0x0b), drum_live(part, note, 0x0c),
		                    drum_live(part, note, 0x0d), drum_live(part, note, 0x0e),
		                    drum_live(part, note, 0x0f));
	}
	// 粗調（3n rr 00）は**鍵をずらす**。組む鍵は 64 のままなので、要素の追従
	// （SFX の音色はほとんど追従しない。SFXKit1 の鍵 36 は 1 半音で 5 セント）ぶんだけ動く
	int sfx_pitch_note(int part, int note, int pnote) const
	{
		const int co = drum_setup_of(part, note, 0x00);
		return std::clamp(pnote + (co < 0 ? 0 : co - 64), 0, 127);
	}
	// 打のパン（3n rr 04）。音色のパンに CC10 の代わりに足す
	int sfx_pan(int part, int note) const
	{
		const int dp = drum_setup_of(part, note, 0x04);
		return dp <= 0 ? 64 : dp;
	}
	// 打の音量（3n rr 02）。**音色の音量の目盛りに先に掛ける**（そのあとでパートの
	// 音量が掛かる）。0 になっても 1 に持ち上げない。SFXKit1 の鍵 36 で 0-127 の
	// 17 点が firmware と一致: 目盛り = ((45 × (音量 + 1)) >> 7) × 101 >> 7
	int sfx_level(int part, int note, int level) const
	{
		if (note < ram::DRUM_SETUP_NOTE0 || note >= ram::DRUM_SETUP_NOTE0 + int(ram::DRUM_SETUP_NOTES))
			return level;
		// 打つ直前に書いた値は RAM にまだ入っていないことがあるので、控えを先に見る
		return (level * (drum_live(part, note, 0x02) + 1)) >> 7;
	}
	// 打のパン（3n rr 04）が 0 ＝ Rnd か
	bool drum_pan_rnd(int part, int note) const { return drum_setup_of(part, note, 0x04) == 0; }

	// **セットアップの控えを忘れる**。`set` が負なら 4 組とも。
	// 実機がセットアップを既定に戻すとき（XG・GM のシステムオン、00 00 7D nn、
	// ドラムのパートの音色の指定）に呼ぶ
	void clear_drum_setup(int set)
	{
		for (int s = 0; s < ram::DRUM_SETUP_SETS; s++)
			if (set < 0 || s == set)
				m_drum_touch[size_t(s)].fill(0);
	}

	// **ドラムのパートに音色の指定が来た**。実機はそのパートの組を既定に戻す
	// （NRPN でも SysEx でも、書いた切る高さ・HPF が消えるのを firmware で確かめた）
	void drum_program(int part)
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return;
		const int mode = int(m_ram[ram::part_base(part) + 0x07]);
		if (mode >= 2 && mode - 2 < ram::DRUM_SETUP_SETS)
			clear_drum_setup(mode - 2);
	}

	// **NRPN の番号 → セットアップの番地**（6.180）。
	// 並びが SysEx（`3n rr pp`）と違う。無いものは -1
	static int drum_nrpn_addr(int msb)
	{
		switch (msb) {
		case 0x14: return 0x0b;    // 切る高さ
		case 0x15: return 0x0c;    // 共振
		case 0x16: return 0x0d;    // 包絡線の立ち上がり
		case 0x17: return 0x0e;    // 包絡線の減衰 1
		case 0x18: return 0x00;    // 高さ（粗）
		case 0x19: return 0x01;    // 高さ（細）
		case 0x1a: return 0x02;    // 音量
		case 0x1c: return 0x04;    // パン
		case 0x1d: return 0x05;    // リバーブ送り
		case 0x1e: return 0x06;    // コーラス送り
		case 0x1f: return 0x07;    // バリエーション送り
		default:   return -1;
		}
	}

	// **ドラムセットアップの値**（3n rr pp）。組はパートモードから決まる
	int drum_setup_of(int part, int note, int param) const
	{
		if (!m_ram || note < ram::DRUM_SETUP_NOTE0
		    || note >= ram::DRUM_SETUP_NOTE0 + int(ram::DRUM_SETUP_NOTES))
			return -1;
		const int mode = int(m_ram[ram::part_base(part) + 0x07]);
		const int set = mode >= 2 ? mode - 2 : 0;
		if (set >= int(ram::DRUM_SETUP_SETS))
			return -1;
		return int(m_ram[ram::drum_setup(set, note, u32(param))]);
	}

	// **その打が離しを受けるか**（3n rr 09）。既定は 0 ＝ 受けない
	// （打ったら鳴りきる）。1 なら離しで止める。実機の離しの速さは
	// 音色によらず 0xCF（鍵 49・38・46・51 で確かめた。6.151）
	bool drum_rcv_note_off(int part, int note) const
	{ return drum_setup_of(part, note, 0x09) > 0; }

	// **その打が押しを受けるか**（3n rr 0A）。既定は 1。0 なら鳴らさない
	bool drum_rcv_note_on(int part, int note) const
	{ return drum_setup_of(part, note, 0x0a) != 0; }

	static constexpr u16 DRUM_OFF_RATE = 0xcf00;

	// **その打のオルタネートグループ**（ドラムセットアップの 3n rr 03）。
	// 0 は「組なし」。同じ組の打は互いを止める（ハイハットの開閉など）
	int drum_alt_group(int part, int note) const
	{
		if (!m_ram || note < ram::DRUM_SETUP_NOTE0
		    || note >= ram::DRUM_SETUP_NOTE0 + int(ram::DRUM_SETUP_NOTES))
			return 0;
		// **組はパートモードから**（08 pp 07）。2-5 が DRUMS1-4 で、
		// それぞれドラムセットアップの組 0-3 にあたる。
		// PART_KIT（+0x110）は音色の記録番号で、組ではない（ここで間違えた）
		const int v = drum_setup_of(part, note, 0x03);
		return v < 0 ? 0 : v;
	}

	// **その口・チャンネルを聞いているパート**（XG の 08 pp 04）。
	// ワーク RAM には**口 x 16 + チャンネル**（0-63 で A01-D16）が入っていて、
	// 127 は OFF。既定はパート n が n。
	//
	//   >= 0 … そのパート 1 つだけが聞いている（既定でも、付け替えでも）
	//   -1   … どのパートも聞いていない（実機は黙る）
	//   -2   … 2 つ以上が聞いている（実機は**重ねて鳴らす**）
	//
	// 2 つ以上のときは firmware に任せる。native は 1 つのノートオンから
	// 複数パートを鳴らす作りになっていないので、無理に鳴らすと薄くなる
	int rcv_part(int port, int ch) const
	{
		const int want = port * 16 + ch;
		if (!m_ram)
			return want;
		int found = -1;
		for (int p = 0; p < PARTS; p++) {
			if (int(m_ram[ram::part_base(p) + 0x04]) != want)
				continue;
			if (found >= 0)
				return -2;
			found = p;
		}
		return found;
	}

	// **鍵の範囲の中か**（XG の 08 pp 0F 下限・10 上限）。実機は範囲の外の
	// 鍵を鳴らさない。ここを見ていないと、**実機が黙っている所で音が出る**。
	// 下限 > 上限のときは「外側」が鳴る（XG の決まり）
	bool note_in_range(int part, int note) const
	{
		if (!m_ram || part < 0 || part >= PARTS)
			return true;
		const u32 b = ram::part_base(part);
		const int lo = int(m_ram[b + 0x0f]), hi = int(m_ram[b + 0x10]);
		return lo <= hi ? (note >= lo && note <= hi) : (note <= hi || note >= lo);
	}

	// その音を native で鳴らせるか（実際に鳴らす前に決める必要がある。
	// 鳴らせないなら firmware に回すので、遅らせてはいけない）
	// **写し取りを 1 音もしない道が既定**（段 4。2026-09-24 に切り替えた。6.223）。
	// 式だけでレジスタを組み、つまみの基準は既定の位置に置く（`nv::default_cal`）。
	//
	// 写し取りの道（firmware に 1 音鳴らさせてレジスタを写す）に戻すには
	// **`SMU2000_CAL=1`**（プラグインは `plugin.ini` の `cal=1`。鳴らし始める前に呼ぶこと）。
	// 前からある `SMU2000_NOCAL=1` は既定と同じなので、そのままでも動く。
	//
	// 切り替えた理由（6.223）: 実機の曲で式だけの道のほうが近く（帯域のずれ 0.50dB 対
	// 0.61dB）、firmware に 1 音も鳴らさせないのでスロットの取り合い（6.219・6.220）が
	// そもそも起きない。速さも落ちない。まだ式で出せない所（パート EQ・ミキサ）は
	// `nv::defaults` の実測の定数のまま
	static void set_nocal(bool on) { nocal_flag() = on ? 1 : 0; }
	static int &nocal_flag()
	{
		static int v = std::getenv("SMU2000_CAL") ? 0 : 1;
		return v;
	}
	static bool nocal_mode() { return nocal_flag() != 0; }

	// 合成の写し。要素ごとに 1 つずつ要る（中身は同じ）ので使い回す。
	// **大きさは変えない**。スロットは `slot_use::cal` でこの中を指すので、
	// あとから伸ばすと前の音の指し先が宙に浮く（dense で落ちた）
	static constexpr int SYNTH_CALS = 16;
	const std::vector<nv::voice_cal> &synth_cals() const
	{
		if (m_synth.empty())
			m_synth.assign(SYNTH_CALS, nv::default_cal());
		return m_synth;
	}

	bool can_play(int part, int note) const
	{
		if (!m_rom || part < 0 || part >= PARTS)
			return false;
		if (m_cc[part].unknown)              // 知らない CC が効いている間は firmware へ
			return false;
		// パートモード「DRUM」（番号なし）も firmware へ（drum_on の説明）
		if (m_ram && m_ram[ram::part_base(part) + 0x07] == 1)
			return false;
		if (is_drum(part)) {
			if (m_drum.find(drum_key(part, note)) != m_drum.end())
				return true;
			// 波形が埋まっていない記録（SFX キット。6.234）は音色記録が引ければ組める
			const u8 *drec = drum_rec_of(part, note);
			return nocal_mode() && (nv::drum_rec_has_wave(drec) ||
			                        nv::sfx_voice_record(m_rom, drec) != 0);
		}
		const u32 rec = record_of(part);
		if (!rec)
			return false;
		// **写し取りが要素の数だけ揃っていなくても native で鳴らす**（6.219）。
		// 足りないぶんは「合成の写し」で組む（note_on）。firmware に戻す道も
		// 試したが、鳴る時刻がずれてスロットの取り合いも変わるので、
		// 実機との差はかえって開いた（パンのずれ 0.41dB 対 0.65dB）
		return nocal_mode() || m_cal.find(cal_key(rec, part)) != m_cal.end();
	}

	// 鍵を押す。写し取りが無ければ false（呼んだ側が firmware に回す）
	bool note_on(int part, int note, int vel)
	{
		if (is_drum(part))
			return drum_on(part, note, vel);
		return note_on_rec(part, note, vel, record_of(part), -1);
	}

	// **記録を指定して押す**。fixed_note が 0 以上なら、要素の選択・波形・音程・
	// 鍵の曲線を**ぜんぶその鍵として**組む（SFX の打。実機は鍵 64 で組む。6.234）。
	// 押した鍵は離すときの照合（keynote）にだけ残る
	bool note_on_rec(int part, int note, int vel, u32 rec, int fixed_note)
	{
		if (!rec || !m_rom)
			return false;
		const auto it = m_cal.find(cal_key(rec, part));
		if (it == m_cal.end() && !nocal_mode())
			return false;
		const std::vector<nv::voice_cal> &cals =
		    it != m_cal.end() ? it->second
		                      : synth_cals();

		// **モノなら前の音を離す**（6.125）
		if (m_cc[part].mono)
			mono_cut(part, note);
		// **キーアサインがシングルなら、同じ鍵の前の音を離す**（08 pp 06）。
		// マルチ（既定）は重ねる。余韻の長い音色で同じ鍵を続けて押すと差が出る
		else if (key_assign_single(part)) {
			for (slot_use &s : m_slot)
				if (s.on && s.part == part && s.keynote == note)
					s.single_cut = true;
			note_off(part, note, true);
		}
		++m_inst;                        // この押しの番号（6.138）
		const int nelem = nv::element_count(m_rom, rec);
		// **ノートシフト**（08 pp 08）。実機は鍵を移してから音色を選ぶので、
		// ここから先はぜんぶ移した鍵で決める。離すときの照合だけ元の鍵
		const int sh = part_shift(part);
		const int pn0 = note + sh;
		const int pnote = fixed_note >= 0 ? fixed_note
		                : pn0 < 0 ? 0 : (pn0 > 127 ? 127 : pn0);
		// **鍵の曲線を引く鍵**（音量・切る高さ・包絡線の速さ）。ふつうは押した鍵
		// （6.172）だが、SFX の打は組む鍵（64）で引く
		const int knote = fixed_note >= 0 ? fixed_note : note;
		// **ベロシティ感度**（08 pp 0C・0D）。これも音色を選ぶ前に掛かる
		const int pvel = part_vel(part, vel);
		slot_bits keymask;
		bool any = false;
		u32 taken = 0;                   // もう使った写し取りの印
		int used = 0;
		int nwrote = 0;                  // レジスタを書いた要素の数
		std::vector<std::pair<slot_bits, u32>> pend;   // スロット → byte72 の遅れ
		// **滑る音は、押した鍵と滑り出す鍵の「高いほう」で波形を選ぶ**（6.168）。
		// 多段サンプルは鍵の上限で選ぶので、滑る範囲のいちばん高い所を
		// 通せる記録でないと足りない。CC84 で 48 から 72 へ滑るときは 72、
		// 84 から 60 へ滑るときは 84 の波形を実機が使っていた
		const int gsrc = m_cc[part].porta_src;
		const int gs = gsrc < 0 ? -1
		             : std::min(127, std::max(0, gsrc + part_shift(part)));
		const int wnote = fixed_note >= 0 ? fixed_note : gs > pnote ? gs : pnote;
		for (int k = 0; k < nelem; k++) {
			const u8 *el = nv::element(m_rom, rec, k);
			if (!nv::element_active(el, pnote, pvel))
				continue;
			// 波形の番地で、写し取ったスロットと結び付ける
			const u8 *we = nv::wave_entry(m_rom, nv::wave_set(el), nv::wave_note(m_rom, el, wnote));
			const nv::voice_cal *c =
			    we ? nv::match_cal(cals, nv::read_wave(we).format_addr, &taken) : nullptr;
			if (!c && size_t(used) < cals.size()) {
				c = &cals[used];
				taken |= u32(1) << used;
			}
			// **写し取りが足りないときは「合成の写し」に落とす**（6.219）。
			// 要素の数より写し取った数が少ないことがある（写し取りの音の最中に
			// firmware がこちらのスロットを取り返すと、その要素は記録が残らない）。
			// ここを null のままにしていたので、**パン・送り・フィルタ・共振を
			// 丸ごと書かずに鳴らしていた**（音が大きく外れる）。nocal の道と同じ
			// 合成の写しを渡せば、式の道としてひと通り組まれる。
			// **使い回しの器**を指すこと（スロットが指したまま残るので、
			// 一時物を指すと宙に浮く）
			if (!c) {
				const std::vector<nv::voice_cal> &sc = synth_cals();
				c = &sc[size_t(used) % sc.size()];
				m_cal_missing++;
				if (debug_on())
					std::fprintf(stderr, "cal 足りない part=%d note=%d vel=%d 要素 %d 個目/%d"
					                     " 写し取り %d 個 → 合成の写しで組む\n",
					             part + 1, note, vel, k + 1, nelem, int(cals.size()));
			}
			used++;
			const int slot = take_slot(part, note);
			if (slot < 0)
				break;
			if (busy() > m_peak)
				m_peak = busy();
			slot_use &su = m_slot[slot];
			su.elem = el;
			su.sfx = fixed_note >= 0;        // SFX の打は離しがドラムの決まり（6.234）
			// **SFX の打にもドラムセットアップが効く**（6.236）。実機は打ごとの
			// 切る高さ・共振・EG の速さを要素のバイトに足してから、旋律の道で組む。
			// 足した写しをスロットに持たせ、ここから先はそれを要素として使う
			if (su.sfx) {
				std::memcpy(su.sfx_el.data(), el, su.sfx_el.size());
				sfx_setup_elem(part, note, su.sfx_el.data());
				el = su.sfx_el.data();
				su.elem = el;
			}
			su.wave = we;
			su.cal = c;
			su.tpos = 0;
			su.tstart = m_clock;
			// **フィルタの包絡線を式で動かす**（録画の代わり）
			su.note = pnote;                 // 鳴らす鍵（移調ぶんを足したもの）
			if (fenv_on() && (c || nv::cut_exact())) {
				fenv_start(su, pvel);
				// **サンプル＆ホールドの音程**（6.206）。初めの刻みは 4、
				// そのあとは `0x40 - byte11` ごとに乱数を引く
				su.sh_off = 0;
				su.sh_cnt = 0;
				su.sh_next = ~u64(0);
				if (el && nv::sh_lfo(el)) {
					su.sh_cnt  = 4;
					su.sh_next = su.fnext + FENV_TICK;
				}
				// **LFO の初めの位相も乱数**（byte10 が 0 の要素だけ）。
				// 引く回数を実機に合わせるので、位相を使わなくても引く
				if (el && nv::lfo_rnd_phase(el))
					su.lph = u32(rnd_next()) & 0x7fff;
			}
			if (c || (nv::cut_exact() && fenv_on()) || m_peg_peek) {
				m_traj = true;
				m_traj_next = 0;       // つぎの tick で見直す
			}
			// **鍵の曲線は押した鍵で引く**（6.172）。波形の段の分
			// （volume_rest の wave_level）だけがずらした鍵に付いていく
			su.lvl0  = nv::volume_level(m_rom, rec, el, knote, c ? c->base_level : 0);
			if (su.sfx)
				su.lvl0 = sfx_level(part, note, su.lvl0);
			const bool sfx_mute = su.sfx && su.lvl0 == 0;   // 打の音量で目盛りが 0 になったら鳴らさない
			su.arest = nv::volume_rest(m_rom, el, pnote, pvel);
			su.att   = nv::clamp_att(nv::volume_att_from(
			    m_rom, su.lvl0, su.arest,
			    vol_gain_of(part,
			                m_cc[part].vol  >= 0 ? m_cc[part].vol
			                                     : (c ? c->cal_vol : 100),
			                m_cc[part].expr >= 0 ? m_cc[part].expr
			                                     : (c ? c->cal_expr : 127)),
			    assign_amp(part, note)));
			// 実機は目盛りが 0 なら減衰をいっぱい（0xFE）にする（SFXKit1 などの音量 0 で確かめた）
			if (sfx_mute)
				su.att = 0xfe;
			const part_cc &pc = m_cc[part];
			// **ポルタメント**（6.41）。前の鍵（CC84 があればその鍵）の音程で
			// 鳴らし始めて、10ms ごとに寄せていく。残りのずれはセント × 256 で持つ。
			// 追従を掛けるのは、鍵 1 つぶんの音程がその要素の追従で決まるから
			su.glide = 0;
			su.glide_step = 0;
			const int src = pc.porta_src >= 0 ? pc.porta_src : pc.last;
			if (pc.porta_on && src >= 0 && src != note) {
				su.glide_step = nv::porta_step(m_rom, pc.porta_time);
				if (su.glide_step > 0) {
					su.glide = (src - note) * nv::key_follow(m_rom, el) * 256;
					// firmware の 10ms タイマは世界共通なので、鍵を押した時刻からで
					// なく**格子**に乗せる（同時に鳴る音の滑りがそろう）。
					// 格子は包絡線と同じ（録画から取った実機の目）を使う（6.82）
					// **滑りだけ実機の 10ms 格子に乗せてみる道**（6.118）。
					// `SMU2000_PORTA_GRID=1` で試せる。包絡線の格子は触らない
					su.glide_next = (m_eg_have && porta_grid())
					              ? eg_after(u64(s64(m_clock) + EG_LAG))
					              : (su.fnext > nv::PORTA_TICK
					                 ? su.fnext - nv::PORTA_TICK
					                 : (m_clock / nv::PORTA_TICK + 1) * nv::PORTA_TICK);
				}
			}
			// **移調した鍵と、感度を掛けた強さで組む**（6.104）。ここに元の鍵を
			// 渡していたので、ノートシフトやマスター移調が音程・波形に効かなかった
			m_peg_flag = el[10] ? 0x4000 : 0;
			nv::slot_regs sr = nv::build_note(m_rom, el, su.sfx ? sfx_pitch_note(part, note, pnote) : pnote,
			                                  note_att(su, part), c,
			                                  nv::defaults(),
			                                  nv::bend_cents(pc.bend, pc.range)
			                                  + part_fine_cents(part)
			                                  + (su.sfx ? drum_live(part, note, 0x01) - 64 : 0)
		                                  + part_scale_cents(part, pnote) + nv::glide_cents(su.glide)
			                                  + assign_cents(part, note),
			                                  pvel, pc.atk, pc.dec,
			                                  pc.vrate, pc.vdep, wnote, knote,
			                                  pc.soft, part_ram(part, 0x62), part_ram(part, 0x63));
			if (c->synth) {
				// SFX の打はドラムと同じく打ごとの EQ（パートの EQ は掛からない）
				if (su.sfx)
					apply_drum_eq(sr, part, note);
				else
					apply_part_eq(sr, part);
			}
			// 音程の包絡線の行き先（byte31）。初めの高さと同じなら書かない
			{
				const u16 tgt = nv::peg_reg(m_rom, nv::peg_cents(el, el[31], pvel), el);
				su.peg_tgt = tgt == sr.v[0x10] ? 0xffff : tgt;
			}
			// **遅れて掛かるビブラート**（6.175）。遅れのあと
			// 20ms ごとに深さをせり上げる。`0x0a` の上位（型と刻み）は
			// 押した瞬間のまま使い回す
			su.vcnt = su.vtgt = su.vdly = su.vcnt2 = 0;
			su.vramp = false;
			su.vnext = ~u64(0);
			su.vamp = 0;
			// **フィルタ側の LFO**（6.189）。鍵を押すと
			// 位相は 0 に戻り、その場で 1 歩進む
			su.lstep = nv::flfo_on()
			         ? nv::lfo_step(m_rom,
			                        nv::vib_rate(int(el[11] & 0x3f), pc.vrate))
			         : u16(0);
			su.ltri  = el[9] != 0;
			su.lph   = su.lstep;
			su.lfull = su.lstep
			         ? nv::lfo_fdepth(m_rom, el, lfo_fdep_extra(part, pnote))
			         : 0;
			su.lrun  = true;             // 遅れが無ければすぐ回り出す
			su.lfdep = su.lfull;
			su.lcut  = 0;
			// **遅れのつまみ**（08 pp 17。6.232）。速さ・深さと同じ
			// 「下は小さいほう・上は大きいほう」。つまみで遅れが付くと、
			// **自身はせり上がらない音色でも遅れて掛かるようになる**
			// （実機の GrandPno は `0a=5f96` を押鍵で書くが、遅れ 100 を
			//  与えると `0a=5f00` で始めて 845ms から 20ms ごとにせり上げ、
			//  1066ms で `5f96` に着く。Vibes の `05` も同じで、押鍵は
			//  `aa00`、遅れが明けてから `aa04`）
			const int vdly_t = nv::vib_delay(nv::vib_delay_ticks(el), pc.vdly);
			if (nv::vib_ramps(el) || vdly_t > 0) {
				su.vtgt  = nv::vib_ramp_target(el);
				su.vstep = nv::vib_ramp_step(el);
				su.vdly  = vdly_t;
				if (su.vdly > 0) {
					su.lfdep = 0;        // 遅れのあいだは掛からない
					su.lrun  = false;    // 位相も止めておく
					// せり上がらない音色は押鍵の値に深さが入っている。
					// 遅れを付けたのだから、そこは 0 から始める
					sr.set(0x0a, u16(sr.v[0x0a] & 0xff00));
					sr.set(0x05, u16(sr.v[0x05] & 0xff00));
				}
				su.vhi   = u16(sr.v[0x0a] & 0xff00);
				su.ahi   = u16(sr.v[0x05] & 0xff00);
				su.vamp  = nv::vib_amp_depth(el);
				su.vfull = nv::vib_depth(nv::vib_ramp_reg(m_rom, su.vtgt) & 0x7f,
				                         pc.vdep);
				su.vdep  = pc.vdep < 0 ? 64 : pc.vdep;
				su.vcnt2 = 0;
				su.vramp = true;
				su.vnext = su.fnext;     // 包絡線と同じ格子に乗せる
				m_traj = true;
				m_traj_next = 0;
			}
			// **段 0 から始める**。実機は 10ms ごとに「着いたか」を見て次の段へ
			su.pvel = pvel;
			su.vel = vel;                    // 液晶のメーター用（6.148）
			su.pstage = 0;
			// **刻みはフィルタの包絡線と同じ**（実機はどちらも同じ 10ms の
			// タイマで動いている）。録画から取った格子に乗せる
			// フィルタの包絡線は「1 目遅らせて進め始める」ので、こちらは
			// その 1 目ぶん手前が実機の格子になる（実測で 312 サンプル）
			// **音程の包絡線の段も実機の 10ms 格子**（6.132）。
			// `SMU2000_PEG_GRID=0` で前の道（録画から取った目）に戻せる
			su.pnext = (m_eg_have && peg_grid())
			         ? eg_after(u64(s64(m_clock) + EG_LAG))
			         : (su.fnext > FENV_TICK ? su.fnext - FENV_TICK
			                                 : (m_clock / FENV_TICK + 1) * FENV_TICK);
			// **打のパンが 0（Rnd）なら打つたびにでたらめ**（6.236。SFX の打も）
			su.rnd_pan = (pan_is_rnd(part) || (su.sfx && drum_pan_rnd(part, note))) ? pan_rnd_draw() : -1;
			// Rnd のときの送りの目減り（音色の持つパンの位置ぶん）
			su.rnd_drop = su.rnd_pan < 0 ? 0
			            : nv::pan_send_drop(m_rom, nv::voice_pan_pos(
			                  m_rom, el, pnote, c ? c->cal_pan : 64));
			// **つまみの差を乗せる元は、式で組んだ値**（写し取りがあれば
			// build_note がそれで上書きしているので同じ値になる）
			su.base32 = sr.v[0x32];
			su.base33 = sr.v[0x33];
			su.base34 = sr.v[0x34];
			if (su.rnd_pan >= 0)
				sr.set(0x32, nv::pan_rnd_reg(m_rom, su.rnd_pan));
			else if (c && c->synth)
				sr.set(0x32, exact_pan(su, part));
			else if (c)
				sr.set(0x32, pan_reg(*c, part, -1, su.base32));
			su.lfo = sr.v[0x0a];
			su.cut = sr.v[0x00];
			if (c) {
				sr.set(0x0a, lfo_reg(su.lfo, *c, part, note));
				// 式で出した値なら鍵の追従はもう入っている（6.123）。
				// 明るさ（CC71）だけを、写し取りとの差ではなくそのまま足す
				sr.set(0x00, nv::cut_exact()
				             // **鍵の曲線は押した鍵で**（6.172）
				             ? cut_plain(nv::cutoff_keyon(m_rom, el, knote, pvel, false,
				                                          m_cc[part].atk,
				                                          nv::soft_vel(pvel, pc.soft)),
				                         part, el, pvel,
				                         assign_cut(part, note))
				             : cutoff_reg(su.cut, *c, part, el, knote,
				                          assign_cut(part, note)));
				// **共振は式で出した値に CC71 の差ぶんを乗せる**（写し取った
				// 値ではない。強さで変わるので写し取りは使えない。6.69）
				// **共振はパートのつまみを式の中に入れる**（6.169）。
				// 差を足す形（reso_reg）だと、実機の
				// 「つまみが 64 以上なら大きいほうを取る」が出ない
				// **写し取りの道でも式で出す**（6.210）。差を足す形だと
				// CC71=0x60 で 1 段ずれていた（実機 0x8000 / こちら 0x9000）
				sr.set(0x04, el
				             ? u16(u16(sr.v[0x04] & 0x07ff) |
				                   u16(u16(nv::reso_level(el, pvel, res_knob(part))) << 11))
				             : reso_reg(sr.v[0x04], *c, part));
				// **フィルタの第 2 段（ハイパス）はパートの HPF（0A pp 20）を足して式で出す**。
				// 写し取った値のままだと、パートの HPF を動かしても音が変わらなかった
				if (el)
					sr.set(0x02, nv::filter2_reg(el[82], su.sfx ? drum_hpf(part, note) : part_hpf(part)));
				if (c->synth) {
					sr.set(0x33, exact_send(su, part, false, su.base33));
					sr.set(0x34, exact_send(su, part, true, su.base34));
					// **インサーションを通るパートはミキサが丸ごと別**（6.161）
					if (ins_routed(part))
						ins_mixer(sr, nv::voice_pan_pos(m_rom, el, pnote), ins_target(part));
				} else {
					sr.set(0x33, send_reg(*c, 0x33, false, pc.rev, c->cal_rev,
					                      su.rnd_drop, su.base33));
					sr.set(0x34, send_reg(*c, 0x34, true, pc.cho, c->cal_cho,
					                      su.rnd_drop, su.base34));
				}
			}
			write_slot(slot, sr);
			if (debug_on())
				std::fprintf(stderr, "note part=%d note=%d vel=%d vol=%d/%d expr=%d/%d pan=%d/%d att=%d->%d\n",
				             part, note, vel, pc.vol, c ? c->cal_vol : -9, pc.expr, c ? c->cal_expr : -9,
				             pc.pan, c ? c->cal_pan : -9, su.att, note_att(su, part));
			// byte72 が 0 でなければ、その要素は遅れて鳴る
			pend.push_back({ slot_bits::of(slot), nv::elem_delay(el) });
			nwrote++;
			any = true;
		}
		if (any) {
			m_cc[part].last = note;      // つぎの音はここから滑る
			m_cc[part].porta_src = -1;   // CC84 の指定は 1 度で使い切る
		}
		// **要素を書き終えてから鍵を押す**（6.117）。SFX の打はそこから
		// さらに sfx_proc() ぶん遅れる（6.234）
		const s64 at0 = s64(write_done(nwrote)) + (fixed_note >= 0 ? sfx_proc() : 0);
		const u64 at = at0 < 0 ? 0 : u64(at0);
		for (const auto &p : pend) {
			if (p.second || at > m_clock)
				m_pend.push_back({ p.first, at + p.second });
			else
				keymask |= p.first;
		}
		if (!keymask)
			return any;                  // 遅らせた要素だけの音もある
		key_on(keymask);
		return true;
	}

	// **firmware が鳴らした音**も、最後に押した鍵として覚える。
	// これが無いと、写し取りの 1 音目のつぎの音が滑らない
	void note_fw(int part, int note)
	{
		if (part < 0 || part >= PARTS)
			return;
		m_cc[part].last = note;
		m_cc[part].porta_src = -1;
		// **firmware が鳴らす打でも、同じオルタネートグループの
		// 打は止める**（6.194）。写し取りの 1 打目は firmware が鳴らすので、
		// その打がこちらの鳴らしている打を止められず、
		// **ペダルハイハット（44）を打っても開いた音（46）が鳴り残っていた**
		if (!is_drum(part))
			return;
		const int grp = drum_alt_group(part, note);
		if (grp)
			wrote_regs(alt_cut(part, note, grp));
	}

	// 鍵を離す。鳴っていなければ false
	// **離すのは 1 回ぶんだけ**（6.138）。同じ鍵を離さずに何度も押すと、
	// 実機は押したぶんだけスロットを使い、ノートオフ 1 つでは**いちばん古い
	// 1 回ぶん**しか離さない（残りには離しの速さを書かない）。こちらは
	// 同じ鍵のスロットを全部離していたので、刻みの曲で音がごっそり消えていた。
	// `all` は全部切るとき（オールノートオフ・モノ・ダンパー離し）
	bool note_off(int part, int note, bool all = false)
	{
		bool any = false;
		u32 want = 0;
		if (!all) {
			// いちばん古い押しの番号を探す
			for (int i = 0; i < SLOTS; i++) {
				const slot_use &s = m_slot[i];
				if (s.on && s.part == part && s.keynote == note &&
				    (!want || s.inst < want))
					want = s.inst;
			}
			if (!want)
				return false;
		}
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (!s.on || s.part != part || s.keynote != note)
				continue;
			if (!all && s.inst != want)
				continue;
			if (debug_on())
				std::fprintf(stderr, "off part=%d note=%d slot=%d clock=%llu%s\n", part, note, i,
				             (unsigned long long)m_clock, s.keyed ? "" : "（押す前。押してから離す）");
			// **まだ鍵を押していない音の離しは、押した直後まで待たせる**（key_on）
			if (!s.keyed) {
				s.off_wait = true;
				any = true;
				continue;
			}
			release_slot(i);
			any = true;
		}
		return any;
	}

	// スロット 1 つを離す（note_off の中身。鍵を押す前に届いた離しは key_on から）
	void release_slot(int i)
	{
		slot_use &s = m_slot[i];
		const int part = s.part;
		const int note = s.keynote;
		{
			if (m_cc[part].damper) {       // ダンパーを踏んでいる間は切らない
				s.held = true;
				return;
			}
			if (s.sost)                    // ソステヌートで待たせている音
				return;
			// 減衰は**いまのつまみで**出す。s.att は鳴らし始めたときの値なので、
			// 途中で音量を絞られた音を離すと、絞る前の大きさで鳴り終わってしまう
			// **SFX の打**（6.234）は要素を持つが、離しを受けるかはドラムの決まり
			// （3n rr 09）。受けるなら**要素の離しの速さ**で離す（実機は鍵 39 で 0xCC、
			// 鍵 68 で 0xA8 と要素ごとの値を書く。ドラムの 0xCF ではない）
			if (s.elem && (!s.sfx || drum_rcv_note_off(part, note)))
				m_poke(u32(i) * 64 + 9, release_of(s, part, s.keynote));
			// **離しを受けるドラム**（3n rr 09）。要素を持たないので
			// 速さだけ与えて、音量はそのときの値にする（6.151）
			else if (!s.elem && drum_rcv_note_off(part, note))
				m_poke(u32(i) * 64 + 9,
				       u16(DRUM_OFF_RATE | u16(note_att(s, part) & 0xff)));
			// ドラムは離しでも音を切らない（実機も打ったら鳴りきる）
			s.on = false;
			// **ドラムも「鳴っている」ことにする**。離しの段は無いが、
			// 打の尾が残っている間はスロットを空けない（6.89・6.139）。
			// `s.elem != nullptr` にしていたので**ドラムだけ外れていて**、
			// 離した打のスロットをすぐ次の打で使い回していた。実機はロールの
			// 1 打ごとに別のスロットを使う（28・29・30・31…）
			s.rel = true;
			s.rel_at = m_clock;
			s.rel_att = s.att;
			s.rpos = 0;
			fenv_release(s);
			peg_release(i);              // 音程の包絡線も離す（6.205）
			if ((s.cal && !s.cal->filter_env.empty())
			    || (nv::cut_exact() && fenv_on() && s.elem)) {
				m_traj = true;
				m_traj_next = 0;
			}
		}
	}

	// そのパートの音を全部止める
	// **こちらで鳴らしている音を全部離す**（native の口を切るときに呼ぶ）。
	// 切ったあとは firmware がこのスロットを知らないので、離しておかないと
	// 鳴りっぱなしになる。ぶつ切りではなく離しの速さで鳴り終わらせる
	void silence()
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (!s.on)
				continue;
			if (s.elem && m_rom && m_poke)
				m_poke(u32(i) * 64 + 9, nv::release_reg(m_rom, s.elem, s.keynote, s.att));
			s.on = false;
			s.held = false;
			s.sost = false;
		}
		m_pend.clear();
		m_busy = 0;
		m_bend_due.fill(0);
		m_bend_next = ~u64(0);
		m_traj = false;
		m_traj_next = 0;
	}

	// 離しの `0x09`。**オールサウンドオフ（CC120）は速さを最大にする**
	// （実機は上位に `0xf0` を書く。6.126）。ふつうの離しは音色の速さ
	// **キーアサインがシングルで切るときの離しの速さ**（doc/native-engine.md
	// の 6.149）。実機は音色によらず 0xD9 を書く（Strings・GrandPno・
	// Square Lead・Music Box の 4 つで確かめた）。音量はそのときの値のまま
	static constexpr u16 SINGLE_CUT_RATE = 0xd900;

	u16 release_of(const slot_use &s, int part, int note) const
	{
		const u16 v = nv::release_reg(m_rom, s.elem, note, note_att(s, part),
		                              m_cc[part].rel);
		if (s.hard)
			return u16(0xf000 | (v & 0xff));
		return s.single_cut ? u16(SINGLE_CUT_RATE | (v & 0xff)) : v;
	}

	// **ダンパーを踏んだ瞬間、離している最中の音を拾う**（6.164）。
	// 実機は離しをやめて減衰 2 の速さに戻す。入れていなかったので、
	// ペダルで拾ったはずの音がそのまま消えていた
	void damper_catch(int part)
	{
		if (!m_rom)
			return;
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part || s.on || !s.rel || !s.elem || s.hard)
				continue;
			s.caught = true;
			m_poke(u32(i) * 64 + 9,
			       nv::damper_hold_reg(m_rom, s.elem, s.keynote, note_att(s, part),
			                           m_cc[part].dec,
			                           s.cal && s.cal->have ? s.cal->dec_adj[1] : 0));
		}
	}

	// CC123（オールノートオフ）は離す。CC120（オールサウンドオフ）は
	// ダンパーも無視して、離しを最速にして切る
	void all_off(int part, bool hard = false)
	{
		for (int i = 0; i < SLOTS; i++) {
			slot_use &s = m_slot[i];
			if (s.part != part)
				continue;
			if (hard && (s.on || s.rel)) {
				s.hard = true;
				s.held = false;
				s.sost = false;
				if (!s.on && s.elem)       // もう離している音も切り直す
					m_poke(u32(i) * 64 + 9, release_of(s, part, s.keynote));
			}
			if (s.on)
				note_off(part, s.keynote, true);
		}
	}

	// ドラムの 1 打。写し取った値をそのまま使い、音量だけ強さで動かす
	bool drum_on(int part, int note, int vel)
	{
		// **パートモード「DRUM」（番号なし、08 pp 07 = 1）は firmware に任せる**。
		// ドラムセットアップ（DRUMS1-4）の編集が効かず、キットの既定値のまま鳴る
		// （firmware で 4 組それぞれの鍵 38 の音量を 0 にして確かめた。DRUMS1-4 の
		// パートは消え、DRUM のパートだけ鳴り続ける）。こちらは既定値の引き方を
		// まだ持たないので、DRUMS1 の値で鳴らすと編集が効いてしまっていた。
		// Bank 127 でドラムにしたパートは DRUMS1-4 が割り当てられるので、ここには来ない
		if (m_ram && part >= 0 && part < PARTS && m_ram[ram::part_base(part) + 0x07] == 1)
			return false;
		const auto it = m_drum.find(drum_key(part, note));
		const bool synth = it == m_drum.end();
		if ((synth && !nocal_mode()) || !m_rom)
			return false;
		// 合成のときは 1 つだけ使う（ドラムは 1 打 1 スロット）
		const std::vector<nv::voice_cal> &dcals = synth ? synth_cals() : it->second;
		const size_t ndcal = synth ? 1 : dcals.size();
		slot_bits keymask;
		int nwrote = 0;
		// **同じオルタネートグループの打を止める**（6.151）。ハイハットの
		// 開いた音は、閉じた音を打った瞬間に止まる。見ていないと刻みが濁る
		const int grp = drum_alt_group(part, note);
		const int cutn = grp ? alt_cut(part, note, grp) : 0;
		wrote_regs(cutn);
		// **波形が埋まっていない記録は、旋律の音色記録から組む**（SFX キットの打。
		// 6.234）。+24/+25 の索引で 0x283B50 の表を引くと音色記録が出るので、
		// 旋律の道（note_on_rec）に鍵 64 で渡す。記録が引けなければ firmware に回す。
		// 同じ組の打を止めるのは上で済ませてある。写し取りがあるなら、それは
		// 実機から取った音なのでそのまま使える
		if (synth) {
			const u8 *drec = drum_rec_of(part, note);
			if (!nv::drum_rec_has_wave(drec)) {
				const u32 vrec = nv::sfx_voice_record(m_rom, drec);
				return vrec ? note_on_rec(part, note, vel, vrec, nv::SFX_NOTE) : false;
			}
		}
		++m_inst;                        // この打の番号（6.138）
		for (size_t di = 0; di < ndcal; di++) {
			const nv::voice_cal &c = dcals[di];
			const int slot = take_slot(part, note);
			if (slot < 0)
				break;
			// 減衰は足し算なので、強さのぶんだけずらせばよい（6.5）
			const int att0 = c.has(9) ? (c.reg[9] & 0xff) : 0x40;
			slot_use &su = m_slot[slot];
			su.elem = nullptr;               // ドラムは離しの速さを写しの値で済ませる
			su.vcnt = su.vtgt = su.vdly = su.vcnt2 = 0;   // 前の音のビブラートのせり上がりを持ち越さない（6.217）
			su.vramp = false;
			su.vnext = ~u64(0);
			su.wave = nullptr;
			su.cal = &c;
			su.tpos = 0;
			su.tstart = m_clock;
			su.rnd_pan = (pan_is_rnd(part) || drum_pan_rnd(part, note)) ? pan_rnd_draw() : -1;
			su.rnd_drop = 0;
			su.vel = vel;
			m_traj = true;
			m_traj_next = 0;
			// **強さの曲線はドラムの列（行 1）**（6.210）。旋律の列（行 0）を
			// 使っていたので、写し取った強さから離れた打で減衰が 2 段（0.75dB）ずれていた
			su.att = att0 + 2 * (nv::velocity_att(m_rom, vel, nv::DRUM_VEL_CURVE) -
					  nv::velocity_att(m_rom, c.cal_vel, nv::DRUM_VEL_CURVE));
			// **写しが無いときは、ドラムセットアップから直に組む**（6.155）
			if (synth) {
				const int lv = drum_setup_of(part, note, 0x02);
				const u8 *rc = m_ram ? nv::drum_record(
				    m_rom, int(m_ram[ram::part_base(part) + nv::PART_KIT]), note)
				                     : nullptr;
				su.att = nv::drum_att(m_rom, rc, lv < 0 ? 127 : lv, vel,
				                      vol_gain_of(part, part_vol(part), part_expr(part)));
				su.lvl0 = 0;
				su.arest = 0;
			}
			const int att = synth ? nv::clamp_att(su.att) : note_att(su, part);
			su.lfo = c.has(0x0a) ? c.reg[0x0a] : 0;
			// **式で組む道**（`SMU2000_DRUM_EXACT=1`）。記録の 42 バイトから
			// 0x00・0x02・0x04・0x06-0x08・0x11・0x12-0x17 を出す（6.86・6.87）。
			// パン・送り・EQ は写し取りのまま（パートの設定を含むので）
			const u8 *drec = nullptr;
			if ((nv::drum_exact() || synth) && m_ram)
				drec = nv::drum_record(m_rom,
				                       int(m_ram[ram::part_base(part) + nv::PART_KIT]), note);
			nv::slot_regs dr;
			if (drec) {
				// **高さも写しではなくドラムセットアップから**（3n rr 00/01）。
				// 渡していなかったので、曲が打の高さを変えても効かなかった
				const int co = drum_setup_of(part, note, 0x00);
				const int fi = drum_setup_of(part, note, 0x01);
				// **切る高さ・共振・立ち上がり・減衰 1・減衰 2**（3n rr 0B-0F）は
				// 表の索引をずらす（6.180）。SysEx でも NRPN でも効く（6.235）。
				// 打つ時点では RAM がまだ古いことがあるので、MIDI の控えを先に見る
				dr = nv::drum_note(m_rom, drec, att, nv::defaults(),
				                   co < 0 ? 64 : co, fi < 0 ? 64 : fi,
				                   drum_live(part, note, 0x0d),
				                   drum_live(part, note, 0x0b),
				                   drum_live(part, note, 0x0c),
				                   drum_live(part, note, 0x0e),
				                   drum_live(part, note, 0x0f));
				// **`0x10` のビット 14 は、直前に鳴らした旋律の音の
				// 印を拾う**（6.179）。実機は旋律の段で `0x43E96E` に
				// byte10 の印を置くが、ドラムの段はそこを書き直さず
				// 前の値をそのまま使う。チップはこのビットを見ていない（`& 0x3fff`）ので
				// 音は変わらないが、合わせておくと物差しが濁らない
				dr.set(0x10, m_peg_flag);
				apply_drum_eq(dr, part, note);
			}
			if (synth) {
				// パン・送りもドラムセットアップから（6.155）。
				// `su.keynote` はもう入っているので exact_* が使える
				dr.set(0x32, su.rnd_pan >= 0 ? nv::pan_rnd_reg(m_rom, su.rnd_pan)
				                             : exact_pan(su, part));
				dr.set(0x33, exact_send(su, part, false, dr.v[0x33]));
				dr.set(0x34, exact_send(su, part, true, dr.v[0x34]));
				if (ins_routed(part)) {
					const int dp = drum_setup_of(part, su.keynote, 0x04);
					ins_mixer(dr, dp < 0 ? 64 : dp, ins_target(part));
				}
			}
			// **つまみの差を乗せる元**。写しがあればその値、
			// 無ければドラムセットアップから組んだ値
			su.base32 = synth ? dr.v[0x32] : (c.has(0x32) ? c.reg[0x32] : dr.v[0x32]);
			su.base33 = synth ? dr.v[0x33] : (c.has(0x33) ? c.reg[0x33] : dr.v[0x33]);
			su.base34 = synth ? dr.v[0x34] : (c.has(0x34) ? c.reg[0x34] : dr.v[0x34]);
			for (int i = 0; i < 0x40; i++)
				if (drec && (dr.write & (u64(1) << i)) && i != 9
				    && (synth || (i != 0x32 && i != 0x33 && i != 0x34
				                  && !(i >= 0x20 && i <= 0x2b)
				                  && i != 0x03 && i != 0x05 && i != 0x0a)))
					m_poke(u32(slot) * 64 + u32(i), dr.v[i]);
				else if (c.has(i) || (synth && (i == 9 || (i >= 0x32 && i <= 0x34))))
					m_poke(u32(slot) * 64 + u32(i),
					       i == 9 ? u16(att)
					              : (i == 0x32 ? pan_reg(c, part, su.rnd_pan, su.base32)
					              : (i == 0x0a ? lfo_reg(c.reg[0x0a], c, part)
					              : (i == 0x33 ? send_reg(c, 0x33, false, m_cc[part].rev, c.cal_rev,
					                                      su.rnd_drop, su.base33)
					              : (i == 0x34 ? send_reg(c, 0x34, true, m_cc[part].cho, c.cal_cho,
					                                      su.rnd_drop, su.base34)
					                           : c.reg[i])))));

			// **フィルタの第 2 段（ハイパス）は打つたびに式で書く**。実機は打つたびに
			// 記録の byte20 × 16 にパートの HPF（0A pp 20）を足して書く（旋律と同じ式）。
			// 写し取りにはこのレジスタが入らないので、書かないと前にそのスロットで
			// 鳴った音の値が残り、パートの HPF を動かしても打の音が変わらなかった
			if (const u8 *rec2 = drec ? drec
			                          : m_ram ? nv::drum_record(m_rom, int(m_ram[ram::part_base(part) + nv::PART_KIT]), note)
			                                  : nullptr)
				m_poke(u32(slot) * 64 + 0x02, nv::filter2_reg(rec2[20], drum_hpf(part, note)));

			su.drum_rel = c.has(9) ? u16(c.reg[9]) : u16(att);
			if (busy() > m_peak)
				m_peak = busy();
			if (debug_on())
				std::fprintf(stderr, "drum part=%d note=%d slot=%d clock=%llu vel=%d/%d att=%d->%d 段 %d 写し %016llx%s",
				             part, note, slot, (unsigned long long)m_clock, vel, c.cal_vel, att0, att,
				             int(c.filter_env.size()), (unsigned long long)c.mask, "\n");
			keymask.set(slot);
			nwrote++;
		}
		if (!keymask)
			return false;
		// **ドラムも要素を書き終えてから押す**（6.117）。そのうえで、実機は
		// ドラムの 1 打を引くのに旋律より少し手間が掛かる（キットの表 →
		// 鍵ごとのずれ → 記録の 3 段引き）。実測で 2 サンプルぶん遅い（6.139）
		s64 at0 = s64(write_done(nwrote)) + drum_proc();
		// **切った打があると打鍵が 1 サンプル遅れる**（6.151）。実機は
		// 切る書き込み（0xD9）を先に済ませてから鍵を押すので、そのぶん
		// 後ろへずれる。打ちっぱなしのときは at0 が m_clock に張り付くので、
		// ここははっきり足す（実測で 4 回とも 1 サンプルだった）。
		// **+2 なのは、ここに来るときの m_clock が 1 つ古いから**
		// （mu2000 は m_ne_clock を進めてから native_pump → tick と呼ぶので、
		// +1 だと同じサンプルの tick で発火してしまう）
		if (cutn && at0 <= s64(m_clock) + 1)
			at0 = s64(m_clock) + 2;
		const u64 at = at0 < 0 ? 0 : u64(at0);
		if (at > m_clock)
			m_pend.push_back({ keymask, at });
		else
			key_on(keymask);
		return true;
	}

	// `SMU2000_DRUM_PROC` で振れる（サンプル数）
	static s64 drum_proc()
	{
		static const s64 v = std::getenv("SMU2000_DRUM_PROC")
		                   ? s64(std::atoi(std::getenv("SMU2000_DRUM_PROC"))) : -2;
		return v;
	}

	// **SFX の打の押鍵の遅れ**（6.234）。実機はキットの表 → 打の記録 → 音色記録の表 →
	// 旋律の組み立て、と 3 段引いてから押すので、旋律の式より遅い。SFX Kit1 の鍵 36 で
	// 実機 s=384306・こちら 384302（33 本の書き込みに 30 サンプル）。ノイズ系の SFX は
	// 3 サンプルずれるだけで相関が消える（ずらせば 100%）。`SMU2000_SFX_PROC` で振れる
	static s64 sfx_proc()
	{
		static const s64 v = std::getenv("SMU2000_SFX_PROC")
		                   ? s64(std::atoi(std::getenv("SMU2000_SFX_PROC"))) : 4;
		return v;
	}

private:

	// いちばん多いときに、いくつのスロットを使ったか（取り合いを見るため）
	int busy() const
	{
		int n = 0;
		for (const slot_use &s : m_slot)
			if (s.on)
				n++;
		return n;
	}

	slot_use fresh(int part, int note)
	{
		slot_use s;
		s.on = true;
		s.part = part;
		s.note = note;
		s.keynote = note;
		s.tstart = m_clock;
		s.age = ++m_age;
		s.inst = m_inst;
		return s;
	}

	// **下の 8 スロットは firmware のために空けておく。**
	// firmware は下から使うので、写し取りの 1 音目とぶつからない。
	// dense（16 パート・60 音）でもこちらが使うのは 36 までなので足りる
	static constexpr int FW_SLOTS = 8;

	// **写し取りをやめた口（段 4）では 1 つも空けない**（6.174）。
	// firmware は 1 音も鳴らさないので、下 8 を遠慮する理由が無い。
	// dense（16 パート × 3 音 = 60 声）では 56 しか使えず、
	// 実機が 1 回も奪わないところをこちらは 4 回奪っていた。
	// 固定の果てにもう鳴らない音が出るので、聴いて分かる違いになる。
	// firmware が鳴らす場合でも `fw_recent` が動的に遠ざける
	int fw_slots() const { return nocal_mode() ? 0 : FW_SLOTS; }

	// 空きスロットを取る。無ければ一番古い声を止めて使う。
	// **上から**取る（firmware は下から使うため）
	int take_slot(int part, int note)
	{
		// **firmware が最近触ったスロットは避ける**。下 8 個を空けるだけでは
		// 足りなかった（声が増えると firmware は上の方も使う）。
		// 窓やプラグインのように MIDI がブロック単位で届くと、同時に鳴る音が
		// 増えて firmware が上まで伸びる。避けないと、firmware が自分の音の
		// 続きを書いたときにこちらの音の包絡線が書き替わって壊れる
		// **離しの最中のスロットは「空き」ではない**。実機は鳴り終わるまで
		// スロットを持ち続ける。こちらは離した瞬間に空きとして配り直して
		// いたので、離しの尾が次の音でぶつ切りになっていた。
		// 利用者の曲は同じパートで 144ms おきに音が来るのに離しは 1.1 秒
		// あるので、実機が 8 声使うところをこちらは 1〜2 声で鳴らしていた
		// （その結果、そのパートだけ 1dB 静かだった）。
		// 空きが無いときだけ、離しの古いものから取る
		for (int pass = 0; pass < 2; pass++) {
			const bool avoid = pass == 0;
			int oldest = -1, oldest_rel = -1;
			u64 oldest_age = ~u64(0), oldest_rel_age = ~u64(0);
			// マスタの上から（下の `keep` 個は firmware に空ける）、埋まったら
			// スレーブの上から。実機の firmware もマスタを使い切ってからスレーブへ回す
			const int keep = fw_slots();
			for (int n2 = 0; n2 < SLOTS - keep; n2++) {
				const int i = n2 < CHIP_SLOTS - keep ? CHIP_SLOTS - 1 - n2
				                                     : SLOTS - 1 - (n2 - (CHIP_SLOTS - keep));
				if (avoid && fw_recent(i))
					continue;
				const slot_use &u = m_slot[i];
				const bool ringing = u.rel && m_clock - u.rel_at <= REL_FOLLOW;
				if (!u.on && !ringing) {
					m_slot[i] = fresh(part, note);
					return i;
				}
				if (!u.on) {
					if (u.age < oldest_rel_age) {
						oldest_rel_age = u.age;
						oldest_rel = i;
					}
				} else if (u.age < oldest_age) {
					oldest_age = u.age;
					oldest = i;
				}
			}
			// 離しの古いものを先に取る（まだ押されている音は最後まで残す）
			if (oldest_rel >= 0) {
				m_slot[oldest_rel] = fresh(part, note);
				return oldest_rel;
			}
			// 避けた結果どこも空いていなければ、2 周目で避けずに探す
			// （音が出ないより、稀にぶつかる方がまし）
			if (oldest >= 0) {
				m_slot[oldest] = fresh(part, note);
				return oldest;
			}
		}
		return -1;
	}

	void write_slot(int slot, const nv::slot_regs &r)
	{
		// スレーブの出口（0x35-0x37）の置き換えは m_poke の先（mu2000）でやる。
		// ドラムは write_slot を通らずに直接書くので、ここでは漏れていた
		for (int i = 0; i < 0x40; i++)
			if (r.write & (u64(1) << i))
				m_poke(u32(slot) * 64 + u32(i), r.v[i]);
	}

	// **音程の包絡線の段を進める**。チップが行き先に着いていたら、
	// 次の段の速さ（0x0b）と行き先（0x10）を張る
	// **音程の包絡線の離しの段**（6.205。実機 0x12B98C-0x12BA06）。
	// 速さは byte29（パートの塡 +0x65 で補正して、CC72 を掛ける）、
	// 行き先は byte34。byte33 と byte34 が同じで、もう段 3 に居て、
	// +0x64 が中央なら何も書かない
	void peg_release(int i)
	{
		slot_use &s = m_slot[i];
		if (!s.elem || !m_rom || !m_ram)
			return;
		const u8 *e = s.elem;
		const u8 *b = m_ram + ram::part_base(s.part);
		if (e[33] == e[34] && s.pstage >= 3 && b[0x64] == 64)
			return;
		s.pstage = 3;
		int raw = int(e[29]);
		const int d = int(b[0x65]) - 64;
		if (d > 0) {
			const int tb = int(m_rom[nv::PEG_REL_TAB + u32(d)]);
			if (raw > tb)
				raw = tb;
		} else {
			raw -= d >> 1;
			if (raw > 63)
				raw = 63;
		}
		const int rel = m_cc[s.part].rel;
		const int rate = nv::peg_rate_reg_raw(m_rom, e, raw, s.keynote, s.pvel,
		                                      64, rel < 0 ? 64 : rel);
		m_poke(u32(i) * 64 + 0x0b, u16(rate << 8));
		// **行き先は byte34 にパートの +0x64 を足す**（6.212）。
		// XG の `08 pp 6B`。見ていなかったので、そこを動かした曲で
		// 離したあとの音程が丸ごと違っていた
		int lv = int(e[34]) + (int(b[0x64]) - 64);
		if (lv < 0) lv = 0;
		if (lv > 127) lv = 127;
		m_poke(u32(i) * 64 + 0x10,
		       nv::peg_reg(m_rom, nv::peg_cents(e, lv, s.pvel), e));
	}

	void peg_advance(int i)
	{
		slot_use &s = m_slot[i];
		if (!s.elem || !m_rom)
			return;
		const u8 *e = s.elem;
		s.pstage++;
		// **行き先が前の段と同じなら段を飛ばす**（6.205。実機 0x12B8D8-0x12B946）。
		// フィルタの包絡線（`fenv_next`）とまったく同じ形。
		// 飛ばして段が無くなったら**何も書かない**
		if (s.pstage == 1 && nv::peg_level_of(e, 0) == nv::peg_level_of(e, 1))
			s.pstage = 2;
		if (s.pstage == 2 && nv::peg_level_of(e, 1) == nv::peg_level_of(e, 2))
			s.pstage = 3;
		if (s.pstage > 2) {
			s.pstage = 3;
			return;
		}
		// **段ごとに渡すつまみが違う**（6.205。実機 0x12B8DC-0x12BB00）。
		// 段 0 は CC73、**段 1 は何も掛からず（0x40 固定）**、段 2 は CC75。
		// 前に「段 1・2 にも掛けたら rpn が悪くなった」のは、
		// 段 0 と同じ CC73 を掛けていたから
		const int dec = m_cc[s.part].dec;
		const int cc = s.pstage == 2 ? (dec < 0 ? 64 : dec) : 64;
		const int rate = nv::peg_rate_reg_stage(m_rom, e, s.pstage, s.keynote, s.pvel,
		                                        64, cc);
		const int lvl  = nv::peg_level_of(e, s.pstage);
		m_poke(u32(i) * 64 + 0x0b, u16(rate << 8));
		m_poke(u32(i) * 64 + 0x10,
		       nv::peg_reg(m_rom, nv::peg_cents(s.elem, lvl, s.pvel), s.elem));
	}

	// **要素 1 つぶんのレジスタを書く時間**（1/64 サンプル単位）。
	// 実機は要素のレジスタを **1 要素 34 本**書き、**全部書き終えてから**
	// 鍵を押す。SWP30 への書き込みは 1 本 440 サイクル待たされるので
	// （＋命令のぶんで 1 本あたり約 560 サイクル）、34 本でちょうど
	// **30 サンプル**かかる。要素が 1 つ増えるごとに鍵がそのぶん遅れる。
	// こちらは一度に書いて即座に押していたので、2 要素の音色
	// （Square Lead など）が 30 サンプル早く鳴っていた
	// （doc/native-engine.md の 6.117）。`SMU2000_ELEM_COST` で振れる
	static u32 elem_cost64()
	{
		static const u32 v = std::getenv("SMU2000_ELEM_COST")
		                   ? u32(std::atoi(std::getenv("SMU2000_ELEM_COST"))) : 30 * 64;
		return v;
	}
	// **要素を全部書き終える時刻**を返す（サンプル）。実機は 1 つの
	// CPU で順番に書くので、前の音がまだ書き終わっていなければその
	// あとに並ぶ。和音や密な曲では、あとの音ほど遅れて鳴る
	// **レジスタ 1 本ぶんの時間**（要素 1 つが 34 本ぶん）。要素の外で
	// 書いたぶんも、実機では同じだけ CPU を食う
	static u64 reg_cost64() { return elem_cost64() / 34; }

	// 要素の外で n 本書いたぶん、つぎの書き込みを後ろへずらす。
	// オルタネートグループで打を切ると、実機は**そのぶん打鍵が 1 サンプル
	// 遅れる**（入れないと閉じたハイハットだけ 1 サンプル早く鳴る。6.151）
	void wrote_regs(int n)
	{
		if (n <= 0)
			return;
		const u64 cost = elem_cost64();
		const u64 now = m_clock * 64;
		const u64 t0 = now > cost ? now - cost : 0;
		const u64 start = t0 > m_busy ? t0 : m_busy;
		m_busy = start + u64(n) * reg_cost64();
	}

	u64 write_done(int nwrote)
	{
		const u64 cost = elem_cost64();
		const u64 now = m_clock * 64;
		// こちらが呼ばれる時刻は「1 要素を書き終えた時刻」なので、
		// 書き始めはその 1 要素ぶん手前
		const u64 t0 = now > cost ? now - cost : 0;
		const u64 start = t0 > m_busy ? t0 : m_busy;
		m_busy = start + u64(nwrote < 1 ? 1 : nwrote) * cost;
		return (m_busy + 32) / 64;
	}

	void key_on(const slot_bits &mask)
	{
		if (debug_on())
			std::fprintf(stderr, "keyon clock=%llu mask=%016llx:%016llx\n", (unsigned long long)m_clock,
			             (unsigned long long)mask.w[1], (unsigned long long)mask.w[0]);
		static const u32 MASK_REG[4] = { 0x1cf, 0x1ce, 0x18f, 0x18e };
		// チップごとに押す。押す声の無いチップには触らない
		for (int chip = 0; chip < 2; chip++) {
			const u64 m = mask.w[chip];
			if (!m)
				continue;
			const u32 base = u32(chip) * 0x1000;
			for (int i = 0; i < 4; i++)
				m_poke(base + MASK_REG[i], u16((m >> (i * 16)) & 0xffff));
			m_poke(base + 0x20e, 1);
		}
		// **音程の包絡線の行き先はキーオンの「あと」に書く**。チップは
		// キーオンのときの `0x10` を初めの高さとして取り込むので、
		// 先に書いてしまうと包絡線が無くなる（実機も 15 サンプル後に書く）
		for (int i = 0; i < SLOTS; i++) {
			if (!mask.test(i))
				continue;
			if (m_slot[i].peg_tgt != 0xffff)
				m_poke(u32(i) * 64 + 0x10, m_slot[i].peg_tgt);
			m_slot[i].peg_tgt = 0xffff;
			m_slot[i].keyed = true;
			if (drum_debug_on() && !m_slot[i].elem) {
				const slot_use &d = m_slot[i];
				std::fprintf(stderr, "[drum] keyon part=%d note=%d slot=%d clock=%llu（受けてから %llu）\n",
				             d.part, d.keynote, i, (unsigned long long)m_clock,
				             (unsigned long long)(m_clock - d.tstart));
				m_drum_watch.push_back({ i, d.inst, d.part, d.keynote, m_clock + 441 });
			}
		}
		// 押す前に届いていた離しを、いま効かせる（slot_use::keyed）
		for (int i = 0; i < SLOTS; i++) {
			if (!mask.test(i) || !m_slot[i].off_wait)
				continue;
			m_slot[i].off_wait = false;
			if (m_slot[i].on)
				release_slot(i);
		}
	}

	// 調べ用（drum_debug_on）: 押した打を 10ms 後に確かめる
	struct drum_watch { int slot; u32 inst; int part; int note; u64 due; };
	std::vector<drum_watch> m_drum_watch;
	void drum_watch_check(u64 clock)
	{
		size_t w = 0;
		for (size_t k = 0; k < m_drum_watch.size(); k++) {
			const drum_watch &d = m_drum_watch[k];
			if (d.due > clock) {
				m_drum_watch[w++] = d;
				continue;
			}
			const slot_use &s = m_slot[size_t(d.slot)];
			const bool same = s.inst == d.inst && s.part == d.part && s.keynote == d.note;
			const bool active = m_slot_peek ? m_slot_peek(d.slot) : true;
			const bool held = m_slot_held ? m_slot_held(d.slot) : true;
			std::fprintf(stderr, "[drum] +10ms part=%d note=%d slot=%d チップ %s / 空き扱い %s / %s\n",
			             d.part, d.note, d.slot, active ? "鳴っている" : "**鳴っていない**",
			             held ? "いいえ" : "はい",
			             same ? "まだこの打" : "**別の音に取られた**");
		}
		m_drum_watch.resize(w);
	}

	poke_fn m_poke;
	peek_fn m_peg_peek;
	peek_fn m_slot_peek;
	peek_fn m_slot_held;
	const u8 *m_rom = nullptr;
	const u8 *m_ram = nullptr;
	u8 *m_ramw = nullptr;           // 同じワーク RAM（Rnd の種を書き戻す用）
	// 写し取り無しで鳴らすときの合成の写し（`nocal_mode`）
	mutable std::vector<nv::voice_cal> m_synth;
	std::unordered_map<u64, std::vector<nv::voice_cal>> m_cal;
	std::unordered_map<u64, std::vector<nv::voice_cal>> m_drum;
	std::array<slot_use, SLOTS> m_slot;
	std::array<part_cc, PARTS> m_cc;
	// 前に sync_cc() で見たワーク RAM の値。ここから動いていれば
	// firmware が書き替えたということ
	struct ram_seen { u8 vol = 0, expr = 0, pan = 0, mod = 0, rev = 0, cho = 0, bri = 0, res = 0,
	                    atk = 0, dec = 0, rel = 0, vrate = 0, vdep = 0, vdly = 0; };
	std::array<ram_seen, PARTS> m_seen{};
	// 自分で引いた音色（0 なら引けていない）と、ドラムかどうか（-1 なら分からない）
	// firmware がそのスロットに最後に書いた時刻（+1。0 は触っていない）
	u64 m_fw_touch[SLOTS] = {};
	// **実機の `0x43E96E`**。旋律の音を鳴らすたびに byte10 で書き換わり、
	// ドラムはその値を拾うだけ（6.179）
	u16 m_peg_flag = 0;
	// **鍵ごとのアフタータッチの値**（6.191）。実機はパートの塊の
	// +0x82 から 62 鍵分を持っている。こちらは 128 鍵分持っておく
	std::array<std::array<u8, 128>, PARTS> m_pat{};
	// 割り当ての控え（6.195）。ASN_BASE の並び・36 本
	std::array<std::array<u8, ASN_N>, PARTS> m_asn{};
	std::array<u64, PARTS> m_asn_have{};
	// **ドラムのセットアップを MIDI で書いた印**（6.180）。組 × 鍵 ごとに、
	// ワーク RAM の並びの番号（0-22。ram::drum_setup_index）のビット
	std::array<std::array<u32, 128>, ram::DRUM_SETUP_SETS> m_drum_touch{};
	// **書いた値も覚えておく**。native の口では firmware を
	// 100ms につき 5ms しか回さないので、打つ時点ではまだ
	// ワーク RAM が書き換わっていない（6.180）
	std::array<std::array<std::array<u8, ram::DRUM_SETUP_PARAM>, 128>, ram::DRUM_SETUP_SETS> m_drum_val{};
	static constexpr u64 FW_KEEP = 44100 * 2;   // 2 秒は firmware のものとみなす
	// 離したあと、つまみの動きを追い続ける長さ。いちばん遅い離しでも
	// これだけあれば鳴り終わる（それ以上はスロットを取り直しているはず）
	static constexpr u64 REL_FOLLOW = 44100 * 8;
	// **サンプル＆ホールドを離したあとも回す長さ**（6.206）
	static constexpr u64 SH_TAIL = 44100 / 4;
	// 実機の包絡線は 441 サンプル（10ms）の格子で進む
	static constexpr u64 FENV_TICK = 441;
	u32 m_eg_phase = 0;
	bool m_eg_have = false;
	u16 m_eg_best = 0;
	std::array<u16, FENV_TICK> m_eg_hits{};
	std::array<u32, PARTS> m_recsel{};
	std::array<s8, PARTS> m_recsel_drum{};
	u64 m_clock = 0;
	u64 m_alt_kill_next = ~u64(0);  // つぎに止めを刺す時刻（6.151）
	int m_peak = 0;
	u32 m_cal_missing = 0;
	bool m_traj = false;
	bool m_rec = false;            // 写し取りの最中（段が後から増える）
	u64 m_traj_next = 0;           // つぎに段を書く時刻
	// 遅らせて鳴らす要素（byte72）。時が来たら key_on する
	struct pending_key { slot_bits mask; u64 at; };
	std::vector<pending_key> m_pend;
	// firmware がレジスタを書き終える時刻（1/64 サンプル単位）
	u64 m_busy = 0;
	u32 m_inst = 0;                 // 押した回数（同じ鍵を重ねたとき用）
	// 格子に乗せたベンドの、パートごとの流す時刻（0 は無し）
	std::array<u64, PARTS> m_bend_due{};
	u64 m_bend_next = ~u64(0);
	u64 m_age = 0;
};

} // namespace xg

#endif // S_MU2000_XG_NATIVE_DRIVER_H
