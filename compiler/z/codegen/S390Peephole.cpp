/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "z/codegen/S390Peephole.hpp"

#include <stdint.h>                                 // for int32_t, etc
#include <stdio.h>                                  // for printf, sprintf
#include <string.h>                                 // for NULL, strcmp, etc
#include "codegen/CodeGenPhase.hpp"                 // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction, etc
#include "codegen/MemoryReference.hpp"              // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/Snippet.hpp"                      // for TR::S390Snippet, etc
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol
#include "ras/Debug.hpp"                            // for TR_DebugBase, etc
#include "ras/DebugCounter.hpp"
#include "ras/Delimiter.hpp"                        // for Delimiter
#include "runtime/Runtime.hpp"                      // for TR_LinkageInfo
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "z/codegen/TRSystemLinkage.hpp"

TR_S390Peephole::TR_S390Peephole(TR::Compilation* comp, TR::CodeGenerator *cg)
   : _fe(comp->fe()),
     _outFile(comp->getOutFile()),
     _cursor(cg->getFirstInstruction()),
     _cg(cg)
   {
   }

TR_S390PreRAPeephole::TR_S390PreRAPeephole(TR::Compilation* comp, TR::CodeGenerator *cg)
   : TR_S390Peephole(comp, cg)
   {
   }

void
TR_S390PreRAPeephole::perform()
   {
   TR::Delimiter d(comp(), comp()->getOption(TR_TraceCG), "preRAPeephole");

   if (comp()->getOption(TR_TraceCG))
      printInfo("\nPost PreRegister Assignment Peephole Optimization Instructions:\n");

   while (_cursor != NULL)
      {
      switch(_cursor->getOpCodeValue())
         {
         case TR::InstOpCode::L:
            {
            attemptLoadStoreReduction(TR::InstOpCode::ST, 4);
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }
            break;
            }
         case TR::InstOpCode::LFH:
            {
            attemptLoadStoreReduction(TR::InstOpCode::STFH, 4);
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }
            break;
            }
         case TR::InstOpCode::LG:
            {
            attemptLoadStoreReduction(TR::InstOpCode::STG, 8);
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }
            break;
            }
         default:
            {
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }
            break;
            }
         }

      _cursor = _cursor->getNext();
      }

   if (comp()->getOption(TR_TraceCG))
      printInfo("\n\n");
   }

bool
TR_S390PreRAPeephole::attemptLoadStoreReduction(TR::InstOpCode::Mnemonic storeOpCode, uint16_t size)
   {
   if (_cursor->getNext()->getOpCodeValue() == storeOpCode)
      {
      TR::S390RXInstruction* loadInst = static_cast<TR::S390RXInstruction*> (_cursor);
      TR::S390RXInstruction* storeInst = static_cast<TR::S390RXInstruction*> (_cursor->getNext());

      // Cannot change to MVC if symbol references are unresolved, MVC doesn't follow same format as load and store so patching won't work
      if ( loadInst->getMemoryReference()->getSymbolReference() && loadInst->getMemoryReference()->getSymbolReference()->isUnresolved() ||
          storeInst->getMemoryReference()->getSymbolReference() && storeInst->getMemoryReference()->getSymbolReference()->isUnresolved())
         {
         return false;
         }

      if (!loadInst->getMemoryReference()->getBaseRegister() && !loadInst->getMemoryReference()->getIndexRegister() ||
          !storeInst->getMemoryReference()->getBaseRegister() && !storeInst->getMemoryReference()->getIndexRegister())
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
      uses += reg == loadInst->getMemoryReference()->getBaseRegister();

      // Bail out if register could have future uses, or if either instruction has dependencies.
      // Since 2 instructions are replaced with one before RA, we can't handle things like merging
      // dependencies.
      if (reg->getTotalUseCount() != uses || loadInst->getDependencyConditions() || storeInst->getDependencyConditions())
         {
         return false;
         }

      if (performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming load-store sequence at %p to MVC.", storeInst))
         {
         TR::DebugCounter::incStaticDebugCounter(_cg->comp(), "z/peephole/load-store");

         loadInst->getMemoryReference()->resetMemRefUsedBefore();
         storeInst->getMemoryReference()->resetMemRefUsedBefore();

         reg->decTotalUseCount(2);

         if (loadInst->getMemoryReference()->getBaseRegister())
            {
            loadInst->getMemoryReference()->getBaseRegister()->decTotalUseCount();
            }

         if (storeInst->getMemoryReference()->getBaseRegister())
            {
            storeInst->getMemoryReference()->getBaseRegister()->decTotalUseCount();
            }

         if (loadInst->getMemoryReference()->getIndexRegister())
            {
            loadInst->getMemoryReference()->getIndexRegister()->decTotalUseCount();
            }

         if (storeInst->getMemoryReference()->getIndexRegister())
            {
            storeInst->getMemoryReference()->getIndexRegister()->decTotalUseCount();
            }

         TR::Instruction * newInst = generateSS1Instruction(_cg, TR::InstOpCode::MVC, comp()->getStartTree()->getNode(), size - 1, storeInst->getMemoryReference(), loadInst->getMemoryReference(), _cursor->getPrev());

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

         _cg->deleteInst(storeInst);

         _cg->replaceInst(loadInst, _cursor = newInst);

         return true;
         }
      }
   return false;
   }

///////////////////////////////////////////////////////////////////////////////

TR::Instruction* realInstruction(TR::Instruction* inst, bool forward)
   {
   while (inst && (inst->getKind() == TR::Instruction::IsPseudo      ||
                   inst->getKind() == TR::Instruction::IsNotExtended ||
                   inst->getKind() == TR::Instruction::IsLabel))
      {
      inst = forward ? inst->getNext() : inst->getPrev();
      }

   return inst;
   }

TR::Instruction* realInstructionWithLabels(TR::Instruction* inst, bool forward)
   {
   while (inst && (inst->getKind() == TR::Instruction::IsPseudo ||
                   inst->getKind() == TR::Instruction::IsNotExtended))
      {
      inst = forward ? inst->getNext() : inst->getPrev();
      }

   return inst;
   }

TR::Instruction* realInstructionWithLabelsAndRET(TR::Instruction* inst, bool forward)
   {
   while (inst && (inst->getKind() == TR::Instruction::IsPseudo ||
                   inst->getKind() == TR::Instruction::IsNotExtended) && !inst->isRet())
      {
      inst = forward ? inst->getNext() : inst->getPrev();
      }

   return inst;
   }

///////////////////////////////////////////////////////////////////////////////

TR_S390PostRAPeephole::TR_S390PostRAPeephole(TR::Compilation* comp, TR::CodeGenerator *cg)
   : TR_S390Peephole(comp, cg)
   {
   }

bool
TR_S390PostRAPeephole::isBarrierToPeepHoleLookback(TR::Instruction *current)
   {
   if (NULL == current) return true;

   TR::Instruction *s390current = current;

   if (s390current->isLabel()) return true;
   if (s390current->isCall())  return true;
   if (s390current->isBranchOp()) return true;
   if (s390current->getOpCodeValue() == TR::InstOpCode::DCB) return true;

   return false;
   }

bool
TR_S390PostRAPeephole::AGIReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable AGIReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed=false;
   int32_t windowSize=0;
   const int32_t MaxWindowSize=8; // Look down 8 instructions (4 cycles)
   const int32_t MaxLAWindowSize=6; // Look up 6 instructions (3 cycles)
   static char *disableRenaming = feGetEnv("TR_DisableRegisterRenaming");

   TR::InstOpCode::Mnemonic curOp = (_cursor)->getOpCodeValue();

   // LR Reduction or LGFR reduction could have changed the original instruction
   // make sure AGI reduction only works on these OpCodes
   if (!(curOp == TR::InstOpCode::LR || curOp == TR::InstOpCode::LTR ||
         curOp == TR::InstOpCode::LGR || curOp == TR::InstOpCode::LTGR))
      {
      return false;
      }

   if (disableRenaming!=NULL)
      return false;

   TR::Register *lgrTargetReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(1);
   TR::Register *lgrSourceReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(2);

   TR::RealRegister *gpr0 = _cg->machine()->getS390RealRegister(TR::RealRegister::GPR0);

   // no renaming possible if both target and source are the same
   // this can happend with LTR and LTGR
   // or if source reg is gpr0
   if  (toRealRegister(lgrSourceReg)==gpr0 || (lgrTargetReg==lgrSourceReg) ||
        toRealRegister(lgrTargetReg)==_cg->getStackPointerRealRegister(NULL))
      return performed;

   TR::Instruction* current=_cursor->getNext();

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
         // do it for all memory reference
         while (mr)
            {
            if (mr && mr->getBaseRegister()==lgrTargetReg)
               {
               // here we would like to swap registers, see if we can
               if(!reachedLabel && !reachedBranch && !sourceRegInvalid)
                  {
                  if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
                  if(performTransformation(comp(), "O^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
                     {
                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nchanging base reg of ");
                        printInstr(comp(), current);
                        }
                     // Update Base Register
                     mr->setBaseRegister(lgrSourceReg, _cg);

                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nto");
                        printInstr(comp(), current);
                        }
                     performed = true;
                     }
                  }
               else // couldn't swap regs, but potentially can use LA
                  {
                  attemptLA = true;
                  }
               }
            if (mr && mr->getIndexRegister()==lgrTargetReg)
               {
               // here we would like to swap registers, see if we can
               if(!reachedLabel && !reachedBranch && !sourceRegInvalid)
                  {
                  if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
                  if(performTransformation(comp(), "O^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
                     {
                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nchanging index reg of ");
                        printInstr(comp(), current);
                        }
                     // Update Index Register
                     mr->setIndexRegister(lgrSourceReg);

                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nto");
                        printInstr(comp(), current);
                        }
                     performed=true;
                     }
                  }
               else // couldn't swap regs, but potentially can use LA
                  {
                  attemptLA = true;
                  }
               }
            if (mr == current->getMemoryReference())
               mr = current->getMemoryReference2();
            else
               mr = NULL;
            }
         }

         // If Current instruction invalidates the source register, subsequent instructions' memref cannot be renamed.
         sourceRegInvalid = sourceRegInvalid || current->matchesTargetRegister(lgrSourceReg);

         current=current->getNext();
         windowSize++;
      }

   // We can switch the _cursor to instruction to LA if:
   // 1. The opcode is LGR
   // 2. The target is 64-bit (because LA sets the upppermost bit to 0 on 31-bit)
   attemptLA &= _cursor->getOpCodeValue() == TR::InstOpCode::LGR && TR::Compiler->target.is64Bit();
   if (attemptLA)
      {
      // in order to switch the instruction to LA, we check to see that
      // we are eliminating the AGI, not just propagating it up in the code
      // so we check for labels, source register invalidation, and routine calls
      // (basically the opposite of the above while loop condition)
      current = _cursor->getPrev();
      windowSize = 0;
      while ((current != NULL) &&
      !isBarrierToPeepHoleLookback(current) &&
      !current->matchesTargetRegister(lgrSourceReg) &&
      windowSize<MaxLAWindowSize)
         {
         current = current->getPrev();
         windowSize++;
         }

      // if we reached the end of the loop and didn't find a conflict, switch the instruction to LA
      if (windowSize==MaxLAWindowSize && performTransformation(comp(), "\nO^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
            {
            if (comp()->getOption(TR_TraceCG))
               {
               printInstr(comp(), _cursor);
               printInfo("\nAGI Reduction by switching the above instruction to LA");
               }

            // create new instruction and delete old one
            TR::MemoryReference* mr = generateS390MemoryReference(lgrSourceReg, 0, _cg);
            TR::Instruction* oldCursor = _cursor;
            _cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, comp()->getStartTree()->getNode(), lgrTargetReg, mr, _cursor->getPrev());
            _cursor->setNext(oldCursor->getNext());
            _cg->deleteInst(oldCursor);

            performed = true;
            }
         }
      }

   return performed;
   }

/**
 * \brief Swaps guarded storage loads with regular loads and a software read barrier
 *
 * \details
 * This function swaps LGG/LLGFSG with regular loads and software read barrier sequence
 * for runs with -Xgc:concurrentScavenge on hardware that doesn't support guarded storage facility.
 * The sequence first checks if concurrent scavange is in progress and if the current object pointer is
 * in the evacuate space then calls the GC helper to update the object pointer.
 */

bool
TR_S390PostRAPeephole::replaceGuardedLoadWithSoftwareReadBarrier()
   {
   if (!TR::Compiler->om.shouldReplaceGuardedLoadWithSoftwareReadBarrier())
      {
      return false;
      }

   auto* concurrentScavangeNotActiveLabel = generateLabelSymbol(_cg);
   TR::S390RXInstruction *load = static_cast<TR::S390RXInstruction*> (_cursor);
   TR::MemoryReference *loadMemRef = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);
   TR::Register *loadTargetReg = _cursor->getRegisterOperand(1);
   TR::Register *vmReg = _cg->getLinkage()->getMethodMetaDataRealRegister();
   TR::Register *raReg = _cg->machine()->getS390RealRegister(_cg->getReturnAddressRegister());
   TR::Instruction* prev = load->getPrev();

   // If guarded load target and mem ref registers are the same,
   // preserve the register before overwriting it, since we need to repeat the load after calling the GC helper.
   bool shouldPreserveLoadReg = (loadMemRef->getBaseRegister() == loadTargetReg);
   if (shouldPreserveLoadReg) {
      TR::MemoryReference *gsIntermediateAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSIntermediateResultOffset(comp()), _cg);
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, load->getNode(), loadTargetReg, gsIntermediateAddrMemRef, prev);
      prev = _cursor;
   }

   if (load->getOpCodeValue() == TR::InstOpCode::LGG)
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, load->getNode(), loadTargetReg, loadMemRef, prev);
      }
   else
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGF, load->getNode(), loadTargetReg, loadMemRef, prev);
      _cursor = generateRSInstruction(_cg, TR::InstOpCode::SLLG, load->getNode(), loadTargetReg, loadTargetReg, TR::Compiler->om.compressedReferenceShift(), _cursor);
      }

   // Check if concurrent scavange is in progress and if object pointer is in the evacuate space
   TR::MemoryReference *privFlagMR = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetConcurrentScavengeActiveByteAddressOffset(comp()), _cg);
   _cursor = generateSIInstruction(_cg, TR::InstOpCode::TM, load->getNode(), privFlagMR, 0x00000002, _cursor);
   _cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   _cursor = generateRXInstruction(_cg, TR::InstOpCode::CG, load->getNode(), loadTargetReg,
       generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateBaseAddressOffset(comp()), _cg), _cursor);
   _cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   _cursor = generateRXInstruction(_cg, TR::InstOpCode::CG, load->getNode(), loadTargetReg,
       generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateTopAddressOffset(comp()), _cg), _cursor);
   _cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   // Save result of LA to gsParameters.operandAddr as invokeJ9ReadBarrier helper expects it to be set
   TR::MemoryReference *loadAddrMemRef = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);
   _cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, load->getNode(), loadTargetReg, loadAddrMemRef, _cursor);
   TR::MemoryReference *gsOperandAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSOperandAddressOffset(comp()), _cg);
   _cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, load->getNode(), loadTargetReg, gsOperandAddrMemRef, _cursor);

   // Use raReg to call handleReadBarrier helper, preserve raReg before the call in the load reg
   _cursor = generateRRInstruction(_cg, TR::InstOpCode::LGR, load->getNode(), loadTargetReg, raReg, _cursor);
   TR::MemoryReference *gsHelperAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSHandlerAddressOffset(comp()), _cg);
   _cursor = generateRXYInstruction(_cg, TR::InstOpCode::LG, load->getNode(), raReg, gsHelperAddrMemRef, _cursor);
   _cursor = new (_cg->trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::BASR, load->getNode(), raReg, raReg, _cursor, _cg);
   _cursor = generateRRInstruction(_cg, TR::InstOpCode::LGR, load->getNode(), raReg, loadTargetReg, _cursor);

   if (shouldPreserveLoadReg) {
      TR::MemoryReference * restoreBaseRegAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSIntermediateResultOffset(comp()), _cg);
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, load->getNode(), loadTargetReg, restoreBaseRegAddrMemRef, _cursor);
   }
   // Repeat load as the object pointer got updated by GC after calling handleReadBarrier helper
   TR::MemoryReference *updateLoadMemRef = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);
   if (load->getOpCodeValue() == TR::InstOpCode::LGG)
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, load->getNode(), loadTargetReg, updateLoadMemRef,_cursor);
      }
   else
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGF, load->getNode(), loadTargetReg, updateLoadMemRef, _cursor);
      _cursor = generateRSInstruction(_cg, TR::InstOpCode::SLLG, load->getNode(), loadTargetReg, loadTargetReg, TR::Compiler->om.compressedReferenceShift(), _cursor);
      }

   _cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   _cg->deleteInst(load);

   return true;
   }

///////////////////////////////////////////////////////////////////////////////
bool
TR_S390PostRAPeephole::ICMReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable ICMReduction on 0x%p.\n",_cursor))
         return false;
      }

   // Look for L/LTR instruction pair and reduce to LTR
   TR::Instruction* prev = _cursor->getPrev();
   TR::Instruction* next = _cursor->getNext();
   if (next->getOpCodeValue() != TR::InstOpCode::LTR && next->getOpCodeValue() != TR::InstOpCode::CHI)
      return false;

   bool performed = false;
   bool isICMOpportunity = false;

   TR::S390RXInstruction* load= (TR::S390RXInstruction*) _cursor;

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
      if (next->getRegisterOperand(1) != ((TR::S390RRInstruction*)next)->getRegisterOperand(2)) return false;
      isICMOpportunity = true;
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
      if (chi->getSourceImmediate() != 0) return false;

      isICMOpportunity = true;
      }

   if (isICMOpportunity && performTransformation(comp(), "\nO^O S390 PEEPHOLE: L being reduced to ICM at [%s].\n", _cursor->getName(comp()->getDebug())))
         {
         if (comp()->getOption(TR_TraceCG))
            {
            printInfo("\n\n *** L reduced to ICM.\n\n Old Instructions:");
            printInst();
            _cursor = _cursor->getNext();
            printInst();
            }

      // Prevent reuse of memory reference
      TR::MemoryReference* memcp = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);

      if ((memcp->getBaseRegister() == NULL) &&
          (memcp->getIndexRegister() != NULL))
         {
         memcp->setBaseRegister(memcp->getIndexRegister(), _cg);
         memcp->setIndexRegister(0);
         }

      // Do the reduction - create the icm instruction
      TR::S390RSInstruction* icm = new (_cg->trHeapMemory()) TR::S390RSInstruction(TR::InstOpCode::ICM, load->getNode(), load->getRegisterOperand(1), 0xF, memcp, prev, _cg);
      _cursor = icm;

      // Check if the load has an implicit NULLCHK.  If so, we need to ensure a GCmap is copied.
      if (load->throwsImplicitNullPointerException())
         {
         icm->setNeedsGCMap(0x0000FFFF);
         icm->setThrowsImplicitNullPointerException();
         icm->setGCMap(load->getGCMap());

         TR_Debug * debugObj = _cg->getDebug();
         if (debugObj)
            debugObj->addInstructionComment(icm, "Throws Implicit Null Pointer Exception");
         }

      _cg->replaceInst(load, icm);
      _cg->deleteInst(next);

      if (comp()->getOption(TR_TraceCG))
         {
         printInfo("\n\n New Instruction:");
         printInst();
         printInfo("\n ***\n");
         }

      memcp->stopUsingMemRefRegister(_cg);
      performed = true;
      }

   return performed;
   }

bool
TR_S390PostRAPeephole::LAReduction()
   {
   TR::Instruction *s390Instr = _cursor;
   bool performed = false;
   if (s390Instr->getKind() == TR::Instruction::IsRX)
      {
      TR::Register *laTargetReg = s390Instr->getRegisterOperand(1);
      TR::MemoryReference *mr = s390Instr->getMemoryReference();
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
         // remove LA Rx,0(Rx)
         if (comp()->getOption(TR_TraceCG))
            { printInfo("\n"); }
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: Removing redundant LA (0x%p) with matching target and base registers (%s)\n", _cursor,comp()->getDebug()->getName(laTargetReg)))
            {
            if (comp()->getOption(TR_TraceCG))
               printInstr(comp(), _cursor);
            _cg->deleteInst(_cursor);
            performed = true;
            }
         }
      }
   return performed;
   }

bool
TR_S390PostRAPeephole::duplicateNILHReduction()
   {
   if (_cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILH)
      {
      TR::Instruction * na = _cursor;
      TR::Instruction * nb = _cursor->getNext();

      // want to delete nb if na == nb - use deleteInst

      if ((na->getKind() == TR::Instruction::IsRI) &&
          (nb->getKind() == TR::Instruction::IsRI))
         {
         // NILH == generateRIInstruction
         TR::S390RIInstruction * cast_na = (TR::S390RIInstruction *)na;
         TR::S390RIInstruction * cast_nb = (TR::S390RIInstruction *)nb;

         if (cast_na->isImm() == cast_nb->isImm())
            {
            if (cast_na->isImm())
               {
               if (cast_na->getSourceImmediate() == cast_nb->getSourceImmediate())
                  {
                  if (cast_na->matchesTargetRegister(cast_nb->getRegisterOperand(1)) &&
                      cast_nb->matchesTargetRegister(cast_na->getRegisterOperand(1)))
                     {
                     if (performTransformation(comp(), "O^O S390 PEEPHOLE: deleting duplicate NILH from pair %p %p*\n", _cursor, _cursor->getNext()))
                        {
                        _cg->deleteInst(_cursor->getNext());
                        return true;
                        }
                     }
                  }
               }
            }
         }
      }
   return false;
   }


bool
TR_S390PostRAPeephole::clearsHighBitOfAddressInReg(TR::Instruction *inst, TR::Register *targetReg)
   {
   if (inst->defsRegister(targetReg))
      {
      if (inst->getOpCodeValue() == TR::InstOpCode::LA)
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "LA inst 0x%x clears high bit on targetReg 0x%x (%s)\n", inst, targetReg, targetReg->getRegisterName(comp()));
         return true;
         }

      if (inst->getOpCodeValue() == TR::InstOpCode::LAY)
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "LAY inst 0x%x clears high bit on targetReg 0x%x (%s)\n", inst, targetReg, targetReg->getRegisterName(comp()));
         return true;
         }

      if (inst->getOpCodeValue() == TR::InstOpCode::NILH)
         {
         TR::S390RIInstruction * NILH_RI = (TR::S390RIInstruction *)inst;
         if (NILH_RI->isImm() &&
             NILH_RI->getSourceImmediate() == 0x7FFF)
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "NILH inst 0x%x clears high bit on targetReg 0x%x (%s)\n", inst, targetReg, targetReg->getRegisterName(comp()));
            return true;
            }
         }
      }
   return false;
   }

bool
TR_S390PostRAPeephole::unnecessaryNILHReduction()
   {
   return false;
   }

bool
TR_S390PostRAPeephole::NILHReduction()
   {
   bool transformed = false;

   transformed |= duplicateNILHReduction();
   TR_ASSERT(_cursor->getOpCodeValue() == TR::InstOpCode::NILH, "Expecting duplicateNILHReduction to leave a NILH as current instruction\n");
   transformed |= unnecessaryNILHReduction();

   return transformed;
   }

static TR::Instruction *
findActiveCCInst(TR::Instruction *curr, TR::InstOpCode::Mnemonic op, TR::Register *ccReg)
   {
   TR::Instruction * next = curr;
   while (next = next->getNext())
      {
      if (next->getOpCodeValue() == op)
         return next;
      if (next->usesRegister(ccReg) ||
          next->isLabel() ||
          next->getOpCode().setsCC() ||
          next->isCall() ||
          next->getOpCode().setsCompareFlag())
         break;
      }
   return NULL;
   }

bool
TR_S390PostRAPeephole::branchReduction()
   {
   return false;
   }

/**
 * Peek ahead in instruction stream to see if we find register being used
 * in a memref
 */
bool
TR_S390PostRAPeephole::seekRegInFutureMemRef(int32_t maxWindowSize, TR::Register *targetReg)
   {
   TR::Instruction * current = _cursor->getNext();
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

/**
 * Removes CGIT/CIT nullchk when CLT can implicitly do nullchk
 * in SignalHandler.c
 */
bool
TR_S390PostRAPeephole::removeMergedNullCHK()
   {
      if (TR::Compiler->target.isZOS())
        {
        // CLT cannot do the job in zOS because in zOS it is legal to read low memory address (like 0x000000, literally NULL),
        // and CLT will read the low memory address legally (in this case NULL) to compare it with the other operand.
        // The result will not make sense since we should trap for NULLCHK first.
        return false;
        }

   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable removeMergedNullCHK on 0x%p.\n",_cursor))
         return false;
      }


   int32_t windowSize=0;
   const int32_t maxWindowSize=8;
   static char *disableRemoveMergedNullCHK = feGetEnv("TR_DisableRemoveMergedNullCHK");

   if (disableRemoveMergedNullCHK != NULL) return false;

   TR::Instruction * cgitInst = _cursor;
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
                if (performTransformation(comp(), "\nO^O S390 PEEPHOLE: removeMergedNullCHK on 0x%p.\n", cgitInst))
                   {
                   printInfo("\nremoving: ");
                   printInstr(comp(), cgitInst);
                   printInfo("\n");
                   }

                _cg->deleteInst(cgitInst);

                return true;
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

/**
 * LRReduction performs several LR reduction/removal transformations:
 *
 * (design 1980)
 * convert
 *      LTR GPRx, GPRx
 * to
 *      CHI GPRx, 0
 * This is an AGI reduction as LTR defines GPRx once again, while CHI simply sets the condition code
 *
 *
 *  removes unnecessary LR/LGR/LTR/LGTR's of the form
 *      LR  GPRx, GPRy
 *      LR  GPRy, GPRx   <--- Removed.
 *      CHI GPRx, 0
 * Most of the  redundant LR's are independently generated by global and
 * local register assignment.
 *
 * There is a further extension to this peephole which can transform
 *      LR  GPRx, GPRy
 *      LTR GPRx, GPRx
 * to
 *      LTR GPRx, GPRy
 * assuming that condition code is not incorrectly clobbered between the
 * LR and LTR.  However, there are very few opportunities to exercise this
 * peephole, so is not included.
 *
 * Convert
 *       LR GPRx, GPRy
 *       CHI GPRx, 0
 * to
 *       LTR GPRx, GPRy
 */
bool
TR_S390PostRAPeephole::LRReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable LRReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed = false;
   int32_t windowSize=0;
   const int32_t maxWindowSize=20;
   static char *disableLRReduction = feGetEnv("TR_DisableLRReduction");

   if (disableLRReduction != NULL) return false;

   //The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg.
   _cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(2);
   TR::Register *lgrTargetReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(1);
   TR::InstOpCode lgrOpCode = _cursor->getOpCode();

   if (lgrTargetReg == lgrSourceReg &&
      (lgrOpCode.getOpCodeValue() == TR::InstOpCode::LR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LGR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LDR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::CPYA))
       {
       if (performTransformation(comp(), "O^O S390 PEEPHOLE: Removing redundant LR/LGR/LDR/CPYA at %p\n", _cursor))
          {
            // Removing redundant LR.
          _cg->deleteInst(_cursor);
          performed = true;
          return performed;
          }
       }

   // If both target and source are the same, and we have a load and test,
   // convert it to a CHI
   if  (lgrTargetReg == lgrSourceReg &&
      (lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTR || lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTGR))
      {
      bool isAGI = seekRegInFutureMemRef(4, lgrTargetReg);

      if (isAGI && performTransformation(comp(), "\nO^O S390 PEEPHOLE: Transforming load and test to compare halfword immediate at %p\n", _cursor))
         {
         // replace LTGR with CGHI, LTR with CHI
         TR::Instruction* oldCursor = _cursor;
         _cursor = generateRIInstruction(_cg, lgrOpCode.is64bit() ? TR::InstOpCode::CGHI : TR::InstOpCode::CHI, comp()->getStartTree()->getNode(), lgrTargetReg, 0, _cursor->getPrev());

         _cg->replaceInst(oldCursor, _cursor);

         performed = true;

         // instruction is now a CHI, not a LTR, so we must return
         return performed;
         }

      TR::Instruction *prev = _cursor->getPrev();
      if((prev->getOpCodeValue() == TR::InstOpCode::LR && lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTR) ||
         (prev->getOpCodeValue() == TR::InstOpCode::LGR && lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTGR))
        {
        TR::Register *prevTargetReg = ((TR::S390RRInstruction*)prev)->getRegisterOperand(1);
        TR::Register *prevSourceReg = ((TR::S390RRInstruction*)prev)->getRegisterOperand(2);
        if((lgrTargetReg == prevTargetReg || lgrTargetReg == prevSourceReg) &&
           performTransformation(comp(), "\nO^O S390 PEEPHOLE: Transforming load register into load and test register and removing current at %p\n", _cursor))
          {
          TR::Instruction *newInst = generateRRInstruction(_cg, lgrOpCode.is64bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, prev->getNode(), prevTargetReg, prevSourceReg, prev->getPrev());
          _cg->replaceInst(prev, newInst);
          if (comp()->getOption(TR_TraceCG))
            printInstr(comp(), _cursor);
          _cg->deleteInst(prev);
          _cg->deleteInst(_cursor);
          _cursor = newInst;
          return true;
          }
        }
      TR::Instruction *next = _cursor->getNext();
      //try to remove redundant LTR, LTGR when we can reuse the condition code of an arithmetic logical operation, ie. Add/Subtract Logical
      //this is also done by isActiveLogicalCC, and the end of generateS390CompareAndBranchOpsHelper when the virtual registers match
      //but those cannot handle the case when the virtual registers are not the same but we do have the same restricted register
      //which is why we are handling it here when all the register assignments are done, and the redundant LR's from the
      //clobber evaluate of the add/sub logical are cleaned up as well.
      // removes the redundant LTR/LTRG, and corrects the mask of the BRC
      // from:
      // SLR @01, @04
      // LTR @01, @01
      // BRC (MASK8, 0x8) Label
      //
      // to:
      // SLR @01, @04
      // BRC (0x10) Label
      //checks that the prev instruction is an add/sub logical operation that sets the same target register as the LTR/LTGR insn, and that we branch immediately after
      if (prev->getOpCode().setsCC() && prev->getOpCode().setsCarryFlag() && prev->getRegisterOperand(1) == lgrTargetReg && next->getOpCodeValue() == TR::InstOpCode::BRC)
         {
         TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) next)->getBranchCondition();

         if ((branchCond == TR::InstOpCode::COND_BE || branchCond == TR::InstOpCode::COND_BNE) &&
            performTransformation(comp(), "\nO^O S390 PEEPHOLE: Removing redundant Load and Test instruction at %p, because CC can be reused from logical instruction %p\n",_cursor, prev))
            {
            _cg->deleteInst(_cursor);
            if (branchCond == TR::InstOpCode::COND_BE)
               ((TR::S390BranchInstruction *) next)->setBranchCondition(TR::InstOpCode::COND_MASK10);
            else if (branchCond == TR::InstOpCode::COND_BNE)
               ((TR::S390BranchInstruction *) next)->setBranchCondition(TR::InstOpCode::COND_MASK5);
            performed = true;
            return performed;
            }
         }
      }

   TR::Instruction * current = _cursor->getNext();

   // In order to remove LTR's, we need to ensure that there are no
   // instructions that set CC or read CC.
   bool lgrSetCC = lgrOpCode.setsCC();
   bool setCC = false, useCC = false;

   while ((current != NULL) &&
            !isBarrierToPeepHoleLookback(current) &&
           !(current->isBranchOp() && current->getKind() == TR::Instruction::IsRIL &&
              ((TR::S390RILInstruction *)  current)->getTargetSnippet() ) &&
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
         if(curTargetReg == lgrTargetReg && (srcImm == 0) && !(setCC || useCC))
            {
            if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
            if(performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming LR/CHI to LTR at %p\n", _cursor))
               {
               if (comp()->getOption(TR_TraceCG))
                  {
                  printInfo("\nRemoving CHI instruction:");
                  printInstr(comp(), current);
                  char tmp[50];
                  sprintf(tmp,"\nReplacing load at %p with load and test", _cursor);
                  printInfo(tmp);
                  }

               // Remove the CHI
               _cg->deleteInst(current);

               // Replace the LR with LTR
               TR::Instruction* oldCursor = _cursor;
               _cursor = generateRRInstruction(_cg, lgrOpCode.is64bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, comp()->getStartTree()->getNode(), lgrTargetReg, lgrSourceReg, _cursor->getPrev());

               _cg->replaceInst(oldCursor, _cursor);

               lgrOpCode = _cursor->getOpCode();

               lgrSetCC = true;

               performed = true;
               }
            }
         }

      // if we encounter the LR  GPRy, GPRx that we want to remove

      if (curOpCode.getOpCodeValue() == lgrOpCode.getOpCodeValue() &&
          current->getKind() == TR::Instruction::IsRR)
         {
         TR::Register *curSourceReg = ((TR::S390RRInstruction*)current)->getRegisterOperand(2);
         TR::Register *curTargetReg = ((TR::S390RRInstruction*)current)->getRegisterOperand(1);

         if ( ((curSourceReg == lgrTargetReg && curTargetReg == lgrSourceReg) ||
              (curSourceReg == lgrSourceReg && curTargetReg == lgrTargetReg)))
            {
            // We are either replacing LR/LGR (lgrSetCC won't be set)
            // or if we are modifying LTR/LGTR, then no instruction can
            // set or read CC between our original and current instruction.

            if ((!lgrSetCC || !(setCC || useCC)))
               {
               if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
               if (performTransformation(comp(), "O^O S390 PEEPHOLE: Duplicate LR/CPYA removal at %p\n", current))
                  {
                  if (comp()->getOption(TR_TraceCG))
                     {
                     printInfo("\nDuplicate LR/CPYA:");
                     printInstr(comp(), current);
                     char tmp[50];
                     sprintf(tmp,"is removed as duplicate of %p.", _cursor);
                     printInfo(tmp);
                     }

                  // Removing redundant LR/CPYA.
                  _cg->deleteInst(current);

                  performed = true;
                  current = current->getNext();
                  windowSize = 0;
                  setCC = setCC || current->getOpCode().setsCC();
                  useCC = useCC || current->getOpCode().readsCC();
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

/**
 * Catch the pattern where an LLC/LGFR can be converted
 *    LLC   Rs, Mem
 *    LGFR  Rs, Rs
 * Can be replaced with
 *    LLGC  Rs, Mem
 * Also applies to the pattern where an LLC/LLGTR can be converted
 *    LLC   Rs, Mem
 *    LLGTR  Rs, Rs
 * Can be replaced with
 *    LLGC  Rs, Mem
 */
bool
TR_S390PostRAPeephole::LLCReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable LLCReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed = false;

   TR::Instruction *current = _cursor->getNext();
   TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();

   if (curOpCode == TR::InstOpCode::LGFR || curOpCode == TR::InstOpCode::LLGTR)
      {
      TR::Register *llcTgtReg = ((TR::S390RRInstruction *) _cursor)->getRegisterOperand(1);

      TR::Register *curSrcReg = ((TR::S390RRInstruction *) current)->getRegisterOperand(2);
      TR::Register *curTgtReg = ((TR::S390RRInstruction *) current)->getRegisterOperand(1);

      if (llcTgtReg == curSrcReg && llcTgtReg == curTgtReg)
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming LLC/%s to LLGC at %p.\n", (curOpCode == TR::InstOpCode::LGFR) ? "LGFR" : "LLGTR" ,current))
            {
            if (comp()->getOption(TR_TraceCG))
               {
               if (curOpCode == TR::InstOpCode::LGFR)
                  printInfo("\nRemoving LGFR instruction:");
               else
                  printInfo("\nRemoving LLGTR instruction:");
               printInstr(comp(), current);
               char tmp[50];
               sprintf(tmp,"\nReplacing LLC at %p with LLGC", _cursor);
               printInfo(tmp);
               }

            // Remove the LGFR/LLGTR
            _cg->deleteInst(current);
            // Replace the LLC with LLGC
            TR::Instruction *oldCursor = _cursor;
            TR::MemoryReference* memRef = ((TR::S390RXInstruction *) oldCursor)->getMemoryReference();
            memRef->resetMemRefUsedBefore();
            _cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGC, comp()->getStartTree()->getNode(), llcTgtReg, memRef, _cursor->getPrev());
            _cg->replaceInst(oldCursor, _cursor);
            performed = true;
            }
         }
      }

   return performed;
   }

/**
 *  Catch the pattern where an LGR/LGFR are redundent
 *    LGR   Rt, Rs
 *    LGFR  Rt, Rt
 *  can be replaced by
 *    LGFR  Rt, Rs
 */
bool
TR_S390PostRAPeephole::LGFRReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable LGFRReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed=false;
   int32_t windowSize=0;
   const int32_t maxWindowSize=2;
   static char *disableLGFRRemoval = feGetEnv("TR_DisableLGFRRemoval");

   if (disableLGFRRemoval != NULL) return false;

   TR::Register *lgrSourceReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(2);
   TR::Register *lgrTargetReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(1);

   // We cannot do anything if both target and source are the same,
   // which can happen with LTR and LTGR
   if  (lgrTargetReg == lgrSourceReg) return performed;

   TR::Instruction * current = _cursor->getNext();
   TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();

   if (curOpCode == TR::InstOpCode::LGFR)
      {
      TR::Register *curSourceReg=((TR::S390RRInstruction*)current)->getRegisterOperand(2);
      TR::Register *curTargetReg=((TR::S390RRInstruction*)current)->getRegisterOperand(1);

      if (curSourceReg == lgrTargetReg && curTargetReg == lgrTargetReg)
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: Redundant LGR at %p.\n", current))
            {
            if (comp()->getOption(TR_TraceCG))
               {
               printInfo("\n\tRedundent LGR/LGFR:");
               printInstr(comp(), current);
               char tmp[50];
               sprintf(tmp, "\t is removed as duplicate of %p.", _cursor);
               printInfo(tmp);
               }

            ((TR::S390RRInstruction*)current)->setRegisterOperand(2,lgrSourceReg);

            // Removing redundant LR.
            _cg->deleteInst(_cursor);

            performed = true;
            _cursor = _cursor->getNext();
            }
         }
      }

   return performed;
   }

/**
 * Catch the pattern where a CRJ/LHI conditional load immediate sequence
 *
 *    CRJ  Rx, Ry, L, M
 *    LHI  Rz, I
 * L: ...  ...
 *
 *  can be replaced by
 *
 *    CR     Rx, Ry
 *    LOCHI  Rz, I, M
 *    ...    ...
 */
bool
TR_S390PostRAPeephole::ConditionalBranchReduction(TR::InstOpCode::Mnemonic branchOPReplacement)
   {
   bool disabled = comp()->getOption(TR_DisableZ13) || comp()->getOption(TR_DisableZ13LoadImmediateOnCond);

   // This optimization relies on hardware instructions introduced in z13
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z13) || disabled)
      return false;

   TR::S390RIEInstruction* branchInst = static_cast<TR::S390RIEInstruction*> (_cursor);

   TR::Instruction* currInst = _cursor;
   TR::Instruction* nextInst = _cursor->getNext();

   TR::Instruction* label = branchInst->getBranchDestinationLabel()->getInstruction();

   // Check that the instructions within the fall-through block can be conditionalized
   while (currInst = realInstructionWithLabelsAndRET(nextInst, true))
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

   currInst = _cursor;
   nextInst = _cursor->getNext();

   TR::InstOpCode::S390BranchCondition cond = getBranchConditionForMask(0xF - (getMaskForBranchCondition(branchInst->getBranchCondition()) & 0xF));

   if (performTransformation(comp(), "O^O S390 PEEPHOLE: Conditionalizing fall-through block following %p.\n", currInst))
      {
      // Conditionalize the fall-though block
      while (currInst = realInstructionWithLabelsAndRET(nextInst, true))
         {
         if (currInst == label)
            break;

         // Because of the previous checks, LHI or LGHI instruction is guaranteed to be here
         TR::S390RIInstruction* RIInst = static_cast<TR::S390RIInstruction*> (currInst);

         // Conditionalize from "Load Immediate" to "Load Immediate on Condition"
         _cg->replaceInst(RIInst, _cursor = generateRIEInstruction(_cg, RIInst->getOpCode().getOpCodeValue() == TR::InstOpCode::LHI ? TR::InstOpCode::LOCHI : TR::InstOpCode::LOCGHI, RIInst->getNode(), RIInst->getRegisterOperand(1), RIInst->getSourceImmediate(), cond, RIInst->getPrev()));

         currInst = _cursor;
         nextInst = _cursor->getNext();
         }

      // Conditionalize the branch instruction from "Compare and Branch" to "Compare"
      _cg->replaceInst(branchInst, generateRRInstruction(_cg, branchOPReplacement, branchInst->getNode(), branchInst->getRegisterOperand(1), branchInst->getRegisterOperand(2), branchInst->getPrev()));

      return true;
      }

   return false;
   }


/**
 * Catch the pattern where an CR/BRC can be converted
 *    CR R1,R2
 *    BRC  Mask, Lable
 * Can be replaced with
 *    CRJ  R1,R2,Lable
 */
bool
TR_S390PostRAPeephole::CompareAndBranchReduction()
   {
   bool performed = false;

   TR::Instruction *current = _cursor;
   TR::Instruction *next = _cursor->getNext();
   TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();
   TR::InstOpCode::Mnemonic nextOpCode = next->getOpCodeValue();
   if ((curOpCode == TR::InstOpCode::CR || curOpCode == TR::InstOpCode::CGR || curOpCode == TR::InstOpCode::CGFR)
      && (nextOpCode == TR::InstOpCode::BRC || nextOpCode == TR::InstOpCode::BRCL))
      {
      printf("Finding CR + BRC\n");
      printf("method=%s\n", comp()->signature());
      }

   return performed;
   }

/**
 * Catch the pattern where a LOAD/NILL load-and-mask sequence
 *
 *    LOAD  Rx, Mem             where LOAD = L     => LZOpCode = LZRF
 *    NILL  Rx, 0xFF00 (-256)         LOAD = LG    => LZOpCode = LZRG
 *                                    LOAD = LLGF  => LZOpCode = LLZRGF
 *  can be replaced by
 *
 *    LZOpCode  Rx, Mem
 */
bool
TR_S390PostRAPeephole::LoadAndMaskReduction(TR::InstOpCode::Mnemonic LZOpCode)
   {
   bool disabled = comp()->getOption(TR_DisableZ13) || comp()->getOption(TR_DisableZ13LoadAndMask);

   // This optimization relies on hardware instructions introduced in z13
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z13) || disabled)
      return false;

   if (_cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILL)
      {
      TR::S390RXInstruction* loadInst = static_cast<TR::S390RXInstruction*> (_cursor);
      TR::S390RIInstruction* nillInst = static_cast<TR::S390RIInstruction*> (_cursor->getNext());

      if (!nillInst->isImm() || nillInst->getSourceImmediate() != 0xFF00)
         return false;

      TR::Register* loadTargetReg = loadInst->getRegisterOperand(1);
      TR::Register* nillTargetReg = nillInst->getRegisterOperand(1);

      if (loadTargetReg != nillTargetReg)
         return false;

      if (performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming load-and-mask sequence at %p.\n", nillInst))
         {
         // Remove the NILL instruction
         _cg->deleteInst(nillInst);

         loadInst->getMemoryReference()->resetMemRefUsedBefore();

         // Replace the load instruction with load-and-mask instruction
         _cg->replaceInst(loadInst, _cursor = generateRXYInstruction(_cg, LZOpCode, comp()->getStartTree()->getNode(), loadTargetReg, loadInst->getMemoryReference(), _cursor->getPrev()));

         return true;
         }
      }
   return false;
   }

bool swapOperands(TR::Register * trueReg, TR::Register * compReg, TR::Instruction * curr)
   {
   TR::InstOpCode::S390BranchCondition branchCond;
   uint8_t mask;
   switch (curr->getKind())
      {
      case TR::Instruction::IsRR:
         branchCond = ((TR::S390RRInstruction *) curr)->getBranchCondition();
         ((TR::S390RRInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RRInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRIE:
         branchCond = ((TR::S390RIEInstruction *) curr)->getBranchCondition();
         ((TR::S390RIEInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RIEInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RIEInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRRS:
         branchCond = ((TR::S390RRSInstruction *) curr)->getBranchCondition();
         ((TR::S390RRSInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RRSInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRSInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
         branchCond = ((TR::S390RRFInstruction *) curr)->getBranchCondition();
         ((TR::S390RRFInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRRF2:
         mask = ((TR::S390RRFInstruction *) curr)->getMask3();
         ((TR::S390RRFInstruction *) curr)->setMask3(getReverseBranchMask(mask));
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      default:
         // unsupport instruction type, bail
        return false;
      }
      return true;
   }

void insertLoad(TR::Compilation * comp, TR::CodeGenerator * cg, TR::Instruction * i, TR::Register * r)
   {
   switch(r->getKind())
     {
     case TR_FPR:
       new (comp->trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::LDR, i->getNode(), r, r, i, cg);
       break;
     default:
       new (comp->trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::LR, i->getNode(), r, r, i, cg);
       break;
     }
   }

bool hasDefineToRegister(TR::Instruction * curr, TR::Register * reg)
   {
   TR::Instruction * prev = curr->getPrev();
   prev = realInstruction(prev, false);
   for(int32_t i = 0; i < 3 && prev; ++i)
      {
      if (prev->defsRegister(reg))
         {
         return true;
         }

      prev = prev->getPrev();
      prev = realInstruction(prev, false);
      }
   return false;
   }

/**
 * z10 specific hw performance bug
 * On z10, applies to GPRs only.
 * There are cases where load of a GPR and its complemented value are required
 * in same grouping, causing pipeline flush + late load = perf hit.
 */
bool
TR_S390PostRAPeephole::trueCompEliminationForCompare()
   {
   // z10 specific
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) || _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }
   TR::Instruction * curr = _cursor;
   TR::Instruction * prev = _cursor->getPrev();
   TR::Instruction * next = _cursor->getNext();
   TR::Register * compReg;
   TR::Register * trueReg;

   prev = realInstruction(prev, false);
   next = realInstruction(next, true);

   switch(curr->getKind())
      {
      case TR::Instruction::IsRR:
         compReg = ((TR::S390RRInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRIE:
         compReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRRS:
         compReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
         compReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(1);
         break;
      default:
         // unsupport instruction type, bail
         return false;
      }

   // only applies to GPR's
   if (compReg->getKind() != TR_GPR || trueReg->getKind() != TR_GPR)
      {
      return false;
      }

   if (!hasDefineToRegister(curr, compReg))
      {
      return false;
      }

   TR::Instruction * branchInst = NULL;
   // current instruction sets condition code or compare flag, check to see
   // if it has multiple branches using this condition code..if so, abort.
   TR::Instruction * nextInst = next;
   while(nextInst && !nextInst->isLabel() && !nextInst->getOpCode().setsCC() &&
         !nextInst->isCall() && !nextInst->getOpCode().setsCompareFlag())
      {
      if(nextInst->isBranchOp())
         {
         if(branchInst == NULL)
            {
            branchInst = nextInst;
            }
         else
            {
            // there are multiple branches using the same branch condition
            // just give up.
            // we can probably just insert load instructions here still, but
            // we have to sort out the wild branches first
            return false;
            }
         }
      nextInst = nextInst->getNext();
      }
   if (branchInst && prev && prev->usesRegister(compReg) && !prev->usesRegister(trueReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 1 at %p.\n",curr))
         {
         swapOperands(trueReg, compReg, curr);
         if(next && next->usesRegister(trueReg)) insertLoad (comp(), _cg, next->getPrev(), compReg);
         TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) branchInst)->getBranchCondition();
         ((TR::S390BranchInstruction *) branchInst)->setBranchCondition(getReverseBranchCondition(branchCond));
         return true;
         }
      else
         return false;
      }
   if (next && next->usesRegister(compReg) && !next->usesRegister(trueReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 2 at %p.\n",curr))
         {
         if (branchInst && prev && !prev->usesRegister(trueReg))
            {
            swapOperands(trueReg, compReg, curr);
            TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) branchInst)->getBranchCondition();
            ((TR::S390BranchInstruction *) branchInst)->setBranchCondition(getReverseBranchCondition(branchCond));
            }
         else
            {
            insertLoad (comp(), _cg, next->getPrev(), compReg);
            }
         return true;
         }
      else
         return false;
      }

   bool loadInserted = false;
   if (prev && prev->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 3 at %p.\n",curr))
         {
         insertLoad (comp(), _cg, prev, trueReg);
         loadInserted = true;
         }
      }
   if (next && next->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 4 at %p.\n",curr))
         {
         insertLoad (comp(), _cg, next->getPrev(), trueReg);
         loadInserted = true;
         }
      }
   return loadInserted ;
   }

/**
 * z10 specific hw performance bug
 * On z10, applies to GPRs only.
 * There are cases where load of a GPR and its complemented value are required
 * in same grouping, causing pipeline flush + late load = perf hit.
 */
bool
TR_S390PostRAPeephole::trueCompEliminationForCompareAndBranch()
   {
   // z10 specific
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) || _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }

   TR::Instruction * curr = _cursor;
   TR::Instruction * prev = _cursor->getPrev();
   prev = realInstruction(prev, false);

   TR::Instruction * next = _cursor->getNext();
   next = realInstruction(next, true);

   TR::Register * compReg;
   TR::Register * trueReg;
   TR::Instruction * btar = NULL;
   TR::LabelSymbol * ls = NULL;

   bool isWCodeCmpEqSwap = false;

   switch (curr->getKind())
      {
      case TR::Instruction::IsRIE:
         compReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(1);
         btar = ((TR::S390RIEInstruction *) curr)->getBranchDestinationLabel()->getInstruction();
         break;
      case TR::Instruction::IsRRS:
         compReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
      case TR::Instruction::IsRRF2:
         compReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(1);
         break;
      default:
         // unsupport instruction type, bail
         return false;
      }

   // only applies to GPR's
   if (compReg->getKind() != TR_GPR || trueReg->getKind() != TR_GPR)
      {
      return false;
      }

   if (!hasDefineToRegister(curr, compReg))
      {
      return false;
      }


   btar = realInstruction(btar, true);
   bool backwardBranch = false;
   if (btar)
      {
      backwardBranch = curr->getIndex() - btar->getIndex() > 0;
      }

   if (backwardBranch)
      {
      if ((prev && btar) &&
          (prev->usesRegister(compReg) || btar->usesRegister(compReg)) &&
          (!prev->usesRegister(trueReg) && !btar->usesRegister(trueReg)) )
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 1 at %p.\n",curr))
            {
            swapOperands(trueReg, compReg, curr);
            if (next && next->usesRegister(trueReg)) insertLoad (comp(), _cg, next->getPrev(), compReg);
            return true;
            }
         else
            return false;
         }
      }
   else
      {
      if ((prev && next) &&
          (prev->usesRegister(compReg) || next->usesRegister(compReg)) &&
          (!prev->usesRegister(trueReg) && !next->usesRegister(trueReg)) )
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(!isWCodeCmpEqSwap && performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 2 at %p.\n",curr))
            {
            swapOperands(trueReg, compReg, curr);
            if (btar && btar->usesRegister(trueReg)) insertLoad (comp(), _cg, btar->getPrev(), compReg);
            return true;
            }
         else
            return false;
         }
      }
   if (prev && prev->usesRegister(compReg) && !prev->usesRegister(trueReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(!isWCodeCmpEqSwap && performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 3 at %p.\n",curr))
         {
         swapOperands(trueReg, compReg, curr);
         if (btar && btar->usesRegister(trueReg)) insertLoad (comp(), _cg, btar->getPrev(), compReg);
         if (next && next->usesRegister(trueReg)) insertLoad (comp(), _cg, next->getPrev(), compReg);
         return true;
         }
      else
         return false;
      }

   bool loadInserted = false;
   if (prev && prev->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 4 at %p.\n",curr) )
         {
         insertLoad (comp(), _cg, prev, trueReg);
         loadInserted = true;
         }
      }
   if (btar && btar->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 5 at %p.\n",curr) )
         {
         insertLoad (comp(), _cg, btar->getPrev(), trueReg);
         loadInserted = true;
         }
      }
   if (next && next->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 6 at %p.\n",curr) )
         {
         insertLoad (comp(), _cg, next->getPrev(), trueReg);
         loadInserted = true;
         }
      }
   return loadInserted;
   }

bool
TR_S390PostRAPeephole::trueCompEliminationForLoadComp()
   {
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) || _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }
   TR::Instruction * curr = _cursor;
   TR::Instruction * next = _cursor->getNext();
   next = realInstruction(next, true);
   TR::Instruction * prev = _cursor->getPrev();
   prev = realInstruction(prev, false);

   TR::Register * srcReg = ((TR::S390RRInstruction *) curr)->getRegisterOperand(2);
   TR::RealRegister *tempReg = NULL;
   if((toRealRegister(srcReg))->getRegisterNumber() == TR::RealRegister::GPR1)
      {
      tempReg = _cg->machine()->getS390RealRegister(TR::RealRegister::GPR2);
      }
   else
      {
      tempReg = _cg->machine()->getS390RealRegister(TR::RealRegister::GPR1);
      }

   if (prev && prev->defsRegister(srcReg))
      {
      // src register is defined in the previous instruction, check to see if it's
      // used in the next instruction, if so, inject a load after the current insruction
      if (next && next->usesRegister(srcReg))
         {
         insertLoad (comp(), _cg, curr, tempReg);
         return true;
         }
      }

   TR::Instruction * prev2 = NULL;
   if (prev)
      {
      prev2 = realInstruction(prev->getPrev(), false);
      }
   if (prev2 && prev2->defsRegister(srcReg))
      {
      // src register is defined 2 instructions ago, insert a load before the current instruction
      // if the true value is used before or after
      if ((next && next->usesRegister(srcReg)) || (prev && prev->usesRegister(srcReg)))
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for load complement at %p.\n",curr))
            {
            insertLoad (comp(), _cg, curr->getPrev(), tempReg);
            return true;
            }
         else
            return false;
         }
      }

   TR::Instruction * prev3 = NULL;
   if (prev2)
      {
      prev3 = realInstruction(prev2->getPrev(), false);
      }
   if (prev3 && prev3->defsRegister(srcReg))
      {
      // src registers is defined 3 instructions ago, insert a load before the current instruction
      // if the true value is use before
      if(prev && prev->usesRegister(srcReg))
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for load complement at %p.\n",curr))
            {
            insertLoad (comp(), _cg, curr->getPrev(), tempReg);
            return true;
            }
         else
            return false;
         }
      }
   return false;
   }

/**
 * Catch pattern where a DAA instruction is followed by 2 BRCL NOPs; The second one to an outlined label, e.g.
 *
 * CVBG    GPR6, 144(GPR5)
 * BRCL    NOP(0x0), Snippet Label L0059, labelTargetAddr=0x0000000029B8CC2A
 * BRCL    NOP(0x0),         Label L0058, labelTargetAddr=0x0000000029B8C896
 *
 * The second BRCL is generate to create the proper RA snapshot in case we branch to the OOL path.
 * Since Peephole optimizations come after RA, we can safely remove this instruction.
 *
 * Note that for 4-byte instructions such as CVB there is an extra 2-byte NOP used as padding following
 * the 4-byte instruction.
 */
bool
TR_S390PostRAPeephole::DAARemoveOutlinedLabelNop(bool hasPadding)
   {
   // Make sure the current instruction is a DAA intrinsic instruction
   if (_cursor->throwsImplicitException())
      {
      TR::Instruction* currInst = _cursor;
      TR::Instruction* nextInst = _cursor->getNext();

      if (hasPadding)
         {
         if (nextInst->getOpCodeValue() == TR::InstOpCode::NOP)
            nextInst = nextInst->getNext();
         else
            {
            TR_ASSERT(0, "S390 PEEPHOLE: DAA instruction padding was not found following instruction %p.\n", nextInst);

            return false;
            }
         }

      TR::Instruction* nextInst2 = nextInst->getNext();

      // Check if we have two NOP BRCs following the DAA intrinsic instruction
      if ((nextInst ->getOpCodeValue() == TR::InstOpCode::BRC || nextInst ->getOpCodeValue() == TR::InstOpCode::BRCL) &&
          (nextInst2->getOpCodeValue() == TR::InstOpCode::BRC || nextInst2->getOpCodeValue() == TR::InstOpCode::BRCL))
         {
         TR::S390RegInstruction* OOLbranchInst = static_cast<TR::S390RegInstruction*> (nextInst2);

         if (OOLbranchInst->getBranchCondition() == TR::InstOpCode::COND_MASK0)
            if (performTransformation(comp(), "O^O S390 PEEPHOLE: Eliminating redundant DAA NOP BRC/BRCL to OOL path %p.\n", OOLbranchInst))
               {
               OOLbranchInst->remove();

               return true;
               }
         }
      }

      return false;
   }

/**
 * Catch pattern where a DAA instruction memory reference generates spill code for long displacement, e.g.
 *
 * STG     GPR1, 148(GPR5)
 * LGHI    GPR1, 74048
 * CVBG    GPR6, 0(GPR1,GPR5)
 * LG      GPR1, 108(GPR5)
 * BRCL    NOP(0x0), Snippet Label L0059, labelTargetAddr=0x0000000029B8CC2A
 * BRCL    NOP(0x0),         Label L0058, labelTargetAddr=0x0000000029B8C896
 *
 * Because the SignalHandler looks for the RestoreGPR7 Snippet address by a hardcoded offset it will
 * instead encounter the memory reference spill code in the place where the RestoreGPR7 Snippet BRCL
 * NOP should have been. This will result in a wild branch.
 *
 * Since this is such a rare occurrence, the workaround is to remove the DAA intrinsic instructions
 * and replace them by an unconditional branch to the OOL path.
 *
 * Note that for 4-byte instructions such as CVB there is an extra 2-byte NOP used as padding following
 * the 4-byte instruction.
 */
bool
TR_S390PostRAPeephole::DAAHandleMemoryReferenceSpill(bool hasPadding)
   {
   // Make sure the current instruction is a DAA intrinsic instruction
   if (_cursor->throwsImplicitException())
      {
      TR::Instruction* currInst = _cursor;
      TR::Instruction* nextInst = _cursor->getNext();

      if (hasPadding)
         {
         if (nextInst->getOpCodeValue() == TR::InstOpCode::NOP)
            nextInst = nextInst->getNext();
         else
            {
            TR_ASSERT(0, "S390 PEEPHOLE: DAA instruction padding was not found following instruction %p.\n", nextInst);

            return false;
            }
         }

      if (nextInst->getOpCodeValue() != TR::InstOpCode::BRC && nextInst->getOpCodeValue() != TR::InstOpCode::BRCL)
         {
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: Eliminating DAA intrinsic due to a memory reference spill near instruction %p.\n", currInst))
            {
            currInst = _cursor;
            nextInst = _cursor->getNext();

            TR::S390BranchInstruction* OOLbranchInst;

            // Find the OOL Branch Instruction
            while (currInst != NULL)
               {
               if (currInst->getOpCodeValue() == TR::InstOpCode::BRC || currInst->getOpCodeValue() == TR::InstOpCode::BRCL)
                  {
                  // Convert the instruction to a Branch Instruction to access the Label Symbol
                  OOLbranchInst = static_cast<TR::S390BranchInstruction*> (currInst);

                  // We have found the correct BRC/BRCL
                  if (OOLbranchInst->getLabelSymbol()->isStartOfColdInstructionStream())
                     break;
                  }

               currInst = nextInst;
               nextInst = nextInst->getNext();
               }

            if (currInst == NULL)
               return false;

            currInst = _cursor;
            nextInst = _cursor->getNext();

            // Remove all instructions up until the OOL Branch Instruction
            while (currInst != OOLbranchInst)
               {
               currInst->remove();

               currInst = nextInst;
               nextInst = nextInst->getNext();
               }

            // Sanity check: OOLbranchInst should be a NOP BRC/BRCL
            TR_ASSERT(OOLbranchInst->getBranchCondition() == TR::InstOpCode::COND_MASK0, "S390 PEEPHOLE: DAA BRC/BRCL to the OOL path is NOT a NOP branch %p.\n", OOLbranchInst);

            // Change the NOP BRC to the OOL path to an unconditional branch
            OOLbranchInst->setBranchCondition(TR::InstOpCode::COND_MASK15);

            _cursor = OOLbranchInst;

            return true;
            }
         }
      }

   return false;
   }

/**
 * Catch pattern where Input and Output registers in an int shift are the same
 *    SLLG    GPR6,GPR6,1
 *    SLAK    GPR15,GPR15,#474 0(GPR3)
 *  replace with shorter version
 *    SLL     GPR6,1
 *    SLA     GPR15,#474 0(GPR3)
 *
 * Applies to SLLK, SRLK, SLAK, SRAK, SLLG, SLAG
 * @return true if replacement ocurred
 *         false otherwise
 */
bool
TR_S390PostRAPeephole::revertTo32BitShift()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable revertTo32BitShift on 0x%p.\n",_cursor))
         return false;
      }

   TR::S390RSInstruction * instr = (TR::S390RSInstruction *) _cursor;

   // The shift is supposed to be an integer shift when reducing 64bit shifts.
   // Note the NOT in front of second boolean expr. pair
   TR::InstOpCode::Mnemonic oldOpCode = instr->getOpCodeValue();
   if ((oldOpCode == TR::InstOpCode::SLLG || oldOpCode == TR::InstOpCode::SLAG)
         && !(instr->getNode()->getOpCodeValue() == TR::ishl || instr->getNode()->getOpCodeValue() == TR::iushl))
      {
      return false;
      }

   TR::Register* sourceReg = (instr->getSecondRegister())?(instr->getSecondRegister()->getRealRegister()):NULL;
   TR::Register* targetReg = (instr->getRegisterOperand(1))?(instr->getRegisterOperand(1)->getRealRegister()):NULL;

   // Source and target registers must be the same
   if (sourceReg != targetReg)
      {
      return false;
      }

   bool reverted = false;
   if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
   if (performTransformation(comp(), "O^O S390 PEEPHOLE: Reverting int shift at %p from SLLG/SLAG/S[LR][LA]K to SLL/SLA/SRL/SRA.\n", instr))
      {
      reverted = true;
      TR::InstOpCode::Mnemonic newOpCode = TR::InstOpCode::BAD;
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
         default: //PANIC!!!
            TR_ASSERT( 0,"Unexpected OpCode for revertTo32BitShift\n");
            break;
         }

      TR::S390RSInstruction* newInstr = NULL;

      if (instr->getSourceImmediate())
         {
         newInstr = new (_cg->trHeapMemory()) TR::S390RSInstruction(newOpCode, instr->getNode(), instr->getRegisterOperand(1), instr->getSourceImmediate(), instr->getPrev(), _cg);
         }
      else if (instr->getMemoryReference())
         {
         TR::MemoryReference* memRef = instr->getMemoryReference();
         memRef->resetMemRefUsedBefore();
         newInstr = new (_cg->trHeapMemory()) TR::S390RSInstruction(newOpCode, instr->getNode(), instr->getRegisterOperand(1), memRef, instr->getPrev(), _cg);
         }
      else
         {
         TR_ASSERT( 0,"Unexpected RSY format\n");
         }

      _cursor = newInstr;
      _cg->replaceInst(instr, newInstr);

      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "\nTransforming int shift from SLLG/SLAG/S[LR][LA]K (%p) to SLL/SLA/SRL/SRA %p.\n", instr, newInstr);
      }
   return reverted;
   }




/**
 * Try to inline EX dispatched constant instruction snippet
 * into the code section to eliminate i-cache misses and improve
 * runtime performance.
 */
bool
TR_S390PostRAPeephole::inlineEXtargetHelper(TR::Instruction *inst, TR::Instruction * instrB4EX)
   {
   if (performTransformation(comp(), "O^O S390 PEEPHOLE: Converting LARL;EX into EXRL instr=[%p]\n", _cursor))
      {
      // fetch the dispatched SS instr
      TR::S390ConstantDataSnippet * cnstDataSnip = _cursor->getMemoryReference()->getConstantDataSnippet();
      TR_ASSERT( (cnstDataSnip->getKind() == TR::Snippet::IsConstantInstruction),"The constant data snippet kind should be ConstantInstruction!\n");
      TR::Instruction * cnstDataInstr = ((TR::S390ConstantInstructionSnippet *)cnstDataSnip)->getInstruction();

      // EX Dispatch would have created a load of the literal pool address, a register which is then killed
      // immediately after its use in the EX instruction. Since we are transforming the EX to an EXRL, this
      // load is no longer needed. We search backwards for this load of the literal pool and remove it.

      TR::Instruction * iterator = instrB4EX;
      int8_t count = 0;
      while (iterator && count < 3)
         {
         if (iterator->getOpCodeValue() != TR::InstOpCode::LARL)
            {
            iterator = realInstructionWithLabels(iterator->getPrev(), false);

            if (iterator == NULL || iterator->isCall() || iterator->isBranchOp())
               break;
            }

         if (iterator->getOpCodeValue() == TR::InstOpCode::LARL)
            {
            TR::S390RILInstruction* LARLInst = reinterpret_cast<TR::S390RILInstruction*> (iterator);

            // Make sure the register we load the literal pool address in is the same register referenced by the EX instruction
            if (LARLInst->isLiteralPoolAddress() && LARLInst->getRegisterOperand(1) == _cursor->getMemoryReference()->getBaseRegister())
               {
               _cg->deleteInst(iterator);
               break;
               }
            }
         else
            {
            // Make sure the register referenced by EX instruction is not be used ?
            TR::Register *baseReg =  _cursor->getMemoryReference()->getBaseRegister();
            if(iterator && iterator->usesRegister(baseReg))
              break;
            }
         ++count;
         }

      // generate new label
      TR::LabelSymbol * ssInstrLabel = generateLabelSymbol(_cg);
      TR::Instruction * labelInstr = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _cursor->getNode(), ssInstrLabel, inst);

      //cnstDataInstr->move(labelInstr);
      cnstDataInstr->setNext(labelInstr->getNext());
      labelInstr->getNext()->setPrev(cnstDataInstr);
      labelInstr->setNext(cnstDataInstr);
      cnstDataInstr->setPrev(labelInstr);

      // generate EXRL
      TR::S390RILInstruction * newEXRLInst = new (_cg->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::EXRL, _cursor->getNode(), _cursor->getRegisterOperand(1), ssInstrLabel, _cursor->getPrev(), _cg);

      // Replace the EX with EXRL
      TR::Instruction * oldCursor = _cursor;
      _cursor = newEXRLInst;
      _cg->replaceInst(oldCursor, newEXRLInst);

      // remove the instr constant data snippet
      (_cg->getConstantInstructionSnippets()).remove(cnstDataSnip);

      return true;
      }
   return false;
   }


bool
TR_S390PostRAPeephole::inlineEXtarget()
   {
   // try to find a label followed an unconditional branch
   TR::Instruction * inst = _cursor;
   int8_t count = 0;
   int8_t maxCount = 30;

   while (inst && count < maxCount)
      {
      inst = realInstructionWithLabels(inst->getPrev(), false);

      if (inst == NULL)
         break;

      // Check to see if we found a label or not
      if (inst->isLabel())
         {
         inst = realInstructionWithLabels(inst->getPrev(), false);

         if (inst == NULL)
            break;

         // check if the label is following an unconditional branch
         if (inst && inst->getOpCode().getOpCodeValue() == TR::InstOpCode::BRC &&
             ((TR::S390BranchInstruction *)inst)->getBranchCondition() == TR::InstOpCode::COND_BRC)
           {
           return inlineEXtargetHelper(inst,_cursor->getPrev());
           }
         }
      else
         {
         }

      ++count;
      }

   if (!comp()->getOption(TR_DisableForcedEXInlining) &&
       performTransformation(comp(), "O^O S390 PEEPHOLE: Inserting unconditional branch to hide EX target instr=[%p]\n",_cursor))
       {
       // Find previous instruction, skipping LARL so we can find it to delete it later.
       TR::Instruction *prev = realInstructionWithLabels(_cursor->getPrev(), false);
       TR::Instruction *instrB4EX = prev;
       if (prev->getOpCode().getOpCodeValue() == TR::InstOpCode::LARL)
          {
          prev = realInstructionWithLabels(prev->getPrev(), false);
          }

       // Generate branch and label
       TR::LabelSymbol *targetLabel = generateLabelSymbol(_cg);
       TR::Instruction *branchInstr = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, _cursor->getNode(), targetLabel, prev);
       TR::Instruction *labelInstr = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _cursor->getNode(), targetLabel, branchInstr);
       inlineEXtargetHelper(branchInstr,instrB4EX);
       return true;
       }
   return false;
   }

/**
 * Exploit zGryphon distinct Operands facility:
 * ex:
 * LR      GPR6,GPR0   ; clobber eval
 * AHI     GPR6,-1
 *
 * becomes:
 * AHIK    GPR6,GPR0, -1
 */
bool
TR_S390PostRAPeephole::attemptZ7distinctOperants()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable attemptZ7distinctOperants on 0x%p.\n",_cursor))
         return false;
      }

   bool performed=false;
   int32_t windowSize=0;
   const int32_t maxWindowSize=4;

   TR::Instruction * instr = _cursor;

   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }

   if (instr->getOpCodeValue() != TR::InstOpCode::LR && instr->getOpCodeValue() != TR::InstOpCode::LGR)
      {
      return false;
      }

   TR::Register *lgrTargetReg = instr->getRegisterOperand(1);
   TR::Register *lgrSourceReg = instr->getRegisterOperand(2);

   TR::Instruction * current = _cursor->getNext();

   while ( (current != NULL) &&
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

      TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();

      // found the first next def of lgrSourceReg
      // todo : verify that reg pair is handled
      if (current->defsRegister(lgrSourceReg))
         {
         // we cannot do this if the source register's value is updated:
         // ex:
         // LR      GPR6,GPR0
         // SR      GPR0,GPR11
         // AHI     GPR6,-1
         //
         // this sequence cannot be transformed into:
         // SR      GPR0,GPR11
         // AHIK    GPR6,GPR0, -1
         return false;
         }
      if (!comp()->getOption(TR_DisableHighWordRA) && instr->getOpCodeValue() == TR::InstOpCode::LGR)
         {
         // handles this case:
         // LGR     GPR2,GPR9        ; LR=Clobber_eval
         // LFH     HPR9,#366#SPILL8 Auto[<spill temp 0x84571FDB0>] ?+0(GPR5) ; Load Spill
         // AGHI    GPR2,16
         //
         // cannot transform this into:
         // LFH     HPR9,#366#SPILL8 Auto[<spill temp 0x84571FDB0>] 96(GPR5) ; Load Spill
         // AGHIK   GPR2,GPR9,16,

         TR::RealRegister * lgrSourceHighWordRegister = toRealRegister(lgrSourceReg)->getHighWordRegister();
         if (current->defsRegister(lgrSourceHighWordRegister))
            {
            return false;
            }
         }
      // found the first next use/def of lgrTargetRegister
      if (current->usesRegister(lgrTargetReg))
         {
         if (current->defsRegister(lgrTargetReg))
            {
            TR::Instruction * newInstr = NULL;
            TR::Instruction * prevInstr = current->getPrev();
            TR::Node * node = instr->getNode();
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

            if (  (current->getOpCode().is32bit() &&  instr->getOpCodeValue() == TR::InstOpCode::LGR)
               || (current->getOpCode().is64bit() &&  instr->getOpCodeValue() == TR::InstOpCode::LR))
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
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ARK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::AGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::AGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::ALR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ALRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::ALGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ALGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::AHI:
                  {
                  int16_t imm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
                  newInstr = generateRIEInstruction(_cg, TR::InstOpCode::AHIK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::AGHI:
                  {
                  int16_t imm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
                  newInstr = generateRIEInstruction(_cg, TR::InstOpCode::AGHIK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::NR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::NRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::NGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::NGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::XR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::XRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::XGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::XGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::OR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ORK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::OGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::OGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLA:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                    mf->resetMemRefUsedBefore();
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLAK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLAK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLL:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                   mf->resetMemRefUsedBefore();
                   newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLLK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLLK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }

               case TR::InstOpCode::SRA:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction *)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                    mf->resetMemRefUsedBefore();
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRAK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRAK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SRL:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                    mf->resetMemRefUsedBefore();
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRLK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRLK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SLRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SLGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               default:
                  return false;
               }

            // Merge LR and the current inst into distinct Operants
            _cg->deleteInst(instr);
            _cg->replaceInst(current, newInstr);
            _cursor = instr->getNext();
            performed = true;
            TR::Instruction * s390NewInstr = newInstr;
            }
         return performed;
         }
      windowSize++;
      current = current->getNext();
      }

   return performed;
   }

void
TR_S390PostRAPeephole::markBlockThatModifiesRegister(TR::Instruction * cursor,
                                               TR::Register * targetReg)
   {
   // some stores use targetReg as part of source
   if (targetReg && !cursor->isStore() && !cursor->isCompare())
      {
      if (targetReg->getRegisterPair())
         {
         TR::RealRegister * lowReg = toRealRegister(targetReg->getLowOrder());
         TR::RealRegister * highReg = toRealRegister(targetReg->getHighOrder());
         lowReg->setModified(true);
         highReg->setModified(true);
         if (cursor->getOpCodeValue() == TR::InstOpCode::getLoadMultipleOpCode())
            {
            uint8_t numRegs = (lowReg->getRegisterNumber() - highReg->getRegisterNumber())-1;
            if (numRegs > 0)
               {
               for (uint8_t i=highReg->getRegisterNumber()+1; i++; i<= numRegs)
                  {
                  _cg->getS390Linkage()->getS390RealRegister(REGNUM(i))->setModified(true);
                  }
               }
            }
         }
      else
         {
         // some stores use targetReg as part of source
         TR::RealRegister * rReg = toRealRegister(targetReg);
         rReg->setModified(true);
         }
      }
   }

void
TR_S390PostRAPeephole::reloadLiteralPoolRegisterForCatchBlock()
   {
   // When dynamic lit pool reg is disabled, we lock R6 as dedicated lit pool reg.
   // This causes a failure when we come back to a catch block because the register context will not be preserved.
   // Hence, we can not assume that R6 will still contain the lit pool register and hence need to reload it.

   bool isZ10 = TR::Compiler->target.cpu.getS390SupportsZ10();

   // we only need to reload literal pool for Java on older z architecture on zos when on demand literal pool is off
   if ( TR::Compiler->target.isZOS() && !isZ10 && !_cg->isLiteralPoolOnDemandOn())
      {
      // check to make sure that we actually need to use the literal pool register
      TR::Snippet * firstSnippet = _cg->getFirstSnippet();
      if ((_cg->getLinkage())->setupLiteralPoolRegister(firstSnippet) > 0)
         {
         // the imm. operand will be patched when the actual address of the literal pool is known at binary encoding phase
         TR::S390RILInstruction * inst = (TR::S390RILInstruction *) generateRILInstruction(_cg, TR::InstOpCode::LARL, _cursor->getNode(), _cg->getLitPoolRealRegister(), reinterpret_cast<void*>(0xBABE), _cursor);
         inst->setIsLiteralPoolAddress();
         }
      }
   }

bool
TR_S390PostRAPeephole::DeadStoreToSpillReduction()
  {
  return false;
  }

bool
TR_S390PostRAPeephole::tryMoveImmediate()
  {
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
bool TR_S390PostRAPeephole::ReduceLHIToXR()
  {
  TR::S390RIInstruction* lhiInstruction = static_cast<TR::S390RIInstruction*>(_cursor);

  if (lhiInstruction->getSourceImmediate() == 0)
     {
     TR::Instruction* nextInstruction = lhiInstruction->getNext();

     while (nextInstruction != NULL && !nextInstruction->getOpCode().readsCC())
        {
        if (nextInstruction->getOpCode().setsCC() || nextInstruction->getNode()->getOpCodeValue() == TR::BBEnd)
           {
           TR::DebugCounter::incStaticDebugCounter(_cg->comp(), "z/peephole/LHI/XR");

           TR::Instruction* xrInstruction = generateRRInstruction(_cg, TR::InstOpCode::XR, lhiInstruction->getNode(), lhiInstruction->getRegisterOperand(1), lhiInstruction->getRegisterOperand(1));

           _cg->replaceInst(lhiInstruction, xrInstruction);

           _cursor = xrInstruction;

           return true;
           }

        nextInstruction = nextInstruction->getNext();
        }
     }

  return false;
  }

void
TR_S390PostRAPeephole::perform()
   {
   TR::Delimiter d(comp(), comp()->getOption(TR_TraceCG), "postRAPeephole");

   if (comp()->getOption(TR_TraceCG))
      printInfo("\nPost PostRegister Assignment Peephole Optimization Instructions:\n");

   bool moveInstr;

   while (_cursor != NULL)
      {
      if (_cursor->getNode()!= NULL && _cursor->getNode()->getOpCodeValue() == TR::BBStart)
         {
         comp()->setCurrentBlock(_cursor->getNode()->getBlock());
         // reload literal pool for catch blocks that need it
         TR::Block * blk = _cursor->getNode()->getBlock();
         if ( blk->isCatchBlock() && (blk->getFirstInstruction() == _cursor) )
            reloadLiteralPoolRegisterForCatchBlock();
         }

      if (_cursor->getOpCodeValue() != TR::InstOpCode::FENCE &&
          _cursor->getOpCodeValue() != TR::InstOpCode::ASSOCREGS &&
          _cursor->getOpCodeValue() != TR::InstOpCode::DEPEND)
         {
         TR::RegisterDependencyConditions * deps =  _cursor->getDependencyConditions();
         bool depCase = (_cursor->isBranchOp() || _cursor->isLabel()) && deps;
         if (depCase)
            {
            _cg->getS390Linkage()->markPreservedRegsInDep(deps);
            }

         //handle all other regs
         TR::Register *reg = _cursor->getRegisterOperand(1);
         markBlockThatModifiesRegister(_cursor, reg);
         }

      // this code is used to handle all compare instruction which sets the compare flag
      // we can eventually extend this to include other instruction which sets the
      // condition code and and uses a complemented register
      if (_cursor->getOpCode().setsCompareFlag() &&
          _cursor->getOpCodeValue() != TR::InstOpCode::CHLR &&
          _cursor->getOpCodeValue() != TR::InstOpCode::CLHLR)
         {
         trueCompEliminationForCompare();
         if (comp()->getOption(TR_TraceCG))
            printInst();
         }

      if (_cursor->isBranchOp())
         forwardBranchTarget();

      moveInstr = true;
      switch (_cursor->getOpCodeValue())
         {
         case TR::InstOpCode::AP:
         case TR::InstOpCode::SP:
         case TR::InstOpCode::MP:
         case TR::InstOpCode::DP:
         case TR::InstOpCode::CP:
         case TR::InstOpCode::SRP:
         case TR::InstOpCode::CVBG:
         case TR::InstOpCode::CVBY:
         case TR::InstOpCode::ZAP:
            if (!DAAHandleMemoryReferenceSpill(false))
               DAARemoveOutlinedLabelNop(false);
            break;

         case TR::InstOpCode::CVB:
            if (!DAAHandleMemoryReferenceSpill(true))
               DAARemoveOutlinedLabelNop(true);
            break;

         case TR::InstOpCode::CPYA:
            {
            LRReduction();
            break;
            }
         case TR::InstOpCode::IPM:
            {
            if (!branchReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;
            }
        case TR::InstOpCode::ST:
            {
            if(DeadStoreToSpillReduction())
              break;
            if(tryMoveImmediate())
              break;
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
        case TR::InstOpCode::STY:
            {
            if(DeadStoreToSpillReduction())
              break;
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
        case TR::InstOpCode::STG:
          if(DeadStoreToSpillReduction())
            break;
          if(tryMoveImmediate())
            break;
          if(comp()->getOption(TR_TraceCG))
            printInst();
          break;
        case TR::InstOpCode::STE:
        case TR::InstOpCode::STEY:
        case TR::InstOpCode::STD:
        case TR::InstOpCode::STDY:
           if (DeadStoreToSpillReduction())
              break;
           if (comp()->getOption(TR_TraceCG))
              printInst();
          break;
         case TR::InstOpCode::LDR:
            {
            LRReduction();
            break;
            }
         case TR::InstOpCode::L:
            {
            if (!ICMReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            LoadAndMaskReduction(TR::InstOpCode::LZRF);
            break;
            }
         case TR::InstOpCode::LA:
            {
            if (!LAReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;
            }

         case TR::InstOpCode::LHI:
            {
            // This optimization is disabled by default because there exist cases in which we cannot determine whether this transformation
            // is functionally valid or not. The issue resides in the various runtime patching sequences using the LHI instruction as a
            // runtime patch point for an offset. One concrete example can be found in the virtual dispatch sequence for unresolved calls
            // on 31-bit platforms where an LHI instruction is used and is patched at runtime.
            //
            // TODO (Issue #255): To enable this optimization we need to implement an API which marks instructions that will be patched at
            // runtime and prevent ourselves from modifying such instructions in any way.
            //
            // ReduceLHIToXR();
            }
            break;

         case TR::InstOpCode::EX:
            {
            static char * disableEXRLDispatch = feGetEnv("TR_DisableEXRLDispatch");

            if (_cursor->isOutOfLineEX() && !comp()->getCurrentBlock()->isCold() && !(bool)disableEXRLDispatch && _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
               inlineEXtarget();
            break;
            }
         case TR::InstOpCode::LHR:
            {
            break;
            }
         case TR::InstOpCode::LR:
         case TR::InstOpCode::LTR:
            {
            LRReduction();
            AGIReduction();
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }

            if (attemptZ7distinctOperants())
               {
               moveInstr = false;
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }

            break;
            }
         case TR::InstOpCode::LLC:
            {
            LLCReduction();
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }
            break;
            }
         case TR::InstOpCode::LLGF:
            {
            LoadAndMaskReduction(TR::InstOpCode::LLZRGF);
            break;
            }
         case TR::InstOpCode::LG:
            {
            LoadAndMaskReduction(TR::InstOpCode::LZRG);
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
         case TR::InstOpCode::LGR:
         case TR::InstOpCode::LTGR:
            {
            LRReduction();
            LGFRReduction();
            AGIReduction();
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }

            if (attemptZ7distinctOperants())
               {
               moveInstr = false;
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }

            break;
            }
         case TR::InstOpCode::CGIT:
            {
            if (TR::Compiler->target.is64Bit() && !removeMergedNullCHK())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;
            }
         case TR::InstOpCode::CRJ:
            {
            ConditionalBranchReduction(TR::InstOpCode::CR);

            trueCompEliminationForCompareAndBranch();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::CGRJ:
            {
            ConditionalBranchReduction(TR::InstOpCode::CGR);

            trueCompEliminationForCompareAndBranch();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::CRB:
         case TR::InstOpCode::CRT:
         case TR::InstOpCode::CGFR:
         case TR::InstOpCode::CGRT:
         case TR::InstOpCode::CLRB:
         case TR::InstOpCode::CLRJ:
         case TR::InstOpCode::CLRT:
         case TR::InstOpCode::CLGRB:
         case TR::InstOpCode::CLGFR:
         case TR::InstOpCode::CLGRT:
            {
            trueCompEliminationForCompareAndBranch();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::LCGFR:
         case TR::InstOpCode::LCGR:
         case TR::InstOpCode::LCR:
            {
            trueCompEliminationForLoadComp();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::SLLG:
         case TR::InstOpCode::SLAG:
         case TR::InstOpCode::SLLK:
         case TR::InstOpCode::SRLK:
         case TR::InstOpCode::SLAK:
         case TR::InstOpCode::SRAK:
            revertTo32BitShift();
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;

         case TR::InstOpCode::NILH:
            if (!NILHReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;

         case TR::InstOpCode::NILF:
            if (_cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILF)
               {
               TR::Instruction * na = _cursor;
               TR::Instruction * nb = _cursor->getNext();

               // want to delete nb if na == nb - use deleteInst

               if ((na->getKind() == TR::Instruction::IsRIL) &&
                   (nb->getKind() == TR::Instruction::IsRIL))
                  {
                  // NILF == generateRILInstruction
                  TR::S390RILInstruction * cast_na = (TR::S390RILInstruction *)na;
                  TR::S390RILInstruction * cast_nb = (TR::S390RILInstruction *)nb;

                  bool instructionsMatch =
                     (cast_na->getTargetPtr() == cast_nb->getTargetPtr()) &&
                     (cast_na->getTargetSnippet() == cast_nb->getTargetSnippet()) &&
                     (cast_na->getTargetSymbol() == cast_nb->getTargetSymbol()) &&
                     (cast_na->getTargetLabel() == cast_nb->getTargetLabel()) &&
                     (cast_na->getMask() == cast_nb->getMask()) &&
                     (cast_na->getSymbolReference() == cast_nb->getSymbolReference());

                  if (instructionsMatch)
                     {
                     if (cast_na->matchesTargetRegister(cast_nb->getRegisterOperand(1)) &&
                         cast_nb->matchesTargetRegister(cast_na->getRegisterOperand(1)))
                        {
                        if (cast_na->getSourceImmediate() == cast_nb->getSourceImmediate())
                           {
                           if (performTransformation(comp(), "O^O S390 PEEPHOLE: deleting duplicate NILF from pair %p %p*\n", _cursor, _cursor->getNext()))
                              {
                              _cg->deleteInst(_cursor->getNext());
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
                        else if (((cast_na->getSourceImmediate() & cast_nb->getSourceImmediate()) == cast_na->getSourceImmediate()) &&
                                 ((cast_nb->getSourceImmediate() & cast_na->getSourceImmediate()) != cast_nb->getSourceImmediate()))
                           {
                           if (performTransformation(comp(), "O^O S390 PEEPHOLE: deleting unnecessary NILF from pair %p %p*\n", _cursor, _cursor->getNext()))
                              {
                              _cg->deleteInst(_cursor->getNext());
                              }
                           }
                        }
                     }
                  }
               }
            break;

         case TR::InstOpCode::SRL:
         case TR::InstOpCode::SLL:
            {
            // Turn
            //     SRL      @00_@GN11888_VSWKSKEY,24
            //     SRL      @00_@GN11888_VSWKSKEY,4
            // into
            //     SRL      @00_@GN11888_VSWKSKEY,28
            if (_cursor->getNext()->getOpCodeValue() ==
                _cursor->getOpCodeValue())
               {
               TR::Instruction * na = _cursor;
               TR::Instruction * nb = _cursor->getNext();

               if ((na->getKind() == TR::Instruction::IsRS) &&
                   (nb->getKind() == TR::Instruction::IsRS))
                  {
                  // SRL == SLL == generateRSInstruction
                  TR::S390RSInstruction * cast_na = (TR::S390RSInstruction *)na;
                  TR::S390RSInstruction * cast_nb = (TR::S390RSInstruction *)nb;

                  bool instrMatch =
                        (cast_na->getFirstRegister() == cast_nb->getFirstRegister()) &&
                        (!cast_na->hasMaskImmediate()) && (!cast_nb->hasMaskImmediate()) &&
                        (cast_na->getMemoryReference() == NULL) && (cast_nb->getMemoryReference() == NULL);

                  if (instrMatch)
                     {
                     if (((cast_na->getSourceImmediate() + cast_nb->getSourceImmediate()) < 64) &&
                         performTransformation(comp(), "O^O S390 PEEPHOLE: merging SRL/SLL pair %p %p\n", _cursor, _cursor->getNext()))
                        {
                        // Delete the second instruction
                        uint32_t deletedImmediate = cast_nb->getSourceImmediate();
                        _cg->deleteInst(_cursor->getNext());
                        cast_na->setSourceImmediate(cast_na->getSourceImmediate() + deletedImmediate);
                        }
                     }
                  }
               }
            }
            break;

         case TR::InstOpCode::LGG:
         case TR::InstOpCode::LLGFSG:
            {
            replaceGuardedLoadWithSoftwareReadBarrier();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         default:
            {
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
         }

      if(moveInstr == true)
         _cursor = _cursor->getNext();
      }

   if (comp()->getOption(TR_TraceCG))
      printInfo("\n\n");
   }

bool TR_S390PostRAPeephole::forwardBranchTarget()
   {
   TR::LabelSymbol *targetLabelSym = NULL;
   switch(_cursor->getOpCodeValue())
      {
      case TR::InstOpCode::BRC: targetLabelSym = ((TR::S390BranchInstruction*)_cursor)->getLabelSymbol(); break;
      case TR::InstOpCode::CRJ:
      case TR::InstOpCode::CGRJ:
      case TR::InstOpCode::CIJ:
      case TR::InstOpCode::CGIJ:
      case TR::InstOpCode::CLRJ:
      case TR::InstOpCode::CLGRJ:
      case TR::InstOpCode::CLIJ:
      case TR::InstOpCode::CLGIJ: targetLabelSym = toS390RIEInstruction(_cursor)->getBranchDestinationLabel(); break;
      default:
         return false;
      }
   if (!targetLabelSym)
      return false;

   auto targetLabelInsn = targetLabelSym->getInstruction();
   if (!targetLabelInsn)
      return false;
   auto tmp = targetLabelInsn;
   while (tmp->isLabel() || tmp->getOpCodeValue() == TR::InstOpCode::FENCE)  // skip labels and fences
      tmp = tmp->getNext();
   if (tmp->getOpCodeValue() == TR::InstOpCode::BRC)
      {
      auto firstBranch = (TR::S390BranchInstruction*)tmp;
      if (firstBranch->getBranchCondition() == TR::InstOpCode::COND_BRC &&
          performTransformation(comp(), "\nO^O S390 PEEPHOLE: forwarding branch target in %p\n", _cursor))
         {
         auto newTargetLabelSym = firstBranch->getLabelSymbol();
         switch(_cursor->getOpCodeValue())
            {
            case TR::InstOpCode::BRC: ((TR::S390BranchInstruction*)_cursor)->setLabelSymbol(newTargetLabelSym); break;
            case TR::InstOpCode::CRJ:
            case TR::InstOpCode::CGRJ:
            case TR::InstOpCode::CIJ:
            case TR::InstOpCode::CGIJ:
            case TR::InstOpCode::CLRJ:
            case TR::InstOpCode::CLGRJ:
            case TR::InstOpCode::CLIJ:
            case TR::InstOpCode::CLGIJ:
               toS390RIEInstruction(_cursor)->setBranchDestinationLabel(newTargetLabelSym); break;
            default:
               return false;
            }
         return true;
         }
      }
   return false;
   }


