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

#if !defined(MASTERGCTHREAD_HPP_)
#define MASTERGCTHREAD_HPP_

#include "omrthread.h"
#include "modronopt.h"

#include "BaseNonVirtual.hpp"

class MM_AllocateDescription;
class MM_CycleState;
class MM_EnvironmentBase;
class MM_GCExtensionsBase;
class MM_Collector;
class MM_RememberedSetCardBucket;

/**
 * Multi-threaded mark and sweep global collector.
 * @ingroup GC_Modron_Standard
 */
class MM_MasterGCThread : public MM_BaseNonVirtual
{
/*
 * Data members
 */
public:
protected:
private:
	typedef enum MasterGCThreadState
	{
		STATE_ERROR = 0,
		STATE_DISABLED,
		STATE_STARTING,
		STATE_WAITING,
		STATE_GC_REQUESTED,
		STATE_RUNNING_CONCURRENT,
		STATE_TERMINATION_REQUESTED,
		STATE_TERMINATED,
	} MasterGCThreadState;
	omrthread_monitor_t _collectorControlMutex;	/**< The interlock between the mutator thread requesting the GC and the internal master GC thread which will drive the GC to ensure that only one of them is running, at any given point */
	volatile MasterGCThreadState _masterThreadState;	/**< The state (protected by _collectorControlMutex) of the background master GC thread */
	omrthread_t _masterGCThread;	/**< The master GC thread which drives the collect */
	MM_CycleState *_incomingCycleState;	/**< The cycle state which the caller wants the master thread to use for its requested collect */
	MM_AllocateDescription *_allocDesc;	/** The allocation failure which triggered the collection */
	MM_GCExtensionsBase *_extensions; /**< The GC extensions */
	MM_Collector *_collector; /**< The garbage collector */

/*
 * Function members
 */
public:

	/**
	 * Early initialize: montior creation
	 * globalCollector[in] the global collector (typically the caller)
	 */
	bool initialize(MM_Collector *globalCollector);

	/**
	 * Teardown resources created by initialize
	 */
	void tearDown(MM_EnvironmentBase *env);

	/**
	 * Start up the master GC thread, waiting until it reports success.
	 * This is typically called by GlobalCollector::collectorStartup()
	 * 
	 * @return true on success, false on failure
	 */
	bool startup();
	
	/**
	 * Shut down the master GC thread, waiting until it reports success.
	 * This is typically called by GlobalCollector::collectorShutdown()
	 */
	void shutdown();

	/**
	 * Perform a garbage collection in the master thread, by calling masterThreadGarbageCollect
	 * with the specified allocDescription.
	 * @param env[in] The external thread requesting that the receiver run a GC
	 * @param allocDescription[in] The description of why the GC is being run
	 * @return True if the GC was attempted or false if the collector hasn't started up yet and, therefore, couldn't run
	 */
	bool garbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
	
	/**
	 * Determine if the master GC thread is busy running a GC, or if it's idle.
	 * 
	 * @return true if a garbage collection is in progress
	 */
	bool isGarbageCollectInProgress() { return STATE_GC_REQUESTED != _masterThreadState; }
	
	MM_MasterGCThread(MM_EnvironmentBase *env);
protected:
private:
	/**
	 * This is the method called by the forked thread. The function doesn't return.
	 */
	void masterThreadEntryPoint();

	/**
	 * This is a helper function, used as a parameter to sig_protect
	 */
	static uintptr_t master_thread_proc2(OMRPortLibrary* portLib, void *info);
	
	/**
	 * This is a helper function, used as a parameter to omrthread_create
	 */
	static int J9THREAD_PROC master_thread_proc(void *info);
};

#endif /* MASTERGCTHREAD_HPP_ */
