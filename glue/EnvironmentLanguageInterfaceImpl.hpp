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

#include "omr.h"

#include "EnvironmentLanguageInterface.hpp"

#include "EnvironmentBase.hpp"

class MM_EnvironmentLanguageInterfaceImpl : public MM_EnvironmentLanguageInterface
{
private:
	OMRPortLibrary *_portLibrary;
	MM_EnvironmentBase *_env;  /**< Associated Environment */
	uintptr_t _exclusiveCount; /**< count of recursive exclusive VM access requests */
	uint64_t _exclusiveAccessTime; /**< time (in ticks) of the last exclusive access request */
	uint64_t _meanExclusiveAccessIdleTime; /**< mean idle time (in ticks) of the last exclusive access request */
	OMR_VMThread* _lastExclusiveAccessResponder; /**< last thread to respond to last exclusive access request */
	uintptr_t _exclusiveAccessHaltedThreads; /**< number of threads halted by last exclusive access request */
	bool _exclusiveAccessBeatenByOtherThread; /**< true if last exclusive access request had to wait for another GC thread */
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

	virtual void acquireVMAccess(MM_EnvironmentBase *env);
	virtual void releaseVMAccess(MM_EnvironmentBase *env);

	virtual bool tryAcquireExclusiveVMAccess();
	virtual void acquireExclusiveVMAccess();
	virtual void releaseExclusiveVMAccess();

	virtual bool acquireExclusiveVMAccessForGC(MM_Collector *collector);
	virtual void releaseExclusiveVMAccessForGC();
	virtual bool tryAcquireExclusiveVMAccessForGC(MM_Collector *collector);
	virtual bool inquireExclusiveVMAccessForGC();
	virtual void unwindExclusiveVMAccessForGC();

	virtual uint64_t getExclusiveAccessTime() { return _exclusiveAccessTime; }
	virtual uint64_t getMeanExclusiveAccessIdleTime() { return _meanExclusiveAccessIdleTime; }
	virtual OMR_VMThread* getLastExclusiveAccessResponder() { return _lastExclusiveAccessResponder; }
	virtual uintptr_t getExclusiveAccessHaltedThreads() { return _exclusiveAccessHaltedThreads; }
	virtual bool exclusiveAccessBeatenByOtherThread() { return _exclusiveAccessBeatenByOtherThread; }

	virtual bool isExclusiveAccessRequestWaiting() { return false; }

	virtual bool saveObjects(omrobjectptr_t objectPtr);
	virtual void restoreObjects(omrobjectptr_t *objectPtrIndirect);

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	virtual void disableInlineTLHAllocate();
	virtual void enableInlineTLHAllocate();
	virtual bool isInlineTLHAllocateEnabled();
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void parallelMarkTask_setup(MM_EnvironmentBase *env);
	virtual void parallelMarkTask_cleanup(MM_EnvironmentBase *env);

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual bool tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats);
	virtual void releaseExclusiveForConcurrentKickoff();
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
};

#endif /* ENVIRONMENTLANGUAGEINTERFACEIMPL_HPP_ */
