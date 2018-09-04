###############################################################################
# Copyright (c) 2016, 2019 IBM Corp. and others
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

MODULE_NAME := omrperfgctest
ARTIFACT_TYPE := cxx_executable

# source files in this directory
SRCS := $(wildcard *.cpp)
OBJECTS := $(SRCS:%.cpp=%)

OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

MODULE_INCLUDES += ./configuration $(OMR_PUGIXML_DIR) $(OMR_GTEST_INCLUDES) ../util
MODULE_INCLUDES += \
  $(top_srcdir)/example/glue \
  $(OMR_IPATH) \
  $(OMRGC_IPATH)

MODULE_CXXFLAGS += $(OMR_GTEST_CXXFLAGS)

MODULE_STATIC_LIBS += \
  pugixml \
  testutil \
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
  omrglue

ifeq (linux,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += rt pthread
endif
ifeq (aix,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += iconv perfstat
endif
ifeq (osx,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += iconv pthread
endif
ifeq (win,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += ws2_32 shell32 Iphlpapi psapi pdh
endif

ifeq (1,$(OMRPORT_OMRSIG_SUPPORT))
  MODULE_SHARED_LIBS += omrsig
endif

include $(top_srcdir)/omrmakefiles/rules.mk
