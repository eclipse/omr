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

#ifndef OMR_VIRTUALMACHINEINTERPRETERARRAY_INCL
#define OMR_VIRTUALMACHINEINTERPRETERARRAY_INCL

#include <stdint.h>
#include "ilgen/VirtualMachineArray.hpp"

namespace TR { class IlBuilder; }
namespace TR { class IlType; }
namespace TR { class IlValue; }
namespace TR { class MethodBuilder; }
namespace TR { class VirtualMachineRegister; }
namespace TR { class VirtualMachineInterpreterArray; }

namespace OMR
{
/**
 * @brief Implementation for reading / writing directly to a virtual machine array
 *
 * VirtualMachineInterpreterArray subclasses VirtualMachineArray to provide a concrete
 * implementation that reads / writes directly to the VirtualMachineRegister arrayBase.
 *
 * VirtualMachineInterpreterArray implements VirtualMachineArray:
 * Commit() no-op
 * Reload() no-op
 * MakeCopy() returns self
 * MergeInto() no-op
 *
 * VirtualMachineInterpreterArray array-y operations:
 *   Get() gets a TR::IlValue from the VirtualMachineRegister arrayBase
 *   Set() sets a TR::IlValue into `index' of the VirtualMachineRegister arrayBase
 *   Move() copies an element from srcIndex to dstIndex in the VirtualMachineRegister arrayBase
 */

class VirtualMachineInterpreterArray : public TR::VirtualMachineArray
   {
   public:
   /**
    * @brief public constructor, must be instantiated inside a compilation because uses heap memory
    * @param mb TR::MethodBuilder object of the method currently being compiled
    * @param elementType TR::IlType representing the underlying type of the virtual machine's operand array entries
    * @param arrayBase previously allocated and initialized VirtualMachineRegister representing the base of the array
    */
   VirtualMachineInterpreterArray(TR::MethodBuilder *mb, TR::IlType *elementType, TR::VirtualMachineRegister *arrayBase);

   /**
    * @brief does nothing since the implementation reads / writes directly to the virtual machine array
    * @param b the builder where the operations will be placed to recreate the virtual machine operand array
    */
   virtual void Commit(TR::IlBuilder *b) {}
   
   /**
    * @brief does nothing since the implementation reads / writes directly to the virtual machine array
    * @param b the builder where the operations will be placed to recreate the array
    */
   virtual void Reload(TR::IlBuilder *b) {}

   /**
    * @brief returns self
    * @returns the urrent object
    */
   virtual TR::VirtualMachineState *MakeCopy();

   /**
    * @brief does nothing since the implementation reads / writes directly to the virtual machine array
    * @param other array for the builder object control is merging into
    * @param b builder object where the operations will be added to make the current array the same as the other
    */
   virtual void MergeInto(TR::VirtualMachineState *other, TR::IlBuilder *b) {}
   
   /**
    * @brief update the values used to read and write the virtual machine array
    * @param b the builder where the values will be placed
    * @param array the new array base address.
    */
   virtual void UpdateArray(TR::IlBuilder *b, TR::IlValue *array);

   /**
    * @brief Returns the expression at the given index of the array
    * @param index the location of the expression to return
    * @returns the expression at the given index
    */
   virtual TR::IlValue *Get(TR::IlBuilder *b, int32_t index);
   
   /**
    * @brief Returns the expression at the given index of the array
    * @param index an IlValue representing the location of the expression to return
    * @returns the expression at the given index
    */
   virtual TR::IlValue *Get(TR::IlBuilder *b, TR::IlValue *index);

   /**
    * @brief Set the expression into the array at the given index
    * @param index the location to store the expression
    * @param value expression to store into the array
    */
   virtual void Set(TR::IlBuilder *b, int32_t index, TR::IlValue *value);

   /**
    * @brief Set the expression into the array at the given index
    * @param index the location to store the expression
    * @param value expression to store into the array
    */
   virtual void Set(TR::IlBuilder *b, TR::IlValue *index, TR::IlValue *value);
  
   /**
    * @brief Move the expression from one index to another index in the array
    * @param dstIndex the location to store the expression
    * @param srcIndex the location to copy the expression from
    */ 
   virtual void Move(TR::IlBuilder *b, int32_t dstIndex, int32_t srcIndex);

   /**
    * @brief Move the expression from one index to another index in the array
    * @param dstIndex the location to store the expression
    * @param srcIndex the location to copy the expression from
    */
   virtual void Move(TR::IlBuilder *b, TR::IlValue *dstIndex, TR::IlValue *srcIndex);

   protected:
   void init();

   private:
   TR::MethodBuilder *_mb;
   TR::VirtualMachineRegister *_arrayBaseRegister;
   TR::IlType *_elementType;
   };
}
#endif // !defined(OMR_VIRTUALMACHINEINTERPRETERARRAY_INCL)
