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

