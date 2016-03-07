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

#include "EnvironmentLanguageInterface.hpp"

void
MM_EnvironmentLanguageInterface::reportExclusiveAccessAcquire()
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	/* record statistics */
	U_64 meanResponseTime = _omrVM->exclusiveVMAccessStats.totalResponseTime / (_omrVM->exclusiveVMAccessStats.haltedThreads + 1); /* +1 for the requester */
	_exclusiveAccessTime = _omrVM->exclusiveVMAccessStats.endTime - _omrVM->exclusiveVMAccessStats.startTime;
	_meanExclusiveAccessIdleTime = _exclusiveAccessTime - meanResponseTime;
	_lastExclusiveAccessResponder = _omrVM->exclusiveVMAccessStats.lastResponder;
	_exclusiveAccessHaltedThreads = _omrVM->exclusiveVMAccessStats.haltedThreads;

	/* report hook */
	/* first the deprecated trigger */
	TRIGGER_J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS(_env->getExtensions()->privateHookInterface, _omrThread);
	/* now the new trigger */
	TRIGGER_J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_ACQUIRE(
			_env->getExtensions()->privateHookInterface,
			_omrThread,
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_ACQUIRE,
			_exclusiveAccessTime,
			_meanExclusiveAccessIdleTime,
			_lastExclusiveAccessResponder,
			_exclusiveAccessHaltedThreads);
}

void
MM_EnvironmentLanguageInterface::reportExclusiveAccessRelease()
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	TRIGGER_J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_RELEASE(
				_env->getExtensions()->privateHookInterface,
				_omrThread,
				omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_RELEASE);
}

