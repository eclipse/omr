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

#if !defined(VERBOSEWRITER_HPP_)
#define VERBOSEWRITER_HPP_

#include "omrcfg.h"
#include "modronbase.h"

#include "Base.hpp"

#include "EnvironmentBase.hpp"

typedef enum {
	VERBOSE_WRITER_STANDARD_STREAM = 1,
	VERBOSE_WRITER_FILE_LOGGING_SYNCHRONOUS = 2,
	VERBOSE_WRITER_FILE_LOGGING_BUFFERED = 3,
	VERBOSE_WRITER_TRACE = 4,
	VERBOSE_WRITER_HOOK = 5
} WriterType;

/**
 * The base class for writers that do output for the verbose GC.
 * Actual writers subclass this.
 */
class MM_VerboseWriter : public MM_Base
{
	/*
	 * Data members
	 */
public:
protected:
private:
	MM_VerboseWriter *_nextWriter;

	char* _header;
	char* _footer;

	WriterType _type;
	bool _isActive;

	/*
	 * Function members
	 */
public:
	virtual void kill(MM_EnvironmentBase *env);

	MM_VerboseWriter* getNextWriter();
	void setNextWriter(MM_VerboseWriter* writer);

	virtual void outputString(MM_EnvironmentBase *env, const char* string) = 0;

	virtual bool reconfigure(MM_EnvironmentBase *env, const char *filename, uintptr_t fileCount, uintptr_t iterations) = 0;

	virtual void endOfCycle(MM_EnvironmentBase *env) = 0;

	virtual void closeStream(MM_EnvironmentBase *env) = 0;

	MMINLINE WriterType getType(void) { return _type; }

	MMINLINE bool isActive(void) { return _isActive; }
	MMINLINE void isActive(bool isActive) { _isActive = isActive; }

protected:
	MM_VerboseWriter(WriterType type);

	virtual void tearDown(MM_EnvironmentBase *env);

	bool initialize(MM_EnvironmentBase* env);

	const char* getHeader(MM_EnvironmentBase *env);
	const char* getFooter(MM_EnvironmentBase *env);
private:
};

#endif /* VERBOSEWRITER_HPP_ */
