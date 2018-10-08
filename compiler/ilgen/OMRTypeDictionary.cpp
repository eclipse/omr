/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <stdlib.h>

#include "il/DataTypes.hpp"
#include "il/SymbolReference.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/Compilation.hpp"
#include "env/FrontEnd.hpp"
#include "ilgen/IlReference.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/STLUtils.hpp"


namespace OMR
{

class PrimitiveType : public TR::IlType
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   PrimitiveType(const char * name, TR::DataType type) :
      TR::IlType(name),
      _type(type)
      { }
   virtual ~PrimitiveType()
      { }

   virtual TR::DataType getPrimitiveType()
      {
      return _type;
      }

   virtual char *getSignatureName() { return (char *) signatureNameForType[_type]; }

   virtual size_t getSize() { return TR::DataType::getSize(_type); }

protected:
   TR::DataType _type;
   };


class FieldInfo
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   FieldInfo(const char *name, size_t offset, TR::IlType *type) :
      _next(0),
      _name(name),
      _offset(offset),
      _type(type),
      _symRef(0)
      {
      }

   void cacheSymRef(TR::SymbolReference *symRef)    { _symRef = symRef; }
   TR::SymbolReference *getSymRef()                 { return _symRef; }
   void clearSymRef()                               { _symRef = NULL; }

   TR::IlType *getType()                            { return _type; }

   TR::IlType *primitiveType(TR::TypeDictionary *d) { return _type->primitiveType(d); }

   TR::DataType getPrimitiveType()                  { return _type->getPrimitiveType(); }

   size_t getOffset()                               { return _offset; }

   FieldInfo *getNext()                             { return _next; }
   void setNext(FieldInfo *next)                    { _next = next; }

//private:
   FieldInfo           * _next;
   const char          * _name;
   size_t                _offset;
   TR::IlType          * _type;
   TR::SymbolReference * _symRef;
   };


class StructType : public TR::IlType
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   StructType(const char *name) :
      TR::IlType(name),
      _firstField(0),
      _lastField(0),
      _size(0),
      _closed(false)
      { }
   virtual ~StructType()
      { }

   TR::IlType *primitiveType(TR::TypeDictionary * d) { return d->Address; }
   TR::DataType getPrimitiveType()                   { return TR::Address; }
   void Close(size_t finalSize)                      { TR_ASSERT(_size <= finalSize, "Final size %d of struct %s is less than its current size %d\n", finalSize, _name, _size); _size = finalSize; _closed = true; };
   void Close()                                      { _closed = true; };

   void AddField(const char *name, TR::IlType *fieldType, size_t offset);
   void AddField(const char *name, TR::IlType *fieldType);
   TR::IlType * getFieldType(const char *fieldName);
   size_t getFieldOffset(const char *fieldName);

   TR::SymbolReference *getFieldSymRef(const char *name);
   bool isStruct() { return true; }
   virtual size_t getSize() { return _size; }

   void clearSymRefs();

protected:
   FieldInfo * findField(const char *fieldName);

   FieldInfo * _firstField;
   FieldInfo * _lastField;
   size_t      _size;
   bool        _closed;
   };

class UnionType : public TR::IlType
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   UnionType(const char *name, TR_Memory* trMemory) :
      TR::IlType(name),
      _firstField(0),
      _lastField(0),
      _size(0),
      _closed(false),
      _symRefBV(4, trMemory),
      _trMemory(trMemory)
      { }
   virtual ~UnionType()
      { }

   TR::IlType *primitiveType(TR::TypeDictionary * d) { return d->Address; }
   TR::DataType getPrimitiveType()                   { return TR::Address; }
   void Close();

   void AddField(const char *name, TR::IlType *fieldType);
   TR::IlType * getFieldType(const char *fieldName);

   TR::SymbolReference *getFieldSymRef(const char *name);
   virtual bool isUnion() { return true; }
   virtual size_t getSize() { return _size; }

   void clearSymRefs();

protected:
   FieldInfo *  findField(const char *fieldName);

   FieldInfo *  _firstField;
   FieldInfo *  _lastField;
   size_t       _size;
   bool         _closed;
   TR_BitVector _symRefBV;
   TR_Memory*   _trMemory;
   };

class PointerType : public TR::IlType
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   PointerType(TR::IlType *baseType) :
      TR::IlType(_nameArray),
      _baseType(baseType)
      {
      char *baseName = (char *)_baseType->getName();
      TR_ASSERT(strlen(baseName) < 45, "cannot store name of pointer type");
      sprintf(_nameArray, "L%s;", baseName);
      }
   virtual bool isPointer() { return true; }

   virtual TR::IlType *baseType() { return _baseType; }

   virtual const char *getName() { return _name; }

   virtual TR::IlType *primitiveType(TR::TypeDictionary * d) { return d->Address; }
   virtual TR::DataType getPrimitiveType()                   { return TR::Address; }

   virtual size_t getSize() { return TR::DataType::getSize(TR::Address); }

protected:
   TR::IlType          * _baseType;
   char                  _nameArray[48];
   };

} //namespace OMR


void
OMR::StructType::AddField(const char *name, TR::IlType *typeInfo, size_t offset)
   {
   if (_closed)
      return;

   TR_ASSERT(_size <= offset, "Offset of new struct field %s::%s is %d, which is less than the current size of the struct %d\n", _name, name, offset, _size);

   OMR::FieldInfo *fieldInfo = new (PERSISTENT_NEW) OMR::FieldInfo(name, offset, typeInfo);
   if (0 != _lastField)
      _lastField->setNext(fieldInfo);
   else
      _firstField = fieldInfo;
   _lastField = fieldInfo;
   _size = offset + typeInfo->getSize();
   }

void
OMR::StructType::AddField(const char *name, TR::IlType *typeInfo)
   {
   if (_closed)
      return;

   TR::DataType primitiveType = typeInfo->getPrimitiveType();
   uint32_t align = primitiveTypeAlignment[primitiveType] - 1;
   _size = (_size + align) & (~align);

   OMR::FieldInfo *fieldInfo = new (PERSISTENT_NEW) OMR::FieldInfo(name, _size, typeInfo);
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
   OMR::FieldInfo *info = _firstField;
   while (NULL != info)
      {
      if (strcmp(info->_name, fieldName) == 0)
         return info;
      info = info->_next;
      }
   return NULL;
   }

TR::IlType *
OMR::StructType::getFieldType(const char *fieldName)
   {
   OMR::FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;
   return info->_type;
   }

size_t
OMR::StructType::getFieldOffset(const char *fieldName)
   {
   OMR::FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return -1;
   return info->getOffset();
   }

TR::SymbolReference *
OMR::StructType::getFieldSymRef(const char *fieldName)
   {
   OMR::FieldInfo *info = findField(fieldName);
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

   return symRef;
   }

void
OMR::StructType::clearSymRefs()
   {
   OMR::FieldInfo *field = _firstField;
   while (field)
      {
      field->clearSymRef();
      field = field->_next;
      }
   }


void
OMR::UnionType::AddField(const char *name, TR::IlType *typeInfo)
   {
   if (_closed)
      return;

   auto fieldSize = typeInfo->getSize();
   if (fieldSize > _size) _size = fieldSize;

   OMR::FieldInfo *fieldInfo = new (PERSISTENT_NEW) OMR::FieldInfo(name, 0 /* no offset */, typeInfo);
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
   OMR::FieldInfo *info = _firstField;
   while (NULL != info)
      {
      if (strcmp(info->_name, fieldName) == 0)
         return info;
      info = info->_next;
      }
   return NULL;
   }

TR::IlType *
OMR::UnionType::getFieldType(const char *fieldName)
   {
   OMR::FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;
   return info->_type;
   }

TR::SymbolReference *
OMR::UnionType::getFieldSymRef(const char *fieldName)
   {
   OMR::FieldInfo *info = findField(fieldName);
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

   return symRef;
   }

void
OMR::UnionType::clearSymRefs()
   {
   OMR::FieldInfo *field = _firstField;
   while (field)
      {
      field->clearSymRef();
      field = field->_next;
      }
   _symRefBV.init(4, _trMemory);
   }


// Note: _memoryRegion and the corresponding TR::SegmentProvider and TR::Memory instances are stored as pointers within TypeDictionary
// in order to avoid increasing the number of header files needed to compile against the JitBuilder library. Because we are storing
// them as pointers, we cannot rely on the default C++ destruction semantic to destruct and deallocate the memory region, but rather
// have to do it explicitly in the TypeDictionary::MemoryManager destructor. And since C++ destroys the other members *after* executing
// the user defined destructor, we need to make sure that any members (and their contents) that are allocated in _memoryRegion are
// explicitly destroyed and deallocated *before* _memoryRegion in the TypeDictionary::MemoryManager destructor.
OMR::TypeDictionary::MemoryManager::MemoryManager() :
   _segmentProvider( new(TR::Compiler->persistentAllocator()) TR::SystemSegmentProvider(1 << 16, TR::Compiler->rawAllocator) ),
   _memoryRegion( new(TR::Compiler->persistentAllocator()) TR::Region(*_segmentProvider, TR::Compiler->rawAllocator) ),
   _trMemory( new(TR::Compiler->persistentAllocator()) TR_Memory(*::trPersistentMemory, *_memoryRegion) )
   {}

OMR::TypeDictionary::MemoryManager::~MemoryManager()
   {
   _trMemory->~TR_Memory();
   ::operator delete(_trMemory, TR::Compiler->persistentAllocator());
   _memoryRegion->~Region();
   ::operator delete(_memoryRegion, TR::Compiler->persistentAllocator());
   static_cast<TR::SystemSegmentProvider *>(_segmentProvider)->~SystemSegmentProvider();
   ::operator delete(_segmentProvider, TR::Compiler->persistentAllocator());
   }

OMR::TypeDictionary::TypeDictionary() :
   _client(0),
   _structsByName(str_comparator, trMemory()->heapMemoryRegion()),
   _unionsByName(str_comparator, trMemory()->heapMemoryRegion())
   {
   // primitive types
   NoType       = _primitiveType[TR::NoType]                = new (PERSISTENT_NEW) OMR::PrimitiveType("NoType", TR::NoType);
   Int8         = _primitiveType[TR::Int8]                  = new (PERSISTENT_NEW) OMR::PrimitiveType("Int8", TR::Int8);
   Int16        = _primitiveType[TR::Int16]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Int16", TR::Int16);
   Int32        = _primitiveType[TR::Int32]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Int32", TR::Int32);
   Int64        = _primitiveType[TR::Int64]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Int64", TR::Int64);
   Float        = _primitiveType[TR::Float]                 = new (PERSISTENT_NEW) OMR::PrimitiveType("Float", TR::Float);
   Double       = _primitiveType[TR::Double]                = new (PERSISTENT_NEW) OMR::PrimitiveType("Double", TR::Double);
   Address      = _primitiveType[TR::Address]               = new (PERSISTENT_NEW) OMR::PrimitiveType("Address", TR::Address);
   VectorInt8   = _primitiveType[TR::VectorInt8]            = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt8", TR::VectorInt8);
   VectorInt16  = _primitiveType[TR::VectorInt16]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt16", TR::VectorInt16);
   VectorInt32  = _primitiveType[TR::VectorInt32]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt32", TR::VectorInt32);
   VectorInt64  = _primitiveType[TR::VectorInt64]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorInt64", TR::VectorInt64);
   VectorFloat  = _primitiveType[TR::VectorFloat]           = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorFloat", TR::VectorFloat);
   VectorDouble = _primitiveType[TR::VectorDouble]          = new (PERSISTENT_NEW) OMR::PrimitiveType("VectorDouble", TR::VectorDouble);

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

OMR::TypeDictionary::~TypeDictionary() throw()
   {
   // Cleanup allocations in _memoryRegion *before* its destroyed in
   // the TypeDictionary::MemoryManager destructor
   _structsByName.clear();
   _unionsByName.clear();
   }

TR::IlType *
OMR::TypeDictionary::LookupStruct(const char *structName)
   {
   return getStruct(structName);
   }

TR::IlType *
OMR::TypeDictionary::LookupUnion(const char *unionName)
   {
   return getUnion(unionName);
   }

TR::IlType *
OMR::TypeDictionary::DefineStruct(const char *structName)
   {
   TR_ASSERT_FATAL(_structsByName.find(structName) == _structsByName.end(), "Struct '%s' already exists", structName);

   OMR::StructType *newType = new (PERSISTENT_NEW) OMR::StructType(structName);
   _structsByName.insert(std::make_pair(structName, newType));

   return newType;
   }

void
OMR::TypeDictionary::DefineField(const char *structName, const char *fieldName, TR::IlType *type, size_t offset)
   {
   getStruct(structName)->AddField(fieldName, type, offset);
   }

void
OMR::TypeDictionary::DefineField(const char *structName, const char *fieldName, TR::IlType *type)
   {
   getStruct(structName)->AddField(fieldName, type);
   }

TR::IlType *
OMR::TypeDictionary::GetFieldType(const char *structName, const char *fieldName)
   {
   return getStruct(structName)->getFieldType(fieldName);
   }

size_t
OMR::TypeDictionary::OffsetOf(const char *structName, const char *fieldName)
   {
   return getStruct(structName)->getFieldOffset(fieldName);
   }

void
OMR::TypeDictionary::CloseStruct(const char *structName, size_t finalSize)
   {
   getStruct(structName)->Close(finalSize);
   }

void
OMR::TypeDictionary::CloseStruct(const char *structName)
   {
   getStruct(structName)->Close();
   }

TR::IlType *
OMR::TypeDictionary::DefineUnion(const char *unionName)
   {
   TR_ASSERT_FATAL(_unionsByName.find(unionName) == _unionsByName.end(), "Union '%s' already exists", unionName);
   
   OMR::UnionType *newType = new (PERSISTENT_NEW) OMR::UnionType(unionName, trMemory());
   _unionsByName.insert(std::make_pair(unionName, newType));

   return newType;
   }

void
OMR::TypeDictionary::UnionField(const char *unionName, const char *fieldName, TR::IlType *type)
   {
   getUnion(unionName)->AddField(fieldName, type);
   }

void
OMR::TypeDictionary::CloseUnion(const char *unionName)
   {
   getUnion(unionName)->Close();
   }

TR::IlType *
OMR::TypeDictionary::UnionFieldType(const char *unionName, const char *fieldName)
   {
   return getUnion(unionName)->getFieldType(fieldName);
   }

TR::IlType *
OMR::TypeDictionary::PointerTo(const char *structName)
   {
   return PointerTo(LookupStruct(structName));
   }

TR::IlType *
OMR::TypeDictionary::PointerTo(TR::IlType *baseType)
   {
   return new (PERSISTENT_NEW) OMR::PointerType(baseType);
   }

TR::IlReference *
OMR::TypeDictionary::FieldReference(const char *typeName, const char *fieldName)
   {
   StructMap::iterator structIterator = _structsByName.find(typeName);
   if (structIterator != _structsByName.end())
      {
      OMR::StructType *theStruct = structIterator->second;
      return new (PERSISTENT_NEW) TR::IlReference(theStruct->getFieldSymRef(fieldName));
      }

   UnionMap::iterator unionIterator = _unionsByName.find(typeName);
   if (unionIterator != _unionsByName.end())
      {
      OMR::UnionType *theUnion = unionIterator->second;
      return new (PERSISTENT_NEW) TR::IlReference(theUnion->getFieldSymRef(fieldName));
      }

   TR_ASSERT_FATAL(false, "No type with name '%s'", typeName);
   return NULL;
   }

void
OMR::TypeDictionary::NotifyCompilationDone()
   {
   // clear all symbol references for fields
   for (StructMap::iterator it = _structsByName.begin(); it != _structsByName.end(); it++)
      {
      OMR::StructType *aStruct = it->second;
      aStruct->clearSymRefs();
      }

   // clear all symbol references for union fields
   for (UnionMap::iterator it = _unionsByName.begin(); it != _unionsByName.end(); it++)
      {
      OMR::UnionType *aUnion = it->second;
      aUnion->clearSymRefs();
      }
   }

OMR::StructType *
OMR::TypeDictionary::getStruct(const char *structName)
   {
   StructMap::iterator it = _structsByName.find(structName);
   TR_ASSERT_FATAL(it != _structsByName.end(), "No struct named '%s'", structName);

   OMR::StructType *theStruct = it->second;
   return theStruct;
   }

OMR::UnionType *
OMR::TypeDictionary::getUnion(const char *unionName)
   {
   UnionMap::iterator it = _unionsByName.find(unionName);
   TR_ASSERT_FATAL(it != _unionsByName.end(), "No union named '%s'", unionName);

   OMR::UnionType *theUnion = it->second;
   return theUnion;
   }

void *
OMR::TypeDictionary::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::TypeDictionary *>(this));
   return _client;
   }

ClientAllocator OMR::TypeDictionary::_clientAllocator = NULL;
ClientAllocator OMR::TypeDictionary::_getImpl = NULL;
