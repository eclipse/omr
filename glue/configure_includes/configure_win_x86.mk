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
  TEMP_AS:=ml64
else
  TEMP_TARGET_DATASIZE:=32
  TEMP_AS:=ml
endif

ifeq (win_x86-64_cmprssptrs_cuda, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_OPT_CUDA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86-64_cmprssptrs, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86-64_cuda, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_OPT_CUDA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86-64, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_GC_ARRAYLETS
endif

ifeq (win_x86, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_WIN32 \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_PORT_ALLOCATE_TOP_DOWN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_THR_THREE_TIER_LOCKING \
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
