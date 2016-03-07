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

#include "AtomicSupport.hpp"

extern "C" {

#include "thrtypes.h"
#include "threaddef.h"

void
omrthread_monitor_pin(omrthread_monitor_t monitor, omrthread_t self)
{
	VM_AtomicSupport::add(&monitor->pinCount, 1);
}

void
omrthread_monitor_unpin(omrthread_monitor_t monitor, omrthread_t self)
{
	VM_AtomicSupport::subtract(&monitor->pinCount, 1);
}

#if defined(OMR_THR_THREE_TIER_LOCKING)

/**
 * Spin on a monitor's lockingWord field until we can atomically swap out a value of SPINLOCK_UNOWNED
 * for the value SPINLOCK_OWNED.
 *
 * @param[in] self the current omrthread_t
 * @param[in] monitor the monitor whose spinlock will be acquired
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
omrthread_spinlock_acquire(omrthread_t self, omrthread_monitor_t monitor)
{
	volatile uintptr_t *target = (volatile uintptr_t *)&monitor->spinlockState;
	intptr_t result = 0;
	uintptr_t oldState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;
	uintptr_t newState = J9THREAD_MONITOR_SPINLOCK_OWNED;
#if defined(OMR_THR_JLM)
	J9ThreadMonitorTracing *tracing = (self->library->flags & J9THREAD_LIB_FLAG_JLM_ENABLED) ? monitor->tracing : NULL;
#endif /* OMR_THR_JLM */

	for (uintptr_t spinCount3 = monitor->spinCount3; spinCount3 > 0; spinCount3--) {
		for (uintptr_t spinCount2 = monitor->spinCount2; spinCount2 > 0; spinCount2--) {
			/* Try to put 0 into the target field (-1 indicates free)'. */
			if (oldState == VM_AtomicSupport::lockCompareExchange(target, oldState, newState)) {
#if defined(OMR_THR_JLM)
				if (NULL != tracing) {
					/* Update JLM spin counts after partial set of spins - add JLM counts atomically.
					 * let m=monitor _spinCount3, n=monitor _spinCount2
					 * let i=spinCount2, j=spinCount3
					 * then yield count += m-j, spin2 count += (m-j)*n + (n-i)+1
					 */
					uintptr_t m = monitor->spinCount3;
					uintptr_t n = monitor->spinCount2;
					uintptr_t j = spinCount3;
					uintptr_t i = spinCount2;
					VM_AtomicSupport::add(&tracing->yield_count, m - j);
					VM_AtomicSupport::add(&tracing->spin2_count, ((m - j) * n) + n - i + 1);
				}
#endif /* OMR_THR_JLM */
				VM_AtomicSupport::readBarrier();
				goto done;
			}

			VM_AtomicSupport::yieldCPU();

			/* begin tight loop */
			for (uintptr_t spinCount1 = monitor->spinCount1; spinCount1 > 0; spinCount1--)	{
				VM_AtomicSupport::nop();
			} /* end tight loop */
		}
#if defined(OMR_THR_YIELD_ALG)
		omrthread_yield_new(spinCount3);
#else /* OMR_THR_YIELD_ALG */
		omrthread_yield();
#endif /* OMR_THR_YIELD_ALG */
	}
	result = -1;
#if defined(OMR_THR_JLM)
	if (NULL != tracing) {
		/*update JLM spin counts after complete set of spins - add JLM counts atomically
		 * let m=monitor _spinCount3, n=monitor _spinCount2
		 * then yield count += m, spin2 count += m*n
		 */
		uintptr_t m = monitor->spinCount3;
		uintptr_t n = monitor->spinCount2;
		VM_AtomicSupport::add(&tracing->yield_count, m);
		VM_AtomicSupport::add(&tracing->spin2_count, m * n);
	}
#endif /* OMR_THR_JLM */
done:
	return result;
}

/**
  * Try to atomically swap out a value of SPINLOCK_UNOWNED from
  * a monitor's lockingWord field for the value SPINLOCK_OWNED.
  *
  * @param[in] self the current omrthread_t
  * @param[in] monitor the monitor whose spinlock will be acquired
  *
  * @return 0 on success, -1 on failure
  */
intptr_t
omrthread_spinlock_acquire_no_spin(omrthread_t self, omrthread_monitor_t monitor)
{
	intptr_t result = -1;
	volatile uintptr_t *target = (volatile uintptr_t *)&monitor->spinlockState;
	uintptr_t oldState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;
	uintptr_t newState = J9THREAD_MONITOR_SPINLOCK_OWNED;
	if (J9THREAD_MONITOR_SPINLOCK_UNOWNED == VM_AtomicSupport::lockCompareExchange(target, oldState, newState)) {
		result = 0;
		VM_AtomicSupport::readBarrier();
	}
	return result;
}

/**
  * Atomically swap in a new value for a monitor's spinlockState.
  *
  * @param[in] monitor the monitor to modify
  * @param[in] newState the new value for spinlockState
  *
  * @return the previous value for spinlockState
  */
uintptr_t
omrthread_spinlock_swapState(omrthread_monitor_t monitor, uintptr_t newState)
{
	volatile uintptr_t *target = (volatile uintptr_t *)&monitor->spinlockState;
	/* If we are writing in UNOWNED, we are exiting the critical section, therefore
	 * have to finish up any writes.
	 */
	if (J9THREAD_MONITOR_SPINLOCK_UNOWNED == newState) {
		VM_AtomicSupport::writeBarrier();
	}
	uintptr_t oldState = VM_AtomicSupport::set(target, newState);
	/* If we entered the critical section, (i.e. we swapped out UNOWNED) then
	 * we have to issue a readBarrier.
	 */
	if (J9THREAD_MONITOR_SPINLOCK_UNOWNED == oldState) {
		VM_AtomicSupport::readBarrier();
	}
	return oldState;
}

#endif /* OMR_THR_THREE_TIER_LOCKING */

}
