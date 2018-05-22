###############################################################################
# Copyright (c) 2015, 2018 IBM Corp. and others
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

# This makefile fragment defines logic that is common to building both shared and static libraries.

# The user of this makefile must include omrmakefiles/configure.mk, to define the environment
# variables and the buildflags.

# The location of the source code is parameterized so that this makefile fragment can be used
# to build object files in a different directory.
PORT_SRCDIR ?= ./

# This flag indicates that we are compiling the port library.
MODULE_CPPFLAGS += -DOMRPORT_LIBRARY_DEFINE

##########
## OBJECTS
OBJECTS :=

ifeq (aix,$(OMR_HOST_OS))
  OBJECTS += omrgetsp
endif

ifeq (zos,$(OMR_HOST_OS))
  # 31- and 64-bit
  OBJECTS += omrgenerate_ieat_dump
  OBJECTS += omrget_large_pageable_pages_supported
  OBJECTS += omrwto
  OBJECTS += omrpgser_release
  OBJECTS += omrgetuserid
  OBJECTS += omrsysinfo_get_number_CPUs
  OBJECTS += omrjobname
  OBJECTS += omruserid
  OBJECTS += omrzfs
  OBJECTS += omrlpdat

  ifeq (1,$(OMR_ENV_DATA64))
    # 64-bit only
    OBJECTS += omrget_large_pages_supported
    OBJECTS += omrget_large_2gb_pages_supported
    OBJECTS += omrvmem_support_above_bar
    OBJECTS += omrvmem_support_below_bar_64
    OBJECTS += omript_ttoken64
  else
    # 31-bit only
    OBJECTS += omrvmem_support_below_bar_31
  endif
else
  OBJECTS += protect_helpers
endif

OBJECTS += omrgetjobname
OBJECTS += omrgetjobid
OBJECTS += omrgetasid

ifeq ($(OMR_HOST_ARCH),$(filter $(OMR_HOST_ARCH),s390 s390x))
  # z/OS and zLinux
  OBJECTS += omrrttime
else
  ifeq (ppc,$(OMR_HOST_ARCH))
    ifeq (1,$(OMR_ENV_DATA64))
      # Linux, be and le
      ifeq (linux,$(OMR_HOST_OS))
        OBJECTS += omrrttime
      endif
    endif
  endif
endif

ifeq ($(OMR_HOST_ARCH),$(filter $(OMR_HOST_ARCH),s390 s390x))
  ifeq ($(OMR_HOST_OS),$(filter $(OMR_HOST_OS),linux linux_ztpf))
    OBJECTS += omrgetstfle
  else
    ifeq (1,$(OMR_ENV_DATA64))
      OBJECTS += omrgetstfle64
    else
      OBJECTS += omrgetstfle31
    endif
  endif
endif

ifeq (aix,$(OMR_HOST_OS))
  OBJECTS += rt_divu64
  OBJECTS += rt_time
endif

OBJECTS += omrcpu
OBJECTS += omrerror
OBJECTS += omrerrorhelpers
OBJECTS += omrexit
OBJECTS += omrfile
OBJECTS += omrfiletext
OBJECTS += omrfilestream
OBJECTS += omrfilestreamtext

ifneq (win,$(OMR_HOST_OS))
  OBJECTS += omriconvhelpers
endif

OBJECTS += omrfile_blockingasync

ifeq (win,$(OMR_HOST_OS))
  OBJECTS += omrfilehelpers
endif

ifeq (1,$(OMR_ENV_DATA64))
  OBJECTS += omrmem32helpers
endif

OBJECTS += omrheap
OBJECTS += omrmem
OBJECTS += omrmemtag
OBJECTS += omrmemcategories
OBJECTS += omrport
OBJECTS += omrmmap
OBJECTS += j9nls
OBJECTS += j9nlshelpers
OBJECTS += omrosbacktrace
OBJECTS += omrosbacktrace_impl
OBJECTS += omrintrospect
OBJECTS += omrintrospect_common
OBJECTS += omrosdump
OBJECTS += omrportcontrol
OBJECTS += omrportptb

OBJECTS += omrsignal
ifneq (win,$(OMR_HOST_OS))
  OBJECTS += omrsignal_context
endif
OBJECTS += omrsl
OBJECTS += omrstr
OBJECTS += omrsysinfo
ifeq (zos,$(OMR_HOST_OS))
  OBJECTS += omrsysinfo_helpers
endif
OBJECTS += omrsyslog
ifeq (win,$(OMR_HOST_OS))
  OBJECTS += omrsyslogmessages.res
endif
OBJECTS += omrtime
OBJECTS += omrtlshelpers
OBJECTS += omrtty
OBJECTS += omrvmem
OBJECTS += ut_omrport
OBJECTS += omrmemtag_checks
ifeq (aix,$(OMR_HOST_OS))
  OBJECTS += omrosdump_helpers
else
  ifeq ($(OMR_HOST_OS),$(filter $(OMR_HOST_OS),linux linux_ztpf))
    OBJECTS += omrosdump_helpers
  endif
  ifeq (osx,$(OMR_HOST_OS))
    OBJECTS += omrosdump_helpers
  endif
endif
ifeq (zos,$(OMR_HOST_OS))
  ifeq (0,$(OMR_ENV_DATA64))
    OBJECTS += omrsignal_ceehdlr
    OBJECTS += omrsignal_context_ceehdlr
  endif
endif

ifeq (linux_ztpf,$(OMR_HOST_OS))
    OBJECTS += omrloadfpc
endif

ifeq (ppc,$(OMR_HOST_ARCH))
  ifeq (linux,$(OMR_HOST_OS))
    OBJECTS += auxv
  endif
endif
ifeq (s390,$(OMR_HOST_ARCH))
  ifeq (linux,$(OMR_HOST_OS))
    OBJECTS += auxv
  endif
endif
ifeq (1,$(OMR_OPT_CUDA))
  OBJECTS += omrcuda
endif

# Append OBJEXT to the complete list of OBJECTS
# except for .res files for windows
OBJECTS := $(sort $(addsuffix $(OBJEXT),$(filter-out %.res,$(OBJECTS))) $(filter %.res,$(OBJECTS)))

#########
## VPATHS
ifeq (zos,$(OMR_HOST_OS))
  vpath % $(PORT_SRCDIR)zos390
  MODULE_INCLUDES += $(PORT_SRCDIR)zos390
endif

ifeq (win,$(OMR_HOST_OS))
  ifeq (1,$(OMR_ENV_DATA64))
    vpath % $(PORT_SRCDIR)win64amd
    MODULE_INCLUDES += $(PORT_SRCDIR)winamd64
  else
  endif
endif
ifeq (aix,$(OMR_HOST_OS))
  ifdef I5_VERSION
    ifeq (1,$(OMR_ENV_DATA64))
      vpath % $(PORT_SRCDIR)iseries64
      MODULE_INCLUDES += $(PORT_SRCDIR)iseries64
    endif
    vpath % $(PORT_SRCDIR)iseries
    MODULE_INCLUDES += $(PORT_SRCDIR)iseries
  endif

  ifeq (1,$(OMR_ENV_DATA64))
    vpath % $(PORT_SRCDIR)aix64
    MODULE_INCLUDES += $(PORT_SRCDIR)aix64
  else
  endif
  vpath % $(PORT_SRCDIR)aix
  MODULE_INCLUDES += $(PORT_SRCDIR)aix
endif
ifeq ($(OMR_HOST_OS),$(filter $(OMR_HOST_OS),linux linux_ztpf))
  ifeq (ppc,$(OMR_HOST_ARCH))
    ifeq (1,$(OMR_ENV_DATA64))
      ifeq (1,$(OMR_ENV_LITTLE_ENDIAN))
        vpath % $(PORT_SRCDIR)linuxppc64le
        MODULE_INCLUDES += $(PORT_SRCDIR)linuxppc64le
      endif
      vpath % $(PORT_SRCDIR)linuxppc64
      MODULE_INCLUDES += $(PORT_SRCDIR)linuxppc64
    endif
    vpath % $(PORT_SRCDIR)linuxppc
    MODULE_INCLUDES += $(PORT_SRCDIR)linuxppc
  endif

  ifeq (linux_ztpf,$(OMR_HOST_OS))
    vpath % $(PORT_SRCDIR)ztpf
    MODULE_INCLUDES += $(PORT_SRCDIR)ztpf
  endif

  ifeq ($(OMR_HOST_ARCH),$(filter $(OMR_HOST_ARCH), s390 s390x))
    ifeq (1,$(OMR_ENV_DATA64))
      vpath % $(PORT_SRCDIR)linuxs39064
      MODULE_INCLUDES += $(PORT_SRCDIR)linuxs39064
    endif
    vpath % $(PORT_SRCDIR)linuxs390
    MODULE_INCLUDES += $(PORT_SRCDIR)linuxs390
  endif

  ifeq (arm,$(OMR_HOST_ARCH))
    vpath % $(PORT_SRCDIR)linuxarm
    MODULE_INCLUDES += $(PORT_SRCDIR)linuxarm
  endif
  
  ifeq (aarch64,$(OMR_HOST_ARCH))
    vpath % $(PORT_SRCDIR)linuxaarch64
    MODULE_INCLUDES += $(PORT_SRCDIR)linuxaarch64
  endif

  ifeq (x86,$(OMR_HOST_ARCH))
    ifeq (1,$(OMR_ENV_DATA64))
      vpath % $(PORT_SRCDIR)linuxamd64
      MODULE_INCLUDES += $(PORT_SRCDIR)linuxamd64
    endif
    vpath % $(PORT_SRCDIR)linux386
    MODULE_INCLUDES += $(PORT_SRCDIR)linux386
  endif

  vpath % $(PORT_SRCDIR)linux
  MODULE_INCLUDES += $(PORT_SRCDIR)linux
endif

ifeq (osx,$(OMR_HOST_OS))
  vpath % $(PORT_SRCDIR)osx_include
  MODULE_INCLUDES += $(PORT_SRCDIR)osx_include

  vpath % $(PORT_SRCDIR)osx
  MODULE_INCLUDES += $(PORT_SRCDIR)osx
endif

ifneq (win,$(OMR_HOST_OS))
  vpath % $(PORT_SRCDIR)unix_include
  MODULE_INCLUDES += $(PORT_SRCDIR)unix_include

  vpath % $(PORT_SRCDIR)unix
  MODULE_INCLUDES += $(PORT_SRCDIR)unix
else
  vpath % $(PORT_SRCDIR)win32_include
  MODULE_INCLUDES += $(PORT_SRCDIR)win32_include

  vpath % $(PORT_SRCDIR)win32
  MODULE_INCLUDES += $(PORT_SRCDIR)win32
endif

vpath % $(PORT_SRCDIR)common
MODULE_INCLUDES += $(PORT_SRCDIR)common

vpath % $(PORT_SRCDIR)include
MODULE_INCLUDES += $(PORT_SRCDIR)include

MODULE_INCLUDES += $(PORT_SRCDIR)

###############
## DEPENDENCIES

ifeq (win,$(OMR_HOST_OS))
omrsyslog.obj: omrsyslogmessages.res
endif

ifeq (linux,$(OMR_HOST_OS))

# Linux standard headers have some functions marked with the attribute "warn_unused_result".
  ifeq (gcc,$(OMR_TOOLCHAIN))
omrfile.o: MODULE_CFLAGS+=-Wno-error
omrfiletext.o: MODULE_CFLAGS+=-Wno-error
omrintrospect.o: MODULE_CFLAGS+=-Wno-error
omrosdump.o: MODULE_CFLAGS+=-Wno-error
omrosdump_helpers.o: MODULE_CFLAGS+=-Wno-error
omrsysinfo.o: MODULE_CFLAGS+=-Wno-error
  endif

  ifeq (s390,$(OMR_HOST_ARCH))
    # OMRTODO: This is to get around a compiler bug:
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119
    MODULE_CFLAGS += -Wno-missing-braces
  endif

  ifeq (ppc,$(OMR_HOST_ARCH))
    ifeq (xlc,$(OMR_TOOLCHAIN))
# Suppress xlc warning:
# "unix/auxv.c", line 66.21: 1506-1385 (W) The attribute "noinline" is not a valid type attribute.I
auxv.o: MODULE_CFLAGS+=-qsuppress=1506-1385

# Suppress xlc warnings:
# "/usr/include/linux/kernel.h", line 42.9: 1506-312 (W) Compiler internal name __FUNCTION__ has been defined as a macro.
# "/usr/include/linux/kernel.h", line 42.9: 1506-236 (W) Macro name __FUNCTION__ has been redefined.
omrsysinfo.o: MODULE_CFLAGS+=-qsuppress=1506-236:1506-312
    endif
  endif
endif

ifeq (1,$(OMR_OPT_CUDA))
  ifeq (win,$(OMR_HOST_OS))
    # CUDA for Windows platforms
    CUDA_HOME ?= $(DEV_TOOLS)/NVIDIA/CUDA/v5.5
  else
    # CUDA for non-Windows platforms
    CUDA_HOME ?= /usr/local/cuda-5.5
  endif

omrcuda$(OBJEXT) : MODULE_INCLUDES += $(CUDA_HOME)/include
omrcuda.i : MODULE_INCLUDES += $(CUDA_HOME)/include
endif
