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

#include "ast.hpp"
#include "omrcfg.h"
#include <stdlib.h>
#include <string>

ASTNode* createNode(const char * name, ASTNodeArg* args, ASTNode* children,  ASTNode* next) {
    return new ASTNode(name, args, children, next);
}

ASTNodeArg* createNodeArg(const char * name, ASTValue* value,  ASTNodeArg* next) {
    return new ASTNodeArg(name, value, next);
}

ASTValue* createIntegerValue(uint64_t val) {
    return new ASTValue(val);
}

ASTValue* createFloatingPointValue(double val) {
    return new ASTValue(val);
}

ASTValue* createStrValue(const char* val) {
    return new ASTValue(val);
}

void appendSiblingNode(ASTNode* list, ASTNode* newNode) {
    ASTNode* n = list;
    while (n->next) { n = n->next; }
    n->next = newNode;
}

void appendSiblingArg(ASTNodeArg* list, ASTNodeArg* newArg) {
   ASTNodeArg* a = list;
   while (a->next) { a = a->next; }
   a->next = newArg;
}

void appendSiblingValue(ASTValue* list, ASTValue* newValue) {
    ASTValue* v = list;
    while (v->next) { v = v->next; }
    v->next = newValue;
}

uint16_t countNodes(const ASTNode* n) {
    uint16_t count = 0;
    while (n) {
       ++count;
       n = n->next;
    }
    return count;
}

const ASTNode* findNodeByNameInList(const ASTNode* list, const char* name) {
    const ASTNode* node = list;
    while (node) {
        if (strcmp(name, node->getName()) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

const ASTNode* findNodeByNameInTree(const ASTNode* tree, const char* name) {
    const ASTNode* node = tree;
    while (node) {
        if (strcmp(name, node->getName()) == 0) {
            return node;
        }

        const ASTNode* child = findNodeByNameInTree(node, name);
        if (child != NULL) {
            return child;
        }

        node = node->next;
    }
    return NULL;
}

void printASTValueUnion(FILE* file, const ASTValue* value) {
    switch (value->getType()) {
#if defined(OMR_ENV_DATA64)
        case ASTValue::Integer: fprintf(file, "%llu", value->getInteger()); break;
#else
        case ASTValue::Integer: fprintf(file, "%lu", value->getInteger()); break;
#endif /* OMR_ENV_DATA64 */
        case ASTValue::FloatingPoint: fprintf(file, "%f", value->getFloatingPoint()); break;
        case ASTValue::String: fprintf(file, "\"%s\"", value->getString()); break;
        default: fprintf(file, "{bad arg type %d}", value->getType());
    };
}

void printASTValue(FILE* file, const ASTValue* value) {
    const ASTValue* v = value;
    int isList = v->next != NULL;
    if (isList) {
        fprintf(file,"[");
    }
    printASTValueUnion(file, v);
    while (v->next) {
        v = v->next;
        fprintf(file, ", ");
        printASTValueUnion(file, v);
    }
    if (isList) {
        fprintf(file, "]");
    }
}

void printASTArgs(FILE* file, const ASTNodeArg* args) {
    const ASTNodeArg* a = args;
    while (a) {
        fprintf(file," ");
        if (a->getName() != NULL && strcmp("", a->getName()) != 0) {
            fprintf(file, "%s=", a->getName());
        }
        printASTValue(file, a->getValue());
        a = a->next;
    }
}

void printTreesToStdErr(const ASTNode* trees) {
   printTrees(stderr, trees, 1);
}

void printTrees(FILE* file, const ASTNode* trees, int indent) {
    const ASTNode* t = trees;
    while(t) {
        int indentVal = indent*2 + (int)strlen(t->getName()); // indent with two spaces
        fprintf(file, "(%p) %*s", t, indentVal, t->getName());
        printASTArgs(file, t->getArgs());
        fprintf(file, "\n");
        printTrees(file, t->getChildren(), indent + 1);
        t = t->next;
    }
}

bool operator == (const ASTNodeArg& lhs, const ASTNodeArg& rhs) {
    std::string lhsName = lhs.getName();
    std::string rhsName = rhs.getName();
    if (lhsName != rhsName) return false;

    auto lhsValue = lhs.getValue();
    auto rhsValue = rhs.getValue();

    while (lhsValue != NULL && rhsValue != NULL) {
        if (*lhsValue != *rhsValue) return false;
        lhsValue = lhsValue->next;
        rhsValue = rhsValue->next;
    }

    if (lhsValue != NULL || rhsValue != NULL) return false;

    return true;
}
