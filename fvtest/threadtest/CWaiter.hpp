/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
		DbgMsg::verbosePrint("ENTERING");
		m_monitor.Enter();
		DbgMsg::verbosePrint("ENTERED - WAITING");

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

		DbgMsg::verbosePrint("WAITED - EXITING");
		m_monitor.Exit();
		DbgMsg::verbosePrint("EXITED");
		return 0;
	}

	CMonitor& m_monitor;
	bool m_interruptable;
	int64_t m_timeToWait;
	intptr_t m_waitRetVal;
	volatile bool m_doneWaiting;
};

#endif /* J9THREADTEST_CWAITER_HPP_INCLUDED */
