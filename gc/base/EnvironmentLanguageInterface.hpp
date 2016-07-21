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

#include "omrcfg.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"

#include "ModronAssertions.h"
#include "objectdescription.h"
#include "omr.h"
#include "omrvm.h"

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
public:

private:
protected:
	MM_EnvironmentLanguageInterface(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		,_omrThread(env->getOmrVMThread())
		,_omrVM(env->getOmrVM())
		,_portLibrary(env->getPortLibrary())
		,_env(env)
	{
		_typeId = __FUNCTION__;
	};

public:
	virtual void kill(MM_EnvironmentBase *env) = 0;

	/***
	 * NOTE: The OMR VM access model grants exclusive VM access to one thread
	 * only if no other thread has VM access. Conversely one of more threads may
	 * be granted shared VM access only if no other thread holds exclusive
	 * VM access. This can be modeled as a write-bias reader/writer lock where
	 * threads holding shared VM access periodically check for pending
	 * requests for exclusive VM access and relinquish VM access in that event.
	 *
	 * OMR threads must acquire shared VM access and hold it as long as they are
	 * accessing VM internal structures. In the event that another thread requests
	 * exclusive VM access, threads holding shared VM access must release shared
	 * VM access and suspend activities involving direct access to VM structures.
	 */

	/**
	 * Acquire shared VM access. Implementation must block calling thread as long
	 * as any other thread has exclusive VM access.
	 */
	virtual void acquireVMAccess() = 0;

	/**
	 * Release shared VM access. This method should be called
	 */
	virtual void releaseVMAccess() = 0;

	/**
	 * Acquire exclusive VM access. This must lock the VM thread list mutex (OMR_VM::_vmThreadListMutex).
	 *
	 * This method may be called by a thread that already holds exclusive VM access. In that case, the
	 * OMR_VMThread::exclusiveCount counter is incremented.
	 */
	virtual void
	acquireExclusiveVMAccess()
	{
		if (0 == _omrThread->exclusiveCount) {
			omrthread_monitor_enter(_env->getOmrVM()->_vmThreadListMutex);
		}
		_omrThread->exclusiveCount += 1;
	}

	/**
	 * Try and acquire exclusive access if no other thread is already requesting it.
	 * Make an attempt at acquiring exclusive access if the current thread does not already have it.  The
	 * attempt will abort if another thread is already going for exclusive, which means this
	 * call can return without exclusive access being held.  As well, this call will block for any other
	 * requesting thread, and so should be treated as a safe point.
	 * @note call can release VM access.
	 * @return true if exclusive access was acquired, false otherwise.
	 */
	virtual bool
	tryAcquireExclusiveVMAccess()
	{
		this->acquireExclusiveVMAccess();
		return true;
	}

	/**
	 * Releases exclusive VM access.
	 */
	virtual void
	releaseExclusiveVMAccess()
	{
		Assert_MM_true(0 < _omrThread->exclusiveCount);
		_omrThread->exclusiveCount -= 1;
		if (0 == _omrThread->exclusiveCount) {
			omrthread_monitor_exit(_env->getOmrVM()->_vmThreadListMutex);
		}
	}

	/**
	 * This method may be required if a concurrent GC strategy is employed. It will
	 * be called by the concurrent GC if it is unable to immediately obtain exclusive
	 * VM access (because a stop-the-world GC is in progress). If the concurrent GC
	 * holds resources that must be temporarily relinquished while waiting for
	 * exclusive VM access, this is the place to release them. They can be recovered
	 * in exclusiveAccessForGCObtainedAfterBeatenByOtherThread(), which will be called
	 * when the stop-the-world GC completes and releases exclusive VM access.
	 */
	virtual void exclusiveAccessForGCBeatenByOtherThread() {}

	/**
	 * This method may be required if a concurrent GC strategy is employed. It will be
	 * called when the stop-the-world GC completes and releases exclusive VM access if
	 * the thread previously ran exclusiveAccessForGCObtainedAfterBeatenByOtherThread().
	 * Any resources temporarily released in that method can be recovered here before
	 * proceeding with exclusive VM access.
	 */
	virtual void exclusiveAccessForGCObtainedAfterBeatenByOtherThread() {}

	virtual void releaseCriticalHeapAccess(uintptr_t *data) {}
	virtual void reacquireCriticalHeapAccess(uintptr_t data) {}

	/**
	 * Give up exclusive access in preparation for transferring it to a collaborating thread
	 * (i.e. main-to-master or master-to-main). Implement if this kind of collaboration is
	 * required.
	 *
	 * @return the exclusive count of the current thread before relinquishing 
	 */
	virtual uintptr_t relinquishExclusiveVMAccess()
	{
		Assert_MM_unreachable();
		return 0;
	}
	
	/**
	 * Assume exclusive access from a collaborating thread (i.e. main-to-master or master-to-main).
	 * Implement if this kind of collaboration is required.
	 *
	 * @param exclusiveCount the exclusive count to be restored 
	 */
	virtual void assumeExclusiveVMAccess(uintptr_t exclusiveCount)
	{
		Assert_MM_unreachable();
	}

	/**
	 * Checks to see if any thread has requested exclusive access
	 * @return true if a thread is waiting on exclusive access, false if not.
	 */
	virtual bool isExclusiveAccessRequestWaiting() = 0;


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
