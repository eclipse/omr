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

#pragma csect(CODE,"OMRZPeephole#C")
#pragma csect(STATIC,"OMRZPeephole#S")
#pragma csect(TEST,"OMRZPeephole#T")

#include "codegen/Peephole.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/S390GenerateInstructions.hpp"
#include "codegen/S390Instruction.hpp"

namespace TR { class Node; }

static bool
isBarrierToPeepHoleLookback(TR::Instruction* cursor)
   {
   if (cursor == NULL)
      return true;

   if (cursor->isLabel())
      return true;

   if (cursor->isCall())
      return true;

   if (cursor->isBranchOp())
      return true;

   if (cursor->getOpCodeValue() == TR::InstOpCode::DCB)
      return true;

   return false;
   }

static TR::Instruction*
realInstructionWithLabelsAndRET(TR::Instruction* cursor)
   {
   while (cursor && (cursor->getKind() == TR::Instruction::IsPseudo ||
                   cursor->getKind() == TR::Instruction::IsNotExtended) && !cursor->isRet())
      {
      cursor = cursor->getNext();
      }

   return cursor;
   }

static bool
seekRegInFutureMemRef(TR::Instruction* cursor, int32_t maxWindowSize, TR::Register *targetReg)
   {
   TR::Instruction * current = cursor->getNext();
   int32_t windowSize=0;

   while ((current != NULL) &&
         !current->matchesTargetRegister(targetReg) &&
         !isBarrierToPeepHoleLookback(current) &&
         windowSize<maxWindowSize)
      {
      // does instruction load or store? otherwise just ignore and move to next instruction
      if (current->isLoad() || current->isStore())
         {
         TR::MemoryReference *mr = current->getMemoryReference();

         if (mr && (mr->getBaseRegister()==targetReg || mr->getIndexRegister()==targetReg))
            {
            return true;
            }
         }
      current = current->getNext();
      windowSize++;
      }

   return false;
   }

OMR::Z::Peephole::Peephole(TR::Compilation* comp) :
   OMR::Peephole(comp)
   {}

bool
OMR::Z::Peephole::performOnInstruction(TR::Instruction* cursor)
   {
   bool performed = false;

   // Cache the cursor for use in the peephole functions
   self()->cursor = cursor;

   if (self()->cg()->afterRA())
      {
      if (cursor->isBranchOp())
         performed |= self()->tryToForwardBranchTarget();

      switch(cursor->getOpCodeValue())
         {
         case TR::InstOpCode::CGIT:
            {
            performed |= self()->tryToRemoveRedundantCompareAndTrap();
            break;
            }
         case TR::InstOpCode::CGRJ:
            {
            performed |= self()->tryToReduceCRJLHIToLOCHI(TR::InstOpCode::CGR);
            break;
            }
         case TR::InstOpCode::CLR:
            {
            performed |= self()->tryToReduceCLRToCLRJ();
            break;
            }
         case TR::InstOpCode::CRJ:
            {
            performed |= self()->tryToReduceCRJLHIToLOCHI(TR::InstOpCode::CR);
            break;
            }
         case TR::InstOpCode::L:
            {
            bool performedCurrentPeephole = false;

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceLToICM();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceLToLZRF(TR::InstOpCode::LZRF);

            performed |= performedCurrentPeephole;
            break;
            }
         case TR::InstOpCode::LA:
            {
            performed |= self()->tryToRemoveRedundantLA();
            break;
            }
         case TR::InstOpCode::LER:
         case TR::InstOpCode::LDR:
            {
            bool performedCurrentPeephole = false;

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLoadRegister();

            performed |= performedCurrentPeephole;
            break;
            }
         case TR::InstOpCode::LG:
            {
            performed |= self()->tryToReduceLToLZRF(TR::InstOpCode::LZRG);
            break;
            }
         case TR::InstOpCode::LGR:
            {
            bool performedCurrentPeephole = false;

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceAGI();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceLGRToLGFR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveRedundantLR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceLRCHIToLTR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLoadRegister();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToFoldLoadRegisterIntoSubsequentInstruction();

            performed |= performedCurrentPeephole;
            break;
            }
         case TR::InstOpCode::LGFR:
            {
            performed |= self()->tryToRemoveRedundant32To64BitExtend(true);
            break;
            }
         case TR::InstOpCode::LHI:
            {
            performed |= self()->tryToReduceLHIToXR();
            break;
            }
         case TR::InstOpCode::LLC:
            {
            performed |= self()->tryToReduceLLCToLLGC();
            break;
            }
         case TR::InstOpCode::LLGF:
            {
            performed |= self()->tryToReduceLToLZRF(TR::InstOpCode::LLZRGF);
            break;
            }
         case TR::InstOpCode::LLGFR:
            {
            performed |= self()->tryToRemoveRedundant32To64BitExtend(false);
            break;
            }
         case TR::InstOpCode::LR:
            {
            bool performedCurrentPeephole = false;

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceAGI();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveRedundantLR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceLRCHIToLTR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLoadRegister();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToFoldLoadRegisterIntoSubsequentInstruction();

            performed |= performedCurrentPeephole;
            break;
            }
         case TR::InstOpCode::LTR:
         case TR::InstOpCode::LTGR:
            {
            bool performedCurrentPeephole = false;

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceAGI();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToReduceLTRToCHI();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveRedundantLTR();

            if (!performedCurrentPeephole)
               performedCurrentPeephole |= self()->tryToRemoveDuplicateLoadRegister();

            performed |= performedCurrentPeephole;
            break;
            }
         case TR::InstOpCode::NILF:
            {
            performed |= self()->tryToRemoveDuplicateNILF();
            break;
            }
         case TR::InstOpCode::NILH:
            {
            performed |= self()->tryToRemoveDuplicateNILH();
            break;
            }
         case TR::InstOpCode::SLLG:
         case TR::InstOpCode::SLAG:
         case TR::InstOpCode::SLLK:
         case TR::InstOpCode::SRLK:
         case TR::InstOpCode::SLAK:
         case TR::InstOpCode::SRAK:
            {
            performed |= self()->tryToReduce64BitShiftTo32BitShift();
            break;
            }
         case TR::InstOpCode::SRL:
         case TR::InstOpCode::SLL:
            {
            performed |= self()->tryToRemoveRedundantShift();
            break;
            }
         default:
            break;
         }
      }
   else
      {
      switch(cursor->getOpCodeValue())
         {
         case TR::InstOpCode::L:
            {
            performed |= self()->tryLoadStoreReduction(TR::InstOpCode::ST, 4);
            break;
            }
         case TR::InstOpCode::LFH:
            {
            performed |= self()->tryLoadStoreReduction(TR::InstOpCode::STFH, 4);
            break;
            }
         case TR::InstOpCode::LG:
            {
            performed |= self()->tryLoadStoreReduction(TR::InstOpCode::STG, 8);
            break;
            }
         default:
            break;
         }
      }

   return performed;
   }

bool
OMR::Z::Peephole::tryLoadStoreReduction(TR::InstOpCode::Mnemonic storeOpCode, uint16_t size)
   {
   if (cursor->getNext()->getOpCodeValue() == storeOpCode)
      {
      TR::S390RXInstruction* loadInst = static_cast<TR::S390RXInstruction*>(cursor);
      TR::S390RXInstruction* storeInst = static_cast<TR::S390RXInstruction*>(cursor->getNext());
      TR::MemoryReference* loadMemRef = loadInst->getMemoryReference();
      TR::MemoryReference* storeMemRef = storeInst->getMemoryReference();

      // Cannot change to MVC if symbol references are unresolved, MVC doesn't follow same format as load and store so patching won't work
      if (loadMemRef->getSymbolReference() && loadMemRef->getSymbolReference()->isUnresolved() ||
          storeMemRef->getSymbolReference() && storeMemRef->getSymbolReference()->isUnresolved())
         {
         return false;
         }

      if (!loadMemRef->getBaseRegister() && !loadMemRef->getIndexRegister() ||
          !storeMemRef->getBaseRegister() && !storeMemRef->getIndexRegister())
         {
         return false;
         }

      TR::Register* reg = loadInst->getRegisterOperand(1);

      if (reg != storeInst->getRegisterOperand(1))
         {
         return false;
         }

      // Register must only be used in the load-store sequence, possibly used in the memory reference
      // of the load however, not in the store since the the register would have to be loaded first.
      ncount_t uses = 2;
      uses += reg == loadMemRef->getBaseRegister();

      // Bail out if register could have future uses, or if either instruction has dependencies.
      // Since 2 instructions are replaced with one before RA, we can't handle things like merging
      // dependencies.
      if (reg->getTotalUseCount() != uses || loadInst->getDependencyConditions() || storeInst->getDependencyConditions())
         {
         return false;
         }

      if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Transforming load-store sequence at %p to MVC.\n", storeInst))
         {
         TR::DebugCounter::incStaticDebugCounter(self()->comp(), "z/peephole/load-store");

         loadMemRef->resetMemRefUsedBefore();
         storeMemRef->resetMemRefUsedBefore();

         reg->decTotalUseCount(2);

         if (loadMemRef->getBaseRegister())
            {
            loadMemRef->getBaseRegister()->decTotalUseCount();
            }

         if (storeMemRef->getBaseRegister())
            {
            storeMemRef->getBaseRegister()->decTotalUseCount();
            }

         if (loadMemRef->getIndexRegister())
            {
            loadMemRef->getIndexRegister()->decTotalUseCount();
            }

         if (storeMemRef->getIndexRegister())
            {
            storeMemRef->getIndexRegister()->decTotalUseCount();
            }

         TR::Instruction * newInst = generateSS1Instruction(self()->cg(), TR::InstOpCode::MVC, loadInst->getNode(), size - 1, storeMemRef, loadMemRef, cursor->getPrev());

         if (loadInst->getGCMap())
            {
            newInst->setGCMap(loadInst->getGCMap());
            }

         if (loadInst->needsGCMap())
            {
            newInst->setNeedsGCMap(loadInst->getGCRegisterMask());
            }

         if (storeInst->getGCMap())
            {
            newInst->setGCMap(storeInst->getGCMap());
            }

         if (storeInst->needsGCMap())
            {
            newInst->setNeedsGCMap(storeInst->getGCRegisterMask());
            }

         storeInst->remove();
         self()->cg()->replaceInst(loadInst, newInst);

         return true;
         }
      }
   return false;
   }

bool
OMR::Z::Peephole::tryToFoldLoadRegisterIntoSubsequentInstruction()
   {
   if (!self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
      {
      return false;
      }

   int32_t windowSize = 0;
   const int32_t maxWindowSize = 4;

   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);
   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);

   TR::Instruction * current = cursor->getNext();

   while ((current != NULL) &&
      !current->isLabel() &&
      !current->isCall() &&
      !(current->isBranchOp() && !(current->isExceptBranchOp())) &&
      windowSize < maxWindowSize)
      {
      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
         current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
         current->getOpCodeValue() == TR::InstOpCode::TEND ||
         current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      // If the source register is redefined we can no longer carry out out this optimization. For example:
      //
      //     LR GPR6,GPR0
      //     SR GPR0,GPR11
      //     AHI GPR6,-1
      //
      // Is not equivalent to:
      //
      //     SR GPR0,GPR11
      //     AHIK GPR6,GPR0, -1
      if (current->defsRegister(lgrSourceReg))
         {
         return false;
         }

      if (current->usesRegister(lgrTargetReg))
         {
         if (current->defsRegister(lgrTargetReg))
            {
            TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();
            TR::Instruction * newInstr = NULL;
            TR::Register * srcReg = NULL;

            if (curOpCode != TR::InstOpCode::AHI && curOpCode != TR::InstOpCode::AGHI &&
               curOpCode != TR::InstOpCode::SLL && curOpCode != TR::InstOpCode::SLA &&
               curOpCode != TR::InstOpCode::SRA && curOpCode != TR::InstOpCode::SRLK)
               {
               srcReg = current->getRegisterOperand(2);

               // ex:  LR R1, R2
               //      XR R1, R1
               // ==>
               //      XRK R1, R2, R2
               if (srcReg == lgrTargetReg)
                  {
                  srcReg = lgrSourceReg;
                  }
               }

            if ((current->getOpCode().is32bit() && cursor->getOpCodeValue() == TR::InstOpCode::LGR) ||
               (current->getOpCode().is64bit() && cursor->getOpCodeValue() == TR::InstOpCode::LR))
               {

               // Make sure we abort if the register copy and the subsequent operation
               //             do not have the same word length (32-bit or 64-bit)
               //
               //  e.g:    LGR R1, R2
               //          SLL R1, 1
               //      ==>
               //          SLLK R1, R2, 1
               //
               //      NOT valid as R1's high word will not be cleared

               return false;
               }

            switch (curOpCode)
               {
               case TR::InstOpCode::AR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::ARK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::AGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::AGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::ALR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::ALRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::ALGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::ALGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::AHI:
                  {
                  int16_t imm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
                  newInstr = generateRIEInstruction(self()->cg(), TR::InstOpCode::AHIK, current->getNode(), lgrTargetReg, lgrSourceReg, imm, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::AGHI:
                  {
                  int16_t imm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
                  newInstr = generateRIEInstruction(self()->cg(), TR::InstOpCode::AGHIK, current->getNode(), lgrTargetReg, lgrSourceReg, imm, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::NR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::NRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::NGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::NGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::XR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::XRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::XGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::XGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::OR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::ORK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::OGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::OGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SLA:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if (mf != NULL)
                     {
                     mf->resetMemRefUsedBefore();
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SLAK, current->getNode(), lgrTargetReg, lgrSourceReg, mf, current->getPrev());
                     }
                  else
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SLAK, current->getNode(), lgrTargetReg, lgrSourceReg, imm, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SLL:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if (mf != NULL)
                     {
                     mf->resetMemRefUsedBefore();
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SLLK, current->getNode(), lgrTargetReg, lgrSourceReg, mf, current->getPrev());
                     }
                  else
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SLLK, current->getNode(), lgrTargetReg, lgrSourceReg, imm, current->getPrev());
                  break;
                  }

               case TR::InstOpCode::SRA:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction *)current)->getMemoryReference();
                  if (mf != NULL)
                     {
                     mf->resetMemRefUsedBefore();
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SRAK, current->getNode(), lgrTargetReg, lgrSourceReg, mf, current->getPrev());
                     }
                  else
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SRAK, current->getNode(), lgrTargetReg, lgrSourceReg, imm, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SRL:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if (mf != NULL)
                     {
                     mf->resetMemRefUsedBefore();
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SRLK, current->getNode(), lgrTargetReg, lgrSourceReg, mf, current->getPrev());
                     }
                  else
                     newInstr = generateRSInstruction(self()->cg(), TR::InstOpCode::SRLK, current->getNode(), lgrTargetReg, lgrSourceReg, imm, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::SRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::SGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SLR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::SLRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               case TR::InstOpCode::SLGR:
                  {
                  newInstr = generateRRRInstruction(self()->cg(), TR::InstOpCode::SLGRK, current->getNode(), lgrTargetReg, lgrSourceReg, srcReg, current->getPrev());
                  break;
                  }
               default:
                  return false;
               }

            cursor->remove();
            self()->cg()->replaceInst(current, newInstr);

            return true;
            }

         // Target register is used, but not defined which means we cannot swing the def of the target past this use
         // and we must abort our search at this point
         return false;
         }

      windowSize++;
      current = current->getNext();
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToForwardBranchTarget()
   {
   TR::LabelSymbol *targetLabelSym = NULL;

   switch(cursor->getOpCodeValue())
      {
      case TR::InstOpCode::BRC:
         targetLabelSym = ((TR::S390BranchInstruction*)cursor)->getLabelSymbol();
         break;

      case TR::InstOpCode::CRJ:
      case TR::InstOpCode::CGRJ:
      case TR::InstOpCode::CIJ:
      case TR::InstOpCode::CGIJ:
      case TR::InstOpCode::CLRJ:
      case TR::InstOpCode::CLGRJ:
      case TR::InstOpCode::CLIJ:
      case TR::InstOpCode::CLGIJ:
         targetLabelSym = toS390RIEInstruction(cursor)->getBranchDestinationLabel();
         break;

      default:
         return false;
      }

   if (targetLabelSym == NULL)
      return false;

   auto targetInst = targetLabelSym->getInstruction();
   if (targetInst == NULL)
      return false;

   while (targetInst->isLabel() || targetInst->getOpCodeValue() == TR::InstOpCode::fence)
      targetInst = targetInst->getNext();

   if (targetInst->getOpCodeValue() == TR::InstOpCode::BRC)
      {
      auto firstBranch = (TR::S390BranchInstruction*)targetInst;
      if (firstBranch->getBranchCondition() == TR::InstOpCode::COND_BRC)
         {
         if (performTransformation(self()->comp(), "\nO^O S390 PEEPHOLE: Forwarding branch target on %s [%p].\n", TR::InstOpCode::metadata[cursor->getOpCodeValue()].name, cursor))
            {
            auto newTargetLabelSym = firstBranch->getLabelSymbol();
            switch (cursor->getOpCodeValue())
               {
               case TR::InstOpCode::BRC:
                  ((TR::S390BranchInstruction*)cursor)->setLabelSymbol(newTargetLabelSym);
                  break;

               case TR::InstOpCode::CRJ:
               case TR::InstOpCode::CGRJ:
               case TR::InstOpCode::CIJ:
               case TR::InstOpCode::CGIJ:
               case TR::InstOpCode::CLRJ:
               case TR::InstOpCode::CLGRJ:
               case TR::InstOpCode::CLIJ:
               case TR::InstOpCode::CLGIJ:
                  toS390RIEInstruction(cursor)->setBranchDestinationLabel(newTargetLabelSym);
                  break;

               default:
                  return false;
               }

            return true;
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToReduce64BitShiftTo32BitShift()
   {
   TR::S390RSInstruction* shiftInst = static_cast<TR::S390RSInstruction*>(cursor);

   // The shift is supposed to be an integer shift when reducing 64bit shifts.
   // Note the NOT in front of second boolean expr. pair
   TR::InstOpCode::Mnemonic oldOpCode = shiftInst->getOpCodeValue();
   if ((oldOpCode == TR::InstOpCode::SLLG || oldOpCode == TR::InstOpCode::SLAG)
         && shiftInst->getNode()->getOpCodeValue() != TR::ishl)
      {
      return false;
      }

   TR::Register* sourceReg = (shiftInst->getSecondRegister())?(shiftInst->getSecondRegister()->getRealRegister()):NULL;
   TR::Register* targetReg = (shiftInst->getRegisterOperand(1))?(shiftInst->getRegisterOperand(1)->getRealRegister()):NULL;

   // Source and target registers must be the same
   if (sourceReg != targetReg)
      {
      return false;
      }

   if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Reverting int shift at %p from SLLG/SLAG/S[LR][LA]K to SLL/SLA/SRL/SRA.\n", shiftInst))
      {
      TR::InstOpCode::Mnemonic newOpCode = TR::InstOpCode::bad;
      switch (oldOpCode)
         {
         case TR::InstOpCode::SLLG:
         case TR::InstOpCode::SLLK:
            newOpCode = TR::InstOpCode::SLL; break;
         case TR::InstOpCode::SLAG:
         case TR::InstOpCode::SLAK:
            newOpCode = TR::InstOpCode::SLA; break;
         case TR::InstOpCode::SRLK:
            newOpCode = TR::InstOpCode::SRL; break;
         case TR::InstOpCode::SRAK:
            newOpCode = TR::InstOpCode::SRA; break;
         default:
            TR_ASSERT_FATAL(false, "Unexpected OpCode for revertTo32BitShift\n");
            break;
         }

      TR::S390RSInstruction* newInstr = NULL;

      if (shiftInst->getSourceImmediate())
         {
         newInstr = new (self()->cg()->trHeapMemory()) TR::S390RSInstruction(newOpCode, shiftInst->getNode(), shiftInst->getRegisterOperand(1), shiftInst->getSourceImmediate(), shiftInst->getPrev(), self()->cg());
         }
      else if (shiftInst->getMemoryReference())
         {
         TR::MemoryReference* memRef = shiftInst->getMemoryReference();
         memRef->resetMemRefUsedBefore();
         newInstr = new (self()->cg()->trHeapMemory()) TR::S390RSInstruction(newOpCode, shiftInst->getNode(), shiftInst->getRegisterOperand(1), memRef, shiftInst->getPrev(), self()->cg());
         }
      else
         {
         TR_ASSERT_FATAL(false, "Unexpected RSY format\n");
         }

      self()->cg()->replaceInst(shiftInst, newInstr);

      return true;
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToReduceAGI()
   {
   bool performed=false;
   int32_t windowSize=0;
   const int32_t MaxWindowSize=8; // Look down 8 instructions (4 cycles)
   const int32_t MaxLAWindowSize=6; // Look up 6 instructions (3 cycles)

   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);
   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);

   TR::RealRegister *gpr0 = self()->cg()->machine()->getRealRegister(TR::RealRegister::GPR0);

   // no renaming possible if both target and source are the same
   // this can happend with LTR and LTGR
   // or if source reg is gpr0
   if  (toRealRegister(lgrSourceReg)==gpr0 || (lgrTargetReg==lgrSourceReg) ||
        toRealRegister(lgrTargetReg)==self()->cg()->getStackPointerRealRegister(NULL))
      return performed;

   TR::Instruction* current = cursor->getNext();

   // if:
   //  1) target register is invalidated, or
   //  2) current instruction is a branch or call
   // then cannot continue.
   // if:
   //  1) source register is invalidated, or
   //  2) current instruction is a label
   // then cannot rename registers, but might be able to replace
   // _cursor instruction with LA

   bool reachedLabel = false;
   bool reachedBranch = false;
   bool sourceRegInvalid = false;
   bool attemptLA = false;

   while ((current != NULL) &&
         !current->matchesTargetRegister(lgrTargetReg) &&
         !isBarrierToPeepHoleLookback(current) &&
         windowSize<MaxWindowSize)
      {
      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      // if we reach a label or the source reg becomes invalidated
      // then we cannot rename regs in mem refs, but we could still try using LA
      reachedLabel = reachedLabel || current->isLabel();
      reachedBranch = reachedBranch || (current->isBranchOp() && !(current->isExceptBranchOp()));

      // does instruction load or store? otherwise just ignore and move to next instruction
      if (current->isLoad() || current->isStore())
         {
         TR::MemoryReference *mr = current->getMemoryReference();
         while (mr)
            {
            if (mr && mr->getBaseRegister() == lgrTargetReg)
               {
               if (!reachedLabel && !reachedBranch && !sourceRegInvalid)
                  {
                  if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: AGI register renaming on [%p] from source load [%p].\n", current, cursor))
                     {
                     mr->setBaseRegister(lgrSourceReg, self()->cg());

                     performed = true;
                     }
                  }
               else
                  {
                  // We couldn't do register renaming, but perhapse can use an LA instruction
                  attemptLA = true;
                  }
               }

            if (mr && mr->getIndexRegister() == lgrTargetReg)
               {
               if (!reachedLabel && !reachedBranch && !sourceRegInvalid)
                  {
                  if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: AGI register renaming on [%p] from source load [%p].\n", current, cursor))
                     {
                     mr->setIndexRegister(lgrSourceReg);

                     performed = true;
                     }
                  }
               else
                  {
                  // We couldn't do register renaming, but perhapse can use an LA instruction
                  attemptLA = true;
                  }
               }

            if (mr == current->getMemoryReference())
               mr = current->getMemoryReference2();
            else
               mr = NULL;
            }
         }

         // If the current instruction invalidates the source register, subsequent instructions' memref cannot be renamed
         sourceRegInvalid = sourceRegInvalid || current->matchesTargetRegister(lgrSourceReg);

         current = current->getNext();
         windowSize++;
      }

   // We can replace the cursor with an LA instruction if:
   //   1. The mnemonic is LGR
   //   2. The target is 64-bit (because LA sets the upppermost bit to 0 on 31-bit)
   attemptLA &= cursor->getOpCodeValue() == TR::InstOpCode::LGR && self()->comp()->target().is64Bit();

   if (attemptLA)
      {
      // We check to see that we are eliminating the AGI, not just propagating it up in the code. We check for labels,
      // source register invalidation, and routine calls (basically the opposite of the above while loop condition).
      current = cursor->getPrev();
      windowSize = 0;
      while ((current != NULL) &&
         !isBarrierToPeepHoleLookback(current) &&
         !current->matchesTargetRegister(lgrSourceReg) &&
         windowSize < MaxLAWindowSize)
         {
         current = current->getPrev();
         windowSize++;
         }

      // if we reached the end of the loop and didn't find a conflict, switch the instruction to LA
      if (windowSize == MaxLAWindowSize)
         {
         if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: AGI LA reduction on [%p] from source load [%p].\n", current, cursor))
            {
            auto laInst = generateRXInstruction(self()->cg(), TR::InstOpCode::LA, cursor->getNode(), lgrTargetReg,
               generateS390MemoryReference(lgrSourceReg, 0, self()->cg()), cursor->getPrev());

            self()->cg()->replaceInst(cursor, laInst);

            performed = true;
            }
         }
      }

   return performed;
   }

bool
OMR::Z::Peephole::tryToReduceCLRToCLRJ()
   {
   bool branchTakenPerformReduction = false;
   bool fallThroughPerformReduction = false;

   if (cursor->getNext()->getOpCodeValue() == TR::InstOpCode::BRC)
      {
      TR::Instruction *clrInstruction = cursor;
      TR::Instruction *brcInstruction = cursor->getNext();
      TR::LabelSymbol *labelSymbol = brcInstruction->getLabelSymbol();

      /* Conditions for reduction
       * - Branch target is a snippet
       *    - we only need to check if CC is consumed in fall through case
       * - Else: branch target is not a snippet
       *    - we need to check if CC is consumed in both branch taken
       *      and fall through case
       */
      if (labelSymbol->getSnippet())
         {
         branchTakenPerformReduction = true;
         }
      else
         {
         // check branch taken case for condition code usage
         TR::Instruction* branchInstruction = labelSymbol->getInstruction();
         for (auto branchTakenInstIndex = 0; branchTakenInstIndex < 5 && NULL != branchInstruction; ++branchTakenInstIndex)
            {
            if (branchInstruction->getOpCode().readsCC())
               {
               break;
               }
            // CC is set before it is read (ordering of the if checks matter)
            if (branchInstruction->getOpCode().setsCC() || TR::BBEnd == branchInstruction->getNode()->getOpCodeValue())
               {
               branchTakenPerformReduction = true;
               break;
               }
            branchInstruction = branchInstruction->getNext();
            }
         }
      // check fall through case for condition code usage
      TR::Instruction* fallThroughInstruction = brcInstruction->getNext();
      for (auto fallThroughInstIndex = 0; fallThroughInstIndex < 5 && NULL != fallThroughInstruction; ++fallThroughInstIndex)
         {
         if (fallThroughInstruction->getOpCode().readsCC())
            {
            break;
            }
         // CC is set before it is read (ordering of the if checks matter)
         if (fallThroughInstruction->getOpCode().setsCC() || TR::BBEnd == fallThroughInstruction->getNode()->getOpCodeValue())
            {
            fallThroughPerformReduction = true;
            break;
            }
         fallThroughInstruction = fallThroughInstruction->getNext();
         }

      if (fallThroughPerformReduction
         && branchTakenPerformReduction
         && performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Transforming CLR [%p] and BRC [%p] to CLRJ\n", clrInstruction, brcInstruction))
         {
         TR_ASSERT_FATAL(clrInstruction->getNumRegisterOperands() == 2, "Number of register operands was not 2: %d\n", clrInstruction->getNumRegisterOperands());

         TR::Instruction *clrjInstruction = generateRIEInstruction(
            self()->cg(),
            TR::InstOpCode::CLRJ,
            clrInstruction->getNode(),
            clrInstruction->getRegisterOperand(1),
            clrInstruction->getRegisterOperand(2),
            labelSymbol,
            static_cast<TR::S390BranchInstruction*>(brcInstruction)->getBranchCondition(),
            clrInstruction->getPrev()
         );
         self()->cg()->replaceInst(clrInstruction, clrjInstruction);
         brcInstruction->remove();
         }
      }

   return fallThroughPerformReduction && branchTakenPerformReduction;
   }

bool
OMR::Z::Peephole::tryToReduceCRJLHIToLOCHI(TR::InstOpCode::Mnemonic compareMnemonic)
   {
   // This optimization relies on hardware instructions introduced in z13
   if (!self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z13))
      return false;

   TR::S390RIEInstruction* branchInst = static_cast<TR::S390RIEInstruction*>(cursor);

   TR::Instruction* currInst = cursor;
   TR::Instruction* nextInst = cursor->getNext();

   TR::Instruction* label = branchInst->getBranchDestinationLabel()->getInstruction();

   // Check that the instructions within the fall-through block can be conditionalized
   while (currInst = realInstructionWithLabelsAndRET(nextInst))
      {
      if (currInst->getKind() == TR::Instruction::IsLabel)
         {
         if (currInst == label)
            break;
         else
            return false;
         }

      if (currInst->getOpCodeValue() != TR::InstOpCode::LHI && currInst->getOpCodeValue() != TR::InstOpCode::LGHI)
         return false;

      nextInst = currInst->getNext();
      }

   currInst = cursor;
   nextInst = cursor->getNext();

   TR::InstOpCode::S390BranchCondition cond = getBranchConditionForMask(0xF - (getMaskForBranchCondition(branchInst->getBranchCondition()) & 0xF));

   if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Conditionalizing fall-through block following [%p].\n", currInst))
      {
      // Conditionalize the fall-though block
      while (currInst = realInstructionWithLabelsAndRET(nextInst))
         {
         if (currInst == label)
            break;

         // Because of the previous checks, LHI or LGHI instruction is guaranteed to be here
         TR::S390RIInstruction* RIInst = static_cast<TR::S390RIInstruction*>(currInst);

         auto lochiInst = generateRIEInstruction(self()->cg(), RIInst->getOpCode().getOpCodeValue() == TR::InstOpCode::LHI ? TR::InstOpCode::LOCHI : TR::InstOpCode::LOCGHI, RIInst->getNode(), RIInst->getRegisterOperand(1), RIInst->getSourceImmediate(), cond, RIInst->getPrev());

         // Conditionalize from "Load Immediate" to "Load Immediate on Condition"
         self()->cg()->replaceInst(RIInst, lochiInst);

         currInst = lochiInst;
         nextInst = lochiInst->getNext();
         }

      auto crInst = generateRRInstruction(self()->cg(), compareMnemonic, branchInst->getNode(), branchInst->getRegisterOperand(1), branchInst->getRegisterOperand(2), branchInst->getPrev());

      // Conditionalize the branch instruction from "Compare and Branch" to "Compare"
      self()->cg()->replaceInst(branchInst, crInst);

      return true;
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToReduceLToICM()
   {
   // Look for L/LTR instruction pair and reduce to LTR
   TR::Instruction* prev = cursor->getPrev();
   TR::Instruction* next = cursor->getNext();
   if (next->getOpCodeValue() != TR::InstOpCode::LTR && next->getOpCodeValue() != TR::InstOpCode::CHI)
      return false;

   bool performed = false;

   TR::S390RXInstruction* load= (TR::S390RXInstruction*) cursor;

   TR::MemoryReference* mem = load->getMemoryReference();

   if (mem == NULL) return false;
   // We cannot reduce L to ICM if the L uses both index and base registers.
   if (mem->getBaseRegister() != NULL && mem->getIndexRegister() != NULL) return false;

   // L and LTR/CHI instructions must work on the same register.
   if (load->getRegisterOperand(1) != next->getRegisterOperand(1)) return false;

   if (next->getOpCodeValue() == TR::InstOpCode::LTR)
      {
      // We must check for sourceRegister on the LTR instruction, in case there was a dead load
      //        L      GPRX, MEM       <-- this is a dead load, for whatever reason (e.g. GRA)
      //        LTR    GPRX, GPRY
      // it is wrong to transform the above sequence into
      //        ICM    GPRX, MEM
      if (next->getRegisterOperand(1) != ((TR::S390RRInstruction*)next)->getRegisterOperand(2))
         return false;
      }
   else
      {
      // CHI Opportunty:
      //      L  GPRX,..
      //      CHI  GPRX, '0'
      // reduce to:
      //      ICM GPRX,...
      TR::S390RIInstruction* chi = (TR::S390RIInstruction*) next;

      // CHI must be comparing against '0' (i.e. NULLCHK) or else our
      // condition codes will be wrong.
      if (chi->getSourceImmediate() != 0)
         return false;
      }

   if (performTransformation(self()->comp(), "\nO^O S390 PEEPHOLE: Reducing L [%p] being reduced to ICM.\n", cursor))
      {
      // Prevent reuse of memory reference
      TR::MemoryReference* memcp = generateS390MemoryReference(*mem, 0, self()->cg());

      if ((memcp->getBaseRegister() == NULL) &&
          (memcp->getIndexRegister() != NULL))
         {
         memcp->setBaseRegister(memcp->getIndexRegister(), self()->cg());
         memcp->setIndexRegister(0);
         }

      // Do the reduction - create the icm instruction
      TR::S390RSInstruction* icm = new (self()->cg()->trHeapMemory()) TR::S390RSInstruction(TR::InstOpCode::ICM, load->getNode(), load->getRegisterOperand(1), 0xF, memcp, prev, self()->cg());

      // Check if the load has an implicit NULLCHK.  If so, we need to ensure a GCmap is copied.
      if (load->throwsImplicitNullPointerException())
         {
         icm->setNeedsGCMap(0x0000FFFF);
         icm->setThrowsImplicitNullPointerException();
         icm->setGCMap(load->getGCMap());

         TR_Debug * debugObj = self()->cg()->getDebug();
         if (debugObj)
            debugObj->addInstructionComment(icm, "Throws Implicit Null Pointer Exception");
         }

      self()->cg()->replaceInst(load, icm);
      next->remove();

      memcp->stopUsingMemRefRegister(self()->cg());
      performed = true;
      }

   return performed;
   }

bool
OMR::Z::Peephole::tryToReduceLToLZRF(TR::InstOpCode::Mnemonic loadAndZeroRightMostByteMnemonic)
   {
   // This optimization relies on hardware instructions introduced in z13
   if (!self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z13))
      return false;

   if (cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILL)
      {
      TR::S390RXInstruction* loadInst = static_cast<TR::S390RXInstruction*>(cursor);
      TR::S390RIInstruction* nillInst = static_cast<TR::S390RIInstruction*>(cursor->getNext());

      if (!nillInst->isImm() || nillInst->getSourceImmediate() != 0xFF00)
         return false;

      TR::Register* loadTargetReg = loadInst->getRegisterOperand(1);
      TR::Register* nillTargetReg = nillInst->getRegisterOperand(1);

      if (loadTargetReg != nillTargetReg)
         return false;

      if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Transforming load-and-mask sequence at [%p].\n", nillInst))
         {
         // Remove the NILL instruction
         nillInst->remove();

         loadInst->getMemoryReference()->resetMemRefUsedBefore();

         auto lzrbInst = generateRXInstruction(self()->cg(), loadAndZeroRightMostByteMnemonic, loadInst->getNode(), loadTargetReg, loadInst->getMemoryReference(), cursor->getPrev());

         // Replace the load instruction with load-and-mask instruction
         self()->cg()->replaceInst(loadInst, lzrbInst);

         return true;
         }
      }
   return false;
   }

bool
OMR::Z::Peephole::tryToReduceLGRToLGFR()
   {
   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);

   TR::Instruction* current = cursor->getNext();

   if (current->getOpCodeValue() == TR::InstOpCode::LGFR)
      {
      TR::Register *curSourceReg = current->getRegisterOperand(2);
      TR::Register *curTargetReg = current->getRegisterOperand(1);

      if (curSourceReg == lgrTargetReg && curTargetReg == lgrTargetReg)
         {
         if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Reducing %s [%p] to LGFR.\n", TR::InstOpCode::metadata[cursor->getOpCodeValue()].name, cursor))
            {
            ((TR::S390RRInstruction*)current)->setRegisterOperand(2, lgrSourceReg);

            cursor->remove();

            return true;
            }
         }
      }

   return false;
   }

/** \details
 *     This transformation may not always be possible because the LHI instruction does not modify the condition
 *     code while the XR instruction does. We must be pessimistic in our algorithm and carry out the transformation
 *     if and only if there exists an instruction B that sets the condition code between the LHI instruction A and
 *     some instruction C that reads the condition code.
 *
 *     That is, we are trying to find instruction that comes after the LHI in the execution order that will clobber
 *     the condition code before any instruction that consumes a condition code.
 */
bool
OMR::Z::Peephole::tryToReduceLHIToXR()
  {
  // This optimization is disabled by default because there exist cases in which we cannot determine whether this
  // transformation is functionally valid or not. The issue resides in the various runtime patching sequences using the
  // LHI instruction as a runtime patch point for an offset. One concrete example can be found in the virtual dispatch
  // sequence for unresolved calls on 31-bit platforms where an LHI instruction is used and is patched at runtime.
  //
  // TODO (#255): To enable this optimization we need to implement an API which marks instructions that will be patched
  // at runtime and prevent ourselves from modifying such instructions in any way.
  return false;

  TR::S390RIInstruction* lhiInstruction = static_cast<TR::S390RIInstruction*>(cursor);

  if (lhiInstruction->getSourceImmediate() == 0)
     {
     TR::Instruction* nextInstruction = lhiInstruction->getNext();

     while (nextInstruction != NULL && !nextInstruction->getOpCode().readsCC())
        {
        if (nextInstruction->getOpCode().setsCC() || nextInstruction->getNode()->getOpCodeValue() == TR::BBEnd)
           {
           TR::DebugCounter::incStaticDebugCounter(self()->comp(), "z/peephole/LHI/XR");

           TR::Instruction* xrInstruction = generateRRInstruction(self()->cg(), TR::InstOpCode::XR, lhiInstruction->getNode(), lhiInstruction->getRegisterOperand(1), lhiInstruction->getRegisterOperand(1));
           self()->cg()->replaceInst(lhiInstruction, xrInstruction);

           return true;
           }

        nextInstruction = nextInstruction->getNext();
        }
     }

  return false;
  }

bool
OMR::Z::Peephole::tryToReduceLLCToLLGC()
   {
   TR::Instruction *nextInst = cursor->getNext();
   auto mnemonic = nextInst->getOpCodeValue();

   if (mnemonic == TR::InstOpCode::LGFR || mnemonic == TR::InstOpCode::LLGTR)
      {
      TR::Register *llcTgtReg = ((TR::S390RRInstruction *) cursor)->getRegisterOperand(1);

      TR::Register *nextSrcReg = ((TR::S390RRInstruction *) nextInst)->getRegisterOperand(2);
      TR::Register *nextTgtReg = ((TR::S390RRInstruction *) nextInst)->getRegisterOperand(1);

      if (llcTgtReg == nextSrcReg && llcTgtReg == nextTgtReg)
         {
         if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Reducing LLC/%s [%p] to LLGC.\n", TR::InstOpCode::metadata[mnemonic].name, nextInst))
            {
            // Remove the LGFR/LLGTR
            nextInst->remove();

            // Replace the LLC with LLGC
            TR::MemoryReference* memRef = ((TR::S390RXInstruction *) cursor)->getMemoryReference();
            memRef->resetMemRefUsedBefore();
            auto llgcInst = generateRXInstruction(self()->cg(), TR::InstOpCode::LLGC, cursor->getNode(), llcTgtReg, memRef, cursor->getPrev());
            self()->cg()->replaceInst(cursor, llgcInst);

            return true;
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToReduceLRCHIToLTR()
   {
   int32_t windowSize = 0;
   const int32_t maxWindowSize = 10;

   // The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg
   cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);
   TR::InstOpCode lgrOpCode = cursor->getOpCode();

   TR::Instruction * current = cursor->getNext();

   while ((current != NULL) &&
           !isBarrierToPeepHoleLookback(current) &&
           !(current->isBranchOp() && current->getKind() == TR::Instruction::IsRIL &&
              ((TR::S390RILInstruction *)current)->getTargetSnippet()) &&
           windowSize < maxWindowSize)
      {
      // Do not look across Transactional Regions, the register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         break;
         }

      TR::InstOpCode curOpCode = current->getOpCode();
      current->setUseDefRegisters(false);
      // if we encounter the CHI GPRx, 0, attempt the transformation the LR->LTR
      // and remove the CHI GPRx, 0
      if ((curOpCode.getOpCodeValue() == TR::InstOpCode::CHI || curOpCode.getOpCodeValue() == TR::InstOpCode::CGHI) &&
            ((curOpCode.is32bit() && lgrOpCode.is32bit()) ||
             (curOpCode.is64bit() && lgrOpCode.is64bit())))
         {
         TR::Register *curTargetReg=((TR::S390RIInstruction*)current)->getRegisterOperand(1);
         int32_t srcImm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
         if (curTargetReg == lgrTargetReg && srcImm == 0)
            {
            if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Transforming LR/CHI to LTR at %p\n", cursor))
               {
               auto ltrInst = generateRRInstruction(self()->cg(), lgrOpCode.is64bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, cursor->getNode(), lgrTargetReg, lgrSourceReg, cursor->getPrev());

               self()->cg()->replaceInst(cursor, ltrInst);
               current->remove();

               return true;
               }
            }
         }

      // Ensure we do not clobber the CC set by another instruction
      if (curOpCode.setsCC() || curOpCode.readsCC())
         break;

      // Ensure the compare acts on the correct register values
      if (current->isDefRegister(lgrSourceReg) ||
          current->isDefRegister(lgrTargetReg))
         break;

      current = current->getNext();

      windowSize++;
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToReduceLTRToCHI()
   {
   // The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg.
   cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);
   TR::InstOpCode lgrOpCode = cursor->getOpCode();

   if (lgrTargetReg == lgrSourceReg &&
      (lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTGR))
      {
      if (seekRegInFutureMemRef(cursor, 4, lgrTargetReg))
         {
         if (performTransformation(self()->comp(), "\nO^O S390 PEEPHOLE: Eliminating AGI by transforming %s [%p] to a compare halfword immediate.\n", TR::InstOpCode::metadata[cursor->getOpCodeValue()].name, cursor))
            {
            auto chiInst = generateRIInstruction(self()->cg(), TR::InstOpCode::getCmpHalfWordImmOpCode(), cursor->getNode(), lgrTargetReg, 0, cursor->getPrev());

            self()->cg()->replaceInst(cursor, chiInst);

            return true;
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveDuplicateLR()
   {
   // The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg
   cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);
   TR::InstOpCode lgrOpCode = cursor->getOpCode();

   if (lgrTargetReg == lgrSourceReg &&
      (lgrOpCode.getOpCodeValue() == TR::InstOpCode::LR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LGR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LDR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LER))
       {
       if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Removing redundant %s [%p]\n", TR::InstOpCode::metadata[cursor->getOpCodeValue()].name, cursor))
          {
          cursor->remove();

          return true;
          }
       }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveDuplicateLoadRegister()
   {
   bool performed = false;
   int32_t windowSize = 0;
   const int32_t maxWindowSize = 20;

   //The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg.
   cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);
   TR::InstOpCode lgrOpCode = cursor->getOpCode();

   TR::Instruction * current = cursor->getNext();

   // In order to remove LTR's, we need to ensure that there are no
   // instructions that set CC or read CC.
   bool lgrSetCC = lgrOpCode.setsCC();
   bool setCC = false, useCC = false;

   while ((current != NULL) &&
           !isBarrierToPeepHoleLookback(current) &&
           !(current->isBranchOp() && current->getKind() == TR::Instruction::IsRIL &&
              ((TR::S390RILInstruction *)current)->getTargetSnippet()) &&
           windowSize < maxWindowSize)
      {

      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         break;
         }

      TR::InstOpCode curOpCode = current->getOpCode();
      current->setUseDefRegisters(false);

      if (curOpCode.getOpCodeValue() == lgrOpCode.getOpCodeValue() &&
          current->getKind() == TR::Instruction::IsRR)
         {
         TR::Instruction* rrInst = current;
         TR::Register *curSourceReg = rrInst->getRegisterOperand(2);
         TR::Register *curTargetReg = rrInst->getRegisterOperand(1);

         if ( ((curSourceReg == lgrTargetReg && curTargetReg == lgrSourceReg) ||
              (curSourceReg == lgrSourceReg && curTargetReg == lgrTargetReg)))
            {
            // We are either replacing LR/LGR (lgrSetCC won't be set)
            // or if we are modifying LTR/LGTR, then no instruction can
            // set or read CC between our original and current instruction.

            if ((!lgrSetCC || !(setCC || useCC)))
               {
               if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Duplicate LR/CPYA removal at %p\n", rrInst))
                  {
                  performed = true;
                  current = current->getNext();
                  windowSize = 0;
                  setCC = setCC || current->getOpCode().setsCC();
                  useCC = useCC || current->getOpCode().readsCC();

                  rrInst->remove();

                  continue;
                  }
               }
            }
         }

      // Flag if current instruction sets or reads CC -> used to determine
      // whether LTR/LGTR transformation is valid.
      setCC = setCC || curOpCode.setsCC();
      useCC = useCC || curOpCode.readsCC();

      // If instruction overwrites either of the original source and target registers,
      // we cannot remove any duplicates, as register contents may have changed.
      if (current->isDefRegister(lgrSourceReg) ||
          current->isDefRegister(lgrTargetReg))
         break;

      current = current->getNext();
      windowSize++;
      }

   return performed;
   }

bool
OMR::Z::Peephole::tryToRemoveDuplicateNILF()
   {
   if (cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILF)
      {
      TR::S390RILInstruction* currInst = static_cast<TR::S390RILInstruction*>(cursor);
      TR::S390RILInstruction* nextInst = static_cast<TR::S390RILInstruction*>(cursor->getNext());

      bool instructionsMatch =
         (currInst->getTargetPtr() == nextInst->getTargetPtr()) &&
         (currInst->getTargetSnippet() == nextInst->getTargetSnippet()) &&
         (currInst->getTargetSymbol() == nextInst->getTargetSymbol()) &&
         (currInst->getTargetLabel() == nextInst->getTargetLabel()) &&
         (currInst->getMask() == nextInst->getMask()) &&
         (currInst->getSymbolReference() == nextInst->getSymbolReference());

      if (instructionsMatch)
         {
         if (currInst->matchesTargetRegister(nextInst->getRegisterOperand(1)) &&
            nextInst->matchesTargetRegister(currInst->getRegisterOperand(1)))
            {
            if (currInst->getSourceImmediate() == nextInst->getSourceImmediate())
               {
               if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: deleting duplicate NILF from pair %p %p*\n", currInst, nextInst))
                  {
                  nextInst->remove();

                  return true;
                  }
               }
            // To perform
            //
            // NILF     @05,X'000000C0'
            // NILF     @05,X'000000FF'
            //
            // ->
            //
            // NILF     @05,X'000000C0'
            //
            // test if
            // ((C0 & FF) == C0) && ((FF & C0) != FF)
            //
            // want to remove the second unnecessary instruction
            //
            else if (((currInst->getSourceImmediate() & nextInst->getSourceImmediate()) == currInst->getSourceImmediate()) &&
               ((nextInst->getSourceImmediate() & currInst->getSourceImmediate()) != nextInst->getSourceImmediate()))
               {
               if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: deleting unnecessary NILF from pair %p %p*\n", currInst, nextInst))
                  {
                  nextInst->remove();

                  return true;
                  }
               }
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveDuplicateNILH()
   {
   if (cursor->getNext()->getKind() == TR::Instruction::IsRI)
      {
      TR::S390RIInstruction* currInst = static_cast<TR::S390RIInstruction*>(cursor);
      TR::S390RIInstruction* nextInst = static_cast<TR::S390RIInstruction*>(cursor->getNext());

      if (currInst->isImm() == nextInst->isImm())
         {
         if (currInst->isImm())
            {
            if (currInst->getSourceImmediate() == nextInst->getSourceImmediate())
               {
               if (currInst->matchesTargetRegister(nextInst->getRegisterOperand(1)) &&
                   nextInst->matchesTargetRegister(currInst->getRegisterOperand(1)))
                  {
                  if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: deleting duplicate NILH from pair %p %p*\n", currInst, nextInst))
                     {
                     nextInst->remove();

                     return true;
                     }
                  }
               }
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveRedundantCompareAndTrap()
   {
   if (self()->comp()->target().isZOS())
      {
      // CLT cannot do the job in zOS because in zOS it is legal to read low memory address (like 0x000000, literally NULL),
      // and CLT will read the low memory address legally (in this case NULL) to compare it with the other operand.
      // The result will not make sense since we should trap for NULLCHK first.
      return false;
      }

   int32_t windowSize=0;
   const int32_t maxWindowSize=8;
   static char *disableRemoveMergedNullCHK = feGetEnv("TR_DisableRemoveMergedNullCHK");

   if (disableRemoveMergedNullCHK != NULL) return false;

   TR::Instruction * cgitInst = cursor;
   TR::Instruction * current = cgitInst->getNext();

   cgitInst->setUseDefRegisters(false);
   TR::Register * cgitSource = cgitInst->getSourceRegister(0);

   while ((current != NULL) &&
         !isBarrierToPeepHoleLookback(current) &&
         windowSize<maxWindowSize)
      {
      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      if (current->getOpCodeValue() == TR::InstOpCode::CLT)
          {
          TR::Instruction * cltInst = current;
          cltInst->setUseDefRegisters(false);
          TR::Register * cltSource = cltInst->getSourceRegister(0);
          TR::Register * cltSource2 = cltInst->getSourceRegister(1);
          if (!cgitSource || !cltSource || !cltSource2)
             return false;
          if (toRealRegister(cltSource2)->getRegisterNumber() == toRealRegister(cgitSource)->getRegisterNumber())
             {
             if (performTransformation(self()->comp(), "\nO^O S390 PEEPHOLE: removeMergedNullCHK on 0x%p.\n", cgitInst))
                {
                cgitInst->remove();

                return true;
                }
             }
          }
       else if (current->usesRegister(cgitSource))
          {
          return false;
          }

      current = current->getNext() == NULL ? NULL : current->getNext();
      windowSize++;
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveRedundantLA()
   {
   TR::Register *laTargetReg = cursor->getRegisterOperand(1);
   TR::MemoryReference *mr = cursor->getMemoryReference();
   TR::Register *laBaseReg = NULL;
   TR::Register *laIndexReg = NULL;
   TR::SymbolReference *symRef = NULL;
   if (mr)
      {
      laBaseReg = mr->getBaseRegister();
      laIndexReg = mr->getIndexRegister();
      symRef = mr->getSymbolReference();
      }

   if (mr &&
         mr->getOffset() == 0 &&
         laBaseReg && laTargetReg &&
         laBaseReg == laTargetReg &&
         laIndexReg == NULL &&
         (symRef == NULL || symRef->getOffset() == 0) &&
         (symRef == NULL || symRef->getSymbol() == NULL))
      {
      if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Removing redundant LA [%p].\n", cursor))
         {
         cursor->remove();

         return true;
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveRedundantShift()
   {
   TR::Instruction* currInst = cursor;
   TR::Instruction* nextInst = cursor->getNext();

   if (currInst->getOpCodeValue() == nextInst->getOpCodeValue())
      {
      if ((currInst->getKind() == TR::Instruction::IsRS) && (nextInst->getKind() == TR::Instruction::IsRS))
         {
         TR::S390RSInstruction* currRSInst = static_cast<TR::S390RSInstruction*>(currInst);
         TR::S390RSInstruction* nextRSInst = static_cast<TR::S390RSInstruction*>(nextInst);

         bool instrMatch =
            (currRSInst->getFirstRegister() == nextRSInst->getFirstRegister()) &&
            (!currRSInst->hasMaskImmediate()) && (!nextRSInst->hasMaskImmediate()) &&
            (currRSInst->getMemoryReference() == NULL) && (nextRSInst->getMemoryReference() == NULL);

         if (instrMatch)
            {
            uint32_t newShift = currRSInst->getSourceImmediate() + nextRSInst->getSourceImmediate();
            if (newShift < 64)
               {
               if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: merging SRL/SLL pair [%p] [%p]\n", cursor, cursor->getNext()))
                  {
                  currRSInst->setSourceImmediate(newShift);
                  nextInst->remove();

                  return true;
                  }
               }
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveRedundantLR()
   {
   // The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg
   cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);

   if (lgrTargetReg == lgrSourceReg)
      {
      TR::Instruction* nextInst = cursor->getNext();

      if (nextInst->getOpCodeValue() == TR::InstOpCode::LTR ||
          nextInst->getOpCodeValue() == TR::InstOpCode::LTGR)
        {
        TR::Register *ltgrTargetReg = nextInst->getRegisterOperand(1);
        TR::Register *ltgrSourceReg = nextInst->getRegisterOperand(2);

        if (lgrTargetReg == ltgrTargetReg || lgrTargetReg == ltgrSourceReg)
           {
           if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Removing redundant %s [%p] which is followed by a load and test register.\n", TR::InstOpCode::metadata[cursor->getOpCodeValue()].name, cursor))
              {
              cursor->remove();

              return true;
              }
           }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveRedundantLTR()
   {
   // The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg
   cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = cursor->getRegisterOperand(2);
   TR::Register *lgrTargetReg = cursor->getRegisterOperand(1);

   if (lgrTargetReg == lgrSourceReg)
      {
      TR::Instruction *prevInst = cursor->getPrev();
      TR::Instruction *nextInst = cursor->getNext();

      // Note that this is also done by isActiveLogicalCC, and the end of generateS390CompareAndBranchOpsHelper when
      // the virtual registers match, but those cannot handle the case when the virtual registers are not the same but
      // we do have the same restricted register, which is why we are handling it here when all the register assignment
      // is done, and the redundant LR's from the clobber evaluate of the add/sub logical are cleaned up as well
      if (prevInst->getOpCode().setsCC() && prevInst->getOpCode().setsCarryFlag() && prevInst->getRegisterOperand(1) == lgrTargetReg && nextInst->getOpCodeValue() == TR::InstOpCode::BRC)
         {
         TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) nextInst)->getBranchCondition();

         if (branchCond == TR::InstOpCode::COND_BE || branchCond == TR::InstOpCode::COND_BNE)
            {
            if (performTransformation(self()->comp(), "O^O S390 PEEPHOLE: Removing redundant Load and Test instruction at %p, because CC can be reused from logical instruction %p\n", cursor, prevInst))
               {
               cursor->remove();

               if (branchCond == TR::InstOpCode::COND_BE)
                  ((TR::S390BranchInstruction *) nextInst)->setBranchCondition(TR::InstOpCode::COND_MASK10);
               else if (branchCond == TR::InstOpCode::COND_BNE)
                  ((TR::S390BranchInstruction *) nextInst)->setBranchCondition(TR::InstOpCode::COND_MASK5);

               return true;
               }
            }
         }
      }

   return false;
   }

bool
OMR::Z::Peephole::tryToRemoveRedundant32To64BitExtend(bool isSigned)
   {
   static const bool disableRemoveExtend = feGetEnv("TR_DisableRemoveRedundant32to64Extend") != NULL;
   if (disableRemoveExtend)
      {
      return false;
      }

   int32_t windowSize = 0;
   const int32_t maxWindowSize = 10;

   const char *lgfrMnemonicName = isSigned ? "LGFR" : "LLGFR";
   TR::Compilation *comp = self()->comp();
   TR::Instruction *lgfr = cursor;
   TR::Register *lgfrReg = lgfr->getRegisterOperand(1);

   if (lgfrReg != lgfr->getRegisterOperand(2))
      return false;

   TR::Instruction *current = lgfr->getPrev();

   while ((current != NULL) &&
      !isBarrierToPeepHoleLookback(current) &&
      windowSize < maxWindowSize)
      {
      TR::InstOpCode::Mnemonic curOpMnemonic = current->getOpCode().getMnemonic();

      if (current->getNumRegisterOperands() > 0 && lgfrReg == current->getRegisterOperand(1))
         {
         TR::MemoryReference *mr = NULL;
         TR::Instruction *replacement = NULL;
         switch (curOpMnemonic)
            {
            case TR::InstOpCode::L:
               if (performTransformation(comp, "O^O S390 PEEPHOLE: Merging L [%p] and %s [%p] into %s.\n",
                     current, lgfrMnemonicName, lgfr, isSigned ? "LGF" : "LLGF"))
                  {
                  mr = current->getMemoryReference();
                  mr->resetMemRefUsedBefore();
                  replacement = generateRXInstruction(self()->cg(), isSigned ? TR::InstOpCode::LGF : TR::InstOpCode::LLGF, current->getNode(), lgfrReg, mr, current->getPrev());
                  }
               break;
            case TR::InstOpCode::LH:
               if (isSigned && performTransformation(comp, "O^O S390 PEEPHOLE: Merging LH [%p] and LGFR [%p] into LGH.\n", current, lgfr))
                  {
                  mr = current->getMemoryReference();
                  mr->resetMemRefUsedBefore();
                  replacement = generateRXInstruction(self()->cg(), TR::InstOpCode::LGH, current->getNode(), lgfrReg, mr, current->getPrev());
                  }
               break;
            case TR::InstOpCode::LLH:
               if (performTransformation(comp, "O^O S390 PEEPHOLE: Merging LLH [%p] and %s [%p] into LLGH.\n", current, lgfrMnemonicName, lgfr))
                  {
                  mr = current->getMemoryReference();
                  mr->resetMemRefUsedBefore();
                  replacement = generateRXInstruction(self()->cg(), TR::InstOpCode::LLGH, current->getNode(), lgfrReg, mr, current->getPrev());
                  }
               break;
            case TR::InstOpCode::LB:
               if (isSigned && performTransformation(comp, "O^O S390 PEEPHOLE: Merging LB [%p] and LGFR [%p] into LGB.\n", current, lgfr))
                  {
                  mr = current->getMemoryReference();
                  mr->resetMemRefUsedBefore();
                  replacement = generateRXInstruction(self()->cg(), TR::InstOpCode::LGB, current->getNode(), lgfrReg, mr, current->getPrev());
                  }
               break;
            case TR::InstOpCode::LLC:
               if (performTransformation(comp, "O^O S390 PEEPHOLE: Merging LLC [%p] and %s [%p] into LLGC.\n", current, lgfrMnemonicName, lgfr))
                  {
                  mr = current->getMemoryReference();
                  mr->resetMemRefUsedBefore();
                  replacement = generateRXInstruction(self()->cg(), TR::InstOpCode::LLGC, current->getNode(), lgfrReg, mr, current->getPrev());
                  }
               break;

            case TR::InstOpCode::XR:
               // The following sequence of instructions
               //    XR          GPR1, GPR1  ; Zero out bottom 32 bits of GPR1
               //    LGFR/LLGFR  GPR1, GPR1  ; Extend those zeros to all 64 bits of GPR1
               // Can be converted to
               //    XGR         GPR1, GPR1  ; Zero out all 64 bits of GPR1
               if (lgfrReg == current->getRegisterOperand(2) &&
                   performTransformation(comp, "O^O S390 PEEPHOLE: Merging XR [%p] and %s [%p] into XGR.\n", current, lgfrMnemonicName, lgfr))
                  replacement = generateRRInstruction(self()->cg(), TR::InstOpCode::XGR, current->getNode(), lgfrReg, lgfrReg, current->getPrev());
               break;
            case TR::InstOpCode::IILF:
               if (performTransformation(comp, "O^O S390 PEEPHOLE: Merging IILF [%p] and %s [%p] into %s.\n", current, lgfrMnemonicName, lgfr, isSigned ? "LGFI" : "LLILF"))
                  replacement = generateRILInstruction(self()->cg(), isSigned ? TR::InstOpCode::LGFI : TR::InstOpCode::LLILF, current->getNode(), lgfrReg, toS390RILInstruction(current)->getSourceImmediate(), current->getPrev());
               break;
            case TR::InstOpCode::LHI:
               if (isSigned && performTransformation(comp, "O^O S390 PEEPHOLE: Merging LHI [%p] and LGFR [%p] into LGH.\n", current, lgfr))
                  {
                  replacement = generateRIInstruction(self()->cg(), TR::InstOpCode::LGHI, current->getNode(), lgfrReg, toS390RIInstruction(current)->getSourceImmediate(), current->getPrev());
                  }
               else if (performTransformation(comp, "O^O S390 PEEPHOLE: Merging LHI [%p] and LLGFR [%p] into LLILF.\n", current, lgfr))
                  {
                  // The following sequence of instructions:
                  //    LHI      GPR1, IMM   ; sign extend IMM from 16 to 32 bits
                  //    LLGFR    GPR1, GPR1  ; zero extend from 32 to 64 bits
                  // Can be converted to
                  //    LLILF    GPR1, IMM'  ; where IMM' is IMM sign extended from 16 to 32 bits
                  int16_t imm = toS390RIInstruction(current)->getSourceImmediate();
                  replacement = generateRILInstruction(self()->cg(), TR::InstOpCode::LLILF, current->getNode(), lgfrReg, static_cast<int32_t>(imm), current->getPrev());
                  }
               break;

            case TR::InstOpCode::LR:
            case TR::InstOpCode::LGR:
               replacement = generateRRInstruction(self()->cg(), isSigned ? TR::InstOpCode::LGFR : TR::InstOpCode::LLGFR, current->getNode(), lgfrReg, current->getRegisterOperand(2), current->getPrev());
               break;
            }

         if (replacement != NULL)
            {
            TR::DebugCounter::incStaticDebugCounter(comp,
               TR::DebugCounter::debugCounterName(comp, "z/peephole/redundant32To64BitExtend/%s/%s/%s/(%s)",
                  current->getOpCode().getMnemonicName(),
                  lgfr->getOpCode().getMnemonicName(),
                  replacement->getOpCode().getMnemonicName(),
                  comp->signature()));
            self()->cg()->replaceInst(current, replacement);
            lgfr->remove();
            return true;
            }
         }

      // Ensure the extend acts on the correct register values
      if (current->isDefRegister(lgfrReg))
         break;

      current = current->getPrev();

      windowSize++;
      }

   return false;
   }
