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

#ifndef OMR_CODECACHE_INCL
#define OMR_CODECACHE_INCL

/*
 * The following #defines and typedefs must appear before any #includes in this file
 */

#ifndef OMR_CODECACHE_CONNECTOR
#define OMR_CODECACHE_CONNECTOR
namespace OMR { class CodeCache; }
namespace OMR { typedef CodeCache CodeCacheConnector; }
#endif

#include <stddef.h>                            // for size_t
#include <stdint.h>                            // for uint8_t, int32_t, etc
#include "env/defines.h"                       // for HOST_OS, OMR_LINUX, etc
#include "il/DataTypes.hpp"                    // for TR_YesNoMaybe
#include "infra/CriticalSection.hpp"           // for CriticalSection
#include "runtime/CodeCacheConfig.hpp"         // for CodeCacheConfig
#include "runtime/Runtime.hpp"                 // for TR_CCPreLoadedCode, etc
#include "runtime/CodeCacheTypes.hpp"

class TR_OpaqueMethodBlock;
namespace TR { class CodeCache; }
namespace TR { class CodeCacheManager; }
namespace TR { class CodeCacheMemorySegment; }
namespace TR { class CodeGenerator; }
namespace TR { class Monitor; }

extern uint8_t *align(uint8_t *ptr, uint32_t alignment);

namespace OMR
{

// Base class capturing VM dependency on the _warmCodeAlloc and _coldCodeAlloc pointers at the
// beginning of a code cache object. Do not change this base class without fully appreciating this
// dependency.
//
class CodeCacheBase
   {
public:
   void setWarmCodeAlloc(uint8_t *wca)   { _warmCodeAlloc = wca; }
   void setColdCodeAlloc(uint8_t *cca)   { _coldCodeAlloc = cca; }

   uint8_t *getWarmCodeAlloc()   { return _warmCodeAlloc; }
   uint8_t *getColdCodeAlloc()   { return _coldCodeAlloc; }

   void alignWarmCodeAlloc(uint32_t round)  { _warmCodeAlloc = align(_warmCodeAlloc, round); }
   void alignColdCodeAlloc(uint32_t round)  { _coldCodeAlloc = align(_coldCodeAlloc, round); }

   // NOTE: warmCodeAlloc and coldCodeAlloc must be the first 2 entries
   // Other components depend on it !!
   //----------------

   uint8_t * _warmCodeAlloc;
   uint8_t * _coldCodeAlloc;
   };


//
// Remaining classes in this file are extensible
//


class OMR_EXTENSIBLE CodeCache : public CodeCacheBase
   {
   TR::CodeCache *self();

public:
   CodeCache() { }

   void *operator new(size_t s, TR::CodeCache *cache) { return cache; }


   class CacheCriticalSection : public CriticalSection
      {
      public:
      CacheCriticalSection(TR::CodeCache *codeCache);
      };

   uint8_t *getCodeAlloc();
   uint8_t *getCodeBase();
   uint8_t *getCodeTop();
   uint8_t *getHelperBase()            { return _helperBase; }
   uint8_t *getHelperTop()             { return _helperTop; }

   TR::CodeCache * getNextCodeCache()  { return _next; }

   static TR::CodeCache *allocate(TR::CodeCacheManager *manager, size_t segmentSize, int32_t reservingCompThreadID);
   void destroy(TR::CodeCacheManager *manager);

   uint8_t *allocateCodeMemory(size_t warmCodeSize,
                               size_t coldCodeSize,
                               uint8_t **coldCode,
                               bool needsToBeContiguous,
                               bool isMethodHeaderNeeded=true);
   bool resizeCodeMemory(void *memoryBlock, size_t newSize);

   CodeCacheMethodHeader *addFreeBlock(void *metaData);

   uint8_t *findFreeBlock(size_t size, bool isCold, bool isMethodHeaderNeeded);

   void reserve(int32_t reservingCompThreadID);

   void unreserve();

   bool isReserved()                          { return _reserved; }

   TR_YesNoMaybe almostFull()                 { return _almostFull; }
   void setAlmostFull(TR_YesNoMaybe fullness) { _almostFull = fullness; }

   CodeCacheTrampolineCode *reserveTrampoline();
   CodeCacheErrorCode::ErrorCode reserveNTrampolines(int64_t n);
   void unreserveTrampoline();
   CodeCacheTrampolineCode *allocateTrampoline();
   CodeCacheTrampolineCode *allocateTempTrampoline();
   CodeCacheTrampolineCode *findTrampoline(TR_OpaqueMethodBlock *method);
   CodeCacheTrampolineCode *findTrampoline(int32_t helperIndex);
   CodeCacheTrampolineCode *replaceTrampoline(TR_OpaqueMethodBlock *method,
                                              void *oldTrampoline,
                                              void *oldTargetPC,
                                              void *newTargetPC,
                                              bool needSync);
   bool saveTempTrampoline(CodeCacheHashEntry *entry);
   void syncTempTrampolines();

   CodeCacheHashEntry *       allocateHashEntry();

   void                       freeHashEntry(CodeCacheHashEntry *entry);

   size_t                     getFreeContiguousSpace() const        { return _coldCodeAlloc - _warmCodeAlloc; }
   int32_t                    getReservingCompThreadID() const      { return _reservingCompThreadID; }
   void                       setReservingCompThreadID(int32_t n)   { _reservingCompThreadID = n; }
   size_t                     getSizeOfLargestFreeWarmBlock() const { return _sizeOfLargestFreeWarmBlock; }
   size_t                     getSizeOfLargestFreeColdBlock() const { return _sizeOfLargestFreeColdBlock; }

   uint32_t                   tempTrampolinesMax()                  { return _tempTrampolinesMax; }
   bool                       addResolvedMethod(TR_OpaqueMethodBlock *method);

   void                       printOccupancyStats();
   void                       printFreeBlocks();
   void                       checkForErrors();
   void                       writeMethodHeader(void *freeBlock, size_t size, bool isCold);
   void                       dumpCodeCache();

   int32_t                    reserveResolvedTrampoline(TR_OpaqueMethodBlock *method, bool inBinaryEncoding);
   void                       createTrampoline(CodeCacheTrampolineCode *trampoline,
                                               void *targetStartPC,
                                               TR_OpaqueMethodBlock *method);
   bool                       allocateTempTrampolineSyncBlock();

   void                       patchCallPoint(TR_OpaqueMethodBlock *method,
                                             void *callSite,
                                             void *newStartPC,
                                             void *extraArg);

   CodeCacheHashEntry *       findResolvedMethod(TR_OpaqueMethodBlock *method);

   void                       findOrAddResolvedMethod(TR_OpaqueMethodBlock *method);

   TR::CodeCache *            next() { return _next; }
   void                       linkTo(TR::CodeCache *next) { _next = next; }
   uint32_t                   flags() { return _flags; }
   void                       addFlags(uint32_t newFlags) { _flags |= newFlags; }

   bool                       isCCPreLoadedCodeInitialized()                            { return _CCPreLoadedCodeInitialized; }
   void                       setCCPreLoadedCodeAddress(TR_CCPreLoadedCode h, void * a) { _CCPreLoadedCode[h] = a; }
   void *                     getCCPreLoadedCodeAddress(TR_CCPreLoadedCode h, TR::CodeGenerator *cg);

   TR::CodeCacheMemorySegment *segment() { return _segment; }

   bool                       initialize(TR::CodeCacheManager *manager,
                                         TR::CodeCacheMemorySegment *codeCacheSegment,
                                         size_t codeCacheSizeAllocated,
                                         CodeCacheHashEntrySlab *hashEntrySlab);

private:
   void                       updateMaxSizeOfFreeBlocks(CodeCacheFreeCacheBlock *blockPtr, size_t blockSize);

   CodeCacheFreeCacheBlock *  removeFreeBlock(size_t blockSize,
                                              CodeCacheFreeCacheBlock *prev,
                                              CodeCacheFreeCacheBlock *curr);

public:
   bool                       addFreeBlock2WithCallSite(uint8_t *start,
                                                        uint8_t *end,
                                                        char *file,
                                                        uint32_t lineNumber);

   /**
    * @brief Getter for cached CodeCacheManager object
    *
    * @returns The cached CodeCacheManager object
    */
   TR::CodeCacheManager *manager() { return _manager; }

   /**
    * @brief Setter for CodeCacheManager object
    *
    * @param[in] m : The CodeCacheManager object
    */
   void setManager(TR::CodeCacheManager *m) { _manager = m; }

   /**
    * @brief Getter for trampolineSyncList
    *
    * @returns The head of the trampolineSyncList
    */
   CodeCacheTempTrampolineSyncBlock *trampolineSyncList() { return _trampolineSyncList; }

   /**
    * @brief Setter for trampolineSyncList
    *
    * @param[in] sl : A CodeCacheTempTrampolineSyncBlock to be the new head of the trampolineSyncList
    */
   void setTrampolineSyncList(CodeCacheTempTrampolineSyncBlock *sl) { _trampolineSyncList = sl; }

   /**
    * @brief Getter for the current hashEntrySlab
    *
    * @returns The head of the hashEntrySlab list (i.e., the current hashEntrySlab)
    */
   CodeCacheHashEntrySlab *hashEntrySlab() { return _hashEntrySlab; }

   /**
    * @brief Setter for hashEntrySlab
    *
    * @param[in] hes : A CodeCacheHashEntrySlab to be the new head of the hashEntrySlab list
    */
   void setHashEntrySlab(CodeCacheHashEntrySlab *hes) { _hashEntrySlab = hes; }

   /**
    * @brief Getter for the current hashEntryFreeList
    *
    * @returns The head of the hashEntryFreeList list
    */
   CodeCacheHashEntry *hashEntryFreeList() { return _hashEntryFreeList; }

   /**
    * @brief Setter for hashEntryFreeList
    *
    * @param[in] : The new head of the list of free CodeCacheHashEntry's
    */
   void setHashEntryFreeList(CodeCacheHashEntry *fl) { _hashEntryFreeList = fl; }

   /**
    * @brief Getter for the current freeBlockList
    *
    * @returns The head of the freeBlockList list
    */
   CodeCacheFreeCacheBlock *freeBlockList() { return _freeBlockList; }

   /**
    * @brief Setter for freeBlockList
    *
    * @param[in] : The new head of the CodeCacheFreeCacheBlock list
    */
   void setFreeBlockList(CodeCacheFreeCacheBlock *fcb) { _freeBlockList = fcb; }

   /**
   * @brief Remember the position of warmCodeAlloc/coldCodeAlloc pointers
   */
   void setAllocationMark();

   /**
   * @brief Restore warmCodeAlloc/coldCodeAlloc pointers to previously memorized positions
   */
   void resetToAllocationMark();

   TR::CodeCacheManager *_manager;

   TR::Monitor *_mutex;
   uint32_t _flags;
   TR::CodeCache *_next;

   TR::CodeCacheMemorySegment *_segment;

   CodeCacheFreeCacheBlock *_freeBlockList;

   // This is used in an attempt to enforce mutually exclusive ownership.
   // flag accessed under mutex <== This is deceiving! There are two different monitors we may hold (not at the same time!) when we write to this.
   // We can either be holding the code cache monitor *OR* the manager's code cache list monitor.
   // TODO: When we move to C++11 replace volatile with something that actually enforces ordering semantics.
   volatile bool _reserved;
   int32_t _reservingCompThreadID;

   size_t _sizeOfLargestFreeColdBlock;
   size_t _sizeOfLargestFreeWarmBlock;

   uint8_t * _helperBase;
   uint8_t * _helperTop;
   uint8_t * _tempTrampolineBase;
   uint8_t * _tempTrampolineTop;
   uint8_t * _tempTrampolineNext;
   uint8_t * _trampolineAllocationMark;
   uint8_t * _trampolineReservationMark;
   uint8_t * _trampolineBase;
   uint32_t _tempTrampolinesMax;
   CodeCacheTempTrampolineSyncBlock* _trampolineSyncList;

   CodeCacheHashEntrySlab * _hashEntrySlab;
   CodeCacheHashEntry* _hashEntryFreeList;

   CodeCacheHashTable * _resolvedMethodHT;
   CodeCacheHashTable * _unresolvedMethodHT;

   bool      _CCPreLoadedCodeInitialized;
   uint8_t * _CCPreLoadedCodeBase;
   uint8_t * _CCPreLoadedCodeTop;
   void    * _CCPreLoadedCode[TR_numCCPreLoadedCode];

   TR_YesNoMaybe _almostFull;
   CodeCacheMethodHeader *_lastAllocatedBlock; // used for error detection (RAS)
   uint8_t * _warmCodeAllocMark; // used to reset the allocation pointers to previous values
   uint8_t * _coldCodeAllocMark;
   };

} // namespace OMR

#endif // !defined(OMR_CODECACHE_INCL)
