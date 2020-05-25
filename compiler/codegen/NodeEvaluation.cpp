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

#include "codegen/CodeGenerator.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Stack.hpp"
#include "ras/Debug.hpp"

#ifdef J9_PROJECT_SPECIFIC
#ifdef TR_TARGET_S390
#include "z/codegen/S390Register.hpp"
#endif
#endif

TR::Register *
OMR::CodeGenerator::evaluate(TR::Node * node)
   {
   TR::Register *reg;

   TR::ILOpCodes opcode = node->getOpCodeValue();

   TR_ASSERT(!self()->comp()->getOption(TR_EnableParanoidRefCountChecks) || node->getOpCode().isTreeTop() || node->getReferenceCount() > 0,
      "OMR::CodeGenerator::evaluate invoked for nontreetop node [%s] with count == 0", node->getName(self()->comp()->getDebug()));

   if (opcode != TR::BBStart && node->getRegister())
      {
      reg = node->getRegister();
      }
   else
      {
      // Evaluation of a TR IL tree can be performed by many functions:
      //
      // 1) evaluate(...)
      // 2) populateMemoryReference(...)
      // 3) populateAddTree(...)
      // ...
      //
      // However all of these functions can be categorized into two classes:
      //
      // A) functions which completely evaluate their subtree.
      // B) functions which partially evaluate their subtree.
      //
      // Because functions of class A and class B can be used interchangably to
      // perform a recursive decent of a TR IL tree, and because A or B functions
      // can perform a destructive evaluation of their subtree, a bug can occur where
      // the results of a partial evalutation are destructively overwritten before
      // being completely evalutated.
      //
      // Ex: the motivating case is the following evaluation pattern:
      //
      // node_A evaluate
      //    node_B populateMemoryReference
      //    node_C evaluate
      //
      // where
      //
      // a) there is a common node between the subtrees of node_B and node_C.
      // b) calling populateMemoryReference on node_B reduces the reference count of one
      //    of the base or index nodes to 1, creating the "opportunity" for a destructive
      //    evaluation.
      //
      // The following chain of events occurs:
      //
      // 1) evaluate is called on node_A. This evaluator can produce instructions of RX form,
      //    and chooses to do so for this node.
      //
      // 2) populateMemoryReference is called on node_B, and evaluates the subtree, returning
      //    a TR_MemoryReference to node_A's evaluator. This memory reference has not been
      //    dereferenced yet, and the base (and optionally index) nodes may have registers
      //    assigned to them.
      //
      // 3) evaluate is called on node_C, which chooses to destructively evaluate the commoned
      //    base node. The memory reference's base register now contains a garbage value.
      //
      // 4) control passes to node_A's evaluator, which emits an RX instruction using node_B's
      //    memory reference and node_C's register.
      //
      // In the past, the fix for this was to switch the order of evaluation: call evaluate
      // on node_C and then call populateMemoryReference on node_B. This fixes this scenario, but
      // the capability of another tree and evaluation pattern to create this bug still exists.
      //
      // As well, more insidious trees exist:
      //
      // ificmpeq
      //    iiload
      //       i2l
      //          x
      //    iiload
      //       ishl
      //          ==> i2l
      //          4
      //
      // The evaluation pattern here could be:
      //
      // evaluate
      //    populateMemoryReference
      //       evaluate
      //    populateMemoryReference
      //       evaluate
      //
      // If the commoned node's reference count is 2 coming into ificmpeq's evaluator, then
      // the second sub-evaluate call could be destructive to the first populateMemoryReference.
      //
      // Even worse, if either subtree could be destructive, then there would be no correct order to
      // perform the function calls:
      //
      // ificmpeq
      //    iiload
      //       ishl
      //          ==> x
      //          7
      //    iiload
      //       ishl
      //          ==> x
      //          4
      //
      // Generally, two conditions must be true for this bug to be possible:
      //
      // 1) the following two classes of recursive decent functions must exist:
      //    A) functions which completely evaluate their subtree.
      //    B) functions which partially evaluate their subtree.
      //
      // 2) destructive evaluation by either class of function must be possible.
      //
      // This code implements changes to eliminate the second condition for this bug by performing
      // the following check and fixup:
      //
      // If in a function which partially evaluates its subtree, note all non-restricted nodes that
      // have a reference count > 1. If any of those node's reference counts reach 1, then artificially
      // inflate those reference counts by 1 for the lifetime of the parent evaluation.
      //
      int32_t topOfNodeStackBeforeEvaluation = _stackOfArtificiallyInflatedNodes.topIndex();

      // Memory references are not like registers, and should not be allowed to escape their evaluator.
      // Explicitly note memory references that are not loaded into registers and automatically call
      // stopUsingMemRefRegister on all memory references that have "escaped".
      //
      // Only the s390 memory references are tracked in this way.
      //
      int32_t topOfMemRefStackBeforeEvaluation = _stackOfMemoryReferencesCreatedDuringEvaluation.topIndex();

      reg = _nodeToInstrEvaluators[opcode](node, self());

      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         {
         traceMsg(self()->comp(), "  evaluated %s", self()->getDebug()->getName(node));
         self()->getDebug()->dumpLiveRegisters();
         traceMsg(self()->comp(), "\n");
         }

      // Pop off and decrement tracked nodes
      //
      while (_stackOfArtificiallyInflatedNodes.topIndex() > topOfNodeStackBeforeEvaluation)
         {
         TR::Node * artificiallyInflatedNode = _stackOfArtificiallyInflatedNodes.pop();

         if (artificiallyInflatedNode->getReferenceCount() == 1)
            {
            // When inflating reference counts, two cases exist:
            //
            // 1) N's ref count reaches 1 in a populate* call, which is then inc'ed to 2.
            //
            // 1a) N is never evaluated, so the ref count never goes down to 1. (node was not commoned in another subtree)
            //
            //     - no tree difference should be seen in this case.
            //
            // 1b) N is evaluated, so the ref count then goes down to 1. (node was commoned in another subtree)
            //
            //     - register shuffling _could_ be seen in this case.
            //     - but a bug might have been avoided: partial and complete evaluation of a commoned node occurred.
            //
            if (self()->comp()->getOption(TR_TraceCG))
               {
               self()->comp()->getDebug()->trace(" _stackOfArtificiallyInflatedNodes.pop(): node %p part of commoned case, might have avoided a bug!\n", artificiallyInflatedNode);
               }
            }

         self()->decReferenceCount(artificiallyInflatedNode);

#ifdef J9_PROJECT_SPECIFIC
#if defined(TR_TARGET_S390)
         if (artificiallyInflatedNode->getOpaquePseudoRegister())
            {
            TR_OpaquePseudoRegister *reg = artificiallyInflatedNode->getOpaquePseudoRegister();
            TR_StorageReference *ref = reg->getStorageReference();
            self()->processUnusedStorageRef(ref);
            }
#endif
#endif

         if (self()->comp()->getOption(TR_TraceCG))
            {
            self()->comp()->getDebug()->trace(" _stackOfArtificiallyInflatedNodes.pop() %p, decReferenceCount(...) called. reg=%s\n", artificiallyInflatedNode,
                                      artificiallyInflatedNode->getRegister()?artificiallyInflatedNode->getRegister()->getRegisterName(self()->comp()):"null");
            }
         }

#if defined(TR_TARGET_S390)
      self()->StopUsingEscapedMemRefsRegisters(topOfMemRefStackBeforeEvaluation);
#endif

      bool checkRefCount = (node->getReferenceCount() <= 1 ||
                              (reg && reg == node->getRegister()));
      // for anchor mode, if node is an indirect store, it can have
      // ref count <= 2
      // but for compressedRefs, the indirect store must be an address
      if (self()->comp()->useAnchors())
         {
         if (((node->getOpCode().isStoreIndirect() &&
               (self()->comp()->useCompressedPointers() && (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address))) ||
                opcode == TR::awrtbari) &&
               node->getReferenceCount() <= 2 &&
               !checkRefCount)
            checkRefCount = true;
         }

      TR_ASSERT(checkRefCount,
                  "evaluate: the node's register wasn't set (node [%s])", node->getName(self()->comp()->getDebug()));
      }

   return reg;
   }


int32_t
OMR::CodeGenerator::whichNodeToEvaluate(
      TR::Node *first,
      TR::Node *second)
   {
   // Use tree depth as an indicator of priority.
   //
   int32_t firstPriority = first->getEvaluationPriority(self());
   int32_t secondPriority = second->getEvaluationPriority(self());
   return (firstPriority >= secondPriority) ? 0 : 1;
   }


int32_t
OMR::CodeGenerator::whichChildToEvaluate(TR::Node * node)
   {
   // Use tree depth as an indicator of priority.
   //
   int32_t nodePriority = 0;
   int32_t bestPriority = INT_MAX;
   int32_t bestChild = 0;

   for (int32_t childCount = 0; childCount < node->getNumChildren(); ++childCount)
      {
      TR::Node *child = node->getChild(childCount);
      int32_t childPriority = child->getEvaluationPriority(self());
      if (childPriority > bestPriority)
         {
         bestPriority = childPriority;
         nodePriority = childPriority + 1;
         bestChild = childCount;
         }
      }

   node->setEvaluationPriority(nodePriority);
   return bestChild;
   }


int32_t
OMR::CodeGenerator::getEvaluationPriority(TR::Node *node)
   {
   // Evaluation priority is the depth of the sub-tree.
   //
   int32_t nodePriority = 0;

   for (int32_t childCount = node->getNumChildren() - 1; childCount >= 0; childCount--)
      {
      TR::Node *child = node->getChild(childCount);
      int32_t childPriority;
      if (child->getRegister() != NULL)
         childPriority = 0;
      else
         childPriority = child->getEvaluationPriority(self());
      if (childPriority >= nodePriority)
         nodePriority = childPriority + 1;
      }

   return nodePriority;
   }


rcount_t
OMR::CodeGenerator::incReferenceCount(TR::Node *node)
   {
   TR::Register *reg = node->getRegister();

#ifdef J9_PROJECT_SPECIFIC
#if defined(TR_TARGET_S390)
   if (reg && reg->getOpaquePseudoRegister())
      {
      TR_OpaquePseudoRegister * pseudoReg = reg->getOpaquePseudoRegister();
      TR_StorageReference * pseudoRegStorageReference = pseudoReg->getStorageReference();

      TR_ASSERT(pseudoRegStorageReference, "the pseudoReg should have a non-null storage reference\n");

      pseudoRegStorageReference->incrementTemporaryReferenceCount();
      }
#endif
#endif

   rcount_t count = node->incReferenceCount();
   return count;
   }

rcount_t
OMR::CodeGenerator::decReferenceCount(TR::Node * node)
   {
   TR::Register *reg = node->getRegister();

   if ((node->getReferenceCount() == 1) &&
       reg && self()->getLiveRegisters(reg->getKind()))
       {
      TR_ASSERT(reg->isLive() ||
                (diagnostic("\n*** Error: Register %s for node "
                             "[%s] died prematurely\n",
                             reg->getRegisterName(self()->comp()),
                             node->getName(self()->comp()->getDebug())),
                 0),
             "Node %s register should be live",self()->getDebug()->getName(node));

      TR_LiveRegisterInfo *liveRegister = reg->getLiveRegisterInfo();
      TR::Register *pair = reg->getRegisterPair();
      if (pair)
         {
         pair->getHighOrder()->getLiveRegisterInfo()->decNodeCount();
         pair->getLowOrder()->getLiveRegisterInfo()->decNodeCount();
         }

      if (liveRegister && liveRegister->decNodeCount() == 0)
         {
         // The register is now dead
         //
         self()->getLiveRegisters(reg->getKind())->registerIsDead(reg);
         }
      }

#ifdef J9_PROJECT_SPECIFIC
#if defined(TR_TARGET_S390)
   if (reg && reg->getOpaquePseudoRegister())
      {
      TR_OpaquePseudoRegister *pseudoReg = reg->getOpaquePseudoRegister();
      TR_StorageReference *storageReference = pseudoReg->getStorageReference();
      TR_ASSERT(storageReference,"the pseudoReg should have a non-null storage reference\n");
      storageReference->decrementTemporaryReferenceCount();
      if (node->getReferenceCount() == 1)
         {
         storageReference->decOwningRegisterCount();
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tdecrement owningRegisterCount %d->%d on ref #%d (%s) for reg %s as %s (%p) refCount == 1 (going to 0)\n",
               storageReference->getOwningRegisterCount()+1,
               storageReference->getOwningRegisterCount(),
               storageReference->getReferenceNumber(),
               self()->getDebug()->getName(storageReference->getSymbol()),
               self()->getDebug()->getName(reg),
               node->getOpCode().getName(),
               node);
         }
      }
   else if (node->getOpCode().hasSymbolReference() && node->getSymbolReference() && node->getSymbolReference()->isTempVariableSizeSymRef())
      {
      TR_ASSERT(false,"tempMemSlots should only be attached to pseudoRegisters and not node %p\n",node);
      }
#endif
#endif

   rcount_t count = node->decReferenceCount();
   return count;
   }

rcount_t
OMR::CodeGenerator::recursivelyDecReferenceCount(TR::Node * node)
   {
   TR_ASSERT(!self()->comp()->getOption(TR_EnableParanoidRefCountChecks) || node->getOpCode().isTreeTop() || node->getReferenceCount() > 0,
      "OMR::CodeGenerator::recursivelyDecReferenceCount invoked for nontreetop node [%s] with count == 0", node->getName(self()->comp()->getDebug()));
   rcount_t count = self()->decReferenceCount(node);
   if (count == 0 && !node->getRegister())
      for (int16_t c = node->getNumChildren() - 1; c >= 0; c--)
         self()->recursivelyDecReferenceCount(node->getChild(c));
   return count;
   }

void
OMR::CodeGenerator::evaluateChildrenWithMultipleRefCount(TR::Node * node)
   {
   for (int i=0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (child->getRegister() == NULL) // not already evaluated
         {
         // Note: we assume things without a symbol reference don't
         // necessarily need to be evaluated here, and can wait
         // until they are actually needed.
         //
         // vft pointers are speical - we need to evaluate the object in all cases
         // but for nopable virtual guards we can wait to load and mask the pointer
         // until we actually need to use it
         //
         if (child->getReferenceCount() > 1 && 
	     (child->getOpCode().hasSymbolReference() ||
	      (child->getOpCodeValue() == TR::l2a && child->getChild(0)->containsCompressionSequence())))
            {
            TR::SymbolReference *vftPointerSymRef = self()->comp()->getSymRefTab()->element(TR::SymbolReferenceTable::vftSymbol);
            if (node->isNopableInlineGuard()
                && self()->getSupportsVirtualGuardNOPing()
                && child->getOpCodeValue() == TR::aloadi
                && child->getChild(0)->getOpCode().hasSymbolReference()
                && child->getChild(0)->getSymbolReference() == vftPointerSymRef
                && child->getChild(0)->getOpCodeValue() == TR::aloadi)
               {
               if (!child->getChild(0)->getChild(0)->getRegister() &&
                   child->getChild(0)->getChild(0)->getReferenceCount() > 1)
                  self()->evaluate(child->getChild(0)->getChild(0));
               else
                  self()->evaluateChildrenWithMultipleRefCount(child->getChild(0)->getChild(0));
               }
            else
               {
               self()->evaluate(child);
               }
            }
         else
            {
            self()->evaluateChildrenWithMultipleRefCount(child);
            }
         }
      }
   }
