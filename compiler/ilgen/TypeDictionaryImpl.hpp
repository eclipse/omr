/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#ifndef OMR_TYPEDICTIONARY_IMPL_INCL
#define OMR_TYPEDICTIONARY_IMPL_INCL


#ifndef TR_TYPEDICTIONARY_IMPL_DEFINED
#define TR_TYPEDICTIONARY_IMPL_DEFINED
#define PUT_OMR_TYPEDICTIONARY_IMPL_INTO_TR
#endif


#include "map"
#include "env/TypedAllocator.hpp"
#include "infra/BitVector.hpp"

class TR_Memory;
class TR_BitVector;

namespace OMR { class StructType; }
namespace OMR { class UnionType; }

namespace TR  { class SegmentProvider; }
namespace TR  { class Region; }
namespace TR  { class SymbolReference; }
namespace TR  { typedef SymbolReference IlReference; }

#include "ilgen/IlTypeImpl.hpp"
#include "ilgen/TypeDictionary.hpp"

namespace OMR
{

class PrimitiveType : public TR::IlTypeImpl
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   PrimitiveType(const char * name, TR::DataType type, TR::TypeDictionary *dict) :
      TR::IlTypeImpl(name, dict),
      _type(type)
      { }
   virtual ~PrimitiveType()
      { }

   virtual TR::IlType *getPrimitiveType()
      {
      return _dict->PrimitiveType(_type);
      }

   virtual size_t getSize() { return TR::DataType::getSize(_type); }

   virtual TR::DataType getRealPrimitiveType()
      {
      return _type;
      }

protected:
   TR::DataType _type;
   };


class FieldInfo
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   FieldInfo(const char *name, size_t offset, TR::IlTypeImpl *type) :
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

   TR::IlTypeImpl *getType()                     { return _type; }

   TR::DataType getPrimitiveType()               { return _type->getRealPrimitiveType(); }

   size_t getOffset()                            { return _offset; }

   FieldInfo *getNext()                          { return _next; }
   void setNext(FieldInfo *next)                 { _next = next; }

   FieldInfo           * _next;
   const char          * _name;
   size_t                _offset;
   TR::IlTypeImpl      * _type;
   TR::SymbolReference * _symRef;
   };


class StructType : public TR::IlTypeImpl
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   StructType(const char *name, TR::TypeDictionary *dict) :
      TR::IlTypeImpl(name, dict),
      _firstField(0),
      _lastField(0),
      _size(0),
      _closed(false)
      { }
   virtual ~StructType()
      { }

   virtual TR::IlType *getPrimitiveType()            { return _dict->PrimitiveType(TR::Address); }
   virtual TR::DataType getRealPrimitiveType()       { return TR::Address; }

   void Close(size_t finalSize)                      { TR_ASSERT(_size <= finalSize, "Final size %d of struct %s is less than its current size %d\n", finalSize, _name, _size); _size = finalSize; _closed = true; };
   void Close()                                      { _closed = true; };

   void AddField(const char *name, TR::IlTypeImpl *fieldType, size_t offset);
   void AddField(const char *name, TR::IlTypeImpl *fieldType);
   TR::IlTypeImpl * getFieldType(const char *fieldName);
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


class UnionType : public TR::IlTypeImpl
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   UnionType(const char *name, TR::TypeDictionary *dict, TR_Memory* trMemory) :
      TR::IlTypeImpl(name, dict),
      _firstField(0),
      _lastField(0),
      _size(0),
      _closed(false),
      _symRefBV(4, trMemory),
      _trMemory(trMemory)
      { }
   virtual ~UnionType()
      { }

   virtual TR::IlType *getPrimitiveType()            { return _dict->PrimitiveType(TR::Address); }
   virtual TR::DataType getRealPrimitiveType()       { return TR::Address; }
   void Close();

   void AddField(const char *name, TR::IlTypeImpl *fieldType);
   TR::IlTypeImpl * getFieldType(const char *fieldName);

   TR::SymbolReference *getFieldSymRef(const char *name);
   virtual bool isUnion() { return true; }
   virtual size_t getSize() { return _size; }

   void clearSymRefs();

protected:
   FieldInfo * findField(const char *fieldName);

   FieldInfo  * _firstField;
   FieldInfo  * _lastField;
   size_t       _size;
   bool         _closed;
   TR_BitVector _symRefBV;
   TR_Memory*   _trMemory;
   };


class PointerType : public TR::IlTypeImpl
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   PointerType(TR::IlTypeImpl *baseType) :
      TR::IlTypeImpl(_nameArray, baseType->dict()),
      _baseType(baseType)
      {
      char *baseName = (char *)_baseType->getName();
      TR_ASSERT(strlen(baseName) < 45, "cannot store name of pointer type");
      sprintf(_nameArray, "L%s;", baseName);
      }
   virtual bool isPointer() { return true; }

   virtual TR::IlTypeImpl *baseType() { return _baseType; }

   virtual const char *getName() { return _name; }

   virtual TR::IlType *getPrimitiveType()      { return _dict->PrimitiveType(TR::Address); }
   virtual TR::DataType getRealPrimitiveType() { return TR::Address; }

   virtual size_t getSize() { return TR::DataType::getSize(TR::Address); }

protected:
   TR::IlTypeImpl      * _baseType;
   char                  _nameArray[48];
   };


class TypeDictionaryImpl
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   static TR::TypeDictionaryImpl *allocate(TR::TypeDictionary *client);

   TypeDictionaryImpl(TR::TypeDictionary *client);
   TypeDictionaryImpl(const TypeDictionaryImpl &src) = delete;
   ~TypeDictionaryImpl() throw();

   /**
    * TypeDictionary public API implementations, in alphabetical order
    */

   void CloseStruct(const char *structName);
   void CloseStruct(const char *structName, size_t finalSize);
   void DefineField(const char *structName, const char *fieldName, TR::IlTypeImpl *type);
   void DefineField(const char *structName, const char *fieldName, TR::IlTypeImpl *type, size_t offset);
   TR::IlTypeImpl * DefineStruct(const char *structName);
   TR::IlTypeImpl * DefineUnion(const char *unionName);
   TR::IlTypeImpl * GetFieldType(const char *structName, const char *fieldName);
   TR::IlTypeImpl * LookupStruct(const char *structName);
   TR::IlTypeImpl * LookupUnion(const char *unionName);
   size_t OffsetOf(const char *structName, const char *fieldName);
   TR::IlTypeImpl *PointerTo(TR::DataType baseType)
      { return PointerTo(_primitiveType[baseType]->impl()); }
   TR::IlTypeImpl *PointerTo(TR::IlTypeImpl *baseType);
   TR::IlTypeImpl *PointerTo(const char *structName);
   TR::IlTypeImpl *PrimitiveType(TR::DataType primitiveType)
      { return _primitiveType[primitiveType]->impl(); }
   void UnionField(const char *unionName, const char *fieldName, TR::IlTypeImpl *type);
   void CloseUnion(const char *unionName);
   TR::IlTypeImpl * UnionFieldType(const char *unionName, const char *fieldName);

   /**
    * public API for other JitBuilder classes
    */

   TR_Memory *trMemory()
      { return _trMemory; }
   TR::IlReference *FieldReference(const char *typeName, const char *fieldName);

   /*
    * @brief advise that compilation is complete so compilation-specific objects like symbol references can be cleared from caches
    */
   void notifyCompilationDone();

   // convenient access for primitive types
   TR::IlTypeImpl * NoType;
   TR::IlTypeImpl * Int8;
   TR::IlTypeImpl * Int16;
   TR::IlTypeImpl * Int32;
   TR::IlTypeImpl * Int64;
   TR::IlTypeImpl * Float;
   TR::IlTypeImpl * Double;
   TR::IlTypeImpl * Address;
   TR::IlTypeImpl * VectorInt8;
   TR::IlTypeImpl * VectorInt16;
   TR::IlTypeImpl * VectorInt32;
   TR::IlTypeImpl * VectorInt64;
   TR::IlTypeImpl * VectorFloat;
   TR::IlTypeImpl * VectorDouble;

   // convenient access for pointer types
   TR::IlTypeImpl * pNoType;
   TR::IlTypeImpl * pInt8;
   TR::IlTypeImpl * pInt16;
   TR::IlTypeImpl * pInt32;
   TR::IlTypeImpl * pInt64;
   TR::IlTypeImpl * pFloat;
   TR::IlTypeImpl * pDouble;
   TR::IlTypeImpl * pAddress;
   TR::IlTypeImpl * pVectorInt8;
   TR::IlTypeImpl * pVectorInt16;
   TR::IlTypeImpl * pVectorInt32;
   TR::IlTypeImpl * pVectorInt64;
   TR::IlTypeImpl * pVectorFloat;
   TR::IlTypeImpl * pVectorDouble;

   // convenient access for machine word sized integer types
   TR::IlType     * Word;
   TR::IlType     * pWord;

protected:
   TR::SegmentProvider * _segmentProvider;
   TR::Region          * _memoryRegion;
   TR_Memory           * _trMemory;
   TR::TypeDictionary  * _client;

   typedef bool (*StrComparator)(const char *, const char *);

   typedef TR::typed_allocator<std::pair<const char * const, OMR::StructType *>, TR::Region &> StructMapAllocator;
   typedef std::map<const char *, OMR::StructType *, StrComparator, StructMapAllocator> StructMap;
   StructMap             _structsByName;

   typedef TR::typed_allocator<std::pair<const char * const, OMR::UnionType *>, TR::Region &> UnionMapAllocator;
   typedef std::map<const char *, OMR::UnionType *, StrComparator, UnionMapAllocator> UnionMap;
   UnionMap              _unionsByName;

   TR::IlTypeImpl      * _primitiveType[TR::NumOMRTypes];

   TR::IlTypeImpl      * _pointerToPrimitiveType[TR::NumOMRTypes];

   OMR::StructType * getStruct(const char *structName);
   OMR::UnionType  * getUnion(const char *unionName);

private:
   static const uint8_t primitiveTypeAlignment[TR::NumOMRTypes];
   };

} // namespace OMR


#if defined(PUT_OMR_TYPEDICTIONARY_IMPL_INTO_TR)

namespace TR
{

class TypeDictionaryImpl : public OMR::TypeDictionaryImpl
   {
   public:
      TypeDictionaryImpl(TR::TypeDictionary *client)
         : OMR::TypeDictionaryImpl(client)
         { }
   };

} // namespace TR

#endif // defined(PUT_OMR_TYPEDICTIONARY_IMPL_INTO_TR)

#endif // !defined(OMR_TYPEDICTIONARY_IMPL_INCL)
