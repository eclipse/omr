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
#ifndef unixpriority_h
#define unixpriority_h

#include "thrdsup.h"


/**
 * Returns the policy from the priority at the requested
 * index in the priority_map[] if realtime scheduling is being used,
 * otherwise returns the default thread scheduling policy
 * @param omrthreadPriority
 * @return J9_DEFAULT_SCHED for non-realtime VMs or the appropriate
 * 		   scheduling policy stored in the priority map for realtime VMs
 */
int
omrthread_get_scheduling_policy(omrthread_prio_t omrthreadPriority);

#endif     /* unixpriority_h */


