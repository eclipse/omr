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

#ifndef OMR_GCSTACKATLAS_INCL
#define OMR_GCSTACKATLAS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_GCSTACKATLAS_CONNECTOR
#define OMR_GCSTACKATLAS_CONNECTOR
namespace OMR { class GCStackAtlas; }
namespace OMR { typedef OMR::GCStackAtlas GCStackAtlasConnector; }
#endif

#include "codegen/GCStackMap.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/List.hpp"
#include "infra/Annotations.hpp"

class TR_GCStackAllocMap;
class TR_GCStackMap;
class TR_InternalPointerMap;
namespace TR { class GCStackAtlas; }
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }

namespace OMR
{

class OMR_EXTENSIBLE GCStackAtlas
   {

   public:

   TR_ALLOC(TR_Memory::GCStackAtlas)

   GCStackAtlas(uint32_t numParms, uint32_t numSlots, TR_Memory * m)
      : _atlasBits(NULL),
        _numberOfParmSlotsMapped(numParms),
        _numberOfSlotsMapped(numSlots),
#ifdef J9_PROJECT_SPECIFIC
        _numberOfPaddingSlots(0),
#endif
        _numberOfSlotsToBeInitialized(0),
        _numberOfPendingPushSlots(0),
        _parmBaseOffset(0),
        _trMemory(m),
        _localBaseOffset(0),
        _numberOfMaps(0),
        _parameterMap(NULL),
        _localMap(NULL),
        _indexOfFirstInternalPtr(0),
        _offsetOfFirstInternalPtr(0),
        _internalPointerMap(NULL),
        _pinningArrayPtrsForInternalPtrRegs(m),
        _stackAllocMap(NULL),
        _mapList(m),
        _hasUninitializedPinningArrayPointer(false) {}

   TR::GCStackAtlas * self();

   TR_Memory *trMemory() { return _trMemory; }

   uint8_t *getAtlasBits() { return _atlasBits; }
   uint8_t *setAtlasBits(uint8_t *p) { return (_atlasBits = p); }

   uint32_t getNumberOfParmSlotsMapped() { return _numberOfParmSlotsMapped; }

   uint32_t getNumberOfLocalSlotsMapped() { return _numberOfSlotsMapped-_numberOfParmSlotsMapped; }

   uint32_t getNumberOfSlotsMapped() { return _numberOfSlotsMapped; }
   uint32_t setNumberOfSlotsMapped(uint32_t n) { return (_numberOfSlotsMapped = n); }

#ifdef J9_PROJECT_SPECIFIC
   uint32_t getNumberOfPaddingSlots()          {return _numberOfPaddingSlots;}
   uint32_t setNumberOfPaddingSlots(uint32_t n) {return (_numberOfPaddingSlots = n);}
#endif

   uint32_t getNumberOfSlotsToBeInitialized() { return _numberOfSlotsToBeInitialized; }
   uint32_t setNumberOfSlotsToBeInitialized(uint32_t n) { return (_numberOfSlotsToBeInitialized = n); }

   uint32_t getNumberOfPendingPushSlots() { return _numberOfPendingPushSlots; }
   uint32_t setNumberOfPendingPushSlots(uint32_t n) { return (_numberOfPendingPushSlots = n); }

   uint32_t getIndexOfFirstSpillTemp() { return _indexOfFirstSpillTemp; }
   uint32_t setIndexOfFirstSpillTemp(uint32_t n) { return (_indexOfFirstSpillTemp = n); }
   uint32_t assignGCIndex() { return _numberOfSlotsMapped++; }

   int32_t getParmBaseOffset() { return _parmBaseOffset; }
   int32_t setParmBaseOffset(int32_t n) { return (_parmBaseOffset = n); }

   int32_t getLocalBaseOffset() { return _localBaseOffset; }
   int32_t setLocalBaseOffset(int32_t n) { return (_localBaseOffset = n); }

   uint32_t getNumberOfMaps() { return _numberOfMaps; }
   uint32_t setNumberOfMaps(uint32_t n) { return (_numberOfMaps = n); }
   void decNumberOfMaps() { _numberOfMaps--; }

   TR_GCStackMap *getParameterMap() { return _parameterMap; }
   TR_GCStackMap *setParameterMap(TR_GCStackMap *map) { return (_parameterMap = map); }

   TR_GCStackMap *getLocalMap() { return _localMap; }
   TR_GCStackMap *setLocalMap(TR_GCStackMap *map) { return (_localMap = map); }

   int32_t getIndexOfFirstInternalPointer() { return _indexOfFirstInternalPtr; }
   int32_t setIndexOfFirstInternalPointer(int32_t n) { return (_indexOfFirstInternalPtr = n); }

   int32_t getOffsetOfFirstInternalPointer() { return _offsetOfFirstInternalPtr; }
   int32_t setOffsetOfFirstInternalPointer(int32_t n) { return (_offsetOfFirstInternalPtr = n); }

   TR_InternalPointerMap *getInternalPointerMap() { return _internalPointerMap; }
   TR_InternalPointerMap *setInternalPointerMap(TR_InternalPointerMap *map) { return (_internalPointerMap = map); }

   TR_GCStackAllocMap *getStackAllocMap() { return _stackAllocMap; }
   TR_GCStackAllocMap *setStackAllocMap(TR_GCStackAllocMap *map) { return (_stackAllocMap = map); }

   uint32_t getNumberOfDistinctPinningArrays();

   List<TR::AutomaticSymbol>& getPinningArrayPtrsForInternalPtrRegs() { return _pinningArrayPtrsForInternalPtrRegs; }
   void addPinningArrayPtrForInternalPtrReg(TR::AutomaticSymbol *symbol)
      {
      if (!_pinningArrayPtrsForInternalPtrRegs.find(symbol))
         _pinningArrayPtrsForInternalPtrRegs.add(symbol);
      }

   void removePinningArrayPtrForInternalPtrReg(TR::AutomaticSymbol *symbol)
      {
      _pinningArrayPtrsForInternalPtrRegs.remove(symbol);
      }

   bool hasUninitializedPinningArrayPointer() { return _hasUninitializedPinningArrayPointer; }
   void setHasUninitializedPinningArrayPointer(bool b) { _hasUninitializedPinningArrayPointer = b; }

   List<TR_GCStackMap>& getStackMapList() { return _mapList; }

   void close(TR::CodeGenerator *);

   void addStackMap(TR_GCStackMap *m);

   private:

   uint8_t *_atlasBits;
   TR_GCStackMap *_parameterMap;
   TR_GCStackMap *_localMap;
   TR_InternalPointerMap *_internalPointerMap;
   TR_GCStackAllocMap *_stackAllocMap;
   TR_Memory *_trMemory;

   List<TR::AutomaticSymbol> _pinningArrayPtrsForInternalPtrRegs;
   List<TR_GCStackMap> _mapList;

   uint32_t _numberOfParmSlotsMapped;// span of the parameter map
   uint32_t _numberOfSlotsMapped;    // total span of the map
#ifdef J9_PROJECT_SPECIFIC
   uint32_t _numberOfPaddingSlots;   // number of padding slots used to align local objects
#endif
   uint32_t _numberOfSlotsToBeInitialized; // number of locals that
   uint32_t _numberOfPendingPushSlots; // number of locals that
                                                // need to be initialized to null
   uint32_t _indexOfFirstSpillTemp;
   int32_t _parmBaseOffset;     // measured from the frame pointer for the method
   int32_t _localBaseOffset;    // measured from the frame pointer for the method
   int32_t _indexOfFirstInternalPtr;
   int32_t _offsetOfFirstInternalPtr;
   uint32_t _numberOfMaps;

   bool _hasUninitializedPinningArrayPointer;

   };

}

#endif
