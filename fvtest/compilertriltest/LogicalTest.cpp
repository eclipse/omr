/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

int32_t ineg(int32_t l) {
    return -l;
}

int32_t iabs(int32_t l) { 
   if (l >= 0) 
      return l;
   else 
      return -1 * l; 
}

int32_t ior(int32_t l, int32_t r) {
    return l | r;
}

int32_t iand(int32_t l, int32_t r) {
    return l & r;
}

int32_t ixor(int32_t l, int32_t r) {
    return l ^ r;
}

int64_t lor(int64_t l, int64_t r) {
    return l | r;
}

int64_t land(int64_t l, int64_t r) {
    return l & r;
}

int64_t lxor(int64_t l, int64_t r) {
    return l ^ r;
}

int64_t lneg(int64_t l) {
    return l == INT64_MIN ? INT64_MIN : -l;
}

class Int32LogicalUnary : public TRTest::UnaryOpTest<int32_t> {}; 

TEST_P(Int32LogicalUnary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) ) )))", param.opcode.c_str(), param.value);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)()>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32LogicalUnary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 args=[Int32] (block (ireturn (%s (iload parm=0) ) )))", param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.value), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int32LogicalUnary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_values<int32_t>()),
    ::testing::Values(
        std::make_tuple("ineg", ineg),
        std::make_tuple("iabs", iabs)
        )));

class Int32LogicalBinary : public TRTest::BinaryOpTest<int32_t> {};

TEST_P(Int32LogicalBinary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32LogicalBinary, UsingLoadParam) {
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

INSTANTIATE_TEST_CASE_P(LogicalTest, Int32LogicalBinary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t,int32_t>()),
    ::testing::Values(
        std::make_tuple("ior", ior),
        std::make_tuple("iand", iand),
        std::make_tuple("ixor", ixor)
        )));

class Int64LogicalBinary : public TRTest::BinaryOpTest<int64_t> {};

TEST_P(Int64LogicalBinary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160, 
        "(method return=Int64"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRId64 ")"
        "        (lconst %" OMR_PRId64 ") ) ) ) )",
        param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64LogicalBinary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160, 
        "(method return=Int64 args=[Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0)"
        "        (lload parm=1) ) ) ) )", 
        param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int64LogicalBinary, ::testing::Combine(
	::testing::ValuesIn(TRTest::const_value_pairs<int64_t,int64_t>()),
	::testing::Values(
		std::make_tuple("lor", lor),
		std::make_tuple("land", land),
		std::make_tuple("lxor", lxor)
				)));

                
class Int64LogicalUnary : public TRTest::UnaryOpTest<int64_t> {};

TEST_P(Int64LogicalUnary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
        "(method return=Int64"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRId64 ") )"
        ")))",
        param.opcode.c_str(),
        param.value);

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)()>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64LogicalUnary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
        "(method return=Int64 args=[Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0) )"
        ")))",
        param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_EQ(param.oracle(param.value), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int64LogicalUnary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_values<int64_t>()),
    ::testing::Values(
        std::make_tuple("lneg", lneg)
        )));
