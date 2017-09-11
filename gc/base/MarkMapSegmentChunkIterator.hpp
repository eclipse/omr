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

#if !defined(MARKMAPSEGMENTCHUNKITERATOR_HPP_)
#define MARKMAPSEGMENTCHUNKITERATOR_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "GCExtensionsBase.hpp"
#include "HeapMapIterator.hpp"

class MM_GCExtensionsBase;
class MM_HeapMap;

/**
 * Iterate over chunks of an area of memory by splitting the extent into even size chunks,
 * then using the mark map to find the first object in each chunk.
 * @note the mark map must be valid in order to use this iterator
 * @ingroup GC_Base
 */
class GC_MarkMapSegmentChunkIterator
{
private:
	MM_GCExtensionsBase * const _extensions; /**< the GC extensions for the JVM */
	UDATA _chunkSize;
	UDATA _segmentBytesRemaining;
	MM_HeapMapIterator _markedObjectIterator;
	UDATA *_nextChunkBase;

public:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	GC_MarkMapSegmentChunkIterator(MM_GCExtensionsBase *extensions, void *lowAddress, void *highAddress, UDATA chunkSize) :
		_extensions(extensions),
		_chunkSize(chunkSize),
		_segmentBytesRemaining((UDATA)highAddress - (UDATA)lowAddress),
		_markedObjectIterator(extensions),
		_nextChunkBase((UDATA *)lowAddress)
	{};

	/**
	 * @note Any chunk returned from this method must have either an object or a free header beginning at
	 * the first slot in the chunk. 
	 * 
	 * @param markMap[in] The mark map to use when finding the next chunk
	 * @param base (OUT parameter) a pointer to the base of the next chunk will be stored into this address
	 * @param top (OUT parameter) a pointer to the top of the next chunk will be stored into this address
	 * @return true if there was a chunk available
	 * @return false if there were no more chunks
	 */
	bool nextChunk(MM_HeapMap *markMap, UDATA **base, UDATA **top);
};

#endif /* MARKMAPSEGMENTCHUNKITERATOR_HPP_ */

