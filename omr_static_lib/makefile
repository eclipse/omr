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

# Create one giant archive for OMR.
top_srcdir := ..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrstatic
ARTIFACT_TYPE := archive
OBJECTS :=

# All libraries which will be combined
OMRLIBS := \
  $(top_srcdir)/util/avl \
  $(top_srcdir)/util/hashtable \
  $(top_srcdir)/util/hookable \
  $(top_srcdir)/omr \
  $(top_srcdir)/omr/startup \
  $(top_srcdir)/util/omrutil \
  $(top_srcdir)/omrtrace \
  $(top_srcdir)/util/pool \
  $(top_srcdir)/omr_glue_static_lib

ifeq (1,$(OMR_THREAD))
OMRLIBS += \
  $(top_srcdir)/thread
endif

ifeq (1,$(OMR_PORT))
OMRLIBS += \
  $(top_srcdir)/port
endif

ifeq (1,$(OMR_GC))
OMRLIBS += \
  $(top_srcdir)/gc/base \
  $(top_srcdir)/gc/base/standard \
  $(top_srcdir)/gc/startup \
  $(top_srcdir)/gc/stats \
  $(top_srcdir)/gc/structs \
  $(top_srcdir)/gc/verbose \
  $(top_srcdir)/gc/verbose/handler_standard

ifeq (1,$(OMR_GC_SEGREGATED))
OMRLIBS += $(top_srcdir)/gc/base/segregated
endif
endif

# Prepend the path to the library for relative paths.
# Absolute paths to object files must be left alone.
OBJECTS := \
$(foreach lib,$(OMRLIBS), \
  $(foreach object, \
    $(shell $(MAKE) -s --no-print-directory -C $(lib) show-objects), \
    $(if $(filter /%,$(object)),$(object),$(lib)/$(object))))

$(OBJECTS):
	@echo $@

include $(top_srcdir)/omrmakefiles/rules.mk
