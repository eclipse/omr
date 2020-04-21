/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "optimizer/GeneralLoopUnroller.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O GENERAL LOOP UNROLLER: "
#define OPT_DETAILS1 "O^O ARRAY ALIASING REFINER: "
#define GET_CLONE_BLOCK(x)   _blockMapper[_iteration%2][(x)->getNumber()]
#define SET_CLONE_BLOCK(x,y) _blockMapper[_iteration%2][(x)->getNumber()]=(y)
#define GET_CLONE_NODE(x)     _nodeMapper[_iteration%2][(x)->getNumber()]
#define SET_CLONE_NODE(x,y)   _nodeMapper[_iteration%2][(x)->getNumber()]=(y)
#define GET_PREV_CLONE_BLOCK(x) _blockMapper[(1+_iteration)%2][(x)->getNumber()]
#define GET_PREV_CLONE_NODE(x)   _nodeMapper[(1+_iteration)%2][(x)->getNumber()]
#define CURRENT_MAPPER          _iteration%2

#define CREATED_BY_GLU 9

// Aliasing based weight analysis
class LoopWeightProbe
   {
   public:
   LoopWeightProbe(TR::Compilation *comp) :
      _numUsed(0), _numKilled(0), _numMatConst(0), _numUnmatConst(0),
      _numCalls(0), _numLeafRoutines(0), _numPureFunctions(0), _numBIFs(0),
      _used(comp->allocator()), _defd(comp->allocator()) {}
   uint32_t          _numUsed;
   uint32_t          _numKilled;
   uint32_t          _numMatConst;
   uint32_t          _numUnmatConst;
   uint32_t          _numCalls;
   uint32_t          _numLeafRoutines;
   uint32_t          _numPureFunctions;
   uint32_t          _numBIFs;  // Builtins; RecognizedMethods
   TR::BitVector     _used;
   TR::BitVector     _defd;

   };

static TR::ILOpCodes getConstOpCode(TR::DataType type)
   {
   switch (type.getDataType())
      {
      case TR::Int8:  return TR::bconst;
      case TR::Int16: return TR::sconst;
      case TR::Int32: return TR::iconst;
      case TR::Int64: return TR::lconst;
      case TR::Address: return TR::comp()->target().is64Bit() ? TR::lconst : TR::iconst;
      default:
         TR_ASSERT(false, "unsupported type");
         return TR::iconst;
      }
   TR_ASSERT(false, "unsupported type");
   return TR::iconst;
   }

static inline int32_t ceiling(int64_t numer, int64_t denom)
   {
   return (numer % denom == 0) ?
      numer / denom :
      numer / denom + 1;
   }

static TR_ScratchList<TR::CFGEdge> *join(TR_ScratchList<TR::CFGEdge> *to, TR_ScratchList<TR::CFGEdge> *from)
   {
   if (from == NULL)
      return to;
   if (to == NULL)
      return from;

   ListIterator<TR::CFGEdge> it(from);
   for (TR::CFGEdge *edge = it.getCurrent(); edge; edge = it.getNext())
      to->add(edge);

  return to;
   }

static TR_ScratchList<TR::CFGEdge> *findCorrespondingCFGEdges(TR_Structure *from, TR_Structure *to, TR::Compilation * comp)
   {
   if (from->asBlock())
      {
      TR::Block *block = from->asBlock()->getBlock();
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         TR::Block *succ = toBlock((*edge)->getTo());
         if (to->contains(succ->getStructureOf()))
            return new (comp->trHeapMemory()) TR_ScratchList<TR::CFGEdge>(*edge, comp->trMemory());
         }
      return NULL;
      }
   else
      {
      TR_ScratchList<TR::CFGEdge> *edges = NULL;
      TR_RegionStructure::Cursor it(*(from->asRegion()));
      TR_StructureSubGraphNode *node;
      for (node = it.getFirst(); node; node = it.getNext())
         edges = join(edges, findCorrespondingCFGEdges(node->getStructure(), to, comp));
      return edges;
      }
   }

static bool isBranchAtEndOfLoop(TR_RegionStructure *loop, TR::Block *branchBlock)
   {
   for (auto e = branchBlock->getSuccessors().begin(); e != branchBlock->getSuccessors().end(); ++e)
      {
      if ((*e)->getTo()->getNumber() == loop->getEntryBlock()->getNumber())
         return true;
      }
   return false;
   }

bool TR_LoopUnroller::nodeRefersToSymbol(TR::Node *node, TR::Symbol *sym)
   {
   if (!node) return false;
   if (node->getOpCode().hasSymbolReference() && node->getSymbol() == sym)
      return true;
   for (int32_t child = 0; child < node->getNumChildren(); child++)
      if (nodeRefersToSymbol(node->getChild(child), sym))
         return true;
   return false;
   }

TR::Symbol *TR_LoopUnroller::findSymbolInTree(TR::Node *node)
   {
   if (!node) return NULL;
   if (node->getOpCode().hasSymbolReference())
      return node->getSymbol();
   for (int32_t child = node->getNumChildren() - 1; child >= 0; child--)
      {
      TR::Symbol *sym;
      if ((sym = findSymbolInTree(node->getChild(child))))
         return sym;
      }
   return NULL;
   }

static bool checkNodeFrequency(TR_StructureSubGraphNode *subNode, TR_RegionStructure *loop)
   {
   TR::Block *block = subNode->getStructure()->asBlock() != NULL ? subNode->getStructure()->asBlock()->getBlock() : NULL;
   return !block || loop->getEntryBlock()->getFrequency() < 5000 || block->getFrequency() > 2000 || block->getFrequency() == -1;
   }

static bool hasConnectableEdges(TR::Compilation *comp, TR_RegionStructure *loop)
   {
   if (!TR::isJ9() || comp->getOption(TR_DisableGLUColdRedirection))
      return true;

   TR_RegionStructure::Cursor nodeIt(*loop);
   TR_StructureSubGraphNode *subNode;
   bool connectable = false;
   for (subNode = nodeIt.getFirst(); subNode; subNode = nodeIt.getNext())
      {
      for (auto edge = subNode->getSuccessors().begin(); edge != subNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (succ == loop->getEntry())
            {
            if (checkNodeFrequency(subNode, loop))
               {
               connectable = true;
               break;
               }
            }
         }
      }
   return connectable;
   }

bool TR_LoopUnroller::shouldConnectToNextIteration(TR_StructureSubGraphNode *subNode, TR_RegionStructure *loop)
   {
   if (!TR::isJ9() || comp()->getMethodHotness() <= warm || comp()->getOption(TR_DisableGLUColdRedirection))
      return true;
   if (!hasConnectableEdges(comp(), loop))
      return true;
   return _unrollKind == CompleteUnroll || isCountedLoop() || checkNodeFrequency(subNode, loop);
   }

void TR_LoopUnroller::unrollLoopOnce(TR_RegionStructure *loop, TR_StructureSubGraphNode *branchNode,
                                     bool finalUnroll)
   {
   TR_StructureSubGraphNode *newEntryNode = NULL;

   //Reset the mappers.
   memset(_blockMapper[CURRENT_MAPPER], 0, _numNodes * sizeof(TR::Block *));
   memset( _nodeMapper[CURRENT_MAPPER], 0, _numNodes * sizeof(TR_StructureSubGraphNode *));

   //First, clone all the original blocks in the loop
   cloneBlocksInRegion(loop);

   //Create duplicates of all sub structures in the loop
   TR_RegionStructure::Cursor nodeIt(*loop);
   TR_StructureSubGraphNode *subNode;
   for (subNode = nodeIt.getFirst(); subNode; subNode = nodeIt.getNext())
      {
      if (subNode->getNumber() >= _numNodes)
         continue; //clone only original nodes

      //Clone all the substructures
      TR_Structure *subStruct = subNode->getStructure();
      TR_Structure *clonedSubStruct = cloneStructure(subStruct);
      TR_StructureSubGraphNode *clonedSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(clonedSubStruct);

      //Memorize the mapping between subNode and clonedSubNode
      SET_CLONE_NODE(subNode, clonedSubNode);
      //Add it to the loop
      loop->addSubNode(clonedSubNode);

      if (subNode == loop->getEntry())
         newEntryNode = clonedSubNode;
      }

   //Fix all the exit edges in the subnodes
   nodeIt.reset();
   for (subNode = nodeIt.getFirst(); subNode; subNode = nodeIt.getNext())
      {
      if (subNode->getNumber() >= _numNodes)
         continue;
      TR_StructureSubGraphNode *clonedSubNode = GET_CLONE_NODE(subNode);

      if (clonedSubNode)
         /*GGLU*/
         {
         if (isCountedLoop())
            fixExitEdges(subNode->getStructure(), clonedSubNode->getStructure());
         else
            fixExitEdges(subNode->getStructure(), clonedSubNode->getStructure(), branchNode);
         }
      }

   if (_iteration == 1)
      _firstEntryNode = newEntryNode;

   //Now Fixup the structure edges
   TR_StructureSubGraphNode *clonedBranchNode = GET_CLONE_NODE(branchNode);

   /*GGLU*/
   //First of all, remove the branch
   bool branchAtEnd = true;
   if (isCountedLoop())
      {
      // If the exit branch is in the middle of the loop and a complete unroll is going
      // to happen, it is not valid to remove the branch for the last unroll (the top half
      // of the loop maybe executed more number of times compared to the bottom half)
      //
      if (finalUnroll && _unrollKind == CompleteUnroll)
         {
         branchAtEnd = isBranchAtEndOfLoop(loop, branchNode->getStructure()->asBlock()->getBlock());
         }

      if (branchAtEnd)
         clonedBranchNode->getStructure()->asBlock()->getBlock()->removeBranch(comp());
      }

   //Look at all the structures edges in the original loop, and create appropriate edges
   //for each edge type (described below).
   nodeIt.reset();
   for (subNode = nodeIt.getFirst(); subNode; subNode = nodeIt.getNext())
      {
      if (subNode->getNumber() >= _numNodes)
         continue;

      TR_StructureSubGraphNode *clonedSubNode = GET_CLONE_NODE(subNode);

      //Now iterate over all the edges originating from subnode and replicate
      //them, if possible, for the clone
      bool subNodeHasBackEdge = false;
      for (auto edge = subNode->getSuccessors().begin(); edge != subNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (succ == loop->getEntry())
            {
            subNodeHasBackEdge = true;
            break;
            }
         }
      for (auto edge = subNode->getSuccessors().begin(); edge != subNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         TR_StructureSubGraphNode *clonedSucc = GET_CLONE_NODE(succ);

         //Edges are classified into 4 Categories:
         //A: exit edge originating from the branch node
         //B: exit edge not originating from the branch node
         //C: back edge to the entry node
         //D: internal edges other than the backedges

         if (subNode == branchNode && !clonedSucc) //Type A
            {
            /*GGLU*/
            bool fixBranch = true;
            if (isCountedLoop())
               {
               //nothing to do, removed branch above.
               fixBranch = false;
               //branch not removed if a complete unroll is being done
               //and it is in the middle of the loop
               if (finalUnroll && _unrollKind == CompleteUnroll && !branchAtEnd)
                  fixBranch = true;
               }
            if (fixBranch)
               {
               addExitEdgeAndFixEverything(loop, *edge, clonedSubNode);
               }
            }
         else if (subNode != branchNode && !clonedSucc) //Type B
            {
            EdgeContext context = InvalidContext;
            if (finalUnroll &&
                  (_unrollKind == CompleteUnroll) &&
                  subNodeHasBackEdge)
               context = BackEdgeFromLastGenerationCompleteUnroll;

            addExitEdgeAndFixEverything(loop, *edge, clonedSubNode, NULL, NULL, context);
            }
         else if (succ == loop->getEntry()) //Type C
            {
            //We have a backedge of the form Edge(A -> E).
            //
            //If this is the final unrolling, and we are not unrolling completely,
            //  add edges back to the original entry pt. (back edges to the true
            //  entry of the loop)
            //  ie, we must create (A' -> E) instead of (A' -> E').
            //
            //If this is the final unrolling, and We are going to unroll the loop
            //  completely, add (A' -> X) where X is the exit destination of the
            //  loop.
            //
            //For all iterations, except the first,
            //  add and edge from the previous generation's A to the entry
            //  point of this unrolling.  ie. (A(prev) -> E(current))
            //
            if (finalUnroll)
               {
               if (_unrollKind != CompleteUnroll)
                  //Add an edge from the cloned subnode to the original entry
                  addEdgeAndFixEverything(loop, *edge, clonedSubNode, loop->getEntry(),
                                          false, false, true, BackEdgeToEntry);
               else
                  {
                  //add an edge to the exit destination
                  //(loop, edge, clonedSubnode, exitDestination, false, false, false,
                  // RedirectBackEdgeToExitDestination
                  redirectBackEdgeToExitDestination(loop, branchNode, clonedSubNode, (subNode != branchNode));
                  }
               }

            if (_iteration != 1) //for all iterations except the first iteration
               {
               if (shouldConnectToNextIteration(toStructureSubGraphNode(subNode), loop))
                  {
                  addEdgeAndFixEverything(loop, *edge, GET_PREV_CLONE_NODE(subNode), newEntryNode,
                                          false, false, false, BackEdgeFromPrevGeneration);
                  }
               else
                  {
                  if (trace())
                     traceMsg(comp(), "Redirecting cold edge from block_%d to unrolled loop header rather than going to the next iteration\n", subNode->getNumber());
                  addEdgeAndFixEverything(loop, *edge, GET_PREV_CLONE_NODE(subNode), loop->getEntry(),
                                          false, false, true, BackEdgeToEntry);
                  }
               }
            }
         else //Type D
            {
            addEdgeAndFixEverything(loop, *edge);
            }
         }
      }


   processSwingQueue();

   if (trace())
      {
      traceMsg(comp(), "\nstructure after cloning the loop for the %dth time:\n\n", _iteration);
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      getDebug()->print(comp()->getOutFile(), _cfg);
      comp()->dumpMethodTrees("method trees:");
      }
   //We are done.
   }

void TR_LoopUnroller::redirectBackEdgeToExitDestination(TR_RegionStructure *loop,
                                                               TR_StructureSubGraphNode *branchNode,
                                                               TR_StructureSubGraphNode *fromNode,
                                                               bool notLoopBranchNode)
   {
   //Find the exit destination
   TR::CFGEdge *exitEdge = NULL;
   for (auto edge = branchNode->getSuccessors().begin(); (edge != branchNode->getSuccessors().end()) && exitEdge == NULL; ++edge)
      {
      if (loop->isExitEdge(*edge))
         exitEdge = *edge;
      }

   //Add A->X*
   addEdgeForSpillLoop
      (loop, exitEdge, fromNode,
       findNodeInHierarchy(loop->getParent()->asRegion(),
                           toStructureSubGraphNode(exitEdge->getTo())->getNumber()),
       false,
       BackEdgeFromLastGenerationCompleteUnroll,
       notLoopBranchNode);
   }

void TR_LoopUnroller::modifyBranchTree(TR_RegionStructure *loop,
                                              TR_StructureSubGraphNode *loopNode,
                                              TR_StructureSubGraphNode *branchNode)
   {
   TR_ASSERT(isCountedLoop() && _unrollKind != CompleteUnroll && _spillLoopRequired,
          "GLU: modifying branch tree under wrong set of conditions");

   TR::Block *branchBlock = branchNode->getStructure()->asBlock()->getBlock();
   TR::Node *branch = branchBlock->getLastRealTreeTop()->getNode();
   TR_RegionStructure *parent = loop->getParent()->asRegion();
   TR_StructureSubGraphNode *predNode = toStructureSubGraphNode
      (loopNode->getPredecessors().front()->getFrom());
   TR_BlockStructure *predBlock = predNode->getStructure()->asBlock();
   TR_ASSERT(predBlock, "Interesting... loop-invariant block is not a block");
   TR::Block *pBlock = predBlock->getBlock();

   // if the loop was the first block in the method, pBlock won't have real
   // entry/exit trees.  Add a dummy block at head
   //
   if (pBlock->getEntry() == 0)
      {
      TR::Block *newBlock = TR::Block::createEmptyBlock(branch, comp(), MAX_COLD_BLOCK_COUNT+1, pBlock); //branchBlock->getFrequency());
      newBlock->getExit()->join(loop->getEntryBlock()->getEntry());
      comp()->setStartTree(newBlock->getEntry());

      _cfg->addNode(newBlock);
      TR_BlockStructure *newBlockStr = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newBlock->getNumber(), newBlock);
      TR_StructureSubGraphNode *newNode  = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newBlockStr);

      parent->addSubNode(newNode);

      TR::CFGEdge *edgeToRemove = pBlock->getSuccessors().front();

      _cfg->addEdge(TR::CFGEdge::createEdge(pBlock,  newBlock, trMemory()));
      TR::CFGEdge::createEdge(predNode,  newNode, trMemory());
      _cfg->addEdge(TR::CFGEdge::createEdge(newBlock, loop->getEntryBlock(), trMemory()));
      TR::CFGEdge::createEdge(newNode,  loopNode, trMemory());

      _cfg->removeEdge(edgeToRemove);
      parent->removeEdge(predBlock, loopNode->getStructure());

      predNode = newNode;
      predBlock = newBlockStr;
      pBlock = newBlock;
      }

   TR::Block *spillBlock = _spillNode->getStructure()->asRegion()->getEntryBlock();

   // 1- Change the bound node in the loop-test.
   //    N gets replaced by N-(ku-1) or N-(ku+1) (increasing or decreasing loops)
   //    If the bound is a constant - replace the constant in-place.
   //    Else save the new bound in a temporary outside the loop.
   //
   TR::Node *originalLimitNode = branch->getSecondChild();
   TR::Node *newLimitNode = NULL;
   int32_t  adjustment   = getLoopStride() * (1+_unrollCount) +
      (isIncreasingLoop() ? -1 : 1);

   bool const_overflow = false;
   bool is64BitComparison = TR::DataType::getSize(getTestChildType()) == 8 ? true : false;

   // check for overflow if upper bound is constant
   if (originalLimitNode->getOpCode().isLoadConst() &&
       getTestIsUnsigned() )
      {
      // Address compare generated by loop strider does not have constant node,
      // thus should not fall into this category
      TR_ASSERT(!originalLimitNode->getType().isAddress(), "Counted loop with a constant address limit node is unexpected, test node is %p", getTestNode());
      // TODO: don't unroll such loops
      if(getTestChildType().isInt32())
         {
         unsigned utemp = originalLimitNode->getUnsignedInt() - adjustment;
         const_overflow = (isIncreasingLoop() && originalLimitNode->getUnsignedInt() < utemp) ||
                         (!isIncreasingLoop() && originalLimitNode->getUnsignedInt() > utemp);
         }
      else if(getTestChildType().isInt64())
         {
         uint64_t utemp = originalLimitNode->getUnsignedLongInt() - adjustment;
         const_overflow = (isIncreasingLoop() && originalLimitNode->getUnsignedLongInt() < utemp) ||
                         (!isIncreasingLoop() && originalLimitNode->getUnsignedLongInt() > utemp);
         }
      }
   //TR_ASSERT(!const_overflow, "Unroll count is bigger than upper bound\n");

   if (originalLimitNode->getOpCode().isLoadConst() && !const_overflow)
      {
      // Address compare generated by loop strider does not have constant node,
      // thus should not fall into this category
      TR_ASSERT(!originalLimitNode->getType().isAddress(), "Counted loop with a constant address limit node is unexpected, test node is %p", getTestNode());
      newLimitNode = TR::Node::create(branch, getConstOpCode(getTestChildType()));
      if (getTestChildType().isInt32())
         newLimitNode->setInt(originalLimitNode->getInt() - adjustment);
      else
         newLimitNode->setLongInt(originalLimitNode->getLongInt() - adjustment);
      branch->setAndIncChild(1, newLimitNode);
      }
   else
      {
      // Hoist computation outside loop, with a test for overflow
      //
      TR::TreeTop *storeToTempTT;
      TR::Node    *storeToTemp;
      TR::Node    *originalLimitClone = originalLimitNode->duplicateTree();
      TR::Node *tempValue = NULL;
      TR::SymbolReference *tempSym;

      if (getIndexType().isAddress())
         {
         tempSym = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address, true);
         tempSym->getSymbol()->castToInternalPointerAutoSymbol()->setPinningArrayPointer
             (_piv->getSymRef()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
         }
      else
         tempSym = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), getTestChildType().getDataType());

      TR::Node *constNode = TR::Node::create(branch, getConstOpCode(getTestChildType()), 0);
      TR::ILOpCodes storeOpCode;

//       if (getTestIsUnsigned())
//          {
//          if (getTestChildType().isInt32())
//             constNode->setInt(adjustment);
//          else
//             constNode->setLongInt(adjustment);

//          TR::ILOpCodes addOpCode = getTestChildType().isInt64() ? TR::lsub  : TR::isub;
//          tempValue = TR::Node::create(addOpCode, 2,originalLimitClone, constNode);

//          storeOpCode = getTestChildType().isInt64() ? TR::lstore : TR::istore;
//          }
//       else
//          {
         if (!is64BitComparison)
            constNode->setInt(-adjustment);
         else
            constNode->setLongInt(-adjustment);

         TR::ILOpCodes addOpCode = getIndexType().isAddress() ?
            (is64BitComparison ? TR::aladd : TR::aiadd) :
            (is64BitComparison ? TR::ladd  : TR::iadd)  ;
         tempValue = TR::Node::create(addOpCode, 2,originalLimitClone, constNode);

         if (getIndexType().isAddress())
            {
            tempValue->setPinningArrayPointer
                (_piv->getSymRef()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            tempValue->setIsInternalPointer(true);
            }

         storeOpCode = getIndexType().isAddress() ?
            TR::astore :
            (is64BitComparison ? TR::lstore : TR::istore);
         //         }

      storeToTemp = TR::Node::createWithSymRef(storeOpCode, 1, 1, tempValue, tempSym);
      storeToTempTT = TR::TreeTop::create(comp(), storeToTemp);
      newLimitNode  = TR::Node::createLoad(branch, tempSym);

      if (getIndexType().isAddress() || getTestChildType().isAddress())
         {
         TR_ASSERT(getIndexType().isAddress() && getTestChildType().isAddress(), "Test child type and IV type should both be address");
         }
      else if (newLimitNode->getType().isAggregate() && !getTestChildType().isAggregate())
         {
         TR::ILOpCodes o2xOp = TR::ILOpCode::getProperConversion(newLimitNode->getDataType(), getTestChildType().getDataType(), true);
         TR_ASSERT(o2xOp != TR::BadILOp,"could not find conv op for %s -> %s conversion\n",TR::DataType::getName(newLimitNode->getDataType()),TR::DataType::getName(getTestChildType().getDataType()));
         newLimitNode = TR::Node::create(o2xOp, 1, newLimitNode);
         }

      branch->setAndIncChild(1, newLimitNode);

      TR::TreeTop *lastTree = pBlock->getLastRealTreeTop();
      if (lastTree->getNode()->getOpCode().isBranch())
         {
         // Splice the block into two, putting the goto in the next block
         TR_ASSERT(lastTree->getNode()->getOpCodeValue() == TR::Goto, "expecting canon loops");

         TR::Block *newBlock = TR::Block::createEmptyBlock(lastTree->getNode(), comp(), pBlock->getFrequency(), pBlock);
         lastTree->getPrevTreeTop()->join(lastTree->getNextTreeTop());
         newBlock->append(lastTree);
         newBlock->getExit()->join(pBlock->getNextBlock()->getEntry());
         pBlock->getExit()->join(newBlock->getEntry());

         _cfg->addNode(newBlock);

         TR_BlockStructure *newBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newBlock->getNumber(), newBlock);
         TR_StructureSubGraphNode *newSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newBlockStructure);
         parent->addSubNode(newSubNode);

         TR::CFGEdge *edgeToRemove = pBlock->getSuccessors().front();

         _cfg->addEdge(TR::CFGEdge::createEdge(pBlock,  newBlock, trMemory()));
         TR::CFGEdge::createEdge(predNode,  newSubNode, trMemory());
         _cfg->addEdge(TR::CFGEdge::createEdge(newBlock, loop->getEntryBlock(), trMemory()));
         TR::CFGEdge::createEdge(newSubNode,  loopNode, trMemory());

         _cfg->removeEdge(edgeToRemove);
         parent->removeEdge(predBlock, loopNode->getStructure());
         }

      // create the n, n - 3 test (overflow test)
      TR::ILOpCodes opCode;
      if (getIndexType().isAddress())
         {
         opCode = isIncreasingLoop() ? TR::ifacmplt : TR::ifacmpgt;
         }
      else
         {
         if (getTestIsUnsigned())
            {
            if (getTestChildType().isInt32())
               opCode = isIncreasingLoop() ? TR::ifiucmplt : TR::ifiucmpgt;
            else
               opCode = isIncreasingLoop() ? TR::iflucmplt : TR::iflucmpgt;
            }
         else
            {
            if (getTestChildType().isInt32())
               opCode = isIncreasingLoop() ? TR::ificmplt : TR::ificmpgt;
            else
               opCode = isIncreasingLoop() ? TR::iflcmplt : TR::iflcmpgt;
            }
         }

      TR::Node *c2 = tempValue;
      pBlock->append(storeToTempTT);
      pBlock->append(TR::TreeTop::create(comp(), TR::Node::createif(opCode,
                                                      originalLimitClone, c2,
                                                      spillBlock->getEntry())));

      _overflowTestBlock = pBlock;

      _cfg->addEdge(TR::CFGEdge::createEdge(pBlock,  spillBlock, trMemory()));
      TR::CFGEdge::createEdge(predNode,  _spillNode, trMemory());
      }

   originalLimitNode->recursivelyDecReferenceCount();

   if (trace())
      {
      comp()->dumpMethodTrees("\nbefore adding the loopiter test");
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      }

   // add the minimum loop iteration test after the overflow test
   //
   if (1) /// moveEntryNode
      {
      TR::ILOpCodes opCode = _branchToExit ?
         branch->getOpCodeValue() : /* use same polarity */
         branch->getOpCode().getOpCodeForReverseBranch();

      TR::TreeTop *lastTree = pBlock->getLastRealTreeTop();
      TR::Node *firstChild = TR::Node::createLoad(branch, _piv->getSymRef());

      if (getIndexType().isAddress() || getTestChildType().isAddress())
         {
         TR_ASSERT(getIndexType().isAddress() && getTestChildType().isAddress(), "Test child type and IV type should both be address");
         }
      else if (firstChild->getType().isAggregate() && !getTestChildType().isAggregate())
         {
         TR::ILOpCodes o2xOp = TR::ILOpCode::getProperConversion(firstChild->getDataType(), getTestChildType().getDataType(), true);
         TR_ASSERT(o2xOp != TR::BadILOp,"could not find conv op for %s -> %s conversion\n",TR::DataType::getName(firstChild->getDataType()),TR::DataType::getName(getTestChildType().getDataType()));
         firstChild = TR::Node::create(o2xOp, 1, firstChild);
         }
      else if (getIndexType().isInt16())
         {
         if (getTestChildType().isInt64())
            firstChild = TR::Node::create(TR::s2l, 1, firstChild);
         else if (getTestChildType().isInt32())
            firstChild = TR::Node::create(TR::s2i, 1, firstChild);
         }
      else if (getIndexType().isInt8())
         {
         if (getTestChildType().isInt64())
            firstChild = TR::Node::create(TR::b2l, 1, firstChild);
         else if (getTestChildType().isInt32())
            firstChild = TR::Node::create(TR::b2i, 1, firstChild);
         else if (getTestChildType().isInt16())
            firstChild = TR::Node::create(TR::b2s, 1, firstChild);
         }
      else if (getIndexType().isInt32() && getTestChildType().isInt64())
         {
         firstChild = TR::Node::create(TR::i2l, 1, firstChild);
         }
      else if (getIndexType().isInt64() && getTestChildType().isInt32())
         {
         firstChild = TR::Node::create(TR::l2i, 1, firstChild);
         }


      TR::Node *dupNewLimit = newLimitNode->duplicateTree();

      if (loop->getPrimaryInductionVariable() &&
          loop->getPrimaryInductionVariable()->usesUnchangedValueInLoopTest())
         {
         //getLoopStride is +/- depending on the direction of the loop
         //
         int32_t incrVal = getLoopStride();
         // always use add for addresses, so the stride needs to be negated
         // because we're using sub below
         //
         bool isAddress = getIndexType().isAddress();
         if (isAddress)
            incrVal = -incrVal;
         TR::ILOpCodes subOp;
         TR::Node *constNode = NULL;
         if (is64BitComparison)
            {
            subOp = isAddress ? TR::aladd : TR::lsub;
            constNode = TR::Node::lconst(newLimitNode, (int64_t) incrVal);
            }
         else
            {
            subOp = isAddress ? TR::aiadd : TR::isub;
            constNode = TR::Node::iconst(newLimitNode, incrVal);
            }

         dupNewLimit = TR::Node::create(subOp, 2, dupNewLimit, constNode);
         }
      TR::TreeTop *iterGuard =
         TR::TreeTop::create(comp(),
            TR::Node::createif(opCode, firstChild, dupNewLimit, spillBlock->getEntry()));

      if (lastTree->getNode()->getOpCodeValue() != TR::Goto)
         {
         // append an if block to the loop invariant block
         //
         TR::Block *newBlock = TR::Block::createEmptyBlock(lastTree->getNode(), comp(), pBlock->getFrequency(), pBlock);
         newBlock->append(iterGuard);

         TR::Block *nextBlock = pBlock->getNextBlock();
         newBlock->getExit()->join(nextBlock->getEntry());
         pBlock->getExit()->join(newBlock->getEntry());

         _cfg->addNode(newBlock);
         TR_BlockStructure *newBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newBlock->getNumber(), newBlock);
         TR_StructureSubGraphNode *newSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newBlockStructure);
         parent->addSubNode(newSubNode);

         TR::CFGEdge *edgeToRemove = NULL;
         for (auto edge = pBlock->getSuccessors().begin(); edge != pBlock->getSuccessors().end(); ++edge)
            {
            if ((*edge)->getTo()->getNumber() != spillBlock->getNumber())
               {
               edgeToRemove = *edge;
               break;
               }
            }

         _cfg->addEdge(TR::CFGEdge::createEdge(pBlock,  newBlock, trMemory()));
         TR::CFGEdge::createEdge(predNode,  newSubNode, trMemory());
         _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  spillBlock, trMemory()));
         TR::CFGEdge::createEdge(newSubNode,  _spillNode, trMemory());
         _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  nextBlock, trMemory()));
         TR::CFGEdge::createEdge(newSubNode, parent->findSubNodeInRegion(nextBlock->getNumber()), trMemory());

         _cfg->removeEdge(edgeToRemove);
         parent->removeEdge(predBlock, parent->findSubNodeInRegion(nextBlock->getNumber())->getStructure());
         _loopIterTestBlock = newBlock;
         }
      else
         {
         // loop invariant block ends with a goto
         // create a new block - move the goto in the new block and add append an if to the loop inv block
         //
         TR::Block *newBlock = pBlock->split(lastTree, _cfg);
         pBlock->append(iterGuard);

         TR_BlockStructure *newBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newBlock->getNumber(), newBlock);
         TR_StructureSubGraphNode *newSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newBlockStructure);
         parent->addSubNode(newSubNode);

         _cfg->addEdge(TR::CFGEdge::createEdge(pBlock,  spillBlock, trMemory()));
         TR::CFGEdge::createEdge(predNode,  _spillNode, trMemory());

         // TR::Block::split does not know how to fix the structure edges, so do that ourselves
         TR::CFGEdge::createEdge(predNode,  newSubNode, trMemory());
         TR::CFGEdge::createEdge(newSubNode,  loopNode, trMemory());

         parent->removeEdge(predBlock, loop);
         _loopIterTestBlock = pBlock;
         }

      if (trace())
         {
         comp()->dumpMethodTrees("\nafter adding loopiter test");
         getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
         }
      }
   }

// if an internalpointer is going to be created, ensure it
// does not exceed the limits
bool TR_LoopUnroller::isInternalPointerLimitExceeded()
   {
   return _spillLoopRequired && isCountedLoop() && getIndexType().isAddress() &&
      comp()->getSymRefTab()->getNumInternalPointers() >= comp()->maxInternalPointers();
   }

void TR_LoopUnroller::modifyOriginalLoop(TR_RegionStructure *loop, TR_StructureSubGraphNode *branchNode)
   {
   int32_t origExitNum = 0;
   if (1)
      {
      ListIterator<TR::CFGEdge> it(&loop->getExitEdges());
      for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
         {
         if (edge->getFrom()->asStructureSubGraphNode() == branchNode)
            {
            origExitNum = edge->getTo()->getNumber();
            break;
            }
         }
      }

   if (!isCountedLoop())
      {
      TR_ASSERT(_unrollKind != CompleteUnroll, "Non induction loops cannot be unrolled completely");
      TR_ASSERT(_spillLoopRequired == false, "Non induction loops must be unrolled without a spill loop");
      }

   TR::CFGEdge *edge;
   //Redirect all the local predecessors of the entry to the 'firstEntryNode'
   for (auto edge = loop->getEntry()->getPredecessors().begin(); edge != loop->getEntry()->getPredecessors().end();)
      {
      TR::CFGEdge* current = *(edge++);
      if (!loop->isExitEdge(current))
         {
         TR_StructureSubGraphNode *predNode = toStructureSubGraphNode(current->getFrom());
         if (predNode->getNumber() < _numNodes)
            {
            if (shouldConnectToNextIteration(predNode, loop))
               {
               addEdgeAndFixEverything(loop, current, predNode, _firstEntryNode, true, true, true, BackEdgeFromPrevGeneration);
               loop->removeEdge(predNode->getStructure(), loop->getEntry()->getStructure());
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "Redirecting cold edge from block_%d to unrolled loop header rather than going to the next iteration\n", predNode->getNumber());
               addEdgeAndFixEverything(loop, current, predNode, loop->getEntry(), true, false, true, BackEdgeToEntry);
               }
            }
         }
      }

   //Find the node corresponding to loop
   TR_RegionStructure *parent = loop->getParent()->asRegion();
   TR_RegionStructure::Cursor nodeIt(*parent);
   TR_StructureSubGraphNode *loopNode = NULL;
   for (TR_StructureSubGraphNode *node = nodeIt.getFirst(); node; node = nodeIt.getNext())
      if (node->getStructure() == loop)
         {
         loopNode = node;
         break;
         }
   TR_ASSERT(loopNode != NULL, "loopNode not found\n");

   TR::Block *branchBlock = branchNode->getStructure()->asBlock()->getBlock();
   TR::Node *branch = branchBlock->getLastRealTreeTop()->getNode();

   // find the insertion point of the unrolled bodies
   //
   TR::TreeTop *oldLoopEntryTree = NULL; //loop->getEntryBlock()->getEntry();

   if (_unrollKind == SPMDKernel)
      {
      if (_spillLoopRequired)
         modifyBranchTree(loop, loopNode, branchNode);
      }
   else
   if (isCountedLoop() && _unrollKind != CompleteUnroll)  /*GGLU*/
      {
      //The branch condition is modified if there is going to be a spill loop.
      if (_spillLoopRequired)
         {
         modifyBranchTree(loop, loopNode, branchNode);
         }

      // IVA does not create PIV for ifacmpne and ifacmpeq, thus loops with ifacmpne are not counted loop
      // and they won't enter this if so there is no need to check for ifacmpne
      TR_ASSERT(branch->getOpCodeValue() != TR::ifacmpeq && branch->getOpCodeValue() != TR::ifacmpne, "Counted loop with branch node %p of opcode %s is unexpected\n", branch, branch->getOpCode().getName());
      //convert the != condition to a '<' or '>' condition.
      if (branch->getOpCodeValue() == TR::ificmpne)
         {
         if (isIncreasingLoop())
            TR::Node::recreate(branch, TR::ificmplt);
         else
            TR::Node::recreate(branch, TR::ificmpgt);
         }
      else if (branch->getOpCodeValue() == TR::iflcmpne)
         {
         if (isIncreasingLoop())
            TR::Node::recreate(branch, TR::iflcmplt);
         else
            TR::Node::recreate(branch, TR::iflcmpgt);
         }
      }
   else if (_unrollKind == CompleteUnroll) /*GGLU*/
      {
      //Complete Unroll.  Remove the branch
      //
      TR::Block *destBlock = branch->getBranchDestination()->getNode()->getBlock();
      TR_Structure *destStruct = destBlock->getStructureOf();
      if (loop->contains(destStruct, loop->getParent()))
         {
         TR::Node *gotoNode = TR::Node::create(branch, TR::Goto);
         TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
         gotoNode->setBranchDestination(destBlock->getEntry());
         gotoNode->setLocalIndex(CREATED_BY_GLU);

         TR::TransformUtil::removeTree(comp(), branchBlock->getLastRealTreeTop());
         branchBlock->append(gotoTree);
         oldLoopEntryTree = branchBlock->getExit()->getNextTreeTop();

         //Find exitNode X.
         //
         TR::CFGNode *exitNode = NULL;
         for (auto edge = branchNode->getSuccessors().begin(); (edge != branchNode->getSuccessors().end()) && exitNode == NULL; ++edge)
            {
            if (loop->isExitEdge(*edge))
               exitNode = (*edge)->getTo();
            }

         //Remove B->X*
         //
         removeExternalEdge(loop, branchNode, exitNode->getNumber());
         TR_StructureSubGraphNode *concreteExitNode =
            findNodeInHierarchy(loop->getParent()->asRegion(), exitNode->getNumber());
         TR::Block *blockX = getEntryBlockNode(concreteExitNode)
            ->getStructure()->asBlock()->getBlock();
         _cfg->removeEdge(branchBlock, blockX);
         }
      else
         {
         // FIXME: this is wrong (unreachable because of hack in heuristic)
         //
         // remove B->X*, replace the branch with a goto to the first entry node
         // create edges for B->F
         //
         TR_ASSERT(0, "should not reach here");
         branchBlock->removeBranch(comp());
         }
      }


   //FIXME: we can improve the following a bit
   //    if (isCountedLoop && !_completeUnroll && !_spillLoopRequired)
   //       {
   //       //Adjust the induction variable information to include the new information
   //       _indVar->setIncr(new (trHeapMemory()) TR::VPIntConst(_loopStride * (1 + _unrollCount)));
   //       }
   //    else
   //       {
   if (_unrollKind != SPMDKernel)
   loop->clearInductionVariables();
   //      }

   /***************************************************************************/
   /* Connect the spill Loop                                                  */
   /***************************************************************************/

   if (_spillLoopRequired)
      {
      // First of all find the exitEdge in L originating from B (B->X*)
      ListIterator<TR::CFGEdge> it(&loop->getExitEdges());
      TR::CFGEdge *exitEdge = NULL;
      for (edge = it.getFirst(); edge && !exitEdge; edge = it.getNext())
         {
         TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(edge->getFrom());
         if (fromNode == branchNode) //this is the exit edge from the branch
            exitEdge = edge;
         }

      // Find the exit node in L
      TR_StructureSubGraphNode *exitNode =
         findNodeInHierarchy(parent, toStructureSubGraphNode(exitEdge->getTo())->getNumber());

      // Look at the exit edges of S and put the representatives in the parent region
      // as needed
      //
      it.set(&_spillNode->getStructure()->asRegion()->getExitEdges());
      for (edge = it.getFirst(); edge; edge = it.getNext())
         {
         TR_StructureSubGraphNode *destNode = toStructureSubGraphNode(edge->getTo());
         int32_t destNum = destNode->getNumber();
         TR_StructureSubGraphNode *concreteNode = findNodeInHierarchy(parent, destNum);

         if (!edgeAlreadyExists(_spillNode, destNum))
            {
            if (concreteNode->getStructure()->getParent() == parent)
               TR::CFGEdge::createEdge(_spillNode,  concreteNode, trMemory());
            else
               parent->addExitEdge(_spillNode, destNum);
            }
         }

      // Create an iteration test
      if (1)
         {
         TR_RegionStructure *spillLoop = _spillNode->getStructure()->asRegion();
         TR::TreeTop     *branchTreeTop = _spillBranchBlock->getLastRealTreeTop();
         TR::Block       *origExitBlock;

         // Find the orig exit block
         //
         if (1)
            {
            TR_StructureSubGraphNode *node = findNodeInHierarchy(loop, origExitNum);

            if (node->getStructure()->asBlock())
               origExitBlock = node->getStructure()->asBlock()->getBlock();
            else
               origExitBlock = node->getStructure()->asRegion()->getEntryBlock();
            }

         TR::Block          *newIfBlock = TR::Block::createEmptyBlock(branchTreeTop->getNode(), comp(), origExitBlock->getFrequency(), origExitBlock);
         TR_BlockStructure *newIfBlockStructure;
         TR_StructureSubGraphNode *newIfSubNode;

         // Duplicate the branch tree from the spill entry block and place it correctly in the trees
         // connect this block up properly in the parent region
         //
         if (1)
            {
            TR::Block *branchDestination = branchTreeTop->getNode()->getBranchDestination()->getNode()->getBlock();
            TR::TreeTop *newIfTree = 0;
            //TR::TreeTop::create(comp(), branchTreeTop->getNode()->duplicateTree());

            if (1)
               {
               TR::Node *ifNode = branchTreeTop->getNode();
               TR::Node *c1 = TR::Node::createLoad(ifNode, _piv->getSymRef());
               if (getIndexType().isAddress() || getTestChildType().isAddress())
                  {
                  TR_ASSERT(getIndexType().isAddress() && getTestChildType().isAddress(), "Test child type and IV type should both be address");
                  }
               else if (c1->getType().isAggregate() && !getTestChildType().isAggregate())
                  {
                  TR::ILOpCodes o2xOp = TR::ILOpCode::getProperConversion(c1->getDataType(), getTestChildType().getDataType(), true);
                  TR_ASSERT(o2xOp != TR::BadILOp,"could not find conv op for %s -> %s conversion\n",TR::DataType::getName(c1->getDataType()),TR::DataType::getName(getTestChildType().getDataType()));
                  c1 = TR::Node::create(o2xOp, 1, c1);
                  }
               else if (getIndexType().isInt8())
                  {
                  if (getTestChildType().isInt64())
                     c1 = TR::Node::create(TR::b2l, 1, c1);
                  else if (getTestChildType().isInt32())
                     c1 = TR::Node::create(TR::b2i, 1, c1);
                  }
               else if (getIndexType().isInt16())
                  {
                  if (getTestChildType().isInt64())
                     c1 = TR::Node::create(TR::s2l, 1, c1);
                  else if (getTestChildType().isInt32())
                     c1 = TR::Node::create(TR::s2i, 1, c1);
                  }
               else if (getIndexType().isInt32() && getTestChildType().isInt64())
                  {
                  c1 = TR::Node::create(TR::i2l, 1, c1);
                  }
               else if (getIndexType().isInt64() && getTestChildType().isInt32())
                  {
                  c1 = TR::Node::create(TR::l2i, 1, c1);
                  }

               TR::Node *boundNode = ifNode->getSecondChild()->duplicateTree();
               if (loop->getPrimaryInductionVariable() &&
                     loop->getPrimaryInductionVariable()->usesUnchangedValueInLoopTest())
                  {
                  TR::ILOpCodes addOp;
                  bool isAddress = getIndexType().isAddress();
                  TR::Node *constNode = NULL;
                  bool is64BitComparison = TR::DataType::getSize(getTestChildType()) == 8 ? true : false;
                  if (!getTestIsUnsigned())
                     {
                     // For signed comparison: LHS op RHS:
                     //     LHS: iv
                     //     RHS: loopStride+bound
                     if (is64BitComparison)
                        {
                        addOp = isAddress ? TR::aladd : TR::ladd;
                        constNode = TR::Node::lconst(boundNode, (int64_t)getLoopStride());
                        }
                     else
                        {
                        addOp = isAddress ? TR::aiadd : TR::iadd;
                        constNode = TR::Node::iconst(boundNode, getLoopStride());
                        }
                     boundNode = TR::Node::create(addOp, 2, boundNode, constNode);
                     }
                  else
                     {
                     // For unsigned comparison: LHS op RHS:
                     //     LHS: iv-loopStride
                     //     RHS: bound
                     // i.e. unsignedness dictates that we can't unconditionally move
                     // loopStride to RHS as in signed compare case
                     if (is64BitComparison)
                        {
                        addOp = isAddress ? TR::aladd : TR::ladd;
                        constNode = TR::Node::lconst(boundNode, -(int64_t)getLoopStride());
                        }
                     else
                        {
                        addOp = isAddress ? TR::aiadd : TR::iadd;
                        constNode = TR::Node::iconst(boundNode, - getLoopStride());
                        }
                     c1 = TR::Node::create(addOp, 2, c1, constNode);
                     }
                  }
               newIfTree = TR::TreeTop::create(comp(),
                  TR::Node::createif(ifNode->getOpCodeValue(),
                                    c1, boundNode,
                                    ifNode->getBranchDestination()));
               }

            newIfBlock->append(newIfTree);
            _cfg->addNode(newIfBlock);
            newIfBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newIfBlock->getNumber(), newIfBlock);
            newIfSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newIfBlockStructure);
            parent->addSubNode(newIfSubNode);

            TR::Node *gotoNode  = TR::Node::create(newIfTree->getNode(), TR::Goto);
            gotoNode->setBranchDestination(spillLoop->getEntryBlock()->getEntry());
            TR::Block *gotoBlock = TR::Block::createEmptyBlock(gotoNode, comp(), origExitBlock->getFrequency(), origExitBlock); //spillLoop->getEntryBlock()->getFrequency());
            gotoBlock->append(TR::TreeTop::create(comp(), gotoNode));
            _cfg->addNode(gotoBlock);
            _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock, spillLoop->getEntryBlock(), trMemory()));

            TR_StructureSubGraphNode *gotoSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode
               (new (_cfg->structureRegion()) TR_BlockStructure(comp(), gotoBlock->getNumber(), gotoBlock));
            parent->addSubNode(gotoSubNode);

            if (spillLoop->contains(branchDestination->getStructureOf(), parent))
               {
               TR::Node::recreate(newIfTree->getNode(),
                  newIfTree->getNode()->getOpCode().getOpCodeForReverseBranch());
               newIfTree->getNode()->setBranchDestination(origExitBlock->getEntry());
               }

            // Find the last treeTop and place these blocks after that
            if (1)
               {
               TR::TreeTop *endTree = comp()->getMethodSymbol()->getLastTreeTop();
               endTree->join(newIfBlock->getEntry());
               newIfBlock->getExit()->join(gotoBlock->getEntry());
               gotoBlock->getExit()->join(0);
               }

            _cfg->addEdge(TR::CFGEdge::createEdge(newIfBlock,  origExitBlock, trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(newIfBlock,  gotoBlock, trMemory()));

            TR_StructureSubGraphNode *otherSuccSubNode = parent->findSubNodeInRegion(origExitBlock->getNumber());
            if (otherSuccSubNode)
               TR::CFGEdge::createEdge(newIfSubNode,  otherSuccSubNode, trMemory());
            else
               parent->addExitEdge(newIfSubNode, origExitBlock->getNumber(), false);
            TR::CFGEdge::createEdge(newIfSubNode,  gotoSubNode, trMemory());
            TR::CFGEdge::createEdge(gotoSubNode,  _spillNode, trMemory());
            }

         // Add L->I
         TR::CFGEdge::createEdge(loopNode,  newIfSubNode, trMemory()); // the CFG is already correct

         // Remove B->X* in L
         removeExternalEdge(loop, branchNode, exitNode->getNumber());

         // Add B->I in L
         addEdgeForSpillLoop(loop, exitEdge, branchNode, newIfSubNode, true);
         }

      // Find the edge corresponding the exitEdge in the successors of the loop node (L->X?)
      TR::CFGEdge *correspondingExitEdge = NULL;
      for (auto edge = loopNode->getSuccessors().begin(); (edge != loopNode->getSuccessors().end()) && !correspondingExitEdge; ++edge)
         if (toStructureSubGraphNode((*edge)->getTo())->getNumber() == exitNode->getNumber())
            correspondingExitEdge = *edge;

      // Remove L->X in P if needed
      if (1)
         {
         ListIterator<TR::CFGEdge> eit(&loop->getExitEdges());
         TR::CFGEdge *e;
         for (e = eit.getFirst(); e; e = eit.getNext())
            {
            if (e->getTo()->getNumber() == exitNode->getNumber())
               break;
            }
         if (!e) // there do not remain an edge in L going to X
            {
            if (exitNode->getStructure()->getParent() == parent)
               {
               bool found = (std::find(loopNode->getSuccessors().begin(), loopNode->getSuccessors().end(), correspondingExitEdge) != loopNode->getSuccessors().end());
               loopNode->getSuccessors().remove(correspondingExitEdge);
               TR_ASSERT(found, "GLU: bad structure while removing edge");
               found = (std::find(exitNode->getPredecessors().begin(), exitNode->getPredecessors().end(), correspondingExitEdge) != exitNode->getPredecessors().end());
               exitNode->getPredecessors().remove(correspondingExitEdge);
               TR_ASSERT(found, "GLU: bad structure while removing edge");
               }
            else
               removeExternalEdge(parent, loopNode, exitNode->getNumber());
            }
         }

      }

   if (0 && _spillLoopRequired)
      {
      //Let
      //B = branch block, L = loop region, X = exit destination,
      //P = L's parent region, S = spill region
      //define e(A) = entry block of strucutre A
      //Notation P->Q denotes an edge from P to Q
      //         P->Q* denotes an exit edge

      //First of all, find the exitEdge in L originating from the B.
      ListIterator<TR::CFGEdge> it(&loop->getExitEdges());
      TR::CFGEdge *exitEdge = NULL;
      for (edge = it.getFirst(); edge && !exitEdge; edge = it.getNext())
         {
         TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(edge->getFrom());
         if (fromNode == branchNode) //this is the exit edge from the branch
            exitEdge = edge;
         }

      //Find the exit node
      TR_StructureSubGraphNode *exitNode =
         findNodeInHierarchy(parent, toStructureSubGraphNode(exitEdge->getTo())->getNumber());

      //Find the edge corresponding to exitEdge in the successors of the loop node
      TR::CFGEdge *correspondingExitEdge = NULL;
      for (auto edge = loopNode->getSuccessors().begin(); (edge != loopNode->getSuccessors().end()) && !correspondingExitEdge; ++edge)
         if (toStructureSubGraphNode((*edge)->getTo())->getNumber() == exitNode->getNumber())
            correspondingExitEdge = *edge;

      //Add L->S in P
      TR::CFGEdge::createEdge(loopNode,  _spillNode, trMemory());

      //Look at exit edges of S, and add appropriate edges as needed
      it.set(&_spillNode->getStructure()->asRegion()->getExitEdges());
      for (edge = it.getFirst(); edge; edge = it.getNext())
         {
         TR_StructureSubGraphNode *destNode = toStructureSubGraphNode(edge->getTo());
         int32_t destNum = destNode->getNumber();
         TR_StructureSubGraphNode *concreteNode = findNodeInHierarchy(parent, destNum);

         //Don't add edge if it already exists
         if (!edgeAlreadyExists(_spillNode, destNum))
            {
            if (concreteNode->getStructure()->getParent() == parent)
               TR::CFGEdge::createEdge(_spillNode,  concreteNode, trMemory());
            else
               parent->addExitEdge(_spillNode, destNum);
            }
         }

      //Remove B->X* in L
      removeExternalEdge(loop, branchNode, exitNode->getNumber());

      //Add B->S* in L
      addEdgeForSpillLoop(loop, exitEdge, branchNode, _spillNode, true);


      if (1) //force block scope
         {
         ListIterator<TR::CFGEdge> eit(&loop->getExitEdges());
         TR::CFGEdge *e;
         for (e = eit.getFirst(); e; e = eit.getNext())
            {
            if (e->getTo()->getNumber() == exitNode->getNumber())
               break;
            }

         if (!e) // there do not remain an edges in L going to X
            {
            //Remove L->X or L->X* in P
            if (exitNode->getStructure()->getParent() == parent)
               {

               bool found = (std::find(loopNode->getSuccessors().begin(), loopNode->getSuccessors().end(), correspondingExitEdge) != loopNode->getSuccessors().end());
               loopNode->getSuccessors().remove(correspondingExitEdge);
               TR_ASSERT(found, "GLU: bad structure while removing edge");
               found = (std::find(exitNode->getPredecessors().begin(), exitNode->getPredecessors().end(), correspondingExitEdge) != exitNode->getPredecessors().end());
               exitNode->getPredecessors().remove(correspondingExitEdge);
               TR_ASSERT(found,
            		   "GLU: bad structure while removing edge");
               }
            else
               {
               removeExternalEdge(parent, loopNode, exitNode->getNumber());
               }
            }
         }

      // Swing all the blocks that have been scheduled to be swung.
      processSwingQueue();

      static       char *skipIt = feGetEnv("TR_NoGLUProper");
      if (!skipIt)
         {
         if (trace())
            {
            traceMsg(comp(), "\nbefore n,n-3 fix:\n\n", loop->getNumber());
            getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
            getDebug()->print(comp()->getOutFile(), _cfg);
            comp()->dumpMethodTrees("Tree tops right before n,n-3 fix:");
            }

         TR_RegionStructure *spillLoop  = _spillNode->getStructure()->asRegion();
         int32_t     oldSpillNum        = _spillNode->getNumber();
         TR::Block   *oldSpillEntryBlock = spillLoop->getEntryBlock();
         TR::TreeTop *branchTreeTop      = oldSpillEntryBlock->getLastRealTreeTop();
         TR_BlockStructure
            *newIfBlockStructure;
         TR_StructureSubGraphNode
            *newIfSubNode;
         TR::Block   *origExitBlock;
         TR::Node    *gotoNode;
         TR::Block   *gotoBlock;

         // Find the orig exit block
         //
         if (1)
            {
            TR_StructureSubGraphNode *node = findNodeInHierarchy(loop, origExitNum);

            if (node->getStructure()->asBlock())
               origExitBlock = node->getStructure()->asBlock()->getBlock();
            else
               origExitBlock = node->getStructure()->asRegion()->getEntryBlock();
            }

          TR::Block   *newIfBlock         = TR::Block::createEmptyBlock(branchTreeTop->getNode(), comp(), origExitBlock->getFrequency(), origExitBlock);

         // Duplicate the branch tree from the spill entry block and place it correctly in the trees
         // connect this block up properly in the parent region
         //
         if (1)
            {
            TR::Block   *branchDestination  = branchTreeTop->getNode()->getBranchDestination()->getNode()->getBlock();
            TR::TreeTop *newIfTree = TR::TreeTop::create(comp(), branchTreeTop->getNode()->duplicateTree());
            newIfBlock->append(newIfTree);

            _cfg->addNode(newIfBlock);
            newIfBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newIfBlock->getNumber(), newIfBlock);
            newIfSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newIfBlockStructure);
            parent->addSubNode(newIfSubNode);

            gotoNode  = TR::Node::create(newIfTree->getNode(), TR::Goto);
            gotoBlock = TR::Block::createEmptyBlock(gotoNode, comp(), newIfBlock->getFrequency(), newIfBlock);
            gotoBlock->append(TR::TreeTop::create(comp(), gotoNode));
            _cfg->addNode(gotoBlock);
            TR_StructureSubGraphNode *gotoSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode
               (new (_cfg->structureRegion()) TR_BlockStructure(comp(), gotoBlock->getNumber(), gotoBlock));
            parent->addSubNode(gotoSubNode);

            if (spillLoop->contains(branchDestination->getStructureOf(), parent))
               {
                TR::Node::recreate(newIfTree->getNode(),
                   newIfTree->getNode()->getOpCode().getOpCodeForReverseBranch());
                newIfTree->getNode()->setBranchDestination(origExitBlock->getEntry());
               }

            TR::Block *prevBlock = oldSpillEntryBlock->getPrevBlock();
            prevBlock->getExit()->join(newIfBlock->getEntry());
            newIfBlock->getExit()->join(gotoBlock->getEntry());
            gotoBlock->getExit()->join(oldSpillEntryBlock->getEntry());

            _cfg->addEdge(TR::CFGEdge::createEdge(newIfBlock,  origExitBlock, trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(newIfBlock,  gotoBlock, trMemory()));
            //_cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  oldSpillEntryBlock, trMemory()));

            TR_StructureSubGraphNode *otherSuccSubNode = parent->findSubNodeInRegion(origExitBlock->getNumber());
            if (otherSuccSubNode)
               TR::CFGEdge::createEdge(newIfSubNode,  otherSuccSubNode, trMemory());
            else
               parent->addExitEdge(newIfSubNode, origExitBlock->getNumber(), false);
            TR::CFGEdge::createEdge(newIfSubNode,  gotoSubNode, trMemory());
            TR::CFGEdge::createEdge(gotoSubNode,   _spillNode, trMemory());
            }

         // Modify entry of the spill loop to be the the original correct entry
         //
         if (1)
            {
            TR_StructureSubGraphNode *origEntryNode;
            TR::CFGEdgeList& list = _spillNode->getStructure()->asRegion()->getEntry()->getSuccessors();
            for (auto edge = list.begin(); edge != list.end(); ++edge)
               {
               TR_StructureSubGraphNode *to = (*edge)->getTo()->asStructureSubGraphNode();
               if (to->getStructure())
                  {
                  origEntryNode = to;
                  break;
                  }
               }

            int32_t origNum = origEntryNode->getNumber();
            _spillNode->getStructure()->asRegion()->setEntry(origEntryNode);
            _spillNode->setNumber(origNum);
            _spillNode->getStructure()->setNumber(origNum);
            }

         // change the B->S* to become B->I* and
         //
         if (1)
            {
            TR_StructureSubGraphNode *fromSubNode;
            ListIterator<TR::CFGEdge> it(&loop->getExitEdges());
            for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
               {
               if (edge->getTo()->getNumber() == oldSpillNum)
                  {
                  fromSubNode = edge->getFrom()->asStructureSubGraphNode();
                  break;
                  }
               }

            TR::Block *fromBlock = fromSubNode->getStructure()->asBlock()->getBlock();
            TR::TreeTop *lastTree = fromBlock->getLastRealTreeTop();

            if (lastTree->getNode()->getOpCode().isBranch() &&
                lastTree->getNode()->getBranchDestination() == oldSpillEntryBlock->getEntry())
               {
               lastTree->getNode()->setBranchDestination(newIfBlock->getEntry());
               }

            _cfg->addEdge(TR::CFGEdge::createEdge(fromBlock,  newIfBlock, trMemory()));
            loop->addExitEdge(fromSubNode, newIfBlock->getNumber());
            _cfg->removeEdge(fromBlock, oldSpillEntryBlock);
            removeExternalEdge(loop, fromSubNode, oldSpillNum);
            }

         // add L->I
         TR::CFGEdge::createEdge(loopNode,  newIfSubNode, trMemory()); // the CFG is already fine

         // Remove L->S'
         if (1)
            {
            for (auto edge = loopNode->getSuccessors().begin(); edge != loopNode->getSuccessors().end(); ++edge)
               {
               if ((*edge)->getTo() == _spillNode)
                  {
                  parent->removeEdge(*edge, false);
                  break;
                  }
               }
            }

         TR::Block *newEntryBlock = spillLoop->getEntryBlock();
         gotoNode->setBranchDestination(newEntryBlock->getEntry());
         _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  newEntryBlock, trMemory()));

         /// Fixup the overflow test
         if (_overflowTestBlock)
            {
            _overflowTestBlock->getLastRealTreeTop()->getNode()->setBranchDestination
               (newEntryBlock->getEntry());
            TR_StructureSubGraphNode *overflowTestNode = parent->findSubNodeInRegion
               (_overflowTestBlock->getNumber());

            _cfg->addEdge(TR::CFGEdge::createEdge(_overflowTestBlock,  newEntryBlock, trMemory()));
            _cfg->removeEdge(_overflowTestBlock, oldSpillEntryBlock);
            // no need to fixup structure edges - they are already fine!!!
            }

         /// fixup the loop iteration test
         if (1 && _loopIterTestBlock)
            {
            TR::Block *newEntryBlock = spillLoop->getEntryBlock();
            _loopIterTestBlock->getLastRealTreeTop()->getNode()->setBranchDestination
               (newEntryBlock->getEntry());
            _cfg->removeEdge(_loopIterTestBlock, oldSpillEntryBlock);
            _cfg->addEdge(TR::CFGEdge::createEdge(_loopIterTestBlock,  newEntryBlock, trMemory()));
            }

         //printf("--glui-- in %s\n", signature(comp()->getCurrentMethod()));
         }
      }

   // Swing all the blocks that have been scheduled to be swung.
   processSwingQueue();

   if (trace())
      {
      traceMsg(comp(), "\nstructure right before the new stuff:\n\n", loop->getNumber());
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      getDebug()->print(comp()->getOutFile(), _cfg);
      comp()->dumpMethodTrees("Tree tops right before swapping the flow order:");
      }

   processSwingQueue();

   /*********************************************************/
   /* Move the entry node if needed                         */
   /*********************************************************/

   if (isCountedLoop() && _unrollKind != CompleteUnroll)
      {
      int32_t                   oldNumber  = loop->getNumber();
      int32_t                   newNumber  = _firstEntryNode->getNumber();
      TR_RegionStructure       *cursor     = loop;
      TR_StructureSubGraphNode *cursorNode;
      TR_RegionStructure       *pCursor;

      // Change the loop structure, and mark the first unroll body as the entry node
      //
      cursor->setEntry(_firstEntryNode);
      dumpOptDetails(comp(), "%schanged entry node of region %d [%x] to %d\n", OPT_DETAILS, cursor->getNumber(), cursor, newNumber);

      // Now fix up the outer regions as necessary
      // More work is required if there are multiple regions with the same number as the loop
      //
      do
         {
         pCursor    = cursor->getParent()->asRegion();
         cursorNode = pCursor->findSubNodeInRegion(cursor->getNumber());

         cursor->setNumber(newNumber);
         cursorNode->setNumber(newNumber);

         // Goto the parent region, and redirect all the edges going to loop's old entry
         // to the new entry.  Look at all the predecessors of the loop entry node.
         //
         for (auto edge = cursorNode->getPredecessors().begin(); edge != cursorNode->getPredecessors().end();)
            {
            auto next = edge;
            ++next;
            TR_StructureSubGraphNode *predNode = toStructureSubGraphNode((*edge)->getFrom());
            addEdgeForSpillLoop(pCursor, *edge, predNode, cursorNode, true);
            pCursor->removeEdge(predNode->getStructure(), cursor);
            edge = next;
            }

         cursor  = pCursor;
         }
      while (cursor->getNumber() == oldNumber && cursor->getParent());

      for (auto edge = loopNode->getPredecessors().begin(); edge != loopNode->getPredecessors().end();)
         {
         auto next = edge;
         ++next;
         TR_StructureSubGraphNode *predNode = toStructureSubGraphNode((*edge)->getFrom());
         addEdgeForSpillLoop(parent, *edge, predNode, loopNode, true);
         parent->removeEdge(predNode->getStructure(), loop);
         edge = next;
         }
      }

   processSwingQueue();

   }

void TR_LoopUnroller::removeExternalEdge(TR_RegionStructure *parent,
                                                TR_StructureSubGraphNode *from,
                                                int32_t to)
   {
   TR::CFGEdge *relevantEdge = NULL;

   ListIterator<TR::CFGEdge> ei(&parent->getExitEdges());
   TR::CFGEdge *edge;
   for (edge = ei.getFirst(); edge; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == to && edge->getFrom() == from)
         {
         relevantEdge = edge;
         break;
         }
      }

   TR_ASSERT(relevantEdge, "edge misssing when attempting to remove it.");

   if (numExitEdgesTo(from->getStructure()->asRegion(), to) == 0)
      {
      bool found = (std::find(relevantEdge->getFrom()->getSuccessors().begin(), relevantEdge->getFrom()->getSuccessors().end(), relevantEdge) != relevantEdge->getFrom()->getSuccessors().end());
      relevantEdge->getFrom()->getSuccessors().remove(relevantEdge);
      TR_ASSERT(found, "GLU: bad structure while removing edge");
      found = (std::find(relevantEdge->getTo()->getPredecessors().begin(), relevantEdge->getTo()->getPredecessors().end(), relevantEdge) != relevantEdge->getTo()->getPredecessors().end());
      relevantEdge->getTo()->getPredecessors().remove(relevantEdge);
      TR_ASSERT(found,
    		  "GLU: bad structure while removing edge");
      parent->getExitEdges().remove(relevantEdge);
      }
   }

int32_t TR_LoopUnroller::numExitEdgesTo(TR_RegionStructure *from, int32_t to)
   {
   if (!from) return 0;

   int count=0;
   ListIterator<TR::CFGEdge> ei(&from->getExitEdges());
   TR::CFGEdge *edge;
   for (edge = ei.getFirst(); edge; edge = ei.getNext())
      if (edge->getTo()->getNumber() == to)
         count++;

   return count;
   }

void TR_LoopUnroller::prepareForArrayShadowRenaming(TR_RegionStructure *loop)
   {
   // Refining aliases only in counted loops.
   // Non-counted loops are being unrolled in such a way that prevents scheduling across iterations,
   // so there is no point to refine this way.
   //
   if (!_piv)
      return;

   _newSymRefs.init();

   collectInternalPointers();

   collectArrayAccesses();

   examineArrayAccesses();
   }

void TR_LoopUnroller::getLoopPreheaders(TR_RegionStructure *loop, TR_ScratchList<TR::Block> *preheaders)
   {
   TR::Block *loopEntry = loop->getEntryBlock();
   // locate the first preheader in the loop
   TR::Block *firstPreheader = NULL;
   for (auto edge = loopEntry->getPredecessors().begin(); edge != loopEntry->getPredecessors().end(); ++edge)
      {
      TR::Block *pred = toBlock((*edge)->getFrom());
      if (pred->getStructureOf()->isLoopInvariantBlock())
         {
         // found the pre-header
         preheaders->add(pred);
         firstPreheader = pred;
         break;
         }
      }

   TR::Block *preheader = firstPreheader;
   for (TR::Block *pred = toBlock(preheader->getPredecessors().front()->getFrom());
        (preheader->getPredecessors().size() == 1) && pred->getStructureOf()->isLoopInvariantBlock();
        preheader = pred, pred = toBlock(preheader->getPredecessors().front()->getFrom()))
      {
      preheaders->add(pred);
      }
   }

static bool isBiv(TR_RegionStructure *loop, TR::SymbolReference *symRef, TR_BasicInductionVariable *&biv)
   {
   biv = loop->getPrimaryInductionVariable();
   if (biv && biv->getSymRef() == symRef)
      return true;

   ListIterator<TR_BasicInductionVariable> it(&loop->getBasicInductionVariables());
   for (biv = it.getFirst(); biv; biv = it.getNext())
      {
      if (biv->getSymRef() == symRef)
         return true;
      }

   return false;
   }

void TR_LoopUnroller::collectInternalPointers()
   {
   TR_ScratchList<TR::Block> preheaders(trMemory());
   getLoopPreheaders(_loop, &preheaders);

   ListIterator<TR::Block> it(&preheaders);
   for (TR::Block *preheader = it.getFirst(); preheader; preheader = it.getNext())
      {
      if (trace())
         traceMsg(comp(), "Examining pre-header %d of loop %d for array aliasing refinement\n", preheader->getNumber(), _loop->getNumber());

      int64_t offsetValue = 0;
      bool isOffsetConst = true;
      TR::TreeTop *currentTree = preheader->getEntry();
      TR::TreeTop *exitTree = preheader->getExit();

      while (currentTree != exitTree)
         {
         TR::Node *node = currentTree->getNode();
         if (node->getOpCodeValue() == TR::astore && node->getSymbol()->isAuto() && node->getSymbol()->castToAutoSymbol()->isInternalPointer() && node->getFirstChild()->getNumChildren() > 1)
            {
            TR::Node *secondChild = node->getFirstChild()->getSecondChild();
               if ((secondChild->getOpCode().isAdd() || secondChild->getOpCode().isSub()) &&
                   secondChild->getSecondChild()->getOpCode().isLoadConst())
                  {
                  TR::Node *firstChild = secondChild->getFirstChild();

                  if ((firstChild->getOpCode().isMul() ||
                       firstChild->getOpCode().isLeftShift()) &&
                      firstChild->getSecondChild()->getOpCode().isLoadConst())
                     {
                     firstChild = firstChild->getFirstChild();
                     if (firstChild->getOpCode().isConversion())
                        {
                        firstChild = firstChild->getFirstChild();
                        }

                     if ((firstChild->getOpCode().isAdd() ||
                          firstChild->getOpCode().isSub()) &&
                         firstChild->getSecondChild()->getOpCode().isLoadConst())
                        {
                        if (firstChild->getSecondChild()->getOpCodeValue() == TR::iconst)
                           offsetValue = (int64_t)firstChild->getSecondChild()->getInt();
                        else if (firstChild->getSecondChild()->getOpCodeValue() == TR::lconst)
                           offsetValue = firstChild->getSecondChild()->getLongInt();
                        else
                           isOffsetConst = false;

                        firstChild = firstChild->getFirstChild();
                        }

                     TR_BasicInductionVariable *biv;
                     if (firstChild->getOpCode().isLoad())
		        {
			bool canMatch = false;
                        bool canMatchAsIP = false;
                        if (isBiv(_loop, firstChild->getSymbolReference(), biv))
			   canMatch = true;
                        else if (isBiv(_loop, node->getSymbolReference(), biv) && firstChild->getOpCode().isLoadVarDirect() &&
                                 firstChild->getSymbol()->isAuto() && node->getFirstChild()->getFirstChild()->getOpCode().isLoadVarDirect() && node->getFirstChild()->getFirstChild()->getSymbol()->isAuto() && node->getFirstChild()->getFirstChild()->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
			  canMatchAsIP = true;

                        if (canMatch || canMatchAsIP)
                           {
                           if (trace())
                              traceMsg(comp(), "\tFound internal pointer %p with iv %d in offset node %p\n", node, biv->getSymRef()->getReferenceNumber(), secondChild);
                           IntrnPtr *intrnPtr = (IntrnPtr *) trMemory()->allocateStackMemory(sizeof(IntrnPtr));
                           intrnPtr->symRefNum = node->getSymbolReference()->getReferenceNumber();
                           if (canMatch)
			      {
			      intrnPtr->bivNum = -1;
                              intrnPtr->biv = biv;
			      }
                           else
			      {
			      intrnPtr->bivNum = firstChild->getSymbolReference()->getReferenceNumber();
                              intrnPtr->biv = NULL;
			      }

                           intrnPtr->offsetNode = secondChild;
                           intrnPtr->isOffsetConst = isOffsetConst;
                           intrnPtr->offsetValue = offsetValue;
                           _listOfInternalPointers.add(intrnPtr);
			   }
                        }
                     }
                  }
               }
         currentTree = currentTree->getNextTreeTop();
         }
      }
   }

void TR_LoopUnroller::collectArrayAccesses()
   {
   intptr_t visitCount = comp()->incVisitCount();
   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   _loop->getBlocks(&blocksInRegion);

   if (trace())
      traceMsg(comp(), "Looking for array accesses in loop %d\n", _loop->getNumber());

   ListIterator<TR::Block> it(&blocksInRegion);
   TR::Block *block;
   for (block = it.getFirst(); block; block = it.getNext())
      {
      // scan original blocks only
      if (block->getNumber() < _numNodes)
         {
         if (trace())
            traceMsg(comp(), "\tScanning block_%d\n", block->getNumber());

         TR::TreeTop *currentTree = block->getEntry();
         TR::TreeTop *exitTree = block->getExit();

         while (currentTree != exitTree)
            {
            TR::Node *node = currentTree->getNode();
            if (node->getNumChildren() > 0)
               examineNode(node, visitCount);
            currentTree = currentTree->getNextTreeTop();
            }
         }
      }
   }

void TR_LoopUnroller::examineNode(TR::Node *node, intptr_t visitCount)
   {
   // If we have seen this node before, we are done
   // Otherwise, set visit count
   //
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   TR::Symbol *symbol = NULL;
   if (node->getOpCode().hasSymbolReference())
      symbol = node->getSymbol();

   if (symbol && symbol->isArrayShadowSymbol())
      {
      TR_ScratchList<ArrayAccess> *listOfAA = NULL;
      ListIterator<ListsOfArrayAccesses> it(&_listOfListsOfArrayAccesses);
      for (ListsOfArrayAccesses *loaa = it.getFirst(); loaa; loaa = it.getNext())
         {
         if (loaa->symRefNum == node->getSymbolReference()->getReferenceNumber())
            listOfAA = loaa->list;
         }
      if (!listOfAA)
         {
         ListsOfArrayAccesses *loaa = (ListsOfArrayAccesses *) trMemory()->allocateStackMemory(sizeof(ListsOfArrayAccesses));
         loaa->symRefNum = node->getSymbolReference()->getReferenceNumber();
         loaa->list = new (trHeapMemory()) TR_ScratchList<ArrayAccess>(trMemory());
         listOfAA = loaa->list;
         _listOfListsOfArrayAccesses.add(loaa);
         }

      ArrayAccess *aa = (ArrayAccess *) trMemory()->allocateStackMemory(sizeof(ArrayAccess));
      aa->aaNode = node;

      if (node->getFirstChild()->getOpCodeValue() == TR::aload &&
          node->getFirstChild()->getSymbol()->isAuto() &&
          node->getFirstChild()->getSymbol()->castToAutoSymbol()->isInternalPointer())
         {
         aa->intrnPtrNode = node->getFirstChild();
         }
      else
         {
         aa->intrnPtrNode = NULL;
         }
      listOfAA->add(aa);

      if (trace())
         traceMsg(comp(), "\t\tFound array access node %p with sym ref %d and internal pointer node %p\n", node, node->getSymbolReference()->getReferenceNumber(), aa->intrnPtrNode);
      }

   /* Walk its children */
   for (intptr_t i = 0; i < node->getNumChildren(); i++)
      {
      examineNode(node->getChild(i), visitCount);
      }
   }

TR_LoopUnroller::IntrnPtr *TR_LoopUnroller::findIntrnPtr(int32_t symRefNum)
   {
   ListIterator<IntrnPtr> it(&_listOfInternalPointers);
   for (IntrnPtr *ip = it.getFirst(); ip; ip= it.getNext())
      {
      if (ip->symRefNum == symRefNum)
         return ip;
      }
   return NULL;
   }

bool TR_LoopUnroller::haveIdenticalOffsets(IntrnPtr *intrnPtr1, IntrnPtr *intrnPtr2)
   {
   if (!intrnPtr1->isOffsetConst || !intrnPtr2->isOffsetConst)
      return false;

   if ((intrnPtr1->biv == NULL) ||
       (intrnPtr2->biv == NULL))
      {
	/*
      if ((intrnPtr1->bivNum >= 0) &&
          (intrnPtr2->bivNum >= 0))
	 {
	 if ((intrnPtr1->bivNum == intrnPtr2->bivNum) &&
             (intrnPtr1->offsetValue == intrnPtr2->offsetValue))
	    return true;
	 }
	*/

      return false;
      }

   if (intrnPtr1->biv == intrnPtr2->biv &&
       intrnPtr1->offsetValue == intrnPtr2->offsetValue)
      return true;

   TR::Node *entryValueNode1 = intrnPtr1->biv->getEntryValue();
   TR::Node *entryValueNode2 = intrnPtr2->biv->getEntryValue();

   if (!entryValueNode1 || !entryValueNode2)
      return false;

   int64_t entryValue1 = entryValueNode1->getType().isInt64() ? entryValueNode1->getLongInt() :
                                                                (int64_t)entryValueNode1->getInt();
   int64_t entryValue2 = entryValueNode2->getType().isInt64() ? entryValueNode2->getLongInt() :
                                                                (int64_t)entryValueNode2->getInt();
   if (intrnPtr1->biv->getDeltaOnBackEdge() == intrnPtr2->biv->getDeltaOnBackEdge() &&
      entryValue1 + intrnPtr1->offsetValue == entryValue2 + intrnPtr2->offsetValue)
      return true;

   return false;
   }

void TR_LoopUnroller::examineArrayAccesses()
   {
   // What we are trying to achieve here is to allow the possibility to schedule array accesses
   // that belong to different unrolled iterations across these iterations.
   // To do so, we need to guarantee that the array accesses, for which we will alow that,
   // will either access the same location at any given iteration,
   // or always access different locations.
   // a[i] and b[i] are an example of the case:
   // if (a == b), the accesses are the same in any given iteration, and, thus, always different
   // between unrolled iterations.
   // if (a != b), the accesses are simply always different.
   // So as long as we can prove that two accesses have the same index,
   // we can refine their shadows between the iterations.
   //
   // Iterate on the collected array accesses and see if they qualify for alias refinement.
   // If an array access does not qualify, remove the list where it belongs,
   // as we cannot quarantee that they all either access the same location or
   // belong to entirely different arrays, and, thus, will never access to the same location.
   //
   ListIterator<ListsOfArrayAccesses> it(&_listOfListsOfArrayAccesses);
   for (ListsOfArrayAccesses *loaa = it.getFirst(); loaa; loaa = it.getNext())
      {
      if (trace())
         traceMsg(comp(), "Examining list of array accesses with sym ref %d\n", loaa->symRefNum);

      // Check whether all the array accesses in the list have the same index.
      //
      bool canProve = true;
      ListIterator<ArrayAccess> it1(loaa->list);
      ArrayAccess *aa1;
      ArrayAccess *aa2;
      for (aa1 = it1.getFirst(); aa1 && (aa2 = it1.getNext()); aa1 = aa2)
         {
         if (trace())
            traceMsg(comp(), "\tComparing array accesses %p and %p\n", aa1->aaNode, aa2->aaNode);

         // Do not take care of array accesses without internal pointers at this point.
         //
         if (!aa1->intrnPtrNode || !aa2->intrnPtrNode)
            {
            canProve = false;
            break;
            }

         IntrnPtr *intrnPtr1 = findIntrnPtr(aa1->intrnPtrNode->getSymbolReference()->getReferenceNumber());
         IntrnPtr *intrnPtr2 = findIntrnPtr(aa2->intrnPtrNode->getSymbolReference()->getReferenceNumber());
         if (intrnPtr1 && intrnPtr2)
            {
            if (aa1->intrnPtrNode == aa2->intrnPtrNode)
               continue;
            if (intrnPtr1->offsetNode == intrnPtr2->offsetNode)
               continue;
            if (haveIdenticalOffsets(intrnPtr1, intrnPtr2))
               continue;
            }
         canProve = false;
         break;
         }

      if (!canProve || !aa1->intrnPtrNode) // if cannot prove or if there is only one element in the list
         {                                 // and it doesn't have an internal pointer
         if (trace())
            traceMsg(comp(), "List of array accesses with sym ref %d does not qualify for aliasing refinement\n", loaa->symRefNum);
         _listOfListsOfArrayAccesses.remove(loaa);
         }
      }
   }

void TR_LoopUnroller::refineArrayAliasing()
   {
   static char *pEnv = feGetEnv("TR_DisableRefineArrayAliasing");
   if (pEnv) return;


   if (_listOfListsOfArrayAccesses.isEmpty() ||
       !performTransformation(comp(), "%sRefine array aliasing in loop %d\n",
                        OPT_DETAILS1, _loop->getNumber()))
      return;

   TR::SymbolReference *newSymRef;
   ListIterator<ListsOfArrayAccesses> it(&_listOfListsOfArrayAccesses);
   for (ListsOfArrayAccesses *loaa = it.getFirst(); loaa; loaa = it.getNext())
      {
      ListIterator<ArrayAccess> it1(loaa->list);
      ArrayAccess *aa = it1.getFirst();
      if (aa)
         {
         newSymRef = comp()->getSymRefTab()->createRefinedArrayShadowSymbolRef(aa->aaNode->getSymbolReference()->getSymbol()->getDataType());
         ListIterator<TR::SymbolReference> it(&_newSymRefs);
         for (TR::SymbolReference *sr = it.getFirst(); sr; sr = it.getNext())
            {
            newSymRef->makeIndependent(comp()->getSymRefTab(), sr);
            }
         _newSymRefs.add(newSymRef);
         }
      for ( ; aa; aa = it1.getNext())
         {
         aa->aaNode->setSymbolReference(newSymRef);
         //traceMsg(comp(), "Replaced sym ref for node %p to %d\n", aa->aaNode, newSymRef->getReferenceNumber());
         }
      }
   }

int32_t TR_LoopUnroller::unroll(TR_RegionStructure *loop, TR_StructureSubGraphNode *branchNode)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Initialize the mapping datastructures
   //
   _blockMapper[0] = (TR::Block **) trMemory()->allocateStackMemory(_numNodes * sizeof(TR::Block *));
   _blockMapper[1] = (TR::Block **) trMemory()->allocateStackMemory(_numNodes * sizeof(TR::Block *));
   _nodeMapper[0]  = (TR_StructureSubGraphNode **) trMemory()->allocateStackMemory(_numNodes * sizeof(TR::Block *));
   _nodeMapper[1]  = (TR_StructureSubGraphNode **) trMemory()->allocateStackMemory(_numNodes * sizeof(TR::Block *));
   memset(_blockMapper[0], 0, _numNodes * sizeof(TR::Block *));
   memset(_nodeMapper[0],  0, _numNodes * sizeof(TR_StructureSubGraphNode *));
   memset(_blockMapper[1], 0, _numNodes * sizeof(TR::Block *));
   memset(_nodeMapper[1],  0, _numNodes * sizeof(TR_StructureSubGraphNode *));

   prepareLoopStructure(loop);

   _cfg->setStructure(NULL);

   if (_spillLoopRequired)
      generateSpillLoop(loop, branchNode);

   prepareForArrayShadowRenaming(loop);
   refineArrayAliasing();

   int32_t unrollCount = (_unrollCount + 1)/_vectorSize - 1;

   for (_iteration = 1; _iteration <= unrollCount; _iteration++)
      {
      unrollLoopOnce(loop, branchNode, _iteration == unrollCount);
      refineArrayAliasing();
      }

   if (!_newSymRefs.isEmpty())
      optimizer()->setAliasSetsAreValid(false);

   modifyOriginalLoop(loop, branchNode);

   _cfg->setStructure(_rootStructure);
   if (trace())
      {
      traceMsg(comp(), "\nstructure after unrolling on loop %d is finished:\n\n", loop->getNumber());
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      getDebug()->print(comp()->getOutFile(), _cfg);
      comp()->dumpMethodTrees(" xxxx Tree tops after unrolling:");
      }

   return _unrollCount * 5;
   }

TR::Node *TR_LoopUnroller::createIfNodeForSpillLoop(TR::Node *ifNode)
   {
   // The spill loop has to do the test upfront even though the original loop
   // might be a do-while loop.
   // We remove the if from the end of the original if block, and stick it into
   // a new block at the entry of the spill loop.  One tricky thing here is that
   // some (evil) post-increment loops test the unincremented value which is
   // different from the stored value of the index
   // NOTE: i have avoided the problem by asserting that loops be canonicalized
   // so that i never have to deal with this problem
   //
   TR::Node *c1;

   c1 = TR::Node::createLoad(ifNode, _piv->getSymRef());
   if (getIndexType().isAddress() || getTestChildType().isAddress())
      {
      TR_ASSERT(getIndexType().isAddress() && getTestChildType().isAddress(), "Test child type and IV type should both be address");
      }
   else if (c1->getType().isAggregate() && !getTestChildType().isAggregate())
      {
      TR::ILOpCodes o2xOp = TR::ILOpCode::getProperConversion(c1->getDataType(), getTestChildType().getDataType(), true);
      TR_ASSERT(o2xOp != TR::BadILOp,"could not find conv op for %s -> %s conversion\n",TR::DataType::getName(c1->getDataType()),TR::DataType::getName(getTestChildType().getDataType()));
      c1 = TR::Node::create(o2xOp, 1, c1);
      }
   else if (getIndexType().isInt32() && getTestChildType().isInt64())
      {
      c1 = TR::Node::create(TR::i2l, 1, c1);
      }
   else if (getIndexType().isInt64() && getTestChildType().isInt32())
      {
      c1 = TR::Node::create(TR::l2i, 1, c1);
      }

   return TR::Node::createif(ifNode->getOpCodeValue(),
                            c1, ifNode->getSecondChild()->duplicateTree(),
                            ifNode->getBranchDestination());
   }

void TR_LoopUnroller::generateSpillLoop(TR_RegionStructure *loop,
                                        TR_StructureSubGraphNode *branchNode)
   {
   //Reset the mappers.
   _iteration=0;
   memset(_blockMapper[CURRENT_MAPPER], 0, _numNodes * sizeof(TR::Block *));
   memset( _nodeMapper[CURRENT_MAPPER], 0, _numNodes * sizeof(TR_StructureSubGraphNode *));

   // clone the loop
   cloneBlocksInRegion(loop, true); // isSpillLoop = true

   TR_RegionStructure *spillLoop = cloneStructure(loop)->asRegion();
   TR_StructureSubGraphNode *spillNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(spillLoop);
   fixExitEdges(loop, spillLoop, branchNode);

   spillLoop->getEntryBlock()->getStructureOf()->setIsEntryOfShortRunningLoop();

   TR_RegionStructure *parent = loop->getParent()->asRegion();
   parent->addSubNode(spillNode);

   processSwingQueue();

   if (trace())
      {
      traceMsg(comp(), "trees after creating the spill loop %d for loop %d:\n",
             spillNode->getNumber(), loop->getNumber());
      comp()->dumpMethodTrees("trees after creating spill loop");
      }

   _spillNode = spillNode;
   _spillBranchBlock = GET_CLONE_NODE(branchNode)->getStructure()->asBlock()->getBlock();


   if (_wasEQorNELoop)
      {
      TR_ASSERT(_spillBranchBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch(),
             "expecting a branch in the spill loop branch block");
      TR::Node::recreate(_spillBranchBlock->getLastRealTreeTop()->getNode(), _origLoopCondition);
      }
   }

/****
void TR_LoopUnroller::generateSpillLoop(TR_RegionStructure *loop,
                                               TR_StructureSubGraphNode *branchNode)
   {
   TR_RegionStructure *parent = loop->getParent()->asRegion();
   //Reset the mappers.
   _iteration=0;
   memset(_blockMapper[CURRENT_MAPPER], 0, _numNodes * sizeof(TR::Block *));
   memset( _nodeMapper[CURRENT_MAPPER], 0, _numNodes * sizeof(TR_StructureSubGraphNode *));


   //clone the loop
   cloneBlocksInRegion(loop);
   TR_RegionStructure *spillLoop = cloneStructure(loop)->asRegion();
   TR_StructureSubGraphNode *spillNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(spillLoop);
   fixExitEdges(loop, spillLoop, branchNode);

   TR_StructureSubGraphNode *clonedBranchNode = GET_CLONE_NODE(branchNode);
   TR_BlockStructure *clonedBranchStructure = clonedBranchNode->getStructure()->asBlock();
   TR::Block *clonedBranchBlock = clonedBranchStructure->getBlock();
   TR::TreeTop *branchTT = clonedBranchBlock->getLastRealTreeTop();
   TR::Node *node = branchTT->getNode();

   //TR::Node *newNode = cloneIfNode(branchTT);
   TR::Node *newNode = createIfNodeForSpillLoop(branchTT->getNode());
   TR::TreeTop *clonedBranchTT = TR::TreeTop::create(comp(), newNode);

   //create a new block B' with only the branch TT in it.
   TR::Block *newBranchBlock = TR::Block::createEmptyBlock(node, comp());
   newBranchBlock->append(clonedBranchTT);
   _cfg->addNode(newBranchBlock);

   //create the structure
   TR_BlockStructure *newBranchStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newBranchBlock->getNumber(),
                                                                 newBranchBlock);
   TR_StructureSubGraphNode *newBranchNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newBranchStructure);
   spillLoop->addSubNode(newBranchNode);

   //make newBranchNode the entryNode
   spillLoop->setEntry(newBranchNode);
   spillLoop->setNumber(newBranchNode->getNumber());
   spillNode->setNumber(newBranchNode->getNumber());

   // mark the new entry block as being a spill entry
   //
   newBranchStructure->setIsEntryOfShortRunningLoop();


   //add spill loop to the parent
   parent->addSubNode(spillNode);

   if (trace())
      {
      traceMsg(comp(), "\nstructure after creating the spill loop %d:\n\n", loop->getNumber());
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      getDebug()->print(comp()->getOutFile(), _cfg);
      comp()->dumpMethodTrees("method trees:");
      }


   //Find the internal edge from B to K (K is the internal successor of B)
   ListIterator<TR::CFGEdge> edgeIt(&clonedBranchNode->getSuccessors());
   TR::CFGEdge *exitEdge = NULL;
   TR::CFGEdge *internalEdge = NULL;

   TR::CFGEdge *edge;
   for (edge = edgeIt.getFirst(); edge; edge = edgeIt.getNext())
      {
      TR_StructureSubGraphNode *successor = toStructureSubGraphNode(edge->getTo());
      if (successor->getStructure() != NULL)
         internalEdge = edge;
      }

   //Find the exit edge from B to X
   //find the original exit edge in the original loop, because the one in the spill does not have cfg edges
   edgeIt.set(&branchNode->getSuccessors());
   for (edge = edgeIt.getFirst(); edge; edge = edgeIt.getNext())
      {
      TR_StructureSubGraphNode *successor = toStructureSubGraphNode(edge->getTo());
      if (successor->getStructure() == NULL)
         exitEdge = edge;
      }

   TR_StructureSubGraphNode *exitNode =
      findNodeInHierarchy(parent, toStructureSubGraphNode(exitEdge->getTo())->getNumber());

   //Add B'->X*
   addEdgeForSpillLoop(spillLoop, exitEdge, newBranchNode, exitNode);

   //Add B'->K
   addEdgeForSpillLoop(spillLoop, internalEdge, newBranchNode,
                       toStructureSubGraphNode(internalEdge->getTo()));
   //Remove B->K
   {
      TR_StructureSubGraphNode *nodeK = toStructureSubGraphNode(internalEdge->getTo());
      TR::Block *blockK = getEntryBlockNode(nodeK)->getStructure()->asBlock()->getBlock();
      spillLoop->removeEdge(clonedBranchStructure,
                            toStructureSubGraphNode(internalEdge->getTo())->getStructure());
      _cfg->removeEdge(clonedBranchBlock, blockK);
   }

   //Remove B->X*
   {
      TR::Block *blockX = getEntryBlockNode(exitNode)->getStructure()->asBlock()->getBlock();
      removeExternalEdge(spillLoop, clonedBranchNode, exitNode->getNumber());
      _cfg->removeEdge(clonedBranchBlock, blockX);
   }

   //Remove the branch from B
   clonedBranchBlock->removeBranch(comp());

   //put branch block after the last treetop
   TR::TreeTop *endTreeTop;
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      treeTop = treeTop->getNode()->getBlock()->getExit();
      endTreeTop = treeTop;
      }
   endTreeTop->join(newBranchBlock->getEntry());

   //add an edge from B to B'
   TR::CFGEdge::createEdge(clonedBranchNode,  newBranchNode, trMemory())
   _cfg->addEdge(clonedBranchBlock, newBranchBlock);

   //we will need to add a goto as well. B must fall through to B'
   TR::Node *gotoNode = TR::Node::create(node, TR::Goto);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
   clonedBranchBlock->append(gotoTree);
   gotoNode->setBranchDestination(newBranchBlock->getEntry());
   gotoNode->setLocalIndex(CREATED_BY_GLU);

   processSwingQueue();

   if (trace())
      {
      traceMsg(comp(), "\nstructure after including the spill loop %d:\n\n", loop->getNumber());
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      getDebug()->print(comp()->getOutFile(), _cfg);
      comp()->dumpMethodTrees("method trees:");
      }

   _spillNode = spillNode;
   }
****/

TR_StructureSubGraphNode *TR_LoopUnroller::findNodeInHierarchy(TR_RegionStructure *region,
                                                                      int32_t num)
   {
   if (!region) return NULL;
   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getNumber() == num)
         return node;
      }
   return findNodeInHierarchy(region->getParent()->asRegion(), num);
   }

void TR_LoopUnroller::addEdgeForSpillLoop(TR_RegionStructure *region,
                                                 TR::CFGEdge *originalEdge,
                                                 TR_StructureSubGraphNode *newFromNode,
                                                 TR_StructureSubGraphNode *newToNode,
                                                 bool removeOriginal,
                                                 EdgeContext edgeContext,
                                                 bool notLoopBranchNode)
   {
   TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(originalEdge->getFrom());
   TR_StructureSubGraphNode *toNode   = toStructureSubGraphNode(originalEdge->getTo());
   if (toNode->getStructure() == NULL) //exit edge
      toNode = findNodeInHierarchy(region->getParent()->asRegion(), toNode->getNumber());

   TR_ScratchList<TR::CFGEdge> *cfgEdges = findCorrespondingCFGEdges
      (fromNode->getStructure(), toNode->getStructure(), comp());
   ListIterator<TR::CFGEdge> it(cfgEdges);
   for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
      {
      TR::Block *from = toBlock(edge->getFrom());
      TR::Block *to   = toBlock(edge->getTo());
      TR::Block *newFrom;
      TR::Block *newTo;

      if (newFromNode->getStructure()->asBlock())
         newFrom = newFromNode->getStructure()->asBlock()->getBlock();
      else
         newFrom = getEntryBlockNode(newFromNode)->getStructure()->asBlock()->getBlock();

      if (newToNode->getStructure()->asBlock())
         newTo = newToNode->getStructure()->asBlock()->getBlock();
      else
         newTo = getEntryBlockNode(newToNode)->getStructure()->asBlock()->getBlock();


      TR::Node *lastNode = from->getLastRealTreeTop()->getNode();

      bool lastGenerationEndsInBranch = false;
      if ((edgeContext == BackEdgeFromLastGenerationCompleteUnroll) &&
            (newFrom->getLastRealTreeTop()->getNode()->getOpCode().isBranch()))
         lastGenerationEndsInBranch = true;

      //BRANCH
      if (lastNode->getOpCode().isBranch() &&
            (lastNode->getBranchDestination() == to->getEntry()) &&
            !lastGenerationEndsInBranch)
         {
         if (newToNode->getStructure()->getParent() == region)
            TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
         else
            region->addExitEdge(newFromNode, newToNode->getNumber());

         _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));

         // In case of this special contex the exit edge was already removed by unrollLoopOnce
         if (edgeContext != BackEdgeFromLastGenerationCompleteUnroll)
            {
            newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), to->getEntry(),
                                                                        newTo->getEntry());
            }
         else
            {
            TR::Node *gotoNode = TR::Node::create(lastNode, TR::Goto);
            gotoNode->setBranchDestination(newTo->getEntry());
            gotoNode->setLocalIndex(CREATED_BY_GLU);
            newFrom->append(TR::TreeTop::create(comp(), gotoNode));
            }
         }
      //SWITCH
      else if (lastNode->getOpCode().isJumpWithMultipleTargets())
         {
         if (newToNode->getStructure()->getParent() == region)
            TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
         else
            region->addExitEdge(newFromNode, newToNode->getNumber());

         _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), to->getEntry(),
                                                                    newTo->getEntry());

         }
      //RETURNS
      else if (lastNode->getOpCode().isReturn())
         {
         if (!edgeAlreadyExists(newFromNode, to->getNumber()))
            region->addExitEdge(newFromNode, to->getNumber());
         if (!cfgEdgeAlreadyExists(newFrom, newTo))
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         }
      //FALLS INTO
      else
         {
         TR::Block *newFromNextBlock = newFrom->getNextBlock();
         if (newFromNextBlock != newTo) //but the copy does not fall through
            {
            //create a new goto block
            TR::Node *gotoNode = TR::Node::create(lastNode, TR::Goto);
            TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
            gotoNode->setBranchDestination(newTo->getEntry());
            gotoNode->setLocalIndex(CREATED_BY_GLU);
            TR::Block *gotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), newFrom->getFrequency(), newFrom);
            gotoBlock->append(gotoTree);
            gotoBlock->getEntry()->getNode()->setLocalIndex(CREATED_BY_GLU);
            _cfg->addNode(gotoBlock);


            //fix the tree tops: insert the goto block in place
            //
            bool addGotoBlock = true;
            // dealing with loop controlling exit?
            // for complete unrolls
            //
            TR::TreeTop *oldDest = from->getEntry();
            if (notLoopBranchNode &&
                  (edgeContext == BackEdgeFromLastGenerationCompleteUnroll))
               {
               TR::Node *newFromLastNode = newFrom->getLastRealTreeTop()->getNode();
               // this cannot be anything other than a branch
               // there are three possibilities
               // a) the branch was going to the loop entry (backedge)
               //     -- it needs to be re-directed to the newexit
               // b) the branch was already going to the exit from the loop test
               //     -- the fall through will now be directed to the gotoblock and
               //        addExitEdgeAndFixEverything will add the original exit edge
               // c) the branch is going to a different loop exit
               //     -- addExitEdgeAndFixEverything will add the new exit edge eventually
               //

               if (newFromLastNode->getOpCode().isBranch())
                  {
                  if (newFromLastNode->getBranchDestination() == _loop->getEntryBlock()->getEntry())
                     {
                     TR::TreeTop *endTree = comp()->getMethodSymbol()->getLastTreeTop();
                     endTree->join(gotoBlock->getEntry());
                     gotoBlock->getExit()->join(0);
                     addGotoBlock = false;
                     oldDest = _loop->getEntryBlock()->getEntry();
                     }
                  /*
                  else if (newFromLastNode->getBranchDestination() == newTo->getEntry())
                     {
                     newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), newTo->getEntry(), gotoBlock->getEntry());
                     }
                  */
                  }
               }

            if (addGotoBlock)
               {
               newFrom->getExit()->join(gotoBlock->getEntry());
               if (newFromNextBlock)
                  gotoBlock->getExit()->join(newFromNextBlock->getEntry());
               else
                  gotoBlock->getExit()->setNextTreeTop(NULL);
               }

            // redirect the target of newFrom to goto the exit
            if (edgeContext == BackEdgeFromLastGenerationCompleteUnroll)
               {
               if (newFrom->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
                  newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), oldDest, gotoBlock->getEntry());
               }

            //create the structures
            TR_BlockStructure *gotoStruct = new (_cfg->structureRegion()) TR_BlockStructure(comp(), gotoBlock->getNumber(),
                                                                  gotoBlock);
            TR_StructureSubGraphNode *gotoSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(gotoStruct);
            region->addSubNode(gotoSubNode);

            //create the cfg edges
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  gotoBlock, trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  newTo, trMemory()));

            TR::CFGEdge::createEdge(newFromNode,  gotoSubNode, trMemory());
            if (newToNode->getStructure()->getParent() == region)
               TR::CFGEdge::createEdge(gotoSubNode,  newToNode, trMemory());
            else
               region->addExitEdge(gotoSubNode, newToNode->getNumber());
            }
         else //newFromNextBlock == newTo
            {
            if (newToNode->getStructure()->getParent() == region)
               TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
            else
               region->addExitEdge(newFromNode, newToNode->getNumber());
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
            }

         }

       if (removeOriginal)
         _cfg->removeEdge(edge);
      }
   }

void TR_LoopUnroller::addExitEdgeAndFixEverything(TR_RegionStructure *region,
                                                         TR::CFGEdge *originalEdge,
                                                         TR_StructureSubGraphNode *newFromNode,
                                                         TR_StructureSubGraphNode *exitNode,
                                                         TR::Block *newExitBlock,
                                                         EdgeContext context)
   {
   TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(originalEdge->getFrom());
   int32_t exitNum = toStructureSubGraphNode(originalEdge->getTo())->getNumber();
   TR_StructureSubGraphNode *toNode = exitNode;

   if (exitNode == NULL)
      toNode =  findNodeInHierarchy(region->getParent()->asRegion(), exitNum);

   TR_ASSERT(toNode->getStructure(), "bad use of addexitedge");
   TR_ScratchList<TR::CFGEdge> *cfgEdges = findCorrespondingCFGEdges
      (fromNode->getStructure(), toNode->getStructure(), comp());
   ListIterator<TR::CFGEdge> it(cfgEdges);
   for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
      {
      TR::Block *from = toBlock(edge->getFrom());
      TR::Block *to   = toBlock(edge->getTo());
      TR::Block *newFrom;
      TR::Block *newTo;

      if (newExitBlock != NULL)
         {
         newTo = newExitBlock;
         exitNum = newExitBlock->getNumber();
         }
      else
         newTo = to;

      if (newFromNode->getStructure()->asBlock())
         newFrom = newFromNode->getStructure()->asBlock()->getBlock();
      else
         newFrom = GET_CLONE_BLOCK(from);

      TR::Node *lastNode = from->getLastRealTreeTop()->getNode();
      //BRANCH
      if (lastNode->getOpCode().isBranch() && lastNode->getBranchDestination() == to->getEntry())
         {
         if (!edgeAlreadyExists(newFromNode, exitNum))
            region->addExitEdge(newFromNode, exitNum);
         if (!cfgEdgeAlreadyExists(newFrom, newTo, context))
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), to->getEntry(),
                                                                    newTo->getEntry());
         }
      //SWITCH
      else if (lastNode->getOpCode().isJumpWithMultipleTargets())
         {
         if (!edgeAlreadyExists(newFromNode, exitNum))
            region->addExitEdge(newFromNode, exitNum);
         if (!cfgEdgeAlreadyExists(newFrom, newTo))
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), to->getEntry(),
                                                                    newTo->getEntry());
         }
      //RETURNS
      else if (lastNode->getOpCode().isReturn())
         {
         if (!edgeAlreadyExists(newFromNode, exitNum))
            region->addExitEdge(newFromNode, exitNum);
         if (!cfgEdgeAlreadyExists(newFrom, newTo))
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         }
      //FALLS INTO
      else
         {
         TR::Block *newFromNextBlock = newFrom->getNextBlock();
         if (newFromNextBlock != newTo)
            {
            //No fall through in the clones
            if (context == ExitEdgeFromBranchNode)
               {
               swingBlocks(newFrom, newTo);
               if (!cfgEdgeAlreadyExists(newFrom, newTo))
                  _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
               if (!edgeAlreadyExists(newFromNode, exitNum))
                  region->addExitEdge(newFromNode, exitNum);
               }
            else
               {
               if (!cfgEdgeAlreadyExists(newFrom, newTo))
                  {
                  //create a new goto block
                  TR::Node *gotoNode = TR::Node::create(lastNode, TR::Goto);
                  TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
                  gotoNode->setBranchDestination(newTo->getEntry());
                  gotoNode->setLocalIndex(CREATED_BY_GLU);
                  TR::Block *gotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), newTo->getFrequency(), newTo);
                  gotoBlock->append(gotoTree);
                  gotoBlock->getEntry()->getNode()->setLocalIndex(CREATED_BY_GLU);
                  _cfg->addNode(gotoBlock);

                  //fix the tree tops: insert the goto block in place
                  newFrom->getExit()->join(gotoBlock->getEntry());
                  if (newFromNextBlock)
                     gotoBlock->getExit()->join(newFromNextBlock->getEntry());
                  else
                     gotoBlock->getExit()->setNextTreeTop(NULL);

                  //create the structures
                  TR_BlockStructure *gotoStruct = new (_cfg->structureRegion()) TR_BlockStructure(comp(), gotoBlock->getNumber(),
                                                                        gotoBlock);
                  TR_StructureSubGraphNode *gotoSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(gotoStruct);
                  region->addSubNode(gotoSubNode);

                  //create the cfg edges
                  _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  gotoBlock, trMemory()));
                  _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  newTo, trMemory()));

                  TR::CFGEdge::createEdge(newFromNode,  gotoSubNode, trMemory());
                  region->addExitEdge(gotoSubNode, exitNum);
                  }
               else if (!edgeAlreadyExists(newFromNode, exitNum))
                  region->addExitEdge(newFromNode, exitNum);
               }
            }
         else
            {
            if (!edgeAlreadyExists(newFromNode, exitNum))
               region->addExitEdge(newFromNode, exitNum);
            if (!cfgEdgeAlreadyExists(newFrom, newTo))
               _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
            }
         }
      }
   }

void TR_LoopUnroller::addEdgeAndFixEverything(TR_RegionStructure *region,
                                                     TR::CFGEdge *originalEdge,
                                                     TR_StructureSubGraphNode *newFromNode,
                                                     TR_StructureSubGraphNode *newToNode,
                                                     bool redirectOriginal,
                                                     bool removeOriginalEdges,
                                                     bool edgeToEntry,
                                                     EdgeContext context)
   {
   TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(originalEdge->getFrom());
   TR_StructureSubGraphNode *toNode   = toStructureSubGraphNode(originalEdge->getTo());

   if (newFromNode == NULL)
      newFromNode = redirectOriginal ? fromNode : GET_CLONE_NODE(fromNode);
   if (newToNode == NULL)
      newToNode = GET_CLONE_NODE(toNode);

   if (toNode->getStructure() == NULL) //exit edge
      toNode = findNodeInHierarchy(region->getParent()->asRegion(), toNode->getNumber());

   TR_ScratchList<TR::CFGEdge> *cfgEdges = findCorrespondingCFGEdges
      (fromNode->getStructure(), toNode->getStructure(), comp());
   ListIterator<TR::CFGEdge> it(cfgEdges);
   for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
      {
      TR::Block *from = toBlock(edge->getFrom());
      TR::Block *to   = toBlock(edge->getTo());
      TR::Block *newFrom;
      TR::Block *newTo;

      if (newFromNode->getStructure()->asRegion())
         newFrom = GET_CLONE_BLOCK(from);
      else
         newFrom = newFromNode->getStructure()->asBlock()->getBlock();

      if (newToNode->getStructure()->asRegion())
         {
         if (edgeToEntry)
            newTo = getEntryBlockNode(newToNode)->getStructure()->asBlock()->getBlock();
         else
            newTo = GET_CLONE_BLOCK(to);
         }
      else
         newTo = newToNode->getStructure()->asBlock()->getBlock();



      TR::Node *lastNode = from->getLastRealTreeTop()->getNode();
      //BRANCH
      if (lastNode->getOpCode().isBranch() && lastNode->getBranchDestination() == to->getEntry())
         {
         //make sure that we really do have a branch at the end of newFrom as well
         if (newFrom->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
            {
            //we do have a branch: simple add the edges & fix the trees
            if (!edgeAlreadyExists(newFromNode,newToNode))
               TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
            if (!cfgEdgeAlreadyExists(newFrom, newTo))
               _cfg->addEdge(newFrom, newTo);
            newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), to->getEntry(),
                                                                       newTo->getEntry());
            }
         else
            {
            //we do NOT have a branch in the newFrom, we must add a goto tree
            if (!edgeAlreadyExists(newFromNode,newToNode))
               TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
            if (!cfgEdgeAlreadyExists(newFrom, newTo))
               _cfg->addEdge(newFrom, newTo);

            TR::TreeTop *lastTreeTop = newFrom->getLastRealTreeTop();
            TR::Node *gotoNode = TR::Node::create(lastNode, TR::Goto);
            TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
            gotoTree->join(lastTreeTop->getNextTreeTop());
            lastTreeTop->join(gotoTree);
            gotoNode->setBranchDestination(newTo->getEntry());
            gotoNode->setLocalIndex(CREATED_BY_GLU);
            }
         }
      //SWITCH
      else if (lastNode->getOpCode().isJumpWithMultipleTargets())
         {
         if (!edgeAlreadyExists(newFromNode,newToNode))
            TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
         if (!cfgEdgeAlreadyExists(newFrom, newTo))
            _cfg->addEdge(newFrom, newTo);
         newFrom->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), to->getEntry(),
                                                                    newTo->getEntry());
         }
      //RETURNS
      else if (lastNode->getOpCode().isReturn())
         {
         if (!edgeAlreadyExists(newFromNode, to->getNumber()))
            region->addExitEdge(newFromNode, to->getNumber());
         if (!cfgEdgeAlreadyExists(newFrom, newTo))
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         }
      //FALLS INTO
      else
         {
         TR::Block *newFromNextBlock = newFrom->getNextBlock();
         if (newFromNextBlock != newTo)
            {
            TR_ASSERT(context == BackEdgeFromPrevGeneration ||
                   context == BackEdgeToEntry,
                   "Original block sequence not preserved while cloning");
            //Swing the blocks into the correct order
            swingBlocks(newFrom, newTo);
            }

         if (!edgeAlreadyExists(newFromNode, newToNode))
            TR::CFGEdge::createEdge(newFromNode,  newToNode, trMemory());
         if (!cfgEdgeAlreadyExists(newFrom, newTo))
            _cfg->addEdge(TR::CFGEdge::createEdge(newFrom,  newTo, trMemory()));
         //I am sure that there is no need to adjust tree tops.
         }

      //Remove the original edge if required . this is the correct
      //place to do it, because it will remove orphans if we did it otherwise.
      if (removeOriginalEdges)
         _cfg->removeEdge(edge);

      }
   }

bool TR_LoopUnroller::cfgEdgeAlreadyExists(TR::Block *from, TR::Block *to, EdgeContext edgeContext)
   {
   for (auto edge = from->getSuccessors().begin(); edge != from->getSuccessors().end(); ++edge)
      {
      TR::Block *succ = toBlock((*edge)->getTo());
      if (succ->getNumber() == to->getNumber())
         return true;

      //Other possibility is that we have already inserted a goto block to get to 'to'
      // Ignore it for the special case of fixing up backedges for the last generation
      // in a complete unroll
      if (edgeContext != BackEdgeFromLastGenerationCompleteUnroll)
         {
         TR::TreeTop *firstRealTree = succ->getFirstRealTreeTop();
         TR::TreeTop *lastRealTree  = succ->getLastRealTreeTop();
         if (firstRealTree == lastRealTree &&
             firstRealTree->getNode()->getOpCodeValue() == TR::Goto)
            {
            TR::Block *dest = firstRealTree->getNode()->getBranchDestination()->getNode()->getBlock();
            // check if the goto block was really added by GLU
            // in some cases, the controlling branch may have been replaced with a goto
            // to the loop entry, so just checking the goto node is insufficient
            //
            if (dest->getNumber() == to->getNumber() &&
                lastRealTree->getNode()->getLocalIndex() == CREATED_BY_GLU &&
                succ->getEntry()->getNode()->getLocalIndex() == CREATED_BY_GLU)
               return true;
            }
         }
      }
   return false;
   }

bool TR_LoopUnroller::edgeAlreadyExists(TR_StructureSubGraphNode *from,
                                               TR_StructureSubGraphNode *to)
   {
   for (auto edge = from->getSuccessors().begin(); edge != from->getSuccessors().end(); ++edge)
      if (toStructureSubGraphNode((*edge)->getTo())->getNumber() == to->getNumber())
          return true;
   return false;
   }

bool TR_LoopUnroller::edgeAlreadyExists(TR_StructureSubGraphNode *from,
                                               int32_t toNum)
   {
   for (auto edge = from->getSuccessors().begin(); edge != from->getSuccessors().end(); ++edge)
      if ((*edge)->getTo()->getNumber() == toNum)
          return true;
   return false;
   }

TR_StructureSubGraphNode *TR_LoopUnroller::getEntryBlockNode(TR_StructureSubGraphNode *node)
   {
   if (node->getStructure()->asBlock())
      return node;
   else
      return getEntryBlockNode(node->getStructure()->asRegion()->getEntry());
   }

void TR_LoopUnroller::cloneBlocksInRegion(TR_RegionStructure *region, bool isSpillLoop)
   {
   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   region->getBlocks(&blocksInRegion);

   //Find the last tree top, so that we can append our blocks after that
   TR::TreeTop *endTreeTop = NULL;
   TR::TreeTop *treeTop;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      treeTop = treeTop->getNode()->getBlock()->getExit();
      endTreeTop = treeTop;
      }

   if (!_startPosOfUnrolledBodies)
      _startPosOfUnrolledBodies = endTreeTop;

   //Clone all the blocks in the region
   ListIterator<TR::Block> it(&blocksInRegion);
   TR::Block *block;
   for (block = it.getCurrent(); block; block = it.getNext())
      {
      //clone original blocks only
      if (block->getNumber() < _numNodes)
         {
         TR_BlockCloner cloner(_cfg, true);
         TR::Block *newBlock = cloner.cloneBlocks(block, block);
         SET_CLONE_BLOCK(block, newBlock);
         }
      }

   //Append the blocks at the end
   TR::TreeTop *stopTreeTop = endTreeTop;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      block = treeTop->getNode()->getBlock();
      if (block->getNumber() < _numNodes)
         {

         TR::Block *clonedBlock = GET_CLONE_BLOCK(block);
         if (clonedBlock)
            {
            TR::TreeTop *newEntryTreeTop = clonedBlock->getEntry();
            TR::TreeTop *newExitTreeTop  = clonedBlock->getExit();
            endTreeTop->join(newEntryTreeTop);
            newExitTreeTop->setNextTreeTop(NULL);
            endTreeTop = newExitTreeTop;
            }
         }

      treeTop = treeTop->getNode()->getBlock()->getExit();
      if (treeTop == stopTreeTop)
         break;
      }

   _endPosOfUnrolledBodies = endTreeTop;

   //We are done.
   }

inline TR_Structure *TR_LoopUnroller::cloneStructure(TR_Structure *s)
   {
   if (s->asRegion()) return cloneRegionStructure(s->asRegion());
   return cloneBlockStructure(s->asBlock());
   }

TR_Structure *TR_LoopUnroller::cloneBlockStructure(TR_BlockStructure *blockStructure)
   {
   TR::Block *block = blockStructure->getBlock();
   TR::Block *clonedBlock = GET_CLONE_BLOCK(block);
   TR_BlockStructure *clonedBlockStructure =
      new (_cfg->structureRegion()) TR_BlockStructure(comp(), clonedBlock->getNumber(), clonedBlock);
   clonedBlockStructure->setAsLoopInvariantBlock(blockStructure->isLoopInvariantBlock());
   clonedBlockStructure->setNestingDepth(blockStructure->getNestingDepth());
   clonedBlockStructure->setMaxNestingDepth(blockStructure->getMaxNestingDepth());
   return clonedBlockStructure;
   }

TR_Structure *TR_LoopUnroller::cloneRegionStructure(TR_RegionStructure *region)
   {
   TR_RegionStructure *clonedRegion = new (_cfg->structureRegion()) TR_RegionStructure(comp(), 0xDeadF00d); //for now
   clonedRegion->setAsCanonicalizedLoop(region->isCanonicalizedLoop());
   clonedRegion->setContainsInternalCycles(region->containsInternalCycles());

   //Make duplicate sub-structures and subgraph noded
   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*region);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      TR_Structure *clonedSubStruct = cloneStructure(subStruct);
      TR_StructureSubGraphNode *clonedSubNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(clonedSubStruct);
      SET_CLONE_NODE(subNode, clonedSubNode);
      clonedRegion->addSubNode(clonedSubNode);
      if (subNode == region->getEntry())
         {
         clonedRegion->setEntry(clonedSubNode);
         clonedRegion->setNumber(clonedSubNode->getNumber());
         }
      }

   //add edges between these fellas
   si.reset();
   for (subNode = si.getFirst(); subNode; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      TR_StructureSubGraphNode *clonedSubNode = GET_CLONE_NODE(subNode);

      for (auto edge = subNode->getSuccessors().begin(); edge != subNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (!region->isExitEdge(*edge))
            addEdgeAndFixEverything(clonedRegion, *edge);
         else
            {
            /*nothing*/
            }
         }

      TR_ASSERT(subNode->getExceptionSuccessors().empty(), "should not reach here - we dont unroll loops with exceptions");
      }

   clonedRegion->setNestingDepth(region->getNestingDepth());
   //FIXME: maxNestingDepth as well.

   return clonedRegion;
   }


void TR_LoopUnroller::swingBlocks(TR::Block *from, TR::Block *to)
   {
   //Put this into a job-queue that will be processed later.
   SwingPair *pair = (SwingPair *) trMemory()->allocateStackMemory(sizeof(SwingPair));
   pair->from = from;
   pair->to = to;
   _swingQueue.add(pair);
   }

void TR_LoopUnroller::processSwingQueue()
   {
   ListIterator<SwingPair> it(&_swingQueue);
   for (SwingPair *pair = it.getFirst(); pair; pair = it.getNext())
      {
      processSwingBlocks(pair->from, pair->to);
      }
   _swingQueue.deleteAll();
   }

//We want block 'from' to fall into block 'to', but they are not it the correct order
//in the tree tops.  Swing some blocks around to make it so.
void TR_LoopUnroller::processSwingBlocks(TR::Block *from, TR::Block *to)
   {
   TR::Block *pF = from->getPrevBlock();
   TR::Block *pT = to->getPrevBlock();
   TR::Block *nF = from->getNextBlock();
   TR::Block *nT = to->getNextBlock();

   //if pFrom does not fall into F, swing F right before T.
   if (pF == NULL || isSuccessor(pF, from) == false)
      {      //insert F between pT and T

      if (pF && nF)
         pF->getExit()->join(nF->getEntry());
      else if (!pF)
         {
         _cfg->setStart(nF);
         nF->getEntry()->setPrevTreeTop(NULL);
         }
      else if (!nF)
         pF->getExit()->setNextTreeTop(NULL);
      else
         TR_ASSERT(0, "GLU: illegal cfg discovered while swinging blocks");

      from->getExit()->join(to->getEntry());
      if (pT)
         pT->getExit()->join(from->getEntry());
      else
         {
         _cfg->setStart(from);
         from->getEntry()->setPrevTreeTop(NULL);
         }
      }
   else if (nT == NULL || isSuccessor(to, nT) == false)
      {    //insert T between F and nF

      if (pT && nT)
         pT->getExit()->join(nT->getEntry());
      else if (!pT)
         {
         _cfg->setStart(nT);
         nT->getEntry()->setPrevTreeTop(NULL);
         }
      else if (!nT)
         pT->getExit()->setNextTreeTop(NULL);
      else
         TR_ASSERT(0, "GLU: illegal cfg discovered while swinging blocks");

      from->getExit()->join(to->getEntry());
      if (nF)
         to->getExit()->join(nF->getEntry());
      else
         to->getExit()->setNextTreeTop(NULL);
      }
   else
      {
      // The real swing
      // Search downwards from T till be find a block G, st, G does not
      // fall into G.  This will happen necessarily before we run into F
      // (if F is below T)
      //
      TR::Block *G = nT;
      TR::Block *nG = G->getNextBlock();
      while (nG && isSuccessor(G, nG))
         {
         G = nG;
         nG = nG->getNextBlock();
         }

      if (nG && pT)
         pT->getExit()->join(nG->getEntry());
      else if (!nG)
         pT->getExit()->setNextTreeTop(NULL);
      else if (!pT)
         TR_ASSERT(0, "GLU: illegal cfg discovered while swinging blocks");
      else
         TR_ASSERT(0, "GLU: illegal cfg discovered while swinging blocks");

      from->getExit()->join(to->getEntry());
      if (nF)
         G->getExit()->join(nF->getEntry());
      else
         G->getExit()->setNextTreeTop(NULL);

      }
   }

bool TR_LoopUnroller::isSuccessor(TR::Block *A, TR::Block *B)
   {
   for (auto edge = A->getSuccessors().begin(); edge != A->getSuccessors().end(); ++edge)
      {
      if (B == toBlock((*edge)->getTo()))
         return true;
      }

   return false;
   }

void TR_LoopUnroller::fixExitEdges(TR_Structure *s, TR_Structure *clone,
                                          TR_StructureSubGraphNode *branchNode)
   {
   if (s->asBlock())
      return;

   TR_RegionStructure *region = s->asRegion();
   TR_RegionStructure *clonedRegion = clone->asRegion();

   TR_RegionStructure::Cursor si(*region);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      TR_StructureSubGraphNode *clonedSubNode = findNodeInHierarchy
         (clonedRegion, _blockMapper[CURRENT_MAPPER][subNode->getNumber()]->getNumber());
      fixExitEdges(subNode->getStructure(), clonedSubNode->getStructure());
      }

   ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
   TR::CFGEdge *edge;
   for (edge = ei.getFirst(); edge; edge= ei.getNext())
      {
      if (region->isExitEdge(edge))
         {
         TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(edge->getFrom());
         int32_t toNum = edge->getTo()->getNumber();
         TR::Block *clonedBlock = _blockMapper[CURRENT_MAPPER][toNum];

         // If branch node is non-null: we need to preserve branch exit
         //
         EdgeContext context = InvalidContext;
         if (branchNode && branchNode == fromNode)
            context = ExitEdgeFromBranchNode;

         //First of all find the corresponding cloned node inside the region.
         //We cannot use GET_CLONE_NODE, because it does not have the correct bottom
         // level mappings at this stage
         TR_StructureSubGraphNode *clonedFromNode =
            findNodeInHierarchy
            (clonedRegion, _blockMapper[CURRENT_MAPPER][fromNode->getNumber()]->getNumber());

         TR_ASSERT(clonedFromNode->getStructure()->getParent() == clonedRegion,
                "exit region not originating from inside the region");

         if (clonedBlock) //ie. the exit node was cloned
            {
            TR_StructureSubGraphNode *exitNode =
               findNodeInHierarchy(region->getParent()->asRegion(), toNum);
            addExitEdgeAndFixEverything(clonedRegion, edge, clonedFromNode,
                                        exitNode, clonedBlock, context);
            }
         else
            {
            TR_StructureSubGraphNode *exitNode =
               findNodeInHierarchy(region->getParent()->asRegion(), toNum);
            addExitEdgeAndFixEverything(clonedRegion, edge, clonedFromNode,
                                        exitNode, NULL, context);
            }

         }
      }
   processSwingQueue();
   }

void
TR_LoopUnroller::prepareLoopStructure(TR_RegionStructure *loop)
   {
   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   loop->getBlocks(&blocksInRegion);

   ListIterator<TR::Block> it(&blocksInRegion);
   for (TR::Block *block = it.getCurrent(); block; block = it.getNext())
      {
      TR::TreeTop *tt = block->getLastRealTreeTop();
      if (tt->getNode()->getOpCodeValue() == TR::Goto)
         tt->getNode()->setLocalIndex(-1);
      }
   }

bool
TR_LoopUnroller::isTransactionStartLoop(TR_RegionStructure *loop, TR::Compilation *comp)
   {
   if(comp->getOption(TR_DisableTLE) || !comp->cg()->getSupportsTM())   // if TLE is not supported or on, it is not possible
      return false;

   TR::Block *entryBlock = loop->getEntryBlock();

   for(TR::TreeTop *tt = entryBlock->getEntry() ; tt != entryBlock->getExit() ; tt = tt->getNextTreeTop())
      {
      if(tt->getNode()->getOpCodeValue() == TR::tstart)
         return true;

      }

   return false;
   }


bool
TR_LoopUnroller::isWellFormedLoop(TR_RegionStructure *loop,
                                    TR::Compilation * comp,
                                    TR::Block *&loopInvariantBlock)
   {
   if (loop->getExitEdges().isEmpty())
      return false;

   TR_ScratchList<TR::Block> blocksInLoop(comp->trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocks(&blocksInLoop);
   for (TR::Block *block = blocks.getFirst(); block; block = blocks.getNext())
      {
      if (block->hasExceptionSuccessors() ||
          block->hasExceptionPredecessors())
         return false;
      }

   //Find the node corresponding to loop
   TR_RegionStructure *parent = loop->getParent()->asRegion();
   TR_RegionStructure::Cursor nodeIt(*parent);
   TR_StructureSubGraphNode *loopNode = NULL;
   for (TR_StructureSubGraphNode *node = nodeIt.getFirst(); node; node = nodeIt.getNext())
      if (node->getStructure() == loop)
         {
         loopNode = node;
         break;
         }

   // check if the loop has a pre-header which is a block
   //
   if (loopNode && (loopNode->getPredecessors().size() == 1))
      {
      TR_StructureSubGraphNode *predNode = toStructureSubGraphNode
         (loopNode->getPredecessors().front()->getFrom());

      TR_BlockStructure *predBlock = predNode->getStructure()->asBlock();
      if (predBlock && predBlock->isLoopInvariantBlock())
         {
         // found a pre-header, now check if all the backedges
         // originate from only blockStructures
         //
         TR_StructureSubGraphNode *entryNode = loop->getEntry();
         bool backEdgesFromBlocks = true;
         for (auto e = entryNode->getPredecessors().begin(); e != entryNode->getPredecessors().end(); ++e)
            {
            TR_StructureSubGraphNode *source = toStructureSubGraphNode((*e)->getFrom());

            // not a backedge
            //
            if (!loop->contains(source->getStructure(), loop->getParent()))
               continue;

            if (!source->getStructure()->asBlock())
               {
               backEdgesFromBlocks = false;
               if (comp->trace(OMR::generalLoopUnroller))
                  traceMsg(comp, "found a backedge originating from a regionStructure %p\n", source);
               break;
               }
            }
            if (backEdgesFromBlocks)
               {
               loopInvariantBlock = predBlock->getBlock();
               return true;
               }
            else
               dumpOptDetails(comp, "loop has backedges from other regions, not a well formed loop\n");
         }
      else
        dumpOptDetails(comp, "loop has no loop-invariant block, not a well formed loop\n");
      }

   return false;
   }


TR_LoopUnroller::TR_LoopUnroller(TR::Compilation *c, TR::Optimizer *optimizer, TR_RegionStructure *loop,
                                 TR_StructureSubGraphNode *branchNode,
                                 int32_t unrollCount, int32_t peelCount, TR::Block *invariantBlock,
                                 UnrollKind unrollKind, int32_t vectorSize)
   : _comp(c), _trMemory(c->trMemory()), _optimizer(optimizer), _loop(loop), _vectorSize(vectorSize),
     _branchNode(branchNode), _unrollCount(unrollCount),
     _peelCount(peelCount), _unrollKind(unrollKind),
     _iteration(0), _firstEntryNode(0), _piv(0),
     _spillLoopRequired(false), _branchToExit(false),
     _spillNode((TR_StructureSubGraphNode *)0xdeadf00d),
     _overflowTestBlock((TR::Block *)0xdeadf00d),
     _loopIterTestBlock((TR::Block *)0xdeadf00d),
     _loopInvariantBlock(invariantBlock),
     _loopInvariantBlockAtEnd(false),
     _swingQueue(c->trMemory()),
     _wasEQorNELoop(false), _origLoopCondition(TR::BadILOp),
     _newSymRefs(c->trMemory()),
     _startPosOfUnrolledBodies(NULL),
     _endPosOfUnrolledBodies(NULL),
     _listOfInternalPointers(c->trMemory()), _listOfListsOfArrayAccesses(c->trMemory())
   {
   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure()->asRegion();
   _numNodes = _cfg->getNextNodeNumber();
   if (!_loopInvariantBlock->getExit()->getNextTreeTop())
      _loopInvariantBlockAtEnd = true;
   }

TR_LoopUnroller::TR_LoopUnroller(TR::Compilation *c, TR::Optimizer *optimizer, TR_RegionStructure *loop,
                                 TR_PrimaryInductionVariable *piv, UnrollKind unrollKind,
                                 int32_t unrollCount, int32_t peelCount, TR::Block *invariantBlock, int32_t vectorSize)
   : _comp(c), _trMemory(c->trMemory()), _optimizer(optimizer), _loop(loop), _unrollKind(unrollKind),  _vectorSize(vectorSize),
     _unrollCount(unrollCount), _peelCount(peelCount), _piv(piv),
     _spillNode(0), _overflowTestBlock(0), _loopIterTestBlock(0), _loopInvariantBlock(invariantBlock),
     _loopInvariantBlockAtEnd(false), _iteration(0), _firstEntryNode(0),// goto init
     _swingQueue(c->trMemory()),
     _wasEQorNELoop(false), _origLoopCondition(TR::BadILOp),
     _newSymRefs(c->trMemory()),
     _startPosOfUnrolledBodies(NULL),
     _endPosOfUnrolledBodies(NULL),
     _listOfInternalPointers(c->trMemory()), _listOfListsOfArrayAccesses(c->trMemory())
   {
   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure()->asRegion();
   _numNodes = _cfg->getNextNodeNumber();

   if (!_loopInvariantBlock->getExit()->getNextTreeTop())
      _loopInvariantBlockAtEnd = true;

   TR::Block *branchBlock = piv->getBranchBlock();
   _branchNode =  loop->findSubNodeInRegion(branchBlock->getNumber());

   TR::Node *branch = branchBlock->getLastRealTreeTop()->getNode();

   _spillLoopRequired = (unrollKind == UnrollWithResidue);

   bool indexOnLHS = nodeRefersToSymbol(branch->getFirstChild(), piv->getSymRef()->getSymbol());
   TR_ASSERT(indexOnLHS, "FIXME: loop not canonicalized");

   _branchToExit =
      !loop->contains(branch->getBranchDestination()->getNode()->getBlock()->getStructureOf(),
                      loop->getParent());
   }

// ::unrollCountedLoop
bool
TR_LoopUnroller::unroll(TR::Compilation *comp, TR_RegionStructure *loop,
                        TR_PrimaryInductionVariable *piv, UnrollKind unrollKind,
                        int32_t unrollCount, int32_t peelCount, TR::Optimizer *optimizer)
   {
   // We dont do peeling as of yet
   //
   if (peelCount != 0)
      {
      //if (comp->trace(generalLoopUnroller))
         dumpOptDetails(comp, "Cannot unroll loop %d: peeling not supported yet\n", loop->getNumber());
      return false;
      }

   TR::Block *loopInvariantBlock = NULL;
   if (!isWellFormedLoop(loop, comp, loopInvariantBlock))
      {
	//if (comp->trace(generalLoopUnroller))
         dumpOptDetails(comp, "Cannot unroll loop %d: not a well formed loop\n", loop->getNumber());
      return false;
      }

   if(isTransactionStartLoop(loop, comp))
      {
         dumpOptDetails(comp, "Cannot unroll loop %d: it is a transaction start loop\n",loop->getNumber());
         return false;
      }

   TR_LoopUnroller unroller(comp, optimizer, loop, piv, unrollKind, unrollCount, peelCount, loopInvariantBlock);

   if (1)
      {
      TR::Block *branchBlock = unroller._branchNode->getStructure()->asBlock()->getBlock();
      TR::Node  *branch = branchBlock->getLastRealTreeTop()->getNode();
      TR::ILOpCodes op = branch->getOpCodeValue();

      if (unroller._spillLoopRequired)
         {

         bool backEdgeFound = isBranchAtEndOfLoop(loop, branchBlock);

         if (!backEdgeFound)
            {
            TR_ScratchList<TR::Block> blocksInLoop(comp->trMemory());
            loop->getBlocks(&blocksInLoop);

            ListIterator<TR::Block> blocksIt(&blocksInLoop);
            TR::Block *nextBlock;
            for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
                {
                if (nextBlock != loop->getEntryBlock())
                   break;
                }

            bool loopCanBeUnrolled = false;
            if (branchBlock == loop->getEntryBlock())
               {
               if ((branchBlock->getLastRealTreeTop() == branchBlock->getFirstRealTreeTop()) ||
                   ((branchBlock->getFirstRealTreeTop()->getNode()->getOpCodeValue() == TR::asynccheck) &&
                    (branchBlock->getLastRealTreeTop() == branchBlock->getFirstRealTreeTop()->getNextTreeTop())))
                  loopCanBeUnrolled = true;
               }

            if (!loopCanBeUnrolled)
               {
               if (blocksInLoop.isDoubleton() && nextBlock->getSuccessors().size() == 1)
                  {
                  if ((nextBlock->getEntry()->getNextTreeTop() == nextBlock->getExit()) ||
                      (nextBlock->getLastRealTreeTop() == nextBlock->getFirstRealTreeTop()) ||
                      ((nextBlock->getFirstRealTreeTop()->getNode()->getOpCodeValue() == TR::asynccheck) &&
                       (nextBlock->getLastRealTreeTop() == nextBlock->getFirstRealTreeTop()->getNextTreeTop())))
                     loopCanBeUnrolled = true;
                  }
               }

            if (loopCanBeUnrolled)
               {
               // Either the entry block has the branch condition or the only 2 blocks in the loop are
               // the entry and the branch block
               // Further the only code other from the entry block in the loop must be the branch (and an optional async check)
               //
               }
            else
               {
               dumpOptDetails(comp, "Cannot unroll loop %d: exit condition is not in a block containing a backedge\n",
                                      loop->getNumber());
               return false;
               }
            }
         }

      if (1)
         {
         bool foundExitEdge = false;
         ListIterator<TR::CFGEdge> it(&loop->getExitEdges());
         for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
            {
            if (edge->getFrom()->getNumber() == branchBlock->getNumber())
               {
               foundExitEdge = true;
               break;
               }
            }

         if (!foundExitEdge)
            {
            dumpOptDetails(comp, "Cannot unroll loop %d: eq loop with branch not an exit out of the loop\n",
                                         loop->getNumber());
            return false;
            }
         }

      // Special case handing of eq or ne loops
      //
      if (TR::ILOpCode::isEqualCmp(op) || TR::ILOpCode::isNotEqualCmp(op))
         {
         if (piv->getDeltaOnExitEdge() != 1 &&
             piv->getDeltaOnExitEdge() != -1)
            {
            dumpOptDetails(comp, "Cannot unroll loop %d: eq loop with non unit inc/dec not supported\n",
                                         loop->getNumber());
            return false;
            }

         // For ifeq loops make sure the branch exits
         // for ifne loops make sure it does not.
         //
         if ((TR::ILOpCode::isEqualCmp(op)    && !unroller._branchToExit) ||
             (TR::ILOpCode::isNotEqualCmp(op) &&  unroller._branchToExit) )
            {
            dumpOptDetails(comp, "Cannot unroll loop %d: stange controlling test\n",
                                         loop->getNumber());
            return false;
            }

         // Canonicalize the loop condition
         //
         //       | incr | decr
         //    ----------------
         //    ne |  lt  |  gt
         //    eq |  ge  |  le
         //
         //
         TR_ASSERT(!branch->getFirstChild()->getType().isAddress(), "Excpeting an integral compare with integral first child");
         TR::ILOpCodes opCode = branch->getFirstChild()->getType().isInt64() ?
            (branch->getOpCode().isUnsignedCompare() ? TR::iflucmpge : TR::iflcmpge) :
            (branch->getOpCode().isUnsignedCompare() ? TR::ifiucmpge : TR::ificmpge) ;

         if (!unroller.isIncreasingLoop())
            opCode = ((TR::ILOpCode*)&opCode)->getOpCodeForSwapChildren();

         if (TR::ILOpCode::isNotEqualCmp(op))
            opCode = ((TR::ILOpCode*)&opCode)->getOpCodeForReverseBranch();

         if (performTransformation(comp, "%sCanonicalize branch test %p for eq/ne loop %d\n",
                                    OPT_DETAILS, branch, loop->getNumber()))
            {
            unroller._wasEQorNELoop = true;
            unroller._origLoopCondition = branch->getOpCodeValue();
            TR::Node::recreate(branch, opCode);
            }
         }

      op = branch->getOpCodeValue();
      if (TR::ILOpCode::isEqualCmp(op) || TR::ILOpCode::isNotEqualCmp(op))
         {
         dumpOptDetails(comp, "Cannot unroll loop %d: unsupported branch opcode\n",
                                      loop->getNumber());
         return false;
         }
      }

   if (unrollKind == CompleteUnroll)
      {
      TR::Block     *branchBlock = unroller._branchNode->getStructure()->asBlock()->getBlock();
      TR::Node      *branch      = branchBlock->getLastRealTreeTop()->getNode();
      TR::Block     *destBlock   = branch->getBranchDestination()->getNode()->getBlock();
      TR_Structure *destStruct  = destBlock->getStructureOf();
      if (!loop->contains(destStruct, loop->getParent()))
         {
         dumpOptDetails(comp, "Cannot unroll loop %d: complete unroll of a loop with a reversed branch\n",
                                      loop->getNumber());
         return false;
         }
      }

   if(unroller.isInternalPointerLimitExceeded())
      {
      dumpOptDetails(comp,"Cannot unroll loop %d: number of internal pointers has been exceeded\n",loop->getNumber());
      return false;
      }

   if (!performTransformation(comp, "%sUnrolling counted loop %d [unrollfactor:%d, peelcount:%d, spill:%s completeunroll:%s]\n",
                               OPT_DETAILS, loop->getNumber(), unrollCount+1, peelCount,
                               unrollKind == UnrollWithResidue? "yes" : "no",
                               unrollKind == CompleteUnroll ? "yes" : "no"))
      {
      return false;
      }
   unroller.unroll(loop, unroller._branchNode);
   //printf("--secs-- unrolled in %s\n", comp->signature());

   return true;
   }

// ::unrollNonCountedLoop
bool
TR_LoopUnroller::unroll(TR::Compilation *comp, TR_RegionStructure *loop,
                        int32_t unrollCount, int32_t peelCount, TR::Optimizer *optimizer)
   {
   // We dont do peeling as of yet
   //
   if (peelCount != 0)
      {
      //if (comp->trace(generalLoopUnroller))
         dumpOptDetails(comp, "Cannot unroll loop %d: peeling not supported yet\n", loop->getNumber());
      return false;
      }

   TR::Block *loopInvariantBlock = NULL;
   if (!isWellFormedLoop(loop, comp, loopInvariantBlock))
      {
      //if (comp->trace(generalLoopUnroller))
         dumpOptDetails(comp, "Cannot unroll loop %d: not a well formed loop\n", loop->getNumber());
      return false;
      }

   // Find a candidate branch node
   // FIXME: skip goto blocks
   TR_StructureSubGraphNode *branchNode = loop->getExitEdges().getListHead()->getData()->getFrom()->asStructureSubGraphNode();
   TR_BlockStructure *branchStr = branchNode->getStructure()->asBlock();
   if (!branchStr)
      {
      //if (comp->trace(generalLoopUnroller))
         dumpOptDetails(comp, "Cannot unroll loop %d: branchnode %d is not a block\n",
                loop->getNumber(), branchNode->getNumber());
      return false;
      }

   TR_LoopUnroller unroller(comp, optimizer, loop, branchNode, unrollCount, peelCount, loopInvariantBlock, GeneralUnroll);

   if (unroller._spillLoopRequired)
      {
      TR::Block *branchBlock = branchStr->getBlock();

      bool backEdgeFound = isBranchAtEndOfLoop(loop, branchBlock);

      if (!backEdgeFound)
         {
         TR_ScratchList<TR::Block> blocksInLoop(comp->trMemory());
         loop->getBlocks(&blocksInLoop);

         ListIterator<TR::Block> blocksIt(&blocksInLoop);
         TR::Block *nextBlock;
         for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
            {
            if (nextBlock != loop->getEntryBlock())
               break;
            }

         bool loopCanBeUnrolled = false;
         if (branchBlock == loop->getEntryBlock())
            {
            if ((branchBlock->getLastRealTreeTop() == branchBlock->getFirstRealTreeTop()) ||
                ((branchBlock->getFirstRealTreeTop()->getNode()->getOpCodeValue() == TR::asynccheck) &&
                 (branchBlock->getLastRealTreeTop() == branchBlock->getFirstRealTreeTop()->getNextTreeTop())))
               loopCanBeUnrolled = true;
            }

         if (!loopCanBeUnrolled)
            {
            if (blocksInLoop.isDoubleton() && nextBlock->getSuccessors().size() == 1)
               {
               if ((nextBlock->getEntry()->getNextTreeTop() == nextBlock->getExit()) ||
                   (nextBlock->getLastRealTreeTop() == nextBlock->getFirstRealTreeTop()) ||
                   ((nextBlock->getFirstRealTreeTop()->getNode()->getOpCodeValue() == TR::asynccheck) &&
                    (nextBlock->getLastRealTreeTop() == nextBlock->getFirstRealTreeTop()->getNextTreeTop())))
                  loopCanBeUnrolled = true;
               }
            }

         if (loopCanBeUnrolled)
            {
            // Either the entry block has the branch condition or the only 2 blocks in the loop are
            // the entry and the branch block
            // Further the only code other from the entry block in the loop must be the branch (and an optional async check)
            //
            }
         else
            {
            dumpOptDetails(comp, "Cannot unroll loop %d: exit condition is not in a block containing a backedge\n",
                                  loop->getNumber());
               return false;
            }
         }
      }

   if (!performTransformation(comp, "%sUnrolling non-counted loop %d [unrollfactor:%d, peelcount:%d]\n",
                               OPT_DETAILS, loop->getNumber(), unrollCount + 1, peelCount))
      return false;
   unroller.unroll(loop, branchNode);
   //printf("--secs-- noncounted loop in %s\n", comp->signature());

   return true;
   }

TR_GeneralLoopUnroller::TR_GeneralLoopUnroller(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager)
   {
   static const char *e = feGetEnv("TR_gluBasicSizeThreshold");
   _basicSizeThreshold = e ? atoi(e) : (comp()->getOption(TR_ProcessHugeMethods) ? 500 : 82);
   };

int32_t
TR_GeneralLoopUnroller::perform()
   {
   if (optimizer()->optsThatCanCreateLoopsDisabled())
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _cfg = comp()->getFlowGraph();
   TR_RegionStructure *root = _cfg->getStructure()->asRegion();

   // Faking having profile info has mixed performance impact.
   // We also know that it tends to increase codesize, some by ##%
   //
   _haveProfilingInfo = true;

   List<TR_RegionStructure> innerLoops(trMemory());
   collectNonColdInnerLoops(root, innerLoops);
   if (innerLoops.isEmpty())
      {
      return 0;
      }

   if (comp()->getMethodHotness() == veryHot &&
       comp()->isProfilingCompilation())
      {
      //profileNonCountedLoops(innerLoops);
      return 1;
      }

   int32_t nodeCount = comp()->getAccurateNodeCount();
   int32_t budget;
   if (comp()->getOption(TR_ProcessHugeMethods))
      {
      budget = 100000;
      }
   else if (comp()->getMethodHotness() == hot ||
       comp()->getMethodHotness() == veryHot)
      {
      budget = 2000 - nodeCount/2;
      if (budget < 300) budget = 300;
      }
   else if (comp()->getMethodHotness() == scorching)
      {
      bool randomly = comp()->getOption(TR_Randomize)? randomBoolean() : false;
      if (randomly)
         {
         budget = randomInt(2000, 20000);
         traceMsg(comp(),"\nTR_Randomize Enabled||budget:%d\n", budget);
         }
      else
         {
         if (nodeCount > 6000)
            budget = 625;
         else if (nodeCount > 5000)
            budget = 750;
         else if (nodeCount > 3000)
            budget = 1000;
         else if (nodeCount > 1000)
            budget = 1250;
         else
            budget = 1500;
         }

      }
   else
      budget = 300;

   //if (trace())
      dumpOptDetails(comp(), "Starting GLU with a budget of %d.  Total number of nodes in method %d\n", budget, nodeCount);

   TR_ScratchList<UnrollInfo> Q(trMemory());
   ListIterator<TR_RegionStructure> it(&innerLoops);
   for (TR_RegionStructure *loop = it.getFirst(); loop; loop = it.getNext())
      {
      int32_t unrollCount = 0, peelCount = 0, cost = 0;
      TR_LoopUnroller::UnrollKind unrollKind;
      int32_t weight = weighNaturalLoop(loop, unrollKind, unrollCount, peelCount, cost);

      bool loopCanBeUnrolled = true;

      if (weight > 0 && unrollCount > 0 && loopCanBeUnrolled)
         {
         UnrollInfo *info = new (trStackMemory()) UnrollInfo(loop, weight, cost,
                                                       unrollKind, unrollCount,
                                                       peelCount);
         Q.add(info);
         }
      }

   while (1)
      {
      if (budget < 0)
         break;

      UnrollInfo *top = 0;
      ListIterator<UnrollInfo> uit(&Q);
      for (UnrollInfo *i = uit.getFirst(); i; i = uit.getNext())
         {
         if (!top || top->_weight < i->_weight)
            top = i;
         }

      if (!top)
         break;

      Q.remove(top);

      if (top->_cost > budget)
         continue;

      budget -= top->_cost;

      if (trace())
         traceMsg(comp(), "<unroll loop=\"%d\">\n", top->_loop->getNumber());

      if (top->_loop->getPrimaryInductionVariable())
         TR_LoopUnroller::unroll(comp(), top->_loop, top->_loop->getPrimaryInductionVariable(),
                                 top->_unrollKind, top->_unrollCount, top->_peelCount, optimizer());
      else
         TR_LoopUnroller::unroll(comp(), top->_loop, top->_unrollCount, top->_peelCount, optimizer());

      if (trace())
         traceMsg(comp(), "</unroll>\n");
      }

   return 1;
   }

void
TR_GeneralLoopUnroller::collectNonColdInnerLoops(TR_RegionStructure *region, List<TR_RegionStructure> &innerLoops)
   {
   if (region->getEntryBlock()->isCold()) return;

   TR_RegionStructure::Cursor it(*region);
   List<TR_RegionStructure> myInnerLoops(trMemory());
   for (TR_StructureSubGraphNode *node = it.getFirst();
        node;
        node = it.getNext())
      {
      if (node->getStructure()->asRegion())
         collectNonColdInnerLoops(node->getStructure()->asRegion(), myInnerLoops);
      }

   if (region->isNaturalLoop() && myInnerLoops.isEmpty())
      innerLoops.add(region);
   else
      innerLoops.add(myInnerLoops);
   }

void
TR_GeneralLoopUnroller::gatherStatistics
(TR_Structure *str,
 int32_t &numNodes, int32_t &numBlocks,
 int32_t &numBranches, int32_t &numSubscripts,
 LoopWeightProbe &lwp)
   {
   if (str->asBlock())
      {
      TR::Block *block = str->getEntryBlock();
      TR_ASSERT(block->getEntry(), "Method entry/exit cannot be inside a loop");

      for (TR::TreeTop *tt = block->getFirstRealTreeTop();
           tt != block->getExit(); tt = tt->getNextRealTreeTop())
         countNodesAndSubscripts(tt->getNode(), numNodes, numSubscripts, lwp);

      numBlocks++;
      const auto &op = block->getLastRealTreeTop()->getNode()->getOpCode();
      if(op.isBranch())
         numBranches++;
      return;
      }
   else
      {
      TR_RegionStructure *region = str->asRegion();

      TR_RegionStructure::Cursor it(*region);
      for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
         {
         TR_Structure *s = node->getStructure();
         gatherStatistics(s, numNodes, numBlocks, numBranches, numSubscripts, lwp);
         }
      }
   }


void
TR_GeneralLoopUnroller::countNodesAndSubscripts(TR::Node *node, int32_t &numNodes, int32_t &numSubscripts,
      LoopWeightProbe &lwp)
   {
   if (!node) return;
   if (node->getVisitCount() == comp()->getVisitCount()) return;
   node->setVisitCount(comp()->getVisitCount());

   if (node->getOpCode().isLikeDef())
      lwp._numKilled++;
   if (node->getOpCode().isLikeUse())
      lwp._numUsed++;
   if (node->getOpCode().hasSymbolReference())
      {
      auto symref = node->getSymbolReference();
      if (symref)
         {
         if (node->getOpCode().isLikeDef())
            {
            TR::BitVector aliases(comp()->allocator());
            symref->getUseDefAliases().getAliases(aliases);
            lwp._defd |= aliases;
            }
         if (node->getOpCode().isLikeUse())
            {
            // Only looks at symrefs used in loop
            lwp._used[symref->getReferenceNumber()] = 1;
            }
         }
      }

   if (node->getOpCode().isLoadConst())
      {
      if(!comp()->cg()->isMaterialized(node))
         {
         lwp._numUnmatConst++;
         return;
         }
      lwp._numMatConst++;
      }

   if (node->getOpCode().isCall())
      {
      lwp._numCalls++;
      if (auto methodSym = node->getSymbol()->getMethodSymbol())
         {
         if (methodSym->isPureFunction())
            lwp._numPureFunctions++;

         // TODO: leaf routine?

         // Is it always inlined BIF?
         if (methodSym->getRecognizedMethod() != TR::unknownMethod)
            lwp._numBIFs++;
         }
      }

   if (node->getOpCodeValue() != TR::treetop)
      numNodes++;

   if (node->getOpCodeValue() == TR::aiadd || node->getOpCodeValue() == TR::aladd)
      {
      // TODO: make this more intelligent -- check if the subscript indexes
      // an induction variable
      numSubscripts++;
      }

   // recurse children
   for (int32_t child = 0; child < node->getNumChildren(); ++child)
      countNodesAndSubscripts(node->getChild(child), numNodes, numSubscripts, lwp);
   }

void
TR_GeneralLoopUnroller::profileNonCountedLoops(List<TR_RegionStructure> &innerLoops)
   {
   ListIterator<TR_RegionStructure> it(&innerLoops);
   for (TR_RegionStructure *loop = it.getFirst(); loop; loop = it.getNext())
      {
      if (loop->getPrimaryInductionVariable())
         continue;

      TR_StructureSubGraphNode *loopNode = loop->getParent()->findSubNodeInRegion(loop->getNumber());
      if (loopNode->getPredecessors().size() != 1)
         continue;

      if (loop->getExitEdges().getSize() != 1)
         continue;

      TR::Block *entryBlock = loop->getEntryBlock();
      TR::Node  *firstNode  = entryBlock->getFirstRealTreeTop()->getNode();
      TR_StructureSubGraphNode *invarNode = loopNode->getPredecessors().front()->getFrom()->asStructureSubGraphNode();

      if (!invarNode->getStructure()->asBlock())
         continue;

      TR::Block *invarBlock = invarNode->getStructure()->asBlock()->getBlock();

      if (!performTransformation(comp(), "%sInserting artificial counter in loop %d in order to profile the itercount\n",
                                  OPT_DETAILS, loop->getNumber()))
         continue;

      TR::SymbolReference *counterSymRef =
         comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int64);

      TR::Node *zero = TR::Node::create(firstNode, TR::lconst, 0); zero->setLongInt(0);
      TR::Node *one  = TR::Node::create(firstNode, TR::lconst, 0); one ->setLongInt(1);

      invarBlock->prepend(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::lstore, 1, 1, zero, counterSymRef)));
      entryBlock->prepend(TR::TreeTop::create(comp(), TR::Node::create(TR::lstore, 1,
                                                         TR::Node::create(TR::ladd, 2,
                                                                         TR::Node::createLoad(firstNode, counterSymRef),
                                                                         one))));

      int32_t postNum = loop->getExitEdges().getListHead()->getData()->getTo()->getNumber();
      TR_StructureSubGraphNode *postNode = TR_LoopUnroller::findNodeInHierarchy(loop->getParent()->asRegion(),
                                                                                postNum);
      TR::Block *postBlock = postNode->getStructure()->getEntryBlock();
      postBlock->prepend(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::lstore, 1, 1,
                                                        TR::Node::createLoad(firstNode, counterSymRef),
                                                        counterSymRef)));
      //FIXME: complete - mark the node as needs to be profiled

      }
   }

static bool exitsLoop(TR::Compilation *comp,
                      TR_RegionStructure *loop,
                      TR_StructureSubGraphNode *source)
   {
   for (auto edge = source->getSuccessors().begin(); edge != source->getSuccessors().end(); ++edge)
      {
      if (loop->getExitEdges().find(*edge))
         return true;
      }
   return false;
   }

#define HIGH_FREQ_LOOP_CUTOFF 7500

// Returns the weight
int32_t
TR_GeneralLoopUnroller::weighNaturalLoop(TR_RegionStructure *loop,
                                         TR_LoopUnroller::UnrollKind &unrollKind,
                                         int32_t &unrollCount, int32_t &peelCount,
                                         int32_t &cost)
   {
   const int32_t maxIterCountForCompleteUnroll = 9;

   int32_t maxSizeForSingleBlockCompleteUnroll;
   int32_t maxSizeForCompleteUnroll;
   int32_t maxSizeThreshold;
   int32_t maxSizeThresholdForNonCountedLoops;

   maxSizeForSingleBlockCompleteUnroll         = 8*_basicSizeThreshold;
   maxSizeForCompleteUnroll                    = 6*_basicSizeThreshold;
   maxSizeThreshold                            = 4*_basicSizeThreshold;
   maxSizeThresholdForNonCountedLoops          = 3*_basicSizeThreshold;

   TR::Block *entryBlock = loop->getEntryBlock();
   if (entryBlock->isCold())
      return -1;

   comp()->incVisitCount();

   int32_t numNodes = 0, numBlocks = 0, numBranches = 0, numSubscripts = 0;
   LoopWeightProbe lwp(comp());
   gatherStatistics(loop, numNodes, numBlocks, numBranches, numSubscripts, lwp);

   int32_t weight  = 0;
   int32_t entryBlockFrequency = _haveProfilingInfo ? entryBlock->getFrequency() : loop->getNestingDepth() * 8;
   if (entryBlockFrequency == 0)
      entryBlockFrequency = 1;

   dumpOptDetails(comp(), "\nAnalyzing Loop %d with entryBlock %d\n", loop->getNumber(), entryBlock->getNumber());
   dumpOptDetails(comp(), "\tnumNodes = %d, numBlocks = %d, entryBlockFreq = %d, numSubscripts = %d\n", numNodes, numBlocks, entryBlockFrequency, numSubscripts);

   // Aliasing heuristics
   TR::BitVector usedOnly(comp()->allocator());
   lwp._used.Andc(lwp._defd, usedOnly);
   TR::BitVector useDefd(comp()->allocator());
   lwp._used.And(lwp._defd, useDefd);
   if (trace())
      {
      dumpOptDetails(comp(), "\tnumKilled = %u, numUsed = %u, numMatConst = %u, numUnmatConst = %u",
            lwp._numKilled, lwp._numUsed, lwp._numMatConst, lwp._numUnmatConst);
      dumpOptDetails(comp(), "\nUSE: ");
      (*comp()) << lwp._used;
      dumpOptDetails(comp(), "\nDEF: ");
      (*comp()) << lwp._defd;
      dumpOptDetails(comp(), "\nUSEONLY: ");
      (*comp()) << usedOnly;
      dumpOptDetails(comp(), "\nUSEDEFD: ");
      (*comp()) << useDefd;
      dumpOptDetails(comp(), "\n");
      }

   auto piv = loop->getPrimaryInductionVariable();
   if (piv)
      useDefd[piv->getSymRef()->getReferenceNumber()] = 0; // Ignore PIV in calculation
   int32_t numUsed = lwp._used.PopulationCount();
   int32_t numUseOnly = usedOnly.PopulationCount();
   int32_t numUseDefd = useDefd.PopulationCount();
   double ratio = (double)numUseOnly / (double)numUsed;
   const int32_t minNumUseOnly = 1;
   const int32_t maxNumUseDefd = 90;
   const double minRatio = 0.2;
   if (trace())
      {
      traceMsg(comp(), "numUsed = %i, numUseOnly = %i(min:%i), numUseDefd = %i(max:%i)\n",
            numUsed, numUseOnly, minNumUseOnly, numUseDefd, maxNumUseDefd);
      traceMsg(comp(), "ratio = numUseOnly/numUsed = %g(minRatio:%g) \n", ratio, minRatio);
      traceMsg(comp(), "numCalls = %u, numBIFs = %u, numPureFns = %u\n",
            lwp._numCalls, lwp._numBIFs, lwp._numPureFunctions);
      }

   // check for multiple branch exits
   //
   if (piv)
      {
      // FIXME: we cannot currently handle loops with multiple branches
      // tagged as loops that contain a primary induction variable because
      // the branches are not fixed up correctly. So tentatively unroll as uncounted
      //
      int32_t numBranches = loop->getPrimaryInductionVariable()->getNumLoopExits();
      dumpOptDetails(comp(), "loop %d has primary iv and %d legitimate branches\n", loop->getNumber(), numBranches);
      if (numBranches > 1)
         {
         ///TR_ASSERT(false, "removing primary induction variable\n");
         loop->setPrimaryInductionVariable(NULL);
         }

      // still have the primary induction variable
      //
      if (loop->getPrimaryInductionVariable())
         {
         // some loops have multiple backedges that do not exit the loop and these
         // branches do not use the loop controlling variable. in such cases, the
         // loop cannot be unrolled as a counted loop.
         //
         TR_StructureSubGraphNode *entryNode = loop->getEntry();
         bool backEdgesFromBlocks = true;
         for (auto e = entryNode->getPredecessors().begin(); e != entryNode->getPredecessors().end(); ++e)
            {
            TR_StructureSubGraphNode *source = toStructureSubGraphNode((*e)->getFrom());

            // not a backedge or the subNode exits the loop
            //
            if (!loop->contains(source->getStructure(), loop->getParent()) ||
                  exitsLoop(comp(), loop, source))
               continue;

            // this is a subNode that contains a backedge and does not
            // exit the loop. make sure the branchNode only contains the piv
            //
            if (source->getStructure()->asBlock())
               {
               TR::Node *branchNode = source->getStructure()->asBlock()->getBlock()->getLastRealTreeTop()->getNode();
               ///traceMsg(comp(), "got branchnode = %p\n", branchNode);
               // if the backedge contains a biv (not the primary iv), then unroll conservatively as non-counted
               //
               if (!branchNode->getOpCode().isGoto() &&
                     !branchContainsInductionVariable(loop, branchNode))
                  {
		  dumpOptDetails(comp(), "backedge branch [%p] is controlled using a biv, so unrolling as non-counted\n", branchNode);
                  loop->setPrimaryInductionVariable(NULL);
                  break;
                  }
               }
            }
         }
      }

   // If we have profile information available, use that to check the the inner loop is
   // relatively hotter than the outer loop.  Otherwise there is not sufficient benefit
   // in unrolling the inner loop.  When profiling information is not available, we
   // statically assume that the inner loops run more frequently.  High relative freq
   // of the inner loop is going provide a weight boost.
   //
   TR_RegionStructure *outerLoop = (TR_RegionStructure*)loop->getContainingLoop();
   if (_haveProfilingInfo && outerLoop)
      {
      int32_t maxCount = comp()->getFlowGraph()->getMaxFrequency();

      if (maxCount > 0)
         {
         int32_t outerLoopFrequency = outerLoop->getEntryBlock()->getFrequency();

         dumpOptDetails(comp(),"\t outerLoop number = %d outerLoop->getEntryBlock->GetNumber = %d\n",outerLoop->getNumber(),outerLoop->getEntryBlock()->getNumber());
         float outerLoopRelativeFrequency = outerLoopFrequency == 6 ?
            1.3 + (10*entryBlockFrequency / (float)maxCount) :
            entryBlockFrequency / (float)outerLoopFrequency;

         dumpOptDetails(comp(), "\touterloop relative frequency = %.2g\n", outerLoopRelativeFrequency);

         if (outerLoopRelativeFrequency <= 1.3f) // FIXME: const
            {
            dumpOptDetails(comp(), "\trejecting loop because its not warm enough compared to the outer loop\n");
            return -1;
            }

         // Increase the weight
         weight += (int)(100 * outerLoopRelativeFrequency); // FIXME: const
         }
      }

   unrollKind = TR_LoopUnroller::NoUnroll;
   int32_t spillLoop   = 0;
   piv = loop->getPrimaryInductionVariable();
   if (piv)
      {
      int32_t boostFactor = 0;
      int32_t iterCount = piv->getIterationCount();

      // Do not unroll if entry and exit value imply wraparound.
      bool wrapAround = false;
      bool increasingLoop = piv->getDeltaOnBackEdge() > 0;
      TR::Node *entryValueNode = piv->getEntryValue();
      bool entryValueConst = entryValueNode ? entryValueNode->getOpCode().isLoadConst() : false;
      int64_t entryValue = entryValueConst ? entryValueNode->get64bitIntegralValue() : 0;
      TR::Node *exitBoundNode = piv->getExitBound();
      bool exitBoundConst = exitBoundNode ? exitBoundNode->getOpCode().isLoadConst() : false;
      int64_t exitBound = exitBoundConst ? exitBoundNode->get64bitIntegralValue() : 0;
      if (entryValueConst && exitBoundConst)
         {
         if ((increasingLoop && (entryValue > exitBound)) || (!increasingLoop && (entryValue < exitBound)))
            {
            dumpOptDetails(comp(), "\twraparound, %s, entry value: 0x%x, exit bound: 0x%x\n",increasingLoop?"loop increasing":"loop decreasing",entryValue,exitBound);
            wrapAround = true;
            }
         }

      if (iterCount > 0 && !wrapAround)
         {
         dumpOptDetails(comp(), "\twe have a primary induction variable #%i with itercount %d\n",
               piv->getSymRef()->getReferenceNumber(), iterCount);


         if (iterCount <= maxIterCountForCompleteUnroll &&
             ((iterCount * numNodes <= maxSizeForSingleBlockCompleteUnroll && numBlocks == 1) ||
              (iterCount * numNodes <= maxSizeForCompleteUnroll)))
            {
            unrollCount = iterCount - 1;
            boostFactor = 400;
            unrollKind = TR_LoopUnroller::CompleteUnroll;
            }
         else
            {
            int32_t preferredUnrollFactor = cg()->getPreferredLoopUnrollFactor();

            bool multipleOf2 = iterCount % 2 == 0;
            bool multipleOf3 = iterCount % 3 == 0;
            bool multipleOf4 = iterCount % 4 == 0;
            bool multipleOfPreferred = preferredUnrollFactor > 0 && (iterCount % preferredUnrollFactor) == 0;

            if (!multipleOfPreferred && !multipleOf2 && !multipleOf3)
               {
               //FIXME once peeling is supported
               //unrollCount = 1; peelCount = 1;
               //unrollKind  = TR_LoopUnroller::PeelOnceUnrollOnce;
               }
            else
               {
               int originalUnrollCount = 0;
               if (comp()->getOption(TR_Randomize))
                  {
                  unrollCount =
                              (multipleOf4 ? 3 :
                              multipleOf3 ? 2 :
                              multipleOf2 ? 1 : 0);

                /*unrollCount = (multipleOf4 ? randomInt(0,3) :
                                multipleOf3 ? randomInt(0,2) :
                                multipleOf2 ? randomInt(0,1) : 0);
                       traceMsg(comp(), "\nTR_Randomize Enabled||TR_GeneralLoopUnroller::weighNaturalLoop(), unrollCount:%d Original unrollCount:%d", unrollCount, originalUnrollCount);*/
                   }
               else
                  {
                  unrollCount = (multipleOfPreferred ? preferredUnrollFactor - 1 :
                                 multipleOf4 ? 3 :
                                 multipleOf3 ? 2 :
                                 multipleOf2 ? 1 : 0);
                  }
               if (unrollCount > 0)
                  {
                  unrollKind = TR_LoopUnroller::ExactUnroll;

                  if ((unrollCount < 3) &&
                      (comp()->getMethodHotness() >= hot) &&
                      (iterCount >= 64))
                     {
                     int32_t unrollCountWithResidue = unrollCount;
                     for (int32_t j = 3; j > unrollCount; --j)
                        if (numNodes * j <= maxSizeThreshold)
                           {
                           unrollCountWithResidue = j; break;
                           }

                     //dumpOptDetails(comp(), "unrollCountWithResidue = %d\n", unrollCountWithResidue);
                     if (unrollCountWithResidue > unrollCount)
                        {
                        unrollCount = unrollCountWithResidue;
                        spillLoop = 1;
                        //boostFactor = 100;
                        unrollKind = TR_LoopUnroller::UnrollWithResidue;
                        }
                     }
                  }
               }
            }
         boostFactor = 300;
         }

      if (iterCount <= 0 || unrollKind == TR_LoopUnroller::NoUnroll)
         {
         if (iterCount <= 0)
            dumpOptDetails(comp(), "\twe have a primary induction variable with unknown itercount stride=%d\n", piv->getDeltaOnBackEdge());

         // Default 4x unrolling unless codegen specifies a factor > 0
         const int32_t preferredUnrollFactor = cg()->getPreferredLoopUnrollFactor();
         const int32_t unrollFactor = preferredUnrollFactor > 0 ? preferredUnrollFactor : 4;

         for (int32_t j = unrollFactor - 1; j >= 1; --j)
            if ((numNodes * (j +1 + peelCount)) <= maxSizeThreshold)//iris
               {
               unrollCount = j; break;
               }
         spillLoop = 1;
         boostFactor = 100;
         unrollKind = TR_LoopUnroller::UnrollWithResidue;
         }

      int32_t growth = (unrollCount + peelCount + spillLoop) * numNodes;
      if (unrollKind != TR_LoopUnroller::CompleteUnroll &&
          growth > maxSizeThreshold)
         {
         dumpOptDetails(comp(), "\tloop unroll size threshold hit: %d using an unroll count of %d\n",
                          growth, unrollCount);
         return -1;
         }

      if ((unrollKind == TR_LoopUnroller::ExactUnroll) && (iterCount == (unrollCount + 1)))
         {
         // eliminate the back edge from an exact unroll where ic == (uc + 1)
         boostFactor = 400;
         unrollKind = TR_LoopUnroller::CompleteUnroll;
         dumpOptDetails(comp(), "\tloop unroll count %d iterCount %d, making complete unroll out of exact unroll\n", unrollCount, iterCount);
         }

      weight += (growth/4 < boostFactor) ? (boostFactor - growth/5) : 0;
      }
   else // of if piv
      {
      dumpOptDetails(comp(), "\tthe loop is not a counted loop\n");

      if (_haveProfilingInfo)
         {
         // We must have enough profile information to be confident enough to unroll
         // this loop.  Current simple heuristic:
         //
         //int32_t maxCount = comp()->getFlowGraph()->getMaxFrequency();
         //if (maxCount >= 100 && entryBlockFrequency * 6 > maxCount)

         if (canUnrollUnCountedLoop(loop, numBlocks, numNodes, entryBlockFrequency))
            {
            int32_t hotnessFactor = 1;
            if ((comp()->getMethodHotness() == scorching) && (entryBlockFrequency >= HIGH_FREQ_LOOP_CUTOFF))
               hotnessFactor = 4;

            for (int32_t j = 3; j >= 1; --j)
               if ((numNodes-numSubscripts*0.5) * (j+1) <= hotnessFactor*maxSizeThresholdForNonCountedLoops)
                  {
                  unrollCount = j; break;
                  }
            spillLoop = 0;
            unrollKind = TR_LoopUnroller::GeneralUnroll;
            }
         }
      else if (debug("eagerGLU"))
         {
         // stress test
         unrollCount = 2;
         spillLoop = 0;
         unrollKind = TR_LoopUnroller::GeneralUnroll;
         printf("--    -- in %s\n", comp()->signature());
         }
      }

   // Unrolling is going to amortize the cost of the async check (if the loop has one)
   //
   if (!loop->getEntryBlock()->getStructureOf()->isEntryOfShortRunningLoop())
      weight += (unrollCount * 100) / numNodes;

   // Presence of any sequential memory access patterns inside the loop gives a boost
   // to the loop as well, since they could overlap.
   // TODO: Need to tweak this to only detect such pattern
   weight += 150 * numSubscripts;

   // The hottest loops in scorching should be favoured regardless of what they contain
   //
   if ((comp()->getMethodHotness() == scorching) && (loop->getEntryBlock()->getFrequency() >= HIGH_FREQ_LOOP_CUTOFF))
      weight += 300;
   else
      {
      // By unrolling, we increase the number of branches inside the loop which is going
      // to limit local optimizations and have an impact on the B2B.
      //
      weight -= unrollCount * 20 * (numBranches - 1);
      }

   //if (loop->getEntryBlock()->isRare())
   //   weight /= 3;

   cost = numNodes * (unrollCount + spillLoop + peelCount);

   // special handling for different methods
   TR::RecognizedMethod methodId = comp()->getMethodSymbol()->getRecognizedMethod();
   if (cg()->getSizeOfCombinedBuffer())
      {
      bool forceUnrollCount = false;
      switch (methodId)
         {
#ifdef J9_PROJECT_SPECIFIC
         case TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop:
            forceUnrollCount = true;
            break;
         case TR::sun_nio_cs_ext_IBM1388_Encoder_encodeArrayLoop:
            forceUnrollCount = loop->getEntryBlock()->getFrequency() >= HIGH_FREQ_LOOP_CUTOFF;
            break;
#endif
         default:
            // leave unrollCount unmodified
            break;
         }
      if (forceUnrollCount)
         unrollCount = 7;
      }

   dumpOptDetails(comp(), "\tweight = %d, cost = %d, unrollCount = %d\n", weight, cost, unrollCount);
   return weight * entryBlockFrequency/16;
   }


// Heuristics For Unrolling uncounted loops
bool
TR_GeneralLoopUnroller::canUnrollUnCountedLoop(TR_RegionStructure *loop,
                                               int32_t numBlocks, int32_t numNodes,
                                               int32_t entryBlockFrequency)
   {
   if (comp()->getOption(TR_DisableUncountedUnrolls))
      return false;

   if (entryBlockFrequency * 6 > (MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT))
      return true;

   // Detect Small loops like:
   //   [0x37047e24] BBStart (block 133) (frequency 127) (is in loop 133)
   //   [0x37047f58]   astore #392[0x36e8c8e4]
   //   [0x37047ef8]     iaload #549[0x37047c9c]+24
   //   [0x37047ed0]       aload #392[0x36e8c8e4]  Auto[<temp slot 8>]   <flags:"0x4" (X!=0 )/>
   //   [0x375191d4]   NULLCHK on [0x37047ef8] #18[0x36a24b1c]  Method[jitThrowNullPointerException]
   //   [0x375191fc]     iaload #549[0x37047c9c]+24
   //                      ==>iaload at [0x37047ef8]
   //   [0x3751925c]   ifacmpne --> block 133 BBStart at [0x37047e24]
   //                    ==>iaload at [0x375191fc]
   //   [0x37519288]     aconst NULL   <flags:"0x2" (X==0 )/>
   //   [0x37519144]   BBEnd (block 133)

   if ((numBlocks <= 1 && numNodes <= 13 &&
          ((entryBlockFrequency * 200) > (MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT))) ||
       (numBlocks <= 2 && numNodes <= 25 &&
          ((entryBlockFrequency * 100) > (MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT))))
      {

      TR_ScratchList<TR::Block> blocksInLoop(trMemory());
      loop->getBlocks(&blocksInLoop);
      ListIterator<TR::Block> bilIt(&blocksInLoop);
      TR::Block *b = NULL;
      for (b = bilIt.getCurrent(); b; b = bilIt.getNext())
         {
         TR::TreeTop *exit = b->getExit();
         TR::TreeTop *entry = b->getFirstRealTreeTop();
         for (TR::TreeTop *tt = entry; tt != exit; tt = tt->getNextRealTreeTop())
            {
            TR::Node *node = tt->getNode();
            TR::Node *firstChild = NULL;
            if (node->getOpCodeValue() == TR::NULLCHK &&
                  (firstChild = node->getFirstChild()) && firstChild->getOpCode().isLoad())
               {
               // Found a NULLCHK node, lets search to see if we have a compare with NIL
               // for the NULLCHK's first child
               for (TR::TreeTop *tt1 = tt; tt1 != exit; tt1 = tt1->getNextRealTreeTop())
                  {
                  TR::Node *n = tt1->getNode();
                  if (n->getOpCode().isBooleanCompare() &&
                         (n->getFirstChild() == firstChild)  &&
                         (n->getSecondChild()->getOpCodeValue() == TR::aconst) &&
                          (n->getSecondChild()->getAddress() == 0))
                     {
                     if (trace())
                        {
                        traceMsg(comp(), "\tLoop %d can be unrolled because of common NULLCHK and compare to NIL\n",loop->getNumber());
                        }

                     return true;
                     }
                  }
               }
            }
         }
      }


   return false;
   }
// Returns true if the controlling branch involves the induction variable
// If the loop exit branch does not involve the induction variable, then it
// can be possibly ignored
//
bool TR_GeneralLoopUnroller::branchContainsInductionVariable(TR_RegionStructure *loop,
                                                             TR::Node *branchNode,
                                                             bool checkOnlyPiv)
   {
   ///ListIterator<TR_BasicInductionVariable> it(&loop->getBasicInductionVariables());
   bool result = false;
   TR_BasicInductionVariable * iv = loop->getPrimaryInductionVariable();

   ///for (iv = it.getFirst(); iv; iv = it.getNext())
      {
      int32_t index = iv->getSymRef()->getReferenceNumber();
      ///traceMsg(comp(), "\tloop %d basicivs: %d\n", loop->getNumber(), index);
      if (branchContainsInductionVariable(branchNode, iv->getSymRef()))
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
            if (trace())
               traceMsg(comp(), "\tbut branch expr [%p] is not in recognized form\n", firstChild);
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

bool TR_GeneralLoopUnroller::branchContainsInductionVariable(TR::Node *node,
                                                             TR::SymbolReference *ivSymRef)
   {
   if (node->getOpCode().hasSymbolReference() &&
         (node->getSymbolReference() == ivSymRef))
      return true;
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      if (branchContainsInductionVariable(node->getChild(i), ivSymRef))
         return true;
   return false;
   }

const char *
TR_GeneralLoopUnroller::optDetailString() const throw()
   {
   return "O^O GENERAL LOOP UNROLLER: ";
   }

#undef GET_PREV_CLONE_BLOCK
#undef GET_PREV_CLONE_NODE
#undef CURRENT_MAPPER
#undef GET_CLONE_BLOCK
#undef SET_CLONE_BLOCK
#undef GET_CLONE_NODE
#undef SET_CLONE_NODE
