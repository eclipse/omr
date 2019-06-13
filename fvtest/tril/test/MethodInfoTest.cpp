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
#include "method_info.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(NULL, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(NULL != (pointer))

TEST(MethodInfoTest, EmptyMethod) {
    auto methodAST = parseString("(method return=\"NoType\")");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("", methodInfo.getName());
    ASSERT_EQ(TR::NoType, methodInfo.getReturnType());
    ASSERT_EQ(0, methodInfo.getArgCount());
    ASSERT_EQ(0, methodInfo.getArgTypes().size());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), methodInfo.getBodyAST());
}

TEST(MethodInfoTest, EmptyMethodWithInt16Return) {
    auto methodAST = parseString("(method return=\"Int16\")");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("", methodInfo.getName());
    ASSERT_EQ(TR::Int16, methodInfo.getReturnType());
    ASSERT_EQ(0, methodInfo.getArgCount());
    ASSERT_EQ(0, methodInfo.getArgTypes().size());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), methodInfo.getBodyAST());
}

TEST(MethodInfoTest, EmptyMethodWithDoubleReturn) {
    auto methodAST = parseString("(method return=\"Double\")");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("", methodInfo.getName());
    ASSERT_EQ(TR::Double, methodInfo.getReturnType());
    ASSERT_EQ(0, methodInfo.getArgCount());
    ASSERT_EQ(0, methodInfo.getArgTypes().size());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), methodInfo.getBodyAST());
}

TEST(MethodInfoTest, EmptyMethodWithName) {
    auto methodAST = parseString("(method name=\"empty\" return=\"NoType\")");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("empty", methodInfo.getName());
    ASSERT_EQ(TR::NoType, methodInfo.getReturnType());
    ASSERT_EQ(0, methodInfo.getArgCount());
    ASSERT_EQ(0, methodInfo.getArgTypes().size());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), methodInfo.getBodyAST());
}

TEST(MethodInfoTest, EmptyMethodWithInt32Argument) {
    auto methodAST = parseString("(method return=\"NoType\" args=[\"Int32\"])");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("", methodInfo.getName());
    ASSERT_EQ(TR::NoType, methodInfo.getReturnType());
    ASSERT_EQ(1, methodInfo.getArgCount());
    ASSERT_EQ(1, methodInfo.getArgTypes().size());
    ASSERT_EQ(TR::Int32, methodInfo.getArgTypes().at(0));
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), methodInfo.getBodyAST());
}

TEST(MethodInfoTest, EmptyMethodWithFloatAndInt32Arguments) {
    auto methodAST = parseString("(method return=\"NoType\" args=[\"Float\", \"Int32\"])");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("", methodInfo.getName());
    ASSERT_EQ(TR::NoType, methodInfo.getReturnType());
    ASSERT_EQ(2, methodInfo.getArgCount());
    ASSERT_EQ(2, methodInfo.getArgTypes().size());
    ASSERT_EQ(TR::Float, methodInfo.getArgTypes().at(0));
    ASSERT_EQ(TR::Int32, methodInfo.getArgTypes().at(1));
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), methodInfo.getBodyAST());
}

TEST(MethodInfoTest, MethodWithBody) {
    auto methodAST = parseString("(method return=\"NoType\" (block (return)))");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("", methodInfo.getName());
    ASSERT_EQ(TR::NoType, methodInfo.getReturnType());
    ASSERT_EQ(0, methodInfo.getArgCount());
    ASSERT_EQ(0, methodInfo.getArgTypes().size());
    ASSERT_NOTNULL(methodInfo.getBodyAST());
}

TEST(MethodInfoTest, MethodWithBodyInt8ReturnInt32andInt64Args) {
    auto methodAST = parseString("(method name=\"foo\" return=\"Int8\" args=[\"Int32\", \"Int64\"] (block (breturn (bconst 3))))");
    auto methodInfo = Tril::MethodInfo(methodAST);

    ASSERT_EQ("foo", methodInfo.getName());
    ASSERT_EQ(TR::Int8, methodInfo.getReturnType());
    ASSERT_EQ(2, methodInfo.getArgCount());
    ASSERT_EQ(2, methodInfo.getArgTypes().size());
    ASSERT_EQ(TR::Int32, methodInfo.getArgTypes().at(0));
    ASSERT_EQ(TR::Int64, methodInfo.getArgTypes().at(1));
    ASSERT_NOTNULL(methodInfo.getBodyAST());
}
