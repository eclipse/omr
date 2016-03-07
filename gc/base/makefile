################################################################################
#
# (c) Copyright IBM Corp. 2014, 2016
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
################################################################################

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

ifeq (linux,$(OMR_HOST_OS))
  ifeq (x86,$(OMR_HOST_ARCH))
    MODULE_CFLAGS += -fpeel-loops
    MODULE_CXXFLAGS += -fpeel-loops
  endif
endif

#<flag name="/w34189" asmflag="false" definition="false">
#<include-if condition="spec.win_x86[^-]*"/>
#</flag>
include $(top_srcdir)/omrmakefiles/rules.mk
