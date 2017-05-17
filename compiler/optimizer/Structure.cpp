/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "optimizer/Structure.hpp"

#include <algorithm>                                // for std::find, etc
#include <limits.h>                                 // for INT_MAX
#include <string.h>                                 // for memcpy
#include "compile/Compilation.hpp"                  // for Compilation
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                             // for Block, toBlock
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for TR::ILOpCode, etc
#include "il/Node.hpp"                              // for Node, vcount_t
#include "il/Node_inlines.hpp"                      // for Node::getChild, etc
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode, etc
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/List.hpp"                           // for ListIterator, etc
#include "infra/TRCfgEdge.hpp"                      // for CFGEdge
#include "infra/TRCfgNode.hpp"                      // for CFGNode
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase

#define OPT_DETAILS "O^O STRUCTURE: "


// The "getKind" methods are made non-inline to identify this as the home for the
// vfts for this class hierarchy.
//
TR_Structure::Kind TR_Structure::getKind()                  {return Blank;}
TR_Structure::Kind TR_BlockStructure::getKind()             {return Block;}
TR_Structure::Kind TR_RegionStructure::getKind()            {return Region;}


TR_BlockStructure::TR_BlockStructure(TR::Compilation * comp, int32_t index, TR::Block *b) :
   TR_Structure(comp, index), _block(b)
   {
   TR_BlockStructure *old = b->getStructureOf();
   if (old)
      {
      setAsLoopInvariantBlock(old->isLoopInvariantBlock());
      if (old->isEntryOfShortRunningLoop())
         setIsEntryOfShortRunningLoop();
      setCreatedByVersioning(old->isCreatedByVersioning());
      }
   b->setStructureOf(this);
   }


bool TR_BlockStructure::doDataFlowAnalysis(TR_DataFlowAnalysis *dfa, bool checkForChanges)
   {
   // LexicalTimer tlex("blockStructure_doDFA", comp()->phaseTimer());
   return dfa->analyzeBlockStructure(this, checkForChanges);
   }

bool TR_RegionStructure::doDataFlowAnalysis(TR_DataFlowAnalysis *dfa, bool checkForChanges)
   {
   // LexicalTimer tlex("regionStructure_doDFA", comp()->phaseTimer());
   return dfa->analyzeRegionStructure(this, checkForChanges);
   }

void TR_BlockStructure::clearAnalysisInfo()
   {
   setAnalysisInfo(NULL);
   }

void TR_RegionStructure::clearAnalysisInfo()
   {
   setAnalysisInfo(NULL);
   TR_RegionStructure::Cursor subs(*this);
   for (TR_StructureSubGraphNode *p = subs.getFirst(); p != NULL; p = subs.getNext())
      p->getStructure()->clearAnalysisInfo();
   }

bool TR_Structure::contains(TR_Structure *other, TR_Structure *commonParent)
   {
   for (TR_Structure *s = other; s != NULL && s != commonParent; s = s->getParent())
      if (s == this)
         return true;
   return false;
   }

TR_Structure *TR_Structure::getContainingLoop()
   {
   for (TR_Structure *p = getParent(); p != NULL ; p = p->getParent())
      if (p->asRegion()->isNaturalLoop())
         return p;
   return NULL;
   }

// Note that if A contains B, the common parent is parent(A)
//
TR_RegionStructure *TR_Structure::findCommonParent(TR_Structure *other, TR::CFG *cfg)
   {
   for (TR_RegionStructure *parent = getParent()->asRegion();
        parent;
        parent = parent->getParent()->asRegion())
      {
      if (parent->contains(other))
         return parent;
      }
   TR_ASSERT(this == cfg->getStructure() ||
          other == cfg->getStructure(),
          "A and B MUST have a common ancestor unless either of them is region 0");
   return 0;
   }

void TR_BlockStructure::replacePart(TR_Structure *from, TR_Structure *to)
   {
   TR_ASSERT(0,"bad structure found trying to replace a substructure");
   }

void TR_RegionStructure::addSubNode(TR_StructureSubGraphNode *subNode)
   {
   uint32_t index = comp()->getFlowGraph()->addStructureSubGraphNodes(subNode);
   _subNodes[index] = 1;
   TR_Structure *node = subNode->getStructure();
   node->setParent(this);
   }


void TR_RegionStructure::cleanupAfterEdgeRemoval(TR::CFGNode *node)
   {
   TR_BlockStructure *blockSt = toStructureSubGraphNode(node)->getStructure()->asBlock();
   if (node != getEntry() &&
       node->getPredecessors().empty() &&
       node->getExceptionPredecessors().empty())
      {
      // The node is now isolated and should be removed
      //
      if (node->getSuccessors().empty() &&
          node->getExceptionSuccessors().empty())
         {
         // The node is completely isolated. It can be removed from the
         // structure.
         //
         if (node->getNumber() != comp()->getFlowGraph()->getEnd()->getNumber())
            removeSubNode(toStructureSubGraphNode(node));
         }
      else
         {
         // The node is isolated but it still has successor edges.
         // It must be part of an unreachable cycle. Tell the CFG so that it
         // can run its pass to remove unreachable cycles later.
         //
         comp()->getFlowGraph()->setHasUnreachableBlocks();
         }
      }

   if (containsInternalCycles())
      checkForInternalCycles();
   else if ((numSubNodes() == 1) && !isNaturalLoop() &&
            !getEntry()->hasSuccessor(getEntry()) &&
            !getEntry()->hasExceptionSuccessor(getEntry()))
      {
      // This may be required if an acyclic region was created by
      // getting rid of the back edge of a natural loop (say with only one block);
      // in that case we want to collapse the entry into the parent
      //
      TR_RegionStructure::Cursor si(*this);
      TR_ASSERT(si.getFirst() == getEntry(),"bad structure found trying to remove a block");
      if (getParent() && (getEntry()->getStructure()->getParent() == this))
         getParent()->replacePart(this, getEntry()->getStructure());
      }
   }



void TR_RegionStructure::cleanupAfterNodeRemoval()
   {
   // If the region now consists of a single node, replace this node in its
   // parent by the subnode.
   //
   // If there is no parent, this node has already been removed from its parent
   // region so don't bother replacing it.
   //
   if ((numSubNodes() == 1) && !isNaturalLoop() &&
       !getEntry()->hasSuccessor(getEntry()) &&
       !getEntry()->hasExceptionSuccessor(getEntry()))
      {
      TR_RegionStructure::Cursor si(*this);
      TR_ASSERT(si.getFirst() == getEntry(),"bad structure found trying to remove a block");
      if (getParent())
         getParent()->replacePart(this, getEntry()->getStructure());
      }
   }



void TR_RegionStructure::removeSubNode(TR_StructureSubGraphNode *node)
   {
   _subNodes[node->getUniqueSubGraphIndex()] = 0;
   comp()->getFlowGraph()->removeStructureSubGraphNodes(node);
   node->getStructure()->setParent(NULL);
   cleanupAfterNodeRemoval();
   }

TR_StructureSubGraphNode *TR_RegionStructure::findSubNodeInRegion(int32_t num)
   {
   TR_RegionStructure::Cursor it(*this);
   for (TR_StructureSubGraphNode *node = it.getCurrent(); node; node = it.getNext())
      if (node->getNumber() == num)
         return node;
   return 0;
   }

void TR_RegionStructure::replacePart(TR_Structure *from, TR_Structure *to)
   {
   // Find the sub-graph node for the sub-structure
   //
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (node->getStructure() == from)
         break;
      }

   TR_ASSERT(node != NULL, "structure %d doesn't include replacee node %d", _nodeIndex, from->getNumber());
   TR_ASSERT(node != getEntry() || _nodeIndex == to->getNumber(), "cannot replace the entry node in %d with node %d", _nodeIndex, to->getNumber());
   node->setStructure(to);
   to->setParent(this);

   // If the node number has changed, replace the exit part in all predecessors
   // that are themselves regions.
   //
   if (from->getNumber() != to->getNumber())
      {
      for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
         {
         TR_RegionStructure *pred = toStructureSubGraphNode((*edge)->getFrom())->getStructure()->asRegion();
         if (pred)
            pred->replaceExitPart(from->getNumber(), to->getNumber());
         }
      for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
         {
         TR_RegionStructure *pred = toStructureSubGraphNode((*edge)->getFrom())->getStructure()->asRegion();
         if (pred)
            pred->replaceExitPart(from->getNumber(), to->getNumber());
         }
      }
   }

void TR_RegionStructure::replaceExitPart(int32_t fromNumber, int32_t toNumber)
   {
   // Find the sub-graph node for the exit node and change its number.
   // Allow for multiple exit nodes with the same number.
   //
   bool haveReplaced = false;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   TR::CFGEdge *edge;

   // First see if there are any region subnodes that exit to the node
   // being replaced. If so, their exit edges have to be fixed up too.
   //
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == fromNumber)
         {
         TR_RegionStructure *pred = toStructureSubGraphNode(edge->getFrom())->getStructure()->asRegion();
         if (pred)
            pred->replaceExitPart(fromNumber, toNumber);
         }
      }

   // Now change the number on the relevant exit node(s)
   //
   ei.reset();
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {

      ///////if (!haveReplaced)
         {
         if (edge->getTo()->getNumber() == fromNumber)
            {
            edge->getTo()->setNumber(toNumber);
            haveReplaced = true;
            }
         }
      }
   TR_ASSERT(haveReplaced,"bad structure found trying to replace a substructure");
   }




void TR_RegionStructure::collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges)
   {
   TR_BitVector *seenNodes = new (trStackMemory()) TR_BitVector(1, trMemory(), stackAlloc, growable);
   TR::CFGEdge *edge;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      if (!seenNodes->get(edge->getFrom()->getNumber()))
         {
         seenNodes->set(edge->getFrom()->getNumber());
         TR_Structure *s = edge->getFrom()->asStructureSubGraphNode()->getStructure();
         int32_t toNum = edge->getTo()->getNumber();
         s->collectExitBlocks(exitBlocks);
         if (s->asBlock() && exitEdges)
            {
            TR::Block *exitBlock = s->asBlock()->getBlock();
            TR_SuccessorIterator exitIt(exitBlock);
            for (TR::CFGEdge * exitEdge = exitIt.getFirst(); exitEdge != NULL; exitEdge = exitIt.getNext())
               {
               if (toNum == exitEdge->getTo()->getNumber())
                  exitEdges->add(exitEdge);
               }
            }
         }
      }
   }


void TR_BlockStructure::collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges)
   {
   exitBlocks->add(getBlock());
   }


void TR_RegionStructure::addGlobalRegisterCandidateToExits(TR_RegisterCandidate *inductionCandidate)
   {
   TR_ScratchList<TR::Block> exitBlocks(trMemory());
   collectExitBlocks(&exitBlocks);

   TR::Block *nextBlock;
   ListIterator<TR::Block> bi(&exitBlocks);
   for (nextBlock = bi.getFirst(); nextBlock; nextBlock = bi.getNext())
      {
      int32_t blockWeight = 1;
      if (nextBlock->getStructureOf())
         {
         nextBlock->getStructureOf()->calculateFrequencyOfExecution(&blockWeight);
         inductionCandidate->addBlock(nextBlock, blockWeight, trMemory());
         }
      }
   }



static bool findCycle(TR_StructureSubGraphNode *node, TR::SparseBitVector &regionNodes, TR_BitVector &nodesSeenOnPath, TR_BitVector &nodesCleared, int32_t entryNode)
   {
   if (nodesSeenOnPath.get(node->getNumber()))
      return true;             // An internal cycle found
   if (nodesCleared.get(node->getNumber()))
      return false;            // This node is already known not to be in a cycle

   nodesSeenOnPath.set(node->getNumber());

   for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
      {
      TR_ASSERT((*edge)->getTo()->asStructureSubGraphNode(),"Expecting a CFG node which can be downcast to StructureSubGraphNode");
      TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
      if (succ->getNumber() != entryNode && regionNodes.ValueAt(succ->getUniqueSubGraphIndex()) &&
          findCycle(succ,regionNodes,nodesSeenOnPath,nodesCleared,entryNode))
         return true;
      }
   for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
      {
      TR_ASSERT((*edge)->getTo()->asStructureSubGraphNode(),"Expecting a CFG node which can be downcast to StructureSubGraphNode");
      TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
      if (/* succ->getNumber() != entryNode && */ regionNodes.ValueAt(succ->getUniqueSubGraphIndex()) &&
          findCycle(succ,regionNodes,nodesSeenOnPath,nodesCleared,entryNode))
         return true;
      }

   nodesSeenOnPath.reset(node->getNumber());
   nodesCleared.set(node->getNumber());
   return false;
   }

void TR_RegionStructure::checkForInternalCycles()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t numNodes = comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector nodesSeenOnPath(numNodes,trMemory(), stackAlloc);
   TR_BitVector nodesCleared(numNodes,trMemory(), stackAlloc);

   setContainsInternalCycles(findCycle(getEntry(), _subNodes, nodesSeenOnPath, nodesCleared, getNumber()));
   }

bool TR_RegionStructure::hasExceptionOutEdges()
   {
   ListIterator<TR::CFGEdge> ei(&getExitEdges());
   for (TR::CFGEdge *edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
      {
      TR_StructureSubGraphNode *edgeTo = toStructureSubGraphNode(edge->getTo());
      if (!edgeTo->getExceptionPredecessors().empty())
         return true;
      }
   return false;
   }

void TR_BlockStructure::removeEdge(TR_Structure *from, TR_Structure *to)
   {
   TR_ASSERT(to == this,"bad structure trying to remove a block");

   // If this structure has no parent, it must represent the exit block. In this
   // case there is no structure edge to remove.
   //
   TR_Structure *parent = getParent();

   // TR_ASSERT(parent != NULL || comp()->getFlowGraph()->getEnd() == _block,
   //       "bad structure (no parent) trying to remove a block");

   if (parent)
      parent->removeEdge(from, to);
   }

void TR_RegionStructure::removeEdge(TR::CFGEdge *edge, bool isExitEdge)
   {
   // Remove this edge from the region's subgraph
   //
   TR::CFGNode *fromNode = edge->getFrom();
   TR::CFGNode *toNode   = edge->getTo();
   auto successorIterator = std::find(fromNode->getSuccessors().begin(), fromNode->getSuccessors().end(), edge);
   bool isRegularEdge = successorIterator != fromNode->getSuccessors().end();

   TR::CFGEdgeList &successorList = isRegularEdge ?
      fromNode->getSuccessors() :
      fromNode->getExceptionSuccessors();

   TR::CFGEdgeList &predecessorList = isRegularEdge ?
      toNode->getPredecessors() :
      toNode->getExceptionPredecessors();

   auto predecessorIterator = std::find(predecessorList.begin(), predecessorList.end(), edge);

   successorIterator = isRegularEdge ?
      successorIterator :
      std::find(successorList.begin(), successorList.end(), edge);

   TR_ASSERT(
      (predecessorIterator != predecessorList.end()) && (successorIterator != successorList.end()),
      "bad structure found trying to remove an out edge"
      );

   predecessorList.erase(predecessorIterator);
   successorList.erase(successorIterator);

   bool cleanupAlreadyDone = false;
   if (isExitEdge)
      _exitEdges.remove(edge);
   else
      {
      cleanupAfterEdgeRemoval(toNode);
      if (toNode == fromNode)
         cleanupAlreadyDone = true;
      }

   if (!cleanupAlreadyDone)
      cleanupAfterEdgeRemoval(fromNode);
   }

void TR_RegionStructure::removeEdge(TR_Structure *from, TR_Structure *to)
   {
   // Find the sub-graph nodes for the "to" and possibly the "from" structure.
   //
   TR_StructureSubGraphNode *fromNode = NULL, *toNode = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (TR_StructureSubGraphNode *node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (fromNode == NULL && node->getStructure()->contains(from, this))
         {
         fromNode = node;
         if (toNode != NULL)
            break;
         }
      if (toNode == NULL && node->getNumber() == to->getNumber())
         {
         toNode = node;
         if (fromNode != NULL)
            break;
         }
      }

   // If the toNode has already been removed from this structure
   // don't do anything.
   //
   if (toNode == NULL)
       return;

   // If the edge to be removed is one coming into this region, let the parent
   // handle it.
   //
   if (fromNode == NULL)
      {
      TR_ASSERT(toNode == getEntry(),"bad structure found trying to remove a block");
      if (getParent())
         getParent()->removeEdge(from, to);
      return;
      }

   // The edge is internal. See if it is a regular edge or an exception edge.
   //
   int removedEdgeType = fromNode->getStructure()->removeExternalEdgeTo(from, to->getNumber());

   if (removedEdgeType == NO_MORE_EDGES_EXIST)
      {
      // Find the edge to be removed
      //
      auto edge = fromNode->getSuccessors().begin();
      for (; edge != fromNode->getSuccessors().end(); ++edge)
         if ((*edge)->getTo()->getNumber() == to->getNumber())
            break;

      if (edge == fromNode->getSuccessors().end())
         {
         edge = fromNode->getExceptionSuccessors().begin();
         for (; edge != fromNode->getExceptionSuccessors().end(); ++edge)
            if ((*edge)->getTo()->getNumber() == to->getNumber())
               break;

         if(edge == fromNode->getExceptionSuccessors().end())
            TR_ASSERT(false, "bad structure found trying to remove a block");
         }

        removeEdge(*edge, false);
      }
   }

int TR_BlockStructure::removeExternalEdgeTo(TR_Structure *from, int32_t toNumber)
   {
   TR_ASSERT(getNumber() == from->getNumber(),"bad structure found trying to remove an out edge");

   return NO_MORE_EDGES_EXIST;
   }

int TR_RegionStructure::removeExternalEdgeTo(TR_Structure *from, int32_t toNumber)
   {
   // Find the node containing the from structure
   //
   TR_StructureSubGraphNode *node = NULL;
   TR_RegionStructure::Cursor ni(*this);
   for (TR::CFGNode *n = ni.getCurrent(); n; n = ni.getNext())
      {
      node = toStructureSubGraphNode(n);
      if (node->getStructure()->contains(from, this))
         break;
      }
   TR_ASSERT(node != NULL,"bad structure found trying to remove an out edge");

   int removedEdgeType;
   TR_Structure *subNode = node->getStructure();

   // Find the exit edge to be removed.
   //
   TR::CFGEdge *edge;
   TR::CFGEdge *relevantEdge = NULL;
   TR::CFGNode *toNode = NULL;
   bool oneExitEdge = false, moreThanOneExitEdgeToSameToNumber = false;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == toNumber)
         {
         if (!toNode && edge->getFrom() == node)
            {
            toNode = edge->getTo();
            relevantEdge = edge;
            }

         if (oneExitEdge)
            moreThanOneExitEdgeToSameToNumber = true;
         oneExitEdge = true;
         }
      }

   TR_ASSERT(relevantEdge != NULL,"bad structure found trying to remove an out edge");

   removedEdgeType = subNode->removeExternalEdgeTo(from, toNumber);
   if (removedEdgeType == NO_MORE_EDGES_EXIST)
      {
      // Remove the edge
      //
      removeEdge(relevantEdge, true);

      if (moreThanOneExitEdgeToSameToNumber)
         removedEdgeType = EDGES_STILL_EXIST;
      else
         {
         if (!toNode->getExceptionPredecessors().empty() ||
             !toNode->getPredecessors().empty())
            removedEdgeType = EDGES_STILL_EXIST;
         }
      }

   return removedEdgeType;
   }


void TR_Structure::setNestingDepths(int32_t *currentNestingDepth)
   {
   if (getParent())
      getParent()->setNestingDepths(currentNestingDepth);

   TR_RegionStructure *region = asRegion();
   if (region && region->isNaturalLoop())
      *currentNestingDepth = (*currentNestingDepth) + 1;

   setNestingDepth(*currentNestingDepth);
   }

void TR_Structure::setAnyCyclicRegionNestingDepths(int32_t *currentNestingDepth)
   {
   if (getParent())
      getParent()->setAnyCyclicRegionNestingDepths(currentNestingDepth);

   TR_RegionStructure *region = asRegion();
   if (region && region->containsInternalCycles())
      *currentNestingDepth = (*currentNestingDepth) + 1;

   setAnyCyclicRegionNestingDepth(*currentNestingDepth);
   }


int32_t TR_Structure::getMaxNestingDepth(int32_t *currentNestingDepth, int32_t *currentMaxNestingDepth)
   {
   return *currentMaxNestingDepth;
   }


int32_t TR_RegionStructure::getMaxNestingDepth(int32_t *currentNestingDepth, int32_t *currentMaxNestingDepth)
   {
   bool isLoop = isNaturalLoop();
   if (isLoop)
      *currentNestingDepth = (*currentNestingDepth) + 1;

   if (*currentNestingDepth > *currentMaxNestingDepth)
       *currentMaxNestingDepth = *currentNestingDepth;

   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      subStruct->getMaxNestingDepth(currentNestingDepth, currentMaxNestingDepth);
      }

   if (isLoop)
      *currentNestingDepth = (*currentNestingDepth) - 1;

   return *currentMaxNestingDepth;
   }



void TR_Structure::calculateFrequencyOfExecution(int32_t *currentWeight)
   {
   if (getParent())
      getParent()->calculateFrequencyOfExecution(currentWeight);

   TR_RegionStructure *region = asRegion();
   if (region && !region->isAcyclic() && (*currentWeight < (INT_MAX/10)))                // careful: don't overflow 32b int!
      *currentWeight = (*currentWeight)*10;
   }


void TR_Structure::setConditionalityWeight(int32_t *currentWeight)
   {
   TR_RegionStructure *region = asRegion();
   bool isAcyclicRegion = false;
   if (region &&
       (!region->isAcyclic() ||
        (region == comp()->getFlowGraph()->getStructure())))
      {
      adjustWeightForBranches(region->getEntry(), region->getEntry(), currentWeight);
      }
   else if (region->isAcyclic())
      isAcyclicRegion = true;

   if (isAcyclicRegion &&
       getParent())
      getParent()->setConditionalityWeight(currentWeight);
   }



void TR_Structure::adjustWeightForBranches(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry, int32_t *currentWeight)
   {
   int32_t startingWeight = *currentWeight;
   if (node->getPredecessors().size() > 1)
      startingWeight = 10*startingWeight/9;

   int32_t weight = startingWeight;

   if (node->getStructure() &&
       (weight > node->getStructure()->getWeight()))
      {
      node->getStructure()->setWeight(weight);
      //dumpOptDetails(comp(), "0Node %p(%d) Structure %p Weight %d\n", node, node->getNumber(), node->getStructure(), weight);
      }
   else
      return;

   if (node->getStructure()->asRegion())
      {
      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;
      TR_RegionStructure::Cursor si(*(node->getStructure()->asRegion()));
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         weight = startingWeight;
         subStruct = subNode->getStructure();
         if (!subStruct->asRegion())
            {
            if (weight > subStruct->getWeight())
               {
               subStruct->setWeight(weight);
               //dumpOptDetails(comp(), "1Node %p(%d) Structure %p Weight %d\n", subNode, subNode->getNumber(), subStruct, weight);
               }
            }
         else if (subStruct->asRegion()->isAcyclic())
            adjustWeightForBranches(subStruct->asRegion()->getEntry(), subStruct->asRegion()->getEntry(), &weight);
         }
      }

   if (node->getSuccessors().size() > 1)
      startingWeight = std::max(1, startingWeight*9/10);

   for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
      {
      weight = startingWeight;
      if ((*edge)->getTo() != entry)
         adjustWeightForBranches(toStructureSubGraphNode((*edge)->getTo()), entry, &weight);
      }

   for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
      {
      weight = startingWeight;
      if ((*edge)->getTo() != entry)
         adjustWeightForBranches(toStructureSubGraphNode((*edge)->getTo()), entry, &weight);
      }
   }

TR_StructureSubGraphNode *TR_Structure::findNodeInHierarchy(TR_RegionStructure *region, int32_t num)
   {
   if (!region)
      return NULL;
   TR_RegionStructure::Cursor sIt(*region);
   for (TR_StructureSubGraphNode *n = sIt.getFirst(); n; n = sIt.getNext())
      {
      if (n->getNumber() == num)
         return n;
      }
   return findNodeInHierarchy(region->getParent()->asRegion(), num);
   }


bool TR_RegionStructure::containsOnlyAcyclicRegions()
   {
   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      TR_RegionStructure *region = subStruct->asRegion();
      if (region &&
          (!region->isAcyclic() ||
           !region->containsOnlyAcyclicRegions()))
         return false;
      }

   return true;
   }



TR_Structure *TR_BlockStructure::cloneStructure(TR::Block **correspondingBlocks, TR_StructureSubGraphNode **correspodingSubNodes, List<TR_Structure> *whileLoops, List<TR_Structure> *correspondingWhileLoops)
   {
   TR::Block *correspondingBlock = correspondingBlocks[getNumber()];
   TR_BlockStructure *clonedBlockStructure = new (trHeapMemory()) TR_BlockStructure(comp(), correspondingBlock->getNumber(), correspondingBlock);
   clonedBlockStructure->setAsLoopInvariantBlock(isLoopInvariantBlock());
   clonedBlockStructure->setNestingDepth(getNestingDepth());
   clonedBlockStructure->setMaxNestingDepth(getMaxNestingDepth());
   if (isEntryOfShortRunningLoop())
      clonedBlockStructure->setIsEntryOfShortRunningLoop();
   clonedBlockStructure->setDuplicatedBlock(this);
   return clonedBlockStructure;
   }




TR_Structure *TR_RegionStructure::cloneStructure(TR::Block **correspondingBlocks, TR_StructureSubGraphNode **correspondingSubNodes, List<TR_Structure> *whileLoops, List<TR_Structure> *correspondingWhileLoops)
   {
   TR_RegionStructure *clonedRegionStructure = new (trHeapMemory()) TR_RegionStructure(comp(), correspondingBlocks[getNumber()]->getNumber());
   clonedRegionStructure->setAsCanonicalizedLoop(isCanonicalizedLoop());
   clonedRegionStructure->setContainsInternalCycles(containsInternalCycles());

   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      TR_Structure *clonedSubStruct = subStruct->cloneStructure(correspondingBlocks, correspondingSubNodes, whileLoops, correspondingWhileLoops);
      TR_StructureSubGraphNode *clonedSubNode = new (trHeapMemory()) TR_StructureSubGraphNode(clonedSubStruct);
      clonedRegionStructure->addSubNode(clonedSubNode);
      if (subNode == getEntry())
         clonedRegionStructure->setEntry(clonedSubNode);

      correspondingSubNodes[subNode->getNumber()] = clonedSubNode;
      }

   TR_ScratchList<TR_StructureSubGraphNode> seenExitNodes(trMemory());
   si.reset();
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      TR_StructureSubGraphNode *correspondingSubNode = correspondingSubNodes[subNode->getNumber()];

      for (auto edge = subNode->getSuccessors().begin(); edge != subNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (!isExitEdge(*edge))
            {
            TR::CFGEdge::createEdge(correspondingSubNode, correspondingSubNodes[succ->getNumber()], trMemory());
            }
         else
            {
            clonedRegionStructure->addExitEdge(correspondingSubNode, succ->getNumber());
            }
         }

      for (auto edge = subNode->getExceptionSuccessors().begin(); edge != subNode->getExceptionSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (!isExitEdge(*edge))
            {
            TR::CFGEdge::createExceptionEdge(correspondingSubNode, correspondingSubNodes[succ->getNumber()], trMemory());
            }
         else
            {
            clonedRegionStructure->addExitEdge(correspondingSubNode, succ->getNumber(), 1);
            }
         }
      }

   clonedRegionStructure->setNestingDepth(getNestingDepth());
   clonedRegionStructure->setMaxNestingDepth(TR_Structure::getMaxNestingDepth());

   if (isNaturalLoop() &&
       whileLoops->find(this))
      {
      correspondingWhileLoops->add(clonedRegionStructure);
      }

   TR_InductionVariable *currInductionVariable = getFirstInductionVariable();
   TR_InductionVariable *prevClonedInductionVariable = NULL;
   while (currInductionVariable)
      {
      TR_InductionVariable *currClonedInductionVariable = new (trHeapMemory()) TR_InductionVariable();
      memcpy(currClonedInductionVariable, currInductionVariable, sizeof(TR_InductionVariable));
      if (!prevClonedInductionVariable)
         clonedRegionStructure->addInductionVariable(currClonedInductionVariable);
      else
         clonedRegionStructure->addAfterInductionVariable(prevClonedInductionVariable, currClonedInductionVariable);

      prevClonedInductionVariable = currClonedInductionVariable;
      currInductionVariable = currInductionVariable->getNext();
      }

   return clonedRegionStructure;
   }





void TR_BlockStructure::cloneStructureEdges(TR::Block **correspondingBlocks)
   {
   }




void TR_RegionStructure::cloneStructureEdges(TR::Block **correspondingBlocks)
   {
   TR_RegionStructure::Cursor si(*this);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subNode->getStructure()->cloneStructureEdges(correspondingBlocks);
      }

   TR_ScratchList<TR::CFGNode> seenExitNodes(trMemory());
   ListIterator<TR::CFGEdge> ei(&getExitEdges());
   TR::CFGEdge *edge;
   for (edge = ei.getFirst(); edge; edge= ei.getNext())
      {
      if (isExitEdge(edge))
         {
         TR::CFGNode *exitNode = edge->getTo();
         if (!seenExitNodes.find(exitNode))
           {
           seenExitNodes.add(exitNode);

           TR::Block   *correspondingBlock = correspondingBlocks[exitNode->getNumber()];
           if (correspondingBlock)
              {
              exitNode->setNumber(correspondingBlock->getNumber());
              }
           }
         }
      }
   }

List<TR::Block> *TR_BlockStructure::getBlocks(List<TR::Block> *blocksInRegion)
   {
   vcount_t visitCount = comp()->incVisitCount();
   return getBlocks(blocksInRegion, visitCount);
   }

List<TR::Block> *TR_RegionStructure::getBlocks(List<TR::Block> *blocksInRegion)
   {
   vcount_t visitCount = comp()->incVisitCount();
   return getBlocks(blocksInRegion, visitCount);
   }



//
// The following code adds the blocks so that the 'next' logical block is added
// after this block to ensure a consistent ordering of the blocks.
// This ordering is required by some users of getBlocks(), namely loopReducer.
//
List<TR::Block> *TR_BlockStructure::getBlocks(List<TR::Block> *blocksInRegion, vcount_t visitCount)
   {
   TR::Block * block = getBlock();
   if (block->getVisitCount() == visitCount)
      {
      return blocksInRegion;
      }

   block->setVisitCount(visitCount);
   blocksInRegion->add(block);

   if (block->getEntry())
      {
      TR::Block * nextBlock = block->getNextBlock();
      if (nextBlock && block->getStructureOf() != NULL && nextBlock->getStructureOf() &&
       block->getStructureOf()->getParent() == nextBlock->getStructureOf()->getParent() &&
          nextBlock->getVisitCount() != visitCount)
         {
         blocksInRegion->add(block->getNextBlock());
         block->getNextBlock()->setVisitCount(visitCount);
         }
      }

   return blocksInRegion;
   }

List<TR::Block> *TR_RegionStructure::getBlocks(List<TR::Block> *blocksInRegion, vcount_t visitCount)
   {
   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);

   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      subStruct->getBlocks(blocksInRegion, visitCount);
      }

   return blocksInRegion;
   }



void TR_Structure::addEdge(TR::CFGEdge *edge, bool isExceptionEdge)
   {
   TR_ASSERT(false, "addEdge called for non-region");
   }

void TR_RegionStructure::addEdge(TR::CFGEdge *edge, bool isExceptionEdge)
   {
   // Find the subnode containing the from node
   //
   TR::Block *from = toBlock(edge->getFrom());
   TR::Block *to = toBlock(edge->getTo());
   TR_StructureSubGraphNode *fromNode;
   TR_Structure             *fromStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (fromNode = si.getCurrent(); fromNode != NULL; fromNode = si.getNext())
      {
      fromStruct = fromNode->getStructure();
      if (fromStruct->contains(from->getStructureOf(), this))
         break;
      }
   TR_ASSERT(fromStruct != NULL,"Structure: bad structure when adding an edge");
   TR_ASSERT(fromNode != NULL, "Structure: fromNode of the edge not found in the structure");

   if (fromStruct->asRegion())
      {
      // If the subnode also contains the to block, let it handle the new edge
      //
      if (fromStruct->contains(to->getStructureOf(), this))
         {
         fromStruct->addEdge(edge, isExceptionEdge);
         return;
         }

      // Tell the from node to add an exit edge to the to node
      //
      fromStruct->addExternalEdge(from->getStructureOf(), to->getNumber(), isExceptionEdge);
      }

   // Find the subgraph node for the to block
   //
   TR_StructureSubGraphNode *toNode;
   TR_Structure             *toStruct = NULL;
   while (1)
      {
      TR_RegionStructure::Cursor si(*this);
      for (toNode = si.getCurrent(); toNode != NULL; toNode = si.getNext())
         {
         toStruct = toNode->getStructure();
         if (toStruct->contains(to->getStructureOf(), this))
            break;
         }

      if (!toNode)
         {
         if (to == comp()->getFlowGraph()->getEnd() /* &&
             to->getPredecessors().empty() */)
            {
            toStruct = to->getStructureOf();
            if (!toStruct)
               toStruct = new (trHeapMemory()) TR_BlockStructure(comp(), to->getNumber(), to);
            toStruct->setNumber(to->getNumber());
            toNode = new (trHeapMemory()) TR_StructureSubGraphNode(toStruct);
            addSubNode(toNode);
            toNode->setNumber(to->getNumber());
            }
         else
            TR_ASSERT(0,"Structure: bad structure when adding an edge");
         }

      // If the edge does not go into the head of the "to" structure, collapse it
      // into this region and try again.
      //
      if (toStruct->getNumber() == to->getNumber())
         break;
      toStruct->asRegion()->collapseIntoParent();
      }

   // Add an edge between the nodes if there isn't one already
   //
   TR::CFGEdgeList* list;
   if (isExceptionEdge)
      list = &(fromNode->getExceptionSuccessors());
   else
      list = &(fromNode->getSuccessors());

   auto iter = list->begin();
   for (; iter != list->end(); ++iter)
      if ((*iter)->getTo() == toNode)
         break;

   if (iter == list->end())
      {
      if (isExceptionEdge)
         TR::CFGEdge::createExceptionEdge(fromNode,toNode,trMemory());
      else
         TR::CFGEdge::createEdge(fromNode, toNode, trMemory());

      // Adding this edge may have given this region internal cycles.
      //
      if (!containsInternalCycles())
         checkForInternalCycles();
      }
   }

void TR_BlockStructure::addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge)
   {
   }

void TR_RegionStructure::addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge)
   {
   // Find the sub-node containing the "from" structure
   //
   TR_StructureSubGraphNode *fromNode;
   TR_Structure             *fromStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (fromNode = si.getCurrent(); fromNode != NULL; fromNode = si.getNext())
      {
      fromStruct = fromNode->getStructure();
      if (fromStruct->contains(from, this))
         break;
      }
   TR_ASSERT(fromNode,"Structure: bad structure when adding an edge");
   fromStruct->addExternalEdge(from, toNumber, isExceptionEdge);

   // Find the exit node for the node to be merged into.
   // If it doesn't exist, create it.  If it exists, return (nothing to do)
   //
   TR::CFGEdge *edge;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   si.reset();
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == toNumber &&
          edge->getFrom() == fromNode)
         return;
      }

   addExitEdge(fromNode, toNumber, isExceptionEdge);
   }

void TR_RegionStructure::collapseIntoParent()
   {
   if (debug("traceCollapseRegion"))
      {
      diagnostic("==> Structure before collapsing %d into parent %d\n",
                  getNumber(), getParent()->getNumber());
      comp()->getDebug()->print(comp()->getOutFile(), getParent(), 6);
      }

   TR_StructureSubGraphNode *node;
   TR::CFGNode               *toNode;
   TR_RegionStructure::Cursor nodes(*this);
   TR_StructureSubGraphNode *target;

   // Build up a bit vector for each node in this region
   //
   TR_BitVector regionNodes(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   for (node = nodes.getCurrent(); node; node = nodes.getNext())
      regionNodes.set(node->getNumber());

   // Find this node in the parent region and build a bit vector for each node
   // in the parent region.
   //
   TR_RegionStructure *parent = getParent()->asRegion();
   TR_RegionStructure::Cursor targets(*parent);
   TR_BitVector parentNodes(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   TR_StructureSubGraphNode *nodeInParent = NULL;
   TR_RegionStructure::Cursor pNodes(*parent);
   for (node = pNodes.getCurrent(); node; node = pNodes.getNext())
      {
      parentNodes.set(node->getNumber());
      if (node->getNumber() == getNumber())
         nodeInParent = node;
      }

   // If this region has internal cycles, the parent now has them too.
   // If this region is a natural loop and is not the entry node of
   // the parent, the parent now has internal cycles.
   //
   if (!parent->containsInternalCycles())
      {
      if (containsInternalCycles())
         parent->setContainsInternalCycles(true);
      else if (isNaturalLoop() && nodeInParent != parent->getEntry())
         parent->setContainsInternalCycles(true);
      }

   // Go through each subnode of this region and move it to the parent region.
   //
   TR_RegionStructure::Cursor subNodes(*this);
   for (node = subNodes.getCurrent(); node; node = subNodes.getNext())
      {
      parent->addSubNode(node);

      // Handle successor edges of this subnode.
      // Internal edges will stay as internal edges.
      // Exit edges to nodes in the parent region are turned into internal
      //    edges.
      // Other exit edges are made into exit edges of the parent region.
      //
      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         toNode = (*edge)->getTo();
         int32_t toNum = toNode->getNumber();
         if (regionNodes.get(toNum))
            {
            // Internal edge, just do nothing
            }
         else if (parentNodes.get(toNum))
            {
            // An exit edge that must be turned into an internal edge in the
            // parent region.
            //
            // Find the node of the parent's graph that represents the
            // same structure as the edge target and change the edge to
            // point to this node.
            // Remove the original edge from the child node to this target; the
            // target node in the child region is discarded.
            //
            TR_RegionStructure::Cursor pTargets(*parent);
            for (target = pTargets.getCurrent(); target; target = pTargets.getNext())
               {
               if (target->getNumber() == toNum)
                  {
                  // We have found the target node in the parent region.
                  // Find the edge from the child region to this node in the
                  // parent CFG and remove it if it is still there. Processing
                  // a previous subnode of the child region may have already
                  // removed it.
                  //
                  for (auto pred = target->getPredecessors().begin(); pred != target->getPredecessors().end(); ++pred)
                     {
                     if ((*pred)->getFrom()->getNumber() == getNumber() &&
                         toStructureSubGraphNode((*pred)->getFrom())->getStructure() == this)
                        {
                        TR::CFGEdgeList &successorList = nodeInParent->getSuccessors();
                        auto successorIterator = std::find(successorList.begin(), successorList.end(), *pred);
                        if (successorIterator != successorList.end()) successorList.erase(successorIterator);

                        target->getPredecessors().erase(pred);
                        break;
                        }
                     }

                  // Link up the edge from the child node to the target in the
                  // parent region.
                  //
                  (*edge)->setTo(target);
                  break;
                  }
               }
            TR_ASSERT(target, "Structure error collapsing regions");

            // Disconnect this edge from the original target node in the child
            // region. This target node is not used in the parent region.
            //
            TR::CFGEdgeList &predecessorList = toNode->getPredecessors();
            auto predecessorIterator = std::find(predecessorList.begin(), predecessorList.end(), *edge);
            if (predecessorIterator != predecessorList.end()) predecessorList.erase(predecessorIterator);
            }
         else
            {
            // An exit edge that is to remain as an exit edge for the parent
            // region.
            // Just add it as an exit edge.
            //
            parent->addExitEdge(node, toNum, false, *edge);
            }
         }

      // Do the same as the above, but for exception successors of the node
      // that is being moved from the child region to the parent region.
      //
      for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
         {
         toNode = (*edge)->getTo();
         int32_t toNum = toNode->getNumber();
         if (regionNodes.get(toNum))
            {
            // Internal edge, do nothing
            }
         else if (parentNodes.get(toNum))
            {
            TR_RegionStructure::Cursor pTargets(*parent);
            for (target = pTargets.getCurrent(); target; target = pTargets.getNext())
               {
               if (target->getNumber() == toNum)
                  {
                  for (auto pred = target->getExceptionPredecessors().begin(); pred != target->getExceptionPredecessors().end(); ++pred)
                     {
                     if (((*pred)->getFrom()->getNumber() == getNumber()) &&
                         (toStructureSubGraphNode((*pred)->getFrom())->getStructure() == this))
                        {
                        TR::CFGEdgeList &successorList = nodeInParent->getExceptionSuccessors();
                        auto successorIterator = std::find(successorList.begin(), successorList.end(), *pred);
                        if (successorIterator != successorList.end()) successorList.erase(successorIterator);

                        target->getExceptionPredecessors().erase(pred);
                        break;
                        }
                     }
                  (*edge)->setExceptionTo(target);
                  break;
                  }
               }
            TR_ASSERT(target, "Structure error collapsing regions");

            TR::CFGEdgeList &predecessorList = toNode->getExceptionPredecessors();
            auto predecessorIterator = std::find(predecessorList.begin(), predecessorList.end(), *edge);
            if (predecessorIterator != predecessorList.end()) predecessorList.erase(predecessorIterator);
            }
         else
            {
            // An exit edge that is to remain as an exit edge for the parent
            // region.
            // Just add it as an exit edge.
            //
            parent->addExitEdge(node, toNum, true, *edge);
            }
         }
      }

   // Change all predecessors of the child region in the parent's CFG to go to
   // the entry node of the child region instead.
   //
   for (auto edge = nodeInParent->getPredecessors().begin(); edge != nodeInParent->getPredecessors().end(); ++edge)
      (*edge)->setTo(getEntry());

   // Ditto for exception predecessors
   //
   for (auto edge = nodeInParent->getExceptionPredecessors().begin(); edge != nodeInParent->getExceptionPredecessors().end(); ++edge)
      (*edge)->setExceptionTo(getEntry());

   // Remove all remaining successors of the child region from the parent's
   // exit edge list.
   // Exit edges from the individual subnodes of the child region have already
   // been added as exit edges to the parent region.
   //
   for (auto edge = nodeInParent->getSuccessors().begin(); edge != nodeInParent->getSuccessors().end();)
      {
	  TR::CFGEdge* current = *(edge++);
      parent->getExitEdges().remove(current);
      TR::CFGNode *toNode = current->getTo();
      nodeInParent->removeSuccessor(current);
      toNode->removePredecessor(current);
      }
   for (auto edge = nodeInParent->getExceptionSuccessors().begin(); edge != nodeInParent->getExceptionSuccessors().end();)
      {
      TR::CFGEdge* current = *(edge++);
      parent->getExitEdges().remove(current);
      TR::CFGNode *toNode = current->getTo();
      nodeInParent->removeExceptionSuccessor(current);
      toNode->removeExceptionPredecessor(current);
      }

   // If the child region was the entry node of the parent, change the entry
   // node to be the child region's entry node.
   //
   if (parent->getEntry() == nodeInParent)
      parent->setEntry(getEntry());

   // Remove the subnode in the parent region that represents the child region.
   // All the subnodes of the child region have now been added to the parent
   // region.
   //
   parent->removeSubNode(nodeInParent);
   }


TR::CFGEdge *
TR_RegionStructure::addExitEdge(TR_StructureSubGraphNode *from, int32_t to, bool isExceptionEdge, TR::CFGEdge *origEdge)
   {
   ListIterator<TR::CFGEdge> it(&getExitEdges());
   TR::CFGEdge *cursor;
   for (cursor = it.getFirst(); cursor; cursor = it.getNext())
      {
      if (cursor->getTo()->getNumber() == to)
         break;
      }

   TR::CFGEdge *edge;
   TR::CFGNode *toNode = cursor ? cursor->getTo() : new (trHeapMemory()) TR_StructureSubGraphNode(to, trMemory());

   if (origEdge)
      {
      if (isExceptionEdge)
         origEdge->setExceptionTo(toNode);
      else
         origEdge->setTo(toNode);
      edge = origEdge;
      }
   else
      {
      edge = isExceptionEdge ?
         TR::CFGEdge::createExceptionEdge(from, toNode,trMemory()) :
         TR::CFGEdge::createEdge(from,  toNode, trMemory());
      }
   _exitEdges.add(edge);
   return edge;
   }

TR_StructureSubGraphNode *
TR_StructureSubGraphNode::create(int32_t num, TR_RegionStructure *region)
   {
   ListIterator<TR::CFGEdge> it(&region->getExitEdges());
   TR::CFGEdge *edge;
   for (edge = it.getFirst(); edge; edge = it.getNext())
      {
      if (edge->getTo()->getNumber() == num)
         break;
      }
   if (edge)
      return edge->getTo()->asStructureSubGraphNode();

   return new (region->trHeapMemory()) TR_StructureSubGraphNode(num, region->trMemory());
   }

TR_StructureSubGraphNode *TR_StructureSubGraphNode::asStructureSubGraphNode() {return this;}

void TR_BlockStructure::checkStructure(TR_BitVector *_blockNumbers)
   {
   TR_ASSERT(this->getNumber()==_block->getNumber(), "Number of BlockStructure is NOT the same as that of the block");
   TR_ASSERT(_blockNumbers->get(this->getNumber())==0, "Structure, Two blocks with the same number");
   //set the value of the bit representing this block number
   //
   _blockNumbers->set(this->getNumber());
   }

void TR_RegionStructure::checkStructure(TR_BitVector* _blockNumbers)
   {
   TR_StructureSubGraphNode *node;
   TR_ASSERT(this->getNumber()==getEntry()->getStructure()->getNumber(), "Entry node does not have same number as this RegionStructure");
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      TR_ASSERT(this==node->getStructure()->getParent(), "subGraphNode does not have this RegionStructure as its parent");
      TR_ASSERT(node->getNumber()==node->getStructure()->getNumber(), "subGraphNode does not have the same node number as its structure");
      TR_StructureSubGraphNode *pred, *succ;
      bool isConsistent;
      for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
         {
         pred = toStructureSubGraphNode((*edge)->getFrom());
         isConsistent = false;
         for (auto succEdge = pred->getSuccessors().begin(); succEdge != pred->getSuccessors().end(); ++succEdge)
            {
            if (*succEdge == *edge)
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT(isConsistent, "predecessor of this subGraph node not not contain this node in its successors");
         }
      for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
         {
         pred = toStructureSubGraphNode((*edge)->getFrom());
         isConsistent = false;
         for (auto succEdge = pred->getExceptionSuccessors().begin(); succEdge != pred->getExceptionSuccessors().end(); ++succEdge)
            {
            if ((*succEdge) == (*edge))
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT(isConsistent, "exception predecessor of this subGraph node not not contain this node in its exception successors");
         }

      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         succ = toStructureSubGraphNode((*edge)->getTo());
         isConsistent = false;
         for (auto predEdge = succ->getPredecessors().begin(); predEdge != succ->getPredecessors().end(); ++predEdge)
            {
            if (*predEdge == *edge)
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT(isConsistent, "successor of this subGraph node not not contain this node in its predecessors");
         }
      for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
         {
         succ = toStructureSubGraphNode((*edge)->getTo());
         isConsistent = false;
         TR::CFGEdge *predEdge;
         for (auto predEdge = succ->getExceptionPredecessors().begin(); predEdge != succ->getExceptionPredecessors().end(); ++predEdge)
            {
            if ((*predEdge) == *edge)
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT(isConsistent, "exception successor of this subGraph node not not contain this node in its exception predecessors");
         }
      node->getStructure()->checkStructure(_blockNumbers);
      }

   TR_ScratchList<TR_StructureSubGraphNode> exitNodes(trMemory());
   ListIterator<TR::CFGEdge> exitEdges(&_exitEdges);
   TR::CFGEdge *edge;
   for (edge = exitEdges.getFirst(); edge; edge = exitEdges.getNext())
      {
      TR_StructureSubGraphNode *toNode = toStructureSubGraphNode(edge->getTo());

      TR_StructureSubGraphNode *seenNode = NULL;
      ListIterator<TR_StructureSubGraphNode> exitNodesIt(&exitNodes);
      for (seenNode = exitNodesIt.getFirst(); seenNode; seenNode = exitNodesIt.getNext())
         {
         if ((seenNode->getNumber() == toNode->getNumber()) &&
             (seenNode != toNode))
            TR_ASSERT(0, "Exit edges to the same node number %d in region %p must have a unique structure subgraph node (node1 %p node2 %p)\n", seenNode->getNumber(), this, seenNode, toNode);
         }
      if (!exitNodes.find(toNode))
         exitNodes.add(toNode);
      }
   }





void TR_BlockStructure::resetAnalysisInfo()
   {
   setAnalysisInfo(NULL);
   }



void TR_RegionStructure::resetAnalysisInfo()
   {
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      node->getStructure()->resetAnalysisInfo();
   setAnalysisInfo(NULL);
   }




void TR_BlockStructure::resetAnalyzedStatus()
   {
   setAnalyzedStatus(false);
   }



void TR_RegionStructure::resetAnalyzedStatus()
   {
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      node->getStructure()->resetAnalyzedStatus();
   setAnalyzedStatus(false);
   }


bool TR_BlockStructure::markStructuresWithImproperRegions()
   {
   setContainsImproperRegion(false);
   return false;
   }



bool TR_RegionStructure::markStructuresWithImproperRegions()
   {
   bool b = false;
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (node->getStructure()->markStructuresWithImproperRegions())
         b = true;
      }

   if (containsInternalCycles())
      b = true;

   if (b)
      setContainsImproperRegion(true);
   else
      setContainsImproperRegion(false);

   return b;
   }




TR::Block *TR_BlockStructure::setBlock(TR::Block *b)
   {
   _block = b;
   TR_BlockStructure *old = b->getStructureOf();
   if (old)
      {
      if (old->isEntryOfShortRunningLoop())
         setIsEntryOfShortRunningLoop();
      }
   return b;
   }


bool TR_BlockStructure::changeContinueLoopsToNestedLoops(TR_RegionStructure *root)
   {
   return false;
   }

bool TR_RegionStructure::changeContinueLoopsToNestedLoops(TR_RegionStructure *root)
   {
   static bool traceMyself=(debug("TR_traceChangeContinues") != 0);

   bool changed = false;
   TR_RegionStructure::Cursor si(*this);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getStructure()->changeContinueLoopsToNestedLoops(root))
         changed = true;
      }

   if (isNaturalLoop())
      {
      TR::CFG *cfg=comp()->getFlowGraph();
      TR::CFGEdge *backEdgeToHeader = NULL;
      TR::Block *entryBlock = getEntryBlock();
      TR_ScratchList<TR::CFGEdge> splitEdges(trMemory());
      TR_ScratchList<TR::CFGEdge> incomingEdges(trMemory());
      unsigned numNewHeaders=0;
      for (auto nextEdge = entryBlock->getPredecessors().begin(); nextEdge != entryBlock->getPredecessors().end(); ++nextEdge)
         {
         TR::Block *pred = (*nextEdge)->getFrom()->asBlock();
         if (contains(pred->getStructureOf(), getParent()))
            {
            // special case backedge: if back-edge is a fall-through, don't do anything to this loop
            if (pred->getExit()->getNextTreeTop() != entryBlock->getEntry())
               {
               numNewHeaders = 0;
               break;
               }
            if (traceMyself)
               diagnostic("found backedge %d\n", pred->asBlock()->getNumber());
            if (backEdgeToHeader)
               {
               if (traceMyself)
                  diagnostic("\tsplitting old backedge pred %d\n", backEdgeToHeader->getFrom()->asBlock()->getNumber());
               splitEdges.add(backEdgeToHeader);
               backEdgeToHeader = *nextEdge;
               numNewHeaders++;
               }
            else
               backEdgeToHeader = *nextEdge;
            }
         else
            {
            if (traceMyself)
               diagnostic("found incoming edge from %d\n", pred->asBlock()->getNumber());
            TR_ASSERT((*nextEdge)->getTo()->asBlock() == entryBlock,"Natural loop shouldn't have incoming edge to nonheader block");
            incomingEdges.add(*nextEdge);
            }
         }

      if (numNewHeaders == 1 && performTransformation(comp(), "%s transforming continues in loop %d to %d new nested loops\n", OPT_DETAILS, getEntryBlock()->asBlock()->getNumber(), numNewHeaders))
         {
         TR::Block *newHeader = NULL;
         TR::Block *prevHeader = entryBlock;
         ListIterator<TR::CFGEdge> seIterator(&splitEdges);
         // for each split edge, change the branch to point to one of the new header blocks
         for (TR::CFGEdge *splitEdge = seIterator.getFirst(); splitEdge; splitEdge=seIterator.getNext())
            {
            TR::Block *newHeader = TR::Block::createEmptyBlock(entryBlock->getEntry()->getNode(), comp(), splitEdge->getFrequency(), prevHeader);

            cfg->addNode(newHeader, entryBlock->getParentStructureIfExists(cfg));
            cfg->addEdge(newHeader, prevHeader);

            if (traceMyself)
               diagnostic("Rerouting edge %d->old header %d to new header %d\n", splitEdge->getFrom()->asBlock()->getNumber(), splitEdge->getTo()->asBlock()->getNumber(), newHeader->getNumber());

            // change destination at the end of splitEdge->getFrom()->asBlock() to branch to newHeader rather than entryBlock, updating edges as needed
            TR::Block::redirectFlowToNewDestination(comp(), splitEdge, newHeader, false);

            // paste trees for this header ahead of prevHeader
            TR::TreeTop *beforePrevHeader = prevHeader->getEntry()->getPrevTreeTop();
            newHeader->getExit()->join(prevHeader->getEntry());
            if (beforePrevHeader)
               beforePrevHeader->join(newHeader->getEntry());
            else
               comp()->setStartTree(newHeader->getEntry());

            prevHeader = newHeader;
            }

         // for each incoming edge to the original header, redirect to the outermost new header block
         // note: newHeader here already holds the header for the outermost loop
         ListIterator<TR::CFGEdge> ieIterator(&incomingEdges);
         for (TR::CFGEdge *incomingEdge = ieIterator.getFirst(); incomingEdge; incomingEdge=ieIterator.getNext())
            {
            if (traceMyself)
               {
               diagnostic("Incoming [%p] edge %d->%d\n", incomingEdge, incomingEdge->getFrom()->asBlock()->getNumber(), incomingEdge->getTo()->asBlock()->getNumber());
               diagnostic("\tassuming it should be ->%d\n", entryBlock->getNumber());
               diagnostic("\tredirecting to ->%d\n", newHeader->getNumber());
               }

            TR::Block::redirectFlowToNewDestination(comp(), incomingEdge, newHeader, false);
            }

         changed = true;
         }
      }

   return changed;
   }


TR_StructureSubGraphNode *
TR_RegionStructure::findNodeInHierarchy(int32_t num)
   {
   TR_RegionStructure::Cursor it(*this);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getNumber() == num)
         return node;
      }
   return getParent() ?
      getParent()->findNodeInHierarchy(num) : 0;
   }

int32_t
TR_Structure::getNumberOfLoops()
   {
   TR_RegionStructure * region = this->asRegion();
   int32_t numberOfLoops = 0;

   if (region == NULL)
      {
      return 0;
      }

   if (!region->isAcyclic())
      {
      numberOfLoops += 1;
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *n = it.getFirst(); n; n = it.getNext())
      {
      numberOfLoops += n->getStructure()->getNumberOfLoops();
      }

   return numberOfLoops;
   }


// merged  -- block to be merged and disappear
// mergedInto -- block to be keeped, but keep the number of merged,
//               reason: we may merge block 2 to block 3, but first block should be numbered as 2
void TR_Structure::mergeBlocks(TR::Block *merged, TR::Block *mergedInto)
   {
   // Grab the block numbers before they are merged
   //
   if (debug("dumpStructure"))
      dumpOptDetails(comp(), "\nStructures after merging blocks %d and %d:\n", merged->getNumber(), mergedInto->getNumber());

   // Update the structure to reflect the block merging
   //
   int16_t newFrequency = std::max(merged->getFrequency(), mergedInto->getFrequency());
   merged->setFrequency(newFrequency);

   mergeInto(merged, mergedInto);

   // Make the block structure for the merged into block represent the merged block.
   //
   TR_BlockStructure *s = mergedInto->getStructureOf()->asBlock();
   s->setBlock(merged);
   merged->setStructureOf(s);

   TR_ASSERT(s->getNumber() == merged->getNumber(),"Structure: bad structure when merging blocks");
   if (debug("dumpStructure"))
      comp()->getDebug()->print(comp()->getOutFile(), comp()->getFlowGraph()->getStructure(), 6);
   }


void TR_Structure::mergeInto(TR::Block *merged, TR::Block *mergedInto)
   {
   // If we have reached this point the structure is invalid since the parent
   // should have handled it.
   //
   TR_ASSERT(false,"Structure: bad structure when merging blocks");
   }


// merged  -- block to be merged and disappear
// mergedInto -- block to be keeped, but keep the number of merged,
//               reason: we may merge block 2 to block 3, but first block should be numbered as 2
void TR_RegionStructure::mergeInto(TR::Block *merged, TR::Block *mergedInto)
   {
   bool mergedIsCatch = false;
   if (merged->isCatchBlock())
      mergedIsCatch = true;


   // Find the subnode containing the block to be removed
   //
   TR_StructureSubGraphNode *fromNode;
   TR_Structure             *fromStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (fromNode = si.getCurrent(); fromNode != NULL; fromNode = si.getNext())
      {
      fromStruct = fromNode->getStructure();
      if (fromStruct->contains(merged->getStructureOf(), this))
         break;
      }
   TR_ASSERT(fromStruct != NULL,"Structure: bad structure when merging blocks");

   // If the subnode also contains the block merged into, let it deal with it.
   //
   if (fromStruct->contains(mergedInto->getStructureOf(), this))
      {
      int32_t mergedIntoNum = mergedInto->getNumber();
      fromStruct->mergeInto(merged, mergedInto);
      if (fromStruct->getKind() == TR_Structure::Region && fromStruct->getNumber() == mergedIntoNum)
         {
         fromStruct->renumber(merged->getNumber());
         fromNode->setNumber(merged->getNumber());
         }
      return;
      }

   // Find the subgraph node for the node to be merged into
   //
   int32_t toBlockNum = mergedInto->getNumber();
   TR_StructureSubGraphNode *toNode = NULL;
   si.reset();
   for (toNode = si.getCurrent(); toNode != NULL; toNode = si.getNext())
      {
      if (toNode->getStructure()->getNumber() == toBlockNum)
         break;
      }
   TR_ASSERT(toNode != NULL,"Structure: bad structure when merging blocks");

   // If the subnode containing the merged block is a block structure it must
   // be removed. Otherwise it must repair itself.
   //
   TR_BlockStructure *bs = fromStruct->asBlock();
   if (bs == NULL)
      {
      fromStruct->removeMergedBlock(merged, mergedInto);

      // Change the "to" structure to become the merged block
      //
      toNode->getStructure()->renumber(merged->getNumber());
      toNode->setNumber(merged->getNumber());
      if (mergedIsCatch)
         {
         while (!toNode->getPredecessors().empty())
            {
            TR::CFGEdge *normalEdge = toNode->getPredecessors().front();
            TR::CFGNode *normalPred = normalEdge->getFrom();
            normalPred->removeSuccessor(normalEdge);
            toNode->removePredecessor(normalEdge);
            normalPred->addExceptionSuccessor(normalEdge);
            toNode->addExceptionPredecessor(normalEdge);
            }
         }
      return;
      }

   if (fromNode == getEntry())
      setEntry(toNode);

   // Change the "to" structure to become the merged block
   //
   toNode->getStructure()->renumber(merged->getNumber());
   toNode->setNumber(merged->getNumber());

   while (!fromNode->getPredecessors().empty())
   {
	  TR::CFGEdge * edge = fromNode->getPredecessors().front();
	  fromNode->getPredecessors().pop_front();
      edge->setTo(toNode);
   }

   while (!fromNode->getExceptionPredecessors().empty())
   {
	   TR::CFGEdge * edge = fromNode->getExceptionPredecessors().front();
	   fromNode->getExceptionPredecessors().pop_front();
	   edge->setExceptionTo(toNode);
   }

   // Remove the 'removed' node from the list of predecessors of its successors
   //
   ListIterator<TR::CFGEdge> exitEdges(&_exitEdges);
   TR::CFGEdge *edge;
   for (edge = exitEdges.getFirst(); edge; )
      {
      TR::CFGEdge *nextEdge = exitEdges.getNext();
      if (edge->getFrom() == fromNode)
         removeEdge(edge, true);
      edge = nextEdge;
      }

   while (!fromNode->getSuccessors().empty())
      {
      edge = fromNode->getSuccessors().front();
      removeEdge(edge, false);
      }

   while (!fromNode->getExceptionSuccessors().empty())
      {
      edge = fromNode->getExceptionSuccessors().front();
      removeEdge(edge, false);
      }

   if (!toNode->getStructure()->asBlock())
      {
      for (auto edge = toNode->getSuccessors().begin(); edge != toNode->getSuccessors().end();)
         {
         if ((*edge)->getTo() == toNode)
            removeEdge(*(edge++), true);
         else
            ++edge;
         }

      for (auto edge = toNode->getExceptionSuccessors().begin(); edge != toNode->getExceptionSuccessors().end();)
         {
         if ((*edge)->getTo() == toNode)
            removeEdge(*(edge++), true);
         else
            ++edge;
         }
      }

   // If the "from" node has not yet been removed, remove it explicitly
   //
   if (fromNode->getStructure()->getParent())
      removeSubNode(fromNode);
   }








void TR_Structure::removeMergedBlock(TR::Block *merged, TR::Block *mergedInto)
   {
   TR_ASSERT(false,"Structure: bad structure when merging blocks");
   }



void TR_RegionStructure::removeMergedBlock(TR::Block *merged, TR::Block *mergedInto)
   {
   // Find the subnode containing the block to be removed
   //
   TR_StructureSubGraphNode *node;
   TR_Structure             *s = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      s = node->getStructure();
      if (s->contains(merged->getStructureOf(), this))
         break;
      }
   TR_ASSERT(s != NULL,"Structure: bad structure when merging blocks");

   // If the subnode is not a block, let it deal with the removal
   // Otherwise remove the block structure from the region.
   //
   ListElement<TR::CFGEdge> *edge;
   TR_StructureSubGraphNode *toNode;
   TR_BlockStructure *bs = s->asBlock();
   if (bs == NULL)
      {
      s->removeMergedBlock(merged, mergedInto);

      bool mergedIsCatch = false;
      if (merged->isCatchBlock())
         mergedIsCatch = true;

      // Change the node number of the exit node(s) to be the number that the
      // merged block will eventually have.
      //
      for (edge = _exitEdges.getListHead(); edge != NULL; edge = edge->getNextElement())
         {
         toNode = toStructureSubGraphNode(edge->getData()->getTo());

         if (toNode->getNumber() == mergedInto->getNumber())
            {
	    if (mergedIsCatch)
	       {
               TR::CFGNode *normalPred;
               for (auto normalEdge = toNode->getPredecessors().begin(); normalEdge != toNode->getPredecessors().end();)
                  {
                  TR::CFGEdge *ne = (*(normalEdge)++);
                  normalPred = ne->getFrom();
                  normalPred->removeSuccessor(ne);
                  toNode->removePredecessor(ne);
                  normalPred->addExceptionSuccessor(ne);
                  toNode->addExceptionPredecessor(ne);
                  }
	       }

           toNode->setNumber(merged->getNumber());
           break;
            }
         }
      return;
      }

   // Find the exit edge representing the edge being merged. There should be
   // only one regular exit edge, but there can also be exception exit edges.
   // These should be removed too.
   //
   TR_StructureSubGraphNode *mergedIntoNode = NULL;
   for (edge = _exitEdges.getListHead(); edge != NULL; edge = edge->getNextElement())
      {
      if (edge->getData()->getFrom() == node)
         {
         toNode = toStructureSubGraphNode(edge->getData()->getTo());
         if (toNode->getNumber() == mergedInto->getNumber())
            mergedIntoNode = toNode;
         removeEdge(edge->getData(), true);
         }
      }

   // The resulting block with have the merged block number
   //
   TR_ASSERT(mergedIntoNode, "bad structure merging blocks");
   mergedIntoNode->setNumber(merged->getNumber());

   // Make all edges into this node into exit edges and remove the subnode
   //
   for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
      {
      _exitEdges.add((*edge));
      }

   for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
      {
      _exitEdges.add((*edge));
      }

   removeSubNode(node);

   // The node now acts as an exit node, so clear out its structure reference
   //
   node->setStructure(NULL);
   }



void TR_BlockStructure::collectCFGEdgesTo(int32_t toNum, List<TR::CFGEdge> *cfgEdges)
   {
   TR::CFGEdge *edge;
   TR_SuccessorIterator sit(getBlock());
   for (edge = sit.getFirst(); edge; edge = sit.getNext())
      {
      TR::CFGNode *succ = edge->getTo();
      if (succ->getNumber() == toNum)
         cfgEdges->add(edge);
      }
   }


void TR_RegionStructure::collectCFGEdgesTo(int32_t toNum, List<TR::CFGEdge> *cfgEdges)
   {
   ListElement<TR::CFGEdge> *edge;
   for (edge = _exitEdges.getListHead(); edge != NULL; edge = edge->getNextElement())
      {
      if (edge->getData()->getTo()->getNumber() == toNum)
        toStructureSubGraphNode(edge->getData()->getFrom())->getStructure()->collectCFGEdgesTo(toNum, cfgEdges);
      }
   }







void TR_BlockStructure::renumber(int32_t num)
   {
   _nodeIndex = num;
   _block->setNumber(num);
   }


void TR_RegionStructure::renumber(int32_t num)
   {
   ListElement<TR::CFGEdge> *edge = NULL, *nextEdge = NULL, *prevEdge = NULL;
   edge = _exitEdges.getListHead();
   //for (edge = _exitEdges.getListHead(); edge != NULL; prevEdge = edge, edge = nextEdge)
   while (edge)
      {
      nextEdge = edge->getNextElement();
      if (edge->getData()->getTo()->getNumber() == num)
         {
         bool isExceptionEdge = true;
         if (std::find(edge->getData()->getFrom()->getExceptionSuccessors().begin(), edge->getData()->getFrom()->getExceptionSuccessors().end(), edge->getData()) == edge->getData()->getFrom()->getExceptionSuccessors().end())
            isExceptionEdge = false;

         if (isExceptionEdge)
              edge->getData()->setExceptionTo(getEntry());
         else
            edge->getData()->setTo(getEntry());

         if (prevEdge)
            prevEdge->setNextElement(nextEdge);
         else
            _exitEdges.setListHead(nextEdge);
         edge = nextEdge;
         continue;
         }
      prevEdge = edge;
      edge = nextEdge;
      }

   _nodeIndex = num;
   getEntry()->setNumber(num);
   getEntry()->getStructure()->renumber(num);
   }


/*
void TR_RegionStructure::renumber(int32_t num)
   {
   ListElement<TR::CFGEdge> *edge, *nextEdge = NULL, *prevEdge = NULL;
   for (edge = _exitEdges.getListHead(); edge != NULL; prevEdge = edge, edge = nextEdge)
      {
      nextEdge = edge->getNextElement();
      if (edge->getData()->getTo()->getNumber() == num)
         {
         bool isExceptionEdge = true;
         if (!edge->getData()->getFrom()->getExceptionSuccessors().find(edge->getData()))
            isExceptionEdge = false;

         if (isExceptionEdge)
              edge->getData()->setExceptionTo(getEntry());
         else
            edge->getData()->setTo(getEntry());

         if (prevEdge)
            prevEdge->setNextElement(nextEdge);
         else
            _exitEdges.setListHead(nextEdge);
         }
      }

   _nodeIndex = num;
   getEntry()->setNumber(num);
   getEntry()->getStructure()->renumber(num);
   }
*/



void TR_BlockStructure::resetVisitCounts(vcount_t num)
   {
   }

void TR_RegionStructure::resetVisitCounts(vcount_t num)
   {
   TR_StructureSubGraphNode *node;
   TR_Structure             *s = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      node->setVisitCount(num);
      TR::CFGEdge *edge;
      TR_SuccessorIterator sit(node);
      for (edge = sit.getFirst(); edge; edge = sit.getNext())
         edge->setVisitCount(num);
      s = node->getStructure();
      s->resetVisitCounts(num);
      }
   }






bool TR_BlockStructure::renumberRecursively(int32_t origNum, int32_t num)
   {
   if (origNum == getNumber())
      {
      renumber(num);
      return true;
      }
   return false;
   }

bool TR_RegionStructure::renumberRecursively(int32_t origNum, int32_t num)
   {
   TR_StructureSubGraphNode *node;
   TR_Structure             *s = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      s = node->getStructure();
      if (s->renumberRecursively(origNum, num))
        node->setNumber(num);
      }

   ListElement<TR::CFGEdge> *edge, *nextEdge = NULL;
   for (edge = _exitEdges.getListHead(); edge != NULL; edge = nextEdge)
      {
      nextEdge = edge->getNextElement();
      if (edge->getData()->getTo()->getNumber() == origNum)
         edge->getData()->getTo()->setNumber(num);
      }


   if (origNum == getNumber())
     {
     _nodeIndex = num;
     return true;
     }

   return false;
   }





void TR_RegionStructure::computeInvariantSymbols()
   {
   int32_t symRefCount = comp()->getSymRefCount();
   _invariantSymbols = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
   _invariantSymbols->setAll(symRefCount);
   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   getBlocks(&blocksInRegion);
   vcount_t visitCount = comp()->incVisitCount();
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();

      TR::TreeTop *currentTree = entryTree->getNextTreeTop();

      while (!(currentTree == exitTree))
         {
         TR::Node *currentNode = currentTree->getNode();
         updateInvariantSymbols(currentNode, visitCount);
         currentTree = currentTree->getNextRealTreeTop();
         }
      }
   }

void TR_RegionStructure::updateInvariantSymbols(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();

      if (symReference->getSymbol()->isVolatile())
         _invariantSymbols->reset(symReference->getReferenceNumber());

      if (opCode.isResolveCheck())
         {
         TR::SymbolReference *childSymRef = node->getFirstChild()->getSymbolReference();
         _invariantSymbols->reset(childSymRef->getReferenceNumber());
         childSymRef->getUseDefAliases().getAliasesAndSubtractFrom(*_invariantSymbols);
         }

      if (!opCode.isLoadVar() && (opCode.getOpCodeValue() != TR::loadaddr))
         {
         if (!opCode.isCheck())
            {
            symReference->getUseDefAliases().getAliasesAndSubtractFrom(*_invariantSymbols);
            }

         if (opCode.isStore())
            _invariantSymbols->reset(symReference->getReferenceNumber());
         }
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateInvariantSymbols(node->getChild(i), visitCount);
   }


void TR_RegionStructure::computeInvariantExpressions()
   {
   computeInvariantSymbols();

   int32_t nodeCount = comp()->getNodeCount();
   _invariantExpressions = new (trStackMemory()) TR_BitVector(nodeCount, trMemory(), stackAlloc, growable);

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   getBlocks(&blocksInRegion);
   vcount_t visitCount = comp()->incVisitCount();
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();

      TR::TreeTop *currentTree = entryTree->getNextTreeTop();

      while (!(currentTree == exitTree))
         {
         TR::Node *currentNode = currentTree->getNode();
         updateInvariantExpressions(currentNode, visitCount);
         currentTree = currentTree->getNextRealTreeTop();
         }
      }
   }

void TR_RegionStructure::updateInvariantExpressions(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (isExprTreeInvariant(node))
      _invariantExpressions->set(node->getGlobalIndex());

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateInvariantExpressions(node->getChild(i), visitCount);
   }


void TR_RegionStructure::resetInvariance()
   {
   _invariantSymbols = NULL;
   _invariantExpressions = NULL;
   }

bool TR_RegionStructure::isExprInvariant(TR::Node *expr, bool usePrecomputed)
   {
   if (_invariantExpressions && usePrecomputed)
      return _invariantExpressions->get(expr->getGlobalIndex());

   return isExprTreeInvariant(expr);
   }

bool TR_RegionStructure::isExprTreeInvariant(TR::Node *expr)
   {
   if (!_invariantSymbols)
      computeInvariantSymbols();

   vcount_t visitCount = comp()->incOrResetVisitCount();

   return isSubtreeInvariant(expr, visitCount);
   }

  bool TR_RegionStructure::isSymbolRefInvariant(TR::SymbolReference *symRef)
   {
   if(!_invariantSymbols)
      computeInvariantSymbols();
   return _invariantSymbols->get(symRef->getReferenceNumber());
   }

bool TR_RegionStructure::isSubtreeInvariant(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return true;

   if (node->getOpCode().isCall())
      return false;

   if (node->getOpCode().hasSymbolReference())
      {
      if (!_invariantSymbols->get(node->getSymbolReference()->getReferenceNumber()))
         return false;
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      if (!isSubtreeInvariant(node->getChild(i), visitCount))
         return false;
      }

   return true;
   }



/****************************************/





void TR_BlockStructure::hoistInvariantsOutOfNestedLoops(TR_LocalTransparency *transparencyInfo, TR_BitVector **optSetInfo, bool insideLoop, TR_BlockStructure *outerLoopInvariantBlock, TR_RegionStructure *outerLoop, int32_t bitVectorSize)
   {
   }


#if 0
void
TR_SequenceStructure::hoistInvariantsOutOfNestedLoops(
   TR_LocalTransparency *transparencyInfo, TR_BitVector **optSetInfo, bool insideLoop, TR_Structure *outerLoop, int32_t bitVectorSize)
   {
   bool isInvariantLoop = false;

   if (_count > 1)
      {
      if ((_subNodes[0]->asBlock() != NULL) && (_subNodes[1]->asDoWhile() != NULL))
         {
         for (int32_t i = 0; i < _count; i++)
            _subNodes[i]->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, true, this, bitVectorSize);

         isInvariantLoop = true;

         TR_BlockStructure *loopInvariantBlock = _subNodes[0]->asBlock();

         if (insideLoop)
            {
            TR_SequenceStructure *outerSequenceStructure = outerLoop->asSequence();
            TR_BlockStructure *outerLoopInvariantBlock = outerSequenceStructure->getSubNode(0)->asBlock();

            if (!(optSetInfo[loopInvariantBlock->getNumber()])->isEmpty())
               {
               TR_BitVector *optMask = new (trStackMemory()) TR_BitVector(bitVectorSize, trMemory(), stackAlloc);

               TR_BitVectorIterator bvi(*(optSetInfo[loopInvariantBlock->getNumber()]));
               int32_t nextOptimalComputation = -1;
               while (bvi.hasMoreElements())
                  {
                  nextOptimalComputation = bvi.getNextElement();
                  if (!(optSetInfo[outerLoopInvariantBlock->getNumber()])->get(nextOptimalComputation))
                     {
                     if (outerSequenceStructure->getSubNode(1)->isExpressionTransparentIn(nextOptimalComputation, transparencyInfo))
                        {
                        TR::Block *innerLoopInvariantBlock = loopInvariantBlock->getBlock();
                        TR::TreeTop *exitTree = innerLoopInvariantBlock->getExit();
                        TR::TreeTop *currentTree = innerLoopInvariantBlock->getEntry();
                        TR::TreeTop *treeToBeHoisted = NULL;
                        while (currentTree != exitTree)
                           {
                           if (currentTree->getNode()->getOpCode().isCheck())
                              {
                              if (currentTree->getNode()->getLocalIndex() == nextOptimalComputation)
                                 {
                                 treeToBeHoisted = currentTree;
                                 break;
                                 }
                              }
                           else if (currentTree->getNode()->getOpCode().isStore())
                              {
                              TR::Node *currentNode = currentTree->getNode();

                              if (currentNode->getSymbolReference()->getSymbol()->isAuto())
                                 {
                                 if (currentNode->getFirstChild()->getLocalIndex() == nextOptimalComputation)
                                    {
                                    treeToBeHoisted = currentTree;
                                    break;
                                    }
                                 }
                              }
                           currentTree = currentTree->getNextTreeTop();
                           }

                        if (treeToBeHoisted != NULL)
                           {
                           TR::TreeTop *prevTree = treeToBeHoisted->getPrevTreeTop();
                           TR::TreeTop *nextTree = treeToBeHoisted->getNextTreeTop();
                           prevTree->setNextTreeTop(nextTree);
                           nextTree->setPrevTreeTop(prevTree);

                           TR::TreeTop *outerExitTree = outerLoopInvariantBlock->getBlock()->getLastRealTreeTop();

                           TR::TreeTop *outerPrevTree = outerExitTree->getPrevTreeTop();
                           treeToBeHoisted->setNextTreeTop(outerExitTree);
                           treeToBeHoisted->setPrevTreeTop(outerPrevTree);
                           outerPrevTree->setNextTreeTop(treeToBeHoisted);
                           outerExitTree->setPrevTreeTop(treeToBeHoisted);
                           }

                        if (debug("tracePR"))
                           {
                           dumpOptDetails(comp(), "Hoisting loop invariant computation %d over nested loops (lifted from inner block_%d to outer block_%d)\n", nextOptimalComputation, loopInvariantBlock->getNumber(), outerLoopInvariantBlock->getNumber());
                           }

                        /////actually hoist the invariant
                        /////also remember to change the optSetInfo for the
                        /////two invariant blocks
                        optMask->set(nextOptimalComputation);
                        }
                     }
                  }
               *(optSetInfo[loopInvariantBlock->getNumber()]) -= *optMask;
               *(optSetInfo[outerLoopInvariantBlock->getNumber()]) |= *optMask;
               }
            }
         }
      }

   if (!isInvariantLoop)
      {
      for (int32_t i = 0; i < _count; i++)
         _subNodes[i]->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, insideLoop, outerLoop, bitVectorSize);
      }
   }
#endif



void
TR_RegionStructure::hoistInvariantsOutOfNestedLoops(
   TR_LocalTransparency *transparencyInfo, TR_BitVector **optSetInfo, bool insideLoop,
   TR_BlockStructure *outerLoopInvariantBlock, TR_RegionStructure *outerLoop, int32_t bitVectorSize)
   {
   bool isInvariantLoop = false;
   TR_RegionStructure *loopStructure = NULL;
   TR_BlockStructure *loopInvariantBlock = NULL;
   TR_StructureSubGraphNode *loopInvariantNode = NULL;

   if (isCanonicalizedLoop() && getEntry()->getSuccessors().size() == 2)
       {
       for (auto succ = getEntry()->getSuccessors().begin(); succ != getEntry()->getSuccessors().end(); ++succ)
          {
          TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) (*succ)->getTo();
          TR_BlockStructure *succStructure = succNode->getStructure()->asBlock();
          if (succStructure == NULL)
             break;
          else
             {
             if (succStructure->isLoopInvariantBlock())
                {
                loopInvariantNode = succNode;
                loopInvariantBlock = succStructure;
                }
             }
          }

       if (loopInvariantBlock)
          {
          if (loopInvariantNode->getSuccessors().size() == 1)
             {
             loopStructure = ((TR_StructureSubGraphNode *) loopInvariantNode->getSuccessors().front()->getTo())->getStructure()->asRegion();
             if (loopStructure)
                {
                if (!loopStructure->getEntry()->getPredecessors().empty())
                   isInvariantLoop = true;
                }
             }
          }

       if (isInvariantLoop)
          {
          TR_StructureSubGraphNode *node;
          TR_RegionStructure::Cursor si(*this);
          for (node = si.getCurrent(); node != NULL; node = si.getNext())
             node->getStructure()->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, true, loopInvariantBlock, loopStructure, bitVectorSize);

          if (insideLoop)
             {
             if (!(optSetInfo[loopInvariantBlock->getNumber()])->isEmpty())
                {
                TR_BitVector *optMask = new (trStackMemory()) TR_BitVector(bitVectorSize, trMemory(), stackAlloc);

                TR_BitVectorIterator bvi(*(optSetInfo[loopInvariantBlock->getNumber()]));
                int32_t nextOptimalComputation = -1;
                while (bvi.hasMoreElements())
                   {
                   nextOptimalComputation = bvi.getNextElement();
                   if (!(optSetInfo[outerLoopInvariantBlock->getNumber()])->get(nextOptimalComputation))
                      {
                      if (outerLoop->isExpressionTransparentIn(nextOptimalComputation, transparencyInfo))
                         {
                         TR::Block *innerLoopInvariantBlock = loopInvariantBlock->getBlock();
                         TR::TreeTop *exitTree = innerLoopInvariantBlock->getExit();
                         TR::TreeTop *currentTree = innerLoopInvariantBlock->getEntry();
                         TR::TreeTop *treeToBeHoisted = NULL;
                         while (currentTree != exitTree)
                            {
                            if (currentTree->getNode()->getOpCode().isCheck())
                               {
                               if (currentTree->getNode()->getLocalIndex() == nextOptimalComputation)
                                  {
                                  treeToBeHoisted = currentTree;
                                  break;
                                  }
                               }
                            else if (currentTree->getNode()->getOpCode().isStore())
                               {
                               TR::Node *currentNode = currentTree->getNode();

                               if (currentNode->getSymbolReference()->getSymbol()->isAuto())
                                   {
                                  if (currentNode->getFirstChild()->getLocalIndex() == nextOptimalComputation)
                                     {
                                     treeToBeHoisted = currentTree;
                                     break;
                                     }
                                  }
                               }
                            currentTree = currentTree->getNextTreeTop();
                            }

                         if (treeToBeHoisted != NULL)
                            {
                            TR::TreeTop *prevTree = treeToBeHoisted->getPrevTreeTop();
                            TR::TreeTop *nextTree = treeToBeHoisted->getNextTreeTop();
                            prevTree->setNextTreeTop(nextTree);
                            nextTree->setPrevTreeTop(prevTree);

                            TR::TreeTop *outerExitTree = outerLoopInvariantBlock->getBlock()->getLastRealTreeTop();

                            TR::TreeTop *outerPrevTree = outerExitTree->getPrevTreeTop();
                            treeToBeHoisted->setNextTreeTop(outerExitTree);
                            treeToBeHoisted->setPrevTreeTop(outerPrevTree);
                            outerPrevTree->setNextTreeTop(treeToBeHoisted);
                            outerExitTree->setPrevTreeTop(treeToBeHoisted);
                            }

                        dumpOptDetails(comp(), "\nO^O PARTIAL REDUNDANCY ELIMINATION: Hoisting loop invariant computation %d over nested loops (lifted from inner block_%d to outer block_%d)\n", nextOptimalComputation, loopInvariantBlock->getNumber(), outerLoopInvariantBlock->getNumber());

                        /////actually hoist the invariant
                        /////also remember to change the optSetInfo for the
                        /////two invariant blocks
                        optMask->set(nextOptimalComputation);
                        }
                     }
                  *(optSetInfo[loopInvariantBlock->getNumber()]) -= *optMask;
                  *(optSetInfo[outerLoopInvariantBlock->getNumber()]) |= *optMask;
                  }
               }
            }
         }
      }

   if (!isInvariantLoop)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*this);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         node->getStructure()->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, insideLoop, outerLoopInvariantBlock, outerLoop, bitVectorSize);
      }
   }




bool TR_BlockStructure::isExpressionTransparentIn(int32_t nextOptimalComputation, TR_LocalTransparency *transparencyInfo)
   {
   TR_LocalTransparency::ContainerType *info = transparencyInfo->getAnalysisInfo(getNumber());
   if (info && info->get(nextOptimalComputation))
      return true;

   return false;
   }



bool TR_RegionStructure::isExpressionTransparentIn(int32_t nextOptimalComputation, TR_LocalTransparency *transparencyInfo)
   {
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (!node->getStructure()->isExpressionTransparentIn(nextOptimalComputation, transparencyInfo))
         return false;
      }

   return true;
   }


TR_InductionVariable* TR_RegionStructure::findMatchingIV(TR::SymbolReference *symRef)
   {
   TR::Symbol* symbol = symRef->getSymbol();
   for(TR_InductionVariable *v = getFirstInductionVariable(); v; v = v->getNext())
      {
      if (symbol == v->getLocal())
         return v;
      }
   return NULL;
   }
