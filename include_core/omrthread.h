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

#ifndef OMRTHREAD_H
#define OMRTHREAD_H

/*
 * @ddr_namespace: default
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "omrcomp.h"

typedef uintptr_t omrthread_tls_key_t;

#define J9THREAD_PROC

typedef int(J9THREAD_PROC *omrthread_entrypoint_t)(void *);
typedef void(J9THREAD_PROC *omrthread_tls_finalizer_t)(void *);

/* Must be wide enough to support error codes from any platform */
typedef intptr_t omrthread_os_errno_t;

typedef struct J9Thread *omrthread_t;
typedef struct J9ThreadMonitor *omrthread_monitor_t;
typedef struct J9Semaphore *j9sem_t;

#include "omrthread_generated.h"

#define J9THREAD_PRIORITY_MIN  0
#define J9THREAD_PRIORITY_USER_MIN  1
#define J9THREAD_PRIORITY_NORMAL  5
#define J9THREAD_PRIORITY_USER_MAX  10
#define J9THREAD_PRIORITY_MAX 11

/*
A thread priority calue can be one of three possible types: a java thread priority, a omrthread thread priority or a native operating system priority.
A single integer can be used to store both the priority value and type, and these macros allow for this storage and conversion to the desired java
or omrthread priority value.
*/
#define PRIORITY_INDICATOR_VALUE(priority) ((priority) & 0x00ffffff)
#define PRIORITY_INDICATOR_TYPE(priority) ((priority) >> 24)
#define PRIORITY_INDICATOR_ADJUSTED_TYPE(type) ((type) << 24)
#define PRIORITY_INDICATOR_J9THREAD_PRIORITY 0
#define PRIORITY_INDICATOR_JAVA_PRIORITY 1
#define PRIORITY_INDICATOR_NATIVE_PRIORITY 2

#define getJavaPriorityFromIndicator(vm, value) ((PRIORITY_INDICATOR_TYPE(value) == PRIORITY_INDICATOR_JAVA_PRIORITY) ? PRIORITY_INDICATOR_VALUE(value) : \
	((PRIORITY_INDICATOR_TYPE(value) == PRIORITY_INDICATOR_NATIVE_PRIORITY) ? (vm)->j9Thread2JavaPriorityMap[omrthread_map_native_priority(PRIORITY_INDICATOR_VALUE(value))] : \
	(vm)->j9Thread2JavaPriorityMap[PRIORITY_INDICATOR_VALUE(value)]))

#define getJ9ThreadPriorityFromIndicator(vm, value) ((PRIORITY_INDICATOR_TYPE(value) == PRIORITY_INDICATOR_JAVA_PRIORITY) ? (vm)->java2J9ThreadPriorityMap[PRIORITY_INDICATOR_VALUE(value)] : \
	((PRIORITY_INDICATOR_TYPE(value) == PRIORITY_INDICATOR_NATIVE_PRIORITY) ? omrthread_map_native_priority(PRIORITY_INDICATOR_VALUE(value)) : PRIORITY_INDICATOR_VALUE(value)))




#define J9THREAD_LOCKING_DEFAULT	0 /* default locking policy for platform */
#define J9THREAD_LOCKING_NO_DATA	(-1)	/* if no policy data is provided */


#define J9THREAD_LIB_FLAG_FAST_NOTIFY  1
#define J9THREAD_LIB_FLAG_ATTACHED_THREAD_EXITED  2 /* VMDESIGN 1645 - currently unused */
#define J9THREAD_LIB_FLAG_NO_SCHEDULING  4
#define J9THREAD_LIB_FLAG_TRACING_ENABLED  8
#define J9THREAD_LIB_FLAG_NOTIFY_POLICY_BROADCAST  0x10
#define J9THREAD_LIB_FLAG_SECONDARY_SPIN_OBJECT_MONITORS_ENABLED  0x20
#define J9THREAD_LIB_FLAG_ADAPTIVE_SPIN_KEEP_SAMPLING  0x40
#define J9THREAD_LIB_FLAG_REALTIME_SCHEDULING_ENABLED  0x80
#define J9THREAD_LIB_FLAG_CUSTOM_ADAPTIVE_SPIN_ENABLED  0x2000
#define J9THREAD_LIB_FLAG_JLM_ENABLED  0x4000
#define J9THREAD_LIB_FLAG_JLM_TIME_STAMPS_ENABLED  0x8000
#define J9THREAD_LIB_FLAG_JLMHST_ENABLED  0x10000
#define J9THREAD_LIB_FLAG_JLM_HAS_BEEN_ENABLED  0x20000
#define J9THREAD_LIB_FLAG_JLM_ENABLED_ALL  (J9THREAD_LIB_FLAG_JLM_ENABLED|J9THREAD_LIB_FLAG_JLM_TIME_STAMPS_ENABLED|J9THREAD_LIB_FLAG_JLMHST_ENABLED)
#define J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED  0x100000
#define J9THREAD_LIB_FLAG_JLM_SLOW_SAMPLING_ENABLED  0x200000
#define J9THREAD_LIB_FLAG_JLM_INFO_SAMPLING_ENABLED  (J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED|J9THREAD_LIB_FLAG_JLM_SLOW_SAMPLING_ENABLED)
#define J9THREAD_LIB_FLAG_JLM_INIT_DATA_STRUCTURES  (J9THREAD_LIB_FLAG_JLM_ENABLED|J9THREAD_LIB_FLAG_JLM_INFO_SAMPLING_ENABLED|J9THREAD_LIB_FLAG_CUSTOM_ADAPTIVE_SPIN_ENABLED)
#define J9THREAD_LIB_FLAG_DESTROY_MUTEX_ON_MONITOR_FREE  0x400000
#define J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR  0x800000
#define J9THREAD_LIB_FLAG_NO_DEFAULT_AFFINITY  0x1000000

#define J9THREAD_LIB_YIELD_ALGORITHM_SCHED_YIELD  0
#define J9THREAD_LIB_YIELD_ALGORITHM_CONSTANT_USLEEP  2
#define J9THREAD_LIB_YIELD_ALGORITHM_INCREASING_USLEEP  3

#define OMRTHREAD_MINIMUM_SPIN_THREADS 1
#define OMRTHREAD_MINIMUM_WAKE_THREADS 1
#define OMRTHREAD_IGNORE_SPIN_THREAD_BOUND 0

#include "thread_api.h"


#define omrthread_monitor_init(pMon,flags)  omrthread_monitor_init_with_name(pMon,flags, #pMon)

#ifdef __cplusplus
}
#endif

#endif /* OMRTHREAD_H */
