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

#include <string.h>

#include "VerboseBuffer.hpp"

#include "omrstdarg.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

/**
 * Instantiate a new buffer object
 * @param size Buffer size
 */
MM_VerboseBuffer *
MM_VerboseBuffer::newInstance(MM_EnvironmentBase *env, uintptr_t size)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	MM_VerboseBuffer *verboseBuffer = (MM_VerboseBuffer *) extensions->getForge()->allocate(sizeof(MM_VerboseBuffer), MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if(NULL != verboseBuffer) {
		new(verboseBuffer) MM_VerboseBuffer(env);
		if (!verboseBuffer->initialize(env, size)) {
			verboseBuffer->kill(env);
			verboseBuffer = NULL;
		}
	}
	return verboseBuffer;
}

/**
 * Initialize the buffer object
 * @param size Buffer size
 */
bool
MM_VerboseBuffer::initialize(MM_EnvironmentBase *env, uintptr_t size)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	if(0 == size) {
		return false;
	}
	
	if(NULL == (_buffer = (char *) extensions->getForge()->allocate(size, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE()))) {
		return false;
	}
	
	_bufferTop = _buffer + size;
	reset();

	return true;
}

/**
 * Free the buffer object
 */
void
MM_VerboseBuffer::kill(MM_EnvironmentBase *env)
{
	tearDown(env);

	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	extensions->getForge()->free(this);
}

/**
 * Free the buffer itself
 */
void
MM_VerboseBuffer::tearDown(MM_EnvironmentBase *env)
{
	if(NULL != _buffer) {
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
		extensions->getForge()->free(_buffer);
	}
}

/**
 * Add a string to the buffer
 * 
 * Concatenates a string to the end of the buffer's
 * current contents
 * 
 * @param string String to add
 * @return true on success, false on failure
 */
bool
MM_VerboseBuffer::add(MM_EnvironmentBase *env, const char *string)
{
	bool result = true;
	uintptr_t stringLength = strlen(string);
	/* we will need space for the string but also ensure that we aren't going to overrun the buffer with the NUL byte */
	uintptr_t spaceNeeded = stringLength + 1;
	
	if(ensureCapacity(env, spaceNeeded)) {
		strcpy(_bufferAlloc, string);
		_bufferAlloc += stringLength;
		result = true;
	} else {
		result = false;
	}
	
	return result;
}

bool
MM_VerboseBuffer::ensureCapacity(MM_EnvironmentBase *env, uintptr_t spaceNeeded)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	bool result = true;
	
	if(freeSpace() < spaceNeeded) {
		/* Not enough space in the current buffer - try to alloc a larger one and use that */
		char *oldBuffer = _buffer;
		uintptr_t currentSize = this->currentSize();
		uintptr_t newStringLength = currentSize + spaceNeeded;
		uintptr_t newSize = newStringLength + (newStringLength / 2);
		char* newBuffer = (char *) extensions->getForge()->allocate(newSize, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
		if(NULL == newBuffer) {
			result = false;
		} else {
			_buffer = newBuffer;

			/* Got a new buffer - initialize it */
			_bufferTop = _buffer + newSize;
			reset();
		
			/* Copy across the contents of the old buffer */
			strcpy(_buffer, oldBuffer);
			_bufferAlloc += currentSize;
				
			/* Delete the old buffer */
			extensions->getForge()->free(oldBuffer);
		}
	}
	return result;
}


bool
MM_VerboseBuffer::vprintf(MM_EnvironmentBase *env, const char *format, va_list args)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	bool result = true;
	uintptr_t spaceFree = freeSpace();
	va_list argsCopy;

	Assert_VGC_true('\0' == _bufferAlloc[0]);

	COPY_VA_LIST(argsCopy, args);
	uintptr_t spaceUsed = omrstr_vprintf(_bufferAlloc, spaceFree, format, argsCopy);

	/* account for the '\0' which isn't included in spaceUsed */
	if ((spaceUsed + 1) < spaceFree) {
		/* the string fit in the buffer */
		_bufferAlloc += spaceUsed;
		Assert_VGC_true('\0' == _bufferAlloc[0]);
	} else {
		/* undo anything that might have been written by the failed call to omrstr_vprintf */
		_bufferAlloc[0] = '\0';
		
		/* grow the buffer and try again */
		COPY_VA_LIST(argsCopy, args);
		uintptr_t spaceNeeded = omrstr_vprintf(NULL, 0, format, argsCopy);
		if (ensureCapacity(env, spaceNeeded)) {
			COPY_VA_LIST(argsCopy, args);
			spaceUsed = omrstr_vprintf(_bufferAlloc, freeSpace(), format, argsCopy);
			Assert_VGC_true(spaceUsed < freeSpace());
			_bufferAlloc += spaceUsed;
			Assert_VGC_true('\0' == _bufferAlloc[0]);
		} else {
			/* failed to expand buffer */
			result = false;
		}
	}

	return result;
}

/**
 * Reset the buffer
 */
void
MM_VerboseBuffer::reset()
{
	_bufferAlloc = _buffer;
	_buffer[0] = '\0';
}
