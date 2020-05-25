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

#include "optimizer/FieldPrivatizer.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/Dominators.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/IsolatedStoreElimination.hpp"
#include "optimizer/LoopCanonicalizer.hpp"


TR_FieldPrivatizer::TR_FieldPrivatizer(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager),
     _privatizedFieldSymRefs(manager->trMemory(), stackAlloc),
     _privatizedRegCandidates(manager->trMemory()),
     _appendCalls(manager->trMemory()),
     _privatizedFieldNodes(manager->trMemory())
   {
   }


int32_t TR_FieldPrivatizer::perform()
   {

   // Need to confirm places calling opCodeFor* APIs are doing the right
   // thing for readbar and wrtbar. All the transformations applied to normal
   // load/store s should be applied to rd/wrtbar s. However, we need to be
   // careful about the difference in the shape of the trees and the
   // mapping relationship between different loads and stores
   if (comp()->incompleteOptimizerSupportForReadWriteBarriers())
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _postDominators = NULL;

   detectCanonicalizedPredictableLoops(comp()->getFlowGraph()->getStructure(), 0, -1);

   TR::TreeTop *node;
   ListIterator<TR::TreeTop> si(&_appendCalls);
   for (node = si.getCurrent(); node != 0; node = si.getNext())
      {
#ifdef J9_PROJECT_SPECIFIC
      TR_InlineCall newInlineCall(optimizer(), this);
      newInlineCall.inlineCall(node);
      requestOpt(OMR::expensiveObjectAllocationGroup);
#endif
      }

   return 1;
   }


bool TR_FieldPrivatizer::isFieldAliasAccessed(TR::SymbolReference * symRef)
   {
   bool aliasAccessed = false;

   if (symRef->getUseDefAliases().hasAliases())
      {

      TR_ASSERT(_allSymRefs[symRef->getReferenceNumber()] == true, "symRef should have been seen in the loop");
      _allSymRefs[symRef->getReferenceNumber()] = false;

      if (symRef->getUseDefAliases().containsAny(_allSymRefs, comp()))
         {
         aliasAccessed = true;
         }
      _allSymRefs[symRef->getReferenceNumber()] = true;

      }

   return aliasAccessed;
   }


int32_t TR_FieldPrivatizer::detectCanonicalizedPredictableLoops(TR_Structure *loopStructure, TR_BitVector **optSetInfo, int32_t bitVectorSize)
   {
   TR_RegionStructure *regionStructure = loopStructure->asRegion();

   if (regionStructure)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != 0; node = si.getNext())
         detectCanonicalizedPredictableLoops(node->getStructure(), optSetInfo, bitVectorSize);
      }

   TR_BlockStructure *loopInvariantBlock = 0;

   if (!regionStructure ||
       !regionStructure->getParent() ||
       !regionStructure->isNaturalLoop())
      return 0;

 //  traceMsg(comp(), "Considering Loop %d\n", regionStructure->getNumber());

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   regionStructure->getBlocks(&blocksInRegion);

   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;

   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      if (nextBlock->hasExceptionPredecessors())
         return 0;

   TR::Block * loopEntryBlock = regionStructure->getEntryBlock();
   TR::CFGEdge * predecessor;
   for(auto predecessor = loopEntryBlock->getPredecessors().begin(); predecessor != loopEntryBlock->getPredecessors().end(); ++predecessor)
      {
        TR::Block * predBlock = toBlock((*predecessor)->getFrom());
        if(predBlock == comp()->getFlowGraph()->getStart()->asBlock())
            return 0;
      }

   TR_RegionStructure *parentStructure = regionStructure->getParent()->asRegion();
   TR_StructureSubGraphNode *subNode = 0;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != 0; subNode = si.getNext())
      {
      if (subNode->getNumber() == loopStructure->getNumber())
         break;
      }

   if ((subNode->getPredecessors().size() == 1))
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
         loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
      }

   _currLoopStructure = loopStructure;
   bool canTransform = true;

   if (!loopInvariantBlock &&
       (regionStructure->getEntryBlock() != comp()->getStartBlock()))
      {
      TR::Block *entryBlock = regionStructure->getEntryBlock();
      for (auto pred = entryBlock->getPredecessors().begin(); pred != entryBlock->getPredecessors().end(); ++pred)
         {
         TR::Block *predBlock = toBlock((*pred)->getFrom());
         if (blocksInRegion.find(predBlock))
            continue;

         TR::Node *node = predBlock->getLastRealTreeTop()->getNode();

         if(node->getOpCodeValue() == TR::treetop)
              node = node->getFirstChild();

         if (node->getOpCode().isJumpWithMultipleTargets() && !node->getOpCode().hasBranchChildren())
            {
            canTransform = false;
            break;
            }
         }
      }


   if (canTransform &&
       !loopInvariantBlock &&
       (regionStructure->getEntryBlock() != comp()->getStartBlock()))
      {
      TR::Block *entryBlock = regionStructure->getEntryBlock();
      int32_t sumPredFreq = 0;
      for (auto pred = entryBlock->getPredecessors().begin(); pred != entryBlock->getPredecessors().end(); ++pred)
         {
         TR::Block *predBlock = toBlock((*pred)->getFrom());
         if (!blocksInRegion.find(predBlock))
            sumPredFreq = sumPredFreq + (*pred)->getFrequency();
         }

      TR::Block *newBlock = TR::Block::createEmptyBlock(entryBlock->getEntry()->getNode(), comp(), sumPredFreq < 0 ? 0 : sumPredFreq, entryBlock);
      bool insertAsFallThrough = false;
      TR::CFG * cfg = comp()->getFlowGraph();
      cfg->addNode(newBlock, parentStructure);
      TR::CFGEdge *entryEdge = cfg->addEdge(newBlock, entryBlock);
      entryEdge->setFrequency(sumPredFreq);
      TR::TreeTop *lastTreeTop = comp()->getMethodSymbol()->getLastTreeTop();

      for (auto pred = entryBlock->getPredecessors().begin(); pred != entryBlock->getPredecessors().end();)
         {
         TR::Block *predBlock = toBlock((*pred)->getFrom());
         if ((predBlock != newBlock) &&
             !blocksInRegion.find(predBlock))
            {
            predBlock->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), entryBlock->getEntry(), newBlock->getEntry());
            if (predBlock->getNextBlock() == entryBlock)
              {
              insertAsFallThrough = true;
              }

            TR::CFGEdge *newEdge = cfg->addEdge(predBlock, newBlock);
            newEdge->setFrequency((*(pred++))->getFrequency());
            cfg->removeEdge(predBlock, entryBlock);
            }
         else
            ++pred;
         }

      TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(entryBlock->getEntry()->getNode(), TR::Goto, 0, entryBlock->getEntry()));
      newBlock->append(gotoTreeTop);

      TR::TreeTop *entryTree = entryBlock->getEntry();
      if (insertAsFallThrough)
         {
         TR::TreeTop *prevTree = entryTree->getPrevTreeTop();
         prevTree->join(newBlock->getEntry());
         newBlock->getExit()->join(entryTree);
         }
      else
         {
         lastTreeTop->join(newBlock->getEntry());
         lastTreeTop = newBlock->getExit();
         }

      loopInvariantBlock = newBlock->getStructureOf();
      }

   if (loopInvariantBlock &&
       ((loopInvariantBlock->getBlock()->getPredecessors().size() == 1) ||
        loopInvariantBlock->getBlock()->getFirstRealTreeTop() == loopInvariantBlock->getBlock()->getLastRealTreeTop()))
       {
       int32_t symRefCount = comp()->getSymRefCount();
       initializeSymbolsWrittenAndReadExactlyOnce(symRefCount, notGrowable);
       _privatizedFields = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
       _fieldsThatCannotBePrivatized = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);

       _needToStoreBack = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growable);
       _criticalEdgeBlock = 0;

       if (trace())
          traceMsg(comp(), "\nChecking loop %d for predictability\n", loopStructure->getNumber());

       _isAddition = false;
       int32_t isPredictableLoop = checkLoopForPredictability(loopStructure, loopInvariantBlock->getBlock(), 0);
       if (isPredictableLoop <= 0)
          {
          vcount_t visitCount = comp()->incVisitCount();
          initializeSymbolsWrittenAndReadExactlyOnce(symRefCount, notGrowable);
          collectSymbolsWrittenAndReadExactlyOnce(loopStructure, visitCount);
          }

       bool loopContainsStringPeephole = false;
       _stringPeepholeTree = 0;
       _tempStringSymRef = 0;
       _stringSymRef = 0;
       _valueOfSymRef = 0;

       bool loopContainsEscapePoints = containsEscapePoints(loopStructure, loopContainsStringPeephole);
       if (_stringPeepholeTree &&
           !_stringSymRef->getUseonlyAliases().hasAliases() &&
           !_valueOfSymRef->getUseonlyAliases().hasAliases())
          {
          _toStringSymRef = 0;
          _initSymRef = 0;
          _appendSymRef = 0;
          _stringBufferClass = fe()->getClassFromSignature("java/lang/StringBuffer", 22, comp()->getCurrentMethod());

          if (_stringBufferClass)
             {
             TR::Block *invariantBlock = loopInvariantBlock->getBlock();
             addStringInitialization(invariantBlock);
             TR_ScratchList<TR::Block> exitBlocks(trMemory());
             loopStructure->collectExitBlocks(&exitBlocks);
             placeStringEpilogueInExits(&exitBlocks, &blocksInRegion);
             cleanupStringPeephole();
             }
          }

       if (isPredictableLoop > 0)
          {
          if (!loopContainsEscapePoints)
             {
             _privatizedFieldNodes.deleteAll();
             _privatizedFields->empty();
             _fieldsThatCannotBePrivatized->empty();

             vcount_t visitCount = comp()->incVisitCount();
             detectFieldsThatCannotBePrivatized(loopStructure, visitCount);

             _privatizedFields->empty();
             _privatizedFieldNodes.deleteAll();

             if (trace())
                {
                traceMsg(comp(), "\nDetected a predictable loop %d\n", loopStructure->getNumber());

                traceMsg(comp(), "Fields that cannot be privatized:\n");
                _fieldsThatCannotBePrivatized->print(comp());
                traceMsg(comp(), "\n");

                traceMsg(comp(), "\nPossible new induction variable candidates :\n");
                }

            visitCount = comp()->incVisitCount();
            privatizeNonEscapingLoop(loopStructure, regionStructure->getEntryBlock(), visitCount);

            TR_ScratchList<TR::Block> exitBlocks(trMemory());
            if (!_privatizedFieldNodes.isEmpty())
               {
               loopStructure->collectExitBlocks(&exitBlocks);
               TR::Block *invariantBlock = loopInvariantBlock->getBlock();
               //placeInitializersInLoopInvariantBlock(invariantBlock);
               TR::Block *pred = toBlock(invariantBlock->getPredecessors().front()->getFrom());
               TR::Block *next;
               for (auto edge = pred->getSuccessors().begin(); edge != pred->getSuccessors().end(); ++edge)
                  {
                  next = toBlock((*edge)->getTo());
                  if (next != invariantBlock)
		     {
                     bool joinHasPredInLoop = false;
                     if (next->getSuccessors().size() == 1)
		        {
			TR::Block *join = next->getSuccessors().front()->getTo()->asBlock();
                        for (auto predEdge = join->getPredecessors().begin(); predEdge != join->getPredecessors().end(); ++predEdge)
                           {
			   if (blocksInRegion.find((*predEdge)->getFrom()->asBlock()))
			      {
			      joinHasPredInLoop = true;
                              break;
			      }
			   }
			}

		     // The assumption is that _criticalEdge block should be
		     // a block inserted by loop canonicalizer to account for the
		     // control flow path where the test done outside the loop fails
		     // and the canonicalized loop is never entered. If it is not,
		     // then we don't have the loop in the exact form we would like
		     // and so we will insert store backs inside the loop (because
		     // _criticalEdgeBlock will be 0 as the 'if' below will not be
		     // entered)
		     //
                     if (joinHasPredInLoop)
		        {
			_criticalEdgeBlock = next;
                        placeInitializersInLoopInvariantBlock(next);
                        }
		     }
                  else
                     placeInitializersInLoopInvariantBlock(next);
                  }

               addPrivatizedRegisterCandidates(loopStructure);
               placeStoresBackInExits(&exitBlocks, &blocksInRegion);
               }
             }
          }
       }
   return 0;
   }






bool TR_FieldPrivatizer::containsEscapePoints(TR_Structure *structure, bool &containsStringPeephole)
   {
   bool result = false;
   if (structure->asBlock() != 0)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();

         if (currentNode->exceptionsRaised())
            result = true;

         // DISABLED for now, although an excellent general purpose opt,
         // this causes a regression, because the loop runs <= 1 times
         // Enable this only when it is known (via profiling that the loop goes > 1
         // times)
         //
         //    if (isStringPeephole(currentNode, currentTree))
         //       containsStringPeephole = true;

         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != 0; node = si.getNext())
         {
         if (containsEscapePoints(node->getStructure(), containsStringPeephole))
            result = true;
         }
      }

   return result;
   }






bool TR_FieldPrivatizer::isStringPeephole(TR::Node *currentNode, TR::TreeTop *currentTree)
   {
   bool foundStringPeephole = false;

   if (currentNode->getOpCode().isTreeTop() &&
       (currentNode->getNumChildren() > 0))
      {
      currentNode = currentNode->getFirstChild();
      if (currentNode->getOpCodeValue() == TR::call &&
          !currentNode->getSymbolReference()->isUnresolved())
         {
         TR_ResolvedMethod *method = currentNode->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
         if (method->isConstructor())
            {
            char *sig = method->signatureChars();
            if (!strncmp(sig, "(Ljava/lang/String;C)", 21))
               {
               TR::Node *valueOfString = currentNode->getSecondChild();
               if (valueOfString->getOpCode().hasSymbolReference() &&
                   currentNode->getFirstChild()->getOpCodeValue() == TR::New)
                  {
                  TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
                  TR::Node *prevNode = prevTree->getNode();
                  if (prevNode->getOpCode().isStore())
                     {
                     _stringSymRef = prevNode->getSymbolReference();
                     _valueOfSymRef = valueOfString->getSymbolReference();
                     prevTree = prevTree->getPrevTreeTop();
                     while (prevTree->getNode()->getOpCodeValue() != TR::BBStart)
                        {
                        prevNode = prevTree->getNode();
                        if (prevNode->getOpCode().isStore() &&
                           prevNode->getSymbolReference() == _valueOfSymRef &&
                           prevNode->getFirstChild()->getOpCode().isLoadVarDirect() &&
                           prevNode->getFirstChild()->getSymbolReference() == _stringSymRef)
                           {
                           foundStringPeephole = true;
                           break;
                           }
                        prevTree = prevTree->getPrevTreeTop();
                        }

                   if (foundStringPeephole)
                      {
                      if (!_writtenExactlyOnce.ValueAt(_stringSymRef->getReferenceNumber()) ||
                          !_readExactlyOnce.ValueAt(_stringSymRef->getReferenceNumber()) ||
                          !_writtenExactlyOnce.ValueAt(_valueOfSymRef->getReferenceNumber()) ||
                          !_readExactlyOnce.ValueAt(_valueOfSymRef->getReferenceNumber()))
                         foundStringPeephole = false;
                      }

                  if (foundStringPeephole)
                     {
                     _stringPeepholeTree = currentTree;
                     }
                   }
                }
             }
           }
        }
     }


   return foundStringPeephole;
   }



void TR_FieldPrivatizer::detectFieldsThatCannotBePrivatized(TR_Structure *structure, vcount_t visitCount)
   {
   if (structure->asBlock() != 0)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         detectFieldsThatCannotBePrivatized(currentNode, visitCount);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != 0; node = si.getNext())
         detectFieldsThatCannotBePrivatized(node->getStructure(), visitCount);
      }
   }





void TR_FieldPrivatizer::detectFieldsThatCannotBePrivatized(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;
   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();

   // Check if field cannot be privatized
   if (opCode.isStore() || opCode.isLoadVar())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *sym = symRef->getSymbol();
      bool staticSym = sym->isStatic();
      size_t symSize = sym->getSize();

      if ((opCode.isIndirect() || staticSym))
         {
    	 bool canPrivatize = true;
    	 if (!TR_LocalAnalysis::isSupportedNodeForFieldPrivatization(node, comp(), NULL))
            canPrivatize = false;

         if (canPrivatize &&
             !sym->isArrayShadowSymbol() &&
             !sym->isVolatile() &&
             comp()->cg()->considerTypeForGRA(symRef) &&
             !_neverWritten->get(symRef->getReferenceNumber()) &&
             (opCode.isIndirect() ? subtreeIsInvariantInLoop(node->getFirstChild()) : true)  &&
             !isFieldAliasAccessed(symRef) &&
             (symSize <= 8 || sym->getType().isVector()))
            {
            bool canPrivatizeBasedOnThisNode = canPrivatizeFieldSymRef(node);

            if (!canPrivatizeBasedOnThisNode)
               {
               if (!_privatizedFields->get(symRef->getReferenceNumber()))
                  {
                  _privatizedFields->set(symRef->getReferenceNumber());
                  _privatizedFieldNodes.add(node->duplicateTree());
                  }
               else
                  {
            	  _fieldsThatCannotBePrivatized->set(symRef->getReferenceNumber());
                  }
               }
            }
         else
            {
            _fieldsThatCannotBePrivatized->set(symRef->getReferenceNumber());
            }
         }
      else
         {
         _fieldsThatCannotBePrivatized->set(symRef->getReferenceNumber());
         }
      }

   int32_t i;
   for (i=0;i<node->getNumChildren();i++)
      detectFieldsThatCannotBePrivatized(node->getChild(i), visitCount);
   }

void TR_FieldPrivatizer::privatizeNonEscapingLoop(TR_Structure *structure, TR::Block *entryBlock, vcount_t visitCount)
   {
   if (structure->asBlock() != 0)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();
      bool postDominatesEntry = _postDominators && _postDominators->dominates(block, entryBlock);

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         privatizeFields(currentNode, postDominatesEntry, visitCount);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != 0; node = si.getNext())
         privatizeNonEscapingLoop(node->getStructure(), entryBlock, visitCount);
      }
   }









void TR_FieldPrivatizer::privatizeFields(TR::Node *node, bool postDominatesEntry, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;
   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();
   TR::DataType nodeType = node->getType();
   TR::DataType nodeDataType = node->getDataType();

   // privatize
   if (opCode.isStore() || opCode.isLoadVar())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *sym = symRef->getSymbol();
      size_t symSize = sym->getSize();

      // Don't add legal here, add it to detectFieldsThatCannotBePrivatized
      if (!_fieldsThatCannotBePrivatized->get(symRef->getReferenceNumber()))
         {
         TR::SymbolReference *autoForField = 0;
         //
         // See if it can be privatized
         //
         //if (_privatizedFields->get(symRef->getReferenceNumber()))
            autoForField = getPrivatizedFieldAutoSymRef(node);

         bool canPrivatizeBasedOnThisNode = performTransformation(comp(), "%s Field access %p using sym ref %d privatized ", optDetailString(), node, symRef->getReferenceNumber());
         if (canPrivatizeBasedOnThisNode)
            {
            if (!autoForField)
               {
               // Create a new auto for this field
               //
               TR_HashId index = 0;
               _privatizedFields->set(symRef->getReferenceNumber());

               autoForField = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), nodeDataType,false,symSize);
               _privatizedFieldSymRefs.add(symRef->getReferenceNumber(),index,autoForField);
               _privatizedRegCandidates.add(comp()->getGlobalRegisterCandidates()->findOrCreate(autoForField));
               _privatizedFieldNodes.add(node->duplicateTree());
               }
            else if (!_privatizedFields->get(symRef->getReferenceNumber()))
               {
               _privatizedFields->set(symRef->getReferenceNumber());
               //Even we are reusing temp, need to save this for initialization and store back
               _privatizedFieldNodes.add(node->duplicateTree());
               }

            dumpOptDetails(comp(), "using auto %d\n", autoForField->getReferenceNumber());

            node->setSymbolReference(autoForField);
            TR::Node *newFirstChild = 0;
            int32_t newFirstChildNum = -1;
            if (opCode.isIndirect())
               {
               if (opCode.isStore())
                  {
                  _needToStoreBack->set(autoForField->getReferenceNumber());
                  TR::Node::recreate(node, comp()->il.opCodeForDirectStore(nodeDataType));
                  newFirstChild = node->getSecondChild();
                  newFirstChildNum = 1;
                  }
               else
                  {
                  TR::Node::recreate(node, comp()->il.opCodeForDirectLoad(nodeDataType));
                  }

               for (int32_t i = 0; i < node->getNumChildren(); i++)
                  {
                  if (i != newFirstChildNum)
                     {
                     node->getChild(i)->recursivelyDecReferenceCount();
                     }
                  }

               if (newFirstChild)
                  {
                  node->setChild(0, newFirstChild);
                  node->setNumChildren(1);
                  }
               else
                  {
                  node->setNumChildren(0);
                  }
               }
            else
               {
               if (opCode.isStore())
                  {
                  _needToStoreBack->set(autoForField->getReferenceNumber());
                  if (node->getOpCodeValue() == TR::awrtbar)
                     {
                     node->getChild(1)->recursivelyDecReferenceCount();
                     node->setNumChildren(1);
                     TR::Node::recreate(node, comp()->il.opCodeForDirectStore(nodeDataType));
                     }
                  }
               }
            }
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      privatizeFields(node->getChild(i), postDominatesEntry, visitCount);
      }
   }



bool TR_FieldPrivatizer::canPrivatizeFieldSymRef(TR::Node *node)
   {
   ListElement<TR::Node> *currentNodeElem = _privatizedFieldNodes.getListHead();
   while (currentNodeElem)
      {
      TR::Node *currentNode = currentNodeElem->getData();
      TR::SymbolReference *symRefInCurrentNode = currentNode->getSymbolReference();
      TR::SymbolReference *symRef = node->getSymbolReference();

      if (symRefInCurrentNode->getReferenceNumber() == symRef->getReferenceNumber())
         {
         // Found a node that looks likely to have been privatized
         //
         if (!node->getOpCode().isIndirect())
            return true;

         if (bothSubtreesMatch(currentNode->getFirstChild(), node->getFirstChild()))
            return true;
         else
	    return false;
         }

      currentNodeElem = currentNodeElem->getNextElement();
      }

   // This return false here has 2 meanings:
   //    1. If the symref is in _privatizedFields then we add this node to the list of those that can't be privatized.
   //    2. If the symref is NOT in _privatizedFields we add it to privatized fields, so essentially it's a true.

   return false;
   }



TR::SymbolReference *TR_FieldPrivatizer::getPrivatizedFieldAutoSymRef(TR::Node *node)
   {
   TR::SymbolReference              *symRef = node->getSymbolReference();
   TR_HashId                        index  = 0;
   if (_privatizedFieldSymRefs.locate(symRef->getReferenceNumber(),index))
      {
      return ((TR::SymbolReference *)_privatizedFieldSymRefs.getData(index));
      }
   return 0;
   }




bool TR_FieldPrivatizer::bothSubtreesMatch(TR::Node *node1, TR::Node *node2)
   {
   if (node1 == node2)
      return true;

   if (node1->getOpCodeValue() == node2->getOpCodeValue())
      {
      if (node1->getOpCode().isLoadVar() ||
          (node1->getOpCodeValue() == TR::loadaddr && node1->getSymbolReference()->getSymbol()->isNotCollected()))
         {
         if (node1->getSymbolReference()->getReferenceNumber() == node2->getSymbolReference()->getReferenceNumber())
            {
            if (node1->getNumChildren() > 0)
               {
               if (bothSubtreesMatch(node1->getFirstChild(), node2->getFirstChild()))
                  return true;
               }
            else
               {
               return true;
               }
            }
         }
      }

   return false;
   }




bool TR_FieldPrivatizer::subtreeIsInvariantInLoop(TR::Node *node)
   {
   if (node->getOpCodeValue() == TR::loadaddr && node->getSymbolReference()->getSymbol()->isNotCollected())
      {
      return true;
      }

   if ((node->getNumChildren() > 1) ||
       !node->getOpCode().isLoadVar() ||
       !_neverWritten->get(node->getSymbolReference()->getReferenceNumber()) ||
       ((node->getNumChildren() > 0) &&
   !subtreeIsInvariantInLoop(node->getFirstChild())))
      return false;

   return true;
   }



void TR_FieldPrivatizer::placeInitializersInLoopInvariantBlock(TR::Block *block)
   {
   ListElement<TR::Node> *currentNodeElem = _privatizedFieldNodes.getListHead();
   TR::SymbolReference   *currentSymRef   = NULL;


   //TR::TreeTop *insertionPoint = block->getEntry();
   TR::TreeTop *placeHolderTree = block->getLastRealTreeTop();
   TR::Node *placeHolderNode = placeHolderTree->getNode();
   if(placeHolderNode->getOpCodeValue() == TR::treetop)
       placeHolderNode = placeHolderNode->getFirstChild();

   TR::ILOpCode &placeHolderOpCode = placeHolderNode->getOpCode();
   if (placeHolderOpCode.isBranch() ||
       placeHolderOpCode.isJumpWithMultipleTargets() ||
       placeHolderOpCode.isReturn() ||
       placeHolderOpCode.getOpCodeValue() == TR::athrow)
      {
      }
   else
      placeHolderTree = block->getExit();

   TR::TreeTop *treeBeforePlaceHolderTree = placeHolderTree->getPrevTreeTop();

   TR_HashId   index  = 0;
   while (currentNodeElem)
      {
      TR::Node *currentNode = currentNodeElem->getData();
      TR::Node *duplicateNode = currentNode->duplicateTree();

      if (duplicateNode->getOpCode().isStore())
	 {
           if (duplicateNode->getOpCode().isIndirect())
              {
              TR::Node::recreate(duplicateNode, comp()->il.opCodeForCorrespondingIndirectStore(duplicateNode->getOpCodeValue()));
              duplicateNode->setNumChildren(1);
              }
           else
              {
              TR::Node::recreate(duplicateNode, comp()->il.opCodeForDirectLoad(duplicateNode->getDataType()));
              duplicateNode->setNumChildren(0);
              }
	 }

      TR::SymbolReference *symRef = duplicateNode->getSymbolReference();
      if (_privatizedFieldSymRefs.locate(symRef->getReferenceNumber(),index))
         {
         currentSymRef =  ((TR::SymbolReference *)_privatizedFieldSymRefs.getData(index));
         }
      else
         TR_ASSERT(false, "\n field #%d should have been privatized but didn't find corresponding new auto in _privatizedFieldSymRefs", symRef->getReferenceNumber());


     dumpOptDetails(comp(), "%s  Privatizing field #%d with temp #%d\n", optDetailString(), duplicateNode->getSymbolReference()->getReferenceNumber(), currentSymRef->getReferenceNumber());


     //if (performTransformation(comp(), "%s  Privatizing field #%d with temp #%d\n", optDetailString(), duplicateNode->getSymbolReference()->getReferenceNumber(), currentSymRefElem->getData()->getReferenceNumber()))
         {
         TR::Node *initNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(duplicateNode->getDataType()), 1, 1, duplicateNode, currentSymRef);
         TR::TreeTop *initTree = TR::TreeTop::create(comp(), initNode, 0, 0);
         treeBeforePlaceHolderTree->join(initTree);
         initTree->join(placeHolderTree);
         placeHolderTree = initTree;
         }

      currentNodeElem = currentNodeElem->getNextElement();
      }
   }




void TR_FieldPrivatizer::placeStoresBackInExits(List<TR::Block> *exitBlocks, List<TR::Block> *blocksInLoop)
   {
   TR::CFG        *cfg              = comp()->getFlowGraph();
   TR_BitVector  *seenExitBlocks   = new (trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), trMemory(), stackAlloc,growable);
   TR_BitVector  *blocksInsideLoop = new (trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), trMemory(), stackAlloc);
   TR_HashTabInt newBlocksHash(trMemory(),stackAlloc);

   TR::Block      *blockInLoop      = 0;
   ListIterator<TR::Block> si(blocksInLoop);
   for (blockInLoop = si.getCurrent(); blockInLoop; blockInLoop = si.getNext())
      blocksInsideLoop->set(blockInLoop->getNumber());

   TR::Block  *exitBlock = 0;
   TR_HashId  index     = 0;
   bool       newBlock = true;
   si.set(exitBlocks);
   for (exitBlock = si.getCurrent(); exitBlock; exitBlock = si.getNext())
       {
       TR::Block *next;
       TR::Block *nnext;
       TR::CFGEdge *edge;
       for (auto edge = exitBlock->getSuccessors().begin(); edge != exitBlock->getSuccessors().end();)
          {
          bool placeAtEnd = false;
          bool ignoreEdge = false;

          TR::CFGEdge* current = *(edge++);
          next = toBlock(current->getTo());

          if (!blocksInsideLoop->get(next->getNumber()) &&
              storesBackMustBePlacedInExitBlock(exitBlock, next, blocksInsideLoop))
             {
             TR_RegionStructure *parent = next->getStructureOf()->asRegion();

             if (newBlocksHash.locate(next->getNumber(),index))
                {
                newBlock = false;
                nnext    = ((TR::Block *)newBlocksHash.getData(index));
                }
             else
                {
                newBlock = true;
                nnext    = TR::Block::createEmptyBlock(next->getEntry()->getNode(),comp(), 0, next);
                newBlocksHash.add(next->getNumber(),index,nnext);
                }

             if (!parent)
                parent = exitBlock->getCommonParentStructureIfExists(next, comp()->getFlowGraph());

             // is next the fall through of exitBlock?
             if (exitBlock->getExit()->getNextTreeTop() == next->getEntry())
                {
                //Update Tree -- insert new block in the middle
                if (newBlock)
                   {
                   exitBlock->getExit()->join(nnext->getEntry());
                   nnext->getExit()->join(next->getEntry());
                   }
                //The block nnext must be appended to the end before, now we can move it to here-pefect
                else
                   {
                   TR::TreeTop *prevTree = nnext->getEntry()->getPrevTreeTop();
                   TR::TreeTop *nextTree = nnext->getExit()->getNextTreeTop();

                   exitBlock->getExit()->join(nnext->getEntry());
                   nnext->getExit()->join(next->getEntry());
                   prevTree->join(nextTree);
                   }

                placeAtEnd = true;
                }
             else
                {
                TR::TreeTop * lastTree = comp()->getMethodSymbol()->getLastTreeTop();

                if (newBlock)
                   {
                   //Append nnext after last tree
                   lastTree->join(nnext->getEntry());
                   nnext->getExit()->join(0);

                   //Add goto next to the end of nnext even if next if fall through
                   TR::Node *gotoNode  = TR::Node::create(next->getEntry()->getNode(), TR::Goto);
                   gotoNode->setBranchDestination(next->getEntry());
                   nnext->append(TR::TreeTop::create(comp(), gotoNode));
                   }


                placeAtEnd = false;
                }

             //Update branch target
             //D191793, there are cases while we goto next block, so update branchDestination conditionaly for safety
             exitBlock->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), next->getEntry(), nnext->getEntry());

             if (newBlock)
                {
                nnext->inheritBlockInfo(next,false);
                comp()->getFlowGraph()->addNode(nnext,parent);
                nnext->setFrequency(current->getFrequency());
                TR::CFGEdge *newEdge = comp()->getFlowGraph()->addEdge(nnext, next);
                newEdge->setFrequency(current->getFrequency());
                if (trace())
                   traceMsg(comp(), "placeStoresBackInExits: added block %d freq %d\n",
                         nnext->getNumber(), nnext->getFrequency());
                }
             TR::CFGEdge *newEdge = comp()->getFlowGraph()->addEdge(exitBlock, nnext);
             newEdge->setFrequency(current->getFrequency());
             if (trace())
                {
                TR::Block *from = newEdge->getFrom()->asBlock();
                TR::Block *to = newEdge->getTo()->asBlock();

                traceMsg(comp(), "new edge %d(%d) -> %d(%d) freq %d\n",
                      from->getNumber(), from->getFrequency(),
                      to->getNumber(), to->getFrequency(),
                      current->getFrequency());
                from = current->getFrom()->asBlock();
                to = current->getTo()->asBlock();
                traceMsg(comp(), "instead of orig edge %d(%d) -> %d(%d) freq %d\n",
                      from->getNumber(), from->getFrequency(),
                      to->getNumber(), to->getFrequency(),
                      current->getFrequency());
                }

             //Remove the origin edge from exitBlock to  next
             //since insertBlockAsFallThrough doesn't do it
             comp()->getFlowGraph()->removeEdge(exitBlock, next);
             next = nnext;
             }
          else if (blocksInsideLoop->get(next->getNumber()))
             ignoreEdge = true;

          if (!ignoreEdge &&
              !seenExitBlocks->get(next->getNumber()))
             {
             if ((exitBlock == next) || !blocksInsideLoop->get(next->getNumber()))
                {
                seenExitBlocks->set(next->getNumber());
                placeStoresBackInExit(next, placeAtEnd);
                }
             }
          }
       }
   }



void TR_FieldPrivatizer::placeStoresBackInExit(TR::Block *block, bool placeAtEnd)
   {
   ListElement<TR::Node> *currentNodeElem = _privatizedFieldNodes.getListHead();
   TR::SymbolReference *currentSymRef     = NULL;
   TR_HashId          index              = 0;
   ListElement<TR_RegisterCandidate> *privatizedCandidate = _privatizedRegCandidates.getListHead();

   int32_t blockWeight = 1;
   optimizer()->getStaticFrequency(block, &blockWeight);

   TR::TreeTop *insertionPoint = block->getEntry();
   if (placeAtEnd)
       insertionPoint = block->getLastRealTreeTop();

   while (currentNodeElem)
      {
      TR::Node            *currentNode   = currentNodeElem->getData();
      TR::Node            *duplicateNode = currentNode->duplicateTree();
      TR::SymbolReference *symRef        = duplicateNode->getSymbolReference();

      if (_privatizedFieldSymRefs.locate(symRef->getReferenceNumber(),index))
         {
         currentSymRef =  ((TR::SymbolReference *)_privatizedFieldSymRefs.getData(index));
         }
      else
         TR_ASSERT(false, "\n field #%d should have been privatized but didn't find corresponding new auto in _privatizedFieldSymRefs", symRef->getReferenceNumber());

      if (_needToStoreBack->get(currentSymRef->getReferenceNumber()))
         {
         if ( currentNode->getOpCode().isIndirect() )
            {
            if (!duplicateNode->getOpCode().isStore())
               {
               TR::Node::recreate(duplicateNode, comp()->il.opCodeForCorrespondingIndirectLoad(duplicateNode->getOpCodeValue()));
               }

            if (duplicateNode->getOpCode().isWrtBar())
               duplicateNode->setNumChildren(3);
            else
               duplicateNode->setNumChildren(2);

            TR::Node *storedBackValue = TR::Node::createWithSymRef(duplicateNode, comp()->il.opCodeForDirectLoad(duplicateNode->getDataType()), 0, currentSymRef);
            duplicateNode->setAndIncChild(1, storedBackValue);
            }
         else
            {
            if (!duplicateNode->getOpCode().isStore())
               TR::Node::recreate(duplicateNode, comp()->il.opCodeForDirectStore(duplicateNode->getDataType()));
            if (duplicateNode->getOpCode().isWrtBar())
               duplicateNode->setNumChildren(2);
            else
               duplicateNode->setNumChildren(1);

            TR::Node *storedBackValue = TR::Node::createWithSymRef(duplicateNode, comp()->il.opCodeForDirectLoad(duplicateNode->getDataType()), 0, currentSymRef);
            duplicateNode->setAndIncChild(0, storedBackValue);
            }

         TR::TreeTop *storeBackTree = TR::TreeTop::create(comp(), duplicateNode, 0, 0);

         if (placeAtEnd && insertionPoint && (insertionPoint->getNode()->getOpCodeValue() != TR::BBStart))
            {
            TR::TreeTop *prevTree = insertionPoint->getPrevTreeTop();
            prevTree->join(storeBackTree);
            storeBackTree->join(insertionPoint);
            }
         else
            {
            TR::TreeTop *nextTree = insertionPoint->getNextTreeTop();
            insertionPoint->join(storeBackTree);
            storeBackTree->join(nextTree);
            }

            // Add global reg candidate in exit as well
            // so that store backs are done from the registers
            //
            privatizedCandidate->getData()->addBlock(block, blockWeight);
         }

      currentNodeElem = currentNodeElem->getNextElement();
      privatizedCandidate = privatizedCandidate->getNextElement();
      }
   }


bool TR_FieldPrivatizer::storesBackMustBePlacedInExitBlock(TR::Block *exitBlock, TR::Block *toBlock, TR_BitVector *blocksInsideLoop)
   {
   if ((toBlock == comp()->getFlowGraph()->getEnd()) ||
       (exitBlock->getSuccessors().size() == 1))
      return true;

   TR::CFGNode *next;
   TR::CFGEdge *edge;
   for (auto edge = toBlock->getPredecessors().begin(); edge != toBlock->getPredecessors().end(); ++edge)
      {
      next = (*edge)->getFrom();
      if (!blocksInsideLoop->get(next->getNumber()) &&
     (next != _criticalEdgeBlock))
         return true;
      }

   return false;
   }



void TR_FieldPrivatizer::addPrivatizedRegisterCandidates(TR_Structure *structure)
   {
   ListElement<TR_RegisterCandidate> *privatizedCandidate = _privatizedRegCandidates.getListHead();
   while (privatizedCandidate)
      {
      if (performTransformation(comp(), "%s Adding auto %d (created for privatization) as a global register candidate in loop %d\n", optDetailString(), privatizedCandidate->getData()->getSymbolReference()->getReferenceNumber(), structure->getNumber()))
         privatizedCandidate->getData()->addAllBlocksInStructure(structure, comp(), trace()? "privatization auto":NULL);
      privatizedCandidate = privatizedCandidate->getNextElement();
      }
   }



void TR_FieldPrivatizer::addStringInitialization(TR::Block *block)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR_ResolvedMethod *vmMethod = comp()->getCurrentMethod();
   TR::ResolvedMethodSymbol *methodSymbol = comp()->getOwningMethodSymbol(vmMethod);

   TR::TreeTop *insertionPoint = block->getEntry();

   _stringBufferClass = fe()->getClassFromSignature("java/lang/StringBuffer", 22, comp()->getCurrentMethod());
   if (!_stringBufferClass) return;
   //the call to findOrCreateClassSymbol is safe even though we pass CPI of -1 since it is guarded by the getClassFromSignature call for _stringBuffer
   TR::SymbolReference *stringBufferSymRef = comp()->getSymRefTab()->findOrCreateClassSymbol(methodSymbol, -1, _stringBufferClass);
   TR::Node *classObject = TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, stringBufferSymRef);
   TR::Node *initNode = TR::Node::createWithSymRef(TR::New, 1, 1, classObject, comp()->getSymRefTab()->findOrCreateNewObjectSymbolRef(methodSymbol));
   initNode = TR::Node::create(TR::treetop, 1, initNode);

   TR::TreeTop *initTree = TR::TreeTop::create(comp(), initNode, 0, 0);

   if (!_initSymRef)
      {
      List<TR_ResolvedMethod> stringBufferMethods(trMemory());
      comp()->fej9()->getResolvedMethods(trMemory(), _stringBufferClass, &stringBufferMethods);
      ListIterator<TR_ResolvedMethod> it(&stringBufferMethods);
      for (TR_ResolvedMethod *method = it.getCurrent(); method; method = it.getNext())
         {
         if (method->isConstructor())
            {
            char *sig = method->signatureChars();
            if (!strncmp(sig, "(Ljava/lang/String;)V", 21))
               {
               _initSymRef = getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, method, TR::MethodSymbol::Special);
               break;
               }
            }
         }
      }

   if (!_initSymRef)
       {
       TR_ASSERT(0, "Expected StringBuffer library method not found\n");
       return;
       }

   _tempStringSymRef = comp()->getSymRefTab()->createTemporary(methodSymbol, TR::Address);

   if (!performTransformation(comp(), "%s  Inserted string init into symRef #%d\n", optDetailString(), _tempStringSymRef->getReferenceNumber()))
      return;

   TR::SymbolReference *initSymRef = _initSymRef ? getSymRefTab()->findOrCreateMethodSymbol(initNode->getFirstChild()->getSymbolReference()->getOwningMethodIndex(), -1, _initSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;
   TR::Node *initCall = TR::Node::create(TR::treetop, 1, TR::Node::createWithSymRef(TR::call, 2, 2,
                              initNode->getFirstChild(), TR::Node::createWithSymRef(initNode->getFirstChild(), TR::aload, 0, _stringSymRef), initSymRef));
   TR::TreeTop *initCallTree = TR::TreeTop::create(comp(), initCall, 0, 0);

   initTree->join(initCallTree);


   TR::TreeTop *nextTree = insertionPoint->getNextTreeTop();
   insertionPoint->join(initTree);
   initCallTree->join(nextTree);
   insertionPoint = initCallTree;

   initNode = TR::Node::createWithSymRef(TR::astore, 1, 1, initNode->getFirstChild(), _tempStringSymRef);
   initTree = TR::TreeTop::create(comp(), initNode, 0, 0);
   nextTree = insertionPoint->getNextTreeTop();
   insertionPoint->join(initTree);
   initTree->join(nextTree);
#endif
   }




void TR_FieldPrivatizer::placeStringEpilogueInExits(List<TR::Block> *exitBlocks, List<TR::Block> *blocksInLoop)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::CFG *cfg = comp()->getFlowGraph();
   TR_BitVector *seenExitBlocks = new (trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), trMemory(), stackAlloc);
   TR_BitVector *blocksInsideLoop = new (trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), trMemory(), stackAlloc);

   TR::Block *blockInLoop = 0;
   ListIterator<TR::Block> si(blocksInLoop);
   for (blockInLoop = si.getCurrent(); blockInLoop; blockInLoop = si.getNext())
      blocksInsideLoop->set(blockInLoop->getNumber());

   TR::Block *exitBlock = 0;
   si.set(exitBlocks);
   for (exitBlock = si.getCurrent(); exitBlock; exitBlock = si.getNext())
       {
       TR::Block *next;
       TR::CFGEdge *edge;
       for (auto edge = exitBlock->getSuccessors().begin(); edge != exitBlock->getSuccessors().end(); ++edge)
          {
          bool placeAtEnd = false;
          next = toBlock((*edge)->getTo());

          //if (next == cfg->getEnd())
          if (!blocksInsideLoop->get(next->getNumber()) &&
             storesBackMustBePlacedInExitBlock(exitBlock, next, blocksInsideLoop))
             {
             next = exitBlock;
             placeAtEnd = true;
             }

          if (!seenExitBlocks->get(next->getNumber()))
             {
             if ((exitBlock == next) ||
                 !blocksInsideLoop->get(next->getNumber()))
                {
                seenExitBlocks->set(next->getNumber());
                placeStringEpiloguesBackInExit(next, placeAtEnd);
                }
             }
         }
      }
#endif
   }




void TR_FieldPrivatizer::placeStringEpiloguesBackInExit(TR::Block *block, bool placeAtEnd)
   {
#if J9_PROJECT_SPECIFIC
   if (!_toStringSymRef)
      {
      TR_ScratchList<TR_ResolvedMethod> stringBufferMethods(trMemory());
      comp()->fej9()->getResolvedMethods(trMemory(), _stringBufferClass, &stringBufferMethods);
      ListIterator<TR_ResolvedMethod> it(&stringBufferMethods);
      for (TR_ResolvedMethod *method = it.getCurrent(); method; method = it.getNext())
         {
         if (!strncmp(method->nameChars(), "toString", 8))
            {
            char *sig = method->signatureChars();
            if (!strncmp(sig, "()Ljava/lang/String;", 20))
               {
               _toStringSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, method, TR::MethodSymbol::Virtual);
               break;
               }
            }
         }
      }

   if (!_toStringSymRef)
     {
     TR_ASSERT(0, "Expected StringBuffer library method not found\n");
     return;
     }

   TR::TreeTop *insertionPoint = block->getEntry();
   if (placeAtEnd)
       insertionPoint = block->getLastRealTreeTop();

   TR::Node *tempStringBufferLoad = TR::Node::createWithSymRef(insertionPoint->getNode(), TR::aload, 0, _tempStringSymRef);

   TR::SymbolReference *toStringSymRef = _toStringSymRef ? comp()->getSymRefTab()->findOrCreateMethodSymbol(tempStringBufferLoad->getSymbolReference()->getOwningMethodIndex(), -1, _toStringSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Virtual) : 0;
   TR::Node *toStringCall = TR::Node::createWithSymRef(TR::acall, 1, 1,
                  tempStringBufferLoad, toStringSymRef);

   TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, toStringCall);
   TR::TreeTop *treetop = TR::TreeTop::create(comp(), treetopNode, 0, 0);


   TR::Node *storeBack = TR::Node::createWithSymRef(TR::astore, 1, 1, toStringCall, _stringSymRef);
   TR::TreeTop *storeBackTree = TR::TreeTop::create(comp(), storeBack, 0, 0);

   treetop->join(storeBackTree);

   if (placeAtEnd)
      {
      TR::TreeTop *prevTree = insertionPoint->getPrevTreeTop();
      prevTree->join(treetop);
      storeBackTree->join(insertionPoint);
      }
   else
      {
      TR::TreeTop *nextTree = insertionPoint->getNextTreeTop();
      insertionPoint->join(treetop);
      storeBackTree->join(nextTree);
      }
#endif
   }


void TR_FieldPrivatizer::cleanupStringPeephole()
   {
#ifdef J9_PROJECT_SPECIFIC
   if (!_tempStringSymRef)
      {
      TR_ASSERT(0, "Temp must have been allocated for string peephole\n");
      return;
      }

  if (!_appendSymRef)
      {
      TR_ScratchList<TR_ResolvedMethod> stringBufferMethods(trMemory());
      comp()->fej9()->getResolvedMethods(trMemory(), _stringBufferClass, &stringBufferMethods);
      ListIterator<TR_ResolvedMethod> it(&stringBufferMethods);
      for (TR_ResolvedMethod *method = it.getCurrent(); method; method = it.getNext())
         {
         if ((method->nameLength() == 15) &&
             !strncmp(method->nameChars(), "jitAppendUnsafe", 15))
            {
            char *sig = method->signatureChars();
            if (!strncmp(sig, "(C)Ljava/lang/StringBuffer;", 27))
               {
               _appendSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, method, TR::MethodSymbol::Special);
               break;
               }
            }
         }

   TR::TreeTop *currTree = _stringPeepholeTree;
   TR::TreeTop *prevTree = currTree->getPrevTreeTop();
   TR::TreeTop *nextTree = currTree->getNextTreeTop();
   TR::Node *prevNode = prevTree->getNode();

   if (prevNode->getOpCode().isStore())
      {
      TR::Node::recreate(prevNode, TR::treetop);
      TR::Node *firstChild = prevNode->getFirstChild();
      TR::TreeTop *cursorTree = prevTree->getPrevTreeTop();
      while(cursorTree)
         {
         if ((cursorTree->getNode()->getNumChildren() > 0) &&
             (cursorTree->getNode()->getFirstChild() == firstChild))
            {
            _appendCalls.add(cursorTree);
            break;
            }
         else if (cursorTree->getNode()->getOpCodeValue() == TR::BBStart)
            break;

         cursorTree = cursorTree->getPrevTreeTop();
         }

       if (firstChild->getOpCodeValue() == TR::New)
          {
          TR::Node::recreate(firstChild, TR::acall);

          TR::SymbolReference *appendSymRef = _appendSymRef ? comp()->getSymRefTab()->findOrCreateMethodSymbol(firstChild->getSymbolReference()->getOwningMethodIndex(), -1, _appendSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod(), TR::MethodSymbol::Special) : 0;

          firstChild->setSymbolReference(appendSymRef);
          int32_t i;
          for (i=0;i<firstChild->getNumChildren();i++)
             firstChild->getChild(i)->recursivelyDecReferenceCount();

          firstChild->setNumChildren(2);
          TR::Node *load = TR::Node::createWithSymRef(prevNode, TR::aload, 0, _tempStringSymRef);
          TR::Node *currNode = currTree->getNode();
          TR::Node *character = currNode->getFirstChild()->getChild(2);
          firstChild->setAndIncChild(0, load);
          firstChild->setAndIncChild(1, character);

          currNode->recursivelyDecReferenceCount();
          prevTree->join(nextTree);
          }
       }
    else
       TR_ASSERT(0, "Pattern not matched at this stage\n");
      }
#endif
   }

const char *
TR_FieldPrivatizer::optDetailString() const throw()
   {
   return "O^O FIELD PRIVATIZATION: ";
   }
