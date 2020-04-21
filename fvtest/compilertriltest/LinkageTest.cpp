/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

#include "JitTest.hpp"
#include "default_compiler.hpp"

/* Template for mapping a C/C++ type to
 * a TR type, and type prefix.
 */
template<typename T>
struct TypeToString {
   static const char * type;
   static const char * prefix;
};

template<> const char * TypeToString<int32_t>::type = "Int32";
template<> const char * TypeToString<int64_t>::type = "Int64";
template<> const char * TypeToString<float>  ::type = "Float";
template<> const char * TypeToString<double> ::type = "Double";

template<> const char * TypeToString<int32_t>::prefix = "i";
template<> const char * TypeToString<int64_t>::prefix = "l";
template<> const char * TypeToString<float>  ::prefix = "f";
template<> const char * TypeToString<double> ::prefix = "d";

TEST(TypeToString, TestTemplate) {
   EXPECT_STREQ("Int32", TypeToString<int32_t>::type);
   EXPECT_STREQ("Double", TypeToString<double>::type);
}

template <typename T>
T return1stArgument(T a) { return a; }

template <typename T>
T return2ndArgument(T a, T b) { return b; }

template <typename T>
T return3rdArgument(T a, T b, T c) { return c; }

template <typename T>
T return4thArgument(T a, T b, T c, T d) { return d; }

template <typename T>
T return5thArgument(T a, T b, T c, T d, T e) { return e; }

template <typename T>
T return6thArgument(T a, T b, T c, T d, T e, T f) { return f; }

template <typename T>
T return7thArgument(T a, T b, T c, T d, T e, T f, T g) { return g; }

template <typename T>
T return8thArgument(T a, T b, T c, T d, T e, T f, T g, T h) { return h; }

template <typename T>
T return9thArgument(T a, T b, T c, T d, T e, T f, T g, T h, T i) { return i; }

template <typename T>
class LinkageTest : public TRTest::JitTest {};

typedef ::testing::Types<int32_t, int64_t, float, double> InputTypes;
TYPED_TEST_CASE(LinkageTest, InputTypes);

TYPED_TEST(LinkageTest, InvalidLinkageTest) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=Int32 args=[Int32]"
       "  (block"
       "    (ireturn"
       "      (icall address=0x%jX args=[Int32] linkage=noexist"
       "        (iload parm=0) ) ) ) )",

       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam)>(return1stArgument<TypeParam>)) // address
    );

    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_NE(0, compiler.compile()) << "Compilation succeeded unexpectedly\n" << "Input trees: " << inputTrees;
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing1Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s] linkage=system"
       "        (%sload parm=0) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam)>(return1stArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(*i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing2Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam)>(return2ndArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing3Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam)>(return3rdArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing4Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam, TypeParam)>(return4thArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing5Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>(return5thArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing6Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>(return6thArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing7Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5)"
       "        (%sload parm=6) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>(return7thArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing8Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5)"
       "        (%sload parm=6)"
       "        (%sload parm=7) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>(return8thArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToNativeParameterPassing9Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5)"
       "        (%sload parm=6)"
       "        (%sload parm=7)"
       "        (%sload parm=8) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>(return9thArgument<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, 7, *i))  << "Input Trees: " << inputTrees;
    }
}

template <typename T>
T return2ndArgumentFromMixedTypes(double a, T b) { return b; }

template <typename T>
T return3rdArgumentFromMixedTypes(double a, int32_t b, T c) { return c; }

template <typename T>
T return4thArgumentFromMixedTypes(double a, int64_t b, float c, T d) { return d; }

template <typename T>
T return5thArgumentFromMixedTypes(float a, int32_t b, float c, int64_t d, T e) { return e; }

template <typename T>
T return6thArgumentFromMixedTypes(float a, int32_t b, double c, int32_t d, int64_t e, T f) { return f; }

template <typename T>
T return7thArgumentFromMixedTypes(double a, int32_t b, double c, int32_t d, int32_t e, int64_t f, T g) { return g; }

template <typename T>
T return8thArgumentFromMixedTypes(int64_t a, int64_t b, float c, double d, int32_t e, int64_t f, int32_t g, T h) { return h; }

template <typename T>
T return9thArgumentFromMixedTypes(double a, int32_t b, float c, int32_t d, int32_t e, float f, int64_t g, int32_t h, T i) { return i; }

template <typename T>
class LinkageWithMixedTypesTest : public TRTest::JitTest {};

typedef ::testing::Types<int32_t, int64_t, float, double> InputTypes;
TYPED_TEST_CASE(LinkageWithMixedTypesTest, InputTypes);

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing2ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,%s] linkage=system"
       "        (dload parm=0)"
       "        (%sload parm=1) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(double, TypeParam)>(return2ndArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing3ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int32,%s] linkage=system"
       "        (dload parm=0)"
       "        (iload parm=1)"
       "        (%sload parm=2) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(double, int32_t, TypeParam)>(return3rdArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int32_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing4ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int64,Float,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int64,Float,%s] linkage=system"
       "        (dload parm=0)"
       "        (lload parm=1)"
       "        (fload parm=2)"
       "        (%sload parm=3) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(double, int64_t, float, TypeParam)>(return4thArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int64_t, float, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing5ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Float,Int32,Float,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Float,Int32,Float,Int64,%s] linkage=system"
       "        (fload parm=0)"
       "        (iload parm=1)"
       "        (fload parm=2)"
       "        (lload parm=3)"
       "        (%sload parm=4) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(float, int32_t, float, int64_t, TypeParam)>(return5thArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(float, int32_t, float, int64_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing6ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Float,Int32,Double,Int32,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Float,Int32,Double,Int32,Int64,%s] linkage=system"
       "        (fload parm=0)"
       "        (iload parm=1)"
       "        (dload parm=2)"
       "        (iload parm=3)"
       "        (lload parm=4)"
       "        (%sload parm=5) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(float, int32_t, double, int32_t, int64_t, TypeParam)>(return6thArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(float, int32_t, double, int32_t, int64_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing7ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,Double,Int32,Int32,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int32,Double,Int32,Int32,Int64,%s] linkage=system"
       "        (dload parm=0)"
       "        (iload parm=1)"
       "        (dload parm=2)"
       "        (iload parm=3)"
       "        (iload parm=4)"
       "        (lload parm=5)"
       "        (%sload parm=6) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(double, int32_t, double, int32_t, int32_t, int64_t, TypeParam)>(return7thArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int32_t, double, int32_t, int32_t, int64_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing8ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Int64,Int64,Float,Double,Int32,Int64,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Int64,Int64,Float,Double,Int32,Int64,Int32,%s] linkage=system"
       "        (lload parm=0)"
       "        (lload parm=1)"
       "        (fload parm=2)"
       "        (dload parm=3)"
       "        (iload parm=4)"
       "        (lload parm=5)"
       "        (iload parm=6)"
       "        (%sload parm=7) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(int64_t, int64_t, float, double, int32_t, int64_t, int32_t, TypeParam)>(return8thArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(int64_t, int64_t, float, double, int32_t, int64_t, int32_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToNativeParameterPassing9ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,Float,Int32,Int32,Float,Int64,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int32,Float,Int32,Int32,Float,Int64,Int32,%s] linkage=system"
       "        (dload parm=0)"
       "        (iload parm=1)"
       "        (fload parm=2)"
       "        (iload parm=3)"
       "        (iload parm=4)"
       "        (fload parm=5)"
       "        (lload parm=6)"
       "        (iload parm=7)"
       "        (%sload parm=8) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(static_cast<TypeParam(*)(double, int32_t, float, int32_t, int32_t, float, int64_t, int32_t, TypeParam)>(return9thArgumentFromMixedTypes<TypeParam>)), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int32_t, float, int32_t, int32_t, float, int64_t, int32_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, 7, *i))  << "Input Trees: " << inputTrees;
    }
}

template <typename T>
T (*getJitCompiledMethodReturning1stArgument())(T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=0) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning2ndArgument())(T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=1) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning3rdArgument())(T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=2) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning4thArgument())(T, T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=3) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning5thArgument())(T, T, T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=4) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T, T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning6thArgument())(T, T, T, T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=5) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T, T, T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning7thArgument())(T, T, T, T, T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=6) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T, T, T, T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning8thArgument())(T, T, T, T, T, T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=7) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T, T, T, T, T, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning9thArgument())(T, T, T, T, T, T, T, T, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=8) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(T, T, T, T, T, T, T, T, T)>();
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing1Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning1stArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s] linkage=system"
       "        (%sload parm=0) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(*i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing2Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning2ndArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing3Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning3rdArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing4Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning4thArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing5Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning5thArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing6Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning6thArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing7Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning7thArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5)"
       "        (%sload parm=6) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing8Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning8thArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5)"
       "        (%sload parm=6)"
       "        (%sload parm=7) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageTest, SystemLinkageJitToJitParameterPassing9Arg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning9thArgument<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[%s,%s,%s,%s,%s,%s,%s,%s,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[%s,%s,%s,%s,%s,%s,%s,%s,%s] linkage=system"
       "        (%sload parm=0)"
       "        (%sload parm=1)"
       "        (%sload parm=2)"
       "        (%sload parm=3)"
       "        (%sload parm=4)"
       "        (%sload parm=5)"
       "        (%sload parm=6)"
       "        (%sload parm=7)"
       "        (%sload parm=8) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix, // load
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, 7, *i))  << "Input Trees: " << inputTrees;
    }
}

template <typename T>
T (*getJitCompiledMethodReturning2ndArgumentFromMixedTypes())(double, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=1) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(double, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning3rdArgumentFromMixedTypes())(double, int32_t, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=2) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(double, int32_t, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning4thArgumentFromMixedTypes())(double, int64_t, float, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int64,Float,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=3) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(double, int64_t, float, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning5thArgumentFromMixedTypes())(float, int32_t, float, int64_t, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Float,Int32,Float,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=4) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(float, int32_t, float, int64_t, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning6thArgumentFromMixedTypes())(float, int32_t, double, int32_t, int64_t, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Float,Int32,Double,Int32,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=5) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(float, int32_t, double, int32_t, int64_t, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning7thArgumentFromMixedTypes())(double, int32_t, double, int32_t, int32_t, int64_t, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,Double,Int32,Int32,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=6) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(double, int32_t, double, int32_t, int32_t, int64_t, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning8thArgumentFromMixedTypes())(int64_t, int64_t, float, double, int32_t, int64_t, int32_t, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Int64,Int64,Float,Double,Int32,Int64,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=7) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(int64_t, int64_t, float, double, int32_t, int64_t, int32_t, T)>();
}

template <typename T>
T (*getJitCompiledMethodReturning9thArgumentFromMixedTypes())(double, int32_t, float, int32_t, int32_t, float, int64_t, int32_t, T) {
    char inputTrees[400] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,Float,Int32,Int32,Float,Int64,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%sload parm=8) ) ) )",

       TypeToString<T>::type, // return
       TypeToString<T>::type, // args
       TypeToString<T>::prefix, // return
       TypeToString<T>::prefix  // load
    );

    auto trees = parseString(inputTrees);
    if (trees == NULL) {
        return NULL;
    }

    Tril::DefaultCompiler compiler(trees);
    if (compiler.compile() != 0) {
        return NULL;
    }

    return compiler.getEntryPoint<T (*)(double, int32_t, float, int32_t, int32_t, float, int64_t, int32_t, T)>();
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing2ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning2ndArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,%s] linkage=system"
       "        (dload parm=0)"
       "        (%sload parm=1) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing3ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning3rdArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int32,%s] linkage=system"
       "        (dload parm=0)"
       "        (iload parm=1)"
       "        (%sload parm=2) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int32_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing4ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning4thArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int64,Float,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int64,Float,%s] linkage=system"
       "        (dload parm=0)"
       "        (lload parm=1)"
       "        (fload parm=2)"
       "        (%sload parm=3) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int64_t, float, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing5ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning5thArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Float,Int32,Float,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Float,Int32,Float,Int64,%s] linkage=system"
       "        (fload parm=0)"
       "        (iload parm=1)"
       "        (fload parm=2)"
       "        (lload parm=3)"
       "        (%sload parm=4) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(float, int32_t, float, int64_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing6ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning6thArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Float,Int32,Double,Int32,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Float,Int32,Double,Int32,Int64,%s] linkage=system"
       "        (fload parm=0)"
       "        (iload parm=1)"
       "        (dload parm=2)"
       "        (iload parm=3)"
       "        (lload parm=4)"
       "        (%sload parm=5) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(float, int32_t, double, int32_t, int64_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing7ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning7thArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,Double,Int32,Int32,Int64,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int32,Double,Int32,Int32,Int64,%s] linkage=system"
       "        (dload parm=0)"
       "        (iload parm=1)"
       "        (dload parm=2)"
       "        (iload parm=3)"
       "        (iload parm=4)"
       "        (lload parm=5)"
       "        (%sload parm=6) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int32_t, double, int32_t, int32_t, int64_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing8ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
 SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
 SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning8thArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Int64,Int64,Float,Double,Int32,Int64,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Int64,Int64,Float,Double,Int32,Int64,Int32,%s] linkage=system"
       "        (lload parm=0)"
       "        (lload parm=1)"
       "        (fload parm=2)"
       "        (dload parm=3)"
       "        (iload parm=4)"
       "        (lload parm=5)"
       "        (iload parm=6)"
       "        (%sload parm=7) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(int64_t, int64_t, float, double, int32_t, int64_t, int32_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, *i))  << "Input Trees: " << inputTrees;
    }
}

TYPED_TEST(LinkageWithMixedTypesTest, SystemLinkageJitToJitParameterPassing9ArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);

    SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
 SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
 SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_RISCV(MissingImplementation) << "Test is skipped on RISC-V because calls are not fully supported (see issue #1645)";

    auto callee_entry_point = getJitCompiledMethodReturning9thArgumentFromMixedTypes<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    char inputTrees[600] = { 0 };
    std::snprintf(inputTrees, sizeof(inputTrees),
       "(method return=%s args=[Double,Int32,Float,Int32,Int32,Float,Int64,Int32,%s]"
       "  (block"
       "    (%sreturn"
       "      (%scall address=0x%jX args=[Double,Int32,Float,Int32,Int32,Float,Int64,Int32,%s] linkage=system"
       "        (dload parm=0)"
       "        (iload parm=1)"
       "        (fload parm=2)"
       "        (iload parm=3)"
       "        (iload parm=4)"
       "        (fload parm=5)"
       "        (lload parm=6)"
       "        (iload parm=7)"
       "        (%sload parm=8) ) ) ) )",

       TypeToString<TypeParam>::type, // return
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix, // return
       TypeToString<TypeParam>::prefix, //call
       reinterpret_cast<uintmax_t>(callee_entry_point), // address
       TypeToString<TypeParam>::type, // args
       TypeToString<TypeParam>::prefix // load
    );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(double, int32_t, float, int32_t, int32_t, float, int64_t, int32_t, TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0, 1, 2, 3, 4, 5, 6, 7, *i))  << "Input Trees: " << inputTrees;
    }
}