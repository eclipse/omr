/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcfg.h"
#include "omr.h"
#include "omrutil.h"

#include <string.h>

#if (defined(LINUX) && defined(S390))
/* _j9Z10Zero() is defined in j9memclrz10_31.s and j9memclrz10_64.s */
extern void _j9Z10Zero(void *ptr, uintptr_t length);
#endif

#if defined(AIXPPC) || defined (LINUXPPC)
static uintptr_t cacheLineSize = 0;
#endif

#if defined (J9ZOS390)
struct IHAPSA {
	char filler1[204];			/* Filler to get to offset 204 or X'CC'  */
	unsigned int filler2 : 1;
	unsigned int filler3 : 1;
	unsigned int FLCFGIEF : 1;	/* General-Instructions-Extension Facility offset X'CC' */
};
#endif

#if defined (S390)
static int isZ10orGreater = -1;
#endif

void
OMRZeroMemory(void *ptr, uintptr_t length)
{
#if defined(AIXPPC) || defined (LINUXPPC)
	char *addr = ptr;
	char *limit;
	uintptr_t localCacheLineSize;

#if defined(LINUXPPC)
	if (length < 2048) {
		memset(ptr, 0, (size_t)length);
		return;
	}
#endif /* defined(LINUXPPC) */

	/* one-time-only calculation of cache line size */
	if (0 == cacheLineSize) {
		cacheLineSize = getCacheLineSize();
	}

	/* Zeroing by dcbz is effective if requested length is at least twice larger then Data Cache Block size */
	if (length < (2 * cacheLineSize)) {
		memset(ptr, 0, (size_t)length);
		return;
	}

	/* VMDESIGN 1314 - Allow the compile to unroll the loop below by avoiding using the global in the loop */

	localCacheLineSize = cacheLineSize;

	/* Zero any initial portion to first cache line boundary
	 * Assumed here that size of first portion (from start to aligned address) is smaller then total requested size
	 * This is correct because sizes smaller then 2 * Data Cache Block size are served already
	 */
	if ((uintptr_t)addr & (localCacheLineSize - 1)) {
		limit = (char *)(((uintptr_t)addr + localCacheLineSize - 1) & ~(localCacheLineSize - 1));
		/* code assumes ptr will have uintptr_t alignment! */
		/* xlc -O3 will unroll this loop */
		for (; addr < limit; addr += sizeof(uintptr_t)) {
			*((uintptr_t *)addr) = 0;
		}
	}

	/* dcbz full cache lines */
	/* dcbz forms a group on POWER4, so there is no reason to unroll */
	limit = (char *)(((uintptr_t)ptr + length) & ~(localCacheLineSize - 1));
	for (; addr < limit; addr += localCacheLineSize) {
		__asm__ __volatile__("dcbz 0,%0" : /* no outputs */ : "r"(addr));
	}

	/* zero final portion smaller than a cache line */
	limit = (char *)((uintptr_t)ptr + length);
	/* code assumes length will be a multiple of sizeof(uintptr_t)! */
	/* xlc -O3 will unroll this loop */
	for (; addr < limit; addr += sizeof(uintptr_t)) {
		*((uintptr_t *)addr) = 0;
	}

#elif defined(OMR_OS_WINDOWS) && !defined(OMR_ENV_DATA64)
	/* NOTE: memset on WIN64 (AMD64) seems to do a fine job all on its own,
	 * so we don't use our own SSE2 code there
	 */
	memset(ptr, 0, (size_t)length);
#elif defined(J9ZOS390)

	if (((struct IHAPSA *)0)->FLCFGIEF) {
		J9ZERZ10(ptr, length);
	} else {
		memset(ptr, 0, (size_t)length);
	}
#else
#if (defined (LINUX) && defined(S390)) && !defined(OMRZTPF)
	if (-1 == isZ10orGreater) {
		int machineType = get390zLinuxMachineType();
		if ((-1 == machineType) || machineType < Z10) {
			isZ10orGreater = 0;
		} else {
			isZ10orGreater = 1;
		}
	}

	if (1 == isZ10orGreater) {
		_j9Z10Zero(ptr, length);
	} else {
		memset(ptr, 0, (size_t)length);
	}
#else /* (defined (LINUX) && defined(S390)) && !defined(OMRZTPF) */
	memset(ptr, 0, (size_t)length);
#endif
#endif
}


uintptr_t
getCacheLineSize(void)
{
#if defined(AIXPPC) || defined (LINUXPPC)
	char buf[1024];
	uintptr_t i, ppcCacheLineSize;

	/* xlc -O3 inlines/unrolls this memset */
	memset(buf, 255, 1024);
	__asm__ __volatile__("dcbz 0,%0" : /* no outputs */ : "r"(&buf[512]));
	for (i = 0, ppcCacheLineSize = 0; i < 1024; i++) {
		if (buf[i] == 0) {
			ppcCacheLineSize++;
		}
	}
	return ppcCacheLineSize;
#else
	return 0;
#endif
}


void
j9memset(void *dest, intptr_t value, uintptr_t size)
{
	memset(dest, (int) value, (size_t) size);
}
