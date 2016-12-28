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

#if !defined(VERBOSEMANAGER_HPP_)
#define VERBOSEMANAGER_HPP_

#include "omrcfg.h"
#include "mmhook_common.h"

#include "VerboseManagerBase.hpp"
#include "VerboseWriter.hpp"


#include "VerboseHandlerOutput.hpp"
#include "VerboseHandlerOutputStandard.hpp"
#include "VerboseWriter.hpp"
#include "VerboseWriterChain.hpp"
#include "VerboseWriterHook.hpp"
#include "VerboseWriterFileLogging.hpp"
#include "VerboseWriterFileLoggingBuffered.hpp"
#include "VerboseWriterFileLoggingSynchronous.hpp"
#include "VerboseWriterStreamOutput.hpp"

class MM_EnvironmentBase;
//class MM_VerboseHandlerOutput;
//class MM_VerboseWriterChain;

/**
 * The central class of the verbose gc mechanism.
 * Acts as the anchor point for the EventStream and Output Agent chain.
 * Provides routines for management of the output agents.
 * Stores data which needs to persist across streams.
 * @ingroup GC_verbose_engine
 */
class MM_VerboseManager : public MM_VerboseManagerBase
{
	/*
	 * Data members
	 */
private:

protected:
	MM_VerboseWriterChain* _writerChain; /**< The chain of writers for new verbose */
	MM_VerboseHandlerOutput *_verboseHandlerOutput;  /**< New verbose format output handler */

public:
	
	/*
	 * Function members
	 */
private:

protected:
	//virtual bool initialize(MM_EnvironmentBase *env);
	//virtual void tearDown(MM_EnvironmentBase *env);
	virtual bool
	initialize(MM_EnvironmentBase *env)
	{
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
		_mmPrivateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
		_omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);

		_writerChain = MM_VerboseWriterChain::newInstance(env);
		if (NULL == _writerChain) {
			return false;
		}

		if(NULL == (_verboseHandlerOutput = createVerboseHandlerOutputObject(env))) {
			return false;
		}

		_lastOutputTime = omrtime_hires_clock();

		return true;
	}

	/**
	 * Tear down the structures managed by the MM_VerboseManager.
	 * Tears down the event stream and outut agents.
	 */
	virtual void
	tearDown(MM_EnvironmentBase *env)
	{
		disableVerboseGC();

		if(NULL != _verboseHandlerOutput) {
			_verboseHandlerOutput->kill(env);
			_verboseHandlerOutput = NULL;
		}

		_writerChain->kill(env);
	}
	//zg.copied from VerboseManager.cpp, to make the defination and delicare into one compile unit.
	//virtual MM_VerboseWriter *createWriter(MM_EnvironmentBase *env, WriterType type, char *filename, uintptr_t fileCount, uintptr_t iterations);
	virtual MM_VerboseWriter *
	createWriter(MM_EnvironmentBase *env, WriterType type, char *filename, uintptr_t fileCount, uintptr_t iterations)
	{
		MM_VerboseWriter *writer = NULL;
		switch(type) {
		case VERBOSE_WRITER_STANDARD_STREAM:
			writer = MM_VerboseWriterStreamOutput::newInstance(env, filename);
			break;
		case VERBOSE_WRITER_HOOK:
			writer = MM_VerboseWriterHook::newInstance(env);
			break;
		case VERBOSE_WRITER_FILE_LOGGING_SYNCHRONOUS:
			writer = MM_VerboseWriterFileLoggingSynchronous::newInstance(env, this, filename, fileCount, iterations);
			if (NULL == writer) {
				writer = findWriterInChain(VERBOSE_WRITER_STANDARD_STREAM);
				if (NULL != writer) {
					writer->isActive(true);
					return writer;
				}
				/* if we failed to create a file stream and there is no stderr stream try to create a stderr stream */
				writer = MM_VerboseWriterStreamOutput::newInstance(env, NULL);
			}
			break;
		case VERBOSE_WRITER_FILE_LOGGING_BUFFERED:
			writer = MM_VerboseWriterFileLoggingBuffered::newInstance(env, this, filename, fileCount, iterations);
			if (NULL == writer) {
				writer = findWriterInChain(VERBOSE_WRITER_STANDARD_STREAM);
				if (NULL != writer) {
					writer->isActive(true);
					return writer;
				}
				/* if we failed to create a file stream and there is no stderr stream try to create a stderr stream */
				writer = MM_VerboseWriterStreamOutput::newInstance(env, NULL);
			}
			break;

		default:
			return NULL;
		}

		return writer;
	}
	/**
	 * Create the output handler specific to the kind of collector currently running.
	 * @param env[in] The master GC thread
	 * @return An instance of a sub-class of the MM_VerboseHandlerOutput abstract class which can handle output of verbose data for the collector currently running
	 */
	//virtual MM_VerboseHandlerOutput *createVerboseHandlerOutputObject(MM_EnvironmentBase *env);

	virtual MM_VerboseHandlerOutput *
	createVerboseHandlerOutputObject(MM_EnvironmentBase *env)
	{
		MM_VerboseHandlerOutput *handler = NULL;
		MM_GCExtensionsBase *extensions = env->getExtensions();

		if (extensions->isStandardGC()) {
	#if defined(OMR_GC_MODRON_STANDARD)
			handler = MM_VerboseHandlerOutputStandard::newInstance(env, this);
	#endif /* defined(OMR_GC_MODRON_STANDARD) */
		} else {
			handler = MM_VerboseHandlerOutput::newInstance(env, this);
		}

		return handler;
	}
	/**
	 * Find the given agent type in the list.
	 * @param type Agent type to look for.
	 * @return An agent matching the type, or NULL if none exists.
	 */
	//virtual MM_VerboseWriter *findWriterInChain(WriterType type);
virtual MM_VerboseWriter *
findWriterInChain(WriterType type)
{
	MM_VerboseWriter *writer = _writerChain->getFirstWriter();

	while (NULL != writer){
		if (type == writer->getType()){
			return writer;
		}
		writer = writer->getNextWriter();
	}

	return NULL;
}
	/**
	 * Disable all verbose writers in the chain.
	 */
	//virtual void disableWriters();
virtual void
disableWriters()
{
	MM_VerboseWriter *writer = _writerChain->getFirstWriter();

	while(NULL != writer) {
		writer->isActive(false);
		writer = writer->getNextWriter();
	}
}
	
	/**
	 * Determine the writer type to initialize given the set of parameters.
	 * @param env A vm thread.
	 * @param filename Filename for output (may be NULL)
	 * @param fileCount Number of files to rotate through (if applicable)
	 * @param iterations Number of stanza markers to allow per file before moving to the next file
	 * @return WriterType representing expected output mechanism.
	 */
//	virtual WriterType parseWriterType(MM_EnvironmentBase *env, char *filename, uintptr_t fileCount, uintptr_t iterations);
virtual WriterType
parseWriterType(MM_EnvironmentBase *env, char *filename, uintptr_t fileCount, uintptr_t iterations)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if(NULL == filename) {
		return VERBOSE_WRITER_STANDARD_STREAM;
	}

	if(!strcmp(filename, "stderr") || !strcmp(filename, "stdout")) {
		return VERBOSE_WRITER_STANDARD_STREAM;
	}

#if defined(OMR_RAS_TDF_TRACE)
	if(!strcmp(filename, "trace")) {
		return VERBOSE_WRITER_TRACE;
	}
#endif /* OMR_RAS_TDF_TRACE */

	if(!strcmp(filename, "hook")) {
		return VERBOSE_WRITER_HOOK;
	}

	if (extensions->bufferedLogging) {
		return VERBOSE_WRITER_FILE_LOGGING_BUFFERED;
	}

	return VERBOSE_WRITER_FILE_LOGGING_SYNCHRONOUS;
}
public:

	/* Interface for Dynamic Configuration */
	//virtual bool configureVerboseGC(OMR_VM *vm, char* filename, uintptr_t fileCount, uintptr_t iterations);
virtual bool
configureVerboseGC(OMR_VM *omrVM, char *filename, uintptr_t fileCount, uintptr_t iterations)
{
	MM_EnvironmentBase env(omrVM);

	MM_VerboseWriter *writer = NULL;

	disableWriters();

	WriterType type = parseWriterType(&env, filename, fileCount, iterations);

	writer = findWriterInChain(type);

	if (NULL != writer) {
		writer->reconfigure(&env, filename, fileCount, iterations);
	} else {

		writer = createWriter(&env, type, filename, fileCount, iterations);

		if(NULL == writer) {
			return false;
		}

		_writerChain->addWriter(writer);
	}

	writer->isActive(true);

	return true;
}
	/**
	 * Determine the number of currently active output mechanisms.
	 * @return a count of the number of active output mechanisms.
	 */
	//virtual uintptr_t countActiveOutputHandlers();
virtual uintptr_t
countActiveOutputHandlers()
{
	MM_VerboseWriter *writer = _writerChain->getFirstWriter();
	uintptr_t count = 0;

	while(NULL != writer) {
		if(writer->isActive()) {
			count += 1;
		}
		writer = writer->getNextWriter();
	}

	return count;
}
	//virtual void enableVerboseGC();
	//virtual void disableVerboseGC();
virtual void
closeStreams(MM_EnvironmentBase *env)
{
	MM_VerboseWriter *writer = _writerChain->getFirstWriter();
	while(NULL != writer) {
		writer->closeStream(env);
		writer = writer->getNextWriter();
	}
}

virtual void
enableVerboseGC()
{
	if (!_hooksAttached) {
		_verboseHandlerOutput->enableVerbose();
		_hooksAttached = true;
	}
}

virtual void
disableVerboseGC()
{
	if (_hooksAttached) {
		_verboseHandlerOutput->disableVerbose();
		_hooksAttached = false;
	}
}
	static MM_VerboseManager *newInstance(MM_EnvironmentBase *env, OMR_VM* vm);
//	virtual void k/ll(MM_EnvironmentBase *env);
virtual void
kill(MM_EnvironmentBase *env)
{
	tearDown(env);

	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	extensions->getForge()->free(this);
}
	/**
	 * Close all output mechanisms on the receiver.
	 * @param env vm thread.
	 */
	//virtual void closeStreams(MM_EnvironmentBase *env);

	MMINLINE MM_VerboseWriterChain* getWriterChain() { return _writerChain; }
	
	virtual void handleFileOpenError(MM_EnvironmentBase *env, char *fileName) {}

	MM_VerboseManager(OMR_VM *omrVM)
		: MM_VerboseManagerBase(omrVM)
		, _writerChain(NULL)
		, _verboseHandlerOutput(NULL)
	{
	}
};

#endif /* VERBOSEMANAGER_HPP_ */
