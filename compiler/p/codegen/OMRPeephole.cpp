/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/Peephole.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GenerateOMRInstructions.hpp"
#include "codegen/Instruction.hpp"
#include "p/codegen/PPCInstruction.hpp"

#define OPT_DETAILS "O^O PPC PEEPHOLE: "

static bool
isWAWOrmrPeepholeCandidateInstr(TR::Instruction *instr)
   {
   if (instr != NULL && !instr->willBePatched() &&
      (!instr->isLabel() ||
       (instr->getNode()->getOpCodeValue() == TR::BBStart &&
        instr->getNode()->getBlock()->isExtensionOfPreviousBlock())) &&
      //FIXME: properly -- defs/refs of instructions with Mem are not quite right.
       (instr->getMemoryReference()==NULL ||
        instr->getMemoryReference()->getUnresolvedSnippet()==NULL))
      {
      return true;
      }
   return false;
   }

static bool
isEBBTerminatingBranch(TR::Instruction *instr)
   {
   TR::Instruction *ppcInstr = instr;
   switch (ppcInstr->getOpCodeValue())
      {
      case TR::InstOpCode::b:                // Unconditional branch
      case TR::InstOpCode::bctr:             // Branch to count register
      case TR::InstOpCode::bctrl:            // Branch to count register and link
      case TR::InstOpCode::bfctr:            // Branch false to count register
      case TR::InstOpCode::btctr:            // Branch true to count register
      case TR::InstOpCode::bl:               // Branch and link
      case TR::InstOpCode::blr:              // Branch to link register
      case TR::InstOpCode::blrl:             // Branch to link register and link
      case TR::InstOpCode::beql:             // Branch and link if equal
      case TR::InstOpCode::bgel:             // Branch and link if greater than or equal
      case TR::InstOpCode::bgtl:             // Branch and link if greater than
      case TR::InstOpCode::blel:             // Branch and link if less than or equal
      case TR::InstOpCode::bltl:             // Branch and link if less than
      case TR::InstOpCode::bnel:             // Branch and link if not equal
      case TR::InstOpCode::vgnop:            // A vgnop can be backpatched to a branch
         return true;
      default:
         return false;
      }
   }

OMR::Power::Peephole::Peephole(TR::Compilation* comp) :
   OMR::Peephole(comp)
   {}

bool
OMR::Power::Peephole::performOnInstruction(TR::Instruction* cursor)
   {
   bool performed = false;

   if (self()->comp()->getOptLevel() == noOpt)
      return performed;

   // Cache the cursor for use in the peephole functions
   self()->cursor = cursor;

   if ((self()->comp()->target().cpu.is(OMR_PROCESSOR_PPC_P6)) && cursor->isTrap())
      {
#if defined(AIXPPC)
      tryToSwapTrapAndLoad();
#endif
      }
   else
      {
      switch (cursor->getOpCodeValue())
         {
         case TR::InstOpCode::cmpi4:
            {
            if (self()->comp()->target().is32Bit())
               performed |= self()->tryToReduceCompareToRecordForm();
            break;
            }
         case TR::InstOpCode::cmpi8:
            {
            if (self()->comp()->target().is64Bit())
               performed |= self()->tryToReduceCompareToRecordForm();
            break;
            }
         case TR::InstOpCode::mr:
            {
            performed |= self()->tryToRemoveRedundantMoveRegister();
            break;
            }
         case TR::InstOpCode::stw:
#if defined(TR_HOST_64BIT)
         case TR::InstOpCode::std:
#endif
            {
            performed |= self()->tryToRemoveRedundantLoadAfterStore();
            break;
            }
         case TR::InstOpCode::sync:
         case TR::InstOpCode::lwsync:
         case TR::InstOpCode::isync:
            {
            performed |= self()->tryToRemoveRedundantSync(self()->comp()->isOptServer() ? 12 : 6);
            break;
            }
         default:
            {
            performed |= self()->tryToRemoveRedundantWriteAfterWrite();
            break;
            }
         }
      }

   return performed;
   }

bool
OMR::Power::Peephole::tryToReduceCompareToRecordForm()
   {
   static bool disableRecordFormPeephole = feGetEnv("TR_DisableRecordFormPeephole") != NULL;
   if (disableRecordFormPeephole)
      return false;

   TR::Instruction* cmpiInstruction = cursor;
   TR::Register *cmpiSourceReg = cmpiInstruction->getSourceRegister(0);
   TR::Register *cmpiTargetReg = cmpiInstruction->getTargetRegister(0);

   if (cmpiInstruction->getSourceImmediate() != 0 ||
       toRealRegister(cmpiTargetReg)->getRegisterNumber() != TR::RealRegister::cr0)
      return false;

   TR::Instruction *current = cmpiInstruction->getPrev();
   while (current != NULL &&
          !current->isLabel() &&
          (!isEBBTerminatingBranch(current) ||
          current->getKind() == OMR::Instruction::IsControlFlow) &&
          current->getOpCodeValue() != TR::InstOpCode::wrtbar)
      {
      if (current->getKind() == OMR::Instruction::IsControlFlow)
         {
         current = current->getPrev();
         continue;
         }

      if (current->getPrimaryTargetRegister() != NULL &&
          current->getPrimaryTargetRegister() == cmpiSourceReg)
         // if target reg is same as source of compare
         {
         if (current->getOpCode().isRecordForm())
            {
            if (performTransformation(self()->comp(), "%sRemove redundant compare immediate %p.\n", OPT_DETAILS, cmpiInstruction))
               {
               cmpiInstruction->remove();
               return true;
               }
            return false;
            }
         if (current->getOpCode().hasRecordForm())
            {
            // avoid certain record forms on POWER4/POWER5
            if (self()->comp()->target().cpu.is(OMR_PROCESSOR_PPC_GP) ||
                self()->comp()->target().cpu.is(OMR_PROCESSOR_PPC_GR))
               {
               TR::InstOpCode::Mnemonic opCode = current->getOpCodeValue();
               // addc_r, subfc_r, divw_r and divd_r are microcoded
               if (opCode == TR::InstOpCode::addc || opCode == TR::InstOpCode::subfc ||
                   opCode == TR::InstOpCode::divw || opCode == TR::InstOpCode::divd)
                  return false;
               // rlwinm_r is cracked, so avoid if possible
               if (opCode == TR::InstOpCode::rlwinm)
                  {
                  TR::PPCTrg1Src1Imm2Instruction *inst = (TR::PPCTrg1Src1Imm2Instruction *)current;
                  // see if we can convert to an andi_r or andis_r
                  if (inst->getSourceImmediate() == 0 &&
                      (inst->getMask()&0xffff0000) == 0)
                     {
                     if (performTransformation(self()->comp(), "%sChange %p to andi_r, remove compare immediate %p.\n", OPT_DETAILS, current, cmpiInstruction))
                        {
                        generateTrg1Src1ImmInstruction(self()->cg(), TR::InstOpCode::andi_r, inst->getNode(), inst->getPrimaryTargetRegister(), inst->getSourceRegister(0),cmpiTargetReg, inst->getMask(), current);

                        // Removing consecutive instructions in a backwards peephole window requires us to update the
                        // restart point so the peephole traversal is able to continue
                        if (current->getNext() == cmpiInstruction)
                           {
                           self()->prevInst = current->getPrev();
                           }

                        current->remove();
                        cmpiInstruction->remove();
                        return true;
                        }
                     return false;
                     }
                  else if (inst->getSourceImmediate() == 0 &&
                      (inst->getMask()&0x0000ffff) == 0)
                     {
                     if (performTransformation(self()->comp(), "%sChange %p to andis_r, remove compare immediate %p.\n", OPT_DETAILS, current, cmpiInstruction))
                        {
                        generateTrg1Src1ImmInstruction(self()->cg(), TR::InstOpCode::andis_r, inst->getNode(), inst->getPrimaryTargetRegister(), inst->getSourceRegister(0), cmpiTargetReg, ((uint32_t)inst->getMask()) >> 16, current);

                        // Removing consecutive instructions in a backwards peephole window requires us to update the
                        // restart point so the peephole traversal is able to continue
                        if (current->getNext() == cmpiInstruction)
                           {
                           self()->prevInst = current->getPrev();
                           }

                        current->remove();
                        cmpiInstruction->remove();
                        return true;
                        }
                     return false;
                     }
                  }
               }
            // change the opcode to get record form
            if (performTransformation(self()->comp(), "%sChange %p to record form, remove compare immediate %p.\n", OPT_DETAILS, current, cmpiInstruction))
               {
               current->setOpCodeValue(current->getRecordFormOpCode());
               cmpiInstruction->remove();
               return true;
               }
            return false;
            }
         return false;
         }
      else
         {
         if (current->defsRegister(toRealRegister(cmpiSourceReg)) ||
             current->defsRegister(toRealRegister(cmpiTargetReg)) ||
             current->getOpCode().isRecordForm())
            return false;
         }
      current = current->getPrev();
      }

   return false;
   }

bool
OMR::Power::Peephole::tryToRemoveRedundantLoadAfterStore()
   {
   static bool disableLHSPeephole = feGetEnv("TR_DisableLHSPeephole") != NULL;
   if (disableLHSPeephole)
      return false;

   // Just because the instruction opcode is a load or store doesn't mean we're necessarily using
   // a MemoryReference. In some cases, we use other instruction kinds (e.g. Trg1Imm) if we're
   // going to patch the instruction.
   if (cursor->getKind() != TR::Instruction::IsMemSrc1 || !cursor->getNext() || cursor->getNext()->getKind() != TR::Instruction::IsTrg1Mem)
      return false;

   TR::Instruction *storeInstruction = cursor;
   TR::Instruction   *loadInstruction = storeInstruction->getNext();
   TR::InstOpCode&      storeOp = storeInstruction->getOpCode();
   TR::InstOpCode&      loadOp = loadInstruction->getOpCode();
   TR::MemoryReference *storeMemRef = ((TR::PPCMemSrc1Instruction *)storeInstruction)->getMemoryReference();
   TR::MemoryReference *loadMemRef = ((TR::PPCTrg1MemInstruction *)loadInstruction)->getMemoryReference();

   // Next instruction has to be a load,
   // the store and load have to agree on size,
   // they have to both be GPR ops or FPR ops,
   // neither can be the update form,
   // and they both have to be resolved.
   // TODO: Relax the first constraint a bit and handle cases were unrelated instructions appear between the store and the load
   if (!loadOp.isLoad() ||
       storeMemRef->getLength() != loadMemRef->getLength() ||
       !(storeOp.gprOp() == loadOp.gprOp() ||
         storeOp.fprOp() == loadOp.fprOp()) ||
       storeOp.isUpdate() ||
       loadOp.isUpdate() ||
       storeMemRef->getUnresolvedSnippet() ||
       loadMemRef->getUnresolvedSnippet())
      {
      return false;
      }

   // TODO: Don't bother with indexed loads/stores for now, fix them later
   if (storeMemRef->getIndexRegister() || loadMemRef->getIndexRegister())
      {
      return false;
      }

   // The store and load mem refs have to agree on whether or not they need delayed offsets
   if (storeMemRef->hasDelayedOffset() != loadMemRef->hasDelayedOffset())
      {
      // Memory references that use delayed offsets will not have the correct offset
      // at this point, so we can't compare their offsets to the offsets used by regular references,
      // however we can still compare two mem refs that both use delayed offsets.
      return false;
      }

   // The store and load have to use the same base and offset
   if (storeMemRef->getBaseRegister() != loadMemRef->getBaseRegister() ||
       storeMemRef->getOffset(*self()->comp()) != loadMemRef->getOffset(*self()->comp()))
      {
      return false;
      }

   TR::Register *srcReg = ((TR::PPCMemSrc1Instruction *)storeInstruction)->getSourceRegister();
   TR::Register *trgReg = ((TR::PPCTrg1MemInstruction *)loadInstruction)->getTargetRegister();

   if (srcReg == trgReg)
      {
      // Found the pattern:
      //   stw/std rX, ...
      //   lwz/ld rX, ...
      // will remove ld
      // and replace lwz with rlwinm, rX, rX, 0, 0xffffffff
      if (loadInstruction->getOpCodeValue() == TR::InstOpCode::lwz)
         {
         if (performTransformation(self()->comp(), "%sReplace lwz " POINTER_PRINTF_FORMAT " with rlwinm after store " POINTER_PRINTF_FORMAT ".\n", OPT_DETAILS, loadInstruction, storeInstruction))
            {
            generateTrg1Src1Imm2Instruction(self()->cg(), TR::InstOpCode::rlwinm, loadInstruction->getNode(), trgReg, srcReg, 0, 0xffffffff, storeInstruction);
            loadInstruction->remove();
            return true;
            }
         }
      else if (performTransformation(self()->comp(), "%sRemove redundant load " POINTER_PRINTF_FORMAT " after store " POINTER_PRINTF_FORMAT ".\n", OPT_DETAILS, loadInstruction, storeInstruction))
         {
         loadInstruction->remove();
         return true;
         }

      return false;
      }

   // Found the pattern:
   //   stw/std rX, ...
   //   lwz/ld rY, ...
   // and will remove lwz, which will result in:
   //   stw rX, ...
   //   rlwinm rY, rX, 0, 0xffffffff
   // or will remove ld, which will result in:
   //   std rX, ...
   //   mr rY, rX
   // and then the mr peephole should run on the resulting mr
   if (loadInstruction->getOpCodeValue() == TR::InstOpCode::lwz)
      {
      if (performTransformation(self()->comp(), "%sReplace redundant load " POINTER_PRINTF_FORMAT " after store " POINTER_PRINTF_FORMAT " with rlwinm.\n", OPT_DETAILS, loadInstruction, storeInstruction))
         {
         generateTrg1Src1Imm2Instruction(self()->cg(), TR::InstOpCode::rlwinm, loadInstruction->getNode(), trgReg, srcReg, 0, 0xffffffff, storeInstruction);
         loadInstruction->remove();
         return true;
         }
      }
   else if (performTransformation(self()->comp(), "%sReplace redundant load " POINTER_PRINTF_FORMAT " after store " POINTER_PRINTF_FORMAT " with mr.\n", OPT_DETAILS, loadInstruction, storeInstruction))
      {
      generateTrg1Src1Instruction(self()->cg(), TR::InstOpCode::mr, loadInstruction->getNode(), trgReg, srcReg, storeInstruction);
      loadInstruction->remove();
      return true;
      }

   return false;
   }

bool
OMR::Power::Peephole::tryToRemoveRedundantMoveRegister()
   {
   static bool disableMRPeepholes = feGetEnv("TR_DisableMRPeepholes") != NULL;
   if (disableMRPeepholes)
      return false;

   int32_t windowSize = 0;
   const int32_t maxWindowSize = self()->comp()->isOptServer() ? 16 : 8;
   TR::list<TR_GCStackMap*> stackMaps(getTypedAllocator<TR_GCStackMap*>(self()->comp()->allocator()));
   TR::Instruction* mrInstruction = cursor;
   TR::Register *mrSourceReg = mrInstruction->getSourceRegister(0);
   TR::Register *mrTargetReg = mrInstruction->getTargetRegister(0);

   if (mrTargetReg == mrSourceReg)
      {
      if (performTransformation(self()->comp(), "%sRemove redundant mr %p.\n", OPT_DETAILS, mrInstruction))
         {
         mrInstruction->remove();
         return true;
         }
      return false;
      }

   TR::Instruction *current = mrInstruction->getNext();
   bool all_mr_source_uses_rewritten = true;
   bool inEBB = false;
   bool extendWindow = false;
   bool performed = false;

   while (isWAWOrmrPeepholeCandidateInstr(current) &&
          !isEBBTerminatingBranch(current) &&
          windowSize < maxWindowSize)
      {
      // Keep track of any GC points we cross in order to fix them if we remove a register move
      // Note: Don't care about GC points in snippets, since we won't remove a reg move if inEBB, i.e. if we've encountered a branch (to a snippet or elsewhere)
      if (current->getGCMap() &&
          current->getGCMap()->getRegisterMap() & self()->cg()->registerBitMask(toRealRegister(mrTargetReg)->getRegisterNumber()))
         stackMaps.push_front(current->getGCMap());

      if (current->isBranchOp())
         inEBB = true;

      if (current->getOpCodeValue() == TR::InstOpCode::mr &&
          current->getSourceRegister(0) == mrTargetReg &&
          current->getTargetRegister(0) == mrSourceReg)
         {
         if (performTransformation(self()->comp(), "%sRemove mr copyback %p.\n", OPT_DETAILS, current))
            {
            current->remove();
            extendWindow = true;
            performed = true;
            }
         else
            return performed;
         // bypass the test below for an overwrite of the source register
         current = current->getNext();
         if (extendWindow)
            {
            windowSize = 0;
            extendWindow = false;
            }
         else
            windowSize++;
         continue;
         }

      TR::MemoryReference *memRef = current->getMemoryReference();

      if (memRef != NULL)
         {
         // See FIXME above: these are all due to defs/refs not accurate.
         // We didn't try to test if base or index will be modified in delayedIndexForm
         if (memRef->isUsingDelayedIndexedForm() &&
             (memRef->getBaseRegister() == mrSourceReg ||
              memRef->getIndexRegister() == mrSourceReg ||
              memRef->getBaseRegister() == mrTargetReg ||
              memRef->getIndexRegister() == mrTargetReg))
            return performed;
         if (current->isUpdate() &&
             (memRef->getBaseRegister() == mrSourceReg ||
              memRef->getBaseRegister() == mrTargetReg))
            return performed;
         if (memRef->getBaseRegister() == mrTargetReg)
            {
            if (toRealRegister(mrSourceReg)->getRegisterNumber() == TR::RealRegister::gr0)
               all_mr_source_uses_rewritten = false;
            else if (performTransformation(self()->comp(), "%sRewrite base register %p.\n", OPT_DETAILS, current))
               {
               memRef->setBaseRegister(mrSourceReg);
               extendWindow = true;
               performed = true;
               }
            else
               return performed;
            }
         if (memRef->getIndexRegister() == mrTargetReg)
            {
            if (performTransformation(self()->comp(), "%sRewrite index register %p.\n", OPT_DETAILS, current))
               {
               memRef->setIndexRegister(mrSourceReg);
               extendWindow = true;
               performed = true;
               }
            else
               return performed;
            }
         if (current->getKind() == OMR::Instruction::IsMemSrc1 &&
             ((TR::PPCMemSrc1Instruction *)current)->getSourceRegister() == mrTargetReg)
            {
            if (performTransformation(self()->comp(), "%sRewrite store source register %p.\n", OPT_DETAILS, current))
               {
               ((TR::PPCMemSrc1Instruction *)current)->setSourceRegister(mrSourceReg);
               extendWindow = true;
               performed = true;
               }
            else
               return performed;
            }
         }
      else if (current->getOpCodeValue() == TR::InstOpCode::vgnop)
         // a vgnop can be backpatched to a branch, and so we must suppress any later attempt to remove the mr
         all_mr_source_uses_rewritten = false;
      else
         {
         switch(current->getKind())
            {
            case OMR::Instruction::IsSrc1:
               {
               TR::PPCSrc1Instruction *inst = (TR::PPCSrc1Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Src1 source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src1:
               {
               TR::PPCTrg1Src1Instruction *inst = (TR::PPCTrg1Src1Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src1 source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src1Imm:
               {
               TR::PPCTrg1Src1ImmInstruction *inst = (TR::PPCTrg1Src1ImmInstruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (toRealRegister(mrSourceReg)->getRegisterNumber() == TR::RealRegister::gr0 &&
                      (current->getOpCodeValue() == TR::InstOpCode::addi ||
                       current->getOpCodeValue() == TR::InstOpCode::addi2 ||
                       current->getOpCodeValue() == TR::InstOpCode::addis))
                     all_mr_source_uses_rewritten = false;
                  else if (performTransformation(self()->comp(), "%sRewrite Trg1Src1Imm source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src1Imm2:
               {
               TR::PPCTrg1Src1Imm2Instruction *inst = (TR::PPCTrg1Src1Imm2Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src1Imm2 source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }

                  if(current->getOpCode().usesTarget() &&
                     inst->getTargetRegister() == mrTargetReg)
                     return performed;
               break;
               }
            case OMR::Instruction::IsSrc2:
               {
               TR::PPCSrc2Instruction *inst = (TR::PPCSrc2Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Src2 1st source register %p.\n", OPT_DETAILS, urrent))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Src2 2nd source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src2:
               {
               if (current->getOpCodeValue() == TR::InstOpCode::xvmaddadp ||
                   current->getOpCodeValue() == TR::InstOpCode::xvmsubadp ||
                   current->getOpCodeValue() == TR::InstOpCode::xvnmsubadp)
                  return performed;

               TR::PPCTrg1Src2Instruction *inst = (TR::PPCTrg1Src2Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src2 1st source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src2 2nd source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src3:
               {
               TR::PPCTrg1Src3Instruction *inst = (TR::PPCTrg1Src3Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src3 1st source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src3 2nd source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               if (inst->getSource3Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src3 3nd source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource3Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src2Imm:
               {
               TR::PPCTrg1Src2ImmInstruction *inst = (TR::PPCTrg1Src2ImmInstruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src2Imm 1st source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(self()->comp(), "%sRewrite Trg1Src2Imm 2nd source register %p.\n", OPT_DETAILS, current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     performed = true;
                     }
                  else
                     return performed;
                  }
               break;
               }
            }
         }

      // if instruction overwrites either of the mr source or target
      // registers, bail out

      if (current->defsRegister(toRealRegister(mrTargetReg)))
         {
         // having rewritten all uses of mrTargetReg, the mr can be removed
         if (!inEBB && all_mr_source_uses_rewritten &&
             performTransformation(self()->comp(), "%sRemove mr %p when target redefined at %p.\n", OPT_DETAILS, mrInstruction, current))
            {
            // Adjust any register maps that will be invalidated by the removal
            if (!stackMaps.empty())
               {
               for (auto stackMapIter = stackMaps.begin(); stackMapIter != stackMaps.end(); ++stackMapIter)
                  {
                  if (self()->cg()->getDebug())
                     if (self()->comp()->getOption(TR_TraceCG))
                        traceMsg(self()->comp(), "Adjusting register map %p; removing %s, adding %s due to removal of mr %p\n",
                                *stackMapIter,
                                self()->cg()->getDebug()->getName(mrTargetReg),
                                self()->cg()->getDebug()->getName(mrSourceReg),
                                mrInstruction);
                  (*stackMapIter)->resetRegistersBits(self()->cg()->registerBitMask(toRealRegister(mrTargetReg)->getRegisterNumber()));
                  (*stackMapIter)->setRegisterBits(self()->cg()->registerBitMask(toRealRegister(mrSourceReg)->getRegisterNumber()));
                  }
               }

            mrInstruction->remove();
            performed = true;
            }
         return performed;
         }

      if (current->defsRegister(toRealRegister(mrSourceReg)))
         return performed;

      current = current->getNext();
      if (extendWindow)
         {
         windowSize = 0;
         extendWindow = false;
         }
      else
         windowSize++;
      }

   return performed;
   }

bool
OMR::Power::Peephole::tryToRemoveRedundantSync(int32_t window)
   {
   static bool disableSyncPeepholes = feGetEnv("TR_DisableSyncPeepholes") != NULL;
   if (disableSyncPeepholes)
      return false;

   TR::Instruction* instructionCursor = cursor;
   TR::Instruction *first = instructionCursor;
   TR::InstOpCode::Mnemonic nextOp = instructionCursor->getNext()->getOpCodeValue();
   int visited = 0;
   bool removeFirst, removeNext;

   while (visited < window)
      {
      instructionCursor = first;

      if (instructionCursor->getNext()->isSyncSideEffectFree())
         {
         for (; visited < window && instructionCursor->getNext()->isSyncSideEffectFree(); visited++)
            {
            instructionCursor = instructionCursor->getNext();
            nextOp = instructionCursor->getNext()->getOpCodeValue();
            }
         }
      else
         {
         visited++;
         nextOp = instructionCursor->getNext()->getOpCodeValue();
         }

      if (visited >= window)
         return false;

      removeFirst = removeNext = false;

      switch(first->getOpCodeValue())
         {
         case TR::InstOpCode::isync:
            {
            if(nextOp == TR::InstOpCode::sync || nextOp == TR::InstOpCode::lwsync || nextOp == TR::InstOpCode::isync)
               removeFirst = true;
            break;
            }
         case TR::InstOpCode::lwsync:
            {
            if(nextOp == TR::InstOpCode::sync)
               removeFirst = true;
            else if(nextOp == TR::InstOpCode::lwsync || nextOp == TR::InstOpCode::isync)
               removeNext = true;
            break;
            }
         case TR::InstOpCode::sync:
            {
            if(nextOp == TR::InstOpCode::sync || nextOp == TR::InstOpCode::lwsync || nextOp == TR::InstOpCode::isync)
               removeNext = true;
            break;
            }
         default:
            return false;
         }

      if (removeFirst)
         {
         if (performTransformation(self()->comp(), "%sRemove redundant syncronization instruction %p.\n", OPT_DETAILS, first))
            {
            self()->cg()->generateNop(first->getNode(), first->getPrev(), TR_NOPStandard);
            first->remove();
            return true;
            }
         }
      else if(removeNext)
         {
         if (performTransformation(self()->comp(), "%sRemove redundant syncronization instruction %p.\n", OPT_DETAILS, instructionCursor->getNext()))
            {
            self()->cg()->generateNop(instructionCursor->getNext()->getNode(), instructionCursor, TR_NOPStandard);
            instructionCursor->getNext()->getNext()->remove();
            return true;
            }
         }
      }

   return false;
   }

bool
OMR::Power::Peephole::tryToRemoveRedundantWriteAfterWrite()
   {
   static bool disableWAWPeephole = feGetEnv("TR_DisableWAWPeephole") != NULL;
   if (disableWAWPeephole)
      return false;

   int32_t window = 0;
   int32_t maxWindowSize = self()->comp()->isOptServer() ? 20 : 10;

   // Get and check if there is a target register
   TR::Register *currTargetReg = cursor->getTargetRegister(0);

   if (currTargetReg && currTargetReg->getKind() != TR_CCR && isWAWOrmrPeepholeCandidateInstr(cursor) &&
       !cursor->isBranchOp() && !cursor->isCall() &&
       !cursor->getOpCode().isRecordForm() &&
       !cursor->getOpCode().setsCarryFlag())
      {
      TR::Instruction *current = cursor->getNext();

      while (isWAWOrmrPeepholeCandidateInstr(current) &&
             !current->isBranchOp() &&
             !current->isCall() &&
             window < maxWindowSize)
         {
         if (current->getOpCode().usesTarget())
            return false;

         switch(current->getKind())
            {
            case OMR::Instruction::IsSrc1:
            case OMR::Instruction::IsTrg1Src1:
            case OMR::Instruction::IsTrg1Src1Imm:
            case OMR::Instruction::IsTrg1Src1Imm2:
               if (currTargetReg == current->getSourceRegister(0))
                  return false;
               break;
            case OMR::Instruction::IsSrc2:
            case OMR::Instruction::IsTrg1Src2:
            case OMR::Instruction::IsTrg1Src2Imm:
            case OMR::Instruction::IsMem:
            case OMR::Instruction::IsTrg1Mem:
               if (currTargetReg == current->getSourceRegister(0) ||
                   currTargetReg == current->getSourceRegister(1))
                  return false;
               break;
            case OMR::Instruction::IsTrg1Src3:
            case OMR::Instruction::IsMemSrc1:
               if (currTargetReg == current->getSourceRegister(0) ||
                   currTargetReg == current->getSourceRegister(1) ||
                   currTargetReg == current->getSourceRegister(2))
                  return false;
               break;
            default:
                  return false;
            }

         if (current->getTargetRegister(0) == currTargetReg)
            {
            TR::Instruction *q[4];
            // In case the instruction is part of the requestor
            bool isConstData = self()->cg()->checkAndFetchRequestor(cursor, q);
            if (isConstData)
               {
               if (performTransformation(self()->comp(), "%sRemove dead instrcution group from WAW %p -> %p.\n", OPT_DETAILS, cursor, current))
                  {
                  for (int32_t i = 0; i < 4; i++)
                     {
                     if (q[i])  q[i]->remove();
                     }

                  return true;
                  }
               }
            else
               {
               if (performTransformation(self()->comp(), "%sRemove dead instrcution from WAW %p -> %p.\n", OPT_DETAILS, cursor, current))
                  {
                  cursor->remove();
                  return true;
                  }
               }

            return false;
            }

         current = current->getNext();
         window++;
         }
      }

   return false;
   }

bool
OMR::Power::Peephole::tryToSwapTrapAndLoad()
   {
   static bool disableTrapPeephole = feGetEnv("TR_DisableTrapPeephole") != NULL;
   if (disableTrapPeephole)
      return false;

   int WINDOW = 2;
   TR::RealRegister *RAr;
   TR::Register *RAv;

   TR::Instruction* instructionCursor = cursor;
   if ((RAv = instructionCursor->getSourceRegister(0)) != NULL)
      RAr = instructionCursor->getSourceRegister(0)->getRealRegister();
   else
      return false;

   if (instructionCursor->getSourceRegister(1) != NULL ||
      instructionCursor->getOffset() != 0)
      {
      return false;
      }

   if ((RAv->getStartOfRange() != NULL) && RAv->getStartOfRange()->isLoad())
      return false;

   TR::RealRegister *RTr;
   TR::Instruction *next = instructionCursor->getNext();

   for(int i = 0; i < WINDOW  && next != NULL; i++)
      {
      if (next->isStore())
         return false;
      if (!next->isLoad() && !next->isStore() && !next->isSyncSideEffectFree())
         return false;
      if (NULL != next->getMemoryReference() &&
          NULL != next->getMemoryReference()->getUnresolvedSnippet())
         return false;
      if (NULL != next->getGCMap())
         return false;

      for (int j = 0; next->getTargetRegister(j) != NULL; j++)
         {
         if (next->getTargetRegister(j)->getRealRegister() == RAr)
            return false;
         }
      if (next->isLoad() && next->getSourceRegister(0) != NULL && next->getSourceRegister(0)->getRealRegister() == RAr)
         {
         if (next->getSourceRegister(1) != NULL)
            return false;

         //Page 0 which is readable on AIX is 4096 bytes long, and the largest possible load is 16 bytes.
         if (next->getOffset() >= (4096-16) || next->getOffset() < 0)
            return false;

         TR::Instruction *trap = instructionCursor;
         TR::Instruction *load = next;

         if (trap->getPrev() != NULL && load->getNext() != NULL)
            {
            for (TR::Instruction *prev = load->getPrev(); prev != trap->getPrev(); prev = prev->getPrev())
               {
               for (int k = 0; load->getTargetRegister(k) != NULL; k++)
                  {
                  for (int l = 0; prev->getSourceRegister(l) != NULL; l++)
                     {
                     if (prev->getSourceRegister(l)->getRealRegister() == load->getTargetRegister(k)->getRealRegister())
                        return false;
                     }
                  for (int m = 0; prev->getTargetRegister(m) != NULL; m++)
                     {
                     if (prev->getTargetRegister(m)->getRealRegister() == load->getTargetRegister(k)->getRealRegister())
                        return false;
                     }
                  }
               }
            if (performTransformation(self()->cg()->self()->comp(), "%sSwap trap %p and load %p instructions.\n", OPT_DETAILS, trap, load))
               {
               if (i > 0)
                  {
                  TR::Instruction *loadPrev = load->getPrev();
                  TR::Instruction *loadNext = load->getNext();
                  trap->getPrev()->setNext(load);
                  trap->getNext()->setPrev(load);
                  load->setNext(trap->getNext());
                  load->setPrev(trap->getPrev());
                  loadPrev->setNext(trap);
                  loadNext->setPrev(trap);
                  trap->setNext(loadNext);
                  trap->setPrev(loadPrev);
                  }
               else
                  {
                  trap->getPrev()->setNext(load);
                  load->setPrev(trap->getPrev());
                  trap->setNext(load->getNext());
                  load->getNext()->setPrev(trap);
                  load->setNext(trap);
                  trap->setPrev(load);
                  }
               return true;
               }

            return false;
            }
         }
      next = next->getNext();
      }

   return false;
   }
