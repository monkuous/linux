#include <linux/compiler.h>
#include <linux/delay.h>

noinline void __delay(unsigned long loops)
{
	asm volatile(
		"	beqz %0, 2f\n"
		"	jal zero, 1f\n"
		".balign 16\n"
		"1:	addi %0, %0, -1\n"
		"	bnez %0, 1b\n"
		"2:"
		: "+r" (loops)
	);
}

void __const_udelay(unsigned long xloops)
{
	xloops *= 4;
	xloops = ((uint64_t)xloops * (loops_per_jiffy * (HZ / 4))) >> 32;

	__delay(++xloops);
}

void __udelay(unsigned long loops)
{
	__const_udelay(loops * UDELAY_CONST_MULT);
}
