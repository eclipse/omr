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
#include "ilgen/ThunkBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"

#define OPT_DETAILS "O^O THUNKBUILDER: "

#include "ilgen/IlType.hpp"
#include "ilgen/ThunkBuilder.hpp"
#include "ilgen/ThunkBuilderImpl.hpp"
#include "ilgen/TypeDictionary.hpp"


TR::ThunkBuilderImpl *
OMR::ThunkBuilder::impl()
   {
   return static_cast<TR::ThunkBuilderImpl *>(_impl);
   }


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
   : TR::MethodBuilder(types)
   {
   // NOTE: responsibility for deleting this array rests with the ThunkBuilderImpl object the array will be passed to
   TR::IlTypeImpl **paramImplTypes = new TR::IlTypeImpl*[numCalleeParams];
   for (uint32_t p=0;p < numCalleeParams;p++)
      {
      paramImplTypes[p] = calleeParamTypes[p]->impl();
      }
   _impl = TR::ThunkBuilderImpl::allocate(static_cast<TR::ThunkBuilder *>(this),
                                          types->impl(),
                                          numCalleeParams,
                                          paramImplTypes);

   DefineLine(__FILE__);
   DefineFile(LINETOSTR(__LINE__));
   DefineName(name);
   DefineReturnType(returnType);
   DefineParameter("target", Address); // target
   DefineParameter("args", types->PointerTo(Word));     // array of arguments

   DefineFunction("thunkTarget",
                  __FILE__,
                  LINETOSTR(__LINE__),
                  0, // entry will be a computed value
                  returnType,
                  numCalleeParams,
                  calleeParamTypes);
   }

OMR::ThunkBuilder::~ThunkBuilder()
   {
   if (_impl)
      {
      //delete impl();
      _impl = NULL; // ensure superclasses do not also delete _impl
      }
   }
