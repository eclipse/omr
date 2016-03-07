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

#if !defined(COLLECTIONSTATISTICSSTANDARD_HPP_)
#define COLLECTIONSTATISTICSSTANDARD_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "Base.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "CollectionStatistics.hpp"

#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "MemoryPool.hpp"
/**
 * A collection of interesting statistics for the Heap.
 * @ingroup GC_Stats
 */
class MM_CollectionStatisticsStandard : public MM_CollectionStatistics
{
private:
protected:
public:
	uintptr_t _totalTenureHeapSize; /**< Total active tenure heap size */
	uintptr_t _totalFreeTenureHeapSize; /**< Total active free tenure heap size */
	bool _loaEnabled; /**< Is the large object area enabled */
	uintptr_t _totalLOAHeapSize; /**< Total active LOA heap size */
	uintptr_t _totalFreeLOAHeapSize; /**< Total active free LOA heap size */
	bool _scavengerEnabled; /**< Is the scavenger enabled */
	uintptr_t _totalNurseryHeapSize; /**< Total active nursery (survivor + allocate) heap size */
	uintptr_t _totalFreeNurseryHeapSize; /**< Total active free nursery heap size */
	uintptr_t _totalSurvivorHeapSize; /**< Total active survivor heap size */
	uintptr_t _totalFreeSurvivorHeapSize; /**< Total active free survivor heap size */
	uintptr_t _rememberedSetCount; /**< remembered set count */
	bool _tenureFragmentation;
	uintptr_t _microFragmentedSize; /**< */
	uintptr_t _macroFragmentedSize; /**< */
private:
protected:
public:

	static MM_CollectionStatisticsStandard *
	getCollectionStatistics(MM_CollectionStatistics *stats)
	{
		return (MM_CollectionStatisticsStandard *)stats;
	}

	MMINLINE void
	collectCollectionStatistics(MM_EnvironmentBase *env, MM_CollectionStatisticsStandard *stats)
	{
		MM_GCExtensionsBase *extensions = env->getExtensions();

		stats->_totalHeapSize = extensions->heap->getActiveMemorySize();
		stats->_totalFreeHeapSize = extensions->heap->getApproximateActiveFreeMemorySize();

		stats->_totalTenureHeapSize = extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD);
		stats->_totalFreeTenureHeapSize = extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);

#if defined(OMR_GC_LARGE_OBJECT_AREA)
		stats->_loaEnabled = extensions->largeObjectArea;
#else
		stats->_loaEnabled = false;
#endif /* OMR_GC_LARGE_OBJECT_AREA */
		if (stats->_loaEnabled) {
			stats->_totalLOAHeapSize = extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD);
			stats->_totalFreeLOAHeapSize = extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD);
		} else {
			stats->_totalLOAHeapSize = 0;
			stats->_totalFreeLOAHeapSize = 0;
		}

#if defined(OMR_GC_MODRON_SCAVENGER)
		stats->_scavengerEnabled = extensions->scavengerEnabled;
#else
		stats->_scavengerEnabled = false;
#endif /* OMR_GC_MODRON_SCAVENGER */
		if (stats->_scavengerEnabled) {
			stats->_totalNurseryHeapSize = extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW);
			stats->_totalFreeNurseryHeapSize = extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW);
			stats->_totalSurvivorHeapSize = extensions->heap->getActiveSurvivorMemorySize(MEMORY_TYPE_NEW);
			stats->_totalFreeSurvivorHeapSize = extensions->heap->getApproximateActiveFreeSurvivorMemorySize(MEMORY_TYPE_NEW);
			stats->_rememberedSetCount = extensions->getRememberedCount();
		} else {
			stats->_totalNurseryHeapSize = 0;
			stats->_totalFreeNurseryHeapSize = 0;
			stats->_totalSurvivorHeapSize = 0;
			stats->_totalFreeSurvivorHeapSize = 0;
			stats->_rememberedSetCount = 0;
		}

		if (_tenureFragmentation) {
			MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
			MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
			MM_LargeObjectAllocateStats* allocateStats = tenureMemorySubspace->getLargeObjectAllocateStats();

			stats->_microFragmentedSize = tenureMemorySubspace->getMemoryPool()->getDarkMatterBytes();
			stats->_macroFragmentedSize = allocateStats->getRemainingFreeMemoryAfterEstimate();

		} else {
			stats->_microFragmentedSize = 0;
			stats->_macroFragmentedSize = 0;
		}
	}

	/* Reset both Macro and Micro Fragmentation Stats after compact */
	MMINLINE void
	resetFragmentionStats(MM_EnvironmentBase *env) {
		MM_GCExtensionsBase *extensions = env->getExtensions();
		MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
		MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
		MM_LargeObjectAllocateStats* allocateStats = tenureMemorySubspace->getLargeObjectAllocateStats();

		tenureMemorySubspace->getMemoryPool()->resetDarkMatterBytes();
		allocateStats->resetRemainingFreeMemoryAfterEstimate();
		_microFragmentedSize = 0;
		_macroFragmentedSize = 0;
	}

	/**
	 * Create a HeapStats object.
	 */
	MM_CollectionStatisticsStandard() :
		MM_CollectionStatistics()
		,_totalTenureHeapSize(0)
		,_totalFreeTenureHeapSize(0)
		,_loaEnabled(false)
		,_totalLOAHeapSize(0)
		,_totalFreeLOAHeapSize(0)
		,_scavengerEnabled(false)
		,_totalNurseryHeapSize(0)
		,_totalFreeNurseryHeapSize(0)
		,_totalSurvivorHeapSize(0)
		,_totalFreeSurvivorHeapSize(0)
		,_rememberedSetCount(0)
		, _tenureFragmentation(false)
		, _microFragmentedSize(0)
		, _macroFragmentedSize(0)
	{};
};

#endif /* COLLECTIONSTATISTICSSTANDARD_HPP_ */

