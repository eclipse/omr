/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#include "ConcurrentSafepointCallback.hpp"

#include "EnvironmentStandard.hpp"
#include "ModronAssertions.h"

#if defined (OMR_GC_MODRON_CONCURRENT_MARK)

MM_ConcurrentSafepointCallback *
MM_ConcurrentSafepointCallback::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentSafepointCallback *callback;

	callback = (MM_ConcurrentSafepointCallback *)env->getForge()->allocate(sizeof(MM_ConcurrentSafepointCallback), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != callback) {
		new(callback) MM_ConcurrentSafepointCallback(env);
	}
	return callback;
}

void
MM_ConcurrentSafepointCallback::kill(MM_EnvironmentBase *env)
{
	env->getForge()->free(this);
}

void
#if defined(AIXPPC) || defined(LINUXPPC)
MM_ConcurrentSafepointCallback::registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData, bool cancelAfterGC)
#else
MM_ConcurrentSafepointCallback::registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData)
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
{
	/* To facilitate simple Card Table logic treat registerCallback as a no-op */
}

void
MM_ConcurrentSafepointCallback::requestCallback(MM_EnvironmentStandard *env)
{
	/* In uncomplicated cases that always call the concurrent write barrier
	 * (MM_CollectorLanguageInterface::writeBarrierStore/Update()) this can
	 * be a no-op.
	 *
	 * To optimize card table maintenance, language can implement this and
	 * call MM_CollectorLanguageInterface::signalThreadsToDirtyCards() here.
	 */
}


void
MM_ConcurrentSafepointCallback::cancelCallback(MM_EnvironmentStandard *env)
{
	/* To facilitate simple Card Table logic treat cancelCallback as a no-op */
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
