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

#include <string>
#include "Operation.hpp"
#include "Type.hpp"
#include "TypeGraph.hpp"
#include "TypeDictionary.hpp"
#include "Value.hpp"

using namespace OMR::JitBuilder;

int64_t OMR::JitBuilder::TypeDictionary::globalIndex = 0;

// Type * OMR::JitBuilder::TypeDictionary::NoType  = Type::NoType;
// Type * OMR::JitBuilder::TypeDictionary::Int8    = Type::Int8;
// Type * OMR::JitBuilder::TypeDictionary::Int16   = Type::Int16;
// Type * OMR::JitBuilder::TypeDictionary::Int32   = Type::Int32;
// Type * OMR::JitBuilder::TypeDictionary::Int64   = Type::Int64;
// Type * OMR::JitBuilder::TypeDictionary::Float   = Type::Float;
// Type * OMR::JitBuilder::TypeDictionary::Double  = Type::Double;
// Type * OMR::JitBuilder::TypeDictionary::Address = Type::Address;

// Should really be either Int64 or Int32 based on Config
// Type * OMR::JitBuilder::TypeDictionary::Word = OMR::JitBuilder::TypeDictionary::Int64;

//
// User types
// BEGIN {

// } END
// User types
// 

TypeDictionary *TypeDictionary::globalDict = NULL;

TypeDictionary *
TypeDictionary::global()
   {
   if (globalDict == NULL)
      globalDict = new TypeDictionary("GlobalTD");

   return globalDict;
   }

TypeDictionary::TypeDictionary()
   : _id(globalIndex++)
   , _name("")
   , _maxTypeID(1)
   , _graph(new TypeGraph(this))
   , _linkedDictionary(NULL)
   {
   createPrimitiveTypes();
   initializeGraph();
   }

TypeDictionary::TypeDictionary(std::string name)
   : _id(globalIndex++)
   , _name(name)
   , _maxTypeID(1)
   , _graph(new TypeGraph(this))
   , _linkedDictionary(NULL)
   {
   createPrimitiveTypes();
   initializeGraph();
   }

TypeDictionary::TypeDictionary(TypeGraph * graph)
   : _id(globalIndex++)
   , _name("")
   , _maxTypeID(1)
   , _graph(graph)
   , _linkedDictionary(NULL)
   {
   createPrimitiveTypes();
   initializeGraph();
   }

TypeDictionary::TypeDictionary(std::string name, TypeGraph * graph)
   : _id(globalIndex++)
   , _name("")
   , _maxTypeID(1)
   , _graph(graph)
   , _linkedDictionary(NULL)
   {
   createPrimitiveTypes();
   initializeGraph();
   }

// Only accessible to subclasses
TypeDictionary::TypeDictionary(std::string name, TypeDictionary * linkedTypes)
   : _id(globalIndex++)
   , _name(name)
   , _maxTypeID(linkedTypes->MaxTypeID())
   , _graph(new TypeGraph(this, linkedTypes->_graph))
   , _linkedDictionary(linkedTypes)
   {
   for (TypeIterator typeIt = linkedTypes->TypesBegin(); typeIt != linkedTypes->TypesEnd(); typeIt++)
      {
      Type *type = *typeIt;
      addType(type);
      }
   _maxTypeID = linkedTypes->_maxTypeID;
   NoType = linkedTypes->NoType;
   Int8 = linkedTypes->Int8;
   Int16 = linkedTypes->Int16;
   Int32 = linkedTypes->Int32;
   Int64 = linkedTypes->Int64;
   Float = linkedTypes->Float;
   Double = linkedTypes->Double;
   Address = linkedTypes->Address;
   Word = linkedTypes->Word;

   //
   // User type copying
   // BEGIN {

   // } END
   // User type copying
   //

   _pointerTypeFromBaseType = linkedTypes->_pointerTypeFromBaseType;
   _structTypeFromName = linkedTypes->_structTypeFromName;
   _functionTypeFromName = linkedTypes->_functionTypeFromName;

   // TODO: copy entire linked type graph?
   //initializeGraph();
   }

void
TypeDictionary::createPrimitiveTypes()
   {
   NoType = NoTypeType::create(this);
   addType(NoType);

   Int8 = Int8Type::create(this);
   addType(Int8);

   Int16 = Int16Type::create(this);
   addType(Int16);

   Int32 = Int32Type::create(this);
   addType(Int32);

   Int64 = Int64Type::create(this);
   addType(Int64);

   Float = FloatType::create(this);
   addType(Float);

   Double = DoubleType::create(this);
   addType(Double);

   Address = PointerType::create(this, "Address", Int8);
   addType(Address);

   Word = Int64; // should really be changed based on Config

   //
   // User type handling
   // BEGIN {

   // } END
   // User type handling
   //

   }

void
TypeDictionary::initializeGraph()
   {
   _graph->registerType(NoType);
   _graph->registerType(Int8);
   _graph->registerType(Int16);
   _graph->registerType(Int32);
   _graph->registerType(Int64);
   _graph->registerType(Float);
   _graph->registerType(Double);
   _graph->registerType(Address);

   //
   // User initialization
   // BEGIN {

   // } END
   // User initialization
   // 

   Add::initializeTypeProductions(this, _graph);
   Sub::initializeTypeProductions(this, _graph);
   Mul::initializeTypeProductions(this, _graph);

   ForLoop::initializeTypeProductions(this, _graph);
   IfCmpGreaterOrEqual::initializeTypeProductions(this, _graph);
   IfCmpGreaterThan::initializeTypeProductions(this, _graph);
   IfCmpLessOrEqual::initializeTypeProductions(this, _graph);
   IfCmpLessThan::initializeTypeProductions(this, _graph);
   IfThenElse::initializeTypeProductions(this, _graph);
   Return::initializeTypeProductions(this, _graph);
   Switch::initializeTypeProductions(this, _graph);
   }

void
OMR::JitBuilder::TypeDictionary::registerReturnType(Type *type)
   {
   _graph->registerValidOperation(type, aReturn, type);
   }

void
OMR::JitBuilder::TypeDictionary::registerPointerType(PointerType * pointerType)
   {
   _graph->registerType(pointerType);

   Type * baseType = pointerType->BaseType();
   _graph->registerValidOperation(pointerType, aAdd, pointerType, Word);
   _graph->registerValidOperation(Word, aSub, pointerType, pointerType);

   _graph->registerValidOperation(pointerType, aIndexAt, pointerType, Word);
   if (Word != Int32)
      _graph->registerValidOperation(pointerType, aIndexAt, pointerType, Int32);

   _graph->registerValidOperation(baseType, aLoadAt, pointerType);
   _graph->registerValidOperation(NoType, aStoreAt, pointerType, baseType);
   }

Type *
OMR::JitBuilder::TypeDictionary::producedType(Action a, Value *v)
   {
   return _graph->producedType(a, v->type());
   }

Type *
OMR::JitBuilder::TypeDictionary::producedType(Action a, Value *left, Value *right)
   {
   return _graph->producedType(a, left->type(), right->type());
   }

Type *
OMR::JitBuilder::TypeDictionary::producedType(Action a, Value *one, Value *two, Value *three)
   {
   return _graph->producedType(a, one->type(), two->type(), three->type());
   }

Type *
OMR::JitBuilder::TypeDictionary::producedType(Action a, FieldType *fieldType, Value *structBase, Value *value)
   {
   if (value)
      return _graph->producedType(a, fieldType, structBase->type(), value->type());
   else
      return _graph->producedType(a, fieldType, structBase->type());
   }

Type *
OMR::JitBuilder::TypeDictionary::producedType(FunctionType *type, int32_t numArgs, va_list args)
   {
   Type *argTypes[numArgs];
   for (int32_t a=0;a < numArgs;a++)
      argTypes[a] = (va_arg(args, Value *))->type();
   return _graph->producedType(type, numArgs, argTypes);
   }

Type *
OMR::JitBuilder::TypeDictionary::producedType(FunctionType *type, int32_t numArgs, Value **args)
   {
   Type *argTypes[numArgs];
   for (int32_t a=0;a < numArgs;a++)
      argTypes[a] = args[a]->type();
   return _graph->producedType(type, numArgs, argTypes);
   }

PointerType *
TypeDictionary::PointerTo(Type * baseType)
   {
   // don't replicate types
   std::map<Type *,PointerType *>::iterator found = _pointerTypeFromBaseType.find(baseType);
   if (found != _pointerTypeFromBaseType.end())
      {
      PointerType *t = found->second;
      return t;
      }

   // if not found already, then create it
   PointerType * newType = PointerType::create(this, std::string("PointerTo(") + baseType->name()  + std::string(")"), baseType);
   addType(newType);
   _pointerTypeFromBaseType[baseType] = newType;
   registerPointerType(newType);
   return newType;
   }

Type *
TypeDictionary::LookupType(uint64_t id)
   {
   for (auto it = TypesBegin(); it != TypesEnd(); it++)
      {
      Type *type = *it;
      if (type->id() == id)
         return type;
      }

   return NULL;
   }

void
TypeDictionary::forgetType(Type *type)
   {
   // brutal performance; should really collect these and do in one pass
   for (auto it = _types.begin(); it != _types.end(); )
      {
      if (*it == type)
         it = _types.erase(it);
      else
         ++it;
      }
   }

void
TypeDictionary::RemoveType(Type *type)
   {

   if (type->isField())
      ; // nothing special for fields themselves
   else if (type->isStruct() || type->isUnion())
      {
      StructType *sType = static_cast<StructType *>(type);
      bool fullyRemove = (type->owningDictionary() == this);
      for (auto it = sType->FieldsBegin(); it != sType->FieldsEnd(); )
         {
         FieldType *fType = it->second;
         forgetType(fType);

         if (fullyRemove)
            it = sType->RemoveField(it);
         else
            it++;
         }

      _structTypeFromName.erase(type->name());
      }
   else if (type->isPointer())
      {
      PointerType *ptrType = static_cast<PointerType *>(type);
      _pointerTypeFromBaseType.erase(ptrType->BaseType());
      }
   else if (type->isFunction())
      _functionTypeFromName.erase(type->name());

   forgetType(type);

   // TODO but not *strictly* necessary: _graph->unregister(type);
   }

StructType *
TypeDictionary::LookupStruct(std::string structName)
   {
   std::map<std::string,StructType *>::iterator found = _structTypeFromName.find(structName);
   if (found != _structTypeFromName.end())
      return found->second;

   return NULL;
   }

StructType *
TypeDictionary::DefineStruct(std::string structName, size_t size)
   {
   StructType *existingType = LookupStruct(structName);
   if (existingType)
      {
      assert(existingType->size() == size);
      return existingType;
      }

   StructType * newType = StructType::create(this, structName, size);
   addType(newType);
   _structTypeFromName.insert({structName, newType});
   _graph->registerType(newType);
   _graph->registerType(PointerTo(newType));
   return newType;
   }

UnionType *
TypeDictionary::DefineUnion(std::string unionName)
   {
   StructType *existingType = LookupStruct(unionName);
   if (existingType)
      {
      assert(existingType->isUnion());
      return static_cast<UnionType *>(existingType);
      }

   UnionType * newType = UnionType::create(this, unionName);
   addType(newType);
   _structTypeFromName.insert({unionName, newType});
   _graph->registerType(newType);
   _graph->registerType(PointerTo(newType));
   return newType;
   }

FieldType *
TypeDictionary::DefineField(StructType *structType, LiteralValue *fieldName, Type * fType, size_t offset)
   {
   assert(structType);
   if (structType->closed())
      {
      // TODO: useful error message
      return NULL;
      }

   assert(fieldName->kind() == T_string || fieldName->kind() == T_typename);
   FieldType *fieldType = structType->LookupField(fieldName->getString());
   if (fieldType)
      {
      assert(fieldType->type() == fType && fieldType->offset() == offset);
      return fieldType;
      }

   fieldType = structType->addField(fieldName, fType, offset);
   addType(fieldType);
   _graph->registerValidDirectFieldAccess(fieldType, structType);
   _graph->registerValidIndirectFieldAccess(fieldType, PointerTo(structType));
   return fieldType;
   }

void
TypeDictionary::CloseStruct(Type *type)
   {
   assert(type && type->isStruct());
   StructType *structType = static_cast<StructType *>(type);
   structType->setClosed();
   }

void
TypeDictionary::CloseUnion(Type *type)
   {
   assert(type && type->isUnion());
   UnionType *unionType = static_cast<UnionType *>(type);
   unionType->setClosed();
   }

FunctionType *
TypeDictionary::DefineFunctionType(std::string name,
                                   Type *returnType,
                                   int32_t numParms,
                                   Type **parmTypes)
   {
   // don't replicate types
   std::map<std::string,FunctionType *>::iterator found = _functionTypeFromName.find(name);
   if (found != _functionTypeFromName.end())
      {
      FunctionType *t = found->second;
      assert(t->returnType() == returnType && t->numParms() == numParms);
      for (int p=0;p < numParms;p++)
         assert(t->parmType(p) == parmTypes[p]);
      return t;
      }

   FunctionType * newType = FunctionType::create(this, name, returnType, numParms, parmTypes);
   addType(newType);
   _graph->registerFunctionType(newType);
   return newType;
   }
