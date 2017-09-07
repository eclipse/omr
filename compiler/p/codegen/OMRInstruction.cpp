/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include <stdint.h>                        // for uint32_t
#include <stdlib.h>                        // for NULL, atoi
#include "codegen/CodeGenerator.hpp"       // for CodeGenerator
#include "codegen/FrontEnd.hpp"            // for feGetEnv
#include "codegen/InstOpCode.hpp"          // for InstOpCode, etc
#include "codegen/Instruction.hpp"         // for Instruction, etc
#include "codegen/MemoryReference.hpp"     // for MemoryReference
#include "codegen/PPCInstruction.hpp"      // for PPCDepImmSymInstruction 
#include "codegen/RegisterConstants.hpp"   // for TR_RegisterKinds
#include "codegen/RegisterDependency.hpp"
#include "compile/Compilation.hpp"         // for Compilation, comp
#include "il/Block.hpp"                    // for Block
#include "il/ILOpCodes.hpp"                // for ILOpCodes::BBStart
#include "il/Node.hpp"                     // for Node

namespace TR { class PPCConditionalBranchInstruction; }
namespace TR { class PPCDepImmInstruction;            }
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

TR::PPCDepImmInstruction *
OMR::Power::Instruction::getPPCDepImmInstruction()
   {
   return NULL;
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
OMR::Power::Instruction::isNopCandidate()
   {
   return (self()->getNode() != NULL
        && self()->getNode()->getOpCodeValue() == TR::BBStart
        && self()->getNode()->getBlock() != NULL
        && self()->getNode()->getBlock()->firstBlockInLoop()
        && !self()->getNode()->getBlock()->isCold());
   }

int
OMR::Power::Instruction::MAX_LOOP_ALIGN_NOPS()
   {
   static int LOCAL_MAX_LOOP_ALIGN_NOPS = -1;
   if (LOCAL_MAX_LOOP_ALIGN_NOPS == -1)
      {
      static char *TR_LoopAlignNops = feGetEnv("TR_LoopAlignNops");
      LOCAL_MAX_LOOP_ALIGN_NOPS = (TR_LoopAlignNops == NULL ? 7 : atoi(TR_LoopAlignNops));
      }
   return LOCAL_MAX_LOOP_ALIGN_NOPS;
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
   intptrj_t  imm = getAddrImmediate();
   intptrj_t  distance = imm - (intptrj_t)cursor;

   if (getOpCodeValue() == TR::InstOpCode::bl || getOpCodeValue() == TR::InstOpCode::b)
      {
      TR::ResolvedMethodSymbol *sym = getSymbolReference()->getSymbol()->getResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = sym == NULL ? NULL : sym->getResolvedMethod();

      if (resolvedMethod != NULL && resolvedMethod->isSameMethod(cg()->comp()->getCurrentMethod()))
         {
         uint8_t *jitTojitStart = cg()->getCodeStart();
         jitTojitStart += ((*(int32_t *)(jitTojitStart - 4)) >> 16) & 0x0000ffff;
         *(int32_t *)cursor |= (jitTojitStart - cursor) & 0x03fffffc;
         }
      else
         {
         if (distance > BRANCH_FORWARD_LIMIT ||
             distance < BRANCH_BACKWARD_LIMIT)
            {
            int32_t refNum = getSymbolReference()->getReferenceNumber();
            if (refNum < TR_PPCnumRuntimeHelpers)
               {
               distance = cg()->fe()->indexedTrampolineLookup(refNum, (void *)cursor) - (intptrj_t)cursor;
               }
            }

         TR_ASSERT(distance <= BRANCH_FORWARD_LIMIT && distance >= BRANCH_BACKWARD_LIMIT,
                    "CodeCache is more than 32MB");
         *(int32_t *)cursor |= distance & 0x03fffffc;
         }
      }
   else
      {
      // Place holder only: non-TR::InstOpCode::b[l] usage of this instruction doesn't
      // exist at this moment.
      *(int32_t *)cursor |= distance & 0x03fffffc;
      }

   cursor += 4;
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }
#endif
