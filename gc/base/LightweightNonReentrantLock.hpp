/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
