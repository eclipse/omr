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

#include "MarkMapSegmentChunkIterator.hpp"

#include "Heap.hpp"
#include "HeapRegionManager.hpp"

/**
 * @see GC_MarkMapSegmentChunkIterator::nextChunk()
 */
bool
GC_MarkMapSegmentChunkIterator::nextChunk(MM_HeapMap *markMap, UDATA **base, UDATA **top)
{
	while (_segmentBytesRemaining > 0) {
		UDATA thisChunkSize = OMR_MIN(_segmentBytesRemaining, _chunkSize);
		UDATA *chunkTop = (UDATA *)((U_8 *)_nextChunkBase + thisChunkSize);
		_segmentBytesRemaining -= thisChunkSize;
		
		/* There may not be a marked object in this chunk */
		/* If that is the case, we move on to the next chunk */
		_markedObjectIterator.reset(markMap, _nextChunkBase, chunkTop);
		omrobjectptr_t firstObject = _markedObjectIterator.nextObject();

		_nextChunkBase = chunkTop;

		if (firstObject != NULL) {
			*base = (UDATA *)firstObject;
			*top = chunkTop;
			if (_extensions->isVLHGC()) {
				/* while this code should work in phase2, the markedObjectIterator can give back a chunkTop which points after an unused region */
				MM_HeapRegionManager *manager = _extensions->getHeap()->getHeapRegionManager();
				MM_HeapRegionDescriptor *desc = manager->tableDescriptorForAddress(firstObject);
				MM_HeapRegionDescriptor *checkDesc = manager->tableDescriptorForAddress((void *)((UDATA)chunkTop - 1));
				Assert_MM_true(desc->_headOfSpan == checkDesc->_headOfSpan);
				Assert_MM_true(desc->isCommitted());
			}
			return true;
		}
	}
	return false;
}

