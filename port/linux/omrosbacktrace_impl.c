/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * @brief Stack backtracing support
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "omrport.h"
#include "omrportpriv.h"
#include "omrsignal_context.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <stdlib.h>
#include <string.h>

#include "omrintrospect.h"

uintptr_t protectedBacktrace(struct OMRPortLibrary *port, void *arg);
uintptr_t backtrace_sigprotect(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, void **address_array, int capacity);

struct frameData {
	void **address_array;
	unsigned int capacity;
};


/*
 * NULL handler. We only care about preventing the signal from propagating up the call stack, no need to do
 * anything in the handler.
 */
static uintptr_t
handler(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *userData)
{
	return OMRPORT_SIG_EXCEPTION_RETURN;
}

/*
 * Wrapped call to libc backtrace function
 */
uintptr_t
protectedBacktrace(struct OMRPortLibrary *port, void *arg)
{
	struct frameData *addresses = (struct frameData *)arg;
	return backtrace(addresses->address_array, addresses->capacity);
}

/*
 * Provides signal protection for the libc backtrace call. If the stack walk causes a segfault then we check the output
 * array to see if we got any frames as we may be able to provide a partial backtrace. We also record that the stack walk
 * was incomplete so that information is available after the fuction returns.
 */
uintptr_t
backtrace_sigprotect(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, void **address_array, int capacity)
{
	uintptr_t ret;
	struct frameData args;
	args.address_array = address_array;
	args.capacity = capacity;

	memset(address_array, 0, sizeof(void *) * capacity);

	if (omrthread_self()) {
		if (portLibrary->sig_protect(portLibrary, protectedBacktrace, &args, handler, NULL, OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_RETURN, &ret) != 0) {
			/* check to see if there were any addresses populated */
			for (ret = 0; ret < args.capacity && address_array[ret] != NULL; ret++);

			threadInfo->error = FAULT_DURING_BACKTRACE;
		}

		return ret;
	} else {
		return backtrace(address_array, capacity);
	}
}



/* This function constructs a backtrace from a CPU context. Generally there are only one or two
 * values in the context that are actually used to construct the stack but these vary by platform
 * so arn't detailed here. If no heap is specified then this function will use malloc to allocate
 * the memory necessary for the stack frame structures which must be freed by the caller.
 *
 * @param portLbirary a pointer to an initialized port library
 * @param threadInfo the thread structure we want  to attach the backtrace to. Must not be NULL.
 * @param heap a heap from which to allocate any necessary memory. If NULL malloc is used instead.
 * @param signalInfo a platform signal context. If not null the context held in threadInfo is replaced
 *  	  with the signal context before the backtrace is generated.
 *
 * @return the number of frames in the backtrace.
 */
uintptr_t
omrintrospect_backtrace_thread_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, void *signalInfo)
{
	void *addresses[50];
	J9PlatformStackFrame **nextFrame;
	J9PlatformStackFrame *junkFrames = NULL;
	J9PlatformStackFrame *prevFrame = NULL;
	OMRUnixSignalInfo *sigInfo = (OMRUnixSignalInfo *)signalInfo;
	int i;
	int discard = 0;
	int ret;
	const char *regName = "";
	void **faultingAddress = 0;

	if (threadInfo == NULL || (threadInfo->context == NULL && sigInfo == NULL)) {
		return 0;
	}

	/* if we've been passed a port library wrapped signal, then extract info from there */
	if (sigInfo != NULL) {
		threadInfo->context = sigInfo->platformSignalInfo.context;

		/* get the faulting address so we can discard frames that are part of the signal handling */
		infoForControl(portLibrary, sigInfo, 0, &regName, (void **)&faultingAddress);
	}

	ret = backtrace_sigprotect(portLibrary, threadInfo, addresses, sizeof(addresses) / sizeof(void *));

	nextFrame = &threadInfo->callstack;
	for (i = 0; i < ret; i++) {
		if (heap != NULL) {
			*nextFrame = portLibrary->heap_allocate(portLibrary, heap, sizeof(J9PlatformStackFrame));
		} else {
			*nextFrame = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9PlatformStackFrame), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		}

		if (*nextFrame == NULL) {
			if (!threadInfo->error) {
				threadInfo->error = ALLOCATION_FAILURE;
			}

			break;
		}

		(*nextFrame)->parent_frame = NULL;
		(*nextFrame)->symbol = NULL;
		(*nextFrame)->instruction_pointer = (uintptr_t)addresses[i];
		(*nextFrame)->stack_pointer = 0;
		(*nextFrame)->base_pointer = 0;

		nextFrame = &(*nextFrame)->parent_frame;

		/* check to see if we should truncate the stack trace to omit handler frames */
		if (prevFrame && faultingAddress && addresses[i] == *faultingAddress) {
			/* discard all frames up to this point */
			junkFrames = threadInfo->callstack;
			threadInfo->callstack = prevFrame->parent_frame;

			/* break the link so we can free the junk */
			prevFrame->parent_frame = NULL;

			/* correct the next frame */
			nextFrame = &threadInfo->callstack->parent_frame;
			/* record how many frames we discard */
			discard = i + 1;
		}

		/* save the frame we've just set up so that we can blank it if necessary */
		if (prevFrame == NULL) {
			prevFrame = threadInfo->callstack;
		} else {
			prevFrame = prevFrame->parent_frame;
		}
	}

	/* if we discarded any frames then free them */
	while (junkFrames != NULL) {
		J9PlatformStackFrame *tmp = junkFrames;
		junkFrames = tmp->parent_frame;

		if (heap != NULL) {
			portLibrary->heap_free(portLibrary, heap, tmp);
		} else {
			portLibrary->mem_free_memory(portLibrary, tmp);
		}
	}

	return i - discard;
}

/* This function takes a thread structure already populated with a backtrace by omrintrospect_backtrace_thread
 * and looks up the symbols for the frames. The format of the string generated is:
 * 		symbol_name (statement_id instruction_pointer [module+offset])
 * If it isn't possible to determine any of the items in the string then they are omitted. If no heap is specified
 * then this function will use malloc to allocate the memory necessary for the symbols which must be freed by the caller.
 *
 * @param portLbirary a pointer to an initialized port library
 * @param threadInfo a thread structure populated with a backtrace
 * @param heap a heap from which to allocate any necessary memory. If NULL malloc is used instead.
 *
 * @return the number of frames for which a symbol was constructed.
 */
uintptr_t
omrintrospect_backtrace_symbols_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap)
{
	J9PlatformStackFrame *frame;
	int i;

	for (i = 0, frame = threadInfo->callstack; frame != NULL; frame = frame->parent_frame, i++) {
		char output_buf[512];
		char *cursor = output_buf;
		Dl_info dlInfo;
		uintptr_t length;
		uintptr_t iar = frame->instruction_pointer;
		uintptr_t symbol_offset = 0;
		uintptr_t module_offset = 0;
		const char *symbol_name = "";
		const char *module_name = "<unknown>";
		short symbol_length = 0;

		memset(&dlInfo, 0, sizeof(Dl_info));

		/* do the symbol resolution while we're here */
		if (dladdr((void *)iar, &dlInfo)) {
			if (dlInfo.dli_sname != NULL) {
				uintptr_t sym = (uintptr_t)dlInfo.dli_saddr;
				symbol_name = dlInfo.dli_sname;
				symbol_length = strlen(symbol_name);
				symbol_offset = iar - (uintptr_t)sym;
			}

			if (dlInfo.dli_fname != NULL) {
				/* strip off the full path if possible */
				module_name = strrchr(dlInfo.dli_fname, '/');
				if (module_name != NULL) {
					module_name++;
				} else {
					module_name = dlInfo.dli_fname;
				}
			}

			if (dlInfo.dli_fbase != NULL) {
				/* set the module offset */
				module_offset = iar - (uintptr_t)dlInfo.dli_fbase;
			}
		}

		/* sanity check */
		if (symbol_name == NULL) {
			symbol_name = "";
			symbol_length = 0;
		}

		/* symbol_name+offset (id, instruction_pointer [module+offset]) */
		if (symbol_length > 0) {
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "%.*s", symbol_length, symbol_name);
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "+0x%x ", symbol_offset);
		}
		cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "(0x%p", frame->instruction_pointer);
		if (module_name[0] != '\0') {
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), " [%s+0x%x]", module_name, module_offset);
		}
		*(cursor++) = ')';
		*cursor = 0;

		length = (cursor - output_buf) + 1;
		if (heap != NULL) {
			frame->symbol = portLibrary->heap_allocate(portLibrary, heap, length);
		} else {
			frame->symbol = portLibrary->mem_allocate_memory(portLibrary, length, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		}

		if (frame->symbol != NULL) {
			strncpy(frame->symbol, output_buf, length);
		} else {
			frame->symbol = NULL;
			if (!threadInfo->error) {
				threadInfo->error = ALLOCATION_FAILURE;
			}
			i--;
		}
	}

	return i;
}
