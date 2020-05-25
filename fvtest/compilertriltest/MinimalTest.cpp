/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <limits.h>
#include <stdio.h>
#include "gtest/gtest.h"
#include "JitTest.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/CompileMethod.hpp"
#include "compile/CompilationTypes.hpp"
#include "il/DataTypes.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"

class MinimalTest : public TRTest::JitTest
   {
   public:
   MinimalTest();
   };

MinimalTest::MinimalTest()
   : TRTest::JitTest::JitTest()
   {}

template <typename S>
class StaticSignatureMethodBuilder : public TR::MethodBuilder
   {
   public:
   typedef S* FunctionPtr;

   StaticSignatureMethodBuilder(TR::TypeDictionary* types)
   	  : TR::MethodBuilder(types)
   	  {}

   FunctionPtr Compile()
      {
      TR::ResolvedMethod resolvedMethod(this);
      TR::IlGeneratorMethodDetails details(&resolvedMethod);
      int32_t rc = 0;
      FunctionPtr entry = (FunctionPtr)compileMethodFromDetails(NULL, details, warm, rc);
      return entry;
      }
   };

class MeaningOfLifeMethod : public StaticSignatureMethodBuilder<int32_t()>
   {
   public:
   MeaningOfLifeMethod(TR::TypeDictionary *types);
   virtual bool buildIL();
   };

MeaningOfLifeMethod::MeaningOfLifeMethod(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t()>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("MeaningOfLife");
   DefineReturnType(Int32);
   }

bool
MeaningOfLifeMethod::buildIL()
   {
   Return(
      ConstInt32(42));
   return true;
   }

class ReturnArgI32Method : public StaticSignatureMethodBuilder<int32_t(int32_t)>
   {
   public:
   ReturnArgI32Method(TR::TypeDictionary *types);
   virtual bool buildIL();
   };

ReturnArgI32Method::ReturnArgI32Method(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t(int32_t)>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("ReturnArgI32");
   DefineParameter("arg", Int32);
   DefineReturnType(Int32);
   }

bool
ReturnArgI32Method::buildIL()
   {
   DefineLocal("result", Int32);
   Store("result",
         Load("arg"));
   Return(
      Load("result"));
   return true;
   }

class MaxIfThenMethod : public StaticSignatureMethodBuilder<int32_t(int32_t, int32_t)>
   {
   public:
   MaxIfThenMethod(TR::TypeDictionary *types);
   virtual bool buildIL();
   };

MaxIfThenMethod::MaxIfThenMethod(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t(int32_t, int32_t)>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("MaxIfThenMethod");
   DefineParameter("leftV", Int32);
   DefineParameter("rightV", Int32);
   DefineReturnType(Int32);
   }

bool
MaxIfThenMethod::buildIL()
   {
   TR::IlBuilder *thenPath = NULL;
   IfThen(&thenPath,
      GreaterThan(
         Load("leftV"),
         Load("rightV")));

   thenPath->Return(
   thenPath->   Load("leftV"));

   Return(
      Load("rightV"));

   return true;
   }

class AddArgConstMethod : public StaticSignatureMethodBuilder<int32_t(int32_t)>
   {
   public:
   AddArgConstMethod(TR::TypeDictionary *types);
   virtual bool buildIL();
   };

AddArgConstMethod::AddArgConstMethod(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t(int32_t)>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("AddArgConst");
   DefineParameter("arg", Int32);
   DefineReturnType(Int32);
   }

bool
AddArgConstMethod::buildIL()
   {
   Return(
      Add(
         Load("arg"),
         ConstInt32(42)));
   return true;
   }

class SubArgArgMethod : public StaticSignatureMethodBuilder<int32_t(int32_t, int32_t)>
   {
   public:
   SubArgArgMethod(TR::TypeDictionary *types);
   virtual bool buildIL();
   };

SubArgArgMethod::SubArgArgMethod(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t(int32_t, int32_t)>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("SubArgArg");
   DefineParameter("arg1", Int32);
   DefineParameter("arg2", Int32);
   DefineReturnType(Int32);
   }

bool
SubArgArgMethod::buildIL()
   {
   Return(
      Sub(
         Load("arg1"),
         Load("arg2")));
   return true;
   }

class FactorialMethod : public StaticSignatureMethodBuilder<int32_t(int32_t)>
   {
   public:
   FactorialMethod(TR::TypeDictionary *types);
   virtual bool buildIL();
   };

FactorialMethod::FactorialMethod(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t(int32_t)>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("Factorial");
   DefineParameter("x", Int32);
   DefineReturnType(Int32);
   }

bool
FactorialMethod::buildIL()
   {
   TR::IlBuilder *baseCase=NULL, *recursiveCase=NULL;
   IfThenElse(&baseCase, &recursiveCase,
      LessThan(
         Load("x"),
         ConstInt32(1)));

   DefineLocal("result", Int32);

   baseCase->Store("result",
   baseCase->   ConstInt32(1));
   recursiveCase->Store("result",
   recursiveCase->   Mul(
   recursiveCase->      Call("Factorial", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("x"),
   recursiveCase->            ConstInt32(1))),
   recursiveCase->      Load("x")));

   Return(
      Load("result"));
   return true;
   }

class RecursiveFibonnaciMethod : public StaticSignatureMethodBuilder<int32_t(int32_t)>
   {
   public:
	RecursiveFibonnaciMethod(TR::TypeDictionary *types);
   virtual bool buildIL();
   };


RecursiveFibonnaciMethod::RecursiveFibonnaciMethod(TR::TypeDictionary *types)
   : StaticSignatureMethodBuilder<int32_t(int32_t)>(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_recur");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
RecursiveFibonnaciMethod::buildIL()
   {
   TR::IlBuilder *baseCase=NULL, *recursiveCase=NULL;
   IfThenElse(&baseCase, &recursiveCase,
      LessThan(
         Load("n"),
         ConstInt32(2)));

   DefineLocal("result", Int32);

   baseCase->Store("result",
   baseCase->   Load("n"));
   recursiveCase->Store("result",
   recursiveCase->   Add(
   recursiveCase->      Call("fib_recur", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("n"),
   recursiveCase->            ConstInt32(1))),
   recursiveCase->      Call("fib_recur", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("n"),
   recursiveCase->            ConstInt32(2)))));

   Return(
      Load("result"));
   return true;
   }


template <typename M>
typename M::FunctionPtr compile()
   {
   TR::TypeDictionary types;
   M m(&types);
   return m.Compile();
   }

TEST_F(MinimalTest, MeaningOfLife)
   {
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<MeaningOfLifeMethod>();

   EXPECT_EQ(entry(), 42);
   }

TEST_F(MinimalTest, ReturnArgI32)
   {
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<ReturnArgI32Method>();

   EXPECT_EQ(entry(42), 42);
   EXPECT_EQ(entry(-1), -1);
   }

TEST_F(MinimalTest, MaxIfThen)
   {
   std::string arch = omrsysinfo_get_CPU_architecture();
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<MaxIfThenMethod>();

   EXPECT_EQ(entry(10, 20), 20);
   EXPECT_EQ(entry(20, 10), 20);
   EXPECT_EQ(entry(-20,10), 10);
   EXPECT_EQ(entry(10,-20), 10);
   }

TEST_F(MinimalTest, AddArgConst)
   {
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<AddArgConstMethod>();

   EXPECT_EQ(entry(42), 42+42);
   EXPECT_EQ(entry(-1), 42-1);
   }

TEST_F(MinimalTest, SubArgArg)
   {
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<SubArgArgMethod>();

   EXPECT_EQ(entry(10, 20), -10);
   EXPECT_EQ(entry(20, 10), 10);
   EXPECT_EQ(entry(-20,10), -30);
   EXPECT_EQ(entry(10,-20), 30);
   }

TEST_F(MinimalTest, Factorial)
   {
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<FactorialMethod>();

   EXPECT_EQ(entry(5), 120);
   }

static int32_t
RecursiveFibonnaci(int32_t n)
   {
   if (n < 2)
      return n;
   return RecursiveFibonnaci(n-1) + RecursiveFibonnaci(n-2);
   }

TEST_F(MinimalTest, RecursiveFibonnaci)
   {
   SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
   SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
   if ("z/OS" != omrsysinfo_get_OS_type()) {
      SKIP_ON_S390(MissingImplementation) << "Test is skipped on S390 because calls are not currently supported (see issue #1645)";
      SKIP_ON_S390X(MissingImplementation) << "Test is skipped on S390x because calls are not currently supported (see issue #1645)";
   }
   SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
   SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

   auto entry = compile<RecursiveFibonnaciMethod>();

   EXPECT_EQ(entry(10), RecursiveFibonnaci(10));
   }
