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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "env/TRMemory.hpp"   // include first to get correct TR_ALLOC definition
#include "ilgen/VirtualMachineInterpreterStack.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "compile/Compilation.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"

OMR::VirtualMachineInterpreterStack::VirtualMachineInterpreterStack(TR::MethodBuilder *mb, TR::VirtualMachineRegister *stackTopRegister, TR::IlType *elementType, bool growsUp, bool preAdjust)
   : TR::VirtualMachineStack(),
   _mb(mb),
   _stackTopRegister(stackTopRegister),
   _elementType(elementType),
   _stackBaseName(NULL),
   _growsUp(growsUp),
   _preAdjust(preAdjust)
   {
   init();
   }

void
OMR::VirtualMachineInterpreterStack::UpdateStack(TR::IlBuilder *b, TR::IlValue *stack)
   {
   TR::IlValue *initialBase = b->Load(_stackBaseName);
   initialBase = b->ConvertTo(_mb->typeDictionary()->toIlType<int64_t>(), initialBase);
   TR::IlValue *currentBase = _stackTopRegister->Load(b);
   currentBase = b->ConvertTo(_mb->typeDictionary()->toIlType<int64_t>(), currentBase);
   TR::IlValue *delta = b->Sub(currentBase, initialBase);

   b->Store(_stackBaseName, stack);
   TR::IlValue *newStackTop = b->Add(stack, delta);
   _stackTopRegister->Store(b, newStackTop);
   }

TR::VirtualMachineState *
OMR::VirtualMachineInterpreterStack::MakeCopy()
   {
   return static_cast<TR::VirtualMachineState *>(this);
   } 

void
OMR::VirtualMachineInterpreterStack::Push(TR::IlBuilder *builder, TR::IlValue *value)
   {
   TR::IlValue *stackAddress = _stackTopRegister->Load(builder);
   if (_growsUp)
      _stackTopRegister->Adjust(builder, 1);
   else
      _stackTopRegister->Adjust(builder, -1);

   if (_preAdjust)
      stackAddress = _stackTopRegister->Load(builder);

   builder->StoreAt(
               stackAddress,
   builder->   ConvertTo(_elementType, value));
   }

TR::IlValue *
OMR::VirtualMachineInterpreterStack::Top(TR::IlBuilder *builder)
   {
   TR::IlValue *stackAddress = _stackTopRegister->Load(builder);
   TR::IlType *pElementType = _mb->typeDictionary()->PointerTo(_elementType);

   int32_t offset = 0;
   if (!_preAdjust)
      {
      if (_growsUp)
         {
         offset = -1;
         }
      else
         {
         offset = 1;
         }
      }

   TR::IlValue *value =
   builder->LoadAt(pElementType,
   builder->   IndexAt(pElementType,
                  stackAddress,
   builder->      ConstInt32(offset)));
   return value;
   }

TR::IlValue *
OMR::VirtualMachineInterpreterStack::Pop(TR::IlBuilder *builder)
   {
   TR::IlValue *stackAddress = _stackTopRegister->Load(builder);
   if (_growsUp)
      _stackTopRegister->Adjust(builder, -1);
   else
      _stackTopRegister->Adjust(builder, 1);

   if (!_preAdjust)
      stackAddress = _stackTopRegister->Load(builder);

   TR::IlType *pElementType = _mb->typeDictionary()->PointerTo(_elementType);
   return builder->LoadAt(pElementType, stackAddress);
   }

TR::IlValue *
OMR::VirtualMachineInterpreterStack::Pick(TR::IlBuilder *builder, int32_t depth)
   {
   return Pick(builder, builder->ConstInt32(depth));
   }

TR::IlValue *
OMR::VirtualMachineInterpreterStack::Pick(TR::IlBuilder *builder, TR::IlValue *depth)
   {
   if (!_preAdjust)
      {
      TR::IlValue *one = builder->ConstInt32(1);
      depth = builder->Add(depth, one);
      }
   TR::IlValue *negatedDepth = builder->Negate(depth);
   if (_growsUp)
      _stackTopRegister->Adjust(builder, negatedDepth);
   else
      _stackTopRegister->Adjust(builder, depth);

   TR::IlValue *stackAddress = _stackTopRegister->Load(builder);
   TR::IlType *pElementType = _mb->typeDictionary()->PointerTo(_elementType);
   TR::IlValue *result = builder->LoadAt(pElementType, stackAddress);

   if (_growsUp)
      _stackTopRegister->Adjust(builder, depth);
   else
      _stackTopRegister->Adjust(builder, negatedDepth);

   return result;
   }

void
OMR::VirtualMachineInterpreterStack::Drop(TR::IlBuilder *builder, int32_t depth)
   {
   Drop(builder, builder->ConstInt32(depth));
   }

void
OMR::VirtualMachineInterpreterStack::Drop(TR::IlBuilder *builder, TR::IlValue *depth)
   {
   TR::IlValue *negatedDepth = builder->Negate(depth);
   _stackTopRegister->Adjust(builder, negatedDepth);
   }

void
OMR::VirtualMachineInterpreterStack::Dup(TR::IlBuilder *builder)
   {
   Push(builder, Top(builder));
   }

void
OMR::VirtualMachineInterpreterStack::init()
   {
   TR::Compilation *comp = TR::comp();
   // Create a temp for the OperandStack base
   TR::SymbolReference *symRef = comp->getSymRefTab()->createTemporary(_mb->methodSymbol(), _mb->typeDictionary()->PointerTo(_elementType)->getPrimitiveType());
   symRef->getSymbol()->setNotCollected();
   char *name = (char *) comp->trMemory()->allocateHeapMemory((11+10+1) * sizeof(char)); // 11 ("_StackBase_") + max 10 digits + trailing zero
   sprintf(name, "_StackBase_%u", symRef->getCPIndex());
   symRef->getSymbol()->getAutoSymbol()->setName(name);
   _mb->defineSymbol(name, symRef);
   _stackBaseName = symRef->getSymbol()->getAutoSymbol()->getName();
   // store current stack value so that we can use this when MoveStack is called
   _mb->Store(_stackBaseName, _stackTopRegister->Load(_mb));
   }
