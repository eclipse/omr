/*******************************************************************************
 * Copyright (c) 2019 IBM Corp. and others
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

DEFINE_BUILDER(TestUInt64Div,
               Int64,
               PARAM("lhs", Int64),
               PARAM("rhs", Int64))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedDiv(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt64Rem,
               Int64,
               PARAM("lhs", Int64),
               PARAM("rhs", Int64))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedRem(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt32Div,
               Int32,
               PARAM("lhs", Int32),
               PARAM("rhs", Int32))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedDiv(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt32Rem,
               Int32,
               PARAM("lhs", Int32),
               PARAM("rhs", Int32))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedRem(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt16Div,
               Int16,
               PARAM("lhs", Int16),
               PARAM("rhs", Int16))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedDiv(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt16Rem,
               Int16,
               PARAM("lhs", Int16),
               PARAM("rhs", Int16))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedRem(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt8Div,
               Int8,
               PARAM("lhs", Int8),
               PARAM("rhs", Int8))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedDiv(lhs, rhs);
   Return(result);
   return true;
   }

DEFINE_BUILDER(TestUInt8Rem,
               Int8,
               PARAM("lhs", Int8),
               PARAM("rhs", Int8))
   {
   OMR::JitBuilder::IlValue *lhs = Load("lhs");
   OMR::JitBuilder::IlValue *rhs = Load("rhs");
   OMR::JitBuilder::IlValue *result = UnsignedRem(lhs, rhs);
   Return(result);
   return true;
   }

class UnsignedDivTest : public JitBuilderTest {};
class UnsignedRemTest : public JitBuilderTest {};

typedef uint64_t (*UInt64ReturnType)(uint64_t, uint64_t);
TEST_F(UnsignedDivTest, UInt64_Test)
   {
   UInt64ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt64Div, testFunction);
   ASSERT_EQ(testFunction(1, 1), 1);
   ASSERT_EQ(testFunction(1, 2), 0);
   ASSERT_EQ(testFunction(10, 3), 3);
   ASSERT_EQ(testFunction(static_cast<uint64_t>(INT64_MIN), static_cast<uint64_t>(-1)), 0);
   ASSERT_EQ(testFunction(UINT64_MAX, 1), UINT64_MAX);
   ASSERT_EQ(testFunction(UINT64_MAX, UINT64_MAX), 1);
   ASSERT_EQ(testFunction(UINT64_MAX - 1, UINT64_MAX), 0);
   ASSERT_EQ(testFunction(UINT64_MAX, UINT64_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint64_t>(INT64_MAX), UINT64_MAX), 0);
   ASSERT_EQ(testFunction(UINT64_MAX, 10), UINT64_MAX / 10);
   }

TEST_F(UnsignedRemTest, UInt64_Test)
   {
   UInt64ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt64Rem, testFunction);
   ASSERT_EQ(testFunction(1, 1), 0);
   ASSERT_EQ(testFunction(1, 2), 1);
   ASSERT_EQ(testFunction(10, 3), 1);
   ASSERT_EQ(testFunction(static_cast<uint64_t>(INT64_MIN), static_cast<uint64_t>(-1)), static_cast<uint64_t>(INT64_MIN));
   ASSERT_EQ(testFunction(UINT64_MAX, 1), 0);
   ASSERT_EQ(testFunction(UINT64_MAX, UINT64_MAX), 0);
   ASSERT_EQ(testFunction(UINT64_MAX - 1, UINT64_MAX), UINT64_MAX - 1);
   ASSERT_EQ(testFunction(UINT64_MAX, UINT64_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint64_t>(INT64_MAX), UINT64_MAX), static_cast<uint64_t>(INT64_MAX));
   ASSERT_EQ(testFunction(UINT64_MAX, 10), UINT64_MAX % 10);
   }

typedef uint32_t (*UInt32ReturnType)(uint32_t, uint32_t);
TEST_F(UnsignedDivTest, UInt32_Test)
   {
   UInt32ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt32Div, testFunction);
   ASSERT_EQ(testFunction(1, 1), 1);
   ASSERT_EQ(testFunction(1, 2), 0);
   ASSERT_EQ(testFunction(10, 3), 3);
   ASSERT_EQ(testFunction(static_cast<uint32_t>(INT32_MIN), static_cast<uint32_t>(-1)), 0);
   ASSERT_EQ(testFunction(UINT32_MAX, 1), UINT32_MAX);
   ASSERT_EQ(testFunction(UINT32_MAX, UINT32_MAX), 1);
   ASSERT_EQ(testFunction(UINT32_MAX - 1, UINT32_MAX), 0);
   ASSERT_EQ(testFunction(UINT32_MAX, UINT32_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint32_t>(INT32_MAX), UINT32_MAX), 0);
   ASSERT_EQ(testFunction(UINT32_MAX, 10), UINT32_MAX / 10);
   }


TEST_F(UnsignedRemTest, UInt32_Test)
   {
   UInt32ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt32Rem, testFunction);
   ASSERT_EQ(testFunction(1, 1), 0);
   ASSERT_EQ(testFunction(1, 2), 1);
   ASSERT_EQ(testFunction(10, 3), 1);
   ASSERT_EQ(testFunction(static_cast<uint32_t>(INT32_MIN), static_cast<uint32_t>(-1)), static_cast<uint32_t>(INT32_MIN));
   ASSERT_EQ(testFunction(UINT32_MAX, 1), 0);
   ASSERT_EQ(testFunction(UINT32_MAX, UINT32_MAX), 0);
   ASSERT_EQ(testFunction(UINT32_MAX - 1, UINT32_MAX), UINT32_MAX - 1);
   ASSERT_EQ(testFunction(UINT32_MAX, UINT32_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint32_t>(INT32_MAX), UINT32_MAX), static_cast<uint32_t>(INT32_MAX));
   ASSERT_EQ(testFunction(UINT32_MAX, 10), UINT32_MAX % 10);
   }

typedef uint16_t (*UInt16ReturnType)(uint16_t, uint16_t);
TEST_F(UnsignedDivTest, UInt16_Test)
   {
   UInt16ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt16Div, testFunction);
   ASSERT_EQ(testFunction(1, 1), 1);
   ASSERT_EQ(testFunction(1, 2), 0);
   ASSERT_EQ(testFunction(10, 3), 3);
   ASSERT_EQ(testFunction(static_cast<uint16_t>(INT16_MIN), static_cast<uint16_t>(-1)), 0);
   ASSERT_EQ(testFunction(UINT16_MAX, 1), UINT16_MAX);
   ASSERT_EQ(testFunction(UINT16_MAX, UINT16_MAX), 1);
   ASSERT_EQ(testFunction(UINT16_MAX - 1, UINT16_MAX), 0);
   ASSERT_EQ(testFunction(UINT16_MAX, UINT16_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint16_t>(INT16_MAX), UINT16_MAX), 0);
   ASSERT_EQ(testFunction(UINT16_MAX, 10), UINT16_MAX / 10);
   }


TEST_F(UnsignedRemTest, UInt16_Test)
   {
   UInt16ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt16Rem, testFunction);
   ASSERT_EQ(testFunction(1, 1), 0);
   ASSERT_EQ(testFunction(1, 2), 1);
   ASSERT_EQ(testFunction(10, 3), 1);
   ASSERT_EQ(testFunction(static_cast<uint16_t>(INT16_MIN), static_cast<uint16_t>(-1)), static_cast<uint16_t>(INT16_MIN));
   ASSERT_EQ(testFunction(UINT16_MAX, 1), 0);
   ASSERT_EQ(testFunction(UINT16_MAX, UINT16_MAX), 0);
   ASSERT_EQ(testFunction(UINT16_MAX - 1, UINT16_MAX), UINT16_MAX - 1);
   ASSERT_EQ(testFunction(UINT16_MAX, UINT16_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint16_t>(INT16_MAX), UINT16_MAX), static_cast<uint16_t>(INT16_MAX));
   ASSERT_EQ(testFunction(UINT16_MAX, 10), UINT16_MAX % 10);
   }

typedef uint8_t (*UInt8ReturnType)(uint8_t, uint8_t);
TEST_F(UnsignedDivTest, UInt8_Test)
   {
   UInt8ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt8Div, testFunction);
   ASSERT_EQ(testFunction(1, 1), 1);
   ASSERT_EQ(testFunction(1, 2), 0);
   ASSERT_EQ(testFunction(10, 3), 3);
   ASSERT_EQ(testFunction(static_cast<uint8_t>(INT8_MIN), static_cast<uint8_t>(-1)), 0);
   ASSERT_EQ(testFunction(UINT8_MAX, 1), UINT8_MAX);
   ASSERT_EQ(testFunction(UINT8_MAX, UINT8_MAX), 1);
   ASSERT_EQ(testFunction(UINT8_MAX - 1, UINT8_MAX), 0);
   ASSERT_EQ(testFunction(UINT8_MAX, UINT8_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint8_t>(INT8_MAX), UINT8_MAX), 0);
   ASSERT_EQ(testFunction(UINT8_MAX, 10), UINT8_MAX / 10);
   }


TEST_F(UnsignedRemTest, UInt8_Test)
   {
   UInt8ReturnType testFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, TestUInt8Rem, testFunction);
   ASSERT_EQ(testFunction(1, 1), 0);
   ASSERT_EQ(testFunction(1, 2), 1);
   ASSERT_EQ(testFunction(10, 3), 1);
   ASSERT_EQ(testFunction(static_cast<uint8_t>(INT8_MIN), static_cast<uint8_t>(-1)), static_cast<uint8_t>(INT8_MIN));
   ASSERT_EQ(testFunction(UINT8_MAX, 1), 0);
   ASSERT_EQ(testFunction(UINT8_MAX, UINT8_MAX), 0);
   ASSERT_EQ(testFunction(UINT8_MAX - 1, UINT8_MAX), UINT8_MAX - 1);
   ASSERT_EQ(testFunction(UINT8_MAX, UINT8_MAX - 1), 1);
   ASSERT_EQ(testFunction(static_cast<uint8_t>(INT8_MAX), UINT8_MAX), static_cast<uint8_t>(INT8_MAX));
   ASSERT_EQ(testFunction(UINT8_MAX, 10), UINT8_MAX % 10);
   }

