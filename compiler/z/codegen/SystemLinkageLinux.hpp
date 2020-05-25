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

#ifndef OMR_Z_SYSTEMLINKAGELINUX_INCL
#define OMR_Z_SYSTEMLINKAGELINUX_INCL

#include <stdint.h>
#include "codegen/Linkage.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/SystemLinkage.hpp"
#include "codegen/snippet/PPA1Snippet.hpp"
#include "codegen/snippet/PPA2Snippet.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/SymbolReference.hpp"

class TR_EntryPoint;
class TR_zOSGlobalCompilationInfo;
namespace TR { class S390ConstantDataSnippet; }
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
template <class T> class List;

namespace TR {

class S390zLinuxSystemLinkage : public TR::SystemLinkage
   {
   public:

   S390zLinuxSystemLinkage(TR::CodeGenerator* cg);

   virtual void createEpilogue(TR::Instruction * cursor);
   virtual void createPrologue(TR::Instruction * cursor);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol>&parmList);

   virtual void generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptr_t targetAddress, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel, TR::Snippet * callDataSnippet, bool isJNIGCPoint);
   virtual TR::Register* callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, intptr_t targetAddress, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel, TR::Snippet * callDataSnippet, bool isJNIGCPoint = true);

   virtual int32_t getRegisterSaveOffset(TR::RealRegister::RegNum);
   virtual void initParamOffset(TR::ResolvedMethodSymbol * method, int32_t stackIndex, List<TR::ParameterSymbol> *parameterList=0);

   private:

   TR::Instruction* fillGPRsInEpilogue(TR::Node* node, TR::Instruction* cursor);
   TR::Instruction* fillFPRsInEpilogue(TR::Node* node, TR::Instruction* cursor);

   TR::Instruction* spillGPRsInPrologue(TR::Node* node, TR::Instruction* cursor);
   TR::Instruction* spillFPRsInPrologue(TR::Node* node, TR::Instruction* cursor);
   };

}

#endif
