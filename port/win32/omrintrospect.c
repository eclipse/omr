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

#define UDATA UDATA_win32_
#include <windows.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <DbgHelp.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#include <stdio.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrutil.h"
#include "omrsignal.h"
#include "omrintrospect.h"
#include "omrintrospect_common.h"


struct PlatformWalkData {
	/* system iterator */
	THREADENTRY32 thread32;
	HANDLE snapshot;
	/* if set this will filter the specified thread out of the system iterator, for use when signal context is specified */
	DWORD filterThread;
	/* records whether the system iterator has been invoked yet */
	BOOL walkStarted;
};


/*
 * Conceptually this function resumes all the threads we suspend with suspend_all_preemtive.
 * In actuality it decrements the suspend count of every thread in the process by 1 even if,
 * somehow, we've not incremented it in suspend_all_preemptive (externally spawned threads by
 * debuggers for example).
 */
int
resume_all_preempted(struct PlatformWalkData *data)
{
	int resumed = 0;
	DWORD current_thread_id = GetCurrentThreadId();
	DWORD current_pid = GetCurrentProcessId();
	int result = 0;

	if (!data || data->snapshot == INVALID_HANDLE_VALUE) {
		return -1;
	}

	/* required or the thread iterator functions fail */
	data->thread32.dwSize = sizeof(THREADENTRY32);

	if (!Thread32First(data->snapshot, &data->thread32)) {
		result = GetLastError();
		return -1;
	}

	do {
		DWORD thread_id = data->thread32.th32ThreadID;
		HANDLE thread_handle;

		if (data->thread32.th32OwnerProcessID != current_pid || thread_id == current_thread_id) {
			continue;
		}

		thread_handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, thread_id);

		if (thread_handle == NULL) {
			result = GetLastError();
			continue;
		}

		while ((result = ResumeThread(thread_handle)) > 1);
		if (result == -1) {
			result = GetLastError();
		} else {
			resumed++;
		}

		CloseHandle(thread_handle);
	} while (Thread32Next(data->snapshot, &data->thread32));

	return resumed;
}

/*
 * This function takes a system snapshot, walks all the threads for this process and suspends all
 * of our threads barring the one executing the function. It then takes a new snapshot to determine
 * if new threads were created during the suspension. This is repeated until the thread count matches
 * between the two snapshots.
 */
int
suspend_all_preemptive(struct PlatformWalkData *data)
{
	int a_count = 0;
	int b_count = 0;
	char passA = 1;
	int result;
	DWORD current_thread_id = GetCurrentThreadId();
	DWORD current_pid = GetCurrentProcessId();


	/* required or the thread iterator functions fail */
	data->thread32.dwSize = sizeof(THREADENTRY32);

	/* primary processing loop */
	do {
		data->snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

		if (data->snapshot == INVALID_HANDLE_VALUE) {
			return -1;
		}

		if (!Thread32First(data->snapshot, &data->thread32)) {
			result = GetLastError();
			goto error_return;
		}

		do {
			DWORD thread_id = data->thread32.th32ThreadID;
			HANDLE thread_handle;

			if (data->thread32.th32OwnerProcessID != current_pid || thread_id == current_thread_id) {
				continue;
			}


			if (passA) {
				a_count++;
			} else {
				b_count++;
			}

			/* not the current thread and belongs to this process */
			thread_handle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread_id);

			if (thread_handle == NULL) {
				result = GetLastError();
				CloseHandle(thread_handle);
				/* failed to get the thread handle */
				goto error_return;
			}

			if (SuspendThread(thread_handle) == -1) {
				result = GetLastError();
				CloseHandle(thread_handle);
				/* failed to get the context */
				goto error_return;
			}
		} while (Thread32Next(data->snapshot, &data->thread32));

		/* next pass */
		passA = !passA;
	} while (a_count != b_count);

	if (a_count != b_count) {
		goto error_return;
	}

	return a_count;


error_return:
	/* we don't close the snapshot so that it's still available to resume any threads we suspended */
	SetLastError(result);

	return -1;
}

/*
 * Frees anything and everything to do within the scope of the specified thread.
 */
void
freeThread(J9ThreadWalkState *state, J9PlatformThread *thread)
{
	J9PlatformStackFrame *frame;

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

	if (thread->context) {
		state->portLibrary->heap_free(state->portLibrary, state->heap, thread->context);
	}

	state->portLibrary->heap_free(state->portLibrary, state->heap, thread);

	if (state->current_thread == thread) {
		state->current_thread = NULL;
	}
}


/*
 * Cleans up all state that we've created as part of the walk, including freeing up
 * the last returned thread, the debug library symbols, etc.
 */
void
cleanup(J9ThreadWalkState *state)
{
	uint8_t result = 0;
	struct PlatformWalkData *data;

	if (state) {
		data = state->platform_data;
		if (data) {
			if (data->snapshot && data->snapshot != INVALID_HANDLE_VALUE) {
				if (resume_all_preempted(data) == -1) {
					result = -1;
				}

				if (result != -1 || (HANDLE) GetLastError() != INVALID_HANDLE_VALUE) {
					/* it seems this can raise an exception if the handle has become invalid */
					CloseHandle(data->snapshot);
				}

				data->snapshot = NULL;
			}

			/* clean up the heap */
			state->portLibrary->heap_free(state->portLibrary, state->heap, data);
			state->platform_data = NULL;
		}

		if (state->current_thread) {
			freeThread(state, state->current_thread);
		}

		free_dbg_symbols(state->portLibrary);
	}

	return;
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
int
setup_native_thread(J9ThreadWalkState *state, CONTEXT *sigContext)
{
	CONTEXT tmpContext;
	int result = 0;

	/* allocate our various data structures */
	state->current_thread = (J9PlatformThread *)state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(J9PlatformThread));
	if (state->current_thread == NULL) {
		return -1;
	}
	memset(state->current_thread, 0, sizeof(J9PlatformThread));

	state->current_thread->context = (PCONTEXT)state->portLibrary->heap_allocate(state->portLibrary, state->heap, sizeof(CONTEXT));
	if (state->current_thread->context == NULL) {
		return -2;
	}
	memset(state->current_thread->context, 0, sizeof(CONTEXT));

	state->current_thread->thread_id = ((struct PlatformWalkData *)state->platform_data)->thread32.th32ThreadID;
	state->current_thread->process_id = ((struct PlatformWalkData *)state->platform_data)->thread32.th32OwnerProcessID;

	tmpContext.ContextFlags = CONTEXT_FULL;

	if (sigContext) {
		/* we're using the provided context instead of generating it */
		memcpy(&tmpContext, sigContext, sizeof(CONTEXT));
	} else if (state->current_thread->thread_id == GetCurrentThreadId()) {
		/* return context for current thread */

		/*
		 * NOTE: According to MSDN this call to GetThreadContext() should not work for the
		 * current thread. It seems to work, however, if we populate the EIP and EBP context
		 * data manually (which is what the assembler does below).
		 */
#ifdef WIN64
		RtlCaptureContext(&tmpContext);
#else
		if (GetThreadContext(GetCurrentThread(), &tmpContext) == FALSE) {
			return -3;
		}
		/* Manually obtain the EIP & EBP registers. We do this because the
		 * Windows GetThreadContext() function used above often fails to correctly
		 * identify the current location in the code. Together, these two registers
		 * indicate where we are. EIP is the instruction pointer and EBP is the
		 * stack frame.
		 *
		 * Explanation of what the assembler does:
		 *
		 * Create an assembler routine called 'getCtx' and call it. The call will
		 * push the current instruction pointer onto the stack.
		 *
		 * Function 'getCtx' then pops the top value from the stack into register
		 * eax. This will be the instruction pointer as at the point of the
		 * 'call' instruction.
		 *
		 * We can assume the stack frame (held in register ebp) will not change
		 * during this function.
		 *
		 * We now have (roughly) the correct instruction pointer in register eax
		 * and the correct stack frame in register ebp.
		 */
		__asm {
			/* Invoke the routine 'getCtx' */
			call getCtx;
			/* Define a routine called 'getCtx' */
			getCtx:
			/* Retrieve the previous instruction pointer from the stack and copy into register eax */
			pop eax;
			/* Copy the contents of eax into the instruction and ebp into  */
			mov tmpContext.Eip, eax;
			/* Copy the contents of ebp into the stack frame part of the context */
			mov tmpContext.Ebp, ebp;
		}
#endif
	} else {
		/* generate context for target thread */
		HANDLE thread_handle = OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION, FALSE, (DWORD)state->current_thread->thread_id);
		if (thread_handle == NULL) {
			return -4;
		}
		if (!GetThreadContext(thread_handle, &tmpContext)) {
			result = GetLastError();
			CloseHandle(thread_handle);
			SetLastError(result);
			/* failed to get the context */
			return -5;
		}
		CloseHandle(thread_handle);
	}

	memcpy(state->current_thread->context, &tmpContext, sizeof(CONTEXT));

	/* assemble the callstack */
	state->portLibrary->introspect_backtrace_thread(state->portLibrary, state->current_thread, state->heap, NULL);
	state->portLibrary->introspect_backtrace_symbols(state->portLibrary, state->current_thread, state->heap);

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
	struct J9Win32SignalInfo *sigFaultInfo = signal_info;
	DWORD processId = GetCurrentProcessId();
	struct PlatformWalkData *data;
	int result = 0;
	CONTEXT *signalContext = NULL;
	state->heap = heap;
	state->portLibrary = portLibrary;
	state->current_thread = NULL;

	/* likely can't operate in stack overflow conditions so inhibit construction of the backtrace */
	if (sigFaultInfo != NULL) {
		if (sigFaultInfo->ExceptionRecord != NULL && sigFaultInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
			RECORD_ERROR(state, INVALID_STATE, -1);
			return NULL;
		}
		signalContext = sigFaultInfo->ContextRecord;
	}

	data = (struct PlatformWalkData *)portLibrary->heap_allocate(portLibrary, heap, sizeof(struct PlatformWalkData));
	state->platform_data = data;

	if (!data) {
		/* can't allocate space for platform data */
		RECORD_ERROR(state, ALLOCATION_FAILURE, 0);
		return NULL;
	}

	data->walkStarted = FALSE;

	/* suspend everything but this thread */
	if (suspend_all_preemptive(data) == -1) {
		result = GetLastError();
		RECORD_ERROR(state, SUSPEND_FAILURE, result);
		goto cleanup;
	}

	/* required or the thread iterator functions fail */
	data->thread32.dwSize = sizeof(THREADENTRY32);
	/* grab the current thread and filter it for the iteration */
	data->thread32.th32ThreadID = GetCurrentThreadId();
	data->thread32.th32OwnerProcessID = processId;
	data->filterThread = data->thread32.th32ThreadID;

	/* make sure the dbg library functions and symbols are loaded and initialized */
	if (0 != load_dbg_functions(portLibrary) || 0 != load_dbg_symbols(portLibrary)) {
		result = GetLastError();
		RECORD_ERROR(state, INITIALIZATION_ERROR, -2);
		goto cleanup;
	}

	if (result = setup_native_thread(state, signalContext) != 0) {
		RECORD_ERROR(state, COLLECTION_FAILURE, result);
		goto cleanup;
	}

	return state->current_thread;

cleanup:
	cleanup(state);
	SetLastError(result);
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
	int result = 0;
	struct PlatformWalkData *data = state->platform_data;
	J9PlatformThread *nativeThread = NULL;
	int rc = 0;
	DWORD processId = GetCurrentProcessId();
	DWORD currentThreadId = GetCurrentThreadId();

	if (data == NULL) {
		/* state is invalid */
		RECORD_ERROR(state, INVALID_STATE, 0);
		return NULL;
	}

	/* cleanup the previous threads */
	freeThread(state, state->current_thread);

	/* initialize the thread walk if not already done */
	if (data->walkStarted == FALSE) {
		result = Thread32First(data->snapshot, &data->thread32);
		data->walkStarted = TRUE;
	} else {
		result = Thread32Next(data->snapshot, &data->thread32);
	}

	if (result == FALSE) {
		result = -1;
		goto cleanup;
	}

	/* get the next thread in this process */
	while (data->thread32.th32OwnerProcessID != processId || data->thread32.th32ThreadID == data->filterThread) {
		if (Thread32Next(data->snapshot, &data->thread32) == FALSE) {
			goto cleanup;
		}
	}

	if (result = setup_native_thread(state, NULL) != 0) {
		RECORD_ERROR(state, COLLECTION_FAILURE, result);
		goto cleanup;
	}

	return state->current_thread;

cleanup:
	if (result != 0) {
		result = GetLastError();
		if (result == ERROR_NO_MORE_FILES) {
			/* this is a ligitimate end of thread list error value */
			result = 0;
		} else {
			RECORD_ERROR(state, COLLECTION_FAILURE, result);
		}
	}

	SetLastError(result);
	cleanup(state);
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
