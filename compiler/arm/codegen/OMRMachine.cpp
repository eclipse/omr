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

#include <algorithm>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/ARMInstruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Machine_inlines.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

static void registerExchange(TR::Instruction     *precedingI,
                             TR_RegisterKinds    rk,
                             TR::RealRegister *tReg,
                             TR::RealRegister *sReg,
                             TR::RealRegister *mReg,
                             TR::CodeGenerator   *cg);

static void registerCopy(TR::Instruction     *precedingI,
                         TR_RegisterKinds    rk,
                         TR::RealRegister *tReg,
                         TR::RealRegister *sReg,
                         TR::CodeGenerator   *cg);

OMR::ARM::Machine::Machine(TR::CodeGenerator *cg) :
      OMR::Machine(cg)
   {
   self()->initializeRegisterFile();
   }


TR::RealRegister *OMR::ARM::Machine::findBestFreeRegister(TR_RegisterKinds rk,
      bool excludeGPR0,
      bool considerUnlatched,
      bool isSinglePrecision)
   {
   int first;
   int last;

   switch(rk)
      {
      case TR_GPR:
         first = TR::RealRegister::FirstGPR;
         last  = TR::RealRegister::LastGPR;
         break;
      #if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR_FPR:
         if (isSinglePrecision)
            {
             first = TR::RealRegister::FirstFSR;
             last  = TR::RealRegister::LastFSR;
            }
         else
            {
            first = TR::RealRegister::FirstFPR;
            last  = TR::RealRegister::LastFPR;
            }
         break;
      #endif
   }

   uint32_t            bestWeightSoFar = 0xffffffff;
   TR::RealRegister *freeRegister    = NULL;
   for (int i = first; i <= last; i++)
      {
      if ((_registerFile[i]->getState() == TR::RealRegister::Free ||
           (considerUnlatched &&
            _registerFile[i]->getState() == TR::RealRegister::Unlatched)) &&
          _registerFile[i]->getWeight() < bestWeightSoFar)
         {
         freeRegister    = _registerFile[i];
         bestWeightSoFar = freeRegister->getWeight();
         }
      }
   if (freeRegister!=NULL && freeRegister->getState()==TR::RealRegister::Unlatched)
      {
      freeRegister->setAssignedRegister(NULL);
      freeRegister->setState(TR::RealRegister::Free);
      }
   return freeRegister;
   }

TR::RealRegister *OMR::ARM::Machine::freeBestRegister(TR::Instruction     *currentInstruction,
                                                    TR_RegisterKinds    rk,
                                                    TR::RealRegister *forced,
                                                    bool                excludeGPR0,
                                                    bool isSinglePrecision)
   {
   TR::Register           *candidates[NUM_ARM_MAXR];
   TR::Compilation *comp = self()->cg()->comp();
   TR::MemoryReference *tmemref;
   TR_BackingStore       *location;
   TR::RealRegister    *best, *crtemp=NULL;
   TR::Instruction     *cursor;
   TR::Node               *currentNode = currentInstruction->getNode();
   TR_ARMOpCodes          opCode;
   int                    numCandidates = 0;
   int                    first, last;

   if (forced != NULL)
      {
      best = forced;
      candidates[0] = best->getAssignedRegister();
      }
   else
      {
      switch (rk)
         {
         case TR_GPR:
            first = TR::RealRegister::FirstGPR;
            last = TR::RealRegister::LastGPR;
            break;
         case TR_FPR:
            if (isSinglePrecision)
               {
               first = TR::RealRegister::FirstFSR;
               last = TR::RealRegister::LastFSR;
               }
            else
               {
               first = TR::RealRegister::FirstFPR;
               last = TR::RealRegister::LastFPR;
               }
            break;
         }

      for (int i  = first; i <= last; i++)
         {
         TR::RealRegister *realReg = self()->getRealRegister((TR::RealRegister::RegNum)i);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            candidates[numCandidates++] = realReg->getAssignedRegister();
            }
         }

      cursor = currentInstruction;
      while (numCandidates > 1                 &&
             cursor != NULL                    &&
             cursor->getOpCodeValue() != ARMOp_label &&
             cursor->getOpCodeValue() != ARMOp_proc)
         {
         for (int i = 0; i < numCandidates; i++)
            {
            if (cursor->refsRegister(candidates[i]))
               {
               candidates[i] = candidates[--numCandidates];
               }
            }
         cursor = cursor->getPrev();
         }
      best = toRealRegister(candidates[0]->getAssignedRegister());
      }

   switch (rk)
      {
      case TR_GPR:
         if (candidates[0]->getBackingStorage())
            {
            // If there is backing storage associated with a register, it means the
            // backing store wasn't returned to the free list and it can be used.
            //
            location = candidates[0]->getBackingStorage();
            if (!location->isOccupied())
               {
               // If best register already has a backing store it's because we reverse spilled it in an
               // OOL region while the free spill list was locked and we didn't clean this up after unlocking
               // the list (see TODO in ARMInstruction.cpp). Therefore we need to set the occupied flag for this reuse.
               location->setIsOccupied();
               }
            else
               {
               location = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), candidates[0]->containsCollectedReference(), NULL);
               }
            }
         else
            {
            location = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), candidates[0]->containsCollectedReference(), NULL);
            }
         break;

      case TR_FPR:
         //  Restore this segment of codes when we floatSpill is implemented.
         //
         //cursor = currentInstruction->getPrev();
         //while (cursor != NULL &&
         //!(cursor->refsRegister(candidates[0])))
         //  cursor = cursor->getPrev();
         //if (!cursor->getOpCode().doubleFPOp())
         //  location = self()->cg()->getFreeLocalFloatSpill();
         //else
         if (candidates[0]->getBackingStorage())
            {
            // If there is backing storage associated with a register, it means the
            // backing store wasn't returned to the free list and it can be used.
            //
            location = candidates[0]->getBackingStorage();

            if (!location->isOccupied())
               {
               // If best register already has a backing store it's because we reverse spilled it in an
               // OOL region while the free spill list was locked and we didn't clean this up after unlocking
               // the list (see TODO in ARMInstruction.cpp). Therefore we need to set the occupied flag for this reuse.
               location->setIsOccupied();
               }
            else
               {
               location = self()->cg()->allocateSpill(8, false, NULL);
               }
            }
         else
            {
            location = self()->cg()->allocateSpill(8, false, NULL); // TODO: use 4 when floatSpill is implemented
            }
         break;
      }
   candidates[0]->setBackingStorage(location);

   tmemref = new (self()->cg()->trHeapMemory()) TR::MemoryReference(currentNode, location->getSymbolReference(), ((rk == TR_FPR) ? 8 : 4), self()->cg());
   if (!comp->getOption(TR_DisableOOL))
      {
      if (!self()->cg()->isOutOfLineColdPath())
         {
         TR_Debug   *debugObj = self()->cg()->getDebug();
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL cold path, post dependencies will be generated using this list
         self()->cg()->getSpilledRegisterList()->push_front(candidates[0]);

         // OOL cold path: depth = 3, hot path: depth =2,  main line: depth = 1
         // if the spill is outside of the OOL cold/hot path, we need to protect the spill slot
         // if we reverse spill this register inside the OOL cold/hot path
         if (!self()->cg()->isOutOfLineHotPath())
            {// main line
            location->setMaxSpillDepth(1);
            }
         else
            {
            // hot path
            // do not overwrite main line spill depth
            if (location->getMaxSpillDepth() != 1)
               location->setMaxSpillDepth(2);
            }
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: adding %s to the spilledRegisterList, maxSpillDepth = %d ",
                                          debugObj->getName(candidates[0]), location->getMaxSpillDepth());
         }
      else
         {
         // do not overwrite mainline and hot path spill depth
         // if this spill is inside OOL cold path, we do not need to protecting the spill slot
         // because the post condition at OOL entry does not expect this register to be spilled
         if (location->getMaxSpillDepth() != 1 &&
             location->getMaxSpillDepth() != 2 )
            {
            location->setMaxSpillDepth(3);
            self()->cg()->traceRegisterAssignment("OOL: In OOL cold path, spilling %s not adding to spilledRegisterList", (candidates[0])->getRegisterName(self()->cg()->comp()));
            }
         }
      }

   if (self()->cg()->comp()->getOption(TR_TraceCG))
      {
      diagnostic("\n\tspilling %s (%s)",
                  best->getAssignedRegister()->getRegisterName(self()->cg()->comp()),
                  best->getRegisterName(self()->cg()->comp()));
      }

   switch (rk)
      {
      case TR_GPR:
         generateTrg1MemInstruction(self()->cg(), ARMOp_ldr, currentNode, best, tmemref, currentInstruction);
         break;

      case TR_FPR:
         //if targetRegister is FS0-FS15, using ARMOp_flds.
         generateTrg1MemInstruction(self()->cg(), (isSinglePrecision ? ARMOp_flds : ARMOp_fldd), currentNode, best, tmemref, currentInstruction);
         break;

      }

   best->setAssignedRegister(NULL);
   best->setState(TR::RealRegister::Free);
   candidates[0]->setAssignedRegister(NULL);
   return best;
   }

TR::RealRegister *OMR::ARM::Machine::reverseSpillState(TR::Instruction      *currentInstruction,
                                                     TR::Register         *spilledRegister,
                                                     TR::RealRegister  *targetRegister,
                                                     bool                excludeGPR0,
                                                     bool isSinglePrecision)
   {
   TR::MemoryReference *tmemref;
   TR::RealRegister    *sameReg, *crtemp   = NULL;
   TR_BackingStore       *location = spilledRegister->getBackingStorage();
   TR::Node               *currentNode = currentInstruction->getNode();
   TR_RegisterKinds       rk       = spilledRegister->getKind();
   TR_ARMOpCodes          opCode;
   TR_Debug          *debugObj = self()->cg()->getDebug();

   if (targetRegister == NULL)
      {
      targetRegister = self()->findBestFreeRegister(rk, excludeGPR0);
      if (targetRegister == NULL)
         {
         targetRegister = self()->freeBestRegister(currentInstruction, rk, NULL, excludeGPR0);
         }
      targetRegister->setState(TR::RealRegister::Assigned);
      }
   if (self()->cg()->isOutOfLineColdPath())
      {
      // the future and total use count might not always reflect register spill state
      // for example a new register assignment in the hot path would cause FC != TC
      // in this case, assign a new register and return
      if (!location)
         {
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: Not generating reverse spill for (%s)\n", debugObj->getName(spilledRegister));
         return targetRegister;
         }
      }
   if (self()->cg()->comp()->getOption(TR_TraceCG))
      {
      diagnostic("\n\tre-assigning spilled %s to %s",
                  spilledRegister->getRegisterName(self()->cg()->comp()),
                  targetRegister->getRegisterName(self()->cg()->comp()));
      }

   tmemref = new (self()->cg()->trHeapMemory()) TR::MemoryReference(currentNode, location->getSymbolReference(), ((rk == TR_FPR) ? 8 : 4), self()->cg());

   if (self()->cg()->comp()->getOption(TR_DisableOOL))
      {
      switch (rk)
         {
         case TR_GPR:
            self()->cg()->freeSpill(location, TR::Compiler->om.sizeofReferenceAddress(), 0);
            generateMemSrc1Instruction(self()->cg(), ARMOp_str, currentNode, tmemref, targetRegister, currentInstruction);
            break;
         case TR_FPR:
            self()->cg()->freeSpill(location, 8, 0); // TODO: use 4 when floatSpill is implemented
            //if targetRegister is FS0-FS15, using ARMOp_fsts.
            generateMemSrc1Instruction(self()->cg(), (isSinglePrecision ? ARMOp_fsts : ARMOp_fstd), currentNode, tmemref, targetRegister, currentInstruction);
            break;
         }
      }
   else
      {
      int32_t dataSize = 0; // GARY OOL
      switch (rk)
         {
         case TR_GPR:
            dataSize = TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR_FPR:
            dataSize = 8; // TODO: use 4 when floatSpill is implemented
            break;
         }
      if (self()->cg()->isOutOfLineColdPath())
         {
         bool isOOLentryReverseSpill = false;
         if (currentInstruction->isLabel())
            {
            if (((TR::ARMLabelInstruction*)currentInstruction)->getLabelSymbol()->isStartOfColdInstructionStream())
               {
               // indicates that we are at OOL entry point post conditions. Since
               // we are now exiting the OOL cold path (going reverse order)
               // and we called reverseSpillState(), the main line path
               // expects the Virt reg to be assigned to a real register
               // we can now safely unlock the protected backing storage
               // This prevents locking backing storage for future OOL blocks
               isOOLentryReverseSpill = true;
               }
            }
         // OOL: only free the spill slot if the register was spilled in the same or less dominant path
         // ex: spilled in cold path, reverse spill in hot path or main line
         // we have to spill this register again when we reach OOL entry point due to post
         // conditions. We want to guarantee that the same spill slot will be protected and reused.
         // maxSpillDepth: 3:cold path, 2:hot path, 1:main line
         // Also free the spill if maxSpillDepth==0, which will be the case if the reverse spill also occured on the hot path.
         // If the reverse spill occured on both paths then this is the last chance we have to free the spill slot.
         if (location->getMaxSpillDepth() == 3 || location->getMaxSpillDepth() == 0 || isOOLentryReverseSpill)
            {
            if (location->getMaxSpillDepth() != 0)
               location->setMaxSpillDepth(0);
            else if (debugObj)
               self()->cg()->traceRegisterAssignment("\nOOL: reverse spill %s in less dominant path (%d / 3), reverse spill on both paths indicated, free spill slot (%p)\n",
                                             debugObj->getName(spilledRegister), location->getMaxSpillDepth(), location);
            self()->cg()->freeSpill(location, dataSize, 0);
            if (!self()->cg()->isFreeSpillListLocked())
               {
               spilledRegister->setBackingStorage(NULL);
               }
            }
         else
            {
            if (debugObj)
               self()->cg()->traceRegisterAssignment("\nOOL: reverse spill %s in less dominant path (%d / 3), protect spill slot (%p)\n",
                                             debugObj->getName(spilledRegister), location->getMaxSpillDepth(), location);
            }
         }
      else if (self()->cg()->isOutOfLineHotPath())
         {
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL path (in backwards RA). Post dependencies will be generated using this list.
         // Any registers reverse spilled before entering OOL should be removed from the spilled list
         if (debugObj)
            self()->cg()->traceRegisterAssignment("\nOOL: removing %s from the spilledRegisterList\n", debugObj->getName(spilledRegister));
         self()->cg()->getSpilledRegisterList()->remove(spilledRegister);
         // Reset maxSpillDepth here so that in the cold path we know to free the spill
         // and so that the spill is not included in future GC points in the hot path while it is protected
         location->setMaxSpillDepth(0);
         if (location->getMaxSpillDepth() == 2)
            {
            self()->cg()->freeSpill(location, dataSize, 0);
            if (!self()->cg()->isFreeSpillListLocked())
               {
               spilledRegister->setBackingStorage(NULL);
               }
            }
         else
            {
            if (debugObj)
               self()->cg()->traceRegisterAssignment("\nOOL: reverse spilling %s in less dominant path (%d / 2), protect spill slot (%p)\n",
                                             debugObj->getName(spilledRegister), location->getMaxSpillDepth(), location);
            }
         }
      else // main line
         {
         if (debugObj)
            self()->cg()->traceRegisterAssignment("\nOOL: removing %s from the spilledRegisterList)\n", debugObj->getName(spilledRegister));
         self()->cg()->getSpilledRegisterList()->remove(spilledRegister);
         location->setMaxSpillDepth(0);
         self()->cg()->freeSpill(location, dataSize, 0);
         if (!self()->cg()->isFreeSpillListLocked())
            {
            spilledRegister->setBackingStorage(NULL);
            }
         }
      switch (rk)
         {
         case TR_GPR:
            generateMemSrc1Instruction(self()->cg(), ARMOp_str, currentNode, tmemref, targetRegister, currentInstruction);
            break;
         case TR_FPR:
             //if targetRegister is FS0-FS15, using ARMOp_fsts.
             generateMemSrc1Instruction(self()->cg(), (isSinglePrecision ? ARMOp_fsts : ARMOp_fstd), currentNode, tmemref, targetRegister, currentInstruction);
            break;
         }
      }
   return targetRegister;
   }

void OMR::ARM::Machine::coerceRegisterAssignment(TR::Instruction                           *currentInstruction,
                                              TR::Register                             *virtualRegister,
                                              TR::RealRegister::RegNum registerNumber)
   {
   TR::RealRegister *targetRegister          = _registerFile[registerNumber];
   TR::RealRegister    *realReg                 = virtualRegister->getAssignedRealRegister();
   TR::RealRegister *currentAssignedRegister = realReg ? realReg : NULL;
   TR::RealRegister *spareReg;
   TR::Register        *currentTargetVirtual;
   TR_RegisterKinds    rk = virtualRegister->getKind();

   if (self()->cg()->comp()->getOption(TR_TraceCG))
      {
      if (currentAssignedRegister)
         diagnostic("\n\tcoercing %s from %s to %s",
                     virtualRegister->getRegisterName(self()->cg()->comp()),
                     currentAssignedRegister->getRegisterName(self()->cg()->comp()),
                     targetRegister->getRegisterName(self()->cg()->comp()));
      else
         diagnostic("\n\tcoercing %s to %s",
                     virtualRegister->getRegisterName(self()->cg()->comp()),
                     targetRegister->getRegisterName(self()->cg()->comp()));
      }

   if (currentAssignedRegister == targetRegister)
       return;
   if (  targetRegister->getState() == TR::RealRegister::Free
      || targetRegister->getState() == TR::RealRegister::Unlatched)
      {
#ifdef DEBUG
      if (self()->cg()->comp()->getOption(TR_TraceCG))
         diagnostic(", which is free");
#endif
      if (currentAssignedRegister == NULL)
         {
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister, false, isSinglePrecision(registerNumber));
            }
         }
      else
         {
         registerCopy(currentInstruction, rk, currentAssignedRegister, targetRegister, self()->cg());
         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }
      }
   else if (targetRegister->getState() == TR::RealRegister::Blocked)
      {
      currentTargetVirtual = targetRegister->getAssignedRegister();
#ifdef DEBUG
      if (self()->cg()->comp()->getOption(TR_TraceCG))
         diagnostic(", which is blocked and assigned to %s",
                     currentTargetVirtual->getRegisterName(self()->cg()->comp()));
#endif
      if (!currentAssignedRegister || rk != TR_GPR)
         {
         spareReg = self()->findBestFreeRegister(rk);
         if (spareReg == NULL)
            {
            virtualRegister->block();
            spareReg = self()->freeBestRegister(currentInstruction, rk);
            virtualRegister->unblock();
            }
         }

      if (currentAssignedRegister)
         {
         registerExchange(currentInstruction, rk, targetRegister, currentAssignedRegister, spareReg, self()->cg());
         currentAssignedRegister->setState(TR::RealRegister::Blocked);
         currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
         currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
         // For Non-GPR, spareReg remains FREE.
         }
      else
         {
         registerCopy(currentInstruction, rk, targetRegister, spareReg, self()->cg());
         spareReg->setState(TR::RealRegister::Blocked);
         currentTargetVirtual->setAssignedRegister(spareReg);
         spareReg->setAssignedRegister(currentTargetVirtual);
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         // spareReg is assigned.
         }
      }
   else if (targetRegister->getState() == TR::RealRegister::Assigned)
      {
      currentTargetVirtual = targetRegister->getAssignedRegister();
#ifdef DEBUG
      if (self()->cg()->comp()->getOption(TR_TraceCG))
         diagnostic(", which is assigned to %s",
                     currentTargetVirtual->getRegisterName(self()->cg()->comp()));
#endif
      spareReg = self()->findBestFreeRegister(rk, false, false, isSinglePrecision(registerNumber));
      if (currentAssignedRegister != NULL)
         {
         TR_ASSERT(!isSinglePrecision(registerNumber), "Dummy FS0-FS15 registers must not be used after this instruction.");
         if ((rk != TR_GPR) && (spareReg == NULL))
            {
            self()->freeBestRegister(currentInstruction, rk, targetRegister);
            }
         else
            {
            registerExchange(currentInstruction, rk, targetRegister,
                 currentAssignedRegister, spareReg, self()->cg());
            currentAssignedRegister->setState(TR::RealRegister::Assigned);
            currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
            // spareReg is still FREE.
            }
         }
      else
         {
         if (spareReg == NULL)
            {
            self()->freeBestRegister(currentInstruction, rk, targetRegister, false, isSinglePrecision(registerNumber));
            }
         else
            {
            TR_ASSERT(!isSinglePrecision(registerNumber), "We want to free all the registers between FS0 and FS15 before DirectToJNI call.");
            registerCopy(currentInstruction, rk, targetRegister, spareReg, self()->cg());
            spareReg->setState(TR::RealRegister::Assigned);
            spareReg->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(spareReg);
            // spareReg is assigned.
            }
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            TR_ASSERT(!isSinglePrecision(registerNumber), "Dummy FS0-FS15 registers must not be used after this instruction.");
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }
      }
   else
      {
#ifdef DEBUG
      if (self()->cg()->comp()->getOption(TR_TraceCG))
         diagnostic(", which is in an unknown state %d", targetRegister->getState());
#endif
      }

   targetRegister->setState(TR::RealRegister::Assigned);
   targetRegister->setAssignedRegister(virtualRegister);
   virtualRegister->setAssignedRegister(targetRegister);
   }


void OMR::ARM::Machine::initializeRegisterFile()
   {
   self()->cg()->_unlatchedRegisterList =
   (TR::RealRegister**)self()->cg()->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters + 1));
   self()->cg()->_unlatchedRegisterList[0] = 0; // mark that list is empty

   _registerFile[TR::RealRegister::NoReg] = NULL;
   _registerFile[TR::RealRegister::SpilledReg] = NULL;
   _registerFile[TR::RealRegister::gr0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr0,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr1,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr2,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr3,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr4,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr5,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr6,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr7,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr8,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr9,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr10,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr11,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr12,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr13,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr14,
                                                 self()->cg());

   _registerFile[TR::RealRegister::gr15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr15,
                                                 self()->cg());

  // need only f0 for floating point return from jni (on some platforms)
   _registerFile[TR::RealRegister::fp0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp0, self()->cg());

   #if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   _registerFile[TR::RealRegister::fp1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp1, self()->cg());

   _registerFile[TR::RealRegister::fp2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp2, self()->cg());

   _registerFile[TR::RealRegister::fp3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp3, self()->cg());

   _registerFile[TR::RealRegister::fp4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp4, self()->cg());

   _registerFile[TR::RealRegister::fp5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp5, self()->cg());

   _registerFile[TR::RealRegister::fp6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp6, self()->cg());

   _registerFile[TR::RealRegister::fp7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp7, self()->cg());

   _registerFile[TR::RealRegister::fp8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp8, self()->cg());

   _registerFile[TR::RealRegister::fp9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp9, self()->cg());

   _registerFile[TR::RealRegister::fp10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp10, self()->cg());

   _registerFile[TR::RealRegister::fp11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp11, self()->cg());

   _registerFile[TR::RealRegister::fp12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp12, self()->cg());

   _registerFile[TR::RealRegister::fp13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp13, self()->cg());

   _registerFile[TR::RealRegister::fp14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp14, self()->cg());

   _registerFile[TR::RealRegister::fp15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp15, self()->cg());

   /* For fs0~fs15 (FSR) */
   _registerFile[TR::RealRegister::fs0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs0, self()->cg());

   _registerFile[TR::RealRegister::fs1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs1, self()->cg());

   _registerFile[TR::RealRegister::fs2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs2, self()->cg());

   _registerFile[TR::RealRegister::fs3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs3, self()->cg());

   _registerFile[TR::RealRegister::fs4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs4, self()->cg());

   _registerFile[TR::RealRegister::fs5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs5, self()->cg());

   _registerFile[TR::RealRegister::fs6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs6, self()->cg());

   _registerFile[TR::RealRegister::fs7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs7, self()->cg());

   _registerFile[TR::RealRegister::fs8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs8, self()->cg());

   _registerFile[TR::RealRegister::fs9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs9, self()->cg());

   _registerFile[TR::RealRegister::fs10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs10, self()->cg());

   _registerFile[TR::RealRegister::fs11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs11, self()->cg());

   _registerFile[TR::RealRegister::fs12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs12, self()->cg());

   _registerFile[TR::RealRegister::fs13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs13, self()->cg());

   _registerFile[TR::RealRegister::fs14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs14, self()->cg());

   _registerFile[TR::RealRegister::fs15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fs15, self()->cg());

   #endif
   }

static void registerCopy(TR::Instruction     *precedingInstruction,
                         TR_RegisterKinds    rk,
                         TR::RealRegister *targetReg,
                         TR::RealRegister *sourceReg,
                         TR::CodeGenerator   *cg)
   {
   TR::Node *node = precedingInstruction->getNode();
   switch (rk)
      {
      case TR_GPR:
         new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(precedingInstruction, ARMOp_mov, node, targetReg, sourceReg, cg);
         break;
      case TR_FPR:
         bool isTargetSinglePrecision = isSinglePrecision(targetReg->getRegisterNumber());
         bool isSourceSinglePrecision = isSinglePrecision(sourceReg->getRegisterNumber());
         if (isTargetSinglePrecision)
            {
            if (isSourceSinglePrecision)
               {
                new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(precedingInstruction, ARMOp_fcpys, node, targetReg, sourceReg, cg);
               }
            else
               {
               generateTrg1Src1Instruction(cg, ARMOp_fcvtsd, node, targetReg, sourceReg);
               }
            }
         else
            {
            if (isSourceSinglePrecision)
               {
               generateTrg1Src1Instruction(cg, ARMOp_fcvtds, node, targetReg, sourceReg);
               }
            else
               {
               new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(precedingInstruction, ARMOp_fcpyd, node, targetReg, sourceReg, cg);
               }
           }
         break;
      }
   }

static void registerExchange(TR::Instruction     *precedingInstruction,
                             TR_RegisterKinds    rk,
                             TR::RealRegister *targetReg,
                             TR::RealRegister *sourceReg,
                             TR::RealRegister *middleReg,
                             TR::CodeGenerator   *cg)
   {
   // middleReg is not used if rk==TR_GPR.

   TR::Node *node = precedingInstruction->getNode();
   if (rk != TR_GPR)
      {
      registerCopy(precedingInstruction, rk, targetReg, middleReg, cg);
      registerCopy(precedingInstruction, rk, sourceReg, targetReg, cg);
      registerCopy(precedingInstruction, rk, middleReg, sourceReg, cg);
      }
   else
      {
      new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(precedingInstruction, ARMOp_eor, node, targetReg, targetReg, sourceReg, cg);
      new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(precedingInstruction, ARMOp_eor, node, sourceReg, targetReg, sourceReg, cg);
      new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(precedingInstruction, ARMOp_eor, node, targetReg, targetReg, sourceReg, cg);
      }
   }

TR::RealRegister *OMR::ARM::Machine::assignSingleRegister(TR::Register *virtualRegister, TR::Instruction *currentInstruction)
   {
   TR::RealRegister       *assignedRegister = virtualRegister->getAssignedRealRegister();
   TR_RegisterKinds       kindOfRegister = virtualRegister->getKind();

   if (assignedRegister == NULL)
      {
      if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
         {
         assignedRegister = self()->reverseSpillState(currentInstruction, virtualRegister, NULL, false);
         }
      else
         {
         if ((assignedRegister = self()->findBestFreeRegister(kindOfRegister, false, true)) == NULL)
            {
            assignedRegister = self()->freeBestRegister(currentInstruction, kindOfRegister, NULL, false);
            }
         }
      virtualRegister->setAssignedRegister(assignedRegister);
      assignedRegister->setAssignedRegister(virtualRegister);
      assignedRegister->setState(TR::RealRegister::Assigned);
      }

   if (virtualRegister->decFutureUseCount() == 0)
      {
      virtualRegister->setAssignedRegister(NULL);
      assignedRegister->setState(TR::RealRegister::Unlatched);
      }
   return assignedRegister;
   }

uint32_t OMR::ARM::Machine::_globalRegisterNumberToRealRegisterMap[MAX_ARM_GLOBAL_GPRS + MAX_ARM_GLOBAL_FPRS] =
   {
   TR::RealRegister::gr9,
   TR::RealRegister::gr10,
   TR::RealRegister::gr11,
   TR::RealRegister::gr6, // BP; cannot offer if debug("hasFramePointer")
   TR::RealRegister::gr4,
   TR::RealRegister::gr5,
   TR::RealRegister::gr3,
   TR::RealRegister::gr2,
   TR::RealRegister::gr1,
   TR::RealRegister::gr0,
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::RealRegister::fp15,
   TR::RealRegister::fp14,
   TR::RealRegister::fp13,
   TR::RealRegister::fp12,
   TR::RealRegister::fp11,
   TR::RealRegister::fp10,
   TR::RealRegister::fp9,
   TR::RealRegister::fp8,
#endif
   TR::RealRegister::fp7,
   TR::RealRegister::fp6,
   TR::RealRegister::fp5,
   TR::RealRegister::fp4,
   TR::RealRegister::fp3,
   TR::RealRegister::fp2,
   TR::RealRegister::fp1,
   TR::RealRegister::fp0,
   };

TR::RegisterDependencyConditions*
OMR::ARM::Machine::createCondForLiveAndSpilledGPRs(bool cleanRegState, TR::list<TR::Register*> *spilledRegisterList)
   {
   // Calculate number of register dependencies required.  This step is not really necessary, but
   // it is space conscious.
   //
   int32_t c = 0;
   int32_t endReg = TR::RealRegister::LastFPR;

   for (int32_t i = TR::RealRegister::FirstGPR; i <= endReg; i++)
      {
      TR::RealRegister *realReg = self()->getRealRegister((TR::RealRegister::RegNum)i);
      TR_ASSERT(realReg->getState() == TR::RealRegister::Assigned ||
              realReg->getState() == TR::RealRegister::Free ||
              realReg->getState() == TR::RealRegister::Locked,
              "cannot handle realReg state %d, (block state is %d)\n",realReg->getState(),TR::RealRegister::Blocked);
      if (realReg->getState() == TR::RealRegister::Assigned)
         c++;
      }

   c += spilledRegisterList ? spilledRegisterList->size() : 0;

   TR::RegisterDependencyConditions *deps = NULL;

   if (c)
      {
      deps = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, c, self()->cg()->trMemory());
      for (int32_t j = TR::RealRegister::FirstGPR; j <= endReg; j++)
         {
         TR::RealRegister *realReg = self()->getRealRegister((TR::RealRegister::RegNum)j);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            TR::Register *virtReg = realReg->getAssignedRegister();
            TR_ASSERT(!spilledRegisterList || !(std::find(spilledRegisterList->begin(), spilledRegisterList->end(), virtReg) != spilledRegisterList->end())
            		,"a register should not be in both an assigned state and in the spilled list\n");
            deps->addPostCondition(virtReg, realReg->getRegisterNumber());
            if (cleanRegState)
               {
               virtReg->incTotalUseCount();
               virtReg->incFutureUseCount();
               virtReg->setAssignedRegister(NULL);
               realReg->setAssignedRegister(NULL);
               realReg->setState(TR::RealRegister::Free);
               }
            }
         }

      if (spilledRegisterList)
         {
         for(auto i = spilledRegisterList->begin(); i != spilledRegisterList->end(); ++i)
            {
        	deps->addPostCondition(*i, TR::RealRegister::SpilledReg);
            }
         }
      }

   return deps;
   }

void
OMR::ARM::Machine::takeRegisterStateSnapShot()
   {
   int32_t i;
   TR::Compilation *comp = self()->cg()->comp();
   for (i = TR::RealRegister::FirstGPR; i < TR::RealRegister::gr12; i++) // Skipping the special/reserved register.  Add FPR later
      {
      //_registerAssociationsSnapShot[i] = _registerAssociations[i];
      _registerStatesSnapShot[i] = _registerFile[i]->getState();
      _assignedRegisterSnapShot[i] = _registerFile[i]->getAssignedRegister();
      _registerFlagsSnapShot[i] = _registerFile[i]->getFlags();
      if (comp->getOption(TR_TraceRA))
         traceMsg(comp,"OOL: Taking snap shot %d, %x, %x, %x\n", i, _registerStatesSnapShot[i], _assignedRegisterSnapShot[i], _registerFlagsSnapShot[i]);
      }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   //snapshot is used only for OOL. For now, single precision registers are used only for DirectToJNI.
   for (i = TR::RealRegister::FirstFPR; i < TR::RealRegister::LastFPR; i++) // Skipping the special/reserved register.  Add FPR later
      {
      _registerStatesSnapShot[i] = _registerFile[i]->getState();
      _assignedRegisterSnapShot[i] = _registerFile[i]->getAssignedRegister();
      _registerFlagsSnapShot[i] = _registerFile[i]->getFlags();
      if (comp->getOption(TR_TraceRA))
         traceMsg(comp,"OOL: Taking snap shot %d, %x, %x, %x\n", i, _registerStatesSnapShot[i], _assignedRegisterSnapShot[i], _registerFlagsSnapShot[i]);
      }
#endif
   }

void
OMR::ARM::Machine::restoreRegisterStateFromSnapShot()
   {
   int32_t i;
   TR::Compilation *comp = self()->cg()->comp();
   for (i = TR::RealRegister::FirstGPR; i < TR::RealRegister::gr12; i++) // Skipping SpilledReg
      {
      _registerFile[i]->setFlags(_registerFlagsSnapShot[i]);
      _registerFile[i]->setState(_registerStatesSnapShot[i]);
      //if (_registerAssociationsSnapShot[i])
      //   setVirtualAssociatedWithReal((TR::RealRegister::RegNum)(i), _registerAssociationsSnapShot[i]);
      if (_registerFile[i]->getState() == TR::RealRegister::Free)
         {
         if (_registerFile[i]->getAssignedRegister() != NULL)
            {
            // clear the Virt -> Real reg assignment if we restored the Real reg state to FREE
            _registerFile[i]->getAssignedRegister()->setAssignedRegister(NULL);
            }
         }
      _registerFile[i]->setAssignedRegister(_assignedRegisterSnapShot[i]);
      // make sure to double link virt - real reg if assigned
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         _registerFile[i]->getAssignedRegister()->setAssignedRegister(_registerFile[i]);
         }
      if (comp->getOption(TR_TraceRA))
         traceMsg(comp,"OOL: Restoring snap shot %d, %x, %x, %x\n", i, _registerFile[i]->getState(), _registerFile[i]->getAssignedRegister(), _registerFile[i]->getFlags());
      }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   //snapshot is used only for OOL. For now, single precision registers are used only for DirectToJNI.
   for (i = TR::RealRegister::FirstFPR; i < TR::RealRegister::LastFPR; i++) // Skipping SpilledReg
      {
      _registerFile[i]->setFlags(_registerFlagsSnapShot[i]);
      _registerFile[i]->setState(_registerStatesSnapShot[i]);
      if (_registerFile[i]->getState() == TR::RealRegister::Free)
         {
         if (_registerFile[i]->getAssignedRegister() != NULL)
            {
            // clear the Virt -> Real reg assignment if we restored the Real reg state to FREE
            _registerFile[i]->getAssignedRegister()->setAssignedRegister(NULL);
            }
         }
      _registerFile[i]->setAssignedRegister(_assignedRegisterSnapShot[i]);
      // make sure to double link virt - real reg if assigned
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         _registerFile[i]->getAssignedRegister()->setAssignedRegister(_registerFile[i]);
         }
      if (comp->getOption(TR_TraceRA))
         traceMsg(comp,"OOL: Restoring snap shot %d, %x, %x, %x\n", i, _registerFile[i]->getState(), _registerFile[i]->getAssignedRegister(), _registerFile[i]->getFlags());
      }
#endif
   }
