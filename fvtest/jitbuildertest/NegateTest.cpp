/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "JBTestUtil.hpp"


DEFINE_BUILDER(TestInt64Negate,
               Int64,
               PARAM("param", Int64))
   {
   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *result = Negate(param);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestInt32Negate,
               Int32,
               PARAM("param", Int32))
   {
   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *result = Negate(param);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestInt16Negate,
               Int16,
               PARAM("param", Int16))
   {
   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *result = Negate(param);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestInt8Negate,
               Int8,
               PARAM("param", Int8))
   {
   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *result = Negate(param);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestFloatNegate,
               Float,
               PARAM("param", Float))
   {
   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *result = Negate(param);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestDoubleNegate,
               Double,
               PARAM("param", Double))
   {
   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *result = Negate(param);
   Return(result);
   return true;
   }

class NegateTest : public JitBuilderTest {};

typedef int64_t (*Int64ReturnType)(int64_t);
TEST_F(NegateTest, Int64_Test)
   {
   Int64ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt64Negate, testFunction);
   ASSERT_EQ(testFunction(0), 0);
   ASSERT_EQ(testFunction(1), -1);
   ASSERT_EQ(testFunction(-1), 1);
   ASSERT_EQ(testFunction(INT64_MAX), INT64_MIN + 1);
   ASSERT_EQ(testFunction(INT64_MIN + 1), INT64_MAX);
   ASSERT_EQ(testFunction(INT64_MIN), INT64_MIN);
   }

typedef int32_t (*Int32ReturnType)(int32_t);
TEST_F(NegateTest, Int32_Test)
   {
   Int32ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt32Negate, testFunction);
   ASSERT_EQ(testFunction(0), 0);
   ASSERT_EQ(testFunction(1), -1);
   ASSERT_EQ(testFunction(-1), 1);
   ASSERT_EQ(testFunction(INT32_MAX), INT32_MIN + 1);
   ASSERT_EQ(testFunction(INT32_MIN + 1), INT32_MAX);
   ASSERT_EQ(testFunction(INT32_MIN), INT32_MIN);
   }

typedef int16_t (*Int16ReturnType)(int16_t);
TEST_F(NegateTest, Int16_Test)
   {
   Int16ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt16Negate, testFunction);
   ASSERT_EQ(testFunction(0), 0);
   ASSERT_EQ(testFunction(1), -1);
   ASSERT_EQ(testFunction(-1), 1);
   ASSERT_EQ(testFunction(INT16_MAX), INT16_MIN + 1);
   ASSERT_EQ(testFunction(INT16_MIN + 1), INT16_MAX);
   ASSERT_EQ(testFunction(INT16_MIN), INT16_MIN);
   }

typedef int8_t (*Int8ReturnType)(int8_t);
TEST_F(NegateTest, Int8_Test)
   {
   Int8ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt8Negate, testFunction);
   ASSERT_EQ(testFunction(0), 0);
   ASSERT_EQ(testFunction(1), -1);
   ASSERT_EQ(testFunction(-1), 1);
   ASSERT_EQ(testFunction(INT8_MAX), INT8_MIN + 1);
   ASSERT_EQ(testFunction(INT8_MIN + 1), INT8_MAX);
   ASSERT_EQ(testFunction(INT8_MIN), INT8_MIN);
   }

typedef float (*FloatReturnType)(float);
TEST_F(NegateTest, Float_Test)
   {
   FloatReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestFloatNegate, testFunction);
   ASSERT_EQ(testFunction(0), 0);
   ASSERT_EQ(testFunction(1), -1);
   ASSERT_EQ(testFunction(-1), 1);
   ASSERT_EQ(testFunction(FLT_MIN), -FLT_MIN);
   ASSERT_EQ(testFunction(FLT_MAX), -FLT_MAX);
   }

typedef double (*DoubleReturnType)(double);
TEST_F(NegateTest, Double_Test)
   {
   DoubleReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestDoubleNegate, testFunction);
   ASSERT_EQ(testFunction(0), 0);
   ASSERT_EQ(testFunction(1), -1);
   ASSERT_EQ(testFunction(-1), 1);
   ASSERT_EQ(testFunction(DBL_MIN), -DBL_MIN);
   ASSERT_EQ(testFunction(DBL_MAX), -DBL_MAX);
   }
