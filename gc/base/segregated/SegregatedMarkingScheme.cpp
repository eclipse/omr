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

#include "EnvironmentBase.hpp"

#include "SegregatedMarkingScheme.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_SegregatedMarkingScheme *
MM_SegregatedMarkingScheme::newInstance(MM_EnvironmentBase *env)
{
	MM_SegregatedMarkingScheme *instance;
	
	instance = (MM_SegregatedMarkingScheme *)env->getForge()->allocate(sizeof(MM_SegregatedMarkingScheme), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (instance) {
		new(instance) MM_SegregatedMarkingScheme(env);
		if (!instance->initialize(env)) { 
			instance->kill(env);
			instance = NULL;
		}
	}

	return instance;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_SegregatedMarkingScheme::kill(MM_EnvironmentBase *env)
{
	tearDown(env); 
	env->getForge()->free(this);
}

#endif /* OMR_GC_SEGREGATED_HEAP */
