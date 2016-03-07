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

top_srcdir := ../..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := invalidAgentMissingOnLoad
ARTIFACT_TYPE := c_shared

OBJECTS := invalidAgentMissingOnLoad
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

EXPORT_FUNCTIONS_FILE := invalidAgentMissingOnLoad.exportlist

include $(top_srcdir)/omrmakefiles/rules.mk
