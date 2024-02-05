###############################################################################
# Copyright IBM Corp. and others 2015
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
# MAKEFLAGS is cleared below because it might contain '-w' which seems to
# nullify the use of '--no-print-directory', or job-server context which
# yields warnings that should not be mixed into the 'show-objects' output.
OBJECTS := \
$(foreach lib,$(OMRLIBS), \
  $(foreach object, \
    $(shell MAKEFLAGS= $(MAKE) -s --no-print-directory -C $(lib) show-objects), \
    $(if $(filter /%,$(object)),$(object),$(lib)/$(object))))

include $(top_srcdir)/omrmakefiles/rules.mk
