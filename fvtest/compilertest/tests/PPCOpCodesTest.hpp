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
class PPCOpCodesTest : public OpCodesTest
   {
   public:
   virtual void compileUnaryTestMethods();
   virtual void compileMemoryOperationTestMethods();
   virtual void compileSelectTestMethods();
   virtual void compileCompareTestMethods();
   virtual void compileBitwiseTestMethods();
   virtual void compileAddressTestMethods();

   virtual void invokeUnaryTests();
   virtual void invokeMemoryOperationTests();
   virtual void invokeSelectTests();
   virtual void invokeCompareTests();
   virtual void invokeBitwiseTests();
   virtual void invokeAddressTests();
   virtual void UnsupportedOpCodesTests();

   virtual void compileDisabledConvertTestMethods();
   virtual void compileDisabledCompareTestMethods();
   virtual void compileDisabledIntegerArithmeticTestMethods();
   virtual void compileDisabledFloatArithmeticTestMethods();
   virtual void compileDisabledMemoryOperationTestMethods();
   virtual void compileDisabledUnaryTestMethods();
   virtual void compileDisabledBitwiseTestMethods();
   virtual void compileDisabledSelectTestMethods();
   virtual void compileDisabledDirectCallTestMethods();

   virtual void invokeDisabledConvertTests();
   virtual void invokeDisabledCompareTests();
   virtual void invokeDisabledIntegerArithmeticTests();
   virtual void invokeDisabledFloatArithmeticTests();
   virtual void invokeDisabledMemoryOperationTests();
   virtual void invokeDisabledUnaryTests();
   virtual void invokeDisabledBitwiseTests();
   virtual void invokeDisabledSelectTest();
   virtual void invokeDisabledDirectCallTest();
   };

} // namespace TestCompiler
