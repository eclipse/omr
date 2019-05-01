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

#include "Pow2.hpp"

Pow2Method::Pow2Method(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("pow2");
   DefineParameter("n", Int64);
   DefineReturnType(Int64);
   }

bool
Pow2Method::buildIL()
   {
   Store("a",
      ConstInt64(1));

   Store("b",
      ConstInt64(1));

   Store("i",
      Load("n"));

   Store("keepIterating",
      GreaterThan(
         Load("i"),
         ConstInt64(-1)));

   OMR::JitBuilder::IlBuilder *loopBody = NULL;
   WhileDoLoop("keepIterating", &loopBody);

   loopBody->Store("a",
   loopBody->   Load("b"));

   loopBody->Store("b",
   loopBody->   Add(
   loopBody->      Load("a"),
   loopBody->      Load("b")));

   loopBody->Store("i",
   loopBody->   Sub(
   loopBody->      Load("i"),
   loopBody->      ConstInt64(1)));

   loopBody->Store("keepIterating",
   loopBody->   GreaterThan(
   loopBody->      Load("i"),
   loopBody->      ConstInt64(-1)));

   Return(
      Load("a"));

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
   Pow2Method Pow2Method(&types);
   void *entry=0;
   int32_t rc = compileMethodBuilder(&Pow2Method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: invoke compiled code\n");
   Pow2FunctionType *pow2 = (Pow2FunctionType *)entry;
   int32_t n = (argc > 1) ? atoi(argv[1]) : 6000000;
   int64_t r;
   for (int32_t i=0;i < n;i++)
      r = pow2((int64_t) 45);

   printf("pow2(45) is %lld\n", r);

   printf ("Step 5: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
