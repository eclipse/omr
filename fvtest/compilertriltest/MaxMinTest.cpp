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
#include "omrformatconsts.h"

int32_t imax(int32_t l, int32_t r) {
  return std::max(l,r);
}

int32_t imin(int32_t l, int32_t r) {
  return std::min(l,r);
}

int64_t lmax(int64_t l, int64_t r) {
  return std::max(l,r);
}

int64_t lmin(int64_t l, int64_t r) {
  return std::min(l,r);
}
class Int32MaxMin : public TRTest::BinaryOpTest<int32_t> {};

TEST_P(Int32MaxMin, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[150] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int32"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iconst %" OMR_PRId32 ")"
        "        (iconst %" OMR_PRId32 "))"
        ")))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int32MaxMin, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[150] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (%s"
        "        (iload parm=0)"
        "        (iload parm=1))"
        ")))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}


INSTANTIATE_TEST_CASE_P(MaxMin, Int32MaxMin, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple("imax", imax),
        std::make_tuple("imin", imin))));

class Int64MaxMin : public TRTest::BinaryOpTest<int64_t> {};

TEST_P(Int64MaxMin, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[150] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int64"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lconst %" OMR_PRId64 ")"
        "        (lconst %" OMR_PRId64 "))"
        ")))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(Int64MaxMin, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[150] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (%s"
        "        (lload parm=0)"
        "        (lload parm=1))"
        ")))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}


INSTANTIATE_TEST_CASE_P(MaxMin, Int64MaxMin, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple("lmax", lmax),
        std::make_tuple("lmin", lmin))));
