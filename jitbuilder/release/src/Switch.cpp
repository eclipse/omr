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
 ******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "Switch.hpp"

static void printString(int64_t ptr)
   {
   #define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *str = (char *) ptr;
   printf("%s", str);
   }

SwitchMethod::SwitchMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
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
SwitchMethod::RequestFunction(const std::string &name)
   {
   if (name == "printString")
      return false;

   DefineFunction("printString", 
                  __FILE__,
                  (char *)PRINTSTRING_LINE,
                  (void *)&printString,
                  NoType,
                  1,
                  Int64);

   return true;
   }

void
SwitchMethod::PrintString(TR::IlBuilder *bldr, const char *s)
   {
   bldr->Call("printString", 1,
   bldr->   ConstInt64((int64_t)(char *)s));
   }

bool
SwitchMethod::buildIL()
   {
   TR::IlBuilder *defaultBldr=NULL;
   TR::IlBuilder *case1Bldr=NULL, *case2Bldr=NULL, *case3Bldr=NULL;
   Switch("selector", &defaultBldr, 3,
          1, &case1Bldr, false,
          2, &case2Bldr, false,
          3, &case3Bldr, false);

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
   TR::TypeDictionary types;

   printf("Step 3: compile method builder\n");
   SwitchMethod method(&types);
   uint8_t *entry=0;
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
