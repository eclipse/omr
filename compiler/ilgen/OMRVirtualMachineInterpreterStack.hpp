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

#ifndef OMR_VIRTUALMACHINEINTERPRETERSTACK_INCL
#define OMR_VIRTUALMACHINEINTERPRETERSTACK_INCL

#include <stdint.h>

#include "ilgen/VirtualMachineStack.hpp"

namespace TR { class IlBuilder; }
namespace TR { class IlValue; }
namespace TR { class IlType; }
namespace TR { class MethodBuilder; }
namespace TR { class VirtualMachineInterpreterStack; }
namespace TR { class VirtualMachineRegister; }

namespace OMR
{

/**
 * @brief simulates an operand stack used by many bytecode based virtual machines
 * In such virtual machines, the operand stack holds the intermediate expression values
 * computed by the bytecodes. The compiler simulates this operand stack as well, but
 * what is pushed to and popped from the simulated operand stack are expression nodes
 * that represent the value computed by the bytecodes. As each bytecode pops expression
 * nodes off the operand stack, it combines them to form more complicated expressions
 * which are then pushed back onto the operand stack for consumption by other bytecodes.
 *
 * VirtualMachineInterpreterStack implements VirtualMachineState:
 * Commit() no-op
 * Reload() no-op
 * MakeCopy() returns this
 * MergeInto() no-op
 *
 * VirtualMachineInterpreterStack provides several stack-y operations:
 *   Push() pushes a TR::IlValue onto the stack
 *   Pop() pops and returns a TR::IlValue from the stack
 *   Top() returns the TR::IlValue at the top of the stack
 *   Pick() returns the TR::IlValue "depth" elements from the top
 *   Drop() discards "depth" elements from the stack
 *   Dup() is a convenience function for Push(Top())
 *
 */

class VirtualMachineInterpreterStack : public TR::VirtualMachineStack
   {
   public:

  /**
    * @brief public constructor, must be instantiated inside a compilation because uses heap memory
    * @param mb TR::MethodBuilder object of the method currently being compiled
    * @param stackTop previously allocated and initialized VirtualMachineRegister representing the top of stack
    * @param elementType TR::IlType representing the underlying type of the virtual machine's operand stack entries
    * @param growsUp to configure virtual machine stack growth direction, set to true if virtual machine stack grows towards larger addresses, false otherwise
    * @param preAdjust is true if the stack is to be adjusted before storing, false otherwise
    */
   VirtualMachineInterpreterStack(TR::MethodBuilder *mb, TR::VirtualMachineRegister *stackTopRegister, TR::IlType *elementType, bool growsUp = true, bool preAdjust = true);

   /**
    * @brief does not do anything since this stack reads//write directly to the virtual machine stack
    * @param b the builder where the operations will be placed to recreate the virtual machine operand stack
    */
   virtual void Commit(TR::IlBuilder *b) {}

   /**
    * @brief does not do anything since this stack reads//write directly to the virtual machine stack
    * @param b the builder where the operations will be placed to recreate the stack
    */
   virtual void Reload(TR::IlBuilder *b) {}

   /**
    * @brief return the current object.
    * @returns the current object
    */
   virtual TR::VirtualMachineState *MakeCopy();

   /**
    * @brief does not do anything as the stacks should all be the same object
    * @param other stack for the builder object control is merging into
    * @param b builder object where the operations will be added to make the current stack the same as the other
    */
   virtual void MergeInto(TR::VirtualMachineState *other, TR::IlBuilder *b) {}

   /**
    * @brief update the values used to read and write the virtual machine stack
    * @param b the builder where the values will be placed
    * @param stack the new stack base address.  It is assumed that the address is already adjusted to _stackOffset
    */
   virtual void UpdateStack(TR::IlBuilder *b, TR::IlValue *stack);

   /**
    * @brief Push an expression onto the stack
    * @param b builder object to use for any operations used to implement the push (e.g. update the top of stack)
    * @param value expression to push onto the stack
    */
   virtual void Push(TR::IlBuilder *b, TR::IlValue *value);

   /**
    * @brief Pops an expression from the top of the stack
    * @param b builder object to use for any operations used to implement the pop (e.g. to update the top of stack)
    * @returns expression popped from the stack
    */
   virtual TR::IlValue *Pop(TR::IlBuilder *b);

   /**
    * @brief Returns the expression at the top of the stack
    * @param b builder object to use for any operations used to implement the pop (e.g. to update the top of stack)
    * @returns the expression at the top of the stack
    */
   virtual TR::IlValue *Top(TR::IlBuilder *b);

   /**
    * @brief Returns an expression below the top of the stack
    * @param depth number of values below top (Pick(0) is same as Top())
    * @returns the requested expression from the stack
    */
   virtual TR::IlValue *Pick(TR::IlBuilder *b, int32_t depth);

   /**
    * @brief Returns an expression below the top of the stack
    * @param depth number of values below top (Pick(0) is same as Top())
    * @returns the requested expression from the stack
    */
   virtual TR::IlValue *Pick(TR::IlBuilder *b, TR::IlValue *depth);

   /**
    * @brief Removes some number of expressions from the stack
    * @param b builder object to use for any operations used to implement the drop (e.g. to update the top of stack)
    * @param depth how many values to drop from the stack
    */
   virtual void Drop(TR::IlBuilder *b, int32_t depth);

   /**
    * @brief Removes some number of expressions from the stack
    * @param b builder object to use for any operations used to implement the drop (e.g. to update the top of stack)
    * @param depth how many values to drop from the stack
    */
   virtual void Drop(TR::IlBuilder *b, TR::IlValue *depth);

   /**
    * @brief Duplicates the expression on top of the stack
    * @param b builder object to use for any operations used to duplicate the expression (e.g. to update the top of stack)
    */
   virtual void Dup(TR::IlBuilder *b);

   protected:
   void init();

   private:
   TR::MethodBuilder *_mb;
   TR::VirtualMachineRegister *_stackTopRegister;
   TR::IlType *_elementType;
   const char *_stackBaseName;
   bool _growsUp;
   bool _preAdjust;
   };
}
#endif // !defined(OMR_VIRTUALMACHINEINTERPRETERSTACK_INCL)
