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

