###############################################################################
# Copyright (c) 2015, 2016 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

# Detect 64-bit vs. 32-bit
ifneq (,$(findstring -64,$(SPEC)))
  TEMP_TARGET_DATASIZE:=64
  TEMP_AS:=ml64
else
  TEMP_TARGET_DATASIZE:=32
  TEMP_AS:=ml
endif

include $(CONFIG_INCL_DIR)/configure_common.mk

CONFIGURE_ARGS += \
  --enable-OMR_EXAMPLE \
  --enable-OMR_GC \
  --enable-OMR_PORT \
  --enable-OMR_THREAD \
  --enable-OMR_OMRSIG \
  --enable-OMR_THR_THREE_TIER_LOCKING \
  --enable-OMR_THR_SPIN_WAKE_CONTROL
  
ifeq (win_x86-64_cmprssptrs_cuda, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ARCH_X86 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_OPT_CUDA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86-64_cmprssptrs, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ARCH_X86 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86-64_cuda, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ARCH_X86 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_OPT_CUDA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86-64, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ARCH_X86 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ARCH_X86 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_GC_ARRAYLETS
endif

CONFIGURE_ARGS += libprefix= exeext=.exe solibext=.dll arlibext=.lib objext=.obj

CONFIGURE_ARGS += 'AS=$(TEMP_AS)'
CONFIGURE_ARGS += 'CC=cl'
CONFIGURE_ARGS += 'CXX=$$(CC)'
#CONFIGURE_ARGS += 'CPP=$$(CC) -EP'
CONFIGURE_ARGS += 'CCLINK=link'
CONFIGURE_ARGS += 'CXXLINK=link'
CONFIGURE_ARGS += 'AR=lib'

CONFIGURE_ARGS += 'OMR_HOST_OS=win'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=x86'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=msvc'
