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

#if !defined(VERBOSEMANAGER_HPP_)
#define VERBOSEMANAGER_HPP_

#include "omrcfg.h"
#include "mmhook_common.h"

#include "VerboseManagerBase.hpp"
#include "VerboseWriter.hpp"

class MM_EnvironmentBase;
class MM_VerboseHandlerOutput;
class MM_VerboseWriterChain;

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
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual MM_VerboseWriter *createWriter(MM_EnvironmentBase *env, WriterType type, char *filename, uintptr_t fileCount, uintptr_t iterations);

	/**
	 * Create the output handler specific to the kind of collector currently running.
	 * @param env[in] The master GC thread
	 * @return An instance of a sub-class of the MM_VerboseHandlerOutput abstract class which can handle output of verbose data for the collector currently running
	 */
	virtual MM_VerboseHandlerOutput *createVerboseHandlerOutputObject(MM_EnvironmentBase *env);

	/**
	 * Find the given agent type in the list.
	 * @param type Agent type to look for.
	 * @return An agent matching the type, or NULL if none exists.
	 */
	virtual MM_VerboseWriter *findWriterInChain(WriterType type);

	/**
	 * Disable all verbose writers in the chain.
	 */
	virtual void disableWriters();
	
	/**
	 * Determine the writer type to initialize given the set of parameters.
	 * @param env A vm thread.
	 * @param filename Filename for output (may be NULL)
	 * @param fileCount Number of files to rotate through (if applicable)
	 * @param iterations Number of stanza markers to allow per file before moving to the next file
	 * @return WriterType representing expected output mechanism.
	 */
	virtual WriterType parseWriterType(MM_EnvironmentBase *env, char *filename, uintptr_t fileCount, uintptr_t iterations);

public:

	/* Interface for Dynamic Configuration */
	virtual bool configureVerboseGC(OMR_VM *vm, char* filename, uintptr_t fileCount, uintptr_t iterations);

	/**
	 * Determine the number of currently active output mechanisms.
	 * @return a count of the number of active output mechanisms.
	 */
	virtual uintptr_t countActiveOutputHandlers();

	virtual void enableVerboseGC();
	virtual void disableVerboseGC();

	static MM_VerboseManager *newInstance(MM_EnvironmentBase *env, OMR_VM* vm);
	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Close all output mechanisms on the receiver.
	 * @param env vm thread.
	 */
	virtual void closeStreams(MM_EnvironmentBase *env);

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
