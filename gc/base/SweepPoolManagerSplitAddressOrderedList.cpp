/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#include "SweepPoolManagerSplitAddressOrderedList.hpp"
#if defined(OMR_GC_MODRON_STANDARD)
/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_SweepPoolManagerSplitAddressOrderedList *
MM_SweepPoolManagerSplitAddressOrderedList::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepPoolManagerSplitAddressOrderedList *sweepPoolManager;
	
	sweepPoolManager = (MM_SweepPoolManagerSplitAddressOrderedList *)env->getForge()->allocate(sizeof(MM_SweepPoolManagerSplitAddressOrderedList), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sweepPoolManager) {
		new(sweepPoolManager) MM_SweepPoolManagerSplitAddressOrderedList(env);
		if (!sweepPoolManager->initialize(env)) { 
			sweepPoolManager->kill(env);        
			sweepPoolManager = NULL;            
		}                                       
	}

	return sweepPoolManager;
}

void
MM_SweepPoolManagerSplitAddressOrderedList::poolPostProcess(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool)
{
	memoryPool->postProcess(envModron, MM_MemoryPool::forSweep);
}

#endif /* defined(OMR_GC_MODRON_STANDARD) */
