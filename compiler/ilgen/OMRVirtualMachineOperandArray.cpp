/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#include "ilgen/VirtualMachineOperandArray.hpp"

#include "compile/Compilation.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineState.hpp"

#define TraceEnabled    (TR::comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(TR::comp(), m, ##__VA_ARGS__);}}

OMR::VirtualMachineOperandArray::VirtualMachineOperandArray(TR::MethodBuilder *mb, int32_t numOfElements, TR::IlType *elementType, TR::VirtualMachineRegister *arrayBaseRegister)
   : TR::VirtualMachineArray(),
   _mb(mb),
   _numberOfElements(numOfElements),
   _elementType(elementType),
   _arrayBaseRegister(arrayBaseRegister)
   {
   init();
   }

OMR::VirtualMachineOperandArray::VirtualMachineOperandArray(TR::VirtualMachineOperandArray *other)
   : TR::VirtualMachineArray(),
   _mb(other->_mb),
   _numberOfElements(other->_numberOfElements),
   _elementType(other->_elementType),
   _arrayBaseRegister(other->_arrayBaseRegister),
   _arrayBaseName(other->_arrayBaseName)
   {
   int32_t numBytes = _numberOfElements * sizeof(TR::IlValue *);
   _values = (TR::IlValue **) TR::comp()->trMemory()->allocateHeapMemory(numBytes);
   memcpy(_values, other->_values, numBytes);
   }


// commits the simulated operand array of values to the virtual machine state
// the given builder object is where the operations to commit the state will be inserted
// into the array which is assumed to be managed independently, most likely
void
OMR::VirtualMachineOperandArray::Commit(TR::IlBuilder *b)
   {
   TR::IlType *Element = _elementType;
   TR::IlType *pElement = _mb->typeDictionary()->PointerTo(Element);

   TR::IlValue *arrayBase = b->Load(_arrayBaseName);

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
   TR::IlValue* array = b->Load(_arrayBaseName);
   for (int32_t i = 0; i < _numberOfElements; i++)
      {
      _values[i] = b->LoadAt(pElement,
                   b->   IndexAt(pElement,
                            array,
                   b->      ConstInt32(i)));
       }
   }

void
OMR::VirtualMachineOperandArray::MergeInto(TR::VirtualMachineState *o, TR::IlBuilder *b)
   {
   TR::VirtualMachineOperandArray *other = (TR::VirtualMachineOperandArray *)o;
   TR_ASSERT(_numberOfElements == other->_numberOfElements, "array are not same size");
   for (int32_t i=0;i < _numberOfElements;i++)
      {
      if (NULL == other->_values[i])
         {
         TR_ASSERT(_values[i] == NULL, "if an element in the dest array at index is NULL it has to be NULL in the src array");
         }
      else if (other->_values[i]->getID() != _values[i]->getID())
         {
         // Types have to match!!!
         TR_ASSERT(_values[i]->getDataType() == other->_values[i]->getDataType(), "invalid array merge: primitive type mismatch at same index");
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
   b->Store(_arrayBaseName, array);
   }

// Allocate a new operand array and copy everything in this state
// If VirtualMachineOperandArray is subclassed, this function *must* also be implemented in the subclass!
TR::VirtualMachineState *
OMR::VirtualMachineOperandArray::MakeCopy()
   {
   TR::VirtualMachineOperandArray *copy = (TR::VirtualMachineOperandArray *) TR::comp()->trMemory()->allocateHeapMemory(sizeof(TR::VirtualMachineOperandArray));
   new (copy) TR::VirtualMachineOperandArray(static_cast<TR::VirtualMachineOperandArray *>(this));

   return copy;
   }

TR::IlValue *
OMR::VirtualMachineOperandArray::Get(TR::IlBuilder *b, int32_t index)
   {
   TR_ASSERT(index < _numberOfElements, "index has to be less than the number of elements");
   TR_ASSERT(index >= 0, "index can not be negative");
   return _values[index];
   }

TR::IlValue *
OMR::VirtualMachineOperandArray::Get(TR::IlBuilder *b, TR::IlValue *index)
   {
   TR_ASSERT(NULL != index, "index can not be NULL");
   return Get(b, index->get32bitConstValue());
   }

void
OMR::VirtualMachineOperandArray::Set(TR::IlBuilder *b, int32_t index, TR::IlValue *value)
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
OMR::VirtualMachineOperandArray::Set(TR::IlBuilder *b, TR::IlValue *index, TR::IlValue *value)
   {
   TR_ASSERT(NULL != index, "index can not be NULL");
   Set(b, index->get32bitConstValue(), value);
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

void
OMR::VirtualMachineOperandArray::Move(TR::IlBuilder *b, TR::IlValue *dstIndex, TR::IlValue *srcIndex)
   {
   Move(b, dstIndex->get32bitConstValue(), srcIndex->get32bitConstValue());
   }

void
OMR::VirtualMachineOperandArray::init()
   {
   int32_t numBytes = _numberOfElements * sizeof(TR::IlValue *);
   _values = (TR::IlValue **) TR::comp()->trMemory()->allocateHeapMemory(numBytes);
   memset(_values, 0, numBytes);

   TR::Compilation *comp = TR::comp();
   // Create a temp for the OperandArray base
   TR::SymbolReference *symRef = comp->getSymRefTab()->createTemporary(_mb->methodSymbol(), _mb->typeDictionary()->PointerTo(_elementType)->getPrimitiveType());
   symRef->getSymbol()->setNotCollected();
   char *name = (char *) comp->trMemory()->allocateHeapMemory((11+10+1) * sizeof(char)); // 11 ("_ArrayBase_") + max 10 digits + trailing zero
   sprintf(name, "_ArrayBase_%u", symRef->getCPIndex());
   symRef->getSymbol()->getAutoSymbol()->setName(name);
   _mb->defineSymbol(name, symRef);

   _arrayBaseName = symRef->getSymbol()->getAutoSymbol()->getName();

   // store current operand stack pointer base address so we can use it whenever we need
   // to recreate the stack as the interpreter would have
   _mb->Store(_arrayBaseName, _arrayBaseRegister->Load(_mb));
   }
