/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef BUILDERTEST_INCL
#define BUILDERTEST_INCL

#include "TestDriver.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"

namespace TestCompiler
{

class BuilderTest;

typedef int32_t (RecursiveFibFunctionType)(int32_t);
typedef int32_t (IterativeFibFunctionType)(int32_t);
typedef int32_t (DoWhileFibFunctionType)(int32_t);
typedef int32_t (DoWhileFunctionType)(int32_t);
typedef int32_t (WhileDoFibFunctionType)(int32_t);
typedef int32_t (WhileDoFunctionType)(int32_t);
typedef int32_t (BasicForLoopFunctionType)(int32_t, int32_t, int32_t);
typedef int32_t (signatureCharI_I_testMethodType)(int32_t);
typedef int32_t (IfFunctionType)(int32_t, int32_t);
typedef int64_t (IfLongFunctionType)(int64_t, int64_t);
typedef int32_t (LoopIfThenElseFunctionType)(int32_t, int32_t, int32_t, int32_t);
typedef int32_t (ForLoopContinueFunctionType)(int32_t);
typedef int32_t (ForLoopBreakFunctionType)(int32_t);
typedef int32_t (ForLoopBreakAndContinueFunctionType)(int32_t);

class BuilderTest : public TestDriver
   {
   public:
   bool *traceEnabledLocation()    { return &_traceEnabled; }
   virtual void compileControlFlowTestMethods();
   virtual void compileNestedControlFlowLoopTestMethods();
   virtual void invokeControlFlowTests();
   virtual void invokeNestedControlFlowLoopTests();

   protected:
   virtual void compileTestMethods();
   virtual void invokeTests();

   private:
   int32_t recursiveFib(int32_t n);
   int32_t iterativeFib(int32_t n);
   int32_t doWhileFib(int32_t n);
   int32_t doWhileWithBreak(int32_t n);
   int32_t doWhileWithContinue(int32_t n);
   int32_t doWhileWithBreakAndContinue(int32_t n);
   int32_t whileDoFib(int32_t n);
   int32_t whileDoWithBreak(int32_t n);
   int32_t whileDoWithContinue(int32_t n);
   int32_t whileDoWithBreakAndContinue(int32_t n);
   int32_t basicForLoop(int32_t initial, int32_t final, int32_t bump, bool countsUp);
   int32_t forLoopIfThenElse(int32_t initial, int32_t final, int32_t bump, int32_t compareValue, bool countsUp = true);
   int32_t whileDoIfThenElse(int32_t initial, int32_t final, int32_t bump, int32_t compareValue);
   int32_t doWhileIfThenElse(int32_t initial, int32_t final, int32_t bump, int32_t compareValue);
   int32_t shootoutNestedLoop(int32_t n);
   int32_t absDiff(int32_t leftV, int32_t rightV);
   int32_t maxIfThen(int32_t leftV, int32_t rightV);
   int64_t subIfFalseThen(int64_t valueA, int64_t valueB);
   int32_t ifThenElseLoop(int32_t leftV, int32_t rightV);
   int32_t forLoopContinue(int32_t i);
   int32_t forLoopBreak(int32_t i);
   int32_t forLoopBreakAndContinue(int32_t i);

   static RecursiveFibFunctionType *_recursiveFibMethod;
   static IterativeFibFunctionType *_iterativeFibMethod;
   static DoWhileFibFunctionType *_doWhileFibMethod;
   static DoWhileFunctionType *_doWhileWithBreakMethod;
   static DoWhileFunctionType *_doWhileWithContinueMethod;
   static DoWhileFunctionType *_doWhileWithBreakAndContinueMethod;
   static WhileDoFibFunctionType *_whileDoFibMethod;
   static WhileDoFunctionType *_whileDoWithBreakMethod;
   static WhileDoFunctionType *_whileDoWithContinueMethod;
   static WhileDoFunctionType *_whileDoWithBreakAndContinueMethod;
   static BasicForLoopFunctionType *_basicForLoopUpMethod;
   static BasicForLoopFunctionType *_basicForLoopDownMethod;
   static signatureCharI_I_testMethodType *_shootoutNestedLoopMethod;
   static IfFunctionType *_absDiffIfThenElseMethod;
   static IfFunctionType *_maxIfThenMethod;
   static IfLongFunctionType *_subIfFalseThenMethod;
   static IfFunctionType *_ifThenElseLoopMethod;
   static LoopIfThenElseFunctionType *_forLoopUpIfThenElseMethod;
   static LoopIfThenElseFunctionType *_whileDoIfThenElseMethod;
   static LoopIfThenElseFunctionType *_doWhileIfThenElseMethod;
   static ForLoopContinueFunctionType *_forLoopContinueMethod;
   static ForLoopBreakFunctionType *_forLoopBreakMethod;
   static ForLoopBreakAndContinueFunctionType *_forLoopBreakAndContinueMethod;
   static bool _traceEnabled;
   };

class RecursiveFibonnaciMethod : public TR::MethodBuilder
   {
   public:
   RecursiveFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class IterativeFibonnaciMethod : public TR::MethodBuilder
   {
   public:
   IterativeFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class DoWhileFibonnaciMethod : public TR::MethodBuilder
   {
   public:
   DoWhileFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class DoWhileWithBreakMethod : public TR::MethodBuilder
   {
   public:
   DoWhileWithBreakMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class DoWhileWithContinueMethod : public TR::MethodBuilder
   {
   public:
   DoWhileWithContinueMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class DoWhileWithBreakAndContinueMethod : public TR::MethodBuilder
   {
   public:
   DoWhileWithBreakAndContinueMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class WhileDoFibonnaciMethod : public TR::MethodBuilder
   {
   public:
   WhileDoFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class WhileDoWithBreakMethod : public TR::MethodBuilder
   {
   public:
   WhileDoWithBreakMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class WhileDoWithContinueMethod : public TR::MethodBuilder
   {
   public:
   WhileDoWithContinueMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class WhileDoWithBreakAndContinueMethod : public TR::MethodBuilder
   {
   public:
   WhileDoWithBreakAndContinueMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class BasicForLoopUpMethod : public TR::MethodBuilder
	{
	public:
	BasicForLoopUpMethod(TR::TypeDictionary *types, BuilderTest *test);
	virtual bool buildIL();
	};

class BasicForLoopDownMethod : public TR::MethodBuilder
	{
	public:
	BasicForLoopDownMethod(TR::TypeDictionary *types, BuilderTest *test);
	virtual bool buildIL();
	};

class ForLoopUPIfThenElseMethod : public TR::MethodBuilder
   {
   public:
   ForLoopUPIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class WhileDoIfThenElseMethod : public TR::MethodBuilder
   {
   public:
   WhileDoIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class DoWhileIfThenElseMethod : public TR::MethodBuilder
   {
   public:
   DoWhileIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class ShootoutNestedLoopMethod : public TR::MethodBuilder
   {
   public:
   ShootoutNestedLoopMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class AbsDiffIfThenElseMethod : public TR::MethodBuilder
   {
   public:
   AbsDiffIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class MaxIfThenMethod : public TR::MethodBuilder
   {
   public:
   MaxIfThenMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class IfThenElseLoopMethod : public TR::MethodBuilder
   {
   public:
   IfThenElseLoopMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class ForLoopContinueMethod : public TR::MethodBuilder
   {
   public:
   ForLoopContinueMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class ForLoopBreakMethod : public TR::MethodBuilder
   {
   public:
   ForLoopBreakMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

class ForLoopBreakAndContinueMethod : public TR::MethodBuilder
   {
   public:
   ForLoopBreakAndContinueMethod(TR::TypeDictionary *types, BuilderTest *test);
   virtual bool buildIL();
   };

} // namespace TestCompiler

#endif // !defined(BUILDERTEST_INCL)
