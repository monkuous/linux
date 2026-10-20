# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#

LDFLAGS_vmlinux := -z norelro
ifeq ($(CONFIG_DYNAMIC_FTRACE),y)
	LDFLAGS_vmlinux += --no-relax
	KBUILD_CPPFLAGS += -DCC_USING_PATCHABLE_FUNCTION_ENTRY
	CC_FLAGS_FTRACE := -fpatchable-function-entry=4,2
endif

ifeq ($(CONFIG_CMODEL_MEDLOW),y)
KBUILD_CFLAGS_MODULE += -mcmodel=medany
endif

KBUILD_CFLAGS += -mabi=ilp32
KBUILD_AFLAGS += -mabi=ilp32
KBUILD_LDFLAGS += -melf32lriscv

UTS_MACHINE := riscv32

# LLVM has an issue with target-features and LTO: https://github.com/llvm/llvm-project/issues/59350
# Ensure it is aware of linker relaxation with LTO, otherwise relocations may
# be incorrect: https://github.com/llvm/llvm-project/issues/65090
ifeq ($(CONFIG_LTO_CLANG),y)
	KBUILD_LDFLAGS += -mllvm -mattr=+c -mllvm -mattr=+relax
endif

# ISA string setting
riscv-march-y				:= rv32im_zalrsc

ifdef CONFIG_TOOLCHAIN_NEEDS_OLD_ISA_SPEC
KBUILD_CFLAGS += -Wa,-misa-spec=2.2
KBUILD_AFLAGS += -Wa,-misa-spec=2.2
else
riscv-march-$(CONFIG_TOOLCHAIN_NEEDS_EXPLICIT_ZICSR_ZIFENCEI) := $(riscv-march-y)_zicsr_zifencei
endif

KBUILD_BASE_ISA = -march=$(riscv-march-y)
export KBUILD_BASE_ISA

KBUILD_CFLAGS += $(KBUILD_BASE_ISA)
KBUILD_AFLAGS += $(KBUILD_BASE_ISA)

KBUILD_CFLAGS += -mno-save-restore

ifeq ($(CONFIG_CMODEL_MEDLOW),y)
	KBUILD_CFLAGS += -mcmodel=medlow
endif
ifeq ($(CONFIG_CMODEL_MEDANY),y)
	KBUILD_CFLAGS += -mcmodel=medany
endif

# Avoid generating .eh_frame sections.
KBUILD_CFLAGS += -fno-asynchronous-unwind-tables -fno-unwind-tables

# The RISC-V attributes frequently cause compatibility issues and provide no
# information, so just turn them off.
KBUILD_CFLAGS += $(call cc-option,-mno-riscv-attribute)
KBUILD_AFLAGS += $(call cc-option,-mno-riscv-attribute)
KBUILD_CFLAGS += $(call as-option,-Wa$(comma)-mno-arch-attr)
KBUILD_AFLAGS += $(call as-option,-Wa$(comma)-mno-arch-attr)

KBUILD_CFLAGS_MODULE += $(call cc-option,-mno-relax)
KBUILD_AFLAGS_MODULE += $(call as-option,-Wa$(comma)-mno-relax)

KBUILD_CFLAGS += $(call cc-option,-mstrict-align)

ifeq ($(CONFIG_STACKPROTECTOR_PER_TASK),y)
prepare: stack_protector_prepare
stack_protector_prepare: prepare0
	$(eval KBUILD_CFLAGS += -mstack-protector-guard=tls		  \
				-mstack-protector-guard-reg=tp		  \
				-mstack-protector-guard-offset=$(shell	  \
			awk '{if ($$2 == "TSK_STACK_CANARY") print $$3;}' \
				$(objtree)/include/generated/asm-offsets.h))
endif

# arch specific predefines for sparse
CHECKFLAGS += -D__riscv -D__riscv_xlen=32

# Default target when executing plain make
boot					:= arch/riscv/boot
boot-image-y				:= Image
boot-image-$(CONFIG_KERNEL_BZIP2)	:= Image.bz2
boot-image-$(CONFIG_KERNEL_GZIP)	:= Image.gz
boot-image-$(CONFIG_KERNEL_LZ4)		:= Image.lz4
boot-image-$(CONFIG_KERNEL_LZMA)	:= Image.lzma
boot-image-$(CONFIG_KERNEL_LZO)		:= Image.lzo
boot-image-$(CONFIG_KERNEL_ZSTD)	:= Image.zst
boot-image-$(CONFIG_KERNEL_XZ)		:= Image.xz
KBUILD_IMAGE				:= $(boot)/$(boot-image-y)

libs-y += arch/riscv/lib/

ifeq ($(KBUILD_EXTMOD),)
prepare: vdso_prepare
vdso_prepare: prepare0
	$(Q)$(MAKE) $(build)=arch/riscv/kernel/vdso include/generated/vdso-offsets.h

endif

vdso-install-y				+= arch/riscv/kernel/vdso/vdso.so.dbg

BOOT_TARGETS := Image Image.gz Image.bz2 Image.lz4 Image.lzma Image.lzo Image.zst Image.xz

all:	$(notdir $(KBUILD_IMAGE))

Image.gz Image.bz2 Image.lz4 Image.lzma Image.lzo Image.zst Image.xz: Image

$(BOOT_TARGETS): vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@
	@$(kecho) '  Kernel: $(boot)/$@ is ready'

# the install target always installs KBUILD_IMAGE (which may be compressed)
# but keep the zinstall target for compatibility with older releases
install zinstall:
	$(call cmd,install)

define archhelp
  echo  '  Image		- Uncompressed kernel image (arch/riscv/boot/Image)'
  echo  '  Image.gz	- Compressed kernel image (arch/riscv/boot/Image.gz)'
  echo  '  Image.bz2	- Compressed kernel image (arch/riscv/boot/Image.bz2)'
  echo  '  Image.lz4	- Compressed kernel image (arch/riscv/boot/Image.lz4)'
  echo  '  Image.lzma	- Compressed kernel image (arch/riscv/boot/Image.lzma)'
  echo  '  Image.lzo	- Compressed kernel image (arch/riscv/boot/Image.lzo)'
  echo  '  Image.zst	- Compressed kernel image (arch/riscv/boot/Image.zst)'
  echo  '  Image.xz	- Compressed kernel image (arch/riscv/boot/Image.xz)'
  echo  '  install	- Install kernel using (your) ~/bin/$(INSTALLKERNEL) or'
  echo  '		  (distribution) /sbin/$(INSTALLKERNEL) or install to '
  echo  '		  $$(INSTALL_PATH)'
endef
