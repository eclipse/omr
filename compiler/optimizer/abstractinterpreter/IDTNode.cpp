/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "optimizer/abstractinterpreter/IDTNode.hpp"

#define SINGLE_CHILD_BIT 1

TR::IDTNode::IDTNode(
      int32_t idx, 
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      uint32_t byteCodeIndex, 
      float callRatio, 
      IDTNode *parent, 
      uint32_t budget) :
   _idx(idx),
   _callTarget(callTarget),
   _staticBenefit(0),
   _byteCodeIndex(byteCodeIndex),
   _symbol(symbol),
   _children(NULL),
   _parent(parent),
   _budget(budget),
   _callRatio(callRatio),
   _rootCallRatio(parent ? parent->_rootCallRatio * callRatio : 1),
   _inliningMethodSummary(NULL)
   {   
   }

TR::IDTNode* TR::IDTNode::addChild(
      int32_t idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      uint32_t byteCodeIndex, 
      float callRatio,
      TR::Region& region)
   {
   uint32_t budget =  getBudget() - callTarget->_calleeMethod->maxBytecodeIndex();
   
   TR::IDTNode* newChild = new (region) TR::IDTNode(
                                             idx, 
                                             callTarget,
                                             symbol,
                                             byteCodeIndex, 
                                             callRatio,
                                             this,
                                             budget);

   // The case where there is no children
   if (getNumChildren() == 0)
      {
      setOnlyChild(newChild);
      return newChild;
      }   

   // The case where there is 1 child
   if (getNumChildren() == 1)
      {
      TR::IDTNode* onlyChild = getOnlyChild();
      _children = new (region) TR::vector<TR::IDTNode*, TR::Region&>(region);
      TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
      _children->push_back(onlyChild);
      }
                     
   _children->push_back(newChild);
   return _children->back();
   }

uint32_t TR::IDTNode::getNumDescendants()
   {
   const uint32_t numChildren = getNumChildren();
   uint32_t sum = 0;
   for (uint32_t i =0; i < numChildren; i ++)
      {
      sum += 1 + getChild(i)->getNumDescendants(); 
      }
   return sum;
   }

uint32_t TR::IDTNode::getRecursiveCost() 
   {
   const uint32_t numChildren = getNumChildren();
   uint32_t cost = getCost();
   for (uint32_t i = 0; i < numChildren; i ++)
      {
      IDTNode *child = getChild(i);
      cost += child->getRecursiveCost();
      }
   
   return cost;
   }

uint32_t TR::IDTNode::getNumChildren()
   {
   if (_children == NULL)
      return 0;
   
   if (getOnlyChild() != NULL)
      return 1;
   
   size_t num = _children->size();
   return num;
   }

TR::IDTNode* TR::IDTNode::getChild(uint32_t index)
   {
   const uint32_t numChildren = getNumChildren();
   TR_ASSERT_FATAL(index < numChildren, "Child index out of range!\n");

   if (index == 0 && numChildren == 1) // only one child
      return getOnlyChild();

   return (*_children)[index];
   }

double TR::IDTNode::getBenefit()
   {
   return _rootCallRatio  * (1 + _staticBenefit) * 10;
   }

TR::IDTNode* TR::IDTNode::findChildWithBytecodeIndex(uint32_t bcIndex)
   {
   const uint32_t size = getNumChildren();
   
   if (size == 0)
      return NULL;
   
   if (size == 1)
      {
      TR::IDTNode* onlyChild = getOnlyChild();
      return onlyChild->getByteCodeIndex() == bcIndex ? onlyChild : NULL;
      }
   
   for (uint32_t i = 0; i < size; i ++)
      {
      if ((*_children)[i]->getByteCodeIndex() == bcIndex)
         return (*_children)[i];
      }

   return NULL;
   }

TR::IDTNode* TR::IDTNode::getOnlyChild()
   {
   if (((uintptr_t)_children) & SINGLE_CHILD_BIT)
      return (TR::IDTNode *)((uintptr_t)(_children) & ~SINGLE_CHILD_BIT);
   return NULL;
   }

void TR::IDTNode::setOnlyChild(TR::IDTNode* child)
   {
   TR_ASSERT_FATAL(!((uintptr_t)child & SINGLE_CHILD_BIT), "Maligned memory address.\n");
   _children = (TR::vector<TR::IDTNode*, TR::Region&>*)((uintptr_t)child | SINGLE_CHILD_BIT);
   }
