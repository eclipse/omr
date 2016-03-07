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

#if !defined(LOCKINGHEAPREGIONQUEUE_HPP_)
#define LOCKINGHEAPREGIONQUEUE_HPP_

#include <assert.h>
#define ASSERT_LEVEL 0
#define assert1(expr) assert((ASSERT_LEVEL < 1) || (expr));

#include "omrcfg.h"
#include "modronopt.h"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionQueue.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_LockingHeapRegionQueue : public MM_HeapRegionQueue
{
/* For efficient pushFront operation */
friend class MM_LockingFreeHeapRegionList;
	
/* Data members & types */
public:
protected:
private:
	MM_HeapRegionDescriptorSegregated *_head;
	MM_HeapRegionDescriptorSegregated *_tail;
	bool _needLock;
	omrthread_monitor_t _lockMonitor;
	
public:
	static MM_LockingHeapRegionQueue *newInstance(MM_EnvironmentBase *env, RegionListKind regionListKind, bool singleRegionOnly, bool concurrentAccess, bool trackFreeBytes = false);
	virtual void kill(MM_EnvironmentBase *env);
	
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * @param trackFreeBytes True if a running total of free bytes in the list should be kept.
	 * This value will not necessarily be accurate. It will remain fairly accurate as long as single
	 * region push and pop operations are used and updateCounts doesn't get called on a region that lives
	 * on the list.
	 */
	MM_LockingHeapRegionQueue(RegionListKind regionListKind, bool singleRegionOnly, bool concurrentAccess, bool trackFreeBytes) :
		MM_HeapRegionQueue(regionListKind, singleRegionOnly, trackFreeBytes),
		_head(NULL),
		_tail(NULL),
		_needLock(concurrentAccess),
		_lockMonitor(NULL)
	{
		_typeId = __FUNCTION__;
	}

	virtual bool isEmpty() { return 0 == _length; }

	virtual uintptr_t getTotalRegions();

	virtual void enqueue(MM_HeapRegionDescriptorSegregated *region)
	{
		lock();
		enqueueInternal(region);
		unlock();
	}

	/* enqueue src at the _end_ of the receiver's queue */
	virtual void enqueue(MM_HeapRegionQueue *srcAsPQ)
	{
		MM_LockingHeapRegionQueue* src = MM_LockingHeapRegionQueue::asLockingHeapRegionQueue(srcAsPQ);
		if (NULL == src->_head) { /* Nothing to move - single read needs no lock */
			return;
		}
		lock();
		src->lock();
		/* Remove from src */
		MM_HeapRegionDescriptorSegregated *front = src->_head;
		MM_HeapRegionDescriptorSegregated *back = src->_tail;
		uintptr_t srcLength = src->_length;
		src->_head = NULL;
		src->_tail = NULL;
		src->_length = 0;
		
		/* Add to back of self */
		front->setPrev(_tail); /* OK even if _tail is NULL */
		if (_tail == NULL) {
			_head = front;
		} else {
			_tail->setNext(front);
		}
		_tail = back;
		_length += srcLength;
		
		src->unlock();
		unlock();
	}

	virtual MM_HeapRegionDescriptorSegregated *dequeue()
	{
		lock();
		MM_HeapRegionDescriptorSegregated *result = dequeueInternal();
		unlock();
		return result;
	}

	/* check that the receiver is not empty before locking it and performing dequeue */
	MM_HeapRegionDescriptorSegregated *dequeueIfNonEmpty()
	{
		MM_HeapRegionDescriptorSegregated *region = NULL;
		if (0 != _length) {
			lock();
			region = dequeueInternal();
			unlock();
		}
		return region;
	}

	virtual uintptr_t dequeue(MM_HeapRegionQueue *targetAsPQ, uintptr_t count)
	{
		MM_LockingHeapRegionQueue* target = MM_LockingHeapRegionQueue::asLockingHeapRegionQueue(targetAsPQ);
		lock();
		target->lock();
		uintptr_t moved = dequeueInternal(target, count);
		target->unlock();
		unlock();
		return moved;
	}

	virtual uintptr_t debugCountFreeBytesInRegions();
	virtual void showList(MM_EnvironmentBase *env);

	/**
	 * Cast a HeapRegionQueue as a LockingHeapRegionQueue
	 */
	MMINLINE static MM_LockingHeapRegionQueue* asLockingHeapRegionQueue(MM_HeapRegionQueue * pl) { return (MM_LockingHeapRegionQueue *)pl; }

protected:
private:		
	MMINLINE void lock() {
		if (_needLock) {
			omrthread_monitor_enter(_lockMonitor);
		}
	}
	MMINLINE void unlock() {
		if (_needLock) {
			omrthread_monitor_exit(_lockMonitor);
		}
	}
	
	void enqueueInternal(MM_HeapRegionDescriptorSegregated *region)
	{ 
		assert1(NULL == region->getNext() && NULL == region->getPrev());
		if (NULL == _head) {
			_head = _tail = region;
		} else {
			_tail->setNext(region);
			region->setPrev(_tail);
			_tail = region;
		}
		_length++;
	}

	uintptr_t dequeueInternal(MM_LockingHeapRegionQueue *target, uintptr_t count)
	{
		uintptr_t moved = 0;
		while (count-- > 0) {
			MM_HeapRegionDescriptorSegregated *p = dequeueInternal();
			if (p == NULL) {
				break;
			}
			moved++;
			target->enqueueInternal(p);
		}
		return moved;
	}

	MM_HeapRegionDescriptorSegregated *dequeueInternal()
	{
		MM_HeapRegionDescriptorSegregated *result = _head;
		if (_head != NULL) {
			_length--;
			_head = result->getNext();
			result->setNext(NULL);
			if (NULL == _head) {
				_tail = NULL;
			} else {
				_head->setPrev(NULL);
			}
		}
		return result;
	}	
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* LOCKINGHEAPREGIONQUEUE_HPP_ */
