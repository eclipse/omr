/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017
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

#ifndef ENVIRONMENTDELEGATE_HPP_
#define ENVIRONMENTDELEGATE_HPP_

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
	 * Set up GC_Environment for marking
	 */
	void markingStarted() { }

	/**
	 * Finalize GC_Environment after marking
	 */
	void markingFinished() { }

	/**
	 * Acquire shared VM access. Threads must acquire VM access before accessing any OMR internal
	 * structures such as the heap. Requests for VM access will be blocked if any other thread is
	 * requesting or has obtained exclusive VM access until exclusive VM access is released.
	 *
	 * This implementation is not pre-emptive. Threads that have obtained shared VM access must
	 * check frequently whether any other thread is requesting exclusive VM access and release
	 * shared VM access as quickly as possible in that event.
	 *
	 * The exclusiveAccessForGCObtainedAfterBeatenByOtherThread parameter may be required if a
	 * concurrent GC strategy is employed. It must be set (true) when this method is called after
	 * the stop-the-world GC completes and releases exclusive VM access if the calling thread
	 * previously called releaseVMAccess(exclusiveAccessForGCBeatenByOtherThread = true).
	 * Any resources temporarily released in that call can be recovered here before proceeding
	 * with exclusive VM access.
	 *
	 * @param exclusiveAccessForGCObtainedAfterBeatenByOtherThread (optional, default = false)
	 */
	void acquireVMAccess(bool exclusiveAccessForGCObtainedAfterBeatenByOtherThread = false);

	/**
	 * Check whether another thread is requesting exclusive VM access. This method must be
	 * called frequently by all threads that are holding shared VM access if the VM access framework
	 * is not pre-emptive. If this method returns true, the calling thread should release shared
	 * VM access as quickly as possible and reacquire it if necessary.

	 * @return true if another thread is waiting to acquire exclusive VM access
	 */
	bool isExclusiveAccessRequestWaiting();

	/**
	 * Release shared VM acccess.
	 *
	 * The exclusiveAccessForGCBeatenByOtherThread parameter may be required if a concurrent
	 * GC strategy is employed. It must be set (true) when this method is called by the concurrent
	 * GC if it is unable to immediately obtain exclusive VM access (because a stop-the-world GC
	 * is in progress). If the concurrent GC holds resources that must be temporarily relinquished
	 * while waiting for exclusive VM access, this is the place to release them. They can be recovered
	 * by calling acquireVMAccess(exclusiveAccessForGCObtainedAfterBeatenByOtherThread = true)
	 * when the stop-the-world GC completes and releases exclusive VM access.
	 *
	 * @param exclusiveAccessForGCBeatenByOtherThread (optional, default = false)
	 */
	void releaseVMAccess(bool exclusiveAccessForGCBeatenByOtherThread = false);

	/**
	 * Acquire exclusive VM access. This method should only be called by the OMR runtime to
	 * perform stop-the-world operations such as garbage collection. Calling thread will be
	 * blocked until all other threads holding shared VM access have release VM access.
	 */
	void acquireExclusiveVMAccess();

	/**
	 * Attempt to acquire exclusive VM access. This method may fail and return false
	 * if another thread is requesting or has obtained exclusive VM access.
	 *
	 * @return true if exclusive VM access has been obtained for the calling thread
	 */
	bool tryAcquireExclusiveVMAccess();

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
	uintptr_t relinquishExclusiveVMAccess();

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
