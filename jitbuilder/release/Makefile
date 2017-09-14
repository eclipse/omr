###############################################################################
# Copyright (c) 2000, 2017 IBM Corp. and others
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

# Sample makefile using g++ may need additional compile options to work in any particular environment

CXX?=g++
CXXFLAGS=-g -std=c++0x -O2 -c -fno-rtti -fPIC -I./include/compiler -I./include

OMR_ROOT_DIR=../..
JIT_BUILDER_DIR=$(OMR_ROOT_DIR)/jitbuilder

.SUFFIXES: .cpp .o

# NOTE: replay purposefully left out of these lists

# These tests may not work on all platforms
ALL_TESTS = \
            atomicoperations \
            call \
            conditionals \
            conststring \
            dotproduct \
            fieldaddress \
            issupportedtype \
            iterfib \
            linkedlist \
            localarray \
            nestedloop \
            operandarraytests \
            operandstacktests \
            pointer \
            pow2 \
            recfib \
            simple \
            structarray \
            switch \
            thunks \
            toiltype \
            transactionaloperations \
            union \
            worklist

# Compile all the tests by default
all: $(ALL_TESTS)

# These tests should run properly on all platforms
# If you add to this list, please also add to ALL_TESTS
common_goal: $(ALL_TESTS)
	./conditionals
	./issupportedtype
	./iterfib
	./nestedloop
	./pow2
	./simple
	./toiltype
	./worklist

# Additional tests that may not work properly on all platforms
# If you add to this list, please also add to ALL_TESTS
all_goal: common_goal
	./call
	./conststring
	./dotproduct
	./fieldaddress
	./linkedlist
	./localarray
	./operandarraytests
	./operandstacktests
	./pointer
	./recfib
	./structarray
	./switch
	./thunks
	./toiltype
	./union

# Tests that are still on the experimental side
# If you add to this list and want them to be built under "make", please
# also add to ALL_TESTS
experimental_goal: common_goal
	./atomicoperations
	./transactionaloperations


# In general, only compile and run tests that are known to work on all platforms
test: common_goal all

# For platforms where everything can run:
testall: all_goal all

# Even experimental stuff:
testexperimental: experimental_goal all


# Rules for individual examples

atomicoperations : libjitbuilder.a AtomicOperations.o
	$(CXX) -g -fno-rtti -o $@ AtomicOperations.o -L. -ljitbuilder -ldl

AtomicOperations.o: src/AtomicOperations.cpp src/AtomicOperations.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

badtoiltype : libjitbuilder.a BadToIlType.o
	$(CXX) -g -fno-rtti -o $@ BadToIlType.o -L. -ljitbuilder -ldl

BadToIlType.o: src/ToIlType.cpp
	$(CXX) -o $@ -DEXPECTED_FAIL $(CXXFLAGS) $<


call : libjitbuilder.a Call.o
	$(CXX) -g -fno-rtti -o $@ Call.o -L. -ljitbuilder -ldl

Call.o: src/Call.cpp src/Call.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


conditionals : libjitbuilder.a Conditionals.o	
	$(CXX) -g -fno-rtti -o $@ Conditionals.o -L. -ljitbuilder -ldl

Conditionals.o: src/Conditionals.cpp src/Conditionals.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


conststring : libjitbuilder.a ConstString.o	
	$(CXX) -g -fno-rtti -o $@ ConstString.o -L. -ljitbuilder -ldl

ConstString.o: src/ConstString.cpp src/ConstString.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


dotproduct : libjitbuilder.a DotProduct.o
	$(CXX) -g -fno-rtti -o $@ DotProduct.o -L. -ljitbuilder -ldl

DotProduct.o: src/DotProduct.cpp src/DotProduct.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


fieldaddress : libjitbuilder.a FieldAddress.o
	$(CXX) -g -fno-rtti -o $@ FieldAddress.o -L. -ljitbuilder -ldl

FieldAddress.o: src/FieldAddress.cpp src/FieldAddress.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


issupportedtype : libjitbuilder.a IsSupportedType.o
	$(CXX) -g -fno-rtti -o $@ IsSupportedType.o -L. -ljitbuilder -ldl

IsSupportedType.o: src/IsSupportedType.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<


iterfib : libjitbuilder.a IterativeFib.o
	$(CXX) -g -fno-rtti -o $@ IterativeFib.o -L. -ljitbuilder -ldl

IterativeFib.o: src/IterativeFib.cpp src/IterativeFib.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


linkedlist : libjitbuilder.a LinkedList.o
	$(CXX) -g -fno-rtti -o $@ LinkedList.o -L. -ljitbuilder -ldl

LinkedList.o: src/LinkedList.cpp src/LinkedList.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


localarray : libjitbuilder.a LocalArray.o
	$(CXX) -g -fno-rtti -o $@ LocalArray.o -L. -ljitbuilder -ldl

LocalArray.o: src/LocalArray.cpp src/LocalArray.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


mandelbrot : libjitbuilder.a Mandelbrot.o
	$(CXX) -g -fno-rtti -o $@ Mandelbrot.o -L. -ljitbuilder -ldl

Mandelbrot.o: src/Mandelbrot.cpp src/Mandelbrot.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


matmult : libjitbuilder.a MatMult.o
	$(CXX) -g -fno-rtti -o $@ MatMult.o -L. -ljitbuilder -ldl

MatMult.o: src/MatMult.cpp src/MatMult.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


nestedloop : libjitbuilder.a NestedLoop.o
	$(CXX) -g -fno-rtti -o $@ NestedLoop.o -L. -ljitbuilder -ldl

NestedLoop.o: src/NestedLoop.cpp src/NestedLoop.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


operandarraytests : libjitbuilder.a OperandArrayTests.o
	g++ -g -fno-rtti -o $@ OperandArrayTests.o -L. -ljitbuilder -ldl

OperandArrayTests.o: src/OperandArrayTests.cpp src/OperandArrayTests.hpp
	g++ -o $@ $(CXXFLAGS) $<


operandstacktests : libjitbuilder.a OperandStackTests.o
	$(CXX) -g -fno-rtti -o $@ OperandStackTests.o -L. -ljitbuilder -ldl

OperandStackTests.o: src/OperandStackTests.cpp src/OperandStackTests.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


pointer : libjitbuilder.a Pointer.o
	$(CXX) -g -fno-rtti -o $@ Pointer.o -L. -ljitbuilder -ldl

Pointer.o: src/Pointer.cpp src/Pointer.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


pow2 : libjitbuilder.a Pow2.o
	$(CXX) -g -fno-rtti -o $@ Pow2.o -L. -ljitbuilder -ldl

Pow2.o: src/Pow2.cpp src/Pow2.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


recfib : libjitbuilder.a RecursiveFib.o
	$(CXX) -g -fno-rtti -o $@ RecursiveFib.o -L. -ljitbuilder -ldl

RecursiveFib.o: src/RecursiveFib.cpp src/RecursiveFib.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


replay: libjitbuilder.a ReplayMethod.o ReplayMethodConstructor.o ReplayMethodBuildIL.o
	$(CXX) -g -fno-rtti -o $@ ReplayMethod.o ReplayMethodConstructor.o ReplayMethodBuilderIL.o -L. -ljitbuilder -ldl

ReplayMethod.o: ReplayMethod.cpp ReplayMethod.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

ReplayMethodConstructor.o: ReplayMethodConstructor.cpp ReplayMethod.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

ReplayMethodBuildIL.o: ReplayMethodBuildIL.cpp ReplayMethod.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


simple : libjitbuilder.a Simple.o
	$(CXX) -g -fno-rtti -o $@ Simple.o -L. -ljitbuilder -ldl

Simple.o: src/Simple.cpp src/Simple.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


structarray : libjitbuilder.a StructArray.o
	$(CXX) -g -fno-rtti -o $@ StructArray.o -L. -ljitbuilder -ldl

StructArray.o: src/StructArray.cpp src/StructArray.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


switch : libjitbuilder.a Switch.o
	$(CXX) -g -fno-rtti -o $@ Switch.o -L. -ljitbuilder -ldl

Switch.o: src/Switch.cpp src/Switch.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


toiltype : libjitbuilder.a ToIlType.o
	$(CXX) -g -fno-rtti -o $@ ToIlType.o -L. -ljitbuilder -ldl

ToIlType.o: src/ToIlType.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<

transactionaloperations : libjitbuilder.a TransactionalOperations.o
	$(CXX) -g -fno-rtti -o $@ TransactionalOperations.o -L. -ljitbuilder -ldl

TransactionalOperations.o: src/TransactionalOperations.cpp src/TransactionalOperations.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

union : libjitbuilder.a Union.o
	$(CXX) -g -fno-rtti -o $@ Union.o -L. -ljitbuilder -ldl

Union.o: src/Union.cpp src/Union.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

worklist : libjitbuilder.a Worklist.o
	$(CXX) -g -fno-rtti -o $@ Worklist.o -L. -ljitbuilder -ldl

Worklist.o: src/Worklist.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<


thunks : libjitbuilder.a Thunk.o
	$(CXX) -g -fno-rtti -o $@ Thunk.o -L. -ljitbuilder -ldl

Thunk.o: src/Thunk.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<

libjitbuilder.a :
	make -C $(JIT_BUILDER_DIR)

clean:
	@rm -f $(ALL_TESTS) replay *.o
