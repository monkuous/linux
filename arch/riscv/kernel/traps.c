// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/kdebug.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/uprobes.h>
#include <asm/uprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/kexec.h>
#include <linux/entry-common.h>

#include <asm/asm-prototypes.h>
#include <asm/bug.h>
#include <asm/csr.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/syscall.h>
#include <asm/thread_info.h>
#include <asm/irq_stack.h>

int show_unhandled_signals = 1;

static DEFINE_RAW_SPINLOCK(die_lock);

static int copy_code(struct pt_regs *regs, u32 *val, const u32 *insns)
{
	const void __user *uaddr = (__force const void __user *)insns;

	if (!user_mode(regs))
		return get_kernel_nofault(*val, insns);

	/* The user space code from other tasks cannot be accessed. */
	if (regs != task_pt_regs(current))
		return -EPERM;

	return copy_from_user_nofault(val, uaddr, sizeof(*val));
}

static void dump_instr(const char *loglvl, struct pt_regs *regs)
{
	char str[sizeof("00000000 ") * 6 + 2 + 1], *p = str;
	const u32 *insns = (u32 *)instruction_pointer(regs);
	long bad;
	u32 val;
	int i;

	for (i = -5; i < 1; i++) {
		bad = copy_code(regs, &val, &insns[i]);
		if (!bad) {
			p += sprintf(p, i == 0 ? "(%08x) " : "%08x ", val);
		} else {
			printk("%sCode: Unable to access instruction at 0x%px.\n",
			       loglvl, &insns[i]);
			return;
		}
	}
	printk("%sCode: %s\n", loglvl, str);
}

void die(struct pt_regs *regs, const char *str)
{
	static int die_counter;
	int ret;
	long cause;
	unsigned long flags;

	oops_enter();

	raw_spin_lock_irqsave(&die_lock, flags);
	console_verbose();
	bust_spinlocks(1);

	pr_emerg("%s [#%d]\n", str, ++die_counter);
	print_modules();
	if (regs) {
		show_regs(regs);
		dump_instr(KERN_EMERG, regs);
	}

	cause = regs ? regs->cause : -1;
	ret = notify_die(DIE_OOPS, str, regs, 0, cause, SIGSEGV);

	if (kexec_should_crash(current))
		crash_kexec(regs);

	bust_spinlocks(0);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	raw_spin_unlock_irqrestore(&die_lock, flags);
	oops_exit();

	if (in_interrupt())
		panic("Fatal exception in interrupt");
	if (panic_on_oops)
		panic("Fatal exception");
	if (ret != NOTIFY_STOP)
		make_task_dead(SIGSEGV);
}

void do_trap(struct pt_regs *regs, int signo, int code, unsigned long addr)
{
	struct task_struct *tsk = current;

	if (show_unhandled_signals && unhandled_signal(tsk, signo)
	    && printk_ratelimit()) {
		pr_info("%s[%d]: unhandled signal %d code 0x%x at 0x" REG_FMT,
			tsk->comm, task_pid_nr(tsk), signo, code, addr);
		print_vma_addr(KERN_CONT " in ", instruction_pointer(regs));
		pr_cont("\n");
		__show_regs(regs);
		dump_instr(KERN_INFO, regs);
	}

	force_sig_fault(signo, code, (void __user *)addr);
}

static void do_trap_error(struct pt_regs *regs, int signo, int code,
	unsigned long addr, const char *str)
{
	current->thread.bad_cause = regs->cause;

	if (user_mode(regs)) {
		do_trap(regs, signo, code, addr);
	} else {
		if (!fixup_exception(regs))
			die(regs, str);
	}
}

#define __trap_section noinstr
#define DO_ERROR_INFO(name, signo, code, str)					\
asmlinkage __visible __trap_section void name(struct pt_regs *regs)		\
{										\
	if (user_mode(regs)) {							\
		irqentry_enter_from_user_mode(regs);				\
		local_irq_enable();						\
		do_trap_error(regs, signo, code, regs->epc, "Oops - " str);	\
		local_irq_disable();						\
		irqentry_exit_to_user_mode(regs);				\
	} else {								\
		irqentry_state_t state = irqentry_nmi_enter(regs);		\
		do_trap_error(regs, signo, code, regs->epc, "Oops - " str);	\
		irqentry_nmi_exit(regs, state);					\
	}									\
}

DO_ERROR_INFO(do_trap_unknown,
	SIGILL, ILL_ILLTRP, "unknown exception");
DO_ERROR_INFO(do_trap_bus_error,
	SIGSEGV, SEGV_ACCERR, "bus error");

asmlinkage __visible __trap_section void do_trap_insn_illegal(struct pt_regs *regs)
{
	if (user_mode(regs)) {
		irqentry_enter_from_user_mode(regs);
		local_irq_enable();

		do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->epc,
			      "Oops - illegal instruction");

		local_irq_disable();
		irqentry_exit_to_user_mode(regs);
	} else {
		irqentry_state_t state = irqentry_nmi_enter(regs);

		do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->epc,
			      "Oops - illegal instruction");

		irqentry_nmi_exit(regs, state);
	}
}

void do_trap_unaligned(struct pt_regs *regs)
{
	irqentry_state_t state;

	if (user_mode(regs)) {
		irqentry_enter_from_user_mode(regs);
		local_irq_enable();
	} else {
		state = irqentry_nmi_enter(regs);
	}

	if (handle_unaligned(regs))
		do_trap_error(regs, SIGBUS, BUS_ADRALN, regs->epc,
			      "Oops - unaligned access");

	if (user_mode(regs)) {
		local_irq_disable();
		irqentry_exit_to_user_mode(regs);
	} else {
		irqentry_nmi_exit(regs, state);
	}
}

static bool probe_single_step_handler(struct pt_regs *regs)
{
	bool user = user_mode(regs);

	return user ? uprobe_single_step_handler(regs) : kprobe_single_step_handler(regs);
}
NOKPROBE_SYMBOL(probe_single_step_handler);

static bool probe_breakpoint_handler(struct pt_regs *regs)
{
	bool user = user_mode(regs);

	return user ? uprobe_breakpoint_handler(regs) : kprobe_breakpoint_handler(regs);
}
NOKPROBE_SYMBOL(probe_breakpoint_handler);

void handle_break(struct pt_regs *regs)
{
	if (probe_single_step_handler(regs))
		return;

	if (probe_breakpoint_handler(regs))
		return;

	current->thread.bad_cause = regs->cause;

	if (user_mode(regs))
		force_sig_fault(SIGTRAP, TRAP_BRKPT, (void __user *)regs->epc);
#ifdef CONFIG_KGDB
	else if (notify_die(DIE_TRAP, "EBREAK", regs, 0, regs->cause, SIGTRAP)
								== NOTIFY_STOP)
		return;
#endif
	else if (report_bug(regs->epc, regs) == BUG_TRAP_TYPE_WARN)
		regs->epc += INSN_LENGTH;
	else
		die(regs, "Kernel BUG");
}
NOKPROBE_SYMBOL(handle_break);

asmlinkage __visible __trap_section void do_trap_break(struct pt_regs *regs)
{
	if (user_mode(regs)) {
		irqentry_enter_from_user_mode(regs);
		local_irq_enable();

		handle_break(regs);

		local_irq_disable();
		irqentry_exit_to_user_mode(regs);
	} else {
		irqentry_state_t state = irqentry_nmi_enter(regs);

		handle_break(regs);

		irqentry_nmi_exit(regs, state);
	}
}

asmlinkage __visible __trap_section  __no_stack_protector
void do_trap_syscall(struct pt_regs *regs)
{
	if (user_mode(regs)) {
		long syscall = regs->a7;

		regs->epc += 4;
		regs->orig_a0 = regs->a0;
		regs->a0 = -ENOSYS;

		if (likely(syscall_enter_from_user_mode_randomize_stack(regs, &syscall))) {
			if (syscall >= 0 && syscall < NR_syscalls) {
				syscall = array_index_nospec(syscall, NR_syscalls);
				syscall_handler(regs, syscall);
			}
		}
		syscall_exit_to_user_mode(regs);
	} else {
		irqentry_state_t state = irqentry_nmi_enter(regs);

		do_trap_error(regs, SIGILL, ILL_ILLTRP, regs->epc,
			"Oops - syscall");

		irqentry_nmi_exit(regs, state);
	}

}
asmlinkage __visible noinstr void do_page_fault(struct pt_regs *regs)
{
	irqentry_state_t state = irqentry_enter(regs);

	handle_page_fault(regs);

	local_irq_disable();

	irqentry_exit(regs, state);
}

static void noinstr handle_riscv_irq(struct pt_regs *regs)
{
	struct pt_regs *old_regs;

	irq_enter_rcu();
	old_regs = set_irq_regs(regs);
	handle_arch_irq(regs);
	set_irq_regs(old_regs);
	irq_exit_rcu();
}

asmlinkage void noinstr do_irq(struct pt_regs *regs)
{
	irqentry_state_t state = irqentry_enter(regs);

	if (IS_ENABLED(CONFIG_IRQ_STACKS) && on_thread_stack())
		call_on_irq_stack(regs, handle_riscv_irq);
	else
		handle_riscv_irq(regs);

	irqentry_exit(regs, state);
}

#ifdef CONFIG_GENERIC_BUG
int is_valid_bugaddr(unsigned long pc)
{
	bug_insn_t insn;

	if (pc < VMALLOC_START)
		return 0;
	if (get_kernel_nofault(insn, (bug_insn_t *)pc))
		return 0;
	return (insn == __BUG_INSN);
}
#endif /* CONFIG_GENERIC_BUG */
