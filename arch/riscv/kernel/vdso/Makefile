# SPDX-License-Identifier: GPL-2.0-only
# Copied from arch/tile/kernel/vdso/Makefile

# Include the generic Makefile to check the built vdso.
include $(srctree)/lib/vdso/Makefile.include
# Symbols present in the vdso
vdso-syms  = rt_sigreturn
vdso-syms += getcpu
vdso-syms += flush_icache
vdso-syms += hwprobe
vdso-syms += sys_hwprobe

# Files to link into the vdso
obj-vdso = $(patsubst %, %.o, $(vdso-syms)) note.o

ccflags-y := -fno-stack-protector
ccflags-y += -DDISABLE_BRANCH_PROFILING
ccflags-y += -fno-builtin
ccflags-y += $(CONFIG_CC_OPT_INLINE_MEMSET)
ccflags-y += $(KBUILD_BASE_ISA)
asflags-y += $(KBUILD_BASE_ISA)

ccflags-remove-y += $(CC_FLAGS_FTRACE) $(CC_FLAGS_LTO)

ifneq ($(c-gettimeofday-y),)
  CFLAGS_vgettimeofday.o += -fPIC -include $(c-gettimeofday-y)
endif

ifneq ($(c-getrandom-y),)
  CFLAGS_getrandom.o += -fPIC -include $(c-getrandom-y)
endif

CFLAGS_hwprobe.o += -fPIC

# Build rules
vdso_offsets := vdso-offsets.h
vdso_o := vdso.o
vdso_so := vdso.so
vdso_so_dbg := vdso.so.dbg
vdso_lds := vdso.lds

targets := $(obj-vdso) $(vdso_so) $(vdso_so_dbg) $(vdso_lds)

obj-vdso := $(addprefix $(obj)/, $(obj-vdso))

obj-y += vdso.o
CPPFLAGS_$(vdso_lds) += -P -C -U$(ARCH)

# Force dependency
$(obj)/$(vdso_o): $(obj)/$(vdso_so)

# link rule for the .so file, .lds has to be first
$(obj)/$(vdso_so_dbg): $(obj)/$(vdso_lds) $(obj-vdso) FORCE
	$(call if_changed,vdsold_and_check)
LDFLAGS_$(vdso_so_dbg) = -shared -soname=linux-vdso.so.1 \
	--build-id=sha1 --eh-frame-hdr

# strip rule for the .so file
$(obj)/%.so: OBJCOPYFLAGS := -S
$(obj)/%.so: $(obj)/%.so.dbg FORCE
	$(call if_changed,objcopy)

# Generate VDSO offsets using helper script
gen-vdsosym := $(src)/gen_vdso_offsets.sh
quiet_cmd_vdsosym = VDSOSYM $@
	cmd_vdsosym = $(NM) $< | $(gen-vdsosym) $(OFFSET_SUFFIX) | LC_ALL=C sort > $@

include/generated/$(vdso_offsets): $(obj)/$(vdso_so_dbg) FORCE
	$(call if_changed,vdsosym)

# actual build commands
# The DSO images are built using a special linker script
# Make sure only to export the intended __vdso_xxx symbol offsets.
quiet_cmd_vdsold_and_check = VDSOLD  $@
      cmd_vdsold_and_check = $(LD) $(CFI_FULL) $(ld_flags) -T $(filter-out FORCE,$^) -o $@.tmp && \
                   $(OBJCOPY) $(patsubst %, -G __vdso_%, $(vdso-syms)) $@.tmp $@ && \
                   rm $@.tmp && \
                   $(cmd_vdso_check)
