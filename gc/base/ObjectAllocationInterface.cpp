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
 * @ingroup GC_Base_Core
 */

#include "ObjectAllocationInterface.hpp"

#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Replenish the allocation interface TLH cache with new storage.
 * This is a placeholder function for all non-TLH implementing configurations until a further revision of the code finally pushes TLH
 * functionality down to the appropriate level, and not so high that all configurations must recognize it.
 * @return true on successful TLH replenishment, false otherwise.
 */
void *
MM_ObjectAllocationInterface::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *memorySubSpace, MM_MemoryPool *memoryPool)
{
	assume0(0);  /* Temporary routine */
	return NULL;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

/**
 * Purge any cached heap memory for object allocation from the interface.
 * For allocation interfaces that keep caches of heap memory from which to allocate (e.g., TLH), release
 * the memory back to the heap.  This does not imply that the memory returned will be available for immediate
 * allocation (i.e., added back to any free lists).  It does imply that the memory will be properly formed
 * within the heap and suitable for a full heap walk (not all allocation schemes require this).
 * 
 * @note The calling environment may not be the owning environment of the receiver.
 */
void
MM_ObjectAllocationInterface::flushCache(MM_EnvironmentBase *env)
{
	/* Do nothing */
}

/**
 * Reconnect any cached heap allocation system against the owning Environment.
 * When an Environment has its memory space switched or some configuration in the system changes such that
 * the caching system must re-evaluate how it performs its allocations, this method must be called.  This call
 * implies a flushCache() call as well.
 * 
 * @note The calling environment may not be the owning environment of the receiver.
 */ 
void
MM_ObjectAllocationInterface::reconnectCache(MM_EnvironmentBase *env)
{
	/* Do nothing */
}

/**
 * Restart the cache from its current start to an appropriate base state.
 * Reset the cache details back to a starting state that is appropriate for where it currently is.
 * This is typically used following a collection.
 * 
 * @note The calling environment may not be the owning environment of the receiver.
 * @note The previous cache state is expected to have been flushed back to the heap. 
 */
void
MM_ObjectAllocationInterface::restartCache(MM_EnvironmentBase *env)
{
	/* Do nothing */
}
