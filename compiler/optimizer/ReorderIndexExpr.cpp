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

#include "optimizer/ReorderIndexExpr.hpp"

#include <stdio.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/InductionVariable.hpp"

#define OPT_DETAILS "O^O ARRAY INDEX EXPRESSION MANIPULATION: "

/**
 * For non-trivial array index expressions, it's important to regroup them so that as much of the
 * array invarient portion is in a subtree as possible so PRE hoists it
 * e.g.
 *
 *   for (i=0;i < endI;++i)
 *     for (j=0; j < endJ; ++j)
 *       a[X + j*sizeJ + Y + i*sizeI]++;
 *
 * could have the array expression rewritten through reassociation as
 *
 *       a[(j*sizeJ) + (X + Y +i*sizeI)]++
 *
 * as long as overflow isn't an issue
 */
TR_IndexExprManipulator::TR_IndexExprManipulator(TR::OptimizationManager *manager)
   : TR::Optimization(manager),_somethingChanged(false)
   {
   _debug=false;
   }

int32_t TR_IndexExprManipulator::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t cost = 0;

   static char *doit= feGetEnv("TR_NOMODINDEXEXPR");
   if (doit != NULL) return 0;

   if (false)printf("called for %s\n",comp()->signature());
   //TR::TreeTop *treeTop;
   //TR::Block   *block;

   _visitCount = comp()->incVisitCount();


   rewriteIndexExpression(comp()->getFlowGraph()->getStructure());


   if (_somethingChanged)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      requestOpt(OMR::partialRedundancyElimination);
      ++cost;
      }

   return cost;
   }

/*
 * Find loops and
 */
void
TR_IndexExprManipulator::rewriteIndexExpression(TR_Structure *loopStructure)
   {
   TR_RegionStructure * regionStructure = loopStructure->asRegion();
   if (regionStructure)
      {

      TR_StructureSubGraphNode *graphNode;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (graphNode = si.getCurrent(); graphNode != 0; graphNode = si.getNext())
         rewriteIndexExpression(graphNode->getStructure());
      }


   if (!regionStructure ||
       !regionStructure->getParent() ||
       !regionStructure->isNaturalLoop())
      return;

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   regionStructure->getBlocks(&blocksInRegion);

   if (_debug) traceMsg(comp(), "XX looking at region %d\n",regionStructure->getNumber());
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;
   TR::TreeTop /**first,*/*last,*curTree;
   TR_PrimaryInductionVariable *primeIV;
   primeIV = regionStructure->getPrimaryInductionVariable();

   if (NULL == primeIV) return;;

     _visitCount = comp()->incOrResetVisitCount();

   if (_debug)traceMsg(comp(), "Loop: %d primeIV:%p\n",regionStructure->getNumber(),primeIV);
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      curTree= nextBlock->getFirstRealTreeTop();
      last = nextBlock->getLastRealTreeTop();
      while(curTree)
         {
         TR::Node * node=curTree->getNode();

         if (node->getOpCode().isStoreIndirect())
           rewriteIndexExpression(primeIV,NULL,node,false);

         if (curTree == last) break;
         curTree = curTree->getNextTreeTop();
         }
      }
   }

void
TR_IndexExprManipulator::rewriteIndexExpression(TR_PrimaryInductionVariable *primeIV,TR::Node *parentNode,TR::Node *node,bool parentIsAiadd)
   {
   if (node->getVisitCount() == _visitCount) return;

   node->setVisitCount(_visitCount);

   parentIsAiadd = parentIsAiadd || node->getOpCode().isArrayRef();

   for (int32_t i=0;i < node->getNumChildren();++i)
      {
      TR::Node * childNode=node->getChild(i);
      rewriteIndexExpression(primeIV,node,childNode,parentIsAiadd);

      if (_debug)  traceMsg(comp(), "traced %p %s\n",childNode,parentIsAiadd?"(arrayRef)":"");
      if (parentIsAiadd)
         {
         if (childNode->getOpCode().hasSymbolReference() && childNode->getSymbol() == primeIV->getSymRef()->getSymbol())
            {
            if (_debug)traceMsg(comp(), "Found reference [%p] to primeiv %p\n",childNode,childNode->getSymbol());
            if (childNode->cannotOverflow() && // no wrapping of index can happen, hence safe to move around
               parentNode->getReferenceCount() < 2 && // safe to swap nodes
               node->getReferenceCount() < 2 &&
               node->getOpCodeValue() == parentNode->getOpCodeValue() && node->getOpCode().isCommutative())
               {
               int32_t swapChildIdx;
               // assume parent is a binary op--determine which child will be swapped
               if (parentNode->getFirstChild() == node)
                  swapChildIdx = 1;
               else if (parentNode->getSecondChild() == node)
                  swapChildIdx = 0;
               else
                  TR_ASSERT(false,"Unexpected node %p\n",parentNode);
               if (performTransformation(comp(), "%sSwapping nodes [%p] and [%p] to create larger loop invariant sub-expression\n",OPT_DETAILS,
                                         childNode,parentNode->getChild(swapChildIdx)))
                  {
                  if (false)  printf("%sSwapping nodes [%p] and [%p] to create larger loop invariant sub-expression\n",OPT_DETAILS,
                                   childNode,parentNode->getChild(swapChildIdx));
                  node->setChild(i,parentNode->getChild(swapChildIdx));
                  parentNode->setChild(swapChildIdx,childNode);
                  _somethingChanged=true;
                  }
               }
            }
         }
      }
   }

const char *
TR_IndexExprManipulator::optDetailString() const throw()
   {
   return "O^O ARRAY INDEX EXPRESSION MANIPULATION: ";
   }
