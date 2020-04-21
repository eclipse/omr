###############################################################################
# Copyright (c) 2015, 2019 IBM Corp. and others
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

MODULE_NAME := ddr-ir
ARTIFACT_TYPE := archive

MODULE_INCLUDES += \
  $(top_srcdir)/ddr/include \
  $(top_srcdir)/include_core

ifeq (gcc,$(OMR_TOOLCHAIN))
  MODULE_CXXFLAGS += -frtti -D__STDC_LIMIT_MACROS -std=c++0x
endif

ifeq (msvc,$(OMR_TOOLCHAIN))
  MODULE_CXXFLAGS += /EHsc
endif

OBJECTS = \
  ClassUDT$(OBJEXT) \
  ClassType$(OBJEXT) \
  Field$(OBJEXT) \
  Macro$(OBJEXT) \
  Members$(OBJEXT) \
  Modifiers$(OBJEXT) \
  NamespaceUDT$(OBJEXT) \
  EnumMember$(OBJEXT) \
  Symbol_IR$(OBJEXT) \
  EnumUDT$(OBJEXT) \
  TextFile$(OBJEXT) \
  Type$(OBJEXT) \
  TypedefUDT$(OBJEXT) \
  TypePrinter$(OBJEXT) \
  UDT$(OBJEXT) \
  UnionUDT$(OBJEXT)

include $(top_srcdir)/omrmakefiles/rules.mk
