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

#include "compiler/il/OMRTreeTop.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"

TR::TreeTop *
OMR::TreeTop::create(TR::Compilation *comp)
   {
   bool trace = comp->getOption(TR_TraceCG) || debug("traceGRA");
   return new (trace, comp->trMemory()) TR::TreeTop();
   }

TR::TreeTop *
OMR::TreeTop::create(TR::Compilation *comp, TR::Node *node, TR::TreeTop *next, TR::TreeTop *prev)
   {
   bool trace = comp->getOption(TR_TraceCG) || debug("traceGRA");
   return new (trace, comp->trMemory()) TR::TreeTop(node, next, prev);
   }

TR::TreeTop *
OMR::TreeTop::create(TR::Compilation *comp, TR::TreeTop *precedingTreeTop, TR::Node *node)
   {
   bool trace = comp->getOption(TR_TraceCG) || debug("traceGRA");
   return new (trace, comp->trMemory()) TR::TreeTop(precedingTreeTop, node, comp);
   }

TR::TreeTop *
OMR::TreeTop::duplicateTree()
   {
   TR::Compilation* comp = TR::comp();
   return self()->create(comp, self()->getNode()->duplicateTree(), self()->getPrevTreeTop(), self()->getNextTreeTop());
   }

TR::TreeTop *
OMR::TreeTop::createIncTree(TR::Compilation * comp, TR::Node *node, TR::SymbolReference * symRef, int32_t incAmount, TR::TreeTop *precedingTreeTop, bool isRecompCounter)
   {
   TR::StaticSymbol * symbol = symRef->getSymbol()->castToStaticSymbol();
   TR::DataType type = symbol->getDataType();
   TR::Node * storeNode;
   if (comp->cg()->getAccessStaticsIndirectly() && !symRef->isUnresolved() && type != TR::Address)
      {
      TR::SymbolReference * symRefShadow;
      if (isRecompCounter)
         symRefShadow = comp->getSymRefTab()->findOrCreateCounterAddressSymbolRef();
      else
         symRefShadow = comp->getSymRefTab()->createKnownStaticDataSymbolRef(symbol->getStaticAddress(), TR::Address);
      TR::Node * loadaddrNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, symRefShadow);
      storeNode = TR::Node::createWithSymRef(TR::istorei, 2, 2, loadaddrNode,
                    TR::Node::create(TR::iadd, 2,
                      TR::Node::createWithSymRef(TR::iloadi, 1, 1, loadaddrNode, symRef),
                        TR::Node::create(node, TR::iconst, 0, incAmount)), symRef);
      }
   else
      {
      storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
                    TR::Node::create(TR::iadd, 2,
                      TR::Node::createWithSymRef(node, TR::iload, 0, symRef),
                        TR::Node::create(node, TR::iconst, 0, incAmount)), symRef);
      }
   return precedingTreeTop == NULL ? TR::TreeTop::create(comp, storeNode) : TR::TreeTop::create(comp, precedingTreeTop, storeNode);
   }

TR::TreeTop *
OMR::TreeTop::createResetTree(TR::Compilation * comp, TR::Node *node, TR::SymbolReference * symRef, int32_t resetAmount, TR::TreeTop *precedingTreeTop, bool isRecompCounter)
   {
   TR::StaticSymbol * symbol = symRef->getSymbol()->castToStaticSymbol();
   TR::DataType type = symbol->getDataType();
   TR::Node * storeNode;
   if (comp->cg()->getAccessStaticsIndirectly() && !symRef->isUnresolved() && type != TR::Address)
      {
      TR::SymbolReference * symRefShadow;
      if (isRecompCounter)
         symRefShadow = comp->getSymRefTab()->findOrCreateCounterAddressSymbolRef();
      else
         symRefShadow = comp->getSymRefTab()->createKnownStaticDataSymbolRef(symbol->getStaticAddress(), TR::Address);
      TR::Node * loadaddrNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, symRefShadow);
      storeNode = TR::Node::createWithSymRef(TR::istorei, 2, 2, loadaddrNode,
                      TR::Node::create(node, TR::iconst, 0, resetAmount), symRef);
      }
   else
      {
      storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
                    TR::Node::create(node, TR::iconst, 0, resetAmount), symRef);
      }
   return precedingTreeTop == NULL ? TR::TreeTop::create(comp, storeNode) : TR::TreeTop::create(comp, precedingTreeTop, storeNode);
   }

void
OMR::TreeTop::insertTreeTops(TR::Compilation *comp, TR::TreeTop *beforeInsertionPoint, TR::TreeTop *firstTree, TR::TreeTop *lastTree = NULL)
   {
   if (!lastTree)
      lastTree = firstTree;
   if (beforeInsertionPoint == NULL)
      comp->setStartTree(firstTree);
   else
      lastTree->join(beforeInsertionPoint->getNextTreeTop());
   beforeInsertionPoint->join(firstTree);
   }

void
OMR::TreeTop::insertTreeTopsBeforeMe(TR::TreeTop *firstTree, TR::TreeTop *lastTree)
   {
   if (!lastTree)
      lastTree = firstTree;
   if (self()->getPrevTreeTop())
      self()->getPrevTreeTop()->join(firstTree);
   else
      firstTree->setPrevTreeTop(NULL);
   lastTree->join(self());
   }

void
OMR::TreeTop::insertTreeTopsAfterMe(TR::TreeTop *firstTree, TR::TreeTop *lastTree)
   {
   if (!lastTree)
      lastTree = firstTree;
   lastTree->join(self()->getNextTreeTop());
   self()->join(firstTree);
   }


// removeDeadTrees will take a list of trees and 'kill' them by converting the children of the tree
// to be tree-tops themselves.
// This is handy if you need to convert a set of trees to a new set of trees.

void
OMR::TreeTop::removeDeadTrees(TR::Compilation * comp, TR::TreeTop* list[])
   {
   for (int i=0; list[i] != NULL; ++i)
      {
      int numChildren = list[i]->getNode()->getNumChildren();
      for (int child = numChildren-1; child>0; --child)
         {
         TR::Node * node = list[i]->getNode()->getChild(child);
         list[i]->insertAfter(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, node)));
         node->decReferenceCount();
         }
      if (numChildren != 0)
         {
         TR::Node * node = list[i]->getNode()->getChild(0);
         list[i]->setNode(TR::Node::create(TR::treetop, 1, node));
         node->decReferenceCount();
         }
      }
   }

void
OMR::TreeTop::removeDeadTrees(TR::Compilation * comp, TR::TreeTop* first, TR::TreeTop* last)
   {
   for (TR::TreeTop* cur = first; cur != last; cur = cur->getNextTreeTop())
      {
      int numChildren = cur->getNode()->getNumChildren();
      for (int child = numChildren-1; child>0; --child)
         {
         TR::Node * node = cur->getNode()->getChild(child);
         cur->insertAfter(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, node)));
         node->decReferenceCount();
         }
      if (numChildren != 0)
         {
         TR::Node * node = cur->getNode()->getChild(0);
         cur->setNode(TR::Node::create(TR::treetop, 1, node));
         node->decReferenceCount();
         }
      }
   }

void *
OMR::TreeTop::operator new(size_t s, bool trace, TR_Memory * m)
   {
   if (!trace)
      return m->allocateHeapMemory(s);

   s += sizeof(void *);

   char * p = (char *)m->allocateHeapMemory(s);

   TR::TreeTop * tt = (TR::TreeTop *)(p + sizeof(void *));
   tt->setLastInstruction(0);
   return tt;
   }

void
OMR::TreeTop::operator delete(void *p, bool trace, TR_Memory *m)
   {
   m->freeMemory(p, TR_AllocationKind::heapAlloc);
   }

OMR::TreeTop::TreeTop(TR::TreeTop *precedingTreeTop, TR::Node *node, TR::Compilation * c) :
   _pNode(node)
   {
   if (precedingTreeTop != NULL)
      {
      self()->setNextTreeTop(precedingTreeTop->getNextTreeTop());
      self()->setPrevTreeTop(precedingTreeTop);
      if (precedingTreeTop->getNextTreeTop())
         {
         precedingTreeTop->getNextTreeTop()->setPrevTreeTop(self());
         }
      precedingTreeTop->setNextTreeTop(self());
      }
   else
      {
      self()->setNextTreeTop(c->getStartTree());
      self()->setPrevTreeTop(NULL);
      if (c->getStartTree())
         {
         c->getStartTree()->setPrevTreeTop(self());
         }
      c->getMethodSymbol()->setFirstTreeTop(self());
      }
   }

TR::TreeTop *
OMR::TreeTop::getExtendedBlockExitTreeTop()
   {
   TR_ASSERT(self()->getNode()->getOpCodeValue() == TR::BBStart, "getExitTreeTop, is only valid for a bbStart");
   TR::Block * b;
   TR::TreeTop * exitTT = self()->getNode()->getBlock()->getExit(), * nextTT;
   while ((nextTT = exitTT->getNextTreeTop()) && (b = nextTT->getNode()->getBlock(), b->isExtensionOfPreviousBlock()))
      exitTT = b->getExit();
   return exitTT;
   }

bool
OMR::TreeTop::isLegalToChangeBranchDestination(TR::Compilation *comp)
   {
   TR::Node *node = self()->getNode();
   TR::ILOpCode opCode = self()->getNode()->getOpCode();

   if (opCode.isBranch())
      {
      return true;
      }
   else if (opCode.isSwitch())
      {
      return true;
      }
   else if (opCode.isJumpWithMultipleTargets() && !opCode.hasBranchChildren())
      {
      return false;
      }

   return true;
   }

bool
OMR::TreeTop::adjustBranchOrSwitchTreeTop(TR::Compilation *comp, TR::TreeTop *oldTargetTree, TR::TreeTop *newTargetTree)
   {
   TR::Node *node = self()->getNode();
   TR::ILOpCode opCode = self()->getNode()->getOpCode();
   bool flag = false;

   if (opCode.isBranch())
      {
      TR::TreeTop *branchTreeTop = self()->getNode()->getBranchDestination();

      if (branchTreeTop == oldTargetTree)
         {
         self()->getNode()->setBranchDestination(newTargetTree);
         flag = true;
         }
      }
   else if (opCode.isSwitch())
      {
      for (int i = self()->getNode()->getCaseIndexUpperBound()-1; i > 0; i--)
         {
         TR::TreeTop *branchTreeTop = self()->getNode()->getChild(i)->getBranchDestination();
         if (branchTreeTop == oldTargetTree)
            {
            flag = true;
            self()->getNode()->getChild(i)->setBranchDestination(newTargetTree);
            }
         }
      }

   else if (opCode.isJumpWithMultipleTargets())
      {
      // TR_ASSERT(false, "Attempting to adjustBranchOrSwitchTreeTop for multi-way branch (isJumpWithMultipleTargets(node) == true): %p\n", node);
      for (int32_t i = 0; i < self()->getNode()->getNumChildren() - 1 ; ++i)     // we don't care about the last monitor child
         {
         TR::TreeTop *branchTreeTop = self()->getNode()->getChild(i)->getBranchDestination();
         if(branchTreeTop == oldTargetTree)
            {
            flag = true;
            self()->getNode()->getChild(i)->setBranchDestination(newTargetTree);
            }
         }
      }

   return flag;
   }

// A naive no-aliasing-needed check to see if a treetop has any chance of killing anything
// Used by no and low opt CodeGenPrep phase passes
bool
OMR::TreeTop::isPossibleDef()
   {
   TR::Node *defNode = self()->getNode()->getOpCodeValue() == TR::treetop ? self()->getNode()->getFirstChild() : self()->getNode();
   if (defNode->getOpCode().isLikeDef())
      {
      return true;
      }
   else
      {
      return false;
      }
  }
