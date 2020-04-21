/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "JBTestUtil.hpp"

DEFINE_BUILDER(TestInt8Select,
               Int8,
               PARAM("condition", Int32),
               PARAM("trueValue", Int8),
               PARAM("falseValue", Int8))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestInt16Select,
               Int16,
               PARAM("condition", Int32),
               PARAM("trueValue", Int16),
               PARAM("falseValue", Int16))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestInt32Select,
               Int32,
               PARAM("condition", Int32),
               PARAM("trueValue", Int32),
               PARAM("falseValue", Int32))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestInt64Select,
               Int64,
               PARAM("condition", Int32),
               PARAM("trueValue", Int64),
               PARAM("falseValue", Int64))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestAddressSelect,
               Address,
               PARAM("condition", Int32),
               PARAM("trueValue", Address),
               PARAM("falseValue", Address))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestFloatSelect,
               Float,
               PARAM("condition", Int32),
               PARAM("trueValue", Float),
               PARAM("falseValue", Float))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestDoubleSelect,
               Double,
               PARAM("condition", Int32),
               PARAM("trueValue", Double),
               PARAM("falseValue", Double))
   {
   OMR::JitBuilder::IlValue * condition = Load("condition");
   OMR::JitBuilder::IlValue * trueValue = Load("trueValue");
   OMR::JitBuilder::IlValue * falseValue = Load("falseValue");
   OMR::JitBuilder::IlValue * result = Select(condition, trueValue, falseValue);
   Return(result);
   return true;
   }

class SelectTest : public JitBuilderTest {};

typedef int8_t (*Int8TestFunctionType)(int32_t,int8_t,int8_t);
TEST_F(SelectTest, Int8_Test)
   {
   Int8TestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt8Select, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }

typedef int16_t (*Int16TestFunctionType)(int32_t,int16_t,int16_t);
TEST_F(SelectTest, Int16_Test)
   {
   Int16TestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt16Select, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }

typedef int32_t (*Int32TestFunctionType)(int32_t,int32_t,int32_t);
TEST_F(SelectTest, Int32_Test)
   {
   Int32TestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt32Select, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }

typedef int64_t (*Int64TestFunctionType)(int32_t,int64_t,int64_t);
TEST_F(SelectTest, Int64_Test)
   {
   Int64TestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt64Select, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }

typedef uintptr_t (*AddressTestFunctionType)(int32_t,uintptr_t,uintptr_t);
TEST_F(SelectTest, Address_Test)
   {
   AddressTestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestAddressSelect, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }

typedef float (*FloatTestFunctionType)(int32_t,float,float);
TEST_F(SelectTest, Float_Test)
   {
   FloatTestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestFloatSelect, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }

typedef double (*DoubleTestFunctionType)(int32_t,double,double);
TEST_F(SelectTest, Double_Test)
   {
   DoubleTestFunctionType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestDoubleSelect, testFunction);
   ASSERT_EQ(testFunction(1,1,0), 1);
   ASSERT_EQ(testFunction(0,1,0), 0);
   }
