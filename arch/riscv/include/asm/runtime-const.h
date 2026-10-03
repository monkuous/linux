/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_RISCV_RUNTIME_CONST_H
#define _ASM_RISCV_RUNTIME_CONST_H

#ifdef MODULE
  #error "Cannot use runtime-const infrastructure from modules"
#endif

#include <asm/asm.h>
#include <asm/cacheflush.h>
#include <linux/memory.h>
#include <asm/text-patching.h>

#include <linux/uaccess.h>

#define RUNTIME_MAGIC __ASM_STR(0x89ABCDEF)

#define RISCV_INSN_NOP4 0x00000013U

#define runtime_const_ptr(sym)						\
({									\
	typeof(sym) __ret;						\
	asm_inline(".option push\n\t"					\
		".option norvc\n\t"					\
		".option norelax\n\t"					\
		"1:\t"							\
		"lui	%[__ret], %%hi(" RUNTIME_MAGIC ")\n\t"		\
		"addi	%[__ret],%[__ret], %%lo(" RUNTIME_MAGIC ")\n\t"	\
		".option pop\n\t"					\
		".pushsection runtime_ptr_" #sym ",\"a\"\n\t"		\
		".long 1b - .\n\t"					\
		".popsection"						\
		: [__ret] "=r" (__ret));				\
	__ret;								\
})

#define runtime_const_shift_right_32(val, sym)			\
({								\
	u32 __ret;						\
	asm_inline(".option push\n\t"				\
		".option norvc\n\t"				\
		"1:\t"						\
		SRLI " %[__ret],%[__val],12\n\t"		\
		".option pop\n\t"				\
		".pushsection runtime_shift_" #sym ",\"a\"\n\t"	\
		".long 1b - .\n\t"				\
		".popsection"					\
		: [__ret] "=r" (__ret)				\
		: [__val] "r" (val));				\
	__ret;							\
})

#define runtime_const_mask_32(val, sym)				\
({								\
	u32 __ret;						\
	asm_inline(".option push\n\t"				\
		".option norvc\n\t"				\
		"1:\t"						\
		SLLI " %[__ret],%[__val],12\n\t"		\
		SRLI " %[__ret],%[__ret],12\n\t"		\
		".option pop\n\t"				\
		".pushsection runtime_mask_" #sym ",\"a\"\n\t"	\
		".long 1b - .\n\t"				\
		".popsection"					\
		: [__ret] "=r" (__ret)				\
		: [__val] "r" (val));				\
	__ret;							\
})

#define runtime_const_init(type, sym) do {			\
	extern s32 __start_runtime_##type##_##sym[];		\
	extern s32 __stop_runtime_##type##_##sym[];		\
								\
	runtime_const_fixup(__runtime_fixup_##type,		\
			    (unsigned long)(sym),		\
			    __start_runtime_##type##_##sym,	\
			    __stop_runtime_##type##_##sym);	\
} while (0)

static inline void __runtime_fixup_caches(void *where, unsigned int insns)
{
	/* On riscv there are currently only cache-wide flushes so va is ignored. */
	__always_unused uintptr_t va = (uintptr_t)where;

	flush_icache_range(va, va + 4 * insns);
}

/*
 * The 32-bit immediate is stored in a lui+addi pairing.
 * lui holds the upper 20 bits of the immediate in the first 20 bits of the instruction.
 * addi holds the lower 12 bits of the immediate in the first 12 bits of the instruction.
 */
static inline void __runtime_fixup_32(__le16 *lui_parcel, __le16 *addi_parcel, unsigned int val)
{
	unsigned int lower_immediate, upper_immediate;
	u32 lui_insn, addi_insn, addi_insn_mask;
	__le32 lui_res, addi_res;

	/* Mask out upper 12 bit of addi */
	addi_insn_mask = 0x000fffff;

	lui_insn = (u32)le16_to_cpu(lui_parcel[0]) | (u32)le16_to_cpu(lui_parcel[1]) << 16;
	addi_insn = (u32)le16_to_cpu(addi_parcel[0]) | (u32)le16_to_cpu(addi_parcel[1]) << 16;

	lower_immediate = sign_extend32(val, 11);
	upper_immediate = (val - lower_immediate);

	if (upper_immediate & 0xfffff000) {
		/* replace upper 20 bits of lui with upper immediate */
		lui_insn &= 0x00000fff;
		lui_insn |= upper_immediate & 0xfffff000;
	} else {
		/* replace lui with nop if immediate is small enough to fit in addi */
		lui_insn = RISCV_INSN_NOP4;
		/*
		 * lui is being skipped, so do a load instead of an add. A load
		 * is performed by adding with the x0 register. Setting rs to
		 * zero with the following mask will accomplish this goal.
		 */
		addi_insn_mask &= 0x07fff;
	}

	if (lower_immediate & 0x00000fff || lui_insn == RISCV_INSN_NOP4) {
		/* replace upper 12 bits of addi with lower 12 bits of val */
		addi_insn &= addi_insn_mask;
		addi_insn |= (lower_immediate & 0x00000fff) << 20;
	} else {
		/* replace addi with nop if lower_immediate is empty */
		addi_insn = RISCV_INSN_NOP4;
	}

	addi_res = cpu_to_le32(addi_insn);
	lui_res = cpu_to_le32(lui_insn);
	mutex_lock(&text_mutex);
	patch_insn_write(addi_parcel, &addi_res, sizeof(addi_res));
	patch_insn_write(lui_parcel, &lui_res, sizeof(lui_res));
	mutex_unlock(&text_mutex);
}

static inline void __runtime_fixup_ptr(void *where, unsigned long val)
{
	__runtime_fixup_32(where, where + 4, val);
	__runtime_fixup_caches(where, 2);
}

/*
 * Replace the least significant 5 bits of the srli/srliw immediate that is
 * located at bits 20-24
 */
static inline void __runtime_fixup_shift(void *where, unsigned long val)
{
	__le32 *ptr = where;
	__le32 res;
	u32 insn;

	insn = le32_to_cpu(ptr[0]);

	insn &= 0xfe0fffff;
	insn |= (val & 0b11111) << 20;

	res = cpu_to_le32(insn);
	mutex_lock(&text_mutex);
	patch_text_nosync(where, &res, sizeof(insn));
	mutex_unlock(&text_mutex);
}

static inline void __runtime_fixup_mask(void *where, unsigned long val)
{
	unsigned int width = (val) ? __fls(val) + 1 : 0;

	/*
	 * XXX: Current implementation only supports patching masks of
	 * form GENMASK(width, 0) (width >= 0) using a SRLI + SLLI
	 * sequence instead of LUI + ADDI + AND sequence to improve
	 * performance, density, and covers all the current use-cases.
	 *
	 * When the need arises to support any generic mask, and this
	 * BUG_ON() is tripped, consider using a:
	 *
	 *   lui  %[__ret], #imm16
	 *   addi %[__ret], #imm16
	 *
	 * sequence to load the 32bit const mask, and perform a logical
	 * and outside the asm block before returning the result. Fixup
	 * can simply reuse the existing __runtime_fixup_32() to patch
	 * the LUI + ADDI sequence.
	 */
	BUG_ON(!val || width > 31 || (GENMASK(width - 1, 0) != val));

	__runtime_fixup_shift(where, 32 - width);
	__runtime_fixup_shift(where + 4, 32 - width);
}

static inline void runtime_const_fixup(void (*fn)(void *, unsigned long),
				       unsigned long val, s32 *start, s32 *end)
{
	while (start < end) {
		fn(*start + (void *)start, val);
		start++;
	}
}

#endif /* _ASM_RISCV_RUNTIME_CONST_H */
