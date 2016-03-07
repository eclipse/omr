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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Heap Utilities
 */
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"

#include <string.h>

struct J9Heap {
	uintptr_t heapSize; /* total size of the heap in number of slots */
	uintptr_t firstFreeBlock; /* slot number of the first free block within the heap */
	uintptr_t lastAllocSlot; /* slot number for the last allocation */
	uintptr_t largestAllocSizeVisited; /* largest free list entry visited while performing the last allocation */
};

#define ALIGNMENT_ROUND_DOWN(value) (((uintptr_t) value) & (~(sizeof(uint64_t) - 1)))
#define ALIGNMENT_ROUND_UP(value) ((((uintptr_t) value) + (sizeof(uint64_t) - 1)) & (~(sizeof(uint64_t) - 1)))

#define GET_SLOT_NUMBER_FROM(heapBase,heapSlot) ((((uintptr_t)heapSlot)-((uintptr_t)heapBase))/sizeof(uint64_t))

/* Amount of metadata used for heap management in bytes. A velid heap size should be at least larger than this.
 * We account for the header size plus 2 padding slots for the initial block in the heap.
 */
#define HEAP_MANAGEMENT_OVERHEAD (sizeof(J9Heap)+2*sizeof(uint64_t))

/**
* Initialize a contiguous region of memory at heapBase as a heap. The size of the heap is bounded by heapSize.
*
* @param[in] portLibrary The port library
* @param[in] heapBase Base address of memory region.
* @param[in] heapSize The size of the memory region to be used as a heap in bytes.
* @param[in] heapFlags Flags that can affect the heap. Currently, this is not used and is there for future use, for example, to zero out memory. User must pass in zero.
*
* @return pointer to an opaque struct representing the heap on success, NULL on failure.
*
* @note this function can also be used to re-initialize a heap -- useful when we want to erase its contents and start from scratch.
*
* @note in case heapBase isn't 8-aligned, it will be rounded up to the nearest 8-aligned value and the heap will be created at the 8-aligned value. The same goes for heapSize, it will be rounded down if not 8 aligned.
*
* @note the algorithm used in this suballocator is based on the first-fit method in KNUTH, D. E. The Art of Computer Programming. Vol. 1: Fundamental Algorithms. (2nd edition). Addison-Wesley, Reading, Mass., 1973, Sect. 2.5.
*
* @note due to the overhead of heap management, the actual available space consumed by the user is less than the size of the heap.
*/
struct J9Heap *
omrheap_create(struct OMRPortLibrary *portLibrary, void *heapBase, uintptr_t heapSize, uint32_t heapFlags)
{
	struct J9Heap *adjustedHeapBase = NULL;
	uintptr_t heapBaseDelta, adjustedHeapSize;
	uintptr_t numSlots = 0;
	uintptr_t blockSize = 0;
	uint64_t *baseSlot;

	Trc_PRT_heap_port_omrheap_create_entry(heapBase, heapSize, heapFlags);

	if (NULL == heapBase) {
		Trc_PRT_heap_port_omrheap_create_null_heapBase_exit();
		return NULL;
	}

	/* first we round up heapBase */
	adjustedHeapBase = (struct J9Heap *)ALIGNMENT_ROUND_UP(heapBase);
	heapBaseDelta = ((uintptr_t)adjustedHeapBase) - ((uintptr_t)heapBase);

	/* check if we have enough space taking into account space wasted in rounding up heapBase */
	if (heapSize <= (HEAP_MANAGEMENT_OVERHEAD + heapBaseDelta)) {
		Trc_PRT_heap_port_omrheap_create_insufficient_heapSize_exit();
		return NULL;
	}
	adjustedHeapSize = heapSize - heapBaseDelta;

	/* now we round down the heap size */
	adjustedHeapSize = ALIGNMENT_ROUND_DOWN(adjustedHeapSize);
	if (adjustedHeapSize <= HEAP_MANAGEMENT_OVERHEAD) {
		Trc_PRT_heap_port_omrheap_create_insufficient_heapSize_exit();
		return NULL;
	}

	numSlots = adjustedHeapSize / sizeof(uint64_t);
	blockSize = numSlots - (HEAP_MANAGEMENT_OVERHEAD / sizeof(uint64_t));

	/* initialize the first 2 slots */
	adjustedHeapBase->heapSize = numSlots;
	adjustedHeapBase->firstFreeBlock = (sizeof(J9Heap) / sizeof(uint64_t));

	/* initialize the top and bottom padding slots */
	baseSlot = (uint64_t *)adjustedHeapBase;
	baseSlot[adjustedHeapBase->firstFreeBlock] = blockSize;
	baseSlot[numSlots - 1] = blockSize;

	adjustedHeapBase->lastAllocSlot = adjustedHeapBase->firstFreeBlock;
	adjustedHeapBase->largestAllocSizeVisited = blockSize;

	Trc_PRT_heap_port_omrheap_create_exit(adjustedHeapBase);

	return adjustedHeapBase;
}

/**
* Suballocate byteAmount bytes from the initialized heap.
*
* @param[in] portLibrary The port library
* @param[in] heap Opaque heap pointer.
* @param[in] byteAmount number of bytes to be suballocated from the heap.
*
* @return pointer to suballocated memory on success, NULL on failure.
*
* @note omrheap_allocate and omrheap_free are not thread safe. It is up to the user to serialize access if necessary.
*/
void *
omrheap_allocate(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, uintptr_t byteAmount)
{
	uintptr_t heapSize = heap->heapSize;
	uintptr_t firstFreeBlock = heap->firstFreeBlock;
	uintptr_t largestChunkSize = 0;
	uintptr_t adjustedRequestSize = 0;
	int64_t *baseSlot = (int64_t *)heap;
	int64_t *lastSlot = &baseSlot[heapSize - 1];
	int64_t *firstFreeCursor = &baseSlot[firstFreeBlock];
	int64_t *blockPaddingCursor = NULL;
	int64_t *nextFreeBlockPaddingCursor = NULL;
	int64_t *candidateBlock = NULL;
	int64_t chunkSize = 0;
	int64_t newSize = 0;
	int64_t residualSize = 0;
	BOOLEAN isFirstFree = FALSE;

	Trc_PRT_heap_port_omrheap_allocate_entry(heap, byteAmount);

	/* firstFreeBlock is 0 means no free space left on the heap */
	if (0 == firstFreeBlock) {
		Trc_PRT_heap_port_omrheap_allocate_heap_full_exit();
		return NULL;
	}

	if (0 == byteAmount) {
		/* if 0 size requested, return just 1 slot */
		adjustedRequestSize = 1;
	} else {
		/* round up byteAmount to nearest 8-aligned value and calculate num of slots required */
		adjustedRequestSize = (ALIGNMENT_ROUND_UP(byteAmount)) / sizeof(uint64_t);
		/* In case of arithmetic overflow, return NULL. */
		if (byteAmount > (adjustedRequestSize * sizeof(uint64_t))) {
			Trc_PRT_heap_port_omrheap_allocate_arithmetic_overflow(byteAmount);
			Trc_PRT_heap_port_omrheap_allocate_exit(NULL);
			return NULL;
		}
	}

	/* even the entire heap won't fit */
	if (adjustedRequestSize > heapSize) {
		Trc_PRT_heap_port_omrheap_allocate_cannot_satisfy_reuqest_exit();
		return NULL;
	}

	/* if the request size is less than the largest size seen up to the point
	 * of the last allocation then start at the first free entry
	 */
	if (adjustedRequestSize <= heap->largestAllocSizeVisited) {
		blockPaddingCursor = firstFreeCursor;
	} else {
		/* since request size is larger than the biggest entry visted during the
		 * last allocation start off where it finished
		 */
		blockPaddingCursor = &baseSlot[heap->lastAllocSlot];
		largestChunkSize = heap->largestAllocSizeVisited;
	}

	/* search for a free block */
	while (blockPaddingCursor < lastSlot) {
		chunkSize = *blockPaddingCursor;
		/*assertion to check the size slot is never 0*/
		Assert_PRT_true(chunkSize != 0);
		if (chunkSize < 0) {
			/* this block is occupied, go to the next chunk. */
			/* We need to skip the 2 slot used as block padding. */
			blockPaddingCursor += ((-chunkSize) + 2);
		} else {
			if ((uintptr_t)chunkSize >= adjustedRequestSize) {
				/* found the chunk, now calculate the residual size after the allocation*/
				newSize = (int64_t)adjustedRequestSize;
				residualSize = chunkSize - newSize;
				if (blockPaddingCursor == firstFreeCursor) {
					isFirstFree = TRUE;
				}
				break;
			} else {
				blockPaddingCursor += (chunkSize + 2);
				if ((uintptr_t)chunkSize > largestChunkSize) {
					largestChunkSize = (uintptr_t)chunkSize;
				}
			}
		}
	}

	if (blockPaddingCursor >= lastSlot) {
		/* we've walked the entire heap and couldn't find a free block large enough */
		Trc_PRT_heap_port_omrheap_allocate_cannot_satisfy_reuqest_exit();
		return NULL;
	}

	/* candidateBlock points to the first slot of the candidate free block */
	candidateBlock = &blockPaddingCursor[1];

	/* need 2 slots for padding, hence we only make a new free block if we have 3 or more slots left */
	if (residualSize >= 3) {
		/* write the negated value of newSize to the top and bottom padding slot of the allocated block */
		blockPaddingCursor[0] = -newSize;
		blockPaddingCursor[newSize + 1] = -newSize;
		/* fill in the top and bottom padding of the new free block */
		residualSize -= 2;
		blockPaddingCursor[newSize + 2] = residualSize;
		blockPaddingCursor[chunkSize + 1] = residualSize;
		if (isFirstFree) {
			heap->firstFreeBlock = (uintptr_t)GET_SLOT_NUMBER_FROM(heap, &blockPaddingCursor[newSize + 2]);
			heap->lastAllocSlot = heap->firstFreeBlock;
			heap->largestAllocSizeVisited = 0;
		} else {
			heap->lastAllocSlot = (uintptr_t)GET_SLOT_NUMBER_FROM(heap, &blockPaddingCursor[newSize + 2]);
			heap->largestAllocSizeVisited = largestChunkSize;
		}
	} else {
		/* only have 2 slots or less left, don't bother creating a new free block */
		blockPaddingCursor[0] = -chunkSize;
		blockPaddingCursor[chunkSize + 1] = -chunkSize;
		if (isFirstFree) {
			/* first we set heap->firstFreeBlock to 0.
			 * If we later find the next free block, it will be overwritten.
			 */
			heap->firstFreeBlock = 0;
			/* we've used up the first free block, find the next free block */
			nextFreeBlockPaddingCursor = &blockPaddingCursor[chunkSize + 2];
			while (nextFreeBlockPaddingCursor < lastSlot) {
				int64_t size = *nextFreeBlockPaddingCursor;
				if (size > 0) {
					heap->firstFreeBlock = (uintptr_t)GET_SLOT_NUMBER_FROM(heap, nextFreeBlockPaddingCursor);
					heap->lastAllocSlot = heap->firstFreeBlock;
					heap->largestAllocSizeVisited = 0;
					break;
				} else {
					size = -size;
				}
				nextFreeBlockPaddingCursor += (size + 2);
			}
		}
	}
	Trc_PRT_heap_port_omrheap_allocate_exit(candidateBlock);
	return candidateBlock;
}

/**
* Free a chunk of memory that was allocated from the heap. address is a memory location returned from calling heap_allocate on the heap
*
* @param[in] portLibrary The port library
* @param[in] heap Opaque pointer of  a heap.
* @param[in] address  Memory location within the heap to be freed, or NULL.
*
* @return nothing.
*
* @note passing in a NULL address is OK. omrheap_free simply returns in this case.
* @note omrheap_allocate and omrheap_free are not thread safe. It is up to the user to serialize access if necessary.
*/
void
omrheap_free(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address)
{
	int64_t *thisBlockTopPadding, *previousBlockBottomPadding, *nextBlockTopPadding;
	int64_t thisBlockSize, previousBlockSize, nextBlockSize;
	uintptr_t firstFreeBlock = heap->firstFreeBlock;
	uintptr_t totalNumSlots = heap->heapSize;
	uintptr_t blockTopSlot;
	int64_t *baseSlot = (int64_t *)heap;

	Trc_PRT_heap_port_omrheap_free_entry(heap, address);

	if (NULL == address) {
		Trc_PRT_heap_port_omrheap_free_null_memptr_exit();
		return;
	}

	thisBlockTopPadding = ((int64_t *)address) - 1;

	/*assertion to check we have an occupied block*/
	Assert_PRT_true(thisBlockTopPadding[0] < 0);

	/* negate the negative value to indicate free block */
	thisBlockSize = -thisBlockTopPadding[0];

	blockTopSlot = GET_SLOT_NUMBER_FROM(heap, thisBlockTopPadding);

	/*determine if the block freed is the first block by checking its slot number. If so there is no previous block to check.*/
	if (blockTopSlot != (sizeof(J9Heap) / sizeof(uint64_t))) {
		previousBlockBottomPadding = &thisBlockTopPadding[-1];
		previousBlockSize = *previousBlockBottomPadding;

		if (previousBlockSize > 0) {
			/* adjust the this block size and top padding location */
			thisBlockSize += (previousBlockSize + 2);
			thisBlockTopPadding = &previousBlockBottomPadding[-previousBlockSize - 1];
			blockTopSlot = GET_SLOT_NUMBER_FROM(heap, thisBlockTopPadding);
			if (previousBlockBottomPadding == &baseSlot[heap->lastAllocSlot - 1]) {
				heap->lastAllocSlot = blockTopSlot;
			}
		}
	}
	thisBlockTopPadding[0] = thisBlockSize;
	thisBlockTopPadding[thisBlockSize + 1] = thisBlockSize;

	/*determine if the block freed is the last block by checking its slot number. If so there is no next block to check.*/
	if (GET_SLOT_NUMBER_FROM(heap, &thisBlockTopPadding[thisBlockSize + 1]) != (totalNumSlots - 1)) {
		nextBlockTopPadding = &thisBlockTopPadding[thisBlockSize + 2];
		nextBlockSize = nextBlockTopPadding[0];

		if (nextBlockSize > 0) {
			/* combine this block and next block */
			thisBlockSize += (nextBlockSize + 2);
			thisBlockTopPadding[0] = thisBlockSize;
			thisBlockTopPadding[thisBlockSize + 1] = thisBlockSize;
			if (nextBlockTopPadding == &baseSlot[heap->lastAllocSlot]) {
				heap->lastAllocSlot = blockTopSlot;
			}
		}
	}

	/* 0 firstFreeBlock means the heap was previously full */
	if ((0 == firstFreeBlock) || (blockTopSlot < firstFreeBlock)) {
		heap->firstFreeBlock = blockTopSlot;
		heap->largestAllocSizeVisited = 0;
		heap->lastAllocSlot = blockTopSlot;
	} else if (blockTopSlot < heap->lastAllocSlot) {
		if ((uintptr_t)thisBlockSize > heap->largestAllocSizeVisited) {
			heap->largestAllocSizeVisited = (uintptr_t)thisBlockSize;
		}
	}

	Trc_PRT_heap_port_omrheap_free_exit();
}

/**
* Change the size of a chunk of memory that was allocated from the heap.
* If omrheap_reallocate() fails, it will return NULL to indicate that
* allocation failed, but will not free the memory chunk passed in.
*
* @param[in] portLibrary The port library
* @param[in] heap Opaque heap pointer.
* @param[in] address Memory location within the heap to be resized.
* @param[in] byteAmount New size for the memory chunk.
*
* @return pointer to re-allocated memory on success, NULL on failure.
*
* @note passing in a NULL address is the same as calling omrheap_allocate().
* @note omrheap_reallocate() is not thread safe. It is up to the user to serialize access if necessary.
*/
void *
omrheap_reallocate(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address, uintptr_t byteAmount)
{
	int64_t *baseSlot = (int64_t *)heap;
	int64_t *lastSlot = &baseSlot[heap->heapSize - 1];
	int64_t *thisBlockTopPadding, *nextBlockTopPadding = NULL;
	int64_t thisBlockSize, nextBlockSize = 0;
	int64_t adjustedRequestSize, growAmount;
	BOOLEAN isLastBlock;
	BOOLEAN nextBlockIsFirstFreeBlock = FALSE;

	Trc_PRT_heap_port_omrheap_reallocate_entry(heap, address, byteAmount);

	if (NULL == address) {
		Trc_PRT_heap_port_omrheap_reallocate_null_address();
		address = omrheap_allocate(portLibrary, heap, byteAmount);
		Trc_PRT_heap_port_omrheap_reallocate_exit(address);
		return address;
	}

	thisBlockTopPadding = ((int64_t *)address) - 1;
	thisBlockSize = -thisBlockTopPadding[0];
	Assert_PRT_true(thisBlockSize > 0);
	Assert_PRT_true(thisBlockSize == -thisBlockTopPadding[thisBlockSize + 1]);

	if (0 == byteAmount) {
		/* If size requested is 0, resize to just 1 slot. */
		adjustedRequestSize = 1;
	} else {
		/* Round up byteAmount to nearest 8-aligned value and calculate number of slots required. */
		adjustedRequestSize = (int64_t)(ALIGNMENT_ROUND_UP(byteAmount)) / sizeof(uint64_t);
		/* In case of arithmetic overflow, return NULL. */
		if (byteAmount > (adjustedRequestSize * sizeof(uint64_t))) {
			Trc_PRT_heap_port_omrheap_reallocate_arithmetic_overflow(byteAmount);
			Trc_PRT_heap_port_omrheap_reallocate_exit(NULL);
			return NULL;
		}
	}

	growAmount = adjustedRequestSize - thisBlockSize;
	if (0 == growAmount) {
		Trc_PRT_heap_port_omrheap_reallocate_no_realloc_necessary();
		Trc_PRT_heap_port_omrheap_reallocate_exit(address);
		return address;
	}

	isLastBlock = GET_SLOT_NUMBER_FROM(heap, &thisBlockTopPadding[thisBlockSize + 1]) == (heap->heapSize - 1);
	if (FALSE == isLastBlock) {
		nextBlockTopPadding = &thisBlockTopPadding[thisBlockSize + 2];
		nextBlockSize = nextBlockTopPadding[0];

		if (0 != heap->firstFreeBlock) {
			int64_t *firstFreeBlockTopPadding = &baseSlot[heap->firstFreeBlock];

			nextBlockIsFirstFreeBlock = (nextBlockTopPadding == firstFreeBlockTopPadding);
		}
	}

	if (growAmount > 0) {
		/*
		 * Relocate if its either the last block, or the next
		 * block taken or the next block is too small.
		 */
		if (isLastBlock || (nextBlockSize < 0) || (nextBlockSize + 2 < growAmount)) {
			/* If there is not enough free space following, we must relocate. */
			void *newAddress;

			Trc_PRT_heap_port_omrheap_reallocate_relocating();
			newAddress = omrheap_allocate(portLibrary, heap, byteAmount);
			if (NULL != newAddress) {
				memcpy(newAddress, address, (size_t)(thisBlockSize * sizeof(uint64_t)));
				omrheap_free(portLibrary, heap, address);
			}
			address = newAddress;
		} else {
			int64_t residualSize = nextBlockSize + 2 - growAmount;

			Trc_PRT_heap_port_omrheap_reallocate_grow(growAmount, residualSize);
			if (residualSize >= 3) {
				thisBlockSize += growAmount;
				thisBlockTopPadding[0] = -thisBlockSize;
				thisBlockTopPadding[thisBlockSize + 1] = -thisBlockSize;
				nextBlockTopPadding = &thisBlockTopPadding[thisBlockSize + 2];
				nextBlockSize -= growAmount;
				nextBlockTopPadding[0] = nextBlockSize;
				nextBlockTopPadding[nextBlockSize + 1] = nextBlockSize;

				if (nextBlockIsFirstFreeBlock) {
					heap->firstFreeBlock = GET_SLOT_NUMBER_FROM(heap, nextBlockTopPadding);
				}
			} else {
				/* Only 2 slots or less left, consume the next block completely. */
				thisBlockSize += growAmount + residualSize;
				thisBlockTopPadding[0] = -thisBlockSize;
				thisBlockTopPadding[thisBlockSize + 1] = -thisBlockSize;

				if (nextBlockIsFirstFreeBlock) {
					/* Find the first free block, if it exists. */
					heap->firstFreeBlock = 0;
					nextBlockTopPadding = &thisBlockTopPadding[thisBlockSize + 2];
					while (nextBlockTopPadding < lastSlot) {
						nextBlockSize = nextBlockTopPadding[0];
						if (nextBlockSize > 0) {
							heap->firstFreeBlock = GET_SLOT_NUMBER_FROM(heap, nextBlockTopPadding);
							break;
						}
						nextBlockTopPadding = &nextBlockTopPadding[-nextBlockSize + 2];
					}
				}
			}
		}
	} else {
		Trc_PRT_heap_port_omrheap_reallocate_shrink(growAmount);

		/*
		 * Shrink request - only shrink it if it makes sense to do.
		 * NOTE: growAmount is negative.
		 */
		if ((FALSE == isLastBlock) && (nextBlockSize > 0)) {
			/* Next block is free, so add the extra space to it. */
			thisBlockSize += growAmount;
			thisBlockTopPadding[0] = -thisBlockSize;
			thisBlockTopPadding[thisBlockSize + 1] = -thisBlockSize;
			nextBlockTopPadding = &thisBlockTopPadding[thisBlockSize + 2];
			nextBlockSize -= growAmount;
			nextBlockTopPadding[0] = nextBlockSize;
			nextBlockTopPadding[nextBlockSize + 1] = nextBlockSize;

			if (nextBlockIsFirstFreeBlock) {
				heap->firstFreeBlock = GET_SLOT_NUMBER_FROM(heap, nextBlockTopPadding);
			}
		} else if (growAmount < -2) {
			/*
			 * Either this is the last block, or the next block is occupied.
			 * Only create a new block if the gained space is 3 slots or more.
			 */
			uintptr_t nextBlockSlot;

			thisBlockSize += growAmount;
			thisBlockTopPadding[0] = -thisBlockSize;
			thisBlockTopPadding[thisBlockSize + 1] = -thisBlockSize;
			nextBlockTopPadding = &thisBlockTopPadding[thisBlockSize + 2];
			nextBlockSize = -growAmount - 2;
			nextBlockTopPadding[0] = nextBlockSize;
			nextBlockTopPadding[nextBlockSize + 1] = nextBlockSize;

			/* Set the new block as the firstFreeBlock if necessary. */
			nextBlockSlot = GET_SLOT_NUMBER_FROM(heap, nextBlockTopPadding);
			if ((0 == heap->firstFreeBlock) || (nextBlockSlot < heap->firstFreeBlock)) {
				heap->firstFreeBlock = nextBlockSlot;
			}
		}
	}

	/* NOTE: take the safe approach of just resetting to the first free block */
	heap->largestAllocSizeVisited = 0;
	heap->lastAllocSlot = heap->firstFreeBlock;

	Trc_PRT_heap_port_omrheap_reallocate_exit(address);
	return address;
}

/**
* Queries the size of a block of memory in the heap
*
* @param[in] portLibrary The port library
* @param[in] heap Opaque heap pointer.
* @param[in] address Memory location returned from omrheap_allocate or omrheap_reallocate
*
* @return size of the allocation in bytes
*
* @note omrheap_query_size() is not thread safe. It is up to the user to serialize access if necessary.
*/
uintptr_t
omrheap_query_size(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address)
{
	int64_t *thisBlockTopPadding = ((int64_t *)address) - 1;
	uintptr_t toReturn;

	Trc_PRT_heap_port_omrheap_query_size_Entry(portLibrary, heap, address);

	/*assertion to check we have an occupied block*/
	Assert_PRT_true(thisBlockTopPadding[0] < 0);

	toReturn = (uintptr_t) - thisBlockTopPadding[0] * sizeof(int64_t);

	Trc_PRT_heap_port_omrheap_query_size_Exit(toReturn);

	return toReturn;
}


/**
* Increase the bounds of the heap by Suballocate byteAmount bytes from the initialized heap.
* Grow is assumed to happen at the end of the memory range originally used for the heap, i.e.
* ideal for situations where the heap is backed by uncommitted virtual memory.
*
* @param[in] portLibrary The port library
* @param[in] heap Opaque heap pointer.
* @param[in] byteAmount number of bytes to be suballocated from the heap.
*
* @return TRUE on success, FALSE otherwise.
*
* @note omrheap_grow is not thread safe. It is up to the user to serialize access if necessary.
*/
BOOLEAN
omrheap_grow(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, uintptr_t growAmount)
{
	uintptr_t heapSize = heap->heapSize;
	uintptr_t firstFreeBlock = heap->firstFreeBlock;

	uintptr_t adjustedGrowAmount;
	uintptr_t numSlots;
	BOOLEAN result = TRUE;
	int64_t temp;
	int64_t *baseSlot;

	Trc_PRT_heap_port_omrheap_grow_entry(heap, growAmount);

	/* Figure out the usable size of the new region */
	adjustedGrowAmount = ALIGNMENT_ROUND_DOWN(growAmount);

	/* And the number of extra slots that we were given */
	numSlots = adjustedGrowAmount / sizeof(uint64_t);
	if (numSlots <= 4) {
		Trc_PRT_heap_port_omrheap_grow_insufficient_heapSize_exit();
		return FALSE;
	}
	/*
	 * Merge the new free slots with the free slots (if there is any) at the end of the current heap.
	 * Initialize the header and tail of the newly added slots.
	 * Head/Tail should be set to the number of free slots between them
	 */
	baseSlot = (int64_t *)heap;
	/* Check whether the tail of the heap is free or not. */
	temp = baseSlot[heapSize - 1];
	/* If the value at the tail node is negative, then it is occupied, if not, it is free */
	if (0 > temp) {
		baseSlot[heapSize] = numSlots - 2;
		baseSlot[heapSize + numSlots - 1] = numSlots - 2;
	} else {
		baseSlot[heapSize - temp - 2] = numSlots + temp;
		baseSlot[heapSize + numSlots - 1] = numSlots + temp;
	}

	/* If firstFreeBlock is not 0, then leave it alone,
	 * otherwise previous heap was completely full and firstFreeBlock should point to the newly added slots */
	if (0 == firstFreeBlock) {
		/* firstFreeBlock is 0 means current heap is completely full */
		heap->firstFreeBlock = heapSize;
	}

	/* Finally bump up the heap size */
	heap->heapSize = heapSize + numSlots;

	Trc_PRT_heap_port_omrheap_grow_exit(result);
	return result;
}
