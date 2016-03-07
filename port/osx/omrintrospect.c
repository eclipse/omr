/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
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

#include <mach/mach.h>
#include <pthread.h>
#define _XOPEN_SOURCE
#include <ucontext.h>

#include "omrintrospect.h"
#include "omrintrospect_common.h"
#include "omrport.h"
#include "omrsignal_context.h"

#define SUSPEND_SIG SIGUSR1

typedef struct PlatformWalkData {
	/* Thread to filter out of the platform iterator */
	thread_port_t filterThread;
	/* Array of mach port thread identifiers */
	thread_act_port_array_t threadList;
	/* Total number of threads in the process, including calling thread */
	long threadCount;
	/* Suspended threads unharvested */
	int threadIndex;
	/* Old signal handler for the suspend signal */
	struct sigaction oldHandler;
	/* Records whether we need to clean up in resume */
	unsigned char cleanupRequired;
	/* Backpointer to encapsulating state */
	J9ThreadWalkState *state;
} PlatformWalkData;

static void *signalHandlerContext;
static int pipeFileDescriptor[2];

static void freeThread(J9ThreadWalkState *state, J9PlatformThread *thread);
static int getThreadContext(J9ThreadWalkState *state);
static void resumeAllPreempted(PlatformWalkData *data);
static int setupNativeThread(J9ThreadWalkState *state, thread_context *sigContext);
static int suspendAllPreemptive(PlatformWalkData *data);
static void upcallHandler(int signal, siginfo_t *siginfo, void *context);

static void
resumeAllPreempted(PlatformWalkData *data)
{
	if (data->cleanupRequired) {
		/* Restore the old signal handler. */
		if ((0 == (SA_SIGINFO & data->oldHandler.sa_flags)) && (SIG_DFL == data->oldHandler.sa_handler)) {
			/* If there wasn't an old signal handler then set this to ignore. There shouldn't be any signals,
			 * but better safe than sorry.
			 */
			data->oldHandler.sa_handler = SIG_IGN;
		}

		sigaction(SUSPEND_SIG, &data->oldHandler, NULL);
	}

	/* Resume each thread. */
	for (; data->threadIndex < data->threadCount; data->threadIndex += 1) {
		if (data->filterThread != data->threadList[data->threadIndex]) {
			thread_resume(data->threadList[data->threadIndex]);
		}
	}

}

/* Store the context and send an arbitrary byte to indicate completion.
 * Pipes are async-signal-safe. signalHandlerContext should be as well
 * since it is only used carefully after waiting on the pipe. Receiving
 * unexpected SUSPEND_SIG signals while iterating threads would cause
 * any implementation to misbehave.
 */
static void
upcallHandler(int signal, siginfo_t *siginfo, void *context)
{
	char *data = "A";

	signalHandlerContext = context;
	write(pipeFileDescriptor[1], data, 1);
}

static int
suspendAllPreemptive(PlatformWalkData *data)
{
	mach_msg_type_number_t threadCount = 0;
	data->threadCount = 0;
	mach_port_t task = mach_task_self();
	struct sigaction upcallAction;
	int rc = 0;

	/* Install a signal handler to get thread context info from the handler. */
	upcallAction.sa_sigaction = upcallHandler;
	upcallAction.sa_flags = SA_SIGINFO | SA_RESTART;

	if (-1 == sigaction(SUSPEND_SIG, &upcallAction, &data->oldHandler)) {
		RECORD_ERROR(data->state, SIGNAL_SETUP_ERROR, -1);
		rc = -1;
	} else if (data->oldHandler.sa_sigaction == upcallHandler) {
		/* Handler's already installed so already iterating threads. We mustn't uninstall the handler
		 * while cleaning as the thread that installed the initial handler will do so.
		 */
		memset(&data->oldHandler, 0, sizeof(struct sigaction));
		RECORD_ERROR(data->state, CONCURRENT_COLLECTION, -1);
		rc = -1;
	}

	if (0 == rc) {
		/* After this point it's safe to go through the full cleanup. */
		data->cleanupRequired = 1;

		/* Suspend all threads until there are no new threads. */
		do {
			int i = 0;

			/* Get a list of the threads within the process. */
			data->threadCount = threadCount;
			if (KERN_SUCCESS != task_threads(task, &data->threadList, &threadCount)) {
				RECORD_ERROR(data->state, SIGNAL_SETUP_ERROR, -1);
				rc = -1;
				break;
			}

			/* Suspend each thread except this one. */
			for (i = data->threadCount; i < threadCount; i += 1) {
				if (data->filterThread != data->threadList[i]) {
					if (KERN_SUCCESS != thread_suspend(data->threadList[i])) {
						data->threadCount = i;
						rc = -1;
						break;
					}
				}
			}
		} while ((threadCount > data->threadCount) && (0 == rc));
	}

	return rc;
}

/*
 * Frees anything and everything to do with the scope of the specified thread.
 */
static void
freeThread(J9ThreadWalkState *state, J9PlatformThread *thread)
{
	J9PlatformStackFrame *frame = NULL;

	if (NULL == thread) {
		return;
	}

	frame = thread->callstack;
	while (NULL != frame) {
		J9PlatformStackFrame *tmp = frame;
		frame = frame->parent_frame;

		if (NULL != tmp->symbol) {
			state->portLibrary->heap_free(state->portLibrary, state->heap, tmp->symbol);
			tmp->symbol = NULL;
		}

		state->portLibrary->heap_free(state->portLibrary, state->heap, tmp);
	}

	if (NULL != thread->context) {
		state->portLibrary->heap_free(state->portLibrary, state->heap, thread->context);
	}

	state->portLibrary->heap_free(state->portLibrary, state->heap, thread);

	if (state->current_thread == thread) {
		state->current_thread = NULL;
	}
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
setupNativeThread(J9ThreadWalkState *state, thread_context *sigContext)
{
	PlatformWalkData *data = (PlatformWalkData *)state->platform_data;
	int size = sizeof(thread_context);
	int rc = 0;

	if (size < sizeof(ucontext_t)) {
		size = sizeof(ucontext_t);
	}

	/* Allocate the thread container. */
	state->current_thread = (J9PlatformThread *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformThread));
	if (NULL == state->current_thread) {
		rc = -1;
	}
	if (0 == rc) {
		memset(state->current_thread, 0, sizeof(J9PlatformThread));

		/* Allocate space for the copy of the context. */
		state->current_thread->context = (thread_context *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, size);
		if (NULL == state->current_thread->context) {
			rc = -1;
		}
	}

	if (0 == rc) {
		state->current_thread->thread_id = data->threadList[data->threadIndex];
		state->current_thread->process_id = getpid();

		if (NULL == sigContext) {
			/* Generate the context by installing a signal handler and raising a signal. None was provided. */
			if (0 == getThreadContext(state)) {
				memcpy(state->current_thread->context, signalHandlerContext, size);
			} else {
				rc = -1;
			}
		} else {
			/* Copy the context. We're using the provided context instead of generating it. */
			memcpy(state->current_thread->context, ((J9UnixSignalInfo *)sigContext)->platformSignalInfo.context, size);
		}
	}

	if (0 == rc) {
		/* Populate backtraces if not present. */
		if (NULL == state->current_thread->callstack) {
			/* Don't pass sigContext in here as we should have fixed up the thread already. It confuses heap/not heap allocations if we
			 * pass it here.
			 */
			SPECULATE_ERROR(state, FAULT_DURING_BACKTRACE, 2);
			state->portLibrary->introspect_backtrace_thread(state->portLibrary, state->current_thread, state->heap, NULL);
			CLEAR_ERROR(state);
		}

		if (NULL == state->current_thread->callstack && state->current_thread->callstack->symbol) {
			SPECULATE_ERROR(state, FAULT_DURING_BACKTRACE, 3);
			state->portLibrary->introspect_backtrace_symbols(state->portLibrary, state->current_thread, state->heap);
			CLEAR_ERROR(state);
		}

		if (0 != state->current_thread->error) {
			RECORD_ERROR(state, state->current_thread->error, 1);
		}
	}

	return rc;
}

static int
getThreadContext(J9ThreadWalkState *state)
{
	int ret = 0;
	char buffer[1];
	PlatformWalkData *data = (PlatformWalkData *)state->platform_data;
	thread_port_t thread = data->threadList[data->threadIndex];

	/* Create a pipe to allow the resumed thread to report it has completed
	 * sending its context info.
	 */
	ret = pipe(pipeFileDescriptor);

	if (0 == ret) {
		ret = pthread_kill(pthread_from_mach_thread_np(thread), SUSPEND_SIG);
	}

	if (0 == ret) {
		/* Resume the thread, with the signal pending. */
		if (KERN_SUCCESS != thread_resume(thread)) {
			ret = -1;
		}
	}

	if (0 == ret) {
		/* Wait for the signal handler to complete. */
		if (0 == read(pipeFileDescriptor[0], buffer, 1)) {
			ret = -1;
		}
	}

	return ret;
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
	PlatformWalkData *data;
	int suspend_result = 0;

	/* Construct the walk state. */
	state->heap = heap;
	state->portLibrary = portLibrary;
	data = (PlatformWalkData *)portLibrary->heap_allocate(portLibrary, heap, sizeof(PlatformWalkData));
	state->platform_data = data;
	state->current_thread = NULL;

	if (NULL == data) {
		RECORD_ERROR(state, ALLOCATION_FAILURE, 0);
		return NULL;
	}

	memset(data, 0, sizeof(PlatformWalkData));
	data->state = state;
	data->filterThread = mach_thread_self();

	/* Suspend all threads bar this one. */
	if (0 != suspendAllPreemptive(state->platform_data)) {
		RECORD_ERROR(state, SUSPEND_FAILURE, suspend_result);
		goto cleanup;
	}

	result = setupNativeThread(state, signal_info);
	if (0 != result) {
		RECORD_ERROR(state, COLLECTION_FAILURE, result);
		goto cleanup;
	}

	return state->current_thread;

cleanup:
	resumeAllPreempted(data);
	return NULL;
}

/* This function is identical to j9introspect_threads_startDo_with_signal but omits the signal argument
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
 * listed detailed for j9introspect_threads_startDo_with_signal.
 */
J9PlatformThread *
omrintrospect_threads_nextDo(J9ThreadWalkState *state)
{
	int result = 0;
	PlatformWalkData *data = state->platform_data;

	if (NULL == data) {
		/* state is invalid */
		RECORD_ERROR(state, INVALID_STATE, 0);
		return NULL;
	}

	/* Cleanup the previous threads. */
	freeThread(state, state->current_thread);

	data->threadIndex += 1;
	if (data->filterThread == data->threadList[data->threadIndex]) {
		data->threadIndex += 1;
	}

	if (data->threadIndex == data->threadCount) {
		/* Finished processing threads. */
		return NULL;
	}

	result = setupNativeThread(state, NULL);
	if (0 != result) {
		RECORD_ERROR(state, COLLECTION_FAILURE, result);
		goto cleanup;
	}

	return state->current_thread;

cleanup:
	resumeAllPreempted(data);
	return NULL;
}

int32_t
omrintrospect_set_suspend_signal_offset(struct OMRPortLibrary *portLibrary, int32_t signalOffset)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrintrospect_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

void
omrintrospect_shutdown(struct OMRPortLibrary *portLibrary)
{
	return;
}
