/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

#include "OpCodeTest.hpp"
#include "default_compiler.hpp"
#include "omrformatconsts.h"

#include <cmath>

#if defined(J9ZOS390) || defined(AIXPPC)
namespace std
{
   using ::isnan;
}
#endif

template <typename T>
T add(T l, T r) {
    return l + r;
}

template <typename T>
T sub(T l, T r) {
    return l - r;
}

template <typename T>
T mul(T l, T r) {
    return l * r;
}

template <typename T>
T div(T l, T r) {
    return l / r;
}

template <typename T>
T udiv(T l, T r) {
    return l / r;
}

template <typename T>
T rem(T l, T r) {
    return l % r;
}

template <typename T>
T urem(T l, T r) {
    return l % r;
}

int32_t imulh(int32_t l, int32_t r) {
    int64_t x = static_cast<int64_t>(l) * static_cast<int64_t>(r);
    return static_cast<int32_t>(x >> 32); // upper 32 bits
}

class Int32Arithmetic : public TRTest::BinaryOpTest<int32_t> {};

class UInt32Arithmetic : public TRTest::BinaryOpTest<uint32_t> {};

class Int64Arithmetic : public TRTest::BinaryOpTest<int64_t> {};

class UInt64Arithmetic : public TRTest::BinaryOpTest<uint64_t> {};

class Int16Arithmetic : public TRTest::BinaryOpTest<int16_t> {};

class Int8Arithmetic : public TRTest::BinaryOpTest<int8_t> {};

TEST_P(Int32Arithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (iconst %d)"
      "        (iconst %d)))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32Arithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32, Int32]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (iload parm=0)"
      "        (iload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

TEST_P(Int32Arithmetic, UsingLoadParamAndLoadConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (iload parm=0)"
      "        (iconst %d)))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(UInt32Arithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF((param.opcode == "iudiv" || param.opcode == "iurem") && (OMRPORT_ARCH_PPC64 == arch || OMRPORT_ARCH_PPC64LE == arch), MissingImplementation)
        << "The Power codegen does not yet support iudiv/iurem (see issue #3673)";

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (iconst %d)"
      "        (iconst %d)))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(UInt32Arithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF((param.opcode == "iudiv" || param.opcode == "iurem") && (OMRPORT_ARCH_PPC64 == arch || OMRPORT_ARCH_PPC64LE == arch), MissingImplementation)
        << "The Power codegen does not yet support iudiv/iurem (see issue #3673)";

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32, Int32]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (iload parm=0)"
      "        (iload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t, uint32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

TEST_P(UInt32Arithmetic, UsingLoadParamAndLoadConst) {
    auto param = TRTest::to_struct(GetParam());

    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF((param.opcode == "iudiv" || param.opcode == "iurem") && (OMRPORT_ARCH_PPC64 == arch || OMRPORT_ARCH_PPC64LE == arch), MissingImplementation)
        << "The Power codegen does not yet support iudiv/iurem (see issue #3673)";

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Int32]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (iload parm=0)"
      "        (iconst %d)))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint32_t (*)(uint32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int64Arithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64"
      "  (block"
      "    (lreturn"
      "      (%s"
      "        (lconst %" OMR_PRId64 ")"
      "        (lconst %" OMR_PRId64 ")))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64Arithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64, Int64]"
      "  (block"
      "    (lreturn"
      "      (%s"
      "        (lload parm=0)"
      "        (lload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

TEST_P(Int64Arithmetic, UsingLoadParamAndLoadConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64]"
      "  (block"
      "    (lreturn"
      "      (%s"
      "        (lload parm=0)"
      "        (lconst %" OMR_PRId64 ")))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(UInt64Arithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(param.opcode == "ludiv" && (OMRPORT_ARCH_PPC64 == arch || OMRPORT_ARCH_PPC64LE == arch), MissingImplementation)
        << "The Power codegen does not yet support ludiv (see issue #2227)";

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64"
      "  (block"
      "    (lreturn"
      "      (%s"
      "        (lconst %" OMR_PRId64 ")"
      "        (lconst %" OMR_PRId64 ")))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(UInt64Arithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(param.opcode == "ludiv" && (OMRPORT_ARCH_PPC64 == arch || OMRPORT_ARCH_PPC64LE == arch), MissingImplementation)
        << "The Power codegen does not yet support ludiv (see issue #2227)";

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64, Int64]"
      "  (block"
      "    (lreturn"
      "      (%s"
      "        (lload parm=0)"
      "        (lload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t, uint64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

TEST_P(UInt64Arithmetic, UsingLoadParamAndLoadConst) {
    auto param = TRTest::to_struct(GetParam());

    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(param.opcode == "ludiv" && (OMRPORT_ARCH_PPC64 == arch || OMRPORT_ARCH_PPC64LE == arch), MissingImplementation)
        << "The Power codegen does not yet support ludiv (see issue #2227)";

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Int64]"
      "  (block"
      "    (lreturn"
      "      (%s"
      "        (lload parm=0)"
      "        (lconst %" OMR_PRId64 ")))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int16Arithmetic, UsingConst) {
    SKIP_ON_RISCV(MissingImplementation);
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int16"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (sconst %" OMR_PRId16 ")"
      "        (sconst %" OMR_PRId16 ")))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int16Arithmetic, UsingLoadParam) {
    SKIP_ON_RISCV(MissingImplementation);

    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int16 args=[Int16, Int16]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (sload parm=0)"
      "        (sload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t, int16_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

TEST_P(Int16Arithmetic, UsingLoadParamAndLoadConst) {
    SKIP_ON_RISCV(MissingImplementation);

    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int16 args=[Int16]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (sload parm=0)"
      "        (sconst %" OMR_PRId16 ")))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

TEST_P(Int8Arithmetic, UsingConst) {
    SKIP_ON_RISCV(MissingImplementation);
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int8"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (bconst %" OMR_PRId8 ")"
      "        (bconst %" OMR_PRId8 ")))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int8Arithmetic, UsingLoadParam) {
    SKIP_ON_RISCV(MissingImplementation);
    
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int8 args=[Int8, Int8]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (bload parm=0)"
      "        (bload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t, int8_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

TEST_P(Int8Arithmetic, UsingLoadParamAndLoadConst) {
    SKIP_ON_RISCV(MissingImplementation);

    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int8 args=[Int8]"
      "  (block"
      "    (ireturn"
      "      (%s"
      "        (bload parm=0)"
      "        (bconst %" OMR_PRId8 ")))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs));
}

INSTANTIATE_TEST_CASE_P(ArithmeticTest, Int32Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("iadd", add),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("isub", sub),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("imul", mul),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("imulh", imulh))));

INSTANTIATE_TEST_CASE_P(ArithmeticTest, Int64Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("ladd", add),
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("lsub", sub),
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("lmul", mul))));

INSTANTIATE_TEST_CASE_P(ArithmeticTest, Int16Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int16_t, int16_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int16_t(*)(int16_t, int16_t)>("sadd", add<int16_t>),
        std::make_tuple<const char*, int16_t(*)(int16_t, int16_t)>("ssub", sub<int16_t>),
        std::make_tuple<const char*, int16_t(*)(int16_t, int16_t)>("smul", mul<int16_t>))));

INSTANTIATE_TEST_CASE_P(ArithmeticTest, Int8Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int8_t, int8_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int8_t(*)(int8_t, int8_t)>("badd", add<int8_t>),
        std::make_tuple<const char*, int8_t(*)(int8_t, int8_t)>("bsub", sub<int8_t>),
        std::make_tuple<const char*, int8_t(*)(int8_t, int8_t)>("bmul", mul<int8_t>))));

/**
 * @brief Filter function for *div opcodes
 * @tparam T is the data type of the div opcode
 * @param a is the set of input values to be tested for filtering
 *
 * This function is a predicate for filtering invalid input values for the
 * *div opcodes. This includes pairs where the divisor is 0, and pairs where
 * dividend is the minimum (negative) value for the type and the divisor is -1.
 * The latter is a problem because in two's complement, the quotient of the
 * signed minimum and -1 is greater than the maximum value representable by
 * the given type. On some platforms this computation results in SIGFPE.
 */
template <typename T>
bool div_filter(std::tuple<T, T> a)
   {
   auto isDivisorZero = std::get<1>(a) == 0;
   auto isDividendMin = std::get<0>(a) == std::numeric_limits<T>::min();
   auto isDivisorNegativeOne = std::get<1>(a) == TRTest::negative_one_value<T>();

   return isDivisorZero || (isDividendMin && isDivisorNegativeOne);
   }

INSTANTIATE_TEST_CASE_P(DivArithmeticTest, Int32Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<int32_t, int32_t>(), div_filter<int32_t> )),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("idiv", div),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("irem", rem))));

INSTANTIATE_TEST_CASE_P(DivArithmeticTest, Int64Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<int64_t, int64_t>(), div_filter<int64_t> )),
    ::testing::Values(
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("ldiv", div),
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("lrem", rem))));

/**
 * @brief Filter function for *udiv opcodes
 * @tparam T is the data type of the udiv opcode
 * @param a is the set of input values to be tested for filtering
 *
 * This function is a predicate for filtering invalid input values for the
 * *udiv opcodes, namely pairs where the divisor is 0.
 */
template <typename T>
bool udiv_filter(std::tuple<T, T> a)
   {
   return std::get<1>(a) == 0;
   }

INSTANTIATE_TEST_CASE_P(DivArithmeticTest, UInt32Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<uint32_t, uint32_t>(), udiv_filter<uint32_t> )),
    ::testing::Values(
        std::make_tuple<const char*, uint32_t(*)(uint32_t, uint32_t)>("iudiv", udiv),
        std::make_tuple<const char*, uint32_t(*)(uint32_t, uint32_t)>("iurem", urem))));

INSTANTIATE_TEST_CASE_P(DivArithmeticTest, UInt64Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<uint64_t, uint64_t>(), udiv_filter<uint64_t> )),
    ::testing::Values(
        std::make_tuple<const char*, uint64_t(*)(uint64_t, uint64_t)>("ludiv", udiv))));

template <typename T>
bool smallFp_filter(std::tuple<T, T> a)
   {
   // workaround: avoid failure caused by snprintf("%f")
   auto a0 = std::get<0>(a);
   auto a1 = std::get<1>(a);
   return ((std::abs(a0) < 0.01 && a0 != 0.0) || (std::abs(a1) < 0.01 && a1 != 0.0));
   }


class FloatArithmetic : public TRTest::BinaryOpTest<float> {};

TEST_P(FloatArithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Float"
      "  (block"
      "    (freturn"
      "      (%s"
      "        (fconst %f)"
      "        (fconst %f)))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

TEST_P(FloatArithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Float args=[Float, Float]"
      "  (block"
      "    (freturn"
      "      (%s"
      "        (fload parm=0)"
      "        (fload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(float, float)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

TEST_P(FloatArithmetic, UsingLoadParamAndLoadConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Float args=[Float]"
      "  (block"
      "    (freturn"
      "      (%s"
      "        (fload parm=0)"
      "        (fconst %f)))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(float)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs);
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

INSTANTIATE_TEST_CASE_P(ArithmeticTest, FloatArithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<float, float>(), smallFp_filter<float>)),
    ::testing::Values(
        std::make_tuple<const char*, float (*)(float, float)>("fadd", static_cast<float (*)(float, float)>(add)),
        std::make_tuple<const char*, float (*)(float, float)>("fsub", static_cast<float (*)(float, float)>(sub)),
        std::make_tuple<const char*, float (*)(float, float)>("fmul", static_cast<float (*)(float, float)>(mul)),
        std::make_tuple<const char*, float (*)(float, float)>("fdiv", static_cast<float (*)(float, float)>(div))
    )));


class DoubleArithmetic : public TRTest::BinaryOpTest<double> {};

TEST_P(DoubleArithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Double"
      "  (block"
      "    (dreturn"
      "      (%s"
      "        (dconst %f)"
      "        (dconst %f)))))",
      param.opcode.c_str(),
      param.lhs,
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

TEST_P(DoubleArithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Double args=[Double, Double]"
      "  (block"
      "    (dreturn"
      "      (%s"
      "        (dload parm=0)"
      "        (dload parm=1)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(double, double)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

TEST_P(DoubleArithmetic, UsingLoadParamAndLoadConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Double args=[Double]"
      "  (block"
      "    (dreturn"
      "      (%s"
      "        (dload parm=0)"
      "        (dconst %f)))))",
      param.opcode.c_str(),
      param.rhs
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(double)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs);
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

INSTANTIATE_TEST_CASE_P(ArithmeticTest, DoubleArithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<double, double>(), smallFp_filter<double>)),
    ::testing::Values(
        std::make_tuple<const char*, double (*)(double, double)>("dadd", add),
        std::make_tuple<const char*, double (*)(double, double)>("dsub", sub),
        std::make_tuple<const char*, double (*)(double, double)>("dmul", mul),
        std::make_tuple<const char*, double (*)(double, double)>("ddiv", div)
    )));

template <typename T>
bool smallFp_unary_filter(T a)
   {
   // workaround: avoid failure caused by snprintf("%f")
   return (std::abs(a) < 0.01 && a != 0.0);
   }

float fneg(float x) {
    return -x;
}

class FloatUnaryArithmetic : public TRTest::UnaryOpTest<float> {};

TEST_P(FloatUnaryArithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Float"
      "  (block"
      "    (freturn"
      "      (%s"
      "        (fconst %f)))))",
      param.opcode.c_str(),
      param.value
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(void)>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

TEST_P(FloatUnaryArithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Float args=[Float]"
      "  (block"
      "    (freturn"
      "      (%s"
      "        (fload parm=0)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(float)>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point(param.value);
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

INSTANTIATE_TEST_CASE_P(ArithmeticTest, FloatUnaryArithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_values<float>(), smallFp_unary_filter<float>)),
    ::testing::Values(
        std::make_tuple<const char*, float (*)(float)>("fabs", static_cast<float (*)(float)>(std::abs)),
        std::make_tuple<const char*, float (*)(float)>("fneg", fneg)
    )));

double dneg(double x) {
    return -x;
}

class DoubleUnaryArithmetic : public TRTest::UnaryOpTest<double> {};

TEST_P(DoubleUnaryArithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Double"
      "  (block"
      "    (dreturn"
      "      (%s"
      "        (dconst %f)))))",
      param.opcode.c_str(),
      param.value
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(void)>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

TEST_P(DoubleUnaryArithmetic, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Double args=[Double]"
      "  (block"
      "    (dreturn"
      "      (%s"
      "        (dload parm=0)))))",
      param.opcode.c_str()
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(double)>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point(param.value);
    if (std::isnan(exp)) {
        ASSERT_EQ(std::isnan(exp), std::isnan(act));
    } else {
        ASSERT_EQ(exp, act);
    }
}

INSTANTIATE_TEST_CASE_P(ArithmeticTest, DoubleUnaryArithmetic, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_values<double>(), smallFp_unary_filter<double>)),
    ::testing::Values(
        std::make_tuple<const char*, double (*)(double)>("dabs", static_cast<double (*)(double)>(std::abs)),
        std::make_tuple<const char*, double (*)(double)>("dneg", dneg)
    )));
