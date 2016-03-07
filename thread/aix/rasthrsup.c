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
#include <pthread.h>
#include "omrthread.h"

uintptr_t
omrthread_get_ras_tid(void)
{
	struct __pthrdsinfo buf;
	pthread_t myThread = pthread_self();
	int retval;
	int regSize = 0;

	retval = pthread_getthrds_np(&myThread, PTHRDSINFO_QUERY_TID, &buf, sizeof(buf), NULL, &regSize);
	if (!retval) {
		return buf.__pi_tid;
	} else {
		return 0;
	}
}
