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

#include "omrTestHelpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "omragent.h"

#define MATRIX_SIZE 1000
#define NUM_ITERATIONS_SYSTEM_CPU_BURN	3000

static intptr_t dummy_omrsysinfo_get_CPU_utilization(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTimeStats);
static void matrixSquare(void);
static void systemTimeCPUBurn(void);
static omr_error_t testProcessCpuLoad(OMR_VMThread *vmThread, OMR_TI const *ti, omr_error_t expectedRc, BOOLEAN dummyPort, BOOLEAN checkRc);
static omr_error_t testSystemCpuLoad(OMR_VMThread *vmThread, OMR_TI const *ti, omr_error_t expectedRc, BOOLEAN dummyPort, BOOLEAN checkRc);
static omr_error_t testTICpuLoad(OMR_VMThread *vmThread, OMR_TI const *ti);

static const char *agentName = "cpuLoadAgent";

typedef intptr_t (*real_sysinfo_get_CPU_utilization)(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTime);
/* real_omrsysinfo_get_CPU_utilization and callIndex are global because dummy_omrsysinfo_get_CPU_utilization() needs to access them */
static real_sysinfo_get_CPU_utilization real_omrsysinfo_get_CPU_utilization = NULL;
static int32_t callIndex = 0;

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	OMR_VMThread *vmThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	omrtty_printf("%s: Agent_OnLoad(options=\"%s\")\n", agentName, options);
	if (OMR_ERROR_NONE == rc) {
		if (NULL == ti) {
			omrtty_printf("%s:%d: NULL OMR_TI interface pointer.\n", __FILE__, __LINE__);
			rc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, "cpuLoadAgent main", &vmThread);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("%s: BindCurrentThread failed, rc=%d\n", agentName, rc);
		} else if (NULL == vmThread) {
			omrtty_printf("%s: BindCurrentThread failed, NULL vmThread was returned\n", agentName);
			rc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("%s: BindCurrentThread passed, vmThread=0x%p\n", agentName, vmThread);
		}
	}
#if !defined(J9ZOS390)
	/**
	 * Exclude z/OS when testing GetProcessCpuLoad() and GetSystemCpuLoad(). Existing implementation of
	 * omrsysinfo_get_CPU_utilization(), which the two APIs rely on, is problematic on z/OS. (details see JAZZ103 53507)
	 */
	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(testTICpuLoad(vmThread, ti));
	}
#endif /* !defined(J9ZOS390) */
	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("%s: UnbindCurrentThread failed, rc=%d\n", agentName, rc);
		} else {
			omrtty_printf("%s: UnbindCurrentThread passed\n", agentName);
		}
	}
	return rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	omrtty_printf("%s: Agent_OnUnload\n", agentName);
	return OMR_ERROR_NONE;
}

static intptr_t
dummy_omrsysinfo_get_CPU_utilization(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTimeStats)
{
	intptr_t rc = 0;

	if ((2 == callIndex) || (6 == callIndex) || (7 == callIndex)) {
		/* return an invalid sample */
		real_omrsysinfo_get_CPU_utilization(portLibrary, cpuTimeStats);
		cpuTimeStats->cpuTime = I_64_MAX;
		cpuTimeStats->timestamp = 0;
	} else if ((7 < callIndex) && (callIndex < 11)) {
		/* set number of CPUs to 1 in the 8th and 10th calls */
		real_omrsysinfo_get_CPU_utilization(portLibrary, cpuTimeStats);
		cpuTimeStats->numberOfCpus = 1;
	} else if (11 == callIndex) {
		/* test INSUFFICIENT_PRIVILEGE case */
		rc = OMRPORT_ERROR_SYSINFO_INSUFFICIENT_PRIVILEGE;
	} else if (12 == callIndex) {
		/* test SYSINFO_NOT_SUPPORTED case */
		rc = OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
	} else {
		real_omrsysinfo_get_CPU_utilization(portLibrary, cpuTimeStats);
	}
	return rc;
}

static omr_error_t
testProcessCpuLoad(OMR_VMThread *vmThread, OMR_TI const *ti, omr_error_t expectedRc, BOOLEAN dummyPort, BOOLEAN checkRc)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;
	double processCpuLoad = 0.0;
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);

	real_omrsysinfo_get_CPU_utilization = OMRPORTLIB->sysinfo_get_CPU_utilization;
	matrixSquare();
	systemTimeCPUBurn();
	if (TRUE == dummyPort) {
		/* use dummy_omrsysinfo_get_CPU_utilization */
		OMRPORTLIB->sysinfo_get_CPU_utilization = dummy_omrsysinfo_get_CPU_utilization;
	}
	rc = ti->GetProcessCpuLoad(vmThread, &processCpuLoad);
	if ((processCpuLoad < 0.0) || (processCpuLoad > 1.0)) {
		omrtty_printf(
			"%s:%d callIndex: %d: GetProcessCpuLoad() returned an invalid value, processCpuLoad = %lf ! ProcessCpuLoad should be in [0, 1].\n",
			__FILE__, __LINE__, callIndex, processCpuLoad);
		testRc = OMR_ERROR_INTERNAL;
	} else if (checkRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(rc, expectedRc);
		if (expectedRc != rc) {
			omrtty_printf(
				"%s:%d callIndex: %d: GetProcessCpuLoad() returned a wrong error code !\n",
				__FILE__, __LINE__, callIndex);
			testRc = OMR_ERROR_INTERNAL;
		} else if (OMR_ERROR_NONE == rc) {
			omrtty_printf(
				"callIndex: %d: rc = %d (%s), the function call is successful ! Process CPU load: %lf\n",
				callIndex, rc, omrErrorToString(rc), processCpuLoad);
		} else {
			omrtty_printf("callIndex: %d: rc = %d (%s), the function call is successful !\n", callIndex, rc, omrErrorToString(rc));
		}
	}
	if (TRUE == dummyPort) {
		/* set sysinfo_get_CPU_utilization back to real_omrsysinfo_get_CPU_utilization */
		OMRPORTLIB->sysinfo_get_CPU_utilization = real_omrsysinfo_get_CPU_utilization;
	}
	return testRc;
}

static omr_error_t
testSystemCpuLoad(OMR_VMThread *vmThread, OMR_TI const *ti, omr_error_t expectedRc, BOOLEAN dummyPort, BOOLEAN checkRc)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;
	double systemCpuLoad = 0.0;
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);

	real_omrsysinfo_get_CPU_utilization = OMRPORTLIB->sysinfo_get_CPU_utilization;
	matrixSquare();
	systemTimeCPUBurn();
	if (TRUE == dummyPort) {
		/* use dummy_omrsysinfo_get_CPU_utilization */
		OMRPORTLIB->sysinfo_get_CPU_utilization = dummy_omrsysinfo_get_CPU_utilization;
	}
	rc = ti->GetSystemCpuLoad(vmThread, &systemCpuLoad);
	if ((systemCpuLoad < 0.0) || (systemCpuLoad > 1.0)) {
		omrtty_printf(
			"%s:%d callIndex: %d: GetSystemCpuLoad() returned an invalid value, systemCpuLoad = %lf ! systemCpuLoad should be in [0, 1].\n",
			__FILE__, __LINE__, callIndex, systemCpuLoad);
		testRc = OMR_ERROR_INTERNAL;
	} else if (checkRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(rc, expectedRc);
		if (expectedRc != rc) {
			omrtty_printf(
				"%s:%d callIndex: %d: GetSystemCpuLoad() returned a wrong error code ! \n",
				__FILE__, __LINE__, callIndex);
			testRc = OMR_ERROR_INTERNAL;
		} else if (OMR_ERROR_NONE == rc) {
			omrtty_printf(
				"callIndex: %d: rc = %d (%s), the function call is successful ! system CPU load: %lf\n",
				callIndex, rc, omrErrorToString(rc), systemCpuLoad);
		} else {
			omrtty_printf("callIndex: %d: rc = %d (%s), the function call is successful !\n", callIndex, rc, omrErrorToString(rc));
		}
	}
	if (TRUE == dummyPort) {
		/* set sysinfo_get_CPU_utilization back to real_omrsysinfo_get_CPU_utilization */
		OMRPORTLIB->sysinfo_get_CPU_utilization = real_omrsysinfo_get_CPU_utilization;
	}
	return testRc;
}

/**
 * Copied from JAZZ103 68519 CTD model for OMR TI APIs related to MXBeans CPU data.
 * Test cases covered by the code:
 *
 * CaseID 	CPUs		userPrivilege 	isSupported		ThreadID 	Sample1 	Sample2 	CurrentSample 	Expected Return Value
 *	1 		multiple 	sufficient 		True 			valid 		valid 		invalid 	valid 			OMR_ERROR_NONE
 *	2 		multiple 	sufficient 		True 			valid 		valid 		valid 		valid 			OMR_ERROR_NONE
 *	3 		multiple 	sufficient 		True 			valid 		valid 		valid 		doNotExist 		OMR_ERROR_NONE
 *	4 		multiple 	sufficient 		True 			valid 		valid 		invalid 	doNotExist 		OMR_ERROR_RETRY
 *	5 		multiple 	sufficient 		True 			valid 		valid 		doNotExist 	doNotExist 		OMR_ERROR_RETRY
 *	6 		multiple 	sufficient 		True 			valid 		valid 		invalid 	invalid 		OMR_ERROR_RETRY
 *	7 		multiple 	sufficient 		True 			valid 		invalid 	valid 		valid 			OMR_ERROR_NONE
 *	8 		single 		sufficient 		True 			valid 		valid 		valid 		valid 			OMR_ERROR_NONE
 *	9 		multiple 	insufficient 	True 			valid 		/ 				/ 		doNotExist 		OMR_ERROR_NOT_AVAILABLE
 *	10 		multiple 	sufficient 		false 			valid 		/ 				/ 		doNotExist 		OMR_ERROR_NOT_AVAILABLE
 *	11		/				/			/				NULL		/				/		doNotExist		OMR_THREAD_NOT_ATTACHED
 *
 *	We could call the API 13 times in the following sequence to cover the above 11 cases:
 *	call 1 ---> call 2 ---> call 3 ---> call 4 ---> call 5 ---> call 6 --->call 7 --->
 *	(valid)		(invalid)	(valid)		(valid)		(valid) 	(invalid) 	(invalid)
 *	---> call 8 --->call 9 --->call 10 ----->call 11 -----> call 12 ----> call 13
 *	(call 8 to 10 valid, single CPU)   (no privilege) (unsupported) (NULL thread)
 *
 *	In this way,
 *	case 1 is covered by call 1 to 3,	case 2 is covered by call 3 to 5, 	case 3 is covered by call 3 to 4,
 *	case 4 is covered by call 1 to 2,	case 5 is covered by call 1, 		case 6 is covered by call 5 to 7,
 *	case 7 is covered by call 2 to 4,	case 8 is covered by call 8 to 10,	case 9 is covered by call 11,
 *	case 10 is covered by call 12, 		case 11 is covered by call 13
 *
 *	Therefore, we should check the return value of the API in the following sequence:
 *	callIndex		expected return value			covered caseID
 *	1				OMR_ERROR_RETRY					5
 *	2				OMR_ERROR_RETRY					4
 *	3				OMR_ERROR_NONE					1
 *	4				OMR_ERROR_NONE					3, 7
 *	5				OMR_ERROR_NONE					2
 *	7				OMR_ERROR_RETRY					6
 *	10				OMR_ERROR_NONE					8
 *	11				OMR_ERROR_NOT_AVAILABLE			9
 *	12 				OMR_ERROR_NOT_AVAILABLE			10
 *	13				OMR_THREAD_NOT_ATTACHED			11
 *
 */

static omr_error_t
testTICpuLoad(OMR_VMThread *vmThread, OMR_TI const *ti)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
	omr_error_t testRc = OMR_ERROR_NONE;

	/* test GetProcessCpuLoad() */
	omrtty_printf("Test GetProcessCpuLoad()\n");
	callIndex = 1;
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_RETRY, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_RETRY, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NONE, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NONE, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NONE, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_RETRY, TRUE, FALSE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_RETRY, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NONE, TRUE, FALSE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NONE, TRUE, FALSE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NONE, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NOT_AVAILABLE, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testProcessCpuLoad(vmThread, ti, OMR_ERROR_NOT_AVAILABLE, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		double cpuLoad = 0.0;
		omr_error_t rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessCpuLoad(NULL, &cpuLoad), OMR_THREAD_NOT_ATTACHED);

		if (OMR_THREAD_NOT_ATTACHED == rc) {
			omrtty_printf(
				"callIndex: %d: rc = %d (%s), the function call is successful !\n",
				callIndex, OMR_THREAD_NOT_ATTACHED, omrErrorToString(OMR_THREAD_NOT_ATTACHED));
		}
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("%s: GetProcessCpuLoad() passed\n", agentName);
		omrtty_printf("Test GetSystemCpuLoad()\n");
		callIndex = 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_RETRY, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_RETRY, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NONE, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NONE, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NONE, FALSE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_RETRY, TRUE, FALSE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_RETRY, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NONE, TRUE, FALSE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NONE, TRUE, FALSE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NONE, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NOT_AVAILABLE, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		testRc = testSystemCpuLoad(vmThread, ti, OMR_ERROR_NOT_AVAILABLE, TRUE, TRUE);
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		double cpuLoad = 0.0;
		omr_error_t rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetSystemCpuLoad(NULL, &cpuLoad), OMR_THREAD_NOT_ATTACHED);

		if (OMR_THREAD_NOT_ATTACHED == rc) {
			omrtty_printf(
				"callIndex: %d: rc = %d (%s), the function call is successful !\n",
				callIndex, OMR_THREAD_NOT_ATTACHED, omrErrorToString(OMR_THREAD_NOT_ATTACHED));
		}
		callIndex += 1;
	}
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("%s: GetSystemCpuLoad() passed\n", agentName);
	}
	return testRc;
}

/**
 * This is the core function to test the user time returned by get_process_times.
 * Its run time is deterministic and it does the same amount of work each time it is invoked.
 * This implementation is copied from thrbasetest/processtimetest.c
 */
static void
matrixSquare(void)
{
	/* Declare and initialize the matrix. memset works here since this is a true 2D
	 * array defined in the same scope as the memset. */
	uintptr_t matrix[MATRIX_SIZE][MATRIX_SIZE];
	uintptr_t i = 0;
	uintptr_t j = 0;

	memset(matrix, 0, sizeof(matrix));

	for (j = 0; j < MATRIX_SIZE; j++) {
		matrix[i][j] = (uintptr_t) pow((double)100, 2.0);
	}
}

/**
 * This is the core function to test system / kernel time returned by get_process_times.
 * Uses file handling sys call to increase time taken in kernel mode
 * Runtime is deterministic and it does the same amount of work each time it is invoked.
 * This implementation is copied from thrbasetest/processtimetest.c
 */

static void
systemTimeCPUBurn(void)
{
	uintptr_t j = 0;
	FILE *tempFile = NULL;

	for (j = 0; j < NUM_ITERATIONS_SYSTEM_CPU_BURN; j++) {
#if defined(WIN32) || defined(WIN64)
		tempFile = fopen("nul", "w");
#else
		tempFile = fopen("/dev/null", "w");
#endif
		fwrite("garbage", 1, sizeof("garbage"), tempFile);
		fclose(tempFile);
	}
}
