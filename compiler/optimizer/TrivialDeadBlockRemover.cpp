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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/TrivialDeadBlockRemover.hpp"

#include "compile/Compilation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/Block.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/OMRSimplifierHelpers.hpp"
#include "optimizer/TransformUtil.hpp"

TR_YesNoMaybe TR_TrivialDeadBlockRemover::evaluateTakeBranch (TR::Node* ifNode)
   {
   if (ifNode->getFirstChild() == ifNode->getSecondChild() &&
       !ifNode->getFirstChild()->getOpCode().isFloatingPoint() &&
         (TR::ILOpCode::isEqualCmp(ifNode->getOpCodeValue()) ||
          TR::ILOpCode::isNotEqualCmp(ifNode->getOpCodeValue())))
      {
      if (trace())
         traceMsg(comp(), "An equality comparison %p folded to %d\n", ifNode, TR::ILOpCode::isEqualCmp(ifNode->getOpCodeValue()));

      return TR::ILOpCode::isEqualCmp(ifNode->getOpCodeValue()) ? TR_yes : TR_no;
      }

   TR::Node *first = ifNode->getFirstChild(), *second = ifNode->getSecondChild() ;

   if (!first->getOpCode().isLoadConst() || !second->getOpCode().isLoadConst())
      return TR_maybe;

   if (!first->getOpCode().isIntegerOrAddress() || ifNode->getSize() > sizeof(ifNode->getConstValue()))
      return TR_maybe;

   bool equal, less, greater;

   TR::ILOpCode&  ifOp = ifNode->getOpCode();
   if (ifOp.isUnsigned())
      {
      less = first->getUnsignedConstValue() < second->getUnsignedConstValue();
      greater = first->getUnsignedConstValue() > second->getUnsignedConstValue();
      equal = first->getUnsignedConstValue() == second->getUnsignedConstValue();
      }
   else
      {
      less = first->getConstValue() < second->getConstValue();
      greater = first->getConstValue() > second->getConstValue();
      equal = first->getConstValue() == second->getConstValue();
      }

   static TR_YesNoMaybe decisionTable [][8] =
      //               <         >        <>         ==       <=        >=
      {
         {TR_maybe,  TR_yes,   TR_no,    TR_yes,   TR_no,    TR_yes,   TR_no,    TR_maybe},  //less
         {TR_maybe,  TR_no,    TR_yes,   TR_yes,   TR_no,    TR_no,    TR_yes,   TR_maybe},  //greater
         {TR_maybe,  TR_no,    TR_no,    TR_no,    TR_yes,   TR_yes,   TR_yes,   TR_maybe}   //equal
      };

   int col = ifOp.isCompareTrueIfLess()*1 + ifOp.isCompareTrueIfGreater()*2 + ifOp.isCompareTrueIfEqual()*4;
   int row = less ? 0 : greater ? 1 : 2;

   if (trace())
      traceMsg(comp(), "ifNode %p folded using a decision table,"
         "row %d col %d value %d\n", ifNode, row, col, decisionTable[row][col]);

   return decisionTable[row][col];
   }

bool TR_TrivialDeadBlockRemover::foldIf (TR::Block* b)
   {
   TR::TreeTop* ifTree = b->getLastRealTreeTop();
   TR::Node* ifNode = ifTree->getNode();

   if (!ifNode->getOpCode().isIf())
      return false;

   TR_YesNoMaybe takeBranch = evaluateTakeBranch(ifNode);

   //if is either not foldable at a compile time
   //or we don't simply know how to do it
   if (takeBranch == TR_maybe)
      return false;

   TR::CFGEdge*  removedEdge = changeConditionalToUnconditional(ifNode, b, takeBranch == TR_yes, ifTree, optDetailString());
   bool blocksWereRemoved = removedEdge ? removedEdge->getTo()->nodeIsRemoved() : false;

   if (takeBranch)
      {
      TR_ASSERT(ifNode->getOpCodeValue() == TR::Goto, "expecting the node to have been converted to a goto");
      ifNode = _gotoSimplifier(ifNode, b, ifTree, this);
      }

   if (!ifNode)
      TR::TransformUtil::removeTree(comp(), ifTree);

   return blocksWereRemoved;
   }

int32_t TR_TrivialDeadBlockRemover::perform ()
   {
   if (comp()->getOption(TR_DisableTrivialDeadBlockRemover))
      return 0;

   TR::CFG* cfg = comp()->getFlowGraph();

   // Usedef info should be automatically cleared when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   cfg->createTraversalOrder(true, stackAlloc);

   bool blocksWereRemoved = false;

   for (int b=0;b < cfg->getForwardTraversalLength();b++)
      {
      TR::Block *block = cfg->getForwardTraversalElement(b)->asBlock();
      if (block && block->getExit() && !block->nodeIsRemoved())
         blocksWereRemoved |= foldIf(block);
      }

   if (blocksWereRemoved)
      {
      //printf("@@@trivial dead block elimination performed in %s\n", comp()->signature()); fflush(stdout);
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      }

   return 1;
   }

const char *
TR_TrivialDeadBlockRemover::optDetailString() const throw()
   {
   return "O^O TRIVIAL DEAD BLOCK REMOVAL: ";
   }
