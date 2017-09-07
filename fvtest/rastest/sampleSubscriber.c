/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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

typedef struct AgentData {
	UtSubscription *subscriptionID;
	FILE *outfile;
	omr_error_t rc;
	const char *failureFile;
	int failureLine;
	const char *trcFileName;
} AgentData;


static void alarmFunc(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc(UtSubscription *subscriptionID);
static void recordErrorStatus(omr_error_t rc, const char *filename, int lineno);

static AgentData agentData;

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	OMR_VMThread *vmThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;
	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;

	agentData.rc = OMR_ERROR_NONE;
	agentData.subscriptionID = NULL;
	agentData.trcFileName = options;

	agentData.outfile = fopen(agentData.trcFileName, "wb");
	if (NULL == agentData.outfile) {
		rc = OMR_ERROR_INTERNAL;
		recordErrorStatus(rc, __FILE__, __LINE__);
		fprintf(stderr, "Failed to open file\n");
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, "onload thread", &vmThread);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->GetTraceMetadata(vmThread, &traceMeta, &traceMetaLength);
		recordErrorStatus(rc, __FILE__, __LINE__);
		if (OMR_ERROR_NONE == rc) {
			fwrite(traceMeta, traceMetaLength, 1, agentData.outfile);
		}
	}
	if (OMR_ERROR_NONE == rc) {
		rc = ti->RegisterRecordSubscriber(vmThread, "sample", subscribeFunc, alarmFunc, (void *)&agentData, &agentData.subscriptionID);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE != agentData.rc) {
		printf(">> agent: OnLoad failed at %s:%d\n", agentData.failureFile, agentData.failureLine);
	}
	return agentData.rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	omr_error_t rc = agentData.rc;
	OMR_VMThread *vmThread = NULL;

	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, "onunload thread", &vmThread);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->DeregisterRecordSubscriber(vmThread, agentData.subscriptionID);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		recordErrorStatus(rc, __FILE__, __LINE__);
	}

	fclose(agentData.outfile);

	if (OMR_ERROR_NONE != agentData.rc) {
		printf(">> agent: OnUnload failed at %s: %d\n", agentData.failureFile, agentData.failureLine);
	}
	return agentData.rc;
}

static omr_error_t
subscribeFunc(UtSubscription *subscriptionID)
{
	fwrite(subscriptionID->data, subscriptionID->dataLength, 1, agentData.outfile);
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
		agentData.rc = rc;
		agentData.failureFile = filename;
		agentData.failureLine = lineno;
	}
}
