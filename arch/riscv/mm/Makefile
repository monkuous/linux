# SPDX-License-Identifier: GPL-2.0-only

CFLAGS_init.o := -mcmodel=medany

ifdef CONFIG_FTRACE
CFLAGS_REMOVE_init.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_cacheflush.o = $(CC_FLAGS_FTRACE)
endif

KCOV_INSTRUMENT_init.o := n

obj-y += init.o
obj-y += extable.o fault.o pageattr.o pgtable.o tlbflush.o
obj-y += cacheflush.o
obj-y += context.o
obj-y += dma-noncoherent.o

obj-$(CONFIG_PTDUMP) += ptdump.o

obj-$(CONFIG_DEBUG_VIRTUAL) += physaddr.o
