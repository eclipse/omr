/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

