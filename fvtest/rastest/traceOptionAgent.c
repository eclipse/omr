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

typedef struct OMRTraceAgentTestData {
	UtSubscription *subscriptionID;
	omr_error_t rc;
	const char *failureFile;
	int failureLine;
} OMRTraceAgentTestData;


static void alarmFunc(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc(UtSubscription *subscriptionID);
static void recordErrorStatus(omr_error_t rc, const char *filename, int lineno);

static OMRTraceAgentTestData testData;
OMR_VMThread *vmThread = NULL;

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	omr_error_t rc = OMR_ERROR_NONE;
	testData.rc = OMR_ERROR_NONE;
	testData.subscriptionID = NULL;

	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, "onload thread", &vmThread);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	/* Test valid OMR trace option */
	if (OMR_ERROR_NONE == rc) {
		const char *setOpts[] =	{ "none", "all", NULL };
		rc = ti->SetTraceOptions(vmThread, setOpts);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	/* Test unsupported option */
	if (OMR_ERROR_NONE == rc) {
		const char *setOpts[] =	{ "INVALID OPT", "VALUE", NULL };
		omr_error_t testRc = ti->SetTraceOptions(vmThread, setOpts);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != testRc) {
			rc = OMR_ERROR_INTERNAL;
		}
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	/* Test invalid OMR trace option (but valid for JVM), OMR_ERROR_ILLEGAL_ARGUMENT is expected */
	if (OMR_ERROR_NONE == rc) {
		const char *setOpts[] =	{ "methods", "java/lang/*", NULL };
		omr_error_t testRc = ti->SetTraceOptions(vmThread, setOpts);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != testRc) {
			rc = OMR_ERROR_INTERNAL;
		}
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->RegisterRecordSubscriber(vmThread, "sample", subscribeFunc, alarmFunc, (void *)&testData, &testData.subscriptionID);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	/* Test valid OMR trace option */
	if (OMR_ERROR_NONE == rc) {
		const char *setOpts[] =	{ "count", "omr_test", NULL };
		rc = ti->SetTraceOptions(vmThread, setOpts);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE != testData.rc) {
		fprintf(stderr, ">> agent: OnLoad failed at %s:%d\n", testData.failureFile, testData.failureLine);
	}
	return testData.rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	omr_error_t rc = testData.rc;

	if (OMR_ERROR_NONE == rc) {
		rc = ti->DeregisterRecordSubscriber(vmThread, testData.subscriptionID);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE != testData.rc) {
		fprintf(stderr, ">> agent: OnUnload failed at %s: %d\n", testData.failureFile, testData.failureLine);
	}
	return testData.rc;
}

static omr_error_t
subscribeFunc(UtSubscription *subscriptionID)
{
	return OMR_ERROR_NONE;
}

static void
alarmFunc(UtSubscription *subscriptionID)
{
	return;
}

static void
recordErrorStatus(omr_error_t rc, const char *filename, int lineno)
{
	if (OMR_ERROR_NONE != rc) {
		testData.rc = rc;
		testData.failureFile = filename;
		testData.failureLine = lineno;
	}
}
