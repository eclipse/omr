/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <cstddef>

#include "FieldAddress.hpp"


GetStructFieldAddressBuilder::GetStructFieldAddressBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   pStructType = d->PointerTo(d->LookupStruct("Struct"));

   DefineName("getStructFieldAddress");
   DefineParameter("s", pStructType);
   DefineReturnType(d->PointerTo(d->GetFieldType("Struct", "f2")));
   }

bool
GetStructFieldAddressBuilder::buildIL()
   {
   Return(
          StructFieldInstanceAddress("Struct", "f2",
                             Load("s")));

   return true;
   }

GetUnionFieldAddressBuilder::GetUnionFieldAddressBuilder(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   pUnionType = d->PointerTo(d->LookupUnion("Union"));

   DefineName("getUnionFieldAddress");
   DefineParameter("u", pUnionType);
   DefineReturnType(d->toIlType<uint8_t *>());
   }

bool
GetUnionFieldAddressBuilder::buildIL()
   {
   Return(
          UnionFieldInstanceAddress("Union", "f2",
                             Load("u")));

   return true;
   }

class StructTypeDictionary : public OMR::JitBuilder::TypeDictionary
   {
   public:
   StructTypeDictionary()
      : OMR::JitBuilder::TypeDictionary()
      {
      DEFINE_STRUCT(Struct);
      DEFINE_FIELD(Struct, f1, toIlType<uint16_t>());
      DEFINE_FIELD(Struct, f2, toIlType<uint8_t>());
      CLOSE_STRUCT(Struct);
      }
   };

class UnionTypeDictionary : public OMR::JitBuilder::TypeDictionary
   {
   public:
   UnionTypeDictionary()
      : OMR::JitBuilder::TypeDictionary()
      {
      DefineUnion("Union");
      UnionField("Union", "f1", toIlType<uint16_t>());
      UnionField("Union", "f2", toIlType<uint8_t>());
      CloseUnion("Union");
      }
   };


int
main(int argc, char *argv[])
   {
   printf("Step 1: initialize JIT\n");
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }

   printf("Step 2: define type dictionaries\n");
   StructTypeDictionary structTypes;
   UnionTypeDictionary unionTypes;

   printf("Step 3: compile getStructFieldAddress builder\n");
   GetStructFieldAddressBuilder getStructFieldAddressBuilder(&structTypes);
   void *getStructFieldAddressEntry;
   int32_t rc = compileMethodBuilder(&getStructFieldAddressBuilder, &getStructFieldAddressEntry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: invoke compiled code for getStructFieldAddress and verify results\n");
   Struct s;
   s.f1 = 1;
   s.f2 = 2;
   auto getStructFieldAddress = (GetStructFieldAddressFunction *) getStructFieldAddressEntry;
   auto structFieldAddress = getStructFieldAddress(&s);
   assert(&(s.f2) == structFieldAddress);

   printf("Step 5: compile getUnionFieldAddress builder\n");
   GetUnionFieldAddressBuilder getUnionFieldAddressBuilder(&unionTypes);
   void *getUnionFieldAddressEntry;
   rc = compileMethodBuilder(&getUnionFieldAddressBuilder, &getUnionFieldAddressEntry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 6: invoke compiled code for getUnionFieldAddress and verify results\n");
   Union u;
   u.f2 = 2;
   auto getUnionFieldAddress = (GetUnionFieldAddressFunction  *) getUnionFieldAddressEntry;
   auto unionFieldAddress = getUnionFieldAddress(&u);
   assert(&(u.f2) == unionFieldAddress);

   printf ("Step 7: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
