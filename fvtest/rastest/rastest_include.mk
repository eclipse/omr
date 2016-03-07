###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
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
###############################################################################

GLUE_OBJECTS := \
  argmain

GLUE_OBJECTS := $(addsuffix $(OBJEXT),$(GLUE_OBJECTS))
OBJECTS += $(GLUE_OBJECTS)

MODULE_INCLUDES += $(OMR_GTEST_INCLUDES) ../util
MODULE_INCLUDES += $(OMR_IPATH)
MODULE_CXXFLAGS += $(OMR_GTEST_CXXFLAGS)

vpath argmain.cpp $(top_srcdir)/fvtest/omrGtestGlue

MODULE_STATIC_LIBS += \
  j9prtstatic \
  j9thrstatic \
  omrutil \
  j9avl \
  j9hashtable \
  j9omr \
  j9pool \
  omrtrace \
  omrGtest \
  testutil \
  omrgcbase \
  omrgcstructs \
  omrgcstats \
  omrgcstandard \
  omrgcstartup \
  omrgcverbose \
  omrgcverbosehandlerstandard \
  omrvmstartup \
  j9hookstatic \
  omrglue

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
