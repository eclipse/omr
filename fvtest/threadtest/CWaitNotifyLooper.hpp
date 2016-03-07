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


#ifndef J9THREADTEST_CWAITNOTIFYLOOPER_HPP_INCLUDED
#define J9THREADTEST_CWAITNOTIFYLOOPER_HPP_INCLUDED

#if 0
#define VERBOSEOUTPUT
#endif

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

		DbgMsg::verbosePrint("thread %p (%p) running\n", m_self, this);
#if VERBOSEOUTPUT
		DbgMsg::print("thread %p entering\n", m_self);
#endif
		m_monitor.Enter();
#if VERBOSEOUTPUT
		DbgMsg::print("thread %p entered\n", m_self);
#endif
		while (m_keepRunning) {
#if VERBOSEOUTPUT
			DbgMsg::print("%p N[\n", m_self);
#endif
			m_monitor.Notify();
#if VERBOSEOUTPUT
			DbgMsg::print("%p N]\n", m_self);
#endif
			*m_notifyCount = *m_notifyCount + 1;
#if VERBOSEOUTPUT
			DbgMsg::print("%p>W[ ", m_self);
#endif
			m_monitor.Wait();
#if VERBOSEOUTPUT
			DbgMsg::print("%p>W] ", m_self);
#endif
		}
		*m_doneRunningCount = *m_doneRunningCount + 1;
#if VERBOSEOUTPUT
		DbgMsg::println("thread %p exiting monitor", m_self);
#endif
		m_monitor.Exit();

		DbgMsg::verbosePrint("thread %p exiting\n", m_self);
		return 0;
	}

	CMonitor& m_monitor;
	volatile bool m_keepRunning;
	unsigned int *m_notifyCount;
	unsigned int *m_doneRunningCount;
};

#endif /* J9THREADTEST_CWAITNOTIFYLOOPER_HPP_INCLUDED */
