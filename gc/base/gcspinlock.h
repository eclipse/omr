/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(GCSPINLOCK_HPP_)
#define GCSPINLOCK_HPP_

#include "omrcomp.h"
#include "omrthread.h"
#include "omr.h"

typedef struct J9GCSpinlock {
    intptr_t target;
    j9sem_t osSemaphore;
    uintptr_t spinCount1;
    uintptr_t spinCount2;
    uintptr_t spinCount3;
} J9GCSpinlock;


intptr_t omrgc_spinlock_destroy(J9GCSpinlock *spinlock);
intptr_t omrgc_spinlock_init(J9GCSpinlock *spinlock);
intptr_t omrgc_spinlock_release(J9GCSpinlock *spinlock);
intptr_t omrgc_spinlock_acquire(J9GCSpinlock *spinlock, J9ThreadMonitorTracing*  lockTracing);

#endif /* GCSPINLOCK_HPP_ */
