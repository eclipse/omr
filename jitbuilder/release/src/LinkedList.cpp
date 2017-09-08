/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "LinkedList.hpp"

static void printString(int64_t ptr)
   {
   #define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *string = (char *) ptr;
   printf("%s", string);
   }

static void printInt16(int16_t val)
   {
   #define PRINTINT16_LINE LINETOSTR(__LINE__)
   printf("%d", val);
   }

static void printInt32(int32_t val)
   {
   #define PRINTINT32_LINE LINETOSTR(__LINE__)
   printf("%d", val);
   }

static void printAddress(void* addr)
   {
   #define PRINTADDRESS_LINE LINETOSTR(__LINE__)
   printf("%p", addr);
   }

LinkedListMethod::LinkedListMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("search");
   TR::IlType *pElementType = d->PointerTo((char *)"Element");
   DefineParameter("list", pElementType);
   DefineParameter("key", Int16);
   DefineReturnType(Int32);

   DefineFunction((char *)"printString", 
                  (char *)__FILE__,
                  (char *)PRINTSTRING_LINE,
                  (void *)&printString,
                  NoType,
                  1,
                  Int64);
   DefineFunction((char *)"printInt16", 
                  (char *)__FILE__,
                  (char *)PRINTINT16_LINE,
                  (void *)&printInt16,
                  NoType,
                  1,
                  Int16);
   DefineFunction((char *)"printInt32", 
                  (char *)__FILE__,
                  (char *)PRINTINT32_LINE,
                  (void *)&printInt32,
                  NoType,
                  1,
                  Int32);
   DefineFunction((char *)"printAddress", 
                  (char *)__FILE__,
                  (char *)PRINTADDRESS_LINE,
                  (void *)&printAddress,
                  NoType,
                  1,
                  Address);
   }

bool
LinkedListMethod::buildIL()
   {
   Store("ptr",
      Load("list"));
   Store("val",
      ConstInt32(-1));

   Store("ptrNotNull",
      NotEqualTo(
         Load("ptr"),
         NullAddress()));
   
   IlBuilder *loop=NULL;
   WhileDoLoop("ptrNotNull", &loop);

   // should really be part of WhileDoLoop but we can fake it for now
   IlBuilder *breakBuilder = OrphanBuilder();
   AppendBuilder(breakBuilder);

   IlBuilder *foundBuilder = NULL;
   loop->Call("printString",  1,
   loop->                     ConstInt64((int64_t) "search["));
   loop->Call("printAddress", 1,
   loop->                     Load("ptr"));
   loop->Call("printString",  1,
   loop->                     ConstInt64((int64_t) "] = { k"));
   loop->Call("printInt16",   1,
   loop->                     LoadIndirect("Element", "key",
   loop->                        Load("ptr")));
   loop->Call("printString",  1,
   loop->                     ConstInt64((int64_t) ",v "));
   loop->Call("printInt32",   1,
   loop->                     LoadIndirect("Element", "val",
   loop->                        Load("ptr")));
   loop->Call("printString",  1,
   loop->                     ConstInt64((int64_t) "}\n"));

   loop->IfThen(&foundBuilder,
   loop->   EqualTo(
   loop->      LoadIndirect("Element", "key",
   loop->         Load("ptr")),
   loop->      Load("key")));

   foundBuilder->Store("val",
   foundBuilder->   LoadIndirect("Element", "val",
   foundBuilder->      Load("ptr")));
   foundBuilder->Goto(&breakBuilder);

   loop->Store("ptr",
   loop->   LoadIndirect("Element", "next",
   loop->      Load("ptr")));
   loop->Store("ptrNotNull",
   loop->   NotEqualTo(
   loop->      Load("ptr"),
   loop->      NullAddress()));

   Return(
      Load("val"));

   return true;
   }


class LinkedListTypeDictionary : public TR::TypeDictionary
   {
   public:
   LinkedListTypeDictionary() :
      TR::TypeDictionary()
      {
      TR::IlType *ElementType = DEFINE_STRUCT(Element);
      TR::IlType *pElementType = PointerTo("Element");
      DEFINE_FIELD(Element, next, pElementType);
      DEFINE_FIELD(Element, key, Int16);
      DEFINE_FIELD(Element, val, Int32);
      CLOSE_STRUCT(Element);
      }
   };


void
printList(Element *ptr)
   {
   while (ptr)
      {
      printf("(k %d,v %d)\n", ptr->key, ptr->val);
      ptr = ptr->next;
      }
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
   LinkedListTypeDictionary types;

   printf("Step 3: compile method builder\n");
   LinkedListMethod method(&types);
   uint8_t *entry=0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: allocate and populate list\n");
   Element *cdr = NULL;
   for (int16_t c=0;c < 100;c++)
      {
      Element *car = new Element();
      car->key = c;
      car->val = 4 * c;
      car->next = cdr;
      cdr = car;
      }
   Element *list = cdr;
   printList(list);

   printf("Step 5: invoke compiled code and verify results\n");
   LinkedListFunctionType *search = (LinkedListFunctionType *)entry;
   int32_t val = search(list, 500);
   printf("search(list,500) == %d\n", val);
   if (val != -1)
      printf("FAIL!\n");
   else
      {
      for (int16_t n=0;n < 100;n++)
         {
         val = search(list, n);
         printf("search(list,%2d) = %d\n", n, val);
         if (val != 4 * n)
            {
            printf("FAIL!\n");
            break;
            }
         }
      }

   printf ("Step 6: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
