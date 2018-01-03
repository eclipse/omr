/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "ilgen/IlBuilderImpl.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlTypeImpl.hpp"
#include "ilgen/IlValueImpl.hpp"
#include "ilgen/ThunkBuilder.hpp"
#include "ilgen/ThunkBuilderImpl.hpp"
#include "ilgen/TypeDictionaryImpl.hpp"

#define OPT_DETAILS "O^O THUNKBUILDER: "

/**
 * ThunkBuilderImpl is a MethodBuilder object representing a thunk for
 * calling native functions with a particular signature (kind of like
 * libffi). It is designed to take a target address and an array of
 * Word-sized arguments and to call that target address with the
 * provided arguments (after converting to the appropriate parameter
 * types). The types of the parameters and return type are provided at
 * construction time so that different ThunkBuilder instances can use
 * different method signatures.
 */

OMR::ThunkBuilderImpl::ThunkBuilderImpl(TR::ThunkBuilder *client,
                                        TR::TypeDictionaryImpl *types,
                                        uint32_t numCalleeParams,
                                        TR::IlTypeImpl **calleeParamTypes)
   : TR::MethodBuilderImpl(client, types),
   _numCalleeParams(numCalleeParams),
   _calleeParamTypes(calleeParamTypes)
   {
   // NOTE: this object now has responsibility to delete[] _calleeParamTypes
   }

OMR::ThunkBuilderImpl::~ThunkBuilderImpl()
   {
   delete[] _calleeParamTypes;
   _calleeParamTypes = NULL;
   }

bool
OMR::ThunkBuilderImpl::buildIL()
   {
   TR::IlTypeImpl *pWord = typeDictionary()->PointerTo(Word);

   uint32_t numArgs = _numCalleeParams+1;
   TR::IlValueImpl **args = (TR::IlValueImpl **) comp()->trMemory()->allocateHeapMemory(numArgs * sizeof(TR::IlValueImpl *));

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

   TR::IlValueImpl *retValue = ComputedCall("thunkTarget", numArgs, args);

   if (getReturnType() != NoType)
      Return(retValue);
   else
      Return();

   return true;
   }

TR::ThunkBuilderImpl *
OMR::ThunkBuilderImpl::allocate(TR::ThunkBuilder *client,
                                TR::TypeDictionaryImpl *types,
                                uint32_t numCalleeParams,
                                TR::IlTypeImpl **calleeParamTypes)
   {
   return new (PERSISTENT_NEW) TR::ThunkBuilderImpl(client, types, numCalleeParams, calleeParamTypes);
   }
