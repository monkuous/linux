/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014 Regents of the University of California
 */

#ifndef _ASM_RISCV_CMPXCHG_H
#define _ASM_RISCV_CMPXCHG_H

#include <linux/bug.h>

#include <asm/fence.h>
#include <asm/processor.h>

#define __arch_xchg_masked(prepend, append, r, p, n)			\
({									\
	u32 *__ptr32b = (u32 *)((ulong)(p) & ~0x3);			\
	ulong __s = ((ulong)(p) & (0x4 - sizeof(*p))) * BITS_PER_BYTE;	\
	ulong __mask = GENMASK(((sizeof(*p)) * BITS_PER_BYTE) - 1, 0)	\
				<< __s;					\
	ulong __newx = (ulong)(n) << __s;				\
	ulong __retx;							\
	ulong __rc;							\
									\
	__asm__ __volatile__ (						\
	       prepend							\
	       "0:	lr.w %0, %2\n"					\
	       "	and  %1, %0, %z4\n"				\
	       "	or   %1, %1, %z3\n"				\
	       "	sc.w %1, %1, %2\n"				\
	       "	bnez %1, 0b\n"					\
	       append							\
	       : "=&r" (__retx), "=&r" (__rc), "+A" (*(__ptr32b))	\
	       : "rJ" (__newx), "rJ" (~__mask), "rJ" (__ptr32b)		\
	       : "memory");						\
									\
	r = (__typeof__(*(p)))((__retx & __mask) >> __s);		\
})

#define __arch_xchg(prepend, append, r, p, n)				\
({									\
	ulong __rc;							\
									\
	__asm__ __volatile__ (						\
		prepend							\
		"0:	lr.w %0, %2\n"					\
		"	sc.w %1, %3, %2\n"				\
		"	bnez %1, 0b\n"					\
		append							\
		: "=&r" (r), "=&r" (__rc), "+A" (*(p))			\
		: "r" (n)						\
		: "memory");						\
})

#define _arch_xchg(ptr, new, prepend, append)				\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(__ptr)) __new = (new);				\
	__typeof__(*(__ptr)) __ret;					\
									\
	switch (sizeof(*__ptr)) {					\
	case 1:								\
	case 2:								\
		__arch_xchg_masked(prepend, append, __ret, __ptr,	\
				   __new);				\
		break;							\
	case 4:								\
		__arch_xchg(prepend, append, __ret, __ptr, __new);	\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	(__typeof__(*(__ptr)))__ret;					\
})

#define arch_xchg_relaxed(ptr, x)					\
	_arch_xchg(ptr, x, "", "")

#define arch_xchg_acquire(ptr, x)					\
	_arch_xchg(ptr, x, "", RISCV_ACQUIRE_BARRIER)

#define arch_xchg_release(ptr, x)					\
	_arch_xchg(ptr, x, RISCV_RELEASE_BARRIER, "")

#define arch_xchg(ptr, x)						\
	_arch_xchg(ptr, x, RISCV_RELEASE_BARRIER, RISCV_FULL_BARRIER)

#define xchg32(ptr, x)							\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 4);				\
	arch_xchg((ptr), (x));						\
})

#define xchg64(ptr, x)							\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_xchg((ptr), (x));						\
})

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define __arch_cmpxchg_masked(prepend, append, r, p, o, n)		\
({									\
	u32 *__ptr32b = (u32 *)((ulong)(p) & ~0x3);			\
	ulong __s = ((ulong)(p) & (0x4 - sizeof(*p))) * BITS_PER_BYTE;	\
	ulong __mask = GENMASK(((sizeof(*p)) * BITS_PER_BYTE) - 1, 0)	\
		       << __s;						\
	ulong __newx = (ulong)(n) << __s;				\
	ulong __oldx = (ulong)(o) << __s;				\
	ulong __retx;							\
	ulong __rc;							\
									\
	__asm__ __volatile__ (						\
		prepend							\
		"0:	lr.w %0, %2\n"					\
		"	and  %1, %0, %z5\n"				\
		"	sub  %1, %1, %z3\n"				\
		"	bnez %1, 1f\n"					\
		"	and  %1, %0, %z6\n"				\
		"	or   %1, %1, %z4\n"				\
		"	sc.w %1, %1, %2\n"				\
		"	bnez %1, 0b\n"					\
		append							\
		"1:\n"							\
		: "=&r" (__retx), "=&r" (__rc), "+A" (*(__ptr32b))	\
		: "rJ" ((long)__oldx), "rJ" (__newx),			\
		  "rJ" (__mask), "rJ" (~__mask)				\
		: "memory");						\
									\
	r = (__typeof__(*(p)))((__retx & __mask) >> __s);		\
})

#define __arch_cmpxchg(prepend, append, r, p, co, o, n)			\
({									\
	register unsigned int __rc;					\
									\
	__asm__ __volatile__ (						\
		prepend							\
		"0:	lr.w %0, %2\n"					\
		"	sub  %1, %0, %z3\n"				\
		"	bnez %1, 1f\n"					\
		"	sc.w %1, %z4, %2\n"				\
		"	bnez %1, 0b\n"					\
		append							\
		"1:\n"							\
		: "=&r" (r), "=&r" (__rc), "+A" (*(p))			\
		: "rJ" (co o), "rJ" (n)					\
		: "memory");						\
})

#define _arch_cmpxchg(ptr, old, new, prepend, append)			\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(__ptr)) __old = (old);				\
	__typeof__(*(__ptr)) __new = (new);				\
	__typeof__(*(__ptr)) __ret;					\
									\
	switch (sizeof(*__ptr)) {					\
	case 1:								\
	case 2:								\
		__arch_cmpxchg_masked(prepend, append, __ret, __ptr,	\
				      __old, __new);			\
		break;							\
	case 4:								\
		__arch_cmpxchg(prepend, append, __ret, __ptr,		\
			       (long)(int)(long), __old, __new);	\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	(__typeof__(*(__ptr)))__ret;					\
})

#define arch_cmpxchg_relaxed(ptr, o, n)					\
	_arch_cmpxchg((ptr), (o), (n), "", "")

#define arch_cmpxchg_acquire(ptr, o, n)					\
	_arch_cmpxchg((ptr), (o), (n), "", RISCV_ACQUIRE_BARRIER)

#define arch_cmpxchg_release(ptr, o, n)					\
	_arch_cmpxchg((ptr), (o), (n), RISCV_RELEASE_BARRIER, "")

#define arch_cmpxchg(ptr, o, n)						\
	_arch_cmpxchg((ptr), (o), (n), RISCV_RELEASE_BARRIER, 		\
		      RISCV_FULL_BARRIER)

#define arch_cmpxchg_local(ptr, o, n)					\
	arch_cmpxchg_relaxed((ptr), (o), (n))

#endif /* _ASM_RISCV_CMPXCHG_H */
