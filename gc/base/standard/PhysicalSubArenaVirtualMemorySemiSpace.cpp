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

#include "PhysicalSubArenaVirtualMemorySemiSpace.hpp"

#include "omrcfg.h"
#include "omrgcconsts.h"
#include "ModronAssertions.h"

#include <string.h>

#include "ContractslotScanner.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionManager.hpp"
#include "HeapWalker.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "ParallelGlobalGC.hpp"
#include "PhysicalArena.hpp"
#include "PhysicalArenaVirtualMemory.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

/**
 * Zos390 Platform dependent trickery for the ar command.  Without this definition
 * the ar command on Zos390 will fail to link this class.
 */
#if defined(J9ZOS390)
#include "ZOSLinkage.hpp"
int j9zos390LinkTrickPhysicalSubArenaVirtualMemorySemiSpace; 
#endif /* J9ZOS390 */

/**
 * Initialization
 */
MM_PhysicalSubArenaVirtualMemorySemiSpace *
MM_PhysicalSubArenaVirtualMemorySemiSpace::newInstance(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_PhysicalSubArenaVirtualMemorySemiSpace *subArena;
	
	subArena = (MM_PhysicalSubArenaVirtualMemorySemiSpace *)env->getForge()->allocate(sizeof(MM_PhysicalSubArenaVirtualMemorySemiSpace), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(subArena) {
		new(subArena) MM_PhysicalSubArenaVirtualMemorySemiSpace(heap);
		if(!subArena->initialize(env)) {
			subArena->kill(env);
			return NULL;
		}
	}
	return subArena;
}

/**
 * Destroy and delete the instance.
 */
void
MM_PhysicalSubArenaVirtualMemorySemiSpace::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the instances internal structures.
 * @return true if the initialization was successful, false otherwise.
 */
bool
MM_PhysicalSubArenaVirtualMemorySemiSpace::initialize(MM_EnvironmentBase *env)
{
	if(!MM_PhysicalSubArenaVirtualMemory::initialize(env)) {
		return false;
	}
	
	_resizable = env->getExtensions()->dynamicNewSpaceSizing;
	_avoidMovingObjects = env->getExtensions()->dnssAvoidMovingObjects;

	return true;
}

/**
 * Destroy and delete the instances internal structures.
 */
void
MM_PhysicalSubArenaVirtualMemorySemiSpace::tearDown(MM_EnvironmentBase *env)
{
	void *lowValidAddress = NULL;
	void *highValidAddress = NULL;
	
	if (NULL != _lowSemiSpaceRegion) {
		lowValidAddress = _lowSemiSpaceRegion->getLowAddress();
		getHeapRegionManager()->destroyAuxiliaryRegionDescriptor(env, _lowSemiSpaceRegion);
		_lowSemiSpaceRegion = NULL;
	}
	if (NULL != _highSemiSpaceRegion) {
		highValidAddress = _highSemiSpaceRegion->getHighAddress();
		getHeapRegionManager()->destroyAuxiliaryRegionDescriptor(env, _highSemiSpaceRegion);
		_highSemiSpaceRegion = NULL;
	}
	
	/* Remove the heap range represented */
	if (NULL != _subSpace) {
		_subSpace->heapRemoveRange(env, _subSpace, ((uintptr_t)_highAddress) - ((uintptr_t)_lowAddress), _lowAddress, _highAddress, lowValidAddress, highValidAddress);
		_subSpace->heapReconfigured(env);
	}
	MM_PhysicalSubArenaVirtualMemory::tearDown(env);
}
	
/**
 * Reserve initial address space within the parent arena.
 * @return true on successful reserve, false otherwise.
 */
bool
MM_PhysicalSubArenaVirtualMemorySemiSpace::inflate(MM_EnvironmentBase *env)
{
	uintptr_t attachPolicy;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	
	if (extensions->enableSplitHeap) {
		/* split heaps require that we allocate new space from the top or it might fall into the gap */
		attachPolicy = modron_pavm_attach_policy_high_memory;
	} else {
		if(extensions->dynamicNewSpaceSizing) {
			attachPolicy = modron_pavm_attach_policy_high_memory;
		} else {
			attachPolicy = modron_pavm_attach_policy_none;
		}
	}
	if(_parent->attachSubArena(env, this, _subSpace->getInitialSize(), attachPolicy)) {
		/* Inflation successful - inform the owning memorySubSpaces semi-spaces */
		MM_MemorySubSpace *subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();
		MM_MemorySubSpace *subSpaceSurvivor = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceSurvivor();

		/* Split the memory into 2 equal sized halves - one for allocate, and one for survivor */
		uintptr_t size = calculateOffsetToHighAddress(_lowAddress);
		uintptr_t semiSpaceSize = size / 2;
		void *semiSpaceMiddle = (void *)(((uintptr_t)_lowAddress) + semiSpaceSize);
	
		Assert_MM_true(size == (semiSpaceSize * 2));

		/* Add the spaces as table-backed heap regions */
		if(NULL == (_highSemiSpaceRegion = getHeapRegionManager()->createAuxiliaryRegionDescriptor(env, subSpaceSurvivor, semiSpaceMiddle, _highAddress))) {
			return false;
		}
		
		if (0 != _numaNode) {
			_highSemiSpaceRegion->setNumaNode(_numaNode);
		}

		Assert_MM_true(_highSemiSpaceRegion->getLowAddress() == semiSpaceMiddle);
		Assert_MM_true(_highSemiSpaceRegion->getHighAddress() == _highAddress);
		if ((_highSemiSpaceRegion->getLowAddress() != semiSpaceMiddle) || (_highSemiSpaceRegion->getHighAddress() != _highAddress)) {
			omrtty_printf(
					"!!! Fatal Error in MM_PhysicalSubArenaVirtualMemorySemiSpace::inflate - bad address range (%p,%p) for highSemiSpaceRegion, expected to be (%p, %p)\n",
					_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress(),
					semiSpaceMiddle, _highAddress);
			Assert_MM_inflateInvalidRange();
		}
		if(NULL == (_lowSemiSpaceRegion = getHeapRegionManager()->createAuxiliaryRegionDescriptor(env, subSpaceAllocate, _lowAddress, semiSpaceMiddle))) {
			return false;
		}

		if (0 != _numaNode) {
			_lowSemiSpaceRegion->setNumaNode(_numaNode);
		}

		Assert_MM_true(_lowSemiSpaceRegion->getLowAddress() == _lowAddress);
		Assert_MM_true(_lowSemiSpaceRegion->getHighAddress() == semiSpaceMiddle);
		if ((_lowSemiSpaceRegion->getLowAddress() != _lowAddress) || (_lowSemiSpaceRegion->getHighAddress() != semiSpaceMiddle)) {
			omrtty_printf(
					"!!! Fatal Error in MM_PhysicalSubArenaVirtualMemorySemiSpace::inflate - bad address range (%p,%p) for lowSemiSpaceRegion, expected to be (%p, %p)\n",
					_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(),
					_lowAddress, semiSpaceMiddle);
			Assert_MM_inflateInvalidRange();
		}

		/* Inform the semi spaces that they have been expanded */
		bool result = subSpaceAllocate->expanded(env, this, _lowSemiSpaceRegion->getSize(), _lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(), false);
		subSpaceAllocate->heapReconfigured(env);
		result = result && subSpaceSurvivor->expanded(env, this, _highSemiSpaceRegion->getSize(), _highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress(), false);
		subSpaceSurvivor->heapReconfigured(env);
		return result;
	}
	return false;
}

/**
 * Expand the physical arena by the parameters in the description.
 * 
 * @return The number of bytes the arena has expanded by.
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemorySemiSpace::expand(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	uintptr_t availableSize, totalExpandSize;
	uintptr_t heapAlignment;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	bool debug = extensions->debugDynamicNewSpaceSizing;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	assume0(expandSize == MM_Math::roundToCeiling(MM_GCExtensions::getExtensions(env)->heapAlignment, expandSize));  /* expand requests must be aligned */

	if(debug) {
		omrtty_printf("New space expand:\n");
	}

	heapAlignment = extensions->heapAlignment;

	totalExpandSize = expandSize;

	uintptr_t maximumExpandSize;
	maximumExpandSize = MM_Math::roundToFloor(heapAlignment * 2, _subSpace->getMaximumSize() - _subSpace->getCurrentSize());
	if(totalExpandSize > maximumExpandSize) {
		totalExpandSize = maximumExpandSize;
	}
	totalExpandSize = MM_Math::roundToFloor(heapAlignment * 2, totalExpandSize);

	/* Check if the requested expand size fits into the current arena */
	if(!_subSpace->canExpand(env, totalExpandSize)) {
		return 0;
	}

	/* Check if the new address range for expansion even makes sense */
	if(totalExpandSize >= (uintptr_t)_lowAddress) {
		/* Not enough room between the bottom of memory and current low address for expansion - adjust */
		totalExpandSize = MM_Math::roundToFloor(heapAlignment * 2, (uintptr_t)_lowAddress);
	}

	/* Adjust the total expand size to be no greater than the address space between the semi spaces
	 * and the previous sub space beside it (if any)
	 */
	if(NULL != _lowArena) {
		uintptr_t spaceAvailable;
		spaceAvailable = ((uintptr_t)_lowAddress) - ((uintptr_t)_lowArena->getHighAddress());
		if(spaceAvailable < totalExpandSize) {
			totalExpandSize = MM_Math::roundToFloor(heapAlignment * 2, spaceAvailable);
		}
	}
	totalExpandSize = MM_Math::roundToFloor(2*getHeapRegionManager()->getRegionSize(), totalExpandSize);

	/* Ask the governing physical arena (associated to the memory space) if the expand request is ok */
	if(!((MM_PhysicalArenaVirtualMemory *)_parent)->canExpand(env, this, (void *)(((uintptr_t)_lowAddress) - totalExpandSize), totalExpandSize)) {
		/* Parent is responsible for setting the expand error code */
		return 0;
	}

	/* Find the amount of free space available between the receiver arena and the neighbouring lower arena */
	if(NULL != _lowArena) {
		availableSize = ((uintptr_t)_lowAddress) - ((uintptr_t)_lowArena->getHighAddress());
	} else {
		availableSize = ((uintptr_t)_lowAddress) - ((uintptr_t)((MM_PhysicalArenaVirtualMemory *)_parent)->getLowAddress());
	}
	totalExpandSize = (totalExpandSize > availableSize) ? availableSize : totalExpandSize;
	assume0((totalExpandSize % heapAlignment) == 0);  // both addresses should be heap alignment safe
	Assert_MM_true(totalExpandSize == MM_Math::roundToCeiling(2*getHeapRegionManager()->getRegionSize(), totalExpandSize));

	if(debug) {
		omrtty_printf("\tadjusted expand size: %p\n", totalExpandSize);
	}

	return expandNoCheck(env, totalExpandSize);
}

/**
 * User data structure for heap move during semi space contract.
 */
struct Modron_psavmssMoveData {
	MM_EnvironmentBase *env;
	void *srcBase;
	void *srcTop;
	void *dstBase;
};

/**
 * Walk function for all slots iterator to adjust pointers during a semi space contract.
 * 
 * @param objectIndirect Slot potentially containing a pointer to process.
 * @param localData User supplied data to the slot walker (contains relocation information).
 */
static void
psavmssMoveFunction(OMR_VM *omrVM, omrobjectptr_t *objectIndirect, void *localData, uint32_t isDerivedPointer)
{
	struct Modron_psavmssMoveData *userData = (struct Modron_psavmssMoveData *)localData;
	omrobjectptr_t objectPtr;

	objectPtr = *objectIndirect;
	if(NULL != objectPtr) {
		if((objectPtr >= (omrobjectptr_t)userData->srcBase) && (objectPtr < (omrobjectptr_t)userData->srcTop)) {
			objectPtr = (omrobjectptr_t)((((uintptr_t)objectPtr) - ((uintptr_t)userData->srcBase)) + ((uintptr_t)userData->dstBase));
			*objectIndirect = objectPtr;
		}
	}
}


/**
 * Contract the sub arenas physical memory by at least the size specified.
 * 
 * @return The amount of heap actually contracted.
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemorySemiSpace::contract(MM_EnvironmentBase *env, uintptr_t contractSize)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	bool debug = extensions->debugDynamicNewSpaceSizing;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uintptr_t heapAlignment = extensions->heapAlignment;
	uintptr_t effectiveAlignment = heapAlignment;
	effectiveAlignment = MM_Math::roundToCeiling(getHeapRegionManager()->getRegionSize(), effectiveAlignment);
	/* this code assumes that the region size is a multiple of the heap alignment so ensure that this is true */
	Assert_MM_true(0 == (effectiveAlignment % heapAlignment));
	Assert_MM_true(contractSize == MM_Math::roundToCeiling(effectiveAlignment, contractSize));  /* expand requests must be aligned */

	Assert_MM_true(_lowAddress == _lowSemiSpaceRegion->getLowAddress());
	Assert_MM_true(_highAddress == _highSemiSpaceRegion->getHighAddress());

	if(debug) {
		omrtty_printf("New space contract:\n");
	}

	uintptr_t survivorSpaceRatio = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getSurvivorSpaceSizeRatio();

	/* Sanity check */
	if(0 == contractSize) {
		return 0;
	}

	/* Calculate the actual contract size used */
	uintptr_t totalContractSize = contractSize;

	/* Make sure we do not contract beyond any maximum.  We align the total contraction size to 2x effectiveAlignment
	 * for when we are splitting up any remaining free space (above and beyond what is minimally required) so that
	 * splitting/alignment math works out.
	 */
	uintptr_t maximumContractSize = MM_Math::roundToFloor(effectiveAlignment * 2, _subSpace->getCurrentSize() - _subSpace->getMinimumSize());
	if(totalContractSize > maximumContractSize) {
		totalContractSize = maximumContractSize;
	}
	totalContractSize = MM_Math::roundToFloor(effectiveAlignment * 2, totalContractSize);
	
	/* Sanity check */
	if(0 == totalContractSize) {
		return 0;
	}

	/* Find where the allocate subspace is */	
	MM_MemorySubSpace *subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();
	MM_MemorySubSpace *subSpaceSurvivor = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceSurvivor();

	if(debug) {
		omrtty_printf("\tlowseg:(%p %p) highseg:(%p %p)\n",
			_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(),
			_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
	}

	MM_HeapRegionDescriptor *allocateRegion = NULL;
	MM_HeapRegionDescriptor *survivorRegion = NULL;
	/* Find which segment is the allocate and survivor for some sanity checking /value calculations */
	if(subSpaceAllocate == _lowSemiSpaceRegion->getSubSpace()) {
		allocateRegion = _lowSemiSpaceRegion;
		survivorRegion = _highSemiSpaceRegion;
	} else {
		allocateRegion = _highSemiSpaceRegion;
		survivorRegion = _lowSemiSpaceRegion;
	}

	uintptr_t allocateLeadingFreeSize = 0;

	/* Determine the slack room in allocate space (also the free size) */
	void *allocateLeadingFreeBase = allocateRegion->getLowAddress();
	void *allocateLeadingFreeTop = subSpaceAllocate->findFreeEntryTopStartingAtAddr(env, allocateRegion->getLowAddress());
	if(NULL != allocateLeadingFreeTop) {
		allocateLeadingFreeSize = ((uintptr_t)allocateLeadingFreeTop) - ((uintptr_t)allocateLeadingFreeBase);
	} else {
		allocateLeadingFreeTop = allocateLeadingFreeBase;
		allocateLeadingFreeSize = 0;
	}
		
	void *allocateTrailingFreeBase = subSpaceAllocate->findFreeEntryEndingAtAddr(env, allocateRegion->getHighAddress());
	/* There may only be a single free entry */
	if (allocateTrailingFreeBase == allocateLeadingFreeBase) {
		allocateTrailingFreeBase = allocateRegion->getHighAddress();
	}
	uintptr_t allocateTrailingFreeSize = ((uintptr_t)allocateRegion->getHighAddress()) - ((uintptr_t)allocateTrailingFreeBase);

	/* Record the used low and high ranges of the allocate space */
	uint8_t *allocateSpaceUsedHeapTop = (uint8_t *)allocateTrailingFreeBase;

	if(NULL == allocateTrailingFreeBase) {
		return 0;
	}

	if(debug) {
		omrtty_printf("\tSurrounding free in allocate (%p %p) (%p %p)\n",
			allocateLeadingFreeBase, allocateLeadingFreeTop,
			allocateTrailingFreeBase, ((uintptr_t)allocateTrailingFreeBase) + allocateTrailingFreeSize);
	}

	/* Find the amount of heap that will be moved */
	uintptr_t heapSizeToMove = ((uintptr_t)allocateTrailingFreeBase) - ((uintptr_t)allocateLeadingFreeTop);
	if(debug) {
		omrtty_printf("\tValid allocate heap (%p %p) size %p\n",
			allocateLeadingFreeTop, allocateTrailingFreeBase, heapSizeToMove);
	}

	/* Determine the total free size available to contract */
	uintptr_t totalFreeSize = survivorRegion->getSize();
	totalFreeSize += allocateTrailingFreeSize;
	totalFreeSize += allocateLeadingFreeSize;

	/* Calculate the aligned minimum size required for the allocate space */
	uintptr_t allocateSizeConsumed = MM_Math::roundToCeiling(effectiveAlignment, heapSizeToMove);

	/* Based on ratios and the minimum sized allocate space, calculate the minimum sized survivor space required */
	/* The calculation is based on the formula,
	 *    X = AllocateSpaceSize + ((N / 100) * X)
	 * where,
	 *    X is the total size of the allocate and survivor spaces.
	 *    N is the minimum free size ratio (over 100) that the survivor space must be of total semispace memory.
	 * The formula solves for X as,
	 *    X = (100 / (100 - N)) * AllocateSpaceSize
	 * NOTE: we round to twice the heap alignment as we are dividing the total memory into two separate segments.
	 * NOTE: If tilted scavenging is not enabled, we know the split is 50%.
	 */
	uintptr_t survivorSizeConsumed = 0;
	if (extensions->tiltedScavenge) {
		uintptr_t minimumTotalSize;
		minimumTotalSize = (allocateSizeConsumed / (100 - survivorSpaceRatio)) * 100;
		minimumTotalSize = MM_Math::roundToCeiling(effectiveAlignment * 2, minimumTotalSize);
	
		/* Record the survivor size */
		survivorSizeConsumed = minimumTotalSize - allocateSizeConsumed;
	} else {
		survivorSizeConsumed = allocateSizeConsumed;
	}

	/* Check if the free size required fits satisfies the minimum free size.
	 * This check makes sure that the current amount of free size in the system is at least as large as
	 * what is the expected free size available.  This may seem confusing, but what it is really doing is
	 * making sure that, given the tilt ratio, there is an expected amount of free memory.  Make sure that
	 * if the current semispace actual tilt is far from its expected tilt, that we do not deviate further
	 * from this by getting rid of even more free space.
	 */
	if(totalFreeSize < (survivorSizeConsumed + (allocateSizeConsumed - heapSizeToMove))) {
		return 0;
	}

	/* Get the remaining free size that is not part of the minimu requirement.  This will be the largest
	 * contract value that can be supported for this round.
	 */
	uintptr_t remainingFreeSize = totalFreeSize - (survivorSizeConsumed + (allocateSizeConsumed - heapSizeToMove));

	/* We have a contract size calculated - adjust to the actual remaining free bytes and total contract size
	 * appropriately
	 */
	if(totalContractSize > remainingFreeSize) {
		totalContractSize = MM_Math::roundToFloor(effectiveAlignment * 2, remainingFreeSize);
	}

	/* Sanity check (the contract size should be heap aligned based on all numbers involved in the
	 * calculation being heap aligned
	 */
	if (0 == totalContractSize) {
		return 0;
	}

	/* Record the virtual addresses for the contraction for counter balancing purposes */
	setVirtualLowAddress((void *)(((uint8_t *)_lowAddress) + totalContractSize));

	/* Inform the subspace lineage that a contract will occur, and to react accordingly.  this may
	 * adjust the size of the contract.
	 */
	totalContractSize = _subSpace->counterBalanceContract(env, totalContractSize, 2 * effectiveAlignment);
	Assert_MM_true(0 == (totalContractSize % (2 * effectiveAlignment)));

	/* Clear the virtual addresses used for counter balancing */
	clearVirtualAddresses();

	/* If the contract size was reduced to 0, we cannot contract */
	if(0 == totalContractSize) {
		_subSpace->clearEnqueuedCounterBalancing(env);
		return 0;
	}

	remainingFreeSize -= totalContractSize;
	Assert_MM_true(0 == (remainingFreeSize % effectiveAlignment));  // only guaranteed to be heap aligned

	/* Split the remaining free size between the allocate and survivor spaces.
	 * Apply the formula described earlier for the split.
	 * NOTE: If tiled scavening is not enabled, we know the split is 50%
	 * NOTE: Remaining free size is NOT effectiveAlignment*2, so we must redistribute the heap
	 * using size differences, instead of using the ratios to calculate both
	 */
	if (extensions->tiltedScavenge) {
		uintptr_t remainingFreeSizeForAllocate = MM_Math::roundToCeiling(effectiveAlignment, (remainingFreeSize / 100) * (100 - survivorSpaceRatio));
		allocateSizeConsumed += remainingFreeSizeForAllocate;
		survivorSizeConsumed += (remainingFreeSize - remainingFreeSizeForAllocate);
	} else {
		uintptr_t remainingFreeSizeForAllocate = MM_Math::roundToCeiling(effectiveAlignment, remainingFreeSize / 2);
		allocateSizeConsumed += remainingFreeSizeForAllocate;
		survivorSizeConsumed += (remainingFreeSize - remainingFreeSizeForAllocate);
	}

	Assert_MM_true((allocateSizeConsumed % effectiveAlignment) == 0);
	Assert_MM_true((survivorSizeConsumed % effectiveAlignment) == 0);
	
	/* Perform actual contract operation (high/low specific code) */
	if(subSpaceAllocate == _lowSemiSpaceRegion->getSubSpace()) {
		if(debug) {
			omrtty_printf("\tlowseg=allocate highseg=survivor\n");
		}

		/* allocate is low region */
		/* survivor is high region */
		/* Find the new ranges for the allocate and survivor segments */
		uint8_t *survivorSegmentBase = ((uint8_t *)_highSemiSpaceRegion->getHighAddress()) - survivorSizeConsumed;
		uint8_t *survivorSegmentTop = (uint8_t *)_highSemiSpaceRegion->getHighAddress();

		uint8_t *allocateSegmentBase = survivorSegmentBase - allocateSizeConsumed;
		uint8_t *allocateSegmentTop = survivorSegmentBase;

		/* Record the memory range that will be removed */
		void *removeMemoryBase = _lowSemiSpaceRegion->getLowAddress();
		void *removeMemoryTop = (void *)allocateSegmentBase;
		uintptr_t removeMemorySize = ((uintptr_t)removeMemoryTop) - ((uintptr_t)removeMemoryBase);

		if(debug) {
			omrtty_printf("\tadjusted allocate (%p %p) survivor (%p %p)\n",
				allocateSegmentBase, allocateSegmentTop,
				survivorSegmentBase, survivorSegmentTop);
			omrtty_printf("\tRemove range calculated as (%p %p)\n", removeMemoryBase, removeMemoryTop);
		}

		/* Check if the move location actually does in fact move any valid heap */
		if(allocateSegmentBase > allocateLeadingFreeTop) {
			/* Adjust all fields that point to the refered to region */
			struct Modron_psavmssMoveData psavmssMoveData;
			psavmssMoveData.env = env;
			psavmssMoveData.srcBase = (void *)allocateLeadingFreeTop;
			psavmssMoveData.srcTop = (void *)(((uintptr_t)allocateLeadingFreeTop) + heapSizeToMove);
			psavmssMoveData.dstBase = (void *)allocateSegmentBase;

			/* TODO: can this walk be parallelized? Probably not trivially, since the mark map isn't up to date at this point */
			MM_HeapWalker *heapWalker = extensions->collectorLanguageInterface->parallelGlobalGC_getHeapWalker();
			heapWalker->allObjectSlotsDo(env, psavmssMoveFunction, (void *)&psavmssMoveData, J9_MU_WALK_ALL | J9_MU_WALK_NEW_AND_REMEMBERED_ONLY, false, true);
		}

		/* Remove the free ranges from allocate space */
		if (allocateLeadingFreeSize > 0) {
			subSpaceAllocate->removeExistingMemory(
				env,
				this,
				allocateLeadingFreeSize,
				allocateLeadingFreeBase,
				allocateLeadingFreeTop);
		}
		if (allocateTrailingFreeSize > 0) {
			subSpaceAllocate->removeExistingMemory(
				env,
				this,
				allocateTrailingFreeSize,
				(void *)allocateTrailingFreeBase,
				allocateRegion->getHighAddress());
		}

		/* Adjust the region boundaries */
		/* we are subtly assuming that allocate size is at least as large as survivor size (the converse makes no sense) so ensure this assumption is correct */
		Assert_MM_true(allocateSizeConsumed >= survivorSizeConsumed);
		/* the high semi-space is shrinking so move its low end */
		getHeapRegionManager()->resizeAuxillaryRegion(env, _highSemiSpaceRegion, survivorSegmentBase, survivorSegmentTop);
		getHeapRegionManager()->resizeAuxillaryRegion(env, _lowSemiSpaceRegion, allocateSegmentBase, allocateSegmentTop);

		/* Check if the move location actually does in fact move any valid heap */
		if(allocateSegmentBase > allocateLeadingFreeTop) {
			/* Broadcast a move of the allocate range */
			if(debug) {
				omrtty_printf("\tMoving heap (%p %p) to (%p %p)\n",
					allocateLeadingFreeTop, allocateTrailingFreeBase,
					allocateSegmentBase, ((uintptr_t)allocateSegmentBase) + heapSizeToMove);
			}
			subSpaceAllocate->moveHeap(env, allocateLeadingFreeTop, allocateTrailingFreeBase, allocateSegmentBase);
	
			/* Move memory (can be overlapping) */
			memmove((void *)allocateSegmentBase, allocateLeadingFreeTop, (size_t)heapSizeToMove);

			/* Now that the memory has been moved fix up the roots */
			MM_ContractSlotScanner contractSlotScanner(env, (void *)allocateLeadingFreeTop, (void *)(((uintptr_t)allocateLeadingFreeTop) + heapSizeToMove), (void *)allocateSegmentBase);
			contractSlotScanner.setIncludeStackFrameClassReferences(false);
			contractSlotScanner.scanAllSlots(env);

			/* Adjust the used heap ranges */
			allocateSpaceUsedHeapTop = ((uint8_t *)allocateSegmentBase) + heapSizeToMove;
		}

		/* Reset any free list information */
		subSpaceAllocate->reset();
		subSpaceSurvivor->reset();

		/* Was there any remaining leading free entry not consumed by the contraction? */
		if(allocateSegmentBase < allocateLeadingFreeTop) {
			/* Rebuild the allocate leading free list */
			if(debug) {
				omrtty_printf("\tAdd free range back (%p %p) size %p\n",
					(void *)allocateSegmentBase,
					allocateLeadingFreeTop,
					(void *)(((uintptr_t)allocateLeadingFreeTop) - ((uintptr_t)allocateSegmentBase)));
			}
			subSpaceAllocate->addExistingMemory(
				env,
				this,
				((uintptr_t)allocateLeadingFreeTop) - ((uintptr_t)allocateSegmentBase),
				(void *)allocateSegmentBase,
				allocateLeadingFreeTop,
				true);
		}

		/* Rebuild the allocate trailing free list */
		if(debug) {
			omrtty_printf("\tAdd free range back (%p %p) size %p\n",
				(void *)allocateSpaceUsedHeapTop,
				(void *)allocateSegmentTop,
				(uintptr_t)(allocateSegmentTop - (uintptr_t)allocateSpaceUsedHeapTop));
		}

		subSpaceAllocate->addExistingMemory(
			env,
			this,
			(uintptr_t)(allocateSegmentTop - (uintptr_t)allocateSpaceUsedHeapTop),
			(void *)allocateSpaceUsedHeapTop,
			(void *)allocateSegmentTop,
			true);

		/* Rebuild the survivor free list */
		subSpaceSurvivor->addExistingMemory(
			env,
			this,
			((uintptr_t)survivorSegmentTop) - ((uintptr_t)survivorSegmentBase),
			(void *)survivorSegmentBase,
			(void *)survivorSegmentTop,
			true);

		/* Find the previous valid address ahead of the area removed - used by removal broadcast and decommit */
		void *previousValidAddressNotRemoved = (NULL != _lowArena) ? _lowArena->getHighAddress() : NULL;
			
		/* Broadcast the contraction of the heap, but do not remove the memory from any free lists */
		if(debug) {
			omrtty_printf("\tRemove and decommit (%p %p) (valid %p %p)\n",
				removeMemoryBase,
				removeMemoryTop,
				previousValidAddressNotRemoved,
				(void *)allocateSegmentBase);
		}
		_subSpace->heapRemoveRange(
			env,
			_subSpace,
			removeMemorySize,
			removeMemoryBase,
			removeMemoryTop,
			previousValidAddressNotRemoved,
			(void *)allocateSegmentBase);
		_subSpace->heapReconfigured(env);

		/* Decommit the heap (the return value really doesn't matter here - its already too late) */
		_heap->decommitMemory(
			removeMemoryBase,
			removeMemorySize,
			previousValidAddressNotRemoved,
			(void *)allocateSegmentBase);
		
		/* Adjust sizing values within memory sub spaces */
		subSpaceAllocate->setCurrentSize(_lowSemiSpaceRegion->getSize());
		subSpaceSurvivor->setCurrentSize(_highSemiSpaceRegion->getSize());

		/* Set the low address of the arena */
		_lowAddress = (void *)allocateSegmentBase;
	} else {
		if(debug) {
			omrtty_printf("\tlowseg=survivor highseg=allocate\n");
		}

		/* allocate is high region */
		/* survivor is low region */
		/* Find the new ranges for the allocate and survivor segments */
		uint8_t *allocateSegmentBase = ((uint8_t *)_highSemiSpaceRegion->getHighAddress()) - allocateSizeConsumed;
		uint8_t *allocateSegmentTop = (uint8_t *)_highSemiSpaceRegion->getHighAddress();

		uint8_t *survivorSegmentBase = allocateSegmentBase - survivorSizeConsumed;
		uint8_t *survivorSegmentTop = allocateSegmentBase;

		/* Record the memory range that will be removed */
		void *removeMemoryBase = _lowSemiSpaceRegion->getLowAddress();
		void *removeMemoryTop = (void *)survivorSegmentBase;
		uintptr_t removeMemorySize = ((uintptr_t)removeMemoryTop) - ((uintptr_t)removeMemoryBase);

		if(debug) {
			omrtty_printf("\tadjusted survivor (%p %p) allocate (%p %p)\n",
				survivorSegmentBase, survivorSegmentTop,
				allocateSegmentBase, allocateSegmentTop);
			omrtty_printf("\tRemove range calculated as (%p %p)\n", removeMemoryBase, removeMemoryTop);
		}

		/* Check if the move location actually does in fact move any valid heap */
		if(allocateSegmentBase > allocateLeadingFreeTop) {
			/* Adjust all fields that point to the refered to region */
			struct Modron_psavmssMoveData psavmssMoveData;
			psavmssMoveData.env = env;
			psavmssMoveData.srcBase = allocateLeadingFreeTop;
			psavmssMoveData.srcTop = (void *)(((uintptr_t)allocateLeadingFreeTop)+ heapSizeToMove);
			psavmssMoveData.dstBase = (void *)allocateSegmentBase;

			/* TODO: can this walk be parallelized? Probably not trivially, since the mark map isn't up to date at this point */
			MM_HeapWalker *heapWalker = env->getExtensions()->collectorLanguageInterface->parallelGlobalGC_getHeapWalker();
			heapWalker->allObjectSlotsDo(env, psavmssMoveFunction, (void *)&psavmssMoveData, J9_MU_WALK_ALL | J9_MU_WALK_NEW_AND_REMEMBERED_ONLY, false, true);
		}

		/* Remove the free ranges from allocate space */
		if(allocateLeadingFreeSize > 0) {
			subSpaceAllocate->removeExistingMemory(
				env,
				this,
				allocateLeadingFreeSize,
				allocateLeadingFreeBase,
				allocateLeadingFreeTop);
		}
		if (allocateTrailingFreeSize > 0) {
			subSpaceAllocate->removeExistingMemory(
				env,
				this,
				allocateTrailingFreeSize,
				(void *)allocateTrailingFreeBase,
				allocateRegion->getHighAddress());
		}

		/* Adjust the region boundaries */
		/* we are subtly assuming that allocate size is at least as large as survivor size (the converse makes no sense) so ensure this assumption is correct */
		Assert_MM_true(allocateSizeConsumed >= survivorSizeConsumed);
		getHeapRegionManager()->resizeAuxillaryRegion(env, _highSemiSpaceRegion, allocateSegmentBase, allocateSegmentTop);
		getHeapRegionManager()->resizeAuxillaryRegion(env, _lowSemiSpaceRegion, survivorSegmentBase, survivorSegmentTop);

		/* Check if the move location actually does in fact move any valid heap */
		if(allocateSegmentBase > allocateLeadingFreeTop) {
			/* Broadcast a move of the allocate range */
			if(debug) {
				omrtty_printf("\tMoving heap (%p %p) to (%p %p)\n",
					allocateLeadingFreeTop, allocateTrailingFreeBase,
					allocateSegmentBase, ((uintptr_t)allocateSegmentBase) + heapSizeToMove);
			}
			subSpaceAllocate->moveHeap(env, allocateLeadingFreeTop, allocateTrailingFreeBase, allocateSegmentBase);
	
			/* Move memory (can be overlapping) */
			memmove((void *)allocateSegmentBase, allocateLeadingFreeTop, (size_t)heapSizeToMove);

			/* Now that the memory has been moved fix up the roots */
			MM_ContractSlotScanner contractSlotScanner(env, (void *)allocateLeadingFreeTop, (void *)(((uintptr_t)allocateLeadingFreeTop) + heapSizeToMove), (void *)allocateSegmentBase);
			contractSlotScanner.setIncludeStackFrameClassReferences(false);
			contractSlotScanner.scanAllSlots(env);

			/* Adjust the used heap ranges */
			allocateSpaceUsedHeapTop = ((uint8_t *)allocateSegmentBase) + heapSizeToMove;
		}

		/* perform all sanity assertions regarding semi-space region geometry */
		Assert_MM_true(_highSemiSpaceRegion->getLowAddress() == _lowSemiSpaceRegion->getHighAddress());

		/* Reset any free list information */
		subSpaceAllocate->reset();
		subSpaceSurvivor->reset();

		/* Was there any remaining leading free entry not consumed by the contraction? */
		if(allocateSegmentBase < allocateLeadingFreeTop) {
			/* Rebuild the allocate leading free list */
			if(debug) {
				omrtty_printf("\tAdd free range back (%p %p) size %p\n",
					(void *)allocateSegmentBase,
					allocateLeadingFreeTop,
					(void *)(((uintptr_t)allocateLeadingFreeTop) - ((uintptr_t)allocateSegmentBase)));
			}
			subSpaceAllocate->addExistingMemory(
				env,
				this,
				((uintptr_t)allocateLeadingFreeTop) - ((uintptr_t)allocateSegmentBase),
				(void *)allocateSegmentBase,
				allocateLeadingFreeTop,
				true);
		}
		
		/* Rebuild the allocate trailing free list */
		if(debug) {
			omrtty_printf("\tAdd free range back (%p %p) size %p\n",
				(void *)allocateSpaceUsedHeapTop,
				(void *)allocateSegmentTop,
				(uintptr_t)(allocateSegmentTop - (uintptr_t)allocateSpaceUsedHeapTop));
		}
		subSpaceAllocate->addExistingMemory(
			env,
			this,
			(uintptr_t)(allocateSegmentTop - (uintptr_t)allocateSpaceUsedHeapTop),
			(void *)allocateSpaceUsedHeapTop,
			(void *)allocateSegmentTop,
			true);

		/* Rebuild the survivor free list */
		subSpaceSurvivor->addExistingMemory(
			env,
			this,
			((uintptr_t)survivorSegmentTop) - ((uintptr_t)survivorSegmentBase),
			(void *)survivorSegmentBase,
			(void *)survivorSegmentTop,
			true);

		/* Find the previous valid address ahead of the area removed - used by removal broadcast and decommit */
		void *previousValidAddressNotRemoved = (NULL != _lowArena) ? _lowArena->getHighAddress() : NULL;
			
		/* Broadcast the contraction of the heap, but do not remove the memory from any free lists */
		if(debug) {
			omrtty_printf("\tRemove and decommit (%p %p) (valid %p %p)\n",
				removeMemoryBase,
				removeMemoryTop,
				previousValidAddressNotRemoved,
				(void *)survivorSegmentBase);
		}
		_subSpace->heapRemoveRange(
			env,
			_subSpace,
			removeMemorySize,
			removeMemoryBase,
			removeMemoryTop,
			previousValidAddressNotRemoved,
			(void *)survivorSegmentBase);
		_subSpace->heapReconfigured(env);

		/* Decommit the heap (the return value really doesn't matter here - its already too late) */
		_heap->decommitMemory(
			removeMemoryBase,
			removeMemorySize,
			previousValidAddressNotRemoved,
			(void *)survivorSegmentBase);
		
		/* Adjust sizing values within memory sub spaces */
		subSpaceAllocate->setCurrentSize(_highSemiSpaceRegion->getSize());
		subSpaceSurvivor->setCurrentSize(_lowSemiSpaceRegion->getSize());

		/* Set the low address of the arena */
		_lowAddress = (void *)survivorSegmentBase;
	}

	if(debug) {
		omrtty_printf("\tSuccessful contract (%p bytes)\n", totalContractSize);
	}

	/* Execute any pending counter balances to the contract that have been enqueued */
	_subSpace->triggerEnqueuedCounterBalancing(env);

	Assert_MM_true(_lowAddress == _lowSemiSpaceRegion->getLowAddress());
	Assert_MM_true(_highAddress == _highSemiSpaceRegion->getHighAddress());

	return totalContractSize;
}

/**
 * Determine whether the child arena is allowed to expand according to the description given.
 * 
 * @return true if the child is allowed to expand, false otherwise.
 * 
 * @note semispace cannot have children that can expand.
 */
bool
MM_PhysicalSubArenaVirtualMemorySemiSpace::canExpand(MM_EnvironmentBase *env)
{
	return _resizable;
}

/**
 * Determine whether the sub arena is allowed to expand.
 * The generic implementation forwards the request to the parent, or returns true if there is none.
 * 
 * @return true if the expansion is allowed, false otherwise.
 */
bool
MM_PhysicalSubArenaVirtualMemorySemiSpace::canContract(MM_EnvironmentBase *env)
{
	if (_resizable) {
		if (_avoidMovingObjects) {
			MM_MemorySubSpace * subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();
			/* VMDESIGN 1690: only permit contraction if the allocate space is the high semi-space. This case is 
			 * much less likely to require object movement and is therefore significantly faster. However, depending on
			 * the tilt ratio and contraction amount, movement may still be required.
			 */
			if(subSpaceAllocate == _highSemiSpaceRegion->getSubSpace()) {
				return true;
			}
		} else {
			return true;
		}
	}
	
	return false;
}

/**
 * Adjust the allocate and survivor spaces to have the sizes specified.
 * Force the tilting of new space to be of the exact sizes specified.  There are a number of assumptions made, including
 * the alignment of the sizes, the sizes are equal to the total semispace size, the sizes fall within ration limits, etc.
 * @note Assumes that there is a single free entry in the allocate space and that it is at the END of the range (segment).
 * @todo This is just an exploratory implementation until the details are actually hammered out.
 */
void
MM_PhysicalSubArenaVirtualMemorySemiSpace::tilt(MM_EnvironmentBase *env, uintptr_t allocateSpaceSize, uintptr_t survivorSpaceSize)
{
	void *expandBase, *expandTop;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase *extensions = env->getExtensions();
	bool debug = extensions->debugTiltedScavenge;

	/* make sure that the sizes we were given are properly aligned */
	Assert_MM_true(0 == (allocateSpaceSize % extensions->heapAlignment));
	Assert_MM_true(0 == (survivorSpaceSize % extensions->heapAlignment));
	Assert_MM_true(0 == (allocateSpaceSize % extensions->regionSize));
	Assert_MM_true(0 == (survivorSpaceSize % extensions->regionSize));

	MM_MemorySubSpace *subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();
	MM_MemorySubSpace *subSpaceSurvivor = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceSurvivor();

	if(subSpaceAllocate == _lowSemiSpaceRegion->getSubSpace()) {
		/* The allocate subspace is the lower segment */
		if(debug) {
			omrtty_printf("\tlowseg=allocate highseg=survivor\n");
			omrtty_printf("\tAllocate (%p %p) survivor (%p %p)\n",
				_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(), 
				_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
		}

		/* Adjust the lower boundary of the survivor (high) segment */
		void *highBase = (void *)(((uintptr_t)_highSemiSpaceRegion->getHighAddress()) - survivorSpaceSize);
		getHeapRegionManager()->resizeAuxillaryRegion(env, _highSemiSpaceRegion, highBase, _highSemiSpaceRegion->getHighAddress());

		/* Record the expand range */
		expandBase = _lowSemiSpaceRegion->getHighAddress();
		expandTop = _highSemiSpaceRegion->getLowAddress();

		/* Adjust the upper boundary of the allocate (low) segment */
		getHeapRegionManager()->resizeAuxillaryRegion(env, _lowSemiSpaceRegion, _lowSemiSpaceRegion->getLowAddress(), expandTop);

		if(debug) {
			omrtty_printf("\tAdjusted Allocate (%p %p) survivor (%p %p)\n",
				_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(), 
				_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
		}

		/* notify the subspaces of the current size changes */
		subSpaceAllocate->setCurrentSize(_lowSemiSpaceRegion->getSize());
		subSpaceSurvivor->setCurrentSize(_highSemiSpaceRegion->getSize());
	} else {
		/* The survivor subspace is the lower segment */
		if(debug) {
			omrtty_printf("\tlowseg=survivor highseg=allocate\n");
			omrtty_printf("\tSurvivor (%p %p) allocate (%p %p)\n",
				_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(), 
				_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
		}
		/* Adjust the lower boundary of the surivor (high) segment */
		void *lowTop = (void *) (((uintptr_t)_lowSemiSpaceRegion->getLowAddress()) + survivorSpaceSize);
		getHeapRegionManager()->resizeAuxillaryRegion(env, _lowSemiSpaceRegion, _lowSemiSpaceRegion->getLowAddress(), lowTop);

		/* Record the expand range */
		expandBase = (void *)_lowSemiSpaceRegion->getHighAddress();
		expandTop = (void *)_highSemiSpaceRegion->getLowAddress();

		/* Adjust the upper boundary of the allocate (low) segment */
		getHeapRegionManager()->resizeAuxillaryRegion(env, _highSemiSpaceRegion, lowTop, _highSemiSpaceRegion->getHighAddress());

		if(debug) {
			omrtty_printf("\tAdjusted Survivor (%p %p) allocate (%p %p)\n",
				_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(), 
				_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
		}

		/* notify the subspaces of the current size changes */
		subSpaceAllocate->setCurrentSize(_highSemiSpaceRegion->getSize());
		subSpaceSurvivor->setCurrentSize(_lowSemiSpaceRegion->getSize());
	}
	if(debug) {
		omrtty_printf("\tNew range added (%p %p)}\n", expandBase, expandTop);
	}

	/* Add the new range to the allocate subspace free list */
	subSpaceSurvivor->removeExistingMemory(env, this, ((uintptr_t)expandTop) - ((uintptr_t)expandBase), expandBase, expandTop);
	subSpaceAllocate->addExistingMemory(env, this, ((uintptr_t)expandTop) - ((uintptr_t)expandBase), expandBase, expandTop, true);

	/* Set the new tilt ratio of the receiver */
	((MM_MemorySubSpaceSemiSpace *)_subSpace)->setSurvivorSpaceSizeRatio(survivorSpaceSize / ((_highSemiSpaceRegion->getSize() + _lowSemiSpaceRegion->getSize()) / 100));

	/* Broadcast that the heap has been reconfigured */
	_subSpace->heapReconfigured(env);
}

/**
 * Slide the boundary between the two semi spaces to create sufficient free space to operate in both the
 * allocate and survivor subspaces.
 * @note Assumes that there is a single free entry in the allocate space and that it is at the END of the range (segment).
 * @todo This is just an exploratory implementation until the details are actually hammered out.
 */
void
MM_PhysicalSubArenaVirtualMemorySemiSpace::tilt(MM_EnvironmentBase *env, uintptr_t survivorSpaceSizeRequested)
{
	/*
	 * 1) Determine the minimum size required for the evacuate space (used * constant)
	 * 2) Associate the high/low segment to the allocate/survivor subspace.
	 * 3) If the evacuate space is greater than the required used space,
	 *   a) Adjust the segment boundaries between the two spaces
	 *   b) Rebuild the evacuate space free entry and expand the allocate space free list with the new range
	 *   c) Adjust memory pool statistics
	 *   d) Report the pseudo-expand event
	 */
	MM_GCExtensionsBase *extensions = env->getExtensions();
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	bool debug = extensions->debugTiltedScavenge;

	if(debug) {
		omrtty_printf("Tilt attempt:\n");
	}

	uintptr_t heapAlignment = extensions->heapAlignment;

	MM_MemorySubSpace *subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();

	/* Calculate the minimum amount of free space required in the evacuate space */
	/* Performance sanity checks for sizing */
	uintptr_t availableBytesForSurvivorSpace;

	/* Calculate how much room is currently available in the survivor semi space */
	if(subSpaceAllocate == _lowSemiSpaceRegion->getSubSpace()) {
		availableBytesForSurvivorSpace = _highSemiSpaceRegion->getSize();
	} else {
		availableBytesForSurvivorSpace = _lowSemiSpaceRegion->getSize();
	}

	uintptr_t survivorSpaceSize = MM_Math::roundToCeiling(heapAlignment, survivorSpaceSizeRequested);
	survivorSpaceSize = (survivorSpaceSize > extensions->absoluteMinimumNewSubSpaceSize) ?
		survivorSpaceSize : MM_Math::roundToCeiling(heapAlignment, extensions->absoluteMinimumNewSubSpaceSize);
	survivorSpaceSize = MM_Math::roundToCeiling(extensions->regionSize, survivorSpaceSizeRequested);

	/* Determine what the minimum and maximum amount free that should be made available in the survivor space for
	 * the next collect.
	 * Constant: The division of 2 is 50% of total new space size.
	 */
	uintptr_t minimumBytesFree, maximumBytesFree;
	double survivorSpaceMinimumSizeRatio = extensions->survivorSpaceMinimumSizeRatio;
	
	minimumBytesFree = (uintptr_t)((_lowSemiSpaceRegion->getSize() + _highSemiSpaceRegion->getSize())  * survivorSpaceMinimumSizeRatio);
	minimumBytesFree = MM_Math::roundToCeiling(extensions->heapAlignment, minimumBytesFree);
	minimumBytesFree = (minimumBytesFree >= extensions->absoluteMinimumNewSubSpaceSize) ?
		minimumBytesFree : extensions->absoluteMinimumNewSubSpaceSize;
	maximumBytesFree = (_lowSemiSpaceRegion->getSize() + _highSemiSpaceRegion->getSize()) / 2;  // 50% of new space
	maximumBytesFree = MM_Math::roundToCeiling(extensions->heapAlignment, maximumBytesFree);
	maximumBytesFree = (maximumBytesFree >= extensions->absoluteMinimumNewSubSpaceSize) ?
		maximumBytesFree : extensions->absoluteMinimumNewSubSpaceSize;

	/* region size rounding is required on these minimum and maximum calculations since they can be used to change the requested survivor space */
	minimumBytesFree = MM_Math::roundToCeiling(extensions->regionSize, minimumBytesFree);
	maximumBytesFree = MM_Math::roundToCeiling(extensions->regionSize, maximumBytesFree);

	/* Adjust the required free if it is less than the minimum free */
	if(survivorSpaceSize < minimumBytesFree) {
		survivorSpaceSize = minimumBytesFree;
	}

	/* Adjust the required free if it is less than the minimum free */
	if(survivorSpaceSize > maximumBytesFree) {
		survivorSpaceSize = maximumBytesFree;
	}

	/* Check if the semi space divider can be slid (this is a one way check - is that ok?) */
	if(survivorSpaceSize >= availableBytesForSurvivorSpace) {
		if(debug) {
			omrtty_printf("\tAvailable: %p Required: %p - TILT ABORTED\n", availableBytesForSurvivorSpace, survivorSpaceSize);
		}
		return ;
	}

	if(debug) {
		omrtty_printf("\tAvailable: %d(%p)  Required: %d(%p)\n", availableBytesForSurvivorSpace, availableBytesForSurvivorSpace, survivorSpaceSize, survivorSpaceSize);
	}

	/* Associate the high/low region to the allocate/survivor subspace */
	tilt(env, (_lowSemiSpaceRegion->getSize() + _highSemiSpaceRegion->getSize()) - survivorSpaceSize, survivorSpaceSize);
}
/**
 * Expand the receiver by the given size.
 * Expand by the given size with no safety checks.  Restrictions on expansion size based on the amount of room available and the
 * room between the receiver and any neighbouring PSA are ignored.  The size for expansion may be reduced based on splitting
 * requirements (the split may not be even, or possible).
 * @warn the routine can fail to expand in extreme circumstances where the free list cannot provide memory (returns 0)
 * @return the size of the expansion
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemorySemiSpace::expandNoCheck(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	/* only check for regular alignment - splitting code will find the correct alignment for each subspace */
	Assert_MM_true(expandSize == MM_Math::roundToCeiling(extensions->heapAlignment, expandSize));

	Assert_MM_true(_subSpace->getCurrentSize() == (_lowSemiSpaceRegion->getSize() + _highSemiSpaceRegion->getSize()));
	Assert_MM_true(_lowAddress == (void *)_lowSemiSpaceRegion->getLowAddress());
	Assert_MM_true(_highAddress == (void *)_highSemiSpaceRegion->getHighAddress());
	Assert_MM_true(expandSize == MM_Math::roundToCeiling(getHeapRegionManager()->getRegionSize(), expandSize));

	bool debug = extensions->debugDynamicNewSpaceSizing;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if(debug) {
		omrtty_printf("Expand no check (size:%p)\n", expandSize);
	}

	/* Need to calculate (based on split) the allocate and survivor space sizes */
	uintptr_t allocateSpaceExpandSize = 0;
	uintptr_t survivorSpaceExpandSize = 0;
	uintptr_t splitExpandSize = calculateExpansionSplit(env, expandSize, &allocateSpaceExpandSize, &survivorSpaceExpandSize);
	Assert_MM_true(splitExpandSize == MM_Math::roundToCeiling(getHeapRegionManager()->getRegionSize(), splitExpandSize));
	
	/* If a split is not possible then we are done */
	if (0 == splitExpandSize) {
		return 0;
	}	
	
	if(debug) {
		omrtty_printf("\tsplit adjusted expand size (size:%p alloc:%p surv:%p)\n",
			splitExpandSize, allocateSpaceExpandSize, survivorSpaceExpandSize);
	}

	Assert_MM_true(allocateSpaceExpandSize == MM_Math::roundToCeiling(extensions->heapAlignment, allocateSpaceExpandSize));
	Assert_MM_true(survivorSpaceExpandSize == MM_Math::roundToCeiling(extensions->heapAlignment, survivorSpaceExpandSize));
	/* Find where the allocate subspace is */	
	MM_MemorySubSpace *subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();
	MM_MemorySubSpace *subSpaceSurvivor = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceSurvivor();

	if(debug) {
		omrtty_printf("\tlowseg:(%p %p) highseg:(%p %p)\n",
			_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(),
			_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
	}

	if(subSpaceAllocate == _lowSemiSpaceRegion->getSubSpace()) {
		/* The allocate subspace is the lower segment */

		if(debug) {
			omrtty_printf("\tlowseg=allocate highseg=survivor\n");
		}

		Assert_MM_true(subSpaceAllocate->getCurrentSize() == _lowSemiSpaceRegion->getSize());
		Assert_MM_true(subSpaceSurvivor->getCurrentSize() == _highSemiSpaceRegion->getSize());

		/* Find the trailing free entry in the memory space (single segment) that will get transfered to the
		 * other semi space
		 */
		void *freeRangeToTransferBase = (void *)_lowSemiSpaceRegion->getHighAddress();
		if(0 != allocateSpaceExpandSize) {
			freeRangeToTransferBase = subSpaceAllocate->findFreeEntryEndingAtAddr(env, (void *)_lowSemiSpaceRegion->getHighAddress());
			freeRangeToTransferBase = (void *)MM_Math::roundToCeiling(extensions->heapAlignment, ((uintptr_t)freeRangeToTransferBase));
			Assert_MM_true(NULL != freeRangeToTransferBase);
			
			/* Calculate the heap alignment boundary for the transfer math */
			uintptr_t freeRangeAvailableToTransferSize = ((uintptr_t)_lowSemiSpaceRegion->getHighAddress()) - ((uintptr_t)freeRangeToTransferBase);
			Assert_MM_true(freeRangeAvailableToTransferSize >= survivorSpaceExpandSize);

			/* Get the base address of the transfer range */
			freeRangeToTransferBase = (void *) (((uintptr_t)freeRangeToTransferBase) + (freeRangeAvailableToTransferSize - survivorSpaceExpandSize));
		}
		
		/* Find the new low address for the semispaces */
		void *newLowAddress = (void *) (((uintptr_t)_lowAddress) - splitExpandSize);

		/* Commit in the memory to be added */
		if(debug) {
			omrtty_printf("\tCommit (%p %p)\n", newLowAddress, ((uintptr_t)newLowAddress) + splitExpandSize);
		}
		if(!_heap->commitMemory(newLowAddress, splitExpandSize)) {
			/* Memory couldn't be commited (for whatever reason) - can't expand */
			return 0;
		}
		/* The survivor space will have its free list rebuilt - don't bother adding memory */
		if(debug) {
			omrtty_printf("\tRemove: allocate(%p %p)\n", freeRangeToTransferBase, (void *)_lowSemiSpaceRegion->getHighAddress());
		}

		subSpaceAllocate->removeExistingMemory(
			env,
			this,
			survivorSpaceExpandSize,
			freeRangeToTransferBase,
			_lowSemiSpaceRegion->getHighAddress());

		/* Transfer the memory from the allocate space to the survivor space */
		/* ensure that the lower region is expanded first, so it is out of the way of the high region */
		getHeapRegionManager()->resizeAuxillaryRegion(env, _lowSemiSpaceRegion, newLowAddress, freeRangeToTransferBase);
		getHeapRegionManager()->resizeAuxillaryRegion(env, _highSemiSpaceRegion, freeRangeToTransferBase, _highSemiSpaceRegion->getHighAddress());

		/* Broadcast the expansion of the heap, but do not add the memory to any free lists.
		 * Must be done after segment information has been updated.
		 */
		_subSpace->heapAddRange(env, _subSpace, splitExpandSize, newLowAddress, (void *) (((uintptr_t)newLowAddress) + splitExpandSize));
		_subSpace->heapReconfigured(env);
		
		/* Include the memory into the free lists */
		if(debug) {
			omrtty_printf("\tAdd: allocate (%p %p)\n", newLowAddress, _lowAddress);
		}
		subSpaceAllocate->addExistingMemory(env, this, splitExpandSize, newLowAddress, _lowAddress, true);
		
		subSpaceSurvivor->reset();
		subSpaceSurvivor->addExistingMemory(
			env,
			this,
			_highSemiSpaceRegion->getSize(),
			_highSemiSpaceRegion->getLowAddress(),
			_highSemiSpaceRegion->getHighAddress(),
			true);

		/* Adjust sizing values within memory sub spaces */
		subSpaceAllocate->setCurrentSize(_lowSemiSpaceRegion->getSize());
		subSpaceSurvivor->setCurrentSize(_highSemiSpaceRegion->getSize());

		/* And adjust the PSA details */
		_lowAddress = newLowAddress;

		Assert_MM_true(subSpaceAllocate->getCurrentSize() == _lowSemiSpaceRegion->getSize());
		Assert_MM_true(subSpaceSurvivor->getCurrentSize() == _highSemiSpaceRegion->getSize());
	} else {
		/* The allocate subspace is the higher segment */
		if(debug) {
			omrtty_printf("\tlowseg=survivor highseg=allocate\n");
		}

		Assert_MM_true(subSpaceAllocate->getCurrentSize() == _highSemiSpaceRegion->getSize());
		Assert_MM_true(subSpaceSurvivor->getCurrentSize() == _lowSemiSpaceRegion->getSize());

		/* Find the new low address for the semispaces */
		void *newLowAddress = (void *) (((uintptr_t)_lowAddress) - splitExpandSize);

		/* Check if the total expand size is valid */
		//TODO: Need to check max

		/* Commit in the memory to be added */
		if(debug) {
			omrtty_printf("\tCommit (%p %p)\n", newLowAddress, ((uintptr_t)newLowAddress)+splitExpandSize);
		}
		if(!_heap->commitMemory(newLowAddress, splitExpandSize)) {
			/* Memory couldn't be commited (for whatever reason) - can't expand */
			return 0;
		}
		/* Adjust the high and low segment ranges (high gains at its base, low gives
		 * way at top and gains at base)
		 */
		uintptr_t newLowSpacesize = _lowSemiSpaceRegion->getSize() + survivorSpaceExpandSize;
		void *previousHighSemiSpaceSegmentBase = (void *)_highSemiSpaceRegion->getLowAddress();
		/* ensure that the lower region is expanded first, so it is out of the way of the high region */
		getHeapRegionManager()->resizeAuxillaryRegion(env, _lowSemiSpaceRegion, newLowAddress, (void*)((uintptr_t)newLowAddress + newLowSpacesize));
		getHeapRegionManager()->resizeAuxillaryRegion(env, _highSemiSpaceRegion, _lowSemiSpaceRegion->getHighAddress(), _highSemiSpaceRegion->getHighAddress());

		/* Broadcast the expansion of the heap, but do not add the memory to any free lists.
		 * Must be done after segment information has been updated.
		 */
		_subSpace->heapAddRange(env, _subSpace, splitExpandSize, newLowAddress, (void *) (((uintptr_t)newLowAddress) + splitExpandSize));
		_subSpace->heapReconfigured(env);

		/* Include the memory into the free lists. */
		if(debug) {
			omrtty_printf("\tAdd: allocate (%p %p)\n", _highSemiSpaceRegion->getLowAddress(), previousHighSemiSpaceSegmentBase);
		}
		subSpaceAllocate->addExistingMemory(env, this, allocateSpaceExpandSize, _highSemiSpaceRegion->getLowAddress(), previousHighSemiSpaceSegmentBase, true);

		subSpaceSurvivor->reset();
		subSpaceSurvivor->addExistingMemory(
			env,
			this,
			_lowSemiSpaceRegion->getSize(),
			_lowSemiSpaceRegion->getLowAddress(),
			_lowSemiSpaceRegion->getHighAddress(),
			true);

		/* Adjust sizing values within memory sub spaces */
		subSpaceAllocate->setCurrentSize(_highSemiSpaceRegion->getSize());
		subSpaceSurvivor->setCurrentSize(_lowSemiSpaceRegion->getSize());

		/* And adjust the PSA details */
		_lowAddress = newLowAddress;

		Assert_MM_true(subSpaceAllocate->getCurrentSize() == _highSemiSpaceRegion->getSize());
		Assert_MM_true(subSpaceSurvivor->getCurrentSize() == _lowSemiSpaceRegion->getSize());
	}

	if(debug) {
		omrtty_printf("\tlowseg:(%p %p) highseg:(%p %p)\n",
			_lowSemiSpaceRegion->getLowAddress(), _lowSemiSpaceRegion->getHighAddress(),
			_highSemiSpaceRegion->getLowAddress(), _highSemiSpaceRegion->getHighAddress());
	}

	Assert_MM_true(_subSpace->getCurrentSize() == (_lowSemiSpaceRegion->getSize() + _highSemiSpaceRegion->getSize()));
	Assert_MM_true(_lowAddress == (void *)_lowSemiSpaceRegion->getLowAddress());
	Assert_MM_true(_highAddress == (void *)_highSemiSpaceRegion->getHighAddress());

	return splitExpandSize;
}

/**
 * Split and reduce the expansion request to meet allocate and survivor space restrictions.
 * Given the expand value (that is rounded to proper alignment), calculate the actual allocate and
 * survivor space splits that can be had from the value (given tilting, physical restrictions, etc).
 * The sum of these two values may be less than or equal to the expand size originally passed in.  The
 * routine guarantees that any result for allocate and surivor space sizes, if added together and fed
 * back into the routine, will return the exact same size split.
 * @return The sum of the allocate and surivivor space sizes
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemorySemiSpace::calculateExpansionSplit(MM_EnvironmentBase *env, uintptr_t requestExpandSize, uintptr_t *allocateSpaceSize, uintptr_t *survivorSpaceSize)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	/* Set the split sizes to 0 as the default */
	*allocateSpaceSize = 0;
	*survivorSpaceSize = 0;

	/* Initialize the split values that are calculated internally (and prime the result) */
	uintptr_t expandSize = requestExpandSize;

	Assert_MM_true(_subSpace->getCurrentSize() == (_lowSemiSpaceRegion->getSize() + _highSemiSpaceRegion->getSize()));
	Assert_MM_true(_lowAddress == _lowSemiSpaceRegion->getLowAddress());
	Assert_MM_true(_highAddress == _highSemiSpaceRegion->getHighAddress());

	uintptr_t allocateSpaceExpandSize = 0;
	uintptr_t survivorSpaceExpandSize = 0;
	uintptr_t survivorSpaceSizeRatio = 0;
	if(extensions->tiltedScavenge) {
		survivorSpaceSizeRatio = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getSurvivorSpaceSizeRatio();
		survivorSpaceExpandSize = MM_Math::roundToCeiling(extensions->heapAlignment, (expandSize / 100) * survivorSpaceSizeRatio);
		survivorSpaceExpandSize = MM_Math::roundToCeiling(getHeapRegionManager()->getRegionSize(), survivorSpaceExpandSize);
		allocateSpaceExpandSize = expandSize - survivorSpaceExpandSize;
		if(0 == allocateSpaceExpandSize) {
			/* We can't seem to actually find a split that works - cannot calculate the size */
			return 0;
		}
	} else {
		allocateSpaceExpandSize = MM_Math::roundToFloor(extensions->heapAlignment, expandSize / 2);
		allocateSpaceExpandSize = MM_Math::roundToFloor(getHeapRegionManager()->getRegionSize(), allocateSpaceExpandSize);
		survivorSpaceExpandSize = allocateSpaceExpandSize;
		expandSize = allocateSpaceExpandSize + survivorSpaceExpandSize;
	}

	/* Find where the allocate subspace is */	
	MM_MemorySubSpace *subSpaceAllocate = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceAllocate();
	MM_MemorySubSpace *subSpaceSurvivor = ((MM_MemorySubSpaceSemiSpace *)_subSpace)->getMemorySubSpaceSurvivor();

	if(subSpaceAllocate == _lowSemiSpaceRegion->getSubSpace()) {
		/* The allocate subspace is the lower segment */
		Assert_MM_true(subSpaceAllocate->getCurrentSize() == _lowSemiSpaceRegion->getSize());
		Assert_MM_true(subSpaceSurvivor->getCurrentSize() == _highSemiSpaceRegion->getSize());

		/* Find the trailing free entry in the memory space (single segment) that will get transfered to the other semi space */
		void *freeRangeToTransferBase = subSpaceAllocate->findFreeEntryEndingAtAddr(env, _lowSemiSpaceRegion->getHighAddress());
		if(NULL == freeRangeToTransferBase) {
			/* No memory available to transfer */
			return 0;
		}
		
		uintptr_t initialFreeRangeToTransferSize = ((uintptr_t)_lowSemiSpaceRegion->getHighAddress()) - ((uintptr_t)freeRangeToTransferBase);
		uintptr_t freeRangeToTransferSize = MM_Math::roundToFloor(extensions->heapAlignment, initialFreeRangeToTransferSize);
		freeRangeToTransferSize = MM_Math::roundToFloor(getHeapRegionManager()->getRegionSize(), freeRangeToTransferSize);
		if(0 == freeRangeToTransferSize) {
			/* Not enough memory available to transfer */
			return 0;
		}

		/* Determine the exact quantity of space to transfer from the allocate to the surivor space.
		 * The tranfer occurs to evenly split the memory added to the front of the semi spaces, which is adjacent to
		 * the allocate space.  To transfer half of the split to the survivor, we use the trailing free space in the 
		 * allocate space.
		 */
		/* Adjust the allocate and survivor expand sizes if needed */
		if(freeRangeToTransferSize < survivorSpaceExpandSize) {
			survivorSpaceExpandSize = freeRangeToTransferSize;
			Assert_MM_true(survivorSpaceExpandSize % extensions->heapAlignment == 0);  // transfer size was align guaranteed before

			if(extensions->tiltedScavenge) {
				/* The math for survivor is as follows,
				 *    survivorSize = (totalSize / 100) * survivorRatio
				 * To find the total size, the formula transforms to,
				 *    totalSize = (survivorSize / survivorRatio) * 100
				 */
				expandSize = (survivorSpaceExpandSize / survivorSpaceSizeRatio) * 100;
				/* No need to round to floor - the size is being reduced, so we can only be equal at most */
				Assert_MM_true(expandSize <= MM_Math::roundToCeiling(extensions->heapAlignment * 2, expandSize));
				expandSize = MM_Math::roundToCeiling(extensions->heapAlignment * 2, expandSize);
				expandSize = MM_Math::roundToCeiling(getHeapRegionManager()->getRegionSize() * 2, expandSize);
				Assert_MM_true(expandSize <= requestExpandSize);
				allocateSpaceExpandSize = expandSize - survivorSpaceExpandSize;
			} else {
				allocateSpaceExpandSize = survivorSpaceExpandSize;
				expandSize = allocateSpaceExpandSize + survivorSpaceExpandSize;
			}
		}
	}

	/* We've decided on some sizes - set the extra return values and end */
	*allocateSpaceSize = allocateSpaceExpandSize;
	*survivorSpaceSize = survivorSpaceExpandSize;

	Assert_MM_true((allocateSpaceExpandSize + survivorSpaceExpandSize) == expandSize);

	return expandSize;
}

/**
 * Check if the receiver can complete a counter balancing with the requested expand size.
 * The check to complete a counter balance with an expand involves verifying address ranges are possible against virtually
 * adjusted address ranges of other PSA's, as well as checking the actual physical boundaries.  The expand size can be adjusted
 * by increments of the delta alignment that is supplied.
 * @todo adjusting the expand size may have an effect on the virtual addresses - needs to be addressed.
 * @return the actual physical size that the receiver can expand by, or 0 if there is no room or alignment restrictions cannot be met.
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemorySemiSpace::checkCounterBalanceExpand(MM_EnvironmentBase *env, uintptr_t expandSizeDeltaAlignment, uintptr_t expandSize)
{
	uintptr_t adjustedExpandSize;
	uintptr_t physicalMaximumExpandSize;

	/* Record the expand size */
	adjustedExpandSize = expandSize;

	/* Determine the physical limit available for expansion (in whichever direction is valid) */
	uintptr_t physicalLimitLowAddress;
	if(NULL != _lowArena) {
		physicalLimitLowAddress = (uintptr_t)_lowArena->getVirtualHighAddress();
	} else {
		physicalLimitLowAddress = (uintptr_t)((MM_PhysicalArenaVirtualMemory *)_parent)->getLowAddress();
	}
	physicalMaximumExpandSize = ((uintptr_t)_lowAddress) - physicalLimitLowAddress;

	/* Given the physical limit, adjust the expand size if necessary, but only by amounts that are rounded
	 * to the expansion alignment.
	 */		
	if(physicalMaximumExpandSize < adjustedExpandSize) {
		uintptr_t expandSizeDelta;
		expandSizeDelta = adjustedExpandSize - physicalMaximumExpandSize;
		expandSizeDelta = MM_Math::roundToCeiling(expandSizeDeltaAlignment, expandSizeDelta);
		if(expandSizeDelta >= adjustedExpandSize) {
			return 0;
		}
		
		adjustedExpandSize -= expandSizeDelta;
	}

	/* Find the actual expand size that fits into the splitting rules of the semi space */
	uintptr_t allocateSpaceSize, survivorSpaceSize;
	uintptr_t splitExpandSize, splitDelta;
	splitExpandSize = calculateExpansionSplit(env, adjustedExpandSize, &allocateSpaceSize, &survivorSpaceSize);
	assume0(splitExpandSize <= adjustedExpandSize);

	/* The splitting can reduce the expand size, so make sure the delta in the expansion is rounded to any requirements.
	 * NOTE: The delta is what needs to be rounded, not the result.
	 */
	splitDelta = adjustedExpandSize - splitExpandSize;
	splitDelta = MM_Math::roundToCeiling(expandSizeDeltaAlignment, splitDelta);
	adjustedExpandSize = (adjustedExpandSize < splitDelta) ? 0 : adjustedExpandSize - splitDelta;
	assume0(adjustedExpandSize == calculateExpansionSplit(env, adjustedExpandSize, &allocateSpaceSize, &survivorSpaceSize));

	/* Found a valid expand size, return */
	assume0(0 == (adjustedExpandSize % MM_GCExtensions::getExtensions(env)->heapAlignment));
	return adjustedExpandSize;
}

#endif /* OMR_GC_MODRON_SCAVENGER */

