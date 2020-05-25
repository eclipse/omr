###############################################################################
# Copyright (c) 2015, 2019 IBM Corp. and others
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

MODULE_NAME := omrutil
ARTIFACT_TYPE := archive

OBJECTS := $(patsubst %.cpp,%$(OBJEXT),$(wildcard *.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT),$(wildcard *.c))

ifeq (s390,$(OMR_HOST_ARCH))
  ifeq (zos,$(OMR_HOST_OS))
    ifeq (1,$(OMR_ENV_DATA64))
      OBJECTS += j9memclrz10_64$(OBJEXT)
      vpath %.s unix/zos/64
    else
      OBJECTS += omrceetbck$(OBJEXT)
      OBJECTS += j9memclrz10_31$(OBJEXT)
      vpath %.s unix/zos/31
    endif

    OBJECTS += zosversion$(OBJEXT) getstoragekey$(OBJEXT) getpsw$(OBJEXT) omrgetdsa$(OBJEXT)
    vpath %.c unix/zos
    vpath %.s unix/zos
  endif

  ifeq (linux,$(OMR_HOST_OS))
    ifeq (1,$(OMR_ENV_DATA64))
      OBJECTS += j9memclrz10_64$(OBJEXT)
      vpath %.s unix/linux/s390/64
    else
      OBJECTS += j9memclrz10_31$(OBJEXT)
      vpath %.s unix/linux/s390/31
    endif

    OBJECTS += archinfo$(OBJEXT) 
    vpath %.c unix/linux/s390
  endif
endif

ifeq (ppc,$(OMR_HOST_ARCH))
  ifeq (linux,$(OMR_HOST_OS))
    ifeq (1,$(OMR_ENV_DATA64))
      ifeq (1,$(OMR_ENV_LITTLE_ENDIAN))
        OBJECTS += gettoc$(OBJEXT)
        vpath %.s unix/linux/ppc/64le
      endif
    endif
  endif
  ifeq (0,$(OMR_ENV_DATA64))
    OBJECTS += cas8help$(OBJEXT)
    ifeq (linux,$(OMR_HOST_OS))
      vpath cas8help.s unix/linux/ppc/32
    else
      vpath cas8help.s unix/aix/32
    endif
  endif
endif

ifeq (riscv,$(OMR_HOST_ARCH))
  ifeq (linux,$(OMR_HOST_OS))
    OBJECTS += cas32helper$(OBJEXT)
    vpath cas32helper.s unix/linux/riscv/32
    ifeq (1,$(OMR_ENV_DATA64))
      OBJECTS += cas64helper$(OBJEXT)
      vpath cas64helper.s unix/linux/riscv/64
    endif
  endif
endif

ifeq (zos,$(OMR_HOST_OS))
  OBJECTS += zbarrier$(OBJEXT)
  ifeq (0,$(OMR_ENV_DATA64))
    vpath zbarrier.s unix/zos/31
  else
    vpath zbarrier.s unix/zos/64
  endif
endif

ifeq (zos,$(OMR_HOST_OS))
  ifeq (1,$(OMR_ENV_DATA64))
	OBJECTS += omrget_userExtendedPrivateAreaMemoryType$(OBJEXT)
    vpath omrget_userExtendedPrivateAreaMemoryType.s unix/zos/64
  endif
endif

ifeq (win,$(OMR_HOST_OS))
  OBJECTS += omrgetdbghelp$(OBJEXT)
  vpath %.c win
endif

include $(top_srcdir)/omrmakefiles/rules.mk

