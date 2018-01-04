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

#include <stddef.h>                            // for NULL
#include "codegen/FrontEnd.hpp"                // for feGetEnv
#include "compile/Compilation.hpp"             // for Compilation
#include "env/jittypes.h"                      // for TR_ByteCodeInfo, etc
#include "il/Block.hpp"                        // for Block
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::Goto, etc
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Cfg.hpp"                       // for CFG
#include "infra/List.hpp"                      // for List, ListIterator, etc
#include "infra/CfgEdge.hpp"                   // for CFGEdge
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/TransformUtil.hpp"         // for TransformUtil
#include "optimizer/VirtualGuardHeadMerger.hpp"
#include "ras/DebugCounter.hpp"
#include "infra/Checklist.hpp"

#define OPT_DETAILS "O^O VG HEAD MERGE: "

static bool isMergeableGuard(TR::Node *node)
   {
   static char *mergeOnlyHCRGuards = feGetEnv("TR_MergeOnlyHCRGuards");
   return mergeOnlyHCRGuards ? node->isHCRGuard() : node->isNopableInlineGuard();
   }

static bool isStopTheWorldGuard(TR::Node *node)
   {
   return node->isHCRGuard() || node->isOSRGuard();
   }

/**
 * Recursive call to anchor nodes with a reference count greater than one prior to a treetop.
 *
 * @param comp Compilation object
 * @param node Node to potentially anchor
 * @param tt Treetop to anchor nodes before
 * @param checklist Checklist of visited nodes
 * @return Whether or not a nodes was anchored.
 */
static bool anchorCommonNodes(TR::Compilation *comp, TR::Node* node, TR::TreeTop* tt, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return false;
   checklist.add(node);

   bool anchored = false;
   if (node->getReferenceCount() > 1)
      {
      tt->insertBefore(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, node)));
      anchored = true;
      }
   else
      {
      for (int i = 0; i <node->getNumChildren(); i++)
         anchored |= anchorCommonNodes(comp, node->getChild(i), tt, checklist);
      }

   return anchored;
   }

/**
 * A runtime guard block may have monitor stores and privarg stores along with the guard
 * it self. This method will rearrange these stores and split the block, managing any
 * uncommoning necessary for eventual block order.
 *
 * The provided block will become the privarg block, containing any privarg stores and additonal
 * temps for uncommoning. It must be evaluated first. The returned block will contain monitor
 * stores and the guard. If no split is required, the provided block will be returned.
 *
 * @param comp Compilation object
 * @param block Block to manipulate
 * @param cfg Current CFG
 * @return The block containing the guard.
 */
static TR::Block* splitRuntimeGuardBlock(TR::Compilation *comp, TR::Block* block, TR::CFG *cfg)
   {
   TR::NodeChecklist checklist(comp);
   TR::TreeTop *start = block->getFirstRealTreeTop();
   TR::TreeTop *guard = block->getLastRealTreeTop();
   TR::TreeTop *firstPrivArg = NULL;
   TR::TreeTop *firstMonitor = NULL;

   // Manage the unexpected case that monitors and priv args are reversed
   bool privThenMonitor = false;

   TR_ASSERT(isMergeableGuard(guard->getNode()), "last node must be guard %p", guard->getNode());

   // Search for privarg and monitor stores
   // Only commoned nodes under the guard are required to be anchored, due to the guard being
   // evaluted before the monitor stores later on
   bool anchoredTemps = false;
   for (TR::TreeTop *tt = start; tt && tt->getNode()->getOpCodeValue() != TR::BBEnd; tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();

      if (node->getOpCode().hasSymbolReference() && node->getSymbol()->holdsMonitoredObject())
         firstMonitor = firstMonitor == NULL ? tt : firstMonitor;
      else if (node->chkIsPrivatizedInlinerArg())
         {
         if (firstPrivArg == NULL)
            {
            firstPrivArg = tt;
            privThenMonitor = (firstMonitor == NULL);
            }
         }
      else if (isMergeableGuard(node))
         anchoredTemps |= anchorCommonNodes(comp, node, start, checklist);
      else
         TR_ASSERT(0, "Node other than monitor or privarg store %p before runtime guard", node);
      }

   // If there are monitors then privargs, they must be swapped around, such that all privargs are
   // evaluated first
   if (firstPrivArg && firstMonitor && !privThenMonitor)
      {
      TR::TreeTop *monitorEnd = firstPrivArg->getPrevTreeTop();
      firstMonitor->getPrevTreeTop()->join(firstPrivArg);
      guard->getPrevTreeTop()->join(firstMonitor);
      monitorEnd->join(guard);
      }

   // If there were temps created or privargs in the block, perform a split
   TR::TreeTop *split = NULL;
   if (firstPrivArg)
      split = firstMonitor ? firstMonitor : guard;
   else if (anchoredTemps)
      split = start;

   if (split)
      return block->split(split, cfg, true /* fixupCommoning */, false /* copyExceptionSuccessors */);
   return block;
   }

/**
 * Collect direct loads in a node and its children, adding them to the provided BitVector.
 *
 * @param node The node to consider.
 * @param loadSymRefs BitVector of symbol reference numbers seen.
 * @param checklist Checklist of visited nodes.
 */
static void collectDirectLoads(TR::Node *node, TR_BitVector &loadSymRefs, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return;
   checklist.add(node);

   if (node->getOpCode().isLoadVarDirect())
      loadSymRefs.set(node->getSymbolReference()->getReferenceNumber());

   for (int i = 0; i < node->getNumChildren(); i++)
      collectDirectLoads(node->getChild(i), loadSymRefs, checklist);
   }

/**
 * Search for direct loads in the taken side of a guard
 *
 * @param firstBlock The guard's branch destination
 * @param coldPathLoads BitVector of symbol reference numbers for any direct loads seen until the merge back to mainline
 */
static void collectColdPathLoads(TR::Block* firstBlock, TR_BitVector &coldPathLoads)
   {
   TR_Stack<TR::Block*> blocksToCheck(TR::comp()->trMemory(), 8, false, stackAlloc);
   blocksToCheck.push(firstBlock);
   TR::NodeChecklist checklist(TR::comp());

   coldPathLoads.empty();
   while (!blocksToCheck.isEmpty())
      {
      TR::Block *block = blocksToCheck.pop();

      for (TR::TreeTop *tt = block->getFirstRealTreeTop(); tt->getNode()->getOpCodeValue() != TR::BBEnd; tt = tt->getNextTreeTop())
         collectDirectLoads(tt->getNode(), coldPathLoads, checklist);

      // Search for any successors that have not merged with the mainline
      for (auto itr = block->getSuccessors().begin(), end = block->getSuccessors().end(); itr != end; ++itr)
         {
         TR::Block *dest = (*itr)->getTo()->asBlock();
         if (dest != TR::comp()->getFlowGraph()->getEnd() && dest->getPredecessors().size() == 1)
            blocksToCheck.push(dest);
         }
      }
   }

static bool safeToMoveGuard(TR::Block *destination, TR::TreeTop *guardCandidate,
   TR::TreeTop *branchDest, TR_BitVector &privArgSymRefs)
   {
   static char *disablePrivArgMovement = feGetEnv("TR_DisableRuntimeGuardPrivArgMovement");
   TR::TreeTop *start = destination ? destination->getExit() : TR::comp()->getStartTree();
   if (guardCandidate->getNode()->isHCRGuard())
      {
      for (TR::TreeTop *tt = start; tt && tt != guardCandidate; tt = tt->getNextTreeTop())
         {
         if (tt->getNode()->canGCandReturn())
            return false;
         }
      }
   else if (guardCandidate->getNode()->isOSRGuard())
      {
      for (TR::TreeTop *tt = start; tt && tt != guardCandidate; tt = tt->getNextTreeTop())
         {
         if (TR::comp()->isPotentialOSRPoint(tt->getNode(), NULL, true))
            return false;
         }
      }
   else
      {
      privArgSymRefs.empty();
      for (TR::TreeTop *tt = start; tt && tt != guardCandidate; tt = tt->getNextTreeTop())
         {
         // It's safe to move the guard if there are only priv arg stores and live monitor stores
         // ahead of the guard
         if (tt->getNode()->getOpCodeValue() != TR::BBStart
             && tt->getNode()->getOpCodeValue() != TR::BBEnd
             && !tt->getNode()->chkIsPrivatizedInlinerArg()
             && !(tt->getNode()->getOpCode().hasSymbolReference() && tt->getNode()->getSymbol()->holdsMonitoredObject())
             && !tt->getNode()->isNopableInlineGuard())
                return false;

         if (tt->getNode()->chkIsPrivatizedInlinerArg() && (disablePrivArgMovement ||
             // If the priv arg is not for this guard
             (guardCandidate->getNode()->getInlinedSiteIndex() > -1 &&
             // if priv arg store does not have the same inlined site index as the guard's caller, that means it is not a priv arg for this guard,
             // then we cannot move the guard and its priv args up across other calls' priv args
             tt->getNode()->getInlinedSiteIndex() != TR::comp()->getInlinedCallSite(guardCandidate->getNode()->getInlinedSiteIndex())._byteCodeInfo.getCallerIndex())))
            return false;

         if (tt->getNode()->chkIsPrivatizedInlinerArg())
            privArgSymRefs.set(tt->getNode()->getSymbolReference()->getReferenceNumber());

         if (tt->getNode()->isNopableInlineGuard()
             && tt->getNode()->getBranchDestination() != branchDest)
            return false;
         }
      }
   return true;
   }

static void moveBlockAfterDest(TR::CFG *cfg, TR::Block *toMove, TR::Block *dest)
   {
   TR::Compilation *comp = TR::comp();
   // Step1 splice out toMove
   TR::Block *toMovePrev = toMove->getPrevBlock();
   TR::Block *toMoveSucc = toMove->getNextBlock();

   toMovePrev->getExit()->join(toMoveSucc->getEntry());

   // Step 2 splice toMove in after dest
   TR::Block *destNext = dest->getNextBlock();
   dest->getExit()->join(toMove->getEntry());
   toMove->getExit()->join(destNext->getEntry());

   cfg->addEdge(toMove, destNext);
   cfg->addEdge(dest, toMove);
   cfg->removeEdge(dest, destNext);

   cfg->addEdge(toMovePrev, toMoveSucc);
   cfg->removeEdge(toMovePrev, toMove);
   cfg->removeEdge(toMove, toMoveSucc);
   }

// This opt tries to reduce merge backs from cold code that are the result of inliner
// gnerated nopable virtual guards
// It looks for one basic pattern
//
// guard1 -> cold1
// BBEND
// BBSTART
// guard2 -> cold2
// if guard1 is the guard for a method which calls the method guard2 protects or cold1 is
// a predecessor of cold2 (a situation commonly greated by virtual guard tail splitter) we
// can transform the guards as follows when guard1 and guard2 a
// guard1 -> cold1
// BBEND
// BBSTART
// guard2 -> cold1
// This is safe because there are no trees between the guards and calling the caller will
// result in the call to the callee if we need to patch guard2. cold2 and its mergebacks
// can then be eliminated
//
// In addition this opt will try to move guard2 up from the end of a block to the
// start of the block. We can do this if guard2 is an HCR guard and there is no GC point
// between BBSTART and guard2 since HCR is a stop-the-world event.
//
// Finally, there is a simple tail splitting step run before the analysis of a guard if we
// detect that the taken side of the guard merges back in the next block - this happens
// for some empty methods and is common for Object.<init> at the top of constructors.
int32_t TR_VirtualGuardHeadMerger::perform() {
   static char *disableVGHeadMergerTailSplitting = feGetEnv("TR_DisableVGHeadMergerTailSplitting");
   TR::CFG *cfg = comp()->getFlowGraph();

   // Cache the loads for the outer guard's cold path
   TR_BitVector coldPathLoads(comp()->trMemory()->currentStackRegion());
   TR_BitVector privArgSymRefs(comp()->trMemory()->currentStackRegion());
   bool evaluatedColdPathLoads = false;

   for (TR::Block *block = optimizer()->getMethodSymbol()->getFirstTreeTop()->getNode()->getBlock();
        block; block = block->getNextBlock())
      {
      TR::Node *guard1 = block->getLastRealTreeTop()->getNode();

      if (isMergeableGuard(guard1))
         {
         if (trace())
            traceMsg(comp(), "Found mergeable guard in block_%d\n", block->getNumber());
         TR::Block *cold1 = guard1->getBranchDestination()->getEnclosingBlock();

         // check for an immediate merge back from the cold block and
         // tail split one block if we can - we only handle splitting a block
         // ending in a fallthrough, a branch or a goto for now for simplicity
         if (!disableVGHeadMergerTailSplitting &&
             (cold1->getSuccessors().size() == 1) &&
             cold1->hasSuccessor(block->getNextBlock()) &&
             cold1->getLastRealTreeTop()->getNode()->getOpCode().isGoto())
            {
            // TODO handle moving code earlier in the block down below the guard
            // tail split
            if ((block->getNextBlock()->getSuccessors().size() == 1) ||
                ((block->getNextBlock()->getSuccessors().size() == 2) &&
                 block->getNextBlock()->getLastRealTreeTop()->getNode()->getOpCode().isBranch()) &&
                performTransformation(comp(), "%sCloning block_%d and placing clone after block_%d to reduce HCR guard nops\n", OPT_DETAILS, block->getNextBlock()->getNumber(), cold1->getNumber()))
               tailSplitBlock(block, cold1);
            }

         // guard motion is fairly complex but what we want to achieve around guard1 is a sequence
         // of relocated privarg blocks, followed by a sequence of runtime patchable guards going to
         // guard1's cold block, followed by a sequence of stop-the-world guards going to guard1's
         // cold block
         //
         // The following code is to setup the various insert points based on the following diagrams
         // of basic blocks:
         //
         // start:               setup:                          end result after moving runtime guard'
         //                       |       |                        +-------+ <-- privargIns
         //                       |       | <-- privargIns             |
         //                       +-------+ <-- runtimeIns         +-------+
         //   |       |               |                            | Guard'|
         //   |       |               V                            +-------+ <-- runtimeIns
         //   +-------+           +-------+                            |
         //   | Guard |           | Guard |                            V
         //   +-------+           +-------+ <-- HCRIns             +-------+
         //       |        ===>       |                    ===>    | Guard |
         //       V                   V                            +-------+ <-- HCRIns
         //   +-------+           +-------+                            |
         //   |       |           |       |                            V
         //   |       |           |       |                        +-------+
         //
         // Note we always split the block - this may create an empty block but preserves the incoming
         // control flow we leave the rest to block extension to fix later

         block = block->split(block->getLastRealTreeTop(), cfg, true, false);
         TR::Block *privargIns = block->getPrevBlock();
         TR::Block *runtimeIns = block->getPrevBlock();
         TR::Block *HCRIns = block;

         // New outer guard so cold paths must be evaluated
         evaluatedColdPathLoads = false;

         // scan for candidate guards to merge with guard1 identified above
         for (TR::Block *nextBlock = block->getNextBlock(); nextBlock; nextBlock = nextBlock->getNextBlock())
            {
            if (!(nextBlock->getPredecessors().size() == 1) ||
                !nextBlock->hasPredecessor(block))
               {
               break;
               }

            TR::TreeTop *guard2Tree = NULL;
            if (isMergeableGuard(nextBlock->getFirstRealTreeTop()->getNode()))
               {
               guard2Tree = nextBlock->getFirstRealTreeTop();
               }
            else if (isMergeableGuard(nextBlock->getLastRealTreeTop()->getNode()))
               {
               guard2Tree = nextBlock->getLastRealTreeTop();
               }
            else
               break;

            TR::Node *guard2 = guard2Tree->getNode();
            TR::Block *guard2Block = nextBlock;

            // It is not possible to shift an OSR guard unless the destination is already an OSR point
            // as the necessary OSR state will not be available
            if (guard2->isOSRGuard() && !guard1->isOSRGuard())
               break;

            TR::Block *insertPoint = isStopTheWorldGuard(guard2) ? HCRIns : runtimeIns;
            if (!safeToMoveGuard(insertPoint, guard2Tree, guard1->getBranchDestination(), privArgSymRefs))
               break;

            // now we figure out if we can redirect guard2 to guard1's cold block
            // ie can we do the head merge
            TR::Block *cold2 = guard2->getBranchDestination()->getEnclosingBlock();
            if (guard1->getInlinedSiteIndex() == guard2->getInlinedSiteIndex())
               {
               if (trace())
                  traceMsg(comp(), "  Guard1 [%p] is guarding the same call as Guard2 [%p] - proceeding with guard merging\n", guard1, guard2);
               }
            else if (guard2->getInlinedSiteIndex() > -1 &&
                guard1->getInlinedSiteIndex() == TR::comp()->getInlinedCallSite(guard2->getInlinedSiteIndex())._byteCodeInfo.getCallerIndex())
               {
               if (trace())
                  traceMsg(comp(), "  Guard1 [%p] is the caller of Guard2 [%p] - proceeding with guard merging\n", guard1, guard2);
               }
            else if ((cold1->getSuccessors().size() == 1) &&
                     cold1->hasSuccessor(cold2))
               {
               if (trace())
                  traceMsg(comp(), "  Guard1 cold destination block_%d has guard2 cold destination block_%d as its only successor - proceeding with guard merging\n", cold1->getNumber(), cold2->getNumber());
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "  Cold1 block_%d and cold2 block_%d of guard2 [%p] in unknown relationship - abandon the merge attempt\n", cold1->getNumber(), cold2->getNumber(), guard2);
               break;
               }

            // Runtime guards will shift their privargs, so it is necessary to check such a move is safe
            // This is possible if a privarg temp was recycled for the inner call site, with a prior use as an
            // argument for the outer call site. As the privargs for the inner call site must be evaluated before
            // both guards, this would result in the recycled temp holding the incorrect value if the guard is ever
            // taken.
            if (!isStopTheWorldGuard(guard2))
               {
               if (!evaluatedColdPathLoads)
                  {
                  collectColdPathLoads(cold1, coldPathLoads);
                  evaluatedColdPathLoads = true;
                  }

               if (coldPathLoads.intersects(privArgSymRefs))
                  {
                  if (trace())
                     traceMsg(comp(), "  Recycled temp live in cold1 block_%d and used as privarg before guard2 [%p] - stop guard merging", cold1->getNumber(), guard2);
                  break;
                  }
               }

            if (!performTransformation(comp(), "%sRedirecting %s guard [%p] in block_%d to parent guard cold block_%d\n", OPT_DETAILS, isStopTheWorldGuard(guard2) ? "stop the world" : "runtime", guard2, guard2Block->getNumber(), cold1->getNumber()))
                  continue;

            if (guard2->getBranchDestination() != guard1->getBranchDestination())
               guard2Block->changeBranchDestination(guard1->getBranchDestination(), cfg);

            if (guard2Tree != guard2Block->getFirstRealTreeTop())
               {
               cfg->setStructure(NULL);

               // We should leave code ahead of an HCR guard in place because:
               // 1, it might have side effect to runtime guards after it, moving it up might cause us to falsely merge
               //    the subsequent runtime guards
               // 2, it might contain live monitor, moving it up above a guard can affect the monitor's live range
               if (!isStopTheWorldGuard(guard2))
                  {
	          // the block created above guard2 contains only privarg treetops or monitor stores if
                  // guard2 is a runtime-patchable guard and is safe to merge. We need to move the priv
                  // args up to the runtime insert point and leave the monitor stores in place
                  // It's safe to do so because there is no data dependency between the monitor store and
                  // the priv arg store, because the priv arg store does not load the value from the temp
                  // holding the monitored object

                  // Split priv arg stores from monitor stores
                  // Monitor store is generated for the caller of the method guard2 protects, so should appear before
                  // priv arg stores for the method guard2 protects
                  TR::Block *privargBlock = guard2Block;
                  guard2Block = splitRuntimeGuardBlock(comp(), guard2Block, cfg);
                  if (privargBlock != guard2Block)
                     {
                     if (trace())
                        traceMsg(comp(), "  Moving privarg block_%d after block_%d\n", privargBlock->getNumber(), privargIns->getNumber());

                     moveBlockAfterDest(cfg, privargBlock, privargIns);

                     if (HCRIns == privargIns)
                        HCRIns = privargBlock;
                     if (runtimeIns == privargIns)
                        runtimeIns = privargBlock;
                     privargIns = privargBlock;

                     // refresh the insertPoint since it could be stale after the above updates
                     insertPoint = runtimeIns;
                     }
                  }

               guard2Block = guard2Block->split(guard2Tree, cfg, true, false);
               if (trace())
                  traceMsg(comp(), "  Created new block_%d to hold guard [%p] from block_%d\n", guard2Block->getNumber(), guard2, guard2Block->getNumber());
               }

            if (insertPoint != guard2Block->getPrevBlock())
               {
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "headMerger/%s_%s/(%s)", isStopTheWorldGuard(guard1) ? "stop the world" : "runtime", isStopTheWorldGuard(guard2) ? "stop the world" : "runtime", comp()->signature()));
               cfg->setStructure(NULL);

               block = nextBlock = guard2Block->getPrevBlock();
               if (trace())
                  traceMsg(comp(), "  Moving guard2 block block_%d after block_%d\n", guard2Block->getNumber(), insertPoint->getNumber());

               moveBlockAfterDest(cfg, guard2Block, insertPoint);

               if (HCRIns == insertPoint)
                  HCRIns = guard2Block;
               if (runtimeIns == insertPoint)
                  runtimeIns = guard2Block;
               }
            else
               {
               block = guard2Block;
               }
            guard1 = guard2;
            }
         }
      }
   return 1;
}

// This function splits a single succeesor block following an guard and is used to
// do the following transform
//    block - cold1         block - cold1
//      \     /        =>     |       |
//     nextBlock           nextBlock nextBlock' (called tailSplitBlock below)
//         |                  \      /
//        ...                   ...
void TR_VirtualGuardHeadMerger::tailSplitBlock(TR::Block * block, TR::Block * cold1)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   cfg->setStructure(NULL);
   TR_BlockCloner cloner(cfg);
   TR::Block *tailSplitBlock = cloner.cloneBlocks(block->getNextBlock(), block->getNextBlock());
   tailSplitBlock->setFrequency(cold1->getFrequency());
   if (cold1->isCold())
      tailSplitBlock->setIsCold();

   // physically put the block after cold1 since we want cold1 to fall through
   tailSplitBlock->getExit()->join(cold1->getExit()->getNextTreeTop());
   cold1->getExit()->join(tailSplitBlock->getEntry());

   // remove cold1's goto
   TR::TransformUtil::removeTree(comp(), cold1->getExit()->getPrevRealTreeTop());

   // copy the exception edges
   for (auto e = block->getNextBlock()->getExceptionSuccessors().begin(); e != block->getNextBlock()->getExceptionSuccessors().end(); ++e)
      cfg->addExceptionEdge(tailSplitBlock, (*e)->getTo());

   cfg->addEdge(cold1, tailSplitBlock);
   // lastly fix up the exit of tailSplitBlock
   TR::Node *tailSplitEnd = tailSplitBlock->getExit()->getPrevRealTreeTop()->getNode();
   if (tailSplitEnd->getOpCode().isGoto())
      {
      tailSplitEnd->setBranchDestination(block->getNextBlock()->getLastRealTreeTop()->getNode()->getBranchDestination());
      cfg->addEdge(tailSplitBlock, block->getNextBlock()->getSuccessors().front()->getTo());
      }
   else if (tailSplitEnd->getOpCode().isBranch())
      {
      TR::Block *gotoBlock = TR::Block::createEmptyBlock(tailSplitEnd, comp(), cold1->getFrequency());
      if (cold1->isCold())
          gotoBlock->setIsCold(true);
      gotoBlock->getExit()->join(tailSplitBlock->getExit()->getNextTreeTop());
      tailSplitBlock->getExit()->join(gotoBlock->getEntry());
      cfg->addNode(gotoBlock);

      gotoBlock->append(TR::TreeTop::create(comp(), TR::Node::create(tailSplitEnd, TR::Goto, 0, block->getNextBlock()->getExit()->getNextTreeTop())));
      cfg->addEdge(tailSplitBlock, gotoBlock);
      cfg->addEdge(tailSplitBlock, tailSplitBlock->getLastRealTreeTop()->getNode()->getBranchDestination()->getEnclosingBlock());
      cfg->addEdge(gotoBlock, block->getNextBlock()->getNextBlock());
      }
   else if (
            !tailSplitEnd->getOpCode().isReturn() &&
            !tailSplitEnd->getOpCode().isJumpWithMultipleTargets() &&
             tailSplitEnd->getOpCodeValue() != TR::athrow &&
            !(tailSplitEnd->getNumChildren() >= 1 && tailSplitEnd->getFirstChild()->getOpCodeValue() == TR::athrow)
           )
      {
      tailSplitBlock->append(TR::TreeTop::create(comp(), TR::Node::create(tailSplitEnd, TR::Goto, 0, block->getNextBlock()->getExit()->getNextTreeTop())));
      cfg->addEdge(tailSplitBlock, block->getNextBlock()->getNextBlock());
      }
   else
      {
      for (auto e = block->getNextBlock()->getSuccessors().begin(); e != block->getNextBlock()->getSuccessors().end(); ++e)
         cfg->addEdge(tailSplitBlock, (*e)->getTo());
      }
   cfg->removeEdge(cold1, block->getNextBlock());

   optimizer()->setUseDefInfo(NULL);
   optimizer()->setValueNumberInfo(NULL);
   }

const char *
TR_VirtualGuardHeadMerger::optDetailString() const throw()
   {
   return "O^O VIRTUAL GUARD HEAD MERGER: ";
   }
