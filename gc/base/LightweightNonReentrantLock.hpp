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

#if !defined(LIGHTWEIGHT_NON_REENTRANT_LOCK_HPP_)
#define LIGHTWEIGHT_NON_REENTRANT_LOCK_HPP_

#include "omrcfg.h"
#include "gcspinlock.h"
#include "omrcomp.h"
#include "omrmutex.h"
#include "modronbase.h"

#include "BaseNonVirtual.hpp"

#define MAX_LWNR_LOCK_NAME_SIZE 256

class MM_GCExtensionsBase;
class MM_EnvironmentBase;
struct ModronLnrlOptions;

/** 
 * Synchronize concurrent activity of threads.
 * Locks allow deterministic behaviour for concurrent running threads.  By 
 * synchronizing access to OS resources and sensitive data regions threads 
 * guarantee predictable behaviour.  
 * 
 * These locks are not re-entrant
 */
class MM_LightweightNonReentrantLock : public MM_BaseNonVirtual
{
private:
	bool _initialized; /**< initialized state */
	char _nameBuf[MAX_LWNR_LOCK_NAME_SIZE]; /* LWNR lock name */
	J9ThreadMonitorTracing *_tracing; /**< lock statistics */
	MM_GCExtensionsBase *_extensions; /**< cache extensions for use in teardown() */

#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)
	J9GCSpinlock _spinlock;
#else /* J9MODRON_USE_CUSTOM_SPINLOCKS */
	MUTEX _mutex;
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */

protected:

public:

private:

protected:

public:
	bool initialize(MM_EnvironmentBase *env, ModronLnrlOptions *options, const char * name);
	void tearDown() ;

	/**
	 * Acquire the lock.
	 * A thread may enter a lock'ed region only once.
	 * The thread will spin waiting for the lock to become free
	 * if it is currently in use.
	 * 
	 * @return TRUE on success
	 * @note Creates a load/store barrier.
	 */
	MMINLINE bool acquire() 
	{
#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)
		omrgc_spinlock_acquire(&_spinlock, _tracing);
#else /* J9MODRON_USE_CUSTOM_SPINLOCKS */
		MUTEX_ENTER(_mutex);
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */
		return true;
	};

	/**
	 * Release the lock.
	 * If the current thread is not the owner of the lock, the
	 * mutex is unaffected, and an error is returned.
	 * 
	 * @return TRUE on success
	 * @note Creates a store barrier.
	 */
	MMINLINE bool release() 
	{
#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)
		omrgc_spinlock_release(&_spinlock);
#else /* J9MODRON_USE_CUSTOM_SPINLOCKS */
		MUTEX_EXIT(_mutex);
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */
		return true;
	};

	MM_LightweightNonReentrantLock() : 
		MM_BaseNonVirtual(),
		_initialized(false),
		_tracing(NULL),
		_extensions(NULL)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* LIGHTWEIGHT_NON_REENTRANT_LOCK_HPP_ */
