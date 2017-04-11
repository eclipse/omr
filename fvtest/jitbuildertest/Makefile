###############################################################################
#
# (c) Copyright IBM Corp. 2017, 2017
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

MODULE_NAME := omrjitbuildertest
ARTIFACT_TYPE := cxx_executable

OBJECTS := \
	main \
	selftest \
	UnionTest \
	FieldAddressTest \
	AnonymousTest

OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

MODULE_INCLUDES += \
  ../util \
  $(top_srcdir)/jitbuilder/release/include/compiler \
  $(top_srcdir)/jitbuilder/release/include
MODULE_INCLUDES += $(OMR_GTEST_INCLUDES)
MODULE_CXXFLAGS += -std=c++0x $(OMR_GTEST_CXXFLAGS)

MODULE_STATIC_LIBS += \
  jitbuilder \
  omrGtest

MODULE_LIBPATH += \
  $(top_srcdir)/jitbuilder/release

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
  MODULE_SHARED_LIBS += ws2_32 shell32 Iphlpapi psapi pdh
endif

include $(top_srcdir)/omrmakefiles/rules.mk

