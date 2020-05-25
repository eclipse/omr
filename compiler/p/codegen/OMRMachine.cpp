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

#include "p/codegen/OMRMachine.hpp"

#include <stdint.h>
#include <string.h>
#include <algorithm>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Machine_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCCRBackingStore.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "ras/Debug.hpp"
#include "env/IO.hpp"

namespace TR { class AutomaticSymbol; }

static void registerExchange(TR::Instruction *precedingI,
                                            TR_RegisterKinds    rk,
                                            TR::RealRegister *tReg,
                                            TR::RealRegister *sReg,
                                            TR::RealRegister *mReg,
                                            TR::CodeGenerator   *cg);
static void registerCopy(TR::Instruction *precedingI,
                                        TR_RegisterKinds    rk,
                                        TR::RealRegister *tReg,
                                        TR::RealRegister *sReg,
                                        TR::CodeGenerator   *cg);

static int32_t spillSizeForRegister(TR::Register *virtReg)
   {
   switch (virtReg->getKind())
      {
      case TR_GPR:
         return TR::Compiler->om.sizeofReferenceAddress();
      case TR_FPR:
         return virtReg->isSinglePrecision() ? 4 : 8;
      case TR_VSX_SCALAR:
         return 8;
      case TR_CCR:
         return 4;
      case TR_VSX_VECTOR:
      case TR_VRF:
         return 16;
      }
   TR_ASSERT(false, "Unexpected register kind");
   return 0;
   }

static const char *
getRegisterName(TR::Register * reg, TR::CodeGenerator * cg)
   {
   return (reg? cg->getDebug()->getName(reg) : "NULL");
   }

static bool boundNext(TR::Instruction *currentInstruction, int32_t realNum, TR::Register *virtReg, bool isOOL)
   {
   TR::Instruction *cursor = currentInstruction;
   TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)realNum;
   TR::Node  *nodeBBStart = NULL;

   while (!(isOOL && cursor == NULL) && cursor->getOpCodeValue() != TR::InstOpCode::proc)
      {
      TR::RegisterDependencyConditions *conditions;
      if ((conditions = cursor->getDependencyConditions()) != NULL)
         {
         TR::Register *boundReg = conditions->searchPostConditionRegister(realReg);
         if (boundReg == NULL)
            boundReg = conditions->searchPreConditionRegister(realReg);
         if (boundReg != NULL)
            {
            if (boundReg == virtReg)
               return true;
            return false;
            }
         }

      TR::Node *node = cursor->getNode();
      if (nodeBBStart!=NULL && node!=nodeBBStart)
         return true;
      if (node!=NULL && node->getOpCodeValue()==TR::BBStart)
         {
         TR::Block *block = node->getBlock();
         if (!block->isExtensionOfPreviousBlock())
            nodeBBStart = node;
         }

      cursor = cursor->getPrev();
      }
   return true;
   }

OMR::Power::Machine::Machine(TR::CodeGenerator *cg) :
      OMR::Machine(cg)
   {
   self()->initializeRegisterFile();
   memset( _registerAssociations, 0, sizeof(TR::Register*)*TR::RealRegister::NumRegisters );
   }


void OMR::Power::Machine::initREGAssociations()
   {
   // Track the last 4 real GPRs assigned so that we try not to reuse them.
   _4thLastGPRAssigned = -1;
   _3rdLastGPRAssigned = -1;
   _2ndLastGPRAssigned = -1;
   _lastGPRAssigned = -1;

   int icount;
   int nextV = 0;
   int lastFPRv, lastVRFv;
   const TR::PPCLinkageProperties &linkage = self()->cg()->getProperties();

   // We assume the volatile/preserved registers to be consecutive
   // because the private linkage will either be a single volatile set or
   // end up resembling the system linkage. In both cases all volatiles are together,
   // and so are the preserved registers

   // pick the volatiles from traditional BFPs
   for (icount=TR::RealRegister::FirstFPR; icount<=TR::RealRegister::LastFPR; icount++)
      {
      if (!linkage.getPreserved((TR::RealRegister::RegNum)icount))
          _registerAllocationFPR[nextV++] = icount;
      }
   lastFPRv = nextV - 1;

   // Then, pick the preserves from traditional BFPs
   for (icount=TR::RealRegister::LastFPR; icount>=TR::RealRegister::FirstFPR; icount--)
      {
      if (linkage.getPreserved((TR::RealRegister::RegNum)icount))
          _registerAllocationFPR[nextV++] = icount;
      }
   _lastPreservedFPRAvail = nextV - 1;

   // pick all the volatiles from VRF
   for (icount=TR::RealRegister::LastFPR+1; icount<=TR::RealRegister::LastVSR; icount++)
      {
      if (!linkage.getPreserved((TR::RealRegister::RegNum)icount))
          _registerAllocationFPR[nextV++] = icount;
      }
   lastVRFv = nextV - 1;

   // Now VRF the preserves
   for (icount=TR::RealRegister::LastVSR; icount>=TR::RealRegister::LastFPR+1; icount--)
      {
      if (linkage.getPreserved((TR::RealRegister::RegNum)icount))
          _registerAllocationFPR[nextV++] = icount;
      }
   _lastPreservedVRFAvail = nextV - 1;


   // Power6:      roll over the biggest set of registers we can afford, because no renaming
   // Power8/SAR:  confine to the smallest set of registers we can get away, because map cache
   // Others:      neutral --- take Power6 way for now

   int rollingAllocator = !(self()->cg()->comp()->target().cpu.id() == TR_PPCp8);


   _inUseFPREnd = rollingAllocator?lastFPRv:0;
   _lastFPRAlloc = _inUseFPREnd;
   _inUseVFREnd = rollingAllocator?lastVRFv:(_lastPreservedFPRAvail + 1);
   _lastVRFAlloc = _inUseVFREnd;
}

TR::RealRegister *OMR::Power::Machine::findBestFreeRegister(TR::Instruction *currentInstruction,
                                                        TR_RegisterKinds  rk,
                                                        bool excludeGPR0,
                                                        bool considerUnlatched,
                                                        TR::Register      *virtualReg)
   {
   uint32_t         preference = (virtualReg==NULL)?0:virtualReg->getAssociation();
   uint64_t         interference = 0;
   int first, maskI;
   int last;
   bool             liveRegOn = (self()->cg()->getLiveRegisters(rk) != NULL);

   if (liveRegOn && virtualReg != NULL)
      interference = virtualReg->getInterference();

   // After liveReg is used by default, the following code should go away
   bool avoidGPR0 = (virtualReg!=NULL ?
                       virtualReg->containsCollectedReference() || virtualReg->containsInternalPointer() :
                       false );
   if (!liveRegOn && avoidGPR0)
      interference |= 1;

   switch(rk)
      {
      case TR_GPR:
         maskI = first = TR::RealRegister::FirstGPR;
         if (excludeGPR0)
            first++;
         last  = TR::RealRegister::LastGPR;
         break;
      case TR_FPR:
      case TR_VSX_SCALAR:
      case TR_VSX_VECTOR:
         maskI = TR::RealRegister::FirstFPR;
         break;
      case TR_CCR:
         maskI = first = TR::RealRegister::FirstCCR;
         last  = TR::RealRegister::LastCCR;
         break;
      case TR_VRF:
         maskI = TR::RealRegister::FirstVRF;
         break;
   }

   if (liveRegOn && preference!=0 && (interference & (1<<(preference-maskI))))
      {
      if (!boundNext(currentInstruction, preference, virtualReg, (self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath())))
         preference = 0;
      }

   if (preference!=0 && (!excludeGPR0 || preference!=TR::RealRegister::gr0) &&
       (_registerFile[preference]->getState() == TR::RealRegister::Free ||
        (considerUnlatched &&_registerFile[preference]->getState() == TR::RealRegister::Unlatched)))
      {
      if (_registerFile[preference]->getState() == TR::RealRegister::Unlatched)
         {
         _registerFile[preference]->setAssignedRegister(NULL);
         _registerFile[preference]->setState(TR::RealRegister::Free);
         }
      return _registerFile[preference];
      }

   uint32_t            bestWeightSoFar = 0xffffffff;
   TR::RealRegister *freeRegister    = NULL;
   uint64_t            iOld=0, iNew;

   // For FPR/VSR/VRF
   if (rk == TR_FPR || rk == TR_VSX_SCALAR || rk == TR_VSX_VECTOR || rk == TR_VRF)
      {
      int rollingAllocator = !(self()->cg()->comp()->target().cpu.id() == TR_PPCp8);

      // Find the best in the used FPR set so far
      int i, idx;
      if (rk != TR_VRF)
         {
         i = _lastFPRAlloc;
         do {
            if (++i > _inUseFPREnd) i = 0;
            iNew = interference & (1<<(_registerAllocationFPR[i]-maskI));
            if ( ( _registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Free ||
                 (considerUnlatched && _registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Unlatched))
                 &&
                 ( freeRegister == NULL ||
                   (iOld && !iNew)      ||
                   ((iOld || !iNew) && !rollingAllocator &&        // Rolling: weight is not considered once used
		    _registerFile[_registerAllocationFPR[i]]->getWeight()<bestWeightSoFar)
		 )
               )
               {
               iOld = iNew;
	       idx = i;
               freeRegister    = _registerFile[_registerAllocationFPR[i]];
	       bestWeightSoFar = freeRegister->getWeight();
               }
	    } while (i != _lastFPRAlloc);

	 if (freeRegister != NULL)
	    _lastFPRAlloc = idx;
         }

      // Find the best in the used VRF set so far
      if (freeRegister==NULL && rk!=TR_FPR)
         {
         i = _lastVRFAlloc;
         do {
	    if (++i > _inUseVFREnd) i = _lastPreservedFPRAvail+1;   // first VRF
            iNew = interference & (1<<(_registerAllocationFPR[i]-maskI));
            if ( ( _registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Free ||
                 (considerUnlatched && _registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Unlatched))
                 &&
                 ( freeRegister == NULL ||
                   (iOld && !iNew)      ||
                   ((iOld || !iNew) && !rollingAllocator &&        // Rolling: weight is not considered once used
		    _registerFile[_registerAllocationFPR[i]]->getWeight()<bestWeightSoFar)
		 )
               )
               {
               iOld = iNew;
	       idx = i;
               freeRegister    = _registerFile[_registerAllocationFPR[i]];
	       bestWeightSoFar = freeRegister->getWeight();
               }
	    } while (i != _lastVRFAlloc);

	 if (freeRegister != NULL)
	    _lastVRFAlloc = idx;
         }

      // If no register was assigned, try the not-used-so-far register set.
      // Remember they are already in increasing weight in _registerAllocationFPR

      if (freeRegister==NULL && rk!=TR_VRF)
         {
         for (i = _inUseFPREnd+1; i<=_lastPreservedFPRAvail; i++)
            {
            iNew = interference & (1<<(_registerAllocationFPR[i]-maskI));
            if ( (_registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Free ||
                  (considerUnlatched && _registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Unlatched))
                 &&
		 ( freeRegister == NULL || !iNew )
               )
               {
	       freeRegister    = _registerFile[_registerAllocationFPR[i]];
	       idx = i;
               if (!iNew)
		  break;
               }
            }

         if (freeRegister != NULL)   // We have assigned an unused reg, add it to the used set
	    _inUseFPREnd = _lastFPRAlloc = idx;
         }

      if (freeRegister==NULL && rk!=TR_FPR)
         {
         for (i = _inUseVFREnd+1; i<=_lastPreservedVRFAvail; i++)
            {
            iNew = interference & (1<<(_registerAllocationFPR[i]-maskI));
            if ( (_registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Free ||
                  (considerUnlatched && _registerFile[_registerAllocationFPR[i]]->getState() == TR::RealRegister::Unlatched))
                 &&
		 ( freeRegister == NULL || !iNew )
               )
               {
	       freeRegister    = _registerFile[_registerAllocationFPR[i]];
	       idx = i;
               if (!iNew)
		  break;
               }
            }

         if (freeRegister != NULL)   // We have assigned an unused reg, add it to the used set
	    _inUseVFREnd = _lastVRFAlloc = idx;
         }
      }
   else
      {
      int iChosen;

      for (int i = first; i <= last; i++)
         {
         int currentReg = 1<<(i-maskI);
         iNew = interference & currentReg;

         //Inject interference for last four assignments to prevent write-after-write dependancy in same p6 dispatch group.
         if(rk == TR_GPR && (self()->cg()->comp()->target().cpu.id() == TR_PPCp6))
            {
            if (_lastGPRAssigned != -1)
               iNew |= currentReg & _lastGPRAssigned;
            if (_2ndLastGPRAssigned != -1)
               iNew |= currentReg & _2ndLastGPRAssigned;
            if (_3rdLastGPRAssigned != -1)
               iNew |= currentReg & _3rdLastGPRAssigned;
            if (_4thLastGPRAssigned != -1)
               iNew |= currentReg & _4thLastGPRAssigned;
            }

         if (     ( _registerFile[i]->getState() == TR::RealRegister::Free ||
                   (considerUnlatched && _registerFile[i]->getState() == TR::RealRegister::Unlatched))
               && ( freeRegister == NULL ||
                    (iOld && !iNew) ||
                    ((iOld || !iNew) && _registerFile[i]->getWeight()<bestWeightSoFar))
            )
            {
            iOld = iNew;
            freeRegister    = _registerFile[i];
            iChosen = i;
            bestWeightSoFar = freeRegister->getWeight();
            }
         }
      //Track the last four registers used for use in above interference injection.
      if ((rk == TR_GPR) && (freeRegister != NULL) && (self()->cg()->comp()->target().cpu.id() == TR_PPCp6))
         {
         _4thLastGPRAssigned = _3rdLastGPRAssigned;
         _3rdLastGPRAssigned = _2ndLastGPRAssigned;
         _2ndLastGPRAssigned = _lastGPRAssigned;
         _lastGPRAssigned = 1<<(iChosen-maskI);
         }
      }

   if (freeRegister!=NULL && freeRegister->getState()==TR::RealRegister::Unlatched)
      {
      freeRegister->setAssignedRegister(NULL);
      freeRegister->setState(TR::RealRegister::Free);
      }
   return freeRegister;
   }

TR::RealRegister *OMR::Power::Machine::freeBestRegister(TR::Instruction     *currentInstruction,
                                                    TR::Register        *virtReg,
                                                    TR::RealRegister *forced,
                                                    bool                excludeGPR0)

   {
   TR::Register           *candidates[NUM_PPC_MAXR];
   TR::MemoryReference *tmemref;
   TR_BackingStore       *location;
   TR::RealRegister    *best, *crtemp=NULL;
   TR::Instruction     *cursor;
   TR::InstOpCode::Mnemonic          opCode;
   TR::Machine *machine = self()->cg()->machine();
   TR::Node               *currentNode = currentInstruction->getNode();
   TR_RegisterKinds       rk = (virtReg==NULL)?TR_GPR:virtReg->getKind();
   int32_t                numCandidates=0, first, last, maskI;
   uint64_t               interference=0;
   TR::RealRegister::RegState crtemp_state;
   TR::Compilation *comp = self()->cg()->comp();

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
            maskI = first = TR::RealRegister::FirstGPR;
            if (excludeGPR0)
               first++;
            last = TR::RealRegister::LastGPR;
            break;
         case TR_FPR:
            maskI = first = TR::RealRegister::FirstFPR;
            last = TR::RealRegister::LastFPR;
            break;
         case TR_VSX_SCALAR:
            maskI = first = TR::RealRegister::FirstFPR;
            last = TR::RealRegister::LastVSR;
            break;
         case TR_VSX_VECTOR:
            maskI = first = TR::RealRegister::FirstFPR;
            last = TR::RealRegister::LastVSR;
            break;
         case TR_CCR:
            maskI = first = TR::RealRegister::FirstCCR;
            last = TR::RealRegister::LastCCR;
            break;
         case TR_VRF:
            maskI = first = TR::RealRegister::FirstVRF;
            last = TR::RealRegister::LastVRF;
            break;
         }

      int32_t  preference = 0, pref_favored = 0;
      if (self()->cg()->getLiveRegisters(rk) != NULL && virtReg != NULL)
         {
         interference = virtReg->getInterference();
         preference = virtReg->getAssociation();

         // Consider yielding
         if (preference != 0 && boundNext(currentInstruction, preference, virtReg, (self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath())))
            {
            pref_favored = 1;
            interference &= ~((uint64_t)(1<<(preference - maskI)));
            }
         }

      uint64_t allInterfere;
      for (int i  = first; i <= last; i++)
         {
         uint64_t iInterfere = interference & (1<<(i-maskI));
         TR::RealRegister *realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            TR::Register *associatedVirtual = realReg->getAssignedRegister();
            if (numCandidates == 0)
               {
               candidates[numCandidates++] = associatedVirtual;
               allInterfere = iInterfere;
               }
            else
               {
               if (iInterfere)
                  {
                  if (allInterfere)
                     candidates[numCandidates++] = associatedVirtual;
                  }
               else
                  {
                  if (allInterfere)
                     {
                     numCandidates = allInterfere = 0;
                     candidates[0] = associatedVirtual;
                     }
                  else
                     {
                     if (i==preference && pref_favored)
                        {
                        TR::Register *tempReg = candidates[0];
                        candidates[0] = associatedVirtual;
                        associatedVirtual = tempReg;
                        }
                     candidates[numCandidates] = associatedVirtual;
                     }
                  numCandidates++;
                  }
               }
            }
         }

      TR_ASSERT(numCandidates !=0, "All %s registers are blocked\n", virtReg->getRegisterKindName(comp, rk));

      cursor = currentInstruction;
      while (numCandidates > 1                 &&
             cursor != NULL                    &&
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
      // XXX: Reusing backing stores here is causing the virtual being spilled to be clobbered
      case TR_GPR:
         if (false /*!cg()->comp()->getOption(TR_DisableOOL) && (cg()->isOutOfLineColdPath() || cg()->isOutOfLineHotPath()) &&
                virtReg->getBackingStorage()*/)
            {
            location = virtReg->getBackingStorage();
            // reuse the spill slot
            if (self()->cg()->getDebug())
            self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                  location, self()->cg()->getDebug()->getName(virtReg));
            }
         else
            {
            if (candidates[0]->getBackingStorage())
               {
               // If there is backing storage associated with a register, it means the
               // backing store wasn't returned to the free list and it can be used.
               //
               location = candidates[0]->getBackingStorage();

               // If best register already has a backing store it's because we reverse spilled it in an
               // OOL region while the free spill list was locked and we didn't clean this up after unlocking
               // the list (see TODO in PPCInstruction.cpp). Therefore we need to set the
               // occupied flag for this reuse.
               if (location->getSymbolReference()->getSymbol()->getSize() > TR::Compiler->om.sizeofReferenceAddress())
                  location->setFirstHalfIsOccupied();
               else
                  location->setIsOccupied();
               }
            else
               {
               if (candidates[0]->containsInternalPointer())
                  {
                  TR::AutomaticSymbol *parray = candidates[0]->getPinningArrayPointer();
                  location = self()->cg()->allocateInternalPointerSpill(parray);
                  }
               else
                  {
                  location = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), candidates[0]->containsCollectedReference(), NULL);
                  }
               }
            }
         break;
      case TR_FPR:
         if (false /*!cg()->comp()->getOption(TR_DisableOOL) && (cg()->isOutOfLineColdPath() || cg()->isOutOfLineHotPath()) &&
                virtReg->getBackingStorage()*/)
            {
            location = virtReg->getBackingStorage();
            // reuse the spill slot
            if (self()->cg()->getDebug())
                self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                  location, self()->cg()->getDebug()->getName(virtReg));
            }
         else
            {
            if (candidates[0]->getBackingStorage())
               {
               // If there is backing storage associated with a register, it means the
               // backing store wasn't returned to the free list and it can be used.
               //
               location = candidates[0]->getBackingStorage();

               // If best register already has a backing store it's because we reverse spilled it in an
               // OOL region while the free spill list was locked and we didn't clean this up after unlocking
               // the list (see TODO in PPCInstruction.cpp). Therefore we need to set the
               // occupied flag for this reuse.
               if (candidates[0]->isSinglePrecision() && location->getSymbolReference()->getSymbol()->getSize() > 4)
                  location->setFirstHalfIsOccupied();
               else
                  location->setIsOccupied();
               }
            else
               {
               location = self()->cg()->allocateSpill(candidates[0]->isSinglePrecision()? 4:8, false, NULL);
               }
            }
         break;
      case TR_VSX_SCALAR:
         if (candidates[0]->getBackingStorage())
            {
            // If there is backing storage associated with a register, it means the
            // backing store wasn't returned to the free list and it can be used.
            //
            location = candidates[0]->getBackingStorage();

            // If best register already has a backing store it's because we reverse spilled it in an
            // OOL region while the free spill list was locked and we didn't clean this up after unlocking
            // the list (see TODO in PPCInstruction.cpp). Therefore we need to set the
            // occupied flag for this reuse.
            if (candidates[0]->isSinglePrecision() && location->getSymbolReference()->getSymbol()->getSize() > 4)
               location->setFirstHalfIsOccupied();
            else
               location->setIsOccupied();
            }
         else
            {
            location = self()->cg()->allocateSpill(candidates[0]->isSinglePrecision()? 4:8, false, NULL);
            }
         break;
      case TR_CCR:
         if (false /*!cg()->comp()->getOption(TR_DisableOOL) && (cg()->isOutOfLineColdPath() || cg()->isOutOfLineHotPath()) &&
                virtReg->getBackingStorage()*/)
            {
            location = virtReg->getBackingStorage();
            // reuse the spill slot
            if (self()->cg()->getDebug())
                self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                  location, self()->cg()->getDebug()->getName(virtReg));
            }
         else
            {
            if (candidates[0]->getBackingStorage())
               {
               // If there is backing storage associated with a register, it means the
               // backing store wasn't returned to the free list and it can be used.
               //
               location = candidates[0]->getBackingStorage();

               // If best register already has a backing store it's because we reverse spilled it in an
               // OOL region while the free spill list was locked and we didn't clean this up after unlocking
               // the list (see TODO in PPCInstruction.cpp). Therefore we need to set the
               // occupied flag for this reuse.
               if (location->getSymbolReference()->getSymbol()->getSize() > 4)
                  location->setFirstHalfIsOccupied();
               else
                  location->setIsOccupied();
               }
            else
               {
               location = new (self()->cg()->trHeapMemory()) TR_PPCCRBackingStore(comp, self()->cg()->allocateSpill(4, false, NULL)); // TODO: Could this be 1 byte?
               }
            }
         break;
      case TR_VSX_VECTOR:
      case TR_VRF:
         if (candidates[0]->getBackingStorage())
            {
            // If there is backing storage associated with a register, it means the
            // backing store wasn't returned to the free list and it can be used.
            //
            location = candidates[0]->getBackingStorage();

            // If best register already has a backing store it's because we reverse spilled it in an
            // OOL region while the free spill list was locked and we didn't clean this up after unlocking
            // the list (see TODO in PPCInstruction.cpp). Therefore we need to set the
            // occupied flag for this reuse.
            location->setIsOccupied();
            }
         else
            {
            location = self()->cg()->allocateSpill(16, false, NULL);
            }
         break;
      }

   if (rk == TR_CCR)
      toPPCCRBackingStore(location)->setCcrFieldIndex(
         best->getRegisterNumber() - TR::RealRegister::FirstCCR);
   candidates[0]->setBackingStorage(location);

   tmemref = new (self()->cg()->trHeapMemory()) TR::MemoryReference(currentNode, location->getSymbolReference(), TR::Compiler->om.sizeofReferenceAddress(), self()->cg());

   if (rk == TR_CCR)
      {
      crtemp = self()->findBestFreeRegister(currentInstruction, TR_GPR);
      if (crtemp == NULL)
         crtemp = self()->freeBestRegister(currentInstruction, NULL);
      else
         crtemp->setHasBeenAssignedInMethod(true);
      crtemp_state = crtemp->getState();
      TR::Instruction *ccrInstr = generateSrc1Instruction(self()->cg(), TR::InstOpCode::mtocrf, currentNode, crtemp, 1<<(7-toPPCCRBackingStore(location)->getCcrFieldIndex()), currentInstruction);
      self()->cg()->traceRAInstruction(ccrInstr);
      }

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
            self()->cg()->traceRegisterAssignment("OOL: adding %s to the spilledRegisterList, maxSpillDepth = %d\n",
                                          debugObj->getName(candidates[0]), location->getMaxSpillDepth());
         }
      else
         {
         // do not overwrite mainline and hot path spill depth
         // if this spill is inside OOL cold path, we do not need to protecting the spill slot
         // because the post condition at OOL entry does not expect this register to be spilled
         if (location->getMaxSpillDepth() != 1 &&
             location->getMaxSpillDepth() != 2 )
            location->setMaxSpillDepth(3);
         }
      }

   TR::Instruction *reloadInstr = NULL;
   TR::RealRegister *tempIndexRegister = NULL;
   switch (rk)
      {
      case TR_GPR:
         reloadInstr = generateTrg1MemInstruction(self()->cg(),TR::InstOpCode::Op_load, currentNode, best, tmemref, currentInstruction);
         break;
      case TR_FPR:
         if (candidates[0]->isSinglePrecision())
            {
            opCode = TR::InstOpCode::lfs;
            tmemref->setLength(4);
            }
         else
            {
            opCode = TR::InstOpCode::lfd;
            tmemref->setLength(8);
            }
         reloadInstr = generateTrg1MemInstruction(self()->cg(), opCode, currentNode, best, tmemref, currentInstruction);
         break;
      case TR_VSX_SCALAR:
         tempIndexRegister = self()->findBestFreeRegister(currentInstruction, TR_GPR);
         if (tempIndexRegister  == NULL)
            tempIndexRegister = self()->freeBestRegister(currentInstruction, NULL);
         tmemref->setUsingDelayedIndexedForm();
         tmemref->setIndexRegister(tempIndexRegister);
         tmemref->setIndexModifiable();
         opCode = TR::InstOpCode::lxsdx;
         tmemref->setLength(8);
         reloadInstr = generateTrg1MemInstruction(self()->cg(), opCode, currentNode, best, tmemref, currentInstruction);
         self()->cg()->stopUsingRegister(tempIndexRegister);
         break;
      case TR_CCR:
         reloadInstr = generateTrg1MemInstruction(self()->cg(),TR::InstOpCode::Op_load, currentNode, crtemp, tmemref, currentInstruction);
         break;
      case TR_VSX_VECTOR:
         tempIndexRegister = self()->findBestFreeRegister(currentInstruction, TR_GPR);
         if (tempIndexRegister  == NULL)
            tempIndexRegister = self()->freeBestRegister(currentInstruction, NULL);
         tmemref->setUsingDelayedIndexedForm();
         tmemref->setIndexRegister(tempIndexRegister);
         tmemref->setIndexModifiable();
         opCode = TR::InstOpCode::lxvd2x;
         tmemref->setLength(16);
         reloadInstr = generateTrg1MemInstruction(self()->cg(), opCode, currentNode, best, tmemref, currentInstruction);
         self()->cg()->stopUsingRegister(tempIndexRegister);
         break;
      case TR_VRF:
         // Until stack frame is 16-byte aligned, we cannot use VMX load/store here
         // So, we use VSX load/store instead as a work-around

         TR_ASSERT(self()->cg()->comp()->target().cpu.getPPCSupportsVSX(), "VSX support not enabled");

         tempIndexRegister = self()->findBestFreeRegister(currentInstruction, TR_GPR);
         if (tempIndexRegister  == NULL)
            tempIndexRegister = self()->freeBestRegister(currentInstruction, NULL);
         tmemref->setUsingDelayedIndexedForm();
         tmemref->setIndexRegister(tempIndexRegister);
         tmemref->setIndexModifiable();
         opCode = TR::InstOpCode::lxvd2x;
         tmemref->setLength(16);
         reloadInstr = generateTrg1MemInstruction(self()->cg(), opCode, currentNode, best, tmemref, currentInstruction);
         self()->cg()->stopUsingRegister(tempIndexRegister);
         break;
      }

   self()->cg()->traceRegFreed(candidates[0], best);
   self()->cg()->traceRAInstruction(reloadInstr);

   best->setAssignedRegister(NULL);
   best->setState(TR::RealRegister::Free);
   candidates[0]->setAssignedRegister(NULL);
   return best;
   }

// Sets the register association of a real register to a virtual register
// Used to track the old real register association
TR::Register *OMR::Power::Machine::setVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum, TR::Register * virtReg)
   {
   if (virtReg)
      {
      // disable previous association
      if (virtReg->getAssociation())
         _registerAssociations[virtReg->getAssociation()] = 0;
      virtReg->setAssociation(regNum);
      }

   return (_registerAssociations[regNum] = virtReg);
   }

TR::Register *OMR::Power::Machine::getVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum)
   {
   return(_registerAssociations[regNum]);
   }

TR::RealRegister *OMR::Power::Machine::reverseSpillState(TR::Instruction      *currentInstruction,
                                                     TR::Register         *spilledRegister,
                                                     TR::RealRegister  *targetRegister,
                                                     bool                excludeGPR0)
   {
   TR::MemoryReference *tmemref;
   TR::Compilation *comp = self()->cg()->comp();
   TR::RealRegister    *sameReg, *crtemp   = NULL;
   TR_BackingStore       *location = spilledRegister->getBackingStorage();
   TR::Node               *currentNode = currentInstruction->getNode();
   TR_RegisterKinds       rk       = spilledRegister->getKind();
   TR::InstOpCode::Mnemonic          opCode;
   TR_Debug          *debugObj = self()->cg()->getDebug();
   if (targetRegister == NULL)
      {
      if ((rk == TR_CCR) && (!self()->cg()->isOutOfLineColdPath() && !self()->cg()->isOutOfLineHotPath()) && ((sameReg=self()->cg()->machine()->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::FirstCCR+toPPCCRBackingStore(location)->getCcrFieldIndex())))->getState() == TR::RealRegister::Free))
         {
         targetRegister = sameReg;
         }
      else
         {
         targetRegister = self()->findBestFreeRegister(currentInstruction, rk, excludeGPR0, false, spilledRegister);
         }
      if (targetRegister == NULL)
         {
         targetRegister = self()->freeBestRegister(currentInstruction,
                                           spilledRegister, NULL, excludeGPR0);
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

   if (rk == TR_CCR)
      {
      crtemp = self()->findBestFreeRegister(currentInstruction, TR_GPR);
      if (crtemp == NULL)
         crtemp = self()->freeBestRegister(currentInstruction, NULL);
      else
         crtemp->setHasBeenAssignedInMethod(true);
      }

   tmemref = new (self()->cg()->trHeapMemory()) TR::MemoryReference(currentNode, location->getSymbolReference(), TR::Compiler->om.sizeofReferenceAddress(), self()->cg());
   TR::Instruction *spillInstr = NULL;

   int32_t dataSize = spillSizeForRegister(spilledRegister);

   if (comp->getOption(TR_DisableOOL))
      {

      self()->cg()->freeSpill(location, dataSize, 0);
      }
   else
      {
      if (self()->cg()->isOutOfLineColdPath())
         {
         bool isOOLentryReverseSpill = false;
         if (currentInstruction->isLabel())
            {
            if (((TR::PPCLabelInstruction*)currentInstruction)->getLabelSymbol()->isStartOfColdInstructionStream())
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
         if (location->getMaxSpillDepth() == 2)
            {
            self()->cg()->freeSpill(location, dataSize, 0);
            location->setMaxSpillDepth(0);
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
            // Reset maxSpillDepth here so that in the cold path we know to free the spill
            // and so that the spill is not included in future GC points in the hot path while it is protected
            location->setMaxSpillDepth(0);
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
      }
   TR::Register *tempIndexRegister = NULL;
   switch (rk)
      {
      case TR_GPR:
         spillInstr = generateMemSrc1Instruction(self()->cg(),TR::InstOpCode::Op_st, currentNode, tmemref, targetRegister, currentInstruction);
            break;
      case TR_FPR:
         if (spilledRegister->isSinglePrecision())
            {
            opCode = TR::InstOpCode::stfs;
            tmemref->setLength(4);
            }
         else
            {
            opCode = TR::InstOpCode::stfd;
            tmemref->setLength(8);
            }
            spillInstr = generateMemSrc1Instruction(self()->cg(), opCode, currentNode, tmemref, targetRegister, currentInstruction);
         break;
      case TR_VSX_SCALAR:
         tempIndexRegister  = self()->findBestFreeRegister(currentInstruction, TR_GPR);
         if (tempIndexRegister  == NULL)
            tempIndexRegister = self()->freeBestRegister(currentInstruction, NULL);
         tmemref->setUsingDelayedIndexedForm();
         tmemref->setIndexRegister(tempIndexRegister);
         tmemref->setIndexModifiable();
         opCode = TR::InstOpCode::stxsdx;
         tmemref->setLength(8);
         spillInstr = generateMemSrc1Instruction(self()->cg(), opCode, currentNode, tmemref, targetRegister, currentInstruction);
         self()->cg()->stopUsingRegister(tempIndexRegister);
         break;
      case TR_CCR:
         spillInstr = generateMemSrc1Instruction(self()->cg(),TR::InstOpCode::Op_st, currentNode, tmemref, crtemp, currentInstruction);
         break;
      case TR_VSX_VECTOR:
         tempIndexRegister  = self()->findBestFreeRegister(currentInstruction, TR_GPR);
         if (tempIndexRegister  == NULL)
            tempIndexRegister = self()->freeBestRegister(currentInstruction, NULL);
         tmemref->setUsingDelayedIndexedForm();
         tmemref->setIndexRegister(tempIndexRegister);
         tmemref->setIndexModifiable();
         opCode = TR::InstOpCode::stxvd2x;
         tmemref->setLength(16);
         spillInstr = generateMemSrc1Instruction(self()->cg(), opCode, currentNode, tmemref, targetRegister, currentInstruction);
         self()->cg()->stopUsingRegister(tempIndexRegister);
         break;
      case TR_VRF:
         // Until stack frame is 16-byte aligned, we cannot use VMX load/store here
         // So, we use VSX load/store instead as a work-around

         TR_ASSERT(self()->cg()->comp()->target().cpu.getPPCSupportsVSX(), "VSX support not enabled");

         tempIndexRegister  = self()->findBestFreeRegister(currentInstruction, TR_GPR);
         if (tempIndexRegister  == NULL)
            tempIndexRegister = self()->freeBestRegister(currentInstruction, NULL);
         tmemref->setUsingDelayedIndexedForm();
         tmemref->setIndexRegister(tempIndexRegister);
         tmemref->setIndexModifiable();
         opCode = TR::InstOpCode::stxvd2x;
         tmemref->setLength(16);
         spillInstr = generateMemSrc1Instruction(self()->cg(), opCode, currentNode, tmemref, targetRegister, currentInstruction);
         self()->cg()->stopUsingRegister(tempIndexRegister);
         break;
      }
   self()->cg()->traceRAInstruction(spillInstr);
   if (rk == TR_CCR)
      {
      int32_t  tindex = targetRegister->getRegisterNumber() - TR::RealRegister::FirstCCR;
      int32_t  sindex = toPPCCRBackingStore(location)->getCcrFieldIndex();
      if (sindex != tindex)
         {
         if (sindex > tindex)
            sindex = 32 - (sindex - tindex) * 4;
         else
            sindex = (tindex - sindex) * 4;
         self()->cg()->traceRAInstruction(generateTrg1Src1Imm2Instruction(self()->cg(), TR::InstOpCode::rlwinm, currentNode, crtemp, crtemp, sindex, 0xFFFFFFFF, currentInstruction));
         }
      self()->cg()->traceRAInstruction(generateTrg1Instruction(self()->cg(), TR::InstOpCode::mfcr, currentNode, crtemp, currentInstruction));
      }

   return targetRegister;
   }


TR::RealRegister *OMR::Power::Machine::assignOneRegister(TR::Instruction *currentInstruction,
                                                     TR::Register    *virtReg,
                                                     bool            excludeGPR0)
   {
   TR_RegisterKinds    rk = virtReg->getKind();
   TR::RealRegister *assignedReg;

   self()->cg()->clearRegisterAssignmentFlags();
   self()->cg()->setRegisterAssignmentFlag(TR_NormalAssignment);

   if (virtReg->getTotalUseCount() != virtReg->getFutureUseCount())
      {
      self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
      assignedReg = self()->reverseSpillState(currentInstruction, virtReg, NULL, excludeGPR0);
      }
   else
      {
      assignedReg = self()->findBestFreeRegister(currentInstruction, rk, excludeGPR0, true, virtReg);
      if (assignedReg == NULL)
         {
         self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
         assignedReg = self()->freeBestRegister(currentInstruction, virtReg, NULL, excludeGPR0);
         }
      }

   virtReg->setAssignedRegister(assignedReg);
   assignedReg->setAssignedRegister(virtReg);
   assignedReg->setState(TR::RealRegister::Assigned);
   self()->cg()->traceRegAssigned(virtReg, assignedReg);
   return(assignedReg);
   }


void OMR::Power::Machine::coerceRegisterAssignment(TR::Instruction                           *currentInstruction,
                                              TR::Register                             *virtualRegister,
                                              TR::RealRegister::RegNum registerNumber)
   {
   TR::RealRegister *targetRegister          = _registerFile[registerNumber];
   TR::RealRegister    *realReg                 = virtualRegister->getAssignedRealRegister();
   TR::RealRegister *currentAssignedRegister = (realReg==NULL)?NULL:toRealRegister(realReg);
   TR_RegisterKinds    vr_rk = virtualRegister->getKind();

   if (currentAssignedRegister == targetRegister)
       return;
   if (  targetRegister->getState() == TR::RealRegister::Free
      || targetRegister->getState() == TR::RealRegister::Unlatched)
      {
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
         registerCopy(currentInstruction, vr_rk,
                      currentAssignedRegister, targetRegister, self()->cg());
         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }
      }
   else
      {
      TR::RealRegister *spareReg = NULL;
      TR::Register        *currentTargetVirtual;
      TR_RegisterKinds    ctv_rk;

      currentTargetVirtual = targetRegister->getAssignedRegister();
      ctv_rk = currentTargetVirtual->getKind();

      // Assumption: rk being different is only possible between VSX_S/V and FPR/VRF.
      bool currentAssignedUseful = (vr_rk == ctv_rk) || !(ctv_rk == TR_FPR || ctv_rk == TR_VRF) ||
				   (currentAssignedRegister!=NULL && currentAssignedRegister->getKind()==ctv_rk);

      // RegisterExchange: GPR/VRF have xor op always, and only CCR has no xor after P6
      bool needTemp = !((ctv_rk == TR_GPR) || (ctv_rk == TR_VRF) || (self()->cg()->comp()->target().cpu.getPPCSupportsVSX() && ctv_rk!=TR_CCR));

      if (targetRegister->getState() == TR::RealRegister::Blocked)
         {
         if ((currentAssignedRegister==NULL) || !currentAssignedUseful || needTemp)
            {
            spareReg = self()->findBestFreeRegister(currentInstruction, ctv_rk, false, false, currentTargetVirtual);

            self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
            if (spareReg == NULL)
               {
               self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
               virtualRegister->block();
               spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual);
               virtualRegister->unblock();
               }
            }
         if (currentAssignedRegister != NULL && currentAssignedUseful)
            {
            self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
            registerExchange(currentInstruction, ctv_rk,
                             targetRegister, currentAssignedRegister, spareReg,
                             self()->cg());
            currentAssignedRegister->setState(TR::RealRegister::Blocked);
            currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
            // spareReg remains FREE, if it is none NULL.
            }
         else
            {
            self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);
            registerCopy(currentInstruction, ctv_rk, targetRegister, spareReg, self()->cg());
            spareReg->setState(TR::RealRegister::Blocked);
            currentTargetVirtual->setAssignedRegister(spareReg);
            spareReg->setAssignedRegister(currentTargetVirtual);
	    // spareReg is assigned.

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
	       registerCopy(currentInstruction, vr_rk, currentAssignedRegister, targetRegister, self()->cg());
	       currentAssignedRegister->setState(TR::RealRegister::Free);
	       currentAssignedRegister->setAssignedRegister(NULL);
	       }
            }
         }
      else if (targetRegister->getState() == TR::RealRegister::Assigned)
         {
         if ((currentAssignedRegister==NULL) || !currentAssignedUseful || needTemp)
            spareReg = self()->findBestFreeRegister(currentInstruction, ctv_rk, false, false, currentTargetVirtual);

         self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
         if (currentAssignedRegister != NULL && currentAssignedUseful)
            {
            if (!needTemp || (spareReg != NULL))
               {
               self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
               registerExchange(currentInstruction, ctv_rk,
                                targetRegister, currentAssignedRegister, spareReg,
                                self()->cg());
               currentAssignedRegister->setState(TR::RealRegister::Assigned);
               currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
               currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
               // spareReg is still FREE.
               }
            else
               {
               self()->freeBestRegister(currentInstruction, currentTargetVirtual, targetRegister);
               self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
               self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
               registerCopy(currentInstruction, vr_rk, currentAssignedRegister, targetRegister, self()->cg());
               currentAssignedRegister->setState(TR::RealRegister::Free);
               currentAssignedRegister->setAssignedRegister(NULL);
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
               registerCopy(currentInstruction, ctv_rk, targetRegister, spareReg, self()->cg());
               spareReg->setState(TR::RealRegister::Assigned);
               spareReg->setAssignedRegister(currentTargetVirtual);
               currentTargetVirtual->setAssignedRegister(spareReg);
               // spareReg is assigned.
               }

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
	       registerCopy(currentInstruction, vr_rk, currentAssignedRegister, targetRegister, self()->cg());
	       currentAssignedRegister->setState(TR::RealRegister::Free);
	       currentAssignedRegister->setAssignedRegister(NULL);
	       }
            }
         self()->cg()->resetRegisterAssignmentFlag(TR_IndirectCoercion);
         }
      }
   targetRegister->setState(TR::RealRegister::Assigned);
   targetRegister->setAssignedRegister(virtualRegister);
   virtualRegister->setAssignedRegister(targetRegister);
   self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
   }


void OMR::Power::Machine::initializeRegisterFile()
   {

   _registerFile[TR::RealRegister::NoReg] = NULL;
   _registerFile[TR::RealRegister::SpilledReg] = NULL;

   _registerFile[TR::RealRegister::gr0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr0,
                                                 TR::RealRegister::gr0Mask, self()->cg());

   _registerFile[TR::RealRegister::gr1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr1,
                                                 TR::RealRegister::gr1Mask, self()->cg());

   _registerFile[TR::RealRegister::gr2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr2,
                                                 TR::RealRegister::gr2Mask, self()->cg());

   _registerFile[TR::RealRegister::gr3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr3,
                                                 TR::RealRegister::gr3Mask, self()->cg());

   _registerFile[TR::RealRegister::gr4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr4,
                                                 TR::RealRegister::gr4Mask, self()->cg());

   _registerFile[TR::RealRegister::gr5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr5,
                                                 TR::RealRegister::gr5Mask, self()->cg());

   _registerFile[TR::RealRegister::gr6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr6,
                                                 TR::RealRegister::gr6Mask, self()->cg());

   _registerFile[TR::RealRegister::gr7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr7,
                                                 TR::RealRegister::gr7Mask, self()->cg());

   _registerFile[TR::RealRegister::gr8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr8,
                                                 TR::RealRegister::gr8Mask, self()->cg());

   _registerFile[TR::RealRegister::gr9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr9,
                                                 TR::RealRegister::gr9Mask, self()->cg());

   _registerFile[TR::RealRegister::gr10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr10,
                                                 TR::RealRegister::gr10Mask, self()->cg());

   _registerFile[TR::RealRegister::gr11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr11,
                                                 TR::RealRegister::gr11Mask, self()->cg());

   _registerFile[TR::RealRegister::gr12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr12,
                                                 TR::RealRegister::gr12Mask, self()->cg());

   _registerFile[TR::RealRegister::gr13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr13,
                                                 TR::RealRegister::gr13Mask, self()->cg());

   _registerFile[TR::RealRegister::gr14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr14,
                                                 TR::RealRegister::gr14Mask, self()->cg());

   _registerFile[TR::RealRegister::gr15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr15,
                                                 TR::RealRegister::gr15Mask, self()->cg());

   _registerFile[TR::RealRegister::gr16] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr16,
                                                 TR::RealRegister::gr16Mask, self()->cg());

   _registerFile[TR::RealRegister::gr17] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr17,
                                                 TR::RealRegister::gr17Mask, self()->cg());

   _registerFile[TR::RealRegister::gr18] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr18,
                                                 TR::RealRegister::gr18Mask, self()->cg());

   _registerFile[TR::RealRegister::gr19] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr19,
                                                 TR::RealRegister::gr19Mask, self()->cg());

   _registerFile[TR::RealRegister::gr20] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr20,
                                                 TR::RealRegister::gr20Mask, self()->cg());

   _registerFile[TR::RealRegister::gr21] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr21,
                                                 TR::RealRegister::gr21Mask, self()->cg());

   _registerFile[TR::RealRegister::gr22] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr22,
                                                 TR::RealRegister::gr22Mask, self()->cg());

   _registerFile[TR::RealRegister::gr23] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr23,
                                                 TR::RealRegister::gr23Mask, self()->cg());

   _registerFile[TR::RealRegister::gr24] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr24,
                                                 TR::RealRegister::gr24Mask, self()->cg());

   _registerFile[TR::RealRegister::gr25] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr25,
                                                 TR::RealRegister::gr25Mask, self()->cg());

   _registerFile[TR::RealRegister::gr26] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr26,
                                                 TR::RealRegister::gr26Mask, self()->cg());

   _registerFile[TR::RealRegister::gr27] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr27,
                                                 TR::RealRegister::gr27Mask, self()->cg());

   _registerFile[TR::RealRegister::gr28] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr28,
                                                 TR::RealRegister::gr28Mask, self()->cg());

   _registerFile[TR::RealRegister::gr29] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr29,
                                                 TR::RealRegister::gr29Mask, self()->cg());

   _registerFile[TR::RealRegister::gr30] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr30,
                                                 TR::RealRegister::gr30Mask, self()->cg());

   _registerFile[TR::RealRegister::gr31] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                 0,
                                                 TR::RealRegister::Free,
                                                 TR::RealRegister::gr31,
                                                 TR::RealRegister::gr31Mask, self()->cg());


   _registerFile[TR::RealRegister::fp0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp0, TR::RealRegister::fp0Mask, self()->cg());

   _registerFile[TR::RealRegister::fp1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp1, TR::RealRegister::fp1Mask, self()->cg());

   _registerFile[TR::RealRegister::fp2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp2, TR::RealRegister::fp2Mask, self()->cg());

   _registerFile[TR::RealRegister::fp3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp3, TR::RealRegister::fp3Mask, self()->cg());

   _registerFile[TR::RealRegister::fp4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp4, TR::RealRegister::fp4Mask, self()->cg());

   _registerFile[TR::RealRegister::fp5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp5, TR::RealRegister::fp5Mask, self()->cg());

   _registerFile[TR::RealRegister::fp6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp6, TR::RealRegister::fp6Mask, self()->cg());

   _registerFile[TR::RealRegister::fp7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp7, TR::RealRegister::fp7Mask, self()->cg());

   _registerFile[TR::RealRegister::fp8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp8, TR::RealRegister::fp8Mask, self()->cg());

   _registerFile[TR::RealRegister::fp9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp9, TR::RealRegister::fp9Mask, self()->cg());

   _registerFile[TR::RealRegister::fp10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp10, TR::RealRegister::fp10Mask, self()->cg());

   _registerFile[TR::RealRegister::fp11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp11, TR::RealRegister::fp11Mask, self()->cg());

   _registerFile[TR::RealRegister::fp12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp12, TR::RealRegister::fp12Mask, self()->cg());

   _registerFile[TR::RealRegister::fp13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp13, TR::RealRegister::fp13Mask, self()->cg());

   _registerFile[TR::RealRegister::fp14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp14, TR::RealRegister::fp14Mask, self()->cg());

   _registerFile[TR::RealRegister::fp15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp15, TR::RealRegister::fp15Mask, self()->cg());

   _registerFile[TR::RealRegister::fp16] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp16, TR::RealRegister::fp16Mask, self()->cg());

   _registerFile[TR::RealRegister::fp17] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp17, TR::RealRegister::fp17Mask, self()->cg());

   _registerFile[TR::RealRegister::fp18] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp18, TR::RealRegister::fp18Mask, self()->cg());

   _registerFile[TR::RealRegister::fp19] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp19, TR::RealRegister::fp19Mask, self()->cg());

   _registerFile[TR::RealRegister::fp20] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp20, TR::RealRegister::fp20Mask, self()->cg());

   _registerFile[TR::RealRegister::fp21] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp21, TR::RealRegister::fp21Mask, self()->cg());

   _registerFile[TR::RealRegister::fp22] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp22, TR::RealRegister::fp22Mask, self()->cg());

   _registerFile[TR::RealRegister::fp23] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp23, TR::RealRegister::fp23Mask, self()->cg());

   _registerFile[TR::RealRegister::fp24] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp24, TR::RealRegister::fp24Mask, self()->cg());

   _registerFile[TR::RealRegister::fp25] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp25, TR::RealRegister::fp25Mask, self()->cg());

   _registerFile[TR::RealRegister::fp26] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp26, TR::RealRegister::fp26Mask, self()->cg());

   _registerFile[TR::RealRegister::fp27] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp27, TR::RealRegister::fp27Mask, self()->cg());

   _registerFile[TR::RealRegister::fp28] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp28, TR::RealRegister::fp28Mask, self()->cg());

   _registerFile[TR::RealRegister::fp29] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp29, TR::RealRegister::fp29Mask, self()->cg());

   _registerFile[TR::RealRegister::fp30] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp30, TR::RealRegister::fp30Mask, self()->cg());

   _registerFile[TR::RealRegister::fp31] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
               0, TR::RealRegister::Free, TR::RealRegister::fp31, TR::RealRegister::fp31Mask, self()->cg());

   _registerFile[TR::RealRegister::cr0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr0, TR::RealRegister::cr0Mask, self()->cg());

   _registerFile[TR::RealRegister::cr1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr1, TR::RealRegister::cr1Mask, self()->cg());

   _registerFile[TR::RealRegister::cr2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr2, TR::RealRegister::cr2Mask, self()->cg());

   _registerFile[TR::RealRegister::cr3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr3, TR::RealRegister::cr3Mask, self()->cg());

   _registerFile[TR::RealRegister::cr4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr4, TR::RealRegister::cr4Mask, self()->cg());

   _registerFile[TR::RealRegister::cr5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr5, TR::RealRegister::cr5Mask, self()->cg());

   _registerFile[TR::RealRegister::cr6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr6, TR::RealRegister::cr6Mask, self()->cg());

   _registerFile[TR::RealRegister::cr7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_CCR,
               0, TR::RealRegister::Free, TR::RealRegister::cr7, TR::RealRegister::cr7Mask, self()->cg());

   _registerFile[TR::RealRegister::lr] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
               0, TR::RealRegister::Free, TR::RealRegister::lr, TR::RealRegister::noRegMask, self()->cg());

   _registerFile[TR::RealRegister::vr0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr0, TR::RealRegister::vr0Mask, self()->cg());

   _registerFile[TR::RealRegister::vr1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr1, TR::RealRegister::vr1Mask, self()->cg());

   _registerFile[TR::RealRegister::vr2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr2, TR::RealRegister::vr2Mask, self()->cg());

   _registerFile[TR::RealRegister::vr3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr3, TR::RealRegister::vr3Mask, self()->cg());

   _registerFile[TR::RealRegister::vr4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr4, TR::RealRegister::vr4Mask, self()->cg());

   _registerFile[TR::RealRegister::vr5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr5, TR::RealRegister::vr5Mask, self()->cg());

   _registerFile[TR::RealRegister::vr6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr6, TR::RealRegister::vr6Mask, self()->cg());

   _registerFile[TR::RealRegister::vr7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr7, TR::RealRegister::vr7Mask, self()->cg());

   _registerFile[TR::RealRegister::vr8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr8, TR::RealRegister::vr8Mask, self()->cg());

   _registerFile[TR::RealRegister::vr9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr9, TR::RealRegister::vr9Mask, self()->cg());

   _registerFile[TR::RealRegister::vr10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr10, TR::RealRegister::vr10Mask, self()->cg());

   _registerFile[TR::RealRegister::vr11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr11, TR::RealRegister::vr11Mask, self()->cg());

   _registerFile[TR::RealRegister::vr12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr12, TR::RealRegister::vr12Mask, self()->cg());

   _registerFile[TR::RealRegister::vr13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr13, TR::RealRegister::vr13Mask, self()->cg());

   _registerFile[TR::RealRegister::vr14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr14, TR::RealRegister::vr14Mask, self()->cg());

   _registerFile[TR::RealRegister::vr15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr15, TR::RealRegister::vr15Mask, self()->cg());

   _registerFile[TR::RealRegister::vr16] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr16, TR::RealRegister::vr16Mask, self()->cg());

   _registerFile[TR::RealRegister::vr17] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr17, TR::RealRegister::vr17Mask, self()->cg());

   _registerFile[TR::RealRegister::vr18] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr18, TR::RealRegister::vr18Mask, self()->cg());

   _registerFile[TR::RealRegister::vr19] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr19, TR::RealRegister::vr19Mask, self()->cg());

   _registerFile[TR::RealRegister::vr20] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr20, TR::RealRegister::vr20Mask, self()->cg());

   _registerFile[TR::RealRegister::vr21] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr21, TR::RealRegister::vr21Mask, self()->cg());

   _registerFile[TR::RealRegister::vr22] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr22, TR::RealRegister::vr22Mask, self()->cg());

   _registerFile[TR::RealRegister::vr23] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr23, TR::RealRegister::vr23Mask, self()->cg());

   _registerFile[TR::RealRegister::vr24] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr24, TR::RealRegister::vr24Mask, self()->cg());

   _registerFile[TR::RealRegister::vr25] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr25, TR::RealRegister::vr25Mask, self()->cg());

   _registerFile[TR::RealRegister::vr26] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr26, TR::RealRegister::vr26Mask, self()->cg());

   _registerFile[TR::RealRegister::vr27] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr27, TR::RealRegister::vr27Mask, self()->cg());

   _registerFile[TR::RealRegister::vr28] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr28, TR::RealRegister::vr28Mask, self()->cg());

   _registerFile[TR::RealRegister::vr29] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr29, TR::RealRegister::vr29Mask, self()->cg());

   _registerFile[TR::RealRegister::vr30] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr30, TR::RealRegister::vr30Mask, self()->cg());

   _registerFile[TR::RealRegister::vr31] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF,
               0, TR::RealRegister::Free, TR::RealRegister::vr31, TR::RealRegister::vr31Mask, self()->cg());
   }

TR::RealRegister **
OMR::Power::Machine::cloneRegisterFileByType(TR::RealRegister **registerFileClone, TR::RealRegister **registerFile, int32_t start, int32_t end, TR_RegisterKinds kind, TR_AllocationKind allocKind)
   {
   TR_LiveRegisters *liveRegs = self()->cg()->getLiveRegisters(kind);
   if(liveRegs && liveRegs->getNumberOfLiveRegisters() > 0)
      {
      for (int32_t i = start; i <= end; i++)
         {
         registerFileClone[i] = (TR::RealRegister *)self()->cg()->trMemory()->allocateMemory(sizeof(TR::RealRegister), allocKind);
         memcpy(registerFileClone[i], registerFile[i], sizeof(TR::RealRegister));
         }
      }
   else
      {
      for (int32_t j = start; j <= end; j++)
         registerFileClone[j] = registerFile[j];
      }
   return registerFileClone;
   }

TR::RealRegister **
OMR::Power::Machine::cloneRegisterFile(TR::RealRegister **registerFile, TR_AllocationKind allocKind)
   {
   int32_t arraySize = sizeof(TR::RealRegister *)*TR::RealRegister::NumRegisters;
   TR::RealRegister  **registerFileClone = (TR::RealRegister **)self()->cg()->trMemory()->allocateMemory(arraySize, allocKind);
   registerFileClone = self()->cloneRegisterFileByType(registerFileClone, registerFile, TR::RealRegister::FirstGPR, TR::RealRegister::LastGPR, TR_GPR, allocKind);
   registerFileClone = self()->cloneRegisterFileByType(registerFileClone, registerFile, TR::RealRegister::FirstFPR, TR::RealRegister::LastFPR, TR_FPR, allocKind);
   registerFileClone = self()->cloneRegisterFileByType(registerFileClone, registerFile, TR::RealRegister::FirstCCR, TR::RealRegister::LastCCR, TR_CCR, allocKind);
   registerFileClone = self()->cloneRegisterFileByType(registerFileClone, registerFile, TR::RealRegister::FirstVRF, TR::RealRegister::LastVRF, TR_VRF, allocKind);

   return registerFileClone;
   }

static void registerCopy(TR::Instruction     *precedingInstruction,
                         TR_RegisterKinds    rk,
                         TR::RealRegister *targetReg,
                         TR::RealRegister *sourceReg,
                         TR::CodeGenerator   *cg)
   {
   TR::Node *currentNode = precedingInstruction->getNode();
   TR::Instruction *instr = NULL;

   // Go for performance, disregarding the dirty SP de-normal condition
   bool useVSXLogical = cg->comp()->target().cpu.getPPCSupportsVSX();
   switch (rk)
      {
      case TR_GPR:
         instr = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, currentNode, targetReg, sourceReg, precedingInstruction);
         break;
      case TR_FPR:
    	 if (useVSXLogical)
    		 instr = generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, currentNode, targetReg, sourceReg, sourceReg, precedingInstruction);
         else
            instr = generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, currentNode, targetReg, sourceReg, precedingInstruction);
         break;
      case TR_CCR:
         instr = generateTrg1Src1Instruction(cg, TR::InstOpCode::mcrf, currentNode, targetReg, sourceReg, precedingInstruction);
         break;
      case TR_VSX_SCALAR:
      case TR_VSX_VECTOR:
         instr = generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, currentNode, targetReg, sourceReg, sourceReg, precedingInstruction);
         break;
      case TR_VRF:
         instr = generateTrg1Src2Instruction(cg, TR::InstOpCode::vor, currentNode, targetReg, sourceReg, sourceReg, precedingInstruction);
         break;
      }
   cg->traceRAInstruction(instr);
   }

static void registerExchange(TR::Instruction     *precedingInstruction,
                             TR_RegisterKinds    rk,
                             TR::RealRegister *targetReg,
                             TR::RealRegister *sourceReg,
                             TR::RealRegister *middleReg,
                             TR::CodeGenerator   *cg)
   {
   TR::Node *currentNode = precedingInstruction->getNode();

   if (middleReg != NULL)
      {
      registerCopy(precedingInstruction, rk, targetReg, middleReg, cg);
      registerCopy(precedingInstruction, rk, sourceReg, targetReg, cg);
      registerCopy(precedingInstruction, rk, middleReg, sourceReg, cg);
      middleReg->setHasBeenAssignedInMethod(true); // register was used
      }
   else
      {
      TR::InstOpCode::Mnemonic opCode;
      switch (rk)
	 {
	 case TR_GPR:
	    opCode = TR::InstOpCode::XOR;
	    break;
	 case TR_FPR:
	    opCode = TR::InstOpCode::xxlxor;
	    break;
	 case TR_VSX_SCALAR:
	 case TR_VSX_VECTOR:
	    opCode = TR::InstOpCode::xxlxor;
	    break;
	 case TR_VRF:
	    opCode = TR::InstOpCode::vxor;
	    break;
	 case TR_CCR:
	    TR_ASSERT(0, "Cannot exchange CCR without a third reg");
	 }
      cg->traceRAInstruction(generateTrg1Src2Instruction(cg, opCode, currentNode, targetReg, targetReg, sourceReg, precedingInstruction));
      cg->traceRAInstruction(generateTrg1Src2Instruction(cg, opCode, currentNode, sourceReg, targetReg, sourceReg, precedingInstruction));
      cg->traceRAInstruction(generateTrg1Src2Instruction(cg, opCode, currentNode, targetReg, targetReg, sourceReg, precedingInstruction));
      }
   }

bool OMR::Power::Machine::setLinkRegisterKilled(bool b)
   {
   TR_ASSERT(self()->cg()->getCurrentEvaluationBlock(), "No current block info\n");
   return _registerFile[TR::RealRegister::lr]->setHasBeenAssignedInMethod(b);
   }

// if cleanRegState == false then this is being called to generate conditions for use in regAssocs
TR::RegisterDependencyConditions*
OMR::Power::Machine::createCondForLiveAndSpilledGPRs(bool cleanRegState, TR::list<TR::Register*> *spilledRegisterList)
   {
   // Calculate number of register dependencies required.  This step is not really necessary, but
   // it is space conscious.
   //
   int32_t c = 0;
   int32_t endReg = TR::RealRegister::LastCCR; // As the real registers order between VR and CR are exchanged, LastCCR will cover all VRFs

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
               if (self()->cg()->isOutOfLineColdPath())
                  virtReg->incOutOfLineUseCount();
               virtReg->setAssignedRegister(NULL);
               realReg->setAssignedRegister(NULL);
               realReg->setState(TR::RealRegister::Free);
               }
            }
         }

      if (spilledRegisterList)
         {
         for (auto i = spilledRegisterList->begin(); i != spilledRegisterList->end(); ++i)
            deps->addPostCondition(*i, TR::RealRegister::SpilledReg);
         }
      }

   return deps;
   }

void
OMR::Power::Machine::takeRegisterStateSnapShot()
   {
   int32_t i;
   for (i = TR::RealRegister::FirstGPR; i < TR::RealRegister::NumRegisters - 1; i++) // Skipping SpilledReg
      {
      if (i == TR::RealRegister::mq || i == TR::RealRegister::ctr)
         continue;
      _registerAssociationsSnapShot[i] = _registerAssociations[i];
      _registerStatesSnapShot[i] = _registerFile[i]->getState();
      _assignedRegisterSnapShot[i] = _registerFile[i]->getAssignedRegister();
      _registerFlagsSnapShot[i] = _registerFile[i]->getFlags();
      }
   }

void
OMR::Power::Machine::restoreRegisterStateFromSnapShot()
   {
   int32_t i;
   for (i = TR::RealRegister::FirstGPR; i < TR::RealRegister::NumRegisters - 1; i++) // Skipping SpilledReg
      {
      if (i == TR::RealRegister::mq || i == TR::RealRegister::ctr)
         continue;
      _registerFile[i]->setFlags(_registerFlagsSnapShot[i]);
      _registerFile[i]->setState(_registerStatesSnapShot[i]);
      if (_registerAssociationsSnapShot[i])
         self()->setVirtualAssociatedWithReal((TR::RealRegister::RegNum)(i), _registerAssociationsSnapShot[i]);
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

/**
 * \brief
 * Decrements the future use count of the given virtual register and unlatches it, if necessary.
 *
 * \param[in] virtualRegister The virtual register to act on.
 *
 * \details
 * This method decrements the future use count of the given virtual register. If register
 * assignment is currently stepping through an out of line code section it also decrements
 * the out of line use count. If the future use count has reached 0, or if register assignment
 * is currently stepping through the 'hot path' of a corresponding out of line code section
 * and the future use count is equal to the out of line use count (indicating that there are
 * no further uses of this virtual register in any non-OOL path) it will unlatch the register.
 * (If the register has any OOL uses remaining it will be restored to its previous assignment
 * elsewhere.)
 *
 * If the register's future use count has reaached 0 and it has a backing storage location
 * and register assignment is currently stepping through an out of line code section (where
 * we expect the free spill list to be locked) the backing storage location will be freed by
 * momentarily unlocking the free spill list.
 *
 * Typically we will free the backing storage of a spilled register once we unspill and
 * assign it, however we sometimes have reason to suppress this behavior for out of line
 * register assignment (where we lock the free spill list). However, if a register's last use
 * is in an out of line code section this will be the only opportunity we have to free its
 * backing storage. This is safe because we can be reasonably sure that dead registers have
 * no further need for their backing storage.
 */
void
OMR::Power::Machine::decFutureUseCountAndUnlatch(TR::Register *virtualRegister)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Compilation   *comp = cg->comp();
   bool               trace = comp->getOption(TR_TraceRA);

   virtualRegister->decFutureUseCount();
   if (self()->cg()->isOutOfLineColdPath())
      virtualRegister->decOutOfLineUseCount();

   TR_ASSERT(virtualRegister->getFutureUseCount() >= virtualRegister->getOutOfLineUseCount(), "Future use count less than out of line use count");

   // This register should be unlatched if there are no more uses
   // or
   // if we're currently in the hot path and all remaining uses are out of line.
   //
   // If the only remaining uses are out of line, then this register should be unlatched
   // here, and when the register allocator reaches the branch to the outlined code it
   // will revive the register and proceed to allocate registers in the outlined code,
   // where presumably the future use count will finally hit 0.
   if (virtualRegister->getFutureUseCount() == 0 ||
       (self()->cg()->isOutOfLineHotPath() && virtualRegister->getFutureUseCount() == virtualRegister->getOutOfLineUseCount()))
      {
      if (virtualRegister->getFutureUseCount() != 0)
         {
         if (trace)
            {
            traceMsg(comp,"\nOOL: %s's remaining uses are out-of-line, unlatching\n", self()->cg()->getDebug()->getName(virtualRegister));
            }
         }
      virtualRegister->getAssignedRealRegister()->setState(TR::RealRegister::Unlatched);
      virtualRegister->setAssignedRegister(NULL);
      }

   // If this register is dead and we're assigning on an out of line path
   // we need to free its backing store, if any, otherwise we will not have
   // any future opportunity to do so and the backing store will never be re-used.
   TR_BackingStore *location = virtualRegister->getBackingStorage();
   if (virtualRegister->getFutureUseCount() == 0 && location != NULL && self()->cg()->isOutOfLineColdPath())
      {
      TR_ASSERT(cg->isFreeSpillListLocked(), "Expecting the free spill list to be locked on this path");
      int32_t size = spillSizeForRegister(virtualRegister);
      if (trace)
         traceMsg(comp, "\nFreeing backing storage " POINTER_PRINTF_FORMAT " of size %u from dead virtual %s\n", location, size, cg->getDebug()->getName(virtualRegister));
      cg->unlockFreeSpillList();
      cg->freeSpill(location, size, 0);
      virtualRegister->setBackingStorage(NULL);
      location->setMaxSpillDepth(0);
      cg->lockFreeSpillList();
      }
   }

/**
 * \brief
 * For every currently assigned register, frees its backing storage, if any.
 *
 * \details
 * This method iterates through the physical register file and, for every register in
 * the assigned state, insures that if the assigned virtual register has a backing
 * storage location (a.k.a. a spill slot) that it is properly freed.
 *
 * Typically we will free the backing storage of a spilled register once we unspill and
 * assign it, however we sometimes have reason to suppress this behavior for out of line
 * register assignment (where we lock the free spill list). This routine is intended to be
 * called once we complete out of line register assignment and can safely free the spill
 * slot of any currently assigned register.
 */
void
OMR::Power::Machine::disassociateUnspilledBackingStorage()
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Compilation   *comp = cg->comp();
   bool               trace = comp->getOption(TR_TraceRA);

   TR_ASSERT(!cg->isOutOfLineHotPath() && !cg->isOutOfLineColdPath(), "Should only be called once register assigner is out of OOL control flow region");
   TR_ASSERT(!cg->isFreeSpillListLocked(), "Should only be called once the free spill list is unlocked");

   for (int32_t i = TR::RealRegister::FirstGPR; i < TR::RealRegister::NumRegisters - 1; i++) // Skipping SpilledReg
      {
      if (i == TR::RealRegister::mq || i == TR::RealRegister::ctr)
         continue;
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         TR::Register    *virtReg = _registerFile[i]->getAssignedRegister();
         TR_BackingStore *location = virtReg->getBackingStorage();

         if (location != NULL)
            {
            int32_t size = spillSizeForRegister(virtReg);
            if (trace)
               traceMsg(comp, "\nDisassociating backing storage " POINTER_PRINTF_FORMAT " of size %u from assigned virtual %s\n", location, size, cg->getDebug()->getName(virtReg));
            cg->freeSpill(location, size, 0);
            virtReg->setBackingStorage(NULL);
            location->setMaxSpillDepth(0);
            }
         }
      }
   }
