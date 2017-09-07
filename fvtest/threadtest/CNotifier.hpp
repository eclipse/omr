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
		omrTestEnv->log("Notifier entering\n");
		m_monitor.Enter();

		if (m_delayBeforeNotify > 0) {
			Sleep(m_delayBeforeNotify);
		}
		if (m_notifyAll) {
			omrTestEnv->log("Notifier notifying all \n");
			m_monitor.NotifyAll();
		} else {
			omrTestEnv->log("Notifier notifying one\n");
			m_monitor.Notify();
		}

		if (m_delayBeforeExit > 0) {
			Sleep(m_delayBeforeExit);
		}
		omrTestEnv->log("Notifier exiting\n");
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
