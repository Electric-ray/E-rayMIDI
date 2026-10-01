// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

// SH7042, sh2 variant

#include "emu.h"
#include "sh7042.h"

DEFINE_DEVICE_TYPE(SH7042,  sh7042_device,  "sh7042",  "Hitachi SH-2 (SH7042)")
DEFINE_DEVICE_TYPE(SH7042A, sh7042a_device, "sh7042a", "Hitachi SH-2 (SH7042A)")
DEFINE_DEVICE_TYPE(SH7043,  sh7043_device,  "sh7043",  "Hitachi SH-2 (SH7043)")
DEFINE_DEVICE_TYPE(SH7043A, sh7043a_device, "sh7043a", "Hitachi SH-2 (SH7043A)")

sh7042_device::sh7042_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	sh7042_device(mconfig, SH7042, tag, owner, clock)
{
	m_die_a = false;
}

sh7042a_device::sh7042a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	sh7042_device(mconfig, SH7042A, tag, owner, clock)
{
	m_die_a = true;
}

sh7043_device::sh7043_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	sh7042_device(mconfig, SH7043, tag, owner, clock)
{
	m_die_a = false;
}

sh7043a_device::sh7043a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	sh7042_device(mconfig, SH7043A, tag, owner, clock)
{
	m_die_a = true;
}

sh7042_device::sh7042_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	// S-MU2000: MAME はここで内蔵周辺のアドレス空間（map）を渡していた。
	// その中身は sh7042_map.hxx に展開してある
	sh2_device(mconfig, type, tag, owner, clock, CPU_TYPE_SH2, address_map_constructor(), 32, 0xffffffff),
	m_intc(*this, "intc"),
	m_adc0(*this, "adc0"),
	m_adc1(*this, "adc1"),
	m_bsc(*this, "bsc"),
	m_cmt(*this, "cmt"),
	m_dmac(*this, "dmac"),
	m_dmac0(*this, "dmac:0"),
	m_dmac1(*this, "dmac:1"),
	m_dmac2(*this, "dmac:2"),
	m_dmac3(*this, "dmac:3"),
	m_mtu(*this, "mtu"),
	m_mtu0(*this, "mtu:0"),
	m_mtu1(*this, "mtu:1"),
	m_mtu2(*this, "mtu:2"),
	m_mtu3(*this, "mtu:3"),
	m_mtu4(*this, "mtu:4"),
	m_porta(*this, "porta"),
	m_portb(*this, "portb"),
	m_portc(*this, "portc"),
	m_portd(*this, "portd"),
	m_porte(*this, "porte"),
	m_portf(*this, "portf"),
	m_sci(*this, "sci%d", 0),
	m_read_adc(*this, 0),
	m_sci_tx(*this),
	m_sci_clk(*this),
	m_read_port16(*this, 0xffff),
	m_write_port16(*this),
	m_read_port32(*this, 0xffffffff),
	m_write_port32(*this)
{
	m_port16_names = "bcef";
	m_port32_names = "ad";
	for(unsigned int i=0; i != m_read_adc.size(); i++)
		m_read_adc[i].bind().set([this, i]() { return adc_default(i); });
	for(unsigned int i=0; i != m_read_port16.size(); i++) {
		m_read_port16[i].bind().set([this, i]() { return port16_default_r(i); });
		m_write_port16[i].bind().set([this, i](u16 data) { port16_default_w(i, data); });
	}
	for(unsigned int i=0; i != m_read_port32.size(); i++) {
		m_read_port32[i].bind().set([this, i]() { return port32_default_r(i); });
		m_write_port32[i].bind().set([this, i](u32 data) { port32_default_w(i, data); });
	}
}

u16 sh7042_device::port16_default_r(int port)
{
	if(!machine().side_effects_disabled())
		logerror("read of un-hooked port %c\n", m_port16_names[port]);
	return 0xffff;
}

void sh7042_device::port16_default_w(int port, u16 data)
{
	logerror("write of un-hooked port %c %04x\n", m_port16_names[port], data);
}

u32 sh7042_device::port32_default_r(int port)
{
	if(!machine().side_effects_disabled())
		logerror("read of un-hooked port %c\n", m_port32_names[port]);
	return 0xffff;
}

void sh7042_device::port32_default_w(int port, u32 data)
{
	logerror("write of un-hooked port %c %04x\n", m_port32_names[port], data);
}


u16 sh7042_device::adc_default(int adc)
{
	logerror("read of un-hooked adc %d\n", adc);
	return 0;
}

void sh7042_device::device_start()
{
	sh2_device::device_start();

	save_item(NAME(m_pcf_ah));
	save_item(NAME(m_pcf_al));
	save_item(NAME(m_pcf_b));
	save_item(NAME(m_pcf_c));
	save_item(NAME(m_pcf_dh));
	save_item(NAME(m_pcf_dl));
	save_item(NAME(m_pcf_e));
	save_item(NAME(m_pcf_if));

	m_pcf_ah = 0;
	m_pcf_al = 0;
	m_pcf_b = 0;
	m_pcf_c = 0;
	m_pcf_dh = 0;
	m_pcf_dl = 0;
	m_pcf_e = 0;
	m_pcf_if = 0;
}

void sh7042_device::execute_set_input(int irqline, int state)
{
	m_intc->set_input(irqline, state);
}

void sh7042_device::device_reset()
{
	sh2_device::device_reset();
}


// S-MU2000: address_map をやめ、番地で振り分ける形にした。
// 移植したのは firmware が実際に触る周辺だけ（SCI/MTU/CMT/INTC/PORT と
// CPU 自身のピン機能設定 pcf_*）。BSC と DMAC は起動時に少し触られるだけなので
// 何もしない。ADC は参照 0 回だったので配線していない。
// 内蔵 RAM 0xFFFFF000- は mem_bus 側で領域として持つ。
// 根拠は doc/design.md「どの内蔵周辺が要るか」。

// 内蔵周辺のレジスタ振り分け。MAME の sh7042_device::map() から機械で起こす。
// 作り直すには tools/gen_sh7042_map.py を走らせる
#include "sh7042_map.hxx"

void sh7042_device::device_add_mconfig(machine_config &config)
{
	SH_INTC(config, m_intc, *this);
	if(m_die_a) {
		SH_ADC_MS(config, m_adc0, *this, m_intc, 0, 136);
		SH_ADC_MS(config, m_adc1, *this, m_intc, 4, 137);
	} else
		SH_ADC_HS(config, m_adc0, *this, m_intc, 136);
	SH_BSC(config, m_bsc);
	SH_CMT(config, m_cmt, *this, m_intc, 144, 148);
	SH_DMAC(config, m_dmac, *this);
	SH_DMAC_CHANNEL(config, m_dmac0, *this, m_intc);
	SH_DMAC_CHANNEL(config, m_dmac1, *this, m_intc);
	SH_DMAC_CHANNEL(config, m_dmac2, *this, m_intc);
	SH_DMAC_CHANNEL(config, m_dmac3, *this, m_intc);
	SH_MTU(config, m_mtu, *this, 5);
	SH_MTU_CHANNEL(config, m_mtu0, *this, 4, 0x60, m_intc, 88,
			sh_mtu_channel_device::DIV_1,
			sh_mtu_channel_device::DIV_4,
			sh_mtu_channel_device::DIV_16,
			sh_mtu_channel_device::DIV_64,
			sh_mtu_channel_device::INPUT_A,
			sh_mtu_channel_device::INPUT_B,
			sh_mtu_channel_device::INPUT_C,
			sh_mtu_channel_device::INPUT_D);
	SH_MTU_CHANNEL(config, m_mtu1, *this, 2, 0x4c, m_intc, 96,
			sh_mtu_channel_device::DIV_1,
			sh_mtu_channel_device::DIV_4,
			sh_mtu_channel_device::DIV_16,
			sh_mtu_channel_device::DIV_64,
			sh_mtu_channel_device::INPUT_A,
			sh_mtu_channel_device::INPUT_B,
			sh_mtu_channel_device::DIV_256,
			sh_mtu_channel_device::CHAIN).set_chain(m_mtu2);
	SH_MTU_CHANNEL(config, m_mtu2, *this, 2, 0x4c, m_intc, 104,
			sh_mtu_channel_device::DIV_1,
			sh_mtu_channel_device::DIV_4,
			sh_mtu_channel_device::DIV_16,
			sh_mtu_channel_device::DIV_64,
			sh_mtu_channel_device::INPUT_A,
			sh_mtu_channel_device::INPUT_B,
			sh_mtu_channel_device::INPUT_C,
			sh_mtu_channel_device::DIV_1024);
	SH_MTU_CHANNEL(config, m_mtu3, *this, 4, 0x60, m_intc, 112,
			sh_mtu_channel_device::DIV_1,
			sh_mtu_channel_device::DIV_4,
			sh_mtu_channel_device::DIV_16,
			sh_mtu_channel_device::DIV_64,
			sh_mtu_channel_device::DIV_256,
			sh_mtu_channel_device::DIV_1024,
			sh_mtu_channel_device::INPUT_A,
			sh_mtu_channel_device::INPUT_B);
	SH_MTU_CHANNEL(config, m_mtu4, *this, 4, 0x60, m_intc, 120,
			sh_mtu_channel_device::DIV_1,
			sh_mtu_channel_device::DIV_4,
			sh_mtu_channel_device::DIV_16,
			sh_mtu_channel_device::DIV_64,
			sh_mtu_channel_device::DIV_256,
			sh_mtu_channel_device::DIV_1024,
			sh_mtu_channel_device::INPUT_A,
			sh_mtu_channel_device::INPUT_B);
	// S-MU2000: MTU 本体から各チャンネルへの結線。MAME は tag で解決していた
	m_mtu->set_channel(0, *m_mtu0);
	m_mtu->set_channel(1, *m_mtu1);
	m_mtu->set_channel(2, *m_mtu2);
	m_mtu->set_channel(3, *m_mtu3);
	m_mtu->set_channel(4, *m_mtu4);

	SH_PORT32(config, m_porta, *this, 0, 0x00000000, 0xff000000);
	SH_PORT16(config, m_portb, *this, 0, 0x0000, 0xfc00);
	SH_PORT16(config, m_portc, *this, 1, 0x0000, 0x0000);
	SH_PORT32(config, m_portd, *this, 1, 0x0000, 0x0000);
	SH_PORT16(config, m_porte, *this, 2, 0x0000, 0x0000);
	SH_PORT16(config, m_portf, *this, 3, 0x0000, 0xff00);
	SH_SCI(config, m_sci[0], 0, *this, m_intc, 128, 129, 130, 131);
	SH_SCI(config, m_sci[1], 1, *this, m_intc, 132, 133, 134, 135);

}

void sh7042_device::internal_update()
{
	internal_update(current_cycles());
}

void sh7042_device::event_tick()
{
	m_in_event = true;
	internal_update(current_cycles());
	m_in_event = false;
}

void sh7042_device::add_event(u64 &event_time, u64 new_event)
{
	if(!new_event)
		return;
	if(!event_time || event_time > new_event)
		event_time = new_event;
}

void sh7042_device::recompute_timer(u64 event_time)
{
	// MAME は event_time + 半サイクルの時刻でスケジューラに起こしてもらっていた。
	// こちらは実行ループが total_cycles() で追い越したときに呼ぶ。
	// 走っている最中に予定が変わったら、その場で切り上げて組み直させる
	if (m_event_cycles != event_time) {
		m_event_cycles = event_time;
		abort_timeslice();
	}
}

TIMER_CALLBACK_MEMBER(sh7042_device::event_timer_tick)
{
	internal_update();
}

void sh7042_device::internal_update(u64 current_time)
{
	u64 event_time = 0;

	add_event(event_time, m_adc0->internal_update(current_time));
	if(m_adc1)
		add_event(event_time, m_adc1->internal_update(current_time));
	add_event(event_time, m_cmt->internal_update(current_time));
	add_event(event_time, m_mtu0->internal_update(current_time));
	add_event(event_time, m_mtu1->internal_update(current_time));
	add_event(event_time, m_mtu2->internal_update(current_time));
	add_event(event_time, m_mtu3->internal_update(current_time));
	add_event(event_time, m_mtu4->internal_update(current_time));
	add_event(event_time, m_sci[0]->internal_update(current_time));
	add_event(event_time, m_sci[1]->internal_update(current_time));

	// S-MU2000: 移植の突き合わせ用。MAME 側にも同じものを入れてある
	if(::smu2000::g_upd_trace)
		fprintf(::smu2000::g_upd_trace, "U %llu -> %llu pc=%08x\n",
		        (unsigned long long)current_time, (unsigned long long)event_time, pc());

	recompute_timer(event_time);
}

u16 sh7042_device::pcf_ah_r()
{
	return m_pcf_ah;
}

void sh7042_device::pcf_ah_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_pcf_ah);
	logerror("pcf ah = %04x\n", m_pcf_ah);
}

u32 sh7042_device::pcf_al_r()
{
	return m_pcf_al;
}

void sh7042_device::pcf_al_w(offs_t, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_pcf_al);
	logerror("pcf al = %08x\n", m_pcf_al);
}

u32 sh7042_device::pcf_b_r()
{
	return m_pcf_b;
}

void sh7042_device::pcf_b_w(offs_t, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_pcf_b);
	logerror("pcf b = %08x\n", m_pcf_b);
}

u16 sh7042_device::pcf_c_r()
{
	return m_pcf_c;
}

void sh7042_device::pcf_c_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_pcf_c);
	logerror("pcf c = %04x\n", m_pcf_c);
}

u32 sh7042_device::pcf_dh_r()
{
	return m_pcf_dh;
}

void sh7042_device::pcf_dh_w(offs_t, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_pcf_dh);
	logerror("pcf dh = %08x\n", m_pcf_dh);
}

u16 sh7042_device::pcf_dl_r()
{
	return m_pcf_dl;
}

void sh7042_device::pcf_dl_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_pcf_dl);
	logerror("pcf dl = %04x\n", m_pcf_dl);
}

u32 sh7042_device::pcf_e_r()
{
	return m_pcf_e;
}

void sh7042_device::pcf_e_w(offs_t, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_pcf_e);
	logerror("pcf e = %08x\n", m_pcf_e);
}

u16 sh7042_device::pcf_if_r()
{
	return m_pcf_if;
}

void sh7042_device::pcf_if_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_pcf_if);
	logerror("pcf if = %04x\n", m_pcf_if);
}

void sh7042_device::set_internal_interrupt(int level, u32 vector)
{
	m_sh2_state->internal_irq_level = level;
	m_internal_irq_vector = vector;
	m_test_irq = 1;
}

void sh7042_device::sh2_exception_internal(const char *message, int irqline, int vector)
{
	sh2_device::sh2_exception_internal(message, irqline, vector);
	m_intc->interrupt_taken(irqline, vector);
}

void sh7042_device::state(state_io &s)
{
	s.tag("sh7042");
	sh2_device::state(s);
	s.v(m_event_cycles); s.v(m_in_event);
	s.v(m_pcf_ah); s.v(m_pcf_al); s.v(m_pcf_b); s.v(m_pcf_c);
	s.v(m_pcf_dh); s.v(m_pcf_dl); s.v(m_pcf_e); s.v(m_pcf_if);

	// 内蔵の周辺。生まれた順にたどる
	m_intc->state(s);
	m_adc0->state(s);
	// S-MU2000: 中速の SH7042 は A/D 変換器を 2 つ持つ。2 つ目（AN4）を写し忘れていたので、
	// 状態を戻すと HOST SELECT の読み値が 0 に戻り、firmware が「USB ではない」と見て
	// USB の受信を止めていた（issue #18）。版 8 から写す
	if (s.version() >= 8 && m_adc1)
		m_adc1->state(s);
	m_bsc->state(s);
	m_cmt->state(s);
	m_dmac->state(s);
	m_dmac0->state(s); m_dmac1->state(s); m_dmac2->state(s); m_dmac3->state(s);
	m_mtu->state(s);
	m_mtu0->state(s); m_mtu1->state(s); m_mtu2->state(s);
	m_mtu3->state(s); m_mtu4->state(s);
	m_porta->state(s); m_portb->state(s); m_portc->state(s);
	m_portd->state(s); m_porte->state(s); m_portf->state(s);
	for (int i = 0; i < 2; i++)
		if (sh_sci_device *p = m_sci[i].lookup())
			p->state(s);
}
