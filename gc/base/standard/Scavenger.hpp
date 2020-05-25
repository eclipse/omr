/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(SCAVENGER_HPP_)
#define SCAVENGER_HPP_

#include "omrcfg.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "omrcomp.h"

#include "CollectionStatisticsStandard.hpp"
#include "Collector.hpp"
#include "ConcurrentPhaseStatsBase.hpp"
#include "CopyScanCacheList.hpp"
#include "CopyScanCacheStandard.hpp"
#include "CycleState.hpp"
#include "GCExtensionsBase.hpp"
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
#include "MasterGCThread.hpp"
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#include "ScavengerDelegate.hpp"

struct J9HookInterface;
class GC_ObjectScanner;
class MM_AllocateDescription;
class MM_CollectorLanguageInterface;
class MM_Dispatcher;
class MM_EnvironmentBase;
class MM_HeapRegionManager;
class MM_MemoryPool;
class MM_MemorySubSpace;
class MM_MemorySubSpaceSemiSpace;
class MM_PhysicalSubArena;
class MM_RSOverflow;
class MM_SublistPool;

struct OMR_VM;

extern "C" void concurrentScavengerAsyncCallbackHandler(OMR_VMThread *omrVMThread);

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_Scavenger : public MM_Collector
{
	/*
	 * Data members
	 */
private:
	MM_ScavengerDelegate _delegate;

	const uintptr_t _objectAlignmentInBytes;	/**< Run-time objects alignment in bytes */
	bool _isRememberedSetInOverflowAtTheBeginning; /**< Cached RS Overflow flag at the beginning of the scavenge */

	MM_GCExtensionsBase *_extensions;
	
	MM_Dispatcher *_dispatcher;

	volatile uintptr_t _doneIndex; /**< sequence ID of completeScan loop, which we may have a few during one GC cycle */

	MM_MemorySubSpaceSemiSpace *_activeSubSpace; /**< top level new subspace subject to GC */
	MM_MemorySubSpace *_evacuateMemorySubSpace; /**< cached pointer to evacuate subspace within active subspace */
	MM_MemorySubSpace *_survivorMemorySubSpace; /**< cached pointer to survivor subspace within active subspace */
	MM_MemorySubSpace *_tenureMemorySubSpace;

	void *_evacuateSpaceBase, *_evacuateSpaceTop;	/**< cached base and top heap pointers within evacuate subspace */
	void *_survivorSpaceBase, *_survivorSpaceTop;	/**< cached base and top heap pointers within survivor subspace */

	uintptr_t _tenureMask; /**< A bit mask indicating which generations should be tenured on scavenge. */
	bool _expandFailed;
	bool _failedTenureThresholdReached;
	uintptr_t _failedTenureLargestObject;
	uintptr_t _countSinceForcingGlobalGC;

	bool _expandTenureOnFailedAllocate;
	bool _cachedSemiSpaceResizableFlag;
	uintptr_t _minTenureFailureSize;
	uintptr_t _minSemiSpaceFailureSize;

	MM_CycleState _cycleState;  /**< Embedded cycle state to be used as the master cycle state for GC activity */
	MM_CollectionStatisticsStandard _collectionStatistics;  /** Common collect stats (memory, time etc.) */

	MM_CopyScanCacheList _scavengeCacheFreeList; /**< pool of unused copy-scan caches */
	MM_CopyScanCacheList _scavengeCacheScanList; /**< scan lists */
	volatile uintptr_t _cachedEntryCount; /**< non-empty scanCacheList count (not the total count of caches in the lists) */
	uintptr_t _cachesPerThread; /**< maximum number of copy and scan caches required per thread at any one time */
	omrthread_monitor_t _scanCacheMonitor; /**< monitor to synchronize threads on scan lists */
	omrthread_monitor_t _freeCacheMonitor; /**< monitor to synchronize threads on free list */
	uintptr_t _waitingCountAliasThreshold; /**< Only alias a copy cache IF the number of threads waiting hasn't reached the threshold*/
	volatile uintptr_t _waitingCount; /**< count of threads waiting  on scan cache queues (blocked via _scanCacheMonitor); threads never wait on _freeCacheMonitor */
	uintptr_t _cacheLineAlignment; /**< The number of bytes per cache line which is used to determine which boundaries in memory represent the beginning of a cache line */
	volatile bool _rescanThreadsForRememberedObjects; /**< Indicates that thread-referenced objects were tenured and threads must be rescanned */

	volatile uintptr_t _backOutDoneIndex; /**< snapshot of _doneIndex, when backOut was detected */

	void *_heapBase;  /**< Cached base pointer of heap */
	void *_heapTop;  /**< Cached top pointer of heap */
	MM_HeapRegionManager *_regionManager;

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	MM_MasterGCThread _masterGCThread; /**< An object which manages the state of the master GC thread */
	
	volatile enum ConcurrentState {
		concurrent_phase_idle,
		concurrent_phase_init,
		concurrent_phase_roots,
		concurrent_phase_scan,
		concurrent_phase_complete
	} _concurrentPhase;
	
	bool _currentPhaseConcurrent;
	
	uint64_t _concurrentScavengerSwitchCount; /**< global counter of cycle start and cycle end transitions */
	volatile bool _shouldYield; /**< Set by the first GC thread that observes that a criteria for yielding is met. Reset only when the concurrent phase is finished. */

	MM_ConcurrentPhaseStatsBase _concurrentPhaseStats;
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#define IS_CONCURRENT_ENABLED _extensions->isConcurrentScavengerEnabled()

protected:

public:
	OMR_VM *_omrVM;
	
	/*
	 * Function members
	 */
private:
	/**
	 * Flush copy/scan count updates, the threads reference and remembered set caches before waiting in getNextScanCache.
	 * This removes the requirement of a synchronization point after calls to completeScan when
	 * it is followed by reference or remembered set processing.
	 * @param env - current thread environment
	 * @param finalFlush - lets the copy/scan flush know if it's the last thread performing the flush
	 */
	void flushBuffersForGetNextScanCache(MM_EnvironmentStandard *env, bool finalFlush = false);
	
	void saveMasterThreadTenureTLHRemainders(MM_EnvironmentStandard *env);
	void restoreMasterThreadTenureTLHRemainders(MM_EnvironmentStandard *env);
	
	void setBackOutFlag(MM_EnvironmentBase *env, BackOutState value);
	MMINLINE bool isBackOutFlagRaised() { return _extensions->isScavengerBackOutFlagRaised(); }
	
	/**
	 * Check if concurrent phase of the cycle should yield to an external activity. If so, set the flag so that other GC threads react appropriately
	 */ 
	MMINLINE bool checkAndSetShouldYieldFlag(MM_EnvironmentStandard *env);
	
	/**
	 * Check if top level scan loop should be aborted before the work is done
	 */
	MMINLINE bool shouldAbortScanLoop(MM_EnvironmentStandard *env) {
		bool shouldAbort = false;

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (IS_CONCURRENT_ENABLED) {
			/* Concurrent Scavenger needs to drain the scan queue in last scan loop before aborted handling starts.
			 * It is however fine to leave it populated, if we want to yield in a middle of concurrent phase which aborted,
			 * since there will be at least one scan loop afterwards in complete phase that will drain it. Bottom line,
			 * we don't care about isBackOutFlagRaised when deciding whether to yield.
			 */
					 
			shouldAbort = _shouldYield;
			if (shouldAbort) {
				Assert_MM_true(concurrent_phase_scan == _concurrentPhase);
				/* Since we are aborting the scan loop without synchornizing with other GC threads (before which we flush buffers),
				 * we have to do it now. 
				 * There should be no danger in not synchonizing with other threads, since we can only abort/yield in main scan loop
				 * and not during clearable STW phase, where is a potential danger of entering a scan loop without ensuring all
				 * threads flushed buffers from previous scan loop.
				 */
				flushBuffersForGetNextScanCache(env);
			}
		} else
#endif /* #if defined(OMR_GC_CONCURRENT_SCAVENGER) */
		{		
			shouldAbort = isBackOutFlagRaised();
		}
		
		return shouldAbort;
	}

	/** 
	 * A simple heuristic that projects the need for copy-scan cache size pool, based on heap size that Scavenger operates with)
	 */	
	uintptr_t calculateMaxCacheCount(uintptr_t activeMemorySize);

public:
	/**
	 * Hook callback. Called when a global collect has started
	 */
	static void hookGlobalCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

	/**
	 * Hook callback. Called when a global collect has completed
	 */
	static void hookGlobalCollectionComplete(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	
	/**
	 *  This method is called on the start of a global GC.
	 *  @param env the current thread.
	 */
	void globalCollectionStart(MM_EnvironmentBase *env);
	
	/**
	 *  This method is called on the completion of a global GC.
	 *  @param env the current thread.
	 */
	void globalCollectionComplete(MM_EnvironmentBase *env);

	/**
	 * Test backout state and inhibit array splitting once backout starts.
	 * @param env current thread environment
	 * @param objectptr the object to scan
	 * @param objectScannerState points to space for inline allocation of scanner
	 * @param flags scanner flags
	 * @return the object scanner
	 */
	MMINLINE GC_ObjectScanner *getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectptr, void *objectScannerState, uintptr_t flags);

	uintptr_t calculateCopyScanCacheSizeForWaitingThreads(uintptr_t maxCacheSize, uintptr_t threadCount, uintptr_t waitingThreads);
	uintptr_t calculateCopyScanCacheSizeForQueueLength(uintptr_t maxCacheSize, uintptr_t threadCount, uintptr_t scanCacheCount);
	MMINLINE uintptr_t calculateOptimumCopyScanCacheSize(MM_EnvironmentStandard *env);
	MMINLINE MM_CopyScanCacheStandard *reserveMemoryForAllocateInSemiSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectToEvacuate, uintptr_t objectReserveSizeInBytes);
	MM_CopyScanCacheStandard *reserveMemoryForAllocateInTenureSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectToEvacuate, uintptr_t objectReserveSizeInBytes);

	MM_CopyScanCacheStandard *getNextScanCache(MM_EnvironmentStandard *env);

	/**
	 * Implementation of CopyAndForward for slotObject input format
	 * @param slotObject input field in slotObject format
	 */
	MMINLINE bool copyAndForward(MM_EnvironmentStandard *env, GC_SlotObject *slotObject);

	MMINLINE bool copyAndForward(MM_EnvironmentStandard *env, volatile omrobjectptr_t *objectPtrIndirect);

	MMINLINE omrobjectptr_t copy(MM_EnvironmentStandard *env, MM_ForwardedHeader* forwardedHeader);
	
	/* Flush remaining Copy Scan updates which would otherwise be discarded 
	 * @param majorFlush last thread to flush updates should perform a major flush (push accumulated updates to history record) 
	 */ 
	MMINLINE void flushCopyScanCounts(MM_EnvironmentBase* env, bool majorFlush);
	MMINLINE void updateCopyScanCounts(MM_EnvironmentBase* env, uint64_t slotsScanned, uint64_t slotsCopied);
	bool splitIndexableObjectScanner(MM_EnvironmentStandard *env, GC_ObjectScanner *objectScanner, uintptr_t startIndex, omrobjectptr_t *rememberedSetSlot);

	/**
	 * Scavenges the contents of an object.
	 * @param env The environment.
	 * @param objectPtr The pointer to the object.
	 * @param scanCache The scan cache for the environment
	 * @param flags A bit map of GC_ObjectScanner::InstanceFlags.
	 * @return Whether or not objectPtr should be remembered.
	 */
	MMINLINE bool scavengeObjectSlots(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard *scanCache, omrobjectptr_t objectPtr, uintptr_t flags, omrobjectptr_t *rememberedSetSlot);
	MMINLINE MM_CopyScanCacheStandard *incrementalScavengeObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, MM_CopyScanCacheStandard* scanCache);	
	
	/**
	 * For fast traversal of deep structure nodes - scan objects with self referencing fields with priority
	 * Split into two functions deepScan and deepScanOutline. Frequently called checks (see lazy start check) must be inlined
	 * @param env The environment.
	 * @param objectPtr The pointer to the object.
	 * @param priorityFieldOffset1 Offset to the first priority field of the object
	 * @param priorityFieldOffset2 Offset to the second priority field, if it can't follow through in one direction, 
	 * it will attempt to use the second self referencing field 
	 */
	MMINLINE void
	deepScan(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, uintptr_t priorityFieldOffset1, uintptr_t priorityFieldOffset2)
	{
		/**
		* Inhibit the special treatment routine with relatively high probability to skip over most  
		* false positives (shorter lists), while only marginally delay detection of very deep structures.
		*/	
		if (shouldStartDeepScan(env, objectPtr)) {
			deepScanOutline(env, objectPtr, priorityFieldOffset1, priorityFieldOffset2);
		}
	}
	
	/**
	 * Deep scan lazy start check - condition used for gatekeeping
	 * @param env The environment.
	 * @param objectPtr The pointer to the object.
	 * @return True If deep scan should start
	 */
	MMINLINE bool
	shouldStartDeepScan(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
	{
		/* Check last few LSB of the object address for probability 1/16 */
		return (0 == ((uintptr_t)objectPtr & 0x78)); 
	}
	
	
	void deepScanOutline(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, uintptr_t priorityFieldOffset1, uintptr_t priorityFieldOffset2);

	MMINLINE bool scavengeRememberedObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	void scavengeRememberedSetList(MM_EnvironmentStandard *env);
	void scavengeRememberedSetOverflow(MM_EnvironmentStandard *env);
	MMINLINE void flushRememberedSet(MM_EnvironmentStandard *env);
	void pruneRememberedSetList(MM_EnvironmentStandard *env);
	void pruneRememberedSetOverflow(MM_EnvironmentStandard *env);

	/**
	 * Checks if the  Object should be remembered or not
	 * @param env Standard Environment
	 * @param objectPtr The pointer to the  Object in Tenured Space.
	 * @return True If Object should be remembered
	 */
	bool shouldRememberObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/**
	 * BackOutFixSlot implementation
	 * @param slotObject input field in slotObject format
	 */
	bool backOutFixSlot(GC_SlotObject *slotObject);

	void backoutFixupAndReverseForwardPointersInSurvivor(MM_EnvironmentStandard *env);
	void processRememberedSetInBackout(MM_EnvironmentStandard *env);
	void completeBackOut(MM_EnvironmentStandard *env);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	void fixupNurserySlots(MM_EnvironmentStandard *env);
	void fixupObjectScan(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	bool fixupSlot(GC_SlotObject *slotObject);
	bool fixupSlotWithoutCompression(volatile omrobjectptr_t *slotPtr);
	
	void scavengeRememberedSetListIndirect(MM_EnvironmentStandard *env);
	void scavengeRememberedSetListDirect(MM_EnvironmentStandard *env);

	MMINLINE void handleInactiveSurvivorCopyScanCache(MM_EnvironmentStandard *currentEnv, MM_EnvironmentStandard *targetEnv, bool flushCaches, bool final);
	MMINLINE void handleSurvivorCopyScanCache(MM_EnvironmentStandard *currentEnv, MM_EnvironmentStandard *targetEnv, bool flushCaches, bool final);
	MMINLINE void handleInactiveTenureCopyScanCache(MM_EnvironmentStandard *currentEnv, MM_EnvironmentStandard *targetEnv, bool flushCaches, bool final);
	MMINLINE void handleTenureCopyScanCache(MM_EnvironmentStandard *currentEnv, MM_EnvironmentStandard *targetEnv, bool flushCaches, bool final);
	MMINLINE void handleInactiveDeferredCopyScanCache(MM_EnvironmentStandard *currentEnv, MM_EnvironmentStandard *targetEnv, bool flushCaches, bool final);
	MMINLINE void handleDeferredCopyScanCache(MM_EnvironmentStandard *currentEnv, MM_EnvironmentStandard *targetEnv, bool flushCaches, bool final);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	/**
 	 * Request for percolate GC
 	 * 
 	 * @return true if Global GC was executed, false if concurrent kickoff forced or Global GC is not possible 
 	 */
	bool percolateGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, PercolateReason percolateReason, uint32_t gcCode);
	
	void reportGCCycleStart(MM_EnvironmentStandard *env);
	void reportGCCycleEnd(MM_EnvironmentStandard *env);
	void reportGCCycleFinalIncrementEnding(MM_EnvironmentStandard *envModron);

	MMINLINE void clearExpandFailedFlag() { _expandFailed = false; };
	MMINLINE void setExpandFailedFlag() { _expandFailed = true; };
	MMINLINE bool expandFailed() { return _expandFailed; };

	MMINLINE void clearFailedTenureThresholdFlag() { _failedTenureThresholdReached = false; };
	MMINLINE void setFailedTenureThresholdFlag() { _failedTenureThresholdReached = true; };
	MMINLINE void setFailedTenureLargestObject(uintptr_t size) { _failedTenureLargestObject = size; };
	MMINLINE uintptr_t getFailedTenureLargestObject() { return _failedTenureLargestObject; };
	MMINLINE bool failedTenureThresholdReached() { return _failedTenureThresholdReached; };

	void completeScanCache(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard* scanCache);
	void incrementalScanCacheBySlot(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard* scanCache);

	MMINLINE MM_CopyScanCacheStandard *aliasToCopyCache(MM_EnvironmentStandard *env, GC_SlotObject *scannedSlot, MM_CopyScanCacheStandard* scanCache, MM_CopyScanCacheStandard* copyCache);
	MMINLINE uintptr_t scanCacheDistanceMetric(MM_CopyScanCacheStandard* cache, GC_SlotObject *scanSlot);
	MMINLINE uintptr_t copyCacheDistanceMetric(MM_CopyScanCacheStandard* cache);

	MMINLINE MM_CopyScanCacheStandard *getNextScanCacheFromList(MM_EnvironmentStandard *env);
	/**
	 * Called at the end of a task to return empty caches to the global free pool
	 */
	void finalReturnCopyCachesToFreeList(MM_EnvironmentStandard *env);
	/* 
	 * Used by CS to return empty caches during intermediate blocks, to aid with more precise counting of free/empty cache in the pool
	 */
	void returnEmptyCopyCachesToFreeList(MM_EnvironmentStandard *env);
	MMINLINE void addCacheEntryToScanListAndNotify(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard *newCacheEntry);

	MMINLINE bool
	isWorkAvailableInCacheWithCheck(MM_CopyScanCacheStandard *cache)
	{
		return ((NULL != cache) && cache->isScanWorkAvailable());
	}

	MMINLINE bool
	isEmptyCacheWithCheck(MM_CopyScanCacheStandard *cache)
	{
		return ((NULL != cache) && !cache->isScanWorkAvailable());
	}

	/**
	 * An attempt to get a preallocated scan cache header, free list will be locked
	 * @param env - current thread environment
	 * @return pointer to scan cache header or NULL if attempt fail
	 */
	MMINLINE MM_CopyScanCacheStandard *getFreeCache(MM_EnvironmentStandard *env);

	/**
	 * An attempt to create chunk of scan cache headers in heap
	 * @param env - current thread environment
	 * @return pointer to allocated chunk of scan cache headers or NULL if attempt fail
	 */
	MM_CopyScanCacheStandard *createCacheInHeap(MM_EnvironmentStandard *env);

	/**
	 * Return cache back to free list if it is not used for copy.
	 * Clear cache if it has not been cleared yet
	 * @param env - current thread environment
	 * @param cache cache to be flushed
	 */
	void flushCache(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard *cache);

	/**
	 * Check can local copy cache be reused
	 * To be reused cache should be has not been scanned and has no scan work to do
	 * @param env - current thread environment
	 * @param cache cache to be flushed
	 * @return true is cache can be reused
	 */
	bool canLocalCacheBeReused(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard *cache);

	/**
	 * Release local Copy cache
	 * Put cache to scanned list if it has not been scanned and has scan work to do
	 * @param env - current thread environment
	 * @param cache cache to be flushed
	 * @return cache to reuse, if any
	 */
	MM_CopyScanCacheStandard *releaseLocalCopyCache(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard *cache);

	/**
	 * Clear cache
	 * Return memory has not been allocated to memory pool
	 * @param env - current thread environment
	 * @param cache cache to be flushed
	 * @return true if sufficent amount of memory is left in the cache to create a 'remainder' for later usage
	 */
	bool clearCache(MM_EnvironmentStandard *env, MM_CopyScanCacheStandard *cache);

	/**
	 * Called (typically at the end of GC) to explicitly abandon the TLH remainders (for the calling thread)
	 */
	void abandonSurvivorTLHRemainder(MM_EnvironmentStandard *env);
	void abandonTenureTLHRemainder(MM_EnvironmentStandard *env, bool preserveRemainders = false);
	
	MMINLINE bool activateSurvivorCopyScanCache(MM_EnvironmentStandard *env);
	MMINLINE bool activateTenureCopyScanCache(MM_EnvironmentStandard *env);
	void activateDeferredCopyScanCache(MM_EnvironmentStandard *env);

	void reportGCStart(MM_EnvironmentStandard *env);
	void reportGCEnd(MM_EnvironmentStandard *env);
	void reportGCIncrementStart(MM_EnvironmentStandard *env);
	void reportGCIncrementEnd(MM_EnvironmentStandard *env);
	void reportScavengeStart(MM_EnvironmentStandard *env);
	void reportScavengeEnd(MM_EnvironmentStandard *env, bool lastIncrement);

	/**
	 * Add the specified object to the remembered set.
	 * Grow the remembered set if necessary and, if that fails, overflow.
	 * If the object is already remembered or is in new space, do nothing.
	 *
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to remember
	 */
	void rememberObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/*
	 * Scan Tenure and add all found Remembered objects to Overflow
	 * @param env - Environment
	 * @param overflow - pointer to RS Overflow
	 */
	void addAllRememberedObjectsToOverflow(MM_EnvironmentStandard *env, MM_RSOverflow *overflow);

	void clearRememberedSetLists(MM_EnvironmentStandard *env);

	MMINLINE bool isRememberedSetInOverflowState() { return _extensions->isRememberedSetInOverflowState(); }
	MMINLINE void setRememberedSetOverflowState() { _extensions->setRememberedSetOverflowState(); }
	MMINLINE void clearRememberedSetOverflowState() { _extensions->clearRememberedSetOverflowState(); }

	/* Auto-remember stack objects so JIT can omit generational barriers */
	void rescanThreadSlots(MM_EnvironmentStandard *env);
	/**
	 * Determine if the specified remembered object was referenced by a thread or stack.
	 *
	 * @param env[in] the current thread
	 * @param objectPtr[in] a remembered object
	 *
	 * @return true if the object is a remembered thread reference
	 */
	bool isRememberedThreadReference(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/**
	 * Determine if the specified remembered object was referenced by a thread or stack and process it
	 *
	 * @param env[in] the current thread
	 * @param objectPtr[in] a remembered object
	 *
	 * @return true if the object is a remembered thread reference
	 */
	bool processRememberedThreadReference(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/**
	 * Clear global (not thread local) stats for current phase/increment
	 * @param firstIncrement true if first increment in a cycle
	 */
	void clearIncrementGCStats(MM_EnvironmentBase *env, bool firstIncrement);
	/**
	 * Clear global (not thread local) cumulative cycle stats 
	 */
	void clearCycleGCStats(MM_EnvironmentBase *env);
	/**
	 * Clear thread local stats for current phase/increment
	 * @param firstIncrement true if first increment in a cycle
	 */
	void clearThreadGCStats(MM_EnvironmentBase *env, bool firstIncrement);
	/**
	 * Merge thread local stats for current phase/increment in to global current increment stats
	 */	
	void mergeThreadGCStats(MM_EnvironmentBase *env);
	/**
	 * Merge global current increment stats in to global cycle stats
	 * @param firstIncrement true if last increment in a cycle
	 */		
	void mergeIncrementGCStats(MM_EnvironmentBase *env, bool lastIncrement);
	/**
	 * Common merge logic used for both thread and increment level merges.
	 */
	void mergeGCStatsBase(MM_EnvironmentBase *env, MM_ScavengerStats *finalGCStats, MM_ScavengerStats *scavStats);
	bool canCalcGCStats(MM_EnvironmentStandard *env);
	void calcGCStats(MM_EnvironmentStandard *env);

	void scavenge(MM_EnvironmentBase *env);
	bool scavengeCompletedSuccessfully(MM_EnvironmentStandard *env);
	virtual	void masterThreadGarbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool initMarkMap = false, bool rebuildMarkBits = false);

	MMINLINE uintptr_t
	isTiltedScavenge()
	{
		return _extensions->tiltedScavenge ? 1 : 0;
	}

	/**
	 * Calculate tilt ratio
	 * @return tiltRatio
	 */
	uintptr_t calculateTiltRatio();

	/**
	 * The implementation of the Lookback scavenger tenure strategy.
	 * This strategy would, for each object age, check the survival history of
	 * that generation of objects (a diagonal down-left check in the survival
	 * history). If, historically, the survival rate of that generation of
	 * objects is always above minimumSurvivalRate, that age will be set for
	 * tenure this scavenge.
	 * @param minimumSurvivalRate The minimum survival rate required to consider tenuring.
	 * @return A tenure mask for the resulting ages to tenure.
	 */
	uintptr_t calculateTenureMaskUsingLookback(double minimumSurvivalRate);

	/**
	 * The implementation of the History scavenger tenure strategy.
	 * This strategy would, for each object age, check the survival history of
	 * that age (a vertical check in the survival history). If, historically,
	 * the survival rate of that age is always above minimumSurvivalRate, that
	 * age will be set for tenure this scavenge.
	 * @param minimumSurvivalRate The minimum survival rate required to consider tenuring.
	 * @return A tenure mask for the resulting ages to tenure.
	 */
	uintptr_t calculateTenureMaskUsingHistory(double minimumSurvivalRate);

	/**
	 * The implementation of the Fixed scavenger tenure strategy.
	 * This strategy will tenure any object who's age is above or equal to
	 * tenureAge.
	 * @param tenureAge The tenure age that objects should be tenured at.
	 * @return A tenure mask for the resulting ages to tenure.
	 */
	uintptr_t calculateTenureMaskUsingFixed(uintptr_t tenureAge);

	/**
	 * Calculates which generations should be tenured in the form of a bit mask.
	 * @return mask of ages to tenure
	 */
	uintptr_t calculateTenureMask();

	/**
	 * reset LargeAllocateStats in Tenure Space
	 * @param env Master GC thread.
	 */
	void resetTenureLargeAllocateStats(MM_EnvironmentBase *env);

	/* API used by ParallelScavengeTask to set _waitingCountAliasThreshold. */
	void setAliasThreshold(uintptr_t waitingCountAliasThreshold) { _waitingCountAliasThreshold = waitingCountAliasThreshold; }
	
	/**
	 * Notify Collector that a thread is about to acquire Exclusive VM access.
	 * This can be useful in scenario when GC is concurrent, and currently in progress.
	 * env invoking thread that is about to acquire Exclusive VM access
	 */
	void externalNotifyToYield(MM_EnvironmentBase* env);
	
	/**
	 * For CS, last thread to block, before notifying other threads to unblock 
	 * will check if all caches are returned to the free global pool. If not,
	 * it will activate Async Handler to force mutators to flush caches, 
	 * and go back to scanning and eventually getting to this point again.
	 */
	bool shouldDoFinalNotify(MM_EnvironmentStandard *env);

protected:
	virtual void setupForGC(MM_EnvironmentBase *env);
	virtual void masterSetupForGC(MM_EnvironmentStandard *env);
	virtual void workerSetupForGC(MM_EnvironmentStandard *env);

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

	/**
	 * process LargeAllocateStats before GC
	 * merge largeObjectAllocateStats in nursery space(no averaging)
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsBeforeGC(MM_EnvironmentBase *env);

	/**
	 * process LargeAllocateStats after GC
	 * merge and average largeObjectAllocateStats in tenure space
	 * merge FreeEntry AllocateStats in tenure space
	 * estimate Fragmentation
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsAfterGC(MM_EnvironmentBase *env);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * Perform partial initialization if Garbage Collection is called earlier then GC Master Thread is activated
	 * @param env Master GC thread.
	 */
	virtual MM_ConcurrentPhaseStatsBase *getConcurrentPhaseStats() { return &_concurrentPhaseStats; }
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	
public:

	static MM_Scavenger *newInstance(MM_EnvironmentStandard *env, MM_HeapRegionManager *regionManager);
	virtual void kill(MM_EnvironmentBase *env);

	MM_ScavengerDelegate* getDelegate() { return &_delegate; }

	/* Read Barrier Verifier specific methods */
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	virtual void scavenger_poisonSlots(MM_EnvironmentBase *env);
	virtual void scavenger_healSlots(MM_EnvironmentBase *env);
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase* extensions);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/* API for interaction with MasterGCTread */
	virtual bool isConcurrentWorkAvailable(MM_EnvironmentBase *env);
	virtual void preConcurrentInitializeStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats);
	virtual uintptr_t masterThreadConcurrentCollect(MM_EnvironmentBase *env);
	virtual void postConcurrentUpdateStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats, UDATA bytesConcurrentlyScanned);

	/* master thread specific methods */
	bool scavengeIncremental(MM_EnvironmentBase *env);
	bool scavengeInit(MM_EnvironmentBase *env);
	bool scavengeRoots(MM_EnvironmentBase *env);
	bool scavengeScan(MM_EnvironmentBase *env);
	bool scavengeComplete(MM_EnvironmentBase *env);
	
	/* mutator thread specific methods */
	void mutatorSetupForGC(MM_EnvironmentBase *env);
	
	/* methods used by either mutator or GC threads */
	/**
	 * All open copy caches (even if not full) are pushed onto scan queue. Unused memory is abondoned.
	 * @param currentEnvBase Current thread in which context this is invoked from. Could be either GC or mutator thread.
	 * @param targetEnvBase  Thread for which copy caches are to be released. Could be either GC or mutator thread.
	 * @param flushCaches If true, really push caches to scan queue, otherwise just deactivate them for possible near future use
	 * @param final If true (typically at the end of a cycle), abandon TLH remainders, too. Otherwise keep them for possible future copy cache refresh.
	 */
	void threadReleaseCaches(MM_EnvironmentBase *currentEnvBase, MM_EnvironmentBase *targetEnvBase, bool flushCaches, bool final);
	
	/**
	 * trigger STW phase (either start or end) of a Concurrent Scavenger Cycle 
	 */ 
	void triggerConcurrentScavengerTransition(MM_EnvironmentBase *envBase, MM_AllocateDescription *allocDescription);
	/**
	 * complete (trigger end) of a Concurrent Scavenger Cycle
	 */
	void completeConcurrentCycle(MM_EnvironmentBase *envBase);

	/* worker thread */
	void workThreadProcessRoots(MM_EnvironmentStandard *env);
	void workThreadScan(MM_EnvironmentStandard *env);
	void workThreadComplete(MM_EnvironmentStandard *env);

	/**
	 * GC threads may call it to determine if running in a context of 
	 * concurrent or STW task
	 */
	bool isCurrentPhaseConcurrent() {
		return _currentPhaseConcurrent;
	}
	
	/**
	 * True if CS cycle is active at any point (STW or concurrent task active,
	 * or even short gaps between STW and concurrent tasks)
	 */
	bool isConcurrentCycleInProgress() {
		return concurrent_phase_idle != _concurrentPhase;
	}
	
	/* TODO: remove once downstream projects start using isConcurrentCycleInProgress/isCurrentPhaseConcurrent */
	bool isConcurrentInProgress() {
		return concurrent_phase_idle != _concurrentPhase;
	}
	
	bool isMutatorThreadInSyncWithCycle(MM_EnvironmentBase *env) {
		return (env->_concurrentScavengerSwitchCount == _concurrentScavengerSwitchCount);
	}

	/**
	 * Enabled/disable approriate thread local resources when starting or finishing Concurrent Scavenger Cycle
	 */ 
	void switchConcurrentForThread(MM_EnvironmentBase *env);	
	
	void reportConcurrentScavengeStart(MM_EnvironmentStandard *env);
	void reportConcurrentScavengeEnd(MM_EnvironmentStandard *env);
	
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	/**
	 * Determine whether the object pointer is found within the heap proper.
	 * @return Boolean indicating if the object pointer is within the heap boundaries.
	 */
	MMINLINE bool
	isHeapObject(omrobjectptr_t objectPtr)
	{
		return ((_heapBase <= (uint8_t *)objectPtr) && (_heapTop > (uint8_t *)objectPtr));
	}

	MMINLINE bool
	isObjectInNewSpace(omrobjectptr_t objectPtr)
	{
		return ((void *)objectPtr >= _survivorSpaceBase) && ((void *)objectPtr < _survivorSpaceTop);
	}

	MMINLINE bool
	isObjectInEvacuateMemory(omrobjectptr_t objectPtr)
	{
		/* check if the object in cached allocate (from GC perspective, evacuate) ranges */
		return ((void *)objectPtr >= _evacuateSpaceBase) && ((void *)objectPtr < _evacuateSpaceTop);
	}
	
	MMINLINE void *
	getEvacuateBase()
	{
		return _evacuateSpaceBase;
	}
	
	MMINLINE void *
	getEvacuateTop()
	{
		return _evacuateSpaceTop;
	}
	
	MMINLINE void *
	getSurvivorBase()
	{
		return _survivorSpaceBase;
	}
	
	MMINLINE void *
	getSurvivorTop()
	{
		return _survivorSpaceTop;
	}

	void workThreadGarbageCollect(MM_EnvironmentStandard *env);

	void scavengeRememberedSet(MM_EnvironmentStandard *env);

	void pruneRememberedSet(MM_EnvironmentStandard *env);

	virtual uintptr_t getVMStateID();

	bool completeScan(MM_EnvironmentStandard *env);

	/**
	 * Attempt to add the specified object to the current thread's remembered set fragment.
	 * Grow the remembered set if necessary and, if that fails, overflow.
	 * The object must already have its remembered bits set.
	 *
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to remember
	 */
	void addToRememberedSetFragment(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/**
	 * Provide public (out-of-line) access to private (inline) copyAndForward(), copy() for client language
	 * runtime. Slot holding reference will be updated with new address for referent on return.
	 * @param[in] env Environment pointer for calling thread
	 * @param[in/out] slotPtr Pointer to slot holding reference to object to be copied and forwarded
	 */
	bool copyObjectSlot(MM_EnvironmentStandard *env, volatile omrobjectptr_t *slotPtr);
	bool copyObjectSlot(MM_EnvironmentStandard *env, GC_SlotObject* slotObject);
	omrobjectptr_t copyObject(MM_EnvironmentStandard *env, MM_ForwardedHeader* forwardedHeader);

	/**
	 * Update the given slot to point at the new location of the object, after copying
	 * the object if it was not already.
	 * Attempt to copy (either flip or tenure) the object and install a forwarding
	 * pointer at the new location. The object may have already been copied. In
	 * either case, update the slot to point at the new location of the object.
	 *
	 * @param env[in] the current thread
	 * @param objectPtrIndirect[in/out] the thread or stack slot to be scavenged
	 */
	void copyAndForwardThreadSlot(MM_EnvironmentStandard *env, omrobjectptr_t *objectPtrIndirect);

	/**
	 * This function is called at the end of scavenging if any stack- (or thread-) referenced
	 * objects were tenured during the scavenge. It is called by the RootScanner on each thread
	 * or stack slot.
	 *
	 * @param env[in] the current thread
	 * @param objectPtrIndirect[in] the slot to process
	 */
	void rescanThreadSlot(MM_EnvironmentStandard *env, omrobjectptr_t *objectPtrIndirect);

	uintptr_t getArraySplitAmount(MM_EnvironmentStandard *env, uintptr_t sizeInElements);

	void backOutObjectScan(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	/**
	 * BackOutFixSlotWithoutCompression implementation
	 * @param slotPrt input slot
	 */
	bool backOutFixSlotWithoutCompression(volatile omrobjectptr_t *slotPtr);
	virtual void *createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool);
	virtual void deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState);

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	virtual void collectorExpanded(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize);
	virtual bool canCollectorExpand(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize);
	virtual uintptr_t getCollectorExpandSize(MM_EnvironmentBase *env);

	MM_Scavenger(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager) :
		MM_Collector()
		, _delegate(env)
		, _objectAlignmentInBytes(env->getObjectAlignmentInBytes())
		, _isRememberedSetInOverflowAtTheBeginning(false)
		, _extensions(env->getExtensions())
		, _dispatcher(_extensions->dispatcher)
		, _doneIndex(0)
		, _activeSubSpace(NULL)
		, _evacuateMemorySubSpace(NULL)
		, _survivorMemorySubSpace(NULL)
		, _tenureMemorySubSpace(NULL)
		, _evacuateSpaceBase(NULL)
		, _evacuateSpaceTop(NULL)
		, _survivorSpaceBase(NULL)
		, _survivorSpaceTop(NULL)
		, _tenureMask(0)
		, _expandFailed(false)
		, _failedTenureThresholdReached(false)
		, _countSinceForcingGlobalGC(0)
		, _expandTenureOnFailedAllocate(true)
		, _minTenureFailureSize(UDATA_MAX)
		, _minSemiSpaceFailureSize(UDATA_MAX)
		, _cycleState()
		, _collectionStatistics()
		, _cachedEntryCount(0)
		, _cachesPerThread(0)
		, _scanCacheMonitor(NULL)
		, _freeCacheMonitor(NULL)
		, _waitingCountAliasThreshold(0)
		, _waitingCount(0)
		, _cacheLineAlignment(0)
#if !defined(OMR_GC_CONCURRENT_SCAVENGER)
		, _rescanThreadsForRememberedObjects(false)
#endif
		, _backOutDoneIndex(0)
		, _heapBase(NULL)
		, _heapTop(NULL)
		, _regionManager(regionManager)
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		, _masterGCThread(env)
		, _concurrentPhase(concurrent_phase_idle)
		, _currentPhaseConcurrent(false)
		, _concurrentScavengerSwitchCount(0)
		, _shouldYield(false)
		, _concurrentPhaseStats()
#endif /* #if defined(OMR_GC_CONCURRENT_SCAVENGER) */

		, _omrVM(env->getOmrVM())
	{
		_typeId = __FUNCTION__;
		_cycleType = OMR_GC_CYCLE_TYPE_SCAVENGE;
	}
};

#endif /* OMR_GC_MODRON_SCAVENGER */
#endif /* SCAVENGER_HPP_ */
