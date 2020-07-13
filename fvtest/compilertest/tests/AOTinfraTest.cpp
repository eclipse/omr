/**********************************************************************
* Copyright (c) 2020, 2020 IBM Corp. and others
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
**********************************************************************/

#include "gtest/gtest.h"
#include "tests/AOTinfraTest.hpp"
#include "env/AOTMethodHeader.hpp"
#include "env/AOTStorageInterface.hpp"
#include "compile/Compilation.hpp"
#include "compiler/env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"

namespace TestCompiler
{

RecursiveFibFunctionType *AOTinfraTest::_recursiveFibMethod = 0;
bool AOTinfraTest::_traceEnabled = false;

void
AOTinfraTest::compileTestMethods()
   {
   int32_t rc = 0;
   uint8_t *entry=0;

   TR::TypeDictionary types;

   RecursiveFibMethod recFibMethodBuilder(&types, this);
   rc = compileMethodBuilder(&recFibMethodBuilder, &entry);
   _recursiveFibMethod = (RecursiveFibFunctionType *) entry;
   }

int32_t
AOTinfraTest::recursiveFib(int32_t n)
   {
   if (n < 2)
      return n;
   return recursiveFib(n-1) + recursiveFib(n-2);
   }

void
AOTinfraTest::AOTMethodHeaderTest()
   {

   uint32_t firstCompiledCodeSize = 64;
   uint8_t* firstCompiledCodeStart = reinterpret_cast<uint8_t*>(&firstCompiledCodeSize);
   uint32_t firstRelocationsSize = 128;
   uint8_t* firstRelocationsStart = reinterpret_cast<uint8_t*>(&firstRelocationsSize);

   TR::AOTMethodHeader firstInstance(firstCompiledCodeStart, firstCompiledCodeSize, firstRelocationsStart, firstRelocationsSize);

   uint32_t firstCompiledCodeSizeOutput = firstInstance.self()->compiledCodeSize;
   uint8_t* firstCompiledCodeStartOutput = firstInstance.self()->compiledCodeStart;
   uint32_t firstRelocationsSizeOutput = firstInstance.self()->relocationsSize;
   uint8_t* firstRelocationsStartOutput = firstInstance.self()->relocationsStart;

   uint8_t* rawData = reinterpret_cast<uint8_t*>(firstInstance.self()->serialize());
   TR::AOTMethodHeader secondInstance(rawData);
   uint8_t* returnedData = reinterpret_cast<uint8_t*>(secondInstance.self()->serialize());
  
   uint32_t secondCompiledCodeSize = secondInstance.self()->compiledCodeSize;
   uint8_t* secondCompiledCodeStart = secondInstance.self()->compiledCodeStart;
   uint32_t secondRelocationsSize = secondInstance.self()->relocationsSize;
   uint8_t* secondRelocationsStart = secondInstance.self()->relocationsStart;

   EXPECT_EQ(*rawData,*returnedData);

   EXPECT_EQ(firstCompiledCodeSize,firstCompiledCodeSizeOutput);
   EXPECT_EQ(firstCompiledCodeStart,firstCompiledCodeStartOutput);
   EXPECT_EQ(firstRelocationsSize,firstRelocationsSizeOutput);
   EXPECT_EQ(firstRelocationsStart,firstRelocationsStartOutput);

   /**
    * After serialization the pointer is no longer the same
    */

   EXPECT_EQ(firstCompiledCodeSize,secondCompiledCodeSize);
   EXPECT_EQ(*firstCompiledCodeStart,*secondCompiledCodeStart);
   EXPECT_EQ(firstRelocationsSize,secondRelocationsSize);
   EXPECT_EQ(*firstRelocationsStart,*secondRelocationsStart);
   }

void
AOTinfraTest::AOTStorageTest()
   {
   uint8_t sizeFirst = 64;
   uint8_t dataFirstArray[] = "64";
   uint8_t* dataFirst = &dataFirstArray[0];
   void* dataVoidFirst = &sizeFirst;
   const char* keyFirst = "1";

   uint8_t sizeSecond = 128;
   uint8_t dataSecondArray[] = "128";
   uint8_t* dataSecond = &dataSecondArray[0];
   void* dataVoidSecond = &sizeSecond;
   const char* keySecond = "2";

   uint8_t sizeThird = 32;
   uint8_t dataThirdArray[] = "32";
   uint8_t* dataThird = &dataThirdArray[0];
   void* dataVoidThird = &sizeThird;
   const char* keyThird = "3";

   AOTStorageInterface test;

   test.storeEntry(keyFirst, dataVoidFirst, sizeFirst);
   test.storeEntry(keySecond, dataVoidSecond, sizeSecond);
   test.storeEntry(keyThird, dataVoidThird, sizeThird);

   uint8_t* returnedFirst = test.loadEntry(keyFirst);
   uint8_t* returnedSecond = test.loadEntry(keySecond);
   uint8_t* returnedThird = test.loadEntry(keyThird);
   
   EXPECT_EQ(*dataFirst, *returnedFirst);
   EXPECT_EQ(*dataSecond, *returnedSecond);
   EXPECT_EQ(*dataThird, *returnedThird);
   }

void
AOTinfraTest::AOTAdapterTest()
   {
   compileTestMethods();
   TR::RelocationRuntime* rr = TR::Compiler->aotAdapter->getRelocationRuntime();
   void* methodCode = TR::Compiler->aotAdapter->getMethodCode("fib_recur");
   void* nullPtr = NULL;
   EXPECT_NE(nullPtr, methodCode);
   EXPECT_NE(nullPtr, rr);
   }

RecursiveFibMethod::RecursiveFibMethod(TR::TypeDictionary *types, AOTinfraTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_recur");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);

   DefineMemory("traceEnabled", Int32, test->traceEnabledLocation());
   }

bool
RecursiveFibMethod::buildIL()
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

}// namespace TestCompiler


TEST(JITILBuilderTest, AOTMethodHeaderTest)
   {
   TestCompiler::AOTinfraTest _aotinfraTest;
   _aotinfraTest.AOTMethodHeaderTest();
   }

TEST(JITILBuilderTest, AOTStorageTest)
   {
   TestCompiler::AOTinfraTest _aotinfraTest;
   _aotinfraTest.AOTStorageTest();
   }

TEST(JITILBuilderTest, AOTAdapterTest)
   {
   TestCompiler::AOTinfraTest _aotinfraTest;
   _aotinfraTest.AOTAdapterTest();
   }
