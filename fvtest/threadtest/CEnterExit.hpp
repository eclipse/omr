/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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


#ifndef CENTEREXIT_HPP_INCLUDED
#define CENTEREXIT_HPP_INCLUDED

#include "threadTestLib.hpp"

/*
 * Thread that acquires and holds a monitor for a time.
 */
class CEnterExit: public CThread
{
public:
	CEnterExit(CMonitor& monitor, int sleep);

protected:
	virtual intptr_t Run(void);
	CMonitor& m_monitor;
	int m_sleep;
};

#endif /* CENTEREXIT_HPP_INCLUDE */
