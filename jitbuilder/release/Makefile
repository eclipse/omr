################################################################################
#
# (c) Copyright IBM Corp. 2000, 2016
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
################################################################################

# Sample makefile using g++ may need additional compile options to work in any particular environment

CXXFLAGS=-g -std=c++0x -O2 -c -fno-rtti -fPIC -I./include/compiler -I./include

.SUFFIXES: .cpp .o

goal: call conststring dotproduct iterfib linkedlist localarray structarray mandelbrot nestedloop pointer recfib simple switch pow2 controlflowtests atom

all: goal

test: goal
	./call
	./conststring
	./dotproduct
	./iterfib
	./linkedlist
	./localarray
	./structarray
	./nestedloop
	./pointer
	./recfib
	./simple
	./switch
	./pow2
	./controlflowtests
	./atom

call : libjitbuilder.a Call.o
	g++ -g -fno-rtti -o $@ Call.o -L. -ljitbuilder -ldl

Call.o: src/Call.cpp src/Call.hpp
	g++ -o $@ $(CXXFLAGS) $<

conststring : libjitbuilder.a ConstString.o	
	g++ -g -fno-rtti -o $@ ConstString.o -L. -ljitbuilder -ldl

ConstString.o: src/ConstString.cpp src/ConstString.hpp
	g++ -o $@ $(CXXFLAGS) $<

dotproduct : libjitbuilder.a DotProduct.o
	g++ -g -fno-rtti -o $@ DotProduct.o -L. -ljitbuilder -ldl

DotProduct.o: src/DotProduct.cpp src/DotProduct.hpp
	g++ -o $@ $(CXXFLAGS) $<

iterfib : libjitbuilder.a IterativeFib.o
	g++ -g -fno-rtti -o $@ IterativeFib.o -L. -ljitbuilder -ldl

IterativeFib.o: src/IterativeFib.cpp src/IterativeFib.hpp
	g++ -o $@ $(CXXFLAGS) $<


linkedlist : libjitbuilder.a LinkedList.o
	g++ -g -fno-rtti -o $@ LinkedList.o -L. -ljitbuilder -ldl

LinkedList.o: src/LinkedList.cpp src/LinkedList.hpp
	g++ -o $@ $(CXXFLAGS) $<


localarray : libjitbuilder.a LocalArray.o
	g++ -g -fno-rtti -o $@ LocalArray.o -L. -ljitbuilder -ldl

LocalArray.o: src/LocalArray.cpp src/LocalArray.hpp
	g++ -o $@ $(CXXFLAGS) $<


mandelbrot : libjitbuilder.a Mandelbrot.o
	g++ -g -fno-rtti -o $@ Mandelbrot.o -L. -ljitbuilder -ldl

Mandelbrot.o: src/Mandelbrot.cpp src/Mandelbrot.hpp
	g++ -o $@ $(CXXFLAGS) $<


matmult : libjitbuilder.a MatMult.o
	g++ -g -fno-rtti -o $@ MatMult.o -L. -ljitbuilder -ldl

MatMult.o: src/MatMult.cpp src/MatMult.hpp
	g++ -o $@ $(CXXFLAGS) $<


nestedloop : libjitbuilder.a NestedLoop.o
	g++ -g -fno-rtti -o $@ NestedLoop.o -L. -ljitbuilder -ldl

NestedLoop.o: src/NestedLoop.cpp src/NestedLoop.hpp
	g++ -o $@ $(CXXFLAGS) $<


pointer : libjitbuilder.a Pointer.o
	g++ -g -fno-rtti -o $@ Pointer.o -L. -ljitbuilder -ldl

Pointer.o: src/Pointer.cpp src/Pointer.hpp
	g++ -o $@ $(CXXFLAGS) $<


recfib : libjitbuilder.a RecursiveFib.o
	g++ -g -fno-rtti -o $@ RecursiveFib.o -L. -ljitbuilder -ldl

RecursiveFib.o: src/RecursiveFib.cpp src/RecursiveFib.hpp
	g++ -o $@ $(CXXFLAGS) $<


replay: libjitbuilder.a ReplayMethod.o ReplayMethodConstructor.o ReplayMethodBuildIL.o
	g++ -g -fno-rtti -o $@ ReplayMethod.o ReplayMethodConstructor.o ReplayMethodBuilderIL.o -L. -ljitbuilder -ldl

ReplayMethod.o: ReplayMethod.cpp ReplayMethod.hpp
	g++ -o $@ $(CXXFLAGS) $<

ReplayMethodConstructor.o: ReplayMethodConstructor.cpp ReplayMethod.hpp
	g++ -o $@ $(CXXFLAGS) $<

ReplayMethodBuildIL.o: ReplayMethodBuildIL.cpp ReplayMethod.hpp
	g++ -o $@ $(CXXFLAGS) $<


simple : libjitbuilder.a Simple.o
	g++ -g -fno-rtti -o $@ Simple.o -L. -ljitbuilder -ldl

Simple.o: src/Simple.cpp src/Simple.hpp
	g++ -o $@ $(CXXFLAGS) $<

atom : libjitbuilder.a Atomic.o
	g++ -g -fno-rtti -o $@ Atomic.o -L. -ljitbuilder -ldl

Atomic.o: src/Atomic.cpp src/Atomic.hpp
	g++ -o $@ $(CXXFLAGS) $<

switch : libjitbuilder.a Switch.o
	g++ -g -fno-rtti -o $@ Switch.o -L. -ljitbuilder -ldl

Switch.o: src/Switch.cpp src/Switch.hpp
	g++ -o $@ $(CXXFLAGS) $<


pow2 : libjitbuilder.a Pow2.o
	g++ -g -fno-rtti -o $@ Pow2.o -L. -ljitbuilder -ldl

Pow2.o: src/Pow2.cpp src/Pow2.hpp
	g++ -o $@ $(CXXFLAGS) $<


structarray : libjitbuilder.a StructArray.o
	g++ -g -fno-rtti -o $@ StructArray.o -L. -ljitbuilder -ldl

StructArray.o: src/StructArray.cpp src/StructArray.hpp
	g++ -o $@ $(CXXFLAGS) $<


controlflowtests : libjitbuilder.a ControlFlowTests.o
	g++ -g -fno-rtti -o $@ ControlFlowTests.o -L. -ljitbuilder -ldl

ControlFlowTests.o: src/ControlFlowTests.cpp src/ControlFlowTests.hpp
	g++ -o $@ $(CXXFLAGS) $<



clean:
	@rm -f call conststring dotproduct iterfib linkedlist localarray structarray mandelbrot matmult nestedloop pointer recfib simple atom switch pow2 *.o
