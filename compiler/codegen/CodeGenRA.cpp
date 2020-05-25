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

#include "codegen/CodeGenerator.hpp"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveReference.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O CODE GENERATION: "

#define NUM_REGS_USED_BY_COMPLEX_OPCODES 3

namespace TR { class RealRegister; }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
void
OMR::CodeGenerator::checkForLiveRegisters(TR_LiveRegisters *liveRegisters)
   {
   TR_Debug * debug = self()->comp()->getDebug();
   bool regsAreLive = false;

   if (liveRegisters && liveRegisters->getFirstLiveRegister())
      {
      traceMsg(self()->comp(), "\n\n");
      for (TR_LiveRegisterInfo *p = liveRegisters->getFirstLiveRegister(); p; p = p->getNext())
         {
         if (p->getNode())
            traceMsg(self()->comp(), "Virtual register %s (for node [%s], ref count=%d) is live at end of method\n",
                        debug->getName(p->getRegister()), debug->getName(p->getNode()), p->getNode()->getReferenceCount());
         else
            traceMsg(self()->comp(), "Virtual register %s %p is live at end of method with node count %d\n", debug->getName(p->getRegister()), p->getRegister(), p->getNodeCount());
         regsAreLive = true;
         }
      }

   TR_ASSERT(!regsAreLive, "Virtual or real registers are live at end of method");
   }
#endif


void
OMR::CodeGenerator::estimateRegisterPressure(TR::Node *node, int32_t &registerPressure, int32_t &maxRegisterPressure, int32_t maxRegisters, TR_BitVector *valuesInGlobalRegs, bool isCold, vcount_t visitCount, TR::SymbolReference *symRef, bool &symRefIsLive, bool checkForIMuls, bool &vmThreadUsed)
   {
   if (node->getVisitCount() == visitCount)
      {
      node->setLocalIndex(node->getLocalIndex()-1);
      if (node->getLocalIndex() == 0)
         {

         if (node->getOpCode().isLoadVar() &&
             node->getSymbol()->isAutoOrParm() &&
             valuesInGlobalRegs &&
             valuesInGlobalRegs->get(node->getSymbolReference()->getReferenceNumber()))
            {
            //
            // This node has already been counted in num registers live
            //
            }
         else
            {

            if (!node->getOpCode().isLoadConst())
               {
               registerPressure--;
               if ((node->getType().isInt64()) &&
                   self()->comp()->target().is32Bit())
                  registerPressure--;
               }

            if (node->getOpCode().isLoadVar() &&
                node->getSymbolReference() == symRef)
               {
               symRefIsLive = false;
               }
            }
         }
      return;
      }

   node->setVisitCount(visitCount);
   if (node->getReferenceCount() > 0)
      node->setLocalIndex(node->getReferenceCount()-1);
   else
      node->setLocalIndex(0);

   int32_t childCount;
   for (childCount = node->getNumChildren()-1; childCount >= 0; childCount--)
      self()->estimateRegisterPressure(node->getChild(childCount), registerPressure, maxRegisterPressure, maxRegisters, valuesInGlobalRegs, isCold, visitCount, symRef, symRefIsLive, checkForIMuls, vmThreadUsed);

   bool highRegisterPressureOpCode = false;
   if (node->getOpCode().isResolveCheck() ||
       (node->getOpCode().isCall()) ||
       node->getOpCodeValue() == TR::New ||
       node->getOpCodeValue() == TR::newarray ||
       node->getOpCodeValue() == TR::anewarray ||
       node->getOpCodeValue() == TR::multianewarray ||
       node->getOpCodeValue() == TR::MergeNew ||
       node->getOpCodeValue() == TR::monent ||
       node->getOpCodeValue() == TR::monexit ||
       node->getOpCodeValue() == TR::checkcast ||
       node->getOpCodeValue() == TR::instanceof ||
       node->getOpCodeValue() == TR::arraycopy)
      highRegisterPressureOpCode = true;

   if (highRegisterPressureOpCode ||
       node->getOpCodeValue() == TR::asynccheck)
      vmThreadUsed = true;

   // Allow these call-like opcodes in cold blocks
   // as we can afford to spill in cold blocks
   //
   if (node->getLocalIndex() > 0)
      {
      if (node->getOpCode().isLoadVar() &&
          node->getSymbol()->isAutoOrParm() &&
          valuesInGlobalRegs &&
          valuesInGlobalRegs->get(node->getSymbolReference()->getReferenceNumber()))
         {
         //
         // This node has already been counted in num registers live
         //
         }
      else
         {
         if (!node->getOpCode().isLoadConst())
            {
            bool gprCandidate = true;
            TR::DataType dtype = node->getDataType();
            if (dtype == TR::Float
                || dtype == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                || dtype == TR::DecimalFloat
                || dtype == TR::DecimalDouble
                || dtype == TR::DecimalLongDouble
#endif
                 )
               gprCandidate = false;
            if (node->getDataType() == TR::Float
                || node->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                || node->getDataType() == TR::DecimalFloat
                || node->getDataType() == TR::DecimalDouble
#endif
                )
               {
               if (!gprCandidate)
                  registerPressure++;
               }
#ifdef J9_PROJECT_SPECIFIC
            else if (node->getDataType() == TR::DecimalLongDouble)
               {
               if (!gprCandidate)
                  registerPressure +=2;
               }
#endif
            else
               {
               if (gprCandidate)
                  {
                  registerPressure++;
                  if ((node->getType().isInt64()) &&
                      self()->comp()->target().is32Bit())
                     registerPressure++;
                  }
               }
            }

         if (node->getOpCode().isLoadVar() &&
             node->getSymbolReference() == symRef)
            {
            symRefIsLive = true;
            }

         if (!symRefIsLive)
            {
            //These opcodes result in high register usage
            //
            if (highRegisterPressureOpCode ||
                ((node->getType().isInt64()) &&
                 self()->comp()->target().is32Bit() &&
                 (node->getOpCode().isMul() ||
                  node->getOpCode().isDiv() ||
                  node->getOpCode().isRem() ||
                  node->getOpCode().isLeftShift() ||
                  node->getOpCode().isRightShift() ||
                  node->getOpCode().isBooleanCompare())))
               {
               if (!isCold)
                  {
                  if (registerPressure >= (maxRegisters - NUM_REGS_USED_BY_COMPLEX_OPCODES -1)) // Only allow if register pressure is known to be very low
                     {
                     maxRegisterPressure = maxRegisters;
                     }
                  }
               }
            else if (checkForIMuls &&
                     (node->getOpCode().isMul() ||
                      node->getOpCode().isDiv()))
               {
               if (!isCold)
                  {
                  maxRegisterPressure = maxRegisters;
                  }
               }
            else
               {
               if (registerPressure > maxRegisterPressure)
                  {
                  if (!isCold)
                     maxRegisterPressure = registerPressure;
                  }
               }
            }
         }
      }
   }



int32_t
OMR::CodeGenerator::estimateRegisterPressure(TR::Block *block, vcount_t visitCount, int32_t maxStaticFrequency, int32_t maxFrequency, bool &vmThreadUsed, int32_t numGlobalRegs, TR_BitVector *valuesInGlobalRegs, TR::SymbolReference * symRef, bool checkForIMuls)
   {
   int32_t maxRegisterPressure = numGlobalRegs;
   int32_t currRegisterPressure = numGlobalRegs;
   block = block->startOfExtendedBlock();
   TR::TreeTop *treeTop = block->getEntry()->getNextTreeTop();
   bool isCold = false;

   if (maxFrequency < 0)
      {
      maxFrequency = 0;
      for (TR::CFGNode *node = self()->comp()->getFlowGraph()->getFirstNode(); node; node = node->getNext())
         {
         if (node->getFrequency() > maxFrequency)
             maxFrequency = node->getFrequency();
         }
      }

   //
   // Use some measure for coldness
   //
   if (block->isCold() ||
       ((maxFrequency > 0) &&
        (block->getFrequency()*100/maxFrequency < 20)))
      isCold = true;

   if (!isCold)
      {
      TR_BlockStructure *blockStructure = block->getStructureOf();
      int32_t blockWeight = 1;
      if (blockStructure &&
          !block->isCold())
         {
         blockStructure->calculateFrequencyOfExecution(&blockWeight);
         }

      if ((maxStaticFrequency > 0) &&
           (blockWeight*100/maxStaticFrequency < 20))
         isCold = true;
      }

   bool symRefIsLive = false;
   while (treeTop)
      {
      TR::Node *node = treeTop->getNode();
      self()->estimateRegisterPressure(node, currRegisterPressure, maxRegisterPressure, self()->getMaximumNumbersOfAssignableGPRs(), valuesInGlobalRegs, isCold, visitCount, symRef, symRefIsLive, checkForIMuls, vmThreadUsed);

      if (vmThreadUsed &&
          (maxRegisterPressure >= self()->getMaximumNumbersOfAssignableGPRs()))
         {
         //dumpOptDetails("For global register candidate %d reg pressure reached limit %d at block_%d isCold %d maxFreq %d\n", rc->getSymbolReference()->getReferenceNumber(), maxRegisterPressure, liveBlockNum, isCold, maxFrequency);
         break;
         }

      if (node->getOpCodeValue() == TR::BBStart)
         {
         if (!node->getBlock()->isExtensionOfPreviousBlock())
            break;

         //
         // Use some measure for coldness
         //
         if (node->getBlock()->isCold() ||
             ((maxFrequency > 0) &&
              (node->getBlock()->getFrequency()*100/maxFrequency < 20)))
            isCold = true;
         else
            {
            TR_BlockStructure *blockStructure = block->getStructureOf();
            int32_t blockWeight = 1;
            if (blockStructure &&
                !block->isCold())
               {
               blockStructure->calculateFrequencyOfExecution(&blockWeight);
               }

            if ((maxStaticFrequency > 0) &&
                (blockWeight*100/maxStaticFrequency < 20))
               isCold = true;
            else
               isCold = false;
            }
         }

      treeTop = treeTop->getNextTreeTop();
      }

   return maxRegisterPressure;
   }

// Set the future use count on all registers, and keep track of what kind of registers
// need to be assigned.
//
TR_RegisterKinds
OMR::CodeGenerator::prepareRegistersForAssignment()
   {
   TR_Array<TR::Register *>&    regArray=self()->getRegisterArray();
   TR::Register               *registerCursor;
   TR_RegisterMask           km, foundKindsMask = 0;
   int i;

   for (i=0; i < regArray.size(); i++)
      {
      registerCursor = regArray[i];
      if (registerCursor->getKind() != TR_SSR)
         registerCursor->setFutureUseCount(registerCursor->getTotalUseCount());
      km = registerCursor->getKindAsMask();

      if (!(foundKindsMask & km))
         foundKindsMask |= km;
      }

   return TR_RegisterKinds(foundKindsMask);
   }


TR_BackingStore *
OMR::CodeGenerator::allocateInternalPointerSpill(TR::AutomaticSymbol *pinningArrayPointer)
   {
   // Search for an existing free spill slot for this pinningArrayPointer
   //
   TR_BackingStore *spill = NULL;
   for(auto i = self()->getInternalPointerSpillFreeList().begin(); i != self()->getInternalPointerSpillFreeList().end(); ++i)
   	  {
      if ((*i)->getSymbolReference()->getSymbol()->getAutoSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer() == pinningArrayPointer)
         {
         spill = *i;
         _internalPointerSpillFreeList.remove(spill);
         break;
         }
      }

   // If there isn't an existing one, make a new one
   //
   if (!spill)
      {
      int32_t slot;
      TR::AutomaticSymbol *spillSymbol =
         TR::AutomaticSymbol::createInternalPointer(self()->trHeapMemory(),
                                                   TR::Address,
                                                   TR::Compiler->om.sizeofReferenceAddress(),
                                                   self()->fe());
      spillSymbol->setSpillTempAuto();
      spillSymbol->setPinningArrayPointer(pinningArrayPointer);
      self()->comp()->getMethodSymbol()->addAutomatic(spillSymbol);
      spill = new (self()->trHeapMemory()) TR_BackingStore(self()->comp()->getSymRefTab(), spillSymbol, 0);
      slot = spill->getSymbolReference()->getCPIndex();
      slot = (slot < 0) ? (-slot - 1) : slot;
      self()->comp()->getJittedMethodSymbol()->getAutoSymRefs(slot).add(spill->getSymbolReference());
      _allSpillList.push_front(spill);
      }

   spill->setIsOccupied();
   return spill;
   }

TR_BackingStore *
OMR::CodeGenerator::allocateSpill(bool containsCollectedReference, int32_t *offset, bool reuse)
    {
    return self()->allocateSpill((int32_t)(TR::Compiler->om.sizeofReferenceAddress()), containsCollectedReference, offset, reuse); // cast parameter explicitly
    }

TR_BackingStore *
OMR::CodeGenerator::allocateSpill(int32_t dataSize, bool containsCollectedReference, int32_t *offset, bool reuse)
   {
   TR_ASSERT_FATAL(dataSize <= 16, "assertion failure");
   TR_ASSERT_FATAL(!containsCollectedReference || (dataSize == TR::Compiler->om.sizeofReferenceAddress()), "assertion failure");

   if (self()->comp()->getOption(TR_TraceRA))
      traceMsg(self()->comp(), "\nallocateSpill(%d, %s, %s)", dataSize, containsCollectedReference? "collected":"uncollected", offset? "offset":"NULL");

   if (offset && self()->comp()->getOption(TR_DisableHalfSlotSpills))
      {
      // Pretend the caller can't handle half-slot spills
      *offset = 0;
      offset = NULL;
      }

   TR_BackingStore *spill = NULL;

   // mapStack will get confused if we put a 4-byte collected reference in an
   // 8-byte spill temp.  It will map the temp according to its GC map index
   // multiplied by the size of a Java pointer, and that will only give it 4 bytes.
   //
   // TODO: Make mapStack smarter and support this in the GC maps.
   //
   bool try8ByteSpills = (dataSize < 16) && ((TR::Compiler->om.sizeofReferenceAddress() == 8) || !containsCollectedReference);
   bool try16ByteSpills = (dataSize == 16);

   if(reuse)
     {
     if (dataSize <= 4 && !self()->getSpill4FreeList().empty())
     {
       spill = self()->getSpill4FreeList().front();
       self()->getSpill4FreeList().pop_front();
     }
     if (!spill && try8ByteSpills && !self()->getSpill8FreeList().empty())
     {
       spill = self()->getSpill8FreeList().front();
       self()->getSpill8FreeList().pop_front();
     }
     if (!spill && try16ByteSpills && !self()->getSpill16FreeList().empty())
     {
       spill = self()->getSpill16FreeList().front();
       self()->getSpill16FreeList().pop_front();
     }
     if (
         (spill && self()->comp()->getOption(TR_TraceRA) && !performTransformation(self()->comp(), "O^O SPILL TEMPS: Reuse spill temp %s\n", self()->getDebug()->getName(spill->getSymbolReference()))))
       {
       // Discard the spill temp we popped and never use it again; allocate a
       // new one instead, and later, where we would have returned this spill
       // temp to the free list, we return the new one instead and continue
       // using that one for future allocations.
       //
       // This approach was chosen because it tends to make lastOptTransformationIndex
       // produce a smaller delta than if we had left the spill in the lists.
       // TODO: Use the same size for the new spill temp so that it will be freed to
       // the same list, thereby further reducing the delta.
       //
       spill = NULL;
       }
     }

   TR::AutomaticSymbol *spillSymbol;
   if (spill)
      {
      spillSymbol = spill->getSymbolReference()->getSymbol()->getAutoSymbol();
      }
   else
      {
      // Must allocate a new one
      //
      int spillSize = std::max(dataSize, static_cast<int32_t>(TR::Compiler->om.sizeofReferenceAddress()));

      TR_ASSERT(4 <= spillSize && spillSize <= 16, "Spill temps should be between 4 and 16 bytes");
      spillSymbol = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::NoType,spillSize);
      spillSymbol->setSpillTempAuto();
      self()->comp()->getMethodSymbol()->addAutomatic(spillSymbol);
      spill = new (self()->trHeapMemory()) TR_BackingStore(self()->comp()->getSymRefTab(), spillSymbol, 0);
      int32_t slot = spill->getSymbolReference()->getCPIndex();
      slot = (slot < 0) ? (-slot - 1) : slot;
      self()->comp()->getJittedMethodSymbol()->getAutoSymRefs(slot).add(spill->getSymbolReference());
      _allSpillList.push_front(spill);
      }

   if (dataSize <= 4 && spillSymbol->getSize() == 8)
      {
      // We must only set the backing store half-occupied flags so that
      // freeSpill will do the right thing.
      //
      if (  offset == NULL
         || spill->secondHalfIsOccupied()
            || !performTransformation(self()->comp(), "O^O HALF-SLOT SPILLS: Use second half of %s\n", self()->getDebug()->getName(spill->getSymbolReference())))
         {
         spill->setFirstHalfIsOccupied();
         }
      else
         {
         *offset = 4;
         spill->setSecondHalfIsOccupied();
         self()->getSpill4FreeList().push_front(spill);
         }
      }
   else
      {
      spill->setIsOccupied();
      }

   if (containsCollectedReference && spillSymbol->getGCMapIndex() < 0)
      {
      spillSymbol->setGCMapIndex(self()->getStackAtlas()->assignGCIndex());
      _collectedSpillList.push_front(spill);
      if (self()->comp()->getOption(TR_TraceRA))
         traceMsg(self()->comp(), "\n -> added to collectedSpillList");
      }
   spill->setContainsCollectedReference(containsCollectedReference);

   TR_ASSERT(spill->isOccupied(), "assertion failure");
   if (self()->comp()->getOption(TR_TraceRA))
     traceMsg(self()->comp(), "\nallocateSpill returning (%s(%d%d), %d) ", self()->getDebug()->getName(spill->getSymbolReference()->getSymbol()), spill->firstHalfIsOccupied()?1:0, spill->secondHalfIsOccupied()?1:0, offset? *offset : 0);
   return spill;
   }

void
OMR::CodeGenerator::freeSpill(TR_BackingStore *spill, int32_t dataSize, int32_t offset)
   {
   TR_ASSERT(1 <= dataSize && dataSize <= 16, "assertion failure");
   TR_ASSERT(offset == 0 || offset == 4, "assertion failure");
   TR_ASSERT(dataSize + offset <= 16, "assertion failure");

   if (self()->comp()->getOption(TR_TraceRA))
      {
      traceMsg(self()->comp(), "\nfreeSpill(%s(%d%d), %d, %d, isLocked=%d)",
               self()->getDebug()->getName(spill->getSymbolReference()->getSymbol()),
               spill->firstHalfIsOccupied()?1:0,
               spill->secondHalfIsOccupied()?1:0,
               dataSize, offset, self()->isFreeSpillListLocked());
      }

   // Do not add a freed spill slot back onto the free list if the list is locked.
   // This is to enforce re-use of the same spill slot for a virtual register
   // while assigning non-linear control flow regions.
   //
   // However, we must still clear the occupied status of the temp.
   //
   bool updateFreeList = self()->isFreeSpillListLocked() ? false : true;

   if (spill->getSymbolReference()->getSymbol()->getAutoSymbol()->isInternalPointer())
      {
      spill->setIsEmpty();

      if (updateFreeList)
         {
         _internalPointerSpillFreeList.push_front(spill);
         if (self()->comp()->getOption(TR_TraceRA))
            traceMsg(self()->comp(), "\n -> Added to internalPointerSpillFreeList");
         }
      }
   else if (dataSize <= 4 && spill->getSymbolReference()->getSymbol()->getSize() == 8)
      {
      if (offset == 0)
         {
         spill->setFirstHalfIsEmpty();
         if (self()->comp()->getOption(TR_TraceRA))
            traceMsg(self()->comp(), "\n -> setFirstHalfIsEmpty");
         }
      else
         {
         TR_ASSERT(offset == 4, "assertion failure");
         spill->setSecondHalfIsEmpty();
         if (self()->comp()->getOption(TR_TraceRA))
            traceMsg(self()->comp(), "\n -> setSecondHalfIsEmpty");
         }

      if (spill->isEmpty())
         {
         TR_ASSERT(spill->isEmpty(), "assertion failure");

         if (updateFreeList)
            {
            _spill4FreeList.remove(spill); // It may have been half-full before
            _spill8FreeList.push_front(spill);
            if (self()->comp()->getOption(TR_TraceRA))
               traceMsg(self()->comp(), "\n -> moved to spill8FreeList");
            }
         }
      else if (spill->firstHalfIsOccupied())
         {
         // TODO: Once every caller can cope with nonzero offsets, we should add first-half-occupied symbols into _spill4FreeList.
         if (self()->comp()->getOption(TR_TraceRA))
            traceMsg(self()->comp(), "\n -> first half is still occupied; conservatively keeping out of spill4FreeList");
         }
      else
         {
         TR_ASSERT(spill->secondHalfIsOccupied(), "assertion failure");

         if (updateFreeList)
            {
            // Half-free
            _spill4FreeList.push_front(spill);
            if (self()->comp()->getOption(TR_TraceRA))
               traceMsg(self()->comp(), "\n -> moved to spill4FreeList");
            }
         }
      }
   else
      {
      spill->setIsEmpty();

      if (updateFreeList)
         {
         if (spill->getSymbolReference()->getSymbol()->getSize() <= 4)
            {
            _spill4FreeList.push_front(spill);
            if (self()->comp()->getOption(TR_TraceRA))
               traceMsg(self()->comp(), "\n -> added to spill4FreeList");
            }
         else if (spill->getSymbolReference()->getSymbol()->getSize() == 8)
            {
            _spill8FreeList.push_front(spill);
            if (self()->comp()->getOption(TR_TraceRA))
               traceMsg(self()->comp(), "\n -> added to spill8FreeList");
            }
         else if (spill->getSymbolReference()->getSymbol()->getSize() == 16)
            {
            _spill16FreeList.push_front(spill);
            if (self()->comp()->getOption(TR_TraceRA))
               traceMsg(self()->comp(), "\n -> added to spill16FreeList");
            }
         }
      }
   }

void
OMR::CodeGenerator::jettisonAllSpills()
   {
   if (self()->comp()->getOption(TR_TraceRA))
      traceMsg(self()->comp(), "jettisonAllSpills: Clearing spill-temp freelists\n");
   _spill4FreeList.clear();
   _spill8FreeList.clear();
   _spill16FreeList.clear();
   _internalPointerSpillFreeList.clear();
   }

TR::Register * OMR::CodeGenerator::allocateRegister(TR_RegisterKinds rk)
   {
   TR_ASSERT(rk != TR_SSR,"use allocatePseudoRegister for TR_SSR registers\n");
   TR::Register * temp = new (self()->trHeapMemory()) TR::Register(rk);
   self()->addAllocatedRegister(temp);
   if (self()->getDebug())
      self()->getDebug()->newRegister(temp);

   return temp;
   }

void
OMR::CodeGenerator::findAndFixCommonedReferences()
   {
   //
   // TODO : Required to re-assess the feasibility of the spill
   // strategy currently adopted at calls (potential GC points) where
   // live references are stored in memory and restored when they need
   // to be used. With register maps in place, the number of spills
   // because of a call might possibly be reduced.
   //
   // When the above adjustment is made, it might be feasible for
   // nodes to linked together across calls without necessarily incurring
   // the cost of spilling the value (corresponding to the node)
   // before a call. Might need to increase the strength of Local
   // Dead Load Elimination in that case to perform linking of loads across
   // calls.
   //
   //
   if (debug("traceSpill"))
      {
      diagnostic("Start spilling at GC safe points\n");
      self()->comp()->dumpMethodTrees("Trees before spilling");
      }

   self()->comp()->incVisitCount();
   for (TR::TreeTop *tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      bool isSafePoint = node->canGCandReturn();
      if (isSafePoint)
         {
         TR::Node *safePointNode;
         if (node->getOpCodeValue() == TR::treetop ||
             node->getOpCode().isResolveOrNullCheck())
            {
            safePointNode = node->getFirstChild();
            }
         else
            {
            safePointNode = node;
            }

         if (safePointNode->getVisitCount() != self()->comp()->getVisitCount())
            {
            // Remember the first available spill temp. Temps added to the available
            // spill temp list by children of this node shouldn't be used to spill
            // around this node, since the temp will have a new value stored before
            // this treetop.
            //
            auto firstAvailableSpillTemp = _availableSpillTemps.begin();
            // Find commoned references in the children of the safe point node.
            // They must be spilled to temps to prepare for this possible GC.
            //
            self()->findCommonedReferences(safePointNode, tt);

            // Now spill the live references to temps
            //
            if (!_liveReferenceList.empty())
               {
               self()->spillLiveReferencesToTemps(tt->getPrevTreeTop(), firstAvailableSpillTemp);
               }
            }

         // Finally see if the safe point node itself is a commoned reference.
         // If so, it will become a live reference for the next GC point.
         //
         if (node != safePointNode)
            {
            self()->findCommonedReferences(node, tt);
            }
         }
      else
         {
         // Find commoned references in this tree.
         //
         self()->findCommonedReferences(node, tt);
         }
      }

   if (debug("traceSpill"))
      diagnostic("End spilling at GC safe points\n");
   }

void
OMR::CodeGenerator::findCommonedReferences(TR::Node *node, TR::TreeTop *treeTop)
   {
   if (node->getVisitCount() == self()->comp()->getVisitCount())
      return;

   node->setVisitCount(self()->comp()->getVisitCount());
   for (int32_t childCount = node->getNumChildren() - 1; childCount >= 0; childCount--)
      {
      TR::Node *child = node->getChild(childCount);

      // Visit the children and add their children's references before adding
      // them to the live reference list. This makes sure that the live reference
      // list is kept in the order in which references must be processed.
      //
      if (child->getVisitCount() != self()->comp()->getVisitCount())
         {
         self()->findCommonedReferences(child, treeTop);
         }
      if (child->getDataType() == TR::Address &&
          !child->getOpCode().isLoadConst()  &&
          child->getOpCodeValue() != TR::loadaddr)
         {
         TR::Symbol * sym = child->getOpCode().hasSymbolReference() ? child->getSymbol() : 0;

         if (child->getReferenceCount() > 1)
            {
            // This is a reference that may be live across a possible GC
            //
            if (!sym || !sym->isNotCollected())
               self()->processReference(child, node, treeTop);
            }
         else if (sym && sym->isSpillTempAuto())
            {
            // This is the only (and hence last) use of a spill temp so it can
            // be released.
            //
            _availableSpillTemps.push_front(child->getSymbolReference());
            if (debug("traceSpill"))
               diagnostic("Release spill temp [%p] at node [%p]\n",child->getSymbolReference(),child);
            }
         }
      }
   }

void
OMR::CodeGenerator::processReference(TR::Node *reference, TR::Node *parent, TR::TreeTop *treeTop)
   {

   // The live reference list appender is used to add and remove elements from the
   // live reference list, so that the list is maintained in the order in which
   // elements are inserted.
   //
   // The list must be maintained in this order so that if both a parent and a
   // child are in the live reference list, when it comes time to spill the
   // original parent will be updated to reference the new copy of the child
   // before a new copy of the parent is created.
   //
   auto iterator = _liveReferenceList.begin();
   TR_LiveReference *cursor;
   while (iterator != _liveReferenceList.end())
      {
      if ((*iterator)->getReferenceNode() == reference)
         {
         // if this is the last parent, then we have found all the parents of the
         // reference and must remove this reference from the list because it can not
         // be alive across any future calls that may be encountered.
         if (reference->getReferenceCount() == (*iterator)->getNumberOfParents() + 1)
            {
        	 _liveReferenceList.erase(iterator);

            // If this reference was to a spill temp, that temp can be freed
            // up to be re-used
            //
            if (reference->getOpCode().hasSymbolReference())
               {
               TR::SymbolReference * symRef = reference->getSymbolReference();
               if (symRef->getSymbol()->isSpillTempAuto())
                  {
                  _availableSpillTemps.push_front(symRef);
                  if (debug("traceSpill"))
                     diagnostic("Release spill temp [%p] at node [%p]\n",symRef,reference);
                  }
               }
            }
         // otherwise we add it to the list so that we will be able to find it
         // if it turns out to be alive across a GC safe point and we have to introduce
         // a temp and fix up all the parents.
         else
            {
        	 (*iterator)->addParentToList(parent);
            self()->needSpillTemp(*iterator, parent, treeTop);
            }
         return;
         }
      ++iterator;
      }

   cursor = new (self()->trHeapMemory()) TR_LiveReference(reference, parent, self()->trMemory());
   _liveReferenceList.push_back(cursor);
   self()->needSpillTemp(cursor, parent, treeTop);
   }

void
OMR::CodeGenerator::spillLiveReferencesToTemps(TR::TreeTop *insertionTree, std::list<TR::SymbolReference*, TR::typed_allocator<TR::SymbolReference*, TR::Allocator> >::iterator firstAvailableSpillTemp)
   {
   if (debug("traceSpill") && !_liveReferenceList.empty())
      diagnostic("Spill at GC safe node [%p]\n",insertionTree->getNextTreeTop()->getNode());

   auto referenceIterator = _liveReferenceList.begin();
   while (referenceIterator != _liveReferenceList.end())
      {
      TR::Node *originalReference = (*referenceIterator)->getReferenceNode();

      // Find or allocate a spill temp.
      // If the reference is to an existing spill temp there is no need to
      // generate another store, we can just create another load after the
      // GC point.
      //
      // If the reference is a simple load and the load is not killed before the
      // first use after this GC point, there is no need for a spill temp, the
      // original load can just be duplicated at the first use after this point.
      //
      // Otherwise pick up an available spill temp from the free list or create
      // a new one.

      bool storeSpillTemp = false;
      TR::SymbolReference *tempSymRef = NULL;
      if (originalReference->getOpCode().hasSymbolReference() &&
          originalReference->getSymbol()->isSpillTempAuto())
         {
         tempSymRef = originalReference->getSymbolReference();
         if (debug("traceSpill"))
            diagnostic("   Re-use spill temp [%p] for reference [%p] without storing\n",tempSymRef,originalReference);
         }
      else if ((*referenceIterator)->needSpillTemp())
         {
         if (!_availableSpillTemps.empty())
            {
            tempSymRef = *firstAvailableSpillTemp;
            firstAvailableSpillTemp = _availableSpillTemps.erase(firstAvailableSpillTemp);
            if (debug("traceSpill"))
               diagnostic("   Re-use available spill temp [%p] for reference [%p]\n",tempSymRef,originalReference);
            }
         else
            {
            int32_t slot;
            TR::AutomaticSymbol *tempSym = TR::AutomaticSymbol::create(self()->trHeapMemory());
            tempSym->setSize(TR::DataType::getSize(TR::Address));
            tempSym->setSpillTempAuto();
            tempSymRef = new (self()->trHeapMemory()) TR::SymbolReference(self()->comp()->getSymRefTab(), tempSym);
            slot = tempSymRef->getCPIndex();
            slot = (slot < 0) ? (-slot - 1) : slot;
            self()->comp()->getJittedMethodSymbol()->getAutoSymRefs(slot).add(tempSymRef);
            self()->comp()->getMethodSymbol()->addAutomatic(tempSym);
            if (debug("traceSpill"))
               diagnostic("   Create new spill temp [%p] for reference [%p]\n",tempSymRef,originalReference);
            }
         storeSpillTemp = true;
         }
      else
         {
         tempSymRef = NULL;
         if (debug("traceSpill"))
            diagnostic("   No need to spill live reference [%p]\n",originalReference);
         }

      // Create a copy of the reference for use by parents before the GC point.
      // Reference count will be number of parents before the GC safe point; if
      // there is a store to a spill temp the reference count will be incremented
      // to take account of this extra reference.
      //
      TR::Node *newReference = TR::Node::copy(originalReference);
      newReference->setReferenceCount((*referenceIterator)->getNumberOfParents());

      // Go through the parent list changing them to point to the new reference
      // node wherever they pointed to the original node
      //
      ListIterator<TR::Node> nodeIterator(&(*referenceIterator)->getParentList());
      TR::Node *nodeCursor = nodeIterator.getFirst();
      while (nodeCursor)
         {
         for (int32_t childCount = 0;
              childCount < nodeCursor->getNumChildren();
              ++childCount)
            {
            if (nodeCursor->getChild(childCount) == originalReference)
              {
              nodeCursor->setChild(childCount, newReference);
              }
            }
         nodeCursor = nodeIterator.getNext();
         }

      if (tempSymRef)
         {
         // There is a spill temp
         // Store it before the GC point if necessary
         //
         if (storeSpillTemp)
            {
            // insert new TR::astore node into tree top list before the call
            TR::TreeTop::create(self()->comp(), insertionTree, TR::Node::createWithSymRef(TR::astore, 1, 1, newReference, tempSymRef));
            }

         // Rewrite the original reference as an aload of the temp.
         TR::Node::recreate(originalReference, TR::aload);
         originalReference->setNumChildren(0);
         originalReference->setSymbolReference(tempSymRef);
         }

      // The original reference is now only used by parents subsequent to the
      // GC safe point. Adjust its reference count.
      //
      originalReference->setReferenceCount(originalReference->getReferenceCount() - (*referenceIterator)->getNumberOfParents());

      // Reset its visit count so that it is processed again for live
      // references.
      //
      originalReference->setVisitCount(0);
      ++referenceIterator;
      }
   _liveReferenceList.clear();
   }

void
OMR::CodeGenerator::needSpillTemp(TR_LiveReference * cursor, TR::Node *parent, TR::TreeTop *treeTop)
   {
   // If the reference is a simple load and the load is not killed before the
   // first use after this GC point, there is no need for a spill temp, the
   // original load can just be duplicated at the first use after this point.
   //
   if (cursor->needSpillTemp())
      return; // Reference already needs a spill temp

   TR::Node *node = cursor->getReferenceNode();
   if (node->getOpCodeValue() != TR::aload)
      {
      cursor->setNeedSpillTemp(true);
      return; // Reference is not an aload
      }

   cursor->setNeedSpillTemp(true);
   }

bool
OMR::CodeGenerator::allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *, TR::Node * branchNode)
   {
   return !(branchNode->getOpCode().isJumpWithMultipleTargets()) || debug("enableSwitch");
   }

///////////////////////////////////////////////////
//
// Register pressure simulation
//

void
OMR::CodeGenerator::initializeRegisterPressureSimulator()
   {
   int32_t numBlocks = self()->comp()->getFlowGraph()->getNextNodeNumber();
   _simulatedNodeStates = new (self()->trStackMemory()) TR_SimulatedNodeState[self()->comp()->getNodeCount()];

   _blockRegisterPressureCache = new (self()->trStackMemory()) TR_RegisterPressureSummary[numBlocks];

#if defined(CACHE_CANDIDATE_SPECIFIC_RESULTS)
   int32_t tagsSize = numBlocks * sizeof(_blockAndCandidateRegisterPressureCacheTags[0]);
   _blockAndCandidateRegisterPressureCache = new (trStackMemory()) TR_RegisterPressureSummary[numBlocks];
   _blockAndCandidateRegisterPressureCacheTags = (int32_t*)trMemory()->allocateStackMemory(tagsSize);
   memset(_blockAndCandidateRegisterPressureCacheTags, -1, tagsSize);
#endif
   }

bool
OMR::CodeGenerator::prepareForGRA()
   {
   TR_ASSERT(self()->getSupportsGlRegDeps(), "assertion failure");
   if (self()->getDebug() && self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
      {
      self()->getDebug()->dumpGlobalRegisterTable();
      }
   return true;
   }

static bool blockIsMuchColderThanContainingLoop(TR::Block *block, TR::CodeGenerator *cg)
   {
   if (cg->comp()->getMethodHotness() <= warm)
      {
      static const char * b = feGetEnv("TR_RegSimBlockFreqCutoff");
      int32_t regSimBlockFreqCutoff = b ? atoi(b) : 1000;
      if (block->getFrequency() < regSimBlockFreqCutoff)
         {
         if (cg->traceSimulateTreeEvaluation())
            traceMsg(cg->comp(), "            Block %d is not hot enough for simulation (%d)\n", block->getNumber(), block->getFrequency());

         return true;
         }
      }

   TR_BlockStructure *bs = block->getStructureOf();
   if (!bs)
      return false;

   TR_Structure *loopStructure = bs->getContainingLoop();
   if (!loopStructure)
      return false;

   int16_t blockFrequency = block->getFrequency();
   int16_t loopFrequency  = loopStructure->getEntryBlock()->getFrequency();

   // "<" excludes situations where some static heuristic uses a factor of 10
   // twice for whatever reason.  We want to be really confident that this
   // block is much colder than the loop.
   //
   bool result = blockFrequency < loopFrequency/100;
   if (result && cg->traceSimulateTreeEvaluation())
      traceMsg(cg->comp(), "            Block %d is much colder than containing loop (%d << %d)\n", block->getNumber(), blockFrequency, loopFrequency);
   return result;
   }

static bool blockIsIgnorablyCold(TR::Block *block, TR::CodeGenerator *cg)
   {
   if (block->isCold() && cg->traceSimulateTreeEvaluation())
      traceMsg(cg->comp(), "            Block %d is cold\n", block->getNumber());
   return (block->isCold() || blockIsMuchColderThanContainingLoop(block, cg));
   }

void
OMR::CodeGenerator::TR_RegisterPressureState::updateRegisterPressure(TR::Symbol *symbol)
   {
   TR::DataType dt = TR::NoType;

   TR::CodeGenerator *cg = TR::comp()->cg();

   if (symbol->getType().isAggregate())
      {
      dt = cg->getDataTypeFromSymbolMap(symbol);
      traceMsg(TR::comp(), "\nxxx2, rcSymbol %p is aggregate but found better dt = %s\n",symbol,dt.toString());
      }

   if (dt == TR::NoType)
      dt = symbol->getDataType();

   _gprPressure += cg->gprCount(TR::DataType(dt),symbol->getSize());
   _fprPressure += cg->fprCount(TR::DataType(dt));
   _vrfPressure += cg->vrfCount(TR::DataType(dt));


   }


TR_GlobalRegisterNumber
OMR::CodeGenerator::pickRegister(TR_RegisterCandidate     *rc,
                               TR::Block               * *allBlocks,
                               TR_BitVector            & availableRegisters,
                               TR_GlobalRegisterNumber & highRegisterNumber,
                               TR_LinkHead<TR_RegisterCandidate> *candidatesAlreadyAssigned)
   {
   static volatile bool isInitialized=false;
   static volatile uint8_t gprsWithheldFromPickRegister=0, fprsWithheldFromPickRegister=0, vrfWithheldFromPickRegister=0, gprsWithheldFromPickRegisterWhenWarm=0;
   int32_t currentCandidateWeight =-1;
   int32_t maxCandidateWeight =-1;
   if (!isInitialized)
      {
      static char *rwgpr = feGetEnv("TR_gprsWithheldFromPickRegister");
      static char *rwgprWarm = feGetEnv("TR_gprsWithheldFromPickRegisterWhenWarm");
      static char *rwfpr = feGetEnv("TR_fprsWithheldFromPickRegister");
      if (rwgpr)
         {
         gprsWithheldFromPickRegister = atoi(rwgpr);
         gprsWithheldFromPickRegisterWhenWarm = gprsWithheldFromPickRegister;
         }
      if (rwgprWarm)
         {
         gprsWithheldFromPickRegisterWhenWarm = atoi(rwgprWarm);
         }
      if (rwfpr)
         {
         fprsWithheldFromPickRegister = atoi(rwfpr);
         }
      isInitialized = true;
      }

   if (self()->comp()->getOption(TR_AssignEveryGlobalRegister))
      {
      // Trivial fixed-sequence algorithm

      TR_BitVectorIterator bvi(availableRegisters);
      if (bvi.hasMoreElements())
         {
         TR_GlobalRegisterNumber lowRegisterNumber = bvi.getNextElement();

         if (self()->comp()->target().is32Bit() && rc->getDataType() == TR::Int64)
            highRegisterNumber = bvi.getNextElement();
         else
            highRegisterNumber = -1;

         return lowRegisterNumber;
         }
      else
         {
         return -1;
         }

      TR_ASSERT(0, "Shouldn't get here");
      }

   if (!self()->comp()->getOption(TR_DisableRegisterPressureSimulation))
      {
      // Register pressure simulation algorithm

      TR::Symbol *rcSymbol = rc->getSymbolReference()->getSymbol();
      TR::DataType dtype = rc->getDataType();

      const bool usesFPR = (dtype == TR::Float
                           || dtype == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                           || dtype == TR::DecimalFloat
                           || dtype == TR::DecimalDouble
                           || dtype == TR::DecimalLongDouble
#endif
                           );

      const bool usesVRF = dtype.isVector();

      if (self()->terseSimulateTreeEvaluation())
         {
         traceMsg(self()->comp(), "         { Picking register for %s%s candidate #%d %s\n",
            highRegisterNumber? "high word of " : "",
            usesFPR? "FPR" : (usesVRF ? "VRF" : "GPR"), rc->getSymbolReference()->getReferenceNumber(), self()->getDebug()->getName(rc->getSymbolReference()));
         traceMsg(self()->comp(), "            Available regs: ");
         self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &availableRegisters);
         traceMsg(self()->comp(), "\n");
         if (debug("dumpAllSpillKinds"))
            {
            for (int32_t i = 0; i < TR_numSpillKinds; i++)
               {
               TR_SpillKinds sk = (TR_SpillKinds)i;
               traceMsg(self()->comp(), "            %s regs: ", self()->getDebug()->getSpillKindName(sk));
               self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), self()->getGlobalRegisters(sk, TR_Private));
               traceMsg(self()->comp(), "\n");
               }
            }
         }

      // Ensure that cache and node state array exist
      //
      if (!_blockRegisterPressureCache)
         {
         // Note that this assumes no new blocks or nodes are added between calls to pickRegister
         self()->initializeRegisterPressureSimulator();
         TR_ASSERT(_blockRegisterPressureCache && _simulatedNodeStates, "assertion failure");
         }

      TR_BitVector remainingRegisters = availableRegisters;
      TR_BitVector *spilledRegisters = self()->getGlobalRegisters(TR_vmThreadSpill, self()->comp()->getMethodSymbol()->getLinkageConvention());
      if (spilledRegisters)
         {
         if (self()->traceSimulateTreeEvaluation())
            {
            TR_BitVector regsToPrint = remainingRegisters;
            regsToPrint &= *spilledRegisters;
            traceMsg(self()->comp(), "            vmThread register not enabled; rejected: ");
            self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &regsToPrint);
            traceMsg(self()->comp(), "\n");
            }
         remainingRegisters -= *spilledRegisters;
         }

      // 1. Simulate register pressure in each extended basic block.
      //
      TR_RegisterPressureSummary highWaterMark(0, 0, 0);
      const bool canExitEarly = !rc->canBeReprioritized();
      TR_BitVector alreadyAssignedOnEntry(self()->comp()->getSymRefCount(), self()->trMemory(), stackAlloc, growable);
      TR_BitVector alreadyAssignedOnExit(self()->comp()->getSymRefCount(), self()->trMemory(), stackAlloc, growable);

      TR_BitVector blocksToVisit = rc->getBlocksLiveOnEntry();
      blocksToVisit |= rc->getBlocksLiveOnExit();

      if (self()->traceSimulateTreeEvaluation())
         {
         traceMsg(self()->comp(), "                Blocks to visit: ");
         self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &blocksToVisit);
         traceMsg(self()->comp(), "\n");
         }

      TR_BitVectorIterator blockIterator(blocksToVisit);
      while (blockIterator.hasMoreElements())
         {
         TR::Block *block = allBlocks[blockIterator.getNextElement()];
         blocksToVisit.set(block->startOfExtendedBlock()->getNumber());
         }

      if (self()->terseSimulateTreeEvaluation())
         {
         traceMsg(self()->comp(), "            Ext blocks to visit: ");
         self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &blocksToVisit);
         traceMsg(self()->comp(), "\n");
         }

      blockIterator = TR_BitVectorIterator(blocksToVisit);
      while (blockIterator.hasMoreElements())
         {
         if (canExitEarly && remainingRegisters.isEmpty())
            break;

         TR::Block *block = allBlocks[blockIterator.getNextElement()];

         if (!block->isExtensionOfPreviousBlock() && !blockIsIgnorablyCold(block, self()))
            {
            // This block starts an extended BB.

            // 1a. Compute which symbol references are already assigned to
            // global regs in this block.
            //
            static int32_t meniscus = feGetEnv("TR_registerPressureMeniscus")? atoi(feGetEnv("TR_registerPressureMeniscus")) : 0;
            alreadyAssignedOnEntry.empty();
            alreadyAssignedOnExit.empty();

            int32_t gprDelta = 0;
            int32_t fprDelta = 0;
            int32_t vrfDelta = 0;

            TR_RegisterPressureState blockEntryState(NULL, false, alreadyAssignedOnEntry, alreadyAssignedOnExit, candidatesAlreadyAssigned,
                                                     self()->getNumberOfGlobalGPRs()+meniscus-gprDelta,
                                                     self()->getNumberOfGlobalFPRs()+meniscus-fprDelta,
                                                     self()->getNumberOfGlobalVRFs()+meniscus-vrfDelta,
                                                     0);
            for (TR_RegisterCandidate *candidate = candidatesAlreadyAssigned->getFirst(); candidate; candidate = candidate->getNext())
               {
               if (candidate->getBlocksLiveOnEntry().get(block->getNumber()))
                  {
                  // Previously-assigned candidate is live on entry to this block
                  //
                  TR::SymbolReference *symRef = candidate->getSymbolReference();
                  alreadyAssignedOnEntry.set(symRef->getReferenceNumber());

                  TR::Symbol *candidateSymbol  = candidate->getSymbolReference()->getSymbol();

                  blockEntryState.updateRegisterPressure(candidateSymbol);

                  }
               if (candidate->getBlocksLiveOnExit().get(block->getNumber()))
                  {
                  // Previously-assigned candidate is live on exit from this block
                  //
                  TR::SymbolReference *symRef = candidate->getSymbolReference();
                  alreadyAssignedOnExit.set(symRef->getReferenceNumber());
                  }
               }

            if (highRegisterNumber)
               {
               // Add 1 because we've already assigned the low half
               if (usesFPR)
                  blockEntryState._fprPressure++;
               else if (usesVRF)
                  blockEntryState._vrfPressure++;
               else
                  blockEntryState._gprPressure++;
               }

            // 1b. Refresh the register pressure summary cache if necessary.
            //
            TR_RegisterPressureSummary *cachedSummary = &_blockRegisterPressureCache[block->getNumber()];

            // Live on exit info is not useful for cacheable simulations, so don't do
            // any for now.  In reality, we'll need to consider exactly when we want to consider
            // the cache unusable.  Presumably we could attempt a cacheable simulation of each
            // block, and if the block is "uncacheable" we could just not use the cache for that one.
            //
            // TODO: support candidate-agnostic simulations with live-on-exit info
            //
            bool cacheIsInconclusive = true;
            bool canAffordFullSimulation = true;

            if (cacheIsInconclusive)
               {
               if (canAffordFullSimulation)
                  {
                  TR_RegisterPressureState state(blockEntryState, rc, rc->getBlocksLiveOnEntry().isSet(block->getNumber()), self()->comp()->incVisitCount());
                  if (rc->getBlocksLiveOnEntry().isSet(block->getNumber()))
                     {
                     state.updateRegisterPressure(rcSymbol);
                     }

                  // Perform the simulation for current block and accumulate into highWaterMark
                  //
                  TR_RegisterPressureSummary summary(state._gprPressure, state._fprPressure, state._vrfPressure);
                  self()->simulateBlockEvaluation(block, &state, &summary);
                  highWaterMark.accumulate(summary);

#if defined(CACHE_CANDIDATE_SPECIFIC_RESULTS)
                  // Save a copy in the candidate-specific cache
                  //
                  _blockAndCandidateRegisterPressureCache    [block->getNumber()] = summary;
                  _blockAndCandidateRegisterPressureCacheTags[block->getNumber()] = rc->getSymbolReference()->getReferenceNumber();
#endif
                  // Save this info in case we need to trim the live range
                  //
                  _blockRegisterPressureCache[block->getNumber()] = summary;
                  if (self()->traceSimulateTreeEvaluation())
                     traceMsg(self()->comp(), "            saved summary for block_%d\n", block->getNumber());
                  }
               else
                  {
                  // Can't afford a full simulation; assume GRA is no longer profitable.
                  //
                  if (self()->terseSimulateTreeEvaluation())
                     traceMsg(self()->comp(), "         } can't afford full simulation -- exiting\n");

                  return -1;
                  }
               }
            else
               {
               // Cache is valid, so use it.
               //
               if (self()->traceSimulateTreeEvaluation())
                  {
                  traceMsg(self()->comp(), "            using cache for block_%d -- g=%d, f=%d, v=%d",
                     block->getNumber(), cachedSummary->_gprPressure, cachedSummary->_fprPressure, cachedSummary->_vrfPressure);
                  cachedSummary->dumpSpillMask(self());
                  traceMsg(self()->comp(), "\n");
                  }
               highWaterMark.accumulate(*cachedSummary);
               }

            // 1d. Eliminate from consideration any regs whose usage may cause a spill.
            // If early exits are allowed, do this on each iteration.
            //
            if (canExitEarly)
               {
               TR_BitVector spilledRegs(self()->getNumberOfGlobalRegisters(), self()->trMemory());
               self()->computeSpilledRegsForAllPresentLinkages(&spilledRegs, highWaterMark);
               remainingRegisters -= spilledRegs;
               if (self()->traceSimulateTreeEvaluation())
                  {
                  traceMsg(self()->comp(), "            rejected registers: ");
                  self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &spilledRegs);
                  highWaterMark.dumpSpillMask(self());
                  traceMsg(self()->comp(), "\n            remaining registers: ");
                  self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &remainingRegisters);
                  traceMsg(self()->comp(), "\n");
                  }
               }
            }
         }

      // 1e. Eliminate from consideration any regs whose usage may cause a spill.
      // If early exits are not allowed, just let the loop finish and do this
      // once at the end.
      //
      if (!canExitEarly)
         {
         TR_BitVector spilledRegs(self()->getNumberOfGlobalRegisters(), self()->trMemory());
         self()->computeSpilledRegsForAllPresentLinkages(&spilledRegs, highWaterMark);
         remainingRegisters -= spilledRegs;
         if (self()->traceSimulateTreeEvaluation())
            {
            traceMsg(self()->comp(), "            rejected registers: ");
            self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &spilledRegs);
            highWaterMark.dumpSpillMask(self());
            traceMsg(self()->comp(), "\n            remaining registers: ");
            self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &remainingRegisters);
            traceMsg(self()->comp(), "\n");
            }
         }

      // (At this point, highWaterMark now gives a pessimistic register
      // pressure and spill estimate for this candidate's live range.)

      // 2. Coalescing:
      // If candidate is involved in a copy that also involves an
      // already-assigned candidate, and that other candidate's register is
      // free, then use it.
      //
      TR_GlobalRegisterNumber hottestCopiedRegister       = -1;
      int32_t                 hottestCopiedRegisterWeight = -1;
      TR_BitVector unpreferredRegisters(self()->getNumberOfGlobalRegisters(), self()->trMemory());

      TR_BitVector visitedNodesForCandidateUse(0, self()->trMemory(), stackAlloc, growable);
      TR_BitVector liveOnEntryOrExit(rc->getBlocksLiveOnEntry());
      liveOnEntryOrExit |= rc->getBlocksLiveOnExit();
      blockIterator = TR_BitVectorIterator(liveOnEntryOrExit);
      while (blockIterator.hasMoreElements())
         {
         TR::Block *block = allBlocks[blockIterator.getNextElement()];
         if (block != self()->comp()->getFlowGraph()->getEnd() && !blockIsIgnorablyCold(block, self()))
            {
            // Only scan for copies if this block is hotter than the hottest known copy
            //
            int32_t blockWeight = 1; // Note that we use a static loop-nesting-depth heuristic here for consistency with the rest of GRA
            if (block->getStructureOf())
               block->getStructureOf()->calculateFrequencyOfExecution(&blockWeight);
            if (blockWeight > hottestCopiedRegisterWeight)
               {
               if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
                  traceMsg(self()->comp(), "            scanning block_%d for copies\n", block->getNumber());

               // Scan for copies
               //
               TR::Block   *stopBlock = block->getNextExtendedBlock();
               TR::TreeTop *stopTree  = stopBlock? stopBlock->getEntry() : NULL;
               for (TR::TreeTop *tt = block->getEntry(); tt != stopTree; tt = tt->getNextTreeTop())
                  {
                  TR::Node *node = tt->getNode();
                  bool isUnpreferred;
                  TR_RegisterCandidate *candidate = self()->findCoalescenceForRegisterCopy(node, rc, &isUnpreferred);

                  if (candidate)
                     {
                     if (!isUnpreferred)
                        {
                        TR_GlobalRegisterNumber preferredRegister;
                        if (highRegisterNumber)
                           preferredRegister = candidate->getHighGlobalRegisterNumber();
                        else
                           preferredRegister = candidate->getGlobalRegisterNumber();
                        if (remainingRegisters.isSet(preferredRegister))
                           {
                           hottestCopiedRegister       = preferredRegister;
                           hottestCopiedRegisterWeight = blockWeight;
                           if (self()->traceSimulateTreeEvaluation())
                              {
                              traceMsg(self()->comp(), "            found copy to/from #%d, register %d (%s), weight %d, at %s\n",
                                      candidate->getSymbolReference()->getReferenceNumber(),
                                      hottestCopiedRegister, self()->getDebug()->getGlobalRegisterName(hottestCopiedRegister),
                                      hottestCopiedRegisterWeight, self()->getDebug()->getName(node));
                              }
                           }
                        }
                     else // is Unpreferred
                        {
                        if (self()->traceSimulateTreeEvaluation())
                           {
                           traceMsg(self()->comp(), "            unprefer copy to/from #%d, register %d (%s), weight %d, at %s\n",
                                   candidate->getSymbolReference()->getReferenceNumber(),
                                   candidate->getGlobalRegisterNumber(), self()->getDebug()->getGlobalRegisterName(candidate->getGlobalRegisterNumber()),
                                   blockWeight, self()->getDebug()->getName(node));
                           }
                        unpreferredRegisters.set(candidate->getGlobalRegisterNumber());
                        if (candidate->getHighGlobalRegisterNumber() != -1)
                           unpreferredRegisters.set(candidate->getHighGlobalRegisterNumber());
                        }
                     }

                  TR::Node *callNode = NULL;
                  if ((node->getOpCodeValue() == TR::treetop) && (node->getFirstChild()->getOpCode().isCall()))
                     {
                     callNode = node->getFirstChild();
                     for (uint32_t i = 0; i < callNode->getNumChildren(); i++)
                        {
                        bool isUnpreferred;
                        TR_GlobalRegisterNumber candidateRegister = self()->findCoalescenceRegisterForParameter(callNode, rc, i, &isUnpreferred);
                        if (candidateRegister != -1)
                           {
                           if (!isUnpreferred)
                              {
                              if (remainingRegisters.isSet(candidateRegister))
                                 {
                                 hottestCopiedRegister       = candidateRegister;
                                 hottestCopiedRegisterWeight = blockWeight;
                                 if (self()->traceSimulateTreeEvaluation())
                                    {
                                    traceMsg(self()->comp(), "            found call parameter index %i, register %d (%s), weight %d, at %s\n",
                                            i,
                                            hottestCopiedRegister, self()->getDebug()->getGlobalRegisterName(hottestCopiedRegister),
                                            hottestCopiedRegisterWeight, self()->getDebug()->getName(callNode));
                                    }
                                 break;
                                 }
                              }
                           else // candidate is unpreferred
                              {
                              if (self()->traceSimulateTreeEvaluation())
                                 {
                                 traceMsg(self()->comp(), "            unprefer call parameter index %i, register %d (%s), weight %d, at %s\n",
                                         i,
                                         candidateRegister, self()->getDebug()->getGlobalRegisterName(candidateRegister),
                                         blockWeight, self()->getDebug()->getName(callNode));
                                 }
                              unpreferredRegisters.set(candidateRegister);
                              }
                           }
                        }
                     }

                  if (!candidate)
                     {
                     TR_RegisterCandidate *candidate = self()->findUsedCandidate(node, rc, &visitedNodesForCandidateUse);
                     if (candidate)
                        {
                        if (self()->traceSimulateTreeEvaluation())
                           {
                           traceMsg(self()->comp(), "            unprefer used candidate #%d, register %d (%s), weight %d, at %s\n",
                                   candidate->getSymbolReference()->getReferenceNumber(),
                                   candidate->getGlobalRegisterNumber(), self()->getDebug()->getGlobalRegisterName(candidate->getGlobalRegisterNumber()),
                                   blockWeight, self()->getDebug()->getName(node));
                           }
                        unpreferredRegisters.set(candidate->getGlobalRegisterNumber());
                        if (candidate->getHighGlobalRegisterNumber() != -1)
                           unpreferredRegisters.set(candidate->getHighGlobalRegisterNumber());
                        }
                     }
                  }
               }
            }
         }

      if (hottestCopiedRegister == -1)
         {
         // Found no preferred register.  If we can avoid the unpreferred registers, do so.
         //
         unpreferredRegisters &= remainingRegisters;
         if (unpreferredRegisters != remainingRegisters)
            {
            if (self()->traceSimulateTreeEvaluation() && !unpreferredRegisters.isEmpty())
               {
               traceMsg(self()->comp(), "            Rejecting copy registers: ");
               self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &unpreferredRegisters);
               traceMsg(self()->comp(), "\n");
               }
            remainingRegisters -= unpreferredRegisters;
            }
         }
      else
         {
         TR_ASSERT(remainingRegisters.isSet(hottestCopiedRegister), "assertion failure");
         if (self()->traceSimulateTreeEvaluation())
            traceMsg(self()->comp(), "            Using copy register %d\n", hottestCopiedRegister);

         // Eliminate all other registers from consideration
         //
         remainingRegisters.empty();
         remainingRegisters.set(hottestCopiedRegister);
         }

      // 3. If the candidate arrived via a linkage register, and that register
      // is a reasonable choice, use it.
      //
      TR_GlobalRegisterNumber linkageRegister = -1;
      if (rcSymbol->isParm())
         {
         int8_t lri = rcSymbol->getParmSymbol()->getLinkageRegisterIndex();
         if (lri >= 0)
            {
            linkageRegister = self()->getLinkageGlobalRegisterNumber(lri, dtype);
            }
         }
      if (linkageRegister == -1)
         {
         // Not a parameter.  If we can avoid linkage registers, do so.
         //
         unpreferredRegisters = *self()->getGlobalRegisters(TR_linkageSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());

         unpreferredRegisters &= remainingRegisters;
         if (unpreferredRegisters != remainingRegisters)
            {
            if (self()->traceSimulateTreeEvaluation() && !unpreferredRegisters.isEmpty())
               {
               traceMsg(self()->comp(), "            Rejecting linkage registers: ");
               self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &unpreferredRegisters);
               traceMsg(self()->comp(), "\n");
               }
            remainingRegisters -= unpreferredRegisters;
            }
         }
      else
         {
         if (remainingRegisters.isSet(linkageRegister))
            {
            if (self()->traceSimulateTreeEvaluation())
               traceMsg(self()->comp(), "            Using linkage register %d\n", linkageRegister);

            // Eliminate all other registers from consideration
            //
            remainingRegisters.empty();
            remainingRegisters.set(linkageRegister);
            }
         }

      if (self()->traceSimulateTreeEvaluation())
         {
         traceMsg(self()->comp(), "            final registers: ");
         self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &remainingRegisters);
         traceMsg(self()->comp(), "\n");
         }

      // 4. Return the first remaining reg, if any.
      //
      if (remainingRegisters.isEmpty())
         {
         if (self()->terseSimulateTreeEvaluation())
            traceMsg(self()->comp(), "         } No good registers for candidate #%d after reg pressure simulation\n", rc->getSymbolReference()->getReferenceNumber());
         else
            dumpOptDetails(self()->comp(), "No good registers for candidate #%d after reg pressure simulation\n", rc->getSymbolReference()->getReferenceNumber());

         return -1;
         }
      else
         {
         // Pick the first remaining register, on the assumption that the
         // platform code has sorted them in descending order of preference.
         //
         TR_GlobalRegisterNumber result = TR_BitVectorIterator(remainingRegisters).getFirstElement();

         if (self()->terseSimulateTreeEvaluation())
            traceMsg(self()->comp(), "         } Picked register %d (%s) for candidate #%d %s\n", result, self()->getDebug()->getGlobalRegisterName(result), rc->getSymbolReference()->getReferenceNumber(), self()->getDebug()->getName(rc->getSymbolReference()));

         // Bump the register pressure in the cache for each block in the
         // candidate's live range.
         //
         TR_BitVectorIterator blockIterator(rc->getBlocksLiveOnEntry());
         while (blockIterator.hasMoreElements() && !availableRegisters.isEmpty()) // TODO: Can availableRegisters possibly be empty here?
            {
            TR::Block *block = allBlocks[blockIterator.getNextElement()];
            if (!block->isExtensionOfPreviousBlock())
               {
               TR_RegisterPressureSummary *cachedSummary = &_blockRegisterPressureCache[block->getNumber()];

               // Note that if cachedSummary->isSpilled is true, then further
               // simulation will tell us nothing, so we don't increment the
               // spill counter because (besides being pointless) it may cause
               // overflow in the counter.
               //
               if (usesFPR && !cachedSummary->isSpilled(TR_fprSpill))
                  {
                  ++cachedSummary->_fprPressure;
                  }
               else if (!cachedSummary->isSpilled(TR_gprSpill))
                  {
                  ++cachedSummary->_gprPressure;
                  }
               else if (usesVRF && !cachedSummary->isSpilled(TR_vrfSpill))
                  {
                  ++cachedSummary->_vrfPressure;
                  }
               }
            }

         return result;
         }
      TR_ASSERT(0, "Shouldn't get here");
      }

   // Older pressure-insensitive algorithm

   if(1)
      {
      TR::DataType dtype = rc->getDataType();
      uint8_t regsWithheld =
         (dtype == TR::Float
          || dtype == TR::Double
#ifdef J9_PROJECT_SPECIFIC
          || dtype == TR::DecimalFloat
          || dtype == TR::DecimalDouble
          || dtype == TR::DecimalLongDouble
#endif
          ) ?
         fprsWithheldFromPickRegister : (self()->comp()->getMethodHotness() == warm)? gprsWithheldFromPickRegisterWhenWarm :
         gprsWithheldFromPickRegister;

      if (availableRegisters.elementCount() <= regsWithheld)
         {
         if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
            {
            traceMsg(self()->comp(), "pickRegister: Withholding %d registers from candidate #%d\n", availableRegisters.elementCount(), rc->getSymbolReference()->getReferenceNumber());
            }
         return -1;
         }
      else
         {
         if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
            {
            traceMsg(self()->comp(),"pickRegister: %d registers still available for candidate #%d\n", availableRegisters.elementCount(), rc->getSymbolReference()->getReferenceNumber());
            }
         }

      TR_BitVectorIterator bvi;
      TR::Symbol            *rcSymbol = rc->getSymbolReference()->getSymbol();

      if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
         {
         traceMsg(self()->comp(), "pickRegister: Candidate #%d {\n", rc->getSymbolReference()->getReferenceNumber());
         }


      //////////////////////////////////////////
      //
      // Phase 1: Decide on preference order
      //

      // Does the candidate enter the method via a linkage register?
      //
      TR_GlobalRegisterNumber linkageRegister = -1;
      if (rcSymbol->isParm())
         {
         int8_t lri = rcSymbol->getParmSymbol()->getLinkageRegisterIndex();
         if (lri >= 0)
            {
            linkageRegister = self()->getLinkageGlobalRegisterNumber(lri, rcSymbol->getDataType());
            }
         }
      if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
         {
         if (linkageRegister == -1)
            {
            traceMsg(self()->comp(), "pickRegister:\tNo linkage register\n");
            }
         else
            {
            traceMsg(self()->comp(), "pickRegister:\tLinkage register #%d\n", linkageRegister);
            }
         }

      // Do we have information about which registers are preserved across calls?
      //
      TR_BitVector *preservedRegisters;
      dtype = rcSymbol->getDataType();
      if (dtype == TR::Float
          || dtype == TR::Double
#ifdef J9_PROJECT_SPECIFIC
          || dtype == TR::DecimalFloat
          || dtype == TR::DecimalDouble
          || dtype == TR::DecimalLongDouble
#endif
          )
         preservedRegisters = self()->getGlobalFPRsPreservedAcrossCalls();
      else
         preservedRegisters = self()->getGlobalGPRsPreservedAcrossCalls();
      if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
         {
         traceMsg(self()->comp(), "pickRegister:\tPreserved register info is %spresent\n", preservedRegisters? "" : "NOT ");
         }

      // Now choose the preference order string.
      //
      const char *preferences=NULL;
      if (preservedRegisters)
         {
         // Does the candidate live across a call in a non-cold block?
         //
         bool isLiveAcrossNonColdCall = false;
         if (preservedRegisters)
            {
            TR_BitVector bv(rc->getBlocksLiveOnEntry());
            bv &= *self()->getBlocksWithCalls();
            bvi.setBitVector(bv);
            while (!isLiveAcrossNonColdCall && bvi.hasMoreElements())
               {
               isLiveAcrossNonColdCall = !blockIsIgnorablyCold(allBlocks[bvi.getNextElement()], self());
               }
            }

         if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
            {
            traceMsg(self()->comp(), "pickRegister:\tRegister is %slive across non-cold call\n", isLiveAcrossNonColdCall? "" : "NOT ");
            }

         if (isLiveAcrossNonColdCall)
            {
            if (linkageRegister == -1)
               {
               preferences = "PN";
               }
            else
               {
               preferences = "PLO";
               }
            }
         else
            {
            if (linkageRegister == -1)
               {
               preferences = "NP";
               }
            else
               {
               preferences = "LOP";
               }
            }
         }
      else
         {
         if (linkageRegister == -1)
            {
            preferences = "M";
            }
         else
            {
            preferences = "LM";
            }
         }

      TR_ASSERT(preferences, "A register selection preference order must be chosen");
      if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
         {
         traceMsg(self()->comp(), "pickRegister:\tPreference order: %s\n", preferences);
         }


      //////////////////////////////////////////
      //
      // Phase 2: Pick based on preference order
      //

      // Loop through preferences and stop when we find one that can be satisfied
      // with an available register.
      //
      // (Note: the register preference letters are contiguous to produce a
      // nice, tight jump table.)
      //
      TR_GlobalRegisterNumber bestLowRegisterNumber;
      for(bestLowRegisterNumber=-1; bestLowRegisterNumber==-1 && preferences[0]; preferences++)
         {
         switch(preferences[0])
            {
            case 'L': // The linkage register carrying the candidate into the method
               TR_ASSERT(linkageRegister != -1, "Candidate must have a linkage register");

               if (availableRegisters.isSet(linkageRegister))
                  bestLowRegisterNumber = linkageRegister;
               break;

            case 'P': // Any preserved register
               TR_ASSERT(preservedRegisters, "Must have preservedRegisters info to select a preserved register");

               {
               TR_BitVector regs(availableRegisters); regs &= *preservedRegisters;
               bestLowRegisterNumber = self()->getFirstBit(regs);
               }
               break;

            case 'N': // Any non-preserved register
               TR_ASSERT(preservedRegisters, "Must have preservedRegisters info to select a non-preserved register");
               TR_ASSERT(linkageRegister == -1, "Candidates arriving in linkage regs can't be given other linkage regs");
               //
               // NOTE: If a parameter can't be given its own linkage register, we
               // don't want give it any linkage register at all.  (In fact, we
               // don't give it any volatile register.)  This avoids strange
               // situations that would complicate the prologue.  For example, if
               // parameters arrive in R1 and R2 but GRA assigns them to R2 and
               // R1, then the prologue would need to be smart enough to be able
               // to exchange the registers, and that's not worth the complexity.

               {
               TR_BitVector regs(availableRegisters); regs -= *preservedRegisters;
               bestLowRegisterNumber = self()->getFirstBit(regs);
               }
               break;

            default:
               TR_ASSERT(0, "Unknown register preference");
               // Continue with the next preference
               break;

            case 'O': // Any non-preserved non-linkage register ('O'ther volatile registers)
               // Not yet implemented
               // Continue with the next preference
               break;

            case 'M': // Any available register ('M'ost registers)
               bestLowRegisterNumber = self()->getFirstBit(availableRegisters);
               break;

            }
         }

      if (debug("tracePickRegister") || self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
         {
         if (bestLowRegisterNumber == -1)
            {
            traceMsg(self()->comp(), "pickRegister:\tNOT assigned a global register\n");
            }
         else
            {
            traceMsg(self()->comp(), "pickRegister:\tPicked global register #%d\n", bestLowRegisterNumber);
            }
         traceMsg(self()->comp(), "pickRegister: }\n");
         }
      return bestLowRegisterNumber;
      }
   TR_ASSERT(0, "pickRegister: unexpected fall-through");
   return 0; // eliminate warning
   }

TR_RegisterCandidate *
OMR::CodeGenerator::findCoalescenceForRegisterCopy(TR::Node *node, TR_RegisterCandidate *rc, bool *isUnpreferred)
   {
   TR_RegisterCandidate *candidate = NULL;
   if (node->getOpCode().isStoreDirect() && node->getFirstChild()->getOpCode().isLoadVarDirect())
      {
      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(self()->comp(), "            found copy %s\n", self()->getDebug()->getName(node));

      TR_RegisterCandidate *storedCand = self()->comp()->getGlobalRegisterCandidates()->find(node->getSymbolReference());

      if (storedCand)
         {
         int32_t loadedSymRefNum = node->getFirstChild()->getSymbolReference()->getReferenceNumber();
         // unpreferred if the candidate for coalescing does not match the candidate rc, currently being evaluated
         *isUnpreferred = !(loadedSymRefNum == rc->getSymbolReference()->getReferenceNumber());
         candidate = storedCand;
         }

      TR_RegisterCandidate *loadedCand = self()->comp()->getGlobalRegisterCandidates()->find(node->getFirstChild()->getSymbolReference());
      if (loadedCand)
         {
         int32_t storedSymRefNum = node->getSymbolReference()->getReferenceNumber();
         // unpreferred if the candidate for coalescing does not match the candidate rc, currently being evaluated
         *isUnpreferred = !(storedSymRefNum == rc->getSymbolReference()->getReferenceNumber());
         candidate = loadedCand;
         }
      }
      return candidate;
   }

TR_GlobalRegisterNumber
OMR::CodeGenerator::findCoalescenceRegisterForParameter(TR::Node *callNode, TR_RegisterCandidate *rc, uint32_t childIndex, bool *isUnpreferred)
   {
   TR::Node *paramNode = callNode->getChild(childIndex);
   if (paramNode->getOpCode().isLoadVarDirect())
      {
      int32_t paramSymRefNum = paramNode->getSymbolReference()->getReferenceNumber();
      *isUnpreferred = !(paramSymRefNum == rc->getSymbolReference()->getReferenceNumber());
      }
   return -1;
   }


TR_RegisterCandidate *
OMR::CodeGenerator::findUsedCandidate(TR::Node *node, TR_RegisterCandidate *rc, TR_BitVector *visitedNodes)
   {
   if (visitedNodes->isSet(node->getGlobalIndex()))
      return NULL;
   else
      visitedNodes->set(node->getGlobalIndex());

   TR_RegisterCandidate *gprToCoalesce = NULL;
   if (node->getOpCode().isLoadVarDirect() || node->getOpCode().isStoreDirect())
      gprToCoalesce = self()->comp()->getGlobalRegisterCandidates()->find(node->getSymbolReference());

   for (int32_t i = 0; (gprToCoalesce == NULL) && (i < node->getNumChildren()); i++)
      gprToCoalesce = self()->findUsedCandidate(node->getChild(i), rc, visitedNodes);

   return gprToCoalesce;
   }


// For tuning experimentation
//
#define MENISCUS meniscus

// Ideally, we should be able to know which spills are caused by call nodes
// (therefore using the callee's linkage), and which should use the caller's
// linkage.  For now, we just always include the linkages of the caller and
// all callees.
//
#define LINKAGE_THAT_IS_ALWAYS_PRESENT (self()->comp()->getMethodSymbol()->getLinkageConvention())

void
OMR::CodeGenerator::computeSimulatedSpilledRegs(TR_BitVector *spilledRegisters, int32_t blockNum, TR::SymbolReference *candidate)
   {
   TR_ASSERT(self()->hasRegisterPressureInfo(), "assertion failure");
#if defined(CACHE_CANDIDATE_SPECIFIC_RESULTS)
   int32_t symRefNum = candidate->getReferenceNumber();
   if (symRefNum == _blockAndCandidateRegisterPressureCacheTags[blockNum])
      {
      if (traceSimulateTreeEvaluation())
         traceMsg(comp(), "Using candidate-specific regsiter pressure simulation cache for block_%d, sym #%d\n", blockNum, symRefNum);
      computeSpilledRegsForAllPresentLinkages(spilledRegisters, _blockAndCandidateRegisterPressureCache[blockNum]);
      }
   else
#endif
      {
      self()->computeSpilledRegsForAllPresentLinkages(spilledRegisters, _blockRegisterPressureCache[blockNum]);
      }
   }

void
OMR::CodeGenerator::computeSpilledRegsForAllPresentLinkages(TR_BitVector *spilledRegisters, TR_RegisterPressureSummary &summary)
   {
   for (int32_t i = 0; i < TR_numProbableSpillKinds; i++)
      {
      TR_SpillKinds spillKind = (TR_SpillKinds)i;
      if (summary.isSpilled(spillKind))
         self()->setSpilledRegsForAllPresentLinkages(spilledRegisters, summary, spillKind);
      }
   }

void
OMR::CodeGenerator::setSpilledRegsForAllPresentLinkages(TR_BitVector *spilledRegisters, TR_RegisterPressureSummary &summary, TR_SpillKinds spillKind)
   {
   for (int32_t i=0; i < TR_NumLinkages; i++)
      {
      TR_LinkageConventions lc = (TR_LinkageConventions)i;
      if (summary.isLinkagePresent(lc) || lc == LINKAGE_THAT_IS_ALWAYS_PRESENT)
         {
         TR_BitVector *regs = self()->getGlobalRegisters(spillKind, lc);
         if (regs)
            *spilledRegisters |= *regs;
         }
      }
   }


TR::SymbolReference *OMR::CodeGenerator::TR_RegisterPressureState::getCandidateSymRef(){ return _candidate? _candidate->getSymbolReference() : NULL; }

void OMR::CodeGenerator::TR_RegisterPressureState::addVirtualRegister(TR::Register *reg)
  {
  if (reg == NULL) return;
  if (reg->getRealRegister()) return;

  if (reg->getTotalUseCount() == reg->getFutureUseCount())
     {
     // first time we see this register
     if (reg->getKind() == TR_GPR)
        _gprPressure++;
     else if (reg->getKind() == TR_FPR)
        _fprPressure++;
     else if (reg->getKind() == TR_VRF)
        _vrfPressure++;
     }
  }

void OMR::CodeGenerator::TR_RegisterPressureState::removeVirtualRegister(TR::Register *reg)
  {
  if (reg == NULL) return;
  if (reg->getRealRegister()) return;

  reg->decFutureUseCount();
  if (reg->getFutureUseCount() == 0)
     {
     // register goes dead
     if (reg->getKind() == TR_GPR)
        _gprPressure--;
     else if (reg->getKind() == TR_FPR)
        _fprPressure--;
     else if (reg->getKind() == TR_VRF)
        _vrfPressure--;
    }
  }

bool OMR::CodeGenerator::TR_RegisterPressureState::isInitialized(TR::Node *node){ return node->getVisitCount() == _visitCountForInit; }


void OMR::CodeGenerator::TR_RegisterPressureSummary::setLinkagePresent(TR_LinkageConventions lc, TR::CodeGenerator *cg)
   {
   _linkageConventionMask |= (1<<lc);
   if (cg->traceSimulateTreeEvaluation())
      {
      // Call setLinkagePresent() after calling spill() to make the
      // traces look nice (like "!volatile.private").
      traceMsg(cg->comp(), ".%s", cg->getDebug()->getLinkageConventionName(lc));
      }
   }

void OMR::CodeGenerator::TR_RegisterPressureSummary::spill(TR_SpillKinds kind, TR::CodeGenerator *cg)
   {
   _spillMask |= (1 << kind);
   if (cg->traceSimulateTreeEvaluation())
      traceMsg(cg->comp(), " !%s", cg->getDebug()->getSpillKindName(kind));
   }

void OMR::CodeGenerator::TR_RegisterPressureSummary::dumpSpillMask(TR::CodeGenerator *cg)
   {
   if (cg->traceSimulateTreeEvaluation())
      {
      for (int32_t i = 0; i < TR_numSpillKinds; i++)
         {
         TR_SpillKinds spillKind = (TR_SpillKinds)i;
         if (isSpilled(spillKind))
            traceMsg(cg->comp(), " %s", cg->getDebug()->getSpillKindName(spillKind));
         }
      }
   }

void OMR::CodeGenerator::TR_RegisterPressureSummary::dumpLinkageConventionMask(TR::CodeGenerator *cg)
   {
   if (cg->traceSimulateTreeEvaluation())
      {
      for (int32_t i = 0; i < TR_NumLinkages; i++)
         {
         TR_LinkageConventions lc = (TR_LinkageConventions)i;
         if (isLinkagePresent(lc))
            traceMsg(cg->comp(), " %s", cg->getDebug()->getLinkageConventionName(lc));
         }
      }
   }

bool OMR::CodeGenerator::isCandidateLoad(TR::Node *node,  TR::SymbolReference *candidate)
   {
   return node->getOpCode().isLoadVarDirect() && node->getSymbolReference() == candidate;
   }

bool OMR::CodeGenerator::isCandidateLoad(TR::Node *node, TR_RegisterPressureState *state) { return state->_candidate? self()->isCandidateLoad(node, state->_candidate->getSymbolReference()) : false; }

bool OMR::CodeGenerator::isLoadAlreadyAssignedOnEntry(TR::Node *node,  TR_RegisterPressureState *state)
   {
   return node->getOpCode().isLoadVarDirect() &&
   (state->_alreadyAssignedOnEntry.isSet(node->getSymbolReference()->getReferenceNumber()));
   }

uint8_t OMR::CodeGenerator::gprCount(TR::DataType type,int32_t size)
   {
   if (type.isAggregate())
      return (self()->comp()->target().is32Bit() && !self()->use64BitRegsOn32Bit() && size > 4 && size <=  8) ? 2 : 1;

   return (type.isInt64() && self()->comp()->target().is32Bit() && !self()->use64BitRegsOn32Bit())? 2 : (type.isIntegral() || type.isAddress())? 1 : 0;
   }

TR::CodeGenerator::TR_SimulatedNodeState &
OMR::CodeGenerator::simulatedNodeState(TR::Node *node, TR_RegisterPressureState *state)
   {
   self()->simulateNodeInitialization(node, state);
   return self()->simulatedNodeState(node);
   }

TR::CodeGenerator::TR_SimulatedNodeState &
OMR::CodeGenerator::simulatedNodeState(TR::Node *node)
   {
   return _simulatedNodeStates[node->getGlobalIndex()];
   }


void OMR::CodeGenerator::addToUnlatchedRegisterList(TR::RealRegister *reg)
   {
   // search the unlatchedRegisterList
   int i;
   for (i=0; _unlatchedRegisterList[i] != 0; i++)
      if (reg == _unlatchedRegisterList[i])
         break; // found my register
   if (_unlatchedRegisterList[i] == 0) // register not in the list
      {
      // add my register to the list
      _unlatchedRegisterList[i] = reg;
      _unlatchedRegisterList[i+1] = 0; // end of list marker
      }
   }


inline void standardNodeSimulationAnnotations(OMR::CodeGenerator::TR_RegisterPressureState *state, TR::Compilation *comp)
   {
   if (state->_candidate)
      traceMsg(comp, " %c%c", state->_pressureRiskFromStart? '+': state->_numLiveCandidateLoads? '|':' ', state->_pressureRiskUntilEnd? '+':' ');
   if (state->_memrefNestDepth >= 2)
      traceMsg(comp, " mem*%d", state->_memrefNestDepth);
   else if (state->_memrefNestDepth >= 1)
      traceMsg(comp, " mem");
   }

inline void leaveSpaceForRegisterPressureState(OMR::CodeGenerator::TR_RegisterPressureState *state, TR::Compilation *comp)
   {
   // Prints spaces to match the amount of space consumed by the g=, f=, c=, etc columns.
   // Call after dumpSimulatedNode for nodes where those columns won't be
   // printed.  This keeps any subsequent annotations lined up with those on
   // other rows.
   //
   traceMsg(comp, "%*s", 26, "");
   standardNodeSimulationAnnotations(state, comp);
   }

static TR_RegisterCandidate *findCandidate(TR::SymbolReference *symRef, TR_LinkHead<TR_RegisterCandidate> *candidates, TR_RegisterCandidate *anotherCandidate=NULL)
   {
   if (anotherCandidate && anotherCandidate->getSymbolReference() == symRef)
      return anotherCandidate;

   TR_RegisterCandidate *result;
   for (result = candidates->getFirst(); result && result->getSymbolReference() != symRef; result = result->getNext());
   return result;
   }

static void rememberMostRecentValue(TR::SymbolReference *symRef, TR::Node *valueNode, OMR::CodeGenerator::TR_RegisterPressureState *state, TR::CodeGenerator *cg)
   {
   if (  state->_alreadyAssignedOnExit.isSet(symRef->getReferenceNumber())
      || (state->_candidate && (state->getCandidateSymRef() == symRef)))
      {
      TR_RegisterCandidate *candidate = findCandidate(symRef, state->_candidatesAlreadyAssigned, state->_candidate);
      TR_ASSERT(candidate, "rememberMostRecentValue: there should be a matching candidate for #%d %s", symRef->getReferenceNumber(), cg->getDebug()->getName(symRef));
      if (candidate)
         candidate->setMostRecentValue(valueNode);
      }
   }

static void
keepMostRecentValueAliveIfLiveOnEntryToSuccessor(
      TR_RegisterCandidate *candidate,
      TR::TreeTop *exitPoint,
      TR::CFGNode *successor,
      OMR::CodeGenerator::TR_RegisterPressureState *state,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (  candidate->getBlocksLiveOnEntry().isSet(successor->getNumber())
      && candidate->getMostRecentValue() )
      {
      TR::Node *mrv = candidate->getMostRecentValue();
      cg->simulatedNodeState(mrv, state)._keepLiveUntil = exitPoint;
      if (comp->getOption(TR_TraceRegisterPressureDetails))
         {
         traceMsg(comp, "\n               Will keep #%s live until %s",
            cg->getDebug()->getName(mrv),
            cg->getDebug()->getName(exitPoint->getNode()));
         }
      }
   }

static void
killMostRecentValueIfKeptAliveUntilCurrentTreeTop(
      TR_RegisterCandidate *candidate,
      OMR::CodeGenerator::TR_RegisterPressureState *state,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (candidate && candidate->getMostRecentValue())
      {
      TR::Node *mrv = candidate->getMostRecentValue();
      TR::CodeGenerator::TR_SimulatedNodeState &nodeState = cg->simulatedNodeState(mrv);
      if (nodeState._keepLiveUntil == state->_currentTreeTop)
         {
         nodeState._keepLiveUntil = NULL;
         if (comp->getOption(TR_TraceRegisterPressureDetails))
            traceMsg(comp, " exiting(%s)", cg->getDebug()->getName(mrv));
         if (mrv->getFutureUseCount() == 0)
            {
            // Resurrect the node and kill it again more thoroughly
            mrv->incFutureUseCount();
            cg->simulateDecReferenceCount(mrv, state);
            }
         }
      }
   }

void
OMR::CodeGenerator::simulateBlockEvaluation(
      TR::Block *block,
      TR_RegisterPressureState *state,
      TR_RegisterPressureSummary *summary)
   {
   TR_ASSERT(!block->isExtensionOfPreviousBlock(), "simulateBlockEvaluation operates on extended basic blocks");
   state->_currentBlock = block;

   if (self()->traceSimulateTreeEvaluation())
      {
      traceMsg(self()->comp(), "            { simulating block_%d", block->getNumber());
      if (state->_candidate)
         traceMsg(self()->comp(), ", candidate %d", state->getCandidateSymRef()->getReferenceNumber());
      else
         traceMsg(self()->comp(), ", candidate ignored");
      traceMsg(self()->comp(), "\n               Already assigned on entry: ");
      self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &state->_alreadyAssignedOnEntry);
      traceMsg(self()->comp(), "\n               Already assigned on exit: ");
      self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &state->_alreadyAssignedOnExit);
      }

   TR::TreeTop *tt;

   if (state->_candidate || state->_candidatesAlreadyAssigned)
      {
      // Search for candidate loads and stores, and remember that they should be
      // considered live until the end of the last block where they are live on exit
      //
      for (tt = block->getEntry()->getNextTreeTop(); tt; tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (node->getOpCodeValue() == TR::BBStart && !node->getBlock()->isExtensionOfPreviousBlock())
            break;
         else
            self()->simulationPrePass(tt, node, state, summary);
         }
      }

   // Main simulation loop
   //
   tt = block->getEntry();
   TR_RegisterPressureSummary perBlockSummary = *summary;
   while (tt && !state->mustAbort())
      {
      state->_currentTreeTop = tt;
      self()->simulateTreeEvaluation(tt->getNode(), state, &perBlockSummary);
      if (tt->getNode()->getOpCode().isStoreDirect())
         rememberMostRecentValue(tt->getNode()->getSymbolReference(), tt->getNode()->getFirstChild(), state, self());
      else if (tt->getNode()->getOpCode().isLoadVarDirect())
         rememberMostRecentValue(tt->getNode()->getSymbolReference(), tt->getNode(), state, self());

      // Stop when we hit the next extended basic block
      //
      tt = tt->getNextTreeTop();
      if (!tt || (tt->getNode()->getOpCodeValue() == TR::BBStart && !tt->getNode()->getBlock()->isExtensionOfPreviousBlock()))
         break;

      if (tt->getNode()->getOpCodeValue() == TR::BBEnd)
         {
         _blockRegisterPressureCache[tt->getNode()->getBlock()->getNumber()] = perBlockSummary;
         summary->accumulate(perBlockSummary);
         perBlockSummary.reset(state->_gprPressure, state->_fprPressure, state->_vrfPressure);
         }
      }

   if (state->mustAbort())
      {
      if (self()->traceSimulateTreeEvaluation())
         traceMsg(self()->comp(), "\n               ABORTED");
      }
   else // Normal termination
      {
      // TODO: There must be some useful TR_ASSERT() I can put in here
      }

   // Only use mandatory spills, except in loop headers
   //
   TR_Structure *b, *loop;
   bool blockIsLoopHeader = true;
   if (1)
      {
      blockIsLoopHeader =
         (b = block->getStructureOf())   &&
         (loop = b->getContainingLoop()) &&
         (loop->getNumber() == block->getNumber());
      }
   if (!blockIsLoopHeader)
      {
      uint32_t mandatoryMask = (1 << TR_numMandatorySpillKinds) - 1;
      if (summary->_spillMask & ~mandatoryMask)
         {
         if (self()->traceSimulateTreeEvaluation())
            {
            traceMsg(self()->comp(), "\n               Removing non-mandatory spill kinds from ");
            summary->dumpSpillMask(self());
            }
         summary->_spillMask &= mandatoryMask;
         }
      }

   if (self()->traceSimulateTreeEvaluation())
      {
      traceMsg(self()->comp(), "\n            } finished simulating block_%d -- g=%d, f=%d, v=%d",
         block->getNumber(), summary->_gprPressure, summary->_fprPressure, summary->_vrfPressure);
      summary->dumpSpillMask(self());
      traceMsg(self()->comp(), "\n");
      }
   }

void
OMR::CodeGenerator::simulationPrePass(
      TR::TreeTop *tt,
      TR::Node *node,
      TR_RegisterPressureState *state,
      TR_RegisterPressureSummary *summary)
   {
   if (node->getVisitCount() == state->_visitCountForInit)
      return;
   else
      self()->simulateNodeInitialization(node, state);

   // Process children
   //
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      self()->simulationPrePass(tt, node->getChild(i), state, summary);

      // Compute node height
      //
      uint8_t childHeight = self()->simulatedNodeState(node->getChild(i), state)._height;
      if (childHeight >= self()->simulatedNodeState(node)._height)
         self()->simulatedNodeState(node).setHeight(childHeight+1);
      }

   // Keep track of the effect that GlRegDeps will have
   //
   if (node->getOpCode().isStoreDirect())
      {
      rememberMostRecentValue(node->getSymbolReference(), node->getFirstChild(), state, self());
      }
   else if (node->getOpCode().isLoadVarDirect())
      {
      rememberMostRecentValue(node->getSymbolReference(), node, state, self());
      TR_RegisterCandidate *rc = findCandidate(node->getSymbolReference(), state->_candidatesAlreadyAssigned, state->_candidate);
      if (rc)
         rc->setLastLoad(node);
      }
   else if (node->getOpCodeValue() == TR::BBEnd)
      {
      for (auto e = node->getBlock()->getSuccessors().begin(); e != node->getBlock()->getSuccessors().end(); ++e)
         {
         if (state->_candidate)
            keepMostRecentValueAliveIfLiveOnEntryToSuccessor(state->_candidate, tt, (*e)->getTo(), state, self());
         if (state->_candidatesAlreadyAssigned)
            for (TR_RegisterCandidate *candidate = state->_candidatesAlreadyAssigned->getFirst(); candidate; candidate = candidate->getNext())
               keepMostRecentValueAliveIfLiveOnEntryToSuccessor(candidate, tt, (*e)->getTo(), state, self());
         }
      }
   }

static bool
nodeGotFoldedIntoMemref(
      TR::Node *node,
      OMR::CodeGenerator::TR_RegisterPressureState *state,
      TR::CodeGenerator *cg)
   {
   return
      (  state->_currentMemref
      && !cg->isCandidateLoad(node, state)
      && cg->nodeResultGPRCount(node, state) == 0
      && cg->nodeResultFPRCount(node, state) == 0
      );
   }

void
OMR::CodeGenerator::simulateTreeEvaluation(TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary)
   {
   // Analogous to cg->evaluate(node).
   //
   // This can be called on nodes that have already been evaluated, and it does
   // extra bookkeeping in addition to the actual evaluation.  It is common on
   // all platforms.
   //
   // See also simulateNodeEvaluation.

   if (state->mustAbort())
      {
      if (self()->traceSimulateTreeEvaluation())
         traceMsg(self()->comp(), " ABORTED");
      return;
      }

   self()->simulateNodeInitialization(node, state);

   // Check for implicit NULLCHKs
   //
   static char *neverSkipNULLCHKs = feGetEnv("TR_neverSkipNULLCHKs");
   if (self()->getHasResumableTrapHandler() && (node->getOpCodeValue() == TR::NULLCHK || node->getOpCodeValue() == TR::ResolveAndNULLCHK) && !neverSkipNULLCHKs)
      {
      TR::Node  *child = node->getFirstChild();
      TR::ILOpCode &op = child->getOpCode();
      TR::TreeTop *nextTreeTop = state->_currentTreeTop->getNextTreeTop();

      // Scenarios where we don't evaluate the child here
      //
      bool isResolvedLoadVar = (op.isLoadVar() && child->getFutureUseCount() == 1 && !child->getSymbolReference()->isUnresolved());
      bool canFoldIntoBNDCHK = (nextTreeTop && nextTreeTop->getNode()->getOpCodeValue() == TR::BNDCHK && nextTreeTop->getNode()->getFirstChild() == child);

      if (isResolvedLoadVar || canFoldIntoBNDCHK)
         {
         // Probably not evaluated here.  No code is generated, so nothing
         // actually goes live at this point.
         self()->simulateSkippedTreeEvaluation(node, state, summary, 'N');
         self()->simulateDecReferenceCount(child, state);
         return;
         }
      }

   // Check for no-oped virtual guards
   //
   if (self()->getSupportsVirtualGuardNOPing() && self()->comp()->performVirtualGuardNOPing() && node->isNopableInlineGuard())
      {
      // Guard will be no-oped.
      self()->simulateSkippedTreeEvaluation(node, state, summary, 'g'); // 'g' for "guard"
      for (uint16_t i = 0; i < node->getNumChildren(); i++)
         self()->simulateDecReferenceCount(node->getChild(i), state);
      return;
      }

   if (self()->comp()->useAnchors() && node->getOpCode().isAnchor())
      {
      // no code is generated for anchor trees
      //
      self()->simulateSkippedTreeEvaluation(node, state, summary, 'T');
      for (uint16_t i = 0; i < node->getNumChildren(); i++)
         self()->simulateDecReferenceCount(node->getChild(i), state);
      return;
      }

   TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node);
   if (nodeState.hasRegister())
      {
      // Node is already live
      if (self()->isCandidateLoad(node, state) && !nodeState._liveCandidateLoad)
         {
         // This is where the candidate load would have gone live if not for GRA
         nodeState._liveCandidateLoad = 1;
         state->_numLiveCandidateLoads++;
         state->_pressureRiskFromStart = false;
         }

      if (self()->traceSimulateTreeEvaluation())
         {
         char tagChar = ' ';
         if (self()->isCandidateLoad(node, state))
            if (state->_candidate->getLastLoad() == node)
               tagChar = 'C';
            else
               tagChar = 'c';
         else if (self()->isLoadAlreadyAssignedOnEntry(node, state))
            {
            if (findCandidate(node->getSymbolReference(), state->_candidatesAlreadyAssigned)->getLastLoad() == node)
               tagChar = 'A';
            else
               tagChar = 'a';
            }

         self()->getDebug()->dumpSimulatedNode(node, tagChar);
         leaveSpaceForRegisterPressureState(state, self()->comp());
         }
      }
   else
      {
      if (node->getOpCodeValue() == TR::BBEnd)
         {
         // Kill anything that's being kept alive until here
         //
         killMostRecentValueIfKeptAliveUntilCurrentTreeTop(state->_candidate, state, self());
         for (TR_RegisterCandidate *candidate = state->_candidatesAlreadyAssigned->getFirst(); candidate; candidate = candidate->getNext())
            killMostRecentValueIfKeptAliveUntilCurrentTreeTop(candidate, state, self());
         }

      uint32_t numGPRTemporaries = 0, numFPRTemporaries = 0, numVRFTemporaries=0;
      bool isCall = node->getOpCode().isFunctionCall();

      if (isCall)
         {
         isCall = self()->willBeEvaluatedAsCallByCodeGen(node, self()->comp());
         }

      if (isCall && node->isTheVirtualCallNodeForAGuardedInlinedCall())
         isCall = false;

      // Simulate the platform tree evaluator
      //
      state->_gprPreviousPressure = state->_gprPressure;
      state->_fprPreviousPressure = state->_fprPressure;
      state->_vrfPreviousPressure = state->_vrfPressure;

      if (self()->nodeWillBeRematerialized(node, state))
         {
         //TR_ASSERT(node->getFutureUseCount() >= 2, "Node %s must have refcount >= 2 to be rematerialized", getDebug()->getName(node));
         TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node, state);

         // Make it look like remat has occurred.  That means making this tree:
         //
         //  (x)     node
         //  (y)       child1
         //  (z)       child2
         //
         // ...look like this:
         //
         //  (1)     node
         //  (y+1)     child1
         //  (z+1)     child2
         //
         //  (x-1)   node
         //            ==>child1
         //            ==>child2
         //
         nodeState._willBeRematerialized = true;
         scount_t originalRefcount = node->getFutureUseCount();
         node->setFutureUseCount(1);
         for (uint16_t i = 0; i < node->getNumChildren(); i++)
            {
            TR::Node *child = node->getChild(i);
            self()->simulateNodeInitialization(child, state);
            child->incFutureUseCount();

            if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
               traceMsg(self()->comp(), " ++%s", self()->getDebug()->getName(child));
            }

         self()->simulateNodeEvaluation(node, state, summary);

         node->setFutureUseCount(originalRefcount); // Parent will decrement
         }
      else
         {
         // Some kinds of nodes very commonly use temporaries, and sometimes
         // the number of temporaries depends on the registers used by
         // children, so compute this while the children are still live.
         //
         if (isCall)
            {
            self()->getNumberOfTemporaryRegistersUsedByCall(node, state, numGPRTemporaries, numFPRTemporaries, numVRFTemporaries);
            if ((numGPRTemporaries >= 1 || numFPRTemporaries >= 1 || numVRFTemporaries >= 1)
               && (self()->comp()->getOption(TR_TraceRegisterPressureDetails) && !performTransformation(self()->comp(), "O^O GLOBAL REGISTER ALLOCATION: Call %p has %d GPR and %d FPR and VRF %d dummy registers\n", node, numGPRTemporaries, numFPRTemporaries, numVRFTemporaries)))
               {
               numGPRTemporaries = 0;
               numFPRTemporaries = 0;
               numVRFTemporaries = 0;
               }
            numGPRTemporaries = std::max<uint32_t>(numGPRTemporaries, 0);
            numFPRTemporaries = std::max<uint32_t>(numFPRTemporaries, 0);
            numVRFTemporaries = std::max<uint32_t>(numVRFTemporaries, 0);
            }

         self()->simulateNodeEvaluation(node, state, summary);
         }

      // Accumulate the current state into the summary
      //
      summary->accumulate(state, self(), numGPRTemporaries, numFPRTemporaries, numVRFTemporaries);

      if (state->candidateIsLiveAfterGRA())
         {
         // Check the special register kinds.
         // TODO: this code should be moved to TR::Linkage later
         if (isCall)
            {
            summary->spill(TR_volatileSpill, self());

            TR::Symbol * rcSymbol = state->_candidate->getSymbolReference()->getSymbol();
            TR::Linkage *linkage = self()->getLinkage();

            // In Xplink, 31-bit mode, each call has a special argument passed in GPR12
            // Incoming special argument can be  passed to the call,
            // otherwise, TR_linkageSpill has to be set so that other candidates don't get
            // assigned GPR12 across this call
            summary->spill(TR_linkageSpill, self());
            summary->setLinkagePresent(node->getSymbol()->castToMethodSymbol()->getLinkageConvention(), self());
            }

         if (isCall)
            {
            summary->spill(TR_vmThreadSpill, self());
            }
         else if (node->getOpCodeValue() == TR::asynccheck || node->getOpCodeValue() == TR::monent || node->getOpCodeValue() == TR::monexit)  // TODO: Is this the best test to use?
            {
            summary->spill(TR_vmThreadSpill, self());
            }
         }
      }
   }

void
OMR::CodeGenerator::simulateSkippedTreeEvaluation(TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary, char tagChar)
   {
   // Called for trees that we expect the codegen not to evaluate.
   // For instance, a multiply by zero doesn't need to evaluate its first child.
   //
   // It is recommended that the caller also decrement the node's refcount as
   // early as possible, to expose more opportunities for efficient use of registers.

   static char *disableSimulateSkippedTreeEvaluation = feGetEnv("TR_disableSimulateSkippedTreeEvaluation");
   if (disableSimulateSkippedTreeEvaluation)
      {
      self()->simulateTreeEvaluation(node, state, summary);
      return;
      }

   self()->simulateNodeInitialization(node,state);

   if (self()->traceSimulateTreeEvaluation())
      {
      self()->getDebug()->dumpSimulatedNode(node, tagChar);
      leaveSpaceForRegisterPressureState(state, self()->comp());
      }
   }

void
OMR::CodeGenerator::simulateNodeEvaluation(TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary)
   {
   // Analogous to TR_XXTreeEvaluator::evaluateXXX(node, cg).
   //
   // Only called on nodes that are not already live, and is responsible for
   // doing any pattern-matching to decide which children to evaluate (which it
   // does by calling simulateTreeEvaluation).  This is a default
   // implementation which is not too shabby, but platforms will probably
   // override and customize it.
   //
   // See also simulateTreeEvaluation.

   TR_ASSERT(!self()->simulatedNodeState(node, state).hasRegister(), "simulateNodeEvaluation called on live node " POINTER_PRINTF_FORMAT, node);

   // If the first child is likely to have low register pressure, we should
   // evaluate the second child first to avoid the situation where we end up
   // keeping the first child's result alive across a very heavyweight second
   // child.
   //
   // The heuristic here is just to compare node heights.  The higher node
   // probably has more register pressure, so should be evaluated first.  A
   // truly accurate way would be to do the simulation both ways and pick the
   // one with lower pressure, but that would make the simulation process too
   // unlike actual tree evaluation (which happens only once).
   //
   bool evaluateSecondChildFirst = false;
   if (  node->getNumChildren() == 2
      && !self()->simulatedNodeState(node, state).hasRegister()
      && !node->getOpCode().isIndirect()
      ){
      evaluateSecondChildFirst = (self()->simulatedNodeState(node->getSecondChild(), state)._height
                                > self()->simulatedNodeState(node->getFirstChild(),  state)._height);
      }

   static char *neverEvaluateSecondChildFirst = feGetEnv("TR_neverEvaluateSecondChildFirst");
   if (neverEvaluateSecondChildFirst)
      evaluateSecondChildFirst = false;

   // For calls, most arguments are passed in memory, and don't add to register pressure,
   // so those children can be decremented immediately.  childrenInRegisters indicates which
   // children have NOT been decremented.
   //
   flags32_t childrenInRegisters(0); // (Don't bother with a bitvector on the slight chance that some child beyond the first 32 will get in a register)
   const int32_t maxChildrenInRegisters = 8 * sizeof(childrenInRegisters);
   int32_t numArgumentRegisters[NumRegisterKinds];
   TR::Linkage *callNodeLinkage = NULL;
   if (node->getOpCode().isFunctionCall())
      callNodeLinkage = self()->getLinkage(node->getSymbol()->castToMethodSymbol()->getLinkageConvention());

   if (callNodeLinkage)
      {
      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(self()->comp(), " (%s %s linkage)",
            self()->getDebug()->getName(node),
            self()->getDebug()->getLinkageConventionName(node->getSymbol()->castToMethodSymbol()->getLinkageConvention()));
      for (int32_t k = 0; k < NumRegisterKinds; k++)
         numArgumentRegisters[k] = callNodeLinkage->numArgumentRegisters((TR_RegisterKinds)k);
      }

   // "Evaluate" children.
   //
   TR_SimulatedMemoryReference memref(self()->trMemory());
   if (evaluateSecondChildFirst)
      {
      TR_ASSERT(node->getNumChildren() == 2, "assertion failure");
      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(self()->comp(), " (%s before %s)", self()->getDebug()->getName(node->getSecondChild()), self()->getDebug()->getName(node->getFirstChild()));
      self()->simulateTreeEvaluation(node->getSecondChild(), state, summary);
      self()->simulateTreeEvaluation(node->getFirstChild(),  state, summary);
      }
   else
      {
      bool hasMemRef = false;
      if (node->getOpCode().isIndirect() && node->getOpCode().isLoadVarOrStore())
         hasMemRef = true;

      for (int32_t childIndex = (hasMemRef?1:0); childIndex < node->getNumChildren(); childIndex++)
         {
         self()->simulateTreeEvaluation(node->getChild(childIndex), state, summary);
         if (callNodeLinkage)
            {
            if (  childIndex < maxChildrenInRegisters
               && --numArgumentRegisters[callNodeLinkage->argumentRegisterKind(node->getChild(childIndex))] >= 0)
               {
               childrenInRegisters.set(1 << childIndex);
               if (callNodeLinkage && self()->comp()->getOption(TR_TraceRegisterPressureDetails))
                  traceMsg(self()->comp(), " (%s arg %d in %s, %d left)",
                     self()->getDebug()->getName(node),
                     childIndex,
                     self()->getDebug()->getRegisterKindName(callNodeLinkage->argumentRegisterKind(node->getChild(childIndex))),
                     numArgumentRegisters[callNodeLinkage->argumentRegisterKind(node->getChild(childIndex))]);
               }
            else
               {
               // Child is in memory, so its register dies immediately after the store
               self()->simulateDecReferenceCount(node->getChild(childIndex), state);
               if (callNodeLinkage && self()->comp()->getOption(TR_TraceRegisterPressureDetails))
                  traceMsg(self()->comp(), " (%s arg %s in mem)",
                     self()->getDebug()->getName(node),
                     self()->getDebug()->getName(node->getChild(childIndex)));
               }
            }
         }

      if (hasMemRef) // Codegen usually evaluates address child last
         self()->simulateMemoryReference(&memref, node->getChild(0), state, summary);
      }

   if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
      {
      traceMsg(self()->comp(), "state->_gprPressure = %d summary->_gprPressure = %d summary->PRESSURE_LIMIT = %d\n",state->_gprPressure,summary->_gprPressure,summary->PRESSURE_LIMIT);
      }

   if (summary->_gprPressure < summary->PRESSURE_LIMIT)
      TR_ASSERT(state->_gprPressure <= summary->_gprPressure, "Children of %s must record max register gprPressure in summary; %d > %d", self()->getDebug()->getName(node), state->_gprPressure, summary->_gprPressure);
   if (summary->_fprPressure < summary->PRESSURE_LIMIT )
      TR_ASSERT(state->_fprPressure <= summary->_fprPressure, "Children of %s must record max register fprPressure in summary; %d > %d", self()->getDebug()->getName(node), state->_fprPressure, summary->_fprPressure);
   if (summary->_vrfPressure <= summary->PRESSURE_LIMIT)
      TR_ASSERT(state->_vrfPressure <= summary->_vrfPressure, "Children of %s must record max register vrfPressure in summary; %d > %d", self()->getDebug()->getName(node), state->_vrfPressure, summary->_vrfPressure);


   // Dec children's ref counts.
   //
   // Note that we do this after evaluating all children.  Otherwise, we
   // don't properly account for cases where one child's result must live
   // across the evaluation of another child.
   //
   char *tag = NULL;
   if (  nodeGotFoldedIntoMemref(node, state, self())
      && self()->simulatedNodeState(node)._willBeRematerialized == 0 // remat nodes have already had their children incremented, so we must decrement them
      ){
      // This node got folded into a memref instead of being evaluated to a
      // register, but we're going to see it at least one more time.
      // We must decrement children only on their last appearance.
      //
      tag = " memFolded";
      state->_currentMemref->add(node, state, self());
      }
   else
      {
      if (callNodeLinkage)
         {
         if (node->getNumChildren() >= 1)
            tag = " decRegArgs";

         if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
            traceMsg(self()->comp(), " childrenInRegisters=" UINT64_PRINTF_FORMAT_HEX, childrenInRegisters.getValue());

         for (int32_t childIndex = std::min<int32_t>(maxChildrenInRegisters, node->getNumChildren())-1; childIndex >= 0; childIndex--)
            if (childrenInRegisters.testAny(1 << childIndex))
               self()->simulateDecReferenceCount(node->getChild(childIndex), state);
         }
      else
         {
         if (node->getNumChildren() >= 1)
            tag = " decChildren";

         for (int32_t childIndex = 0; childIndex < node->getNumChildren(); childIndex++)
            self()->simulateDecReferenceCount(node->getChild(childIndex), state);
         }

      memref.simulateDecNodeReferenceCounts(state, self());
      self()->simulatedNodeState(node)._childRefcountsHaveBeenDecremented = 1;
      }

   // Account for registers needed for this node's result.
   //
   // Note that we do this after decrementing children's ref counts on the
   // assumption that any registers that go free can be used to store this
   // node's result.
   //
   self()->simulateNodeGoingLive(node, state);

   if (tag && self()->comp()->getOption(TR_TraceRegisterPressureDetails))
      traceMsg(self()->comp(), tag);

   }

bool
OMR::CodeGenerator::nodeWillBeRematerialized(TR::Node *node,  TR_RegisterPressureState *state)
   {
   // For the initial candidate-agnostic simulations, don't model remat.  This
   // will have the effect of conservatively overestimating reg pressure, which
   // is what we want.
   //
   if (!state->_candidate)
      return false;

   // Nodes that aren't commoned can't possibly be rematerialized
   //
   if (node->getFutureUseCount() <= 1)
      return false;

   TR::ILOpCode  &op         = node->getOpCode();
   TR::ILOpCodes opCodeValue = op.getOpCodeValue();
   bool result = false;
   if (state->_currentMemref)
      {
      if (op.isArrayRef())
         result = true;
      else if (op.isInteger() || node->getType().isAddress())
         {
         bool isByConst = (node->getNumChildren() >= 2)? (node->getSecondChild()->getOpCode().isLoadConst()) : false;
         if (self()->getSupportsConstantOffsetInAddressing() && (op.isAdd() || op.isSub()) && isByConst)
            result = true;
         else if (self()->getSupportsScaledIndexAddressing() && (op.isMul() || op.isLeftShift()) && isByConst)
            result = true;
         }
      }

   return result;
   }

void
OMR::CodeGenerator::simulateMemoryReference(TR_SimulatedMemoryReference *memref, TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary)
   {
   state->_memrefNestDepth++;
   TR_SimulatedMemoryReference *oldMemref = state->_currentMemref;
   state->_currentMemref = memref;
   self()->simulateTreeEvaluation(node, state, summary);
   state->_currentMemref = oldMemref;
   state->_memrefNestDepth--;
   }

static int64_t powerOfTwoScaleFactor(TR::Node *node) // INT_MAX means the node doesn't represent a fixed-point mul by a power of 2
   {
   TR::ILOpCode &op = node->getOpCode();
   if (!op.isInteger() && !op.isLong())
      return INT_MAX;

   if (node->getNumChildren() < 2)
      return INT_MAX;

   TR::Node *constChild = node->getSecondChild();
   if (!constChild->getOpCode().isLoadConst())
      return INT_MAX;

   int64_t value = (constChild->getOpCode().isLong())? constChild->getLongInt() : constChild->getInt();

   if (op.isMul() && !(value & (value-1)))
      return value;

   if (op.isLeftShift() && value <= 31) // Assuming platforms can't do 4GB strides anyway
      return ((int64_t)1 << value);

   return INT_MAX;
   }

bool
OMR::CodeGenerator::nodeCanBeFoldedIntoMemref(TR::Node  *node,  TR_RegisterPressureState *state)
   {
   // Based on populateMemoryReference, but no recursion because that's done at a higher level

   TR::ILOpCode &op = node->getOpCode();
   TR::Node *firstChild  = (node->getNumChildren() >= 1)? node->getFirstChild()  : NULL;
   TR::Node *secondChild = (node->getNumChildren() >= 2)? node->getSecondChild() : NULL;
   bool isIntOrAddress = (op.isInteger() || node->getType().isAddress());
   bool isByConst      = secondChild? (secondChild->getOpCode().isLoadConst()) : false;
   bool isByIntOrLong  = secondChild? (secondChild->getType().isInt32() || secondChild->getType().isInt64()) : false;

   bool result = false;

   if ((self()->simulatedNodeState(node).hasRegister() || node->getFutureUseCount() > 1) && !self()->nodeWillBeRematerialized(node, state))
      {
      // May survive outside the memref
      }
   else if (op.isAdd() && isIntOrAddress)
      {
      result = true;
      }
   else if (op.isSub() && isByConst && isIntOrAddress)
      {
      // The const can be included in the memref displacement
      result = true;
      }
   else if (powerOfTwoScaleFactor(node) <= (TR_SimulatedMemoryReference::MAX_STRIDE))
      {
      // The const can be included in the memref stride
      result = true;
      }
   else if (op.getOpCodeValue() == TR::i2l && node->skipSignExtension())
      {
      // This i2l is only here for type-correctness
      result = true;
      }
   else if ((op.getOpCodeValue() == TR::loadaddr) || (op.isLoadConst() && isIntOrAddress))
      {
      // Can be included in the memref displacement
      result = true;
      }
   else
      {
      // Can't do anything special
      }

   return result;
   }

bool
OMR::CodeGenerator::nodeResultConsumesNoRegisters(TR::Node *node, TR_RegisterPressureState *state)
   {
   TR::ILOpCode &op = node->getOpCode();
   if (op.isTreeTop())
      {
      // Some treetop nodes lie about their return type
      return true;
      }
   else if (state->_currentMemref && self()->nodeCanBeFoldedIntoMemref(node, state))
      {
      return true;
      }

   return false;
   }

uint8_t
OMR::CodeGenerator::nodeResultGPRCount(TR::Node *node, TR_RegisterPressureState *state)
   {
   if (self()->nodeResultConsumesNoRegisters(node, state))
      return 0;
   else if (node->getOpCodeValue() == TR::PassThrough)
      return self()->nodeResultGPRCount(node->getFirstChild(), state);
   else
      {
      uint8_t result = self()->gprCount(node->getType(), 0);
      if (result == 2 && !node->getType().isAggregate() && node->isHighWordZero() && node->getFutureUseCount() <= 1)
         result = 1;
      return result;
      }
   }

uint8_t
OMR::CodeGenerator::nodeResultFPRCount(TR::Node *node, TR_RegisterPressureState *state)
   {
   if (self()->nodeResultConsumesNoRegisters(node, state))
      return 0;
   else if (node->getOpCodeValue() == TR::PassThrough)
      return self()->nodeResultFPRCount(node->getFirstChild(), state);
   else
      return self()->fprCount(node->getType());
   }

uint8_t
OMR::CodeGenerator::nodeResultVRFCount(TR::Node *node, TR_RegisterPressureState *state)
   {
   if (self()->nodeResultConsumesNoRegisters(node, state))
      return 0;
   else if (node->getOpCodeValue() == TR::PassThrough)
      return self()->nodeResultVRFCount(node->getFirstChild(), state);
   else
      return self()->vrfCount(node->getType());
   }

uint8_t
OMR::CodeGenerator::nodeResultSSRCount(TR::Node *node, TR_RegisterPressureState *state)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (node->getType().isBCD())
      return 1;
   else
#endif
      if (node->getOpCodeValue() == TR::PassThrough)
      return self()->nodeResultSSRCount(node->getFirstChild(), state);
   else
      return 0;
   }
uint8_t
OMR::CodeGenerator::nodeResultARCount(TR::Node *node, TR_RegisterPressureState *state)
   {
   if (node->getOpCodeValue() == TR::PassThrough)
      return self()->nodeResultARCount(node->getFirstChild(), state);
   else
      return 0;
   }

void
OMR::CodeGenerator::getNumberOfTemporaryRegistersUsedByCall(TR::Node * node, TR_RegisterPressureState * state, uint32_t & numGPRTemporaries, uint32_t & numFPRTemporaries, uint32_t & numVRFTemporaries)
   {
   int32_t numGPRVolatileRegs = 0, numGPRLinkageRegs = 0,
      numFPRVolatileRegs = 0, numFPRLinkageRegs = 0,
      numVRFVolatileRegs = 0, numVRFLinkageRegs = 0;

   {
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());
   TR_BitVector bv(self()->getNumberOfGlobalRegisters(), self()->trMemory(), stackAlloc, growable);
   bv  = *self()->getGlobalRegisters(TR_volatileSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   bv |= *self()->getGlobalRegisters(TR_vmThreadSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention()); // vmthread, when present, is also a virtual reg not represented in trees
   bv &= *self()->getGlobalRegisters(TR_gprSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   numGPRVolatileRegs = bv.elementCount();
   bv &= *self()->getGlobalRegisters(TR_linkageSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   numGPRLinkageRegs = bv.elementCount();
   bv  = *self()->getGlobalRegisters(TR_volatileSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   bv &= *self()->getGlobalRegisters(TR_fprSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   numFPRVolatileRegs = bv.elementCount();
   bv &= *self()->getGlobalRegisters(TR_linkageSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   numFPRLinkageRegs = bv.elementCount();
   bv &= *self()->getGlobalRegisters(TR_vrfSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   numVRFVolatileRegs = bv.elementCount();
   bv &= *self()->getGlobalRegisters(TR_linkageSpill, self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   numVRFLinkageRegs = bv.elementCount();
   }

   uint16_t numGPRArgs = 0, numFPRArgs = 0, numVRFArgs = 0;
   for (int32_t i = node->getFirstArgumentIndex(); i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      numGPRArgs = std::min(numGPRLinkageRegs, numGPRArgs + self()->simulatedNodeState(child, state)._liveGPRs);
      numFPRArgs = std::min(numFPRLinkageRegs, numFPRArgs + self()->simulatedNodeState(child, state)._liveFPRs);
      numVRFArgs = std::min(numVRFLinkageRegs, numVRFArgs + self()->simulatedNodeState(child, state)._liveVRFs);
      }

   numGPRTemporaries = numGPRVolatileRegs - numGPRArgs;
   numFPRTemporaries = numFPRVolatileRegs - numFPRArgs;
   numVRFTemporaries = numVRFVolatileRegs - numVRFArgs;
   }

void
OMR::CodeGenerator::simulateNodeInitialization(TR::Node *node, TR_RegisterPressureState *state)
   {
   if (!state->isInitialized(node))
      {
      node->setVisitCount(state->_visitCountForInit);
      node->setFutureUseCount(node->getReferenceCount());
      TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node);
      nodeState.initialize();
      if (  (self()->isCandidateLoad(node, state) && state->candidateIsLiveOnEntry())
         || self()->isLoadAlreadyAssignedOnEntry(node, state))
         {
         // This node is actually already live and included in the pressure totals.
         // Here we will opdate the node's state to reflect this.
         //
         nodeState._liveGPRs = self()->nodeResultGPRCount(node, state);
         nodeState._liveFPRs = self()->nodeResultFPRCount(node, state);
         nodeState._liveVRFs = self()->nodeResultVRFCount(node, state);
         nodeState._childRefcountsHaveBeenDecremented = 1;
         }

      }
   TR_ASSERT(state->isInitialized(node), "assertion failure");
   }

void
OMR::CodeGenerator::simulateNodeGoingLive(TR::Node *node, TR_RegisterPressureState *state)
   {
   TR_ASSERT(state->isInitialized(node), "Node %s should have been initialized before going live", self()->getDebug()->getName(node));

   // Children that will be rematerialized will have a refcount of 1, so their
   // registers should go dead now (regardless of its actual pre-remat refcount).
   //
   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      TR_SimulatedNodeState &childNodeState = self()->simulatedNodeState(child, state);
      if (childNodeState._willBeRematerialized && childNodeState._childRefcountsHaveBeenDecremented)
         {
         if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
            traceMsg(self()->comp(), " rematChild:");
         self()->simulateNodeGoingDead(child, state);
         }
      childNodeState._willBeRematerialized = 0;
      }

   TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node);
   TR_ASSERT(!nodeState.hasRegister(), "assertion failure");
   nodeState._liveGPRs = self()->nodeResultGPRCount(node, state);
   nodeState._liveVRFs = self()->nodeResultVRFCount(node, state);
   nodeState._liveFPRs = self()->nodeResultFPRCount(node, state);
   nodeState._liveSSRs = self()->nodeResultSSRCount(node, state);
   nodeState._liveARs = self()->nodeResultARCount(node, state);
   state->_gprPressure += nodeState._liveGPRs;
   state->_fprPressure += nodeState._liveFPRs;
   state->_vrfPressure += nodeState._liveVRFs;

   // Tracing
   //
   if (self()->traceSimulateTreeEvaluation())
      {
      self()->getDebug()->dumpSimulatedNode(node, self()->isCandidateLoad(node, state)? 'C':' ');
      traceMsg(self()->comp(), "%2d(%d) g%+d=%-2d f%+d=%-2d v%+d=%-2d",
         self()->simulatedNodeState(node, state)._height,
         node->getNumChildren(),
         self()->nodeResultGPRCount(node, state), state->_gprPressure,
         self()->nodeResultFPRCount(node, state), state->_fprPressure,
         self()->nodeResultVRFCount(node, state), state->_vrfPressure);
      standardNodeSimulationAnnotations(state, self()->comp());
      }
   }

void
OMR::CodeGenerator::simulateNodeGoingDead(TR::Node *node, TR_RegisterPressureState *state)
   {
   TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node);
   TR_ASSERT(nodeState._childRefcountsHaveBeenDecremented, "assertion failure");
   if (  self()->isCandidateLoad(node, state)
      && state->candidateIsLiveOnEntry()
      && (node != state->_candidate->getLastLoad()))
      {
      // There's another candidate load coming up, and that one will be live on entry too
      state->_pressureRiskFromStart = true;
      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(self()->comp(), " *%s", self()->getDebug()->getName(node));
      }
   else if (self()->isLoadAlreadyAssignedOnEntry(node, state)
      && (node != findCandidate(node->getSymbolReference(), state->_candidatesAlreadyAssigned)->getLastLoad()))
      {
      // There's another load coming up
      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(self()->comp(), " *%s", self()->getDebug()->getName(node));
      }
   else
      {
      state->_gprPressure -= nodeState._liveGPRs;
      state->_fprPressure -= nodeState._liveFPRs;
      state->_vrfPressure -= nodeState._liveVRFs;
      TR_ASSERT(state->_gprPressure >= 0, "GPR pressure must never be negative");
      TR_ASSERT(state->_fprPressure >= 0, "FPR pressure must never be negative");
      TR_ASSERT(state->_vrfPressure >= 0, "VRF pressure must never be negative");
      if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(self()->comp(), " ~%s", self()->getDebug()->getName(node));
      }

   if (self()->isCandidateLoad(node, state) && nodeState._liveCandidateLoad)
      {
      state->_numLiveCandidateLoads--;
      TR_ASSERT(state->_numLiveCandidateLoads >= 0, "numLiveCandidateNodes must never be negative");
      nodeState._liveCandidateLoad = 0;
      }
   else
      {
      nodeState._liveGPRs = nodeState._liveFPRs = nodeState._liveVRFs = nodeState._liveSSRs = nodeState._liveARs =0;
      TR_ASSERT(!nodeState.hasRegister(), "assertion failure");
      }
   }

void
OMR::CodeGenerator::simulateDecReferenceCount(TR::Node *node, TR_RegisterPressureState *state)
   {
   self()->simulateNodeInitialization(node, state);

   if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
      traceMsg(self()->comp(), " --%s", self()->getDebug()->getName(node));

   TR_ASSERT(node->getFutureUseCount() > 0, "Too many simulated refcount decrements on node %s, refcount %d", self()->getDebug()->getName(node), node->getReferenceCount());
   if (node->decFutureUseCount() == 0)
      {
      TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node, state);
      if (nodeState._childRefcountsHaveBeenDecremented)
         {
         if (nodeState._keepLiveUntil)// && !simulatedNodeState(node, state)._willBeRematerialized)
            {
            // GRA will cause this node to stay live until the end of a block, so
            // don't remove it from the pressure estimates yet.
            //
            // TODO: If the node being kept alive is a RegLoad, this logic will cause it
            // to be counted both as a "liveCandidateLoad" as well as pressure risk, which
            // makes the logs look confusing because you get both a "|" and a "+", making it
            // look like there are two values of the candidate live at the same time.  This
            // should be counted ONLY as pressure risk, not as a live candidate load, because
            // the latter should include only virtual regs that would be live even in the
            // absence of GRA.  I'm thinking I should be calling simulateNodeGoingDead here,
            // but I'm not sure of all the ramifications of that.
            //
            if (state->_candidate && node == state->_candidate->getMostRecentValue())
               {
               // Now there's a risk of increasing register pressure if we give this
               // candidate a register.
               //
               if (!nodeState._isCausingPressureRiskUntilEnd)
                  {
                  state->_pressureRiskUntilEnd++;
                  nodeState._isCausingPressureRiskUntilEnd = 1;
                  if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
                     traceMsg(self()->comp(), " keep:%s", self()->getDebug()->getName(node));
                  }
               }
            else if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
               traceMsg(self()->comp(), " keeping:%s", self()->getDebug()->getName(node)); // Some node other than the candidate's most recent value is being kept alive
            }
         else
            {
            if (nodeState._isCausingPressureRiskUntilEnd)
               {
               state->_pressureRiskUntilEnd--;
               nodeState._isCausingPressureRiskUntilEnd = 0;
               }
            self()->simulateNodeGoingDead(node, state);
            }
         }
      else
         {
         // node was never evaluated, so all its refs to its children were not actually needed.
         // This is kind of like a lazy recursivelyDecReferenceCount.
         //
         if (self()->comp()->getOption(TR_TraceRegisterPressureDetails))
            traceMsg(self()->comp(), " ~~%s", self()->getDebug()->getName(node));

         for (int32_t childIndex = 0; childIndex < node->getNumChildren(); childIndex++)
            {
            TR::Node *child = node->getChild(childIndex);
            self()->simulateDecReferenceCount(child, state);
            }
         }
      }
   }

void
OMR::CodeGenerator::TR_SimulatedMemoryReference::add(
      TR::Node *node,
      TR_RegisterPressureState *state,
      TR::CodeGenerator *cg)
   {
   if (_numRegisters >= MAX_NUM_REGISTERS)
      {
      // Pretend to emit an LEA
      if (cg->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(cg->comp(), " consolidateMemref{");

      simulateDecNodeReferenceCounts(state, cg);
      _numConsolidatedRegisters = 1;
      state->_gprPressure += _numConsolidatedRegisters;

      if (cg->comp()->getOption(TR_TraceRegisterPressureDetails))
         traceMsg(cg->comp(), " }");
      }
   //_undecrementedNodes.add(node);
   _numRegisters += 1;
   //_numRegisters += cg->simulatedNodeState(node)._liveGPRs; // Fix this once we find a platform that can put other reg kinds in a memref
   }

void OMR::CodeGenerator::TR_SimulatedMemoryReference::simulateDecNodeReferenceCounts(
      TR_RegisterPressureState *state,
      TR::CodeGenerator *cg)
   {
   //_undecrementedNodes.deleteAll();
   _numRegisters = 0;
   state->_gprPressure -= _numConsolidatedRegisters;
   _numConsolidatedRegisters = 0;

   // Nodes included in a memref generally have refcount zero anyway, so
   // there's no real need to dec them; and currently decrementing them
   // properly is a bit hairy because a dec of one node could be accompanied by
   // a dec of a nother node that goes recursive and re-decs the first one.
   //
   return;

   //ListIterator<TR::Node> li(&_undecrementedNodes);
   //for (TR::Node *node = li.getFirst(); node; node = li.getNext())
   //   cg->simulateDecReferenceCount(node, state);
   }

void
OMR::CodeGenerator::TR_RegisterPressureSummary::accumulate(
      TR_RegisterPressureState *state,
      TR::CodeGenerator *cg,
      uint32_t gprTemps,
      uint32_t fprTemps,
      uint32_t vrfTemps)
   {
   // Note that we ignore pressure-caused spills when a candidate load is live,
   // because in that case, assigning the candidate to a register makes no
   // difference: that spill would occur either way.  It's only when the
   // candidate would NOT be live that assigning it to a global register would
   // increase pressure.

   uint32_t gprs = state->_gprPressure + gprTemps;
   if (gprs > state->_gprLimit && state->pressureIsAtRisk())
      spill(TR_gprSpill, cg);

   uint32_t fprs = state->_fprPressure + fprTemps;
   if (fprs > state->_fprLimit && state->pressureIsAtRisk())
      spill(TR_fprSpill, cg);

   uint32_t vrfs = state->_vrfPressure + vrfTemps;
   if (vrfs > state->_vrfLimit && state->pressureIsAtRisk())
      spill(TR_vrfSpill, cg);

   accumulate(gprs, fprs, vrfs);
   }
