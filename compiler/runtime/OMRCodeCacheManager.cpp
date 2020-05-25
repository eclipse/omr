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

#include "runtime/CodeCacheManager.hpp"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/defines.h"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "omrformatconsts.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/Runtime.hpp"

#if (HOST_OS == OMR_LINUX)
#include <elf.h>
#include <unistd.h>
#include "codegen/ELFGenerator.hpp"

TR::CodeCacheSymbolContainer * OMR::CodeCacheManager::_symbolContainer = NULL;

#endif //HOST_OS == OMR_LINUX

OMR::CodeCacheManager::CodeCacheManager(TR::RawAllocator rawAllocator) :
   _rawAllocator(rawAllocator),
   _initialized(false),
   _codeCacheFull(false),
   _currTotalUsedInBytes(0),
   _maxUsedInBytes(0)
   {
   }

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

OMR::CodeCacheManager::UsageMonitorCriticalSection::UsageMonitorCriticalSection(TR::CodeCacheManager *mgr)
   : CriticalSection(mgr->_usageMonitor)
   {
   }

TR::CodeCache *
OMR::CodeCacheManager::initialize(
      bool allocateMonolithicCodeCache,
      uint32_t numberOfCodeCachesToCreateAtStartup)
   {
   TR_ASSERT(!self()->initialized(), "cannot initialize code cache manager more than once");

#if (HOST_OS == OMR_LINUX)
   _elfRelocatableGenerator = NULL;
   _elfExecutableGenerator = NULL;

   if (_symbolContainer == NULL){
         TR::CodeCacheSymbolContainer * symbolContainer = static_cast<TR::CodeCacheSymbolContainer *>(self()->getMemory(sizeof(TR::CodeCacheSymbolContainer)));
         symbolContainer->_head = NULL;
         symbolContainer->_tail = NULL;
         symbolContainer->_numSymbols = 0; //does not include UNDEF symbol
         symbolContainer->_totalSymbolNameLength = 1; //precount for the UNDEF symbol
        _symbolContainer = symbolContainer;
   }
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
   mcc_printf("mcc_initialize: code cache size = %" OMR_PRIuSIZE " kb\n",  config.codeCacheKB());
   mcc_printf("mcc_initialize: code cache pad size = %" OMR_PRIuSIZE " kb\n",  config.codeCachePadKB());
   mcc_printf("mcc_initialize: code cache total size = %" OMR_PRIuSIZE " kb\n",  config.codeCacheTotalKB());

   // Initialize the list of code caches
   //
   _codeCacheList._head = NULL;
   _codeCacheList._mutex = TR::Monitor::create("JIT-CodeCacheListMutex");
   if (_codeCacheList._mutex == NULL)
      return NULL;

   if (!(_usageMonitor = TR::Monitor::create("CodeCacheUsageMonitor")))
      return NULL;

#if defined(TR_HOST_POWER)
   #define REACHEABLE_RANGE_KB (32*1024)
#elif defined(TR_HOST_ARM64)
   #define REACHEABLE_RANGE_KB (128*1024)
#else
   #define REACHEABLE_RANGE_KB (2048*1024)
#endif

   config._needsMethodTrampolines =
      !(config.trampolineCodeSize() == 0
        || config.maxNumberOfCodeCaches() == 1
#if !defined(TR_HOST_POWER)
        || (!TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) &&
            self()->usingRepository() &&
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
      codeCache = self()->allocateCodeCacheFromNewSegment(config.codeCacheKB() << 10, -2);
      }

   _curNumberOfCodeCaches = cachesCreatedOnInit;

   return codeCache;
   }


void
OMR::CodeCacheManager::destroy()
   {
#if (HOST_OS == OMR_LINUX)
   // if code cache should be written out as shared object, do that now before destroying anything

   if (_elfRelocatableGenerator)
   {
         TR_ASSERT(_elfRelocatableGenerator->emitELF((const char*)_objectFileName,
                                              _relocatableSymbolContainer->_head,
                                              _relocatableSymbolContainer->_numSymbols,
                                              _relocatableSymbolContainer->_totalSymbolNameLength,
                                              _relocations->_head,
                                              _relocations->_numRelocations),
                                          "Failed to write code cache symbols to executable ELF file.");
   }

   if (_elfExecutableGenerator)
   {
      pid_t jvmPid = getpid();
      static const int maxElfFilenameSize = 15 + sizeof(jvmPid)* 3; // "written to file: /tmp/perf-%d.jit, where d is the pid"
      char elfFilename[maxElfFilenameSize] = { 0 };

      int numCharsWritten = snprintf(elfFilename, maxElfFilenameSize, "/tmp/perf-%d.jit", jvmPid);
      if (numCharsWritten > 0 && numCharsWritten < maxElfFilenameSize)
      {
         TR_ASSERT(_elfExecutableGenerator->emitELF((const char*) &elfFilename,
                                _symbolContainer->_head,
                                _symbolContainer->_numSymbols,
                                _symbolContainer->_totalSymbolNameLength),
                              "Failed to write code cache symbols to relocatable ELF file.");
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

   if (self()->usingRepository())
      {
      self()->freeCodeCacheSegment(_codeCacheRepositorySegment);
      }

   _initialized = false;
   }

void *
OMR::CodeCacheManager::getMemory(size_t sizeInBytes)
   {
   return _rawAllocator.allocate(sizeInBytes, std::nothrow);
   }

void
OMR::CodeCacheManager::freeMemory(void *memoryToFree)
   {
   _rawAllocator.deallocate(memoryToFree);
   }

TR::CodeCache *
OMR::CodeCacheManager::allocateCodeCacheObject(TR::CodeCacheMemorySegment *codeCacheSegment,
                                               size_t codeCacheSize)
   {
   TR::CodeCache *codeCache = static_cast<TR::CodeCache *>(self()->getMemory(sizeof(TR::CodeCache)));
   if (codeCache)
      {
      new (codeCache) TR::CodeCache();
      if (!codeCache->initialize(self(), codeCacheSegment, codeCacheSize))
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

   if (codeCache)
      {
      TR_ASSERT(codeCache->isReserved(), "cache must be reserved\n");
      return codeCache;
      }

   // No existing code cache is available; try to allocate a new one
   if (self()->canAddNewCodeCache())
      {
      TR::CodeCacheConfig &config = self()->codeCacheConfig();
      codeCache = self()->allocateCodeCacheFromNewSegment(config.codeCacheKB() << 10, compThreadID);
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
      codeCache = self()->allocateCodeCacheFromNewSegment(config.codeCacheKB() << 10, reservingCompThreadID);
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


intptr_t
OMR::CodeCacheManager::findHelperTrampoline(int32_t helperIndex, void *callSite)
   {
   /* does the platform need trampolines at all? */
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.trampolineCodeSize())
      return 0;

   TR::CodeCache *codeCache = self()->findCodeCacheFromPC(callSite);
   if (!codeCache)
      return 0;

   return (intptr_t)codeCache->findTrampoline(helperIndex);
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
   codeCache = self()->allocateCodeCacheFromNewSegment(segmentSize, compThreadID);
   if (!codeCache)
      {
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
OMR::CodeCacheManager::getNewCodeCacheMemorySegment(
      size_t segmentSize,
      size_t & codeCacheSizeAllocated)
   {
   TR::CodeCacheMemorySegment *codeCacheSegment;

   if (self()->usingRepository())
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
   if (config.emitExecutableELF())
      self()->initializeExecutableELFGenerator();
   if (config.emitRelocatableELF())
      {
      self()->initializeRelocatableELFGenerator();
      }
#endif // HOST_OS == OMR_LINUX
   }

void
OMR::CodeCacheManager::registerCompiledMethod(const char *sig, uint8_t *startPC, uint32_t codeSize)
   {
#if (HOST_OS == OMR_LINUX)

   TR::CodeCacheSymbol *newSymbol = static_cast<TR::CodeCacheSymbol *> (self()->getMemory(sizeof(TR::CodeCacheSymbol)));
   uint32_t nameLength = strlen(sig) + 1;
   char *name = static_cast<char *> (self()->getMemory(nameLength * sizeof(char)));
   memcpy(name, sig, nameLength);
   newSymbol->_name = name;
   newSymbol->_nameLength = nameLength;
   newSymbol->_start = startPC;
   newSymbol->_size = codeSize;
   newSymbol->_next = NULL;
   if(_symbolContainer->_head){
      _symbolContainer->_tail->_next = newSymbol;
      _symbolContainer->_tail = newSymbol;
   } else {
      _symbolContainer->_head = newSymbol;
      _symbolContainer->_tail = newSymbol;
   }
   _symbolContainer->_numSymbols++;
   _symbolContainer->_totalSymbolNameLength += nameLength;

   if (_elfRelocatableGenerator){
      TR::CodeCacheSymbol *newRelocSymbol = static_cast<TR::CodeCacheSymbol *> (self()->getMemory(sizeof(TR::CodeCacheSymbol)));
      memcpy(newRelocSymbol, newSymbol, sizeof(TR::CodeCacheSymbol));
      newRelocSymbol->_next = NULL;
      if(_relocatableSymbolContainer->_head){
            _relocatableSymbolContainer->_tail->_next = newRelocSymbol;
            _relocatableSymbolContainer->_tail = newRelocSymbol;
      } else {
            _relocatableSymbolContainer->_head = newRelocSymbol;
            _relocatableSymbolContainer->_tail = newRelocSymbol;
      }
      _relocatableSymbolContainer->_numSymbols++;
      _relocatableSymbolContainer->_totalSymbolNameLength += nameLength;
   }
#endif // HOST_OS == OMR_LINUX
   }

void
OMR::CodeCacheManager::registerStaticRelocation(const TR::StaticRelocation &relocation)
   {
#if (HOST_OS == OMR_LINUX)
   if (_elfRelocatableGenerator)
      {
      const char * const symbolName(relocation.symbol());
      uint32_t nameLength = strlen(symbolName) + 1;
      char *name = static_cast<char *>(self()->getMemory(nameLength * sizeof(char)));
      memcpy(name, symbolName, nameLength);
      TR::CodeCacheSymbol *newRelocSymbol = static_cast<TR::CodeCacheSymbol *> (self()->getMemory(sizeof(TR::CodeCacheSymbol)));
      newRelocSymbol->_name = name;
      newRelocSymbol->_nameLength = nameLength;
      newRelocSymbol->_start = 0;
      newRelocSymbol->_size = 0;
      newRelocSymbol->_next = NULL;
      if(_relocatableSymbolContainer->_head){
            _relocatableSymbolContainer->_tail->_next = newRelocSymbol;
            _relocatableSymbolContainer->_tail = newRelocSymbol;
      } else {
            _relocatableSymbolContainer->_head = newRelocSymbol;
            _relocatableSymbolContainer->_tail = newRelocSymbol;
      }
      _relocatableSymbolContainer->_numSymbols++;
      _relocatableSymbolContainer->_totalSymbolNameLength += nameLength;

      uint32_t symbolNumber = static_cast<uint32_t>(_relocatableSymbolContainer->_numSymbols - 1); //symbol index in the linked list
      uint32_t relocationType = _resolver.resolveRelocationType(relocation);
      TR::CodeCacheRelocationInfo *newRelocation = static_cast<TR::CodeCacheRelocationInfo *> (self()->getMemory(sizeof(TR::CodeCacheRelocationInfo)));
      newRelocation->_location = relocation.location();
      newRelocation->_type = relocationType;
      newRelocation->_symbol = symbolNumber; //symbol index along the linked list
      if(_relocations->_head){
            _relocations->_tail->_next = newRelocation;
            _relocations->_tail = newRelocation;
      } else{
            _relocations->_head = newRelocation;
            _relocations->_tail = newRelocation;
      }
      _relocations->_numRelocations++;
      }
#endif
   }

void *
OMR::CodeCacheManager::chooseCacheStartAddress(size_t repositorySize)
   {
   // default: NULL means don't try to pick a starting address
   return NULL;
   }


void
OMR::CodeCacheManager::increaseCurrTotalUsedInBytes(size_t size)
   {
   self()->decreaseFreeSpaceInCodeCacheRepository(size);

      {
      UsageMonitorCriticalSection updateCodeCacheUsage(self());
      _currTotalUsedInBytes += size;
      _maxUsedInBytes = std::max(_maxUsedInBytes, _currTotalUsedInBytes);
      }
   }


void
OMR::CodeCacheManager::decreaseCurrTotalUsedInBytes(size_t size)
   {
   self()->increaseFreeSpaceInCodeCacheRepository(size);

      {
      UsageMonitorCriticalSection updateCodeCacheUsage(self());
      _currTotalUsedInBytes = (size < _currTotalUsedInBytes) ? (_currTotalUsedInBytes - size) : 0;
      }
   }


void
OMR::CodeCacheManager::increaseFreeSpaceInCodeCacheRepository(size_t size)
   {
   // VM DUMP agents set the segment->heapAlloc to (segment->heapTop - (coldCodeAlloc-warmCodeAlloc))
   // For the repository segment we will keep warmCodeAlloc at 0 and set coldCodeAlloc to
   // the value of free space in the entire repository

   // Either use monitors or atomic operations for the following because
   // multiple compilation threads can adjust this value
   if (self()->usingRepository())
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
   if (self()->usingRepository())
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


#if (HOST_OS == OMR_LINUX)

void
OMR::CodeCacheManager::initializeRelocatableELFGenerator(void)
   {
   _objectFileName = TR::Options::getCmdLineOptions()->getObjectFileName();

   TR::CodeCacheSymbolContainer * symbolContainer = static_cast<TR::CodeCacheSymbolContainer *>(self()->getMemory(sizeof(TR::CodeCacheSymbolContainer)));
   symbolContainer->_head = NULL;
   symbolContainer->_tail = NULL;
   symbolContainer->_numSymbols = 0; //does not include UNDEF symbol
   symbolContainer->_totalSymbolNameLength = 1; //precount for the UNDEF symbol
   _relocatableSymbolContainer = symbolContainer;


   TR::CodeCacheRelocationInfoContainer * relInfo = static_cast<TR::CodeCacheRelocationInfoContainer *>(self()->getMemory(sizeof(TR::CodeCacheRelocationInfoContainer)));
   relInfo->_head = NULL;
   relInfo->_tail = NULL;
   relInfo->_numRelocations = 0;
   _relocations = relInfo;

   _elfRelocatableGenerator =
      new (_rawAllocator) TR::ELFRelocatableGenerator(
         _rawAllocator,
         _codeCacheRepositorySegment->segmentBase(),
         _codeCacheRepositorySegment->segmentTop() - _codeCacheRepositorySegment->segmentBase());
   }

void
OMR::CodeCacheManager::initializeExecutableELFGenerator(void)
   {

   _elfExecutableGenerator =
      new (_rawAllocator) TR::ELFExecutableGenerator(
         _rawAllocator,
         _codeCacheRepositorySegment->segmentBase(),
         _codeCacheRepositorySegment->segmentTop() - _codeCacheRepositorySegment->segmentBase()
         );
   }
#endif // HOST_OS==OMR_LINUX


TR::CodeCache *
OMR::CodeCacheManager::allocateCodeCacheFromNewSegment(
      size_t segmentSizeInBytes,
      int32_t reservingCompilationTID)
   {
   TR::CodeCacheConfig & config = self()->codeCacheConfig();
   bool verboseCodeCache = config.verboseCodeCache();

   size_t actualCodeCacheSizeAllocated;
   TR::CodeCacheMemorySegment *codeCacheSegment = self()->getNewCodeCacheMemorySegment(segmentSizeInBytes, actualCodeCacheSizeAllocated);

   if (codeCacheSegment)
      {
      TR::CodeCache *codeCache = self()->allocateCodeCacheObject(codeCacheSegment, actualCodeCacheSizeAllocated);

      if (codeCache)
         {
         // If we wanted to reserve this code cache, then mark it as reserved now
         if (reservingCompilationTID >= -1)
            {
            codeCache->reserve(reservingCompilationTID);
            }

         // Add it to our list of code caches
         self()->addCodeCache(codeCache);

         if (verboseCodeCache)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache allocated %p @ " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT " HelperBase:" POINTER_PRINTF_FORMAT, codeCache, codeCache->getCodeBase(), codeCache->getCodeTop(), codeCache->_helperBase);
            }

         return codeCache;
         }

      // A meta CodeCache object could not be allocated.  Free the code cache segment.
      //
      if (self()->usingRepository())
         {
         // return back the portion we carved
         self()->undoCarvingFromRepository(codeCacheSegment);
         }
      else
         {
         self()->freeMemorySegment(codeCacheSegment);
         }
      }

   if (verboseCodeCache)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "CodeCache maximum allocated");
      }

   return NULL;
   }
