/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdio.h>
#include "omragent.h"
#include "omragent_internal.h"
#include "omrTestHelpers.h"

typedef struct TestChildThreadData {
	OMR_VM *testVM;
	OMR_TI const *ti;
	omr_error_t childRc;
} TestChildThreadData;

typedef struct OMRAgentTestData {
	UtSubscription *subscriptionID;
	FILE *outfile;
	omr_error_t rc;
	const char *failureFile;
	int failureLine;
	const char *trcFileName;
	OMR_VMThread *vmThread;
	int tracePointSubscribeFuncCount;
	int completed;
} OMRAgentTestData;

static void alarmFunc1(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc1(UtSubscription *subscriptionID);
static void alarmFunc2(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc2(UtSubscription *subscriptionID);
static omr_error_t initTestData(OMRAgentTestData *testData, OMR_VM *vm, char const *outfile);
static omr_error_t verifyTraceMetadata(char *traceMeta, OMR_VM *vm);

static int J9THREAD_PROC childThreadMain(void *entryArg);
static omr_error_t startTestChildThread(OMR_TI const *ti, OMR_VM *testVM, OMR_VMThread *curVMThread, omrthread_t *childThread, TestChildThreadData **childData);
static omr_error_t testTraceAPIs(OMR_TI const *ti, OMR_VM *vm, const char *threadName);
static omr_error_t waitForTestChildThread(OMR_TI const *ti, OMR_VM *testVM, omrthread_t childThread, TestChildThreadData *childData);

static OMRAgentTestData testData1;
static OMRAgentTestData testData2;

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_VMThread *curThread = NULL;
	TestChildThreadData *childData = NULL;
	omrthread_t childThread = NULL;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(startTestChildThread(ti, vm, curThread, &childThread, &childData));
	}
	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(waitForTestChildThread(ti, vm, childThread, childData));
	}

	return rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_VMThread *vmThread = NULL;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "test thread3", &vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		if (0 != testData2.completed) {
			omrtty_printf("%s:%d ERROR: Failed to call alarmFunc2()\n", __FILE__, __LINE__);
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/* OMR_ERROR_ILLEGAL_ARGUMENT is expected if alarmFunc1 is executed, otherwise expect OMR_ERROR_NONE to return */
	if (OMR_ERROR_NONE == rc) {
		omr_error_t testRc = ti->DeregisterRecordSubscriber(vmThread, testData1.subscriptionID);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != testRc && OMR_ERROR_NONE != testRc) {
			rc = testRc;
			omrtty_printf("%s:%d ERROR: DeregisterRecordSubscriber() returns unexpected rc: %d\n", __FILE__, __LINE__, rc);
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->DeregisterRecordSubscriber(vmThread, testData2.subscriptionID));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		if (1 != testData1.completed && 1 != testData2.completed) {
			omrtty_printf("%s:%d ERROR: Failed to call alarmFunc1() and alarmFunc2()\n", __FILE__, __LINE__);
			rc = OMR_ERROR_INTERNAL;
		}
	}

	fclose(testData1.outfile);
	fclose(testData2.outfile);

	omrfile_unlink(testData1.trcFileName);
	omrfile_unlink(testData2.trcFileName);

	return rc;
}


static omr_error_t
startTestChildThread(OMR_TI const *ti, OMR_VM *testVM, OMR_VMThread *curVMThread, omrthread_t *childThread, TestChildThreadData **childData)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(testVM);
	TestChildThreadData *newChildData = (TestChildThreadData *)omrmem_allocate_memory(sizeof(*newChildData), OMRMEM_CATEGORY_VM);
	OMR_ThreadAPI *threadAPI = (OMR_ThreadAPI *)ti->internalData;
	omrthread_attr_t attr = NULL;

	if (NULL == newChildData) {
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		omrtty_printf("%s:%d ERROR: Failed to alloc newChildData\n", __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		newChildData->ti = ti;
		newChildData->testVM = testVM;
		newChildData->childRc = OMR_ERROR_NONE;

		rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(threadAPI->omrthread_attr_init(&attr), J9THREAD_SUCCESS);
	}
	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(threadAPI->omrthread_attr_set_detachstate(&attr, J9THREAD_CREATE_JOINABLE), J9THREAD_SUCCESS);

		if (OMR_ERROR_NONE == rc) {
			rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(
					threadAPI->omrthread_create_ex(childThread, &attr, FALSE, childThreadMain, newChildData),
					J9THREAD_SUCCESS);
		}
		if (OMR_ERROR_NONE == rc) {
			*childData = newChildData;
		}

		rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(threadAPI->omrthread_attr_destroy(&attr), J9THREAD_SUCCESS);
	}
	if ((OMR_ERROR_NONE != rc) && (NULL != newChildData)) {
		omrmem_free_memory(newChildData);
	}
	return rc;
}

static omr_error_t
waitForTestChildThread(OMR_TI const *ti, OMR_VM *testVM, omrthread_t childThread, TestChildThreadData *childData)
{
	omr_error_t childRc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(testVM);
	OMR_ThreadAPI *threadAPI = (OMR_ThreadAPI *)ti->internalData;

	childRc = OMRTEST_PRINT_UNEXPECTED_INT_RC(threadAPI->omrthread_join(childThread), J9THREAD_SUCCESS);

	if (OMR_ERROR_NONE == childRc) {
		childRc = childData->childRc;
	}
	omrmem_free_memory(childData);

	return childRc;
}

static int J9THREAD_PROC
childThreadMain(void *entryArg)
{
	TestChildThreadData *childData = (TestChildThreadData *)entryArg;

	childData->childRc = testTraceAPIs(childData->ti, childData->testVM, "child thread");
	return 0;
}

/**
 * Test
 * - non-recursive bind, recursive bind
 * - call SetTraceOptions
 * - call GetTraceMetadata
 * - call RegisterRecordSubscriber
 * - call SetTraceOptions
 * - call RegisterRecordSubscriber with different subscribeFunc and alarmFunc
 * - call FlushTraceData
 * - non-recursive unbind, recursive unbind
 */
static omr_error_t
testTraceAPIs(OMR_TI const *ti, OMR_VM *vm, const char *threadName)
{
	omr_error_t rc = OMR_ERROR_NONE;
	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	initTestData(&testData1, vm, "subscriberAgentWithJ9threadTrace1.out");
	initTestData(&testData2, vm, "subscriberAgentWithJ9threadTrace2.out");

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "test thread1", &testData1.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "test thread2", &testData2.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		const char *setOpts[] = { "maximal", "omrport", NULL};
		rc = OMRTEST_PRINT_ERROR(ti->SetTraceOptions(testData1.vmThread, setOpts));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->GetTraceMetadata(testData1.vmThread, &traceMeta, &traceMetaLength));
		if (OMR_ERROR_NONE == rc) {
			rc = OMRTEST_PRINT_ERROR(verifyTraceMetadata(traceMeta, vm));
			if (OMR_ERROR_NONE == rc) {
				fwrite(traceMeta, traceMetaLength, 1, testData1.outfile);
				fwrite(traceMeta, traceMetaLength, 1, testData2.outfile);
			}
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->RegisterRecordSubscriber(testData1.vmThread, "sample1", subscribeFunc1, alarmFunc1, (void *)&testData1, &testData1.subscriptionID));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->RegisterRecordSubscriber(testData2.vmThread, "sample2", subscribeFunc2, alarmFunc2, (void *)&testData2, &testData2.subscriptionID));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->FlushTraceData(testData1.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(testData1.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(testData2.vmThread));
	}
	return rc;
}

static omr_error_t
subscribeFunc1(UtSubscription *subscriptionID)
{
	testData1.tracePointSubscribeFuncCount++;
	fwrite(subscriptionID->data, subscriptionID->dataLength, 1, testData1.outfile);

	if (2 == testData1.tracePointSubscribeFuncCount) {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	return OMR_ERROR_NONE;
}

static void
alarmFunc1(UtSubscription *subscriptionID)
{
	testData1.completed = 1;
	return;
}

static omr_error_t
subscribeFunc2(UtSubscription *subscriptionID)
{
	testData2.tracePointSubscribeFuncCount++;
	fwrite(subscriptionID->data, subscriptionID->dataLength, 1, testData2.outfile);
	return OMR_ERROR_NONE;
}

static void
alarmFunc2(UtSubscription *subscriptionID)
{
	testData2.completed = 1;
	return;
}

static omr_error_t
initTestData(struct OMRAgentTestData *testData, OMR_VM *vm, char const *outfile)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	testData->rc = OMR_ERROR_NONE;
	testData->subscriptionID = NULL;
	testData->trcFileName = outfile;
	testData->vmThread = NULL;
	testData->completed = 0;
	testData->outfile = fopen(testData->trcFileName, "wb");
	if (NULL == testData->outfile) {
		rc = OMR_ERROR_INTERNAL;
		omrtty_printf("Failed to open file\n");
	}
	return rc;
}

/*
 * Verify trace metadata starts with eyecatcher "UTTH"
 */
static omr_error_t
verifyTraceMetadata(char *traceMeta, OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	const char *eyecatcherASCII = "UTTH";
	char eyecatcherEBCDIC[4] = {228, 227, 227, 200};
	int i;
	for (i = 0; i < 4; i++) {
		char c = traceMeta[i];
		if (eyecatcherASCII[i] != c && eyecatcherEBCDIC[i] != c) {
			rc = OMR_ERROR_INTERNAL;
			omrtty_printf("eyecatcher in trace metadata at index %d doesn't match expectation. Found: %c\n", i, c);
			break;
		}
	}
	return rc;
}
