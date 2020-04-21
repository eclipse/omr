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

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"

class TR_BitVector;
#define MAX_CASES_FOR_STACK_ALLOCATION 16

static int32_t numIterations = 0;

// This file contains the methods specifying the backward
// bit vector analysis rules for each structure. Also contain
// definitions of other methods in class BackwardBitVectorAnalysis.
//
//
template<class Container>void TR_BackwardDFSetAnalysis<Container *>::initializeDFSetAnalysis()
   {
   this->initializeBasicDFSetAnalysis();

   _currentOutSetInfo = (Container **)this->trMemory()->allocateStackMemory(this->_numberOfNodes*sizeof(Container *));
   _originalOutSetInfo = (Container **)this->trMemory()->allocateStackMemory(this->_numberOfNodes*sizeof(Container *));

   for (int32_t i=0;i<this->_numberOfNodes;i++)
      {
      this->allocateContainer(&_currentOutSetInfo[i]);
      this->allocateContainer(&_originalOutSetInfo[i]);
      }
   }

template<class Container>bool TR_BackwardDFSetAnalysis<Container *>::canGenAndKillForStructure(TR_Structure *nodeStructure)
   {
   // Turn off for this release to minimize risk
   //
   //if (!supportsGenAndKillSetsForStructures())
      return false;

   if (this->_hasImproperRegion ||
       nodeStructure->containsImproperRegion())
      return false;

   TR_RegionStructure *naturalLoop = nodeStructure->asRegion();
   if (naturalLoop && naturalLoop->isNaturalLoop())
      {
      TR_StructureSubGraphNode *entryNode = naturalLoop->getEntry();
      TR_PredecessorIterator predecessors(entryNode);
      for (auto pred = predecessors.getFirst(); pred; pred = predecessors.getNext())
         {
         TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) pred->getFrom();
         bool exitEdgeSeen = false;
         TR_SuccessorIterator successors(predNode);
         for (auto succ = successors.getFirst(); succ; succ = successors.getNext())
            {
            if (!naturalLoop->isExitEdge(succ))
               {
               if (succ->getTo() != entryNode)
                  {
                  naturalLoop->setContainsImproperRegion(true);
                  break;
                  }
               }
            else
               exitEdgeSeen = true;
            }

         if (!exitEdgeSeen)
            naturalLoop->setContainsImproperRegion(true);
         }

      TR_BitVector seenExitNodes(this->comp()->trMemory()->currentStackRegion());
      ListIterator<TR::CFGEdge> ei(&naturalLoop->getExitEdges());
      for (TR::CFGEdge *edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
        {
        TR::CFGNode *toNode = edge->getTo();

        if (seenExitNodes.get(toNode->getNumber()))
           continue;

        seenExitNodes.set(toNode->getNumber());

        bool seenNonBackEdge = false;
        bool seenLoopBackEdge = false;

        TR_PredecessorIterator predecessors(toNode);
        for (auto pred = predecessors.getFirst(); pred; pred = predecessors.getNext())
           {
           TR::CFGNode *fromNode = pred->getFrom();
           if (fromNode->hasSuccessor(entryNode) ||
               fromNode->hasExceptionSuccessor(entryNode))
              {
              if (seenNonBackEdge)
                 {
                 naturalLoop->setContainsImproperRegion(true);
                 break;
                 }
              seenLoopBackEdge = true;
              }
           else
              {
              if (seenLoopBackEdge)
                 {
                 naturalLoop->setContainsImproperRegion(true);
                 break;
                 }
              seenNonBackEdge = true;
              }
           }
        }

      if (naturalLoop->containsImproperRegion())
         {
         TR_Structure *parent = naturalLoop->getParent();
         while (parent)
            {
            parent->asRegion()->setContainsImproperRegion(true);
            parent = parent->getParent();
            }
         return false;
         }
      }

   return true;
   }

template<class Container>void TR_BackwardDFSetAnalysis<Container *>::initializeGenAndKillSetInfoForBlock(TR_BlockStructure *s)
   {
   typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *analysisInfo = this->getAnalysisInfo(s);
   if (!s->hasBeenAnalyzedBefore())
       s->setAnalyzedStatus(true);
   else
      return;

   analysisInfo->_regularGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(this->_regularGenSetInfo[s->getNumber()], s->getNumber());
   analysisInfo->_regularGenSetInfo->add(pair);

   analysisInfo->_regularKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(this->_regularKillSetInfo[s->getNumber()], s->getNumber());
   analysisInfo->_regularKillSetInfo->add(pair);

   analysisInfo->_exceptionGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(this->_exceptionGenSetInfo[s->getNumber()], s->getNumber());
   analysisInfo->_exceptionGenSetInfo->add(pair);

   analysisInfo->_exceptionKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(this->_exceptionKillSetInfo[s->getNumber()], s->getNumber());
   analysisInfo->_exceptionKillSetInfo->add(pair);

   analysisInfo->_currentRegularGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentRegularKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentExceptionGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentExceptionKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;

   TR::Block *block = s->asBlock()->getBlock();
   for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
      {
      TR::CFGNode* succBlock = (*succ)->getTo();
      typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, succBlock->getNumber());
      analysisInfo->_currentRegularGenSetInfo->add(pair);
      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, succBlock->getNumber());
      analysisInfo->_currentRegularKillSetInfo->add(pair);
      }

   for (auto succ = block->getExceptionSuccessors().begin(); succ != block->getExceptionSuccessors().end(); ++succ)
      {
      TR::CFGNode* succBlock = (*succ)->getTo();
      typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, succBlock->getNumber());
      analysisInfo->_currentExceptionGenSetInfo->add(pair);
      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, succBlock->getNumber());
      analysisInfo->_currentExceptionKillSetInfo->add(pair);
      }
   }


template<class Container>void TR_BackwardDFSetAnalysis<Container *>::initializeGenAndKillSetInfo(TR_RegionStructure *regionStructure, TR_BitVector &pendingList, TR_BitVector &exitNodes, bool lastIteration)
   {
   while (this->_analysisQueue.getListHead() &&
          (this->_analysisQueue.getListHead()->getData()->getStructure() != regionStructure))
      {
      if (!this->_analysisInterrupted)
	 {
         numIterations++;
         if ((numIterations % 20) == 0)
	    {
	    numIterations = 0;
            if (this->comp()->compilationShouldBeInterrupted(BBVA_INITIALIZE_CONTEXT))
               this->_analysisInterrupted = true;
	    }
	 }

      if (this->_analysisInterrupted)
         {
         TR::Compilation *comp = this->comp();
         comp->failCompilation<TR::CompilationInterrupted>("interrupted in backward bit vector analysis");
         }

      TR::CFGNode *node = this->_analysisQueue.getListHead()->getData();
      TR_StructureSubGraphNode *nodeStructure = (TR_StructureSubGraphNode *) node;

      if (!pendingList.get(nodeStructure->getStructure()->getNumber()) &&
         (*(this->_changedSetsQueue.getListHead()->getData()) == 0))
         {
         this->removeHeadFromAnalysisQueue();
         continue;
         }

      if (traceBBVA())
         traceMsg(this->comp(), "Begin analyzing node %p numbered %d\n", node, node->getNumber());

      bool alreadyVisitedNode = false;
      if (this->_nodesInCycle->get(nodeStructure->getNumber()))
         alreadyVisitedNode = true;

      this->_nodesInCycle->set(nodeStructure->getNumber());

      typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *analysisInfo = this->getAnalysisInfo(nodeStructure->getStructure());

      if (!exitNodes.get(node->getNumber()))
         {
         initializeCurrentGenKillSetInfo();
         }

      TR_BitVector seenSuccNodes(this->comp()->trMemory()->currentStackRegion());
      TR_BitVector seenNodes(this->comp()->trMemory()->currentStackRegion());
         {
         TR_SuccessorIterator successors(nodeStructure);
         bool firstSucc = true;
         bool stopAnalyzingThisNode = false;
         for (auto succ = successors.getFirst(); succ; succ = successors.getNext())
            {
            if (!regionStructure->isExitEdge(succ))
               {
               TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) succ->getTo();
               TR_Structure *succStructure = succNode->getStructure();
               if ((regionStructure->getEntry() == succNode) ||
                  exitNodes.get(succNode->getNumber()))
                  continue;

               if ((pendingList.get(succStructure->getNumber()) && (!alreadyVisitedNode)) /* &&
                                                                                                 (regionStructure->getEntry() != succNode)*/)
                  {
                  this->removeHeadFromAnalysisQueue();
                  this->addToAnalysisQueue(succNode, 0);
                  stopAnalyzingThisNode = true;
                  break;
                  }

               if (pendingList.get(succStructure->getNumber()) && !succStructure->hasBeenAnalyzedBefore()) //_firstIteration)
                  {
                  TR_ASSERT(0, "Should not reach here\n");
                  }
               else
                  {
                  seenSuccNodes.set(succStructure->getNumber());
                  Container *succBitVector = NULL;
                  typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *succInfo = this->getAnalysisInfo(succStructure);
                  }
               }
            firstSucc = false;
            }
         if (stopAnalyzingThisNode)
            continue;
         firstSucc = false;
         }


      bool checkForChange  = !regionStructure->isAcyclic();
      this->_nodesInCycle->empty();
      this->initializeGenAndKillSetInfoForStructure(nodeStructure->getStructure());
      pendingList.reset(node->getNumber());
      this->removeHeadFromAnalysisQueue();


      if (traceBBVA())
         {
         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
         if (analysisInfo->_regularGenSetInfo)
            {
            traceMsg(this->comp(), "\nGen Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_regularGenSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }

         if (analysisInfo->_regularKillSetInfo)
            {
            traceMsg(this->comp(), "\nKill Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_regularKillSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }

         if (analysisInfo->_exceptionGenSetInfo)
            {
            traceMsg(this->comp(), "\nException Gen Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_exceptionGenSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }

         if (analysisInfo->_exceptionKillSetInfo)
            {
            traceMsg(this->comp(), "\nException Kill Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_exceptionKillSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }
         }

      bool isBlockStructure = false;
      if (nodeStructure->getStructure()->asBlock())
         isBlockStructure = true;

      typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *regionAnalysisInfo = this->getAnalysisInfo(regionStructure);
      TR_SuccessorIterator successors(nodeStructure);
      int count = 0;
      for (auto succ = successors.getFirst(); succ; succ = successors.getNext())
         {
         int32_t nodeNumber = succ->getTo()->getNumber();
         bool normalSucc = (++count <= nodeStructure->getSuccessors().size());
         TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>
         *_killSetInfo = normalSucc ? analysisInfo->_regularKillSetInfo : analysisInfo->_exceptionKillSetInfo,
         *_regionKillSetInfo = normalSucc ? regionAnalysisInfo->_regularKillSetInfo : regionAnalysisInfo->_exceptionKillSetInfo,
         *_currentKillSetInfo = normalSucc ? analysisInfo->_currentRegularKillSetInfo : regionAnalysisInfo->_currentExceptionKillSetInfo,
         *_genSetInfo = normalSucc ? analysisInfo->_regularGenSetInfo : analysisInfo->_exceptionGenSetInfo,
         *_regionGenSetInfo = normalSucc ? regionAnalysisInfo->_regularGenSetInfo : regionAnalysisInfo->_exceptionGenSetInfo,
         *_currentGenSetInfo = normalSucc ? analysisInfo->_currentRegularGenSetInfo : regionAnalysisInfo->_currentExceptionGenSetInfo;

         Container *killBitVector =
            analysisInfo->getContainer(_killSetInfo, isBlockStructure ? node->getNumber() : nodeNumber);

         Container *genBitVector =
            analysisInfo->getContainer(_genSetInfo, isBlockStructure ? node->getNumber() : nodeNumber);

         if (!seenSuccNodes.get(nodeNumber))
            {
            this->_temp->empty();
            if (genBitVector)
               *this->_temp |= *genBitVector;

            this->_temp2->empty();
            if (killBitVector)
               *this->_temp2 |= *killBitVector;

            if (node == regionStructure->getEntry())
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair =
                  regionAnalysisInfo->getContainerNodeNumberPair(_regionGenSetInfo, nodeNumber);
               if (pair)
                  {
                  if (!pair->_container)
                     {
                     pair->_container = initializeInfo(NULL);
                     }
                  compose(pair->_container, this->_temp);
                  }

               pair = regionAnalysisInfo->getContainerNodeNumberPair(_regionKillSetInfo, nodeNumber);
               if (pair)
                  {
                  if (!pair->_container)
                     {
                     pair->_container = inverseInitializeInfo(NULL);
                     }
                  inverseCompose(pair->_container, this->_temp2);
                  }
               }
            else
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair =
                  analysisInfo->getContainerNodeNumberPair(_currentGenSetInfo, nodeNumber);
               if (!pair)
                  {
                  pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, nodeNumber);
                  _currentGenSetInfo->add(pair);
                  }

               if (!pair->_container)
                  {
                  pair->_container = initializeInfo(NULL);
                  }

               compose(pair->_container, this->_temp);

               pair = analysisInfo->getContainerNodeNumberPair(_currentKillSetInfo, nodeNumber);
               if (!pair)
                  {
                  pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, nodeNumber);
                  _currentKillSetInfo->add(pair);
                  }

               if (!pair->_container)
                  {
                  pair->_container = inverseInitializeInfo(NULL);
                  }

               inverseCompose(pair->_container, this->_temp2);
               }

            continue;
            }

         typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *succInfo = this->getAnalysisInfo(succ->getTo()->asStructureSubGraphNode()->getStructure());
         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *nodePair;
         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *killNodePair = succInfo->_currentRegularKillSetInfo->getFirst();
         for (nodePair = succInfo->_currentRegularGenSetInfo->getFirst(); nodePair; nodePair = nodePair->getNext(), killNodePair = killNodePair->getNext())
            {
            if (nodePair->_container)
               *this->_temp = *(nodePair->_container);
            else
               this->_temp->empty();

            if (killBitVector)
               *this->_temp -= *killBitVector;
            if (genBitVector)
               *this->_temp |= *genBitVector;

            if (killNodePair->_container)
               *this->_temp2 = *(killNodePair->_container);
            else
               this->_temp2->empty();

            if (genBitVector)
               *this->_temp2 -= *genBitVector;
            if (killBitVector)
               *this->_temp2 |= *killBitVector;

            if (node == regionStructure->getEntry())
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = regionAnalysisInfo->getContainerNodeNumberPair(regionAnalysisInfo->_regularGenSetInfo, nodePair->_nodeNumber);
               if (pair)
                  {
                  if (!pair->_container)
                     {
                     pair->_container = initializeInfo(NULL);
                     }
                  compose(pair->_container, this->_temp);
                  }

               pair = regionAnalysisInfo->getContainerNodeNumberPair(regionAnalysisInfo->_regularKillSetInfo, nodePair->_nodeNumber);
               if (pair)
                  {
                  if (!pair->_container)
                     {
                     pair->_container = inverseInitializeInfo(NULL);
                     }
                  inverseCompose(pair->_container, this->_temp2);
                  }
               }
            else
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = analysisInfo->getContainerNodeNumberPair(analysisInfo->_currentRegularGenSetInfo, nodePair->_nodeNumber);
               if (!pair)
                  {
                  pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, nodePair->_nodeNumber);
                  analysisInfo->_currentRegularGenSetInfo->add(pair);
                  }
               if (!pair->_container)
                  {
                  pair->_container = initializeInfo(NULL);
                  }
               compose(pair->_container, this->_temp);

               pair = analysisInfo->getContainerNodeNumberPair(analysisInfo->_currentRegularKillSetInfo, nodePair->_nodeNumber);
               if (!pair)
                  {
                  pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, nodePair->_nodeNumber);
                  analysisInfo->_currentRegularKillSetInfo->add(pair);
                  }
               if (!pair->_container)
                  {
                  pair->_container = inverseInitializeInfo(NULL);
                  }
               inverseCompose(pair->_container, this->_temp2);
               }
            }

         killNodePair = succInfo->_currentExceptionKillSetInfo->getFirst();
         for (nodePair = succInfo->_currentExceptionGenSetInfo->getFirst(); nodePair; nodePair = nodePair->getNext(), killNodePair = killNodePair->getNext())
            {
            if (nodePair->_container)
               *this->_temp = *(nodePair->_container);
            else
               this->_temp->empty();
            if (killBitVector)
               *this->_temp -= *killBitVector;
            if (genBitVector)
               *this->_temp |= *genBitVector;

            if (killNodePair->_container)
               *this->_temp2 = *(killNodePair->_container);
            else
               this->_temp2->empty();

            if (genBitVector)
               *this->_temp2 -= *genBitVector;
            if (killBitVector)
               *this->_temp2 |= *killBitVector;

            if (node == regionStructure->getEntry())
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = regionAnalysisInfo->getContainerNodeNumberPair(regionAnalysisInfo->_exceptionGenSetInfo, nodePair->_nodeNumber);
               if (pair)
                  {
                  if (!pair->_container)
                     {
                     pair->_container = initializeInfo(NULL);
                     }
                  compose(pair->_container, this->_temp);
                  }

               pair = regionAnalysisInfo->getContainerNodeNumberPair(regionAnalysisInfo->_exceptionKillSetInfo, nodePair->_nodeNumber);
               if (pair)
                  {
                  if (!pair->_container)
                     {
                     pair->_container = inverseInitializeInfo(NULL);
                     }
                  inverseCompose(pair->_container, this->_temp2);
                  }
               }
            else
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = analysisInfo->getContainerNodeNumberPair(analysisInfo->_currentExceptionGenSetInfo, nodePair->_nodeNumber);
               if (!pair)
                  {
                  pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, nodePair->_nodeNumber);
                  analysisInfo->_currentExceptionGenSetInfo->add(pair);
                  }
               if (!pair->_container)
                  {
                  pair->_container = initializeInfo(NULL);
                  }
               compose(pair->_container, this->_temp);


               pair = analysisInfo->getContainerNodeNumberPair(analysisInfo->_currentExceptionKillSetInfo, nodePair->_nodeNumber);
               if (!pair)
                  {
                  pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(NULL, nodePair->_nodeNumber);
                  analysisInfo->_currentExceptionKillSetInfo->add(pair);
                  }
               if (!pair->_container)
                  {
                  pair->_container = inverseInitializeInfo(NULL);
                  }
               inverseCompose(pair->_container, this->_temp2);
               }
            }
         }


      if (traceBBVA())
         {
         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
         if (analysisInfo->_currentRegularGenSetInfo)
            {
            traceMsg(this->comp(), "\nCurrent Gen Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_currentRegularGenSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }

         if (analysisInfo->_currentRegularKillSetInfo)
            {
            traceMsg(this->comp(), "\nCurrent Kill Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_currentRegularKillSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }

         if (analysisInfo->_currentExceptionGenSetInfo)
            {
            traceMsg(this->comp(), "\nCurrent Gen Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_currentExceptionGenSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }

         if (analysisInfo->_currentExceptionKillSetInfo)
            {
            traceMsg(this->comp(), "\nCurrent Kill Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
            for (pair = analysisInfo->_currentExceptionKillSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               if (pair->_container)
                  {
                  traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
                  pair->_container->print(this->comp());
                  traceMsg(this->comp(), "\n");
                  }
               }
            traceMsg(this->comp(), "\n");
            }
         }


      TR_PredecessorIterator predecessors(nodeStructure);
      for (auto pred = predecessors.getFirst(); pred; pred = predecessors.getNext())
         {
         TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) pred->getFrom();
         TR_Structure *predStructure = predNode->getStructure();

         if (pendingList.get(predStructure->getNumber()))
            {
            //this->_compilation->incVisitCount();
            this->_nodesInCycle->empty();
            this->addToAnalysisQueue(toStructureSubGraphNode(predNode), 1);
            }
         }
      }
   }



template<class Container>void TR_BackwardDFSetAnalysis<Container *>::initializeGenAndKillSetInfoForRegion(TR_RegionStructure *region)
   {
   typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *analysisInfo = this->getAnalysisInfo(region);
   if (!region->hasBeenAnalyzedBefore())
       region->setAnalyzedStatus(true);
   else
      return;

   TR_BitVector exitNodes(this->comp()->trMemory()->currentStackRegion());
   TR_BitVector toNodes(this->comp()->trMemory()->currentStackRegion());
   TR_StructureSubGraphNode *subNode;
   TR::CFGEdge *edge;

   analysisInfo->_regularGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_regularKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_exceptionGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_exceptionKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentRegularGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentRegularKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentExceptionGenSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;
   analysisInfo->_currentExceptionKillSetInfo = new (this->trStackMemory())TR_LinkHead<typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair>;

   ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
   for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
      {
      int32_t fromStructureNumber = edge->getFrom()->getNumber();
      int32_t toStructureNumber = edge->getTo()->getNumber();
      if (!toNodes.get(toStructureNumber))
         {
         Container *b = NULL; //;

         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_regularGenSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_regularKillSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_exceptionGenSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_exceptionKillSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_currentRegularGenSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_currentRegularKillSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_currentExceptionGenSetInfo->add(pair);

         pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
         analysisInfo->_currentExceptionKillSetInfo->add(pair);

         toNodes.set(toStructureNumber);
         }
      if (!exitNodes.get(toStructureNumber))
         exitNodes.set(toStructureNumber);
      }

   TR_RegionStructure::Cursor si(*region);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getSuccessors().empty() && subNode->getExceptionSuccessors().empty())
         {
         int32_t fromStructureNumber = subNode->getNumber();
         if (!exitNodes.get(fromStructureNumber))
            {
            Container *b = NULL;
            // allocateContainer(&b);

            typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_regularGenSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_regularKillSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_exceptionGenSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_exceptionKillSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_currentRegularGenSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_currentRegularKillSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_currentExceptionGenSetInfo->add(pair);

            pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, fromStructureNumber);
            analysisInfo->_currentExceptionKillSetInfo->add(pair);
            }
         exitNodes.set(subNode->getNumber());
         }
      }

   if (region->isNaturalLoop())
      {
      int32_t toStructureNumber = region->getNumber();

      Container *b = NULL;

      typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_regularGenSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_regularKillSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_exceptionGenSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_exceptionKillSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_currentRegularGenSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_currentRegularKillSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_currentExceptionGenSetInfo->add(pair);

      pair = new (this->trStackMemory()) typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair(b, toStructureNumber);
      analysisInfo->_currentExceptionKillSetInfo->add(pair);
      }


   TR_BitVector pendingList(this->comp()->trMemory()->currentStackRegion());

   si.reset();
   for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
      {
      pendingList.set(subNode->getNumber());
      }

   bool changed = true;
   int32_t numIterations = 1;
   this->_firstIteration = true;

   while (changed)
      {
      this->_nodesInCycle->empty();
      changed = false;

      if (traceBBVA())
         traceMsg(this->comp(), "\nREGION : %p NUMBER : %d ITERATION NUMBER : %d\n", region, region->getNumber(), numIterations);

      numIterations++;

      ei.reset();
      for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
         {
         TR_StructureSubGraphNode *edgeFrom = toStructureSubGraphNode(edge->getFrom());
         this->addToAnalysisQueue(edgeFrom, 0);
         initializeGenAndKillSetInfo(region, pendingList, exitNodes, !changed);
         }

      bool noExitEdges = region->getExitEdges().isEmpty();
      si.reset();
      for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
         {
         if (noExitEdges || (subNode->getSuccessors().empty() && subNode->getExceptionSuccessors().empty()))
            {
            this->addToAnalysisQueue(subNode, 0);
            initializeGenAndKillSetInfo(region, pendingList, exitNodes, !changed);
            }
         }

      this->_firstIteration = false;
      }
   }

template<class Container>bool TR_BackwardDFSetAnalysis<Container *>::analyzeRegionStructure(TR_RegionStructure *regionStructure, bool checkForChange)
   {
   typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *analysisInfo = NULL;
   if (regionStructure == this->_cfg->getStructure())
      analysisInfo = this->getAnalysisInfo(regionStructure);

   // Use information from last time we analyzed this structure; if
   // analyzing for the first time, initialize information
   //
   if (!regionStructure->hasBeenAnalyzedBefore())
      regionStructure->setAnalyzedStatus(true);
   else
      {
      analysisInfo = this->getAnalysisInfo(regionStructure);

      bool sameAsPreviousIteration = true;
      typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
      for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
         {
         if (!(*_currentOutSetInfo[pair->_nodeNumber] == *(pair->_container)))
            {
            sameAsPreviousIteration = false;
            break;
            }
         }

      if (sameAsPreviousIteration)
         {
         if (traceBBVA())
            {
            traceMsg(this->comp(), "\nSkipping re-analysis of Region : %p numbered %d\n", regionStructure, regionStructure->getNumber());
            }
         return false;
         }
      }

   if (!analysisInfo)
      analysisInfo = this->getAnalysisInfo(regionStructure);

   TR_BitVector exitNodes(this->comp()->trMemory()->currentStackRegion());
   TR_RegionStructure::Cursor si(*regionStructure);
   ListIterator<TR::CFGEdge> ei(&regionStructure->getExitEdges());
   TR_StructureSubGraphNode *subNode;
   TR::CFGEdge *edge;

   //
   // Copy the current out set for comparison the next time we analyze this region
   //
   //
   ei.reset();
   for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
      {
      TR_StructureSubGraphNode *edgeFrom = toStructureSubGraphNode(edge->getFrom());
      int32_t fromStructureNumber = edgeFrom->getStructure()->getNumber();
      int32_t toStructureNumber = edge->getTo()->getNumber();
      Container *outBitVector = analysisInfo->getContainer(analysisInfo->_outSetInfo, toStructureNumber);

      if (outBitVector != NULL)
         this->copyFromInto(_currentOutSetInfo[toStructureNumber], outBitVector);
      exitNodes.set(fromStructureNumber);
      }

   si.reset();
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getSuccessors().empty() && subNode->getExceptionSuccessors().empty())
         exitNodes.set(subNode->getNumber());
      }

   TR_BitVector pendingList(this->comp()->trMemory()->currentStackRegion());
   si.reset();
   for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
      pendingList.set(subNode->getNumber());

   bool changed = true;
   int32_t numIterations = 1;
   this->_firstIteration = true;

   while (changed)
      {
      this->_nodesInCycle->empty();
      changed = false;

      if (traceBBVA())
         traceMsg(this->comp(), "\nREGION : %p NUMBER : %d ITERATION NUMBER : %d\n", regionStructure, regionStructure->getNumber(), numIterations);

      numIterations++;

      ei.reset();
      for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
         {
         TR_StructureSubGraphNode *edgeFrom = toStructureSubGraphNode(edge->getFrom());
         this->addToAnalysisQueue(edgeFrom, 0);
         if (analyzeNodeIfSuccessorsAnalyzed(regionStructure, pendingList, exitNodes))
            {
            if (!this->supportsGenAndKillSets() ||
                !canGenAndKillForStructure(regionStructure))
               changed = true;
            }
         }

      bool noExitEdges = regionStructure->getExitEdges().isEmpty();
      si.reset();
      for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
         {
         if (noExitEdges || (subNode->getSuccessors().empty() && subNode->getExceptionSuccessors().empty()))
            {
            this->addToAnalysisQueue(subNode, 0);
            if (analyzeNodeIfSuccessorsAnalyzed(regionStructure, pendingList, exitNodes))
               {
               if (!this->supportsGenAndKillSets() ||
                   !canGenAndKillForStructure(regionStructure))
                  changed = true;
               }
            }
         }

      this->_firstIteration = false;
      }

   Container *inSetInfo = this->getAnalysisInfo(regionStructure->getEntry()->getStructure())->_inSetInfo;
   if (checkForChange && !(*inSetInfo == *analysisInfo->_inSetInfo))
      changed = true;

   if (this->supportsGenAndKillSets() &&
       (canGenAndKillForStructure(regionStructure) &&
        regionStructure != this->_cfg->getStructure()))
      {
      if (!(*inSetInfo == *analysisInfo->_inSetInfo))
         TR_ASSERT(0, "This should not happen\n");
      }

   this->copyFromInto(inSetInfo, analysisInfo->_inSetInfo);
   return changed;
   }






template<class Container>bool TR_BackwardDFSetAnalysis<Container *>::analyzeNodeIfSuccessorsAnalyzed(TR_RegionStructure *regionStructure, TR_BitVector &pendingList, TR_BitVector &exitNodes)
   {
   bool anyNodeChanged = false;

   while (this->_analysisQueue.getListHead() &&
      (this->_analysisQueue.getListHead()->getData()->getStructure() != regionStructure))
      {
      if (!this->_analysisInterrupted)
	 {
         numIterations++;
         if ((numIterations % 20) == 0)
	    {
	    numIterations = 0;
            if (this->comp()->compilationShouldBeInterrupted(BBVA_ANALYZE_CONTEXT))
               this->_analysisInterrupted = true;
	    }
	 }

      if (this->_analysisInterrupted)
         {
         TR::Compilation *comp = this->comp();
         comp->failCompilation<TR::CompilationInterrupted>("interrupted in backward bit vector analysis");
         }

      TR::CFGNode *node = this->_analysisQueue.getListHead()->getData();
      TR_StructureSubGraphNode *nodeStructure = (TR_StructureSubGraphNode *) node;

      if (!pendingList.get(nodeStructure->getStructure()->getNumber()) &&
          (*(this->_changedSetsQueue.getListHead()->getData()) == 0))
         {
         this->removeHeadFromAnalysisQueue();
         continue;
         }

      if (traceBBVA())
         traceMsg(this->comp(), "Begin analyzing node %p numbered %d\n", node, node->getNumber());


      bool alreadyVisitedNode = false;
      if (this->_nodesInCycle->get(nodeStructure->getNumber()))
         alreadyVisitedNode = true;

      this->_nodesInCycle->set(nodeStructure->getNumber());
      typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *analysisInfo = this->getAnalysisInfo(nodeStructure->getStructure());

      TR_BitVector seenNodes(this->comp()->trMemory()->currentStackRegion());
      TR_SuccessorIterator successors(nodeStructure);
      bool firstSucc = true;
      bool stopAnalyzingThisNode = false;
      for (auto succ = successors.getFirst(); succ; succ = successors.getNext())
         {
         if (!regionStructure->isExitEdge(succ))
            {
            TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) succ->getTo();
            TR_Structure *succStructure = succNode->getStructure();
            if (pendingList.get(succStructure->getNumber()) && (!alreadyVisitedNode))
               {
               this->removeHeadFromAnalysisQueue();
               this->addToAnalysisQueue(succNode, 0);
               stopAnalyzingThisNode = true;
               break;
               }

            if (pendingList.get(succStructure->getNumber()) && !succStructure->hasBeenAnalyzedBefore()) //this->_firstIteration)
               {
               if (firstSucc)
                  initializeInfo(_currentOutSetInfo[succStructure->getNumber()]);
               }
            else
               {
               if (!seenNodes.get(succStructure->getNumber()))
                  initializeInfo(_currentOutSetInfo[succStructure->getNumber()]);
               Container *succinfo = this->getAnalysisInfo(succStructure)->_inSetInfo;
               compose(_currentOutSetInfo[succStructure->getNumber()], succinfo);
               }
            seenNodes.set(succStructure->getNumber());
            }
         firstSucc = false;
         }

      if (stopAnalyzingThisNode)
         continue;

      firstSucc = false;

      if (exitNodes.get(nodeStructure->getNumber()))
         {
         if (!(nodeStructure->getSuccessors().empty() && nodeStructure->getExceptionSuccessors().empty()))
            {
            typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *regionAnalysisInfo = this->getAnalysisInfo(regionStructure);
            typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
            for (pair = regionAnalysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *otherPair;
               for (otherPair = analysisInfo->_outSetInfo->getFirst(); otherPair; otherPair = otherPair->getNext())
                  {
                  if (pair->_nodeNumber == otherPair->_nodeNumber)
                     {
                     compose(_currentOutSetInfo[pair->_nodeNumber], pair->_container);
                     break;
                     }
                  }
               }
            }
         else
            {
            typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
            for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               this->copyFromInto(_originalOutSetInfo[pair->_nodeNumber], _currentOutSetInfo[pair->_nodeNumber]);
               }
            }
         }

     typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *regionAnalysisInfo = this->getAnalysisInfo(regionStructure);

     if (this->supportsGenAndKillSets() &&
         canGenAndKillForStructure(nodeStructure->getStructure()))
        {
        if (nodeStructure->getStructure()->asRegion() &&
            nodeStructure->getStructure()->asRegion()->isNaturalLoop())
           {
           TR_BitVector backEdgeNodes(this->comp()->trMemory()->currentStackRegion());
           ListIterator<TR::CFGEdge> exitIt(&nodeStructure->getStructure()->asRegion()->getExitEdges());
           TR::CFGEdge *exitEdge;
           for (exitEdge = exitIt.getCurrent(); exitEdge != NULL; exitEdge = exitIt.getNext())
              {
              TR_StructureSubGraphNode *edgeFrom = toStructureSubGraphNode(exitEdge->getFrom());
              int32_t toStructureNumber = exitEdge->getTo()->getNumber();

              if (edgeFrom->hasSuccessor(nodeStructure->getStructure()->asRegion()->getEntry()))
                 {
                 if (edgeFrom->hasSuccessor(exitEdge->getTo()))
                    backEdgeNodes.set(toStructureNumber);
                 }
              else if (edgeFrom->hasExceptionSuccessor(nodeStructure->getStructure()->asRegion()->getEntry()))
                 {
                 if (edgeFrom->hasExceptionSuccessor(exitEdge->getTo()))
                    backEdgeNodes.set(toStructureNumber);
                 }
              }

           Container *killBitVector = analysisInfo->getContainer(analysisInfo->_regularKillSetInfo, node->getNumber());
           Container *genBitVector = analysisInfo->getContainer(analysisInfo->_regularGenSetInfo, node->getNumber());
           Container *exceptionKillBitVector = analysisInfo->getContainer(analysisInfo->_exceptionKillSetInfo, node->getNumber());
           Container *exceptionGenBitVector = analysisInfo->getContainer(analysisInfo->_exceptionGenSetInfo, node->getNumber());

           typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *nodePair;
           for (nodePair = analysisInfo->_regularGenSetInfo->getFirst(); nodePair; nodePair = nodePair->getNext())
              {
              if (!backEdgeNodes.get(nodePair->_nodeNumber))
                 continue;

              *this->_temp = *(_currentOutSetInfo[nodePair->_nodeNumber]);
              if (killBitVector)
                 *this->_temp -= *killBitVector;

              if (exceptionKillBitVector)
                 *this->_temp -= *exceptionKillBitVector;

              if (genBitVector)
                 *this->_temp |= *genBitVector;

              if (exceptionGenBitVector)
                 *this->_temp |= *exceptionGenBitVector;
              compose(_currentOutSetInfo[nodePair->_nodeNumber], this->_temp);
              }

           for (nodePair = analysisInfo->_exceptionGenSetInfo->getFirst(); nodePair; nodePair = nodePair->getNext())
              {
              if (!backEdgeNodes.get(nodePair->_nodeNumber))
                 continue;

              *this->_temp = *(_currentOutSetInfo[nodePair->_nodeNumber]);
              if (killBitVector)
                 *this->_temp -= *killBitVector;

              if (exceptionKillBitVector)
                 *this->_temp -= *exceptionKillBitVector;

              if (genBitVector)
                 *this->_temp |= *genBitVector;

              if (exceptionGenBitVector)
                 *this->_temp |= *exceptionGenBitVector;
              compose(_currentOutSetInfo[nodePair->_nodeNumber], this->_temp);
              }
           }
        }

      bool checkForChange  = !regionStructure->isAcyclic();
      bool inSetChanged = false;

     if (this->supportsGenAndKillSets() &&
         canGenAndKillForStructure(nodeStructure->getStructure()))
        {
        bool isBlockStructure = false;
        if (nodeStructure->getStructure()->asBlock())
           isBlockStructure = true;

        if (checkForChange)
           *this->_temp = *(analysisInfo->_inSetInfo);

        bool firstSucc = true;
        typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *nodePair;

        TR_SuccessorIterator successors(nodeStructure);
        int count = 0;
        for (auto succ = successors.getFirst(); succ; succ = successors.getNext())
           {
           bool normalSucc = (++count <= nodeStructure->getSuccessors().size());
           int32_t nodeNumber = isBlockStructure ? node->getNumber() : succ->getTo()->getNumber();
           Container *killBitVector = analysisInfo->getContainer(
                 normalSucc
                 ? analysisInfo->_regularKillSetInfo
                 : analysisInfo->_exceptionKillSetInfo,
                 nodeNumber);
           Container *genBitVector = analysisInfo->getContainer(
                 normalSucc
                 ? analysisInfo->_regularGenSetInfo
                 : analysisInfo->_exceptionGenSetInfo,
                 nodeNumber);

           *this->_regularInfo = *(_currentOutSetInfo[nodeNumber]);

           if (killBitVector)
              *this->_regularInfo -= *killBitVector;

           if (genBitVector)
              *this->_regularInfo |= *genBitVector;

           if (!firstSucc)
              compose(analysisInfo->_inSetInfo, this->_regularInfo);
           else
              this->copyFromInto(this->_regularInfo, analysisInfo->_inSetInfo);
           firstSucc = false;
           }

        if (isBlockStructure && nodeStructure->getSuccessors().empty() &&
            nodeStructure->getExceptionSuccessors().empty())
           {
           Container *genBitVector = analysisInfo->getContainer(analysisInfo->_regularGenSetInfo, node->getNumber());
           this->copyFromInto(genBitVector, analysisInfo->_inSetInfo);
           }

       if (nodeStructure->getStructure()->asRegion() &&
           nodeStructure->getStructure()->asRegion()->isNaturalLoop())
          {
          ListIterator<TR::CFGEdge> exitIt(&nodeStructure->getStructure()->asRegion()->getExitEdges());
          TR::CFGEdge *exitEdge;
          for (exitEdge = exitIt.getCurrent(); exitEdge != NULL; exitEdge = exitIt.getNext())
              {
              int32_t toStructureNumber = exitEdge->getTo()->getNumber();
              compose(_currentOutSetInfo[toStructureNumber], analysisInfo->_inSetInfo);
              }
          }

        if (checkForChange && !(*analysisInfo->_inSetInfo == *this->_temp))
           inSetChanged = true;
        }

     if (traceBBVA())
         {
         traceMsg(this->comp(), "\nBEFORE Out Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
         for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
            {
            traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
            pair->_container->print(this->comp());
            _currentOutSetInfo[pair->_nodeNumber]->print(this->comp());
            traceMsg(this->comp(), "\n");
            }
         traceMsg(this->comp(), "\nBEFORE In Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
         analysisInfo->_inSetInfo->print(this->comp());
         traceMsg(this->comp(), "\n");
         }

      this->_nodesInCycle->empty();
      bool b = nodeStructure->getStructure()->doDataFlowAnalysis(this, checkForChange);
      if (b || pendingList.get(node->getNumber()))
         inSetChanged = true;
      pendingList.reset(node->getNumber());
      this->removeHeadFromAnalysisQueue();

      if (traceBBVA())
         {
         traceMsg(this->comp(), "\nOut Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
         typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
         for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
            {
            traceMsg(this->comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
            pair->_container->print(this->comp());
            traceMsg(this->comp(), "\n");
            }
         traceMsg(this->comp(), "\nIn Set Info for Region or Block : %p numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
         analysisInfo->_inSetInfo->print(this->comp());
         traceMsg(this->comp(), "\n");


         traceMsg(this->comp(), "\nOut Set Info for parent region : %p numbered %d is : \n", regionStructure, regionStructure->getNumber());
         for (pair = regionAnalysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
            {
            traceMsg(this->comp(), "Succ numbered %d : ", pair->_nodeNumber);
            pair->_container->print(this->comp());
            traceMsg(this->comp(), "\n");
            }
         traceMsg(this->comp(), "\nIn Set Info for parent region : %p numbered %d is : \n", regionStructure, regionStructure->getNumber());
         regionAnalysisInfo->_inSetInfo->print(this->comp());
         traceMsg(this->comp(), "\n");
         }

      bool changed = inSetChanged;

      bool needToIterate = false;
      if (!this->supportsGenAndKillSets() ||
          !canGenAndKillForStructure(regionStructure))
         needToIterate = true;

      if (this->supportsGenAndKillSets() &&
         canGenAndKillForStructure(nodeStructure->getStructure()))
        {
        if (exitNodes.get(nodeStructure->getNumber()))
            {
            if (!(nodeStructure->getSuccessors().empty() && nodeStructure->getExceptionSuccessors().empty()))
               {
               typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *regionAnalysisInfo = this->getAnalysisInfo(regionStructure);
               typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
               for (pair = regionAnalysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
                  {
                  typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *otherPair;
                  for (otherPair = analysisInfo->_outSetInfo->getFirst(); otherPair; otherPair = otherPair->getNext())
                     {
                     if (pair->_nodeNumber == otherPair->_nodeNumber)
                        {
                        this->copyFromInto(pair->_container, _currentOutSetInfo[pair->_nodeNumber]);
                        break;
                        }
                     }
                  }
               }
            }
         else
            {
            typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
            for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
               {
               this->copyFromInto(_originalOutSetInfo[pair->_nodeNumber], _currentOutSetInfo[pair->_nodeNumber]);
               }
            }
        }

      TR_PredecessorIterator predecessors(nodeStructure);
      for (auto pred = predecessors.getFirst(); pred; pred = predecessors.getNext())
         {
         TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) pred->getFrom();
         TR_Structure *predStructure = predNode->getStructure();
         if ((needToIterate || (node != regionStructure->getEntry())) &&
            (pendingList.get(predStructure->getNumber()) || inSetChanged))
            {
            this->_nodesInCycle->empty();
            this->addToAnalysisQueue(toStructureSubGraphNode(predNode), inSetChanged ? 1 : 0);
            }
         }
      if (changed)
         anyNodeChanged = true;
      }
   return anyNodeChanged;
   }

template<class Container>bool TR_BackwardDFSetAnalysis<Container *>::analyzeBlockStructure(TR_BlockStructure *blockStructure, bool checkForChange)
   {
   initializeInfo(this->_regularInfo);
   initializeInfo(this->_exceptionInfo);

   // Use information from last time we analyzed this structure; if
   // analyzing for the first time, initialize information
   //
   typename TR_BasicDFSetAnalysis<Container *>::ExtraAnalysisInfo *analysisInfo = NULL; //this->getAnalysisInfo(blockStructure);
   if (blockStructure->hasBeenAnalyzedBefore())
      {
      analysisInfo = this->getAnalysisInfo(blockStructure);
      bool sameOutSets = true;
      typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
      for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
         {
         if (!(*(_currentOutSetInfo[pair->_nodeNumber]) == *(pair->_container)))
            {
            sameOutSets = false;
            break;
            }
         }

      if (sameOutSets)
         {
         if (traceBBVA())
            {
            traceMsg(this->comp(), "\nSkipping re-analysis of Block : %p numbered %d\n", blockStructure, blockStructure->getNumber());
            }
         return false;
         }
      }

   if (!analysisInfo)
      analysisInfo = this->getAnalysisInfo(blockStructure);

   // Copy the current in set for comparison the next time we analyze this block
   //
    typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
    for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
       {
       this->copyFromInto(_currentOutSetInfo[pair->_nodeNumber], pair->_container);
       }


   TR::Block *block = blockStructure->getBlock();
   int32_t blockNum = block->getNumber();
   if (block == this->_cfg->getEnd())
      {
      this->copyFromInto(_originalOutSetInfo[blockNum], this->_regularInfo);
      this->copyFromInto(_originalOutSetInfo[blockNum], this->_exceptionInfo);
      }
   else
      {
      TR::CFGNode *succBlock;
      for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
         {
         succBlock = (*succ)->getTo();
         compose(this->_regularInfo, _currentOutSetInfo[succBlock->getNumber()]);
         }

      for (auto succ = block->getExceptionSuccessors().begin(); succ != block->getExceptionSuccessors().end(); ++succ)
         {
         succBlock = (*succ)->getTo();
         compose(this->_exceptionInfo, _currentOutSetInfo[succBlock->getNumber()]);
         }
      }

   bool changed = false;

   if (blockNum != 0)
      {
      if (this->_regularGenSetInfo)
         {
         if (this->_regularKillSetInfo[blockNum])
            *this->_regularInfo -= *this->_regularKillSetInfo[blockNum];

         if (traceBBVA())
            {
            dumpOptDetails(this->comp(), "Normal info for %d : ", blockNum);
            this->_regularInfo->print(this->comp());
            dumpOptDetails(this->comp(), "\n");
            }

         if (this->_regularGenSetInfo[blockNum])
            *this->_regularInfo |= *this->_regularGenSetInfo[blockNum];

         if (traceBBVA())
            {
            dumpOptDetails(this->comp(), "Normal info for %d : ", blockNum);
            this->_regularInfo->print(this->comp());
            dumpOptDetails(this->comp(), "\n");
            }

         if (this->_exceptionKillSetInfo[blockNum])
            *this->_exceptionInfo -= *this->_exceptionKillSetInfo[blockNum];
         if (this->_exceptionGenSetInfo[blockNum])
            *this->_exceptionInfo |= *this->_exceptionGenSetInfo[blockNum];
         compose(this->_regularInfo, this->_exceptionInfo);

         if (traceBBVA())
            {
            dumpOptDetails(this->comp(), "Normal info for %d : ", blockNum);
            this->_regularInfo->print(this->comp());
            dumpOptDetails(this->comp(), "\n");
            }
         }
      else
         {
         analyzeTreeTopsInBlockStructure(blockStructure);
         analysisInfo->_containsExceptionTreeTop = this->_containsExceptionTreeTop;
         }

      if (checkForChange && !(*analysisInfo->_inSetInfo == *this->_regularInfo))
         changed = true;

      if (this->supportsGenAndKillSets() &&
          canGenAndKillForStructure(blockStructure))
         {
         if (!(*this->_regularInfo == *analysisInfo->_inSetInfo))
            TR_ASSERT(0, "This should not happen for a block\n");
         }

      *analysisInfo->_inSetInfo = *this->_regularInfo;
      if (!this->_blockAnalysisInfo[blockStructure->getNumber()])
         this->allocateBlockInfoContainer(&this->_blockAnalysisInfo[blockStructure->getNumber()], this->_regularInfo);
      this->copyFromInto(this->_regularInfo, this->_blockAnalysisInfo[blockStructure->getNumber()]);
      }


   if (traceBBVA())
      {
      traceMsg(this->comp(), "\nOut Set Info for Block : %p numbered %d is : \n", blockStructure, blockStructure->getNumber());
      typename TR_BasicDFSetAnalysis<Container *>::TR_ContainerNodeNumberPair *pair;
      for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
         {
         traceMsg(this->comp(), "Succ numbered %d : ", pair->_nodeNumber);
         pair->_container->print(this->comp());
         traceMsg(this->comp(), "\n");
         }
      traceMsg(this->comp(), "\nIn Set Info for Block : %p numbered %d is : \n", blockStructure, blockStructure->getNumber());
      analysisInfo->_inSetInfo->print(this->comp());
      traceMsg(this->comp(), "\n");
      }

   blockStructure->setAnalyzedStatus(true);
   return changed;
   }




template<class Container>void TR_BackwardDFSetAnalysis<Container *>::analyzeNode(TR::Node *node, vcount_t visitCount, TR_BlockStructure *blockStructure, Container *_analysisInfo)
   {
   }





template<class Container>void TR_BackwardDFSetAnalysis<Container *>::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {
   TR::Block *block = blockStructure->getBlock();
   TR::TreeTop *currentTree = block->getExit();
   TR::TreeTop *entryTree = block->getEntry();
   vcount_t visitCount = this->comp()->incVisitCount();
   this->_containsExceptionTreeTop = false;
   while (!(currentTree == entryTree))
      {
      bool currentTreeHasChecks = this->treeHasChecks(currentTree);
      if (currentTreeHasChecks)
         compose(this->_regularInfo, this->_exceptionInfo);
      analyzeNode(currentTree->getNode(), visitCount, blockStructure, this->_regularInfo);
      if (!(currentTree == entryTree))
         currentTree = currentTree->getPrevTreeTop();
      }
   }


template<class Container>void TR_BackwardDFSetAnalysis<Container *>::compose(Container *, Container *)
   {
   }

template<class Container>TR_DataFlowAnalysis::Kind TR_BackwardDFSetAnalysis<Container *>::getKind()
   {
   return TR_DataFlowAnalysis::BackwardDFSetAnalysis;
   }

template class TR_BackwardDFSetAnalysis<TR_BitVector *>;
template class TR_BackwardDFSetAnalysis<TR_SingleBitContainer *>;
