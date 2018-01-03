/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include <stdlib.h>

#include "il/DataTypes.hpp"
#include "il/SymbolReference.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/Compilation.hpp"
#include "env/FrontEnd.hpp"
#include "ilgen/TypeDictionaryImpl.hpp"
#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/STLUtils.hpp"


const uint8_t primitiveTypeAlignment[TR::NumOMRTypes] =
   {
   1,  // NoType
   1,  // Int8
   2,  // Int16
   4,  // Int32
   8,  // Int64
   4,  // Float
   8,  // Double
#if TR_TARGET_64BIT
   4,  // Address/Word
#else
   8,  // Address/Word
#endif
   16, // VectorInt8
   16, // VectorInt16
   16, // VectorInt32
   16, // VectorInt64
   16, // VectorFloat
   16  // VectorDouble
   };


void
OMR::StructType::AddField(const char *name, TR::IlTypeImpl *typeInfo, size_t offset)
   {
   if (_closed)
      return;

   TR_ASSERT(_size <= offset, "Offset of new struct field %s::%s is %d, which is less than the current size of the struct %d\n", _name, name, offset, _size);

   FieldInfo *fieldInfo = new (PERSISTENT_NEW) FieldInfo(name, offset, typeInfo);
   if (0 != _lastField)
      _lastField->setNext(fieldInfo);
   else
      _firstField = fieldInfo;
   _lastField = fieldInfo;
   _size = offset + typeInfo->getSize();
   }

void
OMR::StructType::AddField(const char *name, TR::IlTypeImpl *typeInfo)
   {
   if (_closed)
      return;

   TR::DataType primitiveType = typeInfo->getRealPrimitiveType();
   uint32_t align = primitiveTypeAlignment[primitiveType] - 1;
   _size = (_size + align) & (~align);

   FieldInfo *fieldInfo = new (PERSISTENT_NEW) FieldInfo(name, _size, typeInfo);
   if (0 != _lastField)
      _lastField->setNext(fieldInfo);
   else
      _firstField = fieldInfo;
   _lastField = fieldInfo;
   _size += typeInfo->getSize();
   } 

OMR::FieldInfo *
OMR::StructType::findField(const char *fieldName)
   {
   FieldInfo *info = _firstField;
   while (NULL != info)
      {
      if (strncmp(info->_name, fieldName, strlen(fieldName)) == 0)
         return info;
      info = info->_next;
      }
   return NULL;
   }

TR::IlTypeImpl *
OMR::StructType::getFieldType(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;
   return info->_type;
   }

size_t
OMR::StructType::getFieldOffset(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return -1;
   return info->getOffset();
   }

TR::IlReference *
OMR::StructType::getFieldSymRef(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;

   TR::SymbolReference *symRef = info->getSymRef();
   if (NULL == symRef)
      {
      TR::Compilation *comp = TR::comp();

      TR::DataType type = info->getPrimitiveType();

      char *fullName = (char *) comp->trMemory()->allocateHeapMemory((strlen(info->_name) + 1 + strlen(_name) + 1) * sizeof(char));
      sprintf(fullName, "%s.%s", _name, info->_name);
      TR::Symbol *symbol = TR::Symbol::createNamedShadow(comp->trHeapMemory(), type, info->_type->getSize(), fullName);

      // TBD: should we create a dynamic "constant" pool for accesses made by the method being compiled?
      symRef = new (comp->trHeapMemory()) TR::SymbolReference(comp->getSymRefTab(), symbol, comp->getMethodSymbol()->getResolvedMethodIndex(), -1);
      symRef->setOffset(info->getOffset());

      // conservative aliasing
      int32_t refNum = symRef->getReferenceNumber();
      if (type == TR::Address)
         comp->getSymRefTab()->aliasBuilder.addressShadowSymRefs().set(refNum);
      else if (type == TR::Int32)
         comp->getSymRefTab()->aliasBuilder.intShadowSymRefs().set(refNum);
      else
         comp->getSymRefTab()->aliasBuilder.nonIntPrimitiveShadowSymRefs().set(refNum);

      info->cacheSymRef(symRef);
      }

   return (TR::IlReference *)symRef;
   }

void
OMR::StructType::clearSymRefs()
   {
   FieldInfo *field = _firstField;
   while (field)
      {
      field->clearSymRef();
      field = field->_next;
      }
   }


void
OMR::UnionType::AddField(const char *name, TR::IlTypeImpl *typeInfo)
   {
   if (_closed)
      return;

   auto fieldSize = typeInfo->getSize();
   if (fieldSize > _size) _size = fieldSize;

   FieldInfo *fieldInfo = new (PERSISTENT_NEW) FieldInfo(name, 0 /* no offset */, typeInfo);
   if (0 != _lastField)
      _lastField->setNext(fieldInfo);
   else
      _firstField = fieldInfo;
   _lastField = fieldInfo;
   }

void
OMR::UnionType::Close()
   {
   _closed = true;
   }

OMR::FieldInfo *
OMR::UnionType::findField(const char *fieldName)
   {
   FieldInfo *info = _firstField;
   while (NULL != info)
      {
      if (strncmp(info->_name, fieldName, strlen(fieldName)) == 0)
         return info;
      info = info->_next;
      }
   return NULL;
   }

TR::IlTypeImpl *
OMR::UnionType::getFieldType(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;
   return info->_type;
   }

TR::IlReference *
OMR::UnionType::getFieldSymRef(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   TR_ASSERT(info, "Struct %s has no field with name %s\n", getName(), fieldName);

   TR::SymbolReference *symRef = info->getSymRef();
   if (NULL == symRef)
      {
      // create a symref for the new field and set its bitvector
      TR::Compilation *comp = TR::comp();
      auto symRefTab = comp->getSymRefTab();
      TR::DataType type = info->getPrimitiveType();

      char *fullName = (char *) comp->trMemory()->allocateHeapMemory((strlen(info->_name) + 1 + strlen(_name) + 1) * sizeof(char));
      sprintf(fullName, "%s.%s", _name, info->_name);
      TR::Symbol *symbol = TR::Symbol::createNamedShadow(comp->trHeapMemory(), type, info->_type->getSize(), fullName);
      symRef = new (comp->trHeapMemory()) TR::SymbolReference(symRefTab, symbol, comp->getMethodSymbol()->getResolvedMethodIndex(), -1);
      symRef->setOffset(0);
      symRef->setReallySharesSymbol();

      TR_SymRefIterator sit(_symRefBV, symRefTab);
      for (TR::SymbolReference *sr = sit.getNext(); sr; sr = sit.getNext())
          {
          symRefTab->makeSharedAliases(symRef, sr);
          }

      _symRefBV.set(symRef->getReferenceNumber());

      info->cacheSymRef(symRef);
      }

   return static_cast<TR::IlReference *>(symRef);
   }

void
OMR::UnionType::clearSymRefs()
   {
   FieldInfo *field = _firstField;
   while (field)
      {
      field->clearSymRef();
      field = field->_next;
      }
   _symRefBV.init(4, _trMemory);
   }


TR::TypeDictionaryImpl *
OMR::TypeDictionaryImpl::allocate(TR::TypeDictionary *client)
   {
   return new (PERSISTENT_NEW) TR::TypeDictionaryImpl(client);
   }

OMR::TypeDictionaryImpl::TypeDictionaryImpl(TR::TypeDictionary *client) :

   // Note: _memoryRegion and the corresponding TR::SegmentProvider and TR::Memory instances are stored as pointers within TypeDictionaryImpl
   // in order to avoid increasing the number of header files needed to compile against the JitBuilder library. Because we are storing
   // them as pointers, we cannot rely on the default C++ destruction semantic to destruct and deallocate the memory region, but rather
   // have to do it explicitly in the TypeDictionaryImpl destructor. And since C++ destroys the other members *after* executing the user defined
   // destructor, we need to make sure that any members (and their contents) that are allocated in _memoryRegion are explicitly destroyed
   // and deallocated *before* _memoryRegion in the TypeDictionaryImpl destructor.
   _segmentProvider( new(TR::Compiler->persistentAllocator()) TR::SystemSegmentProvider(1 << 16, TR::Compiler->rawAllocator) ),
   _memoryRegion( new(TR::Compiler->persistentAllocator()) TR::Region(*_segmentProvider, TR::Compiler->rawAllocator) ),
   _client(client),
   _trMemory( new(TR::Compiler->persistentAllocator()) TR_Memory(*::trPersistentMemory, *_memoryRegion) ),
   _structsByName(str_comparator, _trMemory->heapMemoryRegion()),
   _unionsByName(str_comparator, _trMemory->heapMemoryRegion())
   {
   // primitive types
   NoType       = _primitiveType[TR::NoType]                = new (PERSISTENT_NEW) OMR::PrimitiveType("NoType", TR::NoType, client);
   Int8         = _primitiveType[TR::Int8]                  = new (PERSISTENT_NEW) OMR::PrimitiveType("Int8", TR::Int8, client);
   Int16        = _primitiveType[TR::Int16]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Int16", TR::Int16, client);
   Int32        = _primitiveType[TR::Int32]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Int32", TR::Int32, client);
   Int64        = _primitiveType[TR::Int64]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Int64", TR::Int64, client);
   Float        = _primitiveType[TR::Float]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Float", TR::Float, client);
   Double       = _primitiveType[TR::Double]                = new (PERSISTENT_NEW) OMR::PrimitiveType("Double", TR::Double, client);
   Address      = _primitiveType[TR::Address]               = new (PERSISTENT_NEW) OMR::PrimitiveType("Address", TR::Address, client);
   VectorInt8   = _primitiveType[TR::VectorInt8]            = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt8", TR::VectorInt8, client);
   VectorInt16  = _primitiveType[TR::VectorInt16]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt16", TR::VectorInt16, client);
   VectorInt32  = _primitiveType[TR::VectorInt32]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt32", TR::VectorInt32, client);
   VectorInt64  = _primitiveType[TR::VectorInt64]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt64", TR::VectorInt64, client);
   VectorFloat  = _primitiveType[TR::VectorFloat]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorFloat", TR::VectorFloat, client);
   VectorDouble = _primitiveType[TR::VectorDouble]          = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorDouble", TR::VectorDouble, client);

   // pointer to primitive types
   pNoType       = _pointerToPrimitiveType[TR::NoType]       = new (PERSISTENT_NEW) OMR::PointerType(NoType);
   pInt8         = _pointerToPrimitiveType[TR::Int8]         = new (PERSISTENT_NEW) OMR::PointerType(Int8);
   pInt16        = _pointerToPrimitiveType[TR::Int16]        = new (PERSISTENT_NEW) OMR::PointerType(Int16);
   pInt32        = _pointerToPrimitiveType[TR::Int32]        = new (PERSISTENT_NEW) OMR::PointerType(Int32);
   pInt64        = _pointerToPrimitiveType[TR::Int64]        = new (PERSISTENT_NEW) OMR::PointerType(Int64);
   pFloat        = _pointerToPrimitiveType[TR::Float]        = new (PERSISTENT_NEW) OMR::PointerType(Float);
   pDouble       = _pointerToPrimitiveType[TR::Double]       = new (PERSISTENT_NEW) OMR::PointerType(Double);
   pAddress      = _pointerToPrimitiveType[TR::Address]      = new (PERSISTENT_NEW) OMR::PointerType(Address);
   pVectorInt8   = _pointerToPrimitiveType[TR::VectorInt8]   = new (PERSISTENT_NEW) OMR::PointerType(VectorInt8);
   pVectorInt16  = _pointerToPrimitiveType[TR::VectorInt16]  = new (PERSISTENT_NEW) OMR::PointerType(VectorInt16);
   pVectorInt32  = _pointerToPrimitiveType[TR::VectorInt32]  = new (PERSISTENT_NEW) OMR::PointerType(VectorInt32);
   pVectorInt64  = _pointerToPrimitiveType[TR::VectorInt64]  = new (PERSISTENT_NEW) OMR::PointerType(VectorInt64);
   pVectorFloat  = _pointerToPrimitiveType[TR::VectorFloat]  = new (PERSISTENT_NEW) OMR::PointerType(VectorFloat);
   pVectorDouble = _pointerToPrimitiveType[TR::VectorDouble] = new (PERSISTENT_NEW) OMR::PointerType(VectorDouble);

   if (TR::Compiler->target.is64Bit())
      {
      Word =  _primitiveType[TR::Int64]; 
      pWord =  _pointerToPrimitiveType[TR::Int64];
      }
   else
      {
      Word =  _primitiveType[TR::Int32];
      pWord =  _pointerToPrimitiveType[TR::Int32];
      }
   }

OMR::TypeDictionaryImpl::~TypeDictionaryImpl() throw()
   {
   // Cleanup allocations in _memoryRegion *before* its destroyed below (see note in constructor)
   _structsByName.clear();
   _unionsByName.clear();

   _trMemory->~TR_Memory();
   ::operator delete(_trMemory, TR::Compiler->persistentAllocator());
   _memoryRegion->~Region();
   ::operator delete(_memoryRegion, TR::Compiler->persistentAllocator());
   static_cast<TR::SystemSegmentProvider *>(_segmentProvider)->~SystemSegmentProvider();
   ::operator delete(_segmentProvider, TR::Compiler->persistentAllocator());
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::LookupStruct(const char *structName)
   {
   return getStruct(structName);
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::LookupUnion(const char *unionName)
   {
   return getUnion(unionName);
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::DefineStruct(const char *structName)
   {
   TR_ASSERT_FATAL(_structsByName.find(structName) == _structsByName.end(), "Struct '%s' already exists", structName);

   StructType *newType = new (PERSISTENT_NEW) StructType(structName, _client);
   _structsByName.insert(std::make_pair(structName, newType));

   return newType;
   }

void
OMR::TypeDictionaryImpl::DefineField(const char *structName, const char *fieldName, TR::IlTypeImpl *type, size_t offset)
   {
   getStruct(structName)->AddField(fieldName, type, offset);
   }

void
OMR::TypeDictionaryImpl::DefineField(const char *structName, const char *fieldName, TR::IlTypeImpl *type)
   {
   getStruct(structName)->AddField(fieldName, type);
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::GetFieldType(const char *structName, const char *fieldName)
   {
   return getStruct(structName)->getFieldType(fieldName);
   }

size_t
OMR::TypeDictionaryImpl::OffsetOf(const char *structName, const char *fieldName)
   {
   return getStruct(structName)->getFieldOffset(fieldName);
   }

void
OMR::TypeDictionaryImpl::CloseStruct(const char *structName, size_t finalSize)
   {
   getStruct(structName)->Close(finalSize);
   }

void
OMR::TypeDictionaryImpl::CloseStruct(const char *structName)
   {
   getStruct(structName)->Close();
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::DefineUnion(const char *unionName)
   {
   TR_ASSERT_FATAL(_unionsByName.find(unionName) == _unionsByName.end(), "Union '%s' already exists", unionName);
   
   UnionType *newType = new (PERSISTENT_NEW) UnionType(unionName, _client, _trMemory);
   _unionsByName.insert(std::make_pair(unionName, newType));

   return newType;
   }

void
OMR::TypeDictionaryImpl::UnionField(const char *unionName, const char *fieldName, TR::IlTypeImpl *type)
   {
   getUnion(unionName)->AddField(fieldName, type);
   }

void
OMR::TypeDictionaryImpl::CloseUnion(const char *unionName)
   {
   getUnion(unionName)->Close();
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::UnionFieldType(const char *unionName, const char *fieldName)
   {
   return getUnion(unionName)->getFieldType(fieldName);
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::PointerTo(const char *structName)
   {
   return PointerTo(LookupStruct(structName));
   }

TR::IlTypeImpl *
OMR::TypeDictionaryImpl::PointerTo(TR::IlTypeImpl *baseType)
   {
   return new (PERSISTENT_NEW) PointerType(baseType);
   }

TR::IlReference *
OMR::TypeDictionaryImpl::FieldReference(const char *typeName, const char *fieldName)
   {
   StructMap::iterator structIterator = _structsByName.find(typeName);
   if (structIterator != _structsByName.end())
      {
      StructType *theStruct = structIterator->second;
      return theStruct->getFieldSymRef(fieldName);
      }

   UnionMap::iterator unionIterator = _unionsByName.find(typeName);
   if (unionIterator != _unionsByName.end())
      {
      UnionType *theUnion = unionIterator->second;
      return theUnion->getFieldSymRef(fieldName);
      }

   TR_ASSERT_FATAL(false, "No type with name '%s'", typeName);
   return NULL;
   }

void
OMR::TypeDictionaryImpl::notifyCompilationDone()
   {
   // clear all symbol references for fields
   for (StructMap::iterator it = _structsByName.begin(); it != _structsByName.end(); it++)
      {
      StructType *aStruct = it->second;
      aStruct->clearSymRefs();
      }

   // clear all symbol references for union fields
   for (UnionMap::iterator it = _unionsByName.begin(); it != _unionsByName.end(); it++)
      {
      UnionType *aUnion = it->second;
      aUnion->clearSymRefs();
      }
   }

OMR::StructType *
OMR::TypeDictionaryImpl::getStruct(const char *structName)
   {
   StructMap::iterator it = _structsByName.find(structName);
   TR_ASSERT_FATAL(it != _structsByName.end(), "No struct named '%s'", structName);

   StructType *theStruct = it->second;
   return theStruct;
   }

OMR::UnionType *
OMR::TypeDictionaryImpl::getUnion(const char *unionName)
   {
   UnionMap::iterator it = _unionsByName.find(unionName);
   TR_ASSERT_FATAL(it != _unionsByName.end(), "No union named '%s'", unionName);

   UnionType *theUnion = it->second;
   return theUnion;
   }
