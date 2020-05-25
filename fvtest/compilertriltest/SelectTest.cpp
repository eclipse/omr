/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

// C++11 upgrade (Issue #1916).
template <typename CompareType, typename ValType>
class SelectTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<std::tuple<CompareType, CompareType>, std::tuple<ValType, ValType>, ValType (*)(CompareType, CompareType, ValType, ValType)>> {};

/** \brief
 *     Struct equivalent of the SelectTestParamStruct tuple.
 *
 *  \tparam CompareType
 *     The type of the compare child of the select opcode.
 *
 *  \tparam ValType
 *     The type that the select opcode returns.
 */
template <typename CompareType, typename ValType>
struct SelectTestParamStruct
   {
   CompareType c1;
   CompareType c2;
   ValType v1;
   ValType v2;
   ValType (*oracle)(CompareType, CompareType, ValType, ValType);
   };

/** \brief
 *     Given an instance of SelectTestParamStruct, returns an equivalent instance of SelectTestParamStruct.
 */
template <typename CompareType, typename ValType>
SelectTestParamStruct<CompareType, ValType> to_struct(std::tuple<std::tuple<CompareType, CompareType>, std::tuple<ValType, ValType>, ValType (*)(CompareType, CompareType, ValType, ValType)> param)
   {
   SelectTestParamStruct<CompareType, ValType> s;

   s.c1 = std::get<0>(std::get<0>(param));
   s.c2 = std::get<1>(std::get<0>(param));
   s.v1 = std::get<0>(std::get<1>(param));
   s.v2 = std::get<1>(std::get<1>(param));
   s.oracle = std::get<2>(param);
   return s;
   }

/** \brief
 *     The oracle function which computes the expected result of the select test.
 *
 *  \tparam CompareType
 *     The type of the values compared by oracle function.
 *  \tparam ValType
 *     The return type of the oracle function.
 */
template <typename CompareType, typename ValType>
static ValType xselectOracle(CompareType c1, CompareType c2, ValType v1, ValType v2)
   {
   return ((c1 < c2) ? v1 : v2);
   }

/** \brief
 *     The function computes the input vector of pairs for the compare node.
 */
template <typename T>
static std::vector<std::tuple<T, T>> compareInputs()
   {
   std::tuple<T, T> inputArray[] =
      {
      std::make_pair(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()),
      std::make_pair(std::numeric_limits<T>::max(), std::numeric_limits<T>::min()),
      std::make_pair(std::numeric_limits<T>::min(), std::numeric_limits<T>::min()),
      std::make_pair(0, std::numeric_limits<T>::max()),
      std::make_pair(0, std::numeric_limits<T>::min()),
      std::make_pair(std::numeric_limits<T>::min(), 0),
      std::make_pair(std::numeric_limits<T>::max(), 0),
      };
   return std::vector<std::tuple<T, T>>(inputArray, inputArray + sizeof(inputArray)/sizeof(std::tuple<T, T>));
   }

/** \brief
 *     The function computes the true/false result vector of pairs for the select node.
 */
template <typename T>
static std::vector<std::tuple<T, T>> resultInputs()
   {
   std::tuple<T, T> inputArray[] =
      {
      std::make_pair(1, 0),
      std::make_pair(0, 1),
      };
   return std::vector<std::tuple<T, T>>(inputArray, inputArray + sizeof(inputArray)/sizeof(std::tuple<T, T>));
   }

class Int32SelectInt32CompareTest : public SelectTest<int32_t, int32_t> {};

TEST_P(Int32SelectInt32CompareTest, UsingLoadParam) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (icmplt"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))"
        ")))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t, int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectInt32CompareTest, UsingConst) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int32"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (icmplt"
        "          (iconst %d)"
        "          (iconst %d))"
        "        (iconst %d)"
        "        (iconst %d))"
        ")))",
        param.c1,
        param.c2,
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point());
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int32SelectInt32CompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<int32_t>()),
            ::testing::ValuesIn(resultInputs<int32_t>()),
            ::testing::Values(xselectOracle<int32_t, int32_t>)));

class Int64SelectInt64CompareTest : public SelectTest<int64_t, int64_t> {};

TEST_P(Int64SelectInt64CompareTest, UsingLoadParam) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64, Int64, Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (lcmplt"
        "          (lload parm=0)"
        "          (lload parm=1))"
        "        (lload parm=2)"
        "        (lload parm=3))"
        ")))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t, int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int64SelectInt64CompareTest, UsingConst) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int64"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (lcmplt"
        "          (lconst %" OMR_PRId64 ")"
        "          (lconst %" OMR_PRId64 "))"
        "        (lconst %" OMR_PRId64 ")"
        "        (lconst %" OMR_PRId64 "))"
        ")))",
        param.c1,
        param.c2,
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point());
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int64SelectInt64CompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<int64_t>()),
            ::testing::ValuesIn(resultInputs<int64_t>()),
            ::testing::Values(xselectOracle<int64_t, int64_t>)));

class Int64SelectDoubleCompareTest : public SelectTest<double, int64_t> {};

TEST_P(Int64SelectDoubleCompareTest, UsingLoadParam) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Double, Double, Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (dcmplt"
        "          (dload parm=0)"
        "          (dload parm=1))"
        "        (lload parm=2)"
        "        (lload parm=3))"
        ")))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(double, double, int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int64SelectDoubleCompareTest, UsingConst) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Double, Double]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (dcmplt"
        "          (dload parm=0)"
        "          (dload parm=1))"
        "        (lconst %" OMR_PRId64 ")"
        "        (lconst %" OMR_PRId64 "))"
        ")))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(double, double)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int64SelectDoubleCompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<double>()),
            ::testing::ValuesIn(resultInputs<int64_t>()),
            ::testing::Values(xselectOracle<double, int64_t>)));

class Int32SelectDoubleCompareTest : public SelectTest<double, int32_t> {};

TEST_P(Int32SelectDoubleCompareTest, UsingLoadParam) {
    SKIP_ON_AARCH64(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Double, Double, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (dcmplt"
        "          (dload parm=0)"
        "          (dload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))"
        ")))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, double, int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectDoubleCompareTest, UsingConst) {
    SKIP_ON_AARCH64(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Double, Double]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (dcmplt"
        "          (dload parm=0)"
        "          (dload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))"
        ")))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, double)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int32SelectDoubleCompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<double>()),
            ::testing::ValuesIn(resultInputs<int32_t>()),
            ::testing::Values(xselectOracle<double, int32_t>)));


class ShortSelectDoubleCompareTest : public SelectTest<double, int16_t> {};

TEST_P(ShortSelectDoubleCompareTest, UsingLoadParam) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Double, Double, Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (dcmplt"
        "            (dload parm=0)"
        "            (dload parm=1))"
        "          (sload parm=2)"
        "          (sload parm=3))"
        "))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(double, double, int16_t, int16_t)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(ShortSelectDoubleCompareTest, UsingConst) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Double, Double]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (dcmplt"
        "            (dload parm=0)"
        "            (dload parm=1))"
        "          (sconst %d)"
        "          (sconst %d))"
        "))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(double, double)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectTest, ShortSelectDoubleCompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<double>()),
            ::testing::ValuesIn(resultInputs<int16_t>()),
            ::testing::Values((xselectOracle<double, int16_t>))));

class FloatSelectInt32CompareTest : public SelectTest<int32_t, float> {};

TEST_P(FloatSelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Int32, Int32, Float, Float]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (icmplt"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (fload parm=2)"
        "        (fload parm=3))"
        ")))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(int32_t, int32_t, float, float)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(FloatSelectInt32CompareTest, UsingConst) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Float"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (icmplt"
        "          (iconst %d)"
        "          (iconst %d))"
        "        (fconst %f)"
        "        (fconst %f))"
        ")))",
        param.c1,
        param.c2,
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(void)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point());
}

INSTANTIATE_TEST_CASE_P(SelectTest, FloatSelectInt32CompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<int32_t>()),
            ::testing::ValuesIn(resultInputs<float>()),
            ::testing::Values(xselectOracle<int32_t, float>)));

class DoubleSelectInt32CompareTest : public SelectTest<int32_t, double> {};

TEST_P(DoubleSelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Int32, Int32, Double, Double]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (icmplt"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (dload parm=2)"
        "        (dload parm=3))"
        ")))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(int32_t, int32_t, double, double)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(DoubleSelectInt32CompareTest, UsingConst) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Double"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (icmplt"
        "          (iconst %d)"
        "          (iconst %d))"
        "        (dconst %f)"
        "        (dconst %f))"
        ")))",
        param.c1,
        param.c2,
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(void)>();
    ASSERT_EQ(param.oracle(param.c1, param.c2, param.v1, param.v2), entry_point());
}

INSTANTIATE_TEST_CASE_P(SelectTest, DoubleSelectInt32CompareTest,
        ::testing::Combine(
            ::testing::ValuesIn(compareInputs<int32_t>()),
            ::testing::ValuesIn(resultInputs<double>()),
            ::testing::Values(xselectOracle<int32_t, double>)));
