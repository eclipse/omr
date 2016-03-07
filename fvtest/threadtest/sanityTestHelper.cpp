/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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


#include "threadTestLib.hpp"
#include "sanityTestHelper.hpp"

static bool TestLoopingWaitNotify(int runTime);

/*
 * Tests operations on a single thread
 */
bool
SimpleSanity(void)
{
	DbgMsg::changeIndent(1);

	CMonitor mon1(0, "monitor1");
	DbgMsg::println("Enter exit on one thread");
	mon1.Enter();
	mon1.Exit();
	DbgMsg::println("ok");

	{
		CMonitor mon2(0, "monitor2");
		DbgMsg::println("Enter on 1 and hold, contended enter on 2");
		DbgMsg::changeIndent(1);
		CEnterExit thread1(mon2, 10 * 1000);
		CEnterExit thread2(mon2, 0);

		DbgMsg::println("Starting thread 1");
		thread1.Start();
		omrthread_sleep(2 * 1000);
		DbgMsg::println("starting thread 2");
		thread2.Start();

		DbgMsg::println("waiting for thread 1 to terminate");
		while (!thread1.Terminated()) {
			omrthread_sleep(1);
		}

		DbgMsg::println("waiting for thread 2 to terminate");
		while (!thread2.Terminated()) {
			omrthread_sleep(1);
		}
		DbgMsg::changeIndent(-1);
		DbgMsg::println("ok");
	}

	{
		CMonitor mon3(0, "monitor3");
		DbgMsg::println("Enter, notify, notify all -  on one thread");
		mon3.Enter();
		mon3.Notify();
		mon3.NotifyAll();
		mon3.Exit();
		DbgMsg::println("ok");
	}

	/* just testing if we return .. not if we're accurate
	 * on the time
	 */

	DbgMsg::println("Enter, wait(1 second); Exit");
	CMonitor mon4(0, "monitor4");

	mon4.Enter();
	mon4.Wait(1000);
	mon4.Exit();
	DbgMsg::println("ok");

	/*
	 * See if there's any concept of fairness.
	 * Launch two tight loopers and see if the current
	 * thread can get in.
	 */

	DbgMsg::println("Three thread enter/exit testing");
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

	if (countAfter < countBefore) {
		DbgMsg::println("ok");
	} else {
		DbgMsg::println("Something's wrong!");
	}

	looper1.StopAndWaitForDeath();
	looper2.StopAndWaitForDeath();

	DbgMsg::changeIndent(-1);
	return true;
}

bool
TestNThreadsLooping(unsigned int numThreads, unsigned int sleepInterval,
					unsigned int runTime, bool keepCount)
{
	CMonitor mon(0, "monitor");
	unsigned int i;

	DbgMsg::changeIndent(+1);

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

	DbgMsg::print("");
	for (i = runTime; i > 0; i--) {
		DbgMsg::printRaw("{");
		omrthread_sleep(1 * 1000);
		DbgMsg::printRaw("}");
	}

	if (keepCount) {
		char buf[256];
		unsigned long count = 0;
		for (i = 0; i < numThreads; i++) {
			count += threads[i]->LoopCount();
		}
		sprintf(buf, "Performance = %lu", count);
		DbgMsg::println("");
		DbgMsg::println(buf);
	}

	for (i = 0; i < numThreads; i++) {
		DbgMsg::println("stopping thread");
		threads[i]->StopRunning();
	}

	for (i = 0; i < numThreads; i++) {
		DbgMsg::println("waiting for a thread to terminate");
		threads[i]->WaitForTermination();
	}

	delete[] threads;

	DbgMsg::changeIndent(-1);
	return true;
}

/*
 * Tests N threads contending on a single monitor with varying hold times
 */
void
SanityTestNThreads(unsigned int numThreads, unsigned int runTime)
{
	DbgMsg::changeIndent(1);

	DbgMsg::println("1000ms");
	TestNThreadsLooping(numThreads, 1000, runTime, false);

	DbgMsg::println("100ms");
	TestNThreadsLooping(numThreads, 100, runTime, false);

	DbgMsg::println("10ms");
	TestNThreadsLooping(numThreads, 10, runTime, false);

	DbgMsg::println("1ms");
	TestNThreadsLooping(numThreads, 1, runTime, false);

	DbgMsg::println("0ms");
	TestNThreadsLooping(numThreads, 0, runTime, false);

	DbgMsg::changeIndent(-1);
}

void
QuickNDirtyPerformanceTest(unsigned int runTime)
{
	DbgMsg::changeIndent(1);

	DbgMsg::println("One thread");
	TestNThreadsLooping(1, 0, runTime, true);
	DbgMsg::println("Two threads");
	TestNThreadsLooping(2, 0, runTime, true);
	DbgMsg::println("Four threads");
	TestNThreadsLooping(4, 0, runTime, true);

	DbgMsg::changeIndent(-1);
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
			DbgMsg::println("%08x not in queue", pThreads[i]->getThread());
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
		DbgMsg::println("Blocking queue is not empty");
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

	DbgMsg::println("Wait notify testing");
	DbgMsg::changeIndent(+1);
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
	DbgMsg::println("Thread 1 waiting");
	mon.Wait();
	DbgMsg::println("Thread 1 done waiting");

	/* main thread should now still be able to
	 * do things on the monitor
	 */
	mon.Notify();
	mon.Wait(100);
	mon.Exit();
	mon.Enter();
	mon.Exit();

	DbgMsg::changeIndent(-1);
	DbgMsg::println("ok");

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

	DbgMsg::print("Letting threads run for ");
	sprintf(numBuf, "%d", runTime);
	DbgMsg::printRaw(numBuf);
	DbgMsg::printRaw(" seconds...");
	DbgMsg::println("");
	DbgMsg::print("");
	for (int i = runTime; i > 0; i--) {
		sprintf(numBuf, "%d ", i);
		DbgMsg::printRaw(numBuf);
		omrthread_sleep(1000);
	}
	DbgMsg::println("");
	DbgMsg::println("Telling threads to stop running...");
	thread1.StopRunning();
	omrthread_sleep(1000);
	thread2.StopRunning();

	/* give them a chance to exit */
	omrthread_sleep(1000);
	/* notify the one thread still waiting that it's over */
	DbgMsg::println("Entering the monitor to notify the last person...");
	mon.Enter();
	DbgMsg::println("Notifying...");
	mon.Notify();
	DbgMsg::println("done notifying...");
	mon.Exit();
	DbgMsg::println("Exiting and waiting for threads to die...");
	while (doneRunningCount != 2) {
		omrthread_sleep(10);
	}
	assert(doneRunningCount == 2);
	DbgMsg::println("ok");

	return true;
}

bool
TestWaitNotify(unsigned int runTime)
{
	DbgMsg::changeIndent(+1);
#if 0
	TestSimpleWaitNotify();
#endif

	TestLoopingWaitNotify(runTime);

	DbgMsg::changeIndent(-1);

	return true;
}
