/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_RISCV_ATOMIC_H
#define _ASM_RISCV_ATOMIC_H

#include <asm-generic/atomic64.h>

#include <asm/cmpxchg.h>

#define __atomic_acquire_fence()					\
	__asm__ __volatile__(RISCV_ACQUIRE_BARRIER "" ::: "memory")

#define __atomic_release_fence()					\
	__asm__ __volatile__(RISCV_RELEASE_BARRIER "" ::: "memory");

static __always_inline int arch_atomic_read(const atomic_t *v)
{
	return READ_ONCE(v->counter);
}
static __always_inline void arch_atomic_set(atomic_t *v, int i)
{
	WRITE_ONCE(v->counter, i);
}

/*
 * First, the atomic ops that have no ordering constraints and therefore don't
 * have the AQ or RL bits set.  These don't return anything, so there's only
 * one version to worry about.
 */
#define ATOMIC_OP(op, asm_op, I, asm_type, c_type, prefix)		\
static __always_inline							\
void arch_atomic##prefix##_##op(c_type i, atomic##prefix##_t *v)	\
{									\
	__asm__ __volatile__ (						\
		"	amo" #asm_op "." #asm_type " zero, %1, %0"	\
		: "+A" (v->counter)					\
		: "r" (I)						\
		: "memory");						\
}									\

#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_OP (op, asm_op, I, w, int,   )

ATOMIC_OPS(add, add,  i)
ATOMIC_OPS(sub, add, -i)
ATOMIC_OPS(and, and,  i)
ATOMIC_OPS( or,  or,  i)
ATOMIC_OPS(xor, xor,  i)

#undef ATOMIC_OP
#undef ATOMIC_OPS

/*
 * Atomic ops that have ordered, relaxed, acquire, and release variants.
 * There's two flavors of these: the arithmetic ops have both fetch and return
 * versions, while the logical ops only have fetch versions.
 */
#define ATOMIC_FETCH_OP(op, asm_op, I, asm_type, c_type, prefix)	\
static __always_inline							\
c_type arch_atomic##prefix##_fetch_##op##_relaxed(c_type i,		\
					     atomic##prefix##_t *v)	\
{									\
	register c_type ret;						\
	__asm__ __volatile__ (						\
		"	amo" #asm_op "." #asm_type " %1, %2, %0"	\
		: "+A" (v->counter), "=r" (ret)				\
		: "r" (I)						\
		: "memory");						\
	return ret;							\
}									\
static __always_inline							\
c_type arch_atomic##prefix##_fetch_##op(c_type i, atomic##prefix##_t *v)	\
{									\
	register c_type ret;						\
	__asm__ __volatile__ (						\
		"	amo" #asm_op "." #asm_type ".aqrl  %1, %2, %0"	\
		: "+A" (v->counter), "=r" (ret)				\
		: "r" (I)						\
		: "memory");						\
	return ret;							\
}

#define ATOMIC_OP_RETURN(op, asm_op, c_op, I, asm_type, c_type, prefix)	\
static __always_inline							\
c_type arch_atomic##prefix##_##op##_return_relaxed(c_type i,		\
					      atomic##prefix##_t *v)	\
{									\
        return arch_atomic##prefix##_fetch_##op##_relaxed(i, v) c_op I;	\
}									\
static __always_inline							\
c_type arch_atomic##prefix##_##op##_return(c_type i, atomic##prefix##_t *v)	\
{									\
        return arch_atomic##prefix##_fetch_##op(i, v) c_op I;		\
}

#define ATOMIC_OPS(op, asm_op, c_op, I)					\
        ATOMIC_FETCH_OP( op, asm_op,       I, w, int,   )		\
        ATOMIC_OP_RETURN(op, asm_op, c_op, I, w, int,   )

ATOMIC_OPS(add, add, +,  i)
ATOMIC_OPS(sub, add, +, -i)

#define arch_atomic_add_return_relaxed	arch_atomic_add_return_relaxed
#define arch_atomic_sub_return_relaxed	arch_atomic_sub_return_relaxed
#define arch_atomic_add_return		arch_atomic_add_return
#define arch_atomic_sub_return		arch_atomic_sub_return

#define arch_atomic_fetch_add_relaxed	arch_atomic_fetch_add_relaxed
#define arch_atomic_fetch_sub_relaxed	arch_atomic_fetch_sub_relaxed
#define arch_atomic_fetch_add		arch_atomic_fetch_add
#define arch_atomic_fetch_sub		arch_atomic_fetch_sub

#undef ATOMIC_OPS

#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_FETCH_OP(op, asm_op, I, w, int,   )

ATOMIC_OPS(and, and, i)
ATOMIC_OPS( or,  or, i)
ATOMIC_OPS(xor, xor, i)

#define arch_atomic_fetch_and_relaxed	arch_atomic_fetch_and_relaxed
#define arch_atomic_fetch_or_relaxed	arch_atomic_fetch_or_relaxed
#define arch_atomic_fetch_xor_relaxed	arch_atomic_fetch_xor_relaxed
#define arch_atomic_fetch_and		arch_atomic_fetch_and
#define arch_atomic_fetch_or		arch_atomic_fetch_or
#define arch_atomic_fetch_xor		arch_atomic_fetch_xor

#undef ATOMIC_OPS

#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN

#define _arch_atomic_fetch_add_unless(_prev, _rc, counter, _a, _u, sfx)	\
({									\
	__asm__ __volatile__ (						\
		"0:	lr." sfx "     %[p],  %[c]\n"			\
		"	beq	       %[p],  %[u], 1f\n"		\
		"	add            %[rc], %[p], %[a]\n"		\
		"	sc." sfx ".rl  %[rc], %[rc], %[c]\n"		\
		"	bnez           %[rc], 0b\n"			\
		RISCV_FULL_BARRIER					\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		: [a]"r" (_a), [u]"r" (_u)				\
		: "memory");						\
})

/* This is required to provide a full barrier on success. */
static __always_inline int arch_atomic_fetch_add_unless(atomic_t *v, int a, int u)
{
       int prev, rc;

	_arch_atomic_fetch_add_unless(prev, rc, v->counter, a, u, "w");

	return prev;
}
#define arch_atomic_fetch_add_unless arch_atomic_fetch_add_unless

#define _arch_atomic_inc_unless_negative(_prev, _rc, counter, sfx)	\
({									\
	__asm__ __volatile__ (						\
		"0:	lr." sfx "      %[p],  %[c]\n"			\
		"	bltz            %[p],  1f\n"			\
		"	addi            %[rc], %[p], 1\n"		\
		"	sc." sfx ".rl   %[rc], %[rc], %[c]\n"		\
		"	bnez            %[rc], 0b\n"			\
		RISCV_FULL_BARRIER					\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		:							\
		: "memory");						\
})

static __always_inline bool arch_atomic_inc_unless_negative(atomic_t *v)
{
	int prev, rc;

	_arch_atomic_inc_unless_negative(prev, rc, v->counter, "w");

	return !(prev < 0);
}

#define arch_atomic_inc_unless_negative arch_atomic_inc_unless_negative

#define _arch_atomic_dec_unless_positive(_prev, _rc, counter, sfx)	\
({									\
	__asm__ __volatile__ (						\
		"0:	lr." sfx "      %[p],  %[c]\n"			\
		"	bgtz            %[p],  1f\n"			\
		"	addi            %[rc], %[p], -1\n"		\
		"	sc." sfx ".rl   %[rc], %[rc], %[c]\n"		\
		"	bnez            %[rc], 0b\n"			\
		RISCV_FULL_BARRIER					\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		:							\
		: "memory");						\
})

static __always_inline bool arch_atomic_dec_unless_positive(atomic_t *v)
{
	int prev, rc;

	_arch_atomic_dec_unless_positive(prev, rc, v->counter, "w");

	return !(prev > 0);
}

#define arch_atomic_dec_unless_positive arch_atomic_dec_unless_positive

#define _arch_atomic_dec_if_positive(_prev, _rc, counter, sfx)		\
({									\
	__asm__ __volatile__ (						\
		"0:	lr." sfx "     %[p],  %[c]\n"			\
		"	addi           %[rc], %[p], -1\n"		\
		"	bltz           %[rc], 1f\n"			\
		"	sc." sfx ".rl  %[rc], %[rc], %[c]\n"		\
		"	bnez           %[rc], 0b\n"			\
		RISCV_FULL_BARRIER					\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		:							\
		: "memory");						\
})

static __always_inline int arch_atomic_dec_if_positive(atomic_t *v)
{
       int prev, rc;

	_arch_atomic_dec_if_positive(prev, rc, v->counter, "w");

	return prev - 1;
}

#define arch_atomic_dec_if_positive arch_atomic_dec_if_positive

#endif /* _ASM_RISCV_ATOMIC_H */
