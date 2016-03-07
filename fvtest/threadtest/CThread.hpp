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
