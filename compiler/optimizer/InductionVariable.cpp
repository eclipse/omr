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

#include "optimizer/InductionVariable.hpp"

#include <limits.h>                              // for INT_MAX
#include <stdint.h>                              // for int32_t, int64_t, etc
#include <stdio.h>                               // for printf
#include <string.h>                              // for NULL, memset
#include "codegen/CodeGenerator.hpp"             // for CodeGenerator
#include "codegen/FrontEnd.hpp"                  // for TR_FrontEnd, etc
#include "compile/Compilation.hpp"               // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "cs2/llistof.h"                         // for QueueOf, etc
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                      // for SparseBitVector, etc
#include "env/jittypes.h"                        // for intptrj_t
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                          // for Block, toBlock
#include "il/DataTypes.hpp"                      // for DataTypes::Address, etc
#include "il/ILOpCodes.hpp"                      // for ILOpCodes::lload, etc
#include "il/ILOps.hpp"                          // for ILOpCode, etc
#include "il/Node.hpp"                           // for Node, etc
#include "il/Node_inlines.hpp"                   // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                         // for Symbol
#include "il/SymbolReference.hpp"                // for SymbolReference
#include "il/TreeTop.hpp"                        // for TreeTop
#include "il/TreeTop_inlines.hpp"                // for TreeTop::getNode, etc
#include "il/symbol/AutomaticSymbol.hpp"         // for AutomaticSymbol
#include "il/symbol/ParameterSymbol.hpp"         // for ParameterSymbol
#include "infra/Array.hpp"                       // for TR_ArrayIterator, etc
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "infra/BitVector.hpp"                   // for TR_BitVector, etc
#include "infra/Cfg.hpp"                         // for CFG, etc
#include "infra/Checklist.hpp"                   // for {Node,Block}Checklist
#include "infra/ILWalk.hpp"                      // for PostorderNodeIterator
#include "infra/List.hpp"                        // for List, ListIterator, etc
#include "infra/CfgEdge.hpp"                     // for CFGEdge
#include "infra/CfgNode.hpp"                     // for CFGNode
#include "optimizer/Optimization.hpp"            // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"               // for Optimizer
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"               // for TR_RegionStructure, etc
#include "optimizer/Dominators.hpp"              // for TR_Dominators
#include "optimizer/UseDefInfo.hpp"              // for TR_UseDefInfo, etc
#include "optimizer/LoopCanonicalizer.hpp"       // for TR_LoopTransformer
#include "optimizer/VPConstraint.hpp"            // for TR::VPConstraint, etc
#include "ras/Debug.hpp"                         // for TR_DebugBase

#define OPT_DETAILS "O^O INDUCTION VARIABLE ANALYSIS: "

// this is actually half of the threshold required
// because for every internal pointer temp created, there is a
// corresponding pinning array temp store
#define MAX_INTERNAL_POINTER_AUTOS_INITIALIZED 1

#define GET32BITINT(n) ((n)->getType().isInt32()?(n)->getInt():(int32_t)((n)->getLongInt()))
#define GET64BITINT(n) ((n)->getType().isInt32()?(int64_t)((n)->getInt()):(n)->getLongInt())

TR_LoopStrider::TR_LoopStrider(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager),
   _reassociatedNodes(trMemory()),
   _storeTreesSingleton((StoreTreeMapComparator()), StoreTreeMapAllocator(comp()->trMemory()->currentStackRegion()))
   {
   _indirectInductionVariable = false; // TODO: add this c->getMethodHotness() >= scorching;
   _autosAccessed = NULL;
   _numInternalPointers = 0;
   }


int32_t TR_LoopStrider::perform()
   {
   //static bool internalPtrs = !feGetEnv("TR_disableInternalPtrs");
   //if (!internalPtrs)
   //   return 0;

   //if (!cg()->supportsInternalPointers())
   //   return 0;
   //
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   static char *disableSignExtn = feGetEnv("TR_disableSelIndVar");

   _registersScarce = cg()->areAssignableGPRsScarce();

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _hoistedAutos = new (stackMemoryRegion) SymRefPairMap((SymRefPairMapComparator()), SymRefPairMapAllocator(stackMemoryRegion));
   _reassociatedAutos = new (stackMemoryRegion) SymRefMap((SymRefMapComparator()), SymRefMapAllocator(stackMemoryRegion));
   _count = 0;
   _newTempsCreated = false;
   _newNonAddressTempsCreated = false;
   _storeTreesList = NULL;
   //_loadUsedInNewLoopIncrementList = NULL;

   // 64-bit sign-extension elimination
   if (usingAladd && !disableSignExtn)
      {
      TR_UseDefInfo *info = optimizer()->getUseDefInfo();

      // find induction variable candidates that can possibly be changed to longs
      TR::NodeChecklist exLoads(comp());
      detectLoopsForIndVarConversion(comp()->getFlowGraph()->getStructure(), exLoads);

      if (_newTempsCreated)
         {
         eliminateSignExtensions(exLoads);
         optimizer()->setUseDefInfo(NULL);
         }
      }

   detectCanonicalizedPredictableLoops(comp()->getFlowGraph()->getStructure(), NULL, -1);

   if (_newTempsCreated)
      {
      requestOpt(OMR::globalValuePropagation);
      requestOpt(OMR::treeSimplification);
      optimizer()->setAliasSetsAreValid(false);
      }

   if (_newNonAddressTempsCreated)
      requestOpt(OMR::loopVersionerGroup);

   return 2;
   }




bool TR_LoopStrider::foundLoad(TR::TreeTop *storeTreeTop, TR::Node *node, int32_t symRefNum, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadVar() &&
       (node->getSymbolReference()->getReferenceNumber() == symRefNum) &&
       (node->getReferenceCount() > 1))
      {
      if (_storeTreesList)
         {
         auto lookup = _storeTreesList->find(symRefNum);
         if (lookup != _storeTreesList->end())
            {
            List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
            ListIterator<TR_StoreTreeInfo> si(storeTreesList);
            TR_StoreTreeInfo *storeTree;
            for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
               {
               if (storeTree->_tt == storeTreeTop)
                  {
                  if (node != storeTree->_loadUsedInLoopIncrement)
                     return true;
                  }
               }
            }
         }
      else if (node != _loadUsedInLoopIncrement)
         return true;
      }

   int32_t childNum;
   for (childNum=0;childNum < node->getNumChildren(); childNum++)
      {
      if (foundLoad(storeTreeTop, node->getChild(childNum), symRefNum, visitCount))
         return true;
      }
   return false;
   }


bool TR_LoopStrider::foundLoad(TR::TreeTop *storeTree, int32_t nextInductionVariableNumber, TR::Compilation *comp)
   {
   TR::TreeTop *cursorTree = storeTree;
   while (cursorTree->getNode()->getOpCodeValue() != TR::BBStart)
      cursorTree = cursorTree->getPrevTreeTop();

   bool storeInRequiredForm = true;
   vcount_t visitCount = comp->incVisitCount();
   while (cursorTree != storeTree)
      {
      if (foundLoad(storeTree, cursorTree->getNode(), nextInductionVariableNumber, visitCount))
         {
         storeInRequiredForm = false;
         break;
         }
      cursorTree = cursorTree->getNextTreeTop();
      }

   return storeInRequiredForm;
   }


static int32_t countInternalPointerOrPinningArrayStores(TR::Compilation *comp, TR::Block *block)
   {
   int32_t count = 0;
   for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isStoreDirect() &&
          ((node->getSymbol()->isAuto() &&
            (node->getSymbol()->castToAutoSymbol()->isInternalPointer() ||
             node->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())) ||
           (node->getSymbol()->isParm() &&
            node->getSymbol()->castToParmSymbol()->isPinningArrayPointer())))
         count++;
      }
   return count;
   }


int32_t TR_LoopStrider::detectCanonicalizedPredictableLoops(TR_Structure *loopStructure, TR_BitVector **optSetInfo, int32_t bitVectorSize)
   {
   // (64-bit)
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   TR_RegionStructure *regionStructure = loopStructure->asRegion();

   if (regionStructure)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         detectCanonicalizedPredictableLoops(node->getStructure(), optSetInfo, bitVectorSize);
      }

   TR_BlockStructure *loopInvariantBlock = NULL;

   if (!regionStructure ||
       !regionStructure->getParent() ||
       !regionStructure->isNaturalLoop() ||
       regionStructure->getEntryBlock()->isCold())
      return 0;

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   regionStructure->getBlocks(&blocksInRegion);
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      if (nextBlock->hasExceptionPredecessors())
         return 0;

   TR_RegionStructure *parentStructure = regionStructure->getParent()->asRegion();
   TR_StructureSubGraphNode *subNode = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getNumber() == loopStructure->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
         loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
      }

   if (loopInvariantBlock)
      {
      int32_t symRefCount = comp()->getSymRefCount();
      _numSymRefs = symRefCount;
      initializeSymbolsWrittenAndReadExactlyOnce(symRefCount, notGrowable);

      if (_storeTreesList == NULL)
         _storeTreesList = &_storeTreesSingleton;
      _storeTreesList->clear();

      //_storeTreesList = (List<TR_StoreTreeInfo> **)trMemory()->allocateStackMemory(symRefCount*sizeof(List<TR_StoreTreeInfo> *));
      //_loadUsedInNewLoopIncrementList = (List<TR::Node> **)trMemory()->allocateStackMemory(symRefCount*sizeof(List<TR::Node> *));
      //memset(_storeTreesList, 0, symRefCount*sizeof(List<TR::TreeTop> *));
      //int32_t i = 0;
      //while (i < symRefCount)
      //   {
      //   _storeTreesList[i] = new (trStackMemory()) TR_ScratchList<TR_StoreTreeInfo>(trMemory());
      //   //_loadUsedInNewLoopIncrementList[i] = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
      //   i++;
      //   }

      _reassociatedAutos->clear();
      _hoistedAutos->clear();
      _reassociatedNodes.deleteAll();
      // compute the number of internal pointer temps or pinning array temps already initialized in
      // the loop pre-header.
      // on some platforms (z in particular), a long sequence of stores results in "VS Tag Not ready"
      // stalls (aka store buffer stalls). This variable controls whether any induction variables are
      // replaced by internal pointers by the analysis
      //
      _numInternalPointerOrPinningArrayTempsInitialized = 0;
      if (1)
         {
         _numInternalPointerOrPinningArrayTempsInitialized += countInternalPointerOrPinningArrayStores(comp(), loopInvariantBlock->getBlock());
         // check if there are multiple pre-headers (introduced by previous passes of loopCanonicalization)
         //
         TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
         TR_StructureSubGraphNode *prevInvariantNode = NULL;

         if (loopInvariantNode->getPredecessors().size() == 1)
            prevInvariantNode = toStructureSubGraphNode(loopInvariantNode->getPredecessors().front()->getFrom());
         if (prevInvariantNode &&
             prevInvariantNode->getStructure()->asBlock() &&
             prevInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
            {
            _numInternalPointerOrPinningArrayTempsInitialized += countInternalPointerOrPinningArrayStores(comp(), prevInvariantNode->getStructure()->asBlock()->getBlock());
            }
         }

      _autosAccessed = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
      _numberOfLinearExprs = 0;

      _parmAutoPairs = NULL;

      if (trace())
         traceMsg(comp(), "\nChecking loop %d for predictability\n", loopStructure->getNumber());

      _isAddition = false;
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

      // this also collects info on symbols read and written exactly once
      int32_t isPredictableLoop = checkLoopForPredictability(loopStructure, loopInvariantBlock->getBlock(), NULL, false);

      vcount_t visitCount = 0;

      if (isPredictableLoop > 0)
         {
         if (trace())
             {
             traceMsg(comp(), "\nDetected a predictable loop %d\n", loopStructure->getNumber());
             traceMsg(comp(), "Possible new induction variable candidates :\n");
             comp()->getDebug()->print(comp()->getOutFile(), &_writtenExactlyOnce);
             traceMsg(comp(), "\n");
             }

         visitCount = comp()->incVisitCount();

         if (_storeTreesList)
            comp()->getSymRefTab()->getAllSymRefs(_writtenExactlyOnce);

         // calculate _numberOfLinearExprs
         identifyExpressionsLinearInInductionVariables(loopStructure, visitCount);

         _linearEquations = (int64_t **)trMemory()->allocateStackMemory(_numberOfLinearExprs*sizeof(int64_t *));
         memset(_linearEquations, 0, _numberOfLinearExprs*sizeof(int64_t *));
         _loadUsedInNewLoopIncrement = (TR::Node **)trMemory()->allocateStackMemory(_numberOfLinearExprs*sizeof(TR::Node *));
         memset(_loadUsedInNewLoopIncrement, 0, _numberOfLinearExprs*sizeof(TR::Node *));

         int32_t j;
         for (j=0;j<_numberOfLinearExprs;j++)
            _linearEquations[j] = (int64_t *)trMemory()->allocateStackMemory(5*sizeof(int64_t));
         _nextExpression = 0;

         TR_ScratchList<TR::TreeTop> predictableComputations(trMemory());
         TR::SymbolReference *newSymbolReference = NULL;
         TR::SymbolReference *inductionVarSymRef = NULL;

         if (_autosAccessed)
            {
            if (_autosAccessed->elementCount() < (comp()->cg()->getMaximumNumbersOfAssignableGPRs() - 5))
               _registersScarce = false;
            }

         // Iterate over the symbols written once (possible induction variables)
         //
         TR::SparseBitVector::Cursor cursor(_writtenExactlyOnce);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t nextInductionVariableNumber = cursor;

            if (!symRefTab->getSymRef(nextInductionVariableNumber) ||
                !symRefTab->getSymRef(nextInductionVariableNumber)->getSymbol()->isAutoOrParm())
               continue;

            _isAddition = false;
            newSymbolReference = NULL;
            inductionVarSymRef = NULL;
            _loadUsedInLoopIncrement = NULL;
            bool storeInRequiredForm = isStoreInRequiredForm(nextInductionVariableNumber, loopStructure);

//            traceMsg(comp(), "storeInRequiredForm=%d for ind var=%d symRef=#%d\n", storeInRequiredForm, nextInductionVariableNumber,
//                  symRefTab->getSymRef(nextInductionVariableNumber)->getReferenceNumber());

            //
            // TODO: If there is a use of an induction variable that refers back to a load
            //       that occurs in a tree PRIOR to the increment/decrement,
            //       then we will not attempt to transform into derived induction variable
            //       as we might produce incorrect trees (wanted old value but will get new inced/deced value) .
            //

            if (storeInRequiredForm)
               {
               TR::TreeTop *storeTree = _storeTrees[nextInductionVariableNumber];
               if (storeTree)
                  storeInRequiredForm = foundLoad(storeTree, nextInductionVariableNumber, comp());

               if (storeInRequiredForm && _storeTreesList)
                  {
                  auto lookup = _storeTreesList->find(nextInductionVariableNumber);
                  if (lookup != _storeTreesList->end())
                     {
                     List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
                     ListIterator<TR_StoreTreeInfo> si(storeTreesList);
                     TR_StoreTreeInfo *storeTree;
                     for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
                        {
                        //traceMsg(comp(), "store tree %p\n", storeTree->_tt->getNode());
                        storeInRequiredForm = foundLoad(storeTree->_tt, nextInductionVariableNumber, comp());
                        if (!storeInRequiredForm)
                           {
                           //traceMsg(comp(), "reject store tree %p\n", storeTree->_tt->getNode());
                           break;
                           }
                        }
                     }
                  }
               }
            else
               continue;

            bool isLoopTestInductionVariable = false;
            bool loopTestCanBeChangedIfRequired = true;
            int32_t maxFactorAllowed = 1;
            TR::Node *storeOfDerivedInductionVariable = NULL;
            int32_t loopTestHasSymRef = -1;

            //traceMsg(comp(), "store in reqd form %d for ind var %d\n", storeInRequiredForm, nextInductionVariableNumber);

            if (_loopTestTree->getNode()->getNumChildren() == 2)
               {
               TR::SymbolReference *indexSymRef = NULL;
               TR::Node *firstChildOfLoopTestNode = _loopTestTree->getNode()->getFirstChild();
               if ((firstChildOfLoopTestNode->getOpCodeValue() == TR::l2i) ||
                   (firstChildOfLoopTestNode->getOpCodeValue() == TR::i2l))
                  firstChildOfLoopTestNode = firstChildOfLoopTestNode->getFirstChild();
               TR::Node *secondChildOfLoopTestNode = _loopTestTree->getNode()->getSecondChild();
               if ((secondChildOfLoopTestNode->getOpCodeValue() == TR::l2i) ||
                     (secondChildOfLoopTestNode->getOpCodeValue() == TR::i2l))
                  secondChildOfLoopTestNode = secondChildOfLoopTestNode->getFirstChild();
               if (firstChildOfLoopTestNode->getOpCode().hasSymbolReference())
                  {
                  indexSymRef = firstChildOfLoopTestNode->getSymbolReference();
                  loopTestHasSymRef = 0;
                  }
               else
                  {
                  TR::TreeTop *storeTree = _storeTrees[nextInductionVariableNumber];
                  TR::Node *storeNode = storeTree->getNode();

                  if (!storeNode->getOpCode().isStore())
                     storeNode = storeNode->getFirstChild();

                  TR_ASSERT(storeNode->getOpCode().isStore(), "Expected to find a store\n");

                  if (firstChildOfLoopTestNode == storeNode->getFirstChild() && _loadUsedInLoopIncrement)
                     indexSymRef = storeNode->getSymbolReference();
                  else
                     {
                     if (_storeTreesList)
                        {
                        auto lookup = _storeTreesList->find(nextInductionVariableNumber);
                        if (lookup != _storeTreesList->end())
                           {
                           List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
                           ListIterator<TR_StoreTreeInfo> si(storeTreesList);
                           TR_StoreTreeInfo *storeTree;
                           for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
                              {
                              TR::Node *storeNode = storeTree->_tt->getNode();

                              if (!storeNode->getOpCode().isStore())
                                 storeNode = storeNode->getFirstChild();

                              TR_ASSERT(storeNode->getOpCode().isStore(), "Expected to find a store\n");

                              if (firstChildOfLoopTestNode == storeNode->getFirstChild() &&
                                 storeTree->_loadUsedInLoopIncrement)
                                 {
                                 indexSymRef = storeNode->getSymbolReference();
                                 break;
                                 }
                              }
                           }
                        }
                     }
                  }

               if (!secondChildOfLoopTestNode->getOpCode().isLoadConst() &&
                   !(secondChildOfLoopTestNode->getOpCode().isLoadVar() &&
                     secondChildOfLoopTestNode->getSymbolReference()->getSymbol()->isAutoOrParm() &&
                     _neverWritten->get(secondChildOfLoopTestNode->getSymbolReference()->getReferenceNumber())))
                  {
                  indexSymRef = NULL;
                  }

               if (indexSymRef &&
                   indexSymRef->getReferenceNumber() == _loopDrivingInductionVar)
                  {
                  isLoopTestInductionVariable = true;
                  if (loopTestHasSymRef != 0)
                     loopTestHasSymRef = 1;
                  loopTestCanBeChangedIfRequired = false;
                  TR_InductionVariable *v = loopStructure->asRegion()->findMatchingIV(indexSymRef);

                  if (v && v->getEntry() && v->getExit())
                     {
                     TR::VPConstraint *entryVal = v->getEntry();
                     TR::VPConstraint *exitVal = v->getExit();
                     if (entryVal->asIntConstraint())
                        {
                        if (entryVal->getHighInt() <= TR::getMaxSigned<TR::Int32>()/8)
                           maxFactorAllowed = 8;
                        else if (entryVal->getHighInt() <= TR::getMaxSigned<TR::Int32>()/4)
                           maxFactorAllowed = 4;
                        else if (entryVal->getHighInt() <= TR::getMaxSigned<TR::Int32>()/2)
                           maxFactorAllowed = 2;

                        if (entryVal->getLowInt() >= TR::getMinSigned<TR::Int32>()/8)
                           {
                           if (maxFactorAllowed >= 8)
                              maxFactorAllowed = 8;
                           }
                        else if (entryVal->getLowInt() >= TR::getMinSigned<TR::Int32>()/4)
                           {
                           if (maxFactorAllowed >= 4)
                              maxFactorAllowed = 4;
                           }
                        else if (entryVal->getLowInt() >= TR::getMinSigned<TR::Int32>()/2)
                           {
                           if (maxFactorAllowed >= 2)
                              maxFactorAllowed = 2;
                           }
                        }

                     if (exitVal->asIntConstraint())
                        {
                        if (maxFactorAllowed > 1)
                           {
                           if (exitVal->getHighInt() <= TR::getMaxSigned<TR::Int32>()/8)
                              {
                              if (maxFactorAllowed >= 8)
                                 maxFactorAllowed = 8;
                              }
                           else if (exitVal->getHighInt() <= TR::getMaxSigned<TR::Int32>()/4)
                              {
                              if (maxFactorAllowed >= 4)
                                 maxFactorAllowed = 4;
                              }
                           else if (exitVal->getHighInt() <= TR::getMaxSigned<TR::Int32>()/2)
                              {
                              if (maxFactorAllowed >= 2)
                                 maxFactorAllowed = 2;
                              }

                           if (exitVal->getLowInt() >= TR::getMinSigned<TR::Int32>()/8)
                              {
                              if (maxFactorAllowed >= 8)
                                 maxFactorAllowed = 8;
                              }
                           else if (exitVal->getLowInt() >= TR::getMinSigned<TR::Int32>()/4)
                              {
                              if (maxFactorAllowed >= 4)
                                 maxFactorAllowed = 4;
                              }
                           else if (exitVal->getLowInt() >= TR::getMinSigned<TR::Int32>()/2)
                              {
                              if (maxFactorAllowed >= 2)
                                 maxFactorAllowed = 2;
                              }
                           }
                        }

                     if (entryVal->asLongConstraint())
                        {
                        if (entryVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/256)
                           maxFactorAllowed = 256;
                        else if (entryVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/8)
                           maxFactorAllowed = 8;
                        else if (entryVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/4)
                           maxFactorAllowed = 4;
                        else if (entryVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/2)
                           maxFactorAllowed = 2;

                        if (entryVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/256)
                           {
                           if (maxFactorAllowed >= 256)
                              maxFactorAllowed = 256;
                           }
                        else if (entryVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/8)
                           {
                           if (maxFactorAllowed >= 8)
                              maxFactorAllowed = 8;
                           }
                        else if (entryVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/4)
                           {
                           if (maxFactorAllowed >= 4)
                              maxFactorAllowed = 4;
                           }
                        else if (entryVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/2)
                           {
                           if (maxFactorAllowed >= 2)
                              maxFactorAllowed = 2;
                           }
                        }

                     if (exitVal->asLongConstraint())
                        {
                        if (maxFactorAllowed > 1)
                           {
                           if (exitVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/256)
                              {
                              if (maxFactorAllowed >= 256)
                                 maxFactorAllowed = 256;
                              }
                           else if (exitVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/8)
                              {
                              if (maxFactorAllowed >= 8)
                                 maxFactorAllowed = 8;
                              }
                           else if (exitVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/4)
                              {
                              if (maxFactorAllowed >= 4)
                                 maxFactorAllowed = 4;
                              }
                           else if (exitVal->getHighLong() <= TR::getMaxSigned<TR::Int64>()/2)
                              {
                              if (maxFactorAllowed >= 2)
                                 maxFactorAllowed = 2;
                              }

                           if (exitVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/256)
                              {
                              if (maxFactorAllowed >= 256)
                                 maxFactorAllowed = 256;
                              }
                           else if (exitVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/8)
                              {
                              if (maxFactorAllowed >= 8)
                                 maxFactorAllowed = 8;
                              }
                           else if (exitVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/4)
                              {
                              if (maxFactorAllowed >= 4)
                                 maxFactorAllowed = 4;
                              }
                           else if (exitVal->getLowLong() >= TR::getMinSigned<TR::Int64>()/2)
                              {
                              if (maxFactorAllowed >= 2)
                                 maxFactorAllowed = 2;
                              }
                           }
                        }

                     if (maxFactorAllowed > 1)
                        {
                        loopTestCanBeChangedIfRequired = true;
                        }
                     }
                  }
               }

            bool exitValueNeeded = false;
            //if (_cannotBeEliminated->get(nextInductionVariableNumber))
            {
               List<TR::Block> exitBlocks(trMemory());
               ListIterator<TR::CFGEdge> ei(&loopStructure->asRegion()->getExitEdges());
               for (TR::CFGEdge *edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
                  {
                  TR_StructureSubGraphNode *target = edge->getTo()->asStructureSubGraphNode();
                  TR::CFGNode *cfgNode =  comp()->getFlowGraph()->getFirstNode();
                  while (cfgNode)
                     {
//                     dumpOptDetails(comp(), "Target %d cfgNode %d\n", target->getNumber(), cfgNode->getNumber());
                     if (cfgNode->getNumber() == target->getNumber())
                        {
                        exitBlocks.add(cfgNode->asBlock());
                        break;
                        }

                     cfgNode = cfgNode->getNext();
                     }
                  }

               ListIterator<TR::Block> bi(&exitBlocks);
               vcount_t visitCount = comp()->incVisitCount();
               for (TR::Block *exitBlock = bi.getCurrent(); exitBlock != NULL; exitBlock = bi.getNext())
                  {
                  bool seenStore = false;
                  exitValueNeeded = unchangedValueNeededIn(exitBlock, nextInductionVariableNumber, seenStore);
                  //dumpOptDetails(comp(), "Exit block is %d and exitValueNeeded %d\n", exitBlock->getNumber(), exitValueNeeded);

                  if (!exitValueNeeded &&
                      !seenStore)
                     {
                     TR::Block   *next;
                     for (auto edge = exitBlock->getSuccessors().begin(); edge != exitBlock->getSuccessors().end(); ++edge)
                        {
                        next = toBlock((*edge)->getTo());
                        exitValueNeeded = unchangedValueNeededIn(next, nextInductionVariableNumber, seenStore);
                        //dumpOptDetails(comp(), "Exit succ block is %d and exitValueNeeded %d\n", next->getNumber(), exitValueNeeded);
                        if (exitValueNeeded)
                           break;
                        }
                     }

                  if (exitValueNeeded)
                     break;
                  }
            }

            //TODO: get rid of this cannotBeEliminated test for PPC and
            // do not change the loop test to use the new variable
            //
            if (storeInRequiredForm &&
                (!_registersScarce ||
                 (!exitValueNeeded &&
                  (!_cannotBeEliminated->get(nextInductionVariableNumber) ||
                   (isLoopTestInductionVariable && loopTestCanBeChangedIfRequired)))) &&
                performTransformation(comp(), "%s Replacing uses of induction var %d in loop %d\n", OPT_DETAILS, nextInductionVariableNumber, loopStructure->getNumber()))
               {
               inductionVarSymRef = symRefTab->getSymRef(_loopDrivingInductionVar);
               _startExpressionForThisInductionVariable = _nextExpression;

               comp()->incVisitCount();

               // This calls examineTreeForInductionVariableUse
               if (replaceAllInductionVariableComputations(loopInvariantBlock->getBlock(), loopStructure, &newSymbolReference, inductionVarSymRef))
                  {
                  int32_t k=_startExpressionForThisInductionVariable;
                  int32_t bestCandidate = k;
                  int64_t highestFactor = -1;
                  TR_InductionVariable *strideForPIV = NULL;
                  for (;k<_nextExpression;k++)
                     {
                     newSymbolReference = symRefTab->getSymRef((int32_t)_linearEquations[k][1]);
                     TR_RegisterCandidate *newInductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(newSymbolReference);
                     //
                     // Add the derived induction variable as a global register candidate
                     // within the loop
                     //
                     if (performTransformation(comp(), "%s Adding auto %d (created for striding) as a global register "
                           "candidate in loop %d\n", OPT_DETAILS,
                           newInductionCandidate->getSymbolReference()->getReferenceNumber(), loopStructure->getNumber()))
                        newInductionCandidate->addAllBlocksInStructure(loopStructure, comp(), trace()? "strider auto":NULL);

                     // Add the initialization of the derived induction variable before the
                     // loop is entered
                     //
                     TR::Node *placeHolderNode = placeInitializationTreeInLoopInvariantBlock(loopInvariantBlock,
                           inductionVarSymRef, newSymbolReference, k, symRefTab);

                     // Add the increment/decrement of the derived induction variable immediately
                     // after the increment/decrement of the original induction variable
                     //
                     TR::Node *storeNode = placeNewInductionVariableIncrementTree(loopInvariantBlock, inductionVarSymRef,
                           newSymbolReference, k, symRefTab, placeHolderNode, loopTestHasSymRef);


                     // Do not consider linear equations with
                     // non-const multiplicative term for loop induction variable
                     if (!isMulTermConst(k)) continue;

                     int64_t mulConst = getMulTermConst(k);

                     if (mulConst < 0)
                        {
                        if ((-1*mulConst > highestFactor) ||
                            ((-1*mulConst == highestFactor) &&
                             (_linearEquations[k][4] > -1)))
                           {
                           bestCandidate = k;
                           highestFactor = -1*mulConst;
                           storeOfDerivedInductionVariable = storeNode;
                           }
                        }
                     else
                        {
                        if ((mulConst > highestFactor) ||
                            ((mulConst == highestFactor) &&
                             (_linearEquations[k][4] > -1)))
                           {
                           bestCandidate = k;
                           highestFactor = mulConst;
                           storeOfDerivedInductionVariable = storeNode;
                           }
                        }

                     // todo: create IV even if non-const additive term? need to check loop-invariance and
                     //       known constraints
                     if (!isAdditiveTermConst(k))
                        continue;

                     if (newSymbolReference->getSymbol()->getDataType() != TR::Address)
                        {
                         TR_InductionVariable *v = loopStructure->asRegion()->findMatchingIV(inductionVarSymRef);
                        if (v)
                           {
                           TR::VPConstraint *entryVal = v->getEntry();
                           TR::VPConstraint *exitVal = v->getExit();
                           TR::VPConstraint *incr = v->getIncr();
                           TR::VPConstraint *newEntryVal = NULL;
                           TR::VPConstraint *newExitVal = NULL;
                           TR::VPConstraint *newIncr = NULL;

                           if (newSymbolReference->getSymbol()->getType().isInt64())
                              {
                              newEntryVal = genVPLongRange(entryVal, mulConst, getAdditiveTermConst(k));
                              newExitVal = genVPLongRange(exitVal, mulConst, getAdditiveTermConst(k));
                              newIncr = genVPLongRange(incr, mulConst, 0);
                              }
                           else
                              {
                              newEntryVal = genVPIntRange(entryVal, mulConst, getAdditiveTermConst(k));
                              newExitVal = genVPIntRange(exitVal, mulConst, getAdditiveTermConst(k));
                              newIncr = genVPIntRange(incr, mulConst, 0);
                              }

                           TR_InductionVariable *iv = new (trHeapMemory()) TR_InductionVariable(newSymbolReference->getSymbol()->castToRegisterMappedSymbol(),
                                                                                                newEntryVal, newExitVal, newIncr,
                                                                                                _loopTestTree->getNode()->getOpCode().isUnsignedCompare() ? TR_no : TR_yes);
                           if (bestCandidate == k)
                              strideForPIV = iv;
                           loopStructure->asRegion()->addInductionVariable(iv);
                           }
                        }

                     if (trace())
                        traceMsg(comp(),"Found an induction var chance in %s\n", comp()->signature());

                     } // for loop over all linear expressions

                  TR::Node *loopTestNode = _loopTestTree->getNode();
                  if (isMulTermConst(bestCandidate) &&
                      isLoopTestInductionVariable &&
                      highestFactor <= maxFactorAllowed &&
                      /*
                       * don't change loop exit test from IV to stride if it's unsafe to do so.
                       * IE. stride straddles zero(1), test is unsigned inequality.
                       * To determine (1), we need the range constraints on stride to test
                       * if it only contains non-negatives or non-positives.
                       * Reject if constraints are unknown due to non-const additive term.
                       *                                                         */
                      !(0 &&   //no-strict IV
                        loopTestNode->getOpCode().isCompareForOrder() &&        //unsigned inequality
                        loopTestNode->getOpCode().isUnsignedCompare() &&
                        (!isAdditiveTermConst(bestCandidate) ||                 //non-const additive term
                         !strideForPIV ||                                       //no stride iv
                         // we have stride, but range not strictly non-neg or non-pos.
                         !(strideForPIV->getEntry()->asLongConstraint() ?
                               (strideForPIV->getEntry()->getLowLong() >= 0 ||
                                strideForPIV->getExit()->getHighLong() <= 0) :
                               (strideForPIV->getEntry()->getLowInt() >= 0 ||   // int constraint versions
                                strideForPIV->getExit()->getHighInt() <= 0)))
                        ) &&
                      performTransformation(comp(),"%sReplacing loop test node in %s\n", OPT_DETAILS, comp()->signature())
                     )
                     {
                     changeLoopCondition(loopInvariantBlock, usingAladd, bestCandidate, storeOfDerivedInductionVariable);
                     }
                  }
               else
                  {
                  TR_RegisterCandidate *inductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(inductionVarSymRef);
                  if (performTransformation(comp(), "%s Adding auto %d as a global register candidate in loop %d\n", OPT_DETAILS, inductionCandidate->getSymbolReference()->getReferenceNumber(), loopStructure->getNumber()))
                     inductionCandidate->addAllBlocksInStructure(loopStructure, comp(), trace()?"auto":NULL);
                  }
               }
            } // loop over the symbols written once
         }

      if (!reassociateAndHoistNonPacked())
         return 0;

      if (trace())
         {
         traceMsg(comp(),"Found an induction var chance in %s\n", comp()->signature());
         }

      TR::Block *b = loopInvariantBlock->getBlock();
      reassociateAndHoistComputations(b, loopStructure);

      TR::Node *byteCodeInfoNode = b->getEntry()->getNode();
      TR::TreeTop *placeHolderTree = b->getLastRealTreeTop();
      if (!placeHolderTree->getNode()->getOpCode().isBranch())
         placeHolderTree = b->getExit();

      int32_t j;
      for (j=0;j<symRefCount;j++)
         {
         auto reassociatedAutoSearchResult = _reassociatedAutos->find(j);
         TR::SymbolReference *reassociatedAuto = reassociatedAutoSearchResult != _reassociatedAutos->end() ?
                                                    reassociatedAutoSearchResult->second : NULL;
         if (reassociatedAuto)
            {
            //dumpOptDetails(comp(), "j = %d reassociated auto = %d (%p)\n", j,
            //       reassociatedAuto->getReferenceNumber(), reassociatedAuto);
            if (trace())
               traceMsg(comp(), "Found an reassociated induction var chance in %s\n", comp()->signature());

            TR::SymbolReference *origAuto = symRefTab->getSymRef(j);

            TR::Node *arrayRefNode;
            int32_t hdrSize = (int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
            if (usingAladd)
               {
               TR::Node *constantNode = TR::Node::create(byteCodeInfoNode, TR::lconst);
               //constantNode->setLongInt(16);
               constantNode->setLongInt((int64_t)hdrSize);
               arrayRefNode = TR::Node::create(TR::aladd, 2,
                     TR::Node::createWithSymRef(byteCodeInfoNode,
                           TR::aload, 0, origAuto), constantNode);
               }
            else
               arrayRefNode = TR::Node::create(TR::aiadd, 2,
                     TR::Node::createWithSymRef(byteCodeInfoNode, TR::aload, 0, origAuto),
                     TR::Node::create(byteCodeInfoNode, TR::iconst, 0, hdrSize));
            arrayRefNode->setIsInternalPointer(true);

            if (!origAuto->getSymbol()->isInternalPointer())
                {
                arrayRefNode->setPinningArrayPointer(reassociatedAuto->getSymbol()-> \
                    castToInternalPointerAutoSymbol()->getPinningArrayPointer());
                reassociatedAuto->getSymbol()->castToInternalPointerAutoSymbol()-> \
                    getPinningArrayPointer()->setPinningArrayPointer();
                }
            else
                arrayRefNode->setPinningArrayPointer(origAuto->getSymbol()-> \
                    castToInternalPointerAutoSymbol()->getPinningArrayPointer());

            TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, arrayRefNode, reassociatedAuto);
            newStore->setLocalIndex(~0);
            TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
            placeHolderTree->getPrevTreeTop()->join(newStoreTreeTop);
            newStoreTreeTop->join(placeHolderTree);

            dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis "
                  "inserted reassociation initialization tree : %p for new symRef #%d\n",
                  newStoreTreeTop->getNode(), reassociatedAuto->getReferenceNumber());

            TR_RegisterCandidate *newInductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(reassociatedAuto);
            //
            // Add the reassociated auto as a global register candidate
            // within the loop
            //
            if (performTransformation(comp(), "%s Adding auto %d (created for striding) as a "
                  "global register candidate in loop %d\n", OPT_DETAILS,
                  newInductionCandidate->getSymbolReference()->getReferenceNumber(), loopStructure->getNumber()))
               newInductionCandidate->addAllBlocksInStructure(loopStructure, comp(), trace()?"strider auto":NULL);
            }

         auto symRefPairSearchResult = _hoistedAutos->find(j);
         SymRefPair *symRefPair = symRefPairSearchResult != _hoistedAutos->end() ? symRefPairSearchResult->second : NULL;

         if (symRefPair)
            {
            if (trace())
               traceMsg(comp(), "Found an internal pointer hoisting chance in %s\n", comp()->signature());

            TR::SymbolReference *origAuto = symRefTab->getSymRef(j);

            while (symRefPair)
               {
               TR::Node *arrayRefNode = NULL;
               if (symRefPair->_isConst)
                  {
                  int32_t index = symRefPair->_index;
                  if (usingAladd)
                     {
                     TR::Node *constantNode  = TR::Node::create(byteCodeInfoNode, TR::lconst);
                     constantNode->setLongInt((int64_t)index);
                     arrayRefNode = TR::Node::create(TR::aladd, 2, TR::Node::createWithSymRef(byteCodeInfoNode, TR::aload, 0, origAuto), constantNode);
                     }
                  else
                     arrayRefNode = TR::Node::create(TR::aiadd, 2, TR::Node::createWithSymRef(byteCodeInfoNode, TR::aload, 0, origAuto), TR::Node::create(byteCodeInfoNode, TR::iconst, 0, index));
                  }
               else
                  {
                  TR::SymbolReference *indexSymRef = symRefPair->_indexSymRef;
                  if (usingAladd)
                     arrayRefNode = TR::Node::create(TR::aladd, 2, TR::Node::createWithSymRef(byteCodeInfoNode, TR::aload, 0, origAuto), TR::Node::createWithSymRef(byteCodeInfoNode, TR::lload, 0, indexSymRef));
                  else
                     arrayRefNode = TR::Node::create(TR::aiadd, 2, TR::Node::createWithSymRef(byteCodeInfoNode, TR::aload, 0, origAuto), TR::Node::createWithSymRef(byteCodeInfoNode, TR::iload, 0, indexSymRef));
                  }

               arrayRefNode->setIsInternalPointer(true);

               if (!origAuto->getSymbol()->isInternalPointer())
                  {
                  symRefPair->_derivedSymRef->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer()->setPinningArrayPointer();
                  arrayRefNode->setPinningArrayPointer(symRefPair->_derivedSymRef->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
                  }
                else
                    arrayRefNode->setPinningArrayPointer(origAuto->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

               TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, arrayRefNode, symRefPair->_derivedSymRef);
               newStore->setLocalIndex(~0);
               TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
               placeHolderTree->getPrevTreeTop()->join(newStoreTreeTop);
               newStoreTreeTop->join(placeHolderTree);

               dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted invariant initialization tree : %p for new symRef #%d\n", newStoreTreeTop->getNode(), symRefPair->_derivedSymRef->getReferenceNumber());

               TR_RegisterCandidate *newInductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(symRefPair->_derivedSymRef);
               //
               // Add the reassociated auto as a global register candidate
               // within the loop
               //
               if (performTransformation(comp(), "%s Adding auto %d (created for reassociation) as a global register candidate in loop %d\n", OPT_DETAILS, newInductionCandidate->getSymbolReference()->getReferenceNumber(), loopStructure->getNumber()))
                  newInductionCandidate->addAllBlocksInStructure(loopStructure, comp(), trace()?"reassociation auto":NULL);

               symRefPair = symRefPair->_next;
               }
            }
         }
      }
      return 1;
   }

bool TR_LoopStrider::reassociateAndHoistNonPacked()
   {
   if ((_registersScarce &&
        cg()->getSupportsScaledIndexAddressing()) ||
        (!cg()->getSupportsScaledIndexAddressing() &&
         cg()->getSupportsConstantOffsetInAddressing()))
       return false;

   return true;
   }


TR::VPLongRange* TR_LoopStrider::genVPLongRange(TR::VPConstraint* cons, int64_t coeff, int64_t additive)
   {
   if (cons)
      {
      int64_t low, high;
      if (cons->asIntConstraint())
         {
         low = (int64_t) cons->getLowInt();
         high = (int64_t) cons->getHighInt();
         }
      else
         {
         low = cons->getLowLong();
         high = cons->getHighLong();
         }

      return new (trHeapMemory()) TR::VPLongRange(low*coeff + additive, high*coeff + additive);
      }
   return NULL;
   }

TR::VPIntRange* TR_LoopStrider::genVPIntRange(TR::VPConstraint* cons, int64_t coeff, int64_t additive)
   {
   if (cons && cons->asIntConstraint())
      {
      int32_t low = cons->getLowInt();
      int32_t high = cons->getHighInt();
      return new (trHeapMemory()) TR::VPIntRange((int32_t)(low*coeff + additive), (int32_t)(high*coeff + additive));
      }
   return NULL;
   }


void TR_LoopStrider::findOrCreateStoreInfo(TR::TreeTop *tree, int32_t i)
   {
   auto lookup = _storeTreesList->find(i);
   if (lookup != _storeTreesList->end())
      {
      List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
      ListIterator<TR_StoreTreeInfo> si(storeTreesList);
      TR_StoreTreeInfo *storeTree;
      for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
         {
         if (storeTree->_tt == tree)
            return;
         }

      lookup->second->add(new (trStackMemory()) TR_StoreTreeInfo(tree, NULL, NULL, NULL, NULL, false, NULL, false));
      }
   else
      {
      TR_ScratchList<TR_StoreTreeInfo> *newList = new (trStackMemory()) TR_ScratchList<TR_StoreTreeInfo>(trMemory());
      newList->add(new (trStackMemory()) TR_StoreTreeInfo(tree, NULL, NULL, NULL, NULL, false, NULL, false));
      (*_storeTreesList)[i] = newList;
      }
   }


void TR_LoopStrider::updateStoreInfo(int32_t i, TR::TreeTop *tree)
   {
   _storeTrees[i] = tree;
   if (_storeTreesList)
      findOrCreateStoreInfo(tree, i);
   }


TR::Node *TR_LoopStrider::getNewLoopIncrement(TR::Node *oldLoad, int32_t k, int32_t symRefNum)
   {
   TR::Node *newLoad = NULL;

   if (!newLoad)
      {
      if (_storeTreesList)
         {
         auto lookup = _storeTreesList->find(symRefNum);
         if (lookup != _storeTreesList->end())
            {
            List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
            ListIterator<TR_StoreTreeInfo> si(storeTreesList);
            TR_StoreTreeInfo *storeTree;
            for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
               {
               if (oldLoad == storeTree->_loadUsedInLoopIncrement &&
                   storeTree->_load)
                  {
                  TR_NodeIndexPair *head = storeTree->_loads;
                  TR_NodeIndexPair *cursor = head;

                  while (cursor)
                     {
                     if (cursor->_index == k)
                        {
                        newLoad = cursor->_node;
                        if (newLoad)
	                   break;
                        }

                     cursor = cursor->_next;
                     }

                  if (newLoad)
	             break;
                  //newLoad = storeTree->_load;
                  //break;
                  }
               }
            }
         }
      }

   if (!newLoad &&
       (oldLoad == _loadUsedInLoopIncrement) &&
       _loadUsedInNewLoopIncrement[k])
      {
      newLoad = _loadUsedInNewLoopIncrement[k];
      }

   return newLoad;
   }


TR::Node *TR_LoopStrider::getNewLoopIncrement(TR_StoreTreeInfo *info, int32_t k)
   {
   if (!info)
      return _loadUsedInNewLoopIncrement[k];

   TR_NodeIndexPair *loadPair = info->_loads;
   TR::Node *loadNode = NULL;
   while (loadPair)
      {
      if (loadPair->_index == k)
         {
         return loadPair->_node;
         break;
         }

      loadPair = loadPair->_next;
      }

   return NULL;
   }

void TR_LoopStrider::changeBranchFromIntToLong(TR::Node* branch)
   {
   if (branch->getOpCodeValue() == TR::ificmplt)
      TR::Node::recreate(branch, TR::iflcmplt);
   else if (branch->getOpCodeValue() == TR::ificmpgt)
      TR::Node::recreate(branch, TR::iflcmpgt);
   else if (branch->getOpCodeValue() == TR::ificmpge)
      TR::Node::recreate(branch, TR::iflcmpge);
   else if (branch->getOpCodeValue() == TR::ificmple)
      TR::Node::recreate(branch, TR::iflcmple);
   else if (branch->getOpCodeValue() == TR::ificmpeq)
      TR::Node::recreate(branch, TR::iflcmpeq);
   else if (branch->getOpCodeValue() == TR::ificmpne)
      TR::Node::recreate(branch, TR::iflcmpne);
   }



void TR_LoopStrider::changeLoopCondition(TR_BlockStructure *loopInvariantBlock, bool usingAladd, int32_t bestCandidate, TR::Node *storeOfDerivedInductionVariable)
   {
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
   // Create the multiply node first; e.g. 4*i
   //
   TR::Node *loopTestNode = _loopTestTree->getNode();

   //
   // take care of trees introduced due to sign-extn
   // ificmpY
   //    l2i
   //      lload
   //
   TR::Node *firstChildIsConversion = NULL;
   TR::Node *secondChildIsConversion = NULL;

   TR::Node *firstChildOfLoopTestNode = loopTestNode->getFirstChild();
   if ((firstChildOfLoopTestNode->getOpCodeValue() == TR::l2i) ||
       (firstChildOfLoopTestNode->getOpCodeValue() == TR::i2l))
      {
      firstChildIsConversion = firstChildOfLoopTestNode;
      firstChildOfLoopTestNode = firstChildOfLoopTestNode->getFirstChild();
      }

   TR::Node *secondChildOfLoopTestNode = loopTestNode->getSecondChild();
   if ((secondChildOfLoopTestNode->getOpCodeValue() == TR::l2i) ||
       (secondChildOfLoopTestNode->getOpCodeValue() == TR::i2l))
      {
      secondChildIsConversion = secondChildOfLoopTestNode;
      secondChildOfLoopTestNode = secondChildOfLoopTestNode->getFirstChild();
      }

   if (!usingAladd &&
       (_linearEquations[bestCandidate][4] > -1) &&
       secondChildOfLoopTestNode->getType().isInt64())
      {
      // Cannot let this case proceed since we will have a SInt64 node created below
      // and then we would be in a situation where we would have to create a TR::aladd node
      // on 32-bit, which we are not allowed to do (at least currently)
      //
      return;
      }

   TR::Node *mulNode;
   if (usingAladd || (secondChildOfLoopTestNode->getType().isInt64()))
      {
      TR::Node *constantNode = duplicateMulTermNode(bestCandidate, secondChildOfLoopTestNode, TR::Int64);

      if (!secondChildOfLoopTestNode->getType().isInt64())
         {
         TR::Node *i2lNode = TR::Node::create(secondChildOfLoopTestNode,
                                            TR::i2l, 1);
         i2lNode->setAndIncChild(0, secondChildOfLoopTestNode->duplicateTree());
         mulNode = TR::Node::create(TR::lmul, 2,
                                   i2lNode, constantNode);
         }
      else
         mulNode = TR::Node::create(TR::lmul, 2, secondChildOfLoopTestNode->duplicateTree(), constantNode);
      }
   else
      mulNode = TR::Node::create(TR::imul, 2, secondChildOfLoopTestNode->duplicateTree(),
                duplicateMulTermNode(bestCandidate, secondChildOfLoopTestNode, TR::Int32));

   mulNode->setLocalIndex(~0);
   mulNode->getSecondChild()->setLocalIndex(~0);
   TR::Node *addNode = NULL;
   if (getAdditiveTermNode(bestCandidate) != 0)
      {

      addNode = TR::Node::create(mulNode->getType().isInt64() ? TR::ladd : TR::iadd, 2, mulNode,
	       	                duplicateAdditiveTermNode(bestCandidate, secondChildOfLoopTestNode,mulNode->getDataType()));

      addNode->setLocalIndex(~0);
      addNode->getSecondChild()->setLocalIndex(~0);
      }
   else
      addNode = mulNode;

   //Create the add node now; e.g. (4*i+16)
   //
   TR::Node *newStore = NULL;
   TR::SymbolReference *newSymbolReference = NULL;
   if ((_linearEquations[bestCandidate][4] > -1) &&
       (comp()->getSymRefTab()->getNumInternalPointers() < maxInternalPointersAtPointTooLateToBackOff()))
      {
      TR::Node *newAload = TR::Node::createLoad(secondChildOfLoopTestNode, symRefTab->getSymRef((int32_t)_linearEquations[bestCandidate][4]));
      newAload->setLocalIndex(~0);
      if (usingAladd)
         addNode = TR::Node::create(TR::aladd, 2, newAload, addNode);
      else
         addNode = TR::Node::create(TR::aiadd, 2, newAload, addNode);
      addNode->setIsInternalPointer(true);

      if (!newAload->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer())
          {
          addNode->setPinningArrayPointer(newAload->getSymbolReference()->getSymbol()->castToAutoSymbol());
          newAload->getSymbolReference()->getSymbol()->setPinningArrayPointer();
          }
      else
          addNode->setPinningArrayPointer(newAload->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

      addNode->setLocalIndex(~0);
      addNode->getSecondChild()->setLocalIndex(~0);
      newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, true);
      _newTempsCreated = true;
      _numInternalPointers++;
      TR::Symbol *symbol = newSymbolReference->getSymbol();

      TR::AutomaticSymbol *pinningArrayPointer = newAload->getSymbolReference()->getSymbol()->castToAutoSymbol();
      if (!pinningArrayPointer->isInternalPointer())
         {
         symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer);
         pinningArrayPointer->setPinningArrayPointer();
         }
      else
         symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

      newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, addNode, newSymbolReference);
      }
      else
         {
         newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(),
            addNode->getDataType(), false);
         _newTempsCreated = true;
         _newNonAddressTempsCreated = true;
         newStore = TR::Node::createWithSymRef(addNode->getType().isInt64() ? TR::lstore : TR::istore, 1, 1, addNode, newSymbolReference);
         }

      TR::TreeTop *newStoreTree = TR::TreeTop::create(comp(), newStore, NULL, NULL);
      TR::Block *b = loopInvariantBlock->getBlock();
      TR::TreeTop *placeHolderTree = b->getLastRealTreeTop();
      if (!placeHolderTree->getNode()->getOpCode().isBranch())
         placeHolderTree = b->getExit();

      TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
      prevTree->join(newStoreTree);
      newStoreTree->join(placeHolderTree);

      TR::Node *firstOperand = NULL;
      if (firstChildOfLoopTestNode->getOpCode().hasSymbolReference())
         {
         //
         // check if the loop test uses the load of original induction variable
         // prior to the increment/decrement value.
         //
         TR::Node *newLoad = getNewLoopIncrement(firstChildOfLoopTestNode, bestCandidate, firstChildOfLoopTestNode->getSymbolReference()->getReferenceNumber());
         /*
         if ((firstChildOfLoopTestNode == _loadUsedInLoopIncrement) &&
              _loadUsedInNewLoopIncrement[bestCandidate])
            {
            newLoad = _loadUsedInNewLoopIncrement[bestCandidate];
            }

         if (!newLoad)
            {
            if (_storeTreesList)
               {
               auto lookup = _storeTreesList->find(bestCandidate);
               if (lookup != _storeTreesList->end())
                  {
                  List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
                  ListIterator<TR_StoreTreeInfo> si(storeTreesList);
                  TR_StoreTreeInfo *storeTree;
                  for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
                     {
                     if (firstChildOfLoopTestNode == storeTree->_loadUsedInLoopIncrement &&
                         storeTree->_load)
                         {
                         newLoad = storeTree->_load;
                         break;
                         }
                     }
                  }
               }
            }
         */

         if (newLoad)
            {
            firstOperand = newLoad;
            }
         else
            {
            TR::SymbolReference *derivedSymbolReference = symRefTab->getSymRef((int32_t)_linearEquations[bestCandidate][1]);
            firstOperand = TR::Node::createWithSymRef(firstChildOfLoopTestNode, comp()->il.opCodeForDirectLoad(derivedSymbolReference->getSymbol()->getDataType()), 0, derivedSymbolReference);
            }
         }
      else
         firstOperand = storeOfDerivedInductionVariable->getFirstChild();

      // dec reference counts of loop test children
      //
      //if (firstChildIsConversion)
      //   firstChildIsConversion->recursivelyDecReferenceCount();
      //else
      //   firstChildOfLoopTestNode->recursivelyDecReferenceCount();

         //if (secondChildIsConversion)
         //secondChildIsConversion->recursivelyDecReferenceCount();
         //else
      //secondChildOfLoopTestNode->recursivelyDecReferenceCount();

      addNode->setLocalIndex(~0);
      TR::Node *secondOperand = TR::Node::createWithSymRef(secondChildOfLoopTestNode, comp()->il.opCodeForDirectLoad(newStore->getDataType()), 0, newSymbolReference);

      if (firstOperand->getDataType() == TR::Address)
         {
         TR_ComparisonTypes compareType = TR::ILOpCode::getCompareType(loopTestNode->getOpCodeValue());
         TR::ILOpCodes op = TR::ILOpCode::compareOpCode(TR::Address, compareType, true);
         TR::Node::recreate(loopTestNode, TR::ILOpCode(op).convertCmpToIfCmp());
         }
     // on 64-bit, change the loop test condition to equivalent longs
     //
     if (usingAladd && firstOperand->getType().isInt64())
        changeBranchFromIntToLong(loopTestNode);

    TR::Node *prevFirst = NULL;
    TR::Node *prevSecond = NULL;
    if (firstChildIsConversion && !usingAladd)
      {
      prevFirst = firstChildIsConversion->getFirstChild();
      if (firstChildIsConversion->getReferenceCount() == 1)
         firstChildIsConversion->setAndIncChild(0, firstOperand);
      else
         {
         TR::Node *dupOfFirstConversionChild = TR::Node::create(firstChildIsConversion->getOpCodeValue(), 1, firstOperand);
         loopTestNode->setAndIncChild(0, dupOfFirstConversionChild);
         firstChildIsConversion->decReferenceCount();
         // remember not to decrement the child's ref count in this case
         prevFirst = NULL;
         }
       }
    else
       {
       prevFirst = loopTestNode->getFirstChild();
       loopTestNode->setAndIncChild(0, firstOperand);
       }

    if (secondChildIsConversion && !usingAladd)
      {
      prevSecond = secondChildIsConversion->getFirstChild();

      if (secondChildIsConversion->getReferenceCount() == 1)
         secondChildIsConversion->setAndIncChild(0, secondOperand);
      else
         {
         TR::Node *dupOfSecondConversionChild = TR::Node::create(secondChildIsConversion->getOpCodeValue(), 1, secondOperand);
         loopTestNode->setAndIncChild(1, dupOfSecondConversionChild);
         secondChildIsConversion->decReferenceCount();
         // remember not to decrement the child's ref count in this case
         prevSecond = NULL;
         }
      }
   else
      {
      prevSecond = loopTestNode->getChild(1);
      loopTestNode->setAndIncChild(1, secondOperand);
      }

   if (prevFirst)
      prevFirst->recursivelyDecReferenceCount();
   if (prevSecond)
      prevSecond->recursivelyDecReferenceCount();
   //TR::Node::recreate(_storeTrees[_loopDrivingInductionVar]->getNode(), TR::treetop);

   // In a case of negative stride, exit branch needs to be reversed
   if (getMulTermConst(bestCandidate) < 0)
      {
      TR::Node::recreate(loopTestNode, loopTestNode->getOpCode().getOpCodeForSwapChildren());
      }
   }



bool TR_LoopStrider::foundValue(TR::Node *node, int32_t symRefNum, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadVar() &&
       (node->getSymbolReference()->getReferenceNumber() == symRefNum))
      return true;

   int32_t childNum;
   for (childNum=0;childNum < node->getNumChildren(); childNum++)
      {
      if (foundValue(node->getChild(childNum), symRefNum, visitCount))
         return true;
      }
   return false;
   }




bool TR_LoopStrider::unchangedValueNeededIn(TR::Block *exitBlock, int32_t nextInductionVariableNumber, bool &seenStore)
   {
   TR::TreeTop *cursorTree = exitBlock->getEntry();
   TR::TreeTop *exitTree = exitBlock->getExit();
   vcount_t visitCount = comp()->getVisitCount();

   while (cursorTree != exitTree)
      {
      TR::Node *cursorNode = cursorTree->getNode();
      if (cursorNode->getOpCode().isNullCheck() ||
          (cursorNode->getOpCodeValue() == TR::treetop))
        cursorNode = cursorNode->getFirstChild();

      //dumpOptDetails(comp(), "cursorNode : %p\n", cursorNode);
      if (foundValue(cursorNode, nextInductionVariableNumber, visitCount))
         return true;

      if (cursorNode->getOpCode().isStore() &&
          cursorNode->getSymbolReference()->getReferenceNumber() == nextInductionVariableNumber)
         {
         seenStore = true;
         return false;
         }

      cursorTree = cursorTree->getNextTreeTop();
      }
   return false;
   }


TR::Node* TR_LoopStrider::genLoad(TR::Node* node, TR::SymbolReference* symRef, bool isInternalPointer)
   {
   TR::Node* result;
   result = TR::Node::createWithSymRef(node,
      isInternalPointer ? TR::aload : (symRef->getSymbol()->getType().isInt64() ? TR::lload : TR::iload),
      0, symRef);

   result->setLocalIndex(~0);
   result->setReferenceCount(0);
   return result;
   }

void TR_LoopStrider::examineOpCodesForInductionVariableUse(TR::Node* node, TR::Node* parent, int32_t &childNum, int32_t &index, TR::Node* originalNode, TR::Node* replacingNode, TR::Node* linearTerm, TR::Node* mulTerm, TR::SymbolReference **newSymbolReference, TR::Block* loopInvariantBlock, TR::AutomaticSymbol* pinningArrayPointer, int64_t differenceInAdditiveConstants, bool &isInternalPointer, bool &downcastNode, bool &usingAladd)
   {
   if ((replacingNode->getOpCodeValue() == TR::iload ||
         replacingNode->getOpCodeValue() == TR::lload) &&
         (differenceInAdditiveConstants == 0))
      {
      bool nodeRememberedInLoadUsed = false;
      if (isInternalPointer)
         {
         node = originalNode;
         TR::Node::recreate(replacingNode, TR::aload);
         }

      if (_usesLoadUsedInLoopIncrement)
         {
         TR::Node *newLoad = getNewLoopIncrement(_storeTreeInfoForLoopIncrement, index);
         //traceMsg(comp(), "33 newLoad %p load %p index %d\n", newLoad, _storeTreeInfoForLoopIncrement->_load, index);
         if (newLoad)
            {
            TR::Node *oldNode = parent->getChild(childNum);


            if ((newLoad->getOpCodeValue() == TR::lload) && downcastNode)
               {
               // We are replacing oldNode(node) with newLoad. Must recursivelyDec all children of oldNode
               for (int i = 0; i < oldNode->getNumChildren(); i++)
                  {
                  oldNode->getChild(i)->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, newLoad);
               TR::Node::recreate(node, TR::l2i);
               node->setNumChildren(1);
               node->setReferenceCount(oldNode->getReferenceCount());
               downcastNode = false;
               }
            else
               {
               oldNode->recursivelyDecReferenceCount();
               node = newLoad;
               parent->setAndIncChild(childNum, node);
               }
            }
         else
            {
            int32_t i=0;
            for (;i<node->getNumChildren();i++)
               node->getChild(i)->recursivelyDecReferenceCount();
            _loadUsedInNewLoopIncrement[index] = node;
            if (_storeTreeInfoForLoopIncrement)
               addLoad(_storeTreeInfoForLoopIncrement, node, index);
            nodeRememberedInLoadUsed = true;
            TR::Node::recreate(node, replacingNode->getOpCodeValue());
            node->setSymbolReference(*newSymbolReference);
            node->setNumChildren(0);
            }
         }
      else
         {
         int32_t i=0;
         for (;i<node->getNumChildren();i++)
            node->getChild(i)->recursivelyDecReferenceCount();
         TR::Node::recreate(node, replacingNode->getOpCodeValue());
         node->setSymbolReference(*newSymbolReference);
         node->setNumChildren(0);
         }
      // downcast if required
      if (downcastNode && node->getOpCodeValue() == TR::lload)
         {
         TR::Node *l2iNode = node->duplicateTree();
         node->setNumChildren(1);
         l2iNode->setNumChildren(0);
         TR::Node::recreate(l2iNode, node->getOpCodeValue());
         l2iNode->setReferenceCount(1);
         node->setChild(0, l2iNode);
         TR::Node::recreate(node, TR::l2i);
         // check if the parent is a long before adding an l2i
         //
         if (parent &&
            parent->getType().isInt64() &&
            // Exclude these guys, even though they're long typed their children shouldn't be
            // For shifts, only the 2nd child shouldn't be long
            (!parent->getOpCode().isShift() || node != parent->getSecondChild()) &&
            !parent->getOpCode().isCall() &&
            !parent->getOpCode().isConversion() &&
            parent->getOpCodeValue() != TR::lternary)
            {
            TR::Node *i2lNode = TR::Node::create(TR::i2l, 1, node);
            node->decReferenceCount();
            parent->setAndIncChild(childNum, i2lNode);
            }
         if (nodeRememberedInLoadUsed)
            {
            nodeRememberedInLoadUsed = false;
            _loadUsedInNewLoopIncrement[index] = node->getFirstChild();
            if (_storeTreeInfoForLoopIncrement)
               addLoad(_storeTreeInfoForLoopIncrement, node->getFirstChild(), index);
            }
         }
      }
   else
      {
      bool canHoistAdditiveTerm = false;
      bool nodeRememberedInLoadUsed = false;
      TR::Node *newLoad = NULL;
      if (_usesLoadUsedInLoopIncrement)
         {
         newLoad = getNewLoopIncrement(_storeTreeInfoForLoopIncrement, index);
         //traceMsg(comp(), "22 at node %p newLoad %p load %p index %d\n", node, newLoad, _storeTreeInfoForLoopIncrement->_load, index);
         if (!newLoad)
            {
            newLoad = genLoad(node, *newSymbolReference, isInternalPointer);
            nodeRememberedInLoadUsed = true;
            _loadUsedInNewLoopIncrement[index] = newLoad;
            if (_storeTreeInfoForLoopIncrement)
               addLoad(_storeTreeInfoForLoopIncrement, newLoad, index);
            }
         }
      else
         {
         newLoad = genLoad(node, *newSymbolReference, isInternalPointer);
         }

      TR::Node *adjustmentNode = NULL;
      if (replacingNode->getOpCodeValue() != TR::iload &&
         replacingNode->getOpCodeValue() != TR::lload)
         {
         TR::Node *mulNode, *constantNode, *indexNode;
         mulNode = constantNode = indexNode = NULL;

         if (usingAladd)
            {
            mulNode = TR::Node::create(node, TR::lmul, 2);
            indexNode = linearTerm;
            if (indexNode->getOpCodeValue() == TR::i2l)
               indexNode = indexNode->getFirstChild()->getSecondChild();
            else
               indexNode = indexNode->getSecondChild();

            if (/* debug("enableRednIndVarElim") && */
               indexNode->getOpCode().isLoadVarDirect() &&
               indexNode->getSymbol()->isAutoOrParm() &&
               _neverWritten->get(indexNode->getSymbolReference()->getReferenceNumber()))
               {
               canHoistAdditiveTerm = true;
               }

            if (!mulTerm->getOpCode().isLoadConst())
               canHoistAdditiveTerm = true;

            // sign-extension required on 64-bit
            if (!indexNode->getType().isInt64())
               {
               TR::Node *i2lNode = TR::Node::create(TR::i2l, 1, indexNode);
               indexNode = i2lNode;
               }
            constantNode = duplicateMulTermNode(index, node, TR::Int64);
            }
         else
            {
            indexNode = linearTerm->getSecondChild();
            if (/* debug("enableRednIndVarElim") && */
               indexNode->getOpCode().isLoadVarDirect() &&
               indexNode->getSymbol()->isAutoOrParm() &&
               _neverWritten->get(indexNode->getSymbolReference()->getReferenceNumber()))
               {
               canHoistAdditiveTerm = true;
               }

            if (!mulTerm->getOpCode().isLoadConst())
               canHoistAdditiveTerm = true;

            mulNode = TR::Node::create(node,
               replacingNode->getType().isInt64() ? TR::lmul : TR::imul, 2);
            constantNode = duplicateMulTermNode(index, node, replacingNode->getDataType());
            }



         mulNode->setChild(0, indexNode);
         mulNode->getFirstChild()->incReferenceCount();
         mulNode->setChild(1, constantNode);
         mulNode->getSecondChild()->setReferenceCount(1);
         mulNode->setLocalIndex(~0);
         mulNode->getSecondChild()->setLocalIndex(~0);
         adjustmentNode = mulNode;
         }

      if (isInternalPointer)
         {
         node = originalNode;
         if (usingAladd)
            {
            if (replacingNode->getOpCodeValue() == TR::lsub)
               {
               TR::Node *negateConstantNode = TR::Node::create(node, TR::lconst);
               negateConstantNode->setLongInt(-1);
               adjustmentNode = TR::Node::create(TR::lmul, 2, adjustmentNode, negateConstantNode);
               adjustmentNode->setLocalIndex(~0);
               adjustmentNode->getSecondChild()->setLocalIndex(~0);
               }
            TR::Node::recreate(replacingNode, TR::aladd);
            replacingNode->setIsInternalPointer(true);
            }
         else
            {
            if (replacingNode->getOpCodeValue() == TR::isub)
               {
               adjustmentNode = TR::Node::create(TR::imul, 2, adjustmentNode,
                  TR::Node::create(node, TR::iconst, 0, -1));
               adjustmentNode->setLocalIndex(~0);
               adjustmentNode->getSecondChild()->setLocalIndex(~0);
               }
            TR::Node::recreate(replacingNode, TR::aiadd);
            replacingNode->setIsInternalPointer(true);
            }
         }
      else if (replacingNode->getOpCodeValue() == TR::iload ||
         replacingNode->getOpCodeValue() == TR::lload)
         {
         if (usingAladd || (replacingNode->getOpCodeValue() == TR::lload))
            TR::Node::recreate(replacingNode, TR::ladd);
         else
            TR::Node::recreate(replacingNode, TR::iadd);
         }

      if (differenceInAdditiveConstants != 0)
         {
         TR::Node *differenceInAdditiveConstantsNode;
         TR::ILOpCodes op;
         if (adjustmentNode)
            {
            if (usingAladd || (adjustmentNode->getType().isInt64()))
               {
               differenceInAdditiveConstantsNode = TR::Node::create(node, TR::lconst);
               differenceInAdditiveConstantsNode->setLongInt((int64_t) differenceInAdditiveConstants);
               op = replacingNode->getOpCode().isAdd() ? TR::ladd : TR::lsub;
               }
            else
               {
               differenceInAdditiveConstantsNode = TR::Node::create(node, TR::iconst, 0, differenceInAdditiveConstants);
               op = replacingNode->getOpCode().isAdd() ? TR::iadd : TR::isub;
               }
            adjustmentNode = TR::Node::create(op, 2, adjustmentNode, differenceInAdditiveConstantsNode);
            }
         else
            {
            if (usingAladd || (replacingNode->getType().isInt64()))
               {
               adjustmentNode = TR::Node::create(node, TR::lconst);
               adjustmentNode->setLongInt((int64_t) differenceInAdditiveConstants);
               }
            else
               adjustmentNode = TR::Node::create(node, TR::iconst, 0,
                  differenceInAdditiveConstants);
            }
         }

      int32_t i=0;
      for (;i<node->getNumChildren();i++)
         node->getChild(i)->recursivelyDecReferenceCount();

      TR::Node::recreate(node, replacingNode->getOpCodeValue());
      if (node->getOpCodeValue() == TR::aiadd || node->getOpCodeValue() == TR::aladd)
         {
         node->setIsInternalPointer(true);

         if (!pinningArrayPointer->isInternalPointer())
            {
            node->setPinningArrayPointer(pinningArrayPointer);
            pinningArrayPointer->setPinningArrayPointer();
            }
         else
            node->setPinningArrayPointer(pinningArrayPointer->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
         }

      node->setNumChildren(2);
      node->setChild(0, newLoad);
      newLoad->incReferenceCount();

      if (canHoistAdditiveTerm)
         {
         TR::SymbolReference *newLoopInvariant = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), adjustmentNode->getDataType(), false);
         TR::Node *newLoad = TR::Node::createLoad(node, newLoopInvariant);
         newLoad->setLocalIndex(~0);
         TR::Node *newStore = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(adjustmentNode->getDataType()), 1, 1, adjustmentNode->duplicateTree(), newLoopInvariant);

         TR::TreeTop *placeHolderTree = loopInvariantBlock->getLastRealTreeTop();
         if (!placeHolderTree->getNode()->getOpCode().isBranch())
            placeHolderTree = loopInvariantBlock->getExit();
         TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();

         newStore->setLocalIndex(~0);
         TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
         prevTree->join(newStoreTreeTop);
         newStoreTreeTop->join(placeHolderTree);

         //printf("Reached here in %s\n", comp()->signature());
         dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted initialization tree : %p for new loop invariant symRef #%d\n", newStoreTreeTop->getNode(), newLoopInvariant->getReferenceNumber());
         int32_t childNum;
         for (childNum=0; childNum < adjustmentNode->getNumChildren(); childNum++)
            adjustmentNode->getChild(childNum)->recursivelyDecReferenceCount();
         adjustmentNode = newLoad;
         }

      node->setChild(1, adjustmentNode);
      adjustmentNode->setReferenceCount(1);
      // downcast if required
      if (downcastNode && newLoad->getType().isInt64())
         {
         TR::Node *oldFirstChild = node->getFirstChild();
         TR::Node *l2iNode = node->duplicateTree();
         l2iNode->setReferenceCount(1);
         // dec the reference counts of the node's children
         node->getFirstChild()->decReferenceCount();
         node->getSecondChild()->recursivelyDecReferenceCount();
         node->setNumChildren(1);
         node->setChild(0, l2iNode);
         TR::Node::recreate(node, TR::l2i);
         if (nodeRememberedInLoadUsed)
            {
            nodeRememberedInLoadUsed = false;
            _loadUsedInNewLoopIncrement[index] = node->getFirstChild()->getFirstChild();
            if (_storeTreeInfoForLoopIncrement)
               addLoad(_storeTreeInfoForLoopIncrement, node->getFirstChild()->getFirstChild(), index);
            }
         else
            {
            TR::Node *newFirstChild = node->getFirstChild()->getFirstChild();
            node->getFirstChild()->setAndIncChild(0, oldFirstChild);
            newFirstChild->recursivelyDecReferenceCount();
            }
         }
      }
   }

bool TR_LoopStrider::examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *parent, int32_t childNum, TR::Node *node, vcount_t visitCount, TR::SymbolReference **newSymbolReference)
   {
   static const char *onlyConstStride = feGetEnv("TR_onlyConstStride");

   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                      true : false;

   bool seenInductionVariableComputation = false;
   bool examineChildren = true;
   if (node->getVisitCount() == visitCount)
      examineChildren = false;

   node->setVisitCount(visitCount);

   //traceMsg(comp(), "Examining node %p\n", node);

   bool isInternalPointer = false;
   int32_t internalPointerSymbol = -1;
   TR::AutomaticSymbol *pinningArrayPointer = NULL;
   TR::Node *originalNode = NULL;
   if (cg()->supportsInternalPointers() &&
       (node->isInternalPointer()) &&
       node->getFirstChild()->getOpCode().isLoadVar() &&
       node->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm() &&
       //node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
       _neverWritten->get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()) &&
       (comp()->getStartBlock()->getFrequency() < 500))
      {
          //printf("Creating internal ptr in %s\n", comp()->signature());
      isInternalPointer = true;
      internalPointerSymbol = node->getFirstChild()->getSymbolReference()->getReferenceNumber();
      if (node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto())
         pinningArrayPointer = node->getFirstChild()->getSymbolReference()->getSymbol()->castToAutoSymbol();
      else
         {
         SymRefPair *pair = _parmAutoPairs;
         while (pair)
            {
            if (pair->_indexSymRef == node->getFirstChild()->getSymbolReference())
               {
               pinningArrayPointer = pair->_derivedSymRef->getSymbol()->castToAutoSymbol();
               internalPointerSymbol = pair->_derivedSymRef->getReferenceNumber();
               break;
               }
            pair = pair->_next;
            }
         }

      originalNode = node;
      node = node->getSecondChild();
      if (node->getOpCodeValue() == TR::l2i)
         {
         node->setVisitCount(visitCount);
         node = node->getFirstChild();
         }
      }

   // support for TR::aladd
   if ((node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::isub) ||
       (node->getOpCodeValue() == TR::ladd || node->getOpCodeValue() == TR::lsub))
      {
      if (isExprLoopInvariant(node->getSecondChild()))
         {
         // For TR::aladd's, if the additive constant might exceed INT_MAX; do nothing
         if (usingAladd)
            if (node->getSecondChild()->getLongInt() > INT_MAX)
               return seenInductionVariableComputation;

         if ((node->getFirstChild()->getOpCodeValue() == TR::imul || node->getFirstChild()->getOpCodeValue() == TR::ishl) ||
             (node->getFirstChild()->getOpCodeValue() == TR::lmul || node->getFirstChild()->getOpCodeValue() == TR::lshl))
            {
            bool shift = node->getFirstChild()->getOpCodeValue() == TR::ishl ||
                         node->getFirstChild()->getOpCodeValue() == TR::lshl;

            bool downcastNode = (node->getType().isInt32()) ? true : false;
            _usesLoadUsedInLoopIncrement = false;
            _storeTreeInfoForLoopIncrement = NULL;

            //traceMsg(comp(), "looking for store at node = %p\n", node);

            TR::Node *replacingNode = NULL, *mulTerm = NULL, *linearTerm = NULL;
            if ((replacingNode = findReplacingNode(node->getFirstChild()->getFirstChild(), usingAladd, -1)))
               {
               linearTerm = node->getFirstChild()->getFirstChild();
               mulTerm = node->getFirstChild()->getSecondChild();
               }
            else if (!shift && (replacingNode = findReplacingNode(node->getFirstChild()->getSecondChild(), usingAladd, -1)))
               {
               linearTerm = node->getFirstChild()->getSecondChild();
               mulTerm = node->getFirstChild()->getFirstChild();
               }

            if (!mulTerm ||
                !isExprLoopInvariant(mulTerm) ||
                !mulTerm->getType().isIntegral() ||   // we were checking for signed here -- i made the test a bit weaker, but I don't think we can reach here with unsigned
                (!mulTerm->getOpCode().isLoadConst() && shift))
                replacingNode = NULL;

            if (onlyConstStride)
               if (!mulTerm->getOpCode().isLoadConst()) replacingNode = NULL;

            if (replacingNode)
               {
               *newSymbolReference = NULL;
               int32_t index = findNewInductionVariable(node, newSymbolReference, true, internalPointerSymbol);

               bool canCreateNewSymRef = false;
               if (*newSymbolReference == NULL)
                  {
                  if (isInternalPointer &&
                      ((comp()->getSymRefTab()->getNumInternalPointers() >= maxInternalPointers()) ||
                       (comp()->cg()->canBeAffectedByStoreTagStalls() &&
                           _numInternalPointerOrPinningArrayTempsInitialized >= MAX_INTERNAL_POINTER_AUTOS_INITIALIZED)))
                     return seenInductionVariableComputation;
                  else if (!isInternalPointer &&
                           !node->isNonNegative() &&
                           !node->isNonPositive())
                     return seenInductionVariableComputation;

                  canCreateNewSymRef = true;
                  }

               if (canCreateNewSymRef)
                  {
                  TR::DataType dataType = findDataType(replacingNode, usingAladd, isInternalPointer);
                  TR_ASSERT(dataType, "dataType cannot be NoType\n");

                  *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType, isInternalPointer);
                  if (isInternalPointer && !pinningArrayPointer)
                     {
                     TR::SymbolReference *newPinningArray = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, false);
                     pinningArrayPointer = newPinningArray->getSymbol()->castToAutoSymbol();
                     createParmAutoPair(comp()->getSymRefTab()->getSymRef(internalPointerSymbol), newPinningArray);

                     TR::Node *newAload = TR::Node::createLoad(node, comp()->getSymRefTab()->getSymRef(internalPointerSymbol));
                     newAload->setLocalIndex(~0);
                     TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, newAload, newPinningArray);
                     newStore->setLocalIndex(~0);
                     internalPointerSymbol = newPinningArray->getReferenceNumber();
                     placeStore(newStore, loopInvariantBlock);
                     dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted initialization tree : %p for new symRef #%d\n", newStore, newPinningArray->getReferenceNumber());
                     _numInternalPointerOrPinningArrayTempsInitialized++;
                     }

                  _newTempsCreated = true;
                  TR::Symbol *symbol = (*newSymbolReference)->getSymbol();
                  if (!isInternalPointer)
                     _newNonAddressTempsCreated = true;
                  else if (pinningArrayPointer)
                     setInternalPointer(symbol, pinningArrayPointer);

                  index = _nextExpression;
                  populateLinearEquation(node, _loopDrivingInductionVar, (*newSymbolReference)->getReferenceNumber(), internalPointerSymbol, mulTerm);
                  }

               seenInductionVariableComputation = true;
               examineChildren = false;

               int64_t differenceInAdditiveConstants = 0;
               if (node->getSecondChild()->getOpCode().isLoadConst())
                  {
                  TR_ASSERT(isAdditiveTermConst(index), "Both terms should be constants %d", index);
                  int64_t additiveConstant = GET64BITINT(node->getSecondChild());
                  if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::ladd)
                     differenceInAdditiveConstants = additiveConstant - getAdditiveTermConst(index);
                  else // if (node->getSecondChild()->getOpCodeValue() == TR::isub)
                     differenceInAdditiveConstants = (-1*additiveConstant) - getAdditiveTermConst(index);
                  }

               examineOpCodesForInductionVariableUse(node, parent, childNum, index, originalNode, replacingNode, linearTerm, mulTerm, newSymbolReference, loopInvariantBlock, pinningArrayPointer, differenceInAdditiveConstants, isInternalPointer, downcastNode, usingAladd);


               dumpOptDetails(comp(), "O^O INDUCTION VARIABLE ANALYSIS: (add/sub) Replaced node : %p "
                              "to (%s) by load of symbol #%d\n",
                              node, comp()->getDebug()->getName(node->getOpCodeValue()),
                              (*newSymbolReference)->getReferenceNumber());
               }
            }
         }
      else
         {
         _usesLoadUsedInLoopIncrement = false;
         _storeTreeInfoForLoopIncrement = NULL;

         // simply called to set _loads on the store info
         // to properly handle the case when we only have 2 stores
         // in a block and no load of the ind var under an expression
         // where striding can be done.
         //
         TR::Node *replacingNode = findReplacingNode(node, usingAladd, -1);
         }
      }
   else if ((node->getOpCodeValue() == TR::imul || node->getOpCodeValue() == TR::ishl) ||
            (node->getOpCodeValue() == TR::lmul || node->getOpCodeValue() == TR::lshl))
      {
      bool shift = node->getOpCodeValue() == TR::ishl ||
                   node->getOpCodeValue() == TR::lshl;

      bool downcastNode = (node->getType().isInt32()) ? true : false;
      _usesLoadUsedInLoopIncrement = false;
      _storeTreeInfoForLoopIncrement = NULL;


      TR::Node *replacingNode = NULL, *mulTerm = NULL, *linearTerm = NULL;
      if ((replacingNode = findReplacingNode(node->getFirstChild(), usingAladd, -1)))
         {
         linearTerm = node->getFirstChild();
         mulTerm = node->getSecondChild();
         }
      else if (!shift && (replacingNode = findReplacingNode(node->getSecondChild(), usingAladd, -1)))
         {
         linearTerm = node->getSecondChild();
         mulTerm = node->getFirstChild();
         }

      if (!mulTerm ||
         !isExprLoopInvariant(mulTerm) ||
         !mulTerm->getType().isIntegral() || // I changed isSignedInt->isIntegral -- but I don't think unsigned types ever reach here
         (!mulTerm->getOpCode().isLoadConst() && shift))
         replacingNode = NULL;

      if (onlyConstStride)
         if (!mulTerm->getOpCode().isLoadConst()) replacingNode = NULL;

      if (replacingNode)
         {
         *newSymbolReference = NULL;
         int32_t index = findNewInductionVariable(node, newSymbolReference, false, internalPointerSymbol);


         bool canCreateNewSymRef = false;
         if (*newSymbolReference == NULL)
            {
            if (isInternalPointer &&
                  ((comp()->getSymRefTab()->getNumInternalPointers() >= maxInternalPointers()) ||
                     (comp()->cg()->canBeAffectedByStoreTagStalls() &&
                     _numInternalPointerOrPinningArrayTempsInitialized >= MAX_INTERNAL_POINTER_AUTOS_INITIALIZED)))
               return seenInductionVariableComputation;
            else if (!isInternalPointer &&
                     !node->isNonNegative() &&
                     !node->isNonPositive())
               return seenInductionVariableComputation;

            canCreateNewSymRef = true;
            }

         if (canCreateNewSymRef)
            {

            TR::DataType dataType = findDataType(replacingNode, usingAladd, isInternalPointer);
            TR_ASSERT(dataType, "dataType cannot be TR::NoType\n");

            *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType, isInternalPointer);
            if (isInternalPointer && !pinningArrayPointer)
               {
               TR::SymbolReference *newPinningArray = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, false);
               pinningArrayPointer = newPinningArray->getSymbol()->castToAutoSymbol();
               createParmAutoPair(comp()->getSymRefTab()->getSymRef(internalPointerSymbol), newPinningArray);

               TR::Node *newAload = TR::Node::createLoad(node, comp()->getSymRefTab()->getSymRef(internalPointerSymbol));
               newAload->setLocalIndex(~0);
               TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, newAload, newPinningArray);
               internalPointerSymbol = newPinningArray->getReferenceNumber();
               newStore->setLocalIndex(~0);
               placeStore(newStore, loopInvariantBlock);
               dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted initialization tree : %p for new symRef #%d\n", newStore, newPinningArray->getReferenceNumber());
               _numInternalPointerOrPinningArrayTempsInitialized++;
               }

            _newTempsCreated = true;
            TR::Symbol *symbol = (*newSymbolReference)->getSymbol();
            if (!isInternalPointer)
               _newNonAddressTempsCreated = true;
            else if (pinningArrayPointer)
               setInternalPointer(symbol, pinningArrayPointer);

            setAdditiveTermNode(NULL, _nextExpression);
            index = _nextExpression;
            populateLinearEquation(node, _loopDrivingInductionVar, (*newSymbolReference)->getReferenceNumber(), internalPointerSymbol, mulTerm);
            }

         seenInductionVariableComputation = true;
         examineChildren = false;

         int32_t differenceInAdditiveConstants = 0;
         if (isAdditiveTermConst(index))
            differenceInAdditiveConstants = (int32_t)(-1*getAdditiveTermConst(index));

         examineOpCodesForInductionVariableUse(node, parent, childNum, index, originalNode, replacingNode, linearTerm, mulTerm, newSymbolReference, loopInvariantBlock, pinningArrayPointer, differenceInAdditiveConstants, isInternalPointer, downcastNode, usingAladd);

         dumpOptDetails(comp(), "O^O INDUCTION VARIABLE ANALYSIS: (mul/shl) Replaced node : "
                        "%p to (%s) by load of symbol #%d\n",
                        node, comp()->getDebug()->getName(node->getOpCodeValue()),
                        (*newSymbolReference)->getReferenceNumber());
         }
      }

   if (examineChildren)
       {
       int32_t i;
       for (i=0;i<node->getNumChildren();i++)
          {
          if (examineTreeForInductionVariableUse(loopInvariantBlock, node, i, node->getChild(i), visitCount, newSymbolReference))
             seenInductionVariableComputation = true;
          }
       }

   return seenInductionVariableComputation;
   }



void TR_LoopStrider::populateLinearEquation(TR::Node *node, int32_t loopDrivingInductionVar, int32_t derivedInductionVar, int32_t internalPointerSymbol, TR::Node *invariantMultiplicationTerm)
   {
//   traceMsg(comp(), "populate node %p number %d\n", node, _nextExpression);

   TR_ASSERT(_nextExpression < _numberOfLinearExprs, "Number of linear expressions exceeded the estimate\n");

   _linearEquations[_nextExpression][0] = loopDrivingInductionVar;
   _linearEquations[_nextExpression][1] = derivedInductionVar;
   _linearEquations[_nextExpression][4] = internalPointerSymbol;
   setAdditiveTermNode(NULL, _nextExpression);

   if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::ladd)
      {
      setAdditiveTermNode(node->getSecondChild(), _nextExpression);

      node = node->getFirstChild();
      }
   else if (node->getOpCodeValue() == TR::isub || node->getOpCodeValue() == TR::lsub)
      {
      bool intType = node->getOpCodeValue() == TR::isub;
      TR::Node *addTerm;

      if (node->getSecondChild()->getOpCode().isLoadConst())
	 {
         int64_t additiveConstant = GET64BITINT(node->getSecondChild());
         if(intType)
            addTerm = TR::Node::create(node,TR::iconst, 0, -1*(int32_t)additiveConstant);
         else
            {
            addTerm = TR::Node::create(node, TR::lconst, 0, 0);
            addTerm->setLongInt((int64_t)(-1*additiveConstant));
            }
         }
      else
         {
         TR::Node *minusOne = 0;
         if(intType)
            minusOne = TR::Node::create(node,TR::iconst, 0, -1);
         else
            {
            minusOne = TR::Node::create(node, TR::lconst, 0, 0);
            minusOne->setLongInt((int64_t)(-1));
            }

         addTerm = TR::Node::create(node, intType ? TR::imul : TR::lmul, 2);
         addTerm->setAndIncChild(0, node->getSecondChild()->duplicateTree());
         addTerm->setAndIncChild(1, minusOne);
         }

      setAdditiveTermNode(addTerm, _nextExpression);
      node = node->getFirstChild();
      }


   if (node->getOpCodeValue() == TR::imul ||
       node->getOpCodeValue() == TR::lmul)
      {
      setMulTermNode(invariantMultiplicationTerm, _nextExpression);
      }
   else if (node->getOpCodeValue() == TR::ishl ||
            node->getOpCodeValue() == TR::lshl)
      {
      TR_ASSERT(invariantMultiplicationTerm->getOpCodeValue() == TR::iconst, "Handling only constant shifts");
      int32_t shiftAmount = node->getSecondChild()->getInt();
      int32_t mulAmount;
      if (shiftAmount > 0)
         mulAmount = 2<<(shiftAmount-1);
      else
         mulAmount = 1;
      setMulTermNode(TR::Node::create(node, TR::iconst, 0, mulAmount), _nextExpression);
      }

   _nextExpression++;
   }




void TR_LoopStrider::setInternalPointer(TR::Symbol *symbol, TR::AutomaticSymbol *pinningArrayPointer)
   {
   _numInternalPointers++;

   if (!pinningArrayPointer->isInternalPointer())
      {
      symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer);
      pinningArrayPointer->setPinningArrayPointer();
      }
   else
      symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
   }



void TR_LoopStrider::placeStore(TR::Node *newStore, TR::Block *loopInvariantBlock)
   {
   TR::TreeTop *placeHolderTree = loopInvariantBlock->getEntry();
   TR::TreeTop *nextTree = placeHolderTree->getNextTreeTop();
   TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
   placeHolderTree->join(newStoreTreeTop);
   newStoreTreeTop->join(nextTree);
   }


// node is linear multiplication term
TR::Node *TR_LoopStrider::findReplacingNode(TR::Node *node, bool usingAladd, int32_t k)
   {
   TR::Node *replacingNode = NULL;
   if (usingAladd)
      {
      if (node->getOpCodeValue() == TR::i2l)
         replacingNode = isExpressionLinearInInductionVariable(node->getFirstChild(), k);
      else
         replacingNode = isExpressionLinearInInductionVariable(node, k);
      }
   else
      replacingNode = isExpressionLinearInInductionVariable(node, k);
   return replacingNode;
   }


TR::DataType TR_LoopStrider::findDataType(TR::Node *node, bool usingAladd, bool isInternalPointer)
   {
   TR::DataType dataType = TR::NoType;
   if (!isInternalPointer)
      {
      // generate long symRefs if on 64-bit
      //
      if (usingAladd)
         dataType = TR::Int64;
      else
         {
         if (node->getType().isInt64())
            dataType = TR::Int64;
         else
            dataType = TR::Int32;
         }
      }
   else
      dataType = TR::Address;

   return dataType;
   }


void TR_LoopStrider::createParmAutoPair(TR::SymbolReference *parmSymRef, TR::SymbolReference *autoSymRef)
   {
   SymRefPair *pair = (SymRefPair *)trMemory()->allocateStackMemory(sizeof(SymRefPair));
   pair->_indexSymRef = parmSymRef;
   pair->_derivedSymRef = autoSymRef;
   pair->_isConst = false;
   pair->_next = _parmAutoPairs;
   _parmAutoPairs = pair;
   }


void TR_LoopStrider::addLoad(TR_StoreTreeInfo *info, TR::Node *load, int32_t index)
   {
   TR_NodeIndexPair *head = info->_loads;
   TR_NodeIndexPair *cursor = head;

   while (cursor)
      {
      if (cursor->_index == index)
         {
         cursor->_node = load;
         return;
         }

      cursor = cursor->_next;
      }

   TR_NodeIndexPair *pair = new (trStackMemory()) TR_NodeIndexPair(load, index, NULL);
   pair->_next = head;
   info->_loads = pair;
   info->_load = load;
   }


TR::Node *TR_LoopStrider::placeInitializationTreeInLoopInvariantBlock(TR_BlockStructure *loopInvariantBlock,
      TR::SymbolReference *inductionVarSymRef, TR::SymbolReference *newSymbolReference, int32_t k,
      TR::SymbolReferenceTable *symRefTab)
   {
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;
   //
   // Place the initialization tree for the derived induction variable
   // in the loop invariant block
   //
   TR::Block *b = loopInvariantBlock->getBlock();
   TR::TreeTop *placeHolderTree = b->getLastRealTreeTop();
   if (!placeHolderTree->getNode()->getOpCode().isBranch())
      placeHolderTree = b->getExit();

   TR::Node *placeHolderNode = placeHolderTree->getNode();
   TR::Node *newLoad = TR::Node::createLoad(placeHolderNode, inductionVarSymRef);
   newLoad->setLocalIndex(~0);

   // Create the multiply node first; e.g. 4*i
   //
   TR::Node *mulNode;
   if (usingAladd)
      {
      TR::Node *multiplicativeConstant = duplicateMulTermNode(k, placeHolderNode, TR::Int64);

      if (!newLoad->getType().isInt64())
         {
         TR::Node *i2lNode = TR::Node::create(placeHolderNode, TR::i2l, 1);
         i2lNode->setChild(0, newLoad);
         i2lNode->getFirstChild()->incReferenceCount();
         newLoad = i2lNode;
         }
      mulNode = TR::Node::create(TR::lmul, 2, newLoad, multiplicativeConstant);
      }
   else
      {
      mulNode = TR::Node::create(newLoad->getType().isInt64() ? TR::lmul : TR::imul, 2, newLoad,
         duplicateMulTermNode(k, placeHolderNode, newLoad->getDataType()));
      }
   mulNode->setLocalIndex(~0);
   mulNode->getSecondChild()->setLocalIndex(~0);
   TR::Node *addNode = NULL;
   if (getAdditiveTermNode(k) != NULL)
      {
      if (usingAladd)
         {
  	 addNode = TR::Node::create(TR::ladd, 2, mulNode,
                                   duplicateAdditiveTermNode(k, placeHolderNode, TR::Int64));
         }
      else
         {
  	 addNode = TR::Node::create(mulNode->getType().isInt64() ? TR::ladd : TR::iadd, 2, mulNode,
                                   duplicateAdditiveTermNode(k, placeHolderNode, mulNode->getDataType()));
         }
      addNode->setLocalIndex(~0);
      addNode->getSecondChild()->setLocalIndex(~0);
      }
   else
      addNode = mulNode;

   //Create the add node now; e.g. (4*i+16)
   //
   TR::Node *newStore = NULL;
   if (_linearEquations[k][4] > -1)
      {
      TR::Node *newAload = TR::Node::createLoad(placeHolderNode, symRefTab->getSymRef((int32_t)_linearEquations[k][4]));
      newAload->setLocalIndex(~0);
      addNode = (usingAladd) ?
                   TR::Node::create(TR::aladd, 2, newAload, addNode) :
                   TR::Node::create(TR::aiadd, 2, newAload, addNode);
      addNode->setIsInternalPointer(true);

      if (!newAload->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer())
         {
         addNode->setPinningArrayPointer(newAload->getSymbolReference()->getSymbol()->castToAutoSymbol());
         newAload->getSymbolReference()->getSymbol()->setPinningArrayPointer();
         }
      else
         addNode->setPinningArrayPointer(newAload->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

      addNode->setLocalIndex(~0);
      addNode->getSecondChild()->setLocalIndex(~0);
      newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, addNode, newSymbolReference);
      }
   else
      {
      TR::ILOpCodes op = (usingAladd || addNode->getType().isInt64()) ? TR::lstore : TR::istore;
      newStore = TR::Node::createWithSymRef(op, 1, 1, addNode, newSymbolReference);
      }

   // Insert the initialization tree
   //
   newStore->setLocalIndex(~0);
   TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
   placeHolderTree->getPrevTreeTop()->join(newStoreTreeTop);
   newStoreTreeTop->join(placeHolderTree);

   dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted initialization tree : %p for new symRef #%d\n", newStoreTreeTop->getNode(), newSymbolReference->getReferenceNumber());
   return placeHolderNode;
   }




TR::Node *TR_LoopStrider::placeNewInductionVariableIncrementTree(TR_BlockStructure *loopInvariantBlock, TR::SymbolReference *inductionVarSymRef, TR::SymbolReference *newSymbolReference, int32_t k, TR::SymbolReferenceTable *symRefTab, TR::Node *placeHolderNode, int32_t loopTestHasSymRef)
   {
   //
   // Create and add the loop increment step for derived induction variable
   //
   // First check if we MUST common the load in prior uses of the derived induction
   // variable with the current use in loop increment step; this may be required
   // if the load used in the increment of the original induction variable i
   // was commoned with prior loads of i
   //
   TR::Node *newLoad = NULL;
   TR::Node *returnValue = NULL;
   int32_t loopTestBlockNum = _loopTestTree->getNextTreeTop()->getNode()->getBlock()->startOfExtendedBlock()->getNumber();

   int32_t symRefNum = inductionVarSymRef->getReferenceNumber();
   auto lookup = _storeTreesList->find(symRefNum);
   if (lookup != _storeTreesList->end())
      {
      List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
      ListIterator<TR_StoreTreeInfo> si(storeTreesList);
      ListElement<TR_StoreTreeInfo> *storeTree;
      storeTree = storeTreesList->getListHead();
      TR_ScratchList<TR::Node> seenLoads(trMemory());
      TR_ScratchList<TR_NodeIndexPair> seenLoadPairs(trMemory());
      for (;storeTree != NULL;)
         {
         TR_NodeIndexPair *loadPair = storeTree->getData()->_loads;
         while (loadPair)
            {
            if (loadPair->_index == k)
               {
               //traceMsg(comp(), "store %p old %p\n", storeTree->getData()->_tt->getNode(), storeTree->getData()->_loadUsedInLoopIncrement);
               seenLoads.add(storeTree->getData()->_loadUsedInLoopIncrement);
               seenLoadPairs.add(loadPair);
               }

            loadPair = loadPair->_next;
            }


         storeTree = storeTree->getNextElement();
         }

      storeTree = storeTreesList->getListHead();
      for (;storeTree != NULL;)
         {
         TR::Node *loadNode = NULL;

         ListElement<TR::Node> *seenLoad = seenLoads.getListHead();
         ListElement<TR_NodeIndexPair> *seenLoadPair = seenLoadPairs.getListHead();
         while (seenLoad)
            {
            //traceMsg(comp(), "store %p old %p seen load %p\n", storeTree->getData()->_tt->getNode(), storeTree->getData()->_loadUsedInLoopIncrement, seenLoad->getData());
            if (seenLoad->getData() == storeTree->getData()->_loadUsedInLoopIncrement)
               {
               loadNode = seenLoadPair->getData()->_node;
               break;
               }

            seenLoad = seenLoad->getNextElement();
            seenLoadPair = seenLoadPair->getNextElement();
            }

         if (!loadNode)
            {
            TR_NodeIndexPair *loadPair = storeTree->getData()->_loads;
            while (loadPair)
               {
               //traceMsg(comp(), "store %p old %p seen load %p\n", storeTree->getData()->_tt->getNode(), storeTree->getData()->_loadUsedInLoopIncrement, loadPair->_node);

               if (loadPair->_index == k)
                  {
                  //seenLoads.add(storeTree->getData()->_loadUsedInLoopIncrement);
                  //seenLoadPairs.add(loadPair);
                  loadNode = loadPair->_node;
                  break;
                  }

               loadPair = loadPair->_next;
               }
            }

         if (loadNode)
             newLoad = loadNode;
         else
            {
            if (_linearEquations[k][4] > -1)
               newLoad = TR::Node::createLoad(placeHolderNode, newSymbolReference);
            else
               newLoad = TR::Node::createLoad(placeHolderNode, newSymbolReference);
            newLoad->setLocalIndex(~0);
            // remember this load here in case the loop test tree needs to refer back to this load
            // instead of the new incremented/decremented value

            storeTree->getData()->_load = newLoad;

            TR_NodeIndexPair *head = storeTree->getData()->_loads;
            TR_NodeIndexPair *pair = new (trStackMemory()) TR_NodeIndexPair(newLoad, k, NULL);
            pair->_next = head;
            storeTree->getData()->_loads = pair;

            seenLoads.add(storeTree->getData()->_loadUsedInLoopIncrement);
            seenLoadPairs.add(storeTree->getData()->_loads);
            }

         TR::Node *newStore = placeNewInductionVariableIncrementTree(loopInvariantBlock, inductionVarSymRef, newSymbolReference, k, symRefTab, placeHolderNode, newLoad, storeTree->getData()->_insertionTreeTop, storeTree->getData()->_constNode, storeTree->getData()->_isAddition);

         int32_t storeBlockNum = storeTree->getData()->_insertionTreeTop->getEnclosingBlock()->startOfExtendedBlock()->getNumber();

         // only update the return value if the storevalue
         // is going to be used in the new loopTest
         //
         if (!(loopTestHasSymRef > 0) ||
               (storeBlockNum == loopTestBlockNum))
               returnValue = newStore;

         storeTree = storeTree->getNextElement();
         }
      }
   else
      {
      if (_loadUsedInNewLoopIncrement[k])
         newLoad = _loadUsedInNewLoopIncrement[k];
      else
         {
         if (_linearEquations[k][4] > -1)
            newLoad = TR::Node::createLoad(placeHolderNode, newSymbolReference);
         else
            newLoad = TR::Node::createLoad(placeHolderNode, newSymbolReference);
         newLoad->setLocalIndex(~0);
         // remember this load here in case the loop test tree needs to refer back to this load
         // instead of the new incremented/decremented value
         _loadUsedInNewLoopIncrement[k] = newLoad;
         }
       returnValue = placeNewInductionVariableIncrementTree(loopInvariantBlock, inductionVarSymRef, newSymbolReference, k, symRefTab, placeHolderNode, newLoad, _insertionTreeTop, _constNode, _isAddition);

      }

   TR_ASSERT(returnValue, "new store for induction variable not found\n");
   return returnValue;
   }




TR::Node *TR_LoopStrider::placeNewInductionVariableIncrementTree(TR_BlockStructure *loopInvariantBlock, TR::SymbolReference *inductionVarSymRef, TR::SymbolReference *newSymbolReference, int32_t k, TR::SymbolReferenceTable *symRefTab, TR::Node *placeHolderNode, TR::Node *newLoad, TR::TreeTop *insertionTreeTop, TR::Node *constNode, bool isAddition)
   {
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   //traceMsg(comp(), "For new sym ref %d _constNode is %x (value %d)\n", newSymbolReference->getReferenceNumber(), constNode, constNode->getInt());

   // Create the multiply for (usually) constant increment;
   // e.g. of the form 4*1
   //
   TR::Node *newMulNode = NULL;
   if (usingAladd)
      {
      TR::Node *multiplicativeConstant = duplicateMulTermNode(k, placeHolderNode, TR::Int64);

      TR::Node *newConstNode = constNode->duplicateTree();
      if (constNode->getOpCode().isLoadConst())
         {
         TR::Node::recreate(newConstNode, TR::lconst);
         newConstNode->setLongInt(GET64BITINT(constNode));
         if (constNode->getType().isInt32())
            {
            if (constNode->getInt() < 0)
               newConstNode->setLongInt(-1*newConstNode->getLongInt());
            }
         else
            {
            if (constNode->getLongInt() < 0)
               newConstNode->setLongInt(-1*newConstNode->getLongInt());
            }
         }
      else if (!constNode->getType().isInt64())
         {
         TR::Node *i2lNode = newConstNode->duplicateTree();
         i2lNode->setReferenceCount(1);
         newConstNode->setNumChildren(1);
         newConstNode->setChild(0, i2lNode);
         TR::Node::recreate(newConstNode, TR::i2l);
         }
      newMulNode = TR::Node::create(TR::lmul, 2, newConstNode, multiplicativeConstant);
      newConstNode->setLocalIndex(~0);
      //_constNode->incReferenceCount();
      }
   else
      {
      newMulNode = TR::Node::create(newLoad->getType().isInt64() ? TR::lmul : TR::imul,
         2, constNode, duplicateMulTermNode(k, placeHolderNode, newLoad->getDataType()));
      }

   newMulNode->setLocalIndex(~0);
   constNode->setLocalIndex(~0);
   newMulNode->getSecondChild()->setLocalIndex(~0);

   if (constNode->getOpCode().isLoadConst())
      {
      if (constNode->getType().isInt32())
         {
         if (constNode->getInt() < 0)
            constNode->setInt(-1*constNode->getInt());
         }
      else
         {
         if (constNode->getLongInt() < 0)
            constNode->setLongInt(-1*constNode->getLongInt());
         }
      }

   // Create the addition for the derived induction variable
   // in loop increment step
   //
   TR::Node *loopIncrementer = NULL;
   if (isAddition)
      {
      if (_linearEquations[k][4] > -1)
         {
         TR::Node *newIvLoad = newLoad;
         if (newLoad->getOpCodeValue() == TR::l2i)
            newIvLoad = newLoad->getFirstChild();

         loopIncrementer = (usingAladd) ?
                              TR::Node::create(TR::aladd, 2, newIvLoad, newMulNode) :
                              TR::Node::create(TR::aiadd, 2, newIvLoad, newMulNode);
         loopIncrementer->setIsInternalPointer(true);

         TR::AutomaticSymbol *autoSym = symRefTab->getSymRef((int32_t)_linearEquations[k][4])->getSymbol()->castToAutoSymbol();
         if (!autoSym->isInternalPointer())
            {
            loopIncrementer->setPinningArrayPointer(autoSym);
            autoSym->setPinningArrayPointer();
            }
         else
            loopIncrementer->setPinningArrayPointer(autoSym->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

         }
      else
         {
         TR::Node *newIvLoad = newLoad;
         if (newLoad->getOpCodeValue() == TR::l2i)
            newIvLoad = newLoad->getFirstChild();

         loopIncrementer = (usingAladd || (newIvLoad->getType().isInt64())) ?
                              TR::Node::create(TR::ladd, 2, newIvLoad, newMulNode) :
                              TR::Node::create(TR::iadd, 2, newIvLoad, newMulNode);
         }
      }
   else
      {
      if (_linearEquations[k][4] > -1)
         {
         if (constNode->getOpCode().isLoadConst())
            {
            TR::Node *changedConstNode = constNode->duplicateTree();
            if (usingAladd)
               {
               TR::Node::recreate(changedConstNode, TR::lconst);
               changedConstNode->setLongInt(-1*GET64BITINT(constNode));
               }
            else
               {
               if (constNode->getType().isInt32())
                  changedConstNode->setInt(-1*constNode->getInt());
               else
                  changedConstNode->setLongInt(-1*constNode->getLongInt());
               }

            // dont dec the reference count for _constNode here if we are in the mood of aladds
            // because it wasnt used to create the newMulNode in the first place...
            // see newConstNode above
            if (!usingAladd)
               constNode->recursivelyDecReferenceCount();
            newMulNode->setAndIncChild(0, changedConstNode);
            }
         else
            {
            TR::ILOpCodes op = (usingAladd || newMulNode->getType().isInt64()) ? TR::lneg : TR::ineg;
            newMulNode = TR::Node::create(op, 1, newMulNode);
            newMulNode->setLocalIndex(~0);
            }

         //traceMsg(comp(), "constNode : %x has value %d\n", constNode, constNode->getInt());
         TR::Node *newIvLoad = newLoad;
         if (newLoad->getOpCodeValue() == TR::l2i)
            newIvLoad = newLoad->getFirstChild();

         loopIncrementer = (usingAladd) ?
                              TR::Node::create(TR::aladd, 2, newIvLoad, newMulNode) :
                              TR::Node::create(TR::aiadd, 2, newIvLoad, newMulNode);
         loopIncrementer->setIsInternalPointer(true);

         TR::AutomaticSymbol *autoSym = symRefTab->getSymRef((int32_t)_linearEquations[k][4])->getSymbol()->castToAutoSymbol();
         if (!autoSym->isInternalPointer())
            {
            loopIncrementer->setPinningArrayPointer(autoSym);
            autoSym->setPinningArrayPointer();
            }
         else
            loopIncrementer->setPinningArrayPointer(autoSym->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

         }
      else
         {
         TR::Node *newIvLoad = newLoad;
         if (newLoad->getOpCodeValue() == TR::l2i)
            newIvLoad = newLoad->getFirstChild();

         loopIncrementer = (usingAladd || (newIvLoad->getType().isInt64())) ?
                              TR::Node::create(TR::lsub, 2, newIvLoad, newMulNode) :
                              TR::Node::create(TR::isub, 2, newIvLoad, newMulNode);
         }
      }

   loopIncrementer->setLocalIndex(~0);

   TR::Node *newStore = NULL;
   //
   //Create the actual store tree and insert it
   //
   if (_linearEquations[k][4] > -1)
      newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, loopIncrementer, newSymbolReference);
   else
      {
      TR::ILOpCodes op =  (usingAladd ||(loopIncrementer->getType().isInt64())) ? TR::lstore : TR::istore;
      newStore = TR::Node::createWithSymRef(op, 1, 1, loopIncrementer, newSymbolReference);
      }

   newStore->setLocalIndex(~0);
   TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
   newStoreTreeTop->join(insertionTreeTop->getNextTreeTop());
   insertionTreeTop->join(newStoreTreeTop);
   dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted loop incremental step tree : %p for new symRef #%d\n", newStoreTreeTop->getNode(), newSymbolReference->getReferenceNumber());
   return newStore;
   }

int32_t TR_LoopStrider::findNewInductionVariable(TR::Node *node, TR::SymbolReference **symRef, bool hasAdditiveTerm, int32_t internalPointerParentSymbol)
   {
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;


   if (hasAdditiveTerm)
      {
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
      int32_t i = _startExpressionForThisInductionVariable;
      for (;i<_nextExpression;i++)
         {
         if (!(isAdditiveTermConst(i) && node->getSecondChild()->getOpCode().isLoadConst()) &&
             !isAdditiveTermEquivalentTo(i, node->getSecondChild()))
            continue;

         bool internalPointerParentSame = false;
         if (internalPointerParentSymbol == _linearEquations[i][4])
            internalPointerParentSame = true;

         if (internalPointerParentSame)
            {
            if (node->getFirstChild()->getOpCodeValue() == TR::imul || node->getFirstChild()->getOpCodeValue() == TR::lmul)
               {
               if (isMulTermEquivalentTo(i, node->getFirstChild()->getSecondChild()))
                  {
                  if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::ladd)
                     {
                     *symRef = symRefTab->getSymRef((int32_t)_linearEquations[i][1]);
                     return i;
                     }
                  else if (node->getOpCodeValue() == TR::isub || node->getOpCodeValue() == TR::lsub)
                     {
                     *symRef = symRefTab->getSymRef((int32_t)_linearEquations[i][1]);
                     return i;
                     }
                  }
               }
            else if (node->getFirstChild()->getOpCodeValue() == TR::ishl || node->getFirstChild()->getOpCodeValue() == TR::lshl)
               {
               TR_ASSERT(node->getFirstChild()->getSecondChild()->getOpCode().isLoadConst(), "assertion failure");

               int32_t multiplicativeConstant = GET32BITINT(node->getFirstChild()->getSecondChild());
               bool sameFactor = false;
               if (isMulTermConst(i) && getMulTermConst(i) == 1)
                  {
                  if (multiplicativeConstant == 0)
                     sameFactor = true;
                  }
               else if (isMulTermConst(i) && getMulTermConst(i) == (2 << (multiplicativeConstant - 1)))
                 sameFactor = true;

               if (sameFactor)
                  {
                  if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::ladd)
                     {
                     *symRef = symRefTab->getSymRef((int32_t)_linearEquations[i][1]);
                     return i;
                     }
                  else if (node->getOpCodeValue() == TR::isub || node->getOpCodeValue() == TR::lsub)
                       {
                       *symRef = symRefTab->getSymRef((int32_t)_linearEquations[i][1]);
                       return i;
                       }
                    }
               }
            }
         }
      }
   else
      {
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
      int32_t i = _startExpressionForThisInductionVariable;

      for (;i<_nextExpression;i++)
         {
         if (!isAdditiveTermConst(i))
            continue;

         bool internalPointerParentSame = false;
         if (internalPointerParentSymbol == _linearEquations[i][4])
            internalPointerParentSame = true;

         if (internalPointerParentSame)
            {
              if (node->getOpCodeValue() == TR::imul || node->getOpCodeValue() == TR::lmul)
               {
               if (isMulTermEquivalentTo(i, node->getSecondChild()))
                  {
                  *symRef = symRefTab->getSymRef((int32_t)_linearEquations[i][1]);
                  return i;
                  }
               }
            else if (node->getOpCodeValue() == TR::ishl || node->getOpCodeValue() == TR::lshl)
               {
               TR_ASSERT(node->getSecondChild()->getOpCode().isLoadConst(), "assertion failure");
               int32_t multiplicativeConstant = GET32BITINT(node->getSecondChild());
               bool sameFactor = false;
               if (isMulTermConst(i) && getMulTermConst(i) == 1)
                  {
                  if (multiplicativeConstant == 0)
                     sameFactor = true;
                  }
               else if (isMulTermConst(i) && getMulTermConst(i) == (2 << (multiplicativeConstant - 1)))
                  sameFactor = true;

               if (sameFactor)
                  {
                  *symRef = symRefTab->getSymRef((int32_t)_linearEquations[i][1]);
                  return i;
                  }
               }
            }
         }
      }

   return -1;
   }










void TR_LoopStrider::identifyExpressionsLinearInInductionVariables(TR_Structure *structure, vcount_t visitCount)
   {
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         _currTree = currentTree;
         identifyExpressionLinearInInductionVariable(currentNode, visitCount);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         identifyExpressionsLinearInInductionVariables(node->getStructure(), visitCount);
         }
      }
   }






bool TR_LoopStrider::identifyExpressionLinearInInductionVariable(TR::Node *node, vcount_t visitCount)
   {
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);
   if (cg()->supportsInternalPointers() &&
       node->isInternalPointer() &&
       node->getFirstChild()->getOpCode().isLoadVar() &&
       node->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm() &&
       //node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
       _neverWritten->get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()) &&
       (comp()->getStartBlock()->getFrequency() < 500))
      {
      node = node->getSecondChild();
      if (node->getOpCodeValue() == TR::l2i)
         {
         node->setVisitCount(visitCount);
         node = node->getFirstChild();
         }
      }

   bool needToVisitChild = true;

   if (node->getOpCode().isLoadVar())
      {
      int32_t symRefNum = node->getSymbolReference()->getReferenceNumber();
      if (_writtenExactlyOnce.ValueAt(symRefNum))
         {
         if ((_currTree != _storeTrees[symRefNum]) /* && (_loopTestTree != _currTree) */)
            _cannotBeEliminated->set(symRefNum);
         }
      else
         {
         auto lookup = _storeTreesList->find(symRefNum);
         if (lookup != _storeTreesList->end())
            {
            List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
            ListIterator<TR_StoreTreeInfo> si(storeTreesList);
            TR_StoreTreeInfo *storeTree;
            bool flag = false;
            for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
                {
                if (_currTree == storeTree->_tt)
                   {
                   flag = true;
                   break;
                   }
                }

            if (!flag)
               _cannotBeEliminated->set(symRefNum);
            }
         }
      }
   else if (((node->getOpCodeValue() == TR::iadd) || (node->getOpCodeValue() == TR::isub))
            || ((node->getOpCodeValue() == TR::ladd) || (node->getOpCodeValue() == TR::lsub)))
      {
      if (isExprLoopInvariant(node->getSecondChild()))
         {
         if (((node->getFirstChild()->getOpCodeValue() == TR::imul)
               || (node->getFirstChild()->getOpCodeValue() == TR::ishl))
             || ((node->getFirstChild()->getOpCodeValue() == TR::lmul)
               || (node->getFirstChild()->getOpCodeValue() == TR::lshl)))
             {
             bool isShift = node->getFirstChild()->getOpCodeValue() == TR::ishl ||
                            node->getFirstChild()->getOpCodeValue() == TR::lshl;
             TR::Node *linearNode, *mulTermNode;
             bool isLinearInInductionVariable;

             if (usingAladd && node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::i2l)
                linearNode = node->getFirstChild()->getFirstChild()->getFirstChild();
             else
                linearNode =  node->getFirstChild()->getFirstChild();
             mulTermNode = node->getFirstChild()->getSecondChild();
             isLinearInInductionVariable = isExpressionLinearInSomeInductionVariable(linearNode);

             if (isLinearInInductionVariable && isExprLoopInvariant(mulTermNode))
                {
                _numberOfLinearExprs++;
                //traceMsg(comp(), "identified node %p number %d\n", node, _numberOfLinearExprs);
                needToVisitChild = false;
                }
             else // if (!isLinearInInductionVariable)
                {
                if (usingAladd && node->getFirstChild()->getSecondChild()->getOpCodeValue() == TR::i2l)
                   linearNode = node->getFirstChild()->getSecondChild()->getFirstChild();
                else
                   linearNode =  node->getFirstChild()->getSecondChild();
                mulTermNode = node->getFirstChild()->getFirstChild();
                isLinearInInductionVariable = isExpressionLinearInSomeInductionVariable(linearNode);

                //traceMsg(comp(), "identifying node %p is linear %d mulTermNode %p\n", node, isLinearInInductionVariable, mulTermNode);

                if (isLinearInInductionVariable && isExprLoopInvariant(mulTermNode))
                   {
                   _numberOfLinearExprs++;
                   //traceMsg(comp(), "identified node %p number %d\n", node, _numberOfLinearExprs);
                   needToVisitChild = false;
                   }
                }
             }
          }
      }
   else if (((node->getOpCodeValue() == TR::imul) || (node->getOpCodeValue() == TR::ishl)) ||
            ((node->getOpCodeValue() == TR::lmul) || (node->getOpCodeValue() == TR::lshl)))
      {

      bool isShift = node->getOpCodeValue() == TR::ishl || node->getOpCodeValue() == TR::lshl;
      TR::Node *linearNode, *mulTermNode;
      bool isLinearInInductionVariable;

      if (usingAladd && node->getFirstChild()->getOpCodeValue() == TR::i2l) // handle both TR::xmul and TR::xshl
          linearNode = node->getFirstChild()->getFirstChild();
      else
          linearNode = node->getFirstChild();
      isLinearInInductionVariable = isExpressionLinearInSomeInductionVariable(linearNode);
      mulTermNode = node->getSecondChild();

//      traceMsg(comp(), "identifying node %p is linear %d linearNode %p mulTermNode %p\n", node, isLinearInInductionVariable, linearNode, mulTermNode);

      if (isLinearInInductionVariable && isExprLoopInvariant(mulTermNode))
         {
         _numberOfLinearExprs++;
//         traceMsg(comp(), "identified node %p number %d\n", node, _numberOfLinearExprs);
         needToVisitChild = false;
         }
      else // if (!isLinearInInductionVariable)
         {
         if (usingAladd && node->getSecondChild()->getOpCodeValue() == TR::i2l) // handle both TR::xmul and TR::xshl
             linearNode = node->getSecondChild()->getFirstChild();
         else
             linearNode = node->getSecondChild();
         isLinearInInductionVariable = isExpressionLinearInSomeInductionVariable(linearNode);
         mulTermNode = node->getFirstChild();

         //traceMsg(comp(), "identifying node %p is linear %d linearNode %p mulTermNode %p\n", node, isLinearInInductionVariable, linearNode, mulTermNode);

         if (isLinearInInductionVariable && isExprLoopInvariant(mulTermNode))
            {
            _numberOfLinearExprs++;
            //traceMsg(comp(), "identified node %p number %d\n", node, _numberOfLinearExprs);
            needToVisitChild = false;
           }
         }
      }

   if (needToVisitChild)
      {
      int32_t i;
      for (i = 0;i < node->getNumChildren();i++)
         {
         identifyExpressionLinearInInductionVariable(node->getChild(i), visitCount);
         }
      }

   return true;
   }






TR::Node *TR_LoopStrider::setUsesLoadUsedInLoopIncrement(TR::Node *node, int32_t k)
   {
   if (_storeTreesList)
      {
      auto lookup = _storeTreesList->find(_loopDrivingInductionVar);
      if (lookup != _storeTreesList->end())
         {
         List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
         ListIterator<TR_StoreTreeInfo> si(storeTreesList);
         TR_StoreTreeInfo *storeTree;
         for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
            {
            if (!storeTree->_loadUsedInLoopIncrement &&
                node->getReferenceCount() > 1)
               return NULL;
            if (node == storeTree->_loadUsedInLoopIncrement && !storeTree->_incrementInDifferentExtendedBlock)
               {
               _usesLoadUsedInLoopIncrement = true;
               _storeTreeInfoForLoopIncrement = storeTree;
               //_loadUsedInNewLoopIncrement[k] = node;
               }
            }
         }
      }
   else
      {
      if (!_loadUsedInLoopIncrement && node->getReferenceCount() > 1)
         return NULL;
      if (node == _loadUsedInLoopIncrement && !_incrementInDifferentExtendedBlock)
         _usesLoadUsedInLoopIncrement = true;
      }
   return node;
   }



TR::Node *TR_LoopStrider::isExpressionLinearInInductionVariable(TR::Node *node, int32_t k)
   {
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   TR::Node *returnedNode = NULL;
   if ((node->getOpCodeValue() == TR::iload) ||
       (node->getOpCodeValue() == TR::lload))
       {
       if (node->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar)
          {
          if (!setUsesLoadUsedInLoopIncrement(node, k))
             return NULL;

          if (usingAladd)
             returnedNode = TR::Node::create(node, TR::lload);
          else
             {
             if (node->getOpCodeValue() == TR::iload)
                returnedNode = TR::Node::create(node, TR::iload);
             else
                returnedNode = TR::Node::create(node, TR::lload);
             }
          returnedNode->setLocalIndex(~0);
          }
       }
   else if ((node->getOpCodeValue() == TR::iadd) ||
            (node->getOpCodeValue() == TR::ladd))
       {
       if ((node->getFirstChild()->getOpCodeValue() == TR::iload) ||
           (node->getFirstChild()->getOpCodeValue() == TR::lload))
          {
          if ((node->getFirstChild()->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar) &&
              (node->getSecondChild()->getOpCode().isLoadConst() ||
              (node->getSecondChild()->getOpCode().isLoadVarDirect() &&
               node->getSecondChild()->getSymbol()->isAutoOrParm() &&
               _neverWritten->get(node->getSecondChild()->getSymbolReference()->getReferenceNumber()))))
             {
             if (!setUsesLoadUsedInLoopIncrement(node->getFirstChild(), k))
                return NULL;

             if (usingAladd)
                returnedNode = TR::Node::create(node, TR::ladd, 2);
             else
                {
                if (node->getFirstChild()->getOpCodeValue() == TR::iload)
                   returnedNode = TR::Node::create(node, TR::iadd, 2);
                else
                   returnedNode = TR::Node::create(node, TR::ladd, 2);
                }
             returnedNode->setLocalIndex(~0);
             }
          }
       }
   else if ((node->getOpCodeValue() == TR::isub) ||
            (node->getOpCodeValue() == TR::lsub))
       {
       if ((node->getFirstChild()->getOpCodeValue() == TR::iload) ||
           (node->getFirstChild()->getOpCodeValue() == TR::lload))
          {
          if ((node->getFirstChild()->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar) &&
              (node->getSecondChild()->getOpCode().isLoadConst() ||
              (node->getSecondChild()->getOpCode().isLoadVarDirect() &&
               node->getSecondChild()->getSymbol()->isAutoOrParm() &&
               _neverWritten->get(node->getSecondChild()->getSymbolReference()->getReferenceNumber()))))
             {
             if (!setUsesLoadUsedInLoopIncrement(node->getFirstChild(), k))
                return NULL;

             if (usingAladd)
                returnedNode = TR::Node::create(node, TR::lsub, 2);
             else
                {
                if (node->getFirstChild()->getOpCodeValue() == TR::iload)
                   returnedNode = TR::Node::create(node, TR::isub, 2);
                else
                   returnedNode = TR::Node::create(node, TR::lsub, 2);
                }
             returnedNode->setLocalIndex(~0);
             }
          }
       }

   return returnedNode;
   }






bool TR_LoopStrider::isExpressionLinearInSomeInductionVariable(TR::Node *node)
   {
   //traceMsg(comp(), "00 identifying node %p in some \n", node);
   if ((node->getOpCodeValue() == TR::iload) ||
       (node->getOpCodeValue() == TR::lload))
       {
       //traceMsg(comp(), "11 identifying node %p in some \n", node);

       if (_storeTreesList || _writtenExactlyOnce.ValueAt(node->getSymbolReference()->getReferenceNumber()))
          {
          //traceMsg(comp(), "22 identifying node %p in some \n", node);
          return true;
          }
       }
   else if ((node->getOpCodeValue() == TR::iadd) ||
            (node->getOpCodeValue() == TR::ladd))
       {
       if ((node->getFirstChild()->getOpCodeValue() == TR::iload) ||
           (node->getFirstChild()->getOpCodeValue() == TR::lload))
          {
          if ( (_storeTreesList || _writtenExactlyOnce.ValueAt(node->getFirstChild()->getSymbolReference()->getReferenceNumber()) ) &&
              (node->getSecondChild()->getOpCode().isLoadConst() ||
              (node->getSecondChild()->getOpCode().isLoadVarDirect() &&
               node->getSecondChild()->getSymbol()->isAutoOrParm() &&
               _neverWritten->get(node->getSecondChild()->getSymbolReference()->getReferenceNumber()))))
             {
             return true;
             }
          }
       }
   else if ((node->getOpCodeValue() == TR::isub) ||
            (node->getOpCodeValue() == TR::lsub))
       {
       if ((node->getFirstChild()->getOpCodeValue() == TR::iload) ||
           (node->getFirstChild()->getOpCodeValue() == TR::lload))
          {
          if ( (_storeTreesList || _writtenExactlyOnce.ValueAt(node->getFirstChild()->getSymbolReference()->getReferenceNumber()) ) &&
              (node->getSecondChild()->getOpCode().isLoadConst() ||
              (node->getSecondChild()->getOpCode().isLoadVarDirect() &&
               node->getSecondChild()->getSymbol()->isAutoOrParm() &&
               _neverWritten->get(node->getSecondChild()->getSymbolReference()->getReferenceNumber()))))
             {
             return true;
             }
          }
       }

   return false;
   }

bool TR_LoopStrider::reassociateAndHoistComputations(TR::Block *loopInvariantBlock, TR_Structure *structure)
   {
   bool reassociatedComputation = false;
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();
      comp()->incVisitCount();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();

         if (reassociateAndHoistComputations(loopInvariantBlock, NULL, -1, currentNode, comp()->getVisitCount()))
            reassociatedComputation = true;

         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;

      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         if (reassociateAndHoistComputations(loopInvariantBlock, node->getStructure()))
            reassociatedComputation = true;
         }
      }

   return reassociatedComputation;
   }





bool TR_LoopStrider::reassociateAndHoistComputations(TR::Block *loopInvariantBlock, TR::Node *parent, int32_t childNum, TR::Node *node, vcount_t visitCount)
   {
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   bool reassociatedComputation = false;
   bool examineChildren = true;
   if (node->getVisitCount() >= visitCount)
      examineChildren = false;

   node->setVisitCount(visitCount);

   bool isInternalPointer = false;
   int32_t internalPointerSymbol = -1;
   TR::AutomaticSymbol *pinningArrayPointer = NULL;
   int32_t originalInternalPointerSymbol = 0;
   TR::Node *originalNode = NULL;
   if (cg()->supportsInternalPointers() &&
       (reassociateAndHoistNonPacked() && node->isInternalPointer()) &&
       node->getFirstChild()->getOpCode().isLoadVar() &&
       node->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm() &&
       //node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
       _neverWritten->get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
      {
          //printf("Creating internal ptr in %s\n", comp()->signature());
      isInternalPointer = true;
      internalPointerSymbol = node->getFirstChild()->getSymbolReference()->getReferenceNumber();
      originalInternalPointerSymbol = internalPointerSymbol;
      if (node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto())
         pinningArrayPointer = node->getFirstChild()->getSymbolReference()->getSymbol()->castToAutoSymbol();
      else
         {
         SymRefPair *pair = _parmAutoPairs;
         while (pair)
            {
            if (pair->_indexSymRef == node->getFirstChild()->getSymbolReference())
               {
               pinningArrayPointer = pair->_derivedSymRef->getSymbol()->castToAutoSymbol();
               internalPointerSymbol = pair->_derivedSymRef->getReferenceNumber();
               break;
               }
            pair = pair->_next;
            }
         }
      originalNode = node;
      node = node->getSecondChild();
      }

   if (isInternalPointer &&
       examineChildren)
      {
      bool isConst = false;
      if (node->getOpCode().isLoadConst())
         isConst = true;


      if ((isConst ||
           (node->getOpCode().hasSymbolReference() &&
            node->getOpCode().isLoadVar() &&
            //node->getSymbolReference()->getSymbol()->isAuto() &&
            node->getSymbolReference()->getSymbol()->isAutoOrParm() &&
            _neverWritten->get(node->getSymbolReference()->getReferenceNumber()))) &&
           (!_registersScarce || (originalNode->getReferenceCount() > 1)) &&
           (comp()->getSymRefTab()->getNumInternalPointers() < maxInternalPointers()) &&
           (!comp()->cg()->canBeAffectedByStoreTagStalls() ||
               _numInternalPointerOrPinningArrayTempsInitialized < MAX_INTERNAL_POINTER_AUTOS_INITIALIZED) &&
           performTransformation(comp(), "%s Replacing invariant internal pointer %p based on symRef #%d\n", OPT_DETAILS, node, internalPointerSymbol))
         {
         TR::SymbolReference *internalPointerSymRef = NULL;
         auto symRefPairSearchResult = _hoistedAutos->find(originalInternalPointerSymbol);
         SymRefPair *symRefPair = symRefPairSearchResult != _hoistedAutos->end() ? symRefPairSearchResult->second : NULL;
         while (symRefPair)
            {
            bool matched = false;
            if (isConst)
               {
               int32_t constNodeValue;
               if (usingAladd && node->getType().isInt64())
                  constNodeValue = (int32_t) node->getLongInt();
               else
                  constNodeValue = node->getInt();
               if (symRefPair->_isConst &&
                   (symRefPair->_index == constNodeValue))
                  matched = true;
               }
            else
               {
               if (!symRefPair->_isConst &&
                   (symRefPair->_indexSymRef == node->getSymbolReference()))
                  matched = true;
               }

            if (matched)
              {
              internalPointerSymRef = symRefPair->_derivedSymRef;
              break;
              }

            symRefPair = symRefPair->_next;
            }

         if (!internalPointerSymRef)
            {
            TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, isInternalPointer);
            if (isInternalPointer && !pinningArrayPointer)
               {
               TR::SymbolReference *newPinningArray = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, false);
               pinningArrayPointer = newPinningArray->getSymbol()->castToAutoSymbol();
               createParmAutoPair(comp()->getSymRefTab()->getSymRef(internalPointerSymbol), newPinningArray);

               TR::Node *newAload = TR::Node::createLoad(node, comp()->getSymRefTab()->getSymRef(internalPointerSymbol));
               newAload->setLocalIndex(~0);
               TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, newAload, newPinningArray);
               internalPointerSymbol = newPinningArray->getReferenceNumber();
               TR::TreeTop *placeHolderTree = loopInvariantBlock->getEntry();
               TR::TreeTop *nextTree = placeHolderTree->getNextTreeTop();
               newStore->setLocalIndex(~0);
               TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
               placeHolderTree->join(newStoreTreeTop);
               newStoreTreeTop->join(nextTree);
               //printf("Reached here in %s\n", comp()->signature());
               dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted initialization tree : %p for new symRef #%d\n", newStoreTreeTop->getNode(), newPinningArray->getReferenceNumber());
               _numInternalPointerOrPinningArrayTempsInitialized++;
               }

            _newTempsCreated = true;
            if (isInternalPointer)
               _numInternalPointers++;
            else
               _newNonAddressTempsCreated = true;

            TR::Symbol *symbol = newSymbolReference->getSymbol();

            if (!pinningArrayPointer->isInternalPointer())
               {
               symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer);
               pinningArrayPointer->setPinningArrayPointer();
               }
            else
               symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

            SymRefPair *pair = (SymRefPair *) trMemory()->allocateStackMemory(sizeof(SymRefPair));
            if (isConst)
               {
               int32_t constNodeValue;
               if (usingAladd && node->getType().isInt64())
                  constNodeValue = (int32_t) node->getLongInt();
               else
                  constNodeValue = node->getInt();

               pair->_index = constNodeValue;
               pair->_isConst = true;
               }
            else
               {
               pair->_indexSymRef = node->getSymbolReference();
               pair->_isConst = false;
               }

            pair->_derivedSymRef = newSymbolReference;
            pair->_next = (*_hoistedAutos)[originalInternalPointerSymbol];
            (*_hoistedAutos)[originalInternalPointerSymbol] = pair;
            internalPointerSymRef = newSymbolReference;
            }

         originalNode->getFirstChild()->recursivelyDecReferenceCount();
         originalNode->getSecondChild()->recursivelyDecReferenceCount();
         TR::Node::recreate(originalNode, TR::aload);
         originalNode->setSymbolReference(internalPointerSymRef);
         originalNode->setNumChildren(0);
         originalNode->setLocalIndex(~0);
         reassociatedComputation = true;
         examineChildren = false;
         }
      }

   if (((node->getOpCodeValue() == TR::iadd) || (node->getOpCodeValue() == TR::isub)) ||
       ((node->getOpCodeValue() == TR::ladd) || (node->getOpCodeValue() == TR::lsub)))
      {
      if (node->getSecondChild()->getOpCodeValue() == TR::iconst ||
          node->getSecondChild()->getOpCodeValue() == TR::lconst)
         {
         bool isAdd = false;
         if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::ladd)
            isAdd = true;

         int32_t constValue;
         if (usingAladd && node->getSecondChild()->getType().isInt64())
            constValue = (int32_t)node->getSecondChild()->getLongInt();
         else
            constValue = node->getSecondChild()->getInt();

         int32_t hdrSize = (int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         if ((isInternalPointer &&
              (comp()->getSymRefTab()->getNumInternalPointers() < maxInternalPointers()) &&
              ((isAdd && (constValue == hdrSize)) ||
               (!isAdd && constValue == -hdrSize))) &&
              (!_registersScarce || (node->getReferenceCount() > 1) || _reassociatedNodes.find(node)) &&
              (!comp()->cg()->canBeAffectedByStoreTagStalls() ||
                  _numInternalPointerOrPinningArrayTempsInitialized < MAX_INTERNAL_POINTER_AUTOS_INITIALIZED) &&
              performTransformation(comp(), "%s Replacing reassociated internal pointer based on symRef #%d\n", OPT_DETAILS, internalPointerSymbol))
            {
            if (_reassociatedAutos->find(originalInternalPointerSymbol) == _reassociatedAutos->end())
               {
               TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, isInternalPointer);
               if (isInternalPointer && !pinningArrayPointer)
                  {
                  TR::SymbolReference *newPinningArray = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, false);
                  pinningArrayPointer = newPinningArray->getSymbol()->castToAutoSymbol();
                  createParmAutoPair(comp()->getSymRefTab()->getSymRef(internalPointerSymbol), newPinningArray);

                  TR::Node *newAload = TR::Node::createLoad(node, comp()->getSymRefTab()->getSymRef(internalPointerSymbol));
                  newAload->setLocalIndex(~0);
                  TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, newAload, newPinningArray);
                  internalPointerSymbol = newPinningArray->getReferenceNumber();
                  TR::TreeTop *placeHolderTree = loopInvariantBlock->getEntry();
                  TR::TreeTop *nextTree = placeHolderTree->getNextTreeTop();
                  newStore->setLocalIndex(~0);
                  TR::TreeTop *newStoreTreeTop = TR::TreeTop::create(comp(), newStore);
                  placeHolderTree->join(newStoreTreeTop);
                  newStoreTreeTop->join(nextTree);
                  dumpOptDetails(comp(), "\nO^O INDUCTION VARIABLE ANALYSIS: Induction variable analysis inserted initialization tree : %p for new symRef #%d\n", newStoreTreeTop->getNode(), newPinningArray->getReferenceNumber());
                  _numInternalPointerOrPinningArrayTempsInitialized++;
                  }

               _newTempsCreated = true;
               if (isInternalPointer)
                  _numInternalPointers++;
               else
                  _newNonAddressTempsCreated = true;

               TR::Symbol *symbol = newSymbolReference->getSymbol();

               if (!pinningArrayPointer->isInternalPointer())
                  {
                  symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer);
                  pinningArrayPointer->setPinningArrayPointer();
                  }
               else
                  symbol->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArrayPointer->castToInternalPointerAutoSymbol()->getPinningArrayPointer());

               (*_reassociatedAutos)[originalInternalPointerSymbol] = newSymbolReference;
               dumpOptDetails(comp(), "reass num %d newsymref %d\n", internalPointerSymbol, newSymbolReference->getReferenceNumber());
               }

            TR::SymbolReference *internalPointerSymRef = (*_reassociatedAutos)[originalInternalPointerSymbol];
            originalNode->getFirstChild()->recursivelyDecReferenceCount();
            //node->recursivelyDecReferenceCount();
            node->decReferenceCount();
            _reassociatedNodes.add(node);

            if (node->getReferenceCount() == 0)
               {
               node->getFirstChild()->decReferenceCount();
               node->getSecondChild()->decReferenceCount();
               }

            TR::Node *newLoad = TR::Node::createWithSymRef(node, TR::aload, 0, internalPointerSymRef);
            newLoad->setLocalIndex(~0);
            originalNode->setAndIncChild(0, newLoad);
            originalNode->setAndIncChild(1, node->getFirstChild());
            reassociatedComputation = true;
            }
         }
      }


   if (examineChildren)
       {
       int32_t i;
       for (i=0;i<node->getNumChildren();i++)
          {
          if (reassociateAndHoistComputations(loopInvariantBlock, node, i, node->getChild(i), visitCount))
             reassociatedComputation = true;
          }
       }

   return reassociatedComputation;
   }




bool TR_LoopStrider::isStoreInRequiredForm(int32_t symRefNum, TR_Structure *loopStructure)
   {
   /* calls too can kill symRefs via loadaddr */
   if (symRefNum != 0 && _allKilledSymRefs[symRefNum] == true)
      return false;

   if (!comp()->getSymRefTab()->getSymRef(symRefNum)->getSymbol()->isAutoOrParm())
      return false;

   if (_storeTreesList)
      {
      auto lookup = _storeTreesList->find(symRefNum);
      if (lookup != _storeTreesList->end())
         {
         List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
         ListIterator<TR_StoreTreeInfo> si(storeTreesList);
         TR_StoreTreeInfo *storeTree;
         bool b = false;
         for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
            {
            TR::Node *storeNode = storeTree->_tt->getNode();
            if ((!storeNode->getType().isInt32() && !storeNode->getType().isInt64()) ||
                (!storeNode->getSymbolReference()->getSymbol()->getType().isInt32() && !storeNode->getSymbolReference()->getSymbol()->getType().isInt64()))
               return false;

            b = isStoreInRequiredForm(storeNode, symRefNum, loopStructure);
            storeTree->_loadUsedInLoopIncrement = _loadUsedInLoopIncrement;

            if (!b)
               return false;

            if (storeTree->_tt->getEnclosingBlock()->getStructureOf()->getContainingLoop() != loopStructure)
               return false;
            }

         return b;
         }
      return false;
      }

   TR::Node *storeNode = _storeTrees[symRefNum]->getNode();
   if (!storeNode->getType().isInt32() && !storeNode->getType().isInt64())
      return false;

   if (_storeTrees[symRefNum]->getEnclosingBlock()->getStructureOf()->getContainingLoop() != loopStructure)
      return false;

   return isStoreInRequiredForm(storeNode, symRefNum, loopStructure);
   }


void TR_LoopStrider::checkIfIncrementInDifferentExtendedBlock(TR::Block *block, int32_t inductionVariable)
   {
   _incrementInDifferentExtendedBlock = false;
   if (block !=
       _storeTrees[inductionVariable]->getEnclosingBlock()->startOfExtendedBlock())
      {
      _incrementInDifferentExtendedBlock = true;
      }

   if (_storeTreesList)
      {
      auto lookup = _storeTreesList->find(inductionVariable);
      if (lookup != _storeTreesList->end())
         {
         List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
         ListIterator<TR_StoreTreeInfo> si(storeTreesList);
         TR_StoreTreeInfo *storeTree;
         for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
            {
            if (block !=
                storeTree->_tt->getEnclosingBlock()->startOfExtendedBlock())
               {
               storeTree->_incrementInDifferentExtendedBlock = true;
               return;
               }
            }
         }
      }

   }


TR::Node *TR_LoopStrider::updateLoadUsedInLoopIncrement(TR::Node *node, int32_t inductionVariable)
   {
   TR::ILOpCode &opCode = node->getOpCode();
   if (_indirectInductionVariable &&
       opCode.isLoadVar() &&
       (node->getSymbolReference()->getReferenceNumber() < _numSymRefs) &&
       ((!_storeTreesList && _writtenExactlyOnce.ValueAt(node->getSymbolReference()->getReferenceNumber())) ||
        (_storeTreesList && _storeTreesList->find(node->getSymbolReference()->getReferenceNumber()) != _storeTreesList->end() && (*_storeTreesList)[node->getSymbolReference()->getReferenceNumber()]->isSingleton())))
      {
      TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
      if (!useDefInfo)
         return NULL;

      uint16_t useIndex = node->getUseDefIndex();
      if (!useIndex || !useDefInfo->isUseIndex(useIndex))
         return NULL;

      TR_UseDefInfo::BitVector defs(comp()->allocator());
      if (useDefInfo->getUseDef(defs, useIndex) && (defs.PopulationCount() == 1))
         {
         TR_UseDefInfo::BitVector::Cursor cursor(defs);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = cursor;
            if (defIndex < useDefInfo->getFirstRealDefIndex())
               return NULL;

            TR::Node *defNode = useDefInfo->getNode(defIndex);
            if (!defNode->getOpCode().isStore())
               //   continue;
               return NULL;

            TR::Node * constNode = containsOnlyInductionVariableAndAdditiveConstant(defNode->getFirstChild(), inductionVariable);
            if (constNode && _storeTrees[defNode->getSymbolReference()->getReferenceNumber()])
               {
               checkIfIncrementInDifferentExtendedBlock(useDefInfo->getTreeTop(defIndex)->getEnclosingBlock()->startOfExtendedBlock(), inductionVariable);
               }
            else
               {
               constNode = NULL;
               }
            return constNode;
            }
         }

      return NULL;
      }

   return NULL;
   }



bool TR_LoopStrider::isStoreInRequiredForm(TR::Node *storeNode, int32_t symRefNum, TR_Structure *loopStructure)
   {
   /* calls too can kill symRefs via loadaddr */
   if (symRefNum != 0 && _allKilledSymRefs[symRefNum] == true)
      return false;

   TR::Node *addNode = storeNode->getFirstChild();
   if (storeNode->getFirstChild()->getOpCode().isConversion() &&
       storeNode->getFirstChild()->getFirstChild()->getOpCode().isConversion())
      {
      TR::Node *conversion1 = storeNode->getFirstChild();
      TR::Node *conversion2 = storeNode->getFirstChild()->getFirstChild();
      if ((conversion1->getOpCodeValue() == TR::s2i &&
           conversion2->getOpCodeValue() == TR::i2s) ||
          (conversion1->getOpCodeValue() == TR::b2i &&
           conversion2->getOpCodeValue() == TR::i2b) ||
          (conversion1->getOpCodeValue() == TR::su2i &&
           conversion2->getOpCodeValue() == TR::i2s))
         addNode = conversion2->getFirstChild();
      }

   _incrementInDifferentExtendedBlock = false;
   _constNode = containsOnlyInductionVariableAndAdditiveConstant(addNode, symRefNum);
   if (_constNode == NULL)
      {
       if (!_indirectInductionVariable) return false;

      _loadUsedInLoopIncrement = NULL;
      // Check: it might be an induction variable incremented indirectly
      TR_InductionVariable *v = loopStructure->asRegion()->findMatchingIV(comp()->getSymRefTab()->getSymRef(symRefNum));
      if (!v) return false;

      _isAddition = true;
      TR::VPConstraint * incr = v->getIncr();
      int64_t low;

      if (incr->asIntConst())
         {
         low = (int64_t) incr->getLowInt();
         _constNode = TR::Node::create(storeNode, TR::iconst, 0, (int32_t)low);
         }
      else if (incr->asLongConst())
         {
         low = incr->getLowLong();
         _constNode = TR::Node::create(storeNode, TR::lconst, 0);
         _constNode->setLongInt(low);
         }
      else
         return false;

	  //constNode will be forced to positive in placeNewInductionVariableIncrementTree(...)
	  if((_constNode->getOpCode().isLoadConst()) && (low < 0))
         _isAddition = !_isAddition;

      if (trace())
         {
         traceMsg(comp(), "Found loop induction variable #%d incremented indirectly by %lld\n", symRefNum, low);
         }
      }
   else
      {
      TR::Node *secondChild = _constNode;
      if (secondChild->getOpCode().isLoadVarDirect())
         {
         int32_t timesWritten = 0;
         if (!isSymbolReferenceWrittenNumberOfTimesInStructure(loopStructure, secondChild->getSymbolReference()->getReferenceNumber(), &timesWritten, 0))
            return false;
         }
      else if (!secondChild->getOpCode().isLoadConst())
         {
         return false;
         }

	  //constNode will be forced to positive in placeNewInductionVariableIncrementTree(...)
      if (secondChild->getOpCode().isLoadConst() &&
          ((secondChild->getType().isInt32() && (secondChild->getInt() < 0)) ||
           (secondChild->getType().isInt64() && (secondChild->getLongInt() < 0))))
         {
         if (_isAddition)
            _isAddition = false;
         else
            _isAddition = true;
         }

      _constNode = _constNode->duplicateTree();
      _constNode->setReferenceCount(0);
      }

   _loopDrivingInductionVar = symRefNum;

   if (storeNode == _storeTrees[symRefNum]->getNode())
      {
      _insertionTreeTop = _storeTrees[symRefNum];
      }

   if (_storeTreesList)
      {
      auto lookup = _storeTreesList->find(symRefNum);
      if (lookup != _storeTreesList->end())
         {
         List<TR_StoreTreeInfo> *storeTreesList = lookup->second;
         ListIterator<TR_StoreTreeInfo> si(storeTreesList);
         TR_StoreTreeInfo *storeTree;
         for (storeTree = si.getCurrent(); storeTree != NULL; storeTree = si.getNext())
            {
            //traceMsg(comp(), "storeTree %p storeNode = %p cursor %p\n", storeTree, storeNode, storeTree->_tt->getNode());
            if (storeNode == storeTree->_tt->getNode())
               {
               storeTree->_insertionTreeTop = storeTree->_tt;
               storeTree->_constNode = _constNode;
               storeTree->_isAddition = _isAddition;
               break;
               }
            }
         }
      }

   //_insertionTreeTop = _storeTrees[symRefNum];

   return true;
   }

// (64-bit)
// sign-extension elimination routines

/**
 * Determine whether "expensive" assertions should be enabled.
 *
 * They are on in debug builds, and otherwise when the environment variable
 * TR_enableExpensiveLoopStriderAssertions is set.
 *
 * \return true if expensive assertions should be performed
 */
static bool enableExpensiveLoopStriderAssertions()
   {
#ifdef DEBUG
   return true;
#else
   static const char *env = feGetEnv("TR_enableExpensiveLoopStriderAssertions");
   static const bool enable = env != NULL && env[0] != '\0';
   return enable;
#endif
   }

// recursive implementation of orderSensitiveDescendants (below)
static void orderSensitiveDescendantsRec(
   TR::Node *node,
   TR::NodeChecklist &out,
   TR::NodeChecklist &visited)
   {
   if (visited.contains(node))
      return;

   visited.add(node);
   if (node->getOpCode().hasSymbolReference() && node->getOpCodeValue() != TR::loadaddr)
      out.add(node);

   for (int i = 0; i < node->getNumChildren(); i++)
      orderSensitiveDescendantsRec(node->getChild(i), out, visited);
   }

/**
 * Add to \p out all descendants of \p node with significant evaluation points.
 *
 * These descendants are precisely the ones that are \em not referentially
 * transparent.
 *
 * \param[in]  node the node whose effect on order of evaluation is of interest.
 * \param[out] out  the checklist to which the descendants are to be added.
 */
static void orderSensitiveDescendants(TR::Node *node, TR::NodeChecklist &out)
   {
   TR::NodeChecklist visited(TR::comp());
   orderSensitiveDescendantsRec(node, out, visited);
   }

/**
 * Determine whether \p orig and \p replacement are equivalent for ordering.
 *
 * Evaluation of a node "always" involves evaluation of each of its (previously
 * unevaluated) descendants. In general, substituting one node for another may
 * change the point of evaluation of yet other nodes beneath either, but not of
 * nodes that appear beneath \em both.
 *
 * This function therefore tests for the following property: each descendant
 * of \p orig or of \p replacement is either referentially transparent or a
 * descendant of both. When this property holds, substitution of \p replacement
 * for \b orig is guaranteed to preserve the evaluation order of other nodes
 * for which it is significant.
 *
 * \param orig        the original node, to be replaced
 * \param replacement the new node to take the place of \p orig
 * \return true if substitution is guaranteed to preserve evaluation order
 */
static bool substPreservesEvalOrder(TR::Node *orig, TR::Node *replacement)
   {
   TR::NodeChecklist origEvaluated(TR::comp());
   TR::NodeChecklist replacementEvaluated(TR::comp());
   orderSensitiveDescendants(orig, origEvaluated);
   orderSensitiveDescendants(replacement, replacementEvaluated);
   return origEvaluated == replacementEvaluated;
   }

/**
 * Assert that substituting \p replacement for \p orig preserves ordering.
 *
 * This is an "expensive" assertion.
 *
 * \param orig        the original node, to be replaced
 * \param replacement the new node to take the place of \p orig
 * \param operation   a name for the error message
 * \see substPreservesEvalOrder(TR::Node*, TR::Node*)
 * \see enableExpensiveLoopStriderAssertions()
 */
static void assertSubstPreservesEvalOrder(
   TR::Node *orig,
   TR::Node *replacement,
   const char *operation)
   {
   if (enableExpensiveLoopStriderAssertions())
      {
      TR_ASSERT_FATAL(
         substPreservesEvalOrder(orig, replacement),
         "%s fails to preserve ordering\n",
         operation);
      }
   }

// node-recursive implementation of assertStructureDoesNotMentionOriginals (below)
static void assertSubtreeDoesNotMentionOriginals(
   TR::Node *node,
   const TR::list<std::pair<int32_t, int32_t> > &widenedIVs,
   TR::NodeChecklist &visited)
   {
   if (visited.contains(node))
      return;

   visited.add(node);
   for (int i = 0; i < node->getNumChildren(); i++)
      assertSubtreeDoesNotMentionOriginals(node->getChild(i), widenedIVs, visited);

   if (!node->getOpCode().hasSymbolReference())
      return;

   int32_t symRefNum = node->getSymbolReference()->getReferenceNumber();
   for (auto it = widenedIVs.begin(); it != widenedIVs.end(); ++it)
      {
      TR_ASSERT_FATAL(
         symRefNum != it->first,
         "n%un is not supposed to have mentions of symref #%d\n",
         node->getGlobalIndex(),
         it->first);
      }
   }

/**
 * Assert that \p structure does not mention originals from \p widenedIVs.
 *
 * This is an "expensive" assertion.
 *
 * \param structure  the region that must not mention original symbol references
 * \param widenedIVs a list of (original, new) pairs of symbol reference numbers
 * \see enableExpensiveLoopStriderAssertions()
 */
static void assertStructureDoesNotMentionOriginals(
   TR_Structure *structure,
   const TR::list<std::pair<int32_t, int32_t> > &widenedIVs)
   {
   if (!enableExpensiveLoopStriderAssertions())
      return;

   if (structure->asBlock() != NULL)
      {
      TR::NodeChecklist visited(TR::comp());
      TR::Block *b = structure->asBlock()->getBlock();
      TR::TreeTop *end = b->getExit();
      for (TR::TreeTop *tt = b->getEntry(); tt != end; tt = tt->getNextTreeTop())
         assertSubtreeDoesNotMentionOriginals(tt->getNode(), widenedIVs, visited);
      }
   else
      {
      TR_RegionStructure::Cursor it(*structure->asRegion());
      for (TR_StructureSubGraphNode *n = it.getFirst(); n != NULL; n = it.getNext())
         assertStructureDoesNotMentionOriginals(n->getStructure(), widenedIVs);
      }
   }

/**
 * Determine whether \p loop iterates enough times on average for widening.
 *
 * \param loop      the loop in which widening is considered
 * \param preheader the preheader of \p loop
 * \return true if strider should attempt to widen
 */
static bool isLoopRelativelyFrequent(TR_Structure *loop, TR::Block *preheader)
   {
   // Tunable parameer values.
   //
   // minumum required loop header frequency
   const int defaultMinFreq = 2000;
   static const char * const envMinFreq =
      feGetEnv("TR_loopStriderSignExtensionMinimumFrequency");

   int minFreq = envMinFreq == NULL ? 0 : atoi(envMinFreq);
   if (minFreq <= 0)
      minFreq = defaultMinFreq;

   // minimum required header/preheader frequency ratio
   // (i.e. estimated average iterations per entry into the loop)
   const float defaultMinIters = 2.f;
   static const char * const envMinIters =
      feGetEnv("TR_loopStriderSignExtensionMinimumIterations");

   float minIters = envMinIters == NULL ? 0.f : float(atof(envMinIters));
   if (minIters <= 0.f)
      minIters = defaultMinIters;

   // Check the loop and preheader frequencies
   uint16_t loopFreq = loop->getEntryBlock()->getFrequency();
   if (loopFreq <= 0)
      return true; // assume frequent when unknown

   if (loopFreq < minFreq)
      {
      dumpOptDetails(TR::comp(),
         "[Sign-Extn] skip loop %d due to frequency %d < %d\n",
         loop->getNumber(),
         loopFreq,
         minFreq);
      return false;
      }

   uint16_t entryFreq = preheader->getFrequency();
   if (entryFreq <= 0)
      return true; // assume frequent when unknown

   float estimatedAverageIters = float(loopFreq) / float(entryFreq);
   if (estimatedAverageIters < minIters)
      {
      dumpOptDetails(TR::comp(),
         "[Sign-Extn] skip loop %d due to estimated average iterations %f < %f\n",
         loop->getNumber(),
         estimatedAverageIters,
         minIters);
      return false;
      }

   return true;
   }

/**
 * Identify desirable (loop, IV) pairs and replace each IV within its loop.
 *
 * This is the first of two phases of sign extension elimination. The second is
 * eliminateSignExtensions(TR::NodeChecklist&). No sign extensions are
 * eliminated yet, and in fact some are introduced.
 *
 * Upon finding a 32-bit IV \em i that should be replaced with a 64-bit \em k
 * in a particular loop, all mentions of \em i are rewritten <em>within that
 * loop</em> in terms of \em k.
 *
 * Since code outside the loop is still dealing with \em i, its value is
 * communicated into the loop body by sign-extending into \em k in the
 * preheader, and back out by truncating \em k into \em i along each loop
 * exit.
 *
 * The lone store of \em i is changed from
 *
   \verbatim
      n1n istore i
      n2n   [...]
   \endverbatim
 *
 * into
 *
   \verbatim
      n1n lstore k
            i2l
      n2n     [...]
   \endverbatim
 *
 * (Note that this transformation introduces a new sign extension, but it
 * should always be eliminable.)
 *
 * Each load of \em i is changed from
 *
   \verbatim
      n1n iload i
   \endverbatim
 *
 * into
 *
   \verbatim
      n1n l2i
      *     lload k
   \endverbatim
 *
 * where the \c lload marked with \c \* is fresh. Each such new \c lload
 * "belongs" to its original \c iload, in particular sharing its point of
 * evaluation.
 *
 * Every \c iload node that becomes an \c l2i node in this way is added to the
 * \p exLoads checklist, which allows later elimination of sign-extensions.
 *
 * \param[in]  loopStructure where to look for loops
 * \param[out] exLoads       checklist of \c l2i nodes that used to be \c iload
 * \see replaceLoadsInStructure(TR_Structure*, int32_t, TR::SymbolReference*, TR::NodeChecklist&, TR::NodeChecklist&)
 * \see extendIVsOnLoopEntry(const TR::list<std::pair<int32_t, int32_t> >&, TR::Block*)
 * \see truncateIVsOnLoopExit(const TR::list<std::pair<int32_t, int32_t> >&, TR_RegionStructure*)
 * \see eliminateSignExtensions(TR::NodeChecklist&)
 */
void TR_LoopStrider::detectLoopsForIndVarConversion(
   TR_Structure *loopStructure,
   TR::NodeChecklist &exLoads)
   {

   // 64-bit
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   // Stress mode: choose to replace *every* possible candidate 32-bit IV with
   // a 64-bit one, regardless of benefit/detriment.
   static const char *stressModeEnv = feGetEnv("TR_stressLoopStriderSignExtension");
   static const bool stressMode = stressModeEnv != NULL && stressModeEnv[0] != '\0';

   TR_RegionStructure *regionStructure = loopStructure->asRegion();
   if (regionStructure)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         detectLoopsForIndVarConversion(node->getStructure(), exLoads);
      }

   TR_BlockStructure *loopInvariantBlock = NULL;

   if (!regionStructure ||
       !regionStructure->getParent() ||
       !regionStructure->isNaturalLoop())
      return;

   if (regionStructure->getEntryBlock()->isCold() && !stressMode)
      return;

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   regionStructure->getBlocks(&blocksInRegion);
   ListIterator<TR::Block> bi (&blocksInRegion);
   TR::Block *nextBlock;
   for (nextBlock = bi.getCurrent(); nextBlock; nextBlock = bi.getNext())
      {
      if (nextBlock->hasExceptionPredecessors())
         return;

      //bail out if there is any exception edges to OSRCatchBlock because there is no
      //safe way to split those edges. Also, there is no need to check if the OSRCatchBlock is
      //within the loop because loop strider already skips any loop that contains a
      //catch block
      if (nextBlock->hasExceptionSuccessors())
         {
         TR_SuccessorIterator sit(nextBlock);
         for (TR::CFGEdge *e = sit.getFirst(); e != NULL; e = sit.getNext())
            {
            TR::Block *dest = toBlock(e->getTo());
            if (dest->isOSRCatchBlock())
               {
               if (trace())
                  traceMsg(comp(),"reject loop %d because dest (block_%d) of an exit edge is OSRCatchBlock\n", loopStructure->getNumber(), dest->getNumber());
               return;
               }
            }
         }
      }

   TR_RegionStructure *parentStructure = regionStructure->getParent()->asRegion();
   TR_StructureSubGraphNode *subNode = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
      {
      if (subNode->getNumber() == loopStructure->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
         loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
      }

   if (loopInvariantBlock)
      {
      // If this loop looks like it barely iterates, leave it alone.
      if (!stressMode && !isLoopRelativelyFrequent(loopStructure, loopInvariantBlock->getBlock()))
         return;

      // We need the latest symref count because the recursive call above may
      // have created new symrefs and created stores to them within this loop's
      // body. If so, the new symref numbers will be used as indices into
      // _storeTrees, which must therefore be large enough to accomodate them.
      int32_t symRefCount = comp()->getSymRefCount();

      initializeSymbolsWrittenAndReadExactlyOnce(symRefCount, growable);

      if (_storeTreesList)
         _storeTreesList->clear();
      _storeTreesList = NULL;
      //_loadUsedInNewLoopIncrementList = NULL;

      _reassociatedAutos->clear();
      _hoistedAutos->clear();

      _numberOfLinearExprs = 0;
      _autosAccessed = NULL;

      int32_t isPredictableLoop = checkLoopForPredictability(loopStructure, loopInvariantBlock->getBlock(), NULL, false);
      vcount_t visitCount = 0;

      // sign-extension elimination on 64-bit
      // attempt conversion of possible 'int' induction variables to 'long's
      TR_UseDefInfo *info = optimizer()->getUseDefInfo();
      if (usingAladd && info)
         {
         if (isPredictableLoop > 0)
            {
            TR::list<std::pair<int32_t, int32_t> > widenedIVs(
               getTypedAllocator<std::pair<int32_t, int32_t> >(comp()->allocator()));
            TR::SparseBitVector::Cursor cursor(_writtenExactlyOnce);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t nextInductionVariable = cursor;
               TR::TreeTop *storeTree = _storeTrees[nextInductionVariable];
               TR_ASSERT(
                  storeTree != NULL,
                  "missing store tree for #%d in _writtenExactlyOnce\n",
                  nextInductionVariable);

               // Only consider 32-bit integer autos. In particular note that
               // any temps newly created by the above recursive calls are
               // Int64, and their stores are all lstore.
               TR::DataType ivType = storeTree->getNode()->getDataType();
               if (ivType != TR::Int32)
                  continue;

               bool storeInRequiredForm = isStoreInRequiredForm(nextInductionVariable, loopStructure);
               // verify induction variable candidate is not a parameter
               if (storeInRequiredForm && performTransformation(comp(), "%s Analyzing candidate: %d in loop: %d for conversion\n", OPT_DETAILS, _loopDrivingInductionVar, loopStructure->getNumber()))
                  {
                  TR_ASSERT(
                     _loopDrivingInductionVar == nextInductionVariable,
                     "_loopDrivingInductionVar #%d does not match nextInductionVariable #%d\n",
                     _loopDrivingInductionVar,
                     nextInductionVariable);

                  visitCount = comp()->incVisitCount();

                  _isInductionVariableMorphed = false;

                  // examine expressions containing induction variable that need to be changed
                  morphExpressionsLinearInInductionVariable(loopStructure, visitCount);

                  if (_isInductionVariableMorphed || stressMode)
                     {
                     if (checkStoreOfIndVar(_storeTrees[nextInductionVariable]->getNode()))
                        {
                        // found a chance to convert the induction variable
                        TR::SymbolReference *ext =
                           comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int64);
                        _newTempsCreated = true;
                        walkTreesAndFixUseDefs(loopStructure, ext, exLoads);
                        widenedIVs.push_back(
                           std::make_pair(
                              nextInductionVariable,
                              ext->getReferenceNumber()));
                        }
                     }
                  }
               }

            if (!widenedIVs.empty())
               {
               assertStructureDoesNotMentionOriginals(regionStructure, widenedIVs);
               extendIVsOnLoopEntry(widenedIVs, loopInvariantBlock->getBlock());
               truncateIVsOnLoopExit(widenedIVs, regionStructure);
               if (trace())
                  comp()->dumpMethodTrees("trees after extending in this loop");
               }
            }
         }
      }
   }

/**
 * Insert 32-bit to 64-bit sign-extending copies into \p preheader.
 *
 * \param ivs       the (original, new) symbol reference number pairs to convert
 * \param preheader the loop's preheader, where conversions should be inserted
 * \see detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&)
 */
void TR_LoopStrider::extendIVsOnLoopEntry(
   const TR::list<std::pair<int32_t, int32_t> > &ivs,
   TR::Block *preheader)
   {
   TR::TreeTop *tt = preheader->getLastRealTreeTop();
   if (!tt->getNode()->getOpCode().isBranch())
      tt = tt->getNextTreeTop();

   TR::Node *bciNode = preheader->getExit()->getNode();
   for (auto iv = ivs.begin(); iv != ivs.end(); ++iv)
      convertIV(bciNode, tt, iv->first, iv->second, TR::i2l);
   }

/**
 * Insert 64-bit to 32-bit truncating copies along each exit from \p loop.
 *
 * \param ivs  the (original, new) symbol reference number pairs to convert
 * \param loop the loop whose exits must set the original variables
 * \see detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&)
 */
void TR_LoopStrider::truncateIVsOnLoopExit(
   const TR::list<std::pair<int32_t, int32_t> > &ivs,
   TR_RegionStructure *loop)
   {
   TR_ScratchList<TR::Block> loopBlocks(trMemory());
   loop->getBlocks(&loopBlocks);

   ListIterator<TR::Block> lit(&loopBlocks);

   TR::BlockChecklist inLoop(comp());
   for (TR::Block *b = lit.getFirst(); b != NULL; b = lit.getNext())
      inLoop.add(b);

   for (TR::Block *src = lit.getFirst(); src != NULL; src = lit.getNext())
      {
      // Defer any edge splitting until iteration is finished.
      TR_ScratchList<TR::Block> exits(trMemory());
      TR_SuccessorIterator sit(src);
      for (TR::CFGEdge *e = sit.getFirst(); e != NULL; e = sit.getNext())
         {
         TR::Block *dest = toBlock(e->getTo());
         if (!inLoop.contains(dest))
            exits.add(dest);
         }

      ListIterator<TR::Block> eit(&exits);
      for (TR::Block *dest = eit.getFirst(); dest != NULL; dest = eit.getNext())
         {
         // (src -> dest) is a loop exit. Need to insert a conversion "along"
         // it. Note that src definitely has another successor, since it's in
         // loop, so the conversion can never simply be put at the end of src.
         int preds = dest->getPredecessors().size()
            + dest->getExceptionPredecessors().size();
         if (preds > 1)
            {
            TR::Block *origDest = dest;
            dest = src->splitEdge(src, dest, comp());
            dumpOptDetails(comp(),
               "[Sign-Extn] split loop exit: block_%d [-> block_%d] -> block_%d\n",
               src->getNumber(),
               dest->getNumber(),
               origDest->getNumber());
            }

         // loop never exits directly to the method exit block, i.e. via return
         // or throw, because that requires a catch block in the body, which
         // would be rejected by detectLoopsForIndVarConversion. So dest will
         // always have an entry tree.
         TR::TreeTop *tt = dest->getEntry()->getNextTreeTop();
         TR::Node *bciNode = dest->getEntry()->getNode();
         for (auto iv = ivs.begin(); iv != ivs.end(); ++iv)
            convertIV(bciNode, tt, iv->second, iv->first, TR::l2i);
         }
      }
   }

/**
 * Emit a variable conversion tree: \p ivOut <- \p op \p ivIn.
 *
 * \param bciNode the node whose bci to copy onto the newly created nodes
 * \param nextTT  the treetop before which the conversion is inserted
 * \param ivIn    the symbol reference number of the input variable
 * \param ivOut   the symbol reference number of the output variable
 * \param op      the conversion operation
 */
void TR_LoopStrider::convertIV(
   TR::Node *bciNode,
   TR::TreeTop *nextTT,
   int32_t ivIn,
   int32_t ivOut,
   TR::ILOpCodes op)
   {
   TR::SymbolReferenceTable *srtab = comp()->getSymRefTab();
   TR::SymbolReference *in = srtab->getSymRef(ivIn);
   TR::SymbolReference *out = srtab->getSymRef(ivOut);
   dumpOptDetails(comp(),
      "[Sign-Extn] convert at loop boundary: #%d <- %s(#%d) in block_%d\n",
      ivOut,
      TR::ILOpCode(op).getName(),
      ivIn,
      nextTT->getEnclosingBlock()->getNumber());
   TR::Node *conv = TR::Node::createStore(bciNode,
      out,
      TR::Node::create(bciNode, op, 1, TR::Node::createLoad(bciNode, in)));
   nextTT->insertBefore(TR::TreeTop::create(comp(), conv));
   }

void TR_LoopStrider::createConstraintsForNewInductionVariable(TR_Structure *loopStructure, TR::SymbolReference *newSymRef, TR::SymbolReference *oldSymRef)
   {

   TR_InductionVariable *v = loopStructure->asRegion()->findMatchingIV(oldSymRef);
   if (v)
      {
      // old constraints
      TR::VPConstraint *entryVal = v->getEntry();
      TR::VPConstraint *exitVal = v->getExit();
      TR::VPConstraint *incr = v->getIncr();

      //new constraints
      TR::VPConstraint *newEntryVal = genVPLongRange(entryVal, 1, 0);  // create the new entry constraint
      TR::VPConstraint *newExitVal = genVPLongRange(entryVal, 1, 0); // create the new exit constraint
      TR::VPConstraint *newIncr = genVPLongRange(incr, 1, 0); // create the new incr constraint

      TR_InductionVariable *oldIV = loopStructure->asRegion()->findMatchingIV(oldSymRef);

      // add the new constraint and new induction variable
      TR_InductionVariable *newv = new (trHeapMemory()) TR_InductionVariable(newSymRef->getSymbol()->castToRegisterMappedSymbol(), newEntryVal, newExitVal, newIncr,
                                                                             oldIV ? oldIV->isSigned() : TR_maybe);
      loopStructure->asRegion()->addInductionVariable(newv);
      }
   }


void TR_LoopStrider::morphExpressionsLinearInInductionVariable(TR_Structure *structure, vcount_t visitCount)
   {
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         _currTree = currentTree;
         morphExpressionLinearInInductionVariable(NULL, -1, currentNode, visitCount);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
          morphExpressionsLinearInInductionVariable(node->getStructure(), visitCount);
      }


   }



bool TR_LoopStrider::morphExpressionLinearInInductionVariable(TR::Node *parent, int32_t childNum, TR::Node *node, vcount_t visitCount)
   {
   // for aladds
   bool usingAladd = (TR::Compiler->target.is64Bit()) ?
                     true : false;

   bool examineChildren = true;
   if (node->getVisitCount() == visitCount)
      examineChildren = false;

   node->setVisitCount(visitCount);
   if (cg()->supportsInternalPointers() &&
       node->isInternalPointer() &&
       node->getFirstChild()->getOpCode().isLoadVar() &&
       node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
       //node->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm() &&
       _neverWritten->get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
      {
      node = node->getSecondChild();
      }

   // pattern match trees
   // ladd      <-- node
   //   lmul    <-- firstChild
   //     i2l
   //       <index> ; where <index>==> iload;   iadd ;   isub
   //     lconst                         iload    iload
   //   lconst                           iconst   iconst

   if (node->getOpCodeValue() == TR::ladd || node->getOpCodeValue() == TR::lsub)
      {
      TR::Node *secondChild = node->getSecondChild();
      if (secondChild->getOpCodeValue() == TR::lconst)
         {
         TR::Node *firstChild = node->getFirstChild();
         if (firstChild->getOpCodeValue() == TR::lmul || firstChild->getOpCodeValue() == TR::lshl)                          {
            bool isLinearInInductionVariable;
            TR::Node *indVarTree = NULL;
            if (firstChild->getFirstChild()->getOpCodeValue() == TR::i2l)
               {
               indVarTree = firstChild->getFirstChild()->getFirstChild();
               isLinearInInductionVariable = checkExpressionForInductionVariable(indVarTree);
               if (isLinearInInductionVariable &&
                   firstChild->getSecondChild()->getOpCodeValue() == TR::lconst)
                  {
                  examineChildren = false;

                  // Request a sign-extension only when there is no arithmetic
                  // here, or the arithmetic is known not to overflow.
                  //
                  // If we're looking at possibly overflowing arithmetic,
                  // another expression still might request a sign-extension,
                  // in which case indVarTree will continue to do 32-bit
                  // arithmetic consuming l2i of the new 64-bit variable.
                  TR::Node *ivLoad = getInductionVariableNode(indVarTree);
                  if (ivLoad != NULL && (ivLoad == indVarTree || indVarTree->cannotOverflow()))
                     _isInductionVariableMorphed = true;
                  }
               }
            }
         }
      }
   // collect uses of ind var
   // parent
   //   iload  <-- node
   else if (node->getOpCodeValue() == TR::iload)
      {
      examineChildren = false;
      }

   if (examineChildren)
      {
      int32_t i;
      for (i = 0; i < node->getNumChildren(); i++)
          morphExpressionLinearInInductionVariable(node, i, node->getChild(i), visitCount);
      }
   return true;
   }




TR::Node *TR_LoopStrider::getInductionVariableNode(TR::Node *node)
   {
   TR::Node *indVarNode = NULL;
   if (node->getOpCodeValue() == TR::iload)
      {
      if (node->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar)
         indVarNode = node;
      }
   else if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::isub)
      {
      if (node->getFirstChild()->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar)
         indVarNode = node->getFirstChild();
      }
   return indVarNode;
   }


/**
 * Replace \c _loopDrivingInductionVar with \p newSymbolReference in \p loopStructure.
 *
 * \param[in]     loopStructure
 * \param[in]     newSymbolReference
 * \param[in,out] exLoads
 * \see detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&)
 */
void TR_LoopStrider::walkTreesAndFixUseDefs(
   TR_Structure *loopStructure,
   TR::SymbolReference *newSymbolReference,
   TR::NodeChecklist &exLoads)
   {
   TR::DebugCounter::incStaticDebugCounter(comp(),
      TR::DebugCounter::debugCounterName(comp(),
         "loopStrider.widen/(%s)/%s/loop=%d/iv=%d",
         comp()->signature(),
         comp()->getHotnessName(comp()->getMethodHotness()),
         loopStructure->getEntryBlock()->getNumber(),
         _loopDrivingInductionVar));

   // Change (istore i ..) to (lstore k (i2l ..))
   TR::Node *storeNode = _storeTrees[_loopDrivingInductionVar]->getNode();
   TR_ASSERT(
      storeNode->getOpCodeValue() == TR::istore,
      "storeNode opcode is %s; expected istore\n",
      storeNode->getOpCode().getName());
   TR_ASSERT(
      storeNode->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar,
      "storeNode stores to #%d; expected #%d\n",
      storeNode->getSymbolReference()->getReferenceNumber(),
      _loopDrivingInductionVar);

   TR::Node *newIntValue = storeNode->getFirstChild();
   storeNode->setAndIncChild(0, TR::Node::create(storeNode, TR::i2l, 1, newIntValue));
   newIntValue->decReferenceCount();
   storeNode->setSymbolReference(newSymbolReference);
   TR::Node::recreate(storeNode, TR::lstore);

   TR::NodeChecklist visited(comp());
   replaceLoadsInStructure(
      loopStructure,
      _loopDrivingInductionVar,
      newIntValue,
      newSymbolReference,
      exLoads,
      visited);

   TR::SymbolReference *oldSymRef = comp()->getSymRefTab()->getSymRef(_loopDrivingInductionVar);
   createConstraintsForNewInductionVariable(loopStructure, newSymbolReference, oldSymRef);
   }

/**
 * Replace occurrences of \p iv with \p newSR within \p structure.
 *
 * Each load of \p iv becomes \c l2i of \c lload \p newSR. These are the (ex-)
 * \c iload nodes that are added to \p exLoads.
 *
 * Signed 32-bit comparisons made directly against \p iv, or directly against
 * \p defRHS, are replaced by the corresponding 64-bit comparisons.
 *
 * \param[in]     structure  where to change loads
 * \param[in]     iv         the symbol reference number of the loads to change
 * \param[in]     defRHS     the right-hand side of the lone store to \p iv
 * \param[in]     newSR      the new symbol reference to use in place of \p iv
 * \param[in,out] exLoads    the checklist to which the changed loads are added
 * \param[in,out] visited    an empty checklist
 * \see detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&)
 */
void TR_LoopStrider::replaceLoadsInStructure(
   TR_Structure *structure,
   int32_t iv,
   TR::Node *defRHS,
   TR::SymbolReference *newSR,
   TR::NodeChecklist &exLoads,
   TR::NodeChecklist &visited)
   {
   if (structure->asBlock() != NULL)
      {
      TR::Block *b = structure->asBlock()->getBlock();
      TR::TreeTop *end = b->getExit();
      for (TR::TreeTop *tt = b->getEntry(); tt != end; tt = tt->getNextTreeTop())
         replaceLoadsInSubtree(tt->getNode(), iv, defRHS, newSR, exLoads, visited);
      }
   else
      {
      TR_RegionStructure::Cursor it(*structure->asRegion());
      for (TR_StructureSubGraphNode *n = it.getFirst(); n != NULL; n = it.getNext())
         replaceLoadsInStructure(n->getStructure(), iv, defRHS, newSR, exLoads, visited);
      }
   }

// node-recursive implementation of replaceLoadsInStructure (above)
void TR_LoopStrider::replaceLoadsInSubtree(
   TR::Node *node,
   int32_t iv,
   TR::Node *defRHS,
   TR::SymbolReference *newSR,
   TR::NodeChecklist &exLoads,
   TR::NodeChecklist &visited)
   {
   if (visited.contains(node))
      return;

   visited.add(node);
   for (int i = 0; i < node->getNumChildren(); i++)
      replaceLoadsInSubtree(node->getChild(i), iv, defRHS, newSR, exLoads, visited);

   if (node->getOpCodeValue() == TR::iload
       && node->getSymbolReference()->getReferenceNumber() == iv)
      {
      TR::Node *lload = TR::Node::createLoad(node, newSR);
      TR::Node::recreate(node, TR::l2i);
      node->setNumChildren(1);
      node->setAndIncChild(0, lload);
      exLoads.add(node);
      }

   widenComparison(node, iv, defRHS, exLoads);
   }

/**
 * Widen comparisons against \p iv.
 *
 * Signed 32-bit comparisons made directly against \p iv, or directly against
 * the node that produces the new value of \p iv, are changed into the
 * corresponding 64-bit comparisons, by wrapping each child in \c i2l.
 *
 * For example, when replacing the 32-bit \c i with the 64-bit \c k, consider
 * the following comparison:
 *
   \verbatim
      ificmpge
        iload i
        iload n
   \endverbatim
 *
 * In the course of replaceInStructure(TR_Structure*), it will be rewritten as:
 *
   \verbatim
      ificmpge
        l2i
          lload k
        iload n
   \endverbatim
 *
 * In order to make it  into a more obvious test of \c k, we change it into a
 * 64-bit comparison using \c i2l like so:
 *
   \verbatim
      iflcmpge
        i2l
          l2i
            lload k
        i2l
          iload n
   \endverbatim
 *
 * Because the \c l2i node here is an ex-load, it will cancel the \c i2l later:
 *
   \verbatim
      iflcmpge
        lload k
        i2l
          iload n
   \endverbatim
 *
 * \param[in] node     the possible comparison node to process
 * \param[in] iv       the symbol reference number of the loads being changed
 * \param[in] defRHS   the right-hand side of the lone store to \p iv
 * \param[in] exLoads  the set of \c l2i nodes that were loads of \p iv
 */
void TR_LoopStrider::widenComparison(
   TR::Node *node,
   int32_t iv,
   TR::Node *defRHS,
   TR::NodeChecklist &exLoads)
   {
   static const char * const disableEnv =
      feGetEnv("TR_disableLoopStriderWidenComparison");
   static const bool disable = disableEnv != NULL && disableEnv[0] != '\0';
   if (disable)
      return;

   TR::ILOpCode op = node->getOpCode();
   TR::ILOpCodes icmp = op.isIf() ? op.convertIfCmpToCmp() : op.getOpCodeValue();
   TR::ILOpCodes lcmp = TR::BadILOp;
   switch (icmp)
      {
      case TR::icmpeq: lcmp = TR::lcmpeq; break;
      case TR::icmpne: lcmp = TR::lcmpne; break;
      case TR::icmplt: lcmp = TR::lcmplt; break;
      case TR::icmple: lcmp = TR::lcmple; break;
      case TR::icmpgt: lcmp = TR::lcmpgt; break;
      case TR::icmpge: lcmp = TR::lcmpge; break;
      default: return; // Not a 32-bit signed integer comparison.
      }

   TR::Node *left = node->getChild(0);
   TR::Node *right = node->getChild(1);
   bool isLeftIV = left == defRHS || exLoads.contains(left);
   bool isRightIV = right == defRHS || exLoads.contains(right);
   if (!isLeftIV && !isRightIV)
      return;

   if (op.isIf())
      lcmp = TR::ILOpCode(lcmp).convertCmpToIfCmp();

   if (!performTransformation(comp(),
         "%s [Sign-Extn] Changing n%un %s into %s\n",
         optDetailString(),
         node->getGlobalIndex(),
         node->getOpCode().getName(),
         TR::ILOpCode(lcmp).getName()))
      return;

   TR::Node::recreate(node, lcmp);
   node->setAndIncChild(0, TR::Node::create(node, TR::i2l, 1, left));
   node->setAndIncChild(1, TR::Node::create(node, TR::i2l, 1, right));
   left->decReferenceCount();
   right->decReferenceCount();
   }

/**
 * Find and eliminate sign extensions that are no longer necessary.
 *
 * This is the second of two phases of sign extension elimination. The first is
 * detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&), which
 * will have replaced 32-bit IVs with new 64-bit ones within certain loop
 * bodies.
 *
 * This function traverses the trees looking at each parent-to-child link once.
 * Whenever a child is an \c i2l, it attempts to replace that occurrence with
 * an equivalent node that can be computed without \c i2l.
 *
 * Whenever an occurrence of \c i2l is eliminated this way, arithmetic nodes
 * beneath the original \c i2l are transmuted in-place into truncations \c l2i
 * of their corresponding 64-bit subtrees. This preserves pre-existing
 * commoning where otherwise it would be destroyed (and difficult to recover).
 *
 * \param exLoads the set of \c l2i nodes that used to be \c iload
 * \see detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&)
 * \see signExtend(TR::Node*, TR::NodeChecklist&, SignExtMemo&)
 * \see transmuteDescendantsIntoTruncations(TR::Node*, TR::Node*)
 */
void TR_LoopStrider::eliminateSignExtensions(TR::NodeChecklist &exLoads)
   {
   TR::NodeChecklist visited(comp());

   auto alloc = comp()->allocator();
   auto mapAlloc = getTypedAllocator<std::pair<ncount_t, TR::Node*> >(alloc);
   SignExtMemo signExtMemo(std::less<ncount_t>(), mapAlloc);

   TR::TreeTop *start = comp()->getStartTree();
   for (TR::TreeTop *tt = start; tt != NULL; tt = tt->getNextTreeTop())
      eliminateSignExtensionsInSubtree(tt->getNode(), exLoads, visited, signExtMemo);

   // References from signExtMemo were included in reference counts; so now
   // release all of them. This avoids incorrectly inflating the reference
   // count of any pre-existing node that was wrapped in an i2l but for which
   // the new i2l node was not used.
   for (auto it = signExtMemo.begin(); it != signExtMemo.end(); ++it)
      it->second.extended->recursivelyDecReferenceCount();

   if (trace())
      comp()->dumpMethodTrees("trees after eliminating sign extensions");
   }

// node-recursive implementation of eliminateSignExtensions (above)
void TR_LoopStrider::eliminateSignExtensionsInSubtree(
   TR::Node *parent,
   TR::NodeChecklist &exLoads,
   TR::NodeChecklist &visited,
   SignExtMemo &signExtMemo)
   {
   if (visited.contains(parent))
      return;

   visited.add(parent);
   for (int i = 0; i < parent->getNumChildren(); i++)
      {
      TR::Node *node = parent->getChild(i);
      eliminateSignExtensionsInSubtree(node, exLoads, visited, signExtMemo);
      if (node->getOpCodeValue() != TR::i2l)
         continue;

      TR::Node *child = node->getFirstChild();
      SignExtEntry entry = signExtend(child, exLoads, signExtMemo);
      TR::Node *extended = entry.extended;
      if (extended == NULL)
         continue;

      if (!performTransformation(comp(),
            "%s [Sign-Extn] Replacing occurrence of n%un i2l with n%un as %dth child of n%un\n",
            optDetailString(),
            node->getGlobalIndex(),
            extended->getGlobalIndex(),
            i,
            parent->getGlobalIndex()))
          continue;

      // Note that node and extended consume precisely the same lloads, whose
      // points of evaluation are therefore left unchanged by the substitution.
      // Every other node beneath either is referentially transparent.
      assertSubstPreservesEvalOrder(node, extended, "i2l elimination");
      parent->setAndIncChild(i, extended);
      transmuteDescendantsIntoTruncations(child, extended);
      node->recursivelyDecReferenceCount();
      }
   }

/**
 * Attempt to fold an \c i2l operation into \p node.
 *
 * A successful result is a long-typed subtree which computes the same value as
 * \c i2l of \p node, but which doesn't contain any \c i2l operations, or which
 * contains exactly one \c i2l operation and fewer \c l2i operations than
 * appear under \p node.
 *
 * The form of subtree into which an \c i2l may be folded is highly
 * constrained. The only acceptable nodes are constants, translated in the
 * obvious way; certain 32-bit arithmetic operations marked \c cannotOverflow;
 * and members of \p exLoads.
 *
 * For arithmetic, notice that
 *
   \verbatim
      i2l
        i{add,sub,mul} (cannotOverflow)
          left
          right
   \endverbatim
 *
 * can be transformed into
 *
   \verbatim
      l{add,sub,mul} (cannotOverflow)
        i2l
          left
        i2l
          right
   \endverbatim
 *
 * which now has more sign-extension operations than before. But if the newly
 * added \c i2l operations can be folded into each of \c left and \c right,
 * yielding \c left' and \c right', respectively, then we can get
 *
   \verbatim
      l{add,sub,mul} (cannotOverflow)
        left'
        right'
   \endverbatim
 *
 * The nodes in \p exLoads were populated by
 * detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&). Each such
 * node is, for some original 32-bit IV \em i and new 64-bit \em k to replace
 * it, of the form <tt>l2i (lload k)</tt>.
 *
 * The important property of one of these is that, if the original \em i were
 * still maintained throughout the loop, we would always have
 *
   \verbatim
      lload k = i2l (iload i)
   \endverbatim
 *
 * for the particular matching pair of \c lload and \c iload nodes. So \c i2l
 * folds into a member of \p exLoads as follows:
 *
   \verbatim
      i2l                  i2l                  i2l               lload k
        l2i          ==>     l2i          ==>     iload i   ==>
          lload k              i2l
                                 iload i
   \endverbatim
 *
 * The result is the particular \c lload node that appeared as the child of the
 * \c l2i node from \p exLoads.
 *
 * Folding \c i2l of an ex-load in this way eliminates not just the \c i2l, but
 * also the \c l2i.
 *
 * For arithmetic nodes, sometimes it isn't possible to eliminate an \c i2l
 * entirely, but it \em is possible to eliminate \c i2l in one child. Take for
 * example the left child,
 *
   \verbatim
      i2l                     l{add,sub,mul}          l{add,sub,mul}
        i{add,sub,mul}          i2l                     left'
          left          ==>       left          ==>     i2l
          right                 i2l                       right
                                  right
   \endverbatim
 *
 * The result is no worse than the original, and it may eliminate an \c l2i on
 * the left. So this is done in cases where it does in fact eliminate an \c l2i.
 *
 * A successful result in general shares as descendants with \p node the
 * \c lload nodes from members of \p exLoads, and nodes in subtrees that were
 * wrapped in "moved" \c i2l operations (if any), and no others. All other
 * nodes beneath either \p node or the result are referentially transparent.
 * This means that a reference to \c i2l of \p node may be simply replaced by a
 * reference to the resulting long-type subtree.
 *
 * A successful result has a structure parallel to that of \p node. Non-leaf
 * nodes in the result can only be arithmetic nodes, corresponding exactly to
 * arithmetic nodes in the same position beneath \p node; or \c i2l nodes
 * corresponding exactly to their own children. For example, assuming implicit
 * \c cannotOverflow flags,
 *
   \verbatim
      isub                          lsub
        imul            (maps to)     lmul
          l2i              ==>          lload k
            lload k
          iload x                       i2l
                                          iload  x
        iconst -1                     lconst -1
   \endverbatim
 *
 * In particular, it is straightforward to pair up the original 32-bit
 * arithmetic nodes with their long-typed counterparts.
 *
 * The results are memoized for efficiency in the presence of commoning visible
 * beneath \p node, and also to preserve any such commoning. For example, again
 * assuming \c cannotOverflow,
 *
   \verbatim
      imul                          lmul
        isub            (maps to)     lsub
          l2i              ==>          lload k
            lload k                     lconst -1
          iconst -1                   =>lsub
        =>isub
   \endverbatim
 *
 * Furthermore, if in this example there is some later \c i2l of the \c isub,
 * it will map to the same \c lsub node.
 *
 * \param[in]     node    the node to convert
 * \param[in]     exLoads the set of \c l2i nodes that used to be \c iload
 * \param[in,out] memo    a map for memoization
 * \return the equivalent 64-bit node, or \c NULL for failure
 * \see detectLoopsForIndVarConversion(TR_Structure*, TR::NodeChecklist&)
 * \see eliminateSignExtensions(TR::NodeChecklist&)
 * \see transmuteDescendantsIntoTruncations(TR::Node*, TR::Node*)
 */
TR_LoopStrider::SignExtEntry TR_LoopStrider::signExtend(
   TR::Node *node,
   TR::NodeChecklist &exLoads,
   SignExtMemo &memo)
   {
   // With this memoization there is also no need to remember the replacement
   // node for a particular i2l node in eliminateSignExtensions, since a later
   // occurrence of the same i2l will ask about the same child, and get back
   // the same 64-bit equivalent node remembered for that child.
   SignExtMemo::iterator existing = memo.find(node->getGlobalIndex());
   if (existing != memo.end())
      return existing->second;

   SignExtEntry result;
   switch (node->getOpCodeValue())
      {
      case TR::iconst:
         result.extended = TR::Node::lconst(node, (int64_t)node->getConst<int32_t>());
         result.cancelsExt = true;
         break;

      case TR::l2i:
         if (exLoads.contains(node))
            {
            TR::Node *child = node->getFirstChild();
            TR_ASSERT(
               child->getOpCodeValue() == TR::lload,
               "exload l2i n%un's child should be an lload\n",
               node->getGlobalIndex());
            result.extended = child;
            result.cancelsExt = true;
            result.cancelsTrunc = true;
            }
         break;

      case TR::iadd:
         result = signExtendBinOp(TR::ladd, node, exLoads, memo);
         break;

      case TR::isub:
         result = signExtendBinOp(TR::lsub, node, exLoads, memo);
         break;

      case TR::imul:
         result = signExtendBinOp(TR::lmul, node, exLoads, memo);
         break;

      default:
         break; // just don't populate result
      }

   if (result.extended != NULL)
      {
      result.extended->incReferenceCount();
      memo.insert(std::make_pair(node->getGlobalIndex(), result));
      if (trace())
         traceMsg(comp(),
            "[Sign-Extn] sign-extended n%un %s into n%un %s\n",
            node->getGlobalIndex(),
            node->getOpCode().getName(),
            result.extended->getGlobalIndex(),
            result.extended->getOpCode().getName());
      }

   TR_ASSERT(
      (result.extended != NULL) == (result.cancelsExt || result.cancelsTrunc),
      "n%un %s should extend if and only if a conversion is canceled\n",
      node->getGlobalIndex(),
      node->getOpCode().getName());

   return result;
   }

// part of implementation of signExtend (above)
TR_LoopStrider::SignExtEntry TR_LoopStrider::signExtendBinOp(
   TR::ILOpCodes op64,
   TR::Node *node,
   TR::NodeChecklist &exLoads,
   SignExtMemo &memo)
   {
   const SignExtEntry rejected;
   if (!node->cannotOverflow())
      return rejected;

   static const char * const moveDisableEnv =
      feGetEnv("TR_disableLoopStriderMoveSignExtIntoChild");
   static const bool moveDisable =
      moveDisableEnv != NULL && moveDisableEnv[0] != '\0';

   TR::Node * const origLeft = node->getChild(0);
   const SignExtEntry left = signExtend(origLeft, exLoads, memo);
   if (moveDisable && !left.cancelsExt)
      return rejected;

   TR::Node * const origRight = node->getChild(1);
   const SignExtEntry right = signExtend(origRight, exLoads, memo);
   if (moveDisable && !right.cancelsExt)
      return rejected;

   // Never push i2l down into the children in cases where it would increase
   // the number of i2l nodes in the subtree.
   if (!left.cancelsExt && !right.cancelsExt)
      return rejected;

   // There may be no way to eliminate i2l entirely. But in that case we can
   // still push i2l down into the children, which might cancel an l2i beneath.
   TR::Node *extLeft = left.extended;
   TR::Node *extRight = right.extended;

   // Here at least one of extLeft and extRight is non-null, because either
   // left.cancelsExt or right.cancelsExt.
   if (extLeft == NULL)
      {
      if (right.cancelsTrunc)
         extLeft = TR::Node::create(origLeft, TR::i2l, 1, origLeft);
      else
         return rejected; // No point in moving i2l
      }
   else if (extRight == NULL)
      {
      if (left.cancelsTrunc)
         extRight = TR::Node::create(origRight, TR::i2l, 1, origRight);
      else
         return rejected; // No point in moving i2l
      }

   // Success
   TR::Node *longNode = TR::Node::create(node, op64, 2, extLeft, extRight);
   longNode->setFlags(node->getFlags());

   // This result satisfies (cancelsExt || cancelsTrunc) as asserted in
   // signExtend, because when i2l is pushed down into a child position, the
   // tests above ensure that the other child has cancelsTrunc.
   SignExtEntry result;
   result.extended = longNode;
   result.cancelsExt = left.cancelsExt && right.cancelsExt;
   result.cancelsTrunc = left.cancelsTrunc || right.cancelsTrunc;
   return result;
   }

/**
 * Transmute arithmetic nodes under \p intNode into \c l2i of long versions.
 *
 * This avoids duplicating arithmetic that is already commoned. The
 * corresponding nodes are found by making use of the convenient parallel
 * structure produced by signExtend(TR::Node*, TR::NodeChecklist&, SignExtMemo&).
 *
 * \param intNode  the original 32-bit int-typed node
 * \param longNode the 64-bit long-typed node corresponding to \p intNode
 */
void TR_LoopStrider::transmuteDescendantsIntoTruncations(
   TR::Node *intNode,
   TR::Node *longNode)
   {
   if (longNode->getOpCodeValue() == TR::i2l)
      {
      // A "moved" (rather than eliminated) i2l node. The child is in the
      // original subtree, and no conversion is needed.
      TR::Node *child = longNode->getChild(0);
      TR_ASSERT(
         child == intNode,
         "transmuting n%un %s does not match child n%un %s of n%un i2l\n",
         intNode->getGlobalIndex(),
         intNode->getOpCode().getName(),
         child->getGlobalIndex(),
         child->getOpCode().getName(),
         longNode->getGlobalIndex());
      return;
      }

   TR::ILOpCodes intOp = intNode->getOpCodeValue();
   if (intOp == TR::l2i || intOp == TR::iconst)
      return; // nothing to do

   TR_ASSERT(
      intOp == TR::ILOpCode::integerOpForLongOp(longNode->getOpCodeValue()),
      "attempted to transmute n%un %s into l2i of n%un %s\n",
      intNode->getGlobalIndex(),
      intNode->getOpCode().getName(),
      longNode->getGlobalIndex(),
      longNode->getOpCode().getName());

   int children = intNode->getNumChildren();
   TR_ASSERT(
      children == longNode->getNumChildren(),
      "transmute child count differs for n%un vs n%un\n",
      intNode->getGlobalIndex(),
      longNode->getGlobalIndex());

   for (int i = 0; i < children; i++)
      {
      TR::Node *intChild = intNode->getChild(i);
      TR::Node *longChild = longNode->getChild(i);
      transmuteDescendantsIntoTruncations(intChild, longChild);
      }

   if (!performTransformation(comp(),
         "%s [Sign-Extn] Transmuting n%un %s into l2i of n%un %s\n",
         optDetailString(),
         intNode->getGlobalIndex(),
         intNode->getOpCode().getName(),
         longNode->getGlobalIndex(),
         longNode->getOpCode().getName()))
      return;

   // Note that intNode and longNode consume precisely the same lloads, whose
   // points of evaluation are therefore left unchanged by this transmutation.
   // Every other node beneath either is referentially transparent.
   //
   // Technically intNode is becoming l2i of longNode, not longNode itself, but
   // there's no difference in the set of consumed lloads.
   assertSubstPreservesEvalOrder(intNode, longNode, "l2i transmutation");

   // Also important: transmuting here can't introduce a cycle, because all
   // descendants of longNode are long-typed, but intNode is int-typed.
   for (int i = 0; i < children; i++)
      {
      intNode->getChild(i)->recursivelyDecReferenceCount();
      intNode->setChild(i, NULL);
      }

   TR::Node::recreate(intNode, TR::l2i);
   intNode->setNumChildren(1);
   intNode->setAndIncChild(0, longNode);
   }


bool TR_LoopStrider::checkStoreOfIndVar(TR::Node *defNode)
   {
   TR::Node *firstChild = defNode->getFirstChild();
   if (firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub())
      {
      // pattern match
      // istore
      //   iadd/isub
      //     iload
      //     iconst
      if (firstChild->getFirstChild()->getOpCode().hasSymbolReference())
         {
         if (firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            if (firstChild->getFirstChild()->getSymbolReference()->getReferenceNumber() == _loopDrivingInductionVar)
               {
               if (!firstChild->cannotOverflow())
                  return false;
               return true;
               }
               else
                  return false;
            }
         // this might introduce a sign-extension inside the loop, so bail out
         // and let actual ind var analysis do its transformations.
         else
            return false;
         }
      else
         return false;
      }
   else
      return false;
   }

bool TR_LoopStrider::checkExpressionForInductionVariable(TR::Node *node)
   {

   if (node->getOpCodeValue() == TR::iload)
      {
      if (_writtenExactlyOnce.ValueAt(node->getSymbolReference()->getReferenceNumber()))
         return true;
      }
   else if (node->getOpCodeValue() == TR::iadd &&
            node->getFirstChild()->getOpCodeValue() == TR::iload &&
            node->getSecondChild()->getOpCodeValue() == TR::iconst)
      {
      if (_writtenExactlyOnce.ValueAt(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
         return true;
      }
   else if (node->getOpCodeValue() == TR::isub &&
            node->getFirstChild()->getOpCodeValue() == TR::iload &&
            node->getSecondChild()->getOpCodeValue() == TR::iconst)
      {
      if (_writtenExactlyOnce.ValueAt(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
         return true;
      }
   return false;
   }



int64_t TR_LoopStrider::getAdditiveTermConst(int32_t k)
   {
   TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs);
   TR::Node *node = (TR::Node*)(intptrj_t)_linearEquations[k][3];
   if (node == NULL)
      return 0;
   TR_ASSERT(isAdditiveTermConst(k), "LoopStrider: expecting constant term\n");
   if (node->getOpCodeValue() == TR::iconst)
      return node->getInt();
   else if (node->getOpCodeValue() == TR::lconst)
      return node->getLongInt();
   else
      {
      TR_ASSERT(false, "LoopStrider: term must be TR::iconst or TR::lconst\n");
      return 0;
      }
   }

bool TR_LoopStrider::isAdditiveTermEquivalentTo(int32_t k, TR::Node * node)
   {
   if (isAdditiveTermConst(k) && node->getOpCode().isLoadConst())
      {
      int32_t addConst = GET32BITINT(node);
      if (getAdditiveTermConst(k)  == addConst)
         return true;
      }
   else if (getAdditiveTermNode(k) &&
      getAdditiveTermNode(k)->getOpCode().hasSymbolReference() &&
      node->getOpCode().hasSymbolReference() &&
      getAdditiveTermNode(k)->getSymbolReference() == node->getSymbolReference())
      {
      if (getAdditiveTermNode(k)->getOpCodeValue() == node->getOpCodeValue())
         return true;
      }
   return false;
   }


TR::Node *TR_LoopStrider::duplicateMulTermNode(int32_t k, TR::Node *node, TR::DataType type)
   {
   TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs);
   TR::Node *new_node = ((TR::Node*)(intptrj_t)_linearEquations[k][2])->duplicateTree();
   new_node->setByteCodeIndex(node->getByteCodeIndex());
   new_node->setInlinedSiteIndex(node->getInlinedSiteIndex());

   if (new_node->getDataType() != type)
      new_node = TR::Node::create(TR::ILOpCode::getProperConversion(new_node->getDataType(), type, false /* !wantZeroExtension */),
         1, new_node);
   return new_node;
   }

int64_t TR_LoopStrider::getMulTermConst(int32_t k)
   {
   TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs);
   TR::Node *node = (TR::Node*)(intptrj_t)_linearEquations[k][2];
   TR_ASSERT(isMulTermConst(k), "LoopStrider: expecting constant term\n");
   if (node->getOpCodeValue() == TR::iconst)
      return node->getInt();
   else if (node->getOpCodeValue() == TR::lconst)
      return node->getLongInt();
   else
      {
      TR_ASSERT(false, "LoopStrider: term must be TR::iconst or TR::lconst\n");
      return 0;
      }
   }

bool TR_LoopStrider::isMulTermEquivalentTo(int32_t k, TR::Node * node)
   {
   if (isMulTermConst(k) && node->getOpCode().isLoadConst())
      {
      int32_t multiplicativeConstant = GET32BITINT(node);
      if (getMulTermConst(k)  == multiplicativeConstant)
         return true;
      }
   else if (getMulTermNode(k) &&
         getMulTermNode(k)->getOpCode().hasSymbolReference() &&
         node->getOpCode().hasSymbolReference() &&
         getMulTermNode(k)->getSymbolReference() == node->getSymbolReference())
      {
      if (getMulTermNode(k)->getOpCodeValue() == node->getOpCodeValue())
         return true;
      }
   return false;
   }

const char *
TR_LoopStrider::optDetailString() const throw()
   {
   return "O^O LOOP STRIDER: ";
   }

// end sign-extension

static inline int64_t ceiling(int64_t numer, int64_t denom)
   {
   return (numer % denom == 0) ?
      numer / denom :
      numer / denom + 1;
   }

TR_InductionVariableAnalysis::TR_InductionVariableAnalysis(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _ivs(0),
     _blockInfo(0),
     _dominators(NULL),
     _seenInnerRegionExit(0, trMemory(), stackAlloc, growable),
     _isOSRInduceBlock(0, trMemory(), stackAlloc, growable)
   {}

int32_t TR_InductionVariableAnalysis::perform()
   {
//    static char *doit = feGetEnv("TR_NewIVA");
//    if (!doit)
//       return 0;

   if (comp()->hasLargeNumberOfLoops())
      {
      return 0;
      }

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // FIXME: Why is this allocated with heap memory?
   _dominators = new (trHeapMemory()) TR_Dominators(comp());

   // Gathers symrefs that are assigned inside a loop
   //
   gatherCandidates(comp()->getFlowGraph()->getStructure(), NULL, NULL);

   perform(comp()->getFlowGraph()->getStructure()->asRegion());
   _dominators = NULL;

   return 1;
   }

void TR_InductionVariableAnalysis::gatherCandidates(TR_Structure *s, TR_BitVector *loopLocalDefs, TR_BitVector *allDefs)
   {
   if (s->asRegion())
      {
      TR_RegionStructure *region = s->asRegion();
      TR_BitVector *myAllDefs = 0;

      if (!region->isAcyclic())
         {
         loopLocalDefs = new (trStackMemory()) TR_BitVector(comp()->getSymRefTab()->getNumSymRefs(), trMemory());
         myAllDefs     = new (trStackMemory()) TR_BitVector(comp()->getSymRefTab()->getNumSymRefs(), trMemory());
         }
      else
         myAllDefs     = allDefs;


      TR_RegionStructure::Cursor it(*region);
      for (TR_StructureSubGraphNode *n = it.getFirst(); n; n = it.getNext())
         gatherCandidates(n->getStructure(), loopLocalDefs, myAllDefs);

      if (!region->isAcyclic())
         {
         region->setAnalysisInfo(new (trStackMemory())  AnalysisInfo(loopLocalDefs, myAllDefs));

         if (trace())
            {
            traceMsg(comp(), "All Defs inside cyclic region %d: ", region->getNumber());
            myAllDefs->print(comp());
            traceMsg(comp(), "\nLoopLocalDefs inside cyclic region %d: ", region->getNumber());
            loopLocalDefs->print(comp());
            traceMsg(comp(), "\n");
            }

         // now that we know myAllDefs -> push it to the outer loops allDefs
         if (allDefs)
            *allDefs |= *myAllDefs;
         }
      }
   else if (loopLocalDefs) // ie inside loop
      {
      // Walk the block and gather any integral symbols defined
      //
      TR::Block *block = s->asBlock()->getBlock();
      for (TR::TreeTop  *tt = block->getEntry();
           tt != block->getExit();
           tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (node->getOpCodeValue() == TR::treetop)
            node = node->getFirstChild();

         if (node->getOpCode().isStoreDirect() &&
             (node->getType().isIntegral() ||
              node->getSymbolReference()->getSymbol()->isInternalPointer()))
            {
            TR::SymbolReference *symRef = node->getSymbolReference();
            int32_t i = symRef->getReferenceNumber();
            allDefs      ->set(i);
            loopLocalDefs->set(i);
            }
         }
      }
   }

void TR_InductionVariableAnalysis::perform(TR_RegionStructure *region)
   {
   if (region->getEntryBlock()->isCold())
      return;

   // Process the children first
   //
   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
      {
      TR_RegionStructure *r = node->getStructure()->asRegion();
      if (r) perform(r);
      }

   if (region->isNaturalLoop())
      analyzeNaturalLoop(region);
   else
      region->setPrimaryInductionVariable(NULL);
   }

void TR_InductionVariableAnalysis::analyzeNaturalLoop(TR_RegionStructure *loop)
   {
   AnalysisInfo *ai         = (AnalysisInfo *)loop->getAnalysisInfo();
   TR_BitVector *candidates = ai->getLoopLocalDefs();

   if (candidates->isEmpty())
      {
      loop->setPrimaryInductionVariable(NULL);
      return;
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      traceMsg(comp(), "<analyzeNaturalLoop loop=%d addr=%p>\n", loop->getNumber(), loop);

   // Initialize the block Info Array
   //
   initializeBlockInfoArray(loop);

   // Empty any info collected in a previous run
   //
   loop->setPrimaryInductionVariable(NULL);
   loop->getBasicInductionVariables().init();

   // For each of the candidates, assign a local index
   //
   int32_t index = 0;
   TR_BitVectorIterator it(*candidates);
   while (it.hasMoreElements())
      {
      int32_t i = it.getNextElement();
      TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(i);
      symRef->getSymbol()->setLocalIndex(index++);
      }

   // Initialize empty info for the loop entry blocks
   //
   TR::Block *entryBlock = loop->getEntryBlock();
   setBlockInfo(entryBlock, newBlockInfo(loop));

   comp()->incVisitCount();

   // Process the loop as an acyclic region
   analyzeAcyclicRegion(loop, loop);

   // Merge all the backedges now into the loopSet
   //
   DeltaInfo **loopSet = newBlockInfo(loop);
   TR_PredecessorIterator pit(loop->getEntry());
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR_StructureSubGraphNode *node = edge->getFrom()->asStructureSubGraphNode();
      TR::Block *block = node->getStructure()->getEntryBlock();

      DeltaInfo **inSet = getBlockInfo(block);
      mergeWithSet(loopSet, inSet, loop);
      }


   analyzeLoopExpressions(loop, loopSet);

   // For each of the candidates, clear the local index to avoid polluting analyses of later loops
   //
   TR_BitVectorIterator clear_it(*candidates);
   while (clear_it.hasMoreElements())
      {
      int32_t i = clear_it.getNextElement();
      TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(i);
      symRef->getSymbol()->setLocalIndex(~0);
      }

   } // scope of the stack memory region

   if (trace())
      traceMsg(comp(), "</analyzeNaturalLoop>\n");
   }

void
TR_InductionVariableAnalysis::analyzeAcyclicRegion(TR_RegionStructure *region, TR_RegionStructure *loop)
   {
   if (loop != region)
      TR_ASSERT(region->isAcyclic(), "assertion failure");

   TR_Queue<TR_StructureSubGraphNode> q(trMemory());
   q.enqueue(region->getEntry());

   while (!q.isEmpty())
      {
      TR_StructureSubGraphNode *node = q.dequeue();

      if (node->getVisitCount() == comp()->getVisitCount()) continue;

      // Make sure all the predecessors are visited already
      //
      if (node != region->getEntry())
         {
         bool notDone = false;
         TR_PredecessorIterator pit(node);
         for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
            {
            TR::CFGNode *pred = edge->getFrom();
            if (pred->getVisitCount() != comp()->getVisitCount())
               { notDone = true; break; }
            }
         if (notDone) continue;
         }

      node->setVisitCount(comp()->getVisitCount());

      TR_Structure *structure = node->getStructure();

      if (structure->asRegion())
         {
         TR_RegionStructure *innerRegion = structure->asRegion();
         if (!innerRegion->isAcyclic())
            analyzeCyclicRegion(innerRegion, loop);
         else
            analyzeAcyclicRegion(innerRegion, loop);
         }
      else
         {
         TR_BlockStructure *blockStructure = structure->asBlock();
         analyzeBlock(blockStructure, loop);
         }

      // Queue up the successors (ignoring exit successors)
      TR_SuccessorIterator sit(node);
      for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
         {
         TR_StructureSubGraphNode *succNode = edge->getTo()->asStructureSubGraphNode();
         if (succNode->getStructure() && succNode != region->getEntry())
            q.enqueue(succNode);
         }
      }
   }

void TR_InductionVariableAnalysis::analyzeBlock(TR_BlockStructure *structure, TR_RegionStructure *loop)
   {
   TR::Block          *block     = structure->getBlock();
   DeltaInfo        **inSet     = getBlockInfo(block);
   TR_BitVector *candidates     = ((AnalysisInfo*)loop->getAnalysisInfo())->getLoopLocalDefs();

    if (trace())
       traceMsg(comp(), "analyzeBlock %d\n", block->getNumber());

   TR_ASSERT(inSet, "no info available for block_%d");


    if (trace())
       {
       traceMsg(comp(), "In Set:\n");

       TR_BitVectorIterator it(*candidates);
       while (it.hasMoreElements())
          {
          int32_t refNum = it.getNextElement();
          TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(refNum);
          int32_t refIndex = symRef->getSymbol()->getLocalIndex();
          DeltaInfo *inSymbol = inSet[refIndex];
          traceMsg(comp(), "\t%d %d %p symRef=%p symbol=%p: ",
                  refNum, refIndex, inSymbol, symRef, symRef->getSymbol());
          if (inSymbol)
             printDeltaInfo(inSymbol);
          else
             traceMsg(comp(), "null\n");
          }
       }

   // Process the contents of the block
   //
   for (TR::TreeTop *tt = block->getEntry();
        tt != block->getExit();
        tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();

      if (node->getOpCode().isStoreDirect() &&
          (node->getType().isIntegral() ||
           node->getSymbolReference()->getSymbol()->isInternalPointer()))
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         uint32_t refNum   = symRef->getReferenceNumber();
         uint32_t refIndex = symRef->getSymbol()->getLocalIndex();

         if (candidates->isSet(refNum))
            {
            if (trace())
               traceMsg(comp(), "node %p effects candidate %d (refNum: %d) symRef=%p symbol=%p\n",
                                node, refIndex, refNum, symRef, symRef->getSymbol());

            DeltaInfo *inSymbol = inSet[refIndex];

            if (!inSymbol)
               inSymbol = inSet[refIndex] = new (trStackMemory())  DeltaInfo(0);

            if (trace())
               {
               traceMsg(comp(), "\tin:  ");
               printDeltaInfo(inSymbol);
               }

            int64_t delta;
            TR_ProgressionKind kind;
            if (isProgressionalStore(node, &kind, &delta))
               {
               if (kind == Arithmetic)
                  inSymbol->arithmeticDelta((int32_t)delta);
               else if (kind == Geometric)
                  inSymbol->geometricDelta((int32_t)delta);
               }
            else
               {
               //traceMsg(comp(),"In analyze block loop %d block %d, setting inSymbol %d to unknown value \n",loop->getNumber(),block->getNumber(),symRef->getReferenceNumber());
               inSymbol->setUnknownValue();

               }

            if (trace())
               {
               traceMsg(comp(), "\tout: ");
               printDeltaInfo(inSymbol);
               }
            }
         }
      }

   // inSet is now called the outSet
   //
   DeltaInfo **outSet = inSet;
   inSet = 0;

   // Push info to each of the successors
   TR_SuccessorIterator sit(block);
   for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
      {
      TR::Block *succ = edge->getTo()->asBlock();

      if (succ != loop->getEntryBlock())
         mergeWithBlock(succ, outSet, loop);
      }

   if (trace()) traceMsg(comp(), "\n");
   }

void TR_InductionVariableAnalysis::analyzeCyclicRegion(TR_RegionStructure *region, TR_RegionStructure *loop)
   {
   TR::Block *entryBlock = region->getEntryBlock();
   DeltaInfo **inSet    = getBlockInfo(entryBlock);

   TR_BitVector *allInnerDefs  = ((AnalysisInfo*)region->getAnalysisInfo())->getAllDefs();
   TR_BitVector *loopLocalDefs = ((AnalysisInfo*)  loop->getAnalysisInfo())->getLoopLocalDefs();

   // For each of symbols s written in the cyclic region,
   //    if s is a candidate in the loop
   //    make s unknown in the inSet
   //
   TR_BitVectorIterator it(*allInnerDefs);
   while (it.hasMoreElements())
      {
      uint32_t refNum = it.getNextElement();

      if (loopLocalDefs->isSet(refNum))
         {
         TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(refNum);
         int32_t refIndex = symRef->getSymbol()->getLocalIndex();

         DeltaInfo *inSymbol = inSet[refIndex];
         if (!inSymbol)
            inSymbol = inSet[refIndex] = new (trStackMemory())  DeltaInfo(0);

         //traceMsg(comp(),"For loop %d setting symref %d to unknown value\n",loop->getNumber(),symRef->getReferenceNumber());
         inSymbol->setUnknownValue();
         }
      }

   // inSet is now called outSet
   //
   DeltaInfo **outSet = inSet;

   // Propagate to the successor blocks
   //
   ListIterator<TR::CFGEdge> eit(&region->getExitEdges());
   for (TR::CFGEdge *edge = eit.getFirst(); edge; edge = eit.getNext())
      {
      int32_t exitNumber = edge->getTo()->getNumber();
      // Find the real node corresponding to this exit num
      TR_StructureSubGraphNode *succ = region->getParent()->asRegion()->findNodeInHierarchy(exitNumber);
      TR_ASSERT(succ && succ->getStructure(), "expecting a real concrete node");
      TR::Block *block = succ->getStructure()->getEntryBlock();

      mergeWithBlock(block, outSet, loop);
      }
   }

void TR_InductionVariableAnalysis::mergeWithBlock(TR::Block *block, DeltaInfo **inSet, TR_RegionStructure *loop)
   {
   DeltaInfo **mySet = getBlockInfo(block);
   if (!mySet)
      mySet = setBlockInfo(block, newBlockInfo(loop));
   mergeWithSet(mySet, inSet, loop);
   }

void TR_InductionVariableAnalysis::mergeWithSet(DeltaInfo **mySet, DeltaInfo **inSet, TR_RegionStructure *loop)
   {
   TR_BitVector *candidates = ((AnalysisInfo*)loop->getAnalysisInfo())->getLoopLocalDefs();
   for (int32_t i = candidates->elementCount()-1; i >= 0; --i)
      {
      DeltaInfo *mySymbol = mySet[i];
      DeltaInfo *inSymbol = inSet[i];


      if (inSymbol)
         {
         if (!mySymbol)
            mySymbol = mySet[i] = new (trStackMemory())  DeltaInfo(inSymbol);
         else
            mySymbol->merge(inSymbol);
         }
      else
         {
         DeltaInfo *identity = new (trStackMemory())  DeltaInfo(0);
         if (!mySymbol)
            mySymbol = mySet[i] = identity;
         else
            mySymbol->merge(identity);
         }
      }

   }


TR_InductionVariableAnalysis::DeltaInfo **
TR_InductionVariableAnalysis::newBlockInfo(TR_RegionStructure *loop)
   {
   int32_t numCandidates = ((AnalysisInfo*)loop->getAnalysisInfo())->getLoopLocalDefs()->elementCount(); /// FIXME
   DeltaInfo **mySet = (DeltaInfo **) trMemory()->allocateStackMemory(numCandidates * sizeof(DeltaInfo *));
   memset(mySet, 0, numCandidates * sizeof(DeltaInfo*));
   return mySet;
   }

void
TR_InductionVariableAnalysis::initializeBlockInfoArray(TR_RegionStructure *loop)
   {
   int32_t numBlocks = comp()->getFlowGraph()->getNextNodeNumber();
   _blockInfo = (DeltaInfo ***)trMemory()->allocateStackMemory(numBlocks * sizeof(DeltaInfo**));
   memset(_blockInfo, 0, numBlocks * sizeof(DeltaInfo**));
   }

// Returns true if the node is an increment of a symbol reference
//
//  istore #24  <--- same sym ref
//    iadd           |
//      iload #24 <--' (could be an iadd/isub subtree)
//      iconst 304
//
// The increment is stored in the location pointed to by incr
//
bool TR_InductionVariableAnalysis::isProgressionalStore(TR::Node *node, TR_ProgressionKind *kind, int64_t *incr)
   {
   TR::Node *cursor = node->getFirstChild();
   TR::SymbolReference *storeRef = node->getSymbolReference();

   // skip over conversion nodes
   //

   //traceMsg(comp(),"In isProgressionalStore considering node %p\n",node);

   while (cursor->getOpCode().isConversion())
      cursor = cursor->getFirstChild();

   if (! (cursor->getOpCode().isAdd() || cursor->getOpCode().isSub() ||
          cursor->getOpCode().isLeftShift() || cursor->getOpCode().isRightShift()))
      {
      //traceMsg(comp(),"In isProgressionalStore cursor node %p returning false. its not add/sub/shift\n",cursor);
      return false;
      }

   TR::SymbolReference *symRef;
   if (!getProgression(cursor, storeRef, &symRef, kind, incr))
      {
      //traceMsg(comp(),"In isProgressionalStore cursor node %p returning false. getProgression returned false\n",cursor);
      return false;
      }

   if (symRef != storeRef) // must be an increment
      {
      //traceMsg(comp(),"In isProgressionalStore cursor node %p returning false. symRef = %p(%d) is not equal to storeRef %p(%d)\n",cursor,symRef,symRef->getReferenceNumber(),storeRef,storeRef->getReferenceNumber());
      return false;
      }
   // Make sure the delta is representable in 32bits
   if (((int64_t)(int32_t)(*incr)) != *incr)
      {
      //traceMsg(comp(),"In isProgressionalStore cursor node %p, delta is not representable in 32bits\n",cursor);
      return false;
      }
   return true;
   }



// Returns true if the controlling branch involves the induction variable
// If the loop exit branch does not involve the induction variable, then it
// can be possibly ignored
//
bool TR_InductionVariableAnalysis::branchContainsInductionVariable(TR_RegionStructure *loop,
                                                                   TR::Node *branchNode,
                                                                   TR_Array<TR_BasicInductionVariable*> &basicIVs)
   {
   TR_ArrayIterator<TR_BasicInductionVariable> it(&basicIVs);
   bool result = false;
   TR_BasicInductionVariable * iv;
   //traceMsg(comp(), "\tloop %d basicivs: ", loop->getNumber());
   for (iv = it.getFirst(); iv; iv = it.getNext())
      {
      int32_t index = iv->getSymRef()->getReferenceNumber();
      if(trace()) traceMsg(comp(), "\t considering branchnode [%p] and basiciv [%d]\n",branchNode,index);
      // setting a node count budget for walking the nodes
      int32_t nodeBudget = 100;
      if (branchContainsInductionVariable(branchNode, iv->getSymRef(), &nodeBudget))
         {
         if (trace())
            traceMsg(comp(), "\tbranchnode [%p] contains basiciv [%d]\n", branchNode, index);
         result = true;
         TR::Node *firstChild = branchNode->getFirstChild();
         if (firstChild->getOpCode().isConversion())
            firstChild = firstChild->getFirstChild();
         if (firstChild->getOpCode().isAdd() ||
               firstChild->getOpCode().isSub() ||
               firstChild->getOpCode().isLoadDirect())
            {
            // the expr is in some recognized form
            }
         else
            {
            result = false;
            if (trace()) traceMsg(comp(), "\tbut branch expr [%p] is not in recognized form\n", firstChild);
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "\tbranchnode [%p] does not contain basiciv [%d]\n", branchNode, index);
         }
      }
   return result;
   }

bool TR_InductionVariableAnalysis::branchContainsInductionVariable(TR::Node *node,
                                                                   TR::SymbolReference *ivSymRef,
                                                                   int32_t *nodeBudget)
   {
   // return the conservative answer when out of node count budget
   if ((*nodeBudget) <= 0)
      return false;

   (*nodeBudget)--;

   if (node->getOpCode().hasSymbolReference() &&
         (node->getSymbolReference() == ivSymRef))
      return true;
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      if (branchContainsInductionVariable(node->getChild(i), ivSymRef, nodeBudget))
         return true;

   return false;
   }

// Finds the progession taking place on an induction variable.  Right now this method recognizes arithmetic
// progressions of all kinds and Geometric progressions where the multiplicative constant (r) is a power of 2 (shift).
// If the storeRef argument has a non-null value, this function will find the progression taking place on that
// symref only.
//
// Returns true if it recognizes a progression taking place.  Returns the symbol reference of the induction
// variable in ref, the kind of progression in prog, and the progression constant in incr.
//
// For identity progression incr has no meaning.
// For arithmetic progressions, incr contains the increment value.
// For geometric progressions, incr represents the multiplicative constant. geometric progression.
// The multiplicative constant for the series (r) = 2^(incr).  Naturally this means
//   that the series is a shrinking series if incr is negative.
//
bool TR_InductionVariableAnalysis::getProgression(TR::Node *expr, TR::SymbolReference *storeRef, TR::SymbolReference **ref, TR_ProgressionKind *prog, int64_t *incr)
   {
   int64_t konst;
   TR_ProgressionKind kind;

   TR::Node *secondChild = expr->getNumChildren() > 1 ? expr->getSecondChild() : 0;

   if (expr->getOpCode().isAdd() && secondChild->getOpCode().isLoadConst() && secondChild->getType().isIntegral())
      {
      if (!getProgression(expr->getFirstChild(), storeRef, ref, &kind, &konst))
         return false;
      if (kind == Geometric) return false;
         konst += secondChild->getOpCode().isUnsigned() ? secondChild->get64bitIntegralValueAsUnsigned() : secondChild->get64bitIntegralValue();
      *prog = Arithmetic;
      }
   else if (expr->getOpCode().isSub() && secondChild->getOpCode().isLoadConst() && secondChild->getType().isIntegral())
      {
      if (!getProgression(expr->getFirstChild(), storeRef, ref, &kind, &konst))
         return false;
      if (kind == Geometric) return false;
         konst -= secondChild->getOpCode().isUnsigned() ? secondChild->get64bitIntegralValueAsUnsigned() : secondChild->get64bitIntegralValue();
      *prog = Arithmetic;
      }
   /*
     TODO : enable this to catch geometric IVs
   else if (expr->getOpCode().isLeftShift() && secondChild->getOpCode().isLoadConst())
      {
      if (!getProgression(expr->getFirstChild(), ref, &kind, &konst))
         return false;
      if (kind == Arithmetic) return false;
      konst += secondChild->getInt();
      *prog = (incr == 0) ? Identity : Geometric;
      }
   else if (expr->getOpCode().isRightShift() && secondChild->getOpCode().isLoadConst())
      {
      if (!getProgression(expr->getFirstChild(), ref, &kind, &konst))
         return false;
      if (kind == Arithmetic) return false;
      konst -= secondChild->getInt();
      *prog = (incr == 0) ? Identity : Geometric;
      }
   */
   else if (expr->getOpCode().isLoadDirect())
      {
      if (expr->getOpCode().hasSymbolReference())
         {
         if (!expr->getSymbolReference()->getSymbol()->isAutoOrParm())
            return false;
         if (storeRef && (expr->getSymbolReference() != storeRef || expr->getVisitCount() == comp()->getVisitCount()))
            return false;
         expr->setVisitCount(comp()->getVisitCount());
         *ref = expr->getSymbolReference();
         konst = 0;
         }
      else
         return false;
      }
   else if (expr->getOpCode().isConversion())
      return (getProgression(expr->getFirstChild(), storeRef, ref, prog, incr));
   else
      return false;

   if (konst == 0) *prog = Identity;
   *incr = konst;
   return true;
   }

void
TR_InductionVariableAnalysis::analyzeLoopExpressions(
   TR_RegionStructure *loop, DeltaInfo **loopSet)
   {
   TR_BitVector *candidates = ((AnalysisInfo *)loop->getAnalysisInfo())->getLoopLocalDefs();

   comp()->incVisitCount();

   TR_Array<TR_BasicInductionVariable*> *ivs =
      new (trHeapMemory()) TR_Array<TR_BasicInductionVariable*>(trMemory(), candidates->elementCount(), true, heapAlloc);
   TR_Array<TR_BasicInductionVariable*> &basicIVs = *ivs;

   TR_BitVectorIterator it(*candidates);
   while (it.hasMoreElements())
      {
      int32_t refNum = it.getNextElement();
      TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(refNum);
      int32_t refIndex = symRef->getSymbol()->getLocalIndex();

      DeltaInfo *info = loopSet[refIndex];
      //TR_ASSERT(info, "missing store/delta information for definitly written symref");

      // It is possible for info to be null if the expression was written to only on
      // a path that exits the loop.
      //
      if (!info)
         continue;
      if (info->isUnknownValue())
         {
         if (trace()) traceMsg(comp(), "----> symRef #%d[%p] is unknown\n", refNum, symRef);
         }
      else if (info->getKind() == Identity)
         {
         if (trace()) traceMsg(comp(), "----> symRef #%d[%p] is using an identity progression\n", refNum, symRef);
         }
      else if (info->getKind() == Arithmetic)
         {
         if (info->getDelta() == 0)
            {
            if (trace()) traceMsg(comp(), "----> symRef #%d[%p] is using an identity progression\n", refNum, symRef);
            }
         else
            {
            // basic linear induction variable
            if (trace())
               traceMsg(comp(), "====> Found basic linear induction variable symRef #%d[%p] with increment %d\n",
                      refNum, symRef, info->getDelta());

            TR_BasicInductionVariable *biv = new (trHeapMemory()) TR_BasicInductionVariable(comp(), loop, symRef);
            biv->setDeltaOnBackEdge(info->getDelta());
            biv->setIncrement(info->getDelta());

            basicIVs[refIndex] = biv;
            }
         }
      else if (info->getKind() == Geometric)
         {
         // basic geometric induction variable
         if (trace())
            traceMsg(comp(), "====> Found basic geometric induction variable symRef #%d[%p] with increment %d\n",
                   refNum, symRef, info->getDelta());
         // TODO - need to write this at some point
         }
      else
         TR_ASSERT(0, "illegal type for the loop expression\n");
       }

   // now we need to find the entry and exit values of the induction variables

//    traceMsg(comp(),"Before findEntryValues basicIVs->isEmpty() = %d\n",basicIVs.isEmpty());
   bool success1 = findEntryValues(loop, basicIVs);

   bool success2 = analyzeExitEdges(loop, candidates, basicIVs);

   // at this point, we have a bunch of basicIVs , one of which may have been
   // recognized as the primary iv. collect the others and hang them off structure
   // so that some other opt may utilize this info
   //
   if (!success2)
      {
      TR_ArrayIterator<TR_BasicInductionVariable> bit(&basicIVs);
      for (TR_BasicInductionVariable *biv = bit.getFirst(); biv; biv = bit.getNext())
         {
         biv->setOnlyIncrementValid(true);
         loop->addInductionVariable(biv);
         }
      }

   _ivs = ivs;
   }

// counts the number of times the node uniquely appears in a subtree
//
static uint16_t countNodeOccurrencesInSubTree(TR::Node *root, TR::Node *node, vcount_t visitCount, TR::Compilation *comp)
   {
   uint16_t count = 0;

   if (root == node)
      return 1;

   if (root->getVisitCount() == visitCount)
      return 0;

   root->setVisitCount(visitCount);

   for (int8_t c = root->getNumChildren() - 1; c >= 0; --c)
      count += countNodeOccurrencesInSubTree(root->getChild(c), node, visitCount, comp);
   return count;
   }

static bool identicalBranchTrees(TR::Node *node1, TR::Node *node2)
   {
   if (node1->getOpCodeValue() != node2->getOpCodeValue())
      return false;

   if (node1->getOpCodeValue() == TR::iconst &&
       node1->getInt() != node2->getInt())
      return false;

   if (node1->getOpCodeValue() == TR::lconst &&
       node1->getLongInt() != node2->getLongInt())
      return false;

   if (node1->getNumChildren() != node2->getNumChildren())
      return false;

   for (int32_t i = 0; i < node1->getNumChildren(); i++)
      if (!identicalBranchTrees(node1->getChild(i), node2->getChild(i)))
         return false;

   return true;
   }

void
TR_InductionVariableAnalysis::appendPredecessors(WorkQueue& dest, TR::Block *block)
   {
   const TR::CFGEdgeList& preds = block->getPredecessors();
   const TR::CFGEdgeList& excs = block->getExceptionPredecessors();
   dest.insert(dest.end(), preds.begin(), preds.end());
   dest.insert(dest.end(), excs.begin(), excs.end());
   }

// Return whether the provided induction variable has been modified in the loop
// body in a block that is not the loop controlling test
bool
TR_InductionVariableAnalysis::isIVUnchangedInLoop(TR_RegionStructure *loop,
                                                  TR::Block *loopTestBlock,
                                                  TR::Node *candidateIV)
   {
   static char *disableEnv = feGetEnv("TR_disableIVAIntermediateValueCheck");
   static bool disable = disableEnv != NULL && disableEnv[0] != '\0';
   if (disable)
      {
      if (trace())
         traceMsg(comp(), "\tintermediate value check disabled; assuming no earlier modifications\n");
      return true;
      }

   static char *verboseIVTrace = feGetEnv("TR_verboseInductionVariableTracing");

   if (trace())
      traceMsg(comp(), "\tTrying to make sure that candidate IV hasn't been modified elsewhere in the loop\n");

   WorkQueue workQueue(comp()->allocator());

   TR::BlockChecklist nodesDone(comp());
   TR::Block * loopEntryBlock = loop->getEntryBlock();
   TR_ASSERT(loopEntryBlock, "Loop cannot have a null entry block");

   // make sure that we don't process the loop test extended block
   loopTestBlock = loopTestBlock->startOfExtendedBlock();
   nodesDone.add(loopTestBlock);
   // get all the blocks that are part of the EBB
   for (TR::Block *block = loopTestBlock->getNextBlock();
         block && block->isExtensionOfPreviousBlock();
         block = block->getNextBlock())
      {
      nodesDone.add(block);
      }

   // if this is the loop header then we don't want to process it's predecessors
   if (loopEntryBlock && loopEntryBlock != loopTestBlock)
      appendPredecessors(workQueue, loopTestBlock);

   while (!workQueue.empty())
      {
      TR::Block *curBlock = workQueue.front()->getFrom()->asBlock();
      workQueue.pop_front();

      // if I've already visited this block, continue
      if (nodesDone.contains(curBlock))
         continue;
      nodesDone.add(curBlock);
      if (trace() && verboseIVTrace)
         traceMsg(comp(), "\t\tTesting Block %d\n", curBlock->getNumber());

      for (TR::TreeTop *tt = curBlock->getFirstRealTreeTop();
            tt->getNode()->getOpCodeValue() != TR::BBEnd;
            tt = tt->getNextTreeTop())
         {
         TR::Node *curNode = tt->getNode();
         if (curNode->getOpCode().isStoreDirect() &&
               curNode->getSymbolReference()->getReferenceNumber() == candidateIV->getSymbolReference()->getReferenceNumber())
            {
            if (trace())
               traceMsg(comp(), "\t\tFound store %p of symRef %p in block %d, which is not a loop test block\n",
                     curNode, candidateIV->getSymbolReference()->getSymbol(), curBlock->getNumber());
            return false;
            }
      }

      // if this is the loop header then we don't want to process it's predecessors
      if (loopEntryBlock != curBlock)
         appendPredecessors(workQueue, curBlock);
      }

   if (trace())
      traceMsg(comp(), "\tIV hasn't been modified in the loop body\n");

   return true;
   }

// Return true if we figure out a loop driving induction variable
bool
TR_InductionVariableAnalysis::analyzeExitEdges(TR_RegionStructure *loop,
                                               TR_BitVector *candidates,
                                               TR_Array<TR_BasicInductionVariable*> &basicIVs)
   {
   // Try to find the loop controlling condition.
   // If there are more than one condition - check the delta at the exit - if
   //    the delta is the same on all conditions, accept, else give up.
   // If there is a single condition - check it its on a basic induction var
   //     if the RHS is not known, exit value is symbolic
   //     if the RHS is known to be constant - exit value is that

   if (trace())
      traceMsg(comp(), "Trying to analyze the exit edges to determine if this is a counted loop\n");

   TR_Array<DeltaInfo*> exitInfos(trMemory(), candidates->elementCount(), true, stackAlloc);
   TR_Array<TR::Node*>   bounds(trMemory(), candidates->elementCount(), true, stackAlloc);

   int32_t     controllingIV = -1;
   TR::TreeTop *controllingBranch = 0;
   TR::Block   *controllingBlock  = 0;
   bool        controllingBranchToExit = false;

   ListIterator<TR::CFGEdge> eit(&loop->getExitEdges());
   int32_t legitimateBranches = 0;
   TR::CFGEdge *edge;
   for (edge = eit.getFirst(); edge; edge = eit.getNext())
      {
      TR_StructureSubGraphNode *subNode = edge->getFrom()->asStructureSubGraphNode();

      if (trace())
         traceMsg(comp(), "\tLook at node %d exiting the loop\n", subNode->getStructure()->getNumber());

      if (subNode->getStructure()->asRegion())
         {
         // OSR blocks can be ignored here as they will not be impacted by a loop controlling
         // condition.
         //
         // Exit edges do not have structure for their destination, rather they only specify
         // the block number. As all OSRInduceBlocks have an edge to the exit, even though
         // they end in a throw, it is possible to search for a block with a matching number.
         // This is potentially expensive, so it is memoized with two bit vectors, _seenInnerRegionExit
         // and _isOSRInduceBlock.
         //
         bool osrInduceExitEdge = false;
         int32_t toNum = edge->getTo()->getNumber();
         if (!_seenInnerRegionExit.get(toNum))
            {
            TR::Block *dest = NULL;
            TR::CFGNode *end = comp()->getFlowGraph()->getEnd();
            for (auto predEdge = end->getPredecessors().begin(); predEdge != end->getPredecessors().end(); ++predEdge)
               {
               if ((*predEdge)->getFrom()->getNumber() == toNum)
                  {
                  dest = (*predEdge)->getFrom()->asBlock();
                  break;
                  }
               }

            _seenInnerRegionExit.set(toNum);
            if (dest && dest->isOSRInduceBlock())
               {
               _isOSRInduceBlock.set(toNum);
               osrInduceExitEdge = true;
               }
            }
         else
            osrInduceExitEdge = _isOSRInduceBlock.get(toNum);

         if (osrInduceExitEdge)
            {
            if (trace()) traceMsg(comp(), "\tbranch from inner region is to OSR block, ignoring\n");
            continue;
            }

         if (trace()) traceMsg(comp(), "\tReject - exit edge from inner region\n");
         return false;
         }

      TR::Block *block = subNode->getStructure()->asBlock()->getBlock();
      TR::Block *skippedGotoBlock = 0;

      // factor out goto blocks
      while (isGotoBlock(block) && (block->getPredecessors().size() == 1))
         {
         TR::Block *predBlock = block->getPredecessors().front()->getFrom()->asBlock();
         if (predBlock->getStructureOf()->getContainingLoop() == block->getStructureOf()->getContainingLoop())
            {
            skippedGotoBlock = block;
            block = predBlock;
            }
         else
            break;
         }

      if (trace())
         traceMsg(comp(), "\t=>Look at branch block_%d\n", block->getNumber());

      TR::TreeTop *branchTree = block->getLastRealTreeTop();
      TR::Node    *node = branchTree->getNode();
      if(node->getOpCodeValue() == TR::treetop)
          node = node->getFirstChild();

      if ( ( (!node->getOpCode().isBooleanCompare() ||!node->getOpCode().isBranch())
               && !node->getOpCode().isJumpWithMultipleTargets())  ||
            (node->getOpCode().isJumpWithMultipleTargets() && !node->getOpCode().hasBranchChildren()) || //allow tstarts to continue
            node->getOpCode().isSwitch())
         {
         if (trace()) traceMsg(comp(), "\tReject - branch block not ending with a branch\n"); // FIXME: can improve a bit here
         return false;
         }

      if (!(node->getFirstChild()->getType().isIntegral()
            || (node->getFirstChild()->getType().isAddress()
                && node->getFirstChild()->isInternalPointer()
                && node->getSecondChild()->isInternalPointer()
                && node->getOpCodeValue() != TR::ifacmpeq
                && node->getOpCodeValue() != TR::ifacmpne))
          && !node->getOpCode().isJumpWithMultipleTargets())
         {
         if (trace()) traceMsg(comp(), "\tReject - Branch node is not integral nor ordered comparison between internal pointers\n");
         return false;
         }


      TR::Block *branchDest=0;
      bool branchToExit;

      if (!node->getOpCode().isJumpWithMultipleTargets())
         {
         branchDest = node->getBranchDestination()->getNode()->getBlock();


         if (skippedGotoBlock == 0)
            branchToExit = !loop->contains(branchDest->getStructureOf());
         else
            {
            if (branchDest == skippedGotoBlock)
               branchToExit = true;
            else
               branchToExit = !loop->contains(branchDest->getStructureOf());
            }
         }
      else if (node->getOpCode().isJumpWithMultipleTargets())
         {
         //TM: a jump with multiple target (this case, a tstart) is not a 'controlling branch to exit' in terms of the temp induction variable, so we can continue

         if (trace()) traceMsg(comp(), "\tbranch is a tstart, not a controlling branch. Ignoring\n");

         continue;
         }

//      traceMsg(comp(),"Before branchContainsInductionVariable basicIVs->isEmpty() = %d\n",basicIVs.isEmpty());

      if (!branchContainsInductionVariable(loop, node, basicIVs))
         {
         // Doesn't the possibility of taking this branch mean that we can't in
         // general know the precise exit value of any IV? Maybe PIV should
         // only provide a bound on the exit value.
         if (trace()) traceMsg(comp(), "\tBranch [%p] does not involve basic ivs, ignoring\n", node);
         continue;
         }

      // Check that before traversing any back-edge, this loop exit test must
      // run. If not, this exit test can't determine a PIV, because it's
      // possible to loop arbitrarily many times without even checking it. But
      // neither can we allow some other test to succeed in creating a PIV,
      // since we might exit via this test instead of that one, in which case
      // we would have an incorrect exit value.

      // Exception successors outside the loop are early exits, but the PIV
      // exit value shouldn't be considered to be known on exception exit.
      // However, we still need to make sure that the test block has no
      // exception successors inside the loop, since those allow execution to
      // reach a back-edge without running this exit test.
      for (auto excEdge = block->getExceptionSuccessors().begin(); excEdge != block->getExceptionSuccessors().end(); ++excEdge)
         {
         if (loop->contains((*excEdge)->getTo()->asBlock()->getStructureOf()))
            return false;
         }

      // Now it's sufficient for this exit test's block to dominate each
      // in-loop predecessor of the loop entry block.
      for (auto predEdge = loop->getEntryBlock()->getPredecessors().begin(); predEdge != loop->getEntryBlock()->getPredecessors().end(); ++predEdge)
         {
         TR::Block *predBlock = (*predEdge)->getFrom()->asBlock();
         if (loop->contains(predBlock->getStructureOf())
             && !_dominators->dominates(block, predBlock))
            {
            return false;
            }
         }

      TR::SymbolReference *symRef;
      TR_ProgressionKind kind;
      int64_t incr;
      if (!getProgression(node->getFirstChild(), 0, &symRef, &kind, &incr))
         {
         if (trace()) traceMsg(comp(), "\tReject - branch node %p in unrecognized form\n", node);
         return false;
         }

      TR_ASSERT(kind != Geometric, "geometric not implemented yet");

      int32_t index = symRef->getSymbol()->getLocalIndex();
      TR_BasicInductionVariable *biv = basicIVs[index];
      if (!biv)
         {
         if (trace()) traceMsg(comp(), "\tReject - test (node %p) on unknown symRef\n", node);
         return false;
         }

      if (controllingIV == -1)
         {
         // first time through
         controllingIV = index;
         controllingBranch = branchTree;
         controllingBlock  = block;
         controllingBranchToExit = branchToExit;
         }
      else if (controllingIV != index || controllingBranchToExit != branchToExit || !identicalBranchTrees(controllingBranch->getNode(), branchTree->getNode()))
         {
         if (trace())
            traceMsg(comp(), "\tReject - more than one loop test %p and %p\n",
                   node, controllingBranch->getNode());
         return false;
         }
      legitimateBranches++;
      }

   // Make sure we have found a loop driving induction variable
   //
   if (controllingIV == -1)
      {
      if (trace()) traceMsg(comp(), "\tReject - could not find a loop controlling test\n");
      return false;
      }

   // Now figure out the delta on the loop controlling exit edge and place the information
   // on all the induction variables
   //
   DeltaInfo **exitSet = getBlockInfo(controllingBlock);
   TR_ArrayIterator<TR_BasicInductionVariable> it(&basicIVs);
   TR_BasicInductionVariable * iv;
   for (iv = it.getFirst(); iv; iv = it.getNext())
      {
      int32_t index = iv->getSymRef()->getSymbol()->getLocalIndex();
      DeltaInfo *exitInfo = exitSet[index];

      if (!exitInfo)
         {
         // we are testing the unmodified value of the induction variable
         // at the branch block
         //
         exitInfo = exitSet[index] = new (trStackMemory())  DeltaInfo(0);
         }

      if (exitInfo->isUnknownValue())
         {
         // nothing to do // FIXME: the uninitialized value of 0 as the
         // exit increment needs fixing
         // FIXME: no longer an induction variable.
         }
      else if (exitInfo->getKind() == Identity)
         {
         iv->setDeltaOnExitEdge(0);
         }
      else if (exitInfo->getKind() == Arithmetic)
         {
         int32_t delta = exitInfo->getDelta();
         iv->setDeltaOnExitEdge(delta);
         }
      else
         TR_ASSERT(0, "code path not implemented");
      }

   TR_BasicInductionVariable *biv = basicIVs[controllingIV];

   // TODO make sure that the loop direction is not non-sensical
   TR::ILOpCode  op     = controllingBranch->getNode()->getOpCode();
   TR::ILOpCodes opCode = op.getOpCodeValue();
   if (!controllingBranchToExit)
      opCode = op.getOpCodeForReverseBranch();
   int32_t delta = biv->getDeltaOnExitEdge();
   if ((TR::ILOpCode::isLessCmp(opCode) && delta > 0) ||
       (TR::ILOpCode::isGreaterCmp(opCode) && delta < 0))
      {
      if (trace()) traceMsg(comp(), "\tReject - uninteresting direction of the loop test\n");
      return false;
      }

   // make sure that the expr on the LHS is the same as the stored
   // value of the induction variable on exit from the loop
   // ie if we had i+=3. then the controlling if could be checking the commoned
   // value i+3 with the bound.
   //
   TR::Node *valueNode = controllingBranch->getNode()->getFirstChild();
   bool valueCouldBeEvaluatedBeforeTest = valueNode->getReferenceCount() > 1;
   if (valueNode->getOpCode().isConversion())
      {
      valueNode = valueNode->getFirstChild();
      if (valueNode->getReferenceCount() > 1)
         valueCouldBeEvaluatedBeforeTest = true;
      }

   bool usesUnchangedValueInLoopTest = false;
   if (valueNode->getOpCode().isLoadVarDirect())
      {
      // If the load is unevaluated before the test tree, it sees the new value.
      static const bool forceILWalk = feGetEnv("TR_forceILWalkForLoadsInIVA") != NULL;
      if (valueCouldBeEvaluatedBeforeTest || forceILWalk)
         {
         // The load might be evaluated earlier. Now we require that it is
         // either evaluated after each store of the same variable in this
         // extended block, or before each such store.
         if (trace())
            {
            traceMsg(comp(),
               "\twalking EBB to determine when n%un %s #%d is evaluated\n",
               valueNode->getGlobalIndex(),
               valueNode->getOpCode().getName(),
               valueNode->getSymbolReference()->getReferenceNumber());
            }

         bool loadFirst = false;
         bool loadLast = false;
         bool seenStore = false;

         TR::Block * const branchBlock = controllingBranch->getEnclosingBlock();
         TR::TreeTop * const start = branchBlock->startOfExtendedBlock()->getEntry();
         TR::TreeTop * const end = controllingBranch->getNextTreeTop();
         for (TR::PostorderNodeIterator it(start, comp()); it != end; ++it)
            {
            TR::Node * const node = it.currentNode();
            if (node == valueNode)
               {
               loadFirst = !seenStore;
               loadLast = true; // so far
               if (trace())
                  {
                  traceMsg(comp(),
                     "\t\tn%un %s #%d\n",
                     node->getGlobalIndex(),
                     node->getOpCode().getName(),
                     node->getSymbolReference()->getReferenceNumber());
                  }
               continue;
               }

            // Skip everything other than stores to this IV.
            if (!node->getOpCode().isStoreDirect())
               continue;

            TR::SymbolReference * const storeSR = node->getSymbolReference();
            TR::SymbolReference * const valueSR = valueNode->getSymbolReference();
            if (storeSR->getReferenceNumber() != valueSR->getReferenceNumber())
               continue;

            // At this point node is a relevant store.
            seenStore = true;
            loadLast = false; // so far
            if (trace())
               {
               traceMsg(comp(),
                  "\t\tn%un %s #%d\n",
                  node->getGlobalIndex(),
                  node->getOpCode().getName(),
                  node->getSymbolReference()->getReferenceNumber());
               }
            }

         // With forceILWalk, we may have !valueCouldBeEvaluatedBeforeTest. If
         // so, check for loadLast, which is the justification for skipping all
         // this analysis and leaving usesUnchangedValueInLoopTest=false.
         TR_ASSERT(valueCouldBeEvaluatedBeforeTest || loadLast,
            "n%un is evaluated before a store despite refcounts indicating "
            "that it is unevaluated before test tree\n",
            valueNode->getGlobalIndex());

         // Determine the results based on loadFirst, loadLast, seenStore.
         if (!seenStore)
            {
            usesUnchangedValueInLoopTest = false;
            if (trace())
               traceMsg(comp(), "\tno stores => value is latest at test\n");
            }
         else if (loadLast)
            {
            usesUnchangedValueInLoopTest = false;
            if (trace())
               traceMsg(comp(), "\tload last => value is latest at test\n");
            }
         else if (loadFirst)
            {
            usesUnchangedValueInLoopTest = true;
            if (trace())
               traceMsg(comp(), "\tload first => value is from before test EBB\n");
            }
         else
            {
            if (trace())
               traceMsg(comp(), "\tRejected - tested value evaluated between stores\n");
            return false;
            }

         if (usesUnchangedValueInLoopTest)
            {
            if (!isIVUnchangedInLoop(loop, controllingBranch->getEnclosingBlock(), valueNode))
               {
               // The loop test may be looking at an intermediate value.
               if (trace())
                  traceMsg(comp(), "\tReject - IV node has been changed in one of the blocks that is not part of the loop test in the loop\n");
               return false;
               }
            }
         }
      }
   else
      {
      // walk backwards to find the store of the symref - and make sure
      // that the rhs of the store is valueNode.
      //
      TR::TreeTop *tt;
      for (tt = controllingBranch; ; tt = tt->getPrevTreeTop())
         {
         TR::Node *curNode = tt->getNode();
         if (curNode->getOpCodeValue() == TR::BBStart &&
             !curNode->getBlock()->isExtensionOfPreviousBlock())
            {
            if (trace()) traceMsg(comp(), "\tRejected - description needed here\n");
            return false; // FIXME: make this not this pessimistic
            }

         if (curNode->getOpCode().isStoreDirect() &&
             curNode->getSymbolReference() == biv->getSymRef())
            {
            if (curNode->getFirstChild() != valueNode)
               {
               if (trace()) traceMsg(comp(), "\tRejected - exit value %p maybe not same as tested value %p\n", curNode, valueNode);
               return false;// FIXME: make this less pessimistic
               }
            else
               break;
            }
         }
      }

   TR::Node *boundNode = controllingBranch->getNode()->getSecondChild();
   if (boundNode->getOpCode().isConversion())
      boundNode = boundNode->getFirstChild();
   loop->resetInvariance();
   if (!loop->isExprInvariant(boundNode))
      {
      if (trace()) traceMsg(comp(), "\tReject - loop bound node is not invariant\n");
      return false;
      }

   if ((boundNode->getOpCode().isLoadVarDirect()) &&
         boundNode->getSymbolReference()->getSymbol()->isAutoOrParm())
      {
      TR::Node *actualValue = findEntryValueForSymRef(loop, boundNode->getSymbolReference());
      if (actualValue)
         boundNode = actualValue;
      }

   if (trace())
      traceMsg(comp(), "Found Loop Controlling Induction Variable: %d\n", biv->getSymRef()->getReferenceNumber());

   // If the entry/exit is non-constant - try to do some pattern matching to detect
   // 1- internal pointer induction variables
   // 2- strided induction variables created by LoopStrider

   // Case1: Check for loop striding:
   if (biv->getEntryValue())
      {
      // FIXME: handle non-int values
      // TODO - move this work into the loop strider
      TR::Node *entry = biv->getEntryValue();
      TR::Node *mulNode =
         ((entry->getOpCode().isAdd() || entry->getOpCode().isSub())&& entry->getSecondChild()->getOpCode().isLoadConst()) ?
         mulNode = entry->getFirstChild() : entry;

      if (mulNode->getOpCode().isMul() &&
          mulNode->getSecondChild()->getOpCode().isLoadConst())
         {
         TR::Node *loadNode = mulNode->getFirstChild();
         if (loadNode->getOpCode().isConversion())
            loadNode = loadNode->getFirstChild();

         if (loadNode->getOpCode().isLoadVarDirect())
            {
            TR::Node *actualValue = findEntryValueForSymRef(loop, loadNode->getSymbolReference());
            if (actualValue && actualValue->getOpCode().isLoadConst())
               {
               if (entry->getType().isInt32())
                  {
                  int32_t actual = GET32BITINT(actualValue);
                  int32_t konst  = actual * mulNode->getSecondChild()->getInt();
                  if (entry->getOpCode().isAdd()) // second child must be a load const
                     konst += entry->getSecondChild()->getInt();
                  else if (entry->getOpCode().isSub())
                     konst -= entry->getSecondChild()->getInt();
                  TR::Node *node =
                     TR::Node::create(actualValue, TR::iconst, 0);
                  node->setInt(konst);
                  biv->setEntryValue(node);
                  }
               else
                  {
                  int64_t actual = GET64BITINT(actualValue);
                  int64_t konst  = actual * mulNode->getSecondChild()->getLongInt();
                  if (entry->getOpCode().isAdd())
                     konst += entry->getSecondChild()->getLongInt();
                  else if (entry->getOpCode().isSub())
                     konst -= entry->getSecondChild()->getLongInt();
                  TR::Node *node =
                     TR::Node::create(actualValue, TR::lconst, 0);
                  node->setLongInt(konst);
                  biv->setEntryValue(node);
                  }
               }
            }
         }
      }

   // Case2: Check for internal pointer variables
   if (biv->getEntryValue() &&
       biv->getSymRef()->getSymbol()->isInternalPointer())
      {
      TR::Node *entry = biv->getEntryValue();
      TR::Node *newEntryValue = 0;
      TR::Node *baseLoad = NULL;
      if (entry->getNumChildren() > 0)
         baseLoad = entry->getFirstChild();
      if (entry->getOpCode().isAdd() && baseLoad &&
          baseLoad->getOpCode().isLoad())
         {
         TR::Node *offset = entry->getSecondChild();
         if (offset->getOpCode().isAdd() || offset->getOpCode().isSub())
            {
            if (offset->getSecondChild()->getOpCode().isLoadConst())
               {
               TR::Node *addConst = offset->getSecondChild();
               TR::Node *otherNode = offset->getFirstChild();

               bool foundConversion = false;
               if (otherNode->getOpCode().isMul() &&
                   otherNode->getSecondChild()->getOpCode().isLoadConst())
                  {
                  TR::Node *loadNode = otherNode->getFirstChild();
                  if (loadNode->getOpCode().isConversion())
                     {
                     loadNode = loadNode->getFirstChild();
                     foundConversion = true;
                     }
                  TR::Node *mulConst = otherNode->getSecondChild();
                  if (loadNode->getOpCode().isLoadVarDirect())
                     {
                     TR::Node *actualValue = findEntryValueForSymRef(loop, loadNode->getSymbolReference());
                     if (actualValue && actualValue->getOpCode().isLoadConst())
                        {
                        if ((actualValue->getSize() == 8) || foundConversion)
                           {
                           int64_t konst = mulConst->getLongInt();
                           if (foundConversion)
                              konst *= (int64_t)actualValue->getInt();
                           else
                              konst *= actualValue->getLongInt();
                           //int64_t konst = actualValue->getLongInt() * mulConst->getLongInt(); // FIXME: overflow
                           if (offset->getOpCode().isAdd())
                              konst += addConst->getLongInt();
                           else if (offset->getOpCode().isSub())
                              konst -= addConst->getLongInt();
                           else TR_ASSERT(0, "should not reach here");
                           TR::Node *node =
                              TR::Node::create(actualValue, TR::lconst, 0);
                           node->setLongInt(konst);
                           newEntryValue = node;
                           }
                        else
                           {
                           int32_t konst = actualValue->getInt() * mulConst->getInt();
                           if (offset->getOpCode().isAdd())
                              konst += addConst->getInt();
                           else if (offset->getOpCode().isSub())
                              konst -= addConst->getInt();
                           else TR_ASSERT(0, "should not reach here");
                           TR::Node *node =
                              TR::Node::create(actualValue, TR::iconst, 0, konst);
                           newEntryValue = node;
                           }
                        }
                     }
                  }
               else if (otherNode->getOpCode().isLoadVarDirect())
                  {
                  TR::Node *actualValue = findEntryValueForSymRef(loop, otherNode->getSymbolReference());
                  if (actualValue && actualValue->getOpCode().isLoadConst())
                     {
                     if (actualValue->getSize() == 8)
                        {
                        int64_t konst = actualValue->getLongInt();
                        if (offset->getOpCode().isAdd())
                           konst += addConst->getLongInt();
                        else if (offset->getOpCode().isSub())
                           konst -= addConst->getLongInt();
                        else TR_ASSERT(0, "should not reach here");
                        TR::Node *node =
                           TR::Node::create(actualValue, TR::lconst, 0);
                        node->setLongInt(konst);
                        newEntryValue = node;
                        }
                     else
                        {
                        int32_t konst = actualValue->getInt();
                        if (offset->getOpCode().isAdd())
                           konst += addConst->getInt();
                        else if (offset->getOpCode().isSub())
                           konst -= addConst->getInt();
                        else TR_ASSERT(0, "should not reach here");
                        TR::Node *node =
                           TR::Node::create(actualValue, TR::iconst, 0, konst);
                        newEntryValue = node;
                        }
                     }
                  }
               }
            }
         else if ((offset->getOpCode().isMul() || offset->getOpCode().isLeftShift()) &&
                   offset->getSecondChild()->getOpCode().isLoadConst())
            {
            TR::Node *loadNode = offset->getFirstChild();
            bool foundConversion = false;
            if (loadNode->getOpCode().isConversion())
               {
               loadNode = loadNode->getFirstChild();
               foundConversion = true;
               }
            TR::Node *mulConst = offset->getSecondChild();
            if (loadNode->getOpCode().isLoadVarDirect())
               {
               TR::Node *actualValue = findEntryValueForSymRef(loop, loadNode->getSymbolReference());
               if (actualValue && actualValue->getOpCode().isLoadConst())
                  {
                  if ((actualValue->getSize() == 8) || foundConversion)
                     {
                     int64_t konst = mulConst->getLongInt();
                     if (foundConversion)
                        konst *= (int64_t)actualValue->getInt();
                     else
                        konst *= actualValue->getLongInt();
                     //int64_t konst = actualValue->getLongInt() * mulConst->getLongInt(); // FIXME: overflow
                     TR::Node *node = TR::Node::create(actualValue, TR::lconst, 0);
                     node->setLongInt(konst);
                     newEntryValue = node;
                     }
                  else
                     {
                     int32_t konst = actualValue->getInt() * mulConst->getInt();
                     TR::Node *node = TR::Node::create(actualValue, TR::iconst, 0, konst);
                     newEntryValue = node;
                     }
                  }
               }
            }
         }

      if (newEntryValue && baseLoad && boundNode->getOpCode().isAdd())
         {
         TR::Node *loadNode = boundNode->getFirstChild();
         if (loadNode->getOpCode().isLoadVarDirect() &&
             loadNode->getSymbolReference() == baseLoad->getSymbolReference())
            {
            boundNode = boundNode->getSecondChild();
            biv->setEntryValue(newEntryValue);
            }
         }
      }

   // Create the PrimaryInductionVariable
   TR_PrimaryInductionVariable *piv = new (trHeapMemory()) TR_PrimaryInductionVariable(biv, controllingBlock, boundNode->duplicateTree(), opCode, comp(), optimizer(), usesUnchangedValueInLoopTest, trace());
   piv->setNumLoopExits(legitimateBranches);
   ///piv->setUsesUnchangedValueInLoopTest(usesUnchangedValueInLoopTest);

   loop->setPrimaryInductionVariable(piv);
   basicIVs[controllingIV] = piv;

   TR_ArrayIterator<TR_BasicInductionVariable> bit(&basicIVs);
   for (iv = bit.getFirst(); iv; iv = bit.getNext())
      {
      if (iv != piv)
         {
         int32_t index = iv->getSymRef()->getSymbol()->getLocalIndex();
         TR_DerivedInductionVariable *div = new (trHeapMemory()) TR_DerivedInductionVariable(comp(), iv, piv);
         basicIVs[index] = div;

         loop->addInductionVariable(div);
         }
      }

   return true;
   }

bool
TR_InductionVariableAnalysis::isGotoBlock(TR::Block *block)
   {
   TR::TreeTop *firstTree = block->getFirstRealTreeTop();
   return firstTree == block->getLastRealTreeTop() && firstTree->getNode()->getOpCodeValue() == TR::Goto;
   }

bool
TR_InductionVariableAnalysis::findEntryValues(TR_RegionStructure *loop,
                                              TR_Array<TR_BasicInductionVariable*> &basicIVs)
   {
   TR_ArrayIterator<TR_BasicInductionVariable> it(&basicIVs);
   for (TR_BasicInductionVariable *biv = it.getFirst(); biv; biv = it.getNext())
      {
      TR::SymbolReference *symRef = biv->getSymRef();
      TR::Node *entryValue = findEntryValueForSymRef(loop, symRef);

      if (entryValue)
         {
         if (trace()) traceMsg(comp(), "\tFound entry value of BIV %d: %p\n", biv->getSymRef()->getReferenceNumber(), entryValue);
         biv->setEntryValue(entryValue);
         }
      }
   return true; // TODO
   }

TR::Node *
TR_InductionVariableAnalysis::findEntryValueForSymRef(TR_RegionStructure *loop,
                                                      TR::SymbolReference *symRef)
   {
   // flow backward until we hit a def on each path

   TR::Block *header = loop->getEntryBlock();
   TR_BitVector nodesDone(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   TR_Array<TR::Node*> cachedValues(trMemory(), comp()->getFlowGraph()->getNextNodeNumber(), true, stackAlloc);

   TR::Node *defValue = (TR::Node *)-1;
   TR_PredecessorIterator pit(header);
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR::Block *pred = edge->getFrom()->asBlock();
      if (loop->contains(pred->getStructureOf()))
         continue;

      TR::Node *thisDef = getEntryValue(pred, symRef, &nodesDone, cachedValues);

      if (!thisDef) { defValue = 0; break; }

      if (defValue == (TR::Node*)-1)
         defValue = thisDef;
      else if (!optimizer()->areNodesEquivalent(thisDef, defValue))
         {
         defValue = 0;
         break;
         }
      }

   return defValue;
   }

TR::Node *
TR_InductionVariableAnalysis::getEntryValue(TR::Block *block,
                                            TR::SymbolReference *symRef,
                                            TR_BitVector *nodesDone,
                                            TR_Array<TR::Node*> &cachedValues)
   {
   if (nodesDone->isSet(block->getNumber()))
      return cachedValues[block->getNumber()];
   nodesDone->set(block->getNumber());

   TR::TreeTop *entryTree = block->getEntry();
   if (!entryTree)
      {
      if (comp()->isLoopTransferDone())
         {
         TR::DataType dataType = symRef->getSymbol()->getDataType();

         TR::Node *dummyNode = TR::Node::create(NULL,
                                                ((dataType == TR::Int32) ? TR::iconst : TR::lconst),
                                                0);
         if (dataType == TR::Int32)
            dummyNode->setInt(0);
         else
            dummyNode->setLongInt(0);
         return dummyNode;
         }
      else
         {
         TR_ASSERT(symRef->getSymbol()->isParm(), "uninitialized local discovered.");
         }
      return 0;
      }

   for (TR::TreeTop *tt = block->getLastRealTreeTop();
        tt != entryTree; tt = tt->getPrevRealTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();

      if (node->getOpCode().isStoreDirect())
         {
         TR::SymbolReference *ref = node->getSymbolReference();
         if (ref->getReferenceNumber() == symRef->getReferenceNumber())
            {
            cachedValues[block->getNumber()] = node->getFirstChild();
            return node->getFirstChild();
            }
         }
      else if( node->mayKill(false).contains(symRef,comp()) || (node->getNumChildren() > 0 && node->getChild(0)->mayKill(false).contains(symRef,comp())))
         {
         // In non-java languages, an auto may be killed if it is passed out by reference to a call.
         // ex, say if your for loop was  for(int i = 0 ; i < num ; i++ )
         // if you find somewhere a call like  foo(&num);  then this kills the num, and we can no longer reason about its value.

         cachedValues[block->getNumber()] = 0;
         return 0;
         }
      }

   // Did not find the store in the block
   //
   TR_PredecessorIterator pit(block);
   TR::Node *defValue = (TR::Node *)-1;
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR::Block *pred = edge->getFrom()->asBlock();
      TR::Node  *thisDef = getEntryValue(pred, symRef, nodesDone, cachedValues);

      if (!thisDef) { defValue = 0; break; }

      if (defValue == (TR::Node*)-1)
         defValue = thisDef;
      else if (!optimizer()->areNodesEquivalent(defValue, thisDef))
         {
         defValue = 0;
         break;
         }
      }

   TR_ASSERT(defValue != (TR::Node*)-1, "control flow walk error");

   cachedValues[block->getNumber()] = defValue;
   return defValue;
   }

const char *
TR_InductionVariableAnalysis::optDetailString() const throw()
   {
   return "O^O INDUCTION VARIABLE ANALYSIS: ";
   }

///////////////////////////////////////////
void
TR_InductionVariableAnalysis::DeltaInfo::arithmeticDelta(int32_t delta)
   {
   if (_kind == Geometric) _unknown = true;
   else if (_kind == Identity) _kind = Arithmetic;
   if (!_unknown) _delta += delta;
   }

void
TR_InductionVariableAnalysis::DeltaInfo::geometricDelta(int32_t delta)
   {
   if (_kind == Arithmetic) _unknown = true;
   else if (_kind == Identity) _kind = Geometric;
   if (!_unknown) _delta += delta;
   }

void
TR_InductionVariableAnalysis::DeltaInfo::merge(DeltaInfo *other)
   {
   if (other->isUnknownValue() || getKind() != other->getKind() ||
       getDelta() != other->getDelta())
      {
      setUnknownValue();
      }
   }

void
TR_InductionVariableAnalysis::printDeltaInfo(DeltaInfo *info)
   {
   if (!trace()) return;

   if (info->isUnknownValue())
      traceMsg(comp(), "[unknown]\n");
   else if (info->getKind() == Identity)
      traceMsg(comp(), "[unmodified]\n");
   else if (info->getKind() == Arithmetic)
      traceMsg(comp(), "[arithmetic increment of %d]\n", info->getDelta());
   else
      traceMsg(comp(), "[geometric shift = %d]\n", info->getDelta());
   }

TR_PrimaryInductionVariable::TR_PrimaryInductionVariable(
   TR_BasicInductionVariable *biv, TR::Block *branchBlock, TR::Node *exitBound, TR::ILOpCodes exitOp,
   TR::Compilation * c, TR::Optimizer *opt, bool usesUnchangedValueInLoopTest, bool trace)
   : TR_BasicInductionVariable(c, biv), _branchBlock(branchBlock),
     _exitBound(exitBound), _exitOp(exitOp), _iterCount(-1)
   {
   _usesUnchangedValueInLoopTest = usesUnchangedValueInLoopTest;

   if (getEntryValue() &&
       getEntryValue()->getOpCode().isLoadConst() &&
       (getEntryValue()->getType().isIntegral() || getEntryValue()->getType().isAddress()) &&
       getExitBound()->getOpCode().isLoadConst() &&
       (getExitBound()->getType().isIntegral() || getExitBound()->getType().isAddress()))
      {
      int64_t exitVal = getExitBound()->getOpCode().isUnsigned() ? getExitBound()->get64bitIntegralValueAsUnsigned() : getExitBound()->get64bitIntegralValue();
      if (TR::ILOpCode::isStrictlyGreaterThanCmp(exitOp))
         exitVal++; // FIXME: overflow?
      else if (TR::ILOpCode::isStrictlyLessThanCmp(exitOp))
         exitVal--; // FIXME: overflow?

      // if this is one of those evil loops that
      // use the unchangedvalue of the induction variable,
      // adjust the exit value by one extra iteration
      //
      if (usesUnchangedValueInLoopTest)
         exitVal += getDeltaOnBackEdge();

      int64_t entryVal = getEntryValue()->getOpCode().isUnsigned() ? getEntryValue()->get64bitIntegralValueAsUnsigned() : getEntryValue()->get64bitIntegralValue();

      int32_t diff = (int32_t)exitVal - (int32_t)entryVal +
         (getDeltaOnBackEdge() - getDeltaOnExitEdge()); // FIXME: overflow

      if ((int64_t)(int32_t)diff != diff)
         _iterCount = 0;
      else
         _iterCount = (int32_t) ceiling(diff, getDeltaOnBackEdge());
      }
   else
      {
      // fprintf(stderr, "--secs-- PIV (??) in %s\n", comp()->signature());
      // maybe we still have hope simplifying?
      // say how about looking at the type signature and guessing the max value based
      // on the return type?
      // TODO: do above
      }

   if (trace || (comp()->getDebug() && comp()->getOptions()->getAnyOption(TR_TraceAll))) //always dump the info
      {
      comp()->incVisitCount();

      traceMsg(comp(), "Loop Controlling Induction Variable %d (%p):\n", getSymRef()->getReferenceNumber(), this);
      if (_iterCount != -1)
         traceMsg(comp(), "  Number Of Loop Iterations: %d\n", _iterCount);
      traceMsg(comp(), "  Branch Block is %d (%p)\n", _branchBlock->getNumber(), _branchBlock);
      traceMsg(comp(), "  EntryValue:\n");
      if (getEntryValue())
         {
         comp()->getDebug()->printWithFixedPrefix
            (comp()->getOutFile(), getEntryValue(), 8, true, false, "\t");
         traceMsg(comp(), "\n");
         }
      else
         traceMsg(comp(), "\t(nil)\n");

      traceMsg(comp(), "  ExitBound:\n");
      comp()->getDebug()->printWithFixedPrefix
         (comp()->getOutFile(), getExitBound(), 8, true, false, "\t");
      traceMsg(comp(), "\n  DeltaOnBackEdge: %d\n", getDeltaOnBackEdge());
      traceMsg(comp(), "  DeltaOnExitEdge: %d\n", getDeltaOnExitEdge());
      traceMsg(comp(), "  UsesUnchangedValueInLoopTest: %d\n", usesUnchangedValueInLoopTest);
      }

   _numLoopExits = 0;
   ///_usesUnchangedValueInLoopTest = false;
   }

TR::Node *TR_PrimaryInductionVariable::getExitValue()
   {
   // TODO: for internal pointers style variables we do not need to do this computation,
   // the loops should always be well formed in which case we can return the exitBound (or
   // some function thereof)
   int32_t iterCount = getIterationCount();
   if (iterCount > 0 && getEntryValue() && getEntryValue()->getOpCode().isLoadConst())
      {
      // TODO
      }
   return 0;
   }


TR::Node *TR_DerivedInductionVariable::getExitValue()
   {
   // TODO -- compute the exit value by getting the itercount from
   // the piv and then returning entry + itercount * deltaOnBackEdge + deltaOnExitEdge
   return 0;
   }


int32_t TR_IVTypeTransformer::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _cfg = comp()->getFlowGraph();
   if (!_cfg || !(_rootStructure = _cfg->getStructure()))
      {
      traceMsg(comp(), "IVTT: requires structure!\n");
      return 1;
      }

   if (trace())
      comp()->dumpMethodTrees("Trees before TR_IVTypeTransformation");

   TR_ScratchList<TR_Structure> whileLoops(trMemory());
   ListAppender<TR_Structure> whileLoopsInnerFirst(&whileLoops);
   TR_ScratchList<TR_Structure> doWhileLoops(trMemory());
   ListAppender<TR_Structure> doWhileLoopsInnerFirst(&doWhileLoops);
   _nodesInCycle = new (trStackMemory()) TR_BitVector(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);

   detectWhileLoops(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, _rootStructure, true);

   if (whileLoops.isEmpty() && doWhileLoops.isEmpty())
      {
      if (trace())
         traceMsg(comp(), "No while or doWhile loops detected\n");
      return 1;
      }

   if (trace())
      traceMsg(comp(), "Number of WhileLoops = %d\n", whileLoops.getSize());

   if (trace())
      traceMsg(comp(), "Number of DoWhileLoops = %d\n", doWhileLoops.getSize());

   ListIterator<TR_Structure> whileLoopsIt(&whileLoops);
   TR_Structure *nextWhileLoop;
   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(), "IVTypeTransformer, expecting natural loop");
      changeIVTypeFromAddrToInt(naturalLoop);
      }

   ListIterator<TR_Structure> doWhileLoopsIt(&doWhileLoops);
   TR_Structure *nextDoWhileLoop;
   for (nextDoWhileLoop = doWhileLoopsIt.getFirst(); nextDoWhileLoop != NULL; nextDoWhileLoop = doWhileLoopsIt.getNext())
      {
      TR_RegionStructure *naturalLoop = nextDoWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(), "IVTypeTransformer, expecting natural loop");
      changeIVTypeFromAddrToInt(naturalLoop);
      }

   return 0;
   }

const char *
TR_IVTypeTransformer::optDetailString() const throw()
   {
   return "O^O INDUCTION VARIABLE TYPE TRANSFORMER: ";
   }

static bool isIdentityExpr(TR::Node *node, TR::SymbolReference *symref)
   {
   return node->getOpCode().isLoadVarDirect() && node->getSymbolReference() == symref;
   }
void TR_IVTypeTransformer::changeIVTypeFromAddrToInt(TR_RegionStructure *natLoop)
   {
   auto cm = comp();
   if (!natLoop->isCanonicalizedLoop())
      {
      if (trace())
         traceMsg(cm, "Not a canonicalized loop. Preheader is needed for transformation\n");
      return;
      }

   List<TR::CFGEdge> &exitEdges = natLoop->getExitEdges();
   ListIterator<TR::CFGEdge> it(&exitEdges);
   auto exitEdge = it.getFirst();

   if (trace())
      traceMsg(cm, "Looking at Loop %i\n", natLoop->getNumber());

   if (!exitEdge)
      {
      if (trace())
         traceMsg(cm, "exitEdge is null\n");
      return;
      }
   if (it.getNext())
      {
      if (trace())
         traceMsg(cm, "Not handling multiple exit edges\n");  // TODO: handle them
      return;
      }

   auto exitSGN = toStructureSubGraphNode(exitEdge->getFrom());
   TR::TreeTop *backEdgeTT = NULL;
   if (auto blockStructure = exitSGN->getStructure()->asBlock())
      backEdgeTT = blockStructure->getBlock()->getExit()->getPrevTreeTop();
   if (!backEdgeTT)
      {
      if (trace())
         traceMsg(cm, "Couldn't find backedge\n");
      return;
      }
   auto backEdgeIfNode = backEdgeTT->getNode();
   if (!(backEdgeIfNode->getOpCode().isIf() &&
         backEdgeIfNode->getChild(0)->getOpCode().isRef()))
      {
      if (trace())
         traceMsg(cm, "Backedge is not a if address test node\n");
      return;
      }

   TR::SymbolReference *firstChildSymRef = findComparandSymRef(backEdgeIfNode->getChild(0));
   TR::SymbolReference *secondChildSymRef = findComparandSymRef(backEdgeIfNode->getChild(1));

   auto tt = backEdgeTT;
   TR::Node *astoreNode = NULL;
   TR::TreeTop *astoreTT = NULL;
   bool found = false;
   int32_t increment = 0;

   do {
      auto node = tt->getNode();
      cm->incVisitCount();
      if (node->getOpCodeValue() == TR::astore &&
          (node->getSymbolReference() == firstChildSymRef ||
           node->getSymbolReference() == secondChildSymRef) &&
          isInAddrIncrementForm(node, increment))
         {
         astoreNode = node;
         astoreTT = tt;
         break;
         }
      } while (!astoreNode &&
               (tt->getNode()->getOpCodeValue() != TR::BBStart ||
                (tt->getNode()->getBlock()->getPredecessors().size() == 1)) &&
               (tt = tt->getPrevTreeTop()));
   if (!astoreNode)
      {
      if (trace())
         traceMsg(cm, "Couldn't find astore near back-edge\n");
      return;
      }

   if (trace())
      {
      traceMsg(cm, "Found astore\n");
      astoreNode->printFullSubtree();
      }

   // Need to ensure that the address is only updated once in the loop body
   using namespace CS2;
   QueueOf<TR_StructureSubGraphNode*, TR::Allocator> SGNQueue(cm->allocator());
   TR::SparseBitVector visitedSGN(cm->allocator()); // SubGraphNodes
   SGNQueue.Push(natLoop->getEntry());
   while (!SGNQueue.IsEmpty())
      {
      if (trace())
         {
         traceMsg(cm, "Checking For Kills Queue: ");
         QueueOf<TR_StructureSubGraphNode*, TR::Allocator>::Cursor cursor(SGNQueue);
         for (cursor.SetToFirst(); cursor.Valid(); cursor.SetToNext())
            {
            traceMsg(cm, "%i ", cursor.Data()->getNumber());
            }
         traceMsg(cm, "\n");
         }
      auto SGNode = SGNQueue.Pop();
      if (visitedSGN[SGNode->getNumber()])
         continue;
      visitedSGN[SGNode->getNumber()] = 1;
      auto structure = SGNode->getStructure();
      if (!structure)   // null structure could mean invalidated structure... let's be pessimistic for now.
         {
         if (trace())
            traceMsg(cm, "Null structure encountered at graph node %i\n", SGNode->getNumber());
         return;
         }

      if (auto region = structure->asRegion())
         {
         SGNode = region->getEntry();   // Look through inner loops as well. TODO: Memoization
         }
      else if (auto block = structure->asBlock())
         {
         auto trblock = block->getBlock();
         auto exitTT = trblock->getEntry()->getExtendedBlockExitTreeTop();
         for (tt = trblock->getEntry(); tt && tt != exitTT; tt = tt->getNextTreeTop())
            {
            if (tt == astoreTT)
               continue;
            // If treetops are strictly ordered by side-effects, I should be ok just checking
            // the def set of the root node
            auto node = tt->getNode();
            if (node->getOpCodeValue() == TR::treetop)
               node = node->getChild(0);

            if (node->getOpCode().isLikeDef() &&
                node->getOpCode().hasSymbolReference())
               {
               auto symref = node->getSymbolReference();
               TR::SparseBitVector useDefAliases(cm->allocator());
               symref->getUseDefAliases().getAliases(useDefAliases);
               if (useDefAliases[ astoreNode->getSymbolReference()->getReferenceNumber() ])
                  {
                  if (trace())
                     traceMsg(cm, "Address symbol killed more than once in loop\n");
                  return;
                  }
               }
            }
         }
      else
         {
         TR_ASSERT(false, "invalid structure type\n");
         }

      if (SGNode != exitSGN)
         {
         TR_SuccessorIterator sit(SGNode);
         for(auto edge = sit.getFirst(); edge; edge = sit.getNext())
            {
            auto succ = toStructureSubGraphNode(edge->getTo());
            if (visitedSGN[succ->getNumber()] == 0)
               SGNQueue.Add(succ);
            }
         }
      }


   /* Currently just returns if anchor is required, as other opts don't see anchored IV as counted loop.
    * If test child has rc>1, find if anchor is required
    * simple ex. is whether tmp=ptr comes before or after ptr++
    * do {
    *   tmp = ptr  // means we have to anchor int test child before increment
    *   ptr++
    * } while(tmp != end)
    *
    * do {
    *   tmp_i = i
    *   i++
    *   ptr++
    * } while (i != (int)(ptr)-(int)(end))
    *
    * Walk forward from entry of EBB to see which is found first: referenced child or incr TT.
    * If latter, we can break anchor
    */
   auto cmpChildWithEndAddr = (astoreNode->getSymbolReference() == firstChildSymRef) ?
         backEdgeIfNode->getChild(1) : backEdgeIfNode->getChild(0);
   auto ptrTestChild = (astoreNode->getSymbolReference() == firstChildSymRef) ?
         backEdgeIfNode->getChild(0) : backEdgeIfNode->getChild(1);

   if (!isIdentityExpr(ptrTestChild, astoreNode->getSymbolReference()))
      {
      if (trace())
         traceMsg(cm, "if test containing address IV is not identity expression\n");
      return;
      }
   auto itTT = backEdgeTT->getNextTreeTop()->getNode()->getBlock()->startOfExtendedBlock()->getEntry();
   bool canBreakAnchor = true;
   cm->incVisitCount();
   while (itTT->getNode() != backEdgeIfNode)
      {
      if (itTT == astoreTT)
         {
         break;
         }
      if (itTT->getNode()->containsNode(ptrTestChild, cm->getVisitCount()))
         {
         canBreakAnchor = false;
         return; // todo: handle this. See below if (!canBreakAnchor)
         }
      itTT = itTT->getNextTreeTop();
      }

   // Transform addr accesses to base addr + int
   if (trace())
      traceMsg(cm, "all ok! Transforming addr accesses to arrayRef form of base addr + int\n");

   // Create copy symbol holding base address
   auto preheaderBlock =  natLoop->getEntryBlock()->getPrevBlock();
   if (!performTransformation(cm, "%s Initializing base and int index in preheader block_%i of loop %i\n",
              optDetailString(), preheaderBlock->getNumber(), natLoop->getNumber()))
      return;

   _addrSymRef = astoreNode->getSymbolReference();
   _baseSymRef = cm->getSymRefTab()->createTemporary(cm->getMethodSymbol(), TR::Address);
   auto baseStoreTT = TR::TreeTop::create(cm,
         TR::Node::createStore(_baseSymRef, TR::Node::createWithSymRef(TR::aload, 0, _addrSymRef)));
   _intIdxSymRef = cm->getSymRefTab()->createTemporary(cm->getMethodSymbol(),
         TR::Compiler->target.is64Bit() ? TR::Int64 : TR::Int32);
   auto intIdxStoreTT = TR::TreeTop::create(cm, TR::Node::createStore(_intIdxSymRef,
         TR::Compiler->target.is64Bit() ? TR::Node::lconst(0) : TR::Node::iconst(0)));

   // Just insert base address copy store in the preheader block. Note that canonicalizer is required.
   preheaderBlock->getEntry()->insertAfter(baseStoreTT);
   baseStoreTT->insertAfter(intIdxStoreTT);

   // Add int increment
   if (!performTransformation(cm, "%s Adding int increment tree\n", optDetailString()))
      return;
   auto intIncrementTT = TR::TreeTop::create(cm, TR::Node::createStore(_intIdxSymRef,
         TR::Node::create(TR::Compiler->target.is64Bit() ? TR::ladd : TR::iadd, 2,
               TR::Node::createLoad(_intIdxSymRef),
               TR::Compiler->target.is64Bit() ? TR::Node::lconst(increment) : TR::Node::iconst(increment))));
   astoreTT->insertAfter(intIncrementTT);

   // Modify the back-edge
   if (!performTransformation(cm, "%s Modifying the back-edge\n", optDetailString()))
      return;

   /* Transform
    * ifacmpne
    *   aload 'c'
    *   aload 'end'
    *                  into
    * iflcmpne
    *   lload int IV
    *   lsub
    *     a2l                   // Handles ptr decrementing as well
    *       aload "end"
    *     a2l
    *       aload "base"
    *
    */
   auto intNodeForBackEdgeTest = TR::Node::createLoad(_intIdxSymRef);
   if (!canBreakAnchor)
      {
      // todo: Make GLU recognize this pattern of loop as a counted loop?
      return;
      itTT->insertAfter(TR::TreeTop::create(cm, TR::Node::create(TR::treetop, 1, intNodeForBackEdgeTest)));
      }
   auto a2intOp = TR::ILOpCode::getProperConversion(TR::Address, TR::Compiler->target.is64Bit() ?
         TR::Int64 : TR::Int32, false);
   TR::Node *newIf;
   if (astoreNode->getSymbolReference() == firstChildSymRef) // Match if children to original backedgeIf children
      {
      newIf = TR::Node::createif(getIntegralIfOpCode(backEdgeIfNode->getOpCodeValue(), TR::Compiler->target.is64Bit()),
         intNodeForBackEdgeTest,
         TR::Node::create(TR::Compiler->target.is64Bit() ? TR::lsub : TR::isub, 2,
            TR::Node::create(a2intOp, 1, cmpChildWithEndAddr),
            TR::Node::create(a2intOp, 1,
               TR::Node::createLoad(_baseSymRef))),
         backEdgeIfNode->getBranchDestination());
      }
   else
      {
      newIf = TR::Node::createif(getIntegralIfOpCode(backEdgeIfNode->getOpCodeValue(), TR::Compiler->target.is64Bit()),
         TR::Node::create(TR::Compiler->target.is64Bit() ? TR::lsub : TR::isub, 2,
            TR::Node::create(a2intOp, 1, cmpChildWithEndAddr),
            TR::Node::create(a2intOp, 1,
               TR::Node::createLoad(_baseSymRef))),
         intNodeForBackEdgeTest,
         backEdgeIfNode->getBranchDestination());
      }
   auto newIfTT = TR::TreeTop::create(cm, newIf);
   backEdgeTT->insertAfter(newIfTT);
   backEdgeTT->unlink(true);


   //return;

#if 1   // todo: is transforming loop body aload's desirable? Shouldn't be, it's undoing ptr striding
   // Transform aload access in loop body
   if (!performTransformation(cm, "%s Transforming aload's in loop body to base+int form\n", optDetailString()))
      return;
   visitedSGN.Clear();
   SGNQueue.Push(natLoop->getEntry());
   while(!SGNQueue.IsEmpty())
      {
      if (trace())
         {
         traceMsg(cm, "Replacing Queue: ");
         QueueOf<TR_StructureSubGraphNode*, TR::Allocator>::Cursor cursor(SGNQueue);
         for (cursor.SetToFirst(); cursor.Valid(); cursor.SetToNext())
            {
            traceMsg(cm, "%i ", cursor.Data()->getNumber());
            }
         traceMsg(cm, "\n");
         }
      auto SGNode = SGNQueue.Pop();
      if (visitedSGN[SGNode->getNumber()])
         continue;
      visitedSGN[SGNode->getNumber()] = 1;
      auto structure = SGNode->getStructure();
      TR_ASSERT(structure != NULL, "IVTT: Structure is NULL\n");
      if (auto region = structure->asRegion())
         {
         SGNode = region->getEntry();   // Look through inner loops as well.
         }
      else if (auto block = structure->asBlock())
         {
         auto trblock = block->getBlock();
         auto exitTT = trblock->getEntry()->getExtendedBlockExitTreeTop();
         cm->incVisitCount();
         for (tt = trblock->getEntry(); tt && tt != exitTT; tt = tt->getNextTreeTop())
            {
            replaceAloadWithBaseIndexInSubtree(tt->getNode());
            }
         }
      else
         {
         TR_ASSERT(false, "invalid structure type\n");
         }

      if (SGNode != exitSGN)
         {
         TR_SuccessorIterator sit(SGNode);
         for(auto edge = sit.getFirst(); edge; edge = sit.getNext())
            {
            auto succ = toStructureSubGraphNode(edge->getTo());
            if (visitedSGN[succ->getNumber()] == 0)
               SGNQueue.Add(succ);
            }
         }
      }
#endif
   }

TR::ILOpCodes TR_IVTypeTransformer::getIntegralIfOpCode(TR::ILOpCodes ifacmp, bool is64bit)
   {
   auto op = ifacmp;
   switch (ifacmp)
      {
      case TR::ifacmpeq: op = TR::iflcmpeq; break;
      case TR::ifacmpge: op = TR::iflcmpge; break;
      case TR::ifacmpgt: op = TR::iflcmpgt; break;
      case TR::ifacmple: op = TR::iflcmple; break;
      case TR::ifacmplt: op = TR::iflcmplt; break;
      case TR::ifacmpne: op = TR::iflcmpne; break;
      default: break;
      }
   if (is64bit)
      return op;
   switch (op)
      {
      case TR::iflcmpeq: op = TR::ificmpeq; break;
      case TR::iflcmpge: op = TR::ificmpge; break;
      case TR::iflcmpgt: op = TR::ificmpgt; break;
      case TR::iflcmple: op = TR::ificmple; break;
      case TR::iflcmplt: op = TR::ificmplt; break;
      case TR::iflcmpne: op = TR::ificmpne; break;
      default: break;
      }
   return op;
   }

void TR_IVTypeTransformer::replaceAloadWithBaseIndexInSubtree(TR::Node *node)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return;
   node->setVisitCount(comp()->getVisitCount());

   auto child = node->getNumChildren() >= 1 ? node->getChild(0) : NULL;
   if (child &&
       child->getOpCodeValue() == TR::aload &&
       child->getSymbolReference() == _addrSymRef &&
       performTransformation(comp(), "%s Replacing n%in aload with base int-index form\n",
             optDetailString(), child->getGlobalIndex()))
      {
      auto arrayRef = TR::Node::recreateWithoutProperties(child, TR::Compiler->target.is64Bit() ? TR::aladd : TR::aiadd, 2,
            TR::Node::createLoad(_baseSymRef),
            TR::Node::createLoad(_intIdxSymRef));
      }
   for (int i=0; i<node->getNumChildren(); i++)
      {
      replaceAloadWithBaseIndexInSubtree(node->getChild(i));
      }
   }

// Sets increment
bool TR_IVTypeTransformer::isInAddrIncrementForm(TR::Node *node, int32_t &increment)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return false;
   node->setVisitCount(comp()->getVisitCount());
   TR::Node *load = NULL;
   TR::Node *incChild = NULL;
   if (node->getOpCodeValue() == TR::astore &&
       node->getChild(0)->getOpCode().isArrayRef() &&
       (load = node->getChild(0)->getChild(0)) &&
       load->getOpCode().isLoadVar() &&
       load->getOpCode().hasSymbolReference() &&
       load->getSymbolReference() == node->getSymbolReference() &&
       (incChild = node->getChild(0)->getChild(1)) &&
       incChild->getOpCode().isLoadConst())     // TODO: non-const, loop-invariant expr
      {
      increment = incChild->getConst<int32_t>();
      return true;
      }
   if (trace())
      traceMsg(comp(), "Not in address increment form\n");
   return false;
   }

TR::SymbolReference *TR_IVTypeTransformer::findComparandSymRef(TR::Node *node)
   {
   if (node->getOpCode().hasSymbolReference())
      return node->getSymbolReference();

   if (node->getOpCode().isArrayRef() &&
       node->getChild(0)->getOpCode().hasSymbolReference())
      return node->getChild(0)->getSymbolReference();

   return NULL;
   }
