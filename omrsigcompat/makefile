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

top_srcdir := ..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrsig
ARTIFACT_TYPE := cxx_shared
OBJECTS := omrsig
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))
ifeq (win,$(OMR_HOST_OS))
	OBJECTS += $(top_srcdir)/omr.res
endif
EXPORT_FUNCTIONS_FILE := omrsig.exportlist

## Create platform specific exports

$(EXPORT_FUNCTIONS_FILE):
	@# all
	@echo omrsig_primary_signal >>$@
	@echo omrsig_handler >>$@
	@echo signal >>$@
ifeq (,$(findstring win,$(OMR_HOST_OS)))
	@# !windows
	@echo omrsig_primary_sigaction >>$@
	@echo sigaction >>$@
	@echo sigset >>$@
	@echo sigignore >>$@
	@echo bsd_signal >>$@
	@echo sysv_signal >>$@
endif
ifneq (,$(findstring linux,$(OMR_HOST_OS)))
	@# linux
	@echo __sysv_signal >>$@
	@echo ssignal >>$@
endif
ifneq (,$(findstring osx,$(OMR_HOST_OS)))
	@# osx
	@echo __sysv_signal >>$@
	@echo ssignal >>$@
endif
ifneq (,$(findstring zos,$(OMR_HOST_OS)))
	@# z/os
	@echo __sigactionset >>$@
endif

clean: clean_exports
clean_exports:
	-$(RM) $(EXPORT_FUNCTIONS_FILE)
.PHONY: clean_exports

MODULE_STATIC_LIBS += omrutil
ifneq (,$(findstring linux,$(OMR_HOST_OS)))
  ifeq (,$(findstring ztpf,$(OMR_HOST_OS)))
    MODULE_SHARED_LIBS += pthread
  endif
endif
ifneq (,$(findstring osx,$(OMR_HOST_OS)))
    MODULE_SHARED_LIBS += pthread
endif

include $(top_srcdir)/omrmakefiles/rules.mk
