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

LLVM_CONFIG 	:= llvm-config
CLANG 		:= g++

OMR_CHECKER_HOME := ../OMRChecker/

# This list taken from https://raw.githubusercontent.com/eliben/llvm-clang-samples/master/Makefile
# List of Clang libraries to link. The proper -L will be provided by the
# call to llvm-config
CLANG_LIBS := \
   -Wl,--start-group \
   $(shell $(LLVM_CONFIG) --libs --system-libs) \
   -lclangAST \
   -lclangAnalysis \
   -lclangBasic \
   -lclangDriver \
   -lclangEdit \
   -lclangFrontend \
   -lclangFrontendTool \
   -lclangLex \
   -lclangParse \
   -lclangSema \
   -lclangEdit \
   -lclangASTMatchers \
   -lclangRewrite \
   -lclangRewriteFrontend \
   -lclangStaticAnalyzerFrontend \
   -lclangStaticAnalyzerCheckers \
   -lclangStaticAnalyzerCore \
   -lclangSerialization \
   -lclangToolingCore \
   -lclangTooling \
   -Wl,--end-group


LDFLAGS   	:= $(shell $(LLVM_CONFIG) --ldflags )
CXXFLAGS  	:= $(shell $(LLVM_CONFIG) --cxxflags) -I$(OMR_CHECKER_HOME) -std=c++0x

PROGRAM := OMRRewriter

$(PROGRAM): $(PROGRAM).o 
	$(CLANG) -o $@ $^ $(LDFLAGS) $(CLANG_LIBS) -lpthread

$(PROGRAM).o: $(PROGRAM).cpp 
	$(CLANG) $(CXXFLAGS) -c -o $@ $^

.PHONY: clean
clean: 
	- rm $(PROGRAM).o $(PROGRAM)  
	- rm TestFile.cpp.OMRRewritten TestFile.hpp.OMRRewritten

.PHONY: test
test: $(PROGRAM)
	python test.py --rewriter $(PROGRAM)

