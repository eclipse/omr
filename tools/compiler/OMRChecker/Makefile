###############################################################################
# Copyright (c) 2015, 2016 IBM Corp. and others
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


#
# Handy macro to check to make sure variables are set
#
REQUIRE_VARS=$(foreach VAR,$(1),$(if $($(VAR)),,$(error $(VAR) must be set)))

#
# Directories for the source files and destination objects files
#
OMRCHECKER_SRCDIR ?= $(PWD)
OMRCHECKER_OBJDIR ?= $(PWD)

OMRCHECKER_OBJECT ?= $(OMRCHECKER_OBJDIR)/OMRChecker.so
CHECKER_O         := $(OMRCHECKER_OBJDIR)/OMRChecker.o
CHECKER_CPP       := $(OMRCHECKER_SRCDIR)/OMRChecker.cpp
CHECKER_HPP       := $(OMRCHECKER_SRCDIR)/OMRChecker.hpp

#
# Build variables
#
LLVM_CONFIG ?= llvm-config
CLANG       ?= clang++
CXX       	?= g++
PYTHON      ?= python2

LDFLAGS   	:= -shared `$(LLVM_CONFIG) --ldflags` 

# Clang headers are producing strict aliasing warnings swamping
# all other output. 
CXXFLAGS  	:= `$(LLVM_CONFIG) --cxxflags` -std=c++0x -Wno-strict-aliasing 

#
# Build rules
#
$(OMRCHECKER_OBJECT): $(CHECKER_O)
	$(CXX) $(LDFLAGS) -o $@ $^

$(CHECKER_O): $(CHECKER_CPP) $(CHECKER_HPP) 
	$(CXX) $(CXXFLAGS) -c -o $@ $<

#
# Rules for running tests
#
test: $(OMRCHECKER_OBJECT)
	$(PYTHON) test.py --checker $(OMRCHECKER_OBJECT)

#
# Clean up rules
#
.PHONY checker-clean:
clean:
	- rm -f $(CHECKER_O)

cleandll:
	- rm -f $(OMRCHECKER_OBJECT)

cleanall: clean cleandll

