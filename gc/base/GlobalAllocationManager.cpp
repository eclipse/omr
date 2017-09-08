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

#include "GlobalAllocationManager.hpp"

#include "AllocationContext.hpp"
/**

 * Initialize GAM
 */
bool
MM_GlobalAllocationManager::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Tear down a GAM instance
 */
void
MM_GlobalAllocationManager::tearDown(MM_EnvironmentBase *env)
{
}

/**
 * Flush all allocation contexts such that the cells allocated to them becomes safe for traversal.
 */
void
MM_GlobalAllocationManager::flushAllocationContexts(MM_EnvironmentBase *env)
{
	Assert_MM_true(_managedAllocationContextCount > 0);
	for (uintptr_t i = 0; i < _managedAllocationContextCount; i++) {
		_managedAllocationContexts[i]->flush(env);
	}
}

void 
MM_GlobalAllocationManager::flushAllocationContextsForShutdown(MM_EnvironmentBase *env)
{
	Assert_MM_true(_managedAllocationContextCount > 0);
	if (NULL != _managedAllocationContexts) {
		for (uintptr_t i = 0; i < _managedAllocationContextCount; i++) {
			if (NULL != _managedAllocationContexts[i]) {
				_managedAllocationContexts[i]->flushForShutdown(env);
			}
		}
	}
}
