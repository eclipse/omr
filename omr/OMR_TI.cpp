/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <string.h>

#include "omrport.h"
#include "omrthread.h"
#include "omragent_internal.h"
#include "omrtrace.h"
#include "ut_omrti.h"

#include "OMR_MethodDictionary.hpp"
#include "OMR_VM.hpp"

#define MINIMUM_CPU_LOAD_INTERVAL J9CONST_I64(10000000)
#define MAXIMUM_NEGATIVE_ELAPSED_TIME_COUNT 3
#define UTSINTERFACE_FROM_OMRVMTHREAD(vmThread) ((vmThread)->_vm->_trcEngine? &(vmThread)->_vm->_trcEngine->omrTraceIntfS : NULL)

typedef enum OMRSysInfoCalculateCpuLoadError {
	OK,
	ELAPSED_TIME_NEGATIVE,
	ELAPSED_TIME_TOO_SMALL,
	NEGATIVE_INTERVAL
} OMRSysInfoCalculateCpuLoadError;

static uint32_t indexFromCategoryCode(uintptr_t categories_mapping_size, uint32_t cc);
static uintptr_t omrtiGetMemoryCategoriesCallback(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *state);
static uintptr_t omrtiCountMemoryCategoriesCallback(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *state);
static uintptr_t omrtiCalculateSlotsForCategoriesMappingCallback(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *state);
static void fillInChildAndSiblingCategories(OMR_TI_MemoryCategory *categories_buffer, int32_t written_count);
static void fillInCategoryDeepCounters(OMR_TI_MemoryCategory *category);

static omr_error_t getOMRSysInfoProcessCpuTime(OMR_VM *omrVM, OMRSysInfoProcessCpuTime *sysInfo, OMRSysInfoCpuLoadCallStatus *status);
static OMRSysInfoCalculateCpuLoadError calculateProcessCpuLoad(OMRSysInfoProcessCpuTime const *endRecord, OMRSysInfoProcessCpuTime const *startRecord, double *cpuLoad);
static omr_error_t getOMRSysInfoSystemCpuTime(OMR_VM *omrVM, J9SysinfoCPUTime *sysInfo, OMRSysInfoCpuLoadCallStatus *status);
static OMRSysInfoCalculateCpuLoadError calculateSystemCpuLoad(J9SysinfoCPUTime const *endRecord, J9SysinfoCPUTime const *startRecord, double *cpuLoad);
static int64_t computeTimeInterval(const int64_t endNS, const int64_t startNS);

omr_error_t
omrtiBindCurrentThread(OMR_VM *vm, const char *threadName, OMR_VMThread **vmThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	if ((NULL == vm) || (NULL == vmThread)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		/* This function will be called from unattached threads. We need
		 * to attach this threads before we can take the TI lock.
		 * (omrthread_attach_ex/detach maintain an attach count so we won't detach
		 * a thread that gets attached again in OMR_Glue_BindCurrentThread)
		 */
		omrthread_monitor_enter(vm->_omrTIAccessMutex);
		rc = OMR_Glue_BindCurrentThread(vm, threadName, vmThread);
		omrthread_monitor_exit(vm->_omrTIAccessMutex);
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}
	return rc;
}

omr_error_t
omrtiUnbindCurrentThread(OMR_VMThread *vmThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	/* We must keep the omrthread attached so that we can exit the TI lock. */
	if ((NULL == vmThread) || (NULL == vmThread->_vm)) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		OMR_VM *omrVM = vmThread->_vm;
		omrthread_monitor_enter(omrVM->_omrTIAccessMutex);
		rc = OMR_Glue_UnbindCurrentThread(vmThread);
		omrthread_monitor_exit(omrVM->_omrTIAccessMutex);
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}
	return rc;
}

omr_error_t
omrtiRegisterRecordSubscriber(OMR_VMThread *vmThread, char const *description,
	utsSubscriberCallback subscriberFunc, utsSubscriberAlarmCallback alarmFunc, void *userData, UtSubscription **subscriptionID)
{
	omr_error_t rc = OMR_ERROR_NONE;

	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if ((NULL == subscriberFunc) || (NULL == subscriptionID) || (NULL == description)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		/*
		 * OMRTODO
		 * If the current thread is not attached to the trace engine, we can't guarantee that
		 * the serverf pointer is consistent. Another thread could be in the process of deleting the
		 * trace engine.
		 *
		 * This situation could occur if the trace engine were started & shutdown, and the current
		 * thread was bound to the OMR_VM after trace shutdown, so it has a valid OMR_VMThread, but
		 * is not attached to the trace engine.
		 *
		 * In practice, this is unlikely to occur because we unload agents before shutting down the trace engine.
		 */
		OMR_TraceInterface *serverf = UTSINTERFACE_FROM_OMRVMTHREAD(vmThread);
		if (NULL == serverf) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		} else {
			rc = serverf->RegisterRecordSubscriber(OMR_TRACE_THREAD_FROM_VMTHREAD(vmThread), description, subscriberFunc,
												   alarmFunc, userData, (UtSubscription **)subscriptionID);
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}

omr_error_t
omrtiDeregisterRecordSubscriber(OMR_VMThread *vmThread, UtSubscription *subscriptionID)
{
	omr_error_t rc = OMR_ERROR_NONE;

	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == subscriptionID) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		/*
		 * OMRTODO
		 * If the current thread is not attached to the trace engine, we can't guarantee that
		 * the serverf pointer is consistent. Another thread could be in the process of deleting the
		 * trace engine.
		 *
		 * This situation could occur if the trace engine were started & shutdown, and the current
		 * thread was bound to the OMR_VM after trace shutdown, so it has a valid OMR_VMThread, but
		 * is not attached to the trace engine.
		 *
		 * In practice, this is unlikely to occur because we unload agents before shutting down the trace engine.
		 */
		OMR_TraceInterface *serverf = UTSINTERFACE_FROM_OMRVMTHREAD(vmThread);
		/* if there is no server, no work needs to be done */
		if (NULL != serverf) {
			rc = serverf->DeregisterRecordSubscriber(OMR_TRACE_THREAD_FROM_VMTHREAD(vmThread), subscriptionID);
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}

omr_error_t
omrtiFlushTraceData(OMR_VMThread *vmThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else {
		OMR_TraceInterface *serverf = UTSINTERFACE_FROM_OMRVMTHREAD(vmThread);
		if (NULL == serverf) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		} else {
			rc = serverf->FlushTraceData(OMR_TRACE_THREAD_FROM_VMTHREAD(vmThread));
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}

omr_error_t
omrtiGetTraceMetadata(OMR_VMThread *vmThread, void **data, int32_t *length)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if ((NULL == data) || (NULL == length)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMR_TraceInterface *serverf = UTSINTERFACE_FROM_OMRVMTHREAD(vmThread);
		if (NULL == serverf) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		} else {
			rc = serverf->GetTraceMetadata(data, length);
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}

omr_error_t
omrtiSetTraceOptions(OMR_VMThread *vmThread, char const *opts[])
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == opts) {
		/* Nothing to do. Not an error. */
	} else {
		OMR_TraceInterface *serverf = UTSINTERFACE_FROM_OMRVMTHREAD(vmThread);
		if (NULL == serverf) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		} else {
			rc = serverf->SetOptions(OMR_TRACE_THREAD_FROM_VMTHREAD(vmThread), opts);
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}

/**
 * Compute (end - start) time interval.
 *
 * Sanity checks to avoid returning negative or unreasonably large intervals if the timer goes backwards.
 *
 * @param[in] endNS end of interval in nanoseconds
 * @param[in] startNS start of interval in nanoseconds
 * @return (end - start) interval in nanoseconds, or a negative value if interval is badly defined
 */
static int64_t
computeTimeInterval(const int64_t endNS, const int64_t startNS)
{
	uint64_t intervalNS = (uint64_t)endNS - (uint64_t)startNS;

	/*
	 * endNS could be less than startNS if the timer wrapped.  However, modular arithmetic would
	 * give us a valid positive interval for (endNS - startNS).
	 *
	 * Excessively large intervals (>292 years approximately or I_64_MAX ns) indicate the timer has gone backwards.
	 * In this case, the return value will be negative when interpreted as an int64_t.
	 */
	return (int64_t)intervalNS;
}

/**
 * This function fills in a record of J9SysinfoCPUTime or sets systemCpuLoadCallStatus
 * @param[in] omrVM the current OMR VM
 * @param[in,out] sysInfo record of J9SysinfoCPUTime, it cannot be NULL
 * @param[in,out] status whether GetSystemCpuLoad is supported on this platform or user have sufficient rights, it cannot be NULL
 * @return OMR error code
 */
static omr_error_t
getOMRSysInfoSystemCpuTime(OMR_VM *omrVM, J9SysinfoCPUTime *systemCpuTime, OMRSysInfoCpuLoadCallStatus *status)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	omr_error_t rc = OMR_ERROR_NONE;
	intptr_t portLibraryStatus = 0;

	portLibraryStatus = omrsysinfo_get_CPU_utilization(systemCpuTime);
	if (0 != portLibraryStatus) {
		switch (portLibraryStatus) {
		case OMRPORT_ERROR_SYSINFO_INSUFFICIENT_PRIVILEGE:
			*status = INSUFFICIENT_PRIVILEGE;
			break;
		case OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED:
			*status = UNSUPPORTED;
			break;
		default:
			*status = CPU_LOAD_ERROR_VALUE;
			break;
		}
		rc = OMR_ERROR_NOT_AVAILABLE;
	}
	return rc;
}

/**
 * This function returns a record of getOMRSysInfoProcessCpuTime or sets processCpuLoadCallStatus
 * @param[in] omrVM the current OMR VM
 * @param[in,out] vm record of OMRSysInfoProcessCpuTime, it cannot be NULL
 * @param[in,out] status whether GetProcessCpuLoad is supported on this platform or user have sufficient rights, it cannot be NULL
 * @return OMR error code
 */
static omr_error_t
getOMRSysInfoProcessCpuTime(OMR_VM *omrVM, OMRSysInfoProcessCpuTime *processCpuTime, OMRSysInfoCpuLoadCallStatus *status)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	omr_error_t rc = OMR_ERROR_NONE;
	J9SysinfoCPUTime cpuTime;
	intptr_t portLibraryStatus = omrsysinfo_get_CPU_utilization(&cpuTime);

	if (0 != portLibraryStatus) {
		switch (portLibraryStatus) {
		case OMRPORT_ERROR_SYSINFO_INSUFFICIENT_PRIVILEGE:
			*status = INSUFFICIENT_PRIVILEGE;
			break;
		case OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED:
			*status = UNSUPPORTED;
			break;
		default:
			*status = CPU_LOAD_ERROR_VALUE;
			break;
		}
		rc = OMR_ERROR_NOT_AVAILABLE;
	} else {
		omrthread_process_time_t processTime;
		intptr_t getProcessTimeStatus = omrthread_get_process_times(&processTime);

		if (0 != getProcessTimeStatus) {
			switch (getProcessTimeStatus) {
			case -1:
				*status = UNSUPPORTED;
				break;
			case -2: /* fall through */
			default:
				*status = CPU_LOAD_ERROR_VALUE;
				break;
			}
			rc = OMR_ERROR_NOT_AVAILABLE;
		} else {
			processCpuTime->numberOfCpus = cpuTime.numberOfCpus;
			processCpuTime->timestampNS = cpuTime.timestamp;
			processCpuTime->systemCpuTimeNS = processTime._systemTime;
			processCpuTime->userCpuTimeNS = processTime._userTime;
		}
	}
	return rc;
}

/**
 * This function calculates system CPU load
 * @param[in] endRecord the end point of the period used to calculate the CPU load, it cannot be NULL
 * @param[in] startRecord the start point of the period used to calculate the CPU load, it cannot be NULL
 * @param[in,out] cpuLoad the system CPU load between the observed time period, it cannot be NULL
 * @return an OMRSysInfoCalculateCpuLoadError error code
 */
static OMRSysInfoCalculateCpuLoadError
calculateSystemCpuLoad(J9SysinfoCPUTime const *endRecord, J9SysinfoCPUTime const *startRecord, double *cpuLoad)
{
	OMRSysInfoCalculateCpuLoadError rc = OK;
	int64_t timestampDelta = computeTimeInterval(endRecord->timestamp, startRecord->timestamp);
	if (timestampDelta < 0) {
		rc = ELAPSED_TIME_NEGATIVE;
	} else if (timestampDelta < MINIMUM_CPU_LOAD_INTERVAL) {
		rc = ELAPSED_TIME_TOO_SMALL;
	} else if (endRecord->cpuTime < startRecord->cpuTime) {
		rc = NEGATIVE_INTERVAL;
	} else {
		int64_t cpuTimeDelta = endRecord->cpuTime - startRecord->cpuTime;
		double result = (double)cpuTimeDelta / (double)(endRecord->numberOfCpus * timestampDelta);
		/* cpuload is in [0, 1] */
		*cpuLoad = OMR_MIN(result, 1.0);
	}
	return rc;
}

/**
 * This function calculates process CPU load
 * @param[in] endRecord the end point of the period used to calculate the CPU load, it cannot be NULL
 * @param[in] startRecord the start point of the period used to calculate the CPU load, it cannot be NULL
 * @param[in,out] cpuLoad the process CPU load between the observed time period, it cannot be NULL
 * @return a OMRSysInfoCalculateCpuLoadError error code
 */
static OMRSysInfoCalculateCpuLoadError
calculateProcessCpuLoad(OMRSysInfoProcessCpuTime const *endRecord, OMRSysInfoProcessCpuTime const *startRecord, double *cpuLoad)
{
	OMRSysInfoCalculateCpuLoadError rc = OK;
	int64_t timestampDelta = computeTimeInterval(endRecord->timestampNS, startRecord->timestampNS);
	if (timestampDelta < 0) {
		rc = ELAPSED_TIME_NEGATIVE;
	} else if (timestampDelta < MINIMUM_CPU_LOAD_INTERVAL) {
		rc = ELAPSED_TIME_TOO_SMALL;
	} else if (endRecord->userCpuTimeNS < startRecord->userCpuTimeNS) {
		rc = NEGATIVE_INTERVAL;
	} else {
		int64_t systemCpuTimeDelta = endRecord->systemCpuTimeNS - startRecord->systemCpuTimeNS;
		int64_t userCpuTimeDelta = endRecord->userCpuTimeNS - startRecord->userCpuTimeNS;
		int64_t processTimeDelta = systemCpuTimeDelta + userCpuTimeDelta;
		if (processTimeDelta < systemCpuTimeDelta) {
			/* operation processTimeDelta = systemCpuTimeDelta + userCpuTimeDelta overflow, set cpuLoad to 1 in this case */
			*cpuLoad = 1.0;
		} else {
			double result = (double)processTimeDelta / (double)(endRecord->numberOfCpus * timestampDelta);
			/* cpuload is in [0, 1] */
			*cpuLoad = OMR_MIN(result, 1.0);
		}
	}
	return rc;
}

omr_error_t
omrtiGetSystemCpuLoad(OMR_VMThread *vmThread, double *systemCpuLoad)
{
	omr_error_t ret = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		ret = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == systemCpuLoad || NULL == vmThread->_vm) {
		ret = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else if (NULL == vmThread->_vm->sysInfo) {
		ret = OMR_ERROR_NOT_AVAILABLE;
	} else if (0 != omrthread_monitor_enter(vmThread->_vm->sysInfo->syncSystemCpuLoad)) {
		ret = OMR_ERROR_INTERNAL;
	} else {
		OMR_SysInfo *curSysInfo = vmThread->_vm->sysInfo;
		if ((SUPPORTED == curSysInfo->systemCpuLoadCallStatus)
			|| (NO_HISTORY == curSysInfo->systemCpuLoadCallStatus)
		) {
			J9SysinfoCPUTime newestSystemCpuTime;
			ret = getOMRSysInfoSystemCpuTime(vmThread->_vm, &newestSystemCpuTime, &curSysInfo->systemCpuLoadCallStatus);
			if (OMR_ERROR_NONE != ret) {
				/* it is not supported on this platform or user has insufficient rights */
				ret = OMR_ERROR_NOT_AVAILABLE;
			} else if (NO_HISTORY == curSysInfo->systemCpuLoadCallStatus) {
				/* first call to this method */
				memcpy(&curSysInfo->oldestSystemCpuTime, &newestSystemCpuTime, sizeof(J9SysinfoCPUTime));
				memcpy(&curSysInfo->interimSystemCpuTime, &newestSystemCpuTime, sizeof(J9SysinfoCPUTime));
				curSysInfo->systemCpuLoadCallStatus = SUPPORTED;
				ret = OMR_ERROR_RETRY;
			} else {
				OMRSysInfoCalculateCpuLoadError rc = OK;
				rc = calculateSystemCpuLoad(&newestSystemCpuTime, &curSysInfo->interimSystemCpuTime, systemCpuLoad);
				if (OK == rc) {
					/* discard the oldestSystemCpuTime, replace it with interimSystemCpuTime and save newestSystemCpuTime as the new interimSystemCpuTime. */
					memcpy(&curSysInfo->oldestSystemCpuTime, &curSysInfo->interimSystemCpuTime, sizeof(J9SysinfoCPUTime));
					memcpy(&curSysInfo->interimSystemCpuTime, &newestSystemCpuTime, sizeof(J9SysinfoCPUTime));
				} else {
					/* interimSystemCpuTime or newestSystemCpuTime is not valid, or the elapsed time between them is too small. */
					if (NEGATIVE_INTERVAL == rc) {
						/* discard the interimSystemCpuLoad and replace it with newestSystemCpuTime. */
						memcpy(&curSysInfo->interimSystemCpuTime, &newestSystemCpuTime, sizeof(J9SysinfoCPUTime));
					}
					/* attempt to recompute using the oldestSystemCpuTime. */
					rc = calculateSystemCpuLoad(&newestSystemCpuTime, &curSysInfo->oldestSystemCpuTime, systemCpuLoad);
					if (OK == rc) {
						curSysInfo->systemCpuTimeNegativeElapsedTimeCount = 0;
					} else if (NEGATIVE_INTERVAL == rc) {
						/* discard oldSystemCpuLoad and replace it with newestSystemCpuTime. */
						memcpy(&curSysInfo->oldestSystemCpuTime, &newestSystemCpuTime, sizeof(J9SysinfoCPUTime));
						curSysInfo->systemCpuTimeNegativeElapsedTimeCount = 0;
						ret = OMR_ERROR_INTERNAL;
					} else if (ELAPSED_TIME_TOO_SMALL == rc) {
						/* return OMR_ERROR_RETRY if interval between endRcord and oldestSystemCpuTime is still too small */
						curSysInfo->systemCpuTimeNegativeElapsedTimeCount = 0;
						ret = OMR_ERROR_RETRY;
					} else if (ELAPSED_TIME_NEGATIVE == rc) {
						if (MAXIMUM_NEGATIVE_ELAPSED_TIME_COUNT == curSysInfo->systemCpuTimeNegativeElapsedTimeCount) {
							/* if this case found consecutively for MAXIMUM_NEGATIVE_ELAPSED_TIME_COUNT times, discard the oldestSystemCpuTime and replace it with newestSystemCpuTime. */
							memcpy(&curSysInfo->oldestSystemCpuTime, &newestSystemCpuTime, sizeof(J9SysinfoCPUTime));
							curSysInfo->systemCpuTimeNegativeElapsedTimeCount = 0;
						} else {
							curSysInfo->systemCpuTimeNegativeElapsedTimeCount += 1;
						}
						ret = OMR_ERROR_RETRY;
					}
				}
			}
		} else {
			ret = OMR_ERROR_NOT_AVAILABLE;
		}
		omrthread_monitor_exit(vmThread->_vm->sysInfo->syncSystemCpuLoad);
	}
	OMR_TI_RETURN(vmThread, ret);
}

omr_error_t
omrtiGetProcessCpuLoad(OMR_VMThread *vmThread, double *processCpuLoad)
{
	omr_error_t ret = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		ret = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == processCpuLoad || NULL == vmThread->_vm) {
		ret = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else if (NULL == vmThread->_vm->sysInfo) {
		ret = OMR_ERROR_NOT_AVAILABLE;
	} else if (0 != omrthread_monitor_enter(vmThread->_vm->sysInfo->syncProcessCpuLoad)) {
		ret = OMR_ERROR_INTERNAL;
	} else {
		OMR_SysInfo *curSysInfo = vmThread->_vm->sysInfo;
		if ((SUPPORTED == curSysInfo->processCpuLoadCallStatus)
			|| (NO_HISTORY == curSysInfo->processCpuLoadCallStatus)
		) {
			OMRSysInfoProcessCpuTime newestProcessCpuTime;
			ret = getOMRSysInfoProcessCpuTime(vmThread->_vm, &newestProcessCpuTime, &curSysInfo->processCpuLoadCallStatus);
			if (OMR_ERROR_NONE != ret) {
				/* it is not supported on the platform or user has insufficient rights*/
				ret = OMR_ERROR_NOT_AVAILABLE;
			} else if (NO_HISTORY == curSysInfo->processCpuLoadCallStatus) {
				/* first call to this method */
				memcpy(&curSysInfo->oldestProcessCpuTime, &newestProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
				memcpy(&curSysInfo->interimProcessCpuTime, &newestProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
				curSysInfo->processCpuLoadCallStatus = SUPPORTED;
				ret = OMR_ERROR_RETRY;
			} else {
				OMRSysInfoCalculateCpuLoadError rc = OK;
				rc = calculateProcessCpuLoad(&newestProcessCpuTime, &curSysInfo->interimProcessCpuTime, processCpuLoad);
				if (OK == rc) {
					/* discard the oldestProcessCpuTime and replace it with interimProcessCpuTime, save newestProcessCpuTime as the new interimProcessCpuTime. */
					memcpy(&curSysInfo->oldestProcessCpuTime, &curSysInfo->interimProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
					memcpy(&curSysInfo->interimProcessCpuTime, &newestProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
				} else {
					/* interimProcessCpuTime or newestProcessCpuTime is not valid, or the elapsed time between them is too small. */
					if (NEGATIVE_INTERVAL == rc) {
						/* discard the interimProcessCpuLoad and replace it with the newestProcessCpuTime */
						memcpy(&curSysInfo->interimProcessCpuTime, &newestProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
					}
					/* attempt to recompute using oldestProcessCpuTime. */
					rc = calculateProcessCpuLoad(&newestProcessCpuTime, &curSysInfo->oldestProcessCpuTime, processCpuLoad);
					if (OK == rc) {
						curSysInfo->processCpuTimeNegativeElapsedTimeCount = 0;
					} else if (NEGATIVE_INTERVAL == rc) {
						/* discard the oldProcessCpuLoad and replace it with newestProcessCpuTime */
						memcpy(&curSysInfo->oldestProcessCpuTime, &newestProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
						curSysInfo->processCpuTimeNegativeElapsedTimeCount = 0;
						ret = OMR_ERROR_INTERNAL;
					} else if (ELAPSED_TIME_TOO_SMALL == rc) {
						/* return OMR_ERROR_RETRY if interval between newestProcessCpuTime and oldestProcessCpuTime is still too small */
						curSysInfo->processCpuTimeNegativeElapsedTimeCount = 0;
						ret = OMR_ERROR_RETRY;
					} else if (ELAPSED_TIME_NEGATIVE == rc) {
						if (MAXIMUM_NEGATIVE_ELAPSED_TIME_COUNT == curSysInfo->processCpuTimeNegativeElapsedTimeCount) {
							/* if this case found consecutively for MAXIMUM_NEGATIVE_ELAPSED_TIME_COUNT times, discard the oldestProcessCpuTime and replace it with newestProcessCpuTime. */
							memcpy(&curSysInfo->oldestProcessCpuTime, &newestProcessCpuTime, sizeof(OMRSysInfoProcessCpuTime));
							curSysInfo->processCpuTimeNegativeElapsedTimeCount = 0;
						} else {
							curSysInfo->processCpuTimeNegativeElapsedTimeCount += 1;
						}
						ret = OMR_ERROR_RETRY;
					}
				}
			}
		} else {
			ret = OMR_ERROR_NOT_AVAILABLE;
		}
		omrthread_monitor_exit(vmThread->_vm->sysInfo->syncProcessCpuLoad);
	}
	OMR_TI_RETURN(vmThread, ret);
}


typedef struct omrtiGetMemoryCategoriesState {
	OMR_TI_MemoryCategory *categories_buffer;
	int32_t max_categories;
	int32_t written_count;
	/* Records mapping of category indexes to slots in categories_buffer */
	OMR_TI_MemoryCategory **categories_mapping;
	BOOLEAN buffer_overflow;
	int32_t total_categories;
	uintptr_t categories_mapping_size;
} omrtiGetMemoryCategoriesState;

/* Categories defined in OMR have category codes from 0x80000000 up */
static uint32_t
indexFromCategoryCode(uintptr_t categories_mapping_size, uint32_t cc)
{
	/* Start the port library codes from the last element (total_categories - 1) */
	return ((cc) > OMRMEM_LANGUAGE_CATEGORY_LIMIT) ?
		   ((((uint32_t)categories_mapping_size) - 1) - (OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(cc))) : (cc);
}

/**
 * Callback used by omrtiGetMemoryCategories with omrmem_walk_categories
 */
static uintptr_t
omrtiGetMemoryCategoriesCallback(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations,
								 BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *state)
{
	struct omrtiGetMemoryCategoriesState *userData = (struct omrtiGetMemoryCategoriesState *)state->userData1;
	uint32_t index = indexFromCategoryCode(userData->categories_mapping_size, categoryCode);

	if (userData->written_count < userData->max_categories) {
		OMR_TI_MemoryCategory *omrtiCategory = &userData->categories_buffer[userData->written_count];
		OMR_TI_MemoryCategory **categories_mapping = userData->categories_mapping;

		/* Record the mapping of index to category */
		categories_mapping[index] = omrtiCategory;

		omrtiCategory->name = categoryName;
		omrtiCategory->liveBytesShallow = liveBytes;
		omrtiCategory->liveAllocationsShallow = liveAllocations;

		if (isRoot) {
			omrtiCategory->parent = NULL;
		} else {
			uint32_t parentIndex = indexFromCategoryCode(userData->categories_mapping_size, parentCategoryCode);

			/* Memory category walk is depth first - we will definitely have walked through and recorded our parent */
			omrtiCategory->parent = userData->categories_mapping[parentIndex];
		}

		userData->written_count++;

		return J9MEM_CATEGORIES_KEEP_ITERATING;
	} else {
		/* We've filled up the user's buffer. Stop iterating */
		userData->buffer_overflow = TRUE;
		return J9MEM_CATEGORIES_STOP_ITERATING;
	}
}

/**
 * Callback used by omrtiGetMemoryCategories with omrmem_walk_categories
 */
static uintptr_t
omrtiCountMemoryCategoriesCallback(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations,
								   BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *state)
{
	omrtiGetMemoryCategoriesState *userData = (struct omrtiGetMemoryCategoriesState *)state->userData1;
	userData->total_categories++;
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Callback used by omrtiGetMemoryCategories with omrmem_walk_categories
 */
static uintptr_t
omrtiCalculateSlotsForCategoriesMappingCallback(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations,
		BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *state)
{

	if (categoryCode < OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
		uintptr_t maxLanguageCategoryIndex = (uintptr_t)state->userData1;
		uintptr_t categoryIndex = (uintptr_t)categoryCode;

		state->userData1 = (void *)((maxLanguageCategoryIndex > categoryIndex) ? maxLanguageCategoryIndex : categoryIndex);

	} else if (categoryCode > OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
		uintptr_t maxOMRCategoryIndex = (uintptr_t)state->userData2;
		uintptr_t categoryIndex = (uintptr_t)OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(categoryCode);

		state->userData2 = (void *)((maxOMRCategoryIndex > categoryIndex) ? maxOMRCategoryIndex : categoryIndex);
	}
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

static void
fillInChildAndSiblingCategories(OMR_TI_MemoryCategory *categories_buffer, int32_t written_count)
{
	int32_t i;

	/* Note: for efficiency, we will temporarily retask the liveBytesDeep field as the tail of the child list */
	for (i = 0; i < written_count; i++) {
		OMR_TI_MemoryCategory *thisCategory = categories_buffer + i;
		OMR_TI_MemoryCategory *parent = thisCategory->parent;
		if (parent) {
			OMR_TI_MemoryCategory *lastChild = (OMR_TI_MemoryCategory *)(uintptr_t)parent->liveBytesDeep;

			if (lastChild) {
				lastChild->nextSibling = thisCategory;
			} else {
				/* We are adding the first child */
				parent->firstChild = thisCategory;
			}
			parent->liveBytesDeep = (int64_t)(uintptr_t)thisCategory;
		}
	}
}

static void
fillInCategoryDeepCounters(OMR_TI_MemoryCategory *category)
{
	OMR_TI_MemoryCategory *childCursor;

	category->liveBytesDeep = category->liveBytesShallow;
	category->liveAllocationsDeep = category->liveAllocationsShallow;

	childCursor = category->firstChild;

	while (childCursor) {
		/* Children will add to our deep counters */
		fillInCategoryDeepCounters(childCursor);

		childCursor = childCursor->nextSibling;
	}

	/* Pass back to our parent's deep counters */
	if (category->parent) {
		category->parent->liveBytesDeep += category->liveBytesDeep;
		category->parent->liveAllocationsDeep += category->liveAllocationsDeep;
	}
}

omr_error_t
omrtiGetMemoryCategories(OMR_VMThread *vmThread, int32_t max_categories, OMR_TI_MemoryCategory *categories_buffer,
						 int32_t *written_count_ptr, int32_t *total_categories_ptr)
{
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread)

	if (NULL == vmThread) {
		OMR_TI_RETURN(vmThread, OMR_THREAD_NOT_ATTACHED);
	} else {
		omr_error_t rc = OMR_ERROR_NOT_AVAILABLE;
		OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
		OMRMemCategoryWalkState walkState;
		omrtiGetMemoryCategoriesState userData;

		Trc_OMRTI_omrtiGetMemoryCategories_Entry(vmThread, max_categories, categories_buffer, written_count_ptr,
				total_categories_ptr);

		memset(&userData, 0, sizeof(struct omrtiGetMemoryCategoriesState));

		userData.categories_buffer = categories_buffer;
		userData.max_categories = max_categories;
		userData.total_categories = 0;

		if (max_categories < 0) {
			max_categories = 0;
		}

		/* If we're asked to write to categories_buffer (max_categories > 0), make sure categories_buffer isn't NULL */
		if (max_categories > 0 && NULL == categories_buffer) {
			Trc_OMRTI_omrtiGetMemoryCategories_NullOutput_Exit(vmThread, max_categories);
			OMR_TI_RETURN(vmThread, OMR_ERROR_ILLEGAL_ARGUMENT);
		}

		/* If the user has set max_categories and categories_buffer, but hasn't set written_count_ptr, fail. */
		if (max_categories > 0 && categories_buffer && NULL == written_count_ptr) {
			Trc_OMRTI_omrtiGetMemoryCategories_NullWrittenPtr_Exit(vmThread, max_categories, categories_buffer);
			OMR_TI_RETURN(vmThread, OMR_ERROR_ILLEGAL_ARGUMENT);
		}

		/* If all the out parameters are NULL, we can't do any work. */
		if (NULL == categories_buffer && NULL == written_count_ptr && NULL == total_categories_ptr) {
			Trc_OMRTI_omrtiGetMemoryCategories_AllOutputsNull_Exit(vmThread);
			OMR_TI_RETURN(vmThread, OMR_ERROR_ILLEGAL_ARGUMENT);
		}

		/* We need to allocate space to hold the categories data.
		 * Do a walk to count the categories and populate userData.total_categories.
		 */
		walkState.walkFunction = omrtiCountMemoryCategoriesCallback;
		walkState.userData1 = &userData;
		omrmem_walk_categories(&walkState);

		if (categories_buffer) {
			int32_t i;

			/* Calculate the storage we need for the mapping array.
			 * It needs to handle the largest possible indexes for OMR or language categories,
			 * not the total number of categories, which may be smaller.
			 */
			walkState.walkFunction = omrtiCalculateSlotsForCategoriesMappingCallback;
			walkState.userData1 = 0;
			walkState.userData2 = 0;
			omrmem_walk_categories(&walkState);
			/* Both + 1 because we have the max indexes which start from 0 */
			userData.categories_mapping_size = ((uintptr_t)walkState.userData1) + 1 + ((uintptr_t)walkState.userData2) + 1;

			walkState.walkFunction = omrtiGetMemoryCategoriesCallback;
			walkState.userData1 = &userData;
			walkState.userData2 = 0;

			userData.categories_mapping = (OMR_TI_MemoryCategory **)omrmem_allocate_memory(
											  userData.categories_mapping_size * sizeof(OMR_TI_MemoryCategory *), OMRMEM_CATEGORY_OMRTI);
			if (NULL == userData.categories_mapping) {
				/* OMR_ERROR_OUT_OF_MEMORY is returned if the buffer the user gave
				 * use wasn't big enough. That's not fatal. This is.
				 */
				Trc_OMRTI_omrtiGetMemoryCategories_J9MemAllocFail_Exit(vmThread,
						userData.categories_mapping_size * sizeof(OMR_TI_MemoryCategory *));
				OMR_TI_RETURN(vmThread, OMR_ERROR_INTERNAL);
			}

			memset(userData.categories_mapping, 0, userData.categories_mapping_size * sizeof(OMR_TI_MemoryCategory *));
			memset(categories_buffer, 0, max_categories * sizeof(OMR_TI_MemoryCategory));

			/* Walk categories to get shallow values */
			omrmem_walk_categories(&walkState);

			/* Fix-up child and sibling references */
			fillInChildAndSiblingCategories(categories_buffer, userData.written_count);

			/* Fill-in deep counters */
			for (i = 0; i < userData.written_count; i++) {
				OMR_TI_MemoryCategory *category = categories_buffer + i;

				if (NULL == category->parent) {
					fillInCategoryDeepCounters(category);
				}
			}

			if (userData.buffer_overflow) {
				Trc_OMRTI_omrtiGetMemoryCategories_BufferOverflow(vmThread);
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			} else {
				rc = OMR_ERROR_NONE;
			}

			omrmem_free_memory(userData.categories_mapping);
		}

		if (written_count_ptr) {
			*written_count_ptr = userData.written_count;
			if (OMR_ERROR_NOT_AVAILABLE == rc) {
				rc = OMR_ERROR_NONE;
			}
		}

		if (total_categories_ptr) {
			*total_categories_ptr = userData.total_categories;
			if (OMR_ERROR_NOT_AVAILABLE == rc) {
				rc = OMR_ERROR_NONE;
			}
		}

		Trc_OMRTI_omrtiGetMemoryCategories_Exit(vmThread, rc);

		OMR_TI_RETURN(vmThread, rc);
	}

}

omr_error_t
omrtiGetMethodDescriptions(OMR_VMThread *vmThread, void **methodArray, size_t methodArrayCount,
	OMR_SampledMethodDescription *methodDescriptions, char *nameBuffer, size_t nameBytes,
	size_t *firstRetryMethod, size_t *nameBytesRemaining)
{
	omr_error_t rc = OMR_ERROR_NONE;

	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == vmThread->_vm->_methodDictionary) {
		rc = OMR_ERROR_NOT_AVAILABLE;
	} else if (0 == methodArrayCount) {
		rc = OMR_ERROR_NONE;
	} else if ((NULL == methodArray) || (NULL == methodDescriptions)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else if ((0 != nameBytes) && (NULL == nameBuffer)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMR_MethodDictionary *const dictionary = (OMR_MethodDictionary *)vmThread->_vm->_methodDictionary;
		rc = dictionary->getEntries(vmThread, methodArray, methodArrayCount, methodDescriptions, nameBuffer, nameBytes, firstRetryMethod, nameBytesRemaining);
	}
	OMR_TI_RETURN(vmThread, rc);
}

/*
 * This function doesn't need to be synchronized because it accesses constant data, doesn't modify data, and doesn't use locks.
 */
omr_error_t
omrtiGetMethodProperties(OMR_VMThread *vmThread, size_t *numProperties, const char *const **propertyNames, size_t *sizeofSampledMethodDesc)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == vmThread->_vm->_methodDictionary) {
		rc = OMR_ERROR_NOT_AVAILABLE;
	} else if ((NULL == numProperties) || (NULL == propertyNames) || (NULL == sizeofSampledMethodDesc)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMR_MethodDictionary *const dictionary = (OMR_MethodDictionary *)vmThread->_vm->_methodDictionary;
		dictionary->getProperties(numProperties, propertyNames, sizeofSampledMethodDesc);
	}
	return rc;
}
