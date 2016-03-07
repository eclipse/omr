/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#ifndef LANGUAGETHREADLOCALHEAP_HPP_
#define LANGUAGETHREADLOCALHEAP_HPP_

#include "omr.h"

#if defined(OMR_GC_THREAD_LOCAL_HEAP)

typedef struct LanguageThreadLocalHeapStruct {
    uint8_t* heapBase;
    uint8_t* realHeapAlloc;
    uintptr_t objectFlags;
    uintptr_t refreshSize;
    void* memorySubSpace;
    void* memoryPool;
} LanguageThreadLocalHeapStruct;


class MM_LanguageThreadLocalHeap {

private:
	LanguageThreadLocalHeapStruct allocateThreadLocalHeap;
	LanguageThreadLocalHeapStruct nonZeroAllocateThreadLocalHeap;

	uint8_t* nonZeroHeapAlloc;
	uint8_t* heapAlloc;

	uint8_t* nonZeroHeapTop;
	uint8_t* heapTop;

	intptr_t nonZeroTlhPrefetchFTA;
	intptr_t tlhPrefetchFTA;

public:
	LanguageThreadLocalHeapStruct* getLanguageThreadLocalHeapStruct(MM_EnvironmentBase* env, bool zeroTLH)
	{
#if defined(OMR_GC_NON_ZERO_TLH)
		if (!zeroTLH) {
			return &nonZeroAllocateThreadLocalHeap;
		}
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
		return &allocateThreadLocalHeap;
	}

	uint8_t ** getPointerToHeapAlloc(MM_EnvironmentBase* env, bool zeroTLH) {
#if defined(OMR_GC_NON_ZERO_TLH)
		if (!zeroTLH) {
			return &nonZeroHeapAlloc;
		}
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
		return &heapAlloc;
	}

	uint8_t ** getPointerToHeapTop(MM_EnvironmentBase* env, bool zeroTLH) {
#if defined(OMR_GC_NON_ZERO_TLH)
		if (!zeroTLH) {
			return &nonZeroHeapTop;
		}
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
		return &heapTop;
	}

	intptr_t * getPointerToTlhPrefetchFTA(MM_EnvironmentBase* env, bool zeroTLH) {
#if defined(OMR_GC_NON_ZERO_TLH)
		if (!zeroTLH) {
			return &nonZeroTlhPrefetchFTA;
		}
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
		return &tlhPrefetchFTA;
	}

	MM_LanguageThreadLocalHeap() :
		allocateThreadLocalHeap(),
		nonZeroAllocateThreadLocalHeap(),
		nonZeroHeapAlloc(NULL),
		heapAlloc(NULL),
		nonZeroHeapTop(NULL),
		heapTop(NULL),
		nonZeroTlhPrefetchFTA(0),
		tlhPrefetchFTA(0)
	{};

};

#endif /* OMR_GC_THREAD_LOCAL_HEAP */

#endif /* LANGUAGETHREADLOCALHEAP_HPP_ */
