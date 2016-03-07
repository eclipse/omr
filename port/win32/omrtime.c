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

#include <windows.h>

#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"

void shutdown_timer(void);
int32_t init_timer(void);

#define OMRTIME_NANOSECONDS_PER_SECOND ((long double) 1000000000)
#define UNIX_EPOCH_TIME_DELTA J9CONST_I64(116444736000000000)
/**
 * Query OS for timestamp.
 * Retrieve the current value of system clock and convert to milliseconds.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on failure, time value in milliseconds on success.
 * @deprecated Use @ref omrtime_hires_clock and @ref omrtime_hires_delta
 */
uintptr_t
omrtime_msec_clock(struct OMRPortLibrary *portLibrary)
{
	LARGE_INTEGER freq, i, multiplier;

	/* Try to use the performance counter, but revert to GetTickCount() if a problem occurs */

	if (QueryPerformanceFrequency(&freq)) {

		multiplier.QuadPart = freq.QuadPart / 1000;
		if (0 != multiplier.QuadPart) {

			if (QueryPerformanceCounter(&i)) {
				return (uintptr_t)(i.QuadPart / multiplier.QuadPart);
			}
		}
	}

	/* GetTickCount() returns ticks in milliseconds */
	return (uintptr_t) GetTickCount();
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
	LARGE_INTEGER freq, i, multiplier;

	/* Try to use the performance counter, but revert to GetTickCount() if a problem occurs */

	if (QueryPerformanceFrequency(&freq)) {

		multiplier.QuadPart = freq.QuadPart / 1000000;
		if (0 != multiplier.QuadPart) {

			if (QueryPerformanceCounter(&i)) {
				return (uintptr_t)(i.QuadPart / multiplier.QuadPart);
			}
		}
	}

	/* GetTickCount() returns ticks in milliseconds */
	return (uintptr_t) GetTickCount() * 1000;
}

uint64_t
omrtime_current_time_nanos(struct OMRPortLibrary *portLibrary, uintptr_t *success)
{
	/* returns in time the number of 100ns elapsed since January 1, 1601 */
	/* subtract 116444736000000000 = number of 100ns from jan 1, 1601 to jan 1, 1970 */
	/* multiply by 100 to get number of nanoseconds since Jan 1, 1970 */

	LONGLONG time;
	GetSystemTimeAsFileTime((FILETIME *)&time);
	*success = 1;
	return (uint64_t)((time - UNIX_EPOCH_TIME_DELTA) * 100);
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
	/* returns in time the number of 100ns elapsed since January 1, 1601 */
	/* subtract 116444736000000000 = number of 100ns from jan 1, 1601 to jan 1, 1970 */
	/* divide by 10000 to get number of milliseconds since Jan 1, 1970 */

	LONGLONG time;
	GetSystemTimeAsFileTime((FILETIME *)&time);
	return (int64_t)((time - UNIX_EPOCH_TIME_DELTA) / 10000);
}
int32_t
init_timer(void)
{
	/* on Win98 this hack forced the process to run with a higher
	 * resolution clock. It made things like Thread.sleep() more
	 * accurate. But the functions it calls are defined in WINMM.DLL,
	 * which forces USER32.DLL, GDI.DLL and other modules to
	 * be loaded, polluting the address space. By not loading WINMM.DLL
	 * we increase the chances of having a large contiguous region
	 * of virtual memory to use as the Java heap.
	 */
#if 0

	TIMECAPS timecaps;

	timeGetDevCaps(&timecaps, sizeof(TIMECAPS));
	timeBeginPeriod(timecaps.wPeriodMin);

#endif

	return 0;
}


void
shutdown_timer(void)
{

	/* see init_timer */
#if 0

	TIMECAPS timecaps;

	timeGetDevCaps(&timecaps, sizeof(TIMECAPS));
	timeEndPeriod(timecaps.wPeriodMin);

#endif
}

int64_t
omrtime_nano_time(struct OMRPortLibrary *portLibrary)
{
	LARGE_INTEGER performanceCounterTicks;
	int64_t ticks = 0;
	int64_t nanos = 0;

	if (0 != QueryPerformanceCounter(&performanceCounterTicks)) {
		ticks = (int64_t)performanceCounterTicks.QuadPart;
	} else {
		ticks = (int64_t)GetTickCount();
	}

	if (PPG_time_hiresClockFrequency == OMRTIME_NANOSECONDS_PER_SECOND) {
		nanos = ticks;
	} else if (PPG_time_hiresClockFrequency < OMRTIME_NANOSECONDS_PER_SECOND) {
		nanos = (int64_t)(ticks * (OMRTIME_NANOSECONDS_PER_SECOND / PPG_time_hiresClockFrequency));
	} else {
		nanos = (int64_t)(ticks / (PPG_time_hiresClockFrequency / OMRTIME_NANOSECONDS_PER_SECOND));
	}

	return nanos;
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
	LARGE_INTEGER i;

	if (QueryPerformanceCounter(&i)) {
		return (uint64_t)i.QuadPart;
	} else {
		return (uint64_t)GetTickCount();
	}
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
	return PPG_time_hiresClockFrequency;
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
	uint64_t frequency = PPG_time_hiresClockFrequency;

	/* modular arithmetic saves us, answer is always ...*/
	ticks = endTime - startTime;

	if (frequency == requiredResolution) {
		/* no conversion necessary */
	} else if (frequency < requiredResolution) {
		ticks = (uint64_t)((double)ticks * ((double)requiredResolution / (double)frequency));
	} else {
		ticks = (uint64_t)((double)ticks / ((double)frequency / (double)requiredResolution));
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
	shutdown_timer();
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
	LARGE_INTEGER freq;

	if (QueryPerformanceFrequency(&freq)) {
		PPG_time_hiresClockFrequency =  freq.QuadPart;
	} else {
		PPG_time_hiresClockFrequency = 1000; /* GetTickCount() returns millis */
	}

	return init_timer();
}

