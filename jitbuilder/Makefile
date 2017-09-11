###############################################################################
# Copyright (c) 2016, 2016 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

#
# "all" should be the first target to appear so it's the default
#
.phony: all clean cleandeps cleandll
all:
clean:
cleandeps:
cleandll:

# This is the logic right now for locating Clang and LLVM-config
# There's probably a nicer way to do all of this... it's pretty bad

ifeq ($(PLATFORM),amd64-linux64-clang)
    # Luckily we can just use the default path for Clang :)
endif

ifeq ($(PLATFORM),ppc64-linux64-clang)
    export CC_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang
    export CXX_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang++
endif

ifeq ($(PLATFORM),s390-zos64-vacpp)
    export CC_PATH?=/usr/lpp/cbclib/xlc/bin/xlc
    export CXX_PATH?=/usr/lpp/cbclib/xlc/bin/xlC
    export A2E_INCLUDE_PATH?=/usr/lpp/cbclib/include
endif

ifeq ($(PLATFORM),arm-linux-gcc-cross)
    export CC_PATH?=arm-bcm2708hardfp-linux-gnueabi-gcc
    export CXX_PATH?=arm-bcm2708hardfp-linux-gnueabi-g++
    export AS_PATH?=arm-bcm2708hardfp-linux-gnueabi-as
    PLATFORM=arm-linux-gcc
endif


ASSUMES=1

#
# First setup some important paths
# Personally, I feel it's best to default to out-of-tree build but who knows, there may be
# differing opinions on that.
#
JIT_SRCBASE?=..
JIT_OBJBASE?=../build
JIT_DLL_DIR?=../jitbuilder/release

#
# Now we need to cleanup these variables on Windows
#
FIXED_SRCBASE=$(subst \,/,$(JIT_SRCBASE))
FIXED_OBJBASE=$(subst \,/,$(JIT_OBJBASE))
FIXED_DLL_DIR=$(subst \,/,$(JIT_DLL_DIR))

# TODO - "debug" as default?
BUILD_CONFIG?=prod

#
# This is where we setup our component dirs
# Note these are all relative to JIT_SRCBASE and JIT_OBJBASE
# It just makes sense since source and build dirs may be in different places 
# in the filesystem :)
#
JIT_OMR_DIRTY_DIR?=compiler
JIT_PRODUCT_DIR?=jitbuilder

#
# Dirs used internally by the makefiles
#
JIT_MAKE_DIR?=$(JIT_SRCBASE)/$(JIT_PRODUCT_DIR)/build
JIT_SCRIPT_DIR?=$(JIT_SRCBASE)/tools/compiler/scripts

#
# First we set a bunch of tokens about the platform that the rest of the
# makefile will use as conditionals
#
include $(JIT_MAKE_DIR)/platform/common.mk

#
# Now we include the names of all the files that will go into building the JIT
# Will automatically include files needed from HOST and TARGET platform
#
include $(JIT_MAKE_DIR)/files/common.mk

#
# Now we configure all the tooling we will use to build the files
#
# There is quite a bit of shared stuff, but the overwhelming majority of this
# is toolchain-dependent.
#
# That makes sense - You can't expect XLC and GCC to take the same arguments
#
include $(JIT_MAKE_DIR)/toolcfg/common.mk

#
# Here's where everything has been setup and we lay down the actual targets and
# recipes that Make will use to build them
#
include $(JIT_MAKE_DIR)/rules/common.mk
