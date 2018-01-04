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

#ifndef OSRDATA_INCL
#define OSRDATA_INCL

#include <stdint.h>                         // for int32_t, uint32_t, etc
#include <map>
#include "cs2/hashtab.h"                    // for HashTable
#include "env/TRMemory.hpp"                 // for TR_Memory, etc
#include "env/jittypes.h"                   // for TR_ByteCodeInfo
#include "infra/Array.hpp"                  // for TR_Array
#include "infra/deque.hpp"                  // for TR_Array
#include "infra/Checklist.hpp"              // for TR::NodeCheckList
#include "infra/vector.hpp"                 // for TR::vector

class TR_BitVector;
class TR_OSRMethodData;
class TR_OSRSlotSharingInfo;
namespace TR { class Block; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
template <class T> class List;

namespace TR
   {
   enum HCRMode
      {
      none,
      osr,
      traditional
      };

/*
 * An OSR transition can occur before or after the OSR point's
 * side-effect, depending on the current mode.
 *
 * In preExecutionOSR, the transition occurs before the side-effects
 * of executing the OSR point's bytecode, resulting in it being
 * evaluated in the interpreter.
 *
 * In postExecutionOSR, the transition occurs after the side-effects
 * of executing the OSR point's bytecode, resulting in the interpreter
 * resuming execution at following bytecode.
 *
 * It is possible for both of these modes to be enabled at once.
 */
   enum OSRTransitionTarget
      {
      disableOSR = 0,
      preExecutionOSR = 1,
      postExecutionOSR = 2,
      preAndPostExecutionOSR = 3
      };

/*
 * OSR can operate in two modes, voluntary and involuntary.
 *
 * In involuntary OSR, the JITed code does not control when an OSR transition occurs. It can be
 * initiated externally at any potential OSR point.
 *
 * In voluntary OSR, the JITed code does control when an OSR transition occurs, allowing it to
 * limit the OSR points with transitions.
 */
   enum OSRMode
      {
      voluntaryOSR,
      involuntaryOSR
      };
   }

typedef TR::typed_allocator<std::pair<int32_t , TR_BitVector*>, TR::Region&> DefiningMapAllocator;
typedef std::less<int32_t> DefiningMapComparator;
typedef std::map<int32_t, TR_BitVector*, DefiningMapComparator, DefiningMapAllocator> DefiningMap;

typedef TR::vector<DefiningMap *, TR::Region&> DefiningMaps;

/**
 * \page OSR On Stack Replacement (OSR)
 *
 * There is one `TR_OSRCompilationData` object per `TR::Compilation` object
 * and it keeps OSR information common to the whole compilation. It has,
 * `osrMethodDataArray`, an array of `TR_OSRMethodData`, that has OSR
 * information specific to each of the inlined methods (including the root
 * method).
 *
 * `TR_OSRCompilationData` and `TR_OSRMethodData` serve three purposes:
 *
 * 1. Record enough information during optimizations such that we know, given a
 *    instruction PC offset and a set of symbols that share slot(s), which symbol is
 *    live at that offset. This is needed so that we can popluate the OSR buffer.
 *
 * 2. Write the information in (1) into the meta data.
 *
 * 3. Check that size of OSR frame, OSR scratch buffer, and OSR stack frame do not
 *    exceed a specified threshold.
 *
 *
 * OSR code block
 * --------------
 *
 * There is one OSR code block for the root method and one for each of the inlined
 * methods.  It's a basic block that initially (i.e., during ILGen) contains a call
 * to `prepareForOSR` which has all the autos and pending push temps of its
 * corresponding method as its children. Eventually, i.e., in `CodeGenPrep.cpp`, the
 * call will be replaced by
 *
 * 1. A number of treetops that store all symbols that do not share slots to the
 *    OSR buffer, and a number of treetops that store all symbols that share slots to
 *    the OSR scratch buffer.
 *
 * 2. A call to a C helper called `_prepareForOSR` (defined in
 *    runtime/CRuntimeImpl.cpp). This helper uses meta data information to find out,
 *    given a instruction PC offset and a set of symbols that share slot(s), the
 *    symbol that is live at that offset. The symbol is read from the OSR scratch
 *    buffer and written to the OSR buffer.
 *
 * During inlining, we link OSR code blocks in the sense that, each OSR code block
 * will have a goto to the OSR code block of its caller. The OSR code block of the
 * root method will have a jump to the VM.
 *
 * `symRefOrder`
 * -------------
 *
 * Each inlined method (including the root method), has two arrays:
 * one for its autos and one for its pending push temps. Each element of the array
 * is a sequence of symrefs. If a symref doesn't share its slot with another
 * symref, the sequence contains only one element, otherwise, the sequence contains
 * the list of all symrefs that share the same slot(s).
 *
 * In the former case, the symRefOrder of such a symRef is assumed to be -1. In
 * the latter case, the symRefOrder of a symref is set to be the index of the
 * symref in that sequence (starting from zero).
 *
 */

class TR_OSRCompilationData
   {
   public:
   TR_ALLOC(TR_Memory::OSR);

   TR_OSRCompilationData(TR::Compilation* comp);
   const TR_Array<TR_OSRMethodData *> &getOSRMethodDataArray() const { return osrMethodDataArray; }
   TR_OSRMethodData *findOSRMethodData(int32_t inlinedSiteIndex, TR::ResolvedMethodSymbol *methodSymbol);
   TR_OSRMethodData *findOrCreateOSRMethodData(int32_t inlinedSiteIndex, TR::ResolvedMethodSymbol *methodSymbol);
   TR_OSRMethodData *findCallerOSRMethodData(TR_OSRMethodData *callee);
   void addSlotSharingInfo(const TR_ByteCodeInfo& T, int32_t slot, int32_t symRefNum,
                           int32_t symRefOrder, int32_t symSize, bool takesTwoSlots);
   void ensureSlotSharingInfoAt(const TR_ByteCodeInfo& T);
   void addInstruction(TR::Instruction* instr);
   void addInstruction(int32_t instructionPC, TR_ByteCodeInfo bcInfo);

   void checkOSRLimits();

   uint32_t getMaxScratchBufferSize() const {return maxScratchBufferSize;}
   void setMaxScratchBufferSize(uint32_t _maxScratchBufferSize);

   uint32_t getSizeOfMetaData() const;
   uint32_t getSizeOfInstruction2SharedSlotMap() const;
   uint32_t getSizeOfCallerIndex2OSRCatchBlockMap() const;

   uint32_t writeMetaData(uint8_t* buffer) const;
   uint32_t writeInstruction2SharedSlotMap(uint8_t* buffer) const;
   uint32_t writeCallerIndex2OSRCatchBlockMap(uint8_t* buffer) const;

   void finishComputingOSRData();
   void setOSRMethodDataArraySize(int newSize);

   void setSeenClassPreventingInducedOSR() { _classPreventingInducedOSRSeen = true; }
   bool seenClassPreventingInducedOSR() { return _classPreventingInducedOSRSeen; }

   int32_t getNumOfSymsThatShareSlot() {return numOfSymsThatShareSlot;}
   void updateNumOfSymsThatShareSlot(int32_t value) {numOfSymsThatShareSlot += value;}
   void buildSymRefOrderMap();
   int32_t getSymRefOrder(int32_t symRefNumber);

   void buildDefiningMap();
   void buildFinalMap(int32_t callerIndex,
                      DefiningMap *finalMap,
                      DefiningMap *workingCatchBlockMap,
                      DefiningMaps &definingSymRefsMapAtOSRCodeBlocks, 
                      DefiningMaps &symRefNumberMapForPrepareForOSRCalls
                      );

   class TR_ScratchBufferInfo
      {
      public:
      TR_ScratchBufferInfo(int32_t _inlinedSiteIndex, int32_t _osrBufferOffset,
                           int32_t _scratchBufferOffset, int32_t _symSize) :
         inlinedSiteIndex(_inlinedSiteIndex), osrBufferOffset(_osrBufferOffset),
         scratchBufferOffset(_scratchBufferOffset), symSize(_symSize) {};
      bool operator==(const TR_ScratchBufferInfo& info) const;
      int32_t writeToBuffer(uint8_t* buffer) const;
      int32_t inlinedSiteIndex;
      int32_t osrBufferOffset;
      int32_t scratchBufferOffset;
      int32_t symSize;
      };

   typedef TR_Array<TR_ScratchBufferInfo> TR_ScratchBufferInfos;


   /**
    * This class is basically a mapping from a given instruction PC offset to information that
    * that enables to copy values of symbols that share slot(s) from OSR scratch buffer to OSR buffer.
    * This information is deduced from live symbols at that instruction pc.
    * More formally, it represents a pair (i, s) where i is an instruction PC and s is an array
    * of `TR_ScratchBufferInfo`.
    */
   class TR_Instruction2SharedSlotMapEntry
      {
      public:
      TR_Instruction2SharedSlotMapEntry(int32_t _instructionPC, const TR_ScratchBufferInfos& _scratchBufferInfos) :
         instructionPC(_instructionPC), scratchBufferInfos(_scratchBufferInfos) {}
      TR_Instruction2SharedSlotMapEntry(const TR_Instruction2SharedSlotMapEntry& entry):
         instructionPC(entry.instructionPC), scratchBufferInfos(entry.scratchBufferInfos) {}
      TR_Instruction2SharedSlotMapEntry& operator=(const TR_Instruction2SharedSlotMapEntry& entry)
         {
         instructionPC = entry.instructionPC;
         scratchBufferInfos = entry.scratchBufferInfos;
         return *this;
         }
      int32_t instructionPC;
      TR_ScratchBufferInfos scratchBufferInfos;
      };

   void addInstruction2SharedSlotMapEntry(int32_t instructionPC, const TR_ScratchBufferInfos& infos);
   void compressInstruction2SharedSlotMap();

   friend TR::Compilation& operator<< (TR::Compilation&, const TR_OSRCompilationData&);

   private:
   typedef TR::deque<TR_Instruction2SharedSlotMapEntry, TR::Region&> TR_Instruction2SharedSlotMap;

   /// a mapping from the symref's reference number to symRefOrder
   /// (see above for the definition of symRefOrder)
   typedef CS2::HashTable<int32_t, int32_t, TR::Allocator> TR_SymRefOrderMap;
   uint32_t getOSRStackFrameSize(uint32_t methodIndex);

   //auxiliary method called only by buildSymRefOrderMap
   void buildSymRefOrderMapAux(TR_Array<List<TR::SymbolReference> >* symListArray);
   TR_SymRefOrderMap                 symRefOrderMap;
   TR_Instruction2SharedSlotMap      instruction2SharedSlotMap;
   //an array of TR_OSRMethodData, one element for each inlined method and one for the root method
   TR_Array<TR_OSRMethodData *>      osrMethodDataArray;
   TR::Compilation*                   comp;
   uint32_t                          maxScratchBufferSize;

   /// Number of sets of symbols that share slots in all inlined
   /// methods, e.g., if symbols 'a' and 'b' share a slot and symbols
   /// 'c' and 'd' share a slot, the following field is set to 2.
   int32_t                           numOfSymsThatShareSlot;

   bool                              _classPreventingInducedOSRSeen;
};


class TR_OSRMethodData
   {
   public:
   TR_ALLOC(TR_Memory::OSR);

   TR_OSRMethodData(int32_t _inlinedSiteIndex, TR::ResolvedMethodSymbol *_methodSymbol,
      TR_OSRCompilationData* _osrCompilationData);
   int32_t getInlinedSiteIndex() const { return inlinedSiteIndex; }
   TR::ResolvedMethodSymbol *getMethodSymbol() const { return methodSymbol; }
   TR::Block * findOrCreateOSRCodeBlock(TR::Node* n);
   TR::Block * findOrCreateOSRCatchBlock(TR::Node* n);
   TR::Block *getOSRCodeBlock() const { return osrCodeBlock; }
   TR::Block *getOSRCatchBlock() const {return osrCatchBlock;}
   bool inlinesAnyMethod() const;
   void setNumOfSymsThatShareSlot(int32_t newValue);
   int32_t getNumOfSymsThatShareSlot() {return numOfSymsThatShareSlot;}
   void addSlotSharingInfo(int32_t byteCodeIndex,
                           int32_t slot, int32_t symRefNum, int32_t symRefOrder, int32_t symSize, bool takesTwoSlots);
   void ensureSlotSharingInfoAt(int32_t byteCodeIndex);
   bool hasSlotSharingOrDeadSlotsInfo();
   void addInstruction(int32_t instructionPC, int32_t byteCodeIndex);

   int32_t getHeaderSize() const;
   int32_t getTotalNumOfSlots() const;
   int32_t getTotalDataSize() const;
   int32_t slotIndex2OSRBufferIndex(int32_t slotIndex, int symSize, bool takesTwoSlots) const;
   void addScratchBufferOffset(int32_t slotIndex, int32_t symRefOrder, int32_t scratchBufferOffset);
   bool isEmpty() const {return bcInfoHashTab.IsEmpty();}
   TR::Compilation* comp() const;

   void setNumSymRefs(int32_t numBits) {_numSymRefs = numBits; }
   int32_t getNumSymRefs() { return _numSymRefs; }

   void addLiveRangeInfo(int32_t byteCodeIndex, TR_BitVector *liveRangeInfo);
   TR_BitVector *getLiveRangeInfo(int32_t byteCodeIndex);

   void addPendingPushLivenessInfo(int32_t byteCodeIndex, TR_BitVector *livenessInfo);
   TR_BitVector *getPendingPushLivenessInfo(int32_t byteCodeIndex);

   void ensureArgInfoAt(int32_t byteCodeIndex, int32_t argNum);
   void addArgInfo(int32_t byteCodeIndex, int32_t argIndex, int32_t argSymRef);
   TR_Array<int32_t>* getArgInfo(int32_t byteCodeIndex);

   bool linkedToCaller() { return _linkedToCaller; }
   void setLinkedToCaller(bool b) { _linkedToCaller = b; }

   void buildDefiningMap(TR::Block *block, DefiningMap *blockDefiningMap, DefiningMap *prepareForOSRCallMap = NULL);
   void buildDefiningMapForBlock(TR::Block *block, DefiningMap *blockMap);
   void buildDefiningMapForOSRCodeBlockAndPrepareForOSRCall(TR::Block *block, DefiningMap *osrCodeBlockMap, DefiningMap *prepareForOSRCallMap);
   DefiningMap* getDefiningMap();
   void collectSubTreeSymRefs(TR::Node *node, TR_BitVector *subTreeSymRefs, TR::NodeChecklist &checklist);
   void setSymRefs(TR_BitVector *symRefs) { _symRefs = symRefs; }
   TR_BitVector *getSymRefs() { return _symRefs; }

   friend TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRMethodData& osrMethodData);

   private:
   void createOSRBlocks(TR::Node* n);

   typedef CS2::HashTable<int32_t, TR_BitVector *, TR::Allocator> TR_BCLiveRangeInfoHashTable;
   typedef CS2::HashTable<int32_t, TR_OSRSlotSharingInfo*, TR::Allocator> TR_BCInfoHashTable;
   typedef CS2::HashTable<int32_t, TR_Array<int32_t>*, TR::Allocator> TR_ArgInfoHashTable;
   typedef CS2::HashTable<int32_t, TR_Array<int32_t>, TR::Allocator> TR_Slot2ScratchBufferOffset;


   /// method symbol corresponding to this method
   TR::ResolvedMethodSymbol       *methodSymbol; // NOTE: This field must appear first, because its comp() is used to initialize other fields

   /// a mapping from shared slot indices to an array of offsets in the scratch buffer.
   /// For each shared slot, there is a set of two or more syms that share that slot. The elements
   /// of the array above is the same as this set. The elements of the array are indexed by their symRefOrder.
   TR_Slot2ScratchBufferOffset  slot2ScratchBufferOffset;

   /// a hash table from bytecode
   TR_BCInfoHashTable           bcInfoHashTab;

   TR_BCLiveRangeInfoHashTable  bcLiveRangeInfoHashTab;
   TR_BCLiveRangeInfoHashTable  bcPendingPushLivenessInfoHashTab;
   DefiningMap                  *_symRefDefiningMap; 
   TR_BitVector                 *_symRefs;

   TR_ArgInfoHashTable argInfoHashTab;

   int32_t _numSymRefs;

   /// the index of the root method inlining table in which this method appears
   int32_t                      inlinedSiteIndex;
   TR::Block                     *osrCodeBlock;

   /// This is an empty catch block right before the osr code block, i.e., it falls through to the osr code block.
   /// All incoming exception edges (from normal blocks) should go to the osr catch block and
   /// all incoming normal edges (from callees code blocks) should go to the osr code block.
   TR::Block                     *osrCatchBlock;

   /// Number of sets of symbols that share slots in this method
   int32_t                      numOfSymsThatShareSlot;

   /// points to the owning object
   TR_OSRCompilationData*       osrCompilationData;

   bool _linkedToCaller;
   };


class TR_OSRPoint
   {
   public:
   TR_ALLOC(TR_Memory::OSR);

   TR_OSRPoint(TR_ByteCodeInfo &bcInfo, TR_OSRMethodData *methodData, TR_Memory *m);
   TR_OSRMethodData *getOSRMethodData() { return _methodData; }

   void setOSRIndex(uint32_t index) { _index = index; }
   uint32_t getOSRIndex() { return _index; }
   TR_ByteCodeInfo& getByteCodeInfo() {return _bcInfo;}

   private:
   TR_OSRMethodData                   *_methodData;
   uint32_t                            _index;
   TR_ByteCodeInfo                     _bcInfo;
   };


class TR_OSRSlotSharingInfo
   {
public:
   TR_ALLOC(TR_Memory::OSR);
   TR_OSRSlotSharingInfo(TR::Compilation* _comp);
   void addSlotInfo(int32_t slot, int32_t symRefNum, int32_t symRefOrder, int32_t symSize, bool takesTwoSlots);
   class TR_SlotInfo
      {
      public:
      TR_SlotInfo(int32_t _slot, int32_t _symRefNum, int32_t _symRefOrder, int32_t _symSize, bool _takesTwoSlots) :
         slot(_slot), symRefNum(_symRefNum), symRefOrder(_symRefOrder), symSize(_symSize),
         takesTwoSlots(_takesTwoSlots) {};
      int32_t slot;
      int32_t symRefNum;
      int32_t symRefOrder;
      int32_t symSize;
      bool takesTwoSlots;
      };
   const TR_Array<TR_SlotInfo>& getSlotInfos() {return slotInfos;}
   friend TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRSlotSharingInfo*);
private:
   TR_Array<TR_SlotInfo> slotInfos;
   TR::Compilation* comp;
   };

TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRCompilationData& osrData);


#endif
