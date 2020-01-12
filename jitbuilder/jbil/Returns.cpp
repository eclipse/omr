/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#include <string>

#include "JitBuilder.hpp"

using namespace OMR::JitBuilder;

class Return3 : public FunctionBuilder
   {
public:
   Return3(TypeDictionary * types)
      : FunctionBuilder(types)
      {
      DefineName("Return3");
      DefineLine(LINETOSTR(__LINE__));
      DefineFile(__FILE__);
      DefineParameter("pi8", types->PointerTo(Int8));
      DefineParameter("pi16", types->PointerTo(Int16));
      DefineParameter("pi32", types->PointerTo(Int32));
      DefineParameter("pi64", types->PointerTo(Int64));
      DefineParameter("pf32", types->PointerTo(Float));
      DefineParameter("pf64", types->PointerTo(Double));
      DefineParameter("pa", types->PointerTo(Address));
      DefineReturnType(Address);
      DefineParameter("ptr", types->PointerTo(Int32));
      }

   virtual bool buildIL()
      {
      Builder *b = OrphanBuilder();

      Value *vi8 = b->Mul(b->ConstInt8(2), b->LoadAt(_types->PointerTo(Int8), b->Load("pi8")));
      b->StoreAt(b->Load("pi8"), vi8);
      Value *vi16 = b->Mul(b->ConstInt16(2), b->LoadAt(_types->PointerTo(Int16), b->Load("pi16")));
      b->StoreAt(b->Load("pi16"), vi16);
      Value *vi32 = b->Mul(b->ConstInt32(2), b->LoadAt(_types->PointerTo(Int32), b->Load("pi32")));
      b->StoreAt(b->Load("pi32"), vi32);
      Value *vi64 = b->Mul(b->ConstInt64(2), b->LoadAt(_types->PointerTo(Int64), b->Load("pi64")));
      b->StoreAt(b->Load("pi64"), vi64);
      Value *vf32 = b->Mul(b->ConstFloat(2.0), b->LoadAt(_types->PointerTo(Float), b->Load("pf32")));
      b->StoreAt(b->Load("pf32"), vf32);
      Value *vf64 = b->Mul(b->ConstDouble(2.0), b->LoadAt(_types->PointerTo(Double), b->Load("pf64")));
      b->StoreAt(b->Load("pf64"), vf64);
      Value *zero = b->ConstInt64(0);
      Value *va64 = b->Add(b->LoadAt(_types->PointerTo(Address), b->IndexAt(_types->PointerTo(Address), b->Load("pa"), zero)), b->ConstInt64(8) );
      AppendBuilder(b);
      Return(va64);
      //Return(CoercePointer(_types->PointerTo(Int8), Load("ptr")));
      return true;
      }
   };

typedef void * (Return3Func)(int8_t*, int16_t*, int32_t*, int64_t*, float*, double*, void **);

int
main(int argc, char *argv[])
   {
   bool success = initializeJit();
   if (!success)
      {
      printf("initializeJit failure\n");
      exit(-1);
      }

   TypeDictionary types("Return3Types");
   Return3 method(&types);
   success = method.Construct();
   if (!success)
      {
      printf("construction failure\n");
      exit(-2);
      }


   std::cerr.setf(std::ios_base::skipws);
   std::cerr << "Method to debug:" << std::endl;
   OMR::JitBuilder::TextWriter printer(&method, std::cout, std::string("    "));
   method.setLogger(&printer);
   printer.print();

   int32_t rc=0;
   Return3Func *func = method.DebugEntry<Return3Func>(&rc);
   if (rc != 0)
      {
      printf("Simulation request returned error code %d\n", rc);
      exit(rc);
      }

   int array[10];
   int8_t i8=1; int16_t i16=2; int32_t i32=4; int64_t i64=8; float f32=16.0; double f64=32.0; void *ptr = array; 
   void *rv = func(&i8, &i16, &i32, &i64, &f32, &f64, &ptr);
 
   printf("rv is %p\n", rv);
   printf("i8  is %d\n", i8);
   printf("i16 is %d\n", i16);
   printf("i32 is %d\n", i32);
   printf("i64 is %lld\n", i64);
   printf("f32 is %f\n", f32);
   printf("f64 is %lf\n", f64);
   printf("ptr is %p (array is %p)\n", ptr, array);

   shutdownJit();

   exit(0);
   }
