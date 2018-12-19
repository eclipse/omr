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
}

/**
 * Closes the dynamic library for NUMA. Should only be called from omrthread_shutdown().
 */
void
omrthread_numa_shutdown(omrthread_library_t lib)
{
}

/**
 * Sets the NUMA enabled status.
 * This applies to the entire process.
 */
void
omrthread_numa_set_enabled(BOOLEAN enabled)
{
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
	/* This is the common function, just return 0 */
	return 0;
}

/**
 * This performs exactly the same operation as omrthread_numa_set_node_affinity, however it does *not*
 * lock the thread. This should only be called when a lock on a thread is already obtained.
 *
 * @return Returns 0 on success, and -1 if we failed to set the thread's affinity. Note
 * that this function will return 0 even if NUMA is not available. Use omrthread_numa_get_max_node()
 * to test for the availability of NUMA.
 *
 * @see omrthread_numa_set_node_affinity(omrthread_t thread, const uintptr_t *nodeList, uintptr_t nodeCount, uint32_t flags)
 */
intptr_t
omrthread_numa_set_node_affinity_nolock(omrthread_t thread, const uintptr_t *nodeList, uintptr_t nodeCount, uint32_t flags)
{
	return 0;
}

/**
 * Set the affinity for the specified thread so that it runs only (or preferentially) on
 * processors associated with the specified NUMA nodes contained in the numaNodes array (of nodeCount length).
 * This may be called before a thread is started or once it is active.
 *
 * On Linux, the requested set of CPUs is intersected with the initial CPU affinity of the process.
 * The J9THREAD_NUMA_OVERRIDE_DEFAULT_AFFINITY flag overrides this behaviour on Linux;
 * it has no effect on other operating systems.
 *
 * Note that in the case that this function is called on a thread that is not yet started,
 * setting the affinity will be deferred until the thread is started. This function will still
 * return 0, indicating success, however there is no way to tell if the affinity was successfully
 * set once the thread is actually started.
 *
 * @param[in] thread The thread to be modified. Can be any arbitrary omrthread_t.
 * @param[in] numaNodes The array of node indexes with which to associate the given thread, where 1 is the first node.
 * @param[in] nodeCount The number of nodes in the numaNodes array. (0 indicates no affinity)
 * @param[in] flags control the behavior of the function
 *
 * @return 0 on success, -1 on failure. Note that this function will return 0 even if NUMA is not available.
 * Use omrthread_numa_get_max_node() to test for the availability of NUMA.
 */
intptr_t
omrthread_numa_set_node_affinity(omrthread_t thread, const uintptr_t *numaNodes, uintptr_t nodeCount, uint32_t flags)
{
	/* This is the common function, just return 0 */
	return 0;
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
	/* This is the common function, just return 0 */
	/* since we know about 0 nodes, don't touch anything in the numaNodes array */
	*nodeCount = 0;
	return 0;
}


uintptr_t
omrthread_numa_get_current_node(){
	return 0;
}
