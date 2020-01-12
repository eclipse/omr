/*
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

#ifndef TYPEGRAPH_INCL
#define TYPEGRAPH_INCL

#include <map>
#include <list>
#include <string>

#include "Action.hpp"

namespace OMR
{

namespace JitBuilder
{

class FieldType;
class FunctionType;
class PointerType;
class StructType;
class Type;
class TypeDictionary;
class TypeSignature;
class Value;

class TypeGraph
   {
public:
   TypeGraph(TypeDictionary * types);
   TypeGraph(TypeDictionary * types, TypeGraph *linkedGraph);

   int64_t id() const { return _id; }

   // create a node representing the given type
   void registerType(Type * type);

   // create a node representing the given function type
   void registerFunctionType(FunctionType * function);

   // get the Type produced by Action a on Value v
   Type * producedType(Action a, Type *t);

   // get the Type produced by Action a on Values left and right
   Type * producedType(Action a, Type *left, Type *right);

   // get the Type produced by Action a on Values one, two, and three
   Type * producedType(Action a, Type *one, Type *two, Type *three);

   // get the Type produced by Action a calling FunctionType type provided a varargs set of argument types
   Type * producedType(FunctionType *type, int32_t numArgs, Type ** argTypes);

   void registerValidOperation(Type * produces, Action a, Type * operand);
   void registerValidOperation(Type * produces, Action a, Type * left, Type * right);
   void registerValidOperation(Type * produces, Action a, Type * one, Type * two, Type * three);
   void registerValidDirectFieldAccess(FieldType *fieldType, StructType *structBaseType);
   void registerValidIndirectFieldAccess(FieldType *fieldType, PointerType *pStructBaseType);

protected:
   enum NodeKind
      {
      TypeKind=0,
      StringKind,
      SignatureOperationKind,
      UnaryOperationKind,
      BinaryOperationKind,
      TrinaryOperationKind,
      FieldAccessOperationKind,
      FunctionOperationKind,
      ExtensionKind
      };

   struct Node
      {
      Node(NodeKind k) : kind(k) { }
      NodeKind kind;
      };

   struct TypeNode : public Node
      {
      TypeNode(Type * t) : Node(TypeKind), type(t) { }
      Type * type;
      bool operator==(TypeNode & other) { return &other==this; }
      };

   struct OperationNode : public Node
      {
      OperationNode(NodeKind k, TypeNode * p, Action a)
         : Node(k)
         , produces(p)
         , action(a)
         , sig(NULL)
         , id(globalID++)
         { }
      OperationNode(Action a, TypeSignature *sig) // takes ownership of "sig" object
         : Node(SignatureOperationKind)
         , produces(NULL)
         , action(a)
         , sig(sig)
         , id(globalID++)
         { }
      TypeNode * produces;
      Action action;
      TypeSignature *sig;

      static uint64_t globalID;
      uint64_t id;
      };

   typedef std::list<OperationNode*> NodeList;

   struct UnaryOperationNode : public OperationNode
      {
      UnaryOperationNode(TypeNode * produces, Action a, TypeNode * o)
         : OperationNode(UnaryOperationKind, produces, a)
         , operand(o)
         { }
      TypeNode * operand;
      };

   struct BinaryOperationNode : public OperationNode
      {
      BinaryOperationNode(TypeNode * produces, Action a, TypeNode * one, TypeNode * two)
         : OperationNode(BinaryOperationKind, produces, a)
         , left(one)
         , right(two)
         { }
      TypeNode * left;
      TypeNode * right;
      };

   struct TrinaryOperationNode : public OperationNode
      {
      TrinaryOperationNode(TypeNode * produces, Action a, TypeNode * one, TypeNode * two, TypeNode * three)
         : OperationNode(TrinaryOperationKind, produces, a)
         , first(one)
         , second(two)
         , third(three)
         { }
      TypeNode * first;
      TypeNode * second;
      TypeNode * third;
      };

   struct FunctionOperationNode : public OperationNode
      {
      FunctionOperationNode(TypeNode * produces, Action a, TypeNode *func, int32_t totalParms, TypeNode ** parms)
         : OperationNode(FunctionOperationKind, produces, a)
         , function(func)
         , numParms(totalParms)
         , parmTypes(parms)
         { }
      TypeNode  * function;
      int32_t     numParms;
      TypeNode ** parmTypes;
      };

   TypeNode * lookupType(Type * t);
   NodeList * lookupNodeList(Action a);

   uint64_t                     _id;
   std::map<Type *,TypeNode *>  _typeToNode;
   std::map<Action, NodeList *> _nodesForAction;
   TypeDictionary             * _types;
   TypeGraph                  * _linkedGraph;

   static uint64_t globalIndex;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(TYPEGRAPH_INCL)
