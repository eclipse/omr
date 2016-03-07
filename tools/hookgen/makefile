###############################################################################
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
###############################################################################

top_srcdir := ../..
include $(top_srcdir)/omrmakefiles/toolconfigure.mk

MODULE_NAME := hookgen
ARTIFACT_TYPE := cxx_executable
OBJECTS := $(patsubst %.cpp,%$(OBJEXT), $(wildcard *.cpp))
OBJECTS += pugixml$(OBJEXT)

MODULE_INCLUDES += $(OMR_PUGIXML_DIR)

vpath %.cpp $(OMR_PUGIXML_DIR)
vpath %.c $(OMR_PUGIXML_DIR)

include $(top_srcdir)/omrmakefiles/rules.mk
