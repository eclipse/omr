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

#include "z/codegen/S390OutOfLineCodeSection.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/OutOfLineCodeSection.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "ras/Debug.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"

namespace TR { class LabelSymbol; }

TR_S390OutOfLineCodeSection::TR_S390OutOfLineCodeSection(TR::Node  *callNode,
                                                 TR::ILOpCodes      callOp,
                                                 TR::Register      *targetReg,
                                                 TR::LabelSymbol    *entryLabel,
                                                 TR::LabelSymbol    *restartLabel,
                                                 TR::CodeGenerator *cg) :
                                                 TR_OutOfLineCodeSection(callNode,callOp,targetReg,entryLabel,restartLabel,cg),
                                                   _targetRegMovOpcode(cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR)
   {
   // isPreparedForDirectJNI also checks if the node is a call
   if(callNode->isPreparedForDirectJNI())
      {
      _callNode->setPreparedForDirectJNI();
      }
   }

TR_S390OutOfLineCodeSection::TR_S390OutOfLineCodeSection(TR::Node  *callNode,
                                                 TR::ILOpCodes      callOp,
                                                 TR::Register      *targetReg,
                                                 TR::LabelSymbol    *entryLabel,
                                                 TR::LabelSymbol    *restartLabel,
                                                 TR::InstOpCode::Mnemonic    targetRegMovOpcode,
                                                 TR::CodeGenerator *cg) : TR_OutOfLineCodeSection(callNode,callOp,targetReg,entryLabel,restartLabel,cg),
                                                	_targetRegMovOpcode(targetRegMovOpcode)
   {
   //generateS390OutOfLineCodeSectionDispatch();
   }

void TR_S390OutOfLineCodeSection::generateS390OutOfLineCodeSectionDispatch()
   {
   // Switch to cold helper instruction stream.
   //
   swapInstructionListsWithCompilation();

   TR::Compilation *comp = _cg->comp();
   TR::Register    *vmThreadReg = _cg->getMethodMetaDataRealRegister();
   TR::Instruction  *temp = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _callNode, _entryLabel);

   _cg->incOutOfLineColdPathNestedDepth();
   TR_Debug * debugObj = _cg->getDebug();
   if (debugObj)
     {
     debugObj->addInstructionComment(temp, "Denotes start of OOL sequence");
     }

   TR::Register *resultReg = TR::TreeEvaluator::performCall(_callNode, _callNode->getOpCode().isCallIndirect(), _cg);

   if (_targetReg)
      temp = generateRRInstruction(_cg, _targetRegMovOpcode, _callNode, _targetReg, _callNode->getRegister());

   _cg->decReferenceCount(_callNode);

   temp = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, _callNode, _restartLabel);

   if (debugObj)
     {
     debugObj->addInstructionComment(temp, "Denotes end of OOL: return to mainline");
     }

   _cg->decOutOfLineColdPathNestedDepth();

   // Switch from cold helper instruction stream.
   swapInstructionListsWithCompilation();
   }

/**
 * Create a NoReg dependency for each child of a call that has been evaluated into a register.
 * Ignore children that do not have a register since their live range should not persist outside of
 * the helper call stream.
 */
TR::RegisterDependencyConditions *TR_S390OutOfLineCodeSection::formEvaluatedArgumentDepList()
   {
   int32_t i, c=0;

   for (i=_callNode->getFirstArgumentIndex(); i<_callNode->getNumChildren(); i++)
      {
      TR::Register *reg = _callNode->getChild(i)->getRegister();
      if (reg)
         {
         TR::RegisterPair *regPair = reg->getRegisterPair();
         c += regPair? 2 : 1;
         }
      }

   TR::RegisterDependencyConditions *depConds = NULL;

   if (c)
      {
      TR::Machine *machine = _cg->machine();
      depConds = generateRegisterDependencyConditions(0, c, _cg);
      for (i=_callNode->getFirstArgumentIndex(); i<_callNode->getNumChildren(); i++)
         {
         TR::Register *reg = _callNode->getChild(i)->getRegister();
         if (reg)
            {
            TR::RegisterPair *regPair = reg->getRegisterPair();
            if (regPair)
               {
               depConds->addPostCondition(regPair->getLowOrder(), TR::RealRegister::NoReg);
               depConds->addPostCondition(regPair->getHighOrder(), TR::RealRegister::NoReg);
               }
            else
               {
               depConds->addPostCondition(regPair->getLowOrder(), TR::RealRegister::NoReg);
               }
            }
         }
      }
   return depConds;
   }

/**
 * Evaluate every subtree S of a node N that meets the following criteria:
 *
 *    (1) the first reference to S is in a subtree of N, and
 *    (2) not all references to S appear under N
 *
 * All subtrees will be evaluated as soon as they are discovered.
 */
void TR_S390OutOfLineCodeSection::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
   {
   TR::Compilation *comp = _cg->comp();
   if (hasBeenRegisterAssigned())
      return;

   // nested internal control flow assert:
   _cg->setInternalControlFlowSafeNestingDepth(_cg->internalControlFlowNestingDepth());

   // Create a dependency list on the first instruction in this stream that captures all current real register associations.
   // This is necessary to get the register assigner back into its original state before the helper stream was processed.
   //
   _cg->incOutOfLineColdPathNestedDepth();

   // this is to prevent the OOL entry label from resetting all register's startOfrange during RA
   _cg->toggleIsInOOLSection();
   TR::RegisterDependencyConditions  *liveRealRegDeps = _cg->machine()->createDepCondForLiveGPRs(_cg->getSpilledRegisterList());

   if (liveRealRegDeps)
      _firstInstruction->setDependencyConditions(liveRealRegDeps);
   _cg->toggleIsInOOLSection(); // toggle it back because there is another toggle inside swapInstructionListsWithCompilation()


   // Register assign the helper dispatch instructions.
   swapInstructionListsWithCompilation();
   _cg->doRegisterAssignment(kindsToBeAssigned);

   TR::list<TR::Register*> *firstTimeLiveOOLRegisterList = _cg->getFirstTimeLiveOOLRegisterList();
   TR::list<TR::Register*> *spilledRegisterList = _cg->getSpilledRegisterList();

   for (auto li = firstTimeLiveOOLRegisterList->begin(); li != firstTimeLiveOOLRegisterList->end(); ++li)
      {
      if ((*li)->getBackingStorage())
         {
         (*li)->getBackingStorage()->setMaxSpillDepth(1);
         traceMsg(comp,"Adding virtReg:%s from _firstTimeLiveOOLRegisterList to _spilledRegisterList \n", _cg->getDebug()->getName((*li)));
         spilledRegisterList->push_front((*li));
         }
      }
   _cg->getFirstTimeLiveOOLRegisterList()->clear();

   swapInstructionListsWithCompilation();

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
