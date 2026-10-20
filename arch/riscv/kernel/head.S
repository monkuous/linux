/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm/asm-offsets.h>
#include <asm/asm.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/thread_info.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/csr.h>
#include <asm/image.h>

__HEAD
SYM_CODE_START(_start)
	/*
	 * Image header expected by Linux boot-loaders. The image header data
	 * structure is described in asm/image.h.
	 * Do not modify it without modifying the structure and all bootloaders
	 * that expects this header format!!
	 */
	/* jump to start kernel */
	j _start_kernel
	/* reserved */
	.word 0
	.balign 8
	/* Image load offset from start of RAM */
	.dword 0
	/* Effective size of kernel image */
	.dword _end - _start
	.dword __HEAD_FLAGS
	.word RISCV_HEADER_VERSION
	.word 0
	.dword 0
	.ascii RISCV_IMAGE_MAGIC
	.balign 4
	.ascii RISCV_IMAGE_MAGIC2
	.word 0

.align 2
	.global relocate_enable_mmu
relocate_enable_mmu:
	/* Relocate return address */
	li a1, KERNEL_LINK_ADDR
	la a2, _start
	sub a1, a1, a2
	add ra, ra, a1

	/* Flush TBs */
	addi t0, zero, TBCTRL_FLUSH_ALL
	csrw CSR_ITBCTRL, t0
	csrw CSR_DTBCTRL, t0

	/* Set up exception block */
1:	auipc t0, %pcrel_hi(exception_block)
	addi t0, t0, %pcrel_lo(1b)
	add t1, t0, a1
	csrw CSR_EB, t1

	/* Set up wired ITB entry for exception block */
	srli t0, t0, PAGE_SHIFT - 5
	srli t1, t1, PAGE_SHIFT
	ori t0, t0, 0x15
	csrw CSR_ITBINDEX, zero
	csrw CSR_ITBTAG, t1
	csrw CSR_ITBPTE, t0

	/* Set up wired ITB entry for head page */
1:	auipc t0, %pcrel_hi(_start)
	addi t0, t0, %pcrel_lo(1b)
	add t1, t0, a1
	srli t0, t0, PAGE_SHIFT - 5
	srli t1, t1, PAGE_SHIFT
	ori t0, t0, 0x15
	csrw CSR_ITBTAG, t1
	csrw CSR_ITBPTE, t0

	/* Set up wired DTB entry for page directory */
	li t0, PGDIR_START >> 12
	srli t2, a0, PAGE_SHIFT
	slli t1, t2, 5
	ori t1, t1, 5
	csrw CSR_DTBINDEX, zero
	csrw CSR_DTBTAG, t0
	csrw CSR_DTBPTE, t1
	csrw CSR_SCRATCH2, t2

	/* Enable paging */
	addi t0, zero, TBINDEX_NUM_WIRED
	li t1, PGTABLE_START
1:	auipc t2, %pcrel_hi(1f)
	addi t2, t2, %pcrel_lo(1b)
	add t2, t2, a1
	addi t3, zero, RS_PM
	csrw CSR_ITBINDEX, t0
	csrw CSR_DTBINDEX, t0
	csrw CSR_ITBADDR, t1
	csrw CSR_DTBADDR, t1
	csrw CSR_EPC, t2
	csrw CSR_RS, t3
	sret
1:
	/* Reload the global pointer */
	load_global_pointer

	ret
SYM_CODE_END(_start)

SYM_CODE_START(_start_kernel)
	/* Reset RS. Get zero using xor so that it still works if RS.T=1 */
	xor t0, t0, t0
	csrw CSR_RS, t0

	/* Load the global pointer */
	load_global_pointer

	la s1, hart_lottery

	/* Verify that our hartid is valid (i.e. not equal to INVALID_HARTID) */
	not t0, a0
	bnez t0, .Lhartid_ok
	/*
	 * It's invalid, but we still have to increment hart_lottery, otherwise
	 * the boot CPU will be forever waiting for the CPU to reach the kernel.
	 * Take care not to be the first to increment it.
	 */
1:	lr.w t0, (s1)
	beqz t0, 1b
	addi t1, t0, 1
	sc.w t1, t1, (s1)
	bnez t1, 1b
	j .Lsecondary_park
.Lhartid_ok:

	/* Pick one hart to run the main boot sequence */
1:	lr.w t0, (s1)
	addi t1, t0, 1
	sc.w t1, t1, (s1)
	bnez t1, 1b
#ifdef CONFIG_SMP
	bnez t0, .Lsecondary_start
	fence rw, rw
#else /* CONFIG_SMP */
	bnez t0, .Lsecondary_park
#endif /* CONFIG_SMP */

	/* Save the total number of CPUs that the bootloader gave us */
	mv s1, a2

	/* Clear BSS for flat non-ELF images */
	la a3, __bss_start
	la a4, __bss_stop
	ble a4, a3, .Lclear_bss_done
.Lclear_bss:
	REG_S zero, (a3)
	add a3, a3, RISCV_SZPTR
	blt a3, a4, .Lclear_bss
.Lclear_bss_done:

	la a2, boot_cpu_hartid
	REG_S a0, (a2)

	/* Initialize page tables and relocate to virtual addresses */
	la tp, init_task
	la sp, init_thread_union + THREAD_SIZE
	addi sp, sp, -PT_SIZE_ON_STACK
	mv a0, a1
	call setup_vm

	la a0, early_pg_dir
	call relocate_enable_mmu

	/* Restore C environment */
	la tp, init_task
	la sp, init_thread_union + THREAD_SIZE
	addi sp, sp, -PT_SIZE_ON_STACK

	/*
	 * Up until now the only memory we've touched is our static image and
	 * the FDT. Once start_kernel is called though, all bets are off, and
	 * any memory in the system might get touched. That isn't safe until
	 * all CPUs have entered the kernel and started spinwaiting: before
	 * that point a CPU might still be in a bootloader or similar.
	 */
	la t0, hart_lottery
1:	lw t1, (t0)
	bne t1, s1, 1b
	fence r, rw

	/* Start the kernel */
	tail start_kernel

#ifdef CONFIG_SMP
.Lsecondary_start:
	la a1, __cpu_spinwait_stack_pointer
	la a2, __cpu_spinwait_task_pointer
	la a3, __cpu_spinwait_current_hart

	/*
	 * This hart didn't win the lottery, so we wait for the winning hart to
	 * get far enough along the boot process that it should continue.
	 */
.Lwait_for_cpu_up:
	REG_L t0, (a3)
	beq t0, a0, .Laccept_boot_info
.option push
.option arch, +zihintpause
	pause
.option pop
	j .Lwait_for_cpu_up
.Laccept_boot_info:
	fence rw, rw

	/*
	 * It's our turn to boot. Get the boot info as quickly as possible.
	 */

	REG_L sp, (a1)
	REG_L tp, (a2)

	/*
	 * Tell the boot cpu that we've accepted our boot info. Use an xchg
	 * and verify the result to make sure the boot cpu didn't decide
	 * to time us out (which would make our boot info invalid).
	 */

	not t0, zero
	fence rw, rw
1:	lr.w t1, (a3)
	bne t1, a0, .Lsecondary_park
	sc.w t1, t0, (a3)
	bnez t1, 1b
	fence rw, rw

	/* Enable virtual memory and relocate to virtual address */
	la a0, swapper_pg_dir
	call relocate_enable_mmu

	call smp_callin
#endif /* CONFIG_SMP */

.align 2
.Lsecondary_park:
	/*
	 * Park this hart if we fail in smp_callin(),
	 * as a successful one wouldn't return.
	 */
	wfi
	j .Lsecondary_park
SYM_CODE_END(_start_kernel)

SYM_FUNC_START(xr17032_switch_mm)
	/* Disable interrupts */
	csrr t0, CSR_RS
	addi t1, zero, -(RS_I + 1)
	and t1, t0, t1
	csrw CSR_RS, t1

	/* Compute tbtag value */
	li t1, PGDIR_START >> 12
	slli t2, a1, TBTAG_ASID_SHIFT
	or t1, t1, t2

	/* Compute tbpte value */
	slli t2, a0, 5
	ori t2, t2, 5

	/* Change wired DTB entry */
	csrr t3, CSR_DTBINDEX
	csrw CSR_DTBINDEX, zero
	csrw CSR_DTBTAG, t1
	csrw CSR_DTBPTE, t2
	csrw CSR_DTBINDEX, t3

	/* Change ITB ASID */
	csrw CSR_ITBTAG, t1

	/* Set root PT PPN */
	csrw CSR_SCRATCH2, a0

	/* Restore interrupts and return */
	csrw CSR_RS, t0
	jalr zero, 0(ra)
SYM_FUNC_END(xr17032_switch_mm)
