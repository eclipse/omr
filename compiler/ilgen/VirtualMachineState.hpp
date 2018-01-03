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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_VIRTUALMACHINESTATE_INCL
#define OMR_VIRTUALMACHINESTATE_INCL


namespace TR { class IlBuilder; }

namespace OMR
{

/**
 * @brief Interface for expressing virtual machine state variables to JitBuilder
 *
 * VirtualMachineState is an interface that language compilers will implement
 * to define all the aspects of the virtual machine state that the JIT compiler will
 * simulate (operand stacks, virtual registers like "pc", "sp", etc.). The interface
 * has four functions: 1) Commit() to cause the simulated state to be written to the
 * actual virtual machine state, 2) Reload() to set the simulated machine state based
 * on the current virtual machine state, 3) MakeCopy() to make an identical copy of
 * the curent virtual machine state object, and 4) MergeInto() to perform the actions
 * needed for the current state to be merged with a second virtual machine state.
 *
 * ##Usage
 *
 * Commit() is typically called when transitioning from compiled code to the interpreter.
 *
 * Reload() is typically called on the transition back from the interpreter to compiled
 * code.
 *
 * MakeCopy() is typically called when control flow edges are created: the current vm state
 * must also become the initial vm state at the target of the control flow edge and be
 * able to evolve independently from the current vm state.
 *
 * MergeInto() is typically called when one compiled code path needs to merge into a second
 * compiled code path (think the bottom of an if-then-else control flow diamond). At the
 * merge point, the locations of all aspects of the simulated machine state must be
 * identical so that the code below the merge point will compute correct results.
 * MergeInto() typically causes the current simulated machine state to be written to the
 * locations that were used by an existing simulated machine state. The "other" object
 * passed to MergeInto() must have the exact same shape (number of components and each
 * component must also match in its type).
 *
 * Language compilers should extend this base class, add instance variables for all
 * necessary virtual machine state variables (using classes like VirtualMachineRegister
 * and VirtualMachineOperandStack) and then implement Commit(), Reload(), MakeCopy(),
 * and MergeInto() to call Commit(), Reload(), MakeCopy(), and MergeInto(), respectively, on
 * each individual instance variable. It feels like boilerplate, but this approach makes
 * it easy for the compiler to reference individual virtual machine state variables and
 * at the same time permits complete flexibility in how the VM-wide Commit(), Reload(),
 * MakeCopy(), and MergeInto() operations can be implemented.
 */

class VirtualMachineState
   {
   public:

   /**
    * @brief Cause all simulated aspects of the virtual machine state to become real.
    * @param b builder object where the operations will be added to change the virtual machine state.
    *
    * The builder object b is assumed to be along a control flow path transitioning
    * from compiled code to the interpreter. Base implementation does nothing.
    */
   virtual void Commit(TR::IlBuilder *b) { }

   /**
    * @brief Load the current virtual machine state into the simulated variables used by compiled code.
    * @param b builder object where the operations will be added to reload the virtual machine state.
    *
    * The builder object b is assumed to be along a control flow path transitioning
    * from the interpreter to compiled code. Base implementation does nothing.
    */
   virtual void Reload(TR::IlBuilder *b) { }

   /**
    * @brief create an identical copy of the current object.
    * @returns the copy of the current object
    *
    * Typically used when propagating the current state along a flow edge to another builder to
    * capture the input state for that other builder.
    * Default implementation simply returns the current object.
    */
   virtual VirtualMachineState *MakeCopy() { return this; }

   /**
    * @brief cause the current state variables to match those used by another vm state
    * @param other current state for the builder object control is merging into
    * @param b builder object where the operations will be added to merge this state into the other
    *
    * The builder object is assumed to be along the control flow edge from one builder object S to
    * another builder object T. "this" vm state is assumed to be the vm state for S. "other"  is
    * assumed to be the vm state for T. Control from S should be to "b", and "b" should eventually
    * transfer to T. Base implementation does nothing.
    */
   virtual void MergeInto(OMR::VirtualMachineState *other, TR::IlBuilder *b) { }

   };

}

#endif // !defined(OMR_VIRTUALMACHINESTATE_INCL)
