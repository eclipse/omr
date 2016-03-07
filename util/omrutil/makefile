################################################################################
#
# (c) Copyright IBM Corp. 2015
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
      OBJECTS += j9ceetbck$(OBJEXT)
      OBJECTS += j9memclrz10_31$(OBJEXT)
      vpath %.s unix/zos/31
    endif

    OBJECTS += zosversion$(OBJEXT) getstoragekey$(OBJEXT) getpsw$(OBJEXT) j9getdsa$(OBJEXT)
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

ifeq (zos,$(OMR_HOST_OS))
  OBJECTS += zbarrier$(OBJEXT)
  ifeq (0,$(OMR_ENV_DATA64))
    vpath zbarrier.s unix/zos/31
  else
    vpath zbarrier.s unix/zos/64
  endif
endif

ifeq (win,$(OMR_HOST_OS))
  OBJECTS += j9getdbghelp$(OBJEXT)
  vpath %.c win
endif

include $(top_srcdir)/omrmakefiles/rules.mk

