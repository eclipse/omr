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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/LocalAnalysis.hpp"

TR_LocalTransparency::TR_LocalTransparency(TR_LocalAnalysisInfo &info, bool t)
   : TR_LocalAnalysis(info, t)
   {
   if (trace())
      traceMsg(comp(), "Starting LocalTransparency\n");

   static const char *e = feGetEnv("TR_loadaddrAsLoad");
   _loadaddrAsLoad = e ? (atoi(e) != 0) : true;

   initializeLocalAnalysis(false, false);

   // Put in this code to guard against going over
   // the visitCount size in the process of incrementing.
   //
   if (comp()->getVisitCount() > 8000)
      comp()->resetVisitCounts(1);

   int32_t symRefCount = comp()->getMaxAliasIndex();
   int32_t numberOfNodes = comp()->getFlowGraph()->getNextNodeNumber();


   // _transparencyInfo contains for each symbol, which expressions are
   // dependent on it. This info is used if a symbol S is written in a block
   // then we can easily obtain the expressions not locally transparent in the
   // block
   //
   _transparencyInfo = (ContainerType**)trMemory()->allocateStackMemory(symRefCount*sizeof(ContainerType*));
   memset(_transparencyInfo, 0, symRefCount*sizeof(ContainerType *));

   int32_t i;
   for (i = 0; i < symRefCount; i++)
      {
      _transparencyInfo[i] = allocateContainer(getNumNodes());
      _transparencyInfo[i]->setAll(getNumNodes());
      }


   /*
    * Caution : we want _transparencyInfo to be around after this routine returns;
    * so we should consider this when moving the stack memory region's scope earlier
    */
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _supportedNodes = allocateContainer(getNumNodes());
   _supportedNodes->setAll(getNumNodes());

    // Used to indicate which expressions are indirect stores
    //
   ContainerType *survivingStoreNodes = allocateContainer(getNumNodes());
   ContainerType *storeNodesInThisBlock = allocateContainer(getNumNodes());

   // This bit vector denotes whether we already have the transparency
   // information when a certain symbol is written. Even though transparency
   // information is stored for each block separately, there is a global
   // dimension about local transparency (look at the formula in Muchnick);
   // the question is what are all the expressions in this ENTIRE method
   // that are transparent in this block ? Since an expression is transparent or
   // not based on whether any of its symbols have been written or not in
   // the block, storing the effect of symbols written on expressions in the
   // entire method would save us considerable effort if the same symbol is
   // written in multiple blocks.
   //
   _hasTransparencyInfoFor = allocateContainer(symRefCount);
   ContainerType *allStoredSymRefsInMethod = allocateContainer(symRefCount);

   ContainerType **definedSymbolReferencesInBlock = (ContainerType **)trMemory()->allocateStackMemory(numberOfNodes*sizeof(ContainerType *));
   memset(definedSymbolReferencesInBlock, 0, numberOfNodes*sizeof(ContainerType *));
   ContainerType **storedSymRefsInBlock = (ContainerType **)trMemory()->allocateStackMemory(numberOfNodes*sizeof(ContainerType *));
   memset(storedSymRefsInBlock, 0, numberOfNodes*sizeof(ContainerType *));
   ContainerType **symRefsDefinedAfterStoredInBlock = (ContainerType **)trMemory()->allocateStackMemory(numberOfNodes*sizeof(ContainerType *));
   memset(symRefsDefinedAfterStoredInBlock, 0, numberOfNodes*sizeof(ContainerType *));
   ContainerType **symRefsUsedAfterDefinedInBlock = (ContainerType **)trMemory()->allocateStackMemory(numberOfNodes*sizeof(ContainerType *));
   memset(symRefsUsedAfterDefinedInBlock, 0, numberOfNodes*sizeof(ContainerType *));


   //traceMsg(comp(), "sym ref count = %d\n", symRefCount);

   for (i=0;i<numberOfNodes;i++)
      {
      definedSymbolReferencesInBlock[i] = allocateContainer(symRefCount);
      storedSymRefsInBlock[i] = allocateContainer(symRefCount);
      symRefsDefinedAfterStoredInBlock[i] = allocateContainer(symRefCount);
      symRefsUsedAfterDefinedInBlock[i] = allocateContainer(symRefCount);
      }

   ContainerType *globalDefinedSymbolReferences = allocateContainer(symRefCount);
   ContainerType *tempContainer = allocateContainer(comp()->getMaxAliasIndex());
   TR_BitVector *temp = new (trStackMemory()) TR_BitVector(comp()->getMaxAliasIndex(), trMemory(), stackAlloc);

   // First pass over trees to collect symbols that are stored/defined
   // in each block and in the method as a whole
   //
   vcount_t startVisitCount = comp()->incVisitCount();
   TR::TreeTop *currentTree = comp()->getStartTree();
   int32_t blockNumber = -1;
   while (currentTree)
      {
      TR::ILOpCode &firstOpCodeInTree = currentTree->getNode()->getOpCode();
      if (firstOpCodeInTree.isCheck())
         _checkTree = currentTree;
      else if (firstOpCodeInTree.getOpCodeValue() == TR::BBStart)
          blockNumber = currentTree->getNode()->getBlock()->getNumber();
      else if (firstOpCodeInTree.getOpCodeValue() == TR::BBEnd)
          *_hasTransparencyInfoFor |= *(storedSymRefsInBlock[blockNumber]);

      updateUsesAndDefs(currentTree->getNode(), globalDefinedSymbolReferences, definedSymbolReferencesInBlock[blockNumber], storedSymRefsInBlock[blockNumber], symRefsDefinedAfterStoredInBlock[blockNumber], symRefsUsedAfterDefinedInBlock[blockNumber], startVisitCount, tempContainer, temp, allStoredSymRefsInMethod);

      currentTree = currentTree->getNextTreeTop();
      }

   // Second pass over the trees; to correspond symbol info collected
   // in first pass with the nodes killed info.
   // For each newly defined symbol in this block, look through ALL the
   // trees in this method and compute the transparency information if
   // that symbol is written. Note that this computation is only done
   // once and it is stored to be used afterwards for any block in which the
   // symbol is written.
   //
   ContainerType *allSymbolReferences = allocateContainer(symRefCount);
   ContainerType *allSymbolReferencesInNullCheckReference = allocateContainer(symRefCount);
   ContainerType *allSymbolReferencesInStore = allocateContainer(symRefCount);
   *_hasTransparencyInfoFor |= *globalDefinedSymbolReferences;

   currentTree = comp()->getStartTree();
   bool inBlockBeingAnalyzed = false;
   while (currentTree)
      {
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

      TR::Node *currentNode = currentTree->getNode();
      TR::Node *storeNode = NULL;
      TR::SymbolReference *storeSymRef = NULL;

      if (currentNode->getOpCodeValue() == TR::BBStart)
         blockNumber = currentNode->getBlock()->getNumber();

      TR::ILOpCode &firstOpCodeInTree = currentNode->getOpCode();
      if (firstOpCodeInTree.getOpCodeValue() == TR::NULLCHK)
         {
         _isNullCheckTree = true;
         _checkTree = currentTree;
         if (currentNode->getFirstChild()->getOpCode().isStore())
            {
            _isStoreTree = true;
            if (currentNode->getFirstChild()->getLocalIndex() == MAX_SCOUNT)
	       storeTreeParticipatesInAnalysis = false;
            storeNode = currentNode->getFirstChild();
            }
         }
      else if (firstOpCodeInTree.isResolveCheck())
         {
         _checkTree = currentTree;
         if (currentNode->getFirstChild()->getOpCode().isStore())
            {
            _isStoreTree = true;
            if (currentNode->getFirstChild()->getLocalIndex() == MAX_SCOUNT)
               storeTreeParticipatesInAnalysis = false;
            storeNode = currentNode->getFirstChild();
            }
         }
      else if ((firstOpCodeInTree.getOpCodeValue() == TR::treetop) ||
               (comp()->useAnchors() && firstOpCodeInTree.isAnchor()))
         {
         if (currentNode->getFirstChild()->getOpCode().isStore())
            {
            _isStoreTree = true;
            if (currentNode->getFirstChild()->getLocalIndex() == MAX_SCOUNT)
               storeTreeParticipatesInAnalysis = false;
            storeNode = currentNode->getFirstChild();
            }
         }
      else if (firstOpCodeInTree.isStore())
         {
         _isStoreTree = true;
         if (currentNode->getLocalIndex() == MAX_SCOUNT)
	    storeTreeParticipatesInAnalysis = false;
         storeNode = currentNode;
         }
      else if (firstOpCodeInTree.isCheck())
         _checkTree = currentTree;

      if (_isStoreTree)
        {
        storeSymRef = storeNode->getSymbolReference();
        _storedSymbolReferenceNumber = storeSymRef->getReferenceNumber();
        }

      // Examine nodes that are not stores or checks and
      // kill the expressions that are not transparent because of writes to this symbol.
      //
      vcount_t visitCount3 = comp()->incOrResetVisitCount();
      updateInfoForSupportedNodes(currentNode, allSymbolReferences, allSymbolReferencesInNullCheckReference, allSymbolReferencesInStore, globalDefinedSymbolReferences, storedSymRefsInBlock[blockNumber], allStoredSymRefsInMethod, visitCount3);

      // Enter only if this is a store or a check for which we want
      // to do PRE. We exclude write barriers currently but I'll enable it
      // for write barriers soon.
      //
      if (_isStoreTree)
         {
         // Check if it is a global store; if so important from a field
         // privatization viewpoint
         //
         if (!_registersScarce &&
             storeTreeParticipatesInAnalysis &&
             !storeNode->getSymbolReference()->getSymbol()->isAutoOrParm())
            {
            TR_LocalAnalysisInfo::LAInfo *binfo = _info+(blockNumber);
            if (binfo->_block)
               binfo->_analysisInfo->set(storeNode->getLocalIndex());
            }

         ContainerType::Cursor bvi(*globalDefinedSymbolReferences);
         for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
            {
            int32_t newDefinedSymbolReference = bvi;

            // For each defined symbol (excludes global stores as those
            // are not considered kills because field privatization will ensure
            // that the value is re-loaded into a temp at these global stores.
            //
            if (!allSymbolReferencesInStore->isEmpty() &&
                storeTreeParticipatesInAnalysis)
               {
               if (allSymbolReferencesInStore->get(newDefinedSymbolReference))
                  {
                  _transparencyInfo[newDefinedSymbolReference]->reset(storeNode->getLocalIndex());
                  if (trace())
                     traceMsg(comp(), "75757Expression %d killed (n%dn) by symRef #%d  %d\n", storeNode->getLocalIndex(), storeNode->getGlobalIndex(), newDefinedSymbolReference, true);
                  }
               }

            // Kill the NULLCHK if any of the symbols in its subtree
            // is written.
            //
            if (_isNullCheckTree)
               {
               if (!allSymbolReferencesInNullCheckReference->isEmpty())
                  {

                  if (allSymbolReferencesInNullCheckReference->get(newDefinedSymbolReference))
                     {
                      if (trace())
                         traceMsg(comp(), "76767Expression %d killed (n%dn) by symRef #%d  %d\n", currentNode->getLocalIndex(), currentNode->getGlobalIndex(), newDefinedSymbolReference, true);

                      _transparencyInfo[newDefinedSymbolReference]->reset(currentNode->getLocalIndex());
                     }
                  }
               }
            }
         }
      else if (_checkTree)
         {
         ContainerType::Cursor bvi(*globalDefinedSymbolReferences);
         for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
            {
            int32_t newDefinedSymbolReference = bvi;

            // If it is any general check node without a store as its
            // child, then check if it is transparent or not.
            //
            ContainerType *symbolReferencesInCheck;
            if (!_isNullCheckTree)
               symbolReferencesInCheck = allSymbolReferences;
            else
               symbolReferencesInCheck = allSymbolReferencesInNullCheckReference;

            if (symbolReferencesInCheck->get(newDefinedSymbolReference))
               {
               if (trace())
                  traceMsg(comp(), "777Expression %d killed (n%dn) by symRef #%d  %d\n", currentNode->getLocalIndex(), currentNode->getGlobalIndex(), newDefinedSymbolReference, true);

               _transparencyInfo[newDefinedSymbolReference]->reset(currentNode->getLocalIndex());
               }
            }
         }

      currentTree = currentTree->getNextTreeTop();
      }


   TR::Node **supportedNodesAsArray = _lainfo._supportedNodesAsArray;

   // Now use the defined symbol references to decide which expressions are transparent in this
   // block
   //
   TR::CFGNode *nextNode;
   for (nextNode = comp()->getFlowGraph()->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR::Block *nextBlock = toBlock(nextNode);
      TR_LocalAnalysisInfo::LAInfo *binfo = _info+(nextBlock->getNumber());
      if (binfo->_block == NULL)
         continue;

      *storeNodesInThisBlock = *(binfo->_analysisInfo);
      storeNodesInThisBlock->reset(0);
      binfo->_analysisInfo->setAll(getNumNodes());
      *(_info[nextBlock->getNumber()]._analysisInfo) &= *_supportedNodes;

      if (trace())
         traceMsg(comp(), "\nBeginning to solve for block number : %d\n",nextBlock->getNumber());


      // We need to kill all expressions dependent on a symbol (other than the load/store) being stored into;
      // this is because we cannot compensate for an expression (o.f+1) where o.f is written in the block.
      // We would adjust the load/store of o.f (say with temp t1) but to adjust the value of the temp t2 that
      // corresponds to (o.f+1) is more complicated
      //
      *tempContainer = *storedSymRefsInBlock[nextBlock->getNumber()];
      *tempContainer |= *definedSymbolReferencesInBlock[nextBlock->getNumber()];

      ContainerType::Cursor iter(*tempContainer);
      for (iter.SetToFirstOne(); iter.Valid(); iter.SetToNextOne())
         {
         int32_t nextDefinedSymbolReference = iter;
         if (trace())
            traceMsg(comp(), "\nUsing transparency info for symRef #%d\n",nextDefinedSymbolReference);

         if (_hasTransparencyInfoFor->get(nextDefinedSymbolReference))
            {
            if (trace())
               {
               traceMsg(comp(), "\nMerging with : ");
               _transparencyInfo[nextDefinedSymbolReference]->print(comp());
               traceMsg(comp(), "\nDefined : ");
               definedSymbolReferencesInBlock[nextBlock->getNumber()]->print(comp());
                traceMsg(comp(), "\nDefined after stored : ");
               symRefsDefinedAfterStoredInBlock[nextBlock->getNumber()]->print(comp());
                traceMsg(comp(), "\nUsed after defined : ");
               symRefsUsedAfterDefinedInBlock[nextBlock->getNumber()]->print(comp());
               traceMsg(comp(), "\n");
               }

            survivingStoreNodes->empty();

            // If the value was not really defined by anything other than a call, ONLY the store/load
            // of the stored value (say o.f) can still survive. All other dependent expressions must be killed
            // though. See discussion above.
            //
            if (!definedSymbolReferencesInBlock[nextBlock->getNumber()]->get(nextDefinedSymbolReference) ||
                (!symRefsDefinedAfterStoredInBlock[nextBlock->getNumber()]->get(nextDefinedSymbolReference) &&
                 !symRefsUsedAfterDefinedInBlock[nextBlock->getNumber()]->get(nextDefinedSymbolReference)))
               {
               ContainerType::Cursor bvi(*storeNodesInThisBlock);
               for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
                  {
                  int32_t storeNode = bvi;

                  // If store of this symRef was transparent so far, then keep it transparent;
                  // i.e. do not kill it because of this store as this store is going to be
                  // privatized
                  //
                  if (_info[nextBlock->getNumber()]._analysisInfo->get(storeNode))
                     {
                     int32_t storeSymRefNum = supportedNodesAsArray[storeNode]->getSymbolReference()->getReferenceNumber();

                     bool childKilled = false;
                     if (supportedNodesAsArray[storeNode]->getOpCode().isIndirect())
                        {
                        TR::Node *child = supportedNodesAsArray[storeNode]->getFirstChild();
                        if ((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0))
                           {
                           if (!_transparencyInfo[nextDefinedSymbolReference]->get(child->getLocalIndex()))
                              childKilled = true;
                           }
                        else
                           childKilled = true;
                        }

		     TR_BitVector *aliases = NULL;

                     bool symRefCanSurvive = false;
                     if (storeSymRefNum == nextDefinedSymbolReference)
                        symRefCanSurvive = true;
                     else if (supportedNodesAsArray[storeNode]->mayKill(true).contains(nextDefinedSymbolReference, comp()))
                        {
                           {
                           if (!definedSymbolReferencesInBlock[nextBlock->getNumber()]->get(storeSymRefNum) ||
                               (!symRefsDefinedAfterStoredInBlock[nextBlock->getNumber()]->get(storeSymRefNum) &&
                                !symRefsUsedAfterDefinedInBlock[nextBlock->getNumber()]->get(storeSymRefNum)))
                              symRefCanSurvive = true;
                           }
                        }

                     if (symRefCanSurvive && !childKilled)
                        {
                        if (trace())
                           traceMsg(comp(), "Store node %d survives\n", storeNode);

                        if (survivingStoreNodes->isEmpty())
                           survivingStoreNodes->set(storeNode);
                        else
                           {
                           if (survivingStoreNodes->elementCount() > 1 || !survivingStoreNodes->get(storeNode))
                              {
                              survivingStoreNodes->empty();
                              break;
                              }
                           }
                        }
                     }
                  }
               }

            *(_info[nextBlock->getNumber()]._analysisInfo) &= *(_transparencyInfo[nextDefinedSymbolReference]);
            *(_info[nextBlock->getNumber()]._analysisInfo) |= *survivingStoreNodes;
            _info[nextBlock->getNumber()]._analysisInfo->reset(0);
            }
         }

      if (trace())
          {
          traceMsg(comp(), "\nSolution for block number : %d\n",nextBlock->getNumber());
          _info[nextBlock->getNumber()]._analysisInfo->print(comp());
          }
      }

   } // scope of the stack memory region

   if (trace())
      traceMsg(comp(), "\nEnding LocalTransparency\n");
   }



TR_LocalAnalysisInfo::ContainerType *
TR_LocalTransparency::getTransparencyInfo(int32_t symRefNum)
   {
   return _transparencyInfo[symRefNum];
   }


void TR_LocalTransparency::updateUsesAndDefs(TR::Node *node, ContainerType *globallySeenDefinedSymbolReferences, ContainerType *seenDefinedSymbolReferences, ContainerType *seenStoredSymRefs, ContainerType *symRefsDefinedAfterStored,  ContainerType *symRefsUsedAfterDefined, vcount_t visitCount, ContainerType *tempContainer, TR_BitVector *temp, ContainerType *allStoredSymRefsInMethod)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateUsesAndDefs(node->getChild(i), globallySeenDefinedSymbolReferences, seenDefinedSymbolReferences, seenStoredSymRefs, symRefsDefinedAfterStored, symRefsUsedAfterDefined, visitCount, tempContainer, temp, allStoredSymRefsInMethod);


   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.hasSymbolReference() &&
       (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      int32_t symRefNum = symReference->getReferenceNumber();

      if (opCode.isResolveCheck())
         {
         //TR::SymbolReference *childSymRef = node->getFirstChild()->getSymbolReference();


#if FLEX_PRE
         temp->empty();
         node->getFirstChild()->mayKill(true).getAliasesAndUnionWith(*temp);
         if (!temp->isEmpty())
            {
            *tempContainer = *temp;
#else
         tempContainer->empty();
         node->getFirstChild()->mayKill(true).getAliasesAndUnionWith(*tempContainer);
         if (!tempContainer->isEmpty())
            {
#endif
            *tempContainer -= *getCheckSymbolReferences();
            *seenDefinedSymbolReferences |= *tempContainer;
            *globallySeenDefinedSymbolReferences |= *tempContainer;
            *tempContainer &= *seenStoredSymRefs;
            *symRefsDefinedAfterStored |= *tempContainer;
            }
         }

      bool isCallDirect = false;
      if (node->getOpCode().isCallDirect())
         isCallDirect = true;

      if ((opCode.isLoadVar() || (loadaddrAsLoad() && opCode.getOpCodeValue() == TR::loadaddr)) &&
           !node->mightHaveVolatileSymbolReference())
         {
         if (seenDefinedSymbolReferences->get(symRefNum))
            {
            //traceMsg(comp(), "sym ref set = %d\n", symRefNum);
            symRefsUsedAfterDefined->set(symRefNum);
            }

#if FLEX_PRE
         temp->empty();
         node->mayKill(true).getAliasesAndUnionWith(*temp);

         if (!temp->isEmpty())
            {
            *tempContainer = *temp;
#else
         tempContainer->empty();
         node->mayUse().getAliasesAndUnionWith(*tempContainer);

         if (!tempContainer->isEmpty())
            {
#endif
            *tempContainer -= *getCheckSymbolReferences();
            *tempContainer &= *seenDefinedSymbolReferences;
            *symRefsUsedAfterDefined |= *tempContainer;
            }
         }
      else
         {
         if (!opCode.isCheck() && !opCode.isStore())
            {
#if FLEX_PRE
            temp->empty();
            node->mayKill(true).getAliasesAndUnionWith(*temp);
            if (!temp->isEmpty())
               {
               *tempContainer = *temp;
#else
            tempContainer->empty();
            node->mayKill(true).getAliasesAndUnionWith(*tempContainer);
            if (!tempContainer->isEmpty())
               {
#endif
               *tempContainer -= *getCheckSymbolReferences();

               bool pureFunction =  node->getOpCode().isCall() &&
                                    symReference &&
                                    symReference->getSymbol()->castToMethodSymbol() &&
                                    symReference->getSymbol()->castToMethodSymbol()->isPureFunction();
               if (pureFunction)
                  tempContainer->reset(symRefNum);

               *seenDefinedSymbolReferences |= *tempContainer;
               *globallySeenDefinedSymbolReferences |= *tempContainer;
               *tempContainer &= *seenStoredSymRefs;
               *symRefsDefinedAfterStored |= *tempContainer;
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
                node->getSymbolReference()->getSymbol()->isAutoOrParm() ||
                symReference->sharesSymbol() ||
                seenStoredSymRefs->get(symRefNum))
               {
               if (seenStoredSymRefs->get(symRefNum))  // see RTC_48539
                  {
                  seenDefinedSymbolReferences->set(symRefNum);
                  symRefsDefinedAfterStored->set(symRefNum);
                  }
               }

            globallySeenDefinedSymbolReferences->set(symRefNum);

#if FLEX_PRE
            temp->empty();
            node->mayKill(true).getAliasesAndUnionWith(*temp, comp());

            if (!temp->isEmpty())
               {
               *tempContainer = *temp;
#else
            tempContainer->empty();
            node->mayKill(true).getAliasesAndUnionWith(*tempContainer);

            if (!tempContainer->isEmpty())
               {
#endif
               *globallySeenDefinedSymbolReferences |= *tempContainer;
               tempContainer->reset(symRefNum);
               *seenDefinedSymbolReferences |= *tempContainer;

               // If this is a volatile store, it should kill any store node that has appeared
               // earlier in the block. Otherwise, the store node could survive the block
               // in local transparency but will be killed in _downwardExposedAnalysisInfo
               // when the volatile store is processed in local anticipatability, which causes
               // inconsistency between local transparency and local anticipatability.
               // This matches how a call node is processed earlier.
               if (node->mightHaveVolatileSymbolReference())
                  {
                  *tempContainer &= *seenStoredSymRefs;
                  *symRefsDefinedAfterStored |= *tempContainer;
                  }
               }

            seenStoredSymRefs->set(symRefNum);
            allStoredSymRefsInMethod->set(symRefNum);
            }
         }
      }
   }






void TR_LocalTransparency::updateInfoForSupportedNodes(TR::Node *node, ContainerType *allSymbolReferences, ContainerType *allSymbolReferencesInNullCheckReference, ContainerType *allSymbolReferencesInStore, ContainerType *seenDefinedSymbolReferences, ContainerType *seenStoredSymRefs, ContainerType *allStoredSymRefsInMethod, vcount_t visitCount)
   {
   if (visitCount <= node->getVisitCount())
      return;

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();

   if (_isNullCheckTree)
      {
      if (node == _checkTree->getNode()->getNullCheckReference())
         _inNullCheckReferenceSubtree = true;
      }

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

      updateInfoForSupportedNodes(child, allSymbolReferences, allSymbolReferencesInNullCheckReference, allSymbolReferencesInStore, seenDefinedSymbolReferences, seenStoredSymRefs, allStoredSymRefsInMethod, visitCount);

      if (node->getLocalIndex() != MAX_SCOUNT &&
          node->getLocalIndex() != 0 &&
          !(opCode.isStore() || opCode.isCheck()))
         {
         bool childHasSupportedOpCode = false;
         TR::ILOpCode &childOpCode = child->getOpCode();

         if (child->getLocalIndex() != MAX_SCOUNT &&
             child->getLocalIndex() != 0 &&
             !(childOpCode.isStore() || childOpCode.isCheck()))
            childHasSupportedOpCode = true;

         if (childHasSupportedOpCode)
            {
            if (!_supportedNodes->get(child->getLocalIndex()))
               _supportedNodes->reset(node->getLocalIndex());
            else
               {
               int32_t i;
               for (i=0;i<comp()->getMaxAliasIndex();i++)
                  {
                  if (!_transparencyInfo[i]->get(child->getLocalIndex()) ||
                      (child->getOpCode().hasSymbolReference() &&
                       !child->getOpCode().isCall() &&
                       i == child->getSymbolReference()->getReferenceNumber() &&
                       seenStoredSymRefs->get(child->getSymbolReference()->getReferenceNumber())))
                     {
                     _transparencyInfo[i]->reset(node->getLocalIndex());
                      if (trace())
                         traceMsg(comp(), "Expression %d (n%dn) killed by symRef #%d because child %d (n%dn) is already killed by the symRef\n", node->getLocalIndex(), node->getGlobalIndex(), i, child->getLocalIndex(), child->getGlobalIndex());
                     }
                  }
               }
            }
         else
            {
            if (child->getOpCode().isLoad() || child->getOpCodeValue() == TR::loadaddr)
               {
               if (child->getOpCode().hasSymbolReference() &&
                   (loadaddrAsLoad() || child->getOpCodeValue() != TR::loadaddr))
                  {
                  int32_t childSymRefNum = child->getSymbolReference()->getReferenceNumber();

                  if (seenDefinedSymbolReferences->get(childSymRefNum) ||
                      seenStoredSymRefs->get(childSymRefNum))
                     {
                     _transparencyInfo[childSymRefNum]->reset(node->getLocalIndex());

                     TR::SparseBitVector aliases(comp()->allocator());
                     child->mayKill(true).getAliases(aliases);

                     TR::SparseBitVector::Cursor aliasesCursor(aliases);
                     for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
                        {
                        int32_t nextAlias = aliasesCursor;
                        _transparencyInfo[nextAlias]->reset(node->getLocalIndex());
                        if (trace())
                           traceMsg(comp(), "9999Expression %d (n%dn) killed by symRef #%d  %d\n", node->getLocalIndex(), node->getGlobalIndex(), nextAlias, seenDefinedSymbolReferences->get(nextAlias));

                        }

                if (trace())
                        traceMsg(comp(), "Expression %d (n%dn) killed by symRef #%d (loaded in child)\n", node->getLocalIndex(), node->getGlobalIndex(), childSymRefNum);
                     }
                  }
               }
            else
               {
               if (child->getOpCode().isTwoChildrenAddress())
                  {
                  TR::Node *firstGrandChild = child->getFirstChild();
                  TR::Node *secondGrandChild = child->getSecondChild();
                  adjustInfoForAddressAdd(node, firstGrandChild, seenDefinedSymbolReferences, seenStoredSymRefs);
                  adjustInfoForAddressAdd(node, secondGrandChild, seenDefinedSymbolReferences, seenStoredSymRefs);
                  }
               else
                  {
                  _supportedNodes->reset(node->getLocalIndex());
                  if (trace())
                     traceMsg(comp(), "Expression %d (n%dn) killed (non supported opcode)\n", node->getLocalIndex(), node->getGlobalIndex());
                  }
               }
            }
         }
      }


   if (_isStoreTree)
      {
      if (node->getOpCode().isStore())
         _inStoreLhsTree = true;
      }

   if (opCode.hasSymbolReference() &&
       (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
      {
      TR::SymbolReference *symRef = node->getSymbolReference();

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
         allSymbolReferences->set(symRef->getReferenceNumber());

         if (_inNullCheckReferenceSubtree)
            allSymbolReferencesInNullCheckReference->set(symRef->getReferenceNumber());

         if (_inStoreLhsTree)
            allSymbolReferencesInStore->set(symRef->getReferenceNumber());
         }
      }

   if (_isNullCheckTree)
      {
      if (node == _checkTree->getNode()->getNullCheckReference())
         _inNullCheckReferenceSubtree = false;
      }

   if (_isStoreTree)
      {
      if (node->getOpCode().isStore())
         _inStoreLhsTree = false;
      }

   if (node->getLocalIndex() != MAX_SCOUNT &&
       node->getLocalIndex() != 0 &&
       !(opCode.isStore() || opCode.isCheck()))
      {
      // If any symbol used in this subtree is defined in this block
      // then not transparent
      //
      if (opCode.hasSymbolReference() &&
          (loadaddrAsLoad() || opCode.getOpCodeValue() != TR::loadaddr))
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         if (seenDefinedSymbolReferences->get(symRef->getReferenceNumber()) ||
             seenStoredSymRefs->get(symRef->getReferenceNumber()))
            {
            if (trace())
               traceMsg(comp(), "Expression %d (n%dn) killed by symRef #%d  %d\n", node->getLocalIndex(), node->getGlobalIndex(), symRef->getReferenceNumber(), seenDefinedSymbolReferences->get(symRef->getReferenceNumber()));
            _transparencyInfo[symRef->getReferenceNumber()]->reset(node->getLocalIndex());

            TR::SparseBitVector aliases(comp()->allocator());
            node->mayKill(true).getAliases(aliases);

            TR::SparseBitVector::Cursor aliasesCursor(aliases);
            for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
               {
               int32_t nextAlias = aliasesCursor;
               _transparencyInfo[nextAlias]->reset(node->getLocalIndex());
               if (trace())
                  traceMsg(comp(), "888Expression %d (n%dn) killed by symRef #%d  %d\n", node->getLocalIndex(), node->getGlobalIndex(), nextAlias, seenDefinedSymbolReferences->get(nextAlias));

               }
            }
         }
      }
   }




void TR_LocalTransparency::adjustInfoForAddressAdd(TR::Node *node, TR::Node *child, ContainerType *seenDefinedSymbolReferences, ContainerType *seenStoredSymRefs)
   {
   bool childHasSupportedOpCode = false;

   TR::ILOpCode &childOpCode = child->getOpCode();

   if (((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0)) && (!(childOpCode.isStore() || childOpCode.isCheck())))
      childHasSupportedOpCode = true;

   if (childHasSupportedOpCode)
      {
      if (!_supportedNodes->get(child->getLocalIndex()))
         _supportedNodes->reset(node->getLocalIndex());
      else
         {
         int32_t i;
         for (i=0;i<comp()->getMaxAliasIndex();i++)
            {
            if (!_transparencyInfo[i]->get(child->getLocalIndex()))
               {
               _transparencyInfo[i]->reset(node->getLocalIndex());
               if (trace())
                  {
                  if (comp()->target().is64Bit())
                     traceMsg(comp(), "Expression %d killed by symRef #%d because grandchild (child of aladd) %d is already killed by the symRef\n", node->getLocalIndex(), i, child->getLocalIndex());
                  else
                     traceMsg(comp(), "Expression %d killed by symRef #%d because grandchild (child of aiadd) %d is already killed by the symRef\n", node->getLocalIndex(), i, child->getLocalIndex());
                  }
               }
            }
         }
      }
   else
      {
      if (child->getOpCode().isLoad() || child->getOpCodeValue() == TR::loadaddr)
         {
         if (child->getOpCode().hasSymbolReference() &&
             (loadaddrAsLoad() || child->getOpCodeValue() != TR::loadaddr))
            {
            TR::SymbolReference *childSymRef = child->getSymbolReference();
            if (seenDefinedSymbolReferences->get(childSymRef->getReferenceNumber()) ||
                seenStoredSymRefs->get(childSymRef->getReferenceNumber()))
               {
               _transparencyInfo[childSymRef->getReferenceNumber()]->reset(node->getLocalIndex());

               TR::SparseBitVector aliases(comp()->allocator());
               child->mayKill(true).getAliases(aliases);

               TR::SparseBitVector::Cursor aliasesCursor(aliases);
               for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
                  {
                  int32_t nextAlias = aliasesCursor;
                  _transparencyInfo[nextAlias]->reset(node->getLocalIndex());
                  if (trace())
                     traceMsg(comp(), "999Expression %d (n%dn) killed by symRef #%d  %d\n", node->getLocalIndex(), node->getGlobalIndex(), nextAlias, seenDefinedSymbolReferences->get(nextAlias));

                  }
               if (trace())
                  traceMsg(comp(), "Expression %d killed by symRef #%d (loaded in grandchild)\n", node->getLocalIndex(), childSymRef->getReferenceNumber());
               }
            }
         }
      else
         {
         _supportedNodes->reset(node->getLocalIndex());
         if (trace())
            traceMsg(comp(), "Expression %d killed (non supported opcode)\n", node->getLocalIndex());
         }
      }
   }
