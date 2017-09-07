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

#include "thrdsup.h"


#if defined(J9_PRIORITY_MAP)
const int priority_map[J9THREAD_PRIORITY_MAX + 1] = J9_PRIORITY_MAP;
#else
int priority_map[J9THREAD_PRIORITY_MAX + 1];
intptr_t
initialize_priority_map(void)
{
	int i = 0;
	for (i = 0; i < (J9THREAD_PRIORITY_MAX + 1); i++) {
		priority_map[i] = 0;
	}

	return 0;
}
#endif /* defined(J9_PRIORITY_MAP) */


intptr_t
set_pthread_priority(OSTHREAD handle, omrthread_prio_t j9ThreadPriority)
{
	/* Do nothing for now on z/OS 390 */
	return 1;
}
void
initialize_thread_priority(omrthread_t thread)
{
	int policy, priority, i;

	/* set the default value */
	thread->priority = J9THREAD_PRIORITY_NORMAL;

	/* are we using priorities at all? */
	if (priority_map[J9THREAD_PRIORITY_MIN] == priority_map[J9THREAD_PRIORITY_MAX]) {
		return;
	}

	return;
}

intptr_t
set_priority_spread(void)
{
	/* This function should not be called on this platform */
	return -1;
}


