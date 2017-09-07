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

#include "runtime/OMRCodeCacheManager.hpp"

#include <algorithm>                    // for std::max, etc
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t, int32_t, uint32_t, etc
#include <stdio.h>                      // for fprintf, stderr, printf, etc
#include <string.h>                     // for memcpy, strcpy, memset, etc
#include "codegen/FrontEnd.hpp"         // for TR_VerboseLog, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"  // for TR::Options, etc
#include "env/IO.hpp"                   // for POINTER_PRINTF_FORMAT
#include "env/defines.h"                // for HOST_OS, OMR_LINUX
#include "env/CompilerEnv.hpp"          // for TR::Compiler
#include "env/jittypes.h"               // for FLUSH_MEMORY
#include "il/DataTypes.hpp"             // for TR_YesNoMaybe::TR_yes, etc
#include "infra/Assert.hpp"             // for TR_ASSERT
#include "infra/CriticalSection.hpp"    // for CriticalSection
#include "infra/Monitor.hpp"            // for Monitor
#include "runtime/CodeCache.hpp"        // for CodeCache
#include "runtime/CodeCacheManager.hpp" // for CodeCacheManager
#include "runtime/CodeCacheMemorySegment.hpp" // for CodeCacheMemorySegment
#include "runtime/CodeCacheConfig.hpp"  // for CodeCacheConfig, etc
#include "runtime/Runtime.hpp"

#ifdef LINUX
#include <elf.h>                        // for EV_CURRENT, SHT_STRTAB, etc
#include <unistd.h>                     // for getpid, pid_t
#endif

TR::CodeCacheManager *
OMR::CodeCacheManager::self()
   {
   return static_cast<TR::CodeCacheManager *>(this);
   }


OMR::CodeCacheManager::CacheListCriticalSection::CacheListCriticalSection(TR::CodeCacheManager *mgr)
   : CriticalSection(mgr->cacheListMutex())
   {
   }


OMR::CodeCacheManager::RepositoryMonitorCriticalSection::RepositoryMonitorCriticalSection(TR::CodeCacheManager *mgr)
   : CriticalSection(mgr->_codeCacheRepositoryMonitor)
   {
   }


TR::CodeCache *
OMR::CodeCacheManager::initialize(
      bool allocateMonolithicCodeCache,
      uint32_t numberOfCodeCachesToCreateAtStartup)
   {
   TR_ASSERT(!self()->initialized(), "cannot initialize code cache manager more than once");

#if (HOST_OS == OMR_LINUX)
   _elfHeader = NULL;
#endif // HOST_OS == OMR_LINUX

   TR::CodeCacheConfig &config = self()->codeCacheConfig();

   if (allocateMonolithicCodeCache)
      {
      size_t size = config.codeCacheTotalKB() * 1024;
      if (self()->allocateCodeCacheRepository(size))
         {
         // Success. In this case we want to pre-allocate just one code cache
         if (config.canChangeNumCodeCaches())
            {
            // code caches will all be nearby each other in the repository so no need to allocate more than 1 up front
            numberOfCodeCachesToCreateAtStartup = 1;
            }
         }
      else
         {
         // when we use 1GB pages we can fail allocation for many reasons
         // the next attempt should be done using 4K pages
         if (config.largeCodePageSize() >= 0x40000000)
            config._largeCodePageSize = 0x1000;

         // print a messages and fall back on the old mechanism
         if (config.verboseCodeCache())
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "failed to allocate codeCacheRepository of size %u KB", (uint32_t)(config.codeCacheTotalKB()));
            }
         }
      }

   mcc_printf("mcc_initialize: initializing %d code cache(s)\n", numberOfCodeCachesToCreateAtStartup);
   mcc_printf("mcc_initialize: code cache size = %u kb\n",  config.codeCacheKB());
   mcc_printf("mcc_initialize: code cache pad size = %u kb\n",  config.codeCachePadKB());
   mcc_printf("mcc_initialize: code cache total size = %u kb\n",  config.codeCacheTotalKB());

   // Initialize the list of code caches
   //
   _codeCacheList._head = NULL;
   _codeCacheList._mutex = TR::Monitor::create("JIT-CodeCacheListMutex");
   if (_codeCacheList._mutex == NULL)
      return NULL;

#if defined(TR_HOST_POWER)
   #define REACHEABLE_RANGE_KB (32*1024)
#else
   #define REACHEABLE_RANGE_KB (2048*1024)
#endif

   config._needsMethodTrampolines =
      !(config.trampolineCodeSize() == 0
        || config.maxNumberOfCodeCaches() == 1
#if !defined(TR_HOST_POWER)
        || (!TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) &&
            !TR::Options::getCmdLineOptions()->getOption(TR_EnableMethodTrampolineReservation) &&
            _codeCacheRepositorySegment &&
            config.codeCacheTotalKB() <= REACHEABLE_RANGE_KB)
#endif
        );
   
   _lowCodeCacheSpaceThresholdReached = false;

   _initialized = true;

   int32_t cachesCreatedOnInit = std::min<int32_t>(config.maxNumberOfCodeCaches(), numberOfCodeCachesToCreateAtStartup);

   TR::CodeCache *codeCache = NULL;
   for (int32_t i = 0; i < cachesCreatedOnInit; i++)
      {
      // These code caches must not be reserved. A value of -2 for reservingCompThreadID
      // instructs allocate() routine to not reserve the code caches
      //
      codeCache = TR::CodeCache::allocate(self(), config.codeCacheKB() << 10, -2); // MCT
      }

   _curNumberOfCodeCaches = cachesCreatedOnInit;

   return codeCache;
   }


// Re-do the helper trampoline initialization for existing codeCaches
void
OMR::CodeCacheManager::lateInitialization()
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.trampolineCodeSize())
      return;

   for (TR::CodeCache * codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      config.mccCallbacks().createHelperTrampolines((uint8_t *)codeCache->getHelperBase(), config.numRuntimeHelpers());
      }
   }



void
OMR::CodeCacheManager::destroy()
   {
#if (HOST_OS == OMR_LINUX)
   // if code cache should be written out as shared object, do that now before destroying anything
   if (_elfHeader)
      {
      self()->initializeELFTrailer();

      pid_t jvmPid = getpid();
      static const int maxElfFilenameSize = 15 + sizeof(jvmPid)* 3; // "/tmp/perf-%ld.jit"
      char elfFilename[maxElfFilenameSize] = { 0 };

      int numCharsWritten = snprintf(elfFilename, maxElfFilenameSize, "/tmp/perf-%d.jit", jvmPid);
      if (numCharsWritten > 0 && numCharsWritten < maxElfFilenameSize)
         {
         FILE *elfFile = fopen(elfFilename, "wb");
         fwrite(_elfHeader, sizeof(uint8_t), sizeof(ELFCodeCacheHeader), elfFile);

         TR::CodeCacheMemorySegment *repositorySegment = _codeCacheRepositorySegment;
         fwrite(repositorySegment->segmentBase(), sizeof(uint8_t), repositorySegment->segmentTop() - repositorySegment->segmentBase(), elfFile);

         fwrite(_elfTrailer, sizeof(uint8_t), _elfTrailerSize, elfFile);

         fclose(elfFile);
         }
      }
#endif // HOST_OS == OMR_LINUX

   TR::CodeCache *codeCache = self()->getFirstCodeCache();
   while (codeCache != NULL)
      {
      TR::CodeCache *nextCache = codeCache->next();
      codeCache->destroy(self());
      self()->freeMemory(codeCache);
      codeCache = nextCache;
      }
   _initialized = false;
   }


TR::CodeCache *
OMR::CodeCacheManager::allocateCodeCacheObject(TR::CodeCacheMemorySegment *codeCacheSegment,
                                             size_t codeCacheSizeAllocated,
                                             CodeCacheHashEntrySlab *hashEntrySlab)
   {
   TR::CodeCache *codeCache = static_cast<TR::CodeCache *>(self()->getMemory(sizeof(TR::CodeCache)));
   if (codeCache)
      {
      new (codeCache) TR::CodeCache();
      if (!codeCache->initialize(self(), codeCacheSegment, codeCacheSizeAllocated, hashEntrySlab))
         {
         self()->freeMemory(codeCache);
         codeCache = NULL;
         }
      }
   return codeCache;
   }


// Start managing the given code cache
//
void
OMR::CodeCacheManager::addCodeCache(TR::CodeCache *codeCache)
   {
   CacheListCriticalSection updateCacheList(self());

   /* add it to the linked list */
   codeCache->linkTo(_codeCacheList._head);
   FLUSH_MEMORY(true);  // Insure codeCache contents are globally visible before adding it to the list!
   _codeCacheList._head = codeCache;
   _curNumberOfCodeCaches++;
   }


void
OMR::CodeCacheManager::unreserveCodeCache(TR::CodeCache *codeCache)
   {
   if (!codeCache)
      return;

   CacheListCriticalSection scanCacheList(self());
   codeCache->unreserve();
}

// The size estimate is just that a guess. We should reserve a code cache that has at least
// that much space available. If sizeEstimate is 0, then there is no estimate.
// compThreadID is the ID of the compilation thread requesting the reservation
// A compThreadID of -1 means unknown. This ID will be written into the code cache
// The ID of the thread that last reserved the cache will remain written after the
// reservation is over. This will allow us to implement some affinity.
TR::CodeCache *
OMR::CodeCacheManager::reserveCodeCache(bool compilationCodeAllocationsMustBeContiguous,
                                      size_t sizeEstimate,
                                      int32_t compThreadID,
                                      int32_t *numReserved)
   {
   int32_t numCachesAlreadyReserved = 0;
   TR::CodeCache *codeCache = NULL;

   // Scan the list of code caches; must acquire a mutex
   //
      {
      CacheListCriticalSection scanCacheList(self());
      for (codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
         {
         if (!codeCache->isReserved()) // we cannot touch the reserved ones
            {
            TR_YesNoMaybe almostFull = codeCache->almostFull();
            if (almostFull == TR_no || (almostFull == TR_maybe && !compilationCodeAllocationsMustBeContiguous))
               {
               // Is the free space big enough?
               if (sizeEstimate == 0 || // If size estimate is not given we'll blindly pick anything
                   codeCache->getFreeContiguousSpace() >= sizeEstimate ||
                   codeCache->getSizeOfLargestFreeWarmBlock() >= sizeEstimate   // we don't know yet the warm/cold requirements
                   )                                                             // so check only for warm part
                  {
                  codeCache->reserve(compThreadID);
                  break;
                  }
               }
            }
         else // code cache is reserved
            {
            numCachesAlreadyReserved++;
            }
         } // end for
      }

   *numReserved = numCachesAlreadyReserved;

#ifdef CODECACHE_STATS
   // Stats: count how many caches were not selected because they were already reserved
   statNumReservedCaches.update(numCachesAlreadyReserved);
#endif

   if (codeCache)
      {
      TR_ASSERT(codeCache->isReserved(), "cache must be reserved\n");
      return codeCache;
      }

   // No existing code cache is available; try to allocate a new one
   if (self()->canAddNewCodeCache()) {
      TR::CodeCacheConfig &config = self()->codeCacheConfig();
      codeCache = TR::CodeCache::allocate(self(), config.codeCacheKB() << 10, compThreadID);
      }
   else
      {
      if (numCachesAlreadyReserved > 0)
         self()->setHasFailedCodeCacheAllocation();
      }

   if (codeCache)
      {
      TR_ASSERT(codeCache->isReserved(), "cache must be reserved\n");
      }
   else
      {
      if (numCachesAlreadyReserved == 0)
         {
         // Cannot reserve a cache and there are no other reserved caches, so
         // no chance to find space for this request. Must declare code cache full.
         self()->setCodeCacheFull();
         }
      }

   return codeCache;
   }

//------------------------------ getNewCodeCache -----------------------------
// Searches for a code cache that hasn't been used before. If not found it
// tries to allocate a new code cache. If that fails too, it returns NULL
// Parameter: the ID of the compilation thread that executes the routine
// Side effects: the new cache is reserved
//----------------------------------------------------------------------------
TR::CodeCache *
OMR::CodeCacheManager::getNewCodeCache(int32_t reservingCompThreadID)
   {
   TR::CodeCache *codeCache = NULL;
   if (self()->canAddNewCodeCache())
      {
      TR::CodeCacheConfig &config = self()->codeCacheConfig();
      codeCache = TR::CodeCache::allocate(self(), config.codeCacheKB() << 10, reservingCompThreadID);
      }

   return codeCache;
   }


// Allocate code memory with separate warm and cold sections.
// Returns the address of the warm section; the "coldCode" slot is filled in with
// the address of the cold section. The sizes may increase due to alignment and headers
//
uint8_t *
OMR::CodeCacheManager::allocateCodeMemory(size_t warmCodeSize,
                                        size_t coldCodeSize,
                                        TR::CodeCache **codeCache_pp,
                                        uint8_t ** coldCode,
                                        bool needsToBeContiguous,
                                        bool isMethodHeaderNeeded)
   {
   uint8_t *methodBlockAddress;
#ifdef CODECACHE_STATS
   statWarmAllocationSizes.update(warmCodeSize);
#endif

   TR_ASSERT((*codeCache_pp)->isReserved(), "Code cache must be reserved"); // MCT

   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   methodBlockAddress = self()->allocateCodeMemoryWithRetries(warmCodeSize,
                                                              coldCodeSize,
                                                              codeCache_pp,
                                                              (int32_t) config.codeCacheMethodBodyAllocRetries(),
                                                              coldCode,
                                                              needsToBeContiguous,
                                                              isMethodHeaderNeeded);
   _lastCache = *codeCache_pp;

   if (config.doSanityChecks() && (*codeCache_pp))
      (*codeCache_pp)->checkForErrors();

   return methodBlockAddress;
   }

// Code executed by compilation thread and by application threads when trying to
// induce a profiling compilation. Acquires codeCacheList.mutex
bool
OMR::CodeCacheManager::almostOutOfCodeCache()
   {
   if (self()->lowCodeCacheSpaceThresholdReached())
      return true;

   TR::CodeCacheConfig &config = self()->codeCacheConfig();

   // If we can allocate another code cache we are fine
   // Put common case first
   if (self()->canAddNewCodeCache())
      return false;
   else
      {
      // Check the space in the the most current code cache
      bool foundSpace = false;

         {
         CacheListCriticalSection scanCacheList(self());
         for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
            {
            if (codeCache->getFreeContiguousSpace() >= config.lowCodeCacheThreshold())
               {
               foundSpace = true;
               break;
               }
            }
         }

      if (!foundSpace)
         {
         _lowCodeCacheSpaceThresholdReached = true;   // Flag can be checked under debugger
         if (config.verbosePerformance())
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"Reached code cache space threshold. Disabling JIT profiling.");
            }

         return true;
         }
      }

   return false;
   }


void
OMR::CodeCacheManager::performSizeAdjustments(size_t &warmCodeSize,
                                            size_t &coldCodeSize,
                                            bool needsToBeContiguous,
                                            bool isMethodHeaderNeeded)
   {
   TR::CodeCacheConfig config = self()->codeCacheConfig();
   size_t round = config.codeCacheAlignment() - 1;

   if (needsToBeContiguous && coldCodeSize)
      {
      warmCodeSize += coldCodeSize;
      coldCodeSize=0;
      }

   // reserve space for code cache header and align to the required boundary
   if (warmCodeSize)
      {
      if (isMethodHeaderNeeded)
         warmCodeSize += sizeof(CodeCacheMethodHeader);
      warmCodeSize = (warmCodeSize + round) & ~round;
      }

   if (coldCodeSize)
      {
      if (isMethodHeaderNeeded)
         coldCodeSize += sizeof(CodeCacheMethodHeader);
      coldCodeSize = (coldCodeSize + round) & ~round;
      }
   }


void
OMR::CodeCacheManager::printMccStats()
   {
   self()->printRemainingSpaceInCodeCaches();
   self()->printOccupancyStats();
#ifdef CODECACHE_STATS
   self()->statNumReservedCaches.report(stderr);
   self()->statWarmAllocationSizes.report(stderr);
#endif // CODECACHE_STATS
   }


void
OMR::CodeCacheManager::printRemainingSpaceInCodeCaches()
   {
   CacheListCriticalSection scanCacheList(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      fprintf(stderr, "cache %p has %u bytes empty\n", codeCache, codeCache->getFreeContiguousSpace());
      if (codeCache->isReserved())
         fprintf(stderr, "Above cache is reserved by compThread %d\n", codeCache->getReservingCompThreadID());
      }
   }


void
OMR::CodeCacheManager::printOccupancyStats()
   {
   CacheListCriticalSection scanCacheList(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      codeCache->printOccupancyStats();
      }
   }

// Find a code cache containing the given address
//
TR::CodeCache *
OMR::CodeCacheManager::findCodeCacheFromPC(void *inCacheAddress)
   {
   TR::CodeCache *codeCache = self()->getFirstCodeCache();
   if (!codeCache)
      return NULL;

   /* scan all code caches to see if they encompass inCacheAddress */
   while (codeCache)
      {
      /* helperTop is heapTop */
      if ((uint8_t *) inCacheAddress >= codeCache->getCodeBase() &&
          (uint8_t *) inCacheAddress <= codeCache->getHelperTop())
         return codeCache;
      codeCache = codeCache->next();
      }

   /* nothing found */
   return NULL;
   }

// Trampoline Lookup
// Find the trampoline for the given method in the code cache containing the
// callingPC.
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCacheManager::findMethodTrampoline(TR_OpaqueMethodBlock *method, void *callingPC)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return NULL;

   TR::CodeCache *codeCache = self()->findCodeCacheFromPC(callingPC);
   if (!codeCache)
      return NULL;

   return codeCache->findTrampoline(method);
   }

// Helper trampoline Lookup
// Find the trampoline for the given helper in the code cache containing the
// callingPC.
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCacheManager::findHelperTrampoline(void *callingPC, int32_t helperIndex)
   {
   /* does the platform need trampolines at all? */
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.trampolineCodeSize())
      return NULL;

   TR::CodeCache *codeCache = self()->findCodeCacheFromPC(callingPC);
   if (!codeCache)
      return NULL;

   return codeCache->findTrampoline(helperIndex);
   }

// Synchronize temporary trampolines in all code caches
//
void
OMR::CodeCacheManager::synchronizeTrampolines()
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

      {
      CacheListCriticalSection scanCacheList(self());
      for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
         {
         /* does the platform need temporary trampolines? */
         if (codeCache->tempTrampolinesMax())
            codeCache->syncTempTrampolines();
         }
      }
   }

// Trampoline Replacement / Patching
// Replace permanent trampoline code with updated target address
//
OMR::CodeCacheTrampolineCode *
OMR::CodeCacheManager::replaceTrampoline(TR_OpaqueMethodBlock *method,
                                       void                 *callSite,
                                       void                 *oldTrampoline,
                                       void                 *oldTargetPC,
                                       void                 *newTargetPC,
                                       bool                  needSync)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return NULL;

   TR::CodeCache *codeCache = self()->findCodeCacheFromPC(callSite);
   return codeCache->replaceTrampoline(method, oldTrampoline, oldTargetPC, newTargetPC, needSync);
   }


// Is there space and are we allowed to allocate a new code cache?
//
bool
OMR::CodeCacheManager::canAddNewCodeCache()
   {
   TR::CodeCacheConfig & config = self()->codeCacheConfig();

   // is there space for another code cache?
   if (config._allowedToGrowCache && _curNumberOfCodeCaches < config._maxNumberOfCodeCaches)
       return true;

   if (config.verboseCodeCache())
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache maximum allocated");
      }

   return false;
   }


#if DEBUG
void
OMR::CodeCacheManager::dumpCodeCaches()
   {
   CacheListCriticalSection scanCacheList(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      codeCache->dumpCodeCache();
   }
#endif


// May add block defined by metaData to freeBlockList.
// Caller should expect that block may sometimes not be added.
void
OMR::CodeCacheManager::addFreeBlock(void *metaData, uint8_t *startPC)
   {
   TR::CodeCache *owningCodeCache = self()->findCodeCacheFromPC(startPC);
   owningCodeCache->addFreeBlock(metaData);
   }


#ifdef CODECACHE_STATS
#include "infra/Statistics.hpp"
TR_StatsHisto<3> statNumReservedCaches("Caches already reserved", 1, 4);
TR_StatsHisto<20> statWarmAllocationSizes("WarmAllocationSizes", 8, 168);
#endif


// Allocate code memory with separate warm and cold sections.
// Returns the address of the warm section; the "coldCode" slot is filled in with
// the address of the cold section.
// This is a private method used by the above allocation methods that allows the
// allocation to be retried if the code cache becomes full.
//
uint8_t *
OMR::CodeCacheManager::allocateCodeMemoryWithRetries(size_t warmCodeSize,
                                                   size_t coldCodeSize,
                                                   TR::CodeCache **codeCache_pp,
                                                   int32_t allocationRetries,
                                                   uint8_t ** coldCode,
                                                   bool needsToBeContiguous,
                                                   bool isMethodHeaderNeeded)
   {
   uint8_t * warmCodeAddress = NULL;
   TR::CodeCache *codeCache;
   TR::CodeCache *originalCodeCache = *codeCache_pp; // diagnostic

   /* prevent infinite recursion on allocation requests larger than possible code cache size etc */
   if (allocationRetries-- < 0)
      return NULL;

   codeCache = *codeCache_pp;
   TR_ASSERT(codeCache->isReserved(), "Code cache must be reserved retries=%d", allocationRetries);
   int32_t compThreadID = codeCache->getReservingCompThreadID(); // read the ID of the comp thread

   // Try to allocate into the suggested code cache
   warmCodeAddress = codeCache->allocateCodeMemory(warmCodeSize,
                                                   coldCodeSize,
                                                   coldCode,
                                                   needsToBeContiguous,
                                                   isMethodHeaderNeeded);

   if (warmCodeAddress)
      return warmCodeAddress;

   if (codeCache->almostFull() == TR_no)
      codeCache->setAlmostFull(TR_maybe);

   // Let's scan the list of caches to find one that has enough space
   // However, for the last trial allocate a cache
   int32_t numCachesVisited = 0;
   int32_t numCachesAlreadyReserved = 0;
   if (allocationRetries > 0)
      {
      // scope for cache list critical section
         {
         CacheListCriticalSection scanCacheList(self());

         for (codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
            {
            numCachesVisited++;
            // Our current cache is reserved, so we cannot find it again
            if (!codeCache->isReserved())
               {
               if (codeCache->almostFull() != TR_yes)
                  {
                  // How about the size
                  size_t warmSize = warmCodeSize;
                  size_t coldSize = coldCodeSize;
                  // note side effect on warmSize and coldSize from performSizeAdjustments
                  self()->performSizeAdjustments(warmSize,
                                                 coldSize,
                                                 needsToBeContiguous,
                                                 isMethodHeaderNeeded);
                   // TODO: should we check the free blocks first?
                  if (codeCache->getFreeContiguousSpace() > warmSize + coldSize)
                     {
                     codeCache->reserve(compThreadID);
                     break;
                     }
                  }
               }
            else // This cache is reserved; would it be OK though?
               {
               numCachesAlreadyReserved++;
               }
            }
         }

      if (codeCache) // found an appropriate code cache
         {
         TR_ASSERT((*codeCache_pp)->isReserved(), "Original code cache must have been reserved"); // MCT
         TR_ASSERT(codeCache->isReserved(), "Selected code cache must have been reserved"); // MCT
         (*codeCache_pp)->unreserve();
#ifdef MCT_DEBUG
         fprintf(stderr, "cache %p reset reservation in allocateCodeMemory after searching\n", *codeCache_pp);
#endif
         *codeCache_pp = codeCache; // code cache has been switched // MCT
         TR::CodeCacheConfig & config = self()->codeCacheConfig();
         if (config.verboseCodeCache())
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "Switching TR::CodeCache to %p @ " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT,
                           codeCache, codeCache->getCodeBase(), codeCache->getCodeTop());
            }

         if (needsToBeContiguous) // We should abort the compilation if we change the cache during a compilation that needs contiguous allocations
            return NULL;

         /* try allocating in the new cache */
         warmCodeAddress = self()->allocateCodeMemoryWithRetries(warmCodeSize,
                                                                 coldCodeSize,
                                                                 codeCache_pp,
                                                                 allocationRetries,
                                                                 coldCode,
                                                                 needsToBeContiguous,
                                                                 isMethodHeaderNeeded);

         return warmCodeAddress;
         }
      }

   // We didn't find an appropriate code cache and must allocate a new code cache
   // Let's see if there actually is a code cache that is free
   //fprintf(stderr, "Will have to allocate a code cache; retries=%d cachesVisited=%d cachedRserved=%d\n",
   //        allocationRetries, numCachesVisited, numCachesAlreadyReserved);
#ifdef MCT_DEBUG
   fprintf(stderr, "Cache %p has %u bytes free. We requested %u bytes\n",
           *codeCache_pp, (*codeCache_pp)->getFreeContiguousSpace(), warmCodeSize+coldCodeSize);
#endif

   codeCache = *codeCache_pp; // MCT
   if (!self()->canAddNewCodeCache())
      {
      if (numCachesAlreadyReserved > 1) // 1 is for my own reserved cache
         {
         // Here we don't declare code cache full, because we may still have the chance to
         // find some space in one of the other caches that are currently reserved
         self()->setHasFailedCodeCacheAllocation();
         }
      else // If one allocation request cannot be satisfied by any code cache
         { // in the system, then we must declare that code caches have become full
         self()->setCodeCacheFull();
         }
      return NULL;
      }

   /* Calculate the code cache segment size */
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   size_t allocateSize = 2*(warmCodeSize +
                            coldCodeSize +
                            config.numRuntimeHelpers() * config.trampolineCodeSize() +
                            config.ccPreLoadedCodeSize());

   // TODO: move these jit config fields into private jit config?
   size_t segmentSize = config.codeCacheKB() << 10;
   if (segmentSize < allocateSize)
      {
      size_t round = config.codeCacheAlignment() - 1;
      segmentSize = (allocateSize + round) & (~round);
      }

   /* Create a new code cache structure and initialize it */
   codeCache = TR::CodeCache::allocate(self(), segmentSize, compThreadID); // the allocated cache is in reserved state // MCT
   if (!codeCache) {
      self()->setCodeCacheFull();
      return NULL;
   }
    // Unreserve the original cache
   TR_ASSERT((*codeCache_pp)->isReserved(), "Code cache must be reserved. Original code cache=%p pp=%p\n", originalCodeCache, *codeCache_pp); // MCT

   (*codeCache_pp)->unreserve();

#ifdef MCT_DEBUG
   fprintf(stderr, "cache %p reset reservation in allocateCodeMemory after new code cache allocation. New cache=%p needsToBeContiguous=%d allocationRetries=%d\n",
           *codeCache_pp, codeCache, needsToBeContiguous, allocationRetries);
#endif

   TR_ASSERT(codeCache->isReserved(), "Allocated code cache must have been reserved"); // MCT
   *codeCache_pp = codeCache;

   if (needsToBeContiguous) // We should abort the compilation if code allocations needs to be contiguous
      return NULL;

   /* try allocating in the new cache */
   warmCodeAddress = self()->allocateCodeMemoryWithRetries(warmCodeSize,
                                                           coldCodeSize,
                                                           codeCache_pp,
                                                           allocationRetries,
                                                           coldCode,
                                                           needsToBeContiguous,
                                                           isMethodHeaderNeeded);

   return warmCodeAddress;
   }


TR::CodeCacheMemorySegment *
OMR::CodeCacheManager::getNewCacheMemorySegment(size_t segmentSize,
                                              size_t & codeCacheSizeAllocated)
   {
   TR::CodeCacheMemorySegment *codeCacheSegment;

   if (_codeCacheRepositorySegment)
      {
      codeCacheSegment = self()->carveCodeCacheSpaceFromRepository(segmentSize, codeCacheSizeAllocated);
      if (!codeCacheSegment)
         {
         mcc_printf("CodeCache::allocate : code cache repository exhausted\n");
         TR::CodeCacheConfig & config = self()->codeCacheConfig();
         if (config.verboseCodeCache())
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "code cache repository exhausted");
         return NULL;
         }
      }
   else // old path
      {
      codeCacheSegment = self()->allocateCodeCacheSegment(segmentSize, codeCacheSizeAllocated, NULL);
      if (!codeCacheSegment)
         {
         mcc_printf("CodeCache::allocate : codeCacheSegment is NULL\n");
         TR::CodeCacheConfig & config = self()->codeCacheConfig();
         if (config.verboseCodeCache())
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "cannot allocate code cache segment");
         // TODO: add trace point
         return NULL;
         }
      }

   return codeCacheSegment;
   }


TR::CodeCache *
OMR::CodeCacheManager::allocateRepositoryCodeCache()
   {
   TR::CodeCache *cache = static_cast<TR::CodeCache *>(self()->getMemory(sizeof(TR::CodeCache)));
   return cache;
   }


TR::CodeCacheMemorySegment *
OMR::CodeCacheManager::allocateCodeCacheRepository(size_t repositorySize)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();

   size_t codeCacheSizeAllocated;
   if (!(_codeCacheRepositoryMonitor = TR::Monitor::create("CodeCacheRepositoryMonitor")))
      return NULL;

   void *startAddress = self()->chooseCacheStartAddress(repositorySize);

   _codeCacheRepositorySegment = self()->allocateCodeCacheSegment(repositorySize,
                                                                  codeCacheSizeAllocated,
                                                                  startAddress);
   if (_codeCacheRepositorySegment)
      {
      _repositoryCodeCache = self()->allocateRepositoryCodeCache();
      new (_repositoryCodeCache) CodeCache();

      // The VM expects the first entry in the segment to be a pointer to
      // a TR::CodeCache structure and the first two entries in the cache
      // to be warmCodeAlloc and coldCodeAlloc.
      uint8_t * start = _codeCacheRepositorySegment->segmentAlloc();
      *((TR::CodeCache**)start) = self()->getRepositoryCodeCacheAddress();

      _codeCacheRepositorySegment->adjustAlloc(sizeof(TR::CodeCache*)); // jump over the pointer we setup

      self()->repositoryCodeCacheCreated();

      // The difference between coldCodeAlloc and warmCodeAlloc must give us the free space
      _repositoryCodeCache->setWarmCodeAlloc(0);
      _repositoryCodeCache->setColdCodeAlloc((uint8_t*)(_codeCacheRepositorySegment->segmentTop() - _codeCacheRepositorySegment->segmentAlloc()));

      if (config.verboseCodeCache())
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "allocateCodeCacheRepository: size=%u heapBase=%p heapAlloc=%p heapTop=%p",
                                                           codeCacheSizeAllocated,
                                                           _codeCacheRepositorySegment->segmentBase(),
                                                           _codeCacheRepositorySegment->segmentAlloc(),
                                                           _codeCacheRepositorySegment->segmentTop());
         }
      }

   return _codeCacheRepositorySegment;
   }


void
OMR::CodeCacheManager::repositoryCodeCacheCreated()
   {
#if (HOST_OS == OMR_LINUX)
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (config.emitElfObject())
      self()->initializeELFHeader();
#endif // HOST_OS == OMR_LINUX
   }

void
OMR::CodeCacheManager::registerCompiledMethod(const char *sig, uint8_t *startPC, uint32_t codeSize)
   {
#if (HOST_OS == OMR_LINUX)
   CodeCacheSymbol *newSymbol = static_cast<CodeCacheSymbol *> (self()->getMemory(sizeof(CodeCacheSymbol)));

   uint32_t nameLength = strlen(sig) + 1;
   char *name = static_cast<char *> (self()->getMemory(nameLength * sizeof(char)));
   memcpy(name, sig, nameLength);
   newSymbol->_name = name;
   newSymbol->_nameLength = nameLength;
   newSymbol->_start = startPC;
   newSymbol->_size = codeSize;
   newSymbol->_next = _symbols;
   _symbols = newSymbol;
   _numELFSymbols++;
   _totalELFSymbolNamesLength += nameLength;
#endif // HOST_OS == OMR_LINUX
   }

void *
OMR::CodeCacheManager::chooseCacheStartAddress(size_t repositorySize)
   {
   // default: NULL means don't try to pick a starting address
   return NULL;
   }


void
OMR::CodeCacheManager::increaseFreeSpaceInCodeCacheRepository(size_t size)
   {
   // VM DUMP agents set the segment->heapAlloc to (segment->heapTop - (coldCodeAlloc-warmCodeAlloc))
   // For the repository segment we will keep warmCodeAlloc at 0 and set coldCodeAlloc to
   // the value of free space in the entire repository

   // Either use monitors or atomic operations for the following because
   // multiple compilation threads can adjust this value
   if (_codeCacheRepositorySegment) // check whether we use the repository at all
      {
      RepositoryMonitorCriticalSection updateRepository(self());
      _repositoryCodeCache->setColdCodeAlloc(_repositoryCodeCache->getColdCodeAlloc() + size);
      }
   }


void
OMR::CodeCacheManager::decreaseFreeSpaceInCodeCacheRepository(size_t size)
   {
   // VM DUMP agents set the segment->heapAlloc to (segment->heapTop - (coldCodeAlloc-warmCodeAlloc))
   // For the repository segment we will keep warmCodeAlloc at 0 and set coldCodeAlloc to
   // the value of free space in the entire repository

   // Either use monitors or atomic operations for the following because
   // multiple compilation threads can adjust this value
   if (_codeCacheRepositorySegment) // check whether we use the repository at all
      {
      RepositoryMonitorCriticalSection updateRepository(self());
      _repositoryCodeCache->setColdCodeAlloc(_repositoryCodeCache->getColdCodeAlloc() - size);
      }
   }


TR::CodeCacheMemorySegment *
OMR::CodeCacheManager::carveCodeCacheSpaceFromRepository(size_t segmentSize,
                                                        size_t &codeCacheSizeToAllocate)
   {
   uint8_t* start = NULL;
   uint8_t* end = NULL;
   size_t freeSpace = 0;

   TR::CodeCacheMemorySegment *repositorySegment = _codeCacheRepositorySegment;

   // If codeCachePadKB is defined, get the maximum between the requested size and codeCachePadKB.
   // If not defined, codeCachePadKB is 0, so getting the maximum will give segmentSize.

   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   codeCacheSizeToAllocate = std::max(segmentSize, (config.codeCachePadKB() << 10));

   // scope for repository monitor critical section
      {
      RepositoryMonitorCriticalSection updateRepository(self());

      // Test for a special case: the repository segment has a pointer at the very beginning
      // To keep caches nicely aligned pretend we want sizeof(TR::CodeCache *) less
      if (repositorySegment->segmentAlloc() - repositorySegment->segmentBase() == sizeof(TR::CodeCache *))
         codeCacheSizeToAllocate -= sizeof(TR::CodeCache*);

      freeSpace = repositorySegment->segmentTop() - repositorySegment->segmentAlloc();
      if (freeSpace >= codeCacheSizeToAllocate)
         {
         // buy the space
         start = repositorySegment->segmentAlloc();
         repositorySegment->adjustAlloc(codeCacheSizeToAllocate);
         end = repositorySegment->segmentAlloc();
         }
      }

   if (config.verboseCodeCache())
      {
      if (start)
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "carved size=%u range: " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT,
                  codeCacheSizeToAllocate, start, end);
      else
         TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "failed to carve size=%lu. Free space = %u",
                  codeCacheSizeToAllocate, freeSpace);
      }

   if (start)
      {
      TR::CodeCacheMemorySegment *memorySegment = self()->setupMemorySegmentFromRepository(start, end, codeCacheSizeToAllocate);
      return memorySegment;
      }

   return NULL;
   }


TR::CodeCacheMemorySegment *
OMR::CodeCacheManager::setupMemorySegmentFromRepository(uint8_t * start,
                                                      uint8_t * end,
                                                      size_t  & codeCacheSizeToAllocate)
   {
   TR::CodeCacheMemorySegment *memorySegment = static_cast<TR::CodeCacheMemorySegment *> (self()->getMemory(sizeof(TR::CodeCacheMemorySegment)));
   new (static_cast<TR::CodeCacheMemorySegment*>(memorySegment)) TR::CodeCacheMemorySegment(start, end);
   return memorySegment;
   }


void
OMR::CodeCacheManager::freeMemorySegment(TR::CodeCacheMemorySegment *segment)
   {
   segment->free(self());
   }

//--------------------------- undoCarvingFromRepository ----------------------------
void
OMR::CodeCacheManager::undoCarvingFromRepository(TR::CodeCacheMemorySegment *segment)
   {
   uint8_t *start = segment->segmentBase();
   size_t allocatedCodeCacheSize = segment->segmentTop() - segment->segmentBase();

   TR::CodeCacheMemorySegment *repositorySegment = _codeCacheRepositorySegment;

   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (config.verboseCodeCache())
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "undoCarving start=%p size=%lu", start, allocatedCodeCacheSize);
      }

   // scope for repository monitor critical section
      {
      RepositoryMonitorCriticalSection updateRepository(self());

      size_t allocatedSpace = repositorySegment->segmentAlloc() - repositorySegment->segmentBase();
      TR_ASSERT(allocatedCodeCacheSize <= allocatedSpace,
                "Error undoing the carving allocatedSpace=%lu < allocatedCodeCacheSize=%lu\n", allocatedSpace, allocatedCodeCacheSize);

      // Make sure nobody has allocated anything on top of us
      if (repositorySegment->segmentAlloc() == start + allocatedCodeCacheSize)
         {
         repositorySegment->adjustAlloc(- (int64_t)allocatedCodeCacheSize);
         }
      }
   }


void
OMR::CodeCacheManager::reservationInterfaceCache(void *callSite, TR_OpaqueMethodBlock *method)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

   TR::CodeCache *codeCache = self()->findCodeCacheFromPC(callSite);
   if (!codeCache)
      return;

   codeCache->findOrAddResolvedMethod(method);
   }




#if (HOST_OS == OMR_LINUX)

OMR::CodeCacheSymbol * OMR::CodeCacheManager::_symbols = NULL;
uint32_t OMR::CodeCacheManager::_numELFSymbols = 0; // does not include UNDEF symbol: embedded in ELfCodeCacheTrailer
uint32_t OMR::CodeCacheManager::_totalELFSymbolNamesLength = 1; // pre-count 0 for the UNDEF symbol name

void
OMR::CodeCacheManager::initializeELFHeader()
   {
   //fprintf(stderr,"segmentAlloc: %p, base %p, diff %d\n", _codeCacheRepositorySegment->segmentAlloc(), _codeCacheRepositorySegment->segmentBase(), _codeCacheRepositorySegment->segmentAlloc() - _codeCacheRepositorySegment->segmentBase());
   ELFCodeCacheHeader *hdr = static_cast<ELFCodeCacheHeader *>(self()->getMemory(sizeof(ELFCodeCacheHeader)));

   // main elf header
   hdr->hdr.e_ident[EI_MAG0] = ELFMAG0;
   hdr->hdr.e_ident[EI_MAG1] = ELFMAG1;
   hdr->hdr.e_ident[EI_MAG2] = ELFMAG2;
   hdr->hdr.e_ident[EI_MAG3] = ELFMAG3;
   hdr->hdr.e_ident[EI_CLASS] = ELFClass;
   hdr->hdr.e_ident[EI_VERSION] = EV_CURRENT;
   for (auto b = EI_PAD;b < EI_NIDENT;b++)
      hdr->hdr.e_ident[b] = 0;

   self()->initializeELFHeaderForPlatform(hdr);

   hdr->hdr.e_type = ET_EXEC;
   hdr->hdr.e_version = EV_CURRENT;

   uint32_t cacheSize = _codeCacheRepositorySegment->segmentTop() - _codeCacheRepositorySegment->segmentBase();
   hdr->hdr.e_entry = (ELFAddress) _codeCacheRepositorySegment->segmentBase();
   hdr->hdr.e_phoff = offsetof(ELFCodeCacheHeader, phdr);
   hdr->hdr.e_shoff = sizeof(ELFCodeCacheHeader) + cacheSize;
   hdr->hdr.e_flags = 0;

   hdr->hdr.e_ehsize = sizeof(ELFHeader);

   hdr->hdr.e_phentsize = sizeof(ELFProgramHeader);
   hdr->hdr.e_phnum = 1;
   hdr->hdr.e_shentsize = sizeof(ELFSectionHeader);
   hdr->hdr.e_shnum = 5; // number of sections in trailer
   hdr->hdr.e_shstrndx = 3; // index to shared string table section in trailer

   // program header
   hdr->phdr.p_type = PT_LOAD;
   hdr->phdr.p_offset = sizeof(ELFCodeCacheHeader);
   hdr->phdr.p_vaddr = (ELFAddress) _codeCacheRepositorySegment->segmentBase();
   hdr->phdr.p_paddr = (ELFAddress) _codeCacheRepositorySegment->segmentBase();
   hdr->phdr.p_filesz = cacheSize;
   hdr->phdr.p_memsz = cacheSize;
   hdr->phdr.p_flags = PF_X | PF_R; // should add PF_W if we get around to loading patchable code
   hdr->phdr.p_align = 0x1000;  //0x10000;
   _elfHeader = hdr;
   }


void
OMR::CodeCacheManager::initializeELFTrailer()
   {
   _elfTrailerSize = sizeof(ELFCodeCacheTrailer) +
                     _numELFSymbols * sizeof(ELFSymbol) + // NOTE: ELFCodeCacheTrailer includes 1 ELFSymbol: UNDEF
                     _totalELFSymbolNamesLength;
   ELFCodeCacheTrailer *trlr = static_cast<ELFCodeCacheTrailer *>(self()->getMemory(_elfTrailerSize));

   uint32_t trailerStartOffset = sizeof(ELFCodeCacheHeader) + _elfHeader->phdr.p_filesz;
   uint32_t symbolsStartOffset = trailerStartOffset + offsetof(ELFCodeCacheTrailer, symbols);
   uint32_t symbolNamesStartOffset = symbolsStartOffset + (_numELFSymbols+1) * sizeof(ELFSymbol);

   trlr->zeroSection.sh_name = 0;
   trlr->zeroSection.sh_type = 0;
   trlr->zeroSection.sh_flags = 0;
   trlr->zeroSection.sh_addr = 0;
   trlr->zeroSection.sh_offset = 0;
   trlr->zeroSection.sh_size = 0;
   trlr->zeroSection.sh_link = 0;
   trlr->zeroSection.sh_info = 0;
   trlr->zeroSection.sh_addralign = 0;
   trlr->zeroSection.sh_entsize = 0;

   trlr->textSection.sh_name = trlr->textSectionName - trlr->zeroSectionName;
   trlr->textSection.sh_type = SHT_PROGBITS;
   trlr->textSection.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
   trlr->textSection.sh_addr = (ELFAddress) _codeCacheRepositorySegment->segmentBase();
   trlr->textSection.sh_offset = sizeof(ELFCodeCacheHeader);
   trlr->textSection.sh_size = _codeCacheRepositorySegment->segmentTop() - _codeCacheRepositorySegment->segmentBase();
   trlr->textSection.sh_link = 0;
   trlr->textSection.sh_info = 0;
   trlr->textSection.sh_addralign = 32; // code cache alignment?
   trlr->textSection.sh_entsize = 0;

   trlr->dynsymSection.sh_name = trlr->dynsymSectionName - trlr->zeroSectionName;
   trlr->dynsymSection.sh_type = SHT_SYMTAB; // SHT_DYNSYM
   trlr->dynsymSection.sh_flags = 0; //SHF_ALLOC;
   trlr->dynsymSection.sh_addr = 0; //(ELFAddress) &((uint8_t *)_elfHeader + symbolStartOffset); // fake address because not continuous
   trlr->dynsymSection.sh_offset = symbolsStartOffset;
   trlr->dynsymSection.sh_size = (_numELFSymbols + 1)*sizeof(ELFSymbol);
   trlr->dynsymSection.sh_link = 4; // dynamic string table index
   trlr->dynsymSection.sh_info = 1; // index of first non-local symbol: for now all symbols are global
   trlr->dynsymSection.sh_addralign = 8;
   trlr->dynsymSection.sh_entsize = sizeof(ELFSymbol);

   trlr->shstrtabSection.sh_name = trlr->shstrtabSectionName - trlr->zeroSectionName;
   trlr->shstrtabSection.sh_type = SHT_STRTAB;
   trlr->shstrtabSection.sh_flags = 0;
   trlr->shstrtabSection.sh_addr = 0;
   trlr->shstrtabSection.sh_offset = trailerStartOffset + offsetof(ELFCodeCacheTrailer, zeroSectionName);
   trlr->shstrtabSection.sh_size = sizeof(trlr->zeroSectionName) +
                                   sizeof(trlr->shstrtabSectionName) +
                                   sizeof(trlr->textSectionName) +
                                   sizeof(trlr->dynsymSectionName) +
                                   sizeof(trlr->dynstrSectionName);
   trlr->shstrtabSection.sh_link = 0;
   trlr->shstrtabSection.sh_info = 0;
   trlr->shstrtabSection.sh_addralign = 1;
   trlr->shstrtabSection.sh_entsize = 0;

   trlr->dynstrSection.sh_name = trlr->dynstrSectionName - trlr->zeroSectionName;
   trlr->dynstrSection.sh_type = SHT_STRTAB;
   trlr->dynstrSection.sh_flags = 0;
   trlr->dynstrSection.sh_addr = 0;
   trlr->dynstrSection.sh_offset = symbolNamesStartOffset;
   trlr->dynstrSection.sh_size = _totalELFSymbolNamesLength;
   trlr->dynstrSection.sh_link = 0;
   trlr->dynstrSection.sh_info = 0;
   trlr->dynstrSection.sh_addralign = 1;
   trlr->dynstrSection.sh_entsize = 0;

   trlr->zeroSectionName[0] = 0;
   strcpy(trlr->shstrtabSectionName, ".shstrtab");
   strcpy(trlr->textSectionName, ".text");
   strcpy(trlr->dynsymSectionName, ".symtab");
   strcpy(trlr->dynstrSectionName, ".dynstr");

   // now walk list of compiled code symbols building up the symbol names and filling in array of ELFSymbol structures
   ELFSymbol *elfSymbols = trlr->symbols + 0;
   char *elfSymbolNames = (char *) (elfSymbols + (_numELFSymbols+1));

   // first symbol is UNDEF symbol: all zeros, even name is zero-terminated empty string
   elfSymbolNames[0] = 0;
   elfSymbols[0].st_name = 0;
   elfSymbols[0].st_info = ELF_ST_INFO(0,0);
   elfSymbols[0].st_other = 0;
   elfSymbols[0].st_shndx = 0;
   elfSymbols[0].st_value = 0;
   elfSymbols[0].st_size = 0;

   CodeCacheSymbol *sym = _symbols;
   ELFSymbol *elfSym = elfSymbols + 1;
   char *names = elfSymbolNames + 1;
   while (sym)
      {
      //fprintf(stderr, "Writing elf symbol %d, name(%d) = %s\n", (elfSym - elfSymbols), sym->_nameLength, sym->_name);
      memcpy(names, sym->_name, sym->_nameLength);

      elfSym->st_name = names - elfSymbolNames;
      elfSym->st_info = ELF_ST_INFO(STB_GLOBAL,STT_FUNC);
      elfSym->st_other = 0;
      elfSym->st_shndx = 1;
      elfSym->st_value = (ELFAddress) sym->_start;
      elfSym->st_size = sym->_size;

      names += sym->_nameLength;
      elfSym++;

      sym = sym->_next;
      }

   _elfTrailer = trlr;
   }

void
OMR::CodeCacheManager::initializeELFHeaderForPlatform(ELFCodeCacheHeader *hdr)
   {
   #if (HOST_ARCH == ARCH_X86)
      hdr->hdr.e_machine = EM_386;
   #elif (HOST_ARCH == ARCH_POWER)
      #if defined(TR_TARGET_64BIT)
         hdr->hdr.e_machine = EM_PPC64;
      #else
         hdr->hdr.e_machine = EM_PPC;
      #endif
   #elif (HOST_ARCH == ARCH_ZARCH)
      hdr->hdr.e_machine = EM_S390;
   #else
      TR_ASSERT(0, "unrecognized architecture: cannot initialize code cache elf header");
   #endif

   #if (HOST_OS == OMR_LINUX)
      hdr->hdr.e_ident[EI_OSABI] = ELFOSABI_LINUX;
   #elif defined(AIXPPC)
      hdr->hdr.e_ident[EI_OSABI] = ELFOSABI_AIX;
   #else
      TR_ASSERT(0, "unrecognized operating system: cannot initialize code cache elf header");
   #endif

   hdr->hdr.e_ident[EI_ABIVERSION] = 0;

   hdr->hdr.e_ident[EI_DATA] = TR::Compiler->host.cpu.isLittleEndian() ? ELFDATA2LSB : ELFDATA2MSB;
   }

#endif // HOST_OS==OMR_LINUX
