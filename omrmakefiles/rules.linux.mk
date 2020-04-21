###############################################################################
# Copyright (c) 2015, 2020 IBM Corp. and others
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

###
### Helpers
###

ifeq (s390,$(OMR_HOST_ARCH))
  ifeq (0,$(OMR_ENV_DATA64))
    J9M31:=-m31
  endif
endif

## Header File Dependencies
ifeq (gcc,$(OMR_TOOLCHAIN))
  ifneq (,$(filter riscv x86,$(OMR_HOST_ARCH)))
    #-- GCC compilers support dependency generation --
    GLOBAL_CFLAGS+=-MMD -MP
    GLOBAL_CXXFLAGS+=-MMD -MP
  endif
endif

###
### Global Flags
###

GLOBAL_CPPFLAGS += -DLINUX -D_REENTRANT -D_FILE_OFFSET_BITS=64
ifeq (gcc,$(OMR_TOOLCHAIN))
  GLOBAL_CFLAGS+=-fno-strict-aliasing
  GLOBAL_CXXFLAGS+=-fno-strict-aliasing -std=c++0x
endif
ifeq (xlc,$(OMR_TOOLCHAIN))
  GLOBAL_CXXFLAGS+= -qlanglvl=extended0x
endif

ifeq (s390,$(OMR_HOST_ARCH))
  GLOBAL_CXXFLAGS+=$(J9M31)
  GLOBAL_CFLAGS+=$(J9M31)
  GLOBAL_LDFLAGS+=$(J9M31)
endif
ifeq (ppc,$(OMR_HOST_ARCH))
  ifeq (xlc,$(OMR_TOOLCHAIN))
    ifeq ($(OMR_ENV_DATA64),1)
      GLOBAL_LDFLAGS+=-q64
    endif
  endif
endif

# Compile without exceptions
ifeq (gcc,$(OMR_TOOLCHAIN))
ifeq (1,$(OMR_RTTI))
  GLOBAL_CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics
else
  GLOBAL_CXXFLAGS+=-fno-exceptions -fno-rtti -fno-threadsafe-statics
endif
else
ifeq (1,$(OMR_RTTI))
  GLOBAL_CXXFLAGS+=-qrtti -qnoeh
else
  GLOBAL_CXXFLAGS+=-qnortti -qnoeh
endif
endif

## Position Independent compile flag
ifeq (gcc,$(OMR_TOOLCHAIN))
  ifeq (ppc,$(OMR_HOST_ARCH))
    # Used for GOT's under 4k, should we just go -fPIC for everyone?
    GLOBAL_CFLAGS+=-fpic
    GLOBAL_CXXFLAGS+=-fpic
  else
    ifeq (x86,$(OMR_HOST_ARCH))
      ifeq (1,$(OMR_ENV_DATA64))
        GLOBAL_CFLAGS+=-fPIC
        GLOBAL_CXXFLAGS+=-fPIC
      else
        GLOBAL_CFLAGS+=-fpic
        GLOBAL_CXXFLAGS+=-fpic
      endif
    else
      GLOBAL_CFLAGS+=-fPIC
      GLOBAL_CXXFLAGS+=-fPIC
    endif
  endif
else
  GLOBAL_CFLAGS+=-qpic=large
  GLOBAL_CXXFLAGS+=-qpic=large
endif

## ASFLAGS
# xlc on ppc does not actually understand this
# option, it is silently ignored.
GLOBAL_ASFLAGS+=-noexecstack

ARM_ARCH_FLAGS=-march=armv6 -marm -mfpu=vfp -mfloat-abi=hard

ifeq (ppc,$(OMR_HOST_ARCH))
  ifeq (gcc,$(OMR_TOOLCHAIN))
    ifeq (1,$(OMR_ENV_DATA64))
      GLOBAL_ASFLAGS+=-a64 -mppc64
    else
      GLOBAL_ASFLAGS+=-a32 -mppc64
    endif
  else
    GLOBAL_ASFLAGS+=-c -o $*.o -qpic=large
    ifeq (1,$(OMR_ENV_DATA64))
      GLOBAL_ASFLAGS+=-q64
      ifeq ($(OMR_ENV_LITTLE_ENDIAN),1)
        GLOBAL_ASFLAGS+=-qarch=pwr7
      else
        GLOBAL_ASFLAGS+=-qarch=ppc64
      endif
    else
        GLOBAL_ASFLAGS+=-qarch=ppc
    endif
  endif
else
  ifeq (s390,$(OMR_HOST_ARCH))
    ifeq (0,$(OMR_ENV_DATA64))
      GLOBAL_ASFLAGS+= -mzarch
    endif
    GLOBAL_ASFLAGS+= -march=z9-109 $(J9M31) -o $*.o
  else
    ifeq (x86,$(OMR_HOST_ARCH))
      ifeq (0,$(OMR_ENV_DATA64))
        GLOBAL_ASFLAGS+=-32
      else
        GLOBAL_ASFLAGS+=-64
      endif
    else
      ifeq (arm,$(OMR_HOST_ARCH))
        GLOBAL_ASFLAGS+=$(ARM_ARCH_FLAGS)
      else ifeq (aarch64,$(OMR_HOST_ARCH))
        GLOBAL_ASFLAGS+=-march=armv8-a+simd
      else
        # Nothing
      endif
    endif
  endif
endif

###
### Platform Flags
###

## Debugging Information
# Indicate that GNU debug symbols are being used
ifeq (gcc,$(OMR_TOOLCHAIN))
  ifneq (,$(filter aarch64 arm ppc riscv s390 x86,$(OMR_HOST_ARCH)))
    USE_GNU_DEBUG:=1
  endif
endif

ifeq (1,$(OMR_DEBUG))
  ifeq (1,$(USE_GNU_DEBUG))
    GLOBAL_ASFLAGS+=-ggdb
    GLOBAL_CXXFLAGS+=-ggdb
    GLOBAL_CFLAGS+=-ggdb
    GLOBAL_LDFLAGS+=-ggdb
  else
    GLOBAL_ASFLAGS+=-g
    GLOBAL_CXXFLAGS+=-g
    GLOBAL_CFLAGS+=-g
    GLOBAL_LDFLAGS+=-g
  endif
endif

#-- Add Platform flags
ifeq (x86,$(OMR_HOST_ARCH))
  ifeq (1,$(OMR_ENV_DATA64))
    GLOBAL_CFLAGS+= -m64
    GLOBAL_CXXFLAGS+=-m64
    GLOBAL_CPPFLAGS+=-DJ9HAMMER
  else
    GLOBAL_CFLAGS+=-m32 -msse2
    GLOBAL_CXXFLAGS+=-m32 -msse2 -I/usr/include/nptl
    GLOBAL_CPPFLAGS+=-DJ9X86
  endif

else ifeq (riscv,$(OMR_HOST_ARCH))
  GLOBAL_CFLAGS  +=-DRISCV
  GLOBAL_CXXFLAGS+=-DRISCV
  GLOBAL_CPPFLAGS+=-DRISCV
  ifeq (1,$(OMR_ENV_DATA64))
    GLOBAL_CFLAGS  +=-DRISCV64
    GLOBAL_CXXFLAGS+=-DRISCV64
    GLOBAL_CPPFLAGS+=-DRISCV64
  else
    GLOBAL_CFLAGS  +=-DRISCV32
    GLOBAL_CXXFLAGS+=-DRISCV32
    GLOBAL_CPPFLAGS+=-DRISCV32
  endif
else ifeq (aarch64,$(OMR_HOST_ARCH))
  GLOBAL_CFLAGS+=-march=armv8-a+simd -Wno-unused-but-set-variable
  GLOBAL_CXXFLAGS+=-march=armv8-a+simd -Wno-unused-but-set-variable
  GLOBAL_CPPFLAGS+=-DJ9AARCH64 -DAARCH64GNU -DAARCH64 -DFIXUP_UNALIGNED -Wno-unused-but-set-variable
else
  ifeq (arm,$(OMR_HOST_ARCH))
    GLOBAL_CFLAGS+=$(ARM_ARCH_FLAGS) -Wno-unused-but-set-variable
    GLOBAL_CPPFLAGS+=-DJ9ARM -DARMGNU -DARM -DFIXUP_UNALIGNED $(ARM_ARCH_FLAGS) -Wno-unused-but-set-variable
  else
    ifeq (ppc,$(OMR_HOST_ARCH))
      GLOBAL_CPPFLAGS+=-DLINUXPPC

      ifeq (gcc,$(OMR_TOOLCHAIN))
        ifeq (1,$(OMR_ENV_DATA64))
          GLOBAL_CFLAGS+=-m64
          GLOBAL_CXXFLAGS+=-m64
          GLOBAL_CPPFLAGS+=-DLINUXPPC64 -DPPC64
        else
          GLOBAL_CFLAGS+=-m32
          GLOBAL_CXXFLAGS+=-m32
        endif
      else
        GLOBAL_CFLAGS+=-qalias=noansi -qxflag=LTOL:LTOL0 -qxflag=selinux
        GLOBAL_CXXFLAGS+=-qalias=noansi -qxflag=LTOL:LTOL0 -qxflag=selinux -qsuppress=1540-1087:1540-1088:1540-1090
        ifeq (1,$(OMR_ENV_DATA64))
          ifeq ($(OMR_ENV_LITTLE_ENDIAN),1)
            GLOBAL_CFLAGS+=-qarch=pwr7
            GLOBAL_CXXFLAGS+=-qarch=pwr7
          else
            GLOBAL_CFLAGS+=-qarch=ppc64
            GLOBAL_CXXFLAGS+=-qarch=ppc64
          endif # endian
          GLOBAL_CFLAGS+=-q64
          GLOBAL_CXXFLAGS+=-q64
          GLOBAL_CPPFLAGS+=-DLINUXPPC64 -DPPC64
        else
          GLOBAL_CFLAGS+=-qarch=ppc
          GLOBAL_CXXFLAGS+=-qarch=ppc
        endif # proc size
      endif # cc type
    else
      ifeq (s390,$(OMR_HOST_ARCH))
        GLOBAL_CFLAGS+=$(J9M31) -fno-strict-aliasing
        GLOBAL_CXXFLAGS+=$(J9M31) -fno-strict-aliasing
        GLOBAL_CPPFLAGS+=-DS390 -D_LONG_LONG
        ifeq (1,$(OMR_ENV_DATA64))
          GLOBAL_CPPFLAGS+=-DS39064
        endif
      endif
    endif
  endif
endif

ifneq (,$(findstring executable,$(ARTIFACT_TYPE)))
  ifeq (x86,$(OMR_HOST_ARCH))
    ifeq (1,$(OMR_ENV_DATA64))
    else
      GLOBAL_LDFLAGS+=-m32
    endif
  endif

  ## Default Libraries
  DEFAULT_LIBS:=-lm -lpthread -lc -lrt -ldl -lutil -Wl,-z,origin,-rpath,\$$ORIGIN,--disable-new-dtags,-rpath-link,$(top_srcdir)
  GLOBAL_LDFLAGS+=$(DEFAULT_LIBS)
endif

###
### Shared Libraries
###

ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))

  ## Export File
  # All linux based toolchains use gcc style linker version scripts.  This
  # includes xlc. The default rules create a gcc style version script.
  $(MODULE_NAME)_LINKER_EXPORT_SCRIPT := $(MODULE_NAME).exp

  ifeq (xlc,$(OMR_TOOLCHAIN))
    ifeq ($(OMR_ENV_DATA64),1)
      GLOBAL_LDFLAGS+=-q64
    endif
    GLOBAL_LDFLAGS+=-qmkshrobj -qxflag=selinux -Wl,-Map=$(MODULE_NAME).map
    GLOBAL_LDFLAGS+=-Wl,-soname=$(LIBPREFIX)$(MODULE_NAME)$(SOLIBEXT),--version-script=$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)

    # UMA_DLL_LINK_POSTFLAGS+=-Wl,--start-group $(UMA_LINK_STATIC_LIBRARIES) -Wl,--end-group
    # UMA_DLL_LINK_POSTFLAGS+=$(UMA_LINK_SHARED_LIBRARIES)
    GLOBAL_LDFLAGS+=-lm -lpthread -lc -Wl,-z,origin,-rpath,\$$ORIGIN,--disable-new-dtags,-rpath-link,$(exe_output_dir)

  else
  # assuming a gcc environment

    GLOBAL_LDFLAGS+=-shared
    GLOBAL_LDFLAGS+=-Wl,-Map=$(MODULE_NAME).map
    GLOBAL_LDFLAGS+=-Wl,--version-script,$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
    GLOBAL_LDFLAGS+=-Wl,-soname=lib$(MODULE_NAME)$(SOLIBEXT)
    GLOBAL_LDFLAGS+=-Xlinker -z -Xlinker origin -Xlinker -rpath -Xlinker \$$ORIGIN -Xlinker --disable-new-dtags

    ifeq (s390,$(OMR_HOST_ARCH))
      GLOBAL_LDFLAGS+=-Xlinker -rpath-link -Xlinker $(exe_output_dir)
    endif
    ifeq ($(OMR_HOST_ARCH),x86)
      ifneq ($(OMR_ENV_DATA64),1)
        GLOBAL_LDFLAGS+=-m32
      else
        GLOBAL_LDFLAGS+=-m64
      endif
    endif

    ifeq ($(OMR_HOST_ARCH),ppc)
      ifeq ($(OMR_ENV_DATA64),1)
        GLOBAL_LDFLAGS+=-m64
      else
        GLOBAL_LDFLAGS+=-m32
      endif
    endif

    ifneq (,$(filter riscv x86,$(OMR_HOST_ARCH)))
      ifneq ($(OMR_ENV_DATA64),1)
        GLOBAL_LDFLAGS+=-lc -lm -ldl
      else
        ifneq (,$(findstring CXX,$(ARTIFACT_TYPE)))
          GLOBAL_LDFLAGS+=-lc
        endif
        GLOBAL_LDFLAGS+=-lm
      endif
    endif
  endif # OMR_TOOLCHAIN is not "xlc"

endif # ARTIFACT_TYPE contains "shared"

###
### Warning As Errors
###

ifeq ($(OMR_WARNINGS_AS_ERRORS),1)
  ifeq (ppc,$(OMR_HOST_ARCH))
    ifeq (gcc,$(OMR_TOOLCHAIN))
      GLOBAL_CFLAGS += -Wreturn-type -Werror
      GLOBAL_CXXFLAGS += -Wreturn-type -Werror
    else
      GLOBAL_CFLAGS += -qhalt=w
      GLOBAL_CXXFLAGS += -qhalt=w
    endif
  else
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
  ifeq (x86,$(OMR_HOST_ARCH))
    ifeq ($(OMR_ENV_DATA64),1)
      OPTIMIZATION_FLAGS+=-O3 -fno-strict-aliasing
    else
      OPTIMIZATION_FLAGS+=-O3 -fno-strict-aliasing -march=pentium4 -mtune=prescott
    endif
  else
    ifeq (arm,$(OMR_HOST_ARCH))
      OPTIMIZATION_FLAGS+=-O3 -fno-strict-aliasing
    else ifeq (aarch64,$(OMR_HOST_ARCH))
      # TODO:AARCH64 Do not optimize just yet. we need debugging support until we are assured this run
      OPTIMIZATION_FLAGS+=-O3 -fno-strict-aliasing
    else
      ifeq (ppc,$(OMR_HOST_ARCH))
        OPTIMIZATION_FLAGS+=-O3
        ifeq ($(OMR_ENV_LITTLE_ENDIAN),1)
          OPTIMIZATION_FLAGS+=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
          ifeq (xlc,$(OMR_TOOLCHAIN))
            OPTIMIZATION_FLAGS+=-qsimd=noauto
          endif
        endif
      else
        ifeq (s390,$(OMR_HOST_ARCH))
          OPTIMIZATION_FLAGS+=-O3 -mtune=z10 -march=z9-109 -mzarch
        else ifeq (riscv,$(OMR_HOST_ARCH))
          OPTIMIZATION_FLAGS+=-O3 -fno-strict-aliasing
        else
          OPTIMIZATION_FLAGS+=-O
        endif
      endif
    endif
  endif
else
  OPTIMIZATION_FLAGS+=-O0
endif
GLOBAL_CFLAGS+=$(OPTIMIZATION_FLAGS)
GLOBAL_CXXFLAGS+=$(OPTIMIZATION_FLAGS)

# Override the default recipe if we are using USE_GNU_DEBUG, so that we strip out the
# symbols and store them seperately.
ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
  ifeq (1,$(OMR_DEBUG))
    ifeq (1,$(USE_GNU_DEBUG))

      define LINK_C_SHARED_COMMAND
        $(CCLINKSHARED) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
        $(OBJCOPY) --only-keep-debug $@ $(@:$(SOLIBEXT)=.debuginfo)
        $(OBJCOPY) --strip-debug $@
        $(OBJCOPY) --add-gnu-debuglink=$(@:$(SOLIBEXT)=.debuginfo) $@
      endef

      define LINK_CXX_SHARED_COMMAND
        $(CXXLINKSHARED) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
        $(OBJCOPY) --only-keep-debug $@ $(@:$(SOLIBEXT)=.debuginfo)
        $(OBJCOPY) --strip-debug $@
        $(OBJCOPY) --add-gnu-debuglink=$(@:$(SOLIBEXT)=.debuginfo) $@
      endef

      ## Files to clean
      CLEAN_FILES=$(OBJECTS) $(OBJECTS:$(OBJEXT)=.i) *.d
      CLEAN_FILES+=$($(MODULE_NAME)_shared:$(SOLIBEXT)=.debuginfo) $(MODULE_NAME).map
      define CLEAN_COMMAND
        -$(RM) $(CLEAN_FILES)
      endef

    endif # USE_GNU_DEBUG
  endif # OMR_DEBUG
endif # ARTIFACT_TYPE contains "shared"

###
###
###
# include *.d files generated by the compiler
DEPS := $(OBJECTS:$(OBJEXT)=.d)
show_deps :
	@echo "Dependencies are: $(DEPS)"

ifneq ($(DEPS),)
  -include $(DEPS)
endif
