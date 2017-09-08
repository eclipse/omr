/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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


#include "modronapicore.hpp"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

#include "pool_api.h"

#include "omrversionstrings.h"

extern "C" {
/**
 * Return the version string for the GC.
 * This string is immutable and is valid for the lifetime of the garbage collector module.
 * 
 * @parm[in] the javaVM
 * @return a GC version string (e.g. "20080103_AB")
 */
const char*
omrgc_get_version(OMR_VM *omrVM)
{
	return OMR_VERSION_STRING
#if defined (OMR_GC_COMPRESSED_POINTERS) 
		"_CMPRSS"
#endif /* OMR_GC_COMPRESSED_POINTERS */
	;
}

uintptr_t
omrgc_condYieldFromGC(OMR_VMThread *omrVMThread, uintptr_t componentType)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	return ((MM_GCExtensionsBase::getExtensions(env->getOmrVM())->dispatcher->condYieldFromGCWrapper(env, 0)) ? 1 : 0);
}

/**
 * Walk all J9ThreadMonitorTracing that are associated with each LightweightNonReentrantLock.
 * @param[in] vm, OMR_VM used to get gc extensions.
 * @param[in, out] state. If state->thePool is NULL, the first trace is returned
 * If non-NULL, the next trace is returned.
 * @return a pointer to a trace, or NULL if all traces walked
 *
 * @note Lock is obtain in the beginning of the walk and released at the end. Caller is expected to walk all traces.
 *
 */
void *
omrgc_walkLWNRLockTracePool(void *omrVM, pool_state *state)
{
	MM_GCExtensionsBase* gcExtensions = MM_GCExtensionsBase::getExtensions((OMR_VM *)omrVM);
	J9Pool* tracingPool = gcExtensions->_lightweightNonReentrantLockPool;
	J9ThreadMonitorTracing *lnrl_lock = NULL;

	if (NULL != tracingPool) {
		if (NULL == state->thePool) {
			omrthread_monitor_enter(gcExtensions->_lightweightNonReentrantLockPoolMutex);
			lnrl_lock = (J9ThreadMonitorTracing *) pool_startDo(tracingPool, state);
		} else {
			lnrl_lock = (J9ThreadMonitorTracing *) pool_nextDo(state);
		}
		if (NULL == lnrl_lock) {
			omrthread_monitor_exit(gcExtensions->_lightweightNonReentrantLockPoolMutex);
		}
	}

	return lnrl_lock;
}

} /* extern "C" */
