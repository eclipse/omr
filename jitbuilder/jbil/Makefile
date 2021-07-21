OMRDIR=../..
JITB=jitbuilder
LIBJITB=lib$(JITB).a
OMRBUILDDIR=$(OMRDIR)/build
LIBJITBDIR=$(OMRBUILDDIR)/jitbuilder

JITB2=jitbuilder2
LIBJITB2=lib$(JITB2).a

all: min increment matmult returns testTypeGraph complexmatmult

LINK_OPTIONS=-L. -l$(JITB2) -L$(LIBJITBDIR) -l$(JITB)

increment: Increment.o JitBuilder.hpp TextWriter.hpp $(LIBJITB2)
	g++ -g -o increment $(LINK_OPTIONS) Increment.o

min: Min.o JitBuilder.hpp TextWriter.hpp $(LIBJITB2)
	g++ -g -o min $(LINK_OPTIONS) Min.o

matmult: MatMult.o JitBuilder.hpp $(LIBJITB2)
	g++ -g -o matmult $(LINK_OPTIONS) MatMult.o

returns: Returns.o JitBuilder.hpp TextWriter.hpp $(LIBJITB2)
	g++ -g -o returns $(LINK_OPTIONS) Returns.o

testTypeGraph: testTypeGraph.o
	g++ -g -o testTypeGraph $(LINK_OPTIONS) testTypeGraph.o

$(LIBJITB2): Action.o Builder.o BuilderBase.o Case.o CodeGenerator.o Debugger.o DialectReducer.o DynamicOperation.o DynamicType.o FunctionBuilder.o LiteralValue.o Location.o Object.o Operation.o OperationBase.o OperationCloner.o OperationReplacer.o Symbol.o TextWriter.o Transformer.o Type.o TypeDictionary.o TypeGraph.o TypeReplacer.o Value.o Visitor.o JitBuilder.o
	ar -cr $(LIBJITB2) \
		Action.o \
		Builder.o \
		BuilderBase.o \
		Case.o \
		CodeGenerator.o \
		Debugger.o \
		DialectReducer.o \
		DynamicOperation.o \
		DynamicType.o \
		FunctionBuilder.o \
		LiteralValue.o \
		Location.o \
		Object.o \
		Operation.o \
		OperationBase.o \
		OperationCloner.o \
		OperationReplacer.o \
		Symbol.o \
		TextWriter.o \
		Transformer.o \
		Type.o \
		TypeDictionary.o \
		TypeGraph.o \
		TypeReplacer.o \
		Value.o \
		Visitor.o \
		JitBuilder.o

#CXXFLAGS=-O3 -g -std=c++0x -fno-rtti -fPIC -Wwritable-strings
CXXFLAGS=-O0 -g -std=c++0x -fno-rtti -fPIC -Wno-writable-strings -D_XOPEN_SOURCE=0

.cpp.o:
	g++ $(CXXFLAGS) -c $<

Action.o: Action.cpp Action.hpp
Builder.o: Builder.cpp Builder.hpp Operation.hpp Type.hpp Value.hpp
DialectReducer.o: DialectReducer.cpp DialectReducer.hpp
FunctionBuilder.o: FunctionBuilder.cpp FunctionBuilder.hpp
Location.o: Location.cpp Location.hpp
Operation.o: Operation.cpp Operation.hpp
Simulator.o: Simulator.cpp Simulator.hpp Builder.hpp FunctionBuilder.hpp Operation.hpp
Symbol.o: Symbol.cpp Symbol.hpp
TextWriter.o: TextWriter.cpp TextWriter.hpp Builder.hpp
Type.o: Type.cpp Type.hpp
TypeDictionary.o: TypeDictionary.cpp TypeDictionary.hpp
TypeGraph.o: TypeGraph.cpp TypeGraph.hpp TextWriter.hpp
Value.o: Value.cpp Value.hpp
Visitor.o: Visitor.cpp Visitor.hpp

JitBuilder.o: JitBuilder.cpp JitBuilder.hpp TextWriter.hpp
	g++ $(CXXFLAGS) -c -I. -I${OMRDIR}/jitbuilder/x/amd64 -I${OMRDIR}/jitbuilder/x -I${OMRDIR}/jitbuilder -I${OMRDIR}/compiler/x/amd64 -I${OMRDIR}/compiler/x -I${OMRDIR}/compiler -I${OMRDIR}/ -I${OMRDIR}/include_core -I${OMRBUILDDIR} $<

CodeGenerator.o: CodeGenerator.cpp CodeGenerator.hpp
	g++ $(CXXFLAGS) -c -I. -I${OMRDIR}/jitbuilder/x/amd64 -I${OMRDIR}/jitbuilder/x -I${OMRDIR}/jitbuilder -I${OMRDIR}/compiler/x/amd64 -I${OMRDIR}/compiler/x -I${OMRDIR}/compiler -I${OMRDIR}/ -I${OMRDIR}/include_core -I${OMRBUILDDIR} $<

testTypeGraph.o: testTypeGraph.cpp Action.hpp TypeDictionary.hpp TypeGraph.hpp

Increment.o: Increment.cpp

Min.o: Min.cpp

clean:
	rm -f matmult return3 *.o $(LIBJITB2)
