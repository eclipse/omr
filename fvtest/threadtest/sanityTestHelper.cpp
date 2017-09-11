/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "threadTestLib.hpp"
#include "sanityTestHelper.hpp"

static bool TestLoopingWaitNotify(int runTime);

/*
 * Tests operations on a single thread
 */
bool
SimpleSanity(void)
{
	bool ok = true;
	omrTestEnv->changeIndent(1);

	CMonitor mon1(0, "monitor1");
	omrTestEnv->log("Enter exit on one thread\n");
	mon1.Enter();
	mon1.Exit();
	omrTestEnv->log("ok\n");

	{
		CMonitor mon2(0, "monitor2");
		omrTestEnv->log("Enter on 1 and hold, contended enter on 2\n");
		omrTestEnv->changeIndent(1);
		CEnterExit thread1(mon2, 1000);
		CEnterExit thread2(mon2, 0);

		omrTestEnv->log("Starting thread 1\n");
		thread1.Start();
		omrthread_sleep(2000);
		omrTestEnv->log("starting thread 2\n");
		thread2.Start();

		omrTestEnv->log("waiting for thread 1 to terminate\n");
		while (!thread1.Terminated()) {
			omrthread_sleep(1);
		}

		omrTestEnv->log("waiting for thread 2 to terminate\n");
		while (!thread2.Terminated()) {
			omrthread_sleep(1);
		}
		omrTestEnv->changeIndent(-1);
		omrTestEnv->log("ok\n");
	}

	{
		CMonitor mon3(0, "monitor3");
		omrTestEnv->log("Enter, notify, notify all -  on one thread\n");
		mon3.Enter();
		mon3.Notify();
		mon3.NotifyAll();
		mon3.Exit();
		omrTestEnv->log("ok\n");
	}

	/* just testing if we return .. not if we're accurate
	 * on the time
	 */

	omrTestEnv->log("Enter, wait(1 second); Exit\n");
	CMonitor mon4(0, "monitor4");

	mon4.Enter();
	mon4.Wait(1000);
	mon4.Exit();
	omrTestEnv->log("ok\n");

	/*
	 * See if there's any concept of fairness.
	 * Launch two tight loopers and see if the current
	 * thread can get in.
	 */

	omrTestEnv->log("Three thread enter/exit testing\n");
	CMonitor mon6(0, "monitor6");
	CEnterExitLooper looper1(mon6, 0);
	CEnterExitLooper looper2(mon6, 0);
	looper1.Start();
	looper2.Start();

	omrthread_sleep(1000);
	unsigned long countBefore = looper1.LoopCount() + looper2.LoopCount();
	mon6.Enter();
	looper1.ResetLoopCount();
	looper2.ResetLoopCount();
	mon6.Exit();
	unsigned long countAfter = looper1.LoopCount() + looper2.LoopCount();
	

	if (countAfter >= countBefore) {
		omrTestEnv->log(LEVEL_ERROR, "Something's wrong!\n");
		ok = false;
	}

	looper1.StopAndWaitForDeath();
	looper2.StopAndWaitForDeath();

	omrTestEnv->changeIndent(-1);
	return ok;
}

bool
TestNThreadsLooping(unsigned int numThreads, unsigned int sleepInterval,
					unsigned int runTime, bool keepCount)
{
	CMonitor mon(0, "monitor");
	unsigned int i;

	omrTestEnv->changeIndent(+1);

	CEnterExitLooper **threads = new CEnterExitLooper *[numThreads];

	for (i = 0; i < numThreads; i++) {
		threads[i] = new CEnterExitLooper(mon, sleepInterval);
	}

	/*
	 * kick all of them off at (approx) the same time
	 */
	for (i = 0; i < numThreads; i++) {
		threads[i]->Start();
	}
	
	omrTestEnv->log("");
	for (i = runTime; i > 0; i--) {
		omrTestEnv->logRaw("{");
		omrthread_sleep(1 * 1000);
		omrTestEnv->logRaw("}");
	}

	if (keepCount) {
		char buf[256];
		unsigned long count = 0;
		for (i = 0; i < numThreads; i++) {
			count += threads[i]->LoopCount();
		}
		sprintf(buf, "Performance = %lu", count);
		omrTestEnv->log("\n");
		omrTestEnv->log("%s\n", buf);
	}

	for (i = 0; i < numThreads; i++) {
		omrTestEnv->log("stopping thread\n");
		threads[i]->StopRunning();
	}

	for (i = 0; i < numThreads; i++) {
		omrTestEnv->log("waiting for a thread to terminate\n");
		threads[i]->WaitForTermination();
	}

	delete[] threads;

	omrTestEnv->changeIndent(-1);
	return true;
}

/*
 * Tests N threads contending on a single monitor with varying hold times
 */
void
SanityTestNThreads(unsigned int numThreads, unsigned int runTime)
{
	omrTestEnv->changeIndent(1);

	omrTestEnv->log("100ms\n");
	TestNThreadsLooping(numThreads, 100, runTime, false);

	omrTestEnv->log("10ms\n");
	TestNThreadsLooping(numThreads, 10, runTime, false);

	omrTestEnv->log("1ms\n");
	TestNThreadsLooping(numThreads, 1, runTime, false);

	omrTestEnv->log("0ms\n");
	TestNThreadsLooping(numThreads, 0, runTime, false);

	omrTestEnv->changeIndent(-1);
}

void
QuickNDirtyPerformanceTest(unsigned int runTime)
{
	omrTestEnv->changeIndent(1);

	omrTestEnv->log("One thread\n");
	TestNThreadsLooping(1, 0, runTime, true);
	omrTestEnv->log("Two threads\n");
	TestNThreadsLooping(2, 0, runTime, true);
	omrTestEnv->log("Four threads\n");
	TestNThreadsLooping(4, 0, runTime, true);

	omrTestEnv->changeIndent(-1);
	return;
}

/*
 * Tests queuing and dequeuing from blocking queue
 */
bool
TestBlockingQueue(CThread& self, const unsigned int numThreads)
{
#if !defined(OMR_THR_THREE_TIER_LOCKING)
	return true;
#else /* !defined(OMR_THR_THREE_TIER_LOCKING) */
	unsigned int i;
	bool ok = true;
	CMonitor mon(0, "TestBlockingQueue");
	mon.Enter();

	CEnterExit **pThreads = new CEnterExit*[numThreads];
	for (i = 0; i < numThreads; i++) {
		pThreads[i] = new CEnterExit(mon, i * 1000);
		pThreads[i]->Start();
		self.Sleep(10);
	}

	/* Hold the monitor until threads have spun out */
	while (mon.numBlocking() < numThreads) {
		self.Sleep(100);
	}

	for (i = 0; i < numThreads; i++) {
		if (!mon.isThreadBlocking(*pThreads[i])) {
			omrTestEnv->log(LEVEL_ERROR, "%08x not in queue\n", pThreads[i]->getThread());
			ok = false;
		}
	}
	mon.Exit();

	for (i = 0; i < numThreads; i++) {
		while (!pThreads[i]->Terminated()) {
			self.Sleep(1000);
		}
		delete pThreads[i];
	}

	if (mon.numBlocking() != 0) {
		omrTestEnv->log(LEVEL_ERROR, "Blocking queue is not empty\n");
		ok = false;
	}
	return ok;
#endif /* !defined(OMR_THR_THREE_TIER_LOCKING) */
}

#if 0
static bool
TestSimpleWaitNotify()
{
	/* just testing if we return .. not if we're accurate
	 * on the time
	 */

	omrTestEnv->log("Wait notify testing\n");
	omrTestEnv->changeIndent(+1);
	/* Thread 1 Thread 2
	 * enter
	 * wait()
	 * ...
	 * enter
	 * notify
	 * exit
	 */
	CMonitor mon(0, "simple wait notify");
	mon.Enter();

	CNotifier thread2(mon, false, 2000, 0, 0);
	thread2.Start();
	omrTestEnv->log("Thread 1 waiting\n");
	mon.Wait();
	omrTestEnv->log("Thread 1 done waiting\n");

	/* main thread should now still be able to
	 * do things on the monitor
	 */
	mon.Notify();
	mon.Wait(100);
	mon.Exit();
	mon.Enter();
	mon.Exit();

	omrTestEnv->changeIndent(-1);
	omrTestEnv->log("ok\n");

	return true;
}
#endif

static bool
TestLoopingWaitNotify(int runTime)
{
	char numBuf[64];
	CMonitor mon(0, "monitor");

	volatile unsigned int doneRunningCount = 0;
	volatile unsigned int notifyCount = 0;
	CWaitNotifyLooper thread1(mon, (unsigned int *)&notifyCount,
							  (unsigned int *)&doneRunningCount);
	CWaitNotifyLooper thread2(mon, (unsigned int *)&notifyCount,
							  (unsigned int *)&doneRunningCount);

	thread1.Start();
	thread2.Start();

	omrTestEnv->log("Letting threads run for ");
	sprintf(numBuf, "%d", runTime);
	omrTestEnv->logRaw(numBuf);
	omrTestEnv->logRaw(" seconds...");
	omrTestEnv->log("\n");
	omrTestEnv->log("");
	for (int i = runTime; i > 0; i--) {
		sprintf(numBuf, "%d ", i);
		omrTestEnv->logRaw(numBuf);
		omrthread_sleep(1000);
	}
	omrTestEnv->log("\n");
	omrTestEnv->log("Telling threads to stop running...\n");
	thread1.StopRunning();
	omrthread_sleep(1000);
	thread2.StopRunning();

	/* give them a chance to exit */
	omrthread_sleep(1000);
	/* notify the one thread still waiting that it's over */
	omrTestEnv->log("Entering the monitor to notify the last person...\n");
	mon.Enter();
	omrTestEnv->log("Notifying...\n");
	mon.Notify();
	omrTestEnv->log("done notifying...\n");
	mon.Exit();
	omrTestEnv->log("Exiting and waiting for threads to die...\n");
	while (doneRunningCount != 2) {
		omrthread_sleep(10);
	}
	assert(doneRunningCount == 2);
	omrTestEnv->log("ok\n");
	
	return true;
}

bool
TestWaitNotify(unsigned int runTime)
{
	omrTestEnv->changeIndent(+1);
#if 0
	TestSimpleWaitNotify();
#endif

	TestLoopingWaitNotify(runTime);

	omrTestEnv->changeIndent(-1);
	
	return true;
}
