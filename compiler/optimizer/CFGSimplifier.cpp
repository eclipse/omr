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

#include "optimizer/CFGSimplifier.hpp"

#include <algorithm>
#include <stddef.h>
#include "compile/Compilation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"

// Set to 0 to disable the special-case pattern matching using the
// s390 condition code.
#define ALLOW_SIMPLIFY_COND_CODE_BOOLEAN_STORE 1

#define OPT_DETAILS "O^O CFG SIMPLIFICATION: "

TR_CFGSimplifier::TR_CFGSimplifier(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}


int32_t TR_CFGSimplifier::perform()
   {
   if (trace())
      traceMsg(comp(), "Starting CFG Simplification\n");

   bool anySuccess = false;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());


   _cfg = comp()->getFlowGraph();
   if (_cfg != NULL)
      {
      for (TR::CFGNode *cfgNode = _cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
         {
         _block = toBlock(cfgNode);
         anySuccess |= simplify();
         }
      }

   // Any transformations invalidate use/def and value number information
   //
   if (anySuccess)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      }

   } // stackMemoryRegion scope

   if (trace())
      {
      traceMsg(comp(), "\nEnding CFG Simplification\n");
      comp()->dumpMethodTrees("\nTrees after CFG Simplification\n");
      }

   return 1; // actual cost
   }

bool TR_CFGSimplifier::simplify()
   {
   // Can't simplify the entry or exit blocks
   //
   if (_block->getEntry() == NULL)
      return false;

   // Find the first two successors
   //
   _succ1 = _succ2 = NULL;
   _next2 = NULL;
   if (_block->getSuccessors().empty())
      {
      _next1 = NULL;
      }
   else
      {
      _succ1 = _block->getSuccessors().front();
      _next1 = toBlock(_succ1->getTo());
      if (_block->getSuccessors().size() > 1)
         {
         _succ2 = *(++(_block->getSuccessors().begin()));
         _next2 = toBlock(_succ2->getTo());
         }
      }
   return simplifyBooleanStore();
   }

// Look for pattern of the form:
//
//    if (cond)
//       x = 0;
//    else
//       x = y;
//
// This can be simplified to remove the control flow if the condition can
// be represented by a "cmp" opcode.
//
// Also look specifically for the following pattern using the S390 condition code:
//
//    if (conditionCode)
//       x = 0;
//    else
//       x = y;
//    if (some cond involving x) goto someLabel
//
// Return "true" if any transformations were made.
//
bool TR_CFGSimplifier::simplifyBooleanStore()
   {
   // There must be exactly two successors, and they must be real blocks
   //
   if (_next1 == NULL || _next2 == NULL)
      return false;
   if (_succ2 == NULL)
      return false;
   if (_block->getSuccessors().size() > 2)
      return false;
   if (_next1->getEntry() == NULL || _next2->getEntry() == NULL)
      return false;

   // The successors must have only this block as their predecessor, and must
   // have a common successor.
   //
   bool needToDuplicateTree = false;
   if (_next1->getPredecessors().empty())
      return false;
   if (!(_next1->getPredecessors().front()->getFrom() == _block && (_next1->getPredecessors().size() == 1)))
      needToDuplicateTree = true;
   if (_next2->getPredecessors().empty())
      return false;
   if (!(_next2->getPredecessors().front()->getFrom() == _block && (_next2->getPredecessors().size() == 1)))
      needToDuplicateTree = true;

   if (_next1->getSuccessors().empty())
      return false;
   if (_next1->getSuccessors().size() != 1)
      return false;
   if (_next2->getSuccessors().empty())
      return false;
   if (_next2->getSuccessors().size() != 1)
      return false;
   if (_next1->getSuccessors().front()->getTo() != _next2->getSuccessors().front()->getTo())
      return false;

   TR::Block *joinBlock = toBlock(_next1->getSuccessors().front()->getTo());

   // This block must end in a compare-and-branch which can be converted to a
   // boolean compare, or a branch using the condition code.
   //
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   bool isBranchOnCondCode = false;
   if (compareNode->isNopableInlineGuard())
      //don't simplify nopable guards
      return false;
   if (compareNode->getOpCode().convertIfCmpToCmp() == TR::BadILOp)
      return false;

   // ... and so one of the successors must be the fall-through successor. Make
   // _next1 be the fall-through successor.
   //
   TR::Block *b = getFallThroughBlock(_block);
   if (b != _next1)
      {
      TR_ASSERT(b == _next2, "CFG Simplifier");
      _next2 = _next1;
      _next1 = b;
      }

   // The trees of each successor block must consist of a single store.
   //
   TR::TreeTop *store1TreeTop = getNextRealTreetop(_next1->getEntry());
   if (store1TreeTop == NULL || getNextRealTreetop(store1TreeTop) != NULL)
      return false;
   TR::Node *store1 = store1TreeTop->getNode();
   if (!store1->getOpCode().isStore())
      return false;
   TR::TreeTop *store2TreeTop = getNextRealTreetop(_next2->getEntry());
   if (store2TreeTop == NULL || getNextRealTreetop(store2TreeTop) != NULL)
      return false;
   TR::Node *store2 = store2TreeTop->getNode();
   if (!store2->getOpCode().isStore())
      return false;

   // The stores must be integer stores to the same variable
   //
   if (store1->getOpCodeValue() != store2->getOpCodeValue())
      return false;
   if (!store1->getOpCode().isInt() && !store1->getOpCode().isByte())
      return false;
   if (store1->getSymbolReference()->getSymbol() != store2->getSymbolReference()->getSymbol())
      return false;

   // Indirect stores must have the same base
   //
   int32_t valueIndex;
   if (store1->getOpCode().isIndirect())
      {
      valueIndex = 1;
      //    - TODO
      ///return false;
      }
   else
      {
      valueIndex = 0;
      }
   TR::Node *value1 = store1->getChild(valueIndex);
   TR::Node *value2 = store2->getChild(valueIndex);

   if (valueIndex == 1) // indirect store, check base objects
      {
      TR::Node *base1 = store1->getFirstChild();
      TR::Node *base2 = store2->getFirstChild();
      if (!base1->getOpCode().hasSymbolReference() || !base2->getOpCode().hasSymbolReference())
         return false;
      if (base1->getSymbolReference()->getReferenceNumber() != base2->getSymbolReference()->getReferenceNumber())
         return false;
      }

   // The value on one of the stores must be zero. There is a special case if
   // one of the values is 0 and the other is 1.
   //
   bool booleanValue = false;
   bool swapCompare  = false;
   if (value1->getOpCode().isLoadConst())
      {
      int32_t int1 = value1->getInt();
      if (value2->getOpCode().isLoadConst())
         {
         int32_t int2 = value2->getInt();
         if (int1 == 1)
            {
            if (int2 == 0)
               {
               booleanValue = true;
               swapCompare = true;
               }
            else
               return false;
            }
         else if (int1 == 0)
            {
            if (int2 == 1)
               booleanValue = true;
            else
               swapCompare = true;
            }
         else if (int2 != 0)
            return false;
         }
      // Is this code really valid when the trees guarded by the if rely on the condition checked in the 'if' (e.g. we could have an indirect access without any checking in guarded code, because the test checked if the value was NULL, in which case by performing the simplification we could end up with a crash when the object is dereferenced)
      //else if (int1 == 0)
      //   swapCompare = true;
      else
         {
         return false;
         }
      }
    // Is this code really valid when the trees guarded by the if rely on the condition checked in the 'if' (e.g. we could have an indirect access without any checking in guarded code, because the test checked if the value was NULL, in which case by performing the simplification we could end up with a crash when the object is dereferenced)
   //else if (!(value2->getOpCode().isLoadConst() && value2->getInt() == 0))
   //   return false;
   else
      return false;

#if ALLOW_SIMPLIFY_COND_CODE_BOOLEAN_STORE
   if (isBranchOnCondCode)
      return simplifyCondCodeBooleanStore(joinBlock, compareNode, store1, store2);
#else
   if (isBranchOnCondCode)
      return false;
#endif


   // The stores can be simplified
   //
   // The steps are:
   //    1) Add an edge from the first block to the join block
   //    2) Set up the istore to replace the compare, e.g.
   //          replace
   //             ificmpeq
   //          with
   //             istore
   //                cmpeq
   //    3) Remove the 2 blocks containing the stores
   //    4) Insert a goto from the first block to the join block if necessary
   //

   // TODO - support going to non-fallthrough join block
   //

   if (!performTransformation(comp(), "%sReplace compare-and-branch node [%p] with boolean compare\n", OPT_DETAILS, compareNode))
      return false;

   _cfg->addEdge(TR::CFGEdge::createEdge(_block, joinBlock, trMemory()));


   // Re-use the store with the non-trivial value - for boolean store it doesn't
   // matter which store we re-use.
   //

   needToDuplicateTree = true; // to avoid setting store node to NULL

   TR::TreeTop *storeTreeTop;
   TR::Node *storeNode;
   if (swapCompare)
      {
      if (needToDuplicateTree)
         {
         storeTreeTop = NULL;
         storeNode = store2->duplicateTree();
         }
      else
         {
         storeTreeTop = store2TreeTop;
         storeNode = store2;
         }
      // Set up the new opcode for the compare node
      //
      TR::Node::recreate(compareNode, compareNode->getOpCode().getOpCodeForReverseBranch());
      }
   else
      {
      if (needToDuplicateTree)
         {
         storeTreeTop = NULL;
         storeNode = store1->duplicateTree();
         }
      else
         {
         storeTreeTop = store1TreeTop;
         storeNode = store1;
         }
      }
   TR::Node *value = storeNode->getChild(valueIndex);

   TR::Node::recreate(compareNode, compareNode->getOpCode().convertIfCmpToCmp());

   TR::Node *node1;
   TR::ILOpCodes convertOpCode = TR::BadILOp;
   if (booleanValue)
      {
      // If the result is a boolean value (i.e. either a 0 or 1 is being stored
      // as a result of the compare), the trees are changed from:
      //   ificmpxx --> L1
      //     ...
      //   ...
      //   istore
      //     x
      //     iconst 0
      //   ...
      //   goto L2
      //   L1:
      //   istore
      //     x
      //     iconst 1
      //   L2:
      //
      // to:
      //   istore
      //     x
      //     possible i2x conversion
      //       icmpxx
      //         ...

      // Separate the original value on the store from the store node
      //
      value->recursivelyDecReferenceCount();

      // The compare node will create a byte value. This must be converted to the
      // type expected on the store.
      //
      int32_t size = storeNode->getOpCode().getSize();
      if (size == 4)
         {
         storeNode->setChild(valueIndex, compareNode);
         compareNode->incReferenceCount();
         }
      else
         {
         if (size == 1)
            convertOpCode = TR::i2b;
         else if (size == 2)
            convertOpCode = TR::i2s;
         else if (size == 8)
            convertOpCode = TR::i2l;
         node1 = TR::Node::create(convertOpCode, 1, compareNode);
         storeNode->setChild(valueIndex, node1);
         node1->incReferenceCount();
         }
      compareTreeTop->setNode(storeNode);
      }
   else
      {
      // If the result is not a boolean value the trees are changed from:
      //   ificmpxx --> L1
      //     ...
      //   ...
      //   istore
      //     x
      //     y
      //   ...
      //   goto L2
      //   L1:
      //   istore
      //     x
      //     iconst 0
      //   L2:
      //
      // to:
      //   istore
      //     x
      //     iand
      //       y
      //       isub
      //         b2i
      //           icmpxx
      //             ...
      //         iconst 1

      // The compare node will create a byte value. This must be converted to the
      // type expected on the store.
      //
      TR::ILOpCodes andOpCode, subOpCode;
      TR::Node *constNode;
      int32_t size = storeNode->getOpCode().getSize();
      if (size == 4)
         {
         andOpCode = TR::iand;
         subOpCode = TR::isub;
         constNode = TR::Node::create(value, TR::iconst);
         constNode->setInt(1);
         }
      else
         {
         if (size == 1)
            {
             andOpCode = TR::band;
            subOpCode = TR::bsub;
            convertOpCode = TR::i2b;
            constNode = TR::Node::create(value, TR::bconst);
            constNode->setByte(1);
            }
         else if (size == 2)
            {
            andOpCode = TR::sand;
            subOpCode = TR::ssub;
            convertOpCode = TR::i2s;
            constNode = TR::Node::create(value, TR::sconst);
            constNode->setShortInt(1);
            }
         else // (size == 8)
            {
            andOpCode = TR::land;
            subOpCode = TR::lsub;
            convertOpCode = TR::i2l;
            constNode = TR::Node::create(value, TR::lconst);
            constNode->setLongInt(1L);
            }
         compareNode = TR::Node::create(convertOpCode, 1, compareNode);
         }
      value->decReferenceCount();
      TR::Node *node2 = TR::Node::create(subOpCode, 2, compareNode, constNode);
      node1 = TR::Node::create(andOpCode, 2, value, node2);
      storeNode->setChild(valueIndex, node1);
      node1->incReferenceCount();
      compareTreeTop->setNode(storeNode);
      }

   int32_t succ1Freq = _succ1->getFrequency();
   int32_t succ2Freq = _succ2->getFrequency();

   if (succ1Freq > 0)
      {
      _next1->setFrequency(std::max(MAX_COLD_BLOCK_COUNT+1, (_next1->getFrequency() - succ1Freq)));
      if (!_next1->getSuccessors().empty())
         {
         TR::CFGEdge* successorEdge = _next1->getSuccessors().front();
         successorEdge->setFrequency(std::max(MAX_COLD_BLOCK_COUNT+1, (successorEdge->getFrequency() - succ1Freq)));
         }
      }

   if (succ2Freq > 0)
      {
      _next2->setFrequency(std::max(MAX_COLD_BLOCK_COUNT, (_next2->getFrequency() - succ2Freq)));
      if (!_next2->getSuccessors().empty())
         {
         TR::CFGEdge* successorEdge = _next2->getSuccessors().front();
         successorEdge->setFrequency(std::max(MAX_COLD_BLOCK_COUNT+1, (successorEdge->getFrequency() - succ2Freq)));
         }
      }

   joinBlock->setFrequency(_block->getFrequency());

   _cfg->removeEdge(_succ1);
   _cfg->removeEdge(_succ2);
   if (getFallThroughBlock(_block) != joinBlock)
      {
      // TODO
      //TR_ASSERT(false,"No fall-through to join block");
      _block->append(TR::TreeTop::create(comp(),
                                        TR::Node::create(compareNode, TR::Goto, 0, joinBlock->getEntry())));
      }
   return true;
   }

TR::TreeTop *TR_CFGSimplifier::getNextRealTreetop(TR::TreeTop *treeTop)
   {
   treeTop = treeTop->getNextRealTreeTop();
   while (treeTop != NULL)
      {
      TR::Node *node = treeTop->getNode();
      TR::ILOpCode &opCode = node->getOpCode();
      if (opCode.getOpCodeValue() == TR::BBEnd ||
          opCode.getOpCodeValue() == TR::Goto)
         return NULL;
      else
         return treeTop;
      }
   return treeTop;
   }

TR::TreeTop *TR_CFGSimplifier::getLastRealTreetop(TR::Block *block)
   {
   TR::TreeTop *treeTop = block->getLastRealTreeTop();
   if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
      return NULL;
   return treeTop;
   }

TR::Block *TR_CFGSimplifier::getFallThroughBlock(TR::Block *block)
   {
   TR::TreeTop *treeTop = block->getExit()->getNextTreeTop();
   if (treeTop == NULL)
      return NULL;
   TR_ASSERT(treeTop->getNode()->getOpCodeValue() == TR::BBStart, "CFG Simplifier");
   return treeTop->getNode()->getBlock();
   }

/* So far, we've matched the normal pattern, except that instead of an ifcmpxx, we have a
   branch that uses the condCode. To fully match the pattern, we need the following:

   1) The final block must start with a compare-and-branch
   2) One of the things being compared must be the same variable that was stored to previously
    (eg. we conditionally store 0 or another value to x; we need to be comparing x to something here)
 */
bool TR_CFGSimplifier::simplifyCondCodeBooleanStore(TR::Block *joinBlock, TR::Node *branchNode, TR::Node *store1Node, TR::Node *store2Node)
   {
   // The first node in the final block must be a compare-and-branch
   TR::TreeTop *compareTreeTop = getNextRealTreetop(joinBlock->getEntry());
   if (compareTreeTop == NULL)
      return false;
   TR::Node *compareNode = compareTreeTop->getNode();
   if (compareNode == NULL || compareNode->getOpCode().convertIfCmpToCmp() == TR::BadILOp)
      return false;
   else if (compareNode == NULL)
      return false;

   int valueIndex = 0;
   if (store1Node->getOpCode().isIndirect())
      valueIndex = 1;
   TR::Node *value1Node = store1Node->getChild(valueIndex);
   TR::Node *value2Node = store2Node->getChild(valueIndex);

   // The compare-and-branch needs to compare a constant with a value
   TR::Node *loadNode = NULL, *constNode = NULL, *child1 = compareNode->getFirstChild(), *child2 = compareNode->getSecondChild();
   if (NULL != child1 && child1->getOpCode().isInteger())
      {
      // Child can be an AND
      if (child1->getOpCode().isAnd())
         {
         // First child must be a load; second child must be a const equal to the non-zero const that we stored
         TR::Node *c1 = child1->getFirstChild(), *c2 = child1->getSecondChild();
         if (NULL != c1 && c1->getOpCode().isLoad())
            loadNode = c1;
         if (NULL != c2 && c2->getOpCode().isLoadConst())
            {
            int32_t constVal = c2->get32bitIntegralValue();
            if (constVal == 0 || (constVal != value1Node->get32bitIntegralValue() && constVal != value2Node->get32bitIntegralValue()))
               loadNode = NULL;
            }
         }
      // Child can be a conversion
      else if (child1->getOpCode().isConversion() && child1->getFirstChild() != NULL)
         {
         TR::Node *c1 = child1->getFirstChild();
         if (NULL != c1 && c1->getOpCode().isLoad())
            loadNode = c1;
         }
      else if (child1->getOpCode().isLoad())
         loadNode = child1;
      }

   if (NULL != child2 && child2->getOpCode().isLoadConst())
      constNode = child2;

   if (NULL == loadNode || NULL == constNode)
      return false;

   // The load node must load from the variable that was previously stored to
   if (store1Node->getSymbolReference()->getSymbol() != loadNode->getSymbolReference()->getSymbol())
      return false;

   // Indirect loads and stores must have the same base
   if (store1Node->getOpCode().isIndirect() != loadNode->getOpCode().isIndirect())
      return false;
   if (store1Node->getOpCode().isIndirect())
      {
      TR::Node *base1 = store1Node->getFirstChild();
      TR::Node *base2 = loadNode->getFirstChild();
      if (!base1->getOpCode().hasSymbolReference() || !base2->getOpCode().hasSymbolReference())
         return false;
      if (base1->getSymbolReference()->getReferenceNumber() != base2->getSymbolReference()->getReferenceNumber())
         return false;
      }

   // The constant in the compare must match one of the constants being stored
   int32_t value1 = value1Node->get32bitIntegralValue(), value2 = value2Node->get32bitIntegralValue(), compareConst = constNode->get32bitIntegralValue();
   if (compareConst != value1 && compareConst != value2)
      return false;


   /* We've matched the pattern:
      if (condCode matches its mask)
         x = a;
      else
         x = b;
      if (x op const) jump to label L1
      label L2 (fallthrough)

      We can rewrite the code as follows:

      if (x == a) or (x <> b):

      if (condCode matches its mask) jump to label L1
      label L2 (fallthrough)

      if (x <> a), if (x == b):

      if (condCode matches its mask) jump to label L2
      label L1 (fallthrough)

      Currently unhandled: cases with a <, >, <= or >= comparison

    */

   // The simpler case is when the comparison check matches the taken branch (eg. x = 0; if (x == 0) then ...)
   bool needToSwap = false;
   if (compareNode->getOpCode().isCompareForEquality())
      {
      if (compareNode->getOpCode().isCompareTrueIfEqual() && compareConst == value1) // if (cond), x == a; if (x == b) jump
         needToSwap = true;
      else if (!compareNode->getOpCode().isCompareTrueIfEqual() && compareConst == value2) // if (cond), x == a; f (x != a), jump
         needToSwap = true;
      }
   else
      // TODO: Handle cases where there's a test for order; sometimes normal, sometimes swap, sometimes can guarantee that only one path will be taken
      {
      traceMsg(comp(), "CFGSimplifier condCode pattern matches but uses test for ordering, not equality\n");
      return false;
      }

   // Everything matches
   if (!performTransformation(comp(), "%sReplace (branch on condition code [%p] -> boolean stores -> branch-and-compare using stored boolean) with single branch on condition code\n", OPT_DETAILS, branchNode))
      return false;

   // Find the fallthrough and taken blocks
   TR::Block *fallthroughBlock = getFallThroughBlock(joinBlock), *takenBlock = NULL;
   TR::CFGEdgeList& joinBlockSuccessors = joinBlock->getSuccessors();

   TR_ASSERT(joinBlockSuccessors.size() == 2, "Block containing only an if tree doesn't have exactly two successors\n");
   TR::CFGEdge* oldTakenEdge = NULL;
   auto succ = joinBlockSuccessors.begin();
   while (succ != joinBlockSuccessors.end())
      {
      TR::Block *block = toBlock((*succ)->getTo());
      if (block != fallthroughBlock)
         {
         oldTakenEdge = *succ;
         takenBlock = block;
         break;
         }
      ++succ;
      };

   TR_ASSERT(fallthroughBlock != NULL && takenBlock != NULL && fallthroughBlock != takenBlock, "Expected unique, non-null taken and fallthrough blocks\n");

   /* At this point, we have the following structure:
      Block A: ends with the branch
                  /            \
      Block B: store 1     Block C: store 2
          (taken)            (fallthrough)
                 \             /
                Block D: if (stmt)
                 /             \
      Block E: taken       Block F: fallthrough

      The revised structure will be:
      Block A: ends with the instruction preceding the branch
                        |
      Block D: branch replaces the if
                 /             \
      Block E: taken       Block F: fallthrough

      If needToSwap is true, the code looks like the following:
      if (cond code matches its mask)
         x = 0
      else
         x = 1
      if (x == 1)
         do something
      else
         do some other thing

      We can't use the structure changes described above and maintain correctness; we need to either
      reverse the cond code mask, if that's allowed, or swap the taken and fallthrough blocks so that
      we branch to the old fallthrough block and fall through to the old taken block.

      If it's safe to invert the mask bits, we can do so and behave as normal. Otherwise, we can
      reverse the blocks, but we also need to add a block containing only a goto node to ensure
      that block A falls through to block F without having to reorder the list of blocks.
      The resulting structure will be the following:
      Block A: ends with the instruction preceding the branch
                        |
      Block D: branch replaces the if
                 /             \
         Block G: goto          \
               /                 \
      Block E: fallthrough   Block F: taken
    */

   if (needToSwap && !canReverseBranchMask())
      {
      TR::Block *temp = takenBlock;
      takenBlock = fallthroughBlock;
      fallthroughBlock = temp;

      // Pull the branch node out of the trees
      TR::TreeTop *branchTreeTop = getLastRealTreetop(_block);
      branchTreeTop->getPrevTreeTop()->join(branchTreeTop->getNextTreeTop());
      compareTreeTop->getPrevTreeTop()->join(compareTreeTop->getNextTreeTop());

      // Insert the branch node into the tree in place of the compare node
      compareTreeTop->getPrevTreeTop()->join(branchTreeTop);
      branchTreeTop->join(compareTreeTop->getNextTreeTop());

      // Redirect the branch to the new taken block (from D to F)
      branchNode->setBranchDestination(takenBlock->getEntry());

      // Add a (always fallthrough) edge from block A to D
      _cfg->addEdge(TR::CFGEdge::createEdge(_block, joinBlock, trMemory()));
      joinBlock->setIsExtensionOfPreviousBlock(true);

      // Create an empty block, G
      TR::Node *lastNode = joinBlock->getLastRealTreeTop()->getNode();
      TR::Block *newGotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), fallthroughBlock->getFrequency(), NULL);

      // Create a goto node in the new block, and have it branch to the new fallthrough block
      TR::TreeTop *gotoBlockEntryTree = newGotoBlock->getEntry();
      TR::TreeTop *gotoBlockExitTree = newGotoBlock->getExit();
      TR::TreeTop *joinExit = joinBlock->getExit();
      TR::Node *gotoNode =  TR::Node::create(lastNode, TR::Goto);
      TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
      gotoNode->setBranchDestination(fallthroughBlock->getEntry());
      gotoBlockEntryTree->join(gotoTree);
      gotoTree->join(gotoBlockExitTree);
      joinExit->join(gotoBlockEntryTree);
      gotoBlockExitTree->join(takenBlock->getEntry());

      // Add the new block to the CFG and update edges
      _cfg->addNode(newGotoBlock, fallthroughBlock->getParentStructureIfExists(_cfg));
      _cfg->addEdge(TR::CFGEdge::createEdge(joinBlock,  newGotoBlock, trMemory())); // Add edge from D to G
      _cfg->addEdge(TR::CFGEdge::createEdge(newGotoBlock,  fallthroughBlock, trMemory())); // Add edge from G to E
      _cfg->removeEdge(oldTakenEdge); // Remove edge from D to E
      }
   else
      {
      // Either no need to swap, or we can swap by flipping the cond code bits

      // Pull the branch node out of the trees
      TR::TreeTop *branchTreeTop = getLastRealTreetop(_block);
      branchTreeTop->getPrevTreeTop()->join(branchTreeTop->getNextTreeTop());
      compareTreeTop->getPrevTreeTop()->join(compareTreeTop->getNextTreeTop());

      // Insert the branch node into the tree in place of the compare node
      compareTreeTop->getPrevTreeTop()->join(branchTreeTop);
      branchTreeTop->join(compareTreeTop->getNextTreeTop());

      // Redirect the branch so it mirrors the compare
      branchNode->setBranchDestination(compareNode->getBranchDestination());

      // Add a (always fallthrough) edge from block A to D
      _cfg->addEdge(TR::CFGEdge::createEdge(_block, joinBlock, trMemory()));
      joinBlock->setIsExtensionOfPreviousBlock(true);

      // Invert the mask bits
      }

   // Remove edges for the blocks that are being removed
   _cfg->removeEdge(_succ1); // Remove edge from A to B
   _cfg->removeEdge(_succ2); // Remove edge from A to C

   return true;
   }

// Returns true if it's safe to reverse the branch mask
bool TR_CFGSimplifier::canReverseBranchMask()
   {
   return false;
   }

const char *
TR_CFGSimplifier::optDetailString() const throw()
   {
   return "O^O CFG SIMPLIFICATION: ";
   }
