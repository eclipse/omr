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

#ifndef OMR_BYTECODEBUILDER_INCL
#define OMR_BYTECODEBUILDER_INCL

#ifndef TR_BYTECODEBUILDER_DEFINED
#define TR_BYTECODEBUILDER_DEFINED
#define PUT_OMR_BYTECODEBUILDER_INTO_TR
#endif // !defined(TR_BYTECODEBUILDER_DEFINED)

#include "ilgen/IlBuilder.hpp"

namespace OMR { class BytecodeBuilderImpl; }
namespace OMR { class MethodBuilder; }
namespace OMR { class MethodBuilderImpl; }
namespace OMR { class VirtualMachineState; }

namespace TR { class BytecodeBuilder; }
namespace TR { class BytecodeBuilderImpl; }
namespace TR { class MethodBuilder; }

namespace OMR
{

/*
 * BytecodeBuilder Client API
 * TODO: put class documentation here
 * Constructors/Destructors at the top, all other API in alphabetical order
 * All implementations are simply delegations to the BytecodeBuilderImpl object
 * stored in _impl (inherited from IlBuilder) . There is a 1-1 relationship
 * between BytecodeBuilder and BytecodeBuilderImpl objects. When the BytecodeBuilder
 * object is destroyed, the associated BytecodeBuilderImpl object will also be
 * destroyed.
 */
class BytecodeBuilder : public TR::IlBuilder
   {
   friend class OMR::MethodBuilder;
   friend class OMR::MethodBuilderImpl;
   friend class OMR::BytecodeBuilderImpl;

protected:
   BytecodeBuilder(TR::BytecodeBuilderImpl *impl, TR::MethodBuilder *mb, TR::TypeDictionary *types);

public:

   virtual ~BytecodeBuilder()
      { }

   /**
    * @brief bytecode index for this builder object
    */
   int32_t bcIndex();  // TODO should capitalize for consistency

   /**
    * @brief returns the initial VM state for this object
    */
   VirtualMachineState *initialVMState();

   /**
    * @brief bytecode name for this builder object
    */
   char *name();       // TODO should capitalize for consistency

   /**
    * @brief returns the current VM state for this object
    */
   VirtualMachineState *vmState();

   void AddFallThroughBuilder(TR::BytecodeBuilder *ftb);
   void AddSuccessorBuilders(uint32_t numBuilders, ...);
   void AddSuccessorBuilder(TR::BytecodeBuilder **b) { AddSuccessorBuilders(1, b); }

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
   void IfCmpGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpNotEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpNotEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpNotEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c);
   void IfCmpNotEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c);
   void IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2);
   void IfCmpUnsignedGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2);

protected:
   TR::BytecodeBuilderImpl *impl();

   };

} // namespace OMR


#if defined(PUT_OMR_BYTECODEBUILDER_INTO_TR)

namespace TR
{
   class BytecodeBuilder : public OMR::BytecodeBuilder
      {
      friend class OMR::MethodBuilder;
      protected:
         BytecodeBuilder(TR::BytecodeBuilderImpl *impl, TR::MethodBuilder *mb, TR::TypeDictionary *types)
            : OMR::BytecodeBuilder(impl, mb, types)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_BYTECODEBUILDER_INTO_TR)

#endif // !defined(OMR_BYTECODEBUILDER_INCL)
