###############################################################################
# Copyright (c) 2018, 2020 IBM Corp. and others
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
include $(CONFIG_INCL_DIR)/configure_common.mk

#target for this file
TARGET=aarch64
OS=linux

#variables for this file
TOOLCHAIN=gcc

#we expect you to export your host triplet to CHOST
CONFIGURE_ARGS += \
	--build=$(shell eval ${TOOLCHAIN} -dumpmachine) \
	--host=$(shell eval '${CHOST}-${TOOLCHAIN}' -dumpmachine) \
	--enable-OMR_ARCH_AARCH64 \
	--enable-OMR_ENV_LITTLE_ENDIAN \
	--enable-OMR_EXAMPLE \
	--enable-OMR_GC \
	--enable-OMR_GC_TLH_PREFETCH_FTA \
	--enable-OMR_PORT \
	--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
	--enable-OMR_THREAD \
	--enable-OMRTHREAD_LIB_UNIX \
	--enable-OMR_THR_FORK_SUPPORT \
	--enable-OMR_OMRSIG \
	--enable-OMR_THR_THREE_TIER_LOCKING \
	--enable-OMR_THR_YIELD_ALG \
	--enable-OMR_THR_SPIN_WAKE_CONTROL \
	--enable-OMR_JITBUILDER \
	--enable-OMR_ENV_DATA64

CONFIGURE_ARGS += \
	--disable-DDR \
	--disable-fvtest

CONFIGURE_ARGS += \
	libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS +=\
	'CC=${CHOST}-gcc' \
	'CXX=${CHOST}-g++' \
	'AR=${CHOST}-ar' \
	'AS=${CHOST}-as' \
	'LD=${CHOST}-ld' \
	'OBJCOPY=${CHOST}-objcopy' \
	'CCLINKEXE=$$(CC)' \
	'CCLINKSHARED=$$(CC)' \
	'CXXLINKEXE=$$(CXX)' \
	'CXXLINKSHARED=$$(CC)' \
	'OMR_HOST_OS=$(OS)' \
	'OMR_HOST_ARCH=$(TARGET)' \
	'OMR_TARGET_DATASIZE=64' \
	'OMR_TOOLCHAIN=$(TOOLCHAIN)' \
	'PLATFORM=$(TARGET)-$(OS)-$(TOOLCHAIN)'
