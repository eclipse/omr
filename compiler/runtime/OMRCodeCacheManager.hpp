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

#ifndef OMR_CODECACHEMANAGER_INCL
#define OMR_CODECACHEMANAGER_INCL

#include <stddef.h>
#include <stdint.h>
#include "il/DataTypes.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/MethodExceptionData.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/CodeCacheTypes.hpp"
#include "env/RawAllocator.hpp"
#include "codegen/StaticRelocation.hpp"
#include "codegen/ELFRelocationResolver.hpp"

class TR_FrontEnd;
class TR_OpaqueMethodBlock;
class TR_Memory;

namespace TR { class CodeCache; }
namespace TR { class CodeCacheManager; }
namespace TR { class CodeCacheMemorySegment; }
namespace TR { class CodeGenerator; }
namespace TR { class Monitor; }
namespace OMR { class CodeCacheHashEntrySlab; }
namespace OMR { class FaintCacheBlock; }
namespace OMR { typedef void CodeCacheTrampolineCode; }
namespace OMR { class CodeCacheManager; }
namespace TR { class StaticRelocation; }
namespace OMR { typedef CodeCacheManager CodeCacheManagerConnector; }

#if (HOST_OS == OMR_LINUX)

namespace TR { class ELFRelocatableGenerator; }
namespace TR { class ELFExecutableGenerator; }

namespace TR {

/**
 * Structure used to track regions of code cache that will become symbols
*/
typedef struct CodeCacheSymbol{
      const char *_name; /**< Symbol name */
      uint32_t _nameLength; /**< start of symbol name */
      uint8_t *_start; /**< Start PC */
      uint32_t _size; /**< Code size */
      struct CodeCacheSymbol *_next; /**< ptr to next CodeCacheSymbol */
} CodeCacheSymbol;

/**
 * Structure used to track the CodeCacheSymbol structs connected through a
 * linked list data structure.
*/
typedef struct CodeCacheSymbolContainer{
      CodeCacheSymbol *_head; /**< ptr to the first CodeCacheSymbol */
      CodeCacheSymbol *_tail; /**< ptr to the latest CodeCacheSymbol */
      uint32_t _numSymbols; /**< Number of symbols in the linked list */
      uint32_t _totalSymbolNameLength; /**< Sum of the symbol names in the linked list */
} CodeCacheSymbolContainer;

/**
 * Structure used for tracking relocations
*/
typedef struct CodeCacheRelocationInfo{
      uint8_t *_location; /**< the address requiring relocation */
      uint32_t _type; /**< relocation type: absolute or relative */
      uint32_t _symbol; /**< index of the symbol to be relocated in the linked list of CodeCacheSymbol, with head symbol being 0 */
      struct CodeCacheRelocationInfo *_next; /**< link to the next symbol */
} CodeCacheRelocationInfo;

typedef struct CodeCacheRelocationInfoContainer{
      CodeCacheRelocationInfo *_head; /**< First CodeCacheRelocationInfo structure */
      CodeCacheRelocationInfo *_tail; /**< Latest CodeCacheRelocationInfo structure added */
      uint32_t _numRelocations; /**< Total number of CodeCacheRelocationInfor in the linked list */
} CodeCacheRelocationInfoContainer;

} // namespace TR

#endif // HOST_OS == OMR_LINUX

namespace OMR {

class OMR_EXTENSIBLE CodeCacheManager
   {
   TR::CodeCacheManager *self();

protected:
   struct CodeCacheList
      {
      TR::CodeCache *_head;
      TR::Monitor *_mutex;
      };

public:

   CodeCacheManager(TR::RawAllocator rawAllocator);

   class CacheListCriticalSection : public CriticalSection
      {
      public:
      CacheListCriticalSection(TR::CodeCacheManager *mgr);
      };

   class RepositoryMonitorCriticalSection : public CriticalSection
      {
      public:
      RepositoryMonitorCriticalSection(TR::CodeCacheManager *mgr);
      };

   class UsageMonitorCriticalSection : public CriticalSection
      {
      public:
      UsageMonitorCriticalSection(TR::CodeCacheManager *mgr);
      };

   TR::CodeCacheConfig & codeCacheConfig() { return _config; }

   /**
    * @brief Inquires whether the code cache full flag has been set
    *
    * @return true if code cache full flag is set; false otherwise.
    */
   bool codeCacheFull() { return _codeCacheFull; }

   /**
    * @brief Sets the flag indicating a full code cache
    */
   void setCodeCacheFull() { _codeCacheFull = true; };

   TR::CodeCache *initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup);

   void destroy();

   // These two functions are for allocating Native backing memory for code cache structures
   // In future these facilities should probably be implemented via cs2 allocators
   void *getMemory(size_t sizeInBytes);
   void  freeMemory(void *memoryToFree);

   TR::CodeCache * allocateCodeCacheObject(TR::CodeCacheMemorySegment *codeCacheSegment,
                                           size_t codeCacheSize);
   void * chooseCacheStartAddress(size_t repositorySize);

   TR::CodeCache * getFirstCodeCache()            { return _codeCacheList._head; }
   int32_t         getCurrentNumberOfCodeCaches() { return _curNumberOfCodeCaches; }
   TR::Monitor *   cacheListMutex() const         { return _codeCacheList._mutex; }

   void addCodeCache(TR::CodeCache *codeCache);

   TR::CodeCacheMemorySegment *getNewCodeCacheMemorySegment(size_t segmentSize, size_t & codeCacheSizeAllocated);

   void        unreserveCodeCache(TR::CodeCache *codeCache);
   TR::CodeCache * reserveCodeCache(bool compilationCodeAllocationsMustBeContiguous,
                                    size_t sizeEstimate,
                                    int32_t compThreadID,
                                    int32_t *numReserved);
   TR::CodeCache * getNewCodeCache(int32_t reservingCompThreadID);

   uint8_t * allocateCodeMemory(size_t warmCodeSize,
                                size_t coldCodeSize,
                                TR::CodeCache **codeCache_pp,
                                uint8_t ** coldCode,
                                bool needsToBeContiguous,
                                bool isMethodHeaderNeeded=true);

   /**
    * @brief Allocate and initialize a new code cache from a new memory segment
    *
    * @param[in] segmentSizeInBytes : segment size to create the code cache from
    * @param[in] reservingCompilationTID : thread id of the requestor
    *               if (reservingCompilationTID)
    *                  <= -2 : no reservation is requested
    *                  == -1 : the application thread is requesting the allocation; new code cache will be reserved
    *                  >=  0 : a compilation thread is requesting the allocation; new code cache will be reserved
    *
    * @return if allocation is successful, the allocated TR::CodeCache object from
    *           the new segment; NULL otherwise.
    */
   TR::CodeCache * allocateCodeCacheFromNewSegment(
      size_t segmentSizeInBytes,
      int32_t reservingCompilationTID);

   TR::CodeCache * findCodeCacheFromPC(void *inCacheAddress);

   /**
    * @brief Finds a helper trampoline for the given helper reachable from the
    *        given code cache address.
    *
    * @param[in] helperIndex : the index of the helper requested
    * @param[in] callSite : the call site address
    *
    * @return The address of the helper trampoline in the same code cache as
    *         the call site address
    */
   intptr_t findHelperTrampoline(int32_t helperIndex, void *callSite);

   CodeCacheTrampolineCode * findMethodTrampoline(TR_OpaqueMethodBlock *method, void *callingPC);
   void synchronizeTrampolines();
   CodeCacheTrampolineCode * replaceTrampoline(TR_OpaqueMethodBlock *method,
                                               void *callSite,
                                               void *oldTrampoline,
                                               void *oldTargetPC,
                                               void *newTargetPC,
                                               bool needSync);

   void performSizeAdjustments(size_t &warmCodeSize,
                               size_t &coldCodeSize,
                               bool needsToBeContiguous,
                               bool isMethodHeaderNeeded);

   bool canAddNewCodeCache();

   // Code Cache Consolidation
   TR::CodeCache *allocateRepositoryCodeCache();
   TR::CodeCacheMemorySegment *allocateCodeCacheRepository(size_t repositorySize);
   TR::CodeCacheMemorySegment *carveCodeCacheSpaceFromRepository(size_t segmentSize,
                                                                 size_t &codeCacheSizeToAllocate);
   void undoCarvingFromRepository(TR::CodeCacheMemorySegment *segment);
   TR::CodeCacheMemorySegment *setupMemorySegmentFromRepository(uint8_t *start,
                                                                uint8_t *end,
                                                                size_t & codeCacheSizeToAllocate);
   void freeMemorySegment(TR::CodeCacheMemorySegment *segment);

   // sneaky accounting to provide the right external perception of how much space is used
   void increaseFreeSpaceInCodeCacheRepository(size_t size);
   void decreaseFreeSpaceInCodeCacheRepository(size_t size);

   bool usingRepository()                          { return _codeCacheRepositorySegment != NULL; }
   TR::CodeCache * getRepositoryCodeCacheAddress() { return _repositoryCodeCache; }
   TR::Monitor *   getCodeCacheRepositoryMonitor() { return _codeCacheRepositoryMonitor; }

   uint8_t * allocateCodeMemoryWithRetries(size_t warmCodeSize,
                                           size_t coldCodeSize,
                                           TR::CodeCache **codeCache_pp,
                                           int32_t allocationRetries,
                                           uint8_t ** coldCode,
                                           bool needsToBeContiguous,
                                           bool isMethodHeaderNeeded=true);
   void setHasFailedCodeCacheAllocation() { }

   bool initialized() const                 { return _initialized; }
   bool lowCodeCacheSpaceThresholdReached() { return _lowCodeCacheSpaceThresholdReached; }

   void repositoryCodeCacheCreated();
   void registerCompiledMethod(const char *sig, uint8_t *startPC, uint32_t codeSize);
   void registerStaticRelocation(const TR::StaticRelocation &relocation);

   /**
    * @brief Hint to free a given code cache segment.
    *
    * This function is intended to allow code cache segments allocated by
    * `allocateCodeCacheSegment()` to be freed. Because it may not always
    * be safe to actually free code cache memory, callers should treat
    * this function as a hint. It is up to downstream projects to decide
    * how to handle these requests. By default, if no overrides are
    * provided, no memory is ever freed.
    *
    * @param memSegment is a pointer to the TR::CodeCacheMemorySegment instance
    *        that handles the segment memory to be freed.
    */
   void freeCodeCacheSegment(TR::CodeCacheMemorySegment * memSegment) {}

   void increaseCurrTotalUsedInBytes(size_t size);
   void decreaseCurrTotalUsedInBytes(size_t size);
   size_t getCurrTotalUsedInBytes() const { return _currTotalUsedInBytes; }
   size_t getMaxUsedInBytes() const { return _maxUsedInBytes; }

protected:

   TR::RawAllocator               _rawAllocator;
   TR::CodeCacheConfig            _config;
   TR::CodeCache                 *_lastCache;                         /*!< last code cache round robined through */
   CodeCacheList                  _codeCacheList;                     /*!< list of allocated code caches */
   int32_t                        _curNumberOfCodeCaches;

   // The following 3 fields are for implementation of code cache consolidation
   TR::CodeCache                 *_repositoryCodeCache;
   TR::CodeCacheMemorySegment    *_codeCacheRepositorySegment;
   TR::Monitor                   *_codeCacheRepositoryMonitor;

   bool                           _initialized;                       /*!< flag to indicate if code cache manager has been initialized or not */
   bool                           _lowCodeCacheSpaceThresholdReached; /*!< true if close to exhausting available code cache */
   bool                           _codeCacheFull;

   TR::Monitor                   *_usageMonitor;
   size_t                         _currTotalUsedInBytes;
   size_t                         _maxUsedInBytes;
#if (HOST_OS == OMR_LINUX)
   public:
   /**
    * Initializes the Relocatable ELF Generator, if the option is enabled
    * @param
    * @return
   */
   void initializeRelocatableELFGenerator(void);

   /**
    * Initializes the Executable ELFGenerator if the option is enabled
    * @param
    * @return
   */
   void initializeExecutableELFGenerator(void);

   protected:

   TR::ELFExecutableGenerator      *_elfExecutableGenerator; /**< Executable ELF generator */
   TR::ELFRelocatableGenerator     *_elfRelocatableGenerator; /**< Relocatable ELF generator */

   // collect information on code cache symbols here, will be post processed into the elf trailer structure
   static TR::CodeCacheSymbolContainer   *_symbolContainer; /**< Symbol container used for tracking CodeCacheSymbols.
                                                                  Note: This static member is not thread-safe. Synchronization is needed if multiple compilation threads are active */
   TR::CodeCacheSymbolContainer          *_relocatableSymbolContainer; /**< Symbol container used for tracking CodeCacheSymbols, for the purpose of writing to relocatable ELF object file */
   TR::CodeCacheRelocationInfoContainer  *_relocations; /**< for tracking relocation info */
   TR::ELFRelocationResolver              _resolver; /**< this translates between a TR::StaticRelocation and the ELF relocation type required for the platform */
   const char                            *_objectFileName; /**< filename of the object file to generate, obtained from cmd line options */


#endif // HOST_OS == OMR_LINUX
   };

} // namespace OMR

#endif // OMR_CODECACHEMANAGER_INCL
