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

#if !defined(HEAPREGIONQUEUE_HPP_)
#define HEAPREGIONQUEUE_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionList.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * A RegionList on which every Region stands only for itself.
 * SingleRegionList support a queue (FIFO) abstraction where Region objects
 * can be enqueued at the tail of the list and dequeued from the head. 
 */
class MM_HeapRegionQueue : public MM_HeapRegionList
{
/* Data members & types */		
public:		
protected:
private:	
	
/* Methods */	
public:

	virtual void kill(MM_EnvironmentBase *env) = 0;
	
	virtual bool initialize(MM_EnvironmentBase *env) = 0;
	virtual void tearDown(MM_EnvironmentBase *env) = 0;

	/**
	 * @param trackFreeBytes True if a running total of free bytes in the list should be kept.
	 * This value will not necessarily be accurate. It will remain fairly accurate as long as single
	 * region push and pop operations are used and updateCounts doesn't get called on a region that lives
	 * on the list.
	 */
	MM_HeapRegionQueue(RegionListKind regionListKind, bool singleRegionsOnly, bool trackFreeBytes) : 
		MM_HeapRegionList(regionListKind, singleRegionsOnly)
	{
		_typeId = __FUNCTION__;
	}
	
	virtual void enqueue(MM_HeapRegionDescriptorSegregated *region) = 0;

	virtual void enqueue(MM_HeapRegionQueue *target) = 0;

	virtual MM_HeapRegionDescriptorSegregated *dequeue() = 0;

	virtual uintptr_t dequeue(MM_HeapRegionQueue *target, uintptr_t count) = 0;

	virtual uintptr_t debugCountFreeBytesInRegions() = 0;

	/* Virtual methods inherited from RegionList */
	virtual bool isEmpty() = 0;
	virtual uintptr_t getTotalRegions() = 0;
	virtual void showList(MM_EnvironmentBase *env) = 0;
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* HEAPREGIONQUEUE_HPP_ */
