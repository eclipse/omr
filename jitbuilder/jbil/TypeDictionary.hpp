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

#ifndef TYPEDICTIONARY_INCL
#define TYPEDICTIONARY_INCL


#include <stdint.h>
#include <map>
#include <vector>
#include "Action.hpp"
#include "Iterator.hpp"
#include "Operation.hpp"
#include "Type.hpp"

namespace OMR
{

namespace JitBuilder
{

class DebugDictionary;
class DynamicType;
class OperationBuilder;
class TypeGraph;

class TypeDictionary
   {
   friend class DebugDictionary;
   friend class DynamicType;
   friend class OperationBuilder;

public:
   TypeDictionary();
   TypeDictionary(std::string name);
   TypeDictionary(TypeGraph * graph);
   TypeDictionary(std::string name, TypeGraph * graph);

   static TypeDictionary *global();

   Type * NoType;
   Type * Int8;
   Type * Int16;
   Type * Int32;
   Type * Int64;
   Type * Float;
   Type * Double;
   Type * Address;
   Type * Word;

   // new types
   // BEGIN {
   //

   //
   // } END
   // new public types

   int64_t NewTypeID() { return _maxTypeID++; }
   int64_t MaxTypeID() const { return _maxTypeID; }

   PointerType * PointerTo(Type * baseType);
   StructType * DefineStruct(std::string structName, size_t size);
   UnionType * DefineUnion(std::string unionName);
   FieldType * DefineField(std::string structName, std::string fieldName, Type * fieldType, size_t offset)
      { // deprecated
      return DefineField(LookupStruct(structName), LiteralValue::create(this, fieldName), fieldType, offset);
      }
   FieldType * DefineField(StructType *structType, std::string fieldName, Type * fieldType, size_t offset)
      {
      return DefineField(structType, LiteralValue::create(this, fieldName), fieldType, offset);
      }
   FieldType * DefineField(StructType *structType, LiteralValue *fieldName, Type * fieldType, size_t offset);
   void CloseStruct(std::string structName)
      { // deprecated
      CloseStruct(LookupStruct(structName));
      }
   void CloseStruct(Type *structType);
   void CloseUnion(std::string unionName)
      { // deprecated
      CloseUnion(static_cast<UnionType *>(LookupStruct(unionName)));
      }
   void CloseUnion(Type *unionType);
   FunctionType *DefineFunctionType(std::string name, Type *returnType, int32_t numParms, Type **parmTypes);

   TypeIterator TypesBegin() const
      {
      return TypeIterator(_types);
      }
   TypeIterator TypesEnd() const   { return TypeIterator(); }

   StructType *LookupStruct(std::string name);
   Type *LookupType(uint64_t id);
   void RemoveType(Type *type);

   int64_t id() const       { return _id; }
   std::string name() const { return _name; }
   bool hasLinkedDictionary() const { return _linkedDictionary != NULL; }
   TypeDictionary *linkedDictionary() { return _linkedDictionary; }

   void registerReturnType(Type *type);

   // get the Type produced by Action a on Value v
   Type * producedType(Action a, Value *v);

   // get the Type produced by Action a on Values left and right
   Type * producedType(Action a, Value *left, Value *right);

   // get the Type produced by Action a on Values one, two, and three
   Type * producedType(Action a, Value *one, Value *two, Value *three);

   // get the Type produced by Action a with struct/union and field and struct base pointer value and (optionally) value
   Type * producedType(Action a, FieldType *fieldType, Value *structBase, Value *value=NULL);

   // get the Type produced for a set of argument types passed to a FunctionType
   Type * producedType(FunctionType *type, int32_t numArgs, va_list args);
   Type * producedType(FunctionType *type, int32_t numArgs, Value **args);

   // new public API
   // BEGIN {
   //

   //
   // } END
   // new public API

protected:
   TypeDictionary(std::string name, TypeDictionary *linkedTypes);

   void createPrimitiveTypes();
   void addType(Type *type)
      {
      _types.push_back(type);
      }
   void initializeGraph();
   void registerPointerType(PointerType * pointerType);
   void forgetType(Type *type);
   void registerDynamicOperation(OperationBuilder * dynamicOperationBuilder);
   void registerDynamicType(DynamicType * dynamicType);

   int64_t                              _id;
   std::string                          _name;
   std::vector<Type *>                  _types;
   int64_t                              _maxTypeID;
   std::map<Type *,PointerType*>        _pointerTypeFromBaseType;
   std::map<std::string,StructType *>   _structTypeFromName;
   std::map<std::string,FunctionType *> _functionTypeFromName;
   TypeGraph                          * _graph;
   TypeDictionary                     * _linkedDictionary;

   static int64_t globalIndex;
   static TypeDictionary *globalDict;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(TYPEDICTIONARY_INCL)
