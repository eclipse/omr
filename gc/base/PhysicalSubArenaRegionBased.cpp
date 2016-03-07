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


#include "PhysicalSubArenaRegionBased.hpp"

#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "PhysicalArenaRegionBased.hpp"
#include "MemorySubSpace.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManagerTarok.hpp"
#include "PhysicalArenaVirtualMemory.hpp"

#include "PhysicalArenaRegionBased.hpp"

MM_PhysicalSubArenaRegionBased::MM_PhysicalSubArenaRegionBased(MM_Heap *heap)
	: MM_PhysicalSubArena(heap)
	, _nextSubArena(NULL)
#if defined (OMR_GC_VLHGC)
	, _affinityLeaders(NULL)
#endif /* defined (OMR_GC_VLHGC) */
	, _affinityLeaderCount(0)
	, _nextNUMAIndex(0)
	,_extensions(NULL)
{
	_typeId = __FUNCTION__;
}

MM_PhysicalSubArenaRegionBased *
MM_PhysicalSubArenaRegionBased::newInstance(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_PhysicalSubArenaRegionBased *arena = (MM_PhysicalSubArenaRegionBased *)env->getForge()->allocate(sizeof(MM_PhysicalSubArenaRegionBased), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (arena) {
		new(arena) MM_PhysicalSubArenaRegionBased(heap);
		if(!arena->initialize(env)) {
			arena->kill(env);
			return NULL;
		}
	}
	return arena;
}

bool 
MM_PhysicalSubArenaRegionBased::initialize(MM_EnvironmentBase *env)
{
	if (!MM_PhysicalSubArena::initialize(env)) {
		return false;
	}

	_extensions = env->getExtensions();
	if (_extensions->isVLHGC()) {
#if defined (OMR_GC_VLHGC)	
		_affinityLeaders = _extensions->_numaManager.getAffinityLeaders(&_affinityLeaderCount);
#endif /* defined (OMR_GC_VLHGC) */		
	}

	return true;
}

void
MM_PhysicalSubArenaRegionBased::tearDown(MM_EnvironmentBase *env)
{
	MM_PhysicalSubArena::tearDown(env);
}

bool
MM_PhysicalSubArenaRegionBased::inflate(MM_EnvironmentBase *env)
{
	return _parent->attachSubArena(env, this, _subSpace->getInitialSize(), modron_pavm_attach_policy_none);
}

uintptr_t 
MM_PhysicalSubArenaRegionBased::getNextNumaNode()
{
	uintptr_t result = 0;

	if (_extensions->isVLHGC()) {
#if defined (OMR_GC_VLHGC)
		if (_nextNUMAIndex < _affinityLeaderCount) {
			result = _affinityLeaders[_nextNUMAIndex].j9NodeNumber;
		}

		if (result > 0) {
			_nextNUMAIndex = (_nextNUMAIndex + 1) % _affinityLeaderCount;
		}
#endif /* defined (OMR_GC_VLHGC) */
	}

	return result;
}

uintptr_t 
MM_PhysicalSubArenaRegionBased::getPreviousNumaNode()
{
	uintptr_t result = 0;

	if (_extensions->isVLHGC()) {
#if defined (OMR_GC_VLHGC)
		if (_affinityLeaderCount > 0) {
			_nextNUMAIndex = (_nextNUMAIndex + _affinityLeaderCount - 1) % _affinityLeaderCount;

			if (_nextNUMAIndex < _affinityLeaderCount) {
				result = _affinityLeaders[_nextNUMAIndex].j9NodeNumber;
			}
		}
#endif /* defined (OMR_GC_VLHGC) */
	}

	return result;
}


/*
 * Perform the expansion and associate the expanded regions with the subspace
 */
uintptr_t
MM_PhysicalSubArenaRegionBased::doContractInSubSpace(MM_EnvironmentBase *env, uintptr_t contractSize, MM_MemorySubSpace *subspace)
{
	uintptr_t didContractBy = 0;
	MM_HeapRegionManagerTarok *manager = MM_HeapRegionManagerTarok::getHeapRegionManager(_heap);
	uintptr_t regionSize = manager->getRegionSize();

	/* we contract one region size at a time */
	while (didContractBy < contractSize) {
		/* always contract from the most recent NUMA index first and work backwards */
		uintptr_t formerNodeIndex = _nextNUMAIndex;
		uintptr_t numaIndex = getPreviousNumaNode(); 
		MM_HeapRegionDescriptor *regionToRelease = subspace->selectRegionForContraction(env, numaIndex);
		
		if (NULL == regionToRelease) {
			/* no more free regions to contract */
			_nextNUMAIndex = formerNodeIndex;
			break;
		}

		void *base = regionToRelease->getLowAddress();
		void *top = regionToRelease->getHighAddress();
		void *contractBase = subspace->removeExistingMemory(env, this, regionSize, base, top);
		Assert_MM_true(contractBase == regionToRelease->getLowAddress());
		manager->releaseTableRegions(env, regionToRelease);
		
		/* We set the low valid and high valid address to make sure 
		 * the rest of the collector structure doesn't incorrectly assume 
		 * that this region is at the edge of the heap.
		 */
		void *lowValidAddress = manager->findHighestValidAddressBelow(regionToRelease); 
		void *highValidAddress = manager->findLowestValidAddressAbove(regionToRelease);
		
		/* decommits the memory */
		_heap->decommitMemory(contractBase, regionSize, lowValidAddress, highValidAddress);
		
		void *contractTop = (void *)(((uintptr_t)contractBase) + regionSize);
		/* Broadcast that heap has been removed */
		subspace->heapRemoveRange(env, _subSpace, regionSize, contractBase, contractTop, lowValidAddress, highValidAddress);
		
		didContractBy += regionSize;
	}
	validateNumaSymmetry(env);
	subspace->heapReconfigured(env);

	return didContractBy;
}

/*
 * Perform the expandsion and associate the expanded regions with the subspace
 */
uintptr_t
MM_PhysicalSubArenaRegionBased::doExpandInSubSpace(MM_EnvironmentBase *env, uintptr_t expandSize, MM_MemorySubSpace *subspace)
{
	uintptr_t didExpandBy = 0;
	MM_HeapRegionManagerTarok *manager = MM_HeapRegionManagerTarok::getHeapRegionManager(_heap);
	uintptr_t regionSize = manager->getRegionSize();

	/* we expand one region size at a time */
	while (didExpandBy < expandSize) {
		uintptr_t formerNodeIndex = _nextNUMAIndex;
		uintptr_t numaNode = getNextNumaNode();
		MM_HeapRegionDescriptor *newRegion = manager->acquireSingleTableRegion(env, subspace, numaNode);

		if (NULL == newRegion) {
			_nextNUMAIndex = formerNodeIndex;
			break;
		}
		Assert_MM_true(newRegion->getNumaNode() == numaNode);
		if (!newRegion->allocateSupportingResources(env)
			|| !_heap->commitMemory(newRegion->getLowAddress(), regionSize)
			|| !subspace->expanded(env, this, newRegion, false)
			) {
			/* if we fail to commit, do not fail silently, bail instead */
			manager->releaseTableRegions(env, newRegion);
			_nextNUMAIndex = formerNodeIndex;
			break;
		}

		didExpandBy += regionSize;

		/* Ensures that expansion is single-threaded */
		Assert_MM_true((0 == _affinityLeaderCount) || ((formerNodeIndex + 1) % _affinityLeaderCount) == _nextNUMAIndex);
	}
	validateNumaSymmetry(env);
	subspace->heapReconfigured(env);

	return didExpandBy;
}

uintptr_t
MM_PhysicalSubArenaRegionBased::performExpand(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	uintptr_t result = 0;
	
	if (((MM_PhysicalArenaRegionBased *)_parent)->canResize(env, this, expandSize)) {
		MM_MemorySubSpace *subSpace = getSubSpace();
		subSpace = (NULL == subSpace->getChildren()) ? subSpace : subSpace->getChildren();
		
		result = doExpandInSubSpace(env, expandSize, subSpace);
	}
	
	return result;
}

uintptr_t
MM_PhysicalSubArenaRegionBased::expand(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	return performExpand(env, expandSize);
}

uintptr_t 
MM_PhysicalSubArenaRegionBased::contract(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	if (((MM_PhysicalArenaRegionBased *)_parent)->canResize(env, this, expandSize)) {
		return doContractInSubSpace(env, expandSize, getSubSpace());
	}
	return 0;
}
	
bool 
MM_PhysicalSubArenaRegionBased::canContract(MM_EnvironmentBase *env)
{
	return _resizable;
}

uintptr_t 
MM_PhysicalSubArenaRegionBased::getAvailableContractionSize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace, MM_AllocateDescription *allocDescription)
{
	/* region based heap is discontiguous so we don't have the restriction that we must contract at the end of the subspace */
	return UDATA_MAX;
}

/**
 * Destroy and delete the instance.
 */
void
MM_PhysicalSubArenaRegionBased::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_PhysicalSubArenaRegionBased::validateNumaSymmetry(MM_EnvironmentBase *env)
{
	Trc_MM_PhysicalSubArenaRegionBased_validateNumaSymmetry_Entry(env->getLanguageVMThread());
	if (_extensions->isVLHGC()) {
#if defined (OMR_GC_VLHGC)	
		if (_extensions->tarokEnableExpensiveAssertions) {
			if (_affinityLeaderCount > 0) {
				uintptr_t currentNode = 0;
				uintptr_t currentCount = 0;
				uintptr_t highestCount = 0;
				uintptr_t lowestCount = UDATA_MAX;
				uintptr_t nodeCount = 0;

				MM_HeapRegionManagerTarok *manager = MM_HeapRegionManagerTarok::getHeapRegionManager(_heap);
				GC_HeapRegionIterator iterator(manager);
				MM_HeapRegionDescriptor *region = NULL;
				while (NULL != (region = iterator.nextRegion())) {
					if (region->getNumaNode() == currentNode) {
						currentCount += 1;
					} else {
						if (0 != currentNode) {
							highestCount = OMR_MAX(currentCount, highestCount);
							lowestCount = OMR_MIN(currentCount, lowestCount);
							Trc_MM_PhysicalSubArenaRegionBased_validateNumaSymmetry_NodeSummary(env->getLanguageVMThread(), currentCount, currentNode);
						}
						currentCount = 1;
						Assert_MM_true(region->getNumaNode() > currentNode);
						currentNode = region->getNumaNode();
						nodeCount += 1;
					}
				}
				highestCount = OMR_MAX(currentCount, highestCount);
				lowestCount = OMR_MIN(currentCount, lowestCount);
				Trc_MM_PhysicalSubArenaRegionBased_validateNumaSymmetry_NodeSummary(env->getLanguageVMThread(), currentCount, currentNode);
				Trc_MM_PhysicalSubArenaRegionBased_validateNumaSymmetry_TotalSummary(env->getLanguageVMThread(), highestCount, lowestCount, nodeCount, _affinityLeaderCount);
				Assert_MM_true(highestCount <= (lowestCount + 1));
				Assert_MM_true((nodeCount == _affinityLeaderCount) || (1 >= highestCount));
			}
		}
#endif /* defined (OMR_GC_VLHGC) */
	}
	Trc_MM_PhysicalSubArenaRegionBased_validateNumaSymmetry_Exit(env->getLanguageVMThread());
}
