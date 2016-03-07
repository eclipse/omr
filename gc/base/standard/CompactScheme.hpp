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

#if !defined(COMPACTSCHEMEBASE_HPP_)
#define COMPACTSCHEMEBASE_HPP_

#include "omrcfg.h"
#include "omr.h"

#include "BaseVirtual.hpp"
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "MarkingScheme.hpp"
#include "MarkMap.hpp"
#include "SlotObject.hpp"

class MM_AllocateDescription;
class MM_EnvironmentStandard;
class MM_Dispatcher;
class MM_Heap;
class MM_HeapRegionDescriptorStandard;
class MM_MemoryPool;
class MM_MemorySubSpace;
class CompactTableEntry;

class MM_CompactMemoryPoolState : public MM_BaseVirtual
{
public :
	MM_MemoryPool				*_memoryPool;
	MM_HeapLinkedFreeHeader		*_freeListHead;
	uintptr_t 						_freeBytes;
	uintptr_t 						_freeHoles;
	uintptr_t 						_largestFreeEntry;

	MM_HeapLinkedFreeHeader		*_previousFreeEntry;
	uintptr_t 						_previousFreeEntrySize;

	MM_CompactMemoryPoolState() :
		_memoryPool(NULL),
		_freeListHead(NULL),
		_freeBytes(0),
		_freeHoles(0),
		_largestFreeEntry(0),
		_previousFreeEntry(NULL),
		_previousFreeEntrySize(0)
	{}

	MMINLINE void clear()
	{
		_memoryPool = NULL;
		_freeListHead = NULL;
		_freeBytes = 0;
		_freeHoles = 0;
		_largestFreeEntry = 0;

		_previousFreeEntry=NULL;
		_previousFreeEntrySize=0;
	}
};

class MM_CompactScheme : public MM_BaseVirtual
{
private:
    /* As usual, region addresses are up to, but not including, hi. */
    struct SubAreaEntry {
    	MM_MemoryPool *memoryPool;
		omrobjectptr_t firstObject;
		omrobjectptr_t freeChunk;
        volatile uintptr_t state;
        volatile uintptr_t currentAction; /**< record the status of the subarea for parallelization */
        
    	/* legal values for currentAction */
    	enum {
    		none = 0,
    		setting_real_limits,
    		evacuating,
    		fixing_up,
    		rebuilding_mark_bits,
    		fixing_heap_for_walk
    	};
    	
    	/* legal values for state
    	 * there are 4 dynamic states that change this way:
    	 * init --> ready <--> busy --> full
    	 * The rest are set once during setupSubAreaTable and doesn't change
    	 * during compaction.
    	 * The end_segment sub areas at the end of each segment are used also to
    	 * hold the segment's top address
    	 */
    	enum State { 
    		init = 0,
    		busy,
    		ready,
    		full,
    		fixup_only,
    		end_segment,
    		end_heap
    	};
    };

protected:
    OMR_VM *_omrVM;
    MM_GCExtensionsBase *_extensions;
    MM_Dispatcher *_dispatcher;
    MM_MarkingScheme *_markingScheme;
	MM_HeapRegionManager *_rootManager; /**< The root region manager which holds the MM_HeapRegionDescriptor instances which manage the properties of the regions */

    MM_Heap *_heap;
    uintptr_t _heapBase;
    CompactTableEntry *_compactTable;
    MM_MarkMap *_markMap;
	uintptr_t _subAreaTableSize;  /**< Size of the subAreaTable */
    SubAreaEntry *_subAreaTable;  /**< Reference to the subAreaTable which is shared data from the SweepHeapSectioning */
    omrobjectptr_t _compactFrom;
    omrobjectptr_t _compactTo;
public:

    /*
     * Page represents space can be marked by one slot(uintptr_t) of Compressed Mark Map
     * In Compressed Mark Map one bit represents twice more space then one bit of regular Mark Map
     * So, sizeof_page should be double of number of bytes represented by one uintptr_t in Mark Map
     */
    enum { sizeof_page = 2 * J9MODRON_HEAP_BYTES_PER_HEAPMAP_SLOT };

private:
    omrobjectptr_t freeChunkEnd(omrobjectptr_t chunk);
	size_t getFreeChunkSize(omrobjectptr_t freeChunk);
	void setFreeChunkSize(omrobjectptr_t deadObject, uintptr_t deadObjectSize);
	size_t setFreeChunk(omrobjectptr_t from, omrobjectptr_t to);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);


    void createSubAreaTable(MM_EnvironmentStandard *env, bool singleThreaded);
    /**
     * Set the real limits for a specific subArea
     *
     * @param env[in] the current thread
     */
    void setRealLimitsSubAreas(MM_EnvironmentStandard *env);
    void removeNullSubAreas(MM_EnvironmentStandard *env);
    void completeSubAreaTable(MM_EnvironmentStandard *env);

    void saveForwardingPtr(class CompactTableEntry&,
                            omrobjectptr_t objectPtr,
                            omrobjectptr_t forwardingPtr,
                            intptr_t &page,
                            intptr_t &counter);

    omrobjectptr_t doCompact(MM_EnvironmentStandard *env,
						MM_MemorySubSpace *memorySubSpace,
                        omrobjectptr_t start,
                        omrobjectptr_t finish,
                        omrobjectptr_t &deadObject,
                        uintptr_t &objectCount,
                        uintptr_t &byteCount,
                        bool evacuate);

    /**
     * Attempt to evacuate objects from the specified subArea.
     *
     * @param env[in] the current thread
     * @param[in] subAreaRegion the region which contains the subArea
     * @param[in] subAreaTableEvacuate the subArea for subAreaRegion
     * @param[in] i The current subArea index
     * @param[in/out] objectCount the number of objects moved (accumulated)
     * @param[in/out] byteCount the number of bytes moved (accumulated)
     * @param[in/out] the number of objects skipped (accumulated)
     */
	void evacuateSubArea(MM_EnvironmentStandard *env,
	    	MM_HeapRegionDescriptorStandard *subAreaRegion,
	    	SubAreaEntry *subAreaTableEvacuate,
	    	intptr_t i,
	    	uintptr_t &objectCount,
	    	uintptr_t &byteCount,
	    	uintptr_t &skippedObjectCount);

    void moveObjects(MM_EnvironmentStandard *env, uintptr_t &objectCount, uintptr_t &byteCount, uintptr_t &skippedObjectCount);

    /**
     * Fix up all references to moved objects in the specified subArea
     *
     * @param env[in] the current thread
     * @param[in] firstObject The first object in the subArea
     * @param[in] finish The last object in the subArea
     * @param markedOnly[in] Should only marked objects be walked
     * @param[in/out] objectCount the number of objects fixed up (accumulated)
     */
    void fixupSubArea(MM_EnvironmentStandard *env, omrobjectptr_t firstObject, omrobjectptr_t finish,  bool markedOnly, uintptr_t& objectCount);
	void fixupObjects(MM_EnvironmentStandard *env, uintptr_t& objectCount);

    void rebuildFreelist(MM_EnvironmentStandard *env);

    void addFreeEntry(MM_EnvironmentStandard *env,
					MM_MemorySubSpace *memorySubSpace,
					MM_CompactMemoryPoolState *poolState,
					void *currentFreeBase,
					uintptr_t currentFreeSize);

	/**
     * Return the page index for an object.
     * long int, always positive (in particular, -1 is an invalid value)
     */
    MMINLINE intptr_t pageIndex(omrobjectptr_t objectPtr) const
    {
        uintptr_t markIndex = (uintptr_t)objectPtr - _heapBase;
        return markIndex / sizeof_page;
    }

	/**
	 * Return the offset in the page for an object
	 */
    MMINLINE intptr_t pageOffset(omrobjectptr_t objectPtr) const
    {
        uintptr_t markIndex = (uintptr_t)objectPtr - _heapBase;
        return markIndex % sizeof_page;
    }

	/**
	 * Return the bit number in Compressed Mark Map slot responsible for this object
	 * Each bit in Compressed Mark Map represents twice more heap bytes then regular mark map
	 * Each page represents number of bytes in heap might be covered by one Compressed Mark Map slot (uintptr_t)
	 */
    MMINLINE intptr_t compressedPageOffset(omrobjectptr_t objectPtr) const
    {
        return pageOffset(objectPtr) / (2 * J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT);
    }

	/**
	 * Return the start of a page
	 */
    MMINLINE omrobjectptr_t pageStart(uintptr_t i) const
    {
        return (omrobjectptr_t) (_heapBase + (i * sizeof_page));
    }

	/**
	 * Return the next page
	 */
    MMINLINE omrobjectptr_t nextPage(omrobjectptr_t objectPtr) const
    {
        return pageStart(pageIndex(objectPtr)+1);
    }

    /**
     * If to is page-aligned, create one chunk (from:to).  Otherwise, create
     * two free chunks: one (from:to_aligned), and the other (to_aligned:to),
     * where to_aligned=ALIGN_DOWNWARDS(to,PAGE_SIZE).
     *
     */
    size_t setFreeChunkPageAligned(omrobjectptr_t from, omrobjectptr_t to);

    void rebuildMarkbits(MM_EnvironmentStandard *env);

    /**
     * Rebuild mark bits within the specified subArea
     *
     * @param env[in] the current thread
     * @param region[in] the region which contains the subArea
     * @param subAreaTable[in] an entry describing the subArea to be rebuilt
     */
	void rebuildMarkbitsInSubArea(MM_EnvironmentStandard *env, MM_HeapRegionDescriptorStandard *region, SubAreaEntry *subAreaTable, intptr_t i);

    /**
     * Atomically change the currentAction value of the subArea to the specified action. This allows
     * a worker thread to claim responsibility for performing the specified action on the specified
     * area.
     *
     * @param env[in] the current thread
     * @param entry[in] the subArea to change
     * @param newAction the desired action for the subArea
     *
     * @return true if the action was changed, or false if another thread already changed it to newAction
     */
    bool changeSubAreaAction(MM_EnvironmentBase *env, SubAreaEntry * entry, uintptr_t newAction);
public:
	static MM_CompactScheme *newInstance(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme);
	
	void kill(MM_EnvironmentBase *env);

    void workerSetupForGC(MM_EnvironmentStandard *env, bool singleThreaded);
	void masterSetupForGC(MM_EnvironmentStandard *env);
    virtual void compact(MM_EnvironmentBase *env, bool rebuildMarkBits, bool aggressive);
    omrobjectptr_t getForwardingPtr(omrobjectptr_t objectPtr) const;
	void flushPool(MM_EnvironmentStandard *env, MM_CompactMemoryPoolState *freeListState);
	void fixHeapForWalk(MM_EnvironmentBase *env);
	void parallelFixHeapForWalk(MM_EnvironmentBase *env);

	/**
	 * Perform fixup for a single object slot
	 * @param slotObject pointer to slotObject for fixup
	 */
	void fixupObjectSlot(GC_SlotObject* slotObject);
	
	MMINLINE void setMarkMap(MM_MarkMap *markMap) {	_markMap = markMap;}

	/**
	 * Create a CompactScheme object.
	 */
    MM_CompactScheme(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme)
        : MM_BaseVirtual()
    	, _omrVM(env->getOmrVM())
        , _extensions(env->getExtensions())
        , _dispatcher(_extensions->dispatcher)
        , _markingScheme(markingScheme)
        , _markMap(markingScheme->getMarkMap())
        , _subAreaTableSize(0)
    	, _subAreaTable(NULL)
    {
    	_typeId = __FUNCTION__;
    }
};

#endif /* COMPACTSCHEMEBASE_HPP_ */
