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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Realtime
 */

#if !defined(HEAPREGIONLIST_HPP_)
#define HEAPREGIONLIST_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"

/**
 * A HeapRegionList is a List (ordered collection) of HeapRegionDescriptrs. 
 * 
 * Regions on a RegionList can either represent just themselves or can be the cannonical
 * Region for a range of Regions.  A particular RegionList instance will only contain one
 * kind of regions (_singleRegionsOnly true/false respectively).
 * 
 * We have two main implementations of RegionList that add additional operations.
 * RegionQueue supports queue operations (FIFO insertion/deletion via enqueue/dequeue).
 * FreeRegionList supports stack operations (LIFO insertion/deletion via push/pop) plus
 * a general removeAny capability to support coalesce. 
 *
 * All Regions on a RegionList must be contained in the RegionTable associated with the RegionList.
 */
class MM_HeapRegionList : public MM_BaseVirtual
{
/* Data members & types */		
public:	
	/**< An Enumeration of the different kinds of RegionList.
	 *   Merely descriptive for Metronome, but in Staccato used to detect that
	 *   a region has been moved from one list to another concurrently with
	 *   region list iteration by another thread.
	 *   
	 *   Within a single GC cycle, the RegionLists form a lattice.
	 *   The valid transitions between region lists are defined by isValidTransition. 
	 */
	typedef enum {
		HRL_KIND_LOCAL_WORK = 0,
		HRL_KIND_MULTI_FREE = 1,
		HRL_KIND_FREE = 2,
		HRL_KIND_AVAILABLE = 3,
		HRL_KIND_FULL = 4,
		HRL_KIND_SWEEP = 5,
		HRL_KIND_COALESCE = 6
	} RegionListKind;
	
protected:
	uintptr_t _length;	
	/**< What kind of region list is this? @see ListKind for description of RegionList kinds */
	RegionListKind _regionListKind;
	/**< Do regions on the list represent only themselves, or do they encode region ranges (Large & MultiFree) */
	bool _singleRegionsOnly;
	
private:	
	
/* Methods */	
public:
	virtual void kill(MM_EnvironmentBase *env) = 0;
	
	virtual bool initialize(MM_EnvironmentBase *env) = 0;
	virtual void tearDown(MM_EnvironmentBase *env) = 0;

	/**
	 * @param pt The RegionList that contains all regions that will be put on this RegionList.
	 * @param regionListKind  The RegionListKind of this RegionList.
	 * @param singleRegionsOnly True if this heapRgionlist is for small/arraylet, false if it is for large (multi-region)
	 */
	MM_HeapRegionList(RegionListKind regionListKind, bool singleRegionsOnly) :
		_length(0),
		_regionListKind(regionListKind),
		_singleRegionsOnly(singleRegionsOnly)
	{
		_typeId = __FUNCTION__;
	}

	uintptr_t length() { return _length; }

	virtual bool isEmpty() = 0;

	virtual uintptr_t getTotalRegions() = 0;

	virtual void showList(MM_EnvironmentBase *env) = 0;
	
	bool isAvailableList() { return _regionListKind == HRL_KIND_AVAILABLE; }
	bool isFullList() { return _regionListKind == HRL_KIND_FULL; }
	bool isSweepList() { return _regionListKind == HRL_KIND_SWEEP; }
	bool isFreeList() { return _regionListKind == HRL_KIND_FREE || _regionListKind == HRL_KIND_MULTI_FREE; }
	
	static const char*
	describeListKind(RegionListKind regionListKind)
	{
		switch (regionListKind) {
		case HRL_KIND_LOCAL_WORK: return "local work list";
		case HRL_KIND_MULTI_FREE: return "multi free list";
		case HRL_KIND_FREE: return "single free list";
		case HRL_KIND_AVAILABLE: return "available list";
		case HRL_KIND_FULL: return "full list";
		case HRL_KIND_SWEEP: return "sweep list";
		case HRL_KIND_COALESCE: return "coalesce list";
		default: return "unknown kind of list";
		}
	}
	
	const char *
	describeList()
	{ 
		return describeListKind(_regionListKind);
	}
	
	static bool
	isValidTransition(RegionListKind from, RegionListKind to)
	{
		if (HRL_KIND_LOCAL_WORK == from || HRL_KIND_LOCAL_WORK == to) {
			return true; // SIGH. Local work blinds us because we need 3 states to really check it.
		}
		
		switch(from) {
		case HRL_KIND_MULTI_FREE:
			return HRL_KIND_FREE == to || HRL_KIND_FULL == to || HRL_KIND_COALESCE == to;
		case HRL_KIND_FREE:
			return HRL_KIND_AVAILABLE == to || HRL_KIND_FULL == to || HRL_KIND_COALESCE == to;
		case HRL_KIND_AVAILABLE:
			return HRL_KIND_FULL == to || HRL_KIND_SWEEP == to;
		case HRL_KIND_FULL:
			return HRL_KIND_SWEEP == to;
		case HRL_KIND_SWEEP:
			return HRL_KIND_FREE == to || HRL_KIND_AVAILABLE == to || HRL_KIND_FULL == to;
		case HRL_KIND_COALESCE:
			/* NOTE: COALESCE ==> COALESCE is only allowable because coalesce list is still using locks */
			return HRL_KIND_FREE == to || HRL_KIND_MULTI_FREE == to || HRL_KIND_FULL == to || HRL_KIND_COALESCE == to;
		default:
			return false;
		}
	}
};

#endif /* HEAPREGIONLIST_HPP_ */
