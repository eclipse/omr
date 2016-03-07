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

###
### omr_defs.mk
###
# This file contains defines which are to be used by languages using OMR.

ifndef OMRDIR
  OMRDIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
endif

ifndef SPEC
  $(error SPEC is not defined)
endif

OMR_IPATH = \
  $(OMRDIR)/include_core \
  $(OMRDIR)/omr/startup \
  $(OMRDIR)/gc/include \
  $(OMRDIR)/gc/startup
  
OMRGC_IPATH = \
  $(OMRDIR)/gc/base \
  $(OMRDIR)/gc/base/standard \
  $(OMRDIR)/gc/verbose \
  $(OMRDIR)/gc/verbose/handler_standard \
  $(OMRDIR)/gc/stats \
  $(OMRDIR)/gc/structs

# Public Core OMR header files
OMRINCLUDECORE = $(addprefix -I,$(OMR_IPATH))

# Internal OMR header files needed by OMR glue code
OMRGLUEINCLUDE = $(addprefix -I, $(OMRGC_IPATH))
