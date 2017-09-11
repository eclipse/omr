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

class Int32Arithmetic : public TRTest::BinaryOpTest<int32_t> {};

TEST_P(Int32Arithmetic, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32Arithmetic, UsingLoadParam) {
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

INSTANTIATE_TEST_CASE_P(ArithmeticTest, Int32Arithmetic, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple("iadd", [](int32_t l, int32_t r){return l+r;}),
        std::make_tuple("isub", [](int32_t l, int32_t r){return l-r;}),
        std::make_tuple("imul", [](int32_t l, int32_t r){return l*r;}) )));

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
        std::make_tuple("idiv", [](int32_t l, int32_t r){return l/r;}),
        std::make_tuple("irem", [](int32_t l, int32_t r){return l%r;}) )));
