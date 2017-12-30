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

INSTANTIATE_TEST_CASE_P(LogicalTest, Int32LogicalBinary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t,int32_t>()),
    ::testing::Values(
        std::make_tuple("ior", ior)
        )));

