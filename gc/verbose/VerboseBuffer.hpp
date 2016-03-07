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
  
#if !defined(VERBOSE_BUFFER_HPP_)
#define VERBOSE_BUFFER_HPP_

#include "omrcfg.h"
#include "modronbase.h"

#include "Base.hpp"
#include "EnvironmentBase.hpp"

#undef _UTE_MODULE_HEADER_
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9vgc.h"

#define INITIAL_BUFFER_SIZE 512

/**
 * Verbose buffer
 * 
 * This class represents a verbose buffer. An output agent that holds one
 * can write verbosegc output to it, where it is stored before being
 * flushed in the future.
 * @ingroup GC_verbose_output_agents
 */
class MM_VerboseBuffer : public MM_Base
{
/*
 * Member data
 */
private:
	char *_buffer; /**< Pointer to the base of the buffer */
	char *_bufferAlloc; /**< Pointer to the next char in the buffer */
	char *_bufferTop; /**< Pointer to the top of the buffer (non-inclusive) */
protected:
public:

/*
 * Member functions
 */
private:
	bool initialize(MM_EnvironmentBase *env, uintptr_t size);
	void tearDown(MM_EnvironmentBase *env);

	/**
	 * Ensure that there are at least spaceNeeded bytes left in the buffer.
	 * @param env[in] the current thread
	 * @param spaceNeeded[in] the minimum number of free bytes needed
	 * @return true on success, false if the buffer could not be expanded
	 */
	bool ensureCapacity(MM_EnvironmentBase *env, uintptr_t spaceNeeded);

protected:
	
public:
	static MM_VerboseBuffer *newInstance(MM_EnvironmentBase *env, uintptr_t size);
	virtual void kill(MM_EnvironmentBase *env);

	void reset();
	
	MMINLINE char *contents() { return _buffer; }
	MMINLINE uintptr_t currentSize() { return _bufferAlloc - _buffer; }
	MMINLINE uintptr_t freeSpace() { return _bufferTop - _bufferAlloc; }
	MMINLINE uintptr_t totalSize() { return _bufferTop - _buffer; }

	/**
	 * Append the specified NUL terminated string to the buffer.
	 * @param env[in] the current thread
	 * @param string[in] the string to append
	 * @return true on success, false if the buffer could not be expanded
	 */
	bool add(MM_EnvironmentBase *env, const char *string);
	
	/**
	 * Format the specified data and append it to the buffer.
	 * @param env[in] the current thread
	 * @param format[in] a format string; see omrstr_printf
	 * @param args[in] a va_list describing the arguments to format
	 * @return true on success, false if the buffer could not be expanded
	 */
	bool vprintf(MM_EnvironmentBase *env, const char *format, va_list args);
	
	MM_VerboseBuffer(MM_EnvironmentBase *env) :
		MM_Base(),
		_buffer(NULL),
		_bufferAlloc(NULL),
		_bufferTop(NULL)
	{}
};

#endif /* VERBOSE_BUFFER_HPP_ */
