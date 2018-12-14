/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(CONCURRENTMARKINGDELEGATE_HPP_)
#define CONCURRENTMARKINGDELEGATE_HPP_

#include "omrcfg.h"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "objectdescription.h"
#include "omrgcconsts.h"
#include "omrport.h"

#include "ConcurrentSafepointCallback.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

class MM_ConcurrentGC;
class MM_MarkingScheme;

/**
 * Provides language-specific support for marking.
 */
class MM_ConcurrentMarkingDelegate
{
	/*
	 * Data members
	 */
private:

protected:
	GC_ObjectModel *_objectModel;
	MM_ConcurrentGC *_collector;
	MM_MarkingScheme *_markingScheme;

public:
	/* This enum extends ConcurrentStatus with values in the exclusive range (CONCURRENT_ROOT_TRACING,
	 * CONCURRENT_TRACE_ONLY). ConcurrentStatus extensions allow the client language to define discrete
	 * units of work that can be executed in parallel by mutator threads when they are called upon to
	 * do some tracing work to pay an allocation tax. To extract this tax, ConcurrentGC will call
	 * MM_ConcurrentMarkingDelegate::collectRoots(..., concurrentStatus, ...) with the current tracing
	 * mode determined by the value returned by MM_ConcurrentMarkingDelegate::getNextTracingMode(). The
	 * thread that receives the collectRoots() call can check the concurrentStatus value to select and
	 * execute the appropriate unit of work.
	 *
	 * @see ConcurrentStatus (omrgcconsts.h)
	 * @see MM_ConcurrentMarkingDelegate::getNextTracingMode(uintptr_t)
	 * @see MM_ConcurrentMarkingDelegate::collectRoots(MM_EnvironmentBase *, uintptr_t, bool *, bool *)
	 */
	enum {
		CONCURRENT_ROOT_TRACING1 = CONCURRENT_ROOT_TRACING + 1
	};

	/*
	 * Function members
	 */
private:

protected:

public:
	/**
	 * Initialize the delegate.
	 *
	 * @param env environment for calling thread
	 * @param collector pointer to MM_ConcurrentGC (concurrent collector)
	 * @return true if delegate initialized successfully
	 */
	bool initialize(MM_EnvironmentBase *env, MM_ConcurrentGC *collector);

	/**
	 * In the case of Weak Consistency platforms we require this method to bring mutator threads to a safe point. A safe
	 * point is a point at which a GC may occur.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return An instance of a MM_ConcurrentSafepointCallback instance that can signal all mutator threads and cause them
	 * to synchronize at a safe point
	 * @see MM_ConcurrentSafepointCallback
	 */
	MMINLINE MM_ConcurrentSafepointCallback*
	createSafepointCallback(MM_EnvironmentBase *env)
	{
		return MM_ConcurrentSafepointCallback::newInstance(env);
	}

	/**
	 * Concurrent marking component maintains a background helper thread to assist with concurrent marking
	 * tasks. The delegate may provide a specialized signal handler (and associated argument) to process
	 * signals raised from this thread.
	 *
	 * @param[out] signalHandlerArg receives (nullable) pointer to argument to be passed to signal handler when invoked
	 * @return a pointer to the signal handler function (or NULL if no signal handler)
	 */
	MMINLINE omrsig_handler_fn
	getProtectedSignalHandler(void **signalHandlerArg)
	{
		*signalHandlerArg = NULL;
		return NULL;
	}

	/**
	 * Test whether a GC can be started. Under some circumstances it may be desirable to circumvent continued
	 * concurrent marking and allow a GC to kick off immediately. In that case this method should return true
	 * and set the kick off reason.
	 *
	 * If this method returns false, the GC cycle may be started immediately. Otherwise, concurrent marking
	 * will kick off and the GC cycle will be deferred until concurrent marking completes.
	 *
	 * @param env the calling thread environment
	 * @param gcCode the GC code identifying the cause of the GC request
	 * @param[out] languageKickoffReason set this to the value to be reported as kickoff reson in verbose GC log
	 * @see J9MMCONSTANT_* (j9nonbuilderr.h) for gcCode values
	 * @return true if Kickoff can be forced
	 */
	MMINLINE bool
	canForceConcurrentKickoff(MM_EnvironmentBase *env, uintptr_t gcCode, uintptr_t *languageKickoffReason)
	{
		if (NULL != languageKickoffReason) {
			*languageKickoffReason = NO_LANGUAGE_KICKOFF_REASON;
		}
		return false;
	}

	/**
	 * Determine the next unit of tracing work that must be performed during root collection. Each distinct
	 * value returned represents a discrete unit of language-dependent root collection work. The executionMode
	 * parameter represents the current tracing mode, the returned valued with be the next tracing mode. The
	 * first call during a concurrent collection cycle will receive CONCURRENT_ROOT_TRACING as current tracing
	 * mode. When all language defined values have been returned, this method must return CONCURRENT_TRACE_ONLY
	 * to indicate that all root objects have been traced.
	 *
	 * @param executionMode the current (most recently completed) tracing mode
	 * @return the next language-defined tracing mode, or CONCURRENT_TRACE_ONLY if all language-defined roots have been traced
	 * @see MM_ConcurrentMarkingDelegate::collectRoots(MM_EnvironmentBase *, uintptr_t, bool *, bool *)
	 */
	MMINLINE uintptr_t
	getNextTracingMode(uintptr_t executionMode)
	{
		uintptr_t nextExecutionMode = CONCURRENT_TRACE_ONLY;
		switch (executionMode) {
		case CONCURRENT_ROOT_TRACING:
			nextExecutionMode = CONCURRENT_ROOT_TRACING1;
			break;
		case CONCURRENT_ROOT_TRACING1:
			nextExecutionMode = CONCURRENT_TRACE_ONLY;
			break;
		default:
			Assert_MM_unreachable();
		}

		return nextExecutionMode;
	}

	/**
	 * Trace language-defined roots. The concurrentStatus parameter receives the current tracing mode, which
	 * will be one of the language-defined tracing modes returned by getNextTracingMode().
	 *
	 * @param env the calling thread environment
	 * @param concurrentStatus the current tracing mode (language-defined ConcurrentStatus enum)
	 * @param[out] collectedRoots set this to true if all roots were collected
	 * @param[out] taxPaid set this to true if any roots were collected
	 * @see MM_ConcurrentMarkingDelegate::getNextTracingMode(uintptr_t)
	 */
	uintptr_t collectRoots(MM_EnvironmentBase *env, uintptr_t concurrentStatus, bool *collectedRoots, bool *paidTax);

	/**
	 * Informative. This method will be called when concurrent initialization is complete and root tracing
	 * is about to begin.
	 */
	MMINLINE void concurrentInitializationComplete(MM_EnvironmentBase *env) { }

	/**
	 * This method will be called to inform all mutator threads that they should trace their own
	 * thread structures and stacks to mark all thread-referenced and stack-referenced heap objects.
	 * Stack tracing can be performed only at safe points and this method may be required to request
	 * an asynchronous callback or otherwise defer this until the receiving thread can safely make
	 * the call.
	 *
	 * @see MM_ConcurrentMarkingDelegate::scanThreadRoots(MM_EnvironmentBase *
	 */
	MMINLINE void signalThreadsToTraceStacks(MM_EnvironmentBase *env) { }

	/**
	 * Once concurrent tracing has started, mutator threads must activate the appropriate
	 * write barrier(s) to trigger whenever a reference-value field is updated, until a GC cycle is started.
	 */
	MMINLINE bool signalThreadsToActivateWriteBarrier(MM_EnvironmentBase *env)
	{
		return true;
	}

	/**
	 * This can be used to optimize the concurrent write barrier(s) by conditioning threads to stop
	 * triggering barriers once a GC has started.
	 */
	MMINLINE void signalThreadsToDeactivateWriteBarrier(MM_EnvironmentBase *env) { }

	/**
	 * Informational. Will be called when concurrent tracing has completed and card cleaning has started.
	 */
	MMINLINE void cardCleaningStarted(MM_EnvironmentBase *env) { }

	/**
	 * This method is called during card cleaning for each object associated with an uncleaned,
	 * dirty card in the card table. No client actions are necessary but this method may be overridden
	 * if desired to hook into card cleaning.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to an object associated with an uncleaned, dirty card.
	 */
	MMINLINE void processItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) { }

	/**
	 * Scan a thread structure and stack frames for roots. Implementation must call
	 * MM_MarkingScheme::markObject(..) for each heap object reference found on the
	 * thread's stack or in thread structure.
	 *
	 * @param env the thread environment for the thread to be scanned
	 * @return true if the thread was scanned successfully
	 */
	MMINLINE bool
	scanThreadRoots(MM_EnvironmentBase *env)
	{
		return true;
	}

	/**
	 * Flush any roots held in thread local buffers.
	 *
	 * @param env the thread environment for the thread to be flushed
	 * @return true if any data were flushed, false otherwise
	 */
	MMINLINE bool
	flushThreadRoots(MM_EnvironmentBase *env)
	{
		return false;
	}

	/**
	 * Informational. Will be called if the concurrent collection cycle is aborted.
	 */
	MMINLINE void abortCollection(MM_EnvironmentBase *env) { }

	/**
	 * Deprecated. Use this default implementation unless otherwise required.
	 */
	MMINLINE bool
	startConcurrentScanning(MM_EnvironmentBase *env, uintptr_t *bytesTraced, bool *collectedRoots)
	{
		*bytesTraced = 0;
		*collectedRoots = false;
		return false;
	}

	/**
	 * Deprecated. Use this default implementation unless otherwise required.
	 */
	MMINLINE void concurrentScanningStarted(MM_EnvironmentBase *env, uintptr_t bytesTraced) { }

	/**
	 * Deprecated. Use this default implementation unless otherwise required.
	 */
	MMINLINE bool
	isConcurrentScanningComplete(MM_EnvironmentBase *env)
	{
		return true;
	}

	/**
	 * Deprecated. Use this default implementation unless otherwise required.
	 */
	MMINLINE uintptr_t
	reportConcurrentScanningMode(MM_EnvironmentBase *env)
	{
		return 0;
	}

	/**
	 * Constructor.
	 */
	MMINLINE MM_ConcurrentMarkingDelegate()
		: _objectModel(NULL)
		, _markingScheme(NULL)
	{ }
};

#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
#endif /* CONCURRENTMARKINGDELEGATE_HPP_ */
