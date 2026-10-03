/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_ELF_H
#define _ASM_RISCV_ELF_H

#include <uapi/linux/elf.h>
#include <linux/compat.h>
#include <uapi/asm/elf.h>
#include <asm/auxvec.h>
#include <asm/byteorder.h>
#include <asm/cacheinfo.h>
#include <uapi/asm/hwcap.h>

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_ARCH	EM_RISCV

#ifndef ELF_CLASS
#define ELF_CLASS	ELFCLASS32
#endif

#define ELF_DATA	ELFDATA2LSB

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) (((x)->e_machine == ELF_ARCH) && \
			   ((x)->e_ident[EI_CLASS] == ELF_CLASS))

#define CORE_DUMP_USE_REGSET
#define ELF_FDPIC_CORE_EFLAGS	0
#define ELF_EXEC_PAGESIZE	(PAGE_SIZE)

/*
 * This is the location that an ET_DYN program is loaded if exec'ed.  Typical
 * use of this is to invoke "./ld.so someprog" to test out a new version of
 * the loader.  We need to make sure that it is out of the way of the program
 * that it will "exec", and that there is sufficient room for the brk.
 */
#define ELF_ET_DYN_BASE		((DEFAULT_MAP_WINDOW / 3) * 2)

/*
 * Provides information on the available set of ISA extensions to userspace,
 * via a bitmap that corresponds to each single-letter ISA extension.  This is
 * essentially defunct, but will remain for compatibility with userspace.
 */
#define ELF_HWCAP	(COMPAT_HWCAP_ISA_I | COMPAT_HWCAP_ISA_A | COMPAT_HWCAP_ISA_A)
extern unsigned long elf_hwcap;

#define ELF_FDPIC_PLAT_INIT(_r, _exec_map_addr, _interp_map_addr, dynamic_addr) \
	do { \
		(_r)->a1 = _exec_map_addr; \
		(_r)->a2 = _interp_map_addr; \
		(_r)->a3 = dynamic_addr; \
	} while (0)

/*
 * This yields a string that ld.so will use to load implementation
 * specific libraries for optimization.  This is more specific in
 * intent than poking at uname or /proc/cpuinfo.
 */
#define ELF_PLATFORM	(NULL)

#define ARCH_DLINFO						\
do {								\
	/*							\
	 * Note that we add ulong after elf_addr_t because	\
	 * casting current->mm->context.vdso triggers a cast	\
	 * warning of cast from pointer to integer for		\
	 * COMPAT ELFCLASS32.					\
	 */							\
	NEW_AUX_ENT(AT_SYSINFO_EHDR,				\
		(elf_addr_t)(ulong)current->mm->context.vdso);	\
	NEW_AUX_ENT(AT_L1I_CACHESIZE,				\
		get_cache_size(1, CACHE_TYPE_INST));		\
	NEW_AUX_ENT(AT_L1I_CACHEGEOMETRY,			\
		get_cache_geometry(1, CACHE_TYPE_INST));	\
	NEW_AUX_ENT(AT_L1D_CACHESIZE,				\
		get_cache_size(1, CACHE_TYPE_DATA));		\
	NEW_AUX_ENT(AT_L1D_CACHEGEOMETRY,			\
		get_cache_geometry(1, CACHE_TYPE_DATA));	\
	NEW_AUX_ENT(AT_L2_CACHESIZE,				\
		get_cache_size(2, CACHE_TYPE_UNIFIED));		\
	NEW_AUX_ENT(AT_L2_CACHEGEOMETRY,			\
		get_cache_geometry(2, CACHE_TYPE_UNIFIED));	\
	NEW_AUX_ENT(AT_L3_CACHESIZE,				\
		get_cache_size(3, CACHE_TYPE_UNIFIED));		\
	NEW_AUX_ENT(AT_L3_CACHEGEOMETRY,			\
		get_cache_geometry(3, CACHE_TYPE_UNIFIED));	\
	/*							 \
	 * Should always be nonzero unless there's a kernel bug. \
	 * If we haven't determined a sensible value to give to	 \
	 * userspace, omit the entry:				 \
	 */							 \
	if (likely(signal_minsigstksz))				 \
		NEW_AUX_ENT(AT_MINSIGSTKSZ, signal_minsigstksz); \
	else							 \
		NEW_AUX_ENT(AT_IGNORE, 0);			 \
} while (0)

#define ARCH_HAS_SETUP_ADDITIONAL_PAGES
struct linux_binprm;
extern int arch_setup_additional_pages(struct linux_binprm *bprm,
	int uses_interp);

#define ELF_CORE_COPY_REGS(dest, regs)			\
do {							\
	*(struct user_regs_struct *)&(dest) =		\
		*(struct user_regs_struct *)regs;	\
} while (0);

#endif /* _ASM_RISCV_ELF_H */
