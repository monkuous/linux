// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/irq.h>
#include <linux/stringify.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/csr.h>
#include <asm/entry-common.h>
#include <asm/hwprobe.h>
#include <asm/insn.h>

#define __read_insn(regs, insn, insn_addr, type)	\
({							\
	int __ret;					\
							\
	if (user_mode(regs)) {				\
		__ret = get_user(insn, (type __user *) insn_addr); \
	} else {					\
		insn = *(type *)insn_addr;		\
		__ret = 0;				\
	}						\
							\
	__ret;						\
})

static inline int get_insn(struct pt_regs *regs, ulong epc, ulong *r_insn)
{
	ulong insn = 0;

	if (__read_insn(regs, insn, epc, u32))
			return -EFAULT;

	*r_insn = insn;
	return 0;
}

union reg_data {
	u8 data_bytes[8];
	ulong data_ulong;
};

/* sysctl hooks */
int unaligned_enabled __read_mostly = 1;	/* Enabled by default */

int handle_unaligned(struct pt_regs *regs)
{
	union reg_data val;
	unsigned long epc = regs->epc;
	unsigned long insn;
	unsigned long addr = regs->badaddr;
	int shift = 0, len = 0, read = 1;

	perf_sw_event(PERF_COUNT_SW_ALIGNMENT_FAULTS, 1, regs, addr);

	if (!unaligned_enabled)
		return -1;

	if (user_mode(regs) && (current->thread.align_ctl & PR_UNALIGN_SIGBUS))
		return -1;

	if (get_insn(regs, epc, &insn))
		return -1;

	regs->epc = 0;

	if ((insn & INSN_MASK_LW) == INSN_MATCH_LW) {
		len = 4;
	} else if ((insn & INSN_MASK_LH) == INSN_MATCH_LH) {
		len = 2;
		shift = 8 * (sizeof(unsigned long) - len);
	} else if ((insn & INSN_MASK_LHU) == INSN_MATCH_LHU) {
		len = 2;
	} if ((insn & INSN_MASK_SW) == INSN_MATCH_SW) {
		len = 4;
		read = 0;
	} else if ((insn & INSN_MASK_SH) == INSN_MATCH_SH) {
		len = 2;
		read = 0;
	}else {
		regs->epc = epc;
		return -1;
	}

	if (read) {
		val.data_ulong = 0;

		if (user_mode(regs)) {
			if (copy_from_user(&val, (u8 __user *)addr, len))
				return -1;
		} else {
			memcpy(&val, (u8 *)addr, len);
		}

		SET_RD(insn, regs, (long)(val.data_ulong << shift) >> shift);
	} else {
		val.data_ulong = GET_RS2(insn, regs);

		if (user_mode(regs)) {
			if (copy_to_user((u8 __user *)addr, &val, len))
				return -1;
		} else {
			memcpy((u8 *)addr, &val, len);
		}
	}

	regs->epc = epc + INSN_LEN(insn);

	return 0;
}
