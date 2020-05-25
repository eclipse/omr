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

#include "gtest/gtest.h"

#include "ast.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(NULL, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(NULL != (pointer))


typedef std::pair<const char *, const char *> TestParamWithExpectedResultString;

class SingleNodeWithName : public testing::TestWithParam<TestParamWithExpectedResultString> {};

TEST_P(SingleNodeWithName, VerifyNode) {
    auto trees = parseString(GetParam().first);

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ(GetParam().second, trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);
}

INSTANTIATE_TEST_CASE_P(
    ParserTest,
    SingleNodeWithName,
    testing::Values(
        TestParamWithExpectedResultString("(nodeName)", "nodeName"),
        TestParamWithExpectedResultString("(@commandName)", "@commandName")
));

TEST(ParserTest, TwoNodesWithJustName) {
    auto trees = parseString("(nodeName)(otherNode)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_NOTNULL(trees->next);

    trees = trees->next;
    ASSERT_STREQ("otherNode", trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);
}

TEST(ParserTest, SingleNodeWithChild) {
    const auto* trees = parseString("(nodeName (childNode))");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_NOTNULL(trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    trees = trees->getChildren();
    ASSERT_STREQ("childNode", trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);
}

TEST(ParserTest, NodeWithChildAndSibling) {
    auto trees = parseString("(nodeName (childNode)) (siblingNode)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_NOTNULL(trees->getChildren());
    ASSERT_NOTNULL(trees->next);

    auto child = trees->getChildren();
    ASSERT_STREQ("childNode", child->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), child->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), child->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), child->next);

    auto sibling = trees->next;
    ASSERT_STREQ("siblingNode", sibling->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), sibling->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), sibling->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), sibling->next);
}

typedef std::pair<const char *, ASTValue::Integer_t> TestParamWithExpectedResultInt;

class SingleNodeWithIntArg : public testing::TestWithParam<TestParamWithExpectedResultInt> {};

TEST_P(SingleNodeWithIntArg, VerifyNode) {
    auto trees = parseString(GetParam().first);

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::Integer, arg->getValue()->getType());
    ASSERT_EQ(GetParam().second, arg->getValue()->getInteger());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

INSTANTIATE_TEST_CASE_P(
    ParserTestSingleNodeWithIntArgAsDecValue,
    SingleNodeWithIntArg,
    testing::Values(
        TestParamWithExpectedResultInt("(nodeName 0)", 0),
        TestParamWithExpectedResultInt("(nodeName 3)", 3),
        TestParamWithExpectedResultInt("(nodeName -3)", -3),
        TestParamWithExpectedResultInt("(nodeName 9223372036854775807)", 9223372036854775807U),
        TestParamWithExpectedResultInt("(nodeName 18446744073709551615)", 18446744073709551615ULL)
));

INSTANTIATE_TEST_CASE_P(
    ParserTestSingleNodeWithIntArgAsHexValue,
    SingleNodeWithIntArg,
    testing::Values(
        TestParamWithExpectedResultInt("(nodeName 0xabc123)", 0xabc123),
        TestParamWithExpectedResultInt("(nodeName -0x249DEF)", -0x249DEF),
        TestParamWithExpectedResultInt("(nodeName 0xFFFFFFFF00000000)", 0xFFFFFFFF00000000)
));

typedef std::pair<const char *, ASTValue::FloatingPoint_t> TestParamWithExpectedResultFloat;

class SingleNodeWithFloatArg : public testing::TestWithParam<TestParamWithExpectedResultFloat> {};

TEST_P(SingleNodeWithFloatArg, VerifyNode) {
    auto trees = parseString(GetParam().first);

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(GetParam().second, arg->getValue()->getFloatingPoint());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

INSTANTIATE_TEST_CASE_P(
    ParserTest,
    SingleNodeWithFloatArg,
    testing::Values(
        TestParamWithExpectedResultFloat("(nodeName 3.00)", 3.00),
        TestParamWithExpectedResultFloat("(nodeName -3.00)", -3.00),
        TestParamWithExpectedResultFloat("(nodeName 9223372036854775807.00)", 9223372036854775807.00)
));

TEST(ParserTest, SingleNodeWithStringArg) {
    auto trees = parseString("(nodeName \"foo\")");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::String, arg->getValue()->getType());
    ASSERT_STREQ("foo", arg->getValue()->getString());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

class SingleNodeWithUnnamedArg : public testing::TestWithParam<TestParamWithExpectedResultString> {};

TEST_P(SingleNodeWithUnnamedArg, VerifyNode) {
    auto trees = parseString(GetParam().first);

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::String, arg->getValue()->getType());
    ASSERT_STREQ(GetParam().second, arg->getValue()->getString());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

INSTANTIATE_TEST_CASE_P(
    ParserTest,
    SingleNodeWithUnnamedArg,
    testing::Values(
        TestParamWithExpectedResultString("(nodeName id)", "id"),
        TestParamWithExpectedResultString("(nodeName @cmd)", "@cmd")
));

TEST(ParserTest, SingleNodeWithNamedArg) {
    auto trees = parseString("(nodeName arg=3.14)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(3.14, arg->getValue()->getFloatingPoint());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithNamedIdentifierArg) {
    auto trees = parseString("(nodeName arg=ID)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_EQ(ASTValue::String, arg->getValue()->getType());
    ASSERT_STREQ("ID", arg->getValue()->getString());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithNamedCommandArg) {
    auto trees = parseString("(nodeName arg=@cmd)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_EQ(ASTValue::String, arg->getValue()->getType());
    ASSERT_STREQ("@cmd", arg->getValue()->getString());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithNamedArgAndAnonymousArg) {
    auto trees = parseString("(nodeName arg=\"foo\" 6.33)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_EQ(ASTValue::String, arg->getValue()->getType());
    ASSERT_STREQ("foo", arg->getValue()->getString());
    ASSERT_NOTNULL(arg->next);

    arg = arg->next;
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(6.33, arg->getValue()->getFloatingPoint());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithNamedListArg) {
    auto trees = parseString("(nodeName arg=[5, 3.14159, \"foo\"])");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_NOTNULL(arg->getValue());

    auto value = arg->getValue();
    ASSERT_EQ(ASTValue::Integer, value->getType());
    ASSERT_EQ(5, value->getInteger());
    ASSERT_NOTNULL(value->next);

    value = value->next;
    ASSERT_EQ(ASTValue::FloatingPoint, value->getType());
    ASSERT_EQ(3.14159, value->getFloatingPoint());
    ASSERT_NOTNULL(value->next);

    value = value->next;
    ASSERT_EQ(ASTValue::String, value->getType());
    ASSERT_STREQ("foo", value->getString());
    ASSERT_EQ(reinterpret_cast<ASTValue*>(NULL), value->next);
}

TEST(ParserTest, SingleNodeWithAnonymousArgAndNamedArg) {
    auto trees = parseString("(nodeName 5.11 arg2=2)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(5.11, arg->getValue()->getFloatingPoint());
    ASSERT_NOTNULL(arg->next);

    arg = arg->next;
    ASSERT_STREQ("arg2", arg->getName());
    ASSERT_EQ(ASTValue::Integer, arg->getValue()->getType());
    ASSERT_EQ(2, arg->getValue()->getInteger());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithNamedArgAndChild) {
    const auto* trees = parseString("(nodeName arg=3 (childNode))");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_NOTNULL(trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_EQ(ASTValue::Integer, arg->getValue()->getType());
    ASSERT_EQ(3, arg->getValue()->getInteger());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);

    trees = trees->getChildren();
    ASSERT_STREQ("childNode", trees->getName());
    ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);
}

TEST(ParserTest, SingleNodeWithAnonymousArgAndChildWithNamedArg) {
    const auto* trees = parseString("(nodeName \"bar\" (childNode arg=4.0))");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_NOTNULL(trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::String, arg->getValue()->getType());
    ASSERT_STREQ("bar", arg->getValue()->getString());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);

    trees = trees->getChildren();
    ASSERT_STREQ("childNode", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    arg = trees->getArgs();
    ASSERT_STREQ("arg", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(4.0, arg->getValue()->getFloatingPoint());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithTwoAnonymousArgs) {
    auto trees = parseString("(nodeName 2.71828 3.14159)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(2.71828, arg->getValue()->getFloatingPoint());
    ASSERT_NOTNULL(arg->next);

    arg = arg->next;
    ASSERT_STREQ("", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(3.14159, arg->getValue()->getFloatingPoint());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ParserTest, SingleNodeWithTwoNamedArgs) {
    auto trees = parseString("(nodeName pi=3.14159 e=2.71828)");

    ASSERT_NOTNULL(trees);
    ASSERT_STREQ("nodeName", trees->getName());
    ASSERT_NOTNULL(trees->getArgs());
    ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), trees->getChildren());
    ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), trees->next);

    auto arg = trees->getArgs();
    ASSERT_STREQ("pi", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(3.14159, arg->getValue()->getFloatingPoint());
    ASSERT_NOTNULL(arg->next);

    arg = arg->next;
    ASSERT_STREQ("e", arg->getName());
    ASSERT_EQ(ASTValue::FloatingPoint, arg->getValue()->getType());
    ASSERT_EQ(2.71828, arg->getValue()->getFloatingPoint());
    ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}
