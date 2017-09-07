/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
