###############################################################################
# Copyright (c) 2015, 2016 IBM Corp. and others
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

top_srcdir := ..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrglue
ARTIFACT_TYPE := archive

OBJECTS := $(patsubst %.cpp,%$(OBJEXT), $(wildcard $(OMRGLUE)/*.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT), $(wildcard $(OMRGLUE)/*.c))

MODULE_CPPFLAGS += $(OMRGLUE_CPPFLAGS)
MODULE_CFLAGS   += $(OMRGLUE_CFLAGS)
MODULE_CXXFLAGS += $(OMRGLUE_CXXFLAGS)
MODULE_INCLUDES += $(OMRGLUE_INCLUDES)

MODULE_INCLUDES += $(OMR_IPATH) $(OMRGC_IPATH)

# TODO fix this when the example code is moved outside of the OMR build infrastructure.
# This include path should be set in OMRGLUE_INCLUDES when we configure OMR.
ifeq (1,$(OMR_EXAMPLE))
MODULE_INCLUDES += $(top_srcdir)/fvtest/util/
endif

include $(top_srcdir)/omrmakefiles/rules.mk
