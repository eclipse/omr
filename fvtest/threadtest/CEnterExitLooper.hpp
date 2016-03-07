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


#ifndef CENTEREXITLOOPER_HPP_INCLUDED
#define CENTEREXITLOOPER_HPP_INCLUDED

#include "threadTestLib.hpp"

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
		DbgMsg::verbosePrint("thread %p running, sleep = %d\n", m_self,
			m_sleep);
		while (m_keepRunning) {
			m_monitor.Enter();
			if (m_sleep > 0) {
				omrthread_sleep(m_sleep);
			}
			++m_loopCount;
			m_monitor.Exit();
		}
		DbgMsg::verbosePrint("thread %p exiting\n", m_self);
		return 0;
	}

	volatile bool m_keepRunning;
	volatile unsigned long m_loopCount;
	CMonitor& m_monitor;
	int m_sleep;
};

#endif /* CENTEREXITLOOPER_HPP_INCLUDED */
