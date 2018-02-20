/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include <gtest/gtest.h>
#include "Jit.hpp"
#include "JitTest.hpp"
#include "default_compiler.hpp"

uint8_t byteValues[] = {
   0x00,
   0x01,
   0x80,
   0x81,
   0xF1,
   0xF0,
   0x0F,
   0xFF,
};
uint16_t shortValues[] = {
   0x0000,
   0x0001,
   0x80FF,
   0x0FF1,
   0x80F1,
   0xFF00,
   0x00FF,
   0xFFFF
};
uint32_t intValues[] = {
   0x00000000,
   0x00000001,
   0x80FF0FF0,
   0x0FFFFFFF,
   0xFF000FFF,
   0x7FFFF1FF,
   0xFFAFFBFF,
   0xFFFFFF0F,
   0X00000001,
   0xFFFFFFFF
};
uint64_t longValues[] = {
   0x0000000000000000,
   0x0000000000000001,
   0x00000000FFFF0FF0,
   0xFFFFFFF00FFFFFFF,
   0xFFFFFFFEFFFFFFFF,
   0xFFFFFFFE7FFFFFFF,
   0xF0FFFFFFFFFFFFFF,
   0xFFFFFFFFFFFFFF0F,
   0XFFFF000000000001,
   0x8000FFFFFFFFFFFF
};

const uint8_t all0[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const uint8_t all1[64] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
const uint8_t all63[64] = {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};
const uint8_t identical[64] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
const uint8_t reverse[64] = {63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
const uint8_t random1[64] = {30, 3, 13, 39, 60, 22, 23, 6, 22, 14, 63, 14, 32, 5, 25, 61, 57, 20, 27, 55, 42, 12, 1, 58, 29, 34, 53, 31, 62, 1, 59, 1, 38, 10, 52, 1, 2, 53, 49, 53, 22, 43, 21, 54, 10, 15, 34, 25, 30, 45, 26, 41, 55, 11, 57, 28, 48, 34, 29, 1, 55, 5, 12, 2};
const uint8_t random2[64] = {43, 6, 19, 36, 51, 55, 9, 50, 48, 24, 12, 47, 32, 50, 41, 7, 19, 16, 18, 41, 62, 20, 49, 5, 9, 26, 2, 56, 1, 15, 19, 57, 63, 41, 58, 43, 3, 38, 13, 31, 29, 18, 52, 55, 27, 23, 49, 49, 38, 13, 31, 27, 27, 6, 25, 57, 38, 39, 21, 35, 61, 39, 45, 43};
const uint8_t random3[64] = {0, 43, 14, 37, 13, 44, 45, 19, 62, 59, 42, 22, 15, 18, 46, 45, 31, 12, 59, 6, 22, 1, 2, 42, 45, 60, 42, 17, 21, 18, 46, 2, 26, 49, 3, 42, 53, 18, 12, 38, 51, 28, 1, 47, 60, 19, 39, 54, 9, 25, 63, 30, 41, 18, 29, 57, 62, 47, 36, 24, 7, 12, 61, 32};
const uint8_t *testArrays[] = {all0, all1, all63, identical, reverse, random1, random2, random3};

const uint32_t byteLengths[]  = {0, 1, 3, 8};
const uint32_t shortLengths[] = {0, 1, 11, 16};
const uint32_t intLengths[]   = {0, 1, 19, 32};
const uint32_t longLengths[]  = {0, 1, 35, 64};

template <typename VarType>
class BitPermuteTest : public ::testing::TestWithParam<std::tuple<VarType, const uint8_t*, uint32_t, VarType (*) (VarType, const uint8_t*, uint32_t)>>
   {
   public:

   static void SetUpTestCase()
      {
      const char *options = "-Xjit:acceptHugeMethods,enableBasicBlockHoisting,omitFramePointer,useILValidator,paranoidoptcheck,disableAsyncCompilation";
      auto initSuccess = initializeJitWithOptions(const_cast<char*>(options));

      ASSERT_TRUE(initSuccess) << "Failed to initialize JIT.";
      }

   static void TearDownTestCase()
      {
      shutdownJit();
      }
   };

template <typename VarType>
struct BitPermuteStruct
   {
   VarType variableValue;
   const uint8_t* arrayAddress;
   uint32_t arrayLength;
   VarType (*oracle)(VarType, const uint8_t*, uint32_t);
   };

template <typename VarType>
BitPermuteStruct<VarType> to_struct(const std::tuple<VarType, const uint8_t*, uint32_t, VarType (*) (VarType, const uint8_t*, uint32_t)> &param)
   {
   BitPermuteStruct<VarType> s;

   s.variableValue = std::get<0>(param);
   s.arrayAddress = std::get<1>(param);
   s.arrayLength = std::get<2>(param);
   s.oracle = std::get<3>(param);

   return s;
   }

template <typename VarType>
static VarType bitPermuteOracle(VarType val, const uint8_t *array, uint32_t length)
   {
   VarType result = 0;
   for (uint32_t i = 0; i < length; ++i)
      result |= (((val >> array[i]) & 1) << i);
   return result;
   }

class lBitPermuteTest : public BitPermuteTest<uint64_t> {};

TEST_P(lBitPermuteTest, ConstAddressLengthTest)
   {
   auto param = to_struct(GetParam());

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64]  "
      " (block                            "
      "  (lreturn                         "
      "  (lbitpermute                     "
      "   (lload parm=0)                  "
      "   (aconst %llu)                   "
      "   (iconst %llu)                   "
      "  ))))                             ",
      param.arrayAddress,
      param.arrayLength
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.arrayAddress, param.arrayLength),
      entry_point(param.variableValue));
   }

TEST_P(lBitPermuteTest, ConstAddressTest)
   {
   auto param = to_struct(GetParam());

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64, Int32]  "
      " (block                                   "
      "  (lreturn                                "
      "  (lbitpermute                            "
      "   (lload parm=0)                         "
      "   (aconst %llu)                          "
      "   (iload parm=1)                         "
      "  ))))                                    ",
      param.arrayAddress
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.arrayAddress, param.arrayLength),
      entry_point(param.variableValue, param.arrayLength));
   }

TEST_P(lBitPermuteTest, NoConstTest)
   {
   auto param = to_struct(GetParam());

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64, Address, Int32]  "
      " (block                                            "
      "  (lreturn                                         "
      "  (lbitpermute                                     "
      "   (lload parm=0)                                  "
      "   (aload parm=1)                                  "
      "   (iload parm=2)                                  "
      "  ))))                                             "
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t, const uint8_t*, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.arrayAddress, param.arrayLength),
      entry_point(param.variableValue, param.arrayAddress, param.arrayLength));
   }

class iBitPermuteTest : public BitPermuteTest<uint32_t> {};

TEST_P(iBitPermuteTest, ConstAddressLengthTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[32];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0x1F;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32]  "
      " (block                            "
      "  (ireturn                         "
      "  (ibitpermute                     "
      "   (iload parm=0)                  "
      "   (aconst %llu)                   "
      "   (iconst %llu)                   "
      "  ))))                             ",
      maskedIndices,
      param.arrayLength
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue));
   }

TEST_P(iBitPermuteTest, ConstAddressTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[32];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0x1F;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32, Int32]  "
      " (block                                   "
      "  (ireturn                                "
      "  (ibitpermute                            "
      "   (iload parm=0)                         "
      "   (aconst %llu)                          "
      "   (iload parm=1)                         "
      "  ))))                                    ",
      maskedIndices
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue, param.arrayLength));
   }

TEST_P(iBitPermuteTest, NoConstTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[32];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0x1F;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32, Address, Int32]  "
      " (block                                            "
      "  (ireturn                                         "
      "  (ibitpermute                                     "
      "   (iload parm=0)                                  "
      "   (aload parm=1)                                  "
      "   (iload parm=2)                                  "
      "  ))))                                             "
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t, uint8_t*, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue, maskedIndices, param.arrayLength));
   }

class sBitPermuteTest : public BitPermuteTest<uint16_t> {};

TEST_P(sBitPermuteTest, ConstAddressLengthTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[16];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0xF;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int16]  "
      " (block                            "
      "  (ireturn                         "
      "  (su2i                            "
      "  (sbitpermute                     "
      "   (sload parm=0)                  "
      "   (aconst %llu)                   "
      "   (iconst %llu)                   "
      "  )))))                            ",
      maskedIndices,
      param.arrayLength
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint16_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue));
   }

TEST_P(sBitPermuteTest, ConstAddressTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[16];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0xF;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int16, Int32]  "
      " (block                                   "
      "  (ireturn                                "
      "  (su2i                                   "
      "  (sbitpermute                            "
      "   (sload parm=0)                         "
      "   (aconst %llu)                          "
      "   (iload parm=1)                         "
      "  )))))                                   ",
      maskedIndices
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint16_t, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue, param.arrayLength));
   }

TEST_P(sBitPermuteTest, NoConstTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[16];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0xF;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int16, Address, Int32]  "
      " (block                                            "
      "  (ireturn                                         "
      "  (su2i                                            "
      "  (sbitpermute                                     "
      "   (sload parm=0)                                  "
      "   (aload parm=1)                                  "
      "   (iload parm=2)                                  "
      "  )))))                                            "
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint16_t, uint8_t*, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue, maskedIndices, param.arrayLength));
   }

class bBitPermuteTest : public BitPermuteTest<uint8_t> {};

TEST_P(bBitPermuteTest, ConstAddressLengthTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[8];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0x7;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int8] "
      " (block                          "
      "  (ireturn                       "
      "  (bu2i                          "
      "  (bbitpermute                   "
      "   (bload parm=0)                "
      "   (aconst %llu)                 "
      "   (iconst %llu)                 "
      "  )))))                          ",
      maskedIndices,
      param.arrayLength
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint8_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue));
   }

TEST_P(bBitPermuteTest, ConstAddressTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[8];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0x7;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int8, Int32] "
      " (block                                 "
      "  (ireturn                              "
      "  (bu2i                                 "
      "  (bbitpermute                          "
      "   (bload parm=0)                       "
      "   (aconst %llu)                        "
      "   (iload parm=1)                       "
      "  )))))                                 ",
      maskedIndices
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint8_t, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue, param.arrayLength));
   }

TEST_P(bBitPermuteTest, NoConstTest)
   {
   auto param = to_struct(GetParam());

   uint8_t maskedIndices[8];
   for (size_t i = 0; i < sizeof(maskedIndices); ++i)
      maskedIndices[i] = param.arrayAddress[i] & 0x7;

   char inputTrees[512] = {0};
   std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int8, Address, Int32] "
      " (block                                          "
      "  (ireturn                                       "
      "  (bu2i                                          "
      "  (bbitpermute                                   "
      "   (bload parm=0)                                "
      "   (aload parm=1)                                "
      "   (iload parm=2)                                "
      "  )))))                                          "
      );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler {trees};

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint8_t, uint8_t*, uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, (const uint8_t*)maskedIndices, param.arrayLength),
      entry_point(param.variableValue, maskedIndices, param.arrayLength));
   }

#if defined (TR_TARGET_X86) || defined (TR_TARGET_S390)
INSTANTIATE_TEST_CASE_P(lBitPermute, lBitPermuteTest,
              ::testing::Combine(
                 ::testing::ValuesIn(longValues),
                 ::testing::ValuesIn(testArrays),
                 ::testing::ValuesIn(longLengths),
                 ::testing::Values(static_cast<uint64_t (*) (uint64_t, const uint8_t*, uint32_t)> (bitPermuteOracle))));

INSTANTIATE_TEST_CASE_P(iBitPermute, iBitPermuteTest,
              ::testing::Combine(
                 ::testing::ValuesIn(intValues),
                 ::testing::ValuesIn(testArrays),
                 ::testing::ValuesIn(intLengths),
                 ::testing::Values(static_cast<uint32_t (*) (uint32_t, const uint8_t*, uint32_t)> (bitPermuteOracle))));

INSTANTIATE_TEST_CASE_P(sBitPermute, sBitPermuteTest,
              ::testing::Combine(
                 ::testing::ValuesIn(shortValues),
                 ::testing::ValuesIn(testArrays),
                 ::testing::ValuesIn(shortLengths),
                 ::testing::Values(static_cast<uint16_t (*) (uint16_t, const uint8_t*, uint32_t)> (bitPermuteOracle))));

INSTANTIATE_TEST_CASE_P(bBitPermute, bBitPermuteTest,
              ::testing::Combine(
                 ::testing::ValuesIn(byteValues),
                 ::testing::ValuesIn(testArrays),
                 ::testing::ValuesIn(byteLengths),
                 ::testing::Values(static_cast<uint8_t (*) (uint8_t, const uint8_t*, uint32_t)> (bitPermuteOracle))));
#endif
