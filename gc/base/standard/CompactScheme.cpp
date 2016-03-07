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
 *******************************************************************************/

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_COMPACTION)

#include "CompactScheme.hpp"

#include "ModronAssertions.h"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "Bits.hpp"
#include "CollectorLanguageInterface.hpp"
#include "CompactFixHeapForWalkTask.hpp"
#include "CompactSchemeFixupObject.hpp"
#include "Debug.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "Heap.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapMapIterator.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapMemoryPoolIterator.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "HeapStats.hpp"
#include "MarkingScheme.hpp"
#include "MarkMap.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectModel.hpp"
#include "ParallelSweepScheme.hpp"
#include "ParallelTask.hpp"
#include "SlotObject.hpp"
#include "SublistPool.hpp"
#include "SublistPuddle.hpp"
#include "SweepHeapSectioning.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"


/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_CompactScheme *
MM_CompactScheme::newInstance(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme)
{
	MM_CompactScheme *compactScheme;

	compactScheme = (MM_CompactScheme *)env->getForge()->allocate(sizeof(MM_CompactScheme), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (compactScheme) {
		new(compactScheme) MM_CompactScheme(env, markingScheme);
		if (!compactScheme->initialize(env)) {
			compactScheme->kill(env);
			compactScheme = NULL;
		}
	}

	return compactScheme;
}

MMINLINE uintptr_t
makeMask(intptr_t maskSize)
{
	return ((uintptr_t)1 << maskSize) - 1;
}

MMINLINE intptr_t
countBits(uintptr_t x)
{
    intptr_t count = 0;
    while (x) {
        count++;
        x &= x-1;
    }
    return count;
}

/************************************************************
 *
 * 32-bit layout
 *
 * addr  0:32  forwardingPtr
 *
 * bits  0:22  compressed mark bits
 * bits 22:28  hint0 bits 0:6
 *
 ************************************************************
 *
 * 64-bit layout
 *
 * addr  0:63  forwardingPtr
 * bits  0:43  compressed mark bits
 * bits 43:50  hint0
 * bits 50:57  hint1
 * bits 57:64  hint2
 */
enum {
#if defined(OMR_ENV_DATA64)
#if defined(OMR_THR_LOCK_NURSERY) || defined(OMR_INTERP_SMALL_MONITOR_SLOT)
	maxOffset = 64, // number of bits in compressed mark bits
	maxHints = 0, // 0 hints on 64 bit hardware
	hintSize = 7, // bits per hint
#else /* OMR_THR_LOCK_NURSERY */
	maxOffset = 43, // number of bits in compressed mark bits
	maxHints = 3, // 3 hints on 64 bit hardware
	hintSize = 7, // bits per hint
#endif /* OMR_THR_LOCK_NURSERY */
#else /* OMR_ENV_DATA64 */
#if defined(OMR_GC_MINIMUM_OBJECT_SIZE)
	maxOffset = 32, // number of bits in compressed mark bits
	maxHints = 0, // 0 hints on 32 bit hardware
	hintSize = 7, // bits per hint
#else /* OMR_GC_MINIMUM_OBJECT_SIZE */
#if defined(OMR_THR_LOCK_NURSERY)
	maxOffset = 32, // number of bits in compressed mark bits
	maxHints = 0, // 0 hints on 32 bit hardware
	hintSize = 6, // bits per hint
#else /* OMR_THR_LOCK_NURSERY */
	maxOffset = 22, // number of bits in compressed mark bits
	maxHints = 1, // 1 hint on 32 bit hardware
	hintSize = 6, // bits per hint
#endif /* OMR_THR_LOCK_NURSERY */
#endif /* OMR_GC_MINIMUM_OBJECT_SIZE */
#endif /* OMR_ENV_DATA64 */
	// Max value of a hint  log2(maxValue) == hintSize
	maxValue = MM_CompactScheme::sizeof_page / sizeof(uintptr_t),
	/* Invalid value is used when, because of object growth due to hash, the hint exceeds or
	 * is equal to maxValue. The hint would then be invalid.
	 */
	invalidValue = maxValue - 1,
};

class CompactTableEntry {
private:
	uintptr_t _addr;
	uintptr_t _bits;

private:
	MMINLINE uintptr_t
	getHintShiftCount(intptr_t index) const
	{
		uintptr_t shiftCount = maxOffset;

#if defined(OMR_ENV_DATA64)
		shiftCount += index * hintSize;
#endif /* OMR_ENV_DATA64 */

		/* assert that the shift value is within range (8 bits per byte) */
		Assert_MM_true(shiftCount < (sizeof(uintptr_t) * 8));

		return shiftCount;
	}

public:
	void
	operator=(const CompactTableEntry& otherEntry)
	{
		_addr = otherEntry._addr;
		_bits = otherEntry._bits;
	}

	void
	initialize(omrobjectptr_t addr)
	{
		_addr = (uintptr_t)addr | 3;   // both 32 and 64 bit hardware
		_bits = 0;
	}

	omrobjectptr_t
	getAddr()
	{
		return ((_addr & 3) != 3) ? 0 : (omrobjectptr_t) (_addr & ~3);
	}

	void
	setBit(intptr_t offset)
	{
		assume0(offset >= 0 && offset < maxOffset);
		_bits |= (uintptr_t)1 << offset;
	}

	uintptr_t
	getBit(intptr_t offset) const
	{
		assume0(offset >= 0 && offset < maxOffset);
		return _bits & ((uintptr_t)1 << offset);
	}

	intptr_t
	getObjectOrdinal(intptr_t offset) const
	{
		return countBits(_bits & makeMask(offset));
	}

	void
	setHint(intptr_t index, intptr_t value)
	{
		/* this function shouldn't be reached if maxHints is 0 */
		Assert_MM_true(maxHints > 0);
		Assert_MM_true((index >= 0) && (index < maxHints));
		Assert_MM_true(value > 0);
		if (value >= maxValue) {
			/* Adding a hash to an object may grow the object. It is possible that because
			 * of this growth, value may be greater than or equal to maxValue. If this
			 * happens, we want to set this hint as invalid.
			 */
			value = invalidValue;
		}

		_bits |= (uintptr_t)value << getHintShiftCount(index);
	}

	/**
	 * Answer the hint for the specified object in a chunk. The hint is the
	 * distance (in UDATAs) of the object from the beginning of the chunk.
	 * index is the ordinality of the object within the chunk, relative to the
	 * first object. i.e. the second object in the chunk has index 0.
	 * (The first object has an implicit hint of 0, and is not handled by this
	 * function).
	 *
	 * @parm[in] index the index of the object, where 0 is the second object
	 * in the chunk
	 * @return the distance of the relocated object from the beginning of the
	 * chunk, in UDATAs
	 */
	intptr_t
	getHint(intptr_t index) const
	{
		Assert_MM_true((index >= 0) && (index < maxHints));

#if defined(OMR_ENV_DATA64)
		intptr_t hint = (_bits >> getHintShiftCount(index)) & makeMask(hintSize);
#else /* OMR_ENV_DATA64 */
		intptr_t hint = _bits >> getHintShiftCount(index);
#endif /* OMR_ENV_DATA64 */
		Assert_MM_true( (hint != 0) && (hint < maxValue) );
		return hint;
	}

	CompactTableEntry()
		: _addr(0)
		, _bits(0)
	{}
};

bool
MM_CompactScheme::initialize(MM_EnvironmentBase *env)
{
	return true;
}

void
MM_CompactScheme::tearDown(MM_EnvironmentBase *env)
{
	/* Do nothing */
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_CompactScheme::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

size_t
MM_CompactScheme::getFreeChunkSize(omrobjectptr_t freeChunk)
{
	if (freeChunk == 0) {
		return 0;
	}
	/* This duplicates code in GC_ObjectHeapIteratorAddressOrderedList::nextObject() */
	if ( ! _extensions->objectModel.isDeadObject(freeChunk)) {
		return _extensions->objectModel.getConsumedSizeInBytesWithHeader(freeChunk);
	}
	if (_extensions->objectModel.isSingleSlotDeadObject(freeChunk)) {
		return _extensions->objectModel.getSizeInBytesSingleSlotDeadObject(freeChunk);
	}
	return _extensions->objectModel.getSizeInBytesMultiSlotDeadObject(freeChunk);
}

void
MM_CompactScheme::setFreeChunkSize(omrobjectptr_t deadObject, uintptr_t deadObjectSize)
{
	if (deadObjectSize == 0) {
		return;
	}

#if defined(DEBUG_PAINT_FREE)
	memset(deadObject, 0xAA, deadObjectSize);
#endif /* DEBUG_PAINT_FREE */
	assume0(deadObjectSize >= sizeof(uintptr_t));

	MM_HeapLinkedFreeHeader::fillWithHoles(deadObject, deadObjectSize);
}

size_t
MM_CompactScheme::setFreeChunk(omrobjectptr_t from, omrobjectptr_t to)
{
	size_t size = (size_t)to - (size_t)from;
	setFreeChunkSize(from, size);
	return size;
}

void
MM_CompactScheme::workerSetupForGC(MM_EnvironmentStandard *env, bool singleThreaded)
{
	createSubAreaTable(env, singleThreaded);
	setRealLimitsSubAreas(env);
	removeNullSubAreas(env);
	completeSubAreaTable(env);
}

void
MM_CompactScheme::masterSetupForGC(MM_EnvironmentStandard *env)
{
	_heap = _extensions->heap;
	_rootManager = _heap->getHeapRegionManager();
	_heapBase = (uintptr_t)_heap->getHeapBase();
	_compactTable = (CompactTableEntry*)_markingScheme->getMarkMap()->getMarkBits();
	_subAreaTable = (SubAreaEntry*)_extensions->sweepHeapSectioning->getBackingStoreAddress();
	_subAreaTableSize = _extensions->sweepHeapSectioning->getBackingStoreSize();
	_extensions->collectorLanguageInterface->compactScheme_languageMasterSetupForGC(env);
}

omrobjectptr_t
MM_CompactScheme::freeChunkEnd(omrobjectptr_t chunk)
{
	if (!chunk) {
		return NULL;
	}
	return (omrobjectptr_t)((uintptr_t)chunk + getFreeChunkSize(chunk));
}

/**
 *  Create sub areas table for regions.
 */
void
MM_CompactScheme::createSubAreaTable(MM_EnvironmentStandard *env, bool singleThreaded)
{
	/* finding whether there are memory limitations */
	uintptr_t max_subarea_num = _subAreaTableSize / sizeof(_subAreaTable[0]);
	uintptr_t necessary_subareas = 0;
	uintptr_t min_subarea_size;

	GC_HeapRegionIteratorStandard regionCounter(_rootManager);
	MM_HeapRegionDescriptorStandard *region = NULL;
	uintptr_t number_of_regions = 0;
	while(NULL != (region = regionCounter.nextRegion())) {
		if (region->isCommitted()) {
			number_of_regions += 1;
		}
	}

	necessary_subareas = 3 * number_of_regions + 1; //1 is for last seg

	Assert_MM_true(max_subarea_num > 0);

	if(max_subarea_num > necessary_subareas) {
		min_subarea_size = _heap->getMaximumPhysicalRange() / (max_subarea_num - necessary_subareas);
	} else {
		min_subarea_size = _heap->getMaximumPhysicalRange();
	}
	uintptr_t size = (DESIRED_SUBAREA_SIZE >= min_subarea_size) ?  DESIRED_SUBAREA_SIZE : min_subarea_size;


	/* Single threaded pass to set tentative sub area limits tentative limits are
	 * listed in freeChunk field. This field will be reset during the third pass.
	 */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		GC_HeapRegionIteratorStandard regionIterator(_rootManager);
		uintptr_t i = 0;
		while(NULL != (region = regionIterator.nextRegion())) {
			if (!region->isCommitted()) {
				continue;
			}
			void *lowAddress = region->getLowAddress();
			void *highAddress = region->getHighAddress();
			uintptr_t areaSize = region->getSize();
			MM_MemorySubSpace *memorySubSpace = region->getSubSpace();
			intptr_t state = SubAreaEntry::init;

			if (singleThreaded) {
				size = areaSize;
			}
			_subAreaTable[i].firstObject = (omrobjectptr_t)lowAddress;

			/* Calculate number of sub areas..take care to avoid overflow if size is large */
			uintptr_t numSubAreas = ((areaSize - 1) / size) + 1;

			for( uintptr_t subAreaNum=0; subAreaNum < numSubAreas; subAreaNum++){
				uint8_t *p = (uint8_t*)(((uintptr_t)lowAddress) + (subAreaNum * size));

				_subAreaTable[i].freeChunk = (omrobjectptr_t)p;
				_subAreaTable[i].memoryPool = memorySubSpace->getMemoryPool(p);
				_subAreaTable[i].state = state;
				_subAreaTable[i++].currentAction = SubAreaEntry::none;
			}
			_subAreaTable[i].freeChunk = (omrobjectptr_t)highAddress;
			_subAreaTable[i].memoryPool = NULL;
			_subAreaTable[i].firstObject = (omrobjectptr_t)highAddress;
			_subAreaTable[i].state = SubAreaEntry::end_segment;
			_subAreaTable[i++].currentAction = SubAreaEntry::none;
		}
		_subAreaTable[i].state = SubAreaEntry::end_heap;

		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

/**
 *  Set real limits for each subarea
 */
void
MM_CompactScheme::setRealLimitsSubAreas(MM_EnvironmentStandard *env)
{
	/* multi threaded pass to find real regions limits - where an object is found */
	for (uintptr_t i = 1; _subAreaTable[i].state != SubAreaEntry::end_heap; i++) {
		/* i=1 because first region starts with heapAlloc thus we don't need to find its first object */
		if ((SubAreaEntry::end_segment == _subAreaTable[i].state)
		|| (SubAreaEntry::end_segment == _subAreaTable[i - 1].state)
		) {
			/* skip the end_segment and its successor */
			continue;
		}

		if (changeSubAreaAction(env, &_subAreaTable[i], SubAreaEntry::setting_real_limits)) {
			uintptr_t *start = (uintptr_t*)pageStart(pageIndex(_subAreaTable[i].freeChunk));
			uintptr_t *end = (uintptr_t*)pageStart(pageIndex(_subAreaTable[i+1].freeChunk));
			MM_HeapMapIterator markedObjectIterator(_extensions, _markMap, start, end);
			omrobjectptr_t objectPtr = markedObjectIterator.nextObject();

			_subAreaTable[i].firstObject = objectPtr;
			Assert_MM_true(objectPtr == 0 || _markMap->isBitSet(objectPtr));
		}
	}
}

/**
 *  Remove empty sub areas from lists.
 */
void
MM_CompactScheme::removeNullSubAreas(MM_EnvironmentStandard *env)
{
	/*single threaded pass to eliminate null sub areas */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		_compactFrom = (omrobjectptr_t)_heap->getHeapTop();
		_compactTo   = (omrobjectptr_t)_heap->getHeapBase();
		uintptr_t j = 0;
		for (uintptr_t i = 0; _subAreaTable[i].state != SubAreaEntry::end_heap; i++) {
			if (NULL != _subAreaTable[i].firstObject) {
				_subAreaTable[j].firstObject = _subAreaTable[i].firstObject;
				_subAreaTable[j].memoryPool = _subAreaTable[i].memoryPool;
				_subAreaTable[j].state = _subAreaTable[i].state;
				if ((j > 0) && (_subAreaTable[j-1].state == SubAreaEntry::init)) {
					_compactFrom = (_compactFrom < _subAreaTable[j-1].firstObject) ? _compactFrom : _subAreaTable[j-1].firstObject;
					_compactTo = (_compactTo > _subAreaTable[j].firstObject) ? _compactTo : _subAreaTable[j].firstObject;
				}
				_subAreaTable[j].freeChunk = 0;
				j++;
			}
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

/**
 *  Complete setup for each sub area.
 */
void
MM_CompactScheme::completeSubAreaTable(MM_EnvironmentStandard *env)
{
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		MM_HeapRegionDescriptorStandard *region = NULL;

		/* Finally iterate over all memory pools and reset in preparation for
		 * rebuild of free list at end of compaction
		 */
		GC_HeapRegionIteratorStandard regionIterator2(_rootManager);
		while(NULL != (region = regionIterator2.nextRegion())) {
			if (!region->isCommitted()) {
				continue;
			}
			MM_MemorySubSpace *subspace = region->getSubSpace();
			MM_MemoryPool *memoryPool = subspace->getMemoryPool();
			memoryPool->reset(MM_MemoryPool::forCompact);
		}

		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

void
MM_CompactScheme::compact(MM_EnvironmentBase *envBase, bool rebuildMarkBits, bool aggressive)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uintptr_t objectCount = 0;
	uintptr_t byteCount = 0;
	uintptr_t skippedObjectCount = 0;
	uintptr_t fixupObjectsCount = 0;
	bool singleThreaded = false;

	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		/* Do any necessary initialization */
		/* TODO: Perhaps the task dispatch should occur internally within so that the initialization doesn't need to be
		 * done at a synchronize point?
		 */
		masterSetupForGC(env);
#if defined(DEBUG)
		_extensions->collectorLanguageInterface->compactScheme_verifyHeap(env, _markMap);
#endif /* DEBUG */

		/* Reset largestFreeEntry of all subSpaces at beginning of compaction */
		_extensions->heap->resetLargestFreeEntry();

		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	/* We force a single sub area compaction if:
	 *  o the compaction is aggressive. We use a single sub area per segment to avoid potentially having
	 *    multiple holes created per segment, thereby fragmenting the space. This will result in
	 *    singlethreaded compaction per segment, and so should only be done in extreme OOM situations.
	 *  o no slave GC threads
	 *  o the J9HOOK_MM_OMR_OBJECT_RENAME hook has registered users. JVMPI does not support events being issued
	 * 	  in parallel so we force single sub area compact to ensure all events issued under master GC thread.
	 */
	if (aggressive ||
		1 == env->_currentTask->getThreadCount() ||
		J9_EVENT_IS_HOOKED(_extensions->omrHookInterface, J9HOOK_MM_OMR_OBJECT_RENAME)) {
		singleThreaded = true;
	}

	env->_compactStats._setupStartTime = omrtime_hires_clock();
	workerSetupForGC(env, singleThreaded);
	env->_compactStats._setupEndTime = omrtime_hires_clock();

	/* If a single threaded compaction force compact to run on master thread. Required
	 * to ensure all events issued on master thread.
	 */
	if (!singleThreaded || env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		env->_compactStats._moveStartTime = omrtime_hires_clock();
		moveObjects(env, objectCount, byteCount, skippedObjectCount);
		env->_compactStats._moveEndTime = omrtime_hires_clock();

		if (!singleThreaded) {
			env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
			MM_AtomicOperations::sync();
		}

		env->_compactStats._fixupStartTime = omrtime_hires_clock();

		fixupObjects(env, fixupObjectsCount);


		env->_compactStats._fixupEndTime = omrtime_hires_clock();

		if (singleThreaded) {
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	}

	/* FixupRoots can always be done in parallel */
	env->_compactStats._rootFixupStartTime = omrtime_hires_clock();
	_extensions->collectorLanguageInterface->compactScheme_fixupRoots(env, this);
	env->_compactStats._rootFixupEndTime = omrtime_hires_clock();

	MM_AtomicOperations::sync();

	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		rebuildFreelist(env);

		MM_MemoryPool *memoryPool;
		MM_HeapMemoryPoolIterator poolIterator(env, _extensions->heap);

		while(NULL != (memoryPool = poolIterator.nextPool())) {
			memoryPool->postProcess(env, MM_MemoryPool::forCompact);
		}

		MM_AtomicOperations::sync();
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	if (rebuildMarkBits) {
		rebuildMarkbits(env);
		MM_AtomicOperations::sync();
	}

	_extensions->collectorLanguageInterface->compactScheme_workerCleanupAfterGC(env);

	env->_compactStats._movedObjects = objectCount;
	env->_compactStats._movedBytes = byteCount;
	env->_compactStats._fixupObjects = fixupObjectsCount;
}

void
MM_CompactScheme::flushPool(MM_EnvironmentStandard *env, MM_CompactMemoryPoolState *poolState)
{
	MM_MemoryPool *memoryPool = poolState->_memoryPool;

	if(poolState->_freeListHead) {
		memoryPool->addFreeEntries(env, poolState->_freeListHead, poolState->_previousFreeEntry, poolState->_freeHoles, poolState->_freeBytes);
	}

	/* Update the free memory values */
	memoryPool->setFreeMemorySize(poolState->_freeBytes);
	memoryPool->setFreeEntryCount(poolState->_freeHoles);
	memoryPool->setLargestFreeEntry(poolState->_largestFreeEntry);
}

void MM_CompactScheme::rebuildFreelist(MM_EnvironmentStandard *env)
{
	uintptr_t i = 0;
	MM_HeapRegionManager *regionManager = _heap->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;
	SubAreaEntry *subAreaTable = _subAreaTable;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (!region->isCommitted()) {
			continue;
		}
		MM_MemorySubSpace *memorySubSpace = region->getSubSpace();
		Assert_MM_true(region->getLowAddress() == subAreaTable[i].firstObject);

		MM_CompactMemoryPoolState poolStateObj;
		MM_CompactMemoryPoolState *poolState = &poolStateObj;

		void *currentFreeBase = NULL;
		uintptr_t currentFreeSize = 0;

		/* Initialize current memory pool sweep chunk */
		poolState->_memoryPool = subAreaTable[i].memoryPool;

		do {
			if (NULL != subAreaTable[i].freeChunk) {
				if (subAreaTable[i].freeChunk == subAreaTable[i].firstObject) {
					/* The entire sub area is free */
					if (NULL == currentFreeBase) {
						currentFreeBase = (void *)subAreaTable[i].firstObject; //orizzz - round down to page
					}
				} else {
					/* There is some free area in the sub area but not all of it */
					if (NULL != currentFreeBase) {
						currentFreeSize = (uintptr_t)subAreaTable[i].firstObject - (uintptr_t)currentFreeBase;

#if defined(DEBUG_PAINT_FREE)
						memset(currentFreeBase, 0xBB, currentFreeSize);
#endif /* DEBUG_PAINT_FREE */
						addFreeEntry(env, memorySubSpace, poolState, currentFreeBase, currentFreeSize);
					}
					currentFreeSize = 0;
					currentFreeBase = (void *)subAreaTable[i].freeChunk;
				}
			} else {
				/* Either there is no free area in the sub area or sub area is
				 * a fixup_only sub area, i.e. IC is active
				 */
				if (NULL != currentFreeBase) {
					currentFreeSize = (uintptr_t)subAreaTable[i].firstObject - (uintptr_t)currentFreeBase;

#if defined(DEBUG_PAINT_FREE)
					memset(currentFreeBase, 0xCC, currentFreeSize);
#endif /* DEBUG_PAINT_FREE */
					addFreeEntry(env, memorySubSpace, poolState, currentFreeBase, currentFreeSize);
				}

				currentFreeBase = NULL;
				currentFreeSize = 0;
			}
        } while (subAreaTable[i++].state != SubAreaEntry::end_segment);

		Assert_MM_true(currentFreeBase == NULL);

		if (NULL != poolState->_freeListHead) {
			/* Terminate the free list with NULL*/
			poolState->_memoryPool->createFreeEntry(env, poolState->_previousFreeEntry,
													(uint8_t *)poolState->_previousFreeEntry + poolState->_previousFreeEntrySize);
		}
		flushPool(env, poolState);
	}
}

/*
 * Call appropriate Memory Pool to add a new free entry to the pool. If the free entry
 * spans more than one subpool then it will be split into 2 free entries.
 *
 * @param previousFreeEntry  Address of previous entry added
 * @param previousFreeEntrySize Size of previous entry
 * @param currentFreeBase Address of new entry to be added to pool
 * @param currentFreeSize Size of new entry
 *
 */
MMINLINE void
MM_CompactScheme::addFreeEntry(MM_EnvironmentStandard *env, MM_MemorySubSpace *memorySubSpace, MM_CompactMemoryPoolState *poolState, void *currentFreeBase, uintptr_t currentFreeSize)
{
	void *highAddr;
	uintptr_t lowChunkSize, highChunkSize;
	MM_MemoryPool *lowPool, *highPool;

	/* Determine which memory pool the free entry belongs in and if
	 * the entry spans the top of the pool
	 */
	lowPool = memorySubSpace->getMemoryPool(env, currentFreeBase, (uint8_t *)currentFreeBase + currentFreeSize, highAddr);

	/* Does new entry belong to same pool as last entry ? */
	if (lowPool != poolState->_memoryPool) {
		/* No.. So flush all statistics for current pool */
		flushPool(env, poolState);
		/* ..and reset all stats data for next pool */
		poolState->clear();

		poolState->_memoryPool = lowPool;
	}

	assume0(lowPool != NULL);

	lowChunkSize = (highAddr ? (uint8_t*)highAddr - (uint8_t*)currentFreeBase : currentFreeSize);
	if (lowChunkSize >  lowPool->getMinimumFreeEntrySize()) {
		if (!poolState->_freeListHead) {
			poolState->_freeListHead = (MM_HeapLinkedFreeHeader *)currentFreeBase;
		}

		lowPool->createFreeEntry(env, currentFreeBase, (uint8_t *)currentFreeBase + lowChunkSize, poolState->_previousFreeEntry, NULL);

		/* Update chunk stats */
		poolState->_freeBytes += lowChunkSize;
		poolState->_freeHoles += 1;
		poolState->_largestFreeEntry = OMR_MAX(poolState->_largestFreeEntry, lowChunkSize);

		poolState->_previousFreeEntry = (MM_HeapLinkedFreeHeader *)currentFreeBase;
		poolState->_previousFreeEntrySize = lowChunkSize;

	} else {
		lowPool->abandonHeapChunk(currentFreeBase, (uint8_t *)currentFreeBase + lowChunkSize);
	}

	/* Did range span top of current pool ? */
	if(NULL != highAddr) {
		/* calculate size of high chunk while we know base of low chunk..*/
		highChunkSize = ((uint8_t *)currentFreeBase + currentFreeSize)  - (uint8_t*)highAddr;

		/* Then flush all statistics for current pool */
		flushPool(env, poolState);

		/* Reset all stats data for next pool */
		poolState->clear();
		highPool = memorySubSpace->getMemoryPool(highAddr);
		poolState->_memoryPool = highPool;

		if ( highChunkSize >  highPool->getMinimumFreeEntrySize()) {

			/* this must be first free chunk in this pool */
			poolState->_freeListHead = (MM_HeapLinkedFreeHeader *)highAddr;

			highPool->createFreeEntry(env, highAddr, (uint8_t *)highAddr + highChunkSize, NULL, NULL);

			/* Update chunk stats */
			poolState->_freeBytes += highChunkSize;
			poolState->_freeHoles += 1;
			poolState->_largestFreeEntry = OMR_MAX(poolState->_largestFreeEntry, highChunkSize);

			poolState->_previousFreeEntry = (MM_HeapLinkedFreeHeader *)highAddr;
			poolState->_previousFreeEntrySize = highChunkSize;
		} else {
			highPool->abandonHeapChunk(highAddr, (uint8_t *)highAddr + highChunkSize);
		}
	}
}


void
MM_CompactScheme::moveObjects(MM_EnvironmentStandard *env, uintptr_t &objectCount, uintptr_t &byteCount, uintptr_t &skippedObjectCount)
{
	MM_HeapRegionManager *regionManager = _heap->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;
	SubAreaEntry *subAreaTable = _subAreaTable;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (!region->isCommitted()) {
			continue;
		}
		intptr_t i;
		for (i = 0; subAreaTable[i].state != SubAreaEntry::end_segment; i++) {
			if (changeSubAreaAction(env, &subAreaTable[i], SubAreaEntry::evacuating)) {
				evacuateSubArea(env, region, subAreaTable, i, objectCount, byteCount, skippedObjectCount);
			}
		}
        /* Number of regions in regionTable, including
         * the end_segment region, is i+1 */
        subAreaTable += (i+1);
	}
}

/* Create two free chunks: the first is (from:to_aligned), and the second
 * is (to_aligned:to), where to_aligned=ALIGN(to,page_size). Return the size
 * of the FIRST chunk (if exists), or zero otherwise.
 *
 * The idea behind the two chunks is that the second chunk is discarded to
 * prevent race conditions on multi-thread evacuation.
 */
size_t
MM_CompactScheme::setFreeChunkPageAligned(omrobjectptr_t from, omrobjectptr_t to)
{
	assume0(from && to);
	omrobjectptr_t to_aligned = pageStart(pageIndex(to));

	if (from >= to_aligned) {
		/* This is the special case where only the SECOND of the two
		 * chunks is created.  Return value is 0... think of the first
		 * chunk as having zero length.
		 */
		setFreeChunk(from, to);
		return 0;
	}

	if (to == to_aligned) {
		/* Here only the FIRST of the two free chunks is created.*/
		return setFreeChunk(from, to_aligned);
	}

	/* Both free chunks are created; returned size of the FIRST one. */
	setFreeChunk(to_aligned, to);
	return setFreeChunk(from, to_aligned);
}


void
MM_CompactScheme::evacuateSubArea(MM_EnvironmentStandard *env, MM_HeapRegionDescriptorStandard *subAreaRegion, SubAreaEntry *subAreaTableEvacuate, intptr_t i, uintptr_t &objectCount, uintptr_t &byteCount, uintptr_t &skippedObjectCount)
{
	uintptr_t minFreeChunk = _extensions->tlhMinimumSize;

	if (subAreaTableEvacuate[i].state != SubAreaEntry::init) {
		Assert_MM_true(subAreaTableEvacuate[i].state == SubAreaEntry::fixup_only);
		return;
	}
	uintptr_t nobjects = 0;
	uintptr_t nbytes = 0;
	omrobjectptr_t freeChunk;
	omrobjectptr_t firstObject = subAreaTableEvacuate[i].firstObject;
	omrobjectptr_t endObject = subAreaTableEvacuate[i+1].firstObject;
	omrobjectptr_t objectPtr = firstObject;
	MM_MemorySubSpace *subspace = subAreaRegion->getSubSpace();

    intptr_t j = -1;
    do {
    	freeChunk = 0;
        for (j++; j < i; j++) { // keeps searching from the prev. value to prevent inf loop
        	if ((subAreaTableEvacuate[j].state == SubAreaEntry::ready) &&
        		(SubAreaEntry::ready == MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[j].state, SubAreaEntry::ready, SubAreaEntry::busy)))
			{
				MM_AtomicOperations::loadSync();
				freeChunk = subAreaTableEvacuate[j].freeChunk;
				break;
			}
        }

        if (j == i) {
        	/* freeChunk == 0 */
            break; // can't evacuate
        }

        nobjects = nbytes = 0;
        objectPtr = doCompact(env, subspace, objectPtr, endObject, freeChunk, nobjects, nbytes, true);

        /* Free chunks initially get into the table after compaction,
         * so the problematic last page had already been truncated by now.
         */
	 	size_t size = getFreeChunkSize(freeChunk);
	 	subAreaTableEvacuate[j].freeChunk = freeChunk;

	 	Trc_MM_CompactScheme_evacuateSubArea_evacuated(env->getLanguageVMThread(), firstObject, endObject, nbytes, subAreaTableEvacuate[j].firstObject);
		if (size < minFreeChunk) {
			Trc_MM_CompactScheme_evacuateSubArea_bytesRemainingIgnored(env->getLanguageVMThread(), size, subAreaTableEvacuate[j].firstObject);
		} else {
			Trc_MM_CompactScheme_evacuateSubArea_bytesRemaining(env->getLanguageVMThread(), size, subAreaTableEvacuate[j].firstObject);
		}

        objectCount += nobjects;
        byteCount += nbytes;

		/* change state from 'busy' to 'ready' or 'full' */
		MM_AtomicOperations::storeSync();

        if (size < minFreeChunk) {
        	uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[j].state, SubAreaEntry::busy, SubAreaEntry::full);
        	Assert_MM_true(state == SubAreaEntry::busy);
        }
        else {
        	uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[j].state, SubAreaEntry::busy, SubAreaEntry::ready);
        	Assert_MM_true(state == SubAreaEntry::busy);
        }
    } while (objectPtr);

	if (objectPtr == 0) {
		/* All objects in the sub area were successfully evacuated. */
		size_t size = setFreeChunkPageAligned(firstObject, endObject);

		if (size < minFreeChunk) {
			Trc_MM_CompactScheme_evacuateSubArea_subAreaFullIgnored(env->getLanguageVMThread(), firstObject, endObject, size);
		} else {
			Trc_MM_CompactScheme_evacuateSubArea_subAreaFull(env->getLanguageVMThread(), firstObject, endObject, size);
		}

		subAreaTableEvacuate[i].freeChunk = firstObject;

		MM_AtomicOperations::storeSync();
		if (size < minFreeChunk) {
			uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::full);
			Assert_MM_true(state == SubAreaEntry::init);
		} else {
			uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::ready);
			Assert_MM_true(state == SubAreaEntry::init);
		}
		return;
	}

	if (objectPtr == firstObject) { // nothing evacuated
		/* Skip over the objects in the beginning of the sub area until
		 * the first empty bubble is found.  Usually, sub areas start with
		 * an object, but the first sub area is an exception.  It starts with
		 * segment->heapBase even if there is no object there.  In that case,
		 * the number of skipped objects is zero.
		 */
		GC_ObjectHeapIteratorAddressOrderedList objectHeapIterator(_extensions, firstObject, pageStart(pageIndex(endObject)), true);
		omrobjectptr_t lastLive = firstObject;
		while (NULL != (objectPtr = objectHeapIterator.nextObject())) {
			if (objectHeapIterator.isDeadObject() || !_markMap->isBitSet(objectPtr)) {
				break;
			}
			lastLive = objectPtr;
			skippedObjectCount++;
		}
		/* We found the first free chunk in the sub area.
		 * It is pointed to by objectPtr, unless it is zero,
		 * which means that the sub area contains no free chunks.
		 */
		if (objectPtr == 0) {
			subAreaTableEvacuate[i].freeChunk = 0;
			omrobjectptr_t lastLiveEnd = (omrobjectptr_t)((uintptr_t)lastLive + _extensions->objectModel.getConsumedSizeInBytesWithHeader(lastLive));
			Assert_MM_true(lastLiveEnd >= pageStart(pageIndex(endObject)) && lastLiveEnd <= endObject);
			setFreeChunk(lastLiveEnd, endObject);

			Trc_MM_CompactScheme_evacuateSubArea_subAreaAlreadyCompacted(env->getLanguageVMThread(), firstObject, endObject);

			MM_AtomicOperations::storeSync();
			uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::full);
			Assert_MM_true(state == SubAreaEntry::init);
			return;
		}

		freeChunk = objectPtr;
		Assert_MM_true(!_markMap->isBitSet(freeChunk));
		/* The free chunk that we found may be followed by more free chunks.
		 * We search for the first object after those free chunks.
		 */
		while (NULL != (objectPtr = objectHeapIterator.nextObject())) {
			if (_markMap->isBitSet(objectPtr)) {
				break;
			}
		}
		/* Now objectPtr either points to the first object after freeChunk,
		 * or it is zero.
		 */
		if (objectPtr == 0) {
			/* The sub area contains zero or more leading objects,
			 * which are followed by a free
			 * chunk, pointed to by freeChunk (more free chunks may follow
			 * the first one).  There are no objects in the sub area that
			 * start after these free chunks, so the sub area is already
			 * compacted.  Moreover, it may contain no objects at all (only
			 * if it is the first sub area).
			 */

			/* BEWARE: freeChunk may start on the same page as endObject.
			 * In this case setFreeChunkPageAligned returns 0 to ensure
			 * that freeChunk will not be used. */
			size_t size = setFreeChunkPageAligned(freeChunk, endObject);
			if (size < minFreeChunk) {
				subAreaTableEvacuate[i].freeChunk = 0;
			} else {
				subAreaTableEvacuate[i].freeChunk = freeChunk;
			}

			if (size < minFreeChunk) {
				Trc_MM_CompactScheme_evacuateSubArea_subAreaAlreadyCompactedFreeSpaceIgnored(env->getLanguageVMThread(), firstObject, endObject);
			} else {
				Trc_MM_CompactScheme_evacuateSubArea_subAreaAlreadyCompactedFreeSpaceRemaining(env->getLanguageVMThread(), firstObject, endObject, size);
			}

			MM_AtomicOperations::storeSync();
			if (size < minFreeChunk) {
				uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::full);
				Assert_MM_true(state == SubAreaEntry::init);
			} else {
				uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::ready);
				Assert_MM_true(state == SubAreaEntry::init);
			}
			return;
		}
		/* The sub area contains zero or more leading objects,
		 * which are followed by a free
		 * chunk, pointed to by freeChunk (more free chunks may immediately
		 * follow the first one).  After these free chunks there is at least one
		 * object, pointed to by objectPtr.
		 */
		Assert_MM_true(_markMap->isBitSet(objectPtr));

		nobjects = nbytes = 0;
		doCompact(env, subspace, objectPtr, endObject, freeChunk, nobjects, nbytes, false);
		Assert_MM_true(freeChunk);
		size_t size = setFreeChunkPageAligned(freeChunk, endObject);
		if (size < minFreeChunk) {
			subAreaTableEvacuate[i].freeChunk = 0;
		} else {
			subAreaTableEvacuate[i].freeChunk = freeChunk;
		}

		if (size < minFreeChunk)  {
			Trc_MM_CompactScheme_evacuateSubArea_subAreaCompactedAFreeSpaceIgnored(env->getLanguageVMThread(), firstObject, endObject, nbytes, size);
		} else {
			Trc_MM_CompactScheme_evacuateSubArea_subAreaCompactedAFreeSpaceRemaining(env->getLanguageVMThread(), firstObject, endObject, nbytes, size);
		}

		objectCount += nobjects;
		byteCount += nbytes;
		MM_AtomicOperations::storeSync();
		if (size < minFreeChunk) {
			uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::full);
			Assert_MM_true(state == SubAreaEntry::init);
		} else {
			uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::ready);
			Assert_MM_true(state == SubAreaEntry::init);
		}
		return;
	}


	/* Some, but not all objects were evacuated from the sub area i.  The remaining
	 * objects are compacted using sliding within the sub area. */
	freeChunk = firstObject;
	setFreeChunk(freeChunk, objectPtr);
	nobjects = nbytes = 0;
	doCompact(env, subspace, objectPtr, endObject, freeChunk, nobjects, nbytes, false);
	Assert_MM_true(freeChunk);
	size_t size = setFreeChunkPageAligned(freeChunk, endObject);
	if (size < minFreeChunk) {
		subAreaTableEvacuate[i].freeChunk = 0;
	} else {
		subAreaTableEvacuate[i].freeChunk = freeChunk;
	}

	if (size < minFreeChunk)  {
		Trc_MM_CompactScheme_evacuateSubArea_subAreaCompactedBFreeSpaceIgnored(env->getLanguageVMThread(), firstObject, endObject, nbytes, size);
	} else {
		Trc_MM_CompactScheme_evacuateSubArea_subAreaCompactedBFreeSpaceRemaining(env->getLanguageVMThread(), firstObject, endObject, nbytes, size);
		Trc_OMRMM_CompactScheme_evacuateSubArea_subAreaCompactedBFreeSpaceRemaining(env->getOmrVMThread(), firstObject, endObject, nbytes, size);

	}

	objectCount += nobjects;
	byteCount += nbytes;

	MM_AtomicOperations::storeSync();
	if (size < minFreeChunk) {
		uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::full);
		Assert_MM_true(state == SubAreaEntry::init);
	} else {
		uintptr_t state = MM_AtomicOperations::lockCompareExchange(&subAreaTableEvacuate[i].state, SubAreaEntry::init, SubAreaEntry::ready);
		Assert_MM_true(state == SubAreaEntry::init);
	}
	return;
}

/* Save forwarding information for relocated object objectPtr.
 *
 * The new location of the first relocated object on a page
 * is forwardingPtr.  The new location of the following objects
 * is deduced from the forwardingPtr and the sequential number
 * of the object on the page.
 *
 * We need to keep some state in the caller's local variables
 * page and counter.
 * Objects are counted on the page for hints.  The first
 * object (count=0) does not need a hint, because its
 * address is directly stored in the addr field.  Depending
 * on the available space, one or more hints are kept for
 * the following objects.
 */
MMINLINE void
MM_CompactScheme::saveForwardingPtr(CompactTableEntry &entry, omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr, intptr_t &page, intptr_t &counter)
{
	if (page != pageIndex(objectPtr)) {
		if (page != -1) {
			_compactTable[page] = entry;
		}
		page = pageIndex(objectPtr);
		entry.initialize(forwardingPtr);
		counter = 0;
	}

	uintptr_t offset = compressedPageOffset(objectPtr);
	assume0(offset*sizeof(J9Object) <= sizeof_page);
	entry.setBit(offset);

	if (counter >= 1 && counter <= maxHints) {
		size_t byteOffset = (uintptr_t)forwardingPtr-(uintptr_t)entry.getAddr();
		entry.setHint(counter-1, byteOffset / sizeof(uintptr_t));
	}

	counter++;
}

/* Move objects between start and finish (not including)
 * to deadObject.  Both start and finish
 * must point to valid objects, where start is the first object in
 * sub area i, and finish is the first object in sub area i+1.
 *
 * Depending on the value of the evacuate flag,
 * objects are either shifted or evacuated.
 *
 * If evacuate==false, input values of deadObject and deadObjectSize
 * are ignored.
 *
 * RETURN VALUE
 *
 * If evacuate==false, the function always succeeds and returns 0.
 * If evacuate==true, the function may fail if the intended free space
 * is too small to contain all the objects.  In this case it returns the
 * pointer to the first object that could not be evacuated.
 *
 * New values of deadObject and deadObjectSize returned by reference.
 *
 * Thread-safe -- does not modify global data, except markbits.
 */
omrobjectptr_t
MM_CompactScheme::doCompact(MM_EnvironmentStandard *env, MM_MemorySubSpace *memorySubSpace, omrobjectptr_t start, omrobjectptr_t finish, omrobjectptr_t &deadObject, uintptr_t &nobjects, uintptr_t &nbytes, bool evacuate)
{
	/* we use the MM_HeapMapWordIterator in this function so ensure that it is safe (can't be tested during init since assertions don't work at that point) */
	Assert_MM_true(0 == (sizeof_page % J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP));
	/* Sanity checks */
	assume0(start && finish && finish > start);
	assume0(_markMap->isBitSet(start));
	assume0(!evacuate || deadObject);

	uintptr_t deadObjectSize = getFreeChunkSize(deadObject);

	MM_HeapMapIterator markedObjectIterator(_extensions, _markMap, (uintptr_t *)start, (uintptr_t *)pageStart(pageIndex(finish)));

	omrobjectptr_t objectPtr = 0;
	omrobjectptr_t nextObject = 0;
	uintptr_t evacuatedBytes = 0;
	intptr_t page = -1; /* invalid value */
	intptr_t counter = 0; /* obj on page, first is zero */
	CompactTableEntry entry;
	for (objectPtr = markedObjectIterator.nextObject(); objectPtr != 0; objectPtr = nextObject) {
		nextObject = markedObjectIterator.nextObject();

		if (evacuate && (pageIndex(objectPtr) != page)) {
			/* We should not start evacuating objects from a page unless we are
			 * sure there is enough space for ALL of them.
			 */
			uintptr_t pageByteCount = 0;
			void *baseOfPage = pageStart(pageIndex(objectPtr));
			for (uintptr_t bias = 0; bias < sizeof_page; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
				void *baseAddress = (void *)((uintptr_t)baseOfPage + bias);
				MM_HeapMapWordIterator pagePieceIterator(_markMap, baseAddress);
				omrobjectptr_t p = NULL;
				while ((NULL != (p = pagePieceIterator.nextObject())) && (pageByteCount <= deadObjectSize)) {
					pageByteCount += _extensions->objectModel.getConsumedSizeInBytesWithHeaderForMove(p);
				}
			}
			if (pageByteCount > deadObjectSize) {
				break;
			}
			evacuatedBytes += pageByteCount;
		}

		uintptr_t objectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
		uintptr_t objectSizeAfterMove = _extensions->objectModel.getConsumedSizeInBytesWithHeaderForMove(objectPtr);

		assume0(!evacuate || objectSizeAfterMove <= deadObjectSize);

		/* Passed by reference: page, counter.  MODIFIED INSIDE the funcall. */
		saveForwardingPtr(entry, objectPtr, deadObject, page, counter);

		/* newObjectHash may cause objects to grow */
		if(deadObject == objectPtr) {
			/* if deadObject == objectPtr just continue with the walk. Do not move any more objects
			 * as this is not needed and it will cause corruption if objects grow after this point.
			 */
			deadObject = (omrobjectptr_t)((uintptr_t)deadObject + objectSize);
			continue;
		}

		TRIGGER_J9HOOK_MM_OMR_OBJECT_RENAME(env->getExtensions()->omrHookInterface, env->getOmrVMThread(), objectPtr, deadObject);

		nobjects++;
		nbytes += objectSizeAfterMove;

		_extensions->objectModel.preMove(env->getOmrVMThread(), objectPtr);

		if (evacuate) {
			deadObjectSize -= objectSizeAfterMove;
			memcpy(deadObject, objectPtr, objectSize);
		} else {
			//deadObjectSize = (uintptr_t)objectPtr - (uintptr_t)deadObject;
			memmove(deadObject, objectPtr, objectSize);
		}

		_extensions->objectModel.postMove(env->getOmrVMThread(), deadObject);

		deadObject = (omrobjectptr_t)((uintptr_t)deadObject+objectSizeAfterMove);
	}

	if (page != -1) {
		_compactTable[page] = entry;
	}

	if (!evacuate) {
		deadObjectSize = (uintptr_t)finish-(uintptr_t)deadObject;
		setFreeChunkSize(deadObject, deadObjectSize);
	} else {
		if (deadObjectSize == 0) {
			deadObject = 0;
		} else {
			setFreeChunkSize(deadObject, deadObjectSize);
#if defined(DEBUG)
		if (deadObjectSize > 2*sizeof(uintptr_t)) {
			uintptr_t junk = _extensions->objectModel.getSizeInBytesMultiSlotDeadObject(deadObject);
			assume0(junk == deadObjectSize);
		}
#endif /* DEBUG */
		}
	}

	return objectPtr;
}

omrobjectptr_t
MM_CompactScheme::getForwardingPtr(omrobjectptr_t objectPtr) const
{
	if (objectPtr < _compactFrom || objectPtr >= _compactTo) {
		return objectPtr;
	}

	intptr_t index = pageIndex(objectPtr);
	omrobjectptr_t forwardingPtr = _compactTable[index].getAddr();
	if (forwardingPtr == 0) {
		forwardingPtr = objectPtr;
		MM_CompactSchemeFixupObject::verifyForwardingPtr(objectPtr, forwardingPtr);
		return forwardingPtr;
	}

	intptr_t offset = compressedPageOffset(objectPtr);

	//OMRTODO: dagar what to replace size with?
	//Assert_MM_true(offset * sizeof(J9Object) <= sizeof_page);

	intptr_t n = _compactTable[index].getObjectOrdinal(offset);
	if (n == 0) {
		if (_compactTable[index].getBit(offset) == 0) {
			forwardingPtr = objectPtr; // first objects on the first page
		}
		MM_CompactSchemeFixupObject::verifyForwardingPtr(objectPtr, forwardingPtr);
		return forwardingPtr;
	}

	if (n <= maxHints) {
		Assert_MM_true((n >= 1) && (n <= maxHints));
		intptr_t hint = _compactTable[index].getHint(n-1);
		/* Check if the hint is invalid due to awkward object growth on compaction. */
		if (hint != invalidValue) {
			uintptr_t byteOffset = sizeof(uintptr_t) * hint;
			forwardingPtr = (omrobjectptr_t)((uintptr_t)forwardingPtr + byteOffset);
			MM_CompactSchemeFixupObject::verifyForwardingPtr(objectPtr, forwardingPtr);
			return forwardingPtr;
		}
	} else if (maxHints > 0) {
		/* May not have any hints */
		for (intptr_t i = maxHints - 1; i >= 0; --i) {
			intptr_t hint = _compactTable[index].getHint(i);
			/* Check if the hint is invalid due to awkward object growth on compaction. */
			if (hint != invalidValue) {
				n -= i + 1;
				Assert_MM_true(n > 0);
				uintptr_t byteOffset = sizeof(uintptr_t) * hint;
				forwardingPtr = (omrobjectptr_t)((uintptr_t)forwardingPtr + byteOffset);
				break;
			}
		}
	}

	for (intptr_t i=0; i < n; i++) {
		size_t size = _extensions->objectModel.getConsumedSizeInBytesWithHeader(forwardingPtr);
		forwardingPtr = (omrobjectptr_t)((uintptr_t)forwardingPtr + size);
	}

	MM_CompactSchemeFixupObject::verifyForwardingPtr(objectPtr, forwardingPtr);
	return forwardingPtr;
}

void
MM_CompactScheme::fixupObjects(MM_EnvironmentStandard *env, uintptr_t& objectCount)
{
	MM_HeapRegionManager *regionManager = _heap->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;
	SubAreaEntry *subAreaTable = _subAreaTable;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (!region->isCommitted()) {
			continue;
		}
		intptr_t i;
        for (i = 0; subAreaTable[i].state != SubAreaEntry::end_segment; i++) {
        	if (changeSubAreaAction(env, &subAreaTable[i], SubAreaEntry::fixing_up)) {
        		fixupSubArea(env, subAreaTable[i].firstObject, subAreaTable[i+1].firstObject, subAreaTable[i].state == SubAreaEntry::fixup_only, objectCount);
			}
        }
        /* Number of regions in regionTable, including
         * the end_segment region, is i+1 */
        subAreaTable += (i+1);
	}
}

void
MM_CompactScheme::fixupObjectSlot(GC_SlotObject* slotObject)
{
	omrobjectptr_t forwardedPtr = getForwardingPtr(slotObject->readReferenceFromSlot());
	slotObject->writeReferenceToSlot(forwardedPtr);
}

void
MM_CompactScheme::fixupSubArea(MM_EnvironmentStandard *env, omrobjectptr_t firstObject, omrobjectptr_t finish,  bool markedOnly, uintptr_t& objectCount)
{
	/* if start address is NULL, means we don't need to fix this subarea */
	if (NULL == firstObject) {
		return;
	}

	MM_CompactSchemeFixupObject fixupObject(env, this);

	if (markedOnly) {
		finish = pageStart(pageIndex(finish));
		MM_HeapMapIterator markedObjectIterator(_extensions, _markMap, (uintptr_t *)firstObject, (uintptr_t *)finish);
		omrobjectptr_t objectPtr = NULL;
		while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
			objectCount++;
			fixupObject.fixupObject(env, objectPtr);
		}
	} else {
		GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, firstObject, finish, false);

		omrobjectptr_t objectPtr;
		while (NULL != (objectPtr = objectIterator.nextObject())) {
			objectCount++;
			fixupObject.fixupObject(env, objectPtr);
		}
	}
}

void
MM_CompactScheme::rebuildMarkbits(MM_EnvironmentStandard *env)
{
	MM_HeapRegionManager *regionManager = _heap->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;
	SubAreaEntry *subAreaTable = _subAreaTable;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (!region->isCommitted()) {
			continue;
		}
		intptr_t i;
        for (i = 0; subAreaTable[i].state != SubAreaEntry::end_segment; i++) {
        	/* We only have to rebuild the markbits for sub areas which contain moved objects */
        	if (subAreaTable->state != SubAreaEntry::fixup_only) {
	        	if (changeSubAreaAction(env, &subAreaTable[i], SubAreaEntry::rebuilding_mark_bits)) {
	        		rebuildMarkbitsInSubArea(env, region, subAreaTable, i);
				}
        	}
        }
        /* Number of regions in regionTable, including
         * the end_segment region, is i+1 */
        subAreaTable += (i+1);
	}
}


void
MM_CompactScheme::rebuildMarkbitsInSubArea(MM_EnvironmentStandard *env, MM_HeapRegionDescriptorStandard *region, SubAreaEntry *subAreaTable, intptr_t i)
{
	omrobjectptr_t start = subAreaTable[i].firstObject;
	omrobjectptr_t end = subAreaTable[i + 1].firstObject;

	/* Zero the markbits to begin with */
	_markMap->setBitsInRange(env, pageStart(pageIndex(start)), pageStart(pageIndex(end)), true);

	/* If the entire region is free then there are no mark bits to set */
	if(subAreaTable[i].freeChunk == subAreaTable[i].firstObject) {
		return;
	}

	/* Now set the markbits according to the new object locations */
	GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, start, end, false);
	omrobjectptr_t objectPtr;
	while(NULL != (objectPtr = objectIterator.nextObject())) {
		_markMap->setBit(objectPtr);
	}
}

/*
 * fixHeapForWalk isn't required in Phase 4, since it simply attempts to fix up any areas which
 * weren't compacted. In Tarok, regions are entirely compacted or entirely fixed up. There is
 * no case in which a region may be only partially compacted.
 */
void
MM_CompactScheme::fixHeapForWalk(MM_EnvironmentBase *env)
{
	MM_CompactFixHeapForWalkTask fixHeapForWalkTask(env, _dispatcher, this);
	_dispatcher->run(env, &fixHeapForWalkTask);
}

void
MM_CompactScheme::parallelFixHeapForWalk(MM_EnvironmentBase *env)
{
	MM_HeapRegionManager *regionManager = _heap->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;
	SubAreaEntry *subAreaTable = _subAreaTable;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (!region->isCommitted()) {
			continue;
		}
		intptr_t i = 0;
		MM_MemorySubSpace *memorySubSpace = region->getSubSpace();
        for (i = 0; subAreaTable[i].state != SubAreaEntry::end_segment; i++) {
        	if (subAreaTable[i].state == SubAreaEntry::fixup_only) {
	        	if (changeSubAreaAction(env, &subAreaTable[i], SubAreaEntry::fixing_heap_for_walk)) {
	        		omrobjectptr_t start = subAreaTable[i].firstObject;
					omrobjectptr_t end   = subAreaTable[i].firstObject;
					omrobjectptr_t alignedEnd = pageStart(pageIndex(end));

					GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, start, end, false);
					omrobjectptr_t objectPtr = NULL;
					while (NULL != (objectPtr = objectIterator.nextObject())) {
						if (objectPtr >= alignedEnd || !_markMap->isBitSet(objectPtr)) {
							// this is a hole that looks like an object and should be made to look like a hole
							uintptr_t deadObjectByteSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
							memorySubSpace->abandonHeapChunk(objectPtr, ((uint8_t*)objectPtr) + deadObjectByteSize);
						}
					}
				}
        	}
        }
        /* Number of regions in regionTable, including
         * the end_segment region, is i+1 */
        subAreaTable += i + 1;
	}
}

bool
MM_CompactScheme::changeSubAreaAction(MM_EnvironmentBase *env, SubAreaEntry * entry, uintptr_t newAction)
{
	bool successful = false;
	uintptr_t previousAction = entry->currentAction;
	if (previousAction != newAction ) {
		uintptr_t action = MM_AtomicOperations::lockCompareExchange(&entry->currentAction, previousAction, newAction);
		if (action == previousAction) {
			successful = true;
		} else {
			/* during this phase it's only legitimate to change to newAction so if currentAction changed underneath us that had better be its new value */
			Assert_MM_true(action == newAction);
		}
	}

	return successful;
}

#endif /* OMR_GC_MODRON_COMPACTION */
