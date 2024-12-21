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

#include "optimizer/StructuralAnalysis.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/StackMemoryRegion.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/DominatorVerifier.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/Dominators.hpp"
#include "ras/Debug.hpp"

void TR_RegionAnalysis::simpleIterator (TR_Stack<int32_t>& workStack,
                                        StructureBitVector& vector,
                                        WorkBitVector &regionNodes,
                                        WorkBitVector &nodesInPath,
                                        bool &cyclesFound,
                                        TR::Block *hdrBlock,
                                        bool doThisCheck)
   {
   TR_BitVectorIterator cursor(vector);
   while (cursor.hasMoreElements())
      {
      uint32_t regionNum = cursor.getNextElement();
      StructInfo &next = getInfo(regionNum);

      // The exit block must only be made part of the region headed by the entry
      // block. It provides an exit for all other regions.
      //
      if (doThisCheck && next._succ.isEmpty() &&
          next._originalBlock == _cfg->getEnd())
         {
         if (hdrBlock->getNumber() != 0)
            continue;
         }

      //short circuit the iteration
      //In the original version we would descend into another level of a recursion
      //but if we do it here, we would screw up big time since we can't push the same node three times.
      //We should be able to push the same node at most twice
      //so if we come across the same node the second time (nodesInPath[next]) we could say that we are done with this
      //particular node and move on
      if (regionNodes.get(regionNum))
         {
         if (!cyclesFound && nodesInPath.get(regionNum) &&
             _dominators.dominates(hdrBlock, next._originalBlock))
            {
            cyclesFound = true;
            if (trace())
               {
               traceMsg(comp(), "cycle found at node = %d\n", (uint32_t)regionNum);
               }
            }
         }
      else
         {
         if (_dominators.dominates(hdrBlock, next._originalBlock))
            {
            workStack.push(regionNum);
            }
         }
      }
   }

void TR_RegionAnalysis::StructInfo::initialize(TR::Compilation * comp, int32_t index, TR::Block *block)
   {
   _structure       = new (comp->getFlowGraph()->structureMemoryRegion()) TR_BlockStructure(comp, block->getNumber(), block);
   _originalBlock   = block;
   _nodeIndex       = index;
   }

/**
 * Create a structure to represent each basic block. These will be the
 * leaves of the region graphs.
 * Create a structure information block for each such structure to control
 * analysis. The structure information blocks are ordered in depth-first
 * order of basic blocks, so we can do a postorder scan of the structures.
 */
void TR_RegionAnalysis::createLeafStructures(TR::CFG *cfg, TR::Region &region)
   {
   TR::CFGNode              *cfgNode;
   TR::Block                *b;
   int32_t                  nodeNum;

   _totalNumberOfNodes = 0;

   _infoTable = (StructInfo**) _workingMemoryRegion.allocate((cfg->getNumberOfNodes()+1)*sizeof(StructInfo*));

   // We need two passes to initialize the array of StructInfo objects
   // because the order of nodes is not necessarily sequential.
   // The first constructs the objects in order and the second initializes them
   // (in a different order)
   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      	// Construct the right number of table entries
      _infoTable[_totalNumberOfNodes+1] = new (region) StructInfo(region);
      _totalNumberOfNodes++;
      }

   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      // Initialize each table entry (not necessarily sequential order)
      b = toBlock(cfgNode);
      nodeNum = _dominators._dfNumbers[b->getNumber()];
      StructInfo &si = getInfo(nodeNum);
      si.initialize(_compilation, nodeNum, b);

      // Initialize predecessors and successors for the leaf structures.
      //
      TR::Block *next;
      int32_t index;
      for (auto p = b->getPredecessors().begin(); p != b->getPredecessors().end(); ++p)
         {
         next = toBlock((*p)->getFrom());
         index = _dominators._dfNumbers[next->getNumber()];
         si._pred.set(index);
         }
      for (auto p = b->getSuccessors().begin(); p != b->getSuccessors().end(); ++p)
         {
         next = toBlock((*p)->getTo());
         index = _dominators._dfNumbers[next->getNumber()];
         si._succ.set(index);
         }
      for (auto p = b->getExceptionPredecessors().begin(); p != b->getExceptionPredecessors().end(); ++p)
         {
         next = toBlock((*p)->getFrom());
         index = _dominators._dfNumbers[next->getNumber()];
         si._exceptionPred.set(index);
         }
      for (auto p = b->getExceptionSuccessors().begin(); p != b->getExceptionSuccessors().end(); ++p)
         {
         next = toBlock((*p)->getTo());
         index = _dominators._dfNumbers[next->getNumber()];
         si._exceptionSucc.set(index);
         }
      }
   }

/**
 * Mainline for performing Region Analysis.
 */
TR_Structure *TR_RegionAnalysis::getRegionsImpl(TR::Compilation *comp, TR_Dominators& dominators, TR::CFG* cfg)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   #if DEBUG
   if (debug("verifyDominator"))
   {
      TR_DominatorVerifier verifyDominator(dominators);
   }
   #endif

   TR_ASSERT(cfg, "cfg is NULL\n");

   TR_RegionAnalysis ra(comp, dominators, cfg, stackMemoryRegion);
   ra._trace = comp->getOption(TR_TraceSA);

   ra._useNew = !comp->getOption(TR_DisableIterativeSA);
   if (ra.trace())
   {
      traceMsg(comp, "Blocks before Region Analysis:\n");
      comp->getDebug()->print(comp->getOutFile(), cfg);
   }

   ra.createLeafStructures(cfg, stackMemoryRegion);

   // Loop through the node set until there is only one node left - this is the
   // root of the control tree.
   //
   TR_Structure *result = ra.findRegions(stackMemoryRegion);

   return result;
   }

TR_Structure *TR_RegionAnalysis::getRegions(TR::Compilation *comp, TR::CFG* cfg, bool acceptUnreachableBlocks)
   {
   // Calculate dominators
   // This has the side-effect of renumbering the blocks in depth-first order
   //
   // If acceptUnreachableBlocks is false, dominators constructor will early return
   //
   TR_Dominators dominators(comp, cfg, acceptUnreachableBlocks);
   if (!acceptUnreachableBlocks && dominators.hasUnreachableBlocks())
      return NULL;

   return getRegionsImpl(comp, dominators, cfg);
   }

TR_Structure *TR_RegionAnalysis::getRegions(TR::Compilation *comp, TR::ResolvedMethodSymbol* methSym)
   {
   // Calculate dominators
   // This has the side-effect of renumbering the blocks in depth-first order
   //
   TR_Dominators dominators(comp);
   TR::CFG *cfg = methSym->getFlowGraph();

   return getRegionsImpl(comp, dominators, cfg);
   }

TR_Structure *TR_RegionAnalysis::getRegions(TR::Compilation *comp)
   {
   // Calculate dominators
   // This has the side-effect of renumbering the blocks in depth-first order
   //
   TR_Dominators dominators(comp);
   TR::CFG *cfg = comp->getFlowGraph();

   return getRegionsImpl(comp, dominators, cfg);
   }

/**
 * Find regions in the flow graph and reduce them to structures.
 *
 * A region consists of one of:
 *    1) a natural loop (aka a proper cyclic region), or
 *    2) a proper acyclic region, or
 *    3) an improper region
 *
 * A proper acyclic region is a sub-graph with a single entry node and no cycles.
 * A proper cyclic region is a sub-graph with a single entry node and one or more
 * back-edges to the entry node, and no internal cycles.
 * An improper region is a sub-graph with a single entry node and which contains
 * cycles.
 *
 * There are two passes.
 * On the first pass we go bottom up in the flow graph looking for natural loops.
 * On the second pass we go bottom up in the flow graph looking for regions large
 * enough to consider.
 * The root region is returned.
 */
TR_Structure *TR_RegionAnalysis::findRegions(TR::Region &memRegion)
   {
   // Create work areas
   TR_BitVector regionNodes(memRegion);
   TR_BitVector nodesInPath(memRegion);
   // Create the array to hold created cfg nodes
   SubGraphNodes cfgNodes(_totalNumberOfNodes, memRegion);

   // Loop through the active nodes in depth-first postorder looking for natural
   // loops.
   //
   int32_t nodeIndex;
   for (nodeIndex = _totalNumberOfNodes-1; nodeIndex >= 0; nodeIndex--)
      {
      StructInfo &node = getInfo(nodeIndex);

      // Skip nodes that aren't in the flow graph any more
      //
      if (node._structure == NULL)
         continue;

      TR_RegionStructure *region;

      // Look for a natural loop. The nodes in a natural loop consist only
      // of the nodes on paths that flow through the back edges of the loop.
      //
      region = findNaturalLoop(node, regionNodes, nodesInPath);
      if (region != NULL)
      	 buildRegionSubGraph(region, node, regionNodes, cfgNodes, memRegion);
      }

   // Loop through the active nodes in depth-first postorder looking for other
   // regions.
   //
   for (nodeIndex = _totalNumberOfNodes-1; nodeIndex >= 0; nodeIndex--)
      {
      StructInfo &node = getInfo(nodeIndex);

      // Skip nodes that aren't in the flow graph any more
      //
      if (node._structure == NULL)
         continue;

      TR_RegionStructure *region;

      // Look for a proper acyclic region or an improper region
      //
      region = findRegion(node, regionNodes, nodesInPath);

      if (region != NULL)
         buildRegionSubGraph(region, node, regionNodes,cfgNodes, memRegion);
      }

   TR_ASSERT(getInfo(0)._structure, "Region Analysis, root region not found");
   return getInfo(0)._structure;
   }

/**
 * See if there is a natural loop at the given node and find its region nodes
 * A natural loop is a loop with one or more back-edges.
 * A back-edge is identified by the fact that the "from" node of the edge is
 * dominated by the entry node.
 */
TR_RegionStructure *TR_RegionAnalysis::findNaturalLoop(StructInfo &node,
                                                       WorkBitVector &regionNodes,
                                                       WorkBitVector &nodesInPath)
   {
   regionNodes.empty();
   regionNodes.set(node._nodeIndex);
   nodesInPath.empty();
   bool cyclesFound = false;

   int32_t numBackEdges = 0;

   TR_BitVectorIterator cursor(node._pred);
   while (cursor.hasMoreElements())
      {
      StructInfo &backEdgeNode = getInfo(cursor.getNextElement());
      if (_dominators.dominates(node._originalBlock, backEdgeNode._originalBlock))
         {
         // A back-edge has been found. Add its loop nodes to the region
         //

         if (_useNew)
            {
	         addNaturalLoopNodesIterativeVersion(backEdgeNode, regionNodes, nodesInPath, cyclesFound, node._originalBlock);
	         }
         else
            {
            addNaturalLoopNodes(backEdgeNode, regionNodes, nodesInPath, cyclesFound, node._originalBlock);
            }

         numBackEdges++;
         }
      }

   if (numBackEdges == 0)
      return NULL;

   TR_RegionStructure *region = new (_structureMemoryRegion) TR_RegionStructure(_compilation, node._structure->getNumber() /* node._nodeIndex */);
   if (cyclesFound)
      {
      if (trace())
         traceMsg(comp(), "   Found improper cyclic region %d\n",node._nodeIndex);
      region->setContainsInternalCycles(true);
      }
   else
      {
      if (trace())
         traceMsg(comp(), "   Found natural loop region %d\n",node._nodeIndex);
      }
   return region;
   }


void TR_RegionAnalysis::addNaturalLoopNodesIterativeVersion(StructInfo &node, WorkBitVector &regionNodes, WorkBitVector &nodesInPath, bool &cyclesFound, TR::Block *hdrBlock)
   {
   //special case for addNaturalLoops
   if (regionNodes.get(node._nodeIndex))
      {
      if (nodesInPath.get(node._nodeIndex))
         {
         cyclesFound = true;
         if (trace())
            {
               traceMsg(comp(), "cycle found at node = %d\n", node._nodeIndex);
            }
         }
      return;
      }

   //We will be pushing elements backwards to emulate processing in a depth-first order
   //Also, we will be pushing the same node twice in order to update nodesInPath correctly
   TR_Stack<int32_t> workStack(comp()->trMemory(), 8, false, stackAlloc);
   workStack.push(node._nodeIndex);

   while (!workStack.isEmpty())
      {
      int32_t index = workStack.pop();

      if (nodesInPath.get(index))
         {
         // If node is in path, in region and cycles have been found then processing
         // the node again will not have any sideeffects, so we can skip it completely
         if (regionNodes.get(index) && cyclesFound)
            continue;
         nodesInPath.reset(index);
         continue;
         }
      else
         {
         workStack.push(index); //push again but now it will be marked in nodesInPath,
         //so the next time we come across this index we won't process it again
         regionNodes.set(index);
         nodesInPath.set(index);
         }

      if (trace())
         {
         traceMsg(comp(), "addNaturalLoopNodesIterativeVersion, index = %d\n", index);
         }

      StructInfo& next = getInfo(index);
      //process the preds of next
      simpleIterator(workStack, next._pred, regionNodes, nodesInPath,
            cyclesFound, hdrBlock);
      //process the exception preds of next
      simpleIterator(workStack, next._exceptionPred, regionNodes, nodesInPath,
            cyclesFound, hdrBlock);
      }
   }

void TR_RegionAnalysis::addNaturalLoopNodes(StructInfo &node, WorkBitVector &regionNodes, WorkBitVector &nodesInPath, bool &cyclesFound, TR::Block *hdrBlock)
   {
   int32_t index = node._nodeIndex;

   if (trace())
      {
      traceMsg(comp(), "addNaturalLoopNodes, index = %d\n", index);
      }

   // If the node was already found in the region we can stop tracking this path.
   //
   if (regionNodes.get(index))
      {
      if (nodesInPath.get(index))
         {
         cyclesFound = true;
         if (trace())
            {
               traceMsg(comp(), "cycle found at node = %d\n", index);
            }
         }
      return;
      }

   // Add this node to the region and to this path and look at its predecessors.
   //
   regionNodes.set(index);
   nodesInPath.set(index);

   TR_BitVectorIterator cursor(node._pred);
   while (cursor.hasMoreElements())
      {
      StructInfo &next = getInfo(cursor.getNextElement());
      if (_dominators.dominates(hdrBlock, next._originalBlock))
         addNaturalLoopNodes(next, regionNodes, nodesInPath, cyclesFound, hdrBlock);
      }
   TR_BitVectorIterator eCursor(node._exceptionPred);
   while (eCursor.hasMoreElements())
      {
      StructInfo &next = getInfo(eCursor.getNextElement());
      if (_dominators.dominates(hdrBlock, next._originalBlock))
         addNaturalLoopNodes(next, regionNodes, nodesInPath, cyclesFound, hdrBlock);
      }
   nodesInPath.reset(index);
   }


/**
 * Find the nodes that would be part of a region whose entry is the given node.
 *
 * The region will consist of the entry node and recursively all successors that
 * are dominated by the entry node.
 *
 * At the same time, determine whether there are cycles in the region.
 * Create a proper acyclic region or an improper region if there are enough nodes
 * to justify it.
 */
TR_RegionStructure *TR_RegionAnalysis::findRegion(StructInfo &node,
		                                          WorkBitVector &regionNodes,
		                                          WorkBitVector &nodesInPath)
   {

   bool cyclesFound = false;
   regionNodes.empty();
   nodesInPath.empty();

   if (_useNew)
      {
      addRegionNodesIterativeVersion(node, regionNodes, nodesInPath, cyclesFound, node._originalBlock);
      }
   else
      {
      addRegionNodes(node, regionNodes, nodesInPath, cyclesFound, node._originalBlock);
      }
   if (!cyclesFound && (node._nodeIndex > 0))
      {
      if (regionNodes.elementCount() < 100)
         return NULL;
      }

   TR_RegionStructure *region = new (_structureMemoryRegion) TR_RegionStructure(_compilation, node._structure->getNumber() /* node._nodeIndex */);
   if (cyclesFound)
      {
      if (trace())
         traceMsg(comp(), "   Found improper cyclic region %d\n",node._nodeIndex);
      region->setContainsInternalCycles(true);
      }
   else
      {
      if (trace())
         traceMsg(comp(), "   Found proper acyclic region %d\n",node._nodeIndex);
      }
   return region;
   }

void TR_RegionAnalysis::addRegionNodesIterativeVersion(StructInfo &node, WorkBitVector &regionNodes, WorkBitVector &nodesInPath, bool &cyclesFound, TR::Block *hdrBlock)
   {
   //We will be pushing elements backwards to emulate processing in a depth-first order
   //Also, we will be pushing the same node twice in order to update nodesInPath correctly
   TR_Stack<int32_t> workStack(comp()->trMemory(), 8, false, stackAlloc);
   workStack.push(node._nodeIndex);
   while (!workStack.isEmpty())
      {
      int32_t index = workStack.pop();
      if (nodesInPath.get(index))
         {
         nodesInPath.reset(index);
         continue;
         }
      else
         {
         workStack.push(index); //push again but now it will be marked in nodesInPath,
         //so the next time we come across this index we won't process it again
         regionNodes.set(index);
         nodesInPath.set(index);
         }

      if (trace())
         {
         traceMsg(comp(), "addRegionNodesIterativeVersion, index = %d\n", index);
         }

      StructInfo& next = getInfo(index);
      //process the succs of next
      simpleIterator(workStack, next._succ, regionNodes, nodesInPath,
            cyclesFound, hdrBlock, true);

      simpleIterator(workStack, next._exceptionSucc, regionNodes, nodesInPath,
            cyclesFound, hdrBlock);
      }
   }

void TR_RegionAnalysis::addRegionNodes(StructInfo &node, WorkBitVector &regionNodes, WorkBitVector &nodesInPath, bool &cyclesFound, TR::Block *hdrBlock)
   {
   int32_t index = node._nodeIndex;

	if (trace())
	{
	traceMsg(comp(), "addRegionNodes, index = %d\n",index);
	}

   // If the node was already found in the region we can stop tracking this path.
   //
   if (regionNodes.get(index))
      {
      if (nodesInPath.get(index))
         {
         cyclesFound = true;
         if (trace())
            {
               traceMsg(comp(), "cycle found at node = %d\n", index);
            }
         }
      return;
      }

   // Add this node to the region and to this path and look at its successors.
   //
   regionNodes.set(index);
   nodesInPath.set(index);

   TR_BitVectorIterator cursor(node._succ);
   while (cursor.hasMoreElements())
      {
      StructInfo &next = getInfo(cursor.getNextElement());

      // The exit block must only be made part of the region headed by the entry
      // block. It provides an exit for all other regions.
      //
      if ((next._succ.isEmpty()) &&
          next._originalBlock == _cfg->getEnd())
         {
         if (hdrBlock->getNumber() != 0)
            continue;
         }
      if (_dominators.dominates(hdrBlock, next._originalBlock))
         addRegionNodes(next, regionNodes, nodesInPath, cyclesFound, hdrBlock);

      }

   TR_BitVectorIterator eCursor(node._exceptionSucc);
   while (eCursor.hasMoreElements())
      {
      StructInfo &next = getInfo(eCursor.getNextElement());
      if (_dominators.dominates(hdrBlock, next._originalBlock))
         addRegionNodes(next, regionNodes, nodesInPath, cyclesFound, hdrBlock);
      }
   nodesInPath.reset(index);
   }

/**
 * Build the sub-graph for a structure that represents a region and update the
 * flow graph.
 */
void TR_RegionAnalysis::buildRegionSubGraph(TR_RegionStructure *region,
                                            StructInfo &entryNode,
                                            WorkBitVector &regionNodes,
                                            SubGraphNodes &cfgNodes,
                                            TR::Region &memRegion)
   {
   int32_t                   fromIndex, toIndex;
   TR_StructureSubGraphNode *from, *to;
   TR::CFGEdge               *edge;

   // Initialize the array of cfg nodes to be used to represent the region subgraph.
   //
   for (fromIndex = 0; fromIndex < _totalNumberOfNodes; fromIndex++)
	  cfgNodes[fromIndex] = NULL;
   //memset(cfgNodes, 0, _totalNumberOfNodes*sizeof(TR_StructureSubGraphNode*));

   // Build the sub-graph representing the region, and update the flow graph by
   // removing internal edges and moving edges to external successors from the
   // non-entry node to the entry node.

   // Bits cannot be turned off while iterating over a CS2 sparse bit vector,
   // so they are accumulated in a separate vector and turned off after iteration
   StructureBitVector bitsToBeRemoved(memRegion);

   TR_BitVectorIterator rCursor(regionNodes);
   while (rCursor.hasMoreElements())
      {
      fromIndex = rCursor.getNextElement();
      StructInfo &fromNode = getInfo(fromIndex);

      if (cfgNodes[fromIndex] == NULL)
         cfgNodes[fromIndex] = new (_structureMemoryRegion) TR_StructureSubGraphNode(fromNode._structure);
      from = cfgNodes[fromIndex];
      region->addSubNode(from);

      TR_BitVectorIterator cursor(fromNode._succ);
      while (cursor.hasMoreElements())
         {
         toIndex = cursor.getNextElement();
         StructInfo &toNode = getInfo(toIndex);
         if (cfgNodes[toIndex] == NULL)
            {
            if (regionNodes.get(toIndex))
               cfgNodes[toIndex] = new (_structureMemoryRegion) TR_StructureSubGraphNode(toNode._structure);
            else
               cfgNodes[toIndex] = new (_structureMemoryRegion) TR_StructureSubGraphNode(toNode._structure->getNumber(), _structureMemoryRegion);
            }
         to = cfgNodes[toIndex];
         edge = TR::CFGEdge::createEdge(from,  to, _structureMemoryRegion);
         if (regionNodes.get(toIndex))
            {
            toNode._pred.reset(fromIndex);
            bitsToBeRemoved.set(toIndex);
            }
         else
            {
            region->addExitEdge(edge);
            if (&fromNode != &entryNode)
                  {
                  // Change the successor edge to come from the new region
                  //
               toNode._pred.reset(fromIndex);
               toNode._pred.set(entryNode._nodeIndex);
               entryNode._succ.set(toIndex);
               }
            }
         }
      fromNode._succ -= bitsToBeRemoved;

      bitsToBeRemoved.empty();
      TR_BitVectorIterator eCursor(fromNode._exceptionSucc);
      while (eCursor.hasMoreElements())
         {
         toIndex = eCursor.getNextElement();
         StructInfo &toNode = getInfo(toIndex);
         if (cfgNodes[toIndex] == NULL)
            {
            if (regionNodes.get(toIndex))
               cfgNodes[toIndex] = new (_structureMemoryRegion) TR_StructureSubGraphNode(toNode._structure);
            else
               cfgNodes[toIndex] = new (_structureMemoryRegion) TR_StructureSubGraphNode(toNode._structure->getNumber(), _structureMemoryRegion);
            }
         to = cfgNodes[toIndex];
         edge = TR::CFGEdge::createExceptionEdge(from, to, _structureMemoryRegion);
         if (regionNodes.get(toIndex))
            {
            toNode._exceptionPred.reset(fromIndex);
            bitsToBeRemoved.set(toIndex);
            }
         else
            {
            region->addExitEdge(edge);
            if (&fromNode != &entryNode)
                  {
               toNode._exceptionPred.reset(fromIndex);
               toNode._exceptionPred.set(entryNode._nodeIndex);
               entryNode._exceptionSucc.set(toIndex);
               }
            }
         }
      fromNode._exceptionSucc -= bitsToBeRemoved;
      fromNode._structure = NULL;
      }

   // The new structure replaces the entry node.
   //
   entryNode._structure = region;

   // Grab the entry node before losing the cfgNode info.
   //
   region->setEntry(cfgNodes[entryNode._nodeIndex]);
   if (trace())
      {
      _compilation->getDebug()->print(_compilation->getOutFile(), region, 6);
      traceMsg(comp(), "   Structure after finding a region:\n");
      _compilation->getDebug()->print(_compilation->getOutFile(), this, 6);
      }
   }
