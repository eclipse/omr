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

#include "omrcfg.h"

#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryManager.hpp"
#include "HeapMemorySnapshot.hpp"

#include "HeapRegionManagerTarok.hpp"

MM_HeapRegionManagerTarok::MM_HeapRegionManagerTarok(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
	: MM_HeapRegionManager(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor)
	, _freeRegionTableSize(0)
	, _freeRegionTable(NULL)
{
	_typeId = __FUNCTION__;
}

MM_HeapRegionManagerTarok *
MM_HeapRegionManagerTarok::newInstance(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
{
	MM_HeapRegionManagerTarok *regionManager = (MM_HeapRegionManagerTarok *)env->getForge()->allocate(sizeof(MM_HeapRegionManagerTarok), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != regionManager) {
		new(regionManager) MM_HeapRegionManagerTarok(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor);
		if (!regionManager->initialize(env)) {
			regionManager->kill(env);
			regionManager = NULL;
		}
	}
	return regionManager;
}

bool 
MM_HeapRegionManagerTarok::initialize(MM_EnvironmentBase *env)
{
	bool result = MM_HeapRegionManager::initialize(env);

	if (result) {
		uintptr_t maximumNodeNumber = env->getExtensions()->_numaManager.getMaximumNodeNumber();

		_freeRegionTableSize = maximumNodeNumber + 1;

		uintptr_t freeRegionTableSizeInBytes = _freeRegionTableSize * sizeof(MM_HeapRegionDescriptor *);
		_freeRegionTable = (MM_HeapRegionDescriptor **)env->getForge()->allocate(freeRegionTableSizeInBytes, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
		if (NULL == _freeRegionTable) {
			return false;
		}
		memset(_freeRegionTable, 0, freeRegionTableSizeInBytes);
	}

	return result;
}

void 
MM_HeapRegionManagerTarok::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _freeRegionTable) {
		env->getForge()->free(_freeRegionTable);
		_freeRegionTable = NULL;
		_freeRegionTableSize = 0;
	}

	MM_HeapRegionManager::tearDown(env);
}

MM_HeapRegionDescriptor *
MM_HeapRegionManagerTarok::acquireSingleTableRegion(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t numaNode)
{
	MM_HeapRegionDescriptor *toReturn = NULL;
	writeLock();
	Trc_MM_HeapRegionManager_acquireSingleTableRegions_Entry(env->getLanguageVMThread(), subSpace, numaNode);
	Assert_MM_true(numaNode < _freeRegionTableSize);
	if (NULL != _freeRegionTable[numaNode]) {
		toReturn = internalAcquireSingleTableRegion(env, subSpace, numaNode);
		Assert_MM_true(NULL != toReturn);
	}
	Trc_MM_HeapRegionManager_acquireSingleTableRegions_Exit(env->getLanguageVMThread(), toReturn, numaNode);
	writeUnlock();
	return toReturn;
}

MM_HeapRegionDescriptor *
MM_HeapRegionManagerTarok::internalAcquireSingleTableRegion(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t freeListIndex)
{
	Assert_MM_true(NULL != _freeRegionTable[freeListIndex]);

	/*since we only need one region, always return the first free region */
	MM_HeapRegionDescriptor *toReturn = _freeRegionTable[freeListIndex];

	_freeRegionTable[freeListIndex] = toReturn->_nextInSet;

	toReturn->_nextInSet = NULL;
	toReturn->_isAllocated = true;
	toReturn->associateWithSubSpace(subSpace);
	_totalHeapSize += toReturn->getSize();

	return toReturn;
}

void *
MM_HeapRegionManagerTarok::findHighestValidAddressBelow(MM_HeapRegionDescriptor *targetRegion)
{
	void *lowValidAddress = _lowTableEdge;
	uintptr_t targetIndex = mapDescriptorToRegionTableIndex(targetRegion);
	uintptr_t cursorIndex = 0;

	while (cursorIndex < targetIndex) {
		MM_HeapRegionDescriptor *cursorRegion = mapRegionTableIndexToDescriptor(cursorIndex);
		if (cursorRegion->_isAllocated) {
			lowValidAddress = cursorRegion->getHighAddress();
		}
		cursorIndex++;
	}
	return lowValidAddress;
}

void *
MM_HeapRegionManagerTarok::findLowestValidAddressAbove(MM_HeapRegionDescriptor *targetRegion)
{
	void *highValidAddress = _highTableEdge;
	uintptr_t targetIndex = mapDescriptorToRegionTableIndex(targetRegion);
	uintptr_t cursorIndex = targetIndex + 1;

	while (cursorIndex < _tableRegionCount) {
		MM_HeapRegionDescriptor *cursorRegion = mapRegionTableIndexToDescriptor(cursorIndex);
		if (cursorRegion->_isAllocated) {
			highValidAddress = cursorRegion->getLowAddress();
			break;
		}
		cursorIndex++;
	}
	return highValidAddress;
}

bool
MM_HeapRegionManagerTarok::setContiguousHeapRange(MM_EnvironmentBase *env, void *lowHeapEdge, void *highHeapEdge)
{
	writeLock();
	/* ensure that this manager was configured with a valid region size */
	Assert_MM_true(0 != _regionSize);
	/* we don't yet support multiple enabling calls (split heaps) */
	/* This assertion would triggered at multiple attempts to initialize heap in gcInitializeDefaults() */
	/* Assert_MM_true(NULL == _regionTable); */
	/* the regions must be aligned (in present implementation) */
	Assert_MM_true(0 == ((uintptr_t)lowHeapEdge % _regionSize));
	Assert_MM_true(0 == ((uintptr_t)highHeapEdge % _regionSize));
	/* make sure that the range is in the right order and of non-zero size*/
	Assert_MM_true(highHeapEdge > lowHeapEdge);
	/* allocate the table */
	uintptr_t size = (uintptr_t)highHeapEdge - (uintptr_t)lowHeapEdge;
	_tableRegionCount = size / _regionSize;
	_regionTable = internalAllocateAndInitializeRegionTable(env, lowHeapEdge, highHeapEdge);
	bool success = false;
	if (NULL != _regionTable) {
		_lowTableEdge = lowHeapEdge;
		_highTableEdge = highHeapEdge;
		success = true;
	}
	writeUnlock();
	return success;
}

void
MM_HeapRegionManagerTarok::destroyRegionTable(MM_EnvironmentBase *env)
{
	if (NULL != _regionTable) {
		 internalFreeRegionTable(env, _regionTable, _tableRegionCount);
		 _regionTable = NULL;
	}
}

void
MM_HeapRegionManagerTarok::internalLinkRegions(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *headRegion, uintptr_t count)
{
	Assert_MM_true(0 < count);
	MM_HeapRegionDescriptor *region = headRegion;

	for (uintptr_t i = 0; i < count; i++) {
		region->_headOfSpan = region;
		region->_regionsInSpan = 1;
		MM_HeapRegionDescriptor *nextRegion = (MM_HeapRegionDescriptor *) ((uintptr_t)region + _tableDescriptorSize);
		region->_nextInSet = nextRegion;
		region = nextRegion;
	}

	/* set the very last region's nextInSet to NULL. */
	MM_HeapRegionDescriptor *lastRegion = (MM_HeapRegionDescriptor *) ((uintptr_t)headRegion + ((count -1) * _tableDescriptorSize));
	lastRegion->_nextInSet = NULL;
}


bool
MM_HeapRegionManagerTarok::enableRegionsInTable(MM_EnvironmentBase *env, MM_MemoryHandle *handle)
{
	bool result = true;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;
	void *lowHeapEdge = memoryManager->getHeapBase(handle);
	void *highHeapEdge = memoryManager->getHeapTop(handle);
	
	/* maintained for RTJ */
	setNodeAndLinkRegions(env, lowHeapEdge, highHeapEdge, 0);

	return result;
}

void
MM_HeapRegionManagerTarok::setNodeAndLinkRegions(MM_EnvironmentBase *env, void *lowHeapEdge, void *highHeapEdge, uintptr_t numaNode)
{
	uintptr_t regionCount = 0;
	MM_HeapRegionDescriptor *firstRegion = NULL;

	Trc_MM_HeapRegionManager_enableRegionsInTable_Entry(env->getLanguageVMThread(), lowHeapEdge, highHeapEdge, numaNode);
	if (highHeapEdge > lowHeapEdge) {
		for (uint8_t* address = (uint8_t*)lowHeapEdge; address < highHeapEdge; address += getRegionSize()) {
			MM_HeapRegionDescriptor *region = tableDescriptorForAddress(address);
			region->setNumaNode(numaNode);
			regionCount += 1;
		}

		firstRegion = tableDescriptorForAddress(lowHeapEdge);
		firstRegion->_nextInSet = _freeRegionTable[numaNode];
		_freeRegionTable[numaNode] = firstRegion;
		internalLinkRegions(env, firstRegion, regionCount);
	}
	Trc_MM_HeapRegionManager_enableRegionsInTable_Exit(env->getLanguageVMThread(), regionCount, firstRegion, numaNode);
}

void
MM_HeapRegionManagerTarok::releaseTableRegions(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
{
	writeLock();
	/* must be a table region */
	Assert_MM_true((region >= _regionTable) && (region < (MM_HeapRegionDescriptor *)((uintptr_t)_regionTable + (_tableRegionCount * _tableDescriptorSize))));
	internalReleaseTableRegions(env, region);
	_totalHeapSize -= region->getSize();
	writeUnlock();
}

void
MM_HeapRegionManagerTarok::internalReleaseTableRegions(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *rootRegion)
{
	/* must be an allocated table region */
	Assert_MM_true(rootRegion >= _regionTable);
	Assert_MM_true(rootRegion < (MM_HeapRegionDescriptor *)((uintptr_t)_regionTable + (_tableRegionCount * _tableDescriptorSize)));
	Assert_MM_true(NULL == rootRegion->_nextInSet);
	Assert_MM_true(rootRegion->_isAllocated);

	rootRegion->_isAllocated = false;
	rootRegion->setRegionType(MM_HeapRegionDescriptor::RESERVED);
	rootRegion->disassociateWithSubSpace();

	uintptr_t freeListIndex = rootRegion->getNumaNode();
	rootRegion->_nextInSet = _freeRegionTable[freeListIndex];
	_freeRegionTable[freeListIndex] = rootRegion;
}

MM_HeapMemorySnapshot*
MM_HeapRegionManagerTarok::getHeapMemorySnapshot(MM_GCExtensionsBase *extensions, MM_HeapMemorySnapshot* snapshot, bool gcEnd)
{
	MM_Heap *heap = extensions->getHeap();
	snapshot->_totalHeapSize = heap->getActiveMemorySize();
	snapshot->_freeHeapSize = heap->getApproximateFreeMemorySize();
	return snapshot;
}

