###############################################################################
# Copyright (c) 2016, 2017 IBM Corp. and others
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
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

###
### Build DDR artifacts
###
### Must be invoked from the current directory.
###

top_srcdir = .
include $(top_srcdir)/omrmakefiles/configure.mk

# Ensure the source tree path is canonically-formed and ends with /
abs_srcroot = $(abspath $(TOP_SRCDIR))/

all: check-vars
	./tools/ddrgen/src/macros/getMacros.sh $(abs_srcroot)
	$(exe_output_dir)/ddrgen --filelist $(DBG_FILE_LIST) --macrolist $(abs_srcroot)macroList 

check-vars:
ifndef TOP_SRCDIR
	$(error TOP_SRCDIR is undefined)
endif
ifndef DBG_FILE_LIST
	$(error DBG_FILE_LIST is undefined)
endif

help:
	@echo "The following variables must be set:"
	@echo "TOP_SRCDIR    Path of the root of the source tree to be scanned."
	@echo "DBG_FILE_LIST Text file containing a list of files that contain"
	@echo "              debug info (e.g. DWARF info) to be scanned."
	@echo ""
	@echo "All paths must be relative to the directory containing this makefile."

.PHONY: all help check-vars
