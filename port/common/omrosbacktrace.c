/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @brief Stack backtracing support
 */
#include "omrport.h"
#include "omrintrospect_common.h"

/* The actual backtrace implementations that the protection wrappers call. These are platform specific */
extern uintptr_t omrintrospect_backtrace_symbols_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap);
extern uintptr_t omrintrospect_backtrace_thread_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, void *signalInfo);

/* Packaging struct to pass aguments through the signal protection */
struct threadData {
	J9PlatformThread *threadInfo;
	J9Heap *heap;
	void *signalInfo;
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

/* Wrapper to unpack structure into arguments */
static uintptr_t
protectedIntrospectBacktraceThread(struct OMRPortLibrary *port, void *arg)
{
	struct threadData *args = (struct threadData *)arg;
	return omrintrospect_backtrace_thread_raw(port, args->threadInfo, args->heap, args->signalInfo);
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
omrintrospect_backtrace_thread(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, void *signalInfo)
{
	uintptr_t ret;
	struct threadData args;
	args.threadInfo = threadInfo;
	args.heap = heap;
	args.signalInfo = signalInfo;

	if (omrthread_self()) {
		if (portLibrary->sig_protect(portLibrary, protectedIntrospectBacktraceThread, &args, handler, NULL, OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_RETURN, &ret) == 0) {
			return ret;
		} else {
			J9PlatformStackFrame *frame;
			int frames = 0;

			/* see if there is a partial backtrace we can return */
			for (frame = threadInfo->callstack; frame != NULL; frame = frame->parent_frame, frames++);

			threadInfo->error = FAULT_DURING_BACKTRACE;
			return frames;
		}
	} else {
		return omrintrospect_backtrace_thread_raw(portLibrary, threadInfo, heap, signalInfo);
	}
}

/* Wrapper to unpack structure into arguments */
static uintptr_t
protectedIntrospectBacktraceSymbols(struct OMRPortLibrary *port, void *arg)
{
	struct threadData *args = (struct threadData *)arg;
	return omrintrospect_backtrace_symbols_raw(port, args->threadInfo, args->heap);
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
omrintrospect_backtrace_symbols(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap)
{
	uintptr_t ret;
	struct threadData args;
	args.threadInfo = threadInfo;
	args.heap = heap;

	if (omrthread_self()) {
		if (portLibrary->sig_protect(portLibrary, protectedIntrospectBacktraceSymbols, &args, handler, NULL, OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_RETURN, &ret) == 0) {
			return ret;
		} else {
			threadInfo->error = FAULT_DURING_BACKTRACE;
			return 0;
		}
	} else {
		return omrintrospect_backtrace_symbols_raw(portLibrary, threadInfo, heap);
	}
}



