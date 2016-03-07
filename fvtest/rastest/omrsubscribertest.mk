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

MODULE_NAME := omrsubscribertest
ARTIFACT_TYPE := cxx_executable

OBJECTS := \
  main \
  rasTestHelpers \
  subscriberTest \
  ut_omr_test
ifeq (1,$(OMR_THR_FORK_SUPPORT))
OBJECTS += subscriberForkTest
endif
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

include ./rastest_include.mk

include $(top_srcdir)/omrmakefiles/rules.mk

