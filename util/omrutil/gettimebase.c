/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
 ******************************************************************************/

#include <stdlib.h>

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrutilbase.h"

#if defined(LINUX) && defined(OMR_ARCH_ARM)
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

#elif defined(LINUX) && defined(S390)
	asm("stck %0" : "=m" (tsc));
#elif defined(J9ZOS390)
	__stck(&tsc);
#elif defined(LINUX) && defined(OMR_ARCH_ARM)
	/* For now, use the system nano clock */
#define J9TIME_NANOSECONDS_PER_SECOND	J9CONST_U64(1000000000)
	struct timespec ts;
	if (0 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
		tsc = ((uint64_t)ts.tv_sec * J9TIME_NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec;
	}
#else
#error "Unsupported platform"
#endif

	return tsc;
}
