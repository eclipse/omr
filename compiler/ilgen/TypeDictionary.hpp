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


#ifndef TR_ILTYPE_DEFINED
#define TR_ILTYPE_DEFINED
#define PUT_OMR_ILTYPE_INTO_TR
#endif

#ifndef TR_TYPEDICTIONARY_DEFINED
#define TR_TYPEDICTIONARY_DEFINED
#define PUT_OMR_TYPEDICTIONARY_INTO_TR
#endif


#include "map"
#include "ilgen/IlBuilder.hpp"
#include "env/TypedAllocator.hpp"

class TR_Memory;

namespace OMR { class StructType; }
namespace OMR { class UnionType; }
namespace TR  { class SegmentProvider; }
namespace TR  { class Region; }
namespace TR  { typedef TR::SymbolReference IlReference; }


namespace OMR
{

class IlType
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   IlType(const char *name) :
      _name(name)
      { }
   IlType() :
      _name(0)
      { }
   virtual ~IlType()
      { }

   const char *getName() { return _name; }
   virtual char *getSignatureName();

   virtual TR::DataType getPrimitiveType() { return TR::NoType; }

   virtual bool isArray() { return false; }
   virtual bool isPointer() { return false; }
   virtual TR::IlType *baseType() { return NULL; }

   virtual bool isStruct() {return false; }
   virtual bool isUnion() { return false; }

   virtual size_t getSize();

protected:
   const char *_name;
   };


/**
 * @brief Convenience API for defining JitBuilder structs from C/C++ structs (PODs)
 * 
 * These macros simply allow the name of C/C++ structs and fields to be used to
 * define JitBuilder structs. This can help ensure a consistent API between a
 * VM struct and JitBuilder's representation of the same struct. Their definitions
 * expand to calls to `TR::TypeDictionary` methods that create a representation
 * corresponding to that specified type.
 * 
 * ## Usage
 * 
 * Given the following struct:
 * 
 * ```c++
 * struct MyStruct {
 *    int32_t id;
 *    int32_t* val;
 * };
 * 
 * a JitBuilder representation of this struct can be defined as follows:
 * 
 * ```c++
 * TR::TypeDictionary d;
 * d.DEFINE_STRUCT(MyStruct);
 * d.DEFINE_FIELD(MyStruct, id, Int32);
 * d.DEFINE_FIELD(MyStruct, val, pInt32);
 * d.CLOSE_STRUCT(MyStruct);
 * ```
 * 
 * This definition will expand to the following:
 * 
 * ```c++
 * TR::TypeDictionary d;
 * d.DefineStruct("MyStruct");
 * d.DefineField("MyStruct", "id", Int32, offsetof(MyStruct, id));
 * d.DefineField("MyStruct", "val", pInt32, offsetof(MyStruct, val));
 * d.CloseStruct("MyStruct", sizeof(MyStruct));
 * ```
 */
#define DEFINE_STRUCT(structName) \
   DefineStruct(#structName)
#define DEFINE_FIELD(structName, fieldName, filedIlType) \
   DefineField(#structName, #fieldName, filedIlType, offsetof(structName, fieldName))
#define CLOSE_STRUCT(structName) \
   CloseStruct(#structName, sizeof(structName))


class TypeDictionary
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   TypeDictionary();
   TypeDictionary(const TypeDictionary &src) = delete;
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
   TR_Memory *trMemory() { return _trMemory; }

   //TR::IlReference *ArrayReference(TR::IlType *arrayType);

   /**
    * @brief A template class for checking whether a particular type is supported by `toIlType<>()`
    * @tparam C/C++ type
    * @return whether `toIlType<>()` can generate a corresponding TR::IlType instance or will fail to compile
    *
    * A type is considered supported iff JitBuilder directly provides, or can derive,
    * a type that corresponds, or that is equivalent, to the specified type.
    * 
    * ## Usage
    * 
    * `is_supported` can be used the same way as any type property check from
    * the C++11 type traits standard library. Eg:
    * 
    * ```c++
    * static_assert(is_supported<int>::value, "int is not a supported type!");
    * ```
    * 
    * ## Examples
    *
    * - `is_supported<int8_t>::value == true` because JitBuilder directly provides the corresponding type `Int8`
    * - `is_supported<int32_t*>::value == true` because JitBuilder directly provides the corresponding type `pInt32` or `PointerTo(Int32)`
    * - `is_supported<double**>::value == true` because JitBuilder can derive the corresponding type `PointerTo(pFloat)` or `PointerTo(PointerTo(Float))`
    * - `is_supported<uint16_t>::value == true` because JitBuilder provides the equivalent type `Int16`
    * - `is_supported<SomeStruct>::value == false` because JitBuilder cannot derive the type of a struct
    * - `is_supported<SomeEnum>::value == false` because JitBuilder cannot derive a type that is equivalent to the underlying type of the enum
    * - `is_supported<void>::value == true` because JitBuilder directly provides the corresponding `NoType`
    */
   template <typename T>
   struct is_supported {
      static const bool value =  std::is_arithmetic<T>::value // note: is_arithmetic = is_integral || is_floating_point
                              || std::is_void<T>::value;
   };
   template <typename T>
   struct is_supported<T*> {
      // a pointer type is supported iff the type being pointed to is supported
      static const bool value =  is_supported<T>::value;
   };

   /** @fn template <typename T> TR::IlType* toIlType()
    * @brief Given a C/C++ type, returns a corresponding TR::IlType, if available
    * @tparam C/C++ type
    * @return TR::IlType instance corresponding to the specified C/C++ type
    * 
    * Given a C/C++ type, `toIlType<>` will attempt to match the type with a
    * corresponding TR::IlType instance that is usable with JitBuilder. If no
    * type is **known** to match the specified type (meaning the type is
    * unsupported), then the function call fails to compile.
    * 
    * Currently, only the following types are supported:
    * 
    * - all integral types (int, long, etc.)
    * - all floating point types (float, double)
    * - void
    * - pointers to all the above types
    * 
    * Note that many user defined types (e.g. structs and arrays) are not
    * currently supported.
    * 
    * For convenience, the template class `is_supported` can be used to determine
    * at compile time whether a particular type is supported.
    * 
    * ## Usage
    * 
    * Using this template function is as simple as:
    * 
    * ```c++
    * auto d = TR::TypeDictionary{};
    * auto t = d.toIlType<int>();
    * ```
    * 
    * If the type specified has no known corresponding TR::IlType, then the function
    * call simply fails to compile, reporting that there is "no matching function
    * call" (or some variation thereof).
    * 
    * ## Design
    * 
    * `toIlType<>()` is an overloaded template function. It uses SFINAE and the C++11
    * type traits library to enable a specific overload (function implementation)
    * that will return an instance of TR::IlType. Type traits are used to define
    * rules that a type must follow to match a given function implementation.
    * 
    * For integer and floating point types, the type traits library is used to
    * identify the "family" of the specified type. For example, `std::is_integral<>`
    * is used to identify all the integer types. The `sizeof` operator is then used
    * to determine the size of the specified type. The combination of the type's
    * family and size is used to select the function that gets selected.
    * 
    * For types that do not belong to a family (e.g. `void`), the size is not needed.
    * 
    * For pointer types, `std::remove_pointer<>` is used to get the type being
    * pointed to. Iff `is_supported<>::value` evaluates to true for this type,
    * then `toIlType<>()` is recursively called on it.
    * 
    * If `toIlType<>()` is called on a type that does not match any rule,
    * no implementation is instantiated and the call fails to compile.
    * 
    * ### Rule definition
    * 
    * The rules for enable a template overload (described above) are defined
    * using `std::enable_if<>`, where conditional enabling is achieved using
    * SFINAE. The field `std::enable_if<>::type` is used as the type of the
    * anonymous argument in `toIlType<>()`.
    * 
    * All definitions take the following form:
    * 
    * ```c++
    * template <typename T>
    * void toIlType(typename std::enable_if<THE CONDITION>::type* = 0) {...}
    * ```
    * 
    * and have the same signature: `void(void*)`. Calls to `toIlType<>()`
    * **must** therefore specify a type parameter to avoid ambiguity.
    * 
    * ### Design considerations
    * 
    * Conceptually, `toIlType<>()` defines a mapping from C/C++ types to
    * `TR::IlType` instances.
    * 
    * A more idiomatic implementation of `toIlType<>()` would have used a template
    * class (metafunction) instead of a template function. However, because instances
    * of `TR::IlType` are stored and returned as pointers, the "return" value of the
    * metafunction cannot be known at compile time. Therefore, a function returning
    * the correct instance must be used instead.
    * 
    * For rule definitions, especially for integer types, although it may seem
    * feasible to simply specialize `toIlType<>()` for `int8_t`, `int16_t`, etc.
    * this approach does not take into consideration that multiple integer types
    * may have the same size (e.g. `int` and `long`). Using this approach would
    * cause `toIlType<>()` to only be implemented for one of those types.
    * 
    * Another approach might be to attempt to specialize for all primitive types
    * (i.e. `int`, `float`, etc). However, in this approach, a specialization
    * would not only have to be provided for each type but also for all
    * `unsigned`, `const`, and `volatile` variations of those types. Furthermore,
    * because the C and C++ standards do not specify the exact size of some types,
    * a size check would have to also be performed. This leads to the combinatorial
    * explosion of the number of template specializations that must be defined,
    * which is rather unmanageable.
    * 
    * As a note, it's important that enabling rules be mutually exclusive so
    * that a type either enables a single template overload, or none at all.
    * Otherwise, calls to `toIlType<>()` can become ambiguous for some types.
    */

   // integral
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 1)>::type* = 0) { return Int8; }
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 2)>::type* = 0) { return Int16; }
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 4)>::type* = 0) { return Int32; }
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 8)>::type* = 0) { return Int64; }

   // floating point
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_floating_point<T>::value && (sizeof(T) == 4)>::type* = 0) { return Float; }
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_floating_point<T>::value && (sizeof(T) == 8)>::type* = 0) { return Double; }

   // void
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_void<T>::value>::type* = 0) { return NoType; }

   // pointer
   template <typename T>
   TR::IlType* toIlType(typename std::enable_if<std::is_pointer<T>::value && is_supported<typename std::remove_pointer<T>::type>::value>::type* = 0) {
      return PointerTo(toIlType<typename std::remove_pointer<T>::type>());
   }

   /*
    * @brief advise that compilation is complete so compilation-specific objects like symbol references can be cleared from caches
    */
   void NotifyCompilationDone();

protected:
   TR::SegmentProvider *_segmentProvider;
   TR::Region *_memoryRegion;
   TR_Memory *_trMemory;

   typedef bool (*StrComparator)(const char *, const char *);

   typedef TR::typed_allocator<std::pair<const char * const, OMR::StructType *>, TR::Region &> StructMapAllocator;
   typedef std::map<const char *, OMR::StructType *, StrComparator, StructMapAllocator> StructMap;
   StructMap        * _structsByName;

   typedef TR::typed_allocator<std::pair<const char * const, OMR::UnionType *>, TR::Region &> UnionMapAllocator;
   typedef std::map<const char *, OMR::UnionType *, StrComparator, UnionMapAllocator> UnionMap;
   UnionMap         * _unionsByName;

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

   OMR::StructType * getStruct(const char *structName);
   OMR::UnionType  * getUnion(const char *unionName);
   };

} // namespace OMR


#if defined(PUT_OMR_ILTYPE_INTO_TR)
namespace TR
{
class IlType : public OMR::IlType
   {
   public:
      IlType(const char *name)
         : OMR::IlType(name)
         { }
      IlType()
         : OMR::IlType()
         { }
   };
} // namespace TR
#endif // defined(PUT_OMR_ILTYPE_INTO_TR)


#if defined(PUT_OMR_TYPEDICTIONARY_INTO_TR)
namespace TR
{
class TypeDictionary : public OMR::TypeDictionary
   {
   public:
      TypeDictionary()
         : OMR::TypeDictionary()
         { }
   };
} // namespace TR
#endif // defined(PUT_OMR_TYPEDICTIONARY_INTO_TR)

#endif // !defined(OMR_TYPEDICTIONARY_INCL)
