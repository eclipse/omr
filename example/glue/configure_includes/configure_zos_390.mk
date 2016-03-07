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

# Detect 64-bit vs. 31-bit
ifneq (,$(findstring -64,$(SPEC)))
  TEMP_TARGET_DATASIZE:=64
else
  TEMP_TARGET_DATASIZE:=31
endif

include $(CONFIG_INCL_DIR)/configure_common.mk

CONFIGURE_ARGS += \
  --enable-OMR_THR_THREE_TIER_LOCKING

ifeq (zos_390-64_cmprssptrs, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_ZOS \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_GC_COMPRESSED_POINTERS \
    --enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
    --enable-OMR_INTERP_SMALL_MONITOR_SLOT \
    --enable-OMR_GC_ARRAYLETS \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390-64, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_ZOS \
    --enable-OMR_ENV_DATA64 \
    --enable-OMR_GC_ARRAYLETS \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390, $(SPEC))
  CONFIGURE_ARGS += \
    --enable-OMRTHREAD_LIB_ZOS \
    --enable-OMR_PORT_ZOS_CEEHDLRSUPPORT \
    --enable-OMR_GC_ARRAYLETS \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=c89'
CONFIGURE_ARGS += 'CC=c89'
CONFIGURE_ARGS += 'CXX=cxx'
#CONFIGURE_ARGS += 'CPP=$$(CC) -P'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=cc'
CONFIGURE_ARGS += 'CXXLINKEXE=cxx' # plus additional flags set by makefile
CONFIGURE_ARGS += 'CXXLINKSHARED=cxx' # plus additional flags set by makefile
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=zos'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=s390'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=xlc'

