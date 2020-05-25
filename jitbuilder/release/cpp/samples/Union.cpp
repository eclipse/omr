/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
#include <assert.h>

#include "Union.hpp"

SetUnionByteBuilder::SetUnionByteBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineFile(__FILE__);
   DefineLine(LINETOSTR(__LINE__));

   DefineName("setUnionByte");
   DefineParameter("u", d->PointerTo(d->LookupUnion("TestUnionInt8pChar")));
   DefineParameter("v", Int8);
   DefineReturnType(NoType);
   }

bool
SetUnionByteBuilder::buildIL()
   {
   StoreIndirect("TestUnionInt8pChar", "v_uint8",
                 Load("u"),
                 Load("v"));
   Return();
   return true;
   }

GetUnionByteBuilder::GetUnionByteBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineFile(__FILE__);
   DefineLine(LINETOSTR(__LINE__));

   DefineName("getUnionByte");
   DefineParameter("u", d->PointerTo(d->LookupUnion("TestUnionInt8pChar")));
   DefineReturnType(Int8);
   }

bool
GetUnionByteBuilder::buildIL()
   {
   Return(
          LoadIndirect("TestUnionInt8pChar", "v_uint8",
                        Load("u")));

   return true;
   }

SetUnionStrBuilder::SetUnionStrBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineFile(__FILE__);
   DefineLine(LINETOSTR(__LINE__));

   DefineName("setUnionStr");
   DefineParameter("u", d->PointerTo(d->LookupUnion("TestUnionInt8pChar")));
   DefineParameter("v", d->toIlType<char*>());
   DefineReturnType(NoType);
   }

bool
SetUnionStrBuilder::buildIL()
   {
   StoreIndirect("TestUnionInt8pChar", "v_pchar",
                 Load("u"),
                 Load("v"));
   Return();
   return true;
   }

GetUnionStrBuilder::GetUnionStrBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineFile(__FILE__);
   DefineLine(LINETOSTR(__LINE__));

   DefineName("getUnionStr");
   DefineParameter("u", d->PointerTo(d->LookupUnion("TestUnionInt8pChar")));
   DefineReturnType(d->toIlType<char*>());
   }

bool
GetUnionStrBuilder::buildIL()
   {
   Return(
          LoadIndirect("TestUnionInt8pChar", "v_pchar",
                        Load("u")));

   return true;
   }

TypePunInt32Int32Builder::TypePunInt32Int32Builder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("typePunInt32Int32Builder");
   DefineParameter("u", d->PointerTo(d->LookupUnion("TestUnionInt32Int32")));
   DefineParameter("v1", Int32);
   DefineParameter("v2", Int32);
   DefineReturnType(Int32);
   }

bool
TypePunInt32Int32Builder::buildIL()
   {
   StoreIndirect("TestUnionInt32Int32", "f1", Load("u"), Load("v1"));
   StoreIndirect("TestUnionInt32Int32", "f2", Load("u"), Load("v2"));
   Return(
          LoadIndirect("TestUnionInt32Int32", "f1", Load("u")));
   return true;
   }

TypePunInt16DoubleBuilder::TypePunInt16DoubleBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("typePunInt16Double");
   DefineParameter("u", d->PointerTo(d->LookupUnion("TestUnionInt16Double")));
   DefineParameter("v1", Int16);
   DefineParameter("v2", Double);
   DefineReturnType(Int16);
   }

bool
TypePunInt16DoubleBuilder::buildIL()
   {
   StoreIndirect("TestUnionInt16Double", "v_uint16", Load("u"), Load("v1"));
   StoreIndirect("TestUnionInt16Double", "v_double", Load("u"), Load("v2"));
   Return(
          LoadIndirect("TestUnionInt16Double", "v_uint16", Load("u")));
   return true;
   }

template <typename Function>
static Function assert_compile(OMR::JitBuilder::MethodBuilder* m)
   {
   void* entry;
   int32_t rc = compileMethodBuilder(m, &entry);
   if (rc != 0)
      {
      fprintf(stderr, "FAIL: could not compile MethodBuilder\n");
      exit(-1);
      }
   return (Function)entry;
   }

int
main()
   {
   std::cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   {
   std::cout << "Step 2: create UnionTypeDictionary\n";
   UnionTypeDictionary typeDictionary;

   std::cout << "Step 3: assert that the size of each union is the size of its largest member\n";
   assert(sizeof(char*) == typeDictionary.LookupUnion("TestUnionInt8pChar")->getSize());
   assert(sizeof(uint32_t) == typeDictionary.LookupUnion("TestUnionInt32Int32")->getSize());
   assert(sizeof(double) == typeDictionary.LookupUnion("TestUnionInt16Double")->getSize());
   }

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   {
   UnionTypeDictionary setUnionByteTypes;
   UnionTypeDictionary getUnionByteTypes;
   TestUnionInt8pChar testUnion;
   const uint8_t constval = 3;

   std::cout << "Step 4: compile setUnionByte\n";
   SetUnionByteBuilder setUnionByte(&setUnionByteTypes);
   auto setByte = assert_compile<SetUnionByteFunction>(&setUnionByte);

   std::cout << "Step 5: invoke setUnionByte\n";
   setByte(&testUnion, constval);
   assert(constval == testUnion.v_uint8);

   std::cout << "Step 6: compile getUnionByte\n";
   GetUnionByteBuilder getUnionByte(&getUnionByteTypes);
   auto getByte = assert_compile<GetUnionByteFunction>(&getUnionByte);

   std::cout << "Step 7: invoke getUnionByte\n";
   auto expectedByte = testUnion.v_uint8;
   assert(expectedByte == getByte(&testUnion));
   }

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   {
   UnionTypeDictionary setUnionStrTypes;
   UnionTypeDictionary getUnionStrTypes;
   TestUnionInt8pChar testUnion;
   const char* msg = "Hello World!\n";

   std::cout << "Step 8: compile setUnionStr\n";
   SetUnionStrBuilder setUnionStr(&setUnionStrTypes);
   auto setStr = assert_compile<SetUnionStrFunction>(&setUnionStr);

   std::cout << "Step 9: invoke setUnionStr\n";
   setStr(&testUnion, msg);
   assert(msg == testUnion.v_pchar);

   std::cout << "Step 10: compile getUnionStr\n";
   GetUnionStrBuilder getUnionStr(&getUnionStrTypes);
   auto getStr = assert_compile<GetUnionStrFunction>(&getUnionStr);

   std::cout << "Step 11: invoke getUnionStr\n";
   auto expectedStr = testUnion.v_pchar;
   assert(expectedStr == getStr(&testUnion));
   }

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   {
   UnionTypeDictionary typeDictionary;
   TestUnionInt32Int32 testUnion;
   const uint32_t v1 = 5;
   const uint32_t v2 = 10;

   std::cout << "Step 12: compile typePunInt32Int32\n";
   TypePunInt32Int32Builder builder(&typeDictionary);
   auto typePunInt32Int32 = assert_compile<TypePunInt32Int32Function>(&builder);

   std::cout << "Step 13: invoke typePunInt32Int32\n";
   assert(v2 == typePunInt32Int32(&testUnion, v1, v2));
   }

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   {
   UnionTypeDictionary typeDictionary;
   TestUnionInt16Double testUnion;
   const uint16_t v1 = 0xFFFF;
   const double v2 = +0.0;

   std::cout << "Step 14: compile typePunInt16Double\n";
   TypePunInt16DoubleBuilder builder(&typeDictionary);
   auto typePunInt16Dboule = assert_compile<TypePunInt16DoubleFunction>(&builder);

   std::cout << "Step 15: invoke typePunInt16Double\n";
   assert(static_cast<uint16_t>(v2) == typePunInt16Dboule(&testUnion, v1, v2));
   }

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   std::cout << "Step 16: shutdown JIT\n";
   shutdownJit();
   }
