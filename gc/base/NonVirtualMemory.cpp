/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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

#if (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) || defined(OMRZTPF)
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

#endif /* (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) || defined(OMRZTPF) */
