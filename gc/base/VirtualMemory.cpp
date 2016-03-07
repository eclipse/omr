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

#include <string.h>

#include "omrcomp.h"
#include "omrport.h"
#include "omr.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "Math.hpp"
#include "ModronAssertions.h"
#include "NUMAManager.hpp"
#include "VirtualMemory.hpp"

#define HIGH_ADDRESS UDATA_MAX

/****************************************
 * Initialization
 ****************************************
 */

MM_VirtualMemory*
MM_VirtualMemory::newInstance(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t size, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t tailPadding, void* preferredAddress, void* ceiling, uintptr_t mode, uintptr_t options, uint32_t memoryCategory)
{
	MM_VirtualMemory* vmem = (MM_VirtualMemory*)env->getForge()->allocate(sizeof(MM_VirtualMemory), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());

	if (vmem) {
		new (vmem) MM_VirtualMemory(env, heapAlignment, pageSize, pageFlags, tailPadding, mode);
		if (!vmem->initialize(env, size, preferredAddress, ceiling, options, memoryCategory)) {
			vmem->kill(env);
			vmem = NULL;
		}
	}

	return vmem;
}

void
MM_VirtualMemory::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_VirtualMemory::initialize(MM_EnvironmentBase* env, uintptr_t size, void* preferredAddress, void* ceiling, uintptr_t options, uint32_t memoryCategory)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	/* there is no memory taken already */
	Assert_MM_true(NULL == _heapBase);

	uintptr_t allocateSize = size + _tailPadding;

	J9PortVmemParams params;
	omrvmem_vmem_params_init(&params);
	params.byteAmount = allocateSize;
	params.mode = _mode;
	params.options |= options;
	params.pageSize = _pageSize;
	params.pageFlags = _pageFlags;
	params.category = memoryCategory;

	if (NULL != preferredAddress) {
		params.startAddress = preferredAddress;
		params.endAddress = preferredAddress;
	}

	if ((NULL != ceiling) && (params.byteAmount <= (uintptr_t)ceiling)) {
		void* maxEndAddress = (void*)((uintptr_t)ceiling - params.byteAmount);

		/*
		 * Temporary fix to cover problem in Port Library:
		 * if direction is top down an allocation would be attempted from endAddress first regardless it page aligned or not
		 * For unaligned case an allocation will succeed but not in requested pages
		 * As far as this is only place in GC used endAddress add rounding here
		 * (another case is handling of preferredAddress - we do not care)
		 */
		maxEndAddress = (void*)MM_Math::roundToFloor(_pageSize, (uintptr_t)maxEndAddress);

		if (params.endAddress > maxEndAddress) {
			params.endAddress = maxEndAddress;
		}
	}

	if (params.startAddress <= params.endAddress) {
		_heapBase = reserveMemory(&params);
	}

	if (NULL != _heapBase) {
		uintptr_t lastByte = (uintptr_t)_heapBase + (allocateSize - 1);

		/* If heap touches top of address range */
		if (lastByte == HIGH_ADDRESS) {
			_heapTop = (void*)MM_Math::roundToFloor(_heapAlignment, ((uintptr_t)_baseAddress) + (allocateSize - _tailPadding - _heapAlignment));
		} else {
			_heapTop = (void*)MM_Math::roundToFloor(_heapAlignment, ((uintptr_t)_baseAddress) + (allocateSize - _tailPadding));
		}

		if ((_heapBase >= _heapTop) /* CMVC 45178: Need to catch the case where we aligned heapTop and heapBase to the same address and consider it an error. */
		|| ((NULL != ceiling) && (_heapTop > ceiling)) /* Check that memory we got is located below ceiling */
		) {
			freeMemory();
			_heapBase = NULL;
		}
	}

	return NULL != _heapBase;
}

void
MM_VirtualMemory::roundDownTop(uintptr_t rounding)
{
	_heapTop = (void*)MM_Math::roundToFloor(_heapAlignment, ((uintptr_t)_heapBase) + (_reserveSize - rounding));
}

void*
MM_VirtualMemory::reserveMemory(J9PortVmemParams* params)
{
	OMRPORT_ACCESS_FROM_OMRVM(_extensions->getOmrVM());

	/* be sure that nothing allocated otherwise it would be lost for memory free operation */
	Assert_MM_true(NULL == _baseAddress);
	Assert_MM_true(0 != _pageSize);

	void* addressToReturn = NULL;
	_reserveSize = MM_Math::roundToCeiling(_pageSize, params->byteAmount);
	params->byteAmount = _reserveSize;

	memset(&_identifier, 0, sizeof(J9PortVmemIdentifier));
	_baseAddress = omrvmem_reserve_memory_ex(&_identifier, params);

	if (NULL != _baseAddress) {
		_pageSize = omrvmem_get_page_size(&_identifier);
		_pageFlags = omrvmem_get_page_flags(&_identifier);
		Assert_MM_true(0 != _pageSize);
		addressToReturn = (void*)MM_Math::roundToCeiling(_heapAlignment, (uintptr_t)_baseAddress);
	}
	return addressToReturn;
}

bool MM_VirtualMemory::freeMemory()
{
	OMRPORT_ACCESS_FROM_OMRVM(_extensions->getOmrVM());
	bool success = (0 == omrvmem_free_memory(_baseAddress, _reserveSize, &_identifier));
	if (success) {
		_baseAddress = NULL;
		_reserveSize = 0;
	}
	return success;
}

/**
 * Commit the address range into physical memory.
 * @return true if successful, false otherwise.
 */
bool
MM_VirtualMemory::commitMemory(void* address, uintptr_t size)
{
	OMRPORT_ACCESS_FROM_OMRVM(_extensions->getOmrVM());
	Assert_MM_true(0 != _pageSize);

	bool success = true;

	/* port library takes page aligned addresses and sizes only */
	void* commitBase = (void*)MM_Math::roundToFloor(_pageSize, (uintptr_t)address);
	void* commitTop = (void*)MM_Math::roundToCeiling(_pageSize, (uintptr_t)address + size + _tailPadding);
	uintptr_t commitSize;

	if (commitBase <= commitTop) {
		commitSize = (uintptr_t)commitTop - (uintptr_t)commitBase;
	} else {
		/* wrapped around - this is end of the memory */
		commitSize = UDATA_MAX - (uintptr_t)commitBase + 1;
	}

	if (0 < commitSize) {
		success = omrvmem_commit_memory(commitBase, commitSize, &_identifier) != 0;
	}

	if (success) {
		Trc_MM_VirtualMemory_commitMemory_success(address, size);
	} else {
		Trc_MM_VirtualMemory_commitMemory_failure(address, size);
	}

	return success;
}

/**
 * Decommit the address range from physical memory.
 * @param address the start of the block to be decommitted
 * @param size the size of the block to be decommitted
 * @param lowValidAddress the end of the previous committed block below address, or NULL if address is the first committed block
 * @param highValidAddress the start of the next committed block above address, or NULL if address is the last committed block
 * @return true if successful, false otherwise.
 */
bool
MM_VirtualMemory::decommitMemory(void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress)
{
	bool result = true;
	void* decommitBase = address;
	void* decommitTop = (void*)((uintptr_t)decommitBase + size + _tailPadding);
	Assert_MM_true(0 != _pageSize);

	OMRPORT_ACCESS_FROM_OMRVM(_extensions->getOmrVM());

	if (NULL != lowValidAddress) {
		/* Ensure that we do not decommit a valid page prior to address */
		/* What about tail padding? Are we deleting someone's pad? */
		lowValidAddress = (void*)((uintptr_t)lowValidAddress + _tailPadding);
		if (lowValidAddress > decommitBase) {
			decommitBase = lowValidAddress;
		}
	}

	if (NULL != highValidAddress) {
		/* Ensure that we do not decommit a valid page after address+size */
		if (highValidAddress < decommitTop) {
			decommitTop = highValidAddress;
		}
	}

	/* port library takes page aligned addresses and sizes only */
	decommitBase = (void*)MM_Math::roundToCeiling(_pageSize, (uintptr_t)decommitBase);
	decommitTop = (void*)MM_Math::roundToFloor(_pageSize, (uintptr_t)decommitTop);

	if (decommitBase < decommitTop) {
		/* There is still memory to decommit, calculate size */
		uintptr_t decommitSize = ((uintptr_t)decommitTop) - ((uintptr_t)decommitBase);
		result = omrvmem_decommit_memory(decommitBase, decommitSize, &_identifier) == 0;
	}

	return result;
}

void
MM_VirtualMemory::tearDown(MM_EnvironmentBase* env)
{
	if (NULL != _heapBase) {
		freeMemory();
		_heapBase = NULL;
	}
}

bool
MM_VirtualMemory::setNumaAffinity(uintptr_t numaNode, void* address, uintptr_t byteAmount)
{
	Assert_MM_true(0 != _pageSize);

	/* start address must be above heap start address */
	Assert_MM_true(address >= _heapBase);
	/* start address must be below heap top address */
	Assert_MM_true(address <= _heapTop);

	/* start address must be aligned to physical page size */
	Assert_MM_true(0 == ((uintptr_t)address % _pageSize));

	void* topAddress = (void*)((uintptr_t)address + byteAmount);

	/* top address must be above heap start address */
	Assert_MM_true(topAddress >= _heapBase);
	/* top address must be below heap top address */
	Assert_MM_true(topAddress <= _heapTop);

	bool didSetAffinity = true;
	if (_extensions->_numaManager.isPhysicalNUMASupported()) {
		OMRPORT_ACCESS_FROM_OMRVM(_extensions->getOmrVM());

		uintptr_t byteAmountPageAligned = MM_Math::roundToCeiling(_pageSize, byteAmount);
		/* aligned high address might be higher then heapTop but 
		 * must be in the heap reserved memory range
		 */
		Assert_MM_true(((uintptr_t)address + byteAmountPageAligned) <= ((uintptr_t)_heapBase + _reserveSize));

		didSetAffinity = (0 == omrvmem_numa_set_affinity(numaNode, address, byteAmountPageAligned, &_identifier));
	}
	return didSetAffinity;
}
