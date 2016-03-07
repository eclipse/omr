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

#include "HeapRegionManager.hpp"

#include <string.h>

#include "omrlinkedlist.h"
#include "omrport.h"

#include "Bits.hpp"
#include "Forge.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptor.hpp"

class MemorySubSpace;

MM_HeapRegionManager::MM_HeapRegionManager(MM_EnvironmentBase* env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
	: MM_BaseVirtual()
	, _auxRegionDescriptorList(NULL)
	, _auxRegionCount(0)
	, _regionSize(regionSize)
	, _regionShift(0)
	, _regionTable(NULL)
	, _tableRegionCount(0)
	, _lowTableEdge(NULL)
	, _highTableEdge(NULL)
	, _tableDescriptorSize(tableDescriptorSize)
	, _regionDescriptorInitializer(regionDescriptorInitializer)
	, _regionDescriptorDestructor(regionDescriptorDestructor)
	, _totalHeapSize(0)
{
	_typeId = __FUNCTION__;
}

MM_HeapRegionManager*
MM_HeapRegionManager::newInstance(MM_EnvironmentBase* env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
{
	MM_HeapRegionManager *regionManager = (MM_HeapRegionManager *)env->getForge()->allocate(sizeof(MM_HeapRegionManager), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (regionManager) {
		new(regionManager) MM_HeapRegionManager(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor);
		if (!regionManager->initialize(env)) {
			regionManager->kill(env);
			regionManager = NULL;
		}
	}
	return regionManager;
}

void
MM_HeapRegionManager::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_HeapRegionManager::initialize(MM_EnvironmentBase* env)
{
	if (0 != _heapRegionListMonitor.initialize(128)) {
		return false;
	}

	_regionShift = MM_Bits::leadingZeroes(_regionSize);
	Assert_MM_true(((uintptr_t)1 << _regionShift) == _regionSize);

	return true;
}

void
MM_HeapRegionManager::tearDown(MM_EnvironmentBase* env)
{
	/* Region Table must be already destroyed at this point */
	Assert_MM_true(NULL == _regionTable);
	_heapRegionListMonitor.tearDown();
}

void
MM_HeapRegionManager::insertHeapRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* heapRegion)
{
	/* without write lock for this mothod, the caller need to acquire the lock before invoking this method */
	MM_HeapRegionDescriptor* nextHeapRegion = _auxRegionDescriptorList;
	MM_HeapRegionDescriptor* lastHeapRegion = NULL;

	while (nextHeapRegion) {
		/* locate the right place to insert */
		if ((uintptr_t)heapRegion->getLowAddress() < (uintptr_t)nextHeapRegion->getLowAddress()) {
			break;
		}
		lastHeapRegion = nextHeapRegion;
		nextHeapRegion = nextHeapRegion->_nextRegion;
	}

	if (nextHeapRegion) {
		J9_LINEAR_LINKED_LIST_ADD_BEFORE(_nextRegion, _previousRegion,
										 _auxRegionDescriptorList, (MM_HeapRegionDescriptor*)nextHeapRegion, (MM_HeapRegionDescriptor*)heapRegion);
	} else {
		if (J9_LINEAR_LINKED_LIST_IS_EMPTY(_auxRegionDescriptorList)) {
			J9_LINEAR_LINKED_LIST_ADD(_nextRegion, _previousRegion, _auxRegionDescriptorList, (MM_HeapRegionDescriptor*)heapRegion);
		} else {
			J9_LINEAR_LINKED_LIST_ADD_AFTER(_nextRegion, _previousRegion,
											_auxRegionDescriptorList, (MM_HeapRegionDescriptor*)lastHeapRegion, (MM_HeapRegionDescriptor*)heapRegion);
		}
	}

	_auxRegionCount++;
	_totalHeapSize += heapRegion->getSize();
}

void
MM_HeapRegionManager::removeHeapRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* heapRegion)
{
	/* without write lock for this method, the caller need to acquire the lock before invoking this method */

	J9_LINEAR_LINKED_LIST_REMOVE(_nextRegion, _previousRegion, _auxRegionDescriptorList, heapRegion);

	_totalHeapSize -= heapRegion->getSize();
	_auxRegionCount--;
}

uintptr_t
MM_HeapRegionManager::getTotalHeapSize()
{
	return _totalHeapSize;
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::createAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, void* lowAddress, void* highAddress)
{
	writeLock();
	MM_HeapRegionDescriptor* toReturn = NULL;
	toReturn = internalCreateAuxiliaryRegionDescriptor(env, subSpace, lowAddress, highAddress);
	writeUnlock();
	return toReturn;
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::internalCreateAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, void* lowAddress, void* highAddress)
{
	MM_HeapRegionDescriptor* desc = NULL;

	desc = internalAllocateAuxiliaryRegionDescriptor(env, lowAddress, highAddress);
	if (NULL != desc) {
		desc->associateWithSubSpace(subSpace);
		desc->setRegionType(MM_HeapRegionDescriptor::ADDRESS_ORDERED);
		insertHeapRegion(env, desc);
	}
	return desc;
}

void
MM_HeapRegionManager::destroyAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* descriptor)
{
	Trc_MM_HeapRegionManager_destroyAuxiliaryRegionDescriptor_Entry(env->getLanguageVMThread(), descriptor);
	writeLock();
	internalDestroyAuxiliaryRegionDescriptor(env, descriptor);
	writeUnlock();
	Trc_MM_HeapRegionManager_destroyAuxiliaryRegionDescriptor_Exit(env->getLanguageVMThread());
}


void
MM_HeapRegionManager::internalDestroyAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* descriptor)
{
	removeHeapRegion(env, descriptor);
	internalFreeAuxiliaryRegionDescriptor(env, descriptor);
}

void
MM_HeapRegionManager::lock()
{
	_heapRegionListMonitor.enterRead();
}

void
MM_HeapRegionManager::unlock()
{
	_heapRegionListMonitor.exitRead();
}

void
MM_HeapRegionManager::reassociateRegionWithSubSpace(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region, MM_MemorySubSpace* subSpace)
{
	writeLock();

	region->disassociateWithSubSpace();
	region->associateWithSubSpace(subSpace);

	writeUnlock();
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::getFirstTableRegion()
{
	return findFirstUsedRegion(_regionTable);
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::getNextTableRegion(MM_HeapRegionDescriptor* heapRegion)
{
	return findFirstUsedRegion((MM_HeapRegionDescriptor*)((uintptr_t)heapRegion + (_tableDescriptorSize * heapRegion->_regionsInSpan)));
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::findFirstUsedRegion(MM_HeapRegionDescriptor* start)
{
	MM_HeapRegionDescriptor* top = (MM_HeapRegionDescriptor*)((uintptr_t)_regionTable + (_tableDescriptorSize * _tableRegionCount));
	MM_HeapRegionDescriptor* usedRegion = NULL;
	MM_HeapRegionDescriptor* current = start;

	while ((NULL == usedRegion) && (current < top)) {
		if (current->_isAllocated) {
			usedRegion = current;
		} else {
			current = (MM_HeapRegionDescriptor*)((uintptr_t)current + (_tableDescriptorSize * current->_regionsInSpan));
		}
	}
	return usedRegion;
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::internalAllocateAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, void* lowAddress, void* highAddress)
{
	MM_HeapRegionDescriptor* desc = (MM_HeapRegionDescriptor*)env->getForge()->allocate(_tableDescriptorSize, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != desc) {
		if (!_regionDescriptorInitializer(env, this, desc, lowAddress, highAddress)) {
			desc = NULL;
		}
	}
	return desc;
}

void
MM_HeapRegionManager::internalFreeAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* descriptor)
{
	if (NULL != _regionDescriptorDestructor) {
		_regionDescriptorDestructor(env, this, descriptor);
	}
	env->getForge()->free(descriptor);
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::internalAllocateAndInitializeRegionTable(MM_EnvironmentBase* env, void* lowHeapEdge, void* highHeapEdge)
{
	uintptr_t size = (uintptr_t)highHeapEdge - (uintptr_t)lowHeapEdge;
	uintptr_t regionSize = getRegionSize();
	uintptr_t regionCount = size / regionSize;
	uintptr_t sizeInBytes = regionCount * _tableDescriptorSize;
	MM_HeapRegionDescriptor* table = (MM_HeapRegionDescriptor*)env->getForge()->allocate(sizeInBytes, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != table) {
		/* the table has been allocated so initialize the descriptors inside it and the meta-data to use the table */
		memset((void*)table, 0, sizeInBytes);
		void* base = lowHeapEdge;
		MM_HeapRegionDescriptor* descriptor = table;
		for (uintptr_t i = 0; i < regionCount; i++) {
			void* next = (void*)((uintptr_t)base + regionSize);
			if (!_regionDescriptorInitializer(env, this, descriptor, base, next)) {
				internalFreeRegionTable(env, table, i);
				return NULL;
			}
			base = next;
			descriptor = (MM_HeapRegionDescriptor*)((uint8_t*)descriptor + _tableDescriptorSize);
		}
	}
	return table;
}

void
MM_HeapRegionManager::internalFreeRegionTable(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* tableBase, uintptr_t tableElementCount)
{
	if (NULL != _regionDescriptorDestructor) {
		MM_HeapRegionDescriptor* descriptor = tableBase;
		for (uintptr_t i = 0; i < tableElementCount; i++) {
			_regionDescriptorDestructor(env, this, descriptor);
			descriptor = (MM_HeapRegionDescriptor*)((uint8_t*)descriptor + _tableDescriptorSize);
		}
	}

	env->getForge()->free(tableBase);
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::auxillaryDescriptorForAddress(const void* address)
{
	MM_HeapRegionDescriptor* region = NULL;
	lock();

	region = getFirstAuxiliaryRegion();
	while (NULL != region) {
		if (region->isAddressInRegion(address)) {
			break;
		}
		region = getNextAuxiliaryRegion(region);
	}

	unlock();
	return region;
}

void
MM_HeapRegionManager::writeLock()
{
	_heapRegionListMonitor.enterWrite();
}

void
MM_HeapRegionManager::writeUnlock()
{
	_heapRegionListMonitor.exitWrite();
}

MM_HeapRegionDescriptor*
MM_HeapRegionManager::mapRegionTableIndexToDescriptor(uintptr_t index)
{
	return (MM_HeapRegionDescriptor*)((uintptr_t)_regionTable + (_tableDescriptorSize * index));
}

uintptr_t
MM_HeapRegionManager::mapDescriptorToRegionTableIndex(MM_HeapRegionDescriptor* region)
{
	return ((uintptr_t)region - (uintptr_t)_regionTable) / _tableDescriptorSize;
}
