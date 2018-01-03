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

#ifndef OMR_BYTECODEBUILDER_IMPL_INCL
#define OMR_BYTECODEBUILDER_IMPL_INCL

#ifndef TR_BYTECODEBUILDER_IMPL_DEFINED
#define TR_BYTECODEBUILDER_IMPL_DEFINED
#define PUT_OMR_BYTECODEBUILDER_IMPL_INTO_TR
#endif // !defined(TR_BYTECODEBUILDER_IMPL_DEFINED)

#include "ilgen/IlBuilderImpl.hpp"

namespace OMR { class VirtualMachineState; }
namespace TR { class BytecodeBuilder; }
namespace TR { class BytecodeBuilderImpl; }
namespace TR { class MethodBuilderImpl; }

namespace OMR
{

class BytecodeBuilderImpl : public TR::IlBuilderImpl
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   BytecodeBuilderImpl(TR::MethodBuilderImpl *methodBuilder, int32_t bcIndex, char *name=NULL);
   virtual ~BytecodeBuilderImpl()
      { }

   void operator delete(void * ptr)
      { }

   /**
    * Support for BytecodeBuilder public API
    */
   void AddFallThroughBuilder(TR::BytecodeBuilderImpl *ftb);
   void AddSuccessorBuilder(TR::BytecodeBuilderImpl **b);

   int32_t bcIndex()
      { return _bcIndex; }

   OMR::VirtualMachineState *initialVMState()
      { return _initialVMState; }

   char *name()
      { return _name; }

   OMR::VirtualMachineState *vmState()
      { return _vmState; }

   /**
    * Public API for other JitBuilder classes
    */

   virtual void initialize(TR::IlGeneratorMethodDetails * details,
                           TR::ResolvedMethodSymbol     * methodSymbol,
                           TR::FrontEnd                 * fe,
                           TR::SymbolReferenceTable     * symRefTab);

   virtual int32_t currentByteCodeIndex()
      { return _bcIndex; }

   virtual uint32_t countBlocks();

   virtual bool isBytecodeBuilderImpl() { return true; } // TODO: eliminate

   void propagateVMState(OMR::VirtualMachineState *fromVMState);

   void setVMState(OMR::VirtualMachineState *vmState)
      { _vmState = vmState; }

   TR::BytecodeBuilder *client();

protected:
   int32_t                         _bcIndex;
   char                          * _name;
   TR::BytecodeBuilderImpl       * _fallThroughBuilder;
   List<TR::BytecodeBuilderImpl> * _successorBuilders;
   OMR::VirtualMachineState      * _initialVMState;
   OMR::VirtualMachineState      * _vmState;

   virtual void appendBlock(TR::Block *block = 0, bool addEdge=true);
   void addAllSuccessorBuildersToWorklist();
   bool connectTrees();
   virtual void setHandlerInfo(uint32_t catchType);
   void transferVMState(TR::BytecodeBuilderImpl **b);
   };

} // namespace OMR


#if defined(PUT_OMR_BYTECODEBUILDER_IMPL_INTO_TR)

namespace TR
{
   class BytecodeBuilderImpl : public OMR::BytecodeBuilderImpl
      {
      public:
         BytecodeBuilderImpl(TR::MethodBuilderImpl *methodBuilder, int32_t bcIndex, char *name=NULL)
            : OMR::BytecodeBuilderImpl(methodBuilder, bcIndex, name)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_BYTECODEBUILDER_IMPL_INTO_TR)

#endif // !defined(OMR_BYTECODEBUILDER_IMPL_INCL)
