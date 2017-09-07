/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef ARMOUTOFLINECODESECTION_INCL
#define ARMOUTOFLINECODESECTION_INCL

#include "codegen/ARMOps.hpp"
#include "codegen/OutOfLineCodeSection.hpp"
#include "env/TRMemory.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
class TR_ARMRegisterDependencyConditions;

class TR_ARMOutOfLineCodeSection : public TR_OutOfLineCodeSection
   {

public:
   TR_ARMOutOfLineCodeSection(TR::LabelSymbol * entryLabel, TR::LabelSymbol * restartLabel,
                               TR::CodeGenerator *cg) : TR_OutOfLineCodeSection(entryLabel, restartLabel, cg)
                              {}

   TR_ARMOutOfLineCodeSection(TR::LabelSymbol * entryLabel,
                               TR::CodeGenerator *cg) : TR_OutOfLineCodeSection(entryLabel, cg)
                              {}
   // For calls
   //
   TR_ARMOutOfLineCodeSection(TR::Node *callNode, TR::ILOpCodes callOp, TR::Register *targetReg, TR::LabelSymbol *entryLabel, TR::LabelSymbol *restartLabel, TR::CodeGenerator *cg);

   TR_ARMOutOfLineCodeSection(TR::Node *callNode, TR::ILOpCodes callOp, TR::Register *targetReg, TR::LabelSymbol *entryLabel, TR::LabelSymbol *restartLabel, TR_ARMOpCodes targetRegMovOpcode, TR::CodeGenerator *cg);

public:
   void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   void generateARMOutOfLineCodeSectionDispatch();
   };
#endif
