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
 * @ingroup Thread
 * @brief Thread profiling support.
 *
 * APIs for querying per-thread statistics: CPU usage, stack usage.
 */

#include <string.h> /* for memset() */
#include "omrcfg.h"

/*
 *	trent: the following must come before the standard includes because thrdsup.h
 *	includes windows.h in Win32.
 */
#include "thrdsup.h"

#include "omrthread.h"
#include "thrtypes.h"
#include "threaddef.h" /* for ASSERT() */
#include "thread_internal.h"
#include "ut_j9thr.h"

/* for syscall getrusage() used in omrthread_get_process_times */
#if defined(LINUX) || defined (J9ZOS390) || defined(AIXPPC) || defined(OSX)
#include <errno.h> /* Examine errno codes. */
#include <sys/time.h> /* Portability */
#include <sys/resource.h>
#endif /* defined(LINUX) || defined (J9ZOS390) || defined(AIXPPC) || defined(OSX) */

#if defined(J9ZOS390)
#include "omrgetthent.h"

#define I32MAXVAL	0x7FFFFFFF
#endif

#if defined(LINUX)
/* pthread_getcpuclockid() is not always declared in pthread.h */
extern int pthread_getcpuclockid(pthread_t thread_id, clockid_t *clock_id);
#endif /* defined(LINUX) */

#define STACK_PATTERN 0xBAADF00D


/**
 * Return the amount of CPU time used by an arbitrary thread, in nanoseconds.
 * If arbitrary thread times are not supported, but current thread times are
 * supported, this function will still return -1 even if the current thread is
 * passed in as an argument.
 * @param[in] thread
 * @return time in nanoseconds
 * @retval -1 if not supported on this platform or some other platform specific error occurred.
 *         See traces for the actual error code.
 * @see omrthread_get_self_cpu_time, omrthread_get_user_time, omrthread_get_cpu_time_ex
 */
int64_t
omrthread_get_cpu_time(omrthread_t thread)
{
	int64_t cpuTime = 0;
	intptr_t result = omrthread_get_cpu_time_ex(thread, &cpuTime);
	if (J9THREAD_SUCCESS != result) {
		return -1;
	}
	return cpuTime;
}


/**
 * Return the amount of CPU time used by an arbitrary thread, in nanoseconds.
 * Returns extended error code information.
 * If arbitrary thread times are not supported, but current thread times are
 * supported, this function will still return J9THREAD_ERR even if the current thread is
 * passed in as an argument.
 * @param[in] thread
 * @param[out] cpuTime	Amount of CPU time used in nanoseconds of thread on success
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_NO_SUCH_THREAD if the thread handle is invalid, or J9THREAD_ERR if not supported
 *		   on this platform or some other platform specific error occurred.
 * @retval J9THREAD_ERR_OS_ERRNO_SET Bit flag optionally or'd into the return code. Indicates that an os_errno is available.
 *         See traces for the actual error code.
 * @see omrthread_get_self_cpu_time, omrthread_get_user_time, omrthread_get_cpu_time
 */
intptr_t
omrthread_get_cpu_time_ex(omrthread_t thread, int64_t *cpuTime)
{
	/* If there is no OS thread attached to this as yet, return error */
	/* This assumes that a valid thread handle can never be 0 */
	if (0 == omrthread_get_handle(thread)) {
		Trc_THR_omrthread_get_cpu_time_ex_nullOSHandle(thread);
		return J9THREAD_ERR_NO_SUCH_THREAD;
	}

#if defined(OMR_OS_WINDOWS) && !defined(BREW)
	{
		intptr_t ret = 0;
		FILETIME creationTime, exitTime, kernelTime, userTime;
		int64_t totalTime;

		/* WARNING! Not supported on Win95!  Need to test to ensure this fails gracefully */

		if (GetThreadTimes(thread->handle, &creationTime, &exitTime, &kernelTime, &userTime)) {
			totalTime = ((int64_t)kernelTime.dwLowDateTime | ((int64_t)kernelTime.dwHighDateTime << 32))
						+ ((int64_t)userTime.dwLowDateTime | ((int64_t)userTime.dwHighDateTime << 32));

			/* totalTime is in 100's of nanos.  Convert to nanos */
			*cpuTime = totalTime * 100;
			return J9THREAD_SUCCESS;
		} else {
			DWORD result = GetLastError();
			Trc_THR_omrthread_get_cpu_time_ex_GetThreadTimes_failed(result, thread);
			thread->os_errno = (omrthread_os_errno_t) result;
			ret = J9THREAD_ERR_OS_ERRNO_SET;
			if (ERROR_INVALID_HANDLE == result) {
				/* Such a thread was not found. */
				ret |= J9THREAD_ERR_NO_SUCH_THREAD;
				return ret;
			}
			/* For any other (internal) errors other than a dead thread, return J9THREAD_ERR. */
		}

		ret |= J9THREAD_ERR;
		return ret;
	}
#endif	/* defined(OMR_OS_WINDOWS) && !defined(BREW) */

#ifdef AIXPPC
	{
		intptr_t ret = 0;
		int result;
		struct rusage usageInfo;
		memset(&usageInfo, 0, sizeof(usageInfo));
		result = pthread_getrusage_np(thread->handle, &usageInfo, PTHRDSINFO_RUSAGE_COLLECT);
		if (0 == result) {
			int64_t totalTime = ((int64_t)usageInfo.ru_stime.tv_sec + usageInfo.ru_utime.tv_sec) * 1000 * 1000; /* microseconds */
			totalTime += (int64_t)usageInfo.ru_stime.tv_usec + usageInfo.ru_utime.tv_usec;
			*cpuTime = totalTime * 1000; 	/* convert to nanoseconds */
			return J9THREAD_SUCCESS;
		} else {
			/* Handle error conditions. */
			Trc_THR_omrthread_get_cpu_time_ex_pthread_getrusage_np_failed((int32_t)result, thread);
			thread->os_errno = (omrthread_os_errno_t) result;
			ret = J9THREAD_ERR_OS_ERRNO_SET;
			if (ESRCH == result) {
				/* Such a thread was not found. */
				ret |= J9THREAD_ERR_NO_SUCH_THREAD;
				return ret;
			}
			/* For internal errors other than a dead thread, return J9THREAD_ERR. */
		}
		ret |= J9THREAD_ERR;
		return ret;
	}
#endif

#if defined(LINUX)
	{
		intptr_t ret = 0;
		int result;
		clockid_t clock_id;
		struct timespec time;
		result = pthread_getcpuclockid(thread->handle, &clock_id);
		if (0 == result) {
			errno = 0;
			if (clock_gettime(clock_id, &time) == 0) {
				*cpuTime = ((int64_t)time.tv_sec * 1000 * 1000 * 1000) + time.tv_nsec;
				return J9THREAD_SUCCESS;
			} else {
				int last_errno = errno;
				Trc_THR_omrthread_get_cpu_time_ex_clock_gettime_failed((int32_t)clock_id, (intptr_t)last_errno, thread);
				thread->os_errno = (omrthread_os_errno_t) last_errno;
				ret = J9THREAD_ERR_OS_ERRNO_SET;
			}
		} else {
			Trc_THR_omrthread_get_cpu_time_ex_pthread_getcpuclockid_failed((int32_t)result, thread);
			thread->os_errno = (omrthread_os_errno_t) result;
			ret = J9THREAD_ERR_OS_ERRNO_SET;
			/* Handle error conditions. */
			if (ESRCH == result) {
				/* Such a thread was not found. */
				ret |= J9THREAD_ERR_NO_SUCH_THREAD;
				return ret;
			}
			/* For internal errors other than a dead thread, return J9THREAD_ERR. */
		}
		ret |= J9THREAD_ERR;
		return ret;
	}
#endif /* defined(LINUX) */

#if defined(J9ZOS390)
	{
		intptr_t ret = 0;
		struct j9pg_thread_data threadData;
		unsigned char *output_buffer = (unsigned char *) &threadData;
		uint32_t output_size = sizeof(struct j9pg_thread_data);
		uint32_t input_size = sizeof(struct pgtha);
		uint32_t ret_val = 0;
		uint32_t ret_code = 0;
		uint32_t reason_code = 0;
		uint32_t data_offset = 0;
		struct pgtha pgthaInst;
		unsigned char *cursor = (unsigned char *) &pgthaInst;
		struct pgthj *pgthj = NULL;

		memset(cursor, 0, sizeof(pgthaInst));
		memset(output_buffer, 0, sizeof(threadData));
		pgthaInst.pid = getpid();
		pgthaInst.thid = thread->handle;
		/* We want data for the current PID only */
		pgthaInst.accesspid = PGTHA_ACCESS_CURRENT;
		/* We want thread data for only the specified thread */
		pgthaInst.accessthid = PGTHA_ACCESS_CURRENT;
		/* We need thread data */
		pgthaInst.flag1 = PGTHA_FLAG_PROCESS_DATA | PGTHA_FLAG_THREAD_DATA;

#if defined(_LP64)
		BPX4GTH(
			&input_size,		/* Size of pgtha structure */
			&cursor,			/* Pointer to address of pgtha */
			&output_size,		/* output size with padding */
			&output_buffer,		/* Output buffer */
			&ret_val,			/* return should be 0 on success */
			&ret_code,			/* On failure, this will have the reason */
			&reason_code);		/* more info on error */
#else
		BPX1GTH(
			&input_size,
			&cursor,
			&output_size,
			&output_buffer,
			&ret_val,
			&ret_code,
			&reason_code);
#endif

		/**
		 * Return -1 on error and trace the error
		 */
		if (-1 == ret_val) {
			/* Return a negative value on error */
			Trc_THR_ThreadGetCpuTime_Bpxgth(thread, ret_val, ret_code, reason_code);
			thread->os_errno = (omrthread_os_errno_t) ret_code;
			thread->os_errno2 = (omrthread_os_errno_t) reason_code;
			ret = J9THREAD_ERR_OS_ERRNO_SET;
			if (ESRCH == ret_code) {
				/* Such a thread was not found. */
				ret |= J9THREAD_ERR_NO_SUCH_THREAD;
			} else {
				ret |= J9THREAD_ERR;
			}
			return ret;
		}

		/* check if thread data has been populated */
		if (PGTH_OK != ((struct pgthb *)output_buffer)->limitc) {
			Trc_THR_ThreadGetCpuTime_BpxgthData(thread, ret_val, ((struct pgthb *)output_buffer)->limitc);
			return J9THREAD_ERR;
		}

		/* Get offset to the thread data in the output buffer */
		data_offset = *(unsigned int *)((struct pgthb *)output_buffer)->offj;
		data_offset = (data_offset & I32MAXVAL) >> 8;

		/* Check if the thread's data is past the end of the populated buffer */
		if (data_offset > ((struct pgthb *)output_buffer)->lenused ||
			data_offset > output_size - sizeof(struct pgthj)
		   ) {
			Trc_THR_ThreadGetCpuTime_BpxgthBuffer(thread, ret_val, data_offset, ((struct pgthb *)output_buffer)->lenused, output_size);
			return J9THREAD_ERR;
		}

		pgthj = (struct pgthj *)((char *)output_buffer + data_offset);
		/* Check if the thread data buffer is indeed pointing to thread data */
		if (__e2a_l(pgthj->id, 4) != 4 || strncmp(pgthj->id, "gthj", 4)) {
			Trc_THR_ThreadGetCpuTime_BpxgthEye(thread, ret_val);
			return J9THREAD_ERR;
		}

		/**
		 * z/OS has ttime = CPU time for the specific thread,
		 * and wtime = Wait time for the specific thread
		 * We are only interested in the ttime
		 * Both these times are in milliseconds, need to convert it to nanoseconds
		 */
		*cpuTime = (int64_t)(pgthj->ttime) * 1000 * 1000;

		return J9THREAD_SUCCESS;
	}
#endif

#if defined(OSX)
	{
		mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
		thread_basic_info_t tbi;
		thread_basic_info_data_t tbid;
		kern_return_t rc;
		mach_port_t tid = pthread_mach_thread_np(thread->handle);

		tbi = &tbid;
		rc = thread_info(tid, THREAD_BASIC_INFO, (thread_info_t)tbi, &count);
		if (rc != 0) {
			return J9THREAD_ERR;
		}
		*cpuTime = ((int64_t)tbi->user_time.seconds + tbi->system_time.seconds) * 1000 * 1000 * 1000
			+ ((int64_t)tbi->user_time.microseconds + tbi->system_time.microseconds) * 1000;

		return J9THREAD_SUCCESS;
	}
#endif /* defined(OSX) */

	return J9THREAD_ERR;
}

/**
 * Return the amount of CPU time used by the current thread, in nanoseconds.
 *
 * @param[in] self The current thread. Must be non-NULL. Pass this in to avoid the cost of pthread_getspecific().
 * @return time in nanoseconds
 * @retval -1 not supported on this platform
 * @see omrthread_get_cpu_time, omrthread_get_self_user_time
 */
int64_t
omrthread_get_self_cpu_time(omrthread_t self)
{
	ASSERT(omrthread_self() == self);

#if defined(J9ZOS390)
	{
		extern void _CPUTIME(uint64_t *); /* defined in thrcputime.s */
		uint64_t time = 0;

		/* _CPUTIME returns time in TOD format (see z/Architecture Principles of Operation) */
		_CPUTIME(&time);

		time >>= 12; /* convert to microseconds */
		time *= 1000; /* convert to nanoseconds */

		/**
		 *  @bug this is not a safe conversion if time is too large
		 */
		return (int64_t)time;
	}
#endif

	/*
	 * CMVC 132647 improve perf of omrthread_get_self_cpu_time()
	 * Testing on various x86 and PPC Linuxes shows some improvement on RHEL5
	 * and no noticeable degradation on older Linuxes.
	 */
#if defined(LINUX) && defined(CLOCK_THREAD_CPUTIME_ID)
	{
		struct timespec time;

		if (0 == clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time)) {
			return ((int64_t)time.tv_sec * 1000 * 1000 * 1000) + time.tv_nsec;
		}
	}
#endif /* defined(LINUX) && defined(CLOCK_THREAD_CPUTIME_ID) */

	return omrthread_get_cpu_time(self);
}

/**
 * Return the amount of user mode CPU time used by an arbitrary thread,
 * in nanoseconds.
 * If arbitrary thread times are not supported, but current thread times are
 * supported, this function will still return -1 even if the current thread is
 * passed in as an argument.
 * @param[in] thread
 * @return user mode CPU time in nanoseconds
 * @return -1 not supported on this platform
 * @see omrthread_get_cpu_time, omrthread_get_self_user_time
 */
int64_t
omrthread_get_user_time(omrthread_t thread)
{
#if defined(OMR_OS_WINDOWS) && !defined(BREW)

	/* In Windows, the time spent in user mode is easily acquired.
	 * Note that this function is not supported in Win95.
	 *
	 */

	FILETIME creationTime, exitTime, kernelTime, userTime;
	int64_t totalTime;

	/* WARNING! Not supported on Win95!  Need to test to ensure this fails gracefully */

	if (GetThreadTimes(thread->handle, &creationTime, &exitTime, &kernelTime, &userTime)) {
		totalTime = ((int64_t)userTime.dwLowDateTime | ((int64_t)userTime.dwHighDateTime << 32));

		/* totalTime is in 100's of nanos.  Convert to nanos */
		return totalTime * 100;
	}

#endif	/* defined(OMR_OS_WINDOWS) && !defined(BREW) */

#if defined(AIXPPC)

	/* AIX provides a function call that returns an entire structure of
	 * information about the thread.
	 *
	 */

	struct rusage usageInfo;

	memset(&usageInfo, 0, sizeof(usageInfo));
	if (0 == pthread_getrusage_np(thread->handle, &usageInfo, PTHRDSINFO_RUSAGE_COLLECT)) {
		int64_t userTime = (int64_t)usageInfo.ru_utime.tv_sec * 1000 * 1000; /* microseconds */
		userTime += (int64_t)usageInfo.ru_utime.tv_usec;
		return userTime * 1000; 	/* convert to nanoseconds */
	}
#endif	/* AIX */

	/* If none of the above platforms, then return -1 (not supported). */

	return -1;
}
/**
 * Return the amount of user mode CPU time used by the current thread,
 * in nanoseconds.
 * @param[in] self The current thread. Must be non-NULL. Pass this in to avoid the cost of pthread_getspecific().
 * @return user mode CPU time in nanoseconds
 * @retval -1 not supported on this platform
 * @see omrthread_get_user_time, omrthread_get_self_cpu_time
 */
int64_t
omrthread_get_self_user_time(omrthread_t self)
{
	return omrthread_get_user_time(self);
}


/**
 * Return the OS handle for a thread.
 *
 * @param thread a thread
 * @return OS handle
 */
uintptr_t
omrthread_get_handle(omrthread_t thread)
{
#if defined(J9ZOS390)
	OSTHREAD tempHandle = thread->handle;
	return (uintptr_t)*(unsigned long long *)&tempHandle;
#else
	return (uintptr_t)thread->handle;
#endif
}


/**
 * Enable or disable monitoring of stack usage.
 *
 * @param[in] enable 0 to disable or non-zero to enable.
 * @return none
 *
 */
void
omrthread_enable_stack_usage(uintptr_t enable)
{
#if defined(OMR_OS_WINDOWS)
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	lib->stack_usage = enable;
#endif /* defined(OMR_OS_WINDOWS) */
}


/**
 * Return the approximate stack usage by a thread
 *
 * @param[in] thread a thread
 * @return 0 if the stack has not been painted<br>
 * (uintptr_t)-1 if the stack has overflowed<br>
 *  otherwise the approximate maximum number of bytes used on the stack
 *
 */
uintptr_t
omrthread_get_stack_usage(omrthread_t thread)
{
#if defined(LINUX) || defined (J9ZOS390) || defined(AIXPPC) || defined(OSX)
	return 0;
#else /* defined(LINUX) || defined (J9ZOS390) || defined(AIXPPC) || defined(OSX) */
	uintptr_t *tos = thread->tos;
	uintptr_t count = thread->stacksize;

	if (tos == NULL || count == 0) {
		return 0;
	}

	if (*tos != STACK_PATTERN) {
		return (uintptr_t)-1;
	}

	while (*tos++ == STACK_PATTERN) {
		count -= sizeof(uintptr_t);
	}

	return count;
#endif /* defined(LINUX) || defined (J9ZOS390) || defined(AIXPPC) || defined(OSX) */
}


/*
 * Paint a thread's stack.
 *
 * Attempt to paint the stack region with STACK_PATTERN so we can
 * detect stack usage.  Sets thread->tos to the maximum stack
 * address.
 * @note This won't work on PA-RISC because of backwards stacks
 *
 * @param thread a thread
 * @return none
 */
void
paint_stack(omrthread_t thread)
{
	/* Only supported on Windows */
#if defined(OMR_OS_WINDOWS)
	MEMORY_BASIC_INFORMATION memInfo;
	SYSTEM_INFO sysInfo;
	uintptr_t *curr;
	uintptr_t *stack = (uintptr_t *)&stack;

	/*	Find out where the stack starts. */
	VirtualQuery(stack, &memInfo, sizeof(MEMORY_BASIC_INFORMATION));

	/* Start painting. Skip the top 32 slots (to protect this stack frame) */
	curr = stack - 32;
	__try {
		while (curr > (uintptr_t *)memInfo.AllocationBase) {
			*curr-- = STACK_PATTERN;
		}
	} __except (1) {
		/*  Ran off the end of the stack. Stop */
	}
	thread->tos = curr + 1;

	/* Round up to the system page size. */
	GetSystemInfo(&sysInfo);
	thread->stacksize = ((uintptr_t)stack - (uintptr_t)thread->tos + sysInfo.dwPageSize) & ~((uintptr_t)sysInfo.dwPageSize - 1);
#endif /* defined(OMR_OS_WINDOWS) */
}


/**
 * Returns a thread's stack size.
 *
 * @param[in] thread a thread
 * @return 0 if the thread is an attached thread
 * or the initial size of the thread's stack,
 *
 */
uintptr_t
omrthread_get_stack_size(omrthread_t thread)
{
	return thread->stacksize;
}


/**
 * Return the OS's scheduling policy and priority for a thread.
 *
 * Query the OS to determine the actual priority of the specified thread.
 * The priority and scheduling policy are stored in the pointers provided.
 * On Windows the "policy" contains the thread's priority class.
 * On POSIX systems it contains the scheduling policy
 * On OS/2 no information is available.  0 is stored in both pointers.
 *
 * @param[in] thread a thread
 * @param[in] policy pointer to location where policy will be stored (non-NULL)
 * @param[in] priority pointer to location where priority will be stored (non-NULL)
 * @return 0 on success or negative value on failure
 *
 */
intptr_t
omrthread_get_os_priority(omrthread_t thread, intptr_t *policy, intptr_t *priority)
{
#ifdef J9_POSIX_THREADS
	{
		struct sched_param sched_param;
		int osPolicy, rc;

		rc = pthread_getschedparam(thread->handle, &osPolicy, &sched_param);
		if (rc) {
			return -1;
		}
		*priority = sched_param.sched_priority;
		*policy = osPolicy;
	}

#elif defined(OMR_OS_WINDOWS) && !defined(BREW)

	*priority = GetThreadPriority(thread->handle);
	if (*priority == THREAD_PRIORITY_ERROR_RETURN) {
		return -1;
	}

	*policy = GetPriorityClass(thread->handle);
	if (*policy == 0) {
		return -1;
	}

#elif defined(OS2) || defined(BREW)
	*priority = 0;
	*policy = 0;

#elif defined(ITRONGNU)
	{
		TPRI taskPriority;
		UH taskState;
		ID task = thread->handle;
		ER errorCode;

		errorCode = vtsk_sts(&taskState, &taskPriority, thread->handle);
		ITRON_DISPLAY_ERCD("vtsk_sts", errorCode);
		*policy = 0;
		*priority = taskPriority;
	}
#elif defined(J9ZOS390)
	*priority = 0;
	*policy = 0;
#else
#error Unknown platform
#endif

	return 0;
}


/**
 * Return the amount of CPU time used by the entire process in nanoseconds.
 * @param void
 * @return time in nanoseconds
 * @retval -1 not supported on this platform
 * @see omrthread_get_self_cpu_time, omrthread_get_user_time
 */
int64_t
omrthread_get_process_cpu_time(void)
{
#if defined(OMR_OS_WINDOWS) && !defined(BREW)
	FILETIME creationTime, exitTime, kernelTime, userTime;
	int64_t totalTime;

	if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
		totalTime = ((int64_t)kernelTime.dwLowDateTime | ((int64_t)kernelTime.dwHighDateTime << 32))
					+ ((int64_t)userTime.dwLowDateTime | ((int64_t)userTime.dwHighDateTime << 32));

		/* totalTime is in 100's of nanos.  Convert to nanos */
		return totalTime * GET_PROCESS_TIMES_IN_NANO;
	}
#endif	/* defined(OMR_OS_WINDOWS) && !defined(BREW) */

	return -1;

}

/**
 * Calculates the total amount of process i.e system (kernel) and user time in nanoseconds.
 * @param[out] processTime is the address of the structure that the user and system time gets stored in.
 * @return 0 : successful completion; -1 : Unsupported / not implemented on this platform; -2 : Error occured.
 */
intptr_t
omrthread_get_process_times(omrthread_process_time_t *processTime)
{
	if (processTime != NULL) {
#if defined(OMR_OS_WINDOWS)

		/* We don't use creationTime and exitTime but GetProcessTimes() needs them */
		FILETIME creationTime;
		FILETIME exitTime;
		FILETIME systemTime;
		FILETIME userTime;

		memset(&creationTime, 0, sizeof(creationTime));
		memset(&exitTime, 0, sizeof(exitTime));
		memset(&systemTime, 0, sizeof(systemTime));
		memset(&userTime, 0, sizeof(userTime));

		/* rvalue's are in 100 nanosecond units. Convert to nano's by multiplying 100 to them
		 * WARNING: GetProcessTimes does not exist on pre-Win 98
		 */
		if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &systemTime, &userTime)) {
			processTime->_userTime = GET_PROCESS_TIMES_IN_NANO *
									 ((int64_t)userTime.dwLowDateTime | ((int64_t)userTime.dwHighDateTime << 32));
			processTime->_systemTime = GET_PROCESS_TIMES_IN_NANO *
									   ((int64_t)systemTime.dwLowDateTime | ((int64_t)systemTime.dwHighDateTime << 32));
			return 0;
		} else {
			/* GetLastError() indicates reason for failure in GetProcessTimes(). */
			Trc_THR_ThreadGetProcessTimes_GetProcessTimesFailed(GetLastError());
			return -2;
		}
#endif	/* defined(OMR_OS_WINDOWS) */

#if defined(LINUX) || defined(AIXPPC) || defined(OSX)
		struct rusage rUsage;
		memset(&rUsage, 0, sizeof(rUsage));

		/* if getrusage() returns successfully, store the time values in processTime.*/
		if (0 == getrusage(RUSAGE_SELF, &rUsage)) {
			processTime->_userTime   = (SEC_TO_NANO_CONVERSION_CONSTANT   * (int64_t)rUsage.ru_utime.tv_sec) +
									   (MICRO_TO_NANO_CONVERSION_CONSTANT * (int64_t)rUsage.ru_utime.tv_usec);
			processTime->_systemTime = (SEC_TO_NANO_CONVERSION_CONSTANT   * (int64_t)rUsage.ru_stime.tv_sec) +
									   (MICRO_TO_NANO_CONVERSION_CONSTANT * (int64_t)rUsage.ru_stime.tv_usec);
			return 0;
		} else {
			/* Error in getrusage */
			Trc_THR_ThreadGetProcessTimes_getrusageFailed(errno);
			return -2;
		}
#endif /* defined(LINUX) || defined(AIXPPC) || defined(OSX) */

#if defined (J9ZOS390)
		/* Input buffer, pointer to buffer, and size of the buffer area. */
		struct pgtha pgthaInst;
		unsigned char *cursor = (unsigned char *) &pgthaInst;
		uint32_t input_size = sizeof(struct pgtha);
		/* Output buffer, pointer to buffer, and size of the buffer area. */
		struct j9pg_thread_data threadData;
		unsigned char *output_buffer = (unsigned char *) &threadData;
		uint32_t output_size = sizeof(struct j9pg_thread_data);
		struct pgthc *pgthc = NULL;
		uint32_t ret_val = 0;
		uint32_t ret_code = 0;
		uint32_t reason_code = 0;
		uint32_t data_offset = 0;

		memset(cursor, 0, sizeof(pgthaInst));
		memset(output_buffer, 0, sizeof(threadData));

		/* Fill in PGTHAPID with current process's PID. */
		pgthaInst.pid = getpid();

		/* We want data for the current PID only */
		pgthaInst.accesspid = PGTHA_ACCESS_CURRENT;

		/* We don't need thread data. */
		pgthaInst.flag1 = PGTHA_FLAG_PROCESS_DATA;
#if defined(_LP64)
		BPX4GTH(
			&input_size,            /* Size of pgtha structure */
			&cursor,                /* Pointer to address of pgtha */
			&output_size,           /* output size with padding */
			&output_buffer,         /* Output buffer */
			&ret_val,               /* return should be 0 on success */
			&ret_code,              /* On failure, this will have the reason */
			&reason_code);          /* more info on error */
#else
		BPX1GTH(
			&input_size,
			&cursor,
			&output_size,
			&output_buffer,
			&ret_val,
			&ret_code,
			&reason_code);
#endif
		if (-1 == ret_val) {
			/* Error in BPXNGTH() invocation. For information on error codes, refer:
			 * http://www-01.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.bpxb100/gth.htm
			 */
			Trc_THR_ThreadGetProcessTimes_bpxNgthFailed(ret_code, reason_code);
			return -2; /* Indicate failure (instead of unsupported). */
		}

		/* Obtain an integer from the 3 bytes filled in. */
		data_offset = *((unsigned int *) threadData.pgthb.offc);

		/* Discard the least significant byte and use only the most significant 3. */
		data_offset = (data_offset & I32MAXVAL) >> 8;

		/* Offset to the structure containing process times (among other things). */
		pgthc = (struct pgthc *)(((char *) &threadData) + data_offset);

		/* These times are in 100's of seconds. Convert to nanoseconds. */
		processTime->_userTime = (int64_t)pgthc->usertime * 10000000;
		processTime->_systemTime = (int64_t)pgthc->systime * 10000000;

		return 0; /* Return success. */
#endif /* J9ZOS390 */
	}

	/* omrthread_get_process_times not implemented on other architectures. */
	return -1;
}

/**
 * Return a monotonically increasing hi resolution clock in nanoseconds.
 * This code is a copy of omrtime_nano_time for Linux and omrtime_hires_clock on Windows
 * from the port library as we cannot call the port functions from the thread library.
 *
 * @return time with nanosecond granularity
 */
uint64_t
omrthread_get_hires_clock(void)
{
#if defined(LINUX) && (defined(J9HAMMER) || defined(J9X86) || defined(RISCV64))
#define J9TIME_NANOSECONDS_PER_SECOND	J9CONST_U64(1000000000)
	struct timespec ts;
	uint64_t hiresTime = 0;

	if (0 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
		hiresTime = ((uint64_t)ts.tv_sec * J9TIME_NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec;
	}

	return hiresTime;
#elif defined(OMR_OS_WINDOWS) /* defined(LINUX) && (defined(J9HAMMER) || defined(J9X86) || defined(RISCV64)) */
	LARGE_INTEGER i;

	if (QueryPerformanceCounter(&i)) {
		return (uint64_t)i.QuadPart;
	} else {
		return (uint64_t)GetTickCount();
	}
#elif defined(OSX) /* defined(OMR_OS_WINDOWS) */
#define J9TIME_NANOSECONDS_PER_SECOND	J9CONST_U64(1000000000)
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	mach_timespec_t mt;
	uint64_t hiresTime = 0;

	clock_get_time(lib->clockService, &mt);

	if (KERN_SUCCESS == clock_get_time(lib->clockService, &mt)) {
		hiresTime = ((uint64_t)mt.tv_sec * J9TIME_NANOSECONDS_PER_SECOND) + (uint64_t)mt.tv_nsec;
	}

	return hiresTime;
#else /* defined(OSX) */
	return GET_HIRES_CLOCK();
#endif /* defined(OSX) */
}

#define THREAD_WALK_RESOURCE_USAGE_MUTEX_HELD	0x1
#define THREAD_WALK_MONITOR_MUTEX_HELD			0x2

/**
 * Calculate the cpu usage information for the thread categories of System, application
 * and monitor. Further categorize system JVM threads into GC, JIT and others.
 * Also add the usage details of threads that have already exited into the appropriate
 * categories.
 * @param cpuUsage[in] Cpu usage details to be filled in.
 * @return 0 on success, -J9THREAD_ERR_USAGE_RETRIEVAL_ERROR on failure
 *         and -J9THREAD_ERR_USAGE_RETRIEVAL_UNSUPPORTED if -XX:-EnableCPUMonitor has been set.
 */
intptr_t
omrthread_get_jvm_cpu_usage_info(J9ThreadsCpuUsage *cpuUsage)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	J9ThreadsCpuUsage *cumulativeUsage = &lib->cumulativeThreadsInfo;
	int64_t gcCpuTime = 0;
	int64_t jitCpuTime = 0;
	int64_t threadCpuTime = 0;
	int64_t systemJvmCpuTime = 0;
	int64_t resourceMonitorCpuTime = 0;
	int64_t applicationCpuTime = 0;
	int64_t userCpuTime[J9THREAD_MAX_USER_DEFINED_THREAD_CATEGORIES] = { 0 };
	omrthread_t walkThread = NULL;
	pool_state state;
	intptr_t ret = J9THREAD_SUCCESS;
	intptr_t result = 0;
	intptr_t i = 0;
	uint64_t preTimestamp = 0;
	uint64_t postTimestamp = 0;

	/* If -XX:-EnableCPUMonitor has been set, this function returns an error */
	if (OMR_ARE_NO_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR)) {
		return -J9THREAD_ERR_USAGE_RETRIEVAL_UNSUPPORTED;
	}

	/* Need to hold the lib->monitor_mutex while walking the thread_pool */
	GLOBAL_LOCK_SIMPLE(lib);
	lib->threadWalkMutexesHeld = THREAD_WALK_MONITOR_MUTEX_HELD;
	OMROSMUTEX_ENTER(lib->resourceUsageMutex);
	lib->threadWalkMutexesHeld |= THREAD_WALK_RESOURCE_USAGE_MUTEX_HELD;

	/* pre timestamp in microseconds */
	preTimestamp = omrthread_get_hires_clock() / 1000;

	/* Walk the list of omrthread's and get the cpu time for each and categorize them */
	walkThread = pool_startDo(lib->thread_pool, &state);
	for (; NULL != walkThread; walkThread = pool_nextDo(&state)) {

		/* If a thread has just been created, possible that the mutex is not yet initialized.
		 * So we check if J9THREAD_FLAG_CPU_SAMPLING_ENABLED is set without holding the lock first.
		 * If not set, we ignore the thread */
		if (0 == (walkThread->flags & J9THREAD_FLAG_CPU_SAMPLING_ENABLED)) {
			continue;
		}
		THREAD_LOCK(walkThread, CALLER_GET_JVM_CPU_USAGE_INFO);
		/* Check the flag again, this time with the THREAD_LOCK held, to ensure that it is still
		 * marked ready for cpu accounting */
		if (0 == (walkThread->flags & J9THREAD_FLAG_CPU_SAMPLING_ENABLED)) {
			THREAD_UNLOCK(walkThread);
			continue;
		}
		threadCpuTime = 0;
		result = omrthread_get_cpu_time_ex(walkThread, &threadCpuTime);
		THREAD_UNLOCK(walkThread);

		/* Obtaining CPU time fails with error code J9THREAD_ERR_NO_SUCH_THREAD, implying the thread isn't available to
		 * the OS; skip accounting for such threads, as storeExitCpuUsage() must have already done this for it. Return
		 * error for other (internal) errors.
		 */
		if (J9THREAD_SUCCESS != result) {
			result &= ~J9THREAD_ERR_OS_ERRNO_SET;
			if (J9THREAD_ERR_NO_SUCH_THREAD == result) {
				continue;
			} else if (J9THREAD_ERR == result) {
				ret = -J9THREAD_ERR_USAGE_RETRIEVAL_ERROR;
				break;
			}
		}
		/* Only account for the quantum from the previous category change */
		threadCpuTime /= 1000;
		threadCpuTime -= walkThread->lastCategorySwitchTime;
		if (OMR_ARE_ALL_BITS_SET(walkThread->effective_category, J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD)) {
			resourceMonitorCpuTime += threadCpuTime;
		} else if (OMR_ARE_ALL_BITS_SET(walkThread->effective_category, J9THREAD_CATEGORY_SYSTEM_THREAD)) {
			systemJvmCpuTime += threadCpuTime;
			if (OMR_ARE_ALL_BITS_SET(walkThread->effective_category, J9THREAD_CATEGORY_SYSTEM_GC_THREAD)) {
				gcCpuTime += threadCpuTime;
			} else if (OMR_ARE_ALL_BITS_SET(walkThread->effective_category, J9THREAD_CATEGORY_SYSTEM_JIT_THREAD)) {
				jitCpuTime += threadCpuTime;
			}
		} else if (OMR_ARE_ALL_BITS_SET(walkThread->effective_category, J9THREAD_CATEGORY_APPLICATION_THREAD)) {
			applicationCpuTime += threadCpuTime;

			/* If the thread has been set to a user defined category, count it in the right category */
			if ((walkThread->effective_category & J9THREAD_USER_DEFINED_THREAD_CATEGORY_MASK) > 0) {
				int userCat = (walkThread->effective_category - J9THREAD_USER_DEFINED_THREAD_CATEGORY_1) & J9THREAD_USER_DEFINED_THREAD_CATEGORY_MASK;
				userCat >>= J9THREAD_USER_DEFINED_THREAD_CATEGORY_BIT_SHIFT;
				ASSERT(userCat >= J9THREAD_MAX_USER_DEFINED_THREAD_CATEGORIES);
				userCpuTime[userCat] += threadCpuTime;
			}
		}
	}

	/* post timestamp in microseconds */
	postTimestamp = omrthread_get_hires_clock() / 1000;
	/* Check for invalid timestamp */
	if ((0 == preTimestamp) || (0 == postTimestamp) || (postTimestamp < preTimestamp)) {
		ret = -J9THREAD_ERR_INVALID_TIMESTAMP;
		goto err_exit;
	}

	/* Add the values that we have obtained above to the CPU usage of the threads that
	 * have previously exited.
	 */
	cpuUsage->timestamp = (preTimestamp + postTimestamp) / 2;
	cpuUsage->applicationCpuTime = cumulativeUsage->applicationCpuTime + applicationCpuTime;
	cpuUsage->resourceMonitorCpuTime = cumulativeUsage->resourceMonitorCpuTime + resourceMonitorCpuTime;
	cpuUsage->systemJvmCpuTime = cumulativeUsage->systemJvmCpuTime + systemJvmCpuTime;
	cpuUsage->gcCpuTime = cumulativeUsage->gcCpuTime + gcCpuTime;
	cpuUsage->jitCpuTime = cumulativeUsage->jitCpuTime + jitCpuTime;
	for (i = 0; i < J9THREAD_MAX_USER_DEFINED_THREAD_CATEGORIES; i++) {
		/* Temp workaround: i386 generates xmm instructions to optimize the array copy and ends up crashing for
		 * some unknown reason. The if check introduces variability in the loop causing gcc not to use xmm.
		 */
		if ((cumulativeUsage->applicationUserCpuTime[i] > 0) || (userCpuTime[i] > 0)) {
			cpuUsage->applicationUserCpuTime[i] = cumulativeUsage->applicationUserCpuTime[i] + userCpuTime[i];
		}
	}

err_exit:
	lib->threadWalkMutexesHeld &= ~(uintptr_t)THREAD_WALK_RESOURCE_USAGE_MUTEX_HELD;
	OMROSMUTEX_EXIT(lib->resourceUsageMutex);
	lib->threadWalkMutexesHeld = 0;
	GLOBAL_UNLOCK_SIMPLE(lib);
	if (ret < 0) {
		Trc_THR_omrthread_get_jvm_cpu_usage_thread_walk_failed(ret);
		Trc_THR_omrthread_get_jvm_cpu_usage_info_get_cpu_time_ex_failed(walkThread, result);
		Trc_THR_omrthread_get_jvm_cpu_usage_timestamp_failed(preTimestamp, postTimestamp);
	}
	return ret;
}

/**
 * Called by the signal handler in javadump.cpp to release any mutexes
 * held by omrthread_get_jvm_cpu_usage_info if the thread walk fails.
 * Can only be used to be called in a signal handler running in the
 * context of the same thread that locked the mutexes in the first place
 * for the locks to be successfully released.
 */
void
omrthread_get_jvm_cpu_usage_info_error_recovery(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	if (lib->threadWalkMutexesHeld & THREAD_WALK_RESOURCE_USAGE_MUTEX_HELD) {
		lib->threadWalkMutexesHeld &= ~(uintptr_t)THREAD_WALK_RESOURCE_USAGE_MUTEX_HELD;
		OMROSMUTEX_EXIT(lib->resourceUsageMutex);
	}
	if (lib->threadWalkMutexesHeld & THREAD_WALK_MONITOR_MUTEX_HELD) {
		lib->threadWalkMutexesHeld = 0;
		GLOBAL_UNLOCK_SIMPLE(lib);
	}
}
