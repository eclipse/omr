/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef IA32_SYSTEMLINKAGE_INCL
#define IA32_SYSTEMLINKAGE_INCL

#include "x/codegen/X86SystemLinkage.hpp"

#include <stdint.h>
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "il/DataTypes.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }

namespace TR {

class IA32SystemLinkage : public TR::X86SystemLinkage
   {
   public:

   IA32SystemLinkage(TR::CodeGenerator *cg);
   void setUpStackSizeForCallNode(TR::Node*);
   protected:
   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *deps);
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs);
   int32_t layoutParm(TR::Node*, int32_t&, uint16_t&, uint16_t&, TR::parmLayoutResult&);
   int32_t layoutParm(TR::ParameterSymbol*, int32_t&, uint16_t&, uint16_t&, TR::parmLayoutResult&);
   virtual TR::Register *buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps);
   virtual TR::RealRegister* getSingleWordFrameAllocationRegister() { return machine()->getRealRegister(TR::RealRegister::ecx); }
   private:
   virtual uint32_t getAlignment(TR::DataType);
   };

}

#endif
