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

#include "gcspinlock.h"
#include "omrthread.h"

#include "AtomicOperations.hpp"

#define UPDATE_JLM_MON_ENTER(tracing)\
	do {\
		tracing->enter_count++;\
		/* handle the roll-over case */ \
		if (tracing->enter_count == 0){\
			tracing->enter_count++;\
			tracing->recursive_count = 0;\
			tracing->slow_count = 0;\
			tracing->holdtime_count = 0;\
			tracing->holdtime_sum = 0;\
			tracing->holdtime_avg = 0;\
			tracing->spin2_count = 0;\
			tracing->yield_count = 0;\
			tracing->spin_time = 0;\
		}\
	} while (0)

/**
 * Wait on a spinlock.
 * @param[in] s spinlock to be waited on
 * @param[in] lockTracing lock statistics
 * @return  0 on success or negative value on failure
 */
intptr_t
omrgc_spinlock_acquire(J9GCSpinlock *spinlock, J9ThreadMonitorTracing*  lockTracing)
{
	volatile intptr_t *target = (volatile intptr_t*) &spinlock->target;
	intptr_t result;
	intptr_t oldValue = -1;
	intptr_t newValue = 0;
	J9ThreadMonitorTracing* tracing = NULL;
	uintptr_t spinCount2 = 0;
	uintptr_t spinCount3 = spinlock->spinCount3;

#if defined(OMR_THR_JLM)
	tracing = lockTracing;
#endif /* OMR_THR_JLM */

	for (; spinCount3 > 0; spinCount3--) {
		for (spinCount2 = spinlock->spinCount2; spinCount2 > 0; spinCount2--) {
			if (oldValue == *target) {
				/* Try to put 0 into the target field (-1 indicates free)'. */
				if (oldValue == (intptr_t) MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*) target, oldValue, newValue))	{
					result = 0;
					goto done;
				}
			}

			MM_AtomicOperations::yieldCPU();

			/* begin tight loop */
			for (uintptr_t spinCount1 = spinlock->spinCount1; spinCount1 > 0; spinCount1--)	{
				MM_AtomicOperations::nop();
			} /* end tight loop */
		}
#if defined(OMR_THR_YIELD_ALG)
		omrthread_yield_new(spinCount3);
#else /* OMR_THR_YIELDALG */
		omrthread_yield();
#endif /* OMR_THR_YIELDALG */
	}

	/* Did not acquire the lock. Increment the count of waiting threads */
	result = *target;
	for (;;) {
		oldValue = result;
		newValue = result + 1;

		result = (intptr_t) MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*) target, oldValue, newValue);
		if (oldValue == result) {
			break;
		}
	}

	if (0 == newValue) {
		/* Acquired it through atomic, afterall. */
    	result = 0;
	} else {
		/* Wait on OS semaphore */
		result = j9sem_wait(spinlock->osSemaphore);
#if defined(OMR_THR_JLM)
		if (tracing != NULL) {
			tracing->slow_count++;
		}
#endif /* OMR_THR_JLM */
	}

done:
#if defined(OMR_THR_JLM)
	if (tracing != NULL) {
		/* if tracing counts are ever shared among multiple spinlocks, the two spin count updates should be atomic */
		uintptr_t yield_count_update = spinlock->spinCount3 - spinCount3;
		tracing->yield_count += yield_count_update;
		uintptr_t spin2_count_update = yield_count_update * spinlock->spinCount2 + (spinlock->spinCount2 - spinCount2);
		tracing->spin2_count += spin2_count_update;
		UPDATE_JLM_MON_ENTER(tracing);
	}
#endif /* OMR_THR_JLM */
	/* On out-of-order memory models (e.g. Power4), ensure that all reads and writes have been completed at this point */
	MM_AtomicOperations::readWriteBarrier();
	return result;
}

/**
 * Destroy a spinlock.
 * @param[in] s spinlock to be destroyed
 * @return  0 on success or negative value on failure
 */
intptr_t
omrgc_spinlock_destroy(J9GCSpinlock *spinlock)
{
	intptr_t result;
	result = j9sem_destroy(spinlock->osSemaphore);
	return result;
}

/**
 * Initialize a spinlock.
 * @param[in] spinlock pointer to spinlock to be initialized
 * @return  0 on success or negative value on failure
 */
intptr_t
omrgc_spinlock_init(J9GCSpinlock *spinlock)
{
	intptr_t result;

	spinlock->target = -1;
	spinlock->lockingWord = 0;

	result = j9sem_init(&spinlock->osSemaphore, 0);

	MM_AtomicOperations::writeBarrier();

	return result;
}

/**
 * Release a spinlock by 1.
 * @return  0 on success or negative value on failure
 */
intptr_t
omrgc_spinlock_release(J9GCSpinlock *spinlock)
{
	intptr_t result;
	MM_AtomicOperations::writeBarrier();
	volatile intptr_t *target = (volatile intptr_t*) &spinlock->target;

	/*
	 * Atomic decrement of target field
	 * set the new value if the current value is still the expected one
	 */
	intptr_t oldValue;
	intptr_t newValue;
	result = *target;
	for (;;) {
		oldValue = result;
		newValue = result - 1;

		result = (intptr_t) MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*) target, oldValue, newValue);
		if (oldValue == result) {
			break;
		}
	}

	if (newValue < 0) {
		result = 0;
	} else {
		result = j9sem_post(spinlock->osSemaphore); /* Release OS semaphore */
	}
	return result;
}
