/******************************************************************************
 Copyright IBM Corp. 2016, 2016
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
#include "StructArray.hpp"


void printStructElems(uint8_t type, int32_t value)
   {
   #define PRINTSTRUCTELEMS_LINE LINETOSTR(__LINE__)
   printf("StructType { type = %#x, value = %d }\n", type, value);
   }

void printElemOffsets(int32_t typeOffset, int32_t valueOffset)
   {
   #define PRINTELEMOFFSETS_LINE LINETOSTR(__LINE__)
   printf("StructType: OffsetOf(type) = %d, OffsetOf(value) = %d\n", typeOffset, valueOffset);
   }


CreateStructArrayMethod::CreateStructArrayMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   StructType = d->LookupStruct("Struct");
   pStructType = d->PointerTo(StructType);

   DefineName("testCreateStructArray");
   DefineParameter("size", Int32);
   DefineReturnType(pStructType);

   DefineFunction("malloc",
                  "",
                  "",
                  (void*)&malloc,
                  Address,
                  1,
                  Int32);
   }

bool
CreateStructArrayMethod::buildIL()
   {
   Store("myArray",
      Call("malloc",1,
         Mul(
            Load("size"),
            ConstInt32(StructType->getSize()))));

   TR::IlBuilder* fillArray = NULL;
   ForLoopUp("i", &fillArray,
      ConstInt32(0),
      Load("size"),
      ConstInt32(1));

   fillArray->Store("element",
   fillArray->   IndexAt(pStructType,
   fillArray->      Load("myArray"),
   fillArray->      Load("i")));

   fillArray->StoreIndirect("Struct", "type",
   fillArray->   Load("element"),
   fillArray->   ConvertTo(Int8,
   fillArray->      Load("i")));

   fillArray->StoreIndirect("Struct", "value",
   fillArray->   Load("element"),
   fillArray->   Load("i"));

   Return(
      Load("myArray"));

   return true;
   }

ReadStructArrayMethod::ReadStructArrayMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   StructType = d->LookupStruct("Struct");
   pStructType = d->PointerTo(StructType);

   DefineName("testReadStructArray");
   DefineParameter("myArray", pStructType);
   DefineParameter("size", Int32);
   DefineReturnType(NoType);

   DefineFunction("printStructElems",
                  __FILE__,
                  PRINTSTRUCTELEMS_LINE,
                  (void *)&printStructElems,
                  NoType,
                  2,
                  Int8,
                  Int32);

   DefineFunction("printElemOffsets",
                  __FILE__,
                  PRINTELEMOFFSETS_LINE,
                  (void *)&printElemOffsets,
                  NoType,
                  2,
                  Int32,
                  Int32);
   }

bool
ReadStructArrayMethod::buildIL()
   {
   Call("printElemOffsets", 2,
      ConstInt32(typeDictionary()->OffsetOf("Struct", "type")),
      ConstInt32(typeDictionary()->OffsetOf("Struct", "value")));

   TR::IlBuilder* readArray = NULL;
   ForLoopUp("i", &readArray,
      ConstInt32(0),
      Load("size"),
      ConstInt32(1));

   readArray->Store("element",
   readArray->   IndexAt(pStructType,
   readArray->      Load("myArray"),
   readArray->      Load("i")));

   readArray->Call("printStructElems", 2,
   readArray->   LoadIndirect("Struct", "type",
   readArray->      Load("element")),
   readArray->   LoadIndirect("Struct", "value",
   readArray->      Load("element")));

   Return();

   return true;
   }


class StructArrayTypeDictionary : public TR::TypeDictionary
   {
   public:
   StructArrayTypeDictionary() :
      TR::TypeDictionary()
      {
      DefineStruct("Struct");
      DefineField("Struct", "type", Int8);
      DefineField("Struct", "value", Int32);
      CloseStruct("Struct");
      }
   };


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
   StructArrayTypeDictionary methodTypes;

   printf("Step 3: compile createMethod builder\n");
   CreateStructArrayMethod createMethod(&methodTypes);
   uint8_t *createEntry;
   int32_t rc = compileMethodBuilder(&createMethod, &createEntry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: compile readMethod builder\n");
   ReadStructArrayMethod readMethod(&methodTypes);
   uint8_t *readEntry;
   rc = compileMethodBuilder(&readMethod, &readEntry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 5: invoke compiled code for createMethod\n");
   auto arraySize = 16;
   CreateStructArrayFunctionType *create = (CreateStructArrayFunctionType *) createEntry;
   Struct* array = create(arraySize);

   printf("Step 6: invoke compiled code for readMethod and verify results\n");
   ReadStructArrayFunctionType *read = (ReadStructArrayFunctionType *) readEntry;
   read(array, arraySize);

   printf ("Step 7: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }

