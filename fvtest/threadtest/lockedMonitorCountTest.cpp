/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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
 * verifies that omrthread_t->lockedmonitorcount is maintained correctly
 */

class LockedMonitorCountTest: public ::testing::Test
{
	/*
	 * Data members
	 */
protected:
	CThread *cthr;
	omrthread_t self;
	CMonitor mon;

	/*
	 * Function members
	 */
protected:
	virtual void
	SetUp()
	{
		cthr = CThread::Attach();
		ASSERT_TRUE(NULL != cthr);

		self = cthr->GetId();
		ASSERT_TRUE(NULL != self);
	}

	virtual void
	TearDown()
	{
		cthr->Detach();
		delete cthr;
		cthr = NULL;
		self = NULL;
	}

public:
	LockedMonitorCountTest() :
		::testing::Test(), cthr(NULL), self(NULL), mon(0, "mon")
	{
	}
};

TEST_F(LockedMonitorCountTest, TestEnterExit)
{
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	mon.Enter();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	mon.EnterUsingThreadId(*cthr);
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);
}

TEST_F(LockedMonitorCountTest, TestTryEnterExit)
{
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	ASSERT_TRUE(0 == mon.TryEnter()) << "tryenter failed";

	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	ASSERT_TRUE(0 == mon.TryEnterUsingThreadId(*cthr)) << "tryenter failed";

	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);
}

TEST_F(LockedMonitorCountTest, TestRecursiveEnterExit)
{
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	mon.Enter();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.EnterUsingThreadId(*cthr);
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.EnterUsingThreadId(*cthr);
	mon.Exit();

	mon.Exit();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);
}

TEST_F(LockedMonitorCountTest, TestRecursiveTryEnterExit)
{
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	mon.Enter();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	ASSERT_TRUE(0 == mon.TryEnterUsingThreadId(*cthr)) << "tryenter failed";

	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	ASSERT_TRUE(0 == mon.TryEnterUsingThreadId(*cthr)) << "tryenter failed";
	mon.Exit();

	ASSERT_TRUE(0 == mon.TryEnter()) << "tryenter failed";
	mon.Exit();

	mon.Exit();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);
}

TEST_F(LockedMonitorCountTest, TestWait)
{
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);

	mon.Enter();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.EnterUsingThreadId(*cthr);
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Wait(0, 1);
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)1, self->lockedmonitorcount);

	mon.Exit();
	ASSERT_EQ((uintptr_t)0, self->lockedmonitorcount);
}
