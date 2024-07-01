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

#include <stddef.h>
#include <stdint.h>
#include "env/defines.h"
#include "il/DataTypes.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/CodeCacheTypes.hpp"
#include "runtime/OMRRSSReport.hpp"
#include "OMR/Bytes.hpp"

class TR_OpaqueMethodBlock;
namespace TR { class CodeCache; }
namespace TR { class CodeCacheManager; }
namespace TR { class CodeCacheMemorySegment; }
namespace TR { class CodeGenerator; }
namespace TR { class Monitor; }

namespace OMR
{

class OMR_EXTENSIBLE CodeCache
   {
   TR::CodeCache *self();

public:
   CodeCache() : _coldCodeRSSRegion(NULL) { }

   void *operator new(size_t s, TR::CodeCache *cache) { return cache; }
   void operator delete(void *p, TR::CodeCache *cache) { /* do nothing */ }


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

   void setWarmCodeAlloc(uint8_t *wca)   { _warmCodeAlloc = wca; }
   void setColdCodeAlloc(uint8_t *cca)   { _coldCodeAlloc = cca; }

   uint8_t *getWarmCodeAlloc()   { return _warmCodeAlloc; }
   uint8_t *getColdCodeAlloc()   { return _coldCodeAlloc; }

   void alignWarmCodeAlloc(uint32_t round)  { _warmCodeAlloc = reinterpret_cast<uint8_t *>(align(reinterpret_cast<size_t>(_warmCodeAlloc), round)); }
   void alignColdCodeAlloc(uint32_t round)  { _coldCodeAlloc = reinterpret_cast<uint8_t *>(align(reinterpret_cast<size_t>(_coldCodeAlloc), round)); }

   TR::CodeCache * getNextCodeCache()  { return _next; }

   static TR::CodeCache *allocate(TR::CodeCacheManager *manager, size_t segmentSize, int32_t reservingCompThreadID);
   void destroy(TR::CodeCacheManager *manager);

   uint8_t *allocateCodeMemory(size_t warmCodeSize,
                               size_t coldCodeSize,
                               uint8_t **coldCode,
                               bool needsToBeContiguous,
                               bool isMethodHeaderNeeded=true);

   /**
    * @brief Trims the size of the previously allocated code memory in this
    *        CodeCache to the specified length.  Any leftover bytes are reclaimed.
    *
    * @param[in] codeMemoryStart : void* address of the start of the code memory
    *               to trim
    * @param[in] actualSizeInBytes : size_t describing the actual number of code
    *               memory bytes to allocate.  A specified size of 0 will return
    *               without any adjustment.
    *
    * @return true if the code memory was successfully reduced; false otherwise.
    */
   bool trimCodeMemoryAllocation(void *codeMemoryStart, size_t actualSizeInBytes);

   CodeCacheMethodHeader *addFreeBlock(void *metaData);

   uint8_t *findFreeBlock(size_t size, bool isCold, bool isMethodHeaderNeeded);

   void reserve(int32_t reservingCompThreadID);

   void unreserve();

   bool isReserved()                          { return _reserved; }

   TR_YesNoMaybe almostFull()                 { return _almostFull; }
   void setAlmostFull(TR_YesNoMaybe fullness) { _almostFull = fullness; }

   /**
    * @brief DEPRECATED Reserve space for a trampoline in the current code cache.
    *        This function simply calls reserveSpaceForTrampoline and will exist only
    *        while API uses in downstream projects are cleaned up.
    */
   CodeCacheErrorCode::ErrorCode reserveSpaceForTrampoline_bridge(int32_t numTrampolines = 1);

   /**
    * @brief Reserve space for a trampoline in the current code cache.
    *
    * @details
    *    Space for multiple trampolines can be reserved at once.
    *    Upon a reservation failure, the code cache is marked as nearing capacity.
    *
    * @param[in] numTrampolines : optional parameter for the number of trampolines to
    *               request at once.  Default value is 1.
    *
    * @return : ERRORCODE_SUCCESS on success; ERRORCODE_INSUFFICIENTSPACE on failure
    */
   CodeCacheErrorCode::ErrorCode reserveSpaceForTrampoline(int32_t numTrampolines = 1);

   /**
    * @brief Reclaim the previously reserved space for a single trampoline in the
    *        current code cache.
    */
   void unreserveSpaceForTrampoline();

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

   /**
    * @brief Initialize an allocated CodeCache object
    *
    * @param[in] manager : the TR::CodeCacheManager
    * @param[in] codeCacheSegment : the code cache memory segment that has been allocated
    * @param[in] allocatedCodeCacheSizeInBytes : the size (in bytes) of the allocated code cache
    *
    * @return true on a successful initialization; false otherwise.
    */
   bool                       initialize(TR::CodeCacheManager *manager,
                                         TR::CodeCacheMemorySegment *codeCacheSegment,
                                         size_t allocatedCodeCacheSizeInBytes);

private:
   void                       updateMaxSizeOfFreeBlocks(CodeCacheFreeCacheBlock *blockPtr, size_t blockSize);

   CodeCacheFreeCacheBlock *  removeFreeBlock(size_t blockSize,
                                              CodeCacheFreeCacheBlock *prev,
                                              CodeCacheFreeCacheBlock *curr);

public:
   bool                       addFreeBlock2WithCallSite(uint8_t *start,
                                                        uint8_t *end,
                                                        const char *file,
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
    * @brief Getter for the base address of temporary trampolines
    *
    * @returns The base address of temporary trampolines
    */
   uint8_t *tempTrampolineBase() { return _tempTrampolineBase; }

   /**
    * @brief Setter for the base address of temporary trampolines
    *
    * @param[in] address : The new base address of temporary trampolines
    */
   void setTempTrampolineBase(uint8_t *address) { _tempTrampolineBase = address; }

   /**
    * @brief Getter for the top address of temporary trampolines
    *
    * @returns The top address of temporary trampolines
    */
   uint8_t *tempTrampolineTop() { return _tempTrampolineTop; }

   /**
    * @brief Setter for the top address of temporary trampolines
    *
    * @param[in] address : The new top address of temporary trampolines
    */
   void setTempTrampolineTop(uint8_t *address) { _tempTrampolineTop = address; }

   /**
    * @brief Getter for the next allocation address of temporary trampolines
    *
    * @returns The next allocation address of temporary trampolines
    */
   uint8_t *tempTrampolineNext() { return _tempTrampolineNext; }

   /**
    * @brief Setter for the next allocation address of temporary trampolines
    *
    * @param[in] address : The new next allocation address of temporary trampolines
    */
   void setTempTrampolineNext(uint8_t *address) { _tempTrampolineNext = address; }

   /**
    * @brief Getter for the allocation mark for trampolines
    *
    * @returns The allocation mark for trampolines
    */
   uint8_t *trampolineAllocationMark() { return _trampolineAllocationMark; }

   /**
    * @brief Setter for the trampoline allocation mark
    *
    * @param[in] address : The new trampoline allocation mark
    */
   void setTrampolineAllocationMark(uint8_t *address) { _trampolineAllocationMark = address; }

   /**
    * @brief Adjusts the trampoline allocation mark by the specified amount
    *
    * @param[in] numBytes : The number of bytes to adjust the allocation mark by.  Negative
    *              values are permitted
    */
   void adjustTrampolineAllocationMark(int32_t numBytes) { _trampolineAllocationMark += numBytes; }

   /**
    * @brief Getter for the reservation mark for trampolines
    *
    * @returns The reservation mark for trampolines
    */
   uint8_t *trampolineReservationMark() { return _trampolineReservationMark; }

   /**
    * @brief Setter for the trampoline reservation mark
    *
    * @param[in] address : The new trampoline reservation mark
    */
   void setTrampolineReservationMark(uint8_t *address) { _trampolineReservationMark = address; }

   /**
    * @brief Adjusts the trampoline reservation mark by the specified amount
    *
    * @param[in] numBytes : The number of bytes to adjust the reservation mark by.  Negative
    *              values are permitted
    */
   void adjustTrampolineReservationMark(int32_t numBytes) { _trampolineReservationMark += numBytes; }

   /**
    * @brief Getter for the trampoline base address
    *
    * @returns The base address of trampolines
    */
   uint8_t *trampolineBase() { return _trampolineBase; }

   /**
    * @brief Setter for the trampoline base address
    *
    * @param[in] address : The new trampoline base address
    */
   void setTrampolineBase(uint8_t *address) { _trampolineBase = address; }

   uint8_t * _warmCodeAlloc;

   uint8_t * _coldCodeAlloc;

   TR::CodeCacheManager *_manager;

   TR::Monitor *_mutex;
   uint32_t _flags;
   TR::CodeCache *_next;

   TR::CodeCacheMemorySegment *_segment;

   CodeCacheFreeCacheBlock *_freeBlockList;

   /**
   * @brief Returns pointer to the cold code RSS Region
   */
   OMR::RSSRegion *getColdCodeRSSRegion() { return _coldCodeRSSRegion; }

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

   OMR::RSSRegion   *_coldCodeRSSRegion;
   };

} // namespace OMR

#endif // !defined(OMR_CODECACHE_INCL)
