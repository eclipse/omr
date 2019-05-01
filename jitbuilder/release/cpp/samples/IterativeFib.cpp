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

#include "IterativeFib.hpp"

IterativeFibonnaciMethod::IterativeFibonnaciMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_iter"); // defines _method
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
IterativeFibonnaciMethod::buildIL()
   {
   OMR::JitBuilder::IlBuilder *returnN = NULL;
   IfThen(&returnN,
      LessThan(
         Load("n"),
         ConstInt32(2)));

   returnN->Return(
   returnN->   Load("n"));

   Store("LastSum",
      ConstInt32(0));

   Store("Sum",
      ConstInt32(1));

   IlBuilder *iloop = NULL;
   ForLoopUp("i", &iloop,
           ConstInt32(1),
           Load("n"),
           ConstInt32(1));

   iloop->Store("tempSum",
   iloop->   Add(
   iloop->      Load("Sum"),
   iloop->      Load("LastSum")));
   iloop->Store("LastSum",
   iloop->   Load("Sum"));
   iloop->Store("Sum",
   iloop->   Load("tempSum"));

   Return(
      Load("Sum"));

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

   printf("Step 3: compile method builder\n");
   IterativeFibonnaciMethod iterFibMethodBuilder(&types);
   void *entry=0;
   int32_t rc = compileMethodBuilder(&iterFibMethodBuilder, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: invoke compiled code\n");
   IterativeFibFunctionType *iter_fib=(IterativeFibFunctionType *)entry;
   for (int32_t n=0;n < 20;n++)
      printf("fib(%2d) = %d\n", n, iter_fib(n));

   printf ("Step 5: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
