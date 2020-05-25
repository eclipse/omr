###############################################################################
# Copyright (c) 2016, 2019 IBM Corp. and others
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

top_srcdir = ../..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := ddrgentest
ARTIFACT_TYPE := cxx_executable
USE_NATIVE_ENCODING := 1

OBJECTS += $(patsubst %.cpp,%$(OBJEXT),$(wildcard *.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard *.c))
OBJECTS += $(patsubst %.cpp,%$(OBJEXT),$(wildcard test_subdir/*.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard test_subdir/*.c))
OBJECTS += $(patsubst %.cpp,%$(OBJEXT),$(wildcard map_to_type/*.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard map_to_type/*.c))

ifeq (gcc,$(OMR_TOOLCHAIN))
  MODULE_CXXFLAGS += -g3 -Wno-error -frtti -std=c++0x
else ifeq (msvc,$(OMR_TOOLCHAIN))
  MODULE_CXXFLAGS += /Zi /EHsc
else ifeq (zos,$(OMR_HOST_OS))
  # no extra flags for z/OS
else
  MODULE_CXXFLAGS += -qdbgfmt=stabstring
endif

include $(top_srcdir)/omrmakefiles/rules.mk
