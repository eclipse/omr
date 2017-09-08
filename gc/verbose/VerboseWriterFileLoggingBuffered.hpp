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

#if !defined(VERBOSEWRITERFILELOGGINGBUFFERED_HPP_)
#define VERBOSEWRITERFILELOGGINGBUFFERED_HPP_

#include "omrcfg.h"

#include "VerboseWriterFileLogging.hpp"

/**
 * Ouptut agent which directs verbosegc output to file.
 */
class MM_VerboseWriterFileLoggingBuffered : public MM_VerboseWriterFileLogging
{
	/*
	 * Data members
	 */
public:
protected:
private:
	OMRFileStream *_logFileStream; /**< the filestream being written to */

	/*
	 * Function members
	 */
public:
	static MM_VerboseWriterFileLoggingBuffered *newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager, char* filename, uintptr_t fileCount, uintptr_t iterations);

	virtual void outputString(MM_EnvironmentBase *env, const char* string);

protected:
	MM_VerboseWriterFileLoggingBuffered(MM_EnvironmentBase *env, MM_VerboseManager *manager);

	virtual bool initialize(MM_EnvironmentBase *env, const char *filename, uintptr_t numFiles, uintptr_t numCycles);

private:
	virtual void tearDown(MM_EnvironmentBase *env);

	bool openFile(MM_EnvironmentBase *env);
	void closeFile(MM_EnvironmentBase *env);
};

#endif /* VERBOSEWRITERFILELOGGINGBUFFERED_HPP_ */
