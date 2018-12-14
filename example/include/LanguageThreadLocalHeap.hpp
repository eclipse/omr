/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
