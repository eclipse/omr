/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * @ingroup Thread
 * @brief NUMA support for Thread library.
 */
#include "omrcfg.h"
#include "threaddef.h"
static BOOLEAN isNumaAvailable = FALSE;

/**
 * Loads the necessary functions for NUMA support from the OS's dynamic library for NUMA.
 * This will initialize the lib->libNumaHandle and lib->numaAvailable fields if OMR_PORT_NUMA_SUPPORT
 * is enabled.
 *
 * This function must only be called *once*. It is called from omrthread_init().
 */
void
omrthread_numa_init(omrthread_library_t lib)
{
	isNumaAvailable = TRUE;
}

/**
 * Closes the dynamic library for NUMA. Should only be called from omrthread_shutdown().
 */
void
omrthread_numa_shutdown(omrthread_library_t lib)
{
	isNumaAvailable = FALSE;
}

void
omrthread_numa_set_enabled(BOOLEAN enabled)
{
	isNumaAvailable = enabled;
}

/**
 * Return the highest NUMA node ID available to the process.
 * The first node is always identified as 1, as 0 is used to indicate no affinity.
 *
 * This function should be called to test the availability of NUMA <em>before</em>
 * calling any of the other NUMA functions.
 *
 * If NUMA is not enabled or supported this will always return 0.
 *
 * @return The highest NUMA node ID available to the process. 0 if no affinity, or NUMA not enabled.
 */
uintptr_t
omrthread_numa_get_max_node(void)
{
	uintptr_t result = 0;
	ULONG nodeNumber = 0;
	if (isNumaAvailable && (GetNumaHighestNodeNumber(&nodeNumber)) && (nodeNumber > 1)) {
		result = (uintptr_t)nodeNumber + 1;
	}
	return result;
}

intptr_t
omrthread_numa_set_node_affinity_nolock(omrthread_t thread, const uintptr_t *nodeList, uintptr_t nodeCount, uint32_t flags)
{
	intptr_t result = J9THREAD_NUMA_OK;
	HANDLE processHandle = GetCurrentProcess();
	DWORD_PTR processAffinityMask = 0;
	DWORD_PTR systemAffinityMask = 0;

	if (isNumaAvailable && GetProcessAffinityMask(processHandle, &processAffinityMask, &systemAffinityMask)) {
		/* go through the nodes and build a mask corresponding to the processors (or groups) on the user-provided nodes and take the intersection of these sets */
		uintptr_t i = 0;
		ULONGLONG mask = 0;
		ULONGLONG comparableProcessAffinityMask = (ULONGLONG)processAffinityMask;
		DWORD_PTR oldValue = 0;
		HANDLE thisThread = GetCurrentThread();

		for (i = 0; i < nodeCount; i++) {
			UCHAR nodeNumber = (UCHAR)(nodeList[i] - 1);
			ULONGLONG thisNodeMask = 0;

			if (GetNumaNodeProcessorMask(nodeNumber, &thisNodeMask)) {
				mask |= (thisNodeMask & comparableProcessAffinityMask);
			}
		}
		oldValue = SetThreadAffinityMask(thisThread, (DWORD_PTR)mask);
		if (0 == oldValue) {
			/* failure */
			result = J9THREAD_NUMA_ERR;
		}
	}
	return result;
}

intptr_t
omrthread_numa_set_node_affinity(omrthread_t thread, const uintptr_t *numaNodes, uintptr_t nodeCount, uint32_t flags)
{
	intptr_t result = J9THREAD_NUMA_ERR;

	if (isNumaAvailable) {
		if (NULL != thread) {
			THREAD_LOCK(thread, 0);
			result = omrthread_numa_set_node_affinity_nolock(thread, numaNodes, nodeCount, flags);
			THREAD_UNLOCK(thread);
		}
	} else {
		result = J9THREAD_NUMA_OK;	
	} 
	return result;
}

/**
 * Determine what the NUMA node affinity is for the specified thread.
 *
 * @param[in] thread - the thread to be queried. Can be any arbitrary omrthread_t
 * @param[out] numaNodes The array of node indexes with which the given thread is associated, where 1 is the first node.
 * @param[in/out] nodeCount The number of nodes in the numaNodes array, on input, and the number of nodes with which this thread is associated, on output.  The minimum of these two values will be the number of entries populated in the numaNodes array (other entries will be untouched).
 *
 * @return 0 on success, non-zero if affinity cannot be determined. Note that this function will return 0 even if
 * NUMA is not available. Use omrthread_numa_get_max_node() to test for the availability of NUMA.
 */
intptr_t
omrthread_numa_get_node_affinity(omrthread_t thread, uintptr_t *numaNodes, uintptr_t *nodeCount)
{
	/* Win32 does not allow us to get the affinity of a thread in terms of NUMA nodes (it can only return the "ideal processor" which is a
	 * completely different level of granularity than this so just return an error.
	 */
	*nodeCount = 0;
	return J9THREAD_NUMA_ERR_AFFINITY_NOT_SUPPORTED;
}

uintptr_t
omrthread_numa_get_current_node()
{
	USHORT node = 0;

	PROCESSOR_NUMBER processorNumber;
	GetCurrentProcessorNumberEx(&processorNumber);
	if (GetNumaProcessorNodeEx(&processorNumber, &node)) {
		node += 1;
	} else {
		node = 0;
	}
	return node;
}
