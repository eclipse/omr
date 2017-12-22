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

#include "optimizer/CopyPropagation.hpp"

#include <stddef.h>                            // for NULL, size_t
#include <stdint.h>                            // for int32_t
#include <string.h>                            // for memset
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "codegen/FrontEnd.hpp"                // for feGetEnv, TR_FrontEnd
#include "compile/Compilation.hpp"             // for Compilation
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"                      // for ABitVector<>::Cursor
#include "cs2/sparsrbit.h"                     // for ASparseBitVector, etc
#include "env/IO.hpp"                          // for POINTER_PRINTF_FORMAT
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                    // for Allocator, etc
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                        // for Block, toBlock
#include "il/DataTypes.hpp"                    // for DataTypes, etc
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::loadaddr, etc
#include "il/ILOps.hpp"                        // for ILOpCode, TR::ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/Node_inlines.hpp"                 // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                       // for Symbol
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/Cfg.hpp"                       // for CFG
#include "infra/List.hpp"                      // for ListIterator, List, etc
#include "infra/CfgEdge.hpp"                   // for CFGEdge
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/UseDefInfo.hpp"            // for TR_UseDefInfo, etc
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"                       // for TR_DebugBase


#define OPT_DETAILS "O^O COPY PROPAGATION: "


void collectNodesForIsCorrectChecks(TR::Node * n, TR::list< TR::Node *> & checkNodes, TR::SparseBitVector & refsToCheckIfKilled, vcount_t vc);

TR_CopyPropagation::TR_CopyPropagation(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
   _storeTreeTops((StoreTreeMapComparator()), StoreTreeMapAllocator(comp()->trMemory()->currentStackRegion())),
   _useTreeTops((UseTreeMapComparator()), UseTreeMapAllocator(comp()->trMemory()->currentStackRegion()))
   {}

template <class T>
TR::Node * getNodeParent(TR::Node * lookForNode, TR::Node * curNode, vcount_t visitCount, T & ci)
   {
   if (lookForNode == curNode)
      return 0; // problematic

   vcount_t oldVisitCount = curNode->getVisitCount();

   if (oldVisitCount == visitCount)
      return 0;

   curNode->setVisitCount(visitCount);

   for (size_t i = 0; i < curNode->getNumChildren(); i++)
      {
      if (curNode->getChild(i) == lookForNode)
         {
         ci = i;
         return curNode;
         }
      else
         {
         TR::Node * ret = getNodeParent(lookForNode, curNode->getChild(i), visitCount, ci);

         if (ret != 0)
            {
            return ret;
            }
         }
      }

   return 0;
   }

bool foundInterferenceBetweenCurrentNodeAndPropagation(TR::Compilation * comp, bool trace, TR::Node * currentNode, TR::Node * storeNode, TR::list< TR::Node *> & checkNodes, TR::SparseBitVector & refsToCheckIfKilled)
   {
   bool result = false;
   vcount_t save_vc = comp->getVisitCount();
   TR::Node * valueNode = storeNode->getOpCode().isStore() ? storeNode->getValueChild() : storeNode;

   if (currentNode->mayKill().containsAny(refsToCheckIfKilled, comp))
      {
      result = true;
      }

   if (trace)
      {
      comp->getDebug()->trace("foundInterferenceBetweenCurrentNodeAndPropagation: currentNode %p storeNode %p checkNodes [", currentNode, storeNode);
      for (auto checkNodesCursor = checkNodes.begin(); checkNodesCursor != checkNodes.end(); ++checkNodesCursor)
         {
         comp->getDebug()->trace("%p ", (*checkNodesCursor));
         }

      comp->getDebug()->trace("] = %s\n", result ? "interference" : "ok");
      }

   comp->setVisitCount(save_vc);
   currentNode->resetVisitCounts(save_vc);
   valueNode->resetVisitCounts(save_vc);
   return result;
   }

// Replace _targetNode with _sourceTree everywhere in the block, if possible.
class TR_ExpressionPropagation
   {
   private:
      TR::Compilation * _compilation;
      bool _trace;

      // Supplied by user
      TR::TreeTop * _targetTreeTop;
      TR::Node * _targetNode;
      TR::Node * _sourceTree;
      TR::Node * _originalDefNodeForSourceTree;

      // Used by class
      int32_t _targetChildIndex;
      TR::Node * _parentOfTargetNode;

   public:
      TR_ExpressionPropagation(TR::Compilation * c, bool t) :
         _compilation(c),
         _trace(t),
         _targetTreeTop(0),
         _targetNode(0),
         _sourceTree(0),
         _originalDefNodeForSourceTree(0),
         _targetChildIndex(-1),
         _parentOfTargetNode(0)
         {
         }

      TR::Compilation * comp()
         {
         return _compilation;
         }

      bool trace()
         {
         return _trace;
         }

      void setTargetTreeTop(TR::TreeTop * tt)
         {
         _targetTreeTop = tt;
         }

      void setTargetNode(TR::Node * tn)
         {
         _targetNode = tn;

         TR_ASSERT(_targetTreeTop->getNode()->containsNode(_targetNode, comp()->incOrResetVisitCount()),
                 "_targetTreeTop->getNode() does not contain _targetNode!");
         }

      void setSourceTree(TR::Node * st)
         {
         _sourceTree = st;
         }

      void setOriginalDefNodeForSourceTree(TR::Node * dn)
         {
         _originalDefNodeForSourceTree = dn;
         }

      void perform()
         {
         // Assert all user supplied values are set.
         TR_ASSERT(_targetTreeTop != 0, "_targetTreeTop is 0!");
         TR_ASSERT(_targetNode != 0, "_targetNode is 0!");
         TR_ASSERT(_sourceTree != 0, "_sourceTree is 0!");
         TR_ASSERT(_originalDefNodeForSourceTree != 0, "_originalDefNodeForSourceTree is 0!");

         // Assert that this class hasn't been used before.
         TR_ASSERT(_targetChildIndex == -1, "_targetChildIndex is -1!");
         TR_ASSERT(_parentOfTargetNode == 0, "_parentOfTargetNode is 0!");

         // If all ok, then do:
         replaceTargetNodeBySourceTree();
#ifdef J9_PROJECT_SPECIFIC
         fixupBCDPrecisionIfRequired();
#endif
         addConversionIfRequired();
         propagateThroughToCommoningPoints();
         }

   private:
      void replaceTargetNodeBySourceTree()
         {
         _parentOfTargetNode = getNodeParent(_targetNode, _targetTreeTop->getNode(), comp()->incOrResetVisitCount(), _targetChildIndex);

         TR_ASSERT(_parentOfTargetNode != 0, "could not find _targetNode " POINTER_PRINTF_FORMAT " parent, _targetTreeTop root node " POINTER_PRINTF_FORMAT "!", _targetNode, _targetTreeTop->getNode());
         TR_ASSERT(_parentOfTargetNode->getChild(_targetChildIndex) == _targetNode, "_targetChildIndex or _parentOfTargetNode is not correct?");

         _parentOfTargetNode->setAndIncChild(_targetChildIndex, _sourceTree);

         // We "unhooked" the node from its parent, so dec ref count.
         _targetNode->decReferenceCount();
         }

      // The difference between this function and replaceTargetNodeBySourceTree is that this function does not set any class fields.
      void replaceAllInstancesOfTargetNodeInTreeBySourceTree(TR::TreeTop * targetTreeTop, TR::Node * targetNode, TR::Node * sourceTree)
         {
         while (targetTreeTop->getNode()->containsNode(targetNode, comp()->incOrResetVisitCount()))
            {
            size_t childIndex;
            TR::Node * parentNode = getNodeParent(targetNode, targetTreeTop->getNode(), comp()->incOrResetVisitCount(), childIndex);

            TR_ASSERT(parentNode != 0, "parentNode == 0!");

            if (trace())
               {
               comp()->getDebug()->trace("%s   Propagating new RHS " POINTER_PRINTF_FORMAT " in place of old instance location " POINTER_PRINTF_FORMAT " child index %d\n",
                                         OPT_DETAILS, sourceTree, parentNode, childIndex);
               }

            TR::Node * commonedNode = parentNode->getChild(childIndex);
            parentNode->setAndIncChild(childIndex, sourceTree);
            targetNode->decReferenceCount();
            }
         }

#ifdef J9_PROJECT_SPECIFIC
      void fixupBCDPrecisionIfRequired()
         {
         TR::Node * oldNode = _targetNode; // existed before
         TR::Node * newNode = _parentOfTargetNode->getChild(_targetChildIndex); // propagated

         if (oldNode->getType().isBCD() && newNode->getType().isBCD())
            {
            int oldPrecision = 0;
            int newPrecision = 0;

            if (newNode->getNumChildren() == 2)
               {
               oldPrecision = oldNode->getDecimalPrecision();
               newPrecision = newNode->getDecimalPrecision();
               }
            else
               {
               oldPrecision = oldNode->getDecimalPrecision();
               int32_t oldSize = oldNode->getSize();

               TR::DataType newDataType = newNode->getOpCode().hasSymbolReference() ? newNode->getSymbolReference()->getSymbol()->getDataType() : newNode->getDataType();
               int32_t      newSymSize  = newNode->getOpCode().hasSymbolReference() ? newNode->getSymbolReference()->getSymbol()->getSize() : newNode->getSize();
               newPrecision = TR::DataType::getBCDPrecisionFromSize(newDataType, newSymSize);

               // for a oldNode with prec=30 and an newNode with prec=31 then if the symbol sizes match and the other checks
               // below pass then can reduce the newPrecision to 30 so a pdModPrec correction is not needed (as the propagated symbol will not have
               // the top nibble 30->31 set).
               if (newNode->getType().isAnyPacked())
                  {
                  if (newPrecision > oldPrecision && // 31 > 30
                      oldPrecision == newNode->getDecimalPrecision() && // 30 == 30
                      oldSize == newNode->getSize() && // 16 == 16
                      oldSize == newSymSize)  // 16 == 16
                     {
                     if (trace() || comp()->cg()->traceBCDCodeGen())
                        {
                        traceMsg(comp(), "reduce newPrecision %d->%d for odd to even truncation (origNode %s (%p) prec=%d, node %s (%p) prec=%d\n",
                                newPrecision, oldPrecision,
                                newNode->getOpCode().getName(), newNode, newNode->getDecimalPrecision(),
                                oldNode->getOpCode().getName(), oldNode, oldPrecision);
                        }

                     newPrecision = oldPrecision; // 31->30 -- no correction needed for same byte odd to even truncation if the origNode was only storing out the even $ of digits
                     }
                  }
               }

            TR_ASSERT(newPrecision >= oldPrecision, "newPrecision < oldPrecision (%d < %d) during copyPropagation\n", newPrecision, oldPrecision);

            bool needsClean = _originalDefNodeForSourceTree && _originalDefNodeForSourceTree->mustClean();
            bool needsPrecisionCorrection = oldPrecision != newPrecision;

            if (needsClean || needsPrecisionCorrection)
               {
               dumpOptDetails(comp(),"node %p precision %d != propagated symRef #%d precision %d and/or needsClean (%s)\n",
                                oldNode, oldPrecision,
                                newNode->getSymbolReference()->getReferenceNumber(), newPrecision, needsClean?"yes":"no");

               if (needsPrecisionCorrection)
                  {
                  TR::ILOpCodes modPrecOp = TR::ILOpCode::modifyPrecisionOpCode(_parentOfTargetNode->getChild(_targetChildIndex)->getDataType());

                  TR_ASSERT(modPrecOp != TR::BadILOp, "no bcd modify precision opcode found for node %p type %s\n",
                          _parentOfTargetNode->getChild(_targetChildIndex),
                          _parentOfTargetNode->getChild(_targetChildIndex)->getDataType().toString());

                  TR::Node * pdModPrecNode = TR::Node::create(modPrecOp, 1, _parentOfTargetNode->getChild(_targetChildIndex));
                  pdModPrecNode->setDecimalPrecision(oldPrecision); // set to existing node's precision

                  _parentOfTargetNode->setAndIncChild(_targetChildIndex, pdModPrecNode);
                  }

               if (needsClean)
                  {
                  TR::ILOpCodes cleanOp = TR::ILOpCode::cleanOpCode(_parentOfTargetNode->getChild(_targetChildIndex)->getDataType());

                  TR_ASSERT(cleanOp != TR::BadILOp, "no bcd clean opcode found for node %p type %s\n",
                          _parentOfTargetNode->getChild(_targetChildIndex),
                          _parentOfTargetNode->getChild(_targetChildIndex)->getDataType().toString());

                  TR::Node * pdCleanNode = TR::Node::create(cleanOp, 1, _parentOfTargetNode->getChild(_targetChildIndex));

                  _parentOfTargetNode->setAndIncChild(_targetChildIndex, pdCleanNode);
                  }
               }
            }
         }
#endif

      void addConversionIfRequired()
         {
         TR::Node * oldNode = _targetNode; // existed before
         TR::Node * newNode = _parentOfTargetNode->getChild(_targetChildIndex); // propagated

         if (newNode->getType().getDataType() != oldNode->getType().getDataType())
            {
            //                                                     /* source */            /* target */
            TR::ILOpCodes convOp = TR::ILOpCode::getProperConversion(newNode->getDataType(), oldNode->getDataType(), true);

            TR_ASSERT(convOp != TR::BadILOp, "no conversion opcode found for old type %s to new type %s!\n",
                    oldNode->getDataType().toString(), newNode->getDataType().toString());

            TR::Node * convNode = TR::Node::create(convOp, 1, _parentOfTargetNode->getChild(_targetChildIndex));

            _parentOfTargetNode->setAndIncChild(_targetChildIndex, convNode);
            }
         }

      void propagateThroughToCommoningPoints()
         {
         TR::Node * oldNode = _targetNode; // existed before
         TR::Node * newNode = _parentOfTargetNode->getChild(_targetChildIndex); // propagated

         // 1) Propagate this RHS change to anything with a commoned old node further down the block.
         //    Common the rematerialization instead - it should have the same value at that evaluation.
         // 2) Make sure evaluation point did not change when old node was replaced.

         // If there are more commoning sites for the old node, then take care of them.
         if (oldNode->getReferenceCount() > 0)
            {
            // Step 1: Propagate this RHS change to anything with a commoned old node further down the block. Common the rematerialization instead.

            // The idea is that we have replaced the RHS of some node, but other nodes further down the block could have commoned the old node.
            // We want to replace those commonings with a commoning to the new RHS of the node because the value is the same.

            // Get a list of load sym refs for the new rematerialized RHS - use this to see if any part of the new node is killed during tree traversal.
            TR::list< TR::Node *> newCheckNodes(getTypedAllocator<TR::Node*>(comp()->allocator()));
            TR::SparseBitVector newRefsToCheckIfKilled(comp()->allocator());

            collectNodesForIsCorrectChecks(newNode, newCheckNodes, newRefsToCheckIfKilled, comp()->incOrResetVisitCount());

            // Start at _targetTreeTop to replace other instances of _targetNode in _targetTreeTop as well as to the end of the block.
            TR::TreeTop * cur_tt = _targetTreeTop;

            while (cur_tt && (cur_tt->getNode()->getOpCodeValue() != TR::BBEnd))
               {
               // Replace multiple commonings with a while loop.
               replaceAllInstancesOfTargetNodeInTreeBySourceTree(cur_tt, oldNode, newNode);

               if (oldNode->getReferenceCount() == 0)
                  {
                  // Replaced all occurrences later in the block, bail out.
                  if (trace())
                     {
                     comp()->getDebug()->trace("%s   Propagating new RHS " POINTER_PRINTF_FORMAT " stops because oldNode ref count = 0\n",
                                               OPT_DETAILS, newNode);
                     }

                  break;
                  }

               cur_tt = cur_tt->getNextTreeTop();

               // Traverse extended basic blocks
               while ((cur_tt->getNode()->getOpCodeValue() == TR::BBEnd) &&
                      (cur_tt->getNextTreeTop() &&
                       (cur_tt->getNextTreeTop()->getNode()->getOpCodeValue() == TR::BBStart) &&
                       (cur_tt->getNextTreeTop()->getNode()->getBlock()->isExtensionOfPreviousBlock())))
                  {
                  //       BBEnd            BBStart         next node
                  cur_tt = cur_tt->getNextTreeTop()->getNextTreeTop();
                  }
               }

            // BBEnd of block could also have GlRegDeps

            if (cur_tt && (cur_tt->getNode()->getOpCodeValue() == TR::BBEnd))
               {
               replaceAllInstancesOfTargetNodeInTreeBySourceTree(cur_tt, oldNode, newNode);
               }
            }
         }
   };

int32_t TR_CopyPropagation::perform()
   {
   if (trace())
      traceMsg(comp(), "Starting CopyPropagation\n");

   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   _canMaintainUseDefs = true;
   if (useDefInfo == NULL)
      {
         {
         if (trace())
            traceMsg(comp(), "Can't do CopyPropagation, no use/def information\n");
         return 0;
         }
      }

   // Pre-compute and cache usedef information for compile time performance
   useDefInfo->buildDefUseInfo();

   // Required beyond the scope of the stack memory region
   bool donePropagation = false;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _cleanupTemps = false;

   TR::TreeTop *currentTree = comp()->getStartTree();
   while (currentTree)
      {
      TR::Node *currentNode = skipTreeTopAndGetNode(currentTree);
      if (currentNode->getOpCode().isLikeDef())
         _storeTreeTops[currentNode] = currentTree;
      currentTree = currentTree->getNextTreeTop();
      }

   int32_t numDefsOnEntry    = useDefInfo->getNumDefsOnEntry();
   int32_t firstRealDefIndex = useDefInfo->getFirstDefIndex() + numDefsOnEntry;
   int32_t lastDefIndex      = useDefInfo->getLastDefIndex();
   int32_t numDefNodes       = lastDefIndex - firstRealDefIndex + 1;

   typedef TR::typed_allocator<std::pair<int32_t const, int32_t>, TR::Region&> UDMapAllocator;
   typedef std::less<int32_t> UDMapComparator;
   typedef std::map<int32_t, int32_t, UDMapComparator, UDMapAllocator> UsesToBeFixedMap;
   typedef std::map<int32_t, int32_t, UDMapComparator, UDMapAllocator> EquivalentDefMap;

   UsesToBeFixedMap usesToBeFixed((UDMapComparator()), UDMapAllocator(trMemory()->currentStackRegion()));
   EquivalentDefMap equivalentDefs((UDMapComparator()), UDMapAllocator(trMemory()->currentStackRegion()));

   typedef TR::typed_allocator<std::pair<TR::Node* const, TR::deque<TR::Node*, TR::Region&>*>, TR::Region&> StoreNodeMapAllocator;
   typedef std::less<TR::Node*> StoreNodeMapComparator;
   typedef std::map<TR::Node*, TR::deque<TR::Node*, TR::Region&>*, StoreNodeMapComparator, StoreNodeMapAllocator> StoreNodeMap;

   StoreNodeMap storeMap((StoreNodeMapComparator()), StoreNodeMapAllocator(trMemory()->currentStackRegion()));

   TR::NodeChecklist checklist(comp());

   _lookForOriginalDefs = false;
   for (TR::TreeTop *tree = comp()->getStartTree(); tree; tree = tree->getNextTreeTop())
      {
      TR::Node *defNode = tree->getNode();
      if (defNode->getOpCodeValue() == TR::BBStart && !defNode->getBlock()->isExtensionOfPreviousBlock())
         {
         storeMap.clear();
         continue;
         }

      collectUseTrees(tree, tree->getNode(), checklist);

      if (!(defNode->getOpCode().isStoreDirect()
            && defNode->getSymbolReference()->getSymbol()->isAutoOrParm()
            && !defNode->storedValueIsIrrelevant()))
         continue;

      TR::Node *rhsOfStoreDefNode = defNode->getChild(0);
      int32_t i = defNode->getUseDefIndex();
      if (i == 0)
         continue;

      auto storeLookup = storeMap.find(rhsOfStoreDefNode);
      if (storeLookup != storeMap.end())
         {
         for (auto itr = storeLookup->second->begin(), end = storeLookup->second->end(); itr != end; ++itr)
            {
            TR::Node *prevDefNode = *itr;
            int32_t j = prevDefNode->getUseDefIndex();
            int32_t thisDefSlot = defNode->getSymbolReference()->getCPIndex();
            int32_t prevDefSlot = prevDefNode->getSymbolReference()->getCPIndex();

            bool thisSymRefIsPreferred = false;
            if (thisDefSlot < 0) // pending pushes
               thisSymRefIsPreferred = true;
            else if (thisDefSlot < comp()->getOwningMethodSymbol(defNode->getSymbolReference()->getOwningMethodIndex())->getFirstJitTempIndex()) // autos and parms
               thisSymRefIsPreferred = true;

            bool prevSymRefIsPreferred = false;
            if (prevDefSlot < 0) // pending pushes
               prevSymRefIsPreferred = true;
            else if (prevDefSlot < comp()->getOwningMethodSymbol(prevDefNode->getSymbolReference()->getOwningMethodIndex())->getFirstJitTempIndex()) // autos and parms
               prevSymRefIsPreferred = true;

#ifdef J9_PROJECT_SPECIFIC
            if (prevDefNode->getType().isBCD())
               {
               TR_ASSERT(defNode->getType().isBCD(),"expecting defNode to be a BCD type (dt=%s) when prevDefNode is a BCD type (dt=%s)\n",
                  defNode->getDataType().toString(),prevDefNode->getDataType().toString());
               if (defNode->getType().isBCD() &&
                   defNode->getDataType() == prevDefNode->getDataType() &&
                   prevDefNode->getDecimalPrecision() == defNode->getDecimalPrecision() &&
                   (defNode->getDataType() != TR::PackedDecimal || prevDefNode->mustCleanSignInPDStoreEvaluator() == defNode->mustCleanSignInPDStoreEvaluator()))
                  {
                  dumpOptDetails(comp(), "%s   Def nodes %p (precision = %d) and %p (precision = %d) are equivalent\n",OPT_DETAILS,
                     defNode,
                     defNode->getDecimalPrecision(),
                     prevDefNode,
                     prevDefNode->getDecimalPrecision());

                  auto equivalentDefLookup = equivalentDefs.find(j);
                  if ((!thisSymRefIsPreferred &&
                       (prevSymRefIsPreferred || (equivalentDefLookup != equivalentDefs.end()))))
                     {
                     if (trace())
                        traceMsg(comp(), "000setting i %d to j %d\n", i, j);
                     if (equivalentDefLookup == equivalentDefs.end())
                        {
                        if (i != j)
                           equivalentDefs[i] = j;
                        }
                     else if (i == equivalentDefLookup->second)
                        {
                        equivalentDefs.erase(i);
                        }
                     else
                        {
                        equivalentDefs[i] = equivalentDefLookup->second;
                        }
                     }
                  else if (equivalentDefs.find(j) == equivalentDefs.end())
                     {
                     if (trace())
                         traceMsg(comp(), "111setting j %d to i %d\n", j, i);
                     if (i != j)
                        equivalentDefs[j] = i;
                     }
                  }
               }
            else
#endif
               {
               dumpOptDetails(comp(), "%s   Def nodes %p and %p are equivalent\n",OPT_DETAILS,
                  defNode,
                  prevDefNode);

               auto equivalentDefLookup = equivalentDefs.find(j);
               if ((!thisSymRefIsPreferred &&
                   (prevSymRefIsPreferred || (equivalentDefLookup != equivalentDefs.end()))))
                  {
                  if (trace())
                     traceMsg(comp(), "000setting i %d to j %d\n", i, j);

                  if (equivalentDefLookup == equivalentDefs.end())
                     {
                     if (i != j)
                        equivalentDefs[i] = j;
                     }
                  else if (i == equivalentDefLookup->second)
                     {
                     equivalentDefs.erase(i);
                     }
                  else
                     {
                     equivalentDefs[i] = equivalentDefLookup->second;
                     }
                  }
               else if (equivalentDefs.find(j) == equivalentDefs.end())
                  {
                  if (trace())
                     traceMsg(comp(), "111setting j %d to i %d\n", j, i);
                  if (i != j)
                     equivalentDefs[j] = i;
                  }
               }
            break;
            }
         }

      if (storeLookup == storeMap.end())
         {
         storeMap[rhsOfStoreDefNode] = new (trStackMemory()) TR::deque<TR::Node*, TR::Region&>(trMemory()->currentStackRegion());
         storeMap[rhsOfStoreDefNode]->push_back(defNode);
         }
      else
         {
         storeMap[rhsOfStoreDefNode]->push_back(defNode);
         }
      }

   int32_t firstUseIndex = useDefInfo->getFirstUseIndex();
   for (auto itr = equivalentDefs.begin(), end = equivalentDefs.end(); itr != end; ++itr)
      {
      int32_t defIndex = itr->first;
      TR::Node *defNode = useDefInfo->getNode(defIndex);
      TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);
      TR::TreeTop *extendedBlockEntry = defTree->getEnclosingBlock()->startOfExtendedBlock()->getEntry();
      TR::TreeTop *extendedBlockExit = extendedBlockEntry->getExtendedBlockExitTreeTop();
      if (defNode == NULL)
         continue;

      TR::Node *equivalentDefNode = useDefInfo->getNode(itr->second);
      TR::TreeTop *equivalentDefTree = useDefInfo->getTreeTop(itr->second);

      TR_UseDefInfo::BitVector uses(comp()->allocator());
      useDefInfo->getUsesFromDef(uses, itr->first, useDefInfo->hasLoadsAsDefs());
      TR_UseDefInfo::BitVector::Cursor useCursor(uses);
      for (useCursor.SetToFirstOne(); useCursor.Valid(); useCursor.SetToNextOne())
         {
         int32_t i = useCursor + firstUseIndex;

         TR_UseDefInfo::BitVector defs(comp()->allocator());
         useDefInfo->getUseDef(defs, i);

         // could use "if (defs.PopulationCount() == 1) continue", but this is more efficient
         TR_UseDefInfo::BitVector::Cursor cursor(defs);
         cursor.SetToFirstOne();
         if (!cursor.Valid())
            continue;
         int32_t defIndex = cursor;
         cursor.SetToNextOne();
         if (cursor.Valid())
            continue;

         TR::Node *useNode = useDefInfo->getNode(i);
         if (!useNode || useNode->getReferenceCount() == 0)
            continue;

         // Now we know there is a single definition for this use node
         //
         TR::SymbolReference *copySymbolReference = defNode->getSymbolReference();
         if (performTransformation(comp(), "%s   Copy1 Propagation replacing Copy symRef #%d %p %p\n",OPT_DETAILS, copySymbolReference->getReferenceNumber(), defNode,  equivalentDefNode))
            {
	      comp()->setOsrStateIsReliable(false); // could check if the propagation was done to a child of the OSR helper call here

            _storeTree = NULL;
            _useTree = NULL;
            _storeBlock = NULL;

            if (!isCorrectToReplace(useNode, equivalentDefNode, defs, useDefInfo))
               continue;

            TR::TreeTop *cursorTree = extendedBlockEntry;

            bool defSeenFirst = false;
            bool equivalentDefSeenFirst = false;
            while (cursorTree != extendedBlockExit)
               {
               if (cursorTree == defTree)
                  {
                  defSeenFirst = true;
                  break;
                  }

               if (cursorTree == equivalentDefTree)
                  {
                  equivalentDefSeenFirst = true;
                  break;
                  }

              cursorTree = cursorTree->getNextTreeTop();
              }


            bool safe = true;
            if (defSeenFirst)
               {
               cursorTree = defTree;
               while (cursorTree != equivalentDefTree)
                  {
                  if (!cursorTree->getNode()->getOpCode().isStore() ||
                        (cursorTree->getNode()->getFirstChild() != defNode->getFirstChild()))
                     {
                     safe = false;
                     break;
                     }

                  cursorTree = cursorTree->getNextTreeTop();
                  }
               }

            if (!safe)
               continue;


            TR::SymbolReference *originalSymbolReference = equivalentDefNode->getSymbolReference();

            dumpOptDetails(comp(), "%s   By Original symRef #%d in Use node : %p\n",OPT_DETAILS, originalSymbolReference->getReferenceNumber(), useNode);
            dumpOptDetails(comp(), "%s   Use #%d[%p] is defined by:\n",OPT_DETAILS,i,useNode);
            dumpOptDetails(comp(), "%s      Def #%d[%p]\n",OPT_DETAILS, defIndex,useDefInfo->getNode(defIndex));

            comp()->incOrResetVisitCount();
            replaceCopySymbolReferenceByOriginalIn(copySymbolReference, equivalentDefNode, useNode, defNode);
            usesToBeFixed[useNode->getUseDefIndex()] = equivalentDefs[defIndex];

            donePropagation = true;

            if (trace())
               {
               traceMsg(comp(), "   Use #%d[%p] :\n",useNode->getUseDefIndex(),useNode);
               traceMsg(comp(), "      Does NOT get Defined by #%d[%p] from now\n",defNode->getUseDefIndex(), defNode);
               }

            if (trace())
               {
               traceMsg(comp(), "   Use #%d[%p] is defined by:\n",useNode->getUseDefIndex(), useNode);
               traceMsg(comp(), "      Added New Def #%d[%p]\n",defIndex,useDefInfo->getNode(equivalentDefs[defIndex]));
               }

            if (trace())
               traceMsg(comp(), "\n");
            }
         }
      }


   bool invalidateDefUseInfo = false;
   int32_t lastUseIndex = useDefInfo->getLastUseIndex();
   for (auto itr = usesToBeFixed.begin(), end = usesToBeFixed.end(); itr != end; )
      {
      int32_t fixUseIndex = itr->first;
      TR::Node *fixUseNode = useDefInfo->getNode(fixUseIndex);
      invalidateDefUseInfo = true;
      if (useDefInfo->hasLoadsAsDefs())
         {
         for (int32_t i = useDefInfo->getFirstUseIndex(); i <= lastUseIndex; i++)
            {
            TR::Node *useNode = useDefInfo->getNode(i);
            if (!useNode || useNode->getReferenceCount() == 0)
               continue;

            if (useDefInfo->getSingleDefiningLoad(useNode) == fixUseNode)
               {
               TR_UseDefInfo::BitVector defsOfUseToBeFixed(comp()->allocator());
               if (useDefInfo->getUseDef(defsOfUseToBeFixed, fixUseIndex))
                  {
                  TR_UseDefInfo::BitVector::Cursor cursor(defsOfUseToBeFixed);
                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t nextDef = cursor;
                     useDefInfo->setUseDef(i, nextDef);
                     }
                  }
               useDefInfo->resetUseDef(i, fixUseIndex);
               }
            }
         }

      useDefInfo->clearUseDef(fixUseIndex);
      useDefInfo->setUseDef(fixUseIndex, itr->second);
      usesToBeFixed.erase(itr++);
      }

   if (_cleanupTemps)
      rematerializeIndirectLoadsFromAutos();

   _lookForOriginalDefs = true;
   for (int32_t i = useDefInfo->getFirstUseIndex(); i <= lastUseIndex; i++)
      {
      TR_UseDefInfo::BitVector defs(comp()->allocator());

      if (useDefInfo->getUseDef(defs, i))
         {
         TR::Node *useNode = useDefInfo->getNode(i);
         _useTree = NULL;

         if (!useNode) continue;
         if (useNode->getReferenceCount() == 0) continue;
         if (useNode->getOpCodeValue() == TR::loadaddr) continue;

         TR::Node *loadNode, *baseNode;
         int32_t regNumber;
         TR::Node *defNode = areAllDefsInCorrectForm(useNode, defs, useDefInfo, firstRealDefIndex, loadNode, regNumber, baseNode);
         if (!defNode) continue;

         findUseTree(useNode);
         TR_ASSERT(_useTree, "Did not find the tree for the use node\n");

         TR::TreeTop *defTree = useDefInfo->getTreeTop(defNode->getUseDefIndex());
         if (defTree->getEnclosingBlock()->startOfExtendedBlock() == _useTree->getEnclosingBlock()->startOfExtendedBlock())
            {
            // PR62462: LocalCSE is a fine and trustworthy opt, and we're not trying to do its job here.
            // Our analysis (particularly of RegLoads) assumes it is propagating copies into successor blocks,
            // and doesn't handle propagation within an extended block properly.
            //
            continue;
            }

         if (baseNode)
            {
            TR_ASSERT(baseNode->getOpCode().hasSymbolReference(), "baseNode should have a symbol reference\n");
            if (!_cleanupTemps ||
                baseNode->getSymbolReference() == defNode->getSymbolReference() ||
                !isNodeAvailableInBlock(_useTree, baseNode))
               continue;
            }

         bool isRegLoad = (regNumber != -1);

         TR::Node *rhsOfStoreDefNode = defNode->getOpCode().isStoreIndirect() ? defNode->getSecondChild() : defNode->getFirstChild();
         TR::SymbolReference *copySymbolReference = defNode->getOpCode().hasSymbolReference()?defNode->getSymbolReference():NULL;

         bool baseAddrAvail = false;
         TR::Node * baseAddr = NULL;

         // Bail out if we're in a NOTEMPS method and we're going to create a temp.  This conditional should match
         // the conditional that covers the anchoring code below.
         if (!isRegLoad &&
                         (loadNode->getOpCodeValue() != TR::loadaddr) &&                   // loadaddr doesn't need to be anchored
                         (loadNode->getOpCode().hasSymbolReference()) &&
                         (loadNode->getSymbolReference() == copySymbolReference) &&
                         (rhsOfStoreDefNode != loadNode) &&
                         comp()->getJittedMethodSymbol() && // avoid NULL pointer on non-Wcode builds
                         comp()->getJittedMethodSymbol()->isNoTemps())
            continue;

         if (performTransformation(comp(), "%s   Copy2 Propagation replacing Copy symRef #%d \n",OPT_DETAILS, copySymbolReference->getReferenceNumber()))
            {
            comp()->setOsrStateIsReliable(false); // could check if the propagation was done to a child of the OSR helper call here
            dumpOptDetails(comp(), "%s   Use #%d[%p] is defined by:\n",OPT_DETAILS,i,useNode);
            TR_UseDefInfo::BitVector::Cursor cursor(defs);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t defIndexTrace = cursor;
               dumpOptDetails(comp(), "%s      Def #%d[%p]\n",OPT_DETAILS, defIndexTrace,useDefInfo->getNode(defIndexTrace));
               }

            if (!isRegLoad &&
                (loadNode->getOpCodeValue() != TR::loadaddr) &&                   // loadaddr doesn't need to be anchored
                (loadNode->getOpCode().hasSymbolReference()) &&
                (loadNode->getSymbolReference() == copySymbolReference) &&
                (rhsOfStoreDefNode != loadNode))                                 // Modifications to this condition should be mirrored in the NOTEMPS check above.
               {
               TR::TreeTop *anchorTree = findAnchorTree(defNode, loadNode);

               // Need to create a temp for case like i = i + 1
               TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), copySymbolReference->getSymbol()->getDataType(), false, copySymbolReference->getSymbol()->getSize());

               dumpOptDetails(comp(), "%s      created new temp #%d\n", OPT_DETAILS, newSymbolReference->getReferenceNumber());

               TR::Node *store = TR::Node::createStore(newSymbolReference, TR::Node::createLoad(defNode, copySymbolReference));

               anchorTree->insertBefore(TR::TreeTop::create(comp(), store));

               loadNode->setSymbolReference(newSymbolReference);
               }

            donePropagation = true;

            if (_propagatingWholeExpression)
               {
               dumpOptDetails(comp(), "%s   Rematerializing whole expression %p to replace %p\n", OPT_DETAILS, rhsOfStoreDefNode, useNode);
               }
            else if (!isRegLoad)
               {
               dumpOptDetails(comp(), "%s   By Original expression in %p Use node : %p\n", OPT_DETAILS, rhsOfStoreDefNode, useNode);
               }
            else
               {
               dumpOptDetails(comp(), "%s   By register number %d in Use node : %p\n", OPT_DETAILS, regNumber, useNode);
               }

            comp()->incOrResetVisitCount();

            if (_propagatingWholeExpression)
               {
               invalidateDefUseInfo = true;

               TR::Node * rematExpression = rhsOfStoreDefNode->getOpCode().isStore() ? rhsOfStoreDefNode->getValueChild() : rhsOfStoreDefNode;
               TR::Node * newExpression = rematExpression->duplicateTree();
               if (!optimizer()->areSyntacticallyEquivalent(rematExpression, newExpression, comp()->incOrResetVisitCount()))
                   {
                   TR_ASSERT(0, "bad equivalence");
                   }

               TR_ExpressionPropagation expressionPropagator(comp(), trace());

               expressionPropagator.setTargetTreeTop(_useTree);
               expressionPropagator.setTargetNode(useNode);
               expressionPropagator.setSourceTree(newExpression);
               expressionPropagator.setOriginalDefNodeForSourceTree(defNode);

               expressionPropagator.perform();
               }
            else if (!isRegLoad)
               {
               replaceCopySymbolReferenceByOriginalIn(copySymbolReference, rhsOfStoreDefNode, useNode, defNode, baseAddr, baseAddrAvail);
               usesToBeFixed[useNode->getUseDefIndex()] = loadNode->getUseDefIndex();
               }
            else
               {
               TR::Node *regLoadNode = NULL;
               TR::Block *useBlock = _useTree->getEnclosingBlock();
               TR::Node *bbstartNode = useBlock->startOfExtendedBlock()->getEntry()->getNode();
               if (bbstartNode->getNumChildren() > 0)
                  {
                  for (int i = 0; i < bbstartNode->getFirstChild()->getNumChildren(); i++)
                     {
                     TR::Node *child = bbstartNode->getFirstChild()->getChild(i);
                     if (child->getGlobalRegisterNumber() == regNumber)
                        {
                        regLoadNode = child;
                        break;
                        }
                     }

                  if (regLoadNode)
                     {
                     TR::TreeTop *cursorTree = useBlock->getEntry();
                     TR::TreeTop *exitTree = cursorTree->getExtendedBlockExitTreeTop()->getNextTreeTop();
                     while (cursorTree != exitTree)
                        {
                        replaceCopySymbolReferenceByOriginalRegLoadIn(regLoadNode, useNode, copySymbolReference, cursorTree->getNode(), NULL, -1);
                        cursorTree = cursorTree->getNextTreeTop();
                        }
                     }
                  }
               }

               if (trace())
                  traceMsg(comp(), "\n");
            }
         }
      }

   if (_cleanupTemps)
      commonIndirectLoadsFromAutos();

   for (auto itr = usesToBeFixed.begin(), end = usesToBeFixed.end(); itr != end; ++itr)
      {
      int32_t fixUseIndex = itr->first;
      TR::Node *fixUseNode = useDefInfo->getNode(fixUseIndex);
      invalidateDefUseInfo = true;

      if (useDefInfo->hasLoadsAsDefs())
         {
         for (int32_t i = useDefInfo->getFirstUseIndex(); i <= lastUseIndex; i++)
            {
            TR::Node *useNode = useDefInfo->getNode(i);
            if (!useNode || useNode->getReferenceCount() == 0)
               continue;

            if (useDefInfo->getSingleDefiningLoad(useNode) == fixUseNode)
               {
               TR_UseDefInfo::BitVector defsOfUseToBeFixed(comp()->allocator());
               if (useDefInfo->getUseDef(defsOfUseToBeFixed, fixUseIndex))
                  {
                  TR_UseDefInfo::BitVector::Cursor cursor(defsOfUseToBeFixed);
                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t nextDef = cursor;
                     useDefInfo->setUseDef(i, nextDef);
                     }
                  useDefInfo->resetUseDef(i, fixUseIndex);
                  }
               }
            }
         }

      useDefInfo->clearUseDef(fixUseIndex);

      if (useDefInfo->hasLoadsAsDefs())
         {
         useDefInfo->setUseDef(fixUseIndex, itr->second);
         }
      else if (itr->second != 0)
         {
         TR_UseDefInfo::BitVector defsOfRhs(comp()->allocator());
         if (useDefInfo->getUseDef(defsOfRhs, itr->second))
            {
            TR_UseDefInfo::BitVector::Cursor cursor(defsOfRhs);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t nextDef = cursor;
               useDefInfo->setUseDef(fixUseIndex, nextDef);
               }
            }
         }
      }

   if (invalidateDefUseInfo)
      useDefInfo->resetDefUseInfo();

   } // scope of the stack memory region

   if (trace())
      {
      traceMsg(comp(), "\nEnding CopyPropagation\n");
      }

   if (donePropagation)
      {
      optimizer()->setAliasSetsAreValid(false);
      requestOpt(OMR::globalDeadStoreElimination, true);
      requestOpt(OMR::partialRedundancyElimination, true);
      }

   optimizer()->setUseDefInfo(NULL);

   return 1; // actual cost
   }

void TR_CopyPropagation::collectUseTrees(TR::TreeTop *tree, TR::Node *node, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return;
   checklist.add(node);

   int32_t useIndex = node->getUseDefIndex();
   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (useIndex >= useDefInfo->getFirstUseIndex() && useIndex <= useDefInfo->getLastUseIndex())
      {
      auto useTreeLookup = _useTreeTops.find(node);
      if (useTreeLookup == _useTreeTops.end())
         _useTreeTops[node] = tree;
      }

   for (int i = 0; i < node->getNumChildren(); i++)
      collectUseTrees(tree, node->getChild(i), checklist);
   }

void TR_CopyPropagation::rematerializeIndirectLoadsFromAutos()
   {
   TR::TreeTop *currentTree = comp()->getStartTree();
   while (currentTree)
      {
      TR::Node *node = currentTree->getNode();
      if (!currentTree->getNextTreeTop())
          break;
      TR::Node *nextNode = currentTree->getNextTreeTop()->getNode();

      if (node->getOpCode().isStoreIndirect() &&
          (node->getType().isIntegral() || node->getType().isAddress()) &&
          node->getFirstChild()->getOpCodeValue() == TR::loadaddr &&
          node->getFirstChild()->getSymbol()->isAutoOrParm() &&
          nextNode->getOpCode().isStoreDirect() &&
          nextNode->getSymbol()->isAutoOrParm() &&
          nextNode->getFirstChild() == node->getSecondChild() &&
          performTransformation(comp(), "%s   Rematerializing indirect load from auto in node %p (temp #%d)\n",OPT_DETAILS, nextNode->getFirstChild(), nextNode->getSymbolReference()->getReferenceNumber()))
         {
         comp()->setOsrStateIsReliable(false); // could check if the propagation was done to a child of the OSR helper call here
         TR::Node * newChild = TR::Node::create(node, comp()->il.opCodeForIndirectLoad(node->getDataType()), 1);
         newChild->setSymbolReference(node->getSymbolReference());
         newChild->setAndIncChild(0, node->getFirstChild());
         nextNode->setAndIncChild(0, newChild);
         node->getSecondChild()->recursivelyDecReferenceCount();
         }
       currentTree = currentTree->getNextTreeTop();
      }

   if (trace())
      comp()->dumpMethodTrees("Trees after rematerialization of indirect loads from autos");

   }


void TR_CopyPropagation::commonIndirectLoadsFromAutos()
   {
   TR::TreeTop *currentTree = comp()->getStartTree();
   while (currentTree)
      {
      TR::Node *node = currentTree->getNode();
      if (!currentTree->getNextTreeTop())
          break;
      TR::Node *nextNode = currentTree->getNextTreeTop()->getNode();
      //   iistore M
      //      loadaddr A
      //      X
      //   istore TMP              ==> istore TMP
      //        iiload M                   X
      //           loadaddr A

      if (node->getOpCode().isStoreIndirect() &&
          (node->getType().isIntegral() || node->getType().isAddress()) &&
          node->getFirstChild()->getOpCodeValue() == TR::loadaddr &&
          node->getFirstChild()->getSymbol()->isAutoOrParm() &&
          nextNode->getOpCode().isStoreDirect() &&
          nextNode->getSymbol()->isAutoOrParm() &&
          nextNode->getFirstChild()->getOpCode().isLoadIndirect() &&
          nextNode->getFirstChild()->getSymbolReference() == node->getSymbolReference() &&
          nextNode->getFirstChild()->getFirstChild() == node->getFirstChild() &&
          performTransformation(comp(), "%s   Commoning indirect load from auto in node %p \n",OPT_DETAILS, nextNode->getFirstChild()))
         {
         comp()->setOsrStateIsReliable(false); // could check if the propagation was done to a child of the OSR helper call here
         nextNode->getFirstChild()->recursivelyDecReferenceCount();
         nextNode->setAndIncChild(0, node->getSecondChild());
         }
      currentTree = currentTree->getNextTreeTop();
      }

   if (trace())
      comp()->dumpMethodTrees("Trees after commoning of indirect loads from autos");

   }

/*
   replaceCopySymbolReferenceByOriginalIn uses a slightly different visitCount idiom --
The visit count for a node is set iff the node has NOT been changed.
This is done to make sure that we will replace ALL qualified nodes (see checks below).

   For example, given the following trees and we copySymbolReference #378
the routine will NOT set the visit count for any iload #378
since it needs to update the symRef for all the ocurrences of such.

TR::treetop
   iload #378
   ...
   more trees
   ...
TR::treetop
   iload #378
*/
void TR_CopyPropagation::replaceCopySymbolReferenceByOriginalIn(TR::SymbolReference *copySymbolReference, TR::Node *origNode, TR::Node *node, TR::Node *defNode, TR::Node * baseAddrNode, bool baseAddrAvail)
   {
   vcount_t curVisit = comp()->getVisitCount();

#ifdef J9_PROJECT_SPECIFIC
   if (_useTree &&
       !node->getOpCode().isLoadConst() &&
       cg()->IsInMemoryType(node->getType()))
      {
      if (cg()->traceBCDCodeGen() && _useTree->getNode()->chkOpsIsInMemoryCopyProp() && !_useTree->getNode()->isInMemoryCopyProp())
         traceMsg(comp(),"\tset IsInMemoryCopyProp on %s (%p), useNode %s (%p)\n",
                 _useTree->getNode()->getOpCode().getName(),_useTree->getNode(),node->getOpCode().getName(),node);
      _useTree->getNode()->setIsInMemoryCopyProp(true);
      }
#endif

   // If this is the first time through this node, examine the children
   //
   if (node->getVisitCount() != curVisit)
      {
      node->setVisitCount(curVisit);
      if (node->getOpCode().hasSymbolReference())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
#ifndef PROPAGATE_GLOBALS
         if (symRef->getReferenceNumber() == copySymbolReference->getReferenceNumber())
#endif
            {
            if (node->getOpCode().isLoadIndirect())
               {
               node->getFirstChild()->recursivelyDecReferenceCount();
               node->setNumChildren(0);
               }

            if (origNode->getOpCode().isLoadIndirect())
               {
               if(baseAddrAvail && baseAddrNode != NULL)
                  {
                  node->setAndIncChild(0, baseAddrNode);
                  node->setNumChildren(1);
                  node->setFlags(origNode->getFlags());
                  node->setSymbolReference(origNode->getSymbolReference());
                  baseAddrNode->setVisitCount(curVisit);
                  }
               else
                  {
                  TR::Node *baseAddrNodeCopy = TR::Node::copy(origNode->getFirstChild());
                  baseAddrNodeCopy->setReferenceCount(0);
                  node->setAndIncChild(0, baseAddrNodeCopy);
                  node->setNumChildren(1);
                  node->setFlags(origNode->getFlags());
                  node->setSymbolReference(origNode->getSymbolReference());
                  }

               // Recreate after all children and symRefs have been set
               TR::Node::recreate(node, origNode->getOpCodeValue());
               }
            else if (origNode->getOpCode().isConversion() &&
                     origNode->getNumChildren() == 1)
               {
               TR::Node *firstChild =  origNode->getFirstChild()->duplicateTree();

               node->setAndIncChild(0, firstChild);
               node->setNumChildren(1);
               node->setFlags(origNode->getFlags());

               // Recreate after all children and symRefs have been set
               TR::Node::recreate(node, origNode->getOpCodeValue());

               firstChild->setVisitCount(curVisit);
               }
#ifdef J9_PROJECT_SPECIFIC
            else if (node->getType().isBCD())
               {
               if (trace() || comp()->cg()->traceBCDCodeGen())
                  traceMsg(comp(),"node %s (%p) #%d->#%d, origNode %s (%p) #%d, defNode %s (%p) #%d,copySymRef #%d\n",
                           node->getOpCode().getName(), node, symRef->getReferenceNumber(),
                           origNode->getOpCode().hasSymbolReference() ? origNode->getSymbolReference()->getReferenceNumber() : -1,
                           origNode->getOpCode().getName(),origNode,
                           origNode->getOpCode().hasSymbolReference() ? origNode->getSymbolReference()->getReferenceNumber() : -1,
                           defNode->getOpCode().getName(),defNode,defNode->getSymbolReference()->getReferenceNumber(),
                           copySymbolReference->getReferenceNumber());

               // BCD tracing might be misleading after this
               if (!origNode->getOpCode().isStore())
                  TR::Node::recreate(node, origNode->getOpCodeValue());

               int32_t newPrecision = 0;
               int32_t currentPrecision = 0;
               bool twoChildrenCase = false;
               TR::Node *firstChild = NULL;
               TR::Node *secondChild = NULL;
               if (origNode->getNumChildren() == 2)
                  {
                  twoChildrenCase = true;
                  TR_ASSERT(0, "Only in WCode we can add extra children\n");
                  }
               else
                  {
                  if (origNode->getOpCode().hasSymbolReference() && node->getOpCode().hasSymbolReference())
                     node->setSymbolReference(origNode->getSymbolReference());
                  int32_t nodePrecision = node->getDecimalPrecision();
                  int32_t nodeSize = node->getSize();
                  int32_t origSymSize = origNode->getSymbolReference()->getSymbol()->getSize();
                  int32_t origSymPrecision = TR::DataType::getBCDPrecisionFromSize(origNode->getDataType(), origSymSize);
                  // for example for a node with prec=30 and an origSymPrecision with prec=31 then if the symbol sizes match and the other checks
                  // below pass then can reduce the newPrecision to 30 so a pdModPrec correction is not needed (as the propagated symbol will not have
                  // the top nibble 30->31 set).
                  currentPrecision = nodePrecision; // 30
                  newPrecision = origSymPrecision; // 31
                  if (node->getType().isAnyPacked())
                     {
                     if (origSymPrecision > nodePrecision && // 31 > 30
                         nodePrecision == origNode->getDecimalPrecision() && // 30 == 30
                         nodeSize == origNode->getSize() && // 16 == 16
                         nodeSize == origSymSize)  // 16 == 16
                        {
                        if (trace() || comp()->cg()->traceBCDCodeGen())
                           traceMsg(comp(),"reduce newPrecision %d->%d for odd to even truncation (origNode %s (%p) prec=%d, node %s (%p) prec=%d\n",
                                   newPrecision,nodePrecision,origNode->getOpCode().getName(),origNode,origNode->getDecimalPrecision(),
                                   node->getOpCode().getName(),node,nodePrecision);
                        newPrecision = nodePrecision; // 31->30 -- no correction needed for same byte odd to even truncation if the origNode was only storing out the even $ of digits
                        }
                     }
                  }

               // if newPrecision < currentPrecision then one or more digits will be lost during the copyPropagation -- the isCorrect* routines disallow this
               TR_ASSERT(newPrecision >= currentPrecision, "newPrecision < currentPrecision (%d < %d) during copyPropagation\n",newPrecision,currentPrecision);

               bool needsClean = defNode && defNode->mustClean();
               bool needsPrecisionCorrection = currentPrecision != newPrecision;

               if (trace() || comp()->cg()->traceBCDCodeGen())
                  traceMsg(comp(),"needsClean = %s, needsPrecisionCorrection = %s (cur %d, new %d)\n",
                          needsClean?"yes":"no",needsPrecisionCorrection?"yes":"no",currentPrecision,newPrecision);


               // pdstore symA-- defNode
               //    pdload symB -- origNode
               //
               // ...
               //    pdload symA->symB -- node
               //
               // may need a pdclean and/or pdModPrec on top of the propagated to node
               //
               //    pdclean  -- node with new op
               //       pdload -- copy of node
               // or
               //    pdclean -- node with new op
               //       pdModPrec -- new node
               //       pdload -- copy of node
               //
               if (needsClean || needsPrecisionCorrection)
                  {
                  // If the copy propagation is going to change the size of the node being replaced then a modifyPrecision node must be prepended in case
                  // the new parent requires explicit widening (such as if the parent is a call).
                  // Also if the defNode was going to clean must also insert a pdclean on top of the node with the propagated symRef

                  TR::Node *nodeCopy = NULL;
                  if (twoChildrenCase)
                     {
                     nodeCopy = TR::Node::copy(origNode);
                     nodeCopy->setReferenceCount(0);
                     nodeCopy->setAndIncChild(0, firstChild);
                     nodeCopy->setAndIncChild(1, secondChild);
                     nodeCopy->setNumChildren(2);
                     }
                  else
                     {
                     nodeCopy = TR::Node::copy(node);
                     nodeCopy->setReferenceCount(0);
                     nodeCopy->setDecimalPrecision(newPrecision);
                     }

                  dumpOptDetails(comp(),"node %p precision %d != propagated symRef #%d precision %d and/or needsClean (%s) so create nodeCopy %p (isTwoChildrenCase=%s)\n",
                                   node,currentPrecision,origNode->getSymbolReference()->getReferenceNumber(),newPrecision,needsClean?"yes":"no",nodeCopy,twoChildrenCase?"yes":"no");

                  if (needsPrecisionCorrection)
                     {
                     TR::ILOpCodes modPrecOp = TR::ILOpCode::modifyPrecisionOpCode(nodeCopy->getDataType());
                     TR_ASSERT(modPrecOp != TR::BadILOp,"no bcd modify precision opcode found for node %p with type %s\n",nodeCopy,nodeCopy->getDataType().toString());
                     if (needsClean) // i.e. if a clean will be generated below then do not modify node op yet, let the top level op (the coming clean) do this
                        {
                        TR::Node *pdModPrecNode = TR::Node::create(modPrecOp, 1, nodeCopy);
                        pdModPrecNode->setDecimalPrecision(currentPrecision);
                        nodeCopy = pdModPrecNode;
                        dumpOptDetails(comp(),"create %s (0x%p) to correct precision %d->%d\n",
                                         pdModPrecNode->getOpCode().getName(),pdModPrecNode,newPrecision,currentPrecision);
                        }
                     else
                        {
                        TR::Node::recreate(node, modPrecOp);

                        node->setAndIncChild(0, nodeCopy);
                        node->setNumChildren(1);
                        node->setFlags(0);
                        dumpOptDetails(comp(),"modify load 0x%p to %s to correct precision %d->%d\n",node,node->getOpCode().getName(),newPrecision,currentPrecision);

                        node = nodeCopy; // so the recursion continues from the correct point below
                        }
                     }

                  if (needsClean)
                     {
                     TR::ILOpCodes cleanOp = TR::ILOpCode::cleanOpCode(nodeCopy->getDataType());
                     TR_ASSERT(cleanOp != TR::BadILOp,"no bcd modify precision cleanOp found for node %p with type %s\n",nodeCopy,nodeCopy->getDataType().toString());
                     // can only clean up to maxPackedPrec -- case allow should have been guaranteed by isCorrectToPropagate
                     TR_ASSERT(node->getDecimalPrecision() <= TR::DataType::getMaxPackedDecimalPrecision(),
                             "node %p prec %d must be <= max %d\n", node, node->getDecimalPrecision(), TR::DataType::getMaxPackedDecimalPrecision());
                     TR::Node::recreate(node, cleanOp);

                     node->setAndIncChild(0, nodeCopy);
                     node->setNumChildren(1);
                     node->setFlags(0);
                     dumpOptDetails(comp(),"modify load 0x%p to %s to clean\n",node,node->getOpCode().getName());

                     node = nodeCopy; // so the recursion continues from the correct point below
                     }
                  }
               else if (twoChildrenCase)
                  {
                  dumpOptDetails(comp(),"attach new first %s (%p) and second %s (%p) to node %s (%p) in the twoChildren=true case\n",
                                   firstChild->getOpCode().getName(),firstChild,
                                   secondChild->getOpCode().getName(),secondChild,
                                   node->getOpCode().getName(),node);

                  node->setAndIncChild(0, firstChild);
                  node->setAndIncChild(1, secondChild);
                  node->setNumChildren(2);
                  node->setFlags(0);

                  firstChild->setVisitCount(curVisit);  // not to visit again ???
                  }
               }
#endif
            else if (origNode->getNumChildren() == 2)
               {
               TR_ASSERT(0, "Only in NON JAVAwe can add extra children\n");
               }
            else
               {
               if (!origNode->getOpCode().isStore())
                  TR::Node::recreate(node, origNode->getOpCodeValue());

               if (origNode->getOpCode().hasSymbolReference() && node->getOpCode().hasSymbolReference())
                  node->setSymbolReference(origNode->getSymbolReference());
               }
            }
         }

#ifndef PROPAGATE_GLOBALS
      for (int i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);
         replaceCopySymbolReferenceByOriginalIn(copySymbolReference, origNode, child, defNode);
         }
#endif
      }
   }


/*
   replaceCopySymbolReferenceByOriginalRegLoadIn uses a slightly different visitCount idiom --
The visit count for a node is set iff the node has NOT been replaced with a regLoadNode.
This is done to make sure that we will replace ALL qualified nodes with the regLoadNode.

   For example, given the following trees and we copySymbolReference #378
the routine will NOT set the visit count for any iload #378
since it needs to remove all the ocurrences of such.

TR::treetop
   iload #378
   ...
   more trees
   ...
TR::treetop
   iload #378
*/
void TR_CopyPropagation::replaceCopySymbolReferenceByOriginalRegLoadIn(TR::Node *regLoadNode, TR::Node *useNode, TR::SymbolReference *copySymbolReference, TR::Node *node, TR::Node *parent, int32_t childNum)
   {
   vcount_t curVisit = comp()->getVisitCount();

   // If this is the first time through this node, examine the children
   //
   if (node->getVisitCount() != curVisit)
      {
      bool changedNode = false;
      if ((useNode == node) && node->getOpCode().hasSymbolReference())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         if (symRef->getReferenceNumber() == copySymbolReference->getReferenceNumber())
            {
            changedNode = true;
            parent->setAndIncChild(childNum, regLoadNode);
            useNode->recursivelyDecReferenceCount();
            }
         }

      if (!changedNode)
         node->setVisitCount(curVisit);

	   for (int i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);
         replaceCopySymbolReferenceByOriginalRegLoadIn(regLoadNode, useNode, copySymbolReference, child, node, i);
         }
      }
   }





bool TR_CopyPropagation::isLoadNodeSuitableForPropagation(TR::Node * useNode, TR::Node * storeNode, TR::Node * loadNode)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (useNode->getType().isBCD() && loadNode->getType().isBCD())
      {
      dumpOptDetails(comp(), "isLoadNodeSuitableForPropagation : useNode %p (prec %d), loadNode %p (prec %d) -- isCorrect=%s (only correct when useNodePrec <= loadNodePrec)\n",
                       useNode, useNode->getDecimalPrecision(), loadNode, loadNode->getDecimalPrecision(), useNode->getDecimalPrecision() <= loadNode->getDecimalPrecision() ? "yes":"no");
      if (useNode->getDecimalPrecision() > loadNode->getDecimalPrecision())
         {
         return false;
         }
      else if (storeNode && storeNode->mustClean() && (useNode->getDecimalPrecision() > TR::DataType::getMaxPackedDecimalPrecision()))
         {
         // have to disallow this case because will not be able to clean useNode after propagation for prec > max
         dumpOptDetails(comp(), "isLoadNodeSuitableForPropagation=false for useNode %s (%p) prec %d > max %d and mustClean store %p\n",
                          useNode->getOpCode().getName(), useNode, useNode->getDecimalPrecision(), TR::DataType::getMaxPackedDecimalPrecision(), storeNode);
         return false;
         }
      }
#endif

   return true;
   }

// isCorrectToPropagate should be called for any load on the RHS with a symbol reference.
void collectNodesForIsCorrectChecks(TR::Node * n, TR::list< TR::Node *> & checkNodes, TR::SparseBitVector & refsToCheckIfKilled, vcount_t vc)
   {
   if (n->getVisitCount() == vc)
      {
      return;
      }

   n->setVisitCount(vc);

   if ((n->getOpCode().isLoadVar() || n->getOpCode().isLoadAddr()) &&
       (n->getSymbolReference() != NULL))
      {
      checkNodes.push_back(n);
      refsToCheckIfKilled[n->getSymbolReference()->getReferenceNumber()] = 1;
      }

   for (size_t i = 0; i < n->getNumChildren(); i++)
      {
      collectNodesForIsCorrectChecks(n->getChild(i), checkNodes, refsToCheckIfKilled, vc);
      }
   }

// If there are load nodes (loadNode or baseNode) in the RHS, and some parent of those nodes has ref count > 1, then add to the
// list of nodes to perform isSafeToPropagate on.
void collectNodesForIsSafeChecks(TR::Node * n, TR::list< TR::Node *> & anchorCheckNodes, vcount_t vc, bool sawRefCountLargerThanOne)
   {
   if (n->getVisitCount() == vc)
      {
      return;
      }

   n->setVisitCount(vc);

   if (n->getReferenceCount() > 1)
      {
      sawRefCountLargerThanOne = true;
      }

   // If either the ref count of the load node itself or a parent node is larger than one,
   // add it to the list of nodes to call isSafeToPropagate on because it could be anchored
   // earlier in the block. Only consider nodes with symbol references, because isSafeToPropagate
   // calls loadNode->getSymbolReference().
   //
   // Note that we want to check any load node - the entire RHS could be propagated.
   //
   if (sawRefCountLargerThanOne &&
       (n->getOpCode().isLoadVar() || n->getOpCode().isLoadAddr()) &&
       (n->getSymbolReference() != NULL))
      {
      anchorCheckNodes.push_back(n);
      }

   for (size_t i = 0; i < n->getNumChildren(); i++)
      {
      collectNodesForIsSafeChecks(n->getChild(i), anchorCheckNodes, vc, sawRefCountLargerThanOne);
      }
   }

void TR_CopyPropagation::adjustUseDefInfo(TR::Node *defNode, TR::Node *useNode, TR_UseDefInfo *useDefInfo)
   {
   // Because loads can now be defs of other loads that they dominate, this
   // adjusting of def info becomes much more complicated.
   // For now we will just invalidate the usedef info.
   //
   optimizer()->setUseDefInfo(NULL);
   }


TR::Node *TR_CopyPropagation::areAllDefsInCorrectForm(TR::Node *useNode, const TR_UseDefInfo::BitVector &defs, TR_UseDefInfo *useDefInfo, int32_t firstRealDefIndex, TR::Node * &loadNode, int32_t &regNumber, TR::Node * &baseNode)
   {
   TR::Node *lastDefNode = NULL;
   loadNode = NULL;
   baseNode = NULL;
   regNumber = -1;

   TR::list< TR::Node *> anchorCheckNodes(getTypedAllocator<TR::Node*>(comp()->allocator()));

   // Disallow more than one def because we can have a situation like
   // a = rhs1;
   // ..
   // b = a;
   //...
   // a = rhs2;
   //...exception point that could reach a catch
   // b = a;
   //...exception point that could reach a catch
   // catch block
   // load b  //  copy propagating "a" here is incorrect since we will end up propagating rhs2 value (in "a") instead of rhs1 value (in "b") changing program semantics
   //
   if (defs.PopulationCount() > 1)
      return NULL;

   TR_UseDefInfo::BitVector::Cursor cursor(defs);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t defIndex = (int32_t) cursor + useDefInfo->getFirstDefIndex();
      if (defIndex < firstRealDefIndex)
         return NULL;  // Definition is method entry

      TR::Node *defNode = useDefInfo->getNode(defIndex);

      if (!defNode) return NULL;

      TR::SymbolReference *copySymbolReference = defNode->getSymbolReference();

#ifdef PROPAGATE_GLOBALS
      if (!defNode->getOpCode().isStore())
         return NULL;
#else
      if (!defNode->getOpCode().isStoreDirect())
         return NULL;
#endif

#ifdef PROPAGATE_GLOBALS
      if (!useDefInfo->isPreciseDef(defNode, useNode))
         return NULL;
#else
      if (!defNode->getSymbolReference()->getSymbol()->isAutoOrParm())
         return NULL;
#endif

      if (defNode->getDataType() != useNode->getDataType() ||
          defNode->getSize() != useNode->getSize())
         return NULL;

      TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);

      TR::Node *rhsOfStoreDefNode = defNode->getOpCode().isStoreIndirect() ? defNode->getChild(1) : defNode->getChild(0);

      _propagatingWholeExpression = false;

      if ((loadNode = isCheapRematerializationCandidate(defNode, rhsOfStoreDefNode)) ||
          (loadNode = isLoadVarWithConst(rhsOfStoreDefNode)) ||
          (loadNode = isIndirectLoadFromAuto(rhsOfStoreDefNode)) ||
          (loadNode = isValidRegLoad(rhsOfStoreDefNode, defTree, regNumber)) ||
          (loadNode = isIndirectLoadFromRegister(rhsOfStoreDefNode, baseNode))
         )

         {
         // !isRegLoad and some node in the defNode has ref count > 1
         if (regNumber == -1)
            {
            anchorCheckNodes.clear();
            collectNodesForIsSafeChecks(rhsOfStoreDefNode, anchorCheckNodes, comp()->incOrResetVisitCount(), false);
            for (auto cur = anchorCheckNodes.begin(); cur != anchorCheckNodes.end(); ++cur)
               {
               if (!isSafeToPropagate(defNode, *cur))
                  {
                  return NULL;
                  }
               }
            }

         if (lastDefNode == NULL)
            {
            lastDefNode = defNode;
            }
         else
            {
            if (loadNode != rhsOfStoreDefNode)
               return NULL; // TODO: handle multiple i = i + 1;

            if (regNumber != -1)
               return NULL; // TODO: handle multiple regLoads

            vcount_t visitCount = comp()->incVisitCount();
            if (!optimizer()->areSyntacticallyEquivalent(lastDefNode->getFirstChild(), rhsOfStoreDefNode, visitCount))
               {
               return NULL;
               }

            lastDefNode = defNode;
            }
         }
      else
         {
         return NULL;
         }
      }

   if (lastDefNode != NULL)
      {
	  TR::list< TR::Node *> checkNodes(getTypedAllocator<TR::Node*>(comp()->allocator()));
      TR::SparseBitVector refsToCheckIfKilled(comp()->allocator());

      // Get all relevant symbol reference numbers.

      collectNodesForIsCorrectChecks(loadNode, checkNodes, refsToCheckIfKilled, comp()->incOrResetVisitCount());

      // Before setting the def node's symbol ref in refsToCheckIfKilled, check to see that the store does not
      // kill the RHS (see inner comment)
      if (_propagatingWholeExpression)
         {
         /*
            Do not propagate i = i + 1 expressions:

            n1800n    istore <temp slot 10> [#498 Auto]
            n1801n      iadd
            n1795n        iload <temp slot 10> [#498 Auto] (cannotOverflow )
            n1799n        ==>iconst -4

          */
         auto lookup = _storeTreeTops.find(lastDefNode);
         if (lookup != _storeTreeTops.end())
            {
            _storeTree = lookup->second;
            _storeBlock = _storeTree->getEnclosingBlock()->startOfExtendedBlock();
            }

         TR::Node * currentNode = skipTreeTopAndGetNode(_storeTree);

         if (foundInterferenceBetweenCurrentNodeAndPropagation(comp(), trace(), currentNode, lastDefNode, checkNodes, refsToCheckIfKilled))
            {
            if (trace())
               {
               comp()->getDebug()->trace("%s   Cannot propagate RHS of i = i + 1 (node %p)\n", OPT_DETAILS, lastDefNode);
               }

            return NULL;
            }
         }

      // Both the load node and def node cannot be redefined between the two points.
      //
      // x <- y
      // ...
      // ...
      // z <- x + ...
      //
      refsToCheckIfKilled[lastDefNode->getSymbolReference()->getReferenceNumber()] = 1;

      if (!isCorrectToPropagate(useNode, lastDefNode, checkNodes, refsToCheckIfKilled, regNumber, defs, useDefInfo))
         {
         return NULL;
         }
      }

   return lastDefNode;
   }



TR::TreeTop * TR_CopyPropagation::findAnchorTree(TR::Node *storeNode, TR::Node *loadNode)
   {
   comp()->incOrResetVisitCount();

   TR::TreeTop *anchor = NULL;

   auto lookup = _storeTreeTops.find(storeNode);
   if (lookup != _storeTreeTops.end())
      {
      TR::TreeTop *treeTop = lookup->second;
      anchor = treeTop;

      if (loadNode)
         {
         TR::SymbolReference *originalSymbolReference = loadNode->getSymbolReference();

         comp()->incOrResetVisitCount();
         while (!((treeTop->getNode()->getOpCode().getOpCodeValue() == TR::BBStart) &&
                  !treeTop->getNode()->getBlock()->isExtensionOfPreviousBlock()))
            {
            comp()->incOrResetVisitCount();
            if (containsNode(treeTop->getNode(), loadNode))
               anchor = treeTop;

            treeTop = treeTop->getPrevTreeTop();
            }
         }
      }

   return anchor;
   }




// Checks to see if the local/parameter on the rhs of the copy
// instruction (say x) might be accessing a value that is anchored earlier
// up in the same block. If this is the case then we need to be
// careful as the value (x) might have been written in the instructions
// between the anchor instruction and the copy instruction, in which case
// propagating the copy would be incorrect (wrong value being propagated)
//
//
bool TR_CopyPropagation::isSafeToPropagate(TR::Node *storeNode, TR::Node *loadNode)
   {
   bool seenStore = false;

   auto lookup = _storeTreeTops.find(storeNode);
   if (lookup != _storeTreeTops.end())
      {
      TR::TreeTop *_storeTreeTop = lookup->second;
      _storeTree = _storeTreeTop;

      if (loadNode)
         {
         TR::SymbolReference *originalSymbolReference = loadNode->getSymbolReference();

         if (storeNode->getSymbolReference() == loadNode->getSymbolReference())
            {
            _storeTreeTop = _storeTreeTop->getPrevTreeTop(); // skip i = i + 1
            }

         comp()->incOrResetVisitCount();

         // Walk the tree backward to first treetop inside extended basic block, reject if org sym is killed
         while (!((_storeTreeTop->getNode()->getOpCode().getOpCodeValue() == TR::BBStart) &&
                  !_storeTreeTop->getNode()->getBlock()->isExtensionOfPreviousBlock()))
            {
            TR::Node * node = skipTreeTopAndGetNode(_storeTreeTop);

            if (node->mayKill().contains(originalSymbolReference, comp()))
               seenStore = true;

            if (seenStore && containsNode(_storeTreeTop->getNode(), loadNode))
               return false;

            _storeTreeTop = _storeTreeTop->getPrevTreeTop();
            }
         }

      return true;
      }

   TR_ASSERT(0, "end of isSafeToPropagate!");
   return false;
   }


TR::Node* TR_CopyPropagation::skipTreeTopAndGetNode (TR::TreeTop* tt)
   {
   TR_ASSERT(tt, "tt should always be set no matter if it comes from _storeTreeTops an iterator");
   return (tt->getNode()->getOpCodeValue() == TR::treetop) ? tt->getNode()->getFirstChild() : tt->getNode();
   }


bool TR_CopyPropagation::containsNode(TR::Node *node1, TR::Node *node2, bool searchClones)
   {
    vcount_t curVisit = comp()->getVisitCount();

   // If this is the first time through this node, examine the children
   //
   if (node1->getVisitCount() != curVisit)
      {
      node1->setVisitCount(curVisit);

        if (node1 == node2)
             return true;

      for (int i = 0; i < node1->getNumChildren(); i++)
         {
         TR::Node *child = node1->getChild(i);
         if (containsNode(child, node2))
            return true;
         }
      }

   return false;
   }

bool TR_CopyPropagation::containsLoadOfSymbol(TR::Node * node, TR::SymbolReference * symRef, TR::Node ** loadNode)
  {
    vcount_t curVisit = comp()->getVisitCount();

    if(node->getVisitCount() != curVisit)
      {
        node->setVisitCount(curVisit);

        if(node->getOpCode().isLoadVar() && node->getSymbolReference() == symRef)
          {
              *loadNode = node;
              return true;
          }

        for(int i = 0; i < node->getNumChildren(); i++)
            {
             TR::Node * child = node->getChild(i);
             if(containsLoadOfSymbol(child,symRef,loadNode))
                  return true;
            }
      }

     return false;
  }

void TR_CopyPropagation::findUseTree(TR::Node *useNode)
   {
   if (_useTree) return;

   auto lookup = _useTreeTops.find(useNode);
   if (lookup != _useTreeTops.end())
      _useTree = lookup->second;
   }

bool TR_CopyPropagation::isCorrectToPropagate(TR::Node *useNode, TR::Node *storeNode, TR::list< TR::Node *> & checkNodes,
      TR::SparseBitVector & refsToCheckIfKilled, int32_t regNumber, const TR_UseDefInfo::BitVector &defs, TR_UseDefInfo *useDefInfo)
   {
   TR::TreeTop *currentTree = comp()->getStartTree();
   _storeTree = NULL;
   _storeBlock = NULL;
   _useTree = NULL;

   findUseTree(useNode);

   if (_storeTree == NULL)
      {
      auto lookup = _storeTreeTops.find(storeNode);
      if (lookup != _storeTreeTops.end())
         {
         _storeTree = lookup->second;
         _storeBlock = _storeTree->getEnclosingBlock()->startOfExtendedBlock();
         }
      }

   // Check if the storeNode's valueChild is suitable for propagation.
   if (!isLoadNodeSuitableForPropagation(useNode, storeNode, storeNode->getValueChild()))
      {
      return false;
      }

   // The logic for propagating iRegLoads does not work if the def and use are in the same extended block
   if (_storeTree->getEnclosingBlock()->startOfExtendedBlock() == _useTree->getEnclosingBlock()->startOfExtendedBlock())
      {
      for (auto checkNodesCursor = checkNodes.begin(); checkNodesCursor != checkNodes.end(); ++checkNodesCursor)
         {
         if ((*checkNodesCursor)->getOpCode().isLoadReg())
            {
            return false;
            }
         }
      }

   currentTree = _useTree->getPrevTreeTop();
   while (!(currentTree->getNode()->getOpCode().getOpCodeValue() == TR::BBStart))
      {
      //TR_ASSERT(_storeTree->getNode()->getOpCode().isStoreDirect(), "Def node should be a direct store\n");
      TR::Node *currentNode = skipTreeTopAndGetNode (currentTree);

      if (currentTree == _storeTree)
         {
         return true;
         }
#ifdef PROPAGATE_GLOBALS
      else if (_lookForOriginalDefs &&
               currentNode->getOpCode().isStore() &&
               defs.ValueAt(currentNode->getUseDefIndex()-useDefInfo->getFirstDefIndex()))
#else
      else if (_lookForOriginalDefs &&
              currentNode->getOpCode().isStoreDirect() &&
              currentNode->getSymbolReference() == storeNode->getSymbolReference())
#endif
         {
         return true;
         }
      else
         {
         if (regNumber == -1)
            {
            if (foundInterferenceBetweenCurrentNodeAndPropagation(comp(), trace(), currentNode, storeNode, checkNodes, refsToCheckIfKilled))
               {
               return false; // is not correct
               }
            }
         }

      currentTree = currentTree->getPrevTreeTop();
      }

   TR::Block *block = currentTree->getNode()->getBlock();
   vcount_t visitCount = comp()->incOrResetVisitCount();
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::CFGEdge *nextEdge;

   for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getFrom());
      if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()) &&
          ((regNumber == -1) || (next->startOfExtendedBlock() != _storeBlock)))
         {
         if (isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, next->getExit(), regNumber, defs, useDefInfo))
            {
            return false;
            }
         }
      }

   for (auto nextEdge = block->getExceptionPredecessors().begin(); nextEdge != block->getExceptionPredecessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getFrom());
      if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()) &&
          ((regNumber == -1) || (next->startOfExtendedBlock() != _storeBlock)))
         {
         if (isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, next->getExit(), regNumber, defs, useDefInfo))
            {
            return false;
            }
         }
      }

   return true;
   }

bool TR_CopyPropagation::isRedefinedBetweenStoreTreeAnd(TR::list< TR::Node *> & checkNodes, TR::SparseBitVector & refsToCheckIfKilled, TR::Node * storeNode, TR::TreeTop *exitTree, int32_t regNumber, const TR_UseDefInfo::BitVector &defs, TR_UseDefInfo *useDefInfo)
   {

   static char *useRecursiveIsRedefinedBetweenStoreTreeAnd = feGetEnv("TR_useRecursiveIsRedefinedBetweenStoreTreeAnd");
   if (useRecursiveIsRedefinedBetweenStoreTreeAnd)
      return recursive_isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, exitTree, regNumber, defs, useDefInfo);

   TR_ScratchList<TR::TreeTop> treeTopList (trMemory());

   TR::TreeTop *currentTree = exitTree;
   bool visitPredecessors;
   vcount_t saved_vc = comp()->getVisitCount();

   while(currentTree != NULL)
      {
      visitPredecessors = true;

      while (!(currentTree->getNode()->getOpCode().getOpCodeValue() == TR::BBStart))
         {
         TR::Node *currentNode = skipTreeTopAndGetNode(currentTree);

         //TR_ASSERT(_storeTree->getNode()->getOpCode().isStoreDirect(), "Def node should be a direct store\n");

         if (currentTree == _storeTree)
            {
            visitPredecessors = false;
            break;
            }
#ifdef PROPAGATE_GLOBALS
         else if (_lookForOriginalDefs &&
                  currentNode->getOpCode().isStore() &&
                  defs.ValueAt(currentNode->getUseDefIndex()-useDefInfo->getFirstDefIndex()))
#else
         else if (_lookForOriginalDefs &&
                 currentNode->getOpCode().isStoreDirect() &&
                 currentNode->getSymbolReference() == storeNode->getSymbolReference())
#endif
            {
            visitPredecessors = false;
            break;
            }

         else
            {
            if (regNumber == -1)
               {
               if (foundInterferenceBetweenCurrentNodeAndPropagation(comp(), trace(), currentNode, storeNode, checkNodes, refsToCheckIfKilled))
                  {
                  return true; // is redefined
                  }
               }
            else
               {
               if (currentNode->getOpCode().isStoreReg())
                  {
                  if (currentNode->getGlobalRegisterNumber() == regNumber)
                     return true;
                  }
               }
            }

         currentTree = currentTree->getPrevTreeTop();
         }

      // Visit count cannot change because it is used to mark blocks traversed.
      TR_ASSERT(saved_vc == comp()->getVisitCount(), "visit count changed in loop!");

      if (visitPredecessors)
         {
         TR::Block *block = currentTree->getNode()->getBlock();
         TR::CFG *cfg = comp()->getFlowGraph();
         vcount_t visitCount = comp()->getVisitCount();
         block->setVisitCount(visitCount);

         TR_PredecessorIterator precedingEdges(block);
         for (auto precedingEdge = precedingEdges.getFirst(); precedingEdge; precedingEdge = precedingEdges.getNext())
            {
            TR::Block *next = toBlock(precedingEdge->getFrom());
            if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()) &&
                ((regNumber == -1) || (next->startOfExtendedBlock() != _storeBlock)))
               {
               treeTopList.add(next->getExit());
               }
            }
         }
      currentTree = treeTopList.popHead();
      }

   return false;
   }

bool TR_CopyPropagation::recursive_isRedefinedBetweenStoreTreeAnd(TR::list< TR::Node *> & checkNodes, TR::SparseBitVector & refsToCheckIfKilled, TR::Node * storeNode, TR::TreeTop *exitTree, int32_t regNumber, const TR_UseDefInfo::BitVector &defs, TR_UseDefInfo *useDefInfo)
   {
   TR::TreeTop *currentTree = exitTree;
   vcount_t saved_vc = comp()->getVisitCount();
   while (!(currentTree->getNode()->getOpCode().getOpCodeValue() == TR::BBStart))
      {
      //TR_ASSERT(_storeTree->getNode()->getOpCode().isStoreDirect(), "Def node should be a direct store\n");

      TR::Node *currentNode = skipTreeTopAndGetNode(currentTree);

      if (currentTree == _storeTree)
         {
         return false;
         }
#ifdef PROPAGATE_GLOBALS
      else if (_lookForOriginalDefs &&
               currentNode->getOpCode().isStore() &&
               defs.ValueAt(currentNode->getUseDefIndex()-useDefInfo->getFirstDefIndex()))
#else
      else if (_lookForOriginalDefs &&
              currentNode->getOpCode().isStoreDirect() &&
              currentNode->getSymbolReference() == storeNode->getSymbolReference())
#endif
         {
         return false;
         }

      else
         {
         if (regNumber == -1)
            {
            if (foundInterferenceBetweenCurrentNodeAndPropagation(comp(), trace(), currentNode, storeNode, checkNodes, refsToCheckIfKilled))
               {
               return true; // is redefined
               }
            }
         else
            {
            if (currentNode->getOpCode().isStoreReg())
               {
               if (currentNode->getGlobalRegisterNumber() == regNumber)
                  return true;
               }
            }
         }

      currentTree = currentTree->getPrevTreeTop();
      }

   TR::Block *block = currentTree->getNode()->getBlock();
   TR::CFG *cfg = comp()->getFlowGraph();
   vcount_t visitCount = comp()->getVisitCount();
   block->setVisitCount(visitCount);

   TR::CFGEdge *nextEdge;

   // Visit count cannot change because it is used to mark blocks traversed.
   TR_ASSERT(saved_vc == comp()->getVisitCount(), "visit count changed in loop!");

   for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getFrom());
      if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()) &&
          ((regNumber == -1) || (next->startOfExtendedBlock() != _storeBlock)))
         {
         if (recursive_isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, next->getExit(), regNumber, defs, useDefInfo))
            return true;
         }
      }

   for (auto nextEdge = block->getExceptionPredecessors().begin(); nextEdge != block->getExceptionPredecessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getFrom());
      if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()) &&
          ((regNumber == -1) || (next->startOfExtendedBlock() != _storeBlock)))
         {
         if (recursive_isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, next->getExit(), regNumber, defs, useDefInfo))
            return true;
         }
      }

   return false;
   }


bool TR_CopyPropagation::isCorrectToReplace(TR::Node *useNode, TR::Node *storeNode, const TR_UseDefInfo::BitVector &defs, TR_UseDefInfo *useDefInfo)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (useNode->getType().isBCD() && storeNode->getType().isBCD())
      {
      dumpOptDetails(comp(),"isCorrectToReplace : useNode %p (prec %d), storeNode %p (prec %d) -- isCorrect=%s (only correct when useNodePrec <= storeNodePrec)\n",
         useNode,useNode->getDecimalPrecision(),storeNode, storeNode->getDecimalPrecision(),useNode->getDecimalPrecision()<=storeNode->getDecimalPrecision() ? "yes":"no");
      if (useNode->getDecimalPrecision() > storeNode->getDecimalPrecision())
         {
         return false;
         }
      }
#endif

   TR::TreeTop *currentTree = comp()->getStartTree();

   TR::SparseBitVector refsToCheckIfKilled(comp()->allocator());
   TR::list<TR::Node *> checkNodes(getTypedAllocator<TR::Node*>(comp()->allocator()));

   // RHS of storeNode will be propagated, so collect stuff from it

   collectNodesForIsCorrectChecks(storeNode, checkNodes, refsToCheckIfKilled, comp()->incOrResetVisitCount());

   // x <- y
   // ..
   // z <- x + a
   refsToCheckIfKilled[storeNode->getSymbolReference()->getReferenceNumber()] = 1;

   _useTree = NULL;
   findUseTree(useNode);

   if (_storeTree == NULL)
      {
      auto lookup = _storeTreeTops.find(storeNode);
      if (lookup != _storeTreeTops.end())
         {
         _storeTree = lookup->second;
         _storeBlock = _storeTree->getEnclosingBlock()->startOfExtendedBlock();
         }
      }

   currentTree = _useTree->getPrevTreeTop();
   while (!(currentTree->getNode()->getOpCode().getOpCodeValue() == TR::BBStart))
      {
      if (currentTree == _storeTree)
         return true;
      else
         {
         TR::Node *currentNode = skipTreeTopAndGetNode(currentTree);
         if (foundInterferenceBetweenCurrentNodeAndPropagation(comp(), trace(), currentNode, storeNode, checkNodes, refsToCheckIfKilled))
            {
            return false; // is not correct
            }
         }

      currentTree = currentTree->getPrevTreeTop();
      }

   TR::Block *block = currentTree->getNode()->getBlock();
   vcount_t visitCount = comp()->incOrResetVisitCount();
   TR::CFGEdge *nextEdge;
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::SymbolReference *symbolReference = storeNode->getSymbolReference();

   for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getFrom());
      if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()))
         {
         if (isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, next->getExit(), -1, defs, useDefInfo))
            return false;
         }
      }

   for (auto nextEdge = block->getExceptionPredecessors().begin(); nextEdge != block->getExceptionPredecessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getFrom());
      if ((next->getVisitCount() != visitCount) && (next != cfg->getStart()))
         {
         if (isRedefinedBetweenStoreTreeAnd(checkNodes, refsToCheckIfKilled, storeNode, next->getExit(), -1, defs, useDefInfo))
            return false;
         }
      }

   return true;
   }



TR::Node * TR_CopyPropagation::isLoadVarWithConst(TR::Node *node)
   {
   if (node->getOpCode().isLoadVarDirect() &&
       node->getSymbolReference()->getSymbol()->isAutoOrParm())
      {
      return node;
      }

   if (TR::TransformUtil::isNoopConversion(comp(), node) &&
       node->getNumChildren() == 1)
      return isLoadVarWithConst(node->getFirstChild());

   return NULL;
   }


TR::Node * TR_CopyPropagation::isIndirectLoadFromAuto(TR::Node *node)
   {
   if (!_cleanupTemps)
      return NULL;

   if (node->getOpCode().isLoadIndirect() &&
       node->getFirstChild()->getOpCodeValue() == TR::loadaddr &&
       node->getFirstChild()->getSymbol()->isAutoOrParm())
      {
      return node;
      }

   return NULL;
   }

TR::Node * TR_CopyPropagation::isIndirectLoadFromRegister(TR::Node *node, TR::Node * &baseNode)
   {
   baseNode = NULL;

   if (!_cleanupTemps)
      return NULL;

   // check that base is direct load for now,
   // we will see if it can end up in a register later
   if (node->getOpCode().isLoadIndirect() &&
       node->getFirstChild()->getOpCode().isLoadVarDirect())
      {
      baseNode = node->getFirstChild();
      return node;
      }

   return NULL;
   }

bool nodeContainsLoadReg(TR::Compilation * comp, TR::Node * n, vcount_t vc)
   {
   if (n->getVisitCount() == vc)
      {
      return false;
      }

   n->setVisitCount(vc);

   if (n->getOpCode().isLoadReg())
      {
      return true;
      }

   for (size_t i = 0; i < n->getNumChildren(); i++)
      {
      if (nodeContainsLoadReg(comp, n->getChild(i), vc))
         {
         return true;
         }
      }

   return false;
   }

TR::Node * TR_CopyPropagation::isCheapRematerializationCandidate(TR::Node * defNode, TR::Node * node)
   {
   // Only run if GRA has already run - do not rematerialize if GRA hasn't had the chance to work with the temps.
   if (!comp()->cg()->getGRACompleted())
      {
      return NULL;
      }

   if ((defNode->getSymbolReference() == NULL) ||
       !(comp()->IsCopyPropagationRematerializationCandidate(defNode->getSymbolReference())))
      {
      return NULL;
      }

   if (node->containsDoNotPropagateNode(comp()->incOrResetVisitCount()))
      {
      return NULL;
      }

   if (nodeContainsLoadReg(comp(), node, comp()->incOrResetVisitCount()))
      {
      return NULL;
      }

   // Identify cheap expressions
   bool cheapExpression = false;

   // 1) indirect loads where the base ptr is on the stack or a location pointer
   //
   //  iaload <refined-array-shadow> [id=238:"PSALIT2"] [#1.64 Shadow] [flags 0x80000607 0x0 ]  [0x2B026BFC] bci=[-1,37,329] rc=2 vc=9438 vn=- sti=- udi=331 nc=1 addr=4
   //    aiadd (internalPtr )                                                            [0x2B016B20] bci=[-1,37,329] rc=1 vc=9438 vn=- sti=- udi=- nc=2 addr=4 flg=0x8000
   //      loadaddr <auto slot 40> [id=152:"PSA"] [#1.63 Auto] [flags 0x4000008 0x0 ]    [0x2B016AD0] bci=[-1,37,329] rc=1 vc=9438 vn=- sti=- udi=- nc=0 addr=4
   //      iconst 1212 (X!=0 X>=0 )                                                      [0x2B016B70] bci=[-1,37,329] rc=1 vc=9438 vn=- sti=- udi=- nc=0 flg=0x104
   //
   if (node->getOpCode().isLoadIndirect())
      {
      TR::Node * addr = node->getFirstChild();

      if ((addr->getOpCodeValue() == TR::loadaddr) &&
          addr->getSymbol()->isAutoOrParm())
         {
         cheapExpression = true;
         }

      if (!cheapExpression && addr->getOpCode().isAdd())
         {
         if ((addr->getFirstChild()->getOpCodeValue() == TR::loadaddr) &&
             addr->getFirstChild()->getSymbol()->isAutoOrParm() &&
             addr->getSecondChild()->getOpCode().isLoadConst())
            {
            cheapExpression = true;
            }
         }
      }

   // 2) conversions
   if (!cheapExpression && node->getOpCode().isConversion())
      {
      cheapExpression = true;
      }

   // TODO: a trdbg option to propagate all expressions anyway?

   if (!cheapExpression)
      {
      if (trace())
         {
         comp()->getDebug()->trace("%s   skipping attempt at propagating %p because it is not cheap\n",
                                   OPT_DETAILS, node);
         }

      return NULL;
      }

   _propagatingWholeExpression = true;
   return node;
   }

bool TR_CopyPropagation::isNodeAvailableInBlock(TR::TreeTop *useTree, TR::Node *loadNode)
   {
   TR::TreeTop *treeTop = useTree;

   comp()->incOrResetVisitCount();
   while (treeTop->getNode()->getOpCode().getOpCodeValue() != TR::BBStart ||
          treeTop->getNode()->getBlock()->isExtensionOfPreviousBlock())
      {
      TR::Node * node = skipTreeTopAndGetNode(treeTop);
      if (loadNode->getOpCode().hasSymbolReference() &&
          node->mayKill().contains(loadNode->getSymbolReference(), comp()))
         return false;

      TR::Node *dummyNode;
      if (containsLoadOfSymbol(node, loadNode->getSymbolReference(), &dummyNode))
         {
         return true;
         }

      treeTop = treeTop->getPrevTreeTop();
      }

   return false;
   }



TR::Node *TR_CopyPropagation::isValidRegLoad(TR::Node *rhsOfStoreDefNode, TR::TreeTop *defTree, int32_t &regNumber)
   {
               regNumber = -1;
               bool isRegLoad = false;
               if (rhsOfStoreDefNode->getOpCode().isLoadReg())
                  {
                  isRegLoad = true;
                  // TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);

                  TR::Block *defBlock = defTree->getEnclosingBlock();
                  TR::Block *prevDefBlock = defBlock;
                  while (defBlock && defBlock->isExtensionOfPreviousBlock())
                     {
                     prevDefBlock = defBlock;

                     TR::TreeTop *lastTree = defBlock->getLastRealTreeTop();
                     TR::Node *lastNode = skipTreeTopAndGetNode(lastTree);
                     if (lastNode->getOpCode().isBranch() ||
                         lastNode->getOpCode().isJumpWithMultipleTargets())
                        {
                        isRegLoad = false;
                        if (lastNode->getNumChildren() > 0)
                           {
                           TR::Node *child = lastNode->getChild(lastNode->getNumChildren()-1);
                           if (child->getOpCodeValue() == TR::GlRegDeps)
                              {
                              for (int i = 0; i < child->getNumChildren(); i++)
                                 {
                                 TR::Node *regChild = child->getChild(i);
                                 TR::Node *rhsChild = regChild;
                                 if (rhsChild->getOpCodeValue() == TR::PassThrough)
                                    rhsChild = rhsChild->getFirstChild();

                                 if (rhsChild == rhsOfStoreDefNode)
                                    {
                                    if (regNumber == -1)
                                       {
                                       regNumber = regChild->getGlobalRegisterNumber();
                                       isRegLoad = true;
                                       }
                                    else if (regNumber == regChild->getGlobalRegisterNumber())
                                       isRegLoad = true;

                                    break;
                                    }
                                 }
                              }
                           }
                        }

                     if (!isRegLoad)
                        break;

                     defBlock = defBlock->getNextBlock();
                     }

                  if (isRegLoad && prevDefBlock)
                     {
                     TR::TreeTop *exitTree = prevDefBlock->getExit();
                     TR::Node *lastNode = exitTree->getNode();
                     isRegLoad = false;

                     if (lastNode->getNumChildren() > 0)
                        {
                        TR::Node *child = lastNode->getChild(lastNode->getNumChildren()-1);
                        if (child->getOpCodeValue() == TR::GlRegDeps)
                           {
                           for (int i = 0; i < child->getNumChildren(); i++)
                              {
                              TR::Node *regChild = child->getChild(i);
                              TR::Node *rhsChild = regChild;
                              if (rhsChild->getOpCodeValue() == TR::PassThrough)
                                 rhsChild = rhsChild->getFirstChild();

                              if (rhsChild == rhsOfStoreDefNode)
                                 {
                                 if (regNumber == -1)
                                    {
                                    regNumber = regChild->getGlobalRegisterNumber();
                                    isRegLoad = true;
                                    }
                                 else if (regNumber == regChild->getGlobalRegisterNumber())
                                    isRegLoad = true;

                                 break;
                                 }
                              }
                           }
                        }
                     }
                  }

               if (!isRegLoad)
                  regNumber = -1;

   return (isRegLoad ? rhsOfStoreDefNode : NULL);
   }

const char *
TR_CopyPropagation::optDetailString() const throw()
   {
   return "O^O COPY PROPAGATION: ";
   }
