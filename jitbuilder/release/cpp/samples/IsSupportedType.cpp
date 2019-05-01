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
struct StructType { int field; };
union UnionType { StructType s; char field; };

int main()
   {
   /* Note: In an actual project, `static_assert` should be used in
    * favour of `assert`. This is not done here for testability
    * purposes.
    */

   std::cout << "Step 1: test signed integral types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int8_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int16_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int32_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int64_t>::value);

   std::cout << "Step 2: test unsigned integral types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<uint8_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<uint16_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<uint32_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<uint64_t>::value);

   std::cout << "Step 3: test language primitive signed integral types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<char>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<short>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<long>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<long long>::value);

   std::cout << "Step 4: test language primitive unsigned integral types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<unsigned char>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<unsigned short>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<unsigned int>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<unsigned long>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<unsigned long long>::value);

   std::cout << "Step 5: test floating point types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<float>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<double>::value);

   std::cout << "Step 6: test cv qualified types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<const int8_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<volatile uint16_t>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<const volatile int32_t>::value);

   std::cout << "Step 7: test void type\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<void>::value);

   std::cout << "Step 8: test pointer to primitive types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int32_t*>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<double*>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<void*>::value);

   std::cout << "Step 9: test pointer to pointer to primitive types\n";
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<int32_t**>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<double**>::value);
   assert(true == OMR::JitBuilder::TypeDictionary::is_supported<void**>::value);

   std::cout << "Step 10: test unsupported types\n";
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<int[10]>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<Int8Enum>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<StructType>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<UnionType>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<StructType*>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<UnionType*>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<StructType**>::value);
   assert(false == OMR::JitBuilder::TypeDictionary::is_supported<UnionType**>::value);

   std::cout << "PASS\n";
   }
