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


#ifndef J9THREADTEST_CMONITOR_HPP_INCLUDED
#define J9THREADTEST_CMONITOR_HPP_INCLUDED

#include "threadTestLib.hpp"

class CMonitor
{
private:
	omrthread_monitor_t m_monitor;

public:
	CMonitor(uintptr_t flags, const char *pName);
	CMonitor(const char *pName);
	char const *GetName(void) const;
	intptr_t Enter(void);
	intptr_t EnterUsingThreadId(CThread& self);
	intptr_t TryEnter(void);
	intptr_t TryEnterUsingThreadId(CThread& self);
	intptr_t Exit(void);
	intptr_t ExitUsingThreadId(CThread& self);
	intptr_t Wait(void);
	intptr_t Wait(int64_t millis);
	intptr_t Wait(int64_t millis, intptr_t nanos);
	intptr_t WaitInterruptable(void);
	intptr_t WaitInterruptable(int64_t millis);
	intptr_t WaitInterruptable(int64_t millis, intptr_t nanos);
	void NotifyAll(void);
	void Notify(void);
	omrthread_monitor_t GetMonitor(void) const;
	~CMonitor(void);
#if defined(OMR_THR_THREE_TIER_LOCKING)
	/*
	 * These functions inspect the internal state of the monitor.
	 * They are extremely unsafe unless the test program is careful.
	 */
	unsigned int numBlocking(void);
	bool isThreadBlocking(CThread& thread);
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
};

class CRWMutex
{
private:
	omrthread_rwmutex_t m_monitor;

public:
	CRWMutex(uintptr_t flags, const char *pName);
	void EnterRead(void);
	void EnterWrite(void);
	void ExitRead(void);
	void ExitWrite(void);
	~CRWMutex(void);
};

#endif
