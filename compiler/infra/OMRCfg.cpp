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

#include "infra/OMRCfg.hpp"

#include <algorithm>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "env/PersistentInfo.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/deque.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "ras/Debug.hpp"

#ifdef __MVS__
#include <stdlib.h>
#endif

#define OPT_DETAILS "O^O LOCAL OPTS: "


TR::CFG *
OMR::CFG::self() {
   return static_cast<TR::CFG*>(this);
}

TR::CFGNode *
OMR::CFG::addNode(TR::CFGNode *n, TR_RegionStructure *parent, bool isEntryInParent)
   {
   _nodes.add(n);
   n->setNumber(allocateNodeNumber());

   if (parent &&
       getStructure())
      {
      TR::Block *block = n->asBlock();
      if (block)
         {
         TR_BlockStructure *blockStructure = block->getStructureOf();
         TR_StructureSubGraphNode *blockNode = NULL;
         if (!blockStructure)
            blockStructure = new (structureRegion()) TR_BlockStructure(comp(), block->getNumber(), block);
         else
            {
            TR_StructureSubGraphNode *node;
            TR_RegionStructure::Cursor si(*parent);
            for (node = si.getCurrent(); node != NULL; node = si.getNext())
               {
               if (node->getStructure() == blockStructure)
                  {
                  blockNode = node;
                  break;
                  }
               }
            }

         blockStructure->setNumber(n->getNumber());

         if (!blockNode)
            {
            blockNode = new (structureRegion()) TR_StructureSubGraphNode(blockStructure);
            if (!isEntryInParent)
               {
               parent->addSubNode(blockNode);
               }
            else
               {
               //TR_ASSERT(0, "Adding node as entry in structure\n");
               setStructure(NULL);
               }
            }
         blockNode->setNumber(n->getNumber());
         }
      else
         TR_ASSERT(0, "Trying to add block structure without creating a block\n");
      }

   return n;
   }

void
OMR::CFG::addEdge(TR::CFGEdge *e)
   {
   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      {
      traceMsg(comp(),"\nAdding edge %d-->%d:\n", e->getFrom()->getNumber(), e->getTo()->getNumber());
      }

     _numEdges++;

   // Tell the control tree to modify the structures containing this edge
   //
   if (getStructure() != NULL)
      {
      getStructure()->addEdge(e, false);
      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         {
         traceMsg(comp(),"\nStructures after adding edge %d-->%d:\n", e->getFrom()->getNumber(), e->getTo()->getNumber());
         comp()->getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
         }
      }
   }


TR::CFGEdge *
OMR::CFG::addEdge(TR::CFGNode *f, TR::CFGNode *t, TR_AllocationKind allocKind)
   {

   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      {
      traceMsg(comp(),"\nAdding real edge %d-->%d:\n", f->getNumber(), t->getNumber());
      }

   TR_ASSERT(!f->hasExceptionSuccessor(t), "adding a non exception edge when there's already an exception edge");

   TR::CFGEdge * e = TR::CFGEdge::createEdge(f, t, trMemory(), allocKind);
   addEdge(e);
   return e;
   }


void
OMR::CFG::addExceptionEdge(
      TR::CFGNode *f,
      TR::CFGNode *t,
      TR_AllocationKind allocKind)
   {
   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      {
      traceMsg(comp(),"\nAdding exception edge %d-->%d:\n", f->getNumber(), t->getNumber());
      }

   TR_ASSERT(!f->hasSuccessor(t), "adding an exception edge when there's already a non exception edge");
   TR::Block * newCatchBlock = toBlock(t);
   for (auto e = f->getExceptionSuccessors().begin(); e != f->getExceptionSuccessors().end(); ++e)
      {
      TR::Block * existingCatchBlock = toBlock((*e)->getTo());
      if (newCatchBlock == existingCatchBlock) return;

      // OSR exception edges are special and we do not want any 'optimization' done to them
      // from the following special checks
      if (newCatchBlock->isOSRCatchBlock() || existingCatchBlock->isOSRCatchBlock()) continue;

      // If the existing catch block is going to be considered first and it catches everything that
      // the new catch block does then the new catch block isn't reaching from the 'f' block
      //
      // Catch block 'A' is considered before catch block 'B' if 'A' is from a greater inline depth
      // or 'A' and 'B' are from the same inline depth and 'A' handler index is less than 'B's.
      //
      int32_t existingDepth = existingCatchBlock->getInlineDepth();
      int32_t newDepth = newCatchBlock->getInlineDepth();
      if (existingDepth < newDepth ||
          (existingDepth == newDepth && existingCatchBlock->getHandlerIndex() > newCatchBlock->getHandlerIndex()))
         continue;

      // The existing catch block is going to be considered first.  Don't add an edge to the new catch
      // block if the existing one catches everything that the new one catches.
      //
      /////void * newEC = newCatchBlock->getExceptionClass(), * existingEC = existingCatchBlock->getExceptionClass();

      if (existingCatchBlock->getCatchType() == 0 ||
          /////(newEC && existingEC && isInstanceOf(newEC, existingEC)) ||
          (existingDepth == newDepth && existingCatchBlock->getCatchType() == newCatchBlock->getCatchType()))
         {
         if (comp()->getOption(TR_TraceAddAndRemoveEdge))
            {
            traceMsg(comp(),"\nAddition of exception edge aborted - existing catch alredy handles this case!");
            }
         return;
         }
      }
   TR::CFGEdge* e = TR::CFGEdge::createExceptionEdge(f,t, trMemory(), allocKind);
   _numEdges++;

   // Tell the control tree to modify the structures containing this edge
   //
   if (getStructure() != NULL)
      {
      getStructure()->addEdge(e, true);
      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         {
         traceMsg(comp(),"\nStructures after adding exception edge %d-->%d:\n", f->getNumber(), t->getNumber());
         comp()->getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
         }
      }
   }



void
OMR::CFG::addSuccessorEdges(TR::Block * block)
   {
   TR::Node * node = block->getLastRealTreeTop()->getNode();
   switch (node->getOpCode().getOpCodeValue())
      {
      case TR::table:
      case TR::lookup:
      case TR::trtLookup:
         {
         vcount_t visitCount = comp()->incVisitCount();
         int32_t n = node->getCaseIndexUpperBound();
         for (int32_t i = 1; i < n; ++i)
            {
            TR::TreeTop * target = node->getChild(i)->getBranchDestination();
            TR::Block * targetBlock = target->getNode()->getBlock();
            if (targetBlock->getVisitCount() != visitCount)
               {
               addEdge(block, targetBlock);
               targetBlock->setVisitCount(visitCount);
               }
            }
         break;
         }
      case TR::ificmpeq: case TR::ificmpne: case TR::ificmplt: case TR::ificmpge: case TR::ificmpgt: case TR::ificmple:
      case TR::ifiucmpeq: case TR::ifiucmpne: case TR::ifiucmplt: case TR::ifiucmpge: case TR::ifiucmpgt: case TR::ifiucmple:
      case TR::iflcmpeq: case TR::iflcmpne: case TR::iflcmplt: case TR::iflcmpge: case TR::iflcmpgt: case TR::iflcmple:
      case TR::iffcmpeq: case TR::iffcmpne: case TR::iffcmplt: case TR::iffcmpge: case TR::iffcmpgt: case TR::iffcmple:
      case TR::ifdcmpeq: case TR::ifdcmpne: case TR::ifdcmplt: case TR::ifdcmpge: case TR::ifdcmpgt: case TR::ifdcmple:
      case TR::ifbcmpeq: case TR::ifbcmpne: case TR::ifbcmplt: case TR::ifbcmpge: case TR::ifbcmpgt: case TR::ifbcmple:
      case TR::ifscmpeq: case TR::ifscmpne: case TR::ifscmplt: case TR::ifscmpge: case TR::ifscmpgt: case TR::ifscmple:
      case TR::ifsucmpeq: case TR::ifsucmpne: case TR::ifsucmplt: case TR::ifsucmpge: case TR::ifsucmpgt: case TR::ifsucmple:
      case TR::ifacmpeq: case TR::ifacmpne:
      case TR::iffcmpequ: case TR::iffcmpneu: case TR::iffcmpltu: case TR::iffcmpgeu: case TR::iffcmpgtu: case TR::iffcmpleu:
      case TR::ifdcmpequ: case TR::ifdcmpneu: case TR::ifdcmpltu: case TR::ifdcmpgeu: case TR::ifdcmpgtu: case TR::ifdcmpleu:
         {
         TR::Block * branchBlock = node->getBranchDestination()->getNode()->getBlock();
         addEdge(block, branchBlock);
         TR::Block * nextBlock = block->getExit()->getNextTreeTop()->getNode()->getBlock();
         if (branchBlock != nextBlock)
            addEdge(block, nextBlock);
         break;
         }
      case TR::Goto:
         addEdge(block, node->getBranchDestination()->getNode()->getBlock());
         break;
      case TR::ireturn: case TR::lreturn: case TR::freturn: case TR::dreturn: case TR::areturn: case TR::Return:

      case TR::athrow:
         addEdge(block, getEnd());
         break;
      case TR::Ret:
         break; // pseudo node representing a ret opcode
      case TR::NULLCHK:
         if (node->getFirstChild()->getOpCodeValue() == TR::athrow)
            addEdge(block, getEnd());
         else
            addEdge(block, block->getExit()->getNextTreeTop()->getNode()->getBlock());
         break;
      default:
         addEdge(block, block->getExit()->getNextTreeTop()->getNode()->getBlock());
      }
   }


void
OMR::CFG::join(TR::Block * b1, TR::Block * b2)
   {
   if (b2) b1->getExit()->join(b2->getEntry());
   self()->addSuccessorEdges(b1);
   }


void
OMR::CFG::insertBefore(TR::Block * b1, TR::Block * b2)
   {
   self()->addNode(b1);
   self()->join(b1, b2);
   }


void
OMR::CFG::copyExceptionSuccessors(
      TR::CFGNode *from,
      TR::CFGNode *to,
      bool (*predicate)(TR::CFGEdge *))
   {
   for (auto e1 = from->getExceptionSuccessors().begin(); e1 != from->getExceptionSuccessors().end(); ++e1)
      {
      if (predicate(*e1))
         {
         self()->addExceptionEdge(to, toBlock((*e1)->getTo()));
         }
      }
   }

TR_Structure *
OMR::CFG::invalidateStructure()
   {
   setStructure(NULL);
   _structureRegion.~Region();
   new (&_structureRegion) TR::Region(comp()->trMemory()->heapMemoryRegion());
   return getStructure();
   }

TR_Structure *
OMR::CFG::setStructure(TR_Structure *p)
   {
   if (_rootStructure && !p)
      {
      dumpOptDetails(comp(), "     (Invalidating structure)\n");
      }
   return (_rootStructure = p);
   }

/**
 * Default predicate for copyExceptionSuccessors.
 */
bool OMR::alwaysTrue(TR::CFGEdge * e)
   {
   return true;
   }


TR_OrderedExceptionHandlerIterator::TR_OrderedExceptionHandlerIterator(TR::Block * tryBlock, TR::Region &workingRegion)
   {
   if (tryBlock->getExceptionSuccessors().empty())
      _dim = 0;
   else
      {
      int32_t handlerDim = 1, inlineDim = 1;
      for (auto e = tryBlock->getExceptionSuccessors().begin(); e != tryBlock->getExceptionSuccessors().end(); ++e)
         {
         TR::Block * b = toBlock((*e)->getTo());
         if (b->getHandlerIndex() >= handlerDim)
            handlerDim = b->getHandlerIndex() + 1;
         if (b->getInlineDepth() >= inlineDim)
            inlineDim = b->getInlineDepth() + 1;
         }

      _dim = handlerDim * inlineDim;
      _handlers = (TR::Block **)workingRegion.allocate(_dim*sizeof(TR::Block *));
      memset(_handlers, 0, _dim*sizeof(TR::Block *));

      for (auto e = tryBlock->getExceptionSuccessors().begin(); e != tryBlock->getExceptionSuccessors().end(); ++e)
         {
         TR::Block * b = toBlock((*e)->getTo());
         TR_ASSERT((_handlers[((inlineDim - b->getInlineDepth() - 1) * handlerDim) + b->getHandlerIndex()] == NULL), "handler entry is not NULL\n");
         _handlers[((inlineDim - b->getInlineDepth() - 1) * handlerDim) + b->getHandlerIndex()] = b;
         }
      }
   }

TR::Block *
TR_OrderedExceptionHandlerIterator::getFirst()
   {
   _index = 0;

   return getCurrent();
   }

TR::Block *
TR_OrderedExceptionHandlerIterator::getNext()
   {
   ++_index;
   return getCurrent();
   }

TR::Block *
TR_OrderedExceptionHandlerIterator::getCurrent()
   {
   TR::Block * handler = 0;
   while (_index < _dim && (handler = _handlers[_index]) == 0)
      ++_index;

   return handler;
   }


TR::CFGEdge::CFGEdge(TR::CFGNode *pF, TR::CFGNode *pT)
   : _pFrom(pF), _pTo(pT), _visitCount(0), _frequency(0), _id(-1)
   {}

TR::CFGEdge * TR::CFGEdge::createEdge (TR::CFGNode *pF, TR::CFGNode *pT, TR_Memory* trMemory, TR_AllocationKind allocKind)
   {
   TR::CFGEdge * newEdge =  new (trMemory, allocKind) TR::CFGEdge(pF, pT);

   pF->addSuccessor(newEdge);
   pT->addPredecessor(newEdge);
   if (pT->getFrequency() >= 0)
      newEdge->setFrequency(pT->getFrequency());
   if ((pF->getFrequency() >= 0) && (pF->getFrequency() < newEdge->getFrequency()))
      newEdge->setFrequency(pF->getFrequency());

   return newEdge;
   }

TR::CFGEdge * TR::CFGEdge::createEdge (TR::CFGNode *pF, TR::CFGNode *pT, TR::Region &region)
   {
   TR::CFGEdge * newEdge =  new (region) TR::CFGEdge(pF, pT);

   pF->addSuccessor(newEdge);
   pT->addPredecessor(newEdge);
   if (pT->getFrequency() >= 0)
      newEdge->setFrequency(pT->getFrequency());
   if ((pF->getFrequency() >= 0) && (pF->getFrequency() < newEdge->getFrequency()))
      newEdge->setFrequency(pF->getFrequency());

   return newEdge;
   }

TR::CFGEdge * TR::CFGEdge::createExceptionEdge (TR::CFGNode *pF, TR::CFGNode *pT, TR_Memory* trMemory, TR_AllocationKind allocKind)
   {
   TR::CFGEdge * newEdge =  new (trMemory, allocKind) TR::CFGEdge(pF, pT);

   pF->addExceptionSuccessor(newEdge);
   pT->addExceptionPredecessor(newEdge);

   return newEdge;
   }

TR::CFGEdge * TR::CFGEdge::createExceptionEdge (TR::CFGNode *pF, TR::CFGNode *pT, TR::Region &region)
   {
   TR::CFGEdge * newEdge =  new (region) TR::CFGEdge(pF, pT);

   pF->addExceptionSuccessor(newEdge);
   pT->addExceptionPredecessor(newEdge);

   return newEdge;
   }

void TR::CFGEdge::setFrom(TR::CFGNode *pF)
   {
   _pFrom = pF;
   pF->addSuccessor(this);
   }

void TR::CFGEdge::setTo(TR::CFGNode *pT)
   {
   _pTo   = pT;
   pT->addPredecessor(this);
   }

void TR::CFGEdge::setExceptionFrom(TR::CFGNode *pF)
   {
   _pFrom = pF;
   pF->addExceptionSuccessor(this);
   }

void TR::CFGEdge::setExceptionTo(TR::CFGNode *pT)
   {
   _pTo   = pT;
   pT->addExceptionPredecessor(this);
   }

void TR::CFGEdge::setFromTo(TR::CFGNode *pF, TR::CFGNode *pT)
   {
   setFrom(pF);
   setTo(pT);
   }

void TR::CFGEdge::setExceptionFromTo(TR::CFGNode *pF, TR::CFGNode *pT)
   {
   _pFrom = pF;
   _pTo   = pT;
   pF->addExceptionSuccessor(this);
   pT->addExceptionPredecessor(this);
   }

static int32_t getTotalEdgesFrequency(TR::CFGEdgeList& edgesList)
   {
   int32_t totalFreq = 0;
   for (auto edge = edgesList.begin(); edge != edgesList.end(); ++edge)
      totalFreq += (*edge)->getFrequency();
   return totalFreq;
   }


bool OMR::CFG::updateBlockFrequency(TR::Block *block, int32_t newFreq)
   {
   int16_t oldFreq = block->getFrequency();
   if (newFreq != oldFreq && newFreq >= 0)
      {
      if (comp()->getOption(TR_TraceBFGeneration))
         traceMsg(comp(), "updated block %d freq from %d to %d\n", block->getNumber(), oldFreq, newFreq);
      block->setFrequency(newFreq);
      return true;
      }
   else
      return false;
   }

void OMR::CFG::updateBlockFrequencyFromEdges(TR::Block *block)
   {
   int32_t totalPredFreq = getTotalEdgesFrequency(block->getPredecessors()) + getTotalEdgesFrequency(block->getExceptionPredecessors());
   int32_t totalSuccFreq = getTotalEdgesFrequency(block->getSuccessors()) + getTotalEdgesFrequency(block->getExceptionSuccessors());

   int32_t maxNewFreq = std::min(totalPredFreq, totalSuccFreq);

   // currently only lower frequency
   if (maxNewFreq < block->getFrequency())
      updateBlockFrequency(block, maxNewFreq);
   }


TR::CFGNode::CFGNode(TR_Memory * m)
   : _nodeNumber(-1),
     _visitCount(0),
     _frequency(-1),
     _forwardTraversalIndex(-1),
     _backwardTraversalIndex(-1),
     _region(m->heapMemoryRegion()),
     _successors(m->heapMemoryRegion()),
     _predecessors(m->heapMemoryRegion()),
     _exceptionSuccessors(m->heapMemoryRegion()),
     _exceptionPredecessors(m->heapMemoryRegion())
   {
   }
TR::CFGNode::CFGNode(int32_t n, TR_Memory * m)
   : _nodeNumber(n),
     _visitCount(0),
     _frequency(-1),
     _forwardTraversalIndex(-1),
     _backwardTraversalIndex(-1),
     _region(m->heapMemoryRegion()),
     _successors(m->heapMemoryRegion()),
     _predecessors(m->heapMemoryRegion()),
     _exceptionSuccessors(m->heapMemoryRegion()),
     _exceptionPredecessors(m->heapMemoryRegion())
   {
   }
TR::CFGNode::CFGNode(TR::Region &region)
   : _nodeNumber(-1),
     _visitCount(0),
     _frequency(-1),
     _forwardTraversalIndex(-1),
     _backwardTraversalIndex(-1),
     _region(region),
     _successors(region),
     _predecessors(region),
     _exceptionSuccessors(region),
     _exceptionPredecessors(region)
   {
   }
TR::CFGNode::CFGNode(int32_t n, TR::Region &region)
   : _nodeNumber(n),
     _visitCount(0),
     _frequency(-1),
     _forwardTraversalIndex(-1),
     _backwardTraversalIndex(-1),
     _region(region),
     _successors(region),
     _predecessors(region),
     _exceptionSuccessors(region),
     _exceptionPredecessors(region)
   {
   }

// This method needs to be non-inlined so that the home for TR::CFGNode's vft is
// established.
//
void TR::CFGNode::removeFromCFG(TR::Compilation *c)
   {
   }


TR::TreeTop * OMR::CFG::findLastTreeTop()
   {
   TR::Block *cursorBlock = getStart()->getSuccessors().front()->getTo()->asBlock();
   TR::Block *prevCursorBlock = NULL;
   while (cursorBlock)
      {
      prevCursorBlock = cursorBlock;
      cursorBlock = cursorBlock->getNextBlock();
      }

   if (prevCursorBlock)
      return prevCursorBlock->getExit();

   return NULL;
   }

TR::CFGEdge *TR::CFGNode::getEdge(TR::CFGNode *n)
   {
   TR_SuccessorIterator ei(this);
   for (TR::CFGEdge * edge = ei.getFirst(); edge != NULL; edge = ei.getNext())
      {
      if (edge->getTo() == n)
         return edge;
      }
   return NULL;
   }

template <typename FUNC>
TR::CFGEdge * TR::CFGNode::getEdgeMatchingNodeInAList (TR::CFGNode * n, TR::CFGEdgeList& list, FUNC blockGetter)
	{
	for (auto edge = list.begin(); edge != list.end(); ++edge)
	  {
	  if (blockGetter(*edge) == n)
		 return *edge;
	  }
	return NULL;
	}

TR::CFGEdge * TR::CFGNode::getSuccessorEdge(TR::CFGNode * n)
	{
	return getEdgeMatchingNodeInAList(n, getSuccessors(), toBlockGetter);
	}

TR::CFGEdge * TR::CFGNode::getExceptionSuccessorEdge(TR::CFGNode * n)
	{
	return getEdgeMatchingNodeInAList(n, getExceptionSuccessors(), toBlockGetter);
	}

TR::CFGEdge * TR::CFGNode::getPredecessorEdge(TR::CFGNode * n)
	{
	return getEdgeMatchingNodeInAList(n, getPredecessors(), fromBlockGetter);
	}

TR::CFGEdge * TR::CFGNode::getExceptionPredecessorEdge(TR::CFGNode * n)
	{
	return getEdgeMatchingNodeInAList(n, getExceptionPredecessors(), fromBlockGetter);
	}

bool TR::CFGNode::hasSuccessor(TR::CFGNode * n)
   {
   return (bool)getSuccessorEdge(n);
   }

bool TR::CFGNode::hasExceptionSuccessor(TR::CFGNode * n)
   {
   return (bool)getExceptionSuccessorEdge(n);
   }

bool TR::CFGNode::hasPredecessor(TR::CFGNode * n)
   {
   return (bool)getPredecessorEdge(n);
   }

bool TR::CFGNode::hasExceptionPredecessor(TR::CFGNode * n)
   {
   return (bool)getExceptionPredecessorEdge(n);
   }


TR::CFGNode *OMR::CFG::removeNode(TR::CFGNode *node)
   {

   if (node->nodeIsRemoved())
      return 0;

   _nodes.remove(node);
   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      traceMsg(comp(),"\nRemoving node %d\n", node->getNumber());


   node->removeFromCFG(comp());

   // Remove the exception successors first, so that try/finally structures get
   // cleaned up in the right order. Nobody else cares about the order.
   //
   ListElement<TR::CFGEdge> *out;
   while (!node->getExceptionSuccessors().empty())
      removeEdge(node->getExceptionSuccessors().front());

   while (!node->getSuccessors().empty())
      removeEdge(node->getSuccessors().front());

   // All predecessors must have been removed otherwise the CFG will be inconsistent.
   //
   TR_ASSERT(node->getPredecessors().empty(),
          "CFG, removing a node that still has predecessors");
   TR_ASSERT(node->getExceptionPredecessors().empty(),
          "CFG, removing a node that still has predecessors");

   node->removeNode();
   return node;
   }

enum OrphanType
   {
   IsParented = 0,
   IsOrphanedNode,
   IsOrphanedRegion
   };

// test if 'to' is an unreachable orphan
//
static OrphanType unreachableOrphan(TR::CFG *cfg, TR::CFGNode *from, TR::CFGNode *to)
   {
   // If the "to" node is orphaned by removing this edge, remove it from the
   // CFG as long as it is not the exit node - it is valid for the exit node
   // to be unreachable.
   //
   if ((to->getPredecessors().empty() && to->getExceptionPredecessors().empty() &&
        to != cfg->getEnd()) ||
       (to->getExceptionPredecessors().empty() && (to->getPredecessors().size() == 1) &&
        to->getPredecessors().front()->getFrom() == to) ||
       (to->getPredecessors().empty() && (to->getExceptionPredecessors().size() == 1) &&
        to->getExceptionPredecessors().front()->getFrom() == to))
      {
      return IsOrphanedNode;
      }

   // If we have structure, and 'to' is the entry node of a cyclic region
   // and as a result of removing this edge we have made 'to' unreachable, except
   // from backedges originating from within 'to' then recognize 'to' to be orphaned
   //
   if (cfg->getStructure())
      {
      TR_RegionStructure *parent = from->asBlock()->getStructureOf()->findCommonParent(to->asBlock()->getStructureOf(), cfg);
      TR_StructureSubGraphNode *toSubNode = parent->findSubNodeInRegion(to->getNumber());
      TR_ASSERT(toSubNode, "cannot have cfg edge a->b in region p where b is not a direct subnode of p");

      TR_RegionStructure *toRegion = toSubNode->getStructure()->asRegion();
      if (toRegion)
         {
         bool onlyBackEdges = true;
         TR_PredecessorIterator pit(to);
         TR::CFGEdge *edge;
         for (edge = pit.getFirst(); edge; edge = pit.getNext())
            {
            if (!toRegion->contains(edge->getFrom()->asBlock()->getStructureOf()) &&
                  (edge->getFrom() != from))
               {
               onlyBackEdges = false;
               break;
               }
            }

         if (onlyBackEdges)
            return IsOrphanedRegion;
         }
      }

   return IsParented;
   }

/*
This Function looks for an edge in the successor/predecessor OR
exceptionSuccessor/exceptionPredecessor Lists from the 'from' and 'to' Nodes of
the edge If the edge is not found in either combination, we return false
*/
bool OMR::CFG::removeEdge(TR::CFGEdge *edge)
   {

   bool blocksWereRemoved = false;

   TR::CFGNode *from = edge->getFrom();
   TR::CFGNode *to = edge->getTo();

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _mightHaveUnreachableBlocks = true;

   bool found = false;

   if (std::find(from->getSuccessors().begin(), from->getSuccessors().end(), edge) != from->getSuccessors().end()) {
      found = true;
      from->getSuccessors().remove(edge);
   }
   else if (std::find(from->getExceptionSuccessors().begin(), from->getExceptionSuccessors().end(), edge) != from->getExceptionSuccessors().end()) {
      found = true;
      from->getExceptionSuccessors().remove(edge);
   }

   if (std::find(to->getPredecessors().begin(), to->getPredecessors().end(), edge) != to->getPredecessors().end()) {
      found = true;
      to->getPredecessors().remove(edge);
   }
   else if (std::find(to->getExceptionPredecessors().begin(), to->getExceptionPredecessors().end(), edge) != to->getExceptionPredecessors().end()) {
      found = true;
      to->getExceptionPredecessors().remove(edge);
   }

   if (!found)
      return false;

   _numEdges--;

   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      traceMsg(comp(), "\nRemoving edge %d-->%d (depth %d):\n", from->getNumber(), to->getNumber(), _removeEdgeNestingDepth);

   TR_ScratchList<TR::CFGNode> nodesToBeRemoved(trMemory());
   bool doWalk = false;
   TR_BitVector *blocksVisited = NULL;
   OrphanType orphan = unreachableOrphan(self(), from, to);
   if (orphan != IsParented)
      {
      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         {
         traceMsg(comp(),"\nblock_%d is an orphan now with type=%d:\n", to->getNumber(), orphan);
         }

         {
         if (comp()->getOption(TR_TraceAddAndRemoveEdge))
            traceMsg(comp(), "\nAdding node %d to nodesToBeRemoved from %d\n", to->getNumber(), from->getNumber());

         nodesToBeRemoved.add(to);
         blocksVisited = new (trStackMemory()) TR_BitVector(getNextNodeNumber(), trMemory(), stackAlloc);
         blocksWereRemoved = true;
         doWalk = true;
         }
      }

   // Tell the control tree to modify the structures containing this edge
   //
   if (getStructure())
      {
      TR_Structure *fromStruct   = toBlock(from)->getStructureOf();
      TR_Structure *toStruct   = toBlock(to)->getStructureOf();
      if (fromStruct && toStruct)
         {
         toStruct->removeEdge(fromStruct, toStruct);
         }

      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         {
         traceMsg(comp(),"\nStructures changed after removing edge %d-->%d:\n", from->getNumber(), to->getNumber());
         }
      }

   if (doWalk)
      {
      TR_Queue<TR::CFGNode> nodeq(trMemory());
      nodeq.enqueue(to);
      blocksVisited->empty();
      do
         {
         from = nodeq.dequeue();

         if (comp()->getOption(TR_TraceAddAndRemoveEdge))
            traceMsg(comp(), "\ndo walk for node %d\n", from->getNumber());

         if (blocksVisited->get(from->getNumber()))
            continue;

         // if the node is already removed, donot process
         //
         if(from->nodeIsRemoved())
             continue;

         if (comp()->getOption(TR_TraceAddAndRemoveEdge))
            traceMsg(comp(), "Processing unreachable node %d\n", from->getNumber());

         blocksVisited->set(from->getNumber());

         TR_SuccessorIterator edgesIt(from);
         for (TR::CFGEdge * e = edgesIt.getFirst(); e; e = edgesIt.getNext())
            {
            to = e->getTo();
            _numEdges--;

            if (comp()->getOption(TR_TraceAddAndRemoveEdge))
               traceMsg(comp(), "\n2Removing edge %d-->%d (depth %d):\n", from->getNumber(), to->getNumber(), _removeEdgeNestingDepth);
            if (std::find(from->getSuccessors().begin(), from->getSuccessors().end(), e) != from->getSuccessors().end())
               from->getSuccessors().remove(e);
            else
               from->getExceptionSuccessors().remove(e);
            if (std::find(to->getPredecessors().begin(), to->getPredecessors().end(), e) != to->getPredecessors().end())
               to->getPredecessors().remove(e);
            else
               to->getExceptionPredecessors().remove(e);

            // break cycles by not adding 'to' that is
            // already in the removed list
            //
            orphan = unreachableOrphan(self(), from, to);
            if (orphan != IsParented &&
                  !blocksVisited->get(to->getNumber()) &&
                  !to->nodeIsRemoved())
               {

                  {
                  if (comp()->getOption(TR_TraceAddAndRemoveEdge))
                     traceMsg(comp(), "\nAdding node %d to nodesToBeRemoved from %d\n", to->getNumber(), from->getNumber());

                  if (orphan == IsOrphanedNode)
                     nodesToBeRemoved.add(to);

                  blocksWereRemoved = true;
                  nodeq.enqueue(to);
                  }
               }

            // Tell the control tree to modify the structures containing this edge
            //
            if (getStructure())
               {
               TR_Structure *fromStruct   = toBlock(from)->getStructureOf();
               TR_Structure *toStruct   = toBlock(to)->getStructureOf();
               if (fromStruct && toStruct)
                  toStruct->removeEdge(fromStruct, toStruct);

               if (comp()->getOption(TR_TraceAddAndRemoveEdge))
                  {
                  traceMsg(comp(),"\nStructure changed after removing edge %d-->%d:\n", from->getNumber(), to->getNumber());
                  ///comp()->getDebug()->print(comp()->getOutFile(), getStructure(), 6);
                  }
               }
            }
         } while (!nodeq.isEmpty());

      // finally remove all the unreachable nodes
      //
      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         traceMsg(comp(),"\nNow actually removing nodes\n");

      ListIterator<TR::CFGNode> nodesIt(&nodesToBeRemoved);
      for (TR::CFGNode *n = nodesIt.getFirst(); n; n = nodesIt.getNext())
         {
            {
            if (_nodes.remove(n))
               {
               if (comp()->getOption(TR_TraceAddAndRemoveEdge))
                  traceMsg(comp(),"\nRemoved node %d\n", n->getNumber());

               n->removeFromCFG(comp());
               n->removeNode();
               }
            }
         }

      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         traceMsg(comp(), "\n_doesHaveUnreachableBlocks %d\n", _doesHaveUnreachableBlocks);

      if (!_ignoreUnreachableBlocks && _doesHaveUnreachableBlocks)
         {
         removeUnreachableBlocks();
         blocksWereRemoved = true;
         }
      }

   return blocksWereRemoved;
   }

bool OMR::CFG::removeEdge(TR::CFGEdge *edge, bool recursiveImpl)
   {
   TR::CFGNode *from = edge->getFrom();
   TR::CFGNode *to   = edge->getTo();

   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      {
      traceMsg(comp(), "\nRemoving edge %d-->%d (depth %d):\n", from->getNumber(), to->getNumber(), _removeEdgeNestingDepth);
      }

   // Keep track of recursion level for this method.
   //
   ++_removeEdgeNestingDepth;

   // FIXME:
   // due to a stack overflow error in JarTester. removeEdge
   // went into a very deep recursion (159 deep) in one of the
   // methods in JarTester trying to remove a large unreachable
   // portion of the method. abandoning the compile at this stage
   // until the non-recursive implementation above becomes active
   //
   if (_removeEdgeNestingDepth >
       (comp()->getOption(TR_ProcessHugeMethods) ?
        (MAX_REMOVE_EDGE_NESTING_DEPTH * 2) : MAX_REMOVE_EDGE_NESTING_DEPTH))
      {
      comp()->failCompilation<TR::ExcessiveComplexity>("Exceeded removeEdge nesting depth");
      }


   _numEdges--;
   _mightHaveUnreachableBlocks = true;
   if (std::find(from->getSuccessors().begin(), from->getSuccessors().end(), edge) != from->getSuccessors().end())
	  from->getSuccessors().remove(edge);
   else
	  from->getExceptionSuccessors().remove(edge);
   if (std::find(to->getPredecessors().begin(), to->getPredecessors().end(), edge) != to->getPredecessors().end())
	  to->getPredecessors().remove(edge);
   else
      to->getExceptionPredecessors().remove(edge);

   bool blocksWereRemoved = false;

   // If we have structure, and 'to' is the entry node of a cyclic region
   // and as a result of removing this edge we have made 'to' unreachable, except
   // from backedges originating from within 'to' then remove the backedges so that we can remove
   // the 'to' node by correctly recognize 'to' to be orphaned
   //
   if (getStructure())
      {
      TR_RegionStructure *parent = from->asBlock()->getStructureOf()->findCommonParent(to->asBlock()->getStructureOf(), self());
      TR_StructureSubGraphNode *toSubNode = parent->findSubNodeInRegion(to->getNumber());
      TR_ASSERT(toSubNode, "cannot have cfg edge a->b in region p where b is not a direct subnode of p");

      TR_RegionStructure *toRegion = toSubNode->getStructure()->asRegion();
      if (toRegion)
         {
         bool onlyBackEdges = true;
         TR_PredecessorIterator pit(to);
         TR::CFGEdge *edge;
         for (edge = pit.getFirst(); edge; edge = pit.getNext())
            {
            if (!toRegion->contains(edge->getFrom()->asBlock()->getStructureOf()))
               {
               onlyBackEdges = false;
               break;
               }
            }

         if (onlyBackEdges)
            {
            while (!to->getExceptionPredecessors().empty())
               removeEdge(to->getExceptionPredecessors().front());
            while (!to->getPredecessors().empty())
               removeEdge(to->getPredecessors().front());
            }
         }
      }

   // If the "to" node is orphaned by removing this edge, remove it from the
   // CFG as long as it is not the exit node - it is valid for the exit node
   // to be unreachable.
   //
   if ((to->getPredecessors().empty() && to->getExceptionPredecessors().empty() &&
        to != getEnd()) ||
       (to->getPredecessors().empty() && (to->getExceptionPredecessors().size() == 1) &&
        to->getExceptionPredecessors().front()->getFrom() == to) ||
       (to->getExceptionPredecessors().empty() && (to->getPredecessors().size() == 1) &&
        to->getPredecessors().front()->getFrom() == to))
      {
      blocksWereRemoved = true;
      removeNode(to);
      }

   // Tell the control tree to modify the structures containing this edge
   //
   if (getStructure())
      {
      TR_Structure *fromStruct   = toBlock(from)->getStructureOf();
      TR_Structure *toStruct   = toBlock(to)->getStructureOf();
      if (fromStruct && toStruct)
         {
         toStruct->removeEdge(fromStruct, toStruct);
         }

      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         {
         traceMsg(comp(),"\nStructures after removing edge %d-->%d:\n", from->getNumber(), to->getNumber());
         comp()->getDebug()->print(comp()->getOutFile(), getStructure(), 6);
         }
      }

   // If end of recursing through removal of edges and an unreachable cycle was
   // definitely found, remove it now before returning.
   //
   if (comp()->getOption(TR_TraceAddAndRemoveEdge))
      traceMsg(comp(), "\n_doesHaveUnreachableBlocks %d\n", _doesHaveUnreachableBlocks);

   if (_removeEdgeNestingDepth == 1 && _doesHaveUnreachableBlocks)
      {
      removeUnreachableBlocks();
      blocksWereRemoved = true;
      }

   --_removeEdgeNestingDepth;
   return blocksWereRemoved;
   }

bool OMR::CFG::removeEdge(TR::CFGNode *from, TR::CFGNode *to)
   {
   TR_SuccessorIterator ei(from);
   for (TR::CFGEdge * edge = ei.getFirst(); edge != NULL; edge = ei.getNext())
      if (edge->getTo() == to)
         return removeEdge(edge);
   return false;
   }

void
OMR::CFG::removeEdge(TR::CFGEdgeList &succList, int32_t selfNumber, int32_t destNumber)
   {

   for (auto edge = succList.begin(); edge != succList.end();)
      {
      TR::Block * dest = toBlock((*edge)->getTo());
      TR::Block * src  = toBlock((*edge)->getFrom());
      if (src->getNumber() == selfNumber && dest->getNumber() == destNumber)
         {
         removeEdge(*(edge++));
         }
      else
         ++edge;
      }
   return;
   }

void
OMR::CFG::removeUnreachableBlocks()
   {
   // Prevent recursion
   //
   if (_removingUnreachableBlocks)
      return;

   _removingUnreachableBlocks = true;
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Find all reachable blocks then remove then rest
   //
   TR_BitVector reachableBlocks(getNumberOfNodes(), comp()->trMemory(), stackAlloc, growable);
   findReachableBlocks(&reachableBlocks);

   TR_Stack<TR::CFGNode*> unreachableNodes(trMemory(), 8, false, stackAlloc);
   TR::CFGNode *node = getFirstNode();
   for (; node; node = node->getNext())
      {
      if (!reachableBlocks.get(node->getNumber()) && node->asBlock() && node != getEnd())
         unreachableNodes.push(node);
      }

   while (!unreachableNodes.isEmpty())
      {
      node = unreachableNodes.pop();
      if (comp()->getOption(TR_TraceAddAndRemoveEdge))
         traceMsg(comp(),"\nBlock_%d [%p] is now unreachable, with 0 predecessors=%d\n", node->getNumber(), node,node->isUnreachable());


      if (node->isUnreachable())
         {
         removeNode(node);
         }
      // has predecessors, likely loop entry
      else
         {
         ListElement<TR::CFGEdge> *in;
         while (!node->getExceptionPredecessors().empty())
            removeEdge(node->getExceptionPredecessors().front());
         while (!node->getPredecessors().empty())
            removeEdge(node->getPredecessors().front());
         }
      }

   _mightHaveUnreachableBlocks = false;
   _doesHaveUnreachableBlocks  = false;
   _removingUnreachableBlocks  = false;
   }


void OMR::CFG::resetVisitCounts(vcount_t count)
   {
   TR::CFGNode *nextNode = getFirstNode();
   for (; nextNode != NULL; nextNode = nextNode->getNext())
       {
        nextNode->setVisitCount(count);
        TR_SuccessorIterator sit(nextNode);
        TR::CFGEdge * edge = sit.getFirst();
        for(; edge != NULL; edge=sit.getNext())
           {
           edge->setVisitCount(count);
           }
       }


   if (getStructure())
     getStructure()->resetVisitCounts(count);
   }

bool OMR::CFG::consumePseudoRandomFrequencies()
   {
   TR::CFGNode *nextNode = getFirstNode();

   int32_t origIndex = comp()->getPersistentInfo()->getCurIndex();
   TR_PseudoRandomNumbersListElement *origNumbers = comp()->getPersistentInfo()->getCurPseudoRandomNumbersListElem();
   int32_t maxFrequency = -1;
   int32_t count = 0;
   int32_t edgeIndex = 0;
   for (; nextNode != NULL; nextNode = nextNode->getNext())
      {
      count++;
      int32_t f = comp()->convertNonDeterministicInput(nextNode->getFrequency(), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, NULL, 0, false);
      if ((f >= 0) && (f <= MAX_COLD_BLOCK_COUNT))
  nextNode->asBlock()->setIsCold();
      if (f > maxFrequency)
  maxFrequency = f;
      nextNode->setFrequency(f);

      TR_SuccessorIterator sit(nextNode);
      for(TR::CFGEdge * nextEdge = sit.getFirst(); nextEdge != NULL; nextEdge = sit.getNext())
          {
             count++;
            nextEdge->setFrequency(comp()->convertNonDeterministicInput(nextEdge->getFrequency(), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, NULL, 0, false));
            edgeIndex++;
          }
      }

   _numEdges = edgeIndex;

   int32_t f2 = comp()->convertNonDeterministicInput(maxFrequency, MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, NULL, 0, false);
   count++;
   if (f2 > getMaxFrequency())
      setMaxFrequency(f2);

   int32_t curIndex = comp()->getPersistentInfo()->getCurIndex();
   TR_PseudoRandomNumbersListElement *curNumbers = comp()->getPersistentInfo()->getCurPseudoRandomNumbersListElem();



   if (((origIndex + count) != curIndex) ||
       ((origNumbers + count) == curNumbers))
      return false;

   return true;
   }


bool
OMR::CFG::setFrequencies()
   {
   if (this == comp()->getFlowGraph())
      {
      self()->resetFrequencies();
      }
   _max_edge_freq = MAX_PROF_EDGE_FREQ;

   if (comp()->getFlowGraph()->getStructure() && (comp()->getFlowGraph() == self()))
      {
      if (!self()->consumePseudoRandomFrequencies())
         {
         _max_edge_freq = MAX_STATIC_EDGE_FREQ;
         self()->setBlockAndEdgeFrequenciesBasedOnStructure();
         if (comp()->getOption(TR_TraceBFGeneration))
            comp()->dumpMethodTrees("Trees after setting frequencies from structures", comp()->getMethodSymbol());
         }

      return true;
      }

   return false;
   }


#define BOTH_SAME 0
#define FIRST_LARGER 1
#define SECOND_LARGER 2
#define BOTH_UNRELATED 3


int32_t OMR::CFG::compareExceptionSuccessors(TR::CFGNode *node1, TR::CFGNode *node2)
   {
   // Handle common cases first
   //
   if (node1->getExceptionSuccessors().empty() &&
       node2->getExceptionSuccessors().empty())
      return BOTH_SAME;

   if ((node1->getExceptionSuccessors().size() == 1) &&
       (node2->getExceptionSuccessors().size() == 1))
      {
      if (node1->getExceptionSuccessors().front() == node2->getExceptionSuccessors().front())
          return BOTH_SAME;
      }

   if (node2->getExceptionSuccessors().empty() &&
       (node1->getExceptionSuccessors().size() == 1))
      return FIRST_LARGER;

   if (node1->getExceptionSuccessors().empty() &&
       (node2->getExceptionSuccessors().size() == 1))
      return SECOND_LARGER;

   // This is not a common case; so create
   // bit vectors to be used to calculate the correct answer
   //
   int32_t numberOfNodes = comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector *firstSucc = new (trStackMemory()) TR_BitVector(numberOfNodes, trMemory(), stackAlloc);
   TR_BitVector *secondSucc = new (trStackMemory()) TR_BitVector(numberOfNodes, trMemory(), stackAlloc);
   TR_BitVector *comparison = new (trStackMemory()) TR_BitVector(numberOfNodes, trMemory(), stackAlloc);
   int32_t result = -1;

   for (auto nextEdge = node1->getExceptionSuccessors().begin(); nextEdge != node1->getExceptionSuccessors().end(); ++nextEdge)
      {
      TR::CFGNode *succ = (*nextEdge)->getTo();
      firstSucc->set(succ->getNumber());
      }

   for (auto nextEdge = node2->getExceptionSuccessors().begin(); nextEdge != node2->getExceptionSuccessors().end(); ++nextEdge)
      {
      TR::CFGNode *succ = (*nextEdge)->getTo();
      secondSucc->set(succ->getNumber());
      }

   if (*firstSucc == *secondSucc)
      result = BOTH_SAME;
   else
      {
      *comparison = *firstSucc;
      *comparison -= *secondSucc;
      if (comparison->isEmpty())
         result = SECOND_LARGER;
      else
         {
         *comparison = *secondSucc;
         *comparison -= *firstSucc;
         if (comparison->isEmpty())
            result = FIRST_LARGER;
         else
            result = BOTH_UNRELATED;
         }
      }

   return result;
   }

bool alwaysTrue(TR::CFGEdge * e)
   {
   return true;
   }


void TR::CFGNode::moveSuccessors(TR::CFGNode * to)
   {
   for (auto e = getSuccessors().begin(); e != getSuccessors().end(); ++e)
      (*e)->setFrom(to);
   getSuccessors().clear();
   }

void TR::CFGNode::movePredecessors(TR::CFGNode * to)
   {
   for (auto e = getPredecessors().begin(); e != getPredecessors().end(); ++e)
      (*e)->setTo(to);
   getPredecessors().clear();
   }

TR::Block * *
OMR::CFG::createArrayOfBlocks(TR_AllocationKind allocKind)
   {
   int32_t numberOfBlocks = getNextNodeNumber();
   TR::Block ** a = (TR::Block **) trMemory()->allocateMemory(numberOfBlocks * sizeof(TR::Block *), allocKind);
   memset(a, 0, numberOfBlocks * sizeof(TR::Block *));
   for (TR::CFGNode * n = getFirstNode(); n; n = n->getNext())
      a[n->getNumber()] = toBlock(n);
   return a;
   }


// Used for temp information in findLoopingBlocks()
class LoopInfo
   {
   public:
   LoopInfo(TR::Region &region) : isInLoop(region) {}
   int32_t top;
   int32_t bottom;
   TR_BitVector isInLoop;
   };

// Find all blocks that are in loops and return as a bit vector.
// This uses less memory than createBlockPredecessorBitVectors() when
// all that is needed is the looping blocks.
void OMR::CFG::findLoopingBlocks(TR_BitVector &loopingBlocks)
   {
   TR::Region workingSpace = TR::Region(comp()->trMemory()->heapMemoryRegion());
   int32_t numberOfBlocks = getNextNodeNumber();
   TR::deque<int32_t, TR::Region&> seenIndex(numberOfBlocks, workingSpace);
   TR::deque<TR::Block *, TR::Region&> stack(numberOfBlocks, workingSpace);
   typedef TR::typed_allocator<std::pair<int32_t const, LoopInfo*>, TR::Region&> LoopMapAllocator;
   typedef std::less<int32_t> LoopMapComparator;
   typedef std::map<int32_t, LoopInfo*, LoopMapComparator, LoopMapAllocator> LoopMap;
   LoopMap loops((LoopMapComparator()), LoopMapAllocator(workingSpace));
   int32_t i, j;
   const bool trace = false;
   vcount_t blockVisitCount = comp()->incVisitCount();
   TR::Block * block = toBlock(getStart()->getSuccessors().front()->getTo());
   for ( ; block; block = block->getNextBlock())
      {
      // Make sure every block gets looked at
      if (block->getVisitCount() == blockVisitCount)
       continue;

      int32_t lowestLoop = 0; // lowest loop in the table of loops
      int32_t top = 0;
      int32_t loopID = 0;
      stack[top] = block;
      for (i = numberOfBlocks-1; i >= 0; i--)
         seenIndex[i] = -1;
      while (top >= 0)
         {
       // Get the top block but don't pop it from the stack yet,
       // in case it has predecessors which have to be pushed on top of it
       int32_t index = top;
         TR::Block * b = stack[top];
         b->setVisitCount(blockVisitCount);
         int32_t number = b->getNumber();
         if (trace)
            dumpOptDetails(comp(), "Look at block %d index %d\n", number, index);
         seenIndex[number] = index;

         // Process the predecessors of this block
         TR_PredecessorIterator pi(b);
         for (TR::CFGEdge * e = pi.getFirst(); e; e = pi.getNext())
            {
            TR::Block * fromBlock = ::toBlock(e->getFrom());
            // If the predecessor has already been seen, there is a loop.
            int32_t n = fromBlock->getNumber();
            if (trace)
               dumpOptDetails(comp(), "Predecessor %d\n", n);
            if (seenIndex[n] >= 0)
               {
               // All existing loops can be collapsed into this loop
               if (trace)
                  dumpOptDetails(comp(), "Set up loop info for predecessor %d\n", n);
               if (lowestLoop > 0)
                  {
                  // There are existing loops on the stack. Collapse them all into
                  // a single loop
                  if (loops[lowestLoop]->bottom > seenIndex[n])
                     loops[lowestLoop]->bottom = seenIndex[n];
                  loops[lowestLoop]->top = index;
                  // Remove any loops that are subsumed by this loop, rolling in their
                  // list of blocks already marked as being in the loop.
                  // This is done iteratively to avoid removing entries while using a cursor.
                  do
                     {
                     i = 0;
                     for (auto itr = loops.begin(), end = loops.end(); itr != end; ++itr)
                        {
                        j = itr->first;
                        if (lowestLoop != j)
                           {
                           // Roll this loop into the new loop
                           loops[lowestLoop]->isInLoop |= itr->second->isInLoop;
                           i = j;  // Remove one entry at a time
                           break;
                           }
                        }
                     if (i > 0)
                        {
                        if (trace)
                           dumpOptDetails(comp(), "Remove loop based at %d\n", stack[loops[i]->bottom]->getNumber());
                        loops.erase(i);
                        }
                     } while (i > 0);
                  }
               else
                  {
                  // Create a new loop entry
                  LoopInfo *info = new (workingSpace.allocate(sizeof(LoopInfo))) LoopInfo(workingSpace);
                  info->bottom = seenIndex[n];
                  info->top = index;
                  lowestLoop = loopID++;
                  loops[lowestLoop] = info;
                  if (trace)
                     dumpOptDetails(comp(), "Create loop at %d up to %d\n", stack[n]->getNumber(), number);
                  }
               }
            else if (fromBlock->getVisitCount() == blockVisitCount)
               {
               // This predecessor has been looked at.
               // If it is a member of an existing loop whose bottom is still on the stack,
               // that loop can be extended to the current block
               for (auto itr = loops.begin(), end = loops.end(); itr != end; ++itr)
                  {
                  if (itr->second->isInLoop.get(n))
                     {
                     // Extend this loop up to the current block
                     itr->second->top = index;
                     if (trace)
                        dumpOptDetails(comp(), "Extend loop at %d up to %d\n", stack[itr->second->bottom]->getNumber(), number);
                     }
                  }
               }
            else
               {
               // This predecessor has not been looked at - push it
               if (trace)
                  dumpOptDetails(comp(), "Push predecessor %d\n", fromBlock->getNumber());
               stack[++top] = fromBlock;
               break;
               }
            }
         if (top == index)
            {
            // Nothing pushed, process this block - see if it is in a loop
            do
               {
               i = 0;
               for (auto itr = loops.begin(), end = loops.end(); itr != end; ++itr)
                  {
                  j = itr->first;
                  if (itr->second->top >= index)
                     {
                     if (trace)
                        dumpOptDetails(comp(), "Block %d in a loop based at block %d\n", number, stack[itr->second->bottom]->getNumber());
                     itr->second->isInLoop.set(number);
                     if (itr->second->bottom >= index)
                        {
                        i = j; // Mark the block for removal
                        break;
                        }
                     itr->second->top = index-1;
                     }
                  if (lowestLoop == 0 || lowestLoop > j)
                     lowestLoop = j;
                  }
               if (i > 0)
                  {
                  // This is the last block for this loop, free the loop entry
                  loopingBlocks |= loops[i]->isInLoop;
                  loops.erase(i);
                  if (i == lowestLoop)
                     lowestLoop = 0;
                  if (trace)
                     dumpOptDetails(comp(), "Remove loop based at %d\n", stack[loops[i]->bottom]->getNumber());
                  }
               } while (i > 0);
            seenIndex[number] = -1;
            top--;
            }
         }
      }
   }

// generate either a forward or backward reverse post-order traversal of the blocks in the CFG
// The traversal order array is always allocated anew since we don't know if the number of blocks
// in the CFG has changed since the last time we allocated the array.
// This function uses two parallel stacks to accomplish a post-order traversal: blockStack and iteratorStack.
// blockStack holds blocks whose out edges (either successors or predecessors depending on the traversal direction)
// haven't yet all been visited.  Blocks are pushed on the stack to be visited.  The iteratorStack holds the iterators for the out edges for the
// blocks on the blockStack.
int32_t OMR::CFG::createTraversalOrder(bool forward, TR_AllocationKind allocationKind, TR_BitVector *backEdges)
   {
   static const bool DERIVE_BACKWARD_FROM_FORWARD = true;
   vcount_t traversalVisitCount = comp()->incVisitCount();
   int32_t numberOfBlocks = getNextNodeNumber();

   /*
    * This is allocated before entering the new stack memory region so that the caller can allocate it on its stack scope if desired.
    */
   TR::CFGNode **traversalOrder = (TR::CFGNode **) trMemory()->allocateMemory(numberOfBlocks * sizeof(TR::CFGNode *), allocationKind, TR_MemoryBase::UnknownType);

   memset(traversalOrder, 0, numberOfBlocks * sizeof(TR::CFGNode *));
   int32_t traversalInsertPosition = numberOfBlocks;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   TR_Stack<TR::CFGNode *> blockStack(trMemory(), numberOfBlocks, false, stackAlloc);
   TR_Stack<TR_CFGIterator*> iteratorStack(trMemory(), numberOfBlocks, false, stackAlloc);
   TR_BitVector *blocksBeingVisited = new (trStackMemory()) TR_BitVector(numberOfBlocks, trMemory(), stackAlloc);

   // put either entry or exit in the stack to begin with
   TR::CFGNode *firstBlock;
   TR_CFGIterator *blockIter;
   if (DERIVE_BACKWARD_FROM_FORWARD || forward)
      {
      firstBlock = getStart();
      blockIter = new (trStackMemory()) TR_SuccessorIterator(firstBlock);
      }
   else
      {
      firstBlock = getEnd();
      blockIter = new (trStackMemory()) TR_PredecessorIterator(firstBlock);
      }

   blockIter->getFirst();
   blockStack.push(firstBlock->asBlock());
   blocksBeingVisited->set(firstBlock->asBlock()->getNumber());
   iteratorStack.push(blockIter);

   int32_t edgeCount = 0;
   while (!blockStack.isEmpty())
      {
      TR::CFGNode *block = blockStack.top();
      block->setVisitCount(traversalVisitCount);
      TR_CFGIterator *iter = iteratorStack.top();

      /* any fall through block within EBB? */
      TR::CFGEdge *fallThroughEdge = 0;
      if (DERIVE_BACKWARD_FROM_FORWARD && !forward)
         fallThroughEdge = block->asBlock()->getFallThroughEdgeInEBB();

      bool foundUnvisitedBlock = false;
      TR::CFGEdge *edge = iter->getCurrent();

      while (edge)
         {
         edge->setId(edgeCount++);

         if ((DERIVE_BACKWARD_FROM_FORWARD && !forward) &&
             edge == fallThroughEdge)
            {
            edge = iter->getNext();
            continue;
            }

         TR::CFGNode * nextBlock;
         if (DERIVE_BACKWARD_FROM_FORWARD || forward)
            nextBlock = ::toBlock(edge->getTo());
         else
            nextBlock = ::toBlock(edge->getFrom());
         if (nextBlock->getVisitCount() != traversalVisitCount)
            {
            // we're going to visit nextBlock, so push it onto blockStack
            blockStack.push(nextBlock);
            blocksBeingVisited->set(nextBlock->getNumber());

            // create the right kind of iterator for it and push it onto iteratorStack
            TR_CFGIterator *blockIter;
            if (DERIVE_BACKWARD_FROM_FORWARD || forward)
               blockIter = new (trStackMemory()) TR_SuccessorIterator(nextBlock);
            else
               blockIter = new (trStackMemory()) TR_PredecessorIterator(nextBlock);
            blockIter->getFirst();
            iteratorStack.push(blockIter);

            foundUnvisitedBlock = true;
            break;
            }
   else if (blocksBeingVisited->get(nextBlock->getNumber()))
            {
            if (_compilation->getOption(TR_TraceBFGeneration))
                dumpOptDetails(comp(), "\nFound backedge from %d to %d", edge->getFrom()->getNumber(), edge->getTo()->getNumber());
            if (backEdges)
               backEdges->set(edge->getId());

            }

         edge = iter->getNext();
         }

      /* Push fall thru block to stack last for forward traversal if we want proper order for ebb */
      if ((DERIVE_BACKWARD_FROM_FORWARD && !forward) &&
          !foundUnvisitedBlock &&
          fallThroughEdge)
         {
         TR::CFGNode * fallThroughBlock = ::toBlock(fallThroughEdge->getTo());
         if (fallThroughBlock->getVisitCount() != traversalVisitCount)
            {
            blockStack.push(fallThroughBlock);
            blocksBeingVisited->set(fallThroughBlock->getNumber());

            TR_CFGIterator *blockIter = new (trStackMemory()) TR_SuccessorIterator(fallThroughBlock);
            blockIter->getFirst();
            iteratorStack.push(blockIter);
            foundUnvisitedBlock = true;
            }
   else if (blocksBeingVisited->get(fallThroughBlock->getNumber()))
            {
            if (_compilation->getOption(TR_TraceBFGeneration))
                dumpOptDetails(comp(), "\nFound backedge from %d to %d",
                                           fallThroughEdge->getFrom()->getNumber(),
                                           fallThroughEdge->getTo()->getNumber());
            if (backEdges)
               backEdges->set(edge->getId());
            }

         }

      if (!foundUnvisitedBlock)
         {
         // we've visited all of the top block's out edges, so add to the traversal order and remove from the block stack
         TR_ASSERT(traversalInsertPosition > 0, "traversed more blocks than CFG claims to have");
         --traversalInsertPosition;
         TR_ASSERT(traversalOrder[traversalInsertPosition] == NULL, "inserted a block into traversal more than once");
         //dumpOptDetails(comp(),"traversal[%d] = %d\n", traversalInsertPosition, block->getNumber());
         traversalOrder[traversalInsertPosition] = block;

         blocksBeingVisited->reset(blockStack.top()->getNumber());
         blockStack.pop();
         iteratorStack.pop();
         }
      }

   //TR_ASSERT(traversalInsertPosition == 0, "not all blocks in CFG found by traversal, unreachable blocks should be removed first");
   if (traversalInsertPosition > 0)
      {
      // there are unreachable blocks so the traversal doesn't start at the beginning of the traversalOrder array
      // shift everything down by the number of empty positions
      //dumpOptDetails(comp(), "Shifting traversal array down by %d entries\n", traversalInsertPosition);
      int32_t i,j;
      for (i=traversalInsertPosition,j=0;i < numberOfBlocks;i++,j++)
         traversalOrder[j] = traversalOrder[i];
      // clear the upper entries in the traversal order array
      numberOfBlocks = j;
      }

   if (forward)
      {
      _forwardTraversalOrder = traversalOrder;
      _forwardTraversalLength = numberOfBlocks;
      for (int32_t idx=0;idx < numberOfBlocks;idx++)
         traversalOrder[idx]->setForwardTraversalIndex(idx);
      }
   else
      {
      if (DERIVE_BACKWARD_FROM_FORWARD)
         {
         for (int32_t idx=0;idx < numberOfBlocks;idx++)
            {
            if (idx > (numberOfBlocks - 1) - idx)
               break;
            TR::CFGNode *temp = traversalOrder[idx];
            traversalOrder[idx] = traversalOrder[numberOfBlocks - 1 - idx];
            traversalOrder[numberOfBlocks - 1 - idx] = temp;
            }
         }
      _backwardTraversalOrder = traversalOrder;
      _backwardTraversalLength = numberOfBlocks;
      for (int32_t idx=0;idx < numberOfBlocks;idx++)
         traversalOrder[idx]->setBackwardTraversalIndex(idx);
      }

   return edgeCount;
   }


class CFGNodeFrequencyPair
  {
  public:
  CFGNodeFrequencyPair() : _frequency(0), _node(NULL) {}

  int32_t       _frequency;
  TR::CFGNode *  _node;

  bool operator<(CFGNodeFrequencyPair &right)
      {
      return _frequency < right._frequency;
      }
  };

TR_BlockCloner *
OMR::CFG::clone()
   {
   TR::TreeTop *exitTree = comp()->findLastTree();
   TR::TreeTop *lastTreeTop = exitTree;

   // Remove Structure, we are not going to maintain it
   //
   setStructure(0);

   TR_BlockCloner *cloner = new (structureRegion()) TR_BlockCloner(self(), false, true);
   TR::Block *clonedBlock = cloner->cloneBlocks(comp()->getStartTree()->getNode()->getBlock(), lastTreeTop->getNode()->getBlock());
   lastTreeTop->join(clonedBlock->getEntry());

   return cloner;
   }


void
OMR::CFG::propagateColdInfo(bool generateFrequencies)
   {
   vcount_t visitCount = comp()->incVisitCount();
   TR_Queue<TR::Block> queue(trMemory());
   unsigned coldBlocks=0;
   bool  noPropagateSuperCold = false;
   bool  allBlocksAreSuperCold = true;
   bool  haveSuperColdBlocks  = false;

   static char *optName = feGetEnv("TR_NoPropagateSuperCold");
   if (optName)
      noPropagateSuperCold = true;

   // Forward Flow
   //
   if (generateFrequencies)
      {
      self()->setFrequencies();
      }

   bool haveStructure = (_rootStructure != NULL);

   if(comp()->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Propagating coldness forward\n");

   createTraversalOrder(true, stackAlloc);

   for (int b=0;b < _forwardTraversalLength;b++)
      {
      TR::CFGNode *block = _forwardTraversalOrder[b];
      if(comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(), "\tExamining block_%d\n", block->getNumber());

      if (!block->asBlock()->isCold() || (haveSuperColdBlocks && !block->asBlock()->isSuperCold() && !block->asBlock()->hasCallToSuperCold()))
         {
         TR_PredecessorIterator pit(block);
         bool allPredsCold = (pit.getFirst() != NULL);
         bool allPredsSuperCold = allPredsCold;
         TR::Block *curColdPred = NULL;
         int32_t coldFreq = -1;
         if (allPredsCold)
            for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
               {
               // Note : Disabled code below because we do not want to make a loop cold just because
               // there is a dominating block outside the loop that is marked cold
               //
               if (haveSuperColdBlocks && (block->getForwardTraversalIndex() <= edge->getFrom()->getForwardTraversalIndex()))
                  {
                  if(comp()->getOption(TR_TraceBFGeneration))
                     dumpOptDetails(comp(), "\t\tignoring backedge from %d\n", edge->getFrom()->getNumber());
                  continue;  // don't bother looking at loop backedges

                  }
               if(comp()->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(comp(),"\t\tpredecessor %d coldness %d\n", edge->getFrom()->getNumber(), (int)edge->getFrom()->asBlock()->isCold());
               TR::Block *fromBlock = edge->getFrom()->asBlock();
               if (!fromBlock->isSuperCold() && !fromBlock->hasCallToSuperCold())
                  {
                  allPredsSuperCold = false;
                  }
               if (!fromBlock->isCold())
                  {
                  allPredsCold = false;
                  break;
                  }
               else
                  {
                  if (curColdPred)
                     {
                     coldFreq = TR::Block::getMaxColdFrequency(curColdPred, fromBlock);
                     if (coldFreq == fromBlock->getFrequency())
                        curColdPred = fromBlock;
                     }
                  else
                     {
                     curColdPred = fromBlock;
                     coldFreq = fromBlock->getFrequency();
                     }
                  }
               }

         if (allPredsCold &&
             performTransformation(comp(),"%smarked block_%d cold (all preds were cold)\n", OPT_DETAILS, block->getNumber()))
            {
            block->asBlock()->setIsCold();
            if (allPredsSuperCold && !noPropagateSuperCold) block->asBlock()->setIsSuperCold();
            block->asBlock()->setFrequency(coldFreq);
            coldBlocks++;
            }
         }
         else
            coldBlocks++;
      }

   // Backward Flow
   //
   if(comp()->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Propagating coldness backward\n");

   createTraversalOrder(false, stackAlloc);

   for (int b=0;b < _backwardTraversalLength;b++)
      {
      TR::CFGNode *block = _backwardTraversalOrder[b];
      if(comp()->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"\tExamining block_%d\n", block->getNumber());
      if (!block->asBlock()->isCold() || (haveSuperColdBlocks && !block->asBlock()->isSuperCold() && !block->asBlock()->hasCallToSuperCold()))
         {
         TR_SuccessorIterator sit(block);
         bool allSuccsCold = (sit.getFirst() != NULL);
         bool allSuccsSuperCold = allSuccsCold;
         TR::Block *curColdSucc = NULL;
         int32_t coldFreq = -1;
         if (allSuccsCold)
            for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
               {
               if(comp()->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(comp(),"\t\tsuccessor %d coldness %d very coldness %d\n", edge->getTo()->getNumber(), (int)edge->getTo()->asBlock()->isCold(),(int)edge->getTo()->asBlock()->isSuperCold());
               TR::Block *toBlock = edge->getTo()->asBlock();
               if (!toBlock->isSuperCold() && !toBlock->hasCallToSuperCold())
                  {
                  allSuccsSuperCold = false;
                  allBlocksAreSuperCold = false;
                  }
               if (!toBlock->isCold())
                  {
                  allSuccsCold = false;
                  break;
                  }
               else
                  {
                  if (curColdSucc)
                     {
                     coldFreq = TR::Block::getMaxColdFrequency(curColdSucc, toBlock);
                     if (coldFreq == toBlock->getFrequency())
                        curColdSucc = toBlock;
                     }
                  else
                     {
                     curColdSucc = toBlock;
                     coldFreq = toBlock->getFrequency();
                     }
                  }
               }

         if (allSuccsCold &&
             (!comp()->ilGenTrace() ||
               performTransformation(comp(),"%smarked block_%d cold (all succs were cold)\n", OPT_DETAILS, block->getNumber())))
            {
            block->asBlock()->setIsCold();
            if (allSuccsSuperCold && !noPropagateSuperCold) block->asBlock()->setIsSuperCold();
            block->setFrequency(coldFreq);
            coldBlocks++;
            }
         }
      else
         coldBlocks++;
      }

   }


void
OMR::CFG::setUniformEdgeFrequenciesOnNode(TR::CFGNode *node, int32_t branchToCount, bool addFrequency, TR::Compilation *comp)
   {
   TR::CFG *cfg = self();
   TR::Block *block = node->asBlock();

   TR_BitVector *frequencySet = cfg->getFrequencySet();

   if (frequencySet)
      {
      if (!frequencySet->get(block->getNumber()))
         addFrequency = false;
      }

   int32_t size = node->getSuccessors().size();

   for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
      {
      if (addFrequency)
         (*e)->setFrequency( (*e)->getFrequency() + branchToCount );
      else
         (*e)->setFrequency( branchToCount );

      cfg->setEdgeProbability(*e, 1.0/(float)size);

      if (comp->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp,"Edge %p between %d and %d has freq %d (Uniform)\n", *e, (*e)->getFrom()->getNumber(), (*e)->getTo()->getNumber(), (*e)->getFrequency());
      }
   }


void
OMR::CFG::setEdgeFrequenciesOnNode(TR::CFGNode *node, int32_t branchToCount, int32_t fallThroughCount, TR::Compilation *comp)
   {
   TR::Block *block = node->asBlock();
   TR::Block *branchToBlock = block->getLastRealTreeTop()->getNode()->getBranchDestination()->getNode()->getBlock();

   for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
      {
      if ((*e)->getTo() == branchToBlock)
         {
         (*e)->setFrequency( branchToCount );
         comp->getFlowGraph()->setEdgeProbability(*e, (float)branchToCount/(float)(branchToCount + fallThroughCount));
         }
      else
         {
         (*e)->setFrequency( fallThroughCount );
         comp->getFlowGraph()->setEdgeProbability(*e, (float)fallThroughCount/(float)(branchToCount + fallThroughCount));
         }

      if (comp->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp, "Edge %p between %d and %d has freq %d\n", *e, (*e)->getFrom()->getNumber(), (*e)->getTo()->getNumber(), (*e)->getFrequency());
      }
   }


bool
OMR::CFG::setEdgeFrequenciesFrom()
   {
   //bool didWeHaveAnyDataAtAll = false;
   // Set the entry edge frequency to MAX
   if (getStructure())
      {
      TR_RegionStructure *region = getStructure()->asRegion();
      if (region)
         {
         TR::Block *start = region->getEntryBlock();

         TR_SuccessorIterator sit(start);
         for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
            edge->setFrequency(_max_edge_freq);
         }
      }

   // Set all that are certain
   TR::CFGNode *node;
   for (node = getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = node->asBlock();

      if (block->isCold())
         {
         //self()->setEdgeFrequenciesOnNode( node, 0, 0, _compilation);
         continue;
         }

      if (block->getEntry()!=NULL &&
          (node->getSuccessors().size() == 2) &&
          block->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
         {
         int32_t fallThroughCount = 0;
         int32_t branchToCount = 0;
         TR::Node *ifNode = block->getLastRealTreeTop()->getNode();
         self()->getBranchCounters(ifNode, block, &branchToCount, &fallThroughCount, comp());

         if (fallThroughCount || branchToCount)
            {
            self()->setEdgeFrequenciesOnNode( node, branchToCount, fallThroughCount, _compilation);
            //didWeHaveAnyDataAtAll = true;
            }
         else if (ifNode->isTheVirtualGuardForAGuardedInlinedCall())
            {
            self()->setEdgeFrequenciesOnNode( node, 0, _max_edge_freq, _compilation);
            }
         else if (!block->isCold())
            {
            self()->setEdgeFrequenciesOnNode( node, _max_edge_freq, _max_edge_freq, _compilation);
            }
         }
      else if ( block->getEntry()!=NULL &&
              !((node->getSuccessors().size() == 1) || (node->getSuccessors().size() == 2)) )
         {
         self()->setUniformEdgeFrequenciesOnNode(node, _max_edge_freq, false, _compilation);
         }
      }

   // go forward and set the GOTOs
   for (node = getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = node->asBlock();
      if (block->isCold())
         continue;

      if (block->getEntry()!=NULL &&
          (node->getSuccessors().size() == 1) &&
          node->getSuccessors().front()->getTo() &&
          block->hasSuccessor(node->getSuccessors().front()->getTo()))
         {
         int32_t edgeFrequency = 0;
         for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
            edgeFrequency += (*e)->getFrequency();
         if (edgeFrequency > _max_edge_freq) edgeFrequency = _max_edge_freq;

         TR::CFGEdge * es = block->getSuccessors().front();
         es->setFrequency(edgeFrequency);

         if (_compilation->getOption(TR_TraceBFGeneration))
            dumpOptDetails(comp(), "Edge %p between %d and %d has freq %d (GOTO forward pass)\n", es, es->getFrom()->getNumber(), es->getTo()->getNumber(), es->getFrequency());
         }
      }

   // go backward and set the GOTOs
   for (node = getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = node->asBlock();

      if (block->isCold())
         continue;

      for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
         {
         if ((*e)->getFrequency()>0)
            continue;

         TR::Block *pred = (*e)->getFrom()->asBlock();
         if (pred->getEntry()!=NULL &&
             (pred->getSuccessors().size() == 1) &&
             pred->getSuccessors().front()->getTo() &&
             pred->hasSuccessor(pred->getSuccessors().front()->getTo()))
            {
            int32_t edgeFrequency = 0;
            for (auto es = block->getSuccessors().begin(); es != block->getSuccessors().end(); ++es)
               edgeFrequency += (*es)->getFrequency();
            if (edgeFrequency > _max_edge_freq) edgeFrequency = _max_edge_freq;

            (*e)->setFrequency(edgeFrequency);
            if (_compilation->getOption(TR_TraceBFGeneration))
               dumpOptDetails(comp(), "Edge %p between %d and %d has freq %d (GOTO backward pass)\n", *e, (*e)->getFrom()->getNumber(), (*e)->getTo()->getNumber(), (*e)->getFrequency());
            }
         }
      }

   //return didWeHaveAnyDataAtAll;
   return true;
   }

void
OMR::CFG::computeEntryFactorsFrom(TR_Structure *str, float &factor)
   {
   if (!str) return;
   TR_RegionStructure *region = str->asRegion();
   if (region == NULL)
      {
      return;
      }

   if (str == getStructure())
      region->setFrequencyEntryFactor (1.0f);
   else
      region->setFrequencyEntryFactor (0.0f);

   float initialFactor = factor;
   float maxSubFactor = factor;
   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node != NULL; node = it.getNext())
      {
      factor = initialFactor;
      computeEntryFactorsFrom(node->getStructure(), factor);
      if (factor > maxSubFactor)
         maxSubFactor = factor;
      }

   // we don't propagate backwards edges in improper regions for now
   if (region->containsInternalCycles())
      computeEntryFactorsAcyclic(region);
   else if (region->isNaturalLoop())
      computeEntryFactorsLoop(region);
   else
      computeEntryFactorsAcyclic(region);

   factor = maxSubFactor*region->getFrequencyEntryFactor();
   //dumpOptDetails(comp(),"maxFactor at %d is %f\n", str->getNumber(), factor);
   }

void
OMR::CFG::propagateEntryFactorsFrom(TR_Structure *str, float factor)
   {
   if (!str) return;
   TR_RegionStructure *region = str->asRegion();
   if (region == NULL)
      {
      TR::Block *block = str->getEntryBlock();
      if (!block->isCold())
         {
         int32_t frequency = (int32_t)((float)block->getFrequency()*factor);
         int32_t delta = (frequency * MAX_BLOCK_COUNT)/_maxFrequency;
         if (delta == 0)
            delta = 1;
         frequency = MAX_COLD_BLOCK_COUNT + delta;
         block->setFrequency(frequency);
         }

      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"Set block frequency on block_%d to %d, current factor %lf\n", block->getNumber(), block->getFrequency(), factor);
      return;
      }

   factor *= region->getFrequencyEntryFactor();

   if (factor > MAX_REGION_FACTOR) factor = MAX_REGION_FACTOR;

   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node != NULL; node = it.getNext())
      {
      propagateEntryFactorsFrom(node->getStructure(), factor);
      }
   }

float
OMR::CFG::computeOutsideEdgeFactor(TR::CFGEdge *outEdge, TR::CFGNode *pred)
   {
   float factor = (float)pred->asBlock()->getFrequency() / (float) INITIAL_BLOCK_FREQUENCY_FACTOR;

   int32_t sumEdgeFreq = 0;
   TR_SuccessorIterator sit(pred);
   for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
      sumEdgeFreq += edge->getFrequency();

   if (!sumEdgeFreq) sumEdgeFreq = 1;
   factor *= ((float)outEdge->getFrequency()/(float)sumEdgeFreq);

   return factor;
   }

float
OMR::CFG::computeInsideEdgeFactor(TR::CFGEdge *inEdge, TR::CFGNode *pred)
   {
   float factor = (float)pred->asBlock()->getFrequency() / (float) INITIAL_BLOCK_FREQUENCY_FACTOR;

   //dumpOptDetails(comp(),"For pred %d inEdgeFrequency %d block freq %d factor %f\n", pred->getNumber(), inEdge->getFrequency(), pred->asBlock()->getFrequency(), factor);

   int32_t sumEdgeFreq = 0;
   TR_SuccessorIterator sit(pred);
   for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
      {
      if (edge != inEdge)
         sumEdgeFreq += edge->getFrequency();
      }

   if (!sumEdgeFreq) sumEdgeFreq = 1;
   factor *= ((float)inEdge->getFrequency()/(float)sumEdgeFreq);

   return factor;
   }

void
OMR::CFG::computeEntryFactorsAcyclic(TR_RegionStructure *region)
   {
   float factor = region->getFrequencyEntryFactor ();
   TR::Block *start = region->getEntryBlock();

   TR_PredecessorIterator pit(start);
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR::CFGNode *pred = edge->getFrom();

      if (edge->getFrequency()<=0)
         continue;

      factor += computeOutsideEdgeFactor(edge, pred);
      }

   region->setFrequencyEntryFactor (factor);

   if (_compilation->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Setting factor of %lf on region %d \n", factor, region->getNumber());
   }

void
OMR::CFG::computeEntryFactorsLoop(TR_RegionStructure *region)
   {
   float factor = region->getFrequencyEntryFactor ();
   TR::Block *start = region->getEntryBlock();

   TR_PredecessorIterator pit(start);
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR::CFGNode *pred = edge->getFrom();
      bool        isBackEdge = false;

      if (edge->getFrequency()<=0)
         continue;

      isBackEdge = region->contains(pred->asBlock()->getStructureOf(), this->getStructure());

      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"For loop %d pred %d isBackEdge %d\n", region->getNumber(), pred->getNumber(), isBackEdge);

      if (!isBackEdge)
         factor += computeOutsideEdgeFactor(edge, pred);
      else
         {
         if (pred->asBlock()->getSuccessors().size() == 1)
            edge->setFrequency(_max_edge_freq);
         factor += computeInsideEdgeFactor(edge, pred);
         }
      }

   region->setFrequencyEntryFactor (factor);

   if (_compilation->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Setting factor of %lf on region %d \n", factor, region->getNumber());
   }

void
OMR::CFG::propagateFrequencyInfoFrom(TR_Structure *str)
   {
   if (!str) return;
   TR_RegionStructure *region = str->asRegion();
   if (region == NULL)
      {
      return;
      }

   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node != NULL; node = it.getNext())
      {
      propagateFrequencyInfoFrom(node->getStructure());
      }

   // we don't propagate backwards edges in improper regions for now
   if (region->containsInternalCycles())
      processAcyclicRegion(region);
   else if (region->isNaturalLoop())
      processNaturalLoop(region);
   else
      processAcyclicRegion(region);
   }

void
OMR::CFG::processAcyclicRegion(TR_RegionStructure *region)
   {
   walkStructure(region);
   }

void
OMR::CFG::processNaturalLoop(TR_RegionStructure *region)
   {
   walkStructure(region);
   }

static void
   setFrequencyOnSuccessor(TR_RegionStructure *region,
                           TR::CFGNode *succ,
                           TR::CFGNode *start,
                           int32_t succNum,
                           int32_t sumEdgeFrequency,
                           int32_t parentFrequency,
                           int32_t *_regionFrequencies,
                           ListIterator<TR::CFGEdge> i,
                           TR::Compilation *_compilation,
                           int32_t max_edge_freq)
   {
   if (toStructureSubGraphNode(succ)->getStructure() &&
       toStructureSubGraphNode(succ)->getStructure()->getParent() &&
       toStructureSubGraphNode(succ)->getStructure()->getParent() == region)
      {
      int32_t thisEdgeFrequency = 0;

      // don't add up loop backedges now, they will be factored in
      if (succ->getNumber() == start->getNumber())
         return;

      for (TR::CFGEdge * e = i.getFirst(); e; e = i.getNext())
         if (e->getTo()->getNumber() == succ->getNumber())
            {
            thisEdgeFrequency = e->getFrequency();
            break;
            }

      int32_t factoredFrequency = (int32_t)((float)parentFrequency * (float)thisEdgeFrequency/(float)sumEdgeFrequency);

      TR::Block *succBlock = NULL;

      if (toStructureSubGraphNode(succ)->getStructure()->asBlock())
          succBlock = toStructureSubGraphNode(succ)->getStructure()->asBlock()->getBlock();

      // now set the block frequencies, in reality this should set another field on the block frequency info
      // something like a temporal frequency
      if ((thisEdgeFrequency ==   max_edge_freq) &&
          (sumEdgeFrequency  ==   succNum * max_edge_freq))
         {
         if (succBlock)
            {
            if (!succBlock->isCold())
               {
               succBlock->setFrequency(INITIAL_BLOCK_FREQUENCY_FACTOR);
               if (_compilation->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(_compilation,"Setting frequency of %d on block_%d (to block)\n", succBlock->getFrequency(), succBlock->getNumber());
               }
            }
         else
            {
            _regionFrequencies[succ->getNumber()] = INITIAL_BLOCK_FREQUENCY_FACTOR;
            if (_compilation->getOption(TR_TraceBFGeneration))
               dumpOptDetails(_compilation,"Setting frequency of %d on region %d (to block)\n", _regionFrequencies[succ->getNumber()], succ->getNumber());
            }
         }
      else
         {
         if (succBlock)
            {
            if (!succBlock->isCold())
               {
               if ((succBlock->getFrequency() + factoredFrequency) > INITIAL_BLOCK_FREQUENCY_FACTOR )
                  succBlock->setFrequency(INITIAL_BLOCK_FREQUENCY_FACTOR);
               else
                  succBlock->setFrequency(succBlock->getFrequency()+ factoredFrequency);

               if (_compilation->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(_compilation,"Setting frequency of %d on block_%d (to block)\n", succBlock->getFrequency(), succBlock->getNumber());
               }
            }
         else
            {
            if ((_regionFrequencies[succ->getNumber()] + factoredFrequency) > INITIAL_BLOCK_FREQUENCY_FACTOR)
                _regionFrequencies[succ->getNumber()] = INITIAL_BLOCK_FREQUENCY_FACTOR;
            else
                _regionFrequencies[succ->getNumber()] += factoredFrequency;

            if (_compilation->getOption(TR_TraceBFGeneration))
               dumpOptDetails(_compilation,"Setting frequency of %d on region %d (to block)\n", _regionFrequencies[succ->getNumber()], succ->getNumber());
            }
         }
      }
   }

static void
   summarizeEdgeFrequencies(ListIterator<TR::CFGEdge> i, int32_t *sumEdgeFreq, int32_t *edgeCount)
   {
   for (TR::CFGEdge * e = i.getFirst(); e; e = i.getNext())
       {
       (*sumEdgeFreq) += e->getFrequency();
       (*edgeCount)++;
       }
   }

/* This will replace the function above, once the other callees are using STL Lists as Argument 1
static void
   summarizeEdgeFrequencies(TR::CFGEdgeList list, int32_t *sumEdgeFreq, int32_t *edgeCount)
   {
   for (auto e = list.begin(); e != list.end(); ++e)
       {
       (*sumEdgeFreq) += (*e)->getFrequency();
       (*edgeCount)++;
       }
   }
*/
static void
   addAllInnerBlocksToLoop(TR_RegionStructure *region,
                           TR_SuccessorIterator sit,
                           TR_BitVector *_seenNodes,
                           TR_ScratchList<TR::CFGNode> *stack,
                           TR::Compilation *_compilation)
   {
   for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
       {
       TR::CFGNode *succ = edge->getTo();

       if (!_seenNodes->isSet(succ->getNumber()) &&
           toStructureSubGraphNode(succ)->getStructure() &&
           toStructureSubGraphNode(succ)->getStructure()->getParent() &&
           toStructureSubGraphNode(succ)->getStructure()->getParent() == region)
          {
          stack->add(succ);
          if (_compilation->getOption(TR_TraceBFGeneration))
             dumpOptDetails(_compilation,"Added block(or region) %d to the walk\n", succ->getNumber());
          }
      }
   }

static void
   generalFrequencyPropagation(TR_RegionStructure *region,
                               TR_SuccessorIterator sit,
                               TR::Block *block,
                               TR_BitVector *_seenNodes,
                               int32_t *_regionFrequencies,
                               TR_ScratchList<TR::CFGNode> *stack,
                               TR::Compilation *_compilation)
   {
   for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
      {
      TR::CFGNode *succ = edge->getTo();

      if (!_seenNodes->isSet(succ->getNumber()) &&
          toStructureSubGraphNode(succ)->getStructure() &&
          toStructureSubGraphNode(succ)->getStructure()->getParent() &&
          toStructureSubGraphNode(succ)->getStructure()->getParent() == region)
         {
         if (toStructureSubGraphNode(succ)->getStructure()->asBlock())
            {
            TR::Block *succBlock = toStructureSubGraphNode(succ)->getStructure()->asBlock()->getBlock();
            if (!succBlock->isCold())
               {
               succBlock->setFrequency(block->getFrequency());
               if (_compilation->getOption(TR_TraceBFGeneration))
                  dumpOptDetails(_compilation,"Setting frequency of %d on block_%d (switch or lookup)\n", succBlock->getFrequency(), succBlock->getNumber());
               }
            }
         else
            {
            _regionFrequencies[succ->getNumber()] = block->getFrequency();
            if (_compilation->getOption(TR_TraceBFGeneration))
                dumpOptDetails(_compilation,"Setting frequency of %d on region %d (switch or lookup)\n", _regionFrequencies[succ->getNumber()], succ->getNumber());
            }
         stack->add(succ);
         if (_compilation->getOption(TR_TraceBFGeneration))
             dumpOptDetails(_compilation,"Added block_%d to the walk\n", succ->getNumber());
         }
      }
   }


int32_t
OMR::CFG::getLowFrequency()
   {
   return LOW_FREQ;
   }

int32_t
OMR::CFG::getAvgFrequency()
   {
   return AVG_FREQ;
   }


void
OMR::CFG::walkStructure(TR_RegionStructure *region)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   bool _foundIllegalStructure = false;
   int32_t numBlocks = comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector *_seenNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   TR_StructureSubGraphNode *start = region->getEntry();

   TR_BlockStructure *startBlock = toStructureSubGraphNode(start)->getStructure()->asBlock();

   if (_compilation->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"Looking at region num = %d\n", region->getNumber());

   if (startBlock)
      {
      if (startBlock->getBlock()->isCold())
         {
         if (_compilation->getOption(TR_TraceBFGeneration))
            dumpOptDetails(comp(),"\tIgnoring because %d is cold\n", startBlock->getBlock()->getNumber());
         return;
         }

      startBlock->getBlock()->setFrequency (INITIAL_BLOCK_FREQUENCY_FACTOR);
      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"Setting frequency of %d on block_%d (entry)\n", INITIAL_BLOCK_FREQUENCY_FACTOR, startBlock->getBlock()->getNumber());
      }

   int numRegions = getNextNodeNumber();
   int32_t *_regionFrequencies = (int32_t *)trMemory()->allocateStackMemory(numRegions * sizeof(int32_t));
   for (int32_t k = 0; k< numRegions; k++) _regionFrequencies[k] = INITIAL_BLOCK_FREQUENCY_FACTOR;

   // Walk reverse post-order
   //
   TR_ScratchList<TR::CFGNode> stack(trMemory());
   stack.add(start);

   while (!stack.isEmpty() && !_foundIllegalStructure)
      {
      TR::CFGNode *node = stack.popHead();
      TR_SuccessorIterator sit(node);

      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"-> %d ", node->getNumber());

      if (_seenNodes->isSet(node->getNumber()))
         {
         if (_compilation->getOption(TR_TraceBFGeneration))
            dumpOptDetails(comp(),"\tI've seen %d, continuing...\n", node->getNumber());
         continue;
         }

      TR_RegionStructure *regionStr = toStructureSubGraphNode(node)->getStructure()->asRegion();
      if (regionStr != NULL)
         {
         _seenNodes->set(node->getNumber());
         if (_compilation->getOption(TR_TraceBFGeneration))
            dumpOptDetails(comp(),"\tSet %d as seen\n", node->getNumber());

         List<TR::CFGEdge> rEdges(trMemory());
         List<TR::Block> rBlocks(trMemory());
         regionStr->collectExitBlocks(&rBlocks, &rEdges);

         int32_t parentFrequency = _regionFrequencies[node->getNumber()];
         int32_t sumEdgeFrequency = 0;
         int32_t succNum = 0;
         ListIterator<TR::CFGEdge> i(&rEdges);

         summarizeEdgeFrequencies(i, &sumEdgeFrequency, &succNum);

         if (sumEdgeFrequency > 0)
            {
            for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
               {
               TR::CFGNode *succ = edge->getTo();

               setFrequencyOnSuccessor(region, succ, start, succNum, sumEdgeFrequency, parentFrequency, _regionFrequencies, i, _compilation, _max_edge_freq);
               }
            }

         addAllInnerBlocksToLoop(region, sit, _seenNodes, &stack, _compilation);
         continue;
         }

      _seenNodes->set(node->getNumber());
      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"\tSet %d as seen\n", node->getNumber());

      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"\tVisiting block region %d\n", toStructureSubGraphNode(node)->getStructure()->asBlock()->getNumber());

      TR::Block *block = toStructureSubGraphNode(node)->getStructure()->asBlock()->getBlock();

      if (block->getEntry())
         {
         if (_compilation->getOption(TR_TraceBFGeneration))
            dumpOptDetails(comp(),"At block_%d\n", block->getNumber());

         if ((block->getSuccessors().size() == 2) &&
             block->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
            {
            int32_t sumEdgeFrequency = 0;
            int32_t succNum = 0;
            // Need a temporary List to pass into setFrequencyOnSuccessor, and summarizeEdgeFrequencies
            // will be removed as part of the STL List replacement process
            List<TR::CFGEdge> temp(comp()->trMemory());
            for (auto iter = block->getSuccessors().begin(); iter != block->getSuccessors().end(); ++iter)
               temp.add(*iter);
            ListIterator<TR::CFGEdge> i(&temp);

            summarizeEdgeFrequencies(i, &sumEdgeFrequency, &succNum);

            if (sumEdgeFrequency > 0)
               {
               int32_t  parentFrequency = block->getFrequency();
               for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
                  {
                  TR::CFGNode *succ = edge->getTo();

                  setFrequencyOnSuccessor(region, succ, start, succNum, sumEdgeFrequency, parentFrequency, _regionFrequencies, i, _compilation, _max_edge_freq);
                  }
               }

            addAllInnerBlocksToLoop(region, sit, _seenNodes, &stack, _compilation);
            }
         else if ((block->getSuccessors().size() == 1) &&
        		 block->getSuccessors().front()->getTo()              &&
                  block->hasSuccessor(block->getSuccessors().front()->getTo()))
            {
            // for GOTO blocks just propagate the block frequency info, edge frequency is unknown
            int32_t blockFreq = block->getFrequency();

            TR::CFGNode *succ = sit.getFirst()->getTo();

            if (toStructureSubGraphNode(succ)->getStructure() &&
                toStructureSubGraphNode(succ)->getStructure()->getParent() &&
                toStructureSubGraphNode(succ)->getStructure()->getParent() == region)
               {
               if (!_seenNodes->isSet(succ->getNumber()))
                  {
                  if (_compilation->getOption(TR_TraceBFGeneration))
                     dumpOptDetails(comp(), "Added block_%d to the walk\n", succ->getNumber());
                  stack.add(succ);
                  }

               TR::Block *branchToBlock = NULL;

               if (toStructureSubGraphNode(succ)->getStructure()->asBlock())
                  branchToBlock = toStructureSubGraphNode(succ)->getStructure()->asBlock()->getBlock();

               if (branchToBlock)
                  {
                  if (!branchToBlock->isCold())
                     {
                     if ((branchToBlock->getFrequency() + blockFreq) > INITIAL_BLOCK_FREQUENCY_FACTOR )
                        {
                        if (_compilation->getOption(TR_TraceBFGeneration))
                           dumpOptDetails(comp(),"Setting frequency of %d on block_%d (GOTO block) - MAX block frequency\n", INITIAL_BLOCK_FREQUENCY_FACTOR, branchToBlock->getNumber());
                        branchToBlock->setFrequency( INITIAL_BLOCK_FREQUENCY_FACTOR );
                        }
                     else
                        {
                        branchToBlock->setFrequency( branchToBlock->getFrequency() + blockFreq);
                        if (_compilation->getOption(TR_TraceBFGeneration))
                           dumpOptDetails(comp(), "Adding frequency of %d on block_%d (GOTO block), prev frequency was %d\n", blockFreq, branchToBlock->getNumber(),branchToBlock->getFrequency() - blockFreq);
                        }
                     }
                  }
               else
                  {
                  if ((_regionFrequencies[succ->getNumber()] + blockFreq) > INITIAL_BLOCK_FREQUENCY_FACTOR)
                     _regionFrequencies[succ->getNumber()] = INITIAL_BLOCK_FREQUENCY_FACTOR;
                  else
                     _regionFrequencies[succ->getNumber()] += blockFreq;

                  if (_compilation->getOption(TR_TraceBFGeneration))
                     dumpOptDetails(comp(),"Setting frequency of %d on region %d (GOTO block)\n", _regionFrequencies[succ->getNumber()], succ->getNumber());
                  }
               }

            int32_t edgeFrequency = 0;
            for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
               edgeFrequency += (*e)->getFrequency();
            if (edgeFrequency > _max_edge_freq) edgeFrequency = _max_edge_freq;

            TR::CFGEdge * es = block->getSuccessors().front();
            es->setFrequency(edgeFrequency);

            if (_compilation->getOption(TR_TraceBFGeneration))
               dumpOptDetails(_compilation,"Edge %p between %d and %d has freq %d (GOTO)\n", es, es->getFrom()->getNumber(), es->getTo()->getNumber(), es->getFrequency());
            }
         else
            {
            generalFrequencyPropagation(region, sit, block, _seenNodes, _regionFrequencies, &stack, _compilation);
            }
         }
      else
         {
         generalFrequencyPropagation(region, sit, block, _seenNodes, _regionFrequencies, &stack, _compilation);
         }
      }
   }


void
OMR::CFG::setBlockAndEdgeFrequenciesBasedOnStructure()
   {
   if (_compilation->getOption(TR_TraceBFGeneration))
      dumpOptDetails(comp(),"\nsetBlockAndEdgeFrequenciesBasedOnStructure: Setting edge frequencies...\n");

   bool setBlockFrequencies = self()->setEdgeFrequenciesFrom(); // also sets edge probabilities

   if (!getStructure())
      return;

   if (setBlockFrequencies)
      {
      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"\nsetBlockAndEdgeFrequenciesBasedOnStructure: Propagating block and edge frequencies within regions...\n");

      if (getStructure())
         {
         TR::CFGNode *node;
         for (node = getFirstNode(); node; node = node->getNext())
            {
            int32_t frequency = node->getFrequency();
            if ((frequency < 0) ||
                (frequency > MAX_COLD_BLOCK_COUNT))
               node->setFrequency(MAX_COLD_BLOCK_COUNT+1);
            }
         }

      propagateFrequencyInfoFrom(getStructure());

      {
      TR::StackMemoryRegion stackMemoryRegion(*trMemory());
      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"\nsetBlockAndEdgeFrequenciesBasedOnStructure: Computing region weight factors based on CFG structure, num regions=%d...\n",comp()->getFlowGraph()->getNextNodeNumber());

      float maxFactor = 1.0f;
      computeEntryFactorsFrom(getStructure(), maxFactor);
      if (maxFactor > MAX_REGION_FACTOR)
         maxFactor = MAX_REGION_FACTOR;

      if (_compilation->getOption(TR_TraceBFGeneration))
         dumpOptDetails(comp(),"\nsetBlockAndEdgeFrequenciesBasedOnStructure: Propagating weight factors based on CFG structure...\n");

      _maxFrequency = (int32_t) ((float) maxFactor*INITIAL_BLOCK_FREQUENCY_FACTOR);
      //dumpOptDetails(comp(),"max factor %f max freq %d\n", maxFactor, _maxFrequency);
      propagateEntryFactorsFrom(getStructure(), 1.0f);
      scaleEdgeFrequencies();
      }

      }
   }


void OMR::CFG::getBranchCounters(TR::Node *node, TR::Block *block, int32_t *taken, int32_t *notTaken, TR::Compilation *comp)
   {
   TR::Block *branchToBlock = node->getBranchDestination()->getNode()->getBlock();
   TR::Block *fallThroughBlock = block->getNextBlock();
   int32_t branchToFrequency = block->getEdge(branchToBlock)->getFrequency();
   int32_t fallThroughFrequency = block->getEdge(fallThroughBlock)->getFrequency();

   if (((branchToBlock->getFrequency() >= 0) && (fallThroughBlock->getFrequency() >= 0)) &&
       (((branchToFrequency > 0) && (fallThroughFrequency >= 0)) ||
        ((fallThroughFrequency > 0) && (branchToFrequency >= 0))))
     {
     int32_t blockFreq = block->getFrequency();
     if (block->getFrequency() <= 0)
        blockFreq = 1;

     *taken = branchToFrequency; //(branchToFrequency*blockFreq)/(branchToFrequency + fallThroughFrequency);
     *notTaken = fallThroughFrequency; //(fallThroughFrequency*blockFreq)/(branchToFrequency + fallThroughFrequency);

     if (comp->getOption(TR_TraceBFGeneration))
        traceMsg(comp, "taken %d NOT taken %d branch %d fall through %d  block freq %d\n", *taken, *notTaken, branchToFrequency, fallThroughFrequency, blockFreq);

     if (*taken > _max_edge_freq)
        *taken = _max_edge_freq;
     if (*notTaken > _max_edge_freq)
        *notTaken = _max_edge_freq;

     int32_t rawScalingFactor = _oldMaxEdgeFrequency;
     if (rawScalingFactor < 0)
        rawScalingFactor = _maxEdgeFrequency;

     if (comp->getOption(TR_TraceBFGeneration))
        traceMsg(comp, "raw scaling %d max edge %d old max edge %d\n", rawScalingFactor, _maxEdgeFrequency, _oldMaxEdgeFrequency);

     if (rawScalingFactor > 0)
        {
        if (*taken > MAX_COLD_BLOCK_COUNT)
           {
           *taken = (rawScalingFactor*(*taken))/(MAX_COLD_BLOCK_COUNT+MAX_BLOCK_COUNT);
           }

        if (*notTaken > MAX_COLD_BLOCK_COUNT)
           {
           *notTaken = (rawScalingFactor*(*notTaken))/(MAX_COLD_BLOCK_COUNT+MAX_BLOCK_COUNT);
           }
        }
      return;
      }
   else if ((branchToBlock->getPredecessors().size() == 1) &&
            (fallThroughBlock->getPredecessors().size() == 1) &&
            (((branchToBlock->getFrequency() > 0) && (fallThroughBlock->getFrequency() >= 0)) ||
             ((fallThroughBlock->getFrequency() > 0) && (branchToBlock->getFrequency() >= 0))))
      {
      int32_t blockFreq = block->getFrequency();
      if (block->getFrequency() <= 0)
         blockFreq = 1;

      *taken = branchToBlock->getFrequency(); // (branchToBlock->getFrequency()*blockFreq)/(branchToBlock->getFrequency() + fallThroughBlock->getFrequency());
      *notTaken = fallThroughBlock->getFrequency(); //(fallThroughBlock->getFrequency()*blockFreq)/(branchToBlock->getFrequency() + fallThroughBlock->getFrequency());

      if (*taken > _max_edge_freq)
         *taken = _max_edge_freq;

      if (*notTaken > _max_edge_freq)
         *notTaken = _max_edge_freq;

      int32_t rawScalingFactor = _oldMaxFrequency;

      if (rawScalingFactor < 0)
         rawScalingFactor = _maxFrequency;

      if (comp->getOption(TR_TraceBFGeneration))
         traceMsg(comp, "raw scaling %d max %d old max %d\n", rawScalingFactor, _maxFrequency, _oldMaxFrequency);

      if (rawScalingFactor > 0)
         {
         if (*taken > MAX_COLD_BLOCK_COUNT)
            {
            *taken = (rawScalingFactor*(*taken))/(MAX_COLD_BLOCK_COUNT+MAX_BLOCK_COUNT);
            }

         if (*notTaken > MAX_COLD_BLOCK_COUNT)
           {
            *notTaken = (rawScalingFactor*(*notTaken))/(MAX_COLD_BLOCK_COUNT+MAX_BLOCK_COUNT);
            }
         }
      return;
      }
   else if (self()->hasBranchProfilingData())
      {
      self()->getBranchCountersFromProfilingData(node, block, taken, notTaken);
      return;
      }
   else if (getStructure())
      {
      TR_BlockStructure *blockStructure = block->getStructureOf();
      TR_RegionStructure *parent = blockStructure->getParent();
      while (parent)
         {
         bool setFreqs = false;
         if (parent->isNaturalLoop())
            {
            if (node->getOpCode().isIf())
               {
               TR::Block *branchToBlock = node->getBranchDestination()->getNode()->getBlock();
               TR::Block *fallThroughBlock = block->getNextBlock();
               bool loopContainsBranchTo = parent->contains(branchToBlock->getStructureOf(), getStructure());
               bool loopContainsFallThrough = parent->contains(fallThroughBlock->getStructureOf(), getStructure());

               //dumpOptDetails(comp(), "block_%d branchToBlock %d fallThroughBlock %d\n", block->getNumber(), branchToBlock->getNumber(), fallThroughBlock->getNumber());

               // Handling loop exits
               if (loopContainsBranchTo && !loopContainsFallThrough)
                  {
                  //dumpOptDetails(comp(),"Taken\n");
                  setFreqs = true;
                  *taken = _max_edge_freq - 1;
                  *notTaken = 1;
                  }
               else if (!loopContainsBranchTo && loopContainsFallThrough)
                  {
                  //dumpOptDetails(comp(),"Not Taken\n");
                  setFreqs = true;
                  *notTaken = _max_edge_freq - 1;
                  *taken = 1;
                  }
               }

            if (!setFreqs)
               {
               *taken = _max_edge_freq/2;
               *notTaken = _max_edge_freq/2;
               }

            return;
            }

         parent = parent->getParent();
         }

      *taken = _max_edge_freq/2;
      *notTaken = _max_edge_freq/2;
      return;
      }

   TR_ASSERT(0, "Branch counters can only be set if we have either an external profiler or structure\n");
   }

void OMR::CFG::scaleEdgeFrequencies()
   {
   _maxEdgeFrequency = -1;
   TR::CFGNode *node;
   for (node = getFirstNode(); node; node = node->getNext())
      {
      int32_t frequency = node->getFrequency();
      if (frequency >= 0)
         {
         int32_t successorFrequency = 0;
         for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
            {
            int32_t nextFrequency = (*e)->getFrequency();
            successorFrequency = successorFrequency + nextFrequency;
            }

         if (successorFrequency > 0)
            {
            TR::CFGEdge *hottestEdge = NULL;
            int32_t hottestEdgeFrequency = -1;
            for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
               {
               int32_t nextFrequency = (*e)->getFrequency();
               int32_t edgeFrequency;

               if (frequency > MAX_COLD_BLOCK_COUNT)
                  {
                  edgeFrequency = (frequency*nextFrequency)/successorFrequency;
                  (*e)->setFrequency(edgeFrequency);
                  }
               else
                  edgeFrequency = frequency;

               if (nextFrequency > hottestEdgeFrequency)
                  {
                  hottestEdgeFrequency = nextFrequency;
                  hottestEdge = *e;
                  }

               if (edgeFrequency > _maxEdgeFrequency)
                  _maxEdgeFrequency = edgeFrequency;
               //dumpOptDetails("1Edge %p between %d and %d has freq %d (_maxEdgeFrequency %d)\n", e, e->getFrom()->getNumber(), e->getTo()->getNumber(), e->getFrequency(), _maxEdgeFrequency);
               }

            if (hottestEdge->getFrequency() == 0)
               {
               hottestEdge->setFrequency(frequency);
               if (frequency > _maxEdgeFrequency)
                  _maxEdgeFrequency = frequency;
               }
            }
         }

      }

   //normalizeEdgeFrequencies;
   }

int32_t TR::CFGNode::normalizedFrequency(int32_t rawFrequency, int32_t maxFrequency)
   {
   int32_t delta;
   if (maxFrequency > 0)
      {
      delta = ((rawFrequency * MAX_BLOCK_COUNT)/maxFrequency);

      if (delta == 0)
         delta = 1;
      }
   else
      delta = 1;
   return MAX_COLD_BLOCK_COUNT + delta;
   }

void TR::CFGNode::normalizeFrequency(int32_t frequency, int32_t maxFrequency)
   {
   if (asBlock()->isCold() && (frequency <= MAX_COLD_BLOCK_COUNT))
      return;

   //dumpOptDetails(comp(), "2Normalizing node %d with freq %d max freq %d\n", getNumber(), frequency, maxFrequency);
   setFrequency(normalizedFrequency(frequency, maxFrequency));
   }


void TR::CFGNode::normalizeFrequency(int32_t maxFrequency)
   {
   int32_t frequency = getFrequency();
   if (asBlock()->isCold() && (frequency <= MAX_COLD_BLOCK_COUNT))
      return;
   normalizeFrequency(frequency, maxFrequency);
   }


int32_t TR::CFGNode::denormalizedFrequency(int32_t normalizedFrequency, int32_t maxFrequency)
   {
   if (normalizedFrequency <= MAX_COLD_BLOCK_COUNT)
      return normalizedFrequency;

   // the multiply below can overflow
   // so cast to int64_t
   int32_t frequency = (int32_t)((int64_t)((maxFrequency*(int64_t)(normalizedFrequency - MAX_COLD_BLOCK_COUNT)))/MAX_BLOCK_COUNT);
   return frequency;
   }

int32_t TR::CFGNode::denormalizeFrequency(int32_t maxFrequency)
   {
   int32_t frequency = denormalizedFrequency(getFrequency(), maxFrequency);
   setFrequency(frequency);
   return frequency;
   }




void TR::CFGEdge::normalizeFrequency(int32_t maxEdgeFrequency)
   {
   int32_t frequency = getFrequency();
   //dumpOptDetails("Edge %p between %d and %d has freq %d (_maxEdgeFrequency %d)\n", this, getFrom()->getNumber(), getTo()->getNumber(), getFrequency(), maxEdgeFrequency);

   if (frequency <= MAX_COLD_BLOCK_COUNT)
      {
      if (!getTo()->asBlock()->isCold() &&
          !getFrom()->asBlock()->isCold())
         setFrequency(MAX_COLD_BLOCK_COUNT+1);
      return;
      }

   setFrequency((frequency * (MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT))/maxEdgeFrequency);
   }


void OMR::CFG::normalizeNodeFrequencies(TR_BitVector *nodesToBeNormalized, TR_Array<TR::CFGEdge *> * edgeArray)
   {
   if (!nodesToBeNormalized)
      return;
   int32_t edgeIndex = 0;
   TR::CFGNode *node;
   if (_maxFrequency < 0)
      {
      for (node = getFirstNode(); node; node = node->getNext())
         {
         int32_t frequency = node->getFrequency();
         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "11maxFrequency old %d new %d node %d\n", _maxFrequency, frequency, node->getNumber());
         if (frequency > _maxFrequency)
            {
            if (comp()->getOption(TR_TraceBFGeneration))
               traceMsg(comp(), "22maxFrequency old %d new %d node %d\n", _maxFrequency, frequency, node->getNumber());
            _maxFrequency = frequency;
            }
         TR_SuccessorIterator sit(node);
         for(TR::CFGEdge * edge = sit.getFirst(); edge; edge = sit.getNext())
             {
               edgeArray->add(edge);
               edgeIndex++;
             }

         }
      }

   _numEdges = edgeIndex;

   if (_maxFrequency <= 0)
      return;

   edgeIndex = 0;
   //TR_ASSERT((_maxFrequency > 0), "Max node frequency must be set by now\n");
   for (node = getFirstNode(); node; node = node->getNext())
      {
      if (nodesToBeNormalized->get(node->getNumber()))
         {
         int32_t frequency = node->getFrequency();
         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "normalize : max frequency %d freq %d node %d\n", _maxFrequency, frequency, node->getNumber());

         node->normalizeFrequency(_maxFrequency);

         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "normalize : final freq %d node %d\n", node->getFrequency(), node->getNumber());
         }
      TR_SuccessorIterator sit(node);
      for(TR::CFGEdge * edge = sit.getFirst(); edge; edge = sit.getNext())
         {
          edgeArray->add(edge);
          edgeIndex++;
          }
      }
    _numEdges = edgeIndex;
   }




void OMR::CFG::normalizeEdgeFrequencies(TR_Array<TR::CFGEdge *> * edgesArray)
   {
   TR::CFGEdge *edge;
   int32_t edgeIndex = 0;
   if (_maxEdgeFrequency < 0)
      {
      for (edgeIndex = 0; edgeIndex < _numEdges; edgeIndex++)
         {
         edge = edgesArray->element(edgeIndex);
         TR_ASSERT(edge != NULL,"Array of edges should not have null entries");
         int32_t frequency = edge->getFrequency();
         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "11maxEdgeFrequency old %d new %d edge (%d -> %d) %p\n", _maxEdgeFrequency, frequency, edge->getFrom()->getNumber(), edge->getTo()->getNumber(), edge);

         if (frequency > _maxEdgeFrequency)
            {
            if (comp()->getOption(TR_TraceBFGeneration))
               traceMsg(comp(), "22maxEdgeFrequency old %d new %d edge (%d -> %d) %p\n", _maxEdgeFrequency, frequency, edge->getFrom()->getNumber(), edge->getTo()->getNumber(), edge);
            _maxEdgeFrequency = frequency;
            }
         }
      }


   if (_maxEdgeFrequency <= 0)
      return;

   if (_maxFrequency > _maxEdgeFrequency)
      _maxEdgeFrequency = _maxFrequency;

   //TR_ASSERT((_maxEdgeFrequency > 0), "Max edge frequency must be set by now\n");
   for (edgeIndex = 0; edgeIndex < _numEdges; edgeIndex++)
      {
      edge = edgesArray->element(edgeIndex);
      edge->normalizeFrequency(_maxEdgeFrequency);
      }
   }


void OMR::CFG::resetFrequencies()
   {
   int32_t edgeIndex  = 0;
   int32_t numBlocks = getNextNodeNumber();
   TR_BitVector *nodesToBeReset = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   nodesToBeReset->setAll(numBlocks);
   _maxFrequency = -1;
   _maxEdgeFrequency = -1;

   TR::CFGNode *node;

   // reset node frequencies
   for (node = getFirstNode(); node; node = node->getNext())
      {
        if (nodesToBeReset->get(node->getNumber()) &&
            !node->asBlock()->isCold())
           {
            node->setFrequency(-1);
           }

        TR_SuccessorIterator sit(node);
        TR::CFGEdge * edge;
        for(edge = sit.getFirst(); edge; edge = sit.getNext())
            {
              if (edge->getFrom()->asBlock()->isCold())
                 edge->setFrequency(edge->getFrom()->getFrequency());
              else if (edge->getTo()->asBlock()->isCold())
                 edge->setFrequency(edge->getTo()->getFrequency());
              else
                 edge->setFrequency(0);
              edgeIndex++;
            }
      }

   _numEdges = edgeIndex;

   if (comp()->getOption(TR_TraceBFGeneration))
      comp()->dumpMethodTrees("Trees after resetFrequencies");

   }



void OMR::CFG::normalizeFrequencies(TR_BitVector *nodesToBeNormalized)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   TR_Array<TR::CFGEdge *> * edgesArray = new(trStackMemory())TR_Array<TR::CFGEdge *>(trMemory(),_numEdges,true,stackAlloc);
   normalizeNodeFrequencies(nodesToBeNormalized,edgesArray);
   if(edgesArray->isEmpty())
       {
         int32_t edgeIndex = 0;
         TR::CFGNode * node;
         for ( node = getFirstNode(); node; node = node->getNext())
         {
          TR_SuccessorIterator sit(node);
          TR::CFGEdge * edge;
          for(edge = sit.getFirst(); edge; edge = sit.getNext())
              {
                edgesArray->add(edge);
                edgeIndex++;
              }
         }
         _numEdges = edgeIndex;
       }
   normalizeEdgeFrequencies(edgesArray);
   }


void
OMR::CFG::findReachableBlocks(TR_BitVector *result)
   {
   TR_ScratchList<TR::CFGEdge> edgesToBeRemoved(trMemory());
   TR_Stack<TR::CFGNode *> stack(trMemory(), 8, false, stackAlloc);
   stack.push(getStart());
   while (!stack.isEmpty())
      {
      TR::CFGNode * node = stack.pop();
      if (!result->get(node->getNumber()))
         {
         result->set(node->getNumber());

            {
            TR_SuccessorIterator edges(node);
            for (TR::CFGEdge * edge = edges.getFirst(); edge; edge = edges.getNext())
               {
               stack.push(edge->getTo());
               }
            }
         }
      }

   if (edgesToBeRemoved.isEmpty()) return;
   //the below code is just a second pass to verify that removing edges from start to the alwayskeepblocks doesn't make the alwayskeepblocks unreachable
   //this is possible when there is a cycle in the CFG
   TR_BitVector reachableBlocksAfterEdgesRemoved(getNumberOfNodes(), comp()->trMemory(), stackAlloc, growable);
   ListIterator<TR::CFGEdge> edgesToRemove(&edgesToBeRemoved);
   for (TR::CFGEdge * edge = edgesToRemove.getFirst(); edge; edge = edgesToRemove.getNext())
      {
      removeEdge(edge);
      }
   stack.push(getStart());
   while (!stack.isEmpty())
      {
      TR::CFGNode * node = stack.pop();
      if (!reachableBlocksAfterEdgesRemoved.get(node->getNumber()))
         {
         reachableBlocksAfterEdgesRemoved.set(node->getNumber());
            {
            TR_SuccessorIterator edges(node);
            for (TR::CFGEdge * edge = edges.getFirst(); edge; edge = edges.getNext())
               {
               stack.push(edge->getTo());
               }
            }
         }
      }
   for (TR::CFGEdge * edge = edgesToRemove.getFirst(); edge; edge = edgesToRemove.getNext())
      {
      if (!reachableBlocksAfterEdgesRemoved.get(edge->getTo()->getNumber()))
         addEdge(getStart(), edge->getTo());
      }
   }
