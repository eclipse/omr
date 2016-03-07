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

#include "VerboseWriterStreamOutput.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "VerboseBuffer.hpp"

#include <string.h>

MM_VerboseWriterStreamOutput::MM_VerboseWriterStreamOutput(MM_EnvironmentBase *env) :
	MM_VerboseWriter(VERBOSE_WRITER_STANDARD_STREAM)
{
	/* No implementation */
}

/**
 * Create a new MM_VerboseWriterStreamOutput instance.
 */
MM_VerboseWriterStreamOutput *
MM_VerboseWriterStreamOutput::newInstance(MM_EnvironmentBase *env, const char *filename)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	MM_VerboseWriterStreamOutput *agent = (MM_VerboseWriterStreamOutput *)extensions->getForge()->allocate(sizeof(MM_VerboseWriterStreamOutput), MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if(agent) {
		new(agent) MM_VerboseWriterStreamOutput(env);
		if(!agent->initialize(env, filename)) {
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseWriterStreamOutput instance.
 */
bool
MM_VerboseWriterStreamOutput::initialize(MM_EnvironmentBase *env, const char *filename)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	
	MM_VerboseWriter::initialize(env);

	setStream(getStreamID(env, filename));
	
	if(STDERR == _currentStream){
		omrfile_printf(OMRPORT_TTY_ERR, "\n");
		omrfile_printf(OMRPORT_TTY_ERR, getHeader(env));
	} else {
		omrfile_printf(OMRPORT_TTY_OUT, "\n");
		omrfile_printf(OMRPORT_TTY_OUT, getHeader(env));
	}
	
	return true;
}

/**
 * Closes the agent's output stream.
 */
void
MM_VerboseWriterStreamOutput::closeStream(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	if(STDERR == _currentStream){
		omrfile_write_text(OMRPORT_TTY_ERR, getFooter(env), strlen(getFooter(env)));
		omrfile_write_text(OMRPORT_TTY_ERR, "\n", strlen("\n"));
	} else {
		omrfile_write_text(OMRPORT_TTY_OUT, getFooter(env), strlen(getFooter(env)));
		omrfile_write_text(OMRPORT_TTY_OUT, "\n", strlen("\n"));
	}
}

/**
 * Tear down the structures managed by the MM_VerboseWriterStreamOutput.
 * Tears down the verbose buffer.
 */
void
MM_VerboseWriterStreamOutput::tearDown(MM_EnvironmentBase *env)
{
	MM_VerboseWriter::tearDown(env);
}

/**
 * Flushes the verbose buffer to the output stream.
 */
void
MM_VerboseWriterStreamOutput::endOfCycle(MM_EnvironmentBase *env)
{
	/* Nothing to really do */
}

bool
MM_VerboseWriterStreamOutput::reconfigure(MM_EnvironmentBase *env, const char *filename, uintptr_t fileCount, uintptr_t iterations)
{
	setStream(getStreamID(env, filename));
	return true;
}

MM_VerboseWriterStreamOutput::StreamID
MM_VerboseWriterStreamOutput::getStreamID(MM_EnvironmentBase *env, const char *string)
{
	if(NULL == string) {
		return STDERR;
	}
	
	if(!strcmp(string, "stdout")) {
		return STDOUT;
	}
	
	return STDERR;
}

void
MM_VerboseWriterStreamOutput::outputString(MM_EnvironmentBase *env, const char* string)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if(STDERR == _currentStream){
		omrfile_write_text(OMRPORT_TTY_ERR, string, strlen(string));
	} else {
		omrfile_write_text(OMRPORT_TTY_OUT, string, strlen(string));
	}
}
