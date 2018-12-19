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

/*
 * This file includes an incomplete implementation of NUMA for AIX.
 * Specifically:
 *  1) omrthread_numa_set_node_affinity_nolock won't build on AIX 5.2
 *  2) omrthread_numa_get_affinity is unimplemented
 * Do not enable OMR_PORT_NUMA_SUPPORT until these issues have been resolved
 */


/**
 * @file
 * @ingroup Thread
 * @brief NUMA support for Thread library.
 */
#include "omrcfg.h"
#include "threaddef.h"
#include "ut_j9thr.h"

#include <sys/rset.h>
#include <stdio.h> /* for printf debugging */
#include <dlfcn.h>


/* This ifdef is specifically provided to allow compilation on AIX 5.3 of features which we can only use on 6.1 */
#if !defined(__ENHANCED_AFFINITY)
#include <sys/systemcfg.h>
#define __ENHANCED_AFFINITY_MASK    0x1000
#define __ENHANCED_AFFINITY() \
           (_system_configuration.kernel & __ENHANCED_AFFINITY_MASK)

/* sys/processor.h */
typedef short sradid_t;

/* sys/rset.h */
#define R_SRADSDL       R_MCMSDL
#define RS_SRADID_LOADAVG   2
#define R_SRADID        13
#define SRADID_ANY          (-1)
typedef struct loadavg_info {
	int load_average;
	int cpu_count;
} loadavg_info_t;

#if !defined(_AIX61) || defined(J9OS_I5_V6R1)
/* create rsid_t_MODIFIED since rsid_t is already defined in 5.3 but is missing the at_sradid field */
typedef union {
	pid_t at_pid;           /* Process id (for R_PROCESS and R_PROCMEM */
	tid_t at_tid;           /* Kernel thread id (for R_THREAD) */
	int at_shmid;           /* Shared memory id (for R_SHM) */
	int at_fd;              /* File descriptor (for R_FILDES) */
	rsethandle_t at_rset;   /* Resource set handle (for R_RSET) */
	subrange_t *at_subrange;  /* Memory ranges (for R_SUBRANGE) */
	sradid_t at_sradid;     /* SRAD id (for R_SRADID) */
#ifdef _KERNEL
	ulong_t at_raw_val;     /* raw value: used to avoid copy typecasting */
#endif
} rsid_t_MODIFIED;
#endif

extern sradid_t rs_get_homesrad(void);
extern int rs_info(void *out, int command, long arg1, long arg2);
#if !defined(_AIX61) || defined(J9OS_I5_V6R1)
extern int ra_attach(rstype_t, rsid_t, rstype_t, rsid_t_MODIFIED, uint_t);
#else
extern int ra_attach(rstype_t, rsid_t, rstype_t, rsid_t, uint_t);
#endif
/* sys/vminfo.h */
struct vm_srad_meminfo {
	int vmsrad_in_size;
	int vmsrad_out_size;
	size64_t vmsrad_total;
	size64_t vmsrad_free;
	size64_t vmsrad_file;
	int vmsrad_aff_priv_pct;
	int vmsrad_aff_avail_pct;
};
#define VM_SRAD_MEMINFO        106

#endif /* !defined(__ENHANCED_AFFINITY) */

/* function pointers defined only in AIX 6.1 */
static sradid_t (*PTR_rs_get_homesrad)(void);
#if !defined(_AIX61) || defined(J9OS_I5_V6R1)
static int (*PTR_ra_attach)(rstype_t, rsid_t, rstype_t, rsid_t_MODIFIED, uint_t);
#else
static int (*PTR_ra_attach)(rstype_t, rsid_t, rstype_t, rsid_t, uint_t);
#endif

static BOOLEAN isNumaAvailable = FALSE;

/**
 * Hides the details of how to assemble the parameters required by ra_attach.
 * @param thread The thread to bind (this thread has started)
 * @param nodeNumber The J9 node number to which we should bind the thread (0 means it can run on all nodes)
 * @return 0 on success or if NUMA is not supported
 */
static intptr_t bindThreadToNode(omrthread_t thread, uintptr_t nodeNumber);


/**
 *
 * This function must only be called *once*. It is called from omrthread_init().
 */
void
omrthread_numa_init(omrthread_library_t threadLibrary)
{
	PTR_rs_get_homesrad = (sradid_t (*)(void))dlsym(RTLD_DEFAULT, "rs_get_homesrad");
#if !defined(_AIX61) || defined(J9OS_I5_V6R1)
	PTR_ra_attach = (int (*)(rstype_t, rsid_t, rstype_t, rsid_t_MODIFIED, uint_t))dlsym(RTLD_DEFAULT, "ra_attach");
#else
	PTR_ra_attach = (int (*)(rstype_t, rsid_t, rstype_t, rsid_t, uint_t))dlsym(RTLD_DEFAULT, "ra_attach");
#endif
	isNumaAvailable = TRUE;
}

void
omrthread_numa_shutdown(omrthread_library_t lib)
{
	PTR_rs_get_homesrad = NULL;
	PTR_ra_attach = NULL;
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

#if defined(OMR_PORT_NUMA_SUPPORT)
	if (isNumaAvailable && (0 != __ENHANCED_AFFINITY())) {
		rsethandle_t rsAll = rs_alloc(RS_ALL);
		/* we need the SRAD SDL since it is the one which applies to NUMA */
		int sradSDL = rs_getinfo(NULL, R_SRADSDL, 0);
		int numSRADs = rs_numrads(rsAll, sradSDL, 0);
		rs_free(rsAll);
		/* SRADS are 0-indexed and contiguous so the highest node number is always 1 less than the total count
		 * - we want to return the 1-indexed internal node values so don't subtract 1
		 */
		result = numSRADs;
	}
#endif
	return result;
}

intptr_t
omrthread_numa_set_node_affinity_nolock(omrthread_t thread, const uintptr_t *nodeList, uintptr_t numaCount, uint32_t flags)
{
	intptr_t result = 0;
#if defined(OMR_PORT_NUMA_SUPPORT)
	if (isNumaAvailable && (NULL != thread)) {
		/* Check the thread's state, if it's started, we can set the affinity now.
		 * Otherwise we'll need defer setting the affinity until later.
		 */
		uintptr_t threadIsStarted = thread->flags & (J9THREAD_FLAG_STARTED | J9THREAD_FLAG_ATTACHED);
		uintptr_t numaNode = 0;

		if (1 == numaCount) {
			numaNode = nodeList[0];
			/* specifying that we intend to be bound to 0 doesn't make sense:  an unbound thread is bound to an empty list of nodes */
			Assert_THR_true(numaNode > 0);

			if (0 != threadIsStarted) {
				result = bindThreadToNode(thread, numaNode);
			} else {
				/* Thread is not yet started, setting deferred affinity */
				result = 0;
			}
		} else if (numaCount > 1) {
			/* we can't bind a thread to multiple SRADs on AIX */
			result = -1;
		} else {
			if (0 != threadIsStarted) {
				/* remove any binding on this thread */
				result = bindThreadToNode(thread, 0);
			} else {
				/* Thread is not yet started, setting deferred affinity */
				result = 0;
			}
		}

		/* Update the thread->numaAffinity field if setting the affinity succeeded */
		if (0 == result) {
			memset(&thread->numaAffinity, 0x0, sizeof(thread->numaAffinity));
			if (0 != numaNode) {
				omrthread_add_node_number_to_affinity_cache(thread, numaNode);
			}
		}
	}
#endif /* OMR_PORT_NUMA_SUPPORT */
	return result;
}

/**
 * Set the affinity for the specified thread so that it runs only (or preferentially) on
 * processors associated with the specified NUMA nodes contained in the numaNodes array (of nodeCount length).
 * This may be called before a thread is started or once it is active.
 *
 * Note that in the case that this function is called on a thread that is not yet started,
 * setting the affinity will be deferred until the thread is started. This function will still
 * return 0, indicating success, however there is no way to tell if the affinity was successfully
 * set once the thread is actually started.
 *
 * @param[in] thread The thread to be modified. Can be any arbitrary omrthread_t.
 * @param[in] numaNodes The array of node indexes with which to associate the given thread, where 1 is the first node.
 * @param[in] nodeCount The number of nodes in the numaNodes array. (0 indicates no affinity)
 *
 * @return 0 on success, -1 on failure. Note that this function will return 0 even if NUMA is not available.
 * Use omrthread_numa_get_max_node() to test for the availability of NUMA.
 */
intptr_t
omrthread_numa_set_node_affinity(omrthread_t thread, const uintptr_t *numaNodes, uintptr_t nodeCount, uint32_t flags)
{
	intptr_t result = -1;

	if (isNumaAvailable && (NULL != thread)) {
		if (NULL != thread) {
			THREAD_LOCK(thread, 0);
			result = omrthread_numa_set_node_affinity_nolock(thread, numaNodes, nodeCount, 0);
			THREAD_UNLOCK(thread);
		}
	} else {
		result = 0;
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
	intptr_t result = 0;
	uintptr_t incomingListSize = *nodeCount;
	uintptr_t nodesSoFar = 0;

#if defined(OMR_PORT_NUMA_SUPPORT)
	if (isNumaAvailable && (NULL != thread)) {
		uintptr_t node = 0;
		uintptr_t numaMaxNode = omrthread_numa_get_max_node();
		THREAD_LOCK(thread, 0);

		for (node = 1; node <= numaMaxNode; node++) {
			if (omrthread_does_affinity_cache_contain_node(thread, node)) {
				if (nodesSoFar < incomingListSize) {
					numaNodes[nodesSoFar] = node;
				}
				nodesSoFar += 1;
			}
		}
		/* handle the case where we aren't on any node - which implies the special node 0 (the global node) */
		if (0 == nodesSoFar) {
			numaNodes[nodesSoFar] = 0;
			nodesSoFar += 1;
		}
		THREAD_UNLOCK(thread);
	}
#endif /* OMR_PORT_NUMA_SUPPORT */
	*nodeCount = nodesSoFar;
	return result;
}

static intptr_t
bindThreadToNode(omrthread_t thread, uintptr_t nodeNumber)
{
	intptr_t result = 0;

	if ((NULL != PTR_ra_attach) && (NULL != PTR_rs_get_homesrad)) {
		rsid_t thisThreadResourceID;
		thisThreadResourceID.at_tid = thread->tid;
#if !defined(_AIX61) || defined(J9OS_I5_V6R1)
		rsid_t_MODIFIED targetSRADResourceID;
#else
		rsid_t targetSRADResourceID;
#endif
		if (nodeNumber > 0) {
			/* bind to a specific node */
			/* don't forget to subtract one from the numaNode to shift it into the 0-indexed numbering scheme used by the system */
			sradid_t desiredSRADID = nodeNumber - 1;
			/* attach srad to current thread */
			targetSRADResourceID.at_sradid = desiredSRADID;
		} else {
			/* Bind to any SRAD. Since there's no way to explicitly do that on AIX, just (nonstrictly) bind to current thread's SRADID. */
			targetSRADResourceID.at_sradid = PTR_rs_get_homesrad();
		}
		/* Currently we attach non-strictly (no R_STRICT_SRAD).  Don't forget to use a non-strict call to PTR_ra_attach
		 * for nodeNumber 0 if we do ever start attaching strictly
		 */
		result = PTR_ra_attach(R_THREAD, thisThreadResourceID, R_SRADID, targetSRADResourceID, 0);
	}
	return result;
}

uintptr_t
omrthread_numa_get_current_node(){
	sradid_t sradid = SRADID_ANY;
	if (NULL != PTR_rs_get_homesrad) {
		sradid = PTR_rs_get_homesrad();
	}

	/* if we couldn't find the home SRADID return 0, meaning no affinity */
	if (sradid == SRADID_ANY) {
		return 0;
	} else {
		return sradid + 1;
	}
}
