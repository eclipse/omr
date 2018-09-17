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

#include "codegen/ARMOutOfLineCodeSection.hpp"
#include "codegen/ARMInstruction.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

TR_ARMOutOfLineCodeSection::TR_ARMOutOfLineCodeSection(TR::Node  *callNode,
                            TR::ILOpCodes      callOp,
                            TR::Register      *targetReg,
                            TR::LabelSymbol    *entryLabel,
                            TR::LabelSymbol    *restartLabel,
                            TR::CodeGenerator *cg) :
                            TR_OutOfLineCodeSection(callNode,callOp,targetReg,entryLabel,restartLabel,cg)
   {
   generateARMOutOfLineCodeSectionDispatch();
   }

void TR_ARMOutOfLineCodeSection::generateARMOutOfLineCodeSectionDispatch()
   {
   // Switch to cold helper instruction stream.
   //
   swapInstructionListsWithCompilation();

   generateLabelInstruction(_cg, ARMOp_label, _callNode, _entryLabel);

   TR::Register *resultReg = NULL;
   if (_callNode->getOpCode().isCallIndirect())
      resultReg = TR::TreeEvaluator::performCall(_callNode, true, _cg);
   else
      resultReg = TR::TreeEvaluator::performCall(_callNode, false, _cg);

   if (_targetReg)
      {
      TR_ASSERT(resultReg, "assertion failure");
      generateTrg1Src1Instruction(_cg, ARMOp_mov, _callNode, _targetReg, resultReg);
      }
   _cg->decReferenceCount(_callNode);

   if (_restartLabel)
      generateLabelInstruction(_cg, ARMOp_b, _callNode, _restartLabel);

   generateLabelInstruction(_cg, ARMOp_label, _callNode, generateLabelSymbol(_cg));

   // Switch from cold helper instruction stream.
   //
   swapInstructionListsWithCompilation();
   }

void TR_ARMOutOfLineCodeSection::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
   {

   if (hasBeenRegisterAssigned())
     return;

   _cg->setInternalControlFlowSafeNestingDepth(_cg->internalControlFlowNestingDepth());

   // Create a dependency list on the first instruction in this stream that captures all current real register associations.
   // This is necessary to get the register assigner back into its original state before the helper stream was processed.
   _cg->incOutOfLineColdPathNestedDepth();

   // This prevents the OOL entry label from resetting all register's startOfranges during RA
   _cg->toggleIsInOOLSection();
   TR::RegisterDependencyConditions  *liveRealRegDeps = _cg->machine()->createCondForLiveAndSpilledGPRs(true, _cg->getSpilledRegisterList());

   if (liveRealRegDeps)
      _firstInstruction->setDependencyConditions(liveRealRegDeps);
   _cg->toggleIsInOOLSection(); // toggle it back because swapInstructionListsWithCompilation() also calls toggle...

   // Register assign the helper dispatch instructions.
   swapInstructionListsWithCompilation();
   _cg->doRegisterAssignment(kindsToBeAssigned);
   swapInstructionListsWithCompilation();

   // If a real reg is assigned at the end of the hot path and free at the beginning, but unused on the cold path
   // it won't show up in the liveRealRegDeps. If a dead virtual occupies that real reg we need to clean it up.
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastFPR; ++i)
      {
      TR::RealRegister *r = _cg->machine()->getRealRegister((TR::RealRegister::RegNum)i);
      TR::Register        *v = r->getAssignedRegister();

      if (v && v != r && v->getFutureUseCount() == 0)
         {
         v->setAssignedRegister(NULL);
         r->setState(TR::RealRegister::Unlatched);
         }
      }

   _cg->decOutOfLineColdPathNestedDepth();

   // Returning to mainline, reset this counter
   _cg->setInternalControlFlowSafeNestingDepth(0);

   // Link in the helper stream into the mainline code.
   // We will end up with the OOL items attached at the bottom of the instruction stream
   TR::Instruction *appendInstruction = _cg->getAppendInstruction();
   appendInstruction->setNext(_firstInstruction);
   _firstInstruction->setPrev(appendInstruction);
   _cg->setAppendInstruction(_appendInstruction);

   setHasBeenRegisterAssigned(true);
   }
