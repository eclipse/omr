###############################################################################
# Copyright (c) 2016, 2017 IBM Corp. and others
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

.PHONY: all clean cleanobjs cleandeps cleandll
all:
clean:
cleanobjs:
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
    SUBMAKE_EXTRAARGS=PLATFORM=arm-linux-gcc
endif

SUBMAKE_EXTRAARGS+=ASSUMES=1

#
# Add JitTest to base targets and lint targets
#
all: testjit
clean: testjit_clean
cleanobjs: testjit_cleanobjs
cleandeps: testjit_cleandeps
cleandll: testjit_cleandll


#
# Add JitTest targets
#
.PHONY: testjit testjit_clean testjit_cleanobjs testjit_cleandeps testjit_cleandll

testjit:
	$(MAKE) -f testjit.mk $(SUBMAKE_EXTRAARGS)

testjit_clean:
	$(MAKE) -f testjit.mk clean $(SUBMAKE_EXTRAARGS)

testjit_cleanobjs:
	$(MAKE) -f testjit.mk cleanobjs $(SUBMAKE_EXTRAARGS)

testjit_cleandeps:
	$(MAKE) -f testjit.mk cleandeps $(SUBMAKE_EXTRAARGS)

testjit_cleandll:
	$(MAKE) -f testjit.mk cleandll $(SUBMAKE_EXTRAARGS)
