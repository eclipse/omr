/*******************************************************************************
 * Copyright (c) 2020 IBM Corp. and others
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

static int64_t globalInt64 = 0;

static int32_t globalInt32 = 0;

static int16_t globalInt16 = 0;

static int8_t globalInt8 = 0;

static float globalFloat = 0.0f;

static double globalDouble = 0.0;

static void *globalAddress = 0;

DEFINE_BUILDER(TestInt64Global,
               Int64,
               PARAM("param", Int64))
   {
   DefineGlobal("int64global",Int64,(void*)(&globalInt64));

   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int64global",param);
   OMR::JitBuilder::IlValue *int64global = Load("int64global");
   OMR::JitBuilder::IlValue *int64Val = ConstInt64(1);
   OMR::JitBuilder::IlValue *result = Add(int64global, int64Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt64GlobalLoad,
               Int64)
   {
   DefineGlobal("int64global",Int64,(void*)(&globalInt64));

   OMR::JitBuilder::IlValue *int64global = Load("int64global");
   OMR::JitBuilder::IlValue *int64Val = ConstInt64(1);
   OMR::JitBuilder::IlValue *result = Add(int64global, int64Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt64GlobalStore,
               NoType,
	       PARAM("param",Int64))
   {
     
   DefineGlobal("int64global",Int64,(void*)(&globalInt64));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int64global",param);

   Return();
   
   return true;
   }

DEFINE_BUILDER(TestInt32Global,
               Int32,
               PARAM("param", Int32))
   {

   DefineGlobal("int32global",Int32,(void*)(&globalInt32));

   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int32global",param);
   OMR::JitBuilder::IlValue *int32global = Load("int32global");
   OMR::JitBuilder::IlValue *int32Val = ConstInt32(1);
   OMR::JitBuilder::IlValue *result = Add(int32global, int32Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt32GlobalLoad,
               Int32)
   {
   DefineGlobal("int32global",Int32,(void*)(&globalInt32));

   OMR::JitBuilder::IlValue *int32global = Load("int32global");
   OMR::JitBuilder::IlValue *int32Val = ConstInt32(1);
   OMR::JitBuilder::IlValue *result = Add(int32global, int32Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt32GlobalStore,
               NoType,
	       PARAM("param",Int32))
   {
     
   DefineGlobal("int32global",Int32,(void*)(&globalInt32));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int32global",param);

   Return();
   
   return true;
   }

DEFINE_BUILDER(TestInt16Global,
               Int16,
               PARAM("param", Int16))
   {

   DefineGlobal("int16global",Int16,(void*)(&globalInt16));

   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int16global",param);
   OMR::JitBuilder::IlValue *int16global = Load("int16global");
   OMR::JitBuilder::IlValue *int16Val = ConstInt16(1);
   OMR::JitBuilder::IlValue *result = Add(int16global, int16Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt16GlobalLoad,
               Int16)
   {
   DefineGlobal("int16global",Int16,(void*)(&globalInt16));

   OMR::JitBuilder::IlValue *int16global = Load("int16global");
   OMR::JitBuilder::IlValue *int16Val = ConstInt16(1);
   OMR::JitBuilder::IlValue *result = Add(int16global, int16Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt16GlobalStore,
               NoType,
	       PARAM("param",Int16))
   {
     
   DefineGlobal("int16global",Int16,(void*)(&globalInt16));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int16global",param);

   Return();
   
   return true;
   }

DEFINE_BUILDER(TestInt8Global,
               Int8,
               PARAM("param", Int8))
   {

   DefineGlobal("int8global",Int8,(void*)(&globalInt8));

   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int8global",param);
   OMR::JitBuilder::IlValue *int8global = Load("int8global");
   OMR::JitBuilder::IlValue *int8Val = ConstInt8(1);
   OMR::JitBuilder::IlValue *result = Add(int8global, int8Val);

   Return(result);

   return true;

   }

DEFINE_BUILDER(TestInt8GlobalLoad,
               Int8)
   {
   DefineGlobal("int8global",Int8,(void*)(&globalInt8));

   OMR::JitBuilder::IlValue *int8global = Load("int8global");
   OMR::JitBuilder::IlValue *int8Val = ConstInt8(1);
   OMR::JitBuilder::IlValue *result = Add(int8global, int8Val);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestInt8GlobalStore,
               NoType,
	       PARAM("param",Int8))
   {
     
   DefineGlobal("int8global",Int8,(void*)(&globalInt8));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("int8global",param);

   Return();
   
   return true;
   }

DEFINE_BUILDER(TestFloatGlobal,
               Float,
               PARAM("param", Float))
   {

   DefineGlobal("floatglobal",Float,(void*)(&globalFloat));

   OMR::JitBuilder::IlValue *param = Load("param");
   Store("floatglobal",param);
   OMR::JitBuilder::IlValue *floatglobal = Load("floatglobal");
   OMR::JitBuilder::IlValue *floatVal = ConstFloat(1);
   OMR::JitBuilder::IlValue *result = Add(floatglobal, floatVal);

   Return(result);

   return true;

   }

DEFINE_BUILDER(TestFloatGlobalLoad,
               Float)
   {
   DefineGlobal("floatglobal",Float,(void*)(&globalFloat));

   OMR::JitBuilder::IlValue *floatglobal = Load("floatglobal");
   OMR::JitBuilder::IlValue *floatVal = ConstFloat(1);
   OMR::JitBuilder::IlValue *result = Add(floatglobal, floatVal);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestFloatGlobalStore,
               NoType,
	       PARAM("param",Float))
   {
     
   DefineGlobal("floatglobal",Float,(void*)(&globalFloat));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("floatglobal",param);

   Return();
   
   return true;
   }

DEFINE_BUILDER(TestDoubleGlobal,
               Double,
               PARAM("param", Double))
   {

   DefineGlobal("doubleglobal",Double,(void*)(&globalDouble));

   OMR::JitBuilder::IlValue *param = Load("param");
   Store("doubleglobal",param);
   OMR::JitBuilder::IlValue *doubleglobal = Load("doubleglobal");
   OMR::JitBuilder::IlValue *doubleVal = ConstDouble(1);
   OMR::JitBuilder::IlValue *result = Add(doubleglobal, doubleVal);

   Return(result);

   return true;

   }

DEFINE_BUILDER(TestDoubleGlobalLoad,
               Double)
   {
   DefineGlobal("doubleglobal",Double,(void*)(&globalDouble));

   OMR::JitBuilder::IlValue *doubleglobal = Load("doubleglobal");
   OMR::JitBuilder::IlValue *doubleVal = ConstDouble(1);
   OMR::JitBuilder::IlValue *result = Add(doubleglobal, doubleVal);

   Return(result);

   return true;
   }

DEFINE_BUILDER(TestDoubleGlobalStore,
               NoType,
	       PARAM("param",Double))
   {
     
   DefineGlobal("doubleglobal",Double,(void*)(&globalDouble));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("doubleglobal",param);

   Return();
   
   return true;
   }

DEFINE_BUILDER(TestAddressGlobal,
               Address,
               PARAM("param", Int64))
   {

   DefineGlobal("addressglobal",PointerTo(Int64),(void*)(&globalAddress));

   OMR::JitBuilder::IlValue *param = Load("param");
   OMR::JitBuilder::IlValue *addressglobal = Load("addressglobal");
   OMR::JitBuilder::IlValue *result = Add(LoadAt(PointerTo(Int64),addressglobal), param);

   Return(result);
   return true;

   }

DEFINE_BUILDER(TestAddressGlobalStore,
               NoType,
               PARAM("param", Address))
   {

   DefineGlobal("addressglobal",PointerTo(Int64),(void*)(&globalAddress));
   OMR::JitBuilder::IlValue *param = Load("param");
   Store("addressglobal",param);

   Return();
   return true;

   }

class GlobalTest : public JitBuilderTest {};

typedef int64_t (*Int64Global)(int64_t);
TEST_F(GlobalTest, Int64_Test)
   {
   Int64Global testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt64Global, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT64_MAX - 1), INT64_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT64_MIN), INT64_MIN + 1);
   }

typedef int64_t (*Int64GlobalLoad)();
TEST_F(GlobalTest, Int64_Load_Test)
   {
   Int64GlobalLoad testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt64GlobalLoad, testFunction);
   globalInt64 = 0;
   ASSERT_EQ(testFunction(), 1);
   globalInt64 = INT64_MAX - 1;
   ASSERT_EQ(testFunction(), INT64_MAX);
   globalInt64 = -1;
   ASSERT_EQ(testFunction(), 0);
   globalInt64 = INT64_MIN;
   ASSERT_EQ(testFunction(), INT64_MIN + 1);
   }

typedef void (*Int64GlobalStore)(int64_t);
TEST_F(GlobalTest, Int64_Store_Test)
   {
   Int64GlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt64GlobalStore, testFunction);
   testFunction(1);
   ASSERT_EQ(globalInt64, 1);
   testFunction(INT64_MAX);
   ASSERT_EQ(globalInt64, INT64_MAX);
   testFunction(0);
   ASSERT_EQ(globalInt64, 0);
   testFunction(INT64_MIN);
   ASSERT_EQ(globalInt64, INT64_MIN);
   }

typedef int32_t (*Int32Global)(int32_t);
TEST_F(GlobalTest, Int32_Test)
   {
   Int32Global testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt32Global, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT32_MAX - 1), INT32_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT32_MIN), INT32_MIN + 1);
   }

typedef int32_t (*Int32GlobalLoad)();
TEST_F(GlobalTest, Int32_Load_Test) 
   {
   Int32GlobalLoad testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt32GlobalLoad, testFunction);
   globalInt32 = 0;
   ASSERT_EQ(testFunction(), 1);
   globalInt32 = INT32_MAX - 1;
   ASSERT_EQ(testFunction(), INT32_MAX);
   globalInt32 = -1;
   ASSERT_EQ(testFunction(), 0);
   globalInt32 = INT32_MIN;
   ASSERT_EQ(testFunction(), INT32_MIN + 1);
   }

typedef void (*Int32GlobalStore)(int32_t);
TEST_F(GlobalTest, Int32_Store_Test)
   {
   Int32GlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt32GlobalStore, testFunction);
   testFunction(1);
   ASSERT_EQ(globalInt32, 1);
   testFunction(INT32_MAX);
   ASSERT_EQ(globalInt32, INT32_MAX);
   testFunction(0);
   ASSERT_EQ(globalInt32, 0);
   testFunction(INT32_MIN);
   ASSERT_EQ(globalInt32, INT32_MIN);
   }

typedef int16_t (*Int16Global)(int16_t);
TEST_F(GlobalTest, Int16_Test)
   {
   Int16Global testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt16Global, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT16_MAX - 1), INT16_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT16_MIN), INT16_MIN + 1);
   }

typedef int16_t (*Int16GlobalLoad)();
TEST_F(GlobalTest, Int16_Load_Test)
   {
   Int16GlobalLoad testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt16GlobalLoad, testFunction);
   globalInt16 = 0;
   ASSERT_EQ(testFunction(), 1);
   globalInt16 = INT16_MAX - 1;
   ASSERT_EQ(testFunction(), INT16_MAX);
   globalInt16 = -1;
   ASSERT_EQ(testFunction(), 0);
   globalInt16 = INT16_MIN;
   ASSERT_EQ(testFunction(), INT16_MIN + 1);
   }

typedef void (*Int16GlobalStore)(int16_t);
TEST_F(GlobalTest, Int16_Store_Test)
   {
   Int16GlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt16GlobalStore, testFunction);
   testFunction(1);
   ASSERT_EQ(globalInt16, 1);
   testFunction(INT16_MAX);
   ASSERT_EQ(globalInt16, INT16_MAX);
   testFunction(0);
   ASSERT_EQ(globalInt16, 0);
   testFunction(INT16_MIN);
   ASSERT_EQ(globalInt16, INT16_MIN);
   }

typedef int8_t (*Int8Global)(int8_t);
TEST_F(GlobalTest, Int8_Test)
   {
   Int8Global testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt8Global, testFunction);
   ASSERT_EQ(testFunction(0), 1);
   ASSERT_EQ(testFunction(INT8_MAX - 1), INT8_MAX);
   ASSERT_EQ(testFunction(-1), 0);
   ASSERT_EQ(testFunction(INT8_MIN), INT8_MIN + 1);
   }

typedef int8_t (*Int8GlobalLoad)();
TEST_F(GlobalTest, Int8_Load_Test)
   {
   Int8GlobalLoad testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt8GlobalLoad, testFunction);
   globalInt8 = 0;
   ASSERT_EQ(testFunction(), 1);
   globalInt8 = INT8_MAX - 1;
   ASSERT_EQ(testFunction(), INT8_MAX);
   globalInt8 = -1;
   ASSERT_EQ(testFunction(), 0);
   globalInt8 = INT8_MIN;
   ASSERT_EQ(testFunction(), INT8_MIN + 1);
   }

typedef void (*Int8GlobalStore)(int8_t);
TEST_F(GlobalTest, Int8_Store_Test)
   {
   Int8GlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestInt8GlobalStore, testFunction);
   testFunction(1);
   ASSERT_EQ(globalInt8, 1);
   testFunction(INT8_MAX);
   ASSERT_EQ(globalInt8, INT8_MAX);
   testFunction(0);
   ASSERT_EQ(globalInt8, 0);
   testFunction(INT8_MIN);
   ASSERT_EQ(globalInt8, INT8_MIN);
   }

typedef float (*FloatGlobal)(float);
TEST_F(GlobalTest, Float_Test)
   {
   FloatGlobal testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestFloatGlobal, testFunction);
   ASSERT_EQ(testFunction(0.0f), 1.0f);
   ASSERT_EQ(testFunction(3.125f - 1), 3.125f);
   ASSERT_EQ(testFunction(-1.0f), 0.0f);
   ASSERT_EQ(testFunction(127.0f), 127.0f + 1);
   }

typedef float (*FloatGlobalLoad)();
TEST_F(GlobalTest, Float_Load_Test)
   {
   FloatGlobalLoad testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestFloatGlobalLoad, testFunction);
   globalFloat = 0.0f;
   ASSERT_EQ(testFunction(), 1.0f);
   globalFloat = 3.125f - 1;
   ASSERT_EQ(testFunction(), 3.125f);
   globalFloat = -1.0f;
   ASSERT_EQ(testFunction(), 0.0f);
   globalFloat = 127.0f;
   ASSERT_EQ(testFunction(), 127.0f + 1);
   }

typedef void (*FloatGlobalStore)(float);
TEST_F(GlobalTest, Float_Store_Test)
   {
   FloatGlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestFloatGlobalStore, testFunction);
   testFunction(0.0f);
   ASSERT_EQ(globalFloat, 0.0f);
   testFunction(3.125f);
   ASSERT_EQ(globalFloat, 3.125f);
   testFunction(1.0f);
   ASSERT_EQ(globalFloat, 1.0f);
   testFunction(128.0f);
   ASSERT_EQ(globalFloat, 128.0f);
   }

typedef double (*DoubleGlobal)(double);
TEST_F(GlobalTest, Double_Test)
   {
   DoubleGlobal testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestDoubleGlobal, testFunction);
   ASSERT_EQ(testFunction(0.0), 1.0);
   ASSERT_EQ(testFunction(3.125 - 1), 3.125);
   ASSERT_EQ(testFunction(-1.0), 0.0);
   ASSERT_EQ(testFunction(2047.0), 2047.0 + 1);
   }

typedef double (*DoubleGlobalLoad)();
TEST_F(GlobalTest, Double_Load_Test)
   {
   DoubleGlobalLoad testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestDoubleGlobalLoad, testFunction);
   globalDouble = 0.0;
   ASSERT_EQ(testFunction(), 1.0);
   globalDouble = 3.125 - 1;
   ASSERT_EQ(testFunction(), 3.125);
   globalDouble = -1.0;
   ASSERT_EQ(testFunction(), 0.0);
   globalDouble = 2047.0;
   ASSERT_EQ(testFunction(), 2047.0 + 1);
   }

typedef void (*DoubleGlobalStore)(double);
TEST_F(GlobalTest, Double_Store_Test)
   {
   DoubleGlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestDoubleGlobalStore, testFunction);
   testFunction(0.0);
   ASSERT_EQ(globalDouble, 0.0);
   testFunction(3.125);
   ASSERT_EQ(globalDouble, 3.125);
   testFunction(1.0);
   ASSERT_EQ(globalDouble, 1.0);
   testFunction(128.0);
   ASSERT_EQ(globalDouble, 128.0);
   }

typedef int64_t (*AddressGlobal)(int64_t);
TEST_F(GlobalTest, Address_Test)
   {
   AddressGlobal testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestAddressGlobal, testFunction);
   globalAddress = &globalInt64;
   globalInt64 = 0;
   ASSERT_EQ(testFunction(1), 1);
   globalInt64 = 0xfffffffffffffffe;
   ASSERT_EQ(testFunction(1), 0xffffffffffffffff);
   globalInt64 = 0xffffffffffffffff;
   ASSERT_EQ(testFunction(1), 0);
   }

typedef void (*AddressGlobalStore)(void*);
TEST_F(GlobalTest, Address_Store_Test)
   {
   AddressGlobalStore testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestAddressGlobalStore, testFunction);
   testFunction(&globalInt64);
   ASSERT_EQ(globalAddress, &globalInt64);
   testFunction(&globalInt32);
   ASSERT_EQ(globalAddress, &globalInt32);
   testFunction(&globalFloat);
   ASSERT_EQ(globalAddress, &globalFloat);
   testFunction(&globalDouble);
   ASSERT_EQ(globalAddress, &globalDouble);
   }
