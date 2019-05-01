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

#ifndef OMR_VIRTUALMACHINEREGISTER_INCL
#define OMR_VIRTUALMACHINEREGISTER_INCL

#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"

namespace TR { class IlBuilder; }
namespace TR { class VirtualMachineRegister; }

namespace OMR
{

/**
 * @brief simulate virtual machine state variable via an address
 * 
 * VirtualMachineRegister can be used to represent values in the virtual machine
 * at any address. The value does not need to be a virtual machine register, but
 * often it is the registers of the virtual machine that are candidates for
 * VirtualMachineRegister. An alternative is VirtualMachineRegisterInStruct,
 * which may be more convenient if the virtual machine value is stored in a
 * structure that the compiled method has easy access to (for example, if the
 * base address of the struct is a parameter of every compiled method such as
 * a "thread" structure or a "frame" structure).
 *
 * The simulated register value is simply stored in a single local variable in the
 * native stack frame, which gives the compiler visibility to all changes to the
 * register (and enables dataflow optimization / simplification). Because there is
 * just a single local variable, the CopyState() and MergeInto() functions do not
 * need to do anything (the value can accessible from that variable at all program
 * locations). The Commit() and Reload() functions simply move the value back and
 * forth between the local variable and the address of the actual virtual machine
 * state variable.
 *
 * VirtualMachineRegister provides two additional functions:
 * Load() will load the *simulated* value of the register for use in the builder "b"
 * Store() will store the provided "value" into the *simulated* register by
 * appending to the builder "b"
 */

class VirtualMachineRegister : public TR::VirtualMachineState
   {
   protected:
   /**
    * @brief constructor can be used by subclasses to initialize just the const pointer
    * @param localName the name of the local variable where the simulated value is to be stored
    */
   VirtualMachineRegister(const char * const localName)
      : TR::VirtualMachineState(),
      _localName(localName)
      { }

   public:
   /**
    * @brief public constructor used to create a virtual machine state variable
    * @param b a builder object where the first Reload operations will be inserted
    * @param localName the name of the local variable where the simulated value is to be stored
    * @param pointerToRegisterType must be pointer to the type of the register
    * @param adjustByStep is a multiplier for the value passed to Adjust()
    * @param addressOfRegister is the address of the actual register
    */
   VirtualMachineRegister(TR::IlBuilder *b,
                          const char * const localName,
                          TR::IlType * pointerToRegisterType,
                          uint32_t adjustByStep,
                          TR::IlValue * addressOfRegister);
   /**
    * @brief public constructor used to create a virtual machine state variable
    * @param localName the name of the local variable where the simulated value is to be stored
    * @param pointerToRegisterType must be pointer to the type of the register
    * @param adjustByStep is a multiplier for the value passed to Adjust()
    * @param addressOfRegister is the address of the actual register
    */
   VirtualMachineRegister(const char * const localName,
                          TR::IlType * pointerToRegisterType,
                          uint32_t adjustByStep,
                          TR::IlValue * addressOfRegister)
      : TR::VirtualMachineState(),
      _localName(localName),
      _addressOfRegister(addressOfRegister),
      _pointerToRegisterType(pointerToRegisterType),
      _adjustByStep(adjustByStep)
      {
      }

   /**
    * @brief write the simulated register value to the virtual machine
    * @param b the builder where the operation will be placed to store the virtual machine value
    */
   virtual void Commit(TR::IlBuilder *b)
      {
      b->StoreAt(
            _addressOfRegister,
      b->   Load(_localName));
      }

   /**
    * @brief transfer the current virtual machine register value to the simulated local variable
    * @param b the builder where the operation will be placed to load the virtual machine value
    */
   virtual void Reload(TR::IlBuilder *b)
      {
      b->Store(_localName,
      b->   LoadAt((TR::IlType *)_pointerToRegisterType,
               _addressOfRegister));
      }

   /**
    * @brief create an identical copy of the current object.
    * @returns the copy of the current object
    */
   virtual TR::VirtualMachineState *MakeCopy();

   /**
    * @brief used in the compiled method to load the (simulated) register's value
    * @param b the builder where the operation will be placed to load the simulated value
    * @returns TR:IlValue * for the loaded simulated register value
    */
   TR::IlValue *Load(TR::IlBuilder *b)
      {
      return b->Load(_localName);
      }

   /**
    * @brief used in the compiled method to store to the (simulated) register's value
    * @param b the builder where the operation will be placed to store to the simulated value
    */
   void Store(TR::IlBuilder *b, TR::IlValue *value)
      {
      b->Store(_localName, value);
      }

   /**
    * @brief used in the compiled method to add to the (simulated) register's value
    * @param b the builder where the operation will be placed to add to the simulated value
    * @param amount the TR::IlValue that should be added to the simulated value, after multiplication by _adjustByStep
    * Adjust() is really a convenience function for the common operation of adding a value to the register. More
    * complicated operations (e.g. multiplying the value) can be built using Load() and Store() if needed.
    */
   virtual void Adjust(TR::IlBuilder *b, TR::IlValue *amount)
      {
      TR::IlValue *off=b->Mul(amount,
                       b->    ConstInteger(_integerTypeForAdjustments, _adjustByStep));
      adjust(b, off);
      }

   /**
    * @brief used in the compiled method to add to the (simulated) register's value
    * @param b the builder where the operation will be placed to add to the simulated value
    * @param amount the constant value that should be added to the simulated value, after multiplication by _adjustByStep
    * Adjust() is really a convenience function for the common operation of adding a value to the register. More
    * complicated operations (e.g. multiplying the value) can be built using Load() and Store() if needed.
    */
   virtual void Adjust(TR::IlBuilder *b, int64_t amount)
      {
      adjust(b, b->ConstInteger(_integerTypeForAdjustments, amount * _adjustByStep));
      }

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
   void adjust(TR::IlBuilder *b, TR::IlValue *rawAmount);

   const char  * const _localName;
   TR::IlValue * _addressOfRegister;
   TR::IlType  * _pointerToRegisterType;
   TR::IlType  * _integerTypeForAdjustments;
   uint32_t      _adjustByStep;
   bool          _isAdjustable;

private:
   static ClientAllocator _clientAllocator;
   static ImplGetter _getImpl;
   };
}

#endif // !defined(OMR_VIRTUALMACHINEREGISTER_INCL)
