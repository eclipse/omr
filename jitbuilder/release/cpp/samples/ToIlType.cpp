/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
 
#include <iostream>
#include <cstdint>
#include <cassert>

#include "JitBuilder.hpp"

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

   std::cout << "Step 2: create TypeDictionary instance\n";
   OMR::JitBuilder::TypeDictionary d;

   std::cout << "Step 3: test signed integral types\n";
   assert(d.Int8 == d.toIlType<int8_t>()->primitiveType(&d));
   assert(d.Int16 == d.toIlType<int16_t>()->primitiveType(&d));
   assert(d.Int32 == d.toIlType<int32_t>()->primitiveType(&d));
   assert(d.Int64 == d.toIlType<int64_t>()->primitiveType(&d));

   std::cout << "Step 4: test unsigned integral types\n";
   assert(d.Int8 == d.toIlType<uint8_t>()->primitiveType(&d));
   assert(d.Int16 == d.toIlType<uint16_t>()->primitiveType(&d));
   assert(d.Int32 == d.toIlType<uint32_t>()->primitiveType(&d));
   assert(d.Int64 == d.toIlType<uint64_t>()->primitiveType(&d));

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
   assert(d.Float == d.toIlType<float>()->primitiveType(&d));
   assert(d.Double == d.toIlType<double>()->primitiveType(&d));

   std::cout << "Step 8: test cv qualified types\n";
   assert(d.Int8 == d.toIlType<const int8_t>()->primitiveType(&d));
   assert(d.Int16 == d.toIlType<volatile uint16_t>()->primitiveType(&d));
   assert(d.Int32 == d.toIlType<const volatile int32_t>()->primitiveType(&d));

   std::cout << "Step 9: test void type\n";
   assert(d.NoType == d.toIlType<void>()->primitiveType(&d));

   std::cout << "Step 10: test pointer to primitive types\n";
   auto pInt32_type = d.toIlType<int32_t*>();
   assert(true == pInt32_type->isPointer());
   assert(d.Int32 == pInt32_type->baseType()->primitiveType(&d));

   auto pDouble_type = d.toIlType<double*>();
   assert(true == pDouble_type->isPointer());
   assert(d.Double == pDouble_type->baseType()->primitiveType(&d));

   auto pVoid_type = d.toIlType<void*>();
   assert(true == pVoid_type->isPointer());
   assert(d.NoType == pVoid_type->baseType()->primitiveType(&d));

   std::cout << "Step 11: test pointer to pointer to primitive types\n";
   auto ppInt32_type = d.toIlType<int32_t**>();
   assert(true == ppInt32_type->isPointer());
   assert(true == ppInt32_type->baseType()->isPointer());
   assert(d.Int32 == ppInt32_type->baseType()->baseType()->primitiveType(&d));

   auto ppDouble_type = d.toIlType<double**>();
   assert(true == ppDouble_type->isPointer());
   assert(true == ppDouble_type->baseType()->isPointer());
   assert(d.Double == ppDouble_type->baseType()->baseType()->primitiveType(&d));

   auto ppVoid_type = d.toIlType<void**>();
   assert(true == ppVoid_type->isPointer());
   assert(true == ppVoid_type->baseType()->isPointer());
   assert(d.NoType == ppVoid_type->baseType()->baseType()->primitiveType(&d));

#ifdef EXPECTED_FAIL
   /* Note: If enabled, the following tests should result in compilation errors. */

   // test enum types
   assert(d.Int8 == d.toIlType<Int8Enum>()->primitiveType(&d));
   assert(d.Int64 == d.toIlType<Int64Enum>()->primitiveType(&d));

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
