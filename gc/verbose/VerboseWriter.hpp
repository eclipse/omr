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

#if !defined(VERBOSEWRITER_HPP_)
#define VERBOSEWRITER_HPP_

#include "omrcfg.h"
#include "modronbase.h"

#include "Base.hpp"

#include "EnvironmentBase.hpp"

typedef enum {
	VERBOSE_WRITER_STANDARD_STREAM = 1,
	VERBOSE_WRITER_FILE_LOGGING = 2,
	VERBOSE_WRITER_TRACE = 3,
	VERBOSE_WRITER_HOOK = 4
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
