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

#ifndef GCSTACKMAP_INCL
#define GCSTACKMAP_INCL


#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "codegen/GCRegisterMap.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/LabelSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"

#define DEFAULT_BYTES_OF_MAP_BITS       4
#define DEFAULT_NUMBER_OF_SLOTS_MAPPED 32

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }

class TR_InternalPointerPair
   {
   TR::AutomaticSymbol *_pinningArrayPtr;
   union
      {
      int32_t              _internalPtrRegNum;
      TR::AutomaticSymbol  *_internalPtrAuto;
      };

public :

   TR_ALLOC(TR_Memory::InternalPointerPair)

   TR_InternalPointerPair(TR::AutomaticSymbol *pinningArrayPtr, TR::AutomaticSymbol *internalPtr)
      : _pinningArrayPtr(pinningArrayPtr),
        _internalPtrAuto(internalPtr)
      {
      TR_ASSERT(!internalPtr || internalPtr->isInternalPointerAuto(), "should be internal pointerauto");
      }

   TR_InternalPointerPair(TR::AutomaticSymbol *pinningArrayPtr, int32_t internalPtrRegNum)
      : _pinningArrayPtr(pinningArrayPtr),
        _internalPtrRegNum(internalPtrRegNum)
      {
      }

   TR_InternalPointerPair *clone(TR_Memory * m)
      {
      return new (m->trHeapMemory()) TR_InternalPointerPair(_pinningArrayPtr, _internalPtrAuto);
      }

   TR::AutomaticSymbol *getPinningArrayPointer() {return _pinningArrayPtr;}
   void setPinningArrayPointer(TR::AutomaticSymbol *pinningArrayPointer) {_pinningArrayPtr = pinningArrayPointer;}
   TR::AutomaticSymbol *getInternalPointerAuto() {return _internalPtrAuto;}
   void setInternalPointerAuto(TR::AutomaticSymbol *internalPointerAuto) { TR_ASSERT(!internalPointerAuto || internalPointerAuto->isInternalPointerAuto(), "should be internal pointerauto"); _internalPtrAuto = internalPointerAuto; }
   int32_t getInternalPtrRegNum() {return _internalPtrRegNum;}
   void setInternalPtrRegNum(int32_t internalPtrRegNum) {_internalPtrRegNum = internalPtrRegNum;}
   };


class TR_InternalPointerMap
   {
   TR_Memory *_trMemory;
   List<TR_InternalPointerPair> _internalPtrPairs;
   int32_t _numDistinctPinningArrays;
   int32_t _size;
   uint8_t _numInternalPtrs;

public:

   TR_ALLOC(TR_Memory::InternalPointerMap)

   TR_InternalPointerMap(TR_Memory * m)
      : _trMemory(m),
        _numInternalPtrs(0),
        _numDistinctPinningArrays(0),
        _internalPtrPairs(m),
        _size(0) {}

   TR_Memory *   trMemory()     { return _trMemory; }
   TR_HeapMemory trHeapMemory() { return trMemory(); }

   TR_InternalPointerMap *clone()
      {
      TR_InternalPointerMap * newIPtrMap = new (trHeapMemory()) TR_InternalPointerMap(trMemory());
      ListIterator<TR_InternalPointerPair> pairIt(&_internalPtrPairs);

      for (TR_InternalPointerPair * pair=pairIt.getFirst(); pair; pair=pairIt.getNext())
      newIPtrMap->addInternalPointerPair(pair->clone(trMemory()));

      newIPtrMap->setNumDistinctPinningArrays(_numDistinctPinningArrays);

      return newIPtrMap;
      }

   List<TR_InternalPointerPair>& getInternalPointerPairs() {return _internalPtrPairs;}

   void addInternalPointerPair(TR_InternalPointerPair *internalPtrPair)
     {
     _numInternalPtrs++;
     _internalPtrPairs.add(internalPtrPair);
     }

   void addInternalPointerPair(TR::AutomaticSymbol *pinningArrayPtr, int32_t internalPtrRegNum)
     {
     addInternalPointerPair(new (trHeapMemory()) TR_InternalPointerPair(pinningArrayPtr, internalPtrRegNum));
     }

   void addInternalPointerPair(TR::AutomaticSymbol *pinningArrayPtr, TR::AutomaticSymbol *internalPtr)
     {
     addInternalPointerPair(new (trHeapMemory()) TR_InternalPointerPair(pinningArrayPtr, internalPtr));
     }

   bool isInternalPointerMapIdenticalTo(TR_InternalPointerMap *);

   int32_t getNumDistinctPinningArrays() {return _numDistinctPinningArrays;}
   void setNumDistinctPinningArrays(int32_t n) {_numDistinctPinningArrays = n;}

   int32_t getNumInternalPointers() {return _numInternalPtrs;}
   void setNumInternalPointers(int32_t n) {_numInternalPtrs = n;}

   int32_t getSize() {return _size;}
   void setSize(int32_t n) {_size = n;}
   };


class TR_GCStackMap
   {
public:

   TR_ALLOC_WITHOUT_NEW(TR_Memory::GCStackMap)

   TR_GCStackMap(uint32_t slots)
      : _lowestCodeOffset(0),
        _lowestOffsetInstruction(0),
        _internalPointerMap(0),
        _liveMonitorBits(0),
        _numberOfSlotsMapped(slots)
      {
      clearBits();
      _byteCodeInfo.setZeroByteCodeIndex();
      _byteCodeInfo.setInvalidCallerIndex();
      _byteCodeInfo.setDoNotProfile(0);
      }

   void * operator new(size_t s, TR_HeapMemory m) {return m.allocate(s);}
   void * operator new(size_t s, TR_HeapMemory m, uint32_t numberOfSlotsToMap)
      {
      if (numberOfSlotsToMap > DEFAULT_NUMBER_OF_SLOTS_MAPPED)
         {
         s += ((numberOfSlotsToMap - DEFAULT_NUMBER_OF_SLOTS_MAPPED) + 7) >> 3; // round up to the next byte and then convert from bits to bytes
         }
      return m.allocate(s);
      }

   void operator delete(void *gcStackMap, TR_HeapMemory m, uint32_t numberOfSlotsToMap)
      {
          m.deallocate(gcStackMap);
      }

   void allocateLiveMonitorBits(TR_Memory * m)
      {
      _liveMonitorBits = (uint8_t *)m->allocateHeapMemory(getMapSizeInBytes());
      memset(_liveMonitorBits, 0, getMapSizeInBytes());
      }

   uint8_t *getMapBits() {return _mapBits;}
   int32_t getMapSizeInBytes() {return (_numberOfSlotsMapped+7) >> 3;}

   uint8_t *getLiveMonitorBits() {return _liveMonitorBits;}

   uint32_t getLowestCodeOffset() {return _lowestCodeOffset;}
   uint32_t setLowestCodeOffset(uint32_t n) {return (_lowestCodeOffset = n);}

   TR_ByteCodeInfo &getByteCodeInfo() { return _byteCodeInfo; }
   void setByteCodeInfo(TR_ByteCodeInfo bci) { _byteCodeInfo = bci; }

   TR::Instruction *getLowestOffsetInstruction() { return _lowestOffsetInstruction; }
   TR::Instruction *setLowestOffsetInstruction(TR::Instruction *i) { return (_lowestOffsetInstruction = i); }

   TR_InternalPointerMap *getInternalPointerMap() {return _internalPointerMap;}
   TR_InternalPointerMap *setInternalPointerMap(TR_InternalPointerMap *map) {return (_internalPointerMap = map);}

   uint32_t getNumberOfSlotsMapped() { return _numberOfSlotsMapped; }

   bool isInternalPointerMapIdenticalTo(TR_GCStackMap *map)
      {
      return _internalPointerMap->isInternalPointerMapIdenticalTo(map->getInternalPointerMap());
      }

   bool isByteCodeInfoIdenticalTo(TR_GCStackMap *map)
      {
      return ((_byteCodeInfo.getCallerIndex() == map->getByteCodeInfo().getCallerIndex()) && (_byteCodeInfo.getByteCodeIndex() == map->getByteCodeInfo().getByteCodeIndex()) && (_byteCodeInfo.doNotProfile() == map->getByteCodeInfo().doNotProfile()));
      }

   void     setRegisterBits(uint32_t bits)    {_registerMap.setRegisterBits(bits);}
   void     resetRegistersBits(uint32_t bits) {_registerMap.resetRegisterBits(bits);}
   uint32_t getRegisterMap()               {return _registerMap.getMap();}
   void     clearRegisterMap()             {_registerMap.empty();}
   void     maskRegisters(uint32_t mask)   {_registerMap.maskRegisters(mask);}
   void     maskRegistersWithInfoBits(uint32_t mask,
                                      uint32_t info) {_registerMap.maskRegistersWithInfoBits(mask, info);}
   void     setInfoBits(uint32_t info) {_registerMap.setInfoBits(info);}

   void setBit(int32_t bitNumber)   { _mapBits[bitNumber >> 3] |= 1 << (bitNumber & 7); }
   bool isSet(int32_t bitNumber)    { return (_mapBits[bitNumber >> 3] & (1 << (bitNumber & 7))) != 0; }
   void resetBit(int32_t bitNumber) { _mapBits[bitNumber >> 3] &= ~(1 << (bitNumber & 7)); }

   void setLiveMonitorBit(int32_t bitNumber)   { _liveMonitorBits[bitNumber >> 3] |= 1 << (bitNumber & 7); }

   void clearBits()
      {
      if (getMapSizeInBytes())
         {
         memset(_mapBits, 0, getMapSizeInBytes());

         if (_liveMonitorBits)
            {
            memset(_liveMonitorBits, 0, getMapSizeInBytes());
            }
         }
      }

   void copy(TR_GCStackMap *other)
      {
      if (other->getMapSizeInBytes())
         {
         memcpy(_mapBits, other->_mapBits, other->getMapSizeInBytes());
         }
      }

   TR_GCStackMap *clone(TR_Memory * m)
      {
      TR_GCStackMap *newMap = new (m->trHeapMemory(), _numberOfSlotsMapped) TR_GCStackMap(_numberOfSlotsMapped);

      if (_internalPointerMap)
         newMap->setInternalPointerMap(_internalPointerMap->clone());
      newMap->setByteCodeInfo(getByteCodeInfo());
      newMap->copy(this);
      if (_liveMonitorBits)
         {
         newMap->allocateLiveMonitorBits(m);
         memcpy(newMap->_liveMonitorBits, getLiveMonitorBits(), getMapSizeInBytes());
         }
      newMap->setRegisterBits(getRegisterMap());
      return newMap;
      }

   void addToAtlas(TR::Instruction *instruction, TR::CodeGenerator *cg);
   void addToAtlas(uint8_t *callSiteAddress, TR::CodeGenerator *cg);

private:
   friend class TR_Debug;

   TR::Instruction *_lowestOffsetInstruction;

   TR_InternalPointerMap *_internalPointerMap;

   uint32_t _lowestCodeOffset; // measured from the beginning of the method
   uint32_t _numberOfSlotsMapped; // total span of the map

   TR::GCRegisterMap _registerMap;

   TR_ByteCodeInfo _byteCodeInfo;

   uint8_t *_liveMonitorBits;

   /*
    * WARNING: This array appears to be of variable length.
    * Before putting fields after this, or deriving from this class,
    * Ensure that doing so is correct.
    */
   uint8_t _mapBits[DEFAULT_BYTES_OF_MAP_BITS];
   };



class TR_GCStackAllocMap
   {
public:

   TR_ALLOC_WITHOUT_NEW(TR_Memory::GCStackMap)

   TR_GCStackAllocMap(uint32_t slots)
      :  _numberOfSlotsMapped(slots)
      {
      clearBits();
      }

   void * operator new(size_t s, TR_HeapMemory m) {return m.allocate(s);}
   void * operator new(size_t s, TR_HeapMemory m, uint32_t numberOfSlotsToMap)
      {
      if (numberOfSlotsToMap > DEFAULT_NUMBER_OF_SLOTS_MAPPED)
         {
         s += ((numberOfSlotsToMap - DEFAULT_NUMBER_OF_SLOTS_MAPPED) + 7) >> 3; // round up to the next byte and then convert from bits to bytes
         }
      return m.allocate(s);
      }

   void operator delete (void *, TR_HeapMemory m) {}
   void operator delete (void *, TR_HeapMemory m, uint32_t numberOfSlotsToMap) {}

   uint8_t *getMapBits()  {return _mapBits;}
   int32_t  getMapSizeInBytes()  {return (_numberOfSlotsMapped+7) >> 3;}


   uint32_t getNumberOfSlotsMapped()      {return _numberOfSlotsMapped;}

   void setBit(int32_t bitNumber)   { _mapBits[bitNumber >> 3] |= 1 << (bitNumber & 7); }
   bool isSet(int32_t bitNumber)    { return (_mapBits[bitNumber >> 3] & (1 << (bitNumber & 7))) != 0; }
   void resetBit(int32_t bitNumber) { _mapBits[bitNumber >> 3] &= ~(1 << (bitNumber & 7)); }


   void clearBits()
      {
      if (getMapSizeInBytes())
         {
         memset(_mapBits, 0, getMapSizeInBytes());
         }
      }

   void copy(TR_GCStackAllocMap *other)
      {
      if (other->getMapSizeInBytes())
         memcpy(_mapBits, other->_mapBits, other->getMapSizeInBytes());
      }

private:
   friend class TR_Debug;
   uint32_t _numberOfSlotsMapped; // total span of the map

   /*
    * WARNING: This array appears to be of variable length.
    * Before putting fields after this, or deriving from this class,
    * Ensure that doing so is correct.
    */
   uint8_t _mapBits[DEFAULT_BYTES_OF_MAP_BITS];
   };

#endif
