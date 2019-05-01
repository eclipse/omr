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


static void
thunk_1b(int8_t b)
   {
   std::cout << "\tthunk_1b: b is " << b << "\n";
   return;
   }

static void
thunk_1b_2(int8_t b)
   {
   std::cout << "\tthunk_1b_2: b-1 is " << (char)(b-1) << "\n";
   return;
   }

static void
thunk_1s(int16_t s)
   {
   std::cout << "\tthunk_1s: s is " << s << "\n";
   return;
   }

static void
thunk_1s_2(int16_t s)
   {
   std::cout << "\tthunk_1s_2: s-1 is " << (s-1) << "\n";
   return;
   }

static void
thunk_1i(int32_t i)
   {
   std::cout << "\tthunk_1i: i is " << i << "\n";
   return;
   }

static void
thunk_1i_2(int32_t i)
   {
   std::cout << "\tthunk_1i_2: i-1 is " << (i-1) << "\n";
   return;
   }

static void
thunk_1l(int64_t l)
   {
   std::cout << "\tthunk_1l: l is " << l << "\n";
   return;
   }

static void
thunk_1l_2(int64_t l)
   {
   std::cout << "\tthunk_1l_2: l-1 is " << (l-1) << "\n";
   return;
   }

typedef void (VoidThunkType)(void *, uintptr_t *);

int main(int argc, char *argv[])
   {
   std::cout << "Step 1: initialize JIT\n";
   bool jit_initialized = initializeJit();
   assert(jit_initialized);

   std::cout << "Step 2: create TypeDictionary instance\n";
   OMR::JitBuilder::TypeDictionary d;

   uint32_t rc;
   void *startPC;

   std::cout << "Step 3: test primitive thunk returning void\n";

   OMR::JitBuilder::IlType *NoType = d.NoType;

   OMR::JitBuilder::IlType *parmTypes_1b[] = { d.toIlType<int8_t>() };
   OMR::JitBuilder::ThunkBuilder thunk1b(&d, "1b", NoType, 1, parmTypes_1b);
   rc = compileMethodBuilder(&thunk1b, &startPC);
   if (rc == 0)
      {
      VoidThunkType *f = (VoidThunkType *)startPC;
      uintptr_t args[] = { (uintptr_t) 'x' };
      f((void*)&thunk_1b, args);
      f((void*)&thunk_1b_2, args);
      }

   OMR::JitBuilder::IlType *parmTypes_1s[] = { d.toIlType<int16_t>() };
   OMR::JitBuilder::ThunkBuilder thunk1s(&d, "1s", NoType, 1, parmTypes_1s);
   rc = compileMethodBuilder(&thunk1s, &startPC);
   if (rc == 0)
      {
      VoidThunkType *f = (VoidThunkType *)startPC;
      uintptr_t args[] = { (uintptr_t) 4097 };
      f((void*)&thunk_1s, args);
      f((void*)&thunk_1s_2, args);
      }

   OMR::JitBuilder::IlType *parmTypes_1i[] = { d.toIlType<int32_t>() };
   OMR::JitBuilder::ThunkBuilder thunk1i(&d, "1i", NoType, 1, parmTypes_1i);
   rc = compileMethodBuilder(&thunk1i, &startPC);
   if (rc == 0)
      {
      VoidThunkType *f = (VoidThunkType *)startPC;
      uintptr_t args[] = { (uintptr_t) 1233475 };
      f((void*)&thunk_1i, args);
      f((void*)&thunk_1i_2, args);
      }

   OMR::JitBuilder::IlType *parmTypes_1l[] = { d.toIlType<int64_t>() };
   OMR::JitBuilder::ThunkBuilder thunk1l(&d, "1l", NoType, 1, parmTypes_1l);
   rc = compileMethodBuilder(&thunk1l, &startPC);
   if (rc == 0)
      {
      VoidThunkType *f = (VoidThunkType *)startPC;
      uintptr_t args[] = { (uintptr_t) 1233475 };
      f((void*)&thunk_1l, args);
      f((void*)&thunk_1l_2, args);
      }

   std::cout << "Step 4: shutdown JIT\n";
   shutdownJit();
   
   std::cout << "PASS\n";
   }
