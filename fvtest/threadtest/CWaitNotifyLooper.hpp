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

#ifndef J9THREADTEST_CWAITNOTIFYLOOPER_HPP_INCLUDED
#define J9THREADTEST_CWAITNOTIFYLOOPER_HPP_INCLUDED

#include "threadTestLib.hpp"

class CWaitNotifyLooper: public CThread
{
public:
	CWaitNotifyLooper(CMonitor& monitor, unsigned int* notifyCount, unsigned int* doneRunningCount) :
			m_monitor(monitor),
			m_keepRunning(true),
			m_notifyCount(notifyCount),
			m_doneRunningCount(doneRunningCount)
	{
		assert(m_notifyCount);
		assert(m_doneRunningCount);
	}

	void
	StopRunning(void)
	{
		m_keepRunning = false;
	}

protected:
	virtual intptr_t
	Run(void)
	{

		omrTestEnv->log(LEVEL_VERBOSE, "thread %p (%p) running\n", m_self, this);
		omrTestEnv->log(LEVEL_VERBOSE, "thread %p entering\n", m_self);
		m_monitor.Enter();
		omrTestEnv->log(LEVEL_VERBOSE, "thread %p entered\n", m_self);
		while (m_keepRunning) {
			omrTestEnv->log(LEVEL_VERBOSE, "%p N[\n", m_self);
			m_monitor.Notify();
			omrTestEnv->log(LEVEL_VERBOSE, "%p N]\n", m_self);
			*m_notifyCount = *m_notifyCount + 1;
			omrTestEnv->log(LEVEL_VERBOSE, "%p>W[ ", m_self);
			m_monitor.Wait();
			omrTestEnv->log(LEVEL_VERBOSE, "%p>W] ", m_self);
		}
		*m_doneRunningCount = *m_doneRunningCount + 1;
		omrTestEnv->log(LEVEL_VERBOSE, "thread %p exiting monitor\n", m_self);
		m_monitor.Exit();

		omrTestEnv->log(LEVEL_VERBOSE, "thread %p exiting\n", m_self);
		return 0;
	}

	CMonitor& m_monitor;
	volatile bool m_keepRunning;
	unsigned int *m_notifyCount;
	unsigned int *m_doneRunningCount;
};

#endif /* J9THREADTEST_CWAITNOTIFYLOOPER_HPP_INCLUDED */
