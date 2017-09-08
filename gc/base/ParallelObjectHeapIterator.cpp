/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Base
 */

#include "ModronAssertions.h"

#include "ParallelObjectHeapIterator.hpp"
#include "ObjectModel.hpp"
#include "MarkMap.hpp"

/**
 * Loop until either the end of the segment is hit, or a new chunk is acquired.
 * @return true if a new chunk was acquired
 * @return false otherwise
 */
bool
GC_ParallelObjectHeapIterator::getNextChunk()
{
	/* Loop until we hit the last chunk, or we find one we are responsible for */
	while (_segmentChunkIterator.nextChunk(_markMap, &_chunkBase, &_chunkTop)) {
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(_env)) {
			/* _chunkTop will be used later. Reseting the top address of the iterator to the end of the segment,
			 * so that iteration of the current chunk may go beyond _chunkTop, until first marked object is found.
			 * Thus we also iterate through dead objects at the begining of the each chunk. Dead objects at the
			 * beginning of the first chunk are also iterated - it is guarantied by setting _chunkBase
			 * for the first chunk to the segment base. */
			_objectHeapIterator.reset(_chunkBase, (UDATA *)_topAddress);
			return true;
		}
	}
	return false;	
}

/**
 * @see GC_ObjectHeapBufferedIterator::nextObject()
 */
omrobjectptr_t
GC_ParallelObjectHeapIterator::nextObject() 
{
	omrobjectptr_t nextObject = NULL;

	while (true) {
		/* Try to get the next object unless we hit end of the segment (what is the top address of the iterator) */
		if (NULL == (nextObject = _objectHeapIterator.nextObject())) {
			return NULL;
		}

		/* If we hit first marked object beyond the chunk boundary,
		 * get the next chunk */
		/* Note:  we can use the mark map directly, in this case, since we know that nextObject must be on the heap */
		if (((UDATA *)nextObject >= _chunkTop) && _markMap->isBitSet(nextObject)) {
			if (!getNextChunk()) {
				return NULL;
			}
		} else {
			return nextObject;
		}
	}
}

/**
 * @see GC_ObjectHeapBufferedIterator::nextObjectNoAdvance()
 * @todo Provide implementation
 */
omrobjectptr_t
GC_ParallelObjectHeapIterator::nextObjectNoAdvance() 
{
	Assert_MM_unimplemented();
	return NULL;
}

/**
 * @see GC_ObjectHeapBufferedIterator::advance()
 * @todo Provide implementation
 */
void
GC_ParallelObjectHeapIterator::advance(UDATA size) 
{
	Assert_MM_unimplemented();
}

/**
 * @see GC_ObjectHeapBufferedIterator::reset()
 */
void 
GC_ParallelObjectHeapIterator::reset(UDATA *base, UDATA *top)
{
	/* TODO: Not sure what the right thing to do here is */
	Assert_MM_unimplemented();
}

