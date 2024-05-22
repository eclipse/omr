/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/**
 * @file
 * @ingroup Port
 * @brief Timer utilities
 */

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatability */
#if defined(LINUX)
#define _GNU_SOURCE
#endif /* defined(LINUX) */

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>

#if !__GLIBC_PREREQ(2,4)
/* Compilation on RHEL 4 (for eventual 7.0 VM to Java 6.0 backport) */
#include <asm/systemcfg.h>
extern volatile struct systemcfg *systemcfgP_millis;
extern volatile struct systemcfg *systemcfgP_nanos;
#else
#include <sys/mman.h>
#include <fcntl.h>
extern volatile struct vdso_data *systemcfgP_millis;
extern volatile struct vdso_data *systemcfgP_nanos;
#endif

#include "omrport.h"

extern int64_t __getMillis(void);
extern int64_t __getNanos(void);

/* Frequency is nanoseconds / second */
#define OMRTIME_HIRES_CLOCK_FREQUENCY J9CONST_U64(1000000000)

static const clockid_t OMRTIME_NANO_CLOCK = CLOCK_MONOTONIC;

/*
 * We don't need anything in this structure, because assembler code has the required fields hard coded
 * This structure has to be large enough to include the fields used by the assembler routines
 * Creating the structure the same size as sizeof(vdso_data) when using asm/vdso_datapage.h
 */
struct vdso_data {
	uint64_t unused_1[12];
};

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
	struct timespec ts;
	uint64_t nsec = 0;
	*success = 0;
	if (0 == clock_gettime(CLOCK_REALTIME, &ts)) {
		nsec = ((uint64_t)ts.tv_sec * OMRPORT_TIME_DELTA_IN_NANOSECONDS) + (uint64_t)ts.tv_nsec;
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
	struct timeval tp;

	if (systemcfgP_millis) {
		return __getMillis();
	}

	gettimeofday(&tp, NULL);
	return ((int64_t)tp.tv_sec) * 1000 + tp.tv_usec / 1000;
}

int64_t
omrtime_nano_time(struct OMRPortLibrary *portLibrary)
{
	struct timespec ts;
	int64_t hiresTime = 0;

	if (0 == clock_gettime(OMRTIME_NANO_CLOCK, &ts)) {
		hiresTime = ((int64_t)ts.tv_sec * OMRPORT_TIME_DELTA_IN_NANOSECONDS) + (int64_t)ts.tv_nsec;
	}

	return hiresTime;
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
	uint64_t ret = 0;
	if (systemcfgP_nanos) {
		ret = __getNanos();
	} else {
		struct timespec ts;
		if (0 == clock_gettime(OMRTIME_NANO_CLOCK, &ts)) {
			ret = ((uint64_t)ts.tv_sec * OMRPORT_TIME_DELTA_IN_NANOSECONDS) + (uint64_t)ts.tv_nsec;
		}
	}
	return ret;
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

#if !__GLIBC_PREREQ(2,4)
	systemcfgP_millis = systemcfg_init();
#else
	/* Avoid mapping systemcfg if a checkpoint is possible because we may be prevented from
	 * restoring it in some environments by a security module (e.g. SELinux). If we're currently
	 * running in an environment that doesn't prevent us from mapping systemcfg, and the
	 * process is later checkpointed, it can only be restored in an environment that allows
	 * systemcfg to be mapped. In order to improve portability we'll only map systemcfg if
	 * no further checkpoints are possible.
	 */
#if defined(PPG_criuSupportFlags)
	if (OMR_ARE_NO_BITS_SET(PPG_criuSupportFlags, OMRPORT_CRIU_SUPPORT_ENABLED)
		|| OMR_ARE_ANY_BITS_SET(PPG_criuSupportFlags, OMRPORT_CRIU_SUPPORT_FINAL_RESTORE)
	)
#endif /* defined(PPG_criuSupportFlags) */
	{
		int procfd;

		procfd = open("/proc/ppc64/systemcfg", O_RDONLY);
		if (-1 != procfd) {
			systemcfgP_millis = mmap(0, sizeof(struct vdso_data), PROT_READ, MAP_SHARED, procfd, 0);
			close(procfd);
			if (MAP_FAILED == systemcfgP_millis) {
				systemcfgP_millis = NULL;
			}
		}
	}
#endif

	/* cmvc 89134 - on some linux kernel versions the struct exists but the time is not filled in, null the pointer to cause the slower fallback code to be used */
	if (systemcfgP_millis && omrtime_current_time_millis(portLibrary) == 0) {
		systemcfgP_millis = NULL;
	}

	/*
	 * The Linux Kernels used in SLES11 and RHEL6 (both 2.6.32) have a bug that breaks our
	 * implementation of __getNanos(). The issue is that the tb_to_xs field in the vdso struct
	 * can change at runtime, which causes __getNanos() to sometimes report inaccurate time -
	 * including time that can go backwards. SLES10 and RHEL5 do not have this issue.
	 *
	 * We work around this by falling back to the gettimeofday() implementation, which was
	 * measured to be around 3x slower compared to __getNanos(), on newer Kernel versions.
	 * Only use __getNanos() if we're on a 2.6 Kernel <= 2.6.18.
	 *
	 * Note: RHEL5 is 2.6.18 and SLES10 is 2.6.16.
	 *
	 * See CMVC 175622 for more details.
	 */
	systemcfgP_nanos = NULL;
	if (NULL != systemcfgP_millis) {
		struct OMROSKernelInfo kernelInfo = {0};

		if (portLibrary->sysinfo_os_kernel_info(portLibrary, &kernelInfo)) {
			if ((2 == kernelInfo.kernelVersion) && (6 == kernelInfo.majorRevision) && (18 >= kernelInfo.minorRevision)) {
				systemcfgP_nanos = systemcfgP_millis;
			}
		}
	}

	/* check if the clock is available */
	if (0 != clock_getres(OMRTIME_NANO_CLOCK, &ts)) {
		rc = OMRPORT_ERROR_STARTUP_TIME;
	}

	return rc;
}
