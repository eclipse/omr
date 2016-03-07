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
 ******************************************************************************/

#if !defined(GCSPINLOCK_HPP_)
#define GCSPINLOCK_HPP_

#include "omrcomp.h"
#include "omrthread.h"
#include "omr.h"

typedef struct J9GCSpinlock {
    intptr_t target;
    j9sem_t osSemaphore;
    uintptr_t lockingWord;
    uintptr_t spinCount1;
    uintptr_t spinCount2;
    uintptr_t spinCount3;
} J9GCSpinlock;


intptr_t omrgc_spinlock_destroy(J9GCSpinlock *spinlock);
intptr_t omrgc_spinlock_init(J9GCSpinlock *spinlock);
intptr_t omrgc_spinlock_release(J9GCSpinlock *spinlock);
intptr_t omrgc_spinlock_acquire(J9GCSpinlock *spinlock, J9ThreadMonitorTracing*  lockTracing);

#endif /* GCSPINLOCK_HPP_ */
