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

#include <stdio.h>
#include "omragent.h"
#include "omrTestHelpers.h"

/*
 * Test Scenario:
 * - In OnLoad
 * 	-> call BindCurrentThread twice
 * 	-> call SetTraceOptions
 *  -> call GetTraceMetadata
 *  -> call RegisterRecordSubscriber
 *  -> call SetTraceOptions
 *  -> call RegisterRecordSubscriber with different subscribeFunc and alarmFunc
 *  -> call FlushTraceData
 *  -> call UnbindCurrentThread twice
 *
 * - In OnUnload
 *  -> call BindCurrentThread twice
 *  -> call DeregisterRecordSubscriber to deregister two subscribers
 *  -> call DeregisterRecordSubscriber again, expect error
 *  -> call UnbindCurrentThread twice
 */

typedef struct OMRAgentTestData {
	UtSubscription *subscriptionID;
	FILE *outfile;
	const char *trcFileName;
	OMR_VMThread *vmThread;
	int tracePointSubscribeFuncCount;
	int completed;
} OMRAgentTestData;

static void alarmFunc1(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc1(UtSubscription *subscriptionID);
static void alarmFunc2(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc2(UtSubscription *subscriptionID);
static omr_error_t initTestData(OMRAgentTestData *testData, char const *outfile);
static omr_error_t verifyTraceMetadata(char *traceMeta);

static OMRAgentTestData testData1;
static OMRAgentTestData testData2;

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	omr_error_t rc = OMR_ERROR_NONE;
	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	initTestData(&testData1, "subscriberAgentTrace1.out");
	initTestData(&testData2, "subscriberAgentTrace2.out");

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
			rc = OMRTEST_PRINT_ERROR(verifyTraceMetadata(traceMeta));
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

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "test thread1", &testData1.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "test thread2", &testData2.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		if (0 != testData2.completed) {
			omrtty_printf("%s:%d ERROR: Failed to call alarmFunc2()\n", __FILE__, __LINE__);
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/* OMR_ERROR_ILLEGAL_ARGUMENT is expected if alarmFunc1 is executed, otherwise expect OMR_ERROR_NONE to return */
	if (OMR_ERROR_NONE == rc) {
		omr_error_t testRc = ti->DeregisterRecordSubscriber(testData1.vmThread, testData1.subscriptionID);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != testRc && OMR_ERROR_NONE != testRc) {
			omrtty_printf("%s:%d ERROR: DeregisterRecordSubscriber() returns expected rc: %d\n", __FILE__, __LINE__, rc);
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->DeregisterRecordSubscriber(testData2.vmThread, testData2.subscriptionID));
	}

	/* Call DeregisterRecordSubscriber one more time, OMR_ERROR_ILLEGAL_ARGUMENT is expected */
	if (OMR_ERROR_NONE == rc) {
		omr_error_t testRc = OMRTEST_PRINT_UNEXPECTED_RC(ti->DeregisterRecordSubscriber(testData2.vmThread, testData2.subscriptionID), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != testRc) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(testData1.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(testData2.vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		if (1 != testData1.completed && 1 != testData2.completed) {
			omrtty_printf("%s:%d ERROR: Failed to call alarmFunc1() and alarmFunc2()\n", __FILE__, __LINE__);
			rc = OMR_ERROR_INTERNAL;
		}
	}

	fclose(testData1.outfile);
	fclose(testData2.outfile);

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
initTestData(struct OMRAgentTestData *testData, char const *outfile)
{
	omr_error_t rc = OMR_ERROR_NONE;
	testData->subscriptionID = NULL;
	testData->trcFileName = outfile;
	testData->vmThread = NULL;
	testData->completed = 0;
	testData->outfile = fopen(testData->trcFileName, "wb");
	if (NULL == testData->outfile) {
		rc = OMR_ERROR_INTERNAL;
		fprintf(stderr, "Failed to open file\n");
	}
	return rc;
}

/*
 * Verify trace metadata starts with eyecatcher "UTTH"
 */
static omr_error_t
verifyTraceMetadata(char *traceMeta)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const char *eyecatcherASCII = "UTTH";
	char eyecatcherEBCDIC[4] = {228, 227, 227, 200};
	int i;
	for (i = 0; i < 4; i++) {
		char c = traceMeta[i];
		if (eyecatcherASCII[i] != c && eyecatcherEBCDIC[i] != c) {
			rc = OMR_ERROR_INTERNAL;
			fprintf(stderr,	"eyecatcher in trace metadata at index %d doesn't match expectation. Found: %c\n", i, c);
			break;
		}
	}
	return rc;
}
