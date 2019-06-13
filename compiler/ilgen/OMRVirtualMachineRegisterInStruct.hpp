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

#ifndef OMR_VIRTUALMACHINEREGISTERINSTRUCT_INCL
#define OMR_VIRTUALMACHINEREGISTERINSTRUCT_INCL

#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineRegisterInStruct.hpp"
#include "ilgen/IlBuilder.hpp"

namespace TR { class VirtualMachineRegisterInStruct; }

namespace OMR
{

/**
 * @brief used to represent virtual machine variables that are maintained in a structure stored in a local variable, such as a thread or frame object passed as a parameter to the method
 *
 * The value does not need to be a virtual machine register, but often it is the registers
 * of the virtual machine that are candidates for VirtualMachineRegisterInStruct. An
 * alternative is VirtualMachineRegister, which can be more convenient if the virtual
 * machine value is stored in a more arbitrary place or in a structure that isn't readily
 * accessible inside the compiled method. 
 * VirtualMachineRegisterInStruct is a subclass of VirtualMachineRegister
 *
 * The simulated register value is simply stored in a single local variable, which
 * gives the compiler visibility to all changes to the register (and enables
 * optimization / simplification). Because there is just a single local variable,
 * the Merge() function does not need to do anything (the value is accessible from
 * the same location at all locations). The Commit() and Reload() functions simply
 * move the value back and forth between the local variable and the structure that
 * holds the actual virtual machine state.
 */

class VirtualMachineRegisterInStruct : public TR::VirtualMachineRegister
   {
   public:
   /**
    * @brief public constructor used to create a virtual machine state variable from struct
    * @param b a builder object where the first Reload operations will be inserted
    * @param structName the name of the struct type that holds the virtual machine state variable
    * @param localNameHoldingStructAddress is the name of a local variable holding the struct base address; it must have been stored in this name before control will reach the builder "b"
    * @param fieldName name of the field in "structName" that holds the virtual machine state variable
    * @param localName the name of the local variable where the simulated value is to be stored
    */
   VirtualMachineRegisterInStruct(TR::IlBuilder *b,
                                  const char * const structName,
                                  const char * const localNameHoldingStructAddress,
                                  const char * const fieldName,
                                  const char * const localName) :
      TR::VirtualMachineRegister(localName),
      _structName(structName),
      _fieldName(fieldName),
      _localNameHoldingStructAddress(localNameHoldingStructAddress)
      {
      _integerTypeForAdjustments = b->typeDictionary()->GetFieldType(structName, fieldName);
      if (_integerTypeForAdjustments->isPointer())
         {
         TR::IlType *baseType = _integerTypeForAdjustments->baseType();
         _integerTypeForAdjustments = b->typeDictionary()->getWord();
         _isAdjustable = true;
         _adjustByStep = baseType->getSize();
         }
      else
         {
         _isAdjustable = false;
         _adjustByStep = 0;
         }
      Reload(b);
      }

   /**
    * @brief public constructor used to create a virtual machine state variable from struct
    * @param structName the name of the struct type that holds the virtual machine state variable
    * @param localNameHoldingStructAddress is the name of a local variable holding the struct base address; it must have been stored in this name before control will reach the builder "b"
    * @param fieldName name of the field in "structName" that holds the virtual machine state variable
    * @param localName the name of the local variable where the simulated value is to be stored
    */
   VirtualMachineRegisterInStruct(const char * const structName,
                                  const char * const localNameHoldingStructAddress,
                                  const char * const fieldName,
                                  const char * const localName) :
      TR::VirtualMachineRegister(localName),
      _structName(structName),
      _fieldName(fieldName),
      _localNameHoldingStructAddress(localNameHoldingStructAddress)
      {
      }

   /**
    * @brief write the simulated register value to the proper field in the struct
    * @param b a builder object where the operations to do the write will be inserted
    */
   virtual void Commit(TR::IlBuilder *b)
      {
      b->StoreIndirect(_structName, _fieldName,
      b->   Load(_localNameHoldingStructAddress),
      b->   Load(_localName));
      }

   /**
    * @brief read the simulated register value from the proper field in the struct
    * @param b a builder object where the operations to do the write will be inserted
    */
   virtual void Reload(TR::IlBuilder *b)
      {
      b->Store(_localName,
      b->   LoadIndirect(_structName, _fieldName,
      b->      Load(_localNameHoldingStructAddress)));
      }

   /**
    * @brief create an identical copy of the current object.
    * @returns the copy of the current object
    */
   virtual TR::VirtualMachineState *MakeCopy();

   /**
    * @brief returns the client object associated with this object
    */
   virtual void *client();

   /**
    * @brief Set the Client Allocator function
    */
   static void setClientAllocator(ClientAllocator allocator)
      {
      _clientAllocator = allocator;
      }

   /**
    * @brief Set the Get Impl function
    *
    * @param getter function pointer to the impl getter
    */
   static void setGetImpl(ImplGetter getter)
      {
      _getImpl = getter;
      }

   protected:
   const char * const   _structName;
   const char * const   _fieldName;
   const char * const   _localNameHoldingStructAddress;

private:
   static ClientAllocator _clientAllocator;
   static ImplGetter _getImpl;
   };
}

#endif // !defined(OMR_VIRTUALMACHINEREGISTERINSTRUCT_INCL)
