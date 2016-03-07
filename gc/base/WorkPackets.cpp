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

#include "omr.h"
#include "omrport.h"
#include "omrthread.h"

#include <string.h>

#include "AtomicOperations.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "Packet.hpp"
#include "PacketList.hpp"
#include "Task.hpp"
#include "WorkPackets.hpp"
#include "WorkPacketOverflow.hpp"

/**
 * Instantiate a MM_WorkPackets
 * @param mode type of packets (used for getting the right overflow handler)
 * @return pointer to the new object
 */
MM_WorkPackets *
MM_WorkPackets::newInstance(MM_EnvironmentBase *env)
{
	MM_WorkPackets *workPackets;
	
	workPackets = (MM_WorkPackets *)env->getForge()->allocate(sizeof(MM_WorkPackets), MM_AllocationCategory::WORK_PACKETS, OMR_GET_CALLSITE());
	if (workPackets) {
		new(workPackets) MM_WorkPackets(env);
		if (!workPackets->initialize(env)) {
			workPackets->kill(env);
			workPackets = NULL;	
		}
	}
	
	return workPackets;
}

/**
 * Kill a MM_WorkPackets object
 */
void
MM_WorkPackets::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize a MM_WorkPackets objects
 * @return true on success, false otherwise
 */
bool
MM_WorkPackets::initialize(MM_EnvironmentBase *env)
{
	uintptr_t initialPacketCount, heapSize;

	/* Initialize these here instead of the constructor initializer list to help us break
	 * dependancy cycle, and allow inlining of functions in MM_WorkStack.
	 * (Dependancy Cycle between WorkPackets.hpp, WorkStack.hpp, and Environment.hpp)
	 */
	_extensions = env->getExtensions();
	_portLibrary = env->getPortLibrary();

	heapSize = _extensions->heap->getMaximumMemorySize();

	if (!_emptyPacketList.initialize(env)) {
		return false;
	}
	if (!_fullPacketList.initialize(env)) {
		return false;
	}
	if (!_nonEmptyPacketList.initialize(env)) {
		return false;
	}
	if (!_relativelyFullPacketList.initialize(env)) {
		return false;
	}
	if (!_deferredPacketList.initialize(env)) {
		return false;
	}
	
	if (!_deferredFullPacketList.initialize(env)) {
		return false;
	}

	if(omrthread_monitor_init_with_name(&_inputListMonitor, 0, "MM_WorkPackets::inputList")) {
		return false;
	}

	if(omrthread_monitor_init_with_name(&_allocatingPackets, 0, "MM_WorkPackets::allocatingPackets")) {
		return false;
	}
	
	_overflowHandler = createOverflowHandler(env, this);
	if (NULL == _overflowHandler) {
		return false;
	}

	if(0 != _extensions->workpacketCount) {
		/* -Xgcworkpackets was specified, so base the number on that */
		initialPacketCount = _extensions->workpacketCount;
	} else {
		/* determine the number of initial active packets heuristically, based on heapsize */
		initialPacketCount = (uintptr_t)((float) heapSize * getHeapCapacityFactor(env) / _packetSize);
	}
	
	/* round the initialPacketCount so there is the same number of packets in each packetblock, and we have a sane minimum */
	initialPacketCount = MM_Math::roundToFloor(_initialBlocks, initialPacketCount);
	initialPacketCount = OMR_MAX(initialPacketCount, _initialBlocks * _minPacketsInBlock);
	
	/* Want to ensure we have enough packets so that every thread can make forward progress at once.
	 * This means that we need to have at least 2 packets per thread (one input and one output.)
	 * See CMVC 194431 for more details.
	 */
	uintptr_t minimumPacketsForForwardProgress = MM_Math::roundToCeiling(_initialBlocks, _extensions->gcThreadCount * 2);
	initialPacketCount = OMR_MAX(initialPacketCount, minimumPacketsForForwardProgress);

	_packetsPerBlock = initialPacketCount / _initialBlocks;

	/* If -Xgcworkpackets was specified  we don't allow later allocation of more packets */
	_maxPackets = (0 != _extensions->workpacketCount) ? initialPacketCount : initialPacketCount * _increaseFactor;
	
	/* NULL out the packetsBlocks array to begin with */
	for(uintptr_t i = 0; i < _maxPacketsBlocks; i++) {    
		_packetsStart[i] = NULL;
	}
	
	/* now allocate the initial active packets */
	while(initialPacketCount > _activePackets) {
		if(!initWorkPacketsBlock(env)) {
			return false;
		}
	}
	
	return true;
}

/**
 * Allocate another workpacket block
 * @return true on sucess, false on allocation failure or if _maxpackets is already reached
 */
bool
MM_WorkPackets::initWorkPacketsBlock(MM_EnvironmentBase *env)
{
	intptr_t blockSize = (sizeof(MM_Packet) + _packetSize) * _packetsPerBlock;
	uintptr_t totalHeaderSize = sizeof(MM_Packet) * _packetsPerBlock;

	
	if (_activePackets >= _maxPackets) {
		return false;
	}

	Assert_MM_true(_packetsBlocksTop < _maxPacketsBlocks);
	/* Build the output packet list */
	if (NULL == (_packetsStart[_packetsBlocksTop] = (MM_Packet *) env->getForge()->allocate(blockSize, MM_AllocationCategory::WORK_PACKETS, OMR_GET_CALLSITE()))) {
		return false;
	}
	memset((void *)_packetsStart[_packetsBlocksTop], 0, totalHeaderSize); /* initialize only MM_Packet's to avoid paging the rest of the block */

	MM_Packet *headPtr = _packetsStart[_packetsBlocksTop];
	MM_Packet *currentPtr = headPtr;
	MM_Packet *tailPtr = (MM_Packet *)(((uintptr_t)currentPtr ) + totalHeaderSize - sizeof(MM_Packet));
	MM_Packet *previousPtr = NULL;
	MM_Packet *nextPtr = currentPtr + 1;

	uintptr_t dataStart = ((uintptr_t) headPtr) + totalHeaderSize;
	uintptr_t dataSize = _slotsInPacket * sizeof(uintptr_t);
	uintptr_t *baseAddress = NULL;

	for(uintptr_t i = 0; i < _packetsPerBlock; i++) {
		baseAddress = (uintptr_t *) (dataStart + (i * dataSize));
		currentPtr->initialize(env, nextPtr, previousPtr, baseAddress, _slotsInPacket);

		previousPtr = currentPtr;
		currentPtr += 1;
		nextPtr = currentPtr + 1;

		if (currentPtr == tailPtr) {
			nextPtr = NULL;
		}
	}

	
	_emptyPacketList.pushList(headPtr, tailPtr, _packetsPerBlock);

	_packetsBlocksTop++;
	_activePackets += _packetsPerBlock;

	return true;
}

/**
 * Destroy the resources a MM_WorkPackets is responsible for
 */
void
MM_WorkPackets::tearDown(MM_EnvironmentBase *env)
{	
	if (NULL != _overflowHandler) {
		_overflowHandler->kill(env);
		_overflowHandler = NULL;
	}

	for(uintptr_t i = 0; i < _packetsBlocksTop; i++) {
		if(NULL != _packetsStart[i]) {
			env->getForge()->free(_packetsStart[i]);
			_packetsStart[i] = NULL;
		}
	}

	if (NULL != _inputListMonitor) {
		omrthread_monitor_destroy(_inputListMonitor);
		_inputListMonitor = NULL;
	}

	if (NULL != _allocatingPackets) {
		omrthread_monitor_destroy(_allocatingPackets);
		_allocatingPackets = NULL;
	}

	_emptyPacketList.tearDown(env);
	_fullPacketList.tearDown(env);
	_nonEmptyPacketList.tearDown(env);
	_relativelyFullPacketList.tearDown(env);
	_deferredPacketList.tearDown(env);
	_deferredFullPacketList.tearDown(env);
}

void
MM_WorkPackets::reset(MM_EnvironmentBase *env)
{
	_overflowHandler->reset(env);
}

/**
 * Iterate over all non empty lists and reset all packets and return them to the empty list.
 */
void
MM_WorkPackets::resetAllPackets(MM_EnvironmentBase *env)
{	
	MM_Packet *packet;
	
	while(NULL != (packet = getPacket(env, &_fullPacketList))) {
		packet->resetData(env);
		putPacket(env, packet);
	}
	while(NULL != (packet = getPacket(env, &_relativelyFullPacketList))) {
		packet->resetData(env);
		putPacket(env, packet);
	}
	while(NULL != (packet = getPacket(env, &_nonEmptyPacketList))) {
		packet->resetData(env);
		putPacket(env, packet);
	}

	while(NULL != (packet = getPacket(env, &_deferredPacketList))) {
		packet->resetData(env); 
		putPacket(env, packet);
	}

	while(NULL != (packet = getPacket(env, &_deferredFullPacketList))) {
		packet->resetData(env);
		putPacket(env, packet);
	}

	/* Do sanity check on ctrs */	
	assume0(_deferredFullPacketList.getCount() == 0);
	assume0(_deferredPacketList.getCount() == 0);
	assume0(_emptyPacketList.getCount() == _activePackets);

	clearOverflowFlag();
}

/**
 * Move all deferred workpackets to the regular lists 
 */
void
MM_WorkPackets::reuseDeferredPackets(MM_EnvironmentBase *env)
{
	MM_Packet *packet;

	if(_deferredPacketList.isEmpty() && _deferredFullPacketList.isEmpty()) {
		/* Both deferred lists are empty, so there is nothing to do */
		return;
	}

	/* Un-defer all partially full packets */
	if(!_deferredPacketList.isEmpty()) {
		while(NULL != (packet = getPacket(env, &_deferredPacketList))) {
			putPacket(env, packet);
		}
	}
	
	/* Un-defer all totally full packets */
	if(!_deferredFullPacketList.isEmpty()) {
		while(NULL != (packet = getPacket(env, &_deferredFullPacketList))) {
			putPacket(env, packet);
		}
	}
}

/**
 * Determine whether an input packet is available
 * @return true if yes, false if no
 */
bool
MM_WorkPackets::inputPacketAvailable(MM_EnvironmentBase *env)
{
	bool res = 	((!_fullPacketList.isEmpty())
				|| (!_relativelyFullPacketList.isEmpty())
				|| (!_nonEmptyPacketList.isEmpty())
				|| (!_overflowHandler->isEmpty()));
				
	return res;
}

/**
 * Transfer a packet to the current overflow handler to be emptied to
 * resolve work packet overflow. 
 * 
 * @param packet - Reference to packet to be emptied 
 */
void
MM_WorkPackets::emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type)
{	
	_overflowHandler->emptyToOverflow(env, packet, type);
}

/**
 * Get an input packet from the current overflow handler
 * 
 * @return a packet if one is found, NULL otherwise
 */
MM_Packet *
MM_WorkPackets::getInputPacketFromOverflow(MM_EnvironmentBase *env)
{
	MM_Packet *overflowPacket;

	while(!_overflowHandler->isEmpty()) {
		if(NULL != (overflowPacket = getPacket(env, &_emptyPacketList))) {
			
			_overflowHandler->fillFromOverflow(env, overflowPacket);
			
			if(overflowPacket->isEmpty()) {
				/* If we didn't end up filling the packet with anything, don't return it and try again */
				putPacket(env, overflowPacket);
			} else {	
				return overflowPacket;
			}
		}
	}

	return NULL;
}

/**
 * Get an input packet if one is available
 * 
 * @return pointer to a packet, or NULL if none available
 */
MM_Packet *
MM_WorkPackets::getInputPacketNoWait(MM_EnvironmentBase *env)
{
	MM_Packet *packet;

	if (!inputPacketAvailable(env)) {
		return NULL;
	}

	if((!_nonEmptyPacketList.isEmpty()) && (_emptyPacketList.getCount() < (_activePackets >> 2))) {
		if(NULL == (packet = getPacket(env, &_nonEmptyPacketList))) {
			if(NULL == (packet = getPacket(env, &_relativelyFullPacketList))) {
				packet = getPacket(env, &_fullPacketList);
			}
		}
	} else {
		if(NULL == (packet = getPacket(env, &_fullPacketList))) {
			if(NULL == (packet = getPacket(env, &_relativelyFullPacketList)))  {
				packet = getPacket(env, &_nonEmptyPacketList);
			}
		}
	}

	if(NULL == packet) {
		packet = getInputPacketFromOverflow(env);
	}

	if(NULL != packet) {
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		env->_workPacketStats.workPacketsAcquired += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
		if((_inputListWaitCount > 0) && inputPacketAvailable(env)) {
			notifyWaitingThreads(env);
		}
	}

	return packet;
}

/**
 * Get an input packet
 * 
 * @return Pointer to an input packet
 */
MM_Packet *
MM_WorkPackets::getInputPacket(MM_EnvironmentBase *env)
{
	MM_Packet *packet = NULL;
	bool doneFlag = false;
	volatile uintptr_t doneIndex = _inputListDoneIndex;
	bool mustSyncThreadsAndExit = (NULL != env->_currentTask) && env->_currentTask->shouldYieldFromTask(env);
	
	while(!doneFlag) {
		if (!mustSyncThreadsAndExit) {
			while (inputPacketAvailable(env)) {
				/* Check if the regular cache list has work to be done */
				if(NULL != (packet = getInputPacketNoWait(env))) {
					return packet;
				}
			}
		}

		omrthread_monitor_enter(_inputListMonitor);

		if(doneIndex == _inputListDoneIndex) {
			_inputListWaitCount += 1;
			
			if(((NULL == env->_currentTask) || (_inputListWaitCount == env->_currentTask->getThreadCount())) 
					&& (mustSyncThreadsAndExit || !inputPacketAvailable(env))) {
				_inputListDoneIndex += 1;
				_inputListWaitCount = 0;
				omrthread_monitor_notify_all(_inputListMonitor);
			} else {
				while(mustSyncThreadsAndExit || (!inputPacketAvailable(env) && (_inputListDoneIndex == doneIndex))) {
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
					uint64_t waitStartTime, waitEndTime;
					OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
					waitStartTime = omrtime_hires_clock();
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
					
					/* This is where all the GC threads end up synchronizing when waiting for work */
					omrthread_monitor_wait(_inputListMonitor);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
					waitEndTime = omrtime_hires_clock();

					if (_inputListDoneIndex != doneIndex) {
						env->_workPacketStats.addToCompleteStallTime(waitStartTime, waitEndTime);
					} else {
						env->_workPacketStats.addToWorkStallTime(waitStartTime, waitEndTime);
					}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

#if defined(OMR_GC_VLHGC)
					if ((NULL != env->_currentTask) && env->_currentTask->shouldYieldFromTask(env)) {
						omrthread_monitor_exit(_inputListMonitor);
						return NULL;
					}
#endif /* OMR_GC_VLHGC */
				}
			}
		}

		/* Set the local done flag. If we are done ,exit
		 *  if we are not yet done only decrement the wait count */
		doneFlag = (_inputListDoneIndex != doneIndex);
		if(!doneFlag) {
			_inputListWaitCount -= 1;
		}
		omrthread_monitor_exit(_inputListMonitor);
	}

	assume0(NULL == packet);
	return packet;
}

/**
 * Get an output packet
 * 
 * @return pointer to an output packet
 */
MM_Packet *
MM_WorkPackets::getOutputPacket(MM_EnvironmentBase *env)
{
	MM_Packet *outputPacket = NULL;

	/* Check the free list */
	outputPacket = getPacket(env, &_emptyPacketList);
	if(NULL != outputPacket) {
		return outputPacket;
	}

	/* emptyPacketList was empty so attempt to get a packet from one of the list of partial full packets */
	outputPacket = getLeastFullPacket(env, 2);
	if(NULL != outputPacket) {
		return outputPacket;
	}
	
	outputPacket = getPacketByAdddingWorkPacketBlock(env);
	if (NULL != outputPacket) {
		return outputPacket;
	}
	
	/* emptyPacketList was empty after attempt to grow.  Try the partially full lists */
	outputPacket = getLeastFullPacket(env, 2);
	if(NULL != outputPacket) {
		return outputPacket;
	}
	
	/* Adding a block of packets failed so move on to overflow processing */
	return getPacketByOverflowing(env);
}

/**
 * Get a packet by initing another block of workPackets.
 * 
 * @return pointer to a packet, or NULL if there are no empty packets available
 */
MM_Packet *
MM_WorkPackets::getPacketByAdddingWorkPacketBlock(MM_EnvironmentBase *env)
{
	MM_Packet *packet = NULL;

	/* Take control of the allocatingPackets monitor */
	omrthread_monitor_enter(_allocatingPackets);
	
	/* Check to see if there are any packets on the empty packetlist since another thread may have allocated
	 * another WorkPacket block while this thread was attempting to take control of the allocatingPackets field
	 */
	packet = getPacket(env, &_emptyPacketList);
	if(NULL == packet) {
		/* Since the current thread now has control of the allocatingPackets field and there are no packets
		 * on the emptyPacketList attempt to initialize another workPackets block
		 */
		if(initWorkPacketsBlock(env)) {
			/* Successfully initialized another workpacket block so get a packet off of the emptyPacketList */
			packet = getPacket(env, &_emptyPacketList);
		}
	}

	/* Relinquish control of the allocatingPackets field */
	omrthread_monitor_exit(_allocatingPackets);

	return packet;
}

/**
 * Get a packet by emptying a full packet
 * 
 * @return pointer to a packet
 */
MM_Packet *
MM_WorkPackets::getPacketByOverflowing(MM_EnvironmentBase *env)
{
	MM_Packet *packet = NULL;
	
	packet = getPacket(env, &_fullPacketList);
	if(NULL != packet) {
		/* Move the contents of the packet to overflow */
		emptyToOverflow(env, packet, OVERFLOW_TYPE_WORKSTACK);
		
		omrthread_monitor_enter(_inputListMonitor);

		/* Overflow was created - alert other threads that are waiting */
		if(_inputListWaitCount > 0) {
			omrthread_monitor_notify(_inputListMonitor);
		}
		
		omrthread_monitor_exit(_inputListMonitor);
	} else {
		packet = getPacket(env, &_emptyPacketList);
		if(NULL == packet) {
			packet = getLeastFullPacket(env, 2);
		}
	}
	
	return packet;
}

/**
 * Get the least-full packet that has capacity of at least the given number of slots
 * 
 * @param requiredSlots The required number of free slots
 * @return pointer to a packet, or NULL if none found
 */
MM_Packet *
MM_WorkPackets::getLeastFullPacket(MM_EnvironmentBase *env, int requiredSlots)
{
	MM_Packet *temps[_maxPacketSearch];
	int i, j, selected;
	intptr_t maxCapacity = requiredSlots - 1; 
	intptr_t curCapacity;
	int satisfactoryCapacity = OMR_MAX(_satisfactoryCapacity, requiredSlots);

	/* Search for a satisfactory packet */
	selected = -1;
	for(j = 0, i = 0; i < _maxPacketSearch; i++) {

		/* Get a packet from either the non empty list or the relatively 
		 * full list. If this fails then leave the loop as there are no more 
		 * packets available
		 */
		if(NULL == (temps[i] = getPacket(env, &_nonEmptyPacketList))) {
			if(NULL == (temps[i] = getPacket(env, &_relativelyFullPacketList))) {
				break;
			}
		}
		j += 1;

		/* If this packet has the greatest capacity so far, then select it. 
		 * If the capacity has reached the satisfactory level, then leave 
		 * the loop. If we have alrady got a non empty packet and we are now 
		 * on the relatively full list, then leave the loop as we are not 
		 * going to find a better packet.
		 */
		curCapacity = temps[i]->freeSlots();
		if(curCapacity > maxCapacity) {
			maxCapacity = curCapacity;
			selected = i;
			if(satisfactoryCapacity <= maxCapacity) {
				break; 
			}
		} else {
			if((maxCapacity >= _fullPacketThreshold)   /* we have a non-empty packet already */
			&& (curCapacity < _fullPacketThreshold)) {   /* we got to the full packet list */
				break;
			}
		}
	}

	/* If we didn't get any packets then return */
	if(0 == j) {
		return NULL;
	}

	/* Return the non selected packets. */
	while(j > 0) {
		j -= 1;
		if(j != selected) {
			putPacket(env, temps[j]);
		}
	}

	/* Return the selected packet. If here is not a selected packet, 
	 * then return NULL.
	 */
	if(-1 == selected) {
		return NULL;
	} else {
		return temps[selected];
	}
}

/**
 * Get a packet from the given list
 * 
 * @param list The list to get from
 * @return pointer to a packet, or NULL
 */
MM_Packet *
MM_WorkPackets::getPacket(MM_EnvironmentBase *env, MM_PacketList *list)
{
	MM_Packet *packet;
	
	packet = list->pop(env);
	
	if (NULL == packet) {
		return NULL;
	}
	
	packet->setOwner(env);

	return packet;
}

/**
 * Put a packet back to the correct list
 * 
 * @param packet The packet to put back
 */
void
MM_WorkPackets::putPacket(MM_EnvironmentBase *env, MM_Packet *packet)
{
	MM_PacketList *list;
	uintptr_t freeSlots = packet->freeSlots();
	bool mustNotifyWaitingThreads = false;

    /* Empty packet */
	if(freeSlots == _slotsInPacket) {
		list = &_emptyPacketList;
		packet->clearOwner();
				
	/* Full packet */
	} else if(freeSlots == 0) {
		list = &_fullPacketList;
		mustNotifyWaitingThreads = list->isEmpty();
		packet->resetOwner();

	/* Relatively full packet */
	} else if(freeSlots < _fullPacketThreshold) {
		list = &_relativelyFullPacketList;
		mustNotifyWaitingThreads = list->isEmpty();
		packet->resetOwner();

	/* Non empty packet */
	} else {
		list = &_nonEmptyPacketList;
		mustNotifyWaitingThreads = list->isEmpty();
		packet->resetOwner();
	}
	
	list->push(env, packet);
	
	if(mustNotifyWaitingThreads && (_inputListWaitCount > 0)) {
		notifyWaitingThreads(env);
	}
}

void
MM_WorkPackets::notifyWaitingThreads(MM_EnvironmentBase *env)
{
	/* Added an entry to a null list - notify any other threads that a new entry has appeared on the list */
	if (0 == omrthread_monitor_try_enter(_inputListMonitor)) {
		if (_inputListWaitCount > 0) {
			omrthread_monitor_notify(_inputListMonitor);
		}
		omrthread_monitor_exit(_inputListMonitor);
	}
}

/**
 * Put an output packet
 * 
 * @param packet The packet to put
 */
void
MM_WorkPackets::putOutputPacket(MM_EnvironmentBase *env, MM_Packet *packet)
{
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	env->_workPacketStats.workPacketsReleased += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	putPacket(env, packet);
}

/**
 * Get a deferred packet
 * If the deferred list is empty we try to get a packet from the empty list
 * 
 * @return pointer to a deferred packet, or NULL
 */
MM_Packet *
MM_WorkPackets::getDeferredPacket(MM_EnvironmentBase *env)
{	
	MM_Packet *packet;
 	
	if(NULL == (packet = getPacket(env, &_deferredPacketList))) {
		packet = getPacket(env, &_emptyPacketList);
	}
	
	return packet;
}

/**
 * Put a deferred packet back to the correct list
 * 
 * @param packet The packet to add to a list
 */
void
MM_WorkPackets::putDeferredPacket(MM_EnvironmentBase *env, MM_Packet *packet)
{	
	MM_PacketList *list;
	uintptr_t freeSlots = packet->freeSlots();

	packet->resetOwner();
	
	if(0 == freeSlots) {
		list = &_deferredFullPacketList;
	} else {
		list = &_deferredPacketList;
	}
	
	list->push(env, packet);
}

/**
 * Return the heap capactify factor used to determine how many packets to create.
 * This is only called during the initialization of the WorkPackets class.
 * 
 * @return the heap capactify factor
 */
float 
MM_WorkPackets::getHeapCapacityFactor(MM_EnvironmentBase *env)
{
	return (float)0.002;
}

/**
 * Get the current overflow handler to overflow this item
 * 
 * @param element - Reference to element to be overflowed 
 */
void
MM_WorkPackets::overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	_overflowHandler->overflowItem(env, item, type);
}

bool
MM_WorkPackets::getOverflowFlag()
{
	return _overflowHandler->_overflow;
}

void
MM_WorkPackets::clearOverflowFlag()
{
	_overflowHandler->_overflow = false;
}

bool
MM_WorkPackets::handleWorkPacketOverflow(MM_EnvironmentBase *env)
{
	bool result = false;

	if (getOverflowFlag()) {
		result = true;
		_overflowHandler->handleOverflow(env);
	}
	return result;
}

MM_WorkPacketOverflow *
MM_WorkPackets::createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
{
	return MM_WorkPacketOverflow::newInstance(env, workPackets);
}
