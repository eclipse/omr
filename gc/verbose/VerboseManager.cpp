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
#include "VerboseManager.hpp"

/**
 * Create a new MM_VerboseManager instance.
 * @return Pointer to the new MM_VerboseManager.
 */
MM_VerboseManager *
MM_VerboseManager::newInstance(MM_EnvironmentBase *env, OMR_VM* vm)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(vm);
	
	MM_VerboseManager *verboseManager = (MM_VerboseManager *)extensions->getForge()->allocate(sizeof(MM_VerboseManager), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (verboseManager) {
		new(verboseManager) MM_VerboseManager(vm);
		if(!verboseManager->initialize(env)) {
			verboseManager->kill(env);
			verboseManager = NULL;
		}
	}
	return verboseManager;
}

/**
 * Kill the MM_VerboseManager instance.
 * Tears down the related structures and frees any storage.
 */
//void
//MM_VerboseManager::kill(MM_EnvironmentBase *env)
//{
//	tearDown(env);
//
//	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
//	extensions->getForge()->free(this);
//}

/**
 * Initializes the MM_VerboseManager instance.
 */
//bool
//MM_VerboseManager::initialize(MM_EnvironmentBase *env)
//{
//	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
//	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
//	_mmPrivateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
//	_omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
//
//	_writerChain = MM_VerboseWriterChain::newInstance(env);
//	if (NULL == _writerChain) {
//		return false;
//	}
//
//	if(NULL == (_verboseHandlerOutput = createVerboseHandlerOutputObject(env))) {
//		return false;
//	}
//
//	_lastOutputTime = omrtime_hires_clock();
//
//	return true;
//}
//
///**
// * Tear down the structures managed by the MM_VerboseManager.
// * Tears down the event stream and outut agents.
// */
//void
//MM_VerboseManager::tearDown(MM_EnvironmentBase *env)
//{
//	disableVerboseGC();
//
//	if(NULL != _verboseHandlerOutput) {
//		_verboseHandlerOutput->kill(env);
//		_verboseHandlerOutput = NULL;
//	}
//
//	_writerChain->kill(env);
//}
//
//MM_VerboseHandlerOutput *
//MM_VerboseManager::createVerboseHandlerOutputObject(MM_EnvironmentBase *env)
//{
//	MM_VerboseHandlerOutput *handler = NULL;
//	MM_GCExtensionsBase *extensions = env->getExtensions();
//
//	if (extensions->isStandardGC()) {
//#if defined(OMR_GC_MODRON_STANDARD)
//		handler = MM_VerboseHandlerOutputStandard::newInstance(env, this);
//#endif /* defined(OMR_GC_MODRON_STANDARD) */
//	} else {
//		handler = MM_VerboseHandlerOutput::newInstance(env, this);
//	}
//
//	return handler;
//}

//void
//MM_VerboseManager::closeStreams(MM_EnvironmentBase *env)
//{
//	MM_VerboseWriter *writer = _writerChain->getFirstWriter();
//	while(NULL != writer) {
//		writer->closeStream(env);
//		writer = writer->getNextWriter();
//	}
//}
//
//void
//MM_VerboseManager::enableVerboseGC()
//{
//	if (!_hooksAttached) {
//		_verboseHandlerOutput->enableVerbose();
//		_hooksAttached = true;
//	}
//}
//
//void
//MM_VerboseManager::disableVerboseGC()
//{
//	if (_hooksAttached) {
//		_verboseHandlerOutput->disableVerbose();
//		_hooksAttached = false;
//	}
//}

/**
 * Finds an agent of a given type in the event chain.
 * @param type Indicates the type of agent to return.
 * @return Pointer to an agent of the specified type.
 */
//MM_VerboseWriter *
//MM_VerboseManager::findWriterInChain(WriterType type)
//{
//	MM_VerboseWriter *writer = _writerChain->getFirstWriter();
//
//	while (NULL != writer){
//		if (type == writer->getType()){
//			return writer;
//		}
//		writer = writer->getNextWriter();
//	}
//
//	return NULL;
//}

/**
 * Counts the number of output agents currently enabled.
 * @return the number of current output agents.
 */
//uintptr_t
//MM_VerboseManager::countActiveOutputHandlers()
//{
//	MM_VerboseWriter *writer = _writerChain->getFirstWriter();
//	uintptr_t count = 0;
//
//	while(NULL != writer) {
//		if(writer->isActive()) {
//			count += 1;
//		}
//		writer = writer->getNextWriter();
//	}
//
//	return count;
//}

/**
 * Walks the output agent chain disabling the agents.
 */
//void
//MM_VerboseManager::disableWriters()
//{
//	MM_VerboseWriter *writer = _writerChain->getFirstWriter();
//
//	while(NULL != writer) {
//		writer->isActive(false);
//		writer = writer->getNextWriter();
//	}
//}

//WriterType
//MM_VerboseManager::parseWriterType(MM_EnvironmentBase *env, char *filename, uintptr_t fileCount, uintptr_t iterations)
//{
//	MM_GCExtensionsBase* extensions = env->getExtensions();
//
//	if(NULL == filename) {
//		return VERBOSE_WRITER_STANDARD_STREAM;
//	}
//
//	if(!strcmp(filename, "stderr") || !strcmp(filename, "stdout")) {
//		return VERBOSE_WRITER_STANDARD_STREAM;
//	}
//
//#if defined(OMR_RAS_TDF_TRACE)
//	if(!strcmp(filename, "trace")) {
//		return VERBOSE_WRITER_TRACE;
//	}
//#endif /* OMR_RAS_TDF_TRACE */
//
//	if(!strcmp(filename, "hook")) {
//		return VERBOSE_WRITER_HOOK;
//	}
//
//	if (extensions->bufferedLogging) {
//		return VERBOSE_WRITER_FILE_LOGGING_BUFFERED;
//	}
//
//	return VERBOSE_WRITER_FILE_LOGGING_SYNCHRONOUS;
//}

/**
 * Configures verbosegc according to the parameters passed.
 * @param filename The name of the file or output stream to log to.
 * @param fileCount The number of files to log to.
 * @param iterations The number of gc cycles to log to each file.
 * @return true on success, false on failure
 */
//bool
//MM_VerboseManager::configureVerboseGC(OMR_VM *omrVM, char *filename, uintptr_t fileCount, uintptr_t iterations)
//{
//	MM_EnvironmentBase env(omrVM);
//
//	MM_VerboseWriter *writer = NULL;
//
//	disableWriters();
//
//	WriterType type = parseWriterType(&env, filename, fileCount, iterations);
//
//	writer = findWriterInChain(type);
//
//	if (NULL != writer) {
//		writer->reconfigure(&env, filename, fileCount, iterations);
//	} else {
//
//		writer = createWriter(&env, type, filename, fileCount, iterations);
//
//		if(NULL == writer) {
//			return false;
//		}
//
//		_writerChain->addWriter(writer);
//	}
//
//	writer->isActive(true);
//
//	return true;
//}

//MM_VerboseWriter *
//MM_VerboseManager::createWriter(MM_EnvironmentBase *env, WriterType type, char *filename, uintptr_t fileCount, uintptr_t iterations)
//{
//	MM_VerboseWriter *writer = NULL;
//	switch(type) {
//	case VERBOSE_WRITER_STANDARD_STREAM:
//		writer = MM_VerboseWriterStreamOutput::newInstance(env, filename);
//		break;
//	case VERBOSE_WRITER_HOOK:
//		writer = MM_VerboseWriterHook::newInstance(env);
//		break;
//	case VERBOSE_WRITER_FILE_LOGGING_SYNCHRONOUS:
//		writer = MM_VerboseWriterFileLoggingSynchronous::newInstance(env, this, filename, fileCount, iterations);
//		if (NULL == writer) {
//			writer = findWriterInChain(VERBOSE_WRITER_STANDARD_STREAM);
//			if (NULL != writer) {
//				writer->isActive(true);
//				return writer;
//			}
//			/* if we failed to create a file stream and there is no stderr stream try to create a stderr stream */
//			writer = MM_VerboseWriterStreamOutput::newInstance(env, NULL);
//		}
//		break;
//	case VERBOSE_WRITER_FILE_LOGGING_BUFFERED:
//		writer = MM_VerboseWriterFileLoggingBuffered::newInstance(env, this, filename, fileCount, iterations);
//		if (NULL == writer) {
//			writer = findWriterInChain(VERBOSE_WRITER_STANDARD_STREAM);
//			if (NULL != writer) {
//				writer->isActive(true);
//				return writer;
//			}
//			/* if we failed to create a file stream and there is no stderr stream try to create a stderr stream */
//			writer = MM_VerboseWriterStreamOutput::newInstance(env, NULL);
//		}
//		break;
//
//	default:
//		return NULL;
//	}
//
//	return writer;
//}
