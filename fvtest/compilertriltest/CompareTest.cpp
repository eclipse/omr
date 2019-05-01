/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include <cmath>

int32_t icmpeq(int32_t l, int32_t r) {
    return (l == r) ? 1 : 0;
}

int32_t icmpne(int32_t l, int32_t r) {
    return (l != r) ? 1 : 0;
}

int32_t icmpgt(int32_t l, int32_t r) {
    return (l > r) ? 1 : 0;
}

int32_t icmpge(int32_t l, int32_t r) {
    return (l >= r) ? 1 : 0;
}

int32_t icmplt(int32_t l, int32_t r) {
    return (l < r) ? 1 : 0;
}

int32_t icmple(int32_t l, int32_t r) {
    return (l <= r) ? 1 : 0;
}

class Int32Compare : public TRTest::BinaryOpTest<int32_t> {};

TEST_P(Int32Compare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
       "(method return=Int32 "
         "(block "
           "(ireturn "
             "(%s "
               "(iconst %d) "
               "(iconst %d)))))",
       param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32Compare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
       "(method return=Int32 args=[Int32, Int32] "
         "(block "
           "(ireturn "
             "(%s "
               "(iload parm=0) "
               "(iload parm=1)))))",
       param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int32Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple("icmpeq", icmpeq),
        std::make_tuple("icmpne", icmpne),
        std::make_tuple("icmpgt", icmpgt),
        std::make_tuple("icmpge", icmpge),
        std::make_tuple("icmplt", icmplt),
        std::make_tuple("icmple", icmple) )));

int32_t iucmpeq(uint32_t l, uint32_t r) {
    return (l == r) ? 1 : 0;
}

int32_t iucmpne(uint32_t l, uint32_t r) {
    return (l != r) ? 1 : 0;
}

int32_t iucmpgt(uint32_t l, uint32_t r) {
    return (l > r) ? 1 : 0;
}

int32_t iucmpge(uint32_t l, uint32_t r) {
    return (l >= r) ? 1 : 0;
}

int32_t iucmplt(uint32_t l, uint32_t r) {
    return (l < r) ? 1 : 0;
}

int32_t iucmple(uint32_t l, uint32_t r) {
    return (l <= r) ? 1 : 0;
}

class UInt32Compare : public TRTest::OpCodeTest<int32_t,uint32_t,uint32_t> {};

TEST_P(UInt32Compare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
       "(method return=Int32 "
         "(block "
           "(ireturn "
             "(%s "
               "(iconst %d) "
               "(iconst %d)))))",
       param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(UInt32Compare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
       "(method return=Int32 args=[Int32, Int32] "
         "(block "
           "(ireturn "
             "(%s "
               "(iload parm=0) "
               "(iload parm=1)))))",
       param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint32_t, uint32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt32Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint32_t, uint32_t>()),
    ::testing::Values(
        std::make_tuple("iucmpeq", iucmpeq),
        std::make_tuple("iucmpne", iucmpne),
        std::make_tuple("iucmpgt", iucmpgt),
        std::make_tuple("iucmpge", iucmpge),
        std::make_tuple("iucmplt", iucmplt),
        std::make_tuple("iucmple", iucmple) )));

int32_t lcmpeq(int64_t l, int64_t r) {
    return (l == r) ? 1 : 0;
}

int32_t lcmpne(int64_t l, int64_t r) {
    return (l != r) ? 1 : 0;
}

int32_t lcmpgt(int64_t l, int64_t r) {
    return (l > r) ? 1 : 0;
}

int32_t lcmpge(int64_t l, int64_t r) {
    return (l >= r) ? 1 : 0;
}

int32_t lcmplt(int64_t l, int64_t r) {
    return (l < r) ? 1 : 0;
}

int32_t lcmple(int64_t l, int64_t r) {
    return (l <= r) ? 1 : 0;
}

int32_t lcmp(int64_t l, int64_t r) {
    return (l < r) ? -1 : ((l > r) ? 1 : 0);
}

class Int64Compare : public TRTest::OpCodeTest<int32_t,int64_t,int64_t> {};

TEST_P(Int64Compare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
       "(method return=Int32 "
         "(block "
           "(ireturn "
             "(%s "
               "(lconst %lld) "
               "(lconst %lld)))))",
       param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64Compare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
       "(method return=Int32 args=[Int64, Int64] "
         "(block "
           "(ireturn "
             "(%s "
               "(lload parm=0) "
               "(lload parm=1)))))",
       param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int64Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple("lcmpeq", lcmpeq),
        std::make_tuple("lcmpne", lcmpne),
        std::make_tuple("lcmpgt", lcmpgt),
        std::make_tuple("lcmpge", lcmpge),
        std::make_tuple("lcmplt", lcmplt),
        std::make_tuple("lcmple", lcmple),
        std::make_tuple("lcmp", lcmp)
 )));

int32_t lucmpeq(uint64_t l, uint64_t r) {
    return (l == r) ? 1 : 0;
}

int32_t lucmpne(uint64_t l, uint64_t r) {
    return (l != r) ? 1 : 0;
}

int32_t lucmpgt(uint64_t l, uint64_t r) {
    return (l > r) ? 1 : 0;
}

int32_t lucmpge(uint64_t l, uint64_t r) {
    return (l >= r) ? 1 : 0;
}

int32_t lucmplt(uint64_t l, uint64_t r) {
    return (l < r) ? 1 : 0;
}

int32_t lucmple(uint64_t l, uint64_t r) {
    return (l <= r) ? 1 : 0;
}

class UInt64Compare : public TRTest::OpCodeTest<int32_t,uint64_t,uint64_t> {};

TEST_P(UInt64Compare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
       "(method return=Int32 "
         "(block "
           "(ireturn "
             "(%s "
               "(lconst %lld) "
               "(lconst %lld)))))",
       param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(UInt64Compare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
       "(method return=Int32 args=[Int64, Int64] "
         "(block "
           "(ireturn "
             "(%s "
               "(lload parm=0) "
               "(lload parm=1)))))",
       param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint64_t, uint64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt64Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint64_t, uint64_t>()),
    ::testing::Values(
        std::make_tuple("lucmpeq", lucmpeq),
        std::make_tuple("lucmpne", lucmpne),
        std::make_tuple("lucmpgt", lucmpgt),
        std::make_tuple("lucmpge", lucmpge),
        std::make_tuple("lucmplt", lucmplt),
        std::make_tuple("lucmple", lucmple) )));

static const int32_t IFCMP_TRUE_NUM = 123;
static const int32_t IFCMP_FALSE_NUM = -456;

int32_t ificmpeq(int32_t l, int32_t r) {
    return (l == r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ificmpne(int32_t l, int32_t r) {
    return (l != r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ificmplt(int32_t l, int32_t r) {
    return (l < r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ificmple(int32_t l, int32_t r) {
    return (l <= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ificmpge(int32_t l, int32_t r) {
    return (l >= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ificmpgt(int32_t l, int32_t r) {
    return (l > r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

class Int32IfCompare : public TRTest::BinaryOpTest<int32_t> {};

TEST_P(Int32IfCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 "
          "(block "
            "(%s target=b1 (iconst %d) (iconst %d))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), param.lhs, param.rhs, IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32IfCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 args=[Int32, Int32] "
          "(block "
            "(%s target=b1 (iload parm=0) (iload parm=1))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int32IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple("ificmpeq", ificmpeq),
        std::make_tuple("ificmpne", ificmpne),
        std::make_tuple("ificmplt", ificmplt),
        std::make_tuple("ificmple", ificmple),
        std::make_tuple("ificmpge", ificmpge),
        std::make_tuple("ificmpgt", ificmpgt)
    )));

int32_t ifiucmpeq(uint32_t l, uint32_t r) {
    return (l == r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifiucmpne(uint32_t l, uint32_t r) {
    return (l != r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifiucmplt(uint32_t l, uint32_t r) {
    return (l < r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifiucmple(uint32_t l, uint32_t r) {
    return (l <= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifiucmpge(uint32_t l, uint32_t r) {
    return (l >= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifiucmpgt(uint32_t l, uint32_t r) {
    return (l > r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

class UInt32IfCompare : public TRTest::OpCodeTest<int32_t, uint32_t, uint32_t> {};

TEST_P(UInt32IfCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 "
          "(block "
            "(%s target=b1 (iconst %d) (iconst %d))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), param.lhs, param.rhs, IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(UInt32IfCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 args=[Int32, Int32] "
          "(block "
            "(%s target=b1 (iload parm=0) (iload parm=1))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint32_t, uint32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt32IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint32_t, uint32_t>()),
    ::testing::Values(
        std::make_tuple("ifiucmpeq", ifiucmpeq),
        std::make_tuple("ifiucmpne", ifiucmpne),
        std::make_tuple("ifiucmplt", ifiucmplt),
        std::make_tuple("ifiucmple", ifiucmple),
        std::make_tuple("ifiucmpge", ifiucmpge),
        std::make_tuple("ifiucmpgt", ifiucmpgt)
    )));

int32_t iflcmpeq(int64_t l, int64_t r) {
    return (l == r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflcmpne(int64_t l, int64_t r) {
    return (l != r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflcmplt(int64_t l, int64_t r) {
    return (l < r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflcmple(int64_t l, int64_t r) {
    return (l <= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflcmpge(int64_t l, int64_t r) {
    return (l >= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflcmpgt(int64_t l, int64_t r) {
    return (l > r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

class Int64IfCompare : public TRTest::OpCodeTest<int32_t, int64_t, int64_t> {};

TEST_P(Int64IfCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 "
          "(block "
            "(%s target=b1 (lconst %lld) (lconst %lld))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), param.lhs, param.rhs, IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64IfCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 args=[Int64, Int64] "
          "(block "
            "(%s target=b1 (lload parm=0) (lload parm=1))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int64IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple("iflcmpeq", iflcmpeq),
        std::make_tuple("iflcmpne", iflcmpne),
        std::make_tuple("iflcmplt", iflcmplt),
        std::make_tuple("iflcmple", iflcmple),
        std::make_tuple("iflcmpge", iflcmpge),
        std::make_tuple("iflcmpgt", iflcmpgt)
    )));

int32_t iflucmpeq(uint64_t l, uint64_t r) {
    return (l == r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflucmpne(uint64_t l, uint64_t r) {
    return (l != r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflucmplt(uint64_t l, uint64_t r) {
    return (l < r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflucmple(uint64_t l, uint64_t r) {
    return (l <= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflucmpge(uint64_t l, uint64_t r) {
    return (l >= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iflucmpgt(uint64_t l, uint64_t r) {
    return (l > r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

class UInt64IfCompare : public TRTest::OpCodeTest<int32_t, uint64_t, uint64_t> {};

TEST_P(UInt64IfCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 "
          "(block "
            "(%s target=b1 (lconst %lld) (lconst %lld))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), param.lhs, param.rhs, IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(UInt64IfCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 args=[Int64, Int64] "
          "(block "
            "(%s target=b1 (lload parm=0) (lload parm=1))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint64_t, uint64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt64IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint64_t, uint64_t>()),
    ::testing::Values(
        std::make_tuple("iflucmpeq", iflucmpeq),
        std::make_tuple("iflucmpne", iflucmpne),
        std::make_tuple("iflucmplt", iflucmplt),
        std::make_tuple("iflucmple", iflucmple),
        std::make_tuple("iflucmpge", iflucmpge),
        std::make_tuple("iflucmpgt", iflucmpgt)
    )));

template <typename T>
bool smallFp_filter(std::tuple<T, T> a)
   {
   // workaround: avoid failure caused by snprintf("%f")
   auto a0 = std::get<0>(a);
   auto a1 = std::get<1>(a);
   return ((std::abs(a0) < 0.01 && a0 != 0.0) || (std::abs(a1) < 0.01 && a1 != 0.0));
   }

int32_t fcmpeq(float l, float r) {
    return (l == r) ? 1 : 0;
}

int32_t fcmpne(float l, float r) {
    return (l != r) ? 1 : 0;
}

int32_t fcmpgt(float l, float r) {
    return (l > r) ? 1 : 0;
}

int32_t fcmpge(float l, float r) {
    return (l >= r) ? 1 : 0;
}

int32_t fcmplt(float l, float r) {
    return (l < r) ? 1 : 0;
}

int32_t fcmple(float l, float r) {
    return (l <= r) ? 1 : 0;
}

class FloatCompare : public TRTest::OpCodeTest<int32_t, float, float> {};

TEST_P(FloatCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, 1024,
       "(method return=Int32 "
         "(block "
           "(ireturn "
             "(%s "
               "(fconst %f) "
               "(fconst %f)))))",
       param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(FloatCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
       "(method return=Int32 args=[Float, Float] "
         "(block "
           "(ireturn "
             "(%s "
               "(fload parm=0) "
               "(fload parm=1)))))",
       param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(float, float)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, FloatCompare, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<float, float>(), smallFp_filter<float>)),
    ::testing::Values(
        std::make_tuple("fcmpeq", fcmpeq),
        std::make_tuple("fcmpne", fcmpne),
        std::make_tuple("fcmpgt", fcmpgt),
        std::make_tuple("fcmpge", fcmpge),
        std::make_tuple("fcmplt", fcmplt),
        std::make_tuple("fcmple", fcmple)
    )));

int32_t dcmpeq(double l, double r) {
    return (l == r) ? 1 : 0;
}

int32_t dcmpne(double l, double r) {
    return (l != r) ? 1 : 0;
}

int32_t dcmpgt(double l, double r) {
    return (l > r) ? 1 : 0;
}

int32_t dcmpge(double l, double r) {
    return (l >= r) ? 1 : 0;
}

int32_t dcmplt(double l, double r) {
    return (l < r) ? 1 : 0;
}

int32_t dcmple(double l, double r) {
    return (l <= r) ? 1 : 0;
}

class DoubleCompare : public TRTest::OpCodeTest<int32_t, double, double> {};

TEST_P(DoubleCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, 1024,
       "(method return=Int32 "
         "(block "
           "(ireturn "
             "(%s "
               "(dconst %f) "
               "(dconst %f)))))",
       param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(DoubleCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
       "(method return=Int32 args=[Double, Double] "
         "(block "
           "(ireturn "
             "(%s "
               "(dload parm=0) "
               "(dload parm=1)))))",
       param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, double)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, DoubleCompare, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<double, double>(), smallFp_filter<double>)),
    ::testing::Values(
        std::make_tuple("dcmpeq", dcmpeq),
        std::make_tuple("dcmpne", dcmpne),
        std::make_tuple("dcmpgt", dcmpgt),
        std::make_tuple("dcmpge", dcmpge),
        std::make_tuple("dcmplt", dcmplt),
        std::make_tuple("dcmple", dcmple)
    )));

int32_t iffcmpeq(float l, float r) {
    return (l == r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iffcmpne(float l, float r) {
    return (l != r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iffcmplt(float l, float r) {
    return (l < r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iffcmple(float l, float r) {
    return (l <= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iffcmpge(float l, float r) {
    return (l >= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t iffcmpgt(float l, float r) {
    return (l > r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

class FloatIfCompare : public TRTest::OpCodeTest<int32_t, float, float> {};

TEST_P(FloatIfCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 "
          "(block "
            "(%s target=b1 (fconst %f) (fconst %f))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), param.lhs, param.rhs, IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(FloatIfCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 args=[Float, Float] "
          "(block "
            "(%s target=b1 (fload parm=0) (fload parm=1))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(float, float)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, FloatIfCompare, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<float, float>(), smallFp_filter<float>)),
    ::testing::Values(
        std::make_tuple("iffcmpeq", iffcmpeq),
        std::make_tuple("iffcmpne", iffcmpne),
        std::make_tuple("iffcmplt", iffcmplt),
        std::make_tuple("iffcmple", iffcmple),
        std::make_tuple("iffcmpge", iffcmpge),
        std::make_tuple("iffcmpgt", iffcmpgt)
    )));

int32_t ifdcmpeq(double l, double r) {
    return (l == r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifdcmpne(double l, double r) {
    return (l != r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifdcmplt(double l, double r) {
    return (l < r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifdcmple(double l, double r) {
    return (l <= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifdcmpge(double l, double r) {
    return (l >= r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

int32_t ifdcmpgt(double l, double r) {
    return (l > r) ? IFCMP_TRUE_NUM : IFCMP_FALSE_NUM;
}

class DoubleIfCompare : public TRTest::OpCodeTest<int32_t, double, double> {};

TEST_P(DoubleIfCompare, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, 1024,
        "(method return=Int32 "
          "(block "
            "(%s target=b1 (dconst %f) (dconst %f))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), param.lhs, param.rhs, IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(DoubleIfCompare, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[256] = {0};
    std::snprintf(inputTrees, 256,
        "(method return=Int32 args=[Double, Double] "
          "(block "
            "(%s target=b1 (dload parm=0) (dload parm=1))) "
          "(block "
            "(ireturn (iconst %d))) "
          "(block name=b1 "
            "(ireturn (iconst %d)))"
        ")",
        param.opcode.c_str(), IFCMP_FALSE_NUM, IFCMP_TRUE_NUM);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, double)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, DoubleIfCompare, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<double, double>(), smallFp_filter<double>)),
    ::testing::Values(
        std::make_tuple("ifdcmpeq", ifdcmpeq),
        std::make_tuple("ifdcmpne", ifdcmpne),
        std::make_tuple("ifdcmplt", ifdcmplt),
        std::make_tuple("ifdcmple", ifdcmple),
        std::make_tuple("ifdcmpge", ifdcmpge),
        std::make_tuple("ifdcmpgt", ifdcmpgt)
    )));
