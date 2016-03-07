/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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

#include "ConcurrentSafepointCallback.hpp"

#include "EnvironmentStandard.hpp"
#include "ModronAssertions.h"

#if defined (OMR_GC_MODRON_CONCURRENT_MARK)

MM_ConcurrentSafepointCallback *
MM_ConcurrentSafepointCallback::newInstance(MM_EnvironmentStandard *env)
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
MM_ConcurrentSafepointCallback::registerCallback(MM_EnvironmentStandard *env, SafepointCallbackHandler handler, void *userData, bool cancelAfterGC)
#else
MM_ConcurrentSafepointCallback::registerCallback(MM_EnvironmentStandard *env, SafepointCallbackHandler handler, void *userData)
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
{
	/* To facilitate simple Card Table logic treat registerCallback as a no-op */
}

void
MM_ConcurrentSafepointCallback::requestCallback(MM_EnvironmentStandard *env)
{
	/* In the case of languages without safepoints, registerCallback should never be called because we're always at a safepoint */
	Assert_MM_unreachable();
}


void
MM_ConcurrentSafepointCallback::cancelCallback(MM_EnvironmentStandard *env)
{
	/* To facilitate simple Card Table logic treat cancelCallback as a no-op */
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
