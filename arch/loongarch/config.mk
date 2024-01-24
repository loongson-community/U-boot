# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2022
# Loongson Ltd. maoxiaochuan@loongson.cn


32bit-emul		:= elf32loongarch
64bit-emul		:= elf64loongarch
32bit-bfd		:= elf32-loongarch
64bit-bfd		:= elf64-loongarch

ifdef CONFIG_64BIT
PLATFORM_CPPFLAGS	+= -mabi=lp64d
KBUILD_LDFLAGS		+= -m $(64bit-emul)
OBJCOPYFLAGS		+= -O $(64bit-bfd)
CONFIG_STANDALONE_LOAD_ADDR	?= 0x9000000080200000
else
PLATFORM_CPPFLAGS	+= -mabi=lp32d
KBUILD_LDFLAGS		+= -m $(32bit-emul)
OBJCOPYFLAGS		+= -O $(32bit-bfd)
CONFIG_STANDALONE_LOAD_ADDR	?= 0x80200000
endif

PLATFORM_CPPFLAGS += -D__LOONGARCH__
PLATFORM_ELFFLAGS += -B loongarch $(OBJCOPYFLAGS)

PLATFORM_CPPFLAGS		+= -G 0 -fno-pic -fno-builtin -fno-delayed-branch -fno-plt
PLATFORM_CPPFLAGS		+= -mstrict-align
# PLATFORM_CPPFLAGS		+= -msoft-float
# when use gcc-13 we can add "--no-warn-rwx-segment" to void
# loongarch64-unknown-linux-gnu-ld.bfd: warning: u-boot-spl has 
# a LOAD segment with RWX permissions 
KBUILD_LDFLAGS			+= -G 0 -static -n -nostdlib 

PLATFORM_RELFLAGS		+= -ffunction-sections -fdata-sections
LDFLAGS_FINAL			+= --gc-sections
LDFLAGS_STANDALONE		+= --gc-sections

LDFLAGS_u-boot += --gc-sections -static -pie
