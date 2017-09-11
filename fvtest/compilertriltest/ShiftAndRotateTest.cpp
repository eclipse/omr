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
#include "jitbuilder_compiler.hpp"

template <typename T> static
std::vector<std::tuple<T, int32_t>> test_input_values()
   {
   return TRTest::combine(TRTest::const_values<T>(), std::vector<int32_t>{0, 1, 5, 8, 25, 8*sizeof(T) - 1, 8*sizeof(T)});
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

template <typename T>
class ShiftAndRotateArithmetic : public TRTest::OpCodeTest<T, T, int32_t> {};

class Int32ShiftAndRotate : public ShiftAndRotateArithmetic<int32_t> {};

TEST_P(Int32ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

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

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int32ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<int32_t>()),
    ::testing::Values( std::make_tuple("ishl", [](int32_t l, int32_t r){return l << r;}),
                       std::make_tuple("ishr", [](int32_t l, int32_t r){return l >> r;}),
                       std::make_tuple("irol", rotate<int32_t >) )));

class Int64ShiftAndRotate : public ShiftAndRotateArithmetic<int64_t> {};

TEST_P(Int64ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int64 (block (lreturn (%s (lconst %ld) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

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

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int64ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<int64_t>()),
    ::testing::Values( std::make_tuple("lshl", [](int64_t l, int32_t r){return l << r;}),
                       std::make_tuple("lshr", [](int64_t l, int32_t r){return l >> r;}),
                       std::make_tuple("lrol", rotate<int64_t >) )));

class Int8ShiftAndRotate : public ShiftAndRotateArithmetic<int8_t> {};

TEST_P(Int8ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 (block (ireturn (%s (bconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int8ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 args=[Int8, Int32] (block (ireturn (%s (bload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int8ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<int8_t>()),
    ::testing::Values( std::make_tuple("bshl", [](int8_t l, int32_t r){return static_cast<int8_t>(l << r);}),
                       std::make_tuple("bshr", [](int8_t l, int32_t r){return static_cast<int8_t>(l >> r);}) )));

class Int16ShiftAndRotate : public ShiftAndRotateArithmetic<int16_t> {};

TEST_P(Int16ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 (block (ireturn (%s (sconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int16ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 args=[Int16, Int32] (block (ireturn (%s (sload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int16ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<int16_t>()),
    ::testing::Values( std::make_tuple("sshl", [](int16_t l, int32_t r){return static_cast<int16_t>(l << r);}),
                       std::make_tuple("sshr", [](int16_t l, int32_t r){return static_cast<int16_t>(l >> r);}) )));

class UInt32ShiftAndRotate : public ShiftAndRotateArithmetic<uint32_t> {};

TEST_P(UInt32ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

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

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt32ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<uint32_t>()),
    ::testing::Values( std::make_tuple("iushr", [](uint32_t l, int32_t r){return l >> r;}) )));

class UInt64ShiftAndRotate : public ShiftAndRotateArithmetic<uint64_t> {};

TEST_P(UInt64ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int64 (block (lreturn (%s (lconst %ld) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

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

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt64ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<uint64_t>()),
    ::testing::Values( std::make_tuple("lushr", [](uint64_t l, int32_t r){return l >> r;}) )));

class UInt8ShiftAndRotate : public ShiftAndRotateArithmetic<uint8_t> {};

TEST_P(UInt8ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 (block (ireturn (%s (bconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt8ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int8 args=[Int8, Int32] (block (ireturn (%s (bload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(uint8_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt8ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<uint8_t>()),
    ::testing::Values( std::make_tuple("bushr", [](uint8_t l, int32_t r){return static_cast<uint8_t>(l >> r);}) )));

class UInt16ShiftAndRotate : public ShiftAndRotateArithmetic<uint16_t> {};

TEST_P(UInt16ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 (block (ireturn (%s (sconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt16ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int16 args=[Int16, Int32] (block (ireturn (%s (sload parm=0) (iload parm=1)) )))", param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(uint16_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt16ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(test_input_values<uint16_t>()),
    ::testing::Values( std::make_tuple("sushr", [](uint16_t l, int32_t r){return static_cast<uint16_t>(l >> r);}) )));
