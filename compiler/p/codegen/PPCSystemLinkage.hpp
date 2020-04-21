/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_POWER_SYSTEMLINKAGE_INCL
#define OMR_POWER_SYSTEMLINKAGE_INCL

#include "codegen/Linkage.hpp"

#include <stdint.h>
#include "codegen/Register.hpp"

namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
template <class T> class List;

namespace TR {

class PPCSystemLinkage : public TR::Linkage
   {
   protected:

   TR::PPCLinkageProperties _properties;

   public:

   PPCSystemLinkage(TR::CodeGenerator *cg);

   virtual const TR::PPCLinkageProperties& getProperties();

   virtual uint32_t getRightToLeft();
   virtual bool hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   virtual void initPPCRealRegisterLinkage();

   virtual void createPrologue(TR::Instruction *cursor);
   virtual void createPrologue(TR::Instruction *cursor, List<TR::ParameterSymbol> &parm);

   virtual void createEpilogue(TR::Instruction *cursor);

   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies);

   virtual void buildVirtualDispatch(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies,
         uint32_t sizeOfArguments);

   void buildDirectCall(
         TR::Node *callNode,
         TR::SymbolReference *callSymRef,
         TR::RegisterDependencyConditions *dependencies,
         const TR::PPCLinkageProperties &pp,
         int32_t argSize);

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode);

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol> &parmList);
   virtual void mapParameters(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol> &parmList);

   /**
    * @brief Provides the entry point in a method to use when that method is invoked
    *        from a method compiled with the same linkage.
    *
    * @details
    *    When asked on the method currently being compiled, this API will return 0 if
    *    asked before code memory has been allocated.
    *
    *    The compiled method entry point may be the same as the interpreter entry point.
    *
    * @return The entry point for compiled methods to use; 0 if the entry point is unknown
    */
   virtual intptr_t entryPointFromCompiledMethod();

   /**
    * @brief Provides the entry point in a method to use when that method is invoked
    *        from an interpreter using the same linkage.
    *
    * @details
    *    When asked on the method currently being compiled, this API will return 0 if
    *    asked before code memory has been allocated.
    *
    *    The compiled method entry point may be the same as the interpreter entry point.
    *
    * @return The entry point for interpreted methods to use; 0 if the entry point is unknown
    */
   virtual intptr_t entryPointFromInterpretedMethod();

   };

}

#endif
