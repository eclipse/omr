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
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "AtomicOperations.hpp"

using std::cout;
using std::cerr;

int
main(int argc, char *argv[])
   {
   cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      cerr << "FAIL: could not initialize JIT\n";
      exit(-1);
      }

   cout << "Step 2: define type dictionary\n";
   OMR::JitBuilder::TypeDictionary types;

   cout << "Step 3: compile 32-bit method builders\n";
   AtomicInt32Add method(&types);
   void *entry32 = 0;
   int32_t rc = compileMethodBuilder(&method, &entry32);
   if (rc != 0)
      {
      cerr << "First method FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 4: start testing 32-bit atomic increment\n";
   typedef int32_t (Atomic32MethodFunction)(int32_t*);
   Atomic32MethodFunction *increment32 = (Atomic32MethodFunction *) entry32;

   int32_t v;
   v=0;   cout << "atomic increment32(" << v; cout << ") == " << increment32(&v) << "\n";
   v=1;   cout << "atomic increment32(" << v; cout << ") == " << increment32(&v) << "\n";
   v=10;  cout << "atomic increment32(" << v; cout << ") == " << increment32(&v) << "\n";
   v=-15; cout << "atomic increment32(" << v; cout << ") == " << increment32(&v) << "\n";

   cout << "Step 5: compile 64-bit method builders\n";
   AtomicInt64Add method2(&types);
   void *entry64 = 0;
   int32_t rsc = compileMethodBuilder(&method2, &entry64);
   if (rsc != 0)
      {   
      cerr << "Second method FAIL: compilation error " << rsc << "\n";
      exit(-2); 
      } 

   cout << "Step 6: start testing 64-bit atomic increment\n";
   typedef int64_t (Atomic64MethodFunction)(int64_t*);
   Atomic64MethodFunction *increment64 = (Atomic64MethodFunction *) entry64;

   int64_t val;
   val=0;   cout << "atomic increment64(" << val; cout << ") == " << increment64(&val) << "\n";
   val=1;   cout << "atomic increment64(" << val; cout << ") == " << increment64(&val) << "\n";
   val=10;  cout << "atomic increment64(" << val; cout << ") == " << increment64(&val) << "\n";
   val=-15; cout << "atomic increment64(" << val; cout << ") == " << increment64(&val) << "\n";

   cout << "Step 7: shutdown JIT\n";
   shutdownJit();
   }

AtomicInt32Add::AtomicInt32Add(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("increment");
   pInt32=d->PointerTo(Int32);
   DefineParameter("addressOfValue", pInt32);
   DefineReturnType(Int32);
   }

bool
AtomicInt32Add::buildIL()
   {
   cout << "AtomicInt32Add::buildIL() running!\n";
   AtomicAdd(
           IndexAt(pInt32, Load("addressOfValue"), ConstInt32(0)),
           ConstInt32(1));
   Return(LoadAt(pInt32, Load("addressOfValue")));
   
   return true;
   }

AtomicInt64Add::AtomicInt64Add(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {   
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("increment64");
   pInt64=d->PointerTo(Int64);
   DefineParameter("addressOfValue", pInt64);
   DefineReturnType(Int64);
   }

bool
AtomicInt64Add::buildIL()
   {   
   cout << "AtomicInt64Add::buildIL() running!\n";
   AtomicAdd(
           IndexAt(pInt64, Load("addressOfValue"), ConstInt64(0)),
           ConstInt64(1));
   Return(LoadAt(pInt64, Load("addressOfValue")));
   
   return true;
   }
