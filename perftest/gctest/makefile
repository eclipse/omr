###############################################################################
#
# (c) Copyright IBM Corp. 2016
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

include $(top_srcdir)/omrmakefiles/rules.mk
