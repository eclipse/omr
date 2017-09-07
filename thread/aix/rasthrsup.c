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
