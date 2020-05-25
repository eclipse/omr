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

#include "env/ExceptionTable.hpp"

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"

class TR_ResolvedMethod;

TR_ExceptionTableEntryIterator::TR_ExceptionTableEntryIterator(TR::Compilation *comp)
   : _compilation(comp)
   {
   int32_t i = comp->getMaxInlineDepth() + 1;
   _tableEntries = (TR_Array<List<TR_ExceptionTableEntry> > *)comp->trMemory()->allocateHeapMemory(sizeof(TR_Array<List<TR_ExceptionTableEntry> >)*i);
   for (int32_t j = 0; j < i; ++j)
      _tableEntries[j].init(comp->trMemory());

   // walk the trees looking for catch blocks
   //
   TR::Block * catchBlock;
   TR::TreeTop * tt = comp->getStartTree();
   for (; tt; tt = catchBlock->getExit()->getNextTreeTop())
      {
      catchBlock = tt->getNode()->getBlock();

      // Skip OSR blocks
      if (catchBlock->isOSRCodeBlock() || catchBlock->isOSRCatchBlock())
         continue;

      if (!catchBlock->getExceptionPredecessors().empty())
         {
         List<TR_ExceptionTableEntry> & tableEntries =
            _tableEntries[catchBlock->getInlineDepth()][catchBlock->getHandlerIndex()];

         tableEntries.setRegion(comp->trMemory()->heapMemoryRegion());

         uint32_t catchType = catchBlock->getCatchType();
         TR_ResolvedMethod * method = catchBlock->getOwningMethod();

         // create exception ranges from the list of exception predecessors
         //
         TR::CFGEdgeList & exceptionPredecessors = catchBlock->getExceptionPredecessors();
         while (!exceptionPredecessors.empty())
            {
            TR::CFGEdge * e = exceptionPredecessors.front();
            exceptionPredecessors.pop_front();
            TR::Block * first, * last;
            first = last = toBlock(e->getFrom());
            addSnippetRanges(tableEntries, first, catchBlock, catchType, method, comp);

            // extend the range backwards
            //
            for (;;)
               {
               TR::TreeTop * tt = first->getEntry()->getPrevTreeTop();
               if (!tt) break;
               bool found = false;
               TR_ASSERT(tt->getNode(), "a treetop doesn't have a node ????");
               TR::Block * prevBlock = tt->getNode()->getBlock();
               for (auto previousEdge = exceptionPredecessors.before_begin(), currentEdge = exceptionPredecessors.begin();
                    currentEdge != exceptionPredecessors.end();
                    ++previousEdge, ++currentEdge)
                  {
                  if (toBlock((*currentEdge)->getFrom()) == prevBlock)
                     {
                     currentEdge = exceptionPredecessors.erase_after(previousEdge);
                     found = true;
                     break;
                     }
                  }
               if (!found) break;
               first = prevBlock;
               addSnippetRanges(tableEntries, prevBlock, catchBlock, catchType, method, comp);
               }

            // extend the range forwards
            //
            for (;;)
               {
               TR::TreeTop * tt = last->getExit()->getNextTreeTop();
               if (!tt) break;
               bool found = false;
               TR::Block * nextBlock = tt->getNode()->getBlock();
               for (auto previousEdge = exceptionPredecessors.before_begin(), currentEdge = exceptionPredecessors.begin();
                    currentEdge != exceptionPredecessors.end();
                    ++previousEdge, ++currentEdge)
                  {
                  if (toBlock((*currentEdge)->getFrom()) == nextBlock)
                     {
                     exceptionPredecessors.erase_after(previousEdge);
                     found = true;
                     break;
                     }
                  }
               if (!found) break;
               last = nextBlock;
               addSnippetRanges(tableEntries, nextBlock, catchBlock, catchType, method, comp);
               }

            TR_ExceptionTableEntry * ete = new (comp->trHeapMemory()) TR_ExceptionTableEntry;
            ete->_instructionStartPC = first->getInstructionBoundaries()._startPC;
            ete->_instructionEndPC = last->getInstructionBoundaries()._endPC;
            ete->_instructionHandlerPC = catchBlock->getInstructionBoundaries()._startPC;
            ete->_catchType = catchType;
            ete->_isSyntheticHandler = catchBlock->isSyntheticHandler();
            ete->_method = method;

            ete->_byteCodeInfo = catchBlock->getByteCodeInfo();
            tableEntries.add(ete);
            }
         }
      }
   }

void
TR_ExceptionTableEntryIterator::addSnippetRanges(
   List<TR_ExceptionTableEntry> & tableEntries, TR::Block * snippetBlock, TR::Block * catchBlock, uint32_t catchType,
   TR_ResolvedMethod * method, TR::Compilation *comp)
   {
   TR::Block::InstructionBoundaries * ib = snippetBlock->getFirstSnippetBoundaries();
   for (; ib; ib = ib->getNext())
      {
      TR_ExceptionTableEntry * ete = new (comp->trHeapMemory()) TR_ExceptionTableEntry;
      ete->_instructionStartPC = ib->_startPC;
      ete->_instructionEndPC = ib->_endPC;
      ete->_instructionHandlerPC = catchBlock->getInstructionBoundaries()._startPC;
      ete->_catchType = catchType;
      ete->_isSyntheticHandler = catchBlock->isSyntheticHandler();
      ete->_method = method;
      ete->_byteCodeInfo = catchBlock->getByteCodeInfo();
      tableEntries.add(ete);
      }
   }

TR_ExceptionTableEntry *
TR_ExceptionTableEntryIterator::getFirst()
   {
   _inlineDepth = _compilation->getMaxInlineDepth();
   if (_inlineDepth >= 0)
      {
      _handlerIndex = 0;
      _entryIterator.set(&_tableEntries[_inlineDepth][_handlerIndex]);
      }
   return getCurrent();
   }

TR_ExceptionTableEntry *
TR_ExceptionTableEntryIterator::getNext()
   {
   _entryIterator.getNext();
   return getCurrent();
   }

TR_ExceptionTableEntry *
TR_ExceptionTableEntryIterator::getCurrent()
   {
   if (_inlineDepth < 0) return 0;

   while (_entryIterator.getCurrent() == 0)
      {
      if (++_handlerIndex >= _tableEntries[_inlineDepth].size())
         {
         if (--_inlineDepth < 0) return 0;
         _handlerIndex = 0;
         }
      _entryIterator.set(&_tableEntries[_inlineDepth][_handlerIndex]);
      }
   return _entryIterator.getCurrent();
   }

uint32_t
TR_ExceptionTableEntryIterator::size()
   {
   uint32_t i = 0;
   for (TR_ExceptionTableEntry * e = getFirst(); e; e = getNext())
      ++i;
   return i;
   }
