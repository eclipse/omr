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

/**
 * @file
 * @ingroup Port
 * @brief process introspection support
 */
#include <pthread.h>
#include <ucontext.h>
#include "sys/sigcontext.h"
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

#include <dirent.h>
#define __USE_GNU 1
#include <dlfcn.h>
#include <sys/utsname.h>

#include <tpf/tpfapi.h>
#include <tpf/c_thgl.h>
#include <tpf/c_cinfc.h>

/* z/TPF doesn't have a fdatasync(), it takes   */
/*    one argument and returns void. Nullify it.*/
#define fdatasync(x)    {}
#include "omrintrospect.h"
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrsignal_context.h"
#include "omrutilbase.h"
#include "ut_omrport.h"

extern void loadfpc();

/*
 * This is the signal that we use to suspend the various threads. This signal must be not be collapsable and
 * must pass on values specified by sigqueue.
 */

#define SUSPEND_SIG SIGUSR1

/*
 * Internal semaphore retry timeouts. Some implementations of poll seem unreliable so we add a
 * timeout to poll calls whether or not we have a deadline so we have a sample based scheduler visible
 * spinlock.
 */
#define POLL_RETRY_INTERVAL -1

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
	/* the semaphore all suspended threads wait on */
	sem_t_r client_sem;
	/* the semaphore the controller thread waits on */
	sem_t_r controller_sem;
	/* barrier to prevent any threads exiting the introspect logic until all are ready */
	barrier_r release_barrier;
};

/* Wrapper for the close function that retries on EINTR. */
static int
close_wrapper(int fd)
{
	int rc = 0;
	if (fd != -1) {
		do {
			rc = close(fd);
		} while ((0 != rc) && (EINTR == errno));
	}

	return rc;
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
	} while (compareAndSwapUDATA((uintptr_t *)&barrier->initial_value, old_value, value) != old_value);
	do {
		old_value = barrier->in_count;
	} while (compareAndSwapUDATA((uintptr_t*)&barrier->in_count, old_value, value) != old_value);
	do {
		old_value = barrier->released;
	} while (compareAndSwapUDATA((uintptr_t*)&barrier->released, old_value, 0) != old_value);

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
	fds[0].events = POLLHUP|POLLERR|POLLNVAL|POLLIN;
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
	if ((ret == -1 && errno != EINTR) || fds[0].revents & (POLLHUP|POLLERR|POLLNVAL)) {
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

	if ((compareAndSwapUDATA((uintptr_t*)&barrier->released, 0, 1)) != 0) {
		/* barrier->released should have been 0, write something into the pipe so that poll doesn't block,
		 * converts this to a busy wait
		 */
		barrier->released = 1;
		write(barrier->descriptor_pair[1], &byte, 1);
	}

	/* wait until all entrants have arrived */
	while ((compareAndSwapUDATA((uintptr_t*)&barrier->in_count, -1, -1)) > 0) {
		if ((result = barrier_block_until_poked(barrier, deadline)) == -1) {
			/* timeout or error */
			break;
		}
	}

	write(barrier->descriptor_pair[1], &byte, 1);
	fdatasync(barrier->descriptor_pair[1]);
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
	} while (compareAndSwapUDATA((uintptr_t*)&barrier->in_count, old_value, old_value - 1) != old_value);

	if (old_value == 1 && (compareAndSwapUDATA((uintptr_t*)&barrier->released, 0, 0))) {
		/* we're the last through the barrier so wake everyone up */
		write(barrier->descriptor_pair[1], &byte, 1);
	}

	/* if we're entering a barrier with a negative count then count us out but we don't need to do anything */

	/* wait until we are formally released */
	while (compareAndSwapUDATA((uintptr_t*)&barrier->in_count, -1, -1) > 0 || !barrier->released) {
		if ((result = barrier_block_until_poked(barrier, deadline)) < 0) {
			/* timeout or error */
			break;
		}
	}

	/* increment the out count */
	do {
		old_value = barrier->out_count;
	} while (compareAndSwapUDATA((uintptr_t*)&barrier->out_count, old_value, old_value + 1) != old_value);
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
	} while (compareAndSwapUDATA((uintptr_t*)&barrier->in_count, old_value, old_value + difference) != old_value);

	if (old_value < 0 || (old_value == 0 && barrier->initial_value != 0)) {
		int restore_value = old_value;

		/* barrier was already exited, so undo update and return error */
		do {
			old_value = barrier->in_count;
		} while (compareAndSwapUDATA((uintptr_t*)&barrier->in_count, old_value, restore_value) != old_value);

		return -1;
	} else {
		/* we've updated the in_count so update the initial_value */
		do {
			old_value = barrier->initial_value;
		} while (compareAndSwapUDATA((uintptr_t*)&barrier->initial_value, old_value, new_value) != old_value);

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
	close_wrapper(barrier->descriptor_pair[1]);
	close_wrapper(barrier->descriptor_pair[0]);

	/* decrement the wait count */
	if (block) {
		do {
			current = compareAndSwapUDATA((uintptr_t*)&barrier->out_count, -1, -1);
			in = compareAndSwapUDATA((uintptr_t*)&barrier->in_count, -1, -1);
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
	fds[0].events = POLLHUP|POLLERR|POLLNVAL|POLLIN;
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
		old_value = compareAndSwapUDATA((uintptr_t*)&sem->sem_value, -1, -1);
		while (old_value > 0) {
			if (compareAndSwapUDATA((uintptr_t*)&sem->sem_value, old_value, old_value - 1) == old_value) {
				/* successfully acquired lock */
				return 0;
			}

			old_value = sem->sem_value;
		}

		/* wait for something to change */
		ret = poll(fds, 1, interval);
		if ((ret == -1 && errno != EINTR) || fds[0].revents & (POLLHUP|POLLERR|POLLNVAL)) {
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
	old_value = compareAndSwapUDATA((uintptr_t*)&sem->sem_value, -1, -1);
	while (old_value > 0) {
		int value = compareAndSwapUDATA((uintptr_t*)&sem->sem_value, old_value, old_value - 1);
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
	} while (compareAndSwapUDATA((uintptr_t*)&sem->sem_value, old_value, old_value + 1) != old_value);

	/* wake up a waiter */
	if (write(sem->descriptor_pair[1], &byte, 1) != 1) {
		return -1;
	}

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
	} while (compareAndSwapUDATA((uintptr_t*)&sem->sem_value, old_value, old_value - sem->initial_value) != old_value);

	if (old_value != sem->initial_value) {
		/* undo the block and return error */
		do {
			old_value = sem->sem_value;
		} while (compareAndSwapUDATA((uintptr_t*)&sem->sem_value, old_value, old_value + sem->initial_value) != old_value);

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
	thread_context *context = (thread_context*)context_arg;
	J9ThreadWalkState *state;
	struct PlatformWalkData *data = NULL;
	int ret = 0;
	pid_t pid = getpid();
	unsigned long tid = omrthread_get_ras_tid();

	/* check that this signal was queued by this process. */
	if (siginfo->si_code != SI_QUEUE || siginfo->si_value.sival_ptr == NULL) {
		/* these are not the signals you are looking for */
		return;
	}

	data = (struct PlatformWalkData*)siginfo->si_value.sival_ptr;
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
			memset(data->thread, 0, sizeof(J9PlatformThread));
			data->thread->thread_id = tid;
			data->platformAllocatedContext = 1;
			data->thread->context = context;
			state->portLibrary->introspect_backtrace_thread(state->portLibrary, data->thread, state->heap, NULL);
			state->portLibrary->introspect_backtrace_symbols(state->portLibrary, data->thread, state->heap);
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

/* These functions count the number of threads in the process. This information is necessary so we can queue the
 * correct number of signals to suspend all the threads (we might be able to simply call sigqueue until we get a
 * queue full error, but then we can't queue additional signals to prompt thread scheduling. Also it may not be
 * capped on all systems).
 *
 * @param data - the platform specific data, ignored
 * @return number of threads in the process
 */
static int
count_threads( struct PlatformWalkData *data ) {
	struct iproc		*pProc;
	struct ithgl		*pThgl;
	if( is_threaded_ecb() ) {
		pProc = iproc_process;
		pThgl = pProc->iproc_thread_global_data;
		return pThgl->active_cnt;
	}
	return 0;
}


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
	errno = EBADF;
	return -1;
}


/*
 * Frees anything and everything to do with the scope of the specified thread.
 */
static void
freeThread(J9ThreadWalkState *state, J9PlatformThread *thread)
{
	J9PlatformStackFrame *frame;
	struct PlatformWalkData *data = (struct PlatformWalkData*)state->platform_data;

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
	sigset_t set;
	int	clean_release = 0;
	struct timespec	time_out;
	J9ThreadWalkState *state = data->state;
	struct OMRPortLibrary *portLibrary = state->portLibrary;
	int	test_sig = SUSPEND_SIG;

	/*
	 * We skip everything but the semaphores and the process mask if we didn't
	 * successfully install the signal handlers. Now there are no outstanding
	 * signals on the queue we can drop the handler
	 */
	if (data->threadsOutstanding > 0) {
		/* inhibit collection of contexts from any of the outstanding threads we release */
		data->error = 1;
	}

	/* allow any outstanding threads to skip the handler. This will cause semwait to return with an error */
	close_wrapper(data->client_sem.descriptor_pair[0]);
	close_wrapper(data->client_sem.descriptor_pair[1]);

	if (data->cleanupRequired) {
		struct OMRPortLibrary *portLibrary = data->state->portLibrary;
		/* clear out the signal queue so that any undispatched suspend signals are not left lying around */
		while (sigpending(&set) == 0 && sigismember(&set, SUSPEND_SIG)) {
			/* there is a suspend signal pending so swallow it. It is worth noting that sigwait isn't in the initial
			 * set of posix signal safe functions, however the fact that it bypasses the installed signal handler is
			 * too useful it ignore, so we risk it. It could be replaced with sigsuspend and some handler juggling if
			 * need be but the added complexity isn't worth it until it's shown to be necessary
			 */
			sigemptyset(&set);
			sigaddset(&set, SUSPEND_SIG);
			sigwait(&set, &test_sig);
		}

		/* restore the old signal handler */
		if ((data->oldHandler.sa_flags & SA_SIGINFO) == 0 && data->oldHandler.sa_handler == SIG_DFL ) {
			/* if there wasn't an old signal handler the set this to ignore. There shouldn't be any suspend signals
			 * left but better safe than sorry
			 */
			data->oldHandler.sa_handler = SIG_IGN;
		}

		sigaction(SUSPEND_SIG, &data->oldHandler, NULL);

		/* release the threads from the upcall handler */
		clean_release = barrier_release_r(&data->release_barrier, timeout(data->state->deadline2));
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
}


int  __attribute__((optimize(0)))
J9ZTPF_getcontext( void *region )
{
        register char *rgn asm("r2") = (char *)region;
        /*
         *      Because of the way the prologue works, registers 1, 11, and 15 are
         *      clobbered; only 11 and 15 are recoverable ... 1 is lost forever.
         *      (( SEE NOTE ABOVE, please. -O3 also clobbers R4 permanently. ))
         *      This block needs to point at the *caller's* NSI, so R14's contents need
         *      to replace bytes 8-15 of the PSW, we'll do that too.
         */
        asm volatile (
                /*
                 *              This section writes the primary ucontext_t block fields the
                 *              user called for, using all 512 bytes of storage we were passed.
                 *              First we set uc_flags & uc_link to zero, as well as the entire
                 *              region occupied by uc_stack.
                 */
                "xc  0(40,%0),0(%0)\n\t"           /* Zeroize uc_flags & uc_link                        */
                /*
                 *              Next portion is the _sigregs (field: uc_mcontext) block
                 *              It starts at offset 0x0028, where the PSW goes. But we need to
                 *              save the caller's registers first thing, so do that first.
                 */
                "stmg %%r0,%%r10,56(%0)\n\t"   /* store grandes 0-10 at  +0x0038 for 0x58 bytes */
                "mvc 144(48,%0),544(%%r11)\n\t" /*              grandes 11-15 at +0x0090 for 0x28 bytes */
                "epsw %%r0,%%r1\n\t"               /* Get PSW into registers 0 & 1                                      */
                "stg  %%r0,40(%0)\n\t"             /* Store PSW hi word at +0x0030 for 0x08 bytes       */
                "lg   %%r0,168(%0)\n\t"            /* Pick up original R14, and                                         */
                "stg  %%r0,48(%r0)\n\t"            /*  replace IC part of PSW with it.                          */
                "stam %%a0,%%a15,184(%0)\n\t"  /* Store A0-15 at +0x00B8 for 0x40 bytes                 */
                "stfpc 248(%0)\n\t"                        /* Store FPC register at +0x00F8 for 4 bytes         */
                "xc  252(4,%0),252(%0)\n\t"    /* Zeroize 4 bytes' alignment padding (+0x0FC for 4)     */
                "std %%f0,256(%0)\n\t"             /* Store FPR0 at +0x0100 for 8 bytes                         */
                "std %%f1,264(%0)\n\t"             /* Store FPR1 at +0x0108 for 8 bytes                         */
                "std %%f2,272(%0)\n\t"             /* Store FPR2 at +0x0110                                                     */
                "std %%f3,280(%0)\n\t"             /* Store FPR3 at +0x0118                                                     */
                "std %%f4,288(%0)\n\t"             /*           FPR4 at +0x0120                                                 */
                "std %%f5,296(%0)\n\t"             /*           FPR5 at +0x0128                                                 */
                "std %%f6,304(%0)\n\t"             /*           FPR6 at +0x0130                                                 */
                "std %%f7,312(%0)\n\t"             /*           FPR7 at +0x0138                                                 */
                "std %%f8,320(%0)\n\t"             /*           FPR8 at +0x0140                                                 */
                "std %%f9,328(%0)\n\t"             /*           FPR9 at +0x0148                                                 */
                "std %%f10,336(%0)\n\t"            /*      FPR10 at +0x0150                                                     */
                "std %%f11,344(%0)\n\t"            /*      FPR11 at +0x0158                                                     */
                "std %%f12,352(%0)\n\t"            /*      FPR12 at +0x0160                                                     */
                "std %%f13,360(%0)\n\t"            /*      FPR13 at +0x0168                                                     */
                "std %%f14,368(%0)\n\t"            /*      FPR14 at +0x0170                                                     */
                "std %%f15,376(%0)\n\t"            /*      FPR15 at +0x0178                                                     */
                /*
                 *       Type sigset_t is next, field uc_sigmask at +0x0180 for 80,
                 *       which will come from sigprocmask(), following.
                 */
                : /* no outputs */                        /* output constraints would go here                           */
                : "a"(rgn)                                        /* input constraints go here                                          */
                : "%r0", "%r1", "%r4", "memory");         /* clobber list goes here                                                     */
        sigprocmask( SIG_BLOCK, NULL, &((ucontext_t *)region)->uc_sigmask );    /* Get sig masks        */
        /*
         *       Total lth = 0x200 (512) bytes. Send it back to the caller
         *       in the address we were passed. We only need to return an
         *       int for success (0) or non-zero for failure. The way this
         *       routine is written, failure is not possible.
         */
        return 0;
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
	struct PlatformWalkData *data = (struct PlatformWalkData*)state->platform_data;
	int size = sizeof(thread_context);

	if (size < sizeof(ucontext_t)) {
		size = sizeof(ucontext_t);
	}

	if (heapAllocate || sigContext) {
		/* allocate the thread container*/
		state->current_thread = (J9PlatformThread*)state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformThread));
		if (state->current_thread == NULL) {
			return -1;
		}
		memset(state->current_thread, 0, sizeof(J9PlatformThread));

		/* allocate space for the copy of the context */
		state->current_thread->context = (thread_context*)state->portLibrary->heap_allocate(state->portLibrary, state->heap, size);
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
			memcpy(state->current_thread->context, ((OMRUnixSignalInfo*)sigContext)->platformSignalInfo.context, size);
		} else if (state->current_thread->thread_id == omrthread_get_ras_tid()) {
			/* return context for current thread */
			J9ZTPF_getcontext((ucontext_t*)state->current_thread->context);
		} else {
			memcpy(state->current_thread->context, (void*)data->thread->context, size);
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
 * On some systems, sigqueue() doesn't reliably lead to receipt of the expected
 * number of signals: On those platforms, the controller must not use sem_timedwait_r
 * otherwise the process may hang waiting for threads which were assumed to have
 * received the SUSPEND_SIG signal but did not. z/TPF doesn't have kernel-space
 * signals and therefore there are _no_ reliable signals in the system at all:
 * always return 0.
 */
static uintptr_t
sigqueue_is_reliable(void)
{
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
J9PlatformThread*
omrintrospect_threads_startDo_with_signal(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state, void *signal_info)
{
	int result = 0;
	struct PlatformWalkData *data;
	J9PlatformThread thread;
	sigset_t mask;
	int suspend_result = 0;
	int flag;

/*
 *	The following preprocessor guard will stop all attempts at attempting to walk the
 *	native thread stacks, thus avoiding some pretty sticky code which may or may not
 *	work on z/TPF. We'll leave this gate open for now, with a pending decision to close
 *	it or not.
 */

	RECORD_ERROR(state, UNSUPPORTED_PLATFORM, 0);
	return NULL;

}

/* This function is identical to omrintrospect_threads_startDo_with_signal but omits the signal argument
 * and instead uses a live context for the calling thread.
 */
J9PlatformThread*
omrintrospect_threads_startDo(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state)
{
	return omrintrospect_threads_startDo_with_signal(portLibrary, heap, state, NULL);
}

/* This function is called repeatedly to get subsequent threads in the iteration. The only way to
 * resume suspended threads is to continue calling this function until it returns NULL.
 *
 * @param state state structure initialized by a call to one of the startDo functions.
 *
 * @return NULL if there is an error or if no more threads are available. Sets the error fields as
 * listed detailed for omrintrospect_threads_startDo_with_signal.
 */
J9PlatformThread*
omrintrospect_threads_nextDo(J9ThreadWalkState *state)
{
	struct PlatformWalkData *data = (struct PlatformWalkData*)state->platform_data;
	struct OMRPortLibrary *portLibrary = state->portLibrary;
	J9PlatformThread *thread = NULL;
	int result = 0;
	sigset_t mask, old_mask;

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

	/* cleanup the previous threads */
	freeThread(state, state->current_thread);

	if (data->threadsOutstanding <= 0) {
		/* we've finished processing threads */
		goto cleanup;
	}

	/* record that we've processed one of the suspended threads */
	data->threadsOutstanding--;

	/* solicit the next thread context */
	result = sem_post_r(&data->client_sem);

	if (result == -1 || data->error) {
		/* failed to solicit thread context */
		RECORD_ERROR(state, COLLECTION_FAILURE, result==-1 ? -1:data->error);
		goto cleanup;
	}

	if (sigqueue_is_reliable()) {
		result = sem_timedwait_r(&data->controller_sem, timeout(data->state->deadline1));
	} else {
		for (;;) {
			result = sem_trywait_r(&data->controller_sem);
			if (0 == result) {
				break;
			} else if (timedOut(data->state->deadline1) || data->error) {
				break;
			} else {
				union sigval val;
				val.sival_ptr = data;

				/*
				 * Because sigqueue is not reliable, we attempt to queue more signals in
				 * the hopes that it will cause one of the remaining threads to respond.
				 */
				sigqueue(getpid(), SUSPEND_SIG, val);
				omrthread_yield();
			}
		}
	}

	if (result != 0 || data->error) {
		/* we've not received notification from a client thread */
		if (data->error) {
			RECORD_ERROR(state, COLLECTION_FAILURE, data->error);
		} else {
			RECORD_ERROR(state, TIMEOUT, result);
		}
		goto cleanup;
	}

	thread = data->thread;
	thread->process_id = getpid();
	data->consistent = 1;

	result = setup_native_thread(state, NULL, 0);
	if (0 != result) {
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
#if defined(J9_CONFIGURABLE_SUSPEND_SIGNAL)
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
#endif /* defined(J9_CONFIGURABLE_SUSPEND_SIGNAL) */
	return result;

}

int32_t
omrintrospect_startup(struct OMRPortLibrary *portLibrary)
{
        loadfpc();

#if defined(J9_CONFIGURABLE_SUSPEND_SIGNAL)
	PPG_introspect_threadSuspendSignal = SIGRTMIN;
#endif /* defined(J9_CONFIGURABLE_SUSPEND_SIGNAL) */
	return 0;
}

void
omrintrospect_shutdown(struct OMRPortLibrary *portLibrary)
{
	return;
}
