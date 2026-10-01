// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller
/*****************************************************************************
 *
 *   sh2.h
 *   Portable Hitachi SH-2 (SH7600 family) emulator interface
 *
 *  This work is based on <tiraniddo@hotmail.com> C/C++ implementation of
 *  the SH-2 CPU core and was heavily changed to the MAME CPU requirements.
 *  Thanks also go to Chuck Mason <chukjr@sundail.net> and Olivier Galibert
 *  <galibert@pobox.com> for letting me peek into their SEMU code :-)
 *
 *****************************************************************************/

#ifndef MAME_CPU_SH_SH2_H
#define MAME_CPU_SH_SH2_H

#pragma once

// S-MU2000: MAME 本体の代わりに互換層を使う
#include "state.h"
#include "../../compat/mamecompat.h"

#include "sh.h"

#include <memory>

class sh2_device : public sh_common_execution
{
public:
	// 状態の保存と復元（src/state.h）
	void state(state_io &s);

	void set_frt_input(int state) {} // not every CPU needs this, let the ones that do override it


protected:

	sh2_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, int cpu_type, address_map_constructor internal_map, int addrlines, uint32_t address_mask);
	virtual ~sh2_device();

	void check_pending_irq(const char *message);

	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// device_execute_interface overrides
	virtual uint32_t execute_min_cycles() const noexcept { return 1; }
	virtual uint32_t execute_max_cycles() const noexcept { return 4; }
	virtual uint32_t execute_default_irq_vector(int inputnum) const noexcept { return 0; }
	virtual bool execute_input_edge_triggered(int inputnum) const noexcept { return inputnum == INPUT_LINE_NMI; }
	virtual void execute_run();
	virtual void execute_set_input(int inputnum, int state);


	// device_state_interface overrides


	virtual void sh2_exception(const char *message, int irqline);
	virtual void sh2_exception_internal(const char *message, int irqline, int vector);

	// m_decrypted_program は sh_common_execution が持つ

	uint32_t m_test_irq = 0;
	int32_t m_internal_irq_vector = 0;
	int8_t m_nmi_line_state = 0;

private:
	virtual uint8_t read_byte(offs_t A);
	virtual uint16_t read_word(offs_t A);
	virtual uint32_t read_long(offs_t A);
	virtual uint16_t decrypted_read_word(offs_t offset);
	virtual void write_byte(offs_t A, uint8_t V);
	virtual void write_word(offs_t A, uint16_t V);
	virtual void write_long(offs_t A, uint32_t V);

	virtual void LDCMSR(const uint16_t opcode);
	virtual void LDCSR(const uint16_t opcode);
	virtual void TRAPA(uint32_t i);
	virtual void RTE();
	virtual void ILLEGAL();

	virtual void execute_one_f000(uint16_t opcode);





	uint32_t m_cpu_off = 0;
	int8_t m_irq_line_state[17] = {};

	// S-MU2000: ROM の上の命令を x86-64 に訳して回す（sh2_jit.cpp）
	struct jit;
	static void jit_delete(jit *j);
	static bool jit_enabled();
	static void jit_exec(sh2_device *c, u32 opcode);
	static void jit_irq(sh2_device *c);
	static bool jit_trace_on();
	static void jit_trace(sh2_device *c, u32 at = 0xffffffffu);
	static u32 jit_rb(sh2_device *c, u32 a);
	static u32 jit_rw(sh2_device *c, u32 a);
	static u32 jit_rl(sh2_device *c, u32 a);
	static void jit_wb(sh2_device *c, u32 a, u32 v);
	static void jit_ww(sh2_device *c, u32 a, u32 v);
	static void jit_wl(sh2_device *c, u32 a, u32 v);
	bool jit_run();
	void jit_flush();
	std::unique_ptr<jit, void (*)(jit *)> m_jit{nullptr, &jit_delete};
};

#endif // MAME_CPU_SH_SH2_H
