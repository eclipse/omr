/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "EvacuatorBase.hpp"

#if defined(EVACUATOR_DEBUG)
void
MM_EvacuatorBase::setDebugFlags(uint64_t debugFlags)
{
	if (EVACUATOR_DEBUG_DEFAULT_FLAGS == debugFlags) {
		const char* debugFlagsValue = getenv("EVACUATOR_DEBUG_FLAGS");
		if (NULL != debugFlagsValue) {
			debugFlags = (uint64_t)atol(debugFlagsValue);
		}
	}
	_debugCycle = (uintptr_t)(debugFlags >> 50);
	_debugEpoch = (uintptr_t)((debugFlags >> 34) & (uint64_t)0x3f);
	_debugTrace = (uintptr_t)((debugFlags >> 32) & (uint64_t)0x3);
	_debugFlags = (uintptr_t)(debugFlags & (uint64_t)0xffffffff);
}

const char *
MM_EvacuatorBase::callsite(const char *id) {
	const char *callsite = strrchr(id, '/');
	if (NULL == callsite) {
		callsite = strrchr(id, '\\');
		if (NULL == callsite) {
			callsite = id;
		}
	}
	if ((NULL != callsite) && (('/' == *callsite) || ('\\' == *callsite))) {
		callsite += 1;
	}
	return callsite;
}
#endif /* defined(EVACUATOR_DEBUG) */
