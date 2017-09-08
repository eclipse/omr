/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#include "omrcfg.h"

#if defined(OMR_GC_CONCURRENT_SCAVENGER)

#include "ModronAssertions.h"

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
