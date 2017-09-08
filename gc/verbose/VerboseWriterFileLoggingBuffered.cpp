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
#include "VerboseManager.hpp"
#include "VerboseWriterFileLoggingBuffered.hpp"

#include "GCExtensionsBase.hpp"
#include "EnvironmentBase.hpp"

#include <string.h>

MM_VerboseWriterFileLoggingBuffered::MM_VerboseWriterFileLoggingBuffered(MM_EnvironmentBase *env, MM_VerboseManager *manager)
	:MM_VerboseWriterFileLogging(env, manager, VERBOSE_WRITER_FILE_LOGGING_BUFFERED)
	,_logFileStream(NULL)
{
	/* No implementation */
}

/**
 * Create a new MM_VerboseWriterFileLoggingBuffered instance.
 * @return Pointer to the new MM_VerboseWriterFileLoggingBuffered.
 */
MM_VerboseWriterFileLoggingBuffered *
MM_VerboseWriterFileLoggingBuffered::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager, char *filename, uintptr_t numFiles, uintptr_t numCycles)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	MM_VerboseWriterFileLoggingBuffered *agent = (MM_VerboseWriterFileLoggingBuffered *)extensions->getForge()->allocate(sizeof(MM_VerboseWriterFileLoggingBuffered), MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if(agent) {
		new(agent) MM_VerboseWriterFileLoggingBuffered(env, manager);
		if(!agent->initialize(env, filename, numFiles, numCycles)) {
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseWriterFileLoggingBuffered instance.
 * @return true on success, false otherwise
 */
bool
MM_VerboseWriterFileLoggingBuffered::initialize(MM_EnvironmentBase *env, const char *filename, uintptr_t numFiles, uintptr_t numCycles)
{
	return MM_VerboseWriterFileLogging::initialize(env, filename, numFiles, numCycles);
}

/**
 * Tear down the structures managed by the MM_VerboseWriterFileLoggingBuffered.
 * Tears down the verbose buffer.
 */
void
MM_VerboseWriterFileLoggingBuffered::tearDown(MM_EnvironmentBase *env)
{
	MM_VerboseWriterFileLogging::tearDown(env);
}

/**
 * Opens the file to log output to and prints the header.
 * @return true on sucess, false otherwise
 */
bool
MM_VerboseWriterFileLoggingBuffered::openFile(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase* extensions = env->getExtensions();
	const char* version = omrgc_get_version(env->getOmrVM());
	
	char *filenameToOpen = expandFilename(env, _currentFile);
	if (NULL == filenameToOpen) {
		return false;
	}
	
	_logFileStream = omrfilestream_open(filenameToOpen, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if(NULL == _logFileStream) {
		char *cursor = filenameToOpen;
		/**
		 * This may have failed due to directories in the path not being available.
		 * Try to create these directories and attempt to open again before failing.
		 */
		while ( (cursor = strchr(++cursor, DIR_SEPARATOR)) != NULL ) {
			*cursor = '\0';
			omrfile_mkdir(filenameToOpen);
			*cursor = DIR_SEPARATOR;
		}

		/* Try again */
		_logFileStream = omrfilestream_open(filenameToOpen, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
		if (NULL == _logFileStream) {
			_manager->handleFileOpenError(env, filenameToOpen);
			extensions->getForge()->free(filenameToOpen);
			return false;
		}
	}

	extensions->getForge()->free(filenameToOpen);
	
	omrfilestream_printf(_logFileStream, getHeader(env), version);
	
	return true;
}

/**
 * Prints the footer and closes the file being logged to.
 */
void
MM_VerboseWriterFileLoggingBuffered::closeFile(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	
	if(NULL != _logFileStream) {
		omrfilestream_write_text(_logFileStream, getFooter(env), strlen(getFooter(env)), J9STR_CODE_PLATFORM_RAW);
		omrfilestream_write_text(_logFileStream, "\n", strlen("\n"), J9STR_CODE_PLATFORM_RAW);
		omrfilestream_close(_logFileStream);
		_logFileStream = NULL;
	}
}
void
MM_VerboseWriterFileLoggingBuffered::outputString(MM_EnvironmentBase *env, const char* string)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if(NULL == _logFileStream) {
		/* we open the file at the end of the cycle so can't have a final empty file at the end of a run */
		openFile(env);
	}

	if(NULL != _logFileStream){
		omrfilestream_write_text(_logFileStream, string, strlen(string), J9STR_CODE_PLATFORM_RAW);
	} else {
		omrfilestream_write_text(OMRPORT_STREAM_ERR, string, strlen(string), J9STR_CODE_PLATFORM_RAW);
	}
}
