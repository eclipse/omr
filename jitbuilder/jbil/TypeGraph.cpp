/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "TypeGraph.hpp"
#include "Operation.hpp"
#include "Type.hpp"
#include "TypeDictionary.hpp"


using namespace OMR::JitBuilder;

uint64_t OMR::JitBuilder::TypeGraph::globalIndex = 0;
uint64_t OMR::JitBuilder::TypeGraph::OperationNode::globalID = 0;

OMR::JitBuilder::TypeGraph::TypeGraph(TypeDictionary * types)
   : _id(globalIndex++)
   , _types(types)
   , _linkedGraph(NULL)
   {
   }

OMR::JitBuilder::TypeGraph::TypeGraph(TypeDictionary * types, TypeGraph *linkedGraph)
   : _id(globalIndex++)
   , _types(types)
   , _linkedGraph(linkedGraph)
   {
   }

OMR::JitBuilder::TypeGraph::TypeNode *
OMR::JitBuilder::TypeGraph::lookupType(Type * type)
   {
   std::map<Type *,TypeNode *>::iterator found=_typeToNode.find(type);
   if (found != _typeToNode.end())
      return found->second;

   if (_linkedGraph)
      return _linkedGraph->lookupType(type);

   assert(false);
   return NULL;
   }

void
OMR::JitBuilder::TypeGraph::registerType(Type * type)
   {
   std::map<Type *,TypeNode *>::iterator found=_typeToNode.find(type);
   if (found == _typeToNode.end())
      {
      TypeNode * node = new TypeNode(type);
      _typeToNode[type] = node;
      }
   }

void
OMR::JitBuilder::TypeGraph::registerFunctionType(FunctionType *function)
   {
   registerType(function);

   NodeList *list = lookupNodeList(aCall);
   TypeNode *functionTypeNode = lookupType(function);
   int32_t numParms = function->numParms();
   TypeNode **parmTypeNodes = new TypeNode*[numParms];
   for (int32_t p=0;p < numParms;p++)
      {
      parmTypeNodes[p] = lookupType(function->parmType(p));
      }
   TypeNode *returnTypeNode = lookupType(function->returnType());

   OperationNode *newFunctionType = new FunctionOperationNode(returnTypeNode, aCall, functionTypeNode, numParms, parmTypeNodes);
   list->push_back(newFunctionType);
   }

OMR::JitBuilder::TypeGraph::NodeList *
OMR::JitBuilder::TypeGraph::lookupNodeList(Action a)
   {
   NodeList *list = NULL;
   std::map<Action, NodeList *>::iterator found = _nodesForAction.find(a);
   if (found != _nodesForAction.end())
      return found->second;

   if (_linkedGraph)
      list = _linkedGraph->lookupNodeList(a);

   if (!list)
      {
      list = new NodeList();
      _nodesForAction[a] = list;
      }

   assert(list);
   return list;
   }

Type *
OMR::JitBuilder::TypeGraph::producedType(Action a, Type * t)
   {
   NodeList *list = lookupNodeList(a);
   TypeNode *tNode = lookupType(t);

   for (NodeList::iterator it=list->begin(); it != list->end(); it++)
      {
      Node * n = *it;
      if (n->kind == UnaryOperationKind)
         {
         UnaryOperationNode *op = static_cast<UnaryOperationNode *>(n);
         if (op->operand == tNode)
            return op->produces->type;
         }
      }

   // no match found, so operation is not valid
   return NULL;
   }

Type *
OMR::JitBuilder::TypeGraph::producedType(Action a, Type *left, Type *right)
   {
   NodeList *list = lookupNodeList(a);
   TypeNode *leftTypeNode = lookupType(left);
   TypeNode *rightTypeNode = lookupType(right);

   for (NodeList::iterator it=list->begin(); it != list->end(); it++)
      {
      Node * n = *it;
      if (n->kind == BinaryOperationKind)
         {
         BinaryOperationNode *op = static_cast<BinaryOperationNode *>(n);
         if (op->left == leftTypeNode && op->right == rightTypeNode)
            return op->produces->type;
         }
      }

   // no match found, so operation is not valid
   return NULL;
   }

Type *
OMR::JitBuilder::TypeGraph::producedType(Action a, Type *one, Type *two, Type *three)
   {
   NodeList *list = lookupNodeList(a);
   TypeNode *oneTypeNode = lookupType(one);
   TypeNode *twoTypeNode = lookupType(two);
   TypeNode *threeTypeNode = lookupType(three);

   for (NodeList::iterator it=list->begin(); it != list->end(); it++)
      {
      Node * n = *it;
      if (n->kind == TrinaryOperationKind)
         {
         TrinaryOperationNode *op = static_cast<TrinaryOperationNode *>(n);
         if (op->first == oneTypeNode && op->second == twoTypeNode && op->third == threeTypeNode)
            return op->produces->type;
         }
      }

   // no match found, so operation is not valid
   return NULL;
   }

Type *
OMR::JitBuilder::TypeGraph::producedType(FunctionType *function, int32_t numArgs, Type ** argTypes)
   {
   NodeList *list = lookupNodeList(aCall);
   TypeNode *functionTypeNode = lookupType(function);
   TypeNode **argTypeNodes = new TypeNode*[numArgs];
   for (int32_t a=0;a < numArgs;a++)
      {
      argTypeNodes[a] = lookupType(argTypes[a]);
      }
   
   for (NodeList::iterator it=list->begin(); it != list->end(); it++)
      {
      Node * n = *it;
      if (n->kind == FunctionOperationKind)
         {
         FunctionOperationNode *op = static_cast<FunctionOperationNode *>(n);
         if (op->function == functionTypeNode && op->numParms == numArgs)
            {
            int32_t a=0;
            for (a=0;a < numArgs;a++)
               if (op->parmTypes[a] != argTypeNodes[a])
                  break;
            if (a == numArgs)
               return op->produces->type;
            }
         }
      }

   // no match found, so operation is not valid
   return NULL;
   }

void
OMR::JitBuilder::TypeGraph::registerValidOperation(Type * produces, Action a, Type * operand)
   {
   NodeList *list = lookupNodeList(a);
   TypeNode *producesTypeNode = lookupType(produces);
   TypeNode *operandTypeNode = lookupType(operand);
   OperationNode *newType = new UnaryOperationNode(producesTypeNode, a, operandTypeNode);
   list->push_back(newType);
   }

void
OMR::JitBuilder::TypeGraph::registerValidOperation(Type * produces, Action a, Type * left, Type * right)
   {
   NodeList *list = lookupNodeList(a);
   TypeNode *producesTypeNode = lookupType(produces);
   TypeNode *leftTypeNode = lookupType(left);
   TypeNode *rightTypeNode = lookupType(right);
   OperationNode *newType = new BinaryOperationNode(producesTypeNode, a, leftTypeNode, rightTypeNode);
   list->push_back(newType);
   }

void
OMR::JitBuilder::TypeGraph::registerValidOperation(Type * produces, Action a, Type * one, Type * two, Type * three)
   {
   NodeList *list = lookupNodeList(a);
   TypeNode *producesTypeNode = lookupType(produces);
   TypeNode *oneTypeNode = lookupType(one);
   TypeNode *twoTypeNode = lookupType(two);
   TypeNode *threeTypeNode = lookupType(three);
   OperationNode *newType = new TrinaryOperationNode(producesTypeNode, a, oneTypeNode, twoTypeNode, threeTypeNode);
   list->push_back(newType);
   }

void
OMR::JitBuilder::TypeGraph::registerValidDirectFieldAccess(FieldType *fieldType, StructType *structBaseType)
   {
   registerType(fieldType);

   TypeNode *fieldTypeNode = lookupType(fieldType);
   TypeNode *structBaseTypeNode = lookupType(structBaseType);

   NodeList *loadList = lookupNodeList(aLoadField);
   TypeNode *producesTypeNode = lookupType(fieldType->type());
   OperationNode *newLoadType = new BinaryOperationNode(producesTypeNode, aLoadField, fieldTypeNode, structBaseTypeNode);
   loadList->push_back(newLoadType);

   NodeList *storeList = lookupNodeList(aStoreField);
   producesTypeNode = lookupType(_types->NoType);
   TypeNode *valueNode = lookupType(fieldType->type());
   OperationNode *newStoreType = new TrinaryOperationNode(producesTypeNode, aStoreField, fieldTypeNode, structBaseTypeNode, valueNode);
   storeList->push_back(newStoreType);
   }

void
OMR::JitBuilder::TypeGraph::registerValidIndirectFieldAccess(FieldType *fieldType, PointerType *pStructBaseType)
   {
   Type *baseType = pStructBaseType->BaseType();
   assert(baseType->isStruct());

   TypeNode *fieldTypeNode = lookupType(fieldType);
   TypeNode *pStructBaseTypeNode = lookupType(pStructBaseType);

   NodeList *loadList = lookupNodeList(aLoadIndirect);
   TypeNode *producesTypeNode = lookupType(fieldType->type());
   OperationNode *newLoadType = new BinaryOperationNode(producesTypeNode, aLoadIndirect, fieldTypeNode, pStructBaseTypeNode);
   loadList->push_back(newLoadType);

   NodeList *storeList = lookupNodeList(aStoreIndirect);
   producesTypeNode = lookupType(_types->NoType);
   TypeNode *valueNode = lookupType(fieldType->type());
   OperationNode *newStoreType = new TrinaryOperationNode(producesTypeNode, aStoreIndirect, fieldTypeNode, pStructBaseTypeNode, valueNode);
   storeList->push_back(newStoreType);
   }
