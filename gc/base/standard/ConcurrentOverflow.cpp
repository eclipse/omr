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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include "modronopt.h"

#include "CardCleanerForMarking.hpp"
#include "ConcurrentCardTable.hpp"
#include "ConcurrentGC.hpp"
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "Packet.hpp"
#include "ParallelTask.hpp"

#include "WorkPackets.hpp"
#include "ConcurrentOverflow.hpp"

/**
 * Create a new MM_ConcurrentOverflow object
 */
MM_ConcurrentOverflow *
MM_ConcurrentOverflow::newInstance(MM_EnvironmentBase *env,MM_WorkPackets *workPackets)
{
	MM_ConcurrentOverflow *overflow;

	overflow = (MM_ConcurrentOverflow *)env->getForge()->allocate(sizeof(MM_ConcurrentOverflow), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != overflow) {
		new(overflow) MM_ConcurrentOverflow(env,workPackets);
		if (!overflow->initialize(env)) {
			overflow->kill(env);
			overflow = NULL;
		}
	}
	return overflow;
}

/**
 * Initialize a MM_ConcurrentOverflow object.
 * 
 * @return true on success, false otherwise
 */
bool
MM_ConcurrentOverflow::initialize(MM_EnvironmentBase *env)
{
	bool result = MM_WorkPacketOverflow::initialize(env);

	if (result) {
		/* Initialize monitor for safe initial Cards cleaning in Work Packets Overflow handler */
		if(omrthread_monitor_init_with_name(&_cardsClearingMonitor, 0, "MM_ConcurrentOverflow::cardsClearingMonitor")) {
			result = false;
		}
	}

	return result;
}

/**
 * Cleanup the resources for a MM_ConcurrentOverflow object
 */
void
MM_ConcurrentOverflow::tearDown(MM_EnvironmentBase *env)
{
	if(NULL != _cardsClearingMonitor) {
		omrthread_monitor_destroy(_cardsClearingMonitor);
		_cardsClearingMonitor = NULL;
	}

	MM_WorkPacketOverflow::tearDown(env);
}

/**
 * Empty a packet on overflow
 * 
 * Empty a packet to resolve overflow by dirtying the appropriate 
 * cards for each object withing a given packet
 * 
 * @param packet - Reference to packet to be empited
 * @param type - ignored for concurrent collector
 *  
 */
void
MM_ConcurrentOverflow::emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type)
{
	MM_ConcurrentGC *collector = (MM_ConcurrentGC *)_extensions->getGlobalCollector();
	void *objectPtr;

	_overflow = true;

	/* Broadcast the overflow to the concurrent collector
	 * so it can take any remedial action */
	collector->concurrentWorkStackOverflow();

	_extensions->globalGCStats.workPacketStats.setSTWWorkStackOverflowOccured(true);
	_extensions->globalGCStats.workPacketStats.incrementSTWWorkStackOverflowCount();
	_extensions->globalGCStats.workPacketStats.setSTWWorkpacketCountAtOverflow(_workPackets->getActivePacketCount());

#if defined(OMR_GC_MODRON_SCAVENGER)
	clearCardsForNewSpace(MM_EnvironmentStandard::getEnvironment(env), collector);
#endif /*  OMR_GC_MODRON_SCAVENGER */
	
	/* Empty the current packet by dirtying its cards now */
	while(NULL != (objectPtr = packet->pop(env))) {
		overflowItemInternal(env, objectPtr, collector->getCardTable());
	}
	
	Assert_MM_true(packet->isEmpty());
}

/**
 * Overflow an item
 * 
 * Overflow an item by dirtying the appropriate card
 * 
 * @param item - item to overflow
 * @param type - ignored for concurrent collector
 *  
 */
void
MM_ConcurrentOverflow::overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	MM_ConcurrentGC *collector = (MM_ConcurrentGC *)_extensions->getGlobalCollector();
	
	_overflow = true;

	/* Broadcast the overflow to the concurrent collector
	 * so it can take any remedial action 
	 */
	collector->concurrentWorkStackOverflow();

	_extensions->globalGCStats.workPacketStats.setSTWWorkStackOverflowOccured(true);
	_extensions->globalGCStats.workPacketStats.incrementSTWWorkStackOverflowCount();
	_extensions->globalGCStats.workPacketStats.setSTWWorkpacketCountAtOverflow(_workPackets->getActivePacketCount());

#if defined(OMR_GC_MODRON_SCAVENGER)
	clearCardsForNewSpace(MM_EnvironmentStandard::getEnvironment(env), collector);
#endif /*  OMR_GC_MODRON_SCAVENGER */

	overflowItemInternal(env, item, collector->getCardTable());
}

void
MM_ConcurrentOverflow::overflowItemInternal(MM_EnvironmentBase *env, void *item, MM_ConcurrentCardTable *cardTable)
{
	MM_EnvironmentStandard *envStandard = MM_EnvironmentStandard::getEnvironment(env);
	void *heapBase = _extensions->heap->getHeapBase();
	void *heapTop = _extensions->heap->getHeapTop();

	/* Check reference is within the heap */
	if ((PACKET_ARRAY_SPLIT_TAG != ((UDATA)item & PACKET_ARRAY_SPLIT_TAG)) && (item >= heapBase) && (item <  heapTop)) {
	/* ..and dirty its card if it is */
		omrobjectptr_t objectPtr = (omrobjectptr_t)item;
		cardTable->dirtyCard(envStandard, objectPtr);
		MM_ParallelGlobalGC *globalCollector = (MM_ParallelGlobalGC *)_extensions->getGlobalCollector();
		globalCollector->getMarkingScheme()->getMarkingDelegate()->handleWorkPacketOverflowItem(env,objectPtr);
	}
}

void
MM_ConcurrentOverflow::fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet)
{
	Assert_MM_unreachable();
}

void
MM_ConcurrentOverflow::reset(MM_EnvironmentBase *env)
{

}

bool
MM_ConcurrentOverflow::isEmpty()
{
	return true;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_ConcurrentOverflow::clearCardsForNewSpace(MM_EnvironmentStandard *env, MM_ConcurrentGC *collector)
{
	/* If scavenger is enabled, we are within a global collect, and we have not already done so,
	 * then we need to clear cards for new space so we can resolve overflow by dirtying cards
	*/
    if(_extensions->scavengerEnabled && collector->isStwCollectionInProgress()) {
    	/*
    	 *	Should be the only one thread cleaning cards for NEW space
    	 *	If any other thread handling an WP Overflow came here while cleaning is in progress it should wait
    	 */
    	omrthread_monitor_enter(_cardsClearingMonitor);

		if(!_cardsForNewSpaceCleared) {
			MM_EnvironmentStandard *envStandard = MM_EnvironmentStandard::getEnvironment(env);
			MM_ConcurrentCardTable *cardTable = collector->getCardTable();
	    	cardTable->clearNonConcurrentCards(envStandard);
	    	_cardsForNewSpaceCleared = true;
	    }

		omrthread_monitor_exit(_cardsClearingMonitor);
	}
}
#endif /*  OMR_GC_MODRON_SCAVENGER */

void
MM_ConcurrentOverflow::handleOverflow(MM_EnvironmentBase *env)
{
	MM_EnvironmentStandard *envStandard = MM_EnvironmentStandard::getEnvironment(env);
	if (envStandard->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		_overflow = false;
		envStandard->_currentTask->releaseSynchronizedGCThreads(envStandard);
	}

	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;
	MM_ConcurrentGC *collector = (MM_ConcurrentGC *)_extensions->getGlobalCollector();
	MM_CardCleanerForMarking cardCleanerForMarking(collector->getMarkingScheme());
	MM_ConcurrentCardTable *cardTable = collector->getCardTable();

	while((region = regionIterator.nextRegion()) != NULL) {
		cardTable->cleanCardTableForRange(envStandard, &cardCleanerForMarking, region->getLowAddress(), region->getHighAddress());
	}

	envStandard->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
