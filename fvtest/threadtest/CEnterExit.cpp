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

/*
 * Thread that acquires and holds a monitor for a time.
 */
CEnterExit::CEnterExit(CMonitor& monitor, int sleep) :
	m_monitor(monitor), m_sleep(sleep)
{
}

intptr_t
CEnterExit::Run(void)
{
	DbgMsg::verbosePrint("ENTERING\n");
	m_monitor.Enter();
	DbgMsg::verbosePrint("ENTERED\n");
	omrthread_sleep(m_sleep);
	DbgMsg::verbosePrint("EXITING\n");
	m_monitor.Exit();
	DbgMsg::verbosePrint("EXITED\n");
	return 0;
}
