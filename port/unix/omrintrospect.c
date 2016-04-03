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
 * @brief process introspection support
 */

#if defined(LINUX)
/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatability */
#define _GNU_SOURCE
#define __USE_GNU 1
#endif /* defined(LINUX) */

#include <pthread.h>
#include <ucontext.h>
#include <sched.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>

#if defined(LINUX)
#include <dirent.h>
#include <dlfcn.h>
#elif defined(AIXPPC)
#include <sys/ldr.h>
#include <sys/debug.h>
#include <procinfo.h>
#include <sys/thread.h>
#elif defined(J9ZOS390)
#include <stdlib.h>
#endif

#include "omrintrospect.h"
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrsignal_context.h"
#include "omrutilbase.h"
#include "ut_omrport.h"

/*
 * This is the signal that we use to suspend the various threads. This signal must be not be collapsable and
 * must pass on values specified by sigqueue.
 */
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
#define SUSPEND_SIG PPG_introspect_threadSuspendSignal
#else /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
#define SUSPEND_SIG SIGRTMIN
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */

/*
 * Internal semaphore retry timeouts. Some implementations of poll seem unreliable so we add a
 * timeout to poll calls whether or not we have a deadline so we have a sample based scheduler visible
 * spinlock.
 */
#if defined(LINUXPPC)
/* poll timeout in milliseconds */
#define POLL_RETRY_INTERVAL 100
#elif defined(AIXPPC)
/* AIX close call blocks until all other calls using the file descriptor return to user
 * space so we need to spin reasonably quickly or we'll not resume until the timeout in
 * error cases.
 */
#define POLL_RETRY_INTERVAL 1000
#else
/* -1 means block indefinitely */
#define POLL_RETRY_INTERVAL -1
#endif



typedef struct {
	int descriptor_pair[2];
	volatile uintptr_t in_count;
	volatile uintptr_t out_count;
	uintptr_t initial_value;
	uintptr_t spinlock;
	volatile uintptr_t released;
} barrier_r;


typedef struct {
	int descriptor_pair[2];
	volatile uintptr_t sem_value;
	volatile uintptr_t initial_value;
	uintptr_t spinlock;
} sem_t_r;

/*
 * This structure contains platform specific information necessary to iterate over the threads
 */
struct PlatformWalkData {
	/* thread to filter out of the platform iterator */
	uintptr_t filterThread;
	/* controller thread */
	uintptr_t controllerThread;
	/* volatile error flag for upcall handlers, shared across threads, non-zero means error */
	volatile unsigned char error;
	/* is the state currently consistent. False inside startDo/nextDo, must be true otherwise */
	volatile unsigned char consistent;
	/* old signal handler for the suspend signal */
	struct sigaction oldHandler;
	/* old mask */
	sigset_t old_mask;
	/* backpointer to encapsulating state */
	J9ThreadWalkState *state;
	/* total number of threads in the process, including calling thread */
	long threadCount;
	/* suspended threads unharvested */
	int threadsOutstanding;
	/* thread we're constructing */
	J9PlatformThread *thread;
	/* platform allocated context, or program allocated context */
	unsigned char platformAllocatedContext;
	/* records whether we need to clean up in resume */
	unsigned char cleanupRequired;
#ifdef AIXPPC
	/* records whether there are threads suspended during initialization */
	int uninitializedThreads;
#if defined(J9OS_I5)
	/* thread detail for the current thread */
	struct __pthrdsinfo pinfo;
	/* which thread will we return next */
	pthread_t thr;
#endif /* defined(J9OS_I5) */
#endif
	/* the semaphore all suspended threads wait on */
	sem_t_r client_sem;
	/* the semaphore the controller thread waits on */
	sem_t_r controller_sem;
	/* barrier to prevent any threads exiting the introspect logic until all are ready */
	barrier_r release_barrier;
};


/* Wrapper for the close function that checks we're not closing stdin,stdout or stderr and
 * retries on EINTR.
 */
static int
close_wrapper(int fd)
{
	if (fd != -1) {
		int rc = 0;
		do {
			close(fd);
		} while (rc == EINTR);

		return rc;
	}

	return 0;
}

/*
 * This function constructs a barrier for use within signal handler contexts. It's a combination o
 * spinlock techniques modified with blocking read/write/poll scheduler visible calls to avoid
 * thread contention.
 *
 * @param barrier the barrier structure to initialize
 * @param value the number of entrants to expect
 *
 * @return 0 on success, -1 otherwise
 */
static int
barrier_init_r(barrier_r *barrier, int value)
{
	uintptr_t old_value;
	memset(barrier, 0, sizeof(barrier_r));

	if (pipe(barrier->descriptor_pair) != 0) {
		return -1;
	}

	do {
		old_value = barrier->initial_value;
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->initial_value, old_value, value, &barrier->spinlock) != old_value);
	do {
		old_value = barrier->in_count;
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->in_count, old_value, value, &barrier->spinlock) != old_value);
	do {
		old_value = barrier->released;
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->released, old_value, 0, &barrier->spinlock) != old_value);

	return 0;
}

/*
 * This function is a utility function used by the blocking barrier functions to wait in a polite, scheduler
 * visible fashion until either prompted or the deadline has expired.
 *
 * @param barrier the barrier we're operating on
 * @param deadline the system time in seconds after which we should return regardless of prompting
 *
 * @return 0 if cleanly prompted, -1 if timed out, other values are various errors
 */
static int
barrier_block_until_poked(barrier_r *barrier, uintptr_t deadline)
{
	int ret = 0;
	struct pollfd fds[1];
	int interval = POLL_RETRY_INTERVAL;
	struct timespec spec;

	fds[0].fd = barrier->descriptor_pair[0];
	fds[0].events = POLLHUP | POLLERR | POLLNVAL | POLLIN;
	fds[0].revents = 0;

	if (deadline == 0) {
		/* block indefinitely */
		interval = POLL_RETRY_INTERVAL;
	} else {
		/* calculate the interval */
		if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
			interval = 0;
		} else {
			interval = (deadline - spec.tv_sec) * 1000;

			/* ensure that the poll timeout is set to the lesser of the retry interval or the deadline */
			if (interval < 0) {
				/* deadline has expired so run through once then bail at the end */
				interval = 0;
			} else if (interval > POLL_RETRY_INTERVAL && POLL_RETRY_INTERVAL != -1) {
				interval = POLL_RETRY_INTERVAL;
			}
		}
	}

	/* wait for something to change */
	ret = poll(fds, 1, interval);
	if ((ret == -1 && errno != EINTR) || fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
		return -2;
	}

	if (deadline != 0) {
		/* check if we've timed out. Do this last as the other possibilities for return are of more interest */
		if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
			return -3;
		}

		if (spec.tv_sec >= deadline) {
			return -1;
		}
	}

	return 0;
}

/*
 * This function is called by the conductor of the barrier. It will block until all entrants have entered
 * the barrier via barrier_enter_r. If a maximum amount of time to block for is specified then this will
 * time out once that deadline has expired.
 *
 * @param barrier the barrier to release
 * @param seconds the maximum number of second for which to block. Indefinitely if 0 is specified.
 *
 * @return 0 on clean release, other values are various errors
 */
static int
barrier_release_r(barrier_r *barrier, uintptr_t seconds)
{
	char byte = 0;
	int result = 0;

	int deadline = 0;
	struct timespec spec;

	if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
		seconds = 0;
	} else {
		deadline = spec.tv_sec + seconds;
	}

	if ((compareAndSwapUDATA((uintptr_t *)&barrier->released, 0, 1, &barrier->spinlock)) != 0) {
		/* barrier->released should have been 0, write something into the pipe so that poll doesn't block,
		 * converts this to a busy wait
		 */
		barrier->released = 1;
		write(barrier->descriptor_pair[1], &byte, 1);
	}

	/* wait until all entrants have arrived */
	while ((compareAndSwapUDATA((uintptr_t *)&barrier->in_count, -1, -1, &barrier->spinlock)) > 0) {
		if ((result = barrier_block_until_poked(barrier, deadline)) == -1) {
			/* timeout or error */
			break;
		}
	}

	write(barrier->descriptor_pair[1], &byte, 1);
#if !defined(J9ZOS390) && !defined(AIXPPC)
	/* On AIX it is not legal to call fdatasync() inside a signal handler */
	fdatasync(barrier->descriptor_pair[1]);
#endif

	return result;
}

/*
 * This function is called by entrants to the barrier. It will block until all expected entrants have called this
 * function and barrier_release_r has been called, or the deadline has expired.
 *
 * @param barrier the barrier to enter
 * @param deadline the system time in seconds after which we should unblock
 *
 * @return 0 on clean release, other values indicate errors
 */
static int
barrier_enter_r(barrier_r *barrier, uintptr_t deadline)
{
	uintptr_t old_value = 0;
	char byte = 1;
	int result = 0;

	/* decrement the wait count */
	do {
		old_value = barrier->in_count;
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->in_count, old_value, old_value - 1, &barrier->spinlock) != old_value);

	if (old_value == 1 && (compareAndSwapUDATA((uintptr_t *)&barrier->released, 0, 0, &barrier->spinlock))) {
		/* we're the last through the barrier so wake everyone up */
		write(barrier->descriptor_pair[1], &byte, 1);
	}

	/* if we're entering a barrier with a negative count then count us out but we don't need to do anything */

	/* wait until we are formally released */
	while (compareAndSwapUDATA((uintptr_t *)&barrier->in_count, -1, -1, &barrier->spinlock) > 0 || !barrier->released) {
		if ((result = barrier_block_until_poked(barrier, deadline)) < 0) {
			/* timeout or error */
			break;
		}
	}

	/* increment the out count */
	do {
		old_value = barrier->out_count;
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->out_count, old_value, old_value + 1, &barrier->spinlock) != old_value);

	return result;
}

/*
 * This function updates the expected number of clients the barrier is waiting for. If all clients have already entered the barrier
 * the the update will fail.
 *
 * @param barrier the barrier to update
 * @param new_value the new number of entrants expected
 *
 * @return 0 on success, -1 on failure
 */
static int
barrier_update_r(barrier_r *barrier, int new_value)
{
	/* we're altering the expected number of entries into the barrier */
	uintptr_t old_value;
	int difference = new_value - barrier->initial_value;

	if (difference == 0) {
		return 0;
	}

	do {
		old_value = barrier->in_count;
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->in_count, old_value, old_value + difference, &barrier->spinlock) != old_value);

	if (old_value < 0 || (old_value == 0 && barrier->initial_value != 0)) {
		int restore_value = old_value;

		/* barrier was already exited, so undo update and return error */
		do {
			old_value = barrier->in_count;
		} while (compareAndSwapUDATA((uintptr_t *)&barrier->in_count, old_value, restore_value, &barrier->spinlock) != old_value);

		return -1;
	} else {
		/* we've updated the in_count so update the initial_value */
		do {
			old_value = barrier->initial_value;
		} while (compareAndSwapUDATA((uintptr_t *)&barrier->initial_value, old_value, new_value, &barrier->spinlock) != old_value);

		/* don't need to notify anyone if updated_value is <= 0 as release will do it when called */

		return 0;
	}
}

/*
 * This function destroys a barrier created by barrier_init_r. It first wakes up any waiters then waits for them
 * to exit the barrier before returning if requested.
 *
 * @param barrier the barrier to destroy
 * @int block if non-zero the barrier will block until waiters have exited
 */
static void
barrier_destroy_r(barrier_r *barrier, int block)
{
	int current = 0;
	int in = 0;
	char byte = 1;

	/* clean up and wait for all waiters to exit */
	write(barrier->descriptor_pair[1], &byte, 1);
#if !defined(J9ZOS390) && !defined(AIXPPC)
	/* On AIX it is not legal to call fdatasync() inside a signal handler */
	fdatasync(barrier->descriptor_pair[1]);
#endif
	close_wrapper(barrier->descriptor_pair[1]);
	close_wrapper(barrier->descriptor_pair[0]);

	/* decrement the wait count */
	if (block) {
		do {
			current = compareAndSwapUDATA((uintptr_t *)&barrier->out_count, -1, -1, &barrier->spinlock);
			in = compareAndSwapUDATA((uintptr_t *)&barrier->in_count, -1, -1, &barrier->spinlock);
		} while (current + in < barrier->initial_value);
	}
}

/*
 * Constructs a semaphore for use within signal handlers. The semaphore uses a combination of
 * spinlock techniques to acquire the lock and blocking read/write/poll calls to prevent massive
 * contention between threads.
 *
 * @param sem the semaphore to acquire
 * @param value the initial count for the semaphore
 *
 * @return 0 on success, -1 otherwise
 */
static int
sem_init_r(sem_t_r *sem, int value)
{
	if (pipe(sem->descriptor_pair) != 0) {
		return -1;
	}

	sem->initial_value = value;
	sem->sem_value = value;
	sem->spinlock = 0;

	return 0;
}

/*
 * This function attempts to acquire the semaphore. It will block for, at most, the number of
 * seconds specified.
 *
 * @param sem the semaphore to acquire
 * @param seconds the maximum number of seconds the block for. Indefinitely if 0 is specified.
 *
 * @return 0 if semaphore is acquire, < 0 on error, -1 means timeout occured
 */
static int
sem_timedwait_r(sem_t_r *sem, uintptr_t seconds)
{
	uintptr_t old_value = -1;
	char byte = 0;
	int ret = -1;
	struct pollfd fds[1];
	struct timespec spec;
	int deadline = 0;
	int interval = seconds;
	fds[0].fd = sem->descriptor_pair[0];
	fds[0].events = POLLHUP | POLLERR | POLLNVAL | POLLIN;
	fds[0].revents = 0;

	if (seconds == 0) {
		/* block indefinitely */
		interval = POLL_RETRY_INTERVAL;
	} else {
		/* calculate the deadline */
		if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
			interval = 0;
		} else {
			deadline = seconds + spec.tv_sec;

			/* ensure that the poll timeout is set to the lesser of the retry interval or the deadline */
			if ((seconds * 1000) < POLL_RETRY_INTERVAL || POLL_RETRY_INTERVAL == -1) {
				/* poll timeout is specified in milliseconds */
				interval = seconds * 1000;
			} else {
				interval = POLL_RETRY_INTERVAL;
			}
		}
	}

	while (1) {
		old_value = compareAndSwapUDATA((uintptr_t *)&sem->sem_value, -1, -1, &sem->spinlock);
		while (old_value > 0) {
			if (compareAndSwapUDATA((uintptr_t *)&sem->sem_value, old_value, old_value - 1, &sem->spinlock) == old_value) {
				/* successfully acquired lock */
				return 0;
			}

			old_value = sem->sem_value;
		}

		/* wait for something to change */
		ret = poll(fds, 1, interval);
		if ((ret == -1 && errno != EINTR) || fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
			return -2;
		}

		/* consume a byte off the pipe if it wasn't a timeout */
		if (ret > 0) {
			/* the pipe is configured as non-blocking, waiting is done on the poll call above*/
			ret = read(fds[0].fd, &byte, 1);
			if (ret == 0) {
				/* EOF, pipe has become invalid */
				return -4;
			}
		}

		/* check if we've timed out. Do this last as the other possibilities for return are of more interest */
		if (ret == 0 && seconds != 0) {
			if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
				return -3;
			}

			if (spec.tv_sec >= deadline) {
				return -1;
			}
		}
	}
}

/*
 * This function attempts to acquire the semaphore. It will not block if the semaphore isn't available.
 * @param sem the semaphore to acquire
 *
 * @return 0 if the semaphore was acquired, -1 otherwise.
 */
static int
sem_trywait_r(sem_t_r *sem)
{
	uintptr_t old_value = 0;

	/* try to get the lock */
	old_value = compareAndSwapUDATA((uintptr_t *)&sem->sem_value, -1, -1, &sem->spinlock);
	while (old_value > 0) {
		int value = compareAndSwapUDATA((uintptr_t *)&sem->sem_value, old_value, old_value - 1, &sem->spinlock);
		if (value == old_value) {
			/* successfully acquired lock */
			return 0;
		}

		/* retry if simply contending rather than failed */
		old_value = value;
	}

	errno = EAGAIN;
	return -1;
}

/*
 * This function wakes a client or controller thread waiting on the semaphore if any. Otherwise it increments the
 * semaphore count and will allow a single subsequent call to semget to succeed.
 *
 * @param sem the semaphore to post
 *
 * @return 0 on success, -1 on error
 */
static int
sem_post_r(sem_t_r *sem)
{
	uintptr_t old_value;
	char byte = 1;

	/* release the lock */
	do {
		old_value = sem->sem_value;
	} while (compareAndSwapUDATA((uintptr_t *)&sem->sem_value, old_value, old_value + 1, &sem->spinlock) != old_value);

	/* wake up a waiter */
	if (write(sem->descriptor_pair[1], &byte, 1) != 1) {
		return -1;
	}
#if !defined(J9ZOS390) && !defined(AIXPPC)
	/* On AIX it is not legal to call fdatasync() inside a signal handler */
	fdatasync(sem->descriptor_pair[1]);
#endif

	return 0;
}

/*
 * This function destroys a semaphore created with sem_init_r. It first reduces the semaphores capacity to 0 so that
 * no more clients can acquire the lock. If something holds the lock then we undo the block and return an error,
 * otherwise we close the file descriptors which should wake up any clients waiting on the lock.
 *
 * @param the semaphore to destroy
 *
 * @return 0 on success, -1 on error
 */
static int
sem_destroy_r(sem_t_r *sem)
{
	uintptr_t old_value;

	/* prevent the semaphore from being acquired by subtracting initial value*/
	do {
		old_value = sem->sem_value;
	} while (compareAndSwapUDATA((uintptr_t *)&sem->sem_value, old_value, old_value - sem->initial_value, &sem->spinlock) != old_value);

	if (old_value != sem->initial_value) {
		/* undo the block and return error */
		do {
			old_value = sem->sem_value;
		} while (compareAndSwapUDATA((uintptr_t *)&sem->sem_value, old_value, old_value + sem->initial_value, &sem->spinlock) != old_value);

		/* semaphore is still in use */
		return -1;
	}

	/* close the pipes */
	close_wrapper(sem->descriptor_pair[0]);
	close_wrapper(sem->descriptor_pair[1]);

	return 0;
}


/*
 * Utility function to check if we've timed out.
 *
 * @param deadline system time by which processing should have completed
 *
 * @return 1 if deadline expired, 0 otherwise
 */
static int
timedOut(uintptr_t deadline)
{
	struct timespec spec;
	if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
		return 0;
	}

	return spec.tv_sec >= deadline;
}

/*
 * Utility function to calculate remaining time before the deadline elapses.
 *
 * @param deadline system time by which processing should have completed
 *
 * @return number of seconds before timeout
 */
static int
timeout(uintptr_t deadline)
{
	int secs = 0;

	struct timespec spec;
	if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
		return 0;
	}

	secs = deadline - spec.tv_sec;

	if (secs > 0) {
		return secs;
	}

	return 0;
}

#if defined(J9OS_I5)
void
get_thread_Info(struct PlatformWalkData *data, void *context_arg, unsigned long tid)
{
	thread_context *context = (thread_context *)context_arg;
	J9ThreadWalkState *state;
	J9PlatformThread *thread;
	char buf;
	int ret = 0;
	int i = 0;
	pid_t pid = getpid();
	int64_t deadline;

	state = data->state;

	/* construct the thread to pass back */
	data->thread = state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformThread));
	if (data->thread == NULL) {
		data->error = ALLOCATION_FAILURE;
	} else {
		memset(data->thread, 0, sizeof(J9PlatformThread));
		data->thread->thread_id = tid;
		data->platformAllocatedContext = 1;
		data->thread->context = context;
	}
}
#else /* defined(J9OS_I5) */
/*
 * Signal handler used to provide preemptive up-call into native threads. We first check that
 * the signal we're handling is one that's expected to give a degree of confidence that the
 * value we're passed is a legitimate platform data pointer. After that the procedure is simple:
 * 1. wait until posted
 * 2. publish thread structure with the threads data to platform data.
 * 3. generate backtraces (must be done here for linux if using 'backtrace' call, other platforms
 *    could do this from another thread).
 * 4. post the coordinating thread
 * 5. wait for release
 *
 * The wait in step one is a cancellation point. If we wake from that semaphore and an error is noted
 * then we skip stack production and wait for release. There's no timeout on the barrier enter as
 * that logic should be dealt with by the coordinating thread.
 *
 * @param signal the number of the signal we receive. Should be SIGRTMIN or equivilent
 * @param siginfo contains data about the signal including the platform data pointer provided when queued
 * @param context_arg the context of the thread when the signal was dispatched
 */
static void
upcall_handler(int signal, siginfo_t *siginfo, void *context_arg)
{
	thread_context *context = (thread_context *)context_arg;
	J9ThreadWalkState *state;
	struct PlatformWalkData *data = NULL;
	int ret = 0;
	pid_t pid = getpid();
	unsigned long tid = omrthread_get_ras_tid();

#ifdef AIXPPCX
	struct sigaction handler;

	/* altering the signal handler doesn't affect already queued signals on AIX */
	if (sigaction(SUSPEND_SIG, NULL, &handler) == -1 || handler.sa_sigaction != upcall_handler) {
		return;
	}
#endif

	/* check that this signal was queued by this process. */
	if (siginfo->si_code != SI_QUEUE || siginfo->si_value.sival_ptr == NULL
#ifndef J9ZOS390
		/* pid is only valid on zOS if si_code <= 0. SI_QUEUE is > 0 */
		|| siginfo->si_pid != pid
#endif
	) {
		/* these are not the signals you are looking for */
		return;
	}

	data = (struct PlatformWalkData *)siginfo->si_value.sival_ptr;
	state = data->state;

	/* ignore the signal if we are the controller thread, or if an error/timeout has already occurred */
	if (data->controllerThread == tid || data->error) {
		return;
	}

	/* block until a context is requested, ignoring interrupts */
	ret = sem_timedwait_r(&data->client_sem, timeout(data->state->deadline1));

	if (ret != 0) {
		/* error or timeout in sem_timedwait_r(), set shared error flag and don't do the backtrace */
		data->error = ret;
	} else if (data->error) {
		/* error set by another thread while we were in sem_timedwait_r(), don't do the backtrace */
	} else {
		/* construct the thread to pass back */
		data->thread = state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformThread));
		if (data->thread == NULL) {
			data->error = ALLOCATION_FAILURE;
		} else {
#ifdef J9ZOS390
			int format = 0;
#endif /* J9ZOS390 */

			memset(data->thread, 0, sizeof(J9PlatformThread));
			data->thread->thread_id = tid;
			data->platformAllocatedContext = 1;
			data->thread->context = context;
#ifdef J9ZOS390
			data->thread->caa = _gtca();
			data->thread->dsa = __dsa_prev(getdsa(), __EDCWCCWI_LOGICAL, __EDCWCCWI_DOWN, NULL, data->thread->caa, &format, NULL, NULL);
			data->thread->dsa_format = format;
#endif /* J9ZOS390 */

#ifdef LINUX
			state->portLibrary->introspect_backtrace_thread(state->portLibrary, data->thread, state->heap, NULL);
			state->portLibrary->introspect_backtrace_symbols(state->portLibrary, data->thread, state->heap);
#endif /* LINUX */
		}
	}

	if (data->error) {
		/* error or timeout, exit signal handler without waiting for the controller thread */
		return;
	}
	/* release the controller */
	sem_post_r(&data->controller_sem);

	/* wait for the controller to close the pipe */
	ret = barrier_enter_r(&data->release_barrier, data->state->deadline2);
	if (ret != 0) {
		/* timeout or error */
		data->error = ret;
	}
}
#endif /* defined(J9OS_I5) */

/* These functions count the number of threads in the process. This information is necessary so we can queue the
 * correct number of signals to suspend all the threads (we might be able to simply call sigqueue until we get a
 * queue full error, but then we can't queue additional signals to prompt thread scheduling. Also it may not be
 * capped on all systems).
 * Linux kernels >= 2.6 we use a count of the entries in /proc/self/task (omitting . and ..)
 * Linux kernels == 2.4 we check the status file for /proc/.* entries and count those where the Tgid matches our pid
 * AIX and z/OS supply the getthrds64 and getthent system calls respectively
 *
 * @param data - the platform specific data, ignored on everything but AIX
 * @return number of threads in the process
 */
#if defined(LINUX)
static int
count_threads(struct PlatformWalkData *data)
{
	int thread_count = 0;
	struct dirent *file;
	int pid = getpid();
	DIR *tids = opendir("/proc/self/task");
	if (tids == NULL) {
		/* try looking for the tasks for linux 2.4 */
		DIR *proc = opendir("/proc");
		if (proc == NULL) {
			return -1;
		}

		/* threads are found in hidden folders in proc, i.e. /proc/.<pid>. We can tell if they belong to
		 * us by checking the thread group id from the 3rd line of /proc/.<pid>/status
		 */
		while ((file = readdir(proc)) != NULL) {
			/* we need a directory who's name starts with a '.' - we filter out '.' and '..' */
			if (file->d_type == DT_DIR && file->d_name[0] == '.' && file->d_name[1] != '\0' && file->d_name[1] != '.') {
				/* "/proc/.<pid>/status" 13 for /proc/ and /status, 11 for the .pid, 1 for the null */
				char buf[13 + 11 + 1] = "/proc/";
				int tgid;
				strncat(buf, file->d_name, 11);
				strncat(buf, "/status", 7);
				FILE *status = fopen(buf, "r");
				if (status != NULL) {
					if (fscanf(status, "%*[^\n]\n%*[^\n]\nTgid:%d", &tgid) == 1 && tgid == pid) {
						thread_count++;
					}
					fclose(status);
				}
			}
		}

		closedir(proc);

		/* add 1 to account for the initial thread */
		thread_count++;
	} else {
		for (thread_count = 0; readdir(tids) != NULL; thread_count++);
		/* remove the count for . and .. */
		thread_count -= 2;

		closedir(tids);
	}

	if (errno == EBADF) {
		return -2;
	}

	return thread_count;
}
#elif defined(AIXPPC)
static int
count_threads(struct PlatformWalkData *data)
{
	struct thrdentry64 thread;
	tid64_t cursor = 0;
	unsigned int count = 0;

	while (getthrds64(getpid(), &thread, sizeof(struct thrdentry64), &cursor, 1) == 1) {
		/* we don't want to count threads that are being disposed of or are pooled for reuse.
		 * Experimentation shows that threads can receive signals before they have a stack or complete construction
		 * so those are included.
		 */
		if (thread.ti_state != TSNONE && thread.ti_state != TSZOMB) {
			count++;
			if (thread.ti_state == TSIDL) {
				data->uninitializedThreads++;
			}
		}
	}

	return count;
}
#elif defined(J9ZOS390)
static int
count_threads(struct PlatformWalkData *data)
{
	/* TODO: find out why we need extra space in the output size */
	int output_size = sizeof(struct pgthb) + sizeof(struct pgthc) + 200;
	char *output_buffer = (char *)alloca(output_size);
	int input_size = sizeof(struct pgtha);
	int ret_val, ret_code, reason_code;
	unsigned int data_offset = 0;
	struct pgtha pgtha;
	struct pgtha *cursor = &pgtha;
	struct pgthc *pgthc;

	/* set up the getthent input area */
	memset(cursor, 0, sizeof(pgtha));
	pgtha.pid = getpid();
	pgtha.accesspid = PGTHA_ACCESS_CURRENT;
	pgtha.accessthid = PGTHA_ACCESS_FIRST;
	pgtha.flag1 = PGTHA_FLAG_PROCESS_DATA;

	/* get thread data and increment index */
	getthent(&input_size, &cursor, &output_size, (void **)&output_buffer, &ret_val, &ret_code, &reason_code);
	if (ret_val == -1) {
		/* failed to get thread context */
		return -1;
	}

	/* sanity check */
	if (__e2a_l(((struct pgthb *)output_buffer)->id, 4) != 4 || strncmp(((struct pgthb *)output_buffer)->id, "gthb", 4)) {
		return -2;
	}

	if (((struct pgthb *)output_buffer)->limitc != PGTH_OK) {
		/* we don't have the thread data */
		return -3;
	}

	data_offset = *(unsigned int *)((struct pgthb *)output_buffer)->offc;
	data_offset = data_offset >> 8;

	if (data_offset > ((struct pgthb *)output_buffer)->lenused || data_offset > output_size - sizeof(struct pgthc)) {
		/* the thread's data is past the end of the populated buffer */
		return -4;
	}

	pgthc = (struct pgthc *)((char *)output_buffer + data_offset);

	/* sanity check */
	if (__e2a_l(pgthc->id, 4) != 4 || strncmp(pgthc->id, "gthc", 4)) {
		return -5;
	}

	return pgthc->cntptcreated;
}
#endif


/* This function installs a signal handler then generates signals until all threads in the process bar the calling
 * thread are suspended. The signals have to be real-time signals (ie. >= SIGRTMIN) or it's not possible to pass
 * a data via sigqueue. Also, non real-time signals of the same value can be collapsed and we require any threads
 * scheduled during this suspension to immediately receive a signal, so we need at least one signal per unsuspended
 * thread on the queue.
 *
 * @param data platform specific control data used during suspend/resume
 * @return the number of threads suspended
 */
static int
suspend_all_preemptive(struct PlatformWalkData *data)
{
#if !defined(J9OS_I5)
	struct sigaction upcall_action;
	pid_t pid = getpid();
	int thread_count = 0;
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	struct OMRPortLibrary *portLibrary = data->state->portLibrary;
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	upcall_action.sa_sigaction = upcall_handler;
	upcall_action.sa_flags = SA_SIGINFO | SA_RESTART;
	sigemptyset(&upcall_action.sa_mask);
	Trc_PRT_introspect_suspendWithSignal(SUSPEND_SIG);
	sigaddset(&upcall_action.sa_mask, SUSPEND_SIG);

	/* install the signal handler */
	if (sigaction(SUSPEND_SIG, &upcall_action, &data->oldHandler) == -1) {
		/* no solution but to bail if we can't install the handler */
		RECORD_ERROR(data->state, SIGNAL_SETUP_ERROR, -1);
		return -1;
	}

	if (data->oldHandler.sa_sigaction == upcall_handler) {
		/* Handler's already installed so already iterating threads. We mustn't uninstall the handler
		 * while cleaning as the thread that installed the initial handler will do so
		 */
		memset(&data->oldHandler, 0, sizeof(struct sigaction));
		RECORD_ERROR(data->state, CONCURRENT_COLLECTION, -1);
		return -1;
	}

	/* after this point it's safe to go through the full cleaup */
	data->cleanupRequired = 1;

	thread_count = count_threads(data);
	if (thread_count < 1) {
		RECORD_ERROR(data->state, THREAD_COUNT_FAILURE, -2);
		return -2;
	}

	data->threadCount = 0;

	/* our main suspend loop. We keep going round this until the number of suspended threads matches the number of
	 * threads in the process
	 */
	do {
		int i = 0;
		sigval_t val;
		val.sival_ptr = data;

		/* fire off enough signals to pause all threads in the process barring us */
		for (i = data->threadCount; i < thread_count - 1; i++) {
			int ret = sigqueue(pid, SUSPEND_SIG, val);

			if (ret != 0) {
				if (errno == EAGAIN) {
					/* retry the queuing until we time out */
					int j;
					for (j = 0; ret != 0 && errno == EAGAIN && !timedOut(data->state->deadline1); j++) {
						omrthread_yield();
						ret = sigqueue(pid, SUSPEND_SIG, val);
					}
				}

				if (ret != 0) {
					/* failed to queue the signal, unrecoverable */
					data->threadsOutstanding = i;
					return -errno;
				}
			}

			/* allow the signals time to be dispatched so we don't overflow the signal queue. 10 is an arbitrary
			 * number to see if we can avoid the added complexity of coordination between the signal handler and
			 * this suspend logic. Testing shows 48 threads suspended on AIX5.3 at the point where we get EAGAIN.
			 */
			if ((i % 10) == 0) {
				omrthread_yield();
			}
		}

		data->threadCount = thread_count;
		thread_count = count_threads(data);
		if (thread_count < 1) {
			data->threadsOutstanding = data->threadCount - 1;
			return -4;
		}

		if (data->threadCount == thread_count) {
			/* nothing changed */
			break;
		} else if (data->threadCount > thread_count) {
			/* threads exited in between us counting and generating signals, so swallow the surplus */
			sigset_t set;
			int sig;

			for (i = 0; i < data->threadCount - thread_count; i++) {
				/* sanity check that there is a signal on the queue for us */
				sigpending(&set);
				if (sigismember(&set, SUSPEND_SIG)) {
					/* there is a suspend signal pending so swallow it */
					sigemptyset(&set);
					sigaddset(&set, SUSPEND_SIG);
#if defined(J9ZOS390)
					sigwait(&set);
#else
					sigwait(&set, &sig);
#endif
				}
			}

			data->threadCount = thread_count;
			break;
		}
	} while (!timedOut(data->state->deadline1));

	if (timedOut(data->state->deadline1)) {
		data->threadsOutstanding = data->threadCount - 1;
		return -5;
	}

	return data->threadCount - 1;

#else /* !defined(J9OS_I5) */
	struct __pthrdsinfo pinfo;
	int regbuf[64];
	int val = sizeof(regbuf);
	pthread_t thr = 0;
	int ret = 0;
	int thread_count = 0;
	pthread_t myThread = pthread_self();

	/* after this point it's safe to go through the full cleaup */
	data->cleanupRequired = 1;
	thread_count = count_threads(data);
	if (thread_count < 1) {
		RECORD_ERROR(data->state, THREAD_COUNT_FAILURE, -2);
		return -2;
	}

	data->threadCount = 0;
	do {
		int i = 0;
		do {
			ret = pthread_getthrds_np(&thr, PTHRDSINFO_QUERY_TID, &pinfo, sizeof(pinfo), regbuf, &val);
			if (ret != 0) {
				if (ret == ESRCH) {

				} else {
					data->threadsOutstanding = i;
					return -ret;
				}
			}
			if (thr == 0) {
				break;
			}
			if (thr == myThread) {
				continue;
			}
			ret = pthread_suspend_np(thr);
			if (ret != 0) {
#if 0
				printf("my thr %d,suspend thread return %d, ESRCH is %d,thr is %d\n", myThread, ret, ESRCH, thr);
#endif
				continue;
			}

			i++;
		} while (1);

		data->threadCount = thread_count;
		thread_count = count_threads(data);
		if (thread_count < 1) {
			data->threadsOutstanding = data->threadCount - 1;
			return -4;
		}

		if (data->threadCount == thread_count) {
			/* nothing changed */
			break;
		} else if (data->threadCount > thread_count) {
			/* threads exited in between us counting and suspend threads, so swallow the surplus */
			data->threadCount = thread_count;
			break;
		} else {
			/* threads created in between us counting and suspend threads, re-counting and suspend again */
		}

	} while (!timedOut(data->state->deadline1));

	if (timedOut(data->state->deadline1)) {
		data->threadsOutstanding = data->threadCount - 1;
		return -5;
	}

	return data->threadCount - 1;

#endif /* !defined(J9OS_I5) */
}


/*
 * Frees anything and everything to do with the scope of the specified thread.
 */
static void
freeThread(J9ThreadWalkState *state, J9PlatformThread *thread)
{
	J9PlatformStackFrame *frame;
	struct PlatformWalkData *data = (struct PlatformWalkData *)state->platform_data;

	if (thread == NULL) {
		return;
	}

	frame = thread->callstack;
	while (frame) {
		J9PlatformStackFrame *tmp = frame;
		frame = frame->parent_frame;

		if (tmp->symbol) {
			state->portLibrary->heap_free(state->portLibrary, state->heap, tmp->symbol);
			tmp->symbol = NULL;
		}

		state->portLibrary->heap_free(state->portLibrary, state->heap, tmp);
	}

	if (!data->platformAllocatedContext && thread->context) {
		state->portLibrary->heap_free(state->portLibrary, state->heap, thread->context);
	}

	state->portLibrary->heap_free(state->portLibrary, state->heap, thread);

	if (state->current_thread == thread) {
		state->current_thread = NULL;
	}
}


/*
 * This functions resumes all threads suspended by suspend_all_preemptive. The general behaviour
 * is to swallow any spurious signals from the queue so they are not delivered after we remove our
 * custom handler, the custom handler is removed, then we wake any threads blocking on the relase
 * barrier. On AIX we set the signal handler to SIG_IGN before attempting to swallow signals off
 * the queue because it seems to be possible for signals to be allocated to threads without yet
 * being delivered, preventing that from working. Setting SIG_IGN seems to remove not yet delivered
 * signals.
 *
 * @param data - platform specific control data used during suspend/resume
 */
static void
resume_all_preempted(struct PlatformWalkData *data)
{
#if !defined(J9OS_I5)
	sigset_t set;
	struct timespec time_out;
	J9ThreadWalkState *state = data->state;


	/* We skip everything but the semaphores and the process mask if we didn't sucessfully install the
	 * signal handlers.
	 */

	/* now there are no outstanding signals on the queue we can drop the handler */
#ifdef AIXPPC
	struct sigaction ign;

	if (data->cleanupRequired) {
		/* On AIX it seems that a signal can be unavailable to sigpending/sigwait but not yet have been delivered
		 * to a thread. As a result we can't be sure that SIG_SUSPEND won't be delivered after we uninstall the handler
		 * so we set it to ignore instead of default
		 */
		ign.sa_sigaction = NULL;
		ign.sa_handler = SIG_IGN;
		ign.sa_flags = 0;
		sigemptyset(&ign.sa_mask);

		/* we try this, nothing we can do if it doesn't work */
		sigaction(SUSPEND_SIG, &ign, NULL);
	}
#endif

	if (data->threadsOutstanding > 0) {
		/* inhibit collection of contexts from any of the outstanding threads we release */
		data->error = 1;
	}

	/* allow any outstanding threads to skip the handler. This will cause semwait to return with an error */
	close_wrapper(data->client_sem.descriptor_pair[0]);
	close_wrapper(data->client_sem.descriptor_pair[1]);

	if (data->cleanupRequired) {
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
		struct OMRPortLibrary *portLibrary = data->state->portLibrary;
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
		/* clear out the signal queue so that any undispatched suspend signals are not left lying around */
		while (sigpending(&set) == 0 && sigismember(&set, SUSPEND_SIG)) {
			/* there is a suspend signal pending so swallow it. It is worth noting that sigwait isn't in the initial
			 * set of posix signal safe functions, however the fact that it bypasses the installed signal handler is
			 * too useful it ignore, so we risk it. It could be replaced with sigsuspend and some handler juggling if
			 * need be but the added complexity isn't worth it until it's shown to be necessary
			 */
			sigemptyset(&set);
			sigaddset(&set, SUSPEND_SIG);
#if defined(J9ZOS390)
			sigwait(&set);
#else
			/* the pending signal may have been dispatched to another thread since we made the sigpending() call above,
			 * so use a non-blocking sigtimedwait() call to clear it if it's still there, don't actually wait
			 */
			time_out.tv_sec = 0;
			time_out.tv_nsec = 0;
			sigtimedwait(&set, NULL, &time_out);
#endif
		}

		/* restore the old signal handler */
		if ((data->oldHandler.sa_flags & SA_SIGINFO) == 0 && data->oldHandler.sa_handler == SIG_DFL) {
			/* if there wasn't an old signal handler the set this to ignore. There shouldn't be any suspend signals
			 * left but better safe than sorry
			 */
			data->oldHandler.sa_handler = SIG_IGN;
		}

		sigaction(SUSPEND_SIG, &data->oldHandler, NULL);

		/* release the threads from the upcall handler */
		barrier_release_r(&data->release_barrier, timeout(data->state->deadline2));
		/* make sure they've all exited. 1 means we block until all threads that have entered the barrier leave */
		barrier_destroy_r(&data->release_barrier, 1);
	}

	if (data->error) {
		/* allow threads in upcall handler to run */
		omrthread_yield();
	}
	/* clean up the semaphores */
	sem_destroy_r(&data->client_sem);
	sem_destroy_r(&data->controller_sem);

	if (data->state->current_thread != NULL) {
		freeThread(data->state, data->state->current_thread);
	}

	/* restore the mask for this thread */
	sigprocmask(SIG_SETMASK, &data->old_mask, NULL);

	/* clean up the heap */
	data->state->portLibrary->heap_free(data->state->portLibrary, data->state->heap, data);
	state->platform_data = NULL;

#else /* !defined(J9OS_I5) */

	sigset_t set;
	int sig;
	int i = 0;
	struct __pthrdsinfo pinfo;
	int regbuf[64];
	int val = sizeof(regbuf);
	pthread_t thr = 0;
	int ret = 0;
	J9ThreadWalkState *state = data->state;


	if (data->threadsOutstanding > 0) {
		/* inhibt collection of contexts from any of the outstanding threads we release */
		data->error = 1;
	}

	while (pthread_getthrds_np(&thr, PTHRDSINFO_QUERY_TID, &pinfo, sizeof(pinfo), regbuf, &val) == 0 && thr != 0) {
		ret = pthread_continue_np(thr);
#if 0
		if (ret) {
			return ;
		}
#endif
	}

	if (data->state->current_thread != NULL) {
		freeThread(data->state, data->state->current_thread);
	}

	/* clean up the heap */
	data->state->portLibrary->heap_free(data->state->portLibrary, data->state->heap, data);
	state->platform_data = NULL;

#endif /* !defined(J9OS_I5) */
}


/*
 * Sets up the native thread structures including the backtrace. If a context is specified it is used instead of grabbing
 * the context for the thread pointed to by state->current_thread.
 *
 * @param state - state variable for controlling the thread walk
 * @param sigContext - context to use in place of live thread context
 *
 * @return - 0 on success, non-zero otherwise.
 */
static int
setup_native_thread(J9ThreadWalkState *state, thread_context *sigContext, int heapAllocate)
{
	struct PlatformWalkData *data = (struct PlatformWalkData *)state->platform_data;
	int size = sizeof(thread_context);

	if (size < sizeof(ucontext_t)) {
		size = sizeof(ucontext_t);
	}

	if (heapAllocate || sigContext) {
		/* allocate the thread container*/
		state->current_thread = (J9PlatformThread *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformThread));
		if (state->current_thread == NULL) {
			return -1;
		}
		memset(state->current_thread, 0, sizeof(J9PlatformThread));

		/* allocate space for the copy of the context */
		state->current_thread->context = (thread_context *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, size);
		if (state->current_thread->context == NULL) {
			return -2;
		}
		memset(state->current_thread->context, 0, size);

		/* copy the thread specifics */
		state->current_thread->thread_id = data->thread->thread_id;
		state->current_thread->process_id = data->thread->process_id;
		state->current_thread->callstack = data->thread->callstack;

		/* copy the context */
		if (sigContext) {
			/* we're using the provided context instead of generating it */
			memcpy(state->current_thread->context, ((J9UnixSignalInfo *)sigContext)->platformSignalInfo.context, size);
		} else if (state->current_thread->thread_id == omrthread_get_ras_tid()) {
			/* return context for current thread */
			getcontext((ucontext_t *)state->current_thread->context);
		} else {
			memcpy(state->current_thread->context, (void *)data->thread->context, size);
		}
	} else {
		/* the data->thread object can be returned */
		state->current_thread = data->thread;
	}


	/* populate backtraces if not present */
	if (state->current_thread->callstack == NULL) {
		/* don't pass sigContext in here as we should have fixed up the thread already. It confuses heap/not heap allocations if we
		 * pass it here.
		 */
		SPECULATE_ERROR(state, FAULT_DURING_BACKTRACE, 2);
		state->portLibrary->introspect_backtrace_thread(state->portLibrary, state->current_thread, state->heap, NULL);
		CLEAR_ERROR(state);

#ifdef AIXPPC
		if (state->current_thread->callstack == NULL && data->uninitializedThreads) {
			/* if we encountered threads under construction while counting then this could be legitimate */
			char *message = "no stack frames available, the thread may not have finished initialization";
			char *symbol = (char *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, strlen(message) + 1);
			J9PlatformStackFrame *frame = (J9PlatformStackFrame *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformStackFrame));
			/* we have to copy the message into heap allocated memory because we can't differentiate constant strings and alloc'd strings
			 * when freeing thread data.
			 */
			strcpy(symbol, message);
			memset(frame, 0, sizeof(J9PlatformStackFrame));
			frame->symbol = symbol;

			state->current_thread->callstack = frame;
		}
#endif
	}

	if (state->current_thread->callstack && state->current_thread->callstack->symbol == NULL) {
		SPECULATE_ERROR(state, FAULT_DURING_BACKTRACE, 3);
		state->portLibrary->introspect_backtrace_symbols(state->portLibrary, state->current_thread, state->heap);
		CLEAR_ERROR(state);
	}

	if (state->current_thread->error != 0) {
		RECORD_ERROR(state, state->current_thread->error, 1);
	}

	return 0;
}


/*
 * Creates an iterator for the native threads present within the process, either all native threads
 * or a subset depending on platform (see platform specific documentation for detail).
 * This function will suspend all of the native threads that will be presented through the iterator
 * and they will remain suspended until this function or nextDo return NULL. The context for the
 * calling thread is always presented first.
 *
 * @param heap used to contain the thread context, the stack frame representations and symbol strings. No
 * 			memory is allocated outside of the heap provided. Must not be NULL.
 * @param state semi-opaque structure used as a cursor for iteration. Must not be NULL User visible fields
 *			are:
 * 			deadline - the system time in seconds at which to abort introspection and resume
 * 			error - numeric error description, 0 on success
 * 			error_detail - detail for the specific error
 * 			error_string - string description of error
 * @param signal_info a signal context to use. This will be used in place of the live context for the
 * 			calling thread.
 *
 * @return NULL if there is a problem suspending threads, gathering thread contexts or if the heap
 * is too small to contain even the context without stack trace. Errors are reported in the error,
 * and error_detail fields of the state structure. A brief textual description is in error_string.
 */
J9PlatformThread *
omrintrospect_threads_startDo_with_signal(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state, void *signal_info)
{
	int result = 0;
	struct PlatformWalkData *data;
	J9PlatformThread thread;
	sigset_t mask;
	int suspend_result = 0;
	int flag;

#ifdef ZOS64
	RECORD_ERROR(state, UNSUPPORT_PLATFORM, 0);
	return NULL;
#endif

	/* construct the walk state and thread structures */
	state->heap = heap;
	state->portLibrary = portLibrary;
	data = (struct PlatformWalkData *)portLibrary->heap_allocate(portLibrary, heap, sizeof(struct PlatformWalkData));
	state->platform_data = data;
	state->current_thread = NULL;

	if (!data) {
		RECORD_ERROR(state, ALLOCATION_FAILURE, 0);
		return NULL;
	}

	memset(data, 0, sizeof(struct PlatformWalkData));
	data->state = state;
	data->controllerThread = omrthread_get_ras_tid();

	memset(&thread, 0, sizeof(J9PlatformThread));

#if !defined(J9OS_I5)
	/* prevent this thread from receiving the suspend signal */
	sigemptyset(&mask);
	sigaddset(&mask, SUSPEND_SIG);
	if (sigprocmask(SIG_BLOCK, &mask, &data->old_mask) != 0) {
		/* can't risk it if we can't filter the suspend signal from this thread */
		RECORD_ERROR(state, SIGNAL_SETUP_ERROR, errno);
		return NULL;
	}

	/* ensure that we can tell which file handles need closing if part of the semaphore initialization fails */
	data->client_sem.descriptor_pair[0] = -1;
	data->client_sem.descriptor_pair[1] = -1;
	data->controller_sem.descriptor_pair[0] = -1;
	data->controller_sem.descriptor_pair[1] = -1;
	data->release_barrier.descriptor_pair[0] = -1;
	data->release_barrier.descriptor_pair[1] = -1;

	/* set up the semaphores */
	if ((sem_init_r(&data->client_sem, 0)) != 0 || (sem_init_r(&data->controller_sem, 0)) != 0) {
		RECORD_ERROR(state, INITIALIZATION_ERROR, errno);
		goto cleanup;
	}
	/* initialise client semaphore pipe to be non-blocking */
	flag = fcntl(data->client_sem.descriptor_pair[0], F_GETFL);
	fcntl(data->client_sem.descriptor_pair[0], F_SETFL, flag | O_NONBLOCK);

	barrier_init_r(&data->release_barrier, 0);
#ifdef AIXPPC
	/* On AIX, initialise semaphore pipes to be sync-on-write (O_DSYNC flag). Previously we used the fdatasync()
	 * call after writing to the pipe, but on AIX it is not legal to call fdatasync() inside a signal handler.
	 */
	flag = fcntl(data->client_sem.descriptor_pair[1], F_GETFL);
	fcntl(data->client_sem.descriptor_pair[1], F_SETFL, flag | O_DSYNC);
	flag = fcntl(data->controller_sem.descriptor_pair[1], F_GETFL);
	fcntl(data->controller_sem.descriptor_pair[1], F_SETFL, flag | O_DSYNC);
	flag = fcntl(data->release_barrier.descriptor_pair[1], F_GETFL);
	fcntl(data->release_barrier.descriptor_pair[1], F_SETFL, flag | O_DSYNC);
#endif
#endif /* !defined(J9OS_I5) */

	/* suspend all threads bar this one */
	suspend_result = suspend_all_preemptive(state->platform_data);
	if (suspend_result < 0) {
		RECORD_ERROR(state, SUSPEND_FAILURE, suspend_result);
#if !defined(J9OS_I5)
		/* update the barrier for the number of signals we actually managed to queue */
		barrier_update_r(&data->release_barrier, data->threadsOutstanding);
#endif /* !defined(J9OS_I5) */
		goto cleanup;
	}

	data->threadsOutstanding = suspend_result;
#if !defined(J9OS_I5)
	barrier_update_r(&data->release_barrier, data->threadsOutstanding);
#endif /* !defined(J9OS_I5) */
	/* we start at zero with no filter */
	data->filterThread = 0;
	data->thread = NULL;
#if defined(J9OS_I5)
	data->thr = 0;
#endif /* defined(J9OS_I5) */

	/* pass back the current thread as we can be sure we won't hit a problem getting it */
	thread.thread_id = omrthread_get_ras_tid();
	thread.process_id = getpid();
	data->thread = &thread;

	data->consistent = 1;

	result = setup_native_thread(state, signal_info, 1);
	if (0 != result) {
		RECORD_ERROR(state, ALLOCATION_FAILURE, result);
		goto cleanup;
	}

	return state->current_thread;

cleanup:
	resume_all_preempted(data);
	return NULL;
}

/* This function is identical to omrintrospect_threads_startDo_with_signal but omits the signal argument
 * and instead uses a live context for the calling thread.
 */
J9PlatformThread *
omrintrospect_threads_startDo(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state)
{
	return omrintrospect_threads_startDo_with_signal(portLibrary, heap, state, NULL);
}

/* This function is called repeatedly to get subsequent threads in the iteration. The only way to
 * resume suspended threads is to continue calling this function until it returns NULL.
 *
 * @param state state structure initialised by a call to one of the startDo functions.
 *
 * @return NULL if there is an error or if no more threads are available. Sets the error fields as
 * listed detailed for omrintrospect_threads_startDo_with_signal.
 */
J9PlatformThread *
omrintrospect_threads_nextDo(J9ThreadWalkState *state)
{
	struct PlatformWalkData *data = (struct PlatformWalkData *)state->platform_data;
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	struct OMRPortLibrary *portLibrary = state->portLibrary;
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	
	J9PlatformThread *thread = NULL;
	int result = 0;
#if !defined(J9OS_I5)
	sigset_t mask, old_mask;
#else /* !defined(J9OS_I5) */
	int regbuf[64];
	int val = sizeof(regbuf);
#endif /* !defined(J9OS_I5) */

	if (data == NULL) {
		/* state is invalid */
		RECORD_ERROR(state, INVALID_STATE, 0);
		return NULL;
	}

	if (!data->consistent) {
		/* state is invalid */
		RECORD_ERROR(state, INVALID_STATE, 1);
		goto cleanup;
	}

	data->consistent = 0;

#if !defined(J9OS_I5)
	/* it's possible someone has played with the signal mask, most likely siglongjmp if there was a segfault or
	 * similar, so ensure it matches our needs
	 */
	sigemptyset(&mask);
	sigaddset(&mask, SUSPEND_SIG);
	if (sigprocmask(SIG_BLOCK, &mask, &old_mask) != 0 && !sigismember(&old_mask, SUSPEND_SIG)) {
		/* can't risk it if we can't filter the suspend signal from this thread */
		RECORD_ERROR(state, SIGNAL_SETUP_ERROR, errno);
		return NULL;
	}
#endif /* !defined(J9OS_I5) */

	/* cleanup the previous threads */
	freeThread(state, state->current_thread);

#if !defined(J9OS_I5)
	if (data->threadsOutstanding <= 0) {
#else /* !defined(J9OS_I5) */
	if ((data->threadsOutstanding <= 0) || (data->threadsOutstanding != data->threadCount - 1) && (data->thr == 0)) {
#endif /* !defined(J9OS_I5) */
		/* we've finished processing threads */
		goto cleanup;
	}

	/* record that we've processed one of the suspended threads */
	data->threadsOutstanding--;

#if !defined(J9OS_I5)
	/* solicit the next thread context */
	result = sem_post_r(&data->client_sem);

	if (result == -1 || data->error) {
		/* failed to solicit thread context */
		RECORD_ERROR(state, COLLECTION_FAILURE, result == -1? -1 : data->error);
		goto cleanup;
	}

#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390)
	do {
		result = sem_trywait_r(&data->controller_sem);
		/* linux seems to have a scheduler bug whereby a process with all threads in system waits doesn't get scheduled
		 * even though there are signals pending and the threads aren't in TASK_UNINTERRUPTABLE. Requeuing the signals
		 * appears to prompt scheduling, we then take outstanding signals off the signal queue with sigwait when
		 * we're done as with other systems.
		 */
		if (result == -1) {
			sigval_t val;
			val.sival_ptr = data;

			sigqueue(getpid(), SUSPEND_SIG, val);
		}
	} while (result == -1 && !timedOut(data->state->deadline1) && !data->error);
#else
	result = sem_timedwait_r(&data->controller_sem, timeout(data->state->deadline1));
#endif

	if (result != 0 || data->error) {
		/* we've not received notification from a client thread */
		if (data->error) {
			RECORD_ERROR(state, COLLECTION_FAILURE, data->error);
		} else {
			RECORD_ERROR(state, TIMEOUT, result);
		}
		goto cleanup;
	}

#else /* !defined(J9OS_I5) */
	while (data->thr == pthread_self() || data->thr == 0) {
		if ((result = pthread_getthrds_np(&data->thr, PTHRDSINFO_QUERY_TID, &data->pinfo, sizeof(data->pinfo), regbuf, &val)) != 0) {
			RECORD_ERROR(state, COLLECTION_FAILURE, result);
			goto cleanup;
		}
	}

	if ((result = pthread_getthrds_np(&data->thr, PTHRDSINFO_QUERY_ALL, &data->pinfo, sizeof(data->pinfo), regbuf, &val)) != 0) {
		RECORD_ERROR(state, COLLECTION_FAILURE, result);
		goto cleanup;
	}


	{
		ucontext_t ut;
		ut.uc_mcontext.jmp_context.gpr[1] = data->pinfo.__pi_context.__pc_gpr[1];

		get_thread_Info(data, (void *)&ut, data->pinfo.__pi_tid);
	}

#endif /* !defined(J9OS_I5) */

	thread = data->thread;

	thread->process_id = getpid();

	data->consistent = 1;

	if (setup_native_thread(state, NULL, 0) != 0) {
		RECORD_ERROR(state, ALLOCATION_FAILURE, result);
		goto cleanup;
	}

	return state->current_thread;

cleanup:
	resume_all_preempted(data);
	return NULL;
}

int32_t
omrintrospect_set_suspend_signal_offset(struct OMRPortLibrary *portLibrary, int32_t signalOffset)
{
	int32_t result = OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	if ((signalOffset < 0)
		|| (signalOffset > (SIGRTMAX - SIGRTMIN))
#if defined(SIG_RI_INTERRUPT_INDEX)
		|| (SIG_RI_INTERRUPT_INDEX == signalOffset)
#endif  /* defined(SIG_RI_INTERRUPT_INDEX) */
	) {
		result = OMRPORT_ERROR_INVALID;
	} else {
		PPG_introspect_threadSuspendSignal = SIGRTMIN + signalOffset;
		result = 0;
	}
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	return result;
}

int32_t
omrintrospect_startup(struct OMRPortLibrary *portLibrary)
{
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	PPG_introspect_threadSuspendSignal = SIGRTMIN;
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	return 0;
}

void
omrintrospect_shutdown(struct OMRPortLibrary *portLibrary)
{
	return;
}
