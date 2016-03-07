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

#include <string.h>

#include "GCExtensionsBase.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryPoolAggregatedCellList.hpp"

#include "HeapRegionDescriptorSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

bool
MM_HeapRegionDescriptorSegregated::initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager)
{
	if (!MM_HeapRegionDescriptor::initialize(env, regionManager)) {
		return false;
	}

	if (!_memoryPoolACL.initialize(env, this)) {
		return false;
	}
	setMemoryPool(&_memoryPoolACL);

	_regionManager = regionManager;

	memset(_arrayletBackPointers, 0, sizeof(uintptr_t *) * env->getExtensions()->arrayletsPerRegion);

	return true;
}

void
MM_HeapRegionDescriptorSegregated::tearDown(MM_EnvironmentBase *env)
{
	setMemoryPool(NULL);
	_memoryPoolACL.tearDown(env);
	MM_HeapRegionDescriptor::tearDown(env);
}

bool
MM_HeapRegionDescriptorSegregated::initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress)
{
	new((MM_HeapRegionDescriptorSegregated*)descriptor) MM_HeapRegionDescriptorSegregated(env, lowAddress, highAddress);
	return ((MM_HeapRegionDescriptorSegregated*)descriptor)->initialize(env, regionManager);
}

void
MM_HeapRegionDescriptorSegregated::setRange(RegionType type, uintptr_t range)
{
	uintptr_t self = _regionManager->mapDescriptorToRegionTableIndex(this);
	assume(self >= 0 || range==0, "setRange"); /* range zero for special entry */
	for (uintptr_t index = 0; index<range; index++) {
		MM_HeapRegionDescriptorSegregated *region = (MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(self + index);
		region->setRegionType(type);
		region->setRangeCount(range - index);
	}
	if (range > 0) { /* check for special entries */
		((MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(self + range - 1))->setRangeHead(this);
	}
	if (range == 1) {
		((MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(self))->setRangeCount(1);
	}
}
/**
 * Split a detached free range - the first portion becomes used
 */
MM_HeapRegionDescriptorSegregated*
MM_HeapRegionDescriptorSegregated::splitRange(uintptr_t numRegionsToSplit)
{
	uintptr_t range = getRange();
	assume(isFree() && range > numRegionsToSplit, "splitRange");
	uintptr_t index = _regionManager->mapDescriptorToRegionTableIndex(this);
	MM_HeapRegionDescriptorSegregated *second = (MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(index + numRegionsToSplit);
	second->resetTailFree(range - numRegionsToSplit);
	return second;
}

bool
MM_HeapRegionDescriptorSegregated::joinFreeRangeInit(MM_HeapRegionDescriptorSegregated *possNext)
{
	assume(isFree() && possNext->isFree(), "joinRange");
	uintptr_t regionSize = _regionManager->getRegionSize();

	uintptr_t selfIndex = _regionManager->mapDescriptorToRegionTableIndex(this);
	uintptr_t selfRange = getRange();
	uintptr_t possNextIndex = _regionManager->mapDescriptorToRegionTableIndex(possNext);
	uintptr_t possNextRange = possNext->getRange();

	if (selfIndex + selfRange == possNextIndex) {

		/* index adjacency makes region adjacency possible */
		uintptr_t *lastRegion = (uintptr_t *)((MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(possNextIndex - 1))->getLowAddress();
		uintptr_t *firstRegion = (uintptr_t *)((MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(possNextIndex))->getLowAddress();

		if ((uintptr_t)lastRegion + regionSize == (uintptr_t)firstRegion) {
			/* setFree(selfRange + possNextRange); */
			setRangeCount(selfRange + possNextRange);
			return true;
		}
	}
	return false;

}

void
MM_HeapRegionDescriptorSegregated::joinFreeRangeComplete()
{
	setFree(getRange());
}

void
MM_HeapRegionDescriptorSegregated::resetTailFree(uintptr_t range)
{
	uintptr_t self = _regionManager->mapDescriptorToRegionTableIndex(this);
	if (range == 1) {
		((MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(self))->setRangeCount(1);
	}
	if (range > 0) {
		((MM_HeapRegionDescriptorSegregated *)_regionManager->mapRegionTableIndexToDescriptor(self + range - 1))->setRangeHead(this);
	}
};

void
MM_HeapRegionDescriptorSegregated::setSmall(uintptr_t sizeClass)
{
	setRegionType(SEGREGATED_SMALL);
	setSizeClass(sizeClass);
	setRangeCount(1);
	_memoryPoolACL.resetCounts();
}

void
MM_HeapRegionDescriptorSegregated::setFree(uintptr_t range)
{
	setRange(FREE, range);
	_memoryPoolACL.resetCounts();
}

uintptr_t
MM_HeapRegionDescriptorSegregated::formatFresh(MM_EnvironmentBase *env, uintptr_t sizeClass, void *lowAddress)
{
	uintptr_t numCells = _memoryPoolACL.reset(env, sizeClass, lowAddress);

	_memoryPoolACL.resetCounts();
	_memoryPoolACL.setFreeCount(numCells);

	return numCells;
}

#if defined(OMR_GC_ARRAYLETS)
uintptr_t *
MM_HeapRegionDescriptorSegregated::allocateArraylet(MM_EnvironmentBase *env, omrarrayptr_t parentIndexableObject)
{
	Assert_MM_true(isArraylet());
	uintptr_t arrayletsPerRegion = env->getExtensions()->arrayletsPerRegion;
	Assert_MM_true(_nextArrayletIndex <= arrayletsPerRegion);

	for (uintptr_t i=_nextArrayletIndex; i < arrayletsPerRegion; i++) {
		if (isArrayletUnused(i)) {
			setArrayletParent(i, (uintptr_t *)parentIndexableObject);
			_memoryPoolACL.addBytesAllocated(env, env->getOmrVM()->_arrayletLeafSize);
			_nextArrayletIndex = (i + 1);
			return getArraylet(i, env->getOmrVM()->_arrayletLeafLogSize);
		}
	}
	_nextArrayletIndex = arrayletsPerRegion;
	return NULL;
}

void
MM_HeapRegionDescriptorSegregated::setArraylet()
{
	setRegionType(ARRAYLET_LEAF);
	setSizeClass(0);
	setRangeCount(1);
	_memoryPoolACL.resetCounts();
}

void
MM_HeapRegionDescriptorSegregated::addBytesFreedToArrayletBackout(MM_EnvironmentBase* env)
{
	Assert_MM_true(isArraylet());

	if (GC_UNMARK == env->getAllocationColor()) {
		_memoryPoolACL.addSweepFreeBytes(env, env->getOmrVM()->_arrayletLeafSize);
	}
}

void
MM_HeapRegionDescriptorSegregated::addBytesFreedToSmallSpineBackout(MM_EnvironmentBase* env)
{
	Assert_MM_true(isSmall());

	uintptr_t cellSize = getCellSize();
	if (GC_UNMARK == env->getAllocationColor()) {
		_memoryPoolACL.addSweepFreeBytes(env, cellSize);
	}
}
#endif /* defined(OMR_GC_ARRAYLETS) */

/**
 * We must be notified when an empty region is allocated so that we can account for the memory lost to
 * internal fragmentation. The amount of internal fragmentation depends on the size class for which
 * the new region has been formatted.
 */
void
MM_HeapRegionDescriptorSegregated::emptyRegionAllocated(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	_memoryPoolACL.setPreSweepFreeBytes(extensions->regionSize);

	if (isSmall()) {
		Assert_MM_true(getRange() == 1);
		/* We need to account for internal fragmentation. */
		_memoryPoolACL.addBytesAllocated(env, (extensions->regionSize - (getNumCells() * getCellSize())));
#if defined(OMR_GC_ARRAYLETS)
	} else if (isArraylet()) {
		/* We only need to take into account potential internal fragmentation of arraylets, the allocation
		 * context will call into the allocation tracker for the allocation of the arraylet.
		 */
		_memoryPoolACL.addBytesAllocated(env, (extensions->regionSize % env->getOmrVM()->_arrayletLeafSize) * getRange());
#endif /* defined(OMR_GC_ARRAYLETS) */
	} else if (isLarge()) {
		/* We need to account for the entire allocation. */
		env->_allocationTracker->addBytesAllocated(env, extensions->regionSize * getRange());
	} else {
		Assert_MM_unreachable();
	}
}

void
MM_HeapRegionDescriptorSegregated::emptyRegionReturned(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (isSmall()) {
		Assert_MM_true(getRange() == 1);
		/* We need to account for internal fragmentation. */
		env->_allocationTracker->addBytesFreed(env, (extensions->regionSize - (getNumCells() * getCellSize())));
#if defined(OMR_GC_ARRAYLETS)
	} else if (isArraylet()) {
		/* We only need to take into account potential internal fragmentation of arraylets. */
		env->_allocationTracker->addBytesFreed(env, (extensions->regionSize % env->getOmrVM()->_arrayletLeafSize) * getRange());
#endif /* defined(OMR_GC_ARRAYLETS) */
	} else if (isLarge()) {
		/* We need to account for the entire allocation. */
		env->_allocationTracker->addBytesFreed(env, extensions->regionSize * getRange());
	} else {
		Assert_MM_unreachable();
	}
}

void
MM_HeapRegionDescriptorSegregated::updateCounts(MM_EnvironmentBase *env, bool fromFlush)
{
#if defined(OMR_GC_ARRAYLETS)
	if (isArraylet()) {

		_memoryPoolACL.resetCounts();
		uintptr_t arrayletsPerRegion = env->getExtensions()->arrayletsPerRegion;

		for (uintptr_t i = 0; i < arrayletsPerRegion; i++) {
			if (isArrayletUnused(i)) {
				_memoryPoolACL.incrementFreeCount();
			}
		}
	} else
#endif /* defined(OMR_GC_ARRAYLETS) */
	if (isSmall()) {
		_memoryPoolACL.updateCounts(env, fromFlush);
	} else if (isLarge()) {
	}
}

uintptr_t
MM_HeapRegionDescriptorSegregated::debugCountFreeBytes()
{
	return _memoryPoolACL.debugCountFreeBytes();
}

#endif /* OMR_GC_SEGREGATED_HEAP */
