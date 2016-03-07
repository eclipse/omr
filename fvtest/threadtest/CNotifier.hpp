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


#ifndef J9THREADTEST_CNOTIFIER_HPP_INCLUDED
#define J9THREADTEST_CNOTIFIER_HPP_INCLUDED

#include "threadTestLib.hpp"

class CNotifier: public CThread
{
public:
	CNotifier(CMonitor& monitor, bool notifyAll, int delayBeforeEnter,
			  int delayBeforeNotify, int delayBeforeExit) :
			m_monitor(monitor),
			m_delayBeforeEnter(delayBeforeEnter), m_delayBeforeNotify(delayBeforeNotify),
			m_delayBeforeExit(delayBeforeExit)
	{
	}

protected:
	virtual intptr_t
	Run(void)
	{

		if (m_delayBeforeEnter > 0) {
			Sleep(m_delayBeforeEnter);
		}
		DbgMsg::println("Notifier entering");
		m_monitor.Enter();

		if (m_delayBeforeNotify > 0) {
			Sleep(m_delayBeforeNotify);
		}
		if (m_notifyAll) {
			DbgMsg::println("Notifier notifying all ");
			m_monitor.NotifyAll();
		} else {
			DbgMsg::println("Notifier notifying one");
			m_monitor.Notify();
		}

		if (m_delayBeforeExit > 0) {
			Sleep(m_delayBeforeExit);
		}
		DbgMsg::println("Notifier exiting");
		m_monitor.Exit();
		return 0;
	}

	CMonitor& m_monitor;
	bool m_notifyAll;
	int m_delayBeforeEnter;
	int m_delayBeforeNotify;
	int m_delayBeforeExit;
};

#endif /* J9THREADTEST_CNOTIFIER_HPP_INCLUDED */
