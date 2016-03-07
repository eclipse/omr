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

#include "SweepPoolManagerAddressOrderedList.hpp"

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_SweepPoolManagerAddressOrderedList *
MM_SweepPoolManagerAddressOrderedList::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepPoolManagerAddressOrderedList *sweepPoolManager;

	sweepPoolManager = (MM_SweepPoolManagerAddressOrderedList *)env->getForge()->allocate(sizeof(MM_SweepPoolManagerAddressOrderedList), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sweepPoolManager) {
		new(sweepPoolManager) MM_SweepPoolManagerAddressOrderedList(env);
		if (!sweepPoolManager->initialize(env)) {
			sweepPoolManager->kill(env);
			sweepPoolManager = NULL;
		}
	}

	return sweepPoolManager;
}
