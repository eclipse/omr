/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

enum class Comparison {
   eq, ne, lt, ge, gt, le
};

template <typename ValType>
class SelectTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<int32_t, std::tuple<ValType, ValType>>> {};

template <typename ValType>
struct SelectTestParamStruct {
    int32_t selector;
    ValType v1;
    ValType v2;
};

template <typename ValType>
SelectTestParamStruct<ValType> to_struct(std::tuple<int32_t, std::tuple<ValType, ValType>> param) {
    SelectTestParamStruct<ValType> s;

    s.selector = std::get<0>(param);
    s.v1 = std::get<0>(std::get<1>(param));
    s.v2 = std::get<1>(std::get<1>(param));

    return s;
}

template <typename CompareType, typename ValType>
class SelectCompareTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<Comparison, std::tuple<CompareType, CompareType>, std::tuple<ValType, ValType>>> {};

template <typename CompareType, typename ValType>
struct SelectCompareTestParamStruct {
    Comparison cmp;
    CompareType c1;
    CompareType c2;
    ValType v1;
    ValType v2;
};

template <typename CompareType, typename ValType>
SelectCompareTestParamStruct<CompareType, ValType> to_struct(std::tuple<Comparison, std::tuple<CompareType, CompareType>, std::tuple<ValType, ValType>> param) {
    SelectCompareTestParamStruct<CompareType, ValType> s;

    s.cmp = std::get<0>(param);
    s.c1 = std::get<0>(std::get<1>(param));
    s.c2 = std::get<1>(std::get<1>(param));
    s.v1 = std::get<0>(std::get<2>(param));
    s.v2 = std::get<1>(std::get<2>(param));

    return s;
}

template <typename CompareType>
static const char* xcmpSuffix(Comparison cmp) {
    switch (cmp) {
        case Comparison::eq:
            return "eq";
        case Comparison::ne:
            return OMR::IsFloatingPoint<CompareType>::VALUE ? "neu" : "ne";
        case Comparison::lt:
            return "lt";
        case Comparison::ge:
            return "ge";
        case Comparison::gt:
            return "gt";
        case Comparison::le:
            return "le";
        default:
            return "";
    }
}

std::ostream& operator<<(std::ostream& os, Comparison cmp) {
    return os << "Comparison::" << xcmpSuffix<int32_t>(cmp);
}

template <typename CompareType>
static int32_t xcmpOracle(Comparison cmp, CompareType c1, CompareType c2) {
    switch (cmp) {
        case Comparison::eq:
            return c1 == c2;
        case Comparison::ne:
            return c1 != c2;
        case Comparison::lt:
            return c1 < c2;
        case Comparison::ge:
            return c1 >= c2;
        case Comparison::gt:
            return c1 > c2;
        case Comparison::le:
            return c1 <= c2;
        default:
            return 0;
    }
}

template <typename ValType>
static ValType xselectOracle(int32_t sel, ValType v1, ValType v2) {
   return sel ? v1 : v2;
}

template <typename T>
static typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE, std::vector<std::tuple<T, T>>>::Type compareInputs() {
    std::tuple<T, T> inputArray[] = {
        std::make_pair(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()),
        std::make_pair(std::numeric_limits<T>::max(), std::numeric_limits<T>::min()),
        std::make_pair(0, std::numeric_limits<T>::max()),
        std::make_pair(0, std::numeric_limits<T>::min()),
        std::make_pair(0, 0),
    };
    return std::vector<std::tuple<T, T>>(inputArray, inputArray + sizeof(inputArray) / sizeof(*inputArray));
}

template <typename T>
static typename OMR::EnableIf<OMR::IsFloatingPoint<T>::VALUE, std::vector<std::tuple<T, T>>>::Type compareInputs() {
    std::tuple<T, T> inputArray[] = {
        std::make_pair(-std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity()),
        std::make_pair(std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity()),
        std::make_pair(T(0), std::numeric_limits<T>::infinity()),
        std::make_pair(T(0), -std::numeric_limits<T>::infinity()),
        std::make_pair(T(0), std::numeric_limits<T>::quiet_NaN()),
        std::make_pair(T(0), T(0)),
    };
    return std::vector<std::tuple<T, T>>(inputArray, inputArray + sizeof(inputArray) / sizeof(*inputArray));
}

template <typename T>
static std::vector<std::tuple<T, T>> resultInputs() {
    std::tuple<T, T> inputArray[] = {
        std::make_pair(T(1), T(0)),
        std::make_pair(T(0), T(1)),
        std::make_pair(T(-1), T(0)),
        std::make_pair(T(0), T(-1)),
        std::make_pair(T(5), T(7)),
    };
    return std::vector<std::tuple<T, T>>(inputArray, inputArray + sizeof(inputArray) / sizeof(*inputArray));
}

static std::vector<int32_t> selectors() {
    int32_t inputArray[] = { 0, 1, 2, -1 };
    return std::vector<int32_t>(inputArray, inputArray + sizeof(inputArray) / sizeof(*inputArray));
}

static std::vector<Comparison> allComparisons() {
    Comparison comparisons[] = {
        Comparison::eq,
        Comparison::ne,
        Comparison::lt,
        Comparison::ge,
        Comparison::gt,
        Comparison::le,
    };
    return std::vector<Comparison>(comparisons, comparisons + sizeof(comparisons) / sizeof(*comparisons));
}

class Int8SelectTest : public SelectTest<int8_t> {};

TEST_P(Int8SelectTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32, Int8, Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (bselect"
        "          (iload parm=0)"
        "          (bload parm=1)"
        "          (bload parm=2))))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int32_t, int8_t, int8_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector, param.v1, param.v2));
}

TEST_P(Int8SelectTest, UsingConstSelector) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int8, Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (bselect"
        "          (iconst %d)"
        "          (bload parm=0)"
        "          (bload parm=1))))))",
        param.selector);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t, int8_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.v1, param.v2));
}

TEST_P(Int8SelectTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (bselect"
        "          (iload parm=0)"
        "          (bconst %" OMR_PRId8 ")"
        "          (bconst %" OMR_PRId8 "))))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector));
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int8SelectTest,
    ::testing::Combine(
        ::testing::ValuesIn(selectors()),
        ::testing::ValuesIn(resultInputs<int8_t>())));

class Int16SelectTest : public SelectTest<int16_t> {};

TEST_P(Int16SelectTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32, Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (iload parm=0)"
        "          (sload parm=1)"
        "          (sload parm=2))))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int32_t, int16_t, int16_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector, param.v1, param.v2));
}

TEST_P(Int16SelectTest, UsingConstSelector) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (iconst %d)"
        "          (sload parm=0)"
        "          (sload parm=1))))))",
        param.selector);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t, int16_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.v1, param.v2));
}

TEST_P(Int16SelectTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (iload parm=0)"
        "          (sconst %" OMR_PRId16 ")"
        "          (sconst %" OMR_PRId16 "))))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector));
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int16SelectTest,
    ::testing::Combine(
        ::testing::ValuesIn(selectors()),
        ::testing::ValuesIn(resultInputs<int16_t>())));

class Int32SelectTest : public SelectTest<int32_t> {};

TEST_P(Int32SelectTest, UsingLoadParam) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (iload parm=0)"
        "        (iload parm=1)"
        "        (iload parm=2)))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector, param.v1, param.v2));
}

TEST_P(Int32SelectTest, UsingConstSelector) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (iconst %d)"
        "        (iload parm=0)"
        "        (iload parm=1)))))",
        param.selector);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.v1, param.v2));
}

TEST_P(Int32SelectTest, UsingConstValues) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (iload parm=0)"
        "        (iconst %d)"
        "        (iconst %d)))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector));
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int32SelectTest,
    ::testing::Combine(
        ::testing::ValuesIn(selectors()),
        ::testing::ValuesIn(resultInputs<int32_t>())));

class Int64SelectTest : public SelectTest<int64_t> {};

TEST_P(Int64SelectTest, UsingLoadParam) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32, Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (iload parm=0)"
        "        (lload parm=1)"
        "        (lload parm=2)))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t, int64_t, int64_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector, param.v1, param.v2));
}

TEST_P(Int64SelectTest, UsingConstSelector) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (iconst %d)"
        "        (lload parm=0)"
        "        (lload parm=1)))))",
        param.selector);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.v1, param.v2));
}

TEST_P(Int64SelectTest, UsingConstValues) {
    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (iload parm=0)"
        "        (lconst %" OMR_PRId64 ")"
        "        (lconst %" OMR_PRId64 ")))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector));
}

INSTANTIATE_TEST_CASE_P(SelectTest, Int64SelectTest,
    ::testing::Combine(
        ::testing::ValuesIn(selectors()),
        ::testing::ValuesIn(resultInputs<int64_t>())));

class FloatSelectTest : public SelectTest<float> {};

TEST_P(FloatSelectTest, UsingLoadParam) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Int32, Float, Float]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (iload parm=0)"
        "        (fload parm=1)"
        "        (fload parm=2)))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(int32_t, float, float)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector, param.v1, param.v2));
}

TEST_P(FloatSelectTest, UsingConstSelector) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Float, Float]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (iconst %d)"
        "        (fload parm=0)"
        "        (fload parm=1)))))",
        param.selector);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(float, float)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.v1, param.v2));
}

TEST_P(FloatSelectTest, UsingConstValues) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Int32]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (iload parm=0)"
        "        (fconst %f)"
        "        (fconst %f)))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector));
}

INSTANTIATE_TEST_CASE_P(SelectTest, FloatSelectTest,
    ::testing::Combine(
        ::testing::ValuesIn(selectors()),
        ::testing::ValuesIn(resultInputs<float>())));

class DoubleSelectTest : public SelectTest<double> {};

TEST_P(DoubleSelectTest, UsingLoadParam) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Int32, Double, Double]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (iload parm=0)"
        "        (dload parm=1)"
        "        (dload parm=2)))))");
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(int32_t, double, double)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector, param.v1, param.v2));
}

TEST_P(DoubleSelectTest, UsingConstSelector) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Double, Double]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (iconst %d)"
        "        (dload parm=0)"
        "        (dload parm=1)))))",
        param.selector);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(double, double)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.v1, param.v2));
}

TEST_P(DoubleSelectTest, UsingConstValues) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Int32]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (iload parm=0)"
        "        (dconst %f)"
        "        (dconst %f)))))",
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(int32_t)>();
    ASSERT_EQ(xselectOracle(param.selector, param.v1, param.v2), entry_point(param.selector));
}

INSTANTIATE_TEST_CASE_P(SelectTest, DoubleSelectTest,
    ::testing::Combine(
        ::testing::ValuesIn(selectors()),
        ::testing::ValuesIn(resultInputs<double>())));

class Int8SelectInt32CompareTest : public SelectCompareTest<int32_t, int8_t> {};

TEST_P(Int8SelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32, Int32, Int8, Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (bselect"
        "          (icmp%s"
        "            (iload parm=0)"
        "            (iload parm=1))"
        "          (bload parm=2)"
        "          (bload parm=3))))))",
        xcmpSuffix<int32_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int32_t, int32_t, int8_t, int8_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int8SelectInt32CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32, Int8, Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (bselect"
        "          (icmp%s"
        "            (iload parm=0)"
        "            (iconst %d))"
        "          (bload parm=1)"
        "          (bload parm=2))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int32_t, int8_t, int8_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int8SelectInt32CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int8 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (bselect"
        "          (icmp%s"
        "            (iload parm=0)"
        "            (iload parm=1))"
        "          (bconst %" OMR_PRId8 ")"
        "          (bconst %" OMR_PRId8 "))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int8SelectInt32CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int32_t>()),
        ::testing::ValuesIn(resultInputs<int8_t>())));

class Int16SelectInt32CompareTest : public SelectCompareTest<int32_t, int16_t> {};

TEST_P(Int16SelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32, Int32, Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (icmp%s"
        "            (iload parm=0)"
        "            (iload parm=1))"
        "          (sload parm=2)"
        "          (sload parm=3))))))",
        xcmpSuffix<int32_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int32_t, int32_t, int16_t, int16_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int16SelectInt32CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32, Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (icmp%s"
        "            (iload parm=0)"
        "            (iconst %d))"
        "          (sload parm=1)"
        "          (sload parm=2))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int32_t, int16_t, int16_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int16SelectInt32CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int16 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (sselect"
        "          (icmp%s"
        "            (iload parm=0)"
        "            (iload parm=1))"
        "          (sconst %" OMR_PRId16 ")"
        "          (sconst %" OMR_PRId16 "))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int16SelectInt32CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int32_t>()),
        ::testing::ValuesIn(resultInputs<int16_t>())));

class Int32SelectInt32CompareTest : public SelectCompareTest<int32_t, int32_t> {};

TEST_P(Int32SelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))))))",
        xcmpSuffix<int32_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectInt32CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iconst %d))"
        "        (iload parm=1)"
        "        (iload parm=2))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int32SelectInt32CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int32SelectInt32CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int32_t>()),
        ::testing::ValuesIn(resultInputs<int32_t>())));

class Int64SelectInt32CompareTest : public SelectCompareTest<int32_t, int64_t> {};

TEST_P(Int64SelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32, Int32, Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (lload parm=2)"
        "        (lload parm=3))))))",
        xcmpSuffix<int32_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t, int32_t, int64_t, int64_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int64SelectInt32CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32, Int64, Int64]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iconst %d))"
        "        (lload parm=1)"
        "        (lload parm=2))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t, int64_t, int64_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int64SelectInt32CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int64 args=[Int32, Int32]"
        "  (block"
        "    (lreturn"
        "      (lselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (lconst %" OMR_PRId64 ")"
        "        (lconst %" OMR_PRId64 "))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int64SelectInt32CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int32_t>()),
        ::testing::ValuesIn(resultInputs<int64_t>())));

class FloatSelectInt32CompareTest : public SelectCompareTest<int32_t, float> {};

TEST_P(FloatSelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Int32, Int32, Float, Float]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (fload parm=2)"
        "        (fload parm=3))))))",
        xcmpSuffix<int32_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(int32_t, int32_t, float, float)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(FloatSelectInt32CompareTest, UsingConstCompare) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Int32, Float, Float]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iconst %d))"
        "        (fload parm=1)"
        "        (fload parm=2))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(int32_t, float, float)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(FloatSelectInt32CompareTest, UsingConstValues) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Int32, Int32]"
        "  (block"
        "    (freturn"
        "      (fselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (fconst %f)"
        "        (fconst %f))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, FloatSelectInt32CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int32_t>()),
        ::testing::ValuesIn(resultInputs<float>())));

class DoubleSelectInt32CompareTest : public SelectCompareTest<int32_t, double> {};

TEST_P(DoubleSelectInt32CompareTest, UsingLoadParam) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Int32, Int32, Double, Double]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (dload parm=2)"
        "        (dload parm=3))))))",
        xcmpSuffix<int32_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(int32_t, int32_t, double, double)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(DoubleSelectInt32CompareTest, UsingConstCompare) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Int32, Double, Double]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iconst %d))"
        "        (dload parm=1)"
        "        (dload parm=2))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(int32_t, double, double)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(DoubleSelectInt32CompareTest, UsingConstValues) {
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Int32, Int32]"
        "  (block"
        "    (dreturn"
        "      (dselect"
        "        (icmp%s"
        "          (iload parm=0)"
        "          (iload parm=1))"
        "        (dconst %f)"
        "        (dconst %f))))))",
        xcmpSuffix<int32_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, DoubleSelectInt32CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int32_t>()),
        ::testing::ValuesIn(resultInputs<double>())));

class Int32SelectInt8CompareTest : public SelectCompareTest<int8_t, int32_t> {};

TEST_P(Int32SelectInt8CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation) << "Opcode bcmpeq is not implemented";
    SKIP_ON_S390(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_S390X(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_X86(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";
    SKIP_ON_HAMMER(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int8, Int8, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (bcmp%s"
        "          (bload parm=0)"
        "          (bload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))))))",
        xcmpSuffix<int8_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int8_t, int8_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectInt8CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation) << "Opcode bcmpeq is not implemented";
    SKIP_ON_S390(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_S390X(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_X86(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";
    SKIP_ON_HAMMER(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int8, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (bcmp%s"
        "          (bload parm=0)"
        "          (bconst %" OMR_PRId8 "))"
        "        (iload parm=1)"
        "        (iload parm=2))))))",
        xcmpSuffix<int8_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int8_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int32SelectInt8CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation) << "Opcode bcmpeq is not implemented";
    SKIP_ON_S390(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_S390X(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_X86(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";
    SKIP_ON_HAMMER(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int8, Int8]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (bcmp%s"
        "          (bload parm=0)"
        "          (bload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))))))",
        xcmpSuffix<int8_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int8_t, int8_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int32SelectInt8CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int8_t>()),
        ::testing::ValuesIn(resultInputs<int32_t>())));

class Int32SelectInt16CompareTest : public SelectCompareTest<int16_t, int32_t> {};

TEST_P(Int32SelectInt16CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation) << "Opcode scmpeq is not implemented";
    SKIP_ON_S390(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_S390X(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_X86(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";
    SKIP_ON_HAMMER(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int16, Int16, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (scmp%s"
        "          (sload parm=0)"
        "          (sload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))))))",
        xcmpSuffix<int16_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int16_t, int16_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectInt16CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation) << "Opcode scmpeq is not implemented";
    SKIP_ON_S390(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_S390X(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_X86(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";
    SKIP_ON_HAMMER(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int16, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (scmp%s"
        "          (sload parm=0)"
        "          (sconst %" OMR_PRId16 "))"
        "        (iload parm=1)"
        "        (iload parm=2))))))",
        xcmpSuffix<int16_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int16_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int32SelectInt16CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation) << "Opcode scmpeq is not implemented";
    SKIP_ON_S390(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_S390X(KnownBug) << "The Z code generator crashes when a sub-integer compare is the first child of an integral select (#5499)";
    SKIP_ON_X86(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";
    SKIP_ON_HAMMER(KnownBug) << "The x86 code generator returns wrong results when a sub-integer compare is the first child of a select (#5501)";

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (scmp%s"
        "          (sload parm=0)"
        "          (sload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))))))",
        xcmpSuffix<int16_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int16_t, int16_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int32SelectInt16CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int16_t>()),
        ::testing::ValuesIn(resultInputs<int32_t>())));

class Int32SelectInt64CompareTest : public SelectCompareTest<int64_t, int32_t> {};

TEST_P(Int32SelectInt64CompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int64, Int64, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (lcmp%s"
        "          (lload parm=0)"
        "          (lload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))))))",
        xcmpSuffix<int64_t>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int64_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectInt64CompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int64, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (lcmp%s"
        "          (lload parm=0)"
        "          (lconst %" OMR_PRId64 "))"
        "        (iload parm=1)"
        "        (iload parm=2))))))",
        xcmpSuffix<int64_t>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int32SelectInt64CompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Int64, Int64]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (lcmp%s"
        "          (lload parm=0)"
        "          (lload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))))))",
        xcmpSuffix<int64_t>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int32SelectInt64CompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<int64_t>()),
        ::testing::ValuesIn(resultInputs<int32_t>())));

class Int32SelectFloatCompareTest : public SelectCompareTest<float, int32_t> {};

TEST_P(Int32SelectFloatCompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Float, Float, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (fcmp%s"
        "          (fload parm=0)"
        "          (fload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))))))",
        xcmpSuffix<float>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(float, float, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectFloatCompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    if (std::isnan(param.c2)) {
        SKIP_ON_ZOS(KnownBug) << "TRIL parser cannot handle NaN values on zOS (see issue #5183)";
        SKIP_ON_WINDOWS(KnownBug) << "TRIL parser cannot handle NaN values on Windows (see issue #5324)";
    }

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Float, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (fcmp%s"
        "          (fload parm=0)"
        "          (fconst %f))"
        "        (iload parm=1)"
        "        (iload parm=2))))))",
        xcmpSuffix<float>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(float, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int32SelectFloatCompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Float, Float]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (fcmp%s"
        "          (fload parm=0)"
        "          (fload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))))))",
        xcmpSuffix<float>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(float, float)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int32SelectFloatCompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<float>()),
        ::testing::ValuesIn(resultInputs<int32_t>())));

class Int32SelectDoubleCompareTest : public SelectCompareTest<double, int32_t> {};

TEST_P(Int32SelectDoubleCompareTest, UsingLoadParam) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Double, Double, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (dcmp%s"
        "          (dload parm=0)"
        "          (dload parm=1))"
        "        (iload parm=2)"
        "        (iload parm=3))))))",
        xcmpSuffix<double>(param.cmp));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, double, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2, param.v1, param.v2));
}

TEST_P(Int32SelectDoubleCompareTest, UsingConstCompare) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    if (std::isnan(param.c2)) {
        SKIP_ON_ZOS(KnownBug) << "TRIL parser cannot handle NaN values on zOS (see issue #5183)";
        SKIP_ON_WINDOWS(KnownBug) << "TRIL parser cannot handle NaN values on Windows (see issue #5324)";
    }

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Double, Int32, Int32]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (dcmp%s"
        "          (dload parm=0)"
        "          (dconst %f))"
        "        (iload parm=1)"
        "        (iload parm=2))))))",
        xcmpSuffix<double>(param.cmp),
        param.c2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, int32_t, int32_t)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.v1, param.v2));
}

TEST_P(Int32SelectDoubleCompareTest, UsingConstValues) {
    SKIP_ON_ARM(MissingImplementation);

    auto param = to_struct(GetParam());

    char inputTrees[512] = {0};
    Tril::format(inputTrees, sizeof(inputTrees),
        "(method return=Int32 args=[Double, Double]"
        "  (block"
        "    (ireturn"
        "      (iselect"
        "        (dcmp%s"
        "          (dload parm=0)"
        "          (dload parm=1))"
        "        (iconst %d)"
        "        (iconst %d))))))",
        xcmpSuffix<double>(param.cmp),
        param.v1,
        param.v2);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    int32_t compileResult = compiler.compile();
    ASSERT_EQ(0, compileResult) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(double, double)>();
    ASSERT_EQ(xselectOracle(xcmpOracle(param.cmp, param.c1, param.c2), param.v1, param.v2), entry_point(param.c1, param.c2));
}

INSTANTIATE_TEST_CASE_P(SelectCompareTest, Int32SelectDoubleCompareTest,
    ::testing::Combine(
        ::testing::ValuesIn(allComparisons()),
        ::testing::ValuesIn(compareInputs<double>()),
        ::testing::ValuesIn(resultInputs<int32_t>())));
