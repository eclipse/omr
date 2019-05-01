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

static int64_t
testInt64(int64_t val)
   {
   #define TEST_INT64_LINE LINETOSTR(__LINE__)
   return val;
   }

static int32_t
testInt32(int32_t val)
   {
   #define TEST_INT32_LINE LINETOSTR(__LINE__)
   return val;
   }

static int16_t
testInt16(int16_t val)
   {
   #define TEST_INT16_LINE LINETOSTR(__LINE__)
   return val;
   }

static int8_t
testInt8(int8_t val)
   {
   #define TEST_INT8_LINE LINETOSTR(__LINE__)
   return val;
   }

DEFINE_BUILDER(TestInt64ReturnType,
               Int64,
               PARAM("param", Int64))
   {
   DefineFunction((char *)"testInt64",
                  (char *)__FILE__,
                  (char *)TEST_INT64_LINE,
                  (void *)&testInt64,
                  Int64,
                  1,
                  Int64);

   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *int64Result = Call("testInt64", 1, param);
   OMR::JitBuilder::IlValue *int64Val = ConstInt64(1);
   OMR::JitBuilder::IlValue *result = Add(int64Result, int64Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt32ReturnType,
               Int32,
               PARAM("param", Int32))
   {
   DefineFunction((char *)"testInt32",
                  (char *)__FILE__,
                  (char *)TEST_INT32_LINE,
                  (void *)&testInt32,
                  Int32,
                  1,
                  Int32);

   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *int32Result = Call("testInt32", 1, param);
   OMR::JitBuilder::IlValue *int32Val = ConstInt32(1);
   OMR::JitBuilder::IlValue *result = Add(int32Result, int32Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt16ReturnType,
               Int16,
               PARAM("param", Int16))
   {
   DefineFunction((char *)"testInt16",
                  (char *)__FILE__,
                  (char *)TEST_INT16_LINE,
                  (void *)&testInt16,
                  Int16,
                  1,
                  Int16);

   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *int16Result = Call("testInt16", 1, param);
   OMR::JitBuilder::IlValue *int16Val = ConstInt16(1);
   OMR::JitBuilder::IlValue *result = Add(int16Result, int16Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt8ReturnType,
               Int8,
               PARAM("param", Int8))
   {
   DefineFunction((char *)"testInt8",
                  (char *)__FILE__,
                  (char *)TEST_INT8_LINE,
                  (void *)&testInt8,
                  Int8,
                  1,
                  Int8);

   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *int8Result = Call("testInt8", 1, param);
   OMR::JitBuilder::IlValue *int8Val = ConstInt8(1);
   OMR::JitBuilder::IlValue *result = Add(int8Result, int8Val);

   Return(result);

   return true;
   }

class ReturnTypeTest : public JitBuilderTest {};

typedef int64_t (*Int64ReturnType)(int64_t);
TEST_F(ReturnTypeTest, Int64_Test)
   {
   Int64ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt64ReturnType, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT64_MAX - 1), INT64_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT64_MIN), INT64_MIN + 1);
   }

typedef int32_t (*Int32ReturnType)(int32_t);
TEST_F(ReturnTypeTest, Int32_Test)
   {
   Int32ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt32ReturnType, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT32_MAX - 1), INT32_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT32_MIN), INT32_MIN + 1);
   }

typedef int16_t (*Int16ReturnType)(int16_t);
TEST_F(ReturnTypeTest, Int16_Test)
   {
   Int16ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt16ReturnType, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT16_MAX - 1), INT16_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT16_MIN), INT16_MIN + 1);
   }

typedef int8_t (*Int8ReturnType)(int8_t);
TEST_F(ReturnTypeTest, Int8_Test)
   {
   Int8ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt8ReturnType, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT8_MAX - 1), INT8_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT8_MIN), INT8_MIN + 1);
   }
