/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef ENVIRONMENTDELEGATE_HPP_
#define ENVIRONMENTDELEGATE_HPP_

#include "objectdescription.h"

class MM_EnvironmentBase;

/**
 * The GC_Environment class is opaque to OMR and may be used by the client language to
 * maintain language-specific information relating to a OMR VM thread, for example, local
 * buffers or caches that are maintained on a per thread basis.
 */
class GC_Environment
{
	/* Data members */
private:

protected:

public:

	/* Function members */
private:

protected:

public:
	GC_Environment() {}
};

/***
 * NOTE: The OMR VM access model grants exclusive VM access to one thread
 * only if no other thread has VM access. Conversely one or more threads may
 * be granted shared VM access only if no other thread holds exclusive
 * VM access. This can be modeled as a write-bias reader/writer lock where
 * threads holding shared VM access periodically check for pending
 * requests for exclusive VM access and relinquish VM access in that event.
 *
 * It is assumed that a thread owning the exclusive lock may attempt to
 * reacquire exclusive access when it already has exclusive access. Each
 * OMR thread maintains a counter to track the number of times it has acquired
 * the lock and not released it. This counter (OMR_VMThread::exclusiveCount)
 * must be maintained in the language-specific environment language interface
 * (this file).
 *
 * OMR threads must acquire shared VM access and hold it as long as they are
 * accessing VM internal structures. In the event that another thread requests
 * exclusive VM access, threads holding shared VM access must release shared
 * VM access and suspend activities involving direct access to VM structures.
 *
 * This (example) environment delegate implements a simplistic VM access framework
 * that is not pre-emptive; that is, it relies on threads that are holding non-
 * exclusive VM access to check frequently (sub-millisecond) whether any other
 * thread is requesting exclusive VM access and release non-exclusive VM
 * access immediately in that event. Continuity of VM access can be ensured by
 * reacquiring non-exclusive VM access immediately after releasing it.
 */

class MM_EnvironmentDelegate
{
	/* Data members */
private:
	MM_EnvironmentBase *_env;
	GC_Environment _gcEnv;

protected:

public:

	/* Function members */
private:

protected:

public:
	/**
	 * Bind current thread to OMR VM.
	 *
	 * @param omrVM Points to OMR VM structure
	 * @param threadName Name to assign to bound thread
	 * @param reason Language-defined value to pass to environment delegate
	 * @return Pointer to OMR_VMThread structure binding calling thread to OMR VM
	 */
	static OMR_VMThread *attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason);

	/**
	 * Unbind current thread from OMR VM
	 *
	 * @param omrVM Points to OMR VM structure
	 * @param omrVMThread Points to OMR_VMThread structure binding calling thread to OMR VM
	 * @param reason Language-defined value to pass to environment delegate
	 */
	static void detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason);

	/**
	 * Initialize the delegate's internal structures and values.
	 * @return true if initialization completed, false otherwise
	 */
	bool
	initialize(MM_EnvironmentBase *env)
	{
		_env = env;
		return true;
	}

	/**
	 * Free any internal structures associated to the receiver.
	 */
	void tearDown() { }

	/**
	 * Return the GC_Environment instance for this thread.
	 */
	GC_Environment *getGCEnvironment() { return &_gcEnv; }


	/**
	 * Flush any local material relating to GC here. This is called before a GC cycle begins,
	 * and can be used to make local GC_Environment content available to the upcoming GC as
	 * required.
	 *
	 * This is informational. OMR does not require any specific action to be implemented.
	 *
	 * @see GC_Environment
	 *
	 */
	void flushNonAllocationCaches() { }

	/**
	 * Set or clear the transient master GC status on this thread. This thread obtains master status
	 * when isMasterThread is true and relinquishes it when isMasterThread is false.
	 *
	 * This is informational. OMR does not require any specific action to be implemented.
	 *
	 * @param isMasterThread true if thread is acquiring master status, false if losing it
	 */
	void setGCMasterThread(bool isMasterThread) { }

	/**
	 * This will be called for every allocated object.  Note this is not necessarily done when the object is allocated, but will
	 * done before start of the next gc for all objects allocated since the last gc.
	 */
	bool objectAllocationNotify(omrobjectptr_t omrObject) { return true; }

	/**
	 * Acquire shared VM access. Threads must acquire VM access before accessing any OMR internal
	 * structures such as the heap. Requests for VM access will be blocked if any other thread is
	 * requesting or has obtained exclusive VM access until exclusive VM access is released.
	 *
	 * This implementation is not pre-emptive. Threads that have obtained shared VM access must
	 * check frequently whether any other thread is requesting exclusive VM access and release
	 * shared VM access as quickly as possible in that event.
	 */
	void acquireVMAccess();
	
	/**
	 * Release shared VM acccess.
	 */
	void releaseVMAccess();

	/**
	 * Check whether another thread is requesting exclusive VM access. This method must be
	 * called frequently by all threads that are holding shared VM access if the VM access framework
	 * is not pre-emptive. If this method returns true, the calling thread should release shared
	 * VM access as quickly as possible and reacquire it if necessary.

	 * @return true if another thread is waiting to acquire exclusive VM access
	 */
	bool isExclusiveAccessRequestWaiting();

	/**
	 * Acquire exclusive VM access. This method should only be called by the OMR runtime to
	 * perform stop-the-world operations such as garbage collection. Calling thread will be
	 * blocked until all other threads holding shared VM access have release VM access.
	 */
	void acquireExclusiveVMAccess();

	/**
	 * Release exclusive VM acccess. If no other thread is waiting for exclusive VM access
	 * this method will notify all threads waiting for shared VM access to continue and
	 * acquire shared VM access.
	 */
	void releaseExclusiveVMAccess();

	/**
	 * Give up exclusive access in preparation for transferring it to a collaborating thread
	 * (i.e. main-to-master or master-to-main). This may involve nothing more than
	 * transferring OMR_VMThread::exclusiveCount from the owning thread to the another
	 * thread that thereby assumes exclusive access. Implement if this kind of collaboration
	 * is required.
	 *
	 * @return the exclusive count of the current thread before relinquishing
	 * @see assumeExclusiveVMAccess(uintptr_t)
	 */
	uintptr_t relinquishExclusiveVMAccess(bool *deferredVMAccessRelease);

	/**
	 * Assume exclusive access from a collaborating thread (i.e. main-to-master or master-to-main).
	 * Implement if this kind of collaboration is required.
	 *
	 * @param exclusiveCount the exclusive count to be restored
	 * @see relinquishExclusiveVMAccess()
	 */
	void assumeExclusiveVMAccess(uintptr_t exclusiveCount);

	void releaseCriticalHeapAccess(uintptr_t *data) {}

	void reacquireCriticalHeapAccess(uintptr_t data) {}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	void forceOutOfLineVMAccess() {}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	/**
	 * Disable inline TLH allocates by hiding the real heap allocation address from
	 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
	 * looks full.
	 *
	 */
	void disableInlineTLHAllocate() {}

	/**
	 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
	 */
	void enableInlineTLHAllocate() {}

	/**
	 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
	 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otherwise
	 */
	bool isInlineTLHAllocateEnabled() { return false; }
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	MM_EnvironmentDelegate()
		: _env(NULL)
	{ }
};

#endif /* ENVIRONMENTDELEGATE_HPP_ */
