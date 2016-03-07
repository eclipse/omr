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

#include "omragent.h"
#include "omrTestHelpers.h"

typedef struct OMRTraceAgentTestData {
	omr_error_t rc;
	UtSubscription *subscriptionID;
} OMRTraceAgentTestData;

static void alarmFunc(UtSubscription *subscriptionID);
static omr_error_t subscribeFunc(UtSubscription *subscriptionID);
static omr_error_t testNegativeCases(OMR_VMThread *vmThread, OMR_TI const *ti);
static omr_error_t testTraceAgentFromUnattachedThread(OMR_TI const *ti, OMRPortLibrary *portLibrary);
static omr_error_t testTraceAgentGetMemorySize(OMR_VMThread *vmThread, OMR_TI const *ti);

static OMRTraceAgentTestData testData;

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	OMR_VMThread *vmThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;
	const char *setOpts[] = { NULL };
	UtSubscription *subscriptionID2 = NULL;

	testData.rc = OMR_ERROR_NONE;
	testData.subscriptionID = NULL;

	/* exercise trace API without attaching thread to VM */
	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(testTraceAgentFromUnattachedThread(ti, OMRPORTLIB));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, NULL, &vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(testTraceAgentGetMemorySize(vmThread, ti));
	}

	if (OMR_ERROR_NONE == rc) {
		omr_error_t apiRc = OMRTEST_PRINT_ERROR(ti->SetTraceOptions(vmThread, setOpts));
		if (OMR_ERROR_NONE != apiRc) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == rc) {
		omr_error_t apiRc = OMRTEST_PRINT_ERROR(ti->GetTraceMetadata(vmThread, &traceMeta, &traceMetaLength));
		if (OMR_ERROR_NONE != apiRc) {
			rc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("   traceMetaLength=%d\n", traceMetaLength);
		}
	}
	if (OMR_ERROR_NONE == rc) {
		omr_error_t apiRc = OMRTEST_PRINT_ERROR(ti->RegisterRecordSubscriber(vmThread, "sample", subscribeFunc, alarmFunc, (void *)"my user data", &testData.subscriptionID));
		if (OMR_ERROR_NONE != apiRc) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->RegisterRecordSubscriber(vmThread, "", subscribeFunc, NULL, NULL, &subscriptionID2));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->DeregisterRecordSubscriber(vmThread, subscriptionID2));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->FlushTraceData(vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(testNegativeCases(vmThread, ti));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(vmThread));
	}

	testData.rc = rc;
	return rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	omr_error_t rc = testData.rc;
	OMR_VMThread *vmThread = NULL;

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, NULL, &vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		omr_error_t apiRc = OMRTEST_PRINT_ERROR(ti->DeregisterRecordSubscriber(vmThread, testData.subscriptionID));
		if (OMR_ERROR_NONE != apiRc) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(vmThread));
	}

	testData.rc = rc;
	return rc;
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

static omr_error_t
testTraceAgentFromUnattachedThread(OMR_TI const *ti, OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;
	OMR_VMThread *vmThread = NULL;
	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;
	UtSubscription *subscriptionID = NULL;
	const char *setOpts[] = { "blah", NULL, NULL };
	uint64_t memorySize = -1;
	size_t methodPropertyCount = 0;
	const char *const *methodPropertyNames;
	size_t sizeofSampledMethodDesc = 0;
	const size_t numMethods = 1;
	OMR_SampledMethodDescription *desc = NULL;
	void *methodArray[1] = { (void *)0 };
	size_t nameBytesRemaining = 0;
	char nameBuffer[5];

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->SetTraceOptions(vmThread, setOpts), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetTraceMetadata(vmThread, &traceMeta, &traceMetaLength), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->FlushTraceData(vmThread), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->RegisterRecordSubscriber(vmThread, "sample", subscribeFunc, alarmFunc, (void *)"my user data", &subscriptionID), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->DeregisterRecordSubscriber(vmThread, subscriptionID), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetFreePhysicalMemorySize(vmThread, &memorySize), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessPrivateMemorySize(vmThread, &memorySize), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessVirtualMemorySize(vmThread, &memorySize), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessPhysicalMemorySize(vmThread, &memorySize), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMethodProperties(vmThread, &methodPropertyCount, &methodPropertyNames, &sizeofSampledMethodDesc), OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(
				 ti->GetMethodDescriptions(vmThread, methodArray, numMethods, desc, nameBuffer, sizeof(nameBuffer), NULL, &nameBytesRemaining),
				 OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	return testRc;
}

static omr_error_t
testTraceAgentGetMemorySize(OMR_VMThread *vmThread, OMR_TI const *ti)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;
	uint64_t memorySize = 0;
#if defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_ERROR(ti->GetFreePhysicalMemorySize(vmThread, &memorySize));
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("%s:%d GetFreePhysicalMemorySize() failed !\n", __FILE__, __LINE__);
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("%s:%d Free physical memory size (in bytes): %llu, rc = %d (%s), the function call is successful !\n", __FILE__, __LINE__, memorySize, rc, omrErrorToString(rc));
		}
	}
#if defined(LINUX) || defined(WIN32) || defined(OSX)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_ERROR(ti->GetProcessVirtualMemorySize(vmThread, &memorySize));
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("%s:%d GetProcessVirtualMemorySize() failed !\n", __FILE__, __LINE__);
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("%s:%d Process virtual memory size (in bytes): %llu, rc = %d (%s), the function call is successful !\n", __FILE__, __LINE__, memorySize, rc, omrErrorToString(rc));
		}
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_ERROR(ti->GetProcessPhysicalMemorySize(vmThread, &memorySize));
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("%s:%d GetProcessPhysicalMemorySize() failed !\n", __FILE__, __LINE__);
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("%s:%d Process physical memory size (in bytes): %llu, rc = %d (%s), the function call is successful !\n", __FILE__, __LINE__, memorySize, rc, omrErrorToString(rc));
		}
	}
#endif /* defined(LINUX) || defined(WIN32) || defined(OSX) */
#if defined(LINUX) || defined(AIXPPC)|| defined(WIN32)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_ERROR(ti->GetProcessPrivateMemorySize(vmThread, &memorySize));
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("%s:%d GetProcessPrivateMemorySize() failed !\n", __FILE__, __LINE__);
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("%s:%d Process private memory size (in bytes): %llu, rc = %d (%s), the function call is successful !\n", __FILE__, __LINE__, memorySize, rc, omrErrorToString(rc));
		}
	}
#endif /* defined(LINUX) || defined(AIXPPC)|| defined(WIN32) */
#endif /* defined(LINUX) || defined(AIXPPC)|| defined(WIN32) || defined(OSX) */
	return testRc;
}

static omr_error_t
testNegativeCases(OMR_VMThread *vmThread, OMR_TI const *ti)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;
	void *traceMeta = NULL;
	int32_t traceMetaLength = 0;
	UtSubscription *subscriptionID = NULL;
	uint64_t *invalidMemorySizePtr = NULL;
#if !defined(LINUX) && !defined(WIN32) && !defined(OSX)
	uint64_t memorySize = -1;
#endif /* !defined(LINUX) && !defined(WIN32) && !defined(OSX) */

	rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetTraceMetadata(vmThread, NULL, &traceMetaLength), OMR_ERROR_ILLEGAL_ARGUMENT);
	if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
		testRc = OMR_ERROR_INTERNAL;
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetTraceMetadata(vmThread, &traceMeta, NULL), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	/* description is NULL */
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->RegisterRecordSubscriber(vmThread, NULL, subscribeFunc, alarmFunc, (void *)"my data", &subscriptionID), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	/* subscribeFunc is NULL */
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->RegisterRecordSubscriber(vmThread, "test", NULL, alarmFunc, (void *)"my data", &subscriptionID), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	/* subscriptionID is NULL */
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->RegisterRecordSubscriber(vmThread, "test", subscribeFunc, alarmFunc, (void *)"my data", NULL), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->DeregisterRecordSubscriber(vmThread, NULL), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	/* Test with invalidMemorySizePtr, OMR_ERROR_ILLEGAL_ARGUMENT is expected */
#if defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetFreePhysicalMemorySize(vmThread, invalidMemorySizePtr), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessPrivateMemorySize(vmThread, invalidMemorySizePtr), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
#if defined(LINUX) || defined(WIN32) || defined(OSX)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessVirtualMemorySize(vmThread, invalidMemorySizePtr), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessPhysicalMemorySize(vmThread, invalidMemorySizePtr), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
#endif /* defined(LINUX) || defined(WIN32) || defined(OSX) */
#endif /* defined(LINUX) || defined(AIXPPC)|| defined(WIN32) || defined(OSX) */

	/* Test GetFreePhysicalMemorySize and GetProcessPrivateMemorySize on platforms other than LINUX, AIXPPC and WIN, OMR_ERROR_NOT_AVAILABLE is expected */
#if !defined(LINUX) && !defined(AIXPPC) && !defined(WIN32)  && !defined(OSX)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetFreePhysicalMemorySize(vmThread, &memorySize), OMR_ERROR_NOT_AVAILABLE);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessPrivateMemorySize(vmThread, &memorySize), OMR_ERROR_NOT_AVAILABLE);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	/* Test GetProcessVirtualMemorySize and GetProcessPhysicalMemorySize on platforms other than LINUX and WIN, OMR_ERROR_NOT_AVAILABLE is expected */
#elif !defined(LINUX) && !defined(WIN32) && !defined(OSX)
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessVirtualMemorySize(vmThread, &memorySize), OMR_ERROR_NOT_AVAILABLE);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetProcessPhysicalMemorySize(vmThread, &memorySize), OMR_ERROR_NOT_AVAILABLE);
		if (OMR_ERROR_NOT_AVAILABLE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

#endif /* !defined(LINUX) && !defined(AIXPPC) && !defined(WIN32) && !defined(OSX) */

	return testRc;
}
