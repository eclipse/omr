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
#include <stdio.h>
#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "runtime/Runtime.hpp"

class TR_OpaqueMethodBlock;
namespace TR { class Register; }

static bool reversedConditionalBranchOpCode(TR::InstOpCode::Mnemonic op, TR::InstOpCode::Mnemonic *rop);

uint8_t *
OMR::Power::Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = self()->cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   if (self()->getOpCodeValue() == TR::InstOpCode::depend || self()->getOpCodeValue() == TR::InstOpCode::assocreg)
      {
      }
   else
      {
      cursor = self()->getOpCode().copyBinaryToBuffer(instructionStart);
      if (self()->getOpCodeValue() == TR::InstOpCode::genop)
         {
         TR::RealRegister::RegNum nopreg = TR::Compiler->target.cpu.id() > TR_PPCp6 ? TR::RealRegister::gr2 : TR::RealRegister::gr1;
         TR::RealRegister *r = self()->cg()->machine()->getRealRegister(nopreg);
         r->setRegisterFieldRS((uint32_t *) cursor);
         r->setRegisterFieldRA((uint32_t *) cursor);
         }
       cursor += PPC_INSTRUCTION_LENGTH;
      }
   self()->setBinaryLength(cursor - instructionStart);
   self()->setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t
OMR::Power::Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   if (self()->getOpCodeValue() == TR::InstOpCode::depend || self()->getOpCodeValue() == TR::InstOpCode::assocreg)
      {
      self()->setEstimatedBinaryLength(0);
      return currentEstimate;
      }
   else
      {
      self()->setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH);
      return currentEstimate + self()->getEstimatedBinaryLength();
      }
   }

uint8_t *TR::PPCLabelInstruction::generateBinaryEncoding()
   {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol *label            = getLabelSymbol();
   uint8_t        *cursor           = instructionStart;

   if (getOpCode().isBranchOp())
      {
          cursor = getOpCode().copyBinaryToBuffer(instructionStart);
          if (label->getCodeLocation() != NULL)
             {
             *(int32_t *)cursor |= ((label->getCodeLocation()-cursor) &
                                    0x03fffffc);
             }
          else
             {
             cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, label));
             }
          cursor += 4;
      }
   else // regular LABEL
      {
      label->setCodeLocation(instructionStart);
      }
   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::PPCLabelInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int8_t loopAlignBytes = 0;
   static int count=0;

   if (isNopCandidate())
      {
	loopAlignBytes = MAX_LOOP_ALIGN_NOPS()*PPC_INSTRUCTION_LENGTH;
      }

   if (getOpCode().isBranchOp())
      {
      setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH);
      }
   else
      {
      setEstimatedBinaryLength(0);
      getLabelSymbol()->setEstimatedCodeLocation(currentEstimate + loopAlignBytes);
      }
   return currentEstimate + getEstimatedBinaryLength() + loopAlignBytes;
   }
static bool reversedConditionalBranchOpCode(TR::InstOpCode::Mnemonic op, TR::InstOpCode::Mnemonic *rop)
   {
   switch (op)
      {
      case TR::InstOpCode::bdnz:
         *rop = TR::InstOpCode::bdz;
         return(false);
      case TR::InstOpCode::bdz:
         *rop = TR::InstOpCode::bdnz;
         return(false);
      case TR::InstOpCode::beq:
         *rop = TR::InstOpCode::bne;
         return(false);
      case TR::InstOpCode::beql:
         *rop = TR::InstOpCode::bne;
         return(true);
      case TR::InstOpCode::bge:
         *rop = TR::InstOpCode::blt;
         return(false);
      case TR::InstOpCode::bgel:
         *rop = TR::InstOpCode::blt;
         return(true);
      case TR::InstOpCode::bgt:
         *rop = TR::InstOpCode::ble;
         return(false);
      case TR::InstOpCode::bgtl:
         *rop = TR::InstOpCode::ble;
         return(true);
      case TR::InstOpCode::ble:
         *rop = TR::InstOpCode::bgt;
         return(false);
      case TR::InstOpCode::blel:
         *rop = TR::InstOpCode::bgt;
         return(true);
      case TR::InstOpCode::blt:
         *rop = TR::InstOpCode::bge;
         return(false);
      case TR::InstOpCode::bltl:
         *rop = TR::InstOpCode::bge;
         return(true);
      case TR::InstOpCode::bne:
         *rop = TR::InstOpCode::beq;
         return(false);
      case TR::InstOpCode::bnel:
         *rop = TR::InstOpCode::beq;
         return(true);
      case TR::InstOpCode::bnun:
         *rop = TR::InstOpCode::bun;
         return(false);
      case TR::InstOpCode::bun:
         *rop = TR::InstOpCode::bnun;
         return(false);
      default:
         TR_ASSERT(0, "New PPC conditional branch opcodes have to have corresponding reversed opcode: %d\n", (int32_t)op);
         *rop = TR::InstOpCode::bad;
         return(false);
      }
   }

uint8_t *TR::PPCAlignedLabelInstruction::generateBinaryEncoding()
   {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol *label            = getLabelSymbol();
   uint8_t        *cursor           = instructionStart;
   int32_t        alignment         = getAlignment();
   int32_t        paddingNOPsNeeded = 0;

   if ((alignment-1) & (intptr_t)cursor)
      {
      paddingNOPsNeeded = (alignment - ((alignment-1) & (intptr_t)cursor))/(PPC_INSTRUCTION_LENGTH);
      }

   if ((paddingNOPsNeeded > 0) ^ getFlipAlignmentDecision())
      {
      for (int32_t i = 0; i < paddingNOPsNeeded; i++)
         {
         TR::InstOpCode opCode(TR::InstOpCode::nop);
         opCode.copyBinaryToBuffer(cursor);
         cursor += PPC_INSTRUCTION_LENGTH;
         }
      }

   label->setCodeLocation(cursor); // point past any NOPs to the following instruction
   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::PPCAlignedLabelInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   TR_ASSERT((getAlignment() % PPC_INSTRUCTION_LENGTH) == 0, "label alignment must be a multiple of the instruction length, alignment = %d", getAlignment());
   TR_ASSERT(getOpCodeValue() == TR::InstOpCode::label, "AlignedLabel only supported for TR::InstOpCode::Label opcodes");
   setEstimatedBinaryLength(getAlignment() - PPC_INSTRUCTION_LENGTH);
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::PPCConditionalBranchInstruction::generateBinaryEncoding()
   {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol *label            = getLabelSymbol();
   uint8_t        *cursor           = instructionStart;
   TR::Compilation *comp = cg()->comp();
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   // bclr doesn't have a label
   if (label)
      {
      if (label->getCodeLocation() != NULL)
         {
         if (!getFarRelocation())
            {
            insertConditionRegister((uint32_t *)cursor);
            *(int32_t *)cursor |= ((label->getCodeLocation()-cursor) & 0xffff);
            }
         else // too far - need fix up
            {
            TR::InstOpCode::Mnemonic reversedOp;
            bool  linkForm = reversedConditionalBranchOpCode(getOpCodeValue(), &reversedOp);
            TR::InstOpCode reversedOpCode(reversedOp);
            TR::InstOpCode extraOpCode(linkForm?TR::InstOpCode::bl:TR::InstOpCode::b);

            cursor = reversedOpCode.copyBinaryToBuffer(cursor);
            insertConditionRegister((uint32_t *)cursor);
            *(int32_t *)cursor |= 0x0008;
            cursor += 4;

            cursor = extraOpCode.copyBinaryToBuffer(cursor);
            *(int32_t *)cursor |= ((label->getCodeLocation() - cursor) & 0x03fffffc);
            }
         }
      else
         {
         if (!getFarRelocation())
            {
            insertConditionRegister((uint32_t *)cursor);
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(cursor, label));
            }
         else // too far - need fix up
            {
            TR::InstOpCode::Mnemonic reversedOp;
            bool  linkForm = reversedConditionalBranchOpCode(getOpCodeValue(), &reversedOp);
            TR::InstOpCode reversedOpCode(reversedOp);
            TR::InstOpCode extraOpCode(linkForm?TR::InstOpCode::bl:TR::InstOpCode::b);

            cursor = reversedOpCode.copyBinaryToBuffer(cursor);
            insertConditionRegister((uint32_t *)cursor);
            *(int32_t *)cursor |= 0x0008;
            cursor += 4;

            cursor = extraOpCode.copyBinaryToBuffer(cursor);
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, label));
            }
         }
      }
   else
      insertConditionRegister((uint32_t *)cursor);

   // set up prediction bits if there is any.
   if (haveHint())
      {
      if (getFarRelocation())
         reverseLikeliness();
      if (getOpCode().setsCTR())
         *(int32_t *)instructionStart |= (getLikeliness() ? PPCOpProp_BranchLikelyMaskCtr : PPCOpProp_BranchUnlikelyMaskCtr);
      else
         *(int32_t *)instructionStart |= (getLikeliness() ? PPCOpProp_BranchLikelyMask : PPCOpProp_BranchUnlikelyMask) ;
      }

   cursor += 4;
   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::PPCConditionalBranchInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH);
   setEstimatedBinaryLocation(currentEstimate);
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::PPCAdminInstruction::generateBinaryEncoding()
   {
   uint8_t  *instructionStart = cg()->getBinaryBufferCursor();
   int i;

   if (_fenceNode != NULL)    // must be TR::InstOpCode::fence
      {
      uint32_t rtype = _fenceNode->getRelocationType();
      if (rtype == TR_AbsoluteAddress)
         {
         for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
            {
            uint8_t **target = (uint8_t **)_fenceNode->getRelocationDestination(i);
            *target = instructionStart;
            }
         }
      else if (rtype == TR_EntryRelative32Bit)
	 {
         for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
	    {
            *(uint32_t *)(_fenceNode->getRelocationDestination(i)) = cg()->getCodeLength();
	    }
	 }
      else // entryrelative16bit
         {
         for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
            {
            *(uint16_t *)(_fenceNode->getRelocationDestination(i)) = (uint16_t)cg()->getCodeLength();
            }
         }
      }
   setBinaryLength(0);
   setBinaryEncoding(instructionStart);

   return instructionStart;
   }

int32_t TR::PPCAdminInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(0);
   return currentEstimate;
   }


void
TR::PPCImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   if (needsAOTRelocation())
      {
         switch(getReloKind())
            {
            case TR_AbsoluteHelperAddress:
               cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getSymbolReference(), TR_AbsoluteHelperAddress, cg()), __FILE__, __LINE__, getNode());
               break;
            case TR_RamMethod:
               if (comp()->getOption(TR_UseSymbolValidationManager))
                  {
                  cg()->addExternalRelocation(
                     new (comp()->trHeapMemory()) TR::ExternalRelocation(
                        cursor,
                        (uint8_t *)comp()->getJittedMethodSymbol()->getResolvedMethod()->resolvedMethodAddress(),
                        (uint8_t *)TR::SymbolType::typeMethod,
                        TR_SymbolFromManager,
                        cg()),
                     __FILE__,
                     __LINE__,
                     getNode());
                  }
               else
                  {
                  cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_RamMethod, cg()), __FILE__, __LINE__, getNode());
                  }
               break;
            case TR_BodyInfoAddress:
               cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, 0, TR_BodyInfoAddress, cg()), __FILE__, __LINE__, getNode());
               break;
            default:
               TR_ASSERT(false, "Unsupported AOT relocation type specified.");
            }
      }

   TR::Compilation *comp = cg()->comp();
   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
      {
      // none-HCR: low-tag to invalidate -- BE or LE is relevant
      //
      void *valueToHash = *(void**)(cursor - (TR::Compiler->target.is64Bit()?4:0));
      void *addressToPatch = TR::Compiler->target.is64Bit()?
         (TR::Compiler->target.cpu.isBigEndian()?cursor:(cursor-4)) : cursor;
      cg()->jitAddPicToPatchOnClassUnload(valueToHash, addressToPatch);
      }

   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
      {
      // HCR: whole pointer replacement.
      //
      void **locationToPatch = (void**)(cursor - (TR::Compiler->target.is64Bit()?4:0));
      cg()->jitAddPicToPatchOnClassRedefinition(*locationToPatch, locationToPatch);
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)locationToPatch, (uint8_t *)*locationToPatch, TR_HCR, cg()), __FILE__,__LINE__, getNode());
      }

   }


uint8_t *TR::PPCImmInstruction::generateBinaryEncoding()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   *(int32_t *)cursor = (int32_t)getSourceImmediate();

   addMetaDataForCodeAddress(cursor);

   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

uint8_t *TR::PPCImm2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertImmediateField(toPPCCursor(cursor));
   insertImmediateField2(toPPCCursor(cursor));
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

uint8_t *TR::PPCSrc1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertSource1Register(toPPCCursor(cursor));
   if (getOpCodeValue() == TR::InstOpCode::mtfsf |
       getOpCodeValue() == TR::InstOpCode::mtfsfl ||
       getOpCodeValue() == TR::InstOpCode::mtfsfw)
      {
      insertMaskField(toPPCCursor(cursor));
      }
   else
      {
      insertImmediateField(toPPCCursor(cursor));
      }
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength()) ;
   return cursor;
   }

uint8_t *TR::PPCDepImmInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   *(int32_t *)cursor = (int32_t)getSourceImmediate();
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::PPCTrg1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::PPCTrg1Src1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;

   if (isRegCopy() && (toRealRegister(getTargetRegister()) == toRealRegister(getSource1Register())))
      {
      }
   else
      {
      cursor = getOpCode().copyBinaryToBuffer(instructionStart);
      insertTargetRegister(toPPCCursor(cursor));
      insertSource1Register(toPPCCursor(cursor));
      cursor += PPC_INSTRUCTION_LENGTH;
      }
   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::PPCTrg1ImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH);
   return currentEstimate + getEstimatedBinaryLength();
   }

static TR::Instruction *loadReturnAddress(TR::Node * node, uintptrj_t value, TR::Register *trgReg, TR::Instruction *cursor)
   {
   return cursor->cg()->loadAddressConstantFixed(node, value, trgReg, cursor);
   }


void
TR::PPCTrg1ImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
      {
      TR::Node *node = getNode();
      cg()->jitAddPicToPatchOnClassUnload((void *)(TR::Compiler->target.is64Bit()?node->getLongInt():node->getInt()), (void *)cursor);
      }

   if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), this) != comp->getStaticMethodPICSites()->end())
      {
      TR::Node *node = getNode();
      cg()->jitAddPicToPatchOnClassUnload((void *) (cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) (TR::Compiler->target.is64Bit()?node->getLongInt():node->getInt()), comp->getCurrentMethod())->classOfMethod()), (void *)cursor);
      }
   }


uint8_t *TR::PPCTrg1ImmInstruction::generateBinaryEncoding()
   {

   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));

   if (getOpCodeValue() == TR::InstOpCode::mtcrf ||
       getOpCodeValue() == TR::InstOpCode::mfocrf)
      {
      *((int32_t *)cursor) |= (getSourceImmediate()<<12);
      if ((TR::Compiler->target.cpu.id() >= TR_PPCgp) &&
          ((getSourceImmediate() & (getSourceImmediate() - 1)) == 0))
         // convert to PPC AS single field form
         *((int32_t *)cursor) |= 0x00100000;
      }
   else if (getOpCodeValue() == TR::InstOpCode::mfcr)
      {
      if ((TR::Compiler->target.cpu.id() >= TR_PPCgp) &&
          ((getSourceImmediate() & (getSourceImmediate() - 1)) == 0))
         // convert to PPC AS single field form
         *((int32_t *)cursor) |= (getSourceImmediate()<<12) | 0x00100000;
      else
         TR_ASSERT(getSourceImmediate() == 0xFF, "Bad field mask on mfcr");
      }
   else
      {
      insertImmediateField(toPPCCursor(cursor));
      }

   addMetaDataForCodeAddress(cursor);

   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }


void
TR::PPCTrg1Src1ImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
      {
      cg()->jitAddPicToPatchOnClassUnload((void *)(getSourceImmPtr()), (void *)cursor);
      }
   if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), this) != comp->getStaticMethodPICSites()->end())
      {
      cg()->jitAddPicToPatchOnClassUnload((void *) (cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) (getSourceImmPtr()), comp->getCurrentMethod())->classOfMethod()), (void *)cursor);
      }
   }


uint8_t *TR::PPCTrg1Src1ImmInstruction::generateBinaryEncoding()
   {

   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));
   insertSource1Register(toPPCCursor(cursor));
   if (getOpCodeValue() == TR::InstOpCode::srawi || getOpCodeValue() == TR::InstOpCode::srawi_r ||
       getOpCodeValue() == TR::InstOpCode::sradi || getOpCodeValue() == TR::InstOpCode::sradi_r ||
       getOpCodeValue() == TR::InstOpCode::extswsli)
      {
      insertShiftAmount(toPPCCursor(cursor));
      }
   else if (getOpCodeValue() == TR::InstOpCode::dtstdg)
      {
      setSourceImmediate(getSourceImmediate() << 10);
      insertImmediateField(toPPCCursor(cursor));
      }
   else
      {
      insertImmediateField(toPPCCursor(cursor));
      }

   addMetaDataForCodeAddress(cursor);

   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }


static void insertMaskField(uint32_t *instruction, TR::InstOpCode::Mnemonic op, int64_t lmask)
   {
   int32_t encoding;
   // A mask is is a string of 1 bits surrounded by a string of 0 bits.
   // For word instructions it is specified through its start and stop bit
   // numbers.  Note - the mask is considered circular so the start bit
   // number may be greater than the stop bit number.
   // Examples:     input     start   stop
   //              00FFFF00      8     23
   //              00000001     31     31
   //              80000001     31      0
   //              FFFFFFFF      0     31  (somewhat arbitrary)
   //              00000000      ?      ?  (illegal)
   //
   // For doubleword instructions only one of the start bit or stop bit is
   // specified and the other is implicit in the instruction.  The bit
   // number is strangely encoded in that the low order bit 5 comes first
   // and the high order bits after.  The field is in bit positions 21-26.

   // For these instructions the immediate is not a mask but a 1-bit immediate operand
   if (op == TR::InstOpCode::cmprb)
      {
      // populate 1-bit L field
      encoding = (((uint32_t)lmask) & 0x1) << 21;
      *instruction |= encoding;
      return;
      }

   // For these instructions the immediate is not a mask but a 2-bit immediate operand
   if (op == TR::InstOpCode::xxpermdi ||
       op == TR::InstOpCode::xxsldwi)
      {
      encoding = (((uint32_t)lmask) & 0x3) << 8;
      *instruction |= encoding;
      return;
      }

   if (op == TR::InstOpCode::addex ||
       op == TR::InstOpCode::addex_r)
      {
      encoding = (((uint32_t)lmask) & 0x3) << 9;
      *instruction |= encoding;
      return;
      }

   // For these instructions the immediate is not a mask but a 4-bit immediate operand
   if (op == TR::InstOpCode::vsldoi)
      {
      encoding = (((uint32_t)lmask) & 0xf)<< 6;
      *instruction |= encoding;
      return;
      }

   TR::InstOpCode       opCode(op);

   if (opCode.isCRLogical())
      {
      encoding = (((uint32_t) lmask) & 0xffffffff);
      *instruction |= encoding;
      return;
      }

   TR_ASSERT(lmask, "A mask of 0 cannot be encoded");   

   if (opCode.isDoubleWord())
      {
      int bitnum;

      if (opCode.useMaskEnd())
	 {
         TR_ASSERT(contiguousBits(lmask) &&
		((lmask & CONSTANT64(0x8000000000000000)) != 0) &&
		((lmask == -1) || ((lmask & 0x1) == 0)),
		"Bad doubleword mask for ME encoding");
         bitnum = leadingOnes(lmask) - 1;
	 }
      else
	 {
         bitnum = leadingZeroes(lmask);
	 // assert on cases like 0xffffff00000000ff
         TR_ASSERT((bitnum != 0) || (lmask == -1) || ((lmask & 0x1) == 0) ||
                             (op!=TR::InstOpCode::rldic   &&
                              op!=TR::InstOpCode::rldimi  &&
                              op!=TR::InstOpCode::rldic_r &&
                              op!=TR::InstOpCode::rldimi_r),
                "Cannot handle wrap-around, check mask for correctness");
	 }
      encoding = ((bitnum&0x1f)<<6) | ((bitnum&0x20));

      }
   else // single word
      {
      // special case the 3-bit rounding mode fields
      if (op == TR::InstOpCode::drrnd || op == TR::InstOpCode::dqua)
         {
         encoding = (lmask << 9) & 0x600;
         }
      else
         {
         int32_t mask = lmask&0xffffffff;
         int32_t maskBegin;
         int32_t maskEnd;

         maskBegin = leadingZeroes(~mask & (2*mask));
         maskBegin = (maskBegin + (maskBegin != 32)) & 0x1f;
         maskEnd  = leadingZeroes(mask & ~(2*mask));
         encoding = 32*maskBegin + maskEnd << 1; // shift encrypted mask into position
         }
      }
   *instruction |= encoding;
   }

uint8_t *TR::PPCTrg1Src1Imm2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));
   insertSource1Register(toPPCCursor(cursor));
   insertShiftAmount(toPPCCursor(cursor));
   insertMaskField(toPPCCursor(cursor), getOpCodeValue(), getLongMask());
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }


uint8_t *TR::PPCTrg1Src2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;

   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));
   insertSource1Register(toPPCCursor(cursor));
   insertSource2Register(toPPCCursor(cursor));
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::PPCTrg1Src2ImmInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));
   insertSource1Register(toPPCCursor(cursor));
   insertSource2Register(toPPCCursor(cursor));
   insertMaskField(toPPCCursor(cursor), getOpCodeValue(), getLongMask());
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);

   return cursor;
   }

uint8_t *TR::PPCTrg1Src3Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toPPCCursor(cursor));
   insertSource1Register(toPPCCursor(cursor));
   insertSource2Register(toPPCCursor(cursor));
   insertSource3Register(toPPCCursor(cursor));
   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::PPCSrc2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertSource1Register(toPPCCursor(cursor));
   insertSource2Register(toPPCCursor(cursor));

   cursor += PPC_INSTRUCTION_LENGTH;
   setBinaryLength(PPC_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }


uint8_t *TR::PPCMemSrc1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;

   getMemoryReference()->mapOpCode(this);

   cursor = getOpCode().copyBinaryToBuffer(instructionStart);

   insertSourceRegister(toPPCCursor(cursor));
   cursor = getMemoryReference()->generateBinaryEncoding(this, cursor, cg());
   setBinaryLength(cursor-instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

uint8_t *TR::PPCMemInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   getMemoryReference()->mapOpCode(this);
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   cursor = getMemoryReference()->generateBinaryEncoding(this, cursor, cg());
   setBinaryLength(cursor-instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }
int32_t TR::PPCMemSrc1Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getMemoryReference()->estimateBinaryLength(*cg()));
   return(currentEstimate + getEstimatedBinaryLength());
   }

uint8_t *TR::PPCTrg1MemInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;

   getMemoryReference()->mapOpCode(this);

   cursor = getOpCode().copyBinaryToBuffer(instructionStart);

   insertTargetRegister(toPPCCursor(cursor));
   // Set hint bit if there is any
   // The control for the different values is done through asserts in the constructor
   if (haveHint())
   {
      *(int32_t *)instructionStart |=  getHint();
   }

   cursor = getMemoryReference()->generateBinaryEncoding(this, cursor, cg());
   setBinaryLength(cursor-instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

int32_t TR::PPCTrg1MemInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getMemoryReference()->estimateBinaryLength(*cg()));
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::PPCControlFlowInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   setBinaryLength(0);
   return cursor;
   }


int32_t TR::PPCControlFlowInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   switch(getOpCodeValue())
      {
      case TR::InstOpCode::iflong:
      case TR::InstOpCode::setbool:
      case TR::InstOpCode::idiv:
      case TR::InstOpCode::ldiv:
      case TR::InstOpCode::ifx:
      case TR::InstOpCode::iternary:
         if (useRegPairForResult())
            {
            if (!useRegPairForCond())
               setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 6);
            else
               setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 8);
            }
         else
            {
            if (!useRegPairForCond())
               setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 4);
            else
               setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 6);
            }
         break;
      case TR::InstOpCode::setbflt:
         setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 5);
         break;
      case TR::InstOpCode::setblong:
      case TR::InstOpCode::flcmpg:
      case TR::InstOpCode::flcmpl:
      case TR::InstOpCode::irem:
      case TR::InstOpCode::lrem:
      case TR::InstOpCode::d2i:
      case TR::InstOpCode::setbx:
         setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 6);
         break;
      case TR::InstOpCode::d2l:
         if (TR::Compiler->target.is64Bit())
            setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 6);
       else
            setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 8);
         break;
      case TR::InstOpCode::lcmp:
         setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 11);
         break;
      case TR::InstOpCode::cfnan:
         setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 2);
         break;
      case TR::InstOpCode::cdnan:
         setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH * 3);
         break;
      default:
         TR_ASSERT(false,"unknown control flow instruction (estimateBinaryLength)");
      }
   return currentEstimate + getEstimatedBinaryLength();
   }

#ifdef J9_PROJECT_SPECIFIC
uint8_t *TR::PPCVirtualGuardNOPInstruction::generateBinaryEncoding()
   {
   uint8_t    *cursor           = cg()->getBinaryBufferCursor();
   TR::LabelSymbol *label        = getLabelSymbol();
   int32_t     length = 0;

   _site->setLocation(cursor);
   if (label->getCodeLocation() == NULL)
      {
      _site->setDestination(cursor);
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation((uint8_t *) (&_site->getDestination()), label));

#ifdef DEBUG
   if (debug("traceVGNOP"))
      printf("####> virtual location = %p, label (relocation) = %p\n", cursor, label);
#endif
      }
   else
      {
       _site->setDestination(label->getCodeLocation());
#ifdef DEBUG
   if (debug("traceVGNOP"))
      printf("####> virtual location = %p, label location = %p\n", cursor, label->getCodeLocation());
#endif
      }

   setBinaryEncoding(cursor);
   if (cg()->sizeOfInstructionToBePatched(this) == 0 ||
       // AOT needs an explicit nop, even if there are patchable instructions at this site because
       // 1) Those instructions might have AOT data relocations (and therefore will be incorrectly patched again)
       // 2) We might want to re-enable the code path and unpatch, in which case we would have to know what the old instruction was
         cg()->comp()->compileRelocatableCode())
      {
      TR::InstOpCode opCode(TR::InstOpCode::nop);
      opCode.copyBinaryToBuffer(cursor);
      length = PPC_INSTRUCTION_LENGTH;
      }

   setBinaryLength(length);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor+length;
   }

int32_t TR::PPCVirtualGuardNOPInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   // This is a conservative estimation for reserving NOP space.
   setEstimatedBinaryLength(PPC_INSTRUCTION_LENGTH);
   return currentEstimate+PPC_INSTRUCTION_LENGTH;
   }
#endif
