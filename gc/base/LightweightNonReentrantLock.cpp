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

#include "omrcfg.h"
#include "omr.h"
#include "omrcomp.h"
#include "omrmutex.h"
#include "modronbase.h"

#include "LightweightNonReentrantLock.hpp"
#include "pool_api.h"

#include "GCExtensionsBase.hpp"
#include "EnvironmentBase.hpp"

/**
 * Initialize a new lock object.
 * A lock must be initialized before it may be used.
 *
 * @param env
 * @param options
 * @param name Lock name
 * @return TRUE on success
 * @note Creates a store barrier.
 */
bool
MM_LightweightNonReentrantLock::initialize(MM_EnvironmentBase *env, ModronLnrlOptions *options, const char * name)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	/* initialize variables in case constructor was not called */
	_initialized = false;
	_tracing = NULL;
	_extensions = env->getExtensions();

	if (NULL != _extensions) {
		J9Pool* tracingPool = _extensions->_lightweightNonReentrantLockPool;
		if (NULL != tracingPool) {
			omrthread_monitor_enter(_extensions->_lightweightNonReentrantLockPoolMutex);
			_tracing = (J9ThreadMonitorTracing *)pool_newElement(tracingPool);
			omrthread_monitor_exit(_extensions->_lightweightNonReentrantLockPoolMutex);

			if (NULL == _tracing) {
				goto error_no_memory;
			}
			_tracing->monitor_name = NULL;

			if (NULL != name) {
				uintptr_t length = omrstr_printf(NULL, 0, "[%p] %s", this, name) + 1;
				if (length > MAX_LWNR_LOCK_NAME_SIZE) {
					goto error_no_memory;
				}
				_tracing->monitor_name = _nameBuf;
				if (NULL == _tracing->monitor_name) {
					goto error_no_memory;
				}
				omrstr_printf(_tracing->monitor_name, length, "[%p] %s", this, name);
			}
		}
	}

#if defined(OMR_ENV_DATA64)
	if(0 != (((uintptr_t)this) % sizeof(uintptr_t))) {
		omrtty_printf("GC FATAL: LWNRL misaligned.\n");
		abort();
	}
#endif

#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)

	_initialized = omrgc_spinlock_init(&_spinlock) ? false : true;

	_spinlock.spinCount1 = options->spinCount1;
	_spinlock.spinCount2 = options->spinCount2;
	_spinlock.spinCount3 = options->spinCount3;
#else /* J9MODRON_USE_CUSTOM_SPINLOCKS */
	_initialized = MUTEX_INIT(_mutex) ? true : false;
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */

	return _initialized;

error_no_memory:
	return false;
};

/**
 * Discards a lock object.
 * An lock must be discarded to free the resources associated with it.
 * 
 * @note  A lock must not be destroyed if threads are waiting on it, or 
 * if it is currently owned.
 */
void
MM_LightweightNonReentrantLock::tearDown()
{
	if(NULL != _extensions) {
		if(NULL != _tracing) {
			if (NULL != _tracing->monitor_name) {
				_tracing->monitor_name = NULL;
			}

			J9Pool* tracingPool = _extensions->_lightweightNonReentrantLockPool;
			if(NULL != tracingPool) {
				omrthread_monitor_enter(_extensions->_lightweightNonReentrantLockPoolMutex);
				pool_removeElement(tracingPool, _tracing);
				omrthread_monitor_exit(_extensions->_lightweightNonReentrantLockPoolMutex);
			}
			_tracing = NULL;
		}
	}

	if (_initialized) {
#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)
		omrgc_spinlock_destroy(&_spinlock);
#else /* J9MODRON_USE_CUSTOM_SPINLOCKS */
		MUTEX_DESTROY(_mutex);
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */
		_initialized = false;
	}
}

