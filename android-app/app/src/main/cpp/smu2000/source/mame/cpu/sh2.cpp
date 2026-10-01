// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller, R. Belmont
/*****************************************************************************
 *
 *   sh2.cpp
 *   Portable Hitachi SH-2 (SH7600 family) emulator
 *
 *  This work is based on <tiraniddo@hotmail.com> C/C++ implementation of
 *  the SH-2 CPU core and was adapted to the MAME CPU core requirements.
 *  Thanks also go to Chuck Mason <chukjr@sundail.net> and Olivier Galibert
 *  <galibert@pobox.com> for letting me peek into their SEMU code :-)
 *
 *****************************************************************************/


#include "emu.h"
#include "sh2.h"




#include <bit>

//#define VERBOSE 1
#include "logmacro.h"

constexpr int SH2_INT_15 = 15;

sh2_device::sh2_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, int cpu_type, address_map_constructor internal_map, int addrlines, uint32_t address_mask)
{
	// S-MU2000: MAME はここでアドレス空間の形（address_space_config）を作っていた。
	// バスは組み立て側が作って set_program_bus() で渡すので、要るのは clock だけ
	set_clock(clock);
	m_cpu_type = cpu_type;
	m_am = address_mask;
}

sh2_device::~sh2_device()
{
}

void sh2_device::device_start()
{
	sh_common_execution::device_start();

	// S-MU2000: MAME は命令フェッチ用に別空間とキャッシュを構えていた。
	// MU2000 は復号なしの単一空間なので、set_program_bus() が両方を指す

	// internals
	save_item(NAME(m_cpu_off));
	save_item(NAME(m_test_irq));
	save_item(NAME(m_irq_line_state));
	save_item(NAME(m_nmi_line_state));
	save_item(NAME(m_internal_irq_vector));

	state_add(STATE_GENPC, "PC", m_sh2_state->pc).mask(m_am).callimport();
	state_add(STATE_GENPCBASE, "CURPC", m_sh2_state->pc).callimport().noshow();

	m_nmi_line_state = 0;

}

void sh2_device::device_reset()
{
	jit_flush();
	std::fill(std::begin(m_sh2_state->r), std::end(m_sh2_state->r), 0);
	std::fill(std::begin(m_irq_line_state), std::end(m_irq_line_state), 0);

	m_sh2_state->pc = m_sh2_state->pr = m_sh2_state->sr = m_sh2_state->gbr = m_sh2_state->vbr = m_sh2_state->mach = m_sh2_state->macl = 0;
	m_sh2_state->evec = m_sh2_state->irqsr = 0;
	m_sh2_state->ea = m_sh2_state->m_delay = 0;
	m_sh2_state->pending_irq = 0;
	m_sh2_state->pending_nmi = 0;
	m_sh2_state->sleep_mode = 0;
	m_sh2_state->internal_irq_level = -1;
	m_sh2_state->sr = SH_I;
	m_sh2_state->pc = read_long(0);
	m_sh2_state->r[15] = read_long(4);

	m_test_irq = 0;
	m_cpu_off = 0;
	m_internal_irq_vector = 0;
	m_cache_dirty = true;
}

// S-MU2000: memory_space_config() と create_disassembler() は MAME のデバッガと
// メモリ機構のためのもの。どちらも持たないので削除した。

uint8_t sh2_device::read_byte(offs_t offset)
{
	if (offset < 0x40000000)
		return m_program->read_byte(offset & m_am);

	return m_program->read_byte(offset);
}

uint16_t sh2_device::read_word(offs_t offset)
{
	if (offset < 0x40000000)
		return m_program->read_word(offset & m_am);

	return m_program->read_word(offset);
}

uint32_t sh2_device::read_long(offs_t offset)
{
	// The cached (0x00000000) and cache-through (0x20000000) windows
	// end up mirroring each other
	if (offset < 0x40000000)
	{
		return m_program->read_dword(offset & m_am);
	}
	return m_program->read_dword(offset);
}

uint16_t sh2_device::decrypted_read_word(offs_t offset)
{
	return m_decrypted_program->read_word(offset);
}

void sh2_device::write_byte(offs_t offset, uint8_t data)
{
	if (offset < 0x40000000)
	{
		m_program->write_byte(offset & m_am, data);
		return;
	}

	m_program->write_byte(offset, data);
}

void sh2_device::write_word(offs_t offset, uint16_t data)
{
	if (offset < 0x40000000)
	{
		m_program->write_word(offset & m_am, data);
		return;
	}

	m_program->write_word(offset, data);
}

void sh2_device::write_long(offs_t offset, uint32_t data)
{
	if (offset < 0x40000000)
	{
		m_program->write_dword(offset & m_am, data);
		return;
	}

	m_program->write_dword(offset, data);
}

void sh2_device::check_pending_irq(const char *message)
{
	if (m_sh2_state->pending_nmi)
	{
		sh2_exception(message, 16);
		m_sh2_state->pending_nmi = 0;
	}
	else
	{
		int irq = m_sh2_state->internal_irq_level;
		if (m_sh2_state->pending_irq)
		{
			int external_irq = std::bit_width(m_sh2_state->pending_irq) - 1;
			if (external_irq >= irq)
			{
				irq = external_irq;
			}
		}

		if (irq >= 0)
		{
			sh2_exception(message, irq);
		}
	}
}

/*  LDC.L   @Rm+,SR */
inline void sh2_device::LDCMSR(const uint16_t opcode) // passes Rn
{
	const uint32_t rn = REG_N;
	m_sh2_state->ea = m_sh2_state->r[rn];
	m_sh2_state->sr = read_long(m_sh2_state->ea) & SH_FLAGS;
	m_sh2_state->r[rn] += 4;
	m_sh2_state->icount -= 2;
	m_test_irq = 1;
}

/*  LDC     Rm,SR */
inline void sh2_device::LDCSR(const uint16_t opcode) // passes Rn
{
	m_sh2_state->sr = m_sh2_state->r[REG_N] & SH_FLAGS;
	m_test_irq = 1;
}

/*  RTE */
inline void sh2_device::RTE()
{
	m_sh2_state->ea = m_sh2_state->r[15];
	m_sh2_state->m_delay = read_long(m_sh2_state->ea);
	m_sh2_state->r[15] += 4;
	m_sh2_state->ea = m_sh2_state->r[15];
	m_sh2_state->sr = read_long(m_sh2_state->ea) & SH_FLAGS;
	m_sh2_state->r[15] += 4;
	m_sh2_state->icount -= 3;
	m_test_irq = 1;
}

/*  TRAPA   #imm */
inline void sh2_device::TRAPA(uint32_t i)
{
	uint32_t imm = i & 0xff;
	debugger_exception_hook(imm);

	m_sh2_state->ea = m_sh2_state->vbr + imm * 4;

	m_sh2_state->r[15] -= 4;
	write_long(m_sh2_state->r[15], m_sh2_state->sr);
	m_sh2_state->r[15] -= 4;
	write_long(m_sh2_state->r[15], m_sh2_state->pc);

	m_sh2_state->pc = read_long(m_sh2_state->ea);

	m_sh2_state->icount -= 7;
}

/*  ILLEGAL */
inline void sh2_device::ILLEGAL()
{
	//logerror("Illegal opcode at %08x\n", m_sh2_state->pc - 2);
	debugger_exception_hook(4);

	m_sh2_state->r[15] -= 4;
	write_long(m_sh2_state->r[15], m_sh2_state->sr);     /* push SR onto stack */
	m_sh2_state->r[15] -= 4;
	write_long(m_sh2_state->r[15], m_sh2_state->pc - 2); /* push PC onto stack */

	/* fetch PC */
	m_sh2_state->pc = read_long(m_sh2_state->vbr + 4 * 4) & m_am;

	/* TODO: timing is a guess */
	m_sh2_state->icount -= 5;
}

void sh2_device::execute_one_f000(uint16_t opcode)
{
	ILLEGAL();
}

void sh2_device::execute_run()
{
	if (m_cpu_off)
	{
		debugger_wait_hook();
		m_sh2_state->icount = 0;
		return;
	}

	const bool use_jit = jit_enabled();
	do
	{
		// S-MU2000: 訳せる所は訳した物で回す。1 命令以上進めて、icount も同じだけ減っている
		if (use_jit && jit_run())
			continue;

		debugger_instruction_hook(m_sh2_state->pc);
		if (jit_trace_on())
			jit_trace(this);

		const uint16_t opcode = m_decrypted_program->read_word(m_sh2_state->pc >= 0x40000000 ? m_sh2_state->pc : m_sh2_state->pc & m_am);

		if (m_sh2_state->m_delay)
		{
			m_sh2_state->pc = m_sh2_state->m_delay;
			m_sh2_state->m_delay = 0;
		}
		else
			m_sh2_state->pc += 2;

		execute_one(opcode);

		if (m_test_irq && !m_sh2_state->m_delay)
		{
			check_pending_irq("mame_sh2_execute");
			m_test_irq = 0;
		}
		m_sh2_state->icount--;
	} while (m_sh2_state->icount > 0);
}

// S-MU2000: init_drc_frontend() は削除（DRC を使わない）



void sh2_device::execute_set_input(int irqline, int state)
{
	if (irqline == INPUT_LINE_NMI)
	{
		if (m_nmi_line_state == state)
			return;

		m_nmi_line_state = state;

		if (state == CLEAR_LINE)
		{
			LOG("SH-2 cleared NMI\n");
		}
		else
		{
			LOG("SH-2 asserted NMI\n");

			m_sh2_state->pending_nmi = 1;

			if (m_sh2_state->m_delay)
				m_test_irq = 1;
			else
				check_pending_irq("sh2_set_nmi_line");
		}
	}
	else
	{
		if (m_irq_line_state[irqline] == state)
			return;

		m_irq_line_state[irqline] = state;

		if (state == CLEAR_LINE)
		{
			LOG("SH-2 cleared irq #%d\n", irqline);
			m_sh2_state->pending_irq &= ~(1 << irqline);
		}
		else
		{
			LOG("SH-2 asserted irq #%d\n", irqline);
			m_sh2_state->pending_irq |= 1 << irqline;

			if (m_sh2_state->m_delay)
				m_test_irq = 1;
			else
				check_pending_irq("sh2_set_irq_line");
		}
	}
}

void sh2_device::sh2_exception(const char *message, int irqline)
{
	// override this at the individual CPU level when special logic is required
	int vector;

	if (irqline != 16)
	{
		if (irqline <= ((m_sh2_state->sr >> 4) & 15)) /* If the cpu forbids this interrupt */
			return;

		// if this is an sh2 internal irq, use its vector
		if (m_sh2_state->internal_irq_level == irqline)
		{
			vector = m_internal_irq_vector;
			/* avoid spurious irqs with this (TODO: needs a better fix) */
			m_sh2_state->internal_irq_level = -1;
			LOG("SH-2 exception #%d (internal vector: $%x) after [%s]\n", irqline, vector, message);
		}
		else
		{
			standard_irq_callback(irqline, m_sh2_state->pc);
			vector = 64 + irqline/2;
			LOG("SH-2 exception #%d (autovector: $%x) after [%s]\n", irqline, vector, message);
		}
	}
	else
	{
		vector = 11;
		LOG("SH-2 nmi exception (autovector: $%x) after [%s]\n", vector, message);
	}

	sh2_exception_internal(message, irqline, vector);
}

void sh2_device::sh2_exception_internal(const char *message, int irqline, int vector)
{
	debugger_exception_hook(vector);

	m_sh2_state->r[15] -= 4;
	write_long(m_sh2_state->r[15], m_sh2_state->sr);     /* push SR onto stack */
	m_sh2_state->r[15] -= 4;
	write_long(m_sh2_state->r[15], m_sh2_state->pc);     /* push PC onto stack */

	/* set I flags in SR */
	if (irqline > SH2_INT_15)
		m_sh2_state->sr = m_sh2_state->sr | SH_I;
	else
		m_sh2_state->sr = (m_sh2_state->sr & ~SH_I) | (irqline << 4);

	/* fetch PC */
	m_sh2_state->pc = read_long(m_sh2_state->vbr + vector * 4) & m_am;

	if (m_sh2_state->sleep_mode == 1)
		m_sh2_state->sleep_mode = 2;
}

// S-MU2000: ここから末尾までの 295 行を削除した。
// func_fastirq / static_generate_entry_point / generate_update_cycles /
// static_generate_memory_accessor はすべて DRC 専用。

void sh2_device::state(state_io &s)
{
	s.tag("sh2");
	sh_common_execution::state(s);
	s.v(m_test_irq); s.v(m_internal_irq_vector); s.v(m_nmi_line_state);
	s.v(m_cpu_off); s.arr(m_irq_line_state);
}
