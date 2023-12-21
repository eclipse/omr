/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "codegen/CodeGenerator.hpp" // IWYU pragma: keep

#include <stdint.h>
#include <string.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Node.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O CODE GENERATION: "


void
OMR::CodeGenerator::createStackAtlas()
   {
   // Assign a GC map index to each reference parameter and each reference local.
   // Stack mapping will have to map the stack in a way that honours these indices
   //
   TR::Compilation *comp = self()->comp();
   TR::ResolvedMethodSymbol * methodSymbol = comp->getMethodSymbol();
   int32_t stackSlotSize = static_cast<int32_t>(TR::Compiler->om.sizeofReferenceAddress());

   int32_t slotIndex = 0;
   int32_t numberOfParmSlots = 0;
   int32_t numberOfPendingPushSlots = 0;

   // Find the offsets of the first and last reference parameters
   //
   int32_t firstMappedParmOffset = methodSymbol->getNumParameterSlots() * stackSlotSize;


   // Now assign GC map indices to parameters depending on the linkage mapping.
   // At the same time build the parameter map.
   //
   TR_GCStackMap * parameterMap = new (self()->trHeapMemory(), numberOfParmSlots) TR_GCStackMap(numberOfParmSlots);


   // --------------------------------------------------------------------------
   // Now assign a GC map index to reference locals. When the stack is mapped,
   // these locals will have to be mapped contiguously in the stack according to
   // this index.
   //
   // Locals that need initialization during the method prologue are mapped first,
   // then the ones that do not need initialization.


   ListIterator<TR::AutomaticSymbol> automaticIterator(&methodSymbol->getAutomaticList());

   TR::AutomaticSymbol * localCursor;

   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->isCollectedReference() &&
          !localCursor->isLocalObject() &&
          !localCursor->isInitializedReference() &&
          !localCursor->isInternalPointer() &&
          !localCursor->isPinningArrayPointer())
         {


         localCursor->setGCMapIndex(slotIndex);
         slotIndex += localCursor->getNumberOfSlots();
         }
      }



   int32_t numberOfSlots = slotIndex;

   // --------------------------------------------------------------------------
   // Build the stack map for a method.  Start with all parameters and locals
   // being live, and selectively mark slots that are not live.
   //
   TR_GCStackMap * localMap = new (self()->trHeapMemory(), numberOfSlots) TR_GCStackMap(numberOfSlots);
   localMap->copy(parameterMap);

   // Set all the local references to be live
   //
   int32_t i;
   for (i = numberOfParmSlots; i < numberOfSlots; ++i)
      {
      localMap->setBit(i);
      }


   self()->setMethodStackMap(localMap);

   // Now create the stack atlas
   //
   TR::GCStackAtlas * atlas = new (self()->trHeapMemory()) TR::GCStackAtlas(numberOfParmSlots, numberOfSlots, self()->trMemory());

   atlas->setParmBaseOffset(firstMappedParmOffset);
   atlas->setParameterMap(parameterMap);
   atlas->setLocalMap(localMap);
   atlas->setStackAllocMap(NULL);
   atlas->setNumberOfSlotsToBeInitialized(slotIndex);
   atlas->setIndexOfFirstSpillTemp(numberOfSlots);
   atlas->setInternalPointerMap(0);
   atlas->setNumberOfPendingPushSlots(numberOfPendingPushSlots);

   self()->setStackAtlas(atlas);
   }

void
OMR::CodeGenerator::buildGCMapsForInstructionAndSnippet(TR::Instruction *instr)
   {
   if (instr->needsGCMap())
      {
      TR_GCStackMap * map = self()->buildGCMapForInstruction(instr);

      // The info bits are encoded in the register mask on the instruction (if there are any).
      //
      map->maskRegistersWithInfoBits(instr->getGCRegisterMask(), self()->getRegisterMapInfoBitsMask());
      instr->setGCMap(map);
      }

   if (instr->getSnippetForGC())
      {
      TR::Snippet * snippet = instr->getSnippetForGC();
      if (snippet && snippet->gcMap().isGCSafePoint() && !snippet->gcMap().getStackMap())
         {
         TR_GCStackMap *map = self()->buildGCMapForInstruction(instr);
         map->maskRegisters(snippet->gcMap().getGCRegisterMask());
         snippet->gcMap().setStackMap(map);
         }
      }
   }

TR_GCStackMap *
OMR::CodeGenerator::buildGCMapForInstruction(TR::Instruction *instr)
   {
   // Build a stack map with enough bits for parms, locals, and collectable temps
   //
   TR::Compilation *comp = self()->comp();
   TR::GCStackAtlas * atlas = self()->getStackAtlas();
   uint32_t numberOfSlots = atlas->getNumberOfSlotsMapped();

   TR_GCStackMap * map = new (self()->trHeapMemory(), numberOfSlots) TR_GCStackMap(numberOfSlots);

   TR_ASSERT(instr, "instruction is NULL\n");
   if (instr->getNode())
      {
      map->setByteCodeInfo(instr->getNode()->getByteCodeInfo());
      }
   else
      {
      TR_ByteCodeInfo byteCodeInfo;
      memset(&byteCodeInfo, 0, sizeof(TR_ByteCodeInfo));
      map->setByteCodeInfo(byteCodeInfo);
      }

   TR::ResolvedMethodSymbol * methodSymbol = comp->getMethodSymbol();

   // If the instruction has information about live locals, use it to build the
   // map for locals. Otherwise just assume all parms and locals are live.
   //
   if (!instr->getLiveLocals())
      {
      map->copy(atlas->getLocalMap());
      }
   else
      {
      map->copy(atlas->getParameterMap());
      }

   TR_BitVector * liveLocals = instr->getLiveLocals();
   TR_BitVector * liveMonitors = instr->getLiveMonitors();

   if (liveMonitors)
      {
      map->allocateLiveMonitorBits(self()->trMemory());
      }

   if (liveLocals || liveMonitors)
      {
      ListIterator<TR::AutomaticSymbol> automaticIterator(&methodSymbol->getAutomaticList());

      if (debug("traceLiveMonitors"))
         {
         if (liveMonitors)
            traceMsg(comp, "building monitor map for instr %p node %p\n", instr, instr->getNode());
         else
            traceMsg(comp, "no monitor map for instr %p node %p\n", instr, instr->getNode());
         }

      for (TR::AutomaticSymbol * localCursor = automaticIterator.getFirst();
           localCursor;
           localCursor = automaticIterator.getNext())
         {
         int32_t mapIndex = localCursor->getGCMapIndex();
         if (mapIndex >= 0 && unsigned(mapIndex) < atlas->getIndexOfFirstSpillTemp())
            {
            if (liveLocals && liveLocals->get(localCursor->getLiveLocalIndex()))
               {
               // If the local is a local object, map all the reference fields
               // inside it. Otherwise, map the local itself.
               //
               if (localCursor->isLocalObject())
                  {
                  }
               else if (localCursor->isCollectedReference() &&
                        !localCursor->isInternalPointer() &&
                        !localCursor->isPinningArrayPointer())
                  {
                  map->setBit(mapIndex);
                  }
               }

            if (_lmmdFailed && localCursor->holdsMonitoredObject())
               map->setBit(mapIndex); // make sure the slot is marked as live

            if (liveMonitors && liveMonitors->get(localCursor->getLiveLocalIndex()))
               {
               if (debug("traceLiveMonitors"))
                  traceMsg(comp, "setting map bit for local %p (%d) mapIndex %d\n", localCursor, localCursor->getLiveLocalIndex(), mapIndex);
               map->setLiveMonitorBit(mapIndex);
               map->setBit(mapIndex); // make sure the slot is marked as live
               }
            }
         }
      }

   // Build the map for spill temps
   //
   for(auto location = self()->getCollectedSpillList().begin(); location != self()->getCollectedSpillList().end(); ++location)
      {
      if ((*location)->containsCollectedReference() &&
          !(*location)->getSymbolReference()->getSymbol()->isInternalPointer() &&
          !(*location)->getSymbolReference()->getSymbol()->isPinningArrayPointer() &&
          (*location)->isOccupied())
         {
         TR::AutomaticSymbol * s = (*location)->getSymbolReference()->getSymbol()->getAutoSymbol();

         // For PPC/390/aarch64 OOL codegen; If a spill is in the collected list and it
         // has maxSpillDepth==0 then the following is true:
         //
         // 1) This GC point is in the hot path of an OOL section
         // 2) The spill has already been reversed
         // 3) The spill slot is still in the collected list because it needs to
         //    be protected for the cold path
         //
         // This means the slot is no longer live at this GC point and we should
         // skip it.  The occupied flag is not accurate in this case because we
         // did not free the spill and therefore did not clear the flag.
         //
         if ((self()->comp()->target().cpu.isPower() || self()->comp()->target().cpu.isZ() || self()->comp()->target().cpu.isARM64()) && (*location)->getMaxSpillDepth() == 0  && comp->cg()->isOutOfLineHotPath())
            {
            if (self()->getDebug())
               traceMsg(comp, "\nSkipping GC map [%p] index %d (%s) for instruction [%p] in OOL hot path because it has already been reverse spilled.\n",
                        map, s->getGCMapIndex(), self()->getDebug()->getName((*location)->getSymbolReference()), instr);
            continue;
            }

         TR_ASSERT(unsigned(s->getGCMapIndex()) >= atlas->getIndexOfFirstSpillTemp(), "Code Gen: error in building stack map");
         map->setBit(s->getGCMapIndex());
         }
      }

   // Build the register map
   //
   self()->buildRegisterMapForInstruction(map);
   return map;
   }

void
OMR::CodeGenerator::remapGCIndicesInInternalPtrFormat()
   {
   // Note that this does NOT handle the temps created by spilling correctly.
   // May require re-mapping indices AFTER register assignment.
   // Also note that as per the proposal we have to mark the base array temp AND the
   // internal pointer itself as scalar temps; so GC ignores them in its normal course.
   //
   TR::Compilation *comp = self()->comp();
   TR::GCStackAtlas * stackAtlas = self()->getStackAtlas();
   int32_t index = stackAtlas->getNumberOfSlotsMapped();
   TR::ResolvedMethodSymbol * methodSymbol = comp->getMethodSymbol();
   TR_InternalPointerMap * internalPtrMap = stackAtlas->getInternalPointerMap();
   stackAtlas->setIndexOfFirstInternalPointer(index);
   ListIterator<TR::AutomaticSymbol> automaticIterator(&methodSymbol->getAutomaticList());
   TR::AutomaticSymbol * localCursor;
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      // Map pinning arrays first; at this stage mark every pinning
      // array as being required only for internal pointers in registers.
      // When we examine internal pointer stack slots in next loop below,
      // we will remove those pinning arrays that are also required for
      // internal pointer stack slots from the 'only for internal pointer
      // regs' list
      //
      if (localCursor->isPinningArrayPointer())
         {
         localCursor->setGCMapIndex(index);
         int32_t roundedSize = (localCursor->getSize()+3)&(~3);
         if (roundedSize == 0)
            roundedSize = static_cast<int32_t>(TR::Compiler->om.sizeofReferenceAddress());
         index += roundedSize / static_cast<int32_t>(TR::Compiler->om.sizeofReferenceAddress());

         if (!localCursor->isInitializedReference())
            stackAtlas->setHasUninitializedPinningArrayPointer(true);

         if (!internalPtrMap)
            {
            internalPtrMap = new (self()->trHeapMemory()) TR_InternalPointerMap(self()->trMemory());
            stackAtlas->setInternalPointerMap(internalPtrMap);
            }
         stackAtlas->addPinningArrayPtrForInternalPtrReg(localCursor);
         }
      }

   //
   // Map internal pointers now
   //
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->isInternalPointer())
         {
         localCursor->setGCMapIndex(index);
         int32_t roundedSize = (localCursor->getSize()+3)&(~3);
         if (roundedSize == 0)
            roundedSize = static_cast<int32_t>(TR::Compiler->om.sizeofReferenceAddress());
         index += roundedSize / static_cast<int32_t>(TR::Compiler->om.sizeofReferenceAddress());

         if (!internalPtrMap)
            {
            internalPtrMap = new (self()->trHeapMemory()) TR_InternalPointerMap(self()->trMemory());
            stackAtlas->setInternalPointerMap(internalPtrMap);
            }

         TR::AutomaticSymbol * internalPtrCursor = localCursor->castToInternalPointerAutoSymbol();
         internalPtrMap->addInternalPointerPair(internalPtrCursor->getPinningArrayPointer(), internalPtrCursor);
         stackAtlas->removePinningArrayPtrForInternalPtrReg(internalPtrCursor->getPinningArrayPointer());
         }
      }

   self()->getStackAtlas()->setNumberOfSlotsMapped(index);
   }


bool
TR_InternalPointerMap::isInternalPointerMapIdenticalTo(TR_InternalPointerMap * map)
   {
   if (getNumInternalPointers() != map->getNumInternalPointers())
      return false;

   if (getNumDistinctPinningArrays() != map->getNumDistinctPinningArrays())
      return false;

   ListIterator<TR_InternalPointerPair> listIt(&_internalPtrPairs);
   ListIterator<TR_InternalPointerPair> list2It(&map->getInternalPointerPairs());
   for (TR_InternalPointerPair * pair = listIt.getFirst(); pair; pair = listIt.getNext())
      {
      bool found = false;
      list2It.reset();
      for (TR_InternalPointerPair * pair2 = list2It.getFirst(); pair2; pair2 = list2It.getNext())
         {
         if ((pair->getPinningArrayPointer() == pair2->getPinningArrayPointer()) &&
             (pair->getInternalPtrRegNum() == pair2->getInternalPtrRegNum()))
            {
            found = true;
            break;
            }
         }

      if (!found)
         return false;
      }

   return true;
   }

void
OMR::CodeGenerator::addToAtlas(TR::Instruction * instr)
   {
   TR::Compilation *comp = self()->comp();
   TR_GCStackMap * map = 0;
   if (instr->needsGCMap())
      {
      map = instr->getGCMap();
      }
   else if (comp->getOption(TR_GenerateCompleteInlineRanges) &&
            instr->getNode() && instr->getPrev() && instr->getPrev()->getNode() &&
            instr->getBinaryLength() > 0 &&
            (instr->getNode()->getByteCodeInfo().getCallerIndex() != instr->getPrev()->getNode()->getByteCodeInfo().getCallerIndex() ||
             0))
      {
      // find the previous instruction that contains a map and copy it with the current instruction's byte code info
      //
      for (TR::Instruction * prev = instr->getPrev(); prev; prev = prev->getPrev())
         {
         TR_GCStackMap * prevMap = prev->getGCMap();
         if (prevMap)
            {
            map = prevMap->clone(self()->trMemory());
            map->setByteCodeInfo(instr->getNode()->getByteCodeInfo());
            break;
            }
         }
      }

   if (map)
      {
      map->addToAtlas(instr, self());
      }
   }

void
TR_GCStackMap::addToAtlas(TR::Instruction * instruction, TR::CodeGenerator *cg)
   {
   // Fill in the code range and add this map to the atlas.
   //
   uint8_t * codeStart = cg->getCodeStart();
   setLowestCodeOffset(static_cast<uint32_t>(instruction->getBinaryEncoding() - codeStart));
   cg->getStackAtlas()->addStackMap(this);
   bool osrEnabled = cg->comp()->getOption(TR_EnableOSR);
   if (osrEnabled)
      cg->addToOSRTable(instruction);
   }

void
TR_GCStackMap::addToAtlas(uint8_t * callSiteAddress, TR::CodeGenerator *cg)
   {
   // Fill in the code range and add this map to the atlas.
   //
   uint32_t callSiteOffset = static_cast<uint32_t>(callSiteAddress - cg->getCodeStart());
   setLowestCodeOffset(callSiteOffset - 1);
   cg->getStackAtlas()->addStackMap(this);
   bool osrEnabled = cg->comp()->getOption(TR_EnableOSR);
   if (osrEnabled)
      cg->addToOSRTable(callSiteOffset, getByteCodeInfo());
   }
