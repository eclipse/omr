###############################################################################
# Copyright (c) 2000, 2016 IBM Corp. and others
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

# fvtest/compilertest/build Makefile

ifeq (,$(TRSRC))
    $(error Must set TRSRC)
endif

LIBJIT_TARBALL=$(TRSRC)/test/libjit/libjit.tgz
JITBUILDER_TARBALL=$(TRSRC)/test/jitbuilder/jitbuilder.tgz

.PHONY: all localclean jitbuilder prod debug clean
all: debug jitbuilder
localclean: clean

prod: jitbuilder ; $(MAKE) -C .. BUILD_CONFIG=prod JIT_DLL_DIR=$(TRSRC)/test/tests CX_FLAGS_EXTRA+='$(USERCFLAGS)'
debug: jitbuilder ; $(MAKE) -C .. BUILD_CONFIG=debug JIT_DLL_DIR=$(TRSRC)/test/tests CX_FLAGS_EXTRA+='$(USERCFLAGS)'
clean: ; $(MAKE) -C .. clean JIT_DLL_DIR=$(TRSRC)/test/tests CX_FLAGS_EXTRA+='$(USERCFLAGS)'

jitbuilder: $(JITBUILDER_TARBALL) $(LIBJIT_TARBALL)

$(JITBUILDER_TARBALL):
	mkdir -p $(dir $@)
	touch $@

$(LIBJIT_TARBALL):
	mkdir -p $(dir $@)
	touch $@
