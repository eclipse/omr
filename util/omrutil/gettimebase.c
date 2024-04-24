/*******************************************************************************
 * Copyright IBM Corp. and others 2014
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdlib.h>

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrutilbase.h"

#if defined(LINUX) && (defined(OMR_ARCH_ARM) || defined(OMR_ARCH_RISCV) || defined(RISCV64))
#include <time.h>
#endif

uint64_t
getTimebase(void)
{
	uint64_t tsc = 0;

#if defined(_MSC_VER)
	tsc = __rdtsc();
#elif defined(J9X86) || defined(J9HAMMER)
	/* Cannot use "=A", as we want to read eax:edx not just rax on x86_64 */
	uint32_t lower;
	uint32_t upper;
	asm volatile("rdtsc" : "=a" (lower), "=d" (upper));
	tsc = ((uint64_t)upper << 32) | lower;
#elif defined(AIXPPC) || defined(LINUXPPC)

#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
	tsc = __builtin_ppc_get_timebase();
#else /* GCC < 4.8 or XLC */
#if defined(OMR_ENV_DATA64)
#if defined(__xlC__)
	/* PPC64 & XLC */
	tsc = __mftb();
#else /* !XLC */
	/* PPC64 & !XLC */
	asm volatile("mftb %0" : "=r" (tsc));
#endif /* __xlC__ */

#else /* !OMR_ENV_DATA64 */
	/* PPC32 */
	uint32_t upper;
	uint32_t lower;
	uint32_t upper2;
	do {
#if defined(__xlC__)
		upper = __mftbu(); /* upper word of the time base register */
		lower = __mftb(); /* lower word of the time base register */
		upper2 = __mftbu();
#else /* !XLC */
		asm volatile("mftbu %0" : "=r" (upper));
		asm volatile("mftb %0" : "=r" (lower));
		asm volatile("mftbu %0" : "=r" (upper2));
#endif /* __xlC__ */
	} while (upper != upper2);
	tsc = ((uint64_t)upper << 32) | lower;
#endif /* OMR_ENV_DATA64 */
#endif /* GCC 4.8 */

#elif (defined(LINUX) && defined(S390)) || (defined(__open_xl__) && defined(J9ZOS390))
	asm(" stck %0" : "=m" (tsc));
#elif defined(J9ZOS390)
	__stck((unsigned long long*)&tsc);
#elif defined(LINUX) && (defined(OMR_ARCH_ARM) || defined(OMR_ARCH_RISCV) || defined(RISCV64))
	/* For now, use the system nano clock */
#define J9TIME_NANOSECONDS_PER_SECOND	J9CONST_U64(1000000000)
	struct timespec ts;
	if (0 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
		tsc = ((uint64_t)ts.tv_sec * J9TIME_NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec;
	}
#elif defined(AARCH64)
	int64_t tsc_int;
	asm volatile("mrs %0, cntvct_el0" : "=r"(tsc_int));
	tsc = (uint64_t)tsc_int;
#else
#error "Unsupported platform"
#endif

	return tsc;
}
