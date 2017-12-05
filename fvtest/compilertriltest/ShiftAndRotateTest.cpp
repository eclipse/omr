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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "OpCodeTest.hpp"
#include "default_compiler.hpp"

template <typename T> static
std::vector<std::tuple<T, int32_t>> test_input_values()
   {
   int32_t inputArray[] = {0, 1, 5, 8, 25, 8*sizeof(T) - 1, 8*sizeof(T)};
   return TRTest::combine(TRTest::const_values<T>(), std::vector<int32_t>(inputArray, inputArray + sizeof(inputArray)/sizeof(int32_t)));
   }



template <typename T> static T rotate(T a, int32_t b)
   {
   bool isOne = false;
   int32_t wordSize = sizeof(a);
   int32_t bitMask = 8;
   while (1 != wordSize)
      {
      bitMask = bitMask << 1;
      wordSize = wordSize >> 1;
      }
   bitMask -= 1;
   int32_t rotateNumber = b & bitMask;
   for (int i = 0; i < rotateNumber; i++)
      {
      if (a < 0)
         {
         isOne = true;
         }
      a = a << 1;
      if (isOne)
         {
         a = a | 1;
         }
      isOne = false;
      }
   return a;
   }

template <typename Left> static Left shift_left(Left left, int32_t right)
   {
   int32_t r = right;
   if (sizeof(Left) <= sizeof(int32_t))
      {
      r = (r & (8 * sizeof(int32_t) - 1));
      }
   else
      {
      r = (r & (8 * sizeof(int64_t) - 1));
      }
   return left << r;
   }

template <typename Left> static Left shift_right(Left left, int32_t right)
   {
   int32_t r = right;
   if (sizeof(Left) <= sizeof(int32_t))
      {
      r = (r & (8 * sizeof(int32_t) - 1));
      }
   else
      {
      r = (r & (8 * sizeof(int64_t) - 1));
      }
   return left >> r;
   }

template <typename T>
class ShiftAndRotateArithmetic : public TRTest::OpCodeTest<T, T, int32_t> {};

class Int32ShiftAndRotate : public ShiftAndRotateArithmetic<int32_t> {};

TEST_P(Int32ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int32ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 args=[Int32, Int32] (block (ireturn (%s (iload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int32ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int32_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("ishl", static_cast<int32_t(*)(int32_t, int32_t)>(shift_left)),
                       std::make_tuple("ishr", static_cast<int32_t(*)(int32_t, int32_t)>(shift_right)),
                       std::make_tuple("irol", static_cast<int32_t(*)(int32_t, int32_t)>(rotate)) )));

class Int64ShiftAndRotate : public ShiftAndRotateArithmetic<int64_t> {};

TEST_P(Int64ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int64 (block (lreturn (%s (lconst %lld) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int64ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int64 args=[Int64, Int32] (block (lreturn (%s (lload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int64ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int64_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("lshl", static_cast<int64_t(*)(int64_t, int32_t)>(shift_left)),
                       std::make_tuple("lshr", static_cast<int64_t(*)(int64_t, int32_t)>(shift_right)),
                       std::make_tuple("lrol", static_cast<int64_t(*)(int64_t,int32_t)>(rotate)) )));

class Int8ShiftAndRotate : public ShiftAndRotateArithmetic<int8_t> {};

TEST_P(Int8ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 (block (ireturn (b2i (%s (bconst %d) (iconst %d)) ))))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int8ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 args=[Int8, Int32] (block (ireturn (b2i (%s (bload parm=0) (iload parm=1)) ))))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int8ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int8_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("bshl", static_cast<int8_t(*)(int8_t, int32_t)>(shift_left)),
                       std::make_tuple("bshr", static_cast<int8_t(*)(int8_t, int32_t)>(shift_right)) )));

class Int16ShiftAndRotate : public ShiftAndRotateArithmetic<int16_t> {};

TEST_P(Int16ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 (block (ireturn (s2i (%s (sconst %d) (iconst %d)) ))))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

#if !defined(TR_TARGET_POWER)
TEST_P(Int16ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 args=[Int16, Int32] (block (ireturn (s2i (%s (sload parm=0) (iload parm=1)) ))))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}
#endif

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int16ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int16_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("sshl", static_cast<int16_t(*)(int16_t, int32_t)>(shift_left)),
                       std::make_tuple("sshr", static_cast<int16_t(*)(int16_t, int32_t)>(shift_right))
                     )));

class UInt32ShiftAndRotate : public ShiftAndRotateArithmetic<uint32_t> {};

TEST_P(UInt32ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt32ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 args=[Int32, Int32] (block (ireturn (%s (iload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt32ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint32_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("iushr", static_cast< uint32_t (*) (uint32_t, int32_t) > (shift_right)) )));

class UInt64ShiftAndRotate : public ShiftAndRotateArithmetic<uint64_t> {};

TEST_P(UInt64ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int64 (block (lreturn (%s (lconst %llu) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt64ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int64 args=[Int64, Int32] (block (lreturn (%s (lload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt64ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint64_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("lushr", static_cast< uint64_t (*) (uint64_t, int32_t) > (shift_right)) )));

class UInt8ShiftAndRotate : public ShiftAndRotateArithmetic<uint8_t> {};

TEST_P(UInt8ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 (block (ireturn (bu2i (%s (bconst %d) (iconst %d)) ))))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt8ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 args=[Int8, Int32] (block (ireturn (bu2i (%s (bload parm=0) (iload parm=1)) ))))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(uint8_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt8ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn( static_cast< std::vector<std::tuple<uint8_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values( std::make_tuple("bushr", static_cast<uint8_t (*) (uint8_t, int32_t)>(shift_right)) )));

class UInt16ShiftAndRotate : public ShiftAndRotateArithmetic<uint16_t> {};

TEST_P(UInt16ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 (block (ireturn (su2i (%s (sconst %d) (iconst %d)) ))))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt16ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 args=[Int16, Int32] (block (ireturn (su2i (%s (sload parm=0) (iload parm=1)) ))))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(uint16_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt16ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint16_t, int32_t>> (*) (void) >(test_input_values)()),
    ::testing::Values( std::make_tuple("sushr", static_cast<uint16_t (*) (uint16_t, int32_t)>(shift_right)) )));
