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

class TR_FrontEnd;
class TR_OpaqueMethodBlock;
namespace TR { class CodeCache; }
namespace TR { class CodeCacheManager; }
namespace TR { class CodeCacheMemorySegment; }
namespace TR { class CodeGenerator; }
namespace TR { class Monitor; }
namespace OMR { class CodeCacheHashEntrySlab; }
namespace OMR { class FaintCacheBlock; }
namespace OMR { typedef void CodeCacheTrampolineCode; }
namespace OMR { class CodeCacheManager; }
namespace OMR { typedef CodeCacheManager CodeCacheManagerConnector; }

#if (HOST_OS == OMR_LINUX)

#include <elf.h>                            // for ELF64_ST_INFO, etc

#if defined(TR_HOST_64BIT)
typedef Elf64_Ehdr ELFHeader;
typedef Elf64_Shdr ELFSectionHeader;
typedef Elf64_Phdr ELFProgramHeader;
typedef Elf64_Addr ELFAddress;
typedef Elf64_Sym ELFSymbol;
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
#define ELFClass ELFCLASS64;
#else
typedef Elf32_Ehdr ELFHeader;
typedef Elf32_Shdr ELFSectionHeader;
typedef Elf32_Phdr ELFProgramHeader;
typedef Elf32_Addr ELFAddress;
typedef Elf32_Sym ELFSymbol;
#define ELF_ST_INFO(bind, type) ELF32_ST_INFO(bind, type)
#define ELFClass ELFCLASS32;
#endif

namespace OMR {
struct ELFCodeCacheHeader
   {
   ELFHeader hdr;
   ELFProgramHeader phdr;
   };

struct ELFCodeCacheTrailer
   {
   ELFSectionHeader zeroSection;
   ELFSectionHeader textSection;
   ELFSectionHeader dynsymSection;
   ELFSectionHeader shstrtabSection;
   ELFSectionHeader dynstrSection;

   char zeroSectionName[1];
   char shstrtabSectionName[10];
   char textSectionName[6];
   char dynsymSectionName[8];
   char dynstrSectionName[8];

   // start of a variable sized region: an ELFSymbol structure per symbol + total size of elf symbol names
   ELFSymbol symbols[1];
   };

// structure used to track regions of code cache that will become symbols
typedef struct CodeCacheSymbol
   {
   const char *_name;
   uint32_t _nameLength;
   uint8_t *_start;
   uint32_t _size;
   struct CodeCacheSymbol *_next;
   } CodeCacheSymbol;
} // namespace OMR

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

   CodeCacheManager(TR_FrontEnd *fe) : _fe(fe)
      {
      _initialized = false;
      }

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
   TR_FrontEnd *fe()                       { return _fe; }

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

protected:

   void printRemainingSpaceInCodeCaches();
   void printOccupancyStats();

   TR::CodeCacheConfig            _config;
   TR_FrontEnd                   *_fe;
   TR::CodeCache                 *_lastCache;                         /*!< last code cache round robined through */
   CodeCacheList                  _codeCacheList;                     /*!< list of allocated code caches */
   int32_t                        _curNumberOfCodeCaches;

   // The following 3 fields are for implementation of code cache consolidation
   TR::CodeCache                 *_repositoryCodeCache;
   TR::CodeCacheMemorySegment    *_codeCacheRepositorySegment;
   TR::Monitor                   *_codeCacheRepositoryMonitor;

   bool                           _initialized;                       /*!< flag to indicate if code cache manager has been initialized or not */
   bool                           _lowCodeCacheSpaceThresholdReached; /*!< true if close to exhausting available code cache */

#if (HOST_OS == OMR_LINUX)
   public:
   void initializeELFHeader();
   void initializeELFTrailer();
   void initializeELFHeaderForPlatform(ELFCodeCacheHeader *hdr);

   protected:
   struct ELFCodeCacheHeader     *_elfHeader;
   struct ELFCodeCacheTrailer    *_elfTrailer;
   uint32_t                       _elfTrailerSize;

   // collect information on code cache symbols here, will be post processed into the elf trailer structure
   static CodeCacheSymbol        *_symbols;
   static uint32_t                _numELFSymbols;
   static uint32_t                _totalELFSymbolNamesLength;
#endif // HOST_OS == OMR_LINUX
   };

} // namespace OMR

#endif // OMR_CODECACHEMANAGER_INCL
