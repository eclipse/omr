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

#include "modronapicore.hpp"
#include "VerboseManager.hpp"
#include "VerboseWriterFileLogging.hpp"

#include "GCExtensionsBase.hpp"
#include "EnvironmentBase.hpp"

#include <string.h>

enum {
	single_file = 0,
	rotating_files
};

MM_VerboseWriterFileLogging::MM_VerboseWriterFileLogging(MM_EnvironmentBase *env, MM_VerboseManager *manager)
	:MM_VerboseWriter(VERBOSE_WRITER_FILE_LOGGING)
	,_filename(NULL)
	,_mode(single_file)
	,_currentFile(0)
	,_currentCycle(0)
	,_logFileDescriptor(-1)
	,_tokens(NULL)
 	,_manager(manager)
{
	/* No implementation */
}

/**
 * Create a new MM_VerboseWriterFileLogging instance.
 * @return Pointer to the new MM_VerboseWriterFileLogging.
 */
MM_VerboseWriterFileLogging *
MM_VerboseWriterFileLogging::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager, char *filename, uintptr_t numFiles, uintptr_t numCycles)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	MM_VerboseWriterFileLogging *agent = (MM_VerboseWriterFileLogging *)extensions->getForge()->allocate(sizeof(MM_VerboseWriterFileLogging), MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if(agent) {
		new(agent) MM_VerboseWriterFileLogging(env, manager);
		if(!agent->initialize(env, filename, numFiles, numCycles)) {
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseWriterFileLogging instance.
 * @return true on success, false otherwise
 */
bool
MM_VerboseWriterFileLogging::initialize(MM_EnvironmentBase *env, const char *filename, uintptr_t numFiles, uintptr_t numCycles)
{
	MM_VerboseWriter::initialize(env);

	_numFiles = numFiles;
	_numCycles = numCycles;
	
	if((_numFiles > 0) && (_numCycles > 0)) {
		_mode = rotating_files;
	} else {
		_mode = single_file;
	}
	
	if (!initializeTokens(env)) {
		return false;
	}
	
	if (!initializeFilename(env, filename)) {
		return false;
	}
	
	intptr_t initialFile = findInitialFile(env);
	if (initialFile < 0) {
		return false;
	}
	_currentFile = initialFile;
	
	if(!openFile(env)) {
		return false;
	}
		
	return true;
}

/**
 * Tear down the structures managed by the MM_VerboseWriterFileLogging.
 * Tears down the verbose buffer.
 */
void
MM_VerboseWriterFileLogging::tearDown(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
		
	omrstr_free_tokens(_tokens);
	_tokens = NULL;
	extensions->getForge()->free(_filename);
	_filename = NULL;

	MM_VerboseWriter::tearDown(env);
}

/**
 * Initialize the _tokens field.
 * JTCJAZZ 36600 alias %p to be the same as %pid
 */
bool 
MM_VerboseWriterFileLogging::initializeTokens(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	char pidBuffer[64];
	
	_tokens = omrstr_create_tokens(omrtime_current_time_millis());
	if (_tokens == NULL) {
		return false;
	}
	
	if (sizeof(pidBuffer) < omrstr_subst_tokens(pidBuffer, sizeof(pidBuffer), "%pid", _tokens)) {
		return false;
	}
	
	if (omrstr_set_token(_tokens, "p", "%s", pidBuffer)) {
		return false;
	}
	
	return true;
}

/**
 * Initialize the _filename field based on filename.
 *
 * Since token substitution only supports tokens starting with %, all # characters in the 
 * filename will be replaced with %seq (unless the # is already preceded by an odd number 
 * of % signs, in which case it is replaced with seq).
 *
 * e.g.  foo#   --> foo%seq
 *       foo%#  --> foo%seq
 *       foo%%# --> foo%%%seq
 * 
 * If %seq or %# is not specified, and if rotating logs have been requested, ".%seq" is 
 * appended to the log name.
 * 
 * If the resulting filename is too large to fit in the buffer, it is truncated.
 * 
 * @param[in] env the current environment
 * @param[in] filename the user specified filename
 * 
 * @return true on success, false on failure
 */
bool 
MM_VerboseWriterFileLogging::initializeFilename(MM_EnvironmentBase *env, const char *filename)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	if (_mode == rotating_files) {
		const char* read = filename;

		/* count the number of hashes in the source string */
		uintptr_t hashCount = 0;
		for (read = filename; '\0' != *read; read++) {
			if ('#' == *read) {
				hashCount++;
			}
		}
		
		/* allocate memory for the copied template filename */ 
		uintptr_t nameLen = strlen(filename) + 1;
		if (hashCount > 0) {
			/* each # expands into %seq, so for each # add 3 to len */
			nameLen += hashCount * (sizeof("seq") - 1);
		} else {
			/* if there are no hashes we may append .%seq to the end */
			nameLen += sizeof(".%seq") - 1;
		}

		_filename = (char*)extensions->getForge()->allocate(nameLen, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
		if (NULL == _filename) {
			return false;
		}
		
		/* copy the original filename into the allocated memory, expanding #s to %seq */
		bool foundSeq = false;
		bool oddPercents = false;
		char* write = _filename;
		for (read = filename; '\0' != *read; read++) {
			/* check to see if %seq appears in the source filename */
			if (oddPercents && (0 == strncmp(read, "seq", 3))) {
				foundSeq = true;
			}

			if ('#' == *read) {
				strcpy(write, oddPercents ? "seq" : "%seq");
				write += strlen(write);
			} else {
				*write++ = *read;
			}
			oddPercents = ('%' == *read) ? !oddPercents : false;
		}

		*write = '\0';
		
		if ( (false == foundSeq) && (0 == hashCount) ) {
			strcpy(write, ".%seq");
		}
	} else {
		_filename = (char*)extensions->getForge()->allocate(strlen(filename) + 1, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
		if (NULL == _filename) {
			return false;
		}
		strcpy(_filename, filename);
	}

	return true;
}

/**
 * Generate an expanded filename based on currentFile.
 * The caller is responsible for freeing the returned memory.
 * 
 * @param env the current thread
 * @param currentFile the current file number to substitute into the filename template
 * 
 * @return NULL on failure, allocated memory on success 
 */
char* 
MM_VerboseWriterFileLogging::expandFilename(MM_EnvironmentBase *env, uintptr_t currentFile)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	if (_mode == rotating_files) {
		omrstr_set_token(_tokens, "seq", "%03zu", currentFile + 1); /* plus one so the filenames start from .001 instead of .000 */
	}
	
	uintptr_t len = omrstr_subst_tokens(NULL, 0, _filename, _tokens);
	char *filenameToOpen = (char*)extensions->getForge()->allocate(len, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if (NULL != filenameToOpen) {
		omrstr_subst_tokens(filenameToOpen, len, _filename, _tokens);
	}
	return filenameToOpen;
	
}

/**
 * Probe the file system for existing files. Determine
 * the first number which is unused, or the number of the oldest
 * file if all numbers are used.
 * @return the first file number to use (starting at 0), or -1 on failure
 */
intptr_t 
MM_VerboseWriterFileLogging::findInitialFile(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	int64_t oldestTime = J9CONST64(0x7FFFFFFFFFFFFFFF); /* the highest possible time. */
	intptr_t oldestFile = 0;

	if (_mode != rotating_files) {
		/* nothing to do */
		return 0;
	}

	for (uintptr_t currentFile = 0; currentFile < _numFiles; currentFile++) {
		char *filenameToOpen = expandFilename(env, currentFile);
		if (NULL == filenameToOpen) {
			return -1;
		}

		int64_t thisTime = omrfile_lastmod(filenameToOpen);
		extensions->getForge()->free(filenameToOpen);
		
		if (thisTime < 0) {
			/* file doesn't exist, or some other problem reading the file */
			oldestFile = currentFile;
			break;
		} else if (thisTime < oldestTime) {
			oldestTime = thisTime;
			oldestFile = currentFile;
		}
	}
	
	return oldestFile; 
}

/**
 * Opens the file to log output to and prints the header.
 * @return true on sucess, false otherwise
 */
bool
MM_VerboseWriterFileLogging::openFile(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase* extensions = env->getExtensions();
	const char* version = omrgc_get_version(env->getOmrVM());
	
	char *filenameToOpen = expandFilename(env, _currentFile);
	if (NULL == filenameToOpen) {
		return false;
	}
	
	_logFileDescriptor = omrfile_open(filenameToOpen, EsOpenRead | EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if(-1 == _logFileDescriptor) {
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
		_logFileDescriptor = omrfile_open(filenameToOpen, EsOpenRead | EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
		if (-1 == _logFileDescriptor) {
			_manager->handleFileOpenError(env, filenameToOpen);
			extensions->getForge()->free(filenameToOpen);
			return false;
		}
	}

	extensions->getForge()->free(filenameToOpen);
	
	omrfile_printf(_logFileDescriptor, getHeader(env), version);
	
	return true;
}

/**
 * Prints the footer and closes the file being logged to.
 */
void
MM_VerboseWriterFileLogging::closeFile(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	
	if(-1 != _logFileDescriptor) {
		omrfile_write_text(_logFileDescriptor, getFooter(env), strlen(getFooter(env)));
		omrfile_write_text(_logFileDescriptor, "\n", strlen("\n"));
		omrfile_close(_logFileDescriptor);
		_logFileDescriptor = -1;
	}
}

/**
 * Closes the agent's output stream.
 */
void
MM_VerboseWriterFileLogging::closeStream(MM_EnvironmentBase *env)
{
	closeFile(env);
}

/**
 * Flushes the verbose buffer to the output stream.
 * Also cycles the output files if necessary.
 */
void
MM_VerboseWriterFileLogging::endOfCycle(MM_EnvironmentBase *env)
{
	if(rotating_files == _mode) {
		_currentCycle = (_currentCycle + 1) % _numCycles;
		if(0 == _currentCycle) {
			closeFile(env);
			_currentFile = (_currentFile + 1) % _numFiles;
		}
	}
}

/**
 * Reconfigures the agent according to the parameters passed.
 * Required for Dynamic verbose gc configuration.
 * @param filename The name of the file or output stream to log to.
 * @param fileCount The number of files to log to.
 * @param iterations The number of gc cycles to log to each file.
 */
bool
MM_VerboseWriterFileLogging::reconfigure(MM_EnvironmentBase *env, const char *filename, uintptr_t numFiles, uintptr_t numCycles)
{
	closeFile(env);
	return initialize(env, filename, numFiles, numCycles);
}

void
MM_VerboseWriterFileLogging::outputString(MM_EnvironmentBase *env, const char* string)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if(-1 == _logFileDescriptor) {
		/* we open the file at the end of the cycle so can't have a final empty file at the end of a run */
		openFile(env);
	}

	if(-1 != _logFileDescriptor){
		omrfile_write_text(_logFileDescriptor, string, strlen(string));
	} else {
		omrfile_write_text(OMRPORT_TTY_ERR, string, strlen(string));
	}
}
