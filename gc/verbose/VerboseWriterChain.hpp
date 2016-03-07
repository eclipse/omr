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

#if !defined(VERBOSEWRITERCHAIN_HPP_)
#define VERBOSEWRITERCHAIN_HPP_

#include "omrcfg.h"
#include "omrstdarg.h"
#include "modronbase.h"

#include "Base.hpp"

#include "EnvironmentBase.hpp"

class MM_VerboseBuffer;
class MM_VerboseWriter;

/**
 * This class manages a list of writers. It formats and buffers output, flushing it
 * to the writers when asked.
 */
class MM_VerboseWriterChain : public MM_Base
{
public:
protected:
private:
	MM_VerboseBuffer *_buffer;
	MM_VerboseWriter *_writers;

public:
	static MM_VerboseWriterChain *newInstance(MM_EnvironmentBase *env);

	void kill(MM_EnvironmentBase *env);

	void formatAndOutput(MM_EnvironmentBase *env, uintptr_t indent, const char *format, ...);
	void formatAndOutputV(MM_EnvironmentBase *env, uintptr_t indent, const char *format, va_list args);
	void flush(MM_EnvironmentBase *env);

	/**
	 * Add a new verbose writer to the list of active output writers.
	 * @param writer[in] New writer to add to list.
	 */
	void addWriter(MM_VerboseWriter* writer);
	
	/**
	 * Fetch the first writer in the linked chain of writers.
	 * @return the first writer
	 */
	MM_VerboseWriter *getFirstWriter() { return _writers; }

	/**
	 * Notify each of the writers in the chain that a GC cycle has ended
	 * @param env[in] the current thread 
	 */
	void endOfCycle(MM_EnvironmentBase *env);
	
protected:
	MM_VerboseWriterChain();
	void tearDown(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase* env);
private:
};

#endif /* VERBOSEWRITERCHAIN_HPP_ */
