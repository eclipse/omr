###############################################################################
# Copyright (c) 2015, 2015 IBM Corp. and others
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
include $(top_srcdir)/tools/toolconfigure.mk

MODULE_NAME := hookgen
ARTIFACT_TYPE := cxx_executable
USE_NATIVE_ENCODING := 1
OBJECTS := $(patsubst %.cpp,%$(OBJEXT), $(wildcard *.cpp))
OBJECTS += pugixml$(OBJEXT)

MODULE_INCLUDES += $(OMR_PUGIXML_DIR)
ifeq ($(I5_VERSION),I5_V7R3)
  # __setjmp supported on pase 7.3, but doesn't support on AIX 7.1.1 which we run the build.
  MODULE_LIBPATH += /osxpf/v7r2m0.cuep/bld/cmvc/pase.pgm/p5.cuep/lib
endif

vpath %.cpp $(OMR_PUGIXML_DIR)
vpath %.c $(OMR_PUGIXML_DIR)

include $(top_srcdir)/omrmakefiles/rules.mk
