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

#include "omrcfg.h"
#include "mmhook_common.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "VerboseWriterHook.hpp"



MM_VerboseWriterHook::MM_VerboseWriterHook(MM_EnvironmentBase *env) :
	MM_VerboseWriter(VERBOSE_WRITER_HOOK)
{
	/* no implementation */
}

/**
 * Create a new MM_VerboseWriterHook instance.
 * @return Pointer to the new MM_VerboseWriterHook.
 */
MM_VerboseWriterHook *
MM_VerboseWriterHook::newInstance(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	MM_VerboseWriterHook *agent = (MM_VerboseWriterHook *)extensions->getForge()->allocate(sizeof(MM_VerboseWriterHook), MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if (agent) {
		new(agent) MM_VerboseWriterHook(env);
		if(!agent->initialize(env)){
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseWriterHook instance.
 */
bool
MM_VerboseWriterHook::initialize(MM_EnvironmentBase *env)
{
	return MM_VerboseWriter::initialize(env);
}

/**
 */
void
MM_VerboseWriterHook::endOfCycle(MM_EnvironmentBase *env)
{
}

/**
 * Closes the agents output stream.
 */
void
MM_VerboseWriterHook::closeStream(MM_EnvironmentBase *env)
{
	/* Should this force an event with "</verbosegc>"? */ 
}

void
MM_VerboseWriterHook::outputString(MM_EnvironmentBase *env, const char* string)
{
	OMR_VMThread* vmThread = env->getOmrVMThread();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	
	/* Call the hook */
	TRIGGER_J9HOOK_MM_OMR_VERBOSE_GC_OUTPUT(
		extensions->omrHookInterface,
		vmThread,
		omrtime_hires_clock(),
		string);
}

