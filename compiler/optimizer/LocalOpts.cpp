/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "optimizer/LocalOpts.hpp"

#include <algorithm>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "cs2/bitvectr.h"
#include "cs2/hashtab.h"
#include "cs2/llistof.h"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/HeuristicRegion.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/ILWalk.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/Inliner.hpp"
#include "infra/Stack.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/OrderBlocks.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/ILValidator.hpp"
#include "ras/ILValidationStrategies.hpp"
#include "runtime/Runtime.hpp"
#include "env/VMAccessCriticalSection.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"
#include "runtime/J9ValueProfiler.hpp"
#include "runtime/J9Profiler.hpp"
#include "env/CHTable.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/RuntimeAssumptionTable.hpp"
#include "env/VMJ9.h"
#include "runtime/RuntimeAssumptions.hpp"
#endif

// basic blocks peephole optimization.
// Return "true" if any basic block peephole optimization was done

TR_PeepHoleBasicBlocks::TR_PeepHoleBasicBlocks(TR::OptimizationManager *manager)
   : TR_BlockManipulator(manager)
   {}

int32_t TR_PeepHoleBasicBlocks::perform()
   {
   // The CFG must exist for this optimization
   //
   TR::CFG * cfg = comp()->getFlowGraph();
   if (cfg == NULL || comp()->getOption(TR_DisableBasicBlockPeepHole))
      return 0;

   TR_OrderBlocks orderBlocks(manager());
   cfg->setIgnoreUnreachableBlocks(true);
   int32_t rc = orderBlocks.lookForPeepHoleOpportunities("O^O BLOCK PEEP HOLE: ");
   cfg->setIgnoreUnreachableBlocks(false);
   if(cfg->getHasUnreachableBlocks())
       cfg->removeUnreachableBlocks();
   return rc;
   }

const char *
TR_PeepHoleBasicBlocks::optDetailString() const throw()
   {
   return "O^O BASIC BLOCK PEEPHOLE: ";
   }

// Extend basic blocks.
// Return "true" if any basic block extension was done
TR_ExtendBasicBlocks::TR_ExtendBasicBlocks(TR::OptimizationManager *manager)
   : TR_BlockManipulator(manager)
   {}

int32_t TR_ExtendBasicBlocks::perform()
   {
   // The CFG must exist for this optimization
   //
   static const char *disableFreqCBO = feGetEnv("TR_disableFreqCBO");
   int32_t orderBlocksResult=-5;

   if (comp()->getFlowGraph() == NULL)
      return 0;

   // There are two basic block extension algorithms depending on whether or not
   // block and edge frequencies are known,
   //
   if (true)
      {
      static const char *p = feGetEnv("TR_OlderBlockReordering");
      if (p)
         return orderBlocksWithFrequencyInfo();
      }

   if (!comp()->getOption(TR_DisableNewBlockOrdering))
      {
      TR_OrderBlocks orderBlocks(manager());
      orderBlocks.extendBlocks();
      orderBlocksResult = orderBlocks.perform();
      // comp()->getFlowGraph()->setStructure(NULL); TR_OrderBlocks should reset structure if needed
      return orderBlocksResult;
      }

   orderBlocksResult = orderBlocksWithoutFrequencyInfo();

   if (!disableFreqCBO)
      comp()->getFlowGraph()->setStructure(NULL);

   return orderBlocksResult;
   }


int32_t TR_ExtendBasicBlocks::orderBlocksWithoutFrequencyInfo()
   {
   // TR_Structure *rootStructure = NULL;
   //
   // Set up the block nesting depths
   //
   TR::CFG *cfg = comp()->getFlowGraph();
   TR_Structure *rootStructure = cfg->getStructure();
   if (rootStructure)
      {
      TR::CFGNode *node;
      for (node = cfg->getFirstNode(); node; node = node->getNext())
         {
         TR::Block *block = toBlock(node);
         int32_t nestingDepth = 0;
         if (block->getStructureOf() != NULL)
            block->getStructureOf()->setNestingDepths(&nestingDepth);
         }
      }

   //markColdBlocks();

   TR::TreeTop *endTreeTop = comp()->getMethodSymbol()->getLastTreeTop();

   vcount_t visitCount = comp()->incVisitCount();
   bool blocksWereExtended = false;
   TR::Block * prevBlock = comp()->getStartTree()->getNode()->getBlock();
   //prevBlock->setVisitCount(visitCount);
   TR::Block * block = prevBlock->getNextBlock();
   bool reattemptThisExtension = false;
   for (; block; prevBlock = block, block = block->getNextBlock())
      {
      if (!reattemptThisExtension &&
         (block->getVisitCount() == visitCount))
          continue;

      reattemptThisExtension = false;
      prevBlock->setVisitCount(visitCount);

      if (block->isExtensionOfPreviousBlock())
         continue;

      // Check that the previous block doesn't end with a switch.
      //
      TR::TreeTop * tt = prevBlock->getLastRealTreeTop();
      TR::Node * prevNode = tt->getNode();
      if(prevNode->getOpCodeValue() == TR::treetop)
          prevNode = prevNode->getFirstChild();

      if ( prevNode->getOpCode().isJumpWithMultipleTargets() || !prevBlock->hasSuccessor(block))
         continue;

      TR::CFGEdgeList & preds = block->getPredecessors();

      bool cannotExtendWithCurrentFallThrough = false;
      if (!(preds.size() == 1))
        cannotExtendWithCurrentFallThrough = true;
      TR::Block *bestExtension = NULL;

      if (trace())
         traceMsg(comp(), "    Current block [%d] looking for best successor to extend it...\n", prevBlock->getNumber());

      //int32_t blockHotness = block->getHotness(cfg);
      int32_t blockFreq = block->getFrequency();
      if (block->isCold() || (blockFreq >= 0))
         {
         bestExtension = getBestChoiceForExtension(prevBlock);

         //
         // If the only choice for extension is dead cold (frequency == 0)
         // then do not extend even if it is possible. This is done to avoid
         // cases where we will extend with a colder block than the original
         // fall through (which was hotter).
         //
         if (cannotExtendWithCurrentFallThrough && bestExtension)
            {
            if (block != bestExtension)
               {
               //int32_t bestHotness = bestExtension->getHotness(cfg);
               int32_t bestFreq = bestExtension->getFrequency();
               if (!block->isCold() &&
                   (blockFreq >= 0) && (bestFreq >= 0))
                   //(blockHotness != unknownHotness) && (bestHotness != unknownHotness))
                  {
                  //if (((bestHotness == deadCold) && (blockHotness != deadCold)) || (blockHotness > (bestHotness + 1)))
                  if ((((bestFreq == (MAX_COLD_BLOCK_COUNT+1)) && (blockFreq != (MAX_COLD_BLOCK_COUNT+1))) || (blockFreq > bestFreq + 100)))
                     {
                     //bestExtension = NULL;
                     continue;
                     }
                  }
               }
            }
         }

      if (trace())
         traceMsg(comp(), "    Current block [%d] with freq %d, best extension is BB[%d]\n", prevBlock->getNumber(), block->getFrequency(), bestExtension?bestExtension->getNumber():-1);

      if (cannotExtendWithCurrentFallThrough ||
          (bestExtension &&
           !bestExtension->isCold() &&
          (bestExtension != block)))
         {
         //
         // The prevBlock cannot be extended to include the current block.
         // See if the prevBlock has a successor whose only predecessor is the prevBlock.
         // If so, see if we can rearrange the blocks so that the prevBlock can be extended
         // without adding any new gotos.
         //
         TR::Block * newBlock = 0;
         if (!bestExtension)
            {
            TR::CFGEdgeList & prevSuccessors = prevBlock->getSuccessors();
            for (auto e = prevSuccessors.begin(); e != prevSuccessors.end(); ++e)
                if ((*e)->getTo()->getPredecessors().size() == 1)
                    { newBlock = toBlock((*e)->getTo()); break; }
            }
         else
            newBlock = bestExtension;

         if (!newBlock)
            continue; // there's no way to extend the prevBlock

         if (!prevBlock->isCold() && newBlock->isCold()) continue;


         // See if we can easily move the current block without adding a new goto.
         //
         TR::Block * lastBlockToMove = block, * next;
         for (;; lastBlockToMove = next)
            {
            next = lastBlockToMove->getNextBlock();
            if (!next || !lastBlockToMove->hasSuccessor(next))
               break;

            //if (!(next->getPredecessors().size() == 1))
            //   { lastBlockToMove = 0; break; }
            }

         if (lastBlockToMove)
            {
            TR::TreeTop *previousTree = newBlock->getEntry()->getPrevTreeTop();

            if (newBlock->getPredecessors().size() == 1)
               {
               int32_t result = performChecksAndTreesMovement(newBlock, prevBlock, block, endTreeTop, visitCount, optimizer());
               if (result < 0)
                  {
                  if (cannotExtendWithCurrentFallThrough)
                     continue;
                  }
              else
                 {
                 if (result == 1)
                    endTreeTop = previousTree;
                  block = newBlock;
                  }
               }
            else
               {
               if (cannotExtendWithCurrentFallThrough)
                  continue;
               }
            }
         else
            {
            if (cannotExtendWithCurrentFallThrough)
               continue;
            }
         }

      // This block can be merged with the preceding block
      //
      TR::TreeTop *firstNonFenceTree = block->getFirstRealTreeTop();
      TR::TreeTop *lastNonFenceTree  = block->getLastRealTreeTop();

      if (prevNode->getOpCodeValue() == TR::Goto)
         {
         if (!performTransformation(comp(), "%sMerge blocks %d and %d\n", optDetailString(), prevBlock->getNumber(), block->getNumber()))
            continue;
         optimizer()->prepareForTreeRemoval(tt);
         TR::TransformUtil::removeTree(comp(), tt);
         }
      else if ((firstNonFenceTree == lastNonFenceTree) &&
               (lastNonFenceTree->getNode()->getOpCodeValue() == TR::Goto)
              )
         {
         TR::Block * nextBlock = block->getNextBlock();
         if (nextBlock && prevNode->getOpCode().isBranch() && !prevNode->isNopableInlineGuard())
            {
            TR::TreeTop *gotoDest = lastNonFenceTree->getNode()->getBranchDestination();
            TR::Block * prevBranchDest = prevNode->getBranchDestination()->getNode()->getBlock();
            if (prevNode->getBranchDestination() == nextBlock->getEntry() && !nextBlock->isCold())
               {
               if (!performTransformation(comp(), "%sReverse branch in block_%d\n", optDetailString(), prevBlock->getNumber()))
                  continue;
               for (auto c = 0; c < prevNode->getNumChildren(); ++c)
                  {
                  TR_ASSERT(prevNode->getChild(c)->getOpCodeValue() != TR::GlRegDeps, "the conditional branch node has a GlRegDep child and we're changing control flow");
                  }
               prevNode->reverseBranch(gotoDest);

               TR::Block *gotoDestBlock = gotoDest->getNode()->getBlock();
               if (!prevBlock->hasSuccessor(gotoDestBlock))
                  cfg->addEdge(prevBlock, gotoDestBlock);
               cfg->removeEdge(block, gotoDestBlock);
               cfg->removeEdge(prevBlock, block);

               prevBlock->getExit()->join(nextBlock->getEntry());

               block = prevBlock;
               reattemptThisExtension = true;
               continue;
               }
            else if (prevBranchDest->getVisitCount() < visitCount &&
                     prevBranchDest->getPredecessors().size() == 1)
               {
               // don't swing up cold blocks
               if (prevBranchDest->isCold())
                  continue;

               if (!performTransformation(comp(), "%sReverse branch in %d (possibly swinging up destintion and removign goto block_%d)\n", optDetailString(), prevBlock->getNumber(), block->getNumber()))
                  continue;

               // snip T (and its fall-through-chain) and swing it upto F, reversing
               // the branch in F, and potentially removing the goto block
               //
               TR::Block * F = prevBlock;
               TR::Block * T = prevBranchDest;
               TR::Block *pT = T->getPrevBlock();
               TR::Block * G;
               TR::Block *nG;

               // if the F and pT are the same (we have a redundant compare) - do not do any transformation
               if (F == pT) continue; // FIXME: maybe we should remove the redundant comparison

               for (G = T, nG = G->getNextBlock();
                    nG && G->hasSuccessor(nG);
                    G = nG, nG = nG->getNextBlock())
                  ;

               if (nG)
                  pT->getExit()->join(nG->getEntry());
               else
                  {
                  pT->getExit()->setNextTreeTop(0);
                  endTreeTop = pT->getExit();
                  }

               F->getExit()->join(T->getEntry());
               G->getExit()->join(block->getEntry());

               TR_ASSERT(prevNode->getNumChildren() != 3, "the branch node has a GlRegDep child that we're about to invalidate");
               prevNode->reverseBranch(gotoDest);

               int32_t copiedFrequency = 0;
               TR::CFGEdge *addedEdge = NULL;
               if (!F->hasSuccessor(gotoDest->getNode()->getBlock()))
                  {
                  addedEdge = cfg->addEdge(F, gotoDest->getNode()->getBlock());

                  TR_SuccessorIterator ei(F);
                  for (TR::CFGEdge * edge = ei.getFirst(); edge != NULL; edge = ei.getNext())
                     {
                     if (edge->getTo() == block)
                        {
                        copiedFrequency = edge->getFrequency();
                        break;
                        }
                     }
                  addedEdge->setFrequency(copiedFrequency);
                  }

               cfg->removeEdge(F, block); // ideally 'block' will become unreachable, and be removed

               block = prevBlock;
               reattemptThisExtension = true;
               continue;
               }
            else if (gotoDest->getNode()->getBlock() == prevBlock->startOfExtendedBlock())
               {
               if (!performTransformation(comp(), "%sRemove backedge goto block_%d by reversing the branch in block_%d\n", optDetailString(), block->getNumber(), prevBlock->getNumber()))
                  continue;

               // goto is a backedge, reverse branch, swap destinations between branch and goto
               //
               TR::TreeTop *prevDest = prevNode->getBranchDestination();
               TR_ASSERT(prevNode->getNumChildren() != 3, "the branch node has a GlRegDep child that we're about to invalidate");

               prevNode->reverseBranch(gotoDest);
               cfg->addEdge   (prevBlock, gotoDest->getNode()->getBlock());
               cfg->addEdge   (block, prevDest->getNode()->getBlock());
               cfg->removeEdge(prevBlock, prevDest->getNode()->getBlock());

               lastNonFenceTree->getNode()->setBranchDestination(prevDest);
               cfg->removeEdge(block, gotoDest->getNode()->getBlock());
               }
            }
         }

      if (!performTransformation(comp(), "%sExtend blocks %d and %d\n", optDetailString(), prevBlock->getNumber(), block->getNumber()))
         continue;

      blocksWereExtended = true;
      block->setIsExtensionOfPreviousBlock();
      }

   if (blocksWereExtended)
      optimizer()->enableAllLocalOpts();

   return 1; // actual cost
   }



struct TR_SequenceEntry
   {
   TR_ALLOC(TR_Memory::Optimization)

   TR_SequenceEntry *next;
   TR::Block      *block;
   TR::Block      *fallThrough;
   };
struct TR_SequenceHead
   {
   TR_SequenceEntry *first;
   TR_SequenceEntry *last;
   };

int32_t TR_ExtendBasicBlocks::orderBlocksWithFrequencyInfo()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Build sequences of blocks that are joined by edges with the highest
   // frequencies.
   //

   TR::CFG          *cfg     = comp()->getFlowGraph();
   TR::Block        *block   = NULL;
   TR::TreeTop      *treeTop = NULL;
   TR::Node         *node    = NULL;
   TR_SequenceEntry *entry   = NULL;
   int32_t           i       = 0;
   int32_t           numBlocks = cfg->getNextNodeNumber();
   int32_t           bestFrequency = 0, bestFrom = 0, bestTo = 0;

   // Array of sequences - the Nth entry is the sequence that starts with
   // block N.
   //
   TR_SequenceHead *sequences = (TR_SequenceHead*)trMemory()->allocateStackMemory(numBlocks*sizeof(TR_SequenceHead));
   memset(sequences, 0, numBlocks*sizeof(TR_SequenceHead));

   // Initialize each sequence with a single block
   //
   block              = toBlock(cfg->getStart());
   entry              = new (trStackMemory()) TR_SequenceEntry;
   entry->next        = NULL;
   entry->block       = block;
   i                  = block->getNumber();
   sequences[i].first = entry;
   sequences[i].last  = entry;

   block              = toBlock(cfg->getEnd());
   entry              = new (trStackMemory()) TR_SequenceEntry;
   entry->next        = NULL;
   entry->block       = block;
   i                  = block->getNumber();
   sequences[i].first = entry;
   sequences[i].last  = entry;

   // Remember if this block falls through to the next block
   //
   entry->fallThrough = NULL;
   for (block = comp()->getStartTree()->getNode()->getBlock(); block;
        block = block->getNextBlock())
      {
      entry              = new (trStackMemory()) TR_SequenceEntry;
      entry->next        = NULL;
      entry->block       = block;
      i                  = block->getNumber();
      sequences[i].first = entry;
      sequences[i].last  = entry;

      // Remember if this block falls through to the next block
      //
      entry->fallThrough = NULL;
      treeTop = block->getLastRealTreeTop();
      node = treeTop->getNode();
      if (node->getOpCode().isCheck() || node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();
      if (node->getOpCode().isBranch())
         {
         if (node->getOpCode().isIf())
            entry->fallThrough = block->getNextBlock();
         }
      else if (!node->getOpCode().isReturn() &&
               !node->getOpCode().isJumpWithMultipleTargets() &&
               node->getOpCodeValue() != TR::athrow)
         {
         entry->fallThrough = block->getNextBlock();
         }
      }

   // Give a preference to edges that will result in an extended basic block.
   // The preference amount is the amount that the edge frequency will be
   // increased by. It is calculated from a percentage point amount, i.e.
   // units of 1/100 of a percent.
   //
   static const char *p;
   static int32_t extendedBlockPointPreference = (p = feGetEnv("TR_ExtendedBlockPoints")) ? atoi(p) : 500;
   int32_t extendedBlockPreference = extendedBlockPointPreference * (MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT) / 10000;

   if (trace())
      traceMsg(comp(), "Start block re-ordering\n");

   // Merge sequences
   //
   int32_t numSequences;
   int32_t oldNumSequences = numBlocks+1;
   for (numSequences = numBlocks; numSequences > 1 && numSequences < oldNumSequences; )
      {
      oldNumSequences = numSequences;

      // Find the sequence that has the output edge to another sequence with
      // the highest frequency
      //
      bestFrequency = -1;
      for (i = 0; i < numBlocks; ++i)
         {
         if (!sequences[i].first)
            continue; // This block already consumed

         TR_SequenceEntry *currentBlockEntry = sequences[i].last;
         for (auto edge = currentBlockEntry->block->getSuccessors().begin(); edge != currentBlockEntry->block->getSuccessors().end(); ++edge)
            {
            int32_t edgeTo = (*edge)->getTo()->getNumber();
            if (edgeTo != i && sequences[edgeTo].first)
               {
               int32_t edgeFrequency = (*edge)->getFrequency();
               if ((*edge)->getTo()->getPredecessors().size() == 1)
                  {
                  block = toBlock((*edge)->getFrom());
                  if (block->getExit() &&
                      !(block->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets()))
                     {
                     edgeFrequency += extendedBlockPreference;
                     }
                  }
               if ((edgeFrequency > bestFrequency) ||
                   ((edgeFrequency == bestFrequency) &&
                    (currentBlockEntry->fallThrough == (*edge)->getTo())))
                  {
                  bestFrequency = edgeFrequency;
                  bestFrom      = i;
                  bestTo        = edgeTo;
                  }
               }
            }
         }

      // Join the sequences
      //
      if (bestFrequency >= 0)
         {
         sequences[bestFrom].last->next = sequences[bestTo].first;
         sequences[bestFrom].last       = sequences[bestTo].last;
         sequences[bestTo].first        = NULL;
         numSequences--;
         if (trace())
            traceMsg(comp(), "   add %d to %d\n", bestTo, bestFrom);
         }
      }

   if (trace())
      for (int32_t i = 0; i < numBlocks; ++i)
    {
    if (sequences[i].first != NULL)
       {
       traceMsg(comp(), "Seq %3d: ", i);
       for (TR_SequenceEntry *entry = sequences[i].first; entry; entry = entry->next)
          {
          TR::Block *block = entry->block;
          traceMsg(comp(), "%3d(%5d) ", block->getNumber(), block->getFrequency());
          }
       traceMsg(comp(), "\n");
       }
    }

   bool blocksWereExtended = false;

   // Now reorder the blocks so that the sequence containing the entry node is
   // first, and order the rest in descending frequency of their first block.
   //
   TR::TreeTop *prevTree = NULL;
   bestFrom = cfg->getStart()->getNumber();
   while (bestFrom >= 0)
      {
      bool firstInSequence = true;
      bool prevBlockHasSwitch = false;
      for (entry = sequences[bestFrom].first; entry; entry = entry->next)
         {
         block = entry->block;
         if (!block->getEntry())
            continue;
         if (trace())
            traceMsg(comp(), "   Insert block_%d %s\n", block->getNumber(), block->isCold() ? "cold" : "" );

         // Join this block's trees to the previously processed block
         //
         if (prevTree)
            prevTree->join(block->getEntry());
         else
            comp()->setStartTree(block->getEntry());

         if (!firstInSequence && !prevBlockHasSwitch &&
             (block->getPredecessors().size() == 1))
            {
            block->setIsExtensionOfPreviousBlock();
            blocksWereExtended = true;
            }

         // See if the end of this block needs adjusting
         //
         treeTop = block->getLastRealTreeTop();
         node    = treeTop->getNode();
         if (entry->next && entry->next->block->getEntry())
            {
            // This block will now fall through to the next block in the
            // sequence
            //
            if (entry->fallThrough)
               {
               if (entry->next && entry->fallThrough == entry->next->block)
                  {
                  // The new fall-through block is the same as the original.
                  // There is nothing to do.
                  }
               else
                  {
                  // This block originally fell through to a different block
                  //
                  TR_ASSERT(node->getOpCode().isIf(), "assertion failure");
                  node->reverseBranch(entry->fallThrough->getEntry());
                  }
               }
            else
               {
               // This block didn't fall through to anything. If it ended in a
               // goto, the goto can be removed
               //
               if (node->getOpCodeValue() == TR::Goto)
                  {
                  optimizer()->prepareForTreeRemoval(treeTop);
                  TR::TransformUtil::removeTree(comp(), treeTop);
                  }
               }
            }
         else
            {
            // This block will not fall through to any successor. If it
            // originally did fall through, we must insert a goto to the
            // original fallthrough target.
            //
            if (entry->fallThrough)
               {
               comp()->getFlowGraph()->setStructure(NULL);
               TR::Block *extraBlock = TR::Block::createEmptyBlock(node, comp(), entry->fallThrough->getFrequency(), entry->fallThrough);
               TR::TreeTop::create(comp(), extraBlock->getEntry(), TR::Node::create(node, TR::Goto, 0, entry->fallThrough->getEntry()));
               block->getExit()->join(extraBlock->getEntry());

               cfg->addNode(extraBlock);
               cfg->addEdge(extraBlock, entry->fallThrough);
               cfg->addEdge(block, extraBlock);
               cfg->removeEdge(block, entry->fallThrough);
               cfg->copyExceptionSuccessors(block, extraBlock);

               block = extraBlock;
               block->setIsExtensionOfPreviousBlock();
               }
            }

         prevTree = block->getExit();
         prevTree->setNextTreeTop(NULL);
         firstInSequence = false;
         prevBlockHasSwitch = node->getOpCode().isJumpWithMultipleTargets();
         }
      sequences[bestFrom].first = NULL;

      // Find the next sequence to be processed
      //
      bestFrequency = -1;
      bestFrom = -1;
      for (i = numBlocks-1; i >= 0; i--)
         {
         entry = sequences[i].first;
         if (entry && entry->block->getFrequency() > bestFrequency)
            {
            bestFrequency = entry->block->getFrequency();
            bestFrom = i;
            }
         }
      }

   if (blocksWereExtended)
      optimizer()->enableAllLocalOpts();

   return 1; // actual cost
   }

const char *
TR_ExtendBasicBlocks::optDetailString() const throw()
   {
   return "O^O BASIC BLOCK EXTENSION: ";
   }

// void TR_BlockManipulator::markColdBlocks()
//    {
//   TR::Block *block = comp()->getStartTree()->getNode()->getBlock();
//    for (; block;block = block->getNextBlock())
//       {
//       TR::TreeTop *lastNonFenceTree  = block->getLastRealTreeTop();
//       TR::Node *node = lastNonFenceTree->getNode();

//       if (node->getOpCode().isReturn() ||
//           (node->getOpCode().isTreeTop() &&
//           (node->getNumChildren() > 0) &&
//            (node->getFirstChild()->getOpCodeValue() == TR::athrow)))
//           block->setIsCold();

//       // mark the guarded virtual call blocks as being cold
//       //
//       if (node->isTheVirtualGuardForAGuardedInlinedCall())
//          {
//          TR::TreeTop *tt = node->getVirtualCallTreeForGuard();
//          if (tt)
//             {
//             for (;
//                  tt->getNode()->getOpCodeValue() != TR::BBEnd;
//                  tt = tt->getNextRealTreeTop())
//                ;
//             tt->getNode()->getBlock()->setIsCold();
//             }
//          }
//       }
//    }

int32_t TR_BlockManipulator::estimatedHotness(TR::CFGEdge *edge, TR::Block *block)
   {
   return edge->getFrequency();
   /*
   int32_t hotness = unknownHotness;

   if (block->isCold())
      return -1;

   if (edge)
      hotness = edge->getHotness(comp()->getFlowGraph());

   if (hotness == unknownHotness)
      {
      if (block->isCold())
         hotness = -2;
      else
         {
         hotness = block->getHotness(comp()->getFlowGraph());
         if (hotness == unknownHotness)
            hotness = block->getNestingDepth();
         }
      }
   return hotness;
   */
   }


TR::Block *TR_BlockManipulator::getBestChoiceForExtension(TR::Block *block1)
   {
   TR::Block *bestExtension = NULL;
   int32_t numberOfRealTreesInBlock = -1;

   TR::Block *fallThroughBlock = 0;
   TR::TreeTop *nextTree = block1->getExit()->getNextRealTreeTop();
   if (nextTree)
      fallThroughBlock = nextTree->getNode()->getBlock();

   if (block1->getLastRealTreeTop()->getNode()->getOpCode().isBranch() &&
       block1->getLastRealTreeTop()->getNode()->isNopableInlineGuard())
      return fallThroughBlock;

   int32_t hotness = -3;
   int32_t nestingDepth = -1;
   for (auto nextEdge = block1->getSuccessors().begin(); nextEdge != block1->getSuccessors().end(); ++nextEdge)
      {
      TR::Block *nextBlock = toBlock((*nextEdge)->getTo());
      if (!(nextBlock->getPredecessors().size() == 1))
         continue; // Not a candidate for extension

      int32_t nextHotness = estimatedHotness(*nextEdge, nextBlock);

      if (trace())
         traceMsg(comp(), "    Estimating hotness for BB [%d], next BB [%d], estimated hotness %d\n", block1->getNumber(), nextBlock->getNumber(), nextHotness);

      if (nextHotness > hotness)
         {
         bestExtension = nextBlock;
         numberOfRealTreesInBlock = countNumberOfTreesInSameExtendedBlock(nextBlock);
         hotness = nextHotness;
    int32_t nextBlockNestingDepth = 1;
    if (nextBlock->getStructureOf())
       nextBlock->getStructureOf()->calculateFrequencyOfExecution(&nextBlockNestingDepth);
    nestingDepth = nextBlockNestingDepth;
         }
      else if (hotness >= 0 && nextHotness == hotness)
         {
    int32_t nextBlockNestingDepth = 1;
    if (nextBlock->getStructureOf())
       nextBlock->getStructureOf()->calculateFrequencyOfExecution(&nextBlockNestingDepth);

         int32_t nextBlockNumTrees = countNumberOfTreesInSameExtendedBlock(nextBlock);
    if ((nextBlockNestingDepth > nestingDepth) ||
        ((nextBlockNestingDepth == nestingDepth) &&
         (nextBlockNumTrees > numberOfRealTreesInBlock)))
       {
       bestExtension = nextBlock;
       numberOfRealTreesInBlock = nextBlockNumTrees;
            hotness = nextHotness;
       nestingDepth = nextBlockNestingDepth;
       }
         else if ((nextBlockNestingDepth == nestingDepth) &&
        (nextBlockNumTrees == numberOfRealTreesInBlock))
            {
            if (nextBlock == fallThroughBlock)
               bestExtension = nextBlock;
            }
         }
      else if (nextHotness == hotness)
         {
         // In case of a tie between cold blocks, favour nextBlock
         // if it is the fall through block.
         //
         if (nextBlock == fallThroughBlock)
            {
            bestExtension = nextBlock;
            numberOfRealTreesInBlock = countNumberOfTreesInSameExtendedBlock(nextBlock);
            int32_t nextBlockNestingDepth = 1;
            if (nextBlock->getStructureOf())
               nextBlock->getStructureOf()->calculateFrequencyOfExecution(&nextBlockNestingDepth);
            nestingDepth = nextBlockNestingDepth;
            }
         }
      }

   return bestExtension;
   }




bool TR_BlockManipulator::isBestChoiceForFallThrough(TR::Block *block1, TR::Block *block2)
   {
   // We don't want supercold block to be fall-through block
   if (block1->isSuperCold() ||
       ((block1->getSuccessors().size() == 1) &&
        block1->getSuccessors().front()->getTo()->asBlock()->isSuperCold()))
      return false;

   bool bestSucc = false;
   bool bestPred = false;
   if (block1->getSuccessors().size() == 1)
      bestSucc = true;
   if (block2->getPredecessors().size() == 1)
      bestPred = true;

   if (bestSucc && bestPred)
      return true;

   // Loop back edges are usually desired to be backward branches
   // into the loop entry block; the loop pre-header should fall into the
   // entry. This is an exception to the usual rule where we allow the hottest
   // block to fall into a successor. In this case the hottest block (block
   // inside the loop that conditionally branches back to entry) branches back to
   // the loop whereas the colder block falls into it
   //
   bool isLoopBackEdge = false;

   if (comp()->getFlowGraph()->getStructure())
      {
      bool isLoopHdr = false;
      TR_Structure *containingLoop = NULL;
      TR_Structure *blockStructure2 = block2->getStructureOf();
      while (blockStructure2)
         {
         if (blockStructure2->asRegion() && blockStructure2->asRegion()->isNaturalLoop())
            {
            if (blockStructure2->getNumber() == block2->getNumber())
               isLoopHdr = true;
            containingLoop = blockStructure2;
            break;
            }

         blockStructure2 = blockStructure2->getParent();
         }

      if (isLoopHdr)
         {
         if (blockStructure2->asRegion()->getEntryBlock()->getStructureOf()->isEntryOfShortRunningLoop())
            return false;

         TR_Structure *blockStructure1 = block1->getStructureOf();
         while (blockStructure1)
            {
            if (blockStructure1 == containingLoop)
               {
               isLoopBackEdge = true;
               break;
               }

            blockStructure1 = blockStructure1->getParent();
            }
         }
      }

   if (isLoopBackEdge)
     {
     return false;
     }

   int32_t numberOfRealTreesInBlock = countNumberOfTreesInSameExtendedBlock(block2);

   int32_t hotness;

   auto nextEdge = block1->getSuccessors().begin();
   for (; nextEdge != block1->getSuccessors().end(); ++nextEdge)
      {
      TR::Block *nextBlock = toBlock((*nextEdge)->getTo());
      if (nextBlock == block2)
         break;
      }

   TR::CFGEdge *currentEdge = NULL;
   if(nextEdge != block1->getSuccessors().end())
      {
      currentEdge = *nextEdge;
      hotness = estimatedHotness(*nextEdge, block2);
      }
   else
      hotness = estimatedHotness(0, block2);

   bool block2Won = false;
   for (nextEdge = block1->getSuccessors().begin(); nextEdge != block1->getSuccessors().end(); ++nextEdge)
      {
      TR::Block *nextBlock = toBlock((*nextEdge)->getTo());
      if (nextBlock == block2)
         continue;

      int32_t nextBlockHotness = estimatedHotness(*nextEdge, nextBlock);
      if (nextBlockHotness > hotness)
         return false;
      else if (nextBlockHotness < hotness)
         block2Won = true;
      else if (hotness >= 0)
         {
         int32_t nextTrees = countNumberOfTreesInSameExtendedBlock(nextBlock);
         if (nextTrees > numberOfRealTreesInBlock)
            return false;
         else if (nextTrees < numberOfRealTreesInBlock)
            block2Won = true;
         }
      }

   if (!block2Won) // if all comparisons were ties - then maintain status quo
      return false;

   if (block1->getLastRealTreeTop()->getNode()->getOpCode().isBranch() &&
       block1->getLastRealTreeTop()->getNode()->isNopableInlineGuard())
      {
      TR::Block *fallThroughBlock = 0;
      TR::TreeTop *nextTree = block1->getExit()->getNextRealTreeTop();
      if (nextTree)
         fallThroughBlock = nextTree->getNode()->getBlock();

      if (block2 != fallThroughBlock)
         return false;
      }

   // at this point we know that block2 is a good choice as a fall through - unless its better
   // to keep it where it is because of synergy with its predecessors

   TR::Block *prevBlock = block2->getPrevBlock();
   if (prevBlock &&
       prevBlock->hasSuccessor(block2))
      {
      int32_t hotness1 = estimatedHotness(currentEdge, block1);
      auto nextEdge = prevBlock->getSuccessors().begin();
      for (; nextEdge != prevBlock->getSuccessors().end(); ++nextEdge)
         {
         TR::Block *nextBlock = toBlock((*nextEdge)->getTo());
         if (nextBlock == block2)
            break;
         }
      int32_t prevBlockHotness;
      if(nextEdge != prevBlock->getSuccessors().end())
         prevBlockHotness = estimatedHotness(*nextEdge, prevBlock);
      else
    	 prevBlockHotness = estimatedHotness(0, prevBlock);
      if (prevBlockHotness > hotness1)
         return false;
      }

   return true;
   }



int32_t TR_BlockManipulator::countNumberOfTreesInSameExtendedBlock(TR::Block *block)
   {
   int32_t numTrees = block->getNumberOfRealTreeTops();
   TR::Block *prevB = block;
   TR::Block *nextB = block->getNextBlock();
   while (nextB &&
          !nextB->isCold() &&
          prevB->hasSuccessor(nextB) &&
          (nextB->getPredecessors().size() == 1))
      {
      numTrees += nextB->getNumberOfRealTreeTops();
      prevB = nextB;
      nextB = nextB->getNextBlock();
      }
   return numTrees;
   }


//
// Checks if block1 and block2 are inside the same loop and block3 is not in that loop
//
static bool insideLoop(TR::Compilation *comp, TR::Block *block1, TR::Block *block2, TR::Block *block3)
   {
   if (!comp->getFlowGraph()->getStructure())
      return false;
   if (!block1->getStructureOf() || !block2->getStructureOf())
      return false;

   TR_Structure * loop1 = block1->getStructureOf()->getContainingLoop();
   TR_Structure * loop2 = block2->getStructureOf()->getContainingLoop();
   TR_Structure * loop3 = block3->getStructureOf() ? block3->getStructureOf()->getContainingLoop() : NULL;

   if (loop1 && loop1 == loop2 && loop1 != loop3)
      return true;

   return false;
   }

// newBlock = the block to be moved
// prevBlock = the block before the target insertion point
// block     = the block after the target insertion point
// endTreeTop = the last treetop in the block to be moved
//
int32_t TR_BlockManipulator::performChecksAndTreesMovement(TR::Block *newBlock, TR::Block *prevBlock, TR::Block *block,
      TR::TreeTop *endTreeTop, vcount_t visitCount, TR::Optimizer *optimizer)
   {
   TR::TreeTop * tt = prevBlock->getLastRealTreeTop();
   TR::Node * prevNode = tt->getNode();

   TR::TreeTop *startOfTreesBeingMoved = newBlock->getEntry();

   if (startOfTreesBeingMoved != comp()->getStartTree())
      {
      if (startOfTreesBeingMoved->getPrevTreeTop()->getNode()->getBlock()->hasSuccessor(startOfTreesBeingMoved->getNode()->getBlock()))
          return -1;
      }
   else
      return -1;


   if (newBlock->getVisitCount() < visitCount)
      {
      // TODO: reenable allowBlockToBeReordered check
      if (
          !performTransformation(comp(), "%sswing up block_%d to maximize fall through of block_%d\n",
                 optDetailString(), newBlock->getNumber(), prevBlock->getNumber()))
         return -1;

      // Follow the fall through chain of newBlock uptil the last non-cold block (or the method
      // entry if there are no cold blocks)
      //
      TR::Block *cursor = NULL;
      TR::Block *cursorNext = NULL;
      for (cursor = newBlock, cursorNext = cursor->getNextBlock();
           cursorNext &&
           (cursor->hasSuccessor(cursorNext) || insideLoop(comp(), cursor, cursorNext, block)) &&
           (!cursorNext->isCold() || cursorNext->isExtensionOfPreviousBlock() ||
                 cursor->getLastRealTreeTop()->getNode()->isNopableInlineGuard());
           cursor = cursorNext, cursorNext = cursorNext->getNextBlock())
         ;

      TR::Block *nextBlock = prevBlock->getNextBlock();
      if (cursorNext && cursor->hasSuccessor(cursorNext))
         {
         // may need to add a goto block (unless cursor is ends with a branch
         // with destination nextBlock)
         //
         TR::Node *lastNode = cursor->getLastRealTreeTop()->getNode();
         if (lastNode->getOpCode().isBranch() &&
             lastNode->getBranchDestination() == nextBlock->getEntry())
            {
            // Reverse the branch
            //
            lastNode->reverseBranch(cursorNext->getEntry());
            }
         else
            cursor = breakFallThrough(cursor, cursorNext, false);
         }


      TR::Block *newPrev = newBlock->getPrevBlock();

      /////// This just surgically reattaches the treetops. This does NOT set successor/predecessor info or changes CFG
      prevBlock->getExit()->join(newBlock->getEntry());

      if (nextBlock)
         cursor->getExit()->join(nextBlock->getEntry());
      else
         cursor->getExit()->setNextTreeTop(NULL);

      newPrev->getExit()->join(cursorNext ? cursorNext->getEntry() : 0);
      ///////


      // If it was non-goto (cond. branch), just reverse condition and make it point to block (old fallthrough)
      // newBlock is the new fallthrough.
      auto &prevOpCode = prevNode->getOpCode();
      if (prevOpCode.isBooleanCompare() && prevOpCode.isBranch())
         {
         prevNode->reverseBranch(block->getEntry());
         }
      // If prevBlock had goto to newBlock, we simply need to just remove the goto
      else if (prevNode->getOpCodeValue() == TR::Goto)
         {
         optimizer->prepareForTreeRemoval(tt);
         TR::TransformUtil::removeTree(comp(), tt);
         }

      return cursorNext ? 2 : 1;
      /*
      TR::TreeTop *lastTree = startOfTreesBeingMoved->getPrevTreeTop();
      TR::TreeTop *exitTree = prevBlock->getExit();
      TR::TreeTop *nextTree = exitTree->getNextTreeTop();

      TR::TreeTop *endOfTreesBeingMoved = endTreeTop;
      TR::TreeTop *followingTree = NULL;
      TR::Block *prevCursorBlock = startOfTreesBeingMoved->getNode()->getBlock();
      TR::TreeTop *cursorTree = prevCursorBlock->getExit()->getNextTreeTop();

      while (cursorTree)
          {
          TR::Block *cursorBlock = cursorTree->getNode()->getBlock();
          if (!prevCursorBlock->hasSuccessor(cursorBlock))
              {
              endOfTreesBeingMoved = prevCursorBlock->getExit();
              followingTree = endOfTreesBeingMoved->getNextTreeTop();
              break;
              }

          if (cursorBlock == newBlock)
              break;

          prevCursorBlock = cursorBlock;
          cursorTree = cursorBlock->getExit()->getNextTreeTop();
          }

      exitTree->join(startOfTreesBeingMoved);
      endOfTreesBeingMoved->join(nextTree);
      lastTree->join(followingTree);

      if (prevNode->getOpCodeValue() != TR::Goto)
         {
         prevNode->reverseBranch(block->getEntry());
         }
      else
         {
         optimizer->prepareForTreeRemoval(tt);
         TR::TransformUtil::removeTree(comp(), tt);
         }
      return followingTree ? 2 : 1;
      */
      }
   else
      {
      TR::TreeTop *endOfTreesBeingMoved = NULL;
      TR::TreeTop *cursorTree = prevBlock->getEntry()->getPrevTreeTop();
      TR::Block *prevCursorBlock = prevBlock;
      while (cursorTree)
          {
          TR::Block *cursorBlock = cursorTree->getNode()->getBlock();
          if (!cursorBlock->hasSuccessor(prevCursorBlock))
              {
              endOfTreesBeingMoved = cursorBlock->getExit();
              break;
              }
          if (cursorBlock == newBlock)
              break;

          prevCursorBlock = cursorBlock;
          cursorTree = cursorBlock->getEntry()->getPrevTreeTop();
          }

      if (
          endOfTreesBeingMoved && !prevNode->isNopableInlineGuard() &&
          performTransformation(comp(), "%sswing down block_%d to maximize fall through with block_%d\n",optDetailString(), newBlock->getNumber(), prevBlock->getNumber()))
         {
         //comp()->dumpMethodTrees("Trees before :");
         TR::TreeTop *nextTree = prevBlock->getExit()->getNextTreeTop();
         TR::TreeTop *exitTree = prevBlock->getExit();
         TR::TreeTop *prevTree = startOfTreesBeingMoved->getPrevTreeTop();
         prevTree->join(endOfTreesBeingMoved->getNextTreeTop());
         exitTree->join(startOfTreesBeingMoved);
         endOfTreesBeingMoved->join(nextTree);
         if (prevNode->getOpCodeValue() != TR::Goto)
            {
            prevNode->reverseBranch(block->getEntry());
            }
         else
            {
            optimizer->prepareForTreeRemoval(tt);
            TR::TransformUtil::removeTree(comp(), tt);
            }

         if (!nextTree)
            return 1;
         return 2;
         }
      else if (!endOfTreesBeingMoved)
         {
         TR::Node *lastNode = newBlock->getLastRealTreeTop()->getNode();
         TR::Block *nextOfNewBlock = newBlock->getNextBlock();
         if (//comp()->cg()->allowBlockToBeReordered(newBlock) &&
           lastNode->getOpCode().isBooleanCompare() && !lastNode->isNopableInlineGuard() && !nextOfNewBlock->isExtensionOfPreviousBlock() &&
             performTransformation(comp(), "%sswing down block_%d and break original fall through to join with block_%d\n",
                     optDetailString(), newBlock->getNumber(), prevBlock->getNumber()))
            {
            TR::Block *prevOfNewBlock = newBlock->getPrevBlock();
            TR::Block *oldDestination = lastNode->getBranchDestination()->getNode()->getBlock();

            prevOfNewBlock->getExit()->join(nextOfNewBlock->getEntry());
            prevBlock->getExit()->join(newBlock->getEntry());

            lastNode->reverseBranch(nextOfNewBlock->getEntry());

            TR::Node *prevBlockLastNode = prevBlock->getLastRealTreeTop()->getNode();
            if (prevBlockLastNode->getOpCode().isBooleanCompare())
               {
               prevBlockLastNode->reverseBranch(block->getEntry());
               }

            if (oldDestination == block)
               {
               // we do not need a goto block
               //
               newBlock->getExit()->join(block->getEntry());
               }
            else
               {
               TR::Node *gotoNode = TR::Node::create(lastNode, TR::Goto);
               TR::Block *gotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), oldDestination->getFrequency(), oldDestination);
               gotoBlock->append(TR::TreeTop::create(comp(), gotoNode));
               if (newBlock->getStructureOf())
             comp()->getFlowGraph()->addNode(gotoBlock, newBlock->getCommonParentStructureIfExists(oldDestination, comp()->getFlowGraph()));
               else
             comp()->getFlowGraph()->addNode(gotoBlock);

               gotoNode->setBranchDestination(oldDestination->getEntry());

               newBlock->getExit()->join(gotoBlock->getEntry());
                    if (block)
                  gotoBlock->getExit()->join(block->getEntry());
                    else
                       gotoBlock->getExit()->setNextTreeTop(0);

               comp()->getFlowGraph()->addEdge(gotoBlock, oldDestination);
               comp()->getFlowGraph()->addEdge(newBlock, gotoBlock);
               comp()->getFlowGraph()->removeEdge(newBlock, oldDestination);
               }
            return 2;
            }
         }
      }
   return -1;
   }


bool TR_HoistBlocks::hasSynergy(TR::Block *block, TR::Node *node1)
   {
   bool synergyExists = false;
   TR::TreeTop *blockEntry = block->getEntry();
   TR::TreeTop *tt2 = block->getExit();
   while (tt2 != blockEntry)
      {
      TR::Node *node2 = tt2->getNode();
      if (node2->getOpCode().isStore())
         {
         int32_t childNum;
         for (childNum = 0; childNum < node1->getNumChildren(); childNum++)
            {
            TR::Node *child = node1->getChild(childNum);
            if (child->getOpCode().hasSymbolReference())
               {
               if (node2->mayKill().contains(child->getSymbolReference(), comp()))
                  {
                  synergyExists = true;
                  break;
                  }
               }
            }
         }
      tt2 = tt2->getPrevRealTreeTop();
      }

   return synergyExists;
   }

const char *
TR_HoistBlocks::optDetailString() const throw()
   {
   return "O^O BASIC BLOCK HOISTING: ";
   }



// For short blocks that consist of either a return statement or
// a conditional branch to the end of all predecessor blocks.
// Returns "true" if any blocks were hoisted.

TR_HoistBlocks::TR_HoistBlocks(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}


int32_t TR_HoistBlocks::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_HoistBlocks::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }

// Used to clone nodes within a block where parent/child links are to be
// duplicated exactly for multiply referenced nodes
//
static TR::Node *duplicateExact(TR::Node *node, List<TR::Node> *seenNodes, List<TR::Node> *duplicateNodes, TR::Compilation *comp)
   {
   vcount_t visitCount = comp->getVisitCount();
   if (visitCount == node->getVisitCount())
      {
      // We've seen this node before - find its duplicate
      //
      ListIterator<TR::Node> seenNodesIt(seenNodes);
      ListIterator<TR::Node> duplicateNodesIt(duplicateNodes);
      TR::Node *nextDuplicateNode = duplicateNodesIt.getFirst();
      TR::Node *nextNode;
      for (nextNode = seenNodesIt.getFirst(); nextNode != NULL; nextNode = seenNodesIt.getNext())
         {
         if (nextNode == node)
            {
            nextDuplicateNode->incReferenceCount();
            return nextDuplicateNode;
            }
         nextDuplicateNode = duplicateNodesIt.getNext();
         }
      TR_ASSERT(nextNode == NULL,"Loop unroller, error duplicating trees");
      }

   TR::Node *newRoot = TR::Node::copy(node);
   if (node->getOpCode().hasSymbolReference())
      {
      newRoot->setSymbolReference(node->getSymbolReference());
      }

   newRoot->setReferenceCount(1);
   node->setVisitCount(visitCount);
   if (node->getReferenceCount() > 1)
      {
      duplicateNodes->add(newRoot);
      seenNodes->add(node);
      }

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      newRoot->setChild(i, duplicateExact(node->getChild(i), seenNodes, duplicateNodes, comp));

   return newRoot;
   }

int32_t TR_HoistBlocks::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   if (cfg == NULL)
      return 0;

   //int32_t debugCounter = 0;
   bool        blocksWereHoisted = false;
   TR::CFGNode *cfgStart = cfg->getStart();
   //TR::CFGNode *cfgEnd   = cfg->getEnd();

   static const char *useEdgeFreqs = feGetEnv("TR_UseEdgeFreqs");
   bool usingEdgeFreqs = (useEdgeFreqs != NULL);

   TR_ScratchList<TR::Block> newBlocks(trMemory());

   for (TR::TreeTop *treeTop = startTree, *exitTreeTop; (treeTop != endTree); treeTop = exitTreeTop->getNextTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Local Opts, expected BBStart treetop");
      TR::Block *block = node->getBlock();
      exitTreeTop     = block->getExit();

      if (block->isCold()) continue;
      if (block->getSuccessors().empty()) continue;
      if (block->getPredecessors().empty())  continue;

      TR::TreeTop *tt1 = block->getLastRealTreeTop();
      TR::Node *node1 = tt1->getNode();

      // If the block in not very complex and has nothing to do with
      // exception edges, then we can benefit
      // in optimizations like local dead store elimination/commoning by
      // hoisting this block into the predecessor.
      //
#ifdef J9_PROJECT_SPECIFIC
      TR_CatchBlockProfileInfo * catchInfo;
#endif

      if ((node1->getOpCode().isReturn() ||
#ifdef J9_PROJECT_SPECIFIC
          (node1->getOpCodeValue() == TR::athrow &&
          (catchInfo = TR_CatchBlockProfileInfo::get(comp())) && catchInfo->getCatchCounter() >= 100) ||
#endif
          (node1->getOpCode().isBranch() && (node1->getOpCodeValue() != TR::Goto)) ||
          ((node1->getOpCodeValue() == TR::Goto) && (!(block->getPredecessors().size() == 1)))))
         {
         bool exceptionEdgesMatter = true;
         if (tt1 == block->getFirstRealTreeTop())
            exceptionEdgesMatter = false;

         bool isLoopHdr = false;
         TR_Structure *containingLoop = NULL;
         //
         // Do not hoist the loop header as this could result in an improper
         // region
         //
         TR_Structure *blockStructure = block->getStructureOf();
         while (blockStructure)
            {
            if (blockStructure->asRegion() && blockStructure->asRegion()->isNaturalLoop() && (blockStructure->getNumber() == block->getNumber()))
               {
               isLoopHdr = true;
               containingLoop = blockStructure;
               break;
               }

            blockStructure = blockStructure->getParent();
            }

         int32_t numColdPreds = 0;
         int32_t numNonColdPreds = 0;
         for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
            {
            TR::Block *predBlock = toBlock((*nextEdge)->getFrom());
            if (predBlock->isCold())
               numColdPreds++;
            else
               numNonColdPreds++;
            }

         //ListIterator<TR::CFGEdge> bi(&block->getPredecessors());
         for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end();)
            {
            auto next = nextEdge;
            ++next;
            TR::CFGEdge* current = *nextEdge;
            TR::Block *prevBlock = toBlock((*nextEdge)->getFrom());

            if ((prevBlock->isCold()) || (prevBlock == cfgStart) || (prevBlock == block) ||
                (exceptionEdgesMatter && cfg->compareExceptionSuccessors(block, prevBlock)))
               {
               nextEdge = next;
               continue;
               };

            bool isLoopBackEdge = false;
            //
            // Do not hoist the loop header as this could result in an improper
            // region
            //
            TR_Structure *prevBlockStructure = prevBlock->getStructureOf();
            while (prevBlockStructure)
               {
               if (prevBlockStructure == containingLoop)
                  {
                  isLoopBackEdge = true;
                  break;
                  }

               prevBlockStructure = prevBlockStructure->getParent();
               }

            if (isLoopBackEdge)
               {
               nextEdge = next;
               continue;
               }


            if (isLoopHdr)
               {
               // Disable partial peeling of loops by hoisting.
               // Gives rise to strage loop layout with part/all of the iteration
               // peeled outside.  No convincing reason to keep hoisting out of
               // loops enabled.
               //
               nextEdge = next;
               continue;
               }


            // Do not hoist this block if the predecessor is only remaining predecessor
            // as this will result in structure repair (because this block would be
            // removed from structure/CFG after this hoisting) that could create improper regions.
            // In any case if this is the last predecessor, then this block should be moved
            // will block extension/trees cleansing to be adjacent to the predecessor (no
            // need for hoisting)
            //
            if (block->getPredecessors().size() == 1)
               {
               nextEdge = next;
               continue;
               };

            TR::TreeTop *tt = prevBlock->getLastRealTreeTop();

            ListIterator<TR::Block> newBlocksIt(&newBlocks);
            TR::Block *nextNewBlock = NULL;
            bool canHoist = true;
            for (nextNewBlock = newBlocksIt.getCurrent(); nextNewBlock; nextNewBlock = newBlocksIt.getNext())
               {
               if ((nextNewBlock == prevBlock) || (nextNewBlock == block))
                  {
                  canHoist = false;
                  break;
                  }
               }


            // We can only perform the hoisting if the previous block
            // ends in a goto to the return block or falls through to the
            // return block.
            //
            if (!tt->getNode()->getOpCode().isJumpWithMultipleTargets() && canHoist)
               {
               bool synergyExists = false;
               bool needToLimitCodeGrowth = true;

               /*
               if (usingEdgeFreqs)
                  {
                  if (nextEdge->getHotness(cfg) >= hot)
                     needToLimitCodeGrowth = false;
                  }
               else if ((block->getHotness(cfg) >= warm) && (prevBlock->getHotness(cfg) >= hot))
                  {
                  //printf("Hoisting without limits in : %s\n", comp()->signature());
                  needToLimitCodeGrowth = false;
                  }
               */

               if ((block->getFrequency() >= MAX_WARM_BLOCK_COUNT) && (prevBlock->getFrequency() >= MAX_HOT_BLOCK_COUNT))
                  {
                  //printf("Hoisting without limits in : %s\n", comp()->signature());
                  needToLimitCodeGrowth = false;
                  }

               if (node1->getOpCode().isReturn())
                  synergyExists = true;
               else
                  {
                  TR::TreeTop *tt2 = tt;
                  if (tt->getNode()->getOpCodeValue() == TR::Goto)
                     tt2 = tt->getPrevRealTreeTop();

                  TR::TreeTop *prevBlockEntry = prevBlock->getEntry();
                  if ((prevBlockEntry == tt2) ||
                      ((numColdPreds > 0) && (numNonColdPreds > 0)))
                     synergyExists = true;
                  else
                     synergyExists = hasSynergy(prevBlock, node1);

                  if (synergyExists)
                     {
                     TR::Block *nextBlock = block->getNextBlock();
                     if (nextBlock && !nextBlock->isCold() && block->hasSuccessor(nextBlock) && (nextBlock->getPredecessors().size() == 1))
                        {
                        TR::Node *lastNode = nextBlock->getLastRealTreeTop()->getNode();
                        if (lastNode->getOpCode().isIf())
                           {
                           if (hasSynergy(block, lastNode))
                              synergyExists = false;
                           }
                        }
                     }
                  }

               int32_t numTrees = 0;
               if (synergyExists && needToLimitCodeGrowth)
                  {
                  TR::TreeTop *countedTree = block->getEntry()->getNextRealTreeTop();
                  while (countedTree != exitTreeTop)
                     {
                     numTrees++;
                     if (numTrees > 10)
                        {
                        synergyExists = false;
                        break;
                        }
                     countedTree = countedTree->getNextRealTreeTop();
                     }
                  }

               if (synergyExists &&
                   performTransformation(comp(), "%sHoist basic block [%d] into predecessor block [%d]\n", optDetailString(), block->getNumber(), prevBlock->getNumber()))
                  {
                    //debugCounter++;
                    //if (debugCounter > 4)
                    // return 1;

                      //printf("%sHoist basic block [%d] into predecessor block [%d]\n", optDetailString(), block->getNumber(), prevBlock->getNumber());
                  if (prevBlock->getSuccessors().size() > 1)
                     {
                     TR::Block *splitBlock = prevBlock->splitEdge(prevBlock, block, comp());
                     dumpOptDetails(comp(), "Aggressive hoisting by creating a new block_%d\n", splitBlock->getNumber());
                     newBlocks.add(splitBlock);

                     for (auto nextSuccEdge = block->getExceptionSuccessors().begin(); nextSuccEdge != block->getExceptionSuccessors().end(); ++nextSuccEdge)
                        {
                        TR::CFGNode *succBlock = (*nextSuccEdge)->getTo();
                        cfg->addExceptionEdge(splitBlock, succBlock);
                        }

                     int32_t splitFrequency = (*nextEdge)->getFrequency();
                     if (splitFrequency < 0)
                        splitFrequency = block->getFrequency();
                     //dumpOptDetails(comp(), "Split block_%d has freq %d\n", splitBlock->getNumber(), splitFrequency);

                     if (block->getFrequency() < prevBlock->getFrequency())
                        splitBlock->setFrequency(block->getFrequency());
                     else
                        splitBlock->setFrequency(prevBlock->getFrequency());

                     //splitBlock->setFrequency(splitFrequency);
                     if (splitFrequency >= 0)
                        splitBlock->getPredecessors().front()->setFrequency(splitFrequency);
                     tt = splitBlock->getLastRealTreeTop();
                     prevBlock = splitBlock;
                     current = splitBlock->getSuccessors().front();
                     if (splitFrequency >= 0)
                        current->setFrequency(splitFrequency);
                     }

                  //if (numTrees > 5)
                  // printf("Hoisting block with %d trees in %s\n", numTrees, comp()->signature());

                  TR::Block *extraGotoBlock = NULL;
                  TR::Block *fallThroughSuccBlock = NULL;
                  TR::TreeTop *fallThroughTree = block->getExit()->getNextTreeTop();

                  if (!node1->getOpCode().isReturn() &&
                      (node1->getOpCodeValue() != TR::Goto))
                     {
                     if (fallThroughTree)
                        fallThroughSuccBlock = fallThroughTree->getNode()->getBlock();
                     }

                  TR::TreeTop *originalNextTree = tt->getNextTreeTop();
                  TR::TreeTop *newPrevTree = tt;
                     {
                     TR::TreeTop *currentTree = block->getEntry()->getNextTreeTop();
                     TR_ScratchList<TR::Node> seenNodes(trMemory()), duplicateNodes(trMemory());
                     vcount_t visitCount = comp()->incVisitCount();
                     TR::TreeTop *newCurrentTree = NULL;
                     while (!(currentTree == exitTreeTop))
                        {
                           {
                           TR::Node *currentNode = currentTree->getNode();

                           TR::Node *newCurrentNode = TR::Node::copy(currentNode);
                           currentNode->setVisitCount(visitCount);
                           duplicateNodes.add(newCurrentNode);
                           seenNodes.add(currentNode);

                           for (int32_t childNum=0;childNum < currentNode->getNumChildren(); childNum++)
                              {
                              TR::Node *duplicateChildNode = duplicateExact(currentNode->getChild(childNum), &seenNodes, &duplicateNodes, comp());
                              newCurrentNode->setChild(childNum, duplicateChildNode);
                              }

                           newCurrentTree = TR::TreeTop::create(comp(), newCurrentNode, NULL, NULL);
                           newCurrentTree->join(originalNextTree);
                           newPrevTree->join(newCurrentTree);
                           }

                        currentTree = currentTree->getNextTreeTop();
                        if (newCurrentTree)
                           newPrevTree = newCurrentTree;
                        }

                     if (fallThroughSuccBlock)
                        {
                        comp()->getFlowGraph()->setStructure(NULL);
                        extraGotoBlock = TR::Block::createEmptyBlock(node1, comp(), fallThroughTree->getNode()->getBlock()->getFrequency(), fallThroughTree->getNode()->getBlock());
                        newBlocks.add(extraGotoBlock);
                        TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(node1, TR::Goto, 0, fallThroughTree));
                        extraGotoBlock->getEntry()->join(gotoTreeTop);
                        gotoTreeTop->join(extraGotoBlock->getExit());
                        TR::TreeTop *origNextTree = prevBlock->getExit()->getNextTreeTop();
                        prevBlock->getExit()->join(extraGotoBlock->getEntry());
                        extraGotoBlock->getExit()->join(origNextTree);
                        //TR_RegionStructure *parent = prevBlock->getParentStructureIfExists(comp()->getFlowGraph());
                                  TR_RegionStructure *parent = prevBlock->getCommonParentStructureIfExists(fallThroughTree->getNode()->getBlock(), comp()->getFlowGraph());
                        cfg->addNode(extraGotoBlock, parent);
                        }
                     }

                  newPrevTree->getNode()->setReferenceCount(0);

                  if (tt->getNode()->getOpCode().isBranch())
                     {
                     TR::TransformUtil::removeTree(comp(), tt);
                     }

                  bool needToRemoveEdge = true;
                  blocksWereHoisted = true;

                  requestOpt(OMR::treeSimplification, true, prevBlock);
                  requestOpt(OMR::localCSE, true, prevBlock);

                  for (auto nextSuccEdge = block->getSuccessors().begin(); nextSuccEdge != block->getSuccessors().end(); ++nextSuccEdge)
                     {
                     TR::CFGNode *succBlock = (*nextSuccEdge)->getTo();
                     if (extraGotoBlock && (succBlock == fallThroughSuccBlock))
                        {
                        //dumpOptDetails(comp(), "Added edge from %d to %d\n", prevBlock->getNumber(), extraGotoBlock->getNumber());
                        cfg->addEdge(prevBlock, extraGotoBlock);
                        //dumpOptDetails(comp(), "Added edge from %d to %d\n\n", extraGotoBlock->getNumber(), succBlock->getNumber());
                        cfg->addEdge(extraGotoBlock, succBlock);
                        int32_t extraGotoFrequency = (*nextSuccEdge)->getFrequency();
                        if (extraGotoFrequency < 0)
                           extraGotoFrequency = (*nextSuccEdge)->getTo()->getFrequency();
                        extraGotoBlock->setFrequency(extraGotoFrequency);
                        if (extraGotoFrequency >= 0)
                           {
                           extraGotoBlock->getPredecessors().front()->setFrequency(extraGotoFrequency);
                           extraGotoBlock->getSuccessors().front()->setFrequency(extraGotoFrequency);
                           }
                        }
                     else
                        {
                        //dumpOptDetails(comp(), "Added edge from %d to %d\n\n", prevBlock->getNumber(), succBlock->getNumber());
                        // We don't want to try to add another edge between prevBlock and
                        // block as there already exists an edge; this will result in
                        // the edge being added to the CFG but NOT to the structure (which
                        // checks for duplicate edges before adding them). Then removing the
                        // edge from prevBlock->block will fail after structure repair as there
                        // would be one edge in the CFG left, but NONE in the structure subgraph.
                        //
                        if (block != succBlock)
                           cfg->addEdge(TR::CFGEdge::createEdge(prevBlock,  succBlock, trMemory()));
                        else
                           needToRemoveEdge = false;
                        }
                     }
                  if (needToRemoveEdge)
                     cfg->removeEdge(current);
                  }
               }
            nextEdge = next;
            }
         }
      }

   return 0; // actual cost
   }


static void markNodesUsedInIndirectAccesses(TR::Node *node, bool examineChildren, TR::Compilation * comp)
   {
   node->setIsNotRematerializeable();
   //dumpOptDetails(comp, "Setting node %p as not rematerializable\n", node);
   //if (!node->isRematerializeable())
   //   dumpOptDetails(comp, "2Setting node %p as not rematerializable\n", node);
   //else
   //   dumpOptDetails(comp, "ERROR Setting node %p as not rematerializable\n", node);

   if (examineChildren && (node->getNumChildren() > 0))
      {
      TR::Node *child = node->getFirstChild();
      if (child->getOpCode().isArrayRef())
         node = child;

      int32_t i;
      for (i = 0; i < node->getNumChildren(); ++i)
         markNodesUsedInIndirectAccesses(node->getChild(i), false, comp);
      }
   }


static void initializeFutureUseCounts(TR::Node *node, TR::Node *parent, vcount_t visitCount, TR::Compilation * comp, int32_t *heightArray)
   {
   //if (node->getOpCode().isLoadVarDirect())
   static const char *regPress1 = feGetEnv("TR_IgnoreRegPressure");

   if (parent)
      {
      if (NULL != regPress1)
         {
         if ((parent->getNumChildren() == 2) && !parent->getOpCode().isCall())
            {
            if (parent->getSecondChild()->getOpCode().isLoadConst())
               node->setIsNotRematerializeable();
            }

         if (parent->getOpCode().isStore() || parent->getOpCode().isCall())
            node->setIsNotRematerializeable();
         }
      }

   if (visitCount == node->getVisitCount())
      return;

   node->setVisitCount(visitCount);
   node->setFutureUseCount(node->getReferenceCount());

   bool indirectAccess = false;
   if ((node->getOpCode().hasSymbolReference() &&
        node->getOpCode().isIndirect()) ||
       (node->getOpCode().isArrayLength()))
      indirectAccess = true;

   int32_t height = 0;

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      initializeFutureUseCounts(node->getChild(i), node, visitCount, comp, heightArray);

      if (heightArray)
         {
         if (heightArray[node->getChild(i)->getGlobalIndex()]+1 > height)
            height = heightArray[node->getChild(i)->getGlobalIndex()]+1;
         }

      // only mark the first child (i.e the pointer child) of an indirect access
      // as Not Rematerializeable
      //
      if (NULL != regPress1)
         {
         if ((indirectAccess) && (i==0))
            markNodesUsedInIndirectAccesses(node->getChild(i), true, comp);
         }
      }

   if (heightArray)
      heightArray[node->getGlobalIndex()] = height;
   }


// Tries to reduce TR::NULLCHK nodes having TR::PassThrough as first child.
//
TR_CompactNullChecks::TR_CompactNullChecks(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}



int32_t TR_CompactNullChecks::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_CompactNullChecks::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }



int32_t TR_CompactNullChecks::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   comp()->incVisitCount();
   TR::TreeTop *treeTop;
   TR::TreeTop *exitTreeTop;
   int32_t symRefCount = comp()->getMaxAliasIndex();
   TR_BitVector writtenSymbols(symRefCount, trMemory(), stackAlloc);
   // Process each block in treetop order
   //
   for (treeTop = startTree; (treeTop != endTree); treeTop = exitTreeTop->getNextTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Local Opts, expected BBStart treetop");
      TR::Block *block              = node->getBlock();
      compactNullChecks(block, &writtenSymbols);
      exitTreeTop                  = block->getEntry()->getExtendedBlockExitTreeTop();
      }

   return 1; // actual cost
   }



void TR_CompactNullChecks::compactNullChecks(TR::Block *block, TR_BitVector *writtenSymbols)
   {
   TR::TreeTop *prevTree = block->getEntry();
   TR::TreeTop *currentTree = block->getFirstRealTreeTop();
   TR::TreeTop *exitTree = block->getExit();
   TR::TreeTop *stopTree = block->getEntry()->getExtendedBlockExitTreeTop();
   vcount_t initialVisitCount = comp()->incVisitCount();
   while (currentTree != stopTree)
      {
      TR::Node *prevNode = prevTree->getNode();
      if (prevNode->getOpCodeValue() == TR::BBStart)
         {
         block = prevNode->getBlock();
         exitTree = block->getExit();
         TR::Block *blockItr = block->getNextBlock();
         while (blockItr && blockItr->isExtensionOfPreviousBlock())
            {
            if (blockItr->getPrevBlock()->getSuccessors().size() != 1)
               break;
            exitTree = blockItr->getExit();
            blockItr = blockItr->getNextBlock();
            }
         }
      if (block->isOSRInduceBlock() || block->isOSRCatchBlock() || block->isOSRCodeBlock())
         return;

      //
      // Mark calls and stores that have already been evaluated with this
      // initial visit count so that we do not 'see' the calls when we
      // walk the trees trying to find an indirect access to use to compact
      // a null check
      //
      if ((prevNode->getOpCodeValue() == TR::NULLCHK) &&
          (prevNode->getFirstChild()->getOpCodeValue() == TR::PassThrough))
          {
          TR::Node *referenceChild = prevNode->getNullCheckReference();

          if (referenceChild == prevNode->getFirstChild()->getFirstChild())
             {
             vcount_t visitCount = comp()->incVisitCount();
             bool isTreeTopNode = false;
             TR::TreeTop *cursorTreeTop = currentTree;
             bool replacedPassThroughNode = false;
             _isNextTree = true;
             writtenSymbols->empty();
             while (!replacedPassThroughNode)
                {
                TR::Node *cursorNode = cursorTreeTop->getNode();
                replacedPassThroughNode = replacePassThroughIfPossible(cursorNode, referenceChild, prevNode, NULL, &isTreeTopNode, writtenSymbols, visitCount, initialVisitCount, cursorTreeTop);

                if (replacedPassThroughNode &&
                    (cursorNode->getOpCodeValue() == TR::NULLCHK) &&
                    (cursorNode->getNullCheckReference() == prevNode->getNullCheckReference()))
                   TR::Node::recreate(cursorNode, TR::treetop);

                // set flag to remove tree if treetop contains vcall and ivcall
                if (replacedPassThroughNode &&
                    cursorNode->getOpCodeValue() == TR::treetop &&
                    cursorNode->getFirstChild() == prevNode->getFirstChild() &&
                    (prevNode->getFirstChild()->getDataType() == TR::NoType))
                   isTreeTopNode = true;

                _isNextTree = false;
                if (cursorTreeTop == exitTree)
                   {
                   isTreeTopNode = false;
                   break;
                   }
                cursorTreeTop = cursorTreeTop->getNextRealTreeTop();
                }

             if (replacedPassThroughNode)
                requestOpt(OMR::deadTreesElimination, true, block);

             if (isTreeTopNode)
                {
                prevTree->join(currentTree->getNextTreeTop());
                if (prevNode->getFirstChild()->getReferenceCount() > 1)
                   prevNode->getFirstChild()->recursivelyDecReferenceCount();
                }
             }
          }
      else if (prevNode->getOpCodeValue() == TR::checkcast)
         {
         TR::Node *objectRef = prevNode->getFirstChild();
         vcount_t visitCount = comp()->incVisitCount();
         TR::TreeTop *cursorTreeTop = currentTree;
         _isNextTree = true;
         writtenSymbols->empty();
         bool replacedNullCheck = false;
         bool compactionDone = false;
         while (cursorTreeTop != exitTree && cursorTreeTop->getNode()->getOpCodeValue() != TR::BBEnd)
            {
            TR::Node *cursorNode = cursorTreeTop->getNode();
            replacedNullCheck = replaceNullCheckIfPossible(cursorNode, objectRef, prevNode,
                                                            NULL, writtenSymbols, visitCount,
                                                            initialVisitCount, compactionDone);
            if (!replacedNullCheck)
               {
               if (cursorNode->getOpCode().isStore() &&
                   cursorNode->getSymbolReference()->getSymbol()->isAutoOrParm() &&
                   cursorNode->getSymbolReference()->getUseonlyAliases().isZero(comp()))
                  {
                  cursorTreeTop = cursorTreeTop->getNextRealTreeTop();
                  continue;
                  }

               _isNextTree = false;
               break;
               }

            _isNextTree = false;
            cursorTreeTop = cursorTreeTop->getNextRealTreeTop();
            }
         if (replacedNullCheck)
            requestOpt(OMR::deadTreesElimination, true, block);
         }
      TR::TransformUtil::recursivelySetNodeVisitCount(prevNode, initialVisitCount);
      prevTree = currentTree;
      currentTree = currentTree->getNextRealTreeTop();
      }
   }



bool TR_CompactNullChecks::replacePassThroughIfPossible(TR::Node *currentNode, TR::Node *referenceChild, TR::Node *prevNode, TR::Node *currentParent, bool *isTreeTopNode, TR_BitVector *writtenSymbols, vcount_t visitCount, vcount_t initialVisitCount, TR::TreeTop *currentTree)
   {
   if (currentNode->getVisitCount() == visitCount)
      return false;

   if (currentNode->getVisitCount() == initialVisitCount)
      return false;

   currentNode->setVisitCount(visitCount);

   if (currentNode->isNopableInlineGuard())
      return false;

   int32_t i = 0;
   for (i = 0; i < currentNode->getNumChildren(); ++i)
      {
      TR::Node *child = currentNode->getChild(i);
      if (replacePassThroughIfPossible(child, referenceChild, prevNode, currentNode, isTreeTopNode, writtenSymbols, visitCount, initialVisitCount, currentTree))
         return true;


      if (!currentNode->mayKill().isZero(comp()))
         {
         TR::SparseBitVector useDefAliases(comp()->allocator());
         currentNode->mayKill().getAliases(useDefAliases);
         *writtenSymbols |= useDefAliases;
         }

      TR::ILOpCode &opCode = currentNode->getOpCode();
      if (opCode.isLikeDef() && opCode.hasSymbolReference())
         writtenSymbols->set(currentNode->getSymbolReference()->getReferenceNumber());

      bool childEquivalent = false;
      if (child == referenceChild)
         childEquivalent = true;
      else
         {
         if (referenceChild->getOpCode().isLoadVarDirect() && child->getOpCode().isLoadVarDirect())
            {
            if (((referenceChild->getSymbol() == child->getSymbol()) &&
                 (referenceChild->getSymbolReference()->getOffset() == child->getSymbolReference()->getOffset())) &&
                 (_isNextTree || !writtenSymbols->get(child->getSymbolReference()->getReferenceNumber())))
               childEquivalent = true;
            }
         }

      TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

      if (!(opCode.isArrayLength() && comp()->cg()->getDisableNullCheckOfArrayLength()) &&
          childEquivalent &&
          (opCode.isIndirect() ||
           (opCode.isArrayLength()) ||
           opCode.isCall() ||
           (opCodeValue == TR::monent) ||
           (opCodeValue == TR::monexit)) &&
          ((!opCode.isCall() && (i==0)) ||
           (opCode.isCall() && !opCode.isIndirect() && (i==0)) ||
           (opCode.isCallIndirect() && (i==1))) &&
          (currentParent == NULL || !currentParent->getOpCode().isResolveCheck())
          )
         {
         bool canCompact = false;
         if (_isNextTree ||
            (opCode.isArrayLength()) ||
            (opCode.isLoadVar() && !writtenSymbols->get(currentNode->getSymbolReference()->getReferenceNumber())))
            canCompact = true;

         // compacting null check into computed calls will result in the null check not being performed before dispatching into the
         // call target if there is no need for dereferencing the reference child for the dispatch
         if (opCode.isCallIndirect() && opCode.hasSymbolReference() && currentNode->getSymbol()->castToMethodSymbol()->isComputed())
            canCompact = false;

         if (canCompact &&
            performTransformation(comp(), "%sCompact null check %p with node %p in next tree\n", optDetailString(), prevNode, currentNode))
            {

            if (opCode.isTreeTop())
               {
               if (!(comp()->useAnchors() && currentTree->getNode()->getOpCode().isAnchor()))
                  (*isTreeTopNode) = true;
               }

            prevNode->getFirstChild()->recursivelyDecReferenceCount();
            prevNode->setAndIncChild(0, currentNode);
            if (child->getOpCodeValue() != TR::loadaddr) //For loadaddr, IsNonNull is always true
               child->setIsNonNull(false); // it is no longer known that the reference is non null

            return true;
            }
         }
      }

   return false;
   }


static bool nodeCanRaiseException(TR::Node *node)
   {

   if (node->getOpCodeValue() == TR::New ||
         node->getOpCodeValue() == TR::newarray ||
         node->getOpCodeValue() == TR::anewarray ||
         node->getOpCodeValue() == TR::multianewarray)
      return false;
   else if (node->getOpCode().canRaiseException())
      return true;
   return false;
   }

bool TR_CompactNullChecks::replaceNullCheckIfPossible(TR::Node *cursorNode, TR::Node *objectRef,
                                                         TR::Node *prevNode, TR::Node *currentParent,
                                                         TR_BitVector *writtenSymbols, vcount_t visitCount,
                                                         vcount_t initialVisitCount, bool &compactionDone)
   {
   if (cursorNode->getVisitCount() == visitCount)
      return true;

   if (cursorNode->getVisitCount() == initialVisitCount)
      return true;

   cursorNode->setVisitCount(visitCount);
   if (cursorNode->getOpCodeValue() == TR::NULLCHK)
      {
      TR::Node *reference = cursorNode->getNullCheckReference();
      bool isEquivalent = false;
      if (reference == objectRef)
         isEquivalent = true;
      else
         {
         if (reference->getOpCode().isLoadVarDirect() && objectRef->getOpCode().isLoadVarDirect())
            {
            if ((reference->getSymbol() == objectRef->getSymbol() &&
                     reference->getSymbolReference()->getOffset() == objectRef->getSymbolReference()->getOffset()) &&
                  (_isNextTree || !writtenSymbols->get(reference->getSymbolReference()->getReferenceNumber())))
               isEquivalent = true;
            }
         }
      ///dumpOptDetails(comp(), "reference [%p] objectRef [%p] isEquivalent %d\n", reference, objectRef, isEquivalent);
      if (isEquivalent)
         {
         bool canBeRemoved = true; //comp()->cg()->canNullChkBeImplicit(cursorNode);
         if (comp()->getOption(TR_DisableTraps) ||
             TR::Compiler->om.offsetOfObjectVftField() >= comp()->cg()->getNumberBytesReadInaccessible())
           canBeRemoved = false;


         if (canBeRemoved &&
               performTransformation(comp(), "%sCompacting checkcast [%p] and null check [%p]\n", optDetailString(),
                                          prevNode, cursorNode))
            {
            ///printf("\n---found opportunity for checkcastAndNULLCHK in %s---\n", comp()->signature());
            ///fflush(stdout);
            TR::Node::recreate(cursorNode, TR::treetop);
            if (cursorNode->getFirstChild()->getOpCodeValue() == TR::PassThrough)
               {
               TR::Node *nullChkRef = cursorNode->getFirstChild()->getFirstChild();
               cursorNode->getFirstChild()->recursivelyDecReferenceCount();
               cursorNode->setAndIncChild(0, nullChkRef);
               }

            // copy the bytecode info into the cast class;
            // this is used to setup the exception point
            //
            // compactionDone is used to detect the case when
            // a checkcast is followed by 2 null-checks on the
            // same object in the same block. in this case,
            // we compact both null-checks but copy the bytecodeinfo
            // of only the first one, since the NPE should be thrown
            // at the first NULLCHK if the object is null
            //
            // checkcast
            //    aload #1
            //    loadaddr
            // ...
            // NULLCHK #1
            //    aloadi
            //      aload #1
            // ..
            // NULLCHK #2
            //    iloadi
            //      aload #1
            if (!compactionDone)
               {
               TR::Node::recreate(prevNode, TR::checkcastAndNULLCHK);
               compactionDone = true;
               TR_Pair<TR_ByteCodeInfo, TR::Node> *bcInfo = new (trHeapMemory()) TR_Pair<TR_ByteCodeInfo, TR::Node> (&prevNode->getByteCodeInfo(), cursorNode);
               comp()->getCheckcastNullChkInfo().push_front(bcInfo);
               }
            return true;
            }
         }
      else
         // this is a non-equivalent null-check
         // so quit
         return false;
      }
   else if (nodeCanRaiseException(cursorNode))
      return false;
   else
      {
      int32_t i = 0;
      for (i = 0; i < cursorNode->getNumChildren(); i++)
         {
         TR::Node *child = cursorNode->getChild(i);
         // this call collects all the information needed
         if (!replaceNullCheckIfPossible(child, objectRef, prevNode, cursorNode,
                                          writtenSymbols, visitCount, initialVisitCount, compactionDone))
            return false;

         if (!cursorNode->mayKill().isZero(comp()))
            {
            TR::SparseBitVector useDefAliases(comp()->allocator());
            cursorNode->mayKill().getAliases(useDefAliases);
            *writtenSymbols |= useDefAliases;
            }

         TR::ILOpCode &opCode = cursorNode->getOpCode();
         if (opCode.isLikeDef() && opCode.hasSymbolReference())
            writtenSymbols->set(cursorNode->getSymbolReference()->getReferenceNumber());
         }
      }
   return true;
   }

const char *
TR_CompactNullChecks::optDetailString() const throw()
   {
   return "O^O COMPACT NULL CHECKS: ";
   }

static int32_t compareValues(TR::Node *node1, TR::Node *node2)
   {
   if (node1->getOpCode().isLoadConst() &&
       node2->getOpCode().isLoadConst())
      {
      int32_t value1 = node1->getInt();
      int32_t value2 = node2->getInt();

      if (value1 >= 0 &&
          value2 >= 0)
        {
        if (value2 > value1)
          return 1;
        else if (value2 == value1)
          return 0;
        else
          return -1;
        }
      }

   return -2;
   }


TR_ArraysetStoreElimination::TR_ArraysetStoreElimination(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}



int32_t TR_ArraysetStoreElimination::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_ArraysetStoreElimination::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }



int32_t TR_ArraysetStoreElimination::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   if (comp()->getOption(TR_DisableArraysetStoreElimination))
     return 0;

   comp()->incVisitCount();
   TR::TreeTop *treeTop;
   TR::TreeTop *exitTreeTop;

   int32_t symRefCount = comp()->getMaxAliasIndex();
   TR_BitVector readAndWrittenSymbols(symRefCount, trMemory(), stackAlloc);
   TR_BitVector symbolsWrittenByArrayset(symRefCount, trMemory(), stackAlloc);
   TR_BitVector scratch(symRefCount, trMemory(), stackAlloc);

   // Process each block in treetop order
   //
   for (treeTop = startTree; (treeTop != endTree); treeTop = exitTreeTop->getNextTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Local Opts, expected BBStart treetop");
      TR::Block *block              = node->getBlock();
      reduceArraysetStores(block, &readAndWrittenSymbols, &symbolsWrittenByArrayset, &scratch);
      exitTreeTop                  = block->getEntry()->getExtendedBlockExitTreeTop();
      }

   return 1; // actual cost
   }


void TR_ArraysetStoreElimination::reduceArraysetStores(TR::Block *block, TR_BitVector *readAndWrittenSymbols, TR_BitVector *symbolsWrittenByArrayset, TR_BitVector *scratch)
   {
   TR::TreeTop *prevTree = block->getEntry();
   TR::TreeTop *currentTree = block->getFirstRealTreeTop();
   TR::TreeTop *exitTree = block->getExit();
   TR::TreeTop *stopTree = block->getEntry()->getExtendedBlockExitTreeTop();
   vcount_t initialVisitCount = comp()->incVisitCount();
   while (currentTree != stopTree)
      {
      TR::Node *prevNode = prevTree->getNode();
      if (prevNode->getOpCodeValue() == TR::BBStart)
         {
         block = prevNode->getBlock();
         exitTree = block->getExit();
         }

      TR::TransformUtil::recursivelySetNodeVisitCount(prevNode, initialVisitCount);

      if ((prevNode->getNumChildren() > 0) &&
          (prevNode->getFirstChild()->getOpCodeValue() == TR::newarray))
          {
          TR::Node *arraysetNode = NULL;
          TR::TreeTop *arraysetTree = NULL;
          while (currentTree != exitTree)
             {
             TR::Node *currentNode = currentTree->getNode();
             if ((currentNode->getNumChildren() > 0) &&
                 (currentNode->getFirstChild()->getOpCodeValue() == TR::arrayset))
                {
                arraysetTree = currentTree;
                arraysetNode = currentNode->getFirstChild();
                break;
                }

             if (!currentNode->getOpCode().isStore() || !currentNode->getSymbol()->isAuto() ||
                 !currentNode->getSymbolReference()->getUseonlyAliases().isZero(comp()))
                break;

             TR::TransformUtil::recursivelySetNodeVisitCount(currentNode, initialVisitCount);

             currentTree = currentTree->getNextTreeTop();
             }

         if (arraysetNode && (arraysetNode->getOpCodeValue() == TR::arrayset) &&
             arraysetNode->getFirstChild()->getOpCode().isArrayRef() &&
             (arraysetNode->getFirstChild()->getFirstChild() == prevNode->getFirstChild()))
             {
             TR::Node *addressChild = arraysetNode->getFirstChild();

             //vcount_t visitCount = comp()->incVisitCount();
             bool optimizedArraysetNode = false;
             readAndWrittenSymbols->empty();
             while (!optimizedArraysetNode)
                {
                TR::Node *cursorNode = currentTree->getNode();
                optimizedArraysetNode = optimizeArraysetIfPossible(cursorNode, addressChild, arraysetTree, NULL, readAndWrittenSymbols, symbolsWrittenByArrayset, scratch, initialVisitCount, currentTree);

                if (currentTree == exitTree)
                   break;

                if ((cursorNode->getNumChildren() > 0) &&
                    (cursorNode->getFirstChild()->getOpCodeValue() == TR::arraycopy))
                  break;

                currentTree = currentTree->getNextRealTreeTop();
                }
             }
         }
      else
         {
         if ((prevNode->getNumChildren() > 0) &&
             (prevNode->getFirstChild()->getOpCodeValue() == TR::arrayset) &&
              prevNode->getFirstChild()->getFirstChild()->getOpCode().isArrayRef())
            {
            readAndWrittenSymbols->empty();
            TR::Node *cursorNode = currentTree->getNode();
            optimizeArraysetIfPossible(cursorNode, prevNode->getFirstChild()->getFirstChild(), prevTree, NULL, readAndWrittenSymbols, symbolsWrittenByArrayset, scratch, initialVisitCount, currentTree);
            }
         }

      prevTree = currentTree;
      if (currentTree == stopTree)
        break;
      currentTree = currentTree->getNextRealTreeTop();
      }
   }





bool TR_ArraysetStoreElimination::optimizeArraysetIfPossible(TR::Node *currentNode, TR::Node *addressChild, TR::TreeTop *arraysetTree, TR::Node *currentParent, TR_BitVector *readAndWrittenSymbols, TR_BitVector *symbolsWrittenByArrayset, TR_BitVector *scratch, vcount_t initialVisitCount, TR::TreeTop *currentTree)
   {
   if (currentNode->getVisitCount() == initialVisitCount)
      return false;

   currentNode->setVisitCount(initialVisitCount);

   TR::Node *arraysetNode = arraysetTree->getNode()->getFirstChild();

   if (currentNode->getOpCodeValue() == TR::arraycopy)
      {
      TR::Node *arraycopyDestAddressNode;
      TR::Node *arraycopyLenNode;
      int32_t numChildren = currentNode->getNumChildren();
      if (numChildren == 3)
         {
         arraycopyDestAddressNode = currentNode->getSecondChild();
         arraycopyLenNode = currentNode->getChild(2);
         }
      else
         {
         arraycopyDestAddressNode = currentNode->getChild(2);
         arraycopyLenNode = currentNode->getChild(4);
         }

      if ((arraycopyDestAddressNode == addressChild) ||
          (arraycopyDestAddressNode->getOpCode().isArrayRef() &&
           addressChild->getOpCode().isArrayRef() &&
           (arraycopyDestAddressNode->getFirstChild() == addressChild->getFirstChild()) &&
           (arraycopyDestAddressNode->getSecondChild() == addressChild->getSecondChild())))
         {
         *scratch = *symbolsWrittenByArrayset;
         *scratch &= *readAndWrittenSymbols;

         if (scratch->isEmpty() && performTransformation(comp(), "%sEliminating some stores done by arrayset node %p because stores done by a following arraycopy node %p will overwrite them\n", optDetailString(), arraysetNode, currentNode))
            {
            TR::Node *arraysetLenNode = arraysetNode->getChild(2);
            TR::Node *subMaxNode;
            TR::Node *newAddressChild;
            if (arraysetLenNode->getType().isInt32())
               {
               subMaxNode = TR::Node::create(TR::isub, 2,
                                                     arraysetLenNode,
                                                     arraycopyLenNode);
               subMaxNode = TR::Node::create(TR::imax, 2,
                                                     subMaxNode,
                                                     TR::Node::iconst(subMaxNode, 0));

               newAddressChild = TR::Node::create(TR::aiadd, 2,
                                                     addressChild,
                                                     arraycopyLenNode);

               }
            else // if (arraysetLenNode->getType().isInt64())
               {
               subMaxNode = TR::Node::create(TR::lsub, 2,
                                                     arraysetLenNode,
                                                     arraycopyLenNode);
               subMaxNode = TR::Node::create(TR::lmax, 2,
                                                     subMaxNode,
                                                     TR::Node::lconst(subMaxNode, 0));

               newAddressChild = TR::Node::create(TR::aladd, 2,
                                                     addressChild,
                                                     arraycopyLenNode);
               }

            arraysetNode->setAndIncChild(0, newAddressChild);
            arraysetNode->setAndIncChild(2, subMaxNode);

            TR::TreeTop *prevTree = arraysetTree->getPrevTreeTop();
            TR::TreeTop *nextTree = arraysetTree->getNextTreeTop();
            prevTree->join(nextTree);

            prevTree = currentTree->getPrevTreeTop();
            prevTree->join(arraysetTree);
            arraysetTree->join(currentTree);

            addressChild->recursivelyDecReferenceCount();
            arraysetLenNode->recursivelyDecReferenceCount();
            return true;
            }
         }

      return false;
      }

   if (currentNode->canGCandReturn()) // cannot have an uninitialized array be held live across a GC point
      return false;

   if (!currentNode->mayKill().isZero(comp()))
      {
      TR::SparseBitVector useDefAliases(comp()->allocator());
      currentNode->mayKill().getAliases(useDefAliases);
      *readAndWrittenSymbols |= useDefAliases;
      }

   if (!currentNode->mayUse().isZero(comp()))
      {
      TR::SparseBitVector useDefAliases(comp()->allocator());
      currentNode->mayUse().getAliases(useDefAliases);
      *readAndWrittenSymbols |= useDefAliases;
      }

   TR::ILOpCode &opCode = currentNode->getOpCode();
   if (opCode.isLikeDef() && opCode.hasSymbolReference())
      readAndWrittenSymbols->set(currentNode->getSymbolReference()->getReferenceNumber());

   if (opCode.isLikeUse() && opCode.hasSymbolReference())
      readAndWrittenSymbols->set(currentNode->getSymbolReference()->getReferenceNumber());


   int32_t i = 0;
   for (i = 0; i < currentNode->getNumChildren(); ++i)
      {
      TR::Node *child = currentNode->getChild(i);
      if (optimizeArraysetIfPossible(child, addressChild, arraysetTree, currentNode, readAndWrittenSymbols, symbolsWrittenByArrayset, scratch, initialVisitCount, currentTree))
         return true;
      }

   return false;
   }

const char *
TR_ArraysetStoreElimination::optDetailString() const throw()
   {
   return "O^O ARRAYSET STORE ELIMINATION: ";
   }

// Tries to simplify ands that are created by loop versioning;
// basically tries avoid doing the same test twice within an and
// condition.
//
TR_SimplifyAnds::TR_SimplifyAnds(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}


int32_t TR_SimplifyAnds::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_SimplifyAnds::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
   return 0;
   }

bool isPowerOfTwo(TR::Compilation *comp, TR::Node *node)
   {
   if (node->getOpCode().isLoadConst() &&
       isNonNegativePowerOf2(node->get64bitIntegralValue()))
      return true;

   return false;
   }

static bool isAndOfTwoFlags(TR::Compilation *comp, TR::Node *andNode, TR::Node *lastRealNode,
                               TR::ILOpCodes ifOpCode, TR::ILOpCodes andOpCode)
   {
   // Returns true for this pattern:
   //
   // ificmpeq
   //    iand
   //      iload a
   //      <power of 2>
   //    iconst 0
   // ificmpeq
   //    iand
   //      iload a
   //      <power of 2>
   //    iconst 0

   return
      (andNode->getOpCodeValue() == ifOpCode &&
       lastRealNode->getOpCodeValue() == ifOpCode &&
       andNode->getBranchDestination() == lastRealNode->getBranchDestination() &&
       andNode->getFirstChild()->getOpCodeValue() == andOpCode &&
       lastRealNode->getFirstChild()->getOpCodeValue() == andOpCode &&
       andNode->getSecondChild()->getOpCode().isLoadConst() &&
       lastRealNode->getSecondChild()->getOpCode().isLoadConst() &&
       andNode->getSecondChild()->get64bitIntegralValue() == 0 &&
       lastRealNode->getSecondChild()->get64bitIntegralValue() == 0 &&
       andNode->getFirstChild()->getFirstChild() ==
       lastRealNode->getFirstChild()->getFirstChild() &&
       isPowerOfTwo(comp, andNode->getFirstChild()->getSecondChild()) &&
       isPowerOfTwo(comp, lastRealNode->getFirstChild()->getSecondChild()));
   }


int32_t TR_SimplifyAnds::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   comp()->incVisitCount();
   TR::TreeTop *treeTop = NULL;
   TR::TreeTop *prevBoundCheckTree = NULL;
   TR_ScratchList<TR::Node> seenAndNodes(trMemory());
   TR::Block *block = NULL;
   bool noSideEffectsInBetween = false;

   // Process each block in treetop order
   //
   for (treeTop = startTree; (treeTop != endTree); treeTop = treeTop->getNextTreeTop())
      {
      // Get information about this block
      //
      TR::Node *lastRealNode = treeTop->getNode();
      if (lastRealNode->getOpCodeValue() == TR::BBStart)
        block = lastRealNode->getBlock();

      if (lastRealNode->getOpCodeValue() == TR::treetop)
          lastRealNode = lastRealNode->getFirstChild();

      if (lastRealNode->exceptionsRaised() ||
          lastRealNode->getOpCode().isStore() ||
          lastRealNode->getOpCode().isCall())
         noSideEffectsInBetween = false;


      //TR::Node *lastRealNode = lastRealTree->getNode();

      if ((lastRealNode->getOpCode().isBranch() &&
           (lastRealNode->getOpCodeValue() != TR::Goto) && (!lastRealNode->getOpCode().isCompBranchOnly())) &&
            !lastRealNode->isNopableInlineGuard())
         {
         TR::TreeTop *lastRealTree = treeTop;
         ListElement<TR::Node> *seenAndNode = seenAndNodes.getListHead();
         bool lastSeenAnd = true;
         bool seenAndBefore = false;
         bool needGoto = false;
         TR::ILOpCodes newOrOpcode = TR::BadILOp;
         TR::Node *andNode = NULL;

         while(seenAndNode)
            {
            andNode = seenAndNode->getData();
            TR::ILOpCodes andOpCodeValue = andNode->getOpCodeValue();
            TR::ILOpCodes lastRealOpCodeValue = lastRealNode->getOpCodeValue();

            if (trace())
               traceMsg(comp(), "Comparing current AND node %p with old AND node\n", lastRealNode, andNode);
            if ((andNode->getFirstChild() == lastRealNode->getFirstChild()) &&
                (andNode->getSecondChild() == lastRealNode->getSecondChild()))
               {
               if (andOpCodeValue == lastRealOpCodeValue)
                   {
                     //if (andOpCodeValue != lastRealOpCodeValue)
                     //printf("detected new chance in %s\n", comp()->signature());

                   seenAndBefore = true;
                   break;
                   }
               else if (andNode->getOpCode().getOpCodeForReverseBranch() == lastRealNode->getOpCodeValue())
                   {
                     //if (andNode->getOpCode().getOpCodeForReverseBranch() != lastRealOpCodeValue)
                     //printf("detected new chance in %s\n", comp()->signature());

                   seenAndBefore = true;
                   needGoto = true;
                   break;
                   }
               }
            else if ((andNode->getFirstChild() == lastRealNode->getFirstChild()) &&
                     andNode->getSecondChild()->getOpCode().isLoadConst() &&
                     lastRealNode->getSecondChild()->getOpCode().isLoadConst() &&
                     (lastRealNode->getSecondChild()->getType().isInt32()) &&
                     lastRealNode->getOpCode().isBooleanCompare())  // this code was allowing unsigned types in -- I made this test stronger // NOTE that isCompare is only true for signed compares -- this is very ugly though
               {
               int32_t oldValue = andNode->getSecondChild()->getInt();
               int32_t newValue = lastRealNode->getSecondChild()->getInt();

               if ((andOpCodeValue == TR::ificmplt &&
                    ((lastRealOpCodeValue == TR::ificmple &&
                      (oldValue >= (newValue+1) && (newValue != TR::getMaxSigned<TR::Int32>()))) ||
                     ((lastRealOpCodeValue == TR::ificmplt) &&
                      (oldValue >= newValue)))) ||
                   (andOpCodeValue == TR::ificmple &&
                    ((lastRealOpCodeValue == TR::ificmplt &&
                      (oldValue >= (newValue-1) && (newValue != TR::getMinSigned<TR::Int32>()))) ||
                     (lastRealOpCodeValue == TR::ificmple &&
                      (oldValue >= newValue)))) ||
                   (andOpCodeValue == TR::ificmpgt &&
                    ((lastRealOpCodeValue == TR::ificmpge &&
                      (newValue >= (oldValue+1) && (oldValue != TR::getMaxSigned<TR::Int32>()))) ||
                     (lastRealOpCodeValue == TR::ificmpgt &&
                      (newValue >= oldValue)))) ||
                   (andOpCodeValue == TR::ificmpge &&
                    ((lastRealOpCodeValue == TR::ificmpgt &&
                      (newValue >= (oldValue-1) && (oldValue != TR::getMinSigned<TR::Int32>()))) ||
                     (lastRealOpCodeValue == TR::ificmpge &&
                      (newValue >= oldValue)))))
                  {
                    //printf("Old value %d New value %d in %s\n", oldValue, newValue, comp()->signature());
                    //printf("detected new chance in %s\n", comp()->signature());
                  seenAndBefore = true;
                  break;
                  }
               else if ((andOpCodeValue == TR::ificmplt &&
                         ((lastRealOpCodeValue == TR::ificmpgt &&
                           (oldValue >= (newValue+1) && (newValue != TR::getMaxSigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ificmpge &&
                           (oldValue >= newValue)))) ||
                        (andOpCodeValue == TR::ificmple &&
                         ((lastRealOpCodeValue == TR::ificmpge &&
                           (oldValue >= (newValue-1) && (newValue != TR::getMinSigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ificmpgt &&
                           (oldValue >= newValue)))) ||
                        (andOpCodeValue == TR::ificmpgt &&
                         ((lastRealOpCodeValue == TR::ificmplt &&
                           (newValue >= (oldValue+1) && (oldValue != TR::getMaxSigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ificmple &&
                           (newValue >= oldValue)))) ||
                        (andOpCodeValue == TR::ificmpge &&
                         ((lastRealOpCodeValue == TR::ificmple &&
                           (newValue >= (oldValue-1) && (oldValue != TR::getMinSigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ificmplt &&
                           (newValue >= oldValue)))))
                  {
                    //printf("detected new chance in %s\n", comp()->signature());
                  seenAndBefore = true;
                  needGoto = true;
                  break;
                  }
               }

            else if ((andNode->getFirstChild() == lastRealNode->getFirstChild()) &&
                     andNode->getSecondChild()->getOpCode().isLoadConst() &&
                     lastRealNode->getSecondChild()->getOpCode().isLoadConst() &&
                     (lastRealNode->getSecondChild()->getType().isInt32()) &&
                     lastRealNode->getOpCode().isUnsignedCompare())
               {
               uint32_t oldValue = andNode->getSecondChild()->getUnsignedInt();
               uint32_t newValue = lastRealNode->getSecondChild()->getUnsignedInt();

               if ((andOpCodeValue == TR::ifiucmplt &&
                    ((lastRealOpCodeValue == TR::ifiucmple &&
                      (oldValue >= (newValue+1) && (newValue != TR::getMaxUnsigned<TR::Int32>()))) ||
                     ((lastRealOpCodeValue == TR::ifiucmplt) &&
                      (oldValue >= newValue)))) ||
                   (andOpCodeValue == TR::ifiucmple &&
                    ((lastRealOpCodeValue == TR::ifiucmplt &&
                      (oldValue >= (newValue-1) && (newValue != TR::getMinUnsigned<TR::Int32>()))) ||
                     (lastRealOpCodeValue == TR::ifiucmple &&
                      (oldValue >= newValue)))) ||
                   (andOpCodeValue == TR::ifiucmpgt &&
                    ((lastRealOpCodeValue == TR::ifiucmpge &&
                      (newValue >= (oldValue+1) && (oldValue != TR::getMaxUnsigned<TR::Int32>()))) ||
                     (lastRealOpCodeValue == TR::ifiucmpgt &&
                      (newValue >= oldValue)))) ||
                   (andOpCodeValue == TR::ifiucmpge &&
                    ((lastRealOpCodeValue == TR::ifiucmpgt &&
                      (newValue >= (oldValue-1) && (oldValue != TR::getMinUnsigned<TR::Int32>()))) ||
                     (lastRealOpCodeValue == TR::ifiucmpge &&
                      (newValue >= oldValue)))))
                  {
                    //printf("Old value %d New value %d in %s\n", oldValue, newValue, comp()->signature());
                    //printf("detected new chance in %s\n", comp()->signature());
                  seenAndBefore = true;
                  break;
                  }
               else if ((andOpCodeValue == TR::ifiucmplt &&
                         ((lastRealOpCodeValue == TR::ifiucmpgt &&
                           (oldValue >= (newValue+1) && (newValue != TR::getMaxUnsigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ifiucmpge &&
                           (oldValue >= newValue)))) ||
                        (andOpCodeValue == TR::ifiucmple &&
                         ((lastRealOpCodeValue == TR::ifiucmpge &&
                           (oldValue >= (newValue-1) && (newValue != TR::getMinUnsigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ifiucmpgt &&
                           (oldValue >= newValue)))) ||
                        (andOpCodeValue == TR::ifiucmpgt &&
                         ((lastRealOpCodeValue == TR::ifiucmplt &&
                           (newValue >= (oldValue+1) && (oldValue != TR::getMaxUnsigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ifiucmple &&
                           (newValue >= oldValue)))) ||
                        (andOpCodeValue == TR::ifiucmpge &&
                         ((lastRealOpCodeValue == TR::ifiucmple &&
                           (newValue >= (oldValue-1) && (oldValue != TR::getMinUnsigned<TR::Int32>()))) ||
                          (lastRealOpCodeValue == TR::ifiucmplt &&
                           (newValue >= oldValue)))))
                  {
                    //printf("detected new chance in %s\n", comp()->signature());
                  seenAndBefore = true;
                  needGoto = true;
                  break;
                  }
               }

            // (a&b && a&c) ==> ((a & b|c) == b|c)
            else if (lastSeenAnd && noSideEffectsInBetween &&
                     andNode->getFirstChild()->getReferenceCount() == 1 &&
                     (isAndOfTwoFlags(comp(), andNode, lastRealNode, TR::ificmpeq,  TR::iand) ||
                      isAndOfTwoFlags(comp(), andNode, lastRealNode, TR::iflcmpeq,  TR::land)))
               {
               if (trace())
                   traceMsg(comp(), "Found two iand nodes: %p %p\n", andNode, lastRealNode);
               TR::DataType trType = andNode->getFirstChild()->getType();
               newOrOpcode = trType.isInt32() ? TR::ior : TR::lor;
               seenAndBefore = true;
               break;
               }

            seenAndNode = seenAndNode->getNextElement();
            lastSeenAnd = false;
            }

         if (!seenAndBefore)
            {
            //
            // NOTE : this portion of code is really a special case peephole optimization
            // suggested by Julian wherein we replace given IL pattern by another
            // IL pattern which is expected to be more efficient from a codegen point
            // of view. Note that this pattern is seen in Compressor.compress() and might
            // result in a slowdown in compress if not performed. Note this is only
            // done on platforms where branches are a real bottleneck; on some PPC
            // architectures, we should not do this.
            //
            bool removingConditional = false;
            if (cg()->branchesAreExpensive() &&
                (lastRealNode->getOpCodeValue() == TR::ificmpge  ||
                 lastRealNode->getOpCodeValue() == TR::ifiucmpge ||
                 lastRealNode->getOpCodeValue() == TR::iflcmpge) &&
                (lastRealNode->getSecondChild()->getOpCodeValue() == TR::iconst ||
                 lastRealNode->getSecondChild()->getOpCodeValue() == TR::lconst) &&
                (lastRealNode->getSecondChild()->get64bitIntegralValue() == 0))
               {
               TR::Block *nextBlock = block->getExit()->getNextTreeTop()->getNode()->getBlock();
               TR::TreeTop *nextToNextBlockEntry = nextBlock->getExit()->getNextTreeTop();

               if (lastRealNode->getBranchDestination() == nextToNextBlockEntry)
                  {
                  TR::TreeTop *asyncTree = NULL;
                  TR::TreeTop *firstTree = nextBlock->getFirstRealTreeTop();
                  if (firstTree->getNode()->getOpCodeValue() == TR::asynccheck)
                     {
                     asyncTree = firstTree;
                     firstTree = firstTree->getNextRealTreeTop();
                     }
                  TR::TreeTop *lastTree = nextBlock->getLastRealTreeTop();
                  if (lastTree->getNode()->getOpCodeValue() == TR::asynccheck)
                     {
                     asyncTree = lastTree;
                     lastTree = lastTree->getPrevRealTreeTop();
                     }

                  if ((firstTree == lastTree) &&
                      (nextBlock->getPredecessors().size() == 1))
                     {
                     TR::TreeTop *treeBefore = lastRealTree->getPrevRealTreeTop();
                     if (treeBefore->getNode()->getOpCodeValue() == TR::asynccheck)
                        {
                        asyncTree = treeBefore;
                        treeBefore = treeBefore->getPrevRealTreeTop();
                        }

                     TR::Node *treeBeforeLastRealNode = treeBefore->getNode();
                     TR::Node *firstNode = firstTree->getNode();
                     if (firstNode->getOpCode().isStoreDirect() &&
                         ((firstNode->getFirstChild()->getOpCode().isAdd() ||
                          firstNode->getFirstChild()->getOpCode().isSub()) &&
                          firstNode->getFirstChild()->getReferenceCount() == 1) &&
                         ((firstNode->getType().isInt32() ||
                          firstNode->getType().isInt64())
                         ))
                        {
                        bool areTreesInRequiredForm = false;

                        if (firstNode->getFirstChild()->getFirstChild()->getOpCode().isLoadVar() &&
                            (firstNode->getFirstChild()->getFirstChild()->getReferenceCount() == 1) && // in actual fact the ref count can be > 1 as long as this is the first reference to this load (we could check that in future if reqd)
                            (firstNode->getFirstChild()->getFirstChild()->getSymbolReference()->getReferenceNumber() == firstNode->getSymbolReference()->getReferenceNumber()))
                           areTreesInRequiredForm = true;
                        else if (treeBeforeLastRealNode->getOpCode().isStoreDirect() &&
                                 (firstNode->getFirstChild()->getFirstChild() == treeBeforeLastRealNode->getFirstChild()) &&
                                 (treeBeforeLastRealNode->getSymbolReference()->getReferenceNumber() == firstNode->getSymbolReference()->getReferenceNumber()))
                            areTreesInRequiredForm = true;

                        if (areTreesInRequiredForm &&
                              performTransformation(comp(), "%sSimplifying branch %p and store %p into conditional\n", optDetailString(), lastRealNode, firstNode))
                            {
                            if (asyncTree)
                                asyncTree->getPrevTreeTop()->join(asyncTree->getNextTreeTop());
                            removingConditional = true;
                            TR::Node *secondGrandChild = firstNode->getFirstChild()->getSecondChild();

                            TR::Node *lastFirstChild = lastRealNode->getFirstChild();

                            // Shift the lastFirstChild right by (size-1) to generate 0x0 or 0xFF..FF
                            TR::Node *newIShrNode = TR::Node::create((lastFirstChild->getType().isInt32())?TR::ishr:TR::lshr, 2, lastFirstChild,
                                                                   TR::Node::create(lastRealNode, TR::iconst, 0, (lastFirstChild->getType().isInt32())?31:63));

                            // Sign-extend / Truncate if necessary.
                            if (firstNode->getType().isInt32() &&
                                !lastFirstChild->getType().isInt32())
                               newIShrNode = TR::Node::create(TR::l2i, 1, newIShrNode);
                            else if (!firstNode->getType().isInt32() &&
                                     lastFirstChild->getType().isInt32())
                               newIShrNode = TR::Node::create(TR::i2l, 1, newIShrNode);

                            // And with original value to generate value to add/sub.
                            TR::Node *newIAndNode = TR::Node::create(firstNode->getType().isInt32() ? TR::iand: TR::land, 2, secondGrandChild, newIShrNode);

                            secondGrandChild->decReferenceCount();
                            firstNode->getFirstChild()->setAndIncChild(1, newIAndNode);
                            TR::TreeTop *prevTree = lastRealTree->getPrevRealTreeTop();
                            prevTree->join(firstTree);
                            firstTree->join(lastRealTree);
                            if (asyncTree)
                               {
                               asyncTree->join(prevTree->getNextTreeTop());
                               prevTree->join(asyncTree);
                               }

                            nextBlock->getEntry()->join(nextBlock->getExit());

                            optimizer()->prepareForTreeRemoval(lastRealTree);
                            TR::TransformUtil::removeTree(comp(), lastRealTree);

                            TR::CFGEdge* currentSucc = block->getSuccessors().front();
                            TR::CFGNode *succBlock = currentSucc->getTo();
                            if (succBlock != nextToNextBlockEntry->getNode()->getBlock())
                               {
                               currentSucc = *(++block->getSuccessors().begin());
                               succBlock = currentSucc->getTo();
                               }
                            comp()->getFlowGraph()->removeEdge(currentSucc);
                            }
                        }
                     }
                  }
               }

            // NOTE: End of special case peephole optimization
            //
            if (!removingConditional)
               {
               if (trace())
                  traceMsg(comp(), "Adding new AND node %p\n", lastRealNode);
               seenAndNodes.add(lastRealNode);
               noSideEffectsInBetween = true;
               }
            }
         else if (performTransformation(comp(), "%sRemoving conditional node %p (condition has been checked before in this extended BB)\n", optDetailString(), lastRealNode))
            {
            TR::TreeTop *previousTree = block->getLastRealTreeTop()->getPrevTreeTop();
            TR::CFGEdge* currentSucc = block->getSuccessors().front();
            TR::CFGNode *succBlock = currentSucc->getTo();
            if (newOrOpcode != TR::BadILOp)
               {
               // Modify first ificmpeq
               TR::Node::recreate(andNode, andNode->getOpCode().getOpCodeForReverseBranch());
               TR::Node *orNode = TR::Node::create(newOrOpcode, 2,
                                                    lastRealNode->getFirstChild()->getSecondChild(),
                                                    andNode->getFirstChild()->getSecondChild());
               andNode->getFirstChild()->getSecondChild()->recursivelyDecReferenceCount();
               andNode->getFirstChild()->setAndIncChild(1, orNode);

               andNode->getSecondChild()->recursivelyDecReferenceCount();
               andNode->setAndIncChild(1, orNode);
               }

            if (needGoto)
               {
               TR::Node *gotoNode = TR::Node::create(lastRealNode, TR::Goto);
               TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
               //block->getEntry()->join(gotoTree);
               previousTree->join(gotoTree);
               gotoTree->join(block->getExit());
               gotoNode->setBranchDestination(lastRealNode->getBranchDestination());

               if (succBlock != block->getExit()->getNextTreeTop()->getNode()->getBlock())
                  {
                  currentSucc = *(++block->getSuccessors().begin());
                  succBlock = currentSucc->getTo();
                  }
               comp()->getFlowGraph()->removeEdge(currentSucc);
               }
            else
               {
               //block->getEntry()->join(block->getExit());
               previousTree->join(block->getExit());

               if (succBlock == block->getExit()->getNextTreeTop()->getNode()->getBlock())
                  {
                  currentSucc = *(++block->getSuccessors().begin());
                  succBlock = currentSucc->getTo();
                  }
               comp()->getFlowGraph()->removeEdge(currentSucc);
               }
            lastRealNode->getFirstChild()->recursivelyDecReferenceCount();
            lastRealNode->getSecondChild()->recursivelyDecReferenceCount();
            }
         }
      else if (lastRealNode->getOpCode().isBndCheck())
         {
         TR::Node *prevNode = NULL;
         if (prevBoundCheckTree)
            prevNode = prevBoundCheckTree->getNode();

         if (prevNode &&
             prevNode->getOpCodeValue() == lastRealNode->getOpCodeValue())
            {
            // The first BNDCHKxxx can subsume the second if it checks against the larger
            // of the two indices.
            //
            if (lastRealNode->getOpCode().isSpineCheck())
               {
               TR_ASSERT(lastRealNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK, "unexpected spine check node");

               // TODO: this opt doesn't work for spine checks because the unique
               // indexes on both BNDCHKxxx nodes need to be maintained.  There
               // might be some simpler cases to optimize, however.
               //
               if (0 &&
                   (prevNode->getChild(2) == lastRealNode->getChild(2)) &&
                   (compareValues(prevNode->getChild(3), lastRealNode->getChild(3)) > 0) &&
                   performTransformation(comp(), "%sRemoving BNDCHK from BNDCHKwithSpineCHK lastRealNode %p, prevNode=%p\n", optDetailString(), lastRealNode, prevNode))
                  {
                  optimizer()->prepareForNodeRemoval(prevNode->getChild(3));
                  prevNode->getChild(3)->recursivelyDecReferenceCount();
                  prevNode->setChild(3, lastRealNode->getChild(3));

                  // The current BNDCHKwithSpineCHK node now becomes a spine check.
                  //
                  TR::Node::recreate(lastRealNode, TR::SpineCHK);

                  // The index is the third child of a spine check.
                  //
                  lastRealNode->setAndIncChild(2, lastRealNode->getChild(3));
                  lastRealNode->setNumChildren(3);
                  }
               else
                  {
                  prevBoundCheckTree = treeTop;
                  }
               }
            else
               {
               if ((prevNode->getFirstChild() == lastRealNode->getFirstChild()) &&
                   (compareValues(prevNode->getSecondChild(), lastRealNode->getSecondChild()) > 0))
                  {
                  //printf("Found a chance in %s\n", comp()->signature());
                  //dumpOptDetails(comp(), "Found a chance with node %p in %s\n", prevNode, comp()->signature());
                  optimizer()->prepareForNodeRemoval(prevNode->getSecondChild());
                  prevNode->getSecondChild()->recursivelyDecReferenceCount();
                  prevNode->setChild(1, lastRealNode->getSecondChild());
                  TR::Node::recreate(lastRealNode, TR::treetop);
                  lastRealNode->setNumChildren(1);
                  }
               else
                  prevBoundCheckTree = treeTop;
               }
            }
         else
            prevBoundCheckTree = treeTop;
         }
      else if ((lastRealNode->getOpCodeValue() == TR::BBStart) ||
                lastRealNode->exceptionsRaised() ||
               (lastRealNode->getOpCode().isStore() &&
                (lastRealNode->getSymbolReference()->getSymbol()->isShadow() ||
                 lastRealNode->getSymbolReference()->getUseonlyAliases().hasAliases() ||
                 (lastRealNode->getSymbolReference()->getSymbol()->isStatic()))))
         prevBoundCheckTree = NULL;
      }

   return 1; // actual cost
   }

const char *
TR_SimplifyAnds::optDetailString() const throw()
   {
   return "O^O AND SIMPLIFICATION: ";
   }


// Basically if there is a branch that has a block B as its target; and block B
// only has a goto in it redirecting control flow to some other block C, then
// this method will remove this needless goto block and make the original
// branch's target block C.
//
TR_EliminateRedundantGotos::TR_EliminateRedundantGotos(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

void TR_EliminateRedundantGotos::renumberInAncestors(TR_Structure *str, int32_t num)
   {
   TR_RegionStructure *parent = str->getParent()->asRegion();
   TR_ASSERT(parent, "assertion failure");

   TR_StructureSubGraphNode *nodeInParent = parent->findSubNodeInRegion(str->getNumber());
   if (parent->getNumber() == str->getNumber())
      {
      renumberInAncestors(parent, num);
      parent->setNumber(num); // don't use parent->renumber, since that recurses to children
      }

   // renumber the exit edges in the predecessor regions
   //
   TR_PredecessorIterator pit(nodeInParent);
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
     {
     TR_StructureSubGraphNode *predNode = toStructureSubGraphNode(edge->getFrom());
     TR_RegionStructure *region = predNode->getStructure()->asRegion();
     if (region)
       {
       renumberExitEdges(region, nodeInParent->getNumber(), num);
       }
     }

   nodeInParent->setNumber(num);
   }

void TR_EliminateRedundantGotos::renumberExitEdges(TR_RegionStructure *region, int32_t oldNum, int32_t newNum)
   {
   // Find the exit node in the region
   //
   TR_StructureSubGraphNode *node;
   ListIterator<TR::CFGEdge> it(&region->getExitEdges());
   TR::CFGEdge *edge;
   for (edge = it.getCurrent(); edge; edge = it.getNext())
      {
      if (edge->getTo()->getNumber() == oldNum)
         {
         node = toStructureSubGraphNode(edge->getTo());

         TR_ASSERT(node && node->getStructure() == 0, "expecting an exit region");

         // Fix all predecessors that need fixing
         //
         TR_PredecessorIterator pit(node);
         for (edge = pit.getFirst(); edge; edge = pit.getNext())
            {
            TR_StructureSubGraphNode *predNode = toStructureSubGraphNode(edge->getFrom());
            TR_RegionStructure *region =  predNode->getStructure()->asRegion();
            if (region)
               {
               renumberExitEdges(region, oldNum, newNum);
               }
            }
         node->setNumber(newNum);
         }
      }
   }

void TR_EliminateRedundantGotos::placeAsyncCheckBefore(TR::TreeTop *tt)
   {
   TR::TreeTop *actt =
      TR::TreeTop::create(comp(), TR::Node::createWithSymRef(tt->getNode(), TR::asynccheck, 0,
                                     getSymRefTab()->findOrCreateAsyncCheckSymbolRef
                                     (comp()->getMethodSymbol())));
   tt->getPrevTreeTop()->join(actt);
   actt->join(tt);
   }


int32_t TR_EliminateRedundantGotos::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_EliminateRedundantGotos::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }



int32_t TR_EliminateRedundantGotos::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   // The CFG must exist for this optimization.
   //
   TR::CFG *cfg = comp()->getFlowGraph();
   if (cfg == NULL)
      return false;

   if (comp()->getProfilingMode() == JitProfiling)
      return 0;

   bool gotosWereEliminated = false;


   for (TR::TreeTop *treeTop = startTree, *nextBlockStartTree = NULL;
        (treeTop != endTree);
        treeTop = nextBlockStartTree)
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Local Opts, expected BBStart treetop");
      TR::Block *block = node->getBlock();
      nextBlockStartTree = block->getExit()->getNextTreeTop();

      if (block->hasExceptionPredecessors())
         continue;

      //It's possible that all the exception predecessor edges of an OSR catch block have been
      //removed because the exception predecessor nodes have become empty and removed.
      //Even in that case, we don't want to remove the gotos at the end of such OSR blocks.
      if (block->isOSRCodeBlock())
         continue;

      if (block->getEntry() &&
          block->getEntry()->getNode()->getLabel())
         continue;

      // See if the only "real" node in this block is a goto or the block is empty.
      // At the same time see if there is an async check in the block.
      //

      TR::TreeTop * lastNonFenceTree = block->getExit()->getPrevTreeTop();
      while (lastNonFenceTree->getNode()->getOpCode().isExceptionRangeFence())
         lastNonFenceTree = lastNonFenceTree->getPrevTreeTop();

      bool emptyBlock = (lastNonFenceTree->getNode()->getOpCodeValue() == TR::BBStart);

      if (lastNonFenceTree->getNode()->getOpCodeValue() != TR::Goto &&
          !emptyBlock)
         continue;

      bool asyncMessagesFlag = false;
      TR::TreeTop *firstNonFenceTree = block->getFirstRealTreeTop();
      while (firstNonFenceTree->getNode()->getOpCodeValue() == TR::asynccheck)
         {
         asyncMessagesFlag = true;
         firstNonFenceTree = firstNonFenceTree->getNextRealTreeTop();
         }

      bool containsTreesOtherThanGoto = false;
      if (firstNonFenceTree != lastNonFenceTree &&
         !emptyBlock)
         {
         containsTreesOtherThanGoto = true;
         asyncMessagesFlag = false;

         //////dumpOptDetails(comp(), "Contains trees other than gotos when at block number : %d\n", block->getNumber());
         }

      bool blockIsRegionEntry = false;
      TR_Structure *rootStructure = cfg->getStructure();
      TR_RegionStructure *blockRegion = NULL;
      if (rootStructure != NULL)
         {
         blockRegion = block->getParentStructureIfExists(cfg);
         blockIsRegionEntry =
            blockRegion != NULL && blockRegion->getNumber() == block->getNumber();
         }

      bool manuallyFixStructure = blockIsRegionEntry;

      if (!block->isExtensionOfPreviousBlock())
         {
         // Check register dependencies
         TR::TreeTop *lastTT = emptyBlock ? block->getExit() : lastNonFenceTree;
         TR::Node *exit = lastTT->getNode();
         if (exit->getNumChildren() > 0)
            {
            TR::Node *exitDeps = exit->getChild(0);

            // If every child of the outgoing GlRegDeps is a register load,
            // then the outgoing registers agree with the incoming registers,
            // because each register load must appear first under BBStart.
            bool depsOK = true;
            for (int i = 0; i < exitDeps->getNumChildren(); i++)
               {
               if (!exitDeps->getChild(i)->getOpCode().isLoadReg())
                  {
                  depsOK = false;
                  break;
                  }
               }

            if (!depsOK)
               continue;
            }
         }

      // This block consists of just a goto with maybe an async check as well.
      //
      // Look at the predecessors and see if they can all absorb the goto
      //
      if (block->getPredecessors().empty())
         continue;

      if (block->isLoopInvariantBlock())
         continue;

      TR::Block *destBlock = block->getSuccessors().front()->getTo()->asBlock();
      if (destBlock == block)
         continue; // No point trying to "update" predecessors

      if (!containsTreesOtherThanGoto && !block->getExceptionSuccessors().empty())
         {
         // Structure repair doesn't deal well with exception successors in this case.
         if (!performTransformation(comp(), "%sRemoving exception successor edges for block_%d\n", optDetailString(), block->getNumber()))
          continue; // The exception successors remain, so skip this block

         TR::CFGEdgeList &excSuccs = block->getExceptionSuccessors();
         while (!excSuccs.empty())
            {
            if (trace())
               {
               TR::Block *from = excSuccs.front()->getFrom()->asBlock();
               TR::Block *to = excSuccs.front()->getTo()->asBlock();
               traceMsg(comp(), "Remove exception edge: block_%d (isValid %d) -> block_%d (isValid %d)\n", from->getNumber(), from->isValid(), to->getNumber(), to->isValid());
               }

            cfg->removeEdge(excSuccs.front());
            }
         }

      TR::CFGEdgeList fixablePreds(comp()->trMemory()->currentStackRegion());
      auto preds = block->getPredecessors();
      for (auto inEdge = preds.begin(); inEdge != preds.end(); ++inEdge)
         {
         if ((*inEdge)->getFrom() == cfg->getStart())
            continue;

         TR::Block *pred = toBlock((*inEdge)->getFrom());
         TR::TreeTop *tt = pred->getLastRealTreeTop();
         TR::Node *ttNode = tt->getNode();
         if(ttNode->getOpCodeValue() == TR::treetop)
             ttNode = ttNode->getFirstChild();

         if (tt->getNode()->getOpCode().isJumpWithMultipleTargets() ||
             tt->getNode()->isTheVirtualGuardForAGuardedInlinedCall())
            continue;

         if (!emptyBlock &&
             (!tt->getNode()->getOpCode().isBranch() ||
               tt->getNode()->getBranchDestination() != block->getEntry()))
            continue;

         if (tt->getNode()->getOpCode().isBranch()  &&
             ((tt->getNode()->getBranchDestination() != block->getEntry()) &&
              (block->isExtensionOfPreviousBlock() ||
               !(destBlock->getSuccessors().size() == 1) ||
               !(destBlock->getPredecessors().size() == 1) ||
               !destBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch())))
            continue;

         // PR 65144: The transformation logic later in this loop doesn't cope
         // with a branch whose target is also the fallthrough block.  Protect
         // against that case, whether it appears in the incoming trees, or
         // arises after an earlier transformation by this very loop.
         //
         if (  tt->getNode()->getOpCode().isBranch()
            && tt->getNode()->getBranchDestination() == block->getEntry()
            && pred->getNextBlock() == block)
            continue;

         //if (asyncMessagesFlag && (tt->getNode()->getOpCodeValue() != TR::Goto))
         //   {
         //   gotoBlockRemovable = false;
         //   break;
         //   }

         fixablePreds.push_front(*inEdge);
         }

      if (fixablePreds.empty())
         continue;

      if (fixablePreds.size() < preds.size())
         {
         // Not all predecessors can be adjusted, so block cannot be removed.
         if (emptyBlock || manuallyFixStructure)
            continue;
         }

      if (containsTreesOtherThanGoto)
         {
         if (block->getEntry()->getNode()->getNumChildren() > 0
             || destBlock->getEntry()->getNode()->getNumChildren() > 0)
            continue; // to move the trees we would need to deal with regdeps

         if (!(destBlock->getPredecessors().size() == 1))
            continue;

         if (destBlock->hasExceptionSuccessors() ||
             block->hasExceptionSuccessors())
            continue;

         TR::TreeTop *entryTree = destBlock->getEntry();
         TR::TreeTop *origNextTree = entryTree->getNextTreeTop();
         TR::TreeTop *firstTreeBeingMoved = block->getEntry()->getNextTreeTop();
         TR::TreeTop *lastTreeBeingMoved = lastNonFenceTree->getPrevTreeTop();

         destBlock->inheritBlockInfo(block, block->isCold());


         //////dumpOptDetails(comp(), "First tree being moved : %p Last tree being moved %p\n", firstTreeBeingMoved->getNode(), lastTreeBeingMoved->getNode());
         entryTree->join(firstTreeBeingMoved);
         lastTreeBeingMoved->join(origNextTree);
         block->getEntry()->join(lastNonFenceTree);
         //destBlock->setEntry(block->getEntry());
         //destBlock->setExit(block->getExit());
         }

      if (emptyBlock &&
          !performTransformation(comp(), "%sRemoving empty block_%d with BBStart %p\n", optDetailString(), block->getNumber(), block->getEntry()->getNode()))
          continue;
      else if (!emptyBlock &&
            !performTransformation(comp(), "%sEliminating goto at the end of block_%d with BBStart %p\n", optDetailString(), block->getNumber(), block->getEntry()->getNode()))
        continue;

      if (manuallyFixStructure)
         {
         // We are removing a goto block that is the head of a structure region.
         // Attempt to remove the goto block will result in the collapse of blockStrucutre
         // into the parent.  1) if the blockRegion is a loop region, we will end up
         // with an improper region 2) otherwise, we will end up with a bigger parent
         // region, which although is not draconian, but is still bad.
         //
         // To avoid that, repair structure manually
         //
         cfg->setStructure(0);

         TR_BlockStructure  *blockStructure  = block->getStructureOf();
         TR_StructureSubGraphNode *blockNode = blockRegion->findSubNodeInRegion(    block->getNumber());
         TR_StructureSubGraphNode * destNode = blockRegion->findSubNodeInRegion(destBlock->getNumber());

         bool cannotRepairStructure = false;
         for (auto edge = blockNode->getPredecessors().begin(); edge != blockNode->getPredecessors().end(); ++edge)
            {
            TR_StructureSubGraphNode *predNode =
               (*edge)->getFrom()->asStructureSubGraphNode();
            if (predNode->getStructure()->asRegion())
               {
               cannotRepairStructure = true;
               break;
               }
            }

         if (!destNode)
            cannotRepairStructure = true;

         if (!cannotRepairStructure)
            {
            renumberInAncestors(blockRegion, destBlock->getNumber());

            // Fixup the region of block, redirect all the structure backedges
            // to goto destNode instead (Don't try to fix CFG edges yet, they will be fixed
            // later)
            //
            for (auto edge = blockNode->getPredecessors().begin(); edge != blockNode->getPredecessors().end();)
               {
               TR_StructureSubGraphNode *predNode =
                  (*(edge++))->getFrom()->asStructureSubGraphNode();

               TR::CFGEdge::createEdge(predNode,  destNode, trMemory());
               blockRegion->removeEdge(predNode->getStructure(), blockStructure);
               }
            }

         // Fixup all the CFGEdges to goto destBlock
         //
         redirectPredecessors(block, destBlock, fixablePreds, emptyBlock, asyncMessagesFlag);

         if (!cannotRepairStructure)
            {
            blockRegion->renumber(destNode->getNumber());
            blockRegion->setEntry(destNode);
            blockRegion->removeEdge(blockStructure, destNode->getStructure());
            }

         cfg->removeEdge(block, destBlock);
         optimizer()->prepareForTreeRemoval(lastNonFenceTree);
         cfg->removeNode(block);

         if (!cannotRepairStructure)
            cfg->setStructure(rootStructure);
         }
      else
         {
         // Okay to allow automatic structure fixup (if it exists)
         //
         redirectPredecessors(block, destBlock, fixablePreds, emptyBlock, asyncMessagesFlag);
         }

      // Place an asynccheck as the first treetop of the successor, if there was one in the removed block
      // This is necessary as placing it in the predecessor may result in a seperation from its OSR guard
      //
      if (asyncMessagesFlag && comp()->getHCRMode() == TR::osr)
         placeAsyncCheckBefore(destBlock->getFirstRealTreeTop());

      if (emptyBlock &&
          !block->isExtensionOfPreviousBlock() &&
          destBlock->isExtensionOfPreviousBlock())
          destBlock->setIsExtensionOfPreviousBlock(false);
      }

   return 0; // actual cost
   }

void TR_EliminateRedundantGotos::redirectPredecessors(
   TR::Block *block,
   TR::Block *destBlock,
   const TR::CFGEdgeList &preds,
   bool emptyBlock,
   bool asyncMessagesFlag)
   {
   TR::CFG *cfg = comp()->getFlowGraph();

   TR::Node *regdepsToMove = NULL;
   TR::Node *newRegdepParent = NULL;

   // In case we're deleting an empty block at the beginning or end of a larger
   // extended block, take care to move any incoming regdeps forward, or
   // outgoing ones backward, to ensure they aren't lost.
   bool regdepsAreOutgoing = block->isExtensionOfPreviousBlock();
   if (regdepsAreOutgoing)
      {
      TR::Node *exitNode = block->getExit()->getNode();
      if (exitNode->getNumChildren() > 0)
         {
         // block is the last in its extended block. It must be empty because
         // it has only one predecessor, and the predecessor falls through into
         // block. Predecessor edges that don't branch to block are rejected
         // unless emptyBlock holds.
         TR_ASSERT_FATAL(
            emptyBlock,
            "expected block_%d to be empty\n",
            block->getNumber());

         regdepsToMove = exitNode->getChild(0);
         exitNode->setChild(0, NULL);
         exitNode->setNumChildren(0);
         newRegdepParent = toBlock(preds.front()->getFrom())->getExit()->getNode();
         }
      }
   else
      {
      TR::Node *entryNode = block->getEntry()->getNode();
      if (emptyBlock &&
          entryNode->getNumChildren() > 0 &&
          destBlock->isExtensionOfPreviousBlock())
         {
         regdepsToMove = entryNode->getChild(0);
         entryNode->setChild(0, NULL);
         entryNode->setNumChildren(0);
         newRegdepParent = destBlock->getEntry()->getNode();
         }
      }

   if (regdepsToMove != NULL)
      {
      TR_ASSERT_FATAL(
         newRegdepParent->getNumChildren() == 0,
         "n%un %s has unexpected register dependencies\n",
         newRegdepParent->getGlobalIndex(),
         newRegdepParent->getOpCode().getName());

      newRegdepParent->setNumChildren(1);
      newRegdepParent->setChild(0, regdepsToMove);
      }

   // Update predecessors now that the regdeps have been moved as appropriate
   for (auto edge = preds.begin(); edge != preds.end(); ++edge)
      {
      TR::CFGEdge* current = *edge;
      TR::Block *predBlock = toBlock(current->getFrom());
      requestOpt(OMR::treeSimplification, true, predBlock);

      if (asyncMessagesFlag && comp()->getHCRMode() != TR::osr)
         placeAsyncCheckBefore(predBlock->getLastRealTreeTop());

      TR::TreeTop *predExitTree = NULL;
      if (predBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch() &&
          predBlock->getLastRealTreeTop()->getNode()->getBranchDestination() == block->getEntry())
         {
         predBlock->changeBranchDestination(
            destBlock->getEntry(),
            cfg,
            /* callerFixesRegdeps = */ true);
         predExitTree = predBlock->getLastRealTreeTop();
         }
      else
         {
         predBlock->redirectFlowToNewDestination(comp(), current, destBlock, false);
         predExitTree = predBlock->getExit();
         }

      if (regdepsToMove == NULL && block->getEntry()->getNode()->getNumChildren() > 0)
         {
         fixPredecessorRegDeps(predExitTree->getNode(), destBlock);
         }
      else
         {
         TR::DebugCounter::incStaticDebugCounter(comp(),
            "redundantGotoElimination.regDeps/none");
         }

      if (predBlock->getNextBlock() == destBlock)
         {
         TR::Node *last = predBlock->getLastRealTreeTop()->getNode();
         if (last->getOpCodeValue() == TR::Goto)
            {
            TR::Node *exit = predBlock->getExit()->getNode();
            TR_ASSERT_FATAL(
               exit->getNumChildren() == 0,
               "n%un BBEnd has GlRegDeps even though it follows goto\n",
               exit->getGlobalIndex());

            if (last->getNumChildren() > 0)
               {
               TR_ASSERT_FATAL(
                  last->getNumChildren() == 1,
                  "n%un goto has %d children\n",
                  last->getGlobalIndex(),
                  last->getNumChildren());

               exit->setNumChildren(1);
               exit->setChild(0, last->getChild(0));
               last->setChild(0, NULL);
               last->setNumChildren(0);
               }

            TR::TreeTop *prev = predBlock->getLastRealTreeTop()->getPrevTreeTop();
            TR::TreeTop *next = predBlock->getLastRealTreeTop()->getNextTreeTop();
            prev->join(next);
            }
         }
      }
   }

void TR_EliminateRedundantGotos::fixPredecessorRegDeps(
   TR::Node *regdepsParent,
   TR::Block *destBlock)
   {
   const int childIndex = regdepsParent->getNumChildren() - 1;
   TR_ASSERT_FATAL(
      childIndex >= 0,
      "n%un should have at least one child "
      "because it leads to a block with incoming regdeps\n",
      regdepsParent->getGlobalIndex());

   TR::Node *regdeps = regdepsParent->getChild(childIndex);
   TR_ASSERT_FATAL(
      regdeps->getOpCodeValue() == TR::GlRegDeps,
      "expected n%un to be a GlRegDeps\n",
      regdeps->getGlobalIndex());

   TR::Node *destEntry = destBlock->getEntry()->getNode();
   if (destEntry->getNumChildren() == 0)
      {
      // No regdeps necessary
      TR::DebugCounter::incStaticDebugCounter(comp(),
         TR::DebugCounter::debugCounterName(comp(),
            "redundantGotoElimination.regDeps/wiped/%s/(%s)/block_%d",
            comp()->getHotnessName(comp()->getMethodHotness()),
            comp()->signature(),
            destBlock->getNumber()));
      regdeps->recursivelyDecReferenceCount();
      regdepsParent->setChild(childIndex, NULL);
      regdepsParent->setNumChildren(childIndex);
      return;
      }

   TR::Node *newReceivingRegdeps = destEntry->getChild(0);
   TR_ASSERT_FATAL(
      newReceivingRegdeps->getOpCodeValue() == TR::GlRegDeps,
      "expected n%un child of n%un BBStart <block_%d> to be GlRegDeps\n",
      newReceivingRegdeps->getGlobalIndex(),
      destEntry->getGlobalIndex(),
      destBlock->getNumber());

   if (regdeps->getNumChildren() == newReceivingRegdeps->getNumChildren())
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
         "redundantGotoElimination.regDeps/retained");
      }
   else
      {
      TR::DebugCounter::incStaticDebugCounter(comp(),
         TR::DebugCounter::debugCounterName(comp(),
            "redundantGotoElimination.regDeps/dropped/%s/(%s)/block_%d",
            comp()->getHotnessName(comp()->getMethodHotness()),
            comp()->signature(),
            destBlock->getNumber()));
      }

   int remainingDeps = 0;
   for (int i = 0; i < regdeps->getNumChildren(); i++)
      {
      TR::Node *dep = regdeps->getChild(i);
      auto reg = dep->getGlobalRegisterNumber();
      bool needed = false;
      for (int j = 0; j < newReceivingRegdeps->getNumChildren(); j++)
         {
         if (newReceivingRegdeps->getChild(j)->getGlobalRegisterNumber() == reg)
            {
            needed = true;
            break;
            }
         }

      if (needed)
         regdeps->setChild(remainingDeps++, dep);
      else
         dep->recursivelyDecReferenceCount();
      }

   TR_ASSERT_FATAL(
      remainingDeps == newReceivingRegdeps->getNumChildren(),
      "n%un: bad number %d of remaining regdeps\n",
      regdeps->getGlobalIndex(),
      remainingDeps);

   regdeps->setNumChildren(remainingDeps);
   }

const char *
TR_EliminateRedundantGotos::optDetailString() const throw()
   {
   return "O^O GOTO ELIMINATION: ";
   }

// If there is a block B with a goto at the end of the block that goes to the
// block C that follows it textually in the IL trees, eliminate the goto.
//
// If the block D that is the target of the goto (in block B) is the block that
// textually follows block C (which textually follows block B), then we can remove
// the trees for block C and place them at the end of the method's tree list
// as it's known that no block falls through to block C. We can then eliminate the
// goto.
//
TR_CleanseTrees::TR_CleanseTrees(TR::OptimizationManager *manager)
   : TR_BlockManipulator(manager)
   {}

int32_t TR_CleanseTrees::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_CleanseTrees::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }


void TR_CleanseTrees::prePerformOnBlocks()
   {
   // TR_Structure *rootStructure = NULL;
   //
   // Set up the block nesting depths
   //
   TR_Structure *rootStructure = comp()->getFlowGraph()->getStructure();
   if (rootStructure)
      {
      TR::CFGNode *node;
      for (node = comp()->getFlowGraph()->getFirstNode(); node; node = node->getNext())
         {
         TR::Block *block = toBlock(node);
         int32_t nestingDepth = 0;
         if (block->getStructureOf() != NULL)
            block->getStructureOf()->setNestingDepths(&nestingDepth);
         }
      }

   //markColdBlocks();
   }



int32_t TR_CleanseTrees::process(TR::TreeTop *startTree, TR::TreeTop *endTreeTop)
   {
   if (comp()->getProfilingMode() == JitProfiling)
      return 0;

   TR_Structure *rootStructure = comp()->getFlowGraph()->getStructure();
   comp()->incVisitCount();
   TR::TreeTop *treeTop;
   TR::TreeTop *exitTreeTop;
   TR::TreeTop *endTree = comp()->getMethodSymbol()->getLastTreeTop();
   bool cleansedTrees = false;

   // Process each block in treetop order
   //
   for (treeTop = startTree; (treeTop != endTreeTop); treeTop = exitTreeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Local Opts, expected BBStart treetop");
      TR::Block *block              = node->getBlock();
      exitTreeTop                  = block->getExit();

      if (block->getVisitCount() == comp()->getVisitCount())
         continue;

      block->setVisitCount(comp()->getVisitCount());

      TR::TreeTop *lastNonFenceTree = block->getLastRealTreeTop();

      node = lastNonFenceTree->getNode();
      //if (!node->getOpCode().isBranch()) // commented out since OrderBlocks is now the pass which will decide fall throughs in cases where the branch ends in an 'if'
      if (node->getOpCodeValue() != TR::Goto)
         continue;

      if ((node->getOpCodeValue() != TR::Goto) &&
         block->getNextBlock() &&
         (block->getNextBlock()->isExtensionOfPreviousBlock() ||
          node->isNopableInlineGuard()))
         continue;

      if (node->getBranchDestination() == treeTop)
         continue;

      TR::Block *block1;
      TR::Block *block2;
      TR::TreeTop *tt1 = exitTreeTop->getNextTreeTop();
      TR::TreeTop *tt2 = NULL;

      if (tt1)
         tt2 = tt1->getNode()->getBlock()->getExit()->getNextTreeTop();

      if (tt2 && node->getBranchDestination() == tt2)
         {

         block1 = tt1->getNode()->getBlock();
         block2 = tt2->getNode()->getBlock();

         tt1 = block1->getLastRealTreeTop();
         if ((tt1->getNode()->getOpCodeValue() == TR::Goto ||
              tt1->getNode()->getOpCode().isReturn() ||
              (tt1->getNode()->getOpCode().isTreeTop() &&
               tt1->getNode()->getNumChildren() > 0 &&
               tt1->getNode()->getFirstChild()->getOpCodeValue() == TR::athrow)) &&
             (/* (!rootStructure) || */ isBestChoiceForFallThrough(block, block2)) &&
             performTransformation(comp(), "%sMoving trees contained in block that was the fall through of block_%d\n", optDetailString(), block->getNumber()))
            {
            // FIXME: the following moves the tree at the end of the method, which is bad
            //
            // Can move the trees between the goto and the target
            // to the end of the method if reqd and eliminate the goto.
            //
            TR::TreeTop *startOfTreesBeingMoved = exitTreeTop->getNextTreeTop();
            TR::TreeTop *endOfTreesBeingMoved = tt2->getPrevTreeTop();
            exitTreeTop->join(tt2);
            endTree->join(startOfTreesBeingMoved);
            endTree = endOfTreesBeingMoved;
            endOfTreesBeingMoved->setNextTreeTop(NULL);

            requestOpt(OMR::treeSimplification, true, exitTreeTop->getNode()->getBlock());
                 //cleansedTrees = true;
            if (lastNonFenceTree->getNode()->getOpCodeValue() != TR::Goto)
               {
               lastNonFenceTree->getNode()->reverseBranch(block1->getEntry());
               }
            else
               {
               optimizer()->prepareForTreeRemoval(lastNonFenceTree);
               TR::TransformUtil::removeTree(comp(), lastNonFenceTree);
               }
            }
         }
      else
         {
         tt2 = node->getBranchDestination();
         block2 = tt2->getNode()->getBlock();

         TR::TreeTop *previousTree = tt2->getPrevTreeTop();

         if ((tt2 == comp()->getStartTree()) ||
             block->isCold() ||
             block2->isCold() ||
             (
              !isBestChoiceForFallThrough(block, tt2->getNode()->getBlock())))
             continue;

         int32_t result;
         //if (node->getOpCodeValue() == TR::Goto)
            {
            result = performChecksAndTreesMovement(tt2->getNode()->getBlock(), block, block->getNextBlock(), endTree, comp()->getVisitCount(), optimizer());
            }
            //else
            //{
            //result = performChecksAndTreesMovement(tt2->getNode()->getBlock(), block, block->getNextBlock(), endTree, comp()->getVisitCount(), optimizer());
            //}

         if (result >= 0)
            {
            cleansedTrees = true;
            requestOpt(OMR::treeSimplification, true, tt2->getNode()->getBlock());
            requestOpt(OMR::treeSimplification, true, block);

            if (result == 1)
               {
               if (previousTree && !previousTree->getNextTreeTop())
                  endTree = previousTree;
               else
                  endTree = comp()->getMethodSymbol()->getLastTreeTop();
               }
            }
         }
      }

   //if (cleansedTrees)
   //   requestOpt(treeSimplification);

   return 0; // actual cost
   }

const char *
TR_CleanseTrees::optDetailString() const throw()
   {
   return "O^O TREE CLEANSING: ";
   }

TR_ByteCodeInfo TR_ProfiledNodeVersioning::temporarilySetProfilingBcInfoOnNewArrayLengthChild(TR::Node *newArray, TR::Compilation *comp)
   {
   TR_ASSERT(newArray->getOpCodeValue() == TR::newarray || newArray->getOpCodeValue() == TR::anewarray, "assertion failure");

   TR::Node *numElementsNode = newArray->getFirstChild();

   // We use numElementsNode as the value to profile, but attach the resulting
   // info to newArray's bcInfo because numElementsNode might already be
   // profiled for other reasons.  We also add 1 to the bcIndex so that this
   // profiling doesn't collide with any other profiling anyone might be doing
   // on newArray, because we're not profiling what they would be expecting.
   // The newarray bytecodes are all more than 1 byte long, so this is safe.
   //
   TR_ByteCodeInfo originalBcInfo = numElementsNode ->getByteCodeInfo();
   TR_ByteCodeInfo profileBcInfo  = newArray        ->getByteCodeInfo();
   profileBcInfo.setByteCodeIndex(profileBcInfo.getByteCodeIndex()+1);
   numElementsNode->setByteCodeInfo(profileBcInfo);
   return originalBcInfo;
   }


int32_t TR_ProfiledNodeVersioning::perform()
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::TreeTop *continuationPoint = NULL;
   TR::Block   *block = NULL;
   for (TR::TreeTop *tt = comp()->findLastTree(); tt; tt = continuationPoint)
      {
      continuationPoint = tt->getPrevTreeTop();
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::BBEnd)
         {
         block = node->getBlock();
         continue;
         }
      else if (block->isCold())
         {
         continue;
         }
      else if (node->getOpCodeValue() == TR::treetop)
         {
         node = node->getFirstChild();
         switch (node->getOpCodeValue())
            {
            case TR::newarray:
            case TR::anewarray:
               {
               TR::Node *numElementsNode = node->getFirstChild();
               if (numElementsNode->getOpCode().isLoadConst())
                  continue;

               TR_ByteCodeInfo originalBcInfo = TR_ProfiledNodeVersioning::temporarilySetProfilingBcInfoOnNewArrayLengthChild(node, comp());
               TR_ValueInfo *numElementsInfo = static_cast<TR_ValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(numElementsNode, comp(), ValueInfo));
               numElementsNode->setByteCodeInfo(originalBcInfo);

               if (numElementsInfo)
                  {
                  int32_t numDifferentSizes = 0;
                  uint32_t profiledSize = 0;
                  uint32_t minProfiledSize = UINT_MAX;
                  float combinedProbability = 0.0;
                  bool doProfiling = false;
                  uint32_t totalFrequency = numElementsInfo->getTotalFrequency();
                  if (totalFrequency > 0)
                     {
                     static char *versionNewarrayForMultipleSizes = feGetEnv("TR_versionNewarrayForMultipleSizes");
                     if (trace())
                        traceMsg(comp(), "Node %s has %d profiled values:\n", getDebug()->getName(node), totalFrequency);
                     TR_ScratchList<TR_ExtraValueInfo> valuesSortedByFrequency(trMemory());
                     numElementsInfo->getSortedList(comp(), &valuesSortedByFrequency);
                     ListIterator<TR_ExtraValueInfo> i(&valuesSortedByFrequency);
                     for (TR_ExtraValueInfo *profiledInfo = i.getFirst(); profiledInfo != NULL; profiledInfo = i.getNext())
                        {
                        float probability = (float)profiledInfo->_frequency / totalFrequency;
                        if (trace())
                           traceMsg(comp(), "%8d %5.1f%%\n", profiledInfo->_value, 100.0 * probability);

                        // The heuristic: accumulate values until we hit the
                        // threshold where it's worth versioning.  After that
                        // point, we become more hesitant to grow profiledSize
                        // for the increasingly rare cases.  The more probable
                        // the case, the more we're willing to grow the size to
                        // catch it.
                        //
                        if (!doProfiling || (profiledInfo->_value < (uint32_t)(profiledSize * (1.0+probability))))
                           {
                           if (numDifferentSizes == 1 && !versionNewarrayForMultipleSizes)
                              break;

                           numDifferentSizes++;
                           profiledSize    = std::max(profiledSize, profiledInfo->_value);
                           minProfiledSize = std::min(minProfiledSize, profiledInfo->_value);

                           combinedProbability += probability;
                           if (combinedProbability > MIN_PROFILED_FREQUENCY)
                              doProfiling = true;

                           }

                        }

                     // Get an idea of the spread, and try to make
                     // sure we don't miss nearby important sizes just
                     // because we didn't happen to see it during
                     // profiling.
                     //
                     uint32_t spread = profiledSize - minProfiledSize;
                     if (profiledSize <= 65 && versionNewarrayForMultipleSizes)
                        {
                        spread = std::max(spread, profiledSize / 5);
                        spread = std::max<uint32_t>(spread, 2);
                        numDifferentSizes = std::max(numDifferentSizes, 2);
                        }
                     profiledSize += spread;
                     }

                  if (doProfiling && performTransformation(comp(), "O^O PROFILED NODE VERSIONING: Versioning %s %s, size %s %d %.01f%%\n",
                                                             node->getOpCode().getName(),
                                                             getDebug()->getName(node),
                                                             (numDifferentSizes == 1)? "==" : "<=",
                                                             profiledSize, 100.0*combinedProbability))
                     {
                     // Create basic diamond
                     //
                     int16_t originalFrequency = block->getFrequency();

                     TR::SymbolReference *storeSymRef = getSymRefTab()->createTemporary(comp()->getMethodSymbol(), numElementsNode->getDataType());

                     TR::TreeTop *storeTree = TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::istore, 1, 1, numElementsNode,  storeSymRef));

                     TR::Node *origNumElementsNode = numElementsNode;
                     numElementsNode =  TR::Node::createWithSymRef(numElementsNode, TR::iload, 0, storeSymRef);
                     continuationPoint->join(storeTree);
                     storeTree->join(tt);
                     node->setAndIncChild(0, numElementsNode);
                     origNumElementsNode->recursivelyDecReferenceCount();

                     TR::TreeTop *compareTree = TR::TreeTop::create(comp(),
                        TR::Node::createif((numDifferentSizes==1)? TR::ificmpne : TR::ificmpgt,
                           numElementsNode->duplicateTree(),
                           TR::Node::iconst(numElementsNode, profiledSize)));

                     TR::Node *variableSizeNew = node->duplicateTree();

                     TR::Node *fixedSizeNew = node->duplicateTree();
                     TR::Node *fixedSizeNewOriginalSize = fixedSizeNew->getFirstChild();
                     fixedSizeNew->setAndIncChild(0, TR::Node::iconst(numElementsNode, profiledSize));

                     TR::TreeTop *slowTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, variableSizeNew));
                     TR::TreeTop *fastTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, fixedSizeNew));

                     block->createConditionalBlocksBeforeTree(tt, compareTree, slowTree, fastTree, comp()->getFlowGraph(), false);

                     if (numDifferentSizes >= 2)
                        {
                        // The array is conservatively large; must clobber its length to the right value
                        TR::TreeTop::create(comp(), fastTree,
                           TR::Node::createWithSymRef(TR::istorei, 2, 2,
                              fixedSizeNew,
                              fixedSizeNewOriginalSize,
                              comp()->getSymRefTab()->findOrCreateContiguousArraySizeSymbolRef()));
                        }

                     fixedSizeNewOriginalSize->recursivelyDecReferenceCount(); // removed from fixedSizeNew

                     // Store new object to a temp, and replace uses of object with loads of the temp
                     //
                     TR::SymbolReference *newObjectPointer = getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);
                     TR::TreeTop::create(comp(), slowTree, TR::Node::createStore(newObjectPointer, variableSizeNew));
                     TR::TreeTop::create(comp(), fastTree, TR::Node::createStore(newObjectPointer, fixedSizeNew));
                     node->removeAllChildren();
                     TR::Node::recreate(node, comp()->il.opCodeForDirectLoad(newObjectPointer->getSymbol()->getDataType()));
                     node->setSymbolReference(newObjectPointer);

                     // Set block frequencies
                     //
                     if (originalFrequency >= 0)
                        {
                        int16_t fastFrequency = std::max<int16_t>(originalFrequency * combinedProbability,       MAX_COLD_BLOCK_COUNT+1);
                        int16_t slowFrequency = std::max<int16_t>(originalFrequency * (1 - combinedProbability), MAX_COLD_BLOCK_COUNT+1);
                        if (trace())
                           {
                           traceMsg(comp(), " -> Setting fast block_%d frequency = %d; slow block_%d frequency = %d\n",
                              fastTree->getEnclosingBlock()->getNumber(), fastFrequency,
                              slowTree->getEnclosingBlock()->getNumber(), slowFrequency);
                           }
                        fastTree->getEnclosingBlock()->setFrequency(fastFrequency);
                        slowTree->getEnclosingBlock()->setFrequency(slowFrequency);
                        }
                     }
                  else if (trace())
                     {
                     traceMsg(comp(), "Not versioning %s node %s because size node %s top probability is %.0f%% < %.0f%%\n",
                             node->getOpCode().getName(),
                             comp()->getDebug()->getName(node),
                             comp()->getDebug()->getName(numElementsNode),
                             100.0 * numElementsInfo->getTopProbability(),
                             100.0 * MIN_PROFILED_FREQUENCY);
                     }
                  }
               else if (trace())
                  {
                  traceMsg(comp(), "No profiling info for size of %s node %s\n",
                          node->getOpCode().getName(),
                          comp()->getDebug()->getName(node));
                  }
               }
               break;
            default:
               break;
            }
         }
      }
#endif

   return 123; // TODO: What to return here?
   }

const char *
TR_ProfiledNodeVersioning::optDetailString() const throw()
   {
   return "O^O PROFILED NODE VERSIONING: ";
   }

TR_Rematerialization::TR_Rematerialization(TR::OptimizationManager *manager)
   : TR::Optimization(manager), _prefetchNodes(trMemory())
   {}


void TR_Rematerialization::rematerializeAddresses(TR::Node *indirectNode, TR::TreeTop *treeTop, vcount_t visitCount)
   {
   if (visitCount == indirectNode->getVisitCount())
      return;

   TR::Symbol * ind_sym = indirectNode->getOpCode().hasSymbolReference() ? indirectNode->getSymbol() : 0;

   indirectNode->setVisitCount(visitCount);
   bool isCommonedAiadd = false;

   if (indirectNode->getOpCode().isIndirect())
      {
      TR::Node *node = indirectNode->getFirstChild();

      if (node->getNumChildren() == 2 && node->isInternalPointer())
         {
         TR::Node *parentOfInternalPointer = indirectNode;
         uintptr_t throttle = 0;
         while (++throttle < 10)
            {
            if  ((node->getReferenceCount() > 1 &&
                 node->isInternalPointer() &&
                 node->getNumChildren() == 2 &&
                 (!node->getSecondChild()->getOpCode().isLoadConst() ||
                  (cg()->isMaterialized(node->getSecondChild()) ||
                        (cg()->getSupportsConstantOffsetInAddressing() &&
                        cg()->getSupportsConstantOffsetInAddressing(node->getSecondChild()->get64bitIntegralValue()))))) ||
                (node->getReferenceCount() == 1 &&
                 node->isInternalPointer() &&
                 node->getNumChildren() == 2 &&
                 node->getFirstChild()->getReferenceCount() > 1 &&
                 node->getFirstChild()->isInternalPointer() &&
                 node->getFirstChild()->getNumChildren() == 2 &&
                 (node->getSecondChild()->getOpCode().isLoadConst() ||
                   (node->getFirstChild()->getSecondChild()->getOpCode().isLoadConst() &&
                   cg()->getSupportsConstantOffsetInAddressing(node->getFirstChild()->getSecondChild()->get64bitIntegralValue())))))
               {
               if (node->getReferenceCount() == 1)
                  {
                  parentOfInternalPointer = node;
                  node = node->getFirstChild();
                  }

#ifdef J9_PROJECT_SPECIFIC
               if (TR::isJ9() && treeTop->getNode()->getOpCodeValue() == TR::pdstorei)
                  {
                  return;
                  }
#endif

               if (performTransformation(comp(), "%sRematerializing node %p(%s)\n", optDetailString(), node, node->getOpCode().getName()))
                  {
                  TR::Node *newChild = TR::Node::create(node->getOpCodeValue(), 2,
                                                      node->getFirstChild(), node->getSecondChild());
                  newChild->setFlags(node->getFlags());
                  if (node->isInternalPointer() && node->getPinningArrayPointer())
                     newChild->setPinningArrayPointer(node->getPinningArrayPointer());

                  parentOfInternalPointer->setAndIncChild(0, newChild);
                  node->recursivelyDecReferenceCount();
                  parentOfInternalPointer = node;
                  if ((node->getNumChildren() == 2) &&
                      node->getFirstChild()->isInternalPointer() &&
                      (node->getFirstChild()->getReferenceCount() > 1) &&
                      (node->getFirstChild()->getNumChildren() == 2))
                     node = node->getFirstChild();
                  else
                     break;
                  }
               else
                  break;
               }
            else
               break;
            }

      if (cg()->getSupportsScaledIndexAddressing() ||
          cg()->getSupportsConstantOffsetInAddressing())
         {
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();
         TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

         comp()->getVisitCount();
         TR::Node *parent = node;
         if (cg()->getSupportsConstantOffsetInAddressing() &&
             (secondChild->getOpCode().isSub() || secondChild->getOpCode().isAdd())
             /*  (((secondChildOp == TR::isub) || (secondChildOp == TR::iadd)) ||
              ((secondChildOp == TR::lsub) || (secondChildOp == TR::ladd)))*/   )
            {
            TR::ILOpCodes childOp = secondChild->getSecondChild()->getOpCodeValue();
            if (secondChild->getSecondChild()->getOpCode().isLoadConst() &&
                  cg()->getSupportsConstantOffsetInAddressing(secondChild->getSecondChild()->get64bitIntegralValue()))
                {
               if (secondChild->getReferenceCount() > 1)
                  {
                  //dumpOptDetails(comp(), "Uncommoning add %p\n", secondChild);
                  //printf("Found a chance in %s\n", comp()->signature());
                  if (performTransformation(comp(), "%sRematerializing node %p(%s)\n", optDetailString(), secondChild, secondChild->getOpCode().getName()))
                     {
                     TR::Node *newSecondChild = TR::Node::create(secondChild->getOpCodeValue(), 2,
                                                                secondChild->getFirstChild(),
                                                                secondChild->getSecondChild());
                      newSecondChild->setFlags(secondChild->getFlags());
                      node->setAndIncChild(1, newSecondChild);
                      secondChild->recursivelyDecReferenceCount();
                      secondChild = newSecondChild;
                      }
                   }

                 parent = secondChild;
                 secondChild = secondChild->getFirstChild();
                 }
              }

           secondChildOp = secondChild->getOpCodeValue();
           if (cg()->getSupportsScaledIndexAddressing() &&
               (((secondChildOp == TR::imul) || (secondChildOp == TR::ishl)) ||
                ((secondChildOp == TR::lmul) || (secondChildOp == TR::lshl))))
              {
              TR::ILOpCodes childOp = secondChild->getSecondChild()->getOpCodeValue();
              if (childOp == TR::iconst || childOp == TR::lconst)
                 {
                 bool allowUncommoning = false;

                 if (childOp == TR::iconst && cg()->isAddressScaleIndexSupported(secondChild->getSecondChild()->getInt()))
                    allowUncommoning = true;

                 if (childOp == TR::lconst && (secondChild->getSecondChild()->getLongIntHigh() == 0) &&
                       cg()->isAddressScaleIndexSupported(secondChild->getSecondChild()->getLongIntLow()))
                    allowUncommoning = true;

                 if (secondChild->getReferenceCount() > 1 && allowUncommoning)
                    {
                    if (performTransformation(comp(), "%sRematerializing node %p(%s)\n", optDetailString(), secondChild, secondChild->getOpCode().getName()))
                       {
                       TR::Node *newSecondChild = TR::Node::create(secondChild->getOpCodeValue(), 2,
                                                                secondChild->getFirstChild(),
                                                                secondChild->getSecondChild());
                       newSecondChild->setFlags(secondChild->getFlags());
                       int32_t childNum = 1;
                       if (parent != node)
                          childNum = 0;
                       //printf("Found a chance in %s\n", comp()->signature());
                       //dumpOptDetails(comp(), "Uncommoning mul %p\n", secondChild);
                       parent->setAndIncChild(childNum, newSecondChild);
                       secondChild->recursivelyDecReferenceCount();
                       }
                    }
                  }
               }
            }
         }
      }

   for (int32_t i = 0; i < indirectNode->getNumChildren(); ++i)
      {
      TR::Node *child = indirectNode->getChild(i);
      if ((child->getOpCodeValue() == TR::iu2l) && comp()->cg()->opCodeIsNoOp(child->getOpCode()))
         {
         if (treeTop->getNode()->getOpCodeValue() != TR::BBEnd)
            {
            if (child->getReferenceCount() > 1)
               {
               if (performTransformation(comp(), "%sRematerializing node %p(%s)\n", optDetailString(), child, child->getOpCode().getName()))
                  {
                  TR::Node *newChild = TR::Node::create(child->getOpCodeValue(), 1,
                               child->getFirstChild());
                  newChild->setFlags(child->getFlags());
                  if (indirectNode->getOpCodeValue() == TR::PassThrough)
                     {
                     TR::Node *anchorNode = TR::Node::create(TR::treetop, 1, newChild);
                     TR::TreeTop *anchorTree = TR::TreeTop::create(comp(), anchorNode);
                     TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
                     prevTree->join(anchorTree);
                     anchorTree->join(treeTop);
                     }
                  indirectNode->setAndIncChild(i, newChild);
                  child->recursivelyDecReferenceCount();
                  }
               }
            else if ((child->getReferenceCount() == 1) &&
                (indirectNode->getOpCodeValue() == TR::PassThrough))
               {
               TR::Node *anchorNode = TR::Node::create(TR::treetop, 1, child);
               TR::TreeTop *anchorTree = TR::TreeTop::create(comp(), anchorNode);
               TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
               prevTree->join(anchorTree);
               anchorTree->join(treeTop);
               }
            }
         else if ((child->getReferenceCount() == 1) &&
                  (indirectNode->getOpCodeValue() == TR::PassThrough))
            {
            TR::Node *anchorNode = TR::Node::create(TR::treetop, 1, child);
            TR::TreeTop *anchorTree = TR::TreeTop::create(comp(), anchorNode);
            TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
            TR::Node *prevNode = prevTree->getNode();
            if(prevNode->getOpCodeValue() == TR::treetop)
                prevNode = prevNode->getFirstChild();
            if (prevNode->getOpCode().isBranch() || prevNode->getOpCode().isJumpWithMultipleTargets())
               prevTree = prevTree->getPrevTreeTop();
            TR::TreeTop *nextTreeTop = prevTree->getNextTreeTop();
            prevTree->join(anchorTree);
            anchorTree->join(nextTreeTop);
            }
         }

      rematerializeAddresses(child, treeTop, visitCount);
      }
   }

int32_t TR_Rematerialization::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 0;
   }


int32_t TR_Rematerialization::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
   return 0;
   }


void TR_Rematerialization::prePerformOnBlocks()
   {
   _counter = 0;
   _curBlock = NULL;
   }

class TR_RematAdjustments
   {
   public:
   int32_t adjustmentFromParent;
   int32_t fpAdjustmentFromParent;
   int32_t vector128AdjustmentFromParent;
   };

class TR_RematState
   {
   public:
   TR_ALLOC(TR_Memory::LocalOpts)

   TR_RematState(TR::Compilation *comp)
      : _currentlyCommonedNodes(comp->trMemory()),
        _currentlyCommonedCandidates(comp->trMemory()),
        _parents(comp->trMemory()),
        _currentlyCommonedLoads(comp->trMemory()),
        _parentsOfCommonedLoads(comp->trMemory()),
        _loadsAlreadyVisited(comp->trMemory()),
        _loadsAlreadyVisitedThatCannotBeRematerialized(comp->trMemory()),

        _currentlyCommonedFPNodes(comp->trMemory()),
        _currentlyCommonedFPCandidates(comp->trMemory()),
        _fpParents(comp->trMemory()),
        _currentlyCommonedFPLoads(comp->trMemory()),
        _parentsOfCommonedFPLoads(comp->trMemory()),
        _fpLoadsAlreadyVisited(comp->trMemory()),
        _fpLoadsAlreadyVisitedThatCannotBeRematerialized(comp->trMemory()),

        _currentlyCommonedVector128Nodes(comp->trMemory()),
        _currentlyCommonedVector128Candidates(comp->trMemory()),
        _vector128Parents(comp->trMemory()),
        _currentlyCommonedVector128Loads(comp->trMemory()),
        _parentsOfCommonedVector128Loads(comp->trMemory()),
        _vector128LoadsAlreadyVisited(comp->trMemory()),
        _vector128LoadsAlreadyVisitedThatCannotBeRematerialized(comp->trMemory())
      {
      }


   TR_ScratchList<TR::Node> _currentlyCommonedNodes;
   TR_ScratchList<TR::Node> _currentlyCommonedCandidates;
   TR_ScratchList< List<TR::Node> > _parents;
   TR_ScratchList<TR::Node> _currentlyCommonedLoads;
   TR_ScratchList< List<TR::Node> > _parentsOfCommonedLoads;
   TR_ScratchList<TR::Node> _loadsAlreadyVisited;
   TR_ScratchList<TR::Node> _loadsAlreadyVisitedThatCannotBeRematerialized;

   TR_ScratchList<TR::Node> _currentlyCommonedFPNodes;
   TR_ScratchList<TR::Node> _currentlyCommonedFPCandidates;
   TR_ScratchList< List<TR::Node> > _fpParents;
   TR_ScratchList<TR::Node> _currentlyCommonedFPLoads;
   TR_ScratchList< List<TR::Node> > _parentsOfCommonedFPLoads;
   TR_ScratchList<TR::Node> _fpLoadsAlreadyVisited;
   TR_ScratchList<TR::Node> _fpLoadsAlreadyVisitedThatCannotBeRematerialized;

   TR_ScratchList<TR::Node> _currentlyCommonedVector128Nodes;
   TR_ScratchList<TR::Node> _currentlyCommonedVector128Candidates;
   TR_ScratchList< List<TR::Node> > _vector128Parents;
   TR_ScratchList<TR::Node> _currentlyCommonedVector128Loads;
   TR_ScratchList< List<TR::Node> > _parentsOfCommonedVector128Loads;
   TR_ScratchList<TR::Node> _vector128LoadsAlreadyVisited;
   TR_ScratchList<TR::Node> _vector128LoadsAlreadyVisitedThatCannotBeRematerialized;
   };

TR::Node * rematerializeNode(TR::Compilation * comp, TR::Node * node)
   {
   TR_ASSERT(node->getReferenceCount() > 1,"node->getReferenceCount() == 1 inside rematerializeNode!");

   TR::Node * newNode = TR::Node::copy(node);
   newNode->setReferenceCount(1);

   for (auto k = 0; k < newNode->getNumChildren(); k++)
      {
      newNode->getChild(k)->incReferenceCount();
      }

   return newNode;
   }



int32_t TR_Rematerialization::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   _underGlRegDeps = false;

   vcount_t visitCount = comp()->incOrResetVisitCount();
   TR::TreeTop *treeTop;
   //
   // Process each block in treetop order
   //


   for (treeTop = startTree; (treeTop != endTree); treeTop = treeTop->getNextRealTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();

      rematerializeAddresses(node, treeTop, visitCount);
      }

   if ((!cg()->doRematerialization()) &&
       !comp()->containsBigDecimalLoad() &&
       !comp()->getSymRefTab()->findPrefetchSymbol())
      return 0;

   _nodeCount = comp()->getNodeCount();
   _heightArray = (int32_t *)trMemory()->allocateStackMemory(_nodeCount*sizeof(int32_t));
   memset(_heightArray, 0, _nodeCount*sizeof(int32_t));

   visitCount = comp()->incVisitCount();
   for (treeTop = startTree; (treeTop != endTree); treeTop = treeTop->getNextTreeTop())
      initializeFutureUseCounts(treeTop->getNode(), NULL, visitCount, comp(), _heightArray);

   visitCount = comp()->incVisitCount();
   TR_RematState *state = new (trStackMemory()) TR_RematState(comp());
   //
   // Process each block in treetop order
   //
   for (treeTop = startTree; (treeTop != endTree); treeTop = treeTop->getNextRealTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         _curBlock = node->getBlock();
         comp()->setCurrentBlock(_curBlock);
         if (!_curBlock->isExtensionOfPreviousBlock())
            {
            TR_ASSERT(state->_currentlyCommonedCandidates.isEmpty(), "Non FP candidates list should be empty and is not\n");
            TR_ASSERT(state->_currentlyCommonedFPCandidates.isEmpty(), "FP candidates list should be empty and is not\n");
            TR_ASSERT(state->_currentlyCommonedVector128Candidates.isEmpty(), "Vector128 candidates list should be empty and is not\n");
            TR_ASSERT(state->_currentlyCommonedLoads.isEmpty(), "Non FP loads list should be empty and is not\n");
            TR_ASSERT(state->_currentlyCommonedFPLoads.isEmpty(), "FP loads list should be empty and is not\n");
            TR_ASSERT(state->_currentlyCommonedVector128Loads.isEmpty(), "Vector128 loads list should be empty and is not\n");
            }
         }

         TR_RematAdjustments newAdjustments = {
                 0,
                 0,
                 0
         };

         examineNode(treeTop, NULL, node, visitCount, state, newAdjustments);
      }

   return 1; // actual cost
   }



bool TR_Rematerialization::isRematerializable(TR::Node *parent, TR::Node *node, bool onlyConsiderOpCode)
   {
   return node->isRematerializable(parent,onlyConsiderOpCode);
   }


bool TR_Rematerialization::isRematerializableLoad(TR::Node *node, TR::Node *parent)
   {
   if ((node->getOpCodeValue() == TR::lloadi) &&
       node->isBigDecimalLoad())
      return true;

   if (parent &&
       (parent->getOpCodeValue() == TR::Prefetch) &&
       (node->getOpCodeValue() == TR::aloadi))
      {
      if (!_prefetchNodes.find(node))
         _prefetchNodes.add(node);
      return true;
      }

   if (!cg()->doRematerialization())
       return false;

   if (node->getOpCode().isLoadVarDirect() &&
       node->getSymbolReference()->getSymbol()->isAutoOrParm())
      return true;

   return false;
   }


static bool storeMightKillRematerializableSym(TR::Node *node)
   {
   if (node->getOpCode().isStoreDirect() &&
       node->getSymbolReference()->getSymbol()->isAutoOrParm())
      return true;

   if (node->getOpCodeValue() == TR::lstorei)
      return true;

   if (node->getOpCode().isStoreIndirect())
      return true;

   return false;
   }



void TR_Rematerialization::removeNodeFromList(TR::Node *node, List<TR::Node> *nodes, List< List<TR::Node> > *parents, bool checkSymRefs, List<TR::Node> *loadsAlreadyVisited, List<TR::Node> *loadsAlreadyVisitedThatCannotBeRematerialized, const TR::SparseBitVector &aliases)
   {
   ListElement<TR::Node> *commonedElement = nodes->getListHead(), *prevNodeElem = NULL;
   ListElement< List<TR::Node> > *parentElement = parents->getListHead(), *prevParentElem = NULL;
   while (commonedElement)
      {
      //dumpOptDetails(comp(), "Commoned load %p store %p\n", commonedElement->getData(), node);
      if ((commonedElement->getData() == node) ||
          (checkSymRefs &&
           ((commonedElement->getData()->getSymbolReference() == node->getSymbolReference()) ||
            aliases.ValueAt(commonedElement->getData()->getSymbolReference()->getReferenceNumber()))))
         {
         if (prevNodeElem)
            {
            prevNodeElem->setNextElement(commonedElement->getNextElement());
            prevParentElem->setNextElement(parentElement->getNextElement());
            }
         else
            {
            nodes->setListHead(commonedElement->getNextElement());
            parents->setListHead(parentElement->getNextElement());
            }

         if (!checkSymRefs)
            break;

         commonedElement = commonedElement->getNextElement();
         parentElement = parentElement->getNextElement();
         continue;
         }

      prevNodeElem = commonedElement;
      prevParentElem = parentElement;
      commonedElement = commonedElement->getNextElement();
      parentElement = parentElement->getNextElement();
      }

   if (checkSymRefs &&
       loadsAlreadyVisited)
      {
      ListElement<TR::Node> *visitedLoadsElement = loadsAlreadyVisited->getListHead();
      ListElement<TR::Node> *prevVisitedLoadsElement = NULL;
      while (visitedLoadsElement)
         {
         if ((visitedLoadsElement->getData()->getSymbolReference() == node->getSymbolReference()) ||
             aliases.ValueAt(visitedLoadsElement->getData()->getSymbolReference()->getReferenceNumber()))
            {
            if (prevVisitedLoadsElement)
               prevVisitedLoadsElement->setNextElement(visitedLoadsElement->getNextElement());
            else
               loadsAlreadyVisited->setListHead(visitedLoadsElement->getNextElement());
            if (!loadsAlreadyVisitedThatCannotBeRematerialized->find(visitedLoadsElement->getData()))
               loadsAlreadyVisitedThatCannotBeRematerialized->add(visitedLoadsElement->getData());
            }
         else
            prevVisitedLoadsElement = visitedLoadsElement;

         visitedLoadsElement = visitedLoadsElement->getNextElement();
         }
      }
   }


void TR_Rematerialization::addParentToList(TR::Node *node, List<TR::Node> *nodes, TR::Node *parent, List< List<TR::Node> > *parents)
   {
   ListElement<TR::Node> *commonedElement = nodes->getListHead();
   ListElement< List<TR::Node> > *parentElement = parents->getListHead();
   while (commonedElement)
      {
      if (commonedElement->getData() == node)
         {
         parentElement->getData()->add(parent);
         break;
         }

      commonedElement = commonedElement->getNextElement();
      parentElement = parentElement->getNextElement();
      }
   }



void
TR_Rematerialization::findSymsUsedInIndirectAccesses(TR::Node *node, List<TR::Node> *nodes, List< List<TR::Node> > *parents)
   {
   if (node->getOpCode().hasSymbolReference() &&
       node->getSymbolReference()->getSymbol()->isAutoOrParm())
      removeNodeFromList(node, nodes, parents, false);

   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      findSymsUsedInIndirectAccesses(node->getChild(i), nodes, parents);
   }



bool TR_Rematerialization::examineNode(TR::TreeTop *treeTop, TR::Node *parent, TR::Node *node,
                                       vcount_t visitCount, TR_RematState *state,
                                       TR_RematAdjustments & adjustments
                                      )
   {

   node->decFutureUseCount();

   if ((((node->getFutureUseCount() & 0x7fff) == 0) &&
        (node->getReferenceCount() > 1)) ||
       (parent &&
        (_underGlRegDeps ||
        parent->getOpCode().isStoreReg())))
      {
      if (node->getOpCode().isFloatingPoint())
         {
         state->_currentlyCommonedFPNodes.remove(node);
         if (isRematerializable(parent, node, true) &&
             node->isRematerializeable())
            {
            if (isRematerializableLoad(node, parent))
               removeNodeFromList(node, &state->_currentlyCommonedFPLoads, &state->_parentsOfCommonedFPLoads, false);
            else
               removeNodeFromList(node, &state->_currentlyCommonedFPCandidates, &state->_fpParents, false);
            }
         }
      else if (node->getOpCode().isVectorResult())
         {
         state->_currentlyCommonedVector128Nodes.remove(node);
         if (isRematerializable(parent, node, true) &&
             node->isRematerializeable())
            {
            if (isRematerializableLoad(node, parent))
               removeNodeFromList(node, &state->_currentlyCommonedVector128Loads, &state->_parentsOfCommonedVector128Loads, false);
            else
               removeNodeFromList(node, &state->_currentlyCommonedVector128Candidates, &state->_vector128Parents, false);
            }
         }
      else
         {
         state->_currentlyCommonedNodes.remove(node);
         if ((isRematerializable(parent, node, true) &&
             node->isRematerializeable()) ||
             _prefetchNodes.find(node))
            {
            if (trace())
               traceMsg(comp(), "Removing node %p\n", node);
            if (isRematerializableLoad(node, parent) || _prefetchNodes.find(node))
               removeNodeFromList(node, &state->_currentlyCommonedLoads, &state->_parentsOfCommonedLoads, false);
            else
               removeNodeFromList(node, &state->_currentlyCommonedCandidates, &state->_parents, false);
            }
         }
      }


   if (((((node->getFutureUseCount() & 0x7fff) > 0) &&
         (node->getReferenceCount() > 1))) &&
       (parent && !_underGlRegDeps &&
        !parent->getOpCode().isStoreReg()))
      {
      if (node->getOpCode().isFloatingPoint())
         {
         if (isRematerializableLoad(node, parent))
            addParentToList(node, &state->_currentlyCommonedFPLoads, parent, &state->_parentsOfCommonedFPLoads);
         else
            addParentToList(node, &state->_currentlyCommonedFPCandidates, parent, &state->_fpParents);
         }
      else if (node->getOpCode().isVectorResult())
         {
         if (isRematerializableLoad(node, parent))
            addParentToList(node, &state->_currentlyCommonedVector128Loads, parent, &state->_parentsOfCommonedVector128Loads);
         else
            addParentToList(node, &state->_currentlyCommonedVector128Candidates, parent, &state->_vector128Parents);
         }
      else
         {
         if (isRematerializableLoad(node, parent))
            addParentToList(node, &state->_currentlyCommonedLoads, parent, &state->_parentsOfCommonedLoads);
         else
            addParentToList(node, &state->_currentlyCommonedCandidates, parent, &state->_parents);
         }
      }

   if (node->getVisitCount() == visitCount)
      return true;

   int32_t childAdjustment = 0;
   int32_t fpChildAdjustment = 0;
   int32_t vector128ChildAdjustment = 0;

   if (node->getOpCodeValue() == TR::GlRegDeps)
      _underGlRegDeps = true;

    if (trace()) traceMsg(comp(), "Parent adjust %d node %p parent %p\n", adjustments.adjustmentFromParent, node, parent);

   //TR_ScratchList<TR::Node> seenChildren(trMemory());
   TR_BitVector seenChildren(node->getNumChildren(), trMemory(), stackAlloc);


   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);

      //if (child->getVisitCount() == visitCount)
      if ((child->getFutureUseCount() & 0x7fff) < child->getReferenceCount())
         {
         if (trace()) traceMsg(comp(), "Adding child %p to parent %p\n", child, node);
         seenChildren.set(i);

         TR_RematAdjustments newAdjustments = {
                 adjustments.adjustmentFromParent + childAdjustment,
                 adjustments.fpAdjustmentFromParent + fpChildAdjustment,
                 adjustments.vector128AdjustmentFromParent + vector128ChildAdjustment
         };

         examineNode(treeTop, node, child, visitCount, state, newAdjustments);

         if (((child->getFutureUseCount() & 0x7fff) == 0) &&
             !child->getOpCode().isLoadConst())
           {
           if (child->getOpCode().isFloatingPoint())
              fpChildAdjustment++;
           else if (child->getOpCode().isVectorResult())
              {
              vector128ChildAdjustment++;
              }
           else
              {
              childAdjustment++;
              }
            }
         }
      }

   while (true)
      {
      int32_t i = 0;
      int32_t maxChildNum = 0;
      TR::Node *maxChild = NULL;
      int32_t maxHeight = -1;
      for (i = 0; i < node->getNumChildren(); ++i)
         {
         TR::Node *child = node->getChild(i);

         //if (child->getVisitCount() != visitCount)
         //if ((child->getFutureUseCount() & 0x7fff) == child->getReferenceCount())
         if (!seenChildren.get(i))
            {
            if (child->getGlobalIndex() < unsigned(_nodeCount))
               {
               if (_heightArray[child->getGlobalIndex()] > maxHeight)
                  {
                  maxHeight = _heightArray[child->getGlobalIndex()];
                  maxChildNum = i;
                  maxChild = child;
                  }
               }
            else
               {
               maxChildNum = i;
               maxChild = child;
               break;
               }
            }
         }

      if (maxChild)
         {
         seenChildren.set(maxChildNum);

         TR_RematAdjustments newAdjustments = {
                 adjustments.adjustmentFromParent + childAdjustment,
                 adjustments.fpAdjustmentFromParent + fpChildAdjustment,
                 adjustments.vector128AdjustmentFromParent + vector128ChildAdjustment
         };
         examineNode(treeTop, node, maxChild, visitCount, state, newAdjustments);

         if (((maxChild->getFutureUseCount() & 0x7fff) == 0) &&
             !maxChild->getOpCode().isLoadConst())
           {
           if (maxChild->getOpCode().isFloatingPoint())
              fpChildAdjustment++;
           else if (maxChild->getOpCode().isVectorResult())
              {
              vector128ChildAdjustment++;
              }
           else
              {
              childAdjustment++;
              if (trace()) traceMsg(comp(), "Child adjust %d child %p node %p\n", childAdjustment, maxChild, node);
              }
            }
         }
      else
         break;
      }

   /*
   if (node->getNumChildren() != seenChildren.getSize())
      {
      traceMsg(comp(), "Node %p num children %d seen children %d\n", node, node->getNumChildren(), seenChildren.getSize());
      TR_ASSERT(0, "Did not visit all children\n");
      }
   */

   if (node->getOpCodeValue() == TR::GlRegDeps)
      _underGlRegDeps = false;

   node->setVisitCount(visitCount);

   static char *regPress = feGetEnv("TR_IgnoreRegPressure");

   if ((regPress != NULL) &&
       (node->getNumChildren() == 2) &&
       isRematerializableLoad(node->getFirstChild(), node) &&

       (!node ||
        (node->getOpCodeValue() != TR::Prefetch) ||
        (node->getFirstChild()->getOpCodeValue() != TR::aloadi)) &&

       ((node->getFirstChild()->getOpCodeValue() != TR::lloadi) ||
        !node->getFirstChild()->isBigDecimalLoad()))
      {
      if (node->getSecondChild()->getOpCode().isLoadConst() ||
          isRematerializableLoad(node->getSecondChild(), node))
         {
         if (trace()) traceMsg(comp(), "1Removing node %p\n", node);
         if (node->getOpCode().isFloatingPoint())
            removeNodeFromList(node->getFirstChild(), &state->_currentlyCommonedFPLoads, &state->_parentsOfCommonedFPLoads, true);
         else if (node->getOpCode().isVectorResult())
            {
            removeNodeFromList(node->getFirstChild(), &state->_currentlyCommonedVector128Loads, &state->_parentsOfCommonedVector128Loads, true);
            }
         else
            removeNodeFromList(node->getFirstChild(), &state->_currentlyCommonedLoads, &state->_parentsOfCommonedLoads, true);
         }
      }

   TR::SparseBitVector aliases(comp()->allocator());
   bool removeNodes = false;
   //if (node->getOpCode().isStoreDirect() &&
   //    node->getSymbolReference()->getSymbol()->isAutoOrParm())
   if (storeMightKillRematerializableSym(node))
      removeNodes = true;

   bool isSimilarToCall = false;
   bool isCall = false;
   if ((node->getOpCode().hasSymbolReference() &&
        node->getSymbolReference()->isUnresolved() &&
        parent &&
        parent->getOpCode().isResolveCheck()) ||
       (node->getOpCodeValue() == TR::monent) ||
       (node->getOpCodeValue() == TR::monexit) ||
       node->mightHaveNonTransparentSymbolReference())
      isSimilarToCall = true;

   bool isCallDirect = false;
   if (node->getOpCode().isCall() &&
      (node->getOpCode().isTreeTop() ||
       (parent && ((parent->getOpCodeValue() == TR::treetop) ||
         (parent->getOpCode().isResolveOrNullCheck())))))
      {
      isCall = true;
      if (node->getOpCode().isCallDirect())
         isCallDirect = true;
      }

   if (isCall ||
       isSimilarToCall)
      {
      removeNodes = true;
      node->mayKill().getAliases(aliases);
      }

   if (removeNodes)
      {
      if (trace()) traceMsg(comp(), "2Removing node %p\n", node);
      if (node->getOpCode().isFloatingPoint())
         removeNodeFromList(node, &state->_currentlyCommonedFPLoads,
                           &state->_parentsOfCommonedFPLoads, true,
                           &state->_fpLoadsAlreadyVisited, &state->_fpLoadsAlreadyVisitedThatCannotBeRematerialized, aliases);
      else if (node->getOpCode().isVectorResult())
         {
         removeNodeFromList(node, &state->_currentlyCommonedVector128Loads,
                           &state->_parentsOfCommonedVector128Loads, true,
                           &state->_vector128LoadsAlreadyVisited, &state->_vector128LoadsAlreadyVisitedThatCannotBeRematerialized, aliases);
         }
      else
         removeNodeFromList(node, &state->_currentlyCommonedLoads, &state->_parentsOfCommonedLoads,
                           true, &state->_loadsAlreadyVisited, &state->_loadsAlreadyVisitedThatCannotBeRematerialized, aliases);
      }

    bool considerRegPressure = true; // enable for other platforms later
    bool rematSpecialNode = false;

    if((NULL != regPress) ||
       (!state->_currentlyCommonedLoads.isEmpty() &&
        (state->_currentlyCommonedLoads.getListHead()->getData()->getOpCodeValue() == TR::lloadi) &&
        state->_currentlyCommonedLoads.getListHead()->getData()->isBigDecimalLoad()))
       considerRegPressure = false;

    // rematerialize to generate TM's. bloadi's are not evaluated
    if (NULL != regPress)
        {
        considerRegPressure = false;
        }

    if ((NULL == regPress) && !considerRegPressure)
       rematSpecialNode = true;

    if ((NULL != regPress) ||
       (!state->_currentlyCommonedLoads.isEmpty() &&
        (state->_currentlyCommonedLoads.getListHead()->getData()->getOpCodeValue() == TR::aloadi) &&
        !state->_parentsOfCommonedLoads.getListHead()->getData()->isEmpty() &&
        (state->_parentsOfCommonedLoads.getListHead()->getData()->getListHead()->getData()->getOpCodeValue() == TR::Prefetch)))
        {
        considerRegPressure = false;
        }


    int32_t numRegisters = 0;
    if (comp()->target().is32Bit())
       {
       ListIterator<TR::Node> nodesIt(&(state->_currentlyCommonedNodes));
       TR::Node *commonedNode = NULL;
       for (commonedNode = nodesIt.getFirst(); commonedNode; commonedNode = nodesIt.getNext())
         {
         if (commonedNode->getType().isInt64())
            numRegisters++;
         numRegisters++;
         }
       }

    if (trace())
       {
       traceMsg(comp(), "At node %p parent %p GPR pressure is %d (child adjust %d parent adjust %d) limit is %d\n", node, parent, numRegisters + childAdjustment + adjustments.adjustmentFromParent, childAdjustment, adjustments.adjustmentFromParent, (cg()->getMaximumNumbersOfAssignableGPRs()-1));
       traceMsg(comp(), "candidate nodes size %d candidate loads size %d\n", state->_currentlyCommonedCandidates.getSize(), state->_currentlyCommonedLoads.getSize());
       }

    if ((!considerRegPressure ||
        ((state->_currentlyCommonedNodes.getSize() + childAdjustment + adjustments.adjustmentFromParent) > (cg()->getMaximumNumbersOfAssignableGPRs() /* -1 */ ))) &&
        (!state->_currentlyCommonedCandidates.isEmpty() || !state->_currentlyCommonedLoads.isEmpty()))
       {
       //_counter++;
       //printf("Rematerializing in %s\n", comp()->signature());
       rematerializeNode(treeTop, parent, node, visitCount, &state->_currentlyCommonedNodes, &state->_currentlyCommonedCandidates, &state->_parents, &state->_currentlyCommonedLoads, &state->_parentsOfCommonedLoads, &state->_loadsAlreadyVisited, &state->_loadsAlreadyVisitedThatCannotBeRematerialized, rematSpecialNode);
       }

      if (trace() && node->getOpCode().isFloatingPoint())
      {
      traceMsg(comp(), "At node %p FPR pressure is %d (child adjust %d parent adjust %d) limit is %d\n", node, state->_currentlyCommonedFPNodes.getSize() + fpChildAdjustment + adjustments.fpAdjustmentFromParent, fpChildAdjustment, adjustments.fpAdjustmentFromParent, (cg()->getMaximumNumbersOfAssignableFPRs()-1));
      traceMsg(comp(), "candidate nodes size %d candidate loads size %d\n", state->_currentlyCommonedFPCandidates.getSize(), state->_currentlyCommonedFPLoads.getSize());
      }

   if ((!considerRegPressure || ((state->_currentlyCommonedFPNodes.getSize() + fpChildAdjustment + adjustments.fpAdjustmentFromParent) > cg()->getMaximumNumbersOfAssignableFPRs() /* -1 */)) &&
      (!state->_currentlyCommonedFPCandidates.isEmpty() || !state->_currentlyCommonedFPLoads.isEmpty()))
      {
      //_counter++;
      //printf("Rematerializing FP node in %s\n", comp()->signature());
      rematerializeNode(treeTop, parent, node, visitCount, &state->_currentlyCommonedFPNodes, &state->_currentlyCommonedFPCandidates, &state->_fpParents, &state->_currentlyCommonedFPLoads, &state->_parentsOfCommonedFPLoads, &state->_fpLoadsAlreadyVisited, &state->_fpLoadsAlreadyVisitedThatCannotBeRematerialized, rematSpecialNode);
      }

      if (trace() && node->getOpCode().isVectorResult())
      {
      traceMsg(comp(), "At node %p VSR pressure is %d (child adjust %d parent adjust %d) limit is %d\n", node, state->_currentlyCommonedVector128Nodes.getSize() + vector128ChildAdjustment + adjustments.vector128AdjustmentFromParent, vector128ChildAdjustment, adjustments.vector128AdjustmentFromParent, vector128ChildAdjustment + adjustments.vector128AdjustmentFromParent);
      traceMsg(comp(), "candidate nodes size %d candidate loads size %d\n", state->_currentlyCommonedVector128Candidates.getSize(), state->_currentlyCommonedVector128Loads.getSize());
      }

   if ((!considerRegPressure || ((state->_currentlyCommonedVector128Nodes.getSize() + vector128ChildAdjustment + adjustments.vector128AdjustmentFromParent) > cg()->getMaximumNumbersOfAssignableVRs() )) &&
      (!state->_currentlyCommonedVector128Candidates.isEmpty() || !state->_currentlyCommonedVector128Loads.isEmpty()))
      {
      //_counter++;
      //printf("Rematerializing Vector128 node in %s\n", comp()->signature());
      rematerializeNode(treeTop, parent, node, visitCount, &state->_currentlyCommonedVector128Nodes, &state->_currentlyCommonedVector128Candidates, &state->_vector128Parents, &state->_currentlyCommonedVector128Loads, &state->_parentsOfCommonedVector128Loads, &state->_vector128LoadsAlreadyVisited, &state->_vector128LoadsAlreadyVisitedThatCannotBeRematerialized, rematSpecialNode);
      }

   if ((node->getFutureUseCount() & 0x7fff) > 0)
      {
      if (!node->getOpCode().isLoadConst())
         {
         if (parent)
            {
            if (node->getOpCode().isFloatingPoint())
               state->_currentlyCommonedFPNodes.add(node);
            else if (node->getOpCode().isVectorResult())
               {
               state->_currentlyCommonedVector128Nodes.add(node);
               }
            else
               state->_currentlyCommonedNodes.add(node);
            }

         //traceMsg(comp(), "Adding node %p\n", node);
         if (parent && !_underGlRegDeps &&
            !parent->getOpCode().isStoreReg() &&
            isRematerializable(parent, node) &&
             node->isRematerializeable())
            {
            if (isRematerializableLoad(node, parent))
               {
               if (node->getOpCode().isFloatingPoint())
                  {
                  if (!state->_fpLoadsAlreadyVisitedThatCannotBeRematerialized.find(node))
                     {
                     //traceMsg(comp(), "1Adding node %p to _currentlyCommonedFPLoads\n", node);
                     state->_currentlyCommonedFPLoads.add(node);
                     TR_ScratchList<TR::Node> *parentList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
                     parentList->add(parent);
                     state->_parentsOfCommonedFPLoads.add(parentList);
                     }
                  }
               else if (node->getOpCode().isVectorResult())
                  {
                  if (!state->_vector128LoadsAlreadyVisitedThatCannotBeRematerialized.find(node))
                     {
                     //traceMsg(comp(), "1Adding node %p to _currentlyCommonedVector128Loads\n", node);
                     state->_currentlyCommonedVector128Loads.add(node);
                     TR_ScratchList<TR::Node> *parentList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
                     parentList->add(parent);
                     state->_parentsOfCommonedVector128Loads.add(parentList);
                     }
                  }
               else
                  {
                  if (!state->_loadsAlreadyVisitedThatCannotBeRematerialized.find(node))
                     {
                     //traceMsg(comp(), "1Adding node %p to _currentlyCommonedLoads\n", node);
                     state->_currentlyCommonedLoads.add(node);
                     TR_ScratchList<TR::Node> *parentList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
                     parentList->add(parent);
                     state->_parentsOfCommonedLoads.add(parentList);
                     }
                  }
               }
            else
               {
               if (node->getOpCode().isFloatingPoint())
                  {
                  //traceMsg(comp(), "2Adding node %p to _currentlyCommonedFPCandidates\n", node);
                  state->_currentlyCommonedFPCandidates.add(node);
                  TR_ScratchList<TR::Node> *parentList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
                  parentList->add(parent);
                  state->_fpParents.add(parentList);
                  }
               else if (node->getOpCode().isVectorResult())
                  {
                  //traceMsg(comp(), "2Adding node %p to _currentlyCommonedVector128Candidates\n", node);
                  state->_currentlyCommonedVector128Candidates.add(node);
                  TR_ScratchList<TR::Node> *parentList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
                  parentList->add(parent);
                  state->_vector128Parents.add(parentList);
                  }
               else
                  {
                  //traceMsg(comp(), "2Adding node %p to _currentlyCommonedCandidates\n", node);
                  state->_currentlyCommonedCandidates.add(node);
                  TR_ScratchList<TR::Node> *parentList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
                  parentList->add(parent);
                  state->_parents.add(parentList);
                  }
               }
            }
         }
      }

   return true;
   }




static List<TR::Node> *getParentList(TR::Node *node, List< List<TR::Node> > *parents, List<TR::Node> *nodes)
   {
   ListElement<TR::Node> *commonedNodeListElem = nodes->getListHead();
   ListElement< List<TR::Node> > *parentListElem = parents->getListHead();

   while (commonedNodeListElem)
      {
      if (commonedNodeListElem->getData() == node)
         return parentListElem->getData();

      parentListElem = parentListElem->getNextElement();
      commonedNodeListElem = commonedNodeListElem->getNextElement();
      }

   return NULL;
   }

void TR_Rematerialization::rematerializeNode(TR::TreeTop *treeTop, TR::Node *parent, TR::Node *node, vcount_t visitCount, List<TR::Node> *currentlyCommonedNodes, List<TR::Node> *currentlyCommonedCandidates, List< List<TR::Node> > *parents, List<TR::Node> *currentlyCommonedLoads, List< List<TR::Node> > *parentsOfCommonedLoads, List<TR::Node> *loadsAlreadyVisited, List<TR::Node> *loadsAlreadyVisitedThatCannotBeRematerialized, bool rematSpecialNode)
   {
   if (trace())
      traceMsg(comp(), "rematerializeNode: parent = %p node = %p\n", parent, node);

   bool mustBeAnchored = false;
   TR::Node *nodeToBeRematerialized = NULL;
   List< List<TR::Node> > *parentsOfNodes = NULL;
   List<TR::Node> *nodes = NULL;
   if (!rematSpecialNode && !currentlyCommonedCandidates->isEmpty())
      {
      ListElement<TR::Node> *prevNodeListElem = NULL;
      ListElement< List<TR::Node> > *prevParentListElem = NULL;

      ListElement<TR::Node> *commonedNodeListElem = currentlyCommonedCandidates->getListHead();
      ListElement< List<TR::Node> > *parentListElem = parents->getListHead();
      TR::Node *commonedNode = NULL;

      while (commonedNodeListElem)
         {
         commonedNode = commonedNodeListElem->getData();

         if (commonedNode->getOpCode().isArrayLength())
            break;

         prevNodeListElem = commonedNodeListElem;
         prevParentListElem = parentListElem;

         commonedNodeListElem = commonedNodeListElem->getNextElement();
         parentListElem = parentListElem->getNextElement();
         }

      if (commonedNodeListElem)
         {
         nodeToBeRematerialized = commonedNode;
         nodes = currentlyCommonedCandidates;
         parentsOfNodes = parents;

         ListElement<TR::Node> *nextNodeListElem = commonedNodeListElem->getNextElement();
         ListElement< List<TR::Node> > *nextParentListElem = parentListElem->getNextElement();

         if (prevNodeListElem)
            {
            prevNodeListElem->setNextElement(nextNodeListElem);
            prevParentListElem->setNextElement(nextParentListElem);
            }
         else
            {
            currentlyCommonedCandidates->setListHead(commonedNodeListElem);
            parents->setListHead(parentListElem);
            }

         if (nodes->getListHead() != commonedNodeListElem)
            {
            commonedNodeListElem->setNextElement(nodes->getListHead());
            parentListElem->setNextElement(parentsOfNodes->getListHead());
            nodes->setListHead(commonedNodeListElem);
            parentsOfNodes->setListHead(parentListElem);
            }
         }
      }

   if (!nodeToBeRematerialized)
      {
      if (!currentlyCommonedLoads->isEmpty())
         {
         nodeToBeRematerialized = currentlyCommonedLoads->getListHead()->getData();
         nodes = currentlyCommonedLoads;
         parentsOfNodes = parentsOfCommonedLoads;
         mustBeAnchored = true;
         }
      else if (!currentlyCommonedCandidates->isEmpty())
         {
         nodeToBeRematerialized = currentlyCommonedCandidates->getListHead()->getData();
         nodes = currentlyCommonedCandidates;
         parentsOfNodes = parents;
         }
      }

   bool isAdjunctNode = nodeToBeRematerialized->isAdjunct();

   if (isAdjunctNode)
      traceMsg (comp(), "Prevented adjunct node %p from being rematerialized\n", nodeToBeRematerialized);

   if (!isAdjunctNode && performTransformation(comp(), "%sRematerializing node %p(%s)\n", optDetailString(), nodeToBeRematerialized, nodeToBeRematerialized->getOpCode().getName()))
      {
      TR::Node *newNode = TR::Node::copy(nodeToBeRematerialized);

      if (nodeToBeRematerialized->getNumChildren() > 0)
         {
         TR::Node *firstChild = nodeToBeRematerialized->getFirstChild();
         TR::Node *newFirstChild = NULL;
         if (isRematerializable(nodeToBeRematerialized, firstChild) &&
             firstChild->isRematerializeable() &&
             !firstChild->getOpCode().isLoadVarDirect() &&

             (!parent ||
              (parent->getOpCodeValue() != TR::Prefetch) ||
              (node->getOpCodeValue() != TR::aloadi)) &&

             ((node->getOpCodeValue() != TR::lloadi) ||
              !node->isBigDecimalLoad()))
             {
             newFirstChild = TR::Node::copy(firstChild);
             newFirstChild->setFutureUseCount(0);
             newFirstChild->setReferenceCount(0);
             for (int32_t j=0;j<newFirstChild->getNumChildren();j++)
                {
                newFirstChild->getChild(j)->incReferenceCount();
                List<TR::Node> *parentList = getParentList(newFirstChild->getChild(j), parents, currentlyCommonedCandidates);
                if (parentList)
                   {
                   parentList->add(newFirstChild);
                   parentList->remove(firstChild);
                   }

                parentList = getParentList(newFirstChild->getChild(j), parentsOfCommonedLoads, currentlyCommonedLoads);
                if (parentList)
                   {
                   parentList->add(newFirstChild);
                   parentList->remove(firstChild);
                   }
                }

             //traceMsg(comp(), "new node %p new first child %p\n", newNode, newFirstChild);
             newNode->setChild(0, newFirstChild);
             //firstChild->recursivelyDecReferenceCount();
             }
          }

      newNode->setFutureUseCount(0);
      newNode->setReferenceCount(0);
      for (int32_t j=0;j<newNode->getNumChildren();j++)
         {
         newNode->getChild(j)->incReferenceCount();

         List<TR::Node> *parentList = getParentList(newNode->getChild(j), parents, currentlyCommonedCandidates);
         if (parentList)
            {
            parentList->add(newNode);
            parentList->remove(nodeToBeRematerialized);
            }

         parentList = getParentList(newNode->getChild(j), parentsOfCommonedLoads, currentlyCommonedLoads);
         if (parentList)
            {
            parentList->add(newNode);
            parentList->remove(nodeToBeRematerialized);
            }
         }

      List<TR::Node> *parentList = parentsOfNodes->getListHead()->getData();
      ListElement<TR::Node> *parentElement = parentList->getListHead();
      while (parentElement)
         {
         TR::Node *parentNode = parentElement->getData();
         if (parentNode /* &&
                           (parentNode->getOpCode().isTreeTop() || parentNode->getReferenceCount() > 0) */)
            {
            for (int32_t j=0;j<parentNode->getNumChildren();j++)
               {
               if (parentNode->getChild(j) == nodeToBeRematerialized)
                  {
                  if (trace())
                     traceMsg(comp(), "\tin parent node %p\n", parentNode);
                  parentNode->setAndIncChild(j, newNode);
                  nodeToBeRematerialized->recursivelyDecReferenceCount();

                  List<TR::Node> *parentList2 = getParentList(nodeToBeRematerialized, parents, currentlyCommonedCandidates);
                  if (parentList2)
                     parentList->remove(parentNode);

                  parentList2 = getParentList(nodeToBeRematerialized, parentsOfCommonedLoads, currentlyCommonedLoads);
                  if (parentList2)
                     parentList2->remove(parentNode);

                  break;
                  }
               }

            if (parentNode->getOpCodeValue() == TR::NULLCHK)
               {
               if (parentList->isSingleton())
                  {
                  parentNode->setAndIncChild(0, TR::Node::create(TR::PassThrough, 1, parentNode->getFirstChild()->getFirstChild()));
                  optimizer()->setRequestOptimization(OMR::compactNullChecks, true, _curBlock);
                  //requestOpt(compactNullChecks);
                  newNode->recursivelyDecReferenceCount();
                  parentNode->getFirstChild()->setFutureUseCount(0);
                  }
               }
            }

         parentElement = parentElement->getNextElement();
         }

     if (!mustBeAnchored)
        nodeToBeRematerialized->setVisitCount(visitCount-1);
     else
        {
        TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, nodeToBeRematerialized);

        List<TR::Node> *parentList = getParentList(nodeToBeRematerialized, parents, currentlyCommonedCandidates);
        if (parentList)
           parentList->add(treetopNode);

         parentList = getParentList(nodeToBeRematerialized, parentsOfCommonedLoads, currentlyCommonedLoads);
         if (parentList)
            parentList->add(treetopNode);

        TR::TreeTop *newTreeTop = TR::TreeTop::create(comp(), treetopNode, NULL, NULL);
        TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
        prevTree->join(newTreeTop);
        newTreeTop->join(treeTop);
        optimizer()->setRequestOptimization(OMR::deadTreesElimination, true, _curBlock);
        nodeToBeRematerialized->setVisitCount(visitCount-1);
        }

     if (mustBeAnchored &&
         !loadsAlreadyVisited->find(nodeToBeRematerialized) &&
         !loadsAlreadyVisitedThatCannotBeRematerialized->find(nodeToBeRematerialized))
         loadsAlreadyVisited->add(nodeToBeRematerialized);

     nodes->setListHead(nodes->getListHead()->getNextElement());
     parentsOfNodes->setListHead(parentsOfNodes->getListHead()->getNextElement());
     currentlyCommonedNodes->remove(nodeToBeRematerialized);
     }
   }

const char *
TR_Rematerialization::optDetailString() const throw()
   {
   return "O^O REMATERIALIZATION: ";
   }

TR_BlockSplitter::TR_BlockSplitter(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

#define DEFAULT_SPLIT_PERCENTAGE      15
#define DEFAULT_SPLIT_PRED_PERCENTAGE 10
//TODO: we should not need a synergy cutoff at all, but we have one to
// avoid breaking DT3 for now - we need to revisit warm block splitting
#define DEFAULT_SYNERGY_CUTOFF       warm

TR_RegionStructure* TR_BlockSplitter::getParentStructure(TR::Block *block)
   {
   return block->getStructureOf() && block->getStructureOf()->getParent() ?
          block->getStructureOf()->getParent()->asRegion() : NULL;
   }

bool TR_BlockSplitter::disableSynergy()
   {
   static const char *disableBlockSplitterSynergyStr = feGetEnv("TR_DisableBlockSplitterSynergy");
   return disableBlockSplitterSynergyStr
          || comp()->getMethodHotness() <= DEFAULT_SYNERGY_CUTOFF
          || (comp()->getMethodHotness() == scorching && !getLastRun());
   }

int32_t TR_BlockSplitter::perform()
   {
   if(trace() && disableSynergy())
      traceMsg(comp(), "Block splitting hotness = %d, method = %s\n", comp()->getMethodHotness(), comp()->signature());
   // There must be profiling information available for this optimization
   //
   TR::CFG *cfg = comp()->getFlowGraph();
   if (!cfg)
      {
      if(trace())
         traceMsg(comp(), "No cfg, aborting BlockSplitter\n");
      return 0;
      }

   TR::Recompilation *recompilationInfo = comp()->getRecompilationInfo();
   if (!recompilationInfo)
      {
      if(trace())
         traceMsg(comp(), "No recompilation info, aborting BlockSplitter\n");
      return 0;
      }

   // Block splitter is now re-enabled for large apps but in very
   // limited way. Look for uses of optServer.
   if (disableSynergy() && comp()->isOptServer())
      {
      if(trace())
         traceMsg(comp(), "Large app detected, making BlockSplitter very conservative to avoid cloning code (increases i-cache pressure)\n");
      }

   //static const char *doit = feGetEnv("TR_BlockSplitting");

   static char *maxWarmSplitterDepth = feGetEnv("TR_maxWarmSplitterDepth");
   static char *maxScorchingSplitterDepth = feGetEnv("TR_maxScorchingSplitterDepth");
   static char *splitterAlpha = feGetEnv("TR_splitterAlpha");

   // use a larger alpha at lower opt levels to split fewer blocks
   if (splitterAlpha)
      {
      alpha = ((float)(atoi(splitterAlpha)))/1000.0;
      }
   else if (comp()->getMethodHotness() <= hot)
      {
      alpha = 0.25;
      }
   else
      {
      alpha = 0.125;
      }

   //int maxDepth = maxBlockSplitterDepth ? atoi(maxBlockSplitterDepth) : 1;
   int32_t maxDepth;
   int32_t totalFrequency;
   switch (comp()->getMethodHotness())
      {
      case warm:
         maxDepth = maxWarmSplitterDepth ? atoi(maxWarmSplitterDepth) : 3;
         totalFrequency = MAX_WARM_BLOCK_COUNT;
         break;
      case hot:
      case veryHot:
      case scorching:
         maxDepth = maxScorchingSplitterDepth ? atoi(maxScorchingSplitterDepth) : 9;
         totalFrequency = (MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT);
         break;
      default:
         maxDepth = 2;
         totalFrequency = (MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT);
      }

   // from this point on, all stack allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Set up thresholds for block splitting
   //
   //int32_t totalFrequency = recompilationInfo->getMaxBlockCount();
   static const char * p;
   static int32_t splitPercentage = (p = feGetEnv("TR_SplitPercentage")) ? atoi(p) : DEFAULT_SPLIT_PERCENTAGE;
   int32_t splitFrequency = splitPercentage * totalFrequency / 100;
   static int32_t splitPredPercentage = (p = feGetEnv("TR_SplitPredPercentage")) ? atoi(p) : DEFAULT_SPLIT_PRED_PERCENTAGE;
   int32_t splitPredFrequency = splitPredPercentage * totalFrequency / 100;
   // the following is a debug facility to allow splitting to be disabled by block number for testing purposes
   static int32_t skipBlock = (p = feGetEnv("TR_SkipMergeBlock")) ? atoi(p) : -1;

   if (trace())
      traceMsg(comp(), "  alpha=%f maxDepth=%d totalFrequency=%d splitFrequency=%d splitPredFrequency=%d\n",
                         alpha,   maxDepth,   totalFrequency,   splitFrequency,   splitPredFrequency);

   // Find all the merge nodes and order them by hotness
   //
   // Merge nodes are defined to be nodes that have multiple predecessors (but
   // no exception predecessors) and that are not loop headers.
   //
   TR_BinaryHeap                 *heap     = new (trStackMemory()) TR_BinaryHeap(trMemory(), 0);
   TR_Stack<TR::CFGNode*>         *children = new (trStackMemory()) TR_Stack<TR::CFGNode*>(trMemory());
   uint32_t                       index    = 0;
   //TR_IndexedBinaryHeapElement*   elem;

   TR::CFGNode                    *node;
   TR::Block                      *mergeNode;
   TR::CFGEdge                    *splitPred_to_mergeNode_edge = NULL;
   bool                           hasNonZeroFrequencies = false;

   comp()->incVisitCount();
   children->push(cfg->comp()->getStartTree()->getNode()->getBlock());
   cfg->comp()->getStartTree()->getNode()->getBlock()->setVisitCount(comp()->getVisitCount());

   // post order traversal of the nodes in the graph looking for the merge points we should visit
   // the order is important because the index will be set in visitation order and we need to sort
   // by this index to obtain optimal splitting

   if (trace())
      traceMsg(comp(), "  Starting search for merge points\n");
   while (children->size() > 0)
      {
      TR::CFGEdge * unvisitedEdge = NULL;
      for (auto edge = children->top()->getSuccessors().begin(); edge != children->top()->getSuccessors().end(); ++edge)
         {
         if((*edge)->getTo()->getVisitCount() != comp()->getVisitCount())
            {
            unvisitedEdge = *edge;
            break;
            }
         }

      if (unvisitedEdge != NULL)
         {
         //traceMsg(comp(), "Found unvisited edge, pushing block_%d\n", toBlock(unvisitedEdge->getTo())->getNumber());
         unvisitedEdge->getTo()->setVisitCount(comp()->getVisitCount());
         children->push(unvisitedEdge->getTo());
         continue;
         }
      mergeNode = toBlock(children->top());
      if (
            (mergeNode->hasExceptionPredecessors() == true) ||
            (mergeNode->getPredecessors().size() == 1) ||
            mergeNode->getPredecessors().empty()
         )
         {
         children->pop();
         continue;
         }
      if (mergeNode->isOSRCodeBlock() || mergeNode->isOSRInduceBlock())
         {
         if (trace())
            traceMsg(comp(), "    rejecting osr block_%d\n", mergeNode->getNumber());
         children->pop();
         continue;
         }
      if (!mergeNode->getEntry())
         {
         if (trace())
            traceMsg(comp(), "    rejecting empty block_%d\n", mergeNode->getNumber());
         children->pop();
         continue;
         }
      if (mergeNode->getFrequency() < splitFrequency)
         {
         if (trace())
            traceMsg(comp(), "    rejecting block_%d, frequency is %d, threshold is %d\n", mergeNode->getNumber(), mergeNode->getFrequency(), splitFrequency);
         children->pop();
         continue;
         }
      if (isLoopHeader(mergeNode))
         {
         if(trace())
            traceMsg(comp(), "    rejecting loop header block_%d\n", mergeNode->getNumber());
         children->pop();
         continue;
         }
      if (hasIVUpdate(mergeNode))
         {
         if (trace())
            traceMsg(comp(), "    reject merge block_%d: IV update\n", mergeNode->getNumber());
         children->pop();
         continue;
         }
      if (hasLoopAsyncCheck(mergeNode))
         {
         if (trace())
            traceMsg(comp(), "    reject merge block_%d: asynccheck\n", mergeNode->getNumber());
         children->pop();
         continue;
         }

      if (mergeNode->getNumber() == skipBlock)
         {
         if (trace())
            traceMsg(comp(), "     rejecting block_%d, magic skip block found\n", mergeNode->getNumber());
         children->pop();
         continue;
         }

      if (mergeNode->getFrequency() > 0)
         hasNonZeroFrequencies = true;

      if (disableSynergy() && comp()->isOptServer() && !mergeNode->isEmptyBlock())
         {
         if(trace())
            traceMsg(comp(), "    opt server mode for mergeNode %d\n", mergeNode->getNumber());
         // very restrictive block splitting
         if (mergeNode->getNumberOfRealTreeTops()>1)
            {
            children->pop();
            if(trace())
               traceMsg(comp(), "    rejecting because mergeNode %d has more than 1 treetop\n", mergeNode->getNumber());
            continue;
            }

         bool allSimplePreds = true;

         for (auto edge = mergeNode->getPredecessors().begin(); edge != mergeNode->getPredecessors().end(); ++edge)
            {
            node = (*edge)->getFrom();
            TR::Block *predBlock = toBlock(node);
            if(predBlock->getEntry() &&
               (predBlock->getFrequency() >= splitFrequency) &&
               predBlock->getNumberOfRealTreeTops()>4)
               {
               allSimplePreds = false;
               if(trace())
                  traceMsg(comp(), "    rejecting because predecessor block %d has more than 4 treetops\n", predBlock->getNumber());
               break;
               }
            }
         if (!allSimplePreds)
            {
            children->pop();
            continue;
            }
         }

      // This is a valid merge block. Put it into the list of merge nodes,
      // ordered by frequency.
      //
      if (!mergeNode->isEmptyBlock())
         {
         if(trace())
            traceMsg(comp(), "    adding merge block_%d to heap\n", mergeNode->getNumber());
         TR_IndexedBinaryHeapElement* temp = new (trStackMemory()) TR_IndexedBinaryHeapElement(mergeNode, index++);
         heap->add(temp);
         }
      children->pop();
      }
   heapElementQuickSort((TR_Array<TR_IndexedBinaryHeapElement*>*)heap, 0, heap->lastIndex());
   if (trace())
      {
      traceMsg(comp(), "  Finished search for merge points\n");
      heap->dumpList(comp());
      }

   if (!hasNonZeroFrequencies)
      {
      if(trace())
         traceMsg(comp(), "Terminating: found merge blocks with nonzero frequency\n");
      return 0;
      }


   // Process the hottest merge block iteratively until it's time to stop
   //
   TR_Array<TR_IndexedBinaryHeapElement*>* array = (TR_Array<TR_IndexedBinaryHeapElement*>*)heap;
   TR_LinkHeadAndTail<BlockMapper> newToOldMapping;
   newToOldMapping.set(0,0);
   int32_t horizontalCount = 0;
   for (int32_t i = 0; i <= array->lastIndex(); ++i)
      {
      mergeNode = (*array)[i]->getObject();

      // Find the hottest predecessor, it will take the new copy of the merged
      // block.
      //
      // At low opt levels the edge is carried from one mergeNode to the next
      // this is EVIL and a BAD DESIGN, but has been in for a long time and
      // NULLing unconditionally at warm degrades DT3
      // TODO: remove this if and make the NULL unconditional
      if (disableSynergy())
         splitPred_to_mergeNode_edge = NULL;
      int32_t predFrequency = 0;
      int32_t predEdgeFrequency = 0;
      TR::Block *splitPred = NULL;
      for (auto edge = mergeNode->getPredecessors().begin(); edge != mergeNode->getPredecessors().end(); ++edge)
         {
         node = (*edge)->getFrom();
         if (trace())
            traceMsg(comp(), "Consider %d -> %d\n", toBlock(node)->getNumber(), mergeNode->getNumber());
         predFrequency += node->getFrequency();
         predEdgeFrequency += (*edge)->getFrequency();
         if(toBlock(node)->getEntry() &&
            toBlock(node)->getExit()->getPrevRealTreeTop() &&
            (toBlock(node)->getExit()->getPrevRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets() && toBlock(node)->getExit()->getPrevRealTreeTop()->getNode()->getOpCode().hasBranchChildren()))
            continue;
         if (toBlock(node)->getNumber() == mergeNode->getNumber())
            continue;

         if (toBlock(node)->getEntry())
            {
            TR::Node* lastRealNode = toBlock(node)->getExit()->getPrevRealTreeTop()->getNode();
            //check to make sure we aren't about to chose a block that is reached as the target
            //of a virtual guard that we are not allowed to reverse, if this is the case we must chose someone else
            if (lastRealNode->getOpCode().isIf() &&
                lastRealNode->isTheVirtualGuardForAGuardedInlinedCall() &&
                lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber() == mergeNode->getNumber())
               continue;
            //check to make sure we aren't about to choose a block that is the target of
            //a GCR counting guard which could be patched, we are not allowed to reverse these either and must choose someone else
            if (lastRealNode->getOpCode().isIf()
                && lastRealNode->getFirstChild()->getOpCodeValue() == TR::iload
                && lastRealNode->getFirstChild()->getSymbolReference()
                && lastRealNode->getFirstChild()->getSymbolReference()->getSymbol()
                && lastRealNode->getFirstChild()->getSymbolReference()->getSymbol()->isCountForRecompile()
                && lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber() == mergeNode->getNumber())
               continue;
            }

         if (!splitPred || splitPred->getFrequency() < node->getFrequency())
             {
             splitPred = toBlock(node);
             splitPred_to_mergeNode_edge = *edge;
             if (trace())
                traceMsg(comp(), "mergeNode(%d) splitPred(%d) blockfreq = %d edgefreq = %d candidate\n",mergeNode->getNumber(),splitPred->getNumber(),splitPred->getFrequency(),(*edge)->getFrequency());
             }
         }

      // Make sure there is a candidate predecessor and that it is hot enough
      //
      if (!splitPred ||
          (splitPred->getFrequency() < splitPredFrequency) ||
          !splitPred_to_mergeNode_edge ||
          // TODO: the divide by 10 here is because edge frequencies are much lower than
          // block frequencies because of frequency maintenance issues elsewhere, it can
          // be removed once all frequencies are properly normalized
          (splitPred_to_mergeNode_edge->getFrequency() < (splitPredFrequency / 10)) ||
          (predFrequency <= 0) ||
          (predEdgeFrequency <= 0))
         {
         if (trace())
            traceMsg(comp(), "Merge block_%d has no suitable predecessor\n", mergeNode->getNumber());
         continue;
         }

      if (!performTransformation(comp(), "%sSplitting merge block_%d for predecessor %d (frequency %d)\n", optDetailString(), mergeNode->getNumber(), splitPred->getNumber(), mergeNode->getFrequency()))
         continue;

      //initialize the block mapper and push the merge node on to the top
      TR_LinkHeadAndTail<BlockMapper> bMap;
      bMap.set(0,0);
      TR::Block * to = new (trHeapMemory()) TR::Block(*mergeNode, TR::TreeTop::create(comp(), NULL), TR::TreeTop::create(comp(), NULL));
      to->getEntry()->join(to->getExit());
      if (bMap.getLast())
         bMap.getLast()->_to->getExit()->join(to->getEntry());
      bMap.append(new (trStackMemory()) BlockMapper(mergeNode, to));

      TR::Block *target = mergeNode;
      int tollerance = mergeNode->getFrequency() / 2;

      int32_t depth = maxDepth;
      while (depth > 1)
         {
         if (trace())
            traceMsg(comp(), "  Processing block_%d, depth budget %d\n", target->getNumber(), depth);

         TR::Block *child;
         int32_t startDepth = depth;
         TR::Node* lastRealNode = target->getExit()->getPrevRealTreeTop()->getNode();
         for (auto edge = target->getSuccessors().begin(); edge != target->getSuccessors().end(); ++edge)
            {
            child = toBlock((*edge)->getTo());

            //Editorial Comment, the parent check is an expensive check so leave it until last
            //we want to skip as many checks as we can, so check the frequency early

            if (!child->getEntry())
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: has no trees\n", child->getNumber());
               continue;
               }

            if (child->getFrequency() <= (target->getFrequency() - tollerance))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: frequency is too low (%d < %d)\n", child->getNumber(), child->getFrequency(), (target->getFrequency() - tollerance));
               continue;
               }

            if (child->hasExceptionPredecessors())
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: catch block\n", child->getNumber());
               continue;
               }

            if (!(child->getPredecessors().size() == 1))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: merge block -- we'll process this separately\n", child->getNumber());
               continue;
               }

            // if we have a cloned node, structure questions need to be asked about the original node
            // so perform the mapping here
            TR::Block *toCheck = child;
            for (BlockMapper* itr = newToOldMapping.getFirst(); itr; itr = itr->getNext())
               {
               if (itr->_to == child)
                  {
                  toCheck = itr->_from;
                  break;
                  }
               }

            if (isLoopHeader(toCheck))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: loop header\n", child->getNumber());
               continue;
               }

            if (hasIVUpdate(toCheck))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: IV update\n", child->getNumber());
               continue;
               }

            if (hasLoopAsyncCheck(toCheck))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: asynccheck\n", child->getNumber());
               continue;
               }

            if (isExitEdge(target, toCheck))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: outside of loop\n", child->getNumber());
               continue;
               }

            if (containCycle(child, &bMap))
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: would cause cycle in block mapper\n", child->getNumber());
               continue;
               }

            //finally check to make sure we aren't about to chose a block that is reached as the target
            //of a virtual guard that we are not allowed to reverse, if this is the case we must stop here
            if (lastRealNode->getOpCode().isIf() &&
                lastRealNode->isTheVirtualGuardForAGuardedInlinedCall() &&
                lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber() == child->getNumber())
               {
               if (trace())
                  traceMsg(comp(), "    Reject successor block_%d: virtual guard branch that can't be reversed\n", child->getNumber());
               continue;
               }

            //if we reach this point, we passed all the checks so we are good to split through the
            //child we are checking
            if (trace())
               traceMsg(comp(), "  Split through successor block_%d\n", child->getNumber());

            //create an empty block ready to be filled by the cloner and add it to the block mapper
            bMap.append(new (trStackMemory()) BlockMapper(child, NULL));

            target = child;
            --depth;
            break;
            }

         //see if we are done because there isn't anywhere to go from here
         if (startDepth == depth)
            break;
         }

      //calculate synergy here
      if(pruneAndPopulateBlockMapper(&bMap, synergisticDepthCalculator(&bMap, splitPred)) > 0)
         {
         for (BlockMapper* itr = bMap.getFirst(); itr; itr = itr->getNext())
            newToOldMapping.append(new (trStackMemory()) BlockMapper(itr->_from, itr->_to));
         //we need to setup the mergeNode with the frequency for all the childern
         int16_t hotFreq = splitPred->getFrequency() * mergeNode->getFrequency() / predFrequency;
         int16_t coldFreq = mergeNode->getFrequency() - hotFreq;
         if (hotFreq < splitPred->getFrequency())
            {
            static const bool force = feGetEnv("TR_forceBlockSplitterFrequencyAdjustment") != NULL;
            if (force || splitPred->getSuccessors().size() == 1)
               {
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "blockSplitter.inconsistentFreqs/%s/(%s)/block_%d", comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(), mergeNode->getNumber()));
               hotFreq = splitPred->getFrequency();
               }
            }
         int16_t hotEdgeFreq = splitPred_to_mergeNode_edge->getFrequency();
         int16_t coldEdgeFreq = (predEdgeFrequency - splitPred_to_mergeNode_edge->getFrequency()) / predEdgeFrequency;
         if (coldFreq < 6)
            coldFreq = 6;

         dumpBlockMapper(&bMap);
         splitBlock(splitPred, &bMap);

         if (trace())
            traceMsg(comp(),"splitPred=%d, mergeNode=%d\n",splitPred->getNumber(),mergeNode->getNumber());

         // Update block frequencies
         if (trace())
            {
            traceMsg(comp(), "  New block frequencies are cold: %d hot: %d\n", coldFreq, hotFreq);
            traceMsg(comp(), "  New edge frequencies are cold: %d hot: %d\n", coldEdgeFreq, hotEdgeFreq);
            }
         for (BlockMapper* itr = bMap.getFirst(); itr; itr = itr->getNext())
            {
            if (trace())
               traceMsg(comp(), "    setting block_%d cold and block_%d hot\n", itr->_from->getNumber(), itr->_to->getNumber());

            int32_t origFreq = itr->_from->getFrequency();

            if (trace())
               traceMsg(comp(),"block %d origFreq = %d ",itr->_from->getNumber(),origFreq);

            itr->_from->setFrequency(coldFreq);

            // set frequencies for incoming/outcoming edges to/from the block itr->_from
            if (origFreq > 0)
               {
               TR::CFGEdgeList & predecessors = itr->_from->getPredecessors();
               for (auto predEdge = predecessors.begin(); predEdge != predecessors.end(); ++predEdge)
                  {
                  int32_t freq = (coldFreq*(*predEdge)->getFrequency())/origFreq;
                  if (!(*predEdge)->getFrom()->asBlock()->isCold() &&
                      !(*predEdge)->getTo()->asBlock()->isCold() &&
                      (freq <= MAX_COLD_BLOCK_COUNT))
                     freq = MAX_COLD_BLOCK_COUNT + 1;
                  else if ((*predEdge)->getFrom()->asBlock()->isCold() &&
                           (*predEdge)->getTo()->asBlock()->isCold() &&
                           freq > MAX_COLD_BLOCK_COUNT)
                     freq = MAX_COLD_BLOCK_COUNT;

                  if (trace())
                     traceMsg(comp(),"predEdge freq=%d. set it to %d instead\n",(*predEdge)->getFrequency(),freq);

                  (*predEdge)->setFrequency(freq);
                  }

               TR::CFGEdgeList & successors = itr->_from->getSuccessors();
               for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
                  {
                  int32_t freq = (coldFreq*(*succEdge)->getFrequency())/origFreq;
                  if (!(*succEdge)->getFrom()->asBlock()->isCold() &&
                      !(*succEdge)->getTo()->asBlock()->isCold() &&
                      (freq <= MAX_COLD_BLOCK_COUNT))
                     freq = MAX_COLD_BLOCK_COUNT + 1;
                  else if ((*succEdge)->getFrom()->asBlock()->isCold() &&
                           (*succEdge)->getTo()->asBlock()->isCold() &&
                           freq > MAX_COLD_BLOCK_COUNT)
                     freq = MAX_COLD_BLOCK_COUNT;

                  if (trace())
                     traceMsg(comp(),"succEdge freq=%d. set it to %d instead\n",(*succEdge)->getFrequency(),freq);

                  (*succEdge)->setFrequency(freq);
                  }

               if (itr->_from->getPredecessors().size() == 1)
                  {
                  TR::CFGEdge * predEdge = itr->_from->getPredecessors().front();
                  if (trace())
                     traceMsg(comp(),"single predEdge freq=%d. set it to %d instead\n",predEdge->getFrequency(),coldEdgeFreq);
                  predEdge->setFrequency(coldEdgeFreq);
                  }

               if (itr->_from->getSuccessors().size() == 1)
                  {
                  TR::CFGEdge * succEdge = itr->_from->getSuccessors().front();
                  if (trace())
                     traceMsg(comp(),"single succEdge freq=%d. set it to %d instead\n",succEdge->getFrequency(),coldEdgeFreq);
                  succEdge->setFrequency(coldEdgeFreq);
                  }
               }

            itr->_to->setFrequency(hotFreq);

            // possibly update frequencies for incoming/outcoming edges to/from the block itr->_to
            if (itr->_to->getPredecessors().size() == 1)
               {
               if (trace())
                  traceMsg(comp(),"predEdge freq=%d. set it to %d instead\n",itr->_to->getPredecessors().front()->getFrequency(),hotEdgeFreq);
               itr->_to->getPredecessors().front()->setFrequency(hotEdgeFreq);
               }

            if (itr->_to->getSuccessors().size() == 1)
               {
               if (trace())
                  traceMsg(comp(),"succEdge freq=%d. set it to %d instead\n",itr->_to->getSuccessors().front()->getFrequency(),hotEdgeFreq);
               itr->_to->getSuccessors().front()->setFrequency(hotEdgeFreq);
               }
            }

         // Add the split block back into the list of merge blocks if it is still
         // a candidate
         //
         if (!(mergeNode->getPredecessors().size() == 1) &&
              mergeNode->getFrequency() >= splitFrequency &&
              horizontalCount < 2)
            {
            ++i;
            ++horizontalCount;
            }
         else if (horizontalCount >= 2)
            {
            horizontalCount = 0;
            }
         }
       }

   return 1;
   }

void TR_BlockSplitter::heapElementQuickSort(TR_Array<TR_IndexedBinaryHeapElement*>* array, int32_t left, int32_t right)
   {
   if (right - left > 1)
      {
      uint32_t middle = (left + right) / 2;
      //use median of 3 pivot selection, place pivot on the right
      if ((*array)[middle]->indexLT((*array)[left]))
         quickSortSwap(array, left, middle);
      if ((*array)[right]->indexLT((*array)[left]))
         quickSortSwap(array, left, right);
      if ((*array)[right]->indexLT((*array)[middle]))
         quickSortSwap(array, middle, right);
      quickSortSwap(array, middle, right - 1);
      TR_IndexedBinaryHeapElement* pivot = (*array)[right - 1];
      int32_t i = left;
      int32_t j = right-1;
      while (true)
         {
         while ((*array)[++i]->indexLT(pivot)) { }
         while (pivot->indexLT((*array)[--j])) { }
         if (i < j)
            quickSortSwap(array, i, j);
         else
            break;
         }
      quickSortSwap(array, i, right - 1);
      if ( left < i )
         heapElementQuickSort(array, left, i-1);
      if (right > i )
         heapElementQuickSort(array, i+1, right);
      }
   else if (right - left == 1)
      {
      if ((*array)[left]->indexGT((*array)[right]))
         quickSortSwap(array, left, right);
      }
   }


void TR_BlockSplitter::quickSortSwap(TR_Array<TR_IndexedBinaryHeapElement*>* array, int32_t left, int32_t right)
   {
   TR_IndexedBinaryHeapElement* temp;
   temp = (*array)[left];
   (*array)[left] = (*array)[right];
   (*array)[right] = temp;
   }

int32_t TR_BlockSplitter::pruneAndPopulateBlockMapper(TR_LinkHeadAndTail<BlockMapper>* bMap, int32_t depth)
   {
   int32_t cloned = 0;
   if (depth)
      {
      if (trace())
         {
         for (BlockMapper* itr = bMap->getFirst(); itr; itr = itr->getNext())
            {
            traceMsg(comp(), "prune bMap iterator, from 0x%p to 0x%p\n", itr->_from, itr->_to);
            }
         }

      BlockMapper* itr = bMap->getFirst();
      while(depth > 0 && itr->getNext())
         {
         itr = itr->getNext();
         --depth;
         }
      bMap->set(bMap->getFirst(), itr);
      itr->setNext(NULL);

      TR::TreeTop *prevTreeTop = NULL;
      for(itr = bMap->getFirst(); itr; itr = itr->getNext())
         {
         ++cloned;
         if (trace())
            traceMsg(comp(), "prune bMap iterator for join, from 0x%p to 0x%p\n", itr->_from, itr->_to);

         itr->_to = new (trHeapMemory()) TR::Block(*itr->_from, TR::TreeTop::create(comp(), NULL), OMR::TreeTop::create(comp(), NULL));
         itr->_to->getEntry()->join(itr->_to->getExit());
         if (prevTreeTop)
            prevTreeTop->join(itr->_to->getEntry());
         prevTreeTop = itr->_to->getExit();
         }
      }
   if (trace())
      traceMsg(comp(), "  pruneAndPopulateBlockMapper returning depth of %d\n", cloned);
   return cloned;
   }

int32_t TR_BlockSplitter::synergisticDepthCalculator(TR_LinkHeadAndTail<BlockMapper>* bMap, TR::Block * startPoint)
   {
   //Step 1 - find the start of the preamble
   TR_Stack<TR::Block *> preamble = TR_Stack<TR::Block *>(trMemory());
   preamble.push(startPoint);

   if (trace())
      traceMsg(comp(), "  Starting synergisticDepthCalculator\n");

   if (!startPoint->getPredecessors().empty())
      {
      TR::CFGEdge * itr;
      for ( itr = startPoint->getPredecessors().front();
            toBlock(itr->getFrom())->getEntry() &&
            (itr->getFrom()->getPredecessors().size() == 1) &&
            (toBlock(itr->getFrom())->hasExceptionPredecessors() == false);
            itr = itr->getFrom()->getPredecessors().front()
          )
          {
          if (trace())
             traceMsg(comp(), "preamble.push from %d to %d\n", itr->getFrom()->getNumber(), itr->getTo()->getNumber());
          preamble.push(toBlock(itr->getFrom()));
          }
          preamble.push(toBlock(itr->getFrom()));
       }

   //print out the preamble if we are printing opt trace
   if (trace())
      {
      traceMsg(comp(), "    Find synergy in preamble blocks");
      for (int32_t i = preamble.lastIndex(); i >= 0; --i)
         traceMsg(comp(), " %d", preamble[i]->getNumber());
      traceMsg(comp(), "\n");
      }

   //Step 2 - process the preamble to preload the symbol information
   comp()->incVisitCount();
   TR_Array<uint32_t> synergyIndices(trMemory());
   while (!preamble.isEmpty())
      {
      TR::Block * blockItr = preamble.pop();
      if (!blockItr->getEntry())
         continue;
      for (TR::TreeTop* treeTopItr = blockItr->getEntry(); treeTopItr && treeTopItr != blockItr->getExit()->getNextTreeTop(); treeTopItr = treeTopItr->getNextTreeTop())
         {
         processNode(treeTopItr->getNode(), 1, &synergyIndices);
         }
      }

   //Step 3 - process each item in the BlockMapper to calculate synergies
   if (trace())
      traceMsg(comp(), "    Find synergy in blocks being split\n");
   comp()->incVisitCount();
   TR_Array<Synergy> synergies(trMemory());
   int32_t blockIndex = 2;
   uint16_t cost = 0;
   for (BlockMapper* itr = bMap->getFirst(); itr; itr = itr->getNext())
      {
      for (TR::TreeTop* treeTopItr = itr->_from->getEntry(); treeTopItr && treeTopItr != itr->_from->getExit()->getNextTreeTop(); treeTopItr = treeTopItr->getNextTreeTop())
         {
         cost += processNode(treeTopItr->getNode(), blockIndex, &synergyIndices, &synergies);
         }
      synergies[blockIndex].replicationCost = cost;
      synergies[blockIndex].blockFrequency  = itr->_from->getFrequency();
      cost = 0;
      ++blockIndex;
      }

   //Step 4 - post-process the synergy data and pick the optimum to return
   uint32_t i;
   for (i = 2; i < synergies.size(); ++i)
      {
      synergies[i].upwardSynergy += synergies[i-1].upwardSynergy;
      synergies[i].replicationCost += synergies[i-1].replicationCost;
      synergies[synergies.lastIndex()-i].downwardSynergy += synergies[synergies.size()-i].downwardSynergy;
      }

   dumpSynergies(&synergies);

   // at warm and below we use a very conservative splitting based on the designs
   // in compjazz 42085 and 39028, but at hot and above we want the full power of
   // blocksplitter so we do our full synergy calculation
   int32_t newDepth;
   if (disableSynergy())
      {
      newDepth = synergies.size()-1;
      for (i = 2; i < synergies.size(); ++i)
         {
         if (synergies[synergies.size()-i].downwardSynergy > 0)
            {
            newDepth = i-1;
            break;
            }
         }
      }
   else
      {
      double bestScore = 0;
      uint16_t bestCost = 0;
      newDepth = 0;
      for (i = 2; i < synergies.size(); ++i)
         {
         double score = calculateBlockSplitScore(synergies[i]);
         if (score < 0)
            continue;

         // alpha represents a ratio of the units of synergy per number of nodes
         // so 1/8 means 1 unit of synergy is worth 8 cloned nodes
         // we always want to push our split as far as possible so once we find the
         // optimal point we can split up to denominator - 1 extra nodes to try and
         // push the merge as far down as possible which is what the below calcualtions
         // represent
         //
         // a scoreDelta >= 0 means we have an equally good solution so take it
         // a scoreDelta between 0 and -1 means the drop is due to extra nodes copied
         // so allow up to one alpha's worth of cost to push the merge point further down
         double scoreDelta = score - bestScore;
         uint16_t costDelta = synergies[i].replicationCost - bestCost;
         if (scoreDelta >= 0)
            {
            bestScore = score;
            bestCost = synergies[i].replicationCost;
            newDepth  = i-1;
            }
         else if (scoreDelta < 0 && scoreDelta >= -1 && (alpha * (double)costDelta) < 1)
            {
            newDepth = i-1;
            }
         }
      }

   if(trace())
      traceMsg(comp(), "  Suggested new depth is %d\n", newDepth);
   return newDepth;
   }

int32_t TR_BlockSplitter::processNode(TR::Node* node, int32_t blockIndex, TR_Array<uint32_t>* synergyIndices, TR_Array<Synergy>* synergies)
   {
   node->setVisitCount(comp()->getVisitCount());
   // splitting through a goto is free because the goto will disappear - otherwise the node counts
   int32_t cloneSize = 1;
   if (node->getOpCode().isGoto() || node->getOpCodeValue() == TR::BBStart
       || node->getOpCodeValue() == TR::BBEnd)
      cloneSize = 0;
   if (node->getOpCode().hasSymbolReference() && (node->getOpCode().isLoad() || node->getOpCode().isStore()))
      {
      int32_t symbolIndex = node->getSymbolReference()->getReferenceNumber() - comp()->getSymRefTab()->getNumHelperSymbols();
      if (synergies && (node->getOpCode().isLoad() || node->getOpCode().isStore()))
         {
         int32_t previousDefinition = (*synergyIndices)[symbolIndex];
         if (previousDefinition && previousDefinition != blockIndex)
            {
            if (trace())
               traceMsg(comp(), "      Synergy on #%d for [%p]\n", node->getSymbolReference()->getReferenceNumber(), node);
            ++(*synergies)[previousDefinition].downwardSynergy;
            ++(*synergies)[blockIndex].upwardSynergy;
            }
         }
         (*synergyIndices)[symbolIndex] = blockIndex;
      }
   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      if (node->getChild(i)->getVisitCount() != comp()->getVisitCount())
         {
         cloneSize += processNode(node->getChild(i), blockIndex, synergyIndices, synergies);
         }
      }
   return cloneSize;
   }

double TR_BlockSplitter::calculateBlockSplitScore(Synergy& synergy)
   {
   int methodFrequency = comp()->getFlowGraph()->getStart()->getFrequency();
   if (methodFrequency == 0)
      {
      methodFrequency = 1;
      }
   return ((double)(synergy.upwardSynergy - synergy.downwardSynergy))
           - ( alpha * (double)synergy.replicationCost );
   }

void TR_BlockSplitter::dumpSynergies(TR_Array<Synergy>* toDump)
   {
   if (trace())
      {
      traceMsg(comp(), "  Synergy results:\n    Score     Up     Down   Cost   Frequency\n");
      for (uint32_t i = 2; i < toDump->size(); ++i)
         traceMsg(comp(), "    %-9.3f %-6d %-6d %-6d %d\n", calculateBlockSplitScore((*toDump)[i]),
                                            (*toDump)[i].upwardSynergy,   (*toDump)[i].downwardSynergy,
                                            (*toDump)[i].replicationCost, (*toDump)[i].blockFrequency);
      }
   }

void TR_BlockSplitter::dumpBlockMapper(TR_LinkHeadAndTail<BlockMapper>* bMap)
   {
   if (trace())
      {
      for (BlockMapper* itr = bMap->getFirst(); itr; itr = itr->getNext())
         {
         if (itr == bMap->getFirst())
            traceMsg(comp(), "    Splitting block_%d for %s", itr->_from->getNumber(), comp()->signature());
         else if (itr == bMap->getFirst()->getNext())
            traceMsg(comp(), "\n      Splitting additional block(s): %d", itr->_from->getNumber());
         else
            traceMsg(comp(), " %d", itr->_from->getNumber());
         }
      traceMsg(comp(), "\n");
      }
   }

bool TR_BlockSplitter::containCycle(TR::Block *blk, TR_LinkHeadAndTail<BlockMapper>* bMap)
   {
   for (auto edge = blk->getSuccessors().begin(); edge != blk->getSuccessors().end(); ++edge)
      {
      TR::Block *succBlk = toBlock((*edge)->getTo());
      for (BlockMapper* itr = bMap->getFirst(); itr; itr = itr->getNext())
         {
         if (itr->_from->getNumber() == succBlk->getNumber())
            return true;
         }
      }
   return false;
   }

bool TR_BlockSplitter::isLoopHeader(TR::Block * block)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   // See if this block is a loop header
   //
   TR_RegionStructure *parent;
   for (parent = getParentStructure(block); parent; parent = parent->getParent() ? parent->getParent()->asRegion() : NULL)
      {
      if (parent->getNumber() == block->getNumber() &&
          (parent->isNaturalLoop() || parent->containsInternalCycles()))
         break;
      }
   // if parent is not null we broke at a loop header
   return parent != NULL;
   }

bool TR_BlockSplitter::isExitEdge(TR::Block *mergeNode, TR::Block *successor)
   {
   if (trace())
      traceMsg(comp(), "    considering isExit on %d and %d\n", mergeNode->getNumber(), successor->getNumber());
   TR_RegionStructure *parent = getParentStructure(mergeNode);

   if (!parent || !parent->isNaturalLoop())
      return false;

   if (trace())
      traceMsg(comp(), "    parent region is %p (%d) and isNaturalLoop is %d\n", parent, parent->getNumber(), (parent ? parent->isNaturalLoop() : 0));

   TR_RegionStructure *child = getParentStructure(successor);
   if (trace())
      traceMsg(comp(), "    child region is %p\n", child);
   return parent != child;
   }

bool TR_BlockSplitter::hasIVUpdate(TR::Block *block)
   {
   TR_RegionStructure *parent = getParentStructure(block);
   if (getLastRun() || comp()->getProfilingMode() == JitProfiling || !parent || !parent->isNaturalLoop())
      return false;

   if (trace())
      traceMsg(comp(), "   checking for IVUpdate in block_%d\n", block->getNumber());
   for (OMR::TreeTop* treeTopItr = block->getEntry(); treeTopItr && treeTopItr != block->getExit()->getNextTreeTop(); treeTopItr = treeTopItr->getNextTreeTop())
      {
      if (treeTopItr->getNode()->getOpCode().isStoreDirect()
          && treeTopItr->getNode()->getOpCode().hasSymbolReference()
          && (treeTopItr->getNode()->getFirstChild()->getOpCode().isAdd()
              || treeTopItr->getNode()->getFirstChild()->getOpCode().isSub())
          && (treeTopItr->getNode()->getFirstChild()->getFirstChild()->getOpCode().isLoadConst()
              || treeTopItr->getNode()->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
          && ((treeTopItr->getNode()->getFirstChild()->getFirstChild()->getOpCode().isLoadVarDirect()
               && treeTopItr->getNode()->getFirstChild()->getFirstChild()->getSymbolReference()->getSymbol() == treeTopItr->getNode()->getSymbolReference()->getSymbol())
              || (treeTopItr->getNode()->getFirstChild()->getSecondChild()->getOpCode().isLoadVarDirect()
               && treeTopItr->getNode()->getFirstChild()->getSecondChild()->getSymbolReference()->getSymbol() == treeTopItr->getNode()->getSymbolReference()->getSymbol()))
          )
         {
         if (trace())
            traceMsg(comp(), "    treetop %p has IVUpdate\n", treeTopItr->getNode());
         return true;
         }
      }
   if (trace())
      traceMsg(comp(), "    no IVUpdate found\n");
   return false;
   }

bool TR_BlockSplitter::hasLoopAsyncCheck(TR::Block *block)
   {
   TR_RegionStructure *parent = getParentStructure(block);
   if (getLastRun() || comp()->getProfilingMode() == JitProfiling || !parent || !parent->isNaturalLoop())
      return false;

   if (trace())
      traceMsg(comp(), "   checking for loopAsyncCheck in block_%d\n", block->getNumber());
   for (OMR::TreeTop* treeTopItr = block->getEntry(); treeTopItr && treeTopItr != block->getExit()->getNextTreeTop(); treeTopItr = treeTopItr->getNextTreeTop())
      {
      if (treeTopItr->getNode()->getOpCodeValue() == TR::asynccheck)
         {
         if (trace())
            traceMsg(comp(), "    treetop %p is asncycheck\n", treeTopItr->getNode());
         return true;
         }
      }
   return false;
   }


TR::Block *TR_BlockSplitter::splitBlock(TR::Block *pred, TR_LinkHeadAndTail<BlockMapper>* bMap)
   {
   TR::Block *candidate = bMap->getFirst()->_from;
   TR::Block *target = bMap->getLast()->_from;

   TR::CFG *cfg = comp()->getFlowGraph();

   // Structure is killed when we start cloning blocks
   //
   cfg->setStructure(NULL);

   // Split the block by cloning
   //
   TR_BlockCloner cloner(cfg, false, true);
   if (trace())
      traceMsg(comp(), "  about to clone %d to %d\n", candidate->getNumber(), target->getNumber());
   TR::Block *cloneStart = cloner.cloneBlocks(bMap);
   TR::Block *cloneEnd   = cloner.getLastClonedBlock();

   if (trace())
      {
      traceMsg(comp(), "  CLONE from block_%d to block_%d\n", cloneStart->getNumber(), cloneEnd->getNumber());
      if (target->getEntry() && target->getNextBlock())
         traceMsg(comp(), "    target next real block_%d\n", target->getNextBlock()->getNumber());
      }

   cfg->addEdge(pred, cloneStart);
   cfg->removeEdge(pred, candidate);


   // Insert the newly-split block after its predecessor
   //
   if(trace())
      {
      if (pred)
         traceMsg(comp(), "  join %d to %d\n", pred->getNumber(), cloneStart->getNumber());
      if (pred && pred->getNextBlock() && pred->getNextBlock()->getEntry())
         traceMsg(comp(), "  join %d to %d\n", cloneEnd->getNumber(), pred->getNextBlock()->getNumber());
      }

   //if we have a valid predecessor we just add the new code as normal
   //if the pred has a null entry we have the dummy start so we must set the new start location
   //for the program and update the trees accordingly
   //
   if (pred->getEntry())
      {
      cloneEnd->getExit()->join(pred->getExit()->getNextTreeTop());
      pred->getExit()->join(cloneStart->getEntry());
      //fix-up predecessor
      TR::Node* lastRealNode = pred->getExit()->getPrevRealTreeTop()->getNode();
      if (lastRealNode->getOpCode().isBranch())
         {
         if (lastRealNode->getOpCode().isIf())
            {
            if (lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber() == candidate->getNumber())
               {
               lastRealNode->reverseBranch(cloneEnd->getExit()->getNextTreeTop());
               if (trace())
                  traceMsg(comp(), "  Reversing branch, node %d now jumps to block_%d\n", pred->getNumber(), lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber());
               //this check handles splitting through a loop header in a region where we have not detected a valid loop header
               if (bMap->getLast()->_from->getNumber() == pred->getNumber())
                  {
                  lastRealNode = bMap->getLast()->_to->getExit()->getPrevRealTreeTop()->getNode();
                  lastRealNode->reverseBranch(cloneEnd->getExit()->getNextTreeTop());
                  if (trace())
                     traceMsg(comp(), "    Also reversing cloned branch, node %d now jumps to block_%d\n", bMap->getLast()->_to->getNumber(), lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber());
                  }
               }
            }
         else if (lastRealNode->getOpCode().isGoto())
            {
            TR::TransformUtil::removeTree(comp(), pred->getExit()->getPrevRealTreeTop());
            }
         }
      }
   else
      {
      cloneEnd->getExit()->join(candidate->getEntry());
      cloneStart->getEntry()->setPrevTreeTop(NULL);
      cfg->comp()->setStartTree(cloneStart->getEntry());
      }


   // Fix up the trees for the new split block to branch to any successor that
   // was a fall-through from the original candidate
   for (BlockMapper* itr = bMap->getFirst(); itr; itr = itr->getNext())
      {
      if (itr->_to != cloneEnd)
         continue;

      TR::Node* lastRealNode = itr->_to->getExit()->getPrevRealTreeTop()->getNode();
      TR::TreeTop* nextTree = itr->_from->getExit()->getNextTreeTop();
      if (
           lastRealNode->getOpCode().isBranch() &&
           lastRealNode->getOpCode().isIf()
         )
         {
          //if we are at the end of the clone we have to get back to the origional execution stream so fall through
          //to a goto
          //
          int32_t newFreq = nextTree->getNode()->getBlock()->getFrequency();
          if (newFreq > cloneEnd->getFrequency())
             newFreq = cloneEnd->getFrequency();

          TR::Block *newBlock = TR::Block::createEmptyBlock(lastRealNode, comp(), newFreq, nextTree->getNode()->getBlock());
          if (nextTree->getNode()->getBlock()->isCold() || cloneEnd->isCold())
             newBlock->setIsCold(true);
          newBlock->getExit()->join(itr->_to->getExit()->getNextTreeTop());
          itr->_to->getExit()->join(newBlock->getEntry());
          cfg->addNode(newBlock);

          newBlock->append(TR::TreeTop::create(comp(), TR::Node::create(lastRealNode, TR::Goto, 0, nextTree)));
          cfg->addEdge(cloneEnd, newBlock);
          cfg->addEdge(newBlock, nextTree->getNode()->getBlock());
          // The branch target may be the fall-through path. If so, since we're redirecting the fall-through path to a goto block
          // and removing the single edge that represents both the fall-through and branch paths, we should also redirect the branch
          // to the goto block.
          if (lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber() == nextTree->getNode()->getBlock()->getNumber())
             {
             if (trace())
                traceMsg(comp(), "   Redirecting branch %d->%d to %d\n", cloneEnd->getNumber(), nextTree->getNode()->getBlock()->getNumber(), newBlock->getNumber());
             lastRealNode->setBranchDestination(newBlock->getEntry());
             }
          cfg->removeEdge(cloneEnd, nextTree->getNode()->getBlock());

          if (trace())
             traceMsg(comp(), "   Create extra goto block_%d --> %d\n", newBlock->getNumber(), nextTree->getNode()->getBlock()->getNumber());
         }
      else if (
               !lastRealNode->getOpCode().isBranch() &&
               !lastRealNode->getOpCode().isReturn() &&
               !lastRealNode->getOpCode().isJumpWithMultipleTargets() &&
               lastRealNode->getOpCodeValue() != TR::athrow &&
               !(lastRealNode->getNumChildren() >= 1 && lastRealNode->getFirstChild()->getOpCodeValue() == TR::athrow)
              )
         {
         itr->_to->append(TR::TreeTop::create(comp(), TR::Node::create(lastRealNode, TR::Goto, 0, nextTree)));
         if (trace())
            traceMsg(comp(), "   Add goto %d --> %d\n", cloneEnd->getNumber(), nextTree->getNode()->getBlock()->getNumber());
         }
      }

   return cloneStart;
   }

const char *
TR_BlockSplitter::optDetailString() const throw()
   {
   return "O^O BLOCK SPLITTER: ";
   }

#include "il/SymbolReference.hpp"

void TR_InvariantArgumentPreexistence::ParmInfo::clear()
   {
   memset(this, 0, sizeof(*this));
   _knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   }

TR_InvariantArgumentPreexistence::TR_InvariantArgumentPreexistence(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   _success = false;
   }

int32_t TR_InvariantArgumentPreexistence::perform()
   {
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   TR_ResolvedMethod       *feMethod     = methodSymbol->getResolvedMethod();
   bool enableTrace = trace();

   if (comp()->mustNotBeRecompiled())
      {
      if (enableTrace)
         traceMsg(comp(), "PREX: Aborting preexistence because %s mustNotBeRecompiled\n", feMethod->signature(trMemory()));
      return 0;
      }

   static const char *disablePREX = feGetEnv("TR_disablePREX");
   if (!comp()->isRecompilationEnabled() || disablePREX || comp()->getOptions()->realTimeGC())
      return 0;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _peekingSymRefTab = comp()->getPeekingSymRefTab();
   _isOutermostMethod = ((comp()->getInlineDepth() == 0) && (!comp()->isPeekingMethod()));

   if (enableTrace)
      traceMsg(comp(), "PREX: Starting preexistence for %s\n", feMethod->signature(trMemory()));

   int32_t numParms = methodSymbol->getParameterList().getSize();
   _parmInfo = (ParmInfo*) trMemory()->allocateStackMemory(numParms * sizeof(ParmInfo));
   for (int i = 0; i < numParms; i++)
      _parmInfo[i].clear();

  // Now iterate over the trees, check if there is a store to any parm symbol that is in the
   // candidate list
   //
   TR::TreeTop *tt;
   int32_t numInvariantArgs = numParms;
   for (TR::TreeTopIterator iter(methodSymbol->getFirstTreeTop(), comp());
        iter != NULL && numInvariantArgs > 0;
        ++iter)
      {
      TR::Node *node = iter.currentTree()->getNode();
      if ((node->getOpCodeValue() == TR::treetop) ||
          (node->getOpCodeValue() == TR::NULLCHK))
         node = node->getFirstChild();

      if (node->getOpCode().isStoreDirect())
         {
         TR::Symbol *symbol = node->getSymbolReference()->getSymbol();
         if (symbol->isParm())
            {
            int32_t index = symbol->getParmSymbol()->getOrdinal();
            _parmInfo[index].setNotInvariant();
            --numInvariantArgs;
            if (enableTrace)
               traceMsg(comp(), "PREX:    Arg %d (%s) is not invariant\n", index, node->getSymbolReference()->getName(comp()->getDebug()));
            }
         }
      }

   TR::KnownObjectTable *knot = comp()->getKnownObjectTable();
   ListIterator<TR::ParameterSymbol> parms(&methodSymbol->getParameterList());
   for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext())
       {
       int32_t index = p->getOrdinal();
       ParmInfo &parmInfo = _parmInfo[index];
       if (p->getDataType() != TR::Address || !parmInfo.isInvariant())
          continue;

       int32_t len = 0;
       const char *sig = p->getTypeSignature(len);
       TR_OpaqueClassBlock *clazz = fe()-> getClassFromSignature(sig, len, feMethod);
       if (!clazz || TR::Compiler->cls.isClassArray(comp(), clazz))
          continue;

       if (_isOutermostMethod)
          {
          parmInfo.setClassIsPreexistent();
          }

       parmInfo.setSymbol(p);
       TR::SymbolReference* symRef = methodSymbol->getParmSymRef(p->getSlot());
       TR::KnownObjectTable::Index koi = symRef->getKnownObjectIndex();
       if ((koi != TR::KnownObjectTable::UNKNOWN)
           && knot
           && !comp()->isOutOfProcessCompilation()
          )
          {
          TR::VMAccessCriticalSection setClass(comp());
          TR_OpaqueClassBlock *fixedClazz = TR::Compiler->cls.objectClass(comp(), knot->getPointer(koi));
          parmInfo.setClassIsFixed();
          parmInfo.setClass(fixedClazz);
          if (enableTrace)
             traceMsg(comp(), "PREX:      parm %d is known object obj.%d\n", index, koi);
          }
       else
          {
          parmInfo.setClass(clazz);
          if (enableTrace)
             traceMsg(comp(), "PREX:      parm %d class %p is %.*s\n", index, clazz, len, sig);

          if (TR::Compiler->cls.isClassFinal(comp(), clazz))
             {
             if (enableTrace)
                traceMsg(comp(), "PREX:      parm %d class is final\n", index);
             parmInfo.setClassIsFixed();
             }
          else if (classIsCurrentlyFinal(clazz))
             {
             if (enableTrace)
                traceMsg(comp(), "PREX:      parm %d class is currently final\n", index);
             parmInfo.setClassIsCurrentlyFinal();
             }
          }
       }

   if (comp()->isPeekingMethod() && comp()->getCurrentPeekingArgInfo())
   //comp()->getCurrentPeekingArgInfo() is supplied only in TR_J9EstimateCodeSize::realEstimateCodeSize,
   //so TR_InvariantArgumentPreexistence can validate preexistence args and propagate info onto parmSymbols
      {
      TR_PeekingArgInfo *peekInfo = comp()->getCurrentPeekingArgInfo();
      if (enableTrace)
         traceMsg(comp(), "PREX:    Populating parmInfo of peeked method %s %p\n", feMethod->signature(trMemory()), peekInfo);

      if (peekInfo)
         {
         if (peekInfo->_method != feMethod)
            {
            TR_ASSERT(false, "Peeking info mismatches the method being analyzed");
            return 1; // on prod builds -- abort
            }

         const char **argInfo = peekInfo->_args;
         int32_t *lenInfo = peekInfo->_lengths;
         for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext())
            {
            int32_t index = p->getOrdinal();
            ParmInfo &parmInfo = _parmInfo[index];
            TR_OpaqueClassBlock *clazzFromMethod = parmInfo.getClass();
            if (argInfo && argInfo[index] && parmInfo.isInvariant() && !parmInfo.classIsCurrentlyFinal() && clazzFromMethod)
               {
               const char *sig = argInfo[index];
               int32_t len = lenInfo[index];
               TR_ASSERT(p->getDataType() == TR::Address, "non address argument cannot be of fixed ref type");

               TR_OpaqueClassBlock *clazz = fe()->getClassFromSignature(sig, len, feMethod);
               if (!clazz || clazz == clazzFromMethod)
                  {
                  if (!clazz && enableTrace)
                     traceMsg(comp(), "Can't obtain class for sig %.*s\n", len, sig);
                  else if (clazz == clazzFromMethod && enableTrace)
                     traceMsg(comp(), "class from peeking arg info is the same as class from method\n");
                  continue;
                  }

               // Peeking arg info is not compatible with the method, looking at dead path, bail out
               if (fe()->isInstanceOf(clazz, clazzFromMethod, true, true, true) != TR_yes)
                  {
                  if (enableTrace)
                     traceMsg(comp(), "Peeking arg info is not compatible with the method, bail out\n");
                  return 1;
                  }

               // Clear info from method parm symbol
               parmInfo.clear();
               parmInfo.setSymbol(p);
               parmInfo.setClassIsRefined();
               parmInfo.setClass(clazz);

               if (classIsCurrentlyFinal(clazz))
                  {
                  parmInfo.setClassIsCurrentlyFinal();
                  }
               }
            }
         }
      }
   else
      {
      TR_PrexArgInfo *argInfo = comp()->getCurrentInlinedCallArgInfo();

      if (argInfo)
         {
         if (enableTrace)
            traceMsg(comp(), "PREX:    Populating parmInfo of inlined method %s from argInfo %p\n", feMethod->signature(trMemory()), argInfo);

         for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext())
            {
            int32_t index = p->getOrdinal();
            TR_PrexArgument *arg = argInfo->get(index);
            ParmInfo &parmInfo = _parmInfo[index];
            TR_OpaqueClassBlock *clazzFromMethod = parmInfo.getClass();

            if (!parmInfo.isInvariant())
               {
               // Clear non-invariant arg info
               argInfo->set(index, NULL);
               if (enableTrace)
                  traceMsg(comp(), "PREX:       parm %d is not invariant\n", index);
               continue;
               }

            if (parmInfo.hasKnownObjectIndex())
               {
               TR_ASSERT_FATAL(parmInfo.getKnownObjectIndex() == arg->getKnownObjectIndex(), "Prex arg info should match existing arg info");
               if (enableTrace)
                  traceMsg(comp(), "PREX:       parm %d is obj.%d\n", index, parmInfo.getKnownObjectIndex());
               continue;
               }
            if (!clazzFromMethod)
               {
               if (enableTrace)
                  traceMsg(comp(), "PREX:       Can't get method class for parm %d, skipping\n", index);
               continue;
               }

            if (!arg)
               {
               if (enableTrace)
                  traceMsg(comp(), "PREX:      No argInfo for parm %d\n", index);
               continue;
               }

            if (enableTrace)
               traceMsg(comp(), "PREX:      Parm %d is arg %p parmInfo %p\n", index, arg, &parmInfo);

            if (arg->hasKnownObjectIndex()
               && !comp()->isOutOfProcessCompilation()
               )
               {
               if (enableTrace)
                  traceMsg(comp(), "PREX:        Parm %d is known object obj%d\n", index, arg->getKnownObjectIndex());

               if (!(arg->getClass() && arg->classIsFixed()))
                  {
                  TR::VMAccessCriticalSection setClass(comp());
                  TR_OpaqueClassBlock *fixedClazz = TR::Compiler->cls.objectClass(comp(), knot->getPointer(arg->getKnownObjectIndex()));
                  arg->setClassIsFixed(fixedClazz);
                  }
               }

            bool classIsFixed       = arg->classIsFixed();
            bool classIsPreexistent = arg->classIsPreexistent();
            if (!classIsFixed && !classIsPreexistent)
               continue;

            TR_OpaqueClassBlock *clazz = arg->getClass() ? arg->getClass() : clazzFromMethod;

            // bail out if types are not compatible, which is an indicator that we're peeking or inlining dead path
            if (clazz != clazzFromMethod && fe()->isInstanceOf(clazz, clazzFromMethod, true, true, true) != TR_yes)
               return 1;

            parmInfo.clear();
            parmInfo.setSymbol(p);
            parmInfo.setKnownObjectIndex(arg->getKnownObjectIndex());
            parmInfo.setClass(clazz);

            if (enableTrace)
               traceMsg(comp(), "PREX:          Parm %d is class %p sig %s\n", index, clazz, TR::Compiler->cls.classSignature(comp(), clazz, trMemory()));

            if (clazz != clazzFromMethod)
               {
               parmInfo.setClassIsRefined();
               if (enableTrace)
                  traceMsg(comp(), "PREX:          Parm %d class is refined\n", index);
               }

            if (classIsFixed || TR::Compiler->cls.isClassFinal(comp(), clazz))
               {
               parmInfo.setClassIsFixed();
               if (enableTrace)
                  traceMsg(comp(), "PREX:        Parm %d class is final\n", index);
               }
            else if (classIsPreexistent)
               {
               if (enableTrace)
                  traceMsg(comp(), "PREX:        Parm %d class is preexistent\n", index);
               parmInfo.setClassIsPreexistent();
               if (classIsCurrentlyFinal(clazz))
                  {
                  parmInfo.setClassIsCurrentlyFinal();
                  if (enableTrace)
                     traceMsg(comp(), "PREX:            Parm %d class is currently final\n", index);
                  }
               }
            }
         }
      else if (!_isOutermostMethod)
         {
         if (enableTrace)
            traceMsg(comp(), "PREX:    No argInfo -- can't populate parmInfo for inlined method %s\n", feMethod->signature(trMemory()));
         }
      }

   if (numInvariantArgs == 0)
      {
      if (enableTrace)
         traceMsg(comp(), "PREX: No invariant arguments\n");
      return 1;
      }

   // Walk the trees and convert indirect dispatches on the fixed parms
   // to direct calls
   //
   if (enableTrace)
      traceMsg(comp(), "PREX:    Walking nodes in %s\n", feMethod->signature(trMemory()));

   vcount_t visitCount = comp()->incOrResetVisitCount();
   for (tt = methodSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      processNode(tt->getNode(), tt, visitCount);

   } // scope of the stack memory region

   if (enableTrace)
      traceMsg(comp(), "PREX: Done preexistence for %s\n", feMethod->signature(trMemory()));
   return 3;
   }

#ifdef J9_PROJECT_SPECIFIC
static bool specializeInvokeExactSymbol(TR::Node *callNode, TR::KnownObjectTable::Index receiverIndex, TR::Compilation *comp, TR::Optimization *opt)
   {
   TR::KnownObjectTable     *knot           = comp->getKnownObjectTable();
   uintptr_t              *refLocation    = knot->getPointerLocation(receiverIndex);
   TR::SymbolReference      *symRef         = callNode->getSymbolReference();
   TR::ResolvedMethodSymbol *owningMethod   = callNode->getSymbolReference()->getOwningMethodSymbol(comp);
   TR_ResolvedMethod       *resolvedMethod = comp->fej9()->createMethodHandleArchetypeSpecimen(comp->trMemory(), refLocation, owningMethod->getResolvedMethod());
   if (resolvedMethod)
      {
      TR::SymbolReference      *specimenSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(owningMethod->getResolvedMethodIndex(), -1, resolvedMethod, TR::MethodSymbol::ComputedVirtual);
      if (performTransformation(comp, "%sSubstituting more specific method symbol on %p: %s <- %s\n", opt->optDetailString(), callNode,
            specimenSymRef->getName(comp->getDebug()),
            callNode->getSymbolReference()->getName(comp->getDebug())))
         {
         callNode->setSymbolReference(specimenSymRef);
         return true;
         }
      }
      return false;
   }
#endif

TR_InvariantArgumentPreexistence::ParmInfo *TR_InvariantArgumentPreexistence::getSuitableParmInfo(TR::Node *node)
   {
   if (!node->getOpCode().isLoadVarDirect())
      return NULL;

   TR::Symbol *symbol = node->getSymbolReference()->getSymbol();
   if (!symbol->isParm())
      return NULL;

   int32_t index = symbol->getParmSymbol()->getOrdinal();
   ParmInfo *info = _parmInfo + index;
   if (!info->getSymbol())
      return NULL;

   // The argument must be preexistent in order to allow us to devirtualize the dispatches.
   // Or if we are peeking - then it must be a refined type
   //
   if (comp()->isPeekingMethod() && !info->classIsRefined())
      return NULL;
   if (!comp()->isPeekingMethod() &&
       !info->classIsPreexistent() && !info->classIsFixed())
      return NULL;

   return info;
   }

bool TR_InvariantArgumentPreexistence::classIsCurrentlyFinal(TR_OpaqueClassBlock* clazz)
   {
   if (TR::Compiler->cls.isConcreteClass(comp(), clazz) &&
       !fe()->classHasBeenExtended(clazz))
      return true;

   return false;
   }

void TR_InvariantArgumentPreexistence::processNode(TR::Node *node, TR::TreeTop *treeTop, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;
   else
      node->setVisitCount(visitCount);

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      processNode(node->getChild(i), treeTop, visitCount);

   if (node->getOpCode().isLoadIndirect())
      processIndirectLoad(node, treeTop, visitCount);
   else if (node->getOpCode().isCallIndirect())
      processIndirectCall(node, treeTop, visitCount);
   }

TR_YesNoMaybe TR_InvariantArgumentPreexistence::classIsCompatibleWithMethod(TR_OpaqueClassBlock* thisClazz, TR_ResolvedMethod* method)
   {
   TR_OpaqueClassBlock *clazz = method->containingClass();

   if (clazz)
      {
      if (thisClazz)
         {
         return fe()->isInstanceOf(thisClazz, clazz, true);
         }
      }
   return TR_maybe;
   }

bool TR_InvariantArgumentPreexistence::devirtualizeVirtualCall(TR::Node *node, TR::TreeTop *treeTop, TR_OpaqueClassBlock* clazz)
   {
   TR::MethodSymbol   *methodSymbol   = node->getSymbol()->castToMethodSymbol();
   TR_ASSERT(methodSymbol->isVirtual(), "Method is not virtual");
   TR_ResolvedMethod *resolvedMethod = methodSymbol->getResolvedMethodSymbol()? methodSymbol->getResolvedMethodSymbol()->getResolvedMethod() : NULL;
   if (!resolvedMethod)
      {
      if (trace())
         traceMsg(comp(), "Method is not resolved, can't devirtualize\n");
      return false;
      }

   TR_ASSERT(classIsCompatibleWithMethod(clazz, resolvedMethod) == TR_yes, "Class should be compatible with method");
   TR::SymbolReference *symRef = node->getSymbolReference();
   int32_t offset = static_cast<int32_t>(symRef->getOffset());
   TR_ResolvedMethod *refinedMethod = symRef->getOwningMethod(comp())->getResolvedVirtualMethod(comp(), clazz, offset);

   if (!refinedMethod)
      {
      if (trace())
         traceMsg(comp(), "Can't find a method from class %p with offset %d\n", clazz, offset);
      return false;
      }

   if (!performTransformation(comp(), "%sspecialize and devirtualize invoke [%p] on currently fixed or final parameter\n", optDetailString(), node))
      return false;

   if (!refinedMethod->isSameMethod(resolvedMethod))
      {
      TR::SymbolReference *newSymRef =
          getSymRefTab()->findOrCreateMethodSymbol
          (symRef->getOwningMethodIndex(), -1, refinedMethod, TR::MethodSymbol::Virtual);
      newSymRef->copyAliasSets(symRef, getSymRefTab());
      newSymRef->setOffset(offset);
      node->setSymbolReference(newSymRef);
      node->devirtualizeCall(treeTop);
      return true;
      }

   node->devirtualizeCall(treeTop);
   return true;
   }

void TR_InvariantArgumentPreexistence::processIndirectCall(TR::Node *node, TR::TreeTop *treeTop, vcount_t visitCount)
   {
#ifdef J9_PROJECT_SPECIFIC

   if (trace())
      traceMsg(comp(), "PREX:      [%p] %s %s\n", node, node->getOpCode().getName(), node->getSymbolReference()->getName(comp()->getDebug()));

   if (!node->getSymbol()->castToMethodSymbol()->firstArgumentIsReceiver())
      {
      if (trace())
         traceMsg(comp(), "PREX:        - First arg is not receiver\n");
      return;
      }

   //
   // Step 1: Analyze
   //

   // Decision variables
   //
   bool  isInterface             = false;
   ParmInfo           tmpInfo;  tmpInfo.clear();
   ParmInfo           *receiverInfo = &tmpInfo;
   TR::Symbol          *receiverSymbol = NULL;
   bool               receiverFromParm = false;
   int32_t            receiverParmOrdinal = -1;
   ParmInfo *existingInfo = NULL;
   TR::MethodSymbol   *methodSymbol   = node->getSymbol()->castToMethodSymbol();

   TR_ResolvedMethod *resolvedMethod = methodSymbol->getResolvedMethodSymbol()? methodSymbol->getResolvedMethodSymbol()->getResolvedMethod() : NULL;
   if (!resolvedMethod)
      {
      if (methodSymbol->isInterface())
         {
         isInterface = true;
         }
      else
         {
         if (trace())
            traceMsg(comp(), "PREX:        - Unresolved\n");
         return;
         }
      }

   TR::Node *receiver = node->getChild(node->getFirstArgumentIndex());
   if (receiver->getOpCode().isLoadDirect())
      {
      existingInfo = getSuitableParmInfo(receiver);
      if (!existingInfo)
         {
         if (trace())
            traceMsg(comp(), "PREX:        - No parm info for receiver\n");
         return;
         }

      receiverInfo = existingInfo;
      receiverFromParm = true;

      receiverSymbol = receiver->getSymbolReference()->getSymbol();
      receiverParmOrdinal = receiverSymbol->getParmSymbol()->getOrdinal();
      if (methodSymbol->isVirtual() || methodSymbol->isInterface())
         {
         if (node->getSymbolReference() == getSymRefTab()->findObjectNewInstanceImplSymbol())
            {
            // Let's not get fancy with these guys
            // They are the java/lang/Object.newInstancePrototype special methods
            if (trace())
               traceMsg(comp(), "PREX:        - newInstancePrototype\n");
            return;
            }

         if (trace())
            traceMsg(comp(), "PREX:        Receiver is %p incoming Parm %d parmInfo %p\n", receiver, receiverParmOrdinal, existingInfo);
         }
      }

   // Bonus goodies for known objects
   //
   if (receiver->getSymbolReference()
       && receiver->getSymbolReference()->hasKnownObjectIndex()
       && !comp()->isOutOfProcessCompilation()
      )
      {
      if (trace())
         traceMsg(comp(), "PREX:          Receiver is obj%d\n", receiver->getSymbolReference()->getKnownObjectIndex());

      receiverInfo->setKnownObjectIndex(receiver->getSymbolReference()->getKnownObjectIndex());
      receiverInfo->setClassIsFixed();

      // Also set the class info
      //
      TR::KnownObjectTable *knot = comp()->getKnownObjectTable();

         {
         TR::ClassTableCriticalSection setClass(comp()->fe());
         receiverInfo->setClass(TR::Compiler->cls.objectClass(comp(), knot->getPointer(receiver->getSymbolReference()->getKnownObjectIndex())));
         }
      }

   // Quit if class is not compatible with the method
   if (resolvedMethod && receiverInfo->getClass() && !classIsCompatibleWithMethod(receiverInfo->getClass(), resolvedMethod))
      {
      if (trace())
         traceMsg(comp(), "PREX:        - Receiver type incompatible with method \n");

      return;
      }

   TR::MethodSymbol *methSymbol = node->getSymbol()->getMethodSymbol();
   if (methSymbol->isInterface())
      {
      // Interface type signatures can't be trusted most places in bytecode, so
      // only transform interface calls when we have a class bound for the
      // receiver.
      TR_OpaqueClassBlock *klass = receiverInfo->getClass();
      if (klass == NULL || TR::Compiler->cls.isInterfaceClass(comp(), klass))
         {
         if (trace())
            {
            traceMsg(
               comp(),
               "PREX:        - No class type bound for interface call receiver\n");
            }

         return;
         }

      TR_ResolvedMethod *caller = node->getSymbolReference()->getOwningMethod(comp());
      TR::Method *callee = methSymbol->getMethod();
      bool aotOk = true;
      TR_OpaqueClassBlock *iface = fe()->getClassFromSignature(
         callee->classNameChars(), callee->classNameLength(), caller, aotOk);

      if (iface == NULL)
         {
         if (trace())
            {
            traceMsg(
               comp(),
               "PREX:        - Failed to identify interface for interface call\n");
            }

         return;
         }

      if (fe()->isInstanceOf(klass, iface, true, true, true) != TR_yes)
         {
         if (trace())
            {
            traceMsg(
               comp(),
               "PREX:        - Insufficient class type bound for interface call receiver\n");
            }

         return;
         }
      }

   //
   // Step 2: Transform
   //

   if (methodSymbol->isComputed())
      {
#ifdef J9_PROJECT_SPECIFIC
      if (methodSymbol->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact && receiverInfo->hasKnownObjectIndex())
         specializeInvokeExactSymbol(node, receiverInfo->getKnownObjectIndex(), comp(), this);

      // The method is a specialized thunk archetype, no further improvement is needed.
      // Keeping running subsequent code may result in a crash because `offset` on the symref is not valid.
      if (node->getSymbol()->castToMethodSymbol()->getMethod()->isArchetypeSpecimen())
         return;
#endif
      }
   else if (!isInterface && receiverInfo->classIsFixed())
      {
      devirtualizeVirtualCall(node, treeTop, receiverInfo->getClass());
      }
   else if (!isInterface && receiverInfo->classIsCurrentlyFinal()
            && comp()->ilGenRequest().details().supportsInvalidation())
      {
      TR_PersistentClassInfo* classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(receiverInfo->getClass(), comp());
      bool canDevirtualize = true;

      if (comp()->getMethodHotness() == warm
          && classInfo
          && classInfo->getNumPrexAssumptions() > comp()->getOptions()->getMaxNumPrexAssumptions())
         canDevirtualize = false;

      if (canDevirtualize && devirtualizeVirtualCall(node, treeTop, receiverInfo->getClass()))
         {
         if (trace())
            traceMsg(comp(), "devirtualize with assumption\n");
         // improve receiverInfo
         receiverInfo->setClassIsFixed();
         // For the outer most method, we have to carry the information of fixed type to inliner,
         // such that it can be propagated to deeper frames.
         // The receiver type is current final, which means it's the most concrete type atm. If
         // the receiver is from parm of the jitted method, this property applies to all call
         // sites to this method in this compilation. Thus, it is safe to set fixed type on the
         // parm of this method.
         //
         if (_isOutermostMethod && receiverFromParm)
            {
            receiverSymbol->getParmSymbol()->setFixedType(receiverInfo->getClass());
            }
         // Improve the prex arg for inlined method
         else if (receiverFromParm && comp()->getCurrentInlinedCallArgInfo())
            {
            TR_PrexArgInfo *argInfo = comp()->getCurrentInlinedCallArgInfo();
            TR_PrexArgument *arg = argInfo->get(receiverParmOrdinal);
            if (arg && !(arg->classIsFixed() && arg->getClass()))
               {
               arg->setClassIsFixed(receiverInfo->getClass());
               }
            }
         TR_ASSERT(receiverInfo->getClass(), "Currently final classes must have a valid class pointer");
         bool inc = comp()->getCHTable()->recompileOnClassExtend(comp(), receiverInfo->getClass());
         if (classInfo && inc) classInfo->incNumPrexAssumptions();
         }
      }
   else if (receiverFromParm)
      {
      // If the method being called is currently not overridden, we can register
      // a recomp action on the method-override event for this method
      //
      if (!isInterface && !resolvedMethod->virtualMethodIsOverridden() && !resolvedMethod->isAbstract())
         {
         // if the number of recompile assumptions on this particular
         // class has exceeded a threshold, don't do prex anymore for this class
         //
         bool addAssumptions = false;
         TR_PersistentMethodInfo *callInfo = NULL;

         if (comp()->ilGenRequest().details().supportsInvalidation())
            {
            addAssumptions = true;
            if ((comp()->getMethodHotness() == warm) &&
               (callInfo = TR_PersistentMethodInfo::get(resolvedMethod)) &&
               callInfo->getNumPrexAssumptions() > comp()->getOptions()->getMaxNumPrexAssumptions())
               addAssumptions = false;
            }

         if (addAssumptions &&
               performTransformation(comp(), "%sdevirtualizing invoke [%p] on preexistent argument %d [%p]\n", optDetailString(), node, receiverParmOrdinal, receiverSymbol))
            {
            if (trace())
               traceMsg(comp(), "secs devirtualizing invoke on preexistent argument %d in %s\n", receiverParmOrdinal, comp()->signature());

            node->devirtualizeCall(treeTop);
            bool inc = comp()->getCHTable()->recompileOnMethodOverride(comp(), resolvedMethod);
            if (callInfo && inc) callInfo->incNumPrexAssumptions();
            }
         }
      else if (receiverInfo->getClass())
         {
#ifdef J9_PROJECT_SPECIFIC
         TR::ClassTableCriticalSection processIndirectCall(comp()->fe());
         TR::SymbolReference *symRef = node->getSymbolReference();
         TR_PersistentCHTable * chTable = comp()->getPersistentInfo()->getPersistentCHTable();
         if (methSymbol->isInterface() || methodSymbol)
            {
            TR_ResolvedMethod * method = NULL;
            bool newMethod = true;
            // There is the risk of invaliding a method repeatedly due to class hierarchy extensions
            // In the following avoid devirtualization based on single implementor if the method has
            // been invalidated once
            //
            TR::Recompilation *recompInfo = comp()->getRecompilationInfo();
            if (recompInfo && recompInfo->getMethodInfo()->getNumberOfInvalidations() >= 1 && !chTable->findSingleConcreteSubClass(receiverInfo->getClass(), comp()))
               {
               // will exit without performing any transformation
               //fprintf(stderr, "will not perform devirt\n");
               }
            else if (methSymbol->isInterface())
               {
               if (comp()->getPersistentInfo()->getRuntimeAssumptionTable()->getAssumptionCount(RuntimeAssumptionOnClassExtend) < 100000)
                  method = chTable->findSingleInterfaceImplementer(receiverInfo->getClass(), node->getSymbolReference()->getCPIndex(), node->getSymbolReference()->getOwningMethod(comp()), comp());
               if (method == NULL)
                  {
                  if (trace())
                     {
                     traceMsg(
                        comp(),
                        "PREX:        - Failed to find interface callee\n");
                     }
                  return;
                  }
               }
            else
               {
               if (resolvedMethod->isAbstract())
                  method = chTable->findSingleAbstractImplementer(receiverInfo->getClass(), symRef->getOffset(), node->getSymbolReference()->getOwningMethod(comp()), comp());
               else if (!chTable->isOverriddenInThisHierarchy(resolvedMethod, receiverInfo->getClass(), symRef->getOffset(), comp()))
                  {
                  method = symRef->getOwningMethod(comp())->getResolvedVirtualMethod(comp(), receiverInfo->getClass(), symRef->getOffset());
                  newMethod = false;
                  }
               }

            if (method && !method->virtualMethodIsOverridden())
               {
               TR_PersistentClassInfo *classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(receiverInfo->getClass(), comp());

               // if the number of recompile assumptions on this particular
               // class has exceeded a threshold, don't do prex anymore for this class
               //
               bool addAssumptions = false;

               if (comp()->ilGenRequest().details().supportsInvalidation())
                  {
                  addAssumptions = true;
                  if ((comp()->getMethodHotness() == warm) &&
                        classInfo &&
                        classInfo->getNumPrexAssumptions() > comp()->getOptions()->getMaxNumPrexAssumptions())
                     addAssumptions = false;

                  // check the subclasses as well
                  if (classInfo && addAssumptions)
                     {
                     TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());
                     TR_ClassQueries::collectAllSubClasses(classInfo, &subClasses, comp());
                     ListIterator<TR_PersistentClassInfo> subClassesIt(&subClasses);
                     for (TR_PersistentClassInfo *subClassInfo = subClassesIt.getFirst(); subClassInfo; subClassInfo = subClassesIt.getNext())
                        {
                        if (subClassInfo->getNumPrexAssumptions() > comp()->getOptions()->getMaxNumPrexAssumptions())
                           {
                           addAssumptions = false;
                           break;
                           }
                        }
                     }
                  }

               if (addAssumptions &&
                   performTransformation(comp(), "%sspecialize and devirtualize invoke [%p] based on only a single implementation for call on parameter %d [%p]\n", optDetailString(), node, receiverParmOrdinal, receiverSymbol))
                  {
                  if (newMethod || !method->isSameMethod(resolvedMethod))
                     {
                     TR::SymbolReference *newSymRef =
                      getSymRefTab()->findOrCreateMethodSymbol
                                   (symRef->getOwningMethodIndex(), -1, method, TR::MethodSymbol::Virtual);
                     newSymRef->copyAliasSets(symRef, getSymRefTab());

                     int32_t offset = -1;
                     if (methSymbol->isInterface())
                        offset = node->getSymbolReference()->getOwningMethod(comp())->getResolvedInterfaceMethodOffset(method->containingClass(), node->getSymbolReference()->getCPIndex());
                     else
                        offset = symRef->getOffset();

                     newSymRef->setOffset(offset);
                     node->setSymbolReference(newSymRef);
                     }

                  node->devirtualizeCall(treeTop);
                  if (treeTop->getNode()->getOpCodeValue() == TR::ResolveCHK)
                     TR::Node::recreate(treeTop->getNode(), TR::treetop);
                  else if (treeTop->getNode()->getOpCodeValue() == TR::ResolveAndNULLCHK)
                     TR::Node::recreate(treeTop->getNode(), TR::NULLCHK);

                  bool doInc = comp()->getCHTable()->recompileOnNewClassExtend(comp(), receiverInfo->getClass());

                  if (classInfo)
                     {
                     classInfo->setShouldNotBeNewlyExtended(comp()->getCompThreadID());
                     if (doInc) classInfo->incNumPrexAssumptions();
                     TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());
                     TR_ClassQueries::collectAllSubClasses(classInfo, &subClasses, comp());
                     ListIterator<TR_PersistentClassInfo> subClassesIt(&subClasses);
                     for (TR_PersistentClassInfo *subClassInfo = subClassesIt.getFirst(); subClassInfo; subClassInfo = subClassesIt.getNext())
                        {
                        TR_OpaqueClassBlock *subClass = (TR_OpaqueClassBlock *) subClassInfo->getClassId();
                        subClassInfo->setShouldNotBeNewlyExtended(comp()->getCompThreadID());
                        if (comp()->getCHTable()->recompileOnNewClassExtend(comp(), subClass))
                           subClassInfo->incNumPrexAssumptions();
                        }
                     }


                  //comp()->getCHTable()->recompileOnMethodOverride(comp(), method);
                  _success = true;
                  }
               else
                  {
                  return;
                  }
               }
            else
               {
               return;
               }
            }
#endif
         }
      }


   if (comp()->isPeekingMethod() && receiverInfo->getClass() && !isInterface)
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t offset = symRef->getOffset();
      //printf("Node %p arg %d\n", node, receiverParmOrdinal);
      //printf("Method is %s\n", resolvedMethod->signature(trMemory()));
      //fflush(stdout);

      TR_ResolvedMethod *originalResolvedMethod = resolvedMethod;
      TR_OpaqueClassBlock *originalClazz = originalResolvedMethod->containingClass();
      bool canRefine = true;
      if (originalClazz != receiverInfo->getClass())
         {
         TR_YesNoMaybe isInstance = fe()->isInstanceOf(originalClazz, receiverInfo->getClass(), true);
         if (isInstance == TR_yes)
            canRefine = false;

         isInstance = fe()->isInstanceOf(receiverInfo->getClass(), originalClazz, true);
         if (isInstance == TR_no)
            canRefine = false;
         }

      TR_ResolvedMethod *resolvedMethod = NULL;
      if (canRefine)
         resolvedMethod = symRef->getOwningMethod(comp())->getResolvedVirtualMethod(comp(), receiverInfo->getClass(), offset);

      if (resolvedMethod)
         {
         if (!originalResolvedMethod || !resolvedMethod->isSameMethod(originalResolvedMethod))
            {
            TR::SymbolReference * newSymRef =
          _peekingSymRefTab->findOrCreateMethodSymbol(
                             symRef->getOwningMethodIndex(), -1, resolvedMethod, TR::MethodSymbol::Virtual);
            newSymRef->copyAliasSets(symRef, _peekingSymRefTab);
            newSymRef->setOffset(offset);
            node->setSymbolReference(newSymRef);
            }
         }
      }
#endif
   }

void TR_InvariantArgumentPreexistence::processIndirectLoad(TR::Node *node, TR::TreeTop *treeTop, vcount_t visitCount)
   {
   TR::Node *ttNode       = treeTop->getNode();
   TR::Node *addressChild = node->getFirstChild();
   if (!addressChild->getOpCode().isLoadVar())
      return;

   if (trace())
      traceMsg(comp(), "PREX:        [%p] %s %s\n", node, node->getOpCode().getName(), node->getSymbolReference()->getName(comp()->getDebug()));

   if (addressChild->getSymbolReference()->isUnresolved())
      {
      if (trace())
         traceMsg(comp(), "PREX:          - unresolved\n");
      return;
      }

   bool somethingMayHaveChanged = false;
   TR::Node *nodeToNullCheck = NULL;
   if (  ttNode->getOpCode().isNullCheck()
      && ttNode->getFirstChild() == node
      && ttNode->getNullCheckReference() == addressChild)
      {
      nodeToNullCheck = treeTop->getNode()->getNullCheckReference();
      }

   TR::Node *removedNode = NULL;
   if (addressChild->getSymbolReference()->hasKnownObjectIndex())
      {
      somethingMayHaveChanged = TR::TransformUtil::transformIndirectLoadChain(comp(), node, addressChild, addressChild->getSymbolReference()->getKnownObjectIndex(), &removedNode);
      }
   else if (addressChild->getSymbol()->isFixedObjectRef())
      {
      somethingMayHaveChanged = TR::TransformUtil::transformIndirectLoadChainAt(comp(), node, addressChild, (uintptr_t*)addressChild->getSymbol()->castToStaticSymbol()->getStaticAddress(), &removedNode);
      }
   else if (addressChild->getSymbol()->isParm())
      {
      int32_t index = addressChild->getSymbol()->castToParmSymbol()->getOrdinal();
      ParmInfo *info = _parmInfo + index;
      if (trace())
         traceMsg(comp(), "PREX:          Indirect load through incoming Parm %d parmInfo %p\n", index, info);
      if (info && info->hasKnownObjectIndex())
         somethingMayHaveChanged = TR::TransformUtil::transformIndirectLoadChain(comp(), node, addressChild, info->getKnownObjectIndex(), &removedNode);
      }

   if (removedNode)
      {
      if (removedNode->getOpCode().isTreeTop())
         TR::TreeTop::create(comp(), treeTop->getPrevTreeTop(), removedNode);
      else
         TR::TreeTop::create(comp(), treeTop->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, removedNode));
      removedNode->decReferenceCount();
      }

   if (somethingMayHaveChanged && nodeToNullCheck)
      {
      // This node was under a NULLCHK and may no longer be a suitable child for the NULLCHK.
      // Put a passThrough there instead to be safe and correct.
      // The old shape was:
      //
      //   ttNode    // NULLCHK (or some other isNullCheck opcode) on nodeToNullCheck
      //     node
      //       nodeToNullCheck
      //
      // We'll anchor the node right after the NULLCHK, then replace it with a passThrough under the NULLCHK.
      //
      //   ttNode    // NULLCHK (or some other isNullCheck opcode) on nodeToNullCheck
      //     passThrough
      //       nodeToNullCheck
      //   treetop
      //     node
      //
      TR::TreeTop::create(comp(), treeTop, TR::Node::create(TR::treetop, 1, node));
      ttNode->getAndDecChild(0);
      ttNode->setAndIncChild(0, TR::Node::create(TR::PassThrough, 1, nodeToNullCheck));
      if (trace())
         traceMsg(comp(), "PREX:          Anchored [%p] formerly under %s [%p]\n", node, ttNode->getOpCode().getName(), ttNode);
      }

   }

const char *
TR_InvariantArgumentPreexistence::optDetailString() const throw()
   {
   return "O^O INVARIANT ARGUMENT PREEXISTENCE: ";
   }

int32_t TR_CheckcastAndProfiledGuardCoalescer::perform()
   {
   if (comp()->getOption(TR_DisableCheckcastAndProfiledGuardCoalescer))
      return 1;

   TR::SymbolReferenceTable *srTab = comp()->getSymRefTab();

   TR::NodeChecklist visited(comp());

   // Set of auto loads that have the latest value of their corresponding autos.
   TR::NodeChecklist fresh(comp());

   // Map from auto symbol reference number to the list of all current fresh
   // loads of that auto.
   IntToNodesMap freshByAuto(
      std::less<int32_t>(), comp()->trMemory()->currentStackRegion());

   bool done = false;
   TR::Block *curBlock = NULL;
   TR::TreeTop *checkcastTree = NULL;

   // Only relevant when we have a candidate checkcast. The transformation can
   // be prevented by simply nulling out checkcastTree.
   TR::Node *cast = NULL;
   TR::Node *castObj = NULL;
   TR::Node *castLoadaddr = NULL;
   TR_OpaqueClassBlock *castType = NULL;
   TR::Block *castBlock = NULL;
   TR::Block *castCatchBlock = NULL;

   // The auto (if any) which can be loaded to rematerialize the object from
   // the candidate checkcast.
   TR::SymbolReference *castObjAuto = NULL;

   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();

      // Find and remember (address-typed) auto loads evaluated in this tree,
      // and detect whether any indirect load is evaluated here.
      bool treeHasIndirectLoad = processSubtree(visited, fresh, freshByAuto, node);
      if (treeHasIndirectLoad
          && checkcastTree != NULL
          && node->getOpCodeValue() != TR::ifacmpne) // load ok in profiled guard
         {
         // It's possible for the safety of the indirect load to depend on the
         // checkcast, in which case the two can't be reordered.
         //
         // It's tempting to look specifically for loads known to be relative
         // to the cast object, but we must be conservative if there is any
         // load that *might* access the cast object. For example, it's
         // possible that two autos refer to the same object, and one is used
         // for checkcast while the other is used for the indirect load. That
         // would guarantee the safety of the load even if it's no longer
         // possible for the compiler to prove that the autos are equal.
         //
         // A possible way to make this test more precise in the future would
         // be to have processSubtree() accept castType and detect only loads
         // that could depend on a cast to castType, i.e. loads of fields of
         // castType or its supertypes, or array elements if castType is an
         // array type, or unsafe loads.
         //
         // However, such precision would require special care. It's possible
         // for future improvements to analyses elsewhere in the compiler to
         // create nonobvious guarantees. For example, a smart enough future
         // analysis could determine that because one object is a particular
         // type, we know additionally that a different object is a different
         // particular type. In the presence of such an analysis, it would be
         // possible for a load to depend on the checkcast even if it loads a
         // field of a type unrelated to castType. Similar situations could be
         // possible with other operations whose safety conditions are even
         // more seemingly unrelated, e.g. integer division (not currently
         // protected against).
         //
         // Indirect stores don't need to be handled here because they will be
         // rejected below due to their side-effects.
         //
         traceCannotTransform(node, "will evaluate an indirect load");
         checkcastTree = NULL;
         }

      switch (node->getOpCodeValue())
         {
         case TR::BBStart:
            {
            curBlock = node->getBlock(); // always keep this up to date
            if (checkcastTree == NULL)
               break; // nothing else to do

            // Decide whether we can keep considering the same candidate
            if (curBlock->isExtensionOfPreviousBlock())
               break; // ok

            if (curBlock->getPredecessors().size() != 1)
               {
               traceCannotTransformDueToMerge(curBlock);
               checkcastTree = NULL;
               break;
               }

            TR_ASSERT_FATAL(
               curBlock->getExceptionPredecessors().empty(),
               "catch block_%d has a non-exception predecessor",
               curBlock->getNumber());

            // The predecessor is necessarily the textually previous block.
            // Otherwise, the previous block would end with control flow that
            // does not fall through, and checkcastTree would have been unset.
            //
            // This is basically like being in an extended block, except that
            // there won't be any commoning, i.e. there won't be any more
            // occurrences of castObj. However, if castObjAuto is set, it's
            // still possible to find loads that match. Without castObjAuto,
            // it's pointless, so give up.
            //
            if (castObjAuto == NULL)
               {
               checkcastTree = NULL;
               if (trace())
                  {
                  traceMsg(
                     comp(),
                     "Cannot transform because the reference "
                     "can no longer be recognized in block_%d\n",
                     curBlock->getNumber());
                  }
               }

            break;
            }

         case TR::checkcast:
         case TR::checkcastAndNULLCHK:
            {
            // If there was a previous candidate checkcast, this checkcast is
            // between it and any later profiled guard. Usually we should be
            // able to set checkcastTree again to the current tt.
            checkcastTree = NULL;

            TR::Node *typeChild = node->getChild(1);
            if (typeChild->getOpCodeValue() != TR::loadaddr
                || typeChild->getSymbolReference()->isUnresolved())
               break; // type is unknown

            checkcastTree = tt;
            cast = node;
            castObj = cast->getChild(0);
            castLoadaddr = typeChild;
            castType = (TR_OpaqueClassBlock*)(
               typeChild->getSymbol()->getStaticSymbol()->getStaticAddress());

            if (fresh.contains(castObj))
               castObjAuto = castObj->getSymbolReference();
            else
               castObjAuto = NULL;

            castBlock = curBlock;
            castCatchBlock = NULL;

            // Determine which block (if any) would catch an exception thrown
            // due to an unexpected type of object at this checkcast.
            if (!castBlock->getExceptionSuccessors().empty())
               {
               // Use a new region just in case oehi allocates a large array
               TR::Region region(comp()->trMemory()->currentStackRegion());
               TR_OrderedExceptionHandlerIterator oehi(curBlock, region);
               for (TR::Block *b = oehi.getFirst(); b != NULL; b = oehi.getNext())
                  {
                  if (b->canCatchExceptions(TR::Block::CanCatchCheckCast))
                     {
                     castCatchBlock = b;
                     break;
                     }
                  }
               }

            if (trace())
               {
               traceMsg(
                  comp(),
                  "\nConsidering checkcast n%un [%p]\nCast object is n%un [%p]",
                  cast->getGlobalIndex(),
                  cast,
                  castObj->getGlobalIndex(),
                  castObj);

               if (castObjAuto != NULL)
                  traceMsg(comp(), " or #%d", castObjAuto->getReferenceNumber());

               traceMsg(comp(), "\n");

               if (castCatchBlock == NULL)
                  {
                  traceMsg(comp(), "Exception escapes\n");
                  }
               else
                  {
                  traceMsg(
                     comp(),
                     "Exception will be caught by block_%d\n",
                     castCatchBlock->getNumber());
                  }
               }

            break;
            }

         case TR::ifacmpne:
            {
            if (checkcastTree == NULL)
               break; // no candidate, so there's nothing to do

            // Reset checkcastTree now so that it doesn't have to be reset on
            // every exit from this case. If we transform, then the tree will
            // be removed. Otherwise, there is a control flow operation between
            // the checkcast and any subsequent profiled guard. Either way, the
            // current checkcastTree will no longer be a candidate for this
            // transformation.
            TR::TreeTop *castTT = checkcastTree;
            checkcastTree = NULL;

            if (!node->isProfiledGuard())
               {
               traceCannotTransform(node, "is not a profiled guard");
               break;
               }

            if (trace())
               {
               traceMsg(
                  comp(),
                  "Found profiled guard n%un [%p]\n",
                  node->getGlobalIndex(),
                  node);
               }

            TR_VirtualGuard *guard = comp()->findVirtualGuardInfo(node);
            if (guard == NULL)
               {
               // not sure whether this is possible, but break is safe
               traceCannotTransform(node, "does not have virtual guard info");
               break;
               }

            // Find the receiver object node used in the guard, and the type
            // bound that will be known for the receiver when the guard passes.
            TR::Node *receiver = NULL;
            TR_OpaqueClassBlock *guardTypeBound = NULL;
            if (guard->getTestType() == TR_VftTest)
               {
               TR::Node *vftLoad = node->getChild(0);
               TR::Node *aconstClass = node->getChild(1);

               if (vftLoad->getOpCodeValue() != TR::aloadi
                   || vftLoad->getSymbolReference() != srTab->findVftSymbolRef()
                   || aconstClass->getOpCodeValue() != TR::aconst
                   || !aconstClass->isClassPointerConstant())
                  {
                  traceCannotTransform(node, "does not have the expected VFT test shape");
                  break;
                  }

               receiver = vftLoad->getChild(0);
               guardTypeBound = (TR_OpaqueClassBlock*)aconstClass->getAddress();
               }
            else if (guard->getTestType() == TR_MethodTest)
               {
               TR::Node *vftEntryLoad = node->getChild(0);
               TR::Node *aconstMethod = node->getChild(1);

               TR::Node *vftLoad = NULL;
               if (vftEntryLoad->getOpCodeValue() == TR::aloadi)
                  vftLoad = vftEntryLoad->getChild(0);

               if (vftLoad->getOpCodeValue() != TR::aloadi
                   || vftLoad->getSymbolReference() != srTab->findVftSymbolRef()
                   || aconstMethod->getOpCodeValue() != TR::aconst
                   || !aconstMethod->isMethodPointerConstant())
                  {
                  traceCannotTransform(node, "does not have the expected method test shape");
                  break;
                  }

               receiver = vftLoad->getChild(0);
               guardTypeBound = fe()->getClassOfMethod(
                  (TR_OpaqueMethodBlock*)aconstMethod->getAddress());
               }

            if (receiver == NULL || guardTypeBound == NULL)
               {
               // failed to understand the guard
               traceCannotTransform(node, "does not use a recognized guard test");
               break;
               }

            // The receiver must be the same object that was cast in the checkcast
            if (!sameValue(receiver, castObj, castObjAuto, fresh))
               {
               traceCannotTransform(node, "tests a (possibly) different object");
               break;
               }

            // Check that guardTypeBound guarantees checkcast success
            bool fixedObjectType = false; // could be true for vft test, but doesn't matter
            bool fixedCastType = true;
            TR_YesNoMaybe expectedReceiverPassesCheckcast =
               comp()->fe()->isInstanceOf(
                  guardTypeBound, castType, fixedObjectType, fixedCastType);

            if (expectedReceiverPassesCheckcast != TR_yes)
               {
               traceCannotTransform(node, "does not guarantee checkcast success");
               break;
               }

            TR::Block *slowPathBlock =
               node->getBranchDestination()->getNode()->getBlock();

            TR_ASSERT_FATAL(
               slowPathBlock->getExceptionPredecessors().empty(),
               "guard n%un [%p] target block_%d is also a catch block",
               node->getGlobalIndex(),
               node,
               slowPathBlock->getNumber());

            if (slowPathBlock->getPredecessors().size() > 1)
               {
               // It's still safe to transform as long as curBlock is
               // immediately followed by a block that:
               // - only contains a conditional branch,
               // - is not a jump target, and
               // - is the only other predecessor of slowPathBlock.
               //
               // In this case, the checkcast will be run unnecessarily if/when
               // the later branch is taken, but it will definitely pass. The
               // transformation should still be a win because checkcast is
               // eliminated in the case where neither the profiled guard nor
               // the following branch is taken.
               //
               // This catches the case where the inlined body is protected by
               // a profiled guard followed by an HCR guard.
               //
               TR::Block *hotBlock = curBlock->getNextBlock();
               TR::Node *ifNode = NULL;
               if (hotBlock->getPredecessors().size() == 1)
                  {
                  TR::Node *n = hotBlock->getFirstRealTreeTop()->getNode();
                  if (n->getOpCode().isIf())
                     {
                     TR::Block *nDest =
                        n->getBranchDestination()->getNode()->getBlock();
                     if (nDest == slowPathBlock
                         && slowPathBlock->getPredecessors().size() == 2)
                        {
                        // Check for an indirect load that will be evaluated in
                        // the hot conditional tree. If there is one, then the
                        // transformation would move checkcast across it too.
                        //
                        // It's safe to processSubtree() here because between
                        // tt and n there is only a BBEnd and a BBStart. When
                        // the tt loop reaches this conditional tree, the
                        // redundant processSubtree() call will just do nothing
                        // and return false. The false result will be fine
                        // (even if the result is true here) because there
                        // won't be a candidate checkcast anymore.
                        //
                        if (processSubtree(visited, fresh, freshByAuto, n))
                           {
                           // leave ifNode unset
                           traceCannotTransform(
                              node,
                              "(hot conditional) will evaluate an indirect load");
                           }
                        else
                           {
                           ifNode = n;
                           }
                        }
                     }
                  }

               if (ifNode == NULL)
                  {
                  traceCannotTransformDueToMerge(slowPathBlock);
                  break; // can't move checkcast past the merge point
                  }

               if (trace())
                  {
                  traceMsg(
                     comp(),
                     "Merge point block_%d is ok because "
                     "the only other predecessor is the conditional n%un [%p]\n",
                     slowPathBlock->getNumber(),
                     ifNode->getGlobalIndex(),
                     ifNode);
                  }
               }

            // OK to transform! Ask permission
            if (!performTransformation(
                  comp(),
                  "%s Merging checkcast n%un [%p] and profiled guard n%un [%p]\n",
                  optDetailString(),
                  cast->getGlobalIndex(),
                  cast,
                  node->getGlobalIndex(),
                  node))
               break;

            // Transformation
            done = true;
            optimizer()->setRequestOptimization(
               OMR::compactNullChecks, true, tt->getEnclosingBlock());

            // Insert a nullchk if we are dealing with checkcastAndNULLCHK
            // (because only the checkcast part is eliminated/moved)
            bool generateNullCheck = cast->getOpCodeValue() == TR::checkcastAndNULLCHK;
            if (generateNullCheck)
               {
#ifdef J9_PROJECT_SPECIFIC
               TR::Node *bciNode = comp()->findNullChkInfo(cast);
#else
               TR::Node *bciNode = cast;
#endif

               TR::Node *passThroughNode =
                  TR::Node::create(bciNode, TR::PassThrough, 1, castObj);

               TR::SymbolReference *nullCheckSR =
                  srTab->findOrCreateNullCheckSymbolRef(NULL);

               TR::Node *nullCheck = TR::Node::createWithSymRef(
                  bciNode, TR::NULLCHK, 1, passThroughNode, nullCheckSR);

               castTT->insertBefore(TR::TreeTop::create(comp(), nullCheck));
               }

            // Recreate the checkcast on the slow path.
            if (castObjAuto != NULL)
               {
               // It's already possible to rematerialize the object reference
               // at the guard and therefore also at the beginning of the
               // target block, so no additional store is needed.
               //
               // Anchor castObj because the original checkcast will be
               // removed, unless a null check was generated just above, which
               // also prevents castObj from swinging down.
               //
               // The type child doesn't need to be anchored because we've
               // already checked that it's a loadaddr.
               //
               if (!generateNullCheck)
                  generateAnchor(castObj, castTT);
               }
            else
               {
               // Can't remat using an existing castObjAuto, so we have to
               // store castObj into a new temp.
               castObjAuto = srTab->createTemporary(
                  comp()->getMethodSymbol(), TR::Address);

               TR::Node *storeNode = TR::Node::createStore(cast, castObjAuto, castObj);
               castTT->insertBefore(TR::TreeTop::create(comp(), storeNode));

               if (trace())
                  {
                  traceMsg(
                     comp(),
                     "Created temp #%d for checkcast reference\n",
                     castObjAuto->getReferenceNumber());
                  }
               }

            castTT->unlink(true);

            TR::SymbolReference *checkcastSR =
               srTab->findOrCreateCheckCastSymbolRef(comp()->getMethodSymbol());

            TR::Node *coldCast =
               TR::Node::createWithSymRef(cast, TR::checkcast, 2, checkcastSR);

            coldCast->setAndIncChild(0, TR::Node::createLoad(cast, castObjAuto));
            coldCast->setAndIncChild(1, castLoadaddr->duplicateTree());

            TR::TreeTop *coldCastTT = TR::TreeTop::create(comp(), coldCast);
            slowPathBlock->prepend(coldCastTT);

            // Make sure that if an exception is thrown from this block, it
            // goes to the same handler as it would have for the original
            // checkcast.
            //
            // No need to fix commoning here, because nothing is commoned
            // between coldCastTT and the rest of the block.
            //
            TR::CFG *cfg = comp()->getFlowGraph();
            slowPathBlock->split(coldCastTT->getNextTreeTop(), cfg);
            while (!slowPathBlock->getExceptionSuccessors().empty())
               cfg->removeEdge(slowPathBlock->getExceptionSuccessors().front());

            if (castCatchBlock != NULL)
               cfg->addExceptionEdge(slowPathBlock, castCatchBlock);

            break;
            }

         case TR::ifacmpeq:
            {
            if (checkcastTree == NULL)
               break;

            // It's safe to move a checkcast across a null test on the same
            // object if we fall through when non-null
            TR::Node *lhs = node->getChild(0);
            TR::Node *rhs = node->getChild(1);
            if (!sameValue(lhs, castObj, castObjAuto, fresh)
                || rhs->getOpCodeValue() != TR::aconst
                || rhs->getAddress() != 0)
               {
               traceCannotTransform(node, "is not a suitable null test");
               checkcastTree = NULL;
               }

            break;
            }

         case TR::NULLCHK:
            if (checkcastTree != NULL
                && !sameValue(node->getNullCheckReference(), castObj, castObjAuto, fresh))
               {
               traceCannotTransform(node, "tests a (possibly) different object");
               checkcastTree = NULL;
               break;
               }

            // Fall through to inspect the child the same way as tree-top. This
            // way we catch side-effects (calls, indirect stores) under NULLCHK

         case TR::treetop:
            node = node->getChild(0);
            // fall through

         default:
            {
            TR::ILOpCode op = node->getOpCode();
            if (op.isStoreDirect())
               {
               TR::SymbolReference *dest = node->getSymbolReference();
               if (dest->getSymbol()->isAutoOrParm())
                  {
                  // After storing, there won't be any fresh loads of dest
                  // anymore. We only care about references for this purpose.
                  if (op.getOpCodeValue() == TR::astore)
                     {
                     TR::Node *val = node->getChild(0);
                     if (fresh.contains(val) && val->getSymbolReference() == dest)
                        break; // store does nothing, ignore

                     if (dest == castObjAuto)
                        {
                        castObjAuto = NULL; // it is no longer possible to remat
                        if (trace())
                           {
                           traceMsg(
                              comp(),
                              "astore n%un [%p] updates #%d, "
                              "which may now differ from n%un [%p]\n",
                              node->getGlobalIndex(),
                              node,
                              dest->getReferenceNumber(),
                              castObj->getGlobalIndex(),
                              castObj);
                           }
                        }

                     auto entry = freshByAuto.find(dest->getReferenceNumber());
                     if (entry != freshByAuto.end())
                        {
                        NodeList &loads = entry->second;
                        for (auto it = loads.begin(); it != loads.end(); ++it)
                           {
                           TR::Node *load = *it;
                           TR_ASSERT_FATAL(
                              fresh.contains(load),
                              "expected n%un [%p] to be a fresh load of #%d",
                              load->getGlobalIndex(),
                              load,
                              dest->getReferenceNumber());

                           fresh.remove(load);
                           }

                        loads.clear();
                        }
                     }

                  // Now we only need to determine whether to invalidate the
                  // current candidate checkcast (if any).
                  if (checkcastTree == NULL)
                     break; // nothing to do

                  // Stores to autos are OK as long as the stored auto is dead
                  // after taking an exception from checkcast. If it's live on
                  // the exception path, then the exception path will see the
                  // new value even though it's supposed to see the old one,
                  // since the exception is supposed to have been thrown before
                  // the store.

                  // We know that an auto is dead on exception if there is no
                  // exception path from the candidate checkcast.
                  if (castCatchBlock == NULL)
                     break; // dest is dead on exception, ok to reorder

                  // The use-only alias set of the checkcast symref contains
                  // all autos that are live on exception (though it may and
                  // often will also contain autos that are not).
                  auto excUses = cast->getSymbolReference()->getUseonlyAliases();
                  if (!excUses.contains(dest, comp()))
                     break; // dest is dead on exception, ok to reorder

                  // We could get more opportunities by computing liveness, but
                  // for now, let's just say dest is possibly live on exception.
                  traceCannotTransform(
                     node, "updates an auto that may be live on exception");

                  checkcastTree = NULL;
                  break;
                  }
               }

            // The checkcast cannot in general be reordered past control flow
            // or side effects. Reject the candidate if necessary.
            if (checkcastTree == NULL)
               break; // nothing to do

            if (op.isStore()
                || op.isBranch()
                || op.isSwitch()
                || op.isJumpWithMultipleTargets()
                || op.isCall()
                || op.isReturn()
                || node->canGCandExcept()
                || !node->mayKill().isZero(comp()))
               {
               traceCannotTransform(node, "has a control or side-effect");
               checkcastTree = NULL;
               }

            break;
            }
         }

      }

   if (done)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      }

   if (trace())
      traceMsg(comp(), "\n");

   return done;
   }

// Add all unvisited (i.e. previously unevaluated) auto aload nodes in the
// subtree rooted at node to fresh, freshByAuto. Also determine whether the
// subtree contains any previously unevaluated indirect loads.
bool TR_CheckcastAndProfiledGuardCoalescer::processSubtree(
   TR::NodeChecklist &visited,
   TR::NodeChecklist &fresh,
   IntToNodesMap &freshByAuto,
   TR::Node *node)
   {
   bool hasIndirectLoad = false;

   if (visited.contains(node))
      return hasIndirectLoad;

   visited.add(node);

   // freshness is relevant only for reference-typed autos
   if (node->getOpCodeValue() == TR::aload)
      {
      TR::SymbolReference *src = node->getSymbolReference();
      if (src->getSymbol()->isAutoOrParm())
         {
         fresh.add(node);

         NodeList empty(comp()->trMemory()->currentStackRegion());
         auto insertResult = freshByAuto.insert(
            std::make_pair(src->getReferenceNumber(), empty));

         auto entry = insertResult.first;
         NodeList &srcFreshLoads = entry->second;
         srcFreshLoads.push_back(node);
         }
      }
   else if (node->getOpCode().isLoadIndirect())
      {
      hasIndirectLoad = true;
      }

   int32_t numChildren = node->getNumChildren();
   for (int32_t i = 0; i < numChildren; i++)
      {
      if (processSubtree(visited, fresh, freshByAuto, node->getChild(i)))
         hasIndirectLoad = true;
      }

   return hasIndirectLoad;
   }

// Determine whether obj is guaranteed to refer to the same instance as castObj
// or (if non-null) castObjAuto. When castObjAuto is non-null, the value
// obtained by loading castObjAuto here is assumed to be the same as castObj.
bool TR_CheckcastAndProfiledGuardCoalescer::sameValue(
   TR::Node *obj,
   TR::Node *castObj,
   TR::SymbolReference *castObjAuto,
   TR::NodeChecklist &fresh)
   {
   if (obj == castObj)
      return true;

   return castObjAuto != NULL
      && obj->getOpCodeValue() == TR::aload
      && obj->getSymbolReference() == castObjAuto
      && fresh.contains(obj);
   }

void TR_CheckcastAndProfiledGuardCoalescer::traceCannotTransform(
   TR::Node *node, const char *why)
   {
   if (!trace())
      return;

   traceMsg(
      comp(),
      "Cannot transform because %s n%un [%p] %s\n",
      node->getOpCode().getName(),
      node->getGlobalIndex(),
      node,
      why);
   }

void TR_CheckcastAndProfiledGuardCoalescer::traceCannotTransformDueToMerge(
   TR::Block *mergeBlock)
   {
   if (!trace())
      return;

   traceMsg(
      comp(),
      "Cannot transform because the start of block_%d is a merge point\n",
      mergeBlock->getNumber());
}

const char *
TR_CheckcastAndProfiledGuardCoalescer::optDetailString() const throw()
   {
   return "O^O CHECKCAST AND PROFILED GUARD COALESCER: ";
   }

TR_ColdBlockMarker::TR_ColdBlockMarker(TR::OptimizationManager *manager)
   : TR_BlockManipulator(manager), _exceptionsAreRare(true), _notYetRunMeansCold(false),
     _enableFreqCBO(true)
   {}

void
TR_ColdBlockMarker::initialize()
   {
   static const char *dontdoit = feGetEnv("TR_disableFreqCBO");
   _enableFreqCBO = !dontdoit && comp()->hasBlockFrequencyInfo();

   _notYetRunMeansCold = comp()->notYetRunMeansCold();

   // Exceptions are rare, unless we have determined otherwise via profiling
   //
   _exceptionsAreRare = true;
#ifdef J9_PROJECT_SPECIFIC
   TR_CatchBlockProfileInfo *catchInfo = TR_CatchBlockProfileInfo::get(comp());
   if (catchInfo && catchInfo->getCatchCounter() > 50)
      _exceptionsAreRare = false;
#endif
   }

int32_t
TR_ColdBlockMarker::perform()
   {
   static char *validate = feGetEnv("TR_validateBeforeColdBlockMarker");
   /* The ILValidator is only created if the useILValidator Compiler Option is set */
   if (validate && comp()->getOption(TR_UseILValidator))
      {
      comp()->validateIL(TR::postILgenValidation);
      }

   identifyColdBlocks();

   static char *dontPropAfterMark = feGetEnv("TR_dontPropagateAfterMarkCold");
   if (dontPropAfterMark == NULL)
      comp()->getFlowGraph()->propagateColdInfo(false);

   return 1;
   }

bool
TR_ColdBlockMarker::hasAnyExistingColdBlocks()
   {
   TR::Block *block = optimizer()->getMethodSymbol()->getFirstTreeTop()->getNode()->getBlock();
   for (; block; block = block->getNextBlock())
      {
      if (block->isCold())
         return true;
      }
   return false;
   }

bool
TR_ColdBlockMarker::identifyColdBlocks()
   {
   initialize();

   bool foundColdBlocks = false;
   for (TR::AllBlockIterator iter(optimizer()->getMethodSymbol()->getFlowGraph(), comp()); iter.currentBlock(); ++iter)
      {
      TR::Block *block = iter.currentBlock();

      if (block->isCold())
         {
         // OSR blocks may not have had a chance to set their frequencies yet
         if (block->isOSRCodeBlock() || block->isOSRCatchBlock())
            block->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);

         foundColdBlocks = true;
         }
      else
         {
         int32_t coldness = isBlockCold(block);

         bool controlled = false;
         if (comp()->ilGenTrace())
            controlled = true;

         if ((coldness <= MAX_COLD_BLOCK_COUNT) &&
             ((!controlled) ||
              performTransformation(comp(), "%s%s marked block_%d cold\n", optDetailString(), name(), block->getNumber())))
            {
            block->setIsCold();
            block->setFrequency(coldness);
            foundColdBlocks = true;
            }
         else if (_enableFreqCBO && block->getFrequency() == 0 &&
                  ((!controlled) ||
                   performTransformation(comp(), "%s%s marked block_%d rare\n", optDetailString(), name(), block->getNumber())))
            {
            foundColdBlocks = true;
            }
         }
      }
   return foundColdBlocks;
   }

int32_t
TR_ColdBlockMarker::isBlockCold(TR::Block *block)
   {
   if (block->isCold()) return block->getFrequency();

   if (block->isExtensionOfPreviousBlock() && block->getPrevBlock()->isCold())
      return block->getPrevBlock()->getFrequency();

   if (_exceptionsAreRare && !block->getExceptionPredecessors().empty() && block->getFrequency() <= 0)
      return CATCH_COLD_BLOCK_COUNT;

   comp()->incVisitCount();

   // You can swap these in for testing purposes
   //for (TR::PreorderNodeOccurrenceIterator iter(block->getFirstRealTreeTop(), this); iter != block->getExit(); ++iter)
   //for (TR::PostorderNodeOccurrenceIterator iter(block->getFirstRealTreeTop(), this); iter != block->getExit(); ++iter)
   //
   for (TR::PreorderNodeIterator iter(block->getFirstRealTreeTop(), comp()); iter != block->getExit(); ++iter)
      {
      TR::Node *node = iter.currentNode();

      // If the block throws an exception, and exceptions are rare in this method, or if the block has
      // some unresolved symbols in it, then it must be a cold block
      //
      // NOTE: return is not considered a cold block: in a loopless methods, a return postdominates
      // everything.
      //
      if (node->getOpCodeValue() == TR::athrow && _exceptionsAreRare && block->getFrequency() <= 0)
         return CATCH_COLD_BLOCK_COUNT;

      if (_notYetRunMeansCold && hasNotYetRun(node))
         {
         if (trace())
            {
            traceMsg(comp(), "%s n%dn [%p] has not yet run\n", node->getOpCode().getName(), node->getGlobalIndex(), node);
            }
         return UNRESOLVED_COLD_BLOCK_COUNT;
         }

      if (  _notYetRunMeansCold
         && node->getOpCode().isCall()
         && node->getSymbol()->isResolvedMethod()
         && !node->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->convertToMethod()->isArchetypeSpecimen()
         && !comp()->getJittedMethodSymbol()->castToResolvedMethodSymbol()->doJSR292PerfTweaks())
         {
         TR::ResolvedMethodSymbol *calleeSymbol = node->getSymbol()->getResolvedMethodSymbol();
         TR_ASSERT(calleeSymbol, "assertion failure"); // cannot reach here otherwise

         if (calleeSymbol->getResolvedMethod()->isCold(comp(), node->getOpCode().isCallIndirect(), calleeSymbol))
            {
            if (trace())
               {
               traceMsg(comp(), "Infrequent interpreted call node %p\n", node);
               }
            return INTERP_CALLEE_COLD_BLOCK_COUNT;
            }
         }
      } // for loop

   return MAX_COLD_BLOCK_COUNT+1;
   }

bool
TR_ColdBlockMarker::hasNotYetRun(TR::Node *node)
   {
   TR_YesNoMaybe hasBeenRun = node->hasBeenRun();
   if (hasBeenRun != TR_maybe)
      {
      return (hasBeenRun == TR_no);
      }
   else if (node->getOpCode().isCall())
      {
      // compjazz 65805 Legacy logic
      // We need to do something extra for interface calls: they are never marked
      // as resolved.  Ignore 'em.
      //
      TR::SymbolReference *symRef = node->getSymbolReference();
      bool isUnresolved;

      if (comp()->compileRelocatableCode() &&
          !comp()->getOption(TR_UseSymbolValidationManager) &&
          !comp()->getOption(TR_DisablePeekAOTResolutions))
         isUnresolved = symRef->isUnresolvedMethodInCP(comp());
      else
         isUnresolved = symRef->isUnresolved();

      if (isUnresolved)
         {
         TR::MethodSymbol *methodSymbol = symRef->getSymbol()->castToMethodSymbol();
         if (!methodSymbol->isInterface())
            return true;
         }
      }
   else if (node->hasUnresolvedSymbolReference())
      {
      // compjazz 65805 Legacy logic
      if (node->getSymbolReference()->getSymbol()->isClassObject() &&
          node->getOpCodeValue() == TR::loadaddr)
         {
         int32_t len;
         const char *name = TR::Compiler->cls.classNameChars(comp(), node->getSymbolReference(), len);
         if (name)
            {
            TR::HeuristicRegion heuristicRegion(comp());
            char *sig = TR::Compiler->cls.classNameToSignature(name, len, comp());
            TR_OpaqueClassBlock *classObject = fe()->getClassFromSignature(sig, len, node->getSymbolReference()->getOwningMethod(comp()));
            if (classObject && !TR::Compiler->cls.isInterfaceClass(comp(), classObject))
               return true;
            }
         else
            return true;
         }
      else
         {
         if (comp()->compileRelocatableCode() &&
             !comp()->getOption(TR_UseSymbolValidationManager) &&
             !comp()->getOption(TR_DisablePeekAOTResolutions))
            {
            bool isUnresolved = node->getSymbolReference()->isUnresolvedFieldInCP(comp());
            //currentely node->hasUnresolvedSymbolReference() returns true more often for AOT than non-AOT beacause of
            //literals() vs romLiterals() used in relocatableresolvedj9method
            //this optimiztaion can be removed when work to clean up ram vs rom literals is done
            if (isUnresolved && node->getSymbol()->isConstString())
                 {
                 TR::ResolvedMethodSymbol *rms = comp()->getOwningMethodSymbol(node->getOwningMethod());
                 isUnresolved = rms->getResolvedMethod()->isUnresolvedString(node->getSymbolReference()->getCPIndex(), true);
                 }
            return isUnresolved;
            }
         else
            {
            if (comp()->compileRelocatableCode()
                && comp()->getOption(TR_UseSymbolValidationManager)
                && node->getSymbol()->isConstString())
               {
               return false;
               }
            return true;
            }
         }
      }
   return false;
   }

const char *
TR_ColdBlockMarker::optDetailString() const throw()
   {
   return "O^O COLD BLOCK MARKER: ";
   }

TR_ColdBlockOutlining::TR_ColdBlockOutlining(TR::OptimizationManager *manager)
   : TR_ColdBlockMarker(manager)
   {}

int32_t
TR_ColdBlockOutlining::perform()
   {
   bool hasColdBlocks = false;
   hasColdBlocks = identifyColdBlocks();

   if (!hasColdBlocks) return 0;

   static char *noOutlining = feGetEnv("TR_NoColdOutlining");
   if (noOutlining)
      return 0;

   comp()->getFlowGraph()->propagateColdInfo(false);



   TR_OrderBlocks orderBlocks(manager(), true);

   if (trace())
      {
      comp()->dumpMethodTrees("Before cold block outlining");
      traceMsg(comp(), "Original ");
      orderBlocks.dumpBlockOrdering(comp()->getMethodSymbol()->getFirstTreeTop());
      }

   reorderColdBlocks();
   requestOpt(OMR::basicBlockPeepHole);

   if (trace())
      {
      traceMsg(comp(), "After outlining cold Block ");
      orderBlocks.dumpBlockOrdering(comp()->getMethodSymbol()->getFirstTreeTop());
      comp()->dumpMethodTrees("After cold block outlining");
      }

   return 1;
   }

static bool coldBlock(TR::Block *block, TR::Compilation *comp)
   {
   int32_t lowCFGFrequency = comp->getFlowGraph()->getLowFrequency();
   bool    retValue        = false;



      retValue = (block->isCold() || (comp->getFlowGraph() &&
                              (comp->getFlowGraph()->getMaxFrequency()>(lowCFGFrequency<<2)) &&
                               block->getFrequency()<=lowCFGFrequency));
   return retValue;
   }


static   uint32_t numBlocksSoFar = 0;

void
TR_ColdBlockOutlining::reorderColdBlocks()
   {
   TR::TreeTop *lastTree = NULL, *exitTree = NULL, *currentTree;
   uint32_t numBlocks = 0;
   for (currentTree = comp()->getStartTree();
   currentTree != NULL;
   currentTree = exitTree->getNextTreeTop())
      exitTree = currentTree->getNode()->getBlock()->getExit();
   lastTree = exitTree;

   TR::Block *startBlock = 0, *realLastBlock = lastTree->getNode()->getBlock();
   for (currentTree = comp()->getStartTree();
        currentTree != NULL;
        currentTree = exitTree->getNextTreeTop())
      {
      TR::Block *currentBlock = currentTree->getNode()->getBlock();

      if (currentBlock == NULL) break;
      exitTree = currentBlock->getExit();

      if (exitTree == lastTree) break;

      if (!coldBlock(currentBlock, comp())/*currentBlock->isCold()*/ /* && !currentBlock->isRare(comp()) */)
         {
         // Skip the whole extended basic block (since we are not going to
         // break the extension to outline something)
         //
         TR::Block *nextExtendedBlock = currentBlock->getNextExtendedBlock();
         if (!nextExtendedBlock)
            break;
         if( !nextExtendedBlock->getEntry() )
            break;
         exitTree = nextExtendedBlock->getPrevBlock()->getExit();
         if (exitTree == lastTree)
            break;
         numBlocks =0;
         continue;
         }

     // Since gen asm flow block may contains asm branch which we can't change it to long form
     // So we don't want to move the successor of a gen asm flow block if gen asm block itself is not cold
     // Also we don't want to move gen asm flow block if one of it's successor is not cold.

     // First, check if a predeccessor of this block is genAsmFlow, this block is not it's predeccessor' fall through, continue if yes
     TR::CFGEdgeList & predecessors = ((TR::CFGNode *)currentBlock)->getPredecessors();

     for (auto predEdge = predecessors.begin(); predEdge != predecessors.end(); ++predEdge)
       {
       TR::Block *predBlock = (*predEdge)->getFrom()->asBlock();

       // we can alwasy insert a long jump if currentBlock is fall through of predBlock
       if (predBlock->getExit() && predBlock->getExit()->getNextTreeTop() &&
           predBlock->getExit()->getNextTreeTop()->getNode()->getBlock() == currentBlock &&
           currentBlock->getEntry()->getNode()->getLabel() == NULL)
          continue;
       }

     if (!startBlock)
         startBlock = currentBlock;

      numBlocks++;

      TR::Block *nextBlock = currentBlock->getNextBlock();
      // nextBlock should always be non-NULL because the if (exitTree == lastTree) checks above
      // guarantee a break out of the loop before this point for the last block (i.e. what was
      // the original last block before any reordering of cold blocks to the end)
      TR_ASSERT(nextBlock, "tree walk error nextBlock should not be NULL");

      if (coldBlock(nextBlock, comp()) /*nextBlock->isCold()*/ /* || nextBlock->isRare(comp()) */)
         continue;

      if (!performTransformation(comp(), "%soutlined cold block sequence (%d-%d)\n",
              optDetailString(), startBlock->getNumber(), currentBlock->getNumber()))
         {
         startBlock=0;
         numBlocks =0;
         continue;
         }

      // We have discovered a linear sequence of blocks starting with 'startBlock' and ending with
      // 'block': snip and place at the end of the method
      //
      TR::Block *prevOfStart = startBlock->getPrevBlock();

      // prevOfStart should be non-null: otherwise the corollary is that method entry block is cold (and hence
      // every single block in the method).
      // FIXME: the following should be an assume, but i am hesitant.. If anybody runs the cold propagtion code
      // ever at fixed opt levels (count=0) then we could results with the entry block as cold
      //
      if (prevOfStart == NULL) return;

      numBlocksSoFar+=numBlocks;
      numBlocks      =0;

      prevOfStart  = breakFallThrough(prevOfStart, startBlock);
      currentBlock = breakFallThrough(currentBlock, nextBlock);

      // SNIP
      prevOfStart->getExit()->join(nextBlock->getEntry());

      // TRANSPLANT
      realLastBlock->getExit()->join(startBlock->getEntry());

      // DONE!
      realLastBlock = currentBlock;
      startBlock = 0;

      // Zing the method end
      realLastBlock->getExit()->setNextTreeTop(0);
      exitTree = prevOfStart->getExit();
      }
   if (trace())
      traceMsg(comp(), "Cold Block Outlining: outlined %d cold blocks so far:\n", numBlocksSoFar);
   }

const char *
TR_ColdBlockOutlining::optDetailString() const throw()
   {
   return "O^O COLD BLOCK OUTLINING: ";
   }

TR::Block *
TR_BlockManipulator::breakFallThrough(TR::Block *faller, TR::Block *fallee, bool isOutlineSuperColdBlock)
   {
   TR::Node *lastNode = faller->getLastRealTreeTop()->getNode();
   if (lastNode->getOpCode().isCheck() || lastNode->getOpCodeValue() == TR::treetop)
      lastNode = lastNode->getFirstChild();

   if (lastNode->getOpCode().isReturn() ||
       lastNode->getOpCode().isGoto()   ||
       (lastNode->getOpCode().isJumpWithMultipleTargets() && lastNode->getOpCode().hasBranchChildren()) ||
       lastNode->getOpCodeValue() == TR::athrow ||
       lastNode->getOpCodeValue() == TR::igoto)
      return faller; // nothing to do

   if (lastNode->getOpCode().isBranch() || lastNode->getOpCode().isJumpWithMultipleTargets())
      {
      TR::Node    *gotoNode = TR::Node::create(lastNode, TR::Goto);
      TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
      gotoNode->setBranchDestination(fallee->getEntry());
      TR::Block  *gotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), (fallee->getFrequency() < faller->getFrequency()) ? fallee->getFrequency() : faller->getFrequency(), fallee);
      gotoBlock->append(gotoTree);

      faller->getExit()->join(gotoBlock->getEntry());
      gotoBlock->getExit()->join(fallee->getEntry());

      if (faller->getStructureOf())
         comp()->getFlowGraph()->addNode(gotoBlock, faller->getCommonParentStructureIfExists(fallee, comp()->getFlowGraph()));
      else
         comp()->getFlowGraph()->addNode(gotoBlock);
      comp()->getFlowGraph()->addEdge(TR::CFGEdge::createEdge(faller,  gotoBlock, trMemory()));
      comp()->getFlowGraph()->addEdge(TR::CFGEdge::createEdge(gotoBlock,  fallee, trMemory()));
      // remove the edge only if the branch target is not pointing to the next block
      if ((lastNode->getOpCode().isBranch() && lastNode->getBranchDestination() != fallee->getEntry()) ||
          (lastNode->getOpCode().isCall() && lastNode->getOpCode().isJumpWithMultipleTargets()))
         comp()->getFlowGraph()->removeEdge(faller, fallee);
      if (fallee->isCold() || faller->isCold())
         {
         //gotoBlock->setIsCold();
         if (fallee->isCold())
            gotoBlock->setFrequency(fallee->getFrequency());
         else
            gotoBlock->setFrequency(faller->getFrequency());
         if (faller->isSuperCold())
            gotoBlock->setIsSuperCold();
         }
      return gotoBlock;
      }

   TR::Node    *gotoNode = TR::Node::create(lastNode, TR::Goto);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
   gotoNode->setBranchDestination(fallee->getEntry());
   faller->append(gotoTree);
   return faller;
   }

#define OPT_DETAILS_TDTR "TRIVIAL DEAD TREE REMOVAL: "


// used by trivialDeadTreeRemoval opt pass and during CodeGenPrep to remove nodes treetops that have no intervening def
void
TR_TrivialDeadTreeRemoval::preProcessTreetop(TR::TreeTop *treeTop, List<TR::TreeTop> &commonedTreeTopList, const char *optDetails, TR::Compilation *comp)
   {
   TR::Node *ttNode = treeTop->getNode();
   if (ttNode->getOpCodeValue() == TR::treetop &&
       ttNode->getFirstChild()->getReferenceCount() >= 1)
      {
      TR::Node *firstChild = ttNode->getFirstChild();
      if (firstChild->getReferenceCount() == 1)
         {
         if (!firstChild->getOpCode().hasSymbolReference() &&
             performTransformation(comp, "%sUnlink trivial %s (%p) of %s (%p) with refCount==1\n",
                optDetails, treeTop->getNode()->getOpCode().getName(),treeTop->getNode(),firstChild->getOpCode().getName(),firstChild))
            {
            if (trace())
               traceMsg(comp,"\tfound trivially anchored ttNode %p with firstChild %s (%p -- refCount == 1)\n",
                  ttNode,firstChild->getOpCode().getName(),firstChild);
            for (int32_t i=0; i < firstChild->getNumChildren(); i++)
               {
               TR::Node *grandChild = firstChild->getChild(i);
               if (!grandChild->getOpCode().isLoadConst() || grandChild->anchorConstChildren())
                  {
                  if (trace())
                     traceMsg(comp,"\t\tcreate new treetop for firstChild->getChild(%d) = %s (%p)\n",i,grandChild->getOpCode().getName(),grandChild);
                  // use insertAfter so the newly anchored trees get visited in the next interation(s)
                  treeTop->insertAfter(TR::TreeTop::create(comp,
                                                              TR::Node::create(TR::treetop, 1, grandChild)));
                  }
               }
            if (trace())
               traceMsg(comp,"\t\tremove trivially anchored ttNode %p with firstChild %s (%p) treetop\n",
                  ttNode,firstChild->getOpCode().getName(),firstChild);
            treeTop->unlink(true);
            }
         }
      else if (!firstChild->getOpCode().hasSymbolReference() ||
               firstChild->getOpCode().isLoadAddr() ||
               (firstChild->getOpCode().isLoad() && !firstChild->getOpCode().isStore()))
         {
         if (trace())
            traceMsg(comp,"\tadd ttNode %p with firstChild %s (%p, refCount %d) to commonedTreeTopList\n",
               ttNode,firstChild->getOpCode().getName(),firstChild,firstChild->getReferenceCount());
         commonedTreeTopList.add(treeTop);
         }
      }
   }

void
TR_TrivialDeadTreeRemoval::postProcessTreetop(TR::TreeTop *treeTop, List<TR::TreeTop> &commonedTreeTopList, const char *optDetails, TR::Compilation *comp)
   {
   if (treeTop->isPossibleDef())
      {
      if (trace())
         traceMsg(comp,"\tfound a possible def at node %p so clear _commonedTreeTopList list\n",treeTop->getNode());
      commonedTreeTopList.deleteAll();
      }
   }

void
TR_TrivialDeadTreeRemoval::processCommonedChild(TR::Node *child, TR::TreeTop *treeTop, List<TR::TreeTop> &commonedTreeTopList, const char *optDetails, TR::Compilation *comp)
   {
   if (child->getReferenceCount() > 1)
      {
      if (!commonedTreeTopList.isEmpty())
         {
         ListIterator<TR::TreeTop> listIt(&commonedTreeTopList);
         ListElement<TR::TreeTop> *prevElement = NULL;
         TR::TreeTop *listTT = listIt.getFirst();
         if (trace())
            traceMsg(comp,"commonedTreeTopList is not empty and found a commoned child %s (%p, refCount %d)\n",child->getOpCode().getName(),child,child->getReferenceCount());
         while (listTT)
            {
            TR_ASSERT(listTT->getNode()->getOpCodeValue() == TR::treetop,"only TR::treetop nodes should be in commonedTreeTopList\n");
            // if listTT == treeTop then the commoned referenced is under the same treetop as the tracked treetop
            // tt
            //   a
            //   =>a
            //
            if (trace())
               traceMsg(comp,"\tcomparing listTT %p with firstChild %s (%p) to commoned child %s (%p, refCount %d) (listTT == _currentTreeTop -- %s)\n",
                  listTT->getNode(),listTT->getNode()->getFirstChild()->getOpCode().getName(),listTT->getNode()->getFirstChild(),child->getOpCode().getName(),child,child->getReferenceCount(),listTT == treeTop?"yes":"no");
            if (listTT->getNode()->getFirstChild() == child)
               {
               if (listTT != treeTop)
                  {
                  if (performTransformation(comp, "%sFound commoned reference to child %s (%p) so unlink %s (0x%p)\n",
                        optDetails, child->getOpCode().getName(),child, listTT->getNode()->getOpCode().getName(),listTT->getNode()))
                     {
                     listTT->unlink(true);
                     }
                  commonedTreeTopList.removeNext(prevElement);
                  return;
                  }
               else
                  {
                  //Unlink if a treetop only has a common child
                  if (treeTop->getNode()->getNumChildren() == 1 && treeTop->getNode()->getOpCodeValue() == TR::treetop && treeTop->getNode()->getFirstChild() == child)
                     {
                     if (performTransformation(comp, "%sFound commoned reference to single child %s (%p) case 1 so unlink %s (0x%p)\n",
                           optDetails, child->getOpCode().getName(),child, treeTop->getNode()->getOpCode().getName(),treeTop->getNode()))
                     treeTop->unlink(true);
                     commonedTreeTopList.removeNext(prevElement);
                     return;
                  }
               }
               }
            prevElement = listIt.getCurrentElement();
            listTT = listIt.getNext();
            }
            if (trace()) traceMsg(comp,"\n");
         }
      else
         {
         //Unlink if a treetop only has a common child
         if (treeTop->getNode()->getNumChildren() == 1 && treeTop->getNode()->getOpCodeValue() == TR::treetop && treeTop->getNode()->getFirstChild() == child)
            {
            if (performTransformation(comp, "%sFound commoned reference to single child %s (%p) case 2 so unlink %s (0x%p)\n",
                optDetails, child->getOpCode().getName(),child, treeTop->getNode()->getOpCode().getName(),treeTop->getNode()))
            treeTop->unlink(true);
            }
         }

      }
   }

void
TR_TrivialDeadTreeRemoval::examineNode(TR::Node *node, vcount_t visitCount)
   {
   node->setVisitCount(visitCount);
//   if (trace())
//      traceMsg(comp(),"examineNode parent %s (%p) node %s (%p, refCount %d)\n",parent?parent->getOpCode().getName():"NULL",parent,node->getOpCode().getName(),node,node->getReferenceCount());
   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
    //  if (trace())
   //      traceMsg(comp(),"\tlooking at index %d : child %s (%p, refCount %d) -- childVC %d, VC %d (descend=%s)\n",i,child->getOpCode().getName(),child,child->getReferenceCount(),child->getVisitCount(),visitCount,child->getVisitCount() != visitCount?"yes":"no");
      if (child->getVisitCount() != visitCount)
         {
         examineNode(child, visitCount);
         }
      else
         {
         processCommonedChild(child, _currentTreeTop, _commonedTreeTopList, OPT_DETAILS_TDTR, comp());
         }
      }
  // if (trace()) traceMsg(comp(),"\n");
   }


void
TR_TrivialDeadTreeRemoval::transformBlock(TR::TreeTop * entryTree, TR::TreeTop * exitTree)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   vcount_t visitCount = comp()->incOrResetVisitCount();

   _currentBlock = entryTree->getNode()->getBlock();
   _commonedTreeTopList.deleteAll();
   if (trace())
      traceMsg(comp(),"TrivialDeadTreeRemoval on block_%d : entryTreeNode %p -> exitTreeNode %p\n",_currentBlock->getNumber(),entryTree->getNode(),exitTree->getNode());
   for (TR::TreeTop * currentTree = entryTree->getNextRealTreeTop();
        currentTree != exitTree;
        currentTree = currentTree->getNextRealTreeTop())
      {
      _currentTreeTop = currentTree;
      preProcessTreetop(currentTree, _commonedTreeTopList, OPT_DETAILS_TDTR, comp());
      examineNode(currentTree->getNode(), visitCount);
      postProcessTreetop(currentTree, _commonedTreeTopList, OPT_DETAILS_TDTR, comp());
      }

   }

int32_t
TR_TrivialDeadTreeRemoval::performOnBlock(TR::Block *block)
   {
   return 0;

//   if (block->getEntry())
//      transformBlock(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
//   return 0;
   }

int32_t
TR_TrivialDeadTreeRemoval::perform()
   {
   return 1;

//   TR::TreeTop *tt = NULL;
//   TR::TreeTop *exitTreeTop = NULL;
//   for (tt = comp()->getStartTree(); tt; tt = exitTreeTop->getNextTreeTop())
//      {
//      exitTreeTop = tt->getExtendedBlockExitTreeTop();
//      transformBlock(tt, exitTreeTop);
//      }
//
//   return 1; // actual cost
   }

const char *
TR_TrivialDeadTreeRemoval::optDetailString() const throw()
   {
   return "O^O TRIVIAL DEAD TREE REMOVAL: ";
   }

int32_t TR_TrivialBlockExtension::perform()
   {
   int32_t cost = 0;
   for (TR::Block *block = comp()->getStartTree()->getNode()->getBlock(); block && block->getEntry(); block = block->getNextBlock())
      cost += performOnBlock(block);
   return cost;
   }

int32_t TR_TrivialBlockExtension::performOnBlock(TR::Block *block)
   {
   // Please resist the urge to put more logic in this opt.  Block extension is
   // a straightforward, well-defined transformation.  If you want more than
   // just marking blocks as extensions, you probably want a new optimization.
   if (block->isExtensionOfPreviousBlock())
      {
      if (trace())
         traceMsg(comp(), "BlockExtension: block_%d is already an extension of the previous block\n", block->getNumber());
      }
   else if (block->getPredecessors().size() == 1)
      {
      TR::Block *pred = block->getPredecessors().front()->getFrom()->asBlock();
      if (pred != block->getPrevBlock())
         {
         if (trace())
            traceMsg(comp(), "BlockExtension: block_%d predecessor is not the previous block\n", block->getNumber());
         }
      else if (!pred->canFallThroughToNextBlock())
         {
         if (trace())
            traceMsg(comp(), "BlockExtension: block_%d does not fall through to block_%d\n", pred->getNumber(), block->getNumber());
         }
      else if (pred->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets())
         {
         if (trace())
            traceMsg(comp(), "BlockExtension: block_%d ends in a switch and so we will not mark block_%d as an extension\n", pred->getNumber(), block->getNumber());
         }
      else
         {
         if (performTransformation(comp(), "O^O BLOCK EXTENSION: Mark block_%d as an extension of block_%d\n", block->getNumber(), pred->getNumber()))
            block->setIsExtensionOfPreviousBlock();
         }
      }
   else
      {
      if (trace())
         traceMsg(comp(), "BlockExtension: block_%d has %d predecessors\n", block->getNumber(), block->getPredecessors().size());
      }

   return 1;
   }

const char *
TR_TrivialBlockExtension::optDetailString() const throw()
   {
   return "O^O TRIVIAL BLOCK EXTENSION: ";
   }
