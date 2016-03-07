/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

#ifndef ENVIRONMENTLANGUAGEINTERFACE_HPP_
#define ENVIRONMENTLANGUAGEINTERFACE_HPP_

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "objectdescription.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
class MM_ConcurrentGCStats;
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

class MM_Collector;

class MM_EnvironmentLanguageInterface : public MM_BaseVirtual
{
private:
protected:
	OMR_VMThread *_omrThread;  /**< Associated OMR_VMThread */
	OMR_VM *_omrVM;
	OMRPortLibrary *_portLibrary;
	MM_EnvironmentBase *_env;  /**< Associated Environment */
	uintptr_t _exclusiveCount; /**< count of recursive exclusive VM access requests */
	uint64_t _exclusiveAccessTime; /**< time (in ticks) of the last exclusive access request */
	uint64_t _meanExclusiveAccessIdleTime; /**< mean idle time (in ticks) of the last exclusive access request */
	OMR_VMThread* _lastExclusiveAccessResponder; /**< last thread to respond to last exclusive access request */
	uintptr_t _exclusiveAccessHaltedThreads; /**< number of threads halted by last exclusive access request */
	bool _exclusiveAccessBeatenByOtherThread; /**< true if last exclusive access request had to wait for another GC thread */
public:

private:
protected:
	MM_EnvironmentLanguageInterface(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		,_omrThread(env->getOmrVMThread())
		,_omrVM(env->getOmrVM())
		,_portLibrary(env->getPortLibrary())
		,_env(env)
		,_exclusiveCount(0)
		,_exclusiveAccessTime(0)
		,_meanExclusiveAccessIdleTime(0)
		,_lastExclusiveAccessResponder(NULL)
		,_exclusiveAccessHaltedThreads(0)
		,_exclusiveAccessBeatenByOtherThread(false)
	{
		_typeId = __FUNCTION__;
	};

	void reportExclusiveAccessAcquire();
	void reportExclusiveAccessRelease();

public:
	virtual void kill(MM_EnvironmentBase *env) = 0;

	/**
	 * Acquire shared VM access.
	 */
	virtual void acquireVMAccess(MM_EnvironmentBase *env) = 0;

	/**
	 * Releases shared VM access.
	 */
	virtual void releaseVMAccess(MM_EnvironmentBase *env) = 0;

	/**
	 * Try and acquire exclusive access if no other thread is already requesting it.
	 * Make an attempt at acquiring exclusive access if the current thread does not already have it.  The
	 * attempt will abort if another thread is already going for exclusive, which means this
	 * call can return without exclusive access being held.  As well, this call will block for any other
	 * requesting thread, and so should be treated as a safe point.
	 * @note call can release VM access.
	 * @return true if exclusive access was acquired, false otherwise.
	 */
	virtual bool tryAcquireExclusiveVMAccess() = 0;

	/**
	 * Acquire exclusive VM access.
	 */
	virtual void acquireExclusiveVMAccess() = 0;

	/**
	 * Releases exclusive VM access.
	 */
	virtual void releaseExclusiveVMAccess() = 0;

	/**
	 * Checks to see if the thread has exclusive access
	 * @return true if the thread has exclusive access, false if not.
	 */
	virtual bool inquireExclusiveVMAccessForGC() = 0;

	/**
	 * Acquire exclusive access to request a gc.
	 * The calling thread will acquire exclusive access for Gc regardless if other threads beat it to exclusive for the same purposes.
	 * @param collector gc intended to be used for collection.
	 * @return boolean indicating whether the thread cleanly won exclusive access for its collector or if it had been beaten already.
	 *
	 * @note this call should be considered a safe-point as the thread may release VM access to allow the other threads to acquire exclusivity.
	 * @note this call supports recursion.
	 */
	virtual bool acquireExclusiveVMAccessForGC(MM_Collector *collector) = 0;

	/**
	 * Release exclusive access.
	 * The calling thread will release one level (recursion) of its exclusive access request, and alert other threads that it has completed its
	 * intended GC request if this is the last level.
	 */
	virtual void releaseExclusiveVMAccessForGC() = 0;

	/**
	 * Release all recursive levels of exclusive access.
	 * The calling thread will release all levels (recursion) of its exclusive access request, and alert other threads that it has completed its
	 * intended GC request.
	 */
	virtual void unwindExclusiveVMAccessForGC() = 0;

	/**
	 * Attempt to acquire exclusive access to request a gc.
	 * If the thread is beaten to acquiring exclusive access by another thread then the caller will not acquire exclusive access.
	 * @param collector gc intended to be used for collection.
	 * @return boolean indicating whether the thread now holds exclusive access (ie: it won the race).
	 *
	 * @note this call should be considered a safe-point as the thread may release VM access to allow the winning thread to acquire exclusivity.
	 * @note this call supports recursion.
	 */
	virtual bool tryAcquireExclusiveVMAccessForGC(MM_Collector *collector) = 0;


#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	/**
	 * Try and acquire exclusive access if no other thread is already requesting it.
	 * Make an attempt at acquiring exclusive access if the current thread does not already have it.  The
	 * attempt will abort if another thread is already going for exclusive, which means this
	 * call can return without exclusive access being held, it will also abort if another another thread
	 * may have beat us to it and prepared the threads or even collected. As well, this call will block for
	 * any other requesting thread, and so should be treated as a safe point.
	 * @note call can release VM access.
	 * @return true if exclusive access was acquired, false otherwise.
	 */
	virtual bool tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats) = 0;

	/**
	 * Release exclusive access.
	 * The calling thread will release one level (recursion) of its exclusive access request, and alert other threads that it has completed its
	 * intended GC request if this is the last level.
	 */
	virtual void releaseExclusiveForConcurrentKickoff() = 0;
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	/**
	 * Checks to see if any thread has requested exclusive access
	 * @return true if a thread is waiting on exclusive access, false if not.
	 */
	virtual bool isExclusiveAccessRequestWaiting() = 0;

	/**
	 * Get the time taken to acquire exclusive access.
	 * Time is stored in raw format (no units).  Output routines
	 * are responsible for converting to the desired resolution (msec, usec)
	 * @return The time taken to acquire exclusive access.
	 */
	virtual uint64_t getExclusiveAccessTime() = 0;

	/**
	 * Get the time average threads were idle while acquiring exclusive access.
	 * Time is stored in raw format (no units).  Output routines
	 * are responsible for converting to the desired resolution (msec, usec)
	 * @return The mean idle time during exclusive access acquisition.
	 */
	virtual uint64_t getMeanExclusiveAccessIdleTime() = 0;

	/**
	 * Get the last thread to respond to the exclusive access request.
	 * @return The last thread to respond
	 */
	virtual OMR_VMThread* getLastExclusiveAccessResponder() = 0;

	/**
	 * Get the number of threads which were halted for the exclusive access request.
	 * @return The number of halted threads
	 */
	virtual uintptr_t getExclusiveAccessHaltedThreads() = 0;

	/**
	 * Enquire whether we were beaten to exclusive access by another thread.
	 * @return true if we were beaten, false otherwise.
	 */
	virtual bool exclusiveAccessBeatenByOtherThread() = 0;

	/**
	 * Saves the given object in the calling VMThread's allocateObjectSavePrivate1 field so that it will be kept alive and can be updated if moved during a GC.
	 *
	 * @param objectPtr The pointer to store (note that this MUST be an OBJECT POINTER and not a HEAP POINTER)
	 * @return true if the object was stored, false if the slot was already in use
	 */
	virtual bool saveObjects(omrobjectptr_t objectPtr) = 0;

	/**
	 * Fetches and clears the calling VMThread's allocateObjectSavePrivate1 field stored by a previous call to saveObjects.
	 *
	 * @param objectPtrIndirect The pointer to the slot which would be restored (note that the restored slot will be an OBJECT POINTER and not a HEAP POINTER)
	 */
	virtual void restoreObjects(omrobjectptr_t *objectPtrIndirect) = 0;

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	/**
	 * Disable inline TLH allocates by hiding the real heap allocation address from
	 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
	 * looks full.
	 *
	 */
	virtual void disableInlineTLHAllocate() = 0;

	/**
	 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
	 */
	virtual void enableInlineTLHAllocate() = 0;

	/**
	 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
	 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otherwise
	 */
	virtual bool isInlineTLHAllocateEnabled() = 0;
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void parallelMarkTask_setup(MM_EnvironmentBase *env) = 0;
	virtual void parallelMarkTask_cleanup(MM_EnvironmentBase *env) = 0;

};

#endif /* ENVIRONMENTLANGUAGEINTERFACE_HPP_ */
