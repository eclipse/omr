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
#include "omr.h"

#include "CollectorLanguageInterface.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "MarkMap.hpp"
#include "ObjectHeapIteratorSegregated.hpp"
#include "Packet.hpp"
#include "SegregatedGC.hpp"
#include "SegregatedMarkingScheme.hpp"
#include "Task.hpp"
#include "WorkPackets.hpp"

#include "OverflowSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

#define SCAN_MAX (uintptr_t)(-1)

/**
 * Create a new MM_OverflowSegregated object
 */
MM_OverflowSegregated *
MM_OverflowSegregated::newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
{
	Assert_MM_true(env->getExtensions()->isSegregatedHeap());
	MM_OverflowSegregated *overflow = (MM_OverflowSegregated *)env->getForge()->allocate(sizeof(MM_OverflowSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != overflow) {
		new(overflow) MM_OverflowSegregated(env,workPackets);
		if (!overflow->initialize(env)) {
			overflow->kill(env);
			overflow = NULL;
		}
	}
	return overflow;
}

bool
MM_OverflowSegregated::initialize(MM_EnvironmentBase *env)
{
	return MM_WorkPacketOverflow::initialize(env);
}

void
MM_OverflowSegregated::tearDown(MM_EnvironmentBase *env)
{
	MM_WorkPacketOverflow::tearDown(env);
}

void
MM_OverflowSegregated::emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type)
{
	void *objectPtr;

	_overflow = true;

	_extensions->globalGCStats.workPacketStats.setSTWWorkStackOverflowOccured(true);
	_extensions->globalGCStats.workPacketStats.incrementSTWWorkStackOverflowCount();
	_extensions->globalGCStats.workPacketStats.setSTWWorkpacketCountAtOverflow(_workPackets->getActivePacketCount());

	/* Empty the current packet by dirtying its cards now */
	while(NULL != (objectPtr = packet->pop(env))) {
		overflowItemInternal(env, objectPtr);
	}

	Assert_MM_true(packet->isEmpty());
}

void
MM_OverflowSegregated::overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	_overflow = true;

	_extensions->globalGCStats.workPacketStats.setSTWWorkStackOverflowOccured(true);
	_extensions->globalGCStats.workPacketStats.incrementSTWWorkStackOverflowCount();
	_extensions->globalGCStats.workPacketStats.setSTWWorkpacketCountAtOverflow(_workPackets->getActivePacketCount());

	overflowItemInternal(env, item);
}

void
MM_OverflowSegregated::overflowItemInternal(MM_EnvironmentBase *env, void *item)
{
	void *heapBase = _extensions->heap->getHeapBase();
	void *heapTop = _extensions->heap->getHeapTop();

	if ((PACKET_ARRAY_SPLIT_TAG != ((uintptr_t)item & PACKET_ARRAY_SPLIT_TAG)) &&  (item >= heapBase) && (item < heapTop)) {
		MM_SegregatedGC *globalCollector = (MM_SegregatedGC *)_extensions->getGlobalCollector();
		MM_SegregatedMarkingScheme *markingScheme = globalCollector->getMarkingScheme();
		MM_MarkMap *markMap = markingScheme->getMarkMap();
		omrobjectptr_t objectPtr = (omrobjectptr_t)item;

		/* object has to be marked already */
		Assert_MM_true(markMap->isBitSet(objectPtr));
		Assert_MM_false(markMap->isBitSet((omrobjectptr_t)((uintptr_t)item + markMap->getObjectGrain())));

		/* set overflow bit (double marking) */
		markMap->atomicSetBit((omrobjectptr_t)((uintptr_t)item + markMap->getObjectGrain()));

		/* Perform language specific actions */
		_extensions->collectorLanguageInterface->workPacketOverflow_overflowItem(env,objectPtr);
	}
}

void
MM_OverflowSegregated::fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet)
{
	Assert_MM_unreachable();
}

void
MM_OverflowSegregated::handleOverflow(MM_EnvironmentBase *env)
{
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		_overflow = false;

		MM_Heap *heap = _extensions->heap;
		MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
		GC_HeapRegionIterator regionIterator(regionManager);
		MM_HeapRegionDescriptorSegregated *region = NULL;
		MM_SegregatedGC *globalCollector = (MM_SegregatedGC *)_extensions->getGlobalCollector();
		MM_SegregatedMarkingScheme *markingScheme = globalCollector->getMarkingScheme();
		MM_MarkMap *markMap = markingScheme->getMarkMap();

		while((region = (MM_HeapRegionDescriptorSegregated *)regionIterator.nextRegion()) != NULL) {
			GC_ObjectHeapIteratorSegregated objectIterator(_extensions, (omrobjectptr_t)region->getLowAddress(), (omrobjectptr_t)region->getHighAddress(), region->getRegionType(), region->getCellSize(), false, false);
			omrobjectptr_t object;

			while((object = objectIterator.nextObject()) != NULL) {
				/* search for double marked (overflowed) objects */
				if ((markMap->isBitSet(object)) && (markMap->isBitSet((omrobjectptr_t)((uintptr_t)object + markMap->getObjectGrain())))) {
					/* clean overflow mark */
					markMap->clearBit((omrobjectptr_t)((uintptr_t)object + markMap->getObjectGrain()));

					/* scan overflowed object */
					/* TODO Fix this as it is wrong for metronome GC policy */
					/* The subclassing of markingscheme by segregatedmarking scheme needs a lot of work and means metronome is pretty broken */
					_extensions->collectorLanguageInterface->markingScheme_scanObject(env, object, MM_CollectorLanguageInterface::SCAN_REASON_OVERFLOWED_OBJECT);
				}
			}
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

void
MM_OverflowSegregated::reset(MM_EnvironmentBase *env)
{

}

bool
MM_OverflowSegregated::isEmpty()
{
	return true;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
