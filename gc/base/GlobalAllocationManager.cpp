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
