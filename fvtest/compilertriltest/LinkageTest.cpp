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
   char inputTrees[200] = {0};
   const auto format_string = "(method return=Int32  args=[Int32] (block (ireturn (icall address=%p args=[Int32] linkage=noexist  (iload parm=0)) )  ))";
   std::snprintf(inputTrees, 200, format_string, &passThrough<TypeParam>); 

   auto trees = parseString(inputTrees);
   ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

#ifdef TR_TARGET_X86
   Tril::DefaultCompiler compiler{trees};
   ASSERT_NE(0, compiler.compile()) << "Compilation succeeded unexpectedly\n" << "Input trees: " << inputTrees;
#endif
}

TYPED_TEST(LinkageTest, SystemLinkageParameterPassingSingleArg) {
    char inputTrees[200] = {0};
    const auto format_string = "(method return=%s args=[%s] (block (%sreturn (%scall address=%p args=[%s] linkage=system (%sload parm=0)) )  ))";
    std::snprintf(inputTrees, 200, format_string, TypeToString<TypeParam>::type, // Return
                                                  TypeToString<TypeParam>::type, //Args
                                                  TypeToString<TypeParam>::prefix, //return
                                                  TypeToString<TypeParam>::prefix, //call
                                                  &passThrough<TypeParam>, //address
                                                  TypeToString<TypeParam>::type, //args
                                                  TypeToString<TypeParam>::prefix //load
                                                  );

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    // Execution of this test is disabled on non-X86 platforms, as we
    // do not have trampoline support, and so this call may be out of
    // range for some architectures.
#ifdef TR_TARGET_X86
    Tril::DefaultCompiler compiler{trees};
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam)>();

    EXPECT_EQ(static_cast<TypeParam>(0), entry_point( static_cast<TypeParam>(0)))  << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(1), entry_point( static_cast<TypeParam>(1)))  << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(-1), entry_point(static_cast<TypeParam>(-1))) << "Input Trees: " << inputTrees;
#endif
}

template <typename T>
T fourthArg(T a, T b, T c, T d) { return d; }


TYPED_TEST(LinkageTest, SystemLinkageParameterPassingFourArg) {
    char inputTrees[400] = {0};
    const auto format_string = "(method return=%s args=[%s,%s,%s,%s] (block (%sreturn (%scall address=%p args=[%s,%s,%s,%s] linkage=system"
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
                                                  &fourthArg<TypeParam>,           // address
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

    // Execution of this test is disabled on non-X86 platforms, as we
    // do not have trampoline support, and so this call may be out of
    // range for some architectures.
#ifdef TR_TARGET_X86
    Tril::DefaultCompiler compiler{trees};
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<TypeParam (*)(TypeParam,TypeParam,TypeParam,TypeParam)>();

    EXPECT_EQ(static_cast<TypeParam>(1024),    entry_point(0,0,0,static_cast<TypeParam>(1024)))     << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(-1),      entry_point(0,0,0,static_cast<TypeParam>(-1)))       << "Input Trees: " << inputTrees;
    EXPECT_EQ(static_cast<TypeParam>(0xf0f0f), entry_point(0,0,0,static_cast<TypeParam>(0xf0f0f)))  << "Input Trees: " << inputTrees;
#endif
}
