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

#include "Simple.hpp"

using std::cout;
using std::cerr;

#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)


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

   cout << "Step 3: compile method builder\n";
   SimpleMethod method(&types);
   void *entry = 0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 4: invoke compiled code and print results\n";
   typedef int32_t (SimpleMethodFunction)(int32_t);
   SimpleMethodFunction *increment = (SimpleMethodFunction *) entry;

   int32_t v;
   v=0;   cout << "increment(" << v << ") == " << increment(v) << "\n";
   v=1;   cout << "increment(" << v << ") == " << increment(v) << "\n";
   v=10;  cout << "increment(" << v << ") == " << increment(v) << "\n";
   v=-15; cout << "increment(" << v << ") == " << increment(v) << "\n";

   cout << "Step 5: shutdown JIT\n";
   shutdownJit();
   }



SimpleMethod::SimpleMethod(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d, (OMR::JitBuilder::VirtualMachineState *) NULL)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("increment");
   DefineParameter("value", Int32);
   DefineReturnType(Int32);
   }

bool
SimpleMethod::buildIL()
   {
   cout << "SimpleMethod::buildIL() running!\n";
   Return(
      Add(
         Load("value"),
         ConstInt32(1)));

   return true;
   }
