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

#include <stdint.h>
#include <string.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "infra/deque.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/Stack.hpp"
#include "optimizer/Optimizations.hpp"
#include "env/IO.hpp"

namespace TR { class ResolvedMethodSymbol; }
namespace TR { class TreeTop; }

template<typename ByteCode,
         ByteCode BCunknown,
         class ByteCodeIterator,
         typename OperandStackElementType> class TR_ByteCodeIteratorWithState : public ByteCodeIterator
   {
   public:
   typedef TR_Stack<OperandStackElementType> ByteCodeStack;

   TR_ByteCodeIteratorWithState(TR::ResolvedMethodSymbol *methodSym, TR::Compilation *comp)
      : ByteCodeIterator(methodSym, comp),
        _unimplementedOpcode(0),
        _inExceptionHandler(false),
        _stack(NULL),
        _backwardBranches(),
        _todoQueue(),
        _stackTemps(comp->trMemory(), 20),
        _tryCatchInfo(comp->allocator("IlGen"))
      {
      _printByteCodes = (comp->getOutFile() != NULL && comp->getOption(TR_TraceBC) && (comp->isOutermostMethod() || comp->getOption(TR_DebugInliner) || comp->trace(OMR::inlining)));
      _cannotAttemptOSR = comp->getOption(TR_EnableOSR) && !comp->isPeekingMethod() && methodSym->cannotAttemptOSRDuring(comp->getCurrentInlinedSiteIndex(), comp);
      }

   void printByteCodes()
      {
      this->printByteCodePrologue();
      for (ByteCode bc = this->first(); bc != BCunknown; bc = this->next())
         this->printByteCode();
      this->printByteCodeEpilogue();
      }

protected:

   enum ByteCodeWithStateFlags
      {
      inExceptionRange    = 0x01,
      generated           = 0x02
      };

   /// used to create a worklist of bytecode indices to traverse
   struct TodoIndex : TR_Link<TodoIndex>
      {
      TodoIndex(int32_t i) : _index(i) { }
      int32_t _index;
      };

   /// used to store a list of edges between bytecode indices
   struct IndexPair : TR_Link<IndexPair>
      {
      IndexPair(int32_t f, int32_t t) : _fromIndex(f), _toIndex(t) { }
      int32_t _fromIndex, _toIndex;
      };

   void initialize()
      {
      uint32_t size = this->maxByteCodeIndex() + 5;
      _flags  = (flags8_t *) this->trMemory()->allocateStackMemory(size * sizeof(flags8_t));
      _blocks = (TR::Block * *) this->trMemory()->allocateStackMemory(size * sizeof(TR::Block *));
      _stacks = (ByteCodeStack * *) this->trMemory()->allocateStackMemory(size * sizeof(ByteCodeStack *));
      memset(_flags, 0, size * sizeof(flags8_t));
      memset(_blocks, 0, size * sizeof(TR::Block *));
      memset(_stacks, 0, size * sizeof(ByteCodeStack *));

      findAndMarkBranchTargets();
      findAndMarkExceptionRanges();

      genBBStart(0);
      setupBBStartContext(0);
      this->setIndex(0);
      }

   /// called for each control flow edge from bytecode at fromIndex, destination bytecode is fromIndex+relativeBranch
   ///   if it's a backward branch (relativeBranch < 0) then the edge is added to the _backwardBranches list
   ///   and some flags are updated on the method symbol to indicate it may have loops or, if appropriate, nested loops
   ///   finally, genBBStart is called for the destination bytecode to register that it is the start of a basic block
   void markTarget(int32_t fromIndex, int32_t relativeBranch)
      {
      int32_t toIndex = fromIndex + relativeBranch;
      if (relativeBranch < 0)
         {
         this->_methodSymbol->setMayHaveLoops(true);

         IndexPair * ip, * prev = 0, * newIP = new (this->comp()->trStackMemory()) IndexPair(fromIndex, toIndex);
         for (ip = _backwardBranches.getFirst(); ip; prev = ip, ip = ip->getNext())
            {
            TR_ASSERT(fromIndex >= ip->_fromIndex, "markTarget: fromIndex's aren't increasing?");
            if (ip->_toIndex < toIndex || fromIndex == ip->_fromIndex)
               break;
            if (debug("onlyConsiderProperlyNestedBackwardBranches") && ip->_toIndex == toIndex)
               break;
            this->_methodSymbol->setMayHaveNestedLoops(true);
            }

         newIP->setNext(ip);
         if (prev)
            prev->setNext(newIP);
         else
            _backwardBranches.setFirst(newIP);
         }
      genBBStart(toIndex);
      }

   /// called to identify the branches and their targets in the method
   ///  causes the _blocks array to be filled in with the basic blocks of the method
   void findAndMarkBranchTargets()
      {
      TR::Compilation *comp = this->comp();
      if (debug("branchTargets"))
         diagnostic("findAndMarkBranchTargets for %s\n", comp->signature());

      aboutToFindBranchTargets();

      for (ByteCode bc = this->first(); bc != BCunknown; bc = this->next())
         {
         if (_printByteCodes)
            this->printByteCode();

         int32_t i = this->bcIndex();
         if (this->isBranch())
            markTarget(i, this->branchDestination(i) - i);
         markAnySpecialBranchTargets(bc);
         }
      finishedFindingBranchTargets();
      }

   /// any work that should be done before starting the search for branches and targets
   virtual void aboutToFindBranchTargets()
      {
      if (_printByteCodes)
         this->printByteCodePrologue();
      }
   /// analyze any bytecodes that don't look like branches but represent control flows and suggest basic block beginnings
   virtual void markAnySpecialBranchTargets(ByteCode bc) { }

   /// any work that should be done after all the branches and their targets have been found
   virtual void finishedFindingBranchTargets()
      {
      if (_printByteCodes)
         this->printByteCodeEpilogue();
      }


   /// update state when an exception range is identified
   ///  create all the appropriate basic blocks
   ///  initialize the summary list of try catch regions
   ///  mark bytecodes in the range as having a handler (inExceptionRange)
   void markExceptionRange(int32_t index, int32_t start, int32_t end, int32_t handler, uint32_t type)
      {
      TR::Compilation *comp = this->comp();

      if (_printByteCodes)
         trfprintf(this->comp()->getOutFile(), "ExceptionRange: start [%8x] end [%8x] handler [%8x] type [%8x] \n", start, end, handler, type);

      genBBStart(start);
      genBBStart(end + 1);
      genBBStart(handler);

      TR_ASSERT(_tryCatchInfo.begin() + index == _tryCatchInfo.end(), "Unexpected insertion order");
      _tryCatchInfo.insert(_tryCatchInfo.begin() + index, typename ByteCodeIterator::TryCatchInfo(start, end, handler, type));

      for (int32_t j = start; j <= end; ++j)
         setIsInExceptionRange(j);
      }

   /// language specifies how exception ranges are discovered
   ///  some languages (like Java) encode exception ranges as meta-data alongside the bytecodes
   ///  other languages need to discover exception ranges because they aren't specified explicitly by the bytecodes
   virtual void findAndMarkExceptionRanges() = 0;

   /// called when a bytecode index is the target of a branch so it needs to be the start of a basic block
   ///  determines to which basic block the nodes for a translated bytecode will go
   void genBBStart(int32_t index)
      {
      if (!blocks(index))
         {
         blocks(index) = TR::Block::createEmptyBlock(this->comp());
         blocks(index)->setByteCodeIndex(index, this->comp());
         }
      }

   /// saveStack preserves the state of the operand stack at the end of a basic block or as needed within a block
   ///  operand stack slots are saved into locals; other blocks can then access the state of the operand stack by loading those locals
   virtual void saveStack(int32_t) = 0;

   /// called when bytecode index "index" has been identified as the target of a branch, so needs its own basic block
   ///  if queueTarget is true, target index added to the _todoQueue to make sure it is translated
   ///  genBBStart to create the basic block structure
   ///  saveStack to make sure the state of the operand stack is accessible at the beginning of the basic block associated with target
   ///  returns the TR::TreeTop to which new treetops in the target block should be appended
   TR::TreeTop * genTarget(int32_t index, bool queueTarget = true)
      {
      if (queueTarget)
         _todoQueue.append(new (this->comp()->trStackMemory()) TodoIndex(index));
      genBBStart(index);
      saveStack(index);
      return blocks(index)->getEntry();
      }

   /// called at end of a basic block with _bcIndex set to the next target
   ///  returns with the next byte code index to translate at, which may not match the target provided
   ///    if that target block has already been generated
   int32_t genBBEndAndBBStart()
      {
      genTarget(this->_bcIndex);
      return findNextByteCodeToGen();
      }

   /// initializes the operand stack state for the given bytecode index, typically called at the beginning of a basic block
   ///   returns the bytecode index provided.
   virtual int32_t setupBBStartContext(int32_t index)
      {
      if (_stacks[index] != NULL)
         {
         *_stack = *_stacks[index];
         _stackTemps = *_stacks[index];
         }
      else
         {
         if (_stack)
            _stack->clear();
         _stackTemps.clear();
         }

      _block = blocks(index);
      return index;
      }

   /// grabs the next basic block start from _todoQueue that has not been generated yet
   ///   once found, setupBBStartContext is called to ensure everything is ready to start translating bytecodes at that index
   /// if nothing left to translate, returns _maxByteCode + 8 as a number bigger than the exit test for the simulation loop
   int32_t findNextByteCodeToGen()
      {
      // find and setup the next bbStart context
      //
      TodoIndex * ti;
      while (ti = _todoQueue.pop())
         {
         if (!isGenerated(ti->_index))
            return setupBBStartContext(ti->_index);
         }

      return this->maxByteCodeIndex() + 8; // indicate that we're done
      }

   /// returns true if the bytecode index i is part of an exception range (try block)
   bool isInExceptionRange(int32_t i)
      {
      return _flags[i].testAny(inExceptionRange);
      }

   /// remember that the bytecode index i is part of an exception range (try block)
   void setIsInExceptionRange(int32_t i)
      {
      _flags[i].set(inExceptionRange);
      }

   /// returns true if the bytecode at index i has already been generated (translated to IR)
   bool isGenerated(int32_t i)
      {
      return _flags[i].testAny(generated);
      }

   /// remember that the bytecode at index i has already been generated (translated to IR)
   void setIsGenerated(int32_t i)
      {
      _flags[i].set(generated);
      }

   /// return the basic block that starts at byte code index i
   TR::Block * &blocks(int32_t i)
      {
      return _blocks[i];
      }

   /// return the i'th element in the operand stack (numbered from the bottom of the operand stack, not the top)
   OperandStackElementType& node(uint32_t i)
      {
      return _stack->element(i);
      }

   /// return the top element on the operand stack
   OperandStackElementType& top()
      {
      return topn(0);
      }

   /// return the top n-th element on the operand stack
   OperandStackElementType& topn(uint32_t n)
      {
      return node(_stack->topIndex() - n);
      }

   /// return the top element of the operand stack and pop it off the operand stack
   OperandStackElementType pop()
      {
      return _stack->pop();
      }

   /// pop n elements from the top of the stack
   void popn(uint32_t n)
      {
      _stack->setSize(_stack->size() - n);
      }

   /// push the element onto the operand stack
   void push(OperandStackElementType n)
      {
      _stack->push(n);
      }

   /// swap the top two elements of the operand stack
   void swap()
      {
      _stack->swap();
      }

   /// sets the n'th element from the top to the new element e
   void setn(int32_t n, OperandStackElementType e)
      {
      topn(n) = e;
      }

   /// duplicate the top of the operand stack
   void dup()
      {
      dupn(1);
      }

   /// duplicate the top n elements of the operand stack
   void dupn(int32_t n)
      {
      shift(n, n);
      }

   /// generic helper routine for shifting duplicates of existing operand stack elements to the top
   ///   first grow the stack by copyCount then shift part of the operand stack (from top) up to the new top
   ///   NOTE: if copyCount > shiftCount then (copyCount - shiftCount) elements above the top of the original operand stack will not be set
   void shift(int32_t shiftCount, int32_t copyCount)
      {
      _stack->setSize(_stack->size() + copyCount);
      for (int32_t i = 0; i < shiftCount; ++i)
         node(_stack->topIndex() - i) = node(_stack->topIndex() - i - copyCount);
      }

   /// generic helper routine for shifting and then copying elements of the operand stack
   ///   first shifts operand stack by shiftCount, then copies the shifted elements underneath the shifted operand stack values
   void shiftAndCopy(int32_t shiftCount, int32_t copyCount)
      {
      shift(shiftCount, copyCount);
      for (int32_t i = 0; i < copyCount; ++i)
         node(_stack->topIndex() - i - shiftCount) = node(_stack->topIndex() - i);
      }

   /// rotates "countArg" elements off the top to the bottom of the operand stack
   ///    Expected: -operand stack size <= countArg <= operand stack size
   ///
   ///    \warning Make sure you know what you're doing regarding pending push
   ///             temps or you'll mess up FSD because of this rotation
   void rotate(int32_t countArg)
      {
      uint32_t count;
      if (countArg < 0)
         count = (uint32_t) (_stack->size() + countArg);
      else
         count = (uint32_t) countArg;

      // Duplicate the entries that will wrap around to the cold end of the operand stack
      uint32_t oldSize = _stack->size();
      shift(oldSize, count);

      // Move them to the cold end and then discard the duplicates
      //
      for (uint32_t i = 0; i < count; ++i)
         node(i) = node(oldSize + i);

      _stack->setSize(oldSize);
      }


   ByteCodeStack *                                _stack; ///< current operand stack
   ByteCodeStack                                  _stackTemps;
   ByteCodeStack * *                              _stacks;
   TR::Block *                                    _block; ///< current block
   TR::Block * *                                  _blocks;
   TR_LinkHeadAndTail<TodoIndex>                  _todoQueue;
   TR_LinkHead<IndexPair>                         _backwardBranches;
   bool                                           _inExceptionHandler;
   bool                                           _printByteCodes;
   bool                                           _cannotAttemptOSR;

   flags8_t *                                     _flags;

   uint8_t                                        _unimplementedOpcode;  ///< (255 if opcode unknown)
   TR::deque<class ByteCodeIterator::TryCatchInfo> _tryCatchInfo;
   };
