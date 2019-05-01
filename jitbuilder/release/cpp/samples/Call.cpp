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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "Call.hpp"

static int32_t
doublesum(int32_t first, int32_t second)
   {
   #define DOUBLESUM_LINE LINETOSTR(__LINE__)
   int32_t result = 2 * (first + second);
   fprintf(stderr,"doublesum(%d, %d) == %d\n", first, second, result);
   return result;
   }

CallMethod::CallMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_calls");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);

   DefineFunction((char *)"doublesum",
                  (char *)__FILE__,
                  (char *)DOUBLESUM_LINE,
                  (void *)&doublesum,
                  Int32,
                  2,
                  Int32,
                  Int32);
   }

bool
CallMethod::buildIL()
   {
   Store("sum",
      ConstInt32(0));

   IlBuilder *loop=NULL;
   ForLoopUp("i", &loop,
             ConstInt32(0),
             Load("n"),
             ConstInt32(1));

   loop->Store("sum",
   loop->   Call("doublesum", 2,
   loop->      Load("sum"),
   loop->      Load("i")));

   Return(
      Load("sum"));

   return true;
   }

ComputedCallMethod::ComputedCallMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_computed_call");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);

   DefineFunction((char *)"doublesum",
                  (char *)__FILE__,
                  (char *)DOUBLESUM_LINE,
                  (void *)&doublesum,
                  Int32,
                  2,
                  Int32,
                  Int32);
   }
#undef DOUBLESUM_LINE

bool
ComputedCallMethod::buildIL()
   {
   Store("sum",
      ConstInt32(0));

   IlBuilder *loop=NULL;
   ForLoopUp("i", &loop,
             ConstInt32(0),
             Load("n"),
             ConstInt32(1));

   loop->Store("sum",
   loop->   ComputedCall("doublesum", 3,
   loop->      ConstAddress((void*) &doublesum),
   loop->      Load("sum"),
   loop->      Load("i")));

   Return(
      Load("sum"));

   return true;
   }

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

   printf("Step 2: define type dictionary\n");
   OMR::JitBuilder::TypeDictionary types;

   printf("Step 3: compile Call method builder\n");
   CallMethod method(&types);
   void *entry=0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: invoke compiled Call method\n");
   CallFunctionType *call=(CallFunctionType *)entry;
   for (int32_t n=0;n < 10;n++)
      printf("call(%2d) = %d\n", n, call(n));

   printf("Step 5: compile ComputedCall method builder\n");
   ComputedCallMethod computedCallMethod(&types);
   rc = compileMethodBuilder(&computedCallMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 6: invoke compiled ComputedCall method\n");
   call=(CallFunctionType *)entry;
   for (int32_t n=0;n < 10;n++)
      printf("call(%2d) = %d\n", n, call(n));

   printf ("Step 7: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
