/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int32Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("icmpeq", icmpeq),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("icmpne", icmpne),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("icmpgt", icmpgt),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("icmpge", icmpge),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("icmplt", icmplt),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("icmple", icmple)
    )));

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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint32_t, uint32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt32Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint32_t, uint32_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("iucmpgt", iucmpgt),
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("iucmpge", iucmpge),
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("iucmplt", iucmplt),
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("iucmple", iucmple)
    )));

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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int64Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmpeq", lcmpeq),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmpne", lcmpne),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmpgt", lcmpgt),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmpge", lcmpge),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmplt", lcmplt),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmple", lcmple),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("lcmp", lcmp)
    )));


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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint64_t, uint64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt64Compare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint64_t, uint64_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("lucmpgt", lucmpgt),
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("lucmpge", lucmpge),
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("lucmplt", lucmplt),
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("lucmple", lucmple)
    )));

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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int32IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("ificmpeq", ificmpeq),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("ificmpne", ificmpne),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("ificmplt", ificmplt),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("ificmple", ificmple),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("ificmpge", ificmpge),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("ificmpgt", ificmpgt)
    )));

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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint32_t, uint32_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt32IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint32_t, uint32_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("ifiucmplt", ifiucmplt),
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("ifiucmple", ifiucmple),
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("ifiucmpge", ifiucmpge),
        std::make_tuple<const char*, int32_t(*)(uint32_t, uint32_t)>("ifiucmpgt", ifiucmpgt)
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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, Int64IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("iflcmpeq", iflcmpeq),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("iflcmpne", iflcmpne),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("iflcmplt", iflcmplt),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("iflcmple", iflcmple),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("iflcmpge", iflcmpge),
        std::make_tuple<const char*, int32_t(*)(int64_t, int64_t)>("iflcmpgt", iflcmpgt)
    )));

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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(uint64_t, uint64_t)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point(param.lhs, param.rhs);
    ASSERT_EQ(exp, act);
}

INSTANTIATE_TEST_CASE_P(CompareTest, UInt64IfCompare, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<uint64_t, uint64_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("iflucmplt", iflucmplt),
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("iflucmple", iflucmple),
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("iflucmpge", iflucmpge),
        std::make_tuple<const char*, int32_t(*)(uint64_t, uint64_t)>("iflucmpgt", iflucmpgt)
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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

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
        std::make_tuple<const char*, int32_t (*)(float, float)>("fcmpeq", fcmpeq),
        std::make_tuple<const char*, int32_t (*)(float, float)>("fcmpne", fcmpne),
        std::make_tuple<const char*, int32_t (*)(float, float)>("fcmpgt", fcmpgt),
        std::make_tuple<const char*, int32_t (*)(float, float)>("fcmpge", fcmpge),
        std::make_tuple<const char*, int32_t (*)(float, float)>("fcmplt", fcmplt),
        std::make_tuple<const char*, int32_t (*)(float, float)>("fcmple", fcmple)
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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

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
        std::make_tuple<const char*, int32_t (*)(double, double)>("dcmpeq", dcmpeq),
        std::make_tuple<const char*, int32_t (*)(double, double)>("dcmpne", dcmpne),
        std::make_tuple<const char*, int32_t (*)(double, double)>("dcmpgt", dcmpgt),
        std::make_tuple<const char*, int32_t (*)(double, double)>("dcmpge", dcmpge),
        std::make_tuple<const char*, int32_t (*)(double, double)>("dcmplt", dcmplt),
        std::make_tuple<const char*, int32_t (*)(double, double)>("dcmple", dcmple)
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
    SKIP_ON_RISCV(MissingImplementation);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(FloatIfCompare, UsingLoadParam) {
    SKIP_ON_RISCV(MissingImplementation);

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

    Tril::DefaultCompiler compiler(trees);

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
        std::make_tuple<const char*, int32_t (*)(float, float)>("iffcmpeq", iffcmpeq),
        std::make_tuple<const char*, int32_t (*)(float, float)>("iffcmpne", iffcmpne),
        std::make_tuple<const char*, int32_t (*)(float, float)>("iffcmplt", iffcmplt),
        std::make_tuple<const char*, int32_t (*)(float, float)>("iffcmple", iffcmple),
        std::make_tuple<const char*, int32_t (*)(float, float)>("iffcmpge", iffcmpge),
        std::make_tuple<const char*, int32_t (*)(float, float)>("iffcmpgt", iffcmpgt)
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
    SKIP_ON_RISCV(MissingImplementation);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(DoubleIfCompare, UsingLoadParam) {
    SKIP_ON_RISCV(MissingImplementation);

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

    Tril::DefaultCompiler compiler(trees);

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
        std::make_tuple<const char*, int32_t (*)(double, double)>("ifdcmpeq", ifdcmpeq),
        std::make_tuple<const char*, int32_t (*)(double, double)>("ifdcmpne", ifdcmpne),
        std::make_tuple<const char*, int32_t (*)(double, double)>("ifdcmplt", ifdcmplt),
        std::make_tuple<const char*, int32_t (*)(double, double)>("ifdcmple", ifdcmple),
        std::make_tuple<const char*, int32_t (*)(double, double)>("ifdcmpge", ifdcmpge),
        std::make_tuple<const char*, int32_t (*)(double, double)>("ifdcmpgt", ifdcmpgt)
    )));
