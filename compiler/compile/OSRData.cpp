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

#include "compile/OSRData.hpp"

#include <algorithm>                           // for std::max
#include <stdint.h>                            // for int32_t, uint32_t
#include <stdio.h>                             // for NULL, sprintf
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd, etc
#include "codegen/Instruction.hpp"             // for Instruction
#include "compile/Compilation.hpp"             // for Compilation
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"                 // for ObjectModel
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"                      // for intptrj_t
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"                        // for Block
#include "il/Node.hpp"                         // for Node
#include "il/Node_inlines.hpp"                 // for getInt
#include "il/Symbol.hpp"                       // for Symbol
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/Cfg.hpp"                       // for CFG
#include "infra/List.hpp"                      // for List, ListIterator

TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRSlotSharingInfo* osrSlotSharingInfo);

TR_OSRCompilationData::TR_OSRCompilationData(TR::Compilation* _comp) :
   instruction2SharedSlotMap(_comp->trMemory()->heapMemoryRegion()),
   comp(_comp),
   _classPreventingInducedOSRSeen(false),
   osrMethodDataArray(_comp->trMemory()),
   symRefOrderMap(_comp->allocator()),
   numOfSymsThatShareSlot(0), maxScratchBufferSize(0)
   {};

/**
  This is top-level method that is called by the OSR def analysis
  Each call to this method means that the symbol with symref number of "symRefNum" is live
  at the location given by bcinfo.
  That symref is in slot "slot", has symRefOrder "symRefOrder", its size is "symSize" and
  it takes two slots if "takesTwoSlots" is true.
*/
void
TR_OSRCompilationData::addSlotSharingInfo(const TR_ByteCodeInfo& bcinfo,
   int32_t slot, int32_t symRefNum, int32_t symRefOrder, int32_t symSize, bool takesTwoSlots)
   {
   //we'll add the symref to the inlined method that corresponds to the location given by bcinfo.
   getOSRMethodDataArray()[bcinfo.getCallerIndex()+1]->
      addSlotSharingInfo(bcinfo.getByteCodeIndex(), slot, symRefNum, symRefOrder, symSize, takesTwoSlots);
   }

void
TR_OSRCompilationData::ensureSlotSharingInfoAt(const TR_ByteCodeInfo& bcinfo)
   {
   TR_OSRMethodData* osrMethodData = getOSRMethodDataArray()[bcinfo.getCallerIndex()+1];
   if (osrMethodData && osrMethodData->getOSRCodeBlock())
      osrMethodData->ensureSlotSharingInfoAt(bcinfo.getByteCodeIndex());
   }

/**
 * This is the top-level method that is called by the codegen to populate
 * the maps that are eventually written to meta data.
 *
 * In voluntary OSR, only induce OSR calls must be added to the
 * OSR table, as these are the only points that will transition.
 * However, under involuntary OSR, all points must support
 * transitions.
 */
void
TR_OSRCompilationData::addInstruction(TR::Instruction* instr)
   {
   TR::Node *node = instr->getNode();
   if (comp->getOSRMode() == TR::voluntaryOSR
       && !(node
         && node->getOpCode().hasSymbolReference()
         && node->getSymbolReference()->getReferenceNumber() == TR_induceOSRAtCurrentPC))
      return;

   int32_t instructionPC = instr->getBinaryEncoding() - instr->cg()->getCodeStart();
   TR_ByteCodeInfo& bcInfo = instr->getNode()->getByteCodeInfo();
   addInstruction(instructionPC, bcInfo);
   }

void
TR_OSRCompilationData::addInstruction(int32_t instructionPC, TR_ByteCodeInfo bcInfo)
   {
   bool trace = comp->getOption(TR_TraceOSR);
   if (trace)
      traceMsg(comp, "instructionPC %x callerIndex %d bcidx %d ", instructionPC,
              bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
   if (instructionPC < 0)
      {
      if (trace)
         traceMsg(comp, "  rejected: instructionPC %d < 0\n", instructionPC);
      return;
      }
   // Algorithm:
   // 1) p = callsite/bytecode point that correspond to the instruction
   // 2) Find the live shared slots at p (by calling osrMethodData->addInstruction(...) below)
   // 3) if p is at the root method exit.
   // 4) p = caller point of p
   // 5) goto 2

   while (1)
      {
      if (bcInfo.getCallerIndex()+1 >= getOSRMethodDataArray().size())
         {
         if (trace)
            traceMsg(comp, "  rejected: caller index %d +1 >= %d\n", bcInfo.getCallerIndex(), getOSRMethodDataArray().size());
         break;
         }
      TR_OSRMethodData* osrMethodData = getOSRMethodDataArray()[bcInfo.getCallerIndex()+1];
      if (!osrMethodData || !osrMethodData->getOSRCodeBlock())
         {
         if (trace)
            traceMsg(comp, "  rejected: no osrMethodData\n");
         break;
         }
      TR_ASSERT(getNumOfSymsThatShareSlot() >= 0, "number of symbols that share slots must be non-negative; actually %d", getNumOfSymsThatShareSlot());
      if (getNumOfSymsThatShareSlot() == 0)
         {
         if (trace)
            traceMsg(comp, "  rejected: no slot-sharing symbols in CompilationData\n");
         break;
         }

      osrMethodData->addInstruction(instructionPC, bcInfo.getByteCodeIndex());
      if (bcInfo.getCallerIndex() == -1)
         break;
      bcInfo = comp->getInlinedCallSite(bcInfo.getCallerIndex())._byteCodeInfo;
      if (trace)
         traceMsg(comp, "  callerIndex %d bcidx %d ", bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
      }
   }

TR_OSRMethodData *
TR_OSRCompilationData::findOSRMethodData(int32_t inlinedSiteIndex, TR::ResolvedMethodSymbol *methodSymbol)
   {
   int32_t index = inlinedSiteIndex + 1;
   TR_OSRMethodData *osrMethodData = osrMethodDataArray.isEmpty() ? NULL : osrMethodDataArray[index];

   if (osrMethodData &&
       osrMethodData->getInlinedSiteIndex() == inlinedSiteIndex &&
       osrMethodData->getMethodSymbol() == methodSymbol)
      return osrMethodData;

   return NULL;
   }

// TODO: there is currently an implicit assumption that we are calling findOrCrete from within
//       the inliner where the current compilation is at the correct depth to generate the
//       inlinedSiteIndex - we should factor inlinedSiteIndex into the API to avoid accidental
//       calls from other locations unaware of the implicit assumption here in
TR_OSRMethodData *
TR_OSRCompilationData::findOrCreateOSRMethodData(int32_t inlinedSiteIndex, TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR_OSRMethodData *osrMethodData = findOSRMethodData(inlinedSiteIndex, methodSymbol);

   if (osrMethodData)
      return osrMethodData;

   int32_t index = inlinedSiteIndex + 1;
   osrMethodData = new (comp->trHeapMemory()) TR_OSRMethodData(inlinedSiteIndex, methodSymbol, this);
   if (comp->getOption(TR_TraceOSR))
      traceMsg(comp, "osrMethodData index %d created\n", index); // TODO: Print methodSymbol
   osrMethodDataArray[index] = osrMethodData;
   return osrMethodData;
   }

TR_OSRMethodData *
TR_OSRCompilationData::findCallerOSRMethodData(TR_OSRMethodData *callee)
   {
   TR_InlinedCallSite &ics = comp->getInlinedCallSite(callee->getInlinedSiteIndex());
   int32_t callerIndex = ics._byteCodeInfo.getCallerIndex();
   TR_OSRMethodData *osrMethodData = osrMethodDataArray[callerIndex+1];
   return osrMethodData;
   }


void
TR_OSRCompilationData::addInstruction2SharedSlotMapEntry(
   int32_t instructionPC,
   const TR_ScratchBufferInfos& infos)
   {
   auto cur = instruction2SharedSlotMap.begin(), end = instruction2SharedSlotMap.end();
   while (cur != end)
      {
      if (instructionPC <= (*cur).instructionPC)
         break;
      ++cur;
      }

   if (cur != end && instructionPC == (*cur).instructionPC)
      (*cur).scratchBufferInfos.append(infos);
   else
      instruction2SharedSlotMap.insert(cur, TR_Instruction2SharedSlotMapEntry(instructionPC, infos));
   }

/*
  Reduces the size of the instruction2SharedSlot map by merging consecutive equal elements.
 */
void
TR_OSRCompilationData::compressInstruction2SharedSlotMap()
   {
   auto cur = instruction2SharedSlotMap.begin();
   while (cur != instruction2SharedSlotMap.end())
      {
      auto next = cur + 1;

      for (; next != instruction2SharedSlotMap.end(); ++next)
         {
         const TR_ScratchBufferInfos& curInfo = (*cur).scratchBufferInfos;
         const TR_ScratchBufferInfos& nextInfo = (*next).scratchBufferInfos;
         if (curInfo.size() != nextInfo.size())
            break;

         int j;
         for (j = 0; j < curInfo.size(); j++)
            if (!(curInfo[j] == nextInfo[j]))
               break;
         if (j != curInfo.size())
            break;
         }

      if (cur + 1 != next)
         cur = instruction2SharedSlotMap.erase(cur + 1, next);
      else
         ++cur;
      }
   }

/*
  During inlining, osrMethodDataArray array is grown during genIL of methods that the inliner
  intends to inline. However, at some point after genIL is finished, the inliner may decide not to inline
  that method. In that case, we need to remove the elements of osrMethodDataArray that were created
  for that method (and any method it inlines).
 */
void
TR_OSRCompilationData::setOSRMethodDataArraySize(int newSize)
   {
   osrMethodDataArray.setSize(newSize);
   }

//computes the size of the meta data
uint32_t
TR_OSRCompilationData::getSizeOfMetaData() const
   {
   uint32_t size = 0;
   //section 0
   if (comp->getOption(TR_DisableOSRSharedSlots))
      {
      size += sizeof(uint32_t);
      }
   else
      {
      size += getSizeOfInstruction2SharedSlotMap();
      }
   //section 1
   size += getSizeOfCallerIndex2OSRCatchBlockMap();
   return size;
   }

//computes size of section 0
uint32_t
TR_OSRCompilationData::getSizeOfInstruction2SharedSlotMap() const
   {
   int size = 0;
   size += sizeof(uint32_t); //number of bytes used to store the whole section
   size += sizeof(uint32_t); //maximum size of scratch buffer
   size += sizeof(int32_t); //number of mappings
   for (auto itr = instruction2SharedSlotMap.begin(), end = instruction2SharedSlotMap.end(); itr != end; ++itr)
      {
      size += sizeof(int32_t); //instructionPC
      //storing the number of elements of the array (which is represented as the data of the hash table
      size += sizeof(int32_t);
      size += sizeof(TR_ScratchBufferInfo) * (*itr).scratchBufferInfos.size(); //data
      }
   return size;
   }

//computes size of section 1
uint32_t
TR_OSRCompilationData::getSizeOfCallerIndex2OSRCatchBlockMap() const
   {
   int size = 0;
   size += sizeof(uint32_t); //number of bytes for the rest of this section
   size += sizeof(int32_t); //number of mappings
   size += sizeof(uint32_t) //osrCatchBlockAddress
      * getOSRMethodDataArray().size();
   return size;
   }

/*
   This is the top-level method that writes OSR-specific meta data.
   The meta data has two sections:
   0) a mapping from instruction PC offsets to a list of shared slots that are live at that offset.
   1) a mapping from from the call site index to OSR catch block
   The first 4 bytes of each section is the size of the section in bytes (including the size field itself)
*/
uint32_t
TR_OSRCompilationData::writeMetaData(uint8_t* buffer) const
   {
   uint32_t numberOfBytesWritten = 0;
   TR_ASSERT(comp->getOption(TR_EnableOSR), "OSR must be enabled for TR_OSRCompilationData::writeMetaData");
   //section 0
   if (comp->getOption(TR_DisableOSRSharedSlots))
      {
      //write an empty section so that we always have two sections regardless of whether
      //osr def analysis is enabled or not
      uint32_t sectionSize = sizeof(uint32_t);
      *((uint32_t*)(buffer+numberOfBytesWritten)) = sectionSize;
      numberOfBytesWritten += sizeof(uint32_t);
      }
   else
      {
      numberOfBytesWritten += writeInstruction2SharedSlotMap(buffer + numberOfBytesWritten);
      }

   //section 1
   numberOfBytesWritten += writeCallerIndex2OSRCatchBlockMap(buffer + numberOfBytesWritten);
/*
   traceMsg(comp, "%%%%%%%%%%%%%%%%%%% OSR MetaData %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
   for (int i = 0; i < numberOfBytesWritten; i+=4)
      {
      traceMsg(comp, "%010x ", *(uint32_t*)(buffer + i));
      if ((i % 16) == 12) traceMsg(comp, "\n");
      }
   traceMsg(comp, "\n");
*/
   return numberOfBytesWritten;
   }

/*
  This function writes section 0 of the meta data. This section has the following fields:
  - size of section 0
  - maximum size of the scratch buffer
  - number of mappings in the instruction->(list of live shared slots) map
  - for each mapping
    - instruction PC offset
    - for i = 1 to (size of list of live shared slots)
      - callsite index that the shared slot belongs to
      - offset in the osr buffer that the shared slot corresponds to. This is the slot that needs to be
        written to by the jit helper
      - offset in the scratch buffer that the live shared slot corresponds to. This is the slot that
        needs to be read by the jit helper
      - symSize: size of the symbol that the shared slot corresponds to.
  The function returns the number of bytes written.
 */

uint32_t
TR_OSRCompilationData::writeInstruction2SharedSlotMap(uint8_t* buffer) const
   {
   uint8_t* initialBuffer = buffer;
   uint32_t sectionSize = getSizeOfInstruction2SharedSlotMap();
   *((uint32_t*)buffer) = sectionSize; buffer += sizeof(uint32_t);
   *((uint32_t*)buffer) = getMaxScratchBufferSize(); buffer += sizeof(uint32_t);
   int32_t numberOfMappings = instruction2SharedSlotMap.size();
   *((int32_t*)buffer) = numberOfMappings; buffer += sizeof(int32_t);
   for (auto itr = instruction2SharedSlotMap.begin(), end = instruction2SharedSlotMap.end(); itr != end; ++itr)
      {
      *((int32_t*)buffer) = (*itr).instructionPC; buffer += sizeof(int32_t);
      *((int32_t*)buffer) = (*itr).scratchBufferInfos.size(); buffer += sizeof(int32_t);
      for (int j = 0; j < (*itr).scratchBufferInfos.size(); j++)
         {
         const TR_ScratchBufferInfo info = (*itr).scratchBufferInfos[j];
         buffer += info.writeToBuffer(buffer);
         }
      }
   uint32_t numberOfBytesWritten = buffer - initialBuffer;
   TR_ASSERT(numberOfBytesWritten == sectionSize, "numberOfBytesWritten (%d) must match sectionSize (%d)", numberOfBytesWritten, sectionSize);
   return numberOfBytesWritten;
   }

/*
  This function writes section 1 of the meta data. This section has the following fields:
  - size of section 1
  - number of mappings in the (callsite index)->(OSR catch block) map
  - for each mapping
    - instruction PC of the OSR catch block. Note that the call site index is implicit (starting from -1).
  The function returns the number of bytes written.
 */

uint32_t
TR_OSRCompilationData::writeCallerIndex2OSRCatchBlockMap(uint8_t* buffer) const
   {
   uint8_t* initialBuffer = buffer;
   uint32_t sectionSize = getSizeOfCallerIndex2OSRCatchBlockMap();
   *((uint32_t*)buffer) = sectionSize; buffer += sizeof(uint32_t);

   int32_t numberOfMappings = getOSRMethodDataArray().size();
   *((int32_t*)buffer) = numberOfMappings; buffer += sizeof(int32_t);
   for (int i = 0; i < numberOfMappings; i++)
      {
      TR_OSRMethodData* osrMethodData = getOSRMethodDataArray()[i];
      if (osrMethodData == NULL || osrMethodData->getOSRCodeBlock() == NULL)
         *((int32_t*)buffer) = 0;
      else
         *((int32_t*)buffer) = osrMethodData->getOSRCatchBlock()->getInstructionBoundaries()._startPC; buffer += sizeof(int32_t);
      }
   uint32_t numberOfBytesWritten = buffer - initialBuffer;
   TR_ASSERT(numberOfBytesWritten == sectionSize, "numberOfBytesWritten (%d) must match sectionSize (%d)", numberOfBytesWritten, sectionSize);
   return numberOfBytesWritten;
   }

/// This method computes the sizes of all of the various OSR parameters and requests the VM to accomodate the JIT
/// if the VM replies that it cannot accommodate the compile we are doing we abort the compile. It is NEVER safe
/// to toggle our OSR state during a compile so this method will either return on success or call outofmemory on failure
///
void TR_OSRCompilationData::checkOSRLimits()
   {
   uint32_t maxFrameSize = 0;
   uint32_t maxStackFrameSize = 0;
   int32_t numOfInlinedCalls = comp->getNumInlinedCallSites();

   {
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   uint32_t *frameSizes = (uint32_t *) comp->trMemory()->allocateStackMemory(numOfInlinedCalls * sizeof(uint32_t));
   uint32_t *stackFrameSizes = (uint32_t *) comp->trMemory()->allocateStackMemory(numOfInlinedCalls * sizeof(uint32_t));
   uint32_t rootFrameSize = TR::Compiler->vm.OSRFrameSizeInBytes(comp, comp->getCurrentMethod()->getPersistentIdentifier());
   uint32_t rootStackFrameSize = getOSRStackFrameSize(0);
   for (int i = 0; i < numOfInlinedCalls; ++i)
      {
      frameSizes[i] = 0;
      stackFrameSizes[i] = 0;
      }

   for (int i = 0; i < numOfInlinedCalls; ++i)
      {
      TR_InlinedCallSite &callSite = comp->getInlinedCallSite(i);
      TR_ByteCodeInfo &bcInfo = callSite._byteCodeInfo;
      frameSizes[i] = TR::Compiler->vm.OSRFrameSizeInBytes(comp, callSite._methodInfo);
      frameSizes[i] += (bcInfo.getCallerIndex() == -1) ? rootFrameSize : frameSizes[bcInfo.getCallerIndex()];
      stackFrameSizes[i] = getOSRStackFrameSize(i + 1);
      stackFrameSizes[i] = (bcInfo.getCallerIndex() == -1) ? rootStackFrameSize : stackFrameSizes[bcInfo.getCallerIndex()];
      }

   maxFrameSize = rootFrameSize;
   maxStackFrameSize = rootStackFrameSize;
   for (int i = 0; i < numOfInlinedCalls; ++i)
      {
      maxFrameSize = std::max(maxFrameSize, frameSizes[i]);
      maxStackFrameSize = std::max(maxStackFrameSize, stackFrameSizes[i]);
      }

   //getFrameSizeInBytes() is what is determined only after code generation and that's probably why
   //we perform this check against the threshold after code generation.
   maxStackFrameSize += comp->cg()->getFrameSizeInBytes();

   } // scope of the stack memory region

   if (!TR::Compiler->vm.ensureOSRBufferSize(comp, maxFrameSize, getMaxScratchBufferSize(), maxStackFrameSize))
      {
      if (comp->getOptions()->getAnyOption(TR_TraceAll) || comp->getOption(TR_TraceOSR))
         traceMsg(comp, "OSR COMPILE ABORT: frame size %d, scratch size %d, stack size %d were not accommodated by the VM\n",
                  maxFrameSize, getMaxScratchBufferSize(), maxStackFrameSize);
      if (comp->getOptions()->isAnyVerboseOptionSet(TR_VerboseOSR, TR_VerboseOSRDetails))
         TR_VerboseLog::writeLineLocked(TR_Vlog_OSR, "OSR COMPILE ABORT in %s: frame size %d, scratch size %d, stack size %d were not accommodated by the VM\n",
                  comp->signature(), maxFrameSize, getMaxScratchBufferSize(), maxStackFrameSize);
      comp->failCompilation<TR::CompilationException>("OSR buffers could not be enlarged to accomodate compiled method");
      }
   }

void
TR_OSRCompilationData::setMaxScratchBufferSize(uint32_t newSize)
   {
   maxScratchBufferSize = std::max(maxScratchBufferSize, newSize);
   }

/**
 * Computes the size of the stack frame needed for OSR operations for a single method
 *
*/
uint32_t TR_OSRCompilationData::getOSRStackFrameSize(uint32_t methodIndex)
   {
   //when methodIndex corresponds, for example, to a JNI method, the corresponding element
   //in OSRMethodDataArray doesn't exist or is NULL.
   if (methodIndex < getOSRMethodDataArray().size())
      {
      TR_OSRMethodData* osrMethodData = getOSRMethodDataArray()[methodIndex];
      if (osrMethodData != NULL)
         return (1 + osrMethodData->getMethodSymbol()->getNumParameterSlots())
            * TR::Compiler->om.sizeofReferenceAddress();
      else
         return 0;
      }
   else
      return 0;
   }


static void printMap(DefiningMap *map, TR::Compilation *comp)
   {
   for (auto it = map->begin(); it != map->end(); ++it)
      {
      traceMsg(comp, "# %d:", it->first);
      it->second->print(comp);
      traceMsg(comp, "\n");
      }
   }

/*
 * \brief This is the top level function to start building defining maps for the symbol references
 * under prepareForOSRCall for each method
 *
 * DefiningMap maps each symRef to the set of symRefs that define it in one block or several 
 * contiguous blocks. 
 *
 * There are 4 DefiningMaps for each methods and their ranges are different:
 *    - osrCatchBlock: starts from the osrCatchBlock and stops before the osrCodeBlock of THIS caller
 *    - osrCodeBlock: starts from the osrCodeBlock and stops before the osrCodeBlock of the NEXT caller
 *    - prepareForOSR: starts from the osrCodeBlock and stops at prepareForOSR call
 *    - FinalMap: this one maps all the symbol references under each prepareForOSR call from the current
 *      caller up until the top most caller and the is the only map published to the OSRMethodMetaData
 *
 * Combining the FinalMap with liveness info from OSRLiveRangeAnalysis, we are able to find which 
 * symbols should be kept alive at each osrPoints.
 */

void TR_OSRCompilationData::buildDefiningMap()
   {
   const TR_Array<TR_OSRMethodData *>& methodDataArray = getOSRMethodDataArray();
   int numOfMethods = methodDataArray.size();

   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   DefiningMaps definingMapAtOSRCatchBlocks(numOfMethods, static_cast<DefiningMap*>(NULL), comp->trMemory()->currentStackRegion());
   DefiningMaps definingMapAtOSRCodeBlocks(numOfMethods, static_cast<DefiningMap*>(NULL), comp->trMemory()->currentStackRegion());
   DefiningMaps definingMapAtPrepareForOSRCalls(numOfMethods, static_cast<DefiningMap*>(NULL), comp->trMemory()->currentStackRegion());

   for (intptrj_t i = 0; i < methodDataArray.size(); ++i)
      {
      TR_OSRMethodData *osrMethodData = methodDataArray[i];
      if (!osrMethodData)
         continue;

      //Those 2 bool variables are necessary for identifying the cases where the OSRCode/CatchBlock is already 
      //removed by the optimizer but osrMethodData still points to the block that's no longer in the CFG
      bool osrCatchBlockRemoved = false;
      bool osrCodeBlockRemoved = false;

      TR::Block *osrCatchBlock = osrMethodData->getOSRCatchBlock();
      if (osrCatchBlock && osrCatchBlock->getExceptionPredecessors().size() > 0)
         {
         definingMapAtOSRCatchBlocks[i] = new DefiningMap(DefiningMapComparator(), DefiningMapAllocator(comp->trMemory()->currentStackRegion()));
         osrMethodData->buildDefiningMapForBlock(osrCatchBlock, definingMapAtOSRCatchBlocks[i]);
         }
      else osrCatchBlockRemoved = true;

      TR::Block *osrCodeBlock = osrMethodData->getOSRCodeBlock();
      if (osrCodeBlock && osrCodeBlock->getPredecessors().size() > 0)
         {
         definingMapAtOSRCodeBlocks[i] = new DefiningMap(DefiningMapComparator(), DefiningMapAllocator(comp->trMemory()->currentStackRegion()));
         definingMapAtPrepareForOSRCalls[i] = new DefiningMap(DefiningMapComparator(), DefiningMapAllocator(comp->trMemory()->currentStackRegion()));
         osrMethodData->buildDefiningMapForOSRCodeBlockAndPrepareForOSRCall(osrCodeBlock, definingMapAtOSRCodeBlocks[i], definingMapAtPrepareForOSRCalls[i]);
         }
      else osrCodeBlockRemoved = true;
   
      if (!osrCatchBlockRemoved && !osrCodeBlockRemoved )
         buildFinalMap(i-1, osrMethodData->getDefiningMap(), definingMapAtOSRCatchBlocks[i], definingMapAtOSRCodeBlocks, definingMapAtPrepareForOSRCalls);
      }

   if (comp->getOption(TR_TraceOSR))
      {
      for (intptrj_t i = 0; i < methodDataArray.size(); ++i)
         {
         TR_OSRMethodData *osrMethodData = methodDataArray[i];
         if (!osrMethodData)
            continue;
         DefiningMap *definingMap = osrMethodData->getDefiningMap();
         if (osrMethodData->getOSRCatchBlock())
            {
            traceMsg(comp, "final map for OSRCatchBlock(block_%d): \n", osrMethodData->getOSRCatchBlock()->getNumber());
            printMap(definingMap, comp);
            }
         }
      }
   }

/* 
 * \brief This function replace the symbols in \p subTreeSymRefs with the symbols defining them according to the \p DefiningMap
 *
 * @param subTreeSymRefs original symbols
 * @param DefiningMap maps each symbol to the group of symbols it depends on
 * @param resultSymRefs updated symbols with original symbols replaced with its defining symbols
 */
static void updateDefiningSymRefs(TR_BitVector *subTreeSymRefs, DefiningMap *definingMap, TR_BitVector *resultSymRefs)
   {
   TR_BitVectorIterator cursor(*subTreeSymRefs);
   while (cursor.hasMoreElements())
      {
      int32_t j = cursor.getNextElement();
      if ( definingMap->find(j) == definingMap->end() || 
           (*definingMap)[j]->isEmpty())   
         resultSymRefs->set(j);
      else
         *resultSymRefs |= *(*definingMap)[j];
      } 
   }

/*
 * \brief merge the 2 DefiningMaps
 *
 * This is a helper function that update the defs in the `firstMap` with the defs in the `secondMap`. 
 * For each entry in the `secondMap`, there are 2 steps:
 *    step 1: overwrite the entry with the same key in the `firstMap`
 *    setp 2: update the defining symbols of that entry according to the `firstMap`
 * Here is an example:
 *    firstMap:
 *       ------------------
 *       | a0 -> {a1}     |
 *       | a2 -> {a3, a4} |
 *       ------------------
 *    secondMap:
 *       ------------------
 *       | a2 -> {a0}     |
 *       ------------------
 *
 * after step 1, the firstMap looks like:
 *       ------------------
 *       | a0 -> {a1}     |
 *       | a2 -> {a0}     |
 *       ------------------
 * after step 2, the firstMap looks like:
 *       ------------------
 *       | a0 -> {a1}     |
 *       | a2 -> {a1}     |
 *       ------------------
 */
static void mergeDefiningMaps(DefiningMap *firstMap, DefiningMap *secondMap, TR::Compilation *comp)
   {
   if (comp->getOption(TR_TraceOSR))
      {
      traceMsg(comp, "mergeDefiningMaps: firstMap before\n"); 
      printMap(firstMap, comp);
      traceMsg(comp, "mergeDefiningMaps: secondMap before\n"); 
      printMap(secondMap, comp);
      }

   for (auto it = secondMap->begin(); it != secondMap->end(); ++it)   
      {
      int32_t symRefNumber = it->first;
      TR_BitVector *definingSymRefs = NULL;
      if (firstMap->find(symRefNumber) == firstMap->end())
         definingSymRefs = new (comp->trStackMemory()) TR_BitVector(comp->trMemory()->currentStackRegion());
      else 
         {
         definingSymRefs = (*firstMap)[symRefNumber];
         definingSymRefs->empty();
         }
      updateDefiningSymRefs(it->second, firstMap, definingSymRefs);
      (*firstMap)[symRefNumber] = definingSymRefs;
      }

   if (comp->getOption(TR_TraceOSR))
      {
      traceMsg(comp, "mergeDefiningMaps: firstMap after\n"); 
      printMap(firstMap, comp);
      }
   }

/*
 * \brief Solve and publish the final defining map which maps all the symbols under the prepareForOSR calls
 *        to their defining symbols
 * 
 * @param finalMap the map that will be published to OSRMethodMetaData
 * @param workingCatchBlockMap this map maintains all the defining relationsships while iterates through all the blocks 
 * @param definingMapAtOSRCodeBlocks the OSRCodeBlock map for each method which needed in building the maps for all the callees
 * @param definingMapAtPrepareForOSRCalls the prepareForOSRCall map needed in building maps for all the callees
 *
 * This function does the following 2 things:
 *    - put the symbols under current prepareForOSRCall into the finalMap
 *    - merge the workingCatchBlockMap with the OSRCodeBlockMap 
 *
 * This is a recursive function walking up the call chain and stops at the top most method. The walking path is the same as
 * the execution path of an OSR transition.
 */
void 
TR_OSRCompilationData::buildFinalMap (int32_t callerIndex,
                                      DefiningMap *finalMap,
                                      DefiningMap *workingCatchBlockMap,
                                      DefiningMaps &definingMapAtOSRCodeBlocks, 
                                      DefiningMaps &definingMapAtPrepareForOSRCalls
                                      )
   {
   if (comp->getOption(TR_TraceOSR))
      traceMsg(comp, "buildFinalMap callerIndex %d\n", callerIndex);
   TR_OSRMethodData *osrMethodData = getOSRMethodDataArray()[callerIndex+1];
   DefiningMap *codeBlockMap = definingMapAtOSRCodeBlocks[callerIndex+1];
   DefiningMap *prepareForOSRCallMap = definingMapAtPrepareForOSRCalls[callerIndex+1];
   for (auto entry = prepareForOSRCallMap->begin(); entry != prepareForOSRCallMap->end(); ++entry )
      {
      int32_t symRefNum = entry->first;
      TR_BitVector *definingSymbols = entry->second;
      TR_BitVector *result = new (comp->trHeapMemory()) TR_BitVector(0, comp->trMemory(), heapAlloc);
      updateDefiningSymRefs(definingSymbols, workingCatchBlockMap, result);
      TR_ASSERT(finalMap->find(symRefNum) == finalMap->end(), "same symbol reference shouldn't be written twice under different prepareForOSRCall");
      if (comp->getOption(TR_TraceOSR))
         {
         traceMsg(comp, "adding symRef #%d and its defining symbols to finalMap\n", symRefNum);
         result->print(comp);
         traceMsg(comp, "\n");
         }
      (*finalMap)[symRefNum] = result;
      }

   if (callerIndex == -1)   
      return;
   mergeDefiningMaps(workingCatchBlockMap, codeBlockMap, comp);
   TR::Block *osrCodeBlock = osrMethodData->getOSRCodeBlock();
   TR::Block *nextBlock = toBlock(osrCodeBlock->getSuccessors().front()->getTo());
   while (!nextBlock->isOSRCodeBlock())
      {
      TR_ASSERT(nextBlock->getSuccessors().size() == 1, "blocks between osrCodeBlock can only have one successor (block_%d)", nextBlock->getNumber());
      nextBlock = toBlock(nextBlock->getSuccessors().front()->getTo());
      }

   int32_t nextCallerIndex = nextBlock->getEntry()->getNode()->getByteCodeInfo().getCallerIndex();
   buildFinalMap (nextCallerIndex, finalMap, workingCatchBlockMap, definingMapAtOSRCodeBlocks, definingMapAtPrepareForOSRCalls);
   }

/*
 * This function build the OSRCodeBlock map and the prepareForOSRCall map 
 */
void
TR_OSRMethodData::buildDefiningMapForOSRCodeBlockAndPrepareForOSRCall(TR::Block *block, DefiningMap *osrCodeBlockMap, DefiningMap *prepareForOSRCallMap)
   {
   TR_ASSERT(block->getSuccessors().size() == 1, "OSRCodeBlock should have one successor but block_%d has %d", block->getNumber(), block->getSuccessors().size());
   if (comp()->getOption(TR_TraceOSR))
      traceMsg(comp(), "buildDefiningMapForOSRCodeBlockAndPrepareForOSRCall block_%d\n", block->getNumber()); 
   buildDefiningMap(block, osrCodeBlockMap, prepareForOSRCallMap);
   if (block->getEntry()->getNode()->getByteCodeInfo().getCallerIndex() == -1)
      return;
   TR::Block *nextBlock = toBlock(block->getSuccessors().front()->getTo()); 
   // Keep processing all blocks one after another until reaching the next OSRCodeBlock
   if (!nextBlock->isOSRCodeBlock())
      buildDefiningMapForBlock(nextBlock, osrCodeBlockMap);
   }

/*
 * \brief This function builds the defining map for the given block and all the blocks after
 * until reaching the `next OSRCodeBlock`. 
 *
 * If called from `buildDefiningMapForOSRCodeBlockAndPrepareForOSRCall`, the map is for
 * an OSRCodeBlock and `next OSRCodeBlock` belongs to the next caller in the call chain of this 
 * method. Note that it's possible that the immediate next caller doesn't have an OSRCodeBlock
 * and the `next OSRCodeBlock` would belong to the caller's caller.
 *
 * If called from `buildDefiningMap`, the map is for the OSRCatchBlock, and the `next OSRCodeBlock`
 * belongs to the same method as the OSRCatchBlock.
 */
void
TR_OSRMethodData::buildDefiningMapForBlock(TR::Block *block, DefiningMap *blockMap)
   {
   TR_ASSERT(block->getSuccessors().size() == 1, "OSRCatchBlock should have one successor but block_%d has %d", block->getNumber(), block->getSuccessors().size());
   if (comp()->getOption(TR_TraceOSR))
      traceMsg(comp(), "buildDefiningMapForBlock block_%d\n", block->getNumber()); 
   buildDefiningMap(block, blockMap);
   TR::Block *nextBlock = toBlock(block->getSuccessors().front()->getTo()); 
   if (!nextBlock->isOSRCodeBlock())
      buildDefiningMapForBlock(nextBlock, blockMap);
   }

/*
 * \brief This is the most basic function iterating through all store nodes and add entries to the DefiningMap to
 * map each symbol to the group of symbols defining it
 */
void
TR_OSRMethodData::buildDefiningMap(TR::Block *block, DefiningMap *blockMap, DefiningMap *prepareForOSRCallMap)
   {
   for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextRealTreeTop())
      {
      TR::SymbolReference * symRef = NULL;
      TR::Node *node = tt->getNode();

      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "buildDefiningMap node n%dn\n", node->getGlobalIndex());

      if (node->getOpCode().isStoreDirect())
         symRef = node->getSymbolReference();
      else if (node->getOpCode().isStoreReg())
         symRef = node->getRegLoadStoreSymbolReference();

      if (symRef)
         {
         if (symRef->getSymbol()->isAutoOrParm())
            {
            TR_BitVector subTreeSymRefs(comp()->trMemory()->currentStackRegion());
            TR::NodeChecklist checklist(comp());
            collectSubTreeSymRefs(node->getFirstChild(), &subTreeSymRefs, checklist);

            if (comp()->getOption(TR_TraceOSR))
               {
               traceMsg(comp(), "buildDefiningMap: node n%dn: defining symbol #%d: ", node->getGlobalIndex(), symRef->getReferenceNumber());
               subTreeSymRefs.print(comp());
               traceMsg(comp(), "\n");
               }

            TR_BitVector *symRefs = NULL;
            if (blockMap->find(symRef->getReferenceNumber()) != blockMap->end())
               {
               symRefs = (*blockMap)[symRef->getReferenceNumber()];
               symRefs->empty(); // the current store kills previous defs
               }

            if (!subTreeSymRefs.isEmpty())
               {
               if (!symRefs)
                  {
                  symRefs = new (comp()->trStackMemory()) TR_BitVector(comp()->trMemory()->currentStackRegion());
                  (*blockMap)[symRef->getReferenceNumber()] = symRefs;
                  }
               updateDefiningSymRefs(&subTreeSymRefs, blockMap, symRefs);
               }
            }
         }
      else if (node->getFirstChild() &&
               node->getFirstChild()->getOpCode().isCall() &&
               node->getFirstChild()->getSymbolReference()->getReferenceNumber() == TR_prepareForOSR)
         {
         TR::Node *callNode = node->getFirstChild();
         const int firstSymChildIndex = 2;
         for (int32_t child = firstSymChildIndex; child+2 < callNode->getNumChildren(); child += 3)
            {
            TR::Node* loadNode = callNode->getChild(child);
            int32_t symRefNumber = callNode->getChild(child+1)->getInt();
            TR::SymbolReference* symRef = comp()->getSymRefTab()->getSymRef(symRefNumber);
            //GC map for Monitored object temps are handled specially by lmmd
            if (symRef->getSymbol()->holdsMonitoredObject() 
               || symRef->getSymbol()->isThisTempForObjectCtor())
               continue;
            TR_BitVector subTreeSymRefs(comp()->trMemory()->currentStackRegion());
            TR::NodeChecklist checklist(comp());
            collectSubTreeSymRefs(loadNode, &subTreeSymRefs, checklist);
            if (comp()->getOption(TR_TraceOSR))
               {
               traceMsg(comp(), "collect subTreeSymRefs of loadNode n%dn for original symRef #%d\n", loadNode->getGlobalIndex(), symRefNumber);
               subTreeSymRefs.print(comp());
               traceMsg(comp(), "\n");
               }

            if (!subTreeSymRefs.isEmpty())
               {
               TR_BitVector *symRefs = new (comp()->trStackMemory()) TR_BitVector(comp()->trMemory()->currentStackRegion());
               updateDefiningSymRefs(&subTreeSymRefs, blockMap, symRefs);
               (*prepareForOSRCallMap)[symRefNumber] = symRefs;
               }
            }
         }
      }
   }

/*
 * find all the symbols on the right hand side of the store node
 */
void 
TR_OSRMethodData::collectSubTreeSymRefs(TR::Node *node, TR_BitVector *subTreeSymRefs, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return;
   else checklist.add(node);
   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isAutoOrParm())
      {
      subTreeSymRefs->set(node->getSymbolReference()->getReferenceNumber());
      }
   else if (node->getRegLoadStoreSymbolReference())
      {
      subTreeSymRefs->set(node->getRegLoadStoreSymbolReference()->getReferenceNumber());
      }
   if (node->getNumChildren() > 0)
      {
      for (int i=0; i < node->getNumChildren(); i++)
         collectSubTreeSymRefs(node->getChild(i), subTreeSymRefs, checklist);
      }
   }

void TR_OSRCompilationData::buildSymRefOrderMap()
   {
   for (int i = 0; i < getOSRMethodDataArray().size(); i++)
      {
      TR_OSRMethodData* osrMethodData = getOSRMethodDataArray()[i];
      if (osrMethodData == NULL ||
          osrMethodData->getOSRCodeBlock() == NULL)
         continue;
      TR::ResolvedMethodSymbol *methSym = osrMethodData->getMethodSymbol();
      buildSymRefOrderMapAux(methSym->getPendingPushSymRefs());
      buildSymRefOrderMapAux(methSym->getAutoSymRefs());
      }
   }


void TR_OSRCompilationData::buildSymRefOrderMapAux( TR_Array<List<TR::SymbolReference> >* symListArray)
   {
   if (symListArray == NULL)
      return;

   for (intptrj_t j = 0; j < symListArray->size(); j++)
      {
      List<TR::SymbolReference>& symList = (*symListArray)[j];
      bool sharedSlot = symList.getSize() > 1;
      if (!sharedSlot) continue;
      int32_t symRefOrder = 0;
      ListIterator<TR::SymbolReference> symIt(&symList);
      for (TR::SymbolReference* symRef = symIt.getFirst(); symRef; symRef = symIt.getNext(), symRefOrder++)
         {
         symRefOrderMap.Add(symRef->getReferenceNumber(), symRefOrder);
         }
      }
   }

int32_t TR_OSRCompilationData::getSymRefOrder(int32_t symRefNumber)
   {
   CS2::HashIndex hashIndex;
   if (symRefOrderMap.Locate(symRefNumber, hashIndex))
      return symRefOrderMap.DataAt(hashIndex);
   else
      return -1;
   }


TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRCompilationData& osrCompilationData)
   {
   out << "{";
   bool first = true;
   for (int i = 0; i < osrCompilationData.getOSRMethodDataArray().size(); i++)
      {
      TR_OSRMethodData* osrMethodData = osrCompilationData.getOSRMethodDataArray()[i];
      if (osrMethodData == NULL || osrMethodData->getOSRCodeBlock() == NULL || osrMethodData->isEmpty()) continue;
      if (first)
         out << "osrMethodDataArray: [\n";
      else
         out << ",\n";
      out << "callerIdx:" << i-1 << " -> " << *osrMethodData ;
      first = false;
      }
   if (!first)
      out << "]\n";

   const TR_OSRCompilationData::TR_Instruction2SharedSlotMap& array1 = osrCompilationData.instruction2SharedSlotMap;
   if (array1.size() != 0)
      {
      out << ", Instr2SharedSlotMetaData: " << array1.size() << "[\n";
      bool first = true;
      for (auto itr = array1.begin(), end = array1.end(); itr != end; ++itr)
         {
         if (!first)
            out << ",\n";
         char tmp[20];
         sprintf(tmp, "%x", (*itr).instructionPC);
         const  TR_OSRCompilationData::TR_ScratchBufferInfos& array2 = (*itr).scratchBufferInfos;
         out << tmp << " -> " << array2.size() << "[ ";
         for (int j = 0; j < array2.size(); j++)
            {
            if (j != 0)
               out << ", ";
            out << "{"
                << array2[j].inlinedSiteIndex << ", "
                << array2[j].osrBufferOffset << ", "
                << array2[j].scratchBufferOffset << ", "
                << array2[j].symSize
                << "}";
            }
         out << "]";
         first = false;
         }
      out << "]";
      }
   out << "}\n";
   return out;
   }

TR_OSRMethodData::TR_OSRMethodData(int32_t _inlinedSiteIndex, TR::ResolvedMethodSymbol *_methodSymbol,
   TR_OSRCompilationData* _osrCompilationData)
      : inlinedSiteIndex(_inlinedSiteIndex),
        methodSymbol(_methodSymbol),
        osrCompilationData(_osrCompilationData),
        osrCodeBlock(NULL),
        osrCatchBlock(NULL),
        numOfSymsThatShareSlot(-1),
        _linkedToCaller(false),
        slot2ScratchBufferOffset(comp()->allocator()),
        _numSymRefs(0),
        bcInfoHashTab(comp()->allocator()),
        bcLiveRangeInfoHashTab(comp()->allocator()),
        bcPendingPushLivenessInfoHashTab(comp()->allocator()),
        argInfoHashTab(comp()->allocator()),
        _symRefDefiningMap(NULL),
        _symRefs(NULL)
   {}

TR::Block *
TR_OSRMethodData::findOrCreateOSRCodeBlock(TR::Node* n)
   {
   if (getOSRCodeBlock() != NULL)
      return getOSRCodeBlock();
   TR_ASSERT(getOSRCatchBlock() == NULL, "osr catch block is not NULL\n");
   createOSRBlocks(n);
   return getOSRCodeBlock();
   }

void
TR_OSRMethodData::createOSRBlocks(TR::Node* n)
   {
   if (getOSRCodeBlock() != NULL) return;
   bool duringIlgen = comp()->getCurrentIlGenerator() != NULL;

   // Create one if there is no match
   TR_ASSERT(n, "reference node must be provided");
   osrCodeBlock = TR::Block::createEmptyBlock(n, comp(), duringIlgen ? -1 : 0);
   osrCodeBlock->setIsCold();
   osrCodeBlock->setIsOSRCodeBlock();
   osrCodeBlock->setDoNotProfile();

   osrCatchBlock = TR::Block::createEmptyBlock(n, comp(), duringIlgen ? -1 : 0);
   osrCatchBlock->setIsCold();
   osrCatchBlock->setDoNotProfile();
   osrCatchBlock->setIsOSRCatchBlock();
   osrCatchBlock->setHandlerInfoWithOutBCInfo(TR::Block::CanCatchOSR, comp()->getInlineDepth(), -1, getMethodSymbol()->getResolvedMethod(), comp());

   TR::CFG * cfg = getMethodSymbol()->getFlowGraph();
   cfg->addNode(osrCatchBlock);
   cfg->addNode(osrCodeBlock);
   cfg->addEdge(osrCatchBlock, osrCodeBlock);
   osrCodeBlock->getEntry()->insertTreeTopsBeforeMe(osrCatchBlock->getEntry(), osrCatchBlock->getExit());
   if (comp()->getOptions()->getVerboseOption(TR_VerboseOSRDetails))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "Created OSR code block and catch block for inlined index %d in %s calling %s",
              getInlinedSiteIndex(),
              comp()->signature(),
              getMethodSymbol()->signature(comp()->trMemory()));
      }
   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "Created OSR code block_%d(%p) and OSR catch block_%d(%p) for %s %s\n",
              osrCodeBlock->getNumber(), osrCodeBlock,
              osrCatchBlock->getNumber(), osrCatchBlock,
              getInlinedSiteIndex() == -1 ? "topmost method" : "inlined method",
              getMethodSymbol()->signature(comp()->trMemory()));
      }
   }


TR::Block *
TR_OSRMethodData::findOrCreateOSRCatchBlock(TR::Node* n)
   {
   if (getOSRCatchBlock() != NULL)
      return getOSRCatchBlock();
   TR_ASSERT(getOSRCodeBlock() == NULL, "osr code block is not NULL\n");
   createOSRBlocks(n);
   return getOSRCatchBlock();
   }

bool
TR_OSRMethodData::inlinesAnyMethod() const
   {
   TR::Compilation* comp = getMethodSymbol()->comp();
   for (int32_t i = 0; i < comp->getNumInlinedCallSites(); i++)
      {
      TR_InlinedCallSite& ics = comp->getInlinedCallSite(i);
      if (ics._byteCodeInfo.getCallerIndex() == inlinedSiteIndex)
          return true;
      }
   return false;
   }

void
TR_OSRMethodData::addLiveRangeInfo(int32_t byteCodeIndex, TR_BitVector *liveRangeInfo)
   {
   bcLiveRangeInfoHashTab.Add(byteCodeIndex, liveRangeInfo);
   }

TR_BitVector *
TR_OSRMethodData::getLiveRangeInfo(int32_t byteCodeIndex)
   {
   CS2::HashIndex hashIndex;
   TR_BitVector* liveRangeInfo = NULL;
   if (bcLiveRangeInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      liveRangeInfo = bcLiveRangeInfoHashTab.DataAt(hashIndex);
      }

   return liveRangeInfo;
   }

/*
 * Ensure it is possible to store the required number of symbol reference
 * arguments against a bytecode index
 */
void
TR_OSRMethodData::ensureArgInfoAt(int32_t byteCodeIndex, int32_t argNum)
   {
   CS2::HashIndex hashIndex;
   bool generate = false;
   if (!argInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      generate = true;
      }
   else
      {
      TR_Array<int32_t> *args = getArgInfo(byteCodeIndex);
      if (args->size() != argNum)
         {
         argInfoHashTab.Remove(byteCodeIndex);
         generate = true;
         }
      }
 
   if (generate)
      argInfoHashTab.Add(byteCodeIndex,
         new (comp()->trMemory()->trHeapMemory()) TR_Array<int32_t>(comp()->trMemory(), argNum, false, heapAlloc), hashIndex);
   }

/*
 * Stash the symbol reference numbers against a bytecode index,
 * to be used as arguments when transitioning to this index.
 */
void
TR_OSRMethodData::addArgInfo(int32_t byteCodeIndex, int32_t argIndex, int32_t argSymRef)
   {
   CS2::HashIndex hashIndex;
   TR_Array<int32_t> *args;
   if (argInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      args = argInfoHashTab.DataAt(hashIndex);
      (*args)[argIndex] = argSymRef;
      }
   }

/*
 * Get the list of symbol reference numbers for a bytecode index
 * to be used as arguments to the induce call targeting it
 */
TR_Array<int32_t>*
TR_OSRMethodData::getArgInfo(int32_t byteCodeIndex)
   {
   CS2::HashIndex hashIndex;
   TR_Array<int32_t> *args = NULL;
   if (argInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      args = argInfoHashTab.DataAt(hashIndex);
      }
   return args;
   }


bool
TR_OSRMethodData::hasSlotSharingOrDeadSlotsInfo()
   {
   return !bcInfoHashTab.IsEmpty();
   }

/*
 * Add pending push live range info for a BCI.
 *
 * During IlGen, the live pending push symbol references may be available.
 * They can be stashed against the BCI, so that OSRLiveRangeAnalysis can
 * skip them, avoiding costly region and liveness analysis.
 */
void
TR_OSRMethodData::addPendingPushLivenessInfo(int32_t byteCodeIndex, TR_BitVector *livenessInfo)
   {
   bcPendingPushLivenessInfoHashTab.Add(byteCodeIndex, livenessInfo);
   }

/*
 * Get pending push live range info for a BCI. This represents all pending pushes
 * that are live at a BCI. It will only be valid if they are stash here using
 * addPendingPushLivenessInfo.
 */
TR_BitVector *
TR_OSRMethodData::getPendingPushLivenessInfo(int32_t byteCodeIndex)
   {
   CS2::HashIndex hashIndex;
   TR_BitVector* livenessInfo = NULL;
   if (bcPendingPushLivenessInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      livenessInfo = bcPendingPushLivenessInfoHashTab.DataAt(hashIndex);
      }

   return livenessInfo;
   }

void
TR_OSRMethodData::addSlotSharingInfo(int32_t byteCodeIndex, int32_t slot, int32_t symRefNum,
                                     int32_t symRefOrder, int32_t symSize, bool takesTwoSlots)
   {
   CS2::HashIndex hashIndex;
   TR_OSRSlotSharingInfo* ssinfo;
   if (bcInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      ssinfo = bcInfoHashTab.DataAt(hashIndex);
      }
   else
      {
      ssinfo = new (getMethodSymbol()->comp()->trHeapMemory()) TR_OSRSlotSharingInfo(getMethodSymbol()->comp());
      bcInfoHashTab.Add(byteCodeIndex, ssinfo);
      }
   ssinfo->addSlotInfo(slot, symRefNum, symRefOrder, symSize, takesTwoSlots);
   }

void
TR_OSRMethodData::ensureSlotSharingInfoAt(int32_t byteCodeIndex)
   {
   CS2::HashIndex hashIndex;
   if (!bcInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      TR_OSRSlotSharingInfo* ssinfo = new (getMethodSymbol()->comp()->trHeapMemory()) TR_OSRSlotSharingInfo(getMethodSymbol()->comp());
      bcInfoHashTab.Add(byteCodeIndex, ssinfo);
      }
   }

void
TR_OSRMethodData::addScratchBufferOffset(int32_t slotIndex, int32_t symRefOrder, int32_t scratchBufferOffset)
   {
   CS2::HashIndex hashIndex;
   if (!slot2ScratchBufferOffset.Locate(slotIndex, hashIndex))
      {
      slot2ScratchBufferOffset.Add(slotIndex, TR_Array<int32_t>(comp()->trMemory()), hashIndex);
      }
   slot2ScratchBufferOffset.DataAt(hashIndex)[symRefOrder] = scratchBufferOffset;
   }

void
TR_OSRMethodData::addInstruction(int32_t instructionPC, int32_t byteCodeIndex)
   {
   bool trace = comp()->getOption(TR_TraceOSR);
   //add the necessary mapping in "instruction -> shared slot info" map
   TR_ASSERT(getNumOfSymsThatShareSlot() >= 0, "number of symbols that share slots (%d) cannot be negative", getNumOfSymsThatShareSlot());
   if (getNumOfSymsThatShareSlot() == 0)
      {
      if (trace)
         traceMsg(comp(), "  rejected: no slot-sharing symbols in OSRMethodData\n");
      return;
      }
   CS2::HashIndex hashIndex;
   TR_ASSERT(comp()->getOption(TR_EnableOSR), "OSR must be enabled for TR_OSRMethodData::addInstruction");
   //traceMsg(comp(), "instructionPC %x bcidx %x ", instructionPC, instr->getNode()->getByteCodeInfo().getByteCodeIndex());
   //proceed only if there has been an OSR point at this location
   if (bcInfoHashTab.Locate(byteCodeIndex, hashIndex))
      {
      if (trace)
         traceMsg(comp(), "  Adding info for each slot\n");
      const TR_Array<TR_OSRSlotSharingInfo::TR_SlotInfo>& slotInfos =
         bcInfoHashTab.DataAt(hashIndex)->getSlotInfos();
      TR_OSRCompilationData::TR_ScratchBufferInfos info(comp()->trMemory());
      //The "composition" of the three types of tuples happens here
      for (int i = 0; i < slotInfos.size(); i++)
         {
         int32_t scratchBufferOffset;
         bool found = slot2ScratchBufferOffset.Locate(slotInfos[i].slot, hashIndex);
         TR_ASSERT(found, "slot %d symref #%d must be in slot2ScratchBufferOffset hash table\n",
                 slotInfos[i].slot, slotInfos[i].symRefNum);
         if (slotInfos[i].symRefOrder == -1)
            {
            TR_ASSERT(slotInfos[i].symRefNum == -1, "slotInfos[%d].symRefOrder==%d.  Expected symRefNum==-1; actually %d\n", i, slotInfos[i].symRefOrder, slotInfos[i].symRefNum);
            scratchBufferOffset = -1;
            }
         else
            scratchBufferOffset = slot2ScratchBufferOffset.DataAt(hashIndex)[slotInfos[i].symRefOrder];
         int32_t osrBufferOffset = slotIndex2OSRBufferIndex(slotInfos[i].slot, slotInfos[i].symSize, slotInfos[i].takesTwoSlots);
         info.add(TR_OSRCompilationData::TR_ScratchBufferInfo(getInlinedSiteIndex(), osrBufferOffset,
                                                              scratchBufferOffset, slotInfos[i].symSize));
         if (trace)
            traceMsg(comp(), "    %3d: %3d %3d %3d %3d\n", i, getInlinedSiteIndex(), osrBufferOffset, scratchBufferOffset, slotInfos[i].symSize);
         }
      osrCompilationData->addInstruction2SharedSlotMapEntry(instructionPC, info);
      }
   else
      {
      if (trace)
         traceMsg(comp(), "  rejected: byteCodeIndex %d is not an OSR point\n", byteCodeIndex);
      }
   }

TR::Compilation* TR_OSRMethodData::comp() const
   {
   return methodSymbol->comp();
   }

int32_t
TR_OSRMethodData::getHeaderSize() const
   {
   return TR::Compiler->vm.OSRFrameHeaderSizeInBytes(comp());
   }

int32_t
TR_OSRMethodData::slotIndex2OSRBufferIndex(int32_t slotIndex, int symSize, bool takesTwoSlots) const
   {
   int32_t headerSize = getHeaderSize();
   int32_t numOfSyncObjects = (getMethodSymbol()->getSyncObjectTemp() != NULL)?1:0;
   //Note that in 64 bit mode, a long or a double value will be stored in the low-memory slot, and an int will be store in the low-memory double-word
   if (slotIndex < 0)
      return
         headerSize
         + (getMethodSymbol()->getNumPPSlots() + slotIndex + (takesTwoSlots ? -1 : 0)) * TR::Compiler->om.sizeofReferenceAddress();
   else
      return
         headerSize + (getMethodSymbol()->getNumPPSlots() +
                       getMethodSymbol()->getResolvedMethod()->numberOfTemps() +
                       numOfSyncObjects +
                       getMethodSymbol()->getNumParameterSlots()
                       - slotIndex - 1 + (takesTwoSlots ? -1 : 0))* TR::Compiler->om.sizeofReferenceAddress();
   }

int32_t
TR_OSRMethodData::getTotalNumOfSlots() const
   {
   int32_t numOfSyncObjects = (getMethodSymbol()->getSyncObjectTemp() != NULL)?1:0;
   return getMethodSymbol()->getNumPPSlots() +
      getMethodSymbol()->getResolvedMethod()->numberOfTemps() +
      numOfSyncObjects +
      getMethodSymbol()->getNumParameterSlots();
   }


int32_t
TR_OSRMethodData::getTotalDataSize() const
   {
   int32_t numOfSyncObjects = (getMethodSymbol()->getSyncObjectTemp() != NULL)?1:0;
   return getHeaderSize() + getTotalNumOfSlots() * TR::Compiler->om.sizeofReferenceAddress();
   }


void
TR_OSRMethodData::setNumOfSymsThatShareSlot(int32_t newValue)
   {
   TR_ASSERT(newValue >= 0, "setNumOfSymsThatShareSlot cannot accept a negative value");
   if (newValue == numOfSymsThatShareSlot)
      //this can happen if there are two OSR helper calls for the same inlined method
      //For now, this happens, e.g., when profileGenerator clones every block in the cfg.
      return;
   //Otherwise, you can initialize this field only once
   TR_ASSERT(numOfSymsThatShareSlot == -1, "value has already been initialized to %d and now we want to set it to %d\n", numOfSymsThatShareSlot, newValue);
   numOfSymsThatShareSlot = newValue;
   osrCompilationData->updateNumOfSymsThatShareSlot(newValue);
   }

DefiningMap *
TR_OSRMethodData::getDefiningMap()
   { 
   if (_symRefDefiningMap == NULL)
      _symRefDefiningMap = new DefiningMap(DefiningMapComparator(), DefiningMapAllocator(comp()->trMemory()->heapMemoryRegion()));
   return _symRefDefiningMap; 
   }

TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRMethodData& osrMethodData)
   {
   bool first = true;
   const TR_OSRMethodData::TR_BCInfoHashTable& table = osrMethodData.bcInfoHashTab;
   TR_OSRMethodData::TR_BCInfoHashTable::Cursor c(table);
   out << "[";
   for (c.SetToFirst(); c.Valid(); c.SetToNext())
      {
      if (!first)
         out << ",\n";
      char bcIndex[20];
      sprintf(bcIndex, "%x", table.KeyAt(c));

      out << bcIndex << " -> " << table.DataAt(c);
      first = false;
      }
   out << "]\n";
   return out;
   }

TR_OSRPoint::TR_OSRPoint(TR_ByteCodeInfo &bcInfo, TR_OSRMethodData *methodData, TR_Memory *m)
   : _methodData(methodData), _bcInfo(bcInfo)
   {
   }

TR_OSRSlotSharingInfo::TR_OSRSlotSharingInfo(TR::Compilation* _comp) :
   slotInfos(_comp->trMemory()),
   comp(_comp) {};

void
TR_OSRSlotSharingInfo::addSlotInfo(int32_t slot, int32_t symRefNum, int32_t symRefOrder, int32_t symSize, bool takesTwoSlots)
   {
   static bool trace = comp->getOption(TR_TraceOSR);
   TR::SymbolReferenceTable* symRefTab = comp->getSymRefTab();
   bool found = false;
   for (int i = 0; i < slotInfos.size(); i++)
      {
      TR_SlotInfo& info = slotInfos[i];
      if (info.symRefNum != -1)
         {
         if ((info.slot == slot) &&
            (info.symRefNum != symRefNum) &&
            (symRefTab->getSymRef(info.symRefNum)->getSymbol()->getDataType() == symRefTab->getSymRef(symRefNum)->getSymbol()->getDataType()))
            {
            //Two different variables sharing the same slot, can be live at a certain node (when there is a loop). So commenting the assertion out for now.
            //TR_ASSERTC(0, comp, "ERROR: For slot %d symref #%d conflicts with symref#%d\n",
            //    symRefNum, info.symRefNum, slot);
            }
         if ((info.slot == slot) && (info.symRefNum == symRefNum))
            {
            TR_ASSERT(info.symRefOrder==symRefOrder && info.symSize==symSize, "symref order (%d,%d) and symsize (%d,%d) must match", info.symRefOrder, symRefOrder, info.symSize, symSize);
            found = true;
            }
         }
      //check if the two syms overlap, if they do we will write zero in that stack slot. That's because
      //two shared symbols cannot be live at the same OSR point and we write the value zero because
      //it's a valid value for both reference and non-reference types.
      if (info.symRefNum != symRefNum && ((slot < 0 && info.slot < 0) || (slot >= 0 && info.slot >= 0)))
         {
         int32_t startSlot1 = slot > 0 ? slot : -slot;
         int32_t startSlot2 = info.slot > 0 ? info.slot : -info.slot;
         int32_t endSlot1 = startSlot1 + (takesTwoSlots ? 2 : 1) - 1;
         int32_t endSlot2 = startSlot2 + (info.takesTwoSlots ? 2 : 1) - 1;
         if ((startSlot1 <= endSlot2) && (startSlot2 <= endSlot1))
            {
            //if (trace) traceMsg(comp, "symbols %d and %d overlap\n", symRefNum, info.symRefNum);
            // don't add any more symbols to the list
            found = true;
            //mark the symbol
            info.symRefNum = -1;
            info.symRefOrder = -1;
            if (symSize > info.symSize)
               {
               info.symSize = symSize;
               info.slot = slot;
               }
            }
         }
      }
   if (!found)
      slotInfos.add(TR_SlotInfo(slot, symRefNum, symRefOrder, symSize, takesTwoSlots));
   }

TR::Compilation& operator<< (TR::Compilation& out, const TR_OSRSlotSharingInfo* osrSlotSharingInfo)
   {
   out << "{slotInfos: [";
   for (int i = 0; i < osrSlotSharingInfo->slotInfos.size(); i++)
      {
      const TR_OSRSlotSharingInfo::TR_SlotInfo& info = osrSlotSharingInfo->slotInfos[i];
      if (i != 0)
         out << ", ";
      out << "{" << info.slot << ", " << info.symRefNum << ", " << info.symRefOrder << ", " << info.symSize << ", " << (info.takesTwoSlots?"two slots": "one slot") << "}";
      }
   out << "]}";
   return out;
   }

bool TR_OSRCompilationData::TR_ScratchBufferInfo::operator==(const TR_ScratchBufferInfo& info) const
   {
   return inlinedSiteIndex == info.inlinedSiteIndex && osrBufferOffset == info.osrBufferOffset &&
      scratchBufferOffset == info.scratchBufferOffset && symSize == info.symSize;
   }

#if defined(TR_HOST_POWER) && defined(TR_TARGET_POWER) && defined(__IBMCPP__)
__attribute__((__noinline__))
//This tiny function when inlined by xlC 12 at -O3 breaks java -version
#endif
int32_t TR_OSRCompilationData::TR_ScratchBufferInfo::writeToBuffer(uint8_t* buffer) const
   {
   uint8_t* bufferStart = buffer;
   *((int32_t*)buffer) = inlinedSiteIndex; buffer += sizeof(int32_t);
   *((int32_t*)buffer) = osrBufferOffset; buffer += sizeof(int32_t);
   *((int32_t*)buffer) = scratchBufferOffset; buffer += sizeof(int32_t);
   *((int32_t*)buffer) = symSize; buffer += sizeof(int32_t);
   return buffer - bufferStart;
   }
