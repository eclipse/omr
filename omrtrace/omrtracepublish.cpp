/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#include "AtomicSupport.hpp"

#include "omrtrace_internal.h"
#include "thread_api.h"

omr_error_t
publishTraceBuffer(OMR_TraceThread *currentThr, OMR_TraceBuffer *buf)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const uint32_t bufFlags = buf->flags;

	incrementRecursionCounter(currentThr);

	/* Ensure the buffer's owner can't use it anymore.
	 * If buf->thr is NULL, then the thread hasn't tried to use it yet.
	 */
	if (NULL != buf->thr) {
		/* CAS is not currently needed because we always publish buffers synchronously from
		 * the thread that owns the trace buffer.
		 */
		buf->thr->trcBuf = NULL;
	}

	/* only publish a buffer if data has been written to it */
	if ((bufFlags & UT_TRC_BUFFER_ACTIVE) && !(bufFlags & UT_TRC_BUFFER_NEW)) {
		const uint32_t newFlags = (bufFlags & (~(UT_TRC_BUFFER_ACTIVE | UT_TRC_BUFFER_NEW))) | UT_TRC_BUFFER_FULL;
		/* CAS is not needed because flags is modified only by the thread that owns the buffer */
		buf->flags = newFlags;

		omrthread_monitor_t const subscribersLock = OMR_TRACEGLOBAL(subscribersLock);
		omrthread_monitor_enter(subscribersLock);
		for (UtSubscription *subscription = (UtSubscription *)OMR_TRACEGLOBAL(subscribers); subscription; subscription = subscription->next) {
			subscription->dataLength = OMR_TRACEGLOBAL(bufferSize);
			subscription->data = &(buf->record);

			omr_error_t subscriberRc = subscription->subscriber(subscription);
			if (OMR_ERROR_NONE != subscriberRc) {
				/* If the subscriber callback fails, call the alarm callback and
				 * remove the subscription.
				 */
				UtSubscription *subscriptionToDestroy = subscription;

				/* adjust the loop iterator */
				subscription = subscriptionToDestroy->prev;

				getTraceLock(currentThr);
				destroyRecordSubscriber(currentThr, subscriptionToDestroy, 1);
				freeTraceLock(currentThr);

				if (NULL == subscription) {
					break;
				}
			}
		}
		omrthread_monitor_exit(subscribersLock);
	}
	releaseTraceBuffer(currentThr, buf);

	decrementRecursionCounter(currentThr);
	return rc;
}

omr_error_t
releaseTraceBuffer(OMR_TraceThread *currentThr, OMR_TraceBuffer *buf)
{
	incrementRecursionCounter(currentThr);

	/* Ensure the buffer's owner can't use it anymore.
	 * If buf->thr is NULL, then the thread hasn't tried to use it yet.
	 */
	if (NULL != buf->thr) {
		/* CAS is not currently needed because we always release buffers synchronously from
		 * the thread that owns the trace buffer.
		 */
		buf->thr->trcBuf = NULL;
	}

	omrthread_monitor_enter(OMR_TRACEGLOBAL(freeQueueLock));
	buf->next = OMR_TRACEGLOBAL(freeQueue);
	OMR_TRACEGLOBAL(freeQueue) = buf;
	omrthread_monitor_exit(OMR_TRACEGLOBAL(freeQueueLock));

	decrementRecursionCounter(currentThr);
	return OMR_ERROR_NONE;
}

OMR_TraceBuffer *
recycleTraceBuffer(OMR_TraceThread *currentThr)
{
	incrementRecursionCounter(currentThr);

	omrthread_monitor_enter(OMR_TRACEGLOBAL(freeQueueLock));
	OMR_TraceBuffer *recycledBuf = OMR_TRACEGLOBAL(freeQueue);
	if (recycledBuf) {
		OMR_TRACEGLOBAL(freeQueue) = recycledBuf->next;
		recycledBuf->next = NULL;
	}
	omrthread_monitor_exit(OMR_TRACEGLOBAL(freeQueueLock));

	decrementRecursionCounter(currentThr);
	return recycledBuf;
}
