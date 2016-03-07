/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2008, 2016
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

#include <float.h>

#include "omrport.h"
#include "omrTest.h"
#include "testHelper.hpp"
#include "thread_api.h"

#define MILLI_TIMEOUT	10000
#define NANO_TIMEOUT	0

#define STEP_MILLI_TIMEOUT		600000
#define STEP_NANO_TIMEOUT		0

extern ThreadTestEnvironment *omrTestEnv;

/* structure used to pass info to concurrent threads for some tests */
typedef struct SupportThreadInfo {
	volatile omrthread_rwmutex_t handle;
	omrthread_monitor_t synchronization;
	omrthread_entrypoint_t *functionsToRun;
	uintptr_t numberFunctions;
	volatile uintptr_t readCounter;
	volatile uintptr_t writeCounter;
	volatile BOOLEAN done;
} SupportThreadInfo;

/* forward declarations */
void freeSupportThreadInfo(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC runRequest(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC enter_rwmutex_read(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC exit_rwmutex_read(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC enter_rwmutex_write(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC try_enter_rwmutex_write(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC exit_rwmutex_write(SupportThreadInfo *info);
static intptr_t J9THREAD_PROC nop(SupportThreadInfo *info);

/**
 * This method is called to run the set of steps that will be run on a thread
 * @param info SupportThreadInfo structure that contains the information for the steps and other parameters
 *        that are used
 */
static intptr_t J9THREAD_PROC
runRequest(SupportThreadInfo *info)
{
	intptr_t result = 0;
	uintptr_t i = 0;

	omrthread_monitor_enter(info->synchronization);
	omrthread_monitor_exit(info->synchronization);
	for (i = 0; i < info->numberFunctions; i++) {
		result = info->functionsToRun[i]((void *)info);
		omrthread_monitor_enter(info->synchronization);
		omrthread_monitor_notify(info->synchronization);
		if (info->done == TRUE) {
			omrthread_monitor_exit(info->synchronization);
			break;
		}
		omrthread_monitor_wait_interruptable(info->synchronization,
											STEP_MILLI_TIMEOUT, STEP_NANO_TIMEOUT);
		omrthread_monitor_exit(info->synchronization);
	}

	return result;
}

/**
 * This method is called to create a SupportThreadInfo for a test.  It will populate the monitor and
 * rwmutex being used and zero out the counter
 *
 * @param functionsToRun an array of functions pointers. Each function will be run one in sequence synchronized
 *        using the monitor within the SupporThreadInfo
 * @param numberFunctions the number of functions in the functionsToRun array
 * @returns a pointer to the newly created SupporThreadInfo
 */
SupportThreadInfo *
createSupportThreadInfo(omrthread_entrypoint_t *functionsToRun, uintptr_t numberFunctions)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	SupportThreadInfo *info = (SupportThreadInfo *)omrmem_allocate_memory(sizeof(SupportThreadInfo), OMRMEM_CATEGORY_THREADS);
	info->readCounter = 0;
	info->writeCounter = 0;
	info->functionsToRun = functionsToRun;
	info->numberFunctions = numberFunctions;
	info->done = FALSE;
	omrthread_rwmutex_init((omrthread_rwmutex_t *)&info->handle, 0, "supportThreadInfo rwmutex");
	omrthread_monitor_init_with_name(&info->synchronization, 0, "supportThreadAInfo monitor");
	return info;
}

/**
 * This method free the internal structures and memory for a SupportThreadInfo
 * @param info the SupportThreadInfo instance to be freed
 */
void
freeSupportThreadInfo(SupportThreadInfo *info)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	if (info->synchronization != NULL) {
		omrthread_monitor_destroy(info->synchronization);
	}
	if (info->handle != NULL) {
		omrthread_rwmutex_destroy(info->handle);
	}
	omrmem_free_memory(info);
}

/**
 * This method is called to push the concurrent thread to run the next function
 */
void
triggerNextStepWithStatus(SupportThreadInfo *info, BOOLEAN done)
{
	omrthread_monitor_enter(info->synchronization);
	info->done = done;
	omrthread_monitor_notify(info->synchronization);
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
}

/**
 * This method is called to push the concurrent thread to run the next function
 */
void
triggerNextStep(SupportThreadInfo *info)
{
	triggerNextStepWithStatus(info, FALSE);
}

/**
 * This method is called to push the concurrent thread to run the next function
 * and tell the thread that the test is done
 */
void
triggerNextStepDone(SupportThreadInfo *info)
{
	triggerNextStepWithStatus(info, TRUE);
}

/**
 * This method starts a concurrent thread and runs the functions specified in the
 * SupportThreadInfo passed in.  It only returns once the first function has run
 *
 * @param info SupporThreadInfo structure containing the functions and rwmutex for the tests
 * @returns 0 on success
 */
intptr_t
startConcurrentThread(SupportThreadInfo *info)
{
	omrthread_t newThread = NULL;

	omrthread_monitor_enter(info->synchronization);
	omrthread_create_ex(
		&newThread,
		J9THREAD_ATTR_DEFAULT, /* default attr */
		0, /* start immediately */
		(omrthread_entrypoint_t) runRequest,
		(void *)info);

	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);

	return 0;
}

/***********************************************
 * Functions that can be used as steps in concurrent threads
 * for a test
 ************************************************/
/**
 * This step enters the rwmutex in the SupporThreadInfo for read
 * @param info the SupporThreadInfo which can be used by the step
 */
static intptr_t J9THREAD_PROC
enter_rwmutex_read(SupportThreadInfo *info)
{
	omrthread_rwmutex_enter_read(info->handle);
	info->readCounter++;
	return 0;
}

/**
 * This step exits the rwmutex in the SupporThreadInfo for read
 * @param info the SupporThreadInfo which can be used by the step
 */
static intptr_t J9THREAD_PROC
exit_rwmutex_read(SupportThreadInfo *info)
{
	omrthread_rwmutex_exit_read(info->handle);
	info->readCounter--;
	return 0;
}

/**
 * This step enters the rwmutex in the SupporThreadInfo for write
 * @param info the SupporThreadInfo which can be used by the step
 */
static intptr_t J9THREAD_PROC
enter_rwmutex_write(SupportThreadInfo *info)
{
	omrthread_rwmutex_enter_write(info->handle);
	info->writeCounter++;
	return 0;
}

/**
 * This step enters the rwmutex in the SupporThreadInfo for write
 * @param info the SupporThreadInfo which can be used by the step
 */
static intptr_t J9THREAD_PROC
try_enter_rwmutex_write(SupportThreadInfo *info)
{
	if (omrthread_rwmutex_try_enter_write(info->handle) == 0) {
		info->writeCounter++;
	}
	return 0;
}

/**
 * This step exits the rwmutex in the SupporThreadInfo for write
 * @param info the SupporThreadInfo which can be used by the step
 */
static intptr_t
J9THREAD_PROC exit_rwmutex_write(SupportThreadInfo *info)
{
	omrthread_rwmutex_exit_write(info->handle);
	info->writeCounter--;
	return 0;
}

/**
 * This step does nothing
 * @param info the SupporThreadInfo which can be used by the step
 */
static intptr_t
J9THREAD_PROC nop(SupportThreadInfo *info)
{
	return 0;
}

/**
 * validate that we can create a reate/write mutex successfully
 */
TEST(RWMutex, CreateTest)
{
	intptr_t result;
	omrthread_rwmutex_t handle;
	uintptr_t flags = 0;
	const char *mutexName = "test_mutex";

	result = omrthread_rwmutex_init(&handle, flags, mutexName);
	ASSERT_TRUE(0 == result);

	/* clean up */
	result = omrthread_rwmutex_destroy(handle);
	ASSERT_TRUE(0 == result);
}

/**
 * Validate that we can enter/exit a RWMutex for read
 */
TEST(RWMutex, RWReadEnterExitTest)
{
	intptr_t result;
	omrthread_rwmutex_t handle;
	uintptr_t flags = 0;
	const char *mutexName = "test_mutex";

	result = omrthread_rwmutex_init(&handle, flags, mutexName);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_enter_read(handle);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_exit_read(handle);
	ASSERT_TRUE(0 == result);

	/* clean up */
	result = omrthread_rwmutex_destroy(handle);
	ASSERT_TRUE(0 == result);
}

/**
 * Validate that we can enter/exit a RWMutex for write
 */
TEST(RWMutex, RWWriteEnterExitTest)
{
	intptr_t result;
	omrthread_rwmutex_t handle;
	uintptr_t flags = 0;
	const char *mutexName = "test_mutex";

	result = omrthread_rwmutex_init(&handle, flags, mutexName);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_enter_write(handle);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_exit_write(handle);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_try_enter_write(handle);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_exit_write(handle);
	ASSERT_TRUE(0 == result);

	/* clean up */
	result = omrthread_rwmutex_destroy(handle);
	ASSERT_TRUE(0 == result);
}

/**
 * Validate that is_writelocked return true in writing state
 */
TEST(RWMutex, IsWriteLockedTest)
{
	intptr_t result;
	omrthread_rwmutex_t handle;
	uintptr_t flags = 0;
	BOOLEAN ret;
	const char *mutexName = "test_mutex";

	result = omrthread_rwmutex_init(&handle, flags, mutexName);
	ASSERT_TRUE(0 == result);

	ret = omrthread_rwmutex_is_writelocked(handle);
	ASSERT_TRUE(FALSE == ret);

	result = omrthread_rwmutex_enter_read(handle);
	ASSERT_TRUE(0 == result);

	ret = omrthread_rwmutex_is_writelocked(handle);
	ASSERT_TRUE(FALSE == ret);

	result = omrthread_rwmutex_exit_read(handle);
	ASSERT_TRUE(0 == result);

	result = omrthread_rwmutex_enter_write(handle);
	ASSERT_TRUE(0 == result);

	ret = omrthread_rwmutex_is_writelocked(handle);
	ASSERT_TRUE(TRUE == ret);

	result = omrthread_rwmutex_exit_write(handle);
	ASSERT_TRUE(0 == result);

	/* clean up */
	result = omrthread_rwmutex_destroy(handle);
	ASSERT_TRUE(0 == result);
}

TEST(RWMutex, MultipleReadersTest)
{
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	info = createSupportThreadInfo(functionsToRun, 2);
	startConcurrentThread(info);

	/* now the concurrent thread should have acquired the rwmutex
	 * so validate that we can acquire it as well
	 */
	ASSERT_TRUE(1 == info->readCounter);
	omrthread_rwmutex_enter_read(info->handle);
	ASSERT_TRUE(1 == info->readCounter);
	omrthread_rwmutex_exit_read(info->handle);

	/* ok we were not blocked by the other thread holding the mutex for read
	 * so ask it to release the mutex
	 */
	triggerNextStepDone(info);
	ASSERT_TRUE(0 == info->readCounter);
	freeSupportThreadInfo(info);
}

/**
 * validates the following
 *
 * readers are excludes while another thread holds the rwmutex for write
 * once writer exits, reader can enter
 */
TEST(RWMutex, ReadersExcludedTest)
{
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	info = createSupportThreadInfo(functionsToRun, 2);

	/* first enter the mutex for write */
	ASSERT_TRUE(0 == info->readCounter);
	omrthread_rwmutex_enter_write(info->handle);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	ASSERT_TRUE(0 == info->readCounter);

	/* now release the rwmutex and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	omrthread_rwmutex_exit_write(info->handle);
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	ASSERT_TRUE(1 == info->readCounter);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	ASSERT_TRUE(0 == info->readCounter);
	freeSupportThreadInfo(info);
}

/**
 * validates the following
 *
 * readers are excludes while another thread holds the rwmutex for write entered using try_enter
 * once writer exits, reader can enter
 */
TEST(RWMutex, ReadersExcludedTesttryenter)
{
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	info = createSupportThreadInfo(functionsToRun, 2);

	/* first enter the mutex for write */
	ASSERT_TRUE(0 == info->readCounter);
	omrthread_rwmutex_try_enter_write(info->handle);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	ASSERT_TRUE(0 == info->readCounter);

	/* now release the rwmutex and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	omrthread_rwmutex_exit_write(info->handle);
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	ASSERT_TRUE(1 == info->readCounter);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	ASSERT_TRUE(0 == info->readCounter);
	freeSupportThreadInfo(info);
}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the rwmutex for read
 * once reader exits writer can enter
 */
TEST(RWMutex, WritersExcludedTest)
{
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	info = createSupportThreadInfo(functionsToRun, 2);

	/* first enter the mutex for read */
	ASSERT_TRUE(0 == info->writeCounter);
	omrthread_rwmutex_enter_read(info->handle);

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	ASSERT_TRUE(0 == info->writeCounter);

	/* now release the rwmutex and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	omrthread_rwmutex_exit_read(info->handle);
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	ASSERT_TRUE(1 == info->writeCounter);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	ASSERT_TRUE(0 == info->writeCounter);
	freeSupportThreadInfo(info);
}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the rwmutex for write
 * once writer exits second writer can enter
 */
TEST(RWMutex, WriterExcludesWriterTest)
{
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	info = createSupportThreadInfo(functionsToRun, 2);

	/* first enter the mutex for write */
	ASSERT_TRUE(0 == info->writeCounter);
	omrthread_rwmutex_enter_write(info->handle);

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	ASSERT_TRUE(0 == info->writeCounter);

	/* now release the rwmutex and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	omrthread_rwmutex_exit_write(info->handle);
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	ASSERT_TRUE(1 == info->writeCounter);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	freeSupportThreadInfo(info);
}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the rwmutex for write
 * once writer exits second writer can enter
 */
TEST(RWMutex, WriterExcludesWriterTesttryenter)
{
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	info = createSupportThreadInfo(functionsToRun, 2);

	/* first enter the mutex for write */
	ASSERT_TRUE(0 == info->writeCounter);
	omrthread_rwmutex_try_enter_write(info->handle);

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	ASSERT_TRUE(0 == info->writeCounter);

	/* now release the rwmutex and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	omrthread_rwmutex_exit_write(info->handle);
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	ASSERT_TRUE(1 == info->writeCounter);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	freeSupportThreadInfo(info);
}

/* validates the following
 *
 * reader can enter rwmutex even if write is pending (waiting on another reader)
 * 2nd reader to enter rwmutex continues to block write after first thread exits
 */
TEST(RWMutex, SecondReaderExcludesWrite)
{
	omrthread_rwmutex_t saveHandle;
	SupportThreadInfo *info;
	SupportThreadInfo *infoReader;
	omrthread_entrypoint_t functionsToRun[2];
	omrthread_entrypoint_t functionsToRunReader[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunReader[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;

	info = createSupportThreadInfo(functionsToRun, 2);
	infoReader = createSupportThreadInfo(functionsToRunReader, 2);

	/* set the two SupporThreadInfo structures so that they use the same rwmutex */
	saveHandle = infoReader->handle;
	infoReader->handle = info->handle;

	/* first enter the mutex for read */
	ASSERT_TRUE(0 == info->writeCounter);
	omrthread_rwmutex_enter_read(info->handle);

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	ASSERT_TRUE(0 == info->writeCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is not blocked
	 */
	startConcurrentThread(infoReader);
	ASSERT_TRUE(1 == infoReader->readCounter);

	/* now release the rwmutex and validate that the second readers still excludes the writer */
	omrthread_monitor_enter(info->synchronization);
	omrthread_rwmutex_exit_read(infoReader->handle);
	ASSERT_TRUE(0 == info->writeCounter);
	ASSERT_TRUE(1 == infoReader->readCounter);

	/* now ask the reader to exit the mutex */
	triggerNextStepDone(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now validate that the writer has now entered the mutex */
	omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	ASSERT_TRUE(1 == info->writeCounter);

	/* ok now let the writer exit */
	triggerNextStepDone(info);
	ASSERT_TRUE(0 == info->writeCounter);

	/* now let the threads clean up. First fix up handle in infoReader so that we
	 * can clean up properly
	 */
	infoReader->handle = saveHandle;
	freeSupportThreadInfo(info);
	freeSupportThreadInfo(infoReader);
}

/**
 * validates the following
 *
 * readers are excludes while another thread holds the rwmutex for write
 * once writer exits, all readers wake up and can enter
 */
TEST(RWMutex, AllReadersProceedTest)
{
	omrthread_rwmutex_t saveHandle;
	SupportThreadInfo *infoReader1;
	SupportThreadInfo *infoReader2;
	omrthread_entrypoint_t functionsToRunReader1[2];
	omrthread_entrypoint_t functionsToRunReader2[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRunReader1[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader1[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	functionsToRunReader2[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader2[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;

	infoReader1 = createSupportThreadInfo(functionsToRunReader1, 2);
	infoReader2 = createSupportThreadInfo(functionsToRunReader2, 2);

	/* set the two SupporThreadInfo structures so that they use the same rwmutex */
	saveHandle = infoReader2->handle;
	infoReader2->handle = infoReader1->handle;

	/* first enter the mutex for write */
	ASSERT_TRUE(0 == infoReader1->readCounter);
	ASSERT_TRUE(0 == infoReader2->readCounter);
	omrthread_rwmutex_enter_write(infoReader1->handle);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader1);
	ASSERT_TRUE(0 == infoReader1->readCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader2);
	ASSERT_TRUE(0 == infoReader2->readCounter);

	/* now release the rwmutex and validate that the second readers still excludes the writer */
	omrthread_monitor_enter(infoReader2->synchronization);
	omrthread_monitor_enter(infoReader1->synchronization);

	omrthread_rwmutex_exit_write(infoReader1->handle);
	ASSERT_TRUE(0 == infoReader1->writeCounter);

	/* now validate that the readers have entered the mutex*/
	omrthread_monitor_wait_interruptable(infoReader1->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader1->synchronization);
	omrthread_monitor_wait_interruptable(infoReader2->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader2->synchronization);

	ASSERT_TRUE(1 == infoReader1->readCounter);
	ASSERT_TRUE(1 == infoReader2->readCounter);

	/* ok now let the readers exit */
	triggerNextStepDone(infoReader1);
	ASSERT_TRUE(0 == infoReader1->readCounter);
	triggerNextStepDone(infoReader2);
	ASSERT_TRUE(0 == infoReader2->readCounter);

	/* now let the threads clean up. First fix up handle in infoReader so that we
	 * can clean up properly
	 */
	infoReader2->handle = saveHandle;
	freeSupportThreadInfo(infoReader1);
	freeSupportThreadInfo(infoReader2);
}

/**
 * This test validates that
 *
 * a thread can enter a rwmutex for read recursively
 * as a thread exits a rwmutex for read any threads trying to enter remain blocked
 *   the same number of exits as enters have been done
 * a thread waiting to enter a rwmutex wakes up and enter when the last exit for
 *   a series of recusive enters is called
 */
TEST(RWMutex, RecursiveReadTest)
{
	int i;
	omrthread_rwmutex_t saveHandle;
	SupportThreadInfo *infoReader1;
	SupportThreadInfo *infoWriter1;
	omrthread_entrypoint_t functionsToRunReader1[7];
	omrthread_entrypoint_t functionsToRunWriter1[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRunReader1[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader1[1] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader1[2] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader1[3] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	functionsToRunReader1[4] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	functionsToRunReader1[5] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	functionsToRunReader1[6] = (omrthread_entrypoint_t) &nop;
	functionsToRunWriter1[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter1[1] = (omrthread_entrypoint_t) &exit_rwmutex_write;

	infoReader1 = createSupportThreadInfo(functionsToRunReader1, 7);
	infoWriter1 = createSupportThreadInfo(functionsToRunWriter1, 2);

	/* set the two SupporThreadInfo structures so that they use the same rwmutex */
	saveHandle = infoWriter1->handle;
	infoWriter1->handle = infoReader1->handle;
	ASSERT_TRUE(0 == infoReader1->readCounter);
	ASSERT_TRUE(0 == infoWriter1->writeCounter);

	/* start the concurrent thread that will try to enter for read */
	startConcurrentThread(infoReader1);
	ASSERT_TRUE(1 == infoReader1->readCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoWriter1);
	ASSERT_TRUE(0 == infoWriter1->writeCounter);

	/* now re-enter 2 more times for read on the same thread */
	triggerNextStep(infoReader1);
	triggerNextStep(infoReader1);
	ASSERT_TRUE(3 == infoReader1->readCounter);

	/* now read exit the rwmutex making sure the writer is blocked until
	 * we have called the required number of exits
	 */
	for (i = 3; i > 0; i--) {
		omrthread_monitor_enter(infoWriter1->synchronization);
		triggerNextStep(infoReader1);
		/* cannot validate this on RT as although the reader has exited the mutex it
		 * will not proceed until the writer has exited */
		ASSERT_TRUE((uintptr_t)(i - 1) == infoReader1->readCounter);

		/* make sure waiter is still waiting*/
		if (i > 1) {
			omrthread_monitor_wait_interruptable(infoWriter1->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
			omrthread_monitor_exit(infoWriter1->synchronization);
			/* cannot validate this on RT as although the reader has exited the mutex it
			 * will not proceed until the writer has exited
			 */
			ASSERT_TRUE((uintptr_t)(i - 1) == infoReader1->readCounter);
			ASSERT_TRUE(0 == infoWriter1->writeCounter);
		} else {
			/* all readers have exited so writer should be in */
			omrthread_monitor_wait_interruptable(infoWriter1->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
			omrthread_monitor_exit(infoWriter1->synchronization);
			/* cannot validate this on RT as although the reader has exited the mutex it
			 * will not proceed until the writer has exited
			 */
			ASSERT_TRUE(0 == infoReader1->readCounter);
			ASSERT_TRUE(1 == infoWriter1->writeCounter);
		}
	}

	/* ok now let the writer exit */
	triggerNextStepDone(infoWriter1);
	ASSERT_TRUE(0 == infoWriter1->writeCounter);

	/* now let the threads clean up. First fix up handle in infoReader so that we
	 * can clean up properly
	 */
	triggerNextStepDone(infoReader1);
	infoWriter1->handle = saveHandle;
	freeSupportThreadInfo(infoReader1);
	freeSupportThreadInfo(infoWriter1);
}

/**
 * This test validates that
 * a thread can enter a rwmutex for write recursively
 * threads waiting to enter for read wait until the an equal number of
 * 		exits to enters have been called
 * threads waiting to enter for read wake up and enter when the last exit for read occurs
 */
TEST(RWMutex, RecursiveWriteTest)
{
	int i;
	omrthread_rwmutex_t saveHandle;
	SupportThreadInfo *infoWriter;
	SupportThreadInfo *infoReader;
	omrthread_entrypoint_t functionsToRunWriter[7];
	omrthread_entrypoint_t functionsToRunReader[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRunWriter[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter[1] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter[2] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter[3] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunWriter[4] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunWriter[5] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunWriter[6] = (omrthread_entrypoint_t) &nop;
	functionsToRunReader[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;

	infoWriter = createSupportThreadInfo(functionsToRunWriter, 7);
	infoReader = createSupportThreadInfo(functionsToRunReader, 2);

	/* set the two SupporThreadInfo structures so that they use the same rwmutex */
	saveHandle = infoReader->handle;
	infoReader->handle = infoWriter->handle;
	ASSERT_TRUE(0 == infoWriter->writeCounter);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* start the concurrent thread that will try to enter for write */
	startConcurrentThread(infoWriter);
	ASSERT_TRUE(1 == infoWriter->writeCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now re-enter 2 more times for write on the same thread */
	triggerNextStep(infoWriter);
	triggerNextStep(infoWriter);
	ASSERT_TRUE(3 == infoWriter->writeCounter);

	/* now write exit the rwmutex making sure the reader is blocked until
	 * we have called the required number of exits
	 */
	for (i = 3; i > 0; i--) {
		omrthread_monitor_enter(infoReader->synchronization);
		triggerNextStep(infoWriter);
		ASSERT_TRUE((uintptr_t)(i - 1) == infoWriter->writeCounter);

		/* make sure waiter is still waiting*/
		if (i > 1) {
			omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
			omrthread_monitor_exit(infoReader->synchronization);
			ASSERT_TRUE((uintptr_t)(i - 1) == infoWriter->writeCounter);
			ASSERT_TRUE(0 == infoReader->readCounter);
		} else {
			/* all readers have exited so writer should be in */
			omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
			omrthread_monitor_exit(infoReader->synchronization);
			ASSERT_TRUE(0 == infoWriter->writeCounter);
			ASSERT_TRUE(1 == infoReader->readCounter);
		}
	}

	/* ok now let the reader exit */
	triggerNextStepDone(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now let the threads clean up. First fix up handle in infoReader so that we
	 * can clean up properly
	 */
	triggerNextStepDone(infoWriter);
	infoReader->handle = saveHandle;
	freeSupportThreadInfo(infoWriter);
	freeSupportThreadInfo(infoReader);
}

/**
 * This test validates that
 * a thread can enter a rwmutex for write recursively
 * threads waiting to enter for read wait until the an equal number of
 * 		exits to enters have been called using when try_enter is used for one of the enters
 * threads waiting to enter for read wake up and enter when the last exit for read occurs
 * 		when try_enter was used for one of the enters
 */
TEST(RWMutex, RecursiveWriteTesttryenter)
{
	int i;
	omrthread_rwmutex_t saveHandle;
	SupportThreadInfo *infoWriter;
	SupportThreadInfo *infoReader;
	omrthread_entrypoint_t functionsToRunWriter[7];
	omrthread_entrypoint_t functionsToRunReader[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRunWriter[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter[1] = (omrthread_entrypoint_t) &try_enter_rwmutex_write;
	functionsToRunWriter[2] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter[3] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunWriter[4] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunWriter[5] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunWriter[6] = (omrthread_entrypoint_t) &nop;
	functionsToRunReader[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;

	infoWriter = createSupportThreadInfo(functionsToRunWriter, 7);
	infoReader = createSupportThreadInfo(functionsToRunReader, 2);

	/* set the two SupporThreadInfo structures so that they use the same rwmutex */
	saveHandle = infoReader->handle;
	infoReader->handle = infoWriter->handle;
	ASSERT_TRUE(0 == infoWriter->writeCounter);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* start the concurrent thread that will try to enter for write */
	startConcurrentThread(infoWriter);
	ASSERT_TRUE(1 == infoWriter->writeCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked */
	startConcurrentThread(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now re-enter 2 more times for write on the same thread */
	triggerNextStep(infoWriter);
	triggerNextStep(infoWriter);
	ASSERT_TRUE(3 == infoWriter->writeCounter);

	/* now write exit the rwmutex making sure the reader is blocked until
	 * we have called the required number of exits
	 */
	for (i = 3; i > 0; i--) {
		omrthread_monitor_enter(infoReader->synchronization);
		triggerNextStep(infoWriter);
		ASSERT_TRUE((uintptr_t)(i - 1) == infoWriter->writeCounter);

		/* make sure waiter is still waiting*/
		if (i > 1) {
			omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
			omrthread_monitor_exit(infoReader->synchronization);
			ASSERT_TRUE((uintptr_t)(i - 1) == infoWriter->writeCounter);
			ASSERT_TRUE(0 == infoReader->readCounter);
		} else {
			/* all readers have exited so writer should be in */
			omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
			omrthread_monitor_exit(infoReader->synchronization);
			ASSERT_TRUE(0 == infoWriter->writeCounter);
			ASSERT_TRUE(1 == infoReader->readCounter);
		}
	}

	/* ok now let the reader exit */
	triggerNextStepDone(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now let the threads clean up. First fix up handle in infoReader so that we
	 * can clean up properly
	 */
	triggerNextStepDone(infoWriter);
	infoReader->handle = saveHandle;
	freeSupportThreadInfo(infoWriter);
	freeSupportThreadInfo(infoReader);
}

/**
 * This test validates that
 * a thread can enter a rwmutex for read after it already has it for write
 */
TEST(RWMutex, ReadAfterWriteTest)
{
	omrthread_rwmutex_t saveHandle;
	SupportThreadInfo *infoWriter;
	SupportThreadInfo *infoReader;
	omrthread_entrypoint_t functionsToRunWriter[4];
	omrthread_entrypoint_t functionsToRunReader[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRunWriter[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRunWriter[1] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunWriter[2] = (omrthread_entrypoint_t) &exit_rwmutex_read;
	functionsToRunWriter[3] = (omrthread_entrypoint_t) &exit_rwmutex_write;
	functionsToRunReader[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRunReader[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;

	infoWriter = createSupportThreadInfo(functionsToRunWriter, 4);
	infoReader = createSupportThreadInfo(functionsToRunReader, 2);

	/* set the two SupporThreadInfo structures so that they use the same rwmutex */
	saveHandle = infoReader->handle;
	infoReader->handle = infoWriter->handle;
	ASSERT_TRUE(0 == infoWriter->writeCounter);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* start the concurrent thread that will try to enter for write */
	startConcurrentThread(infoWriter);
	ASSERT_TRUE(1 == infoWriter->writeCounter);
	ASSERT_TRUE(0 == infoWriter->readCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* have the thread that entered for write enter for read */
	omrthread_monitor_enter(infoReader->synchronization);
	triggerNextStep(infoWriter);
	ASSERT_TRUE(1 == infoWriter->writeCounter);
	ASSERT_TRUE(1 == infoWriter->readCounter);

	/* make sure waiter is still waiting*/
	omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader->synchronization);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now have the thread that entered for both write and read exit for read */
	omrthread_monitor_enter(infoReader->synchronization);
	triggerNextStep(infoWriter);
	ASSERT_TRUE(1 == infoWriter->writeCounter);
	ASSERT_TRUE(0 == infoWriter->readCounter);

	/* make sure waiter is still waiting*/
	omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader->synchronization);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now have the thread that entered for both write and read exit for write */
	omrthread_monitor_enter(infoReader->synchronization);
	triggerNextStepDone(infoWriter);
	ASSERT_TRUE(0 == infoWriter->writeCounter);
	ASSERT_TRUE(0 == infoWriter->readCounter);

	/* make sure waiter is still waiting*/
	omrthread_monitor_wait_interruptable(infoReader->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader->synchronization);
	ASSERT_TRUE(1 == infoReader->readCounter);

	/* ok now let the reader exit */
	triggerNextStepDone(infoReader);
	ASSERT_TRUE(0 == infoReader->readCounter);

	/* now let the threads clean up. First fix up handle in infoReader so that we
	 * can clean up properly
	 */
	infoReader->handle = saveHandle;
	freeSupportThreadInfo(infoWriter);
	freeSupportThreadInfo(infoReader);
}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the rwmutex for read but
 * does not block if try_enter_write was used instead of enter_write
 */
TEST(RWMutex, WritersExcludedNonBlockTest)
{
	intptr_t result = 0;
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_read;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_read;

	info = createSupportThreadInfo(functionsToRun, 2);

	/* start the concurrent thread that will try to enter for read */
	startConcurrentThread(info);
	ASSERT_TRUE(1 == info->readCounter);

	/* now try to enter for write making sure we don't block */
	result = omrthread_rwmutex_try_enter_write(info->handle);
	ASSERT_TRUE(1 == info->readCounter);
	ASSERT_TRUE(J9THREAD_RWMUTEX_WOULDBLOCK == result);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	freeSupportThreadInfo(info);
}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the rwmutex for write but
 * does not block if try_enter_write was used instead of enter_write
 */
TEST(RWMutex, WritersExcludedByWriterNonBlockTest)
{
	intptr_t result = 0;
	SupportThreadInfo *info;
	omrthread_entrypoint_t functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwmutex_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwmutex_write;

	info = createSupportThreadInfo(functionsToRun, 2);

	/* start the concurrent thread that will try to enter for write */
	startConcurrentThread(info);
	ASSERT_TRUE(1 == info->writeCounter);

	/* now try to enter for write making sure we don't block */
	result = omrthread_rwmutex_try_enter_write(info->handle);
	ASSERT_TRUE(1 == info->writeCounter);
	ASSERT_TRUE(J9THREAD_RWMUTEX_WOULDBLOCK == result);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	freeSupportThreadInfo(info);
}
