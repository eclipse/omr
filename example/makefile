###############################################################################
# Copyright (c) 2015, 2016 IBM Corp. and others
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

# top_srcdir is the relative path to the top of the OMR source tree.
# It is used to locate 'configure.mk'.
top_srcdir := ..

# configure.mk defines build flags, tools, file extensions, and other configurable settings.
include $(top_srcdir)/omrmakefiles/configure.mk

# MODULE_NAME is the name of the executable or library to build in this makefile.
MODULE_NAME := omrgcexample

# ARTIFACT_TYPE specifies the type of library or executable to build.
# Valid values are: archive, c_executable, cxx_executable, c_shared, cxx_shared
# Refer to rules.mk for the description of the artifact types.
ARTIFACT_TYPE := cxx_executable

# SRCS is a local intermediate variable. It has no special significance to the
# makefile infrastructure. Here, it is initialized to the list of .cpp files in
# this directory.
SRCS := $(wildcard *.cpp)

# OBJECTS is a special variable that is used by the rules defined by
# rules.mk. It contains the list of object files used to build the library or
# executable.
#
# Here, OBJECTS is initialized with the list of source files in this directory,
# and the .cpp file extension is stripped from each filename.
OBJECTS := $(SRCS:%.cpp=%) main_function

# Append the object file extension (.o or .obj) to each filename.
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

vpath main_function.cpp $(top_srcdir)/util/main_function

# MODULE_INCLUDES is a special variable that contains additional include paths.
# These include paths take precedence over include paths specified by the
# makefile infrastructure.
# 
# Here, GC source code directories and the example glue directory are added
# as include paths.
MODULE_INCLUDES += \
  $(OMR_IPATH) \
  $(OMRGC_IPATH) \
  $(top_srcdir)/fvtest/util \
  $(OMR_GTEST_INCLUDES) \
  glue \
  include \
  include_extended

# OMR_GTEST_DIR is a special variable defined by configure.mk. It contains the
# location of the googletest source code.

# MODULE_STATIC_LIBS is a special variable that contains a list of static
# libraries needed to build this artifact.
MODULE_STATIC_LIBS += \
  omrglue \
  j9omr \
  omrgcbase \
  omrgcstructs \
  omrgcstats \
  omrgcstandard \
  omrgcstartup \
  j9hookstatic \
  j9prtstatic \
  j9thrstatic \
  omrgcverbose \
  omrgcverbosehandlerstandard \
  omrutil \
  j9avl \
  j9hashtable \
  j9pool \
  omrtrace \
  omrvmstartup \
  testutil

# MODULE_SHARED_LIBS is a special variable that contains a list of shared
# libraries needed to build this artifact.
ifeq (gcc,$(OMR_TOOLCHAIN))
  MODULE_SHARED_LIBS += stdc++
endif
ifeq (linux,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += rt pthread
endif
ifeq (osx,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += iconv pthread
endif
ifeq (aix,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += iconv perfstat
endif
ifeq (win,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += ws2_32 # socket library
  MODULE_SHARED_LIBS += shell32 Iphlpapi psapi pdh
endif

# rules.mk defines the rules for building object files, libraries, and executables.
include $(top_srcdir)/omrmakefiles/rules.mk
