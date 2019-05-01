/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef OMR_BYTECODEBUILDER_INCL
#define OMR_BYTECODEBUILDER_INCL

#include "ilgen/IlBuilder.hpp"

namespace TR { class BytecodeBuilder; }
namespace TR { class MethodBuilder; }
namespace TR { class VirtualMachineState; }

namespace OMR
{

class BytecodeBuilder : public TR::IlBuilder
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   BytecodeBuilder(TR::MethodBuilder *methodBuilder, int32_t bcIndex, char *name=NULL, int32_t bcLength=-1);

   virtual bool isBytecodeBuilder() { return true; }

   /**
    * @brief bytecode index for this builder object
    */
   int32_t bcIndex() { return _bcIndex; }
   virtual int32_t currentByteCodeIndex() { return _bcIndex; } // override from IlGenerator

   /* @brief after calling this, all IL nodes created will have this BytecodeBuilder's _bcIndex */
   void SetCurrentIlGenerator();

   /* The name for this BytecodeBuilder. This can be very helpful for debug output */
   char *name() { return _name; }

   /**
    * @brief bytecode length for this builder object
    */
   int32_t bcLength() { return _bcLength; }

   virtual uint32_t countBlocks();

   void AddFallThroughBuilder(TR::BytecodeBuilder *ftb);

   void AddSuccessorBuilders(uint32_t numBuilders, ...);
   void AddSuccessorBuilder(TR::BytecodeBuilder **b) { AddSuccessorBuilders(1, b); }

   virtual TR::VirtualMachineState *initialVMState()        { return _initialVMState; }
   virtual TR::VirtualMachineState *vmState()               { return _vmState; }
   void setVMState(TR::VirtualMachineState *vmState)        { _vmState = vmState; }

   void propagateVMState(TR::VirtualMachineState *fromVMState);

   // The following control flow services are meant to hide the similarly named services
   // provided by the IlBuilder class. The reason these implementations exist is to
   // automatically manage the propagation of virtual machine states between bytecode
   // builders. By using these services, and AddFallthroughBuilder(), users do not have
   // to do anything to propagate VM states; it's all just taken care of under the covers.
   void Goto(TR::BytecodeBuilder **dest);
   void Goto(TR::BytecodeBuilder *dest);
   void IfCmpEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c);
   void IfCmpEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c);
   void IfCmpNotEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpNotEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpNotEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c);
   void IfCmpNotEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c);
   void IfCmpLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);

   /**
    * @brief returns the client object associated with this object, allocating it if necessary
    */
   void *client();

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
   TR::BytecodeBuilder       * _fallThroughBuilder;
   List<TR::BytecodeBuilder> * _successorBuilders;
   int32_t                     _bcIndex;
   char                      * _name;
   int32_t                     _bcLength;
   TR::VirtualMachineState   * _initialVMState;
   TR::VirtualMachineState   * _vmState;

   virtual void appendBlock(TR::Block *block = 0, bool addEdge=true);
   void addAllSuccessorBuildersToWorklist();
   bool connectTrees();
   virtual void setHandlerInfo(uint32_t catchType);
   void transferVMState(TR::BytecodeBuilder **b);

private:
   static ClientAllocator      _clientAllocator;
   static ImplGetter _getImpl;
   };

} // namespace OMR

#endif // !defined(OMR_BYTECODEBUILDER_INCL)
