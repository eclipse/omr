/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 ******************************************************************************/

#if !defined(COLLECTOR_HPP_)
#define COLLECTOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "omrgcconsts.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"

class MM_AllocateDescription;
class MM_AllocationContext;
class MM_CollectorLanguageInterface;
class MM_MemoryPool;
class MM_ObjectAllocationInterface;
class MM_MemorySubSpace;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_Collector : public MM_BaseVirtual {
	/*
	 * Data members
	 */
private:
	uintptr_t _exclusiveAccessCount; /**< The number of times exclusive access requests have been made to use the receiver */

protected:
	MM_CollectorLanguageInterface *_cli; /**< Language specific interface used to extend the Modron GC framework */

	uintptr_t _bytesRequested;

	bool _globalCollector;
	bool _gcCompleted;
	bool _isRecursiveGC;
	uintptr_t _collectorExpandedSize;
	uintptr_t _cycleType;

	uint64_t _masterThreadCpuTimeStart; /**< slot to store the master CPU time at the beginning of the collection */

public:
	/*
	 * Determine if a collector (some parent, typically the global collector) wishes to usurp any minor collection.
	 * @return boolean indicating if the parent collector should be invoked in place of a child.
	 */
	virtual bool isTimeForGlobalGCKickoff();

	/**
 	 * Perform any collector-specific initialization.
 	 * @return TRUE if startup completes OK, FALSE otherwise
 	 */
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);

	/**
	 * Perform any collector-specific shutdown actions.
	 */
	virtual void collectorShutdown(MM_GCExtensionsBase* extensions);

	/**
	 * Determine if the last requested GC completed successfully.
	 * @note This call is only valid for any concluded GC activity and is undefined for in progress activity.
	 * @return boolean indicating if the last GC completed successfully.
	 */
	bool gcCompleted() { return _gcCompleted; }

private:
	void setThreadFailAllocFlag(MM_EnvironmentBase *env, bool flag);

	void recordExcessiveStatsForGCStart(MM_EnvironmentBase *env);
	void recordExcessiveStatsForGCEnd(MM_EnvironmentBase *env);

protected:

	/**
 	 * Perform collector specific initailization 
 	 */
	virtual void setupForGC(MM_EnvironmentBase* env) = 0;

	/**
 	 * Invoke garbage collection
 	 *
	 * @param subSpace the memory subspace where the collection is occurring
	 * @param allocDescription if non-NULL, contains information about the allocation
	 * which triggered the GC. If NULL, the GC is a system GC.
 	 * @param aggressive Set TRUE is previous collect failed to free sufficient storage to 
 	 * request that this collect should be more aggressive, ie clear any soft references. 
 	 * @return true if gc completed OK, false otherwise
 	 */
	virtual bool internalGarbageCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, MM_AllocateDescription* allocDescription) = 0;

	/**
	 * Perform any collector setup activities.
	 * @param env Master GC thread.
	 * @param subSpace the memory subspace where the collection is occurring
	 * @param allocDescription Allocation description causing the GC (or detailing the GC request)
	 * @param gcCode High level reason for invoking the GC
	 */
	virtual void internalPreCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, MM_AllocateDescription* allocDescription, uint32_t gcCode) = 0;

	/**
	 * Post collection activities,  indicating that the collection has been completed.
	 * @param subSpace the memory subspace where the collection occurred
	 */
	virtual void internalPostCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace) = 0;

	/**
	 * Pre-condition initialization for garbage collection cycle.
	 * This routine is meant to be an initializer for meta state on a Gc cycle, such as the aggressiveness of collection, excessive GC calculation,
	 * and cycle state setup for the master thread.
	 * @param env Master GC thread.
	 * @param subSpace the memory subspace where the collection is occurring
	 * @param allocDescription Allocation description causing the GC (or detailing the GC request)
	 * @param gcCode High level reason for invoking the GC
	 */
	void preCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, MM_AllocateDescription* allocDescription, uint32_t gcCode);

	/**
	 * process LargeAllocateStats before GC
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsBeforeGC(MM_EnvironmentBase* env)
	{
	}

	/**
	 * process LargeAllocateStats after GC
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsAfterGC(MM_EnvironmentBase* env)
	{
	}

public:
	/**
	 * Return the uintptr_t corresponding to the VMState for this Collector.
	 * @note All collectors must implement this method - the IDs are defined in @ref j9modron.h
	 */
	virtual uintptr_t getVMStateID(void) = 0;

	virtual void kill(MM_EnvironmentBase* env) = 0;

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 * 
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @return true if operation completes with success
	 */
	virtual bool heapAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress) = 0;

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 * 
	 * @param size The amount of memory removed from the heap
	 * @param lowAddress The base address of the memory removed from the heap
	 * @param highAddress The top address (non-inclusive) of the memory removed from the heap
	 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
	 * @param highValidAddress The first valid address following the highest in the heap range being removed
	 * @return true if operation completes with success
	 */
	virtual bool heapRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress, void* validLowAddress, void* validHighAddress) = 0;

	/**
	 * Called after the heap geometry changes to allow any data structures dependent on this to be updated.
	 * This call could be triggered by memory ranges being added to or removed from the heap or memory being
	 * moved from one subspace to another.
	 * @param env[in] The thread which performed the change in heap geometry 
	 */
	virtual void heapReconfigured(MM_EnvironmentBase* env) = 0;

	/**
	 * Post collection broadcast event, indicating that the collection has been completed.
	 * @param subSpace the memory subspace where the collection occurred
	 */
	void postCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace);

	void* garbageCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* callingSubSpace, MM_AllocateDescription* allocDescription, uint32_t gcCode, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_AllocationContext* context);
	virtual bool forceKickoff(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, MM_AllocateDescription* allocDescription, uint32_t gcCode);

	virtual void collectorExpanded(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, uintptr_t expandSize);
	virtual bool canCollectorExpand(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, uintptr_t expandSize);
	virtual uintptr_t getCollectorExpandSize(MM_EnvironmentBase* env);
	virtual uint32_t getGCTimePercentage(MM_EnvironmentBase* env)
	{
		return 0;
	};

	virtual void scanThread(MM_EnvironmentBase* env) {};

	virtual void payAllocationTax(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace,
								  MM_MemorySubSpace* baseSubSpace, MM_AllocateDescription* allocDescription);

	virtual bool replenishPoolForAllocate(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool, uintptr_t size);

	/**
	 * Marks this collector instance as a global or local collector
	 */
	void setGlobalCollector(bool isGlobal)
	{
		_globalCollector = isGlobal;
	}

	/**
	 * Called to request the number of times the environment has incremented the exclusive access count of the receiver.
	 * This is most often used to determine if a GC completed while a thread was blocked requesting exclusive.
	 * @return The number of exclusive access requests made to use this collector
	 */
	MMINLINE uintptr_t getExclusiveAccessCount()
	{
		return _exclusiveAccessCount;
	}

	/**
	 * Called by the environment to increment the number of exclusive access requests made to use this collector.
	 */
	MMINLINE void incrementExclusiveAccessCount()
	{
		_exclusiveAccessCount += 1;
	}

	/**
 	* Abstract for request to create sweepPoolState class for pool
 	* @param  memoryPool memory pool to attach sweep state to
 	* @return pointer to created class
 	*/
	virtual void* createSweepPoolState(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool) = 0;

	/**
 	* Abstract for request to destroy sweepPoolState class for pool
 	* @param  sweepPoolState class to destroy
 	*/
	virtual void deleteSweepPoolState(MM_EnvironmentBase* env, void* sweepPoolState) = 0;

	virtual void abortCollection(MM_EnvironmentBase* env, CollectionAbortReason reason);

	/**
	 * Change the thread priority on all active worker GC threads.
	 * Default action is nothing - only required at this point for realtime
	 * GC
	 */
	/* TODO: RTJ Merge: Somewhat scary to use a virtual with inline implementation */
	virtual void setGCThreadPriority(OMR_VMThread* vmThread, uintptr_t priority)
	{
	}

	/**
	 * @param objectPtr[in] The object pointer to check
	 * @return true if the collector implementation knows that the given objectPtr is marked (false if not or if the implementation doesn't know)
	 */
	virtual bool isMarked(void* objectPtr);

	MM_Collector(MM_CollectorLanguageInterface *cli)
		: MM_BaseVirtual()
		, _exclusiveAccessCount(0)
		, _cli(cli)
		, _bytesRequested(0)
		, _globalCollector(false)
		, _gcCompleted(false)
		, _isRecursiveGC(false)
		, _collectorExpandedSize(0)
		, _cycleType(OMR_GC_CYCLE_TYPE_DEFAULT)
		, _masterThreadCpuTimeStart(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* COLLECTOR_HPP_ */
