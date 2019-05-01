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

#include "optimizer/RematTools.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/Block.hpp"

void RematSafetyInformation::add(TR::TreeTop *argStore, TR::SparseBitVector &symRefDependencies)
   {
   dependentSymRefs.push_back(symRefDependencies);
   argumentTreeTops.push_back(argStore);
   rematTreeTops.push_back(argStore);
   }
   
void RematSafetyInformation::add(TR::TreeTop *argStore, TR::TreeTop *rematStore)
   {
   TR::SparseBitVector symrefDeps(comp->allocator());
   symrefDeps[rematStore->getNode()->getSymbolReference()->getReferenceNumber()] = true;
   dependentSymRefs.push_back(symrefDeps);
   argumentTreeTops.push_back(argStore);
   rematTreeTops.push_back(rematStore);
   }
   
void RematSafetyInformation::dumpInfo(TR::Compilation *comp)
   {
   for (uint32_t i = 0; i < dependentSymRefs.size(); ++i)
      {
      traceMsg(comp,"  Arg Remat Safety Info for priv arg store node %d", argumentTreeTops[i]->getNode()->getGlobalIndex());
      if (rematTreeTops[i])
         {
         if (rematTreeTops[i] != argumentTreeTops[i])
            traceMsg(comp,"     partial remat candidate node %d", rematTreeTops[i]->getNode()->getGlobalIndex());
         else
            traceMsg(comp,"     node candidate for full remat");
         traceMsg(comp,"    dependent symrefs: ");
         (*comp) << dependentSymRefs[i];
         traceMsg(comp,"\n");
         }
      else
         {
         traceMsg(comp,"    candidate is unsafe for remat - no candidates under consideration");
         }
      }
   }

/*
 * This function allows the remat methods to follow nodes along a path. The path is described
 * as a bit vector of blocks that may be visited, including both the start and the end blocks.
 * To ensure the path is deterministic, none of the blocks along the path may be a merge point,
 * except for the start block. Due to this, the function must be provided with the start block,
 * to avoid revisiting it.
 *
 * Modifies the provided TreeTop pointer to point at the next treetop or the treetop for
 * the first valid successor if one exists.
 *
 * Returns true if it was able to find a valid treetop, false otherwise.
 */
bool RematTools::getNextTreeTop(TR::TreeTop *&treeTop, TR_BitVector *blocksToVisit, TR::Block *startBlock)
   {
   if (!blocksToVisit || !treeTop->getNode() || treeTop->getNode()->getOpCodeValue() != TR::BBEnd)
      {
      treeTop = treeTop->getNextTreeTop();
      return true;
      }

   TR::Block *block = treeTop->getNode()->getBlock();
   TR::Block *nextBlock = NULL;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   blocksToVisit->reset(block->getNumber());
#endif

   for (auto e = block->getSuccessors().begin(); e != block->getSuccessors().end(); ++e)
      if (blocksToVisit->get((*e)->getTo()->getNumber()) && (*e)->getTo() != startBlock)
         {
         nextBlock = (*e)->getTo()->asBlock();
         break;
         }

   if (!nextBlock)
      for (auto e = block->getExceptionSuccessors().begin(); e != block->getExceptionSuccessors().end(); ++e)
         if (blocksToVisit->get((*e)->getTo()->getNumber()) && (*e)->getTo() != startBlock)
            {
            nextBlock = (*e)->getTo()->asBlock();
            break;
            }

   if (nextBlock)
      {
      if (nextBlock->getPredecessors().size() + nextBlock->getExceptionPredecessors().size() != 1)
         {
         TR_ASSERT(0, "blocks on the path must have a single predecessor");
         return false;
         }
      else
         {
         treeTop = nextBlock->getFirstRealTreeTop();
         return true;
         }
      }

   TR_ASSERT(0, "there should always be a remaining block to visit");
   return false;
   }
   
/*
 * This function is used to walk the nodes from each of the privatized inliner
 * args down to (a) gather nodes we need to check for safety and (b) prempt
 * further checking if not needed
 * Return TR_yes if the node is a candidate for remat after safety checks
 * (eg it is an expression containing loads and constants), TR_maybe if it is a
 * constant expression (we won't duplicate because constant prop should sort these),
 * TR_no if remat is unsafe
 * Side-effect - adds node to scanTarget if returning true, otherwise none
 */
TR_YesNoMaybe RematTools::gatherNodesToCheck(TR::Compilation *comp,
      TR::Node *privArg, TR::Node *currentNode, TR::SparseBitVector &scanTargets,
      TR::SparseBitVector &symRefsToCheck, bool trace, TR::SparseBitVector &visitedNodes)
   {
   visitedNodes[currentNode->getGlobalIndex()] = true;

   TR::ILOpCode &opCode = currentNode->getOpCode();
   if (opCode.hasSymbolReference() && !opCode.isLoad())
      {
      if (trace)
         traceMsg(comp, "  priv arg remat: Can't fully remat [%p] due to [%p] - non-load with a symref", privArg, currentNode);
      return TR_no;
      }
   else if (opCode.isLoadVarDirect())
      {
      if (currentNode->getSymbolReference()->hasKnownObjectIndex() ||
          currentNode->getSymbol()->isConstObjectRef())
         {
         // In these limited cases we are dealing with a known constant object
         // which we can trust so we don't need to scan for safety
         return TR_yes;
         }
      else
         {
         scanTargets[currentNode->getGlobalIndex()] = true;
         symRefsToCheck[currentNode->getSymbolReference()->getReferenceNumber()] = true;
         return TR_yes;
         }
      }
   else if (opCode.isLoadIndirect())
      {
      // cannot rematerialize an array access without also creating a spine check
      // avoiding this case for now but this case can be handled in the future by creating a spine check
      //
      if (comp->requiresSpineChecks() && currentNode->getSymbol()->isArrayShadowSymbol())
         {
         if (trace)
            traceMsg(comp, "  priv arg remat: Can't fully remat [%p] due to [%p] - array access needs spine check", privArg, currentNode);

         return TR_no;
         }

      // only add this node to the checking set if the root of the dereference
      // chain does not prempt remat checks
      if (gatherNodesToCheck(comp, privArg, currentNode->getFirstChild(), scanTargets, symRefsToCheck, trace, visitedNodes) != TR_no)
         {
         scanTargets[currentNode->getGlobalIndex()] = true;
         symRefsToCheck[currentNode->getSymbolReference()->getReferenceNumber()] = true;
         return TR_yes;
         }
      // chained TR_no so no diagnostics here
      return TR_no;
      }
   else if (opCode.isLoadConst())
      {
      // constants are ok too, but we don't need to clone them since
      // constant prop should take care of them
      return TR_maybe;
      }
   else if (opCode.isArithmetic()  || opCode.isConversion() ||
            opCode.isMax()         || opCode.isMin()        ||
            opCode.isArrayLength() || (opCode.isBooleanCompare() && !opCode.isBranch())
           )
      {
      // gather the child nodes of interest separately until we know it is worth checking them
      TR::SparseBitVector childScanTargets(comp->allocator());
      TR::SparseBitVector childSymRefsToCheck(comp->allocator());
      TR_YesNoMaybe candidateForRemat = TR_maybe;
      for (uint16_t i = 0; i < currentNode->getNumChildren(); ++i)
         {
         // avoid exponential traversal
         if (visitedNodes.ValueAt(currentNode->getChild(i)->getGlobalIndex()))
            continue;

         TR_YesNoMaybe childResult = gatherNodesToCheck(comp, privArg, currentNode->getChild(i), childScanTargets,
                                                   childSymRefsToCheck, trace, visitedNodes);
         if (childResult == TR_no)
            {
            if (trace)
               traceMsg(comp, "  priv arg remat: Can't fully remat [%p] due to [%p] - unsafe arithmetic child %d", privArg, currentNode, i);
            candidateForRemat = TR_no;
            break;
            }
         else if (childResult == TR_yes)
            {
            candidateForRemat = TR_yes;
            }
         }
      // only union in child symrefs if there is benefit from doing so
      if (candidateForRemat == TR_yes)
         {
         scanTargets |= childScanTargets;
         symRefsToCheck |= childSymRefsToCheck;
         }
      return candidateForRemat;
      }
   else
      {
      // anything we haven't considered is dangerous as a conservative assumption
      if (trace)
         traceMsg(comp, "  guarded call remat: Can't fully remat [%p] due to [%p] - unhandled case", privArg, currentNode);
      return TR_no;
      }
   }
   
TR_YesNoMaybe RematTools::gatherNodesToCheck(TR::Compilation *comp,
      TR::Node *privArg, TR::Node *currentNode, TR::SparseBitVector &scanTargets,
      TR::SparseBitVector &symRefsToCheck, bool trace)
   {
   TR::SparseBitVector visitedNodes(comp->allocator());
   return gatherNodesToCheck(comp, privArg, currentNode, scanTargets,
                             symRefsToCheck, trace, visitedNodes);
   }
   
void RematTools::walkNodesCalculatingRematSafety(TR::Compilation *comp,
      TR::Node *currentNode, TR::SparseBitVector &scanTargets, 
      TR::SparseBitVector &enabledSymRefs, TR::SparseBitVector &unsafeSymRefs,
      bool trace, TR::SparseBitVector &visitedNodes)
   {
   for (uint16_t i = 0; i < currentNode->getNumChildren(); ++i)
      {
      if (visitedNodes.ValueAt(currentNode->getChild(i)->getGlobalIndex()))
         continue;

      visitedNodes[currentNode->getChild(i)->getGlobalIndex()] = true;

      walkNodesCalculatingRematSafety(comp, currentNode->getChild(i),
            scanTargets, enabledSymRefs, unsafeSymRefs, trace, visitedNodes);
      }

   if (trace)
      {
      traceMsg(comp, "Remat Safety Walk visiting n%dn\n", currentNode->getGlobalIndex());
      traceMsg(comp, "Enabled symrefs: ");
      (*comp) << enabledSymRefs;
      traceMsg(comp, "\nUnsafe symrefs: ");
      (*comp) << unsafeSymRefs;
      traceMsg(comp, "\n");
      }

   // step 1 - add the current node kills to our running kills
   currentNode->mayKill().getAliasesAndUnionWith(unsafeSymRefs);

   TR::ILOpCode &opCode = currentNode->getOpCode();
   if (opCode.isLikeDef() && opCode.hasSymbolReference())
      {
      if (trace)
         traceMsg(comp, "Setting symref #%d as unsafe\n", currentNode->getSymbolReference()->getReferenceNumber());
      unsafeSymRefs[currentNode->getSymbolReference()->getReferenceNumber()] = true;
      }

   // update unsafeSymRefs - only set enabledSymRefs since the others are not "live"
   unsafeSymRefs &= enabledSymRefs;

   // step 2 - if we have encountered a node we are checking for, update
   // enabledSymRefs
   if (scanTargets.ValueAt(currentNode->getGlobalIndex()) && opCode.hasSymbolReference())
      {
      // enable the symref we just found
      if (trace)
         traceMsg(comp, "Enabling symref #%d\n", currentNode->getSymbolReference()->getReferenceNumber());
      enabledSymRefs[currentNode->getSymbolReference()->getReferenceNumber()] = true;
      }
   if (trace)
      {
      traceMsg(comp, "Remat Safety Walk after visiting n%dn\n", currentNode->getGlobalIndex());
      traceMsg(comp, "Enabled symrefs: ");
      (*comp) << enabledSymRefs;
      traceMsg(comp, "\nUnsafe symrefs: ");
      (*comp) << unsafeSymRefs;
      traceMsg(comp, "\n");
      }
   }

/*
 * This method walks the nodes between the start treetop inclusive and
 * the end treetop exclusive. If these nodes are in different blocks,
 * a path between them must be provided as a BitVector of blocks to visit.
 * This should contain the block containing the start treetop, the block
 * containing the end treetop and all in between. If the nodes are in the
 * same block, a NULL pointer can be provided.
 *
 * When it encounters a node in the scanTargets it adds the symref of the
 * scan target to its set of interest and continues scanning. Any symrefs
 * killed after they have been added to the set of interest are added to
 * the unsafeSymRefs vector for later use
 *
 * Returns true to indicate it successfully completed the analysis.
 */
bool RematTools::walkTreesCalculatingRematSafety(TR::Compilation *comp,
   TR::TreeTop *start, TR::TreeTop *end, TR::SparseBitVector &scanTargets, 
   TR::SparseBitVector &unsafeSymRefs, TR_BitVector *blocksToVisit, bool trace)
   {
   TR::Block *firstBlock = start->getEnclosingBlock();
   TR_BitVector *blocks = blocksToVisit;
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   if (blocks)
      {
      blocks = new (comp->trStackMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory(), stackAlloc);
      (*blocks) = (*blocksToVisit);
      }
#endif

   TR::SparseBitVector enabledSymRefs(comp->allocator());
   TR::SparseBitVector visitedNodes(comp->allocator());

   while (start != end)
      {
      if (start->getNode())
         {
         walkNodesCalculatingRematSafety(comp, start->getNode(),
            scanTargets, enabledSymRefs, unsafeSymRefs, trace, visitedNodes);
         }
      if (!getNextTreeTop(start, blocks, firstBlock))
         {
         traceMsg(comp, "  remat tools: failed to follow path for remat safety at [%p]\n", start->getNode());
         return false;
         }
      }

   return true;
   }

void RematTools::walkTreesCalculatingRematSafety(TR::Compilation *comp,
   TR::TreeTop *start, TR::TreeTop *end, TR::SparseBitVector &scanTargets,
   TR::SparseBitVector &unsafeSymRefs, bool trace)
   {
   // As no path is provided, the call cannot fail so its return value is ignored
   walkTreesCalculatingRematSafety(comp, start, end, scanTargets, unsafeSymRefs, NULL, trace);
   }

/*
 * This method walks the nodes between the start treetop inclusive and
 * the end treetop exclusive. If these nodes are in different blocks,
 * a path between them must be provided as a BitVector of blocks to visit.
 * This should contain the block containing the start treetop, the block
 * containing the end treetop and all in between. If the nodes are in the
 * same block, a NULL pointer can be provided.
 *
 * This method is used to find alternative temps which we might be able to load from
 * if we can't full rematerialize a privatized inliner arg. If we find a store of
 * one of the failed arguments, we record the store node and add the symref to the
 * dependencies for the arg. If the symref is not killed before the end of the block
 * we do a partial rematerialization by simply loading the temp.
 * Returns true to indicate it successfully completed the analysis.
 */
bool RematTools::walkTreeTopsCalculatingRematFailureAlternatives(TR::Compilation *comp,
   TR::TreeTop *start, TR::TreeTop *end, TR::list<TR::TreeTop*> &failedArgs,
   TR::SparseBitVector &scanTargets, RematSafetyInformation &rematInfo, TR_BitVector *blocksToVisit,
   bool trace)
   {
   TR::Block *firstBlock = start->getEnclosingBlock();
   TR_BitVector *blocks = blocksToVisit;
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   if (blocks)
      {
      blocks = new (comp->trStackMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory(), stackAlloc);
      (*blocks) = (*blocksToVisit);
      }
#endif

   TR::SparseBitVector failedTargets(comp->allocator());
   for (auto iter = failedArgs.begin(); iter != failedArgs.end(); ++iter)
      {
      failedTargets[(*iter)->getNode()->getFirstChild()->getGlobalIndex()] = true;
      }

   while (start != end)
      {
      if (trace)
         traceMsg(comp, "  priv arg remat: visiting [%p]: isStore %d privArg %d failedTargetMatch %d", start->getNode(),
            start->getNode()->getOpCode().isStoreDirect(),
            start->getNode()->getSymbol() && start->getNode()->getSymbol()->isAuto(),
            start->getNode()->getNumChildren() > 0 ? failedTargets.ValueAt(start->getNode()->getFirstChild()->getGlobalIndex()) : -1);
      if (start->getNode() && start->getNode()->getOpCode().isStoreDirect() &&
          start->getNode()->getSymbol()->isAuto() &&
          failedTargets.ValueAt(start->getNode()->getFirstChild()->getGlobalIndex()))
         {
         if (trace)
            traceMsg(comp, "  priv arg remat: considering partial remat with node [%p]", start->getNode());
         for (auto iter = failedArgs.begin(); iter != failedArgs.end(); ++iter)
            {
            if ((*iter) &&
                (*iter)->getNode()->getOpCode().hasSymbolReference() &&
                (*iter)->getNode()->getSymbolReference()->getReferenceNumber() != start->getNode()->getSymbolReference()->getReferenceNumber() &&
                (*iter)->getNode()->getFirstChild() == start->getNode()->getFirstChild())
                {
                if (trace)
                   traceMsg(comp, "  priv arg remat: attempting partial remat with node [%p] for [%p]", start->getNode(), (*iter)->getNode());
                rematInfo.add(*iter, start);
                scanTargets[start->getNode()->getGlobalIndex()] = true;
                failedTargets[start->getNode()->getFirstChild()->getGlobalIndex()] = false;
                *iter = NULL;
                }
            }
         }
      if (!getNextTreeTop(start, blocks, firstBlock))
         {
         traceMsg(comp, "  remat tools: failed to follow path for failure alternatives at [%p]\n", start->getNode());
         return false;
         }
      }

   return true;
   }

void RematTools::walkTreeTopsCalculatingRematFailureAlternatives(TR::Compilation *comp,
   TR::TreeTop *start, TR::TreeTop *end, TR::list<TR::TreeTop*> &failedArgs,
   TR::SparseBitVector &scanTargets, RematSafetyInformation &rematInfo, bool trace)
   {
   // As no path is provided, the call cannot fail so its return value is ignored
   walkTreeTopsCalculatingRematFailureAlternatives(comp, start, end, failedArgs, scanTargets, rematInfo, NULL, trace);
   }
