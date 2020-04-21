/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#include "VMRegister.hpp"
#include "JitBuilder.hpp"

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

   cout << "Step 3: compile vmregister method builder\n";
   VMRegisterMethod method(&types);
   void *entry = 0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 4: invoke compiled vmregister function and print results\n";
   typedef int32_t (VMRegisterMethodFunction)(int8_t **values, int32_t count);
   VMRegisterMethodFunction *vmregister = (VMRegisterMethodFunction *) entry;

   int8_t values[] = {7,2,9,5,3,1,6};
   int8_t *vals = values;
   int32_t retVal = vmregister(&vals, 7);
   cout << "vmregister(values) returned " << retVal << "\n";

   cout << "Step 5: compile vmregisterInStruct method builder\n";
   VMRegisterInStructMethod method2(&types);
   entry = 0;
   rc = compileMethodBuilder(&method2, &entry);
   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 6: invoke compiled vmregisterInStruct function and print results\n";
   typedef int32_t (VMRegisterInStructMethodFunction)(VMRegisterStruct *param);
   VMRegisterInStructMethodFunction *vmregisterInStruct = (VMRegisterInStructMethodFunction *) entry;

   VMRegisterStruct param;
   param.count = 7;
   param.values = values;
   retVal = vmregisterInStruct(&param);
   cout << "vmregisterInStruct(values) returned " << retVal << "\n";

   cout << "Step 7: shutdown JIT\n";
   shutdownJit();
   }



VMRegisterMethod::VMRegisterMethod(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d, (OMR::JitBuilder::VirtualMachineState *) NULL)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("vmregister");
   DefineParameter("valuesPtr", d->PointerTo(d->PointerTo(Int8)));
   DefineParameter("count", Int32);
   DefineReturnType(Int32);
   }

bool
VMRegisterMethod::buildIL()
   {
   OMR::JitBuilder::TypeDictionary *dict = typeDictionary();
   OMR::JitBuilder::IlType *pElementType = dict->PointerTo(Int8);
   OMR::JitBuilder::IlType *ppElementType = dict->PointerTo(pElementType);
   OMR::JitBuilder::VirtualMachineRegister *vmreg = new OMR::JitBuilder::VirtualMachineRegister(this, "MYBYTES", ppElementType, sizeof(int8_t), Load("valuesPtr"));

   Store("result", ConstInt32(0));

   OMR::JitBuilder::IlBuilder *loop = NULL;
   ForLoopUp("i", &loop,
           ConstInt32(0),
           Load("count"),
           ConstInt32(1));

   loop->Store("valAddress",
               vmreg->Load(loop));

   loop->Store("val",
   loop->      LoadAt(pElementType,
   loop->             Load("valAddress")));

   loop->Store("result",
   loop->      Add(
   loop->          Load("result"),
   loop->          ConvertTo(Int32,
   loop->                    Load("val"))));
   vmreg->Adjust(loop, 1);

   Return(Load("result"));

   return true;
   }

VMRegisterInStructMethod::VMRegisterInStructMethod(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d, (OMR::JitBuilder::VirtualMachineState *) NULL)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   d->DefineStruct("VMRegisterStruct");
   d->DefineField("VMRegisterStruct", "values", d->PointerTo(Int8), offsetof(VMRegisterStruct, values));
   d->DefineField("VMRegisterStruct", "count", Int32, offsetof(VMRegisterStruct, count));
   d->CloseStruct("VMRegisterStruct");

   DefineName("vmregisterInStruct");
   DefineParameter("param", d->PointerTo("VMRegisterStruct"));
   DefineReturnType(Int32);
   }

bool
VMRegisterInStructMethod::buildIL()
   {
   OMR::JitBuilder::TypeDictionary *dict = typeDictionary();
   OMR::JitBuilder::IlType *pElementType = dict->PointerTo(Int8);
   OMR::JitBuilder::IlType *ppElementType = dict->PointerTo(pElementType);
   OMR::JitBuilder::VirtualMachineRegisterInStruct *vmreg = new OMR::JitBuilder::VirtualMachineRegisterInStruct(this, "VMRegisterStruct", "param", "values", "VALUES");

   Store("count",
      LoadIndirect("VMRegisterStruct", "count",
                  Load("param")));

   Store("result", ConstInt32(0));

   OMR::JitBuilder::IlBuilder *loop = NULL;
   ForLoopUp("i", &loop,
           ConstInt32(0),
           Load("count"),
           ConstInt32(1));

   loop->Store("valAddress",
               vmreg->Load(loop));

   loop->Store("val",
   loop->      LoadAt(pElementType,
   loop->             Load("valAddress")));

   loop->Store("result",
   loop->      Add(
   loop->          Load("result"),
   loop->          ConvertTo(Int32,
   loop->                    Load("val"))));
   vmreg->Adjust(loop, 1);

   Return(Load("result"));

   return true;
   }
