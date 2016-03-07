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

#include "Forge.hpp"

#include "omrcomp.h"
#include "EnvironmentBase.hpp"

typedef struct MM_MemoryHeader {
	uintptr_t allocatedBytes;
	MM_AllocationCategory::Enum category;
} MM_MemoryHeader;

typedef union MM_AlignedMemoryHeader {
	double forAlignment;
	MM_MemoryHeader header;
} MM_AlignedMemoryHeader;

bool
MM_Forge::initialize(MM_EnvironmentBase* env)
{
	_portLibrary = env->getPortLibrary();

	if (0 != omrthread_monitor_init_with_name(&_mutex, 0, "MM_Forge")) {
		return false;
	}	

	for (uintptr_t i = 0; i < MM_AllocationCategory::CATEGORY_COUNT; i++) {
		_statistics[i].category = (MM_AllocationCategory::Enum) i;
	}
	
	return true;
}

void 
MM_Forge::tearDown(MM_EnvironmentBase* env)
{
	_portLibrary = NULL;
	
	if (NULL != _mutex) {
		omrthread_monitor_destroy(_mutex);
		_mutex = NULL;
	}
}

/**
 * Allocates the amount of memory requested in bytesRequested.  Returns a pointer to the allocated memory, or NULL if the request could
 * not be performed.  This function is a wrapper of omrmem_allocate_memory.
 *
 * @param[in] byesRequested - the number of bytes to allocate
 * @param[in] category - the memory usage category for the allocated memory
 * @param[in] callsite - the origin of the memory request (e.g. filename.c:5), which should be found using the OMR_GET_CALLSITE() macro
 * @return a pointer to the allocated memory, or NULL if the request could not be performed
 */
void* 
MM_Forge::allocate(uintptr_t bytesRequested, MM_AllocationCategory::Enum category, const char* callsite)
{
	MM_AlignedMemoryHeader* memoryPointer;

	memoryPointer = (MM_AlignedMemoryHeader *) _portLibrary->mem_allocate_memory(_portLibrary, bytesRequested + sizeof(MM_AlignedMemoryHeader), callsite, OMRMEM_CATEGORY_MM);
	if (NULL != memoryPointer) {
		memoryPointer->header.allocatedBytes = bytesRequested;
		memoryPointer->header.category = category;

		omrthread_monitor_enter(_mutex);

		_statistics[category].allocated += bytesRequested;
		if (_statistics[category].allocated > _statistics[category].highwater) {
			_statistics[category].highwater = _statistics[category].allocated; 
		}

		omrthread_monitor_exit(_mutex);
		memoryPointer += 1;
	}
	
	return memoryPointer;
}

/**
 * Deallocate memory that has been allocated by the garbage collector.  This function should not be called to deallocate memory that has
 * not been allocated by either the allocate or reallocate functions.  This function is a wrapper of omrmem_free_memory.
 *
 * @param[in] memoryPointer - a pointer to the memory that will be freed
 */
void 
MM_Forge::free(void* memoryPointer)
{	
	if (NULL == memoryPointer) {
		return;
	}

	MM_AlignedMemoryHeader* alignedHeader = (MM_AlignedMemoryHeader *) memoryPointer;
	alignedHeader -= 1;


	omrthread_monitor_enter(_mutex);
	_statistics[alignedHeader->header.category].allocated -= alignedHeader->header.allocatedBytes; 
	omrthread_monitor_exit(_mutex);
	
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	omrmem_free_memory(alignedHeader);
}

/**
 * Returns the current memory usage statistics for the garbage collector.  Each entry in the array corresponds to a memory usage category type.
 * To locate memory usage statistics for a particular category, use the enumeration value as the array index (e.g. stats[REFERENCES]).
 *
 * @return an array of memory usage statistics indexed using the MM_AllocationCategory enumeration
 */
MM_MemoryStatistics*
MM_Forge::getCurrentStatistics()
{
	return _statistics;
}
