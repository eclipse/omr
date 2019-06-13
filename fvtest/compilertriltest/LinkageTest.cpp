/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

TEST(TypeToString,TestTemplate) {
   EXPECT_STREQ("Int32", TypeToString<int32_t>::type);
   EXPECT_STREQ("Double", TypeToString<double>::type);
}

template <typename T>
T passThrough(T x) { return x; }

template <typename T>
class LinkageTest : public TRTest::JitTest {};

typedef ::testing::Types<int32_t, int64_t, float, double> InputTypes;
TYPED_TEST_CASE(LinkageTest, InputTypes);

TYPED_TEST(LinkageTest, InvalidLinkageTest) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[200] = {0};
    const auto format_string = "(method return=Int32  args=[Int32] (block (ireturn (icall address=0x%jX args=[Int32] linkage=noexist  (iload parm=0)) )  ))";
    std::snprintf(inputTrees, 200, format_string, reinterpret_cast<uintmax_t>(static_cast<TypeParam (*)(TypeParam)>(passThrough<TypeParam>)));

    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_NE(0, compiler.compile()) << "Compilation succeeded unexpectedly\n" << "Input trees: " << inputTrees;
}

TYPED_TEST(LinkageTest, SystemLinkageParameterPassingSingleArg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[200] = {0};
    const auto format_string = "(method return=%s args=[%s] (block (%sreturn (%scall address=0x%jX args=[%s] linkage=system (%sload parm=0)) )  ))";
    std::snprintf(inputTrees, 200, format_string, TypeToString<TypeParam>::type, // Return
                                                  TypeToString<TypeParam>::type, //Args
                                                  TypeToString<TypeParam>::prefix, //return
                                                  TypeToString<TypeParam>::prefix, //call
                                                  reinterpret_cast<uintmax_t>(static_cast<TypeParam (*)(TypeParam)>(passThrough<TypeParam>)),//address
                                                  TypeToString<TypeParam>::type, //args
                                                  TypeToString<TypeParam>::prefix //load
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

template <typename T>
T fourthArg(T a, T b, T c, T d) { return d; }

TYPED_TEST(LinkageTest, SystemLinkageParameterPassingFourArg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[400] = {0};
    const auto format_string = "(method return=%s args=[%s,%s,%s,%s] (block (%sreturn (%scall address=0x%jX args=[%s,%s,%s,%s] linkage=system"
                                 " (%sload parm=0)"
                                 " (%sload parm=1)"
                                 " (%sload parm=2)"
                                 " (%sload parm=3)"
                                 ") )  ))";
    std::snprintf(inputTrees, 400, format_string, TypeToString<TypeParam>::type,   // Return
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::prefix, // return
                                                  TypeToString<TypeParam>::prefix, // call
                                                  reinterpret_cast<uintmax_t>(static_cast<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam)>(fourthArg<TypeParam>)),// address
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix  // load
                                                  );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0,0,0, *i))  << "Input Trees: " << inputTrees;
    }
}

template <typename T>
T (*get_fourth_arg_from_callee())(T,T,T,T) {
    char inputTrees[400] = {0};
    const auto format_string =
        "(method return=%s args=[%s,%s,%s,%s] (block (%sreturn (%sload parm=3))))";
    std::snprintf(inputTrees, 400, format_string,
        TypeToString<T>::type,   // Return
        TypeToString<T>::type,   // Args
        TypeToString<T>::type,   // Args
        TypeToString<T>::type,   // Args
        TypeToString<T>::type,   // Args
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

    return compiler.getEntryPoint<T (*)(T,T,T,T)>();
}

TYPED_TEST(LinkageTest, SystemLinkageJitedToJitedParameterPassingFourArg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[400] = {0};

    auto callee_entry_point = get_fourth_arg_from_callee<TypeParam>();
    ASSERT_NOTNULL(callee_entry_point) << "Compilation of the callee failed unexpectedly\n";

    EXPECT_EQ(static_cast<TypeParam>(1024),    callee_entry_point(0,0,0,static_cast<TypeParam>(1024)));
    EXPECT_EQ(static_cast<TypeParam>(-1),      callee_entry_point(0,0,0,static_cast<TypeParam>(-1)));
    EXPECT_EQ(static_cast<TypeParam>(0xf0f0f), callee_entry_point(0,0,0,static_cast<TypeParam>(0xf0f0f)));

    const auto format_string = "(method return=%s args=[%s,%s,%s,%s] (block (%sreturn (%scall address=0x%jX args=[%s,%s,%s,%s] linkage=system"
                                 " (%sload parm=0)"
                                 " (%sload parm=1)"
                                 " (%sload parm=2)"
                                 " (%sload parm=3)"
                                 ") )  ))";
    std::snprintf(inputTrees, 400, format_string, TypeToString<TypeParam>::type,   // Return
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::prefix, // return
                                                  TypeToString<TypeParam>::prefix, // call
                                                  callee_entry_point,              // address
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix  // load
                                                  );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0,0,0, *i))  << "Input Trees: " << inputTrees;
    }
}

template <typename T>
T fifthArg(T a, T b, T c, T d, T e) { return e; }

/**
 * In accordance with the Microsoft x64 calling convention, only four parameters can be passed in registers (both integer
 * and floating point). Additional arguments are pushed onto the stack (right to left). It is an interesting case when a JITed
 * method calls a method with more than four parameters.
 */
TYPED_TEST(LinkageTest, SystemLinkageParameterPassingFiveArg) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[400] = {0};
    const auto format_string = "(method return=%s args=[%s,%s,%s,%s,%s] (block (%sreturn (%scall address=0x%jX args=[%s,%s,%s,%s,%s] linkage=system"
                                 " (%sload parm=0)"
                                 " (%sload parm=1)"
                                 " (%sload parm=2)"
                                 " (%sload parm=3)"
                                 " (%sload parm=4)"
                                 ") )  ))";
    std::snprintf(inputTrees, 400, format_string, TypeToString<TypeParam>::type,   // Return
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::prefix, // return
                                                  TypeToString<TypeParam>::prefix, // call
                                                  reinterpret_cast<uintmax_t>(static_cast<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam,TypeParam)>(fifthArg<TypeParam>)),// address
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix  // load
                                                  );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam,TypeParam)>();

    auto inputs = TRTest::const_values<TypeParam>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(static_cast<TypeParam>(*i), entry_point(0,0,0,0, *i))  << "Input Trees: " << inputTrees;
    }
}

/*
 * It doesn't matter what this function exactly do. It is requred just to demonstrate what can be expected if a callee
 * actively uses the stack (has a lot of local variables).
 */
template <typename T>
T stackUser(T a, T b, T c, T d, T e) {
    volatile T x, y, z, w;
    for (int32_t i = 0; i < a; i+= b) {
        w = (c + b) * (i + e);
        x = a * (i + d + 1);
        y = b - d + c;
        z = c * 2 * (i + 1);
    }
    return x + y + z + w;
}

/**
 * In the Microsoft x64 calling convention, it is the caller's responsibility to allocate 32 bytes of "shadow space" on
 * the stack right before calling the function (regardless of the actual number of parameters used), and to pop the stack
 * after the call. In the case, when the callee actively uses the stack, the callee can relate on this "shadow space"
 * and save arguments or other nonvolatile registers there. The test demonstrates what can be happen when the callee is
 * an actively stack user.
 */
TYPED_TEST(LinkageTest, SystemLinkageParameterPassingFiveArgToStackUser) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[400] = {0};
    const auto format_string = "(method return=%s args=[%s,%s,%s,%s,%s] (block (%sreturn (%scall address=0x%jX args=[%s,%s,%s,%s,%s] linkage=system"
                                 " (%sload parm=0)"
                                 " (%sload parm=1)"
                                 " (%sload parm=2)"
                                 " (%sload parm=3)"
                                 " (%sload parm=4)"
                                 ") )  ))";
    std::snprintf(inputTrees, 400, format_string, TypeToString<TypeParam>::type,   // Return
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::type,   // Args
                                                  TypeToString<TypeParam>::prefix, // return
                                                  TypeToString<TypeParam>::prefix, // call
                                                  reinterpret_cast<uintmax_t>(static_cast<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam,TypeParam)>(stackUser<TypeParam>)),// address
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::type,   // args
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix, // load
                                                  TypeToString<TypeParam>::prefix  // load
                                                  );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam,TypeParam)>();

    // Check the compiled function itself
    EXPECT_EQ(static_cast<TypeParam>(2053),     stackUser<TypeParam>(1,1,1,1,static_cast<TypeParam>(1024)))     << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(3),        stackUser<TypeParam>(1,1,1,1,static_cast<TypeParam>(-1)))       << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(0x1e1e23), stackUser<TypeParam>(1,1,1,1,static_cast<TypeParam>(0xf0f0f)))  << "Input Trees: " << inputTrees;

    // Check the linkage
    EXPECT_EQ(static_cast<TypeParam>(2053),     entry_point(1,1,1,1,static_cast<TypeParam>(1024)))     << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(3),        entry_point(1,1,1,1,static_cast<TypeParam>(-1)))       << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(0x1e1e23), entry_point(1,1,1,1,static_cast<TypeParam>(0xf0f0f)))  << "Input Trees: " << inputTrees;
}

class LinkageWithMixedTypesTest : public TRTest::JitTest {};

int32_t fourthArgFromMixedTypes(double a, int32_t b, double c, int32_t d) { return d; }

typedef int32_t (*FourMixedArgumentFunction)(double,int32_t,double,int32_t);

TEST_F(LinkageWithMixedTypesTest, SystemLinkageParameterPassingFourArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[400] = {0};
    const auto format_string = "(method return=Int32 args=[Double,Int32,Double,Int32]"
                               "  (block (ireturn (icall address=0x%jX args=[Double,Int32,Double,Int32] linkage=system"
                                 " (dload parm=0)"
                                 " (iload parm=1)"
                                 " (dload parm=2)"
                                 " (iload parm=3)"
                                 ") )  ))";
    std::snprintf(inputTrees, 400, format_string,
        reinterpret_cast<uintmax_t>(static_cast<FourMixedArgumentFunction>(fourthArgFromMixedTypes)));

    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<FourMixedArgumentFunction>();

    auto inputs = TRTest::const_values<int32_t>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_EQ(*i, entry_point(0.0,0,0.0, *i))  << "Input Trees: " << inputTrees;
    }
}

double fifthArgFromMixedTypes(double a, int32_t b, double c, int32_t d, double e) { return e; }

typedef double (*FiveMixedArgumentFunction)(double,int32_t,double,int32_t,double);

TEST_F(LinkageWithMixedTypesTest, SystemLinkageParameterPassingFiveArgWithMixedTypes) {
    OMRPORT_ACCESS_FROM_OMRPORT(TRTest::TestWithPortLib::privateOmrPortLibrary);
    std::string arch = omrsysinfo_get_CPU_architecture();
    SKIP_IF(OMRPORT_ARCH_HAMMER != arch, MissingImplementation)
        << "Test is skipped on non-x86-64 platforms because calls are not currently supported on other platforms (see issue #1645)";

    char inputTrees[400] = {0};
    const auto format_string = "(method return=Double args=[Double,Int32,Double,Int32,Double]"
                               "  (block (dreturn (dcall address=0x%jX args=[Double,Int32,Double,Int32,Double] linkage=system"
                                 " (dload parm=0)"
                                 " (iload parm=1)"
                                 " (dload parm=2)"
                                 " (iload parm=3)"
                                 " (dload parm=4)"
                                 ") )  ))";
    std::snprintf(inputTrees, 400, format_string,
        reinterpret_cast<uintmax_t>(static_cast<FiveMixedArgumentFunction>(fifthArgFromMixedTypes)));

    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<FiveMixedArgumentFunction>();

    auto inputs = TRTest::const_values<double>();
    for (auto i = inputs.begin(); i != inputs.end(); ++i) {
        SCOPED_TRACE(*i);
        EXPECT_DOUBLE_EQ(*i, entry_point(0.0,0,0.0,0, *i))  << "Input Trees: " << inputTrees;
    }
}
