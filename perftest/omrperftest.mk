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

top_srcdir := .
include $(top_srcdir)/omrmakefiles/configure.mk

######################J9 Lab specific settings#################################
ifneq (,$(findstring linux_ppc,$(SPEC)))
	EXPORT_LIB=true
else
ifneq (,$(findstring linux_390,$(SPEC)))
	EXPORT_LIB=true
endif
endif

ifeq ($(EXPORT_LIB),true)
LIB_PATH:=$(SITE_PATH_STREAMROOT)/../../javatest/lib/googletest/lib/$(SPEC):$(LD_LIBRARY_PATH)
export LD_LIBRARY_PATH=$(LIB_PATH)
$(warning In $(SPEC), set LD_LIBRARY_PATH=$(LIB_PATH))
endif

######################J9 Lab specific settings#################################

all: test
	
omr_perfgctest:
	./omrgctest --gtest_filter="perfTest*" -keepVerboseLog
	./omrperfgctest

.PHONY: all test omr_perfgctest 
