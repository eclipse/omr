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
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrport.h"

#define OMRTIME_CLOCK_DELTA_ADJUSTMENT_INTERVAL_USEC J9CONST_I64(60 * 1000 * 1000)

#define OMRTIME_NANOSECONDS_PER_SECOND J9CONST_I64(1000000000)
static const clockid_t OMRTIME_NANO_CLOCK = CLOCK_MONOTONIC;

extern int64_t maxprec();

static void omrtime_calculate_hw_time_delta(struct OMRPortLibrary *portLibrary);

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
uintptr_t
omrtime_msec_clock(struct OMRPortLibrary *portLibrary)
{
	int64_t msec;
	int64_t usec = maxprec() / OMRPORT_TIME_HIRES_MICROTIME_DIVISOR;

	msec = usec / 1000;

	if ((usec - PPG_last_clock_delta_update > OMRTIME_CLOCK_DELTA_ADJUSTMENT_INTERVAL_USEC)
	 || (usec < PPG_last_clock_delta_update)
	) {
		omrtime_calculate_hw_time_delta(portLibrary);
	}

	msec += PPG_software_msec_clock_delta;

	return msec;
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
	int64_t usec = maxprec() / OMRPORT_TIME_HIRES_MICROTIME_DIVISOR;

	if ((usec - PPG_last_clock_delta_update > OMRTIME_CLOCK_DELTA_ADJUSTMENT_INTERVAL_USEC)
	 || (usec < PPG_last_clock_delta_update)
	) {
		omrtime_calculate_hw_time_delta(portLibrary);
	}

	usec += (PPG_software_msec_clock_delta * 1000);

	return usec;
}

uint64_t
omrtime_current_time_nanos(struct OMRPortLibrary *portLibrary, uintptr_t *success)
{
	struct timespec ts;
	uint64_t nsec = 0;
	*success = 0;
	if (0 == clock_gettime(CLOCK_REALTIME, &ts)) {
		nsec = ((uint64_t)ts.tv_sec * OMRTIME_NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec;
		*success = 1;
	}
	return nsec;
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
	int64_t msec;
	int64_t usec = maxprec() / OMRPORT_TIME_HIRES_MICROTIME_DIVISOR;

	/*
	 * Note that this function is called by JIT inlined code to not only
	 * retrieve the time but also to perform the periodic delta update.
	 * This is done in order to avoid a situation where all relevant code
	 * becomes jitted and we are left with no interpreted callsites into this
	 * function.
	 *
	 * The s390 provides an instruction to retrieve a hardware clock, unfortunately
	 * that particular clock does not necessarily match the user adjusted OS clock.
	 * This leads to a discrepency between the time the user has set and wants to see
	 * and what we provide by retrieving the hw clock that the user is not allowed to
	 * change.  This code will apply a time delta to the retrieved hardware clock
	 * effectively synchronizing it. Some precision is lost due to the problems
	 * inherent in calculating the delta itself.
	 *
	 * In order to account for the possibility of the OS clock being changed after
	 * the initial delta is calculated we periodically recompute it. This has the
	 * effect of keeping a good performance profile while still being moderately
	 * correct with respect to the time expected by the user.
	 *
	 * See Design 1183 for more details.
	 */

	if ((usec - PPG_last_clock_delta_update > OMRTIME_CLOCK_DELTA_ADJUSTMENT_INTERVAL_USEC)
	 || (usec < PPG_last_clock_delta_update)
	) {
		omrtime_calculate_hw_time_delta(portLibrary);
	}

	msec = (usec / 1000) + PPG_software_msec_clock_delta;

	return msec;
}

int64_t
omrtime_nano_time(struct OMRPortLibrary *portLibrary)
{
	const int64_t subnanosec = maxprec();

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
	int64_t hires = maxprec();
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
 * \brief Calculate the time delta between the zLinux hardware clock and OS clock
 *
 * @param[in] portLibrary The port library.
 *
 *	Calculate the time difference between the STCK retrieved hw clock and the
 *	OS gettimeofday() retrieved clock. The time delta is applied to all
 *	subsequent msec, usec and current_time_milli calls.
 */
static void
omrtime_calculate_hw_time_delta(struct OMRPortLibrary *portLibrary)
{
	int64_t currentHWTime, currentSWTime;
	struct timeval tp;

	/* Get hardware microsecond UTC clock */
	currentHWTime = maxprec() / OMRPORT_TIME_HIRES_MICROTIME_DIVISOR;

	/* Get OS microsecond UTC clock */
	gettimeofday(&tp, NULL);
	currentSWTime = ((uint64_t) tp.tv_sec * 1000000) + tp.tv_usec;

	/* Calculate the millisecond time delta between the software and hardware clocks */
	PPG_software_msec_clock_delta = (currentSWTime - currentHWTime) / 1000;

	/* Save the last time we've adjusted the time delta */
	PPG_last_clock_delta_update = currentHWTime;
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
	struct timespec ts;

	omrtime_calculate_hw_time_delta(portLibrary);

	/* check if the clock is available */
	if (0 != clock_getres(OMRTIME_NANO_CLOCK, &ts)) {
		rc = OMRPORT_ERROR_STARTUP_TIME;
	}

	return rc;
}

