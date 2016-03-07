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
#include "omrTest.h"

/*
 * Verifies return values from wait() for different methods of waking the waiter
 */

/*
 * TODO Add cases where intr & prio-intr & notify occur between the waiter's
 * COND_WAIT() and THREAD_LOCK(). Requires code instrumentation to stretch out
 * some of the intervals.
 */

static bool isRetvalOk(CWaiter& waiter, intptr_t expected);
static bool isRetvalOk(CWaiter& waiter, intptr_t expected1, intptr_t expected2);

static bool
isRetvalOk(CWaiter& waiter, intptr_t expected)
{
	bool ok = true;
	if (waiter.GetWaitReturnValue() != expected) {
		ok = false;
		DbgMsg::println("Failed:\n\twait returned %s (%d), expected %s\n",
						waiter.GetWaitReturnValueAsString(),
						waiter.GetWaitReturnValue(),
						waiter.MapReturnValueToString(expected));
	}
	return ok;
}

static bool
isRetvalOk(CWaiter& waiter, intptr_t expected1, intptr_t expected2)
{
	bool ok = true;
	if ((waiter.GetWaitReturnValue() != expected1)
		&& (waiter.GetWaitReturnValue() != expected2)) {
		ok = false;
		DbgMsg::println("Failed:\n\twait returned %s (%d), expected %s or %s\n",
						waiter.GetWaitReturnValueAsString(),
						waiter.GetWaitReturnValue(),
						waiter.MapReturnValueToString(expected1),
						waiter.MapReturnValueToString(expected2));
	}
	return ok;
}

TEST(PriorityInterrupt, testNotified)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	mon.Enter();
	mon.Notify();
	mon.Exit();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, 0));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testInterrupted)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	waiter.Interrupt();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testPrioInterrupted)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	waiter.PriorityInterrupt();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_PRIORITY_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testNotifiedAndInterrupted)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	mon.Enter();
	mon.Notify();
	waiter.Interrupt();
	mon.Exit();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, 0, J9THREAD_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testInterruptedAndNotified)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	mon.Enter();
	waiter.Interrupt();
	mon.Notify();
	mon.Exit();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_INTERRUPTED, J9THREAD_SUCCESS));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testNotifiedAndPrioInterrupted)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	mon.Enter();
	mon.Notify();
	waiter.PriorityInterrupt();
	mon.Exit();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, 0, J9THREAD_PRIORITY_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testPrioInterruptedAndNotified)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	mon.Enter();
	waiter.PriorityInterrupt();
	mon.Notify();
	mon.Exit();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_PRIORITY_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testIntrAndPrioInterrupted)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	waiter.Interrupt();
	waiter.PriorityInterrupt();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(
		isRetvalOk(waiter, J9THREAD_INTERRUPTED, J9THREAD_PRIORITY_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testAllIntr)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	waiter.Start();
	pSelf->Sleep(100);

	mon.Enter();
	mon.Notify();
	waiter.Interrupt();
	waiter.PriorityInterrupt();
	mon.Exit();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, 0, J9THREAD_PRIORITY_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testIntrBeforeWait)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	mon.Enter();
	waiter.Interrupt();
	mon.Exit();

	waiter.Start();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testPrioIntrBeforeWait)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	mon.Enter();
	waiter.PriorityInterrupt();
	mon.Exit();

	waiter.Start();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_PRIORITY_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}

TEST(PriorityInterrupt, testAllIntrBeforeWait)
{
	CThread *pSelf = CThread::Attach();
	CMonitor mon(0, "PriorityInterruptTest");
	CWaiter waiter(mon, true);

	mon.Enter();
	waiter.PriorityInterrupt();
	waiter.Interrupt();
	mon.Exit();

	waiter.Start();
	while (!waiter.DoneWaiting()) {
		pSelf->Sleep(10);
	}

	EXPECT_TRUE(isRetvalOk(waiter, J9THREAD_INTERRUPTED));

	while (!waiter.Terminated()) {
		pSelf->Sleep(10);
	}

	pSelf->Detach();
}
