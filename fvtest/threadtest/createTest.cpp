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


#if defined(J9ZOS390) || defined(LINUX) || defined(AIXPPC) || defined(OSX)
#include <pthread.h>
#include <limits.h>	/* for PTHREAD_STACK_MIN */
#if defined(LINUX) || defined(OSX)
#include <unistd.h> /* required for the _SC_PAGESIZE  constant */
#endif /* defined(LINUX) || defined(OSX) */
#endif /* defined(J9ZOS390) || defined(LINUX) || defined(AIXPPC) || defined(OSX) */
#include "createTestHelper.h"
#include "common/omrthreadattr.h"
#if defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX)
#include "unix/unixthreadattr.h"
#endif /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX) */
#include "thread_api.h"
#include "thrdsup.h"
#include "omrTest.h"
#include "testHelper.hpp"

#define END_IF_FAILED(status) \
	do { if (status != 0) goto endtest; } while(0)

#define INVALID_VALUE(rc) \
	(((rc) & ~J9THREAD_ERR_OS_ERRNO_SET) == J9THREAD_ERR_INVALID_VALUE)

extern ThreadTestEnvironment *omrTestEnv;
extern "C" {
	OMRPortLibrary *portLib;
}

class ThreadCreateTest: public ::testing::Test
{
protected:
	static void
	SetUpTestCase()
	{
		portLib = omrTestEnv->getPortLibrary();
#if defined(LINUX)
		if (omrTestEnv->realtime) {
			omrthread_lib_control(J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING, 1);
		}
		if (omrthread_lib_use_realtime_scheduling()) {
			initRealtimePrioMap();
		} else {
			initPrioMap();
		}
#else
		initPrioMap();
#endif /* defined(LINUX) */
	}
};

typedef struct create_attr_t {
	const char *name;
	uintptr_t stacksize;
	omrthread_prio_t priority;
	omrthread_schedpolicy_t policy;

	size_t osStacksize;
	int osPriority;
	int osPolicy;
	int osInheritsched;
	int osThreadweight;
	int osDetachstate;
	int osScope;
} create_attr_t;

typedef struct threaddata_t {
	const create_attr_t *expected;
	uintptr_t status;
	int curPriority;
	int curPolicy;
} threaddata_t;

typedef struct numadata_t {
	omrthread_monitor_t monitor;
	uintptr_t expectedAffinity;
	uintptr_t status;
} numadata_t;

static int threadmain(void *arg);
static uintptr_t canCreateThread(const create_attr_t *expected, const omrthread_attr_t attr);
static uintptr_t isAttrOk(const create_attr_t *expected, const omrthread_attr_t actual);
static void initDefaultExpected(create_attr_t *expected);
static size_t getOsStacksize(size_t stacksize);
static int getOsPolicy(omrthread_schedpolicy_t policy, omrthread_prio_t priority);
static void getCurrentOsSched(int *priority, int *policy);
static int numaSetAffinitySuspendedThreadMain(void *arg);
static int numaSetAffinityThreadMain(void *arg);

static void
printMismatchU(const char *property, uintptr_t expected, uintptr_t actual)
{
	PRINTF("wrong %s: expected=%d, actual=%d\n", property, expected, actual);
}

static void
printMismatchI(const char *property, intptr_t expected, intptr_t actual)
{
	PRINTF("wrong %s: expected=%d, actual=%d\n", property, expected, actual);
}

static void
printMismatchS(const char *property, const char *expected, const char *actual)
{
	PRINTF("wrong %s: expected=%s, actual=%s\n", property, expected, actual);
}

static const char *
mapOSInherit(intptr_t policy)
{
	switch (policy) {
	case OS_PTHREAD_EXPLICIT_SCHED:
		return "PTHREAD_EXPLICIT_SCHED";
	case OS_PTHREAD_INHERIT_SCHED:
		return "PTHREAD_INHERIT_SCHED";
	default:
		return "unknown inheritsched attribute";
	}
}

/*
 * The usage of "expected" is not threadsafe. Ensure that expected doesn't go out of scope
 * while the child thread is running, and that the parent waits for the child thread to
 * complete before modifying expected.
 */

static int
threadmain(void *arg)
{
#if defined(J9ZOS390)
	/* Threads are detached. It isn't safe to access data in this thread. */
	return 0;
#else
	threaddata_t *data = (threaddata_t *)arg;
	const create_attr_t *expected = data->expected;
	int osPolicy;
	int osPriority;

	getCurrentOsSched(&osPriority, &osPolicy);

	if (!omrthread_lib_use_realtime_scheduling() && (expected->policy == J9THREAD_SCHEDPOLICY_INHERIT)) {
		if (osPriority != data->curPriority) {
			printMismatchI("child thread priority", data->curPriority, osPriority);
			data->status |= WRONG_OS_PRIORITY;
		}
		if (osPolicy != data->curPolicy) {
			printMismatchS("child thread policy", mapOSPolicy(data->curPolicy), mapOSPolicy(osPolicy));
			data->status |= WRONG_OS_SCHEDPOLICY;
		}
	} else {
		if (osPriority != expected->osPriority) {
			printMismatchI("child thread priority", expected->osPriority, osPriority);
			data->status |= WRONG_OS_PRIORITY;
		}

		if (osPolicy != expected->osPolicy) {
			printMismatchS("child thread policy", mapOSPolicy(expected->osPolicy), mapOSPolicy(osPolicy));
#if defined(LINUXPPC)
			/* bug? Some Linux PPC versions create the thread with the wrong policy.
			 * Warn, but don't flag this as an error. The parent thread has already
			 * checked for errors in the pthread_attr_t.
			 */
			if (!((osPolicy == OS_SCHED_OTHER) && (expected->osPolicy != OS_SCHED_OTHER))) {
				data->status |= WRONG_OS_SCHEDPOLICY;
			} else {
				PRINTF("  LINUXPPC: ignoring wrong schedpolicy\n");
			}
#else
			data->status |= WRONG_OS_SCHEDPOLICY;
#endif
		}
	}
	/* no way to verify stack size */
	return 0;
#endif
}

static uintptr_t
canCreateThread(const create_attr_t *expected, const omrthread_attr_t attr)
{
	uintptr_t status = 0;
	uintptr_t rc;
	omrthread_t handle;
	threaddata_t data;

	data.expected = expected;
	data.status = 0;
	getCurrentOsSched(&data.curPriority, &data.curPolicy);

	rc = J9THREAD_VERBOSE(omrthread_create_ex(&handle, (attr? &attr : J9THREAD_ATTR_DEFAULT), 1, threadmain, &data));
	if (J9THREAD_SUCCESS == rc) {
		OSTHREAD tid = handle->handle;

		omrthread_resume(handle);

#if defined(SPEC_PTHREAD_API)
#if defined(LINUX)
		/* bug? Linux can return a success code, but still fail to create the thread.
		 * NOTE: It is not guaranteed that tid 0 is invalid in all pthread implementations.
		 */
		if (0 == tid) {
			PRINTF("  LINUX: tid 0 returned\n");
			status |= CREATE_FAILED;
			return status;
		}
#endif /* defined(LINUX) */
		/* this may fail because omrthreads detach themselves upon exiting */

#if defined(OSX)
		/* OSX TODO: Why do the tests segfault in child thread when accessing &data on OSX without a 1ms sleep? */
		omrthread_sleep(10);
#endif /* defined(OSX) */
		PTHREAD_VERBOSE(pthread_join(tid, NULL));
		status |= data.status;

#elif defined(SPEC_WIN_API)
		WaitForSingleObject(tid, INFINITE);
		status |= data.status;
#else
		/* In this case,
		 * threadmain() doesn't access the expected data, so it's ok to continue without
		 * waiting for the child thread to complete.
		 */
#endif /* defined(SPEC_PTHREAD_API) */
	} else {
		status |= CREATE_FAILED;
	}
	return status;
}

static uintptr_t
isAttrOk(const create_attr_t *expected, const omrthread_attr_t actual)
{
	uintptr_t status = 0;

	/* expected should never be null */
	if (expected == NULL) {
		return BAD_TEST;
	}

	if (actual == NULL) {
		status |= NULL_ATTR;
		/* no further checks are possible */
		PRINTF("attr is NULL\n");
		return status;
	}

	/* both should refer to the same memory */
	if (expected->name != actual->name) {
		PRINTF("wrong name\n");
		status |= WRONG_NAME;
	}

	if (expected->stacksize != actual->stacksize) {
		printMismatchU("stacksize", expected->stacksize, actual->stacksize);
		status |= WRONG_STACKSIZE;
	}

	if (expected->policy != actual->schedpolicy) {
		printMismatchI("schedpolicy", expected->policy, actual->schedpolicy);
		status |= WRONG_SCHEDPOLICY;
	}

	if (expected->priority != actual->priority) {
		printMismatchU("priority", expected->priority, actual->priority);
		status |= WRONG_PRIORITY;
	}

#if defined(SPEC_PTHREAD_API)
	{
		pthread_attr_t *attr = &((unixthread_attr_t)actual)->pattr;
		size_t osStacksize;
#ifndef J9ZOS390
		struct sched_param osSchedparam;
		int osPolicy;
		int osInheritsched;
#endif
#if defined(AIXPPC) || defined(LINUX) || defined(OSX)
		int osScope;
#endif /* defined(AIXPPC) || defined(LINUX) || defined(OSX) */
#ifdef J9ZOS390
		int osThreadweight;
		int osDetachstate;
#endif

		PTHREAD_VERBOSE(pthread_attr_getstacksize(attr, &osStacksize));
		if (osStacksize != expected->osStacksize) {
			printMismatchI("os stacksize", expected->osStacksize, osStacksize);
			status |= WRONG_OS_STACKSIZE;
		}

#ifndef J9ZOS390

		PTHREAD_VERBOSE(pthread_attr_getschedpolicy(attr, &osPolicy));
#if (defined(AIXPPC) || defined(LINUX) || defined(OSX))
		/* aix and linux bug - need to set the schedpolicy to OTHER if inheritsched used */
		if (J9THREAD_SCHEDPOLICY_INHERIT == expected->policy) {
			if (osPolicy != OS_SCHED_OTHER) {
				printMismatchS("os schedpolicy", mapOSPolicy(OS_SCHED_OTHER), mapOSPolicy(osPolicy));
				status |= WRONG_OS_SCHEDPOLICY;
			}
		}
		else
#endif /* (defined(AIXPPC) || defined(LINUX) || defined(OSX)) */
		{
			if (osPolicy != expected->osPolicy) {
				printMismatchS("os schedpolicy", mapOSPolicy(expected->osPolicy), mapOSPolicy(osPolicy));
				status |= WRONG_OS_SCHEDPOLICY;
			}
		}

		PTHREAD_VERBOSE(pthread_attr_getinheritsched(attr, &osInheritsched));
		if (osInheritsched != expected->osInheritsched) {
			printMismatchS("os inheritsched", mapOSInherit(expected->osInheritsched), mapOSInherit(osInheritsched));
			status |= WRONG_OS_INHERITSCHED;
		}

		PTHREAD_VERBOSE(pthread_attr_getschedparam(attr, &osSchedparam));
		if (osSchedparam.sched_priority != expected->osPriority) {
			printMismatchI("os priority", expected->osPriority, osSchedparam.sched_priority);
			status |= WRONG_OS_PRIORITY;
		}
#endif /* not J9ZOS390 */

#if defined(AIXPPC) || defined(LINUX) || defined(OSX)
		PTHREAD_VERBOSE(pthread_attr_getscope(attr, &osScope));
		if (osScope != expected->osScope) {
			printMismatchI("os scope", expected->osScope, osScope);
			status |= WRONG_OS_SCOPE;
		}
#endif /* defined(AIXPPC) || defined(LINUX) || defined(OSX) */

#ifdef J9ZOS390
		osThreadweight = pthread_attr_getweight_np(attr);
		if (osThreadweight != expected->osThreadweight) {
			printMismatchI("os threadweight", expected->osThreadweight, osThreadweight);
			status |= WRONG_OS_THREADWEIGHT;
		}

		osDetachstate = pthread_attr_getdetachstate(attr);
		if (osDetachstate != expected->osDetachstate) {
			printMismatchI("os detachstate", expected->osDetachstate, osDetachstate);
			status |= WRONG_OS_DETACHSTATE;
		}
#endif
	}
#endif /* defined(SPEC_PTHREAD_API) */

	if (status == 0) {
		status |= canCreateThread(expected, actual);
	}
	return status;
}

static void
getCurrentOsSched(int *priority, int *policy)
{
#if defined(SPEC_PTHREAD_API) && !defined(J9ZOS390)
	OSTHREAD tid = pthread_self();
	struct sched_param param;

	PTHREAD_VERBOSE(pthread_getschedparam(tid, policy, &param));
	*priority = param.sched_priority;

#elif defined(SPEC_WIN_API)
	OSTHREAD tid = GetCurrentThread();
	*priority = GetThreadPriority(tid);
	*policy = -1;

#else
	*priority = -1;
	*policy = -1;
#endif /* defined(SPEC_PTHREAD_API) */
}

static int
getOsPolicy(omrthread_schedpolicy_t policy, omrthread_prio_t priority)
{
	int ospolicy = -1;

#if defined(SPEC_PTHREAD_API)
	switch (policy) {
	default:
	case J9THREAD_SCHEDPOLICY_INHERIT:
		ospolicy = -1;
		break;
	case J9THREAD_SCHEDPOLICY_OTHER:
		ospolicy = SCHED_OTHER;
		break;
	case J9THREAD_SCHEDPOLICY_FIFO:
		ospolicy = SCHED_FIFO;
		break;
	case J9THREAD_SCHEDPOLICY_RR:
		ospolicy = SCHED_RR;
		break;
	}
#endif /* defined(SPEC_PTHREAD_API) */

#if defined(LINUX) || defined(OSX)
	/* overwrite the ospolicy on LINUX if we're using realtime scheduling */
	if (omrthread_lib_use_realtime_scheduling()) {
		ospolicy = getRTPolicy(priority);
	}
#endif /* defined(LINUX) || defined(OSX) */

	return ospolicy;
}

static size_t
getOsStacksize(size_t stacksize)
{
	if (stacksize == 0) {
		stacksize = STACK_DEFAULT_SIZE;
	}

#if defined(LINUX) || defined(OSX)
	/* Linux allocates 2MB if you ask for a stack smaller than STACK_MIN */
	{
		size_t pageSafeMinimumStack = 2 * sysconf(_SC_PAGESIZE);

		if (pageSafeMinimumStack < PTHREAD_STACK_MIN) {
			pageSafeMinimumStack = PTHREAD_STACK_MIN;
		}
		if (stacksize < pageSafeMinimumStack) {
			stacksize = pageSafeMinimumStack;
		}
	}
#endif /* defined(LINUX) || defined(OSX) */

	return stacksize;
}

static void
initDefaultExpected(create_attr_t *expected)
{
	expected->name = NULL;
	expected->stacksize = STACK_DEFAULT_SIZE;
	expected->priority = J9THREAD_PRIORITY_NORMAL;
	expected->policy = J9THREAD_SCHEDPOLICY_INHERIT;

	expected->osStacksize = getOsStacksize((size_t)expected->stacksize);
	expected->osPriority = getOsPriority(expected->priority);
	expected->osPolicy = getOsPolicy(expected->policy, expected->priority);
	if (expected->osPolicy == -1) {
		expected->osPolicy = OS_SCHED_OTHER;
	}

	if (omrthread_lib_use_realtime_scheduling()) {
		expected->osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
	} else {
		expected->osInheritsched = OS_PTHREAD_INHERIT_SCHED;
	}

	expected->osScope = OS_PTHREAD_SCOPE_SYSTEM;
	expected->osThreadweight = OS_MEDIUM_WEIGHT;
	expected->osDetachstate = 1;
}

TEST_F(ThreadCreateTest, SetAttrNull)
{
	uintptr_t status = 0;
	create_attr_t expected;

	initDefaultExpected(&expected);
	status |= canCreateThread(&expected, NULL);
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrDefault)
{
	uintptr_t status = 0;
	uintptr_t rc = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);

	rc = J9THREAD_VERBOSE(omrthread_attr_init(&attr));
	if (rc != J9THREAD_SUCCESS) {
		status |= INIT_FAILED;
	}
	status |= isAttrOk(&expected, attr);

	rc = J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	if (rc != J9THREAD_SUCCESS) {
		status |= DESTROY_FAILED;
	}

	if (attr != NULL) {
		PRINTF("destroy didn't NULL attr\n");
		status |= DESTROY_FAILED;
	}

	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrDestroynull)
{
	uintptr_t status = 0;
	uintptr_t rc = 0;

	rc = J9THREAD_VERBOSE(omrthread_attr_destroy(NULL));
	if (rc != J9THREAD_ERR_INVALID_ATTR) {
		status |= DESTROY_FAILED;
	}

	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrBadDestroy)
{
	uintptr_t status = 0;
	uintptr_t rc = 0;
	uintptr_t *ptr = NULL;
	omrthread_attr_t attr = NULL;

	ptr = (uintptr_t *)malloc(sizeof(uintptr_t));
	ASSERT(ptr);
	*ptr = 3; /* invalid size */
	attr = (omrthread_attr_t)ptr;

	rc = J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	if (rc != J9THREAD_ERR_INVALID_ATTR) {
		status |= DESTROY_FAILED;
	}
	if ((uintptr_t *)attr != ptr) {
		PRINTF("destroy modified ptr\n");
		status |= DESTROY_MODIFIED_PTR;
	}
	free(ptr);
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrName)
{
	uintptr_t status = 0;
	uintptr_t rc = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;
	const char *testname = "TheTestThread";
	const char *testname2 = "another name";

	initDefaultExpected(&expected);

	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	expected.name = testname;
	rc = J9THREAD_VERBOSE(omrthread_attr_set_name(&attr, testname));
	if (INVALID_VALUE(rc)) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	expected.name = testname2;
	rc = J9THREAD_VERBOSE(omrthread_attr_set_name(&attr, testname2));
	if (INVALID_VALUE(rc)) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	expected.name = NULL;
	rc = J9THREAD_VERBOSE(omrthread_attr_set_name(&attr, NULL));
	if (INVALID_VALUE(rc)) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrStacksize)
{
	uintptr_t status = 0;
	uintptr_t rc = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);
	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	rc = J9THREAD_VERBOSE(omrthread_attr_set_stacksize(&attr, STACK_DEFAULT_SIZE));
	if (rc != J9THREAD_SUCCESS) {
		status |= EXPECTED_VALID;
		PRINTF("failed to set STACK_DEFAULT_SIZE %x (%d)\n", STACK_DEFAULT_SIZE, STACK_DEFAULT_SIZE);
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_stacksize(&attr, 0x6000)))) {
		expected.stacksize = 0x6000;
		expected.osStacksize = getOsStacksize(expected.stacksize);
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_stacksize(&attr, 0x7000)))) {
		expected.stacksize = 0x7000;
		expected.osStacksize = getOsStacksize(expected.stacksize);
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_stacksize(&attr, 0x8000)))) {
		expected.stacksize = 0x8000;
		expected.osStacksize = getOsStacksize(expected.stacksize);
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	rc = J9THREAD_VERBOSE(omrthread_attr_set_stacksize(&attr, 0));
	if (rc != J9THREAD_SUCCESS) {
		status |= EXPECTED_VALID;
		PRINTF("failed to set 0 (default)\n");
	}
	expected.stacksize = STACK_DEFAULT_SIZE;
	expected.osStacksize = getOsStacksize(STACK_DEFAULT_SIZE);
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrPolicy)
{
	uintptr_t status = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);
	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	/*
	 * On Linux non-RT, RR and FIFO will fail thread creation because the
	 * thread priority isn't set properly. Our VM->OS mapping converts all priorities to 0
	 * because some versions of Linux support only priority 0 for SCHED_OTHER.
	 * TODO We need a different priority mapping for other scheduling policies.
	 */

	/*
	 * On AIX, one needs root permissions to create threads with RR or FIFO.
	 */

	expected.policy = J9THREAD_SCHEDPOLICY_RR;
	if (!omrthread_lib_use_realtime_scheduling()) {
		expected.osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
		expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	}
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_RR)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
#if (defined(LINUX) || defined(AIXPPC))
	PRINTF("  ignoring omrthread_create failure\n");
	status &= ~CREATE_FAILED;
#endif /* (defined(LINUX) || defined(AIXPPC)) */
	END_IF_FAILED(status);

	expected.policy = J9THREAD_SCHEDPOLICY_FIFO;
	if (!omrthread_lib_use_realtime_scheduling()) {
		expected.osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
		expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	}
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_FIFO)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
#if (defined(LINUX) || defined(AIXPPC))
	PRINTF("  ignoring omrthread_create failure\n");
	status &= ~CREATE_FAILED;
#endif /* (defined(LINUX) || defined(AIXPPC)) */
	END_IF_FAILED(status);

	expected.policy = J9THREAD_SCHEDPOLICY_INHERIT;
	if (!omrthread_lib_use_realtime_scheduling()) {
		expected.osInheritsched = OS_PTHREAD_INHERIT_SCHED;
	}
	/* expected.osPolicy unchanged */
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_INHERIT)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	expected.policy = J9THREAD_SCHEDPOLICY_OTHER;
	if (!omrthread_lib_use_realtime_scheduling()) {
		expected.osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
		expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	}
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrBadPolicy)
{
	uintptr_t status = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);
	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	/* invalid value */
	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, omrthread_schedpolicy_LastEnum)))) {
		status |= EXPECTED_INVALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrPriority)
{
	uintptr_t status = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);
	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	/* set the schedpolicy to explicit so that priorities are used by thread creation */
	expected.policy = J9THREAD_SCHEDPOLICY_OTHER;
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	expected.osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
	J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER));
	END_IF_FAILED(status);

	expected.priority = 5;
	expected.osPriority = getOsPriority(expected.priority);
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 5)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	expected.priority = 11;
	expected.osPriority = getOsPriority(expected.priority);
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 11)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	expected.priority = 2;
	expected.osPriority = getOsPriority(expected.priority);
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 2)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrBadPriority)
{
	uintptr_t status = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);

	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	/* set the schedpolicy to explicit so that priorities are used by thread creation */
	expected.policy = J9THREAD_SCHEDPOLICY_OTHER;
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	expected.osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
	J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER));

	expected.priority = 4;
	expected.osPriority = getOsPriority(expected.priority);
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 4)))) {
		status |= EXPECTED_VALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, -1)))) {
		status |= EXPECTED_INVALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 90)))) {
		status |= EXPECTED_INVALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (!INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 12)))) {
		status |= EXPECTED_INVALID;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

TEST_F(ThreadCreateTest, SetAttrInheritSched)
{
	uintptr_t status = 0;
	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);
	J9THREAD_VERBOSE(omrthread_attr_init(&attr));

	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_INHERIT)))) {
		status |= EXPECTED_VALID;
	}
	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_priority(&attr, 6)))) {
		status |= EXPECTED_VALID;
	}
	expected.policy = J9THREAD_SCHEDPOLICY_INHERIT;
	expected.priority = 6;
	expected.osPriority = getOsPriority(expected.priority);
	/* expected.osPolicy is unchanged */
	if (!omrthread_lib_use_realtime_scheduling()) {
		expected.osInheritsched = OS_PTHREAD_INHERIT_SCHED;
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

	if (INVALID_VALUE(J9THREAD_VERBOSE(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER)))) {
		status |= EXPECTED_VALID;
	}
	expected.policy = J9THREAD_SCHEDPOLICY_OTHER;
	expected.osPolicy = getOsPolicy(expected.policy, expected.priority);
	expected.osInheritsched = OS_PTHREAD_EXPLICIT_SCHED;
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

static int numaSetAffinityThreadMain(void *arg)
{
	uintptr_t status = 0;
	intptr_t result = 0;
	uintptr_t maxNumaNode = omrthread_numa_get_max_node();
	uintptr_t numaNode = 0;
	uintptr_t expectedNodeForNoAffinity = 0;
	numadata_t *data = (numadata_t *)arg;
	omrthread_monitor_t monitor = data->monitor;
	omrthread_monitor_enter(monitor);
	if (maxNumaNode > 0) {
		/* Check that our current affinity is 0 */
		uintptr_t currentAffinity = 0;
		uintptr_t nodeCount = 1;
		if (0 != J9THREAD_VERBOSE(omrthread_numa_get_node_affinity(omrthread_self(), &currentAffinity, &nodeCount))) {
			PRINTF("Failed to get thread affinity\n");
			status |= EXPECTED_VALID;
			goto endthread;
		}

		/* We used to check that a child thread inherits its parent's affinity. This is Linux behaviour, but not
		 * AIX or Windows behaviour. We don't rely on, specify or document this behaviour, so don't test it.
		 */
#if 0
		if (currentAffinity != data->expectedAffinity) {
			PRINTF("Thread's initial affinity is different from parent's. Expected:%zu Actual:%zu\n", data->expectedAffinity, currentAffinity);
			status |= EXPECTED_VALID;
			goto endthread;
		}
#endif

		expectedNodeForNoAffinity = currentAffinity;

		/* Set the affinity to the highest node which has CPUs associated to it */
		numaNode = maxNumaNode;
		while (numaNode > 0) {
			PRINTF("Setting thread numa affinity to %zu\n", numaNode);
			result = omrthread_numa_set_node_affinity(omrthread_self(), &numaNode, 1, 0);

			if (result == J9THREAD_NUMA_ERR_NO_CPUS_FOR_NODE) {
				PRINTF("Tried to set thread numa affinity to node %zu, but no CPUs associated with node\n", numaNode);
				numaNode--;
				continue;
			} else if (result != 0) {
				PRINTF("Failed to set affinity to %zu\n", numaNode);
				status |= EXPECTED_VALID;
				goto endthread;
			} else {
				break;
			}
		}

		/* Verify that it was correctly set */
		nodeCount = 1;
		if (0 != J9THREAD_VERBOSE(omrthread_numa_get_node_affinity(omrthread_self(), &currentAffinity, &nodeCount))) {
			PRINTF("Failed to get thread affinity\n");
			status |= EXPECTED_VALID;
			goto endthread;
		}

		if (numaNode != currentAffinity) {
			PRINTF("Affinity is incorrectly set. Expected:%zu Actual:%zu\n", numaNode, currentAffinity);
			status |= EXPECTED_VALID;
			goto endthread;
		}

		/* Now unset the affinity */
		currentAffinity = 0;
		if (0 != J9THREAD_VERBOSE(omrthread_numa_set_node_affinity(omrthread_self(), &currentAffinity, 1, 0))) {
			PRINTF("Failed to clear affinity\n");
			status |= EXPECTED_VALID;
			goto endthread;
		}

		/* Verify that it was correctly cleared */
		nodeCount = 1;
		if (0 != J9THREAD_VERBOSE(omrthread_numa_get_node_affinity(omrthread_self(), &currentAffinity, &nodeCount))) {
			PRINTF("Failed to get thread affinity\n");
			status |= EXPECTED_VALID;
			goto endthread;
		}

		if (expectedNodeForNoAffinity != currentAffinity) {
			PRINTF("Affinity is incorrectly set. Expected:%zu Actual:%zu\n", expectedNodeForNoAffinity, currentAffinity);
			status |= EXPECTED_VALID;
			goto endthread;
		}

	} else {
		PRINTF("Doesn't look like NUMA is available on this system\n");
	}
endthread:
	data->status = status;
	omrthread_monitor_notify_all(monitor);
	omrthread_monitor_exit(monitor);
	return 0;
}

/* Since we can't read affinity on Win32, at least make sure that we can call the setAffinity API without crashing.
 * Note that this should be safe on all platforms (either succeeds or fails but we ignore the return code since
 * any answer is correct, so long as we don't crash - without reading the affinity, we don't know of any more
 * interesting tests).
 */
TEST_F(ThreadCreateTest, NumaSetAffinityBlindly)
{
#define NODE_LIST_SIZE 4
	uintptr_t nodeList[NODE_LIST_SIZE];
	uintptr_t i = 0;

	for (i = 0; i < NODE_LIST_SIZE; i++) {
		nodeList[i] = i + 1;
	}
	omrthread_numa_set_node_affinity(omrthread_self(), nodeList, NODE_LIST_SIZE, 0);
}

TEST_F(ThreadCreateTest, NumaSetAffinity)
{
	uintptr_t status = 0;
	omrthread_t thread;
	omrthread_monitor_t monitor;
	numadata_t data;
	uintptr_t nodeCount = 0;
	intptr_t affinityResultCode = 0;

	if (0 != J9THREAD_VERBOSE(omrthread_monitor_init(&monitor, 0))) {
		PRINTF("Failed to initialize monitor\n");
		status |= NULL_ATTR;
		goto endtest;
	}

	data.monitor = monitor;
	data.status = 0;
	data.expectedAffinity = 0;

	omrthread_monitor_enter(monitor);

	nodeCount = 1;
	affinityResultCode = omrthread_numa_get_node_affinity(omrthread_self(), &data.expectedAffinity, &nodeCount);
	if (J9THREAD_NUMA_ERR_AFFINITY_NOT_SUPPORTED == affinityResultCode) {
		/* this platform can't meaningfully run this test so just end */
		PRINTF("NUMA-level thread affinity not supported on this platform\n");
		goto endtest;
	}
	if (J9THREAD_NUMA_OK != affinityResultCode) {
		PRINTF("Failed to get parent thread's affinity\n");
		status |= CREATE_FAILED;
		goto endtest;
	}

	if (J9THREAD_SUCCESS != J9THREAD_VERBOSE(omrthread_create_ex(&thread, J9THREAD_ATTR_DEFAULT, 0, numaSetAffinityThreadMain, &data))) {
		PRINTF("Failed to create the thread\n");
		status |= CREATE_FAILED;
		goto endtest;
	}

	if (0 != omrthread_monitor_wait(monitor)) {
		PRINTF("Failed to wait on monitor\n");
		status |= NULL_ATTR;
		goto endtest;
	}

	status |= data.status;
endtest:
	omrthread_monitor_exit(monitor);
	omrthread_monitor_destroy(monitor);
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

static int
numaSetAffinitySuspendedThreadMain(void *arg)
{
	numadata_t *data = (numadata_t *)arg;
	omrthread_monitor_t monitor = data->monitor;

	uintptr_t status = 0;
	uintptr_t currentAffinity = 0;
	uintptr_t nodeCount = 1;

	J9THREAD_VERBOSE(omrthread_monitor_enter(monitor));
	if (0 != J9THREAD_VERBOSE(omrthread_numa_get_node_affinity(omrthread_self(), &currentAffinity, &nodeCount))) {
		status |= EXPECTED_VALID;
		goto endthread;
	}

	if (currentAffinity != data->expectedAffinity) {
		PRINTF("Affinity different from expected. Expected:%zu Actual:%zu\n", data->expectedAffinity, currentAffinity);
		status |= EXPECTED_VALID;
		goto endthread;
	}

endthread:
	data->status = status;
	J9THREAD_VERBOSE(omrthread_monitor_notify_all(monitor));
	J9THREAD_VERBOSE(omrthread_monitor_exit(monitor));
	return 0;
}

TEST_F(ThreadCreateTest, NumaSetAffinitySuspended)
{
	omrthread_t thread;
	omrthread_monitor_t monitor;
	numadata_t data;

	intptr_t result = 0;
	uintptr_t status = 0;
	uintptr_t numaMaxNode = omrthread_numa_get_max_node();
	uintptr_t numaNode = 0;
	uintptr_t expectedAffinityBeforeStart = 0;
	omrthread_monitor_init(&monitor, 0);

	if (numaMaxNode > 0) {
		uintptr_t nodeCount = 1;
		/* first, see if we can even run this test */
		intptr_t affinityResultCode = omrthread_numa_get_node_affinity(omrthread_self(), &data.expectedAffinity, &nodeCount);
		data.monitor = monitor;
		data.status = 0;

		if (J9THREAD_NUMA_ERR_AFFINITY_NOT_SUPPORTED == affinityResultCode) {
			/* this platform can't meaningfully run this test so just end */
			PRINTF("NUMA-level thread affinity not supported on this platform\n");
			goto endtest;
		}
		/* Create the thread suspended */
		if (J9THREAD_SUCCESS != J9THREAD_VERBOSE(omrthread_create_ex(&thread, J9THREAD_ATTR_DEFAULT, 1, numaSetAffinitySuspendedThreadMain, &data))) {
			status |= CREATE_FAILED;
			goto endtest;
		}

		/* Set the affinity to the highest node which has CPUs associated to it */
		numaNode = numaMaxNode;
		while (numaNode > 0) {
			PRINTF("Setting thread numa affinity to %zu\n", numaNode);
			result = omrthread_numa_set_node_affinity(thread, &numaNode, 1, 0);
			if (result == J9THREAD_NUMA_ERR_NO_CPUS_FOR_NODE) {
				PRINTF("Tried to set thread numa affinity to node %zu, but no CPUs associated with node\n", numaNode);
				numaNode--;
				continue;
			} else if (result != 0) {
				PRINTF("Failed to set affinity to %zu\n", numaNode);
				status |= EXPECTED_VALID;
				goto endtest;
			} else {
				data.expectedAffinity = numaNode;
				break;
			}
		}

		/* Check that the affinity on the suspended thread is indeed what we set it to */
		if (0 != J9THREAD_VERBOSE(omrthread_numa_get_node_affinity(thread, &expectedAffinityBeforeStart, &nodeCount))) {
			PRINTF("Failed to get affinity on the thread while it's still suspended\n");
			status |= EXPECTED_VALID;
			goto endtest;
		}

		if (expectedAffinityBeforeStart != data.expectedAffinity) {
			PRINTF("Suspended thread's deferred affinity is not what it should be. Expected:%zu Actual:%zu\n", data.expectedAffinity, expectedAffinityBeforeStart);
			status |= EXPECTED_VALID;
			goto endtest;
		}

		J9THREAD_VERBOSE(omrthread_monitor_enter(monitor));
		if (1 != omrthread_resume(thread)) {
			PRINTF("Failed to resume the thread\n");
			goto endtest;
		}

		if (0 != J9THREAD_VERBOSE(omrthread_monitor_wait(monitor))) {
			status |= NULL_ATTR;
			goto endtest;
		}

		status |= data.status;
	} else {
		PRINTF("Doesn't look like NUMA is available on this system\n");
	}

endtest:
	omrthread_monitor_exit(monitor);
	omrthread_monitor_destroy(monitor);
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}

/*
 * Using DISABLED_ to run this test separately, because the thread weight can only
 * be set once. Later changes to omrthread_global("thread_weight") are ignored.
 */
TEST_F(ThreadCreateTest, DISABLED_SetAttrThreadWeight)
{
	uintptr_t status = 0;
	const char **weight;

	omrthread_attr_t attr = NULL;
	create_attr_t expected;

	initDefaultExpected(&expected);

	weight = (const char **)omrthread_global((char *)"thread_weight");

	*weight = "heavy";
	expected.osThreadweight = OS_HEAVY_WEIGHT;
	if (J9THREAD_VERBOSE(omrthread_attr_init(&attr)) != J9THREAD_SUCCESS) {
		status |= INIT_FAILED;
		PRINTF("failed to init heavyweight thread\n");
	}
	status |= isAttrOk(&expected, attr);
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	END_IF_FAILED(status);

	*weight = "medium";
	expected.osThreadweight = OS_HEAVY_WEIGHT;
	if (J9THREAD_VERBOSE(omrthread_attr_init(&attr)) != J9THREAD_SUCCESS) {
		status |= INIT_FAILED;
		PRINTF("failed to init mediumweight thread\n");
	}
	status |= isAttrOk(&expected, attr);
	END_IF_FAILED(status);

endtest:
	J9THREAD_VERBOSE(omrthread_attr_destroy(&attr));
	ASSERT_EQ((uintptr_t)0, status) << "Failed with Code: " << std::hex << status;
}
