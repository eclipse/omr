/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#include "optimizer/abstractinterpreter/InliningProposal.hpp"
#include "infra/deque.hpp"
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "infra/BitVector.hpp"
#include "env/VerboseLog.hpp"
#include "infra/String.hpp"

TR::InliningProposal::InliningProposal(TR::Region& region, TR::IDT *idt):
   _region(region),
   _nodes(NULL), // Lazy Initialization of BitVector
   _cost(0),
   _benefit(0),
   _idt(idt),
   _frozen(false)
   {
   }

TR::InliningProposal::InliningProposal(const InliningProposal &proposal, TR::Region& region):
   _region(region),
   _cost(proposal._cost),
   _benefit(proposal._benefit),
   _idt(proposal._idt),
   _frozen(false)
   {
   if (proposal._nodes)
      {
      _nodes = new (region) TR_BitVector(proposal._nodes->getHighestBitPosition(), region);
      *_nodes = *proposal._nodes;
      }
   else
      _nodes = NULL;
   }

void TR::InliningProposal::print(TR::Compilation* comp)
   {
   bool traceBIProposal = comp->getOption(TR_TraceBIProposal);
   bool verboseInlining = comp->getOptions()->getVerboseOption(TR_VerboseInlining);

   if (!traceBIProposal && !verboseInlining) //no need to run the following code if neither flag is set
      return;

   TR_VerboseLog::CriticalSection vlogLock(verboseInlining);

   if (!_nodes)
      {
      if (traceBIProposal)
         traceMsg(comp, "Inlining Proposal is NULL\n");
      if (verboseInlining)
         TR_VerboseLog::writeLine(TR_Vlog_BI, "%s", "Inlining Proposal is NULL\n");
      return;
      }

   const uint32_t numMethodsToInline =  _nodes->elementCount()-1;

   TR_ASSERT_FATAL(_idt, "Must have an IDT");

   TR::StringBuf line(comp->trMemory()->currentStackRegion());
   line.appendf("#Proposal: %d methods inlined into %s, cost: %d", numMethodsToInline, _idt->getRoot()->getName(comp->trMemory()), getCost());

   if (traceBIProposal)
      traceMsg(comp, "%s\n", line.text());
   if (verboseInlining)
      TR_VerboseLog::writeLine(TR_Vlog_BI, "%s", line.text());

   TR::deque<TR::IDTNode*, TR::Region&> idtNodeQueue(comp->trMemory()->currentStackRegion());
   idtNodeQueue.push_back(_idt->getRoot());

   //BFS
   while (!idtNodeQueue.empty())
      {
      TR::IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();
      const int32_t index = currentNode->getGlobalIndex();

      if (index != -1) //do not print the root node
         {
         line.clear();
         line.appendf("#Proposal: #%d : #%d %s @%d -> bcsz=%d target %s, benefit = %f, cost = %d, budget = %d",
            currentNode->getGlobalIndex(),
            currentNode->getParentGlobalIndex(),
            _nodes->isSet(index + 1) ? "INLINED" : "NOT inlined",
            currentNode->getByteCodeIndex(),
            currentNode->getByteCodeSize(),
            currentNode->getName(comp->trMemory()),
            currentNode->getBenefit(),
            currentNode->getCost(),
            currentNode->getBudget()
         );

         if (traceBIProposal)
            traceMsg(comp, "%s\n", line.text());
         if (verboseInlining)
            TR_VerboseLog::writeLine(TR_Vlog_BI, "%s", line.text());
         }

      //process children
      const uint32_t numChildren = currentNode->getNumChildren();

      for (uint32_t i = 0; i < numChildren; i++)
         idtNodeQueue.push_back(currentNode->getChild(i));

      }
   if (traceBIProposal)
      traceMsg(comp, "\n");
   }


void TR::InliningProposal::addNode(TR::IDTNode *node)
   {
   TR_ASSERT_FATAL(!_frozen, "TR::InliningProposal::addNode proposal is frozen, cannot be mutated");
   ensureBitVectorInitialized();

   const int32_t index = node->getGlobalIndex() + 1;
   if (_nodes->isSet(index))
      {
      return;
      }

   _nodes->set(index);

   _cost = 0;
   _benefit = 0;
   }

bool TR::InliningProposal::isEmpty()
   {
   if (!_nodes)
      return true;

   return _nodes->isEmpty();
   }

uint32_t TR::InliningProposal::getCost()
   {
   if (_cost == 0)
      {
      computeCostAndBenefit();
      }

   return _cost;
   }

double TR::InliningProposal::getBenefit()
   {
   if (_benefit == 0)
      {
      computeCostAndBenefit();
      }

   return _benefit;
   }

void TR::InliningProposal::computeCostAndBenefit()
   {
   _cost = 0;
   _benefit = 0;

   if (!_idt)
      return;

   TR_BitVectorIterator bvi(*_nodes);
   int32_t idtNodeIndex;

   while (bvi.hasMoreElements())
      {
      idtNodeIndex = bvi.getNextElement();
      IDTNode *node = _idt->getNodeByGlobalIndex(idtNodeIndex - 1);
      _cost += node->getCost();
      _benefit += node->getBenefit();
      }
   }

void TR::InliningProposal::ensureBitVectorInitialized()
   {
   TR_ASSERT_FATAL(!_frozen, "TR::InliningProposal::ensureBitVectorInitialized proposal is frozen, cannot be mutated");
   if (!_nodes)
      _nodes = new (_region) TR_BitVector(_region);
   }

bool TR::InliningProposal::isNodeInProposal(TR::IDTNode* node)
   {
   if (_nodes == NULL)
      return false;
   if (_nodes->isEmpty())
      return false;

   const int32_t idx = node->getGlobalIndex() + 1;

   return _nodes->isSet(idx);
   }

void TR::InliningProposal::merge(TR::InliningProposal *a, TR::InliningProposal *b)
   {
   TR_ASSERT_FATAL(!_frozen, "TR::InliningProposal::merge proposal is frozen, cannot be mutated");
   if (a->_nodes && b->_nodes)
      {
      TR_BitVector temp(*a->_nodes);
      temp |= *b->_nodes;
      ensureBitVectorInitialized();
      *_nodes = temp;
      }
   else if (a->_nodes)
      {
      ensureBitVectorInitialized();
      *_nodes = *a->_nodes;
      }
   else if (b->_nodes)
      {
      ensureBitVectorInitialized();
      *_nodes = *b->_nodes;
      }
   else if (_nodes) // this need to be cleared out if both a->_nodes and b->_nodes are null
      {
      _nodes->empty();
      }

   _cost = 0;
   _benefit = 0;
   }

bool TR::InliningProposal::intersects(TR::InliningProposal* other)
   {
   if (!_nodes || !other->_nodes)
      return false;

   return _nodes->intersects(*other->_nodes);
   }


TR::InliningProposalTable::InliningProposalTable(uint32_t rows, uint32_t cols, TR::Region& region) :
      _rows(rows),
      _cols(cols),
      _region(region)
   {
   _table = new (region) InliningProposal**[rows];

   for (uint32_t i = 0; i < rows; i ++)
      {
      _table[i] = new (region) InliningProposal*[cols]();
      }

   _emptyProposal = new (region) TR::InliningProposal(region, NULL);
   _emptyProposal->setFrozen();
   }

TR::InliningProposal* TR::InliningProposalTable::get(uint32_t row, uint32_t col)
   {
   TR_ASSERT_FATAL(row < _rows, "TR::InliningProposalTable::get Invalid row index");
   TR_ASSERT_FATAL(col < _cols, "TR::InliningProposalTable::get Invalid col index");
   return _table[row][col] ? _table[row][col] : getEmptyProposal();
   }

TR::InliningProposal* TR::InliningProposalTable::getByOffset(uint32_t row, uint32_t rowOffset, uint32_t col, uint32_t colOffset)
   {
   InliningProposal* proposal = NULL;

   // inlinerPacking() may request negative indices
   if (rowOffset > row || colOffset > col)
      proposal = getEmptyProposal();
   else
      proposal = this->get(row - rowOffset, col - colOffset);

   return proposal;
   }

void TR::InliningProposalTable::set(uint32_t row, uint32_t col, TR::InliningProposal* proposal)
   {
   TR_ASSERT_FATAL(proposal, "TR::InliningProposalTable::set proposal is NULL");
   TR_ASSERT_FATAL(row < _rows, "TR::InliningProposalTable::set Invalid row index" );
   TR_ASSERT_FATAL(col < _cols, "TR::InliningProposalTable::set Invalid col index" );

   _table[row][col] = proposal;
   proposal->setFrozen();
   }
