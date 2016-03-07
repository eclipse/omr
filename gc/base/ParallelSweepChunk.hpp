/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(PARALLELSWEEPCHUNK_HPP_)
#define PARALLELSWEEPCHUNK_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#include <string.h>

#include "BaseNonVirtual.hpp"

class MM_MemoryPool;
class MM_HeapLinkedFreeHeader;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_ParallelSweepChunk : public MM_BaseNonVirtual
{
public:
	void *chunkBase;
	void *chunkTop;
	void *leadingFreeCandidate;
	uintptr_t leadingFreeCandidateSize;
	void *trailingFreeCandidate;
	uintptr_t trailingFreeCandidateSize;
	uintptr_t projection;
	MM_HeapLinkedFreeHeader *freeListHead;
	uintptr_t freeListHeadSize;
	MM_HeapLinkedFreeHeader *freeListTail;
	uintptr_t freeListTailSize;
	MM_HeapLinkedFreeHeader *previousFreeListTail; /**< previous free entry of freeListTail */
	bool _coalesceCandidate;  /**< Flag if the chunk can coalesce with the previous chunks free information */
	MM_MemoryPool *memoryPool;
	uintptr_t freeBytes;
	uintptr_t freeHoles;
	uintptr_t _darkMatterBytes;
	uintptr_t _darkMatterSamples;

	uintptr_t _scannableBytes;	/**< sum of scannable object sizes - sample only */
	uintptr_t _nonScannableBytes;	/**< sum of non-scannable object sizes - sample only */
	uintptr_t _largestFreeEntry;  /**< largest free entry found in the given chunk */
	MM_HeapLinkedFreeHeader *_previousLargestFreeEntry; /**< previous free entry of the Largest Free Entry */
	MM_ParallelSweepChunk *_previous;  /**< previous heap address ordered chunk */
	MM_ParallelSweepChunk *_next;  /**< next heap address ordered chunk */

#if defined(OMR_GC_CONCURRENT_SWEEP)
	MM_ParallelSweepChunk *_nextChunk;
	uintptr_t _concurrentSweepState;
#endif /* OMR_GC_CONCURRENT_SWEEP */

	/* Split Free List Data */
	MM_HeapLinkedFreeHeader* _splitCandidate;
	MM_HeapLinkedFreeHeader* _splitCandidatePreviousEntry;
	uintptr_t _accumulatedFreeSize;
	uintptr_t _accumulatedFreeHoles;

	/**
	 * clear the Chunk object.
	 */	
	MMINLINE void clear()
	{
		memset(this, 0, sizeof (*this));
	}

	/**
	 * Calculate the physical size of the chunk.
	 * @return the size in bytes the chunk represents on the heap.
	 */
	MMINLINE uintptr_t size() {
		return (((uintptr_t)chunkTop) - ((uintptr_t)chunkBase));
	}

	/**
	 * Update the previous largest Free Entry and the largest free entry size (only be called from SweepPoolManagerSplitAddressOrderedList)
	 */
	MMINLINE void updateLargestFreeEntry(UDATA largestFreeEntrySizeCandidate, MM_HeapLinkedFreeHeader * previousLargestFreeEntryCandidate)
	{
		if (largestFreeEntrySizeCandidate > _largestFreeEntry) {
			_previousLargestFreeEntry = previousLargestFreeEntryCandidate;
			_largestFreeEntry = largestFreeEntrySizeCandidate;
 		}
	}

	/**
	 * Create a ParallelSweepChunk object.
	 */	
	MM_ParallelSweepChunk() :
		MM_BaseNonVirtual(),
		chunkBase(NULL),
		chunkTop(NULL),
		leadingFreeCandidate(NULL),
		leadingFreeCandidateSize(0),
		trailingFreeCandidate(NULL),
		trailingFreeCandidateSize(0),
		projection(0),
		freeListHead(NULL),
		freeListHeadSize(0),
		freeListTail(NULL),
		freeListTailSize(0),
		previousFreeListTail(NULL),
		_coalesceCandidate(false),
		memoryPool(NULL),
		freeBytes(0),
		freeHoles(0),
		_darkMatterBytes(0),
		_darkMatterSamples(0),
		_scannableBytes(0),
		_nonScannableBytes(0),
		_largestFreeEntry(0),
		_previousLargestFreeEntry(NULL),
		_previous(NULL),
		_next(NULL),
		_splitCandidate(NULL),
		_splitCandidatePreviousEntry(NULL),
		_accumulatedFreeSize(0),
		_accumulatedFreeHoles(0)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* PARALLELSWEEPCHUNK_HPP_ */


