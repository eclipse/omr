/*******************************************************************************
 * Copyright (c) 1991, 2022 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcfg.h"
#include "omr.h"
#include "GlobalCollector.hpp"

#include <string.h>

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentGC.hpp"
#include "ConcurrentGCStats.hpp"
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
#include "Configuration.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "MarkMap.hpp"
#include "MarkingScheme.hpp"
#include "Task.hpp"
#if defined(OMR_GC_REALTIME)
#include "WorkPacketsSATB.hpp"
#endif /* defined(OMR_GC_REALTIME) */
#include "RememberedSetSATB.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "WorkPacketsConcurrent.hpp"
#else
#include "WorkPacketsStandard.hpp"
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_MarkingScheme *
MM_MarkingScheme::newInstance(MM_EnvironmentBase *env)
{
	MM_MarkingScheme *markingScheme;

	markingScheme = (MM_MarkingScheme *)env->getForge()->allocate(sizeof(MM_MarkingScheme), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (markingScheme) {
		new(markingScheme) MM_MarkingScheme(env);
		if (!markingScheme->initialize(env)) {
			markingScheme->kill(env);
			markingScheme = NULL;
		}
	}

	return markingScheme;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_MarkingScheme::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the receivers internal structures and resources.
 * @return true if initialization is successful, false otherwise.
 */
bool
MM_MarkingScheme::initialize(MM_EnvironmentBase *env)
{
	_markMap = MM_MarkMap::newInstance(env, _extensions->heap->getMaximumPhysicalRange());

	if (!_markMap) {
		goto error_no_memory;
	}

	_workPackets = createWorkPackets(env);
	if (NULL == _workPackets) {
		goto error_no_memory;
	}

	return _delegate.initialize(env, this);

error_no_memory:
	return false;
}

/**
 * Free the receivers internal structures and resources.
 */
void
MM_MarkingScheme::tearDown(MM_EnvironmentBase *env)
{
	if (_markMap) {
		_markMap->kill(env);
		_markMap = NULL;
	}

	if(_workPackets) {
		_workPackets->kill(env);
		_workPackets = NULL;
	}
}

/**
 * Adjust internal structures to reflect the change in heap size.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @return true if operation completes with success
 */
bool
MM_MarkingScheme::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool result = true;
	/* Record the range in which valid objects appear */
	_heapBase = _extensions->heap->getHeapBase();
	_heapTop = _extensions->heap->getHeapTop();

	if (result) {
		result = _markMap->heapAddRange(env, size, lowAddress, highAddress);
	}
	return result;
}

/**
 * Adjust internal structures to reflect the change in heap size.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * @return true if operation completes with success
 */
bool
MM_MarkingScheme::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	bool result = true;
	/* Record the range in which valid objects appear */
	_heapBase = _extensions->heap->getHeapBase();
	_heapTop = _extensions->heap->getHeapTop();

	if (result) {
		_markMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return result;
}

/****************************************
 * Runtime initialization
 ****************************************
 */
void
MM_MarkingScheme::mainSetupForGC(MM_EnvironmentBase *env)
{
	/* Initialize the marking stack */
	_workPackets->reset(env);

	_delegate.mainSetupForGC(env);
}

/**
 * Set up the marking scheme for marking the heap in preparation for walking the heap.
 * This should be called in place of mainSetupForGC if no actual garbage collection
 * is to take place.
 *
 * @parm[in] env the current environment
 */
void
MM_MarkingScheme::mainSetupForWalk(MM_EnvironmentBase *env)
{
	/* Initialize the marking stack */
	_workPackets->reset(env);

	_delegate.mainSetupForWalk(env);
}

void
MM_MarkingScheme::mainCleanupAfterGC(MM_EnvironmentBase *env)
{
	_delegate.mainCleanupAfterGC(env);
}

void
MM_MarkingScheme::workerSetupForGC(MM_EnvironmentBase *env)
{
	env->_markStats.clear();
	env->_workPacketStats.clear();
	env->_workStack.reset(env, _workPackets);
	_delegate.workerSetupForGC(env);
}

void
MM_MarkingScheme::workerCleanupAfterGC(MM_EnvironmentBase *env)
{
	_delegate.workerCleanupAfterGC(env);
#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
	_extensions->globalGCStats.markStats.merge(&env->_markStats);
	_extensions->globalGCStats.workPacketStats.merge(&env->_workPacketStats);
#endif /* defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME) */
}


/****************************************
 * Marking Helpers
 ****************************************
 */

/**
 * External access to inlined method isMarked
 */
bool
MM_MarkingScheme::isMarkedOutline(omrobjectptr_t objectPtr)
{
	return isMarked(objectPtr);
}

bool
MM_MarkingScheme::markObjectNoCheck(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType)
{
	return inlineMarkObjectNoCheck(env, objectPtr, leafType);
}

bool
MM_MarkingScheme::markObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType)
{
	return inlineMarkObject(env, objectPtr, leafType);
}

/**************************************************************************
 * name        -  numMarkBitsInRange
 *
 * description -  Determine number of mark bits for specified heap range
 *
 *
 * parameters  - heapBase - base of region of heap whoose mark bits are to be
 * 						    counted
 * 			     heapTop  - top of region of heap whoose mark bits to be counted
 *
 * return      - the number of bytes worth of mark bits for specified heap
 * 				 range
 * *************************************************************************/
uintptr_t
MM_MarkingScheme::numMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop)
{
	return  _markMap->numberBitsInRange(env, heapBase, heapTop);
}

/**************************************************************************
 * name        -  setMarkBitsInRange
 *
 * description -  Set all mark bits for specified heap range either ON or OFF
 *
 *
 * parameters  - heapBase - base of region of heap whoose mark bits are to be set
 * 			     heapTop  - top of region of heap whoose mark bits to be set
 * 				 clear 	  - if TRUE set BITS OFF; otherwise set bits ON
 *
 * return      - the amount of memory actually cleard
 * *************************************************************************/
uintptr_t
MM_MarkingScheme::setMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop, bool clear)
{
	return _markMap->setBitsInRange(env, heapBase, heapTop, clear);
}

/****************************************
 * Scanning
 ****************************************
 */
/**
 * Private internal. Called exclusively from completeScan();
 */
uintptr_t
MM_MarkingScheme::scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	uintptr_t sizeToDo = UDATA_MAX;
	GC_ObjectScannerState objectScannerState;
	GC_ObjectScanner *objectScanner = _delegate.getObjectScanner(env, objectPtr, &objectScannerState, SCAN_REASON_PACKET, &sizeToDo);
	if (NULL != objectScanner) {
		bool isLeafSlot = false;
		GC_SlotObject *slotObject;
#if defined(OMR_GC_LEAF_BITS)
		while (NULL != (slotObject = objectScanner->getNextSlot(&isLeafSlot))) {
#else /* OMR_GC_LEAF_BITS */
		while (NULL != (slotObject = objectScanner->getNextSlot())) {
#endif /* OMR_GC_LEAF_BITS */
			fixupForwardedSlot(slotObject);

			inlineMarkObjectNoCheck(env, slotObject->readReferenceFromSlot(), isLeafSlot);
		}
	}
	return sizeToDo;
}


/**
 * Scan until there are no more work packets to be processed.
 * @note This is a joining scan: a thread will not exit this method until
 * all threads have entered and all work packets are empty.
 */
void
MM_MarkingScheme::completeScan(MM_EnvironmentBase *env)
{
	do {
		omrobjectptr_t objectPtr = NULL;
		while (NULL != (objectPtr = (omrobjectptr_t )env->_workStack.pop(env))) {
			env->_markStats._bytesScanned += scanObject(env, objectPtr);
			env->_markStats._objectsScanned += 1;
		}
	} while (_workPackets->handleWorkPacketOverflow(env));
}

/****************************************
 * Marking Core Functionality
 ****************************************/

/**
 *  Initialization for Mark
 *  Actual startup for Mark procedure
 *  @param[in] env - passed Environment 
 *  @param[in] initMarkMap instruct should mark map be initialized too
 */
void
MM_MarkingScheme::markLiveObjectsInit(MM_EnvironmentBase *env, bool initMarkMap)
{
	workerSetupForGC(env);

	if(initMarkMap) {
		_markMap->initializeMarkMap(env);
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
	}
}

/**
 *  Mark Roots
 *  Create Root Scanner and Mark all roots including classes and classloaders if dynamic class unloading is enabled
 *  @param[in] env - passed Environment 
 *  @param[in] shouldScan instruct should scanning also be active while roots are marked   
 */
void
MM_MarkingScheme::markLiveObjectsRoots(MM_EnvironmentBase *env, bool processLists)
{
	_delegate.scanRoots(env, processLists);
}

void
MM_MarkingScheme::markLiveObjectsScan(MM_EnvironmentBase *env)
{
	completeMarking(env);
}

void
MM_MarkingScheme::completeMarking(MM_EnvironmentBase *env)
{
	completeScan(env);

	_delegate.completeMarking(env);
}

/**
 *  Final Mark services including scanning of Clearables
 *  @param[in] env - passed Environment 
 */
void
MM_MarkingScheme::markLiveObjectsComplete(MM_EnvironmentBase *env)
{
	_delegate.workerCompleteGC(env);
}

MM_WorkPackets *
MM_MarkingScheme::createWorkPackets(MM_EnvironmentBase *env)
{
	MM_WorkPackets *workPackets = NULL;
	if (_extensions->isConcurrentMarkEnabled()) {
		if (_extensions->configuration->isSnapshotAtTheBeginningBarrierEnabled()) {
#if defined(OMR_GC_REALTIME)
			MM_WorkPacketsSATB *workPacketsSATB = MM_WorkPacketsSATB::newInstance(env);
			_extensions->sATBBarrierRememberedSet = MM_RememberedSetSATB::newInstance(env, workPacketsSATB);
			workPackets = workPacketsSATB;
#endif /* defined(OMR_GC_REALTIME) */
		} else {
#if defined OMR_GC_MODRON_CONCURRENT_MARK
			workPackets = MM_WorkPacketsConcurrent::newInstance(env);
#endif /* defined OMR_GC_MODRON_CONCURRENT_MARK */
		}
	} else {
		workPackets = MM_WorkPacketsStandard::newInstance(env);
	}

	return workPackets;
}

bool
MM_MarkingScheme::fixupForwardedSlot(omrobjectptr_t *slotPtr) {
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	bool const compressed = _extensions->compressObjectReferences();
	if (_extensions->getGlobalCollector()->isStwCollectionInProgress()) {
		MM_ForwardedHeader forwardHeader(*slotPtr, compressed);
		omrobjectptr_t forwardPtr = forwardHeader.getNonStrictForwardedObject();

		if (NULL != forwardPtr) {
			if (forwardHeader.isSelfForwardedPointer()) {
				forwardHeader.restoreSelfForwardedPointer();
			} else {
				*slotPtr = forwardPtr;
				return true;
			}
		}
	}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	return false;
}

uintptr_t
MM_MarkingScheme::setupIndexableScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason, uintptr_t *sizeToDo, uintptr_t *sizeInElementsToDo, fomrobject_t **basePtr, uintptr_t *flags)
{
	return _delegate.setupIndexableScanner(env, objectPtr, reason, sizeToDo, sizeInElementsToDo, basePtr, flags);
}

void
MM_MarkingScheme::markObjectsForRange(MM_EnvironmentBase *env, uint8_t *objPtrLow, uint8_t *objPtrHigh)
{
	Assert_MM_true(NULL != objPtrLow);
	Assert_MM_true(NULL != objPtrHigh);

	Assert_MM_true(objPtrHigh >= objPtrLow);

	uintptr_t slotIndexLow = 0;
	uintptr_t slotIndexHigh = 0;
	uintptr_t bitMaskLow = 0;
	uintptr_t bitMaskHigh = 0;

	/* high bit block mask for low slot word - pass in false for lowBlock */
	_markMap->getSlotIndexAndBlockMask((omrobjectptr_t) objPtrLow, &slotIndexLow, &bitMaskLow, false);

	 /* low bit block mask for high slot word - pass in true for lowBlock */
	_markMap->getSlotIndexAndBlockMask((omrobjectptr_t) objPtrHigh, &slotIndexHigh, &bitMaskHigh, true);

	if (slotIndexLow == slotIndexHigh) {
		_markMap->markBlockAtomic(slotIndexLow, bitMaskLow & bitMaskHigh);
	} else {
		_markMap->markBlockAtomic(slotIndexLow, bitMaskLow);
		_markMap->setMarkBlock(slotIndexLow + 1, slotIndexHigh - 1, (uintptr_t)-1);
		_markMap->markBlockAtomic(slotIndexHigh, bitMaskHigh);
	}
}

