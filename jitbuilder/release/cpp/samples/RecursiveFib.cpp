/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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

#include "RecursiveFib.hpp"

/* Un comment to enable debug output */
/* #define RFIB_DEBUG_OUTPUT */

static void
printString(int64_t stringPointer)
   {
   #define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *strPtr = (char *)stringPointer;
   fprintf(stderr, "%s", strPtr);
   }

static void
printInt32(int32_t value)
   {
   #define PRINTINT32_LINE LINETOSTR(__LINE__)
   fprintf(stderr, "%d", value);
   }

RecursiveFibonnaciMethod::RecursiveFibonnaciMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);

   DefineFunction((char *)"printString",
                  (char *)__FILE__,
                  (char *)PRINTSTRING_LINE,
                  (void *)&printString,
                  NoType,
                  1,
                  Int64);
   DefineFunction((char *)"printInt32",
                  (char *)__FILE__,
                  (char *)PRINTINT32_LINE,
                  (void *)&printInt32,
                  NoType,
                  1,
                  Int32);
   }

bool
RecursiveFibonnaciMethod::buildIL()
   {
   OMR::JitBuilder::IlBuilder *baseCase=NULL, *recursiveCase=NULL;
   IfThenElse(&baseCase, &recursiveCase,
      LessThan(
         Load("n"),
         ConstInt32(2)));

   DefineLocal("result", Int32);

   baseCase->Store("result",
   baseCase->   Load("n"));

   recursiveCase->Store("result",
   recursiveCase->   Add(
   recursiveCase->      Call("fib", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("n"),
   recursiveCase->            ConstInt32(1))),
   recursiveCase->      Call("fib", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("n"),
   recursiveCase->            ConstInt32(2)))));
   
#if defined(RFIB_DEBUG_OUTPUT)
   static const char *prefix = "fib(";
   static const char *middle = ") = ";
   static const char *suffix = "\n";

   Call("printString", 1,
      ConstInt64((int64_t)prefix));
   Call("printInt32", 1,
      Load("n"));
   Call("printString", 1,
      ConstInt64((int64_t)middle));
   Call("printInt32", 1,
      Load("result"));
   Call("printString", 1,
      ConstInt64((int64_t)suffix));
#endif

   Return(
      Load("result"));

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

   printf("Step 2: define relevant types\n");
   OMR::JitBuilder::TypeDictionary types;

   printf("Step 3: compile method builder\n");
   RecursiveFibonnaciMethod method(&types);
   void *entry=0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: invoke compiled code\n");
   RecursiveFibFunctionType *fib = (RecursiveFibFunctionType *)entry;
   for (int32_t n=0;n < 20;n++)
      printf("fib(%2d) = %d\n", n, fib(n));

   printf ("Step 5: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
