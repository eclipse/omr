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

#if !defined(HEAPREGIONDESCRIPTOR_HPP)
#define HEAPREGIONDESCRIPTOR_HPP

#include "omrcfg.h"
#include "omrcomp.h"
#include "ModronAssertions.h"
#include "j9nongenerated.h"

#include "BaseVirtual.hpp"

#include "MemorySubSpace.hpp"

class MM_MemoryPool;
class MM_MemorySubSpace;
class MM_HeapRegionManager;
class MM_EnvironmentBase;
/**
 * Base abstract class for heap region descriptors.
 * For Tarok, the heap is divided into a number of regions, each with potentially many different properties
 * (e.g., object size class, node affinity, etc).  The heap region descriptor is found in the heap region table
 * which describes a single region.
 */
class MM_HeapRegionDescriptor : public MM_BaseVirtual
{
friend class MM_HeapRegionManager;
friend class MM_HeapRegionManagerTarok;
friend class MM_MemorySubSpace;
public:
	enum RegionType {
		RESERVED = 0, 				/**< Represents regions that are currently uncommited */
		FREE = 1,  					/**< Represents regions that are completely free */
		SEGREGATED_SMALL = 2,		/**< Represents regions that contain objects */
		SEGREGATED_LARGE = 3,		/**< Represents spanning regions that contain objects */
		ARRAYLET_LEAF = 4,			/**< Represents regions that contain arraylet leaf's */
		ADDRESS_ORDERED = 5,		/**< Represents regions that have an AOL MemoryPool */
		ADDRESS_ORDERED_IDLE = 6,	/**< Represents regions that have an AOL MemoryPool but are not actively being used (that is, they can be made into other kinds of regions without penalty) */
		ADDRESS_ORDERED_MARKED = 7,	/**< Represents regions that have an AOL MemoryPool and an accurate mark set in extensions->previousMarkMap */
		BUMP_ALLOCATED = 8,		/**< Represents regions that have a BumpPointer MemoryPool */
		BUMP_ALLOCATED_IDLE = 9,	/**< Represents regions that have a BumpPointer MemoryPool but are not actively being used (that is, they can be made into other kinds of regions without penalty) */
		BUMP_ALLOCATED_MARKED = 10,	/**< Represents regions that have a BumpPointer MemoryPool and an accurate mark set in extensions->previousMarkMap */
		LAST_REGION_TYPE  /**< This dummy type is sed later as a size of an array for stats of region counts */
	};
	uintptr_t _regionsInSpan; /**< most likely a temporary piece of data which, along with _nextInSet, builds a freelist (public for assertions) */
	MM_HeapRegionDescriptor *_headOfSpan; /**< in a spanning region, points to the HEAD of the span.  In non-spanning, points at this */

	/**
	 * The values of the members in this enum will be ORed into @ref _regionProperties
	 * as flags they should therefore be powers of 2 which don't exceed the range
	 * of @ref _regionProperties
	 */
	enum RegionProperties {
		ALL = 0xFFFFFFFF
		,MANAGED = 1
	};

protected:

private:
	void *_lowAddress;  /**< low address of the region (inclusive) */
	void *_highAddress;  /**< high address of the region (exclusive) */

	MM_HeapRegionDescriptor *_previousRegion; /**< previous region descriptor in the list (used by HeapRegionManager) */
	MM_HeapRegionDescriptor *_nextRegion; /**< next region descriptor in the list (used by HeapRegionManager) */
	
	MM_HeapRegionDescriptor *_previousRegionInSubSpace; /**< previous region descriptor in the list (used by MM_MemorySubSpaceGeneric) */
	MM_HeapRegionDescriptor *_nextRegionInSubSpace; /**< next region descriptor in the list (used by MM_MemorySubSpaceGeneric) */

	MM_HeapRegionDescriptor *_nextInSet; /**< most likely a temporary link which is used for passing around ad-hoc "collections" of regions */
	bool _isAllocated; /**< This will become an enumeration of more interesting region states once we have them */

	MM_MemorySubSpace *_memorySubSpace; /**< the memory subspace associated with this region */
	
	RegionType _regionType; /**< This regions current type */
	MM_MemoryPool *_memoryPool; /**< The memory pool associated with this region.  This may be NULL */

	uintptr_t _numaNode; /**< The NUMA node this region is associated with */
	
	uint32_t _regionProperties; /**< A bitmap of the RegionProperties this region possesses */

public:
	MM_HeapRegionDescriptor(MM_EnvironmentBase *env, void *lowAddress, void *highAddress);
	virtual void associateWithSubSpace(MM_MemorySubSpace *subSpace);
	virtual void disassociateWithSubSpace();

	/**
	 * Set the specified property in the @ref _regionProperties bitmap
	 * @param[in] property The RegionProperties property to set
	 */
	MMINLINE void setRegionProperty(RegionProperties property) { _regionProperties |= property; }

	/**
	 * Clear the specified property from the @ref _regionProperties bitmap
	 * @param[in] property The RegionProperties property to clear
	 */
	MMINLINE void clearRegionProperty(RegionProperties property) { _regionProperties &= ~property; }

	/**
	 * Get this region's @ref _regionProperties
	 *
	 * @return This region's @ref _regionProperties
	 */
	MMINLINE uint32_t getRegionProperties() { return _regionProperties; }

	/**
	 * Test whether or not a certain property is set in the @ref _regionProperties bitmap
	 * @param[in] property The RegionProperties property to test
	 *
	 * @return True if the property is set, false otherwise
	 */
	MMINLINE bool regionPropertyIsSet(RegionProperties property) { return (0 != (getRegionProperties() & property)); }

	/**
	 * Get the low address of this region
	 *
	 * @return the lowest address in the region
	 */
	void*
	getLowAddress() const
	{
		void *low = _lowAddress;
		return low;
	}

	/**
	 * Get the high address of this region
	 *
	 * @return the first address beyond the end of the region
	 */
	void*
	getHighAddress() const
	{
		void *result = NULL;
		if (0 == _regionsInSpan) {
			result = _highAddress;
		} else {
			void *low = _lowAddress;
			void *high = _highAddress;
			uintptr_t delta = (uintptr_t)high - (uintptr_t)low;
			result = (void *)((uintptr_t)low + _regionsInSpan * delta);
		}
		/* Note that we can't assert that this is the same high address as our root backing since, when a spanning
		 * region is being released, a root backing becomes temporarily out of sync with a descriptor built on it
		 */
		return result;
	}

	/**
	 * Determine if the specified address is in the region
	 * @parm address - the address to test
	 * @return true if address is within the receiver, false otherwise
	 */
	bool
	isAddressInRegion(const void* address)
	{
		return (address >= getLowAddress()) && (address < getHighAddress());
	}

	/**
	 * Returns the MM_MemorySubSpace associated with the receiver.
	 *
	 * @return The MM_MemorySubSpace associated with the receiver
	 */
	MM_MemorySubSpace *
	getSubSpace()
	{
		return _memorySubSpace;
	}

	/**
	 * A helper to request the type flags from the Subspace associated with the receiving region.
	 *
	 * @return The type flags of the Subspace associated with this region
	 */
	uintptr_t
	getTypeFlags()
	{
		return MEMORY_TYPE_RAM | getSubSpace()->getTypeFlags();
	}

	/**
	 * @return The number of contiguous bytes represented by the receiver
	 */
	uintptr_t
	getSize() const
	{
		return (uintptr_t)getHighAddress() - (uintptr_t)getLowAddress();
	}
	
	void
	setRegionType(RegionType type)
	{
		_regionType = type;
	}
	
	RegionType
	getRegionType()
	{
		return _regionType;
	}
	
	void
	setMemoryPool(MM_MemoryPool *memoryPool)
	{
		_memoryPool = memoryPool;
	}
	
	MM_MemoryPool*
	getMemoryPool()
	{
		return _memoryPool;
	}

	/**
	 * @return True if the memory backing this region is committed
	 */
	bool
	isCommitted()
	{
		/* currently, we only check to see if we have set a subspace but this will eventually be a meta-data flag */
		return (NULL != getSubSpace());
	}

	/**
	 * Set the NUMA node the memory in the region is associated with
	 */
	void
	setNumaNode(uintptr_t numaNode) {
		_numaNode = numaNode;
	}

	/**
	 * @return the NUMA node the memory in the region is associated with
	 */
	uintptr_t
	getNumaNode() {
		return _numaNode;
	}
	
	/**
	 * @return true if the region is a type which can contain objects
	 */
	bool
	containsObjects()
	{
		bool result = false;
		switch (getRegionType()) {
		case SEGREGATED_SMALL:
		case SEGREGATED_LARGE:
		case ADDRESS_ORDERED:
		case ADDRESS_ORDERED_MARKED:
		case BUMP_ALLOCATED:
		case BUMP_ALLOCATED_MARKED:
			result = true;
			break;
		default:
			/* all other types do not contain objects */
			result = false;
			break;
		}
		return result;
	}

	/**
	 * @return true if the region has an up-to-date mark map
	 */
	bool
	hasValidMarkMap()
	{
		bool result = false;
		switch (getRegionType()) {
		case ADDRESS_ORDERED_MARKED:
		case BUMP_ALLOCATED_MARKED:
			result = true;
			break;
		default:
			/* all other types do not have valid mark maps */ 
			result = false;
			break;
		}
		return result;
	}
	
	/**
	 * @return true if the region is free or idle
	 */
	bool
	isFreeOrIdle()
	{
		bool result = false;
		switch (getRegionType()) {
		case ADDRESS_ORDERED_IDLE:
		case BUMP_ALLOCATED_IDLE:
		case FREE:
			result = true;
			break;
		default:
			/* all other types are not free */ 
			result = false;
			break;
		}
		return result;
	}
	
	/**
	 * Sets a BUMP_ALLOCATED region to BUMP_ALLOCATED_MARKED.  Asserts if called on any other region type.
	 * This can be extended to other region types which have a "marked" variant as they are needed.
	 */
	void setMarkMapValid()
	{
		Assert_MM_true(BUMP_ALLOCATED == getRegionType());
		setRegionType(BUMP_ALLOCATED_MARKED);
	}
	
	/**
	 * Allocate supporting resources (large enough to justify not to preallocate them for all regions at the startup) when region is being committed.
	 * @param env[in] of a GC thread
	 * @return true if allocation is successful
	 */
	virtual bool allocateSupportingResources(MM_EnvironmentBase *env) { return true; }

	virtual bool check(MM_EnvironmentBase *env) { return true; }

	bool initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager);
	void tearDown(MM_EnvironmentBase *env);
	
	static bool initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress);
	static void destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor);
	
protected:
	void reinitialize(MM_EnvironmentBase *env, void *lowAddress, void *highAddress);
private:

};

#endif /* HEAPREGIONDESCRIPTOR_HPP */
