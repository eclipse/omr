/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "OpCodesTest.hpp"

namespace TestCompiler
{
class X86OpCodesTest : public OpCodesTest
   {
   public:
   virtual void compileIntegerArithmeticTestMethods();
   virtual void compileFloatArithmeticTestMethods();
   virtual void compileMemoryOperationTestMethods();
   virtual void compileUnaryTestMethods();
   virtual void compileBitwiseTestMethods();
   virtual void compileDirectCallTestMethods();
   virtual void compileCompareTestMethods();
   virtual void compileSelectTestMethods();
   virtual void compileAddressTestMethods();

   virtual void invokeIntegerArithmeticTests();
   virtual void invokeFloatArithmeticTests();
   virtual void invokeMemoryOperationTests();
   virtual void invokeUnaryTests();
   virtual void invokeBitwiseTests();
   virtual void invokeDirectCallTests();
   virtual void invokeCompareTests();
   virtual void invokeSelectTests();
   virtual void invokeAddressTests();
   virtual void UnsupportedOpCodesTests();

   virtual void compileDisabledIntegerArithmeticTestMethods();
   virtual void invokeDisabledIntegerArithmeticTests();
   virtual void compileDisabledConvertOpCodesTest();
   virtual void invokeDisabledConvertOpCodesTest();
   virtual void compileDisabledMemoryOpCodesTest();
   virtual void invokeDisabledMemoryOpCodesTest();

   virtual void invokeNoHelperUnaryTests();
   };

} // namespace TestCompiler
