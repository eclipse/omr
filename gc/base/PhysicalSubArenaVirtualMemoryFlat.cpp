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


#include "omrcfg.h"

#include "PhysicalSubArenaVirtualMemoryFlat.hpp"

#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySubSpace.hpp"
#include "PhysicalArena.hpp"
#include "PhysicalArenaVirtualMemory.hpp"
#include "VirtualMemory.hpp"
#include "ModronAssertions.h"

/**
 * Zos390 Platform dependent trickery for the ar command.  Without this definition
 * the ar command on Zos390 will fail to link this class.
 */
#if defined(J9ZOS390)
#include "ZOSLinkage.hpp"
int j9zos390LinkTrickPhysicalSubArenaVirtualMemoryFlat;
#endif /* J9ZOS390 */

/**
 * Create and return a new instance of MM_PhysicalSubArenaVirtualMemoryFlat.
 */
MM_PhysicalSubArenaVirtualMemoryFlat *
MM_PhysicalSubArenaVirtualMemoryFlat::newInstance(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_PhysicalSubArenaVirtualMemoryFlat *subArena;

	subArena = (MM_PhysicalSubArenaVirtualMemoryFlat *)env->getForge()->allocate(sizeof(MM_PhysicalSubArenaVirtualMemoryFlat), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(subArena) {
		new(subArena) MM_PhysicalSubArenaVirtualMemoryFlat(heap);
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
MM_PhysicalSubArenaVirtualMemoryFlat::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_PhysicalSubArenaVirtualMemoryFlat::initialize(MM_EnvironmentBase *env)
{
	if(!MM_PhysicalSubArenaVirtualMemory::initialize(env)) {
		return false;
	}

	return true;
}

/**
 * Destroy and delete the instances internal structures.
 */
void
MM_PhysicalSubArenaVirtualMemoryFlat::tearDown(MM_EnvironmentBase *env)
{
	void *lowValidAddress = NULL;
	void *highValidAddress = NULL;

	if (NULL != _region) {
		lowValidAddress = _region->getLowAddress();
		highValidAddress = _region->getHighAddress();
		getHeapRegionManager()->destroyAuxiliaryRegionDescriptor(env, _region);
		_region = NULL;
	}
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
MM_PhysicalSubArenaVirtualMemoryFlat::inflate(MM_EnvironmentBase *env)
{
	bool result = false;
	if(_parent->attachSubArena(env, this, _subSpace->getInitialSize(), modron_pavm_attach_policy_none)) {
		MM_HeapRegionManager *regionManager = getHeapRegionManager();
		_region = regionManager->createAuxiliaryRegionDescriptor(env, _subSpace->getChildren(), _lowAddress, _highAddress);
		if(NULL != _region) {
			Assert_MM_true((_lowAddress == _region->getLowAddress()) && (_highAddress == _region->getHighAddress()));
			/* Inflation successful - inform the owning memorySubSpace */
			//TODO: Like the semi space arena, this should dispatch directory to the child subspace
			result = _subSpace->expanded(env, this, _region->getSize(), _region->getLowAddress(), _region->getHighAddress(), false);
			_subSpace->heapReconfigured(env);
		}
	}
	return result;
}

/**
 * Expand the physical arena by the parameters in the description.
 * The expand request size is just a guideline - if the subarena cannot satisfy the request,
 * @return Number of bytes the arena was expanded by
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemoryFlat::expand(MM_EnvironmentBase *env, uintptr_t requestExpandSize)
{
	uintptr_t expandSize;

	/* Adjust the expand size to alignment requirements */
	expandSize = MM_Math::roundToCeiling(env->getExtensions()->heapAlignment, requestExpandSize);
	expandSize = MM_Math::roundToCeiling(getHeapRegionManager()->getRegionSize(), expandSize);

	/* Find the physical number of bytes that we could possibly expand by */
	expandSize = OMR_MIN(expandSize, ((MM_PhysicalArenaVirtualMemory *)_parent)->getPhysicalMaximumExpandSizeHigh(env, _highAddress));

	/* If we have a neighbouring arena, check if our expand request can be met - if not, try and contract the neighbour.
	 * No matter what the result, adjust the expand size to be as much as our neighbour will allow.
	 */
	if(_highArena && ((((uintptr_t)_highArena->getLowAddress()) - ((uintptr_t)_highAddress)) < expandSize)) {
		uintptr_t neighbourContractSize = expandSize - (((uintptr_t)_highArena->getLowAddress()) - ((uintptr_t)_highAddress));
		assume0(neighbourContractSize % env->getExtensions()->heapAlignment == 0);

		/* Contract the neighbour by the amount needed to achieve the request expand size.
		 * Note that the contract may not get back everything we've asked for.
		 */
		env->getExtensions()->heap->getResizeStats()->setLastContractReason(SATISFY_EXPAND);
		_highArena->getSubSpace()->contract(env, neighbourContractSize);

		/* Adjust the expand amount if the contract didn't net what was required */
		if((((uintptr_t)_highArena->getLowAddress()) - ((uintptr_t)_highAddress)) < expandSize) {
			expandSize = ((uintptr_t)_highArena->getLowAddress()) - ((uintptr_t)_highAddress);
		}
	}

	/* Adjust the expand size to meet the maximum expand size allowed */
	expandSize = OMR_MIN(_subSpace->maxExpansionInSpace(env), expandSize);

	/* Check if the requested expand size fits into the current arena (sanity check) */
	if(!_subSpace->canExpand(env, expandSize)) {
		return 0;
	}

	/* Ask the governing physical arena (associated to the memory space) if the expand request is ok (sanity check) */
	if(!((MM_PhysicalArenaVirtualMemory *)_parent)->canExpand(env, this, _highAddress, expandSize)) {
		/* Parent is responsible for setting the expand error code */
		return 0;
	}

	/* Complete the expand */
	expandNoCheck(env, expandSize);

	return expandSize;
}

/**
 * Expand the receiver by the given size.
 * Expand by the given size with no safety checks - checking we've assured that the expand size will in fact fit and can
 * be properly distributed within the receivers parameters.
 * @return the size of the expansion (which should match the size supplied)
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemoryFlat::expandNoCheck(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	/* If we couldn't expand, how did we get here? */
	Assert_MM_true(((MM_PhysicalArenaVirtualMemory *)_parent)->canExpand(env, this, _highAddress, expandSize));
	Assert_MM_true(_lowAddress == _region->getLowAddress());
	Assert_MM_true(_highAddress == _region->getHighAddress());

	/* Find the expansion addresses */
	void *lowExpandAddress = _highAddress;
	void *highExpandAddress = (void *)(((uintptr_t)_highAddress) + expandSize);

	/* Get the heap memory */
	if(!_heap->commitMemory(lowExpandAddress, expandSize)) {
		return 0;
	}

	if (_highAddress != highExpandAddress) {
		/* the area has been expanded.  Update internal values */
		_highAddress = highExpandAddress;
		/* Reconstruct the region at its new size */
		getHeapRegionManager()->resizeAuxillaryRegion(env, _region, _lowAddress, _highAddress);
		Assert_MM_true(NULL != _region);

		/* Update the owning subspace */
		_subSpace->expanded(env, this, expandSize, lowExpandAddress, highExpandAddress, true);
		_subSpace->heapReconfigured(env);
	}

	Assert_MM_true(_lowAddress == _region->getLowAddress());
	Assert_MM_true(_highAddress == _region->getHighAddress());

	return expandSize;
}

/**
 * Determine whether the sub arena is allowed to contract
 *
 * @return true if contraction is allowed, false otherwise.
 */
bool
MM_PhysicalSubArenaVirtualMemoryFlat::canContract(MM_EnvironmentBase *env)
{
	return _resizable;
}

/**
 * Contract the sub arenas physical memory by at least the size specified.
 * @note the contraction size can only be reduced from the value requested (space restrictions have already been imposed).
 * @return The amount of heap actually contracted.
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemoryFlat::contract(MM_EnvironmentBase *env, uintptr_t requestContractSize)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	uintptr_t contractSize = requestContractSize;
	/* Get the memory subspace associated with the contract */
	MM_MemorySubSpace *genericSubSpace = _region->getSubSpace();
	void *oldLowAddress = _region->getLowAddress();
	void *oldHighAddress = _region->getHighAddress();

	Assert_MM_true(contractSize  % extensions->heapAlignment == 0);
	Assert_MM_true(_lowAddress == oldLowAddress);
	Assert_MM_true(_highAddress == oldHighAddress);

	/* Find the physical number of bytes that we could possibly contract by */
	contractSize = OMR_MIN(contractSize, ((MM_PhysicalArenaVirtualMemory *)_parent)->getPhysicalMaximumContractSizeLow(env, _highAddress));

	/* Find the contract range available */
	uint8_t *contractBase = (uint8_t *)genericSubSpace->findFreeEntryEndingAtAddr(env, oldHighAddress);
	uint8_t *contractTop = (uint8_t *)oldHighAddress;
	uintptr_t contractSizeAvailable = ((uintptr_t)contractTop) - ((uintptr_t)contractBase);

	/* Calculate the actual physical memory available for contract */
	contractSize = OMR_MIN(contractSizeAvailable, contractSize);
	contractSize = MM_Math::roundToFloor(env->getExtensions()->heapAlignment, contractSize);
	contractSize = MM_Math::roundToFloor(getHeapRegionManager()->getRegionSize(), contractSize);

	if (0 == contractSize) {
		return 0;
	}

	/* Record the virtual addresses for the contraction for counter balancing purposes */
	setVirtualHighAddress((void *)(contractTop - contractSize));

	/* Inform the subspace lineage that a contract will occur, and to react accordingly.  this may
	 * adjust the size of the contract.
	 */
	contractSize = _subSpace->counterBalanceContract(env, contractSize, extensions->heapAlignment);
	Assert_MM_true(0 == (contractSize % extensions->heapAlignment));

	/* Clear the virtual addresses used for counter balancing */
	clearVirtualAddresses();

	/* If the contract size was reduced to 0, we cannot contract */
	if(0 == contractSize) {
		return 0;
	}

	/* Based on the decided contract size, calculate the range of heap to be contracted */
	contractBase = contractTop - contractSize;

	/* Determine the surrounding valid memory addresses of the contract for decommit purposes */
	void *lowValidAddress = (void *)contractBase;
	void *highValidAddress = findAdjacentHighValidAddress(env);

	/* Remove the range from the free list (must do this before decommiting */
	genericSubSpace->removeExistingMemory(env, this, contractSize, (void *)contractBase, (void *)contractTop);

	/* Everything is ok - decommit the memory */
	_heap->decommitMemory((void *)contractBase, contractSize, lowValidAddress, highValidAddress);

	/* Success - the area has been contracted.  Update internal values */
	_highAddress = (void *)contractBase;
	getHeapRegionManager()->resizeAuxillaryRegion(env, _region, _lowAddress, _highAddress);
	Assert_MM_true(NULL != _region);

	/* Broadcast that heap has been removed */
	genericSubSpace->heapRemoveRange(env, _subSpace, contractSize, (void *)contractBase, (void *)contractTop, lowValidAddress, highValidAddress);
	genericSubSpace->heapReconfigured(env);

	/* Execute any pending counter balances to the contract that have been enqueued */
	_subSpace->triggerEnqueuedCounterBalancing(env);

	Assert_MM_true(_lowAddress == _region->getLowAddress());
	Assert_MM_true(_highAddress == _region->getHighAddress());

	return contractSize;
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
MM_PhysicalSubArenaVirtualMemoryFlat::checkCounterBalanceExpand(MM_EnvironmentBase *env, uintptr_t expandSizeDeltaAlignment, uintptr_t expandSize)
{
	uintptr_t adjustedExpandSize;
	uintptr_t physicalMaximumExpandSize;

	/* Record the expand size */
	adjustedExpandSize = expandSize;

	/* Find the upper physical limit to an expand */
	uintptr_t physicalLimitHighAddress;
	if(NULL != _highArena) {
		physicalLimitHighAddress = (uintptr_t)_highArena->getVirtualLowAddress();
	} else {
		physicalLimitHighAddress = (uintptr_t)((MM_PhysicalArenaVirtualMemory *)_parent)->getHighAddress();
	}
	physicalMaximumExpandSize = physicalLimitHighAddress - ((uintptr_t)_highAddress);

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

	/* Found a valid expand size, return */
	assume0(adjustedExpandSize % env->getExtensions()->heapAlignment == 0);
	return adjustedExpandSize;
}

/**
 * @copydoc MM_PhysicalSubArena::::getAvailableContractionSize(MM_EnvironmentBase *, MM_MemorySubSpace *, MM_AllocateDescription *)
 */
uintptr_t
MM_PhysicalSubArenaVirtualMemoryFlat::getAvailableContractionSize(
	MM_EnvironmentBase *env,
	MM_MemorySubSpace *memorySubSpace,
	MM_AllocateDescription *allocDescription)
{
	return memorySubSpace->getAvailableContractionSizeForRangeEndingAt(env, allocDescription, getLowAddress(), getHighAddress());
}
