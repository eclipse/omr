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

#if !defined(HEAPREGIONDESCRIPTORSEGREGATED_HPP_)
#define HEAPREGIONDESCRIPTORSEGREGATED_HPP_

#if defined(OMR_GC_SEGREGATED_HEAP)

#include "omrcfg.h"
#include "omrcomp.h"
#include "sizeclasses.h"

#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptor.hpp"
#include "MemoryPoolAggregatedCellList.hpp"

class MM_HeapRegionDescriptorSegregated : public MM_HeapRegionDescriptor
{
	/*
	 * Data members
	 */
public:
protected:
	uintptr_t **_arrayletBackPointers;

private:
	uintptr_t _sizeClass;
	MM_MemoryPoolAggregatedCellList _memoryPoolACL;
	MM_HeapRegionDescriptorSegregated *_prev;		/**< used by RegionList implementations to maintain links */
	MM_HeapRegionDescriptorSegregated *_next;		/**< used by RegionList implementations to maintain links */
	MM_HeapRegionManager *_regionManager;
	OMR_SizeClasses *_segregatedSizeClasses;
	uintptr_t _nextArrayletIndex; /**< next arraylet to use for allocation */
	
	/*
	 * Function members
	 */
public:
	MM_HeapRegionDescriptorSegregated(MM_EnvironmentBase *env, void *lowAddress, void *highAddress) :
		MM_HeapRegionDescriptor(env, lowAddress, highAddress)
		,_sizeClass(0)
		,_memoryPoolACL(env, 0 /* minimumFreeEntrySize */)
		,_prev(NULL)
		,_next(NULL)
		,_regionManager(NULL)
		,_segregatedSizeClasses(env->getOmrVM()->_sizeClasses)
		,_nextArrayletIndex(0)
	{
		_arrayletBackPointers = ((uintptr_t **)(this + 1));
		_typeId = __FUNCTION__;
	}
	
	bool initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager);
	void tearDown(MM_EnvironmentBase *env);

	static bool initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress);

	/* HeapRegionList/Queue helpers */
	MMINLINE MM_HeapRegionDescriptorSegregated *getNext() { return _next; }
	MMINLINE void setNext(MM_HeapRegionDescriptorSegregated *next) { _next = next; }
	MMINLINE MM_HeapRegionDescriptorSegregated *getPrev() { return _prev; }
	MMINLINE void setPrev(MM_HeapRegionDescriptorSegregated *prev) { _prev = prev; }

	MMINLINE MM_MemoryPoolAggregatedCellList *getMemoryPoolACL() { return (MM_MemoryPoolAggregatedCellList *)getMemoryPool(); }
	void setSizeClass(uintptr_t sizeClass) {_sizeClass = sizeClass;}
	uintptr_t getSizeClass() {return _sizeClass;}
	uintptr_t getCellSize()
	{
		OMR_SizeClasses *sizeClasses = _segregatedSizeClasses;
		return *(&sizeClasses->smallCellSizes[getSizeClass()]);
	}
	uintptr_t getNumCells()
	{
		OMR_SizeClasses *sizeClasses = _segregatedSizeClasses;
		return *(&sizeClasses->smallNumCells[getSizeClass()]);
	}

	bool isReserved() { return getRegionType() == RESERVED; }
	bool isSmall() { return getRegionType() == SEGREGATED_SMALL; }
	bool isLarge() { return getRegionType() == SEGREGATED_LARGE; }
#if defined(OMR_GC_ARRAYLETS)
	bool isArraylet() { return getRegionType() == ARRAYLET_LEAF; }
#endif /* defined(OMR_GC_ARRAYLETS) */
	bool isFree() { return getRegionType() == FREE; }
	bool isCanonical() { return isReserved() || isSmall() || getRangeCount() >= 1; }

	void setRange(RegionType type, uintptr_t range);
	uintptr_t getRange() { return getRangeCount(); };
	MM_HeapRegionDescriptorSegregated *splitRange(uintptr_t numRegionsToSplit);
	void resetTailFree(uintptr_t range);

	bool joinFreeRangeInit(MM_HeapRegionDescriptorSegregated *possNext);
	void joinFreeRangeComplete();

	MM_HeapRegionDescriptorSegregated *getRangeHead() { return (MM_HeapRegionDescriptorSegregated *)_headOfSpan; }
	void setRangeHead(MM_HeapRegionDescriptorSegregated *head) { _headOfSpan = head; }
	uintptr_t getRangeCount() { return _regionsInSpan; }
	void setRangeCount(uintptr_t count) { _regionsInSpan = count; }

	uintptr_t formatFresh(MM_EnvironmentBase *env, uintptr_t sizeClass, void *lowAddress);
	void emptyRegionAllocated(MM_EnvironmentBase* env);
	void emptyRegionReturned(MM_EnvironmentBase* env);

#if defined(OMR_GC_ARRAYLETS)
	uintptr_t *allocateArraylet(MM_EnvironmentBase *env, omrarrayptr_t parentIndexableObject);
	void setArraylet();
	MMINLINE uintptr_t whichArraylet(uintptr_t *arraylet, uintptr_t arrayletLeafLogSize) { return ((uintptr_t)arraylet - (uintptr_t)getLowAddress()) >> arrayletLeafLogSize; }
	MMINLINE uintptr_t* getArraylet(uintptr_t index, uintptr_t arrayletLeafLogSize) { return (uintptr_t*) (((uintptr_t)getLowAddress()) + (index << arrayletLeafLogSize)); }
	MMINLINE uintptr_t* getArrayletParent(uintptr_t index) { return _arrayletBackPointers[index]; }
	MMINLINE void   setArrayletParent(uintptr_t index, uintptr_t *backPointer) { _arrayletBackPointers[index] = backPointer; }
	MMINLINE void clearArraylet(uintptr_t index) { _arrayletBackPointers[index] = NULL; }
	MMINLINE bool isArrayletUnused(uintptr_t index) { return _arrayletBackPointers[index] == NULL; }
	MMINLINE bool isArrayletUsed(uintptr_t index)   { return _arrayletBackPointers[index] != NULL; }
	MMINLINE void setNextArrayletIndex (uintptr_t index)   { _nextArrayletIndex = index; }
	void addBytesFreedToArrayletBackout(MM_EnvironmentBase* env);
	void addBytesFreedToSmallSpineBackout(MM_EnvironmentBase* env);
#endif /* defined(OMR_GC_ARRAYLETS) */

	void setLarge(uintptr_t range) { setRange(SEGREGATED_LARGE, range); }
	void setSmall(uintptr_t sizeClass);
	void setFree(uintptr_t range);
	void updateCounts(MM_EnvironmentBase *env, bool fromFlush);
	uintptr_t debugCountFreeBytes();
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* HEAPREGIONDESCRIPTORSEGREGATED_HPP_ */
