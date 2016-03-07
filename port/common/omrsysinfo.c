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
 * @brief System information
 */
#include "omrport.h"
#include <string.h>



/**
 * Sets the number of entitled CPUs, which is specified by the user.
 *
 * @param[in] portLibrary The port library.
 * @param[in] number Number of entitled CPUs.
 */
void
omrsysinfo_set_number_entitled_CPUs(struct OMRPortLibrary *portLibrary, uintptr_t number)
{
	portLibrary->portGlobals->entitledCPUs = number;
	return;
}

/**
* Test if the given process exists in the system.
*
* @param[in] portLibrary The port library
* @param[in] pid Process ID of the process to be checked
*
* @return positive value if the process exists, 0 if the process does not exist, otherwise negative error code
*
**/
intptr_t
omrsysinfo_process_exists(struct OMRPortLibrary *portLibrary, uintptr_t pid)
{
	return 0;
}

/**
 * Determine the CPU architecture.
 *
 * @param[in] portLibrary The port library.
 *
 * @return A null-terminated string describing the CPU architecture of the hardware, NULL on error.
 *
 * @note portLibrary is responsible for allocation/deallocation of returned buffer.
 * @note See http://www.tolstoy.com/samizdat/sysprops.html for good values to return.
 */
const char *
omrsysinfo_get_CPU_architecture(struct OMRPortLibrary *portLibrary)
{
#if   defined(J9HAMMER)
	return OMRPORT_ARCH_HAMMER;
#elif defined(PPC64)
#ifdef OMR_ENV_LITTLE_ENDIAN
	return OMRPORT_ARCH_PPC64LE;
#else /* OMR_ENV_LITTLE_ENDIAN */
	return OMRPORT_ARCH_PPC64;
#endif /* OMR_ENV_LITTLE_ENDIAN */
#elif defined(PPC)
	return OMRPORT_ARCH_PPC;
#elif defined(S39064)
	return OMRPORT_ARCH_S390X;
#elif defined(S390)
	return OMRPORT_ARCH_S390;
#elif defined(X86)
	return OMRPORT_ARCH_X86;
#else
	return "unknown";
#endif
}
/**
 * Query the operating system for environment variables.
 *
 * Obtain the value of the environment variable specified by envVar from the operating system
 * and write out to supplied buffer.
 *
 * @param[in] portLibrary The port library.
 * @param[in] envVar Environment variable to query.
 * @param[out] infoString Buffer for value string describing envVar.
 * @param[in] bufSize Size of buffer to hold returned string.
 *
 * @return 0 on success, number of bytes required to hold the
 *	information if the output buffer was too small, -1 on failure.
 *
 * @note infoString is undefined on error or when supplied buffer was too small.
 */
intptr_t
omrsysinfo_get_env(struct OMRPortLibrary *portLibrary, const char *envVar, char *infoString, uintptr_t bufSize)
{
	return -1;
}
/**
 * Determines an absolute pathname for the executable.
 *
 * @param[in] portLibrary The port library.
 * @param[in] argv0 argv[0] value
 * @param[out] result Null terminated pathname string
 *
 * @return 0 on success, -1 on error (or information is not available).
 *
 * @note Caller should /not/ de-allocate memory in the result buffer, as string containing
 * the executable name is system-owned (managed internally by the port library).
 */
intptr_t
omrsysinfo_get_executable_name(struct OMRPortLibrary *portLibrary, const char *argv0, char **result)
{
	if (NULL == argv0) {
		return -1;
	}

	*result = (portLibrary->mem_allocate_memory)(portLibrary, strlen(argv0) + 1, OMR_GET_CALLSITE());
	if (NULL == *result) {
		return -1;
	}

	strcpy(*result, argv0);
	return 0;
}
/**
 * Determine the number of CPUs. Argument type is used to qualify the type of information:
 * 	- OMRPORT_CPU_PHYSICAL: Number of physical CPU's on this machine
 * 	- OMRPORT_CPU_ONLINE: Number of online CPU's on this machine
 * 	- OMRPORT_CPU_BOUND: Number of physical CPU's bound to this process
 * 	- OMRPORT_CPU_ENTITLED: Number of CPU's the user has specified should be used by the process
 * 	- OMRPORT_CPU_TARGET: Number of CPU's that should be used by the process. This is OMR_MIN(BOUND, ENTITLED).
 *
 * @param[in] portLibrary The port library.
 * @param[in] type Flag to indicate the information type (see function description).
 *
 * @return The number of CPUs, qualified by the argument type, on success.
 * 			Returns 0 if:
 * 			 - Physical failed (error)
 * 			 - Bound failed (error)
 * 			 - Entitled is disabled
 * 			 - For target if bound failed (error)
 */
uintptr_t
omrsysinfo_get_number_CPUs_by_type(struct OMRPortLibrary *portLibrary, uintptr_t type)
{
	uintptr_t toReturn = 0;

	switch (type) {
	case OMRPORT_CPU_PHYSICAL:
		toReturn = 0;
		break;
	case OMRPORT_CPU_ONLINE:
		toReturn = 0;
		break;
	case OMRPORT_CPU_BOUND:
		toReturn = 0;
		break;
	case OMRPORT_CPU_ENTITLED:
		toReturn = portLibrary->portGlobals->entitledCPUs;
		break;
	case OMRPORT_CPU_TARGET:
		toReturn = 0;
		break;
	default:
		/* Invalid argument */
		toReturn = 0;
		break;
	}

	return toReturn;
}

/**
 * Determine the OS type.
 *
 * @param[in] portLibrary The port library.
 *
 * @return OS type string (NULL terminated) on success, NULL on error.
 *
 * @note portLibrary is responsible for allocation/deallocation of returned buffer.
 */
const char *
omrsysinfo_get_OS_type(struct OMRPortLibrary *portLibrary)
{
	return "unknown";
}
/**
 * Determine version information from the operating system.
 *
 * @param[in] portLibrary The port library.
 *
 * @return OS version string (NULL terminated) on success, NULL on error.
 *
 * @note portLibrary is responsible for allocation/deallocation of returned buffer.
 */

const char *
omrsysinfo_get_OS_version(struct OMRPortLibrary *portLibrary)
{
	return "unknown";
}

/**
 * Provides memory usage statistics at a given point in time.
 *
 * @param[in] portLibrary The port library.
 * @param[out] memInfo Pointer to J9MemoryInfo to be populated.
 *
 * @return 0 on success; on failure, one of these values are returned:
 *    OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED - a NULL 'memInfo' was received by the function
 *    OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO - internal error while retrieving memory
 *    usage info.
 *    OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE - usage stats were found in invalid range.
 *
 * On failure, values in memInfo are not valid.
 *
 * @note If a particular memory usage parameter is not available on a platform, it is set as
 * OMRPORT_MEMINFO_NOT_AVAILABLE.
 */
int32_t
omrsysinfo_get_memory_info(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo, ...)
{
	return -1;
}
/**
 * Determine the size of the total physical memory in the system, in bytes.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 if the information was unavailable, otherwise total physical memory in bytes.
 */
uint64_t
omrsysinfo_get_physical_memory(struct OMRPortLibrary *portLibrary)
{
	return 0;
}
/**
 * Determine the process ID of the calling process.
 *
 * @param[in] portLibrary The port library.
 *
 * @return the PID.
 */
uintptr_t
omrsysinfo_get_pid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Determine the process ID of the calling process's parent.
 * Returns 0 if not supported. If the process has no parent,
 * the return value is OS specific.
 *
 * @param[in] portLibrary The port library.
 *
 * @return the PPID.
 */
uintptr_t
omrsysinfo_get_ppid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Return the effective group ID of the current process.
 *
 * This call always succeeds
 *
 * @param[in] portLibrary The port library
 *
 * @return effective group ID of the process. 0 is always returned on Windows and other platforms on which the numeric group ID is not available.
 */

uintptr_t
omrsysinfo_get_egid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Return the effective user ID of the current process.
 *
 * This call always succeeds
 *
 * @param[in] portLibrary The port library
 *
 * @return effective user ID of the process. 0 is always returned on Windows and other platforms on which the numeric user ID is not available.
 */

uintptr_t
omrsysinfo_get_euid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Returns a list of supplementary groups that this process belongs to.
 * Note that the returned list may not include effective group id of the process.
 *
 * @param[in] portLibrary The port library
 * @param[out] gidList On return points to list of supplementary group IDs. Caller passes a pointer to uint32_t*.
 * 					   On success this function allocates a new array and assigns it to *gidList.
 * 					   Caller is expected to free the array using omrmem_free_memory().
 * 					   On error *gidList is set to NULL.
 * @param[categoryCode] Memory allocation category code
 *
 * @return On success returns number of supplementary group IDs in the array pointed by *gidList, on error returns -1
 */
intptr_t
omrsysinfo_get_groups(struct OMRPortLibrary *portLibrary, uint32_t **gidList, uint32_t categoryCode)
{
	return -1;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrsysinfo_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrsysinfo_shutdown(struct OMRPortLibrary *portLibrary)
{
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the system information operations may be created here.  All resources created here should be destroyed
 * in @ref omrsysinfo_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_SYSINFO
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrsysinfo_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Query the operating system for the name of the user associate with the current thread
 *
 * Obtain the value of the name of the user associated with the current thread, and then write it out into the buffer
* supplied by the user
*
* @param[in] portLibrary The port Library
* @param[out] buffer Buffer for the name of the user
* @param[in,out] length The length of the buffer
*
* @return 0 on success, number of bytes required to hold the
* information if the output buffer was too small, -1 on failure.
*
* @note buffer is undefined on error or when supplied buffer was too small.
*/
intptr_t
omrsysinfo_get_username(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length)
{
	return -1;
}
/**
 * Query the operating system for the name of the group associate with the current thread
 *
 * Obtain the value of the name of the group associated with the current thread, and then write it out into the buffer
* supplied by the user
*
* @param[in] portLibrary The port Library
* @param[out] buffer Buffer for the name of the group
* @param[in,out] length The length of the buffer
*
* @return 0 on success, number of bytes required to hold the
* information if the output buffer was too small, -1 on failure.
*
* @note buffer is undefined on error or when supplied buffer was too small.
*/
intptr_t
omrsysinfo_get_groupname(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length)
{
	return -1;
}

/**
 * Fetch the system limit associated with the specified resource.
 * This is roughly equivalent to the POSIX getrlimit API.
 *
 * resourceID should be one of:
 *   OMRPORT_RESOURCE_SHARED_MEMORY
 *   OMRPORT_RESOURCE_ADDRESS_SPACE
 *   OMRPORT_RESOURCE_CORE_FILE
 *   OMRPORT_RESOURCE_CORE_FLAGS
 *          Operating system specific core information
 *            On AIX limit is set to the sys_parm fullcore value
 *            Not defined on other operating systems
 *
 * resourceID may be bit-wise or'ed with one of:
 *    OMRPORT_LIMIT_SOFT
 *    OMRPORT_LIMIT_HARD
 *
 * If neither OMRPORT_LIMIT_SOFT nor OMRPORT_LIMIT_HARD is specified,
 * the API returns the soft limit.
 *
 * The limit, if one exists, is returned through the uint64_t limit pointer.
 *
 * @param[in] portLibrary The Port library
 * @param[in] resourceID Identification of the limit requested
 * @param[out] limit Address of the variable to be updated with the limit value
 *
 * @return
 *  \arg OMRPORT_LIMIT_UNLIMITED (limit is set to max uint64_t)
 *  \arg OMRPORT_LIMIT_UNKNOWN (limit is set to OMRPORT_LIMIT_UNKNOWN_VALUE)
 *  \arg OMRPORT_LIMIT_LIMITED (limit is set to actual limit)
 */
uint32_t
omrsysinfo_get_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t *limit)
{
	*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
	return OMRPORT_LIMIT_UNKNOWN;
}

/**
 * Set the system limit associated with the specified resource.
 * This is roughly equivalent to the POSIX setrlimit API.
 *
 * resourceID should be one of:
 *   OMRPORT_RESOURCE_ADDRESS_SPACE
 *   OMRPORT_RESOURCE_CORE_FILE
 *   OMRPORT_RESOURCE_CORE_FLAGS
 *          Operating system specific core information
 *            On AIX this attempts to set the sys_parm fullcore value to limit (requires root to successfully change)
 *            No effect on other operating systems
 *
 * resourceID may be bit-wise or'ed with one of:
 *    OMRPORT_LIMIT_SOFT
 *    OMRPORT_LIMIT_HARD
 *
 * If neither OMRPORT_LIMIT_SOFT or OMRPORT_LIMIT_HARD is specified,
 * the API attempts to set the soft limit.
 *
 *
 * @param[in] portLibrary The Port library
 * @param[in] resourceID Identification of the limit requested
 * @param[in] setrlimit rlim_cur value
 *
 * @return
 *  \arg 0 on success
 *  \arg negative on error
 */
uint32_t
omrsysinfo_set_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit)
{
	return OMRPORT_LIMIT_UNKNOWN;
}

/**
 * Provides the system load average for the past 1, 5 and 15 minutes, if available.
 * The load average is defined as the number of runnable (including running) processes
 *  averaged over a period of time, however, it may also include processes in uninterruptable
 *  sleep states (Linux does this).
 *
 * @param[in] portLibrary The port library.
 * @param[out] loadAverage must be non-Null. Contains the load average data for the past 1, 5 and 15 minutes.
 * 			A load average of -1 indicates the load average for that specific period was not available.
 *
 * @return 0 on success, non-zero on error.
 *
 */
intptr_t
omrsysinfo_get_load_average(struct OMRPortLibrary *portLibrary, struct J9PortSysInfoLoadData *loadAverageData)
{
	return -1;
}

/**
 * Obtain the cumulative CPU utilization of all CPUs on the system.
 * The cpuTime and timestamp values have no absolute significance: they should be used only to compute
 * differences from previous values.
 * On an N-processor  system, cpuTimeStats.cpuTime may increase up to N times faster than real time.
 *
 * @param[in] OMRPortLibrary portLibrary The port library.
 * @param[out] J9SysinfoCPUTime cpuTime  struct to receive the CPU time and a timestamp
 *
 * @return 0 on success, negative portable error code on failure.
 *
 */
intptr_t
omrsysinfo_get_CPU_utilization(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTimeStats)
{
	return OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
}

/**
 * Initializes the iterator state to be used by @ref omrsysinfo_limit_iterator_next()
 *
 * Storage for state must be provided by the caller.
 *
 * @param[in] portLibrary The port library
 * @param[out] state the state of the iterator. Storage must be provided by the caller.
 *
 * @return
 * 			- 0 on success
 * 			- negative portable error code on failure :
 *				\arg OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM if the platform does not support the iterator
 *				\arg OMRPORT_ERROR_SYSINFO_OPFAILED for any other failure
*/
int32_t
omrsysinfo_limit_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state)
{
	return OMRPORT_ERROR_SYSINFO_OPFAILED;
}

/**
 * Returns TRUE if @ref omrsysinfo_limit_iterator_next() will return another limit element
 *
 * @param[in] portLibrary The port library
 * @param[out] state the state of the iterator that has been initialized using @ref omrsysinfo_limit_iterator_init
 *
 * @return TRUE if @ref omrsysinfo_limit_iterator_next() will return another limit element, FALSE otherwise.
*/
BOOLEAN
omrsysinfo_limit_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state)
{
	return FALSE;
}

/**
 * Returns the name, soft and hard values of user limits.
 *
 * @ref omrsysinfo_limit_iterator_init() must be used to initialize @ref state.
 *
 * @param[in] portLibrary The port library
 * @param[in] state the state of the iterator, must be initialized by @ref omrsysinfo_limit_iterator_init()
 * @param[out] limitElement callerAllocated structure is filled out with limit information by this function.
 * 					- the resourceName will always be valid, however the value is undefined on failure.
 *
 * @return 	0 if another element has been returned, otherwise portable error code.
 *
 * @note The caller must not modify the value returned by name, hardValue or softValue.
 */
int32_t
omrsysinfo_limit_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state, J9SysinfoUserLimitElement *limitElement)
{
	return OMRPORT_ERROR_SYSINFO_OPFAILED;
}

/**
 * Initializes the iterator state such that it can be used by @ref omrsysinfo_env_iterator_next()
 *
 * Storage for state must be provided by the caller.
 *
 * @param[in] 	portLibrary The port library
 * @param[in]	buffer 		caller-allocated buffer that stores the environment variables and values.
 *									- This can be freed by the caller once the iterator is no longer needed.
 * @param[in]	bufferSizeBytes	size of @ref buffer.
 * @param[out] 	state 		Contains state information for the iterator. Storage must be provided by the caller.
 *
 * @return:
 * 		- 0 on success,
 * 		- the required size of the buffer (a positive value) if @ref environBuffer was too small.
 * 			- in this case the iterator will still function:
 * 				- the environment will be truncated to fit the buffer
 * 				- the caller can iterate through the truncated environment.
 * 		- if @ref buffer is NULL, the required size of the buffer is returned.
 * 		- negative portable error code on failure:
 *			\arg OMRPORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER if the environment changed while it was being copied.
 *				- The caller can re-attempt initialization if this occurs.
 *			\arg OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM if the iterator is not supported on this platform
 */
int32_t
omrsysinfo_env_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, void *buffer, uintptr_t bufferSizeBytes)
{
	return OMRPORT_ERROR_SYSINFO_OPFAILED;
}

/**
 * Returns TRUE if @ref omrsysinfo_env_iterator_next() will return another environment element
 *
 * @param[in] 	portLibrary The port library
 * @param[out] 	state 		The state of the iterator that has been initialized using @ref omrsysinfo_env_iterator_init
 *
 * @return TRUE if @ref omrsysinfo_env_iterator_next() will return another limit element, FALSE otherwise.
*/
BOOLEAN
omrsysinfo_env_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state)
{
	return FALSE;
}

/**
 * Provides a pointer to the null terminated name=value pair in the process' environment
 *
 * @ref omrsysinfo_env_iterator_start() must be used to initialize state.
 * @ref omrsysinfo_env_iterator_hasNext() must return TRUE prior to every call to this function.
 *
 * @param[in] portLibrary The port library
 * @param[in] state the state of the iterator, must be initialized by @ref omrsysinfo_env_iterator_init()
 * @param[out] envElement caller-allocated structure contains name and value of environment variable on success.
 * 					- @ref nameAndValue field of @ref envElement points into the @ref buffer field of @ref state
 * 					- values of the fields are undefined if OMRPORT_SYSINFO_ITERATOR_END is returned.
 *
 * @return 	0 if another element has been returned, otherwise portable error code.
 */
int32_t
omrsysinfo_env_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, J9SysinfoEnvElement *envElement)
{
	return OMRPORT_ERROR_SYSINFO_OPFAILED;
}

/**
 * Returns a snapshot of processor usages for all the (logical) processors present and online
 * on the underlying machine. The function allocates memory for 'totalProcessorCount + 1' records into
 * the procInfoArray - the 0th record representing aggregates of processor usages of all online
 * processors, and records from 1 ... (totalProcessorCount) containing processor usages for individual
 * processors, while omrsysinfo_destroy_processor_info() destroys it.
 *
 * @param[in] portLibrary The port library.
 * @param[out] procInfo pointer to J9ProcessorInfos instance whose field 'procInfoArray' needs to be
 * allocated here, (and destroyed using omrsysinfo_destroy_processor_infos()) and populated to contain a
 * current snapshot of the processor usages.
 *
 * @return 0 on success; on failure, one of these values are returned:
 *    OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED - a NULL 'procInfo' was received by the function
 *    OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED - memory allocation failed.
 *    OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO - internal error while retrieving processor
 *    usage info
 *
 * On failure, values in procInfo are not valid.
 *
 * @note If a particular processor usage parameter is not available on a platform, it is set to
 * OMRPORT_PROCINFO_NOT_AVAILABLE.
 */
int32_t
omrsysinfo_get_processor_info(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
	return -1;
}

/**
 * Destroys the array J9ProcessorInfos.procInfoArray that was allocated by omrsysinfo_get_processor_info()
 * and sets this to NULL.
 *
 * @param[in] portLibrary The port library.
 * @param[in/out] procInfos the J9ProcessorInfos instance whose field procInfoArray is to be destroyed.
 */
void
omrsysinfo_destroy_processor_info(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfos)
{
	return;
}

/**
 * Provides the path of the current working directory (CWD) of the process.
 *
 * @param[in]      portLibrary The port library.
 * @param[in/out]  buf         user-allocated buffer that gets filled in with CWD
 *                             - Windows: CWD will be in UTF-8 encoding (Windows omrfile APIs expect the file name to be UTF-8)
 *                             - Non-Windows: CWD will be in platform encoding, to be consistent with rest of port library, as
 *                               all omrfile APIs currently expect file names to be in platform encoding
 * @param[in]     bufLen  size in bytes of buf
 *
 * @return        - 0 on success
 *                   - if bufLen is too small, the size of the buffer required to contain the directory
 *                   - otherwise negative portable error code.
 *
 * @not           - this function allocate/frees memory (for scratchspace) and therefore should not be used in the context of a signal
 *                  handler, as memory allocation is not typically signal-safe
 */
intptr_t
omrsysinfo_get_cwd(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen)
{
	return -1;
}


/**
 * Provides the path of the TMP directory of the process.
 *
 * @param[in]         portLibrary The port library.
 * @param[in/out]     buf         user-allocated buffer that gets filled in with TMP directory
 * @param[in]         bufLen      size in bytes of buf
 *                                - Windows: TMP directory will be in UTF-8 encoding (Windows omrfile APIs expect the file name to be UTF-8)
 *                                - Non-Windows: TMP directory will be in platform encoding, to be consistent with rest of port library, as
 *                                all omrfile APIs currently expect file names to be in platform encoding
 * @param[in]         ignoreEnvVariable Ignore checking any environment variable to get temporary directory. Used only on non-windows platforms.
 *
 * @return        - 0 on success
 *                        - if bufLen is too small, the size of the buffer required to contain the directory
 *                        - otherwise negative portable error code.
 *
 * @note            - this function allocate/frees memory (for scratchspace) and therefore should not be used in the context of a signal handler,
 *                    as memory allocation is not typically signal-safe
 * @note            - no check is performed to ensure that the TMP directory path returned either exists or has appropriate read/write
 *                    permissions - this is a machine configuration issue.
 */
intptr_t
omrsysinfo_get_tmp(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, BOOLEAN ignoreEnvVariable)
{
	return -1;
}
