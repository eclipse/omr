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
 * @file j9zfs.c
 * @ingroup Port
 * @brief Contains implementation of methods to retrieve ZFS file system cache and buffer details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atoe.h"
#include "omrport.h"
#include "omrzfs.h"
#include "omrsysinfo_helpers.h"
#include "ut_omrport.h"

/**
 * Internal helper method to initialize members of the file system call parameter structure.
 * @param[in] syscallParamListPtr Pointer to the system call parameter structure.
 * @param[in] opcode the file system operation code.
 * @param[in] param2 value for the second parameter.
 * @return none
 */
static void
fillSyscallParamList(J9ZFSSyscallParamList *syscallParamListPtr, int32_t opcode, int32_t param2)
{
	/* syscallParamListPtr should not be null */
	Assert_PRT_true(NULL != syscallParamListPtr);

	memset(syscallParamListPtr, 0, sizeof(J9ZFSSyscallParamList));
	syscallParamListPtr->opcode = opcode;
	syscallParamListPtr->params[0] = sizeof(J9ZFSSyscallParamList);
	/* The second parameter is dependent on the type of operation being performed */
	syscallParamListPtr->params[1] = param2;
	return;
}

/**
 * Helper function to retrieve the size of ZFS file system client cache.
 * @param[out] clientCacheSizePtr Pointer to the returned client cache area size (in bytes).
 * @return 0 on success; negative value on failure.
 */
static int32_t
getZFSClientCacheSize(uint64_t *clientCacheSizePtr)
{
	uint64_t clientCacheSize = 0;
	J9ZFSConfigParam configParam = {0};
	J9ZFSConfigOption *configOptionPtr = &(configParam.configOption);
	char *cfgoEye = NULL;
	char *fsName = NULL;
	char *endPtr = NULL;
	char *ascName = NULL;
	int32_t bpxrv = 0;
	int32_t bpxrc = 0;
	int32_t bpxrs = 0;

	/* clientCacheSizePtr should not be null */
	Assert_PRT_true(NULL != clientCacheSizePtr);

	/* Query client cache size
	 * Initialize the file system call parameter list with
	 * the operation code and the size of structures we
	 * are passing in - for querying client cache stats this
	 * is 0
	 */
	fillSyscallParamList(&(configParam.syscallParamList), J9CFGOP_QUERY_CLIENT_CACHE_SIZE, 0);

	/* Initialize the configuration option fields */
	memset(configOptionPtr, 0, sizeof(J9ZFSConfigOption));
	cfgoEye = a2e(J9CFGO_EYE, sizeof(configOptionPtr->co_eye));
	if (NULL == cfgoEye) {
		Trc_PRT_getZFSClientCacheSize_MallocFailure();
		return -1;
	}
	memcpy(configOptionPtr->co_eye, cfgoEye, sizeof(configOptionPtr->co_eye));
	free(cfgoEye);
	configOptionPtr->co_ver = J9CO_VER_INITIAL;
	configOptionPtr->co_len = (int16_t)sizeof(J9ZFSConfigOption);

	fsName = a2e(J9ZFSNAME, strlen(J9ZFSNAME));
	if (NULL == fsName) {
		Trc_PRT_getZFSClientCacheSize_MallocFailure();
		return -1;
	}
	/* Invoke pfsctl() callable service */
#if defined(_LP64)					/* AMODE 64-bit */
	BPX4PCT(fsName,                 /* File system type: ZFS (in) */
			J9ZFSCALL_CONFIG,       /* Command: Configuration operation (in) */
			sizeof(configParam),    /* Argument length (in) */
			(char *)&configParam,   /* Argument (in & out) */
			&bpxrv,                 /* Return_value (out) */
			&bpxrc,                 /* Return_code (out) */
			&bpxrs);                /* Reason_code (out) */
#else								/* AMODE 31-bit */
	BPX1PCT(fsName,
			J9ZFSCALL_CONFIG,
			sizeof(configParam),
			(char *)&configParam,
			&bpxrv,
			&bpxrc,
			&bpxrs);
#endif /* defined(_LP64) */
	free(fsName);

	/* Check pfsctl() service return value */
	if (0 > bpxrv) {
		Trc_PRT_getZFSClientCacheSize_StatsFailure(bpxrv, bpxrc, bpxrs);
		return bpxrv;
	}

	/* The sizes are suffixed with 'G', 'M' or 'K'. Convert to bytes. */
	ascName = e2a_func(configParam.configOption.co_string, strlen(configParam.configOption.co_string));
	if (NULL == ascName) {
		Trc_PRT_getZFSClientCacheSize_MallocFailure();
		return -1;
	}
	clientCacheSize = strtol(ascName, &endPtr, J9SIZE_CONVERSION_BASE);
	if (('g' == *endPtr) || ('G' == *endPtr)) {
		clientCacheSize *= J9G_TO_BYTES_CONVERSION_FACTOR;
	} else if (('m' == *endPtr) || ('M' == *endPtr)) {
		clientCacheSize *= J9M_TO_BYTES_CONVERSION_FACTOR;
	} else if (('k' == *endPtr) || ('K' == *endPtr)) {
		clientCacheSize *= J9K_TO_BYTES_CONVERSION_FACTOR;
	}
	free(ascName);

	/* Set the returned parameter */
	*clientCacheSizePtr = clientCacheSize;
	return 0;
}

/* Retrieve the amount of used ZFS file system user cache. */
int32_t
getZFSUserCacheUsed(uint64_t *userCacheUsedPtr)
{

	uint64_t userCacheTotal = 0;
	uint64_t userCacheFree = 0;
	uint64_t userCacheUsed = 0;
	uint64_t clientCacheSize = 0;
	J9ZFSStatParam statParam = {0};
	J9ZFSStatApi *statApiPtr = &(statParam.statApi);
	char *fsName = NULL;
	char *saEye = NULL;
	int32_t bpxrv = 0;
	int32_t bpxrc = 0;
	int32_t bpxrs = 0;
	int32_t rc = 0;

	/* userCacheUsedPtr should not be null */
	Assert_PRT_true(NULL != userCacheUsedPtr);

	/* Do performance statistics operation for user cache
	 * Initialize the file system call parameter list with
	 * the operation code and the size of structures we
	 * are passing in - for querying user cache stats this
	 * is the sum of parameter list and stats control
	 * structure
	 */
	fillSyscallParamList(&(statParam.syscallParamList), J9STATOP_USER_CACHE,
						 (sizeof(J9ZFSSyscallParamList) + sizeof(J9ZFSStatApi)));

	/* Initialize the statistics API query control fields */
	memset(statApiPtr, 0, sizeof(J9ZFSStatApi));
	saEye = a2e(J9SA_EYE, sizeof(statApiPtr->sa_eye));
	if (NULL == saEye) {
		Trc_PRT_getZFSUserCacheUsed_MallocFailure();
		return -1;
	}
	memcpy(statApiPtr->sa_eye, saEye, sizeof(statApiPtr->sa_eye));
	free(saEye);

	statApiPtr->sa_ver = J9SA_VER_INITIAL;
	statApiPtr->sa_len = (int32_t)sizeof(J9ZFSStatUserCache);

	fsName = a2e(J9ZFSNAME, strlen(J9ZFSNAME));
	if (NULL == fsName) {
		Trc_PRT_getZFSUserCacheUsed_MallocFailure();
		return -1;
	}
	/* Invoke pfsctl() callable service */
#if defined(_LP64)					/* AMODE 64-bit */
	BPX4PCT(fsName,                 /* File system type: ZFS (in) */
			J9ZFSCALL_STATS, 	    /* Command: Statistics operation (in) */
			sizeof(statParam),    	/* Argument length (in) */
			(char *)&statParam,    	/* Argument (in & out) */
			&bpxrv,                 /* Return_value (out) */
			&bpxrc,                 /* Return_code (out) */
			&bpxrs);                /* Reason_code (out) */
#else								/* AMODE 31-bit */
	BPX1PCT(fsName,
			J9ZFSCALL_STATS,
			sizeof(statParam),
			(char *)&statParam,
			&bpxrv,
			&bpxrc,
			&bpxrs);
#endif /* defined(_LP64) */
	free(fsName);

	/* Check pfsctl() service return value */
	if (0 > bpxrv) {
		Trc_PRT_getZFSUserCacheUsed_StatsFailure(bpxrv, bpxrc, bpxrs);
		return bpxrv;
	}

	/* The above statistics operation returns the total and available user cache in pages. */
	userCacheTotal = (statParam.statUserCache.stuc_cache_pages * J9BYTES_PER_PAGE);
	userCacheFree = (statParam.statUserCache.stuc_total_free * J9BYTES_PER_PAGE);

	/* Adjust the total user cache to include the size of client cache as well */
	rc = getZFSClientCacheSize(&clientCacheSize);
	if (0 > rc) {
		/* Trace calls inside the called routine are invoked by this time. */
		return rc;
	}

	userCacheTotal += clientCacheSize;
	userCacheUsed = (userCacheTotal - userCacheFree);

	/* Set the returned parameter */
	*userCacheUsedPtr = userCacheUsed;
	return 0;
}

/* Retrieve the size of ZFS file system metadata cache buffers. */
int32_t
getZFSMetaCacheSize(uint64_t *bufferCacheSizePtr)
{
	uint64_t primaryBufferCount = 0;
	uint64_t primaryBufferSize = 0;
	uint64_t backingBufferCount = 0;
	uint64_t backingBufferSize = 0;
	uint64_t bufferCacheSize = 0;
	J9ZFSMetaStatParam metaStatParam = {0};
	J9ZFSStatApi *statApiPtr = &(metaStatParam.statApi);
	char *fsName = NULL;
	char *saEye = NULL;
	int32_t bpxrv = 0;
	int32_t bpxrc = 0;
	int32_t bpxrs = 0;

	/* bufferCacheSizePtr should not be null */
	Assert_PRT_true(NULL != bufferCacheSizePtr);

	/* Do performance statistics operation for metadata cache
	 * Initialize the file system call parameter list with
	 * the operation code and the size of structures we
	 * are passing in - for querying meta cache stats this
	 * is the sum of parameter list and stats control
	 * structure
	 */
	fillSyscallParamList(&(metaStatParam.syscallParamList), J9STATOP_META_CACHE,
						 (sizeof(J9ZFSSyscallParamList) + sizeof(J9ZFSStatApi)));

	/* Initialize the statistics API query control fields */
	memset(statApiPtr, 0, sizeof(J9ZFSStatApi));
	saEye = a2e(J9SA_EYE, sizeof(statApiPtr->sa_eye));
	if (NULL == saEye) {
		Trc_PRT_getZFSMetaCacheSize_MallocFailure();
		return -1;
	}
	memcpy(statApiPtr->sa_eye, saEye, sizeof(statApiPtr->sa_eye));
	free(saEye);
	statApiPtr->sa_ver = J9SA_VER_INITIAL;
	statApiPtr->sa_len = (int32_t)sizeof(J9ZFSAPIMetaStats);

	fsName = a2e(J9ZFSNAME, strlen(J9ZFSNAME));
	if (NULL == fsName) {
		Trc_PRT_getZFSMetaCacheSize_MallocFailure();
		return -1;
	}
	/* Invoke pfsctl() callable service */
#if defined(_LP64)					/* AMODE 64-bit */
	BPX4PCT(fsName,                 /* File system type: ZFS (in) */
			J9ZFSCALL_STATS,        /* Command: Statistics operation (in) */
			sizeof(metaStatParam), 	/* Argument length (in) */
			(char *)&metaStatParam, /* Argument (in & out) */
			&bpxrv,                 /* Return_value (out) */
			&bpxrc,                 /* Return_code (out) */
			&bpxrs);                /* Reason_code (out) */
#else								/* AMODE 31-bit */
	BPX1PCT(fsName,
			J9ZFSCALL_STATS,
			sizeof(metaStatParam),
			(char *)&metaStatParam,
			&bpxrv,
			&bpxrc,
			&bpxrs);
#endif /* defined(_LP64) */
	free(fsName);

	/* Check pfsctl() service return value */
	if (0 > bpxrv) {
		Trc_PRT_getZFSMetaCacheSize_StatsFailure(bpxrv, bpxrc, bpxrs);
		return bpxrv;
	}

	/* Metadata Primary Cache */
	primaryBufferCount = metaStatParam.apiMetaStats.am_primary.buffers.low;
	/* Buffer size is returned in kiloBytes */
	primaryBufferSize = metaStatParam.apiMetaStats.am_primary.buffsize * J9K_TO_BYTES_CONVERSION_FACTOR;
	/* Metadata Backing Cache */
	backingBufferCount = metaStatParam.apiMetaStats.am_backcache.buffers.low;
	backingBufferSize = metaStatParam.apiMetaStats.am_backcache.buffsize * J9K_TO_BYTES_CONVERSION_FACTOR;

	bufferCacheSize = (primaryBufferCount * primaryBufferSize) + (backingBufferCount * backingBufferSize);

	/* Set the returned parameter */
	*bufferCacheSizePtr = bufferCacheSize;
	return 0;
}
