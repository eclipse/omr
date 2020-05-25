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

#include "x/codegen/OutlinedInstructions.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/RegisterUsage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "codegen/X86Instruction.hpp"
#include "env/CompilerEnv.hpp"

TR_OutlinedInstructions::TR_OutlinedInstructions(TR::LabelSymbol *entryLabel, TR::CodeGenerator *cg):
   _entryLabel(entryLabel),
   _cg(cg),
   _firstInstruction(NULL),
   _appendInstruction(NULL),
   _restartLabel(NULL),
   _block(cg->getCurrentEvaluationBlock()),
   _callNode(NULL),
   _targetReg(NULL),
   _hasBeenRegisterAssigned(false),
   _postDependencyMergeList(NULL),
   _outlinedPathRegisterUsageList(NULL),
   _mainlinePathRegisterUsageList(NULL),
   _registerAssignerStateAtMerge(NULL)
   {
   _entryLabel->setStartOfColdInstructionStream();
   }

TR_OutlinedInstructions::TR_OutlinedInstructions(
   TR::Node          *callNode,
   TR::ILOpCodes      callOp,
   TR::Register      *targetReg,
   TR::LabelSymbol    *entryLabel,
   TR::LabelSymbol    *restartLabel,
   TR::CodeGenerator *cg) :
      _restartLabel(restartLabel),
      _block(cg->getCurrentEvaluationBlock()),
      _firstInstruction(NULL),
      _appendInstruction(NULL),
      _targetReg(targetReg),
      _entryLabel(entryLabel),
      _hasBeenRegisterAssigned(false),
      _postDependencyMergeList(NULL),
      _outlinedPathRegisterUsageList(NULL),
      _mainlinePathRegisterUsageList(NULL),
      _registerAssignerStateAtMerge(NULL),
      _cg(cg)
   {
   _entryLabel->setStartOfColdInstructionStream();
   _callNode = createOutlinedCallNode(callNode, callOp);
   generateOutlinedInstructionsDispatch();
   }

void TR_OutlinedInstructions::swapInstructionListsWithCompilation()
   {
   TR::Instruction *temp;

   temp = cg()->getFirstInstruction();
   cg()->setFirstInstruction(_firstInstruction);
   _firstInstruction = temp;

   temp = cg()->getAppendInstruction();
   cg()->setAppendInstruction(_appendInstruction);
   _appendInstruction = temp;
   }

void TR_OutlinedInstructions::generateOutlinedInstructionsDispatch()
   {
   // Switch to cold helper instruction stream.
   //
   TR::Instruction *savedFirstInstruction = cg()->getFirstInstruction();
   TR::Instruction *savedAppendInstruction = cg()->getAppendInstruction();
   cg()->setFirstInstruction(NULL);
   cg()->setAppendInstruction(NULL);

   new (_cg->trHeapMemory()) TR::X86LabelInstruction(NULL, LABEL, _entryLabel, _cg);

   TR::Register *resultReg=NULL;
   if (_callNode->getOpCode().isCallIndirect())
      resultReg = TR::TreeEvaluator::performCall(_callNode, true, false, _cg);
   else
      resultReg = TR::TreeEvaluator::performCall(_callNode, false, false, _cg);

   if (_targetReg)
      {
      TR_ASSERT(resultReg, "assertion failure");
      TR_ASSERT(resultReg->getKind() == _targetReg->getKind(), "OutlinedInstructions: targetReg must have same kind as resultReg.");

      TR::RegisterPair *targetRegPair = _targetReg->getRegisterPair();
      TR::RegisterPair *resultRegPair =  resultReg->getRegisterPair();

      TR_ASSERT((targetRegPair == NULL && resultRegPair == NULL) ||
                (targetRegPair != NULL && resultRegPair != NULL),
                "OutlinedInstructions: targetReg and resultReg must be both register pairs or neither register pairs.");
      if (targetRegPair)
         {
         generateRegRegInstruction(MOVRegReg(), _callNode, targetRegPair->getLowOrder(),  resultRegPair->getLowOrder(),  _cg);
         generateRegRegInstruction(MOVRegReg(), _callNode, targetRegPair->getHighOrder(), resultRegPair->getHighOrder(), _cg);
         }
      else
         {
         TR_X86OpCodes mov = BADIA32Op;
         switch (resultReg->getKind())
            {
            case TR_GPR:
               mov = MOVRegReg();
               break;
            case TR_FPR:
            case TR_VRF:
               mov = MOVDQURegReg;
               break;
            default:
               TR_ASSERT(false, "OutlinedInstructions: unsupported result register kind.");
               break;
            }
         generateRegRegInstruction(mov, _callNode, _targetReg, resultReg, _cg);
         }
      }

   _cg->decReferenceCount(_callNode);

   if (_restartLabel)
      generateLabelInstruction(JMP4, _callNode, _restartLabel, _cg);
   else
      {
      // Java-specific.
      // No restart label implies we're not coming back from this call,
      // so it's safe to put data after the call.  In the case of calling a throw
      // helper, there's an ancient busted handshake that expects to find a 4-byte
      // offset here, so we have to comply...
      //
      // When the handshake is removed, we can delete this zero.
      //
      generateImmInstruction(DDImm4, _callNode, 0, _cg);
      }

   // Dummy label to delimit the end of the helper call dispatch sequence (for exception ranges).
   //
   generateLabelInstruction(LABEL, _callNode, TR::LabelSymbol::create(_cg->trHeapMemory(),_cg), _cg);

   // Switch from cold helper instruction stream.
   //
   _firstInstruction = cg()->getFirstInstruction();
   _appendInstruction = cg()->getAppendInstruction();
   cg()->setFirstInstruction(savedFirstInstruction);
   cg()->setAppendInstruction(savedAppendInstruction);
   }


// Create a NoReg dependency for each child of a call that has been evaluated into a register.
// Ignore children that do not have a register since their live range should not persist outside of
// the helper call stream.
//
TR::RegisterDependencyConditions *TR_OutlinedInstructions::formEvaluatedArgumentDepList()
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
               depConds->addPostCondition(regPair->getLowOrder(),  TR::RealRegister::NoReg, _cg);
               depConds->addPostCondition(regPair->getHighOrder(), TR::RealRegister::NoReg, _cg);
               }
            else
               {
               depConds->addPostCondition(reg, TR::RealRegister::NoReg, _cg);
               }
            }
         }

      depConds->stopAddingConditions();
      }

   return depConds;
   }


void TR_OutlinedInstructions::assignRegisters(TR_RegisterKinds kindsToBeAssigned, TR::X86VFPSaveInstruction *vfpSaveInstruction)
   {
   if (hasBeenRegisterAssigned())
      return;

   // nested internal control flow assert:
   _cg->setInternalControlFlowSafeNestingDepth(_cg->internalControlFlowNestingDepth());

   // Create a dependency list on the first instruction in this stream that captures all
   // current real register associations.  This is necessary to get the register assigner
   // back into its original state before the helper stream was processed.
   //
   TR::RegisterDependencyConditions *liveRealRegDeps = _cg->machine()->createDepCondForLiveGPRs();
   _firstInstruction->setDependencyConditions(liveRealRegDeps);

   // TODO:AMD64: Fix excessive register assignment exchanges in outlined instruction dispatch.

   // Ensure correct VFP state at the start of the outlined instruction sequence.
   //
   generateVFPRestoreInstruction(cg()->getAppendInstruction(), vfpSaveInstruction, _cg);
   // Link in the helper stream into the mainline code.
   //
   TR::Instruction *appendInstruction = cg()->getAppendInstruction();
   appendInstruction->setNext(_firstInstruction);
   _firstInstruction->setPrev(appendInstruction);
   cg()->setAppendInstruction(_appendInstruction);

   // Register assign the helper dispatch instructions.
   //
   _cg->doBackwardsRegisterAssignment(kindsToBeAssigned, _appendInstruction, appendInstruction);

   // Returning to mainline, reset this counter
   _cg->setInternalControlFlowSafeNestingDepth(0);

   setHasBeenRegisterAssigned(true);
   }


void TR_OutlinedInstructions::assignRegistersOnOutlinedPath(TR_RegisterKinds kindsToBeAssigned, TR::X86VFPSaveInstruction *vfpSaveInstruction)
   {
   if (hasBeenRegisterAssigned())
      {
      TR_ASSERT(0, "these registers should not have been assigned already");
      return;
      }

   // Register assign the outlined instructions.
   //
   _cg->doBackwardsRegisterAssignment(kindsToBeAssigned, _appendInstruction);

   // Ensure correct VFP state at the start of the outlined instruction sequence.
   //
   generateVFPRestoreInstruction(cg()->getAppendInstruction(), vfpSaveInstruction, _cg);

   // Link in the helper stream into the mainline code.
   //
   TR::Instruction *appendInstruction = cg()->getAppendInstruction();
   appendInstruction->setNext(_firstInstruction);
   _firstInstruction->setPrev(appendInstruction);
   cg()->setAppendInstruction(_appendInstruction);

   setHasBeenRegisterAssigned(true);
   }

OMR::RegisterUsage *TR_OutlinedInstructions::findInRegisterUsageList(TR::list<OMR::RegisterUsage*> *rul, TR::Register *virtReg)
   {
   for(auto iter = rul->begin(); iter != rul->end(); ++iter)
      {
      if ((*iter)->virtReg == virtReg)
         {
         return *iter;
         }
      }

   return NULL;
   }

TR::Node *TR_OutlinedInstructions::createOutlinedCallNode(TR::Node *callNode, TR::ILOpCodes callOp)
   {
   int32_t   i;
   TR::Node  *child;

   //We pass true for getSymbolReference because
   TR::Node *newCallNode = TR::Node::createWithSymRef(callNode, callOp, callNode->getNumChildren(), callNode->getSymbolReference());

   newCallNode->setReferenceCount(1);

   for (i=0; i<callNode->getNumChildren(); i++)
      {
      child = callNode->getChild(i);

      if (child->getRegister() != NULL)
         {
         // Child has already been evaluated outside this tree.
         //
         newCallNode->setAndIncChild(i, child);
         }
      else if (child->getOpCode().isLoadConst())
         {
         // Copy unevaluated constant nodes.
         //
         child = TR::Node::copy(child);
         child->setReferenceCount(1);
         newCallNode->setChild(i, child);
         }
      else
         {
         if ((child->getOpCodeValue() == TR::loadaddr) &&
             (child->getSymbolReference()->getSymbol()) &&
             (child->getSymbolReference()->getSymbol()->getStaticSymbol()))
            {
            child = TR::Node::copy(child);
            child->setReferenceCount(1);
            newCallNode->setChild(i, child);
            }
         else
            {
            // Be very conservative at this point, even though it is possible to make it less so.  For example, this will catch
            // the case of an unevaluated argument not persisting outside of the outlined region even though one of its subtrees will.
            //
            (void)_cg->evaluate(child);

            // Do not decrement the reference count here.  It will be decremented when the call node is evaluated
            // again in the helper instruction stream.
            //
            newCallNode->setAndIncChild(i, child);
            }
         }
      }
   if(callNode->isPreparedForDirectJNI())
      {
         newCallNode->setPreparedForDirectJNI();
      }

   return newCallNode;
   }

TR_OutlinedInstructionsGenerator::TR_OutlinedInstructionsGenerator(TR::LabelSymbol* entryLabel, TR::Node* node, TR::CodeGenerator* cg)
   : _hasEnded(false)
   {
   _oi = new (cg->trHeapMemory()) TR_OutlinedInstructions(entryLabel, cg);
   _oi->setCallNode(node);
   cg->getOutlinedInstructionsList().push_front(_oi);
   _oi->swapInstructionListsWithCompilation();
   generateLabelInstruction(LABEL, node, entryLabel, cg);
   }

void
TR_OutlinedInstructionsGenerator::endOutlinedInstructionSequence()
   {
   generateLabelInstruction(LABEL, _oi->_callNode, generateLabelSymbol(_oi->_cg), _oi->_cg);
   _oi->swapInstructionListsWithCompilation();
   _hasEnded = true;
   }

TR_OutlinedInstructionsGenerator::~TR_OutlinedInstructionsGenerator()
   {
   if (!std::uncaught_exception())
      {
      TR_ASSERT_FATAL(_hasEnded, "TR_OutlinedInstructionsGenerator object has gone out of scope before call to endOutlinedInstructionSequence().");
      }
   }
