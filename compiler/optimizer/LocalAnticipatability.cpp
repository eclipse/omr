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

#include <stdint.h>
#include <stdlib.h>
#include "env/StackMemoryRegion.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "infra/BitVector.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "compile/AliasBuilder.hpp"

TR_LocalAnticipatability::TR_LocalAnticipatability(TR_LocalAnalysisInfo &info, TR_LocalTransparency *lt, bool t)
   : _localTransparency(lt),
    TR_LocalAnalysis(info, t)
   {
   if (trace())
      traceMsg(comp(), "Starting LocalAnticipatability\n");

   static const char *e = feGetEnv("TR_loadaddrAsLoad");
   _loadaddrAsLoad = e ? atoi(e) : true;

   initializeLocalAnalysis(true);
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

   // Used to filter out alias information that is unused for
   // this analysis as calls cannot really be modified (written).
   // Pure calls can be modified (ie. PRE'd), so exclude those.
   _seenCallSymbolReferences = allocateContainer(comp()->getMaxAliasIndex());
   auto methodsBV = symRefTab->aliasBuilder.methodSymRefs(); // grab a Copy
   TR_SymRefIterator itr(methodsBV, symRefTab);

   // reset pure function symbol references
   for (auto methodSymRef = itr.getNext(); methodSymRef; methodSymRef = itr.getNext())
      {
      if (methodSymRef->getSymbol()->getMethodSymbol() &&
          methodSymRef->getSymbol()->getMethodSymbol()->isPureFunction())
         methodsBV.reset(methodSymRef->getReferenceNumber());
      }

   *_seenCallSymbolReferences |= methodsBV;

   //int32_t symRefCount = comp()->getMaxAliasIndex();
   //_filteredAliases = allocateContainer(symRefCount);

   // Used for aliasing calls
   TR_BitVector *temp = new (trStackMemory()) TR_BitVector(comp()->getMaxAliasIndex(), trMemory(), stackAlloc);
   _downwardExposedBeforeButNotAnymore = allocateTempContainer(getNumNodes());
   _notDownwardExposed = allocateTempContainer(getNumNodes());
   _visitedNodes = allocateTempContainer(comp()->getNodeCount());
   _visitedNodesAfterThisTree = allocateTempContainer(comp()->getNodeCount());
   _temp = allocateTempContainer(getNumNodes());
   _temp2 = allocateTempContainer(getNumNodes());

   vcount_t visitCount1 = 0;
   vcount_t visitCount2 = 0;
   TR::Block *block = NULL;
   for (block = comp()->getStartBlock(); block!=NULL; block = block->getNextBlock())
      {
      if (!block->isExtensionOfPreviousBlock())
         {
         visitCount1 = comp()->incOrResetVisitCount();
         visitCount2 = comp()->incVisitCount();
         }

      TR_LocalAnalysisInfo::LAInfo *binfo = _info+(block->getNumber());
      if (binfo->_block == NULL)
          continue;
      binfo->_analysisInfo->empty();
      binfo->_downwardExposedAnalysisInfo->empty();
      binfo->_downwardExposedStoreAnalysisInfo->empty();
      _downwardExposedBeforeButNotAnymore->empty();
      _notDownwardExposed->empty();
      _temp->empty();
      _temp2->empty();

      analyzeBlock(block, visitCount1, visitCount2, temp);

      if (trace())
          {
          traceMsg(comp(), "\nSolution for block number : %d\n", block->getNumber());
          binfo->_analysisInfo->print(comp());
          binfo->_downwardExposedAnalysisInfo->print(comp());
          binfo->_downwardExposedStoreAnalysisInfo->print(comp());
          }
      }

   if (trace())
      traceMsg(comp(), "\nEnding LocalAnticipatability\n");
   }

void TR_LocalAnticipatability::analyzeBlock(TR::Block *block, vcount_t visitCount1, vcount_t visitCount2, TR_BitVector *temp)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR::TreeTop *currentTree = block->getEntry();
   TR::TreeTop *exitTree = block->getExit();
   int32_t symRefCount = comp()->getMaxAliasIndex();

   ContainerType *seenDefinedSymbolReferences = allocateTempContainer(symRefCount);
   ContainerType *seenStoredSymbolReferences = allocateTempContainer(symRefCount);
   ContainerType *seenUsedSymbolReferences = allocateTempContainer(symRefCount);
   ContainerType *allSymbolReferences = allocateTempContainer(symRefCount);
   ContainerType *allSymbolReferencesInNullCheckReference = allocateTempContainer(symRefCount);
   ContainerType *tempContainer = allocateTempContainer(symRefCount);
   ContainerType *storeNodes = allocateTempContainer(getNumNodes());

   // Must leave as TR_BitVectory because of use in aliasing calls
   TR_BitVector *allSymbolReferencesInStore = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);

   // Once an expression is killed, it cannot become anticiptable again. This is to avoid
   // running into problems where there are two loads of the same symbol (one before a def
   // point (say a call) and one after the def point in a given basic block). In this case we MUST say that
   // the symbol load is not anticipatable as we do not want to common the two different uses
   // leading to possibly incorrect code.
   //
   ContainerType *killedExpressions = allocateTempContainer(getNumNodes());

   // Walk through the trees in this block.
   //
   while (currentTree != exitTree)
      {
      TR::ILOpCode &firstOpCodeInTree = currentTree->getNode()->getOpCode();

      allSymbolReferences->empty();
      allSymbolReferencesInNullCheckReference->empty();
      allSymbolReferencesInStore->empty();
      _storedSymbolReferenceNumber = -1;
      _checkTree = NULL;
      _isStoreTree = false;
      _isNullCheckTree = false;
      _inNullCheckReferenceSubtree = false;
      _inStoreLhsTree = false;

      bool storeTreeParticipatesInAnalysis = true;
      TR::SymbolReference *storeSymRef = NULL;
      TR::Node *storeSymRefNode = NULL;
      if (firstOpCodeInTree.getOpCodeValue() == TR::NULLCHK)
         {
         _isNullCheckTree = true;
         _checkTree = currentTree;
         if (currentTree->getNode()->getFirstChild()->getOpCode().isStore())
            {
            _isStoreTree = true;
            if (currentTree->getNode()->getFirstChild()->getLocalIndex() == MAX_SCOUNT)
               storeTreeParticipatesInAnalysis = false;
            storeSymRef = currentTree->getNode()->getFirstChild()->getSymbolReference();
            storeSymRefNode = currentTree->getNode()->getFirstChild();
            _storedSymbolReferenceNumber = storeSymRef->getReferenceNumber();
            }
         }
      else if (firstOpCodeInTree.isResolveCheck())
         {
         _checkTree = currentTree;
         if (currentTree->getNode()->getFirstChild()->getOpCode().isStore())
            {
            if (currentTree->getNode()->getFirstChild()->getLocalIndex() == MAX_SCOUNT)
               storeTreeParticipatesInAnalysis = false;
            _isStoreTree = true;
            storeSymRef = currentTree->getNode()->getFirstChild()->getSymbolReference();
            storeSymRefNode = currentTree->getNode()->getFirstChild();
            _storedSymbolReferenceNumber = storeSymRef->getReferenceNumber();
            }
         }
      else if ((firstOpCodeInTree.getOpCodeValue() == TR::treetop) ||
               (comp()->useAnchors() && firstOpCodeInTree.isAnchor()))
         {
         if (currentTree->getNode()->getFirstChild()->getOpCode().isStore())
            {
            _isStoreTree = true;
             if (currentTree->getNode()->getFirstChild()->getLocalIndex() == MAX_SCOUNT)
             storeTreeParticipatesInAnalysis = false;
             storeSymRef = currentTree->getNode()->getFirstChild()->getSymbolReference();
             storeSymRefNode = currentTree->getNode()->getFirstChild();
             _storedSymbolReferenceNumber = storeSymRef->getReferenceNumber();
            }
         }
      else if (firstOpCodeInTree.isStore())
         {
         _isStoreTree = true;
         if (currentTree->getNode()->getLocalIndex() == MAX_SCOUNT)
            storeTreeParticipatesInAnalysis = false;
         storeSymRef = currentTree->getNode()->getSymbolReference();
         storeSymRefNode = currentTree->getNode();
         _storedSymbolReferenceNumber = storeSymRef->getReferenceNumber();
         }
      else if (firstOpCodeInTree.isCheck())
         _checkTree = currentTree;

      updateAnticipatabilityForSupportedNodes(currentTree->getNode(), seenDefinedSymbolReferences, seenStoredSymbolReferences, block, killedExpressions, allSymbolReferences, allSymbolReferencesInNullCheckReference, allSymbolReferencesInStore, storeNodes, visitCount1);

      if (_isStoreTree)
         {
         // If this tree has a store in it
         //
         TR::Node *treeTopNode = currentTree->getNode();
         TR::Node *node = treeTopNode->getStoreNode();

         TR::SymbolReference *symRef = node->getSymbolReference();

         //firstOpCodeInTree.isStore()
         if (storeSymRef)
            {
            if (storeSymRef->sharesSymbol(true))
               {
               storeSymRefNode->mayKill(true).getAliasesAndSubtractFrom(*allSymbolReferencesInStore);
               }
            else
               allSymbolReferencesInStore->reset(_storedSymbolReferenceNumber);
            }

         bool isCurrentTreeTopAnticipatable = true;
         if (!storeTreeParticipatesInAnalysis)
            isCurrentTreeTopAnticipatable = false;
         //
         // If any symbol being used in this tree is written in this block,
         // then NOT anticipatable
         //
         if (!allSymbolReferencesInStore->isEmpty())
            {
            tempContainer->empty();
            *tempContainer |= *seenDefinedSymbolReferences;
            *tempContainer -= *_seenCallSymbolReferences;
            *tempContainer &= *allSymbolReferencesInStore;
            if (!tempContainer->isEmpty())
                isCurrentTreeTopAnticipatable = false;

            if (isCurrentTreeTopAnticipatable)
               {
               tempContainer->empty();
               *tempContainer |= *seenStoredSymbolReferences;
               *tempContainer -= *_seenCallSymbolReferences;
               *tempContainer &= *allSymbolReferencesInStore;
               if (!tempContainer->isEmpty())
                  {
                  if (storeTreeParticipatesInAnalysis &&
                      !storeNodes->get(node->getLocalIndex()))
                      isCurrentTreeTopAnticipatable = false;
                  }
               }
            }

         // If the symbol being defined by the store is used in this block
         // then NOT anticipatable
         //
         if (symRef->getSymbol()->isAutoOrParm())
            {
            if (seenUsedSymbolReferences->get(symRef->getReferenceNumber()))
               isCurrentTreeTopAnticipatable = false;
            }

         if (storeTreeParticipatesInAnalysis)
            {
             bool downwardExposed = true;
             if (node->getOpCode().isIndirect() &&
                 ((node->getFirstChild()->getLocalIndex() != MAX_SCOUNT && node->getFirstChild()->getLocalIndex() != 0) &&
                   ((_downwardExposedBeforeButNotAnymore->get(node->getFirstChild()->getLocalIndex()) &&
                     _visitedNodes->get(node->getFirstChild()->getGlobalIndex())) ||
                    !_info[block->getNumber()]._downwardExposedAnalysisInfo->get(node->getFirstChild()->getLocalIndex()))))
                downwardExposed = false;

            if (downwardExposed)
               {
               _info[block->getNumber()]._downwardExposedStoreAnalysisInfo->set(node->getLocalIndex());
               if (trace())
                  traceMsg(comp(), "\n11Store Definition #%d is computed in block_%d\n",node->getLocalIndex(),block->getNumber());

               _info[block->getNumber()]._downwardExposedAnalysisInfo->set(node->getLocalIndex());
               if (trace())
                  traceMsg(comp(), "\n11Definition #%d is computed in block_%d\n",node->getLocalIndex(),block->getNumber());
               }
            }

         if (isCurrentTreeTopAnticipatable)
            {
            if (!killedExpressions->get(node->getLocalIndex()) ||
                // if store appears in the block and is in its localTransparency set,
                // it has to be locally anticipatable.
                _localTransparency->getAnalysisInfo(block->getNumber())->get(node->getLocalIndex()))
               {
               _info[block->getNumber()]._analysisInfo->set(node->getLocalIndex());
               if (trace())
                   traceMsg(comp(), "\n11Definition #%d is locally anticipatable in block_%d\n",node->getLocalIndex(),block->getNumber());
               }
            }
         else if (storeTreeParticipatesInAnalysis)
            {
            _info[block->getNumber()]._analysisInfo->reset(node->getLocalIndex());
            killedExpressions->set(node->getLocalIndex());
            if (trace())
               traceMsg(comp(), "\n11Definition #%d is NOT locally anticipatable in block_%d\n",node->getLocalIndex(),block->getNumber());
            }

         // Above we updated anticipatability info for the store; now we will
         // do so for the null check if there is one in this tree
         //
         if (_isNullCheckTree)
            {
            bool isCurrentTreeTopAnticipatable = true;
            // If any symbol used in the null check reference subtree is defined
            // in this block, then NOT anticipatable
            //
            if (!allSymbolReferencesInNullCheckReference->isEmpty())
               {
               tempContainer->empty();
               *tempContainer |= *seenDefinedSymbolReferences;
               *tempContainer -= *_seenCallSymbolReferences;
               *tempContainer &= *allSymbolReferencesInNullCheckReference;
               if (!tempContainer->isEmpty())
                  isCurrentTreeTopAnticipatable = false;
               }

            TR::Node *nullCheckReference = treeTopNode->getNullCheckReference();
            if ((nullCheckReference->getLocalIndex() != MAX_SCOUNT) && (nullCheckReference->getLocalIndex() != 0))
               {
               tempContainer->empty();
               *tempContainer |= *seenStoredSymbolReferences;
               *tempContainer -= *_seenCallSymbolReferences;
               *tempContainer &= *allSymbolReferencesInNullCheckReference;
               if (!tempContainer->isEmpty())
                  {
                  if (!storeNodes->get(nullCheckReference->getLocalIndex()))
                     isCurrentTreeTopAnticipatable = false;
                  }

               if (killedExpressions->get(nullCheckReference->getLocalIndex()))
                  isCurrentTreeTopAnticipatable = false;
               }
            else
               {
               if (!nullCheckReference->getOpCode().isLoadConst())
                   isCurrentTreeTopAnticipatable = false;
               }

            bool downwardExposed = true;
            if (((nullCheckReference->getLocalIndex() != MAX_SCOUNT) && (nullCheckReference->getLocalIndex() != 0)) &&
                ((_downwardExposedBeforeButNotAnymore->get(nullCheckReference->getLocalIndex()) && _visitedNodes->get(nullCheckReference->getGlobalIndex())) ||
                 _notDownwardExposed->get(nullCheckReference->getLocalIndex()) ||
                 !_info[block->getNumber()]._downwardExposedAnalysisInfo->get(nullCheckReference->getLocalIndex())))
               downwardExposed = false;

            if (downwardExposed && !_downwardExposedBeforeButNotAnymore->get(treeTopNode->getLocalIndex()) && !_notDownwardExposed->get(node->getLocalIndex()))
               {
               _info[block->getNumber()]._downwardExposedAnalysisInfo->set(treeTopNode->getLocalIndex());
               if (trace())
                  traceMsg(comp(), "\n11Definition #%d is computed in block_%d\n",node->getLocalIndex(),block->getNumber());
               }

            if (isCurrentTreeTopAnticipatable)
               {
               if (!killedExpressions->get(treeTopNode->getLocalIndex()))
                  {
                  _info[block->getNumber()]._analysisInfo->set(treeTopNode->getLocalIndex());
                  if (trace())
                     traceMsg(comp(), "\n11Definition #%d is locally anticipatable in block_%d\n",treeTopNode->getLocalIndex(),block->getNumber());
                  }
               }
            else
               {
               killedExpressions->set(treeTopNode->getLocalIndex());
               _info[block->getNumber()]._analysisInfo->reset(treeTopNode->getLocalIndex());
               if (trace())
                  traceMsg(comp(), "\n11Definition #%d is NOT locally anticipatable in block_%d\n",treeTopNode->getLocalIndex(),block->getNumber());
               }
            }
         }
      else if (_checkTree)
         {
         TR::Node *node = currentTree->getNode();
         ContainerType *symbolReferencesInCheck;
         if (!_isNullCheckTree)
            symbolReferencesInCheck = allSymbolReferences;
         else
            symbolReferencesInCheck = allSymbolReferencesInNullCheckReference;

         // If any symbol used in the check is defined in this block
         // then NOT anticipatable
         //
         bool isCurrentTreeTopAnticipatable = true;

         //dumpOptDetails(comp(), "\nFor Check Node %p with local index %d\n", node, node->getLocalIndex());
         tempContainer->empty();
         *tempContainer |= *seenDefinedSymbolReferences;
         *tempContainer -= *_seenCallSymbolReferences;
         *tempContainer &= *symbolReferencesInCheck;

         if (!tempContainer->isEmpty())
            isCurrentTreeTopAnticipatable = false;
         else
            {
            if (!_isNullCheckTree)
               {
               int32_t childNum;
               for (childNum=0; childNum < node->getNumChildren(); childNum++)
                  {
                  TR::Node *child = node->getChild(childNum);
                  if ((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0))
                     {
                     if (killedExpressions->get(child->getLocalIndex()))
                        {
                        isCurrentTreeTopAnticipatable = false;
                        break;
                        }
                     }
                  else
                     {
                     if (!child->getOpCode().isLoadConst() &&
                         (child->getOpCodeValue() != TR::PassThrough))
                        {
                         isCurrentTreeTopAnticipatable = false;
                        break;
                        }
                     }
                  }
               }
            else
               {
               TR::Node *nullCheckReference = node->getNullCheckReference();
               if ((nullCheckReference->getLocalIndex() != MAX_SCOUNT) && (nullCheckReference->getLocalIndex() != 0))
                   {
                   if (killedExpressions->get(nullCheckReference->getLocalIndex()))
                      isCurrentTreeTopAnticipatable = false;
                   }
                else if (!nullCheckReference->getOpCode().isLoadConst())
                  isCurrentTreeTopAnticipatable = false;
               }
            }

         bool downwardExposed;
         if (!_isNullCheckTree)
            {
            downwardExposed = true;
            int32_t childNum;
            for (childNum=0;childNum < node->getNumChildren();childNum++)
               {
               TR::Node *child = node->getChild(childNum);
               bool childIsDownwardExposed = true;
               if ((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0))
                   {
                   if ((_downwardExposedBeforeButNotAnymore->get(child->getLocalIndex()) && _visitedNodes->get(child->getGlobalIndex())) ||
                       _notDownwardExposed->get(child->getLocalIndex()) ||
                       !_info[block->getNumber()]._downwardExposedAnalysisInfo->get(child->getLocalIndex()))
                      childIsDownwardExposed = false;
                   }

               if (!childIsDownwardExposed)
                  {
                  downwardExposed = false;
                  break;
                  }
               }
            }
         else
            {
            downwardExposed = true;
            TR::Node *nullCheckReference = node->getNullCheckReference();
            if (((nullCheckReference->getLocalIndex() != MAX_SCOUNT) && (nullCheckReference->getLocalIndex() != 0)) &&
                ((_downwardExposedBeforeButNotAnymore->get(nullCheckReference->getLocalIndex()) && _visitedNodes->get(nullCheckReference->getGlobalIndex())) ||
                 _notDownwardExposed->get(nullCheckReference->getLocalIndex()) ||
                 !_info[block->getNumber()]._downwardExposedAnalysisInfo->get(nullCheckReference->getLocalIndex())))
               downwardExposed = false;
            }

         if (downwardExposed && !_downwardExposedBeforeButNotAnymore->get(node->getLocalIndex()) && !_notDownwardExposed->get(node->getLocalIndex()))
            {
            _info[block->getNumber()]._downwardExposedAnalysisInfo->set(node->getLocalIndex());
            if (trace())
               traceMsg(comp(), "\n11Definition #%d is seen in block_%d\n",node->getLocalIndex(),block->getNumber());
            }

         if (isCurrentTreeTopAnticipatable)
            {
            if (!killedExpressions->get(node->getLocalIndex()))
               {
               _info[block->getNumber()]._analysisInfo->set(node->getLocalIndex());
               if (trace())
                  traceMsg(comp(), "\n22Definition #%d is locally anticipatable in block_%d\n",node->getLocalIndex(),block->getNumber());
               }
            }
         else
            {
            if (trace())
               traceMsg(comp(), "\n22Definition #%d is NOT locally anticipatable in block_%d\n",node->getLocalIndex(),block->getNumber());
            killedExpressions->set(node->getLocalIndex());
            _info[block->getNumber()]._analysisInfo->reset(node->getLocalIndex());
            }
         }

      // Continue walk to update info about uses and defs seen in this block
      //
      updateUsesAndDefs(currentTree->getNode(), block, seenDefinedSymbolReferences, seenStoredSymbolReferences, seenUsedSymbolReferences, tempContainer, temp, storeNodes, visitCount2);

      if (!(currentTree == exitTree))
         currentTree = currentTree->getNextTreeTop();

      *_visitedNodes |= *_visitedNodesAfterThisTree;
      }


   int32_t i = 0;
   TR::Node **supportedNodesAsArray = _lainfo._supportedNodesAsArray;
   while (i < getNumNodes())
      {
      TR::Node *node = supportedNodesAsArray[i];
      if (node && _info[block->getNumber()]._analysisInfo->get(node->getLocalIndex()))
         {
         int32_t numChildren = node->getNumChildren();
         int32_t j = 0;
         while (j < numChildren)
	        {
	        TR::Node *child = node->getChild(j);
	        if ((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0))
 	           {
	           if (killedExpressions->get(child->getLocalIndex()) &&
                   // see comment about localTransparency above
                   !_localTransparency->getAnalysisInfo(block->getNumber())->get(node->getLocalIndex()))
                  {
                  if (trace())
                     traceMsg(comp(), "\n55Definition #%d is NOT locally anticipatable in block_%d\n",node->getLocalIndex(),block->getNumber());
                  killedExpressions->set(node->getLocalIndex());
                  _info[block->getNumber()]._analysisInfo->reset(node->getLocalIndex());
                  break;
   	              }
               }
               j++;
            }
         }
      i++;
      }

   _info[block->getNumber()]._analysisInfo->reset(0);

   }

bool TR_LocalAnticipatability::updateAnticipatabilityForSupportedNodes(TR::Node *node, ContainerType *seenDefinedSymbolReferences, ContainerType *seenStoredSymbolReferences, TR::Block *block, ContainerType *killedExpressions, ContainerType *allSymbolReferences, ContainerType *allSymbolReferencesInNullCheckReference, TR_BitVector *allSymbolReferencesInStore, ContainerType *storeNodes, vcount_t visitCount)
   {
   TR::ILOpCode &opCode = node->getOpCode();

   if (visitCount <= node->getVisitCount())
      {
      if ((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0) &&
          !(opCode.isStore() || opCode.isCheck()))
         {
         if (opCode.hasSymbolReference()  &&
             (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
            {
            if (seenStoredSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
               return false;
            }

         if (!_info[block->getNumber()]._analysisInfo->get(node->getLocalIndex()))
            return false;
         }
      else
         {
         if (opCode.isLoad() || node->getOpCodeValue() == TR::loadaddr)
            {
            if (opCode.hasSymbolReference() &&
                (loadaddrAsLoad() || node->getOpCodeValue() != TR::loadaddr))
               {
               if (seenDefinedSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
                  return false;

               if (seenStoredSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
                  return false;
               }
            }
         else if (node->getOpCode().isTwoChildrenAddress())
            {
            TR::Node *firstChild = node->getFirstChild();
            TR::Node *secondChild = node->getSecondChild();
            if (!adjustInfoForAddressAdd(node, firstChild, seenDefinedSymbolReferences, seenStoredSymbolReferences, killedExpressions, storeNodes, block))
               return false;
            if (!adjustInfoForAddressAdd(node, secondChild, seenDefinedSymbolReferences, seenStoredSymbolReferences, killedExpressions, storeNodes, block))
               return false;
            }
         else
            return false;
         }

      return true;
      }

   node->setVisitCount(visitCount);
   _visitedNodesAfterThisTree->set(node->getGlobalIndex());

   if (_isNullCheckTree && node == _checkTree->getNode()->getNullCheckReference())
      {
      _inNullCheckReferenceSubtree = true;
      }

   bool flag = true;

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);

      if (_isStoreTree)
         {
         if (node->getOpCode().isStore())
            {
            if ((i == 0) && node->getOpCode().isIndirect())
               _inStoreLhsTree = true;
            else
               _inStoreLhsTree = false;
            }
         }

      if (!updateAnticipatabilityForSupportedNodes(child, seenDefinedSymbolReferences, seenStoredSymbolReferences, block, killedExpressions, allSymbolReferences, allSymbolReferencesInNullCheckReference, allSymbolReferencesInStore, storeNodes, visitCount))
         flag = false;
      }

   if (_isStoreTree && node->getOpCode().isStore())
      {
      _inStoreLhsTree = true;
      }

   if (opCode.hasSymbolReference() &&
       (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
      {
      //TR::SymbolReference *symRef = node->getSymbolReference();

      TR::SparseBitVector aliases(comp()->allocator());
      node->mayKill(true).getAliases(aliases);
      if (!aliases.IsZero())
         {
         *allSymbolReferences |= aliases;
         if (_inNullCheckReferenceSubtree)
            *allSymbolReferencesInNullCheckReference |= aliases;
         if (_inStoreLhsTree)
             *allSymbolReferencesInStore |= aliases;
         }
      else
         {
         allSymbolReferences->set(node->getSymbolReference()->getReferenceNumber());
         if (_inNullCheckReferenceSubtree)
            allSymbolReferencesInNullCheckReference->set(node->getSymbolReference()->getReferenceNumber());

         if (_inStoreLhsTree)
            allSymbolReferencesInStore->set(node->getSymbolReference()->getReferenceNumber());
         }
      }

   if (_isNullCheckTree)
      {
      if (node == _checkTree->getNode()->getNullCheckReference())
         _inNullCheckReferenceSubtree = false;
      }

   if (_isStoreTree)
      {
      if (opCode.isStore())
         _inStoreLhsTree = false;
      }

   bool flagToBeReturned = true;
   if ((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0) &&
       !(opCode.isStore() || opCode.isCheck()))
      {
      bool isCurrentTreeTopAnticipatable = true;

      if (opCode.hasSymbolReference() &&
          (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
         {
         if (seenDefinedSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
            isCurrentTreeTopAnticipatable = false;

         if (seenStoredSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
             {
             if (!storeNodes->get(node->getLocalIndex()))
                isCurrentTreeTopAnticipatable = false;
             flagToBeReturned = false;
             }
         }

      bool downwardExposed = true;
      int32_t childNum;
      for (childNum=0; childNum < node->getNumChildren(); childNum++)
         {
         TR::Node *child = node->getChild(childNum);
         bool childIsDownwardExposed = true;
         if ((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0))
            {
            if ((_downwardExposedBeforeButNotAnymore->get(child->getLocalIndex()) && _visitedNodes->get(child->getGlobalIndex())) ||
                _notDownwardExposed->get(child->getLocalIndex()) ||
                !_info[block->getNumber()]._downwardExposedAnalysisInfo->get(child->getLocalIndex()))
                childIsDownwardExposed = false;
            }

         if (!childIsDownwardExposed)
            {
            downwardExposed = false;
            break;
            }
         }

      if (downwardExposed && !_downwardExposedBeforeButNotAnymore->get(node->getLocalIndex()) && !_notDownwardExposed->get(node->getLocalIndex()))
         {
         _info[block->getNumber()]._downwardExposedAnalysisInfo->set(node->getLocalIndex());
         if (trace())
            traceMsg(comp(), "\n33Definition #%d is seen in block_%d\n",node->getLocalIndex(),block->getNumber());
         }

      if (isCurrentTreeTopAnticipatable)
         {
         if ((!killedExpressions->get(node->getLocalIndex())) && flag)
            {
            _info[block->getNumber()]._analysisInfo->set(node->getLocalIndex());
            if (trace())
               traceMsg(comp(), "\n33Definition #%d is locally anticipatable in block_%d\n", node->getLocalIndex(),block->getNumber());
            }
         else if (!flag)
            {
            killedExpressions->set(node->getLocalIndex());
            if (trace())
               traceMsg(comp(), "\n330Definition #%d is NOT locally anticipatable in block_%d\n", node->getLocalIndex(),block->getNumber());
            _info[block->getNumber()]._analysisInfo->reset(node->getLocalIndex());
            }
         }
      else
         {
         flag = false;
         killedExpressions->set(node->getLocalIndex());
         if (trace())
            traceMsg(comp(), "\n331Definition #%d is NOT locally anticipatable in block_%d\n", node->getLocalIndex(),block->getNumber());
         _info[block->getNumber()]._analysisInfo->reset(node->getLocalIndex());
         }
      }
   else
      {
      if (opCode.isLoad() || opCode.getOpCodeValue() == TR::loadaddr)
         {
         if (opCode.hasSymbolReference() &&
             (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
            {
            if (seenDefinedSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
                return false;
            if (seenStoredSymbolReferences->get(node->getSymbolReference()->getReferenceNumber()))
               {
               if (!storeNodes->get(node->getLocalIndex()))
                  return false;
               flagToBeReturned = false;
               }
            }
         }
      else if (opCode.isTwoChildrenAddress())
         {
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();
         if (!adjustInfoForAddressAdd(node, firstChild, seenDefinedSymbolReferences, seenStoredSymbolReferences, killedExpressions, storeNodes, block))
            return false;
         if (!adjustInfoForAddressAdd(node, secondChild, seenDefinedSymbolReferences, seenStoredSymbolReferences, killedExpressions, storeNodes, block))
            return false;
         }
      else
         return false;
      }

   if (!flagToBeReturned)
      return false;
   return flag;
   }


/**
  *For both TR_Aiadd and TR_Aladd
  */
bool TR_LocalAnticipatability::adjustInfoForAddressAdd(TR::Node *node, TR::Node *child, ContainerType *seenDefinedSymbolReferences, ContainerType *seenStoredSymbolReferences, ContainerType *killedExpressions, ContainerType *storeNodes, TR::Block *block)
   {
   bool childHasSupportedOpCode = false;

   TR::ILOpCode &childOpCode = child->getOpCode();

   if ((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0) &&
       !(childOpCode.isStore() || childOpCode.isCheck()))
      childHasSupportedOpCode = true;

   if (childHasSupportedOpCode)
      {
      if (killedExpressions->get(child->getLocalIndex()))
         {
         if (trace())
            (TR::Compiler->target.is64Bit()
             ) ?
               traceMsg(comp(), "\n330Definition #%d (aladd) is NOT locally anticipatable in block_%d because of child\n", node->getLocalIndex(),block->getNumber())
               : traceMsg(comp(), "\n330Definition #%d (aiadd) is NOT locally anticipatable in block_%d because of child\n", node->getLocalIndex(),block->getNumber());
         return false;
         }
      }
   else
      {
      if (childOpCode.isLoad() || child->getOpCodeValue() == TR::loadaddr)
         {
         if (childOpCode.hasSymbolReference() &&
             (loadaddrAsLoad() || child->getOpCodeValue() != TR::loadaddr))
            {
            if (seenDefinedSymbolReferences->get(child->getSymbolReference()->getReferenceNumber()) ||
                (seenStoredSymbolReferences->get(child->getSymbolReference()->getReferenceNumber()) &&
                 ((child->getLocalIndex() == MAX_SCOUNT) ||
                  (child->getLocalIndex() == 0) ||
                  !storeNodes->get(child->getLocalIndex()))))
               {
               if (trace())
                  (TR::Compiler->target.is64Bit()
                   ) ?
                  traceMsg(comp(), "\n330Definition #%d (aladd) is NOT locally anticipatable in block_%d because of child\n", node->getLocalIndex(),block->getNumber())
                  : traceMsg(comp(), "\n330Definition #%d (aiadd) is NOT locally anticipatable in block_%d because of child\n", node->getLocalIndex(),block->getNumber());
               return false;
               }
            }
         }
      else
         return false;
      }

   return true;
   }



void TR_LocalAnticipatability::updateUsesAndDefs(TR::Node *node, TR::Block *block, ContainerType *seenDefinedSymbolReferences, ContainerType *seenStoredSymbolReferences, ContainerType *seenUsedSymbolReferences, ContainerType *tempContainer, TR_BitVector *temp, ContainerType *storeNodes, vcount_t visitCount)
   {
   if (visitCount <= node->getVisitCount())
      return;

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.hasSymbolReference()  &&
       (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
      {
      TR::SymbolReference *symReference = node->getSymbolReference();

      if (opCode.isResolveCheck())
         {
         TR::SymbolReference *childSymRef = node->getFirstChild()->getSymbolReference();


         TR_NodeKillAliasSetInterface aliasSet = node->getFirstChild()->mayKill(true);
         if (!aliasSet.isZero(comp()))
            {
            temp->empty();
#if FLEX_PRE
            aliasSet.getAliasesAndUnionWith(*temp);
            *tempContainer = *temp;
#else
            aliasSet.getAliasesAndUnionWith(*tempContainer);
#endif
            *tempContainer -= *_seenCallSymbolReferences;
            *tempContainer -= *getCheckSymbolReferences();
            *seenDefinedSymbolReferences |= *tempContainer;
            killDownwardExposedExprs(block, tempContainer, node->getFirstChild());
            }
         }

      if ((opCode.isLoadVar() || (loadaddrAsLoad() && opCode.getOpCodeValue() == TR::loadaddr)) &&
           !node->mightHaveVolatileSymbolReference())
         {
#if FLEX_PRE
         node->mayKill(false, true).getAliasesAndUnionWith(*temp);
         *seenUsedSymbolReferences = *temp;
#else
         node->mayKill(true).getAliasesAndUnionWith(*seenUsedSymbolReferences);
#endif
         seenUsedSymbolReferences->set(symReference->getReferenceNumber()); // in case non-reflexive
         }
      else
         {

         if (!opCode.isCheck() && !opCode.isStore())
            {
            TR_NodeKillAliasSetInterface useDefAliases = node->mayKill(true);
            if (!useDefAliases.isZero(comp()))
               {
#if FLEX_PRE
               temp->empty();
               useDefAliases.getAliasesAndUnionWith(*temp);
               *tempContainer = *temp;
#else
               tempContainer->empty();
               useDefAliases.getAliasesAndUnionWith(*tempContainer);
#endif

               *tempContainer -= *_seenCallSymbolReferences;
               *tempContainer -= *getCheckSymbolReferences();
               *seenDefinedSymbolReferences |= *tempContainer;
               killDownwardExposedExprs(block, tempContainer, node);
               }
            }

         if (opCode.isStore())
            {
            // Only perform privatization of stores to fields/statics
            // and that too only when registers are available as it is not
            // guaranteed to be win otherwise, especially on IA32 where a
            // temp store/load is quite likely as opposed to PPC where a
            // reg store/load is likely.
            //
            if (_registersScarce ||
                (node->getSymbolReference() == comp()->getSymRefTab()->findVftSymbolRef()) ||
                node->getSymbolReference()->getSymbol()->isAutoOrParm() ||
                symReference->sharesSymbol() ||
                seenStoredSymbolReferences->get(symReference->getReferenceNumber()))
               {
               if (!seenStoredSymbolReferences->get(symReference->getReferenceNumber()))
                  {
                  if (node->getLocalIndex() != MAX_SCOUNT)
                     storeNodes->set(node->getLocalIndex());
                  seenStoredSymbolReferences->set(symReference->getReferenceNumber());
                  }
               else
                  {
                  if (!storeNodes->get(node->getLocalIndex()))
                     seenDefinedSymbolReferences->set(symReference->getReferenceNumber());
                  }

               TR_NodeKillAliasSetInterface aliases = node->mayKill(true);
               if (!aliases.isZero(comp()))
                  {
                  bool bitAlreadySet = false;
                  if (seenDefinedSymbolReferences->get(symReference->getReferenceNumber()))
                     bitAlreadySet = true;

                 temp->empty();
                 aliases.getAliasesAndUnionWith(*temp);
                 *seenDefinedSymbolReferences |= *temp;
                 killDownwardExposedExprs(block, temp, node);

                 if (!bitAlreadySet)
                    seenDefinedSymbolReferences->reset(symReference->getReferenceNumber());
                  }
               }
            else
               {
               storeNodes->set(node->getLocalIndex());
               seenStoredSymbolReferences->set(symReference->getReferenceNumber());
               killDownwardExposedExprs(block, node);
               }
            }
         }
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateUsesAndDefs(node->getChild(i), block, seenDefinedSymbolReferences, seenStoredSymbolReferences, seenUsedSymbolReferences, tempContainer, temp, storeNodes, visitCount);
   }


void TR_LocalAnticipatability::killDownwardExposedExprs(TR::Block *block, ContainerType *tempContainer, TR::Node *node)
   {
   ContainerType::Cursor iter(*tempContainer);
   bool nodeBitShouldBeOn = false;
   bool storeNodeBitShouldBeOn = false;
   if (node && (node->getLocalIndex() != MAX_SCOUNT))
      {
      if (_info[block->getNumber()]._downwardExposedAnalysisInfo->get(node->getLocalIndex()))
         nodeBitShouldBeOn = true;

      if (_info[block->getNumber()]._downwardExposedStoreAnalysisInfo->get(node->getLocalIndex()))
         storeNodeBitShouldBeOn = true;
      }

   *_temp = *(_info[block->getNumber()]._downwardExposedAnalysisInfo);

   for (iter.SetToFirstOne(); iter.Valid(); iter.SetToNextOne())
      {
      int32_t nextDefinedSymbolReference = iter;
      ContainerType *tinfo = _localTransparency->getTransparencyInfo(nextDefinedSymbolReference);
      *(_info[block->getNumber()]._downwardExposedAnalysisInfo) &= *tinfo;
      *(_info[block->getNumber()]._downwardExposedStoreAnalysisInfo) &= *tinfo;
      }

   if (nodeBitShouldBeOn)
     _info[block->getNumber()]._downwardExposedAnalysisInfo->set(node->getLocalIndex());

   if (storeNodeBitShouldBeOn)
      _info[block->getNumber()]._downwardExposedStoreAnalysisInfo->set(node->getLocalIndex());

   *_temp -= *(_info[block->getNumber()]._downwardExposedAnalysisInfo);
   *_downwardExposedBeforeButNotAnymore |= *_temp;

   }


void TR_LocalAnticipatability::killDownwardExposedExprs(TR::Block *block, TR::Node *node)
   {
   bool nodeBitShouldBeOn = false;
   bool storeNodeBitShouldBeOn = false;
   if (node && (node->getLocalIndex() != MAX_SCOUNT))
      {
      if (_info[block->getNumber()]._downwardExposedAnalysisInfo->get(node->getLocalIndex()))
         nodeBitShouldBeOn = true;

      if (_info[block->getNumber()]._downwardExposedStoreAnalysisInfo->get(node->getLocalIndex()))
          storeNodeBitShouldBeOn = true;
      }

   *_temp = *(_info[block->getNumber()]._downwardExposedAnalysisInfo);

   if (node->getOpCode().hasSymbolReference())
      {
      ContainerType *tinfo = _localTransparency->getTransparencyInfo(node->getSymbolReference()->getReferenceNumber());
      *(_info[block->getNumber()]._downwardExposedAnalysisInfo) &= *tinfo;
      *(_info[block->getNumber()]._downwardExposedStoreAnalysisInfo) &= *tinfo;
      }

   if (nodeBitShouldBeOn)
      _info[block->getNumber()]._downwardExposedAnalysisInfo->set(node->getLocalIndex());

   if (storeNodeBitShouldBeOn)
      _info[block->getNumber()]._downwardExposedStoreAnalysisInfo->set(node->getLocalIndex());

   *_temp -= *(_info[block->getNumber()]._downwardExposedAnalysisInfo);
   *_downwardExposedBeforeButNotAnymore |= *_temp;
   }
