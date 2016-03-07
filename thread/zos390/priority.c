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


