/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
 
#include <iostream>
#include <cstdint>
#include <cassert>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"

// types for use in test cases
enum Int8Enum : int8_t { Field_8 };
enum Int64Enum : int64_t { Field_64 };
struct StructType { int field; };
union UnionType { StructType s; char field; };

int main()
   {
   std::cout << "Step 1: initialize JIT\n";
   bool jit_initialized = initializeJit();
   assert(jit_initialized);

   std::cout << "Step 2: create TR::TypeDictionary instance\n";
   TR::TypeDictionary d;

   std::cout << "Step 3: test signed integral types\n";
   assert(TR::Int8 == d.toIlType<int8_t>()->getPrimitiveType());
   assert(TR::Int16 == d.toIlType<int16_t>()->getPrimitiveType());
   assert(TR::Int32 == d.toIlType<int32_t>()->getPrimitiveType());
   assert(TR::Int64 == d.toIlType<int64_t>()->getPrimitiveType());

   std::cout << "Step 4: test unsigned integral types\n";
   assert(TR::Int8 == d.toIlType<uint8_t>()->getPrimitiveType());
   assert(TR::Int16 == d.toIlType<uint16_t>()->getPrimitiveType());
   assert(TR::Int32 == d.toIlType<uint32_t>()->getPrimitiveType());
   assert(TR::Int64 == d.toIlType<uint64_t>()->getPrimitiveType());

   std::cout << "Step 5: test language primitive signed integral types\n";
   assert(sizeof(char) == d.toIlType<char>()->getSize());
   assert(sizeof(short) == d.toIlType<short>()->getSize());
   assert(sizeof(int) == d.toIlType<int>()->getSize());
   assert(sizeof(long) == d.toIlType<long>()->getSize());
   assert(sizeof(long long) == d.toIlType<long long>()->getSize());

   std::cout << "Step 6: test language primitive unsigned integral types\n";
   assert(sizeof(unsigned char) == d.toIlType<unsigned char>()->getSize());
   assert(sizeof(unsigned short) == d.toIlType<unsigned short>()->getSize());
   assert(sizeof(unsigned int) == d.toIlType<unsigned int>()->getSize());
   assert(sizeof(unsigned long) == d.toIlType<unsigned long>()->getSize());
   assert(sizeof(unsigned long long) == d.toIlType<unsigned long long>()->getSize());

   std::cout << "Step 7: test floating point types\n";
   assert(TR::Float == d.toIlType<float>()->getPrimitiveType());
   assert(TR::Double == d.toIlType<double>()->getPrimitiveType());

   std::cout << "Step 8: test cv qualified types\n";
   assert(TR::Int8 == d.toIlType<const int8_t>()->getPrimitiveType());
   assert(TR::Int16 == d.toIlType<volatile uint16_t>()->getPrimitiveType());
   assert(TR::Int32 == d.toIlType<const volatile int32_t>()->getPrimitiveType());

   std::cout << "Step 9: test void type\n";
   assert(TR::NoType == d.toIlType<void>()->getPrimitiveType());

   std::cout << "Step 10: test pointer to primitive types\n";
   auto pInt32_type = d.toIlType<int32_t*>();
   assert(true == pInt32_type->isPointer());
   assert(TR::Int32 == pInt32_type->baseType()->getPrimitiveType());

   auto pDouble_type = d.toIlType<double*>();
   assert(true == pDouble_type->isPointer());
   assert(TR::Double == pDouble_type->baseType()->getPrimitiveType());

   auto pVoid_type = d.toIlType<void*>();
   assert(true == pVoid_type->isPointer());
   assert(TR::NoType == pVoid_type->baseType()->getPrimitiveType());

   std::cout << "Step 11: test pointer to pointer to primitive types\n";
   auto ppInt32_type = d.toIlType<int32_t**>();
   assert(true == ppInt32_type->isPointer());
   assert(true == ppInt32_type->baseType()->isPointer());
   assert(TR::Int32 == ppInt32_type->baseType()->baseType()->getPrimitiveType());

   auto ppDouble_type = d.toIlType<double**>();
   assert(true == ppDouble_type->isPointer());
   assert(true == ppDouble_type->baseType()->isPointer());
   assert(TR::Double == ppDouble_type->baseType()->baseType()->getPrimitiveType());

   auto ppVoid_type = d.toIlType<void**>();
   assert(true == ppVoid_type->isPointer());
   assert(true == ppVoid_type->baseType()->isPointer());
   assert(TR::NoType == ppVoid_type->baseType()->baseType()->getPrimitiveType());

#ifdef EXPECTED_FAIL
   /* Note: If enabled, the following tests should result in compilation errors. */

   // test enum types
   assert(TR::Int8 == d.toIlType<Int8Enum>()->getPrimitiveType());
   assert(TR::Int64 == d.toIlType<Int64Enum>()->getPrimitiveType());

   // test array type
   d.toIlType<int[10]>();

   // test struct type
   d.toIlType<StructType>();

   // test pointer to struct type
   d.toIlType<StructType*>();

   // test pointer to pointer to struct type
   d.toIlType<StructType**>();

   // test union type
   d.toIlType<UnionType>();

   // test pointer to union type
   d.toIlType<UnionType*>();

   // test pointer to pointer to union type
   d.toIlType<UnionType**>();

#endif // defined(EXPECTED_FAIL)

   std::cout << "Step 12: shutdown JIT\n";
   shutdownJit();
   
   std::cout << "PASS\n";
   }
