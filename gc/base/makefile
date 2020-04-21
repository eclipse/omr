###############################################################################
# Copyright (c) 2014, 2019 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

top_srcdir := ../..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrgcbase
ARTIFACT_TYPE := archive

OBJECTS := $(patsubst %.cpp,%$(OBJEXT),$(wildcard *.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard *.c))

MODULE_INCLUDES += ../include ../stats ../structs standard segregated $(OMRGLUE_INCLUDES)

ifeq (1, $(OMR_GC_SEGREGATED_HEAP))
OBJECTS += $(patsubst %.cpp,%$(OBJEXT),$(wildcard segregated/*.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard segregated/*.c))
MODULE_INCLUDES += segregated
endif

ifeq (1, $(OMR_GC_VLHGC))
OBJECTS += $(patsubst %.cpp,%$(OBJEXT),$(wildcard vlhgc/*.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard vlhgc/*.c))
MODULE_INCLUDES += vlhgc
endif

ifeq (linux,$(OMR_HOST_OS))
  ifeq (x86,$(OMR_HOST_ARCH))
    MODULE_CFLAGS += -funroll-loops
    MODULE_CXXFLAGS += -funroll-loops
  endif
endif

#<flag name="/w34189" asmflag="false" definition="false">
#<include-if condition="spec.win_x86[^-]*"/>
#</flag>
include $(top_srcdir)/omrmakefiles/rules.mk
