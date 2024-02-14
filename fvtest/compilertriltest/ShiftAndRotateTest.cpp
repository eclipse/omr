/*******************************************************************************
 * Copyright IBM Corp. and others 2017
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "OpCodeTest.hpp"
#include "default_compiler.hpp"
#include "omrformatconsts.h"

template <typename T> static
std::vector<int32_t> test_shifts()
    {
    int32_t shiftArray[] = {0, 1, 5, 4*sizeof(T), 8*sizeof(T) - 5, 8*sizeof(T) - 1};
    return std::vector<int32_t>(shiftArray, shiftArray + sizeof(shiftArray)/sizeof(int32_t));
    }

template <typename T> static
std::vector<std::tuple<T, int32_t>> test_input_values()
   {
   return TRTest::combine(TRTest::const_values<T>(), test_shifts<T>());
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
   if (right < 0 || right >= sizeof(Left) * 8)
      {
      throw std::runtime_error("Shift amount outside 0..width-1 range");
      }
   return left << right;
   }

template <typename Left> static Left shift_right(Left left, int32_t right)
   {
   if (right < 0 || right >= sizeof(Left) * 8)
      {
      throw std::runtime_error("Shift amount outside 0..width-1 range");
      }
   return left >> right;
   }

template <typename T>
class ShiftAndRotateArithmetic : public TRTest::OpCodeTest<T, T, int32_t> {};

class Int32ShiftAndRotate : public ShiftAndRotateArithmetic<int32_t> {};

TEST_P(Int32ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    Tril::format(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)))))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int32ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iload parm=0)"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int32ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iconst %d)"
        "        (iload parm=0)))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(Int32ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iload parm=0)"
        "        (iload parm=1)))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int32ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int32_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values(std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("ishl", static_cast<int32_t(*)(int32_t, int32_t)>(shift_left)),
                      std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("ishr", static_cast<int32_t(*)(int32_t, int32_t)>(shift_right)),
                      std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("irol", static_cast<int32_t(*)(int32_t, int32_t)>(rotate))
    )));

class Int64ShiftAndRotate : public ShiftAndRotateArithmetic<int64_t> {};

TEST_P(Int64ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRId64 ")"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int64ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0)"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int64ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRId64 ")"
        "        (iload parm=0)))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(Int64ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64, Int32]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0)"
        "        (iload parm=1)))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int64ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int64_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values(std::tuple<const char*, int64_t(*)(int64_t, int32_t)>("lshl", static_cast<int64_t(*)(int64_t, int32_t)>(shift_left)),
                      std::tuple<const char*, int64_t(*)(int64_t, int32_t)>("lshr", static_cast<int64_t(*)(int64_t, int32_t)>(shift_right)),
                      std::tuple<const char*, int64_t(*)(int64_t, int32_t)>("lrol", static_cast<int64_t(*)(int64_t,int32_t)>(rotate))
    )));

class Int8ShiftAndRotate : public ShiftAndRotateArithmetic<int8_t> {};

TEST_P(Int8ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (bconst %d)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int8ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (bload parm=0)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int8ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (bconst %d)"
        "          (iload parm=0))))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(Int8ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8, Int32]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (bload parm=0)"
        "          (iload parm=1))))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int8ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int8_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values(std::tuple<const char*, int8_t(*)(int8_t, int32_t)>("bshl", static_cast<int8_t(*)(int8_t, int32_t)>(shift_left)),
                      std::tuple<const char*, int8_t(*)(int8_t, int32_t)>("bshr", static_cast<int8_t(*)(int8_t, int32_t)>(shift_right))
    )));

class Int16ShiftAndRotate : public ShiftAndRotateArithmetic<int16_t> {};

TEST_P(Int16ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sconst %d)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int16ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sload parm=0)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int16ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sconst %d)"
        "          (iload parm=0))))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(Int16ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16, Int32]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sload parm=0)"
        "          (iload parm=1))))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int16ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<int16_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values(std::tuple<const char*, int16_t(*)(int16_t, int32_t)>("sshl", static_cast<int16_t(*)(int16_t, int32_t)>(shift_left)),
                      std::tuple<const char*, int16_t(*)(int16_t, int32_t)>("sshr", static_cast<int16_t(*)(int16_t, int32_t)>(shift_right))
    )));

class UInt32ShiftAndRotate : public ShiftAndRotateArithmetic<uint32_t> {};

TEST_P(UInt32ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iconst %u)"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt32ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iload parm=0)"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(UInt32ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iconst %u)"
        "        (iload parm=0)))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(UInt32ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iload parm=0)"
        "        (iload parm=1)))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt32ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint32_t, int32_t>> (*) (void)>(test_input_values)()),
    ::testing::Values(std::tuple<const char*, uint32_t (*) (uint32_t, int32_t)>("iushr", static_cast<uint32_t (*) (uint32_t, int32_t)>(shift_right))
    )));

class UInt64ShiftAndRotate : public ShiftAndRotateArithmetic<uint64_t> {};

TEST_P(UInt64ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRIu64 ")"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt64ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0)"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(UInt64ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRIu64 ")"
        "        (iload parm=0)))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(UInt64ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64, Int32]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0)"
        "        (iload parm=1)))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt64ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint64_t, int32_t>> (*) (void)>(test_input_values)()),
    ::testing::Values(std::tuple<const char*, uint64_t (*) (uint64_t, int32_t)>("lushr", static_cast<uint64_t (*) (uint64_t, int32_t)>(shift_right))
    )));

class UInt8ShiftAndRotate : public ShiftAndRotateArithmetic<uint8_t> {};

TEST_P(UInt8ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8"
        "  (block"
        "    (ireturn"
        "      (bu2i"
        "        (%s"
        "          (bconst %u)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt8ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8]"
        "  (block"
        "    (ireturn"
        "      (bu2i"
        "        (%s"
        "          (bload parm=0)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(uint8_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(UInt8ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (bu2i"
        "        (%s"
        "          (bconst %u)"
        "          (iload parm=0))))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(UInt8ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8, Int32]"
        "  (block"
        "    (ireturn"
        "      (bu2i"
        "        (%s"
        "          (bload parm=0)"
        "          (iload parm=1))))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(uint8_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt8ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint8_t, int32_t>> (*) (void) > (test_input_values)()),
    ::testing::Values(std::tuple<const char*, uint8_t (*) (uint8_t, int32_t)>("bushr", static_cast<uint8_t (*) (uint8_t, int32_t)>(shift_right))
    )));

class UInt16ShiftAndRotate : public ShiftAndRotateArithmetic<uint16_t> {};

TEST_P(UInt16ShiftAndRotate, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16"
        "  (block"
        "    (ireturn"
        "      (su2i"
        "        (%s"
        "          (sconst %u)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(UInt16ShiftAndRotate, UsingRhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16]"
        "  (block"
        "    (ireturn"
        "      (su2i"
        "        (%s"
        "          (sload parm=0)"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(uint16_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(UInt16ShiftAndRotate, UsingLhsConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (su2i"
        "        (%s"
        "          (sconst %u)"
        "          (iload parm=0))))))",
        param.opcode.c_str(),
        param.lhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.rhs));
}

TEST_P(UInt16ShiftAndRotate, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16, Int32]"
        "  (block"
        "    (ireturn"
        "      (su2i"
        "        (%s"
        "          (sload parm=0)"
        "          (iload parm=1))))))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(uint16_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt16ShiftAndRotate, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<uint16_t, int32_t>> (*) (void) >(test_input_values)()),
    ::testing::Values(std::tuple<const char*, uint16_t (*) (uint16_t, int32_t)>("sushr", static_cast<uint16_t (*) (uint16_t, int32_t)>(shift_right))
    )));

template <typename T> static
std::vector<T> test_masks();

static uint64_t mask64Values[] =
    {
    0x0000000000000000,
    0x0000000000000001,
    0x0000000000000002,
    0x000000000000003e,
    0x000000000000ffff,
    0x00000000ffffffff,
    0x00000003c0000000,
    0x0000003f00000000,
    0x00007fffffffffff,
    0x4000000000000000,
    0x4000000000000001,
    0x7000000000000000,
    0x7c00000000000000,
    0x7fffffffffffffff,
    0x8000000000000000,
    0x8000000000000002,
    0xf0f0f0f0f0f0f0f0,
    0xffff000000000000,
    0xffffffff00000000,
    0xffffffffffff0000,
    0xffffffffffffffff
    };

template <>
std::vector<uint64_t> test_masks<uint64_t>()
    {
    return std::vector<uint64_t>(mask64Values, mask64Values + sizeof(mask64Values)/sizeof(*mask64Values));
    }

template <>
std::vector<int64_t> test_masks<int64_t>()
    {
    return std::vector<int64_t>(mask64Values, mask64Values + sizeof(mask64Values)/sizeof(*mask64Values));
    }

static uint32_t mask32Values[] =
    {
    0x00000000,
    0xffffffff,
    0x7fffffff,
    0x80000000,
    0x003f0000,
    0xffff0000,
    0x0000ffff,
    0xff0000ff
    };

template <>
std::vector<uint32_t> test_masks<uint32_t>()
    {
    return std::vector<uint32_t>(mask32Values, mask32Values + sizeof(mask32Values)/sizeof(*mask32Values));
    }

template <>
std::vector<int32_t> test_masks<int32_t>()
    {
    return std::vector<int32_t>(mask32Values, mask32Values + sizeof(mask32Values)/sizeof(*mask32Values));
    }

static uint16_t mask16Values[] =
    {
    0x0000,
    0xffff,
    0x7fff,
    0x8000,
    0x03f0,
    0xff00,
    0x00ff,
    0xf00f
    };

template <>
std::vector<uint16_t> test_masks<uint16_t>()
    {
    return std::vector<uint16_t>(mask16Values, mask16Values + sizeof(mask16Values)/sizeof(*mask16Values));
    }

template <>
std::vector<int16_t> test_masks<int16_t>()
    {
    return std::vector<int16_t>(mask16Values, mask16Values + sizeof(mask16Values)/sizeof(*mask16Values));
    }

static uint8_t mask8Values[] =
    {
    0x00,
    0xff,
    0x7f,
    0x80,
    0x3c,
    0xf0,
    0x0f,
    0xc3
    };

template <>
std::vector<uint8_t> test_masks<uint8_t>()
    {
    return std::vector<uint8_t>(mask8Values, mask8Values + sizeof(mask8Values)/sizeof(*mask8Values));
    }

template <>
std::vector<int8_t> test_masks<int8_t>()
    {
    return std::vector<int8_t>(mask8Values, mask8Values + sizeof(mask8Values)/sizeof(*mask8Values));
    }

template <typename T> static
std::vector<std::tuple<std::tuple<T, T>, int32_t>> test_mask_then_shift_values()
   {
   return TRTest::combine(TRTest::combine(TRTest::const_values<T>(), test_masks<T>()), test_shifts<T>());
   }

template <typename T>
T mask_then_shift_right(T value, T mask, int32_t shift)
    {
    if (sizeof(T) <= sizeof(int32_t))
        {
        shift &= (shift & (8 * sizeof(int32_t) - 1));
        }
    else
        {
        shift &= (8 * sizeof(int64_t) - 1);
        }
    return (value & mask) >> shift;
    }

template <typename T>
T mask_then_shift_left(T value, T mask, int32_t shift)
    {
    if (sizeof(T) <= sizeof(int32_t))
        {
        shift &= (shift & (8 * sizeof(int32_t) - 1));
        }
    else
        {
        shift &= (8 * sizeof(int64_t) - 1);
        }
    return (value & mask) << shift;
    }

template <typename T>
struct MaskThenShiftParamStruct {
    T value;
    T mask;
    int32_t shift;
    std::string opcode;
    T (*oracle)(T, T, int32_t);
};

template <typename T>
MaskThenShiftParamStruct<T> to_mask_then_shift_struct(std::tuple<std::tuple<std::tuple<T, T>, int32_t>, std::tuple<std::string, T (*)(T, T, int32_t)>> param) {
    MaskThenShiftParamStruct<T> s;

    s.value = std::get<0>(std::get<0>(std::get<0>(param)));
    s.mask = std::get<1>(std::get<0>(std::get<0>(param)));
    s.shift = std::get<1>(std::get<0>(param));
    s.opcode = std::get<0>(std::get<1>(param));
    s.oracle = std::get<1>(std::get<1>(param));

    return s;
}

template <typename T>
class MaskThenShiftArithmetic : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<std::tuple<std::tuple<T, T>, int32_t>, std::tuple<std::string, T (*)(T, T, int32_t)>>> {};

class UInt64MaskThenShift : public MaskThenShiftArithmetic<uint64_t> {};

TEST_P(UInt64MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (land"
        "          (lload parm=0)"
        "          (lconst %" OMR_PRIu64 "))"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt64MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<uint64_t, uint64_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, uint64_t (*) (uint64_t, uint64_t, int32_t)>("lushr", static_cast<uint64_t (*) (uint64_t, uint64_t, int32_t)>(mask_then_shift_right))
    )));

class Int64MaskThenShift : public MaskThenShiftArithmetic<int64_t> {};

TEST_P(Int64MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (land"
        "          (lload parm=0)"
        "          (lconst %" OMR_PRId64 "))"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int64MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<int64_t, int64_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(
        std::tuple<const char*, int64_t (*) (int64_t, int64_t, int32_t)>("lshr", static_cast<int64_t (*) (int64_t, int64_t, int32_t)>(mask_then_shift_right)),
        std::tuple<const char*, int64_t (*) (int64_t, int64_t, int32_t)>("lshl", static_cast<int64_t (*) (int64_t, int64_t, int32_t)>(mask_then_shift_left))
    )));

class UInt32MaskThenShift : public MaskThenShiftArithmetic<uint32_t> {};

TEST_P(UInt32MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iand"
        "          (iload parm=0)"
        "          (iconst %u))"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt32MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<uint32_t, uint32_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, uint32_t (*) (uint32_t, uint32_t, int32_t)>("iushr", static_cast<uint32_t (*) (uint32_t, uint32_t, int32_t)>(mask_then_shift_right))
    )));

class Int32MaskThenShift : public MaskThenShiftArithmetic<int32_t> {};

TEST_P(Int32MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iand"
        "          (iload parm=0)"
        "          (iconst %d))"
        "        (iconst %d)))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int32MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<int32_t, int32_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, int32_t (*) (int32_t, int32_t, int32_t)>("ishr", static_cast<int32_t (*) (int32_t, int32_t, int32_t)>(mask_then_shift_right))
    )));

class UInt16MaskThenShift : public MaskThenShiftArithmetic<uint16_t> {};

TEST_P(UInt16MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16]"
        "  (block"
        "    (ireturn"
        "      (su2i"
        "        (%s"
        "          (sand"
        "            (sload parm=0)"
        "            (sconst %u))"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint16_t (*)(uint16_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt16MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<uint16_t, uint16_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, uint16_t (*) (uint16_t, uint16_t, int32_t)>("sushr", static_cast<uint16_t (*) (uint16_t, uint16_t, int32_t)>(mask_then_shift_right))
    )));

class Int16MaskThenShift : public MaskThenShiftArithmetic<int16_t> {};

TEST_P(Int16MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sand"
        "            (sload parm=0)"
        "            (sconst %d))"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int16MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<int16_t, int16_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, int16_t (*) (int16_t, int16_t, int32_t)>("sshr", static_cast<int16_t (*) (int16_t, int16_t, int32_t)>(mask_then_shift_right))
    )));

class UInt8MaskThenShift : public MaskThenShiftArithmetic<uint8_t> {};

TEST_P(UInt8MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8]"
        "  (block"
        "    (ireturn"
        "      (bu2i"
        "        (%s"
        "          (band"
        "            (bload parm=0)"
        "            (bconst %u))"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint8_t (*)(uint8_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, UInt8MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<uint8_t, uint8_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, uint8_t (*) (uint8_t, uint8_t, int32_t)>("bushr", static_cast<uint8_t (*) (uint8_t, uint8_t, int32_t)>(mask_then_shift_right))
    )));

class Int8MaskThenShift : public MaskThenShiftArithmetic<int8_t> {};

TEST_P(Int8MaskThenShift, UsingLoadParam) {
    auto param = to_mask_then_shift_struct(GetParam());

    char inputTrees[300] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (band"
        "            (bload parm=0)"
        "            (bconst %d))"
        "          (iconst %d))))))",
        param.opcode.c_str(),
        param.mask,
        param.shift);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t)>();
    ASSERT_EQ(param.oracle(param.value, param.mask, param.shift), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(ShiftAndRotateTest, Int8MaskThenShift, ::testing::Combine(
    ::testing::ValuesIn(static_cast< std::vector<std::tuple<std::tuple<int8_t, int8_t>, int32_t>> (*) (void)>(test_mask_then_shift_values)()),
    ::testing::Values(std::tuple<const char*, int8_t (*) (int8_t, int8_t, int32_t)>("bshr", static_cast<int8_t (*) (int8_t, int8_t, int32_t)>(mask_then_shift_right))
    )));
