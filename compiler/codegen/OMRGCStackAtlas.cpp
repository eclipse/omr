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

#include "codegen/OMRGCStackAtlas.hpp"  // for GCStackAtlas

#include <stdint.h>                               // for int32_t, uint32_t, etc
#include <string.h>                               // for memcmp
#include "codegen/CodeGenerator.hpp"              // for CodeGenerator
#include "codegen/GCStackAtlas.hpp"               // for TR::GCStackAtlas
#include "codegen/GCStackMap.hpp"                 // for TR_GCStackMap, etc
#include "compile/Compilation.hpp"                // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "infra/List.hpp"                         // for List, ListIterator, etc
#include "ras/Debug.hpp"                          // for TR_DebugBase

namespace TR { class AutomaticSymbol; }

TR::GCStackAtlas *
OMR::GCStackAtlas::self()
   {
   return static_cast<TR::GCStackAtlas *>(this);
   }

void
OMR::GCStackAtlas::close(TR::CodeGenerator *cg)
   {
   // Dump the atlas before merging. The atlas after merging is given by the
   // dump of the external atlas
   //
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_TraceCG))
      {
      comp->getDebug()->print(comp->getOutFile(), self());
      }

   // Merge adjacent similar maps
   //
   bool merge = !comp->getOption(TR_DisableMergeStackMaps);
   uint8_t *start = cg->getCodeStart();
   ListElement<TR_GCStackMap> *mapEntry, *next;
   TR_GCStackMap *map, *nextMap;

   for (mapEntry = _mapList.getListHead(); mapEntry; mapEntry = next)
      {
      next = mapEntry->getNextElement();
      map = mapEntry->getData();

      // See if the next map can be merged with this one.
      // If they have the same contents, the ranges are merged and a single map
      // represents both ranges.
      //
      if (!next)
         {
         continue;
         }

      nextMap = next->getData();

      int32_t mapBytes = map->getMapSizeInBytes();
      if (mapBytes == nextMap->getMapSizeInBytes() &&
          map->getRegisterMap() == nextMap->getRegisterMap() &&
          map->getHighWordRegisterMap() == nextMap->getHighWordRegisterMap() &&
          !memcmp(map->getMapBits(), nextMap->getMapBits(), mapBytes) &&
          (comp->getOption(TR_DisableShrinkWrapping) ||
           (map->getRegisterSaveDescription() == nextMap->getRegisterSaveDescription())) &&
          map->isByteCodeInfoIdenticalTo(nextMap))
         {
         // Maps are the same - can merge
         //
         map->setLowestCodeOffset(nextMap->getLowestCodeOffset());
         _mapList.removeNext(mapEntry);
         _numberOfMaps--;
         next = mapEntry;
         }
      }
   }


void
OMR::GCStackAtlas::addStackMap(TR_GCStackMap *m)
   {
   // Put the map entry in the correct place in the atlas.
   // If the map has the same offset and stack map as an existing map, merge
   // them into a single map by merging the register maps.
   // (this happens for a resolve and a null check on the same instruction).
   //
   ListElement<TR_GCStackMap> * mapEntry = _mapList.getListHead();
   if (!mapEntry)
      {
      _mapList.add(m);
      }
   else
      {
      TR_GCStackMap * map = mapEntry->getData();

      if (m->getLowestCodeOffset() > map->getLowestCodeOffset())
         {
         _mapList.add(m);
         }
      else
         {
         int32_t mapBytes = m->getMapSizeInBytes();
         ListElement<TR_GCStackMap> * prev = 0;
         while (mapEntry)
            {
            map = mapEntry->getData();
            if (m->getLowestCodeOffset() == map->getLowestCodeOffset() &&
                mapBytes == map->getMapSizeInBytes() &&
                !memcmp(m->getMapBits(), map->getMapBits(), mapBytes) &&
                ((m->getLiveMonitorBits() != 0) == (map->getLiveMonitorBits() != 0) &&
                 (m->getLiveMonitorBits() == 0 ||
                  !memcmp(m->getLiveMonitorBits(), map->getLiveMonitorBits(), mapBytes))) &&
                ((!m->getInternalPointerMap() &&
                  !map->getInternalPointerMap()) ||
                 (m->getInternalPointerMap() &&
                  map->getInternalPointerMap() &&
                  map->isInternalPointerMapIdenticalTo(m))))
               {
               map->setRegisterBits(m->getRegisterMap());
               map->setHighWordRegisterBits(m->getHighWordRegisterMap());

               // Adjust for the fact that we're not adding a map
               //
               --_numberOfMaps;
               break;
               }
            if (m->getLowestCodeOffset() > map->getLowestCodeOffset())
               {
               _mapList.addAfter(m, prev);
               break;
               }
            prev = mapEntry;
            mapEntry = mapEntry->getNextElement();
            }

         if (!mapEntry)
            {
            _mapList.addAfter(m, prev);
            }
         }
      }

   ++_numberOfMaps;
   if (m->getNumberOfSlotsMapped() > self()->getNumberOfSlotsMapped())
      {
      self()->setNumberOfSlotsMapped(m->getNumberOfSlotsMapped());
      }
   }


uint32_t
OMR::GCStackAtlas::getNumberOfDistinctPinningArrays()
   {
   uint32_t numDistinctPinningArrays = 0;
   if (self()->getInternalPointerMap())
      {
      // First collect all pinning arrays that are the base for at least
      // one derived internal pointer stack slot
      //
      List<TR_InternalPointerPair> seenInternalPtrPairs(self()->trMemory());
      List<TR::AutomaticSymbol> seenPinningArrays(self()->trMemory());
      ListIterator<TR_InternalPointerPair> internalPtrIt(&self()->getInternalPointerMap()->getInternalPointerPairs());

      for (
         TR_InternalPointerPair * internalPtrPair = internalPtrIt.getFirst();
         internalPtrPair;
         internalPtrPair = internalPtrIt.getNext())
         {
         bool seenPinningArrayBefore = false;
         ListIterator<TR_InternalPointerPair> seenInternalPtrIt(&seenInternalPtrPairs);
         for (
            TR_InternalPointerPair * seenInternalPtrPair = seenInternalPtrIt.getFirst();
            seenInternalPtrPair && (seenInternalPtrPair != internalPtrPair);
            seenInternalPtrPair = seenInternalPtrIt.getNext())
            {
            if (internalPtrPair->getPinningArrayPointer() == seenInternalPtrPair->getPinningArrayPointer())
               {
               seenPinningArrayBefore = true;
               break;
               }
            }

         if (!seenPinningArrayBefore)
            {
            seenPinningArrays.add(internalPtrPair->getPinningArrayPointer());
            seenInternalPtrPairs.add(internalPtrPair);
            numDistinctPinningArrays++;
            }
         }

      // Now collect all pinning arrays that are the base for only
      // internal pointers in registers
      //
      ListIterator<TR::AutomaticSymbol> autoIt(&self()->getPinningArrayPtrsForInternalPtrRegs());
      TR::AutomaticSymbol * autoSymbol;
      for (autoSymbol = autoIt.getFirst(); autoSymbol; autoSymbol = autoIt.getNext())
         {
         if (!seenPinningArrays.find(autoSymbol))
            {
            seenPinningArrays.add(autoSymbol);
            numDistinctPinningArrays++;
            }
         }

      }

   return numDistinctPinningArrays;
   }
