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

#include "optimizer/CFGSimplifier.hpp"

#include <algorithm>
#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
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
#include "optimizer/TransformUtil.hpp"
#include "ras/DebugCounter.hpp"
#include "infra/Checklist.hpp"

// Set to 0 to disable the special-case pattern matching using the
// s390 condition code.
#define ALLOW_SIMPLIFY_COND_CODE_BOOLEAN_STORE 1

#define OPT_DETAILS "O^O CFG SIMPLIFICATION: "

TR::Optimization* OMR::CFGSimplifier::create(TR::OptimizationManager *manager)
    {
    return new (manager->allocator()) TR::CFGSimplifier(manager);
    }

OMR::CFGSimplifier::CFGSimplifier(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}


int32_t OMR::CFGSimplifier::perform()
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

bool OMR::CFGSimplifier::simplify()
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

   return simplifyIfStructure();
   }

bool OMR::CFGSimplifier::simplifyIfStructure()
   {
   if (trace())
      traceMsg(comp(), "Attempting if simpliciaton on block_%d\n", _block->getNumber());
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

   // This block must end in a compare-and-branch which can be converted to a
   // boolean compare, or a branch using the condition code.
   //
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   if (!compareNode->getOpCode().isIf())
      return false;
   if (compareNode->isNopableInlineGuard())
      //don't simplify nopable guards
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
   return simplifyIfPatterns(needToDuplicateTree);
   }

bool OMR::CFGSimplifier::simplifyIfPatterns(bool needToDuplicateTree)
   {
   return simplifyBooleanStore(needToDuplicateTree)
          || simplifyNullToException(needToDuplicateTree)
          || simplifySimpleStore(needToDuplicateTree)
          || simplifyCondStoreSequence(needToDuplicateTree)
          || simplifyInstanceOfTestToCheckcast(needToDuplicateTree)
          || simplifyBoundCheckWithThrowException(needToDuplicateTree)
          ;
   }

bool  OMR::CFGSimplifier::hasExceptionPoint(TR::Block *block, TR::TreeTop *end)
   {
   if (!block->getExceptionSuccessors().empty())
      return true;

   for (TR::TreeTop *itr = block->getEntry(); itr && itr != end; itr = itr->getNextTreeTop())
       {
       if (itr->getNode()->exceptionsRaised() != 0)
          return true;
       }
   return false;
   }

bool OMR::CFGSimplifier::simplifyInstanceOfTestToCheckcast(bool needToDuplicateTree)
   {
   static char *disableSimplifyInstanceOfTestToCheckcast = feGetEnv("TR_disableSimplifyInstanceOfTestToCheckcast");
   if (disableSimplifyInstanceOfTestToCheckcast != NULL)
      return false;

   if (comp()->getOSRMode() == TR::involuntaryOSR)
      return false;

   if (_block->isCatchBlock())
      return false;

   if (trace())
      traceMsg(comp(), "Start simplifyInstanceOfTestToCheckcast block_%d\n", _block->getNumber());

   // This block must end in an ifacmpeq or ifacmpne against aconst NULL
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   if (compareNode->getOpCodeValue() != TR::ificmpeq
       && compareNode->getOpCodeValue() != TR::ificmpne)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ificmp[eq/ne] n%dn\n", compareNode->getGlobalIndex());

   if (compareNode->getSecondChild()->getOpCodeValue() != TR::iconst
       || compareNode->getSecondChild()->getInt() != 0)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ificmp[eq/ne] against zero n%dn\n", compareNode->getGlobalIndex());

   if (compareNode->getFirstChild()->getOpCodeValue() != TR::instanceof)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ificmp[eq/ne] of an instanceof against 0 n%dn\n", compareNode->getGlobalIndex());

   if (compareNode->getFirstChild()->getSecondChild()->getOpCodeValue() != TR::loadaddr)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ificmp[eq/new] of an instanceof a constant class against zero n%dn\n", compareNode->getGlobalIndex());

   TR::Block *throwBlock = NULL, *fallthroughBlock = NULL;
   if (compareNode->getOpCodeValue() == TR::ificmpeq)
      {
      if (_next2->getLastRealTreeTop()->getNode()->getNumChildren() != 1
          || _next2->getLastRealTreeTop()->getNode()->getFirstChild()->getOpCodeValue() != TR::athrow)
         return false;

      if (trace())
         traceMsg(comp(), "   Found an ificmpeq of an instanceof against zero which throws on taken size\n");
      throwBlock = _next2;
      fallthroughBlock = _next1;
   } else {
      if (_next1->getLastRealTreeTop()->getNode()->getNumChildren() != 1
          || _next1->getLastRealTreeTop()->getNode()->getFirstChild()->getOpCodeValue() != TR::athrow)
         return false;

      if (trace())
         traceMsg(comp(), "   Found an ificmpne of an instance of against zero which throws on the fallthrough path\n");
      throwBlock = _next1;
      fallthroughBlock = _next2;
   }

   if (!performTransformation(comp(), "%sReplace ificmp of instanceof with throw failure with checkcastAndNULLCHK in block_%d\n", OPT_DETAILS, _block->getNumber()))
      return false;

   _cfg->invalidateStructure();

   TR::Node *objNode = compareNode->getFirstChild()->getFirstChild();
   TR::Node *classNode = compareNode->getFirstChild()->getSecondChild();

   TR::Block *catchBlock = TR::Block::createEmptyBlock(compareNode, comp(), throwBlock->getFrequency());
   catchBlock->setHandlerInfo(0, static_cast<uint8_t>(comp()->getInlineDepth()), 0, comp()->getCurrentMethod(), comp());
   TR::Node *gotoNode = TR::Node::create(compareNode, TR::Goto, 0);
   gotoNode->setBranchDestination(throwBlock->getEntry());
   catchBlock->append(TR::TreeTop::create(comp(), gotoNode));

   TR::TreeTop *lastTree = comp()->findLastTree();
   catchBlock->getExit()->join(lastTree->getNextTreeTop());
   lastTree->join(catchBlock->getEntry());

   TR::Node *checkcastAndNULLCHKNode = TR::Node::createWithSymRef(compareNode->getFirstChild(), TR::checkcastAndNULLCHK, 2, comp()->getSymRefTab()->findOrCreateCheckCastSymbolRef(comp()->getMethodSymbol()));
   TR_Pair<TR_ByteCodeInfo, TR::Node> *bcInfo = new (trHeapMemory()) TR_Pair<TR_ByteCodeInfo, TR::Node> (&compareNode->getFirstChild()->getByteCodeInfo(), checkcastAndNULLCHKNode);
   comp()->getCheckcastNullChkInfo().push_front(bcInfo);
   checkcastAndNULLCHKNode->setAndIncChild(0, objNode);
   checkcastAndNULLCHKNode->setAndIncChild(1, classNode);

   if (trace())
      traceMsg(comp(), "Remove compareTreeTop n%dn\n", compareTreeTop->getNode()->getGlobalIndex());
   TR::TransformUtil::removeTree(comp(), compareTreeTop);

   TR::TreeTop *checkcastAndNULLCHKTree = TR::TreeTop::create(comp(), checkcastAndNULLCHKNode);

   if (trace())
      traceMsg(comp(), "Create checkcastAndNULLCHK Node n%dn\n", checkcastAndNULLCHKNode->getGlobalIndex());

   _block->append(checkcastAndNULLCHKTree);

   if (hasExceptionPoint(_block, checkcastAndNULLCHKTree))
      _block = _block->split(checkcastAndNULLCHKTree, _cfg, true, false);

   _cfg->addNode(catchBlock);
   _cfg->addExceptionEdge(_block, catchBlock);
   _cfg->addEdge(catchBlock, throwBlock);
   _cfg->removeEdge(_block, throwBlock);

   if (_block->getNextBlock() != fallthroughBlock) {
      TR::Node *gotoNode = TR::Node::create(checkcastAndNULLCHKNode, TR::Goto, 0);
      gotoNode->setBranchDestination(fallthroughBlock->getEntry());
      _block->append(TR::TreeTop::create(comp(), gotoNode));
   }

   if (trace())
      traceMsg(comp(), "End simplifyInstanceOfTestToCheckcast.\n");

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "cfgSimpCheckcast/(%s)", comp()->signature()));

   return true;
   }

// Looks for one of the patterns of the form:
//   Pattern-1:
//    ificmplt (i, 0) --------
//        | (FallThrough)     |
//    ificmplt (i, length) ======    
//        |                   |  |
//      throw  <--------------   |
//                               |
//      return <=================
//
//   Pattern-2:
//    ificmplt (i, 0) -----------
//        | (FallThrough)        |
//    ificmpge (i, length) ------|   
//        |                      |
//      return                   |
//                               |
//      throw <------------------
//
//   Pattern-3:
//    ificmpge (i, 0) -----------
//        | (FallThrough)        |
//  --> throw                    |
// |                             |
//  - ificmpge (i, length) <-----
//        |                      
//      return
//
// That can be generated from the typical bound check code,
//    if (i < 0 || i >= length)
//       throw Exception();
//    return i;
//
//  Or,
//     if (i>=0 && i < length)
//        return i;
//     throw Exception();
//
// And tries to simplify them by replacing the ifcmp[lt/ge] blocks with a BNDCHK such that it becomes
//
//
//  Simplification:
//    BNDCHK (i, length)  ----(exp edge) ------ 
//      |                                      |
//    return                                   |
//                                             |
//     goto  <---------------------------------
//      |
//    throw Exception() 
// Or,
//
//  Simplification:
//    BNDCHK (i, length)  ----(exp edge) ------
//      |                                      |
//    goto =================                   |
//                          |                  |
//    goto   <---------------------------------
//      |                   |
//    throw Exception()     |
//                          |  
//    return <==============
//
// Return "true" if any transformations were made.
//
bool OMR::CFGSimplifier::simplifyBoundCheckWithThrowException(bool needToDuplicateTree) 
   {
   static char *disableSimplifyBoundCheckWithThrowException = feGetEnv("TR_disableSimplifyBoundCheckWithThrowException");
   if (disableSimplifyBoundCheckWithThrowException != NULL)
      return false;
   if (trace())
      traceMsg(comp(), "Start simplifyBoundCheckWithThrowException\n");
   TR::TreeTop * treeTop = getLastRealTreetop(_block);
   if (!treeTop)
      return false;
   TR::Node *compareNode = treeTop->getNode();

   TR::ILOpCodes opCode = compareNode->getOpCodeValue();
   if (opCode != TR::ificmplt && opCode != TR::ificmpge)
      {
      if (trace())
         traceMsg(comp(), "   Abort simplifyBoundCheckWithThrowException : pattern not matched\n");
      return false;
      }

   bool isCompareLT = opCode == TR::ificmplt;
   bool patternMatched = false;

   TR::Block *throwBlock = NULL;
   TR::Block *nextCompareBlock = NULL;
   TR::Node *firstChildBndChk = NULL;
   TR::Node *secondChildBndChk = NULL;

   TR::Node *firstChild = compareNode->getFirstChild();
   TR::Node *secondChild = compareNode->getSecondChild();
   TR::SymbolReference *firstSymRef = NULL;

   if (firstChild->getOpCodeValue() == TR::iload)
      {
      firstSymRef = firstChild->getSymbolReference();
      if (secondChild->getOpCodeValue() == TR::iconst && secondChild->getConstValue() == 0)
         {
         patternMatched = true;
         throwBlock = isCompareLT ? compareNode->getBranchDestination()->getNode()->getBlock() : _block->getNextBlock();
         nextCompareBlock = isCompareLT ? _block->getNextBlock() : compareNode->getBranchDestination()->getNode()->getBlock();
         firstChildBndChk = firstChild;
         }
      else if (secondChild->getOpCode().isIntegralLoadVar() || secondChild->getOpCode().isLoadIndirect())
         {
         patternMatched = true;
         throwBlock = isCompareLT ? _block->getNextBlock() : compareNode->getBranchDestination()->getNode()->getBlock();
         nextCompareBlock = isCompareLT ? compareNode->getBranchDestination()->getNode()->getBlock() : _block->getNextBlock();
         secondChildBndChk = secondChild;
         }
      }

   if (!patternMatched)
      return false;

   if (trace())
      traceMsg(comp(), "   Matched with first %s in block_%d\n", isCompareLT ? "ificmplt" : "ificmpge",
                  _block->getNumber());

   if (nextCompareBlock->getFirstRealTreeTop() != nextCompareBlock->getLastRealTreeTop()) 
      {
      if (trace())
         traceMsg(comp(), "   Abort simplifyBoundCheckWithThrowException : pattern not matched\n");
      return false;
      }

   TR::Node *secondCompareNode = nextCompareBlock->getLastRealTreeTop()->getNode();

   if (secondCompareNode->getOpCodeValue() != TR::ificmplt && secondCompareNode->getOpCodeValue() != TR::ificmpge) 
      {
      if (trace())
         traceMsg(comp(), "   Abort simplifyBoundCheckWithThrowException : The second compare block does not contain ificmplt or ificmpge\n");
      return false;
      }

   bool firstNodeRequiresDuplicate = false;
   bool secondNodeRequiresDuplicate = false;
   bool isLastBlockRetBlock = true;
   patternMatched = false;
   TR::Block *retBlock = NULL;
   TR::Block *secondThrowBlock = NULL;
   bool isSecondCompareLT = secondCompareNode->getOpCodeValue() == TR::ificmplt;

   firstChild = secondCompareNode->getFirstChild();
   secondChild = secondCompareNode->getSecondChild();
   TR::SymbolReference *secondSymRef = NULL;
   
   if (firstChild->getOpCodeValue() == TR::iload)
      {
      secondSymRef = firstChild->getSymbolReference();
      if (secondChild->getOpCodeValue() == TR::iconst && secondChild->getConstValue() == 0)
         {
         patternMatched = true;
         secondThrowBlock = isSecondCompareLT ? secondCompareNode->getBranchDestination()->getNode()->getBlock() : nextCompareBlock->getNextBlock();
         retBlock = isSecondCompareLT ? nextCompareBlock->getNextBlock() : secondCompareNode->getBranchDestination()->getNode()->getBlock();
         isLastBlockRetBlock = !isSecondCompareLT;
         firstChildBndChk = firstChild;
         firstNodeRequiresDuplicate = true;
         }
      else if (secondChild->getOpCode().isIntegralLoadVar() || secondChild->getOpCode().isLoadIndirect())
         {
         patternMatched = true;
         secondThrowBlock = isSecondCompareLT ? nextCompareBlock->getNextBlock() : secondCompareNode->getBranchDestination()->getNode()->getBlock();
         retBlock = isSecondCompareLT ? secondCompareNode->getBranchDestination()->getNode()->getBlock() : nextCompareBlock->getNextBlock();
         isLastBlockRetBlock = isSecondCompareLT;
         secondChildBndChk = secondChild;
         secondNodeRequiresDuplicate = true;
         }
      }

   if (!patternMatched || !firstChildBndChk || !secondChildBndChk || throwBlock != secondThrowBlock)
      {
      if (trace())
         traceMsg(comp(), "Abort simplifyBoundCheckWithThrowException : %s\n", !patternMatched ? "Pattern did not matched" : 
            (!firstChildBndChk ? "None of the ificmp matches (param < 0) or (param >= 0)" : 
            (!secondChildBndChk ? "None of the ificmp matches (param < limit) or (param >= limit)" : 
            "Each of the branch jumps to different throw block")));
      return false;
      }

   if (!firstSymRef || firstSymRef != secondSymRef)
      {
      if (trace())
         traceMsg(comp(), "Abort simplifyBoundCheckWithThrowException : compare nodes uses different variables, pattern not matched\n");
      return false;
      }

   if (trace())
      traceMsg(comp(), "   Matched with second %s in block_%d\n", isSecondCompareLT ? "ificmplt" : "ificmpge",
               nextCompareBlock->getNumber());
   
   TR::Node *throwNode = throwBlock->getLastRealTreeTop()->getNode();
   if (throwNode->getNumChildren() < 1 || throwNode->getFirstChild()->getOpCodeValue() != TR::athrow) 
      {
      if (trace())
         traceMsg(comp(), "Abort simplifyBoundCheckWithThrowException : %s\n", throwNode->getNumChildren() < 1 ? "Expected throwNode does not contain children" :
         "The throwNode does not contain throw, pattern not matched");
      return false;
      }

   if (!performTransformation(comp(), "%sReplace %s n%dn [%p] followed by a %s n%dn [%p] that throws (block_%d) with a BNDCHK to a catch which goes to block_%d\n", 
            OPT_DETAILS, isCompareLT ? "ificmplt" : "ificmpge",
            compareNode->getGlobalIndex(), compareNode, isSecondCompareLT ? "ificmplt" : "ificmpge",
            secondCompareNode->getGlobalIndex(), secondCompareNode, throwBlock->getNumber(), throwBlock->getNumber()))
      return false;

   _cfg->invalidateStructure();

   TR::Block *compareBlock = _block;
   if (hasExceptionPoint(compareBlock, treeTop)) 
      compareBlock = compareBlock->split(treeTop, _cfg, true, false);

   TR::Node *bndChkNode = TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, 
                              secondNodeRequiresDuplicate ? secondChildBndChk->duplicateTree(true) : secondChildBndChk,
                              firstNodeRequiresDuplicate ? firstChildBndChk->duplicateTree(true) : firstChildBndChk,
                              comp()->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp()->getMethodSymbol()));

   bndChkNode = compareBlock->append(TR::TreeTop::create(comp(), bndChkNode))->getNode();

   TR::Block *catchBlock = TR::Block::createEmptyBlock(bndChkNode, comp(), throwBlock->getFrequency());
   catchBlock->setHandlerInfo(0, static_cast<uint8_t>(comp()->getInlineDepth()), 0, comp()->getCurrentMethod(), comp());
   TR::Node *gotoNode = TR::Node::create(bndChkNode, TR::Goto, 0);
   gotoNode->setBranchDestination(throwBlock->getEntry());
   catchBlock->append(TR::TreeTop::create(comp(), gotoNode));
   
   TR::TreeTop *swapTree = throwBlock->getExit()->getNextTreeTop();
   throwBlock->getExit()->join(catchBlock->getEntry());
   catchBlock->getExit()->join(swapTree);

   ncount_t replacedNodeId = compareNode->getGlobalIndex();
   TR::TransformUtil::removeTree(comp(), treeTop);

   if (trace()) 
      {
      traceMsg(comp(), "   Replaced %s n%dn with BNDCHK n%dn\n", isCompareLT ? "ificmplt" : "ificmpge", 
               replacedNodeId, bndChkNode->getGlobalIndex());
      traceMsg(comp(), "   Added a new goto node n%dn that branches to throw block (block_%d)\n", 
               gotoNode->getGlobalIndex(), throwBlock->getNumber());
      }

   TR::Block *jmpBlock = NULL;
   if (isLastBlockRetBlock) 
      {
      jmpBlock = TR::Block::createEmptyBlock(_block->getLastRealTreeTop()->getNode(), comp(), retBlock->getFrequency());
      jmpBlock->setHandlerInfo(0, static_cast<uint8_t>(comp()->getInlineDepth()), 0, comp()->getCurrentMethod(), comp());
      TR::Node *jmpNode = TR::Node::create(_block->getLastRealTreeTop()->getNode(), TR::Goto, 0);
      jmpNode->setBranchDestination(retBlock->getEntry());
      jmpBlock->append(TR::TreeTop::create(comp(), jmpNode));

      compareBlock->getExit()->join(jmpBlock->getEntry());
      jmpBlock->getExit()->join(nextCompareBlock->getEntry());

      if (trace())
         traceMsg(comp(), "   Added a new goto node n%dn that branches to return block (block_%d)\n",
                  jmpNode->getGlobalIndex(), retBlock->getNumber());
      }

   TR::TransformUtil::removeTree(comp(), nextCompareBlock->getLastRealTreeTop());
   TR::TransformUtil::removeTree(comp(), nextCompareBlock->getEntry());
   TR::TransformUtil::removeTree(comp(), nextCompareBlock->getExit());

   if (trace())
      traceMsg(comp(), "   Removed %s block (block_%d) which either branches to throw block (block_%d) or normal block (block_%d)\n",
            isSecondCompareLT ? "ificmplt" : "ificmpge", nextCompareBlock->getNumber(), throwBlock->getNumber(), retBlock->getNumber());

   _cfg->addNode(catchBlock);
   _cfg->addExceptionEdge(compareBlock, catchBlock);
   _cfg->addEdge(catchBlock, throwBlock);
   if (isLastBlockRetBlock) 
      {
      _cfg->addNode(jmpBlock);
      _cfg->addEdge(compareBlock, jmpBlock);
      _cfg->addEdge(jmpBlock, retBlock);
      }
   else
      {
      _cfg->addEdge(compareBlock, retBlock);
      }
   _cfg->removeEdge(compareBlock, throwBlock);
   _cfg->removeEdge(compareBlock, nextCompareBlock);
   _cfg->removeEdge(nextCompareBlock, retBlock);
   _cfg->removeEdge(nextCompareBlock, throwBlock);
   _cfg->removeNode(nextCompareBlock);

   nextCompareBlock->removeFromCFG(comp());

   if (trace()) 
      {
      traceMsg(comp(), "   Updated CFG\n");
      traceMsg(comp(), "End simplifyBoundCheckWithThrowException\n");
      }
   
   return true;
   }

static bool containsIndirectOperationImpl(TR::Node *node, TR::NodeChecklist *visited, int32_t depth)
   {
   if (visited->contains(node))
      return false;

   if (depth == 0)
      return true;

   visited->add(node);

   if (!(node->getOpCode().isArithmetic() && !node->getOpCode().isDiv())
       && !node->getOpCode().isLoadVarDirect()
       && !node->getOpCode().isSelect()
       && !node->getOpCode().isLoadConst())
      return true;

   if (node->getOpCode().hasSymbolReference()
       && !node->getSymbolReference()->getSymbol()->isAutoOrParm())
      return true;

   for (int i = 0; i < node->getNumChildren(); ++i)
      {
      if (containsIndirectOperationImpl(node->getChild(i), visited, depth-1))
         return true;
      }
   return false;
   }

static bool containsIndirectOperation(TR::Compilation *comp, TR::TreeTop *treetop)
   {
   TR::NodeChecklist visited(comp);
   return containsIndirectOperationImpl(treetop->getNode()->getFirstChild(), &visited, 3);
   }

bool OMR::CFGSimplifier::simplifyCondStoreSequence(bool needToDuplicateTree)
   {
   static char *enableSimplifyCondStoreSequence = feGetEnv("TR_enableSimplifyCondStoreSequence");
   if (enableSimplifyCondStoreSequence == NULL)
      return false;

   if (!(comp()->cg()->getSupportsSelect()))
      return false;

   if (trace())
      traceMsg(comp(), "Start simplifyCondStoreSequence block_%d\n", _block->getNumber());

   TR::TreeTop *compareTree = _block->getLastRealTreeTop();
   TR::Node *compareNode = compareTree->getNode();

   bool triangle2 = _next2->getSuccessors().size() == 1
       && _next2->getExceptionSuccessors().size() == 0
       && toBlock(_next2->getSuccessors().front()->getTo()) == _next1;
   bool triangle1 = _next1->getSuccessors().size () == 1
       && _next1->getExceptionSuccessors().size() == 0
       && toBlock(_next1->getSuccessors().front()->getTo()) == _next2;

   if (trace())
      {
      traceMsg(comp(), "   block%d triangle1: %d triangle2: %d\n", _block->getNumber(), triangle1, triangle2);
      }

   if (!triangle1 || triangle2) { return false; }

   TR::Block *toCheck = triangle1 ? _next1 : _next2;
   TR::TreeTop *treeCursor = toCheck->getEntry()->getNextTreeTop();
   int32_t count = 0;
   while (treeCursor->getNode()->getOpCode().isStoreDirect()
          && !treeCursor->getNode()->getOpCode().isWrtBar()
          && !containsIndirectOperation(comp(), treeCursor))
      {
      if (treeCursor->getNode()->getFirstChild()->isInternalPointer())
         return false;
      if (trace())
         traceMsg(comp(), "   Store value is not internal pointer\n");
      if (!treeCursor->getNode()->getDataType().isIntegral()
          && !treeCursor->getNode()->getDataType().isAddress())
         return false;
      if (trace())
         traceMsg(comp(), "   Store node n%dn data type checks out\n", treeCursor->getNode()->getGlobalIndex());
      if (!treeCursor->getNode()->getSymbolReference()->getSymbol()->isAutoOrParm())
         return false;
      if (trace())
         traceMsg(comp(), "   Store node n%dn symRef checks out\n", treeCursor->getNode()->getGlobalIndex());
      treeCursor = treeCursor->getNextTreeTop();
      count++;
      }

   if (treeCursor->getNode()->getOpCodeValue() != TR::BBEnd || count < 2)
      return false;

   if (!performTransformation(comp(), "%sReplace conditional stores in block_%d with stores of appropriate select at nodes\n", OPT_DETAILS, toCheck->getNumber()))
      return false;

   _cfg->invalidateStructure();

   TR::Node *condition = TR::Node::create(compareNode, compareNode->getOpCode().convertIfCmpToCmp(), 2,
         compareNode->getFirstChild(),
         compareNode->getSecondChild());

   treeCursor = toCheck->getEntry()->getNextTreeTop();
   while (treeCursor->getNode()->getOpCode().isStoreDirect()
          && !treeCursor->getNode()->getOpCode().isWrtBar())
      {
      TR::Node *storeNode = treeCursor->getNode();
      TR::Node *trueValue = triangle1 ? TR::Node::createWithSymRef(comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, storeNode->getSymbolReference()) : (needToDuplicateTree ? storeNode->getFirstChild()->duplicateTree() : storeNode->getFirstChild());
      TR::Node *falseValue = triangle1 ? (needToDuplicateTree ? storeNode->getFirstChild()->duplicateTree() : storeNode->getFirstChild()) : TR::Node::createWithSymRef(comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, storeNode->getSymbolReference());

      TR::Node *select = TR::Node::create(storeNode, comp()->il.opCodeForSelect(storeNode->getDataType()), 3);
      if (trace())
         traceMsg(comp(), "Created select node n%dn\n", select->getGlobalIndex());

      select->setAndIncChild(0, condition);
      select->setAndIncChild(1, trueValue);
      select->setAndIncChild(2, falseValue);
      TR::TreeTop *insTree = TR::TreeTop::create(comp(), TR::Node::createWithSymRef(storeNode, storeNode->getOpCodeValue(), 1, select, storeNode->getSymbolReference()));

      if (storeNode->getOpCodeValue() == TR::astore && storeNode->isHeapificationStore())
         insTree->getNode()->setHeapificationStore(true);

      compareTree->insertBefore(insTree);
      treeCursor = treeCursor->getNextTreeTop();
      }

   if (triangle1) {
      _cfg->removeEdge(_block, _next1);
      if (_block->getNextBlock() != _next2)
         {
         TR::Node *gotoNode = TR::Node::create(compareNode, TR::Goto, 0);
         gotoNode->setBranchDestination(_next2->getEntry());
         _block->append(TR::TreeTop::create(comp(), gotoNode));
         }
   } else {
      _cfg->removeEdge(_block, _next2);
      if (_block->getNextBlock() != _next2)
         {
         TR::Node *gotoNode = TR::Node::create(compareNode, TR::Goto, 0);
         gotoNode->setBranchDestination(_next1->getEntry());
         _block->append(TR::TreeTop::create(comp(), gotoNode));
         }
   }
   TR::TransformUtil::removeTree(comp(), compareTree);
   if (trace())
      traceMsg(comp(), "End simplifyCondStoreSequence.\n");
   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "cfgSimpMovSeq/%d/(%s)", count, comp()->signature()));
   return true;
   }

bool OMR::CFGSimplifier::simplifySimpleStore(bool needToDuplicateTree)
   {
   static char *enableSimplifySimpleStore = feGetEnv("TR_enableSimplifySimpleStore");
   if (enableSimplifySimpleStore == NULL)
      return false;

   if (!(comp()->cg()->getSupportsSelect()))
      return false;

   if (trace())
      traceMsg(comp(), "Start simplifySimpleStore block_%d\n", _block->getNumber());

   TR::TreeTop *compareTree = _block->getLastRealTreeTop();
   TR::Node *compareNode = compareTree->getNode();

   bool triangle2 = _next2->getSuccessors().size() == 1
       && _next2->getExceptionSuccessors().size() == 0
       && toBlock(_next2->getSuccessors().front()->getTo()) == _next1;
   bool triangle1 = _next1->getSuccessors().size () == 1
       && _next1->getExceptionSuccessors().size() == 0
       && toBlock(_next1->getSuccessors().front()->getTo()) == _next2;
   bool diamond = _next2->getSuccessors().size() == 1
       && _next2->getExceptionSuccessors().size() == 0
       && _next1->getSuccessors().size() == 1
       && _next1->getExceptionSuccessors().size() == 0
       && toBlock(_next1->getSuccessors().front()->getTo()) == toBlock(_next2->getSuccessors().front()->getTo());

   if (trace())
      {
      traceMsg(comp(), "   block%d triangle1: %d triangle2: %d diamond: %d\n", _block->getNumber(), triangle1, triangle2, diamond);
      }

   static char *disableSimplifySimpleStoreTriangle = feGetEnv("TR_disableSimplifySimpleStoreTriangle");
   if ((triangle1 || triangle2) && disableSimplifySimpleStoreTriangle != NULL)
      return false;

   static char *disableSimplifySimpleStoreDiamond = feGetEnv("TR_disableSimplifySimpleStoreDiamond");
   if ((diamond) && disableSimplifySimpleStoreDiamond != NULL)
      return false;

   if (!triangle1 && !triangle2 && !diamond)
      return false;

   if (trace())
      traceMsg(comp(), "   compareTree has correctType\n");

   TR::TreeTop *treeCursor = NULL;
   TR::Node *trueValue = NULL, *falseValue = NULL, *storeNode = NULL;
   bool isHeapificationStore = false;

   if (triangle2 || diamond)
      {
      treeCursor =_next2->getEntry()->getNextTreeTop();
      if (!treeCursor->getNode()->getOpCode().isStoreDirect()
          || treeCursor->getNode()->getOpCode().isWrtBar()
          || containsIndirectOperation(comp(), treeCursor))
         return false;

      if (trace())
         traceMsg(comp(), "   Take side has an appropriate store as the first tree\n");

      trueValue = treeCursor->getNode()->getFirstChild();
      if (treeCursor->getNode()->getFirstChild()->isInternalPointer())
         return false;

      if (trace())
         traceMsg(comp(), "   Value on taken side is not internal pointer\n");

      storeNode = treeCursor->getNode();
      isHeapificationStore = storeNode->getOpCodeValue() == TR::astore && storeNode->isHeapificationStore();

      if (treeCursor->getNextTreeTop()->getNode()->getOpCodeValue() != TR::BBEnd
          && treeCursor->getNextTreeTop()->getNode()->getOpCodeValue() != TR::Goto)
         return false;

      trueValue = treeCursor->getNode()->getFirstChild();

      if (trace())
         traceMsg(comp(), "   Taken side checks out\n");
      }

   if (triangle1 || diamond)
      {
      treeCursor = _next1->getEntry()->getNextTreeTop();
      if (!treeCursor->getNode()->getOpCode().isStoreDirect()
          || treeCursor->getNode()->getOpCode().isWrtBar()
          || containsIndirectOperation(comp(), treeCursor))
         return false;
      if (trace())
         traceMsg(comp(), "   Fallthrough side has an appropriate store as the first tree\n");

      if (treeCursor->getNode()->getFirstChild()->isInternalPointer())
         return false;

      if (trace())
         traceMsg(comp(), "   Value on fallthrough side is not internal pointer\n");

      if (storeNode != NULL
          && treeCursor->getNode()->getSymbolReference()->getReferenceNumber() != storeNode->getSymbolReference()->getReferenceNumber())
         return false;

      storeNode = treeCursor->getNode();
      isHeapificationStore = storeNode->getOpCodeValue() == TR::astore && ((diamond && isHeapificationStore && storeNode->isHeapificationStore()) || (!diamond && storeNode->isHeapificationStore()));

      if (trace())
         traceMsg(comp(), "   Fallthrough side is storing to the same symeref\n");

      traceMsg(comp(), "Next tree n%dn\n", treeCursor->getNextTreeTop()->getNode()->getGlobalIndex());
      if (treeCursor->getNextTreeTop()->getNode()->getOpCodeValue() != TR::BBEnd
          && treeCursor->getNextTreeTop()->getNode()->getOpCodeValue() != TR::Goto)
         return false;

      falseValue = treeCursor->getNode()->getFirstChild();

      if (trace())
         traceMsg(comp(), "   Fallthrough checks out\n");
      }

   if (!storeNode->getDataType().isIntegral()
       && !storeNode->getDataType().isAddress())
      return false;

   if (trace())
      traceMsg(comp(), "   StoreNode data type checks out\n");

   if (!diamond && !storeNode->getSymbolReference()->getSymbol()->isAutoOrParm())
      return false;

   if (trace())
      traceMsg(comp(), "   StoreNode symRef checks out\n");

   if (!performTransformation(comp(), "%sReplace conditional store with store of an appropriate select at node [%p]\n", OPT_DETAILS, compareNode))
      return false;

   _cfg->invalidateStructure();

   TR::Node *select = TR::Node::create(storeNode, comp()->il.opCodeForSelect(storeNode->getDataType()), 3);
   select->setAndIncChild(0,
      TR::Node::create(compareNode, compareNode->getOpCode().convertIfCmpToCmp(), 2,
         compareNode->getFirstChild(),
         compareNode->getSecondChild()));
   select->setAndIncChild(1,
      trueValue ? (needToDuplicateTree ? trueValue->duplicateTree() : trueValue) : TR::Node::createWithSymRef(comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, storeNode->getSymbolReference()));
   select->setAndIncChild(2,
      falseValue ? (needToDuplicateTree? falseValue->duplicateTree() : falseValue) : TR::Node::createWithSymRef(comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, storeNode->getSymbolReference()));
   TR::TreeTop *cmov = TR::TreeTop::create(comp(), TR::Node::createWithSymRef(storeNode, storeNode->getOpCodeValue(), 1, select, storeNode->getSymbolReference()));
   compareTree->insertBefore(cmov);
   if (isHeapificationStore)
      cmov->getNode()->setHeapificationStore(true);

   if (trace())
      traceMsg(comp(), "End simplifySimpleStore. New select node is n%dn\n", select->getGlobalIndex());

   TR::Block *dest = nullptr;
   if (diamond) {
      dest = toBlock(_next1->getSuccessors().front()->getTo());
      _cfg->addEdge(_block, dest);
      _cfg->removeEdge(_block, _next1);
      _cfg->removeEdge(_block, _next2);
   } else if (triangle2) {
      dest = _next1;
      _cfg->removeEdge(_block, _next2);
   } else if (triangle1) {
      dest = _next2;
      _cfg->removeEdge(_block, _next1);
   }
   if (_block->getNextBlock() != dest)
      {
      TR::Node *gotoNode = TR::Node::create(compareNode, TR::Goto, 0);
      gotoNode->setBranchDestination(dest->getEntry());
      _block->append(TR::TreeTop::create(comp(), gotoNode));
      }

   TR::TransformUtil::removeTree(comp(), compareTree);

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "cfgSimpCMOV/%s/(%s)", (diamond ? "diamond" : (triangle1 ? "triangle1" : "triangle2")), comp()->signature()));
   return true;
   }

bool OMR::CFGSimplifier::simplifyNullToException(bool needToDuplicateTree)
   {
   static char *disableSimplifyExplicitNULLTest = feGetEnv("TR_disableSimplifyExplicitNULLTest");
   static char *disableSimplifyNullToException = feGetEnv("TR_disableSimplifyNullToException");
   if (disableSimplifyExplicitNULLTest != NULL || disableSimplifyNullToException != NULL)
      return false;

   if (comp()->getOSRMode() == TR::involuntaryOSR)
      return false;

   if (trace())
      traceMsg(comp(), "Start simplifyNullToException\n");

   // This block must end in an ifacmpeq or ifacmpne against aconst NULL
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   if (compareNode->getOpCodeValue() != TR::ifacmpeq
       && compareNode->getOpCodeValue() != TR::ifacmpne)
      return false;

   if (trace())
      traceMsg(comp(), "   Found an ifacmp[eq/ne] n%dn\n", compareNode->getGlobalIndex());

   if (compareNode->getSecondChild()->getOpCodeValue() != TR::aconst
       || compareNode->getSecondChild()->getAddress() != 0)
      return false;

   // _next1 is fall through so grab the block where the value is NULL
   TR::Block *nullBlock = compareNode->getOpCodeValue() == TR::ifacmpeq ? _next2 : _next1;

   if (trace())
      traceMsg(comp(), "   Matched nullBlock %d\n", nullBlock->getNumber());

   // we want code sequence ending in a throw (any throw will do)
   TR::Node *lastRootNode = nullBlock->getLastRealTreeTop()->getNode();
   if (lastRootNode->getNumChildren() < 1
       || lastRootNode->getFirstChild()->getOpCodeValue() != TR::athrow)
      return false;

   if (!performTransformation(comp(), "%sReplace ifacmpeq/ifacmpne of NULL node n%dn [%p] to a blcok ending in throw with a NULLCHK to a catch which goes to block_%d\n", OPT_DETAILS, compareNode->getGlobalIndex(), compareNode, nullBlock->getNumber()))
      return false;

   _cfg->invalidateStructure();

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "cfgSimpNULLCHK/nullToException/(%s)", comp()->signature()));

   TR::Block *compareBlock = _block;
   if (hasExceptionPoint(compareBlock, compareTreeTop))
      compareBlock = compareBlock->split(compareTreeTop, _cfg, true, false);

   if (compareBlock->getNextBlock() == nullBlock) {
      TR::Node *gotoNode = TR::Node::create(compareNode, TR::Goto, 0);
      gotoNode->setBranchDestination((nullBlock == _next1 ? _next2 : _next1)->getEntry());
      compareBlock->append(TR::TreeTop::create(comp(), gotoNode));
   }

   TR::Node *nullchkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(compareNode, TR::PassThrough, 1, compareNode->getFirstChild()), comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol()));
   if (trace())
      traceMsg(comp(), "End simplifyNullToException. New NULLCHK node is n%dn\n", nullchkNode->getGlobalIndex());
   compareTreeTop->insertBefore(TR::TreeTop::create(comp(), nullchkNode));

   TR::Block *catchBlock = TR::Block::createEmptyBlock(compareNode, comp(), nullBlock->getFrequency());
   catchBlock->setHandlerInfo(0, static_cast<uint8_t>(comp()->getInlineDepth()), 0, comp()->getCurrentMethod(), comp());
   TR::Node *gotoNode = TR::Node::create(compareNode, TR::Goto, 0);
   gotoNode->setBranchDestination(nullBlock->getEntry());
   catchBlock->append(TR::TreeTop::create(comp(), gotoNode));

   TR::TreeTop *lastTree = comp()->findLastTree();
   catchBlock->getExit()->join(lastTree->getNextTreeTop());
   lastTree->join(catchBlock->getEntry());

   _cfg->addNode(catchBlock);
   _cfg->addExceptionEdge(compareBlock, catchBlock);
   _cfg->addEdge(catchBlock, nullBlock);
   _cfg->removeEdge(compareBlock, nullBlock);

   TR::TransformUtil::removeTree(comp(), compareTreeTop);
   return true;
   }

static bool checkEquivalentIndirectLoadChain(TR::Node *lhs, TR::Node *rhs)
   {
   if (lhs->getOpCodeValue() != rhs->getOpCodeValue())
      return false;

   if (lhs->getNumChildren() != rhs->getNumChildren())
      return false;

   if (lhs->getOpCode().hasSymbolReference()
       && lhs->getSymbolReference()->getReferenceNumber() != rhs->getSymbolReference()->getReferenceNumber())
      return false;

   if (lhs->getOpCode().isLoadDirect())
      {
      return true;
      }
   else if (lhs->getOpCode().isLoadIndirect()
            && lhs->getNumChildren() == 1
            && checkEquivalentIndirectLoadChain(lhs->getFirstChild(), rhs->getFirstChild()))
      {
      return true;
      }
   else if (lhs->getOpCodeValue() == TR::aladd
            && lhs->getSecondChild()->getOpCodeValue() == rhs->getSecondChild()->getOpCodeValue()
            && lhs->getSecondChild()->getOpCodeValue() == TR::lconst
            && lhs->getSecondChild()->getConstValue() == rhs->getSecondChild()->getConstValue()
            && checkEquivalentIndirectLoadChain(lhs->getFirstChild(), rhs->getFirstChild()))
      {
      return true;
      }
   else if (lhs->getOpCodeValue() == TR::aiadd
            && lhs->getSecondChild()->getOpCodeValue() == rhs->getSecondChild()->getOpCodeValue()
            && lhs->getSecondChild()->getOpCodeValue() == TR::iconst
            && lhs->getSecondChild()->getConstValue() == rhs->getSecondChild()->getConstValue()
            && checkEquivalentIndirectLoadChain(lhs->getFirstChild(), rhs->getFirstChild()))
      {
      return true;
      }
   return false;
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
bool OMR::CFGSimplifier::simplifyBooleanStore(bool needToDuplicateTree)
   {
   static char *enableSimplifyBooleanStore = feGetEnv("TR_enableSimplifyBooleanStore");
   if (enableSimplifyBooleanStore == NULL)
      return false;

   if (!(comp()->cg()->getSupportsSelect()))
      return false;

   if (trace())
      traceMsg(comp(), "Start simplifyBooleanStore\n");

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

   if (trace())
      traceMsg(comp(), "   Control flow checks out\n");

   TR::Block *joinBlock = toBlock(_next1->getSuccessors().front()->getTo());

   // This block must end in a compare-and-branch which can be converted to a
   // boolean compare, or a branch using the condition code.
   //
   TR::TreeTop *compareTreeTop = getLastRealTreetop(_block);
   TR::Node *compareNode       = compareTreeTop->getNode();
   bool isBranchOnCondCode = false;
   if (compareNode->getOpCode().convertIfCmpToCmp() == TR::BadILOp)
      return false;

   if (trace())
      traceMsg(comp(), "   Found a Compare node n%dn\n", compareNode->getGlobalIndex());

   // The trees of each successor block must consist of a single store.
   //
   TR::TreeTop *store1TreeTop = getNextRealTreetop(_next1->getEntry());
   if (store1TreeTop == NULL || getNextRealTreetop(store1TreeTop) != NULL)
      return false;
   TR::Node *store1 = store1TreeTop->getNode();
   if (!store1->getOpCode().isStore())
      return false;
   if (trace())
      traceMsg(comp(), "   Successor block_%d is single store\n", _next1->getNumber());
   TR::TreeTop *store2TreeTop = getNextRealTreetop(_next2->getEntry());
   if (store2TreeTop == NULL || getNextRealTreetop(store2TreeTop) != NULL)
      return false;
   TR::Node *store2 = store2TreeTop->getNode();
   if (!store2->getOpCode().isStore())
      return false;
   if (trace())
      traceMsg(comp(), "   Successor block_%d is single store\n", _next2->getNumber());

   // Store values cannot be internal pointers
   //
   int32_t valueIndex = store1->getOpCode().isIndirect() ? 1 : 0;
   TR::Node *value1 = store1->getChild(valueIndex);
   TR::Node *value2 = store2->getChild(valueIndex);
   if (value1->isInternalPointer() || value2->isInternalPointer())
      return false;
   if (trace())
      traceMsg(comp(), "   Store values are not internal pointers\n");

   // The stores must be integer stores to the same variable
   //
   if (store1->getOpCodeValue() != store2->getOpCodeValue())
      return false;
   if (!store1->getOpCode().isInt() && !store1->getOpCode().isByte())
      return false;
   if (store1->getSymbolReference()->getSymbol() != store2->getSymbolReference()->getSymbol())
      return false;
   if (trace())
      traceMsg(comp(), "   Store nodes opcode and symref checks out\n");

   // Indirect stores must have the same base
   //
   if (valueIndex == 1) // indirect store, check base objects
      {
      TR::Node *base1 = store1->getFirstChild();
      TR::Node *base2 = store2->getFirstChild();
      // accessing the same symbol
      if (!checkEquivalentIndirectLoadChain(base1, base2))
         return false;
      if (trace())
         traceMsg(comp(), "   Indirect store base node opcode and symref checks out\n");
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

   if (trace())
      traceMsg(comp(), "   Comparison values check out\n");

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
   if (trace())
      traceMsg(comp(), "End simplifyBooleanStore. New store node is n%dn\n", storeNode->getGlobalIndex());
   return true;
   }

TR::TreeTop *OMR::CFGSimplifier::getNextRealTreetop(TR::TreeTop *treeTop)
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

TR::TreeTop *OMR::CFGSimplifier::getLastRealTreetop(TR::Block *block)
   {
   TR::TreeTop *treeTop = block->getLastRealTreeTop();
   if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
      return NULL;
   return treeTop;
   }

TR::Block *OMR::CFGSimplifier::getFallThroughBlock(TR::Block *block)
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
bool OMR::CFGSimplifier::simplifyCondCodeBooleanStore(TR::Block *joinBlock, TR::Node *branchNode, TR::Node *store1Node, TR::Node *store2Node)
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
bool OMR::CFGSimplifier::canReverseBranchMask()
   {
   return false;
   }

const char *
OMR::CFGSimplifier::optDetailString() const throw()
   {
   return "O^O CFG SIMPLIFICATION: ";
   }
