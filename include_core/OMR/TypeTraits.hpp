/*******************************************************************************
 *  Copyright (c) 2018, 2018 IBM and others
 *
 *  This program and the accompanying materials are made available under
 *  the terms of the Eclipse Public License 2.0 which accompanies this
 *  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 *  or the Apache License, Version 2.0 which accompanies this distribution and
 *  is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 *  This Source Code may also be made available under the following
 *  Secondary Licenses when the conditions for such availability set
 *  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 *  General Public License, version 2 with the GNU Classpath
 *  Exception [1] and GNU General Public License, version 2 with the
 *  OpenJDK Assembly Exception [2].
 *
 *  [1] https://www.gnu.org/software/classpath/license.html
 *  [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 *  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(OMR_TYPETRAITS_HPP_)
#define OMR_TYPETRAITS_HPP_

namespace OMR
{

///
/// Basic constants
///

/// Define an integral constant.
template <typename T, T value>
struct IntegralConstant
{
	typedef T ValueType;

	static const ValueType VALUE = value;

	ValueType operator()() const { return VALUE; }
};

template <typename T, T value>
const T IntegralConstant<T, value>::VALUE;

template <bool value>
struct BoolConstant : IntegralConstant<bool, value> {};

struct FalseConstant : BoolConstant<false> {};

struct TrueConstant : BoolConstant<true> {};

template <typename T>
struct TypeAlias
{
	typedef T Type;
};

///
/// Type modifications: add or remove qualifiers or type modifiers
///

/// RemoveConst<T> is T without any const qualifiers.
template <typename T>
struct RemoveConst : TypeAlias<T> {};

template <typename T>
struct RemoveConst<const T> : TypeAlias<T> {};

/// RemoveVolatile<T>::Type is T without volatile qualifiers.
template <typename T>
struct RemoveVolatile : TypeAlias<T> {};

template <typename T>
struct RemoveVolatile<volatile T> : TypeAlias<T> {};

/// RemoveCv<T>::Type is T without const/volatile qualifiers.
template <typename T>
struct RemoveCv : RemoveVolatile<typename RemoveConst<T>::Type> {};

/// RemoveReference<T>::Type is T as a non-reference.
template <typename T>
struct RemoveReference : TypeAlias<T> {};

template <typename T>
struct RemoveReference<T&> : TypeAlias<T> {};

// TODO: Handle RValue references in typetraits
// template <typename T>
// struct RemoveReference<T&&> : RemoveReference<T> {};

/// Remove cv-qualifiers and one layer of references.
template <typename T>
struct RemoveCvRef : RemoveCv<typename RemoveReference<T>::Type> {};

// TODO: Handle rvalue references in type traits
// template <typename T>
// struct RemoveCvRef<T&&> : RemoveCvRef<T> {};

/// Remove one layer of non-cv-qualified pointers
template <typename T>
struct RemoveNonCvPointer : TypeAlias<T> {};

template <typename T>
struct RemoveNonCvPointer<T*> : TypeAlias<T> {};

/// Remove one layer of possibly cv-qualified pointers
template <typename T>
struct RemovePointer : RemoveNonCvPointer<typename RemoveCv<T>::Type> {};

///
/// Type reflection: statically query types
///

/// True if T and U are the exact same type. Does not remove qualifiers.
template <typename T, typename U>
struct IsSame : FalseConstant {};

template <typename T>
struct IsSame<T, T> : TrueConstant {};

/// true if T is a reference type.
template <typename T>
struct IsReference : FalseConstant {};

template <typename T>
struct IsReference<T&> : TrueConstant {};

// TODO: Handle rvalue references in type traits
// template <typename T>
// struct IsReference<T&&> : TrueConstant {};

/// Helper for IsPointer.
template <typename T>
struct IsNonCvPointer : FalseConstant {};

template <typename T>
struct IsNonCvPointer<T*> : TrueConstant {};

/// IsPointer<T>::VALUE is true if T is a (possibly cv qualified) primitive pointer type.
template <typename T>
struct IsPointer : IsNonCvPointer<typename RemoveCv<T>::Type> {};

/// IsVoid<T>::VALUE is true if T is (possibly cv qualified) void.
template <typename T>
struct IsVoid : IsSame<typename RemoveCv<T>::Type, void> {};

/// IsNonCvIntegral<T>::VALUE is true if T is a non-cv-qualified primitive integral type
///
/// Note: Unlike `std::is_integral`, any compiler-defined extended integer types are not
/// supported by IsNonCvIntegral.
template <typename T>
struct IsNonCvIntegral : FalseConstant {};

template <> struct IsNonCvIntegral<bool> : TrueConstant {};
template <> struct IsNonCvIntegral<char> : TrueConstant {};
template <> struct IsNonCvIntegral<signed char> : TrueConstant {};
template <> struct IsNonCvIntegral<unsigned char> : TrueConstant {};
template <> struct IsNonCvIntegral<short> : TrueConstant {};
template <> struct IsNonCvIntegral<int> : TrueConstant {};
template <> struct IsNonCvIntegral<long> : TrueConstant {};
template <> struct IsNonCvIntegral<long long> : TrueConstant {};
template <> struct IsNonCvIntegral<unsigned short> : TrueConstant {};
template <> struct IsNonCvIntegral<unsigned int> : TrueConstant {};
template <> struct IsNonCvIntegral<unsigned long> : TrueConstant {};
template <> struct IsNonCvIntegral<unsigned long long> : TrueConstant {};

/// IsIntegral<T>::VALUE is true if T is a (possibly cv-qualified) primitive integral type.
///
/// (see note for IsNonCvIntegral)
template <typename T>
struct IsIntegral : IsNonCvIntegral<typename RemoveCv<T>::Type> {};

/// IsNonCvFloatingPoint<T>::VALUE is true if T is a non-cv-qualified floating point type
template <typename T>
struct IsNonCvFloatingPoint : FalseConstant {};

template <> struct IsNonCvFloatingPoint<float> : TrueConstant {};
template <> struct IsNonCvFloatingPoint<double> : TrueConstant {};
template <> struct IsNonCvFloatingPoint<long double> : TrueConstant {};

/// IsFloatingPoint<T>::VALUE is tue if T is a (possibly cv-qualified) floating point type
template <typename T>
struct IsFloatingPoint : IsNonCvFloatingPoint<typename RemoveCv<T>::Type> {};

/// IsArithmetic<T>::VALUE is true if T is a (possibly cv-qualified) primtive integral or floating point type
template <typename T>
struct IsArithmetic : BoolConstant<IsIntegral<T>::VALUE || IsFloatingPoint<T>::VALUE> {};

///
/// Miscellaneous transformations
///

/// EnableIf<B, T>::Type will exist if-and-only-if B is true (a basic SFINAE construct)
template <bool B, typename T = void>
struct EnableIf {};

template <typename T>
struct EnableIf<true, T>
{
	typedef T Type;
};
}  // namespace OMR

#endif // OMR_TYPETRAITS_HPP_
