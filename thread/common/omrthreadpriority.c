/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2006, 2016
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

/**
 * @file
 * @ingroup Thread
 * @brief Threading and synchronization support
 */

#include "omrthread.h"
#include "thrdsup.h"
#include "thread_internal.h"

uintptr_t
omrthread_map_native_priority(int nativePriority)
{
	int i;
	int mapPriority;
	int normal = omrthread_get_mapped_priority(J9THREAD_PRIORITY_NORMAL);
	int ascending = omrthread_get_mapped_priority(J9THREAD_PRIORITY_MIN)
					<= omrthread_get_mapped_priority(J9THREAD_PRIORITY_MAX);

	for (i = J9THREAD_PRIORITY_MIN; i <= J9THREAD_PRIORITY_MAX; i++) {
		/* find the closest priority that has higher priority than the given priority */
		mapPriority = omrthread_get_mapped_priority(i);
		if (ascending? (nativePriority <= mapPriority) : (nativePriority >= mapPriority)) {
			if (mapPriority == normal) {
				/* if the native priority of i is the same as the native priority of J9THREAD_PRIORITY_NORMAL
				* then we default to J9THREAD_PRIORITY_NORMAL
				*/
				return J9THREAD_PRIORITY_NORMAL;
			}
			return i;
		}
	}
	/* there is no omrthread priority that is higher than the given priority, so
	 * we return the highest omrthread priority available
	 */
	return J9THREAD_PRIORITY_MAX;
}


int
omrthread_get_mapped_priority(omrthread_prio_t omrthreadPriority)
{
	int priority = priority_map[omrthreadPriority];

	/* When realtime scheduling is used the upper 8 bits are used
	 * for the scheduling policy
	 */
	if (omrthread_lib_use_realtime_scheduling()) {
		priority = (0x00ffffff & priority);
	}

	return priority;
}
