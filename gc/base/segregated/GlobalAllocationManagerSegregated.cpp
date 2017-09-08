/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
		if (NULL != ac) {
			ac->enter(env);
			env->setAllocationContext(ac);
			return true;
		}
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
