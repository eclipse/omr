/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/CodeGenerator.hpp"               // for CodeGenerator
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/Instruction.hpp"                 // for Instruction
#include "codegen/Register.hpp"                    // for Register
#include "compile/Compilation.hpp"                 // for Compilation
#include "il/TreeTop.hpp"                          // for TreeTop
#include "il/TreeTop_inlines.hpp"                  // for TreeTop::getNode
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "ras/Debug.hpp"                           // for TR_DebugBase

namespace TR { class Node; }

namespace OMR
{
Instruction::Instruction(
      TR::CodeGenerator *cg,
      TR::InstOpCode::Mnemonic op,
      TR::Node *node) :
   _opcode(op),
   _next(0),
   _prev(cg->getAppendInstruction()),
   _node(node),
   _binaryEncodingBuffer(0),
   _binaryLength(0),
   _estimatedBinaryLength(0),
   _cg(cg),
   _liveLocals(0),
   _liveMonitors(0),
   _registerSaveDescription(0),
   _gc()
   {
   TR_ASSERT(node || !debug("checkByteCodeInfo"), "TR::Node * not passed to OMR::Instruction ctor");

   TR::Compilation *comp = cg->comp();

   if (self()->getPrev())
      {
      _prev->setNext(self());
      cg->setAppendInstruction(self());
      _index = (_prev->getIndex()+INSTRUCTION_INDEX_INCREMENT) & ~FlagsMask;
      }
   else
      {
      self()->setNext(cg->getFirstInstruction());
      self()->setPrev(0);
      TR::Instruction *first = cg->getFirstInstruction();

      if (first)
         {
         first->setPrev(self());
         _index = first->getIndex() / 2;
         if (!_node)
            {
            _node = first->_node;
            }
         }
      else
         {
         _index = INSTRUCTION_INDEX_INCREMENT;
         if (!_node)
            {
            _node = comp->getStartTree()->getNode();
            }
         }

      cg->setFirstInstruction(self());

      if (cg->getAppendInstruction() == 0)
         {
         cg->setAppendInstruction(self());
         }
      }

   if (cg->getDebug())
      {
      cg->getDebug()->newInstruction(self());
      }

   }

Instruction::Instruction(
      TR::CodeGenerator *cg,
      TR::Instruction *precedingInstruction,
      TR::InstOpCode::Mnemonic op,
      TR::Node *node) :
   _opcode(op),
   _node(node),
   _binaryEncodingBuffer(0),
   _binaryLength(0),
   _estimatedBinaryLength(0),
   _cg(cg),
   _liveLocals(0),
   _liveMonitors(0),
   _registerSaveDescription(0),
   _gc()
   {

   TR::Compilation *comp = cg->comp();

   if (precedingInstruction != 0)
      {
      self()->setNext(precedingInstruction->getNext());
      self()->setPrev(precedingInstruction);

      if (precedingInstruction->getNext())
         {
         precedingInstruction->getNext()->setPrev(self());
         TIndex prevIndex = precedingInstruction->getIndex();
         TIndex nextIndex = precedingInstruction->getNext()->getIndex();
         // TODO: This will do the wrong thing if nextIndex < prevIndex, but as long as it doesn't affect the flags it won't hurt
         _index = (prevIndex + ((nextIndex - prevIndex) / 2)) & ~FlagsMask;
         }
      else
         {
         _index = (precedingInstruction->getIndex() + INSTRUCTION_INDEX_INCREMENT) & ~FlagsMask;
         cg->setAppendInstruction(self());
         }

      precedingInstruction->setNext(self());

      if (!_node)
         {
         _node = precedingInstruction->_node;
         }
      }
   else
      {
      self()->setNext(cg->getFirstInstruction());
      self()->setPrev(0);
      TR::Instruction *first = cg->getFirstInstruction();

      if (first)
         {
         first->setPrev(self());
         _index = first->getIndex() / 2;
         if (!_node)
            {
            _node = first->_node;
            }
         }
      else
         {
         _index = INSTRUCTION_INDEX_INCREMENT;
         if (!_node)
            {
            _node = comp->getStartTree()->getNode();
            }
         }

      cg->setFirstInstruction(self());

      if (cg->getAppendInstruction() == 0)
         {
         cg->setAppendInstruction(self());
         }
      }

   if (cg->getDebug())
      {
      cg->getDebug()->newInstruction(self());
      }

   }

TR::Instruction *
Instruction::move(TR::Instruction *newLocation)
   {
   TR_ASSERT(self() != newLocation, "An instruction cannot be its own predecessor");

   self()->remove();

   TR::Instruction *newLocNext = newLocation->getNext();

   if (newLocNext)
      {
      newLocNext->setPrev(self());
      }

   self()->setNext(newLocNext);
   self()->setPrev(newLocation);

   newLocation->setNext(self());

   TR::CodeGenerator *cg = self()->cg();

   // Update the append instruction if we are moving this instruction to the current append instruction
   if (cg->getAppendInstruction() == newLocation)
      {
      cg->setAppendInstruction(self());
      }

   // TODO: Updating this instruction's index might be worth while

   return self();
   }

void
Instruction::remove()
   {
   TR::Instruction *prev = self()->getPrev();
   TR::Instruction *next = self()->getNext();

   if (prev)
      {
      prev->setNext(next);
      }
   if (next)
      {
      next->setPrev(prev);
      }

   TR::CodeGenerator *cg = self()->cg();

   // Update the append instruction if we are removing the current instruction
   if (cg->getAppendInstruction() == self())
      {
      cg->setAppendInstruction(prev);
      }
   }

void
Instruction::useRegister(TR::Register *reg)
   {

   if (reg->getStartOfRange() == 0 ||
       (reg->getStartOfRange()->getIndex() > self()->getIndex()) &&
       !self()->cg()->getIsInOOLSection())
      {
      reg->setStartOfRange(self());
      }

   if (reg->getEndOfRange() == 0 ||
       (reg->getEndOfRange()->getIndex() < self()->getIndex()) &&
       !self()->cg()->getIsInOOLSection())
      {
      reg->setEndOfRange(self());
      }

   if (self()->cg()->getEnableRegisterUsageTracking())
      {
      self()->cg()->recordSingleRegisterUse(reg);
      }

   reg->incTotalUseCount();

   if (self()->cg()->getIsInOOLSection())
      reg->incOutOfLineUseCount();
   }

bool
Instruction::requiresAtomicPatching()
   {
   return !(self()->getNode() && self()->getNode()->isStopTheWorldGuard());
   }

bool
Instruction::isMergeableGuard()
   {
   static char *mergeOnlyHCRGuards = feGetEnv("TR_MergeOnlyHCRGuards");
   return mergeOnlyHCRGuards ? self()->getNode()->isStopTheWorldGuard() : self()->getNode()->isNopableInlineGuard();
   }
}
