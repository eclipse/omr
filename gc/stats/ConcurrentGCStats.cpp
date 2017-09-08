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

#include "ConcurrentGCStats.hpp"
#include "EnvironmentBase.hpp"

const char*
MM_ConcurrentGCStats::getConcurrentStatusString(MM_EnvironmentBase *env, uintptr_t status, char *statusBuffer, uintptr_t statusBufferLength)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	switch (status) {
	case CONCURRENT_OFF:
		omrstr_printf(statusBuffer, statusBufferLength, "off");
		break;
	case CONCURRENT_INIT_RUNNING:
		omrstr_printf(statusBuffer, statusBufferLength, "init running");
		break;
	case CONCURRENT_INIT_COMPLETE:
		omrstr_printf(statusBuffer, statusBufferLength, "init complete");
		break;
	case CONCURRENT_ROOT_TRACING:
		omrstr_printf(statusBuffer, statusBufferLength, "root tracing");
		break;
	case CONCURRENT_TRACE_ONLY:
		omrstr_printf(statusBuffer, statusBufferLength, "trace only");
		break;
	case CONCURRENT_CLEAN_TRACE:
		omrstr_printf(statusBuffer, statusBufferLength, "clean trace");
		break;
	case CONCURRENT_EXHAUSTED:
		omrstr_printf(statusBuffer, statusBufferLength, "exhausted");
		break;
	case CONCURRENT_FINAL_COLLECTION:
		omrstr_printf(statusBuffer, statusBufferLength, "final collection");
		break;
	default:
		if (CONCURRENT_ROOT_TRACING < status) {
			omrstr_printf(statusBuffer, statusBufferLength, "root tracing + %lld", (status - (uintptr_t)CONCURRENT_ROOT_TRACING));
		} else {
			omrstr_printf(statusBuffer, statusBufferLength, "unknown");
		}
		break;
	}
	statusBuffer[statusBufferLength - 1] = 0;
	return (const char *)statusBuffer;
}
