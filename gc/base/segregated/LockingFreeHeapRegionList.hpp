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

#if !defined(LOCKINGFREEHEAPREGIONLIST_HPP_)
#define LOCKINGFREEHEAPREGIONLIST_HPP_

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "modronopt.h"

#include "FreeHeapRegionList.hpp"
#include "LockingHeapRegionQueue.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * The Locking implementation of a FreeHeapRegionList.
 */
class MM_LockingFreeHeapRegionList : public MM_FreeHeapRegionList
{
/* Data members & types */
public:
protected:
private:
	MM_HeapRegionDescriptorSegregated *_head;
	MM_HeapRegionDescriptorSegregated *_tail;
	omrthread_monitor_t _lockMonitor;

/* Methods */
public:
	static MM_LockingFreeHeapRegionList *newInstance(MM_EnvironmentBase *env, MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly);
	virtual void kill(MM_EnvironmentBase *env);
	
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_LockingFreeHeapRegionList(MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly) :
		MM_FreeHeapRegionList(regionListKind, singleRegionsOnly),
		_head(NULL),
		_tail(NULL),
		_lockMonitor(NULL)
	{
		_typeId = __FUNCTION__;
	}

	virtual void
	push(MM_HeapRegionDescriptorSegregated *region)
	{
		lock();
		pushInternal(region);
		unlock();
	}
	
	virtual void
	push(MM_HeapRegionQueue *srcAsPQ)
	{ 
		MM_LockingHeapRegionQueue* src = MM_LockingHeapRegionQueue::asLockingHeapRegionQueue(srcAsPQ);
		if (src->_head == NULL) { /* Nothing to move - single read needs no lock */
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
		
		/* Add to front of self */
		back->setNext(_head); /* OK even if _head is NULL */
		if (_head == NULL) {
			_tail = back;
		} else {
			_head->setPrev(back);
		}
		_head = front;
		_length += srcLength;
		
		src->unlock();
		unlock();
	}
	
	virtual void 
	push(MM_FreeHeapRegionList *srcAsFPL) 
	{ 
		MM_LockingFreeHeapRegionList* src = MM_LockingFreeHeapRegionList::asLockingFreeHeapRegionList(srcAsFPL);
		if (src->_head == NULL) { /* Nothing to move - single read needs no lock */
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
		
		/* Add to front of self */
		back->setNext(_head); /* OK even if _head is NULL */
		if (_head == NULL) {
			_tail = back;
		} else {
			_head->setPrev(back);
		}
		_head = front;
		_length += srcLength;
		
		src->unlock();
		unlock();
	}

	virtual MM_HeapRegionDescriptorSegregated *
	pop()
	{
		lock();
		MM_HeapRegionDescriptorSegregated *result = popInternal();
		unlock();
		return result;
	}
	
	virtual void
	detach(MM_HeapRegionDescriptorSegregated *cur)
	{
		lock();
		detachInternal(cur);
		unlock();
	}

	virtual MM_HeapRegionDescriptorSegregated* allocate(MM_EnvironmentBase *env, uintptr_t szClass, uintptr_t numRegions, uintptr_t maxExcess);

	virtual uintptr_t getTotalRegions();
	virtual uintptr_t getMaxRegions();

	virtual void showList(MM_EnvironmentBase *env);

	/**
	 * Cast a FreeHeapRegionList as a LockingFreeHeapRegionList
	 */
	MMINLINE static MM_LockingFreeHeapRegionList*
	asLockingFreeHeapRegionList(MM_FreeHeapRegionList * pl)
	{
		return (MM_LockingFreeHeapRegionList *)pl;
	}

protected:
private:
	MMINLINE void lock() { omrthread_monitor_enter(_lockMonitor); }
	
	MMINLINE void unlock() { omrthread_monitor_exit(_lockMonitor); }

	void
	pushInternal(MM_HeapRegionDescriptorSegregated *region)
	{
		Assert_MM_true(NULL == region->getNext() && NULL == region->getPrev());
		_length++;
		if (NULL == _head) {
			_head = region;
			_tail = region;
		} else {
			_head->setPrev(region);
			region->setNext(_head);
			_head = region;
		}
	}

	MM_HeapRegionDescriptorSegregated *
	popInternal()
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

	/*
	 * This method should be used with care.  In particular, it is wrong to detach from a freelist
	 * while iterating over it unless the detach stops further iteration.
	 */
	void
	detachInternal(MM_HeapRegionDescriptorSegregated *cur)
	{
		_length--;
		MM_HeapRegionDescriptorSegregated *prev = cur->getPrev();
		MM_HeapRegionDescriptorSegregated *next = cur->getNext();
		if (prev != NULL) {
			Assert_MM_true(prev->getNext() == cur);
			prev->setNext(next);
		} else {
			Assert_MM_true(cur == _head);
		}
		if (next != NULL) {
			Assert_MM_true(next->getPrev() == cur);
			next->setPrev(prev);
		} else {
			Assert_MM_true(cur == _tail);
		}
		cur->setPrev(NULL);
		cur->setNext(NULL);
		if (_head == cur) {
			_head = next;
		}
		if (_tail == cur) {
			_tail = prev;
		}
	}
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* LOCKINGFREEHEAPREGIONLIST_HPP_ */
