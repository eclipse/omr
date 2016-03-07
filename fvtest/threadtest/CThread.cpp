/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2007, 2016
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


#include "threadTestLib.hpp"

CThread::CThread(omrthread_t self) :
	m_self(self), m__terminated(false)
{
	m__exitlock = new CMonitor("exitlock");
}

CThread::CThread(void) :
	m_self(0), m__terminated(false)
{
	m__exitlock = new CMonitor("exitlock");

	/* create a suspended thread */
	omrthread_create_ex(&m_self, J9THREAD_ATTR_DEFAULT, 1, StartFunction, (void *)this);
}

CThread::~CThread(void)
{
	delete m__exitlock;
}

void
CThread::Start(void)
{
	omrthread_resume(m_self);
}

bool
CThread::Terminated(void)
{
	return m__terminated;
}

omrthread_t
CThread::GetId(void) const
{
	return m_self;
}

void
CThread::Interrupt(void)
{
	/* omrthread_interrupt on a dead thread will crash because the thread deallocates itself on exit */
	m__exitlock->Enter();
	if (!m__terminated) {
		omrthread_interrupt(m_self);
	}
	m__exitlock->Exit();
}

bool
CThread::IsInterrupted(void)
{
	return (omrthread_interrupted(m_self) == 0) ? false : true;
}

void
CThread::PriorityInterrupt(void)
{
	/* omrthread_interrupt on a dead thread will crash because the thread deallocates itself on exit */
	m__exitlock->Enter();
	if (!m__terminated) {
		omrthread_priority_interrupt(m_self);
	}
	m__exitlock->Exit();
}

bool
CThread::IsPriorityInterrupted(void)
{
	return (omrthread_priority_interrupted(m_self) == 0) ? false : true;
}

CThread *
CThread::Attach(void)
{
	omrthread_t newSelf;
	if (omrthread_attach_ex(&newSelf, J9THREAD_ATTR_DEFAULT) != 0) {
		return 0;
	}

	return new CThread(newSelf);
}

void
CThread::Detach(void)
{
	omrthread_detach(m_self);
}

intptr_t
CThread::Sleep(int64_t millis)
{
	return omrthread_sleep(millis);
}

intptr_t
CThread::SleepInterruptable(int64_t millis, intptr_t nanos)
{
	return omrthread_sleep_interruptable(millis, nanos);
}

omrthread_t
CThread::getThread(void)
{
	return m_self;
}

/* just a blank so that we can attach self */
intptr_t
CThread::Run(void)
{
	return 0;
}

int
CThread::StartFunction(void *data)
{
	CThread *pObj = (CThread *) data;
	intptr_t rv = pObj->Run();
	pObj->m__terminated = true;
	return (int)rv;
}
