/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef EVACUATORWORKLIST_HPP_
#define EVACUATORWORKLIST_HPP_

#include "omrmutex.h"

#include "Base.hpp"
#include "EvacuatorBase.hpp"
#include "Forge.hpp"
#include "ForwardedHeader.hpp"

/**
 * Location and length in bytes of a contiguous strip of copy in survivor or tenure space. These
 * are allocated from objects already evacuated, overlaying the object body starting at object
 * head + 8 bytes and arranged in linked lists. Each evacuator thread  maintains two such lists
 * -- one for its unscanned queue and one for free MM_EvacuatorWorkPacket objects.
 *
 */
typedef struct MM_EvacuatorWorkPacket {
	omrobjectptr_t base;			/* points to base of material to be scanned in packet */
	uintptr_t length;				/* length of material to be scanned in work packet, starting from base + offset */
	uintptr_t offset;				/* >0 for split arrays only, offset from base to first slot to scan */
	MM_EvacuatorWorkPacket *next;	/* points to next work packet in queue, or NULL */
} MM_EvacuatorWorkPacket;

/**
 * Linked list of free MM_EvacuatorWorkPacket is accessed only by owning thread, in LIFO order.
 */
class MM_EvacuatorFreelist : public MM_Base
{
/*
 * Data members
 */
public:

private:
	/* if starved for free elements, allocate a chunk of this size from system memory and refresh */
	static const uintptr_t work_packet_chunksize = 128;

	/* underflow chunk obtained from evacuator stack or system memory to seed or refresh free list */
	typedef struct UnderflowChunk {
		MM_EvacuatorWorkPacket packet[work_packet_chunksize];
		UnderflowChunk *next;
	} UnderflowChunk;

	MM_EvacuatorWorkPacket *_head;	/* head of free list (last one in) */
	volatile uintptr_t _count;		/* number of elements on free list */
	UnderflowChunk *_underflow;		/* head of list of underflow chunks allocated from system memory */
	MM_Forge *_forge;				/* memory allocator for underflow chunks */

protected:

/*
 * Function members
 */
public:
	/**
	 * Mark work packet as free and return its successor
	 */
	MMINLINE MM_EvacuatorWorkPacket *flush(MM_EvacuatorWorkPacket *packet) { packet->base = NULL; return packet->next; }

	/**
	 * Return the number of contained free elements
	 */
	MMINLINE uintptr_t getCount() { return _count; }

	/**
	 * Get the next available free packet
	 *
	 * @return the next available free packet, or NULL if none available
	 */
	MMINLINE MM_EvacuatorWorkPacket *
	next()
	{
		Debug_MM_true((0 == _count) == (NULL == _head));
		if (0 == _count) {
			refresh();
		}
		MM_EvacuatorWorkPacket *free = NULL;
		if (0 < _count) {
			free = _head;
			free->length = 0;
			_head = free->next;
			free->next = NULL;
			free->offset = 0;
			_count -= 1;
		}
		Debug_MM_true(NULL != free);
		return free;
	}

	/**
	 * Add a free packet at the head of the list. This may include work packets taken from
	 * forge memory owned by another evacuator.
	 *
	 * @param free the packet to add
	 */
	MMINLINE void
	add(MM_EvacuatorWorkPacket *free)
	{
		Debug_MM_true((0 == _count) == (NULL == _head));
		free->length = 0;
		free->base = NULL;
		free->offset = 0;
		free->next = _head;
		_head = free;
		_count += 1;
		Debug_MM_true((0 == _count) == (NULL == _head));
	}

	/**
	 * Claim 0 or more free elements from system (forge) memory or C++ stack space
	 *
	 * @return the number of free elements added
	 */
	MMINLINE uintptr_t
	refresh()
	{
		if ((NULL == _underflow) || (0 == _count)) {
			/* allocate a new underflow chunk and link it in at head of underflow list*/
			UnderflowChunk *underflowChunk = (UnderflowChunk *)_forge->allocate(sizeof(UnderflowChunk), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
			underflowChunk->next = _underflow;
			_underflow = underflowChunk;

			/* add free elements from underflow chunk to free list */
			MM_EvacuatorWorkPacket *packet = underflowChunk->packet;
			MM_EvacuatorWorkPacket *end = packet + work_packet_chunksize;
			while (packet < end) {
				add(packet);
				packet += 1;
			}
		}

		Debug_MM_true(0 < _count);
		return _count;
	}

	MMINLINE void
	reload()
	{
		if (NULL != _underflow) {
			_count = 0;
			_head = NULL;
			for (UnderflowChunk *underflowChunk = _underflow; NULL != underflowChunk; underflowChunk = underflowChunk->next) {
				/* add free elements from underflow chunk to free list */
				MM_EvacuatorWorkPacket *packet = underflowChunk->packet;
				MM_EvacuatorWorkPacket *end = packet + work_packet_chunksize;
				while (packet < end) {
					Debug_MM_true(NULL == packet->base);
					add(packet);
					packet += 1;
				}
			}
		} else {
			refresh();
		}
	}

	/**
	 * Releases all underflow chunks, if any are allocated, and resets list to empty state.
	 */
	MMINLINE void
	reset()
	{
		/* deallocate all underflow chunks allocated from system memory (forged) */
		while (NULL != _underflow) {
			UnderflowChunk *next = _underflow->next;
			_forge->free(_underflow);
			_underflow = next;
		}
		_head = NULL;
		_count = 0;
	}

	/**
	 * Constructor.
	 */
	MM_EvacuatorFreelist(MM_Forge *forge)
		: MM_Base()
		, _head(NULL)
		, _count(0)
		, _underflow(NULL)
		, _forge(forge)
	{ }
};

/**
 * FIFO queue of work packets. Other threads may access this list if their own queues run dry.  The
 * queue volume is modified only within critical regions guarded by a mutex declared by the evacuator
 * that owns the queue. It is exposed to other evacuator threads as a volatile value to enable them to
 * check queue volume without taking the mutex.
 */
class MM_EvacuatorWorklist : public MM_Base
{
/*
 * Data members
 */
private:
	MM_EvacuatorWorkPacket *_head;	/* points to head of worklist */
	MM_EvacuatorWorkPacket *_tail;	/* points to tail of worklist */
	volatile uint64_t _volume;		/* (bytes) volume of work contained in list */

protected:
public:

/*
 * Function members
 */
private:
protected:
public:
	/**
	 * Returns a pointer to the volatile sum of the number of bytes contained in work packets in the list
	 */
	MMINLINE volatile uint64_t *volume() { return &_volume; }

	/**
	 * Add a work packet at the end of the list. If the contained work is contiguous with the work in the
	 * packet at the tail of the list the contained work will be coalesced into the tail packet and the
	 * packet will be added to the freelist.
	 *
	 * @param work the packet to add
	 * @param freelist the list of free work packets
	 * @return true if the worklist was empty before adding the packet
	 */
	MMINLINE void
	add(MM_EvacuatorWorkPacket *work, MM_EvacuatorFreelist *freeList)
	{
		Debug_MM_true((0 == _volume) == (NULL == _head));
		Debug_MM_true((NULL == _head) == (NULL == _tail));
		Debug_MM_true((NULL != work->base) && (0 < work->length));

		work->next = NULL;
		uintptr_t length = work->length;
		if (NULL != _tail) {
			if ((0 == work->offset) && (((uintptr_t)_tail->base + _tail->length) == (uintptr_t)work->base)) {
				_tail->length += work->length;
				freeList->add(work);
			} else {
				_tail->next = work;
				_tail = work;
			}
		} else {
			_head = _tail = work;
		}

		VM_AtomicSupport::addU64(&_volume, length);
		Debug_MM_true((0 == _volume) == (NULL == _head));
	}

	/**
	 * Get the next available work packet from the head of the list.
	 *
	 * @return the next work packet, if available, or NULL
	 */
	MMINLINE MM_EvacuatorWorkPacket *
	next()
	{
		Debug_MM_true((0 == _volume) == (NULL == _head));
		Debug_MM_true((NULL == _head) == (NULL == _tail));

		MM_EvacuatorWorkPacket *take = NULL;
		if (NULL != _head) {
			VM_AtomicSupport::subtractU64(&_volume, _head->length);
			take = _head;
			_head = take->next;
			take->next = NULL;
			if (take == _tail) {
				Debug_MM_true(NULL == _head);
				_tail = NULL;
			}
		}

		Debug_MM_true((0 == _volume) == (NULL == _head));
		return take;
	}

	/**
	 * Just clear count and list pointers. The freelist owns the forge memory
	 * allocated for one evacuator and this worklist may contain work packets
	 * allocated and distributed from other evacuators. Each evacuator reclaims
	 * free itpacketsrom its own allocations at the beginning of each gc cycle.
	 */
	MMINLINE void
	flush(MM_EvacuatorFreelist *freeList)
	{
		Debug_MM_true((0 == _volume) == (NULL == _head));
		VM_AtomicSupport::setU64(&_volume, 0);
		_head = _tail = NULL;
		Debug_MM_true((0 == _volume) == (NULL == _head));
	}

	/**
	 * Constructor
	 */
	MM_EvacuatorWorklist()
		: MM_Base()
		, _head(NULL)
		, _tail(NULL)
		, _volume(0)
	{ }
};

#endif /* EVACUATORWORKLIST_HPP_ */
