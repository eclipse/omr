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

ifeq (linux_arm, $(SPEC))
	CONFIGURE_ARGS += \
		--host=arm-bcm2708hardfp-linux-gnueabi \
		--build=x86_64-pc-linux-gnu \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_ARM \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_TLH_PREFETCH_FTA \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
	    --enable-OMR_THR_FORK_SUPPORT \
		--enable-OMR_THR_THREE_TIER_LOCKING \
		--enable-OMR_THR_YIELD_ALG \
		--enable-OMR_GC_ARRAYLETS
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

# Customize this include path for your build environment
CONFIGURE_ARGS += 'GLOBAL_INCLUDES=/bluebird/tools/arm/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/arm-bcm2708hardfp-linux-gnueabi/include'

CONFIGURE_ARGS += 'AS=bcm2708hardfp-as'
CONFIGURE_ARGS += 'CC=bcm2708hardfp-cc'
CONFIGURE_ARGS += 'CXX=bcm2708hardfp-c++'
#CONFIGURE_ARGS += 'CPP=bcm2708hardfp-cpp -E -P'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'AR=bcm2708hardfp-ar'
CONFIGURE_ARGS += 'OBJCOPY=bcm2708hardfp-objcopy'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=arm'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
