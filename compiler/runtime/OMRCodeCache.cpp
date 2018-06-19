/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "runtime/OMRCodeCache.hpp"

#include <algorithm>                    // for std::max, etc
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t, int32_t, uint32_t, etc
#include <stdio.h>                      // for fprintf, stderr, printf, etc
#include <string.h>                     // for memcpy, strcpy, memset, etc
#include "codegen/FrontEnd.hpp"         // for TR_VerboseLog, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"  // for TR::Options, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                   // for POINTER_PRINTF_FORMAT
#include "env/defines.h"                // for HOST_OS, OMR_LINUX
#include "env/jittypes.h"               // for FLUSH_MEMORY
#include "il/DataTypes.hpp"             // for TR_YesNoMaybe::TR_yes, etc
#include "infra/Assert.hpp"             // for TR_ASSERT
#include "infra/CriticalSection.hpp"    // for CriticalSection
#include "infra/Monitor.hpp"            // for Monitor
#include "runtime/CodeCache.hpp"        // for CodeCache
#include "runtime/CodeCacheManager.hpp" // for CodeCache Manager
#include "runtime/CodeCacheMemorySegment.hpp" // for CodeCacheMemorySegment
#include "runtime/CodeCacheConfig.hpp"  // for CodeCacheConfig, etc
#include "runtime/Runtime.hpp"

#ifdef LINUX
#include <elf.h>                        // for EV_CURRENT, SHT_STRTAB, etc
#include <unistd.h>                     // for getpid, pid_t
#endif

namespace TR { class CodeGenerator; }

/*****************************************************************************
 *   Multi Code Cache page management
 */

/**
 * \page CodeCache  Code Cache Layout:
 *
 *  Basic layout of the code cache memory (capitalized names are fixed, others
 *  move)
 *
 *
 *                               TRAMPOLINEBASE              TEMPTRAMPOLINETOP
 *                                      |      TEMPTRAMPOLINEBASE   |
 *                                      |               |           |
 *          warmCodeAlloc coldCodeAlloc |               |       HELPERBASE HELPERTOP
 *             |-->           <--|      |               |           |         |
 *       |_____v_________________v______v_______________v___________v_________v
 *        warm                     cold
 *          \                       /
 *           ----> Method Bodies<---    ^  Trampolines  ^ TempTramps^ Helpers
 *
 *       (Low memory)   ---------------------------------------> (High memory)
 *
 *
 * * There are a fixed number of helper trampolines. Each helper has an index into
 *    these trampolines.
 * * There are a fixed number of temporary trampoline slots. `_tempTrampolineNext`
 *    goes from `_tempTrampolineBase`, allocating to `tempTrampolineMax.`
 * * There are a fixed number of permanent trampoline slots.
 * **  `_trampolineReservationMark` goes from `_tempTrampolineBase` to `_trampolineBase`
 *    backwards, reserving trampoline slots.
 * ** `_trampolineAllocationMark` goes from `_tempTrampolineBase` to
 *    `_trampolineReservationMark` filling in the trampoline slots.
 *
 * When there is no more room for trampolines the code cache is full.
 *
 * 5% of the code cache is allocated to the above trampolines. The rest is used
 * for method bodies. Regular parts of method bodies are allocated from the
 * heapBase forwards, and cold parts of method bodies are allocated from
 * `_trampolineBase` backwards. When the two meet, the code cache is full.
 *
 */

OMR::CodeCache::CacheCriticalSection::CacheCriticalSection(TR::CodeCache *codeCache)
   : CriticalSection(codeCache->_mutex)
   {
   }


TR::CodeCache *
OMR::CodeCache::self()
   {
   return static_cast<TR::CodeCache *>(this);
   }


void
OMR::CodeCache::destroy(TR::CodeCacheManager *manager)
   {
   while (_hashEntrySlab)
      {
      CodeCacheHashEntrySlab *slab = _hashEntrySlab;
      _hashEntrySlab = slab->_next;
      slab->free(manager);
      }

#if defined(TR_HOST_POWER)
   if (_trampolineSyncList)
      {
      if (_trampolineSyncList->_hashEntryArray)
         manager->freeMemory(_trampolineSyncList->_hashEntryArray);
      manager->freeMemory(_trampolineSyncList);
      }
#endif
   if (manager->codeCacheConfig().needsMethodTrampolines())
      {
      if (_resolvedMethodHT)
         {
         if (_resolvedMethodHT->_buckets)
            manager->freeMemory(_resolvedMethodHT->_buckets);
         manager->freeMemory(_resolvedMethodHT);
         }

      if (_unresolvedMethodHT)
         {
         if (_unresolvedMethodHT->_buckets)
            manager->freeMemory(_unresolvedMethodHT->_buckets);
         manager->freeMemory(_unresolvedMethodHT);
         }
      }
   }



uint8_t *
OMR::CodeCache::getCodeAlloc()
   {
   return _segment->segmentAlloc();
   }


uint8_t *
OMR::CodeCache::getCodeBase()
   {
   return _segment->segmentBase();
   }


uint8_t *
OMR::CodeCache::getCodeTop()
   {
   return _segment->segmentTop();
   }


void
OMR::CodeCache::reserve(int32_t reservingCompThreadID)
   {
   _reserved = true;
   _reservingCompThreadID = reservingCompThreadID;
   }


void
OMR::CodeCache::unreserve()
   {
   _reserved = false;
   _reservingCompThreadID = -2;
   }


void
OMR::CodeCache::writeMethodHeader(void *freeBlock, size_t size, bool isCold)
   {
   CodeCacheMethodHeader * block = (CodeCacheMethodHeader *)freeBlock;
   block->_size = size;

   TR::CodeCacheConfig & config = _manager->codeCacheConfig();

   if (!isCold)
      memcpy(block->_eyeCatcher, config.warmEyeCatcher(), sizeof(block->_eyeCatcher));
   else
      memcpy(block->_eyeCatcher, config.coldEyeCatcher(), sizeof(block->_eyeCatcher));
   block->_metaData = NULL;
   }


// Resize code memory within a code cache
//
bool
OMR::CodeCache::resizeCodeMemory(void *memoryBlock, size_t newSize)
   {
   if (newSize == 0) return false; // nothing to resize, just return

   TR::CodeCacheConfig & config = _manager->codeCacheConfig();
   size_t round = config.codeCacheAlignment() - 1;

   memoryBlock = (uint8_t *) memoryBlock - sizeof(CodeCacheMethodHeader); // Do we always have a header?
   newSize     = (newSize + sizeof(CodeCacheMethodHeader) + round) & ~round;

   CodeCacheMethodHeader *cacheHeader = (CodeCacheMethodHeader *) memoryBlock;

   // sanity check, the eyecatcher must be there
   TR_ASSERT(cacheHeader->_eyeCatcher[0] == config.warmEyeCatcher()[0], "Missing eyecatcher during resizeCodeMemory");

   size_t oldSize = cacheHeader->_size;

   if (newSize >= oldSize)
      return false;

   size_t shrinkage = oldSize - newSize;

   uint8_t *expectedHeapAlloc = (uint8_t *) memoryBlock + oldSize;
   if (config.verboseReclamation())
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"--resizeCodeMemory-- CC=%p cacheHeader=%p oldSize=%u newSize=%d shrinkage=%u", this, cacheHeader, oldSize, newSize, shrinkage);
      }

   if (expectedHeapAlloc == _warmCodeAlloc)
      {
      _manager->increaseFreeSpaceInCodeCacheRepository(shrinkage);
      _warmCodeAlloc -= shrinkage;
      cacheHeader->_size = newSize;
      return true;
      }
   else // the allocation could have been from a free block or from the cold portion
      {
      if (shrinkage >= MIN_SIZE_BLOCK)
         {
         // addFreeBlock needs to be done with VM access because the GC may also
         // change the list of free blocks
         if (self()->addFreeBlock2((uint8_t *) memoryBlock+newSize, (uint8_t *)expectedHeapAlloc))
            {
            //fprintf(stderr, "---ccr--- addFreeBlock due to shrinkage\n");
            }
         cacheHeader->_size = newSize;
         return true;
         }
      }
   //fprintf(stderr, "--ccr-- shrink code by %d\n", shrinkage);
   return false;
   }


// Initialize a code cache
//
bool
OMR::CodeCache::initialize(TR::CodeCacheManager *manager,
                         TR::CodeCacheMemorySegment *codeCacheSegment,
                         size_t codeCacheSizeAllocated,
                         CodeCacheHashEntrySlab *hashEntrySlab)
   {
   _manager = manager;

   // heapSize can be calculated as (codeCache->helperTop - codeCacheSegment->heapBase), which is equal to segmentSize
   // If codeCachePadKB is set, this will make the system believe that we allocated segmentSize bytes,
   // instead of _jitConfig->codeCachePadKB * 1024 bytes
   // If codeCachePadKB is not set, heapSize is segmentSize anyway
   _segment = codeCacheSegment;

   // helperTop is heapTop, usually
   // When codeCachePadKB > segmentSize, the helperTop is not at the very end of the segemnt
   _helperTop = _segment->segmentBase() + codeCacheSizeAllocated;

   _hashEntrySlab = hashEntrySlab;
   TR::CodeCacheConfig &config = manager->codeCacheConfig();

   // FIXME: try to provide different names to the mutex based on the codecache
   if (!(_mutex = TR::Monitor::create("JIT-CodeCacheMonitor-??")))
      return false;

   _hashEntryFreeList = NULL;
   _freeBlockList     = NULL;
   _flags = 0;
   _CCPreLoadedCodeInitialized = false;
   self()->unreserve();
   _almostFull = TR_no;
   _sizeOfLargestFreeColdBlock = 0;
   _sizeOfLargestFreeWarmBlock = 0;
   _lastAllocatedBlock = NULL; // MP

   *((TR::CodeCache **)(_segment->segmentBase())) = self(); // Write a pointer to this cache at the beginning of the segment
   _warmCodeAlloc = _segment->segmentBase() + sizeof(this);

   _warmCodeAlloc = align(_warmCodeAlloc, config.codeCacheAlignment() -  1);
   _warmCodeAllocMark = _warmCodeAlloc;

   if (!config.trampolineCodeSize())
      {
      // _helperTop is heapTop
      _trampolineBase = _helperTop;
      _helperBase = _helperTop;
      _trampolineReservationMark = _trampolineAllocationMark = _trampolineBase;

      // set the pre loaded per Cache Helper slab
      _CCPreLoadedCodeTop = (uint8_t *)(((size_t)_trampolineBase) & (~config.codeCacheHelperAlignmentMask()));
      _CCPreLoadedCodeBase = _CCPreLoadedCodeTop - config.ccPreLoadedCodeSize();
      TR_ASSERT( (((size_t)_CCPreLoadedCodeBase) & config.codeCacheHelperAlignmentMask()) == 0, "Per-code cache helper sizes do not account for alignment requirements." );
      _coldCodeAlloc = _CCPreLoadedCodeBase;
      _coldCodeAllocMark = _coldCodeAlloc;
      _trampolineSyncList = NULL;

      return true;
      }

   // Helpers are located at the top of the code cache (offset N), growing down towards the base (offset 0)
   size_t trampolineSpaceSize = config.trampolineCodeSize() * config.numRuntimeHelpers();
   // _helperTop is heapTop
   _helperBase = _helperTop - trampolineSpaceSize;
   _helperBase = (uint8_t *)(((size_t)_helperBase) & (~config.codeCacheTrampolineAlignmentBytes()));

   if (!config.needsMethodTrampolines())
      {
      // There is no need in method trampolines when there is going to be
      // only one code cache segment
      //
      _trampolineBase = _helperBase;
      _tempTrampolinesMax = 0;
      }
   else
      {
      // _helperTop is heapTop
      // (_helperTop - segment->heapBase) is heapSize

      _trampolineBase = _helperBase -
                        ((_helperBase - _segment->segmentBase())*config.trampolineSpacePercentage()/100);

      // Grab the configuration details from the JIT platform code
      //
      // (_helperTop - segment->heapBase) is heapSize
      config.mccCallbacks().codeCacheConfig((_helperTop - _segment->segmentBase()), &_tempTrampolinesMax);
      }

   mcc_printf("mcc_initialize: trampoline base %p\n",  _trampolineBase);

   // set the temporary trampoline slab right under the helper trampolines, should be already aligned
   _tempTrampolineTop  = _helperBase;
   _tempTrampolineBase = _tempTrampolineTop - (config.trampolineCodeSize() * _tempTrampolinesMax);
   _tempTrampolineNext = _tempTrampolineBase;

   // Check if we have enough space in the code cache to contain the trampolines
   if (_trampolineBase >= _tempTrampolineNext && config.needsMethodTrampolines())
      return false;

   // set the allocation pointer to right after the temporary trampolines
   _trampolineAllocationMark  = _tempTrampolineBase;
   _trampolineReservationMark = _trampolineAllocationMark;

   // set the pre loaded per Cache Helper slab
   _CCPreLoadedCodeTop = (uint8_t *)(((size_t)_trampolineBase) & (~config.codeCacheHelperAlignmentMask()));
   _CCPreLoadedCodeBase = _CCPreLoadedCodeTop - config.ccPreLoadedCodeSize();
   TR_ASSERT( (((size_t)_CCPreLoadedCodeBase) & config.codeCacheHelperAlignmentMask()) == 0, "Per-code cache helper sizes do not account for alignment requirements." );
   _coldCodeAlloc = _CCPreLoadedCodeBase;
   _coldCodeAllocMark = _coldCodeAlloc;

   // Set helper trampoline table available
   //
   config.mccCallbacks().createHelperTrampolines((uint8_t *)_helperBase, config.numRuntimeHelpers());

   _trampolineSyncList = NULL;
   if (_tempTrampolinesMax)
      {
      // Initialize temporary trampoline synchronization list
      if (!self()->allocateTempTrampolineSyncBlock())
         return false;
      }

   if (config.needsMethodTrampolines())
      {
      // Initialize hashtables to hold trampolines for resolved and unresolved methods
      _resolvedMethodHT   = CodeCacheHashTable::allocate(manager);
      _unresolvedMethodHT = CodeCacheHashTable::allocate(manager);
      if (_resolvedMethodHT==NULL || _unresolvedMethodHT==NULL)
         return false;
      }

   // Before returning, let's adjust the free space seen by VM.
   // Usable space is between _warmCodeAlloc and _trampolineBase. Everything else is overhead
   // Only relevant if code cache repository is used
   size_t spaceLost = (_warmCodeAlloc - _segment->segmentBase()) + (_segment->segmentTop() - _trampolineBase);
   _manager->decreaseFreeSpaceInCodeCacheRepository(spaceLost);

   return true;
   }


/*****************************************************************************
 *  Trampoline Reservation
 */


// Allocate a trampoline in given code cache
//
// An allocation MUST be preceeded by at least one reservation. If we get a case
// where we dont have reserved space, abort
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCache::allocateTrampoline()
   {
   CodeCacheTrampolineCode *trampoline = NULL;

   if (_trampolineAllocationMark > _trampolineReservationMark)
      {
      TR::CodeCacheConfig &config = _manager->codeCacheConfig();
      _trampolineAllocationMark -= config.trampolineCodeSize();
      trampoline = (CodeCacheTrampolineCode *) _trampolineAllocationMark;
      }
   else
      {
      //TR_ASSERT(0);
      }

   return trampoline;
   }


// Allocate a temporary trampoline
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCache::allocateTempTrampoline()
   {
   CodeCacheTrampolineCode *freeTrampolineSlot;

   if (_tempTrampolineNext >= _tempTrampolineTop)
      return NULL;

   freeTrampolineSlot = _tempTrampolineNext;

   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   _tempTrampolineNext += config.trampolineCodeSize();

   return freeTrampolineSlot;
   }

// Reserve space for a trampoline
//
// The returned trampoline pointer is meaningless, ie should not be used to
// create trampoline code in just yet.
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCache::reserveTrampoline()
   {
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();

   // see if we are hitting against the method body allocation pointer
   // indicating that there is no more free space left in this code cache
   if (_trampolineReservationMark < _trampolineBase + config.trampolineCodeSize())
      {
      // no free trampoline space
      return NULL;
      }

   // advance the reservation mark
   _trampolineReservationMark -= config.trampolineCodeSize();

   return (CodeCacheTrampolineCode *) _trampolineReservationMark;
   }

// Cancel a reservation for a trampoline
//
void
OMR::CodeCache::unreserveTrampoline()
   {
   // sanity check, should never have the reservation mark dip past the
   // allocation mark
   //TR_ASSERT(_trampolineReservationMark < _trampolineAllocationMark);

   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   _trampolineReservationMark += config.trampolineCodeSize();
   }


//------------------------------ reserveResolvedTrampoline ------------------
// Find or create a reservation for an resolved method trampoline.
// Method must be called with VM access in hand to prevent unloading
// Returns 0 on success or a negative error code on failure
//---------------------------------------------------------------------------
int32_t
OMR::CodeCache::reserveResolvedTrampoline(TR_OpaqueMethodBlock *method,
                                        bool inBinaryEncoding)
   {
   int32_t retValue = CodeCacheErrorCode::ERRORCODE_SUCCESS; // assume success
   TR_ASSERT(_reserved, "CodeCache %p is not reserved when calling reservedResolvedTrampoline\n", this);

   // does the platform need trampolines at all?
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return retValue;

   // scope for cache critical section
      {
      CacheCriticalSection reserveTrampoline(self());

      // see if a reservation for this method already exists, return corresponding
      // code cache if thats the case
      CodeCacheHashEntry *entry = _resolvedMethodHT->findResolvedMethod(method);
      if (!entry)
         {
         // reserve a new trampoline since we got no active reservation for given method */
         CodeCacheTrampolineCode *trampoline = self()->reserveTrampoline();
         if (trampoline)
            {
            // add hashtable entry
            if (!self()->addResolvedMethod(method))
               retValue = CodeCacheErrorCode::ERRORCODE_FATALERROR; // couldn't allocate memory from VM
            }
         else // no space in this code cache; must allocate a new one
            {
            _almostFull = TR_yes;
            retValue = CodeCacheErrorCode::ERRORCODE_INSUFFICIENTSPACE;
            if (config.verboseCodeCache())
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache %p marked as full in reserveResolvedTrampoline", this);
               }
            }
         }
      }

      return retValue;
   }



// Trampoline lookup for resolved methods
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCache::findTrampoline(TR_OpaqueMethodBlock * method)
   {
   CodeCacheTrampolineCode *trampoline;

   // scope for critical section
      {
      CacheCriticalSection resolveAndCreateTrampoline(self());

      CodeCacheHashEntry *entry = _resolvedMethodHT->findResolvedMethod(method);
      trampoline = entry->_info._resolved._currentTrampoline;
      if (!trampoline)
         {
         void *newPC = (void *) TR::Compiler->mtd.startPC(method);

         trampoline = self()->allocateTrampoline();

         self()->createTrampoline(trampoline, newPC, method);

         entry->_info._resolved._currentTrampoline = trampoline;
         entry->_info._resolved._currentStartPC = newPC;
         }
      }

   return trampoline;
   }

// Trampoline lookup for helper methods
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCache::findTrampoline(int32_t helperIndex)
   {
   CodeCacheTrampolineCode *trampoline;
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   trampoline = (CodeCacheTrampolineCode *) (_helperBase + (config.trampolineCodeSize() * helperIndex));

   //TR_ASSERT(trampoline < _helperTop);

   return trampoline;
   }


// Replace a trampoline
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCache::replaceTrampoline(TR_OpaqueMethodBlock *method,
                                void                 *oldTrampoline,
                                void                 *oldTargetPC,
                                void                 *newTargetPC,
                                bool                  needSync)
   {
   CodeCacheTrampolineCode *trampoline = oldTrampoline;
   CodeCacheHashEntry *entry;

   entry = _resolvedMethodHT->findResolvedMethod(method);
   //suspicious that this assertion is commented out...
   //TR_ASSERT(entry);

   if (needSync)
      {
      // Trampoline CANNOT be safely modified in place
      // We have to allocate a new temporary trampoline
      // and rely on the sync later on to update the old one using the
      // temporary data
      if (oldTrampoline != NULL)
         {
         // A permanent trampoline already exists, create a temporary one,
         // might fail due to lack of free slots
         trampoline = self()->allocateTempTrampoline();

         // Save the temporary trampoline entry for future temp->parm synchronization
         self()->saveTempTrampoline(entry);

         if (!trampoline)
            // Unable to fullful the replacement request, no temp space left
            return NULL;

         }
      else
         {
         // No oldTrampoline present, ie its a new trampoline creation request
         trampoline = self()->allocateTrampoline();

         entry->_info._resolved._currentTrampoline = trampoline;
         }

      // update the hash entry for this method
      entry->_info._resolved._currentStartPC = newTargetPC;

      }
   else
      {
      // Trampoline CAN be safely to modified in place
      if (oldTrampoline == NULL)
         {
         // no old trampoline present, simply allocate a new one
         trampoline = self()->allocateTrampoline();

         entry->_info._resolved._currentTrampoline = trampoline;
         }

      // update the hash entry for this method
      entry->_info._resolved._currentStartPC = newTargetPC;
      }

   return trampoline;
   }


// Synchronize all temporary trampolines in given code cache
//
void
OMR::CodeCache::syncTempTrampolines()
   {
   if (_flags & CODECACHE_FULL_SYNC_REQUIRED)
      {
      for (uint32_t entryIdx = 0; entryIdx < _resolvedMethodHT->_size; entryIdx++)
         {
         for (CodeCacheHashEntry *entry = _resolvedMethodHT->_buckets[entryIdx]; entry; entry = entry->_next)
            {
            void *newPC = (void *) TR::Compiler->mtd.startPC(entry->_info._resolved._method);
            void *trampoline = entry->_info._resolved._currentTrampoline;
            if (trampoline && entry->_info._resolved._currentStartPC != newPC)
               {
               self()->createTrampoline(trampoline,
                                        newPC,
                                        entry->_info._resolved._method);
               entry->_info._resolved._currentStartPC = newPC;
               }
            }
         }

      for (CodeCacheTempTrampolineSyncBlock *syncBlock = _trampolineSyncList; syncBlock; syncBlock = syncBlock->_next)
         syncBlock->_entryCount = 0;

      _flags &= ~CODECACHE_FULL_SYNC_REQUIRED;
      }
   else
      {
      CodeCacheTempTrampolineSyncBlock *syncBlock = NULL;
      // Traverse the list of trampoline synchronization blocks
      for (syncBlock = _trampolineSyncList; syncBlock; syncBlock = syncBlock->_next)
         {
         //TR_ASSERT(syncBlock->_entryCount <= syncBlock->_entryListSize);
         // Synchronize all stored sync requests
         for (uint32_t entryIdx = 0; entryIdx < syncBlock->_entryCount; entryIdx++)
            {
            CodeCacheHashEntry *entry = syncBlock->_hashEntryArray[entryIdx];
            void *newPC = (void *) TR::Compiler->mtd.startPC(entry->_info._resolved._method);

            // call the codegen to perform the trampoline code modification
            self()->createTrampoline(entry->_info._resolved._currentTrampoline,
                                     newPC,
                                     entry->_info._resolved._method);
            entry->_info._resolved._currentStartPC = newPC;
            }

         syncBlock->_entryCount = 0;
         }
      }

   _tempTrampolineNext = _tempTrampolineBase;
   }


// Patch the address of a method in this code cache's trampolines
//
void
OMR::CodeCache::patchCallPoint(TR_OpaqueMethodBlock *method,
                             void *callSite,
                             void *newStartPC,
                             void *extraArg)
   {
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();

   // the name is still the same. The logic of callPointPatching is as follow:
   //   look up the right codeCache,
   //   search for the hashEntry for the method,
   //   grab its current trampoline plus its current pointed-at address (if no trampoline, both are NULL).
   //   Then, call code_patching with these things as argument.

   // scope to patch trampoline
      {
      CacheCriticalSection patching(self());
      CodeCacheTrampolineCode *resolvedTramp = NULL;
      void *methodRunAddress = NULL;

      if (config.needsMethodTrampolines())
         {
         CodeCacheHashEntry *entry = _resolvedMethodHT->findResolvedMethod(method);

         if (entry)
            {
            resolvedTramp = entry->_info._resolved._currentTrampoline;
            if (resolvedTramp)
               methodRunAddress = entry->_info._resolved._currentStartPC;
            }
         }
      else if (TR::Options::getCmdLineOptions()->getOption(TR_UseGlueIfMethodTrampolinesAreNotNeeded))
         {
         // Safety measure, return to the old behaviour of always going through the glue
         return;
         }

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePatching))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PATCH, "Patching callsite=0x%p using j9method=0x%p,resolvedTramp=0x%p,methodRunAddress=0x%p,newStartPC=0x%p,extraArg=0x%p",
                                        callSite,
                                        method,
                                        resolvedTramp,
                                        methodRunAddress,
                                        newStartPC,
                                        extraArg);
         }

      // Patch the code for a method trampoline
      int32_t rc = _manager->codeCacheConfig().mccCallbacks().patchTrampoline(method, callSite, methodRunAddress, resolvedTramp, newStartPC, extraArg);

      }
   }

// Create code for a method trampoline at indicated address
//
// Calls platform dependent code to create a trampoline code snippet at the
// specified address and initialize it to jump to targetStartPC.
//
void
OMR::CodeCache::createTrampoline(CodeCacheTrampolineCode *trampoline,
                               void *targetStartPC,
                               TR_OpaqueMethodBlock *method)
   {
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   config.mccCallbacks().createMethodTrampoline(trampoline, targetStartPC, method);
   }


bool
OMR::CodeCache::saveTempTrampoline(CodeCacheHashEntry *entry)
   {
   CodeCacheTempTrampolineSyncBlock *freeSyncBlock = NULL;
   CodeCacheTempTrampolineSyncBlock *syncBlock;

   for (syncBlock = _trampolineSyncList; syncBlock; syncBlock = syncBlock->_next)
      {
      // See if the entry already exists in the block.
      // If so, don't add another copy.
      //
      for (int32_t i = 0; i < syncBlock->_entryCount; i++)
         {
         if (syncBlock->_hashEntryArray[i] == entry)
            return true;
         }

      // Remember the first sync block that has some free slots
      //
      if (syncBlock->_entryCount < syncBlock->_entryListSize && !freeSyncBlock)
         freeSyncBlock = syncBlock;
      }


   if (!freeSyncBlock)
      {
      // Allocate a new temp trampoline sync block
      //
      if (!self()->allocateTempTrampolineSyncBlock())
         {
         // Can't allocate another block, mark the code cache as needing a
         // full/slow sync during a safe point
         //
         _flags |= CODECACHE_FULL_SYNC_REQUIRED;
         return false;
         }
      // New block is now at the at head of list
      //
      freeSyncBlock = _trampolineSyncList;
      }

   freeSyncBlock->_hashEntryArray[freeSyncBlock->_entryCount] = entry;
   freeSyncBlock->_entryCount++;

   return true;
   }

// Allocate a new temp trampoline sync block and add it to the head of the list
// of such blocks.
//
bool
OMR::CodeCache::allocateTempTrampolineSyncBlock()
   {
   CodeCacheTempTrampolineSyncBlock *block = static_cast<CodeCacheTempTrampolineSyncBlock*>(_manager->getMemory(sizeof(CodeCacheTempTrampolineSyncBlock)));
   if (!block)
      return false;

   TR::CodeCacheConfig &config = _manager->codeCacheConfig();

   block->_hashEntryArray = static_cast<CodeCacheHashEntry**>(_manager->getMemory(sizeof(CodeCacheHashEntry *) * config.codeCacheTempTrampolineSyncArraySize()));
   if (!block->_hashEntryArray)
      {
      _manager->freeMemory(block);
      return false;
      }

   mcc_printf("mcc_temptrampolinesyncblock: trampolineSyncList = %p\n",  _trampolineSyncList);
   mcc_printf("mcc_temptrampolinesyncblock: block = %p\n",  block);

   block->_entryCount = 0;
   block->_entryListSize = config.codeCacheTempTrampolineSyncArraySize();
   block->_next = _trampolineSyncList;
   _trampolineSyncList = block;

   return true;
   }

// Allocate a trampoline hash entry
//

OMR::CodeCacheHashEntry *
OMR::CodeCache::allocateHashEntry()
   {
   CodeCacheHashEntry *entry;
   CodeCacheHashEntrySlab *slab = _hashEntrySlab;

   // Do we have any free entries that were previously allocated from (perhaps)
   // a slab or this list and were released onto it?
   if (_hashEntryFreeList)
      {
      entry = _hashEntryFreeList;
      _hashEntryFreeList = entry->_next;
      return entry;
      }

   // Look for a slab with free hash entries
   if ((slab->_heapAlloc + sizeof(CodeCacheHashEntry)) > slab->_heapTop)
      {
      TR::CodeCacheConfig & config = _manager->codeCacheConfig();
      slab = CodeCacheHashEntrySlab::allocate(_manager, config.codeCacheHashEntryAllocatorSlabSize());
      if (slab == NULL)
         return NULL;
      slab->_next = _hashEntrySlab;
      _hashEntrySlab = slab;
      }

   entry = (CodeCacheHashEntry *) slab->_heapAlloc;
   slab->_heapAlloc += sizeof(CodeCacheHashEntry);

   return entry;
   }

// Release an unused trampoline hash entry back onto the free list
//

void
OMR::CodeCache::freeHashEntry(CodeCacheHashEntry *entry)
   {
   // put it back onto the first slabs free list, see comment above
   entry->_next = _hashEntryFreeList;
   _hashEntryFreeList = entry;
   }


// Add a resolved method to the trampoline hash table
//
bool
OMR::CodeCache::addResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   CodeCacheHashEntry *entry = self()->allocateHashEntry();
   if (!entry)
      return false;

   entry->_key = _resolvedMethodHT->hashResolvedMethod(method);
   entry->_info._resolved._method = method;
   entry->_info._resolved._currentStartPC = NULL;
   entry->_info._resolved._currentTrampoline = NULL;
   _resolvedMethodHT->add(entry);
   return true;
   }


// empty by default
OMR::CodeCacheMethodHeader *
OMR::CodeCache::addFreeBlock(void * metaData)
   {
   return NULL;
   }

// May add the block between start and end to freeBlockList.
// returns false if block is not added
// No lock is needed because this operation is performed at GC points
// when GC has exclusive access; This is why the allocation (by the compilation
// thread needs to be done with VM access
// It can also be done by a compilation thread that performs a resize, but we
// also take VM access around resize.
bool
OMR::CodeCache::addFreeBlock2WithCallSite(uint8_t *start,
                                        uint8_t *end,
                                        char *file,
                                        uint32_t lineNumber)
   {
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();

   // align start on a code cache alignment boundary
   uint8_t *start_o = start;
   uint32_t round = config.codeCacheAlignment() - 1;
   start = align(start, round);

   // make sure aligning start didn't push it past end
   if (end <= (start+sizeof(CodeCacheFreeCacheBlock)))
      {
      if (config.verboseReclamation())
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"addFreeBlock2[%s.%d]: failed to add free block. start = 0x%016x end = 0x%016x alignment = 0x%04x sizeof(CodeCacheFreeCacheBlock) = 0x%08x",
            file, lineNumber, start_o, end, config.codeCacheAlignment(), sizeof(CodeCacheFreeCacheBlock));
         }
      return false;
      }

   uint64_t size = end - start; // Size of space to be freed

   // Destroy the eyeCatcher; note that there might not be an eyecatcher at all
   //
   if (size >= sizeof(CodeCacheMethodHeader))
      ((CodeCacheMethodHeader*)start)->_eyeCatcher[0] = 0;

   //fprintf(stderr, "--ccr-- newFreeBlock size %d at %p\n", size, start);
   CodeCacheFreeCacheBlock *mergedBlock = NULL;
   CodeCacheFreeCacheBlock *link = NULL;
   if (_freeBlockList)
      {
      CodeCacheFreeCacheBlock *curr;
      // find the insertion point
      for (curr = _freeBlockList; curr->_next && (uint8_t *)(curr->_next) < start; curr = curr->_next)
         {}

      if (start < (uint8_t *)curr && (uint8_t *)curr - end < sizeof(CodeCacheFreeCacheBlock))
         {
         // merge with the curr block ahead, which is also the first block
         TR_ASSERT(end <= (uint8_t *)curr, "assertion failure"); // check for no overlap of blocks
         // we should not merge warm block with cold blocks
         if (!(start < _warmCodeAlloc && (uint8_t *)curr >= _coldCodeAlloc))
            {
            // which is also the first block
            link = (CodeCacheFreeCacheBlock *) start;
            mergedBlock = curr;
            //fprintf(stderr, "--ccr-- merging new free block of the size %d with a block of the size %d at %p\n", size, curr->size, link);
            link->_size = (uint8_t *)curr + curr->_size - start;
            link->_next = curr->_next;
            _freeBlockList = link;
            //fprintf(stderr, "--ccr-- new merged free block's size is %d\n", link->size);
            }
         }
      else if (curr->_next && ((uint8_t *)curr->_next - end < sizeof(CodeCacheFreeCacheBlock)) &&
         !(start < _warmCodeAlloc && (uint8_t *)curr->_next >= _coldCodeAlloc))
         {
         // merge with the next block, but don't merge warm blocks with cold blocks
         if ((start - ((uint8_t *)curr + curr->_size) < sizeof(CodeCacheFreeCacheBlock)) &&
             !((uint8_t *)curr < _warmCodeAlloc && start >= _coldCodeAlloc))
            {
            // merge with the previous and the next blocks
            mergedBlock = curr;
            //fprintf(stderr, "--ccr-- merging new free block of the size %d with blocks of the size %d and %d at %p\n", size, curr->_size, curr->_next->_size, curr);
            curr->_size = (uint8_t *)curr->_next + curr->_next->_size - (uint8_t *)curr;
            curr->_next = curr->_next->_next;
            //fprintf(stderr, "--ccr-- new merged free block's size is %d\n", curr->_size);
            link = curr;
#ifdef DEBUG
            start = (uint8_t *)curr;
#endif
            }
         else
            {
            mergedBlock = curr->_next;
            link = (CodeCacheFreeCacheBlock *) start;
            //fprintf(stderr, "--ccr-- merging new free block of the size %d with a block of the size %d at %p\n", size, curr->next->size, link);
            link->_size = (uint8_t *)curr->_next + curr->_next->_size - start;
            link->_next = curr->_next->_next;
            curr->_next = link;
            //fprintf(stderr, "--ccr-- new merged free block's size is %d\n", link->_size);
            }
         }
      else if ((uint8_t *)curr < start && start - ((uint8_t *)curr + curr->_size) < sizeof(CodeCacheFreeCacheBlock))
         {
         // merge with the previous block
         if (!((uint8_t *)curr < _warmCodeAlloc && start >= _coldCodeAlloc))
            {
            mergedBlock = curr;
            curr->_size = start + size - (uint8_t *)curr;
            //fprintf(stderr, "--ccr-- new merged free block's size is %d\n", curr->_size);
            link = curr;
#ifdef DEBUG
            start = (uint8_t *)curr;
#endif
            }
         }
      if (!link) // no merging happened
         {
         link = (CodeCacheFreeCacheBlock *) start;
         link->_size = size;
         if (start < (uint8_t *)curr)
            {
            link->_next = _freeBlockList;
            _freeBlockList = link;
            }
         else
            {
            link->_next = curr->_next;
            curr->_next = link;
            }
         }
      }
   else // This is the first block in the list
      {
      _freeBlockList = (CodeCacheFreeCacheBlock *) start;
      _freeBlockList->_size = size;
      _freeBlockList->_next = NULL;
      //updateMaxSizeOfFreeBlocks(_freeBlockList, _freeBlockList->_size);
      link = _freeBlockList;
      }

   self()->updateMaxSizeOfFreeBlocks(link, link->_size);

   if (config.verboseReclamation())
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"--ccr-- addFreeBlock2WithCallSite CC=%p start=%p end=%p mergedBlock=%p link=%p link->_size=%u, _sizeOfLargestFreeWarmBlock=%d _sizeOfLargestFreeColdBlock=%d warmCodeAlloc=%p coldBlockAlloc=%p",
         this,  (void*)start, (void*)end, mergedBlock, link, (uint32_t)link->_size, _sizeOfLargestFreeWarmBlock, _sizeOfLargestFreeColdBlock, _warmCodeAlloc, _coldCodeAlloc);
      }
#ifdef DEBUG
   uint8_t *paintStart = start + sizeof(CodeCacheFreeCacheBlock);
   memset((void*)paintStart, 0xcc, ((CodeCacheFreeCacheBlock*)start)->_size - sizeof(CodeCacheFreeCacheBlock));
#endif

   if (config.doSanityChecks())
      self()->checkForErrors();

   return true;
   }


void
OMR::CodeCache::updateMaxSizeOfFreeBlocks(CodeCacheFreeCacheBlock *blockPtr, size_t blockSize)
   {
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   if (config.codeCacheFreeBlockRecylingEnabled())
      {
      if ((uint8_t *)blockPtr < _warmCodeAlloc)
         {
         if (blockSize > _sizeOfLargestFreeWarmBlock)
            {
            //fprintf(stderr, "_sizeOfLargestFreeBlock for cache %p increased to %d\n", this, blockSize);
            _sizeOfLargestFreeWarmBlock = blockSize;
            }
         }
      else // block belongs to cold code
         {
         if (blockSize > _sizeOfLargestFreeColdBlock)
            _sizeOfLargestFreeColdBlock = blockSize;
         }
      }
   }

// Find the smallest free block that will satisfy the request.
//
// isCold indicates whether a warm or cold block of memory is required.
//
uint8_t *
OMR::CodeCache::findFreeBlock(size_t size, bool isCold, bool isMethodHeaderNeeded)
   {
   CodeCacheFreeCacheBlock *currLink;
   CodeCacheFreeCacheBlock *prevLink;
   CodeCacheFreeCacheBlock *bestFitLink = NULL, *bestFitLinkPrev = NULL;
   CodeCacheFreeCacheBlock *biggestLink = NULL;
   CodeCacheFreeCacheBlock *secondBiggestLink = NULL;

   TR_ASSERT(_freeBlockList, "Because we first checked that a freeBlockExists, freeBlockList cannot be null");

   // Find the smallest free link to fit the requested blockSize
   for (currLink = _freeBlockList, prevLink = NULL; currLink; prevLink = currLink, currLink = currLink->_next)
      {
      if (isCold)
         {
         if ((void*)currLink < (void*)_coldCodeAlloc)
            continue;
         }
      else
         {
         if ((void*)currLink >= (void*)_warmCodeAlloc)
            continue;
         // curLink is warm code
         }

      //fprintf(stderr, "cache %p findFreeBlock bs=%u\n", this, currLink->size);
      if (!biggestLink)
         {
         biggestLink = currLink;
         }
      else
         {
         if (currLink->_size >= biggestLink->_size) // >= is important; do not use > only as secondBiggestLink may be wrong
            {
            secondBiggestLink = biggestLink;
            biggestLink = currLink;
            }
         else
            {
            if (secondBiggestLink)
               {
               if (currLink->_size >= secondBiggestLink->_size)
                  secondBiggestLink = currLink;
               }
            else
               {
               secondBiggestLink = currLink;
               }
            }
         }

      if (currLink->_size >= size)
         {
         if (!bestFitLink)
            {
            bestFitLink = currLink;
            bestFitLinkPrev = prevLink;
            }
         else if (bestFitLink->_size > currLink->_size)
            {
            bestFitLink = currLink;
            bestFitLinkPrev = prevLink;
            }
         }
      } // end for

   // safety net
   TR_ASSERT(biggestLink, "There must be a biggestLink");
   TR_ASSERT(bestFitLink, "There must be a bestFitLink");

   TR::CodeCacheConfig & config = _manager->codeCacheConfig();
   if (!isCold)
      {
      TR_ASSERT(!config.codeCacheFreeBlockRecylingEnabled() ||
              _sizeOfLargestFreeWarmBlock == biggestLink->_size, "_sizeOfLargestFreeWarmBlock=%d  biggestLink->_size=%d",
           _sizeOfLargestFreeWarmBlock, (int32_t)biggestLink->_size);
      }
   else
      {
      TR_ASSERT(!config.codeCacheFreeBlockRecylingEnabled() ||
         _sizeOfLargestFreeColdBlock == biggestLink->_size, "assertion failure");
      }

   if (bestFitLink)
      {
      // Fix the linked list by removing the allocated block AND if there is any unused
      // space left in the currLink chunk, reclaim it and put back on the freeList
      CodeCacheFreeCacheBlock *leftBlock = self()->removeFreeBlock(size, bestFitLinkPrev, bestFitLink);

      if (bestFitLink == biggestLink)  // Size of biggest might have changed
         {
         uint64_t leftBlockSize     = leftBlock ? leftBlock->_size : 0;
         uint64_t secondBiggestSize = secondBiggestLink ? secondBiggestLink->_size : 0;
         uint64_t biggestSize       = leftBlockSize > secondBiggestSize ? leftBlockSize : secondBiggestSize;
         if (!isCold)
            {
            _sizeOfLargestFreeWarmBlock = (size_t)biggestSize;
            }
         else
            _sizeOfLargestFreeColdBlock = (size_t)biggestSize;
         }
     //fprintf(stderr, "--ccr-- reallocate free'd block of size %d\n", size);
     if (config.verboseReclamation())
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"--ccr- findFreeBlock: CodeCache=%p size=%u isCold=%d bestFitLink=%p bestFitLink->size=%u leftBlock=%p", this, size, isCold, bestFitLink, bestFitLink->_size, leftBlock);
         }
      }
   // Because we call this method only after we made sure a free block exists
   // this function can never return NULL
   TR_ASSERT(bestFitLink, "FindFreeBlock return NULL");

   if (isMethodHeaderNeeded)
      self()->writeMethodHeader(bestFitLink, bestFitLink->_size, isCold);

   if (config.doSanityChecks())
      self()->checkForErrors();

   return (uint8_t *) bestFitLink;
   }


// Remove a free block from the list of free blocks for this code cache to make
// it available for re-use.
//
// blockSize is the amount of memory needed from this free block.
//
// The function returns the remaining part of the block that was split
OMR::CodeCacheFreeCacheBlock *
OMR::CodeCache::removeFreeBlock(size_t blockSize,
                              CodeCacheFreeCacheBlock *prev,
                              CodeCacheFreeCacheBlock *curr)
   {
   CodeCacheFreeCacheBlock *next = curr->_next;

   // Is there any left over space in the current link? Save it as a
   // separate link and adjust the sizes of the two split resulting blocks
   if (curr->_size - blockSize >= MIN_SIZE_BLOCK)
      {
      size_t splitSize = curr->_size - blockSize; // remaining portion
      curr->_size = blockSize;
      curr = (CodeCacheFreeCacheBlock *) ((uint8_t *) curr + blockSize);
      curr->_size = splitSize;
      curr->_next = next;

      if (prev)
         prev->_next = curr;
      else
         _freeBlockList = curr;
      return curr;
      }
   else // Use the entire block
      {
      if (prev)
         prev->_next = next;
      else
         _freeBlockList = next;
      return NULL;
      }
   }


void
OMR::CodeCache::dumpCodeCache()
   {
   printf("Code Cache @%p\n", this);
   printf("  |-- warmCodeAlloc          = 0x%08x\n", _warmCodeAlloc );
   printf("  |-- coldCodeAlloc          = 0x%08x\n", _coldCodeAlloc );
   printf("  |-- tempTrampsMax          = %d\n",     _tempTrampolinesMax );
   printf("  |-- flags                  = %d\n",     _flags );
   printf("  |-- next                   = 0x%p\n",   _next );
   }


void
OMR::CodeCache::printOccupancyStats()
   {
   fprintf(stderr, "Code Cache @%p flags=0x%x almostFull=%d\n", this, _flags, _almostFull);
   fprintf(stderr, "   cold-warm hole size        = %8u bytes\n", self()->getFreeContiguousSpace());
   fprintf(stderr, "   warmCodeAlloc=%p coldCodeAlloc=%p\n", (void*)_warmCodeAlloc, (void*)_coldCodeAlloc);
   if (_freeBlockList)
      {
      fprintf(stderr, "   sizeOfLargestFreeColdBlock = %8d bytes\n", _sizeOfLargestFreeColdBlock);
      fprintf(stderr, "   sizeOfLargestFreeWarmBlock = %8d bytes\n", _sizeOfLargestFreeWarmBlock);
      fprintf(stderr, "   reclaimed sizes:");
      // scope for critical section
         {
         CacheCriticalSection resolveAndCreateTrampoline(self());
         for (CodeCacheFreeCacheBlock *currLink = _freeBlockList; currLink; currLink = currLink->_next)
            fprintf(stderr, " %u", currLink->_size);
         }
      fprintf(stderr, "\n");
      }

   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   if (config.trampolineCodeSize())
      {
      fprintf(stderr, "   trampoline free space = %d (temp=%d)\n",
         (int32_t)(_trampolineReservationMark - _trampolineBase),
         (int32_t)(_tempTrampolineNext - _tempTrampolineBase));
      }
   }


void
OMR::CodeCache::printFreeBlocks()
   {
   fprintf(stderr, "List of free blocks:\n");
   // scope for cache walk
      {
      CacheCriticalSection walkList(self());
      for (CodeCacheFreeCacheBlock *currLink = _freeBlockList; currLink; currLink = currLink->_next)
         fprintf(stderr, "%p - %p\n", currLink, ((char*)currLink) + currLink->_size);
      }
   }


//-------------------- checkForErrors ---------------------------------
// Scan the entire list of free blocks and make sure numbers make sense
// Blocks should be in ascending order of their addresses
// StartBlock+size should lead to an allocated portion (otherwise a merge
// would have happened)
// StartBlock and StartBlock+size should be within the cache
//---------------------------------------------------------------------
void
OMR::CodeCache::checkForErrors()
   {
   if (_freeBlockList)
      {
      bool doCrash = false;
      size_t maxFreeWarmSize = 0, maxFreeColdSize = 0;
      // scope for cache walk
         {
         CacheCriticalSection walkFreeList(self());

         for (CodeCacheFreeCacheBlock *currLink = _freeBlockList; currLink; currLink = currLink->_next)
            {
            // Does the size look right?
            uint64_t cacheSize = _segment->segmentTop() - _segment->segmentBase();
            if (currLink->_size > cacheSize)
               {
               fprintf(stderr, "checkForErrors cache %p: Error: Size of the free block %u is bigger than the size of the cache %u\n", this, (uint32_t)currLink->_size, (uint32_t)cacheSize);
               doCrash = true;
               }
            // Is the block fully contained in this code cache?
            if ((uint8_t*)currLink < _segment->segmentBase()+sizeof(void*) || (uint8_t*)currLink > _segment->segmentTop())
               {
               fprintf(stderr, "checkForErrors cache %p: Error: curLink %p is outside cache boundaries\n", this, currLink);
               doCrash = true;
               }
            uint8_t *endBlock = (uint8_t *)currLink + currLink->_size;
            if (endBlock < _segment->segmentBase()+sizeof(void*)  || endBlock > _segment->segmentTop())
               {
               fprintf(stderr, "checkForErrors cache %p: Error: End of block %p residing at %p is outside cache boundaries\n", this, currLink, endBlock);
               doCrash = true;
               }
            // Next free block (if any) should be after the end of this free block
            if (currLink->_next)
               {
               if ((uint8_t*)currLink->_next == endBlock)
                  {
                  // Two freed blocks can be adjacent if one belongs to the warm region
                  // and the other one belongs to the cold region
                  if (!((uint8_t*)currLink < _warmCodeAlloc && endBlock >= _coldCodeAlloc))
                     {
                     fprintf(stderr, "checkForErrors cache %p: Error: missed freed block coalescing opportunity. Next block (%p) is adjacent to current one %p-%p\n", this, currLink->_next, currLink, endBlock);
                     doCrash = true;
                     }
                  }
               else
                  {
                  if ((uint8_t*)currLink->_next <= endBlock)
                     {
                     fprintf(stderr, "checkForErrors cache %p: Error: next block (%p) should come after end of current one %p-%p\n", this, currLink->_next, currLink, endBlock);
                     doCrash = true;;
                     }
                  // A free block is always followed by a used block except when this is the last free block in the warm region
                  if (endBlock != _warmCodeAlloc)
                     {
                     // There should be valid code between this block and the next
                     uint8_t* eyeCatcherPosition = endBlock+offsetof(CodeCacheMethodHeader, _eyeCatcher);
                     if (*eyeCatcherPosition != *(_manager->codeCacheConfig().warmEyeCatcher()))
                        {
                        fprintf(stderr, "checkForErrors cache %p: Error: block coming after this free one (%p-%p) does not have the eye catcher but %u\n", this, currLink, endBlock, *eyeCatcherPosition);
                        doCrash = true;
                        }
                     }
                  }
               }
            if ((uint8_t*)currLink < _warmCodeAlloc) // warm block
               {
               if (currLink->_size > maxFreeWarmSize)
                  maxFreeWarmSize = currLink->_size;
               }
            else // cold block
               {
               if (currLink->_size > maxFreeColdSize)
                  maxFreeColdSize = currLink->_size;
               }
            } // end for
         if (_sizeOfLargestFreeWarmBlock != maxFreeWarmSize)
            {
            fprintf(stderr, "checkForErrors cache %p: Error: _sizeOfLargestFreeWarmBlock(%d) != maxFreeWarmSize(%d)\n", this, _sizeOfLargestFreeWarmBlock, maxFreeWarmSize);
            doCrash = true;
            }
         if (_sizeOfLargestFreeColdBlock != maxFreeColdSize)
            {
            fprintf(stderr, "checkForErrors cache %p: Error: _sizeOfLargestFreeColdBlock(%d) != maxFreeColdSize(%d)\n", this, _sizeOfLargestFreeColdBlock, maxFreeColdSize);
            doCrash = true;
            }

         // Blocks must come one after another;
         // 1. A free block must be followed by a used block;
         //    The only exception is when we make transition from warm to cold section
         // 2. A used block must be followed by another used block or by a free block
         // 3. All used blocks must have a CodeCacheMethodHeader with size as first parameter, folowed by eyeCatcher and metaData
         TR::CodeCacheConfig &config = _manager->codeCacheConfig();
         uint8_t *start = align(((uint8_t*)_segment->segmentTop()) + sizeof(char*), config.codeCacheAlignment() -  1);
         uint8_t *prevBlock = NULL;
         while (start < this->_trampolineBase)
            {
            // Is it a free segment?
            bool freeSeg = false;
            CodeCacheFreeCacheBlock *currLink = _freeBlockList;
            for (; currLink; currLink = currLink->_next)
               if (start == (uint8_t *)currLink)
                  {
                  freeSeg = true;
                  break;
                  }
               if (freeSeg)
                  {
                  prevBlock = start;
                  start = (uint8_t *)currLink + currLink->_size;
                  continue;
                  }
               else // used seg
                  {
                  // There should be valid code between this block and the next
                  uint8_t * eyeCatcherPosition = start+offsetof(CodeCacheMethodHeader, _eyeCatcher);
                  if (*eyeCatcherPosition != *(_manager->codeCacheConfig().warmEyeCatcher()))
                     {
                     prevBlock = start;
                     start = start + ((CodeCacheMethodHeader*)start)->_size;
                     if (start >= _warmCodeAlloc)
                        start = _coldCodeAlloc;
                     continue;
                     }
                  else
                     {
                     fprintf(stderr, "checkForErrors cache %p: block %p is not in the free list and does not have eye-catcher; prevBlock=%p\n", this, start, prevBlock);
                     doCrash = true;
                     break;
                     }
                  }
            } // end while
         }

      if (doCrash)
         {
         self()->dumpCodeCache();
         self()->printOccupancyStats();
         self()->printFreeBlocks();
         *((int32_t*)1) = 0xffffffff; // cause a crash
         }
      }
   }


OMR::CodeCacheHashEntry *
OMR::CodeCache::findResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   return _resolvedMethodHT->findResolvedMethod(method);
   }


void
OMR::CodeCache::findOrAddResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   CacheCriticalSection codeCacheLock(self());
   CodeCacheHashEntry *entry = self()->findResolvedMethod(method);
   if (!entry)
      self()->addResolvedMethod(method);
   }

// Allocate code memory within a code cache. Some of the allocation may come from the free list
//

uint8_t *
OMR::CodeCache::allocateCodeMemory(size_t warmCodeSize,
                                 size_t coldCodeSize,
                                 uint8_t **coldCode,
                                 bool needsToBeContiguous,
                                 bool isMethodHeaderNeeded)
   {
   TR::CodeCacheConfig & config = _manager->codeCacheConfig();

   uint8_t * warmCodeAddress = NULL;
   uint8_t * coldCodeAddress = NULL;
   uint8_t * cacheHeapAlloc;
   bool warmIsFreeBlock = false;
   bool coldIsFreeBlock = false;

   size_t warmSize = warmCodeSize;
   size_t coldSize = coldCodeSize;
   _manager->performSizeAdjustments(warmSize, coldSize, needsToBeContiguous, isMethodHeaderNeeded); // side effect on warmSize and coldSize

   // Acquire mutex because we are walking the list of free blocks
   CacheCriticalSection walkingFreeList(self());

   // See if we can get a warm and/or cold block from the reclaimed method list
   if (!needsToBeContiguous)
      {
      if (warmSize)
         warmIsFreeBlock = _sizeOfLargestFreeWarmBlock >= warmSize;
      if (coldSize)
         coldIsFreeBlock = _sizeOfLargestFreeColdBlock >= coldSize;
      }

   // We want to avoid the case where we allocate the warm portion and then
   // discover that we cannot allocate the cold portion and switch to
   // another code cache. Therefore, lets make sure that we can allocate
   // cold before proceding with the allocation
   if (coldSize != 0 && !coldIsFreeBlock &&
       coldSize + (warmIsFreeBlock ? 0 : warmSize) > self()->getFreeContiguousSpace())
      {
      return NULL;
      }

   size_t round = config.codeCacheAlignment() - 1;
   if (!warmIsFreeBlock)
      {
      if (warmSize)
         {
         // Try to allocate a block from the code cache heap
         cacheHeapAlloc  = _warmCodeAlloc;

         cacheHeapAlloc = (uint8_t*)(((size_t)cacheHeapAlloc + round) & ~round);
         warmCodeAddress = cacheHeapAlloc;
         cacheHeapAlloc += warmSize;

         // Do we have enough space in codeCache to allocate the warm code?
         if (cacheHeapAlloc > _coldCodeAlloc)
            {
            // No - just return failure
            //
            return NULL;
            }

         // _warmCodeAlloc will change to its new value 'cacheHeapAlloc'
         // Thus the free code cache space decreases by (cacheHeapAlloc-_warmCodeAlloc)
         _manager->decreaseFreeSpaceInCodeCacheRepository(cacheHeapAlloc-_warmCodeAlloc);
         _warmCodeAlloc = cacheHeapAlloc;
         if (isMethodHeaderNeeded)
            self()->writeMethodHeader(warmCodeAddress, warmSize, false);
         }
      else
         warmCodeAddress = _warmCodeAlloc;
      }
   else
      {
      warmCodeAddress = self()->findFreeBlock(warmSize, false, isMethodHeaderNeeded); // side effect: free block is unlinked
      // If isMeathodHeaderNeeded is true, warmCodeAddress will look like a pointer to CodeCacheMethodHeader
      // Otherwise, it look like a pointer to CodeCacheFreeCacheBlock

      // Note that findFreeBlock may return a block which is slightly bigger than what we wanted
      // Change the warmSize to the higher size so that the size of the block is correctly maintained
      //if (((CodeCacheFreeCacheBlock*)warmCodeAddress)->size > warmSize)
      //   warmSize = ((CodeCacheFreeCacheBlock*)warmCodeAddress)->size;
      }

   if (!coldIsFreeBlock)
      {
      if (coldSize)
         {
         cacheHeapAlloc = _coldCodeAlloc - coldSize;
         cacheHeapAlloc = (uint8_t *)(((size_t)cacheHeapAlloc) & ~round);
         // Do we have enough space in codeCache to allocate the warm code?
         if (cacheHeapAlloc < _warmCodeAlloc)
            {
            // This case should never happen now because we checked above that both warm and cold can be allocated
            TR_ASSERT(false, "Must be able to find space for cold code");
            // No - just return failure
            //
            if (!warmIsFreeBlock)
               {
               _warmCodeAlloc = warmCodeAddress;
               }
            return NULL;
            }
         _manager->decreaseFreeSpaceInCodeCacheRepository(_coldCodeAlloc - cacheHeapAlloc);
         _coldCodeAlloc = cacheHeapAlloc;
         coldCodeAddress = cacheHeapAlloc;
         if (isMethodHeaderNeeded)
            self()->writeMethodHeader(coldCodeAddress, coldSize, true);
         }
      else
         coldCodeAddress = _coldCodeAlloc;
      }
   else
      {
      coldCodeAddress = self()->findFreeBlock(coldSize, true, isMethodHeaderNeeded); // side effect: free block is unlinked
      // Note that findFreeBlock may return a block which is slightly bigger than what we wanted
      // Change the warmSize to the higher size so that the size of the block is correctly maintained
      //if (((CodeCacheFreeCacheBlock*)coldCodeAddress)->size > coldSize)
      //  coldSize = ((CodeCacheFreeCacheBlock*)coldCodeAddress)->size;
      }

   /////diagnostic("allocated method @0x%p [cc=0x%p] (reclaimed block)\n", methodBlockAddress, this);

   _lastAllocatedBlock = (CodeCacheMethodHeader*)warmCodeAddress; // MP
   if (isMethodHeaderNeeded)
      {
      if (warmSize)
         {
         warmCodeAddress += sizeof(CodeCacheMethodHeader);
         }
      if (coldSize)
         {
         coldCodeAddress += sizeof(CodeCacheMethodHeader);
         }
      }

   if (!needsToBeContiguous)
      *coldCode = coldCodeAddress;
   else
      *coldCode = warmCodeAddress;
   return warmCodeAddress;
   }


void *
OMR::CodeCache::getCCPreLoadedCodeAddress(TR_CCPreLoadedCode h, TR::CodeGenerator *codeGenerator)
   {
   if (!_CCPreLoadedCodeInitialized)
      {
      if (TR_numCCPreLoadedCode)
         {
         TR::CodeCacheConfig &config = _manager->codeCacheConfig();
         config.mccCallbacks().createCCPreLoadedCode(_CCPreLoadedCodeBase,
                                                     _CCPreLoadedCodeTop,
                                                     _CCPreLoadedCode,
                                                     codeGenerator);
         _CCPreLoadedCodeInitialized = true;
         }
      }
   return (h >= 0 && h < TR_numCCPreLoadedCode) ? _CCPreLoadedCode[h] : (void *) (uintptr_t) 0xDEADBEEF;
   }


OMR::CodeCacheErrorCode::ErrorCode
OMR::CodeCache::reserveNTrampolines(int64_t n)
   {
   CacheCriticalSection updatingCodeCache(self());
   CodeCacheErrorCode::ErrorCode status = CodeCacheErrorCode::ERRORCODE_SUCCESS;
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   size_t size = n * config.trampolineCodeSize();

   if (size)
      {
      if (_trampolineReservationMark >= _trampolineBase + size)
         {
         _trampolineReservationMark -= size;
         }
      else
         {
         status = CodeCacheErrorCode::ERRORCODE_INSUFFICIENTSPACE;
         _almostFull = TR_yes;
         self()->unreserve();
         if (config.verboseCodeCache())
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache %p marked as full in reserveNTrampoline", this);
            }
         }
      }

   return status;
   }


// Allocate and initialize a new code cache
// If reservingCompThreadID >= -1, then the new code codecache will be reserved
// A value of -1 for this parameter means that an application thread is requesting the reservation
// A positive value means that a compilation thread is requesting the reservation
// A value of -2 (or less) means that no reservation is requested
//
TR::CodeCache *
OMR::CodeCache::allocate(TR::CodeCacheManager *manager,
                       size_t segmentSize,
                       int32_t reservingCompThreadID)
   {
   TR::CodeCacheConfig & config = manager->codeCacheConfig();
   bool verboseCodeCache = config.verboseCodeCache();

   size_t codeCacheSizeAllocated;
   TR::CodeCacheMemorySegment *codeCacheSegment = manager->getNewCacheMemorySegment(segmentSize, codeCacheSizeAllocated);

   if (codeCacheSegment)
      {
      // allocate hash entry slab now, instead of after codeCache allocation as its less expensive and easier to
      // fully unroll
      CodeCacheHashEntrySlab *hashEntrySlab = CodeCacheHashEntrySlab::allocate(manager, config.codeCacheHashEntryAllocatorSlabSize());
      if (hashEntrySlab)
         {
         uint8_t *start = NULL;
         TR::CodeCache *codeCache = manager->allocateCodeCacheObject(codeCacheSegment,
                                                                     codeCacheSizeAllocated,
                                                                     hashEntrySlab);

         if (codeCache)
            {
            // If we wanted to reserve this code cache, then mark it as reserved now
            if (reservingCompThreadID >= -1)
               {
               codeCache->reserve(reservingCompThreadID);
               }

            // Add it to our list of code caches
            manager->addCodeCache(codeCache);

            if (verboseCodeCache)
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache allocated %p @ " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT " HelperBase:" POINTER_PRINTF_FORMAT, codeCache, codeCache->getCodeBase(), codeCache->getCodeTop(), codeCache->_helperBase);
               }

            return codeCache;
            }

         // failed to allocate code cache so need to free hash entry slab
         hashEntrySlab->free(manager);
         }

      // failed to allocate hash entry slab so need to free code cache segment
      if (manager->usingRepository())
         {
         // return back the portion we carved
         manager->undoCarvingFromRepository(codeCacheSegment);
         }
      else
         {
         manager->freeMemorySegment(codeCacheSegment);
         }
      }

   if (verboseCodeCache)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache maximum allocated");
      }

   return NULL;
   }

// The following two routines can be used for TOSS_CODE situations where we
// allocate some space for code, but we discard it immediately after the compilation
// Code cache must be reserved
void
OMR::CodeCache::setAllocationMark()
   {
   _warmCodeAllocMark = _warmCodeAlloc;
   _coldCodeAllocMark = _coldCodeAlloc;
   }

void
OMR::CodeCache::resetToAllocationMark()
   {
   TR_ASSERT((uintptr_t)_warmCodeAlloc >= (uintptr_t)_warmCodeAllocMark,
      "_warmCodeAlloc=%p must be larger or equal to _warmCodeAllocMark=%p", _warmCodeAlloc, _warmCodeAllocMark);
   TR_ASSERT((uintptr_t)_coldCodeAlloc <= (uintptr_t)_coldCodeAllocMark,
      "_coldCodeAlloc=%p must be smaller or equal to _coldCodeAllocMark=%p", _coldCodeAlloc, _coldCodeAllocMark);
   size_t warmSize = _warmCodeAlloc - _warmCodeAllocMark;
   size_t coldSize = _coldCodeAllocMark - _coldCodeAlloc;
   _manager->increaseFreeSpaceInCodeCacheRepository(warmSize + coldSize);
   _warmCodeAlloc = _warmCodeAllocMark;
   _coldCodeAlloc = _coldCodeAllocMark;
   }
