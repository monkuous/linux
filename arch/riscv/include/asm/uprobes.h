/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_RISCV_UPROBES_H
#define _ASM_RISCV_UPROBES_H

#include <asm/probes.h>
#include <asm/text-patching.h>
#include <asm/bug.h>

#define MAX_UINSN_BYTES		8

#define UPROBE_SWBP_INSN	__BUG_INSN
#define UPROBE_SWBP_INSN_SIZE	INSN_LENGTH
#define UPROBE_XOL_SLOT_BYTES	MAX_UINSN_BYTES

typedef u32 uprobe_opcode_t;

struct arch_uprobe_task {
	unsigned long   saved_cause;
};

struct arch_uprobe {
	union {
		u8 insn[MAX_UINSN_BYTES];
		u8 ixol[MAX_UINSN_BYTES];
	};
	struct arch_probe_insn api;
	bool simulate;
};

#ifdef CONFIG_UPROBES
bool uprobe_breakpoint_handler(struct pt_regs *regs);
bool uprobe_single_step_handler(struct pt_regs *regs);
#else
static inline bool uprobe_breakpoint_handler(struct pt_regs *regs)
{
	return false;
}

static inline bool uprobe_single_step_handler(struct pt_regs *regs)
{
	return false;
}
#endif /* CONFIG_UPROBES */
#endif /* _ASM_RISCV_UPROBES_H */
