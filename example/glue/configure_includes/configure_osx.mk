###############################################################################
#
# (c) Copyright IBM Corp. 2016
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

TEMP_TARGET_DATASIZE:=64

include $(CONFIG_INCL_DIR)/configure_common.mk

CONFIGURE_ARGS += \
  --enable-OMR_EXAMPLE \
  --enable-OMR_GC \
  --enable-OMR_PORT \
  --enable-OMR_THREAD \
  --enable-OMR_OMRSIG \
  --enable-OMRTHREAD_LIB_UNIX \
  --enable-OMR_ENV_DATA64 \
  --enable-OMR_ENV_LITTLE_ENDIAN \
  --enable-OMR_GC_TLH_PREFETCH_FTA \
  --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
  --enable-OMR_THR_FORK_SUPPORT \
  --enable-OMR_THR_THREE_TIER_LOCKING \
  --enable-OMR_THR_YIELD_ALG \
  --enable-OMR_GC_ARRAYLETS


CONFIGURE_ARGS += libprefix=lib exeext= solibext=.dylib arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=as'
CONFIGURE_ARGS += 'CC=cc'
CONFIGURE_ARGS += 'CXX=c++'
#CONFIGURE_ARGS += 'CPP=cpp -E -P'
CONFIGURE_ARGS += 'CCLINK=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=osx'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=x86'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
