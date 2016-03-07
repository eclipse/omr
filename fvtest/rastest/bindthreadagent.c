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

#include <stdio.h>
#include "omragent.h"
#include "omragent_internal.h"
#include "omrTestHelpers.h"

typedef struct TestChildThreadData {
	OMR_VM *testVM;
	OMR_TI const *ti;
	omrthread_monitor_t shutdownCond;
	BOOLEAN isDead;
	omr_error_t childRc;
} TestChildThreadData;

static int J9THREAD_PROC childThreadMain(void *entryArg);
static omr_error_t startTestChildThread(OMR_TI const *ti, OMR_VM *testVM, OMR_VMThread *curVMThread, omrthread_t *childThread, TestChildThreadData **childData);
static omr_error_t testBindUnbind(OMR_TI const *ti, OMR_VM *vm, const char *threadName);
static omr_error_t testBindUnbindNegativeCases(OMR_TI const *ti, OMR_VM *vm);
static omr_error_t waitForTestChildThread(OMR_TI const *ti, OMR_VM *testVM, omrthread_t childThead, TestChildThreadData *childData);

static const char *agentName = "bindthreadagent";

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	omr_error_t rc = OMRTEST_PRINT_ERROR(testBindUnbind(ti, vm, "Initial thread"));

	if (OMR_ERROR_NONE == rc) {
		OMR_VMThread *curThread = NULL;
		TestChildThreadData *childData = NULL;
		omrthread_t childThread = NULL;

		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "Initial thread", &curThread));
		if (OMR_ERROR_NONE == rc) {
			rc = OMRTEST_PRINT_ERROR(startTestChildThread(ti, vm, curThread, &childThread, &childData));
		}
		if (OMR_ERROR_NONE == rc) {
			rc = OMRTEST_PRINT_ERROR(waitForTestChildThread(ti, vm, childThread, childData));
		}
		if (OMR_ERROR_NONE == rc) {
			rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(curThread));
		}
	}
	return rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	return OMR_ERROR_NONE;
}


static omr_error_t
startTestChildThread(OMR_TI const *ti, OMR_VM *testVM, OMR_VMThread *curVMThread, omrthread_t *childThread, TestChildThreadData **childData)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(testVM);
	TestChildThreadData *newChildData = (TestChildThreadData *)omrmem_allocate_memory(sizeof(*newChildData), OMRMEM_CATEGORY_VM);
	OMR_ThreadAPI *threadAPI = (OMR_ThreadAPI *)ti->internalData;

	if (NULL == newChildData) {
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		omrtty_printf("%s:%d ERROR: Failed to alloc newChildData\n", __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		if (0 != threadAPI->omrthread_monitor_init_with_name(&newChildData->shutdownCond, 0, "traceTestChildShutdown")) {
			rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
			omrtty_printf("%s:%d ERROR: Failed to init shutdownCond monitor\n", __FILE__, __LINE__);
			omrmem_free_memory(newChildData);
		} else {
			newChildData->ti = ti;
			newChildData->testVM = testVM;
			newChildData->isDead = FALSE;
			newChildData->childRc = OMR_ERROR_NONE;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		if (0 != threadAPI->omrthread_create_ex(
			NULL, /* handle */
			J9THREAD_ATTR_DEFAULT, /* attr */
			FALSE, /* suspend */
			childThreadMain, /* entrypoint */
			newChildData) /* entryarg */
		) {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			omrtty_printf("%s:%d ERROR: Failed to init shutdownCond monitor\n", __FILE__, __LINE__);
			threadAPI->omrthread_monitor_destroy(newChildData->shutdownCond);
			omrmem_free_memory(newChildData);
		} else {
			*childData = newChildData;
		}
	}
	return rc;
}

static omr_error_t
waitForTestChildThread(OMR_TI const *ti, OMR_VM *testVM, omrthread_t childThead, TestChildThreadData *childData)
{
	omr_error_t childRc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(testVM);
	OMR_ThreadAPI *threadAPI = (OMR_ThreadAPI *)ti->internalData;

	threadAPI->omrthread_monitor_enter(childData->shutdownCond);
	while (!childData->isDead) {
		threadAPI->omrthread_monitor_wait(childData->shutdownCond);
	}
	threadAPI->omrthread_monitor_exit(childData->shutdownCond);

	childRc = childData->childRc;

	threadAPI->omrthread_monitor_destroy(childData->shutdownCond);
	omrmem_free_memory(childData);

	/*
	 * Workaround for the design limitation of the original j9thr library
	 * Sleep for 0.2 sec to avoid unloading the agent library before the child thread has completed.
	 * For more detail, please see JAZZ103 74016
	 */
#ifdef WIN32
	Sleep(200);
#else
	usleep(200000);
#endif

	return childRc;
}

static int J9THREAD_PROC
childThreadMain(void *entryArg)
{
	omr_error_t rc = OMR_ERROR_NONE;
	TestChildThreadData *childData = (TestChildThreadData *)entryArg;
	OMR_ThreadAPI *threadAPI = (OMR_ThreadAPI *)childData->ti->internalData;

	rc = testBindUnbind(childData->ti, childData->testVM, "child thread");

	threadAPI->omrthread_monitor_enter(childData->shutdownCond);
	childData->isDead = TRUE;
	threadAPI->omrthread_monitor_notify_all(childData->shutdownCond);
	threadAPI->omrthread_monitor_exit(childData->shutdownCond);
	return rc;
}

/**
 * Test
 * - non-recursive bind, recursive bind
 * - non-recursive unbind, recursive unbind
 */
static omr_error_t
testBindUnbind(OMR_TI const *ti, OMR_VM *vm, const char *threadName)
{
	OMR_VMThread *vmThread = NULL;
	OMR_VMThread *vmThread2 = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	/* test non-recursive attach */
	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, threadName, &vmThread);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: BindCurrentThread failed, rc=%d\n", agentName, rc);
		} else if (NULL == vmThread) {
			printf("%s: BindCurrentThread failed, NULL vmThread was returned\n", agentName);
			rc = OMR_ERROR_INTERNAL;
		} else {
			printf("%s: BindCurrentThread passed, vmThread=%p\n", agentName, vmThread);
		}
	}

	/* test recursive attach */
	if (OMR_ERROR_NONE == rc) {
		rc = ti->BindCurrentThread(vm, threadName, &vmThread2);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: recursive BindCurrentThread failed, rc=%d\n", agentName, rc);
		} else if (NULL == vmThread2) {
			printf("%s: recursive BindCurrentThread failed, NULL vmThread2 was returned\n", agentName);
			rc = OMR_ERROR_INTERNAL;
		} else if (vmThread != vmThread2) {
			printf("%s: recursive BindCurrentThread failed, vmThread (%p) != vmThread2 (%p)\n", agentName, vmThread, vmThread2);
			rc = OMR_ERROR_INTERNAL;
		} else {
			printf("%s: recursive BindCurrentThread passed, vmThread2=%p\n", agentName, vmThread2);
		}
	}

	/* test recursive detach */
	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: recursive UnbindCurrentThread failed, rc=%d\n", agentName, rc);
		} else {
			printf("%s: recursive UnbindCurrentThread passed\n", agentName);
		}
	}

	/* test non-recursive detach */
	if (OMR_ERROR_NONE == rc) {
		rc = ti->UnbindCurrentThread(vmThread);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: UnbindCurrentThread failed, rc=%d\n", agentName, rc);
		} else {
			printf("%s: UnbindCurrentThread passed\n", agentName);
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = testBindUnbindNegativeCases(ti, vm);
		if (OMR_ERROR_NONE != rc) {
			printf("%s: testBindUnbindNegativeCases failed, rc=%d\n", agentName, rc);
		}
	}

	return rc;
}

static omr_error_t
testBindUnbindNegativeCases(OMR_TI const *ti, OMR_VM *vm)
{
	OMR_VMThread *vmThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;

	if (OMR_ERROR_NONE == testRc) {
		rc = ti->UnbindCurrentThread(vmThread);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			printf("  Did not get expected rc (%d OMR_THREAD_NOT_ATTACHED)\n", OMR_THREAD_NOT_ATTACHED);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = ti->BindCurrentThread(NULL, "test thread", &vmThread);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_ILLEGAL_ARGUMENT)\n", OMR_ERROR_ILLEGAL_ARGUMENT);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = ti->BindCurrentThread(vm, "test thread", NULL);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			printf("  Did not get expected rc (%d OMR_ERROR_ILLEGAL_ARGUMENT)\n", OMR_ERROR_ILLEGAL_ARGUMENT);
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	return testRc;
}
