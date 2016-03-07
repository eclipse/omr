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

#include "omrcfg.h"

#include "SweepPoolManagerHybrid.hpp"
#if defined(OMR_GC_MODRON_STANDARD)

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_SweepPoolManagerHybrid *
MM_SweepPoolManagerHybrid::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepPoolManagerHybrid *sweepPoolManager;
	
	sweepPoolManager = (MM_SweepPoolManagerHybrid *)env->getForge()->allocate(sizeof(MM_SweepPoolManagerHybrid), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sweepPoolManager) {
		new(sweepPoolManager) MM_SweepPoolManagerHybrid(env);
		if (!sweepPoolManager->initialize(env)) { 
			sweepPoolManager->kill(env);        
			sweepPoolManager = NULL;            
		}                                       
	}

	return sweepPoolManager;
}

#endif /* defined(OMR_GC_MODRON_STANDARD) */
