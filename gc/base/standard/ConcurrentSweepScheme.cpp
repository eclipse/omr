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

#include "omrcfg.h"
#include "omrmodroncore.h"
#include "omrthread.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_CONCURRENT_SWEEP)

#include "ConcurrentSweepScheme.hpp"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentGC.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#include "Dispatcher.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapMemoryPoolIterator.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceChildIterator.hpp"
#include "MemorySubSpacePoolIterator.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectModel.hpp"
#include "ParallelSweepChunk.hpp"
#include "ParallelTask.hpp"
#include "SweepHeapSectioning.hpp"
#include "SweepPoolState.hpp"

/* Uncomment to enable debugging */
/* #define CONCURRENT_SWEEP_SCHEME_DEBUG */

#if defined(CONCURRENT_SWEEP_SCHEME_DEBUG)
#define CONSS_DEBUG_OMRPORT_ACCESS_FROM_OMRPORT(x) OMRPORT_ACCESS_FROM_OMRPORT(x)
#define CONSS_DEBUG_PRINTF(x) omrtty_printf x
#else /* CONCURRENT_SWEEP_SCHEME_DEBUG */
#define CONSS_DEBUG_OMRPORT_ACCESS_FROM_OMRPORT(x)
#define CONSS_DEBUG_PRINTF(x) 
#endif /* CONCURRENT_SWEEP_SCHEME_DEBUG */


#define PREVIOUS_FREE_RATIO_INITIAL					((float)0.3)
#define PREVIOUS_FREE_HISTORY_WEIGHT_STARTUP		((float)0.5)
#define PREVIOUS_FREE_HISTORY_WEIGHT_RUNNING		((float)0.8)
#define VM_STARTUP_SWEEP_COUNT 						5

/**
 * Sweep state of a given memory pool.
 * @note Defined privately by MM_ConcurrentSweepScheme
 */
class MM_ConcurrentSweepPoolState : public MM_SweepPoolState
{
	/*
	 * Data members
	 */
private:
	UDATA _sweepCount;							/**< Counter of number of sweep cycles */
	MM_ParallelSweepChunk *_currentSweepChunk;  /**< Next chunk to concurrent sweep in the given memory pool */
	MM_ParallelSweepChunk *_currentSweepChunkReverse; /**< Next chunk to sweep when sweeping high to low in a given memory pool */
	MM_LightweightNonReentrantLock _sweepChunkIteratorLock;  /**< Lock for the next chunk to concurrent sweep list */

	MM_ParallelSweepChunk *_connectCurrentChunk;  /**< Next chunk to concurrent connect in the given memory pool */
	MM_HeapLinkedFreeHeader *_connectNextFreeEntry;  /**< Existing free list entry immediately following the connect range */
	UDATA _connectNextFreeEntrySize;	/**< Existing free list entry size immediately following the connect range */

	MM_ParallelSweepChunk *_currentInitChunk;  /**< Last chunk initialized for concurrent sweep in the given memory pool (used for _next pointer) */

	UDATA _freeMemoryConnected;			/**< Running count of the total free memory connected into the memory pool  since last GC */
	
	float _previousFreeRatio; 			/**< Ratio of free memory connected into the pool from previous GC */
	float _previousFreeHistoryWeight;	/**< Weight to be used when calculating weighted avarage _previousFreeRatio */

	UDATA _heapSizeToConnect;			/**< Total heap size to be connected in this pool */
	UDATA _heapSizeConnected;			/**< Heap size connected into the pool */

	bool _finalFlushed;					/**< Has the pool had its final chunk flushed this cycle */

protected:
public:
	
	/*
	 * Function members
	 */
private:
	virtual bool initialize(MM_EnvironmentBase *env);

protected:
public:
	/**
	 * Free the receiver and all associated resources.
	 * @param pool J9Pool used for allocation 
	 * @param mutex mutex to protect J9Pool operations
	 */
	void kill(MM_EnvironmentBase *env,  J9Pool *pool, omrthread_monitor_t mutex)
	{
		tearDown(env);

		omrthread_monitor_enter(mutex);
		pool_removeElement(pool, this);
		omrthread_monitor_exit(mutex);
	}

	/**
	 * Allocate and initialize a new instance of the receiver.
	 * @param pool J9Pool should be used for allocation
	 * @param mutex mutex to protect J9Pool operations
	 * @param memoryPool memory pool this sweepPoolState should be associated with
	 * @return a new instance of the receiver, or NULL on failure.
	 */
	static MM_ConcurrentSweepPoolState *newInstance(MM_EnvironmentBase *env, J9Pool *pool, omrthread_monitor_t mutex, MM_MemoryPool *memoryPool)
	{
		MM_ConcurrentSweepPoolState *sweepPoolState;
		
		omrthread_monitor_enter(mutex);
		sweepPoolState = (MM_ConcurrentSweepPoolState *)pool_newElement(pool);
		omrthread_monitor_exit(mutex);

		if (sweepPoolState) {
			new(sweepPoolState) MM_ConcurrentSweepPoolState(memoryPool);
			if (!sweepPoolState->initialize(env)) { 
				sweepPoolState->kill(env, pool, mutex);        
				sweepPoolState = NULL;            
			}                                       
		}

		return sweepPoolState;
	}

	/**
	 * Build a MM_ConcurrentSweepPoolState object within the memory supplied
	 * @param memPtr pointer to the memory to build the object within
	 */
	static void create(MM_EnvironmentBase *env, void *memPtr, MM_MemoryPool *memoryPool) {
		MM_ConcurrentSweepPoolState *poolState = (MM_ConcurrentSweepPoolState *) memPtr;
		new(poolState) MM_ConcurrentSweepPoolState(memoryPool);
		poolState->initialize(env);
	}
	
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual void initializeForSweep(MM_EnvironmentBase *env);

	MM_ConcurrentSweepPoolState(MM_MemoryPool *memoryPool) :
		MM_SweepPoolState(memoryPool),
		_sweepCount(0),
		_currentSweepChunk(NULL),
		_currentSweepChunkReverse(NULL),
		_sweepChunkIteratorLock(),
		_connectCurrentChunk(NULL),
		_connectNextFreeEntry(NULL),
		_connectNextFreeEntrySize(0),
		_currentInitChunk(NULL),
		_freeMemoryConnected(0),
		_previousFreeRatio(PREVIOUS_FREE_RATIO_INITIAL),
		_previousFreeHistoryWeight(PREVIOUS_FREE_HISTORY_WEIGHT_STARTUP),
		_heapSizeToConnect(0),
		_heapSizeConnected(0),
		_finalFlushed(false)
	{}

	/*
	 * Friends
	 */
	friend class MM_ConcurrentSweepScheme;
};

/**
 * Initialize the receivers internal structures.
 * @return true on success, false on failure.
 */
bool
MM_ConcurrentSweepPoolState::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if (!MM_SweepPoolState::initialize(env)) {
		return false;
	}

	_sweepChunkIteratorLock.initialize(env, &extensions->lnrlOptions, "MM_ConcurrentSweepPoolState:_sweepChunkIteratorLock");

	return true;
}

/**
 * Free any structures or resources associated to the receiver.
 */
void
MM_ConcurrentSweepPoolState::tearDown(MM_EnvironmentBase *env)
{
	_sweepChunkIteratorLock.tearDown();	
}

/**
 * @copydoc MM_SweepPoolState::initializeForSweep(MM_EnvironmentBase *)
 * @todo The approximation code needs to be fixed
 */
void
MM_ConcurrentSweepPoolState::initializeForSweep(MM_EnvironmentBase *env)
{
	MM_SweepPoolState::initializeForSweep(env);
	
	/* Record the free to total free memory ratio for the previous cycle 
	 * (used for approximation of free memory remaining). Can't do this on
	 * very first cycle as required statistics not available.
	 */
	 if (++_sweepCount > 1) {
		if (_sweepCount == VM_STARTUP_SWEEP_COUNT ) { 
			/* Adjust weight now we have had a few collections worth of sweeps */
			_previousFreeHistoryWeight = PREVIOUS_FREE_HISTORY_WEIGHT_RUNNING;
		}	
		
	 	float newFreeRatio =  _freeMemoryConnected == 0 ? 0: (float)_freeMemoryConnected / (float)_heapSizeConnected;
		_previousFreeRatio = MM_Math::weightedAverage(_previousFreeRatio,  newFreeRatio, _previousFreeHistoryWeight);
	 }	 
	 
	_currentSweepChunk = NULL;
	_connectPreviousChunk = NULL;
	_connectCurrentChunk = NULL;
	_connectNextFreeEntry = NULL;
	_connectNextFreeEntrySize = 0;
	_currentInitChunk = NULL;
	_freeMemoryConnected = 0;
	_heapSizeToConnect = 0;
	_heapSizeConnected = 0;
	_finalFlushed = false;
}

/**
 * Parallel Task to complete the in progress concurrent sweep.
 * Skeletal task object that calls back to the sweeper to complete any pending work
 * for concurrent sweep.
 * 
 * @note Defined privately by MM_ConcurrentSweepScheme
 */
class MM_ConcurrentSweepCompleteSweepTask : public MM_ParallelSweepTask
{
private:
protected:
public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_CONCURRENT_SWEEP_COMPLETE_SWEEP; };
	
	virtual void run(MM_EnvironmentBase *env);

	MM_ConcurrentSweepCompleteSweepTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ConcurrentSweepScheme *concurrentSweepScheme) :
		MM_ParallelSweepTask(env, dispatcher, concurrentSweepScheme)
	{}
};

/**
 * Complete any sweep work remaining.
 * Callback to the sweeper to complete sweep work.  No true functionality.
 */
void
MM_ConcurrentSweepCompleteSweepTask::run(MM_EnvironmentBase *env)
{
	((MM_ConcurrentSweepScheme *)_sweepScheme)->workThreadCompleteSweep(env);
}

/**
 * Parallel task to sweep and connect sufficient memory to find an entry of at least a particular size.
 * Skeletal task object.
 * 
 * @note Defined privately by MM_ConcurrentSweepScheme.
 */
class MM_ConcurrentSweepFindMinimumSizeFreeTask : public MM_ParallelSweepTask
{
	/*
	 * Data members
	 */
private:
	MM_MemorySubSpace *_memorySubSpace;  /**< Memory subspace to find minimum free sized entry in */
	UDATA _minimumFreeSize;  /**< The minimum sized free entry to find */

protected:
public:
	volatile bool _foundMinimumSizeFreeEntry;
	volatile bool _foundLiveObject;

	/*
	 * Function members
	 */
private:
protected:
public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_CONCURRENT_SWEEP_FIND_MINIMUM_SIZE_FREE; }
	
	virtual void run(MM_EnvironmentBase *env);

	MM_ConcurrentSweepFindMinimumSizeFreeTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ConcurrentSweepScheme *concurrentSweepScheme, MM_MemorySubSpace *memorySubSpace, UDATA minimumFreeSize)
		: MM_ParallelSweepTask(env, dispatcher, concurrentSweepScheme)
		, _memorySubSpace(memorySubSpace)
		, _minimumFreeSize(minimumFreeSize)
		, _foundMinimumSizeFreeEntry(false)
		, _foundLiveObject(false)
		{}
};		

/**
 * Sweep sufficient data to find a free entry of minimum size.
 * Callback to the sweeper to find the free entry.  No true functionality.
 */
void
MM_ConcurrentSweepFindMinimumSizeFreeTask::run(MM_EnvironmentBase *env)
{
	((MM_ConcurrentSweepScheme *)_sweepScheme)->workThreadFindMinimumSizeFreeEntry(env, _memorySubSpace, _minimumFreeSize);
}

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_ConcurrentSweepScheme *
MM_ConcurrentSweepScheme::newInstance(MM_EnvironmentBase *env, MM_GlobalCollector *collector)
{
	MM_ConcurrentSweepScheme *sweepScheme;
	
	sweepScheme = (MM_ConcurrentSweepScheme *)env->getForge()->allocate(sizeof(MM_ConcurrentSweepScheme), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sweepScheme) {
		new(sweepScheme) MM_ConcurrentSweepScheme(env, collector);
		if (!sweepScheme->initialize(env)) { 
        	sweepScheme->kill(env);        
        	sweepScheme = NULL;            
		}                                       
	}

	return sweepScheme;
}

/**
 * Initialize any internal structures.
 * @return true if initialization is successful, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::initialize(MM_EnvironmentBase *env)
{
	if(!MM_ParallelSweepScheme::initialize(env)) {
		return false;
	}

	if(omrthread_monitor_init_with_name(&_completeSweepingConcurrentlyLock, 0, "MM_ConcurrentSweepScheme::completeSweepingConcurrentlyLock")) {
		return false;
	}

	return true;
}

/**
 * Tear down internal structures.
 */
void
MM_ConcurrentSweepScheme::tearDown(MM_EnvironmentBase *env)
{
	MM_ParallelSweepScheme::tearDown(env);

	if (NULL != _completeSweepingConcurrentlyLock) {
		omrthread_monitor_destroy(_completeSweepingConcurrentlyLock);
		_completeSweepingConcurrentlyLock = NULL;
	}	
}

/**
 * Request to create sweepPoolState class for pool
 * @param  memoryPool memory pool to attach sweep state to
 * @return pointer to created class
 */
void *
MM_ConcurrentSweepScheme::createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	omrthread_monitor_enter(_mutexSweepPoolState);
	if (NULL == _poolSweepPoolState) {
		_poolSweepPoolState = pool_new(sizeof(MM_ConcurrentSweepPoolState), 0, 2 * sizeof(UDATA), 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(OMRPORTLIB));
		if (NULL == _poolSweepPoolState) {
			omrthread_monitor_exit(_mutexSweepPoolState);
			return NULL;
		}
	}
	omrthread_monitor_exit(_mutexSweepPoolState);
	
	return MM_ConcurrentSweepPoolState::newInstance(env, _poolSweepPoolState, _mutexSweepPoolState, memoryPool);
}

/**
 * Report that the sweep phase has been completed concurrently (not the connect phase).
 */
void
MM_ConcurrentSweepScheme::reportConcurrentlyCompletedSweepPhase(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	
	Trc_MM_ConcurrentlyCompletedSweepPhase(env->getLanguageVMThread(), _stats._concurrentCompleteSweepBytesSwept);
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENTLY_COMPLETED_SWEEP_PHASE(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENTLY_COMPLETED_SWEEP_PHASE,
		omrtime_hires_delta(_stats._concurrentCompleteSweepTimeStart, _stats._concurrentCompleteSweepTimeEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS),
		_stats._concurrentCompleteSweepBytesSwept);
}

/**
 * Report that the entire sweep has been completed (sweep + connect).
 */
void
MM_ConcurrentSweepScheme::reportCompletedConcurrentSweep(MM_EnvironmentBase *env, SweepCompletionReason reason)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	
	Trc_MM_CompletedConcurrentSweep(env->getLanguageVMThread(), _stats._completeConnectPhaseBytesConnected);
	TRIGGER_J9HOOK_MM_PRIVATE_COMPLETED_CONCURRENT_SWEEP(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_COMPLETED_CONCURRENT_SWEEP,
		omrtime_hires_delta(_stats._completeSweepPhaseTimeStart, _stats._completeSweepPhaseTimeEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS),
		_stats._completeSweepPhaseBytesSwept,
		omrtime_hires_delta(_stats._completeConnectPhaseTimeStart, _stats._completeConnectPhaseTimeEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS),
		_stats._completeConnectPhaseBytesConnected,
		reason);
}

/**
 * Determine whether a particular subspace is concurrent sweepable.
 * The current criterea for sweeping a memory subspace is that it be a leaf subspace, that it is
 * active, that it have a memory pool, and that it be responsible for heap memory of type MEMORY_TYPE_OLD
 * (ie: non-nursery).
 * @return true if the subspace is a concurrent sweepable candidate, false otherwise.
 */
MMINLINE bool
MM_ConcurrentSweepScheme::isConcurrentSweepCandidate(MM_MemorySubSpace *memorySubSpace)
{
	return
		memorySubSpace->isLeafSubSpace()
		&& memorySubSpace->isActive()
		&& (NULL != memorySubSpace->getMemoryPool())
		&& (MEMORY_TYPE_OLD == memorySubSpace->getTypeFlags());
}

/**
 * Prepare the chunk list for sweeping.
 * Given the current state of the heap, prepare the chunk list such that all entries are assign
 * a range of memory (and all memory has a chunk associated to it), sectioning the heap based on
 * the configuration, as well as configuration rules.
 * @note this routine is slightly different that MM_ParallelSweepScheme::initializeChunks(MM_EnvironmentStandard *env)
 * because it represents the work for a concurrent sweep, not an STW sweep.
 * @todo the forward and backward linkage (but based on pool type) could probably be folded back into the
 * main chunk initialization code.
 */
void
MM_ConcurrentSweepScheme::initializeChunks(MM_EnvironmentBase *env)
{
	MM_ParallelSweepChunk *chunk;
	UDATA totalChunkCount;

	Assert_MM_true(0 == _stats._totalChunkCount);

	/* Reset and reassign all chunks to heap memory */
	totalChunkCount = _sweepHeapSectioning->reassignChunks(env);

	/* Update stats */	
	_stats._totalChunkCount = totalChunkCount;
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	env->_sweepStats.sweepChunksTotal = _stats._totalChunkCount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/* Concurrent sweep requires some extra linkage and starting data in order to function.  Walk
	 * the chunk list collecting and building this information.
	 */
	MM_SweepHeapSectioningIterator sectioningIterator(_sweepHeapSectioning);	
	while(0 != totalChunkCount--) {
		chunk = sectioningIterator.nextChunk();
		MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(chunk->memoryPool); /* sweep state for the current pool */
		
		/* Assign the head of the concurrent sweep lists if this is the first chunk for this subspace */
		if(NULL == sweepState->_currentSweepChunk) {
			sweepState->_currentSweepChunk = chunk;
			sweepState->_connectCurrentChunk = chunk;
		}
		
		/* Connect the current chunk into the linked list associated with the memory pool */
		if(NULL != sweepState->_currentInitChunk) {
			sweepState->_currentInitChunk->_nextChunk = chunk;
			Assert_MM_true(sweepState->_currentInitChunk == chunk->_previous);
			Assert_MM_true(sweepState->_currentInitChunk->_next == sweepState->_currentInitChunk->_nextChunk);
		}
		sweepState->_currentInitChunk = chunk;

		/* Prime to address last chunk for pool */
		sweepState->_currentSweepChunkReverse = chunk;

		/* add to the total size of all chunks in pool to be connected */
		sweepState->_heapSizeToConnect += chunk->size();
	} /* end of chunks */
}

/**
 * Perform necessary work before a connection into the free list is done.
 * Before a concurrent connection into the free list, reset the sweep states free stats to 0.  The stats accumulated during
 * the connect will be used to update the memory pool as well as other running metrics (total heap swept, largest free
 * entry, etc).
 */
void
MM_ConcurrentSweepScheme::preConnectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk)
{
	Assert_MM_true(chunk != NULL);

	MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(chunk->memoryPool);
	
	Assert_MM_true((void *)sweepState->_connectPreviousFreeEntry < chunk->chunkBase);
	Assert_MM_true((sweepState->_connectNextFreeEntry == NULL) || ((void *)sweepState->_connectNextFreeEntry >= chunk->chunkTop));

	/* Reset the free stats for the current chunk connection.  The stats from the
	 * connection will be added to the memory pool afterwards.
	 */
	sweepState->resetFreeStats();
}

/**
 * Perform necessary work after a connection into the free list is done.
 * After a concurrent connection into the free list, use the stats to update the memory pool, as well as other running
 * metrics, including the approximate free remaining memory.  Make sure the last connected entry is properly formed and
 * terminated.
 * The approximate free size in bytes remaining is updated to reflect what may come from further concurrent sweeping.
 * The running total of free memory connected this cycle is also recorded.
 * 
 * @note The _connectPreviousFreeEntry in the state must be valid.
 * @todo Handle the possibility of a "next" free entry (keep it in the sweep state?)
 */
void
MM_ConcurrentSweepScheme::postConnectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk)
{
	Assert_MM_true(chunk != NULL);

	MM_MemoryPool *memoryPool = chunk->memoryPool;
	MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
	
	Assert_MM_true((sweepState->_connectNextFreeEntry == NULL) || ((void *)sweepState->_connectNextFreeEntry >= chunk->chunkTop));

	/* update the total heap size connected for the subspace */
	sweepState->_heapSizeConnected += chunk->size();

	/* Calculate and set the new approximation of free memory still to be connected into this pool */
	calculateApproximateFree(env, memoryPool, sweepState);

	/* Update the running total of free memory connected for this cycle */
	sweepState->_freeMemoryConnected += sweepState->_sweepFreeBytes;
	
	/* Flush the connection stats to the memory pool */
	memoryPool->setFreeMemorySize(memoryPool->getActualFreeMemorySize() + sweepState->_sweepFreeBytes);
	memoryPool->setFreeEntryCount(memoryPool->getActualFreeEntryCount() + sweepState->_sweepFreeHoles);

	/* Make the final free entry valid on the free list.  The entry itself has already been counted in the stats
	 * book keeping - just a matter of getting the right header installed.
	 */
	if(NULL != sweepState->_connectPreviousFreeEntry) {
		/* If we are connecting to the top of the chunk then we may need to fix the heap to ensure it is still walkable.
		 * The scenario that can result in a corrupted heap is having an object/dead entry span two different chunks, with the last valid connection
		 * being at the end of the current chunk.  The result is the 2nd chunk can be unwalkable.
		 * The solution is to fix the heap by splitting the overlapping memory in the two chunks, and install a dead object header (or slots)
		 * at the beginning of the next chunk.  This ensures that a walk starting at the beginning of the next chunk will be safe.
		 */
		void *endFreeEntry = (void *)(((UDATA)sweepState->_connectPreviousFreeEntry) + sweepState->_connectPreviousFreeEntrySize);
		if (endFreeEntry == chunk->chunkTop) {
			GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, (omrobjectptr_t)sweepState->_connectPreviousFreeEntry, (omrobjectptr_t)chunk->chunkTop, true);
			omrobjectptr_t currentObject;
			UDATA entrySize;
			
			/* find the entry that takes us to the end of the chunk, or into the next chunk */
			while (NULL != (currentObject = objectIterator.nextObjectNoAdvance())) {
				if (objectIterator.isDeadObject()) {
					entrySize = objectIterator.getDeadObjectSize();
				} else {
					entrySize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(currentObject);
				}
				
				if ((UDATA)currentObject + entrySize > (UDATA)chunk->chunkTop) {
					/* Found an overlapping entry - fix the trailing portion that appears in the next chunk to be walkable */
					entrySize -= (UDATA)chunk->chunkTop - (UDATA)currentObject;
					memoryPool->abandonHeapChunk((void *)chunk->chunkTop, (void *)((UDATA)chunk->chunkTop + entrySize));						
					break;
				}
				objectIterator.advance(entrySize);
			}
		}

		/* Make the final free entry valid (as mentioned earlier) */
		memoryPool->createFreeEntry(env,
			(void *)sweepState->_connectPreviousFreeEntry, 
			(void *)(((UDATA)sweepState->_connectPreviousFreeEntry) + sweepState->_connectPreviousFreeEntrySize));

		/* If there is a free list entry beyond the connection point, link the last free entry to it. */
		/* TODO: This "connect" operation should really be left to the pool */
		if(NULL != sweepState->_connectNextFreeEntry) {
			Assert_MM_true(sweepState->_connectPreviousFreeEntry < sweepState->_connectNextFreeEntry);
			sweepState->_connectPreviousFreeEntry->setNext(sweepState->_connectNextFreeEntry);
		}
	}
	
	assume0(memoryPool->isValidListOrdering());
}

/**
 * Calculate the new value for approximate free
 * 
 * The approximate free remaining is the calculation of,
 *   ((Previous GC Free memory found) / (Previous GC Total Memory)) = ((Estimated Free Memory) / (Total Memory to be Connected))
 * which reduces to,
 *   (Estimated Free Memory) = ((Previous GC Free memory found) / (Previous GC Total Memory)) * (Total Memory to be Connected)
 * 
 */
MMINLINE void
MM_ConcurrentSweepScheme::calculateApproximateFree(
	MM_EnvironmentBase *env,
	MM_MemoryPool *memoryPool,
	MM_ConcurrentSweepPoolState *sweepState)
{
	UDATA approximateFree;
	UDATA heapSizeRemainingToBeConnected;
	
	/* Calculate the free memory approximation.  If there is no more connection work to take place, the approximation is 0 */
	Assert_MM_true(sweepState->_heapSizeToConnect >= sweepState->_heapSizeConnected);
	heapSizeRemainingToBeConnected = sweepState->_heapSizeToConnect - sweepState->_heapSizeConnected;
	approximateFree = (UDATA)(sweepState->_previousFreeRatio * heapSizeRemainingToBeConnected);
	
	approximateFree= MM_Math::roundToCeiling(sizeof(UDATA), approximateFree); 
	
	if(approximateFree > heapSizeRemainingToBeConnected) {
		approximateFree = heapSizeRemainingToBeConnected;
	}
	
	memoryPool->setApproximateFreeMemorySize(approximateFree);
	
}	

/**
 * @copydoc MM_ParallelSweepScheme::connectChunk(MM_EnvironmentStandard *, MM_ParallelSweepChunk *)
 * @note The sweep state stats are reset for every connection.
 * @note The _connectPreviousFreeEntry in the state must be valid.
 * @todo Need to account for a next free entry, assuming Address ordered lists
 */
void
MM_ConcurrentSweepScheme::connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk)
{
	/* Make sure all state information is set up for a connection into the free list */
	preConnectChunk(env, chunk);

	/* Do the connection work */
	MM_AtomicOperations::loadSync();  /* Make sure we haven't read into the chunk too early. */
	MM_ParallelSweepScheme::connectChunk(env, chunk);
	/* If this is the final chunk, make sure we finish it off */
	if(NULL == chunk->_nextChunk) {
		MM_MemoryPool *memoryPool = chunk->memoryPool;
		MM_ConcurrentSweepPoolState *state = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
		flushFinalChunk(env, memoryPool);
		state->_finalFlushed = true;
	}

	/* Update all running stats and make sure the free list is properly formed */
	postConnectChunk(env, chunk);
}

/**
 * Flush any remaining free list data from the last chunk processed for every memory subspace.
 */
void
MM_ConcurrentSweepScheme::flushAllFinalChunks(MM_EnvironmentBase *env)
{
	Assert_MM_unreachable();  // should not be making this call
	return ;
}

/**
 * Initialize all internal structures in order to perform a concurrent sweep.
 * The initialization includes clearing all per-subspace stats and data as well as any segment and chunk
 * information (linkage, etc) used for sweeping (concurrent or otherwise).
 * 
 * @note called by the master thread only
 */
void
MM_ConcurrentSweepScheme::setupForSweep(MM_EnvironmentBase *env)
{
	/* Initialize structures needed by the superclass */
	MM_ParallelSweepScheme::setupForSweep(env);

	/* Clear current statistics */
	_stats.clear();

	/* SweepChunk init: Reset largestFreeEntry of all subSpaces at beginning of sweep */
	_extensions->heap->resetLargestFreeEntry();

	/* Initialize sweep states in all memory subspaces */
	initializeSweepStates(env);
	
	/* Initialize the chunk range values */
	initializeChunks(env);
}

void
MM_ConcurrentSweepScheme::verifyFreeList(MM_EnvironmentStandard *env, MM_HeapLinkedFreeHeader *freeListHead)
{
	MM_HeapLinkedFreeHeader *current;

	current = freeListHead;
	while(NULL != current) {
		assume(current < current->afterEnd(), "Free list size overflows");
		assume( (current->getNext() == NULL) || (current < current->getNext()), "Free list next pointer is lower in memory");
		assume( (current->getNext() == NULL) || (current->getNext() > current->afterEnd()), "Size is too large (flows into next free entry)");

#if 1
		{
			MM_HeapLinkedFreeHeader *tempNext;
			UDATA tempSize;
			tempNext = current->getNext();
			tempSize = current->getSize();
			
			memset(current, 0xFA, tempSize);
			
			current->setNext(tempNext);
			current->setSize(tempSize);
		}
#endif /* 0 */
		
		current = current->getNext();
	}
}

/**
 * Find the next chunk in sequence to be swept.
 * @todo We should consider a compare and swap operation for pulling the next sweep chunk from the list.
 * @return chunk that may be eligible for sweeping.
 */
MM_ParallelSweepChunk *
MM_ConcurrentSweepScheme::getNextSweepChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState)
{
	MM_ParallelSweepChunk *chunk;

	sweepState->_sweepChunkIteratorLock.acquire();

	chunk = sweepState->_currentSweepChunk;
	while (chunk != NULL) {
		/* ensure that this chunk actually should be associated with the given sweepState */
		Assert_MM_true(sweepState == (MM_ConcurrentSweepPoolState *)getPoolState(chunk->memoryPool));
		/* Check chunk has not already been processed */
		if (chunk->_concurrentSweepState == modron_concurrentsweep_state_unprocessed) {
			break;
		}	
		/* get next chunk, if any */
		chunk = chunk->_nextChunk;	
	}	
	
	if(NULL != chunk) {
		sweepState->_currentSweepChunk = chunk->_nextChunk;
	} else {
		sweepState->_currentSweepChunk = NULL;
	}

	sweepState->_sweepChunkIteratorLock.release();
	
	return chunk;
}

/**
 * Find the previous chunk in sequence to be swept.
 * @todo We should consider a compare and swap operation for pulling the next sweep chunk from the list.
 * @return chunk that may be eligible for sweeping.
 */
MM_ParallelSweepChunk *
MM_ConcurrentSweepScheme::getPreviousSweepChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState)
{
	MM_ParallelSweepChunk *chunk;

	sweepState->_sweepChunkIteratorLock.acquire();

	chunk = sweepState->_currentSweepChunkReverse;
	if(NULL != chunk) {
		/* ensure that this chunk actually should be associated with the given sweepState */
		Assert_MM_true(sweepState == (MM_ConcurrentSweepPoolState *)getPoolState(chunk->memoryPool));
		Assert_MM_true(chunk->_concurrentSweepState == modron_concurrentsweep_state_unprocessed);
		MM_ParallelSweepChunk *previous = chunk->_previous;
		MM_ParallelSweepChunk *reverseChunk = NULL;
		
		/* we can only step back the chunk for this sweep state if the chunk preceding us points to us as its next chunk (otherwise, it is associated with a different pool) */
		if ((NULL != previous) && (previous->_nextChunk == chunk)) {
			reverseChunk = previous;
		}
		sweepState->_currentSweepChunkReverse = reverseChunk;
	}	
	
	sweepState->_sweepChunkIteratorLock.release();
	
	return chunk;
}

/**
 * Sweep the given chunk.
 * @note The chunk is expected to have been found through getNextSweepChunk or 
 * getPreviousSweepChunk(MM_EnvironmentStandard *, MM_ConcurrentSweepPoolState *).
 * @return TRUE if at least one live object in chunk; FALSE otherwise
 */
bool 
MM_ConcurrentSweepScheme::incrementalSweepChunk(
	MM_EnvironmentStandard *env,
	MM_ParallelSweepChunk *chunk)
{
	bool liveObjectFound;
	Assert_MM_true(modron_concurrentsweep_state_unprocessed == chunk->_concurrentSweepState);

	chunk->_concurrentSweepState = modron_concurrentsweep_state_busy_sweep;
	liveObjectFound = sweepChunk(env, chunk);	

	MM_AtomicOperations::add((UDATA *)&_stats._totalChunkSweptCount, 1);

	/* Make sure all sweeping information is flushed to memory before marking the chunk as having been swept.
	 * This is to avoid having the connector view the chunk as swept without all the data having been commited
	 * to memory.
	 */
	MM_AtomicOperations::storeSync();

	Assert_MM_true(modron_concurrentsweep_state_busy_sweep == chunk->_concurrentSweepState);

	chunk->_concurrentSweepState = modron_concurrentsweep_state_swept;
	
	return liveObjectFound;
}

/**
 * Find the next available chunk and attempt to sweep it.
 * @note sweep work is self contained, but we do need to be cautious concurrently setting that statistics.
 * @return true if a chunk was found and swept by the caller, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::sweepNextAvailableChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState)
{
	MM_ParallelSweepChunk *chunk;

	if(NULL != (chunk = getNextSweepChunk(env, sweepState))) {
		Assert_MM_true(!_stats.hasCompletedSweepConcurrently());
		incrementalSweepChunk(env, chunk);
		if(concurrentsweep_mode_completing_sweep_phase_concurrently == _stats._mode) {
			MM_AtomicOperations::add((UDATA *)&_stats._concurrentCompleteSweepBytesSwept, chunk->size());
		} else if (concurrentsweep_mode_stw_complete_sweep == _stats._mode) {
			MM_AtomicOperations::add((UDATA *)&_stats._completeSweepPhaseBytesSwept, chunk->size());
		}
		return true;
	}
	
	return false;
}

/**
 * Find the previous available chunk and attempt to sweep it.
 * @note this call is made by threads which do not participate in potential conflicting concurrent operations.
 * @return true if a chunk was found and swept by the caller, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::sweepPreviousAvailableChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState)
{
	MM_ParallelSweepChunk *chunk;
	MM_ConcurrentSweepFindMinimumSizeFreeTask *task = (MM_ConcurrentSweepFindMinimumSizeFreeTask *)env->_currentTask;
	
	if(NULL != (chunk = getPreviousSweepChunk(env, sweepState))) {
		if (incrementalSweepChunk(env, chunk)) {
			/* Only ever set this flag to true, otherwise the last thread to sweep the chunk can
			 * overwrite (and hide) the fact that a live object was found
			 */
			task->_foundLiveObject = true;
		}
		return true;
	}
	return false;
}

/**
 * Find the next available chunk and attempt to sweep it.
 * @note This call is made by java threads participating regular work (e.g., allocation tax, replenishing, etc).
 * @return true if a chunk was found and swept by the caller, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::concurrentSweepNextAvailableChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState)
{
	bool result = false;

	increaseActiveSweepingThreadCount(env, false);
	result = sweepNextAvailableChunk(env, sweepState);
	decreaseActiveSweepingThreadCount(env, false);

	return result;
}

/**
 * Find the next chunk in sequence to be connected.
 * @note the sweep states associated memory pool allocation lock (or equivalent) is expected to be held.
 * @return chunk that may be eligible for connecting.
 */
MM_ParallelSweepChunk *
MM_ConcurrentSweepScheme::getNextConnectChunk(MM_EnvironmentStandard *env, MM_ConcurrentSweepPoolState *sweepState)
{
	MM_ParallelSweepChunk *chunk;

	chunk = sweepState->_connectCurrentChunk;
	if(NULL != chunk) {
		sweepState->_connectCurrentChunk = chunk->_nextChunk;
	}	
	
	return chunk;
}

/**
 * Connect the given chunk to the memory pool free list.
 * 
 * @note Assumed to be under allocation lock, so only 1 connection can occur at any given time.
 * @return true if the chunk was connected, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::incrementalConnectChunk(
	MM_EnvironmentStandard *env,
	MM_ParallelSweepChunk *chunk,
	MM_ConcurrentSweepPoolState *sweepState,
	MM_MemoryPoolAddressOrderedList *memoryPool)
{
	/* We can only connect the chunk if it has been swept */
	if(modron_concurrentsweep_state_swept == chunk->_concurrentSweepState) {
		/* Unprocessed - take possession of connecting the chunk by flipping its state.
		 * Since moving to the connected state is uncontended, a simple store is enough.
		 * The busy_connect stage is really more for debugging than anything (since you can't
		 * exit the routine without flipping to connected).
		 */
		chunk->_concurrentSweepState = modron_concurrentsweep_state_busy_connect;

		/* Add the chunk to the free list */
		/* Do all connection work for the chunk.  This includes updating all running stats. */
		connectChunk(env, chunk);

//		verifyFreeList(env, memoryPool->_heapFreeList);

		/* Advance the chunk state to swept */			
		chunk->_concurrentSweepState = modron_concurrentsweep_state_connected;

		return true;
	}
	return false;
}

/**
 * Initialize any state information preceeding a round of connections.
 * Find the free entries of the pool that surround the specified chunk and update the state to treat these
 * as the previous and next free entries during connection.  This routine is called once for any round of
 * multiple connections; since sweeping is in address order, and expansion cannot occur in the middle of
 * the free list, the chunk is representative of all heap memory to be swept (ie: the next free entry will
 * be the same for all remaining chunks).
 * The memory pool hints are also update to be no higher than the connected chunks.
 * @note The initialization is valid only for a single round of connections (ie: all must be done under the same lock)
 */
void
MM_ConcurrentSweepScheme::initializeStateForConnections(
	MM_EnvironmentBase *envModron,
	MM_MemoryPoolAddressOrderedList *memoryPool,
	MM_ConcurrentSweepPoolState *sweepState,
	MM_ParallelSweepChunk *chunk)
{
	/* Find the previous/next free list entries that surround the chunk range.  This code
	 * really belongs in the pool itself, but keep hacks localized for now.
	 * In reality, the pool should maintain the end entry itself and just hand it back.
	 */
	MM_HeapLinkedFreeHeader *existingPrevious, *existingNext;
	existingPrevious = NULL;
	existingNext = memoryPool->_heapFreeList;
	if(NULL != chunk) {
		while(NULL != existingNext) {
			if((void *)existingNext > chunk->chunkBase) {
				break;
			}
			existingPrevious = existingNext;
			existingNext = existingNext->getNext();
		}
	}
	/* Adjust the state information to reflect the correct "on the fly" free list information */
	sweepState->_connectPreviousFreeEntry = existingPrevious;
	sweepState->_connectPreviousFreeEntrySize = (NULL == existingPrevious) ? 0 : existingPrevious->getSize();
	sweepState->_connectNextFreeEntry = existingNext;
	sweepState->_connectNextFreeEntrySize = (NULL == existingNext) ? 0 : existingNext->getSize();

	/* Update the memory pool hints to not miss any of the free entries that will be connected */
	memoryPool->updateHintsBeyondEntry(sweepState->_connectPreviousFreeEntry);
}

/**
 * @copydoc MM_ParallelSweepScheme::replenishPoolForAllocate(MM_EnvironmentBase *, MM_MemoryPool *, UDATA)
 * 
 * Finds the next available connection chunk and adds it to the free list.  The caller will attempt to
 * sweep the chunk if it is not ready to be connected.  If the chunk is currently being swept by another
 * thread, the caller will find sweep work to perform in the interim (or yield).   The process is repeated
 * until a free chunk large enough to satisfy the allocate request is found.
 */
bool
MM_ConcurrentSweepScheme::replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, UDATA size)
{
	if(isConcurrentSweepActive()) {
		MM_EnvironmentStandard *envStandard = MM_EnvironmentStandard::getEnvironment(env);
		MM_MemoryPoolAddressOrderedList *memoryPoolAOL = (MM_MemoryPoolAddressOrderedList *)memoryPool;

		MM_ParallelSweepChunk *chunk;
		/* Get the sweep state from the pool */
		MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPoolAOL);

		/* Make sure all free entry information is up to date for this round of connections.
		 * (e.g., previous/next free entry, hints are adusted accordingly, etc)
		 */
		initializeStateForConnections(envStandard, memoryPoolAOL, sweepState, sweepState->_connectCurrentChunk);

		/* Find the next available chunk to connect.  If a chunk is successfully connected, return.
		 * Otherwise, find a new chunk until all chunks have been processed.
		 */
		while(NULL != (chunk = getNextConnectChunk(envStandard, sweepState))) {
#if defined(CONCURRENT_SWEEP_TRACE)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	omrtty_printf("L");
}
#endif /* CONCURRENT_SWEEP_TRACE */
			/* Loop until the next chunk to connect has at least reached the swept stage */
			while(chunk->_concurrentSweepState < modron_concurrentsweep_state_swept) {
				/* The chunk hasn't been swept yet - move the sweeping work along for the state */
				if(!concurrentSweepNextAvailableChunk(envStandard, sweepState)) {
					/* No work was done, yield (someone else was trying to sweep our chunk) */
					omrthread_yield();
				}
			}
	
			/* If the chunk is in the swept state, try and connect */
			if(chunk->_concurrentSweepState == modron_concurrentsweep_state_swept) {
				if(incrementalConnectChunk(envStandard, chunk, sweepState, memoryPoolAOL)) {
					/* Update statistics depending on mode */
					if(concurrentsweep_mode_stw_find_minimum_free_size == _stats._mode) {
						_stats._minimumFreeEntryBytesConnected += chunk->size();
					}

					/* Successfully connected the chunk.   Check if the allocation request can be satisfied. */
					if(sweepState->_largestFreeEntry >= size) {
						return true;
					}
				}
			}
		}
	}

	/* No more chunks available to connect */
	return false;
}

/**
 * Calculate the number of chunks to sweep for the given allocation size.
 * @return The taxable number of chunks to sweep for the allocation.
 */
UDATA
MM_ConcurrentSweepScheme::calculateTax(MM_EnvironmentBase *envModron, UDATA allocationSize)
{
	UDATA remainingFree;
	UDATA chunkTax;
	double allocatePercentage;

	Assert_MM_true(_stats._totalChunkCount >= _stats._totalChunkSweptCount);

	remainingFree = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);	

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	if(_extensions->concurrentMark) {
		UDATA kickoffThreshold = ((MM_ConcurrentGC *)_collector)->_stats->getKickoffThreshold();
		
		/* Another thread may have pushed us over the kickoffThreshold leaving us with a 
		 * negative difference between remainingFree and kickoffThreshold 
		 */
		if (kickoffThreshold < remainingFree) {
			remainingFree -= kickoffThreshold;
		} else {
			remainingFree = 0;
		}
	}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	/* Get the ratio between the allocation size and the remaining heap size */
	if (remainingFree == 0) {
		allocatePercentage = (double)1.0;
	} else {
		allocatePercentage = ((double)allocationSize) / ((double)remainingFree);
		allocatePercentage = (allocatePercentage > (double)1.0) ? (double)1.0 : allocatePercentage;
	}

	chunkTax = (UDATA)(((double)(_stats._totalChunkCount - _stats._totalChunkSweptCount)) * allocatePercentage);

	/* Make sure we get at least 1 chunk */
	return (0 == chunkTax) ? 1 : chunkTax;
}

/**
 * Pay the sweep tax associated to the allocation amount.
 * 
 * Find most appropriate pool to sweep/connect based on allocation size. We will 
 * pay our tax sweeping/connecting this pool until all chunks connected. If we have
 * any outstanding tax to pay we will help out by sweeping chunks in any other 
 * pools with work left to do.
 *  
 */
void
MM_ConcurrentSweepScheme::payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace,  MM_AllocateDescription *allocDescription)
{
	UDATA allocationSize = allocDescription->getAllocationTaxSize();
	if(isConcurrentSweepActive()) {
		UDATA chunkTax = calculateTax(env, allocationSize);
		
		if (chunkTax > 0 ) {	
			
			/* First sweep the allocation pool */		
			MM_MemoryPool *allocPool = subSpace->getMemoryPool(allocationSize);
			if (NULL != allocPool) {
				chunkTax -= sweepPool(env, allocPool, chunkTax);
			}
		
			/* Have we paid all our tax yet ? */		
			if (chunkTax > 0 ) {
				/* Sweep any other pools */
				MM_MemorySubSpacePoolIterator mssPoolIterator(subSpace);
				MM_MemoryPool *memoryPool;
				
				while(chunkTax > 0 && (
					  NULL != (memoryPool = mssPoolIterator.nextPool())) ) {
				
					/* Skip the allocation pool..we've done that already  */	  	
					if (memoryPool == allocPool) {
						continue;
					}
					
					chunkTax -= sweepPool(env, memoryPool, chunkTax);
				}
			}	
		}	
	}	
}

/**
 * Sweep a pool chunks.
 *
 * Sweep sufficient of a pool sweep chunks to pay specified chunk tax 
 * @param memoryPool Pool whose chunks are to be swept
 * @param chunkTax Maxiimum number of chunks to be swept
 * @return Number of chunks actully swept
 */

MMINLINE UDATA
MM_ConcurrentSweepScheme::sweepPool(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool, UDATA chunkTax) 
{
	UDATA taxPaid = 0;
	MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
				
	/* If there any work to do on this pool ? */
	if(!sweepState->_finalFlushed) {
		while(taxPaid < chunkTax) {
			if(!concurrentSweepNextAvailableChunk(MM_EnvironmentStandard::getEnvironment(envModron), sweepState)) {
				break;
			}
			
			/* We swept another chunk */
			taxPaid++;
			
#if defined(CONCURRENT_SWEEP_TRACE)
			OMRPORT_ACCESS_FROM_OMRPORT(envModron->getPortLibrary());
			omrtty_printf("T");
#endif /* CONCURRENT_SWEEP_TRACE */
		}
	}
	
	return taxPaid;
}	

/**
 * Complete the sweep such that all pending chunks have been swept and connected to the appropriate
 * free lists.
 * @note Expects to have exclusive access.
 * @note Expects to have control over the parallel GC threads (ie: able to dispatch tasks).
 * @param Reason code to identify why completion of concurrent sweep is required.
 */
void
MM_ConcurrentSweepScheme::completeSweep(MM_EnvironmentBase* envBase, SweepCompletionReason reason)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	MM_Dispatcher *dispatcher = envBase->getExtensions()->dispatcher;
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());

	/* Do no work if we weren't in concurrent sweep mode */
	if(!isConcurrentSweepActive()) {
		return ;
	}

#if defined(CONCURRENT_SWEEP_TRACE)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	omrtty_printf("{");
}
#endif /* CONCURRENT_SWEEP_TRACE */

	_stats.setMode(concurrentsweep_mode_stw_complete_sweep);

	/* First complete the sweep of all chunks */
	_stats._completeSweepPhaseTimeStart = omrtime_hires_clock();
	if(reason == ABOUT_TO_GC) {
		_extensions->globalGCStats.sweepStats.clear();
	}
	MM_ConcurrentSweepCompleteSweepTask completeSweepTask(envBase, dispatcher, this);
	dispatcher->run(envBase, &completeSweepTask);
	_stats._completeSweepPhaseTimeEnd = omrtime_hires_clock();

	/* Find the last free entry for each memory pool and set up the sweep state to continue connections at that point */
	_stats._completeConnectPhaseTimeStart = omrtime_hires_clock();

	MM_MemoryPoolAddressOrderedList *memoryPool;
	MM_HeapMemoryPoolIterator poolIterator(envBase, _extensions->heap);
	while(NULL != (memoryPool = (MM_MemoryPoolAddressOrderedList *)poolIterator.nextPool())) {
		MM_ConcurrentSweepPoolState *sweepState;

		/* Find the previous/next free list entries that surround the chunk range.  This code
		 * really belongs in the pool itself, but keep hacks localized for now.
		 * In reality, the pool should maintain the end entry itself and just hand it back.
		 */
		sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
		
		/*
		 * Most likely would be NULL if wrong type of Memory Pool is used
		 */
		Assert_MM_true(NULL != sweepState);

		/* Make sure all free entry information is up to date for this round of connections.
		 * (e.g., previous/next free entry, hints are adusted accordingly, etc)
		 */
		initializeStateForConnections(env, memoryPool, sweepState, sweepState->_connectCurrentChunk);

		/* Complete the connection of all chunks for the given state */
		MM_ParallelSweepChunk *chunk;
		chunk = sweepState->_connectCurrentChunk;
		while(NULL != chunk) {
			Assert_MM_true(modron_concurrentsweep_state_swept == chunk->_concurrentSweepState);
			connectChunk(env, chunk);
			_stats._completeConnectPhaseBytesConnected += chunk->size();
			chunk->_concurrentSweepState = modron_concurrentsweep_state_connected;
			chunk = chunk->_nextChunk;
		}

		/* If there where chunks to sweep then finalFlushed should be true by now */
		Assert_MM_true(sweepState->_connectCurrentChunk == NULL || sweepState->_finalFlushed);
	}
	_stats._completeConnectPhaseTimeEnd = omrtime_hires_clock();

	/* Report that we have completed concurrent sweep  */
	reportCompletedConcurrentSweep(env, reason);
	
	/* The concurrent sweep has now completed - set mode indicating that it is inactive */
	_stats.switchMode(concurrentsweep_mode_stw_complete_sweep, concurrentsweep_mode_off);

#if defined(CONCURRENT_SWEEP_TRACE)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	omrtty_printf("}");
}
#endif /* CONCURRENT_SWEEP_TRACE */
}

/**
 * Complete the sweep of all remaining chunks.
 * This routine is the task entry point for all GC threads.  It does not connect chunks.
 * @note Do not call directly as this is the entry point for the dispatched task
 */
void
MM_ConcurrentSweepScheme::workThreadCompleteSweep(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);

	/* Find all sweep states in the system and complete their sweep work */
	MM_MemoryPool *memoryPool;
	MM_HeapMemoryPoolIterator poolIterator(envBase, _extensions->heap);
	while(NULL != (memoryPool = poolIterator.nextPool())) {
		MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);

		/* Complete the sweep of all chunks.
		 * Note that the very nature of the call makes it parallel thread safe, and manages its own work units.
		 */
		while(sweepNextAvailableChunk(env, sweepState)) {
			/* No action - continue doing the work until there's nothing more to do */
		}
	}
}

/**
 * Sweep enough heap to find a free entry of the minimum free size.
 * This routine is the task entry point for all GC Threads.  The thread responsibilities in the task are split into 
 * two groups.  The master will attempt to connect the next available chunk, checking if the minimum free size has been
 * found during the connection.  If it hasn't, it moves to the next chunk and repeats the process (until all chunks have
 * been processed).  If the next available chunk is not ready for connecting, the master thread will attempt to find
 * another chunk to sweep to move total work forward before returning to connecting.  Slave threads are strictly responsible
 * for finding and sweeping chunks.
 * When the master thread has finally connected in the minimum sized free entry, it will flag all slave threads to
 * complete current work and stop any further sweeping.
 * @note The _largestFreeEntry value for the pool will be set
 * @note Do not call directly as this is the entry point for the dispatched task.
 */
void
MM_ConcurrentSweepScheme::workThreadFindMinimumSizeFreeEntry(MM_EnvironmentBase *envBase, MM_MemorySubSpace *memorySubSpace, UDATA minimumFreeSize)
{
	MM_MemoryPool *memoryPool;
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	MM_ConcurrentSweepFindMinimumSizeFreeTask *task = (MM_ConcurrentSweepFindMinimumSizeFreeTask *)env->_currentTask; 
	UDATA poolCount = memorySubSpace->getActiveMemoryPoolCount();

	if (poolCount > 1) {
		/* Segment managed as multiple subpools so to enable us to connect chunks in any given pool
		 * in any order we must sweep enough chunks in any immediately lower pool to determine 
		 * the size of any projection into the higher pool
		 */ 
		MM_MemorySubSpacePoolIterator mssPoolIterator(memorySubSpace);
		UDATA poolNum = 1;
 		
		while(poolNum < poolCount &&  NULL != (memoryPool = mssPoolIterator.nextPool()) )  {

			MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
			
			/* Sweep chunks in reverse order until the first live object is found */
			while(!task->_foundLiveObject) {
				if(!sweepPreviousAvailableChunk(env, sweepState)) {
					/* no more work to do - leave */
					break;
				}
			}

			/* we now sync all threads and master thread propagates any projections */
			if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
				 
				/* Any projections need adjusting ? */
				if (task->_foundLiveObject) {
					/* Get reference to last chunk we have swept which may not be one with
					 * live object but if we work forward from it we will find the one
					 * with live object eventually.
					 *
					 * Note: sweepState->_currentSweepChunkReverse is updated during the sweepPreviousAvailableChunk
					 * call above to point to the next available predecessor.  This will be the chunk prior to the last swept chunk.
					 * If sweepState->_currentSweepChunkReverse is now pointing to NULL, then all the chunks have been swept;
					 * Otherwise the last swept chunk will be the next chunk after sweepState->_currentSweepChunkReverse.
					 */
					if (NULL == sweepState->_currentSweepChunkReverse) {
						if (NULL != sweepState->_currentSweepChunk) {
							/* Start propagating projections from the first chunk in the pool */
							propagateChunkProjections(env, sweepState->_currentSweepChunk);
							
							/* Ensure the first chunk in the next subpool will have enough information
							 * to be walkable without needing to walk objects in this subpool.
							 */
							abandonOverlappedChunks(env, sweepState->_currentSweepChunk, true);
						}
					} else {
						/* Start propagating from the last chunk swept */
						propagateChunkProjections(env, sweepState->_currentSweepChunkReverse->_nextChunk);

						/* Ensure the first chunk in the next subpool will have enough information
						 * to be walkable without needing to walk objects in this subpool.
						 */
						abandonOverlappedChunks(env, sweepState->_currentSweepChunkReverse->_nextChunk, false);
					}
				} else {
					/* There were no live objects in this pool, we still need to ensure that the first chunk
					 * in the next subpool will have enough information to be walkable without needing to walk
					 * objects in this subpool, but only if this pool had any chunks.
					 */
					if (NULL != sweepState->_currentSweepChunk) {
						abandonOverlappedChunks(env, sweepState->_currentSweepChunk, true);
					}
				}
				
				/* Initialize for next pool, if any */
				task->_foundLiveObject = false;
						
				env->_currentTask->releaseSynchronizedGCThreads(env);
			}
			
			/* Process next pool */
			poolNum++;
		}	
	}

	/* Do we actually need to connect any free chunks to satisfy an AF ? */	
	if (0 == minimumFreeSize) {
		/* No ..we are done then. All sweep/connect work will be done by mutators */
		return;
	}	
	
	/* Determine which pool is the most appropriate to sweep given required minimum free size */
	memoryPool = memorySubSpace->getMemoryPool(minimumFreeSize);
	if (NULL != memoryPool) {
		if(env->isMasterThread()) {
			/* Master thread: connect chunks until the correct entry is found or there are no more chunks to connect.
			 * If an entry is found, set the task flag for slave termination to true
			 */
			task->_foundMinimumSizeFreeEntry = replenishPoolForAllocate(env, memoryPool, minimumFreeSize);
			memoryPool->setLargestFreeEntry(getPoolState(memoryPool)->_largestFreeEntry);
		} else {
			/* Slave thread: keep finding and sweeping chunks until there are no more chunks to sweep or
			 * the master has found an entry of the correct size and set the task termination flag.
			 * Note: We might want to have a different flagging system that allows a slave to find the entry from
			 * its per-chunk statistics.
			 */
			MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
	
			while(!task->_foundMinimumSizeFreeEntry) {
				if(!sweepNextAvailableChunk(env, sweepState)) {
					/* no more work to do - leave */
					break;
				}
			}
		}
	}
}

/**
 * Do checks on the runtime system to be sure it is safe to have concurrent sweep.
 * This routine is for development purposes and just checks a bunch of preconditions with assumes.
 */
void
MM_ConcurrentSweepScheme::checkRestrictions(MM_EnvironmentBase *env)
{
#if defined(OMR_GC_MODRON_SCAVENGER)
	assume(env->getExtensions()->scavengerEnabled == false, "Must be a flat collector");
#endif /* OMR_GC_MODRON_SCAVENGER */	
}

/**
 * Perform a basic sweep operation.
 * There is no expectation for amount of work done for this routine.  The receiver is entitled to do as much or as little
 * work towards completing the sweep as it wants.  In this case, the minimum setup work for starting a concurrent
 * sweep is performed.
 * 
 * @note Expect to have exclusive access
 */
void
MM_ConcurrentSweepScheme::sweep(MM_EnvironmentBase *env)
{
	checkRestrictions(env);
	
	_stats.switchMode(concurrentsweep_mode_off, concurrentsweep_mode_on);

	setupForSweep(env);
}

/**
 * Sweep and connect the heap until a free entry of the specified size is found.
 * The subspace and all of its children are processed for the minimum free size.  A success is considered to be having
 * at least one of the children containing a free entry.  The expectation is that an allocation request to this space
 * will be able to meet at least the minimum value specified in some space.
 * 
 * @note Expects to have exclusive access
 * @note Expects to have control over the parallel GC threads (ie: able to dispatch tasks)
 * @note This only sweeps spaces that are concurrent sweepable
 * @return true if a free entry of at least the requested size was found, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::sweepForMinimumSize(
	MM_EnvironmentBase *env,
	MM_MemorySubSpace *baseMemorySubSpace, 
	MM_AllocateDescription *allocateDescription)
{	
	UDATA minimumFreeSize = (allocateDescription ? allocateDescription->getBytesRequested() : 0);

	MM_MemorySubSpace *memorySubSpace;
	MM_MemorySubSpaceChildIterator mssChildIterator(baseMemorySubSpace);
	bool minimumSizeFound;

#if defined(CONCURRENT_SWEEP_TRACE)
OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
omrtty_printf("[");
#endif /* CONCURRENT_SWEEP_TRACE */

	checkRestrictions(env);

	_stats.switchMode(concurrentsweep_mode_off, concurrentsweep_mode_stw_find_minimum_free_size);

	setupForSweep(MM_EnvironmentStandard::getEnvironment(env));

	minimumSizeFound = false;

	while(NULL != (memorySubSpace = mssChildIterator.nextSubSpace())) {
		if(isConcurrentSweepCandidate(memorySubSpace)) {
			MM_MemorySubSpacePoolIterator mssPoolIterator(memorySubSpace);
			MM_MemoryPool *memoryPool;
			MM_ConcurrentSweepPoolState *state;
			
			MM_ConcurrentSweepFindMinimumSizeFreeTask findMinimumSizeFreeEntryTask(env, _dispatcher, this, memorySubSpace, minimumFreeSize);
			_dispatcher->run(env, &findMinimumSizeFreeEntryTask);
			minimumSizeFound |= (bool)findMinimumSizeFreeEntryTask._foundMinimumSizeFreeEntry;
			
			/* Ensure approximate free reset in all memory pools not just the ones we sweep */
			while(NULL != (memoryPool = mssPoolIterator.nextPool())) {
				state = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);
	        	calculateApproximateFree(env, memoryPool, state);
			}
		}
	}

	_stats.switchMode(concurrentsweep_mode_stw_find_minimum_free_size, concurrentsweep_mode_on);

#if defined(CONCURRENT_SWEEP_TRACE)
omrtty_printf("]");
#endif /* CONCURRENT_SWEEP_TRACE */

	return minimumSizeFound;
}

/**
 * Complete the sweeping phase for all memory pools concurrently.
 * Walk all memory pools in the system, completing the sweep (not connect) phases have been completed for each one.
 * Threads are assumed to have VM access, and threads may join the work at any time (and leave at any time).  The
 * final thread to leave the work with no work remaining is responsible for flagging all sweep work as completed.
 * @note All participating threads have VM access.
 * @return true if sweeping has been completed, false otherwise.
 */
bool
MM_ConcurrentSweepScheme::completeSweepingConcurrently(MM_EnvironmentBase *envModron)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envModron);

	/* Sanity check - is there anything to do here? */
	if(!_stats.canCompleteSweepConcurrently()) {
		return true;
	}

	if(!increaseActiveSweepingThreadCount(env, true)) {
		return true;
	}

	/* Walk all pools, sweeping every remaining chunk in the to-be-swept list */
	MM_HeapMemoryPoolIterator poolIterator(envModron, _extensions->heap);
	MM_MemoryPoolAddressOrderedList *memoryPool;
	while(NULL != (memoryPool = (MM_MemoryPoolAddressOrderedList *)poolIterator.nextPool())) {
		MM_ConcurrentSweepPoolState *sweepState = (MM_ConcurrentSweepPoolState *)getPoolState(memoryPool);

		while(sweepNextAvailableChunk(env, sweepState)) {
#if defined(CONCURRENT_SWEEP_TRACE)
			OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
			omrtty_printf("C");
#endif /* CONCURRENT_SWEEP_TRACE */
		}
	}

	decreaseActiveSweepingThreadCount(env, true);
	
	return true;
}

/**
 * Add to the concurrently sweeping thread pool count.
 * 
 * @param completeSweeping Flag determining whether the thread is attempting to complete all sweeping concurrently.
 * @return true if there is work to be done (successfully bumped the count), false otherwise.
 */
bool
MM_ConcurrentSweepScheme::increaseActiveSweepingThreadCount(MM_EnvironmentBase *envModron, bool completeSweeping)
{
	omrthread_monitor_enter(_completeSweepingConcurrentlyLock);
	Assert_MM_true(isConcurrentSweepActive());
	
	if(completeSweeping) {
		/* Sweeping could have been completed (but since we have access, after the check above, all sweep/connection
		 * activity cannot) while acquiring the lock - check again.
		 */
		if(_stats.hasCompletedSweepConcurrently()) {
			omrthread_monitor_exit(_completeSweepingConcurrentlyLock);
			return false;
		}

		/* If we are the first thread to start sweeping during the complete sweep phase concurrently, switch the execution mode */
		/* NOTE: We can't use the threadcount here to check as other threads not actively completing the sweep may have
		 * bumped the count above zero (sweeping to connect, general allocation tax that missed kickoff, etc).
		 */
		if(concurrentsweep_mode_on == _stats._mode) {
			OMRPORT_ACCESS_FROM_OMRPORT(envModron->getPortLibrary());
			_stats.switchMode(concurrentsweep_mode_on, concurrentsweep_mode_completing_sweep_phase_concurrently);
			_stats._concurrentCompleteSweepTimeStart = omrtime_hires_clock();
		}
	}

	_concurrentSweepingThreadCount += 1;
	
	omrthread_monitor_exit(_completeSweepingConcurrentlyLock);

	return true;
}

/**
 * Subtract from the concurrently sweeping thread pool count.
 * @return true if all concurrent sweeping activity is complete (if started), false otherwise.
 */
bool
MM_ConcurrentSweepScheme::decreaseActiveSweepingThreadCount(MM_EnvironmentBase *envModron, bool completeSweeping)
{
	omrthread_monitor_enter(_completeSweepingConcurrentlyLock);

	_concurrentSweepingThreadCount -= 1;
	if(_stats.isCompletingSweepConcurrently()) {
		/* We've completed a full pass through the memory pools, and done all the work we could.  If we're the last
		 * thread leaving the operation, then we must have successfully completed the sweeping phase (set the
		 * flag to say as much).
		 */
		if(0 == _concurrentSweepingThreadCount) {
			OMRPORT_ACCESS_FROM_OMRPORT(envModron->getPortLibrary());
			_stats._concurrentCompleteSweepTimeEnd = omrtime_hires_clock();
			reportConcurrentlyCompletedSweepPhase(envModron);
			_stats.switchMode(
				concurrentsweep_mode_completing_sweep_phase_concurrently,
				concurrentsweep_mode_completed_sweep_phase_concurrently);
			omrthread_monitor_notify_all(_completeSweepingConcurrentlyLock);
		}
		else {
			if(completeSweeping) {
				omrthread_monitor_wait(_completeSweepingConcurrentlyLock);
			}
		}
	}

	omrthread_monitor_exit(_completeSweepingConcurrentlyLock);

	if(completeSweeping) {
		omrthread_yield();
	}

	return true;
}

/**
 * Walk the chunks forward propagating any projections.  Projections are propagated into the next chunk if the projection is larger than
 * the chunk.  Projections are propagated to the end of the memory pool.
 * 
 * @param firstChunkToPropagate starting chunk to propogate projections
 * @note Only the chunks after the first live object will be gaurenteed to have proper projections
 */
void
MM_ConcurrentSweepScheme::propagateChunkProjections(MM_EnvironmentBase *envModron, MM_ParallelSweepChunk *startingChunkToPropagate)
{
	MM_ParallelSweepChunk *previousChunk, *currentChunk;
	UDATA projection;
	
	previousChunk= startingChunkToPropagate;
	currentChunk = startingChunkToPropagate->_nextChunk;

	while (NULL != currentChunk) {
		/* Sanity checks */
		Assert_MM_true(currentChunk->_concurrentSweepState >= modron_concurrentsweep_state_swept);
		Assert_MM_true(previousChunk->chunkTop == currentChunk->chunkBase);              
		Assert_MM_true(previousChunk->memoryPool == currentChunk->memoryPool);
		
		/* Get size of any projection from previous chunk */
		projection = previousChunk->projection;

		/* Does the projection extend beyond this chunk ? */
		if(projection > (((UDATA)currentChunk->chunkTop) - ((UDATA)currentChunk->chunkBase))) {
			currentChunk->projection = projection - (((UDATA)currentChunk->chunkTop) - ((UDATA)currentChunk->chunkBase));
		}

		/* Move to next chunk */
		previousChunk = currentChunk;
		currentChunk = currentChunk->_nextChunk;
	}	
}

/**
 * Walk the chunks forward propagating any projections.  Projections are propagated into the next chunk if the projection is larger than
 * the chunk.  Projections are propagated to the end of the memory pool.
 * 
 * @param firstChunkToPropagate starting chunk to propogate projections
 * @note Only the chunks after the first live object will be gaurenteed to have proper projections
 */
void
MM_ConcurrentSweepScheme::abandonOverlappedChunks(MM_EnvironmentBase *envModron, MM_ParallelSweepChunk *startingChunk, bool isFirstChunkInSubpool)
{
	MM_ParallelSweepChunk *currentChunk = startingChunk;
	bool chunkProcessed = false;
	CONSS_DEBUG_OMRPORT_ACCESS_FROM_OMRPORT(envModron->getPortLibrary());

	do {
		void *walkStart;

		CONSS_DEBUG_PRINTF(("investigating chunk [%p]\n", currentChunk));

		/* If the chunk has a projection, then the next chunks starting walk point will be well defined (there is no leading free entry that may
		 * have started in a previous chunk).  This chunk can be safely ignored.
		 */
		if(0 != currentChunk->projection) {
			CONSS_DEBUG_PRINTF(("has projection, next chunk safe walkstart known\n"));
			chunkProcessed = true;
			continue;
		}

		walkStart = NULL;
		
		/* If the chunk has a trailing free candidate then there must have been a live object within the chunk, or a projection into the chunk.
		 * This is a well defined starting point.  A walk is required to properly abandon any memory overlap as this chunk may be the chunk
		 * adjacent to the next subpool (ie: the last chunk in the SOA before the LOA chunks)
		 */
		if(NULL != currentChunk->trailingFreeCandidate) {
			walkStart = currentChunk->trailingFreeCandidate;
			CONSS_DEBUG_PRINTF(("trailing(size: %p): set walkstart [%p]\n", currentChunk->trailingFreeCandidateSize, walkStart));
		} else {
			if(NULL != currentChunk->leadingFreeCandidate) {
				/* Check if the chunk is completely empty */
				if(((void *)(((UDATA)currentChunk->leadingFreeCandidate) + currentChunk->leadingFreeCandidateSize) == currentChunk->chunkTop)) {
					/* If this is the very first chunk in the pool (the pool was entirely empty.)  It will be safe to walk from the beginning. */
					if (isFirstChunkInSubpool && (currentChunk == startingChunk))
					{
						walkStart = (void *)currentChunk->leadingFreeCandidate;
						CONSS_DEBUG_PRINTF(("initial(size: %p): set walkstart [%p]\n", currentChunk->leadingFreeCandidateSize, walkStart));
					} else if (chunkProcessed) {
						/* A chunk has been previously processed -- it is safe to walk the chunk from the beginning, or at a projection point */

						/* It's possible to think that this chunk is completely empty, even if there is a projection, so factor in the previous chunk
						 * projection, and use this information to determine the walk starting point.
						 */
						walkStart = (void *)((UDATA)currentChunk->chunkBase + currentChunk->_previous->projection);
						CONSS_DEBUG_PRINTF(("projection(size: %p): set walkstart [%p]\n", currentChunk->_previous->projection, walkStart));
					} else {
						/* The startingChunk can be a chunk previous to the chunk found with the live object in it (as per comment in
						 * workThreadFindMinimumSizeFreeEntry.)  This must be a chunk before the chunk with the live object in it.  If there was
						 * no live object then this would have to be the first chunk in the subpool which would have been handle in the case above.
						 * It can be skipped because there will be a live object before the end of this subpools chunks.
						 * Do not flag chunkProcessed because no chunk has been processed yet.
						 */
						CONSS_DEBUG_PRINTF(("starting empty chunk -- ignore\n"));
						continue;
					}
				} else {
					/* If the chunk does not have a trailing free entry and has a leading free entry, there must be a live object at the end of
					 * the chunk.  The live object can not overlap between chunks, otherwise there would have been a projection, which is handled
					 * above.  The next chunk will be walkable from the beginning of the chunk.  No walk needs to be done for the current chunk.
					 */
					CONSS_DEBUG_PRINTF(("live object terminates at end of chunk -- next chunk walkable from start\n"));
					chunkProcessed = true;
					continue;
				}
			} else {
				/* There is neither a trailing free entry or a leading free entry.  There must be a live object at the end of the chunk.  If the
				 * live object overlaps onto the next chunk there would have been a projection, which is handled above.  The next chunk will be
				 * walkable from the beginning of the chunk, so no walk needs to be done for this chunk.
				 */
				CONSS_DEBUG_PRINTF(("live objects at beginning and end of chunk -- next chunk walkable from start\n"));
				chunkProcessed = true;
				continue;
			}
		}

		chunkProcessed = true;

		/* Setup the next chunk in case we need to walk it */
		walkChunkForOverlappingDeadSpace(envModron, currentChunk, walkStart);
	} while(NULL != (currentChunk = currentChunk->_nextChunk));

	CONSS_DEBUG_PRINTF(("done investigating chunks\n"));
} 

/** Walk a chunk and ensure that the next chunk will have enough information in the chunk to start an object walk
 *  from the its base.
 *
 *  @param currentChunk the chunk to walk for overlapping dead space 
 *  @param walkStart address to start the object walk
 *  @note The chunk MUST be known to have dead space at the end
 */
void
MM_ConcurrentSweepScheme::walkChunkForOverlappingDeadSpace(MM_EnvironmentBase *envModron, MM_ParallelSweepChunk *currentChunk, void *walkStart)
{
	CONSS_DEBUG_OMRPORT_ACCESS_FROM_OMRPORT(envModron->getPortLibrary());

	Assert_MM_true(NULL != walkStart);
	Assert_MM_true(0 == currentChunk->projection);

	CONSS_DEBUG_PRINTF(("walking chunk [%p] at [%p]\n", currentChunk, walkStart));

	/* Walk to find overlap and abandon both sides */
	GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, (omrobjectptr_t)walkStart, (omrobjectptr_t)currentChunk->chunkTop, true);
	omrobjectptr_t currentObject;
	UDATA entrySize;

	/* find the entry that takes us to the end of the chunk, or into the next chunk */
	while (NULL != (currentObject = objectIterator.nextObjectNoAdvance())) {
		CONSS_DEBUG_PRINTF(("obj[%p]\n", currentObject));

		if (objectIterator.isDeadObject()) {
			entrySize = objectIterator.getDeadObjectSize();
			CONSS_DEBUG_PRINTF(("deadsize[%p]\n", entrySize));
		} else {
			entrySize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(currentObject);
			CONSS_DEBUG_PRINTF(("livesize[%p]\n", entrySize));
		}

		/* If the currentObject (or dark matter) extends past the top of the chunk (overlaps multiple chunks) then abandon both halves.
		 * In order to get here, the currentChunk must not have a projection, so we know that the currentObject would be dead, and
		 * hence safe to abandon.
		 */
		if ((UDATA)currentObject + entrySize > (UDATA)currentChunk->chunkTop) {
			/* Found an overlapping dead entry */
			/* TODO: when splitting regions spanning more than two chunks all the inner chunks have abandon called twice. */
			entrySize -= (UDATA)currentChunk->chunkTop - (UDATA)currentObject;

			CONSS_DEBUG_PRINTF(("abandoning [%p - %p] and [%p - %p]\n", currentObject, currentChunk->chunkTop,
				currentChunk->chunkTop, (void *)((UDATA)currentChunk->chunkTop + entrySize)));

			currentChunk->memoryPool->abandonHeapChunk((void *)currentObject, (void *)currentChunk->chunkTop);
			currentChunk->memoryPool->abandonHeapChunk((void *)currentChunk->chunkTop, (void *)((UDATA)currentChunk->chunkTop + entrySize));
			break;
		}

		objectIterator.advance(entrySize);
	}
}

#endif /* OMR_GC_CONCURRENT_SWEEP */
