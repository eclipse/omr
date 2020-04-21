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

#include "codegen/OutOfLineCodeSection.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"

TR_OutOfLineCodeSection::TR_OutOfLineCodeSection(TR::Node          *callNode,
                                                 TR::ILOpCodes      callOp,
                                                 TR::Register      *targetReg,
                                                 TR::LabelSymbol    *entryLabel,
                                                 TR::LabelSymbol    *restartLabel,
                                                 TR::CodeGenerator *cg) :
   _restartLabel(restartLabel),
   _firstInstruction(NULL),
   _appendInstruction(NULL),
   _targetReg(targetReg),
   _entryLabel(entryLabel),
   _hasBeenRegisterAssigned(false),
   _cg(cg)
   {
   _entryLabel->setStartOfColdInstructionStream();
   _restartLabel->setEndOfColdInstructionStream();
   _block = callNode->getSymbolReference()->canCauseGC() ? cg->getCurrentEvaluationBlock() : 0;

   _callNode = createOutOfLineCallNode(callNode, callOp);
   }

TR_OutOfLineCodeSection::TR_OutOfLineCodeSection(TR::LabelSymbol *entryLabel, TR::CodeGenerator *cg) :
   _entryLabel(entryLabel),
   _cg(cg),
   _firstInstruction(NULL),
   _appendInstruction(NULL),
   _restartLabel(NULL),
   _block(NULL),
   _callNode(NULL),
   _targetReg(NULL),
   _hasBeenRegisterAssigned(false)
   {
   _entryLabel->setStartOfColdInstructionStream();
   }

TR_OutOfLineCodeSection::TR_OutOfLineCodeSection(TR::LabelSymbol *entryLabel, TR::LabelSymbol *restartLabel, TR::CodeGenerator *cg) :
   _entryLabel(entryLabel),
   _cg(cg),
   _firstInstruction(NULL),
   _appendInstruction(NULL),
   _restartLabel(restartLabel),
   _block(NULL),
   _callNode(NULL),
   _targetReg(NULL),
   _hasBeenRegisterAssigned(false)
   {
   _entryLabel->setStartOfColdInstructionStream();
   _restartLabel->setEndOfColdInstructionStream();
   _block = cg->getCurrentEvaluationBlock();
   }

void TR_OutOfLineCodeSection::swapInstructionListsWithCompilation()
   {
   TR::Instruction *temp;

   temp = _cg->getFirstInstruction();
   _cg->setFirstInstruction(_firstInstruction);
   _firstInstruction = temp;

   temp = _cg->getAppendInstruction();
   _cg->setAppendInstruction(_appendInstruction);
   _appendInstruction = temp;
   _cg->toggleIsInOOLSection();
   }

// Evaluate every subtree S of a node N that meets the following criteria:
//
//    (1) the first reference to S is in a subtree of N, and
//    (2) not all references to S appear under N
//
// All subtrees will be evaluated as soon as they are discovered.
//
void TR_OutOfLineCodeSection::preEvaluatePersistentHelperArguments()
   {
   TR_ASSERT(_callNode, "preEvaluatePersistentHelperArguments can only be called for TR_OutOfLineCodeSection which are helper calls");
   TR::TreeEvaluator::initializeStrictlyFutureUseCounts(_callNode, _cg->comp()->incVisitCount(), _cg);
   evaluateNodesWithFutureUses(_callNode);
   }

void TR_OutOfLineCodeSection::evaluateNodesWithFutureUses(TR::Node *node)
   {
   if (node->getRegister() != NULL)
      {
      // Node has already been evaluated outside this tree.
      //
      return;
      }

   if (node->getFutureUseCount() > 0)
      {
      (void)_cg->evaluate(node);

      // Do not decrement the reference count here.  It will be decremented when the call node is evaluated
      // again in the helper instruction stream.
      //

      return;
      }

   for (int32_t i = 0; i<node->getNumChildren(); i++)
      evaluateNodesWithFutureUses(node->getChild(i));
   }

TR::Node *TR_OutOfLineCodeSection::createOutOfLineCallNode(TR::Node *callNode, TR::ILOpCodes callOp)
   {
   int32_t   i;
   vcount_t  visitCount = _cg->comp()->incVisitCount();
   TR::Node  *child;

   for (i=0; i<callNode->getNumChildren(); i++)
      {
      child = callNode->getChild(i);
      TR::TreeEvaluator::initializeStrictlyFutureUseCounts(child, visitCount, _cg);
      }

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
         if ((child->getOpCodeValue() == TR::loadaddr)                                                       &&
             (callNode->getOpCodeValue() == TR::instanceof || callNode->getOpCodeValue() == TR::checkcast || callNode->getOpCodeValue() == TR::checkcastAndNULLCHK)    &&
             (child->getSymbolReference()->getSymbol())                                                     &&
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

   return newCallNode;
   }
