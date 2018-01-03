/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/BytecodeBuilderImpl.hpp"
#include "ilgen/IlType.hpp"
#include "ilgen/IlBuilderImpl.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/MethodBuilderImpl.hpp"
#include "ilgen/TypeDictionary.hpp"


TR::MethodBuilderImpl *
OMR::MethodBuilder::impl()
   {
   // unfortunately, cannot use a static_cast here without including MethodBuilderImpl.hpp
   // which pulls in more compiler headers we don't want
   return (TR::MethodBuilderImpl *)_impl;
   }

OMR::MethodBuilder::MethodBuilder(TR::IlBuilderImpl *impl, TR::TypeDictionary *types, OMR::VirtualMachineState *vmState)
   : TR::IlBuilder(impl, static_cast<TR::MethodBuilder *>(this), types)
   { }

OMR::MethodBuilder::MethodBuilder(TR::TypeDictionary *types, OMR::VirtualMachineState *vmState)
   : TR::IlBuilder(TR::MethodBuilderImpl::allocate(static_cast<TR::MethodBuilder *>(this), types->impl(), vmState),
                   static_cast<TR::MethodBuilder *>(this),
                   types),
   _types(types)
   { }

OMR::MethodBuilder::~MethodBuilder()
   {
   for(std::vector<TR::IlBuilder *>::iterator it = _ownedBuilderObjects.begin(); it != _ownedBuilderObjects.end(); ++it)
      {
      //delete *it;
      }

   if (_impl)
      {
      //delete impl();
      _impl = NULL; // ensure superclasses do not also delete _impl
      }
   }

void
OMR::MethodBuilder::AllLocalsHaveBeenDefined()
   {
   impl()->AllLocalsHaveBeenDefined();
   }

void
OMR::MethodBuilder::AppendBuilder(TR::BytecodeBuilder *bb)
   {
   impl()->AppendBuilder(bb->impl());
   }

void
OMR::MethodBuilder::AppendBuilder(TR::IlBuilder *b)
   {
   impl()->AppendBuilder(b->impl());
   }

void
OMR::MethodBuilder::AppendBytecodeBuilder(TR::BytecodeBuilder *bb)
   {
   impl()->AppendBytecodeBuilder(bb->impl());
   }

void
OMR::MethodBuilder::DefineArrayParameter(const char *name, TR::IlType *dt)
   {
   impl()->DefineArrayParameter(name, dt->impl());
   }

void
OMR::MethodBuilder::DefineFile(const char *fileName)
   {
   impl()->DefineFile(fileName);
   }

void
OMR::MethodBuilder::DefineFunction(const char* const name,
                                   const char* const fileName,
                                   const char* const lineNumber,
                                   void            * entryPoint,
                                   TR::IlType      * returnType,
                                   int32_t           numParms,
                                   ...)
   {
   TR::IlTypeImpl **parmTypeImpls = new TR::IlTypeImpl*[numParms];

   va_list parms;
   va_start(parms, numParms);
   for (int32_t p=0;p < numParms;p++)
      {
      parmTypeImpls[p] = va_arg(parms, TR::IlType *)->impl();
      }
   va_end(parms);

   impl()->DefineFunction(name, fileName, lineNumber, entryPoint, returnType->impl(), numParms, parmTypeImpls);
   // parmTypeImpls array now owned by impl() object
   }

void
OMR::MethodBuilder::DefineFunction(const char* const name,
                                   const char* const fileName,
                                   const char* const lineNumber,
                                   void            * entryPoint,
                                   TR::IlType      * returnType,
                                   int32_t           numParms,
                                   TR::IlType     ** parmTypes)
   {
   TR::IlTypeImpl **parmTypeImpls = new TR::IlTypeImpl*[numParms];
   for (int32_t p=0;p < numParms;p++)
      {
      parmTypeImpls[p] = parmTypes[p]->impl();
      }

   impl()->DefineFunction(name, fileName, lineNumber, entryPoint, returnType->impl(), numParms, parmTypeImpls);
   // parmTypeImpls array now owned by impl() object
   }

void
OMR::MethodBuilder::DefineLine(const char *line)
   {
   impl()->DefineLine(line);
   }

void
OMR::MethodBuilder::DefineLine(int line)
   {
   impl()->DefineLine(line);
   }

void
OMR::MethodBuilder::DefineLocal(const char *name, TR::IlType *dt)
   {
   impl()->DefineLocal(name, dt->impl());
   }

void
OMR::MethodBuilder::DefineMemory(const char *name, TR::IlType *dt, void *location)
   {
   impl()->DefineMemory(name, dt->impl(), location);
   }

void
OMR::MethodBuilder::DefineName(const char *name)
   {
   impl()->DefineName(name);
   }

void
OMR::MethodBuilder::DefineParameter(const char *name, TR::IlType *type)
   {
   impl()->DefineParameter(name, type->impl());
   }

void
OMR::MethodBuilder::DefineReturnType(TR::IlType *dt)
   {
   impl()->DefineReturnType(dt->impl());
   }

int32_t
OMR::MethodBuilder::GetNextBytecodeFromWorklist()
   {
   return impl()->GetNextBytecodeFromWorklist();
   }

TR::BytecodeBuilder *
OMR::MethodBuilder::OrphanBytecodeBuilder(int32_t bcIndex, char *name)
   {
   TR::BytecodeBuilderImpl *bbi = impl()->OrphanBytecodeBuilderImpl(bcIndex, name);
   TR::BytecodeBuilder *bcb = new TR::BytecodeBuilder(bbi, static_cast<TR::MethodBuilder *>(this), _types);
   bbi->setClient(bcb);
   newBuilder(bcb);
   return bcb;
   }

void
OMR::MethodBuilder::setVMState(OMR::VirtualMachineState *vmState)
   {
   impl()->SetVMState(vmState);
   }

void
OMR::MethodBuilder::newBuilder(TR::IlBuilder *b)
   {
   _ownedBuilderObjects.push_back(b);
   }
