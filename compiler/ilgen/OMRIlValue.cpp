/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#include "compile/Compilation.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/IlValue.hpp" // must follow include for compile/Compilation.hpp for TR_Memory
#include "ilgen/MethodBuilder.hpp"


OMR::IlValue::IlValue(TR::Node *node, TR::TreeTop *treeTop, TR::Block *block, TR::MethodBuilder *methodBuilder)
   : _client(0),
     _id(methodBuilder->getNextValueID()),
     _nodeThatComputesValue(node),
     _treeTopThatAnchorsValue(treeTop),
     _blockThatComputesValue(block),
     _methodBuilder(methodBuilder),
     _symRefThatCanBeUsedInOtherBlocks(0)
   {
   }

TR::DataType
OMR::IlValue::getDataType()
   {
   return _nodeThatComputesValue->getDataType();
   }

void
OMR::IlValue::storeToAuto()
   {
   if (_symRefThatCanBeUsedInOtherBlocks == NULL)
      {
      TR::Compilation *comp = TR::comp();

      // first use from another block, need to create symref and insert store tree where node  was computed
      TR::SymbolReference *symRef = comp->getSymRefTab()->createTemporary(_methodBuilder->methodSymbol(), _nodeThatComputesValue->getDataType());
      symRef->getSymbol()->setNotCollected();
      char *name = (char *) comp->trMemory()->allocateHeapMemory((2+10+1) * sizeof(char)); // 2 ("_T") + max 10 digits + trailing zero
      sprintf(name, "_T%u", symRef->getCPIndex());
      symRef->getSymbol()->getAutoSymbol()->setName(name);
      _methodBuilder->defineSymbol(name, symRef);

      // create store and its treetop
      TR::Node *storeNode = TR::Node::createStore(symRef, _nodeThatComputesValue);
      TR::TreeTop *prevTreeTop = _treeTopThatAnchorsValue->getPrevTreeTop();
      TR::TreeTop *newTree = TR::TreeTop::create(comp, storeNode);
      newTree->insertNewTreeTop(prevTreeTop, _treeTopThatAnchorsValue);

      _treeTopThatAnchorsValue->unlink(true);

      _treeTopThatAnchorsValue = newTree;
      _symRefThatCanBeUsedInOtherBlocks = symRef;
      }
   }

TR::Node *
OMR::IlValue::load(TR::Block *block)
   {
   if (_blockThatComputesValue == block)
      return _nodeThatComputesValue;

   storeToAuto();
   return TR::Node::createLoad(_symRefThatCanBeUsedInOtherBlocks);
   }

void
OMR::IlValue::storeOver(TR::IlValue *value, TR::Block *block)
   {
   if (value->_blockThatComputesValue == block && _blockThatComputesValue == block)
      {
      // probably not very common scenario, but if both values are in the same block as current
      // then storeOver just needs to update node pointer
      _nodeThatComputesValue = value->_nodeThatComputesValue;
      }
   else
      {
      // more commonly, if any value is in another block or this use will be in a different block, first ensure this value is
      // stored to an auto symbol
      // NOTE this may mean that nodes will be stored to more than one auto, but that's ok
      storeToAuto();

      // on the off chance that value was computed in this block, we can use its node pointer directly here
      TR::Node *sourceValue = value->_nodeThatComputesValue;

      // if, however, value was computed in a different block, make sure it's stored to an auto and then load it from that auto
      if (value->_blockThatComputesValue != block)
         {
         value->storeToAuto();
         sourceValue = value->load(block);
         }

      // finally store sourceValue into our own auto symbol (which we know exists because we called storeToAuto())
      TR::Node *store = TR::Node::createStore(_symRefThatCanBeUsedInOtherBlocks, sourceValue);
      TR::TreeTop *tt = TR::TreeTop::create(TR::comp(), store);
      block->append(tt);

      // any downstream use of "this" IlValue will now load the value computed by "value"
      }
   }

void *
OMR::IlValue::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::IlValue *>(this));
   return _client;
   }

ClientAllocator OMR::IlValue::_clientAllocator = NULL;
ClientAllocator OMR::IlValue::_getImpl = NULL;
