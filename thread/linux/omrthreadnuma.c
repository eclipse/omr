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
 * @ingroup Thread
 * @brief NUMA support for Thread library.
 */
/* _GNU_SOURCE must be defined for CPU_SETSIZE */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <sys/stat.h>
#if !defined(OMRZTPF)
#include <sys/syscall.h>
#endif /* !defined(OMRZTPF) */
#include <stdio.h>

#include "omrcfg.h"
#include "threaddef.h"

#if defined(J9ZTPF)
#define CPU_SETSIZE 4
#define CPU_ISSET(x,y) 0
#define CPU_SET(x,y) {}
#define CPU_CLR(x,y) {}
typedef struct {
        char __junkbits;
} cpu_set_t[CPU_SETSIZE];
#endif

static BOOLEAN isNumaAvailable = FALSE;
static uintptr_t numNodes = 0;

#if defined(OMR_PORT_NUMA_SUPPORT)

static uintptr_t cpuset_subset_or_equal(cpu_set_t *subset, cpu_set_t *superset);
static void cpuset_logical_or(cpu_set_t *destination, const cpu_set_t *source);
#undef OMR_VERBOSE_NUMA_NODE_DATA
#ifdef OMR_VERBOSE_NUMA_NODE_DATA
 void dumpNumaInfo();
#endif

static cpu_set_t defaultAffinityMask = {{0}};

/* A table of data about each NUMA node. Node 0 is the synthetic node which includes all resources. */
static struct {
	cpu_set_t cpu_set;
	uintptr_t cpu_count;
} *numaNodeData;

/**
 * Tests whether a given bitfield is a subset or equal to another bitfield.
 * It does this by checking that each element of the subset has a
 * corresponding element set in the superset.
 *
 * @return 1 if subset is a subset or equal to superset, 0 otherwise.
 */
static uintptr_t
cpuset_subset_or_equal(cpu_set_t *subset, cpu_set_t *superset)
{
	uintptr_t i = 0;
	uintptr_t result = 1;
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, subset) && !CPU_ISSET(i, superset)) {
			result = 0;
			break;
		}
	}
	return result;
}

/**
 * Combines the two given bitsets, destination and source, storing their logical OR result into destination
 * Note that this can be replaced with "CPU_OR" once we update to glibc 2.7.
 * @param destination[in/out] One source and a destination of the logical OR
 * @param source[in] The other source of the logical OR
 */
static void
cpuset_logical_or(cpu_set_t *destination, const cpu_set_t *source)
{
	uintptr_t i = 0;

	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, source) && !CPU_ISSET(i, destination)) {
			CPU_SET(i, destination);
		}
	}
}

/**
 * Combines the two given bitsets, destination and source, storing their logical AND result into destination
 * @param destination[in/out] One source and a destination of the logical AND
 * @param source[in] The other source of the logical AND
 */
static void
cpuset_logical_and(cpu_set_t *destination, const cpu_set_t *source)
{
	uintptr_t i = 0;

	for (i = 0; i < CPU_SETSIZE; i++) {
		if (!CPU_ISSET(i, source) && CPU_ISSET(i, destination)) {
			CPU_CLR(i, destination);
		}
	}
}

static uintptr_t
countNumaNodes(void)
{
	uintptr_t maxNode = 0;
	DIR *nodes = opendir("/sys/devices/system/node/");
	if (NULL != nodes) {
		struct dirent *node = readdir(nodes);
		while (NULL != node) {
			unsigned long nodeIndex = 0;
			if (1 == sscanf(node->d_name, "node%lu", &nodeIndex)) {
				if (nodeIndex > maxNode) {
					maxNode = nodeIndex;
				}
			}
			node = readdir(nodes);
		}
		closedir(nodes);
	}

	return maxNode + 1;
}

static intptr_t
initializeNumaNodeData(omrthread_library_t threadLibrary, uintptr_t numNodes)
{
	uintptr_t result = 0;

	numaNodeData = omrthread_allocate_memory(threadLibrary, sizeof(numaNodeData[0]) * (numNodes + 1), OMRMEM_CATEGORY_THREADS);
	if (NULL == numaNodeData) {
		result = -1;
	} else {
		DIR *nodes = NULL;
		unsigned long nodeIndex = 0;
		const char NODE_PATH[] = "/sys/devices/system/node/";
		for (nodeIndex = 0; nodeIndex <= numNodes; nodeIndex++) {
			CPU_ZERO(&numaNodeData[nodeIndex].cpu_set);
			numaNodeData[nodeIndex].cpu_count = 0;
		}

		nodes = opendir(NODE_PATH);
		if (NULL == nodes) {
			result = -1;
		} else {
			struct dirent *node = readdir(nodes);
			if (NULL != node) {
				const char CPUMAP[] = "/cpumap";
				char pathBuffer[sizeof(NODE_PATH) + sizeof(CPUMAP) + 16]; /* extra space for "node<n>" & terminating null */
				strcpy(pathBuffer, NODE_PATH);
				char *nodeRoot = pathBuffer + sizeof(NODE_PATH) - 1; /* shortcut to the end of the string */
				do {
					if (1 == sscanf(node->d_name, "node%lu", &nodeIndex)) {
						if (nodeIndex < numNodes) {
							strcpy(nodeRoot, node->d_name);
							strcat(nodeRoot, CPUMAP);
							int cpumapFile = open(pathBuffer, O_RDONLY);
							BOOLEAN cpumapOkay = FALSE;
							if (-1 != cpumapFile) {
								char defaultMapBuffer[128];
								char * mapBuffer = defaultMapBuffer;
								size_t bufferSize = sizeof(defaultMapBuffer);
								size_t bytesRead = read(cpumapFile, mapBuffer, bufferSize);
								if (bytesRead == bufferSize) { /* buffer possibly not big enough */
									bufferSize = 4096;
									mapBuffer = malloc(bufferSize);
									if (NULL == mapBuffer) {
										result = -1;
										close(cpumapFile);
										break;
									}
									size_t bytesRead = read(cpumapFile, mapBuffer, bufferSize-1);
									mapBuffer[bytesRead] = '\0';
								}
								if ((0 != bytesRead) && (bytesRead < bufferSize-1)) { /* the buffer was too small */
									/* scan from the end of the line (lowest numbered CPU) toward the front */
									char *cursor = mapBuffer+strlen(mapBuffer);
									uint32_t charCount = 0;
									do {
										while ((cursor > mapBuffer) && !isxdigit(*cursor)) { /* find the start of the hex string */
											cursor -= 1;
										}
										uint32_t cpuIndex = 4 * charCount; /* each hex digit represents 4 CPUs.  Use the count from the last iteration */
										while ((cursor > mapBuffer) && isxdigit(*cursor)) {
											charCount += 1;
											cursor -= 1;
										}
										uintmax_t cpuMap = strtoumax((cursor == mapBuffer)? cursor: cursor + 1, NULL, 16);
										/* cursor points to a non-hex char if not at the front of the buffer */
										while ((0 != cpuMap)) {
											if (0 != (cpuMap  & 1)) {
												CPU_SET(cpuIndex, &numaNodeData[nodeIndex + 1].cpu_set);
												numaNodeData[nodeIndex + 1].cpu_count += 1;

												/* add all cpus to the synthetic 0 node */
												CPU_SET(cpuIndex, &numaNodeData[0].cpu_set);
												numaNodeData[0].cpu_count += 1;
											}
											cpuMap >>= 1;
											cpuIndex += 1;
										}
									} while (cursor > mapBuffer);
								}
								if (mapBuffer != defaultMapBuffer) {
									free(mapBuffer);
								}
								close(cpumapFile);
								cpumapOkay = TRUE;
							}
							if (!cpumapOkay) { /* read the directory entries */
								strcpy(nodeRoot, node->d_name); /* strip off "/cpulist".  Updates pathBuffer  */
								DIR *cpus =  opendir(pathBuffer);
								if (NULL != cpus) {
									struct dirent *cpu = readdir(cpus);
									while (NULL != cpu) {
										unsigned long cpuIndex = 0;
										if (1 == sscanf(cpu->d_name, "cpu%lu", &cpuIndex)) {
											CPU_SET(cpuIndex, &numaNodeData[nodeIndex + 1].cpu_set);
											numaNodeData[nodeIndex + 1].cpu_count += 1;

											/* add all cpus to the synthetic 0 node */
											CPU_SET(cpuIndex, &numaNodeData[0].cpu_set);
											numaNodeData[0].cpu_count += 1;
										}
										cpu = readdir(cpus);
									}
									closedir(cpus);
								}
							}
						}
					}
					node = readdir(nodes);
				} while (NULL != node);
			}
			closedir(nodes);
		}
	}

	return result;
}

#endif /* defined(OMR_PORT_NUMA_SUPPORT) */

/**
 * Initializes NUMA data by parsing the /sys/devices/system/node/ directory.
 *
 * This function must only be called *once*. It is called from omrthread_init().
 */
void
omrthread_numa_init(omrthread_library_t threadLibrary)
{
	isNumaAvailable = FALSE;
#if defined(OMR_PORT_NUMA_SUPPORT)
	/* count the NUMA node data */
	numNodes = countNumaNodes();
	if (1 < numNodes) {
		if (0 == initializeNumaNodeData(threadLibrary, numNodes)) {
			isNumaAvailable = TRUE;
		}
#ifdef OMR_VERBOSE_NUMA_NODE_DATA
		dumpNumaInfo();
#endif
	}
	/* find our current affinity mask since we will need it when removing any affinity binding from threads, later (since we still want to honour restrictions we inherited from the environment) */
	CPU_ZERO(&defaultAffinityMask);
#if __GLIBC_PREREQ(2,4) || defined(LINUXPPC)
	/*
	 * LIR 902 : On Linux PPC, rolling up tool chain level to VAC 8 on RHEL 4.
	 * The libc version on RHEL 4 requires 3 arg to sched_setaffinity.
	 * Note: this will not compile/run properly on RHEL 3.
	 */
	if (0 != sched_getaffinity(0, sizeof(cpu_set_t), &defaultAffinityMask))
#else
	if (0 != sched_getaffinity(0, &defaultAffinityMask))
#endif
	{
		/* we failed to check our affinity so we shouldn't try to use the affinity APIs */
		isNumaAvailable = FALSE;
	}

	if (!isNumaAvailable) {
		/* Clean up */
		omrthread_numa_shutdown(threadLibrary);
	}
#endif
}

void
omrthread_numa_shutdown(omrthread_library_t lib)
{
#if defined(OMR_PORT_NUMA_SUPPORT)
	isNumaAvailable = FALSE;
	numNodes = 0;
	CPU_ZERO(&defaultAffinityMask);
	omrthread_free_memory(lib, numaNodeData);
	numaNodeData = NULL;
#endif /* OMR_PORT_NUMA_SUPPORT */
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
	uintptr_t result = numNodes;
	return isNumaAvailable ? result : 0;
}

intptr_t
omrthread_numa_set_node_affinity_nolock(omrthread_t thread, const uintptr_t *nodeList, uintptr_t nodeCount, uint32_t flags)
{
	intptr_t result = 0;
#if defined(OMR_PORT_NUMA_SUPPORT)
	if (isNumaAvailable && (NULL != thread)) {
		/* bind the thread to the given list of nodes */
		/* Check the thread's state, if it's started, we can set the affinity now.
		 * Otherwise we'll need defer setting the affinity until later.
		 * NOTE: that we still want to run the loop to check that this is a
		 * valid node selection (otherwise we will report the error too late)
		 */
		uintptr_t threadIsStarted = thread->flags & (J9THREAD_FLAG_STARTED | J9THREAD_FLAG_ATTACHED);
		/* now, assemble the CPU set for the requested nodes if the thread has
		 * started (otherwise, just validate the nodes)
		 */
		uintptr_t listIndex = 0;
		cpu_set_t affinityCPUs;

		CPU_ZERO(&affinityCPUs);
		if (nodeCount > 0) {
			for (listIndex = 0; (0 == result) && (listIndex < nodeCount); listIndex++) {
				uintptr_t numaNode = nodeList[listIndex];
				if (numaNode > numNodes) {
					result = J9THREAD_NUMA_ERR;
				} else if (0 == numaNodeData[numaNode].cpu_count) {
					result = J9THREAD_NUMA_ERR_NO_CPUS_FOR_NODE;
				} else if (0 != threadIsStarted) {
					cpu_set_t *newSet = &numaNodeData[numaNode].cpu_set;

					cpuset_logical_or(&affinityCPUs, newSet);
				}
			}
			if (OMR_ARE_NO_BITS_SET(flags, J9THREAD_NUMA_OVERRIDE_DEFAULT_AFFINITY)) {
				cpuset_logical_and(&affinityCPUs, &defaultAffinityMask);
			}
		} else {
			/* remove all affinity bindings from the thread so replace it with the affinity with which we started */
			memcpy(&affinityCPUs, &defaultAffinityMask, sizeof(cpu_set_t));
		}
		if ((threadIsStarted) && (0 == result)) {
#if __GLIBC_PREREQ(2,4) || defined(LINUXPPC)
			/*
			 * LIR 902 : On Linux PPC, rolling up tool chain level to VAC 8 on RHEL 4.
			 * The libc version on RHEL 4 requires 3 arg to sched_setaffinity.
			 * Note: this will not compile/run properly on RHEL 3.
			 */
			if (0 != sched_setaffinity(thread->tid, sizeof(cpu_set_t), &affinityCPUs))
#else
			if (0 != sched_setaffinity(thread->tid, &affinityCPUs))
#endif
			{
				result = J9THREAD_NUMA_ERR;
			}
		}

		/* Update the thread->numaAffinity field if setting the affinity succeeded */
		if (0 == result) {
			uintptr_t listIndex = 0;

			memset(&thread->numaAffinity, 0x0, sizeof(thread->numaAffinity));
			for (listIndex = 0; listIndex < nodeCount; listIndex++) {
				uintptr_t numaNode = nodeList[listIndex];

				omrthread_add_node_number_to_affinity_cache(thread, numaNode);
			}
		}
	}
#endif /* OMR_PORT_NUMA_SUPPORT */
	return result;
}


intptr_t
omrthread_numa_set_node_affinity(omrthread_t thread, const uintptr_t *numaNodes, uintptr_t nodeCount, uint32_t flags)
{
	intptr_t result = 0;

	if (isNumaAvailable && (NULL != thread)) {
		THREAD_LOCK(thread, 0);
		result = omrthread_numa_set_node_affinity_nolock(thread, numaNodes, nodeCount, flags);
		THREAD_UNLOCK(thread);
	}

	return result;
}

intptr_t
omrthread_numa_get_node_affinity(omrthread_t thread, uintptr_t *numaNodes, uintptr_t *nodeCount)
{
	intptr_t result = 0;
	uintptr_t nodesSoFar = 0;

#if defined(OMR_PORT_NUMA_SUPPORT)
	uintptr_t incomingListSize = *nodeCount;

	if (isNumaAvailable && (NULL != thread)) {
		uintptr_t threadIsStarted = 0;

		THREAD_LOCK(thread, 0);
		threadIsStarted = thread->flags & (J9THREAD_FLAG_STARTED | J9THREAD_FLAG_ATTACHED);
		if (0 != threadIsStarted) {
			cpu_set_t affinityCPUs;
			CPU_ZERO(&affinityCPUs);

			/* Get the affinity for the current thread. Note that we can't simply lookup
			 * the thread->numaAffinity field, since it's possible for the affinity to be changed
			 * by an external program (see: http://www.linuxjournal.com/article/6799)
			 */

#if __GLIBC_PREREQ(2,4) || defined(LINUXPPC)
			/*
			 * LIR 902 : On Linux PPC, rolling up tool chain level to VAC 8 on RHEL 4.
			 * The libc version on RHEL 4 requires 3 arg to sched_getaffinity.
			 * Note: this will not compile/run properly on RHEL 3.
			 */
			if (0 == sched_getaffinity(thread->tid, sizeof(cpu_set_t), &affinityCPUs))
#else /* defined(LINUXPPC) */
			if (0 == sched_getaffinity(thread->tid, &affinityCPUs))
#endif /* defined(LINUXPPC) */
			{
				/* Now try matching it up with CPUs from known NUMA nodes */
				uintptr_t node = 1;
				uintptr_t numaMaxNode = omrthread_numa_get_max_node();
				memset(&thread->numaAffinity, 0x0, sizeof(thread->numaAffinity));

				/* Look through all the numa nodes and try to see if any matches this affinity mask */
				for (node = 1; node <= numaMaxNode; node++) {
					/* Validate that this node has CPUs associated with it.
					 * If not we should not use this one in the check otherwise
					 * we may incorrectly report this node for threads that have
					 * no affinity set.
					 */
					if (numaNodeData[node].cpu_count > 0) {
						cpu_set_t *numaNodeCPUs = &numaNodeData[node].cpu_set;
						if (cpuset_subset_or_equal(&affinityCPUs, numaNodeCPUs)) {
							if (nodesSoFar < incomingListSize) {
								numaNodes[nodesSoFar] = node;
							}
							nodesSoFar += 1;
							omrthread_add_node_number_to_affinity_cache(thread, node);
						}
					}
				}
				/* handle the case where we aren't on any node - which implies the special node 0 (the global node) */
				if (0 == nodesSoFar) {
					numaNodes[nodesSoFar] = 0;
					nodesSoFar += 1;
				}
			} else {
				/* couldn't find the thread affinity so return error */
				result = -1;
			}
		} else {
			/* Thread hasn't started yet. Return the deferred affinity */
			uintptr_t node = 0;
			uintptr_t numaMaxNode = omrthread_numa_get_max_node();

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
		}
		THREAD_UNLOCK(thread);
	}
#endif /* OMR_PORT_NUMA_SUPPORT */
	*nodeCount = nodesSoFar;
	return result;
}

#ifdef OMR_VERBOSE_NUMA_NODE_DATA
 void
dumpNumaInfo() {
	uintptr_t numaMaxNode = omrthread_numa_get_max_node();
	uint32_t node = 0;

	for (node = 0; node <= numaMaxNode; node++) {
		uint32_t i = 0;
		printf("Node %d cpus:", node);
		cpu_set_t *numaNodeCPUs = &numaNodeData[node].cpu_set;
		for (i = 0; i < CPU_SETSIZE; i++) {
			if (CPU_ISSET(i, numaNodeCPUs)) {
				printf(" %d ", i);
			}
		}
		printf("\n");
	}

}
#endif

uintptr_t
omrthread_numa_get_current_node()
{
    unsigned node = 0;
#if defined(OMR_PORT_NUMA_SUPPORT)
	/* On some older kernels the syscall appears to be SYS_get_cpu rather than SYS_getcpu */
#if !defined(SYS_getcpu)
#define SYS_getcpu SYS_get_cpu
#endif

    if (0 == syscall(SYS_getcpu, NULL, &node, NULL)) {
        ++node;
    } else {
        node = 0;
    }
#endif
    return node;
}
