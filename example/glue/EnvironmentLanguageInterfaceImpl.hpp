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

#ifndef ENVIRONMENTLANGUAGEINTERFACEIMPL_HPP_
#define ENVIRONMENTLANGUAGEINTERFACEIMPL_HPP_

#include "omr/gc/modronAssertions.h"
#include "omr.h"

#include "EnvironmentLanguageInterface.hpp"

#include "EnvironmentBase.hpp"
#include "omrExampleVM.hpp"

class MM_EnvironmentLanguageInterfaceImpl : public MM_EnvironmentLanguageInterface
{
private:
protected:
public:

private:
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_EnvironmentLanguageInterfaceImpl(MM_EnvironmentBase *env);

public:
	static MM_EnvironmentLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	static MM_EnvironmentLanguageInterfaceImpl *getInterface(MM_EnvironmentLanguageInterface *linterface) { return (MM_EnvironmentLanguageInterfaceImpl *)linterface; }

	virtual bool saveObjects(omrobjectptr_t objectPtr);
	virtual void restoreObjects(omrobjectptr_t *objectPtrIndirect);

	/**
	 * Acquire shared VM access.
	 */
	virtual void
	acquireVMAccess()
	{
		OMR_VM_Example *exampleVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
		omrthread_rwmutex_enter_read(exampleVM->_vmAccessMutex);
	}

	/**
	 * Releases shared VM access.
	 */
	virtual void
	releaseVMAccess()
	{
		OMR_VM_Example *exampleVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
		omrthread_rwmutex_exit_read(exampleVM->_vmAccessMutex);
	}

	/**
	 * Acquire exclusive VM access.
	 */
	virtual void
	acquireExclusiveVMAccess()
	{
		if (0 == _omrThread->exclusiveCount) {
			OMR_VM *omrVM = _env->getOmrVM();
			OMR_VM_Example *exampleVM = (OMR_VM_Example *)omrVM->_language_vm;

			/* tell the rest of the world that a thread is going for exclusive VM< access */
			MM_AtomicOperations::add(&exampleVM->_vmExclusiveAccessCount, 1);
			
			/* unconditionally acquire exclusive VM access by locking the VM thread list mutex */
			omrthread_rwmutex_enter_write(exampleVM->_vmAccessMutex);
			omrthread_monitor_enter(omrVM->_vmThreadListMutex);
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
		if (0 == _omrThread->exclusiveCount) {
			OMR_VM *omrVM = _env->getOmrVM();
			OMR_VM_Example *exampleVM = (OMR_VM_Example *)omrVM->_language_vm;

			/* tell the rest of the world that a thread is going for exclusive VM< access */
			uintptr_t vmExclusiveAccessCount = MM_AtomicOperations::add(&exampleVM->_vmExclusiveAccessCount, 1);
			
			/* try to acquire exclusive VM access by locking the VM thread list mutex */
			if ((1 != vmExclusiveAccessCount) || (0 != omrthread_rwmutex_try_enter_write(exampleVM->_vmAccessMutex))) {
				/* failed to acquire exclusive VM access */
				Assert_MM_true(0 < exampleVM->_vmExclusiveAccessCount);
				MM_AtomicOperations::subtract(&exampleVM->_vmExclusiveAccessCount, 1);
				return false;
			}
			omrthread_monitor_enter(omrVM->_vmThreadListMutex);
		}
		_omrThread->exclusiveCount += 1;
		return true;
	}

	/**
	 * Releases exclusive VM access.
	 */
	virtual void
	releaseExclusiveVMAccess()
	{
		if (1 == _omrThread->exclusiveCount) {
			OMR_VM_Example *exampleVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
			omrthread_monitor_exit(_env->getOmrVM()->_vmThreadListMutex);
			omrthread_rwmutex_exit_write(exampleVM->_vmAccessMutex);
			Assert_MM_true(0 < exampleVM->_vmExclusiveAccessCount);
			MM_AtomicOperations::subtract(&exampleVM->_vmExclusiveAccessCount, 1);
			_omrThread->exclusiveCount -= 1;
		} else if (1 < _omrThread->exclusiveCount) {
			_omrThread->exclusiveCount -= 1;
		}
	}

	virtual bool
	isExclusiveAccessRequestWaiting()
	{
		OMR_VM_Example *exampleVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
		if ((0 < exampleVM->_vmExclusiveAccessCount) || omrthread_rwmutex_is_writelocked(exampleVM->_vmAccessMutex)) {
			return true;
		}
		if (NULL != _env->getExtensions()->gcExclusiveAccessThreadId) {
			return true;
		}
		return false;
	}

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
	virtual uintptr_t
	relinquishExclusiveVMAccess()
	{
		uintptr_t relinquishedExclusiveCount = _omrThread->exclusiveCount;
		_omrThread->exclusiveCount = 0;
		return relinquishedExclusiveCount;
	}

	/**
	 * Assume exclusive access from a collaborating thread (i.e. main-to-master or master-to-main).
	 * Implement if this kind of collaboration is required.
	 *
	 * @param exclusiveCount the exclusive count to be restored
	 * @see relinquishExclusiveVMAccess(uintptr_t)
	 */
	virtual void
	assumeExclusiveVMAccess(uintptr_t exclusiveCount)
	{
		_omrThread->exclusiveCount = exclusiveCount;
	}

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	virtual void disableInlineTLHAllocate();
	virtual void enableInlineTLHAllocate();
	virtual bool isInlineTLHAllocateEnabled();
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void parallelMarkTask_setup(MM_EnvironmentBase *env);
	virtual void parallelMarkTask_cleanup(MM_EnvironmentBase *env);
};

#endif /* ENVIRONMENTLANGUAGEINTERFACEIMPL_HPP_ */
