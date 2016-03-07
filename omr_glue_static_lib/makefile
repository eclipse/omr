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

top_srcdir := ..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrglue
ARTIFACT_TYPE := archive

OBJECTS := $(patsubst %.cpp,%$(OBJEXT), $(wildcard $(OMRGLUE)/*.cpp))
OBJECTS += $(patsubst %.c,%$(OBJEXT), $(wildcard $(OMRGLUE)/*.c))

MODULE_CPPFLAGS += $(OMRGLUE_CPPFLAGS)
MODULE_CFLAGS   += $(OMRGLUE_CFLAGS)
MODULE_CXXFLAGS += $(OMRGLUE_CXXFLAGS)
MODULE_INCLUDES += $(OMRGLUE_INCLUDES)

MODULE_INCLUDES += $(OMR_IPATH) $(OMRGC_IPATH)

# TODO fix this when the example code is moved outside of the OMR build infrastructure.
# This include path should be set in OMRGLUE_INCLUDES when we configure OMR.
ifeq (1,$(OMR_EXAMPLE))
MODULE_INCLUDES += $(top_srcdir)/fvtest/util/
endif

include $(top_srcdir)/omrmakefiles/rules.mk
