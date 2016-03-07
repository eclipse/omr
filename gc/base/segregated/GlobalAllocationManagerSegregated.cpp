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

#include "omrport.h"
#include "ModronAssertions.h"

#include "AllocationContextSegregated.hpp"
#include "EnvironmentBase.hpp"
#include "RegionPoolSegregated.hpp"
#include "SweepSchemeSegregated.hpp"

#include "GlobalAllocationManagerSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

MM_GlobalAllocationManagerSegregated*
MM_GlobalAllocationManagerSegregated::newInstance(MM_EnvironmentBase *env, MM_RegionPoolSegregated *regionPool)
{
	MM_GlobalAllocationManagerSegregated *allocationManager = (MM_GlobalAllocationManagerSegregated *)env->getForge()->allocate(sizeof(MM_GlobalAllocationManagerSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (allocationManager) {
		allocationManager = new(allocationManager) MM_GlobalAllocationManagerSegregated(env);
		if (!allocationManager->initialize(env, regionPool)) {
			allocationManager->kill(env);
			allocationManager = NULL;
		}
	}
	return allocationManager;
}

void
MM_GlobalAllocationManagerSegregated::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_GlobalAllocationManagerSegregated::initialize(MM_EnvironmentBase *env, MM_RegionPoolSegregated *regionPool)
{
	bool result = MM_GlobalAllocationManager::initialize(env);
	_regionPool = regionPool;
	if (result) {
		_managedAllocationContextCount = _extensions->managedAllocationContextCount;
		if (0 == _managedAllocationContextCount) {
			OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
			uintptr_t desiredAllocationContextCount = 2 * omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);
			uintptr_t regionCount = _extensions->memoryMax / _extensions->regionSize;
			/* heuristic -- ACs are permitted to waste up to 1/8th of the heap in slack regions. This number may need to be adjusted */ 
			uintptr_t maxAllocationContextCount = regionCount / 8;
			_managedAllocationContextCount = OMR_MAX(1, OMR_MIN(desiredAllocationContextCount, maxAllocationContextCount));
		}

		result = initializeAllocationContexts(env, regionPool);
	}

	return result;
}

void
MM_GlobalAllocationManagerSegregated::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _managedAllocationContexts) {
		for (uintptr_t i = 0; i < _managedAllocationContextCount; i++) {
			if (NULL != _managedAllocationContexts[i]) {
				_managedAllocationContexts[i]->kill(env);
				_managedAllocationContexts[i] = NULL;
			}
		}

		env->getForge()->free(_managedAllocationContexts);
		_managedAllocationContexts = NULL;
	}
	MM_GlobalAllocationManager::tearDown(env);
}

MM_AllocationContextSegregated *
MM_GlobalAllocationManagerSegregated::createAllocationContext(MM_EnvironmentBase * env, MM_RegionPoolSegregated *regionPool)
{
	return MM_AllocationContextSegregated::newInstance(env, this, regionPool);
}

void
MM_GlobalAllocationManagerSegregated::setSweepScheme(MM_SweepSchemeSegregated *sweepScheme)
{
	_regionPool->setSweepScheme(sweepScheme);
}

void
MM_GlobalAllocationManagerSegregated::setMarkingScheme(MM_SegregatedMarkingScheme *markingScheme)
{
	for (uintptr_t i = 0; i < _managedAllocationContextCount; i++) {
		((MM_AllocationContextSegregated *) _managedAllocationContexts[i])->setMarkingScheme(markingScheme);
	}
}

bool
MM_GlobalAllocationManagerSegregated::initializeAllocationContexts(MM_EnvironmentBase * env, MM_RegionPoolSegregated *regionPool)
{
	Assert_MM_true(0 != _managedAllocationContextCount);

	MM_AllocationContextSegregated **contexts = (MM_AllocationContextSegregated **)env->getForge()->allocate(sizeof(MM_AllocationContextSegregated*) * _managedAllocationContextCount, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == contexts) {
		return false;
	}
	_managedAllocationContexts = (MM_AllocationContext **) contexts;
	memset(contexts, 0, sizeof(MM_AllocationContextSegregated *) * _managedAllocationContextCount);

	for (uintptr_t i = 0; i < _managedAllocationContextCount; i++) {
		if (NULL == (contexts[i] = createAllocationContext(env, regionPool))) {
			return false;
		}
	}
	return true;
}

bool
MM_GlobalAllocationManagerSegregated::acquireAllocationContext(MM_EnvironmentBase *env)
{
	if (env->getAllocationContext() == NULL) {
		uintptr_t allocationContextIndex = _nextAllocationContext++;
		allocationContextIndex %= _managedAllocationContextCount;
		MM_AllocationContextSegregated *ac = (MM_AllocationContextSegregated *)_managedAllocationContexts[allocationContextIndex];
		ac->enter(env);
		env->setAllocationContext(ac);
	}
	return false;
}

void
MM_GlobalAllocationManagerSegregated::releaseAllocationContext(MM_EnvironmentBase *env)
{
	MM_AllocationContextSegregated *ac = (MM_AllocationContextSegregated *) env->getAllocationContext();
	if (ac != NULL) {
		ac->exit(env);
		env->setAllocationContext(NULL);
	}
}

void
MM_GlobalAllocationManagerSegregated::flushCachedFullRegions(MM_EnvironmentBase *env)
{
	Assert_MM_true(_managedAllocationContextCount > 0);

	for (uintptr_t i = 0; i < _managedAllocationContextCount; i++) {
		((MM_AllocationContextSegregated *)(_managedAllocationContexts[i]))->returnFullRegionsToRegionPool(env);
	}
}

#endif /* OMR_GC_SEGREGATED_HEAP */
