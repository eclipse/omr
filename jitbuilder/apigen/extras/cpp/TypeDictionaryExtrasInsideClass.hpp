/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
 *  * This program and the accompanying materials are made available under
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


#ifndef TYPEDICTIONARY_EXTRAS_INSIDE_CLASS_INCL
#define TYPEDICTIONARY_EXTRAS_INSIDE_CLASS_INCL

   /**
    * @brief A template class for checking whether a particular type is supported by `toIlType<>()`
    * @tparam C/C++ type
    * @return whether `toIlType<>()` can generate a corresponding IlType instance or will fail to compile
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
      static const bool value =  OMR::IsArithmetic<T>::VALUE // note: IsArithmetic = IsIntegral || IsFloatingPoint
                              || OMR::IsVoid<T>::VALUE;
   };
   template <typename T>
   struct is_supported<T*> {
      // a pointer type is supported iff the type being pointed to is supported
      static const bool value =  is_supported<T>::value;
   };

   /** @fn template <typename T> IlType* toIlType()
    * @brief Given a C/C++ type, returns a corresponding IlType, if available
    * @tparam C/C++ type
    * @return IlType instance corresponding to the specified C/C++ type
    *
    * Given a C/C++ type, `toIlType<>` will attempt to match the type with a
    * corresponding IlType instance that is usable with JitBuilder. If no
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
    * auto d = TypeDictionary{};
    * auto t = d.toIlType<int>();
    * ```
    *
    * If the type specified has no known corresponding IlType, then the function
    * call simply fails to compile, reporting that there is "no matching function
    * call" (or some variation thereof).
    *
    * ## Design
    *
    * `toIlType<>()` is an overloaded template function. It uses SFINAE and the C++11
    * type traits library to enable a specific overload (function implementation)
    * that will return an instance of IlType. Type traits are used to define
    * rules that a type must follow to match a given function implementation.
    *
    * For integer and floating point types, the type traits library is used to
    * identify the "family" of the specified type. For example, `OMR::IsIntegral<>`
    * is used to identify all the integer types. The `sizeof` operator is then used
    * to determine the size of the specified type. The combination of the type's
    * family and size is used to select the function that gets selected.
    *
    * For types that do not belong to a family (e.g. `void`), the size is not needed.
    *
    * For pointer types, `OMR::RemovePointer<>` is used to get the type being
    * pointed to. Iff `is_supported<>::value` evaluates to true for this type,
    * then `toIlType<>()` is recursively called on it.
    *
    * If `toIlType<>()` is called on a type that does not match any rule,
    * no implementation is instantiated and the call fails to compile.
    *
    * ### Rule definition
    *
    * The rules for enable a template overload (described above) are defined
    * using `OMR::EnableIf<>`, where conditional enabling is achieved using
    * SFINAE. The field `OMR::EnableIf<>::Type` is used as the type of the
    * anonymous argument in `toIlType<>()`.
    *
    * All definitions take the following form:
    *
    * ```c++
    * template <typename T>
    * void toIlType(typename OMR::EnableIf<THE CONDITION>::Type* = 0) {...}
    * ```
    *
    * and have the same signature: `void(void*)`. Calls to `toIlType<>()`
    * **must** therefore specify a type parameter to avoid ambiguity.
    *
    * ### Design considerations
    *
    * Conceptually, `toIlType<>()` defines a mapping from C/C++ types to
    * `IlType` instances.
    *
    * A more idiomatic implementation of `toIlType<>()` would have used a template
    * class (metafunction) instead of a template function. However, because instances
    * of `IlType` are stored and returned as pointers, the "return" value of the
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
   IlType* toIlType(typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE && (sizeof(T) == 1)>::Type* = 0) { return Int8; }
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE && (sizeof(T) == 2)>::Type* = 0) { return Int16; }
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE && (sizeof(T) == 4)>::Type* = 0) { return Int32; }
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE && (sizeof(T) == 8)>::Type* = 0) { return Int64; }

   // floating point
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsFloatingPoint<T>::VALUE && (sizeof(T) == 4)>::Type* = 0) { return Float; }
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsFloatingPoint<T>::VALUE && (sizeof(T) == 8)>::Type* = 0) { return Double; }

   // void
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsVoid<T>::VALUE>::Type* = 0) { return NoType; }

   // pointer
   template <typename T>
   IlType* toIlType(typename OMR::EnableIf<OMR::IsPointer<T>::VALUE && is_supported<typename OMR::RemovePointer<T>::Type>::value>::Type* = 0)
      {
      return PointerTo(toIlType<typename OMR::RemovePointer<T>::Type>());
      }

#endif // !defined(TYPEDICTIONARY_EXTRAS_INSIDE_CLASS_INCL)
