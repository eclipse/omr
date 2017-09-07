/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef BYTECODE_BUILDER_INCL
#define BYTECODE_BUILDER_INCL

#ifndef TR_BYTECODEBUILDER_DEFINED
#define TR_BYTECODEBUILDER_DEFINED
#define PUT_OMR_BYTECODEBUILDER_INTO_TR
#endif // !defined(TR_BYTECODEBUILDER_DEFINED)

#include "ilgen/IlBuilder.hpp"

namespace TR { class BytecodeBuilder; }
namespace TR { class MethodBuilder; }
namespace OMR { class VirtualMachineState; }

namespace OMR
{

class BytecodeBuilder : public TR::IlBuilder
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   BytecodeBuilder(TR::MethodBuilder *methodBuilder, int32_t bcIndex, char *name=NULL);

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

   virtual uint32_t countBlocks();

   void AddFallThroughBuilder(TR::BytecodeBuilder *ftb);

   void AddSuccessorBuilders(uint32_t numBuilders, ...);
   void AddSuccessorBuilder(TR::BytecodeBuilder **b) { AddSuccessorBuilders(1, b); }

   OMR::VirtualMachineState *initialVMState()                { return _initialVMState; }
   OMR::VirtualMachineState *vmState()                       { return _vmState; }
   void setVMState(OMR::VirtualMachineState *vmState)        { _vmState = vmState; }

   void propagateVMState(OMR::VirtualMachineState *fromVMState);

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

protected:
   TR::BytecodeBuilder       * _fallThroughBuilder;
   List<TR::BytecodeBuilder> * _successorBuilders;
   int32_t                     _bcIndex;
   char                      * _name;
   OMR::VirtualMachineState  * _initialVMState;
   OMR::VirtualMachineState  * _vmState;

   virtual void appendBlock(TR::Block *block = 0, bool addEdge=true);
   void addAllSuccessorBuildersToWorklist();
   bool connectTrees();
   virtual void setHandlerInfo(uint32_t catchType);
   void transferVMState(TR::BytecodeBuilder **b);
   };

} // namespace OMR


#if defined(PUT_OMR_BYTECODEBUILDER_INTO_TR)

namespace TR
{
   class BytecodeBuilder : public OMR::BytecodeBuilder
      {
      public:
         BytecodeBuilder(TR::MethodBuilder *methodBuilder, int32_t bcIndex, char *name=NULL)
            : OMR::BytecodeBuilder(methodBuilder, bcIndex, name)
            { }
         void initialize(TR::IlGeneratorMethodDetails * details,
                           TR::ResolvedMethodSymbol     * methodSymbol,
                           TR::FrontEnd                 * fe,
                           TR::SymbolReferenceTable     * symRefTab); 
      };

} // namespace TR

#endif // defined(PUT_OMR_BYTECODEBUILDER_INTO_TR)

#endif // !defined(OMR_ILBUILDER_INCL)
