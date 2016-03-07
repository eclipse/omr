/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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

#if !defined(OMRAGENT_H_INCLUDED)
#define OMRAGENT_H_INCLUDED

#include "omrcomp.h"
#include "omr.h"
#include "ute_core.h"

/*
 * This header file should be used by agent writers.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define OMR_TI_VERSION_0 0

typedef struct OMR_TI_MemoryCategory OMR_TI_MemoryCategory;
typedef struct OMR_SampledMethodDescription OMR_SampledMethodDescription;

typedef struct OMR_TI {
	int32_t version;
	void *internalData; /* AGENTS MUST NOT ACCESS THIS FIELD */

	/**
	 * Bind the current thread to an OMR VM.
	 * A thread can bind itself multiple times.
	 * Binds must be paired with an equal number of Unbinds.
	 *
	 * @param[in] vm the OMR vm
	 * @param[in] threadName The thread name. May be NULL. The caller is responsible for managing this memory.
	 * @param[out] vmThread the current OMR VMThread
	 * @return an OMR error code
	 *	 OMR_ERROR_NONE - BindCurrentThread complete
	 *	 OMR_ERROR_ILLEGAL_ARGUMENT - if vm is NULL
	 *	 OMR_ERROR_ILLEGAL_ARGUMENT - if vmThread is NULL
	 */
	omr_error_t (*BindCurrentThread)(OMR_VM *vm, const char *threadName, OMR_VMThread **vmThread);

	/**
	 * Unbind the current thread from its OMR VM.
	 * Unbinds must be paired with an equal number of binds.
	 * When the bind count of a thread reaches 0, the OMR VMThread
	 * is freed, and can no longer be used.
	 *
	 * @param[in,out] vmThread the current OMR VMThread
	 * @return an OMR error code
	 */
	omr_error_t (*UnbindCurrentThread)(OMR_VMThread *vmThread);

	/**
	 * Create a trace record subscriber.
	 *
	 * The returned subscriptionID uniquely identifies the new subscriber.
	 * The record subscriber must be shut down with a call to DeregisterRecordSubscriber() passing the same subscriptionID.
	 *
	 * The subscriber function will be called when a trace record is complete. The record will be passed to
	 * the subscriber in subscription->data. The size of the record will be in subscription->dataLength.
	 * The record may be re-used or freed by trace after the subscriber function returns. The UtSubscription
	 * returned via subscriptionID is the same subscription as the subscriber and alarm functions will be
	 * passed. UtSubscription->data and UtSubscription->dataLength are undefined outside of a call to the
	 * subscriber function.
	 *
	 * When the subscriber or alarm functions are called they will be passed the userData parameter in
	 * subscription->userData. They will be passed a copy of the string in the description parameter in
	 * subscription->description.
	 *
	 * The subscriber and alarm functions passed to RegisterRecordSubscriber() must not call any OMR_TI functions,
	 * including BindCurrentThread().
	 *
	 * @param[in] vmThread The current VM thread.
	 * @param[in] description Description of the subscriber. May not be NULL.
	 * @param[in] subscriberFunc Trace subscriber callback function. May not be NULL. This function will be passed records containing trace data
	 * 			  as they are processed.
	 * @param[in] alarmFunc Alarm callback function that's executed if the subscriber shuts down, returns an error or misbehaves. May be NULL.
	 * @param[in] userData Data passed to subscriber and alarm callbacks. May be NULL.
	 * @param[out] subscriptionID The returned subscription identifier.
	 * @return An OMR error code, including the following:
	 * @retval OMR_ERROR_NONE Success.
	 * @retval OMR_THREAD_NOT_ATTACHED vmThread is NULL.
	 * @retval OMR_ERROR_ILLEGAL_ARGUMENT description, subscriberFunc, or subscriptionID is NULL.
	 * @retval OMR_ERROR_NOT_AVAILABLE The trace engine is not enabled.
	 */
	omr_error_t	(*RegisterRecordSubscriber)(OMR_VMThread *vmThread, char const *description,
		utsSubscriberCallback subscriberFunc, utsSubscriberAlarmCallback alarmFunc, void *userData,	UtSubscription **subscriptionID);

	/**
	 * Shuts down the registered trace record subscriber.
	 * Removes the specified subscriber callback, preventing it from being passed any more data.
	 * This function may block indefinitely if the agent registers a subscription callback that does not terminate.
	 * A subscriptionID that was returned from a RegisterRecordSubscriber call must be used.
	 * The subscriber identified by the subscriptionID should not be deregistered more than once.
	 * DeregisterRecordSubscriber will cause the subscription thread to call the registered alarmFunc.
	 * @param[in] vmThread the current VM thread
	 * @param[in] subscriberID the subscription to deregister
	 * @return an OMR error code
	 *	 OMR_ERROR_NONE - deregistration complete
	 *	 OMR_THREAD_NOT_ATTACHED - no current VM thread
	 *	 OMR_ERROR_NOT_AVAILABLE - deregistration is unsuccessful
	 *	 OMR_ERROR_ILLEGAL_ARGUMENT - unknown subscriber
	 */
	omr_error_t (*DeregisterRecordSubscriber)(OMR_VMThread *vmThread, UtSubscription *subscriptionID);

	/**
	 * This function supplies the trace metadata for use with the trace formatter.
	 * @param[in] vmThread the current VM thread
	 * @param[out] data metadata in a form usable by the trace formatter
	 * @param[out] length the length of the metadata returned
	 * @return an OMR error code
	 *   OMR_ERROR_NONE - success
	 *   OMR_THREAD_NOT_ATTACHED - no current VM thread
	 *   OMR_ERROR_NOT_AVAILABLE - No trace engine
	 *   OMR_ERROR_INTERNAL - other error
	 */
	omr_error_t (*GetTraceMetadata)(OMR_VMThread *vmThread, void **data, int32_t *length);

	/**
	 * Set trace options at runtime.
	 * @param[in] vmThread the current VM thread
	 * @param[in] opts NULL-terminated array of option strings. The options must be provided in name / value pairs.
	 * 					If a name has no value, a NULL array entry must be provided.
	 * 					e.g. { "print", "none", NULL } is valid
	 * 					e.g. { "print", NULL } is invalid
	 * @return an OMR error code
	 */
	omr_error_t (*SetTraceOptions)(OMR_VMThread *vmThread, char const *opts[]);

	/**
	 * This function gets the system CPU load
	 * @param[in] vmThread the current OMR VM thread
	 * @param[out] systemCpuLoad the system CPU load between the observed time period, it cannot be NULL
	 * @return an OMR error code, 	return OMR_ERROR_NONE on successful call.
	 * 								return OMR_ERROR_NOT_AVAILABLE if not supported or insufficient user privilege or vm->sysInfo is not initialized.
	 * 								return OMR_ERROR_RETRY on the first call or if the interval between two calls is too small.
	 * 								return OMR_ERROR_INTERNAL if the calculated CPU load is invalid.
	 * 								return OMR_ERROR_ILLEGAL_ARGUMENT if systemCpuLoad is NULL.
	 * 								return OMR_THREAD_NOT_ATTACHED if vmThread is NULL.
	 */
	omr_error_t (*GetSystemCpuLoad)(OMR_VMThread *vmThread, double *systemCpuLoad);

	/**
	 * This function gets the process CPU load
	 * @param[in] vmThread the current OMR VM thread
	 * @param[out] processCpuLoad the process CPU load between the observed time period, it cannot be NULL.
	 * @return an OMR error code, 	return OMR_ERROR_NONE on successful call.
	 * 								return OMR_ERROR_NOT_AVAILABLE if not supported or insufficient user privilege or vm->sysInfo is not initialized.
	 * 								return OMR_ERROR_RETRY on the first call or if the interval between two calls is too small.
	 * 							  	return OMR_ERROR_INTERNAL if the calculated CPU load is invalid.
	 * 							  	return OMR_ERROR_ILLEGAL_ARGUMENT if processCpuLoad is NULL.
	 * 							  	return OMR_THREAD_NOT_ATTACHED if vmThread is NULL.
	 */
	omr_error_t (*GetProcessCpuLoad)(OMR_VMThread *vmThread, double *processCpuLoad);

	/**
	  * Samples the values of the JVM memory categories and writes them into a buffer allocated by the user.
	  *
	  * @param[in] vmThread the current OMR VM thread
	  * @param[in] max_categories Maximum number of categories to read into category_buffer
	  * @param[out] categories_buffer Block of memory to write result into. 0th entry is the first root category. All other nodes can be walked from the root.
	  * @param[out] written_count_ptr If not NULL, the number of categories written to the buffer is written to this address
	  * @param[out] total_categories_ptr If not NULL, the total number of categories available is written to this address
	  *
	  * @return omr_error_t error code:
	  * OMR_ERROR_NONE - success
	  * OMR_ERROR_ILLEGAL_ARGUMENT - Illegal argument (categories_buffer, count_ptr & total_categories_ptr are all NULL)
	  * OMR_ERROR_OUT_OF_MEMORY - Memory category data was truncated because category_buffer/max_categories was not large enough
	  * OMR_ERROR_INTERNAL - GetMemoryCategories was unable to allocate memory for it's own use.
	  * OMR_THREAD_NOT_ATTACHED - The vmThread parameter was NULL
	  *
	  */
	omr_error_t (*GetMemoryCategories)(OMR_VMThread *vmThread, int32_t max_categories, OMR_TI_MemoryCategory *categories_buffer,
			int32_t *written_count_ptr, int32_t *total_categories_ptr);

	/**
	 * Adds all trace buffers containing data to the write queue, then prompts processing of the write
	 * queue via the standard mechanism.
	 *
	 * @param[in] vmThread the current OMR VM thread
	 * @return Return values with meaning specific to this function:
	 * @retval OMR_ERROR_NONE - success
	 * @retval OMR_ERROR_ILLEGAL_ARGUMENT - If another flush operation was in progress or no record subscribers are registered
	 * @retval OMR_THREAD_NOT_ATTACHED - The vmThread parameter was NULL
	 * @retval OMR_ERROR_NOT_AVAILABLE - No trace engine
	 */
	omr_error_t (*FlushTraceData)(OMR_VMThread *vmThread);

	/**
	 * This function gets the free physical memory size on the system in bytes
	 * @param[in] vmThread the current OMR VM thread
	 * @param[out] freePhysicalMemorySize the amount of available memory size in bytes
	 * @return an OMR error code
	 */
	omr_error_t (*GetFreePhysicalMemorySize)(OMR_VMThread *vmThread, uint64_t *freePhysicalMemorySize);

	/**
	 * This function gets the process virtual memory size in bytes
	 * @param[in] vmThread the current OMR VM thread
	 * @param[out] processVirtualMemorySize the amount of process virtual memory in bytes
	 * @return an OMR error code
	 */
	omr_error_t (*GetProcessVirtualMemorySize)(OMR_VMThread *vmThread, uint64_t *processVirtualMemorySize);

	/**
	 * This function gets the process private memory size in bytes
	 * @param[in] vmThread the current OMR VM thread
	 * @param[out] processPrivateMemorySize the amount of process private memory in bytes
	 * @return an OMR error code
	 */
	omr_error_t (*GetProcessPrivateMemorySize)(OMR_VMThread *vmThread, uint64_t *processPrivateMemorySize);

	/**
	 * This function gets the process physical memory size in bytes
	 * @param[in] vmThread the current OMR VM thread
	 * @param[out] processPhysicalMemorySize the amount of process physical memory in bytes
	 * @return an OMR error code
	 */
	omr_error_t (*GetProcessPhysicalMemorySize)(OMR_VMThread *vmThread, uint64_t *processPhysicalMemorySize);

	/**
	 * Retrieve the descriptions of methods returned from callstack sampling tracepoints.
	 *
	 * Method descriptions are returned in the order they are specified in methodArray.
	 * We will return as many complete method descriptions as possible given the amount of nameBuffer provided.
	 * We won't return incomplete method descriptions.
	 * The reasonCode of each method description is set to one of the following values:
	 * 1) OMR_ERROR_NONE: The method description was successfully retrieved.
	 * 2) OMR_ERROR_NOT_AVAILABLE: The method description is not available.
	 * 3) OMR_ERROR_RETRY: The method description could not be retrieved due to lack of space in nameBuffer.
	 *
	 * A method's description is deleted from the VM's method dictionary after it is has been been retrieved.
	 * It can't be retrieved twice.
	 *
	 * Methods created before the VM's method dictionary is enabled will not be available.
	 *
	 * @param[in] vmThread The current OMR VM thread.
	 * @param[in] methodArray The methods for which descriptions are requested.
	 *            Must not be NULL if methodArrayCount is non-zero.
	 * @param[in] methodArrayCount The number of elements in methodArray.
	 * @param[out] methodDescriptions A pre-allocated buffer where the requested method descriptions will be written.
	 *             The buffer size must be at least (methodArrayCount x sizeofSampledMethodDesc) bytes, where
	 *             sizeofSampledMethodDesc is retrieved using GetMethodProperties().
	 *             Must not be NULL if methodArrayCount is non-zero.
	 * @param[out] nameBuffer A preallocated buffer where string data for the requested method descriptions will be written.
	 *             Must not be NULL if methodArrayCount and nameBytes are non-zero.
	 * @param[in] nameBytes The size of nameBuffer, in bytes.
	 * @param[out] firstRetryMethod If the return value is OMR_ERROR_RETRY, the first method that could not be retrieved
	 *             due to lack of space in nameBuffer.
	 * @param[out] nameBytesRemaining If the return value is OMR_ERROR_RETRY, the number of nameBuffer bytes needed
	 *             to fetch the remaining methods.
	 *
	 * @return An OMR error code.
	 * @retval OMR_ERROR_NONE All available method descriptions were successfully retrieved. Some descriptions may have been unavailable.
	 * @retval OMR_THREAD_NOT_ATTACHED vmThread is NULL.
	 * @retval OMR_ERROR_RETRY Some method descriptions could not be retrieved because nameBuffer was too small.
	 * @retval OMR_ERROR_ILLEGAL_ARGUMENT A NULL pointer was passed in for a buffer that has a non-zero size.
	 * @retval OMR_ERROR_NOT_AVAILABLE The method dictionary has not been enabled.
	 * @retval OMR_ERROR_INTERNAL An unexpected internal error occurred. firstRetryMethod indicates the
	 * entry of methodArray where the error occurred.
	 */
	omr_error_t (*GetMethodDescriptions)(OMR_VMThread *vmThread, void **methodArray, size_t methodArrayCount,
		OMR_SampledMethodDescription *methodDescriptions, char *nameBuffer, size_t nameBytes,
		size_t *firstRetryMethod, size_t *nameBytesRemaining);

	/**
	 * Retrieve the language-specific method properties.
	 *
	 * @param[in] vmThread The current OMR VM thread.
	 * @param[out] numProperties The number of method properties. Must be non-NULL.
	 * @param[out] propertyNames The ordered list of method property names. Must be non-NULL. Don't modify the data provided by this function.
	 * @param[out] sizeofSampledMethodDesc The size, in bytes, of a method description returned from GetMethodDescriptions().
	 *
	 * @return An OMR error code.
	 * @retval OMR_ERROR_NONE Success.
	 * @retval OMR_THREAD_NOT_ATTACHED vmThread is NULL.
	 * @retval OMR_ERROR_NOT_AVAILABLE The method dictionary has not been enabled.
	 * @retval OMR_ERROR_ILLEGAL_ARGUMENT A NULL pointer was passed in for an output parameter.
	 */
	omr_error_t (*GetMethodProperties)(OMR_VMThread *vmThread, size_t *numProperties, const char *const **propertyNames, size_t *sizeofSampledMethodDesc);
} OMR_TI;

/*
 * Return data for the GetMemoryCategories API
 */
struct OMR_TI_MemoryCategory {
	/* Category name */
	const char *name;

	/* Bytes allocated under this category */
	int64_t liveBytesShallow;

	/* Bytes allocated under this category and all child categories */
	int64_t liveBytesDeep;

	/* Number of allocations under this category */
	int64_t liveAllocationsShallow;

	/* Number of allocations under this category and all child categories */
	int64_t liveAllocationsDeep;

	/* Pointer to the first child category (NULL if this node has no children) */
	struct OMR_TI_MemoryCategory *firstChild;

	/* Pointer to the next sibling category (NULL if this node has no next sibling)*/
	struct OMR_TI_MemoryCategory *nextSibling;

	/* Pointer to the parent category. (NULL if this node is a root) */
	struct OMR_TI_MemoryCategory *parent;
};

/**
 * Description of a method that was sampled by the profiler, which is retrieved using GetMethodDescriptions() in OMR_TI.
 * The size of this structure is language-specific. Call GetMethodProperties() to determine its required size.
 */
#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif /* defined(_MSC_VER) */
struct OMR_SampledMethodDescription {
	/** See comments for GetMethodDescriptions(). */
	omr_error_t reasonCode;

	/**
	 * In the output from GetMethodProperties(), the reasonCode will be followed by
	 * a flexible array of property values, corresponding to the properties returned
	 * by GetMethodProperties().
	 * Some elements of propertyValues[] may be NULL.
	 */
	const char *propertyValues[];
};

typedef struct OMR_AgentCallbacks {
	uint32_t version; /* version counter, initially 0 */
	omr_error_t (*onPreFork)(void);
	omr_error_t (*onPostForkParent)(void);
	omr_error_t (*onPostForkChild)(void);
} OMR_AgentCallbacks;

/*
 * Required agent entry points:
 */
#if defined(_MSC_VER)
omr_error_t __cdecl OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...);
omr_error_t __cdecl OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm);
#else
/**
 * Invoked when an agent is loaded.
 *
 * Don't assume any VM threads exist when OnLoad is invoked.
 *
 * @param[in] ti The OMR tooling interface.
 * @param[in] vm The OMR vm.
 * @param[in] options A read-only string. Don't free or modify it.
 * @param[in,out] agentCallbacks A table of function pointers, to be filled in by the agent.
 *                The caller preallocates the table, and fills in the value of agentCallbacks->version.
 *                The agent must not free agentCallbacks.
 * @return OMR_ERROR_NONE for success, an OMR error code otherwise.
 */
omr_error_t OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...);

/**
 * Invoked when an agent is unloaded.
 *
 * Don't assume any VM threads exist when OnUnload is invoked.
 * Don't assume that OnUnload is invoked from the same thread that invoked OnLoad.
 * The agent must deregister its subscribers before returning from this function.
 *
 * @param[in] ti The OMR tooling interface.
 * @param[in] vm The OMR vm.
 * @return omr_error_t error code:
 *   OMR_ERROR_NONE - Success: it is safe to unload the agent lib
 *   Otherwise, any other return value indicates the VM cannot unload the agent lib
 */
omr_error_t OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm);
#endif

#ifdef __cplusplus
}
#endif

#endif /* OMRAGENT_H_INCLUDED */
