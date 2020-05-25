/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include <stddef.h> // for NULL, etc
#include <stdint.h> // for uint16_t, int32_t, etc

#include "codegen/RVInstruction.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Machine_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "infra/Assert.hpp"

OMR::RV::Machine::Machine(TR::CodeGenerator *cg) :
      OMR::Machine(cg)
   {
   self()->initializeRegisterFile();
   }

TR::RealRegister *OMR::RV::Machine::findBestFreeRegister(TR_RegisterKinds rk,
                                                            bool considerUnlatched)
   {
   int32_t first;
   int32_t last;

   switch(rk)
      {
      case TR_GPR:
         first = TR::RealRegister::FirstGPR;
         last  = TR::RealRegister::LastAssignableGPR;
         break;
      case TR_FPR:
         first = TR::RealRegister::FirstFPR;
         last  = TR::RealRegister::LastFPR;
         break;
      default:
         TR_ASSERT(false, "Unsupported RegisterKind.");
   }

   uint32_t bestWeightSoFar = 0xffffffff;
   TR::RealRegister *freeRegister = NULL;
   for (int32_t i = first; i <= last; i++)
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
   if (freeRegister != NULL && freeRegister->getState() == TR::RealRegister::Unlatched)
      {
      freeRegister->setAssignedRegister(NULL);
      freeRegister->setState(TR::RealRegister::Free);
      }
   return freeRegister;
   }

TR::RealRegister *OMR::RV::Machine::freeBestRegister(TR::Instruction *currentInstruction,
                                                        TR::Register *virtualRegister,
                                                        TR::RealRegister *forced)
   {
   TR::Register *candidates[NUM_RV_MAXR];
   TR::Compilation *comp = self()->cg()->comp();
   TR::MemoryReference *tmemref;
   TR_BackingStore *location;
   TR::RealRegister *best;
   TR::Instruction *cursor;
   TR::Node *currentNode = currentInstruction->getNode();
   TR_RegisterKinds rk = (virtualRegister == NULL) ? TR_GPR : virtualRegister->getKind();
   int numCandidates = 0;
   int first, last;
   int32_t dataSize = 0;
   bool containsCollectedReference;
   TR::InstOpCode::Mnemonic loadOp;

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
            first = TR::RealRegister::FirstFPR;
            last = TR::RealRegister::LastFPR;
            break;
         default:
            TR_ASSERT(false, "Unsupported RegisterKind.");
            break;
         }

      for (int i = first; i <= last; i++)
         {
         TR::RealRegister *realReg = self()->getRealRegister((TR::RealRegister::RegNum)i);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            candidates[numCandidates++] = realReg->getAssignedRegister();
            }
         }

      cursor = currentInstruction;
      while (numCandidates > 1 &&
             cursor != NULL &&
             cursor->getOpCodeValue() != TR::InstOpCode::label &&
             cursor->getOpCodeValue() != TR::InstOpCode::proc)
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
         dataSize = TR::Compiler->om.sizeofReferenceAddress();
         containsCollectedReference = candidates[0]->containsCollectedReference();
         break;
      case TR_FPR:
         dataSize = 8;
         containsCollectedReference = false;
         break;
      default:
         TR_ASSERT(false, "Unsupported RegisterKind.");
         break;
      }

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
         // the list. Therefore we need to set the occupied flag for this reuse.
         location->setIsOccupied();
         }
      else
         {
         location = self()->cg()->allocateSpill(dataSize, containsCollectedReference, NULL);
         }
      }
   else
      {
      location = self()->cg()->allocateSpill(dataSize, containsCollectedReference, NULL);
      }

   candidates[0]->setBackingStorage(location);

   tmemref = new (self()->cg()->trHeapMemory()) TR::MemoryReference(currentNode, location->getSymbolReference(), 8, self()->cg());

   if (!comp->getOption(TR_DisableOOL))
      {
      if (!self()->cg()->isOutOfLineColdPath())
         {
         TR_Debug *debugObj = self()->cg()->getDebug();
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL cold path, post dependencies will be generated using this list
         self()->cg()->getSpilledRegisterList()->push_front(candidates[0]);

         // OOL cold path: depth = 3, hot path: depth = 2,  main line: depth = 1
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
               {
               location->setMaxSpillDepth(2);
               }
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
         loadOp = TR::InstOpCode::_ld;
         break;
      case TR_FPR:
         loadOp = TR::InstOpCode::_fld;
         break;
      default:
         TR_ASSERT(false, "Unsupported RegisterKind.");
         break;
      }
   generateLOAD(loadOp, currentNode, best, tmemref, self()->cg(), currentInstruction);

   self()->cg()->traceRegFreed(candidates[0], best);

   best->setAssignedRegister(NULL);
   best->setState(TR::RealRegister::Free);
   candidates[0]->setAssignedRegister(NULL);
   return best;
   }

TR::RealRegister *OMR::RV::Machine::reverseSpillState(TR::Instruction *currentInstruction,
                                                     TR::Register *spilledRegister,
                                                     TR::RealRegister *targetRegister)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR::MemoryReference *tmemref;
   TR::RealRegister *sameReg;
   TR_BackingStore *location = spilledRegister->getBackingStorage();
   TR::Node *currentNode = currentInstruction->getNode();
   TR_RegisterKinds rk = spilledRegister->getKind();
   TR_Debug *debugObj = self()->cg()->getDebug();
   int32_t dataSize = 0;
   TR::InstOpCode::Mnemonic storeOp;

   if (targetRegister == NULL)
      {
      targetRegister = self()->findBestFreeRegister(rk);
      if (targetRegister == NULL)
         {
         targetRegister = self()->freeBestRegister(currentInstruction, spilledRegister, NULL);
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

   if (comp->getOption(TR_TraceCG))
      {
      diagnostic("\n\tre-assigning spilled %s to %s",
                  spilledRegister->getRegisterName(comp),
                  targetRegister->getRegisterName(comp));
      }

   tmemref = new (self()->cg()->trHeapMemory()) TR::MemoryReference(currentNode, location->getSymbolReference(), 8, self()->cg());

   if (comp->getOption(TR_DisableOOL))
      {
      switch (rk)
         {
         case TR_GPR:
            dataSize = TR::Compiler->om.sizeofReferenceAddress();
            storeOp = TR::InstOpCode::_sd;
            break;
         case TR_FPR:
            dataSize = 8;
            storeOp = TR::InstOpCode::_fsd;
            break;
         default:
            TR_ASSERT(false, "Unsupported RegisterKind.");
            break;
         }
         self()->cg()->freeSpill(location, dataSize, 0);
         generateSTORE(storeOp, currentNode, tmemref, targetRegister, self()->cg(), currentInstruction);
      }
   else
      {
      switch (rk)
         {
         case TR_GPR:
            dataSize = TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR_FPR:
            dataSize = 8;
            break;
         default:
            TR_ASSERT(false, "Unsupported RegisterKind.");
            break;
         }
      if (self()->cg()->isOutOfLineColdPath())
         {
         bool isOOLentryReverseSpill = false;
         if (currentInstruction->isLabel())
            {
            if (((TR::LabelInstruction*)currentInstruction)->getLabelSymbol()->isStartOfColdInstructionStream())
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
            storeOp = TR::InstOpCode::_sd;
            break;
         case TR_FPR:
            storeOp = TR::InstOpCode::_fsd;
            break;
         default:
            TR_ASSERT(false, "Unsupported RegisterKind.");
            break;
         }
         generateSTORE(storeOp, currentNode, tmemref, targetRegister, self()->cg(), currentInstruction);
      }
   return targetRegister;
   }

TR::RealRegister *OMR::RV::Machine::assignOneRegister(TR::Instruction *currentInstruction,
                                                         TR::Register *virtualRegister)
   {
   TR_RegisterKinds rk = virtualRegister->getKind();
   TR::RealRegister *assignedRegister = virtualRegister->getAssignedRealRegister();

   if (assignedRegister == NULL)
      {
      self()->cg()->clearRegisterAssignmentFlags();
      self()->cg()->setRegisterAssignmentFlag(TR_NormalAssignment);

      if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
         {
         self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
         assignedRegister = self()->reverseSpillState(currentInstruction, virtualRegister, NULL);
         }
      else
         {
         assignedRegister = self()->findBestFreeRegister(rk, true);
         if (assignedRegister == NULL)
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
            assignedRegister = self()->freeBestRegister(currentInstruction, virtualRegister, NULL);
            }
         }

      virtualRegister->setAssignedRegister(assignedRegister);
      assignedRegister->setAssignedRegister(virtualRegister);
      assignedRegister->setState(TR::RealRegister::Assigned);
      self()->cg()->traceRegAssigned(virtualRegister, assignedRegister);
      }

   if (virtualRegister->decFutureUseCount() == 0)
      {
      virtualRegister->setAssignedRegister(NULL);
      assignedRegister->setState(TR::RealRegister::Unlatched);
      }

   return assignedRegister;
   }

/* generate instruction for register copy */
static void registerCopy(TR::Instruction *precedingInstruction,
                         TR_RegisterKinds rk,
                         TR::RealRegister *targetReg,
                         TR::RealRegister *sourceReg,
                         TR::CodeGenerator *cg)
   {
   TR::Node *node = precedingInstruction->getNode();
   switch (rk)
      {
      case TR_GPR:
         generateITYPE(TR::InstOpCode::_addi, node, targetReg, sourceReg, 0, cg, precedingInstruction);
         break;
      case TR_FPR:
         TR_ASSERT(false, "Unsupported RegisterKind.");
         break;
      default:
         TR_ASSERT(false, "Unsupported RegisterKind.");
         break;
      }
   }

/* generate instructions for register exchange */
static void registerExchange(TR::Instruction *precedingInstruction,
                             TR_RegisterKinds rk,
                             TR::RealRegister *targetReg,
                             TR::RealRegister *sourceReg,
                             TR::RealRegister *middleReg,
                             TR::CodeGenerator *cg)
   {
   // middleReg is not used if rk==TR_GPR.

   TR::Node *node = precedingInstruction->getNode();
   if (rk == TR_GPR)
      {
      generateRTYPE(TR::InstOpCode::_xor, node, targetReg, targetReg, sourceReg, cg, precedingInstruction);
      generateRTYPE(TR::InstOpCode::_xor, node, sourceReg, targetReg, sourceReg, cg, precedingInstruction);
      generateRTYPE(TR::InstOpCode::_xor, node, targetReg, targetReg, sourceReg, cg, precedingInstruction);
      }
   else
      {
      registerCopy(precedingInstruction, rk, targetReg, middleReg, cg);
      registerCopy(precedingInstruction, rk, sourceReg, targetReg, cg);
      registerCopy(precedingInstruction, rk, middleReg, sourceReg, cg);
      }
   }

void OMR::RV::Machine::coerceRegisterAssignment(TR::Instruction *currentInstruction,
                                                   TR::Register *virtualRegister,
                                                   TR::RealRegister::RegNum registerNumber)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR::RealRegister *targetRegister = _registerFile[registerNumber];
   TR::RealRegister *realReg = virtualRegister->getAssignedRealRegister();
   TR::RealRegister *currentAssignedRegister = realReg ? toRealRegister(realReg) : NULL;
   TR_RegisterKinds rk = virtualRegister->getKind();
   TR::RealRegister *spareReg;
   TR::Register *currentTargetVirtual;

   if (comp->getOption(TR_TraceCG))
      {
      if (currentAssignedRegister)
         diagnostic("\n\tcoercing %s from %s to %s",
                     virtualRegister->getRegisterName(comp),
                     currentAssignedRegister->getRegisterName(comp),
                     targetRegister->getRegisterName(comp));
      else
         diagnostic("\n\tcoercing %s to %s",
                     virtualRegister->getRegisterName(comp),
                     targetRegister->getRegisterName(comp));
      }

   if (currentAssignedRegister == targetRegister)
       return;
   if (  targetRegister->getState() == TR::RealRegister::Free
      || targetRegister->getState() == TR::RealRegister::Unlatched)
      {
#ifdef DEBUG
      if (comp->getOption(TR_TraceCG))
         diagnostic(", which is free");
#endif
      if (currentAssignedRegister == NULL)
         {
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
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
      if (comp->getOption(TR_TraceCG))
         diagnostic(", which is blocked and assigned to %s",
                     currentTargetVirtual->getRegisterName(comp));
#endif
      if (!currentAssignedRegister || rk != TR_GPR)
         {
         spareReg = self()->findBestFreeRegister(rk);
         if (spareReg == NULL)
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
            virtualRegister->block();
            spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual);
            virtualRegister->unblock();
            }
         }

      if (currentAssignedRegister)
         {
         self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
         registerExchange(currentInstruction, rk, targetRegister, currentAssignedRegister, spareReg, self()->cg());
         currentAssignedRegister->setState(TR::RealRegister::Blocked);
         currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
         currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
         // For Non-GPR, spareReg remains FREE.
         }
      else
         {
         self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);
         registerCopy(currentInstruction, rk, targetRegister, spareReg, self()->cg());
         spareReg->setState(TR::RealRegister::Blocked);
         currentTargetVirtual->setAssignedRegister(spareReg);
         spareReg->setAssignedRegister(currentTargetVirtual);
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         // spareReg is assigned.
         }
      }
   else if (targetRegister->getState() == TR::RealRegister::Assigned)
      {
      currentTargetVirtual = targetRegister->getAssignedRegister();
#ifdef DEBUG
      if (comp->getOption(TR_TraceCG))
         diagnostic(", which is assigned to %s",
                     currentTargetVirtual->getRegisterName(comp));
#endif
      spareReg = self()->findBestFreeRegister(rk);

      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
      if (currentAssignedRegister != NULL)
         {
         if ((rk != TR_GPR) && (spareReg == NULL))
            {
            self()->freeBestRegister(currentInstruction, currentTargetVirtual, targetRegister);
            }
         else
            {
            self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
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
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
            self()->freeBestRegister(currentInstruction, currentTargetVirtual, targetRegister);
            }
         else
            {
            self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);
            registerCopy(currentInstruction, rk, targetRegister, spareReg, self()->cg());
            spareReg->setState(TR::RealRegister::Assigned);
            spareReg->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(spareReg);
            // spareReg is assigned.
            }
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }
      self()->cg()->resetRegisterAssignmentFlag(TR_IndirectCoercion);
      }
   else
      {
#ifdef DEBUG
      if (comp->getOption(TR_TraceCG))
         diagnostic(", which is in an unknown state %d", targetRegister->getState());
#endif
      }

   targetRegister->setState(TR::RealRegister::Assigned);
   targetRegister->setAssignedRegister(virtualRegister);
   virtualRegister->setAssignedRegister(targetRegister);
   self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
   }

void OMR::RV::Machine::initializeRegisterFile()
   {
   _registerFile[TR::RealRegister::NoReg] = NULL;
   _registerFile[TR::RealRegister::SpilledReg] = NULL;

#define DECLARE_REG(regname, type)                                             \
   _registerFile[TR::RealRegister:: regname] =                                 \
         new (self()->cg()->trHeapMemory()) TR::RealRegister(type,             \
                                                 0,                            \
                                                 TR::RealRegister::Free,       \
                                                 TR::RealRegister:: regname,   \
                                                 self()->cg());
#define DECLARE_GPR(regname, abiname, encoding) DECLARE_REG(regname, TR_GPR)
#define DECLARE_FPR(regname, abiname, encoding) DECLARE_REG(regname, TR_FPR)
#include <codegen/riscv-regs.h>
#undef DECLARE_GPR
#undef DECLARE_FPR
#undef DECLARE_REG
   }

void
OMR::RV::Machine::takeRegisterStateSnapShot()
   {
   int32_t i;
   for (i = TR::RealRegister::FirstGPR; i < TR::RealRegister::NumRegisters - 1; i++)
      {
      _registerStatesSnapShot[i] = _registerFile[i]->getState();
      _assignedRegisterSnapShot[i] = _registerFile[i]->getAssignedRegister();
      _registerFlagsSnapShot[i] = _registerFile[i]->getFlags();
      }
   }

void
OMR::RV::Machine::restoreRegisterStateFromSnapShot()
   {
   int32_t i;
   for (i = TR::RealRegister::FirstGPR; i < TR::RealRegister::NumRegisters - 1; i++) // Skipping SpilledReg
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
      // Don't restore registers that died after the snapshot was taken since they are guaranteed to not be used in the outlined path
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned &&
          _registerFile[i]->getAssignedRegister()->getFutureUseCount() == 0)
         {
         _registerFile[i]->setState(TR::RealRegister::Free);
         _registerFile[i]->getAssignedRegister()->setAssignedRegister(NULL);
         _registerFile[i]->setAssignedRegister(NULL);
         }
      }
   }

uint32_t OMR::RV::Machine::_globalRegisterNumberToRealRegisterMap[] =
   {
   // GPRs
   TR::RealRegister::x15,
   TR::RealRegister::x14,
   TR::RealRegister::x13,
   TR::RealRegister::x12,
   TR::RealRegister::x11,
   TR::RealRegister::x10,
   TR::RealRegister::x9,
   TR::RealRegister::x8, // indirect result location register
   TR::RealRegister::x18, // platform register
   // callee-saved registers
   TR::RealRegister::x28,
   TR::RealRegister::x27,
   TR::RealRegister::x26,
   TR::RealRegister::x25,
   TR::RealRegister::x24,
   TR::RealRegister::x23,
   TR::RealRegister::x22,
   TR::RealRegister::x21,
   TR::RealRegister::x20,
   TR::RealRegister::x19,
   // parameter registers
   TR::RealRegister::x7,
   TR::RealRegister::x6,
   TR::RealRegister::x5,
   TR::RealRegister::x4,
   TR::RealRegister::x3,
   TR::RealRegister::x2,
   TR::RealRegister::x1,
   TR::RealRegister::x0,

   // FPRs
   TR::RealRegister::f31,
   TR::RealRegister::f30,
   TR::RealRegister::f29,
   TR::RealRegister::f28,
   TR::RealRegister::f27,
   TR::RealRegister::f26,
   TR::RealRegister::f25,
   TR::RealRegister::f24,
   TR::RealRegister::f23,
   TR::RealRegister::f22,
   TR::RealRegister::f21,
   TR::RealRegister::f20,
   TR::RealRegister::f19,
   TR::RealRegister::f18,
   TR::RealRegister::f17,
   TR::RealRegister::f16,
   // callee-saved registers
   TR::RealRegister::f15,
   TR::RealRegister::f14,
   TR::RealRegister::f13,
   TR::RealRegister::f12,
   TR::RealRegister::f11,
   TR::RealRegister::f10,
   TR::RealRegister::f9,
   TR::RealRegister::f8,
   // parameter registers
   TR::RealRegister::f7,
   TR::RealRegister::f6,
   TR::RealRegister::f5,
   TR::RealRegister::f4,
   TR::RealRegister::f3,
   TR::RealRegister::f2,
   TR::RealRegister::f1,
   TR::RealRegister::f0
   };
