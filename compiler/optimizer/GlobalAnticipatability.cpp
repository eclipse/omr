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
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/LocalAnalysis.hpp"

class TR_OpaqueClassBlock;
namespace TR { class Optimizer; }

// #define MAX_BLOCKS_FOR_STACK_ALLOCATION 16

// This file contains an implementation of Global Anticipatability which
// is the first global bit vector analyses used by PRE. Global anticipatability
// attempts to discover points in the program that are possibilities for
// being optimal. If an expression is not globally anticipatable at some
// program point, then that point cannot be an optimal placement point
// for the expression.
//
//
TR_DataFlowAnalysis::Kind TR_GlobalAnticipatability::getKind()
   {
   return GlobalAnticipatability;
   }

TR_GlobalAnticipatability *TR_GlobalAnticipatability::asGlobalAnticipatability()
   {
   return this;
   }


int32_t TR_GlobalAnticipatability::getNumberOfBits()
   {
   return _localTransparency.getNumNodes();
   }


void TR_GlobalAnticipatability::analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *)
   {
   }




TR_GlobalAnticipatability::TR_GlobalAnticipatability(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure, bool trace)
   : TR_BackwardIntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, trace),
     _localAnalysisInfo(comp, trace),
     _localTransparency(_localAnalysisInfo, trace),
     _localAnticipatability(_localAnalysisInfo, &_localTransparency, trace)
   {
   if (trace)
      traceMsg(comp, "Starting GlobalAnticipatability\n");

   _supportedNodesAsArray = _localAnalysisInfo._supportedNodesAsArray;

   initializeBlockInfo();

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   performAnalysis(rootStructure, false);

   int32_t j;
   for (j = 0; j < _numberOfNodes; j++)
     *_blockAnalysisInfo[j] |= *(_localAnticipatability.getDownwardExposedAnalysisInfo(j));

   if (trace)
      {
      int32_t i;
      for (i = 0; i < _numberOfNodes; i++)
         {
         traceMsg(comp, "Block number : %d has solution : ", i);
         _blockAnalysisInfo[i]->print(comp);
         traceMsg(comp, "\n");
         }

      traceMsg(comp, "Ending GlobalAnticipatability\n");
      }

   } // scope of the stack memory region

   // Null out info not used by callers
   _outSetInfo = NULL;
   }

bool TR_GlobalAnticipatability::postInitializationProcessing()
   {
   // In this case, i.e. global anticipatability _outSetInfo is required
   // as a field. So it is specifically allocated here and not as part
   // of the other more general bit vectors allocated by any backward
   // bit vector analysis.
   //
   _outSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numberOfNodes*sizeof(ContainerType *));

   int32_t i;
   for (i = 0; i < _numberOfNodes; i++)
      allocateContainer(_outSetInfo+i);

   allocateContainer(&_scratch);
   allocateContainer(&_scratch2);
   allocateContainer(&_scratch3);

   // this array will store _checkExpressions info per block
   // (some expressions can be exceptional in some blocks but not others)
   _checkExpressionsInBlock = (ContainerType **)trMemory()->allocateStackMemory(_numberOfNodes*sizeof(ContainerType *));
   memset(_checkExpressionsInBlock, 0, _numberOfNodes*sizeof(ContainerType *));
   return true;
   }


static bool isFieldAccess(TR::Node *nextNode)
   {
   if (nextNode->getOpCode().isIndirect() &&
       nextNode->getOpCode().isLoadVar() &&
       nextNode->getOpCode().hasSymbolReference() &&
       !nextNode->getSymbolReference()->isUnresolved() &&
       nextNode->getSymbolReference()->getSymbol()->isShadow() &&
       !nextNode->isInternalPointer() &&
       (!nextNode->getOpCode().isArrayLength()))
      {
      TR::Node *firstChild = nextNode->getFirstChild();
      if ((firstChild->getOpCodeValue() == TR::aload) &&
          firstChild->getSymbolReference()->getSymbol()->isAutoOrParm())
         return true;
      }

   return false;
   }


static bool nodeCanSurvive(TR::Node *nextNode, TR::Node *lastNodeFirstChild, TR::Node *lastNodeSecondChild, TR::Compilation *comp, bool trace)
   {
   if (isFieldAccess(nextNode))
      {
      int32_t similarOffset = -1;
      bool seenSimilarAccess = false;
      TR::Node *firstChild = nextNode->getFirstChild();
      if (lastNodeFirstChild)
         {
         if (lastNodeFirstChild->getFirstChild()->getLocalIndex() == firstChild->getLocalIndex())
            {
            similarOffset = lastNodeFirstChild->getSymbolReference()->getOffset();
            seenSimilarAccess = true;
            }
         }

      if (lastNodeSecondChild)
         {
         if (lastNodeSecondChild->getFirstChild()->getLocalIndex() == firstChild->getLocalIndex())
            {
            if (similarOffset < lastNodeSecondChild->getSymbolReference()->getOffset())
               similarOffset = lastNodeSecondChild->getSymbolReference()->getOffset();

            seenSimilarAccess = true;
            }
         }

      if (trace)
         traceMsg(comp, "seen similar access %d\n", seenSimilarAccess);

      if (seenSimilarAccess)
         {
         if (similarOffset >= nextNode->getSymbolReference()->getOffset())
            return true;

         TR::SymbolReference *symRef = firstChild->getSymbolReference();
         int32_t len;
         const char *sig = symRef->getTypeSignature(len);

         TR::SymbolReference *otherSymRef = nextNode->getSymbolReference();

         TR_OpaqueClassBlock *cl = NULL;
         if (sig && (len > 0))
            cl = comp->fe()->getClassFromSignature(sig, len, otherSymRef->getOwningMethod(comp));

         TR_OpaqueClassBlock *otherClassObject = NULL;
         int32_t otherLen;
         const char *otherSig = otherSymRef->getOwningMethod(comp)->classNameOfFieldOrStatic(otherSymRef->getCPIndex(), otherLen);
         if (otherSig)
            {
            otherSig = classNameToSignature(otherSig, otherLen, comp);
            otherClassObject = comp->fe()->getClassFromSignature(otherSig, otherLen, otherSymRef->getOwningMethod(comp));
            }

         if (trace)
            traceMsg(comp, "cl %p other cl %p\n", cl, otherClassObject);

         if (cl && otherClassObject && (comp->fe()->isInstanceOf(cl, otherClassObject, true) == TR_yes))
            return true;
         }
      }

   return false;
   }

extern bool isExceptional(TR::Compilation *, TR::Node *); // in LocalAnalysis.cpp

bool TR_GlobalAnticipatability::isExceptionalInBlock(TR::Node * node, int32_t blockNumber, ContainerType *alreadyInBlock, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (isExceptionalInBlock(child, blockNumber, alreadyInBlock, visitCount))
         {
         node->setVisitCount(visitCount-1); // so that node gets visited again for another expression in this block
         return true;
         }
      }

   if (node->getLocalIndex() != -1 &&
       alreadyInBlock->get(node->getLocalIndex()))
      return false;

   if (isExceptional(comp(), node))
      {
      node->setVisitCount(visitCount-1);
      return true;
      }
   else
      {
      return false;
      }
   }


TR_GlobalAnticipatability::ContainerType * TR_GlobalAnticipatability::getCheckExpressionsInBlock(int32_t blockNumber)
   {
   return _localTransparency.getCheckExpressions();
   }


bool isRareEdge(TR::Compilation *comp, TR::CFGEdge *edge)
   {
   return false; // has to be performance tested with Java
/*

#if 0
   traceMsg (comp, "Edge from %d to %d has freq %d\n", toBlock(edge->getFrom())->getNumber(),
                                                     toBlock(edge->getTo())->getNumber(),
                                                     edge->getFrequency());
#endif
   return (edge->getFrequency() == 1);*/
   }

void TR_GlobalAnticipatability::killBasedOnSuccTransparency(TR::Block *block)
   {
   TR::Block   *nextSucc;
   for (auto succEdge = block->getSuccessors().begin(); succEdge != block->getSuccessors().end(); ++succEdge)
      {
      nextSucc = toBlock((*succEdge)->getTo());
      *_scratch = *(_localTransparency.getAnalysisInfo(nextSucc->getNumber()));
      *_scratch |= *(_localAnticipatability.getAnalysisInfo(nextSucc->getNumber()));
      *_regularInfo &= *_scratch;
      }
   }

// Overrides the implementation in the superclass as this analysis
// is slightly different from conventional bit vector analyses.
// It uses the results from local analyses instead of examining
// each tree top for effects on the input bit vector at that tree top.
// This analysis has a trivial analyzeNode(...) method as a result.
//
//
void TR_GlobalAnticipatability::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {
   TR::Block *block = blockStructure->getBlock();
   TR::TreeTop *currentTree = block->getExit();
   TR::TreeTop *entryTree = block->getEntry();
   bool notSeenTreeWithChecks = true;

   killBasedOnSuccTransparency(block);
   copyFromInto(_regularInfo, _outSetInfo[blockStructure->getNumber()]);

   _containsExceptionTreeTop = false;
   while (!(currentTree == entryTree))
      {
      if (notSeenTreeWithChecks)
         {
         bool currentTreeHasChecks = treeHasChecks(currentTree);
         if (currentTreeHasChecks)
            {
            notSeenTreeWithChecks = false;
            _containsExceptionTreeTop = true;
            compose(_regularInfo, _exceptionInfo);
            compose(_outSetInfo[blockStructure->getNumber()], _exceptionInfo);
            }
         }
      else
         break;

      if (!(currentTree == entryTree))
         currentTree = currentTree->getPrevTreeTop();
      }

   if (block != comp()->getFlowGraph()->getEnd())
      {
      // Ignore the effect of cold blocks on anticipatability
      //
      TR::CFGEdge *edge;
      TR::Block   *next;
      bool hasNonColdSuccessor = false;
      bool analyzedSucc = true;
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         next = toBlock((*edge)->getTo());
         int32_t blockWeight = 1;
         if (!next->isCold())
            next->getStructureOf()->calculateFrequencyOfExecution(&blockWeight);

         bool rare = isRareEdge(comp(), *edge);

         if (!rare &&
             blockWeight > 1 &&
             next->getStructureOf()->hasBeenAnalyzedBefore())
	    {
	    hasNonColdSuccessor = true;
	    }

         if (!next->getStructureOf()->hasBeenAnalyzedBefore())
	    {
	    analyzedSucc = false;
	    }

         if (hasNonColdSuccessor && !analyzedSucc)
	    break;
	 }

      if (analyzedSucc)
	 {
         _regularInfo->setAll(_numberOfBits);
         killBasedOnSuccTransparency(block);

         TR::Node *lastNodeFirstChild = NULL;
         TR::Node *lastNodeSecondChild = NULL;

         TR::TreeTop *lastTree = block->getLastRealTreeTop();
         if (lastTree)
            {
            TR::Node *lastNode = lastTree->getNode();
            if (lastNode->getOpCode().isIf())
               {
               lastNodeFirstChild = lastNode->getFirstChild();
               if (!isFieldAccess(lastNodeFirstChild))
                  lastNodeFirstChild = NULL;

               lastNodeSecondChild = lastNode->getSecondChild();
               if (!isFieldAccess(lastNodeSecondChild))
                  lastNodeSecondChild = NULL;
               }
            }

         for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
            {
            next = toBlock((*edge)->getTo());
            ExtraAnalysisInfo *analysisInfo = getAnalysisInfo(next->getStructureOf());

            int32_t blockWeight = 1;
            bool rare = isRareEdge(comp(),*edge);

            if (!next->isCold())
               next->getStructureOf()->calculateFrequencyOfExecution(&blockWeight);

            if ((!rare && !next->isCold() && blockWeight > 1) ||
	        !hasNonColdSuccessor)
               {
               *_regularInfo &= *(analysisInfo->_inSetInfo);
               }
            else
               {
               _scratch->setAll(_numberOfBits);

               *_scratch2 = *_scratch;
               *_scratch2 &= *_localTransparency.getCheckExpressions();
               //*_scratch2 &= *(_localAnticipatability.getAnalysisInfo(next->getNumber()));
               _scratch3->empty();

               if (trace())
                  {
                  //traceMsg(comp(), "_scratch2 : ");
                  //_scratch2->print(comp());
                  //traceMsg(comp(), "\n");
                  }
               if ((lastNodeFirstChild || lastNodeSecondChild) &&
                  !_scratch2->isEmpty())
                  {
                   ContainerType::Cursor bvi(*_scratch2);
                   for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
                      {
                      int32_t nextExpression = bvi;
                     TR::Node *nextNode = _supportedNodesAsArray[nextExpression];
                     if (trace())
                        traceMsg(comp(), "next expression %d\n", nextExpression);
                     if (nodeCanSurvive(nextNode, lastNodeFirstChild, lastNodeSecondChild, comp(), trace()))
                        {
                        _scratch3->set(nextExpression);
                        }
                     }
                  }

               *_scratch -= *_localTransparency.getCheckExpressions();
               *_scratch |= *_scratch3;
               *_scratch |= *(analysisInfo->_inSetInfo);
               *_regularInfo &= *_scratch;
               }
            }

         // make expressions available on both sides of 'if' if it post-dominates loop entry
         int32_t blockWeight = 1;
         blockStructure->calculateFrequencyOfExecution(&blockWeight);
         if (blockWeight > 1 &&
             block->getFrequency() == blockStructure->getParent()->getEntryBlock()->getFrequency() &&
             block->getFrequency() == (MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT) &&
             block->isPRECandidate())   // set in loop versioner
            {
            _scratch->empty();
            for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
                {
                next = toBlock((*edge)->getTo());
                ExtraAnalysisInfo *analysisInfo = getAnalysisInfo(next->getStructureOf());
                bool rare = isRareEdge(comp(),*edge);

                int32_t blockWeight = 1;
                if (!next->isCold())
                   next->getStructureOf()->calculateFrequencyOfExecution(&blockWeight);

                if (!rare && blockWeight > 1)
                   {
                   dumpOptDetails(comp(), "Ignoring ifcmp within loop in block_%d and adding available expressions from block_%d\n", block->getNumber(), next->getNumber());
                   *_scratch |= *(analysisInfo->_inSetInfo);
                   }
                }

            if (!_scratch->isEmpty())
               {
#if 0
               *_scratch -= _localTransparency.getCheckExpressions();
#else
               *_scratch -= *getCheckExpressionsInBlock(block->getNumber());
#endif
               *_regularInfo |= *_scratch;
               }
            }
	 }

      *_regularInfo &= *(_localTransparency.getAnalysisInfo(blockStructure->getBlock()->getNumber()));
      *_regularInfo |= *(_localAnticipatability.getDownwardExposedAnalysisInfo(blockStructure->getBlock()->getNumber()));
   }

   if (trace())
      {
      traceMsg(comp(), "\nLocal Anticipatability of Block : %d\n", blockStructure->getBlock()->getNumber());
      _localAnticipatability.getDownwardExposedAnalysisInfo(blockStructure->getBlock()->getNumber())->print(comp());

      traceMsg(comp(), "\nIn Set of Block : %d\n", blockStructure->getNumber());
      _regularInfo->print(comp());

      }
   }
