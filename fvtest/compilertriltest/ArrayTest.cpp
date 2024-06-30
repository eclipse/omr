/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
#include <vector>

static const int32_t returnValueForArraycmpGreaterThan = 2;
static const int32_t returnValueForArraycmpLessThan = 1;
static const int32_t returnValueForArraycmpEqual = 0;
/**
 * @brief TestFixture class for arraycmp test
 *
 * @details Used for arraycmp test with the arrays with same data.
 * The parameter is the length parameter for the arraycmp evaluator.
 */
class ArraycmpEqualTest : public TRTest::JitTest, public ::testing::WithParamInterface<int64_t> {};
/**
 * @brief TestFixture class for arraycmp test
 *
 * @details Used for arraycmp test which has mismatched element.
 * The first parameter is the length parameter for the arraycmp evaluator.
 * The second parameter is the offset of the mismatched element in the arrays.
 */
class ArraycmpNotEqualTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<int64_t, int64_t>> {};

TEST_P(ArraycmpEqualTest, ArraycmpSameArray) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = GetParam();
    char inputTrees[1024] = {0};
    /*
     * "address=0" parameter is needed for arraycmp opcode because "Call" property is set to the opcode.
     */
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(returnValueForArraycmpEqual, entry_point(&s1[0], &s1[0]));
}

TEST_P(ArraycmpEqualTest, ArraycmpEqualConstLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = GetParam();
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(returnValueForArraycmpEqual, entry_point(&s1[0], &s2[0]));
}

TEST_P(ArraycmpEqualTest, ArraycmpEqualVariableLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = GetParam();
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address, Int64]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *, int64_t)>();
    EXPECT_EQ(returnValueForArraycmpEqual, entry_point(&s1[0], &s2[0], length));
}

INSTANTIATE_TEST_CASE_P(ArraycmpTest, ArraycmpEqualTest, ::testing::Range(static_cast<int64_t>(1), static_cast<int64_t>(128)));

TEST_P(ArraycmpNotEqualTest, ArraycmpGreaterThanConstLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = std::get<0>(GetParam());
    auto offset = std::get<1>(GetParam());
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x81;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(returnValueForArraycmpGreaterThan, entry_point(&s1[0], &s2[0]));
}

TEST_P(ArraycmpNotEqualTest, ArraycmpGreaterThanVariableLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = std::get<0>(GetParam());
    auto offset = std::get<1>(GetParam());
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address, Int64]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x81;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *, int64_t)>();
    EXPECT_EQ(returnValueForArraycmpGreaterThan, entry_point(&s1[0], &s2[0], length));
}

TEST_P(ArraycmpNotEqualTest, ArraycmpLessThanConstLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = std::get<0>(GetParam());
    auto offset = std::get<1>(GetParam());
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x21;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(returnValueForArraycmpLessThan, entry_point(&s1[0], &s2[0]));
}

TEST_P(ArraycmpNotEqualTest, ArraycmpLessThanVariableLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = std::get<0>(GetParam());
    auto offset = std::get<1>(GetParam());
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int32 args=[Address, Address, Int64]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x21;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *, int64_t)>();
    EXPECT_EQ(returnValueForArraycmpLessThan, entry_point(&s1[0], &s2[0], length));
}

static std::vector<std::tuple<int64_t, int64_t>> createArraycmpNotEqualParam() {
  std::vector<std::tuple<int64_t, int64_t>> v;
  /* Small arrays */
  for (int i = 1; i < 32; i++) {
    for (int j = 0; j < i; j++) {
      v.push_back(std::make_tuple(i, j));
    }
  }
  /* Variation of the offset of mismatched element in 128 bytes array */
  for (int i = 0; i < 128; i++) {
    v.push_back(std::make_tuple(128, i));
  }
  /* Medium size arrays with the mismatched element near the end of the arrays */
  for (int i = 120; i < 136; i++) {
    for (int j = 96; j < i; j++) {
      v.push_back(std::make_tuple(i, j));
    }
  }
  /* A large size array with the mismatched element near the end of the array */
  for (int i = 4000; i < 4096; i++) {
    v.push_back(std::make_tuple(4096, i));
  }
  return v;
}
INSTANTIATE_TEST_CASE_P(ArraycmpTest, ArraycmpNotEqualTest, ::testing::ValuesIn(createArraycmpNotEqualParam()));


/**
 * @brief TestFixture class for arraycmplen test
 *
 * @details Used for arraycmplen test with the arrays with same data.
 * The parameter is the length parameter for the arraycmp evaluator.
 */
class ArraycmplenEqualTest : public TRTest::JitTest, public ::testing::WithParamInterface<int64_t> {};
/**
 * @brief TestFixture class for arraycmplen test
 *
 * @details Used for arraycmplen test which has mismatched element.
 * The first parameter is the length parameter for the arraycmp evaluator.
 * The second parameter is the offset of the mismatched element in the arrays.
 */
class ArraycmplenNotEqualTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<int64_t, int64_t>> {};

TEST_P(ArraycmplenEqualTest, ArraycmpLenSameArray) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = GetParam();
    char inputTrees[1024] = {0};
    /*
     * "address=0" parameter is needed for arraycmp opcode because "Call" property is set to the opcode.
     */
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Address, Address]"
      "  (block"
      "    (lreturn"
      "      (arraycmplen address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int64_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(length, entry_point(&s1[0], &s1[0]));
}

TEST_P(ArraycmplenEqualTest, ArraycmpLenEqualConstLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = GetParam();
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Address, Address]"
      "  (block"
      "    (lreturn"
      "      (arraycmplen address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int64_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(length, entry_point(&s1[0], &s2[0]));
}

TEST_P(ArraycmplenEqualTest, ArraycmpLenEqualVariableLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = GetParam();
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Address, Address, Int64]"
      "  (block"
      "    (lreturn"
      "      (arraycmplen address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int64_t (*)(unsigned char *, unsigned char *, int64_t)>();
    EXPECT_EQ(length, entry_point(&s1[0], &s2[0], length));
}

INSTANTIATE_TEST_CASE_P(ArraycmplenTest, ArraycmplenEqualTest, ::testing::Range(static_cast<int64_t>(1), static_cast<int64_t>(128)));

TEST_P(ArraycmplenNotEqualTest, ArraycmpLenNotEqualConstLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = std::get<0>(GetParam());
    auto offset = std::get<1>(GetParam());
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Address, Address]"
      "  (block"
      "    (lreturn"
      "      (arraycmplen address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lconst %" OMR_PRId64 ")))))",
      length
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x3f;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(unsigned char *, unsigned char *)>();
    EXPECT_EQ(offset, entry_point(&s1[0], &s2[0]));
}

TEST_P(ArraycmplenNotEqualTest, ArraycmpLenNotEqualVariableLen) {
    SKIP_ON_ARM(MissingImplementation);
    SKIP_ON_RISCV(MissingImplementation);

    auto length = std::get<0>(GetParam());
    auto offset = std::get<1>(GetParam());
    char inputTrees[1024] = {0};
    std::snprintf(inputTrees, sizeof(inputTrees),
      "(method return=Int64 args=[Address, Address, Int64]"
      "  (block"
      "    (lreturn"
      "      (arraycmplen address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (lload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x3f;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(unsigned char *, unsigned char *, int64_t)>();
    EXPECT_EQ(offset, entry_point(&s1[0], &s2[0], length));
}

INSTANTIATE_TEST_CASE_P(ArraycmplenTest, ArraycmplenNotEqualTest, ::testing::ValuesIn(createArraycmpNotEqualParam()));
