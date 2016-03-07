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
#    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
###############################################################################

top_srcdir := ..
include $(top_srcdir)/omrmakefiles/configure.mk

MODULE_NAME := omrsig
ARTIFACT_TYPE := c_shared
OBJECTS := omrsig
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))
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
ifeq (,$(findstring win,$(OMR_HOST_OS)))
ifeq (,$(findstring zos,$(OMR_HOST_OS)))
	@# !win && !zos
	@echo sigvec >>$@
endif
endif

clean: clean_exports
clean_exports:
	-$(RM) $(EXPORT_FUNCTIONS_FILE)
.PHONY: clean_exports

MODULE_STATIC_LIBS += omrutil
ifneq (,$(findstring linux,$(OMR_HOST_OS)))
    MODULE_SHARED_LIBS += pthread
endif
ifneq (,$(findstring osx,$(OMR_HOST_OS)))
    MODULE_SHARED_LIBS += pthread
endif

include $(top_srcdir)/omrmakefiles/rules.mk
