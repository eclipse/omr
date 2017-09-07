/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/
#include "omrthread.h"

/**
 * Get the RAS thread ID of the current thread.
 * The RAS thread ID is the platform-dependent thread ID that matches thread IDs used by the native debugger.
 * The current thread might not be attached to the thread library.
 *
 * @return a thread ID
 * @see omrthread_get_osId
 */
uintptr_t
omrthread_get_ras_tid(void)
{
#error omrthread_get_ras_tid() is not implemented on this platform
	return 0;
}

