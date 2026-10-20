/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2006  Ralf Baechle (ralf@linux-mips.org)
 * Copyright (c) 2018  Jim Wilson (jimw@sifive.com)
 */

#ifndef _ASM_RISCV_FUTEX_H
#define _ASM_RISCV_FUTEX_H

#include <linux/futex.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <asm/asm.h>
#include <asm/asm-extable.h>
#include <asm/fence.h>

/* We don't even really need the extable code, but for now keep it simple */

#define __futex_atomic_op(insn, ret, tmp, oldval, uaddr, oparg)	\
{								\
	__asm__ __volatile__ (					\
	RISCV_RELEASE_BARRIER					\
	"1:	lr.w %[ov], %[u]			\n"	\
	"	" insn " %[tmp], %[ov], %z[op]		\n"	\
	"2:	sc.w %[tmp], %[tmp], %[u]		\n"	\
	"	bnez %[tmp], 1b				\n"	\
	"3:						\n"	\
	RISCV_FULL_BARRIER					\
	_ASM_EXTABLE_UACCESS_ERR(1b, 3b, %[r])			\
	_ASM_EXTABLE_UACCESS_ERR(2b, 3b, %[r])			\
	: [r] "+r" (ret), [ov] "=&r" (oldval),			\
	  [tmp] "=&r" (tmp), [u] "+A" (*uaddr)		\
	: [op] "Jr" (oparg)					\
	: "memory");						\
}

static inline int
arch_futex_atomic_op_inuser(int op, int oparg, int *oval, u32 __user *uaddr)
{
	int oldval = 0, ret = 0;
	uintptr_t tmp;

	if (!access_ok(uaddr, sizeof(u32)))
		return -EFAULT;

	switch (op) {
	case FUTEX_OP_SET:
		__asm__ __volatile__ (
		RISCV_RELEASE_BARRIER
		"1:	lr.w %[ov], %[u]		\n"
		"2:	sc.w %[tmp], %z[op], %[u]	\n"
		"	bnez %[tmp], 1b			\n"
		"3:					\n"
		RISCV_FULL_BARRIER
		_ASM_EXTABLE_UACCESS_ERR(1b, 3b, %[r])
		_ASM_EXTABLE_UACCESS_ERR(2b, 3b, %[r])
		: [r] "+r" (ret), [ov] "=&r" (oldval),
		  [tmp] "=&r" (tmp), [u] "+A" (*uaddr)
		: [op] "Jr" (oparg)
		: "memory");
		break;
	case FUTEX_OP_ADD:
		__futex_atomic_op("add", ret, tmp, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_OR:
		__futex_atomic_op("or", ret, tmp, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ANDN:
		__futex_atomic_op("and", ret, tmp, oldval, uaddr, ~oparg);
		break;
	case FUTEX_OP_XOR:
		__futex_atomic_op("xor", ret, tmp, oldval, uaddr, oparg);
		break;
	default:
		ret = -ENOSYS;
	}

	if (!ret)
		*oval = oldval;

	return ret;
}

static inline int
futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
			      u32 oldval, u32 newval)
{
	int ret = 0;
	u32 val;
	uintptr_t tmp;

	if (!access_ok(uaddr, sizeof(u32)))
		return -EFAULT;

	__asm__ __volatile__ (
	RISCV_RELEASE_BARRIER
	"1:	lr.w %[v],%[u]			        \n"
	"	sub  %[t],%[v],%z[ov]			\n"
	"	bnez %[t],3f				\n"
	"2:	sc.w %[t],%z[nv],%[u]		        \n"
	"	bnez %[t],1b				\n"
	"3:						\n"
	RISCV_FULL_BARRIER
	_ASM_EXTABLE_UACCESS_ERR(1b, 3b, %[r])
	_ASM_EXTABLE_UACCESS_ERR(2b, 3b, %[r])
	: [r] "+r" (ret), [v] "=&r" (val), [u] "+m" (*uaddr), [t] "=&r" (tmp)
	: [ov] "Jr" ((long)(int)oldval), [nv] "Jr" (newval)
	: "memory");

	*uval = val;
	return ret;
}

#endif /* _ASM_RISCV_FUTEX_H */
