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

#ifndef J9THREADTEST_CWAITER_HPP_INCLUDED
#define J9THREADTEST_CWAITER_HPP_INCLUDED

#include "threadTestLib.hpp"

class CWaiter: public CThread
{
public:

	CWaiter(CMonitor& monitor, bool interruptable) :
		m_monitor(monitor),
		m_interruptable(interruptable),
		m_timeToWait(-1),
		m_doneWaiting(false)
	{
	}

	CWaiter(CMonitor& monitor, bool interruptable, int64_t timeToWait) :
		m_monitor(monitor), m_interruptable(interruptable), m_timeToWait(
			timeToWait), m_doneWaiting(false)
	{
	}

	bool
	DoneWaiting()
	{
		return m_doneWaiting;
	}
	intptr_t
	GetWaitReturnValue()
	{
		return m_waitRetVal;
	}
	const char *
	GetWaitReturnValueAsString() const
	{
		const char *pRetVal;

		pRetVal = MapReturnValueToString(m_waitRetVal);
		return pRetVal;
	}

	static const char *
	MapReturnValueToString(intptr_t retval)
	{
		const char *pRetVal = "<unknown return value>";

		switch (retval) {
		case 0:
			pRetVal = "0 (zero)";
			break;
		case J9THREAD_INVALID_ARGUMENT:
			pRetVal = "J9THREAD_INVALID_ARGUMENT";
			break;
		case J9THREAD_ILLEGAL_MONITOR_STATE:
			pRetVal = "J9THREAD_ILLEGAL_MONITOR_STATE";
			break;
		case J9THREAD_INTERRUPTED:
			pRetVal = "J9THREAD_INTERRUPTED";
			break;
		case J9THREAD_PRIORITY_INTERRUPTED:
			pRetVal = "J9THREAD_PRIORITY_INTERRUPTED";
			break;
		case J9THREAD_TIMED_OUT:
			pRetVal = "J9THREAD_TIMED_OUT";
			break;
		}
		return pRetVal;
	}

protected:
	virtual intptr_t 
	Run()
	{
		omrTestEnv->log(LEVEL_VERBOSE, "ENTERING");
		m_monitor.Enter();
		omrTestEnv->log(LEVEL_VERBOSE, "ENTERED - WAITING");

		if (m_timeToWait < 0) {
			m_waitRetVal =
				(m_interruptable) ?
					m_monitor.WaitInterruptable() : m_monitor.Wait();
		} else {
			m_waitRetVal =
				(m_interruptable) ?
					m_monitor.WaitInterruptable(m_timeToWait) :
					m_monitor.Wait(m_timeToWait);
		}
		m_doneWaiting = true;

		omrTestEnv->log(LEVEL_VERBOSE, "WAITED - EXITING");
		m_monitor.Exit();
		omrTestEnv->log(LEVEL_VERBOSE, "EXITED");
		return 0;
	}

	CMonitor& m_monitor;
	bool m_interruptable;
	int64_t m_timeToWait;
	intptr_t m_waitRetVal;
	volatile bool m_doneWaiting;
};

#endif /* J9THREADTEST_CWAITER_HPP_INCLUDED */
