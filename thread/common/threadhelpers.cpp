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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "AtomicSupport.hpp"

extern "C" {

#include "thrtypes.h"
#include "threaddef.h"
#include "ut_j9thr.h"

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
 * Spin on a monitor's spinlockState field until we can atomically swap out a value of SPINLOCK_UNOWNED
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
	intptr_t result = -1;
	uintptr_t oldState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;
	uintptr_t newState = J9THREAD_MONITOR_SPINLOCK_OWNED;
	omrthread_library_t const lib = self->library;

#if defined(OMR_THR_JLM)
	J9ThreadMonitorTracing *tracing = NULL;
	if (OMR_ARE_ALL_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_JLM_ENABLED)) {
		tracing = monitor->tracing;
	}
#endif /* OMR_THR_JLM */

	uintptr_t spinCount3Init = monitor->spinCount3;
	uintptr_t spinCount2Init = monitor->spinCount2;
	uintptr_t spinCount1Init = monitor->spinCount1;

#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	BOOLEAN spinning = TRUE;
	if (OMRTHREAD_IGNORE_SPIN_THREAD_BOUND != lib->maxSpinThreads) {
		if (monitor->spinThreads < lib->maxSpinThreads) {
			VM_AtomicSupport::add(&monitor->spinThreads, 1);
		} else {
			spinCount1Init = 1;
			spinCount2Init = 1;
			spinCount3Init = 1;
			spinning = FALSE;
		}
	}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

	uintptr_t spinCount3 = spinCount3Init;
	uintptr_t spinCount2 = spinCount2Init;

	for (; spinCount3 > 0; spinCount3--) {
		for (spinCount2 = spinCount2Init; spinCount2 > 0; spinCount2--) {
			/* Try to put 0 into the target field (-1 indicates free)'. */
			if (oldState == VM_AtomicSupport::lockCompareExchange(target, oldState, newState, true)) {
				result = 0;
				VM_AtomicSupport::readBarrier();
				goto update_jlm;
			}
			/* Stop spinning if adaptive spin heuristic disables spinning */
			if (OMR_ARE_ALL_BITS_SET(monitor->flags, J9THREAD_MONITOR_DISABLE_SPINNING)) {
				goto update_jlm;
			}
			VM_AtomicSupport::yieldCPU();
			/* begin tight loop */
			for (uintptr_t spinCount1 = spinCount1Init; spinCount1 > 0; spinCount1--)	{
				VM_AtomicSupport::nop();
			} /* end tight loop */
		}
#if defined(OMR_THR_YIELD_ALG)
		omrthread_yield_new(spinCount3);
#else /* OMR_THR_YIELD_ALG */
		omrthread_yield();
#endif /* OMR_THR_YIELD_ALG */
	}

update_jlm:
#if defined(OMR_THR_JLM)
	if (NULL != tracing) {
		/* Add JLM counts atomically:
		 * let m=spinCount3Init, n=spinCount2Init
		 * let i=spinCount2, j=spinCount3
		 *
		 * after partial set of spins (0 == result),
		 * yield_count += m-j, spin2_count += ((m-j)*n)+(n-i+1)
		 *
		 * after complete set of spins (-1 == result),
		 * yield_count += m, spin2_count += m*n
		 */
		uintptr_t yield_count = spinCount3Init - spinCount3;
		uintptr_t spin2_count = yield_count * spinCount2Init;
		if (0 != yield_count) {
			spin2_count += (spinCount2Init - spinCount2 + 1);
		}
		VM_AtomicSupport::add(&tracing->yield_count, yield_count);
		VM_AtomicSupport::add(&tracing->spin2_count, spin2_count);
	}
#endif /* OMR_THR_JLM */

#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	if (spinning && (OMRTHREAD_IGNORE_SPIN_THREAD_BOUND != lib->maxSpinThreads)) {
		VM_AtomicSupport::subtract(&monitor->spinThreads, 1);
	}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

	return result;
}

/**
  * Try to atomically swap out a value of SPINLOCK_UNOWNED from
  * a monitor's spinlockState field for the value SPINLOCK_OWNED.
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
	if (oldState == VM_AtomicSupport::lockCompareExchange(target, oldState, newState, true)) {
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

#if defined(OMR_THR_MCS_LOCKS)
/**
 * Acquire the MCS lock.
 *
 * @param[in] self the current omrthread_t
 * @param[in] monitor the monitor to be acquired
 * @param[in] mcsNode the MCS node belonging to self
 * @param[in] retry specifies if a MCS node is reused in which case the MCS lock queue
 * and MCS node characteristics should not be updated. Only spinning should be performed
 * against mcsNode->blocked.
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
omrthread_mcs_lock(omrthread_t self, omrthread_monitor_t monitor, omrthread_mcs_node_t mcsNode, BOOLEAN retry)
{
#if defined(THREAD_ASSERTS)
	ASSERT(mcsNode != NULL);
#endif /* defined(THREAD_ASSERTS) */

	intptr_t result = -1;
	omrthread_mcs_node_t predecessor = NULL;

#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	BOOLEAN spinning = FALSE;
	omrthread_library_t const lib = self->library;
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

	if (!retry) {
		/* Initialize the MCS node. */
		mcsNode->queueNext = NULL;
		mcsNode->monitor = NULL;
		mcsNode->thread = self;
		mcsNode->blocked = OMRTHREAD_MCS_THREAD_BLOCKED;

		/* Install the mcsNode at the tail of the MCS lock queue (monitor->queueTail). */
		predecessor = (omrthread_mcs_node_t)VM_AtomicSupport::set(
				(volatile uintptr_t *)&monitor->queueTail,
				(uintptr_t)mcsNode);
	}

	if ((NULL != predecessor) || retry) {
		/* If a predecessor MCS node exists, then the current thread blocks (waits) until it receives
		 * a notification from the thread that owns the predecessor MCS node.
		 */
		if (!retry) {
			/* Enqueue the mcsNode next to the predecessor node in the MCS lock queue. */
			predecessor->queueNext = mcsNode;
			VM_AtomicSupport::writeBarrier();
		}

		uintptr_t spinCount3Init = monitor->spinCount3;
		uintptr_t spinCount2Init = monitor->spinCount2;
		uintptr_t spinCount1Init = monitor->spinCount1;

#if defined(OMR_THR_SPIN_WAKE_CONTROL)
		spinning = TRUE;
		if (OMRTHREAD_IGNORE_SPIN_THREAD_BOUND != lib->maxSpinThreads) {
			if (monitor->spinThreads < lib->maxSpinThreads) {
				VM_AtomicSupport::add(&monitor->spinThreads, 1);
			} else {
				spinCount1Init = 1;
				spinCount2Init = 1;
				spinCount3Init = 1;
				spinning = FALSE;
			}
		}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

		/* Three-tier busy-wait loop checks if the mcsNode->blocked value is reset by the thread
		 * that owns the predecessor node. A similar three-tier busy-wait loop is also used in
		 * omrthread_spinlock_acquire, where the loop has a compare-and-swap operation.
		 * TODO: Optimize the MCS lock's three-tier busy-wait loop to account for the absent
		 * compare-and-swap operation; this corresponds to an increase in spin parameters.
		 */
		for (uintptr_t spinCount3 = spinCount3Init; spinCount3 > 0; spinCount3--) {
			for (uintptr_t spinCount2 = spinCount2Init; spinCount2 > 0; spinCount2--) {
				/* Check if the thread can acquire the lock. */
				if (OMRTHREAD_MCS_THREAD_ACQUIRE == mcsNode->blocked) {
					goto lockAcquired;
				}
				/* Stop spinning if adaptive spin heuristic disables spinning. */
				if (OMR_ARE_ALL_BITS_SET(monitor->flags, J9THREAD_MONITOR_DISABLE_SPINNING)) {
					goto exit;
				}
				VM_AtomicSupport::yieldCPU();
				/* Begin tight loop. */
				for (uintptr_t spinCount1 = spinCount1Init; spinCount1 > 0; spinCount1--) {
					VM_AtomicSupport::nop();
				} /* End tight loop. */
			}
#if defined(OMR_THR_YIELD_ALG)
			omrthread_yield_new(spinCount3);
#else /* OMR_THR_YIELD_ALG */
			omrthread_yield();
#endif /* OMR_THR_YIELD_ALG */
		}
	} else {
		/* The lock can be acquired since no predecessor MCS node exists. */
		mcsNode->blocked = OMRTHREAD_MCS_THREAD_ACQUIRE;
lockAcquired:
		/* monitor->spinlockState is maintained for compatibility with the existing omrthread API. */
		monitor->spinlockState = J9THREAD_MONITOR_SPINLOCK_OWNED;
		result = 0;

		mcsNode->monitor = monitor;

		if (NULL == self->mcsNodes->stackHead) {
			self->mcsNodes->stackHead = mcsNode;
			mcsNode->stackNext = NULL;
		} else {
			omrthread_mcs_node_t oldMCSNode = self->mcsNodes->stackHead;
			self->mcsNodes->stackHead = mcsNode;
			mcsNode->stackNext = oldMCSNode;
		}
	}

exit:
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	if (spinning && (OMRTHREAD_IGNORE_SPIN_THREAD_BOUND != lib->maxSpinThreads)) {
		VM_AtomicSupport::subtract(&monitor->spinThreads, 1);
	}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	return result;
}

/**
 * Try to acquire the MCS lock.
 *
 * @param[in] self the current omrthread_t
 * @param[in] monitor the monitor to be acquired
 * @param[in] mcsNode the MCS node belonging to self
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
omrthread_mcs_trylock(omrthread_t self, omrthread_monitor_t monitor, omrthread_mcs_node_t mcsNode)
{
#if defined(THREAD_ASSERTS)
	ASSERT(mcsNode != NULL);
#endif /* defined(THREAD_ASSERTS) */

	intptr_t result = -1;
	uintptr_t oldState = 0;

	/* Initialize the MCS node. */
	mcsNode->queueNext = NULL;
	mcsNode->blocked = OMRTHREAD_MCS_THREAD_BLOCKED;
	mcsNode->monitor = NULL;
	mcsNode->thread = self;

	/* If monitor->queueTail is NULL (no-one is waiting to acquire the lock), then it is
	 * swapped with the mcsNode, and the lock is acquired. */
	if (oldState == VM_AtomicSupport::lockCompareExchange(
			(volatile uintptr_t *)&monitor->queueTail,
			(uintptr_t)oldState,
			(uintptr_t)mcsNode)
	) {
		/* monitor->spinlockState is maintained for compatibility with the existing omrthread API. */
		monitor->spinlockState = J9THREAD_MONITOR_SPINLOCK_OWNED;

		mcsNode->monitor = monitor;
		mcsNode->blocked = OMRTHREAD_MCS_THREAD_ACQUIRE;

		if (NULL == self->mcsNodes->stackHead) {
			self->mcsNodes->stackHead = mcsNode;
			mcsNode->stackNext = NULL;
		} else {
			omrthread_mcs_node_t oldMCSNode = self->mcsNodes->stackHead;
			self->mcsNodes->stackHead = mcsNode;
			mcsNode->stackNext = oldMCSNode;
		}
		result = 0;
	}

	return result;
}

/**
 * Unlock the MCS lock.
 *
 * @param[in] self the current omrthread_t
 * @param[in] monitor the monitor to be released
 *
 * @return the next thread which will acquire the lock
 */
omrthread_t
omrthread_mcs_unlock(omrthread_t self, omrthread_monitor_t monitor)
{
	omrthread_t nextThread = NULL;

	omrthread_mcs_node_t mcsNode = self->mcsNodes->stackHead;
	omrthread_mcs_node_t prevMcsNode = mcsNode;
	while (mcsNode->monitor != monitor) {
		mcsNode = mcsNode->stackNext;
		if (mcsNode->monitor != monitor) {
			prevMcsNode = mcsNode;
		}
	}

#if defined(THREAD_ASSERTS)
	ASSERT(mcsNode != NULL);
	ASSERT(mcsNode->monitor == monitor);
	ASSERT(mcsNode->thread == self);
#endif /* defined(THREAD_ASSERTS) */

	/* Get the successor of the mcsNode. */
	if (NULL == mcsNode->queueNext) {
		/* If no successor exists, then the mcsNode is at the tail of the MCS lock queue.
		 * Release the lock by replacing the mcsNode at the tail of the queue with NULL.
		 */
		uintptr_t newState = 0;
		if ((uintptr_t)mcsNode == VM_AtomicSupport::lockCompareExchange((volatile uintptr_t *)&monitor->queueTail, (uintptr_t)mcsNode, newState)) {
			monitor->spinlockState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;
			goto lockReleased;
		}

		/* Another thread is recording a successor in mcsNode->queueNext. Wait for the thread
		 * to record the successor.
		 */
		while (NULL == mcsNode->queueNext) {
			VM_AtomicSupport::yieldCPU();
		}
	}

	/* monitor->spinlockState is maintained for compatibility with the existing omrthread API. */
	monitor->spinlockState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;

	/* Allow the successor to acquire the lock. */
	mcsNode->queueNext->blocked = OMRTHREAD_MCS_THREAD_ACQUIRE;

	nextThread = mcsNode->queueNext->thread;
lockReleased:
	/* Pop the mcsNode from the thread's MCS node stack. */
	if (mcsNode == self->mcsNodes->stackHead) {
		self->mcsNodes->stackHead = mcsNode->stackNext;
	} else {
		prevMcsNode->stackNext = mcsNode->stackNext;
	}

	/* Return the MCS node to the thread's MCS node pool since it is no longer used. */
	omrthread_mcs_node_free(self, mcsNode);

	return nextThread;
}

/**
 * Allocate memory and get an instance of OMRThreadMCSNode.
 *
 * @param[in] self the current omrthread_t
 *
 * @return a pointer to a new OMRThreadMCSNode on success and NULL on failure
 */
omrthread_mcs_node_t
omrthread_mcs_node_allocate(omrthread_t self)
{
	/* An instance of J9Pool is used per thread to manage memory for a thread's
	 * MCS nodes.
	 */
	return (omrthread_mcs_node_t)pool_newElement(self->mcsNodes->pool);
}

/**
 * Free memory and return the instance of OMRThreadMCSNode.
 *
 * @param[in] self the current omrthread_t
 * @param[in] mcsNode the MCS node belonging to self
 *
 * @return void
 */
void
omrthread_mcs_node_free(omrthread_t self, omrthread_mcs_node_t mcsNode)
{
	if (NULL != mcsNode) {
		/* Clear the fields of the mcsNode. */
		mcsNode->stackNext = NULL;
		mcsNode->queueNext = NULL;
		mcsNode->thread = NULL;
		mcsNode->blocked = OMRTHREAD_MCS_THREAD_BLOCKED;

		pool_removeElement(self->mcsNodes->pool, mcsNode);
	}
}
#endif /* defined(OMR_THR_MCS_LOCKS) */

#endif /* OMR_THR_THREE_TIER_LOCKING */

}
