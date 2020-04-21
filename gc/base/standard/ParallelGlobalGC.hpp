/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(PARALLELGLOBALGC_HPP_)
#define PARALLELGLOBALGC_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "CollectionStatisticsStandard.hpp"
#if defined(OMR_GC_CONCURRENT_SWEEP)
#include "ConcurrentSweepScheme.hpp"
#endif /* defined(OMR_GC_CONCURRENT_SWEEP) */
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "MarkMap.hpp"
#include "MarkingScheme.hpp"
#include "ParallelHeapWalker.hpp"
#include "ParallelSweepScheme.hpp"

/* Declaration of "C" style Read Barrier Verifier specific methods */
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
void poisonReferenceSlots(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData);
void poisonReferenceSlot(MM_EnvironmentBase *env, GC_SlotObject *slotObject);
void healReferenceSlot(MM_EnvironmentBase *env, GC_SlotObject *slotObject);
void healReferenceSlots(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData);
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

class MM_CollectionStatisticsStandard;
class MM_CompactScheme;
class MM_Dispatcher;
class MM_MarkingScheme;
class MM_MemorySubSpace;

/**
 * Multi-threaded mark and sweep global collector.
 */
class MM_ParallelGlobalGC : public MM_GlobalCollector
{
/*
 * Data members
 */
protected:
	MM_GCExtensionsBase *_extensions;

private:
	OMRPortLibrary *_portLibrary;

#if defined(OMR_GC_MODRON_COMPACTION)
	MM_CompactScheme *_compactScheme;
	bool _compactThisCycle;		/**< keep a decision should compact run this cycle */
#endif /* OMR_GC_MODRON_COMPACTION */

protected:
	MM_MarkingScheme *_markingScheme;
	MM_ParallelSweepScheme *_sweepScheme;
	MM_ParallelHeapWalker *_heapWalker;
	MM_Dispatcher *_dispatcher;
	MM_CycleState _cycleState;  /**< Embedded cycle state to be used as the master cycle state for GC activity */
	MM_CollectionStatisticsStandard _collectionStatistics; /** Common collect stats (memory, time etc.) */
	bool _fixHeapForWalkCompleted;
public:
	
/*
 * Function members
 */
private:
	/**
	 *  main method to provide sweep
	 */
	void sweep(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool rebuildMarkBits);

	/**
	 * Answer whether a complete rebuild of freelist should be performed during the sweep phase.
	 * @return true if sweep work should be completed, false otherwise.
	 * Also returns the reason why completion of concurrent sweep is required 
	 */
	bool completeFreelistRebuildRequired(MM_EnvironmentBase *env, SweepCompletionReason *reason);

#if defined(OMR_GC_MODRON_COMPACTION)
	/**
	 * Determine whether a compaction is required or not.
	 * The decision is based on the setting of the following 
	 * command line options:
	 *   -Xcompactgc 			compact on all GC's, AF or system.
	 *   -Xnocompactgc			do NOT compact on any GC's
	 * 
	 * 	If either of the above options is specified then they alone control
	 * 	whether we compact or not. All other options are ignored and no
	 * 	compaction is performed based on any triggers. The default action 
	 * 	if neither of above are specified is to compact when it would be 
	 * 	benifical to the running application based on a series of triggers.
	 * 
	 * 	The current compaction triggers are:
	 *  				
	 * 	- less than 4% of free space in tenured area 
	 * 	- less than 128KB of free sapce in tenured area
	 * 	- average TLH size drops below threshold; currently 1024 bytes
	 *  - If -Xgc:compactToSatisfyAllocate is set, we ONLY compact if the
	 *    allocate could not be satisfied
	 * 
	 * Further the user can control whether a compaction is performed 
	 * on a system GC or not using the following command line options:
	 *   -Xcompactexplicitgc		compact on all system GC's
	 *   -Xnocompactexplicitgc	do not compact on system GC's 
	 * 
	 * If either of above options is specified we will still compact 
	 * on non-system GCs if any of the compaction triggers are met.
	 * 
	 * @param env[in] The master thread of this collection
	 * @param allocDescription[in] The allocation description which triggered the GC (NULL if this collection was not triggered by an allocation)
	 * @param activeSubspaceMaxExpansionInSpace[in] The maximum expansion size of the active subspace
	 * @param gcCode[in] a code describing the type of GC cycle
	 *
	 * @return true if a compaction is required, false otherwise
	 */
	bool shouldCompactThisCycle(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t activeSubspaceMaxExpansionInSpace, MM_GCCode gcCode);
	/**
	 * Determine if a compact is required to aid contraction.
	 * A heap contraction is due so decide whether a compaction would be
	 * beneficial before we attempt to contract the heap.
	 * @return true if a compaction is required, false otherwise.
	 */
	bool compactRequiredBeforeHeapContraction(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t contractionSize);
#endif /* OMR_GC_MODRON_COMPACTION */

	/**
	 *	Mark operation - Complete Mark operation
	 *	Use for compatibility with old collectors
	 *	@param initMarkMap instruct should mark map be initialized (might be already partially done like in conrurrentGC) 
	 */
	void markAll(MM_EnvironmentBase *env, bool initMarkMap);
	
	/**
	 *	Main call for Sweep operation
	 *	Start of sweep for concurrentGC, full sweep for other cases
	 */
	void masterThreadSweepStart(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
	
	/**
	 * 	Complete Sweep operation for concurrentGC, empty for all other cases 
	 */
	void masterThreadSweepComplete(MM_EnvironmentBase *env, SweepCompletionReason reason);
	
#if defined(OMR_GC_MODRON_COMPACTION)
	/**
	 *	Main call for Compact operation
	 *	@param rebuildMarkBits rebuild of mark bits required
	 */
	void masterThreadCompact(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool rebuildMarkBits);
#endif /* OMR_GC_MODRON_COMPACTION */

	void masterThreadRestartAllocationCaches(MM_EnvironmentBase *env);

	/**
	 *	Initializations before GC cycle 
	 */
	void setupBeforeGC(MM_EnvironmentBase *env);
	
	/**
	 *	Last few settings to complete GC cycle 
	 */
	void cleanupAfterGC(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	/**
	 * redistribute free memory in tenure after global collection (move free memory from LOA to SOA)
	 */
	void tenureMemoryPoolPostCollect(MM_EnvironmentBase *env);
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	virtual void masterThreadGarbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool initMarkMap, bool rebuildMarkBits);

	virtual void setupForGC(MM_EnvironmentBase *env);
	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

	/**
	 * Update tuning statistics at end of a concurrent cycle.
	 *  need this function here empty, a real implementation is in ConcurrentGC
	 */
	virtual void updateTuningStatistics(MM_EnvironmentBase *env) { }

	void reportGCCycleStart(MM_EnvironmentBase *env);
	void reportGCCycleEnd(MM_EnvironmentBase *env);
	void reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env);
	void reportGCStart(MM_EnvironmentBase *env);
	void reportGCEnd(MM_EnvironmentBase *env);
	void reportGlobalGCIncrementStart(MM_EnvironmentBase *env);
	void reportGlobalGCIncrementEnd(MM_EnvironmentBase *env);
	void reportGlobalGCCollectComplete(MM_EnvironmentBase *env);
	void reportGCIncrementStart(MM_EnvironmentBase *env);
	void reportGCIncrementEnd(MM_EnvironmentBase *env);
	void reportMarkStart(MM_EnvironmentBase *env);
	void reportMarkEnd(MM_EnvironmentBase *env);
	void reportSweepStart(MM_EnvironmentBase *env);
	void reportSweepEnd(MM_EnvironmentBase *env);

#if defined(OMR_GC_MODRON_COMPACTION)
	void reportCompactStart(MM_EnvironmentBase *env);
	void reportCompactEnd(MM_EnvironmentBase *env);
#endif /* OMR_GC_MODRON_COMPACTION */

	/**
	 * process LargeAllocateStats before GC
	 * merge and average largeObjectAllocateStats in tenure space
	 * merge largeObjectAllocateStats in nursery space(no averaging)
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsBeforeGC(MM_EnvironmentBase *env);

	/**
	 * process LargeAllocateStats after Sweep
	 * merge FreeEntry AllocateStats in tenure space
	 * estimate Fragmentation
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsAfterSweep(MM_EnvironmentBase *env);

	/**
	 * compaction would change freeEntries stats, merge FreeEntryAllocateStats after compaction.
	 * currently processLargeAllocateStatsAfterCompact() is same as processLargeAllocateStatsAfterSweep()
	 */
	void processLargeAllocateStatsAfterCompact(MM_EnvironmentBase *env);

	virtual void postMark(MM_EnvironmentBase *env);

	MM_ParallelSweepScheme*
	createSweepScheme(MM_EnvironmentBase *env, MM_GlobalCollector *globalCollector)
	{
		MM_ParallelSweepScheme *sweepScheme = NULL;

#if defined(OMR_GC_CONCURRENT_SWEEP)
		if(_extensions->concurrentSweep) {
			sweepScheme = MM_ConcurrentSweepScheme::newInstance(env, globalCollector);
		} else
#endif /* defined(OMR_GC_CONCURRENT_SWEEP) */
		{
			sweepScheme = MM_ParallelSweepScheme::newInstance(env);
		}

		return sweepScheme;
	}

public:
	static MM_ParallelGlobalGC *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual uintptr_t getVMStateID();

	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase *extensions);

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	void poisonHeap(MM_EnvironmentBase *env);
	void healHeap(MM_EnvironmentBase *env);
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	/**
	 *  Fixes up all unloaded objects so that the heap can be walked and only live objects returned
	 *  @param reason fix heap reason
	 */
	uintptr_t fixHeapForWalk(MM_EnvironmentBase *env, UDATA walkFlags, uintptr_t walkReason, MM_HeapWalkerObjectFunc walkFunction);
	MM_HeapWalker *getHeapWalker() { return _heapWalker; }
	virtual void prepareHeapForWalk(MM_EnvironmentBase *env);

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	virtual void heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress);

	virtual	uint32_t getGCTimePercentage(MM_EnvironmentBase *env);

	/**
 	* Request to create sweepPoolState class for pool
 	* @param  memoryPool memory pool to attach sweep state to
 	* @return pointer to created class
 	*/
	virtual void *createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool);

	/**
 	* Request to destroy sweepPoolState class for pool
 	* @param  sweepPoolState class to destroy
 	*/
	virtual void deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState);

	virtual bool isMarked(void *objectPtr);

	/**
	 * Return reference to Marking Scheme
	 */
	MM_MarkingScheme *getMarkingScheme()
	{
		return _markingScheme;
	}
	
#if defined(OMR_GC_MODRON_COMPACTION)
	MM_CompactScheme *
	getCompactScheme(MM_EnvironmentBase *env) {
		return _compactScheme;
	}
#endif /* OMR_GC_MODRON_COMPACTION */

	virtual void completeExternalConcurrentCycle(MM_EnvironmentBase *env);
	/**
	 * Notify any (concurrent) collector (like Concurrent Scavenger) that might block and hold VM access
	 * that an Exclusive VM Access is to be requested so that VM access can be released
	 */	
	virtual void notifyAcquireExclusiveVMAccess(MM_EnvironmentBase *env);

	MM_ParallelGlobalGC(MM_EnvironmentBase *env)
		: MM_GlobalCollector()
		, _extensions(MM_GCExtensionsBase::getExtensions(env->getOmrVM()))
		, _portLibrary(env->getPortLibrary())
#if defined(OMR_GC_MODRON_COMPACTION)
		, _compactScheme(NULL)
		, _compactThisCycle(false)
#endif /* OMR_GC_MODRON_COMPACTION */
		, _markingScheme(NULL)
		, _sweepScheme(NULL)
		, _heapWalker(NULL)
		, _dispatcher(_extensions->dispatcher)
		, _cycleState()
		, _collectionStatistics()
		, _fixHeapForWalkCompleted(false)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* PARALLELGLOBALGC_HPP_ */
