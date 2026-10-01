// license:BSD-3-Clause
// copyright-holders:David Haywood

#include "emu.h"
#include "sh.h"





#include <bit>
#include <locale>


// S-MU2000: MAME のデバイス生成引数と DRC のメンバを持たないので、
// コンストラクタは既定のもので足りる。


void sh_common_execution::device_start()
{
	/* allocate the implementation-specific state from the full cache */
	// S-MU2000: DRC の drc_cache ではなく、素のメンバを指す
	m_sh2_state = &m_sh2_state_storage;

	save_item(NAME(m_sh2_state->pc));
	save_item(NAME(m_sh2_state->sr));
	save_item(NAME(m_sh2_state->pr));
	save_item(NAME(m_sh2_state->gbr));
	save_item(NAME(m_sh2_state->vbr));
	save_item(NAME(m_sh2_state->mach));
	save_item(NAME(m_sh2_state->macl));
	save_item(NAME(m_sh2_state->r));
	save_item(NAME(m_sh2_state->ea));
	save_item(NAME(m_sh2_state->m_delay));
	save_item(NAME(m_sh2_state->pending_irq));
	save_item(NAME(m_sh2_state->pending_nmi));
	save_item(NAME(m_sh2_state->irqline));
	save_item(NAME(m_sh2_state->evec));
	save_item(NAME(m_sh2_state->irqsr));
	save_item(NAME(m_sh2_state->target));
	save_item(NAME(m_sh2_state->internal_irq_level));
	save_item(NAME(m_sh2_state->sleep_mode));
	save_item(NAME(m_sh2_state->icount));

	m_sh2_state->pc = 0;
	m_sh2_state->pr = 0;
	m_sh2_state->sr = 0;
	m_sh2_state->gbr = 0;
	m_sh2_state->vbr = 0;
	m_sh2_state->mach = 0;
	m_sh2_state->macl = 0;
	memset(m_sh2_state->r, 0, sizeof(m_sh2_state->r));
	m_sh2_state->ea = 0;
	m_sh2_state->m_delay = 0;
	m_sh2_state->pending_irq = 0;
	m_sh2_state->pending_nmi = 0;
	m_sh2_state->irqline = 0;
	m_sh2_state->evec = 0;
	m_sh2_state->irqsr = 0;
	m_sh2_state->target = 0;
	m_sh2_state->internal_irq_level = 0;
	m_sh2_state->icount = 0;
	m_sh2_state->sleep_mode = 0;
	m_sh2_state->arg0 = 0;

	state_add(SH4_PC, "PC", m_sh2_state->pc).formatstr("%08X").callimport();
	state_add(SH_SR, "SR", m_sh2_state->sr).formatstr("%08X").callimport();
	state_add(SH4_PR, "PR", m_sh2_state->pr).formatstr("%08X");
	state_add(SH4_GBR, "GBR", m_sh2_state->gbr).formatstr("%08X");
	state_add(SH4_VBR, "VBR", m_sh2_state->vbr).formatstr("%08X");
	state_add(SH4_MACH, "MACH", m_sh2_state->mach).formatstr("%08X");
	state_add(SH4_MACL, "MACL", m_sh2_state->macl).formatstr("%08X");
	state_add(SH4_R0, "R0", m_sh2_state->r[0]).formatstr("%08X");
	state_add(SH4_R1, "R1", m_sh2_state->r[1]).formatstr("%08X");
	state_add(SH4_R2, "R2", m_sh2_state->r[2]).formatstr("%08X");
	state_add(SH4_R3, "R3", m_sh2_state->r[3]).formatstr("%08X");
	state_add(SH4_R4, "R4", m_sh2_state->r[4]).formatstr("%08X");
	state_add(SH4_R5, "R5", m_sh2_state->r[5]).formatstr("%08X");
	state_add(SH4_R6, "R6", m_sh2_state->r[6]).formatstr("%08X");
	state_add(SH4_R7, "R7", m_sh2_state->r[7]).formatstr("%08X");
	state_add(SH4_R8, "R8", m_sh2_state->r[8]).formatstr("%08X");
	state_add(SH4_R9, "R9", m_sh2_state->r[9]).formatstr("%08X");
	state_add(SH4_R10, "R10", m_sh2_state->r[10]).formatstr("%08X");
	state_add(SH4_R11, "R11", m_sh2_state->r[11]).formatstr("%08X");
	state_add(SH4_R12, "R12", m_sh2_state->r[12]).formatstr("%08X");
	state_add(SH4_R13, "R13", m_sh2_state->r[13]).formatstr("%08X");
	state_add(SH4_R14, "R14", m_sh2_state->r[14]).formatstr("%08X");
	state_add(SH4_R15, "R15", m_sh2_state->r[15]).formatstr("%08X");
	state_add(SH4_EA, "EA", m_sh2_state->ea).formatstr("%08X");

	state_add(SH4_SP, "SP", m_sh2_state->r[15]).noshow();
	state_add(STATE_GENFLAGS, "GENFLAGS", m_sh2_state->sr).formatstr("%20s").noshow();

	set_icountptr(m_sh2_state->icount);

	// S-MU2000: MAME は space(AS_PROGRAM) で自分のアドレス空間を取っていた。
	// こちらは呼び出し側が set_program_bus() で渡す
}


// S-MU2000: drc_start() は削除（DRC を使わない）


/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 1100  1       -
 *  ADD     Rm,Rn
 */
void sh_common_execution::ADD(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] += m_sh2_state->r[m];
}

/*  code                 cycles  t-bit
 *  0111 nnnn iiii iiii  1       -
 *  ADD     #imm,Rn
 */
void sh_common_execution::ADDI(uint32_t i, uint32_t n)
{
	m_sh2_state->r[n] += util::sext(i, 8);
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 1110  1       carry
 *  ADDC    Rm,Rn
 */
void sh_common_execution::ADDC(uint32_t m, uint32_t n)
{
	uint32_t tmp0, tmp1;

	tmp1 = m_sh2_state->r[n] + m_sh2_state->r[m];
	tmp0 = m_sh2_state->r[n];
	m_sh2_state->r[n] = tmp1 + (m_sh2_state->sr & SH_T);
	if (tmp0 > tmp1)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
	if (tmp1 > m_sh2_state->r[n])
		m_sh2_state->sr |= SH_T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 1111  1       overflow
 *  ADDV    Rm,Rn
 */
void sh_common_execution::ADDV(uint32_t m, uint32_t n)
{
	int32_t dest = BIT(m_sh2_state->r[n], 31);
	int32_t src = BIT(m_sh2_state->r[m], 31);
	src += dest;

	m_sh2_state->r[n] += m_sh2_state->r[m];

	int32_t ans = BIT(m_sh2_state->r[n], 31);
	ans += dest;

	if (src != 1)
	{
		if (ans == 1)
			m_sh2_state->sr |= SH_T;
		else
			m_sh2_state->sr &= ~SH_T;
	}
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0010 nnnn mmmm 1001  1       -
 *  AND     Rm,Rn
 */
void sh_common_execution::AND(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] &= m_sh2_state->r[m];
}

/*  code                 cycles  t-bit
 *  1100 1001 iiii iiii  1       -
 *  AND     #imm,R0
 */
void sh_common_execution::ANDI(uint32_t i)
{
	m_sh2_state->r[0] &= i;
}

/*  code                 cycles  t-bit
 *  1100 1101 iiii iiii  1       -
 *  AND.B   #imm,@(R0,GBR)
 */
void sh_common_execution::ANDM(uint32_t i)
{
	m_sh2_state->ea = m_sh2_state->gbr + m_sh2_state->r[0];
	uint32_t temp = i & read_byte(m_sh2_state->ea);
	write_byte(m_sh2_state->ea, temp);
	m_sh2_state->icount -= 2;
}

/*  code                 cycles  t-bit
 *  1000 1011 dddd dddd  3/1     -
 *  BF      disp8
 */
void sh_common_execution::BF(uint32_t d)
{
	if ((m_sh2_state->sr & SH_T) == 0)
	{
		int32_t disp = util::sext(d, 8);
		m_sh2_state->pc = m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
		m_sh2_state->icount -= 2;
	}
}

/*  code                 cycles  t-bit
 *  1000 1111 dddd dddd  3/1     -
 *  BFS     disp8
 */
void sh_common_execution::BFS(uint32_t d)
{
	if ((m_sh2_state->sr & SH_T) == 0)
	{
		int32_t disp = util::sext(d, 8);
		m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
		m_sh2_state->icount--;
	}
}

/*  code                 cycles  t-bit
 *  1010 dddd dddd dddd  2       -
 *  BRA     disp12
 */
void sh_common_execution::BRA(uint32_t d)
{
	int32_t disp = util::sext(d, 12);

#if BUSY_LOOP_HACKS
	if (disp == -2)
	{
		uint32_t next_opcode = read_word(m_sh2_state->pc & m_am);
		/* BRA  $
		 * NOP
		 */
		if (next_opcode == 0x0009)
			m_sh2_state->icount %= 3;   /* cycles for BRA $ and NOP taken (3) */
	}
#endif
	m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
	m_sh2_state->icount--;
}

/*  code                 cycles  t-bit
 *  0000 mmmm 0010 0011  2       -
 *  BRAF    Rm
 */
void sh_common_execution::BRAF(uint32_t m)
{
	m_sh2_state->m_delay = m_sh2_state->pc + m_sh2_state->r[m] + 2;
	m_sh2_state->icount--;
}

/*  code                 cycles  t-bit
 *  1011 dddd dddd dddd  2       -
 *  BSR     disp12
 */
void sh_common_execution::BSR(uint32_t d)
{
	int32_t disp = util::sext(d, 12);

	m_sh2_state->pr = m_sh2_state->pc + 2;
	m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
	m_sh2_state->icount--;
}

/*  code                 cycles  t-bit
 *  0000 mmmm 0000 0011  2       -
 *  BSRF    Rm
 */
void sh_common_execution::BSRF(uint32_t m)
{
	m_sh2_state->pr = m_sh2_state->pc + 2;
	m_sh2_state->m_delay = m_sh2_state->pc + m_sh2_state->r[m] + 2;
	m_sh2_state->icount--;
}

/*  code                 cycles  t-bit
 *  1000 1001 dddd dddd  3/1     -
 *  BT      disp8
 */
void sh_common_execution::BT(uint32_t d)
{
	if ((m_sh2_state->sr & SH_T) != 0)
	{
		int32_t disp = util::sext(d, 8);
		m_sh2_state->pc = m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
		m_sh2_state->icount -= 2;
	}
}

/*  code                 cycles  t-bit
 *  1000 1101 dddd dddd  2/1     -
 *  BTS     disp8
 */
void sh_common_execution::BTS(uint32_t d)
{
	if ((m_sh2_state->sr & SH_T) != 0)
	{
		int32_t disp = util::sext(d, 8);
		m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
		m_sh2_state->icount--;
	}
}

/*  code                 cycles  t-bit
 *  0000 0000 0010 1000  1       -
 *  CLRMAC
 */
void sh_common_execution::CLRMAC()
{
	m_sh2_state->mach = 0;
	m_sh2_state->macl = 0;
}

/*  code                 cycles  t-bit
 *  0000 0000 0000 1000  1       -
 *  CLRT
 */
void sh_common_execution::CLRT()
{
	m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0000  1       comparison result
 *  CMP_EQ  Rm,Rn
 */
void sh_common_execution::CMPEQ(uint32_t m, uint32_t n)
{
	if (m_sh2_state->r[n] == m_sh2_state->r[m])
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0011  1       comparison result
 *  CMP_GE  Rm,Rn
 */
void sh_common_execution::CMPGE(uint32_t m, uint32_t n)
{
	if ((int32_t)m_sh2_state->r[n] >= (int32_t)m_sh2_state->r[m])
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0111  1       comparison result
 *  CMP_GT  Rm,Rn
 */
void sh_common_execution::CMPGT(uint32_t m, uint32_t n)
{
	if ((int32_t)m_sh2_state->r[n] > (int32_t)m_sh2_state->r[m])
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0110  1       comparison result
 *  CMP_HI  Rm,Rn
 */
void sh_common_execution::CMPHI(uint32_t m, uint32_t n)
{
	if (m_sh2_state->r[n] > m_sh2_state->r[m])
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0010  1       comparison result
 *  CMP_HS  Rm,Rn
 */
void sh_common_execution::CMPHS(uint32_t m, uint32_t n)
{
	if (m_sh2_state->r[n] >= m_sh2_state->r[m])
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0100 nnnn 0001 0101  1       comparison result
 *  CMP_PL  Rn
 */
void sh_common_execution::CMPPL(uint32_t n)
{
	if ((int32_t)m_sh2_state->r[n] > 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0100 nnnn 0001 0001  1       comparison result
 *  CMP_PZ  Rn
 */
void sh_common_execution::CMPPZ(uint32_t n)
{
	if ((int32_t)m_sh2_state->r[n] >= 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0010 nnnn mmmm 1100  1       comparison result
 * CMP_STR  Rm,Rn
 */
void sh_common_execution::CMPSTR(uint32_t m, uint32_t n)
{
	uint32_t temp = m_sh2_state->r[n] ^ m_sh2_state->r[m];
	uint8_t upper_byte = (temp >> 24) & 0xff;
	uint8_t mid_upper_byte = (temp >> 16) & 0xff;
	uint8_t mid_lower_byte = (temp >> 8) & 0xff;
	uint8_t lower_byte = temp & 0xff;
	if (upper_byte && mid_upper_byte && mid_lower_byte && lower_byte)
		m_sh2_state->sr &= ~SH_T;
	else
		m_sh2_state->sr |= SH_T;
}

/*  code                 cycles  t-bit
 *  1000 1000 iiii iiii  1       comparison result
 *  CMP/EQ #imm,R0
 */
void sh_common_execution::CMPIM(uint32_t i)
{
	uint32_t imm = (uint32_t)util::sext(i, 8);

	if (m_sh2_state->r[0] == imm)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0010 nnnn mmmm 0111  1       calculation result
 *  DIV0S   Rm,Rn
 */
void sh_common_execution::DIV0S(uint32_t m, uint32_t n)
{
	if (!BIT(m_sh2_state->r[n], 31))
		m_sh2_state->sr &= ~SH_Q;
	else
		m_sh2_state->sr |= SH_Q;

	if (!BIT(m_sh2_state->r[m], 31))
		m_sh2_state->sr &= ~SH_M;
	else
		m_sh2_state->sr |= SH_M;

	if (BIT(m_sh2_state->r[m] ^ m_sh2_state->r[n], 31))
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  code                 cycles  t-bit
 *  0000 0000 0001 1001  1       0
 *  DIV0U
 */
void sh_common_execution::DIV0U()
{
	m_sh2_state->sr &= ~(SH_M | SH_Q | SH_T);
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0100  1       calculation result
 *  DIV1 Rm,Rn
 */
void sh_common_execution::DIV1(uint32_t m, uint32_t n)
{
	uint32_t old_q = m_sh2_state->sr & SH_Q;
	if (0x80000000 & m_sh2_state->r[n])
		m_sh2_state->sr |= SH_Q;
	else
		m_sh2_state->sr &= ~SH_Q;

	m_sh2_state->r[n] = (m_sh2_state->r[n] << 1) | (m_sh2_state->sr & SH_T);

	if (!old_q)
	{
		if (!(m_sh2_state->sr & SH_M))
		{
			uint32_t tmp = m_sh2_state->r[n];
			m_sh2_state->r[n] -= m_sh2_state->r[m];
			if (!(m_sh2_state->sr & SH_Q))
				if (m_sh2_state->r[n] > tmp)
					m_sh2_state->sr |= SH_Q;
				else
					m_sh2_state->sr &= ~SH_Q;
			else
				if (m_sh2_state->r[n] > tmp)
					m_sh2_state->sr &= ~SH_Q;
				else
					m_sh2_state->sr |= SH_Q;
		}
		else
		{
			uint32_t tmp = m_sh2_state->r[n];
			m_sh2_state->r[n] += m_sh2_state->r[m];
			if (!(m_sh2_state->sr & SH_Q))
			{
				if (m_sh2_state->r[n] < tmp)
					m_sh2_state->sr &= ~SH_Q;
				else
					m_sh2_state->sr |= SH_Q;
			}
			else
			{
				if (m_sh2_state->r[n] < tmp)
					m_sh2_state->sr |= SH_Q;
				else
					m_sh2_state->sr &= ~SH_Q;
			}
		}
	}
	else
	{
		if (!(m_sh2_state->sr & SH_M))
		{
			uint32_t tmp = m_sh2_state->r[n];
			m_sh2_state->r[n] += m_sh2_state->r[m];
			if (!(m_sh2_state->sr & SH_Q))
				if (m_sh2_state->r[n] < tmp)
					m_sh2_state->sr |= SH_Q;
				else
					m_sh2_state->sr &= ~SH_Q;
			else
				if (m_sh2_state->r[n] < tmp)
					m_sh2_state->sr &= ~SH_Q;
				else
					m_sh2_state->sr |= SH_Q;
		}
		else
		{
			uint32_t tmp = m_sh2_state->r[n];
			m_sh2_state->r[n] -= m_sh2_state->r[m];
			if (!(m_sh2_state->sr & SH_Q))
				if (m_sh2_state->r[n] > tmp)
					m_sh2_state->sr &= ~SH_Q;
				else
					m_sh2_state->sr |= SH_Q;
			else
				if (m_sh2_state->r[n] > tmp)
					m_sh2_state->sr |= SH_Q;
				else
					m_sh2_state->sr &= ~SH_Q;
		}
	}

	uint32_t tmp = (m_sh2_state->sr & (SH_Q | SH_M));
	if (tmp == 0 || tmp == 0x300) /* if Q == M set T else clear T */
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  DMULS.L Rm,Rn */
void sh_common_execution::DMULS(uint32_t m, uint32_t n)
{
	int32_t tempn = (int32_t)m_sh2_state->r[n];
	int32_t tempm = (int32_t)m_sh2_state->r[m];
	bool fnlml = (bool)BIT(tempn ^ tempm, 31);

	if (tempn < 0)
		tempn = 0 - tempn;
	if (tempm < 0)
		tempm = 0 - tempm;

	uint32_t rn_l = (uint32_t)tempn & 0x0000ffff;
	uint32_t rn_h = (uint32_t)tempn >> 16;
	uint32_t rm_l = (uint32_t)tempm & 0x0000ffff;
	uint32_t rm_h = (uint32_t)tempm >> 16;

	uint32_t temp0 = rm_l * rn_l;
	uint32_t temp1 = rm_h * rn_l;
	uint32_t temp2 = rm_l * rn_h;
	uint32_t temp3 = rm_h * rn_h;

	uint32_t res2 = 0;
	uint32_t res1 = temp1 + temp2;
	if (res1 < temp1)
		res2 += 0x00010000;
	temp1 = res1 << 16;

	uint32_t res0 = temp0 + temp1;
	if (res0 < temp0)
		res2++;
	res2 = res2 + (res1 >> 16) + temp3;

	if (fnlml)
	{
		res2 = ~res2;
		if (res0 == 0)
			res2++;
		else
			res0 = (~res0) + 1;
	}

	m_sh2_state->mach = res2;
	m_sh2_state->macl = res0;
	m_sh2_state->icount--;
}

/*  DMULU.L Rm,Rn */
void sh_common_execution::DMULU(uint32_t m, uint32_t n)
{
	uint32_t rn_l = m_sh2_state->r[n] & 0x0000ffff;
	uint32_t rn_h = m_sh2_state->r[n] >> 16;
	uint32_t rm_l = m_sh2_state->r[m] & 0x0000ffff;
	uint32_t rm_h = m_sh2_state->r[m] >> 16;

	uint32_t temp0 = rm_l * rn_l;
	uint32_t temp1 = rm_h * rn_l;
	uint32_t temp2 = rm_l * rn_h;
	uint32_t temp3 = rm_h * rn_h;

	uint32_t res2 = 0;
	uint32_t res1 = temp1 + temp2;
	if (res1 < temp1)
		res2 += 0x00010000;

	temp1 = res1 << 16;
	uint32_t res0 = temp0 + temp1;
	if (res0 < temp0)
		res2++;

	res2 = res2 + (res1 >> 16) + temp3;

	m_sh2_state->mach = res2;
	m_sh2_state->macl = res0;
	m_sh2_state->icount--;
}

/*  DT      Rn */
void sh_common_execution::DT(uint32_t n)
{
	m_sh2_state->r[n]--;
	if (m_sh2_state->r[n] == 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
#if BUSY_LOOP_HACKS
	{
		uint32_t next_opcode = read_word(m_sh2_state->pc & AM);
		/* DT   Rn
		 * BF   $-2
		 */
		if (next_opcode == 0x8bfd)
		{
			while (m_sh2_state->r[n] > 1 && m_sh2_state->icount > 4)
			{
				m_sh2_state->r[n]--;
				m_sh2_state->icount -= 4;   /* cycles for DT (1) and BF taken (3) */
			}
		}
	}
#endif
}

/*  EXTS.B  Rm,Rn */
void sh_common_execution::EXTSB(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = util::sext(m_sh2_state->r[m], 8);
}

/*  EXTS.W  Rm,Rn */
void sh_common_execution::EXTSW(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = util::sext(m_sh2_state->r[m], 16);
}

/*  EXTU.B  Rm,Rn */
void sh_common_execution::EXTUB(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->r[m] & 0x000000ff;
}

/*  EXTU.W  Rm,Rn */
void sh_common_execution::EXTUW(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->r[m] & 0x0000ffff;
}

/*  JMP     @Rm */
void sh_common_execution::JMP(uint32_t m)
{
	m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->icount--;
}

/*  JSR     @Rm */
void sh_common_execution::JSR(uint32_t m)
{
	m_sh2_state->pr = m_sh2_state->pc + 2;
	m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->icount--;
}

/*  LDC     Rm,GBR */
void sh_common_execution::LDCGBR(uint32_t m)
{
	m_sh2_state->gbr = m_sh2_state->r[m];
}

/*  LDC     Rm,VBR */
void sh_common_execution::LDCVBR(uint32_t m)
{
	m_sh2_state->vbr = m_sh2_state->r[m];
}

/*  LDC.L   @Rm+,GBR */
void sh_common_execution::LDCMGBR(uint32_t m)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->gbr = read_long(m_sh2_state->ea);
	m_sh2_state->r[m] += 4;
	m_sh2_state->icount -= 2;
}

/*  LDC.L   @Rm+,VBR */
void sh_common_execution::LDCMVBR(uint32_t m)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->vbr = read_long(m_sh2_state->ea);
	m_sh2_state->r[m] += 4;
	m_sh2_state->icount -= 2;
}

/*  LDS     Rm,MACH */
void sh_common_execution::LDSMACH(uint32_t m)
{
	m_sh2_state->mach = m_sh2_state->r[m];
}

/*  LDS     Rm,MACL */
void sh_common_execution::LDSMACL(uint32_t m)
{
	m_sh2_state->macl = m_sh2_state->r[m];
}

/*  LDS     Rm,PR */
void sh_common_execution::LDSPR(uint32_t m)
{
	m_sh2_state->pr = m_sh2_state->r[m];
}

/*  LDS.L   @Rm+,MACH */
void sh_common_execution::LDSMMACH(uint32_t m)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->mach = read_long(m_sh2_state->ea);
	m_sh2_state->r[m] += 4;
}

/*  LDS.L   @Rm+,MACL */
void sh_common_execution::LDSMMACL(uint32_t m)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->macl = read_long(m_sh2_state->ea);
	m_sh2_state->r[m] += 4;
}

/*  LDS.L   @Rm+,PR */
void sh_common_execution::LDSMPR(uint32_t m)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->pr = read_long(m_sh2_state->ea);
	m_sh2_state->r[m] += 4;
}

/*  MAC.L   @Rm+,@Rn+ */
void sh_common_execution::MAC_L(uint32_t m, uint32_t n)
{
	int32_t tempn = (int32_t)read_long(m_sh2_state->r[n]);
	m_sh2_state->r[n] += 4;

	int32_t tempm = (int32_t)read_long(m_sh2_state->r[m]);
	m_sh2_state->r[m] += 4;

	bool fnlml = BIT(tempn ^ tempm, 31);

	if (tempn < 0)
		tempn = 0 - tempn;
	if (tempm < 0)
		tempm = 0 - tempm;

	uint32_t rn_l = (uint32_t)tempn & 0x0000ffff;
	uint32_t rn_h = (uint32_t)tempn >> 16;
	uint32_t rm_l = (uint32_t)tempm & 0x0000ffff;
	uint32_t rm_h = (uint32_t)tempm >> 16;

	uint32_t temp0 = rm_l * rn_l;
	uint32_t temp1 = rm_h * rn_l;
	uint32_t temp2 = rm_l * rn_h;
	uint32_t temp3 = rm_h * rn_h;

	uint32_t res2 = 0;
	uint32_t res1 = temp1 + temp2;
	if (res1 < temp1)
		res2 += 0x00010000;
	temp1 = res1 << 16;

	uint32_t res0 = temp0 + temp1;
	if (res0 < temp0)
		res2++;
	res2 = res2 + (res1 >> 16) + temp3;

	if (fnlml)
	{
		res2 = ~res2;
		if (res0 == 0)
			res2++;
		else
			res0 = (~res0) + 1;
	}

	if (m_sh2_state->sr & SH_S)
	{
		// full 64-bit accumulate, saturated to the 48-bit range with the sign taken from the product
		uint64_t sum = (((uint64_t)m_sh2_state->mach << 32) | m_sh2_state->macl) + (((uint64_t)res2 << 32) | res0);

		if (sum > 0x00007fffffffffffULL && sum < 0xffff800000000000ULL)
			sum = fnlml ? 0xffff800000000000ULL : 0x00007fffffffffffULL;

		m_sh2_state->mach = (uint32_t)(sum >> 32);
		m_sh2_state->macl = (uint32_t)sum;
	}
	else
	{
		res0 = m_sh2_state->macl + res0;
		if (m_sh2_state->macl > res0)
			res2++;
		res2 += m_sh2_state->mach;
		m_sh2_state->mach = res2;
		m_sh2_state->macl = res0;
	}
	m_sh2_state->icount -= 2;
}

/*  MAC.W   @Rm+,@Rn+ */
void sh_common_execution::MAC_W(uint32_t m, uint32_t n)
{
	int32_t tempn = (int32_t)(int16_t)read_word(m_sh2_state->r[n]);
	m_sh2_state->r[n] += 2;

	int32_t tempm = (int32_t)(int16_t)read_word(m_sh2_state->r[m]);
	m_sh2_state->r[m] += 2;

	uint32_t templ = m_sh2_state->macl;
	tempm *= tempn;

	int32_t dest = BIT(m_sh2_state->macl, 31);
	int32_t src = BIT(tempm, 31) + dest;
	tempn = BIT(tempm, 31) ? -1 : 0;

	m_sh2_state->macl += tempm;

	int32_t ans = BIT(m_sh2_state->macl, 31) + dest;

	if (m_sh2_state->sr & SH_S)
	{
		if (ans == 1)
		{
			// src 0 or 2 means both addends had the same sign, so the sign change is an overflow
			if (src == 0)
			{
				m_sh2_state->macl = 0x7fffffff;
				m_sh2_state->mach |= 1;
			}
			else if (src == 2)
			{
				m_sh2_state->macl = 0x80000000;
				m_sh2_state->mach |= 1;
			}
		}
	}
	else
	{
		m_sh2_state->mach += tempn;
		if (templ > m_sh2_state->macl)
			m_sh2_state->mach += 1;
	}
	m_sh2_state->icount -= 2;
}

/*  MOV     Rm,Rn */
void sh_common_execution::MOV(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->r[m];
}

/*  MOV.B   Rm,@Rn */
void sh_common_execution::MOVBS(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n];
	write_byte(m_sh2_state->ea, m_sh2_state->r[m] & 0x000000ff);
}

/*  MOV.W   Rm,@Rn */
void sh_common_execution::MOVWS(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n];
	write_word(m_sh2_state->ea, m_sh2_state->r[m] & 0x0000ffff);
}

/*  MOV.L   Rm,@Rn */
void sh_common_execution::MOVLS(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->r[m]);
}

/*  MOV.B   @Rm,Rn */
void sh_common_execution::MOVBL(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->r[n] = (uint32_t)util::sext(read_byte(m_sh2_state->ea), 8);
}

/*  MOV.W   @Rm,Rn */
void sh_common_execution::MOVWL(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->r[n] = (uint32_t)util::sext(read_word(m_sh2_state->ea), 16);
}

/*  MOV.L   @Rm,Rn */
void sh_common_execution::MOVLL(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[m];
	m_sh2_state->r[n] = read_long(m_sh2_state->ea);
}

/*  MOV.B   Rm,@-Rn */
void sh_common_execution::MOVBM(uint32_t m, uint32_t n)
{
	uint8_t data(m_sh2_state->r[m]);

	m_sh2_state->r[n] -= 1;
	write_byte(m_sh2_state->r[n], data);
}

/*  MOV.W   Rm,@-Rn */
void sh_common_execution::MOVWM(uint32_t m, uint32_t n)
{
	uint16_t data(m_sh2_state->r[m]);

	m_sh2_state->r[n] -= 2;
	write_word(m_sh2_state->r[n], data);
}

/*  MOV.L   Rm,@-Rn */
void sh_common_execution::MOVLM(uint32_t m, uint32_t n)
{
	uint32_t data = m_sh2_state->r[m];

	m_sh2_state->r[n] -= 4;
	write_long(m_sh2_state->r[n], data);
}

/*  MOV.B   @Rm+,Rn */
void sh_common_execution::MOVBP(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = (uint32_t)util::sext(read_byte(m_sh2_state->r[m]), 8);
	if (n != m)
		m_sh2_state->r[m] += 1;
}

/*  MOV.W   @Rm+,Rn */
void sh_common_execution::MOVWP(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = (uint32_t)util::sext(read_word(m_sh2_state->r[m]), 16);
	if (n != m)
		m_sh2_state->r[m] += 2;
}

/*  MOV.L   @Rm+,Rn */
void sh_common_execution::MOVLP(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = read_long(m_sh2_state->r[m]);
	if (n != m)
		m_sh2_state->r[m] += 4;
}

/*  MOV.B   Rm,@(R0,Rn) */
void sh_common_execution::MOVBS0(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n] + m_sh2_state->r[0];
	write_byte(m_sh2_state->ea, (uint8_t)m_sh2_state->r[m]);
}

/*  MOV.W   Rm,@(R0,Rn) */
void sh_common_execution::MOVWS0(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n] + m_sh2_state->r[0];
	write_word(m_sh2_state->ea, (uint16_t)m_sh2_state->r[m]);
}

/*  MOV.L   Rm,@(R0,Rn) */
void sh_common_execution::MOVLS0(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n] + m_sh2_state->r[0];
	write_long(m_sh2_state->ea, m_sh2_state->r[m]);
}

/*  MOV.B   @(R0,Rm),Rn */
void sh_common_execution::MOVBL0(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[m] + m_sh2_state->r[0];
	m_sh2_state->r[n] = (uint32_t)util::sext(read_byte(m_sh2_state->ea), 8);
}

/*  MOV.W   @(R0,Rm),Rn */
void sh_common_execution::MOVWL0(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[m] + m_sh2_state->r[0];
	m_sh2_state->r[n] = (uint32_t)util::sext(read_word(m_sh2_state->ea), 16);
}

/*  MOV.L   @(R0,Rm),Rn */
void sh_common_execution::MOVLL0(uint32_t m, uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[m] + m_sh2_state->r[0];
	m_sh2_state->r[n] = read_long(m_sh2_state->ea);
}

/*  MOV     #imm,Rn */
void sh_common_execution::MOVI(uint32_t i, uint32_t n)
{
	m_sh2_state->r[n] = (uint32_t)util::sext(i, 8);
}

/*  MOV.W   @(disp8,PC),Rn */
void sh_common_execution::MOVWI(uint32_t d, uint32_t n)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->pc + disp * 2 + 2;
	m_sh2_state->r[n] = (uint32_t)util::sext(read_word(m_sh2_state->ea), 16);
}

/*  MOV.L   @(disp8,PC),Rn */
void sh_common_execution::MOVLI(uint32_t d, uint32_t n)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = ((m_sh2_state->pc + 2) & ~3) + disp * 4;
	m_sh2_state->r[n] = read_long(m_sh2_state->ea);
}

/*  MOV.B   @(disp8,GBR),R0 */
void sh_common_execution::MOVBLG(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->gbr + disp;
	m_sh2_state->r[0] = (uint32_t)util::sext(read_byte(m_sh2_state->ea), 8);
}

/*  MOV.W   @(disp8,GBR),R0 */
void sh_common_execution::MOVWLG(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->gbr + disp * 2;
	m_sh2_state->r[0] = (int32_t)util::sext(read_word(m_sh2_state->ea), 16);
}

/*  MOV.L   @(disp8,GBR),R0 */
void sh_common_execution::MOVLLG(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->gbr + disp * 4;
	m_sh2_state->r[0] = read_long(m_sh2_state->ea);
}

/*  MOV.B   R0,@(disp8,GBR) */
void sh_common_execution::MOVBSG(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->gbr + disp;
	write_byte(m_sh2_state->ea, (uint8_t)m_sh2_state->r[0]);
}

/*  MOV.W   R0,@(disp8,GBR) */
void sh_common_execution::MOVWSG(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->gbr + disp * 2;
	write_word(m_sh2_state->ea, (uint16_t)m_sh2_state->r[0]);
}

/*  MOV.L   R0,@(disp8,GBR) */
void sh_common_execution::MOVLSG(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = m_sh2_state->gbr + disp * 4;
	write_long(m_sh2_state->ea, m_sh2_state->r[0]);
}

/*  MOV.B   R0,@(disp4,Rn) */
void sh_common_execution::MOVBS4(uint32_t d, uint32_t n)
{
	uint32_t disp = d & 0x0f;
	m_sh2_state->ea = m_sh2_state->r[n] + disp;
	write_byte(m_sh2_state->ea, (uint8_t)m_sh2_state->r[0]);
}

/*  MOV.W   R0,@(disp4,Rn) */
void sh_common_execution::MOVWS4(uint32_t d, uint32_t n)
{
	uint32_t disp = d & 0x0f;
	m_sh2_state->ea = m_sh2_state->r[n] + disp * 2;
	write_word(m_sh2_state->ea, (uint16_t)m_sh2_state->r[0]);
}

/* MOV.L Rm,@(disp4,Rn) */
void sh_common_execution::MOVLS4(uint32_t m, uint32_t d, uint32_t n)
{
	uint32_t disp = d & 0x0f;
	m_sh2_state->ea = m_sh2_state->r[n] + disp * 4;
	write_long(m_sh2_state->ea, m_sh2_state->r[m]);
}

/*  MOV.B   @(disp4,Rm),R0 */
void sh_common_execution::MOVBL4(uint32_t m, uint32_t d)
{
	uint32_t disp = d & 0x0f;
	m_sh2_state->ea = m_sh2_state->r[m] + disp;
	m_sh2_state->r[0] = (uint32_t)util::sext(read_byte(m_sh2_state->ea), 8);
}

/*  MOV.W   @(disp4,Rm),R0 */
void sh_common_execution::MOVWL4(uint32_t m, uint32_t d)
{
	uint32_t disp = d & 0x0f;
	m_sh2_state->ea = m_sh2_state->r[m] + disp * 2;
	m_sh2_state->r[0] = (uint32_t)util::sext(read_word(m_sh2_state->ea), 16);
}

/*  MOV.L   @(disp4,Rm),Rn */
void sh_common_execution::MOVLL4(uint32_t m, uint32_t d, uint32_t n)
{
	uint32_t disp = d & 0x0f;
	m_sh2_state->ea = m_sh2_state->r[m] + disp * 4;
	m_sh2_state->r[n] = read_long(m_sh2_state->ea);
}

/*  MOVA    @(disp8,PC),R0 */
void sh_common_execution::MOVA(uint32_t d)
{
	uint32_t disp = d & 0xff;
	m_sh2_state->ea = ((m_sh2_state->pc + 2) & ~3) + disp * 4;
	m_sh2_state->r[0] = m_sh2_state->ea;
}

/*  MOVT    Rn */
void sh_common_execution::MOVT(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->sr & SH_T;
}

/*  MUL.L   Rm,Rn */
void sh_common_execution::MULL(uint32_t m, uint32_t n)
{
	m_sh2_state->macl = m_sh2_state->r[n] * m_sh2_state->r[m];
	m_sh2_state->icount--;
}

/*  MULS    Rm,Rn */
void sh_common_execution::MULS(uint32_t m, uint32_t n)
{
	m_sh2_state->macl = (int16_t)m_sh2_state->r[n] * (int16_t)m_sh2_state->r[m];
}

/*  MULU    Rm,Rn */
void sh_common_execution::MULU(uint32_t m, uint32_t n)
{
	m_sh2_state->macl = (uint16_t)m_sh2_state->r[n] * (uint16_t)m_sh2_state->r[m];
}

/*  NEG     Rm,Rn */
void sh_common_execution::NEG(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = 0 - m_sh2_state->r[m];
}

/*  NEGC    Rm,Rn */
void sh_common_execution::NEGC(uint32_t m, uint32_t n)
{
	uint32_t temp = m_sh2_state->r[m];
	m_sh2_state->r[n] = -temp - (m_sh2_state->sr & SH_T);
	if (temp || (m_sh2_state->sr & SH_T))
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  NOP */
void sh_common_execution::NOP(void)
{
}

/*  NOT     Rm,Rn */
void sh_common_execution::NOT(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = ~m_sh2_state->r[m];
}

/*  OR      Rm,Rn */
void sh_common_execution::OR(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] |= m_sh2_state->r[m];
}

/*  OR      #imm,R0 */
void sh_common_execution::ORI(uint32_t i)
{
	m_sh2_state->r[0] |= i;
}

/*  OR.B    #imm,@(R0,GBR) */
void sh_common_execution::ORM(uint32_t i)
{
	m_sh2_state->ea = m_sh2_state->gbr + m_sh2_state->r[0];
	write_byte(m_sh2_state->ea, read_byte(m_sh2_state->ea) | (uint8_t)i);
	m_sh2_state->icount -= 2;
}

/*  ROTCL   Rn */
void sh_common_execution::ROTCL(uint32_t n)
{
	uint32_t temp = (m_sh2_state->r[n] >> 31) & SH_T;
	m_sh2_state->r[n] = (m_sh2_state->r[n] << 1) | (m_sh2_state->sr & SH_T);
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | temp;
}

/*  ROTCR   Rn */
void sh_common_execution::ROTCR(uint32_t n)
{
	uint32_t temp = (m_sh2_state->sr & SH_T) << 31;
	if (m_sh2_state->r[n] & SH_T)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
	m_sh2_state->r[n] = (m_sh2_state->r[n] >> 1) | temp;
}

/*  ROTL    Rn */
void sh_common_execution::ROTL(uint32_t n)
{
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | ((m_sh2_state->r[n] >> 31) & SH_T);
	m_sh2_state->r[n] = std::rotl(m_sh2_state->r[n], 1);
}

/*  ROTR    Rn */
void sh_common_execution::ROTR(uint32_t n)
{
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | (m_sh2_state->r[n] & SH_T);
	m_sh2_state->r[n] = std::rotr(m_sh2_state->r[n], 1);
}

/*  RTS */
void sh_common_execution::RTS()
{
	m_sh2_state->m_delay = m_sh2_state->ea = m_sh2_state->pr;
	m_sh2_state->icount--;
}

/*  SETT */
void sh_common_execution::SETT()
{
	m_sh2_state->sr |= SH_T;
}

/*  SHAL    Rn      (same as SHLL) */
void sh_common_execution::SHAL(uint32_t n)
{
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | ((m_sh2_state->r[n] >> 31) & SH_T);
	m_sh2_state->r[n] <<= 1;
}

/*  SHAR    Rn */
void sh_common_execution::SHAR(uint32_t n)
{
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | (m_sh2_state->r[n] & SH_T);
	m_sh2_state->r[n] = (uint32_t)((int32_t)m_sh2_state->r[n] >> 1);
}

/*  SHLL    Rn      (same as SHAL) */
void sh_common_execution::SHLL(uint32_t n)
{
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | ((m_sh2_state->r[n] >> 31) & SH_T);
	m_sh2_state->r[n] <<= 1;
}

/*  SHLL2   Rn */
void sh_common_execution::SHLL2(uint32_t n)
{
	m_sh2_state->r[n] <<= 2;
}

/*  SHLL8   Rn */
void sh_common_execution::SHLL8(uint32_t n)
{
	m_sh2_state->r[n] <<= 8;
}

/*  SHLL16  Rn */
void sh_common_execution::SHLL16(uint32_t n)
{
	m_sh2_state->r[n] <<= 16;
}

/*  SHLR    Rn */
void sh_common_execution::SHLR(uint32_t n)
{
	m_sh2_state->sr = (m_sh2_state->sr & ~SH_T) | (m_sh2_state->r[n] & SH_T);
	m_sh2_state->r[n] >>= 1;
}

/*  SHLR2   Rn */
void sh_common_execution::SHLR2(uint32_t n)
{
	m_sh2_state->r[n] >>= 2;
}

/*  SHLR8   Rn */
void sh_common_execution::SHLR8(uint32_t n)
{
	m_sh2_state->r[n] >>= 8;
}

/*  SHLR16  Rn */
void sh_common_execution::SHLR16(uint32_t n)
{
	m_sh2_state->r[n] >>= 16;
}


/*  STC     SR,Rn */
void sh_common_execution::STCSR(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->sr;
}

/*  STC     GBR,Rn */
void sh_common_execution::STCGBR(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->gbr;
}

/*  STC     VBR,Rn */
void sh_common_execution::STCVBR(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->vbr;
}

/*  STC.L   SR,@-Rn */
void sh_common_execution::STCMSR(uint32_t n)
{
	m_sh2_state->r[n] -= 4;
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->sr);
	m_sh2_state->icount--;
}

/*  STC.L   GBR,@-Rn */
void sh_common_execution::STCMGBR(uint32_t n)
{
	m_sh2_state->r[n] -= 4;
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->gbr);
	m_sh2_state->icount--;
}

/*  STC.L   VBR,@-Rn */
void sh_common_execution::STCMVBR(uint32_t n)
{
	m_sh2_state->r[n] -= 4;
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->vbr);
	m_sh2_state->icount--;
}

/*  STS     MACH,Rn */
void sh_common_execution::STSMACH(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->mach;
}

/*  STS     MACL,Rn */
void sh_common_execution::STSMACL(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->macl;
}

/*  STS     PR,Rn */
void sh_common_execution::STSPR(uint32_t n)
{
	m_sh2_state->r[n] = m_sh2_state->pr;
}

/*  STS.L   MACH,@-Rn */
void sh_common_execution::STSMMACH(uint32_t n)
{
	m_sh2_state->r[n] -= 4;
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->mach);
}

/*  STS.L   MACL,@-Rn */
void sh_common_execution::STSMMACL(uint32_t n)
{
	m_sh2_state->r[n] -= 4;
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->macl);
}

/*  STS.L   PR,@-Rn */
void sh_common_execution::STSMPR(uint32_t n)
{
	m_sh2_state->r[n] -= 4;
	m_sh2_state->ea = m_sh2_state->r[n];
	write_long(m_sh2_state->ea, m_sh2_state->pr);
}

/*  SUB     Rm,Rn */
void sh_common_execution::SUB(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] -= m_sh2_state->r[m];
}

/*  SUBC    Rm,Rn */
void sh_common_execution::SUBC(uint32_t m, uint32_t n)
{
	uint32_t tmp1 = m_sh2_state->r[n] - m_sh2_state->r[m];
	uint32_t tmp0 = m_sh2_state->r[n];
	m_sh2_state->r[n] = tmp1 - (m_sh2_state->sr & SH_T);
	if (tmp0 < tmp1)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
	if (tmp1 < m_sh2_state->r[n])
		m_sh2_state->sr |= SH_T;
}

/*  SUBV    Rm,Rn */
void sh_common_execution::SUBV(uint32_t m, uint32_t n)
{
	int32_t dest = BIT(m_sh2_state->r[n], 31);
	int32_t src = BIT(m_sh2_state->r[m], 31);
	src += dest;

	m_sh2_state->r[n] -= m_sh2_state->r[m];

	int32_t ans = BIT(m_sh2_state->r[n], 31);
	ans += dest;

	if (src == 1 && ans == 1)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  SWAP.B  Rm,Rn */
void sh_common_execution::SWAPB(uint32_t m, uint32_t n)
{
	uint32_t temp = m_sh2_state->r[m] & 0xffff0000;
	temp |= (m_sh2_state->r[m] & 0x000000ff) << 8;
	m_sh2_state->r[n] = (uint8_t)(m_sh2_state->r[m] >> 8);
	m_sh2_state->r[n] = m_sh2_state->r[n] | temp;
}

/*  SWAP.W  Rm,Rn */
void sh_common_execution::SWAPW(uint32_t m, uint32_t n)
{
	uint32_t temp = m_sh2_state->r[m] >> 16;
	m_sh2_state->r[n] = (m_sh2_state->r[m] << 16) | temp;
}

/*  TAS.B   @Rn */
void sh_common_execution::TAS(uint32_t n)
{
	m_sh2_state->ea = m_sh2_state->r[n];

	/* Bus Lock enable */
	uint32_t temp = read_byte(m_sh2_state->ea);
	if (temp == 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
	temp |= 0x80;
	/* Bus Lock disable */
	write_byte(m_sh2_state->ea, temp);
	m_sh2_state->icount -= 3;
}

/*  TST     Rm,Rn */
void sh_common_execution::TST(uint32_t m, uint32_t n)
{
	if ((m_sh2_state->r[n] & m_sh2_state->r[m]) == 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  TST     #imm,R0 */
void sh_common_execution::TSTI(uint32_t i)
{
	uint32_t imm = i & 0xff;

	if ((imm & m_sh2_state->r[0]) == 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
}

/*  TST.B   #imm,@(R0,GBR) */
void sh_common_execution::TSTM(uint32_t i)
{
	uint32_t imm = i & 0xff;

	m_sh2_state->ea = m_sh2_state->gbr + m_sh2_state->r[0];
	if ((imm & read_byte(m_sh2_state->ea)) == 0)
		m_sh2_state->sr |= SH_T;
	else
		m_sh2_state->sr &= ~SH_T;
	m_sh2_state->icount -= 2;
}

/*  XOR     Rm,Rn */
void sh_common_execution::XOR(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] ^= m_sh2_state->r[m];
}

/*  XOR     #imm,R0 */
void sh_common_execution::XORI(uint32_t i)
{
	m_sh2_state->r[0] ^= i & 0x000000ff;
}

/*  XOR.B   #imm,@(R0,GBR) */
void sh_common_execution::XORM(uint32_t i)
{
	m_sh2_state->ea = m_sh2_state->gbr + m_sh2_state->r[0];
	write_byte(m_sh2_state->ea, read_byte(m_sh2_state->ea) ^ (uint8_t)i);
	m_sh2_state->icount -= 2;
}

/*  XTRCT   Rm,Rn */
void sh_common_execution::XTRCT(uint32_t m, uint32_t n)
{
	m_sh2_state->r[n] = (m_sh2_state->r[n] >> 16) | (m_sh2_state->r[m] << 16);
}

/*  SLEEP */
void sh_common_execution::SLEEP()
{
	/* 0 = normal mode */
	/* 1 = enters into power-down mode */
	/* 2 = go out the power-down mode after an exception */
	if (m_sh2_state->sleep_mode != 2)
		m_sh2_state->pc -= 2;
	m_sh2_state->icount -= 2;
	/* Wait_for_exception; */
	if (m_sh2_state->sleep_mode == 0)
		m_sh2_state->sleep_mode = 1;
	else if (m_sh2_state->sleep_mode == 2)
		m_sh2_state->sleep_mode = 0;
}

/* Common dispatch */

void sh_common_execution::op0010(uint16_t opcode)
{
	switch (opcode & 15)
	{
	case  0: MOVBS(REG_M, REG_N);   break;
	case  1: MOVWS(REG_M, REG_N);   break;
	case  2: MOVLS(REG_M, REG_N);   break;
	case  3: ILLEGAL();             break;
	case  4: MOVBM(REG_M, REG_N);   break;
	case  5: MOVWM(REG_M, REG_N);   break;
	case  6: MOVLM(REG_M, REG_N);   break;
	case  7: DIV0S(REG_M, REG_N);   break;
	case  8: TST(REG_M, REG_N);     break;
	case  9: AND(REG_M, REG_N);     break;
	case 10: XOR(REG_M, REG_N);     break;
	case 11: OR(REG_M, REG_N);      break;
	case 12: CMPSTR(REG_M, REG_N);  break;
	case 13: XTRCT(REG_M, REG_N);   break;
	case 14: MULU(REG_M, REG_N);    break;
	case 15: MULS(REG_M, REG_N);    break;
	}
}

void sh_common_execution::op0011(uint16_t opcode)
{
	switch (opcode & 15)
	{
	case  0: CMPEQ(REG_M, REG_N);   break;
	case  1: ILLEGAL();             break;
	case  2: CMPHS(REG_M, REG_N);   break;
	case  3: CMPGE(REG_M, REG_N);   break;
	case  4: DIV1(REG_M, REG_N);    break;
	case  5: DMULU(REG_M, REG_N);   break;
	case  6: CMPHI(REG_M, REG_N);   break;
	case  7: CMPGT(REG_M, REG_N);   break;
	case  8: SUB(REG_M, REG_N);     break;
	case  9: ILLEGAL();             break;
	case 10: SUBC(REG_M, REG_N);    break;
	case 11: SUBV(REG_M, REG_N);    break;
	case 12: ADD(REG_M, REG_N);     break;
	case 13: DMULS(REG_M, REG_N);   break;
	case 14: ADDC(REG_M, REG_N);    break;
	case 15: ADDV(REG_M, REG_N);    break;
	}
}

void sh_common_execution::op0110(uint16_t opcode)
{
	switch (opcode & 15)
	{
	case  0: MOVBL(REG_M, REG_N);   break;
	case  1: MOVWL(REG_M, REG_N);   break;
	case  2: MOVLL(REG_M, REG_N);   break;
	case  3: MOV(REG_M, REG_N);     break;
	case  4: MOVBP(REG_M, REG_N);   break;
	case  5: MOVWP(REG_M, REG_N);   break;
	case  6: MOVLP(REG_M, REG_N);   break;
	case  7: NOT(REG_M, REG_N);     break;
	case  8: SWAPB(REG_M, REG_N);   break;
	case  9: SWAPW(REG_M, REG_N);   break;
	case 10: NEGC(REG_M, REG_N);    break;
	case 11: NEG(REG_M, REG_N);     break;
	case 12: EXTUB(REG_M, REG_N);   break;
	case 13: EXTUW(REG_M, REG_N);   break;
	case 14: EXTSB(REG_M, REG_N);   break;
	case 15: EXTSW(REG_M, REG_N);   break;
	}
}

void sh_common_execution::op1000(uint16_t opcode)
{
	switch ((opcode >> 8) & 15)
	{
	case  0: MOVBS4(opcode & 0x0f, REG_M);  break;
	case  1: MOVWS4(opcode & 0x0f, REG_M);  break;
	case  2: ILLEGAL();                     break;
	case  3: ILLEGAL();                     break;
	case  4: MOVBL4(REG_M, opcode & 0x0f);  break;
	case  5: MOVWL4(REG_M, opcode & 0x0f);  break;
	case  6: ILLEGAL();                     break;
	case  7: ILLEGAL();                     break;
	case  8: CMPIM(opcode & 0xff);          break;
	case  9: BT(opcode & 0xff);             break;
	case 10: ILLEGAL();                     break;
	case 11: BF(opcode & 0xff);             break;
	case 12: ILLEGAL();                     break;
	case 13: BTS(opcode & 0xff);            break;
	case 14: ILLEGAL();                     break;
	case 15: BFS(opcode & 0xff);            break;
	}
}


void sh_common_execution::op1100(uint16_t opcode)
{
	switch ((opcode >> 8) & 15)
	{
	case  0: MOVBSG(opcode & 0xff);     break;
	case  1: MOVWSG(opcode & 0xff);     break;
	case  2: MOVLSG(opcode & 0xff);     break;
	case  3: TRAPA(opcode & 0xff);      break; // sh2/4 differ
	case  4: MOVBLG(opcode & 0xff);     break;
	case  5: MOVWLG(opcode & 0xff);     break;
	case  6: MOVLLG(opcode & 0xff);     break;
	case  7: MOVA(opcode & 0xff);       break;
	case  8: TSTI(opcode & 0xff);       break;
	case  9: ANDI(opcode & 0xff);       break;
	case 10: XORI(opcode & 0xff);       break;
	case 11: ORI(opcode & 0xff);        break;
	case 12: TSTM(opcode & 0xff);       break;
	case 13: ANDM(opcode & 0xff);       break;
	case 14: XORM(opcode & 0xff);       break;
	case 15: ORM(opcode & 0xff);        break;
	}
}

// SH4 cases fall through to here too
void sh_common_execution::execute_one_0000(uint16_t opcode)
{
	// 04,05,06,07 always the same, 0c,0d,0e,0f always the same, other change based on upper bits

	switch (opcode & 0x3f)
	{
	case 0x00: ILLEGAL();               break;
	case 0x01: ILLEGAL();               break;
	case 0x02: STCSR(REG_N);            break;
	case 0x03: BSRF(REG_N);             break;
	case 0x04: MOVBS0(REG_M, REG_N);    break;
	case 0x05: MOVWS0(REG_M, REG_N);    break;
	case 0x06: MOVLS0(REG_M, REG_N);    break;
	case 0x07: MULL(REG_M, REG_N);      break;
	case 0x08: CLRT();                  break;
	case 0x09: NOP();                   break;
	case 0x0a: STSMACH(REG_N);          break;
	case 0x0b: RTS();                   break;
	case 0x0c: MOVBL0(REG_M, REG_N);    break;
	case 0x0d: MOVWL0(REG_M, REG_N);    break;
	case 0x0e: MOVLL0(REG_M, REG_N);    break;
	case 0x0f: MAC_L(REG_M, REG_N);     break;

	case 0x10: ILLEGAL();               break;
	case 0x11: ILLEGAL();               break;
	case 0x12: STCGBR(REG_N);           break;
	case 0x13: ILLEGAL();               break;
	case 0x14: MOVBS0(REG_M, REG_N);    break;
	case 0x15: MOVWS0(REG_M, REG_N);    break;
	case 0x16: MOVLS0(REG_M, REG_N);    break;
	case 0x17: MULL(REG_M, REG_N);      break;
	case 0x18: SETT();                  break;
	case 0x19: DIV0U();                 break;
	case 0x1a: STSMACL(REG_N);          break;
	case 0x1b: SLEEP();                 break;
	case 0x1c: MOVBL0(REG_M, REG_N);    break;
	case 0x1d: MOVWL0(REG_M, REG_N);    break;
	case 0x1e: MOVLL0(REG_M, REG_N);    break;
	case 0x1f: MAC_L(REG_M, REG_N);     break;

	case 0x20: ILLEGAL();               break;
	case 0x21: ILLEGAL();               break;
	case 0x22: STCVBR(REG_N);           break;
	case 0x23: BRAF(REG_N);             break;
	case 0x24: MOVBS0(REG_M, REG_N);    break;
	case 0x25: MOVWS0(REG_M, REG_N);    break;
	case 0x26: MOVLS0(REG_M, REG_N);    break;
	case 0x27: MULL(REG_M, REG_N);      break;
	case 0x28: CLRMAC();                break;
	case 0x29: MOVT(REG_N);             break;
	case 0x2a: STSPR(REG_N);            break;
	case 0x2b: RTE();                   break;
	case 0x2c: MOVBL0(REG_M, REG_N);    break;
	case 0x2d: MOVWL0(REG_M, REG_N);    break;
	case 0x2e: MOVLL0(REG_M, REG_N);    break;
	case 0x2f: MAC_L(REG_M, REG_N);     break;

	case 0x30: ILLEGAL();               break;
	case 0x31: ILLEGAL();               break;
	case 0x32: ILLEGAL();               break;
	case 0x33: ILLEGAL();               break;
	case 0x34: MOVBS0(REG_M, REG_N);    break;
	case 0x35: MOVWS0(REG_M, REG_N);    break;
	case 0x36: MOVLS0(REG_M, REG_N);    break;
	case 0x37: MULL(REG_M, REG_N);      break;
	case 0x38: ILLEGAL();               break;
	case 0x39: ILLEGAL();               break;
	case 0x3a: ILLEGAL();               break;
	case 0x3b: ILLEGAL();               break;
	case 0x3c: MOVBL0(REG_M, REG_N);    break;
	case 0x3d: MOVWL0(REG_M, REG_N);    break;
	case 0x3e: MOVLL0(REG_M, REG_N);    break;
	case 0x3f: MAC_L(REG_M, REG_N);     break;
	}
}

// SH4 cases fall through to here too
void sh_common_execution::execute_one_4000(uint16_t opcode)
{
	// 0f always the same, others differ

	switch (opcode & 0x3f)
	{
	case 0x00: SHLL(REG_N);         break;
	case 0x01: SHLR(REG_N);         break;
	case 0x02: STSMMACH(REG_N);     break;
	case 0x03: STCMSR(REG_N);       break;
	case 0x04: ROTL(REG_N);         break;
	case 0x05: ROTR(REG_N);         break;
	case 0x06: LDSMMACH(REG_N);     break;
	case 0x07: LDCMSR(opcode);      break;
	case 0x08: SHLL2(REG_N);        break;
	case 0x09: SHLR2(REG_N);        break;
	case 0x0a: LDSMACH(REG_N);      break;
	case 0x0b: JSR(REG_N);          break;
	case 0x0c: ILLEGAL();           break;
	case 0x0d: ILLEGAL();           break;
	case 0x0e: LDCSR(opcode);       break;
	case 0x0f: MAC_W(REG_M, REG_N); break;

	case 0x10: DT(REG_N);           break;
	case 0x11: CMPPZ(REG_N);        break;
	case 0x12: STSMMACL(REG_N);     break;
	case 0x13: STCMGBR(REG_N);      break;
	case 0x14: ILLEGAL();           break;
	case 0x15: CMPPL(REG_N);        break;
	case 0x16: LDSMMACL(REG_N);     break;
	case 0x17: LDCMGBR(REG_N);      break;
	case 0x18: SHLL8(REG_N);        break;
	case 0x19: SHLR8(REG_N);        break;
	case 0x1a: LDSMACL(REG_N);      break;
	case 0x1b: TAS(REG_N);          break;
	case 0x1c: ILLEGAL();           break;
	case 0x1d: ILLEGAL();           break;
	case 0x1e: LDCGBR(REG_N);       break;
	case 0x1f: MAC_W(REG_M, REG_N); break;

	case 0x20: SHAL(REG_N);         break;
	case 0x21: SHAR(REG_N);         break;
	case 0x22: STSMPR(REG_N);       break;
	case 0x23: STCMVBR(REG_N);      break;
	case 0x24: ROTCL(REG_N);        break;
	case 0x25: ROTCR(REG_N);        break;
	case 0x26: LDSMPR(REG_N);       break;
	case 0x27: LDCMVBR(REG_N);      break;
	case 0x28: SHLL16(REG_N);       break;
	case 0x29: SHLR16(REG_N);       break;
	case 0x2a: LDSPR(REG_N);        break;
	case 0x2b: JMP(REG_N);          break;
	case 0x2c: ILLEGAL();           break;
	case 0x2d: ILLEGAL();           break;
	case 0x2e: LDCVBR(REG_N);       break;
	case 0x2f: MAC_W(REG_M, REG_N); break;

	case 0x30: ILLEGAL();           break;
	case 0x31: ILLEGAL();           break;
	case 0x32: ILLEGAL();           break;
	case 0x33: ILLEGAL();           break;
	case 0x34: ILLEGAL();           break;
	case 0x35: ILLEGAL();           break;
	case 0x36: ILLEGAL();           break;
	case 0x37: ILLEGAL();           break;
	case 0x38: ILLEGAL();           break;
	case 0x39: ILLEGAL();           break;
	case 0x3a: ILLEGAL();           break;
	case 0x3b: ILLEGAL();           break;
	case 0x3c: ILLEGAL();           break;
	case 0x3d: ILLEGAL();           break;
	case 0x3e: ILLEGAL();           break;
	case 0x3f: MAC_W(REG_M, REG_N); break;

	}
}

void sh_common_execution::execute_one(const uint16_t opcode)
{
	switch ((opcode >> 12) & 15)
	{
		case  0: execute_one_0000(opcode);              break;
		case  1: MOVLS4(REG_M, opcode & 0xf, REG_N);    break;
		case  2: op0010(opcode);                        break;
		case  3: op0011(opcode);                        break;
		case  4: execute_one_4000(opcode);              break;
		case  5: MOVLL4(REG_M, opcode & 0x0f, REG_N);   break;
		case  6: op0110(opcode);                        break;
		case  7: ADDI(opcode & 0xff, REG_N);            break;
		case  8: op1000(opcode);                        break;
		case  9: MOVWI(opcode & 0xff, REG_N);           break;
		case 10: BRA(opcode & 0xfff);                   break;
		case 11: BSR(opcode & 0xfff);                   break;
		case 12: op1100(opcode);                        break;
		case 13: MOVLI(opcode & 0xff, REG_N);           break;
		case 14: MOVI(opcode & 0xff, REG_N);            break;
		case 15: execute_one_f000(opcode);              break;
	}
}

// DRC / UML related
// S-MU2000: ここから末尾までの 2252 行を削除した。
// すべて DRC（動的再コンパイラ）の生成コードと、そこからしか呼ばれない
// cfunc_*/func_* ヘルパ。インタプリタ経路 execute_one() は上に残っている。

void sh_common_execution::state(state_io &s)
{
	s.tag("shcore");
	s.v(*m_sh2_state);          // まるごと。POD なのでそのまま写せる
	s.v(m_pcfsel);
	// **これが要る**。周辺（タイマ、SCI、ADC）はこの数を今の時刻として
	// 見ているので、写し忘れると戻した瞬間から予定が全部ずれる
	s.v(m_total_cycles);
	s.v(m_cycles_this_run);
}
