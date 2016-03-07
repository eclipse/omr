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

#include <string.h>

#include "omrcomp.h"
#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "NonVirtualMemory.hpp"

/**
 * Create and initialize a new instance of NonVirtualMemory.
 * @return Pointer to a NonVirtualMemory object if initialisation successful, NULL otherwise
 */
MM_NonVirtualMemory*
MM_NonVirtualMemory::newInstance(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t size, uint32_t memoryCategory)
{
	MM_NonVirtualMemory* vmem = (MM_NonVirtualMemory*)env->getForge()->allocate(sizeof(MM_NonVirtualMemory), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (vmem) {
		new (vmem) MM_NonVirtualMemory(env, heapAlignment);
		if (!vmem->initialize(env, size, NULL, NULL, 0, memoryCategory)) {
			vmem->kill(env);
			vmem = NULL;
		}
	}
	return vmem;
}

#if (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064)
/**
 * Allocate a chunk of memory.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory.
 */
void*
MM_NonVirtualMemory::reserveMemory(J9PortVmemParams* params)
{
	uintptr_t bytesToAllocate = params->byteAmount;
	/* should be params->alignmentInBytes but that requires a change in Port to go through, first, so use _heapAlignment for now */
	uintptr_t alignment = _heapAlignment;
	if (alignment > 0) {
		bytesToAllocate += (alignment - 1);
	}
	_baseAddress = _extensions->getForge()->allocate(bytesToAllocate, MM_AllocationCategory::JAVA_HEAP, OMR_GET_CALLSITE());
	void* addressToReturn = _baseAddress;
	if ((NULL != _baseAddress) && (alignment > 0)) {
		addressToReturn = (void*)(((uintptr_t)addressToReturn + alignment - 1) & ~(alignment - 1));
	}
	return addressToReturn;
}

/**
 * Frees the associated chunk of memory.
 */
void
MM_NonVirtualMemory::tearDown(MM_EnvironmentBase* env)
{
	if (NULL != _baseAddress) {
		_extensions->getForge()->free(_baseAddress);
		_baseAddress = NULL;
	}
}

/**
 * Commit the address range into physical memory.
 * @return true if successful, false otherwise.
 */
bool
MM_NonVirtualMemory::commitMemory(void* address, uintptr_t size)
{
	return true;
}

/**
 * Decommit the address range from physical memory.
 * @return true if successful, false otherwise.
 */
bool
MM_NonVirtualMemory::decommitMemory(void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress)
{
	return true;
}

bool
MM_NonVirtualMemory::setNumaAffinity(uintptr_t numaNode, void* address, uintptr_t byteAmount)
{
	return true;
}

#endif /* (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) */
