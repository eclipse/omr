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


