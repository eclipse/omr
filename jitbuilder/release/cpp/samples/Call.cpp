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

#include "Call.hpp"

static CallFunctionType2Arg *nativeToJitMethod = NULL;

static int32_t
nativeMethod(int32_t x, int32_t y)
   {
   #define NATIVE_METHOD_LINE LINETOSTR(__LINE__)
   int32_t result = nativeToJitMethod(x, y);
   fprintf(stderr,"(%d + %d) * 2 == %d\n", x, y, result);
   return result;
   }

JitToNativeCallMethod::JitToNativeCallMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_jit_to_native");
   DefineParameter("x", Int32);
   DefineParameter("y", Int32);
   DefineReturnType(Int32);

   DefineFunction((char *)"nativeMethod",
                  (char *)__FILE__,
                  (char *)NATIVE_METHOD_LINE,
                  (void *)&nativeMethod,
                  Int32,
                  2,
                  Int32,
                  Int32);
   }

bool
JitToNativeCallMethod::buildIL()
   {
   Store("doublesum",
      Call("nativeMethod", 2,
         Load("x"),
         Load("y")));

   Return(
      Load("doublesum"));

   return true;
   }

JitToNativeComputedCallMethod::JitToNativeComputedCallMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_jit_to_native_computed");
   DefineParameter("x", Int32);
   DefineParameter("y", Int32);
   DefineReturnType(Int32);

   DefineFunction((char *)"nativeMethod",
                  (char *)__FILE__,
                  (char *)NATIVE_METHOD_LINE,
                  (void *)&nativeMethod,
                  Int32,
                  2,
                  Int32,
                  Int32);
   }

bool
JitToNativeComputedCallMethod::buildIL()
   {
   Store("doublesum",
      ComputedCall("nativeMethod", 3,
         ConstAddress((void*) &nativeMethod),
         Load("x"),
         Load("y")));

   Return(
      Load("doublesum"));

   return true;
   }

NativeToJitCallMethod::NativeToJitCallMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_native_to_jit");
   DefineParameter("x", Int32);
   DefineParameter("y", Int32);
   DefineReturnType(Int32);
   }

bool
NativeToJitCallMethod::buildIL()
   {
   Store("doublesum",
      Mul(
         Add(
            Load("x"),
            Load("y")),
         ConstInt32(2)));

   Return(
      Load("doublesum"));

   return true;
   }

JitToJitCallMethod::JitToJitCallMethod(OMR::JitBuilder::TypeDictionary *types, OMR::JitBuilder::MethodBuilder *jitMethodBuilder)
   : OMR::JitBuilder::MethodBuilder(types), jitMethodBuilder(jitMethodBuilder)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_jit_to_jit");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
JitToJitCallMethod::buildIL()
   {
   Store("sum",
      ConstInt32(0));

   IlBuilder *loop = NULL;
   ForLoopUp("i", &loop,
             ConstInt32(0),
             Load("n"),
             ConstInt32(1));

   loop->Store("sum",
   loop->   Call(jitMethodBuilder, 2,
   loop->      Load("sum"),
   loop->      Load("i")));

   Return(
      Load("sum"));

   return true;
   }

#undef NATIVE_METHOD_LINE

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

   void *entry = 0;

   printf("Step 3: compile JitToNativeCall method builder\n");
   JitToNativeCallMethod jitToNativeCallMethod(&types);
   int32_t rc = compileMethodBuilder(&jitToNativeCallMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   CallFunctionType2Arg *jitToNativeMethod = (CallFunctionType2Arg*)entry;

   printf("Step 4: compile JitToNativeComputedCall method builder\n");
   JitToNativeComputedCallMethod jitToNativeComputedCallMethod(&types);
   rc = compileMethodBuilder(&jitToNativeComputedCallMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   CallFunctionType2Arg *jitToNativeComputedMethod = (CallFunctionType2Arg*)entry;

   printf("Step 5: compile NativeToJitCall method builder\n");
   NativeToJitCallMethod nativeToJitCallMethod(&types);
   rc = compileMethodBuilder(&nativeToJitCallMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   nativeToJitMethod = (CallFunctionType2Arg*)entry;

   printf("Step 6: compile JitToJitCall (direct call to native) method builder\n");
   JitToJitCallMethod jitToJitCallMethod(&types, &jitToNativeCallMethod);
   rc = compileMethodBuilder(&jitToJitCallMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   CallFunctionType1Arg *jitToJitMethod = (CallFunctionType1Arg *)entry;

   printf("Step 7: invoke compiled JitToJitCall (direct call to native) method\n");
   for (int32_t n = 0;n < 10; n++)
      printf("jitToJitMethod(%d) = %d\n", n, jitToJitMethod(n));

   printf("Step 8: compile JitToJitCall (computed/indirect call to native) method builder\n");
   JitToJitCallMethod jitToJitComputedCallMethod(&types, &jitToNativeComputedCallMethod);
   rc = compileMethodBuilder(&jitToJitComputedCallMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   CallFunctionType1Arg *jitToJitComputedMethod = (CallFunctionType1Arg *)entry;

   printf("Step 9: invoke compiled JitToJitCall (computed/indirect call to native) method\n");
   for (int32_t n = 0;n < 10; n++)
      printf("jitToJitComputedMethod(%d) = %d\n", n, jitToJitComputedMethod(n));

   printf ("Step 10: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
