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

MODULE_NAME := tracemerge
ARTIFACT_TYPE := cxx_executable
OBJECTS := ArgParser FileReader DATMerge FileUtils StringUtils Port main
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

vpath %.c $(top_srcdir)/tools/tracegen
vpath %.cpp $(top_srcdir)/tools/tracegen
MODULE_INCLUDES := $(top_srcdir)/tools/tracegen

include $(top_srcdir)/omrmakefiles/rules.mk
