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


#include "OpCodeTest.hpp"
#include "default_compiler.hpp"
#include "omrformatconsts.h"
#include <limits>

int16_t sbyteswap(int16_t l) {
    return ((l << 8) & 0xff00)
         | ((l >> 8) & 0x00ff);
}

int32_t ineg(int32_t l) {
    return -l;
}

int32_t iabs(int32_t l) {
   if (l >= 0)
      return l;
   else
      return -1 * l;
}

int32_t ibyteswap(int32_t l) {
    return ((l << 24) & 0xff000000)
         | ((l <<  8) & 0x00ff0000)
         | ((l >>  8) & 0x0000ff00)
         | ((l >> 24) & 0x000000ff);
}

int64_t lbyteswap(int64_t l) {
    return ((l << 56) & 0xff00000000000000)
         | ((l << 40) & 0x00ff000000000000)
         | ((l << 24) & 0x0000ff0000000000)
         | ((l <<  8) & 0x000000ff00000000)
         | ((l >>  8) & 0x00000000ff000000)
         | ((l >> 24) & 0x0000000000ff0000)
         | ((l >> 40) & 0x000000000000ff00)
         | ((l >> 56) & 0x00000000000000ff);
}

int64_t lneg(int64_t l) {
    return l == INT64_MIN ? INT64_MIN : -l;
}

template <typename int_t>
int_t or_(int_t l, int_t r) {
    return l | r;
}

template <typename int_t>
int_t and_(int_t l, int_t r) {
    return l & r;
}

template <typename int_t>
int_t xor_(int_t l, int_t r) {
    return l ^ r;
}

template <typename int_t>
static inline int_t compressbits(int_t src, int_t mask) {
    int_t res = 0;
    int k = 0;
    const int_t sign_mask = ~std::numeric_limits<int_t>::min();
    while (mask) {
        if (mask & 1) {
            res |= (src & 1) << k;
            k++;
        }
        mask = (mask >> 1) & sign_mask;
        src = (src >> 1) & sign_mask;
    }
    return res;
}

template <typename int_t>
static inline int_t expandbits(int_t src, int_t mask) {
    int_t res = 0;
    const int_t sign_mask = ~std::numeric_limits<int_t>::min();
    for (int n = 0; mask; n++) {
        if (mask & 1) {
            res |= (src & 1) << n;
            src = (src >> 1) & sign_mask;
        }
        mask = (mask >> 1) & sign_mask;
    }
    return res;
}

class Int16LogicalUnary : public TRTest::UnaryOpTest<int16_t> {};

TEST_P(Int16LogicalUnary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    if (param.opcode == "sbyteswap") {
        SKIP_ON_ARM(MissingImplementation);
        SKIP_ON_RISCV(MissingImplementation);
    }

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
        "(method return=Int16"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sconst %" OMR_PRId16 ") )"
        "))))",
        param.opcode.c_str(),
        param.value);

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)()>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int16LogicalUnary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    if (param.opcode == "sbyteswap") {
        SKIP_ON_ARM(MissingImplementation);
        SKIP_ON_RISCV(MissingImplementation);
    }

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120,
        "(method return=Int16 args=[Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "         (%s"
        "           (sload parm=0) )"
        "))))",
        param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t)>();
    ASSERT_EQ(param.oracle(param.value), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int16LogicalUnary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_values<int16_t>()),
    ::testing::Values(
        std::tuple<const char*, int16_t(*)(int16_t)>("sbyteswap", sbyteswap)
        )));

class Int32LogicalUnary : public TRTest::UnaryOpTest<int32_t> {};

TEST_P(Int32LogicalUnary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    if (param.opcode == "ibyteswap") {
        SKIP_ON_ARM(MissingImplementation);
        SKIP_ON_RISCV(MissingImplementation);
    }

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) ) )))", param.opcode.c_str(), param.value);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)()>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32LogicalUnary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    if (param.opcode == "ibyteswap") {
        SKIP_ON_ARM(MissingImplementation);
        SKIP_ON_RISCV(MissingImplementation);
    }

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 args=[Int32] (block (ireturn (%s (iload parm=0) ) )))", param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_EQ(param.oracle(param.value), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int32LogicalUnary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_values<int32_t>()),
    ::testing::Values(
        std::tuple<const char*, int32_t(*)(int32_t)>("ibyteswap", ibyteswap),
        std::tuple<const char*, int32_t(*)(int32_t)>("ineg", ineg),
        std::tuple<const char*, int32_t(*)(int32_t)>("iabs", iabs)
    )));

class Int8LogicalBinary : public TRTest::BinaryOpTest<int8_t> {};

TEST_P(Int8LogicalBinary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "bcompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "bexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
        "(method return=Int8"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (bconst %" OMR_PRId8 ")"
        "          (bconst %" OMR_PRId8 ") ) ) ) ) )",
        param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int8LogicalBinary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "bcompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "bexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
        "(method return=Int8 args=[Int8, Int8]"
        "  (block"
        "    (ireturn"
        "      (b2i"
        "        (%s"
        "          (bload parm=0)"
        "          (bload parm=1) ) ) ) ) )",
        param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int8_t (*)(int8_t, int8_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int8LogicalBinary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int8_t,int8_t>()),
    ::testing::Values(
        std::tuple<const char*, int8_t(*)(int8_t, int8_t)>("bor", or_),
        std::tuple<const char*, int8_t(*)(int8_t, int8_t)>("band", and_),
        std::tuple<const char*, int8_t(*)(int8_t, int8_t)>("bxor", xor_),
        std::tuple<const char*, int8_t(*)(int8_t, int8_t)>("bcompressbits", compressbits),
        std::tuple<const char*, int8_t(*)(int8_t, int8_t)>("bexpandbits", expandbits)
   )));


class Int16LogicalBinary : public TRTest::BinaryOpTest<int16_t> {};

TEST_P(Int16LogicalBinary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "scompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "sexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
        "(method return=Int16"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sconst %" OMR_PRId16 ")"
        "          (sconst %" OMR_PRId16 ") ) ) ) ) )",
        param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int16LogicalBinary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "scompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "sexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

    char inputTrees[160] = {0};
    std::snprintf(inputTrees, 160,
        "(method return=Int16 args=[Int16, Int16]"
        "  (block"
        "    (ireturn"
        "      (s2i"
        "        (%s"
        "          (sload parm=0)"
        "          (sload parm=1) ) ) ) ) )",
        param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int16_t (*)(int16_t, int16_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int16LogicalBinary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int16_t,int16_t>()),
    ::testing::Values(
        std::tuple<const char*, int16_t(*)(int16_t, int16_t)>("sor", or_),
        std::tuple<const char*, int16_t(*)(int16_t, int16_t)>("sand", and_),
        std::tuple<const char*, int16_t(*)(int16_t, int16_t)>("sxor", xor_),
        std::tuple<const char*, int16_t(*)(int16_t, int16_t)>("scompressbits", compressbits),
        std::tuple<const char*, int16_t(*)(int16_t, int16_t)>("sexpandbits", expandbits)
   )));

class Int32LogicalBinary : public TRTest::BinaryOpTest<int32_t> {};

TEST_P(Int32LogicalBinary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "icompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "iexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 (block (ireturn (%s (iconst %d) (iconst %d)) )))", param.opcode.c_str(), param.lhs, param.rhs);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int32LogicalBinary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "icompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "iexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

    char inputTrees[120] = {0};
    std::snprintf(inputTrees, 120, "(method return=Int32 args=[Int32, Int32] (block (ireturn (%s (iload parm=0) (iload parm=1)) )))", param.opcode.c_str());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int32LogicalBinary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int32_t,int32_t>()),
    ::testing::Values(
        std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("ior", or_),
        std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("iand", and_),
        std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("ixor", xor_),
        std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("icompressbits", compressbits),
        std::tuple<const char*, int32_t(*)(int32_t, int32_t)>("iexpandbits", expandbits)
    )));

class Int64LogicalBinary : public TRTest::BinaryOpTest<int64_t> {};

TEST_P(Int64LogicalBinary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "lcompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "lexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(void)>();
    volatile auto exp = param.oracle(param.lhs, param.rhs);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64LogicalBinary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    SKIP_IF(param.opcode == "lcompressbits" && !cpu.hasBitCompressInstruction(), MissingImplementation);
    SKIP_IF(param.opcode == "lexpandbits" && !cpu.hasBitExpandInstruction(), MissingImplementation);

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t, int64_t)>();
    ASSERT_EQ(param.oracle(param.lhs, param.rhs), entry_point(param.lhs, param.rhs));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int64LogicalBinary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_value_pairs<int64_t,int64_t>()),
    ::testing::Values(
        std::tuple<const char*, int64_t(*)(int64_t, int64_t)>("lor", or_),
        std::tuple<const char*, int64_t(*)(int64_t, int64_t)>("land", and_),
        std::tuple<const char*, int64_t(*)(int64_t, int64_t)>("lxor", xor_),
        std::tuple<const char*, int64_t(*)(int64_t, int64_t)>("lcompressbits", compressbits),
        std::tuple<const char*, int64_t(*)(int64_t, int64_t)>("lexpandbits", expandbits)
   )));


class Int64LogicalUnary : public TRTest::UnaryOpTest<int64_t> {};

TEST_P(Int64LogicalUnary, UsingConst) {
    auto param = TRTest::to_struct(GetParam());

    if (param.opcode == "lbyteswap") {
        SKIP_ON_ARM(MissingImplementation);
        SKIP_ON_RISCV(MissingImplementation);
    }

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)()>();
    volatile auto exp = param.oracle(param.value);
    volatile auto act = entry_point();
    ASSERT_EQ(exp, act);
}

TEST_P(Int64LogicalUnary, UsingLoadParam) {
    auto param = TRTest::to_struct(GetParam());

    if (param.opcode == "lbyteswap") {
        SKIP_ON_ARM(MissingImplementation);
        SKIP_ON_RISCV(MissingImplementation);
    }

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

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_EQ(param.oracle(param.value), entry_point(param.value));
}

INSTANTIATE_TEST_CASE_P(LogicalTest, Int64LogicalUnary, ::testing::Combine(
    ::testing::ValuesIn(TRTest::const_values<int64_t>()),
    ::testing::Values(
        std::tuple<const char*, int64_t(*)(int64_t)>("lbyteswap", lbyteswap),
        std::tuple<const char*, int64_t(*)(int64_t)>("lneg", lneg)
        )));
