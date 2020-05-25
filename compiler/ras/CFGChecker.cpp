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

#include "ras/CFGChecker.hpp"

#include <stdint.h>
#include <string.h>
#include "env/StackMemoryRegion.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "ras/Debug.hpp"

void
TR_Debug::verifyCFG(TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR_CFGChecker checker(methodSymbol, this);
   checker.check();
   }

TR_CFGChecker::TR_CFGChecker(TR::ResolvedMethodSymbol *methodSymbol, TR_Debug * debug)
   : _cfg(methodSymbol->getFlowGraph()), _fe(debug->_fe), _outFile(debug->_comp->getOutFile())
   {
   _blockChecklist.init(0, debug->_comp->trMemory(), heapAlloc, growable);
   }

void TR_CFGChecker::check()
   {

   if (_cfg == NULL)
      return;

   {
   TR::StackMemoryRegion stackMemoryRegion(*_cfg->comp()->trMemory());

   // _numBlocks includes the dummy start and end blocks
   //
   _numBlocks = _cfg->getNumberOfNodes();
   _numRealBlocks = _numBlocks-2;

   if (debug("traceCFGCHK"))
      {
      _cfg->comp()->dumpMethodTrees("Printing out the TreeTops from CFGChecker");
      if (_outFile) trfprintf(_outFile, "Printing out the CFG from CFGChecker\n");
      _cfg->comp()->getDebug()->print(_cfg->comp()->getOutFile(), _cfg);
      }

   // All blocks in the CFG will be marked with this visit count, so we know
   // when predecessors or successors are not in the CFG.
   //
   _blockChecklist.empty();
   markCFGNodes();


#if defined(DEBUG) || (defined(PROD_WITH_ASSUMES) && !defined(DISABLE_CFG_CHECK))
   performCorrectnessCheck();
   performConsistencyCheck();
#else
   _successorsCorrect = true;
   _isCFGConsistent = true;
#endif
   } // scope of the stack memory region

   if (_successorsCorrect && _isCFGConsistent)
      {
      if (debug("traceCFGCHK"))
         if (_outFile) trfprintf(_outFile, "The CFG is correct\n");
      }
   else
      {
      if (_outFile)
         {
         trfprintf(_outFile, "The CFG is NOT correct\n");
         trfflush(_outFile);
         }
      if (_outFile) trfprintf(_outFile, "Printing out the CFG from CFGChecker\n");
      _cfg->comp()->getDebug()->print(_cfg->comp()->getOutFile(), _cfg);
      TR_ASSERT(0, "The CFG is NOT correct");
      }
   }



// Mark each node in the CFG with the current visit count
//
void TR_CFGChecker::markCFGNodes()
   {
   for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
      _blockChecklist.set(node->getNumber());
   }



// Driver for the correctness checks performed on the CFG
// (Step 1 of our CFG checker).
//
void TR_CFGChecker::performCorrectnessCheck()
   {
   _successorsCorrect = true;

   if (_cfg->getStart()->getSuccessors().size() != 1)
      {
      if (_outFile) trfprintf(_outFile, "There is more than one successor block for the start block\n");
      _successorsCorrect = false;
      }
   else if (!_cfg->getEnd()->getSuccessors().empty())
      {
      if (_outFile) trfprintf(_outFile, "There is a successor for the end block\n");
      _successorsCorrect = false;
      }
   else
      {
      if (!arrangeBlocksInProgramOrder())
         {
         _successorsCorrect = false;
         }

      // Check if the lone successor basic block of the CFG start block
      // is the first basic block in the actual program. Note that
      // this check is performed in this manner as there are no IL
      // instructions in the CFG start block, and so correctness of the
      // successor must be verified in this manner.
      //
      if (_cfg->getStart()->getSuccessors().front()->getTo() != _blocksInProgramOrder[0])
         {
         if (_outFile) trfprintf(_outFile, "The successor block for the (dummy) start block in the CFG is NOT the start block in the actual program\n");
         _successorsCorrect = false;
         }
      }

   if (_successorsCorrect)
      {
      for (int32_t i = 0; i < _numRealBlocks; i++)
         {
         if (!areSuccessorsCorrect(i))
            {
            _successorsCorrect = false;
            break;
            }
         }
      }

   if (!_successorsCorrect)
      if (_outFile) trfprintf(_outFile, "Check for correctness of successors is NOT successful\n");
   }


// Called by performCorrectnessCheck() to store the list of basic blocks
// in program order in blocksInProgramOrder. This is used to obtain the
// adjacent block of a basic block containing an if (used to check if the
// successors of the block containing the if are correct).
//
bool TR_CFGChecker::arrangeBlocksInProgramOrder()
   {
   // Create an extra slot at the end for a null pointer so we can always look
   // at the "next" block to a real block.
   //
   _blocksInProgramOrder = (TR::Block **) _cfg->comp()->trMemory()->allocateStackMemory((_numRealBlocks+1)*sizeof(TR::Block *));

   memset(_blocksInProgramOrder, 0, (_numRealBlocks+1)*sizeof(TR::Block *));

   int i = 0;
   TR::TreeTop *cursorTreeTop = _cfg->comp()->getStartTree();

   // Check the next available node number in the CFG
   //
   int32_t nextNodeNumber = _cfg->getNextNodeNumber();
   if (nextNodeNumber < -1 ||
       (nextNodeNumber >= 0 && nextNodeNumber < _numBlocks))
      {
      if (_outFile) trfprintf(_outFile, "CFG has a bad nextNodeNumber [%d]\n", nextNodeNumber);
      return false;
      }

   // Traverse the list of TreeTops which are known to be in
   // program order and collect basic blocks in program order
   // in the process.
   //
   while (cursorTreeTop)
      {
      // Check for too many real blocks in the trees
      //
      TR_ASSERT(i < _numRealBlocks, "CFG Checker, too many blocks in trees");
      TR::Node *node = cursorTreeTop->getNode();
      TR_ASSERT( node->getOpCodeValue() == TR::BBStart, "Missing TR::BBStart");
      TR::Block *b = node->getBlock();

      if (!_blockChecklist.isSet(b->getNumber()))
         {
         if (_outFile) trfprintf(_outFile, "Block %d [%p]  at tree node [%p] is in the trees but not in the CFG\n", b->getNumber(),
                                     b, node);
         return false;
         }

      // Check the node number of the block
      //
      if ((nextNodeNumber < 0 && b->getNumber() != nextNodeNumber) ||
          (nextNodeNumber >= 0 && b->getNumber() >= nextNodeNumber))
         {
         if (_outFile) trfprintf(_outFile, "Block %d [%p]  at tree node [%p] has a bad node number [%d]\n", b, node, b->getNumber());
         return false;
         }
      _blocksInProgramOrder[i++] = b;
      cursorTreeTop = b->getExit()->getNextTreeTop();
      }

   // Check for the right number of blocks in the trees
   //
   if (i != _numRealBlocks)
      {
      if (_outFile) trfprintf(_outFile, "Number of blocks in trees [%d] does not match number in CFG [%d]\n", i, _numRealBlocks);
      return false;
      }
   return true;
   }



// Check if the successors of the basic block have been
// computed correctly. This check is performed by examining
// the IL instruction at the exit of the basic block, and comparing the
// targets of the branch (goto or if) or switch statement, with
// the successors in the CFG. Returns true if the successors are correct.
//
bool TR_CFGChecker::areSuccessorsCorrect(int32_t i)
   {
   TR::Block *block = _blocksInProgramOrder[i];

   // If the block is null, there was something wrong with the block counts
   // earlier.
   //
   if (block == NULL)
      return true;

   /*
   if ((_cfg->getMaxFrequency() >= 0) &&
       (block->getFrequency() < 0))
      {
      if (_outFile) trfprintf(_outFile, "Block [%d] does not have frequency set\n", block->getNumber());
      return false;
      }
   */

   // Check that all successors are in the CFG and are unique
   //
   TR::Block   *next;
   for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
      {
      next = toBlock((*edge)->getTo());
      if (!_blockChecklist.isSet(next->getNumber()))
         {
         if (_outFile) trfprintf(_outFile, "Successor block [%d] of block [%d] is not in the CFG\n",
                                     next->getNumber(), block->getNumber());
         return false;
         }
      }
   for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
      {
      next = toBlock((*edge)->getTo());
      if (!_blockChecklist.isSet(next->getNumber()))
         {
         if (_outFile) trfprintf(_outFile, "Exception successor block [%d] of block [%d] is not in the CFG\n",
                                     next->getNumber(), block->getNumber());
         return false;
         }
      for (auto edge2 = block->getExceptionSuccessors().begin(); edge2 != block->getExceptionSuccessors().end(); ++edge2)
         if (((*edge) != (*edge2)) && (next == (*edge2)->getTo()))
            {
            if (_outFile) trfprintf(_outFile, "Exception successor block [%d] of block [%d] is listed more than once\n",
                                        next->getNumber(), block->getNumber());
            return false;
            }
      }

   TR::Node * lastNode = block->getExit()->getPrevTreeTop()->getNode();
   if (lastNode->getOpCodeValue() == TR::fence) lastNode = block->getExit()->getPrevTreeTop()->getPrevTreeTop()->getNode();
   if ((lastNode->getOpCodeValue() == TR::NULLCHK) || (lastNode->getOpCodeValue() == TR::treetop)) lastNode = lastNode->getFirstChild(); // athrow might be a child of a null check
   TR::ILOpCode &opCode = lastNode->getOpCode();


     if (!(opCode.isBranch() || opCode.isSwitch() || opCode.isReturn() || opCode.isJumpWithMultipleTargets() ||  opCode.isStoreReg() || opCode.getOpCodeValue() == TR::athrow))
     {
      if (!(block->getSuccessors().size() == 1))
	      {
         if(_outFile) trfprintf(_outFile, "Considering Node %p\n",lastNode);
         if (_outFile) trfprintf(_outFile, "Last non-fence opcode in block [%d] is not a branch, switch, or a return and it does not have exactly one successor\n", block->getNumber());
         return false;
         }

      if (block->getSuccessors().front()->getTo() != _blocksInProgramOrder[i+1])
         {
         if (_outFile) trfprintf(_outFile, "Successor block [%d] of block [%d] (with no branch, switch, or return at the end) is not the fall through block\n", block->getSuccessors().front()->getTo()->getNumber(), block->getNumber());
         return false;
         }
      }


   if (opCode.isBranch())
      {
      TR::Block *fallThroughBlock = _blocksInProgramOrder[i+1];
      TR::Block *branchBlock = lastNode->getBranchDestination()->getNode()->getBlock();

      // Simple checks for the CFG being incorrect for a branch.
      //
      if (opCode.getOpCodeValue() == TR::Goto)
	 {
         if (!(block->getSuccessors().size() == 1))
	    {
            if (_outFile) trfprintf(_outFile, "Number of successors of block [%d] having a goto at the exit is not equal to one\n", block->getNumber());
            return false;
            }
         }
      else
	 {
         int numUniqueChildren;
         if (fallThroughBlock == branchBlock)
            numUniqueChildren = 1;
         else
            numUniqueChildren = 2;

         if (!(numUniqueChildren == block->getSuccessors().size()))
            {
            if (_outFile) trfprintf(_outFile, "Number of successors of block [%d] having an if at the exit is not equal to the number of unique targets of the if\n", block->getNumber());
            return false;
            }
         }


      // Check for whether the CFG is correct for a branch.
      //
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         next = toBlock((*edge)->getTo());
         if (!( (next == fallThroughBlock) || (next == branchBlock) ))
            {
            if (_outFile) trfprintf(_outFile, "Successor block [%d] of block [%d] containing a branch does not match the destination(s) specified in the IL branch instruction\n",
                                        next->getNumber(), block->getNumber());
            return false;
            }
         }
      }
   else if (opCode.isSwitch())
      {
      // Simple check for the CFG being incorrect for a switch.
      //
      // if (!((lastNode->getCaseIndexUpperBound()-1) == block->getSuccessors().size()))

      if (!(getNumUniqueCases(lastNode) == block->getSuccessors().size()))
         {
         if (_outFile) trfprintf(_outFile, "Number of successors of block [%d] having a switch at the exit is not equal to the number of destinations in the IL switch instruction\n", block->getNumber());
         return false;
         }

      // Check for whether the CFG is correct for a switch.
      //
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         next = toBlock((*edge)->getTo());

         if ( ! equalsAnyChildOf(next->getEntry(), lastNode) )
            {
            if (_outFile) trfprintf(_outFile, "Successor block [%d] of block [%d] containing a switch does not match any of the destinations specified in the IL switch instruction\n", next->getNumber(), block->getNumber());
            return false;
            }
         }
      }
   else if (opCode.isReturn() || opCode.getOpCodeValue() == TR::athrow)
      {
      if (!(block->getSuccessors().size() == 1))
	 {
         if (_outFile) trfprintf(_outFile, "Number of successors of block [%d] having a return at the exit is not equal to one\n", block->getNumber());
         return false;
         }

      TR::CFGNode *cfgEnd = _cfg->getEnd();
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         next = toBlock((*edge)->getTo());
         if (next != cfgEnd)
	    {
            if (_outFile) trfprintf(_outFile, "Successor block [%d] of block [%d] containing a return is NOT the exit block\n", next->getNumber(), block->getNumber());
            return false;
            }
         }
      }
   return true;
   }



// Called by areSuccessorsCorrect() to check if the entry TreeTop
// in a successor block of a basic block containing a switch is also
// found in the list of branch destinations of the switch. Returns true
// if the TreeTop is present in the list of branch destinations of the
// switch node.
//
bool TR_CFGChecker::equalsAnyChildOf(TR::TreeTop *treeTop, TR::Node *switchNode)
   {
   for (int i = switchNode->getCaseIndexUpperBound()-1; i > 0; i--)
         {
         TR::TreeTop *branchTreeTop = switchNode->getChild(i)->getBranchDestination();
         if (treeTop == branchTreeTop)
            return true;
         }

   return false;
   }








int32_t TR_CFGChecker::getNumUniqueCases(TR::Node *switchNode)
   {
   int32_t numCases = switchNode->getCaseIndexUpperBound() - 1;
   int32_t numUniqueCases = 0;

   TR::TreeTop **uniqueTreeTops;
   uniqueTreeTops = (TR::TreeTop**)_cfg->comp()->trMemory()->allocateStackMemory(numCases*sizeof(TR::TreeTop*));

   memset(uniqueTreeTops, 0, numCases*sizeof(TR::TreeTop*));

   TR::TreeTop *defaultBranchTreeTop = switchNode->getSecondChild()->getBranchDestination();
   uniqueTreeTops[numUniqueCases++] = defaultBranchTreeTop;

   for (int i = 2; i <= numCases; i++)
      {
      TR::TreeTop *branchTreeTop = switchNode->getChild(i)->getBranchDestination();

      bool unique = true;
      for (int j = 0; j < numUniqueCases; j++)
         {
         if (uniqueTreeTops[j] == branchTreeTop)
            {
            unique = false;
            break;
            }
         }

      if (unique)
         uniqueTreeTops[numUniqueCases++] = branchTreeTop;
      }

   // if (_outFile) trfprintf(_outFile, "numUniqueChildren = %d\n",numUniqueChildren);

   return numUniqueCases;
   }










// Driver for the consistency checks performed on the CFG
// (Step 2 of our CFG checker).
//
void TR_CFGChecker::performConsistencyCheck()
   {
   _isCFGConsistent = true;
 /*
   TR::CFGEdge *edge;
   for (edge = _cfg->getFirstEdge(); edge; edge = edge->getNext())
      {
      TR::CFGNode *from = edge->getFrom();
      if (!_cfg->getNodes().find(from))
         {
         if (_outFile) trfprintf(_outFile, "Block [%p] numbered %d is not in the CFG nodes list\n", from, from->getNumber());
        _isCFGConsistent = false;
         break;
         }
      if (!from->getSuccessors().find(edge) && !from->getExceptionSuccessors().find(edge))
         {
         if (_outFile) trfprintf(_outFile, "Edge between block [%p] numbered %d and block [%p] numbered %d is in the CFG edge list but not in successors list\n", from, from->getNumber(), edge->getTo(), edge->getTo()->getNumber());
        _isCFGConsistent = false;
         break;
         }
      }
   */

   TR::CFGNode *node;

   node = _cfg->getStart();
   if (!(node->getPredecessors().empty() && node->getExceptionPredecessors().empty()))
      {
      if (_outFile) trfprintf(_outFile, "CFG Start block has predecessors\n");
      _isCFGConsistent = false;
      }
   node = _cfg->getEnd();
   if (!isConsistent(toBlock(node)))
      _isCFGConsistent = false;

   for (int32_t i = 0; i < _numRealBlocks; i++)
      {
      if (!isConsistent(_blocksInProgramOrder[i]))
         _isCFGConsistent = false;
      }

   if (checkForUnreachableCycles())
      _isCFGConsistent = false;

   if (!_isCFGConsistent)
      if (_outFile) trfprintf(_outFile, "Check for consistency of CFG is NOT successful\n");
   }




// Checks if the basic block obeys the consistency requirements.
// The predecessor list of this block is examined and each block
// belonging to that list must have this block in its successor list.
// Returns true if this is the case.
//
bool TR_CFGChecker::isConsistent(TR::Block *block)
   {
   // If the block is null, there was something wrong with the block counts
   // earlier.
   //
   if (block == NULL)
      return true;

   TR::CFGEdge *nextEdge;
   TR::Block   *nextBlock;

   // The only blocks allowed to have no predecessors or exception predecessors
   // are the start and end blocks. The end block is the only one that can reach
   // here.
   //
   if (block->getPredecessors().empty() &&
       block->getExceptionPredecessors().empty())
      {
      if (block == _cfg->getEnd())
         return true;
      if (_outFile) trfprintf(_outFile, "Block %d [%p] is an orphan\n", block->getNumber(), block);
      return false;
      }

   for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
      {
      nextBlock = toBlock((*nextEdge)->getFrom());
      if (!_blockChecklist.isSet(nextBlock->getNumber()))
         {
         if (_outFile) trfprintf(_outFile, "Predecessor block [%d] of block [%d] is not in the CFG\n", nextBlock->getNumber(), block->getNumber());
         return false;
         }
      auto matchEdge = nextBlock->getSuccessors().begin();
      for (; matchEdge != nextBlock->getSuccessors().end(); ++matchEdge)
         {
         if (*matchEdge == *nextEdge)
            break;
         }
      if (matchEdge == nextBlock->getSuccessors().end())
         {
         if (_outFile) trfprintf(_outFile, "Predecessor block [%d] of block [%d] does not contain block [%d] in its successors list\n", nextBlock->getNumber(), block->getNumber(), block->getNumber());
         return false;
         }
      }

   bool ok = true;
   auto basicP = block->getPredecessors().begin();
   while (basicP != block->getPredecessors().end())
      {
      TR::CFGEdge * e = (*basicP);
      if (e->getTo() != block)
         {
         if (_outFile)
            {
            trfprintf(_outFile,"ERROR: edge from %d to %d does not point to block_%d\n",
                          e->getFrom()->getNumber(), e->getTo()->getNumber(), block->getNumber());
            }
         ok = false;
         }
      bool foundMe = false;
      auto basicP2 = e->getFrom()->getSuccessors().begin();
      while ((basicP2 != e->getFrom()->getSuccessors().end()) && !foundMe)
         {
         if ((*basicP2)->getTo() == block)
            {
            foundMe = true;
            }
         ++basicP2;
         }
      if (!foundMe)
         {
         if (_outFile)
            {
            trfprintf(_outFile,"ERROR: block_%d is a predecessor of block_%d but the reverse is not true\n",
                          e->getFrom()->getNumber(), block->getNumber());
            }
         ok = false;
         }
      ++basicP;
      }

   auto basicS = block->getSuccessors().begin();
   while (basicS != block->getSuccessors().end())
      {
      TR::CFGEdge * e = (*basicS);
      if (e->getFrom() != block)
         {
         if (_outFile)
            {
            trfprintf(_outFile,"ERROR: edge from %d to %d does not come from block_%d\n",
                          e->getFrom()->getNumber(), e->getTo()->getNumber(), block->getNumber());
            }
         ok = false;
         }
      bool foundMe = false;
      auto basicS2 = e->getTo()->getPredecessors().begin();
      while ((basicS2 != e->getTo()->getPredecessors().end()) && !foundMe)
         {
         if ((*basicS2)->getFrom() == block)
            {
            foundMe = true;
            }
         ++basicS2;
         }
      if (!foundMe)
         {
         if (_outFile)
            {
            trfprintf(_outFile,"ERROR: block_%d is a successor of block_%d but the reverse is not true\n",
                          e->getTo()->getNumber(), block->getNumber());
            }
         ok = false;
         }
      ++basicS;
      }

   if (!ok) return false;

   for (auto nextEdge = block->getExceptionPredecessors().begin(); nextEdge != block->getExceptionPredecessors().end(); ++nextEdge)
      {
      nextBlock = toBlock((*nextEdge)->getFrom());
      if (!_blockChecklist.isSet(nextBlock->getNumber()))
         {
         if (_outFile) trfprintf(_outFile, "Exception predecessor block [%d] of block [%d] is not in the CFG\n", nextBlock->getNumber(), block->getNumber());
         return false;
         }
      auto matchEdge = nextBlock->getExceptionSuccessors().begin();
      for (;matchEdge != nextBlock->getExceptionSuccessors().end(); ++matchEdge)
         {
         if ((*matchEdge) == (*nextEdge))
            break;
         }
      if (matchEdge == nextBlock->getExceptionSuccessors().end())
         {
         if (_outFile) trfprintf(_outFile, "Exception Predecessor block [%d] of block [%d] does not contain block [%d] in its exception successors list\n", nextBlock->getNumber(), block->getNumber(), block->getNumber());
         return false;
         }
      }

   // One would expect that we could check here for a single block exception loop,
   // however, because of the strange nature of monexit, we need to accomodate the
   // possibility of a single block exception loop.

   return true;
   }

bool
TR_CFGChecker::checkForUnreachableCycles()
   {
   TR::StackMemoryRegion stackMemoryRegion(*_cfg->comp()->trMemory());

   TR_BitVector reachableBlocks;
   reachableBlocks.init(_cfg->getNumberOfNodes(), _cfg->comp()->trMemory(), stackAlloc, growable);
   _cfg->comp()->getFlowGraph()->findReachableBlocks(&reachableBlocks);

   bool unreachable = false;
   for (TR::CFGNode * node = _cfg->getFirstNode(); node; node = node->getNext())
      if (!reachableBlocks.isSet(node->getNumber()) && node->asBlock() && node != _cfg->getEnd())
         {
         unreachable = true;
         if (_outFile) trfprintf(_outFile, "Block %d [%p] is unreachable or is in an unreachable cycle\n", node->getNumber(), node);
         }

   return unreachable;
   }
