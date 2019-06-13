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

#include <cstring>
#include <gtest/gtest.h>
#include "ast.hpp"

TEST(ASTValueTest, CreateInt64ASTValue) {
   auto baseValue = 3UL;

   auto value = createIntegerValue(baseValue);

   ASSERT_EQ(ASTValue::Integer, value->getType());
   ASSERT_EQ(baseValue, value->get<ASTValue::Integer_t>());
}

TEST(ASTValueTest, CreateFloat64ASTValue) {
   auto baseValue = 3.0;

   auto value = createFloatingPointValue(baseValue);

   ASSERT_EQ(ASTValue::FloatingPoint, value->getType());
   ASSERT_EQ(baseValue, value->get<ASTValue::FloatingPoint_t>());
}

TEST(ASTValueTest, CreateStringASTValue) {
   auto baseValue = "a simple string";

   auto value = createStrValue(baseValue);

   ASSERT_EQ(ASTValue::String, value->getType());
   ASSERT_STREQ(baseValue, value->get<ASTValue::String_t>());
}

TEST(ASTValueTest, CreateValueList) {
    auto intBaseValue = 3UL;
    auto doubleBaseValue = 2.71828;
    auto stringBaseValue = "a string";

    auto intValue = createIntegerValue(intBaseValue);
    auto doubleValue = createFloatingPointValue(doubleBaseValue);
    auto stringValue = createStrValue(stringBaseValue);

    appendSiblingValue(intValue, doubleValue);
    appendSiblingValue(intValue, stringValue);

    auto v = intValue;
    ASSERT_EQ(ASTValue::Integer, v->getType());
    ASSERT_EQ(intBaseValue, v->getInteger());
    ASSERT_EQ(doubleValue, v->next);

    v = v->next;
    ASSERT_EQ(ASTValue::FloatingPoint, v->getType());
    ASSERT_EQ(doubleBaseValue, v->getFloatingPoint());
    ASSERT_EQ(stringValue, v->next);

    v = v->next;
    ASSERT_EQ(ASTValue::String, v->getType());
    ASSERT_STREQ(stringBaseValue, v->getString());
    ASSERT_EQ(reinterpret_cast<ASTValue*>(NULL), v->next);
}

/*
 * Since the comparison tests verify the behavior of overloaded comparison
 * operators, they explicitly call the operators inside an ASSERT_{TRUE|FALSE}
 * instead of using comparison ASSERTs (ASSERT_EQ, ASSERT_NE, etc.).
 */

TEST(ASTValueTest, CompareIntegerValueWithSelf) {
    const auto intBaseValue = 10UL;
    auto value = createIntegerValue(intBaseValue);

    ASSERT_TRUE(*value == *value);
    ASSERT_FALSE(*value != *value);
}

TEST(ASTValueTest, CompareEqualIntegerValues) {
    const auto intBaseValue = 10UL;
    auto value1 = createIntegerValue(intBaseValue);
    auto value2 = createIntegerValue(intBaseValue);

    ASSERT_TRUE(*value1 == *value2);
    ASSERT_FALSE(*value1 != *value2);
}

TEST(ASTValueTest, CompareFloatingPointValueWithSelf) {
    const double doubleBaseValue = 4.56;
    auto value = createFloatingPointValue(doubleBaseValue);

    ASSERT_TRUE(*value == *value);
    ASSERT_FALSE(*value != *value);
}

TEST(ASTValueTest, CompareEqualFloatingPointValues) {
    const double doubleBaseValue = 4.56;
    auto value1 = createFloatingPointValue(doubleBaseValue);
    auto value2 = createFloatingPointValue(doubleBaseValue);

    ASSERT_TRUE(*value1 == *value2);
    ASSERT_FALSE(*value1 != *value2);
}

TEST(ASTValueTest, CompareStringValueWithSelf) {
    auto baseStrValue = "Some string\n";
    auto value = createStrValue(baseStrValue);

    ASSERT_TRUE(*value == *value);
    ASSERT_FALSE(*value != *value);
}

TEST(ASTValueTest, CompareEqualStringValues) {
    auto baseStrValue = "Some string\n";
    auto value1 = createStrValue(baseStrValue);
    auto value2 = createStrValue(baseStrValue);

    ASSERT_TRUE(*value1 == *value2);
    ASSERT_FALSE(*value1 != *value2);
}

TEST(ASTValueTest, CompareDifferentIntegerValues) {
    auto value1 = createIntegerValue(3);
    auto value2 = createIntegerValue(15);

    ASSERT_TRUE(*value1 != *value2);
    ASSERT_FALSE(*value1 == *value2);
}

TEST(ASTValueTest, CompareDifferentFloatingPointValues) {
    auto value1 = createFloatingPointValue(3.6);
    auto value2 = createFloatingPointValue(3.4);

    ASSERT_TRUE(*value1 != *value2);
    ASSERT_FALSE(*value1 == *value2);
}

TEST(ASTValueTest, CompareDifferentStringValues) {
    auto value1 = createStrValue("string 1");
    auto value2 = createStrValue("string 2");

    ASSERT_TRUE(*value1 != *value2);
    ASSERT_FALSE(*value1 == *value2);
}

TEST(ASTValueTest, CompareIntegerAndFloatingPointValues) {
    auto value1 = createIntegerValue(3);
    auto value2 = createFloatingPointValue(3.0);

    ASSERT_TRUE(*value1 != *value2);
    ASSERT_FALSE(*value1 == *value2);
}

TEST(ASTValueTest, CompareFloatingPointAndStringValues) {
    auto value1 = createFloatingPointValue(3.6);
    auto value2 = createStrValue("3.6");

    ASSERT_TRUE(*value1 != *value2);
    ASSERT_FALSE(*value1 == *value2);
}

TEST(ASTValueTest, CompareStringAndIntegerValues) {
    auto value1 = createStrValue("123");
    auto value2 = createIntegerValue(123);

    ASSERT_TRUE(*value1 != *value2);
    ASSERT_FALSE(*value1 == *value2);
}

TEST(ASTNodeArgumentTest, CreateNodeArgumentWithJustInt64Value) {
   auto baseValue = 3UL;
   auto value = createIntegerValue(baseValue);
   auto arg = createNodeArg(NULL, value, NULL);

   ASSERT_STREQ(NULL, arg->getName());
   ASSERT_EQ(value, arg->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ASTNodeArgumentTest, CreateNodeArgumentWithJustFloat64Value) {
   auto baseValue = 3.0;
   auto value = createFloatingPointValue(baseValue);
   auto arg = createNodeArg(NULL, value, NULL);

   ASSERT_STREQ(NULL, arg->getName());
   ASSERT_EQ(value, arg->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ASTNodeArgumentTest, CreateNodeArgumentWithJustStringValue) {
   auto baseValue = "a simple string";
   auto value = createStrValue(baseValue);
   auto arg = createNodeArg(NULL, value, NULL);

   ASSERT_STREQ(NULL, arg->getName());
   ASSERT_EQ(value, arg->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ASTNodeArgumentTest, CreateNodeArgumentWithNameAndInt64Value) {
   auto baseValue = 3UL;
   auto argName = "the argument name";
   auto value = createIntegerValue(baseValue);
   auto arg = createNodeArg(argName, value, NULL);

   ASSERT_STREQ(argName, arg->getName());
   ASSERT_EQ(value, arg->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg->next);
}

TEST(ASTNodeArgumentTest, CreateListFrom2SingleNodeArguments) {
   auto baseValue_1 = "a simple string";
   auto argName_1 = "another name";
   auto value_1 = createStrValue(baseValue_1);
   auto arg_1 = createNodeArg(argName_1, value_1, NULL);

   auto baseValue_0 = 3.0;
   auto argName_0 = "the argument name";
   auto value_0 = createFloatingPointValue(baseValue_0);
   auto arg_0 = createNodeArg(argName_0, value_0, arg_1);

   ASSERT_STREQ(argName_0, arg_0->getName());
   ASSERT_EQ(value_0, arg_0->getValue());
   ASSERT_EQ(arg_1, arg_0->next);

   ASSERT_STREQ(argName_1, arg_1->getName());
   ASSERT_EQ(value_1, arg_1->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg_1->next);
}

TEST(ASTNodeArgumentTest, Concatenate2SingleNodeArguments) {
   auto baseValue_1 = "a simple string";
   auto argName_1 = "another name";
   auto value_1 = createStrValue(baseValue_1);
   auto arg_1 = createNodeArg(argName_1, value_1, NULL);

   auto baseValue_0 = 3.0;
   auto argName_0 = "the argument name";
   auto value_0 = createFloatingPointValue(baseValue_0);
   auto arg_0 = createNodeArg(argName_0, value_0, NULL);

   appendSiblingArg(arg_0, arg_1);

   ASSERT_STREQ(argName_0, arg_0->getName());
   ASSERT_EQ(value_0, arg_0->getValue());
   ASSERT_EQ(arg_1, arg_0->next);

   ASSERT_STREQ(argName_1, arg_1->getName());
   ASSERT_EQ(value_1, arg_1->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg_1->next);
}

TEST(ASTNodeArgumentTest, CreateListFromListOf2AndSingleArgument) {
   auto baseValue_2 = 3.0;
   auto argName_2 = "the argument name";
   auto value_2 = createFloatingPointValue(baseValue_2);
   auto arg_2 = createNodeArg(argName_2, value_2, NULL);

   auto baseValue_1 = "a simple string";
   auto argName_1 = "another name";
   auto value_1 = createStrValue(baseValue_1);
   auto arg_1 = createNodeArg(argName_1, value_1, NULL);

   auto baseValue_0 = 3.0;
   auto argName_0 = "yet another name";
   auto value_0 = createFloatingPointValue(baseValue_0);
   auto arg_0 = createNodeArg(argName_0, value_0, arg_1);

   appendSiblingArg(arg_0, arg_2);

   ASSERT_STREQ(argName_0, arg_0->getName());
   ASSERT_EQ(value_0, arg_0->getValue());
   ASSERT_EQ(arg_1, arg_0->next);

   ASSERT_STREQ(argName_1, arg_1->getName());
   ASSERT_EQ(value_1, arg_1->getValue());
   ASSERT_EQ(arg_2, arg_1->next);

   ASSERT_STREQ(argName_2, arg_2->getName());
   ASSERT_EQ(value_2, arg_2->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg_2->next);
}

TEST(ASTNodeArgumentTest, CreateListFrom3SingleArguments) {
   auto baseValue_2 = 3.0;
   auto argName_2 = "the argument name";
   auto value_2 = createFloatingPointValue(baseValue_2);
   auto arg_2 = createNodeArg(argName_2, value_2, NULL);

   auto baseValue_1 = "a simple string";
   auto argName_1 = "another name";
   auto value_1 = createStrValue(baseValue_1);
   auto arg_1 = createNodeArg(argName_1, value_1, NULL);

   auto baseValue_0 = 3.0;
   auto argName_0 = "yet another name";
   auto value_0 = createFloatingPointValue(baseValue_0);
   auto arg_0 = createNodeArg(argName_0, value_0, NULL);

   appendSiblingArg(arg_0, arg_1);
   appendSiblingArg(arg_0, arg_2);

   ASSERT_STREQ(argName_0, arg_0->getName());
   ASSERT_EQ(value_0, arg_0->getValue());
   ASSERT_EQ(arg_1, arg_0->next);

   ASSERT_STREQ(argName_1, arg_1->getName());
   ASSERT_EQ(value_1, arg_1->getValue());
   ASSERT_EQ(arg_2, arg_1->next);

   ASSERT_STREQ(argName_2, arg_2->getName());
   ASSERT_EQ(value_2, arg_2->getValue());
   ASSERT_EQ(reinterpret_cast<ASTNodeArg*>(NULL), arg_2->next);
}

TEST(ASTNodeArgumentTest, CompareArgumentsWithSelf) {
    auto v = createIntegerValue(10);
    auto name = "arg0";
    auto arg = createNodeArg(name, v, NULL);

    ASSERT_TRUE(*arg == *arg);
    ASSERT_FALSE(*arg != *arg);
}

TEST(ASTNodeArgumentTest, CompareEqualArguments) {
    auto v0 = createIntegerValue(10);
    auto v1 = createIntegerValue(10);
    auto name0 = "arg0";
    auto name1 = "arg0";
    auto arg0 = createNodeArg(name0, v0, NULL);
    auto arg1 = createNodeArg(name1, v1, NULL);

    ASSERT_TRUE(*arg0 == *arg1);
    ASSERT_FALSE(*arg0 != *arg1);
}

TEST(ASTNodeArgumentTest, CompareArgumentsWithDifferentNames) {
    auto v0 = createIntegerValue(10);
    auto v1 = createIntegerValue(10);
    auto name0 = "arg0";
    auto name1 = "arg1";
    auto arg0 = createNodeArg(name0, v0, NULL);
    auto arg1 = createNodeArg(name1, v1, NULL);

    ASSERT_TRUE(*arg0 != *arg1);
    ASSERT_FALSE(*arg0 == *arg1);
}

TEST(ASTNodeArgumentTest, CompareArgumentsWithDifferentValues) {
    auto v0 = createIntegerValue(10);
    auto v1 = createIntegerValue(14);
    auto name0 = "arg0";
    auto name1 = "arg0";
    auto arg0 = createNodeArg(name0, v0, NULL);
    auto arg1 = createNodeArg(name1, v1, NULL);

    ASSERT_TRUE(*arg0 != *arg1);
    ASSERT_FALSE(*arg0 == *arg1);
}

TEST(ASTNodeArgumentTest, CompareArgumentsWithDifferentTypes) {
    auto v0 = createIntegerValue(10);
    auto v1 = createFloatingPointValue(10.0);
    auto name0 = "arg0";
    auto name1 = "arg0";
    auto arg0 = createNodeArg(name0, v0, NULL);
    auto arg1 = createNodeArg(name1, v1, NULL);

    ASSERT_TRUE(*arg0 != *arg1);
    ASSERT_FALSE(*arg0 == *arg1);
}

TEST(ASTNodeTest, CreateNullNode) {
   auto node = createNode(NULL, NULL, NULL, NULL);

   ASSERT_STREQ(NULL, node->getName());
   ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), node->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node->next);
}

TEST(ASTNodeTest, CreateNodeWithJustName) {
   auto nodeName = "theName";
   auto node = createNode(nodeName, NULL, NULL, NULL);

   ASSERT_STREQ(nodeName, node->getName());
   ASSERT_EQ(reinterpret_cast<const ASTNodeArg*>(NULL), node->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node->next);
}

TEST(ASTNodeTest, CreateNodeWithJust1Argument) {
   auto nodeArgValue = createFloatingPointValue(3.0);
   auto nodeArg = createNodeArg("someArgument", nodeArgValue, NULL);
   auto node = createNode(NULL, nodeArg, NULL, NULL);

   ASSERT_STREQ(NULL, node->getName());
   ASSERT_EQ(nodeArg, node->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node->next);
}

ASTNodeArg* getNodeArgList() {
   auto nodeArgValue_2 = createFloatingPointValue(3.0);
   auto nodeArg_2 = createNodeArg("someArgument", nodeArgValue_2, NULL);
   auto nodeArgValue_1 = createIntegerValue(3);
   auto nodeArg_1 = createNodeArg("someArgument", nodeArgValue_1, nodeArg_2);
   auto nodeArgValue_0 = createStrValue("some string");
   auto nodeArg_0 = createNodeArg("someArgument", nodeArgValue_0, nodeArg_1);

   return nodeArg_0;
}

TEST(ASTNodeTest, CreateNodeWithJustArgumentList) {
   auto argList = getNodeArgList();
   auto node = createNode(NULL, argList, NULL, NULL);

   ASSERT_STREQ(NULL, node->getName());
   ASSERT_EQ(argList, node->getArgs());
   ASSERT_EQ(argList->next, node->getArgs()->next);
   ASSERT_EQ(argList->next->next, node->getArgs()->next->next);
   ASSERT_EQ(argList->next->next->next, node->getArgs()->next->next->next);
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node->next);
}

TEST(ASTNodeTest, CreateListFrom2SingleNodes) {
   auto nodeName_1 = "a node name";
   ASTNodeArg* argList_1 = NULL;
   auto node_1 = createNode(nodeName_1, argList_1, NULL, NULL);
   auto nodeName_0 = "another node name";
   ASTNodeArg* argList_0 = getNodeArgList();
   auto node_0 = createNode(nodeName_0, argList_0, NULL, NULL);

   appendSiblingNode(node_0, node_1);

   ASSERT_STREQ(nodeName_0, node_0->getName());
   ASSERT_EQ(argList_0, node_0->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_0->getChildren());
   ASSERT_EQ(node_1, node_0->next);

   ASSERT_STREQ(nodeName_1, node_1->getName());
   ASSERT_EQ(argList_1, node_1->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_1->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node_1->next);
}

TEST(ASTNodeTest, Concatenate2SingleNodes)  {
   auto nodeName_1 = "a node name";
   ASTNodeArg* argList_1 = NULL;
   auto node_1 = createNode(nodeName_1, argList_1, NULL, NULL);
   auto nodeName_0 = "another node name";
   ASTNodeArg* argList_0 = getNodeArgList();
   auto node_0 = createNode(nodeName_0, argList_0, NULL, node_1);

   ASSERT_STREQ(nodeName_0, node_0->getName());
   ASSERT_EQ(argList_0, node_0->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_0->getChildren());
   ASSERT_EQ(node_1, node_0->next);

   ASSERT_STREQ(nodeName_1, node_1->getName());
   ASSERT_EQ(argList_1, node_1->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_1->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node_1->next);
}

TEST(ASTNodeTest, CreateListFromListOf2AndSingleNode) {
   auto nodeName_2 = "some node name";
   ASTNodeArg* argList_2 = getNodeArgList();
   auto node_2 = createNode(nodeName_2, argList_2, NULL, NULL);
   auto nodeName_1 = "a node name";
   ASTNodeArg* argList_1 = NULL;
   auto node_1 = createNode(nodeName_1, argList_1, NULL, NULL);
   auto nodeName_0 = "another node name";
   ASTNodeArg* argList_0 = getNodeArgList();
   auto node_0 = createNode(nodeName_0, argList_0, NULL, NULL);

   appendSiblingNode(node_0, node_1);
   appendSiblingNode(node_0, node_2);

   ASSERT_STREQ(nodeName_0, node_0->getName());
   ASSERT_EQ(argList_0, node_0->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_0->getChildren());
   ASSERT_EQ(node_1, node_0->next);

   ASSERT_STREQ(nodeName_1, node_1->getName());
   ASSERT_EQ(argList_1, node_1->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_1->getChildren());
   ASSERT_EQ(node_2, node_1->next);

   ASSERT_STREQ(nodeName_2, node_2->getName());
   ASSERT_EQ(argList_2, node_2->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_2->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node_2->next);
}

TEST(ASTNodeTest, Concatenate3SingleNodes)  {
   auto nodeName_2 = "some node name";
   ASTNodeArg* argList_2 = getNodeArgList();
   auto node_2 = createNode(nodeName_2, argList_2, NULL, NULL);
   auto nodeName_1 = "a node name";
   ASTNodeArg* argList_1 = NULL;
   auto node_1 = createNode(nodeName_1, argList_1, NULL, NULL);
   auto nodeName_0 = "another node name";
   ASTNodeArg* argList_0 = getNodeArgList();
   auto node_0 = createNode(nodeName_0, argList_0, NULL, node_1);

   appendSiblingNode(node_0, node_2);

   ASSERT_STREQ(nodeName_0, node_0->getName());
   ASSERT_EQ(argList_0, node_0->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_0->getChildren());
   ASSERT_EQ(node_1, node_0->next);

   ASSERT_STREQ(nodeName_1, node_1->getName());
   ASSERT_EQ(argList_1, node_1->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_1->getChildren());
   ASSERT_EQ(node_2, node_1->next);

   ASSERT_STREQ(nodeName_2, node_2->getName());
   ASSERT_EQ(argList_2, node_2->getArgs());
   ASSERT_EQ(reinterpret_cast<const ASTNode*>(NULL), node_2->getChildren());
   ASSERT_EQ(reinterpret_cast<ASTNode*>(NULL), node_2->next);
}

ASTNodeArg* getMixedArgumentList() {
   auto argList = createNodeArg("arg0", createIntegerValue(3), NULL);
   appendSiblingArg(argList, createNodeArg("", createStrValue("value1"), NULL));
   appendSiblingArg(argList, createNodeArg("arg1", createFloatingPointValue(5.4), NULL));
   appendSiblingArg(argList, createNodeArg("", createStrValue("value3"), NULL));

   return argList;
}

TEST(ASTNodeTest, GetArgsTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getArgs();
   ASSERT_EQ(*argList, *arg);
   arg = arg->next;
   argList = argList->next;
   ASSERT_EQ(*argList, *arg);
   arg = arg->next;
   argList = argList->next;
   ASSERT_EQ(*argList, *arg);
   arg = arg->next;
   argList = argList->next;
   ASSERT_EQ(*argList, *arg);
}

TEST(ASTNodeTest, GetFirstArgumentTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getArgument(0);
   ASSERT_EQ(*argList, *arg);
}

TEST(ASTNodeTest, GetThirdArgumentTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getArgument(2);
   argList = argList->next->next;
   ASSERT_EQ(*argList, *arg);
}

TEST(ASTNodeTest, GetFirstPositionalArgumentTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getPositionalArg(0);
   argList = argList->next;
   ASSERT_EQ(*argList, *arg);
}

TEST(ASTNodeTest, GetSecondPositionalArgumentTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getPositionalArg(1);
   argList = argList->next->next->next;
   ASSERT_EQ(*argList, *arg);
}

TEST(ASTNodeTest, GetFirstNamedArgumentTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getArgByName("arg0");
   ASSERT_EQ(*argList, *arg);
}

TEST(ASTNodeTest, GetSecondNamedArgumentTest) {
   auto argList = getMixedArgumentList();
   auto node = createNode("testNode", argList, NULL, NULL);

   auto arg = node->getArgByName("arg1");
   argList = argList->next->next;
   ASSERT_EQ(*argList, *arg);
}
