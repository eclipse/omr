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

#ifndef OMR_CODECACHEMANAGER_INCL
#define OMR_CODECACHEMANAGER_INCL

#include <stddef.h>                            // for size_t
#include <stdint.h>                            // for uint8_t, int32_t, etc
#include "il/DataTypes.hpp"                    // for TR_YesNoMaybe
#include "infra/CriticalSection.hpp"           // for CriticalSection
#include "runtime/CodeCacheConfig.hpp"         // for CodeCacheConfig
#include "runtime/MethodExceptionData.hpp"     // for MethodExceptionData
#include "runtime/Runtime.hpp"                 // for TR_CCPreLoadedCode, etc
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
   enum ErrorCode { };

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

   TR::CodeCacheConfig & codeCacheConfig() { return _config; }
   bool codeCacheIsFull() { return _codeCacheIsFull; }
   void setCodeCacheIsFull(bool codeCacheIsFull) { _codeCacheIsFull = codeCacheIsFull; }

   TR::CodeCache *initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup);
   void lateInitialization();

   void destroy();

   // These two functions are for allocating Native backing memory for code cache structures
   // In future these facilities should probably be implemented via cs2 allocators
   void *getMemory(size_t sizeInBytes);
   void  freeMemory(void *memoryToFree);

   TR::CodeCache * allocateCodeCacheObject(TR::CodeCacheMemorySegment *codeCacheSegment,
                                           size_t codeCacheSizeAllocated,
                                           CodeCacheHashEntrySlab *hashEntrySlab);
   void * chooseCacheStartAddress(size_t repositorySize);

   TR::CodeCache * getFirstCodeCache()            { return _codeCacheList._head; }
   int32_t         getCurrentNumberOfCodeCaches() { return _curNumberOfCodeCaches; }
   TR::Monitor *   cacheListMutex() const         { return _codeCacheList._mutex; }

   void addCodeCache(TR::CodeCache *codeCache);

   TR::CodeCacheMemorySegment *getNewCacheMemorySegment(size_t segmentSize, size_t & codeCacheSizeAllocated);

   void        unreserveCodeCache(TR::CodeCache *codeCache);
   TR::CodeCache * reserveCodeCache(bool compilationCodeAllocationsMustBeContiguous,
                                    size_t sizeEstimate,
                                    int32_t compThreadID,
                                    int32_t *numReserved);
   TR::CodeCache * getNewCodeCache(int32_t reservingCompThreadID);

   void addFreeBlock(void *metaData, uint8_t *startPC);

   uint8_t * allocateCodeMemory(size_t warmCodeSize,
                                size_t coldCodeSize,
                                TR::CodeCache **codeCache_pp,
                                uint8_t ** coldCode,
                                bool needsToBeContiguous,
                                bool isMethodHeaderNeeded=true);

   TR::CodeCache * findCodeCacheFromPC(void *inCacheAddress);

   CodeCacheTrampolineCode * findMethodTrampoline(TR_OpaqueMethodBlock *method, void *callingPC);
   CodeCacheTrampolineCode * findHelperTrampoline(void *callingPC, int32_t helperIndex);
   void synchronizeTrampolines();
   CodeCacheTrampolineCode * replaceTrampoline(TR_OpaqueMethodBlock *method,
                                               void *callSite,
                                               void *oldTrampoline,
                                               void *oldTargetPC,
                                               void *newTargetPC,
                                               bool needSync);
   void reservationInterfaceCache(void *callSite, TR_OpaqueMethodBlock *method);

   void performSizeAdjustments(size_t &warmCodeSize,
                               size_t &coldCodeSize,
                               bool needsToBeContiguous,
                               bool isMethodHeaderNeeded);

   bool almostOutOfCodeCache();
   void printMccStats();

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

#if DEBUG
   void dumpCodeCaches();
#endif

   uint8_t * allocateCodeMemoryWithRetries(size_t warmCodeSize,
                                           size_t coldCodeSize,
                                           TR::CodeCache **codeCache_pp,
                                           int32_t allocationRetries,
                                           uint8_t ** coldCode,
                                           bool needsToBeContiguous,
                                           bool isMethodHeaderNeeded=true);
   void setCodeCacheFull() { };  // default does nothing
   void setHasFailedCodeCacheAllocation() { }

   bool initialized() const                 { return _initialized; }
   bool lowCodeCacheSpaceThresholdReached() { return _lowCodeCacheSpaceThresholdReached; }

   void repositoryCodeCacheCreated();
   void registerCompiledMethod(const char *sig, uint8_t *startPC, uint32_t codeSize);
   void registerStaticRelocation(const TR::StaticRelocation &relocation);

   /**
    * Code Cache Access
    *
    * Accessing the internals of a code cache during compilation may require
    * special setup surrounding the manipulation of the code cache.  Two
    * symmetric functions (`prepareToAccessCodeCache` and
    * `prepareToEgressCodeCache`) are provided to handle any actions that must
    * be completed prior to accessing the internals of a code cache and once
    * access to those internals are complete.
    *
    * The functions are generally invoked as a pair and the actions undertaken
    * by each are specific to a language runtime.  Any data that must be
    * captured during `prepareToAccessCodeCache` for use during the matching
    * egress function can be collected in a structure whose type is a
    * project-provided subclass of `OMR::CodeCacheAccessMetaData`.  This structure
    * is passed as a parameter to both functions.
    *
    * An example use case for these functions is that some language environments
    * may need to guard access to the code cache with a monitor because of the
    * possibility of multiple compilation threads competing for the resource.
    * Capturing the execution state prior to accessing the code cache and then
    * restoring it when egressed is important for consistency.
    *
    */

   /**
    * @brief Performs project-specific actions prior to accessing code cache internals
    *
    * @details
    *   Consuming projects can override this function to provide any
    *   functionality needed prior to accessing code cache internals.  For
    *   example, acquiring a monitor to guard access to the code cache. A call
    *   to this function is usually paired with a call to
    *   `prepareToEgressCodeCache` when access is complete.
    *
    * @param[in] comp : TR::Compilation object for the current compilation
    * @param[in] metaData : OMR::CodeCacheAccessMetaData or subclass that holds
    *               any data that must be maintained prior to accessing the code
    *               cache that may be relevant when egressing code cache
    *               internals. For example, the existing state of a monitor
    *               prior to accessing a code cache that must be restored upon
    *               egressing. The use of this parameter is project-specific.
    */
   void prepareToAccessCodeCache(TR::Compilation *comp, OMR::CodeCacheAccessMetaData &metaData)
      {
      }

   /**
    * @brief Performs project-specific actions upon egressing code cache internals
    *
    * @details
    *    Consuming projects can override this function to provide any
    *    functionality needed once access to code cache internals is no longer
    *    required.  For example, releasing a monitor to guard access to the code
    *    cache.  A call to this function is usually paired with a prior call to
    *    `prepareToAccessCodeCache`.
    *
    * @param[in] comp : TR::Compilation object for the current compilation
    * @param[in] metaData : OMR::CodeCacheAccessMetaData or subclass that holds any
    *               data that may have been collected prior to accessing the
    *               code cache internals (through a call to
    *               `prepareToAccessCodeCache`).  For example, the existing state
    *               of a monitor prior to accessing a code cache that must be
    *               restored upon egressing.  The use of this parameter is
    *               project-specific.
    */
   void prepareToEgressCodeCache(TR::Compilation *comp, OMR::CodeCacheAccessMetaData &metaData)
      {
      }

protected:

   void printRemainingSpaceInCodeCaches();
   void printOccupancyStats();

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
   bool                           _codeCacheIsFull;

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
