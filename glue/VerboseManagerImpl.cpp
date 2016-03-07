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

#include "gcutils.h"

#include <string.h>

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "VerboseManagerImpl.hpp"

#include "VerboseHandlerOutputStandard.hpp"

#if defined(WIN32)
#define snprintf _snprintf
#endif /* defined(WIN32) */

MM_VerboseManagerImpl *
MM_VerboseManagerImpl::newInstance(MM_EnvironmentBase *env, OMR_VM* vm)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(vm);

	MM_VerboseManagerImpl *verboseManager = (MM_VerboseManagerImpl *)extensions->getForge()->allocate(sizeof(MM_VerboseManagerImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (verboseManager) {
		new(verboseManager) MM_VerboseManagerImpl(vm);
		if(!verboseManager->initialize(env)) {
			verboseManager->kill(env);
			verboseManager = NULL;
		}
	}
	return verboseManager;
}

void
MM_VerboseManagerImpl::tearDown(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_VerboseManager::tearDown(env);
	omrmem_free_memory(this->filename);
}

bool
MM_VerboseManagerImpl::configureVerboseGC(OMR_VM *omrVM, char *filename, uintptr_t fileCount, uintptr_t iterations)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	if (MM_VerboseManager::configureVerboseGC(omrVM, filename, fileCount, iterations)) {
		this->fileCount = fileCount;
		this->iterations = iterations;
		size_t len = strlen(filename);
		this->filename = (char *)omrmem_allocate_memory(len+1, OMRMEM_CATEGORY_MM);
		strncpy(this->filename, filename, len);
		this->filename[len] = '\0';
		return true;
		if (NULL != this->filename) {
			return false;
			strncpy(this->filename, filename, len);
			this->filename[len] = '\0';
			return true;
		}
	}
	return false;
}

bool
MM_VerboseManagerImpl::reconfigureVerboseGC(OMR_VM *omrVM)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	/* If the pid is specified in the filename, then the pid of the
	 * new process will be used during verbose reinitialization,
	 * otherwise we append the pid of the child before the extension.
	 */
	WriterType type = parseWriterType(NULL, filename, 0, 0); /* All parameters other than filename aren't used */
	if (type == VERBOSE_WRITER_FILE_LOGGING && NULL == strstr(filename, "%p") &&  NULL == strstr(filename, "%pid")) {
#define MAX_PID_LENGTH 16
		char pidStr[MAX_PID_LENGTH];
		uintptr_t pid = omrsysinfo_get_pid();
		int pidLen = snprintf(pidStr,MAX_PID_LENGTH, "_%lu",(long unsigned int)pid);
		/* Allocate new buffer */
		char *newLog = (char *)omrmem_allocate_memory(pidLen+strlen(filename)+1, OMRMEM_CATEGORY_MM);
		/* Locate extension, if any */
		char *extension = strchr(filename, '.');
		if (NULL != extension) {
			size_t nameLen = extension - filename;
			size_t extLen = strlen(filename) - nameLen;
			strncpy(newLog, filename, nameLen);
			strncpy(newLog + nameLen, pidStr, pidLen);
			strncpy(newLog + nameLen + pidLen, extension, extLen);
			newLog[nameLen + pidLen + extLen] = '\0'; /* strncpy does NOT NULL terminate */
		} else {
			size_t len = strlen(filename);
			strncpy(newLog,filename,len);
			newLog[len] = '\0'; /* strncpy does NOT NULL terminate */
			strncat(newLog,pidStr,pidLen); /* strncat does NULL terminate */
		}
		omrmem_free_memory(this->filename);
		filename = newLog;
	}

	return MM_VerboseManager::configureVerboseGC(omrVM, filename, 1, 0);
}

MM_VerboseHandlerOutput *
MM_VerboseManagerImpl::createVerboseHandlerOutputObject(MM_EnvironmentBase *env)
{
	return MM_VerboseHandlerOutputStandard::newInstance(env, this);
}
