/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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
#include <stdlib.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/PPCInstruction.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "runtime/CodeCacheManager.hpp"

namespace TR { class PPCConditionalBranchInstruction; }
namespace TR { class PPCImmInstruction;               }
namespace TR { class Register; }

OMR::Power::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, precedingInstruction, op, node),
                  _conditions(0),
                  _asyncBranch(false)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }

OMR::Power::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, op, node),
     _conditions(0),
     _asyncBranch(false)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }

void
OMR::Power::Instruction::remove()
   {
   self()->getPrev()->setNext(self()->getNext());
   self()->getNext()->setPrev(self()->getPrev());
   }

TR::Register*
OMR::Power::Instruction::getTargetRegister(uint32_t i)
   {
   return (_conditions==NULL) ? NULL : _conditions->getTargetRegister(i, self()->cg());
   }

TR::Register*
OMR::Power::Instruction::getSourceRegister(uint32_t i)
   {
   return (_conditions==NULL) ? NULL : _conditions->getSourceRegister(i);
   }

bool
OMR::Power::Instruction::isCall()
   {
   return _opcode.isCall() || (self()->getMemoryReference() && self()->getMemoryReference()->getUnresolvedSnippet());
   }


void
OMR::Power::Instruction::PPCNeedsGCMap(uint32_t mask)
   {
   if (TR::comp()->useRegisterMaps())
      self()->setNeedsGCMap(mask);
   }

TR::Register *
OMR::Power::Instruction::getMemoryDataRegister()
   {
   return(NULL);
   }

bool
OMR::Power::Instruction::refsRegister(TR::Register * reg)
   {
   return OMR::Power::Instruction::getDependencyConditions() ? OMR::Power::Instruction::getDependencyConditions()->refsRegister(reg) : false;
   }

bool
OMR::Power::Instruction::defsRegister(TR::Register * reg)
   {
   return OMR::Power::Instruction::getDependencyConditions() ? OMR::Power::Instruction::getDependencyConditions()->defsRegister(reg) : false;
   }

bool
OMR::Power::Instruction::defsRealRegister(TR::Register * reg)
   {
   return OMR::Power::Instruction::getDependencyConditions() ? OMR::Power::Instruction::getDependencyConditions()->defsRealRegister(reg) : false;
   }

bool
OMR::Power::Instruction::usesRegister(TR::Register * reg)
   {
   return OMR::Power::Instruction::getDependencyConditions() ? OMR::Power::Instruction::getDependencyConditions()->usesRegister(reg) : false;
   }

bool
OMR::Power::Instruction::dependencyRefsRegister(TR::Register * reg)
   {
   return OMR::Power::Instruction::getDependencyConditions() ? OMR::Power::Instruction::getDependencyConditions()->refsRegister(reg) : false;
   }

TR::PPCConditionalBranchInstruction *
OMR::Power::Instruction::getPPCConditionalBranchInstruction()
   {
   return NULL;
   }

// The following safe virtual downcast method is only used in an assertion
// check within "toPPCImmInstruction"
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
TR::PPCImmInstruction *
OMR::Power::Instruction::getPPCImmInstruction()
   {
   return NULL;
   }
#endif


void
OMR::Power::Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if( OMR::Power::Instruction::getDependencyConditions() )
      {
      OMR::Power::Instruction::getDependencyConditions()->assignPostConditionRegisters(self(), kindToBeAssigned, self()->cg());
      OMR::Power::Instruction::getDependencyConditions()->assignPreConditionRegisters(self()->getPrev(), kindToBeAssigned, self()->cg());
      }
   }

bool
OMR::Power::Instruction::isBeginBlock()
   {
   return self()->getPrev()->getOpCodeValue() == TR::InstOpCode::label;
   }

bool
OMR::Power::Instruction::setsCountRegister()
   {
   if (_opcode.getOpCodeValue() == TR::InstOpCode::beql)
      return true;
   return self()->isCall()|_opcode.setsCountRegister();
   }

bool
OMR::Power::Instruction::is4ByteLoad()
   {
   return (self()->getOpCodeValue() == TR::InstOpCode::lwz);
   }

int32_t
OMR::Power::Instruction::getMachineOpCode()
   {
   return self()->getOpCodeValue();
   }

uint8_t
OMR::Power::Instruction::getBinaryLengthLowerBound()
   {
   return self()->getEstimatedBinaryLength();
   }

int32_t TR::PPCDepImmSymInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int32_t length = PPC_INSTRUCTION_LENGTH;
   setEstimatedBinaryLength(length);
   return(currentEstimate + length);
   }
// This is overrriden by J9 but we haven't made this extensible completely yet,
// and so for now we simply don't build this one.
#ifndef J9_PROJECT_SPECIFIC
uint8_t *TR::PPCDepImmSymInstruction::generateBinaryEncoding()
   {
   uint8_t   *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t   *cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   intptr_t  distance;

   if (getOpCodeValue() == TR::InstOpCode::bl || getOpCodeValue() == TR::InstOpCode::b)
      {
      if (cg()->comp()->isRecursiveMethodTarget(getSymbolReference()->getSymbol()))
         {
         uint8_t *jitTojitStart = cg()->getCodeStart();
         jitTojitStart += ((*(int32_t *)(jitTojitStart - 4)) >> 16) & 0x0000ffff;
         distance = (intptr_t)(jitTojitStart - cursor);
         }
      else
         {
         intptr_t targetAddress = getAddrImmediate();

         if (cg()->directCallRequiresTrampoline(targetAddress, (intptr_t)cursor))
            {
            int32_t refNum = getSymbolReference()->getReferenceNumber();
            if (refNum < TR_PPCnumRuntimeHelpers)
               {
               targetAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(refNum, (void *)cursor);
               }
            else
               {
               targetAddress = cg()->fe()->methodTrampolineLookup(cg()->comp(), getSymbolReference(), (void *)cursor);
               }
            }

         TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinIFormBranchRange(targetAddress, (intptr_t)cursor),
                         "Target address is out of range");

         distance = targetAddress - (intptr_t)cursor;
         }
      }
   else
      {
      // Place holder only: non-TR::InstOpCode::b[l] usage of this instruction doesn't
      // exist at this moment.
      distance = getAddrImmediate() - (intptr_t)cursor;
      }

   *(int32_t *)cursor |= distance & 0x03fffffc;

   cursor += 4;
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }
#endif
