/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "optimizer/RegDepCopyRemoval.hpp"

#include "compile/Compilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Checklist.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "ras/DebugCounter.hpp"

TR::RegDepCopyRemoval::RegDepCopyRemoval(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   , _regBegin(cg()->getFirstGlobalGPR())
   , _regEnd(cg()->getLastGlobalGPR() + 1)
   , _regDepInfoTable(getTypedAllocator<RegDepInfo>(allocator()))
   , _nodeChoiceTable(getTypedAllocator<NodeChoice>(allocator()))
   , _treetop(NULL)
   , _regDeps(NULL)
   {
   TR_ASSERT(_regBegin <= _regEnd, "upper and lower global register number bounds out of order: [%d, %d)\n", _regBegin, _regEnd);
   int regCount = _regEnd - _regBegin;
   _regDepInfoTable.resize(regCount);
   _nodeChoiceTable.resize(regCount);
   discardAllNodeChoices();
   clearRegDepInfo();
   }

int32_t
TR::RegDepCopyRemoval::perform()
   {
   if (!cg()->supportsPassThroughCopyToNewVirtualRegister())
      return 0;

   discardAllNodeChoices();
   TR::TreeTop *tt;
   for (tt = comp()->getStartTree(); tt != NULL; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      switch (node->getOpCodeValue())
         {
         case TR::BBStart:
            if (!node->getBlock()->isExtensionOfPreviousBlock())
               {
               if (trace())
                  traceMsg(comp(), "clearing remembered node choices at start of extended block at block_%d\n", node->getBlock()->getNumber());
               discardAllNodeChoices();
               }
            if (node->getNumChildren() > 0)
               processRegDeps(node->getFirstChild(), tt);
            break;
         case TR::BBEnd:
            if (node->getNumChildren() > 0)
               processRegDeps(node->getFirstChild(), tt);
            break;
         default:
            if (node->getOpCode().isSwitch())
               {
               TR::Node *defaultDest = node->getSecondChild();
               if (defaultDest->getNumChildren() > 0)
                  processRegDeps(defaultDest->getFirstChild(), tt);
               }
            else if (node->getOpCode().isBranch())
               {
               int nChildren = node->getNumChildren();
               // only the last child may be GlRegDeps
               for (int i = 0; i < nChildren - 1; i++)
                  TR_ASSERT(node->getChild(i)->getOpCodeValue() != TR::GlRegDeps, "GlRegDeps for branch is not the last child\n");
               if (nChildren > 0)
                  {
                  TR::Node *lastChild = node->getChild(nChildren - 1);
                  if (lastChild->getOpCodeValue() == TR::GlRegDeps)
                     processRegDeps(lastChild, tt);
                  }
               }
            else if (node->getOpCode().isStoreReg() 
                     && node->getHighGlobalRegisterNumber() == static_cast<TR_GlobalRegisterNumber>(-1)
                     && (node->getType().isIntegral() || node->getType().isAddress()))
               {
               TR_GlobalRegisterNumber lowReg = node->getLowGlobalRegisterNumber();
               NodeChoice &choice = getNodeChoice(lowReg);
               choice.regStoreNode = node;
               }
            break;
         }
      }
   return 1; // a bit arbitrary...
   }

const char *
TR::RegDepCopyRemoval::registerName(TR_GlobalRegisterNumber reg)
   {
   // this defaultSize only works for GPRs
   TR_RegisterSizes defaultSize = comp()->target().is64Bit() ? TR_DoubleWordReg : TR_WordReg;
   return comp()->getDebug()->getGlobalRegisterName(reg, defaultSize);
   }

void
TR::RegDepCopyRemoval::rangeCheckRegister(TR_GlobalRegisterNumber reg)
   {
   TR_ASSERT(reg >= _regBegin && reg < _regEnd, "global register number %d out of range\n", (int)reg);
   }

TR::RegDepCopyRemoval::RegDepInfo &
TR::RegDepCopyRemoval::getRegDepInfo(TR_GlobalRegisterNumber reg)
   {
   rangeCheckRegister(reg);
   return _regDepInfoTable.at(reg - _regBegin);
   }

TR::RegDepCopyRemoval::NodeChoice &
TR::RegDepCopyRemoval::getNodeChoice(TR_GlobalRegisterNumber reg)
   {
   rangeCheckRegister(reg);
   return _nodeChoiceTable.at(reg - _regBegin);
   }

void
TR::RegDepCopyRemoval::discardAllNodeChoices()
   {
   for (TR_GlobalRegisterNumber reg = _regBegin; reg < _regEnd; reg++)
      clearNodeChoice(reg);
   }

void
TR::RegDepCopyRemoval::discardNodeChoice(TR_GlobalRegisterNumber reg)
   {
   NodeChoice &choice = getNodeChoice(reg);
   choice.original = NULL;
   choice.selected = NULL;
   }

void
TR::RegDepCopyRemoval::clearNodeChoice(TR_GlobalRegisterNumber reg)
   {
   NodeChoice &choice = getNodeChoice(reg);
   choice.original = NULL;
   choice.selected = NULL;
   choice.regStoreNode = NULL;
   }

void
TR::RegDepCopyRemoval::rememberNodeChoice(TR_GlobalRegisterNumber reg, TR::Node *selected)
   {
   const RegDepInfo &dep = getRegDepInfo(reg);
   NodeChoice &choice = getNodeChoice(reg);
   choice.original = dep.value;
   choice.selected = selected;
   }

void
TR::RegDepCopyRemoval::processRegDeps(TR::Node *deps, TR::TreeTop *depTT)
   {
   if (trace())
      traceMsg(comp(), "processing GlRegDeps n%un\n", deps->getGlobalIndex());

   TR_ASSERT(deps->getOpCodeValue() == TR::GlRegDeps, "processDeps: deps is not GlRegDeps\n");

   _treetop = depTT;
   _regDeps = deps;

   clearRegDepInfo();
   readRegDeps();

   TR::NodeChecklist usedNodes(comp());
   selectNodesToReuse(usedNodes);
   selectNodesToCopy(usedNodes);
   updateRegDeps(usedNodes);
   }

void
TR::RegDepCopyRemoval::clearRegDepInfo()
   {
   for (TR_GlobalRegisterNumber reg = _regBegin; reg < _regEnd; reg++)
      {
      RegDepInfo &dep = getRegDepInfo(reg);
      dep.node = NULL;
      dep.value = NULL;
      dep.state = REGDEP_ABSENT;
      dep.childIndex = -1;
      }
   }

void
TR::RegDepCopyRemoval::readRegDeps()
   {
   for (int i = 0; i < _regDeps->getNumChildren(); i++)
      {
      TR::Node *depNode = _regDeps->getChild(i);
      TR::Node *depValue = depNode;
      if (depValue->getOpCodeValue() == TR::PassThrough)
         {
         do
            depValue = depValue->getFirstChild();
         while (depValue->getOpCodeValue() == TR::PassThrough);
         }
      else
         {
         TR_ASSERT(depNode->getOpCode().isLoadReg(), "invalid GlRegDeps child opcode n%un %s\n", depNode->getGlobalIndex(), depNode->getOpCode().getName());
         }

      // Avoid register pairs for simplicity, at least for now
      bool isRegPairDep = depNode->getHighGlobalRegisterNumber() != (TR_GlobalRegisterNumber)-1;
      bool valueNeedsRegPair = depValue->requiresRegisterPair(comp());
      TR_ASSERT(isRegPairDep == valueNeedsRegPair, "mismatch on number of registers required for n%un\n", depNode->getGlobalIndex());
      if (isRegPairDep)
         {
         ignoreRegister(depNode->getLowGlobalRegisterNumber());
         ignoreRegister(depNode->getHighGlobalRegisterNumber());
         continue;
         }


      TR_GlobalRegisterNumber reg = depNode->getGlobalRegisterNumber();
      TR::DataType depType = depValue->getType();
      // Only process integral and address-type nodes; they'll go into GPRs
      if (!depType.isIntegral() && !depType.isAddress()) 
         {
         ignoreRegister(reg);
         continue;
         }
      
      NodeChoice &choice = getNodeChoice(reg);
      // In a rare case, for the given register last regStore we have seen used different node then the node under PassThrough
      // Skipping such cases. 
      if (choice.regStoreNode != NULL && choice.regStoreNode->getFirstChild() != depValue)
         {
         ignoreRegister(reg);
         continue;
         }
      RegDepInfo &dep = getRegDepInfo(reg);
      TR_ASSERT(dep.state == REGDEP_ABSENT, "register %s is multiply-specified\n", registerName(reg));
      dep.node = depNode;
      dep.value = depValue;
      dep.state = REGDEP_UNDECIDED;
      dep.childIndex = i;
      }
   }

void
TR::RegDepCopyRemoval::ignoreRegister(TR_GlobalRegisterNumber reg)
   {
   if (reg < _regBegin || reg >= _regEnd)
      return; // don't even have corresponding array elements
   RegDepInfo &dep = getRegDepInfo(reg);
   TR_ASSERT(dep.state == REGDEP_ABSENT, "attempted to ignore previously specified register %s\n", registerName(reg));
   dep.state = REGDEP_IGNORED;
   }

void
TR::RegDepCopyRemoval::selectNodesToReuse(TR::NodeChecklist &usedNodes)
   {
   for (TR_GlobalRegisterNumber reg = _regBegin; reg < _regEnd; reg++)
      {
      RegDepInfo &dep = getRegDepInfo(reg);
      if (dep.state != REGDEP_UNDECIDED)
         continue;

      NodeChoice &prevChoice = getNodeChoice(reg);
      if (dep.value != prevChoice.original)
         continue; // can't reuse

      // Reuse our previous choice!
      if (trace())
         traceMsg(comp(), "\t%s: prefer to reuse previous choice n%un\n", registerName(reg), prevChoice.selected->getGlobalIndex());

      TR_ASSERT(!usedNodes.contains(prevChoice.selected), "attempted to reuse the same node more than once\n");
      if (prevChoice.selected == dep.value)
         {
         dep.state = REGDEP_NODE_ORIGINAL;
         usedNodes.add(dep.value);
         }
      else
         {
         dep.state = REGDEP_NODE_REUSE_COPY;
         }
      }
   }

void
TR::RegDepCopyRemoval::selectNodesToCopy(TR::NodeChecklist &usedNodes)
   {
   for (TR_GlobalRegisterNumber reg = _regBegin; reg < _regEnd; reg++)
      {
      RegDepInfo &dep = getRegDepInfo(reg);
      if (dep.state != REGDEP_UNDECIDED)
         continue;

      if (!usedNodes.contains(dep.value))
         {
         dep.state = REGDEP_NODE_ORIGINAL;
         usedNodes.add(dep.value);
         if (trace())
            traceMsg(comp(), "\t%s: prefer to keep the original node n%un\n", registerName(reg), dep.value->getGlobalIndex());
         }
      else
         {
         dep.state = REGDEP_NODE_FRESH_COPY;
         if (trace())
            traceMsg(comp(), "\t%s: prefer to make a new copy of n%un\n", registerName(reg), dep.value->getGlobalIndex());
         }
      }
   }

void
TR::RegDepCopyRemoval::updateRegDeps(TR::NodeChecklist &usedNodes)
   {
   for (TR_GlobalRegisterNumber reg = _regBegin; reg < _regEnd; reg++)
      {
      RegDepInfo &dep = getRegDepInfo(reg);
      switch (dep.state)
         {
         case REGDEP_NODE_ORIGINAL:
            rememberNodeChoice(reg, dep.value);
            break;
         case REGDEP_NODE_REUSE_COPY:
            reuseCopy(reg);
            break;
         case REGDEP_NODE_FRESH_COPY:
            makeFreshCopy(reg);
            break;
         case REGDEP_IGNORED:
            discardNodeChoice(reg);
            break;
         case REGDEP_ABSENT:
            {
            TR::Node *prevSel = getNodeChoice(reg).selected;
            if (prevSel != NULL && usedNodes.contains(prevSel))
               discardNodeChoice(reg);
            break;
            }
         case REGDEP_UNDECIDED:
            TR_ASSERT(false, "undecided preference for %s survived to updateRegDeps()\n", registerName(reg));
            break;
         default:
            TR_ASSERT(false, "%s has bad RegDepInfo state %d\n", registerName(reg), (int)dep.state);
            break;
         }
      }
   }

void
TR::RegDepCopyRemoval::reuseCopy(TR_GlobalRegisterNumber reg)
   {
   RegDepInfo &dep = getRegDepInfo(reg);
   NodeChoice &prevChoice = getNodeChoice(reg);
   TR_ASSERT(prevChoice.original == dep.value, "previous copy for %s doesn't match original\n", registerName(reg));
   TR_ASSERT(prevChoice.selected != dep.value, "previous copy is the same as original for %s\n", registerName(reg));
   if (performTransformation(comp(),
         "%schange %s in GlRegDeps n%un to use previous copy n%un of n%un\n",
         optDetailString(),
         registerName(reg),
         _regDeps->getGlobalIndex(),
         prevChoice.selected->getGlobalIndex(),
         prevChoice.original->getGlobalIndex()))
      {
      generateRegcopyDebugCounter("reuse-copy");
      updateSingleRegDep(reg, prevChoice.selected);
      }
   }

void
TR::RegDepCopyRemoval::makeFreshCopy(TR_GlobalRegisterNumber reg)
   {
   RegDepInfo &dep = getRegDepInfo(reg);
   if (!performTransformation(comp(),
         "%schange %s in GlRegDeps n%un to an explicit copy of n%un\n",
         optDetailString(),
         registerName(reg),
         _regDeps->getGlobalIndex(),
         dep.value->getGlobalIndex()))
      return;

   // Split the block at fallthrough if necessary to avoid putting copies
   // between branches and BBEnd.
   TR::Node *curNode = _treetop->getNode();
   if (curNode->getOpCodeValue() == TR::BBEnd)
      {
      TR::Block *curBlock = curNode->getBlock();
      if (curBlock->getLastRealTreeTop() != curBlock->getLastNonControlFlowTreeTop())
         {
         TR::Block *fallthrough = curBlock->getNextBlock();
         fallthrough = curBlock->splitEdge(curBlock, fallthrough, comp());
         TR_ASSERT(curBlock->getNextBlock() == fallthrough, "bad block placement from splitEdge\n");
         fallthrough->setIsExtensionOfPreviousBlock();
         _treetop = fallthrough->getExit();
         TR::Node *newNode = _treetop->getNode();
         newNode->setChild(0, _regDeps);
         newNode->setNumChildren(1);
         curNode->setNumChildren(0);
         if (trace())
            traceMsg(comp(), "\tsplit fallthrough edge to insert copy, created block_%d\n", fallthrough->getNumber());
         }
      }

   // Make and insert the copy
   TR::Node *copyNode = NULL;
   if (dep.value->getOpCode().isLoadConst())
      {
      // No need to depend on the other register.
      // TODO heuristic for whether this is really better than a reg-reg move?
      generateRegcopyDebugCounter("const-remat");
      copyNode = TR::Node::create(dep.value->getOpCodeValue(), 0);
      copyNode->setConstValue(dep.value->getConstValue());
      }
   else
      {
      generateRegcopyDebugCounter("fresh-copy");
      copyNode = TR::Node::create(TR::PassThrough, 1, dep.value);
      copyNode->setCopyToNewVirtualRegister();
      }
   NodeChoice &choice = getNodeChoice(reg);
   if (choice.regStoreNode == NULL)
      {
      // As we walk down in Extended Basic Block, for each register, if exists, we record a regStore node, if we do not have one found for given register, node should be regLoad.
      // It is possible that the child node of GlRegDep is PassThrough with regLoad child where both uses same register. In this case, it won't have the corresponding regStore.
      TR_ASSERT_FATAL_WITH_NODE(dep.node,
                                 dep.node->getOpCode().isLoadReg()
                                 || (dep.node->getOpCodeValue() == TR::PassThrough
                                    && dep.value->getOpCode().isLoadReg()),
                                 "Only PassThrough (with corresponding regStore appeared before or contains regLoad child) or regLoad nodes are expected as children of GlRegDeps.");
      choice.regStoreNode = TR::Node::create(dep.node, comp()->il.opCodeForRegisterStore(dep.value->getDataType()), 1, copyNode);
      _treetop->insertBefore(TR::TreeTop::create(comp(), choice.regStoreNode));
      choice.regStoreNode->setGlobalRegisterNumber(dep.node->getGlobalRegisterNumber());
      choice.regStoreNode->setRegLoadStoreSymbolReference(dep.value->getRegLoadStoreSymbolReference());
      }
   else
      {
      choice.regStoreNode->setAndIncChild(0, copyNode);
      dep.value->recursivelyDecReferenceCount();
      }
   if (trace())
      traceMsg(comp(), "\tcopy is n%un\n", copyNode->getGlobalIndex());

   updateSingleRegDep(reg, copyNode);
   }

void
TR::RegDepCopyRemoval::updateSingleRegDep(TR_GlobalRegisterNumber reg, TR::Node *newValueNode)
   {
   RegDepInfo &dep = getRegDepInfo(reg);
   TR_ASSERT(_treetop->getNode()->getOpCodeValue() != TR::BBStart, "attempted to change %s in incoming GlRegDeps on BBStart n%un\n", registerName(reg), _treetop->getNode()->getGlobalIndex());

   TR::Node *prevChild = _regDeps->getChild(dep.childIndex);
   TR_ASSERT(prevChild == dep.node, "childIndex and node inconsistent in RegDepInfo for %s\n", registerName(reg));
   TR_ASSERT(prevChild->getGlobalRegisterNumber() == reg, "childIndex and reg inconsistent in RegDepInfo for %s\n", registerName(reg));

   if (newValueNode->getOpCode().isLoadReg()
       && newValueNode->getGlobalRegisterNumber() == reg)
      {
      _regDeps->setAndIncChild(dep.childIndex, newValueNode);
      }
   else
      {
      TR::Node *newOutgoingPassThroughNode = TR::Node::create(TR::PassThrough, 1, newValueNode);
      newOutgoingPassThroughNode->setGlobalRegisterNumber(reg);
      _regDeps->setAndIncChild(dep.childIndex, newOutgoingPassThroughNode);
      }

   prevChild->recursivelyDecReferenceCount();
   rememberNodeChoice(reg, newValueNode);
   }

void
TR::RegDepCopyRemoval::generateRegcopyDebugCounter(const char *category)
   {
   if (!comp()->getOptions()->enableDebugCounters())
      return;
   const char *fullName = TR::DebugCounter::debugCounterName(comp(),
      "regcopy/RegDepCopyRemoval/%s/(%s)/%s/block_%d",
      category,
      comp()->signature(),
      comp()->getHotnessName(comp()->getMethodHotness()),
      _treetop->getEnclosingBlock()->getNumber());
   TR::DebugCounter::prependDebugCounter(comp(), fullName, _treetop);
   }

const char *
TR::RegDepCopyRemoval::optDetailString() const throw()
   {
   return "O^O REGISTER DEPENDENCY COPY REMOVAL: ";
   }
