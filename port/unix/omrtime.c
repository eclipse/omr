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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Timer utilities
 */

#if defined(OSX)
#include <mach/clock.h>
#include <mach/mach.h>
#endif /* defined(OSX) */
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include "omrport.h"

/* Frequency is microseconds / second */
#define OMRTIME_HIRES_CLOCK_FREQUENCY J9CONST_U64(1000000)

#define OMRTIME_NANOSECONDS_PER_SECOND J9CONST_I64(1000000000)

#if defined(OSX)
static clock_serv_t cs_t;
#else /* defined(OSX) */
static const clockid_t OMRTIME_NANO_CLOCK = CLOCK_MONOTONIC;
#endif /* defined(OSX) */


/**
 * Query OS for timestamp.
 * Retrieve the current value of system clock and convert to milliseconds.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on failure, time value in milliseconds on success.
 * @deprecated Use @ref omrtime_hires_clock and @ref omrtime_hires_delta
 */
/*  technically, this should return int64_t since both timeval.tv_sec and timeval.tv_usec are long */

uintptr_t
omrtime_msec_clock(struct OMRPortLibrary *portLibrary)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}
/**
 * Query OS for timestamp.
 * Retrieve the current value of system clock and convert to microseconds.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on failure, time value in microseconds on success.
 * @deprecated Use @ref omrtime_hires_clock and @ref omrtime_hires_delta
 */
uintptr_t
omrtime_usec_clock(struct OMRPortLibrary *portLibrary)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return (tp.tv_sec * 1000000) + tp.tv_usec;
}

uint64_t
omrtime_current_time_nanos(struct OMRPortLibrary *portLibrary, uintptr_t *success)
{
	uint64_t nsec = 0;
#if defined(OSX)
	struct timeval ts;
	*success = 0;
	if (0 == gettimeofday(&ts, NULL)) {
		nsec = ((uint64_t)ts.tv_sec * OMRTIME_NANOSECONDS_PER_SECOND) + (uint64_t)(ts.tv_usec * 1000);
		*success = 1;
	}
#else /* defined(OSX) */
	struct timespec ts;
	*success = 0;
	if (0 == clock_gettime(CLOCK_REALTIME, &ts)) {
		nsec = ((uint64_t)ts.tv_sec * OMRTIME_NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec;
		*success = 1;
	}
#endif /* defined(OSX) */
	return nsec;
}

int64_t
omrtime_nano_time(struct OMRPortLibrary *portLibrary)
{
	int64_t hiresTime = 0;
#if defined(OSX)
	mach_timespec_t mt;
	if (KERN_SUCCESS == clock_get_time(cs_t, &mt)) {
		hiresTime = ((int64_t)mt.tv_sec * OMRTIME_NANOSECONDS_PER_SECOND) + (int64_t)mt.tv_nsec;
	}
#else /* defined(OSX) */
	struct timespec ts;

	if (0 == clock_gettime(OMRTIME_NANO_CLOCK, &ts)) {
		hiresTime = ((int64_t)ts.tv_sec * OMRTIME_NANOSECONDS_PER_SECOND) + (int64_t)ts.tv_nsec;
	}
#endif /* defined(OSX) */

	return hiresTime;
}

/**
 * Query OS for timestamp.
 * Retrieve the current value of system clock and convert to milliseconds since
 * January 1st 1970 UTC.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on failure, time value in milliseconds on success.
 */
int64_t
omrtime_current_time_millis(struct OMRPortLibrary *portLibrary)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return ((int64_t)tp.tv_sec) * 1000 + tp.tv_usec / 1000;
}
/**
 * Query OS for timestamp.
 * Retrieve the current value of the high-resolution performance counter.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on failure, time value on success.
 */
uint64_t
omrtime_hires_clock(struct OMRPortLibrary *portLibrary)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return ((uint64_t)tp.tv_sec * 1000000) + (uint64_t)tp.tv_usec;
}
/**
 * Query OS for clock frequency
 * Retrieves the frequency of the high-resolution performance counter.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on failure, number of ticks per second on success.
 */
uint64_t
omrtime_hires_frequency(struct OMRPortLibrary *portLibrary)
{
	return OMRTIME_HIRES_CLOCK_FREQUENCY;
}
/**
 * Calculate time difference between two hires clock timer values @ref omrtime_hires_clock.
 *
 * Given a start and end time determine how much time elapsed.  Return the value as
 * requested by the required resolution
 *
 * @param[in] portLibrary The port library.
 * @param[in] startTime Timer value at start of timing interval
 * @param[in] endTime Timer value at end of timing interval
 * @param[in] requiredResolution Returned timer resolution as a fraction of a second.  For example:
 *  \arg 1 to report elapsed time in seconds
 *  \arg 1,000 to report elapsed time in milliseconds
 *  \arg 1,000,000 to report elapsed time in microseconds
 *
 * @return 0 on failure, time difference on success.
 *
 * @note helper macros are available for commonly requested resolution
 *  \arg OMRPORT_TIME_DELTA_IN_SECONDS return timer value in seconds.
 *  \arg OMRPORT_TIME_DELTA_IN_MILLISECONDS return timer value in milliseconds.
 *  \arg OMRPORT_TIME_DELTA_IN_MICROSECONDS return timer value in micoseconds.
 *  \arg OMRPORT_TIME_DELTA_IN_NANOSECONDS return timer value in nanoseconds.
 */
uint64_t
omrtime_hires_delta(struct OMRPortLibrary *portLibrary, uint64_t startTime, uint64_t endTime, uint64_t requiredResolution)
{
	uint64_t ticks;

	/* modular arithmetic saves us, answer is always ...*/
	ticks = endTime - startTime;

	if (OMRTIME_HIRES_CLOCK_FREQUENCY == requiredResolution) {
		/* no conversion necessary */
	} else if (OMRTIME_HIRES_CLOCK_FREQUENCY < requiredResolution) {
		ticks = (uint64_t)((double)ticks * ((double)requiredResolution / (double)OMRTIME_HIRES_CLOCK_FREQUENCY));
	} else {
		ticks = (uint64_t)((double)ticks / ((double)OMRTIME_HIRES_CLOCK_FREQUENCY / (double)requiredResolution));
	}
	return ticks;
}
/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrtime_startup
 * should be destroyed here.
 *
 * @param[in] portLib The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrtime_shutdown(struct OMRPortLibrary *portLibrary)
{
#if defined(OSX)
	mach_port_deallocate(mach_task_self(), cs_t);
#endif /* defined(OSX) */
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the time operations may be created here.  All resources created here should be destroyed
 * in @ref omrtime_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_TIME
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrtime_startup(struct OMRPortLibrary *portLibrary)
{
	int32_t rc = 0;
#if defined(OSX)
	if (KERN_SUCCESS != host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cs_t)) {
		rc = OMRPORT_ERROR_STARTUP_TIME;
	}
#else /* defined(OSX) */
	struct timespec ts;

	/* check if the clock is available */
	if (0 != clock_getres(OMRTIME_NANO_CLOCK, &ts)) {
		rc = OMRPORT_ERROR_STARTUP_TIME;
	}
#endif /* defined(OSX) */

	return rc;
}


