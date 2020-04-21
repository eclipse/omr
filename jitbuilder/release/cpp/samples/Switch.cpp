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
#include <string.h>
#include <errno.h>

#include "Switch.hpp"

static void printString(int64_t ptr)
   {
   #define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *str = (char *) ptr;
   printf("%s", str);
   }

SwitchMethod::SwitchMethod(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("testSwitch");

   DefineParameter("selector", Int32);

   DefineReturnType(NoType);
   }

/**
 * Example to show how RequestFunction can be used to define functions
 * on demand, rather than up front. A perfectly viable approach (especially
 * in this sample) would be to call DefineFunction in the MethodBuilder's
 * constructor. But it is also possible to implement RequestFunction in
 * a MethodBuilder subclass to define functions when they are first called.
 * Typically used in JIT compilers rather than examples like this where
 * "printString" is always going to be called anyway.
 */
bool
SwitchMethod::RequestFunction(const char *name)
   {
   if (strncmp(name, "printString", 12) != 0)
      return false;

   DefineFunction((char *)"printString", 
                  (char *)__FILE__,
                  (char *)PRINTSTRING_LINE,
                  (void *)&printString,
                  NoType,
                  1,
                  Int64);

   return true;
   }

void
SwitchMethod::PrintString(OMR::JitBuilder::IlBuilder *bldr, const char *s)
   {
   bldr->Call("printString", 1,
   bldr->   ConstInt64((int64_t)(char *)s));
   }

bool
SwitchMethod::buildIL()
   {
   OMR::JitBuilder::IlBuilder *defaultBldr=NULL;
   OMR::JitBuilder::IlBuilder *case1Bldr=NULL, *case2Bldr=NULL, *case3Bldr=NULL;
   Switch(Load("selector"), &defaultBldr, 3,
          MakeCase(1, &case1Bldr, false),
          MakeCase(2, &case2Bldr, false),
          MakeCase(3, &case3Bldr, false));

   PrintString(case1Bldr, "\tcase 1 reached\n");

   PrintString(case2Bldr, "\tcase 2 reached\n");

   PrintString(case3Bldr, "\tcase 3 reached\n");

   PrintString(defaultBldr, "\tdefault case reached\n");

   Return();

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
   SwitchMethod method(&types);
   void *entry=0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: invoke compiled code and verify results\n");
   SwitchFunctionType *test = (SwitchFunctionType *)entry;
   int s;
   s=0;      printf("Test varargs switch (%d):", s); test(s);   // default
   s=1;      printf("Test varargs switch (%d):", s); test(s);   // case1
   s=2;      printf("Test varargs switch (%d):", s); test(s);   // case2
   s=3;      printf("Test varargs switch (%d):", s); test(s);   // case3
   s=4;      printf("Test varargs switch (%d):", s); test(s);   // default
   s=-9;     printf("Test varargs switch (%d):", s); test(s);   // default
   s=512288; printf("Test varargs switch (%d):", s); test(s);   // default

   printf ("Step 5: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
