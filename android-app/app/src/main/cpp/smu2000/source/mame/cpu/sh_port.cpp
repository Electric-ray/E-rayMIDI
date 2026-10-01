// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    sh_port.h

    SH i/o ports

***************************************************************************/

#include "emu.h"
#include "sh_intc.h"

#include "sh7042.h"

DEFINE_DEVICE_TYPE(SH_PORT16, sh_port16_device, "sh_port16", "SH 16-bits port")
DEFINE_DEVICE_TYPE(SH_PORT32, sh_port32_device, "sh_port32", "SH 32-bits port")

sh_port16_device::sh_port16_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, SH_PORT16, tag, owner, clock),
	m_cpu(*this, finder_base::DUMMY_TAG)
{
}

void sh_port16_device::device_start()
{
	m_io = m_default_io;
	save_item(NAME(m_dr));
	save_item(NAME(m_io));
}

void sh_port16_device::device_reset()
{
}

u16 sh_port16_device::dr_r()
{
	// S-MU2000: 移植の突き合わせ用。MAME 側にも同じものを入れてある
	if(::smu2000::g_port_trace && m_index == 2) {
		const u16 ext = m_cpu->do_read_port16(m_index);
		const u16 v = (~m_io & ~m_mask) ? u16((m_dr & m_io) | (ext & ~m_io)) : m_dr;
		fprintf(::smu2000::g_port_trace, "R dr=%04x io=%04x mask=%04x ext=%04x -> %04x\n",
		        m_dr, m_io, m_mask, ext, v);
		return v;
	}
	if(~m_io & ~m_mask)
		return (m_dr & m_io) | (m_cpu->do_read_port16(m_index) & ~m_io);
	return m_dr;
}

void sh_port16_device::dr_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_dr);
	if(::smu2000::g_port_trace && m_index == 2)
		fprintf(::smu2000::g_port_trace, "W dr=%04x io=%04x data=%04x mask=%04x\n",
		        m_dr, m_io, data, mem_mask);
	m_dr &= ~m_mask;
	if(m_io)
		m_cpu->do_write_port16(m_index, m_dr & m_io, m_io);
}

u16 sh_port16_device::io_r()
{
	return m_io;
}

void sh_port16_device::io_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_io);
	if(::smu2000::g_port_trace && m_index == 2)
		fprintf(::smu2000::g_port_trace, "IO io=%04x data=%04x\n", m_io, data);
	m_io &= ~m_mask;
	if(m_io)
		m_cpu->do_write_port16(m_index, m_dr & m_io, m_io);
}


sh_port32_device::sh_port32_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, SH_PORT32, tag, owner, clock),
	m_cpu(*this, finder_base::DUMMY_TAG)
{
}

void sh_port32_device::device_start()
{
	m_io = m_default_io;
	save_item(NAME(m_dr));
	save_item(NAME(m_io));
}

void sh_port32_device::device_reset()
{
}

u32 sh_port32_device::dr_r()
{
	if((~m_io) & (~m_mask))
		return (m_dr & m_io) | (m_cpu->do_read_port32(m_index) & ~m_io);
	return m_dr;
}

void sh_port32_device::dr_w(offs_t, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_dr);
	m_dr &= ~m_mask;
	if(m_io)
		m_cpu->do_write_port32(m_index, m_dr & m_io, m_io);
}

u32 sh_port32_device::io_r()
{
	return m_io;
}

void sh_port32_device::io_w(offs_t, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_io);
	m_io &= ~m_mask;
	if(m_io)
		m_cpu->do_write_port32(m_index, m_dr & m_io, m_io);
}


void sh_port16_device::state(state_io &s)
{
	s.tag("port16");
	s.v(m_dr); s.v(m_io);
}

void sh_port32_device::state(state_io &s)
{
	s.tag("port32");
	s.v(m_dr); s.v(m_io);
}
