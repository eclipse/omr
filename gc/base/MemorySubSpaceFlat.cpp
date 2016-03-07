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
#include "modronopt.h"
#include "ModronAssertions.h"

#include "MemorySubSpaceFlat.hpp"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "PhysicalSubArena.hpp"

/**
 * Zos390 Platform dependent trickery for the ar command.  Without this definition
 * the ar command on Zos390 will fail to link this class.
 */
#if defined(J9ZOS390)
#include "ZOSLinkage.hpp"
int j9zos390LinkTrickMemorySubSpaceFlat;
#endif /* J9ZOS390 */


/****************************************
 * Allocation
 ****************************************
 */

/**
 * 
 * @param allocDescription
 * @return Address of allocated object or NULL if no storage available
 */

void*
MM_MemorySubSpaceFlat::allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure)
{
	void* result = NULL;

	if (shouldCollectOnFailure) {
		result = _memorySubSpace->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
	} else {
		/* If request came from parent, forward the failure handling to the child first */
		if (previousSubSpace == _parent) {
			result = _memorySubSpace->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		}
	}
	return result;
}

void*
MM_MemorySubSpaceFlat::allocationRequestFailed(MM_EnvironmentBase* env, MM_AllocateDescription* allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace)
{
	void* addr = NULL;

	/* If the request came from the parent, forward the failure handling to the child first */
	if (previousSubSpace == _parent) {
		addr = _memorySubSpace->allocationRequestFailed(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace, this);
		if (NULL != addr) {
			return addr;
		}
	}

	/* If there is a collector present, execute and retry the failure on the child */
	if (_collector) {
		allocateDescription->saveObjects(env);
		/* acquire exclusive access and, after we get it, see if we need to perform a collect or if someone else beat us to it */
		if (!env->tryAcquireExclusiveVMAccessForGC(_collector)) {
			allocateDescription->restoreObjects(env);
			/* Beaten to exclusive access for our collector by another thread - a GC must have occurred.  This thread
			 * does NOT have exclusive access at this point.  Try and satisfy the allocate based on a GC having occurred.
			 */
			addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, _memorySubSpace);
			if (NULL != addr) {
				return addr;
			}

			/* Failed to satisfy allocate - now really go for a GC */
			allocateDescription->saveObjects(env);
			/* acquire exclusive access and, after we get it, see if we need to perform a collect or if someone else beat us to it */
			if (!env->acquireExclusiveVMAccessForGC(_collector)) {
				/* we have exclusive access but another thread beat us to the GC so see if they collected enough to satisfy our request */
				allocateDescription->restoreObjects(env);
				addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, _memorySubSpace);
				if (NULL != addr) {
					/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
					 * an event for tracing / verbose to report the occurrence.
					 */
					reportAcquiredExclusiveToSatisfyAllocate(env, allocateDescription);
					return addr;
				}

				/* we still failed the allocate so try a resize to get more space */
				reportAllocationFailureStart(env, allocateDescription);
				performResize(env, allocateDescription);
				addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace);

				if (addr) {
					/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
					 * an event for tracing / verbose to report the occurrence.
					 */
					reportAcquiredExclusiveToSatisfyAllocate(env, allocateDescription);
					reportAllocationFailureEnd(env);
					return addr;
				}
				allocateDescription->saveObjects(env);
				/* we still failed so we will need to run a GC */
			} else {
				/* we have exclusive and no other thread beat us to it so we can now collect */
				reportAllocationFailureStart(env, allocateDescription);
			}
		} else {
			/* we have exclusive and no other thread beat us to it so we can now collect */
			reportAllocationFailureStart(env, allocateDescription);
		}

		Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

		/* run the collector in the default mode (ie:  not explicitly aggressive) */
		allocateDescription->setAllocationType(allocationType);
		addr = _collector->garbageCollect(env, this, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_DEFAULT, objectAllocationInterface, baseSubSpace, NULL);
		allocateDescription->restoreObjects(env);

		if (addr) {
			reportAllocationFailureEnd(env);
			return addr;
		}

		allocateDescription->saveObjects(env);
		/* The collect wasn't good enough to satisfy the allocate so attempt an aggressive collection */
		addr = _collector->garbageCollect(env, this, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE, objectAllocationInterface, baseSubSpace, NULL);
		allocateDescription->restoreObjects(env);

		reportAllocationFailureEnd(env);

		if (addr) {
			return addr;
		}
		/* there was nothing we could do to satisfy the allocate at this level (we will either OOM or attempt a collect at the higher level in our parent) */
	}

	/* If the caller was the child, forward the failure notification to the parent for handling */
	if ((NULL != _parent) && (previousSubSpace != _parent)) {
		/* see if the parent can find us some space */
		return _parent->allocationRequestFailed(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace, this);
	}

	/* Nothing else to try - fail */
	return NULL;
}

#if defined(OMR_GC_ARRAYLETS)
void*
MM_MemorySubSpaceFlat::allocateArrayletLeaf(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure)
{

	void* result = NULL;

	if (shouldCollectOnFailure) {
		result = _memorySubSpace->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
	} else {
		/* If request came from parent, forward the failure handling to the child first */
		if (previousSubSpace == _parent) {
			result = _memorySubSpace->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		}
	}
	return result;
}
#endif /* OMR_GC_ARRAYLETS */

/**
 * Return the allocation failure stats for this subSpace.
 * When scavenger enabled there is no collector for this subSpace, must ask the
 * parent for the stats.
 * @return Addres of allocation failure statistics
 */
MM_AllocationFailureStats*
MM_MemorySubSpaceFlat::getAllocationFailureStats()
{
	if (_collector) {
		return MM_MemorySubSpace::getAllocationFailureStats();
	}

	return _parent->getAllocationFailureStats();
}

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * 
 * @param allocDescription
 * @param tlh
 * 
 * @return TRUE if TLH allocated; FALSE otherwise
 */
void*
MM_MemorySubSpaceFlat::allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure)
{
	if (shouldCollectOnFailure) {
		return _memorySubSpace->allocateTLH(env, allocDescription, objectAllocationInterface, baseSubSpace, this, shouldCollectOnFailure);
	} else {
		if (previousSubSpace == _parent) {
			/* Retry the allocate - should succeed unless heap fragmented */
			return _memorySubSpace->allocateTLH(env, allocDescription, objectAllocationInterface, baseSubSpace, this, shouldCollectOnFailure);
		}
		return NULL;
	}
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

/****************************************
 * Internal Allocation
 ****************************************
 */

/**
 * 
 * @param addrBase
 * @param addrTop
 * 
 */
void
MM_MemorySubSpaceFlat::abandonHeapChunk(void* addrBase, void* addrTop)
{
}

/****************************************
 * Sub Space finding
 ****************************************
 */

/**
 * Return default meory subspace.
 * 
 * @return Address of default memory subspace
 */
MM_MemorySubSpace*
MM_MemorySubSpaceFlat::getDefaultMemorySubSpace()
{
	return getChildSubSpace();
}

/**
 * Return tenure memory subspace
 * 
 * @return Address of tenure memory subspace
 */
MM_MemorySubSpace*
MM_MemorySubSpaceFlat::getTenureMemorySubSpace()
{
	return getChildSubSpace();
}

/**
 * Initialization
 */
MM_MemorySubSpaceFlat*
MM_MemorySubSpaceFlat::newInstance(
	MM_EnvironmentBase* env, MM_PhysicalSubArena* physicalSubArena, MM_MemorySubSpace* childMemorySubSpace,
	bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize,
	uintptr_t memoryType, uint32_t objectFlags)
{
	MM_MemorySubSpaceFlat* memorySubSpace;

	memorySubSpace = (MM_MemorySubSpaceFlat*)env->getForge()->allocate(sizeof(MM_MemorySubSpaceFlat), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memorySubSpace) {
		new (memorySubSpace) MM_MemorySubSpaceFlat(env, physicalSubArena, childMemorySubSpace, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryType, objectFlags);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	}
	return memorySubSpace;
}

bool
MM_MemorySubSpaceFlat::initialize(MM_EnvironmentBase* env)
{
	if (!MM_MemorySubSpace::initialize(env)) {
		return false;
	}

	/* attach the child to the hierarchy */
	registerMemorySubSpace(_memorySubSpace);

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	if (env->getExtensions()->concurrentMark) {
		setConcurrentCollectable();

		MM_MemorySubSpace* child = getChildren();
		while (child) {
			child->setConcurrentCollectable();
			child = child->getNext();
		}
	}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	return true;
}

/**
 * Adjust the specified expansion amount by the specified user increment amount (i.e. -Xmoi)
 * @return the updated expand size
 */
uintptr_t
MM_MemorySubSpaceFlat::adjustExpansionWithinUserIncrement(MM_EnvironmentBase* env, uintptr_t expandSize)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (extensions->allocationIncrementSetByUser) {
		uintptr_t expandIncrement = extensions->allocationIncrement;

		/* increment of 0 means no expansion is to occur. Don't round  to a size of 0 */
		if (0 == expandIncrement) {
			return expandSize;
		}

		/* Round to the Xmoi value */
		return MM_Math::roundToCeiling(expandIncrement, expandSize);
	}

	return MM_MemorySubSpace::adjustExpansionWithinUserIncrement(env, expandSize);
}

/**
 * Determine the maximum expansion amount the memory subspace can expand by.
 * The amount returned is restricted by values within the receiver of the call, as well as those imposed by
 * the parents of the receiver and the owning MemorySpace of the receiver.
 * 
 * @return the amount by which the receiver can expand
 */
uintptr_t
MM_MemorySubSpaceFlat::maxExpansionInSpace(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t expandIncrement = extensions->allocationIncrement;
	uintptr_t maxExpandAmount;

	if (extensions->allocationIncrementSetByUser) {
		/* increment of 0 means no expansion */
		if (0 == expandIncrement) {
			return 0;
		}
	}

	maxExpandAmount = MM_MemorySubSpace::maxExpansionInSpace(env);

	return maxExpandAmount;
}

/**
 * Handle the initial heap expansion during startup.
 */
bool
MM_MemorySubSpaceFlat::expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, MM_HeapRegionDescriptor* region, bool canCoalesce)
{
	/* Inform the child */
	return _memorySubSpace->expanded(env, subArena, region, canCoalesce);
}

/**
 * Memory described by the range has been added to the heap and been made available to the subspace as free memory.
 */
bool
MM_MemorySubSpaceFlat::expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce)
{
	/* Inform the child */
	return _memorySubSpace->expanded(env, subArena, size, lowAddress, highAddress, canCoalesce);
}

/**
 * Get the size of heap available for contraction.
 * Return the amount of heap available to be contracted, factoring in any potential allocate that may require the
 * available space.
 * @return The amount of heap available for contraction factoring in the size of the allocate (if applicable)
 */
uintptr_t
MM_MemorySubSpaceFlat::getAvailableContractionSize(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	return _physicalSubArena->getAvailableContractionSize(env, _memorySubSpace, allocDescription);
}

/**
 * Expand the heap during a collection by the amount specified.
 * Generational style collectors, which move objects from one memory subspace to another
 * during collection, may require that the destination area expand during the collection 
 * cycle.  An example of this would be the tenure area in a 2-generational system, if the
 * tenure area memory has been exhausted.
 * @note This call is not protected by any locking mechanism.
 */
uintptr_t
MM_MemorySubSpaceFlat::collectorExpand(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t expansionAmount;
	uintptr_t expandSize;

	Trc_MM_MemorySubSpaceFlat_collectorExpand_Entry(env->getLanguageVMThread(), requestCollector, allocDescription->getBytesRequested());

	/* Determine the amount to expand the heap */
	/* TODO: When this turns to %s, getActiveMemorySize() should be used to help determine the expand size */
	expandSize = calculateCollectorExpandSize(env, requestCollector, allocDescription);

	/* Does the collector think this subspace can expand? */
	if (!requestCollector->canCollectorExpand(env, this, expandSize)) {
		/* Not allowed */
		Trc_MM_MemorySubSpaceFlat_collectorExpand_Exit2(env->getLanguageVMThread());
		return 0;
	}

	extensions->heap->getResizeStats()->setLastExpandReason(SATISFY_COLLECTOR);

	/* ...and expand */
	expansionAmount = expand(env, expandSize);

	/* Inform the requesting collector that an expand attempt took place (even if the expansion failed) */
	requestCollector->collectorExpanded(env, this, expansionAmount);

	Trc_MM_MemorySubSpaceFlat_collectorExpand_Exit3(env->getLanguageVMThread(), expansionAmount);
	return expansionAmount;
}
