/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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

#include <gtest/gtest.h>
#include "JitTest.hpp"
#include "default_compiler.hpp"

// C++11 upgrade (Issue #1916).
template <typename ValType>
class IfxcmpgeReductionTest : public TRTest::TestWithPortLib, public ::testing::WithParamInterface<std::tuple<ValType, ValType, int32_t (*)(ValType, ValType)>>
   {
   public:

   static void SetUpTestCase()
      {
      // Disable global value propagation and local common subexpression elimination.
      // Otherwise, the trees are optimized into nothing.
      const char *options = "-Xjit:acceptHugeMethods,enableBasicBlockHoisting,omitFramePointer,"
         "useILValidator,paranoidoptcheck,disableGlobalVP,disableLocalCSE";

      auto initSuccess = initializeJitWithOptions(const_cast<char*>(options));

      ASSERT_TRUE(initSuccess) << "Failed to initialize the JIT.";
      }

   static void TearDownTestCase()
      {
      shutdownJit();
      }
   };

/** \brief
 *     Struct equivalent of the IfxcmpgeReductionParamType tuple.
 *
 *  \tparam ValType
 *     The type of the method parameter in the compilation.
 */
template <typename ValType>
struct IfxcmpgeReductionParamStruct
   {
   ValType val1;
   ValType val2;
   int32_t (*oracle)(ValType, ValType);
   };

/** \brief
 *     Given an instance of IfxcmpgeReductionParamType, returns an equivalent instance of IfxcmpgeReductionParamStruct.
 */
template <typename ValType>
IfxcmpgeReductionParamStruct<ValType> to_struct(std::tuple<ValType, ValType, int32_t (*)(ValType, ValType)> param)
   {
   IfxcmpgeReductionParamStruct<ValType> s;

   s.val1 = std::get<0>(param);
   s.val2 = std::get<1>(param);
   s.oracle = std::get<2>(param);

   return s;
   }

/** \brief
 *     The oracle function which computes the expected result of the reduction tests.
 *
 *  \tparam T
 *     The type of the parameters of the oracle function.
 */
template <typename T>
static int32_t ifxcmpgeReductionOracle(T a, T b)
   {
   return (a & b) >= b;
   }

class Int8ReductionTest : public IfxcmpgeReductionTest<int8_t> {};

TEST_P(Int8ReductionTest, Reduction)
   {
   SKIP_ON_AARCH64(MissingImplementation) << "The AArch64 codegen does not yet support ifbcmpge (see issue #5893)";
   SKIP_ON_RISCV(MissingImplementation);

   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int32]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (ifbcmpge target=\"t\""
         "      (band"
         "        (i2b"
         "          (iload parm=0) )"
         "        (bconst %d) )"
         "      (bconst %d) ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of the 'bconst' in 'band'
         param.val2  // value of the 'bconst' in 'ifbcmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int8_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, Int8ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<int8_t>()),
            ::testing::ValuesIn(TRTest::const_values<int8_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<int8_t>)));

class UInt8ReductionTest : public IfxcmpgeReductionTest<uint8_t> {};

TEST_P(UInt8ReductionTest, Reduction)
   {
   SKIP_ON_AARCH64(MissingImplementation) << "The AArch64 codegen does not yet support ifbucmpge (see issue #5893)";
   SKIP_ON_RISCV(MissingImplementation);

   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int32]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (ifbucmpge target=\"t\""
         "      (band"
         "        (i2b"
         "          (iload parm=0) )"
         "        (bconst %d) )"
         "      (bconst %d) ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'bconst' in 'band'
         param.val2  // value of 'bconst' in 'ifbucmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int8_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, UInt8ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<uint8_t>()),
            ::testing::ValuesIn(TRTest::const_values<uint8_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<uint8_t>)));

class Int16ReductionTest : public IfxcmpgeReductionTest<int16_t> {};

TEST_P(Int16ReductionTest, Reduction)
   {
   SKIP_ON_AARCH64(MissingImplementation) << "The AArch64 codegen does not yet support ifscmpge (see issue #5893)";
   SKIP_ON_RISCV(MissingImplementation);

   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int32]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (ifscmpge target=\"t\""
         "      (sand"
         "        (i2s"
         "          (iload parm=0) )"
         "        (sconst %d) )"
         "      (sconst %d) ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'sconst' in 'sand'
         param.val2  // value of 'sconst' in 'ifscmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int16_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, Int16ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<int16_t>()),
            ::testing::ValuesIn(TRTest::const_values<int16_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<int16_t>)));

class UInt16ReductionTest : public IfxcmpgeReductionTest<uint16_t> {};

TEST_P(UInt16ReductionTest, Reduction)
   {
   SKIP_ON_AARCH64(MissingImplementation) << "The AArch64 codegen does not yet support ifsucmpge (see issue #5893)";
   SKIP_ON_RISCV(MissingImplementation);

   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int32]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (ifsucmpge target=\"t\""
         "      (sand"
         "        (i2s"
         "          (iload parm=0) )"
         "        (sconst %d) )"
         "      (sconst %d) ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'sconst' in 'sand'
         param.val2  // value of 'sconst' in 'ifsucmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int16_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, UInt16ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<uint16_t>()),
            ::testing::ValuesIn(TRTest::const_values<uint16_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<uint16_t>)));

class Int32ReductionTest : public IfxcmpgeReductionTest<int32_t> {};

TEST_P(Int32ReductionTest, Reduction)
   {
   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int32]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (ificmpge target=\"t\""
         "      (iand"
         "        (iload parm=0)"
         "        (iconst %d) )"
         "      (iconst %d) ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'iconst' in 'iand'
         param.val2  // value of 'iconst' in 'ificmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, Int32ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<int32_t>()),
            ::testing::ValuesIn(TRTest::const_values<int32_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<int32_t>)));

class UInt32ReductionTest : public IfxcmpgeReductionTest<uint32_t> {};

TEST_P(UInt32ReductionTest, Reduction)
   {
   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int32]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (ifiucmpge target=\"t\""
         "      (iand"
         "        (iload parm=0)"
         "        (iconst %d) )"
         "      (iconst %d) ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'iconst' in 'iand'
         param.val2  // value of 'iconst' in 'ifiucmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, UInt32ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<uint32_t>()),
            ::testing::ValuesIn(TRTest::const_values<uint32_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<uint32_t>)));

class Int64ReductionTest : public IfxcmpgeReductionTest<int64_t> {};

TEST_P(Int64ReductionTest, Reduction)
   {
   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int64]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (iflcmpge target=\"t\""
         "      (land"
         "        (lload parm=0)"
         "        (lconst %" OMR_PRId64 ") )"
         "      (lconst %" OMR_PRId64 ") ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'lconst' in 'land'
         param.val2  // value of 'lconst' in 'iflcmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, Int64ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<int64_t>()),
            ::testing::ValuesIn(TRTest::const_values<int64_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<int64_t>)));

class UInt64ReductionTest : public IfxcmpgeReductionTest<uint64_t> {};

TEST_P(UInt64ReductionTest, Reduction)
   {
   auto param = to_struct(GetParam());

   char inputTrees[600] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int32 args=[Int64]"
         "  (block name=\"b\" fallthrough=\"f\""
         "    (iflucmpge target=\"t\""
         "      (land"
         "        (lload parm=0)"
         "        (lconst %" OMR_PRIu64 ") )"
         "      (lconst %" OMR_PRIu64 ") ) )"
         "  (block name=\"f\""
         "    (ireturn (iconst 0) ) )"
         "  (block name=\"t\""
         "    (ireturn (iconst 1) ) ) )",

         param.val2, // value of 'lconst' in 'land'
         param.val2  // value of 'lconst' in 'iflucmpge'
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int64_t)>();

   ASSERT_EQ(param.oracle(param.val1, param.val2), entry_point(param.val1));
   }

INSTANTIATE_TEST_CASE_P(IfxcmpgeReductionTest, UInt64ReductionTest,
        ::testing::Combine(
            ::testing::ValuesIn(TRTest::const_values<uint64_t>()),
            ::testing::ValuesIn(TRTest::const_values<uint64_t>()),
            ::testing::Values(ifxcmpgeReductionOracle<uint64_t>)));
