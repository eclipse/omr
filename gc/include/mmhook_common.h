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

/*
 * Shared structs between internal and external mmhook interfaces
 */

#ifndef MMHOOK_H_
#define MMHOOK_H_

#include "omr.h"

/**
 * Defines data which is common to all types of GC start and end events.
 */
typedef struct MM_CommonGCData {
	uintptr_t nurseryFreeBytes; /**&lt; number of bytes free in the nursery */
	uintptr_t nurseryTotalBytes; /**&lt; total number of bytes in the nursery */
	uintptr_t tenureFreeBytes; /**&lt; number of bytes free in the tenure area */
	uintptr_t tenureTotalBytes; /**&lt; total number of bytes in the tenure area */
	uintptr_t loaEnabled; /**&lt; flag to indicate whether or not the LOA is enabled */
	uintptr_t tenureLOAFreeBytes; /**&lt; number of bytes free in the LOA */
	uintptr_t tenureLOATotalBytes; /**&lt; total number of bytes in the LOA */
	uintptr_t immortalFreeBytes; /**&lt; number of bytes free in the immortal memory area */
	uintptr_t immortalTotalBytes; /**&lt; total number of bytes in the immortal memory area */
	uintptr_t rememberedSetCount; /**&lt; number of elements in the rememberedSet */
} MM_CommonGCData;

/**
 * Defines data which is common to all types of GC start events.
 * This generally describes the state of the heap during the last
 * GC cycle.
 *
 * (In general, more events in this file should be changed to use
 * structs like this to avoid the large amount of duplicated code
 * we currently have)
 */
typedef struct MM_CommonGCStartData {
	MM_CommonGCData commonData; /**&lt; common data to GC start and GC end */

	uint64_t exclusiveAccessTime; /**&lt; time taken to get exclusive access (in ticks) */
	uint64_t meanExclusiveAccessIdleTime; /**&lt; the average time each thread (including the requester) spent idle until the last thread checked in (in ticks) */
	uintptr_t haltedThreads; /**&lt; the number of threads which had to be halted to acquire exclusive access */
	struct OMR_VMThread* lastResponder; /**&lt; the last thread to respond the the exclusive request */
	uintptr_t beatenByOtherThread; /**&lt; flag to indicate if another thread beat us to exclusive access */

	uintptr_t tlhAllocCount; /**&lt; number of TLHs allocated since the last GC */
	uintptr_t tlhAllocBytes; /**&lt; number of bytes allocated for TLHs since the last GC */
	uintptr_t tlhRequestedBytes; /**&lt; number of bytes requested for TLHs since the last GC */
	uintptr_t nonTlhAllocCount; /**&lt; number of non-TLH allocates since the last GC */
	uintptr_t nonTlhAllocBytes; /**&lt; number of bytes allocated for other than for TLHs since the last GC */
} MM_CommonGCStartData;

/**
 * Defines data which is common to all types of GC end events.
 * This generally describes the state of the heap during the last
 * GC cycle.
 */
typedef struct MM_CommonGCEndData {
	MM_CommonGCData commonData; /**&lt; common data to GC start and GC end */
} MM_CommonGCEndData;



#endif /* MMHOOK_H_ */
