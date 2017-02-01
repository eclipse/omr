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

top_srcdir := ../..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrgcstartup
ARTIFACT_TYPE := archive

OBJECTS := $(patsubst %.cpp,%$(OBJEXT),$(wildcard *.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard *.c))

MODULE_INCLUDES += ../base ../base/standard ../include ../stats ../structs ../verbose ../verbose/handler_standard $(top_srcdir)/omr/startup $(OMRGLUE_INCLUDES)

ifeq (linux,$(OMR_HOST_OS))
  ifeq (x86,$(OMR_HOST_ARCH))
    MODULE_CFLAGS += -funroll-loops
    MODULE_CXXFLAGS += -funroll-loops
  endif
endif

#<!-- mminit and mmparse include a number of static functions which aren't all used in some specifications. Suppress warnings about these. --> 
#<flag name="/w34189" asmflag="false" definition="false">
#<include-if condition="spec.win_x86[^-]*"/>
#</flag>
#<flag name="-Wno-unused-function" asmflag="false" definition="false">
#<include-if condition="spec.linux_x86.*"/>
#</flag>
include $(top_srcdir)/omrmakefiles/rules.mk

