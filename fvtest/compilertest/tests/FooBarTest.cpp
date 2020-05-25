/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
#include "compile/ResolvedMethod.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "OpCodesTest.hpp"
#include "tests/FooBarTest.hpp"
#include "tests/injectors/BarIlInjector.hpp"
#include "tests/injectors/FooIlInjector.hpp"
#include "gtest/gtest.h"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"

namespace TestCompiler
{


FooMethodType  * FooBarTest::_foo = 0;
BarMethodType  * FooBarTest::_bar = 0;
TR::ResolvedMethod * FooBarTest::_barCompilee;

void
FooBarTest::compileTestMethods()
   {
   int32_t rc = 0;

   TR::TypeDictionary types;

   //_bar = &FooBarTest::bar;
   BarIlInjector barIlInjector(&types, this);
   int32_t numberOfArguments = 1;
   TR::IlType *Int32 = types.PrimitiveType(TR::Int32);
   TR::IlType **argTypes = new TR::IlType*[numberOfArguments];
   argTypes[0] = Int32;
   bool argIsArray[1] = { false };
   TR::ResolvedMethod barCompilee(__FILE__, LINETOSTR(__LINE__), "bar", numberOfArguments, argTypes, Int32, 0, &barIlInjector);
   _barCompilee = &barCompilee;
   TR::IlGeneratorMethodDetails barDetails(&barCompilee);

   _bar = (BarMethodType *) (compileMethod(barDetails, warm, rc));
   barCompilee.setEntryPoint((void *)_bar);

   FooIlInjector fooIlInjector(&types, this);
   TR::ResolvedMethod fooCompilee(__FILE__, LINETOSTR(__LINE__), "foo", numberOfArguments, argTypes, Int32, 0, &fooIlInjector);
   TR::IlGeneratorMethodDetails fooDetails(&fooCompilee);
   _foo = (FooMethodType *) (compileMethod(fooDetails, warm, rc));

   }

const int32_t FooBarTest::_dataArraySize;
int32_t FooBarTest::_dataArray[100];

int32_t
FooBarTest::foo(int32_t index)
   {
   int32_t newIndex = _bar(index);
   if (newIndex < 0 || newIndex >= _dataArraySize)
      return -1;
   return _dataArray[newIndex];
   }

int32_t
FooBarTest::bar(int32_t index)
   {
   if (index < 0 || index >= _dataArraySize)
      return -1;
   return _dataArray[index];
   }


void
FooBarTest::invokeTests()
   {
   int32_t i;

//   // First set of tests should map i to i
   for (i=0;i < _dataArraySize;i++)
      _dataArray[i] = _dataArraySize - i;

   int32_t testID = 0, result;
   OMR_CT_EXPECT_EQ(_foo, -1, _foo(0));

   for (i = 1; i < _dataArraySize;i++,testID++)
      {
      OMR_CT_EXPECT_EQ(_foo, i, _foo(i));
      }

   // Second set of tests should map i to N-i
   for (i = 0; i < _dataArraySize;i++/*,testID++*/)
      {
      OMR_CT_EXPECT_EQ(_bar, _dataArraySize - i, _bar(i));
      }

   // Third set of tests should map i to 1
   for (int32_t i=0;i < _dataArraySize;i++)
      _dataArray[i] = 1;
   for (i = 0;i < _dataArraySize;i++,testID++)
      {
      OMR_CT_EXPECT_EQ(_foo, 1, _foo(i));
      }

   OMR_CT_EXPECT_EQ(_foo, -1, _foo(-1));
   OMR_CT_EXPECT_EQ(_foo, -1, _foo(INT_MIN));
   OMR_CT_EXPECT_EQ(_foo, -1, _foo(_dataArraySize));
   OMR_CT_EXPECT_EQ(_foo, -1, _foo(INT_MAX));
   }

} // namespace TestCompiler

// This test will get assertion on S390-64, because of
// "compiler/codegen/FrontEnd.cpp" TR_FrontEnd::methodTrampolineLookup is unimplemented and
// "test/env/FrontEnd.cpp"  TestCompiler::FrontEnd::methodTrampolineLookup is "methodTrampolineLookup not implemented yet".
// This test also failed intermittent (segfault) on PPCLE, temporarily disabled this test on PPCLE, under track of Work Item
// Please remove this #ifdef after those functions are implemented.
#if defined(TR_TARGET_X86) || defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
TEST(JITTest, FooBarTest)
   {
   ::TestCompiler::FooBarTest _fooBarTest;
   _fooBarTest.RunTest();
   }
#endif
