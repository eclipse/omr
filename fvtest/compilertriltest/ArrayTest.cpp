/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "OpCodeTest.hpp"
#include "default_compiler.hpp"
#include "omrformatconsts.h"
#include <algorithm>
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
class ArraycmpEqualTest : public TRTest::JitTest, public ::testing::WithParamInterface<int32_t> {};
/**
 * @brief TestFixture class for arraycmp test
 *
 * @details Used for arraycmp test which has mismatched element.
 * The first parameter is the length parameter for the arraycmp evaluator.
 * The second parameter is the offset of the mismatched element in the arrays.
 */
class ArraycmpNotEqualTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {};

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
      "        (iconst %d)))))",
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
      "        (iconst %d)))))",
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
      "(method return=Int32 args=[Address, Address, Int32]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (iload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *, int32_t)>();
    EXPECT_EQ(returnValueForArraycmpEqual, entry_point(&s1[0], &s2[0], length));
}

INSTANTIATE_TEST_CASE_P(ArraycmpTest, ArraycmpEqualTest, ::testing::Range(1, 128));

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
      "        (iconst %d)))))",
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
      "(method return=Int32 args=[Address, Address, Int32]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (iload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x81;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *, int32_t)>();
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
      "        (iconst %d)))))",
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
      "(method return=Int32 args=[Address, Address, Int32]"
      "  (block"
      "    (ireturn"
      "      (arraycmp address=0 args=[Address, Address]"
      "        (aload parm=0)"
      "        (aload parm=1)"
      "        (iload parm=2)))))"
      );
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    std::vector<unsigned char> s1(length, 0x5c);
    std::vector<unsigned char> s2(length, 0x5c);
    s1[offset] = 0x21;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(unsigned char *, unsigned char *, int32_t)>();
    EXPECT_EQ(returnValueForArraycmpLessThan, entry_point(&s1[0], &s2[0], length));
}

template <typename intXX_t>
static std::vector<std::tuple<intXX_t, intXX_t>> createArraycmpNotEqualParam() {
  std::vector<std::tuple<intXX_t, intXX_t>> v;
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
INSTANTIATE_TEST_CASE_P(ArraycmpTest, ArraycmpNotEqualTest, ::testing::ValuesIn(createArraycmpNotEqualParam<int32_t>()));


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

INSTANTIATE_TEST_CASE_P(ArraycmplenTest, ArraycmplenNotEqualTest, ::testing::ValuesIn(createArraycmpNotEqualParam<int64_t>()));

template <typename T>
void assert_array_equals(std::vector<T> &s1, std::vector<T> &s2) {
  auto it1 = s1.begin();
  auto it2 = s2.begin();
  while (it1 != s1.end()) {
    ASSERT_EQ(*it1, *it2) << "Found mismatch at pos = " << (it1 - s1.begin());
    it1++;
    it2++;
  }
}
template<typename T>
class IntArraysetTestBase : public TRTest::JitTest {
  public:
    T getValue2(T value) { return value ^ -1;}
};
template<typename T>
class FloatArraysetTestBase : public TRTest::JitTest {
  public:
    T getValue2(T value) { return (value != static_cast<T>(0)) ? static_cast<T>(0) : static_cast<T>(1);}
};

#define ParameterizedArraysetTest(baseclass, type, typeName, elemType, elemPrefix, formatString)               \
/**                                                                                                            \
 * @brief TestFixture class for arrayset test                                                                  \
 *                                                                                                             \
 * @details Used for arrayset test.                                                                            \
 * The first parameter is the value set to the array.                                                          \
 * The second parameter is the length parameter for the arraycmp evaluator.                                    \
 */                                                                                                            \
class  type##ArraysetTest : public baseclass<elemType>,                                                        \
                            public ::testing::WithParamInterface<std::tuple<elemType, int64_t>> {};            \
TEST_P(type##ArraysetTest, UsingConstLenConstVal) {                                                            \
    SKIP_ON_ARM(MissingImplementation);                                                                        \
    SKIP_ON_RISCV(MissingImplementation);                                                                      \
    auto value = std::get<0>(GetParam());                                                                      \
    auto length = std::get<1>(GetParam());                                                                     \
    auto value2 = getValue2(value);                                                                            \
    char inputTrees[1024] = {0};                                                                               \
    std::snprintf(inputTrees, sizeof(inputTrees),                                                              \
      "(method return=NoType args=[Address]"                                                                   \
      "  (block"                                                                                               \
      "    (arrayset address=0 args=[Address " typeName " Int64]"                                              \
      "      (aload parm=0)"                                                                                   \
      "      (" elemPrefix "const " formatString ")"                                                           \
      "      (lconst %" OMR_PRId64 "))"                                                                        \
      "    (return)))",                                                                                        \
      value, length                                                                                            \
      );                                                                                                       \
    auto trees = parseString(inputTrees);                                                                      \
                                                                                                               \
    ASSERT_NOTNULL(trees);                                                                                     \
                                                                                                               \
    Tril::DefaultCompiler compiler(trees);                                                                     \
                                                                                                               \
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;  \
    auto nelem = length / sizeof(elemType);                                                                    \
    std::vector<elemType> s1(nelem + 1, value2);                                                               \
    std::vector<elemType> s2(nelem + 1, value);                                                                \
    /* Verifies that arrayset does not write past the end */                                                   \
    s2[nelem] = value2;                                                                                        \
    auto entry_point = compiler.getEntryPoint<void (*)(elemType *)>();                                         \
    entry_point(&s1[0]);                                                                                       \
    assert_array_equals(s1, s2);                                                                               \
}                                                                                                              \
TEST_P(type##ArraysetTest, UsingConstLen) {                                                                    \
    SKIP_ON_ARM(MissingImplementation);                                                                        \
    SKIP_ON_RISCV(MissingImplementation);                                                                      \
    auto value = std::get<0>(GetParam());                                                                      \
    auto length = std::get<1>(GetParam());                                                                     \
    auto value2 = getValue2(value);                                                                            \
    char inputTrees[1024] = {0};                                                                               \
    std::snprintf(inputTrees, sizeof(inputTrees),                                                              \
      "(method return=NoType args=[Address, " typeName " ]"                                                    \
      "  (block"                                                                                               \
      "    (arrayset address=0 args=[Address " typeName " Int64]"                                              \
      "      (aload parm=0)"                                                                                   \
      "      (" elemPrefix "load parm=1)"                                                                      \
      "      (lconst %" OMR_PRId64 "))"                                                                        \
      "    (return)))",                                                                                        \
      length                                                                                                   \
      );                                                                                                       \
    auto trees = parseString(inputTrees);                                                                      \
                                                                                                               \
    ASSERT_NOTNULL(trees);                                                                                     \
                                                                                                               \
    Tril::DefaultCompiler compiler(trees);                                                                     \
                                                                                                               \
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;  \
    auto nelem = length / sizeof(elemType);                                                                    \
    std::vector<elemType> s1(nelem + 1, value2);                                                               \
    std::vector<elemType> s2(nelem + 1, value);                                                                \
    /* Verifies that arrayset does not write past the end */                                                   \
    s2[nelem] = value2;                                                                                        \
    auto entry_point = compiler.getEntryPoint<void (*)(elemType *, elemType)>();                               \
    entry_point(&s1[0], value);                                                                                \
    assert_array_equals(s1, s2);                                                                               \
}                                                                                                              \
TEST_P(type##ArraysetTest, UsingVariableLenConstVal) {                                                         \
    SKIP_ON_ARM(MissingImplementation);                                                                        \
    SKIP_ON_RISCV(MissingImplementation);                                                                      \
    auto value = std::get<0>(GetParam());                                                                      \
    auto length = std::get<1>(GetParam());                                                                     \
    auto value2 = getValue2(value);                                                                            \
    char inputTrees[1024] = {0};                                                                               \
    std::snprintf(inputTrees, sizeof(inputTrees),                                                              \
      "(method return=NoType args=[Address, Int64]"                                                            \
      "  (block"                                                                                               \
      "    (arrayset address=0 args=[Address " typeName " Int64]"                                              \
      "      (aload parm=0)"                                                                                   \
      "      (" elemPrefix "const " formatString ")"                                                           \
      "      (lload parm=1))"                                                                                  \
      "    (return)))",                                                                                        \
      value                                                                                                    \
      );                                                                                                       \
    auto trees = parseString(inputTrees);                                                                      \
                                                                                                               \
    ASSERT_NOTNULL(trees);                                                                                     \
                                                                                                               \
    Tril::DefaultCompiler compiler(trees);                                                                     \
                                                                                                               \
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;  \
    auto nelem = length / sizeof(elemType);                                                                    \
    std::vector<elemType> s1(nelem + 1, value2);                                                               \
    std::vector<elemType> s2(nelem + 1, value);                                                                \
    /* Verifies that arrayset does not write past the end */                                                   \
    s2[nelem] = value2;                                                                                        \
    auto entry_point = compiler.getEntryPoint<void (*)(elemType *, int64_t)>();                                \
    entry_point(&s1[0], length);                                                                               \
    assert_array_equals(s1, s2);                                                                               \
}                                                                                                              \
TEST_P(type##ArraysetTest, UsingVariableLen) {                                                                 \
    SKIP_ON_ARM(MissingImplementation);                                                                        \
    SKIP_ON_RISCV(MissingImplementation);                                                                      \
    auto value = std::get<0>(GetParam());                                                                      \
    auto length = std::get<1>(GetParam());                                                                     \
    auto value2 = getValue2(value);                                                                            \
    char inputTrees[1024] = {0};                                                                               \
    std::snprintf(inputTrees, sizeof(inputTrees),                                                              \
      "(method return=NoType args=[Address, " typeName ", Int64]"                                              \
      "  (block"                                                                                               \
      "    (arrayset address=0 args=[Address " typeName " Int64]"                                              \
      "      (aload parm=0)"                                                                                   \
      "      (" elemPrefix "load parm=1)"                                                                      \
      "      (lload parm=2))"                                                                                  \
      "    (return)))"                                                                                         \
      );                                                                                                       \
    auto trees = parseString(inputTrees);                                                                      \
                                                                                                               \
    ASSERT_NOTNULL(trees);                                                                                     \
                                                                                                               \
    Tril::DefaultCompiler compiler(trees);                                                                     \
                                                                                                               \
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;  \
    auto nelem = length / sizeof(elemType);                                                                    \
    std::vector<elemType> s1(nelem + 1, value2);                                                               \
    std::vector<elemType> s2(nelem + 1, value);                                                                \
    /* Verifies that arrayset does not write past the end */                                                   \
    s2[nelem] = value2;                                                                                        \
    auto entry_point = compiler.getEntryPoint<void (*)(elemType *, elemType, int64_t)>();                      \
    entry_point(&s1[0], value, length);                                                                        \
    assert_array_equals(s1, s2);                                                                               \
}                                                                                                              \

ParameterizedArraysetTest(IntArraysetTestBase, Int8, "Int8", int8_t, "b", "%d")
ParameterizedArraysetTest(IntArraysetTestBase, Int16, "Int16", int16_t, "s", "%d")
ParameterizedArraysetTest(IntArraysetTestBase, Int32, "Int32", int32_t, "i", "%d")
ParameterizedArraysetTest(IntArraysetTestBase, Int64, "Int64", int64_t, "l", "%" OMR_PRId64)
ParameterizedArraysetTest(FloatArraysetTestBase, Float, "Float", float, "f", "%f")
ParameterizedArraysetTest(FloatArraysetTestBase, Double, "Double", double, "d", "%lf")

template <typename T>
bool arraysetFp_filter(T a)
   {
   // workaround: avoid failure caused by snprintf("%f")
   return std::isnan(a) || (std::abs(a) < 0.01 && a != 0.0);
   }

template<typename T>
std::vector<std::tuple<T, int64_t>> generateArraysetParm(const std::vector<T> &constVals) {
  int index = 0;
  int nVals = constVals.size();
  int step = sizeof(T);
  std::vector<std::tuple<T, int64_t>> v;
  for (int length = step; length <= 1024; length += step) {
      for (int i = 0; i < 2; i++) {
        v.push_back(std::make_tuple(constVals[(index++) % nVals], length));
      }
  }
  /* Large sized arrays */
  for (int length = 16384; length <= 16416; length += step) {
      v.push_back(std::make_tuple(constVals[(index++) % nVals], length));
  }
  return v;
}
template<typename T>
std::vector<std::tuple<T, int64_t>> generateArraysetParm() {
  return generateArraysetParm(TRTest::const_values<T>());
}
template<typename T>
std::vector<std::tuple<T, int64_t>> generateArraysetFPParm() {
  return generateArraysetParm(TRTest::filter(TRTest::const_values<T>(), arraysetFp_filter<T>));
}
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int8ArraysetTest, testing::ValuesIn(generateArraysetParm<int8_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int16ArraysetTest, testing::ValuesIn(generateArraysetParm<int16_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int32ArraysetTest, testing::ValuesIn(generateArraysetParm<int32_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int64ArraysetTest, testing::ValuesIn(generateArraysetParm<int64_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, FloatArraysetTest, testing::ValuesIn(generateArraysetFPParm<float>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, DoubleArraysetTest, testing::ValuesIn(generateArraysetFPParm<double>()));

#define ParameterizedArraysetUnalignedTest(baseclass, type, typeName, elemType, elemPrefix)                             \
/**                                                                                                                     \
 * @brief TestFixture class for unaligned arrayset test                                                                 \
 *                                                                                                                      \
 * @details Used for unaligned arrayset test.                                                                           \
 * The first parameter is the value set to the array.                                                                   \
 * The second parameter is the length parameter for the arraycmp evaluator.                                             \
 * The third parameter is the residue of src address modulo 16.                                                         \
 */                                                                                                                     \
class  type##ArraysetUnalignedTest : public baseclass<elemType>,                                                        \
                                     public ::testing::WithParamInterface<std::tuple<elemType, int64_t, int32_t>> {};   \
TEST_P(type##ArraysetUnalignedTest, UsingConstLen) {                                                                    \
    SKIP_ON_ARM(MissingImplementation);                                                                                 \
    SKIP_ON_RISCV(MissingImplementation);                                                                               \
                                                                                                                        \
    auto value = std::get<0>(GetParam());                                                                               \
    auto length = std::get<1>(GetParam());                                                                              \
    auto offset = std::get<2>(GetParam()) / sizeof(elemType);                                                           \
    auto value2 = getValue2(value);                                                                                     \
    char inputTrees[1024] = {0};                                                                                        \
    std::snprintf(inputTrees, sizeof(inputTrees),                                                                       \
      "(method return=NoType args=[Address, " typeName "]"                                                              \
      "  (block"                                                                                                        \
      "    (arrayset address=0 args=[Address, " typeName ", Int64]"                                                     \
      "      (aload parm=0)"                                                                                            \
      "      (" elemPrefix "load parm=1)"                                                                               \
      "      (lconst %" OMR_PRId64 "))"                                                                                 \
      "    (return)))",                                                                                                 \
      length                                                                                                            \
      );                                                                                                                \
    auto trees = parseString(inputTrees);                                                                               \
                                                                                                                        \
    ASSERT_NOTNULL(trees);                                                                                              \
                                                                                                                        \
    Tril::DefaultCompiler compiler(trees);                                                                              \
                                                                                                                        \
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;           \
    auto nelem = length / sizeof(elemType);                                                                             \
    std::vector<elemType> s1(nelem + 1 + offset, value2);                                                               \
    std::vector<elemType> s2(nelem + 1 + offset, value2);                                                               \
    /* Verifies that arrayset does not write before the beginning or past the end */                                    \
    std::fill(s2.begin() + offset, s2.end() - 1, value);                                                                \
                                                                                                                        \
    auto entry_point = compiler.getEntryPoint<void (*)(elemType *, elemType)>();                                        \
    entry_point(&s1[offset], value);                                                                                    \
    assert_array_equals(s1, s2);                                                                                        \
}                                                                                                                       \
TEST_P(type##ArraysetUnalignedTest, UsingVariableLen) {                                                                 \
    SKIP_ON_ARM(MissingImplementation);                                                                                 \
    SKIP_ON_RISCV(MissingImplementation);                                                                               \
                                                                                                                        \
    auto value = std::get<0>(GetParam());                                                                               \
    auto length = std::get<1>(GetParam());                                                                              \
    auto offset = std::get<2>(GetParam()) / sizeof(elemType);                                                           \
    auto value2 = getValue2(value);                                                                                     \
    char inputTrees[1024] = {0};                                                                                        \
    std::snprintf(inputTrees, sizeof(inputTrees),                                                                       \
      "(method return=NoType args=[Address, " typeName ", Int64]"                                                       \
      "  (block"                                                                                                        \
      "    (arrayset address=0 args=[Address, " typeName ", Int64]"                                                     \
      "      (aload parm=0)"                                                                                            \
      "      (" elemPrefix "load parm=1)"                                                                               \
      "      (lload parm=2))"                                                                                           \
      "    (return)))"                                                                                                  \
      );                                                                                                                \
    auto trees = parseString(inputTrees);                                                                               \
                                                                                                                        \
    ASSERT_NOTNULL(trees);                                                                                              \
                                                                                                                        \
    Tril::DefaultCompiler compiler(trees);                                                                              \
                                                                                                                        \
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;           \
    auto nelem = length / sizeof(elemType);                                                                             \
    std::vector<elemType> s1(nelem + 1 + offset, value2);                                                               \
    std::vector<elemType> s2(nelem + 1 + offset, value2);                                                               \
    /* Verifies that arrayset does not write before the beginning or past the end */                                    \
    std::fill(s2.begin() + offset, s2.end() - 1, value);                                                                \
                                                                                                                        \
    auto entry_point = compiler.getEntryPoint<void (*)(elemType *, elemType, int64_t)>();                               \
    entry_point(&s1[offset], value, length);                                                                            \
    assert_array_equals(s1, s2);                                                                                        \
}                                                                                                                       \

ParameterizedArraysetUnalignedTest(IntArraysetTestBase, Int8, "Int8", int8_t, "b")
ParameterizedArraysetUnalignedTest(IntArraysetTestBase, Int16, "Int16", int16_t, "s")
ParameterizedArraysetUnalignedTest(IntArraysetTestBase, Int32, "Int32", int32_t, "i")
ParameterizedArraysetUnalignedTest(IntArraysetTestBase, Int64, "Int64", int64_t, "l")
ParameterizedArraysetUnalignedTest(FloatArraysetTestBase, Float, "Float", float, "f")
ParameterizedArraysetUnalignedTest(FloatArraysetTestBase, Double, "Double", double, "d")

template<typename T>
std::vector<std::tuple<T, int64_t, int32_t>> generateArraysetUnalignedParm(const std::vector<T> &constVals) {
  int index = 0;
  int nVals = constVals.size();
  int step = sizeof(T);
  int m = 4096 / ((1024 / step) * (15 / step));
  int n = std::min(std::max(m, 1), 2);
  std::vector<std::tuple<T, int64_t, int32_t>> v;
  for (int length = step; length <= 1024; length += step) {
    for (int offset = step; offset < 16; offset += step) {
      for (int i = 0; i < n; i++) {
        v.push_back(std::make_tuple(constVals[(index++) % nVals], length, offset));
      }
    }
  }
  return v;
}
template<typename T>
std::vector<std::tuple<T, int64_t, int32_t>> generateArraysetUnalignedParm() {
  return generateArraysetUnalignedParm(TRTest::const_values<T>());
}
template<typename T>
std::vector<std::tuple<T, int64_t, int32_t>> generateArraysetUnalignedFPParm() {
  return generateArraysetUnalignedParm(TRTest::filter(TRTest::const_values<T>(), arraysetFp_filter<T>));
}
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int8ArraysetUnalignedTest, testing::ValuesIn(generateArraysetUnalignedParm<int8_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int16ArraysetUnalignedTest, testing::ValuesIn(generateArraysetUnalignedParm<int16_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int32ArraysetUnalignedTest, testing::ValuesIn(generateArraysetUnalignedParm<int32_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, Int64ArraysetUnalignedTest, testing::ValuesIn(generateArraysetUnalignedParm<int64_t>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, FloatArraysetUnalignedTest, testing::ValuesIn(generateArraysetUnalignedFPParm<float>()));
INSTANTIATE_TEST_CASE_P(ArraysetTest, DoubleArraysetUnalignedTest, testing::ValuesIn(generateArraysetUnalignedFPParm<double>()));
