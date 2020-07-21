/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
#ifndef omrcgroup_h
#define omrcgroup_h

#if defined(LINUX)

/**
 * Stores memory usage statistics of the cgroup. These stats are collected from the files present
 * in memory resource controller of the cgroup.
 * Refer https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt for more details.
 *
 * Parameter which is not available is set to OMRPORT_MEMINFO_NOT_AVAILABLE by default.
 */
typedef struct OMRCgroupMemoryInfo {
	uint64_t memoryLimit; /**< memory limit in bytes (as in memory.limit_in_bytes file)*/
	uint64_t memoryUsage; /**< current memory usage in bytes (as in memory.usage_in_bytes file)*/
	uint64_t memoryAndSwapLimit; /**< memory + swap limit in bytes (as in memory.memsw.limit_in_bytes file)*/
	uint64_t memoryAndSwapUsage; /**< current memory + swap usage in bytes (as in memory.memsw.usage_in_bytes file) */
	uint64_t swappiness; /**< indicates kernel aggressivenss (0-100) in swapping memory pages */
	uint64_t cached; /**< page cache memory (as in memory.stat file)*/
} OMRCgroupMemoryInfo;

#endif /* defined(LINUX) */

#endif /* omrcgroup_h */
