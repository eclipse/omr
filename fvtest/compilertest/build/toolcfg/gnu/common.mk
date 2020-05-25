###############################################################################
# Copyright (c) 2016, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

#
# Explicitly set shell
#
SHELL=/bin/sh

#
# These are the prefixes and suffixes that all GNU tools use for things
#
OBJSUFF=.o
ARSUFF=.a
SOSUFF=.so
EXESUFF=
LIBPREFIX=lib
DEPSUFF=.depend.mk

#
# Paths for default programs on the platform
# Most rules will use these default programs, but they can be overwritten individually if,
# for example, you want to compile .spp files with a different C++ compiler than you use
# to compile .cpp files
#

AR_PATH?=ar
M4_PATH?=m4
SED_PATH?=sed
AR_PATH?=ar
PERL_PATH?=perl

# The default OS X `as` binary acts differently than clang's built-in
# assembler, despite identifying as the same in `as --version`.
# Notably, the `-D` flag does not seem to be supported in `as`.
ifeq ($(OS),osx)
   AS_PATH?=clang
else
   AS_PATH?=as
endif

ifeq ($(C_COMPILER),gcc)
    CC_PATH?=gcc
    CXX_PATH?=g++
endif

ifeq ($(C_COMPILER),clang)
    CC_PATH?=clang
    CXX_PATH?=clang++
endif

AS_VERSION:=$(shell $(AS_PATH) --version)
ifneq (,$(findstring LLVM,$(AS_VERSION)))
    LLVM_ASSEMBLER:=1
endif

# This is the script that's used to generate TRBuildName.cpp
GENERATE_VERSION_SCRIPT?=$(JIT_SCRIPT_DIR)/generateVersion.pl

# This is the command to check Z assembly files
ZASM_SCRIPT?=$(JIT_SCRIPT_DIR)/s390m4check.pl

#
# First setup C and C++ compilers. 
#
#     Note: "CX" means both C and C++
#

CX_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    SUPPORTS_THREAD_LOCAL \
    _LONG_LONG


CX_FLAGS+=\
    -pthread \
    -fomit-frame-pointer \
    -fasynchronous-unwind-tables \
    -Wreturn-type

CXX_FLAGS+=\
    -std=c++0x \
    -fno-rtti \
    -fno-threadsafe-statics \
    -Wno-deprecated \
    -Wno-enum-compare \
    -Wno-invalid-offsetof \
    -Wno-write-strings \
    -Wno-narrowing

CX_DEFINES_DEBUG+=DEBUG
CX_FLAGS_DEBUG+=-ggdb3

CX_DEFAULTOPT=-O3
CX_OPTFLAG?=$(CX_DEFAULTOPT)
CX_FLAGS_PROD+=$(CX_OPTFLAG)

ifeq ($(HOST_ARCH),x)
    CX_FLAGS+=-mfpmath=sse -msse -msse2 -fno-strict-aliasing -fno-math-errno -fno-rounding-math -fno-trapping-math -fno-signaling-nans
    ifeq ($(HOST_BITS),32)
        CX_FLAGS+=-m32 -fpic
    endif
    
    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=J9HAMMER
        CX_FLAGS+=-m64 -fPIC
    endif
endif

ifeq ($(HOST_ARCH),p)
    CX_DEFAULTOPT=-O2
    
    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=LINUXPPC LINUXPPC64 USING_ANSI
        CX_FLAGS+=-fpic
        CX_FLAGS_PROD+=-mcpu=powerpc64
    endif
    
    ifdef ENABLE_SIMD_LIB
        CX_DEFINES+=ENABLE_SPMD_SIMD
        CX_FLAGS+=-qaltivec -qarch=pwr7 -qtune=pwr7
    endif
endif

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=S390 FULL_ANSI S39064
        CX_FLAGS+=-fPIC -fno-strict-aliasing -mtune=z10 -march=z9-109 -mzarch
    else
        CX_DEFINES+=S390 FULL_ANSI
        CX_FLAGS+=-m31 -fPIC -fno-strict-aliasing -mtune=z10 -march=z9-109 -mzarch
        CX_FLAGS_DEBUG+=-gdwarf-2
    endif
endif

ifeq ($(C_COMPILER),clang)
    CX_FLAGS+=-Wno-parentheses -Werror=header-guard
endif

ifeq ($(BUILD_CONFIG),debug)
    CX_DEFINES+=$(CX_DEFINES_DEBUG)
    CX_FLAGS+=$(CX_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    CX_DEFINES+=$(CX_DEFINES_PROD)
    CX_FLAGS+=$(CX_FLAGS_PROD)
endif

C_CMD?=$(CC_PATH)
C_INCLUDES=$(PRODUCT_INCLUDES)
C_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(C_DEFINES_EXTRA)
C_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(C_FLAGS_EXTRA)

CXX_CMD?=$(CXX_PATH)
CXX_INCLUDES=$(PRODUCT_INCLUDES)
CXX_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(CXX_DEFINES_EXTRA)
CXX_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(CXX_FLAGS_EXTRA)

#
# Now setup GAS
#
S_CMD?=$(AS_PATH)

S_INCLUDES=$(PRODUCT_INCLUDES)
S_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)

ifeq ($(LLVM_ASSEMBLER),1)
    S_FLAGS+=-Wa,--noexecstack
else
    S_FLAGS+=--noexecstack
    S_FLAGS_DEBUG+=--gstabs
endif

S_DEFINES_DEBUG+=DEBUG

ifeq ($(HOST_ARCH),x)
    ifeq ($(HOST_BITS),32)
        S_FLAGS+=--32
    endif
    
    ifeq ($(HOST_BITS),64)
        ifeq ($(LLVM_ASSEMBLER),1)
            S_FLAGS+=-march=x86-64 -c
        else
	    S_FLAGS+=--64
        endif
    endif
endif

ifeq ($(HOST_ARCH),z)
    S_FLAGS+=-march=z9-109 -mzarch
    
    ifeq ($(HOST_BITS),32)
        S_FLAGS+=-m31
    endif
    
    ifeq ($(HOST_BITS),64)
        S_FLAGS+=-m64
    endif
endif

ifeq ($(BUILD_CONFIG),debug)
    S_DEFINES+=$(S_DEFINES_DEBUG)
    S_FLAGS+=$(S_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    S_DEFINES+=$(S_DEFINES_PROD)
    S_FLAGS+=$(S_FLAGS_PROD)
endif

S_DEFINES+=$(S_DEFINES_EXTRA)
S_FLAGS+=$(S_FLAGS_EXTRA)

#
# Setup MASM2GAS to preprocess x86 assembly files
# PASM files are first preprocessed by CPP as well
#
ifeq ($(HOST_ARCH),x)

    ASM_SCRIPT=$(JIT_SCRIPT_DIR)/masm2gas.pl

    ASM_INCLUDES=$(PRODUCT_INCLUDES)

    ifeq ($(HOST_BITS),64)
        ASM_FLAGS+=--64
    endif

    ifeq ($(BUILD_CONFIG),debug)
        ASM_FLAGS+=$(ASM_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        ASM_FLAGS+=$(ASM_FLAGS_PROD)
    endif
    
    ASM_FLAGS+=$(ASM_FLAGS_EXTRA)
    
    PASM_CMD=$(CC_PATH)
    
    PASM_INCLUDES=$(PRODUCT_INCLUDES)
    PASM_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
    
    ifeq ($(BUILD_CONFIG),debug)
        PASM_DEFINES+=$(PASM_DEFINES_DEBUG)
        PASM_FLAGS+=$(PASM_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        PASM_DEFINES+=$(PASM_DEFINES_PROD)
        PASM_FLAGS+=$(PASM_FLAGS_PROD)
    endif
    
    PASM_DEFINES+=$(PASM_DEFINES_EXTRA)
    PASM_FLAGS+=$(PASM_FLAGS_EXTRA)
endif

#
# Setup CPP and SED to preprocess PowerPC Assembly Files
# 
ifeq ($(HOST_ARCH),p)
    IPP_CMD=$(SED_PATH)
    
    ifeq ($(BUILD_CONFIG),debug)
        IPP_FLAGS+=$(IPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        IPP_FLAGS+=$(IPP_FLAGS_PROD)
    endif
    
    IPP_FLAGS+=$(IPP_FLAGS_EXTRA)
    
    SPP_CMD=$(CC_PATH)
    
    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    
    SPP_DEFINES+=$(CX_DEFINES)
    SPP_FLAGS+=$(CX_FLAGS)
    
    ifeq ($(BUILD_CONFIG),debug)
        SPP_DEFINES+=$(SPP_DEFINES_DEBUG)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_DEFINES+=$(SPP_DEFINES_PROD)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif
    
    SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif

#
# Now we setup M4 to preprocess Z assembly files
#
ifeq ($(HOST_ARCH),z)
    M4_CMD?=$(M4_PATH)

    M4_INCLUDES=$(PRODUCT_INCLUDES)
    M4_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
    
    M4_DEFINES+=$(M4_DEFINES_EXTRA)
    M4_FLAGS+=$(M4_FLAGS_EXTRA)
endif

#
# Now setup stuff for ARM assembly
#
ifeq ($(HOST_ARCH),arm)
    ARMASM_CMD?=$(SED_PATH)

    SPP_CMD?=$(CC_PATH)
    
    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    
    SPP_DEFINES+=$(CX_DEFINES)
    SPP_FLAGS+=$(CX_FLAGS)
    
    ifeq ($(BUILD_CONFIG),debug)
        SPP_DEFINES+=$(SPP_DEFINES_DEBUG)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_DEFINES+=$(SPP_DEFINES_PROD)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif
    
    SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif

#
# Now setup stuff for AARCH64 assembly
#
ifeq ($(HOST_ARCH),aarch64)
    AARCH64ASM_CMD?=$(SED_PATH)

    SPP_CMD?=$(CC_PATH)
    
    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    
    SPP_DEFINES+=$(CX_DEFINES)
    SPP_FLAGS+=$(CX_FLAGS)
    
    ifeq ($(BUILD_CONFIG),debug)
        SPP_DEFINES+=$(SPP_DEFINES_DEBUG)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_DEFINES+=$(SPP_DEFINES_PROD)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif
    
    SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif

#
# Setup the archiver
#
AR_CMD?=$(AR_PATH)

#
# Finally setup the linker
#
SOLINK_CMD?=$(CXX_PATH)

SOLINK_FLAGS+=-pthread
SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)
SOLINK_SLINK+=$(PRODUCT_SLINK) m dl

SOLINK_FLAGS_PROD+=-Wl,-S

ifeq ($(HOST_BITS),32)
    ifeq ($(HOST_ARCH),z)
        SOLINK_FLAGS+=-m31
    else
        SOLINK_FLAGS+=-m32
    endif
endif

ifeq ($(HOST_BITS),64)
    SOLINK_FLAGS+=-m64
endif

ifeq ($(HOST_ARCH),p)
    SOLINK_FLAGS+=-fpic
endif

ifeq ($(HOST_ARCH),z)
    SOLINK_FLAGS+=-fpic -Wl,-z,defs
endif

ifeq ($(BUILD_CONFIG),debug)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_PROD)
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
