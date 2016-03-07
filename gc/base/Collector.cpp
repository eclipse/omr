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

#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"
#include "Heap.hpp"
#include "AllocateDescription.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "MemorySubSpace.hpp"

class MM_MemorySubSpace;
class MM_MemorySpace;

/**
 * Report API for when an expansion has occurred during a collection.
 * This call is typically made from collectorAllocate() class of calls where a collector asks a subspace to expand
 * while trying to allocate memory for objects.  An example is the scavenger, which will attempt to expand the
 * tenure area if it fails to allocate tenure space.  This call is expected to be made under the allocation lock
 * of a particular sub space - if there is no lock, or multiple sub spaces could expand, some form of atomicity
 * to this operation needs to be introduced.
 * @note This call is NOT made pre/post collection, when actual user type allocation requests are being statisfied.
 * @param subSpace memory subspace that has expanded.
 * @param expandSize number of bytes the subspace was expanded by.
 */
void
MM_Collector::collectorExpanded(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, uintptr_t expandSize)
{
	_collectorExpandedSize += expandSize;
}

/**
 * Answer whether the subspace can expand on a collector-invoked allocate request.
 * The query is made only on collectorAllocate() type requests when the allocation fails, and a decision on whether
 * to expand the subspace to satisfy the allocate is being made.
 * @note Call is only made during collection and during a collectorAllocate() type request to the subspace.
 * @return true if the subspace is allowed to expand, false otherwise.
 */
bool
MM_Collector::canCollectorExpand(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, uintptr_t expandSize)
{
	return false;
}

/**
 * Returns requested expand size.
 * The query is made only on collectorAllocate() type requests when the allocation fails, and a decision
 * on how much to expand the subspace to satisfy the allocate is being made.
 * @note Call is only made during collection and during a collectorAllocate() type request to the subspace.
 * @return size to subspace expand by.
 * @ingroup GC_Base_Core
 */
uintptr_t
MM_Collector::getCollectorExpandSize(MM_EnvironmentBase* env)
{
	return 0;
}

/**
 * Serve forced request for kickoff (generic)
 *
 * @param subSpace the memory subspace where the collection is occurring
 * @param allocDescription if non-NULL, contains information about the allocation
 * which triggered the GC. If NULL, the GC is a system GC.
 * @param gcCode requested GC code
 * @return true if kickoff can be forced
 */
bool
MM_Collector::forceKickoff(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, MM_AllocateDescription* allocDescription, uint32_t gcCode)
{
	return false;
}

/**
 * Pay the allocation tax of the mutator.
 * The base implementation is to do nothing.
 */
void
MM_Collector::payAllocationTax(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, MM_MemorySubSpace* baseSubSpace, MM_AllocateDescription* allocDescription)
{
	return;
}

/**
 * Replenish a pools free lists to satisfy a given allocate.
 * The given pool was unable to satisfy an allocation request of (at least) the given size.  See if there is work
 * that can be done to increase the free stores of the pool so that the request can be met.
 * @note This call is made under the pools allocation lock (or equivalent)
 * @note Base implementation does no work.
 * @return True if the pool was replenished with a free entry that can satisfy the size, false otherwise.
 */
bool
MM_Collector::replenishPoolForAllocate(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool, uintptr_t size)
{
	return false;
}

/**
 * Record statistics (stored in MM_ExcessiveGCStats) at the beginning of a GC.
 * @todo This code probably belongs in MM_ExcessiveGCStats, but with the
 * current directory structure, it can't see GCExtensions
 * @todo Maybe this should be a virtual call, to allow subclasses to
 * collect the stats they require
 */
void
MM_Collector::recordExcessiveStatsForGCStart(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	extensions->excessiveGCStats.gcCount += 1;

	/* Record the heap free so we can calculate how much we've actually freed in the collect */
	extensions->excessiveGCStats.freeMemorySizeBefore = extensions->heap->getActualActiveFreeMemorySize();

	/* Record the start time of the GC */
	extensions->excessiveGCStats.startGCTimeStamp = omrtime_hires_clock();
}

/**
 * Record statistics (stored in MM_ExcessiveGCStats) at the end of a GC.
 * @todo This code probably belongs in MM_ExcessiveGCStats, but with the
 * current directory structure, it can't see GCExtensions
 * @todo Maybe this should be a virtual call, to allow subclasses to
 * collect the stats they require
 */
void
MM_Collector::recordExcessiveStatsForGCEnd(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	MM_ExcessiveGCStats* stats = &extensions->excessiveGCStats;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	/* Record the end time of the GC */
	stats->endGCTimeStamp = omrtime_hires_clock();

	/* We use the approximate value here because we want to be sure the math believes we are actually
	 * doing sufficient freeing work to not decide there is excessive GC'ing going on.
	 */
	stats->freeMemorySizeAfter = extensions->heap->getApproximateActiveFreeMemorySize();

	/* now calculate weighted average ratio */
	/* (protect from malicious clock jitters) */
	if (stats->endGCTimeStamp > stats->startGCTimeStamp) {
		/* Tally the time spent gc'ing (both local and global gcs) */
		stats->totalGCTime += (uint64_t)omrtime_hires_delta(stats->startGCTimeStamp, stats->endGCTimeStamp, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	}

	if (stats->endGCTimeStamp > stats->lastEndGlobalGCTimeStamp) {
		/* The issue with doing the weighted average on the global collect is that the
		 * new space collections can only change the ratio by is the maximum of the
		 * weight.  Given that in a generational system, you are likely to experience
		 * many more new space collects, this would make the excessive gc detection
		 * be very, very slow.
		 *
		 * By keeping the % at every GC, but only doing detection on the global,
		 * what we say is "Yes, we may be spending too much time in gc, but we
		 * won't really make a decision until after a global" - which give the global
		 * a chance to free up a lot of memory, increasing the free % above the
		 * threshold, and therefore not trigger an excessive GC.
		 */
		stats->newGCToUserTimeRatio = (float)((int64_t)stats->totalGCTime * 100.0 / (int64_t)omrtime_hires_delta(stats->lastEndGlobalGCTimeStamp, stats->endGCTimeStamp, OMRPORT_TIME_DELTA_IN_MICROSECONDS));
		stats->avgGCToUserTimeRatio = MM_Math::weightedAverage(stats->avgGCToUserTimeRatio, stats->newGCToUserTimeRatio, extensions->excessiveGCnewRatioWeight);
	}
}


/**
 * Perform any collector setup activities.
 * @param subSpace the memory subspace where the collection is occurring
 * @param systemGC True if this is a system GC; FALSE otherwise
 * @param aggressive True if this is an aggressive collect, otherwise false
 */
void
MM_Collector::preCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, MM_AllocateDescription* allocDescription, uint32_t gcCode)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	/* Record the master GC thread CPU time at the start to diff later */
	_masterThreadCpuTimeStart = omrthread_get_self_cpu_time(env->getOmrVMThread()->_os_thread);

	_cli->doFrequentObjectAllocationSampling(env);

	_bytesRequested = (allocDescription ? allocDescription->getBytesRequested() : 0);

	internalPreCollect(env, subSpace, allocDescription, gcCode);

	extensions->aggressive = (env->getCycleStateGCCode().isAggressiveGC() ? 1 : 0);

	_isRecursiveGC = extensions->isRecursiveGC;

	/* ExcessiveGC is tested when a global gc occurs.  The global gc may occur as a result
	 * of a local gc.  We use extensions->didGlobalGC to track if a global gc occured.
	 * If this is the outter most garbage collect invocation then set the global gc flag to	false.
	 */
	if (!_isRecursiveGC) {
		extensions->didGlobalGC = false;

		/* In case of recursive GC calls, we want to time only the outermost GC invocation
		 * Only time implicit GCs -- system GCs are accounted for "user" time in GC/total
		 * time ratio calculation
		 */
		if (!env->getCycleStateGCCode().isExplicitGC()) {
			recordExcessiveStatsForGCStart(env);
			/* Inner invocations will see the flag as true */
			extensions->isRecursiveGC = true;
		}
	}

	/* If this is a global collection then set the globalgc flag.  This will allow us to
	 * trigger excessiveGC checks from a local collection that has recursed into a global gc .
	 */
	if (_globalCollector) {
		extensions->didGlobalGC = true;
	}
}

/**
 * Post collection broadcast event, indicating that the collection has been completed.
 * @param subSpace the memory subspace where the collection occurred
 */
void
MM_Collector::postCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	/* Calculate the master GC thread CPU time. Do this immediately
	 * so the information will be available to cycle end hooks.
	 */
	uint64_t masterThreadCpuTime = omrthread_get_self_cpu_time(env->getOmrVMThread()->_os_thread);
	masterThreadCpuTime -= _masterThreadCpuTimeStart;
	extensions->_masterThreadCpuTimeNanos += masterThreadCpuTime;

	internalPostCollect(env, subSpace);

	extensions->bytesAllocatedMost = 0;
	extensions->vmThreadAllocatedMost = NULL;

	if (!_isRecursiveGC) {
		bool excessiveGCDetected = false;

		if (!env->getCycleStateGCCode().isExplicitGC()) {
			/* Outermost invocation restores the flag */
			extensions->isRecursiveGC = false;
			recordExcessiveStatsForGCEnd(env);

			if (extensions->excessiveGCEnabled._valueSpecified) {
				excessiveGCDetected = _cli->checkForExcessiveGC(env, this);
			}
		}

		/* We check gc time vs user time on intervals between the ends of global GCs:
		 * ulg*ulululg* (g=global; l=local; u=user; we sum the gc times between the *s and compare
		 * against the total time between the *s.)
		 * We need to clear the accumulated gc times and track the end of this gc if a global collection
		 * occured, in order to adhere to the interval above.
		 * This needs to be done here to reset the stats when a system gc has occured.
		 */
		if (extensions->didGlobalGC) {
			extensions->excessiveGCStats.totalGCTime = 0;
			extensions->excessiveGCStats.lastEndGlobalGCTimeStamp = extensions->excessiveGCStats.endGCTimeStamp;
		}

		/* Set the excessive GC state, whether it was an implicit or system GC */
		setThreadFailAllocFlag(env, excessiveGCDetected);
	}
}

/**
 * Perform a garbage collection.
 * This method is called for both implicit (caused by an allocation failure)
 * and explicit (system GC) collections.
 *
 * @param subSpace the memory subspace where the collection is occurring
 * @param allocDescription if non-NULL, contains information about the allocation
 * which triggered the GC. If NULL, the GC is a system GC.
 *
 */
void*
MM_Collector::garbageCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* callingSubSpace, MM_AllocateDescription* allocateDescription, uint32_t gcCode, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_AllocationContext* context)
{
	Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

	Assert_MM_true(NULL == env->_cycleState);
	preCollect(env, callingSubSpace, allocateDescription, gcCode);
	Assert_MM_true(NULL != env->_cycleState);

	/* ensure that we aren't trying to collect while in a NoGC allocation */
	Assert_MM_false(env->_isInNoGCAllocationCall);

	uintptr_t vmState = env->pushVMstate(getVMStateID());

	/* First do any pre-collection initialization of the collector*/
	setupForGC(env);

	/* perform the collection */
	_gcCompleted = internalGarbageCollect(env, callingSubSpace, allocateDescription);

	env->popVMstate(vmState);

	/* now, see if we need to resume an allocation or replenishment attempt */
	void* postCollectAllocationResult = NULL;
	if (NULL != allocateDescription) {
		MM_MemorySubSpace::AllocationType allocationType = allocateDescription->getAllocationType();
		allocateDescription->restoreObjects(env);
		if (NULL != context) {
			/* replenish this context */
			postCollectAllocationResult = baseSubSpace->lockedReplenishAndAllocate(env, context, objectAllocationInterface, allocateDescription, allocationType);
		} else if (NULL != baseSubSpace) {
			/* try allocating in this subspace. indicate that this thread just did a GC and is ok to try a parent,
			 * if current subspace fails to satisfy */
			allocateDescription->setClimb();
			postCollectAllocationResult = callingSubSpace->allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace);
		}
		allocateDescription->saveObjects(env);
	}
	/* finally, run postCollect */
	postCollect(env, callingSubSpace);
	Assert_MM_true(NULL != env->_cycleState);
	env->_cycleState = NULL;

	return postCollectAllocationResult;
}

/**
 * Sets excessive GC state
 */
void
MM_Collector::setThreadFailAllocFlag(MM_EnvironmentBase* env, bool flag)
{
	OMR_VMThread* vmThread = NULL;

	GC_OMRVMThreadListIterator threadListIterator(env->getOmrVM());
	while ((vmThread = threadListIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentBase::getEnvironment(vmThread)->_failAllocOnExcessiveGC = flag;
	}
}

bool
MM_Collector::isTimeForGlobalGCKickoff()
{
	Assert_MM_unreachable();
	return false;
}

/**
 * Abort any currently active garbage collection activity.
 * The abort consists of halting any activity related to garbage collection, and resetting/releasing said
 * work such that a resumption would be the equivalent of a fresh start.  All hidden references to heap objects
 * should be flushed/released at this stage.
 */
void
MM_Collector::abortCollection(MM_EnvironmentBase* env, CollectionAbortReason reason)
{
	Assert_MM_unreachable();
}

/**
 * Perform any collector initialization particular to the concurrent collector.
 */
bool
MM_Collector::collectorStartup(MM_GCExtensionsBase* extensions)
{
	Assert_MM_unreachable();
	return true;
}

/**
 * Perform any collector shutdown particular to the concurrent collector.
 * Currently this just involves stopping the concurrent background helper threads.
 */
void
MM_Collector::collectorShutdown(MM_GCExtensionsBase* extensions)
{
	Assert_MM_unreachable();
}

bool
MM_Collector::isMarked(void *objectPtr)
{
	Assert_MM_unreachable();
	return false;
}
