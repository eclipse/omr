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

#ifndef J9THREADTEST_CTHREAD_HPP_INCLUDED
#define J9THREADTEST_CTHREAD_HPP_INCLUDED

#include "threadTestLib.hpp"

class CThread
{
public:
	CThread(omrthread_t self);
	CThread(void);
	virtual ~CThread(void);
	virtual void Start(void);

	/* disable assignment */
	CThread& operator=(const CThread&);

	bool Terminated(void);
	omrthread_t GetId(void) const;
	void Interrupt(void);
	bool IsInterrupted(void);
	void PriorityInterrupt(void);
	bool IsPriorityInterrupted(void);
	static CThread *Attach(void);
	void Detach(void);
	intptr_t Sleep(int64_t millis);
	intptr_t SleepInterruptable(int64_t millis, intptr_t nanos);
	omrthread_t getThread(void);

protected:
	virtual intptr_t Run(void);
	omrthread_t m_self;
	int m_attachCount;

private:
	static int StartFunction(void *data);
	CMonitor *m__exitlock;
	volatile bool m__terminated;
};

#endif /* J9THREADTEST_CTHREAD_HPP_INCLUDED */
