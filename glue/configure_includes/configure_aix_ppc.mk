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

ifeq (aix_ppc-64_cmprssptrs, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_AIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (aix_ppc-64, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_AIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (aix_ppc, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_AIX \
    --enable-OMR_ARCH_POWER \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
    --enable-OMR_THR_FORK_SUPPORT \
    --enable-OMR_GC_ARRAYLETS
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=as'
CONFIGURE_ARGS += 'CC=xlC_r'
CONFIGURE_ARGS += 'CXX=$$(CC)'
#CONFIGURE_ARGS += 'CPP=$$(CC) -P'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=ld'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKSHARED=makeC++SharedLib_r'
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=aix'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=ppc'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=xlc'
