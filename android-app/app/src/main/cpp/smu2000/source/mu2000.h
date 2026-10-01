// license:BSD-3-Clause
//
// MU2000 一台ぶんの組み立て。
//
// MAME の src/mame/yamaha/ymmu2000.cpp（mu500_state / mu1000_state /
// mu2000_state）に当たるもの。machine_config と address_map で書かれていた
// 配線を、素のコードに置き換えてある。

#ifndef S_MU2000_MU2000_H
#define S_MU2000_MU2000_H

#pragma once

#include "smartmedia.h"
#include "state.h"
#include "xg/native_driver.h"
#include "compat/mamecompat.h"
#include "compat/membus.h"
#include "mame/cpu/sh7042.h"
#include "mame/sound/swp30.h"
#include "mame/machine/sci4.h"
#include "mame/video/hd44780.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cstring>
#include <atomic>
#include <array>
#include <deque>
#include <deque>
#include <map>
#include <memory>
#include <thread>
#include <string>

class mu2000
{
public:
	mu2000();
	~mu2000();

	// ---- ROM。どれも利用者が自分の実機から吸い出したもの

	// ROM は読むだけなので、何台の MU2000 で分け合っても構わない。
	// 一度読んだものを渡せば、読み直しも 36MB の複製もしなくて済む
	using u8rom  = std::shared_ptr<std::vector<u8>>;
	using u16rom = std::shared_ptr<std::vector<u16>>;
	u8rom  program_rom() const { return m_prog; }
	u8rom  wave_rom()    const { return m_wave; }
	u16rom sintab_rom()  const { return m_sintab; }
	void set_program_rom(u8rom p);
	void set_wave_rom(u8rom p);
	void set_sintab_rom(u16rom p);
	u8rom  lcd_font()    const { return m_lcd_font; }
	void set_lcd_font(u8rom p);
	// 代用フォントに欠けているレベルメータの字を規則から起こす
	static void fill_missing_glyphs(std::vector<u8> &rom);

	// CPU から見えるままの 4MB（MU2000 リポジトリの roms/mu2000_flash.bin）
	bool load_program(const std::string &path);
	// 波形 ROM 32MB。ic49/ic50/ic53/ic54 を 32bit 語に組む
	bool load_wave(const std::string &dir);
	// MEG が使う sin 表。まだ実機から取れていないので代用品でもよい
	bool load_sintab(const std::string &path);
	// LCD の文字の絵（HD44780U B04 の CGROM 4KB）。無くても音は出る
	bool load_lcd_font(const std::string &path);

	void reset();

	// ワーク RAM（0x400000-0x43ffff、256KB）。実機では電池で保持される。
	// MAME も NVRAM としてこれを保存している（ymmu2000.cpp）。
	// 入れるのは reset() の前。大きさが違えば false
	const std::vector<u8> &nvram() const { return m_ram; }
	bool set_nvram(const u8 *p, size_t n)
	{
		if (n != m_ram.size())
			return false;
		std::memcpy(m_ram.data(), p, n);
		return true;
	}

	// 状態の保存と復元。**機械まるごと**（CPU・RAM・SWP30・LCD・タイマ）。
	// ROM は入れないので、戻すときは同じ ROM を積んでおくこと。
	// 正しさは「戻した続きの音が、戻さず走り続けた音と 1 バイトも
	// 違わないこと」で確かめる（tools/state_test.py）
	std::vector<u8> save_state() const;
	void state(state_io &s);
	bool load_state(const u8 *p, size_t n, std::string &err);
	// いま書き出す形の版。起動後の写し（bootcache.h）の鍵に混ぜる
	static u32 state_version();

	// n サイクルぶん進める。周辺のイベントはこの中で挟む
	void run_cycles(u64 n);

	// S-MU2000: SH-2 を回さずに SWP30 だけ進める（レジスタ列の再生。doc/native-engine.md）
	void set_cpu_enabled(bool on) { m_cpu_enabled = on; }
	// 記録した書き込みを、外から SWP30 へ入れる（master=false でスレーブ）
	void poke_swp(bool master, u32 reg, u16 value)
	{ (master ? m_swpm : m_swps).write16(reg, value); }

	// MIDI の入口。実機の DIN は **A と B の 2 口**で、それぞれ SH7043 の
	// 内蔵 SCI ch0 / ch1 に繋がっている（docs/hardware.md）。
	// パートは A が 1-16、B が 17-32。
	// C と D は USB（M37640 マイコン）側の口で、パートは 33-48 / 49-64。
	// そちらは usb.h の代役を通す（doc/dump/usb.md「MIDI C/D の口」）
	static constexpr int MIDI_DIN_PORTS = 2;
	static constexpr int MIDI_PORTS = 4;

	// 受信が有効になったか。firmware が起動を終えた印。
	// これを待たずに流すと、曲頭のリセットや音色指定が全部捨てられる
	bool midi_ready(int port = 0) const { return m_cpu->sci(port)->rx_enabled(); }
	void set_fast_midi(bool fast) { m_fast_midi = fast; }

	// 1 バイト送る。既定では実機と同じ 31250bps の直列で流れる。
	// fast MIDI では firmware が前のバイトを読むと、待たずに次を渡す。
	// 仮想の口で MIDI の輪ができると際限なく積まれるので、上限を超えたら捨てる。
	//
	// 上限は**実際の演奏では届かない大きさ**にしておく。firmware がさばけるのは 1 秒に 3kB ほど
	// （ピッチベンドなら 1,040 個）で、DAW でホイールを回すとそれを超えて溜まる。前は 65,536 バイトで
	// 捨てていて、16 チャンネルにブロックごとのベンドを 15 秒流す（124kB）と 10,634 バイト捨て、
	// その中のノートオフが消えて音が鳴りっぱなしになった（issue #18）。同じ MIDI を実機に USB で
	// 送ると、何も失わずに約 40 秒遅れて全部さばき、後の音も普通に鳴って止まる（2026-09-17）。
	// 4MB はさばく速さで 20 分以上ぶん。輪ができても gui の THRU は流量を絞っている（midi_guard.h）
	static constexpr size_t MIDI_QUEUE_LIMIT = size_t(1) << 22;
	//
	// ケーブルメッセージ `F5 nn`（nn = 1-4）を受けると、その入口から後に来るバイトを口 nn へ回す。
	// MU80/MU100/MU128 の TO HOST と S-YXG50 の流儀で、1 本の入口から 64 パート全部に届く（issue #24）。
	// `F5 nn` 自体は firmware に渡さない。実機の MU2000 は USB で PC から送った F5 を無視する
	// （2026-09-17 に実機で確かめた）が、そのまま渡すと firmware の USB の受け口（0x042932）が
	// 口の切り替えと読み、こちらが挟む `F5 <口>` と食い違う。範囲外の nn は読み捨てて口を変えない。
	// 戻り値はバイトを回した口。`F5 nn` を読んだときは -1
	int midi_in(u8 byte, int port = 0)
	{
		if (port < 0 || port >= MIDI_PORTS)
			port = 0;
		// native の口が動いているときは、鍵の上げ下げをこちらで処理する
		// （firmware に渡さない）。詳しくは xg/native_driver.h
		if (m_native_engine && native_midi(byte, port))
			return port;
		if (byte < 0xf8) {                     // リアルタイムは F5 と nn の間に挟まってもよい
			if (m_cable_wait[port]) {
				m_cable_wait[port] = false;
				if (!(byte & 0x80)) {
					if (byte >= 1 && byte <= MIDI_PORTS)
						m_cable[port] = byte - 1;
					return -1;
				}
			}
			if (byte == 0xf5) {
				m_cable_wait[port] = true;
				return -1;
			}
		}
		const int to = m_cable[port];
		if (to >= MIDI_DIN_PORTS || m_usb_host)
			usb_midi_in(byte, to);
		else if (m_midi[to].queue.size() < MIDI_QUEUE_LIMIT)
			m_midi[to].queue.push_back(byte);
		else
			m_midi_dropped.fetch_add(1, std::memory_order_relaxed);
		return to;
	}
	// 溢れて捨てたバイト数（どの糸から読んでもよい）
	u64 midi_dropped() const { return m_midi_dropped.load(std::memory_order_relaxed); }
	// Bytes sitting on the wire, including the one in flight.
	// The 31250bps throttle asks this to decide whether the line is free
	size_t midi_queued(int port) const
	{
		const midi_line &m = m_midi[port == 1 ? 1 : 0];
		return m.queue.size() + (m.bit >= 0 ? 1 : 0);
	}
	size_t midi_pending() const
	{
		size_t pending = m_usb.rx.size() + (m_usb.have ? 1 : 0);
		for (const midi_line &m : m_midi)
			pending += m.queue.size() + (!m_fast_midi && m.bit >= 0 ? 1 : 0);
		if (m_fast_midi)
			for (int port = 0; port < MIDI_DIN_PORTS; port++)
				pending += m_cpu->sci(port)->rx_byte_pending() ? 1 : 0;
		return pending;
	}
	bool midi_idle(int port) const
	{
		if (port >= MIDI_DIN_PORTS || m_usb_host)
			return usb_idle();
		return m_midi[port].queue.empty() &&
			(m_fast_midi ? !m_cpu->sci(port)->rx_byte_pending() : m_midi[port].bit < 0);
	}
	bool midi_idle() const
	{
		if (!usb_idle())
			return false;
		for (const midi_line &m : m_midi)
			if (!m.queue.empty() || (!m_fast_midi && m.bit >= 0))
				return false;
		if (m_fast_midi)
			for (int port = 0; port < MIDI_DIN_PORTS; port++)
				if (m_cpu->sci(port)->rx_byte_pending())
					return false;
		return true;
	}

	// ---- USB（M37640）の代役
	//
	// 実機の MIDI IN C・D は USB 側のマイコンが受けて、SH-2 へは 0xF80000/0xF80001 の
	// 2 番地と割り込み 2 本だけで渡している。渡されるのは**ただの MIDI バイト列**で、
	// その中に `F5 <口>` が挟まって口が切り替わる（口は 1 始まりで 1=A 2=B 3=C 4=D）。
	// マイコン自身の ROM は要らない。詳しくは doc/dump/usb.md
	//
	// ただし firmware は HOST SELECT が USB のときしか C・D を通さないので、
	// この口を使うなら set_usb_host(true) を**起動前に**呼ぶこと。そのときは
	// A・B も USB 側を通る（実機で DIN が黙るのと同じ）
	void set_usb_host(bool on) { m_usb_host = on; }
	bool usb_host() const { return m_usb_host; }
	bool usb_idle() const { return m_usb.rx.empty() && m_usb.cmd.empty() && !m_usb.have; }
	// firmware が USB へ出したバイト。口は 0 始まり（-1 は口の指定より前）
	bool usb_out_take(u8 &v, int &port);

	// MIDI OUT。実機の OUT 端子で、SH7043 の SCI ch0 の送信線に繋がっている
	// （MAME の ymmu2000.cpp と同じ）。firmware が送り出したもの
	// （XG の問い合わせやダンプ要求への返事など）を 1 バイトずつ取る。
	// **run_sample と同じ糸から呼ぶこと**。溜めは 4096 バイトで、溢れたら捨てる。
	// 状態の保存には入れない（読み戻したときは空から始まる）
	bool midi_out_take(u8 &v)
	{
		// USB を使っているときは、firmware は返事も USB 側へ出す（DIN の
		// MIDI OUT は黙る）。呼ぶ側から見た「音源が出したもの」は同じなので、
		// ここで拾い分ける
		if (m_usb_host) {
			int port;
			return usb_out_take(v, port);
		}
		if (m_tx_r == m_tx_w)
			return false;
		v = m_tx_buf[m_tx_r];
		m_tx_r = (m_tx_r + 1) & TX_MASK;
		return true;
	}

	// スレーブの SWP30 を別スレッドで回すか。
	// 2 個の SWP30 は 1 サンプルの中では互いに独立している（相手の出力は
	// 前サンプルのものしか使わない）ので、並べて走らせても結果は変わらない。
	// 別スレッドにするのは、動いている台数が論理コア数の 1/4 以下のときだけ（SMU2000_THREADED_MAX）。
	// 台数が増えたら run_sample の中で 1 本に戻し、減ったらまた別スレッドにする
	void set_threaded(bool on);
	bool threaded() const { return m_slave_thread.joinable(); }

	// 1 サンプル（44.1kHz 相当）ぶん進めて、DAC 出力を返す。
	// 値は MAME 内部と同じ目盛りで、全振幅が DAC_FULL_SCALE。
	// 16bit にするときは >> 2（MAME の put_int_clamp(..., 1<<17) と同じ）
	static constexpr s32 DAC_FULL_SCALE = 1 << 17;
	void run_sample(s32 &left, s32 &right);

	// A/D INPUT に入れる音。次の run_sample の 1 サンプルぶんで、16bit の目盛り（±32768 が全振幅）。
	// 左が AD1、右が AD2。A/D パート（スレーブの MELI 6/7）と、サンプリングの録音（REC の InputSrc で選ぶ）、
	// レベルメーター（CPU の AN0 / AN2）に使う
	void set_audio_input(s32 ad1, s32 ad2) { m_ad_in[0] = ad1; m_ad_in[1] = ad2; }

	// 前面のカードの差し込み口（SmartMedia）。create / load で差し、eject で抜く。
	// 中身は状態の保存に入れないので、使う側がファイルに書き出す（take_dirty_blocks / write_blocks）
	smu2000::smartmedia &card() { return m_card; }
	// サンプリング RAM（4MB）。確かめる用
	const std::vector<u8> &sample_ram() const { return m_sampram; }

	// ---- S-MU2000: パートの音（画面のスペクトラム用）
	// 声（2 つのチップで 128）の出力を、混ぜる前に拾ってパートごとに足す。見たいパートを
	// 決めたときだけ動く（-1 で止める）。声 → パートは、firmware が鳴らした声なら firmware の
	// 声の表（ワーク RAM 0x424386 + 声 × 148 にパートの塊の番地。実測で 11387 回とも一致）、
	// native が鳴らしている声なら native_driver が持つもの。音には触らない
	static constexpr size_t SCOPE_N = 4096;       // 溜めておくサンプル数（2 の冪）
	void set_scope_part(int part);
	int scope_part() const { return m_scope_part.load(std::memory_order_relaxed); }
	// 直近の n サンプル（n ≤ SCOPE_N、古い順）。読み手は画面の糸。途中の値が混ざってもよい
	void scope_read(float *out, size_t n) const;
	// 同じパートの、インサーションを通したあとの音（MEG の出口）。そのパートにインサーションが
	// 付いていなければ scope_read と同じ（声の和）。目盛りは声の和にそろえてある。
	// 返り値は通しているインサーションの番号（1-4。無ければ 0）
	int scope_read_post(float *out, size_t n) const;
	// 見ているパートに付いているインサーション（1-4。無ければ 0）
	int scope_insertion() const { const int s = m_scope_ins.load(std::memory_order_relaxed); return s < 0 ? 0 : s + 1; }
	// エフェクトごとの入口（MEG への送り）と出口。見たいパートを決めているあいだだけ溜める。
	// インサーションとインサーション接続のバリエーションは掛けたパートだけの音、
	// システムのリバーブ・コーラス・バリエーションは全パートの送りを混ぜた音。MIX は戻りと乾いた音を混ぜた
	// マスター EQ の前（出口のみ意味がある）。どれも左右の平均
	enum scope_fx : int { SCOPE_INS1, SCOPE_INS2, SCOPE_INS3, SCOPE_INS4, SCOPE_VAR, SCOPE_CHO, SCOPE_REV, SCOPE_MIX, SCOPE_FX_N };
	void scope_read_fx(int fx, bool out, float *dst, size_t n) const;

	// ---- S-MU2000: 全パートの音と最終の出力（一覧の小さなスペクトラム用）
	// 上の 1 パートぶんと同じく、声の出力を混ぜる前に拾ってパートごとに足す。こちらは 64 パート
	// 全部を同時に、短い輪（PSCOPE_N）で持つ。最終の出力はマスタの DAC に出る左右の平均。
	// 一覧が見えている間だけ動かす（set_part_scopes(false) で止まる）。音には触らない
	static constexpr size_t PSCOPE_N = 1024;
	void set_part_scopes(bool on);
	// part 0-63 はそのパートの声の和、PSCOPE_OUT は最終の出力。直近の n サンプル（n ≤ PSCOPE_N、古い順）
	static constexpr int PSCOPE_OUT = 64;
	void part_scope_read(int part, float *out, size_t n) const;

	sh7043a_device &cpu()  { return *m_cpu; }
	swp30_device   &swpm() { return m_swpm; }

	// S-MU2000: エフェクトを C++ で鳴らす軽量モード（doc/native-dsp.md）。
	// 既定は切。入れると MEG のエフェクトは無音を受け、代わりに dsp::native_fx が鳴る
	//   0 切 / 1 エフェクトだけ C++（MEG も回る）/ 2 MEG を回さない（いちばん軽い）
	void set_native_fx(int mode);
	int native_fx() const { return m_nfx_on; }
	swp30_device   &swps() { return m_swps; }
	hd44780_device &lcd()  { return m_lcd; }

	// ---- フロントパネル

	// パネルのボタン。MAME の mu500 の入力ポートと同じ並び。
	// firmware は m_ledsw1 で行を選び、押されている桁を 0 で読む
	enum class button {
		strings, bass, guitar, organ, chrom_perc, piano,
		synth_pad, synth_lead, pipe, reed, brass, ensemble,
		drum, model_excl, sfx, percussive, ethnic, synth_effects,
		part_plus, part_minus, mute_solo, effect, util, edit, play,
		value_plus, value_minus, exit, select_right, select_left, enter, seq,
		audition, select, sampling_mode,
		count
	};
	static const char *button_name(button b);
	void set_button(button b, bool pressed);
	bool button_pressed(button b) const;

	// 前面の大きなダイヤル（ロータリーエンコーダ）。正が右回り。
	// 線はポート A の bit17（A 相）と bit16（B 相）。
	// firmware は 2.5ms ごとにここを読み、**A が立っていれば 1 目盛り**、
	// 向きは B（0 で増、1 で減）で決める。実測でそう決まっている。
	// 走査 1 回につき 1 目盛りなので、最大 400 目盛り/秒
	void turn_encoder(int detents)
	{
		m_enc_pending += detents;
		panel_touched();
	}
	bool encoder_busy() const { return m_enc_pending != 0; }

	// パネルの LED 10 個。MAME の mulcd_device::set_leds と同じ並び
	u16 leds() const;
	// UTIL > SYS の Contrast（1-8）。firmware が d80000 の下 3bit に
	// 「値 − 1」を書く。まだ書かれていなければ工場出荷の 2
	int lcd_contrast() const { return m_d80 ? (m_d80 & 7) + 1 : 2; }

	const std::string &error() const { return m_error; }

	void print_swp_widths() const
	{ std::printf("SWP30 アクセス: 書き byte %llu / word %llu / dword %llu、読み byte %llu\n",
	              (unsigned long long)m_swp_w8, (unsigned long long)m_swp_w16,
	              (unsigned long long)m_swp_w32, (unsigned long long)m_swp_r8); }

	// SWP30 への書き込みを全部書き出す（MAME と突き合わせるため）
	void set_swp_trace(std::FILE *f, bool with_reads = false)
	{ m_swp_trace = f; m_swp_trace_reads = with_reads; }

	// **firmware を走らせない口**（doc/native-engine.md の段 2）。
	// 1: 鍵の上げ下げを native driver でさばき、CPU はその間止める
	void set_native_engine(int mode);
	int native_engine() const { return m_native_engine; }

	// 画面へ渡す液晶の絵（hd44780::render と同じ並び）。native の口で
	// firmware を細く回している間は、覚えた点滅をこちらで切り替えて描く
	const u8 *lcd_render();
	// native の口の内訳（調べ用）
	// SH-2 を回したのはなぜか（サンプル数）。doc/native-engine.md の 6.21
	std::atomic<u64> m_ne_by_note{0};    // firmware が鳴らしている音がある
	std::atomic<u64> m_ne_by_sysex{0};   // SysEx のあと
	std::atomic<u64> m_ne_by_other{0};   // 音色の指定・CC など
	std::atomic<u64> m_ne_by_learn{0};   // 写し取り（その音色の 1 音目）
	std::atomic<u64> m_ne_by_midi{0};    // 渡した MIDI を受け取らせている
	std::atomic<u64> m_ne_by_keep{0};    // 止めきらないために細く回している
	std::atomic<u64> m_ne_by_panel{0};   // パネル（ボタン・ダイヤル・液晶）を触っている
	u8   m_fw_why = 0;                   // いまの hold の理由（1 SysEx / 2 そのほか）
	// SysEx の頭を少し覚えて、長く回す必要があるかを見分ける
	int  m_sx_pos = -1;
	// XG のパラメータチェンジは 43 1n 4C hh mm ll dd… の形。パートの設定
	// （08 pp ll）は自分でも効かせたいので、値まで取っておく
	u8   m_sx[24] = {};

	struct native_stats { u64 note_native = 0, note_fw = 0, learn = 0, other = 0; };
	native_stats native_counts() const { return m_ne_stats; }

	// **写し取りをファイルに残す・戻す**（voicecache.h）。
	// これがあれば、2 回目からは 1 音目も native で鳴らせる
	std::vector<u8> native_cal_save() const;
	bool native_cal_load(const u8 *data, size_t n);
	size_t native_cal_count() const { return m_ndrv.cal_count(); }
	int native_peak_slots() const { return m_ndrv.peak_slots(); }
	u32 native_cal_missing() const { return m_ndrv.cal_missing(); }

	struct native_why { u64 total, by_note, by_sysex, by_other, by_learn, by_midi, by_keep; };
	native_why native_why_counts() const
	{
		return { m_ne_samples.load(std::memory_order_relaxed),
		         m_ne_by_note.load(std::memory_order_relaxed),
		         m_ne_by_sysex.load(std::memory_order_relaxed),
		         m_ne_by_other.load(std::memory_order_relaxed),
		         m_ne_by_learn.load(std::memory_order_relaxed),
		         m_ne_by_midi.load(std::memory_order_relaxed),
		         m_ne_by_keep.load(std::memory_order_relaxed) };
	}

	// native の口が、いま firmware を回している割合（0-1。小さいほど軽い）
	double native_firmware_share() const
	{
		const u64 t = m_ne_samples.load(std::memory_order_relaxed);
		return t ? double(m_ne_fw_samples.load(std::memory_order_relaxed)) / double(t) : 0.0;
	}

	// SWP30 への書き込みを、その場で拾う（掃引の道具用。doc/native-engine.md の段 1）
	using swp_watch_fn = std::function<void(bool master, u32 reg, u16 value)>;
	void set_swp_watch(swp_watch_fn fn) { m_swp_watch = std::move(fn); }

private:
	void build_bus();
	void start_devices();

	machine_config  m_config;
	running_machine m_machine;   // 時計とタイマの置き場
	required_device<sh7043a_device> m_cpu_finder;
	sh7043a_device *m_cpu = nullptr;

	bool m_cpu_enabled = true;     // false なら SH-2 を回さない（再生のとき）

	// ---- native の口（段 2）
	int  m_native_engine = 0;
	// **リセットが効き終わるまで、こちらの発音を待たせる**（issue #51）。
	// 実機は firmware が MIDI を順番に処理するので、リセットの直後に並んだ打鍵は
	// リセットのあとで鳴る。こちらは打鍵を自分でさばくため、待たせないと先に鳴り、
	// あとから終わる firmware のリセットに消される（曲頭が丸ごと無音になる）。
	// 決め打ちの秒数で待つのではなく、**firmware が SWP30 を触らなくなったら**解く。
	// 取り逃しても期限で必ず解ける
	bool   m_ne_reset_hold = false;      // いま待たせているか
	size_t m_ne_reset_free = 0;          // 待たせる前から並んでいた分（これだけは流す）
	u64    m_ne_reset_deadline = 0;      // これを過ぎたら必ず解く
	u64    m_fw_swp_at = 0;              // firmware が最後に SWP30 を触った時刻
	// **同じ値の CC が続いたときに firmware を起こし直さないため**の控え。
	// CC は 1 つ来るたびに firmware を 2〜20ms 全速で回すので、同じ値が並ぶ曲
	// （実測: 実曲の CC の 2 割が「直前と同じ番号・同じ値」）では回りっぱなしになる。
	// **バイトは今までどおり線に流す**ので、実機の時間の進み方は変わらない。
	// 0xff は「まだ見ていない」。リセットで忘れる
	u8     m_cc_last[64][128];
	static constexpr u64 RESET_QUIET = 44100 / 50;    // 20ms 触らなければ「終わった」
	static constexpr u64 RESET_HOLD_MAX = 44100 * 2 / 5;   // 400ms で必ず解く
	void hold_after_reset(u64 fire);
	u32  m_fw_hold = 0;            // このサンプル数だけ firmware を回す

	// **液晶の点滅を native で受け持つ**。点滅（カーソル・値・▼）は firmware が
	// 時間を数えて書き換えるので、firmware を細く回すと 20 分の 1 の速さになり
	// 止まって見える。firmware が全速のときに「2 つの値を一定の間隔で行き来する
	// マス」を覚え、細く回している間はその間隔でこちらが切り替えて描く。
	// マスは DDRAM 0x00-0x7F と CGRAM 0x80-0xBF
	// 点いている時間と消えている時間は同じとは限らない（演奏画面の ▼ は
	// 点いて 325ms・消えて 75ms）ので、2 つの値それぞれの長さを覚える
	struct blink_cell {
		u8  v[2] = {};             // 行き来する 2 つの値
		u64 dur[2] = {};           // それぞれが続く長さ（firmware の時刻、サンプル）
		u8  count = 0;             // 同じ長さで続いた回数
		bool on = false;           // 点滅とみなしている
		u64 last_fw = 0;           // 最後に書き換わった firmware の時刻
		u64 anchor = 0;            // 切り替えの起点（実時間 = m_ne_clock）
		u8  anchor_i = 0;          // 起点で出ていた値（v の添字）
	};
	blink_cell m_blink[0xC0];
	u64  m_fw_clock = 0;           // firmware を回したサンプル数（firmware の時刻）
	u64  m_thr_fw0 = 0;            // 細く回しているかを測る窓の頭の m_fw_clock
	bool m_throttled = false;      // いま firmware を細く回している
	void blink_learn();
	// 調べ用の切り替えは**作るときに 1 回だけ読む**。run_sample から
	// `std::getenv` を呼ぶと、それだけで 1 サンプルあたり 1µs 以上かかる
	// （環境の表を毎回なめるため。doc/native-dsp.md「測るときの注意」と同じ罠）
	const bool m_fw_always = std::getenv("SMU2000_FW_ALWAYS") != nullptr;
	const bool m_meter_dbg = std::getenv("SMU2000_METER_DBG") != nullptr;
	std::atomic<u64> m_ne_samples{0}, m_ne_fw_samples{0};
	xg::native_driver m_ndrv;
	// 写し取り中の状態
	bool m_learning = false;
	u32  m_learn_rec = 0;
	std::map<u32, u16> m_learn_first, m_learn_last;
	u64  m_learn_mask = 0, m_learn_keyed = 0;
	// 写し取りの間の、フィルタ・LFO の動き（鍵を押した瞬間からの時刻つき）。
	// 写し取りが終わってから録り始めると、**最初の数十 ms が抜ける**
	u64  m_learn_key_clock = 0;
	std::vector<std::pair<int, xg::nv::fstep>> m_learn_traj;
	int  m_learn_left = 0;         // 残りサンプル数
	int  m_learn_want = 1;         // 鳴るはずの要素の数（そろうまで待つ）
	// **実機と同じだけ遅らせる**（doc/native-engine.md の 6.16）。
	// firmware は MIDI を受けてから 74 サンプル（1.68ms）後に鳴らす。native も同じ
	// だけ待たないと、同じ曲の中で native の音だけ 1.7ms 早く出てしまう
	// 内訳: MIDI は 1 バイト 10 ビット・31250 baud なので 14.1 サンプルかかる。
	// 3 バイトの鍵で 42 サンプル、残り 32 サンプルが firmware の中の手間
	static constexpr u32 NATIVE_DELAY = 74;
	// **バイトを受け終えてから鳴るまで**（1/64 サンプル単位）。
	// 実機の遅れは 72 と 73 を行き来する ＝ 端数がある。整数で足していた
	// ころは必ず 73 になり、1 サンプルずれる音が出ていた（doc の 6.92）。
	// `SMU2000_NATIVE_PROC` で振れる（1/64 サンプル単位）
	// **離しの処理にかかる時間**（1/64 サンプル）。押しとは別に持つ。
	//
	// 押しは 32 サンプル（音色を引いて要素を組み立てる）かかるが、
	// **離しは 2 サンプル**だった。実機は最後のバイトを受けてすぐ 0x09 を
	// 書いている。押しと同じ 32 にしていたので、**すべての離しが
	// 30 サンプル遅れていた**（旋律もドラムも同じだけ遅れる。
	// doc/native-engine.md の 6.152）。`SMU2000_OFF_PROC` で振れる
	static u32 off_proc64()
	{
		static const u32 v = std::getenv("SMU2000_OFF_PROC")
		                   ? u32(std::atoi(std::getenv("SMU2000_OFF_PROC"))) : 2 * 64;
		return v;
	}
	// 重い SysEx（エフェクトの種類など）のあと、firmware を全速で回す長さ
	static u32 fx_hold()
	{
		static const u32 v = std::getenv("SMU2000_FX_HOLD")
			? u32(44100 * std::atoi(std::getenv("SMU2000_FX_HOLD")) / 1000)
			: u32(44100 * 3 / 10);
		return v;
	}
	static u32 native_proc64()
	{
		static const u32 v = std::getenv("SMU2000_NATIVE_PROC")
		                   ? u32(std::atoi(std::getenv("SMU2000_NATIVE_PROC"))) : 32 * 64;
		return v;
	}
	// 1 バイト（1/64 サンプル単位）。`SMU2000_RX_BYTE` で振れる（0 にすると
	// 和音の音が全部同じ時刻に出る。相対のずれを調べる用。doc の 6.78）。
	// **DIN は 31250 baud で 1 バイト 10 ビット ＝ 14.1 サンプル**、
	// **USB は実機で測った 10000 byte/s ＝ 4.41 サンプル**（6.218。19500 は
	// 実機 → PC の向きの値で、受けるほうはその半分だった）。
	// USB の口なのに DIN の速さで並べていたので、プラグイン（USB が既定）では
	// 音が 1 つにつき 37 サンプル遅れていた（doc/native-engine.md の 6.120）
	static u64 rx_byte_tick()
	{
		static const u64 v = std::getenv("SMU2000_RX_BYTE")
		                   ? u64(std::atoi(std::getenv("SMU2000_RX_BYTE"))) : 903;
		return v;
	}
	// **線の刻みは 1/8000 サンプルで数える**（doc/native-engine.md の 6.156）。
	// DIN の 1 バイトは 10 ビット / 31250 baud ＝ 28MHz で 8960 サイクル ＝
	// **ちょうど 14.112 サンプル**。1/64 では割り切れず（903.168）、
	// 切り捨てていたぶんが溜まって和音の 2 音目から 1 サンプル遅れていた。
	// 1/8000 なら 112896 でぴったり合う（USB の 4.40625 サンプルは 35250）
	static constexpr u64 RX_UNIT = 8000;
	static constexpr u64 RX_SCALE = RX_UNIT / 64;      // 1/64 → 1/8000
	static u64 rx_byte_tick8()
	{
		static const u64 v = std::getenv("SMU2000_RX_BYTE")
		                   ? rx_byte_tick() * RX_SCALE : 112896;
		return v;
	}
	static u64 rx_byte_tick_usb8() { return rx_byte_tick_usb() * RX_SCALE; }
	static u64 usb_sub8() { return usb_sub64() * RX_SCALE; }
	// 押しの処理時間。`SMU2000_NATIVE_PROC8` なら 1/8000 サンプルで振れる
	static u64 native_proc8()
	{
		static const u64 v = std::getenv("SMU2000_NATIVE_PROC8")
		                   ? u64(std::atoi(std::getenv("SMU2000_NATIVE_PROC8")))
		                   : u64(native_proc64()) * RX_SCALE;
		return v;
	}
	u64  m_rx_at[MIDI_PORTS] = {};                   // その口が次のバイトを受け終える時刻
	u64  m_rx_at_usb = 0;                            // USB の線（4 口で分け合う）
	u8   m_tick_seen = 0xff;                         // 10ms の印の前の値（6.145）
	int  m_rx_usb_port = -1;                         // USB で最後に選んだ口
	// kind 0=離し 1=押し 2=CC 3=ベンド 4=音色の指定 5=XG のパートの設定（08 pp d0=d1）
	struct nev { u64 at; u8 kind, part, d0, d1; };
	std::deque<nev> m_nq;
	u64  m_ne_clock = 0;
	// **native で鳴らしている鍵の数**（パート x 鍵）。ビット 1 つだと、
	// 同じ鍵を重ねて押されたとき（キーアサインがマルチの曲）2 回目以降の
	// 離しを取りこぼし、その音だけ鳴り残る（doc/native-engine.md の 6.149）
	u8   m_nown[64][128] = {};

	void native_pump();
	// 写し取った音の、フィルタの動きを録る（doc/native-engine.md の 6.17）
	// **フィルタの動きの録り**。同時に何本も走らせる。
	// 1 本しか持てなかったころは、次の音色の写し取りが始まると前の録りが
	// そこで切れていた。切れないように「録っている間は写し取りを始めない」
	// ようにしていたが、そうすると窓を延ばせず、押している間の包絡線が
	// 1 秒で止まっていた（doc/native-engine.md の 6.61）
	struct traj_rec {
		std::vector<xg::nv::voice_cal> *cals = nullptr;
		u64  start = 0;
		u32  left = 0;          // 0 なら空き
		u32  n = 0;
		u32  rec_key = 0;
		u32  ctx = 0;
		u64  drum_key = 0;
		s8   chan[64] = {};     // チャンネル → 何番目の写しか（-1 は関係なし）
		u64  rel_at[64] = {};   // そのスロットを離した時刻（0 はまだ）
	};
	static constexpr int TRAJ_MAX = 6;
	traj_rec m_trajs[TRAJ_MAX];
	bool traj_any() const
	{
		for (const traj_rec &t : m_trajs)
			if (t.left)
				return true;
		return false;
	}
	void traj_step();               // 1 サンプルぶん進める
	void traj_watch(u32 reg, u16 value);
	bool m_traj_rec = false;        // どれか 1 本でも録っているか（native_driver へ渡す用）
	// **写し取ったスロット → 写し取りの並びの番号**。写し取りを組むときに
	// 覚えておき、段の録画（traj）でそのまま使う。前はキーオンの順で
	// 数え直していたので、途中で捨てたスロットがあるとずれていた
	// （ドラムは 1 段も録れていなかった。doc/native-engine.md の 6.88）
	s8 m_learn_chan[64] = {};
	void traj_start(u32 rec, u64 drum_key, int ncal, u32 ctx);
	void traj_finish_one(int i);

	// **短すぎる写しは取り直す**。フィルタの動きは firmware に鳴らさせた
	// 1 音から録るので、その音が短いと途中で切れる。切れたぶんは native で
	// 鳴らすときに「そこで止まった音」になり、実機より暗い（利用者の曲で
	// 中域が 1dB 足りなかった）。何度か取り直して、いちばん長いものを使う
	static constexpr u32 TRAJ_ENOUGH = 60;   // 60 段 ＝ 0.6 秒ぶん
	static constexpr int TRAJ_TRIES  = 4;
	std::map<u64, int> m_traj_tries;
	// そのバイトを受け終える時刻を進めて、鳴らすべき時刻（サンプル）を返す
	// その口のバイトが USB を通るか（midi_in の振り分けと同じ見立て）
	bool rx_usb(int port) const
	{
		return m_usb_host || m_cable[port] >= MIDI_DIN_PORTS;
	}
	static u64 usb_sub64()
	{
		static const u64 v = std::getenv("SMU2000_USB_SUB")
		                   ? u64(std::atoi(std::getenv("SMU2000_USB_SUB"))) : 6 * 64;
		return v;
	}
	// 空いている USB に 1 バイト目が渡るまで（1/8000 サンプル）。6.218
	static u64 usb_hand8()
	{
		static const u64 v = std::getenv("SMU2000_USB_HAND")
		                   ? u64(std::atoi(std::getenv("SMU2000_USB_HAND"))) : 2 * RX_UNIT;
		return v;
	}
	static u64 rx_byte_tick_usb()
	{
		static const u64 v = std::getenv("SMU2000_RX_BYTE_USB")
		                   ? u64(std::atoi(std::getenv("SMU2000_RX_BYTE_USB"))) : 282;
		return v;
	}
	u64 rx_advance(int port)
	{
		const u64 now = m_ne_clock * RX_UNIT;
		// **USB は 4 つの口が 1 本の線を分け合う**（doc/native-engine.md の 6.126）。
		// DIN は口ごとに別の線なので別々に数えるが、USB では口 A のバイトが
		// 口 C のバイトを待たせる。口ごとに数えていたので、口 B・C・D の音が
		// 実機より 80-94 サンプル早く出ていた
		const bool usb = rx_usb(port);
		u64 &at = usb ? m_rx_at_usb : m_rx_at[port];
		const bool idle = at < now;              // 線が空いていた
		if (idle)
			at = now;
		u64 bytes = 1;
		// **口が変わると `F5 <口>` が 2 バイト挟まる**（usb_midi_in と同じ）。
		// 数えていないと、口をまたぐ曲でこちらだけ早く鳴る
		if (usb && port != m_rx_usb_port) {
			m_rx_usb_port = port;
			bytes += 2;
		}
		// **USB は空いていれば 1 バイト目をその場で渡す**（`usb_step` は
		// `now >= u.next` で渡すので、間が空いていれば待ち無し）。その 1 バイトぶんを
		// 足していたので、口の速さを実測の 10,000 byte/s にしたとき、firmware の道より
		// 3-4 サンプル遅れるようになった（6.218。19,500 のときは 1 サンプルで隠れていた）。
		// ただし渡すのは走らせる区切りの頭なので、まるまる 0 ではなく 2 サンプルほど遅れる
		// （`SMU2000_USB_HAND` で振れる）
		if (usb && idle) {
			bytes--;
			at += usb_hand8();
		}
		at += bytes * (usb ? rx_byte_tick_usb8() : rx_byte_tick8());
		// **USB の口 B・C・D は実機のほうが 6 サンプル遅い**（6.129）。
		// 口 A は合っている。DIN では 4 口とも同じなので、USB のときだけ。
		// 1 口だけ使う曲を 4 通り作って測った（`SMU2000_USB_SUB` で振れる）。
		// **`--bootcache` で測ってはいけない**。そちらだと 76 サンプルに
		// 見えるが、ほんとうに起動させると 6 だった（6.121 と同じ罠）
		const u64 extra = (usb && port > 0) ? usb_sub8() : 0;
		return (at + extra + native_proc8()) / RX_UNIT;
	}

	bool nown(int part, int note) const { return m_nown[part][note & 0x7f] != 0; }
	void nown_set(int part, int note, bool on)
	{
		u8 &n = m_nown[part][note & 0x7f];
		if (on) { if (n < 255) n++; }
		else    { if (n) n--; }
	}

	// **音色を自分で引く**（firmware の RAM を待たずに済む）。
	// バンクとプログラムをパートごとに覚えて、xg::voice_rom::lookup に渡す
	struct part_prog { u8 msb = 0, lsb = 0, prog = 0; };
	part_prog m_prog_sel[64];
	// **前に見たワーク RAM のバンクと音色**（パートの塊 +1/+2/+3）。
	// パネルのダイヤルや PART+/- で音色を替えると MIDI を通らないので、
	// ここを見張って拾い直す（doc/native-engine.md の 6.146）。
	// 0xff は「まだ見ていない」
	u8 m_prog_seen[64][3];
	void sync_prog();
	// **液晶のメーターを埋める**（doc/native-engine.md の 6.148）
	void draw_meter();
	u8 m_meter_lv[16] = {};         // native が鳴らしている音の目盛り
	u8 m_meter_smooth[16] = {};     // なまし（実機と同じ半分ずつ寄せる）
	u8 m_meter_cell[16] = {};       // 前に液晶へ置いた棒の字（下 8 + 上 8）
	u64 m_meter_next = 0;           // つぎになます時刻
	// **演奏画面の音色まわりを native が描く**（6.190）。
	// 名前（行 0 の 9-16）・プログラムの 3 桁（行 1 の 14-16）・
	// 楽器の絵（外字 0-2・4-6）だけ。firmware は 100ms につき 5ms しか
	// 回らないので、任せると音色を替えてから最大 100ms 遅れる
	void draw_voice_fields();
	void release_voice_fields();
	bool m_vf_owned = false;        // いま持っているか
	u8  m_vf_name[8] = {};          // 前に置いた名前
	u8  m_vf_prog[3] = {};          // 前に置いた番号
	u8  m_vf_bank[3] = {};          // 前に置いたバンク（6.202）
	u16 m_vf_icon[16] = {};         // 前に置いた絵
	int m_vf_part = -1;
	// **パートの種類**（XG の 08 pp 07。0 が旋律、2-5 がドラム 1-4）。
	// -1 はまだ SysEx を見ていない（ワーク RAM を読む）。バンク 127/126 で
	// なくてもここでドラムになるので、音色の引き方を変える必要がある
	// （doc/native-engine.md の 6.137）
	s8 m_part_mode[64] = {};
	static u64 drum_lead()
	{
		static const u64 v = std::getenv("SMU2000_DRUM_LEAD")
		                   ? u64(std::atoi(std::getenv("SMU2000_DRUM_LEAD"))) : 3;
		return v;
	}
	bool part_is_drum(int part) const
	{
		if (part < 0 || part >= 64)
			return false;
		if (m_part_mode[part] >= 0)
			return m_part_mode[part] != 0;
		const u32 off = xg::ram::part_base(part) + 0x07;
		return m_ram.size() > off && m_ram[off] != 0;
	}
	void native_select_voice(int part);
	// そのバンク LSB を実機が受け付けるか（6.202）
	bool voice_lsb_ok(int msb, int lsb) const;
	// 受け取り終えた XG の SysEx を、native の側にも効かせる
	void native_sysex(u64 fire);

	// 口ごとの MIDI の読み取り
	struct nmidi { u8 status = 0; u8 d0 = 0; int have = 0; };
	nmidi m_nmidi[MIDI_PORTS];

	int  m_learn_note = 60, m_learn_vel = 100, m_learn_part = 0;
	// firmware が鳴らしている音の数（パートごと）。0 でなければベンドも firmware へ回す
	u8   m_fw_notes[64] = {};
	// firmware が鳴らしている音の、液晶のメーター用の目盛り（6.188）。
	// 打った時刻も覚えておく（m_fw_notes はオールノートオフなどで
	// 戻らないことがあり、そのままだと棒が立ちっぱなしになる）
	u8   m_fw_meter[16] = {};
	u64  m_fw_meter_at[16] = {};
	static constexpr u64 FW_METER_HOLD = 44100 * 4;
	u32  m_fw_note_total = 0;
	// firmware の音のために回すのは、いちばん新しい音から この長さだけ。
	// フィルタ・LFO の包絡線はそのころには落ち着いている。
	// 3 秒でも試験の 7 曲は 1 つも変わらなかったが、長い音のために余裕を見る
	static constexpr u64 FW_NOTE_RUN = 44100 * 5;   // 1.2 秒
	u64  m_fw_note_until = 0;
	u64  m_learn_drum = 0;         // ドラムのとき、覚える鍵
	// 写し取りのとき、firmware がこちらの鳴っているスロットを取ってしまった回数
	u32  m_ne_slot_clash = 0;
	// 写し取りの窓の中で、別の音が同じスロットに鳴り始めた回数
	u32  m_ne_learn_dirty = 0;
	// 写し取りで、その音色のものでないスロットを掴んで捨てた回数
	u32  m_ne_learn_wrong = 0;
	// **写し取りの鍵**。firmware は XG のノートシフト（08 pp 08）を足して
	// から鳴らすので、こちらの式もその鍵で見ないと合わない
	// **写し取りの強さ**。firmware はベロシティ感度（08 pp 0C・0D）を掛けて
	// から鳴らすので、こちらの式もその強さで見る
	int  learn_vel_sensed() const
	{ return m_ndrv.part_vel(m_learn_part, m_learn_vel); }
	int  learn_note_shifted() const
	{
		const int n = m_learn_note + m_ndrv.part_shift(m_learn_part);
		return n < 0 ? 0 : (n > 127 ? 127 : n);
	}
	// 実機のボイスの塊から読んだ音量の目盛りが、写し取った 0x09 と合わなかった数
	u32  m_ne_lvl_miss = 0;
	// firmware が、こちらが鳴らしているスロットに書いた回数
	u32  m_ne_fw_stomp = 0;
	void note_fw_swp(bool master, u32 reg, u16 value);
	u64  m_fw_keymask[2] = { 0, 0 };  // firmware がつぎに鳴らすスロットのマスク（マスタ・スレーブ）
	// firmware を細く回し続ける刻み（100ms ごとに 5ms）。止めきると液晶・
	// ボタン・firmware 自身の後始末が全部止まる
	// **パネルを触っている間は firmware を全速で回す**（doc/native-engine.md の 6.119）。
	// native の口では firmware を 100ms につき 5ms しか回さないので、
	// firmware の中の時間は 20 分の 1 でしか進まない。液晶もボタンも
	// ダイヤルも firmware の仕事なので、そのままだと
	//   * ダイヤルが毎秒 20 目盛りしか進まない（実機は 400）
	//   * 画面が変わるまでひと呼吸かかる
	// になる。触ってから この長さだけ全速で回すと、実機と同じ手触りになる。
	// 触っていない間は今までどおり細く回すだけ（CPU は増えない）
	// **0.5 秒では足りなかった**（6.146）。ダイヤルを 4 目盛り回すと、
	// 実機モードは 4 つとも効くのに native は 1 つしか効かない。firmware は
	// 目盛りを受け取ってから画面と音色を作り直すのに、firmware の中の時間で
	// 1 秒近く掛かる。
	// `SMU2000_PANEL_RUN` で振れる（サンプル数。0 で前の道に戻る）
	static u32 panel_run()
	{
		static const u32 v = std::getenv("SMU2000_PANEL_RUN")
		                   ? u32(std::atoi(std::getenv("SMU2000_PANEL_RUN")))
		                   : u32(44100);          // 1 秒
		return v;
	}
	void panel_touched() { m_panel_hold = panel_run(); }
	// 液晶を書き換えている間の延長ぶん（短くてよい。止まればすぐ戻る）
	static constexpr u32 LCD_RUN = 44100 / 10;     // 0.1 秒
	u32 m_panel_hold = 0;

	static constexpr u32 KEEPALIVE_EVERY = 4410;
	static constexpr u32 KEEPALIVE_RUN = 220;
public:
	u32  native_slot_clash() const { return m_ne_slot_clash; }
	u32  native_learn_dirty() const { return m_ne_learn_dirty; }
	u32  native_learn_wrong() const { return m_ne_learn_wrong; }
	u32  native_level_miss() const { return m_ne_lvl_miss; }
	u32  native_fw_stomp() const { return m_ne_fw_stomp; }
private:
	native_stats m_ne_stats;

	bool native_midi(u8 byte, int port);
	void replay_note(u8 status, u8 d0, u8 d1, int port);
	void native_learn_start(u32 rec);
	void native_learn_finish();

	swp30_device m_swpm, m_swps;   // マスタ 0x800000 / スレーブ 0x802000

	// パートの音（set_scope_part）。チップごとに輪を持ち、読むときに足す
	// （スレーブは別の糸で回ることがあるので、書き手を分ける）
	struct scope_tap { mu2000 *self; int chip; };
	scope_tap m_scope_ctx[2] = { { this, 0 }, { this, 1 } };
	std::atomic<int> m_scope_part{-1};
	std::array<std::atomic<s8>, 128> m_scope_owner{};     // 声 → パート（-1 は無し）
	std::array<std::array<float, SCOPE_N>, 2> m_scope_ring{};
	std::array<std::atomic<u32>, 2> m_scope_w{};          // チップごとの書いた数
	u32 m_scope_tick = 0;
	static void scope_tap_fn(void *ctx, const s32 *samples);
	// インサーションの出口（MEG の m20-m2f。scope_meg_fn）。インサーション 1 はマスタの m28/m29、
	// 2-4 はスレーブの m28/m29・m2a/m2b・m2c/m2d（firmware が組む MEG の割り付け。エミュで実測）
	std::atomic<int> m_scope_ins{-1};                     // 見ているパートのインサーション（0-3、-1 は無し）
	// チップごと、m20-m2f の 8 組 × 入口・出口の輪（[chip][pair * 2 + out][SCOPE_N]。大きいので別に取る）
	std::vector<float> m_fx_ring = std::vector<float>(2 * 16 * SCOPE_N);
	std::array<std::atomic<u32>, 2> m_fx_w{};
	static void scope_meg_fn(void *ctx, const s32 *in, const s32 *out);
	void scope_refresh_owner();
	// 全パートの輪（[chip][k][part]）と最終の出力の輪
	std::atomic<bool> m_pscope_on{false};
	std::vector<float> m_pscope = std::vector<float>(2 * PSCOPE_N * 64);
	std::array<std::atomic<u32>, 2> m_pscope_w{};
	std::vector<float> m_oscope = std::vector<float>(PSCOPE_N);
	std::atomic<u32> m_oscope_w{0};
	required_device<sci4_device> m_sci4_finder;
	sci4_device *m_sci4 = nullptr;   // PLG ボード用 0xf00000
	mem_bus      m_bus;

	u8rom  m_prog;                  // プログラム ROM 4MB
	u8rom  m_wave;                  // 波形 ROM 32MB
	u16rom m_sintab;
	std::vector<u8>  m_ram;         // ワーク RAM  0x400000-0x43ffff
	std::vector<u8>  m_dram;        // DRAM        0x1000000-0x107ffff
	std::vector<u8>  m_iram;        // CPU 内蔵    0xfffff000-0xffffffff
	smu2000::smartmedia m_card;     // 前面のカードの差し込み口（SmartMedia）
	std::vector<u8>  m_sampram;     // SWP30 のサンプリング RAM（4MB、SWP30 から見て 0x1000000 語目から）
	s32 m_ad_in[2] = {};            // A/D INPUT（set_audio_input）
	s32 m_ad_peak[2] = {};          // A/D INPUT のピーク（レベルメーター、AN0 / AN2）。状態の保存には入れない
	u16 ad_level_adc(int i) const
	{
		// 0xff から引いた値が 0x18（無音）から 0x85（振り切れ）。10bit にして返す
		const u32 v = 0x18 + u32(m_ad_peak[i]) * (0x85 - 0x18) / 32768;
		return u16((0xff - v) << 2);
	}

	// パネルまわり。音そのものには関わらないが、firmware は起動時に触る。
	// LCD は「要らない」ように見えて必要だった。firmware は初期化のたびに
	// ビジーフラグが立つのを確かめており、常に空いていると先へ進まない
	hd44780_device m_lcd;
	u8  m_ledsw1 = 0, m_ledsw2 = 0;
	// d80000: LCD のコントラストほか（MAME の地図では "contrast, levels"）
	u8  m_d80 = 0;
	// 押されているボタン。行 6 × 桁 8。押すと 0 になる
	u8  m_sws[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8   ledsw_r() const;

	// あと何目盛りぶん送るか。符号が向き。読まれるたびに 1 ずつ減る
	int m_enc_pending = 0;
	bool m_enc_high = true;
	u16 m_pe = 0;
	u8rom m_lcd_font;             // HD44780 の CGROM 4KB

	u16  lcd_port_r();
	void lcd_port_w(u16 data);

	// SCI4 の割り込み。0 と 1 は OR して CPU の IRQ0 へ（MAME の input_merger）
	int  m_sci_irq[2] = { 0, 0 };
	void update_sci_irq();

	std::string m_error;
	// SWP30 へのアクセス幅の内訳（byte 幅があると片側が壊れる）
	u64 m_swp_w8 = 0, m_swp_r8 = 0, m_swp_w16 = 0, m_swp_w32 = 0;

	// 記録に入れるサンプル番号（0 起点。run_sample の頭で進めるので 1 引く）
	u64 trace_sample() const { return m_sample_count ? m_sample_count - 1 : 0; }
	u64         m_sample_count = 0;  // 電源投入から数えたサンプル数（記録と再生の目印）
	// **MU の表示灯の点滅**（native の口。leds()）。firmware は MIDI を受けると MU の灯を
	// 一瞬消すが、native の口では firmware を間引いて回すので点いたままになる。
	// 受けた時刻から、消す区間を m_ne_clock の目盛りで持つ
	u64         m_led_off_from = 0, m_led_off_until = 0;
	void        led_blink(u64 at);
	swp_watch_fn m_swp_watch;
	std::FILE  *m_swp_trace = nullptr;
	bool        m_swp_trace_reads = false;

	// 44.1kHz 1 サンプルあたりの CPU サイクル。端数は繰り越す
	u64 m_cycle_debt = 0;
	// 命令の途中で止まれず走りすぎた分。次の呼び出しから引く
	u64 m_overrun = 0;
	// SWP30 のレジスタに書いたので、このサンプルの残りは CPU を止める（run_cycles の説明）
	// マスタの SWP30 へ 1 本書くと CPU が待たされるサイクル数（build_bus の説明）。
	// 実機で測った 61.4 サンプルに合う値（doc/upstream.md の 36）
	static constexpr u64 SWP_WRITE_CYCLES = 440;
	u64 m_swp_wait = 0;      // まだ消化していない待ち
	bool m_profile = false;

	// スレーブ用のスレッド。合図は atomic の回し合いで、錠は使わない。
	// 44100 回/秒の受け渡しなので、待つのは眠らずに回して待つ
	std::thread m_slave_thread;
	bool m_want_threaded = false;
	u32  m_thread_check = 0;
	void apply_threading();
	std::atomic<u64> m_slave_go{0}, m_slave_done{0};
	std::atomic<bool> m_slave_quit{false};
	s32 m_slave_l = 0, m_slave_r = 0;
	void slave_loop(u64 seen);

	// Parallel real-time audio workgroup (macOS) for the slave thread below,
	// as an os_workgroup_t. Plain void* so this header stays platform-free;
	// only macOS front ends set it. The slave joins whatever is set (null
	// keeps today's behavior); see slave_loop for the join itself.
	std::atomic<void *> m_rt_wg_want{nullptr};

public:
	// Parallel real-time audio workgroup (macOS) for the slave thread: an
	// os_workgroup_t, kept as void* so this header stays platform-free.
	void set_realtime_workgroup(void *wg) { m_rt_wg_want.store(wg, std::memory_order_release); }

	// 速さの手掛かり。1 サンプルあたり実行ループを何周したか
	u64 m_loops = 0, m_timer_fires = 0, m_event_fires = 0;
	// 区間ごとの所要時間（QueryPerformanceCounter の刻み）。
	// **set_profile(true) のときだけ測る**（1 サンプルにつき 3 回読むので、
	// 常に測ると 0.3% ほど食う）
	u64 m_t_cpu = 0, m_t_swpm = 0, m_t_swps = 0, m_t_n = 0;
	// m_t_cpu の中をさらに割る（式だけの口でここが臨界経路の半分を占めるので、
	// 何に使っているのかを見るため）。**測るときだけ**時計を 3 対よけいに読むので、
	// この 3 つを足しても m_t_cpu とは一致しない（その差が時計の代金）
	//   m_t_sh2    SH-2 を回した時間（run_cycles）。回した回数は m_n_sh2
	//   m_t_ndrv   native の口の毎サンプルの仕事（native_driver::tick）
	//   m_t_nemisc その他の native の口の面倒（見張り・メーター・つまみの拾い直し）
	u64 m_t_sh2 = 0, m_t_ndrv = 0, m_t_nemisc = 0, m_n_sh2 = 0;
	// SWP30 の中の MEG の時間は m_swpm / m_swps の m_t_meg（ns）に入る
	void set_profile(bool on) { m_profile = on; m_swpm.m_profile = on; m_swps.m_profile = on; }
	void clear_profile()
	{
		m_t_cpu = m_t_swpm = m_t_swps = m_t_n = m_loops = 0;
		m_t_sh2 = m_t_ndrv = m_t_nemisc = m_n_sh2 = 0;
		m_swpm.m_t_meg = m_swps.m_t_meg = 0;
	}
private:

	// S-MU2000: 軽量モード（doc/native-dsp.md）。RAM の XG の設定を読んで C++ 側へ渡す
	void native_fx_update();

	smu2000::dsp::native_fx m_nfx;
	int  m_nfx_on = 0;
	bool m_nfx_ready = false;      // 遅延の線を用意したか（台ごと）
	u32  m_nfx_tick = 0;

	// MIDI IN A / B。バイトを 31250bps の直列に崩して RX 線に流す。
	// 2 口は別々の SCI なので、状態も別々に持つ
	struct midi_line {
		std::deque<u8> queue;
		int bit  = -1;    // -1 待ち / 0 スタート / 1-8 データ / 9 ストップ
		u8  cur  = 0;
		u64 next = 0;
	};
	void midi_step(u64 now);
	std::array<midi_line, MIDI_DIN_PORTS> m_midi;
	std::atomic<u64> m_midi_dropped{0};
	bool m_fast_midi = false;

	// USB の代役。SH-2 から見えるのは 2 番地だけなので、持つものも少ない
	struct usb_line {
		std::deque<u8> rx;      // F5 <口> を挟んだ MIDI バイト列
		int  in_port  = -1;     // 溜めに積んだ最後の口（F5 を挟む判断に使う）
		u64  next     = 0;      // 次のバイトを渡してよい時刻
		bool have     = false;  // 渡したバイトをまだ読まれていない
		u8   cur      = 0;
		std::deque<u8> cmd;     // M37640 からのコマンド。状態の bit6 を立てて渡す
		bool cur_cmd  = false;  // 渡しているバイトがコマンドか
		u64  tx_next  = 0;
		std::deque<u8> tx;      // firmware が出した MIDI バイト（F5 込み）
		int  out_port = -1;     // 取り出し側が見ている口
	};
	void usb_midi_in(u8 byte, int port);
	// ケーブルメッセージ（midi_in の説明）。入口ごとに、いま回している口と、F5 の後の番号待ち
	std::array<int, MIDI_PORTS>  m_cable = { 0, 1, 2, 3 };
	std::array<bool, MIDI_PORTS> m_cable_wait = {};
	void usb_step(u64 now);
	u8   usb_r(offs_t a);
	void usb_w(offs_t a, u8 v);
	usb_line m_usb;
	bool m_usb_host = false;

	// MIDI OUT の線から枠を組み立てる。SCI は 1 ビットにつき 1 回だけ線の値を
	// 知らせてくるので、時刻を見なくても「0 で開始、8 ビット、1 で終わり」で読める
	void tx_line(int state);
	static constexpr size_t TX_SIZE = 4096, TX_MASK = TX_SIZE - 1;
	u8     m_tx_buf[TX_SIZE] = {};
	size_t m_tx_r = 0, m_tx_w = 0;
	int    m_tx_bit = -1;       // -1 待ち / 0-7 データ / 8 ストップ
	u8     m_tx_cur = 0;
};

#endif // S_MU2000_MU2000_H
