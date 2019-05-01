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

#ifndef OMR_VIRTUALMACHINEOPERANDSTACK_INCL
#define OMR_VIRTUALMACHINEOPERANDSTACK_INCL

#include <stdint.h>

#include "ilgen/VirtualMachineState.hpp"

namespace TR { class IlBuilder; }
namespace TR { class IlValue; }
namespace TR { class IlType; }
namespace TR { class MethodBuilder; }
namespace TR { class VirtualMachineOperandStack; }
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
 * The stack is represented as an array of pointers to TR::IlValue's, making it
 * easy to use IlBuilder services to consume and compute new values. Note that,
 * unlike VirtualMachineRegister, the simulated operand stack is *not* maintained
 * by the method code as part of the method's stack frame. This approach requires
 * modelling the state of the operand stack at all program points, which means
 * there cannot only be one VirtualMachineOperandStack object.
 *
 * The current implementation does not share anything among different
 * VirtualMachineOperandStack objects. Possibly, some of the state could be
 * shared to save some memory. For now, simplicity is the goal.
 *
 * VirtualMachineOperandStack implements VirtualMachineState:
 * Commit() simply iterates over the simulated operand stack and stores each
 *   value onto the virtual machine's operand stack (more details at definition).
 * Reload() is left empty; assumption is that each BytecodeBuilder handler will
 *   update the state of the operand stack appropriately on return from the
 *   interpreter.
 * MakeCopy() copies the state of the operand stack
 * MergeInto() is slightly subtle. Operations may have been already created
 *   below the merge point, and those operations will have assumed the
 *   expressions are stored in the TR::IlValue's for the state being merged
 *   *to*. So the purpose of MergeInto() is to store the values of the current
 *   state into the same variables as in the "other" state.
 * 
 * VirtualMachineOperandStack provides several stack-y operations:
 *   Push() pushes a TR::IlValue onto the stack
 *   Pop() pops and returns a TR::IlValue from the stack
 *   Top() returns the TR::IlValue at the top of the stack
 *   Pick() returns the TR::IlValue "depth" elements from the top
 *   Drop() discards "depth" elements from the stack
 *   Dup() is a convenience function for Push(Top())
 *
 */

class VirtualMachineOperandStack : public TR::VirtualMachineState
   {
   public:
  
  /**
    * @brief public constructor, must be instantiated inside a compilation because uses heap memory
    * @param mb TR::MethodBuilder object of the method currently being compiled
    * @param sizeHint initial size used to allocate the stack; will grow larger if needed
    * @param elementType TR::IlType representing the underlying type of the virtual machine's operand stack entries
    * @param stackTop previously allocated and initialized VirtualMachineRegister representing the top of stack
    * @param growsUp to configure virtual machine stack growth direction, set to true if virtual machine stack grows towards larger addresses, false otherwise 
    * @param stackInitialOffset to configure virtual machine stack stack offset 
    * set to the difference in elements between initial stack pointer and actual bottom of stack
    * Some stacks Push by incrementing stack pointer then storing, some by storing and then
    * incrementing stack pointer. In the first case, stackInitialOffset should be -1
    * because the stack pointer initially points one element below the bottom of the stack.
    * In the second case, stackInitialOffset should be 0, because the stack pointer
    * initially points at the bottom of the stack. Other values are possible but would be
    * considered highly unusual. 
    * Default behaviour for compatibility constructor will be optional arguments, growsUp is true, and stackInitialOffset is -1.
    */

   VirtualMachineOperandStack(TR::MethodBuilder *mb, int32_t sizeHint, TR::IlType *elementType, TR::VirtualMachineRegister *stackTopRegister,
    bool growsUp = true, int32_t stackInitialOffset = -1);

   /**
    * @brief constructor used to copy the stack from another state
    * @param other the operand stack whose values should be used to initialize this object
    */
   VirtualMachineOperandStack(TR::VirtualMachineOperandStack *other);

   /**
    * @brief write the simulated operand stack to the virtual machine
    * @param b the builder where the operations will be placed to recreate the virtual machine operand stack
    */
   virtual void Commit(TR::IlBuilder *b);
   
   /**
    * @brief read the virtual machine stack back into the simulated operand stack
    * @param b the builder where the operations will be placed to recreate the simulated operand stack 
    * Users can call Drop beforehand with the appropriate positive or negative count to ensure the simulated
    * stack accounts for new or dropped virtual machine stack elements. 
    */
   virtual void Reload(TR::IlBuilder *b);

   /**
    * @brief create an identical copy of the current object.
    * @returns the copy of the current object
    */
   virtual TR::VirtualMachineState *MakeCopy();

   /**
    * @brief emit operands to store current operand stack values into same variables as used in another operand stack
    * @param other operand stack for the builder object control is merging into
    * @param b builder object where the operations will be added to make the current operand stack the same as the other
    */
   virtual void MergeInto(TR::VirtualMachineState *o, TR::IlBuilder *b);

   /**
    * @brief update the values used to read and write the virtual machine stack
    * @param b the builder where the values will be placed
    * @param stack the new stack base address.  It is assumed that the address is already adjusted to _stackOffset
    */
   virtual void UpdateStack(TR::IlBuilder *b, TR::IlValue *stack);

   /**
    * @brief Push an expression onto the simulated operand stack
    * @param b builder object to use for any operations used to implement the push (e.g. update the top of stack)
    * @param value expression to push onto the simulated operand stack
    */
   virtual void Push(TR::IlBuilder *b, TR::IlValue *value);

   /**
    * @brief Pops an expression from the top of the simulated operand stack
    * @param b builder object to use for any operations used to implement the pop (e.g. to update the top of stack)
    * @returns expression popped from the stack
    */
   virtual TR::IlValue *Pop(TR::IlBuilder *b);

   /**
    * @brief Returns the expression at the top of the simulated operand stack
    * @returns the expression at the top of the operand stack
    */
   virtual TR::IlValue *Top();

   /**
    * @brief Returns an expression below the top of the simulated operand stack
    * @param depth number of values below top (Pick(0) is same as Top())
    * @returns the requested expression from the operand stack
    */
   virtual TR::IlValue *Pick(int32_t depth);

   /**
    * @brief Removes some number of expressions from the operand stack
    * @param b builder object to use for any operations used to implement the drop (e.g. to update the top of stack)
    * @param depth how many values to drop from the stack
    */
   virtual void Drop(TR::IlBuilder *b, int32_t depth);

   /**
    * @brief Duplicates the expression on top of the simulated operand stack
    * @param b builder object to use for any operations used to duplicate the expression (e.g. to update the top of stack)
    */
   virtual void Dup(TR::IlBuilder *b);

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
   void copyTo(TR::VirtualMachineOperandStack *copy);
   void checkSize();
   void grow(int32_t growAmount = 0);
   void init();

   private:
   TR::MethodBuilder *_mb;
   TR::VirtualMachineRegister *_stackTopRegister;
   int32_t _stackMax;
   int32_t _stackTop;
   TR::IlValue **_stack;
   TR::IlType *_elementType;
   int32_t _pushAmount;
   int32_t _stackOffset;
   const char *_stackBaseName;

   static ClientAllocator _clientAllocator;
   static ImplGetter _getImpl;
   };
}

#endif // !defined(OMR_VIRTUALMACHINEOPERANDSTACK_INCL)
