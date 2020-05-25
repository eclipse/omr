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

#include "il/Block.hpp"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/ILWalk.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"

class TR_Memory;
namespace TR { class DebugCounterAggregation; }
namespace TR { class Instruction; }

TR::Block *
OMR::Block::createEmptyBlock(TR::Compilation *comp, int32_t frequency, TR::Block *block)
   {
   if (!comp->isPeekingMethod() && (comp->getFlowGraph()->getMaxFrequency() >= 0))
      TR_ASSERT((frequency >= 0), "Block frequency must be non negative\n");
   return TR::Block::createEmptyBlock(0, comp, frequency, block);
   }

TR::Block *
OMR::Block::asBlock() { return self(); }

OMR::Block::Block(TR_Memory * m) :
   TR::CFGNode(m)
   {
   self()->init(NULL, NULL);
   self()->setFrequency(-1);
   self()->setUnrollFactor(0);
   }

OMR::Block::Block(TR::CFG &cfg) :
   TR::CFGNode(cfg.getInternalRegion())
   {
   self()->init(NULL, NULL);
   self()->setFrequency(-1);
   self()->setUnrollFactor(0);
   }

OMR::Block::Block(TR::TreeTop *entry, TR::TreeTop *exit, TR_Memory * m) :
   TR::CFGNode(m)
   {
   self()->init(entry, exit);
   self()->setFrequency(-1);
   self()->setUnrollFactor(0);
   if (entry && entry->getNode()) entry->getNode()->setBlock(self());
   if (exit && exit->getNode())   exit->getNode()->setBlock(self());
   }

OMR::Block::Block(TR::TreeTop *entry, TR::TreeTop *exit, TR::CFG &cfg) :
   TR::CFGNode(cfg.getInternalRegion())
   {
   self()->init(entry, exit);
   self()->setFrequency(-1);
   self()->setUnrollFactor(0);
   if (entry && entry->getNode()) entry->getNode()->setBlock(self());
   if (exit && exit->getNode())   exit->getNode()->setBlock(self());
   }

void
OMR::Block::init(TR::TreeTop *entry, TR::TreeTop *exit)
   {
   _pEntry = entry;
   _pExit = exit;
   _pStructureOf = NULL;
   _liveLocals = NULL;
   _globalRegisters = 0;
   _catchBlockExtension = NULL;
   _firstInstruction = NULL;
   _lastInstruction = NULL;
   _blockSize = -1;
   _debugCounters = NULL;
   _flags = 0;
   _moreflags = 0;
   }

TR::Block*
OMR::Block::createBlock(TR::TreeTop *entry, TR::TreeTop *exit, TR::CFG &cfg)
   {
   return new (cfg.getInternalRegion()) TR::Block(entry, exit, cfg);
   }

/// Copy constructor
OMR::Block::Block(TR::Block &other, TR::TreeTop *entry, TR::TreeTop *exit) :
   TR::CFGNode(other._region),
   _pEntry(entry),
   _pExit(exit),
   _pStructureOf(NULL),
   _liveLocals(NULL),
   _globalRegisters(0),
   _firstInstruction(other._firstInstruction),
   _lastInstruction(other._lastInstruction),
   _blockSize(other._blockSize),
   _debugCounters(other._debugCounters),
   _catchBlockExtension(NULL)
   {
   if (entry && entry->getNode()) entry->getNode()->setBlock(self());
   if (exit && exit->getNode())   exit->getNode()->setBlock(self());

   if (other._liveLocals)
      _liveLocals = new (other._region) TR_BitVector(*other._liveLocals);

   if (other._catchBlockExtension)
      {
      _catchBlockExtension = new (other._region)
         TR_CatchBlockExtension(*other._catchBlockExtension);
      }

   self()->setFrequency(other.getFrequency());

   if (other._globalRegisters)
      _globalRegisters = new (other._region) TR_Array<TR_GlobalRegister>(*other._globalRegisters);

   _flags.set(other._flags.getValue());
   _moreflags.set(other._moreflags.getValue());
   _unrollFactor = other.getUnrollFactor();
   }

TR::Block *
OMR::Block::getNextBlock()
   {
   TR::TreeTop * t = _pExit->getNextTreeTop();
   return t ? t->getNode()->getBlock() : 0;
   }

TR::Block *
OMR::Block::getPrevBlock()
   {
   TR::TreeTop * t = _pEntry->getPrevTreeTop();
   return t ? t->getNode()->getBlock() : 0;
   }

TR::Block *
OMR::Block::getNextExtendedBlock()
   {
   TR::Block * b = self()->getNextBlock();
   for (; b && b->isExtensionOfPreviousBlock(); b = b->getNextBlock())
      ;
   return b;
   }


// append a node under a treetop and before the bbend
TR::TreeTop *
OMR::Block::append(TR::TreeTop * tt)
   {
   return self()->getExit()->insertBefore(tt);
   }

/**
 * Prepend a OMR::Block::node under a treetop immediately after the bbStart
 */
TR::TreeTop *
OMR::Block::prepend(TR::TreeTop * tt)
   {
   return self()->getEntry()->insertAfter(tt);
   }

TR::TreeTop *
OMR::Block::getFirstRealTreeTop()
   {
   TR_ASSERT(self()->getEntry() && self()->getEntry()->getNextTreeTop(), "Block must have valid entry treetop\n");
   TR::TreeTop * tt = self()->getEntry()->getNextTreeTop();
   while (tt->getNode()->getOpCode().isExceptionRangeFence())
      tt = tt->getNextTreeTop();
   return tt;
   }

TR::TreeTop *
OMR::Block::getLastRealTreeTop()
   {
   TR_ASSERT(self()->getExit() && self()->getExit()->getPrevTreeTop(), "Block must have valid exit treetop\n");
   TR::TreeTop * tt = self()->getExit()->getPrevTreeTop();
   while (tt->getNode()->getOpCode().isExceptionRangeFence())
      tt = tt->getPrevTreeTop();
   return tt;
   }

TR::TreeTop *
OMR::Block::getLastNonControlFlowTreeTop()
   {
   TR::TreeTop *tt = self()->getLastRealTreeTop();
   while (tt->getNode()->getOpCode().isBranch() ||
          tt->getNode()->getOpCode().isReturn() ||
          tt->getNode()->getOpCode().isJumpWithMultipleTargets())
      {
      tt = tt->getPrevTreeTop();
      }
   return tt;
   }

int32_t
OMR::Block::getNumberOfRealTreeTops()
   {
   TR_ASSERT(self()->getEntry(), "Block must have valid entry treetop\n");
   int32_t numRealTreeTops = 0;
   TR::TreeTop * tt = self()->getEntry()->getNextRealTreeTop();
   TR::TreeTop *exitTree = self()->getExit();
   while (tt != exitTree)
      {
      tt = tt->getNextRealTreeTop();
      numRealTreeTops++;
      }
   return numRealTreeTops;
   }


TR::Block *
OMR::Block::createEmptyBlock(TR::Node * n, TR::Compilation *comp, int32_t frequency, TR::Block *block)
   {
   if (!comp->isPeekingMethod() && (comp->getFlowGraph()->getMaxFrequency() >= 0))
      {
      TR_ASSERT((frequency >= 0), "Block frequency must be non negative\n");
      }

   if (block!=NULL) comp->setCurrentBlock(block);

   TR::Block * b =
      new (comp->trHeapMemory()) TR::Block(
         TR::TreeTop::create(comp, TR::Node::create(n, TR::BBStart)),
         TR::TreeTop::create(comp, TR::Node::create(n, TR::BBEnd)),
         comp->trMemory());
   b->getEntry()->join(b->getExit());
   b->setFrequency(frequency);
   return b;
   }

int32_t
OMR::Block::getMaxColdFrequency(TR::Block *b1, TR::Block *b2)
   {
   int32_t max = -1;
   int32_t frequency = b1->getFrequency();
   if (b1->isCold())
      max = frequency;
   frequency = b2->getFrequency();
   if (b2->isCold())
      {
      if (frequency > max)
         max = frequency;
      }
   //TR_ASSERT((max > -1), "At least one of the blocks should be cold\n");
   return max;
   }

int32_t
OMR::Block::getMinColdFrequency(TR::Block *b1, TR::Block *b2)
   {
   int32_t min = INT_MAX;
   int32_t frequency = b1->getFrequency();
   if (b1->isCold())
      min = frequency;
   frequency = b2->getFrequency();
   if (b2->isCold())
      {
      if (frequency < min)
         min = frequency;
      }
   TR_ASSERT((min < INT_MAX), "At least one of the blocks should be cold\n");
   return min;
   }

int32_t
OMR::Block::getScaledSpecializedFrequency(int32_t fastFrequency)
   {
   return (int32_t) ((float) (1.0f - MIN_PROFILED_FREQUENCY)*fastFrequency);
   }

bool
OMR::Block::isGotoBlock(TR::Compilation * comp, bool allowPrecedingSnapshots)
   {
   bool isGoToBlock = (   self()->getEntry() != NULL
                       && self()->getLastRealTreeTop()->getPrevTreeTop() == self()->getEntry()
                       && self()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto);
   if (!isGoToBlock && allowPrecedingSnapshots && comp->getOption(TR_EnableSnapshotBlockOpts) && comp->getMethodSymbol()->hasSnapshots())
      {
      if (self()->getEntry() != NULL && self()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto)
         {
         for (TR::TreeTop *tt = self()->getLastRealTreeTop()->getPrevTreeTop(); tt != self()->getEntry(); tt = tt->getPrevTreeTop())
            {
            return false;
            }
         isGoToBlock = true;
         }
      }
   return isGoToBlock;
   }

bool
OMR::Block::canFallThroughToNextBlock()
   {
   TR::Node *lastNode = self()->getLastRealTreeTop()->getNode();
   if (lastNode->getOpCodeValue() == TR::treetop)
      lastNode = lastNode->getFirstChild();
   if (lastNode->getOpCode().isGoto() ||
       lastNode->getOpCode().isReturn() ||
       lastNode->getOpCodeValue() == TR::athrow ||
       lastNode->getOpCodeValue() == TR::igoto)
      return false;
   return true;
   }

/**
 * Here we will detect the case where we have a label at BBStart but that will never be used.
 * If the only predecessor to the block belonging to the BBStart is the previous block that means that
 * we will fallthrough to this block without a branch, and therefore we don't need a label. There are
 * some exceptions:
 *   - If the previous block is a lookup, table or tstart block, then we might have a branch
 *     instruction to this block even if this block is the fallthrough block
 *   - if "canFallThroughToNextBlock()" returns false, we abort (it checks for similar situations)
 */
bool
OMR::Block::doesNotNeedLabelAtStart()
   {

   if (!self()->getEntry()->getNode()->getLabel())
      return true;

   if (self()->getEntry()->getNode()->getLabel()->isHasAddrTaken())
      return false;

   for (auto thisEdge = self()->getPredecessors().begin(); thisEdge != self()->getPredecessors().end(); ++thisEdge)
      {
      TR::Block * from = (*thisEdge)->getFrom()->asBlock();
      TR::Block * prevBlock = self()->getPrevBlock();
      if (from == prevBlock && from->canFallThroughToNextBlock())
         {
         TR::Node *lastNode = from->getLastRealTreeTop()->getNode();
         if (lastNode->getOpCodeValue() == TR::treetop)
            lastNode = lastNode->getFirstChild();
         if (lastNode->getOpCodeValue() != TR::lookup &&
             lastNode->getOpCodeValue() != TR::table &&
             lastNode->getOpCodeValue() != TR::tstart &&
             lastNode->getOpCodeValue() != TR::trtLookup)
            continue;
         }
      return false;
      }

   return true;
   }


/**
 * Return fall through edge within a Ebb if any.
 * Can handle dummy block created during inlining (callee has dummy nodes).
 */
TR::CFGEdge *
OMR::Block::getFallThroughEdgeInEBB()
   {
   TR::Block *fallThruCandidate = self()->getExit() ? self()->getNextBlock() : 0;
   if (fallThruCandidate && fallThruCandidate->isExtensionOfPreviousBlock())
      return self()->getEdge(fallThruCandidate);
   return 0;
   }



TR::Block *
OMR::Block::breakFallThrough(TR::Compilation *comp, TR::Block *faller, TR::Block *fallee)
   {
   TR::Node *lastNode = faller->getLastRealTreeTop()->getNode();
   if (lastNode->getOpCode().isCheck() || lastNode->getOpCodeValue() == TR::treetop)
      lastNode = lastNode->getFirstChild();

   if (lastNode->getOpCode().isReturn() ||
       lastNode->getOpCode().isGoto()   ||
       lastNode->getOpCode().isSwitch() ||
       lastNode->getOpCode().isJumpWithMultipleTargets() ||
       lastNode->getOpCodeValue() == TR::athrow)
      return faller; // nothing to do

   if (lastNode->getOpCode().isBranch())
      {
      TR::Node    *gotoNode = TR::Node::create(lastNode, TR::Goto);
      TR::TreeTop *gotoTree = TR::TreeTop::create(comp, gotoNode);
      gotoNode->setBranchDestination(fallee->getEntry());
      TR::Block  *gotoBlock = TR::Block::createEmptyBlock(lastNode, comp, (fallee->getFrequency() < faller->getFrequency()) ? fallee->getFrequency() : faller->getFrequency(), fallee);
      gotoBlock->append(gotoTree);

      faller->getExit()->join(gotoBlock->getEntry());
      gotoBlock->getExit()->join(fallee->getEntry());

      if (faller->getStructureOf())
         comp->getFlowGraph()->addNode(gotoBlock, faller->getCommonParentStructureIfExists(fallee, comp->getFlowGraph()));
      else
         comp->getFlowGraph()->addNode(gotoBlock);
      comp->getFlowGraph()->addEdge(TR::CFGEdge::createEdge(faller,  gotoBlock, comp->trMemory()));
      comp->getFlowGraph()->addEdge(TR::CFGEdge::createEdge(gotoBlock,  fallee, comp->trMemory()));
      // remove the edge only if the branch target is not pointing to the next block
      if (lastNode->getBranchDestination() != fallee->getEntry())
         comp->getFlowGraph()->removeEdge(faller, fallee);
      if (fallee->isCold() || faller->isCold())
         {
         //gotoBlock->setIsCold();
         if (fallee->isCold())
            gotoBlock->setFrequency(fallee->getFrequency());
         else
            gotoBlock->setFrequency(faller->getFrequency());
         }
      return gotoBlock;
      }

   TR::Node    *gotoNode = TR::Node::create(lastNode, TR::Goto);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp, gotoNode);
   gotoNode->setBranchDestination(fallee->getEntry());
   faller->append(gotoTree);
   return faller;
   }


/**
 * @note insertBlockAsFallThrough creates a new edge from block -> newFallThroughBlock.  It does _NOT_
 *       remove any fall-through edge that might have previously existed from block.  That responsibility
 *       is left to the caller since removing the edge at the wrong time could leave the original fallthrough
 *       successor with no predecessors which could leave it in an unexpectedly deleted state.
 */
void
OMR::Block::insertBlockAsFallThrough(TR::Compilation *comp, TR::Block *block, TR::Block *newFallThroughBlock)
   {
   TR::TreeTop *origFallThroughEntry = block->getExit()->getNextRealTreeTop();
   TR::TreeTop *newFallThroughEntry = newFallThroughBlock->getEntry();
   TR::TreeTop *beforeNewFallThrough = newFallThroughEntry->getPrevTreeTop();
   TR::TreeTop *afterNewFallThrough = newFallThroughBlock->getExit()->getNextTreeTop();
   TR::TreeTop *beforeOrigFallThrough = block->getExit();

   // rip newFallThrough block out of trees
   beforeNewFallThrough->join(afterNewFallThrough);

   // now insert newFallThrough between end of block and its original fall-through successor
   TR::TreeTop::insertTreeTops(comp, block->getExit(), newFallThroughBlock->getEntry(), newFallThroughBlock->getExit());
   if (!block->hasSuccessor(newFallThroughBlock))
      comp->getFlowGraph()->addEdge(block, newFallThroughBlock);
   }


/**
 * comment about when edge from from -> origTo will be removed (clients should be careful that origTo doesn't unexpectedly go poof!)
 * @note  redirectFlowToNewDestination will both create a new edge from "from" to "newTo" as well as remove the original edge from
 *        "from" to origTo.  BE CAREFUL when calling this function that there is another predecessor edge for "origTo" or else when
 *        redirectFlowToNewDestination removes the predecessor "from", "origTo" might be deleted.
 */
void
OMR::Block::redirectFlowToNewDestination(TR::Compilation *comp, TR::CFGEdge *origEdge, TR::Block *newTo, bool useGotoForFallThrough)
   {
   TR::Block *from = origEdge->getFrom()->asBlock();
   TR::Block *origTo = origEdge->getTo()->asBlock();
   int32_t fromBlockNumber = from->getNumber();
   int32_t origToBlockNumber = origTo->getNumber();
   int32_t newToBlockNumber = newTo->getNumber();
   if (from->getEntry() == NULL)
      {
      // from is entry, so only have to change the flow edges (no branch to change)
      if (!from->hasSuccessor(newTo))
         comp->getFlowGraph()->addEdge(from, newTo);
      comp->getFlowGraph()->removeEdge(from, origTo);
      }
   else
      {
      TR::TreeTop *lastTreeTop = from->getLastRealTreeTop();
      TR_ASSERT(lastTreeTop != NULL, "how can there be an entry but no last tree top?");
      TR::Node *lastNode = lastTreeTop->getNode();
      if (lastNode->getOpCode().isBranch() && lastNode->getBranchDestination() == origTo->getEntry())
         {
         // need to change the branch destination from origTo to newTo
         from->changeBranchDestination(newTo->getEntry(), comp->getFlowGraph());
         }
      else if (lastNode->getOpCode().isSwitch())
         {
         TR::TreeTop *origToEntry = origTo->getEntry();
         TR::TreeTop *newToEntry = newTo->getEntry();

         // find correct switch target(s) and change it to newTo->getNumber()
         for (int i=lastNode->getCaseIndexUpperBound()-1; i > 0; i--)
            {
            TR::Node *child=lastNode->getChild(i);
            if (child->getBranchDestination() == origToEntry)
               child->setBranchDestination(newToEntry);
            }
         if (!from->hasSuccessor(newTo))
            comp->getFlowGraph()->addEdge(from, newTo);
         comp->getFlowGraph()->removeEdge(from, origTo);
         }
      else if (lastNode->getOpCode().isJumpWithMultipleTargets())
         {
         if (lastNode->getOpCode().hasBranchChildren())
            {
            traceMsg(comp, "Jump with multple targets, with non fall through path to empty block\n");
            TR::TreeTop *origToEntry = origTo->getEntry();
            TR::TreeTop *newToEntry = newTo->getEntry();
            if(origToEntry)
               traceMsg(comp, "jumpwithmultipletargets: origToEntry->getNode = %p\n",origToEntry->getNode());
            if(newToEntry)
               traceMsg(comp, "jumpwithmultipletargets: newToEntry->getNode = %p\n",newToEntry->getNode());
            // Get the right branch target
            for (int32_t i=0 ; i < lastNode->getNumChildren()-1 ; i++)
               {
               TR::Node *child = lastNode->getChild(i);
               traceMsg(comp, "considering node %p with branch destination %p \n",child,child->getBranchDestination() ? child->getBranchDestination()->getNode() : 0 );
               if (child->getBranchDestination() == origToEntry)
                  {
                  child->setBranchDestination(newToEntry);
                  if (!from->hasSuccessor(newTo))
                     comp->getFlowGraph()->addEdge(from, newTo);
                  comp->getFlowGraph()->removeEdge(from, origTo);
                  }
               }
            }
         else
            {
            if (!from->hasSuccessor(newTo))
               comp->getFlowGraph()->addEdge(from, newTo);
            comp->getFlowGraph()->removeEdge(from, origTo);
            }
         }
      else
         // from falls through to origTo
         {
         TR::TreeTop *origToEntry = origTo->getEntry();
         TR::TreeTop *newToEntry = newTo->getEntry();
         TR::TreeTop *afterNewTo = newTo->getExit()->getNextTreeTop();
         TR_ASSERT(from->getExit()->getNextTreeTop() == origToEntry, "Expecting from's fallthrough to be origTo");

         if (useGotoForFallThrough)
            {
            TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp, TR::Node::create(lastNode, TR::Goto, 0, newTo->getEntry()));
            if (lastNode->getOpCode().isBranch())
               {
               int32_t freq = origEdge->getFrequency();

               TR::Block *newGotoBlock = TR::Block::createEmptyBlock(lastNode, comp, freq, from);
               newGotoBlock->append(gotoTreeTop);
               comp->getFlowGraph()->addNode(newGotoBlock);
               comp->getFlowGraph()->addEdge(from, newGotoBlock)->setFrequency(freq);
               newGotoBlock->setIsExtensionOfPreviousBlock();
               TR::Block::insertBlockAsFallThrough(comp, from, newGotoBlock);  // edge won't be duplicated

               comp->getFlowGraph()->addEdge(newGotoBlock, newTo)->setFrequency(freq);
               comp->getFlowGraph()->removeEdge(from, origTo);
               }
            else
               {
               from->append(gotoTreeTop);
               if (!from->hasSuccessor(newTo))
                  comp->getFlowGraph()->addEdge(from, newTo);
               comp->getFlowGraph()->removeEdge(from, origTo);
               }
            }
         else
            {
            TR::Block::insertBlockAsFallThrough(comp, from, newTo);
            comp->getFlowGraph()->removeEdge(from, origTo);
            }
         }
      }
   }

/**
 * This block is being removed from the CFG.
 * Remove its trees from the list of trees.
 */
void
OMR::Block::removeFromCFG(TR::Compilation *c)
   {
   // Remove the trees for this block
   //
   if (_pEntry != NULL)
      {

      // Remove trees
      for (TR::TreeTop *treeTop = _pEntry, *next; ; treeTop = next)
         {
         next = treeTop->getNextTreeTop();
         TR::TransformUtil::removeTree(c, treeTop);
         if (treeTop == _pExit)
            break;
         }
      }

   }


TR_RegionStructure *
OMR::Block::getParentStructureIfExists(TR::CFG *cfg)
   {
   TR_RegionStructure *parent = NULL;
   if (self()->getStructureOf() &&
       cfg->getStructure())
      parent = self()->getStructureOf()->getParent()->asRegion();

   return parent;
   }

TR_RegionStructure *
OMR::Block::getCommonParentStructureIfExists(TR::Block *b, TR::CFG *cfg)
   {
   TR_RegionStructure *parent = NULL;
   if (self()->getStructureOf() &&
       b &&
       b->getStructureOf() &&
       cfg->getStructure())
      parent = self()->getStructureOf()->findCommonParent(b->getStructureOf(), cfg);

   return parent;
   }

void
OMR::Block::removeBranch(TR::Compilation *c)
   {
   TR::TreeTop * tt = self()->getLastRealTreeTop();
   TR_ASSERT(tt->getNode()->getOpCode().isBranch(), "OMR::Block::removeBranch, block doesn't have a branch");
   c->getFlowGraph()->removeEdge(self(), tt->getNode()->getBranchDestination()->getNode()->getBlock());
   TR::TransformUtil::removeTree(c, tt);
   }

TR::Block *
OMR::Block::findVirtualGuardBlock(TR::CFG *cfg)
   {
   //TR_ASSERT(getExceptionPredecessors().empty(), "snippet block has an exception predecessor");
   for (auto nextEdge = self()->getPredecessors().begin(); nextEdge != self()->getPredecessors().end(); ++nextEdge)
      {
      TR::Block *prevBlock = toBlock((*nextEdge)->getFrom());
      // check for the entry block being the predecessor
      // ie. the guard has probably been removed
      if (prevBlock == cfg->getStart())
         break;
      //else if (prevBlock->getLastRealTreeTop()->getNode()->isTheVirtualGuardForAGuardedInlinedCall())
      //   return prevBlock;
      else
         {
         TR::Node *node = prevBlock->getLastRealTreeTop()->getNode();
         if (node->isTheVirtualGuardForAGuardedInlinedCall())
            {
            // extra check for profiled guards based on VFT
            if (node->isProfiledGuard())
               {
               TR_ASSERT(((node->getOpCodeValue() == TR::ifacmpne) || (node->getOpCodeValue() == TR::ifacmpeq)), "Profile guards must use TR::ifacmpne || TR::ifacmpeq");
               if (node->getOpCodeValue() == TR::ifacmpne)
                  {
                  if (node->getBranchDestination()->getEnclosingBlock() == self())
                     return prevBlock;
                  }
               else if (node->getOpCodeValue() == TR::ifacmpeq)
                  {
                  if (prevBlock->getNextBlock() == self())
                     return prevBlock;
                  }
               }
            else
               {
               return prevBlock;
               }
            }
         }
      }
   return 0;
   }

void
OMR::Block::changeBranchDestination(TR::TreeTop * newDestination, TR::CFG *cfg, bool callerFixesRegdeps)
   {

   TR::Node * branchNode = self()->getLastRealTreeTop()->getNode();
   TR_ASSERT(branchNode->getOpCode().isBranch(), "OMR::Block::changeBranchDestination: can't find the existing branch node");

   TR::TreeTop * prevDestinationTree = branchNode->getBranchDestination();
   if (newDestination == prevDestinationTree)
      return; // Nothing to do. Stop here so we don't remove the edge.

   TR::Block * prevDestinationBlock = prevDestinationTree->getNode()->getBlock();

   branchNode->setBranchDestination(newDestination);

   TR::Block * newDestinationBlock = newDestination->getNode()->getBlock();
   TR::CFGEdge *prevEdge = self()->getEdge(prevDestinationBlock);
   if (!self()->hasSuccessor(newDestinationBlock))
      {
      TR::CFGEdge *newEdge = cfg->addEdge(self(), newDestinationBlock);
      int32_t prevEdgeFrequency = prevEdge->getFrequency();
      if (prevDestinationBlock->getFrequency() > 0)
         {
         int32_t newEdgeFrequency = (prevEdgeFrequency*newDestinationBlock->getFrequency())/prevDestinationBlock->getFrequency();
         newEdge->setFrequency(newEdgeFrequency);
         }
      }

   cfg->removeEdge(prevEdge);

   if (callerFixesRegdeps)
      return;

   int32_t numChildren = branchNode->getNumChildren();
   if (numChildren >0 &&
      branchNode->getChild(numChildren - 1)->getOpCodeValue() == TR::GlRegDeps)
      {
      TR::Node * fromGlRegDep = branchNode->getChild(numChildren - 1);
      TR_ASSERT(fromGlRegDep->getOpCodeValue() == TR::GlRegDeps, "expected the branch child to be a GlRegDep.  It's not!");
      int32_t numFromGlRegDeps = fromGlRegDep->getNumChildren();
      if (newDestination->getNode()->getNumChildren() == 0)
         {
         branchNode->setNumChildren(numChildren - 1);
         for (int32_t i = 0; i < numFromGlRegDeps; ++i)
            fromGlRegDep->getChild(i)->recursivelyDecReferenceCount();
         }
      else
         {
         TR::Node * toGlRegDep = newDestination->getNode()->getChild(0);
         TR_ASSERT(toGlRegDep->getOpCodeValue() == TR::GlRegDeps, "expected the bbstart child to be a GlRegDep.  It's not!");
   TR_ASSERT(0, "todo: update the GlRegDep info");
         }
      }
   else
      {
      TR_ASSERT(newDestination->getNode()->getNumChildren() == 0, "todo: update the GlRegDep info");
      }
   }

void
OMR::Block::uncommonNodesBetweenBlocks(TR::Compilation *comp, TR::Block *newBlock, TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR_ScratchList<TR::SymbolReference> symbolReferenceTempsA(comp->trMemory());
   TR_ScratchList<TR::SymbolReference> injectedBasicBlockTemps(comp->trMemory());
   TR_ScratchList<TR::SymbolReference> symbolReferenceTempsC(comp->trMemory());

   TR_HandleInjectedBasicBlock ibb(comp, NULL, methodSymbol? methodSymbol: comp->getMethodSymbol(),
                                   symbolReferenceTempsA,
                                   injectedBasicBlockTemps,
                                   symbolReferenceTempsC);
   ibb.findAndReplaceReferences(self()->getEntry(), newBlock, NULL);

   ListIterator<TR::SymbolReference> injectedBasicBlockTempIter(&injectedBasicBlockTemps);
   TR::SymbolReference * injectedBasicBlockTemp = injectedBasicBlockTempIter.getFirst();

   for (; injectedBasicBlockTemp; injectedBasicBlockTemp = injectedBasicBlockTempIter.getNext())
      {
      comp->getMethodSymbol()->addAutomatic(injectedBasicBlockTemp->getSymbol()->castToAutoSymbol());
      }
   }

bool
OMR::Block::hasExceptionSuccessors()
   {
   bool hasExceptionSuccessors = (self()->getExceptionSuccessors().empty() == false);
   return hasExceptionSuccessors;
   }

bool
OMR::Block::isTargetOfJumpWhoseTargetCanBeChanged(TR::Compilation * comp)
   {
   TR::Block *startBlock = comp->getFlowGraph()->getStart()->asBlock();
   for (auto predEdge = self()->getPredecessors().begin(); predEdge != self()->getPredecessors().end(); ++predEdge)
      {
      TR::Block *predBlock = (*predEdge)->getFrom()->asBlock();
      if (predBlock != startBlock && predBlock->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets() &&
          predBlock->getLastRealTreeTop()->getNode()->getOpCode().hasBranchChildren())
         return true;
      }
   return false;
   }

TR::Block *
OMR::Block::splitWithGivenMethodSymbol(TR::ResolvedMethodSymbol *methodSymbol, TR::TreeTop * startOfNewBlock, TR::CFG * cfg, bool fixupCommoning, bool copyExceptionSuccessors){
   return self()->split(startOfNewBlock, cfg, fixupCommoning, copyExceptionSuccessors, methodSymbol);
}

// Required data structures for analysis in SplitPostGRA
typedef TR::typed_allocator<std::pair<TR::Node* const, std::pair<int32_t, TR::Node *> >, TR::Region&> NodeTableAllocator; 
typedef std::less< TR::Node *> NodeTableComparator; 
typedef std::map<TR::Node *, std::pair<int32_t, TR::Node *>, NodeTableComparator, NodeTableAllocator> NodeTable;

typedef TR::typed_allocator<std::pair <TR::Node* const, TR::Node *>, TR::Region&> StoreRegNodeTableAllocator;
typedef std::map<TR::Node *, TR::Node *, NodeTableComparator, StoreRegNodeTableAllocator> StoreRegNodeTable;

/*
 * Recursive call to replace node with replacement node underneath a target node.
 *
 * @param A node to be replaced
 * @param A table containing information about original nodes to be replaced with corresponding replacement node
 * @param checklist Checklist of nodes that have already been searched.
 */
static void replaceNodesInSubtree(TR::Node *node, NodeTable *nodeInfo, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return;

   checklist.add(node);

   for (int i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      auto entry = nodeInfo->find(child);
      if (entry != nodeInfo->end())
         {
         node->setAndIncChild(i, entry->second.second);
         child->decReferenceCount();
         }
      else
         {
         replaceNodesInSubtree(child, nodeInfo, checklist);
         }
      }
   }

/*
 * Search for the nodes to be uncommoned from the split point till end of extended basic block
 *
 * @param Compilation object
 * @param start tree top from where it needs to start searching
 * @param table containing information about original nodes to be replaced with corresponding replacement node 
 */
static void replaceNodesInTrees(TR::Compilation * comp, TR::TreeTop *start, NodeTable *nodeInfo)
   {
   TR::NodeChecklist checklist(comp);
   while (start)
      {
      if (start->getNode()->getOpCodeValue() == TR::BBStart && !start->getNode()->getBlock()->isExtensionOfPreviousBlock())
         break;
      replaceNodesInSubtree(start->getNode(), nodeInfo, checklist);
      start = start->getNextTreeTop();
      }
   }

/*
 * Analyzes the node and list of unavailable registers to find a register pair which can be used to store the node
 *
 * @param Compilation object
 * @param Node which requires to be uncommoned
 * @param BitVector constaining information about the unavailable registers
 * @return Pair of GlobalRegisterNumber which can be used to store node
 */
static std::pair<TR_GlobalRegisterNumber,TR_GlobalRegisterNumber> findAvailableRegister(TR::Compilation *comp, TR::Node *node, TR_BitVector &unavailableRegisters)
   {
   TR_GlobalRegisterNumber firstGPR = comp->cg()->getFirstGlobalGPR();
   TR_GlobalRegisterNumber lastGPR = comp->cg()->getLastGlobalGPR();
   TR_GlobalRegisterNumber firstFPR = comp->cg()->getFirstGlobalFPR();
   TR_GlobalRegisterNumber lastFPR = comp->cg()->getLastGlobalFPR();
   TR_GlobalRegisterNumber firstVRF = comp->cg()->getFirstGlobalVRF();
   TR_GlobalRegisterNumber lastVRF = comp->cg()->getLastGlobalVRF();

   std::pair<TR_GlobalRegisterNumber,TR_GlobalRegisterNumber> toReturn = std::make_pair<TR_GlobalRegisterNumber,TR_GlobalRegisterNumber>(-1,-1);

   TR_GlobalRegisterNumber start, end;
   TR::DataType dt = node->getType();
   if (dt.isIntegral() || dt.isAddress())
      {
      start = firstGPR;
      end = lastGPR;
      }
   else if (dt.isFloatingPoint())
      {
      start = firstFPR;
      end = lastFPR;
      }
   else if (dt.isVector())
      {
      start = firstVRF;
      end = lastVRF;
      }
   else
      TR_ASSERT_FATAL(false, "Unknown data type %s encountered when trying to pick a register for postGRA block splitting!", dt.toString());

   for (TR_GlobalRegisterNumber itr = start; itr < end; ++itr)
      {
      if (!unavailableRegisters.get(itr))
         {
         toReturn.first = itr;
         unavailableRegisters.set(itr);
         break;
         }
      }
   if (node->requiresRegisterPair(comp) && toReturn.first > -1)
      {
      for (TR_GlobalRegisterNumber itr = start; itr < end; ++itr)
         {
         if (!unavailableRegisters.get(itr))
            {
            toReturn.second = itr;
            unavailableRegisters.set(itr);
            break;
            }
         }
      if (toReturn.second == -1)
         {
         unavailableRegisters.reset(toReturn.first);
         toReturn.first = -1;
         }
      }
   return toReturn;
   }

/*
 * Returns a boolean notifying if the registers used by given nodes are available or not
 *
 * @param Compilation object
 * @param Node whose used registers are required to be checked for availability 
 * @param BitVector constaining information about the unavailable registers
 * @return A boolean notifying if the registers used by the RegStore can be uses for uncommoning node
 */
static bool checkIfRegisterIsAvailable(TR::Compilation *comp, TR::Node *node, TR_BitVector &unavailableRegisters)
   {
   // Following assert is to make sure that this function is called with node which have Global Registers associated with it.
   TR_ASSERT_FATAL(node->getOpCode().isStoreReg(), "checkIfRegisterIsAvailable is used with %s while it is intended to use with RegStore nodes only", node->getName(comp->getDebug()));
   bool registersAreAvailable = !unavailableRegisters.isSet(node->getGlobalRegisterNumber());
   if (node->requiresRegisterPair(comp))
      registersAreAvailable &= !unavailableRegisters.isSet(node->getHighGlobalRegisterNumber());
   return registersAreAvailable; 
   }

/*
 * Iterate through the list of StoreReg Node and associate the given passThroughNode with it.
 *
 * @param PassThrough node
 * @param List of RegStore nodes
 * @return A boolean notifying if registers used by passthrough node can be used for uncommoning
 */
static bool checkStoreRegNodeListForNode(TR::Node *passThroughNode, List<TR::Node> *storeNodeList)
   {
   TR::Node *value = passThroughNode->getFirstChild();
   ListIterator<TR::Node> nodeListIter(storeNodeList);
   for (TR::Node *storeNode = nodeListIter.getCurrent(); storeNode; storeNode=nodeListIter.getNext())
      {
      if (storeNode->getFirstChild() == value &&
         storeNode->getLowGlobalRegisterNumber() == passThroughNode->getLowGlobalRegisterNumber() &&
         storeNode->getHighGlobalRegisterNumber() == passThroughNode->getHighGlobalRegisterNumber())
         return true;
      }
   return false;
   }

/*
 * Analyze the register dependency and update the list of unavailable registers.
 * It examines the node under passthrough children of the glRegDeps and does the following.
 *    If node requires uncommoning and we do not have replacement yet, looks into the list
 *    that contains regStore before split point.
 *       a. If register is available, uses the register to create replacement.
 *       b. If register is not available, and corresponding is before splitPoint 
 *          then creates a regStore of the node to that register before passThrough node which uses it
 *
 * @param Compilation object
 * @param GlRegDep node which will be analyzed
 * @param TreeTop with the given GlRegDep child
 * @param NodeTable containing information about the nodes which requires uncommoning across split point
 * @param StoreRegNodeTable Table containing information about node which are stored into the registers before split point
 * @param List of the RegStores post split point
 * @param BitVector containing information about unavailable registers
 */
static void gatherUnavailableRegisters(TR::Compilation *comp, TR::Node *regDeps, TR::TreeTop *currentTT, 
                                          NodeTable *nodeInfo, StoreRegNodeTable *storeNodeInfo,
                                          List<TR::Node> *storeRegNodePostSplitPoint, TR_BitVector &unavailableRegisters)
   {
   TR_ASSERT_FATAL(regDeps->getOpCodeValue() == TR::GlRegDeps, "A GlRegDeps node is required to gather unavaialble registers\n");
   for (int i = 0; i < regDeps->getNumChildren(); i++)
      {
      TR::Node *dep = regDeps->getChild(i);
      if (dep->getOpCodeValue() == TR::PassThrough)
         {
         TR::Node *value = dep->getFirstChild();
         auto nodeInfoEntry = nodeInfo->find(value);
         // If a node referenced under the PassThrough node post split point requires uncommoning
         // We need to check if the we can use the information to allocate the register for the node or not.
         if (nodeInfoEntry != nodeInfo->end())
            {
            bool needToCheckStoreRegPostSplitPoint = true;
            bool needToCreateRegStore = true;
            if (nodeInfoEntry->second.second == NULL)
               {
               auto storeRegNodeInfoEntry = storeNodeInfo->find(value);
               if (storeRegNodeInfoEntry == storeNodeInfo->end() && checkStoreRegNodeListForNode(dep, storeRegNodePostSplitPoint))
                  {
                  // Node under the PassThrough is not stored into register before the split point which means we must have stored it into the register after split point.
                  // In this case we do not have to do anything as when we replace the node post split point, it will be replaced under corresponding regstore as well.
                  needToCreateRegStore = false;
                  }
               else
                  {
                  // Node under passthrough is stored into some set of registers (Not necessarily same set of registers as we only record last regStore for the node)
                  // and if those set of registers are available, then use them to create a replacement.
                  TR_ASSERT_FATAL (storeRegNodeInfoEntry != storeNodeInfo->end(), "We should have a regStore pre split point that can be used to allocate register for replacement node");
                  TR::Node *storeNode = storeRegNodeInfoEntry->second;
                  if (checkIfRegisterIsAvailable(comp, storeNode, unavailableRegisters))
                     {
                     // Whether the PassThrough uses same register or not, use the last recorded regStore to assign register to the node.
                     TR::Node *regLoad = TR::Node::create(value, comp->il.opCodeForRegisterLoad(value->getDataType()));
                     regLoad->setRegLoadStoreSymbolReference(storeNode->getRegLoadStoreSymbolReference());
                     regLoad->setGlobalRegisterNumber(storeNode->getGlobalRegisterNumber());
                     unavailableRegisters.set(storeNode->getGlobalRegisterNumber());
                     if (value->requiresRegisterPair(comp))
                        {
                        regLoad->setHighGlobalRegisterNumber(storeNode->getHighGlobalRegisterNumber());
                        unavailableRegisters.set(storeNode->getHighGlobalRegisterNumber());
                        }
                     nodeInfoEntry->second.second = regLoad;
                     // Course of action to analyze reg store for block splitter is to check for the registers used for the node post split point before
                     // we check the regStore information pre split point. As regStore post split point is most likely to be used later. This will reduce number of shuffles.
                     // So if we get the candidate from before split point, we do not need to check list of RegStores post split point.
                     needToCheckStoreRegPostSplitPoint = false;
                     }
                  }
               }
   
            
            if (nodeInfoEntry->second.second != NULL && nodeInfoEntry->second.second->getOpCode().isLoadReg() && 
               dep->getLowGlobalRegisterNumber() == nodeInfoEntry->second.second->getLowGlobalRegisterNumber() && 
               dep->getHighGlobalRegisterNumber() == nodeInfoEntry->second.second->getHighGlobalRegisterNumber())
               {
               // Passthrough uses same register as the replacement, do not need to do anything
               }
            else if (needToCreateRegStore && (!needToCheckStoreRegPostSplitPoint || !checkStoreRegNodeListForNode(dep, storeRegNodePostSplitPoint)))
               {
               // We have either used different register for replacement to value node after split point or Passthrough has corresponding regStore before spilt point, but registers were unavailable.
               // In these cases we need to create a regStore for the PassThrough.
               // If we have replacement then need to store the replacement to register, else, value needs to be stored. 
               TR::Node *nodeToBeStored = nodeInfoEntry->second.second != NULL ? nodeInfoEntry->second.second : value;
               TR::Node *regStore = TR::Node::create(value, comp->il.opCodeForRegisterStore(value->getDataType()), 1, nodeToBeStored);
               currentTT->insertBefore(TR::TreeTop::create(comp, regStore));
               regStore->setGlobalRegisterNumber(dep->getGlobalRegisterNumber());
               if (nodeToBeStored->requiresRegisterPair(comp))
                  regStore->setHighGlobalRegisterNumber(dep->getHighGlobalRegisterNumber());
               TR::SymbolReference *ref = NULL;
               if (nodeToBeStored->getOpCode().isLoadConst() || nodeToBeStored->getOpCodeValue() == TR::loadaddr || nodeInfoEntry->second.second == NULL)
                  {
                  // See if we have stored constants in register before split point.
                  auto storeRegNodeInfoEntry = storeNodeInfo->find(value);
                  TR_ASSERT_FATAL(storeRegNodeInfoEntry != storeNodeInfo->end(),
                     "We have a node n%dn under PassThrough node n%dn but we did not find a regStore that is using the info.", value->getGlobalIndex(), dep->getGlobalIndex());
                  ref = storeRegNodeInfoEntry->second->getRegLoadStoreSymbolReference();
                  }
               else
                  {
                  ref = nodeToBeStored->getRegLoadStoreSymbolReference();
                  }
               regStore->setRegLoadStoreSymbolReference(ref);
               dep->setAndIncChild(0, nodeToBeStored);
               value->decReferenceCount();
               }
            }
         }
      else if (!dep->getOpCode().isLoadReg())
         {
         TR_ASSERT_FATAL(false, "Expected to find only PassThrough and regLoad operations under a GlRegDepNode, but got %s\n", dep->getName(comp->getDebug()));
         }
      }
   }

/*
 * When a node is stored into either a register or on temp/auto slot, it needs corresponding symbol reference which
 * is flagged properly as per the type of the node. Creates a symbol reference to be used to store the node.
 *
 * @param Compilation object
 * @param MethodSymbol object
 * @param value Node for which we need to create a symref
 * @param insertBefore treetop before which if needed store node tree will be created
 * @return A SymbolReference to store given node
 */
static TR::SymbolReference * createSymRefForNode(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, TR::Node *value, TR::TreeTop *insertBefore)
   {
   TR::DataType dataType = value->getDataType();
   TR::SymbolReference *symRef = NULL;
   if (value->isInternalPointer() && value->getPinningArrayPointer())
      {
      symRef = comp->getSymRefTab()->createTemporary(methodSymbol, TR::Address, true, 0);
      TR::Symbol *symbol = symRef->getSymbol()->castToInternalPointerAutoSymbol()->setPinningArrayPointer(value->getPinningArrayPointer());
      }
   else
      {
      bool isInternalPointer = false;
      if ((value->hasPinningArrayPointer() &&
            value->computeIsInternalPointer()) ||
               (value->getOpCode().isLoadVarDirect() &&
                  value->getSymbolReference()->getSymbol()->isAuto() &&
                  value->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer()))
         isInternalPointer = true;
      
      if (value->isNotCollected() && dataType == TR::Address)
         {
         symRef = comp->getSymRefTab()->createTemporary(methodSymbol, dataType, false, 
#ifdef J9_PROJECT_SPECIFIC
                                                                                    value->getType().isBCD() ? value->getSize() : 
#endif   
                                                                                    0);
         symRef->getSymbol()->setNotCollected();
         isInternalPointer = false;
         }
      if (isInternalPointer)
         {
         symRef = comp->getSymRefTab()->createTemporary(methodSymbol, TR::Address, true, 0);
         if (value->isNotCollected())
            symRef->getSymbol()->setNotCollected();
         else if (value->getOpCode().isArrayRef())
            value->setIsInternalPointer(true);
         
         TR::AutomaticSymbol *pinningArray = NULL;
         if (value->getOpCode().isArrayRef())
            {
            TR::Node *valueChild = value->getFirstChild();
            if (valueChild->isInternalPointer() &&
                  valueChild->getPinningArrayPointer())
               {
               pinningArray = valueChild->getPinningArrayPointer();
               }
            else
               {
               while (valueChild->getOpCode().isArrayRef())
                  valueChild = valueChild->getFirstChild();
               // If the node we are uncommoning is internal pointer and while iterating a child we found a node that is not 
               // Array Reference, There are only two possibilities.
               // 1. It is  internal poiter which was commoned means could be stored into register or on temp slot. 
               // 2. Itself is a pointer to array object.
               TR::SymbolReference *valueChildSymRef = valueChild->getSymbolReference();
               if (valueChildSymRef != NULL &&
                  (valueChild->getOpCode().isLoadVarDirect() && valueChildSymRef->getSymbol()->isAuto()) ||
                  (valueChild->getOpCode().isLoadReg() && valueChildSymRef->getSymbol()->castToAutoSymbol()->isInternalPointer()))
                  {
                  if (valueChildSymRef->getSymbol()->castToAutoSymbol()->isInternalPointer())
                     {
                     pinningArray = valueChildSymRef->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer();
                     }
                  else
                     {
                     pinningArray = valueChildSymRef->getSymbol()->castToAutoSymbol();
                     pinningArray->setPinningArrayPointer();
                     }
                  }
               else
                  {
                  TR::SymbolReference *newValueArrayRef = comp->getSymRefTab()->
                                                         createTemporary(methodSymbol, TR::Address, false, 0);
                  TR::Node *newStore = TR::Node::createStore(newValueArrayRef, valueChild);
                  TR::TreeTop *newStoreTree = TR::TreeTop::create(comp, newStore);
                  insertBefore->insertBefore(newStoreTree);
                  if (!newValueArrayRef->getSymbol()->isParm()) // newValueArrayRef could contain a parm symbol
                     {
                     pinningArray = newValueArrayRef->getSymbol()->castToAutoSymbol();
                     pinningArray->setPinningArrayPointer();
                     }
                  }
               }
            }
         else
            {
            pinningArray = value->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer();
            }
         symRef->getSymbol()->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArray);
         if (value->isInternalPointer() && pinningArray)
            {
            value->setPinningArrayPointer(pinningArray);
            } 
         }
      }
   
   if (dataType == TR::Aggregate)
      {
      uint32_t size = value->getSize();
      symRef = new (comp->trHeapMemory()) TR::SymbolReference(comp->getSymRefTab(),
                         TR::AutomaticSymbol::create(comp->trHeapMemory(),dataType,size),
                         methodSymbol->getResolvedMethodIndex(),
                         methodSymbol->incTempIndex(comp->fe()));


      if (value->isNotCollected())
         symRef->getSymbol()->setNotCollected();
      }

   if (!symRef)
      {
      symRef = comp->getSymRefTab()->createTemporary(methodSymbol, value->getDataType());
      if (value->getType() == TR::Address && value->isNotCollected())
         symRef->getSymbol()->setNotCollected();
      }
   return symRef;
   }

/**
 * Splits the blocks from the split point manually handling the GlRegDeps created by the Global Register Allocator.
 * If there are available global registers which can be used, it will utililze them to uncommon nodes across split points.
 * 
 * @details
 * It uses following simplified algorithm to split blocks and uncommon nodes. 
 * 1. Analyze trees before the split point to gather information about nodes which are referenced in new block and needs replacement after split.
 * 2. Now as it has information about all the nodes which are required to be uncommoned, it starts cretaing replacment nodes as per following rules.
 *    a. If node to be uncommoned is regLoad / load of constant then replacement node will be copy of the original node and also mark the used register unavailable. 
 *    b. If node to be uncommoned is stored into a register after split point and that register is available, then replacement node will be regLoad from that
 *       register. It also makes sure that regStore tree is moved to before split point also marks used register unavailable.
 *    c. If node to be uncommoned is stored into a register before split point and node does not succeeds in [b] case and register is available then replacement
 *       node will be regLoad from that register also marks used register unavailable.
 * 3. While checking for case-b and case-c from Step 2 it also checks regStores and PassThrough nodes after split point. 
 *    a. If node under pass through requires to be uncommoned and uses different register then the replacement node
 *       then it stores replacement node into a register used by passThrough before the tree containing that passthrough node.
 *    b. If node under regStore does not need to be uncommoned, then mark the used register unavailable. 
 * 4. For the remaining of nodes to be uncommoned, it tries to find the available register using list of unavailable registers and if it can find one, then stores node
 *    into that register otherwise creates a auto/temp slot to uncommon them. 
 * 5. Last step is to start walking treetops after split point and replace nodes to be uncommoned with corresponding replacement nodes.
 * 
 * @param startOfNewBlock TreeTop from which new block will start
 * @param cfg TR::CFG object
 * @param copyExceptionSuccessors Boolean stating if we need to copy the exceptionSuccessors of the original blocks to new block
 * @param methodSymbol TR::ResolvedMethodSymbol object
 * @return TR::Block* Returns new Block
 */
TR::Block *
OMR::Block::splitPostGRA(TR::TreeTop * startOfNewBlock, TR::CFG *cfg, bool copyExceptionSuccessors, TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR::Compilation *comp = cfg->comp();
   TR::Block *newBlock = self()->split(startOfNewBlock, cfg, false, copyExceptionSuccessors, methodSymbol);
   if (methodSymbol == NULL)
      methodSymbol = comp->getMethodSymbol();


      {
      TR::StackMemoryRegion stackMemoryRegion(*(comp->trMemory())); 
      NodeTable *nodeInfo = new (stackMemoryRegion) NodeTable(NodeTableComparator(), NodeTableAllocator(stackMemoryRegion));
      StoreRegNodeTable *storeNodeInfo = new (stackMemoryRegion) StoreRegNodeTable(NodeTableComparator(), StoreRegNodeTableAllocator(stackMemoryRegion));
      TR::Block *startOfExtendedBlock = self()->startOfExtendedBlock();
      TR::TreeTop *start = startOfExtendedBlock->getEntry();
      TR::TreeTop *end = self()->getExit();
      /**
       * Step-1 : Analyze the trees before split point to record nodes which will be referenced in new block and needs replacement after split
       * It collects following information.
       * a. Node which are referenced after split point in nodeInfo
       * b. Node which are referenced after split point and stored into register in storeNodeInfo
       */
      for (TR::PostorderNodeOccurrenceIterator iter(start, comp, "FIX_POSTGRA_SPLIT_COMMONING"); 
         iter != end; ++iter)
         {
         TR::Node *node = iter.currentNode();
         auto entry = nodeInfo->find(node);
         if (entry == nodeInfo->end() && node->getReferenceCount() > 1)
            {
            (*nodeInfo)[node] = std::make_pair<int32_t, TR::Node*>(node->getReferenceCount() - 1, NULL);
            }
         else if (entry != nodeInfo->end() && entry->second.first > 1)
            {
            entry->second.first -= 1;
            }
         else if (entry != nodeInfo->end())
            {
            nodeInfo->erase(entry);
            auto storeNodeEntry = storeNodeInfo->find(node);
            if (storeNodeEntry != storeNodeInfo->end())
               storeNodeInfo->erase(storeNodeEntry);
            }
         // Record the last used RegStore for the node before split point
         if (node->getOpCode().isStoreReg())
            {
            storeNodeInfo->insert(std::make_pair(node->getFirstChild(), node));
            }
         }

      /** 
       * Step - 2: Analyze nodeInfo for regLoad / load constant.
       *    if found, create a replacement node which is copy of the original node. 
       *    In case of regLoad mark used registers unavailable
       */ 
      
      TR_BitVector unavailableRegisters(0, comp->trMemory(), stackAlloc);
      for (auto iter = nodeInfo->begin(), end = nodeInfo->end(); iter != end; ++iter)
         {
         TR::Node *node = iter->first;
         if (node->getOpCode().isLoadReg())
            {
            if (node->requiresRegisterPair(comp))
               {
               unavailableRegisters.set(node->getLowGlobalRegisterNumber());
               unavailableRegisters.set(node->getHighGlobalRegisterNumber());
               }
            else
               {
               unavailableRegisters.set(node->getGlobalRegisterNumber());
               }
            iter->second.second = TR::Node::copy(node);
            iter->second.second->setReferenceCount(0);
            }
         else if (node->getOpCode().isLoadConst() || node->getOpCodeValue() == TR::loadaddr)
            {
            // If node is constant load or loadaddr, do not waste a slot/register for that. Instead create new node.
            iter->second.second = TR::Node::copy(node);
            iter->second.second->setReferenceCount(0);
            }
         }
      /**
       * Step - 3 : Walk though treeTops after split point analyzing regStore/PassThrough nodes. 
       *    a. regStore Node :
       *       1. If it stores a node which is required to be uncommoned and the used register is available, 
       *          then create a replacement node for that node using the register and mark used register unavailable
       *       2. If node under regStore is not required to be uncommoned then mark the used register unavailable.
       *    b. PassThrough Node (Analysis of these kind of nodes are done in gatherUnavailableRegisters)
       *       1. If node under PassThrough node is stored into the same register after split point, we do not have to do anything. 
       *       2. If node is stored into same register before split point and if register is available, then use the information to store the node. 
       *             In other cases where register used in the PassThrough is not available, then create a Store into a register same as PassThrough before the tree containing the passThrough node.
       */
      List<TR::Node> storeRegNodePostSplitPoint(stackMemoryRegion);
      TR::TreeTop *nextExtendedBlockEntry = newBlock->getNextExtendedBlock() ? newBlock->getNextExtendedBlock()->getEntry() : NULL;
      for (TR::TreeTop *iter = newBlock->getEntry(); iter != nextExtendedBlockEntry; )
         {
         TR::TreeTop *nextTreeTop = iter->getNextTreeTop();
         TR::Node *node = iter->getNode();

         if (node->getOpCode().isStoreReg()) // Case - 1 : TreeTop Node is RegStore
            {
            TR::Node *storedNode = node->getFirstChild();
            auto nodeInfoEntry = nodeInfo->find(storedNode);
            // If storedNode is needed to be uncommoned and we do not have found replacement node yet and it is stored in register which is still available
            // then use the register to store the node.
            if (nodeInfoEntry != nodeInfo->end() && nodeInfoEntry->second.second == NULL && checkIfRegisterIsAvailable(comp, node, unavailableRegisters))
               {
               TR::Node *regLoad = TR::Node::create(storedNode, comp->il.opCodeForRegisterLoad(storedNode->getDataType()));
               regLoad->setRegLoadStoreSymbolReference(node->getRegLoadStoreSymbolReference());
               regLoad->setGlobalRegisterNumber(node->getGlobalRegisterNumber());
               unavailableRegisters.set(regLoad->getGlobalRegisterNumber());
               if (storedNode->requiresRegisterPair(comp))
                  {
                  regLoad->setHighGlobalRegisterNumber(node->getHighGlobalRegisterNumber());
                  unavailableRegisters.set(regLoad->getHighGlobalRegisterNumber());
                  }
               nodeInfoEntry->second.second = regLoad;
               // Move the regStore TreeTop before the split point. 
               iter->unlink(false);
               self()->getExit()->insertBefore(iter);
               }
            else
               {
               // Mark register unavailable so we do not use it.
               unavailableRegisters.set(node->getGlobalRegisterNumber());
               if (node->requiresRegisterPair(comp))
                  unavailableRegisters.set(node->getHighGlobalRegisterNumber());
               storeRegNodePostSplitPoint.add(node);
               }
            }
         else if (node->getOpCode().isBranch() && node->getNumChildren() > 0) // Case - 2 : TreeTop Node is Branch
            {
            // Checking the exit GlRegDeps For PassThrough
            TR::Node *regDeps = node->getChild(node->getNumChildren() - 1);
            if (regDeps->getOpCodeValue() == TR::GlRegDeps)
               {
               gatherUnavailableRegisters(comp, regDeps, iter, nodeInfo, storeNodeInfo, &storeRegNodePostSplitPoint, unavailableRegisters);
               }
            }
         else if (node->getOpCode().hasBranchChildren()) // Case - 3 : TreeTop Node has branch children
            {
            for (int32_t i = 0; i < node->getNumChildren(); ++i)
               {
               TR::Node *branch = node->getChild(i);
               if (branch->getOpCode().isBranch() && branch->getNumChildren() > 0)
                  {
                  TR::Node *regDeps = node->getChild(node->getNumChildren() - 1);
                  if (regDeps->getOpCodeValue() == TR::GlRegDeps)
                     {
                     gatherUnavailableRegisters(comp, regDeps, iter, nodeInfo, storeNodeInfo, &storeRegNodePostSplitPoint, unavailableRegisters);
                     }
                  }
               }
            }
         if (nextTreeTop == nextExtendedBlockEntry) // TreeTop Node is BBExit and next tree top is nextExtendedBlockEntry
            {
            TR::Node *regDeps = iter->getNode()->getNumChildren() > 0 ? iter->getNode()->getChild(0) : NULL;
            if (regDeps && regDeps->getOpCodeValue() == TR::GlRegDeps)
               {
               // If the previous tree top of BBEnd contains branch, we need to make sure any regStore we add are added before that.
               TR::TreeTop *prevTT = iter->getPrevTreeTop();
               if (prevTT->getNode()->getOpCode().isBranch() || prevTT->getNode()->getOpCode().hasBranchChildren())
                  iter = prevTT;
               gatherUnavailableRegisters(comp, regDeps, iter, nodeInfo, storeNodeInfo, &storeRegNodePostSplitPoint, unavailableRegisters);
               }
            }
         iter = nextTreeTop;
         }

      /**
       * Step - 4 : Create nodes for the entries remaining in the nodeInfo map:
       *         if we have ar register available:
       *             1) a PassThrough on the exit glregdeps with the register and the node and a RegStore
       *             2) a regload of the appropriate regiser on the entry glregdeps
       *          if no register is available
       *             1) a store to a temp at the end of the original block
       */

      int depCount = 0;
      List<TR::Node> exitDeps(stackMemoryRegion);
      List<TR::Node> entryDeps(stackMemoryRegion);
      for (auto iter = nodeInfo->begin(), end = nodeInfo->end(); iter != end; ++iter)
         {
         TR::Node *value = iter->first;
         // For all the nodes in the nodeTable for which we can not find replacement yet, create replacement node using available registers.
         if (!iter->second.second)
            {
            std::pair<TR_GlobalRegisterNumber,TR_GlobalRegisterNumber> regInfo = findAvailableRegister(comp, iter->first, unavailableRegisters);
            if (regInfo.first > -1)
               {
               TR::SymbolReference *ref = value->getOpCode().isLoadReg() ? value->getRegLoadStoreSymbolReference() : createSymRefForNode(comp, methodSymbol, value, self()->getExit());
               TR::Node *regLoad = TR::Node::create(value, comp->il.opCodeForRegisterLoad(value->getDataType()));
               TR::Node *regStore = TR::Node::create(value, comp->il.opCodeForRegisterStore(value->getDataType()), 1, value);
               self()->getExit()->insertBefore(TR::TreeTop::create(comp, regStore));
               regStore->setRegLoadStoreSymbolReference(ref);
               regLoad->setRegLoadStoreSymbolReference(ref);
               if (regInfo.second > -1)
                  {
                  regLoad->setLowGlobalRegisterNumber(regInfo.first);
                  regLoad->setHighGlobalRegisterNumber(regInfo.second);
                  regStore->setLowGlobalRegisterNumber(regInfo.first);
                  regStore->setHighGlobalRegisterNumber(regInfo.second);
                  }
               else
                  {
                  regLoad->setGlobalRegisterNumber(regInfo.first);
                  regStore->setGlobalRegisterNumber(regInfo.first);
                  }
               iter->second.second = regLoad;
               }
            // If we can not find a register, store it into temp slot.
            else
               {
               TR::DataType originalValDataType = value->getDataType();
               TR::DataType convertedValDataType = comp->fe()->dataTypeForLoadOrStore(originalValDataType);
               TR::Node *convertedValueNode  = value;
               // In case for a given node, if the data type of stack load/store is not same as node's data type,
               // then we need to convert the node to the type which can be stored onto the stack. 
               // Also we need to make sure that replacement node is a node that is converted back to original type of the node.
               if (convertedValDataType != originalValDataType)
                  {
                  TR::ILOpCodes convertOpCode = TR::ILOpCode::getProperConversion(originalValDataType, convertedValDataType, false);
                  convertedValueNode = TR::Node::create(convertOpCode, 1, value);
                  }
               TR::SymbolReference *storedValueSymRef = createSymRefForNode(comp, methodSymbol, convertedValueNode, self()->getExit());
               self()->getExit()->insertBefore(TR::TreeTop::create(comp,
                                                                     TR::Node::createWithSymRef(value,
                                                                        comp->il.opCodeForDirectStore(convertedValDataType),
                                                                        1,
                                                                        convertedValueNode,
                                                                        storedValueSymRef)));
               TR::Node *replacementNode = TR::Node::createLoad(value, storedValueSymRef);
               if (convertedValDataType != originalValDataType)
                  {
                  TR::ILOpCodes convertOpCode = TR::ILOpCode::getProperConversion(convertedValDataType, originalValDataType, false);
                  replacementNode = TR::Node::create(convertOpCode, 1, replacementNode);
                  }
               iter->second.second = replacementNode;
               }
            }
         
         // Now as we have created a node to replace all the references of value node after split point, add all nodes which are not stored into the temp slot to the list for entry and exit dependency 
         // which will be used to create GlRegDeps for exit node of the original block and entry node of new block.
         TR::Node *replacement = iter->second.second;
         if (replacement->getOpCode().isLoadReg())
            {
            depCount++;
            if (value->getOpCode().isLoadReg())
               {
               exitDeps.add(value);
               }
            else
               {
               TR::Node *passthrough = TR::Node::create(value, TR::PassThrough, 1, value);
               passthrough->setLowGlobalRegisterNumber(replacement->getLowGlobalRegisterNumber());
               passthrough->setHighGlobalRegisterNumber(replacement->getHighGlobalRegisterNumber());
               exitDeps.add(passthrough);
               }
            entryDeps.add(replacement);
            }
         }
      if (depCount > 0)
         {
         ListIterator<TR::Node> entryIter(&entryDeps);
         TR::Node *entryGlRegDeps = TR::Node::create(newBlock->getEntry()->getNode(), TR::GlRegDeps, depCount);
         int childIdx = 0;
         for (TR::Node *dep = entryIter.getCurrent(); dep; dep=entryIter.getNext())
            {
            entryGlRegDeps->setAndIncChild(childIdx++, dep);
            }

         ListIterator<TR::Node> exitIter(&exitDeps);
         TR::Node *exitGlRegDeps = TR::Node::create(self()->getExit()->getNode(), TR::GlRegDeps, depCount);
         childIdx = 0;
         for (TR::Node *dep = exitIter.getCurrent(); dep; dep=exitIter.getNext())
            {
            exitGlRegDeps->setAndIncChild(childIdx++, dep);
            }
   
         newBlock->getEntry()->getNode()->addChildren(&entryGlRegDeps, 1);
         self()->getExit()->getNode()->addChildren(&exitGlRegDeps, 1);
         }

      // Step - 5  : Now replace all the occurrences of nodes to be uncommoned with replacement nodes.
      replaceNodesInTrees(comp, newBlock->getEntry()->getNextTreeTop(), nodeInfo);
      }
   return newBlock;
   }



TR::Block *
OMR::Block::split(TR::TreeTop * startOfNewBlock, TR::CFG * cfg, bool fixupCommoning, bool copyExceptionSuccessors, TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR_Structure * structure = cfg->getStructure();
   cfg->setStructure(NULL);
   TR::Compilation * comp = cfg->comp();
   comp->setCurrentBlock(self());
   TR::Node * startNode = startOfNewBlock->getNode();
   TR::Block * block2 = new (comp->trHeapMemory()) TR::Block(TR::TreeTop::create(comp, TR::Node::create(startNode, TR::BBStart)), self()->getExit(), comp->trMemory());
   block2->inheritBlockInfo(self());

   cfg->addNode(block2);


   self()->setExit(TR::TreeTop::create(comp, startOfNewBlock->getPrevTreeTop(), TR::Node::create(startNode, TR::BBEnd)));
   self()->getExit()->join(block2->getEntry());
   self()->getExit()->getNode()->setBlock(self());

   block2->getEntry()->join(startOfNewBlock);

   if (fixupCommoning)
      {
      self()->uncommonNodesBetweenBlocks(comp, block2, methodSymbol);
      }

   self()->moveSuccessors(block2);
   cfg->addEdge(self(), block2);
   if (copyExceptionSuccessors)
      cfg->copyExceptionSuccessors(self(), block2);

   if (structure)
      {
      TR_BlockStructure *thisBlockStructure = self()->getStructureOf();
      if (thisBlockStructure)
         {
         TR_BlockStructure *blockStructure2 = new (cfg->structureRegion()) TR_BlockStructure(comp, block2->getNumber(), block2);
         TR_RegionStructure *parentStructure = thisBlockStructure->getParent()->asRegion();
         TR_StructureSubGraphNode *blockStructureNode2 = new (cfg->structureRegion()) TR_StructureSubGraphNode(blockStructure2);
         TR_StructureSubGraphNode *subNode;
         TR_RegionStructure::Cursor si(*parentStructure);
         for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
            {
            if (subNode->getStructure() == thisBlockStructure)
               break;
            }

         parentStructure->addSubNode(blockStructureNode2);

         for (auto nextSucc = subNode->getSuccessors().begin(); nextSucc != subNode->getSuccessors().end(); ++nextSucc)
          (*nextSucc)->setFrom(blockStructureNode2);

         subNode->getSuccessors().clear();
         TR::CFGEdge::createEdge(subNode,  blockStructureNode2, comp->trMemory());

         for (auto nextSucc = subNode->getExceptionSuccessors().begin(); nextSucc != subNode->getExceptionSuccessors().end(); ++nextSucc)
          {
          // Sometimes the catch block is not added to the exc successors
          // by the call to copyExceptionSuccessors above; this can happen if
          // a catch block has the same range as an earlier catch block and is
          // (its catch type) subsumed by the earlier catch block. We should not
          // add the structure edge either in this rare case; exposed by a JCK test
          //
          int32_t nextSuccNum = (*nextSucc)->getTo()->getNumber();
          bool excSuccWasAdded = false;
          for (auto e1 = block2->getExceptionSuccessors().begin(); e1 != block2->getExceptionSuccessors().end(); ++e1)
             {
             if (nextSuccNum == (*e1)->getTo()->getNumber())
               {
               excSuccWasAdded = true;
               break;
               }
             }

          if (excSuccWasAdded)
             {
             if (toStructureSubGraphNode((*nextSucc)->getTo())->getStructure())
                 TR::CFGEdge::createExceptionEdge(blockStructureNode2, (*nextSucc)->getTo(), comp->trMemory());
             else
                parentStructure->addExitEdge(blockStructureNode2, (*nextSucc)->getTo()->getNumber(), true);
             }
          }
         }
      }

   cfg->setStructure(structure);

   return block2;
   }


TR::Block *
OMR::Block::splitBlockAndAddConditional(TR::TreeTop *tree,
                                      TR::TreeTop *compareTree,
                                      TR::CFG *cfg,
                                      bool remainderBlockShouldExtend)
   {
   cfg->setStructure(0);

   TR::Block * remainderBlock = self()->split(tree, cfg, !remainderBlockShouldExtend);
   if (remainderBlockShouldExtend)
      remainderBlock->setIsExtensionOfPreviousBlock();

   self()->append(compareTree);

   // remove the original tree from the remainderBlock
   //
   TR::Node * node = tree->getNode();
   node->removeAllChildren();
   tree->getPrevTreeTop()->join(tree->getNextTreeTop());

   return remainderBlock;
   }


/**
 * createConditionalSideExitBeforeTree converts from
 *    start block
 *      ...
 *      <tree>
 *      ...
 *    end block
 *
 *    to:
 *
 *    start block
 *      ...
 *     if <cond> goto block C (compareTree)
 *    end block
 *    new block B start (remainder)
 *      ... (stuff in original block after <tree>)
 *    new block B end (remainder)
 *
 *    <after next block that cannot fallthrough or at end of cfg>
 *    new block C start (if)
 *      <exitTree>
 *      <returnTree>
 *    new block C end (if)
 *
 * @param this         the block of the original tree to be split up
 * @param tree         the tree identifying the split point. This tree will be removed from its block, compareTree will
 *                     now end this block.
 * @param  compareTree the compare tree to use in constructing the conditional blocks and cfg
 * @param  exitTree    the tree to use on the 'if' branch when constructing the blocks and cfg
 * @param  returnTree  the tree appended to the exitBlock to return
 *                     NOTE: branch destination in node under returnTree MUST be set
 * @param  cfg         the cfg to update with the new information
 * @param  markCold    if the side exit should be marked as cold otherwise inherits frequence from block
 */
TR::Block *
OMR::Block::createConditionalSideExitBeforeTree(TR::TreeTop *tree,
                                              TR::TreeTop *compareTree,
                                              TR::TreeTop *exitTree,
                                              TR::TreeTop *returnTree,
                                              TR::CFG *cfg,
                                              bool markCold)

   {
   auto comp = TR::comp();

   TR::Block *remainderBlock = self()->splitBlockAndAddConditional(tree, compareTree, cfg, true);

   // append the 'exit' block to the end of the method
   // hook the exit tree on to the exit block and insert the return true
   //
   TR::Node *node = tree->getNode();
   TR::Block * exitBlock = TR::Block::createEmptyBlock(node, comp, 0, self());
   cfg->addNode(exitBlock);

   TR::Block *cursorBlock = remainderBlock;
   while (cursorBlock && cursorBlock->canFallThroughToNextBlock())
      cursorBlock = cursorBlock->getNextBlock();

   if (cursorBlock)
      {
      TR::TreeTop *cursorTree = cursorBlock->getExit();
      TR::TreeTop *nextTree = cursorTree->getNextTreeTop();
      cursorTree->join(exitBlock->getEntry());
      exitBlock->getExit()->join(nextTree);
      }
   else
      cfg->findLastTreeTop()->join(exitBlock->getEntry());

   if (markCold)
      {
      exitBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);
      exitBlock->setIsCold();
      }
   else
      {
      exitBlock->setFrequency(remainderBlock->getFrequency());
      }

   exitBlock->append(exitTree);
   exitBlock->append(returnTree);
   compareTree->getNode()->setBranchDestination(exitBlock->getEntry());

   cfg->addEdge(TR::CFGEdge::createEdge(self(),  exitBlock, comp->trMemory()));

   TR::CFGNode *afterExitBlock = cfg->getEnd(); // typically is a return which goes to EXIT
   if (returnTree->getNode()->getOpCode().isBranch())
      afterExitBlock = returnTree->getNode()->getBranchDestination()->getNode()->getBlock();
   cfg->addEdge(TR::CFGEdge::createEdge(exitBlock,  afterExitBlock, comp->trMemory()));

   cfg->copyExceptionSuccessors(self(), exitBlock);

   return remainderBlock;
   }


/**
 * Converts from
 *
 *     start block
 *        ...
 *        <tree>
 *        ...
 *     end block
 *
 *    to:
 *
 *     start block
 *        ...
 *       if <cond> goto block C (rare)
 *     end block
 *     new block B start (else optional)
 *       <elseTree>
 *     new block B end (else optional)
 *     new block D start (remainder)
 *        ... (stuff in original block after <tree>)
 *     new block D end (remainder)
 *
 *     <at end of cfg>
 *     new block C start (if)
 *        <ifTree>
 *        goto block D
 *     new block C end (if)
 *
 *
 *   \param this          the block of the original tree to be split up
 *   \param tree          the tree identifying the split point. This tree will
 *                        be removed from its block, compareTree will now end this block.
 *   \param compareTree   the compare tree to use in constructing the conditional blocks and cfg
 *   \param ifTree        the tree to use on the 'if' branch when constructing the blocks and cfg
 *   \param elseTree      the tree to use on the 'else' branch when constructing the blocks and cfg
 *   \param cfg           the cfg to update with the new information
 */
TR::Block *
OMR::Block::createConditionalBlocksBeforeTree(TR::TreeTop * tree,
                                            TR::TreeTop * compareTree,
                                            TR::TreeTop * ifTree,
                                            TR::TreeTop * elseTree,
                                            TR::CFG * cfg,
                                            bool changeBlockExtensions,
                                            bool markCold)
   {
   auto comp = TR::comp();

   cfg->setStructure(0);

   TR::Block * remainderBlock = self()->split(tree, cfg, true);
   if (changeBlockExtensions)
      remainderBlock->setIsExtensionOfPreviousBlock(false); // not an extension since has 2 predecessors

   self()->append(compareTree);

   // remove the original tree from the remainderBlock
   TR::Node * node = tree->getNode();
   node->removeAllChildren();
   tree->getPrevTreeTop()->join(tree->getNextTreeTop());

   // append the 'if' block to the end of the method
   // hook the if tree on to the if block and add a goto at the end
   // to return to the mainline
   //
   TR::Block * ifBlock = TR::Block::createEmptyBlock(node, comp, 0, self());
   if (markCold)
      {
      ifBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);
      ifBlock->setIsCold();
      }
   else
      {
      ifBlock->setFrequency(remainderBlock->getFrequency());
      }
   cfg->addNode(ifBlock);

   TR::Block *cursorBlock = remainderBlock;
   while (cursorBlock && cursorBlock->canFallThroughToNextBlock())
      {
      cursorBlock = cursorBlock->getNextBlock();
      }

   if (cursorBlock)
      {
      TR::TreeTop *cursorTree = cursorBlock->getExit();
      TR::TreeTop *nextTree = cursorTree->getNextTreeTop();
      cursorTree->join(ifBlock->getEntry());
      ifBlock->getExit()->join(nextTree);
      }
   else
      cfg->findLastTreeTop()->join(ifBlock->getEntry());


   ifBlock->append(ifTree);
   ifBlock->append(TR::TreeTop::create(comp, TR::Node::create(node, TR::Goto, 0, remainderBlock->getEntry())));
   compareTree->getNode()->setBranchDestination(ifBlock->getEntry());

   cfg->addEdge(TR::CFGEdge::createEdge(self(),  ifBlock, comp->trMemory()));
   cfg->addEdge(TR::CFGEdge::createEdge(ifBlock,    remainderBlock, comp->trMemory()));

   cfg->copyExceptionSuccessors(self(), ifBlock);

   // create link in the else tree in the else block
   if (elseTree)
      {
      TR::Block * elseBlock = TR::Block::createEmptyBlock(node, comp, self()->getFrequency(), self());

      elseBlock->append(elseTree);

      self()->getExit()->join(elseBlock->getEntry());
      elseBlock->getExit()->join(remainderBlock->getEntry());

      if (changeBlockExtensions)
         elseBlock->setIsExtensionOfPreviousBlock(true);       // fall-through block is an extension

      cfg->addNode(elseBlock);
      cfg->addEdge(TR::CFGEdge::createEdge(self(),  elseBlock, comp->trMemory()));
      cfg->addEdge(TR::CFGEdge::createEdge(elseBlock,  remainderBlock, comp->trMemory()));
      cfg->copyExceptionSuccessors(self(), elseBlock);
      cfg->removeEdge(self(), remainderBlock);
      }

   return remainderBlock;
   }

OMR::Block::StandardException OMR::Block::_standardExceptions[] =
   {
#define MIN_EXCEPTION_NAME_SIZE 5
   { 5, "Error", CanCatchNew | CanCatchArrayNew },
   { 9, "Exception", CanCatchNullCheck | CanCatchDivCheck | CanCatchBoundCheck |
                     CanCatchArrayStoreCheck | CanCatchArrayNew |
                     CanCatchCheckCast | CanCatchMonitorExit },
   { 9, "Throwable", CanCatchEverything },
   {12, "UnknownError", CanCatchNew | CanCatchArrayNew },
   {13, "InternalError", CanCatchNew | CanCatchArrayNew },
   {16, "OutOfMemoryError", CanCatchNew | CanCatchArrayNew },
   {16, "RuntimeException", CanCatchNullCheck | CanCatchDivCheck |
                            CanCatchBoundCheck | CanCatchArrayStoreCheck |
                            CanCatchArrayNew | CanCatchCheckCast |
                            CanCatchMonitorExit },
   {18, "ClassCastException", CanCatchCheckCast },
   {18, "IllegalAccessError", CanCatchArrayNew },
   {18, "InstantiationError", CanCatchNew },
   {18, "StackOverflowError", CanCatchNew | CanCatchArrayNew },
   {19, "ArithmeticException", CanCatchDivCheck },
   {19, "ArrayStoreException", CanCatchArrayStoreCheck },
   {19, "VirtualMachineError", CanCatchNew | CanCatchArrayNew },
   {20, "NullPointerException", CanCatchNullCheck },
   {25, "IndexOutOfBoundsException", CanCatchBoundCheck },
   {26, "NegativeArraySizeException", CanCatchArrayNew },
   {28, "IllegalMonitorStateException", CanCatchMonitorExit },
   {28, "IncompatibleClassChangeError", CanCatchNew | CanCatchArrayNew },
   {30, "ArrayIndexOutOfBoundsException", CanCatchBoundCheck },
#define MAX_EXCEPTION_NAME_SIZE 30
   {99, "", 0 }
   };

static TR::Node *
findFirstReference(TR::Node * n, TR::Symbol * sym, vcount_t visitCount)
   {
   if (n->getVisitCount() == visitCount)
      return 0;
   n->setVisitCount(visitCount);

   TR::Node * firstReference = 0;
   for (int32_t i = 0; i < n->getNumChildren(); ++i)
      if ((firstReference = findFirstReference(n->getChild(i), sym, visitCount)))
         return firstReference;

   if (n->getOpCode().hasSymbolReference() && n->getSymbol() == sym)
      return n;

   return 0;
   }

TR::Node *
OMR::Block::findFirstReference(TR::Symbol * sym, vcount_t visitCount)
   {
   TR::Node * node = 0;
   for (TR::TreeTop * tt = self()->getFirstRealTreeTop(); tt != self()->getExit(); tt = tt->getNextTreeTop())
      if ((node = ::findFirstReference(tt->getNode(), sym, visitCount)))
         break;
   return node;
   }


void
OMR::Block::collectReferencedAutoSymRefsIn(TR::Compilation *comp, TR_BitVector *referencedAutoSymRefs, vcount_t visitCount)
   {
   if (!self()->getEntry() || !self()->getExit())
      return;

   for (TR::TreeTop * tt = self()->getFirstRealTreeTop(); tt != self()->getExit(); tt = tt->getNextTreeTop())
     self()->collectReferencedAutoSymRefsIn(comp, tt->getNode(), referencedAutoSymRefs, visitCount);
   }

void
OMR::Block::collectReferencedAutoSymRefsIn(TR::Compilation *comp, TR::Node *node, TR_BitVector *referencedAutoSymRefs, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isAutoOrParm())
      {
      TR::SymbolReference* symRef = node->getSymbolReference();
      if (symRef->getSymbol()->isAutoOrParm() || symRef->getSymbol()->isMethodMetaData())
         referencedAutoSymRefs->set(symRef->getReferenceNumber());
      else if (comp->getOptLevel() > warm)
         {
         TR::SparseBitVector indirectMethodMetaUses(comp->allocator());
         symRef->getUseDefAliases(node->getOpCode().isCallDirect()).getAliases(indirectMethodMetaUses);
         if (!indirectMethodMetaUses.IsZero())
            {
            TR::SparseBitVector::Cursor aliasesCursor(indirectMethodMetaUses);
            for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
               {
               int32_t usedSymbolIndex;
               TR::SymbolReference *usedSymRef = comp->getSymRefTab()->getSymRef(aliasesCursor);
               TR::RegisterMappedSymbol *usedSymbol = usedSymRef->getSymbol()->getMethodMetaDataSymbol();
               if (usedSymbol && usedSymbol->getDataType() != TR::NoType)
                  {
                  usedSymbolIndex = usedSymbol->getLiveLocalIndex();
                  referencedAutoSymRefs->set(usedSymbolIndex);
                  }
               }
            }
         }
      }


   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      self()->collectReferencedAutoSymRefsIn(comp, node->getChild(i), referencedAutoSymRefs, visitCount);
   }

TR::Block *
OMR::Block::splitEdge(TR::Block *from, TR::Block *to, TR::Compilation *c, TR::TreeTop **lastTreeTop, bool findOptimalInsertionPoint)
   {

   //traceMsg("Splitting edge (%d,%d)\n", from->getNumber(), to->getNumber());
   TR_ASSERT(!to->isOSRCatchBlock(), "Splitting edge to OSRCatchBlock (block_%d -> block_%d) is not supported\n", from->getNumber(), to->getNumber());
   TR::Node *exitNode = from->getExit()->getNode();
   TR::CFG *cfg = c->getFlowGraph();
   TR_Structure *rootStructure = cfg->getStructure();
   TR_Structure *fromContainingLoop = NULL;
   if (rootStructure && from->getStructureOf())
      fromContainingLoop = from->getStructureOf()->getContainingLoop();
   TR_Structure *toContainingLoop = NULL;
   if (rootStructure && to->getStructureOf())
      toContainingLoop = to->getStructureOf()->getContainingLoop();
   if (fromContainingLoop != toContainingLoop)
      {
      while (fromContainingLoop)
         {
         if (fromContainingLoop == toContainingLoop)
            {
            exitNode = to->getEntry()->getNode();
            break;
            }
         fromContainingLoop = fromContainingLoop->getContainingLoop();
         }
      }

   TR::Compilation *comp = cfg->comp();
   TR::TreeTop *entryTree = to->getEntry();
   TR::Block * newBlock;
   if (entryTree)
      {
      const bool isExceptionEdge = to->isCatchBlock();
      int32_t freq = from->getEdge(to)->getFrequency();
      newBlock = TR::Block::createEmptyBlock(exitNode, c, freq, from);

      if (from->isCold() || to->isCold())
         {
         newBlock->setFrequency(TR::Block::getMinColdFrequency(from, to));
         newBlock->setIsCold();
         }

      cfg->addNode(newBlock, from->getCommonParentStructureIfExists(to, cfg));
      from->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp, to->getEntry(), newBlock->getEntry());

      // inserting newBlock between insertionExit and insertionEntry treetops
      // by default insertionExit is NULL
      TR::TreeTop *insertionExit = NULL;
      TR::Block *prevBlock = to->getPrevBlock();
      if (prevBlock == from)
         {
         insertionExit = entryTree->getPrevTreeTop();
         }
      else if (findOptimalInsertionPoint && !from->isCold() && !to->isCold())
         {
         while (prevBlock && prevBlock->canFallThroughToNextBlock())
            prevBlock = prevBlock->getPrevBlock();
         if (prevBlock)
            insertionExit = prevBlock->getExit();
         else
            {
            TR::Block *nextBlock = from->getPrevBlock();
            while (nextBlock && nextBlock->canFallThroughToNextBlock())
               nextBlock = nextBlock->getPrevBlock();
            if (nextBlock)
               insertionExit = nextBlock->getExit();
            else
               {
               prevBlock = to->getNextBlock();
               while (prevBlock && prevBlock->canFallThroughToNextBlock())
                  prevBlock = prevBlock->getNextBlock();
               if (prevBlock)
                  insertionExit = prevBlock->getExit();
               else
                  {
                  nextBlock = from->getNextBlock();
                  while (nextBlock && nextBlock->canFallThroughToNextBlock())
                     nextBlock = nextBlock->getNextBlock();
                  if (nextBlock)
                     insertionExit = nextBlock->getExit();
                  }
               }
            }
         }

      if (!insertionExit)
         insertionExit = c->getMethodSymbol()->getLastTreeTop();
      TR::TreeTop *insertionEntry = insertionExit->getNextTreeTop();

      insertionExit->join(newBlock->getEntry());
      newBlock->getExit()->join(insertionEntry);

      if (isExceptionEdge)
         {
         newBlock->setHandlerInfo(
            to->getCatchType(),
            to->getInlineDepth(),
            to->getHandlerIndex(),
            to->getOwningMethod(),
            c);
         TR::ResolvedMethodSymbol *method = c->getMethodSymbol();
         TR::SymbolReferenceTable *srtab = c->getSymRefTab();
         TR::SymbolReference *excSR = srtab->findOrCreateExcpSymbolRef();
         TR::SymbolReference *throwSR = srtab->findOrCreateAThrowSymbolRef(method);
         TR::Node *exc = TR::Node::createLoad(excSR);
         TR::Node *rethrow = TR::Node::createWithSymRef(TR::athrow, 1, 1, exc, throwSR);
         newBlock->append(TR::TreeTop::create(c, rethrow));
         }
      else if (entryTree != insertionEntry)
         {
         TR::TreeTop *gotoTreeTop = TR::TreeTop::create(c, TR::Node::create(from->getExit()->getNode(), TR::Goto, 0, to->getEntry()));
         newBlock->append(gotoTreeTop);
         if (lastTreeTop)
            *lastTreeTop = newBlock->getExit();
         }
      else
         {
         if (to->isExtensionOfPreviousBlock())
            newBlock->setIsExtensionOfPreviousBlock();
         }

      if (isExceptionEdge)
         {
         cfg->addExceptionEdge(newBlock, to);
         cfg->removeEdge(from, to);
         cfg->addExceptionEdge(from, newBlock);
         cfg->addEdge(newBlock, cfg->getEnd());
         }
      else
         {
         cfg->addEdge(from, newBlock);
         cfg->addEdge(newBlock, to);
         cfg->removeEdge(from, to);
         }
      }
   // if entryTree is NULL, then to is exit and from better end in a return or a throw
   // splitting the block at the point of the return or throw is the safest way to do this
   else
      {
      TR::TreeTop *lastTree = from->getLastRealTreeTop();
      TR::ILOpCode &opCode = lastTree->getNode()->getOpCodeValue() == TR::treetop || lastTree->getNode()->getOpCode().isCheck() ? lastTree->getNode()->getFirstChild()->getOpCode() : lastTree->getNode()->getOpCode();
      TR_ASSERT(opCode.getOpCodeValue() == TR::athrow || opCode.isReturn(), "edge to EXIT should end in throw or return");

      newBlock = self()->split(lastTree, cfg, true);
      }

   return newBlock;
   }

void
OMR::Block::takeGlRegDeps(TR::Compilation *comp, TR::Node *glRegDeps)
   {
   if (!glRegDeps)
      return;

   TR::Node *duplicateGlRegDeps = glRegDeps->duplicateTree();
   self()->getEntry()->getNode()->setNumChildren(1);
   self()->getEntry()->getNode()->setAndIncChild(0, duplicateGlRegDeps);
   TR::Node *origDuplicateGlRegDeps = duplicateGlRegDeps;
   duplicateGlRegDeps = TR::Node::copy(duplicateGlRegDeps);
   for (int32_t i = origDuplicateGlRegDeps->getNumChildren() - 1; i >= 0; --i)
      {
      TR::Node * dep = origDuplicateGlRegDeps->getChild(i);
      duplicateGlRegDeps->setAndIncChild(i, dep);
      }
   self()->getExit()->getNode()->setNumChildren(1);
   self()->getExit()->getNode()->setChild(0, duplicateGlRegDeps);
   }

TR::TreeTop *
OMR::Block::getExceptingTree()
   {
   TR_ASSERT(self()->getExceptionSuccessors().size() <= 1,"routine only works when block has 0 or 1 (and not %d) exception edges\n",self()->getExceptionSuccessors().size());
   for (TR::TreeTop *tt = self()->getEntry(); tt != self()->getExit(); tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->exceptionsRaised())
         return tt;
      }
   return NULL;
   }

const char *
OMR::Block::getName(TR_Debug * debug)
   {
   if (debug)
      return debug->getName(self());
   else
      return "<unknown block>";
   }

void
OMR::Block::inheritBlockInfo(TR::Block * org, bool inheritFreq)
   {
   self()->setIsCold(org->isCold());
   self()->setIsSuperCold(org->isSuperCold());
   if (inheritFreq)
      self()->setFrequency(org->getFrequency());

   self()->setIsSpecialized(org->isSpecialized());
   }

TR::Block *
OMR::Block::startOfExtendedBlock()
   {
   return self()->isExtensionOfPreviousBlock() ?
      self()->getEntry()->getPrevTreeTop()->getNode()->getBlock()->startOfExtendedBlock()
      : self();
   }


bool
OMR::Block::canCatchExceptions(uint32_t flags)
   {
   return _catchBlockExtension
      && ((flags & _catchBlockExtension->_exceptionsCaught) != 0);
   }

void
OMR::Block::setByteCodeIndex(int32_t index, TR::Compilation *comp)
   {
   self()->getEntry()->getNode()->setByteCodeIndex(index);
   }

bool
OMR::Block::endsInGoto()
   {
   TR::TreeTop *lastTT=NULL;
   if (self()->getEntry() != NULL)
      lastTT = self()->getLastRealTreeTop();
   return (lastTT != NULL
           && lastTT->getNode()->getOpCodeValue() == TR::Goto);
   }

bool
OMR::Block::endsInBranch()
   {
   TR::TreeTop * lastTT = NULL;
   TR::Node *    lastNode = NULL;
   if (self()->getEntry() != NULL)
      {
      lastTT = self()->getLastRealTreeTop();
      lastNode = lastTT->getNode();
      }
   return (lastTT != NULL
           && lastNode->getOpCode().isBranch()
           && lastNode->getOpCodeValue() != TR::Goto);
   }

bool
OMR::Block::isEmptyBlock()
   {

   return (self()->getEntry() != NULL && self()->getEntry()->getNextTreeTop() == self()->getExit());
   }


void
OMR::Block::ensureCatchBlockExtensionExists(TR::Compilation *comp)
   {
   if (_catchBlockExtension == NULL)
      {
      _catchBlockExtension = new (comp->trHeapMemory()) TR_CatchBlockExtension();
      }
   }


bool
OMR::Block::hasExceptionPredecessors()
   {
   return !self()->getExceptionPredecessors().empty();
   }



/**
 * Field functions
 */

int32_t
OMR::Block::getNestingDepth()
   {
   if (_pStructureOf)
      return _pStructureOf->getNestingDepth();
   return -1;
   }

TR_Array<TR_GlobalRegister> &
OMR::Block::getGlobalRegisters(TR::Compilation *c)
   {
   if (!_globalRegisters)
      _globalRegisters = new (c->trStackMemory()) TR_Array<TR_GlobalRegister>(c->trMemory(), c->cg()->getNumberOfGlobalRegisters(), true, stackAlloc);
   return *_globalRegisters;
   }

void
OMR::Block::setInstructionBoundaries(uint32_t startPC, uint32_t endPC)
   {
   _instructionBoundaries._startPC = startPC;
   _instructionBoundaries._endPC = endPC;
   }

void
OMR::Block::addExceptionRangeForSnippet(uint32_t startPC, uint32_t endPC)
   {
   _snippetBoundaries.add(new (TR::comp()->trHeapMemory()) InstructionBoundaries(startPC, endPC));
   }

void
OMR::Block::setHandlerInfo(uint32_t c, uint8_t d, uint16_t i, TR_ResolvedMethod * m, TR::Compilation *comp)
   {
   self()->ensureCatchBlockExtensionExists(comp);
   TR_CatchBlockExtension *cbe = _catchBlockExtension;
   cbe->_catchType = c;
   cbe->_inlineDepth = d;
   cbe->_handlerIndex = i;
   cbe->_exceptionsCaught = CanCatchEverything;
   cbe->_owningMethod = m;
   cbe->_byteCodeInfo = _pEntry->getNode()->getByteCodeInfo();

   if (c != 0)
      {
      uint32_t length;
      char * name = m->getClassNameFromConstantPool(c, length);
      self()->setExceptionClassName(name, length, comp);
      }
   }

void
OMR::Block::setHandlerInfoWithOutBCInfo(uint32_t c, uint8_t d, uint16_t i, TR_ResolvedMethod * m, TR::Compilation *comp)
   {
   self()->ensureCatchBlockExtensionExists(comp);
   TR_CatchBlockExtension *cbe = _catchBlockExtension;
   cbe->_catchType = c;
   cbe->_inlineDepth = d;
   cbe->_handlerIndex = i;
   if (c == CanCatchOSR)
     cbe->_exceptionsCaught = CanCatchEverything;
   cbe->_owningMethod = m;
   }

bool
OMR::Block::isCatchBlock()
   {
   return !self()->getExceptionPredecessors().empty();
   }

uint32_t
OMR::Block::getCatchType()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_catchType;
   }

uint8_t
OMR::Block::getInlineDepth()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_inlineDepth;
   }

uint16_t
OMR::Block::getHandlerIndex()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_handlerIndex;
   }

TR_ByteCodeInfo
OMR::Block::getByteCodeInfo()
   {
   TR_ASSERT(_catchBlockExtension, "Catch block extension does not exist");
   return _catchBlockExtension->_byteCodeInfo;
   }

TR_OpaqueClassBlock *
OMR::Block::getExceptionClass()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_exceptionClass;
   }

char *
OMR::Block::getExceptionClassNameChars()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_exceptionClassNameChars;
   }

int32_t
OMR::Block::getExceptionClassNameLength()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_exceptionClassNameLength;
   }

void
OMR::Block::setExceptionClassName(char *name, int32_t length, TR::Compilation *comp)
   {
   self()->ensureCatchBlockExtensionExists(comp);
   _catchBlockExtension->_exceptionClassNameChars  = name;
   _catchBlockExtension->_exceptionClassNameLength = length;

   if (name == NULL)
      {
      _catchBlockExtension->_exceptionsCaught = CanCatchEverything;
      _catchBlockExtension->_exceptionClass = 0;
      return;
      }

   _catchBlockExtension->_exceptionClass =
      self()->getOwningMethod()->fe()->getClassFromSignature(name, length, self()->getOwningMethod());

   // Set up flags for which standard exceptions can be caught by this block
   //
   _catchBlockExtension->_exceptionsCaught = CanCatchUserThrows | CanCatchResolveCheck;

   if (length < 10+MIN_EXCEPTION_NAME_SIZE ||
       length > 10+MAX_EXCEPTION_NAME_SIZE ||
       strncmp(name, "java/lang/", 10))
      return;

   name   += 10;
   length -= 10;

   for (int32_t i = 0; ; ++i)
      {
      StandardException &excp = _standardExceptions[i];
      if (excp.length > length)
         break;
      if (excp.length == length && !strncmp(name, excp.name, length))
         {
         _catchBlockExtension->_exceptionsCaught |= excp.exceptions;
         break;
         }
      }
   }

TR_ResolvedMethod *
OMR::Block::getOwningMethod()
   {
   TR_ASSERT(_catchBlockExtension, "catch block extension does not exist");
   return _catchBlockExtension->_owningMethod;
   }

int32_t
OMR::Block::getNormalizedFrequency(TR::CFG * cfg)
   {
   if (self()->getFrequency() < 0)
      return (MAX_WARM_BLOCK_COUNT * 100) / (MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT);
   return (self()->getFrequency() * 100) / (MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT);
   }

int32_t
OMR::Block::getGlobalNormalizedFrequency(TR::CFG * cfg)
   {
   int32_t frequency = self()->getNormalizedFrequency(cfg);

   TR_Hotness hotness = cfg->comp()->getMethodHotness();
   if (hotness >= scorching)
      return frequency * 100;
   if (hotness >= hot)
      return frequency * 10;
   return frequency;
   }

/*
 * An OSR induce block is used in voluntary OSR to contain the OSR point's dead stores
 * and to induce the transition. It will always have a single edge to the exit and at least one
 * exception successor to an OSR catch block.
 *
 * This function verifies that any blocks marked as an OSRInduceBlock hold these properties.
 */
bool
OMR::Block::verifyOSRInduceBlock(TR::Compilation *comp)
   {
   // This check only applies when in voluntary OSR
   if (comp->getOSRMode() != TR::voluntaryOSR)
      return true;

   // OSR induce blocks should contain an induce OSR call
   TR::TreeTop *cursor = self()->getExit();
   bool foundOSRInduceCall = false;
   while (cursor && cursor->getNode()->getOpCodeValue() != TR::BBStart)
      {
      TR::Node *node = cursor->getNode();
      if (node->getOpCodeValue() == TR::treetop && node->getFirstChild()->getOpCode().hasSymbolReference()
          && node->getFirstChild()->getSymbolReference()->isOSRInductionHelper())
         {
         foundOSRInduceCall = true;
         break;
         }
      cursor = cursor->getPrevTreeTop();
      }
   if (self()->isOSRInduceBlock() != foundOSRInduceCall)
      return false;

   if (!self()->isOSRInduceBlock())
      return true;

   // OSR induce blocks should have a single successor to the exit
   if (self()->getSuccessors().size() != 1
       || self()->getSuccessors().front()->getTo() != comp->getFlowGraph()->getEnd())
      return false;

   // OSR induce blocks should have at least one exception edge to an OSR catch block
   auto e = self()->getExceptionSuccessors().begin();
   for (; e != self()->getExceptionSuccessors().end(); ++e)
      if ((*e)->getTo()->asBlock()->isOSRCatchBlock())
         break;
   return e != self()->getExceptionSuccessors().end();
   }


/**
 * Field functions end
 */

/**
 * Flag functions
 */

void
OMR::Block::setIsExtensionOfPreviousBlock(bool b)
   {
   _flags.set(_isExtensionOfPreviousBlock, b);
   if (!b && TR::comp()->getOptimizer())
      {
      TR::comp()->getOptimizer()->setCachedExtendedBBInfoValid(false);
      }
   }

bool
OMR::Block::isExtensionOfPreviousBlock()
   {
   return _flags.testAny(_isExtensionOfPreviousBlock);
   }

void
OMR::Block::setIsSuperCold(bool b)
   {
   _flags.set(_isSuperCold, b);
   if (b)
      {
      self()->setIsCold();
      self()->setFrequency(0);
      }
   }

bool
OMR::Block::isSuperCold()
   {
   return _flags.testAny(_isSuperCold);
   }

void
OMR::Block::setIsSyntheticHandler()
   {
   TR_ASSERT_FATAL(_catchBlockExtension, "can't call setIsSyntheticHandler without _catchBlockExtension");
   _catchBlockExtension->_isSyntheticHandler = true;
   }

/**
 * Flag functions end
 */

TR::Block *
TR_BlockCloner::cloneBlocks(TR::Block * from, TR::Block * lastBlock)
   {
   TR_LinkHeadAndTail<BlockMapper> bMap;
   bMap.set(0, 0);

   // The block mappings (including the BBStart treetops) have to be created
   // before any branch nodes are cloned so that mappings can be used to fixup
   // the destination of the cloned branch.
   // However, the nodes must all be created at the same time so that nodes can
   // be shared properly.
   // To do this, trees are created in two passes. In the first pass the cloned
   // blocks are created along with the BBStart and BBEnd treetops (but not the
   // nodes). In the second pass the BBStart and BBEnd nodes are created, and
   // the treetops and nodes for the rest of the block.
   //
   TR::Compilation *comp = _cfg->comp();
   for (; from; from = from->getNextBlock())
      {
      comp->setCurrentBlock(from);
      TR::Block * to =
         new (comp->trHeapMemory()) TR::Block(*from, TR::TreeTop::create(comp, NULL), TR::TreeTop::create(comp, NULL));
      to->getEntry()->join(to->getExit());

      if (bMap.getLast())
         bMap.getLast()->_to->getExit()->join(to->getEntry());
      bMap.append(new (comp->trStackMemory()) BlockMapper(from, to));
      if (from == lastBlock)
         break;
      }
   return doBlockClone(&bMap);
   }

TR::Block *
TR_BlockCloner::cloneBlocks(TR_LinkHeadAndTail<BlockMapper>* bMap)
   {
   TR::Block * toReturn = doBlockClone(bMap);

   // for a path of blocks we're cloning that doesn't follow the fall-through path, the branches will be set to fall through node (incorrectly) by cloneNode.  This code patches up the branches so they point to the right target (the old fall through node)
   // we don't mess with the last node as the connection is handled by whoever called us
   //
   for (BlockMapper* itr = bMap->getFirst(); itr->getNext(); itr = itr->getNext())
      {
      TR::Node* lastRealNode = itr->_to->getExit()->getPrevRealTreeTop()->getNode();
      TR::TreeTop* nextTree = itr->_from->getExit()->getNextTreeTop();
      //if we are in the middle of the clone check and see if we took a branch at an if
      //if branch was taken reverse the branch condition so we fall through to the next block as expected
      //
      if (
          lastRealNode->getOpCode().isIf() &&
          lastRealNode->getBranchDestination()->getNode()->getBlock()->getNumber() == itr->_to->getNextBlock()->getNumber()
         )
         {
         TR::TreeTop *newDest = NULL;
         if (_cloneBranchesExactly)
            newDest = itr->_from->getExit()->getNextTreeTop();
         else
            newDest = getToBlock(itr->_from->getExit()->getNextTreeTop()->getNode()->getBlock())->getEntry();
         lastRealNode->reverseBranch(newDest);
         }
      }

   return toReturn;
   }

TR::Block *
TR_BlockCloner::getToBlock(TR::Block * from)
   {
   for (BlockMapper * m = _blockMappings.getFirst(); m; m = m->getNext())
      if (m->_from == from)
         return m->_to;
   return from;
   }

TR::Block *
TR_BlockCloner::doBlockClone(TR_LinkHeadAndTail<BlockMapper>* bMap)
   {
   _blockMappings = *bMap;
   TR::Compilation *comp = _cfg->comp();
   BlockMapper * m;
   for (m = _blockMappings.getFirst(); m; m = m->getNext())
      {
      TR::TreeTop * fromTT = m->_from->getEntry();

      // Start with new node mappings if this is the start of a new extended
      // basic block.
      //
      if (!fromTT->getNode()->getBlock()->isExtensionOfPreviousBlock())
         _nodeMappings.clear();

      // Create the BBStart node
      //
      m->_to->getEntry()->setNode(cloneNode(fromTT->getNode()));
      m->_to->getEntry()->getNode()->setBlock(m->_to);

      // Create the trees for the body of the block
      //
      for (fromTT = fromTT->getNextTreeTop(); fromTT != m->_from->getExit(); fromTT = fromTT->getNextTreeTop())
         {
         TR::TreeTop * toTT = TR::TreeTop::create(comp, cloneNode(fromTT->getNode()));
         m->_to->append(toTT);
         }

      // Create the BBEnd node
      //
      m->_to->getExit()->setNode(cloneNode(fromTT->getNode()));
      m->_to->getExit()->getNode()->setBlock(m->_to);

      comp->setCurrentBlock(m->_from);

      }

   // Create CFG edges for the cloned blocks
   //
   // Some Extra remarks about this code.  This cfg code looks insufficient to really handle the case when you are not cloning along a
   // fall through path, and you are trying to preserve branches exactly (ie _cloneBranchesExactly is set to true
   // Luckily, so far no code has ever not done that.  Most code that clonesBranchesExactly clones a single block, and the few cases that
   // don't appear to be cloning along a fall through path
   //
   for (m = _blockMappings.getFirst(); m; m = m->getNext())
      {
      _cfg->addNode(m->_to);
      if (comp->ilGenTrace())
         dumpOptDetails(comp,"BLOCK CLONER: Newly created block_%d is a clone of original block_%d\n", m->_to->getNumber(), m->_from->getNumber());
      if (_cloneSuccessorsOfLastBlock ||
          m != _blockMappings.getLast())
         {
         for (auto e = m->_from->getSuccessors().begin(); e != m->_from->getSuccessors().end(); ++e)
            _cfg->addEdge(m->_to, getToBlock(toBlock((*e)->getTo())));

         for (auto e = m->_from->getExceptionSuccessors().begin(); e != m->_from->getExceptionSuccessors().end(); ++e)
            _cfg->addExceptionEdge(m->_to, getToBlock(toBlock((*e)->getTo())));

         for (auto e = m->_from->getExceptionPredecessors().begin(); e != m->_from->getExceptionPredecessors().end(); ++e)
            _cfg->addExceptionEdge(toBlock((*e)->getFrom()), m->_to);
         }
      }

   _lastToBlock = _blockMappings.getLast()->_to;

   return _blockMappings.getFirst()->_to;
   }

TR::Node *
TR_BlockCloner::cloneNode(TR::Node * from)
   {
   TR::Node * to;
   if (from->getReferenceCount() > 1 && (to = _nodeMappings.getTo(from)))
      return to;

   int32_t n = from->getNumChildren();
   to = TR::Node::copy(from);

   if (from->getOpCode().getOpCodeValue() == TR::allocationFence)
      {
      if (from->getAllocation())
         {
         TR::Node *dupAllocNode = _nodeMappings.getTo(from->getAllocation());
         if (dupAllocNode)
            to->setAllocation(dupAllocNode);
         }
      }

   if (from->getOpCode().isBranch())
      {
      if (_cloneBranchesExactly)
         to->setBranchDestination(from->getBranchDestination());
      else
         to->setBranchDestination(getToBlock(from->getBranchDestination()->getNode()->getBlock())->getEntry());
      }

   for (int32_t i = 0; i < n; ++i)
      to->setChild(i, cloneNode(from->getChild(i)));

   if (from->getReferenceCount() > 1)
      _nodeMappings.add(from, to, _cfg->comp()->trMemory());

   return to;
   }

TR::Block *
TR_ExtendedBlockSuccessorIterator::getFirst()
   {
   if (_firstBlock == _cfg->getEnd())
      return 0;

   setCurrentBlock(_firstBlock);
   _iterator = _list->begin();
   TR::Block * b = toBlock(_list->front()->getTo());
   return b == _nextBlockInExtendedBlock ? getNext() : b;
   }

TR::Block *
TR_ExtendedBlockSuccessorIterator::getNext()
   {
   TR::CFGEdge* edge;
   if ((_iterator == _list->end()) || (++_iterator == _list->end()))
      {
      if (!_nextBlockInExtendedBlock)
         return 0;
      setCurrentBlock(_nextBlockInExtendedBlock);
      edge = _list->empty() ? NULL : _list->front();
      _iterator = _list->begin();
      }
   else
      edge = *_iterator;
   TR::Block * b = edge ? toBlock(edge->getTo()) : NULL;
   return b == _nextBlockInExtendedBlock ? getNext() : b;
   }

void
TR_ExtendedBlockSuccessorIterator::setCurrentBlock(TR::Block * b)
   {
   _list = &b->getSuccessors();
   _iterator = _list->begin();
   TR::Block * nextBlock = b->getNextBlock();
   _nextBlockInExtendedBlock = nextBlock && nextBlock->isExtensionOfPreviousBlock() ? nextBlock : 0;
   }
