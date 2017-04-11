/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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
   _prev(TR::comp()->getAppendInstruction()),
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
      comp->setAppendInstruction(self());
      _index = (_prev->getIndex()+INSTRUCTION_INDEX_INCREMENT) & ~FlagsMask;
      }
   else
      {
      self()->setNext(comp->getFirstInstruction());
      self()->setPrev(0);
      TR::Instruction *first = comp->getFirstInstruction();

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

      comp->setFirstInstruction(self());

      if (comp->getAppendInstruction() == 0)
         {
         comp->setAppendInstruction(self());
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
         comp->setAppendInstruction(self());
         }

      precedingInstruction->setNext(self());

      if (!_node)
         {
         _node = precedingInstruction->_node;
         }
      }
   else
      {
      self()->setNext(comp->getFirstInstruction());
      self()->setPrev(0);
      TR::Instruction *first = comp->getFirstInstruction();

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

      comp->setFirstInstruction(self());

      if (comp->getAppendInstruction() == 0)
         {
         comp->setAppendInstruction(self());
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

   TR::Compilation *comp = self()->cg()->comp();

   // Update the append instruction if we are moving this instruction to the current append instruction
   if (comp->getAppendInstruction() == newLocation)
      {
      comp->setAppendInstruction(self());
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

   TR::Compilation *comp = self()->cg()->comp();

   // Update the append instruction if we are removing the current instruction
   if (comp->getAppendInstruction() == self())
      {
      comp->setAppendInstruction(prev);
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
}
