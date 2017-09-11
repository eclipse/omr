/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMRAGENT_INTERNAL_H
#define OMRAGENT_INTERNAL_H

/*
 * @ddr_namespace: default
 */

/**
 * This header file is used internally by OMR library. OMR agents should not use this header file.
 * The structures defined in this file may be changed without notice.
 */

#if defined(WIN32)
/* pdh.h include windows.h which defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win32_
#include <pdh.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#endif /* defined(WIN32) */

#include "omrcomp.h"
#include "omragent.h"
#include "thread_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OMR_TI_ENTER_FROM_VM_THREAD(_omrVMThread) \
		BOOLEAN _omrTIAccessMutex_taken = FALSE; \
		if (NULL != (_omrVMThread)) { \
			omrthread_monitor_enter((_omrVMThread)->_vm->_omrTIAccessMutex); \
			_omrTIAccessMutex_taken = TRUE; \
		}

#define OMR_TI_RETURN(_omrVMThread, _rc) \
		if (TRUE == _omrTIAccessMutex_taken) { \
			omrthread_monitor_exit((_omrVMThread)->_vm->_omrTIAccessMutex); \
			return (_rc); \
		} else { \
			return (_rc); \
		}

typedef struct OMRSysInfoProcessCpuTime {
	int64_t timestampNS; 			/* time in nanoseconds from a fixed but arbitrary point in time */
	int64_t systemCpuTimeNS;		/* system time, in nanoseconds, consumed by this process on all CPUs. */
	int64_t userCpuTimeNS;			/* user time, in nanoseconds, consumed by this process on all CPUs. */
	int32_t numberOfCpus;			/* number of CPUs */
} OMRSysInfoProcessCpuTime;

typedef enum OMRSysInfoCpuLoadCallStatus {
	NO_HISTORY,
	SUPPORTED,
	CPU_LOAD_ERROR_VALUE,
	INSUFFICIENT_PRIVILEGE,
	UNSUPPORTED,
	OMRSysInfoCpuLoadCallStatus_EnsureWideEnum = 0x1000000	/* force 4-byte enum */
} OMRSysInfoCpuLoadCallStatus;

typedef struct OMR_SysInfo {
	uintptr_t systemCpuTimeNegativeElapsedTimeCount;		/* consecutive negative elapsed time count for system CPU load calculation */
	uintptr_t processCpuTimeNegativeElapsedTimeCount;		/* consecutive negative elapsed time count for process CPU load calculation */
	OMRSysInfoCpuLoadCallStatus systemCpuLoadCallStatus;		/* whether GetSystemCpuLoad is supported on this platform or user have sufficient rights */
	OMRSysInfoCpuLoadCallStatus processCpuLoadCallStatus;		/* whether GetProcessCpuLoad is supported on this platform or user have sufficient rights */

	J9SysinfoCPUTime oldestSystemCpuTime;			/* the oldest GetSystemCpuLoad() call record */
	J9SysinfoCPUTime interimSystemCpuTime; 			/* the 2nd oldest GetSystemCpuLoad() call record */
	OMRSysInfoProcessCpuTime oldestProcessCpuTime;	/* the oldest GetProcessCpuLoad() call record */
	OMRSysInfoProcessCpuTime interimProcessCpuTime;	/* the 2nd oldest GetProcessCpuLoad() call record */

	omrthread_monitor_t syncSystemCpuLoad;
	omrthread_monitor_t syncProcessCpuLoad;
} OMR_SysInfo;

omr_error_t omrtiBindCurrentThread(OMR_VM *vm, const char *threadName, OMR_VMThread **vmThread);
omr_error_t omrtiUnbindCurrentThread(OMR_VMThread *vmThread);
omr_error_t omrtiRegisterRecordSubscriber(OMR_VMThread *vmThread, char const *description,
	utsSubscriberCallback subscriberFunc, utsSubscriberAlarmCallback alarmFunc, void *userData, UtSubscription **subscriptionID);
omr_error_t omrtiDeregisterRecordSubscriber(OMR_VMThread *vmThread, UtSubscription *subscriptionID);
omr_error_t omrtiFlushTraceData(OMR_VMThread *vmThread);
omr_error_t omrtiGetTraceMetadata(OMR_VMThread *vmThread, void **data, int32_t *length);
omr_error_t omrtiSetTraceOptions(OMR_VMThread *vmThread, char const *opts[]);

omr_error_t omrtiGetSystemCpuLoad(OMR_VMThread *vmThread, double *systemCpuLoad);
omr_error_t omrtiGetProcessCpuLoad(OMR_VMThread *vmThread, double *processCpuLoad);
omr_error_t omrtiGetMemoryCategories(OMR_VMThread *vmThread, int32_t max_categories, OMR_TI_MemoryCategory *categories_buffer, int32_t *written_count_ptr, int32_t *total_categories_ptr);
omr_error_t omrtiGetFreePhysicalMemorySize(OMR_VMThread *vmThread, uint64_t *freePhysicalMemorySize);
omr_error_t omrtiGetProcessVirtualMemorySize(OMR_VMThread *vmThread, uint64_t *processVirtualMemorySize);
omr_error_t omrtiGetProcessPhysicalMemorySize(OMR_VMThread *vmThread, uint64_t *processPhysicalMemorySize);
omr_error_t omrtiGetProcessPrivateMemorySize(OMR_VMThread *vmThread, uint64_t *processPrivateMemorySize);

omr_error_t omrtiGetMethodDescriptions(OMR_VMThread *vmThread, void **methodArray, size_t methodArrayCount,
	OMR_SampledMethodDescription *methodDescriptions, char *nameBuffer, size_t nameBytes,
	size_t *firstRetryMethod, size_t *nameBytesRemaining);
omr_error_t omrtiGetMethodProperties(OMR_VMThread *vmThread, size_t *numProperties, const char *const **propertyNames, size_t *sizeofSampledMethodDesc);

/* This is an internal API which is subject to change without notice. Agents must not use this API. */
typedef struct OMR_ThreadAPI {
	intptr_t (*omrthread_create)(omrthread_t *handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend, omrthread_entrypoint_t entrypoint, void *entryarg);
	intptr_t (*omrthread_monitor_init_with_name)(omrthread_monitor_t *handle, uintptr_t flags, const char *name);
	intptr_t (*omrthread_monitor_destroy)(omrthread_monitor_t monitor);
	intptr_t (*omrthread_monitor_enter)(omrthread_monitor_t monitor);
	intptr_t (*omrthread_monitor_exit)(omrthread_monitor_t monitor);
	intptr_t (*omrthread_monitor_wait)(omrthread_monitor_t monitor);
	intptr_t (*omrthread_monitor_notify_all)(omrthread_monitor_t monitor);
	intptr_t (*omrthread_attr_init)(omrthread_attr_t *attr);
	intptr_t (*omrthread_attr_destroy)(omrthread_attr_t *attr);
	intptr_t (*omrthread_attr_set_detachstate)(omrthread_attr_t *attr, omrthread_detachstate_t detachstate);
	intptr_t (*omrthread_attr_set_category)(omrthread_attr_t *attr, uint32_t category);
	intptr_t (*omrthread_create_ex)(omrthread_t *handle, const omrthread_attr_t *attr, uintptr_t suspend, omrthread_entrypoint_t entrypoint, void *entryarg);
	intptr_t (*omrthread_join)(omrthread_t thread);
} OMR_ThreadAPI;

#ifdef __cplusplus
}
#endif

#endif /* OMRAGENT_INTERNAL_H */
