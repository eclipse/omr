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
 * @file sysinfo_helpers.c
 * @ingroup Port
 * @brief Contains implementation of methods to retrieve memory and processor info details.
 */

#include <errno.h>
#include <sys/__wlm.h>

#include "atoe.h"
#include "omrport.h"
#include "omrsimap.h"
#include "omrzfs.h"
#include "omrsysinfo_helpers.h"
#include "ut_omrport.h"

/* Function retrieves and populates memory usage statistics on a z/OS platform. */
int32_t
retrieveZOSMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo)
{
	uint64_t userCacheUsed = 0;
	uint64_t bufferCacheSize = 0;
	int32_t rc = 0;
	/* CVT address should be obtained in the following way to avoid Zero-Address Detection (ZAD)
	 * event (see JTCJAZZ: 60569)
	 */
	J9CVT *__ptr32 cvtp = ((J9PSA * __ptr32)0)->flccvt;
	J9RCE *__ptr32 rcep = cvtp->cvtrcep;
	J9ASMVT *__ptr32 asmvtp = cvtp->cvtasmvt;

	Trc_PRT_retrieveZOSMemoryStats_Entered();
	Assert_PRT_true(NULL != memInfo);
	memInfo->totalPhysical = ((uint64_t)rcep->rcepool * J9BYTES_PER_PAGE);
	memInfo->availPhysical = ((uint64_t)rcep->rceafc * J9BYTES_PER_PAGE);
	memInfo->totalSwap = ((uint64_t)asmvtp->asmslots * J9BYTES_PER_PAGE);
	memInfo->availSwap = ((uint64_t)(asmvtp->asmslots - (asmvtp->asmvsc + asmvtp->asmnvsc
									 + asmvtp->asmerrs)) * J9BYTES_PER_PAGE);
	rc = getZFSUserCacheUsed(&userCacheUsed);
	if (0 == rc) {
		memInfo->cached = userCacheUsed;
	}

	rc = getZFSMetaCacheSize(&bufferCacheSize);
	if (0 == rc) {
		memInfo->buffered = bufferCacheSize;
	}

	Trc_PRT_retrieveZOSMemoryStats_memUsageStats_v1(
		memInfo->totalPhysical,
		memInfo->availPhysical,
		memInfo->totalSwap,
		memInfo->availSwap,
		memInfo->cached,
		memInfo->buffered);

	Trc_PRT_retrieveZOSMemoryStats_Exit(0);
	return 0;
}

/* Function retrieves and populates processor usage statistics on a z/OS platform. */
int32_t
retrieveZOSProcessorStats(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
	/* CVT address should be obtained in the following way to avoid Zero-Address Detection (ZAD)
	 * event (see JTCJAZZ: 60569)
	 */
	J9CVT *__ptr32 cvtp = ((J9PSA * __ptr32)0)->flccvt;
	J9RMCT *__ptr32 rmctp = cvtp->cvtopctp;
	J9CCT *__ptr32 cctp = rmctp->rmctcct;

	Trc_PRT_retrieveZOSProcessorStats_Entered();
	Assert_PRT_true(NULL != procInfo);
	/* The only valid processor times that z/OS exposes are the total Busy and Idle times. */
	procInfo->procInfoArray[0].idleTime = cctp->ccvrbswt;
	procInfo->procInfoArray[0].busyTime = (cctp->ccvrbstd * cctp->ccvcpuct) - cctp->ccvrbswt;

	Trc_PRT_retrieveZOSProcessorStats_cpuUsageStats_v1(procInfo->procInfoArray[0].idleTime, procInfo->procInfoArray[0].busyTime);

	Trc_PRT_retrieveZOSProcessorStats_Exit(0);
	return 0;
}
