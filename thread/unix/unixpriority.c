/*******************************************************************************
 * Copyright (c) 2006, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
