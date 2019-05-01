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

#include "Pointer.hpp"

static void printString(int64_t ptr)
   {
   #define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *str = (char *) ptr;
   printf("%s", str);
   }

static void printInt32(int32_t val)
   {
   #define PRINTINT32_LINE LINETOSTR(__LINE__)
   printf("%d", val);
   }

static void printFloat(float val)
   {
   #define PRINTFLOAT_LINE LINETOSTR(__LINE__)
   printf("%f", val);
   }

static void printDouble(double val)
   {
   #define PRINTDOUBLE_LINE LINETOSTR(__LINE__)
   printf("%lf", val);
   }

static void printPointer(void* val)
   {
   #define PRINTPOINTER_LINE LINETOSTR(__LINE__)
   printf("%p", val);
   }

int32_t PointerMethod::staticInt32 = 3;

PointerMethod::PointerMethod(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("testPointer");

   pInt32 = d->PointerTo(Int32);
   pFloat = d->PointerTo(Float);
   pDouble = d->PointerTo(Double);
   ppDouble = d->PointerTo(pDouble);

   DefineParameter("pInt32", pInt32);
   DefineParameter("pFloat", pFloat);
   DefineParameter("ppDouble", ppDouble);

   DefineReturnType(NoType);

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
   DefineFunction((char *)"printFloat",
                  (char *)__FILE__,
                  (char *)PRINTFLOAT_LINE,
                  (void *)&printFloat,
                  NoType,
                  1,
                  Float);
   DefineFunction((char *)"printDouble",
                  (char *)__FILE__,
                  (char *)PRINTDOUBLE_LINE,
                  (void *)&printDouble,
                  NoType,
                  1,
                  Double);
   DefineFunction((char *)"printPointer",
                  (char *)__FILE__,
                  (char *)PRINTPOINTER_LINE,
                  (void *)&printPointer,
                  NoType,
                  1,
                  Int64);
   }

void
PointerMethod::PrintString(OMR::JitBuilder::IlBuilder *bldr, const char *s)
   {
   bldr->Call("printString", 1,
   bldr->   ConstInt64((int64_t)(char *)s));
   }

bool
PointerMethod::buildIL()
   {
   PrintString(this, "Pointer parameters:\n");

   PrintString(this, "   pInt32[");
   Call("printPointer", 1,
      Load("pInt32"));
   PrintString(this, "] = ");
   Call("printInt32", 1,
      LoadAt(pInt32,
         Load("pInt32")));

   PrintString(this, "   pFloat[");
   Call("printPointer", 1,
      Load("pFloat"));
   PrintString(this, "] = ");
   Call("printFloat", 1,
      LoadAt(pFloat,
         Load("pFloat")));

   PrintString(this, "   ppDouble[");
   Call("printPointer", 1,
      Load("ppDouble"));
   PrintString(this, "] = ");
   Call("printDouble", 1,
      LoadAt(pDouble,
         LoadAt(ppDouble,
            Load("ppDouble"))));

   PrintString(this, "\nOther pointers:\n");

   PrintString(this, "   staticInt32[");
   Call("printPointer", 1,
      ConstAddress(&staticInt32));
   PrintString(this, "] = ");
   Call("printInt32", 1,
      LoadAt(pInt32,
         ConstAddress(&staticInt32)));

   PrintString(this, "\n");

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
   PointerMethod method(&types);
   void *entry;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: define values\n");
   PointerFunctionType *test = (PointerFunctionType *)entry;
   int32_t theI32=15;
   float theFloat=225.225;
   double theDouble=123.456789;
   double *pTheDouble = &theDouble;

   printf("Step 5: invoke compiled code and verify results\n");
   test(&theI32, &theFloat, &pTheDouble);

   printf ("Step 6: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
