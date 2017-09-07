/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Timer utilities
 */


#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"

extern int64_t MAXPREC();

/**
 * Multiply a number by a fraction
 *
 * @param[in] num The number to be multiplied by the fraction
 * @param[in] numerator The numerator of the fraction
 * @param[in] denominator The denominator of the fraction
 *
 * @return The result of the multiplication
 */
static int64_t
muldiv64(const int64_t num, const int64_t numerator, const int64_t denominator)
{
	const int64_t quotient = num / denominator;
	const int64_t remainder = num - quotient * denominator;
	const int64_t res = quotient * numerator + ((remainder * numerator) / denominator);
	return res;
}

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
	int64_t millisec = MAXPREC() / OMRPORT_TIME_HIRES_MILLITIME_DIVISOR;
	return millisec;
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
	int64_t microsec = MAXPREC() / OMRPORT_TIME_HIRES_MICROTIME_DIVISOR;
	return microsec;
}

uint64_t
omrtime_current_time_nanos(struct OMRPortLibrary *portLibrary, uintptr_t *success)
{
	*success = 1;

	const int64_t subnanosec = MAXPREC();

	// calculate nanosec = subnanosec * NUMERATOR / DENOMINATOR via integer arithmetics
	const int64_t nanosec = muldiv64(subnanosec,
			OMRPORT_TIME_HIRES_NANOTIME_NUMERATOR, OMRPORT_TIME_HIRES_NANOTIME_DENOMINATOR);

	return nanosec;
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
	int64_t millisec = MAXPREC() / OMRPORT_TIME_HIRES_MILLITIME_DIVISOR;
	return millisec;
}

int64_t
omrtime_nano_time(struct OMRPortLibrary *portLibrary)
{
	const int64_t subnanosec = MAXPREC();

	// calculate nanosec = subnanosec * NUMERATOR / DENOMINATOR via integer arithmetics
	const int64_t nanosec = muldiv64(subnanosec,
			OMRPORT_TIME_HIRES_NANOTIME_NUMERATOR, OMRPORT_TIME_HIRES_NANOTIME_DENOMINATOR);

	return nanosec;
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
	int64_t hires = MAXPREC();
	return hires;
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
		ticks = muldiv64(ticks, requiredResolution, OMRTIME_HIRES_CLOCK_FREQUENCY);
	} else {
		/*equivalent to ticks / (OMRTIME_HIRES_CLOCK_FREQUENCY / requiredResolution)*/
		ticks = muldiv64(ticks, requiredResolution, OMRTIME_HIRES_CLOCK_FREQUENCY);
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
}

int32_t
omrtime_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}


