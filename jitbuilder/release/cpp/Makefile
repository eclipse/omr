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
# SPDX-L$(LIBJITBUILDERDIR)cense-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

# Sample makefile using g++ may need additional compile options to work in any particular environment

CC?=gcc
CXX?=g++
# one glorious day we can remove -Wno-write-strings
CXXFLAGS=-g -std=c++0x -O0 -c -fno-rtti -fPIC -I./include -I../../../include_core -Wno-write-strings

.SUFFIXES: .cpp .o

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
            mandelbrot \
            matmult \
            nestedloop \
            operandarraytests \
            operandstacktests \
            pointer \
            pow2 \
            recfib \
            simple \
            structarray \
            switch \
            tableswitch \
            thunks \
            toiltype \
            transactionaloperations \
            union \
            vmregister \
            worklist \

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
	./mandelbrot 10000 out
	./matmult
	./operandarraytests
	./operandstacktests
	./pointer
	./recfib
	./structarray
	./switch
	./thunks
	./toiltype
	./union
	./vmregister

# Tests that are still on the experimental side
# If you add to this list and want them to be built under "make", please
# also add to ALL_TESTS
experimental_goal: useIncrement useCall
	./atomicoperations
	./transactionaloperations
	./useIncrement
	./useCall


# In general, only compile and run tests that are known to work on all platforms
test: common_goal all

# For platforms where everything can run:
testall: all_goal all

# Even experimental stuff:
testexperimental: experimental_goal all


LIBJITBUILDERDIR=.
LIBJITBUILDER=$(LIBJITBUILDERDIR)/libjitbuilder.a

%.o: %.cpp
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

SAMPLE_SRC=./samples

# Rules for individual examples

atomicoperations : $(LIBJITBUILDER) AtomicOperations.o
	$(CXX) -g -fno-rtti -o $@ AtomicOperations.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

AtomicOperations.o: $(SAMPLE_SRC)/AtomicOperations.cpp $(SAMPLE_SRC)/AtomicOperations.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

badtoiltype : $(LIBJITBUILDER) BadToIlType.o
	$(CXX) -g -fno-rtti -o $@ BadToIlType.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

BadToIlType.o: $(SAMPLE_SRC)/ToIlType.cpp
	$(CXX) -o $@ -DEXPECTED_FAIL $(CXXFLAGS) $<


call : $(LIBJITBUILDER) Call.o
	$(CXX) -g -fno-rtti -o $@ Call.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Call.o: $(SAMPLE_SRC)/Call.cpp $(SAMPLE_SRC)/Call.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


conditionals : $(LIBJITBUILDER) Conditionals.o
	$(CXX) -g -fno-rtti -o $@ Conditionals.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Conditionals.o: $(SAMPLE_SRC)/Conditionals.cpp $(SAMPLE_SRC)/Conditionals.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


conststring : $(LIBJITBUILDER) ConstString.o
	$(CXX) -g -fno-rtti -o $@ ConstString.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

ConstString.o: $(SAMPLE_SRC)/ConstString.cpp $(SAMPLE_SRC)/ConstString.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


dotproduct : $(LIBJITBUILDER) DotProduct.o
	$(CXX) -g -fno-rtti -o $@ DotProduct.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

DotProduct.o: $(SAMPLE_SRC)/DotProduct.cpp $(SAMPLE_SRC)/DotProduct.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


fieldaddress : $(LIBJITBUILDER) FieldAddress.o
	$(CXX) -g -fno-rtti -o $@ FieldAddress.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

FieldAddress.o: $(SAMPLE_SRC)/FieldAddress.cpp $(SAMPLE_SRC)/FieldAddress.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


issupportedtype : $(LIBJITBUILDER) IsSupportedType.o
	$(CXX) -g -fno-rtti -o $@ IsSupportedType.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

IsSupportedType.o: $(SAMPLE_SRC)/IsSupportedType.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<


iterfib : $(LIBJITBUILDER) IterativeFib.o
	$(CXX) -g -fno-rtti -o $@ IterativeFib.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

IterativeFib.o: $(SAMPLE_SRC)/IterativeFib.cpp $(SAMPLE_SRC)/IterativeFib.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


linkedlist : $(LIBJITBUILDER) LinkedList.o
	$(CXX) -g -fno-rtti -o $@ LinkedList.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

LinkedList.o: $(SAMPLE_SRC)/LinkedList.cpp $(SAMPLE_SRC)/LinkedList.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


localarray : $(LIBJITBUILDER) LocalArray.o
	$(CXX) -g -fno-rtti -o $@ LocalArray.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

LocalArray.o: $(SAMPLE_SRC)/LocalArray.cpp $(SAMPLE_SRC)/LocalArray.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


mandelbrot : $(LIBJITBUILDER) Mandelbrot.o
	$(CXX) -g -fno-rtti -o $@ Mandelbrot.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Mandelbrot.o: $(SAMPLE_SRC)/Mandelbrot.cpp $(SAMPLE_SRC)/Mandelbrot.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


matmult : $(LIBJITBUILDER) MatMult.o
	$(CXX) -g -fno-rtti -o $@ MatMult.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

MatMult.o: $(SAMPLE_SRC)/MatMult.cpp $(SAMPLE_SRC)/MatMult.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


nestedloop : $(LIBJITBUILDER) NestedLoop.o
	$(CXX) -g -fno-rtti -o $@ NestedLoop.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

NestedLoop.o: $(SAMPLE_SRC)/NestedLoop.cpp $(SAMPLE_SRC)/NestedLoop.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


operandarraytests : $(LIBJITBUILDER) OperandArrayTests.o
	$(CXX) -g -fno-rtti -o $@ OperandArrayTests.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

OperandArrayTests.o: $(SAMPLE_SRC)/OperandArrayTests.cpp $(SAMPLE_SRC)/OperandArrayTests.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


operandstacktests : $(LIBJITBUILDER) OperandStackTests.o
	$(CXX) -g -fno-rtti -o $@ OperandStackTests.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

OperandStackTests.o: $(SAMPLE_SRC)/OperandStackTests.cpp $(SAMPLE_SRC)/OperandStackTests.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


pointer : $(LIBJITBUILDER) Pointer.o
	$(CXX) -g -fno-rtti -o $@ Pointer.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Pointer.o: $(SAMPLE_SRC)/Pointer.cpp $(SAMPLE_SRC)/Pointer.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


pow2 : $(LIBJITBUILDER) Pow2.o
	$(CXX) -g -fno-rtti -o $@ Pow2.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Pow2.o: $(SAMPLE_SRC)/Pow2.cpp $(SAMPLE_SRC)/Pow2.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


recfib : $(LIBJITBUILDER) RecursiveFib.o
	$(CXX) -g -fno-rtti -o $@ RecursiveFib.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

RecursiveFib.o: $(SAMPLE_SRC)/RecursiveFib.cpp $(SAMPLE_SRC)/RecursiveFib.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


simple : $(LIBJITBUILDER) Simple.o
	$(CXX) -g -fno-rtti -o $@ Simple.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Simple.o: $(SAMPLE_SRC)/Simple.cpp $(SAMPLE_SRC)/Simple.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


structarray : $(LIBJITBUILDER) StructArray.o
	$(CXX) -g -fno-rtti -o $@ StructArray.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

StructArray.o: $(SAMPLE_SRC)/StructArray.cpp $(SAMPLE_SRC)/StructArray.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


switch : $(LIBJITBUILDER) Switch.o
	$(CXX) -g -fno-rtti -o $@ Switch.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Switch.o: $(SAMPLE_SRC)/Switch.cpp $(SAMPLE_SRC)/Switch.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


tableswitch : $(LIBJITBUILDER) TableSwitch.o
	$(CXX) -g -fno-rtti -o $@ TableSwitch.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

TableSwitch.o: $(SAMPLE_SRC)/TableSwitch.cpp $(SAMPLE_SRC)/TableSwitch.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<


toiltype : $(LIBJITBUILDER) ToIlType.o
	$(CXX) -g -fno-rtti -o $@ ToIlType.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

ToIlType.o: $(SAMPLE_SRC)/ToIlType.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<

transactionaloperations : $(LIBJITBUILDER) TransactionalOperations.o
	$(CXX) -g -fno-rtti -o $@ TransactionalOperations.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

TransactionalOperations.o: $(SAMPLE_SRC)/TransactionalOperations.cpp $(SAMPLE_SRC)/TransactionalOperations.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

union : $(LIBJITBUILDER) Union.o
	$(CXX) -g -fno-rtti -o $@ Union.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Union.o: $(SAMPLE_SRC)/Union.cpp $(SAMPLE_SRC)/Union.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<
	
vmregister : $(LIBJITBUILDER) VMRegister.o
	$(CXX) -g -fno-rtti -o $@ VMRegister.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

VMRegister.o: $(SAMPLE_SRC)/VMRegister.cpp $(SAMPLE_SRC)/VMRegister.hpp
	$(CXX) -o $@ $(CXXFLAGS) $<

worklist : $(LIBJITBUILDER) Worklist.o
	$(CXX) -g -fno-rtti -o $@ Worklist.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Worklist.o: $(SAMPLE_SRC)/Worklist.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<


thunks : $(LIBJITBUILDER) Thunk.o
	$(CXX) -g -fno-rtti -o $@ Thunk.o -L$(LIBJITBUILDERDIR) -ljitbuilder -ldl

Thunk.o: $(SAMPLE_SRC)/Thunk.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<


useIncrement : increment.o UseIncrement.o
	$(CC) -g -o $@ increment.o UseIncrement.o

UseIncrement.o : $(SAMPLE_SRC)/UseIncrement.c
	$(CC) -g -o $@ -c $<

increment.o : simple
	TR_Options='enableObjectFileGeneration,objectFile=increment.o' ./simple


useCall : test_calls.o UseCall.o
	$(CC) -g -o $@ test_calls.o UseCall.o

UseCall.o : $(SAMPLE_SRC)/UseCall.c
	$(CC) -g -o $@ -c $<

test_calls.o : call
	TR_Options='enableObjectFileGeneration,objectFile=test_calls.o' ./call


clean:
	@rm -f $(ALL_TESTS) useIncrement useCall *.o
