/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
#include "ilgen/VirtualMachineOperandStack.hpp"

#include "ilgen/VirtualMachineRegister.hpp"
#include "compile/Compilation.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"

OMR::VirtualMachineOperandStack::VirtualMachineOperandStack(TR::MethodBuilder *mb, int32_t sizeHint, TR::IlType *elementType,
   TR::VirtualMachineRegister *stackTopRegister, bool growsUp, int32_t stackInitialOffset)
   : TR::VirtualMachineState(),
   _mb(mb),
   _stackTopRegister(stackTopRegister),
   _stackMax(sizeHint),
   _stackTop(-1),
   _elementType(elementType),
   _pushAmount(growsUp ? +1 : -1),
   _stackOffset(stackInitialOffset)
   {
   init();
   }

OMR::VirtualMachineOperandStack::VirtualMachineOperandStack(TR::VirtualMachineOperandStack *other)
   : TR::VirtualMachineState(),
   _mb(other->_mb),
   _stackTopRegister(other->_stackTopRegister),
   _stackMax(other->_stackMax),
   _stackTop(other->_stackTop),
   _elementType(other->_elementType),
   _pushAmount(other->_pushAmount),
   _stackOffset(other->_stackOffset),
   _stackBaseName(other->_stackBaseName)
   {
   int32_t numBytes = _stackMax * sizeof(TR::IlValue *);
   _stack = (TR::IlValue **) TR::comp()->trMemory()->allocateHeapMemory(numBytes);
   memcpy(_stack, other->_stack, numBytes);
   }


// commits the simulated operand stack of values to the virtual machine state
// the given builder object is where the operations to commit the state will be inserted
// the top of the stack is assumed to be managed independently, most likely
//    as a VirtualMachineRegister or a VirtualMachineRegisterInStruct
void
OMR::VirtualMachineOperandStack::Commit(TR::IlBuilder *b)
   {
   TR::IlType *Element = _elementType;
   TR::IlType *pElement = _mb->typeDictionary()->PointerTo(Element);

   TR::IlValue *stack = b->Load(_stackBaseName);

   // Adjust the vm _stackTopRegister by number of elements that have been pushed onto the stack.
   // _stackTop is -1 at 0 pushes, 0 for 1 push, so # of elements to adjust by is _stackTop+1
   _stackTopRegister->Store(b, stack);
   _stackTopRegister->Adjust(b, (_stackTop+1)*_pushAmount);

   for (int32_t i = _stackTop;i >= 0;i--)
      {
      // TBD: how to handle hitting the end of stack?

      b->StoreAt(
      b->   IndexAt(pElement,
               stack,
      b->      ConstInt32(i - _stackOffset)),
            Pick(_stackTop-i)); // should generalize, maybe delegate element storage ?
      }
   }

void
OMR::VirtualMachineOperandStack::Reload(TR::IlBuilder* b)
   {
   TR::IlType* Element = _elementType;
   TR::IlType* pElement = _mb->typeDictionary()->PointerTo(Element);
   // reload the elements back into the simulated operand stack
   // If the # of stack element has changed, the user should adjust the # of elements
   // using Drop beforehand to add/delete stack elements.
   TR::IlValue* stack = b->Load(_stackBaseName);
   for (int32_t i = _stackTop; i >= 0; i--)
      {
      _stack[i] = b->LoadAt(pElement,
                  b->   IndexAt(pElement,
                           stack,
                  b->      ConstInt32(i - _stackOffset)));
       }
   }

void
OMR::VirtualMachineOperandStack::MergeInto(TR::VirtualMachineState* o, TR::IlBuilder* b)
   {
   TR::VirtualMachineOperandStack *other = static_cast<TR::VirtualMachineOperandStack *>(o);
   TR_ASSERT_FATAL(_stackTop == other->_stackTop, "stacks are not same size");
   for (int32_t i=_stackTop;i >= 0;i--)
      {
      // only need to do something if the two entries aren't already the same
      if (other->_stack[i]->getID() != _stack[i]->getID())
         {
         // what if types don't match? could use ConvertTo, but seems...arbitrary
         // nobody *should* design bytecode set where corresponding elements of stacks from
         // two incoming control flow edges can have different primitive types. objects, sure
         // but not primitive types (even different types of objects should have same primitive
         // type: Address. Expecting to be disappointed here some day...
         TR_ASSERT_FATAL(_stack[i]->getDataType() == other->_stack[i]->getDataType(), "invalid stack merge: primitive type mismatch at same depth stack elements");
         b->StoreOver(other->_stack[i], _stack[i]);
         }
      }
   }

// Update the OperandStack_base and _stackTopRegister after the Virtual Machine moves the stack.
// This call will normally be followed by a call to Reload if any of the stack values changed in the move
void
OMR::VirtualMachineOperandStack::UpdateStack(TR::IlBuilder *b, TR::IlValue *stack)
   {
   b->Store(_stackBaseName, stack);
   }

// Allocate a new operand stack and copy everything in this state
// If VirtualMachineOperandStack is subclassed, this function *must* also be implemented in the subclass!
TR::VirtualMachineState *
OMR::VirtualMachineOperandStack::MakeCopy()
   {
   TR::VirtualMachineOperandStack *copy = (TR::VirtualMachineOperandStack *) TR::comp()->trMemory()->allocateHeapMemory(sizeof(TR::VirtualMachineOperandStack));
   new (copy) TR::VirtualMachineOperandStack(static_cast<TR::VirtualMachineOperandStack *>(this));

   return copy;
   }

void
OMR::VirtualMachineOperandStack::Push(TR::IlBuilder *b, TR::IlValue *value)
   {
   checkSize();
   _stack[++_stackTop] = value;
   }

TR::IlValue *
OMR::VirtualMachineOperandStack::Top()
   {
   TR_ASSERT_FATAL(_stackTop >= 0, "no top: stack empty");
   return _stack[_stackTop];
   }

TR::IlValue *
OMR::VirtualMachineOperandStack::Pop(TR::IlBuilder *b)
   {
   TR_ASSERT_FATAL(_stackTop >= 0, "stack underflow");
   return _stack[_stackTop--];
   }

TR::IlValue *
OMR::VirtualMachineOperandStack::Pick(int32_t depth)
   {
   TR_ASSERT_FATAL(_stackTop >= depth, "pick request exceeds stack depth");
   return _stack[_stackTop - depth];
   }

void
OMR::VirtualMachineOperandStack::Drop(TR::IlBuilder *b, int32_t depth)
   {
   TR_ASSERT_FATAL(_stackTop >= depth-1, "stack underflow");
   _stackTop-=depth;
   }

void
OMR::VirtualMachineOperandStack::Dup(TR::IlBuilder *b)
   {
   TR_ASSERT_FATAL(_stackTop >= 0, "cannot dup: stack empty");
   TR::IlValue *top = _stack[_stackTop];
   Push(b, top);
   }

void
OMR::VirtualMachineOperandStack::copyTo(TR::VirtualMachineOperandStack *copy)
   {
   }

void
OMR::VirtualMachineOperandStack::checkSize()
   {
   if (_stackTop == _stackMax - 1)
      grow();
   }

void
OMR::VirtualMachineOperandStack::grow(int32_t growAmount)
   {
   if (growAmount == 0)
      growAmount = (_stackMax >> 1);

   // if _stackMax == 1, growAmount would still be zero, so bump to 1
   if (growAmount == 0)
      growAmount = 1;

   int32_t newMax = _stackMax + growAmount;
   int32_t newBytes = newMax * sizeof(TR::IlValue *);
   TR::IlValue ** newStack = (TR::IlValue **) TR::comp()->trMemory()->allocateHeapMemory(newBytes);

   memset(newStack, 0, newBytes);

   int32_t numBytes = _stackMax * sizeof(TR::IlValue *);
   memcpy(newStack, _stack, numBytes);

   _stack = newStack;
   _stackMax = newMax;
   }

void *
OMR::VirtualMachineOperandStack::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::VirtualMachineOperandStack *>(this));
   return _client;
   }

ClientAllocator OMR::VirtualMachineOperandStack::_clientAllocator = NULL;
ClientAllocator OMR::VirtualMachineOperandStack::_getImpl = NULL;

void
OMR::VirtualMachineOperandStack::init()
   {
   int32_t numBytes = _stackMax * sizeof(TR::IlValue *);
   _stack = (TR::IlValue **) TR::comp()->trMemory()->allocateHeapMemory(numBytes);
   memset(_stack, 0, numBytes);

   TR::Compilation *comp = TR::comp();
   // Create a temp for the OperandStack base
   TR::SymbolReference *symRef = comp->getSymRefTab()->createTemporary(_mb->methodSymbol(), _mb->typeDictionary()->PointerTo(_elementType)->getPrimitiveType());
   symRef->getSymbol()->setNotCollected();
   char *name = (char *) comp->trMemory()->allocateHeapMemory((11+10+1) * sizeof(char)); // 11 ("_StackBase_") + max 10 digits + trailing zero
   sprintf(name, "_StackBase_%u", symRef->getCPIndex());
   symRef->getSymbol()->getAutoSymbol()->setName(name);
   _mb->defineSymbol(name, symRef);

   _stackBaseName = symRef->getSymbol()->getAutoSymbol()->getName();

   // store current operand stack pointer base address so we can use it whenever we need
   // to recreate the stack as the interpreter would have
   _mb->Store(_stackBaseName, _stackTopRegister->Load(_mb));
   }
