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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base_Core
 */

#include <string.h>

#include "omrcfg.h"

#include "TLHAllocationInterface.hpp"

#include "omrport.h"
#include "ModronAssertions.h"

#include "AllocateDescription.hpp"
#include "AllocationContext.hpp"
#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "FrequentObjectsStats.hpp"
#include "GCExtensionsBase.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Create and return a new instance of MM_TLHAllocationInterface.
 * 
 * @return the new instance, or NULL on failure.
 */
MM_TLHAllocationInterface *
MM_TLHAllocationInterface::newInstance(MM_EnvironmentBase *env)
{
	MM_TLHAllocationInterface *allocationInterface;

	allocationInterface = (MM_TLHAllocationInterface *)env->getForge()->allocate(sizeof(MM_TLHAllocationInterface), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != allocationInterface) {
		new(allocationInterface) MM_TLHAllocationInterface(env);
		if (!allocationInterface->initialize(env)) {
			allocationInterface->kill(env);
			return NULL;
		}
	}
	return allocationInterface;
}

/**
 * One time initialization of the receivers state.
 * @return true on successful initialization, false otherwise.
 */
bool
MM_TLHAllocationInterface::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	bool result = true;

	Assert_MM_true(NULL == _frequentObjectsStats);

	if (extensions->doFrequentObjectAllocationSampling){
		_frequentObjectsStats = MM_FrequentObjectsStats::newInstance(env);
		result = (NULL != _frequentObjectsStats);
	}

	if (result) {
		reconnect(env, false);
	}

	return result;
}

/**
 * Internal clean up of the receivers state and resources.
 */
void
MM_TLHAllocationInterface::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _frequentObjectsStats) {
		_frequentObjectsStats->kill(env);
		_frequentObjectsStats = NULL;
	}
}

/**
 * Clean up all state and resources held by the receiver.
 */
void
MM_TLHAllocationInterface::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Reconnect the cache to the owning environment.
 * The environment is either newly created or has had its properties changed such that the existing cache is
 * no longer valid.  An example of this is a change of memory space.  Perform the necessary flushing and
 * re-initialization of the cache details such that new allocations will occur in the correct fashion.
 * 
 * @param shouldFlush determines whether the existing cache information should be flushed back to the heap.
 */
 void
 MM_TLHAllocationInterface::reconnect(MM_EnvironmentBase *env, bool shouldFlush)
{
	 MM_GCExtensionsBase *extensions = env->getExtensions();

	 if(shouldFlush) {
		extensions->allocationStats.merge(&_stats);
		_stats.clear();
		/* Since AllocationStats have been reset, reset the base */
		_bytesAllocatedBase = 0;
	}
	
	_tlhAllocationSupport.reconnect(env, shouldFlush);

#if defined(OMR_GC_NON_ZERO_TLH)
	_tlhAllocationSupportNonZero.reconnect(env, shouldFlush);
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
};


/**
 * Attempt to allocate an object in this TLH.
 */
void *
MM_TLHAllocationInterface::allocateFromTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool shouldCollectOnFailure)
{
	void *result = NULL;

#if defined(OMR_GC_NON_ZERO_TLH)
	/* If requested to skip zero init (NON_ZERO_TLH flag set), and we are in dual TLH (default TLH is batch cleared),
	 * we have to redirect this allocation to the non zero TLH */
	if (allocDescription->getNonZeroTLHFlag()) {
		result = _tlhAllocationSupportNonZero.allocateFromTLH(env, allocDescription, shouldCollectOnFailure);
	} else
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
	{
		result = _tlhAllocationSupport.allocateFromTLH(env, allocDescription, shouldCollectOnFailure);
	}
	
	return result;
};

void *
MM_TLHAllocationInterface::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	void *result = NULL;
	MM_AllocationContext *ac = env->getAllocationContext();
	_bytesAllocatedBase = _stats.bytesAllocated();

	if (NULL != ac) {
		/* ensure that we are allowed to use the AI in this configuration in the Tarok case */
		/* allocation contexts currently aren't supported with generational schemes */
		Assert_MM_true(memorySpace->getTenureMemorySubSpace() == memorySpace->getDefaultMemorySubSpace());
	}

	/* Record the memory space from which the allocation takes place in the AD */
	allocDescription->setMemorySpace(memorySpace);
	if (allocDescription->getTenuredFlag()) {
		Assert_MM_true(shouldCollectOnFailure);
		MM_AllocationContext *cac = env->getCommonAllocationContext();
		if (NULL != cac) {
			result = cac->allocateObject(env, allocDescription, shouldCollectOnFailure);
		} else if (NULL != ac) {
			result = ac->allocateObject(env, allocDescription, shouldCollectOnFailure);
		} else {
			MM_MemorySubSpace *subspace = memorySpace->getTenureMemorySubSpace();
			result = subspace->allocateObject(env, allocDescription, NULL, NULL, shouldCollectOnFailure);
		}
	} else {
		result = allocateFromTLH(env, allocDescription, shouldCollectOnFailure);

		if (NULL == result) {
			if (NULL != ac) {
				result = ac->allocateObject(env, allocDescription, shouldCollectOnFailure);
			} else {
				result = memorySpace->getDefaultMemorySubSpace()->allocateObject(env, allocDescription, NULL, NULL, shouldCollectOnFailure);
			}
		}

	}

	if ((NULL != result) && !allocDescription->isCompletedFromTlh()) {
#if defined(OMR_GC_OBJECT_ALLOCATION_NOTIFY)
		MM_GCExtensionsBase *extensions = env->getExtensions();
		MM_CollectorLanguageInterface * cli = extensions->collectorLanguageInterface;
		cli->objectAllocationNotify(env, (omrobjectptr_t)result);
#endif /* OMR_GC_OBJECT_ALLOCATION_NOTIFY */
		_stats._allocationBytes += allocDescription->getContiguousBytes();
		_stats._allocationCount += 1;

	}

	env->_oolTraceAllocationBytes += (_stats.bytesAllocated() - _bytesAllocatedBase); /* Increment by bytes allocated */

	return result;
}

void *
MM_TLHAllocationInterface::allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	return allocateObject(env, allocateDescription, memorySpace, shouldCollectOnFailure);
}

#if defined(OMR_GC_ARRAYLETS)
void *
MM_TLHAllocationInterface::allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	return allocateObject(env, allocateDescription, memorySpace, shouldCollectOnFailure);
}
void *
MM_TLHAllocationInterface::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	void *result = NULL;
	MM_AllocationContext *ac = env->getAllocationContext();
	MM_AllocationContext *cac = env->getCommonAllocationContext();
	
	if((NULL != cac) && (allocateDescription->getTenuredFlag())) {
		result = cac->allocateArrayletLeaf(env, allocateDescription, shouldCollectOnFailure);
	} else if (NULL != ac) {
		/* ensure that we are allowed to use the AI in this configuration in the Tarok case */
		/* allocation contexts currently aren't supported with generational schemes */
		Assert_MM_true(memorySpace->getTenureMemorySubSpace() == memorySpace->getDefaultMemorySubSpace());
		result = ac->allocateArrayletLeaf(env, allocateDescription, shouldCollectOnFailure);
	} else {
		result = memorySpace->getDefaultMemorySubSpace()->allocateArrayletLeaf(env, allocateDescription, NULL, NULL, shouldCollectOnFailure);
	}

	if (NULL != result) {
		_stats._arrayletLeafAllocationBytes += env->getOmrVM()->_arrayletLeafSize;
		_stats._arrayletLeafAllocationCount += 1;
	}

	return result;
}
#endif /* OMR_GC_ARRAYLETS */

/**
 * Replenish the allocation interface TLH cache with new storage.
 * This is a placeholder function for all non-TLH implementing configurations until a further revision of the code finally pushes TLH
 * functionality down to the appropriate level, and not so high that all configurations must recognize it.
 * For this implementation we simply redirect back to the supplied memory pool and if successful, populate the localized TLH and supplied
 * AllocationDescription with the appropriate information.
 * @return true on successful TLH replenishment, false otherwise.
 */
void *
MM_TLHAllocationInterface::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *memorySubSpace, MM_MemoryPool *memoryPool)
{
	void *result = NULL;

#if defined(OMR_GC_NON_ZERO_TLH)
	if (allocDescription->getNonZeroTLHFlag()) {
		result = _tlhAllocationSupportNonZero.allocateTLH(env, allocDescription, memorySubSpace, memoryPool);
	} else
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
	{
		result = _tlhAllocationSupport.allocateTLH(env, allocDescription, memorySubSpace, memoryPool);
	}

	return result;
}

void
MM_TLHAllocationInterface::flushCache(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	
#if defined(OMR_GC_THREAD_LOCAL_HEAP)	
	if (!_owningEnv->isInlineTLHAllocateEnabled()) {
		/* Clear out realHeapAlloc field; tlh code below will take care of rest */
		_owningEnv->enableInlineTLHAllocate();
	}	
#endif /* OMR_GC_THREAD_LOCAL_HEAP */		
	
	extensions->allocationStats.merge(&_stats);
	_stats.clear();
	/* Since AllocationStats have been reset, reset the base as well*/
	_bytesAllocatedBase = 0;
	_tlhAllocationSupport.flushCache(env);

#if defined(OMR_GC_NON_ZERO_TLH)
	_tlhAllocationSupportNonZero.flushCache(env);
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
}

void
MM_TLHAllocationInterface::reconnectCache(MM_EnvironmentBase *env)
{
	uintptr_t vmState;
	
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	if (!_owningEnv->isInlineTLHAllocateEnabled()) {
		/* Only need to reset heapAlloc and realHeapAlloc if there is an existing TLH to reset */
		_owningEnv->enableInlineTLHAllocate();
	}	
#endif /* OMR_GC_THREAD_LOCAL_HEAP */
	
	vmState = env->pushVMstate(J9VMSTATE_GC_TLH_RESET);

	reconnect(env, true);

	env->popVMstate(vmState);
}

void
MM_TLHAllocationInterface::restartCache(MM_EnvironmentBase *env)
{
	_tlhAllocationSupport.restart(env);

#if defined(OMR_GC_NON_ZERO_TLH)
	_tlhAllocationSupportNonZero.restart(env);
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
}

#endif /* OMR_GC_THREAD_LOCAL_HEAP */
