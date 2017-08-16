/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
#include <string>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "Call.hpp"

static int32_t
doublesum(int32_t first, int32_t second)
   {
   #define DOUBLESUM_LINE LINETOSTR(__LINE__)
   int32_t result = 2 * (first + second);
   fprintf(stderr,"doublesum(%d, %d) == %d\n", first, second, result);
   return result;
   }

CallMethod::CallMethod(TR::TypeDictionary *types)
   : MethodBuilder(types)
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

ComputedCallMethod::ComputedCallMethod(TR::TypeDictionary *types)
   : MethodBuilder(types)
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
   TR::TypeDictionary types;

   printf("Step 3: compile Call method builder\n");
   CallMethod method(&types);
   uint8_t *entry=0;
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
