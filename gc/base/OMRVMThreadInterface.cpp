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


#include "omrcfg.h"

#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectAllocationInterface.hpp"

#include "OMRVMThreadInterface.hpp"

void
GC_OMRVMThreadInterface::flushCachesForWalk(MM_EnvironmentBase *env)
{
	env->_objectAllocationInterface->flushCache(env);
}

void
GC_OMRVMThreadInterface::flushNonAllocationCaches(MM_EnvironmentBase *env)
{
	env->flushNonAllocationCaches();

#if defined(OMR_GC_MODRON_SCAVENGER)
	if (env->getExtensions()->isStandardGC()) {
		((MM_EnvironmentStandard *)env)->flushRememberedSet();
	}
#endif
}

void
GC_OMRVMThreadInterface::flushCachesForGC(MM_EnvironmentBase *env)
{
	flushCachesForWalk(env);
	flushNonAllocationCaches(env);
}
