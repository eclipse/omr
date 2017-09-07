/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
