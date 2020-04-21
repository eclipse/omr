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
#include "OpCodesTest.hpp"
#include "tests/Qux2Test.hpp"
#include "tests/injectors/Qux2IlInjector.hpp"
#include "gtest/gtest.h"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"

namespace TestCompiler
{
class Qux2IlInjector;
testMethodType  * Qux2Test::_qux2 = 0;

void
Qux2Test::compileTestMethods()
   {
   int32_t rc = 0;

   TR::TypeDictionary types;
   Qux2IlInjector quxIlInjector(&types, this);
   int32_t numberOfArguments = 1;
   TR::IlType ** argTypes = new TR::IlType*[numberOfArguments];
   TR::IlType *Int32 = types.PrimitiveType(TR::Int32);
   argTypes[0]= Int32;
   TR::ResolvedMethod qux2Compilee(__FILE__, LINETOSTR(__LINE__), "qux2", numberOfArguments, argTypes, Int32, 0, &quxIlInjector);
   TR::IlGeneratorMethodDetails qux2Details(&qux2Compilee);
   _qux2 = (testMethodType *) (compileMethod(qux2Details, warm, rc));
   }

int32_t
Qux2Test::qux2(int32_t num)
   {
   int i = num;
   i = i * 2;
   return i;
   }

void
Qux2Test::invokeTests()
   {
   OMR_CT_EXPECT_EQ(_qux2, qux2(5), _qux2(5));
   OMR_CT_EXPECT_EQ(_qux2, 10, _qux2(5));
   OMR_CT_EXPECT_EQ(_qux2, qux2(INT_MAX/2), _qux2(INT_MAX/2));
//   OMR_CT_EXPECT_EQ(_qux2, qux2(INT_MIN), _qux2(INT_MIN));
   }

} // namespace TestCompiler

TEST(JITQuxTest, QuxTest2)
   {
   ::TestCompiler::Qux2Test qux2Test;
   qux2Test.RunTest();
   }
