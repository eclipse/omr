/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#include <stdlib.h>

#include "il/DataTypes.hpp"
#include "il/SymbolReference.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/Compilation.hpp"
#include "env/FrontEnd.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "infra/BitVector.hpp"


namespace OMR
{

static const char *signatureNameForType[] =
   {
   "V",  // NoType
   "B",  // Int8
   "C",  // Int16
   "I",  // Int32
   "J",  // Int64
   "F",  // Float
   "D",  // Double
   "L",  // Address
   // TODO: vector types!
   };

char *
IlType::getSignatureName()
   {
   TR::DataType dt = getPrimitiveType();
   if (dt == TR::Address)
      return (char *)_name;
   return (char *) signatureNameForType[dt];
   }

size_t
IlType::getSize()
   {
   TR_ASSERT(0, "The input type does not have a defined size\n");
   return 0;
   }

const uint8_t primitiveTypeAlignment[TR::NumOMRTypes] =
   {
   1,  // NoType
   1,  // Int8
   2,  // Int16
   4,  // Int32
   8,  // Int64
   4,  // Float
   8,  // Double
#if TR_TARGET_64BIT // HOST?
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

   void cacheSymRef(TR::SymbolReference *symRef) { _symRef = symRef; }
   TR::SymbolReference *getSymRef()              { return _symRef; }
   void clearSymRef()                            { _symRef = NULL; }

   TR::IlType *getType()                         { return _type; }

   TR::DataType getPrimitiveType()               { return _type->getPrimitiveType(); }

   size_t getOffset()                            { return _offset; }

   FieldInfo *getNext()                          { return _next; }
   void setNext(FieldInfo *next)                 { _next = next; }

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

   TR::DataType getPrimitiveType()                 { return TR::Address; }
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

void
StructType::AddField(const char *name, TR::IlType *typeInfo, size_t offset)
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
StructType::AddField(const char *name, TR::IlType *typeInfo)
   {
   if (_closed)
      return;

   TR::DataType primitiveType = typeInfo->getPrimitiveType();
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

FieldInfo *
StructType::findField(const char *fieldName)
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

TR::IlType *
StructType::getFieldType(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;
   return info->_type;
   }

size_t
StructType::getFieldOffset(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return -1;
   return info->getOffset();
   }

TR::IlReference *
StructType::getFieldSymRef(const char *fieldName)
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
StructType::clearSymRefs()
   {
   FieldInfo *field = _firstField;
   while (field)
      {
      field->clearSymRef();
      field = field->_next;
      }
   }


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

   TR::DataType getPrimitiveType()                 { return TR::Address; }
   void Close();

   void AddField(const char *name, TR::IlType *fieldType);
   TR::IlType * getFieldType(const char *fieldName);

   TR::SymbolReference *getFieldSymRef(const char *name);
   virtual bool isUnion() { return true; }
   virtual size_t getSize() { return _size; }

   void clearSymRefs();

protected:
   FieldInfo * findField(const char *fieldName);

   FieldInfo * _firstField;
   FieldInfo * _lastField;
   size_t      _size;
   bool        _closed;
   TR_BitVector _symRefBV;
   TR_Memory* _trMemory;
   };

void
UnionType::AddField(const char *name, TR::IlType *typeInfo)
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
UnionType::Close()
   {
   _closed = true;
   }

FieldInfo *
UnionType::findField(const char *fieldName)
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

TR::IlType *
UnionType::getFieldType(const char *fieldName)
   {
   FieldInfo *info = findField(fieldName);
   if (NULL == info)
      return NULL;
   return info->_type;
   }

TR::IlReference *
UnionType::getFieldSymRef(const char *fieldName)
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
UnionType::clearSymRefs()
   {
   FieldInfo *field = _firstField;
   while (field)
      {
      field->clearSymRef();
      field = field->_next;
      }
   _symRefBV.init(4, _trMemory);
   }


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

   virtual TR::DataType getPrimitiveType() { return TR::Address; }

   virtual size_t getSize() { return TR::DataType::getSize(TR::Address); }

protected:
   TR::IlType          * _baseType;
   char                  _nameArray[48];
   };


TypeDictionary::TypeDictionary() :
   _segmentProvider( static_cast<TR::SegmentProvider *>(new(TR::Compiler->persistentAllocator()) TR::SystemSegmentProvider(1 << 16, TR::Compiler->rawAllocator)) ),
   _memoryRegion( new(TR::Compiler->persistentAllocator()) TR::Region(*_segmentProvider, TR::Compiler->rawAllocator) ),
   _trMemory( new(TR::Compiler->persistentAllocator()) TR_Memory(*::trPersistentMemory, *_memoryRegion) )
   {
   _structsByName = new (PERSISTENT_NEW) TR_HashTabString(trMemory());
   _unionsByName = new (PERSISTENT_NEW) TR_HashTabString(trMemory());

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
                   _pointerToPrimitiveType[TR::NoType]       = NULL;
   pInt8         = _pointerToPrimitiveType[TR::Int8]         = new (PERSISTENT_NEW) PointerType(Int8);
   pInt16        = _pointerToPrimitiveType[TR::Int16]        = new (PERSISTENT_NEW) PointerType(Int16);
   pInt32        = _pointerToPrimitiveType[TR::Int32]        = new (PERSISTENT_NEW) PointerType(Int32);
   pInt64        = _pointerToPrimitiveType[TR::Int64]        = new (PERSISTENT_NEW) PointerType(Int64);
   pFloat        = _pointerToPrimitiveType[TR::Float]        = new (PERSISTENT_NEW) PointerType(Float);
   pDouble       = _pointerToPrimitiveType[TR::Double]       = new (PERSISTENT_NEW) PointerType(Double);
   pAddress      = _pointerToPrimitiveType[TR::Address]      = new (PERSISTENT_NEW) PointerType(Address);
   pVectorInt8   = _pointerToPrimitiveType[TR::VectorInt8]   = new (PERSISTENT_NEW) PointerType(VectorInt8);
   pVectorInt16  = _pointerToPrimitiveType[TR::VectorInt16]  = new (PERSISTENT_NEW) PointerType(VectorInt16);
   pVectorInt32  = _pointerToPrimitiveType[TR::VectorInt32]  = new (PERSISTENT_NEW) PointerType(VectorInt32);
   pVectorInt64  = _pointerToPrimitiveType[TR::VectorInt64]  = new (PERSISTENT_NEW) PointerType(VectorInt64);
   pVectorFloat  = _pointerToPrimitiveType[TR::VectorFloat]  = new (PERSISTENT_NEW) PointerType(VectorFloat);
   pVectorDouble = _pointerToPrimitiveType[TR::VectorDouble] = new (PERSISTENT_NEW) PointerType(VectorDouble);

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

TypeDictionary::~TypeDictionary() throw()
   {
   _trMemory->~TR_Memory();
   ::operator delete(_trMemory, TR::Compiler->persistentAllocator());
   _memoryRegion->~Region();
   ::operator delete(_memoryRegion, TR::Compiler->persistentAllocator());
   static_cast<TR::SystemSegmentProvider *>(_segmentProvider)->~SystemSegmentProvider();
   ::operator delete(_segmentProvider, TR::Compiler->persistentAllocator());
   }

TR::IlType *
TypeDictionary::LookupStruct(const char *structName)
   {
   TR_HashId structID = 0;
   if (_structsByName->locate(structName, structID))
      return (TR::IlType *) _structsByName->getData(structID);
   return NULL; 
   }

TR::IlType *
TypeDictionary::LookupUnion(const char *unionName)
   {
   TR_HashId unionID = 0;
   if (_unionsByName->locate(unionName, unionID))
      return (TR::IlType *) _unionsByName->getData(unionID);
   return NULL;
   }

TR::IlType *
TypeDictionary::DefineStruct(const char *structName)
   {
   TR_HashId structID=0;
   _structsByName->locate(structName, structID);

   StructType *newType = new (PERSISTENT_NEW) StructType(structName);
   _structsByName->add(structName, structID, (void *) newType);

   return (TR::IlType *) newType;
   }

void
TypeDictionary::DefineField(const char *structName, const char *fieldName, TR::IlType *type, size_t offset)
   {
   TR_HashId structID=0;
   if (_structsByName->locate(structName, structID))
      {
      StructType *structType = (StructType *) _structsByName->getData(structID);
      structType->AddField(fieldName, type, offset);
      }
   }

void
TypeDictionary::DefineField(const char *structName, const char *fieldName, TR::IlType *type)
   {
   TR_HashId structID=0;
   if (_structsByName->locate(structName, structID))
      {
      StructType *structType = (StructType *) _structsByName->getData(structID);
      structType->AddField(fieldName, type);
      }
   }

TR::IlType *
TypeDictionary::GetFieldType(const char *structName, const char *fieldName)
   {
   TR_HashId structID = 0;
   _structsByName->locate(structName, structID);
   StructType *theStruct = (StructType *) _structsByName->getData(structID);
   return theStruct->getFieldType(fieldName);
   }

size_t
TypeDictionary::OffsetOf(const char *structName, const char *fieldName)
   {
   TR_HashId structID = 0;
   bool structNameFound = _structsByName->locate(structName, structID);
   TR_ASSERT(structNameFound, "No struct with name %s found when getting offset of field %s\n", structName, fieldName);
   StructType *theStruct = (StructType *) _structsByName->getData(structID);
   return theStruct->getFieldOffset(fieldName);
   }

void
TypeDictionary::CloseStruct(const char *structName, size_t finalSize)
   {
   TR_HashId structID = 0;
   _structsByName->locate(structName, structID);
   StructType *theStruct = (StructType *) _structsByName->getData(structID);
   theStruct->Close(finalSize);
   }

void
TypeDictionary::CloseStruct(const char *structName)
   {
   TR_HashId structID = 0;
   _structsByName->locate(structName, structID);
   StructType *theStruct = (StructType *) _structsByName->getData(structID);
   theStruct->Close();
   }

TR::IlType *
TypeDictionary::DefineUnion(const char *unionName)
   {
   TR_HashId unionID=0;
   _unionsByName->locate(unionName, unionID);

   UnionType *newType = new (PERSISTENT_NEW) UnionType(unionName, _trMemory);
   _unionsByName->add(unionName, unionID, (void *) newType);

   return (TR::IlType *) newType;
   }

void
TypeDictionary::UnionField(const char *unionName, const char *fieldName, TR::IlType *type)
   {
   TR_HashId unionID=0;
   if (_unionsByName->locate(unionName, unionID))
      {
      UnionType *unionType = (UnionType *) _unionsByName->getData(unionID);
      unionType->AddField(fieldName, type);
      }
   }

void
TypeDictionary::CloseUnion(const char *unionName)
   {
   TR_HashId unionID = 0;
   _unionsByName->locate(unionName, unionID);
   UnionType *theUnion = (UnionType *) _unionsByName->getData(unionID);
   theUnion->Close();
   }

TR::IlType *
TypeDictionary::UnionFieldType(const char *unionName, const char *fieldName)
   {
   TR_HashId unionID = 0;
   _unionsByName->locate(unionName, unionID);
   UnionType *theUnion = (UnionType *) _unionsByName->getData(unionID);
   return theUnion->getFieldType(fieldName);
   }

TR::IlType *
TypeDictionary::PointerTo(const char *structName)
   {
   TR_HashId structID = 0;
   _structsByName->locate(structName, structID);
   StructType *theStruct = (StructType *) _structsByName->getData(structID);
   return PointerTo(theStruct);
   }

TR::IlType *
TypeDictionary::PointerTo(TR::IlType *baseType)
   {
   return new (PERSISTENT_NEW) PointerType(baseType);
   }

TR::IlReference *
TypeDictionary::FieldReference(const char *typeName, const char *fieldName)
   {
   TR_HashId typeID = 0;

   auto found = _structsByName->locate(typeName, typeID);
   if (found)
      {
      StructType *theStruct = (StructType *) _structsByName->getData(typeID);
      return theStruct->getFieldSymRef(fieldName);
      }

   found = _unionsByName->locate(typeName, typeID);
   if (found)
      {
      UnionType *theUnion = (UnionType *) _unionsByName->getData(typeID);
      return theUnion->getFieldSymRef(fieldName);
      }

   TR_ASSERT(false, "No type with name `%s`", typeName);
   return NULL;
   }

void
TypeDictionary::NotifyCompilationDone()
   {
   // clear all symbol references for fields
   TR_HashTabIterator structIterator(_structsByName);
   for (StructType *aStruct = (StructType *)structIterator.getFirst();aStruct;aStruct = (StructType *)structIterator.getNext())
      {
      aStruct->clearSymRefs();
      }

   // clear all symbol references for union fields
   TR_HashTabIterator unionIterator(_unionsByName);
   for (UnionType *aUnion = (UnionType *)unionIterator.getFirst();aUnion;aUnion = (UnionType *)unionIterator.getNext())
      {
      aUnion->clearSymRefs();
      }
   }

} // namespace OMR
