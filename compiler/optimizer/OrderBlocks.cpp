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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/OrderBlocks.hpp"

#include <algorithm>                           // for std::min
#include <stdint.h>                            // for int32_t, int16_t
#include <stdio.h>                             // for NULL, fprintf, printf, etc
#include <stdlib.h>                            // for atoi
#include <string.h>                            // for strlen, memcpy
#include "codegen/FrontEnd.hpp"                // for feGetEnv, TR_FrontEnd
#include "compile/Compilation.hpp"             // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"         // for TR::Options, etc
#include "cs2/bitvectr.h"                      // for ABitVector<>::BitRef
#include "env/IO.hpp"                          // for POINTER_PRINTF_FORMAT
#include "env/StackMemoryRegion.hpp"           // for TR:StackMemoryRegion
#include "env/TRMemory.hpp"                    // for TR_Memory, BitVector
#include "il/Block.hpp"                        // for Block, toBlock
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::BBStart, etc
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node, vcount_t
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/LabelSymbol.hpp"           // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/Cfg.hpp"                       // for CFG, etc
#include "infra/List.hpp"                      // for List, ListIterator, etc
#include "infra/CfgEdge.hpp"                   // for CFGEdge
#include "infra/CfgNode.hpp"                   // for CFGNode
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"   // for OptimizationManager
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/Structure.hpp"             // for TR_BlockStructure, etc

#include "optimizer/OMRSimplifierHelpers.hpp"

#define OPT_DETAILS "O^O ORDER BLOCKS: "

// statistics collectors
static unsigned numberOfReorderings = 0;
static unsigned numberOfCompiles = 0;
static unsigned numberMethodReplicationCandidates = 0;
static unsigned numberReplicationCandidates = 0;
static unsigned numberHazardCandidates = 0;

void printReorderingStatistics(void)
   {
   if (numberOfCompiles++)
      {
      printf("Fall-through successor changed %d times\n", numberOfReorderings);
      printf("Compiled %d times\n", numberOfCompiles);
      printf("Average reorderings = %f\n", (float)numberOfReorderings/(float)numberOfCompiles);
      printf("\nReplication candidates: %d\n", numberReplicationCandidates);
      printf("\nCandidates chosen on hazards: %d\n", numberHazardCandidates);
      }
   }

void checkOrderingConsistency(TR::Compilation *comp);


TR_OrderBlocks::TR_OrderBlocks(TR::OptimizationManager *manager, bool beforeExtension)
   : TR_BlockOrderingOptimization(manager), _hotPathList(manager->trMemory()), _coldPathList(manager->trMemory())
   {
   setTrace(comp()->getOptions()->trace(OMR::basicBlockOrdering));

   _doPeepHoleOptimizationsBefore = true;
   _doPeepHoleOptimizationsAfter  = true;
   _donePeepholeGotoToLoopHeader  = false;
   _changeBlockOrderBasedOnHWProfile = false;

   _superColdBlockOnly = false;
   _extendBlocks = false;

   _needInvalidateStructure = false;

      _reorderBlocks = true;
   }


// returns TRUE if no need to find a better candidate for block
bool TR_OrderBlocks::needBetterChoice(TR::CFG *cfg, TR::CFGNode *block, TR::CFGNode *bestSucc)
   {
   // True by default if not _superColdBlockOnly
   // True if the following is meet:
   // 1. bestSucc is not block 2
   // 2. bestSucc is super cold but block is not
   // 3. bestSucc is safe to move away
   if (!_superColdBlockOnly) return true;

   // If hot list is empty, we have to choose the cold ones
   // bestSucc is NULL, then block could be 1
   if (_hotPathList.isEmpty() || bestSucc == NULL) return false;

   //if (trace()) traceMsg(comp(), "\t\tneedBetterChoice: block_%d:sec_%d: bestSucc_%d:sec_%d\n",block->getNumber(), block->asBlock()->getSectionNumber(), bestSucc->getNumber(), bestSucc->asBlock()->getSectionNumber());

   // Choose a better one if a cold block follows a hot block
   if (bestSucc != comp()->getStartBlock() && bestSucc->asBlock()->isSuperCold() &&
       !block->asBlock()->isSuperCold())
      {
      if (trace()) traceMsg(comp(), "\t\tneedBetterChoice: hot block_%d:cold_%d: follows a cold block bestSucc_%d:cold_%d\n",block->getNumber(), block->asBlock()->isSuperCold(), bestSucc->getNumber(), block->asBlock()->isSuperCold());
      return true;
      }


   return false;
   }

// returns TRUE if block absolutely cannot follow prevBlock in any legal ordering
bool TR_OrderBlocks::cannotFollowBlock(TR::Block *block, TR::Block *prevBlock)
   {
   TR::TreeTop *nextTT = NULL;
   // if block extends its predecessor and that's not prevBlock, then we cannot choose block
   if (block->isExtensionOfPreviousBlock())
      {
      if (block->getEntry()->getPrevTreeTop() != prevBlock->getExit())
         {
         if (trace()) traceMsg(comp(), "\t\textends some other block, can't follow\n");
         return true;
         }
      }

   return false;
   }

// returns TRUE if block absolutely must follow prevBlock in any legal ordering
bool TR_OrderBlocks::mustFollowBlock(TR::Block *block, TR::Block *prevBlock)
   {
   // if block is prevBlock's fall-through successor then we're stuck, we have to choose it
   if (block->isExtensionOfPreviousBlock())
      {
      if (block->getEntry()->getPrevTreeTop() == prevBlock->getExit())
         {
         if (trace()) traceMsg(comp(), "\t\textends previous block, must follow\n");
         return true;
         }
      }

   return false;
   }

//Return the first valid block in the list. If HW Profile info exists return the first valid block only if there are no
//valid hot blocks.
TR::CFGNode *TR_OrderBlocks::findSuitablePathInList(List<TR::CFGNode> & list, TR::CFGNode *prevBlock)
   {
   ListElement<TR::CFGNode> *prev=NULL;
   ListElement<TR::CFGNode> *ptr=list.getListHead();
   TR::CFGNode *block=NULL;
   ListElement<TR::CFGNode> *firstValidBlock = NULL;
   ListElement<TR::CFGNode> *prevFirstValidBlock = NULL;
   TR::CFGNode *validBlock=NULL;

   while (ptr != NULL)
      {
      block = ptr->getData();
      if (trace()) traceMsg(comp(), "\t\tconsidering block_%d freq: %d\n", block->getNumber(), block->getFrequency());
      ListElement<TR::CFGNode> *next = ptr->getNextElement();

      if (block->getVisitCount() == _visitCount)
         {
         // need to pop this guy off the list...no sense continuing to look at him
         if (prev != NULL)
            prev->setNextElement(next);
         else
            list.setListHead(next);
         if (trace()) traceMsg(comp(), "\t\t block  %d is visited\n", block->getNumber());
         }
      else
         {
         if (trace()) traceMsg(comp(), "\t\t block  %d is valid\n", block->getNumber());
         if (prevBlock == NULL || !cannotFollowBlock(block->asBlock(), prevBlock->asBlock()))
            {
            if (!_changeBlockOrderBasedOnHWProfile)
               // valid to choose this block, so break out of the loop
               break;

            if (!firstValidBlock)
               {
               // valid to choose this block if hot block does not exist
               firstValidBlock = ptr;
               prevFirstValidBlock = prev;
               validBlock = block;
               }
            if ((_numUnschedHotBlocks == 0) || block->getFrequency() > 0)
               {
               // valid to choose this block, so break out of the loop
               break;
               }
            }
         prev = ptr;
         }

      block = NULL;
      ptr = next;
      }

   if (_changeBlockOrderBasedOnHWProfile && ptr == NULL)
      {
      ptr = firstValidBlock;
      prev = prevFirstValidBlock;
      block = validBlock;
      }

   if (ptr != NULL)
      {
      if (trace()) traceMsg(comp(), "\t\tRemoving block_%d from list\n", block->getNumber());
      if (prev != NULL)
         prev->setNextElement(ptr->getNextElement());
      else
         list.setListHead(ptr->getNextElement());
      }

   return block;
   }

//Return true if a valid block that can follow prevBlock exists in the list. Otherwise return false.
bool TR_OrderBlocks::hasValidCandidate(List<TR::CFGNode> & list, TR::CFGNode *prevBlock)
   {
   ListElement<TR::CFGNode> *prev=NULL;
   ListElement<TR::CFGNode> *ptr=list.getListHead();
   TR::CFGNode *block=NULL;
   while (ptr != NULL)
      {
      block = ptr->getData();
      if (trace()) traceMsg(comp(), "\t\tconsidering block_%d\n", block->getNumber());
      ListElement<TR::CFGNode> *next = ptr->getNextElement();

      if (block->getVisitCount() == _visitCount)
         {
         // need to pop this guy off the list...no sense continuing to look at him
         if (prev != NULL)
            prev->setNextElement(next);
         else
            list.setListHead(next);
         if (trace()) traceMsg(comp(), "\t\t block  %d is visited\n", block->getNumber());
         }
      else
         {
         if (trace()) traceMsg(comp(), "\t\t block  %d is valid\n", block->getNumber());
         if (prevBlock == NULL || !cannotFollowBlock(block->asBlock(), prevBlock->asBlock()))
            // valid to choose this block, so break out of the loop
            return true;
         prev = ptr;
         }

      block = NULL;
      ptr = next;
      }

   return false;
   }

// simplest implementation for now: later, look at frequencies, nesting depth, maybe even distance from predecessors
TR::CFGNode *TR_OrderBlocks::findBestPath(TR::CFGNode *prevBlock)
   {
   TR::CFGNode *nextBlock = findSuitablePathInList(_hotPathList, prevBlock);
   if (nextBlock == NULL)
      {
      if (!_changeBlockOrderBasedOnHWProfile)
         TR_ASSERT(_hotPathList.isEmpty(), "available hot blocks");
      nextBlock                   = findSuitablePathInList(_coldPathList, prevBlock);
      }

   return nextBlock;
   }

bool TR_OrderBlocks::endPathAtBlock(TR::CFGNode *block, TR::CFGNode *bestSucc, TR::CFG *cfg)
   {
   if (block == NULL || bestSucc == NULL)
      return true;

   if( block->asBlock()->getExit())
   {
       TR::Block *nb=block->asBlock()->getNextBlock();

       if (nb && nb->isExtensionOfPreviousBlock())
           return false;
   }


   if (_superColdBlockOnly )
      {
      if (block->asBlock()->isSuperCold() != bestSucc->asBlock()->isSuperCold() &&
          block->asBlock()->getExit() && block->asBlock()->getExit()->getNextTreeTop())
         {
         // end path if coldness changes
         if (trace()) traceMsg(comp(), "\t\tEnd path because coldness changed from block_%d to block_%d block->asBlock()->getExit()=%p block->asBlock()->getExit()->getNextTreeTop()=%p\n", block->asBlock()->getNumber(),bestSucc->asBlock()->getNumber(), block->asBlock()->getExit(), block->asBlock()->getExit()->getNextTreeTop());
         return true;
        }
      }


   //????: I think that's wrong
   //if (!block->asBlock()->isCold() && bestSucc->asBlock()->isCold())
   if (!_changeBlockOrderBasedOnHWProfile
         &&  block->asBlock()->isCold() != bestSucc->asBlock()->isCold())
      {
      if (trace()) traceMsg(comp(), "\t\tEnd path because coldness changed from block_%d to block_%d \n", block->asBlock()->getNumber(),bestSucc->asBlock()->getNumber());
      return true;
      }

   if (block->asBlock()->getExit())
      {
      TR::TreeTop *lastTT = block->asBlock()->getLastRealTreeTop();
      if (lastTT != NULL)
         {
         TR::Node *maybeBranchNode = lastTT->getNode();
         if (maybeBranchNode->getOpCode().isJumpWithMultipleTargets())
            return false;
         }
      }

   if (block->getSuccessors().size() == 1)
      return false;

   TR_BlockStructure *bestSuccStructure = bestSucc->asBlock()->getStructureOf();
   TR_BlockStructure *blockStructure = block->asBlock()->getStructureOf();
   if (bestSuccStructure == NULL || blockStructure == NULL)
      {
      if (trace()) traceMsg(comp(), "\t\tEnd path because structure is NULL block_%d=%p to block_%d=%p \n", block->asBlock()->getNumber(),blockStructure, bestSucc->asBlock()->getNumber(),bestSuccStructure);
      return true;
      }

   if (bestSuccStructure->getContainingLoop() != blockStructure->getContainingLoop())
      {
      int32_t bestSuccNum = bestSuccStructure->getNumber();
      TR_Structure *bestSuccLoop = bestSuccStructure->getContainingLoop();
      TR_Structure *blockLoop = blockStructure->getContainingLoop();
      while (bestSuccLoop &&
             (bestSuccLoop->getNumber() == bestSuccNum))
         {
         bestSuccLoop = bestSuccLoop->getContainingLoop();
         }

      return (bestSuccLoop != blockLoop);
      if (bestSuccLoop != blockLoop)
         {
         if (trace()) traceMsg(comp(), "\t\tEnd path because different loop block_%d=%p to block_%d=%p \n", block->asBlock()->getNumber(),blockLoop, bestSucc->asBlock()->getNumber(),bestSuccLoop);
         return true;
         }
         else return false;
      }
   else return false;
   ///return (bestSuccStructure->getContainingLoop() != blockStructure->getContainingLoop());
   }

bool TR_OrderBlocks::analyseForHazards(TR::CFGNode *block)
   {
   TR::TreeTop *tt = block->asBlock()->getEntry();
   if (tt)
      {
      for ( ; tt != block->asBlock()->getExit(); tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (node != NULL)
            {
            if (node->getOpCode().isCall())
               return true;
            if (node->getOpCode().isReturn())
               return true;
            if (node->getOpCodeValue() == TR::athrow)
               return true;
            }
         }
      }
   return false;
   }

bool TR_OrderBlocks::isCandidateReallyBetter(TR::CFGEdge *candEdge, TR::Compilation *comp)
   {
   TR::CFGEdgeList & predecessors = candEdge->getTo()->getPredecessors();
   //traceMsg(comp, "iCRB cand block_%d\n", candEdge->getTo()->getNumber());
   for (auto predEdge = predecessors.begin(); predEdge != predecessors.end(); ++predEdge)
      {
      TR::CFGEdgeList & predSuccessors = (*predEdge)->getFrom()->getSuccessors();
      float numFactor = 1.5;
      for (auto predSuccEdge = predSuccessors.begin(); predSuccEdge != predSuccessors.end(); ++predSuccEdge)
         {

         if (_changeBlockOrderBasedOnHWProfile && ((*predSuccEdge)->getFrom()->getVisitCount() == _visitCount))
            continue;

	   //traceMsg(comp, "comparing pred edge  %d -> %d freq %d\n", predSuccEdge->getFrom()->getNumber(), predSuccEdge->getTo()->getNumber(), predSuccEdge->getFrequency());
	   //traceMsg(comp, "cand edge %d -> %d freq %d\n", candEdge->getFrom()->getNumber(), candEdge->getTo()->getNumber(), candEdge->getFrequency());
         if (((*predSuccEdge) != candEdge) &&
               ((float)(*predSuccEdge)->getFrequency() > (float)(numFactor * (float)candEdge->getFrequency())))
            //return predSuccEdge->getFrom();
            {
	      //traceMsg(comp, "rejecting cand block_%d\n", candEdge->getTo()->getNumber());
            return false;
            }
         }
      }
   ///return candEdge->getFrom();
   return true;
   }


static int32_t numTransforms = 0;

static bool isCandidateTheHottestSuccessor(TR::CFGEdge *candEdge, TR::Compilation *comp)
   {
///return true;
   static char *pEnv = feGetEnv("TR_pNum");
   int32_t count = 0;
   if (pEnv) count = atoi(pEnv);

   if (!comp->getFlowGraph()->getStructure()) // we cannot trust below checks if someone invalidated structure earlier in this opt
      return true;

   TR::CFGEdgeList & predecessors = candEdge->getTo()->getPredecessors();
   TR_BlockStructure *candBlock = candEdge->getTo()->asBlock()->getStructureOf();
   if (candBlock && candBlock->getContainingLoop() &&
         candBlock->getContainingLoop()->getNumber() == candBlock->getNumber())
      return true;
   if (candBlock)
   ///if (1)
      {
      for (TR_Structure *s = candBlock->getParent(); s; s = s->getParent())
            {
            TR_RegionStructure *region = s->asRegion();
            if (region && region->containsInternalCycles())
               {
               return true;
               }
             }
      }
   //traceMsg(comp, "iCHS cand block_%d\n", candEdge->getTo()->getNumber());
   for (auto predEdge = predecessors.begin(); predEdge != predecessors.end(); ++predEdge)
      {
      if (((*predEdge)->getFrequency() > candEdge->getFrequency()) &&
          ((*predEdge)->getFrom() != candEdge->getTo()))
         {
         ///numTransforms++;
         //traceMsg(comp, "iCHS rejecting cand block_%d numTransforms %d\n", candEdge->getTo()->getNumber(), numTransforms);
         ///if (numTransforms < count)
           {
           //traceMsg(comp, "really iCHS rejecting cand block_%d numTransforms %d\n", candEdge->getTo()->getNumber(), numTransforms);
           return false;
           }
         }
      }
   return true;
   }


bool TR_OrderBlocks::candidateIsBetterSuccessorThanBest(TR::CFGEdge *candidateEdge, TR::CFGEdge *currentBestEdge)
   {
   TR::CFGNode *candidate = candidateEdge->getTo();
   TR::CFGNode *currentBest = currentBestEdge->getTo();
   TR::CFGNode *prevBlock = candidateEdge->getFrom();

   TR_ASSERT(prevBlock == currentBestEdge->getFrom(), "edges are from different blocks");

   // is succBlock a better choice than current best?
   if (trace())
      traceMsg(comp(), "\tComparing candidate %d(%d) to current best %d(%d) as successor for %d(%d)\n", candidate->getNumber(), candidate->getFrequency(), currentBest->getNumber(), currentBest->getFrequency(), prevBlock->getNumber(), prevBlock->getFrequency());


   //if (!_superColdBlockOnly)
   //   {
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
         {
         if (candidateEdge->getFrequency() >= 0)
            {
            if (candidateEdge->getFrequency() == currentBestEdge->getFrequency())
               {
               OMR::TreeTop *lexicalSuccessorEntry = candidateEdge->getFrom()->asBlock()->getExit()->getNextTreeTop();
               if (lexicalSuccessorEntry != NULL && lexicalSuccessorEntry->getNode()->getBlock() == candidateEdge->getTo())
                  {
                  if (trace()) traceMsg(comp(), "\t\tis equally hot, but is currently the lexical successor, making it my best choice\n");
                  return true;
                  }
               }
            if (candidateEdge->getFrequency() > currentBestEdge->getFrequency())
               {
               if (trace()) traceMsg(comp(), "\t\thas hotter edge, making it my best choice\n");
               return true;
               }
            else if (candidateEdge->getFrequency() < currentBestEdge->getFrequency())
               {
               if (trace()) traceMsg(comp(), "\t\thas colder edge than my best choice, so discarding\n");
               return false;
               }
            }
         }

       if (candidate->hasSuccessor(currentBest))
          {
          if (candidate->getFrequency() > (prevBlock->getFrequency() - candidate->getFrequency()))
             {
             if (trace()) traceMsg(comp(), "\t\thas has current best succ as a succ, detecting an if-then structure and making the if block my best choice\n");
             return true;
             }
          }

      if (candidate->getFrequency() >= 0)
         {
         // candidate is better if it's hotter than bestSuccessor
         if (candidate->getFrequency() > currentBest->getFrequency())
            {
            if (trace()) traceMsg(comp(), "\t\tis hotter, making it my best choice\n");
            return true;
            }
         else if (candidate->getFrequency() < currentBest->getFrequency())
            {
            if (trace()) traceMsg(comp(), "\t\tis colder than my best choice, so discarding\n");
            return false;
            }
         }

      if (!_changeBlockOrderBasedOnHWProfile)
         {
         // candidate is not cold, currentBest is cold
         if (!candidate->asBlock()->isCold() && currentBest->asBlock()->isCold())
            {
            if (trace()) traceMsg(comp(), "\t\tcurrent best choice is cold but this one isn't, making it my best choice\n");
            return true;
            }
         else if (candidate->asBlock()->isCold() && !currentBest->asBlock()->isCold())
            {
            if (trace()) traceMsg(comp(), "\t\tis cold while current best choice isn't cold, so discarding\n");
            return false;
            }
         }
   //}

   // candidate is better if it's more deeply nested than currentBest
   if (candidate->asBlock()->getNestingDepth() > currentBest->asBlock()->getNestingDepth())
      {
      if (trace()) traceMsg(comp(), "\t\thas deeper nesting level, making it my best choice\n");
      return true;
      }
   else if (candidate->asBlock()->getNestingDepth() < currentBest->asBlock()->getNestingDepth())
      {
      if (trace()) traceMsg(comp(), "\t\thas lower nesting level than my best choice, so discarding\n");
      return false;
      }

   // ok, we're left to static heuristics now
   //TR::Node *branchNode = prevBlock->asBlock()->getLastRealTreeTop()->getNode();
   bool bestHasHazards = analyseForHazards(currentBest);
   bool candHasHazards = analyseForHazards(candidate);
   if (bestHasHazards && !candHasHazards)
      {
      if (trace()) traceMsg(comp(), "\t\tbest choice has hazards but candidate doesn't, making it my best choice\n");
      //numberHazardCandidates++;
      return true;
      }

   // we've exhausted good ideas for choosing, so now try to choose simply based on extension opportunity
   if (!(currentBest->getPredecessors().size() == 1) && (candidate->getPredecessors().size() == 1))
      {
      if (trace()) traceMsg(comp(), "\t\tbetter candidate for extension, making it my best choice\n");
      return true;
      }

   return false;
   }

TR::CFGNode *TR_OrderBlocks::chooseBestFallThroughSuccessor(TR::CFG *cfg, TR::CFGNode *block, int32_t & numCandidates)
   {
   static char *reallyNewReordering=feGetEnv("TR_reallyNewReordering");

   List<TR::CFGEdge> candidateEdges(trMemory());
   //int32_t numCandidates = 0;
   TR::CFGEdge *bestSuccessorEdge = NULL;

   if (block == NULL)
      return NULL;

   if (trace()) traceMsg(comp(), "Block %d: looking for best successor\n", block->getNumber());

   // first, build up a list of potential choices: unvisited successors
   TR::CFGEdgeList & successors = block->getSuccessors();
   for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
      {
      TR::Block *succBlock = (*succEdge)->getTo()->asBlock();

      if (trace()) traceMsg(comp(), "\t\texamining successor %d\n", succBlock->getNumber());

      if (reallyNewReordering == NULL && succBlock->getVisitCount() == _visitCount)
         {
         if (trace()) traceMsg(comp(), "\t\tblock already visited\n");
         continue;
         }

      if (reallyNewReordering != NULL && succBlock->getVisitCount() != _visitCount)
         {
         if (trace()) traceMsg(comp(), "\t\tblock not yet visited\n");
         continue;
         }

      if (cannotFollowBlock(succBlock, block->asBlock()))
         {
         if (trace()) traceMsg(comp(), "\t\tcannot follow block, so we can't choose it\n");
         continue;
         }

      if (mustFollowBlock(succBlock, block->asBlock()))
         {
         if (trace()) traceMsg(comp(), "\t\tmust follow block, so we have to choose it\n");
         numCandidates = 1;
         return succBlock;
         }

      if (block->hasExceptionSuccessor(succBlock))
         {
         if (trace()) traceMsg(comp(), "\t\texceptional successor: not a candidate\n");
         continue;
         }

      if (block->asBlock()->getExit() && block->asBlock()->getExit()->getNextTreeTop() &&
          succBlock == block->asBlock()->getExit()->getNextTreeTop()->getNode()->getBlock())
         {
         bestSuccessorEdge = *succEdge;
         if (trace()) traceMsg(comp(), "\t\tfound original fall-through successor %d: making it initial best successor\n", succBlock->getNumber());
         }


      if (trace()) traceMsg(comp(), "\t\trecording as a candidate\n");

      candidateEdges.add(*succEdge);
      numCandidates++;
      }

   // no-brainer if there's only one to choose from!
   if (numCandidates == 1)
      {
      TR::CFGNode *theCandidate = candidateEdges.popHead()->getTo();
      TR::Block *candBlock    = theCandidate->asBlock();
      TR::Block *headBlock    = NULL;
      if (trace()) traceMsg(comp(), "\tOnly one candidate %d\n", theCandidate->getNumber());

      if (!_changeBlockOrderBasedOnHWProfile)
         {
         // Bail if it is cold and we still have hot block to chose from
         if (candBlock->isSuperCold())
            {
            if (!_hotPathList.isEmpty())
               {
               if (trace()) traceMsg(comp(), "\tcandidate %d is cold, discard because there are still hot choices \n", theCandidate->getNumber());
               return NULL;
               }
            else if (!_coldPathList.isEmpty() && !_coldPathList.getHeadData()->asBlock()->isSuperCold())
               {
               if (trace()) traceMsg(comp(), "\tcandidate %d is super cold, discard because there are still choices which are not so cold \n", theCandidate->getNumber());
               return NULL;
               }
            }
         }
      else
      {
         // If HW Profile info exists take the block frequency into account
         if (candBlock->getFrequency() == 0 && hasValidCandidate(_hotPathList, block))
            {
            if (trace())
               traceMsg(comp(), "HW Profile: candidate %d has freq zero, discard because there are still hot choices \n", candBlock->getNumber());
            return NULL;
            }
      }

      static const char *envp = feGetEnv("TR_NEARLYEXIT");

      // if candBlock is a loop exit but the loop has multiple exits, choose the hottest exit instead of candBlock
      if (!envp && cfg->getStructure())
         //Make sure we still have structure info
         {
         TR_Structure *innerLoop = block->asBlock()->getStructureOf()->getContainingLoop();

         // the block is contained in the inner loop
         while (innerLoop)
            {
            TR_Structure *outerLoop = candBlock->getStructureOf()->getContainingLoop() ;

            //outer loop does not exist or contains the inner loop
            if (!outerLoop || ((innerLoop!=outerLoop) && outerLoop->contains(innerLoop)))
               {
               TR::CFGEdgeList& successors = block->asBlock()->getSuccessors();
               if (successors.size() !=2 )
                  break;
               //get exit edge frequency
               int16_t exitFrequency=0;
               for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
                  {
                  if ((*succEdge)->getTo()->asBlock()->getNumber()==theCandidate->getNumber())
                     {
                     exitFrequency=(*succEdge)->getFrequency();
                     if (trace())
                        traceMsg(comp(), "\t -> block_%d\tfrequency %4d\n", (*succEdge)->getTo()->getNumber(), (*succEdge)->getFrequency());
                     }
                  }

               if (!exitFrequency)
                  break;

               //get all exit edges including early exit

               if (trace())
                  {
                  traceMsg(comp(), "\tThis is the loop exit: %d -> %d\n", block->getNumber(), theCandidate->getNumber());
                  traceMsg(comp(), "\t\tInner loop %d\n", innerLoop->getNumber());
                  traceMsg(comp(), "\t\tOuter loop %d\n", outerLoop?outerLoop->getNumber():-1);
                  }

               List<TR::CFGEdge>& exitEdges= innerLoop->asRegion()->getExitEdges();

               if (exitEdges.getSize()>1)
                  {

                  List<TR::CFGEdge> rEdges(trMemory());
                  List<TR::Block> rBlocks(trMemory());

                  innerLoop->asRegion()->collectExitBlocks(&rBlocks, &rEdges);

                  if (trace()) traceMsg(comp(), "\t rEdges.getSize()=%d\n", rEdges.getSize());

                  ListIterator<TR::CFGEdge> exitIt(&rEdges);

                  for (TR::CFGEdge *edge = exitIt.getFirst(); edge; edge = exitIt.getNext())
                     {
                     if (edge->getTo()->asBlock()->getVisitCount() != _visitCount)
                        {

                        if (trace())
                           traceMsg(comp(), "\t -> block_%d\tfrequency %4d, current exit frequency %4d\n",edge->getTo()->getNumber(), edge->getFrequency(), exitFrequency);

                        if (edge->getFrequency() > exitFrequency)
                           {
                           theCandidate = edge->getTo();
                           candBlock = theCandidate->asBlock();
                           exitFrequency=edge->getFrequency();

                           if (trace())
                              traceMsg(comp(), "\t Loop %d has multiple exits, consider hotter exit %d\n", innerLoop->getNumber(), theCandidate->getNumber() );
                           }

                        }
                     }
                  }
               }
            // end of the while
            break;
            }
         }

         return theCandidate;
      }

   if (numCandidates == 0)
      {
      if (trace()) traceMsg(comp(), "\tNo candidates\n");
      return NULL;
      }

   if (trace()) traceMsg(comp(), "\tMultiple candidates, have to choose:\n");

   // we now know there are multiple candidates, so we have to choose the best one
   ListIterator<TR::CFGEdge> succCandEdgeIt(&candidateEdges);
   if (bestSuccessorEdge == NULL)
      {
      for (TR::CFGEdge* succEdge = succCandEdgeIt.getFirst() ; succEdge; succEdge = succCandEdgeIt.getNext())
         {
         if (isCandidateReallyBetter(succEdge, comp()))
            {
            bestSuccessorEdge = succEdge;
            break;
            }
         }
      if (trace())
         traceMsg(comp(), "iCRB initial best %p\n", bestSuccessorEdge);

      if (!bestSuccessorEdge) return NULL;

      if (trace())
         traceMsg(comp(), "iCRB initial best %d\n", bestSuccessorEdge->getTo()->getNumber());
      }


   if (trace()) traceMsg(comp(), "\tInitial best candidate is %d(%d)\n",  bestSuccessorEdge->getTo()->getNumber(), bestSuccessorEdge->getTo()->getFrequency());
   candidateEdges.remove(bestSuccessorEdge);

   succCandEdgeIt.reset();
   for (TR::CFGEdge* succEdge = succCandEdgeIt.getFirst() ; succEdge; succEdge = succCandEdgeIt.getNext())
      {
      if (succEdge == bestSuccessorEdge)
         continue;

      if (trace()) traceMsg(comp(), "\tExamining candidate %d(%d)\n", succEdge->getTo()->getNumber(), succEdge->getTo()->getFrequency());

      if (candidateIsBetterSuccessorThanBest(succEdge, bestSuccessorEdge) &&
           isCandidateReallyBetter(succEdge, comp()))
         {
         bestSuccessorEdge = succEdge;
         }
      else
         {
         if (trace()) traceMsg(comp(), "\t\tworse than my current best choice\n");
         }
      }

   if (trace()) traceMsg(comp(), "\tBest successor is %d\n", bestSuccessorEdge->getTo()->getNumber());

   TR::CFGEdgeList & predecessors = bestSuccessorEdge->getTo()->getPredecessors();
   for (auto predEdge = predecessors.begin(); predEdge != predecessors.end(); ++predEdge)
      {
      TR::CFGNode *pred = (*predEdge)->getFrom();
      if (pred != block)
         {
         auto predSuccEdge = successors.begin();
         for (; predSuccEdge != successors.end(); ++predSuccEdge)
            {
            TR::CFGNode *predSucc = (*predSuccEdge)->getTo();
            if (predSucc != bestSuccessorEdge->getTo() && predSucc->getFrequency() > bestSuccessorEdge->getTo()->getFrequency())
               break;  // predecessor's hottest fall-through isn't bestSuccessor
            }

         if (predSuccEdge == successors.end())
            {
            if (trace()) traceMsg(comp(), "\t\tbut it has another predecessor %d for which it is the hottest successor\n", pred->getNumber());
            if (trace()) traceMsg(comp(), "\t\tcounting this block as a candidate for replication\n", pred->getNumber());
            numberMethodReplicationCandidates++;
            }
         }
      }

   if (_changeBlockOrderBasedOnHWProfile)
      {
      TR::Block *candBlock = bestSuccessorEdge->getTo()->asBlock();
      //If HW Profile info exists take it into account
      if (candBlock->getFrequency() == 0 && hasValidCandidate(_hotPathList, block))
         {
         if (trace())
            traceMsg(comp(), "HW Profile: candidate %d has freq zero, discard because there are still hot choices \n", candBlock->getNumber());
         return NULL;
         }
      }
   return bestSuccessorEdge->getTo();
   }

void TR_OrderBlocks::removeFromOrderedBlockLists(TR::CFGNode *block)
   {
   _coldPathList.remove(block);
   _hotPathList.remove(block);
   if (_changeBlockOrderBasedOnHWProfile && block->getFrequency() > 0)
      {
      _numUnschedHotBlocks--;
      if (trace()) traceMsg(comp(), "\t_numUnschedHotBlocks remove %d (blockNum:%d) \n",
            _numUnschedHotBlocks, block->getNumber());
      }
   }

void TR_OrderBlocks::addToOrderedBlockList(TR::CFGNode *block, TR_BlockList & list, bool useNumber)
   {
   TR_BlockListIterator iter(&list);
   TR_BlockListElement *prevBlockElement=NULL;
   TR::CFGNode *listBlock;
   for (listBlock=iter.getFirst(); listBlock != NULL; prevBlockElement=iter.getCurrentElement(),listBlock=iter.getNext() )
      {
      TR::Block *cBlock = block->asBlock();
      TR::Block *lBlock = listBlock->asBlock();

      // if it's already on the list, don't bother adding it again
      if (block->getNumber() == listBlock->getNumber())
         return;


      // guard NestingDepth and Number are important for performance of RELBench
      if (!_superColdBlockOnly)
         {
         if (cBlock->isCold() && !cBlock->isSuperCold() &&
               lBlock->isSuperCold())
            break;
         else if (cBlock->isSuperCold() &&
               lBlock->isCold() && !lBlock->isSuperCold())
            continue;

         if (block->getFrequency() > listBlock->getFrequency())
            break;
         else if (block->getFrequency() < listBlock->getFrequency())
            continue;

         if (cBlock->getNestingDepth() > lBlock->getNestingDepth())
            break;
         else if (cBlock->getNestingDepth() < lBlock->getNestingDepth())
            continue;

         if (useNumber && block->getNumber() < listBlock->getNumber())
            break;
         }
      }

   if (prevBlockElement)
      list.addAfter(block, prevBlockElement);
   else
      list.add(block);

   }

void TR_OrderBlocks::addRemainingSuccessorsToList(TR::CFGNode *block, TR::CFGNode *excludeBlock)
   {

   // everything is already on the list at the beginning
   if (_superColdBlockOnly) return;

   if (trace()) traceMsg(comp(), "\tadding remaining successors of block_%d to queue\n", block->getNumber());

   TR::CFGEdgeList & successors = block->getSuccessors();
   for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
      {
      TR::CFGNode *succBlock = (*succEdge)->getTo();
      // If the edge is created by BE, don't add block 1 to list for now
      if (succBlock != excludeBlock && succBlock->getVisitCount() != _visitCount &&
           isCandidateTheHottestSuccessor(*succEdge, comp()))
         {
         if (succBlock->asBlock()->isCold())
            {
            if (trace()) traceMsg(comp(), "\t\tAdding unvisited cold successor %d\n", succBlock->getNumber());
            addToOrderedBlockList(succBlock, _coldPathList, true);
            }
         else
            {
            if (trace()) traceMsg(comp(), "\t\tAdding unvisited non-cold successor %d\n", succBlock->getNumber());
            addToOrderedBlockList(succBlock, _hotPathList, false);
            }
         }
      }

   TR::CFGEdgeList & excSuccessors = block->getExceptionSuccessors();
   for (auto excSuccEdge = excSuccessors.begin(); excSuccEdge != excSuccessors.end(); ++excSuccEdge)
      {
      TR::CFGNode *succBlock = (*excSuccEdge)->getTo();
      if (succBlock->getVisitCount() != _visitCount)
         {
         if (succBlock->asBlock()->isCold() && succBlock->asBlock()->getFrequency() <= 0)
            {
            if (trace()) traceMsg(comp(), "\t\tAdding unvisited cold exception successor %d\n", succBlock->getNumber());
            addToOrderedBlockList(succBlock, _coldPathList, true);
            }
         else
            {
            if (trace()) traceMsg(comp(), "\t\tAdding unvisited non-cold exception successor %d\n", succBlock->getNumber());
            addToOrderedBlockList(succBlock, _hotPathList, false);
            }
         }
      }
   }


//Take the successor's frequencies when considering which successor to add to the hot list
void TR_OrderBlocks::addRemainingSuccessorsToListHWProfile(TR::CFGNode *block, TR::CFGNode *excludeBlock)
   {

   if (trace()) traceMsg(comp(), "\tadding remaining successors of block_%d to queue\n", block->getNumber());

   TR::CFGEdgeList & successors = block->getSuccessors();
   for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
      {
      TR::CFGNode *succBlock = (*succEdge)->getTo();
      // If the edge is created by BE, don't add block 1 to list for now
      if (succBlock != excludeBlock && succBlock->getVisitCount() != _visitCount &&
            succBlock->getFrequency() > 0)
         {
         if (trace()) traceMsg(comp(), "\t\tAdding unvisited non-cold successor %d\n", succBlock->getNumber());
         addToOrderedBlockList(succBlock, _hotPathList, false);
         }
      }

   TR::CFGEdgeList & excSuccessors = block->getExceptionSuccessors();
   for (auto excSuccEdge = excSuccessors.begin(); excSuccEdge != excSuccessors.end(); ++excSuccEdge)
      {
      TR::CFGNode *succBlock = (*excSuccEdge)->getTo();
      if (succBlock->getVisitCount() != _visitCount && succBlock->getFrequency() > 0)
         {
         addToOrderedBlockList(succBlock, _hotPathList, false);
         }
      }
   }


// pattern: block has goto to fall-through block
//   block assumed to end in a goto
// returns TRUE if the pattern was found and replaced
bool TR_OrderBlocks::peepHoleGotoToFollowing(TR::CFG *cfg, TR::Block *block, TR::Block *followingBlock, char *title)
   {
   // pattern: block has goto to fall-through block
   TR::Node *gotoNode = block->getLastRealTreeTop()->getNode();
   if (branchToFollowingBlock(gotoNode, block, comp()))
      {
      // remove the goto statement
      if (performTransformation(comp(), "%s dest of goto in block_%d is the following block_%d, removing the goto node\n", title, block->getNumber(), followingBlock->getNumber()))
         {
         TR::TreeTop *nextTreeTop = block->getLastRealTreeTop()->getNextTreeTop();
         // for PLX, there may be something like unrestricted after goto
         block->getLastRealTreeTop()->getPrevTreeTop()->join(nextTreeTop);
         gotoNode->recursivelyDecReferenceCount();
         return true;
         }
      }
   return false;
   }

// pattern: block contains goto to a block that only contains a goto
//   block assumed to end in a goto
// returns TRUE if the pattern was found and replaced
bool TR_OrderBlocks::peepHoleGotoToGoto(TR::CFG *cfg, TR::Block *block, TR::Node *gotoNode, TR::Block *destOfGoto, char *title,
                                        TR::BitVector &skippedGotoBlocks)
   {
   if (comp()->getProfilingMode() == JitProfiling)
      return false;

   if (destOfGoto->isGotoBlock(comp(),true)
       )
      {
      // redirect first goto to the dest of the second goto
      TR::Block *finalGotoBlock = destOfGoto->getSuccessors().front()->getTo()->asBlock();

      // exclude goto block that points to itself
      if (finalGotoBlock == destOfGoto) return false;

      // exclude if this is an infinite loop of goto blocks
      if (skippedGotoBlocks[destOfGoto->getNumber()]) // already skipped
         return false;

      skippedGotoBlocks[destOfGoto->getNumber()] = 1;

      if (performTransformation(comp(), "%s in block_%d, dest of goto (%d) is also goto block, forwarding destination (%d) back into goto\n", title, block->getNumber(), destOfGoto->getNumber(), finalGotoBlock->getNumber()))
         {
         TR_RegionStructure *parent = destOfGoto->getCommonParentStructureIfExists(finalGotoBlock, comp()->getFlowGraph());
         if (parent &&
             parent->isNaturalLoop() &&
             (parent->getNumber() == destOfGoto->getNumber()))
            cfg->setStructure(0); // to avoid automatic structure repair creating an improper region due to temporary state after adding the first edge

         TR::TreeTop *finalGotoDest = finalGotoBlock->getEntry();
         gotoNode->setBranchDestination(finalGotoDest);
         cfg->addEdge(block, finalGotoBlock);
         cfg->removeEdge(block, destOfGoto);
         return true;
         }
      }
   return false;
   }

// pattern: block contains a goto to an empty block
//   block assumed to end in a goto
// returns TRUE if the pattern was found and replaced
bool TR_OrderBlocks::peepHoleGotoToEmpty(TR::CFG *cfg, TR::Block *block, TR::Node *gotoNode, TR::Block *destOfGoto, char *title)
   {
   if (comp()->getProfilingMode() == JitProfiling)
      return false;

   if (destOfGoto->isEmptyBlock() && !(destOfGoto->getStructureOf() && destOfGoto->getStructureOf()->isLoopInvariantBlock()) &&
       !(block->getStructureOf() && block->getStructureOf()->isLoopInvariantBlock()))
      {
      // redirect the goto to the fall-through of the empty block
      TR::TreeTop *destFallThroughDest = destOfGoto->getExit()->getNextTreeTop();
      if (destFallThroughDest != NULL)
         {
         TR::Block *destFallThroughBlock = destFallThroughDest->getNode()->getBlock();
         if (performTransformation(comp(), "%s in block_%d, dest of goto is empty block, forwarding destination (%d) back into goto\n", title, block->getNumber(), destFallThroughBlock->getNumber()))
            {
            TR_RegionStructure *parent = destOfGoto->getCommonParentStructureIfExists(destFallThroughBlock, comp()->getFlowGraph());
            if (parent &&
                parent->isNaturalLoop() &&
                (parent->getNumber() == destOfGoto->getNumber()))
               cfg->setStructure(0); // to avoid automatic structure repair creating an improper region due to temporary state after adding the first edge

            gotoNode->setBranchDestination(destFallThroughDest);
            cfg->addEdge(block, destFallThroughBlock);
            cfg->removeEdge(block, destOfGoto);
            destFallThroughBlock->setIsExtensionOfPreviousBlock(false);
            return true;
            }
         }
      }
   return false;
   }

static bool peepHoleGotoToLoopHeader(TR::CFG *cfg, TR::Block *block, TR::Block *dest, char *title)
   {
   // try to peephole the following:
   // dest:      loopHeader
   // predBlock: branch -> exit
   // block:     goto loopHeader
   //
   // and change into
   // dest:      loopHeader
   // predBlock: reverse-branch -> loopHeader
   // block:     goto exit
   //
   bool success = false;
   TR_Structure *destStructure = dest->getStructureOf();
   TR_Structure *blockStructure = block->getStructureOf();
   if (destStructure && blockStructure)
      {
      TR_Structure *parent = destStructure->getParent();
      bool destIsSibling = (parent == blockStructure->getParent());
      bool isSingleton = (block->getPredecessors().size() == 1);
      TR::Block *predBlock = NULL;
      bool predBlockIsSibling = false;
      if (isSingleton)
         {
         predBlock = toBlock((block->getPredecessors().front())->getFrom());
         if (predBlock && predBlock->getStructureOf())
            predBlockIsSibling = (predBlock->getStructureOf()->getParent() == blockStructure->getParent());
         }
      if (parent && parent->asRegion() && destIsSibling && predBlockIsSibling &&
          predBlock && predBlock->endsInBranch() &&
          (destStructure->getNumber() == parent->getNumber()))
         {
         TR::Node *branchNode = predBlock->getLastRealTreeTop()->getNode();

         TR::Block *branchDest = branchNode->getBranchDestination()->getNode()->getBlock();
         bool blockIsFallThrough = (predBlock->getNextBlock() == block);

         if(branchNode->getOpCodeValue() == TR::treetop)
             branchNode = branchNode->getFirstChild();

         if (!branchNode->isTheVirtualGuardForAGuardedInlinedCall() &&
             (branchDest != dest) && blockIsFallThrough && (branchNode->getOpCodeValue() != TR::ibranch && branchNode->getOpCodeValue() != TR::mbranch) &&
             performTransformation(cfg->comp(), "%s applied goto-loop header peephole for block_%d dest %d\n", title, block->getNumber(), dest->getNumber()))
            {
            success = true;
            block->getLastRealTreeTop()->getNode()->setBranchDestination(branchDest->getEntry());
            cfg->addEdge(TR::CFGEdge::createEdge(block,  branchDest, cfg->trMemory()));
            cfg->addEdge(TR::CFGEdge::createEdge(predBlock,  dest, cfg->trMemory()));
            cfg->removeEdge(block, dest);
            cfg->removeEdge(predBlock, branchDest);
            branchNode->reverseBranch(dest->getEntry());
            }
         }
      }

   return success;
   }
//


// repeatedly apply peephole optimizations to blocks that end in a goto until no more can be done
// assume block->endsInGoto()
// returns TRUE if block was removed
//   currently always returns false because this code can't remove block itself
void TR_OrderBlocks::peepHoleGotoBlock(TR::CFG *cfg, TR::Block *block, char *title)
   {
   TR_ASSERT(block->endsInGoto(), "peepHoleGotoBlock called on block that doesn't end in a goto!");
   bool madeAChange;
   TR::Node *gotoNode = block->getLastRealTreeTop()->getNode();
   bool ranLoopHeaderPeepholeOnce = false;
   TR::BitVector skippedGotoBlocks(comp()->allocator());
   do
      {
      madeAChange = false;

      if (trace()) traceMsg(comp(), "\t\tlooking for goto optimizations:\n");

      TR::Block *destOfGoto = block->getSuccessors().front()->getTo()->asBlock();
      if (peepHoleGotoToGoto(cfg, block, gotoNode, destOfGoto, title, skippedGotoBlocks))
         madeAChange = true;

      else if (peepHoleGotoToEmpty(cfg, block, gotoNode, destOfGoto, title))
         madeAChange = true;
      else if (!ranLoopHeaderPeepholeOnce && block->isGotoBlock(comp()) && peepHoleGotoToLoopHeader(cfg, block, destOfGoto, title))
         {
         madeAChange = true;
         ranLoopHeaderPeepholeOnce = true;
	 _donePeepholeGotoToLoopHeader = true;
         }
      }
   while (block->endsInGoto() && madeAChange);
   }

void TR_OrderBlocks::removeRedundantBranch(TR::CFG *cfg, TR::Block *block, TR::Node *branchNode, TR::Block *takenBlock)
   {
   // remove the branch node
   branchNode->recursivelyDecReferenceCount();
   TR::TreeTop *nextTreeTop = block->getLastRealTreeTop()->getNextTreeTop();
   // for PLX, there may be something like unrestricted after goto
   block->getLastRealTreeTop()->getPrevTreeTop()->join(nextTreeTop);
   //block->getLastRealTreeTop()->getPrevTreeTop()->join(block->getExit());

   // make sure there is only one edge to this successor
   TR_SuccessorIterator ei(block);
   bool seenEdgeToTakenBlock=false;
   for (TR::CFGEdge * edge = ei.getFirst(); edge != NULL; edge = ei.getNext())
      if (edge->getTo() == takenBlock)
         {
         if (seenEdgeToTakenBlock)
            cfg->removeEdge(block, takenBlock);
         else
            seenEdgeToTakenBlock = true;
         }

   }


bool TR_OrderBlocks::peepHoleBranchToLoopHeader(TR::CFG *cfg, TR::Block *block, TR::Block *fallThrough, TR::Block *dest, char *title)
   {
   bool success = false;
   TR_Structure *destStructure = dest->getStructureOf();
   TR_Structure *fallThroughStructure = fallThrough->getStructureOf();
   TR_Structure *blockStructure = block->getStructureOf();
   if (destStructure && fallThroughStructure && blockStructure)
      {
      TR_RegionStructure *parent = blockStructure->getParent();
      bool fallThroughIsSibling = (fallThroughStructure->getParent() == parent);
      bool destIsSibling = (destStructure->getParent() == parent);
      TR::Block *predBlock = NULL;
      TR::Node *branchNode = block->getLastRealTreeTop()->getNode();
      if(branchNode->getOpCodeValue() == TR::treetop)
          branchNode = branchNode->getFirstChild();

      if (parent && parent->asRegion() && fallThroughIsSibling && !destIsSibling &&
          (fallThroughStructure->getNumber() == parent->getNumber()) &&
          !branchNode->isTheVirtualGuardForAGuardedInlinedCall() && (branchNode->getOpCodeValue() != TR::ibranch && branchNode->getOpCodeValue() != TR::mbranch) &&
          /* !dest->isCold() &&
 */
          performTransformation(comp(), "%s applied loop header peephole for block_%d fall through %d dest %d\n", title, block->getNumber(), fallThrough->getNumber(), dest->getNumber()))
         {
         success = true;

         TR::TreeTop *fallThroughTT = fallThrough->getEntry();

         TR::Block *gotoBlock = insertGotoFallThroughBlock(dest->getEntry(), dest->getEntry()->getNode(),
                                                          block, dest);

         TR::TreeTop *exitTT = block->getExit();
         exitTT->join(gotoBlock->getEntry());
         gotoBlock->getExit()->join(fallThroughTT);
         branchNode->reverseBranch(fallThroughTT);
         }
      }

   return success;
   }

// apply peephole optimizations to blocks that end in a branch and branch around a single goto
void TR_OrderBlocks::peepHoleBranchAroundSingleGoto(TR::CFG *cfg, TR::Block *block, char *title)
   {
   TR_ASSERT(block->endsInBranch(), "peepHoleBranchBlock called on block that doesn't end in a branch!");
   TR::Node *branchNode = block->getLastRealTreeTop()->getNode();
   TR::TreeTop *takenEntry = branchNode->getBranchDestination();
   TR::Block *takenBlock = takenEntry->getNode()->getBlock();

   TR::TreeTop *fallThroughEntry = block->getExit()->getNextTreeTop();
   TR::Block *fallThroughBlock = fallThroughEntry->getNode()->getBlock();

#if 1
   //if we branch around a jump (only a jump), then redirect our branch direction
   if (!branchNode->isTheVirtualGuardForAGuardedInlinedCall() &&
       fallThroughBlock->isGotoBlock(comp(),true) &&
       (fallThroughBlock->getPredecessors().size() == 1) &&
       fallThroughBlock->getExit()->getNextTreeTop())
      {
      TR::Block *blockAfterFallThrough = fallThroughBlock->getExit()->getNextTreeTop()->getNode()->getBlock();


      TR::CFGEdgeList &fallThroughSuccessors = fallThroughBlock->getSuccessors();
#if defined(ENABLE_SPMD_SIMD)
      static const char *e = feGetEnv("TR_DisableSPMD");
      static int disableSPMD = e ? atoi(e) : false;
      bool doNotRemove = false;
      if (!disableSPMD)
         {
         TR_BlockStructure *blockStructure = blockAfterFallThrough->getStructureOf();
         if (blockStructure)
            {
            if (blockStructure->isLoopInvariantBlock())
               {
               if (trace()) traceMsg(comp(), "\t\tavoid redirecting a jump to a pre-header block %d for later SPMD optimization\n", blockAfterFallThrough->getNumber());
               doNotRemove = true;
               }
            }
         }
      if (!doNotRemove)
#endif
      //fall through block contain only a jump/goto
      if (takenBlock == blockAfterFallThrough && (fallThroughBlock->getLastRealTreeTop() == fallThroughBlock->getFirstRealTreeTop()))
         {
         TR::Node    *gotoNode      = fallThroughBlock->getLastRealTreeTop()->getNode();
         TR::TreeTop *gotoDest      = gotoNode->getBranchDestination();
         TR::Block   *gotoDestBlock = gotoDest->getNode()->getBlock();
         // Next block jump away
         if (fallThroughSuccessors.front()->getTo()->asBlock() != blockAfterFallThrough
            )
            {

            if (performTransformation(comp(), "%s in block_%d, branch taken dest (%d) is a block after a single goto, so redirecting to its goto's destination (%d)\n", title, block->getNumber(), takenBlock->getNumber(), gotoDestBlock->getNumber()))
               {
               TR_ASSERT(branchNode->getOpCode().isBranch(), "expected branch at end of prevBlock to reverse, but found something else");
               branchNode->reverseBranch(gotoDest);
               if (!block->hasSuccessor(gotoDestBlock))
                  {
                  cfg->addEdge(block, gotoDestBlock);
                  block->getSuccessors().front()->setFrequency(std::min(block->getEdge(fallThroughBlock)->getFrequency() , fallThroughBlock->getEdge(gotoDestBlock)->getFrequency()));
                  }
               cfg->removeEdge(fallThroughBlock, gotoDestBlock);
               cfg->removeEdge(block, fallThroughBlock);
               removeEmptyBlock(cfg, fallThroughBlock, title);
               //fallThroughBlock->getLastRealTreeTop()->unlink(false);

               }
            }
         }
      }
#endif
   }

// apply peephole optimizations to blocks that end in a branch
// no looping here because there are only two possible optimizations done but we know which order to try them in
// assume block->endsInBranch()
void TR_OrderBlocks::peepHoleBranchBlock(TR::CFG *cfg, TR::Block *block, char *title)
   {
   TR_ASSERT(block->endsInBranch(), "peepHoleBranchBlock called on block that doesn't end in a branch!");
   TR::Node *branchNode = block->getLastRealTreeTop()->getNode();
   TR::TreeTop *takenEntry = branchNode->getBranchDestination();
   TR::Block *takenBlock = takenEntry->getNode()->getBlock();

   TR::TreeTop *fallThroughEntry = block->getExit()->getNextTreeTop();
   TR::Block *fallThroughBlock = fallThroughEntry->getNode()->getBlock();

   // if taken block is still a goto block, then redirect our taken branch direction
   while (takenBlock->isGotoBlock(comp(),true))
      {
      TR::Node *takenGotoNode = takenBlock->getLastRealTreeTop()->getNode();
      TR::TreeTop *takenGotoDest = takenGotoNode->getBranchDestination();
      TR::Block *takenGotoDestBlock = takenGotoDest->getNode()->getBlock();

      // exclude goto block that points to itself
      if (takenBlock == takenGotoDestBlock) break;


      if ((!takenBlock->getStructureOf() || !takenBlock->getStructureOf()->isLoopInvariantBlock()) &&
          performTransformation(comp(), "%s in block_%d, branch taken dest (%d) is a goto block, so redirecting to its destination (%d)\n", title, block->getNumber(), takenBlock->getNumber(), takenGotoDestBlock->getNumber()))
         {
         branchNode->setBranchDestination(takenGotoDest);
         bool isDoubleton = (block->getSuccessors().size() == 2);

         if (isDoubleton && !block->hasSuccessor(takenGotoDestBlock))
            {
            TR::CFGEdge *oldEdge = block->getEdge(takenBlock);
            int32_t oldEdgeFreq = oldEdge->getFrequency();
            TR::CFGEdge *newEdge = cfg->addEdge(block, takenGotoDestBlock);
            cfg->removeEdge(block, takenBlock);
            newEdge->setFrequency(oldEdgeFreq);

            if (trace())
               {
               traceMsg(comp(), "\t\t\tcreating new edge (b_%d -> b_%d) freq: %d\n", block->getNumber(), takenGotoDestBlock->getNumber(),
                     newEdge->getFrequency());
               traceMsg(comp(), "\t\t\tinstead of edge (b_%d -> b_%d) freq: %d \n", block->getNumber(), takenBlock->getNumber(),
                     newEdge->getFrequency());
               }

            // transfer frequency between blocks
            cfg->updateBlockFrequency(takenBlock, takenBlock->getFrequency() - oldEdgeFreq);
            cfg->updateBlockFrequencyFromEdges(takenBlock);
            cfg->updateBlockFrequency(takenGotoDestBlock, takenGotoDestBlock->getFrequency() + oldEdgeFreq);
            if (trace())
               {
               traceMsg(comp(), "\t\t\ttakenBlock (b_%d) new Frequency: %d\n", takenBlock->getNumber(), takenBlock->getFrequency());
               traceMsg(comp(), "\t\t\ttakenGotoDestBlock (b_%d) new Frequency: %d\n", takenGotoDestBlock->getNumber(), takenGotoDestBlock->getFrequency());
               }
            }
         else
            {
            if (!block->hasSuccessor(takenGotoDestBlock))
               cfg->addEdge(block, takenGotoDestBlock);
            if (isDoubleton)
               cfg->removeEdge(block, takenBlock);
            }
         }
      else
         break;

      takenEntry = branchNode->getBranchDestination();
      takenBlock = takenEntry->getNode()->getBlock();
      }

   peepHoleBranchToLoopHeader(cfg, block, fallThroughBlock, takenBlock, title);
   }

// look for a branch with taken block same as fall-through block
bool TR_OrderBlocks::peepHoleBranchToFollowing(TR::CFG *cfg, TR::Block *block, TR::Block *followingBlock, char *title)
   {
   TR_ASSERT(block->endsInBranch(), "peepHoleBranchToFollowing called on block that doesn't end in a branch!");
   TR::Node *branchNode = block->getLastRealTreeTop()->getNode();
   TR::TreeTop *takenEntry = branchNode->getBranchDestination();
   TR::Block *takenBlock = takenEntry->getNode()->getBlock();

   // this pattern is not one that can be legally left in the CFG, so no performTransformation here
   if (takenBlock == followingBlock)
      {
      if (trace())
         dumpOptDetails(comp(), "block_%d ends in redundant branch to %d\n", block->getNumber(), takenBlock->getNumber());
      removeRedundantBranch(cfg, block, branchNode, takenBlock);
      return true;
      }

   return false;
   }

void TR_OrderBlocks::removeEmptyBlock(TR::CFG *cfg, TR::Block *block, char *title)
   {
   TR_ASSERT(block->getExceptionPredecessors().empty(), "removeEmpty block doesn't deal with empty catch blocks properly");

   TR::LabelSymbol * entryLabel = NULL;

   if (performTransformation(comp(), "%s empty block_%d, redirecting edges around this block then removing it\n", title, block->getNumber()))
      {
      bool isLoopEntryBlock = block->getStructureOf() &&
                              block->getStructureOf()->getContainingLoop() &&
                              block->getStructureOf()->getContainingLoop()->getEntryBlock() == block;
      if (!block->getSuccessors().empty())
         {
         // all predecessors must be redirected to empty block's fall-through block
         TR::Block *fallThroughBlock = block->getExit()->getNextTreeTop()->getNode()->getBlock();
    	 if (trace()) traceMsg(comp(), "\t\t\tredirecting edges to block's fall-through successor %d\n", fallThroughBlock->getNumber());

    	 if (!block->isExtensionOfPreviousBlock() &&
    	     fallThroughBlock->isExtensionOfPreviousBlock())
    	    fallThroughBlock->setIsExtensionOfPreviousBlock(false);

         for (auto predEdge = block->getPredecessors().begin(); predEdge != block->getPredecessors().end();)
            {
            TR::CFGEdge* current = *(predEdge++);
            TR::Block *fromBlock = current->getFrom()->asBlock();
            if (trace()) traceMsg(comp(), "\t\t\tredirecting edge (%d,%d) to new dest %d\n", fromBlock->getNumber(), current->getTo()->getNumber(), fallThroughBlock->getNumber());
            fromBlock->redirectFlowToNewDestination(comp(), current, fallThroughBlock, false);
            }
         // If we removed the loop entry block then invalidate structure to avoid false improper regions
         if (isLoopEntryBlock)
            invalidateStructure();
         }
      else if (!block->getPredecessors().empty())
         {
         if (trace()) traceMsg(comp(), "\t\tblock has no successors so no edges to redirect, just removing predecessor edges\n");
         for (auto predEdge = block->getPredecessors().begin(); predEdge != block->getPredecessors().end();)
            {
            TR::Block *fromBlock = (*predEdge)->getFrom()->asBlock();
            if (trace()) traceMsg(comp(), "\t\t\tremoving edge (%d,%d)\n", fromBlock->getNumber(), (*predEdge)->getTo()->getNumber());
            cfg->removeEdge(*(predEdge++));
            }
         // If we removed the loop entry block then invalidate structure to avoid false improper regions
         if (isLoopEntryBlock)
            invalidateStructure();
         // the last removeEdge will remove block so nothing left to do at this point
         return;
         }
      else
         if (trace()) traceMsg(comp(), "\t\tblock has no successors edges to redirect, no predecessor edges to remove\n");

      TR_ASSERT(block->getPredecessors().empty(), "how can there be any more predecessors?");

      // clip block out of trees
      TR::TreeTop *prevTT=block->getEntry()->getPrevTreeTop();
      if (prevTT != NULL)
         prevTT->join(block->getExit()->getNextTreeTop());
      else
         comp()->getMethodSymbol()->setFirstTreeTop(block->getExit()->getNextTreeTop());

      // finally, remove block from the CFG
      cfg->removeNode(block);
      }
   }


// apply a small set of transformations to try to clean up blocks before ordering
//  repeatedly apply these transformations until none apply
//  returns TRUE if block still exists after peepholing
bool TR_OrderBlocks::doPeepHoleBlockCorrections(TR::Block *block, char *title)
   {
   TR::CFG *cfg             = comp()->getFlowGraph();

   // pattern: block has nothing in it and no exceptional predecessors (can redirect edges around it and remove it)
   if ((block->isEmptyBlock() && (block->hasExceptionPredecessors() == false) && comp()->getProfilingMode() != JitProfiling
       && !(block->getStructureOf() && block->getStructureOf()->isLoopInvariantBlock()))
       && (block->isTargetOfJumpWhoseTargetCanBeChanged(comp())))
       //TODO enable for PLX, currently disabled because we cannot re-direct edges for ASM flows
      {
      removeEmptyBlock(cfg, block, title);
      return false;
      }

   // pattern: dead block because it has no predecessors
   if (block->getPredecessors().empty() && (block->hasExceptionPredecessors() == false) && block->getEntry() != NULL)
      {
      if (performTransformation(comp(), "%s block_%d has no predecessors so removing it and its out edges from the flow graph\n", title, block->getNumber()))
         {
         // disconnect any out edges
         TR_SuccessorIterator outEdges(block);
         for (auto succeedingEdge = outEdges.getFirst(); succeedingEdge; succeedingEdge = outEdges.getNext())
            cfg->removeEdge(succeedingEdge->getFrom(), succeedingEdge->getTo());

         removeEmptyBlock(cfg, block, title);
         return false;
         }
      }

   // look repeatedly for opportunities to optimize the previous block
   bool madeAChange = true;
   while (madeAChange)
      {
      TR::TreeTop *prevTT = block->getEntry()->getPrevTreeTop();
      TR::Block *prevBlock = (prevTT != NULL) ? prevTT->getNode()->getBlock() : NULL;

      madeAChange = false;

      if (prevBlock != NULL)
         {
         if (prevBlock->endsInGoto() && peepHoleGotoToFollowing(cfg, prevBlock, block, title))
            madeAChange = true;

         else if (prevBlock->endsInBranch() && peepHoleBranchToFollowing(cfg, prevBlock, block, title))
            madeAChange = true;

         // if we made either of the above changes, then prevBlock might be empty now (remove it if it is)
         if (madeAChange && prevBlock->isEmptyBlock() && (prevBlock->hasExceptionPredecessors() == false) && comp()->getProfilingMode() != JitProfiling &&
             prevBlock->isTargetOfJumpWhoseTargetCanBeChanged(comp()))
            {
            removeEmptyBlock(cfg, prevBlock, title);
            }
         }
      }

   // if block ends in goto or branch, look for opportunities to optimize the destination
   if (block->endsInGoto())
      peepHoleGotoBlock(cfg, block, title);
   else if (block->endsInBranch())
      {
      peepHoleBranchBlock(cfg, block, title);
      peepHoleBranchAroundSingleGoto(cfg, block, title);
      }

   // block still exists in cfg if we got here
   return true;
   }

// go through the blocks looking for peephole optimization opportunities
// return TRUE if blocks were removed during the analysis
bool TR_OrderBlocks::lookForPeepHoleOpportunities(char *title)
   {
   static bool doPeepHoling=(feGetEnv("TR_noBlockOrderPeepholing") == NULL);
   if (!doPeepHoling)
      return false;

#if 0
   if (comp()->getOptIndex() < comp()->getOptions()->getFirstOptIndex() ||
       comp()->getOptIndex() > comp()->getOptions()->getLastOptIndex())
      return false;
   comp()->incOptIndex();
#endif

   TR::CFG *cfg = comp()->getFlowGraph();

   bool blocksWereRemoved=false;
   TR::TreeTop *tt = comp()->getStartTree();
   TR_ASSERT(tt->getNode()->getOpCodeValue() == TR::BBStart, "first tree isn't BBStart");
   TR::Block *lastBlock = NULL;

   if (trace()) traceMsg(comp(), "Looking for peephole opportunities:\n");
   while (tt != NULL)
      {
      TR_ASSERT(tt->getNode()->getOpCodeValue() == TR::BBStart, "tree walk reached something that isn't BBStart");

      TR::Block *block=tt->getNode()->getBlock();
      TR::TreeTop *nextBlockTT = block->getExit()->getNextTreeTop();
      if (trace()) traceMsg(comp(), "\tBlock %d:\n", block->getNumber());

      bool blockStillExists = doPeepHoleBlockCorrections(block,title);
      tt = nextBlockTT;
      if (!blockStillExists)
         blocksWereRemoved = true;
      }



   return blocksWereRemoved;
   }

void TR_OrderBlocks::initialize()
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   TR_Structure *rootStructure = cfg->getStructure();

   // set up block nesting depths
   if (trace()) traceMsg(comp(), "Checking block frequencies, computing nesting depths:\n");
   TR::CFGNode *node;
   for (node = cfg->getFirstNode(); node; node = node->getNext())
      {
      if (trace()) traceMsg(comp(), "\tBlock %d:\n", node->getNumber());
      if (node->getFrequency() <= 0)
         {
         if (trace()) traceMsg(comp(), "\t\tForcing original frequency %d to 0\n", node->getFrequency());
         node->setFrequency(0);
         }

      if (rootStructure)
         {
         int32_t nestingDepth = 0;
         TR::Block *block = toBlock(node);
         if (block->getStructureOf() != NULL)
            {
            block->getStructureOf()->setNestingDepths(&nestingDepth);
            if (trace()) traceMsg(comp(), "\t\tLoop nesting depth set to %d\n", block->getNestingDepth());
            }
         }
      }
   }

//Fill _coldPathList block list with all of the unscheduled blocks
void TR_OrderBlocks::insertBlocksToList()
   {
   TR::CFGNode *block =  comp()->getStartBlock();
   _numUnschedHotBlocks = 0;
   while (block != NULL)
      {
      if (_visitCount != block->getVisitCount())
         {
         //skip already scheduled blocks
         if (block->getFrequency() > 0)
            _numUnschedHotBlocks++;

         addToOrderedBlockList(block, _coldPathList, false);
         }
      if (block->asBlock()->getExit() == NULL ||  block->asBlock()->getExit()->getNextTreeTop() == NULL)
         block = NULL;
      else
         block = block->asBlock()->getExit()->getNextTreeTop()->getNode()->getBlock();
      }
   TR::CFGNode *startBlock = comp()->getFlowGraph()->getStart();
   TR::CFGNode *endBlock = comp()->getFlowGraph()->getEnd();
   if (startBlock->getFrequency() > 0 && startBlock->getVisitCount() != _visitCount)
      _numUnschedHotBlocks++;

   if (endBlock->getFrequency() > 0 && endBlock->getVisitCount() != _visitCount)
      _numUnschedHotBlocks++;

   if (trace()) traceMsg(comp(), "\t_numUnschedHotBlocks %s %d\n", comp()->signature(), _numUnschedHotBlocks);
   }


void TR_OrderBlocks::generateNewOrder(TR_BlockList & newBlockOrder)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   ListElement<TR::CFGNode> *lastElementInOrder = NULL;

   //TR::CFGNode *block = cfg->getStart();
   TR::CFGNode *block =  comp()->getStartBlock();

   int32_t numSuperColdSteps = 0;
   int32_t currentSuperColdStep = 0;

   // put everything in the list upfront to be safe
   if (_superColdBlockOnly)
     {
//     lastElementInOrder = newBlockOrder.addAfter(cfg->getStart(), lastElementInOrder);
     while (block != NULL)
        {
        //traceMsg(comp(), "Checking Block %d [%p] getEntry()=%p methodSymbol->getFirstTreeTop()=%p\n", block->getNumber(), block,block->asBlock()->getEntry(),optimizer()->getMethodSymbol()->getFirstTreeTop());

        if (block->asBlock()->isSuperCold())
           {
           addToOrderedBlockList(block, _coldPathList, false);
           }
        else
           addToOrderedBlockList(block, _hotPathList, false);
           //lastElementInOrder = newBlockOrder.addAfter(block, lastElementInOrder);
        if (block->asBlock()->getExit() == NULL ||  block->asBlock()->getExit()->getNextTreeTop() == NULL)
           block = NULL;
        else
            block = block->asBlock()->getExit()->getNextTreeTop()->getNode()->getBlock();
        }
     }

   block = cfg->getStart();
   while (block != NULL || !(_hotPathList.isEmpty() && _coldPathList.isEmpty() ))
      {
      if (trace())
         {
         if (block)
            {
            traceMsg(comp(), "Block %d [%p]\n", block->getNumber(), block);
            int32_t numPreds = block->getPredecessors().size();
            const char *predString = (numPreds == 0) ? "" : " (join)";
            traceMsg(comp(), "\t%d predecessors%s\n", numPreds, predString);
            int32_t numSuccs = block->getSuccessors().size();
            const char *succString = (numSuccs == 0) ? "" : " (split)";
            traceMsg(comp(), "\t%d successors%s\n",   numSuccs, succString);
            }
         else
            traceMsg(comp(), "Block NULL\n");
         traceMsg(comp(), "Forest of hot paths: ");
         TR_BlockListIterator hotList(&_hotPathList);
         for (TR::CFGNode *hotBlock=hotList.getFirst(); hotBlock != NULL; hotBlock = hotList.getNext())
            traceMsg(comp(), "%d ", hotBlock->getNumber());
         traceMsg(comp(), "\n\tForest of cold paths: ");
         TR_BlockListIterator coldList(&_coldPathList);
         for (TR::CFGNode *coldBlock=coldList.getFirst(); coldBlock != NULL; coldBlock = coldList.getNext())
            traceMsg(comp(), "%d ", coldBlock->getNumber());
         traceMsg(comp(), "\n");
         }

      // if block is NULL, then we need to pick a new path to walk from the forest
      // else if block is non-NULL and its best successor is in a different structure, then we should see if there's a better path
      if (block == NULL)
         {
         TR::CFGNode *lastBlock = NULL;
         if (lastElementInOrder != NULL)
            lastBlock = lastElementInOrder->getData();

         block = findBestPath(lastBlock);

         continue;
         }

      block->setVisitCount(_visitCount);
      if (trace())
         {
         traceMsg(comp(), "\t\tset visit count for block_%d to %d\n", block->getNumber(), _visitCount);

         if (lastElementInOrder)
            traceMsg(comp(), "\tadding %d to order after %d\n", block->getNumber(), lastElementInOrder->getData()->getNumber());
         else
            traceMsg(comp(), "\tadding %d to order\n", block->getNumber());
         }

      lastElementInOrder = newBlockOrder.addAfter(block, lastElementInOrder);

      int32_t     numCandidates = 0;
      TR::CFGNode *bestSucc      = NULL;
      TR::Block   *nextBlock     = NULL;

      if (block->asBlock()->getExit() && block->asBlock()->getExit()->getNextTreeTop())
         {
         nextBlock = block->asBlock()->getExit()->getNextTreeTop()->getNode()->getBlock();
         if (trace()) traceMsg(comp(), "Lexical order block_%d visitCount=%d _visitCount=%d\n",nextBlock->getNumber(), nextBlock->getVisitCount(),  _visitCount);
         if (nextBlock->getVisitCount() != _visitCount)
            {
            bestSucc = nextBlock;
            if (trace()) traceMsg(comp(), "Choosing to default lexical order block\n",bestSucc->asBlock()->getNumber());
            }
         }
      else if (block->asBlock() == cfg->getStart()->asBlock())
         bestSucc = comp()->getStartBlock();

      if(needBetterChoice(cfg, block, bestSucc) &&
         performTransformation(comp(), "%s choose best successor for block_%d (freq:%d)\n", OPT_DETAILS, block->getNumber(),
               block->getFrequency()))
         bestSucc = chooseBestFallThroughSuccessor(cfg, block, numCandidates);

      //TR::CFGNode *bestSucc = chooseBestFallThroughSuccessor(cfg, block, numCandidates);

      addRemainingSuccessorsToList(block, bestSucc);

      // if bestSucc is in a different structure, then just put bestSucc into the right forest of paths
      // and set block to NULL so that we'll choose the best (possibly higher frequency) path to follow
      if (bestSucc != NULL && (numCandidates != 1) && endPathAtBlock(block, bestSucc, cfg) &&
          performTransformation(comp(), "%s Reordering blocks to optimize fall-through paths\n", OPT_DETAILS))
         {
         if (!_superColdBlockOnly)
            {
            if (trace()) traceMsg(comp(), "Choosing to end path here, block_% will be added into list\n",bestSucc->asBlock()->getNumber());
            if (bestSucc->asBlock()->isCold())
               addToOrderedBlockList(bestSucc, _coldPathList, true);
            else
               addToOrderedBlockList(bestSucc, _hotPathList, true);
            }
         else
            {
            if (trace()) traceMsg(comp(), "Choosing to end path here, no need to add block_% into list\n",bestSucc->asBlock()->getNumber());
            }
         block = NULL;
         }
      else
         // bestSucc is still the best path to follow
         block = bestSucc;

      if (_changeBlockOrderBasedOnHWProfile && block == NULL)
         {
         TR::CFGNode *lastBlock = NULL;
         if (lastElementInOrder != NULL)
            lastBlock = lastElementInOrder->getData();

         block = findBestPath(lastBlock);
         }

      }

   TR_ASSERT(_hotPathList.isEmpty(), "Error: blocks left on hot path list");
   TR_ASSERT(_coldPathList.isEmpty(), "Error: blocks left on cold path list");
   }


// prevBlock's fall-through successor used to be "origSucc" but now it is some other block
// so: insert a block following prevBLock that contains a goto node to "origSucc"
TR::Block *TR_BlockOrderingOptimization::insertGotoFallThroughBlock(TR::TreeTop *fallThroughTT, TR::Node *node,
                                                     TR::CFGNode *prevBlock, TR::CFGNode *origSucc, TR_RegionStructure *parent)
   {
   // insert a goto so that control gets to prevBlock's original fall-through successor
   TR::CFG *cfg=comp()->getFlowGraph();
   int32_t frequency = prevBlock->getFrequency();
   if (origSucc->getFrequency() < frequency)
      frequency = origSucc->getFrequency();

   TR::Block *gotoBlock = TR::Block::createEmptyBlock(fallThroughTT->getNode(), comp(), frequency, prevBlock->asBlock());
   TR::TreeTop::create(comp(), gotoBlock->getEntry(), TR::Node::create(node, TR::Goto, 0, fallThroughTT));

   if (!parent)
      parent = prevBlock->asBlock()->getCommonParentStructureIfExists(origSucc->asBlock(), comp()->getFlowGraph());

   cfg->addNode(gotoBlock, parent);

   cfg->addEdge(gotoBlock, origSucc);
   cfg->addEdge(prevBlock, gotoBlock);
   cfg->removeEdge(prevBlock, origSucc);
   //cfg->copyExceptionSuccessors(prevBlock, gotoBlock);
   gotoBlock->asBlock()->inheritBlockInfo(prevBlock->asBlock(), prevBlock->asBlock()->isCold());

   if (trace()) traceMsg(comp(), "\tadded extra goto block_%d\n", gotoBlock->getNumber());

   return gotoBlock;
   }


// connect all the trees together according to the order of blocks in newBlockOrder
void TR_BlockOrderingOptimization::connectTreesAccordingToOrder(TR_BlockList & newBlockOrder)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();

   // now put the trees in the order dictated by newBlockOrder (they are stored on the list IN ORDER)
   // right now, this code just assumes that it can safely rewrite the trees...should probably put some assumes in here
   TR::Block *prevBlock = newBlockOrder.popHead()->asBlock();
   TR_ASSERT(prevBlock->getNumber() == 0, "first block must be 0!!");

   if (trace()) traceMsg(comp(), "%d\n", prevBlock->getNumber());

   prevBlock = newBlockOrder.popHead()->asBlock();
   if (trace()) traceMsg(comp(), "block =%d prevBlock->getEntry()->getNode()=%p methodSymbol->getFirstTreeTop()->getNode()=%p\n", prevBlock->getNumber(),prevBlock->getEntry()->getNode(), methodSymbol->getFirstTreeTop()->getNode());

   TR_ASSERT(prevBlock->getEntry() == methodSymbol->getFirstTreeTop(), "first tree shouldn't change!");

   TR::TreeTop *currentLastTreeTop = prevBlock->getExit();
   if (trace()) traceMsg(comp(), "%d\n", prevBlock->getNumber());
   unsigned numberOfBlocks=1;

   while (!newBlockOrder.isEmpty())
      {
      TR::Block *block = newBlockOrder.popHead()->asBlock();
      if (trace()) traceMsg(comp(), "newBlockOrder Head: %d \n", block->getNumber());
      TR::Block *fallThroughBlock = NULL;
      TR::TreeTop *fallThroughTT = currentLastTreeTop->getNextRealTreeTop();

      numberOfBlocks++;

      if (fallThroughTT != block->getEntry())
         {
         numberOfReorderings++;

         // if prevBlock is exit, or prevBlock was originally the last block, then there's no special work to do
         // also, if there is no flow edge from prevBlock to the original fall through block, nothing to worry about
         if (prevBlock->getEntry() != NULL && fallThroughTT != NULL && prevBlock->hasSuccessor(fallThroughTT->getNode()->getBlock()))
            {
            TR::CFGNode *origSucc = fallThroughTT->getNode()->getBlock();

            if (trace()) traceMsg(comp(), "\t%d did not originally follow %d in trees, might need to move trees\n", block->getNumber(), prevBlock->getNumber());
            // 5 possibilities, the first two of which require some fix-up:
            //     1) block is not a successor of prevBlock, but prevBlock's fall-through successor has already been placed elsewhere
            //     2) block is a different successor block than the original fall-through
            //        similar to 5, but we can reverse the branch in this case
            //     3) block is the only successor of prevBlock, but prevBlock ends in a goto (straightening should clean-up later)
            //     4) block is not a successor of prevBlock, but it begins the path we (arbitrarily?) chose to follow prevBlock
            //     5) block is a successor of prevBlock, but it's not the original fall through (due to endPathAtBlock)
            //        and we can't reverse branch (TR::branch and isJumpWithMulitpleTargets), we need a go to for this case
            TR::Node *branchNode = prevBlock->getLastRealTreeTop()->getNode();
            if(branchNode->getOpCodeValue() == TR::treetop)
                branchNode = branchNode->getFirstChild();

            if (branchNode->getOpCodeValue() != TR::Goto  &&
                (!branchNode->getOpCode().isJumpWithMultipleTargets() || // don't worry about igotos
                 (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().isCall())))
               {
               bool          isSucc       = prevBlock->hasSuccessor(block);
               TR::ILOpCodes  branchOpCode = branchNode->getOpCodeValue();
               if (!isSucc
                   || branchNode->isTheVirtualGuardForAGuardedInlinedCall()
                   || (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().isCall()))
                  {
                  if (trace())
                     traceMsg(comp(), "\tneed to add extra goto block so that %d will fall-through to %d\n", prevBlock->getNumber(), origSucc->getNumber());
                  TR::Block *gotoBlock = insertGotoFallThroughBlock(fallThroughTT, branchNode, prevBlock, origSucc);
                  currentLastTreeTop->join(gotoBlock->getEntry());
                  currentLastTreeTop = gotoBlock->getExit();
                  prevBlock = gotoBlock;
                  }
               else
                  {
                  // reverse the branch
                  if (trace()) traceMsg(comp(), "\tdecided to reverse the branch at the end of %d\n", prevBlock->getNumber());
                  TR::Node *branchNode = prevBlock->getLastRealTreeTop()->getNode();
                  TR_ASSERT(branchNode->getOpCode().isBranch(), "expected branch at end of prevBlock to reverse, but found something else");
                  branchNode->reverseBranch(fallThroughTT);
                  }

               }
            }
         else
            {
            if (trace()) traceMsg(comp(), "\t%d did not originally follow %d in trees, but no trees manipulation needed\n", block->getNumber(), prevBlock->getNumber());
            }

         // connect the successor's trees to the last tree
         if (block->getEntry())
            {
            if (trace()) traceMsg(comp(), "\tconnecting the trees\n");
            currentLastTreeTop->setNextTreeTop(block->getEntry());
            block->getEntry()->setPrevTreeTop(currentLastTreeTop);
            }
         else
            {
            if (trace()) traceMsg(comp(), "\tlooks like exit block, no trees to connect\n");
            }
         }

      if (block->getExit())
         currentLastTreeTop = block->getExit();

      prevBlock = block;
      }

   // check last block to see if it needs a goto
   TR::TreeTop *fallThroughTT = currentLastTreeTop->getNextRealTreeTop();
   if (fallThroughTT != NULL)
      {
      TR::CFGNode *origSucc = fallThroughTT->getNode()->getBlock();
      TR::Node *maybeBranchNode=NULL;
      if (prevBlock->getEntry() != NULL && prevBlock->getLastRealTreeTop() != NULL)
         maybeBranchNode = prevBlock->getLastRealTreeTop()->getNode();

      if (prevBlock->hasSuccessor(origSucc) && !prevBlock->endsInGoto() && (maybeBranchNode == NULL || !(maybeBranchNode->getOpCode().isJumpWithMultipleTargets())
    		  || (maybeBranchNode->getOpCode().isJumpWithMultipleTargets() && maybeBranchNode->getOpCode().isCall())))
		{
         if (trace()) traceMsg(comp(), "\tneed to add extra goto block so that %d will fall-through to %d\n", prevBlock->getNumber(), origSucc->getNumber());
         TR::Node *branchNode = prevBlock->getLastRealTreeTop()->getNode();
         TR::Block *gotoBlock = insertGotoFallThroughBlock(fallThroughTT, branchNode, prevBlock, origSucc);
         currentLastTreeTop->join(gotoBlock->getEntry());
         currentLastTreeTop = gotoBlock->getExit();
         }
      }

   currentLastTreeTop->setNextTreeTop(NULL);

   static char *reorderingStats = feGetEnv("TR_reorderingStats");
   if (reorderingStats)
      fprintf(stderr, "%d replication candidates in method %s (has %d blocks)\n", numberMethodReplicationCandidates, comp()->signature(), numberOfBlocks);
   numberReplicationCandidates += numberMethodReplicationCandidates;

   }


bool TR_OrderBlocks::doBlockExtension()
   {
   bool blocksWereExtended=false;
   TR::TreeTop *tt = comp()->getStartTree();
   TR_ASSERT(tt->getNode()->getOpCodeValue() == TR::BBStart, "first tree isn't BBStart");

#if 0
   if (comp()->getOptIndex() < comp()->getOptions()->getFirstOptIndex() ||
       comp()->getOptIndex() > comp()->getOptions()->getLastOptIndex())
      return false;
   comp()->incOptIndex();
#endif

   TR::Block *block = tt->getNode()->getBlock();
   if (trace())
      {
      traceMsg(comp(), "Extending blocks:\n");
      traceMsg(comp(), "\tBlock %d:\n", block->getNumber());
      }

   TR::Block *prevBlock = block;
   tt = block->getExit()->getNextTreeTop();
   while (tt != NULL)
      {
      TR_ASSERT(tt->getNode()->getOpCodeValue() == TR::BBStart, "tree walk reached something that isn't BBStart");

      block=tt->getNode()->getBlock();

      if (trace()) traceMsg(comp(), "\tBlock %d:", block->getNumber());
      //IvanB: see if this condition can be relaxed to not exclude jumps with multiple targets
      if ((block->getPredecessors().size() == 1) &&
          prevBlock->hasSuccessor(block) &&
          prevBlock->canFallThroughToNextBlock() &&
          !prevBlock->getLastRealTreeTop()->getNode()->getOpCode().isReturn() &&
          !prevBlock->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets() &&
          !(prevBlock->getLastRealTreeTop()->getNode()->getOpCodeValue()==TR::treetop &&
            prevBlock->getLastRealTreeTop()->getNode()->getFirstChild()->getOpCode().isJumpWithMultipleTargets()) &&
          !block->isOSRCodeBlock())
         {
         if (performTransformation(comp(), "%s block_%d is extension of previous block\n", OPT_DETAILS, block->getNumber()))
            {
            blocksWereExtended = true;
            block->setIsExtensionOfPreviousBlock();
            }
         }
      else
         {
         if (trace()) traceMsg(comp(), "cannot extend previous block\n");
         }

      prevBlock = block;
      tt = block->getExit()->getNextTreeTop();
      }

   return blocksWereExtended;
   }


void TR_OrderBlocks::doReordering()
   {
   //if (!performTransformation(comp(), "%s ORDER BLOCK: Reordering blocks to optimize fall-through paths\n", OPT_DETAILS))
   //   return;

#if 0
   if (trace())
      traceMsg(comp(), "Entered TR_OrderBlocks::optIndex=%d firstOptIndex=%d lastOptIndex=%d ",comp()->getOptIndex(), comp()->getOptions()->getFirstOptIndex(),comp()->getOptions()->getLastOptIndex());
   if (comp()->getOptIndex() < comp()->getOptions()->getFirstOptIndex() ||
       comp()->getOptIndex() > comp()->getOptions()->getLastOptIndex())
      return ;
   comp()->incOptIndex();
#endif

   _visitCount = comp()->incVisitCount();

   TR_BlockList newBlockOrder(trMemory());
   generateNewOrder(newBlockOrder);

   //if (performTransformation(comp(), "%s Reordering blocks to optimize fall-through paths\n", OPT_DETAILS))
      connectTreesAccordingToOrder(newBlockOrder);

   if (trace())
      {
      traceMsg(comp(), "After reorder block ");
      TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
      dumpBlockOrdering(methodSymbol->getFirstTreeTop());
      }

   if(needInvalidateStructure())
      {
      if (trace()) traceMsg(comp(), "Invalidate structure ");
      comp()->getFlowGraph()->setStructure(0);
      }

   // do another round of peepholing, because there may be more opportunities now
   if (_doPeepHoleOptimizationsAfter)
      {
      if (trace()) comp()->dumpMethodTrees("Before final peepholing");
      lookForPeepHoleOpportunities(OPT_DETAILS);
      }
   }


bool TR_OrderBlocks::shouldPerform()
   {
   if (comp()->getOption(TR_DisableNewBlockOrdering))
      return false;


   return true;
   }


int32_t TR_OrderBlocks::perform()
   {
   numberOfCompiles++;
   numberMethodReplicationCandidates = 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace()) comp()->dumpMethodTrees("Before ordering");

   initialize();

   if (_doPeepHoleOptimizationsBefore)
      {
      lookForPeepHoleOpportunities(OPT_DETAILS);
      if (trace()) comp()->dumpMethodTrees("After early peepholing");
      //comp()->getDebug()->verifyCFG(optimizer()->getMethodSymbol());

      }

   if (_reorderBlocks && performTransformation(comp(), "%s Propagating coldness information\n", OPT_DETAILS))
      comp()->getFlowGraph()->propagateColdInfo(false);

   //comp()->getFlowGraph()->setFrequencies();

   if (trace())
      {
      traceMsg(comp(), "Original ");
      TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
      dumpBlockOrdering(methodSymbol->getFirstTreeTop());
      }

   if (_reorderBlocks)
      doReordering();

   //comp()->getFlowGraph()->setFrequencies();
   //comp()->getDebug()->verifyCFG(getOptimizer()->getMethodSymbol());

   // block extension must be the last thing we do...particularly after peephole opts because they can change a block's predecessors
   if (_extendBlocks)
      {
      if (trace()) comp()->dumpMethodTrees("Before extending blocks");
      bool blocksWereExtended = doBlockExtension();
      if (trace()) comp()->dumpMethodTrees("After extending blocks");
      //comp()->getDebug()->verifyCFG(getOptimizer()->getMethodSymbol());
      if (blocksWereExtended)
         optimizer()->enableAllLocalOpts();
      }
      if (trace()) comp()->dumpMethodTrees("After enableAllLocalOpts");
      //comp()->getDebug()->verifyCFG(getOptimizer()->getMethodSymbol());

   if (trace())
      {
      traceMsg(comp(), "Final ");
      TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
      dumpBlockOrdering(methodSymbol->getFirstTreeTop());
      }
   if (_donePeepholeGotoToLoopHeader)
     comp()->getFlowGraph()->setStructure(0);

   static char *noCheckOrdering = feGetEnv("TR_noOrderingCheck");
   if (noCheckOrdering == NULL)
      {
      checkOrderingConsistency(comp());
      }

   return 1; // actual cost
   }

const char *
TR_OrderBlocks::optDetailString() const throw()
   {
   return "O^O ORDER BLOCKS: ";
   }

void checkOrderingConsistency(TR::Compilation *comp)
   {
   static char *debugConsistencyCheck = feGetEnv("TR_debugBlockOrderingConsistencyCheck");

   TR::CFG *cfg=comp->getFlowGraph();
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

   vcount_t visitCount = comp->incVisitCount();

   TR::Block * prevBlock = comp->getStartTree()->getNode()->getBlock();
   TR::Block * block = prevBlock->getNextBlock();
   bool seenColdBlock = prevBlock->isCold();
   if (debugConsistencyCheck)
      fprintf(stderr, "Checking ordering consistency for method %s\n", comp->signature());
   for (; block; prevBlock = block, block = block->getNextBlock())
      {
      // see if block looks like it should follow prevBlock
      block->setVisitCount(visitCount);

      if (!block->isCold())
         {
         if (seenColdBlock)
            {
            char *fmt= "Non-cold block_%d found after a cold block in method %s\n";
            char *buffer = (char *) comp->trMemory()->allocateStackMemory((strlen(fmt) + strlen(comp->signature()) + 15) * sizeof(char));
            sprintf(buffer, fmt, block->getNumber(), comp->signature());
            //TR_ASSERT(0, buffer);
            }
         }
      else if (!seenColdBlock)
         {
         if (debugConsistencyCheck)
            fprintf(stderr, "First cold block_%d\n", block->getNumber());
         seenColdBlock = true;
         }

      bool prevFlowsToBlock = false;
      TR::CFGNode *prevsHotterSuccessor = NULL;

      if (!block->isExtensionOfPreviousBlock())
         {
         TR::CFGEdgeList & successors = prevBlock->getSuccessors();
         for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
            {
            TR::CFGNode *succ = (*succEdge)->getTo();
            if (succ->getVisitCount() == visitCount)
               continue;   // if laid out before us, it's not really a candidate for fall-through

            if (succ == block)
               prevFlowsToBlock = true;

            if (succ->getFrequency() > block->getFrequency())
               prevsHotterSuccessor = succ;
            else if (rootStructure && succ->getFrequency() == block->getFrequency() && succ->asBlock()->getNestingDepth() > block->asBlock()->getNestingDepth())
               prevsHotterSuccessor = succ;
            }
         }

      if (debugConsistencyCheck && prevFlowsToBlock && prevsHotterSuccessor != NULL)
         {
         if (rootStructure)
            fprintf(stderr, "Block %d(%d) doesn't look like the best successor compared to %d(%d)\n",
                block->getNumber(), block->getFrequency(),
                prevsHotterSuccessor->getNumber(), prevsHotterSuccessor->getFrequency());
         else
            fprintf(stderr, "Block %d(%d,%d) doesn't look like the best successor compared to %d(%d,%d)\n",
                block->getNumber(), block->getFrequency(), block->asBlock()->getNestingDepth(),
                prevsHotterSuccessor->getNumber(), prevsHotterSuccessor->getFrequency(), prevsHotterSuccessor->asBlock()->getNestingDepth());
         }
      }
   }


void TR_BlockOrderingOptimization::dumpBlockOrdering(TR::TreeTop *tt, char *title)
   {
   traceMsg(comp(), "%s:\n", title? title : "Block ordering");
   unsigned numberOfColdBlocks = 0;
   while (tt != NULL)
      {
      TR::Node *node = tt->getNode();
      if (node && node->getOpCodeValue() == TR::BBStart)
         {
         TR::Block *block = node->getBlock();
         traceMsg(comp(), "block_%-4d\t[ " POINTER_PRINTF_FORMAT "]\tfrequency %4d", block->getNumber(), block, block->getFrequency());
         if (block->isSuperCold())
            {
            numberOfColdBlocks++;
            traceMsg(comp(), "\t(super cold)\n");
            }
         else if (block->isCold())
            traceMsg(comp(), "\t(cold)\n");
         else
            traceMsg(comp(), "\n");
#if 0
         TR::CFGEdgeList & successors = block->getSuccessors();
         for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
            traceMsg(comp(), "\t -> block_%-4d\tfrequency %d\n", (*succEdge)->getTo()->getNumber(), (*succEdge)->getFrequency());
#endif
         }
      tt = tt->getNextTreeTop();
      }
   traceMsg(comp(), "\nTotal number of super cold blocks:%d \n",numberOfColdBlocks);
   }

TR::Block **TR_BlockShuffling::allocateBlockArray()
   {
   return (TR::Block **)trMemory()->allocateStackMemory(_numBlocks * sizeof(TR::Block *));
   }

void TR_BlockShuffling::traceBlocks(TR::Block **blocks)
   {
   const int32_t BLOCKS_PER_LINE=30;
   if (trace())
      {
      char *sep = "";
      for (int32_t i = 0; i < _numBlocks; i++)
         {
         traceMsg(comp(), "%s%d", sep, blocks[i]->getNumber());
         if ((i % BLOCKS_PER_LINE) == BLOCKS_PER_LINE-1)
            sep = "\n";
         else
            sep = " ";
         }
      }
   }

int32_t TR_BlockShuffling::perform()
   {
   // Initialization
   //
   TR::TreeTop *firstTree            = optimizer()->getMethodSymbol()->getFirstTreeTop();
   TR::Block   *firstExecutableBlock = firstTree->getNode()->getBlock();
   TR::Block   *firstShufflableBlock = firstExecutableBlock->getNextBlock(); // first executable block must be physically first

   if (!firstShufflableBlock) // empty method -- only has entry and exit blocks
      return 0;

   int32_t i; TR::Block *block;

   for (i=0, block=firstShufflableBlock; block; block = block->getNextBlock(), i++)
      {}

   _numBlocks = i;

   // Build the block array
   //
   TR::Block **blocks = allocateBlockArray();
   for (i=0, block=firstShufflableBlock; block; block = block->getNextBlock(), i++)
      blocks[i] = block;

   if (trace())
      dumpBlockOrdering(firstTree, "Initial block order");

   // Do the requested shuffling operations
   //
   char *sequence = comp()->getOptions()->getBlockShufflingSequence();
   if (trace())
      traceMsg(comp(), "Using shuffling sequence <%s>\n", sequence);
   for (const char *c = sequence; *c; c++)
      {
      // Convention: let's use uppercase for randomizing shuffles, and
      // lowercase for deterministic ones.
      //
      switch (*c)
         {
         case 'r': reverse(blocks);  break;
         case 'R': riffle(blocks);   break;
         case 'S': scramble(blocks); break;
         default:  TR_ASSERT(0, "Unknown shuffling sequence character '%c'", *c); break;
         }
      }

   // Rearrange the code according to the new block order.
   // To do this, we build a list of all blocks (not just shufflable ones) and
   // hand it to connectTreesAccordingToOrder.
   //
   TR_BlockList newBlockOrder(trMemory());
   newBlockOrder.add(comp()->getFlowGraph()->getEnd());
   for (int32_t i = _numBlocks-1; i >= 0; i--)
      newBlockOrder.add(blocks[i]);
   for (block = firstShufflableBlock->getPrevBlock(); block; block = block->getPrevBlock())
      newBlockOrder.add(block);
   newBlockOrder.add(comp()->getFlowGraph()->getStart());

   connectTreesAccordingToOrder(newBlockOrder);

   if (trace())
      dumpBlockOrdering(firstTree, "Final block order");

   return 0;
   }

void TR_BlockShuffling::scramble(TR::Block **blocks)
   {
   if (!performTransformation(comp(), "O^O BLOCK SHUFFLING: Performing scramble shuffle\n"))
      return;
   for (int32_t location = 0; location < _numBlocks; location++)
      {
      int32_t toMove = randomInt(location, _numBlocks-1);
      if (performTransformation(comp(), "O^O BLOCK SHUFFLING:   move to [%3d] block_%d\n", location, blocks[toMove]->getNumber()))
         swap(blocks, toMove, location);
      }
   }

void TR_BlockShuffling::riffle(TR::Block **blocks)
   {
   // Pick a split point that's likely near _numBlocks/2
   //
   int32_t split = 0;
   int32_t i;
   for (i = 0; i < 5; i++)
      split += randomInt(_numBlocks-1);
   split /= i;

   if (!performTransformation(comp(), "O^O BLOCK SHUFFLING: Performing riffle shuffle, splitting at #%d/%d = block_%d\n", split, _numBlocks, blocks[split]->getNumber()))
      return;

   // It's hard to merge in-place; let's keep it simple.
   //
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR::Block **originalBlocks = allocateBlockArray();
   memcpy(originalBlocks, blocks, _numBlocks * sizeof(blocks[0]));

   int32_t upper  = 0;
   int32_t lower  = split;
   int32_t target = 0;
   while (upper < split || lower < _numBlocks)
      {
      // We want the probability of picking the upper or lower card to be
      // proportional to how many cards are on each side, in order to
      // mix them evenly.
      //
      int32_t decision = randomInt(upper-split, _numBlocks-lower-1);
      if (decision < 0)
         {
         if (performTransformation(comp(), "O^O BLOCK SHUFFLING:   move to [%3d] upper (%3d) block_%d\n", target, upper, originalBlocks[upper]->getNumber()))
            blocks[target++] = originalBlocks[upper++];
         }
      else
         {
         if (performTransformation(comp(), "O^O BLOCK SHUFFLING:   move to [%3d] lower (%3d) block_%d\n", target, lower, originalBlocks[lower]->getNumber()))
            blocks[target++] = originalBlocks[lower++];
         }
      }
   }

void TR_BlockShuffling::reverse(TR::Block **blocks)
   {
   if (!performTransformation(comp(), "O^O BLOCK SHUFFLING: Reversing blocks\n"))
      return;

   int32_t upper, lower;
   for (upper=0, lower=_numBlocks-1; upper < lower; upper++, lower--)
      if (performTransformation(comp(), "O^O BLOCK SHUFFLING:   swap [%3d] and [%3d] (block_%d and block_%d)\n", upper, lower, blocks[upper]->getNumber(), blocks[lower]->getNumber()))
         swap(blocks, upper, lower);
   }

const char *
TR_BlockShuffling::optDetailString() const throw()
   {
   return "O^O BLOCK SHUFFLING: ";
   }
