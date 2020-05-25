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

float f_max(float l, float r) {
  if (std::isnan(l)) return std::numeric_limits<float>::quiet_NaN();
  if (std::isnan(r)) return std::numeric_limits<float>::quiet_NaN();
  return std::max(l,r);
}

float f_min(float l, float r) {
  if (std::isnan(l)) return std::numeric_limits<float>::quiet_NaN();
  if (std::isnan(r)) return std::numeric_limits<float>::quiet_NaN();
  return std::min(l,r);
}

double dmax(double l, double r) {
  if (std::isnan(l)) return std::numeric_limits<double>::quiet_NaN();
  if (std::isnan(r)) return std::numeric_limits<double>::quiet_NaN();
  return std::max(l,r);
}

double dmin(double l, double r) {
  if (std::isnan(l)) return std::numeric_limits<double>::quiet_NaN();
  if (std::isnan(r)) return std::numeric_limits<double>::quiet_NaN();
  return std::min(l,r);
}

template <typename T>
bool smallFp_filter(std::tuple<T, T> a)
   {
   // workaround: avoid failure caused by snprintf("%f")
   auto a0 = std::get<0>(a);
   auto a1 = std::get<1>(a);
   return ((std::abs(a0) < 0.01 && a0 != 0.0) || (std::abs(a1) < 0.01 && a1 != 0.0));
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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}


INSTANTIATE_TEST_CASE_P(MaxMin, Int32MaxMin, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("imax", imax),
        std::make_tuple<const char*, int32_t(*)(int32_t, int32_t)>("imin", imin))));

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

    Tril::DefaultCompiler compiler(trees);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}


INSTANTIATE_TEST_CASE_P(MaxMin, Int64MaxMin, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t, int64_t>()),
    ::testing::Values(
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("lmax", lmax),
        std::make_tuple<const char*, int64_t(*)(int64_t, int64_t)>("lmin", lmin))));

class FloatMaxMin : public TRTest::BinaryOpTest<float> {};

TEST_P(FloatMaxMin, UsingConst) {
    SKIP_ON_X86(KnownBug) << "The X86 code generator currently doesn't support fmax/fmin (see issue #4276)";
    SKIP_ON_HAMMER(KnownBug) << "The AMD64 code generator currently doesn't support fmax/fmin (see issue #4276)";

    auto param = TRTest::to_struct(GetParam());

    if (std::isnan(param.lhs) || std::isnan(param.rhs)) {
       SKIP_ON_POWER(KnownBug) << "fmin / fmax returns wrong value for NaN on POWER (see issue #5156)";
       SKIP_ON_S390(KnownBug) << "fmin / fmax returns wrong value for NaN on Z (see issue #5157)";
       SKIP_ON_S390X(KnownBug) << "fmin / fmax returns wrong value for NaN on Z (see issue #5157)";
    }

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Float"
        "  (block"
        "    (freturn"
        "      (%s"
        "        (fconst %f )"
        "        (fconst %f ))"
        ")))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(FloatMaxMin, UsingLoadParam) {
    SKIP_ON_X86(KnownBug) << "The X86 code generator currently doesn't support fmax/fmin (see issue #4276)";
    SKIP_ON_HAMMER(KnownBug) << "The AMD64 code generator currently doesn't support fmax/fmin (see issue #4276)";

    auto param = TRTest::to_struct(GetParam());

    if (std::isnan(param.lhs) || std::isnan(param.rhs)) {
       SKIP_ON_POWER(KnownBug) << "fmin / fmax returns wrong value for NaN on POWER (see issue #5156)";
       SKIP_ON_S390(KnownBug) << "fmin / fmax returns wrong value for NaN on Z (see issue #5157)";
       SKIP_ON_S390X(KnownBug) << "fmin / fmax returns wrong value for NaN on Z (see issue #5157)";
    }

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Float args=[Float, Float]"
        "  (block"
        "    (freturn"
        "      (%s"
        "        (fload parm=0)"
        "        (fload parm=1))"
        ")))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<float (*)(float, float)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}


INSTANTIATE_TEST_CASE_P(MaxMin, FloatMaxMin, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<float, float>(), smallFp_filter<float>)),
    ::testing::Values(
        std::make_tuple<const char*, float(*)(float, float)>("fmax", f_max),
        std::make_tuple<const char*, float(*)(float, float)>("fmin", f_min))));

class DoubleMaxMin : public TRTest::BinaryOpTest<double> {};

TEST_P(DoubleMaxMin, UsingConst) {
    SKIP_ON_X86(KnownBug) << "The X86 code generator currently doesn't support dmax/dmin (see issue #4276)";
    SKIP_ON_HAMMER(KnownBug) << "The AMD64 code generator currently doesn't support dmax/dmin (see issue #4276)";

    auto param = TRTest::to_struct(GetParam());

    if (std::isnan(param.lhs) || std::isnan(param.rhs)) {
       SKIP_ON_POWER(KnownBug) << "dmin / dmax returns wrong value for NaN on POWER (see issue #5156)";
       SKIP_ON_S390(KnownBug) << "dmin / dmax returns wrong value for NaN on Z (see issue #5157)";
       SKIP_ON_S390X(KnownBug) << "dmin / dmax returns wrong value for NaN on Z (see issue #5157)";
    }

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Double"
        "  (block"
        "    (dreturn"
        "      (%s"
        "        (dconst %f )"
        "        (dconst %f ))"
        ")))",
        param.opcode.c_str(),
        param.lhs,
        param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(void)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point());
}

TEST_P(DoubleMaxMin, UsingLoadParam) {
    SKIP_ON_X86(KnownBug) << "The X86 code generator currently doesn't support dmax/dmin (see issue #4276)";
    SKIP_ON_HAMMER(KnownBug) << "The AMD64 code generator currently doesn't support dmax/dmin (see issue #4276)";

    auto param = TRTest::to_struct(GetParam());

    if (std::isnan(param.lhs) || std::isnan(param.rhs)) {
       SKIP_ON_POWER(KnownBug) << "dmin / dmax returns wrong value for NaN on POWER (see issue #5156)";
       SKIP_ON_S390(KnownBug) << "dmin / dmax returns wrong value for NaN on Z (see issue #5157)";
       SKIP_ON_S390X(KnownBug) << "dmin / dmax returns wrong value for NaN on Z (see issue #5157)";

    }

    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
        "(method return=Double args=[Double, Double]"
        "  (block"
        "    (dreturn"
        "      (%s"
        "        (dload parm=0)"
        "        (dload parm=1))"
        ")))",
        param.opcode.c_str());
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<double (*)(double, double)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}


INSTANTIATE_TEST_CASE_P(MaxMin, DoubleMaxMin, ::testing::Combine(
    ::testing::ValuesIn(
        TRTest::filter(TRTest::const_value_pairs<double, double>(), smallFp_filter<double>)),
    ::testing::Values(
        std::make_tuple<const char*, double(*)(double, double)>("dmax", dmax),
        std::make_tuple<const char*, double(*)(double, double)>("dmin", dmin))));
