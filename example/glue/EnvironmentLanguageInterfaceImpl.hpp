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

#include "ModronAssertions.h"
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
		OMR_VM *omrVM = _env->getOmrVM();
		OMR_VM_Example *exampleVM = (OMR_VM_Example *)omrVM->_language_vm;

		/* unconditionally acquire exclusive VM access by locking the VM thread list mutex */
		MM_AtomicOperations::add(&exampleVM->_vmExclusiveAccessCount, 1);
		MM_AtomicOperations::readBarrier();
		omrthread_rwmutex_enter_write(exampleVM->_vmAccessMutex);
		omrthread_monitor_enter(omrVM->_vmThreadListMutex);
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
		OMR_VM *omrVM = _env->getOmrVM();
		OMR_VM_Example *exampleVM = (OMR_VM_Example *)omrVM->_language_vm;

		/* try to acquire exclusive VM access by locking the VM thread list mutex */
		uintptr_t vmExclusiveAccessCount = MM_AtomicOperations::add(&exampleVM->_vmExclusiveAccessCount, 1);
		MM_AtomicOperations::readBarrier();
		if ((1 == vmExclusiveAccessCount) && (0 == omrthread_rwmutex_try_enter_write(exampleVM->_vmAccessMutex))) {
			omrthread_monitor_enter(omrVM->_vmThreadListMutex);
			return true;
		}

		/* failed to acquire exclusive VM access */
		Assert_MM_true(0 < exampleVM->_vmExclusiveAccessCount);
		MM_AtomicOperations::subtract(&exampleVM->_vmExclusiveAccessCount, 1);
		MM_AtomicOperations::readBarrier();
		return false;
	}

	/**
	 * Releases exclusive VM access.
	 */
	virtual void
	releaseExclusiveVMAccess()
	{
		OMR_VM_Example *exampleVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
		omrthread_monitor_exit(_env->getOmrVM()->_vmThreadListMutex);
		omrthread_rwmutex_exit_write(exampleVM->_vmAccessMutex);
		Assert_MM_true(0 < exampleVM->_vmExclusiveAccessCount);
		MM_AtomicOperations::subtract(&exampleVM->_vmExclusiveAccessCount, 1);
		MM_AtomicOperations::readBarrier();
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

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	virtual void disableInlineTLHAllocate();
	virtual void enableInlineTLHAllocate();
	virtual bool isInlineTLHAllocateEnabled();
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void parallelMarkTask_setup(MM_EnvironmentBase *env);
	virtual void parallelMarkTask_cleanup(MM_EnvironmentBase *env);
};

#endif /* ENVIRONMENTLANGUAGEINTERFACEIMPL_HPP_ */
