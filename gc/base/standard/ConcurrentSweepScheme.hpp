/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTSWEEPSCHEME_HPP_)
#define CONCURRENTSWEEPSCHEME_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#if defined(OMR_GC_CONCURRENT_SWEEP)

class MM_AllocateDescription;
class MM_GlobalCollector;
class MM_MemoryPool;
class MM_MemoryPoolAddressOrderedList;
class MM_ConcurrentSweepCompleteSweepTask;
class MM_ConcurrentSweepFindMinimumSizeFreeTask;
class MM_ConcurrentSweepPoolState;

#include "EnvironmentStandard.hpp"
#include "ParallelSweepScheme.hpp"

#include "ConcurrentSweepStats.hpp"
#include "LightweightNonReentrantLock.hpp"

/**
 * Chunk states when being processed by concurrent sweep.
 * These states should really be declared with the Chunk definition in the end (and
 * made a typedef).
 */
enum {
	modron_concurrentsweep_state_unprocessed = 0,  /**< has not been processed by the sweeper */
	modron_concurrentsweep_state_busy_sweep,  /**< in the process of being swept */
	modron_concurrentsweep_state_swept,  /**< has been swept */
	modron_concurrentsweep_state_busy_connect,  /**< in the process of being connected to the free list */
	modron_concurrentsweep_state_connected  /**< has been connected to the free list */
};

/**
 * Work in progress concurrent sweep implementation.
 * 
 * @todo proper class description once we get this fleshed out.
 * 
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentSweepScheme : public MM_ParallelSweepScheme
{
	/*
	 * Data members
	 */
private:
	volatile UDATA _concurrentSweepingThreadCount;  /**< Number of threads currently sweeping concurrently */
	omrthread_monitor_t _completeSweepingConcurrentlyLock;  /**< Lock to gain access to number of threads completing sweep concurrently */
protected:
	MM_GlobalCollector *_collector;  /**< Global collector to which the receiver is associated */
	MM_ConcurrentSweepStats _stats;  /**< Runtime statistics for execution and debugging (e.g., verbose gc) */
public:
	
	/*
	 * Function members
	 */
private:
	void initializeChunks(MM_EnvironmentBase *env);

	void verifyFreeList(MM_EnvironmentStandard *env, MM_HeapLinkedFreeHeader *freeListHead);

	void checkRestrictions(MM_EnvironmentBase *env);

	MM_ParallelSweepChunk *getNextSweepChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepPoolState);
	MM_ParallelSweepChunk *getPreviousSweepChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState);
	bool incrementalSweepChunk(MM_EnvironmentStandard *env, MM_ParallelSweepChunk *chunk);
	UDATA sweepPool(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool, UDATA chunkTax);
	bool sweepNextAvailableChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepPoolState);
	bool sweepPreviousAvailableChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepPoolState);
	bool concurrentSweepNextAvailableChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState);
	void propagateChunkProjections(MM_EnvironmentBase *envModron, MM_ParallelSweepChunk *startingChunkToPropagate);
	void abandonOverlappedChunks(MM_EnvironmentBase *envModron, MM_ParallelSweepChunk *startingChunk, bool isFirstChunkInSubpool);
	void walkChunkForOverlappingDeadSpace(MM_EnvironmentBase *envModron, MM_ParallelSweepChunk *currentChunk, void *walkStart);

	bool increaseActiveSweepingThreadCount(MM_EnvironmentBase *envModron, bool completeSweeping);
	bool decreaseActiveSweepingThreadCount(MM_EnvironmentBase *envModron, bool completeSweeping);

	MM_ParallelSweepChunk *getNextConnectChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepPoolState);
	void initializeStateForConnections(
		MM_EnvironmentBase *envModron,
		MM_MemoryPoolAddressOrderedList *memoryPool,
		MM_ConcurrentSweepPoolState *sweepState,
		MM_ParallelSweepChunk *chunk);
	bool incrementalConnectChunk(
		MM_EnvironmentStandard *env,
		MM_ParallelSweepChunk *chunk,
		MM_ConcurrentSweepPoolState *sweepState,
		MM_MemoryPoolAddressOrderedList *memoryPool);

	void workThreadCompleteSweep(MM_EnvironmentBase *env);

	void workThreadFindMinimumSizeFreeEntry(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace, UDATA minimumFreeSize);

	bool isConcurrentSweepCandidate(MM_MemorySubSpace *memorySubSpace);

	void preConnectChunk(MM_EnvironmentBase* env, MM_ParallelSweepChunk *chunk);
	void postConnectChunk(MM_EnvironmentBase* env, MM_ParallelSweepChunk *chunk);

	UDATA calculateTax(MM_EnvironmentBase *envModron, UDATA allocationSize);

	void reportConcurrentlyCompletedSweepPhase(MM_EnvironmentBase *envModron);
	void reportCompletedConcurrentSweep(MM_EnvironmentBase *envModron, SweepCompletionReason reason);

	static void hookMemoryPoolNew(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
	static void hookMemoryPoolKill(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

	void calculateApproximateFree(MM_EnvironmentBase* env, MM_MemoryPool *memoryPool, MM_ConcurrentSweepPoolState *sweepState);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual void setupForSweep(MM_EnvironmentBase *env);

	virtual void connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk);

	virtual void flushAllFinalChunks(MM_EnvironmentBase *env);

public:
	static MM_ConcurrentSweepScheme *newInstance(MM_EnvironmentBase *env, MM_GlobalCollector *collector);

	/**
 	* Request to create sweepPoolState class for pool
 	* @param  memoryPool memory pool to attach sweep state to
 	* @return pointer to created class
 	*/
	virtual void *createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool);

	bool isConcurrentSweepActive() { return _stats.isConcurrentSweepActive(); }

	virtual void sweep(MM_EnvironmentBase *env);
	virtual void completeSweep(MM_EnvironmentBase* env, SweepCompletionReason reason);
	virtual bool sweepForMinimumSize(MM_EnvironmentBase *env, MM_MemorySubSpace *baseMemorySubSpace, MM_AllocateDescription *allocateDescription);
	bool completeSweepingConcurrently(MM_EnvironmentBase *envModron);

	virtual bool replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, UDATA size);
	void payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace,  MM_AllocateDescription *allocDescriptionn);

	MM_ConcurrentSweepScheme(MM_EnvironmentBase *env, MM_GlobalCollector *collector)
		: MM_ParallelSweepScheme(env)
		, _concurrentSweepingThreadCount(0)
		, _completeSweepingConcurrentlyLock(NULL)
		, _collector(collector)
		, _stats()
	{
		_typeId = __FUNCTION__;
	}
	
	/*
	 * Friends
	 */
	friend class MM_ConcurrentSweepCompleteSweepTask;
	friend class MM_ConcurrentSweepFindMinimumSizeFreeTask;
};

#endif /* OMR_GC_CONCURRENT_SWEEP */

#endif /* CONCURRENTSWEEPSCHEME_HPP_ */
