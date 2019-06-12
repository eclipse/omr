/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#define J9_EXTERNAL_TO_VM

#include "mmprivatehook.h"
#include "modronbase.h"
#include "modronopt.h"
#include "ModronAssertions.h"
#include "omr.h"

#include <string.h>

#include "ConcurrentGCIncrementalUpdate.hpp"
#include "ConcurrentCardTableForWC.hpp"

/**
 * Create new instance of ConcurrentGCIncrementalUpdate object.
 *
 * @return Reference to new MM_ConcurrentGCIncrementalUpdate object or NULL
 */
MM_ConcurrentGCIncrementalUpdate *
MM_ConcurrentGCIncrementalUpdate::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentGCIncrementalUpdate *concurrentGC =
			(MM_ConcurrentGCIncrementalUpdate *)env->getForge()->allocate(sizeof(MM_ConcurrentGCIncrementalUpdate),
					OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != concurrentGC) {
		new(concurrentGC) MM_ConcurrentGCIncrementalUpdate(env);
		if (!concurrentGC->initialize(env)) {
			concurrentGC->kill(env);
			concurrentGC = NULL;
		}
	}

	return concurrentGC;
}

/**
 * Destroy instance of an ConcurrentGCIncrementalUpdate object.
 *
 */
void
MM_ConcurrentGCIncrementalUpdate::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}


/* Teardown a ConcurrentGCIncrementalUpdate object
* Destroy referenced objects and release
* all allocated storage before ConcurrentGCIncrementalUpdate object is freed.
*/
void
MM_ConcurrentGCIncrementalUpdate::tearDown(MM_EnvironmentBase *env)
{
	/* ..and then tearDown our super class */
	MM_ConcurrentGC::tearDown(env);
}

/**
 * Initialize a new ConcurrentGCIncrementalUpdate object.
 * Instantiate card table and concurrent RAS objects(if required) and initialize all
 * monitors required by concurrent. Allocate and initialize the concurrent helper thread
 * table.
 *
 * @return TRUE if initialization completed OK;FALSE otheriwse
 */
bool
MM_ConcurrentGCIncrementalUpdate::initialize(MM_EnvironmentBase *env)
{
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);

	if (!MM_ConcurrentGC::initialize(env)) {
		goto error_no_memory;
	}

	if (!createCardTable(env)) {
		goto error_no_memory;
	}

	/* Register on any hook we are interested in */
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CARD_CLEANING_PASS_2_START, hookCardCleanPass2Start, OMR_GET_CALLSITE(), (void *)this);

	return true;

	error_no_memory:
	return false;
}

bool
MM_ConcurrentGCIncrementalUpdate::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool clearCards = (CONCURRENT_OFF < _stats.getExecutionMode()) && subspace->isConcurrentCollectable();

	/* Expand any superclass structures including mark bits*/
	bool result = MM_ConcurrentGC::heapAddRange(env, subspace, size, lowAddress, highAddress);

	if (result) {
		/* expand the card table */
		result = ((MM_ConcurrentCardTable *)_cardTable)->heapAddRange(env, subspace, size, lowAddress, highAddress, clearCards);
		if (!result) {
			/* Expansion of Concurrent Card Table has failed
			 * ParallelGlobalGC expansion must be reversed
			 */
			MM_ParallelGlobalGC::heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
		}
	}

	_heapAlloc = _extensions->heap->getHeapTop();

	return result;
}

/**
 * Hook function called when an the 2nd pass over card table to clean cards starts.
 * This is a wrapper into the non-static MM_ConcurrentGCIncrementalUpdate::recordCardCleanPass2Start
 */
void
MM_ConcurrentGCIncrementalUpdate::hookCardCleanPass2Start(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_CardCleanPass2StartEvent* event = (MM_CardCleanPass2StartEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_ConcurrentGCIncrementalUpdate *)userData)->recordCardCleanPass2Start(env);

	/* Boost the trace rate for the 2nd card cleaning pass */
}

void
MM_ConcurrentGCIncrementalUpdate::recordCardCleanPass2Start(MM_EnvironmentBase *env)
{
	_pass2Started = true;

	/* Record how mush work we did before pass 2 KO */
	_totalTracedAtPass2KO = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();
	_totalCleanedAtPass2KO = _stats.getCardCleanCount() + _stats.getConHelperCardCleanCount();

	/* ..and boost tracing rate from here to end of cycle so we complete pass 2 ASAP */
	_allocToTraceRate *= _allocToTraceRateCardCleanPass2Boost;
}

bool
MM_ConcurrentGCIncrementalUpdate::createCardTable(MM_EnvironmentBase *env)
{
	bool result = false;

	Assert_MM_true(NULL == _cardTable);
	Assert_MM_true(NULL == _extensions->cardTable);

#if (HOST_OS == OMR_AIX) || (HOST_OS == OMR_LINUX)
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if ((uintptr_t)omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE) > 1 ) {
		_cardTable = MM_ConcurrentCardTableForWC::newInstance(env, _extensions->getHeap(), _markingScheme, this);
	} else
#endif /* AIXPPC || LINUXPPC */
	{
		_cardTable = MM_ConcurrentCardTable::newInstance(env, _extensions->getHeap(), _markingScheme, this);
	}

	if(NULL != _cardTable) {
		result = true;
		/* Set card table address in GC Extensions */
		_extensions->cardTable = _cardTable;
	}

	return result;
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
