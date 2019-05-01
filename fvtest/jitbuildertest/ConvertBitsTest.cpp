/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

class BitConversionTest : public JitBuilderTest {};

typedef float (BitConvertInt2FloatFunction)(uint32_t);

DEFINE_BUILDER( BitConvertInt2Float,
                Float,
                PARAM("p", Int32) )
   {
   Return(
      ConvertBitsTo(Float, Load("p")));
   return true;
   }

TEST_F(BitConversionTest, Int2Float)
   {
   BitConvertInt2FloatFunction* int2float;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, BitConvertInt2Float, int2float);

   ASSERT_EQ(0.0f, int2float(0));
   ASSERT_EQ(-1.0f, int2float(0xbf800000U));
   ASSERT_EQ(3.125f, int2float(0x40480000U));
   ASSERT_NE(127.0f, int2float(127));
   }

typedef uint32_t (BitConvertFloat2IntFunction)(float);

DEFINE_BUILDER( BitConvertFloat2Int,
                Int32,
                PARAM("p", Float) )
   {
   Return(
      ConvertBitsTo(Int32, Load("p")));
   return true;
   }

TEST_F(BitConversionTest, Float2Int)
   {
   BitConvertFloat2IntFunction* float2int;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, BitConvertFloat2Int, float2int);

   ASSERT_EQ(UINT32_C(0), float2int(0.0f));
   ASSERT_EQ(UINT32_C(0xbf800000), float2int(-1.0f));
   ASSERT_EQ(UINT32_C(0x40480000), float2int(3.125f));
   ASSERT_NE(UINT32_C(127), float2int(127.0f));
   }

typedef double (BitConvertLong2DoubleFunction)(uint64_t);

DEFINE_BUILDER( BitConvertLong2Double,
                Double,
                PARAM("p", Int64) )
   {
   Return(
      ConvertBitsTo(Double, Load("p")));
   return true;
   }

TEST_F(BitConversionTest, Long2Double)
   {
   BitConvertLong2DoubleFunction* long2double;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, BitConvertLong2Double, long2double);

   ASSERT_EQ(0.0, long2double(0));
   ASSERT_EQ(-1.0, long2double(0xbff0000000000000ULL));
   ASSERT_EQ(3.125, long2double(0x4009000000000000ULL));
   ASSERT_NE(2047.0, long2double(2047));
   }

typedef uint64_t (BitConvertDouble2LongFunction)(double);

DEFINE_BUILDER( BitConvertDouble2Long,
                Int64,
                PARAM("p", Double) )
   {
   Return(
      ConvertBitsTo(Int64, Load("p")));
   return true;
   }

TEST_F(BitConversionTest, Double2Long)
   {
   BitConvertDouble2LongFunction* double2long;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, BitConvertDouble2Long, double2long);

   ASSERT_EQ(UINT64_C(0), double2long(0.0));
   ASSERT_EQ(UINT64_C(0xbff0000000000000), double2long(-1.0));
   ASSERT_EQ(UINT64_C(0x4009000000000000), double2long(3.125f));
   ASSERT_NE(UINT64_C(2047), double2long(2047.0f));
   }
