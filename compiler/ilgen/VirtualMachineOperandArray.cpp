/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "ilgen/VirtualMachineOperandArray.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/IlType.hpp"
#include "ilgen/IlValue.hpp"
#include "ilgen/TypeDictionary.hpp"

// TraceIL historically injected some useful comments into the compile log; need to decide a mechanism
// for how this works independently of a compilation. For now, just #define TraceIL to nothing so the
// messages can still live in the code.

#if 0
#define TraceEnabled    (TR::comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(TR::comp(), m, ##__VA_ARGS__);}}
#else
#define TraceIL(m, ...)
#endif

// Also historically worked very well during a compilation, but need a long-term solution here when
// JitBuilder is used outside of a compilation. For now, just use assert to check the same condition
// and rely on consumer to be able to look up file and line number to see helpful message
#ifndef TR_ASSERT
#include <assert.h>
#define TR_ASSERT(c, m) assert(c)
#endif


OMR::VirtualMachineOperandArray::VirtualMachineOperandArray(TR::MethodBuilder *mb, int32_t numOfElements, TR::IlType *elementType, VirtualMachineRegister *arrayBaseRegister)
   : VirtualMachineState(),
   _mb(mb),
   _numberOfElements(numOfElements),
   _elementType(elementType),
   _arrayBaseRegister(arrayBaseRegister)
   {
   _values = new TR::IlValue * [_numberOfElements];
   memset(_values, 0, _numberOfElements * sizeof(TR::IlValue *));

   // store current operand stack pointer base address so we can use it whenever we need
   // to recreate the stack as the interpreter would have
   mb->Store("OperandArray_base", arrayBaseRegister->Load(mb));
   }

OMR::VirtualMachineOperandArray::VirtualMachineOperandArray(OMR::VirtualMachineOperandArray *other)
   : VirtualMachineState(),
   _mb(other->_mb),
   _numberOfElements(other->_numberOfElements),
   _elementType(other->_elementType),
   _arrayBaseRegister(other->_arrayBaseRegister)
   {
   _values = new TR::IlValue * [_numberOfElements];
   memcpy(_values, other->_values, _numberOfElements*sizeof(TR::IlValue *));
   }

OMR::VirtualMachineOperandArray::~VirtualMachineOperandArray()
   {
   delete[] _values;
   }

// commits the simulated operand array of values to the virtual machine state
// the given builder object is where the operations to commit the state will be inserted
// into the array which is assumed to be managed independently, most likely
void
OMR::VirtualMachineOperandArray::Commit(TR::IlBuilder *b)
   {
   TR::IlType *Element = _elementType;
   TR::IlType *pElement = _mb->typeDictionary()->PointerTo(Element);

   TR::IlValue *arrayBase = b->Load("OperandArray_base");

   for (int32_t i = 0;i < _numberOfElements;i++)
      {
      TR::IlValue *element = _values[i];
      if (element != NULL)
         {
         b->StoreAt(
         b->   IndexAt(pElement,
                  arrayBase,
         b->      ConstInt32(i)),
               element);
         }
      }
   }

void
OMR::VirtualMachineOperandArray::Reload(TR::IlBuilder* b)
   {
   TR::IlType* Element = _elementType;
   TR::IlType* pElement = _mb->typeDictionary()->PointerTo(Element);
   // reload the elements back into the simulated operand array
   TR::IlValue* array = b->Load("OperandArray_base");
   for (int32_t i = 0; i < _numberOfElements; i++)
      {
      _values[i] = b->LoadAt(pElement,
                   b->   IndexAt(pElement,
                            array,
                   b->      ConstInt32(i)));
       }
   }

void
OMR::VirtualMachineOperandArray::MergeInto(OMR::VirtualMachineState *o, TR::IlBuilder *b)
   {
   VirtualMachineOperandArray *other = (VirtualMachineOperandArray *)o;
   TR_ASSERT(_numberOfElements == other->_numberOfElements, "array are not same size");
   for (int32_t i=0;i < _numberOfElements;i++)
      {
      if (NULL == other->_values[i])
         {
         TR_ASSERT(_values[i] == NULL, "if an element in the dest array at index is NULL it has to be NULL in the src array");
         }
      else if (other->_values[i]->getID() != _values[i]->getID())
         {
         TraceIL("VirtualMachineOperandArray[ %p ]::MergeInto builder %p index %d storeOver %p(%d) with %p(%d)\n", this, b, i, other->_values[i], other->_values[i]->getID(), _values[i], _values[i]->getID());
         b->StoreOver(other->_values[i], _values[i]);
         }
      }
   }

// Update the OperandArray_base after the Virtual Machine moves the array.
// This call will normally be followed by a call to Reload if any of the array values changed in the move
void
OMR::VirtualMachineOperandArray::UpdateArray(TR::IlBuilder *b, TR::IlValue *array)
   {
   b->Store("OperandArray_base", array);
   }

// Allocate a new operand array and copy everything in this state
// If VirtualMachineOperandArray is subclassed, this function *must* also be implemented in the subclass!
OMR::VirtualMachineState *
OMR::VirtualMachineOperandArray::MakeCopy()
   {
   VirtualMachineOperandArray *copy = new VirtualMachineOperandArray(this);

   return copy;
   }

TR::IlValue *
OMR::VirtualMachineOperandArray::Get(int32_t index)
   {
   TR_ASSERT(index < _numberOfElements, "index has to be less than the number of elements");
   TR_ASSERT(index >= 0, "index can not be negative");
   return _values[index];
   }

void
OMR::VirtualMachineOperandArray::Set(int32_t index, TR::IlValue *value)
   {
   TR_ASSERT(index < _numberOfElements, "index has to be less than the number of elements");
   TR_ASSERT(index >= 0, "index can not be negative");

   if (NULL != _values[index])
      {
      TraceIL("VirtualMachineOperandArray[ %p ]::Set index %d to %p(%d) old value was %p(%d)\n", this, index, value, value->getID(), _values[index], _values[index]->getID());
      }
   else
      {
      TraceIL("VirtualMachineOperandArray[ %p ]::Set index %d to %p(%d)\n", this, index, value, value->getID());
      }

   _values[index] = value;
   }


void
OMR::VirtualMachineOperandArray::Move(TR::IlBuilder *b, int32_t dstIndex, int32_t srcIndex)
   {
   TR_ASSERT(dstIndex < _numberOfElements, "dstIndex has to be less than the number of elements");
   TR_ASSERT(dstIndex >= 0, "dstIndex can not be negative");
   TR_ASSERT(srcIndex < _numberOfElements, "srcIndex has to be less than the number of elements");
   TR_ASSERT(srcIndex >= 0, "srcIndex can not be negative");

   _values[dstIndex] = b->Copy(_values[srcIndex]);
   TraceIL("VirtualMachineOperandArray[ %p ]::Move builder %p move srcIndex %d %p(%d) to dstIndex %d %p(%d)\n", this, b, srcIndex, _values[srcIndex], _values[srcIndex]->getID(), dstIndex, _values[dstIndex], _values[dstIndex]->getID());
   }
