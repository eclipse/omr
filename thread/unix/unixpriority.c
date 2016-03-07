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

/**
 * Both the scheduling policy and the priority come from the priority_map.
 * See @ref omrthread_get_mapped_priority() for information on how the priority
 * is obtained.
 */
int
omrthread_get_scheduling_policy(omrthread_prio_t omrthreadPriority)
{
	int policy = J9_DEFAULT_SCHED;

	/* When realtime scheduling is used the upper 8 bits are used
	 * for the scheduling policy
	 */
	if (omrthread_lib_use_realtime_scheduling()) {
		policy = (priority_map[omrthreadPriority] >> 24);
	}

	return policy;
}
