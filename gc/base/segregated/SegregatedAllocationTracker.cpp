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

#include "omrcomp.h"
#include "omrport.h"

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

#include "SegregatedAllocationTracker.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

MM_SegregatedAllocationTracker*
MM_SegregatedAllocationTracker::newInstance(MM_EnvironmentBase *env, volatile uintptr_t *globalBytesInUse, uintptr_t flushThreshold)
{
	MM_SegregatedAllocationTracker* allocationTracker;
	allocationTracker = (MM_SegregatedAllocationTracker*)env->getForge()->allocate(sizeof(MM_SegregatedAllocationTracker), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(NULL != allocationTracker) {
		new(allocationTracker) MM_SegregatedAllocationTracker(env);
		if(!allocationTracker->initialize(env, globalBytesInUse, flushThreshold)) {
			allocationTracker->kill(env);
			return NULL;
		}
	}
	return allocationTracker;
}

void
MM_SegregatedAllocationTracker::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_SegregatedAllocationTracker::initialize(MM_EnvironmentBase *env, uintptr_t volatile *globalBytesInUse, uintptr_t flushThreshold)
{
	_bytesAllocated = 0;
	_flushThreshold = flushThreshold;
	_globalBytesInUse = globalBytesInUse;
	updateAllocationTrackerThreshold(env);
	return true;
}

void
MM_SegregatedAllocationTracker::tearDown(MM_EnvironmentBase *env)
{
	/* This is used to flush the bytes allocated left in the allocation tracker
	 * due to the allocation tracker flush threshold. It also updates the flush threshold if necessary.
	 */
	flushBytes();
	updateAllocationTrackerThreshold(env);
}

void
MM_SegregatedAllocationTracker::updateAllocationTrackerThreshold(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t perThreadFlushThreshold = extensions->allocationTrackerMaxTotalError;
	
	if (extensions->currentEnvironmentCount > 0) {
		perThreadFlushThreshold /= extensions->currentEnvironmentCount;
	}
	
	extensions->allocationTrackerFlushThreshold = OMR_MIN(perThreadFlushThreshold, extensions->allocationTrackerMaxThreshold);
}

/**
 * Should be called once Xmx has been set in GCExtensions
 */
void
MM_SegregatedAllocationTracker::initializeGlobalAllocationTrackerValues(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions =  env->getExtensions();
	
	/* Only set the allocation tracker max total error if it hasn't been specified on the command line */
	if (UDATA_MAX == extensions->allocationTrackerMaxTotalError) {
		extensions->allocationTrackerMaxTotalError = extensions->memoryMax / 100; /* 1% of -Xmx */
	}
	
	updateAllocationTrackerThreshold(env);
}

void
MM_SegregatedAllocationTracker::addBytesAllocated(MM_EnvironmentBase *env, uintptr_t bytesAllocated)
{
	_bytesAllocated += bytesAllocated;
	if (_bytesAllocated > 0) {
		if ((uintptr_t)_bytesAllocated > _flushThreshold) {
			flushBytes();
		}
	}
}

void
MM_SegregatedAllocationTracker::addBytesFreed(MM_EnvironmentBase *env, uintptr_t bytesFreed)
{
	_bytesAllocated -= bytesFreed;
	if (_bytesAllocated < 0) {
		if ((uintptr_t)(_bytesAllocated * -1) > _flushThreshold) {
			flushBytes();
		}
	}
}

/**
 * Atomically adds this thread's bytes in use to the global memory pool's bytes in use variable used to obtain the current free space approximation.
 */
void
MM_SegregatedAllocationTracker::flushBytes()
{
	MM_AtomicOperations::add(_globalBytesInUse, _bytesAllocated);
	_bytesAllocated = 0;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
