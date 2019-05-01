/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/ThunkBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"

#define OPT_DETAILS "O^O THUNKBUILDER: "

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

/**
 * ThunkBuilder is a MethodBuilder object representing a thunk for
 * calling native functions with a particular signature (kind of like
 * libffi). It is designed to take a target address and an array of
 * Word-sized arguments and to call that target address with the
 * provided arguments (after converting to the appropriate parameter
 * types). The types of the parameters and return type are provided at
 * construction time so that different ThunkBuilder instances can use
 * different method signatures.
 */

OMR::ThunkBuilder::ThunkBuilder(TR::TypeDictionary *types, const char *name, TR::IlType *returnType,
                                uint32_t numCalleeParams, TR::IlType **calleeParamTypes)
   : TR::MethodBuilder(types),
   _numCalleeParams(numCalleeParams),
   _calleeParamTypes(calleeParamTypes)
   {
   DefineLine(__FILE__);
   DefineFile(LINETOSTR(__LINE__));
   DefineName(name);
   DefineReturnType(returnType);
   DefineParameter("target", Address); // target
   DefineParameter("args", types->PointerTo(Word));     // array of arguments

   DefineFunction("thunkCallee",
                  __FILE__,
                  LINETOSTR(__LINE__),
                  0, // entry will be a computed value
                  returnType,
                  numCalleeParams,
                  calleeParamTypes);
   }

bool
OMR::ThunkBuilder::buildIL()
   {
   TR::IlType *pWord = typeDictionary()->PointerTo(Word);

   uint32_t numArgs = _numCalleeParams+1;
   TR::IlValue **args = (TR::IlValue **) comp()->trMemory()->allocateHeapMemory(numArgs * sizeof(TR::IlValue *));

   // first argument is the target address
   args[0] = Load("target");

   // followed by the actual arguments
   for (uint32_t p=0; p < _numCalleeParams; p++)
      {
      uint32_t a=p+1;
      args[a] = ConvertTo(_calleeParamTypes[p],
                   LoadAt(pWord,
                      IndexAt(pWord,
                         Load("args"),
                         ConstInt32(p))));
      }

   TR::IlValue *retValue = ComputedCall("thunkCallee", numArgs, args);

   if (getReturnType() != NoType)
      Return(retValue);

   Return();

   return true;
   }

void *
OMR::ThunkBuilder::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::ThunkBuilder *>(this));
   return _client;
   }

ClientAllocator OMR::ThunkBuilder::_clientAllocator = NULL;
ClientAllocator OMR::ThunkBuilder::_getImpl = NULL;
