###############################################################################
# Copyright (c) 2015, 2016 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code is also Distributed under one or more Secondary Licenses,
# as those terms are defined by the Eclipse Public License, v. 2.0: GNU
# General Public License, version 2 with the GNU Classpath Exception [1]
# and GNU General Public License, version 2 with the OpenJDK Assembly
# Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# Contributors:
#   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
#   James Johnston (IBM Corp.) - z/TPF does not need to link in libpthread.so
###############################################################################

top_srcdir := ..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrsig
ARTIFACT_TYPE := c_shared
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
