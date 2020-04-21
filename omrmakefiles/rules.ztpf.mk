###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#      
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#    
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

# z/TPF only has one s390x configuration:
#  - OMR_HOST_OS=linux_ztpf, OMR_HOST_ARCH=s390, OMR_ENV_DATA64=1
#  - OMR_TOOLCHAIN=gcc, OMR_RTTI=0, OMR_OPTIMIZE=1, OMR_DEBUG=0
#  - OMR_WARNINGS_AS_ERRORS=1, OMR_ENHANCED_WARNINGS=1
# Applicable conditionals to the above fields have been left in this file to
# maintain similarity with Linux, while other architecture-dependent flags
# have been removed for simplicity.

###
### Helpers
###

ifeq (s390,$(OMR_HOST_ARCH))
    ifeq (0,$(OMR_ENV_DATA64))
        J9M31:=-m31
    endif
endif

###
### Global Flags
###

GLOBAL_CPPFLAGS += -DLINUX -D_REENTRANT -DJ9ZTPF -DOMRZTPF -DOMRPORT_JSIG_SUPPORT -DIBMLOCKS
GLOBAL_CPPFLAGS += -D_GNU_SOURCE -DIBM_ATOE -D_TPF_SOURCE -D_TPF_THREADS -DZTPF_POSIX_SOCKET

ifeq (s390,$(OMR_HOST_ARCH))
    GLOBAL_CXXFLAGS+=$(J9M31)
    GLOBAL_CFLAGS+=$(J9M31)
    GLOBAL_LDFLAGS+=$(J9M31)
endif

# Compile without exceptions
ifeq (gcc,$(OMR_TOOLCHAIN))
    ifeq (1,$(OMR_RTTI))
        GLOBAL_CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics
    else
        GLOBAL_CXXFLAGS+=-fno-exceptions -fno-rtti -fno-threadsafe-statics
    endif
endif

## Position Independent compile flag
ifeq (gcc,$(OMR_TOOLCHAIN))
    ifeq (s390,$(OMR_HOST_ARCH))
        GLOBAL_CFLAGS+=-fPIC
        GLOBAL_CXXFLAGS+=-fPIC
    endif
endif

## ASFLAGS
# xlc on ppc does not actually understand this
# option, it is silently ignored.
GLOBAL_ASFLAGS+=-noexecstack

ifeq (s390,$(OMR_HOST_ARCH))
    ifeq (0,$(OMR_ENV_DATA64))
        GLOBAL_ASFLAGS+= -mzarch
    endif
    GLOBAL_ASFLAGS+= -march=z9-109 $(J9M31) -o $*.o
endif

###
### Platform Flags
###

## Debugging Infomation
ifeq (1,$(OMR_DEBUG))
    GLOBAL_ASFLAGS+=-g
    GLOBAL_CXXFLAGS+=-g
    GLOBAL_CFLAGS+=-g
    GLOBAL_LDFLAGS+=-g
endif

#-- Add Platform flags
ifeq (s390,$(OMR_HOST_ARCH))
    GLOBAL_CFLAGS+=$(J9M31) -fno-strict-aliasing
    GLOBAL_CXXFLAGS+=$(J9M31) -fno-strict-aliasing
    GLOBAL_CPPFLAGS+=-DS390 -D_LONG_LONG
    ifeq (1,$(OMR_ENV_DATA64))
        GLOBAL_CPPFLAGS+=-DS39064
    endif
endif

ifneq (,$(findstring executable,$(ARTIFACT_TYPE)))
    ## Default Libraries
    DEFAULT_LIBS:=-Wl,--disable-new-dtags,$(top_srcdir)
    GLOBAL_LDFLAGS+=$(DEFAULT_LIBS)
endif

TPF_ROOT ?= /ztpf/java/bld/jvm/userfiles /zbld/svtcur/gnu/all /ztpf/commit

###
### Shared Libraries
###

ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))

## Export File
# All linux based toolchains use gcc style linker version scripts.  This
# includes xlc. The default rules create a gcc style version script.
$(MODULE_NAME)_LINKER_EXPORT_SCRIPT := $(MODULE_NAME).exp

ifeq (gcc,$(OMR_TOOLCHAIN))
# assuming a gcc environment

    GLOBAL_LDFLAGS+=-shared
    GLOBAL_LDFLAGS+=-Wl,-Map=$(MODULE_NAME).map
    GLOBAL_LDFLAGS+=-Wl,--version-script,$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
    GLOBAL_LDFLAGS+=-Wl,-soname=lib$(MODULE_NAME)$(SOLIBEXT)
    GLOBAL_LDFLAGS+=-Xlinker --disable-new-dtags

    # CTIS needs to be linked before CISO from sysroot for gettimeofday
    GLOBAL_LDFLAGS+=-Wl,-entry=0 
    GLOBAL_LDFLAGS+=-Wl,-script=$(word 1,$(wildcard $(foreach d,$(TPF_ROOT),$d/base/util/tools/tpfscript)))
    GLOBAL_LDFLAGS+=-Wl,--as-needed
    GLOBAL_LDFLAGS+=-Wl,--eh-frame-hdr
    GLOBAL_LDFLAGS+=$(foreach d,$(TPF_ROOT),-L$d/base/lib)
    GLOBAL_LDFLAGS+=$(foreach d,$(TPF_ROOT),-L$d/base/stdlib)
    GLOBAL_LDFLAGS+=$(foreach d,$(TPF_ROOT),-L$d/opensource/stdlib)
    GLOBAL_LDFLAGS+=-lgcc
    GLOBAL_LDFLAGS+=-lCTOE
    GLOBAL_LDFLAGS+=-lCTIS

endif # OMR_TOOLCHAIN is "gcc"

endif # ARTIFACT_TYPE contains "shared"


###
### Warning As Errors
###

ifeq ($(OMR_WARNINGS_AS_ERRORS),1)
    ifeq (s390,$(OMR_HOST_ARCH))
        GLOBAL_CFLAGS+=-Wimplicit -Wreturn-type -Werror
        GLOBAL_CXXFLAGS+=-Wreturn-type -Werror
    endif
endif


###
### Enhanced Warnings
###

ifeq ($(OMR_ENHANCED_WARNINGS),1)
    ifneq (ppc,$(OMR_HOST_ARCH))
        GLOBAL_CFLAGS+=-Wall
        GLOBAL_CXXFLAGS+=-Wall -Wno-non-virtual-dtor
    endif
endif

###
### Optimization Flags
###

ifeq ($(OMR_OPTIMIZE),1)
    ifeq (s390,$(OMR_HOST_ARCH))
        OPTIMIZATION_FLAGS+=-O3 -march=z10 -mtune=z9-109 -mzarch
    endif		
else
    OPTIMIZATION_FLAGS+=-O0
endif

GLOBAL_CFLAGS+=$(OPTIMIZATION_FLAGS)
GLOBAL_CXXFLAGS+=$(OPTIMIZATION_FLAGS)

###
### TPF-specific flags
###

TPF_INCLUDES := $(foreach d,$(TPF_ROOT),-I$d/base/a2e/headers)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/base/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/include46/g++)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/include46/g++/backward)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/noship/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d/opensource/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d/noship/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d)

TPF_FLAGS += -fexec-charset=ISO-8859-1 -fmessage-length=0 -funsigned-char -fverbose-asm -fno-builtin-abort -fno-builtin-exit -fno-builtin-sprintf -ffloat-store -gdwarf-2 -Wno-format-extra-args -Wno-int-to-pointer-cast -Wno-unknown-pragmas -Wno-unused-but-set-variable -Wno-write-strings
###
### Add for gcc7 builds.
###
TPF_FLAGS += -Wno-unused -fno-tree-dse -fno-optimize-strlen -fabi-version=2
TPF_FLAGS += -fno-delete-null-pointer-checks -fno-lifetime-dse -fno-optimize-strlen
TPF_C_FLAGS := -std=gnu90

GLOBAL_CFLAGS += $(TPF_FLAGS) $(TPF_INCLUDES) $(TPF_C_FLAGS) -Wa,-alshd=$*.lst
GLOBAL_CXXFLAGS += $(TPF_FLAGS) $(TPF_INCLUDES) -Wa,-alshd=$*.lst

###
###
###
# include *.d files generated by the compiler
DEPS := $(OBJECTS:$(OBJEXT)=.d)
show_deps:
	@echo "Dependencies are: $(DEPS)"

ifneq ($(DEPS),)
-include $(DEPS)
endif

