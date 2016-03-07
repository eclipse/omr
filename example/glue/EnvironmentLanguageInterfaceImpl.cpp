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

#include "Collector.hpp"
#include "EnvironmentLanguageInterfaceImpl.hpp"
#include "EnvironmentStandard.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"


MM_EnvironmentLanguageInterfaceImpl::MM_EnvironmentLanguageInterfaceImpl(MM_EnvironmentBase *env)
	: MM_EnvironmentLanguageInterface(env)
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

MM_EnvironmentLanguageInterfaceImpl *
MM_EnvironmentLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_EnvironmentLanguageInterfaceImpl *eliJava = NULL;

	eliJava = (MM_EnvironmentLanguageInterfaceImpl *)env->getForge()->allocate(sizeof(MM_EnvironmentLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != eliJava) {
		new(eliJava) MM_EnvironmentLanguageInterfaceImpl(env);
		if (!eliJava->initialize(env)) {
        	eliJava->kill(env);
        	eliJava = NULL;
		}
	}

	return eliJava;
}

void
MM_EnvironmentLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_EnvironmentLanguageInterfaceImpl::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_EnvironmentLanguageInterfaceImpl::tearDown(MM_EnvironmentBase *env)
{
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
bool
MM_EnvironmentLanguageInterfaceImpl::tryAcquireExclusiveVMAccess()
{
	if (0 == _exclusiveCount) {
		_env->getOmrVMThread()->exclusiveCount = 1;
		reportExclusiveAccessAcquire();
	} else {
		_exclusiveCount += 1;
	}
	return true;
}

/**
 * Checks to see if the thread has exclusive access
 * @return true if the thread has exclusive access, false if not.
 */
bool
MM_EnvironmentLanguageInterfaceImpl::inquireExclusiveVMAccessForGC()
{
	return (_exclusiveCount > 0);
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
bool
MM_EnvironmentLanguageInterfaceImpl::tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats)
{
	Assert_MM_unreachable();
	return true;
}

void
MM_EnvironmentLanguageInterfaceImpl::releaseExclusiveForConcurrentKickoff()
{
	Assert_MM_unreachable();
	return;
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

bool
MM_EnvironmentLanguageInterfaceImpl::tryAcquireExclusiveVMAccessForGC(MM_Collector *collector)
{
	if (0 == _exclusiveCount) {
		_env->getOmrVMThread()->exclusiveCount = 1;
		reportExclusiveAccessAcquire();
	} else {
		_exclusiveCount += 1;
	}
	return true;
}

bool
MM_EnvironmentLanguageInterfaceImpl::acquireExclusiveVMAccessForGC(MM_Collector *collector)
{
	if (0 == _exclusiveCount) {
		_env->getOmrVMThread()->exclusiveCount = 1;
		reportExclusiveAccessAcquire();
	} else {
		_exclusiveCount += 1;
	}
	return true;
}

void
MM_EnvironmentLanguageInterfaceImpl::releaseExclusiveVMAccessForGC()
{
	_exclusiveCount -= 1;
	if (0 == _exclusiveCount) {
		_env->getOmrVMThread()->exclusiveCount = 0;
		reportExclusiveAccessRelease();
	}
}

void
MM_EnvironmentLanguageInterfaceImpl::unwindExclusiveVMAccessForGC()
{
	if (_exclusiveCount > 0) {
		_env->getOmrVMThread()->exclusiveCount = 0;
		reportExclusiveAccessRelease();
	}
}

/**
 * Aquires exclusive VM access
 */
void
MM_EnvironmentLanguageInterfaceImpl::acquireExclusiveVMAccess()
{
	if (0 == _exclusiveCount) {
		_env->getOmrVMThread()->exclusiveCount = 1;
		reportExclusiveAccessAcquire();
	} else {
		_exclusiveCount += 1;
	}
}

/**
 * Releases exclusive VM access.
 */
void
MM_EnvironmentLanguageInterfaceImpl::releaseExclusiveVMAccess()
{
	_exclusiveCount -= 1;
	if (0 == _exclusiveCount) {
		_env->getOmrVMThread()->exclusiveCount = 0;
		reportExclusiveAccessRelease();
	}
}

bool
MM_EnvironmentLanguageInterfaceImpl::saveObjects(omrobjectptr_t objectPtr)
{
	return true;
}

void
MM_EnvironmentLanguageInterfaceImpl::restoreObjects(omrobjectptr_t *objectPtrIndirect)
{
}

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Disable inline TLH allocates by hiding the real heap allocation address from
 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
 * looks full.
 *
 */
void
MM_EnvironmentLanguageInterfaceImpl::disableInlineTLHAllocate()
{
}

/**
 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
 */
void
MM_EnvironmentLanguageInterfaceImpl::enableInlineTLHAllocate()
{
}

/**
 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otheriwse
 */
bool
MM_EnvironmentLanguageInterfaceImpl::isInlineTLHAllocateEnabled()
{
	return false;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

void
MM_EnvironmentLanguageInterfaceImpl::parallelMarkTask_setup(MM_EnvironmentBase *env)
{
}

void
MM_EnvironmentLanguageInterfaceImpl::parallelMarkTask_cleanup(MM_EnvironmentBase *envBase)
{
}
