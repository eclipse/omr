/*******************************************************************************
 * Copyright (c) 2016 IBM Corporation
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    IBM Corporation - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "omrcfg.h"

#if defined(OMR_GC_CONCURRENT_SCAVENGER)

#include "omr/gc/modronAssertions.h"

#include "EnvironmentStandard.hpp"
#include "Scavenger.hpp"

#include "ConcurrentScavengeTask.hpp"

void
MM_ConcurrentScavengeTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);

	switch (_action) {
	case SCAVENGE_ALL:
		_collector->workThreadProcessRoots(env);
		_collector->workThreadScan(env);
		_collector->workThreadComplete(env);
		break;
	case SCAVENGE_ROOTS:
		_collector->workThreadProcessRoots(env);
		break;
	case SCAVENGE_SCAN:
		_collector->workThreadScan(env);
		break;
	case SCAVENGE_COMPLETE:
		_collector->workThreadComplete(env);
		break;
	default:
		Assert_MM_unreachable();
	}
}

#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
