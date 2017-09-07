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

#ifndef CENTEREXITLOOPER_HPP_INCLUDED
#define CENTEREXITLOOPER_HPP_INCLUDED

#include "threadTestLib.hpp"
#include "testHelper.hpp"

extern ThreadTestEnvironment *omrTestEnv;

class CEnterExitLooper: public CThread
{
public:
	CEnterExitLooper(CMonitor& monitor, int sleep) :
		m_keepRunning(true), m_loopCount(0), m_monitor(monitor), m_sleep(sleep)
	{
	}

	void
	StopRunning(void)
	{
		m_keepRunning = false;
	}

	void
	StopAndWaitForDeath(void)
	{
		StopRunning();
		while (!Terminated()) {
			omrthread_sleep(1);
		}
	}

	void
	WaitForTermination(void)
	{
		while (!Terminated()) {
			omrthread_sleep(1);
		}
	}

	unsigned long
	LoopCount(void) const
	{
		return m_loopCount;
	}

	void
	ResetLoopCount(void)
	{
		/* unsafe */
		m_loopCount = 0;
	}

protected:
	virtual intptr_t
	Run(void)
	{
		omrTestEnv->log(LEVEL_VERBOSE,"thread %p running, sleep = %d\n", 
			m_self, m_sleep);
		while (m_keepRunning) {
			m_monitor.Enter();
			if (m_sleep > 0) {
				omrthread_sleep(m_sleep);
			}
			++m_loopCount;
			m_monitor.Exit();
		}
		omrTestEnv->log(LEVEL_VERBOSE, "thread %p exiting\n", m_self);
		return 0;
	}

	volatile bool m_keepRunning;
	volatile unsigned long m_loopCount;
	CMonitor& m_monitor;
	int m_sleep;
};

#endif /* CENTEREXITLOOPER_HPP_INCLUDED */
