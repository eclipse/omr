###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

# Detect 64-bit vs. 32-bit
ifneq (,$(findstring -64,$(SPEC)))
  TEMP_TARGET_DATASIZE:=64
else
  TEMP_TARGET_DATASIZE:=32
endif

GCC_TOOLS:=\
'OMR_TOOLCHAIN=gcc' \
'AS=as' \
'CC=gcc' \
'CXX=c++' \
'CCLINKEXE=$$(CC)' \
'CCLINKSHARED=$$(CC)' \
'CXXLINKEXE=$$(CXX)' \
'CXXLINKSHARED=$$(CC)'
# CPP is unused: 'CPP=cpp' 

XLC_TOOLS:=\
'OMR_TOOLCHAIN=xlc' \
'AS=xlC_r' \
'CC=xlC_r' \
'CXX=$$(CC)' \
'CCLINKEXE=xlc_r' \
'CCLINKSHARED=xlc_r' \
'CXXLINKEXE=$$(CC)' \
'CXXLINKSHARED=xlc_r'
# CPP is unused: 'CPP=cpp' 

# All buildspecs named "_gcc" use the same gcc tools
ifneq (,$(findstring _gcc,$(SPEC)))
TEMP_TOOLS=$(GCC_TOOLS)
endif

# not currently in builds
ifeq (linux_ppc-64_cmprssptrs_gcc, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_GCC \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (linux_ppc-64_cmprssptrs_le_gcc_cuda, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_GCC \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_OPT_CUDA \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (linux_ppc-64_cmprssptrs_le_gcc, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_GCC \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (linux_ppc-64_cmprssptrs, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS

  TEMP_TOOLS=$(XLC_TOOLS)
endif

ifeq (linux_ppc-64_le_gcc_cuda, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_GCC \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_OPT_CUDA \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (linux_ppc-64_le_gcc, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_GCC \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (linux_ppc-64, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_GCC \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS

  # This spec is the only one that uses gcc, but doesn't have "gcc" in its name.
  TEMP_TOOLS=$(GCC_TOOLS)
endif

ifeq (linux_ppc, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_NUMA_SUPPORT \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_GC_ARRAYLETS

  TEMP_TOOLS=$(XLC_TOOLS)
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

ifndef TEMP_TOOLS
  $(error No toolchain selected)
endif

CONFIGURE_ARGS += $(TEMP_TOOLS)
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=ppc'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
# OMR_TOOLCHAIN is set as part of TEMP_TOOLS above.
