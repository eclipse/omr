/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
