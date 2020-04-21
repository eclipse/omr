###############################################################################
# Copyright (c) 2016, 2020 IBM Corp. and others
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

top_srcdir = ../../..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := ddrgen
ARTIFACT_TYPE := cxx_executable

MODULE_STATIC_LIBS += omrstatic

# TODO libpath hack because ddrgen libs don't go into the same directory as omr libs (yet)
MODULE_LIBPATH += $(top_srcdir)/lib

MODULE_INCLUDES += \
  $(top_srcdir)/ddr/include \
  $(top_srcdir)/include_core

ifeq (linux,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += rt pthread dwarf
  ifeq (x86,$(OMR_HOST_ARCH))
    MODULE_SHARED_LIBS += elf
  endif
  ifeq (ppc,$(OMR_HOST_ARCH))
    MODULE_SHARED_LIBS += elf
  endif
  ifeq (s390,$(OMR_HOST_ARCH))
    MODULE_SHARED_LIBS += elf z
  endif
endif

ifeq (aix,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += iconv perfstat
else ifeq (osx,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += iconv
else ifeq (win,$(OMR_HOST_OS))
  MODULE_SHARED_LIBS += ws2_32 shell32 Iphlpapi psapi pdh
endif

ifeq (gcc,$(OMR_TOOLCHAIN))
  MODULE_CXXFLAGS += -frtti -D__STDC_LIMIT_MACROS -std=c++0x
  MODULE_LDFLAGS += -static-libstdc++
else ifeq (msvc,$(OMR_TOOLCHAIN))
  MODULE_CXXFLAGS += /EHsc
endif

ifeq (1,$(OMRPORT_OMRSIG_SUPPORT))
  MODULE_SHARED_LIBS += omrsig
endif

DDRGEN_STATIC_LIBS := \
  ddr-scanner \
  ddr-ir \
  ddr-blobgen \
  ddr-macros

DEPENDENCIES += $(call buildStaticLibFilename,$(DDRGEN_STATIC_LIBS))
MODULE_STATIC_LIBS += $(DDRGEN_STATIC_LIBS)

OBJECTS = \
  ddrgen$(OBJEXT)

include $(top_srcdir)/omrmakefiles/rules.mk

# This must follow the include above because we want to append to LD_STATIC_LIBS.
ifeq (zos,$(OMR_HOST_OS))
  ifeq (1,$(OMR_ENV_DATA64))
    LD_STATIC_LIBS += /usr/lpp/cbclib/lib/libelfdwarf64.x
  else
    LD_STATIC_LIBS += /usr/lpp/cbclib/lib/libelfdwarf32.x
  endif
endif
