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

/*
 * Test trace TI functions when the trace engine has not been started.
 * For testing purpose, this test agent will only use public header files (no internal or OMR test libraries)
 */

static omr_error_t subscribeFunc(UtSubscription *subscriptionID);
static void alarmFunc(UtSubscription *subscriptionID);
static omr_error_t testTraceNotStarted(OMR_TI const *ti, OMR_VMThread *vmThread);

static const char *agentName = "traceNotStartedAgent";

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	OMR_VMThread *vmThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	printf("%s: Agent_OnLoad(options=\"%s\")\n", agentName, options);

	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, "sample main", &vmThread);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: BindCurrentThread failed, rc=%d\n", agentName, rc);
		} else if (NULL == vmThread) {
			printf("%s: BindCurrentThread failed, NULL vmThread was returned\n", agentName);
			rc = OMR_ERROR_INTERNAL;
		} else {
			printf("%s: BindCurrentThread passed, vmThread=0x%p\n", agentName, vmThread);
		}
	}

	/* exercise trace API */
	if (OMR_ERROR_NONE == rc) {
		rc = testTraceNotStarted(ti, vmThread);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: UnbindCurrentThread failed, rc=%d\n", agentName, rc);
		} else {
			printf("%s: UnbindCurrentThread passed\n", agentName);
		}
	}
	return rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	printf("%s: Agent_OnUnload\n", agentName);
	return OMR_ERROR_NONE;
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

/**
 * Test tracing API when trace is not started.
 * Nothing should crash.
 * Error codes should be returned.
 */
static omr_error_t
testTraceNotStarted(OMR_TI const *ti, OMR_VMThread *vmThread)
{
	omr_error_t testRc = OMR_ERROR_NONE;
	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;
	UtSubscription *subscriptionID = NULL;
	const char *setOpts[] = { "blah", NULL, NULL };

	if (OMR_ERROR_NONE == testRc) {
		omr_error_t rc = ti->SetTraceOptions(vmThread, setOpts);
		printf("%s: SetTraceOptions: rc = %d\n", agentName, rc);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_NOT_AVAILABLE)\n", OMR_ERROR_NOT_AVAILABLE);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		omr_error_t rc = ti->GetTraceMetadata(vmThread, &traceMeta, &traceMetaLength);
		printf("%s: GetTraceMetadata: rc = %d\n", agentName, rc);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_NOT_AVAILABLE)\n", OMR_ERROR_NOT_AVAILABLE);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		omr_error_t rc = ti->RegisterRecordSubscriber(vmThread, "sample", subscribeFunc, alarmFunc, (void *)"my user data", &subscriptionID);
		printf("%s: RegisterRecordSubscriber: rc = %d\n", agentName, rc);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_NOT_AVAILABLE)\n", OMR_ERROR_NOT_AVAILABLE);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		omr_error_t rc = ti->FlushTraceData(vmThread);
		printf("%s: FlushTraceData: rc = %d\n", agentName, rc);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_NOT_AVAILABLE)\n", OMR_ERROR_NOT_AVAILABLE);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		omr_error_t rc = ti->DeregisterRecordSubscriber(vmThread, subscriptionID);
		printf("%s: DeregisterRecordSubscriber: rc = %d\n", agentName, rc);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_ILLEGAL_ARGUMENT)\n", OMR_ERROR_ILLEGAL_ARGUMENT);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	return testRc;
}
