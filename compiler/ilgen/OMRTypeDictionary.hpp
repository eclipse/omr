/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#ifndef OMR_TYPEDICTIONARY_INCL
#define OMR_TYPEDICTIONARY_INCL


#include "map"
#include "ilgen/IlBuilder.hpp"
#include "env/TypedAllocator.hpp"

class TR_Memory;

namespace OMR { class StructType; }
namespace OMR { class UnionType; }
namespace TR  { class IlReference; }
namespace TR  { class SegmentProvider; }
namespace TR  { class Region; }

extern "C" {
typedef void * (*ClientAllocator)(void * impl);
typedef void * (*ImplGetter)(void *client);
}

namespace OMR
{

class TypeDictionary
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   TypeDictionary();
   TypeDictionary(const TypeDictionary &src); // = delete;
   ~TypeDictionary() throw();

   TR::IlType * LookupStruct(const char *structName);
   TR::IlType * LookupUnion(const char *unionName);

   /**
    * @brief Begin definition of a new structure type
    * @param structName the name of the new type
    * @return pointer to IlType instance of the new type being defined
    * 
    * The name of the new type will have to be used when specifying
    * fields of the type. This method must be invoked once before any
    * calls to `DefineField()` and `CloseStruct()`.
    */
   TR::IlType * DefineStruct(const char *structName);

   /**
    * @brief Define a member of a new structure type
    * @param structName the name of the struct type on which to define the field
    * @param fieldName the name of the field
    * @param type the IlType instance representing the type of the field
    * @param offset the offset of the field within the structure (in bytes)
    * 
    * Fields defined using this method must be defined in offset order.
    * Specifically, the `offset` on any call to this method must be greater
    * than or equal to the size of the new struct at the time of the call
    * (`getSize() <= offset`). Failure to meet this condition will result
    * in a runtime failure. This was done as an initial attempt to prevent
    * struct fields from overlapping in memory.
    * 
    * This method can only be called after a call to `DefineStruct` and
    * before a call to `CloseStruct` with the same `structName`.
    */
   void DefineField(const char *structName, const char *fieldName, TR::IlType *type, size_t offset);

   /**
    * @brief Define a member of a new structure type
    * @param structName the name of the struct type on which to define the field
    * @param fieldName the name of the field
    * @param type the IlType instance representing the type of the field
    * 
    * This is an overloaded method. Since no offset for the new struct field is
    * specified, it will be added to the end of the struct using alignment rules
    * internally defined by JitBuilder. These are not guaranteed match the rules
    * used by a C/C++ compiler as alignment rules are compiler specific. However,
    * the alignment should be the same in most cases.
    * 
    * This method can only be called after a call to `DefineStruct` and
    * before a call to `CloseStruct` with the same `structName`.
    */
   void DefineField(const char *structName, const char *fieldName, TR::IlType *type);

   /**
    * @brief End definition of a new structure type
    * @param structName the name of the new type of which the definition is ended
    * @param finalSize the final size (in bytes) of the type
    * 
    * The `finalSize` of the struct must be greater than or equal to the size of
    * the new struct at the time of the call (`getSize() <= finalSize`). If
    * `finalSize` is greater, the size of the struct will be adjusted. Failure to
    * meet this condition will result in a runtime failure. This was done as
    * done as an initial attempt to help ensure that adequate padding is added to
    * the end of the new struct for use in arrays and nested structs.
    */
   void CloseStruct(const char *structName, size_t finalSize);

   /**
    * @brief End definition of a new structure type
    * @param structName the name of the new type of which the definition is ended
    * 
    * This is an overloaded method. Since the final size of the struct is not
    * specified, the size of the new struct at the time of call to this method
    * will be the final size of the new struct type.
    */
   void CloseStruct(const char *structName);

   TR::IlType * GetFieldType(const char *structName, const char *fieldName);

   /**
    * @brief Returns the offset of a field in a struct
    * @param structName the name of the struct containing the field
    * @param fieldName the name of the field in the struct
    * @return the memory offset of the field in bytes
    */
   size_t OffsetOf(const char *structName, const char *fieldName);

   TR::IlType * DefineUnion(const char *unionName);
   void UnionField(const char *unionName, const char *fieldName, TR::IlType *type);
   void CloseUnion(const char *unionName);
   TR::IlType * UnionFieldType(const char *unionName, const char *fieldName);

   TR::IlType *PrimitiveType(TR::DataType primitiveType)
      {
      return _primitiveType[primitiveType];
      }

   //TR::IlType *ArrayOf(TR::IlType *baseType);

   TR::IlType *PointerTo(TR::IlType *baseType);
   TR::IlType *PointerTo(const char *structName);
   TR::IlType *PointerTo(TR::DataType baseType)  { return PointerTo(_primitiveType[baseType]); }

   TR::IlReference *FieldReference(const char *typeName, const char *fieldName);
   TR_Memory *trMemory() { return memoryManager._trMemory; }
   TR::IlType *getWord() { return Word; }

   /*
    * @brief advise that compilation is complete so compilation-specific objects like symbol references can be cleared from caches
    */
   void NotifyCompilationDone();

   /**
    * @brief associates this object with a particular client object
    */
   void setClient(void *client)
      {
      _client = client;
      }

   /**
    * @brief returns the client object associated with this object
    */
   void *client();

   /**
    * @brief Set the Client Allocator function
    *
    * @param allocator a function pointer to the client object allocator
    */
   static void setClientAllocator(ClientAllocator allocator)
      {
      _clientAllocator = allocator;
      }

   /**
    * @brief Set the Get Impl function
    *
    * @param getter function pointer to the impl getter
    */
   static void setGetImpl(ImplGetter getter)
      {
      _getImpl = getter;
      }

protected:
   // We have MemoryManager as the first member of TypeDictionary, so that
   // it is the last one to get destroyed and all objects allocated using
   // MemoryManager->_memoryRegion may be safely destroyed in the destructor.
   typedef struct MemoryManager
      {
      MemoryManager();
      ~MemoryManager();

      TR::SegmentProvider *_segmentProvider;
      TR::Region *_memoryRegion;
      TR_Memory *_trMemory;
      } MemoryManager;

   MemoryManager memoryManager;

   OMR::StructType * getStruct(const char *structName);
   OMR::UnionType  * getUnion(const char *unionName);

   /**
    * @brief pointer to a client object that corresponds to this object
    */
   void * _client;

   /**
    * @brief pointer to the function used to allocate an instance of a
    * client object
    */
   static ClientAllocator _clientAllocator;

   /**
    * @brief pointer to impl getter function
    */
   static ImplGetter _getImpl;

   typedef bool (*StrComparator)(const char *, const char *);

   typedef TR::typed_allocator<std::pair<const char * const, OMR::StructType *>, TR::Region &> StructMapAllocator;
   typedef std::map<const char *, OMR::StructType *, StrComparator, StructMapAllocator> StructMap;
   StructMap          _structsByName;

   typedef TR::typed_allocator<std::pair<const char * const, OMR::UnionType *>, TR::Region &> UnionMapAllocator;
   typedef std::map<const char *, OMR::UnionType *, StrComparator, UnionMapAllocator> UnionMap;
   UnionMap           _unionsByName;

public:
   // convenience for primitive types
   TR::IlType       * _primitiveType[TR::NumOMRTypes];
   TR::IlType       * NoType;
   TR::IlType       * Int8;
   TR::IlType       * Int16;
   TR::IlType       * Int32;
   TR::IlType       * Int64;
   TR::IlType       * Word;
   TR::IlType       * Float;
   TR::IlType       * Double;
   TR::IlType       * Address;
   TR::IlType       * VectorInt8;
   TR::IlType       * VectorInt16;
   TR::IlType       * VectorInt32;
   TR::IlType       * VectorInt64;
   TR::IlType       * VectorFloat;
   TR::IlType       * VectorDouble;

   TR::IlType       * _pointerToPrimitiveType[TR::NumOMRTypes];
   TR::IlType       * pNoType;
   TR::IlType       * pInt8;
   TR::IlType       * pInt16;
   TR::IlType       * pInt32;
   TR::IlType       * pInt64;
   TR::IlType       * pWord;
   TR::IlType       * pFloat;
   TR::IlType       * pDouble;
   TR::IlType       * pAddress;
   TR::IlType       * pVectorInt8;
   TR::IlType       * pVectorInt16;
   TR::IlType       * pVectorInt32;
   TR::IlType       * pVectorInt64;
   TR::IlType       * pVectorFloat;
   TR::IlType       * pVectorDouble;
   };

} // namespace OMR

#endif // !defined(OMR_TYPEDICTIONARY_INCL)
