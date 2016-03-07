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
 ******************************************************************************/

#if !defined(LOCK_HPP_)
#define LOCK_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "modronbase.h"

#include "BaseNonVirtual.hpp"

/**
 * Synchronize concurrent activity of threads.
 * 
 * Locks allow deterministic behaviour for concurrent running threads.  By 
 * synchronizing access to OS resources and sensitive data regions threads 
 * guarantee predictable behaviour.  Threads that wait on locks will release
 * OS resources
 * 
 * Locks are re-entrant, allowing threads to obtain the same lock multiple
 * times.  The thread must release the lock the same number of times it obtains
 * the lock.
 * 
 * @ingroup GC_Base_Core
 */
class MM_Lock : public MM_BaseNonVirtual
{
	omrthread_monitor_t _monitor;

public:
	MM_Lock() :
		MM_BaseNonVirtual()
	{
		_typeId = __FUNCTION__;
	};

	/**
	 * Initialize a new lock object.
	 * A lock must be initialized before it may be used.
	 * 
	 * @param flags should be null, passed to thread library
	 * @return 0 on success -1 on failure
	 */
	MMINLINE intptr_t initialize(uintptr_t flags, char *name) 
	{ 
		return omrthread_monitor_init_with_name(&_monitor, flags, name); 
	};

	/**
	 * Enter the lock'ed region.
	 * A thread may enter a lock it owns multiple times, but must
	 * exit the lock the same number of times before other threads
	 * waiting on the lock are permitted to continue.
	 * 
	 * @return 0 on success
	 */
	MMINLINE intptr_t enter()
	{
		return omrthread_monitor_enter(_monitor);
	};

	/**
	 * Exit the lock'ed region. 
	 * If the current thread is not the owner of the lock, the
	 * mutex is unaffected, and an error is returned.
	 * 
	 * @return 0 on success
	 */
	MMINLINE intptr_t exit()
	{
		return omrthread_monitor_exit(_monitor);
	};

	/**
	 * Wait on the lock. 
	 * Release the lock <i>n</i> times and waits for a signal.  Once a signal occurs or
	 * the thread is interurpted, the lock is reobtained <i>n</i> times.
	 * 
	 * @return 0 on success
	 */
	MMINLINE intptr_t wait()
	{
		return omrthread_monitor_wait(_monitor);
	};

	/**
	 * Signal a thread waiting on the lock. 
	 * If any threads are waiting on this lock, one of them is signaled
	 * that the lock is now free.  A thread is considered to be waiting
	 * on the lock if it is currently blocked while executing #wait.
	 * If no threads are waiting this method does nothing
	 * 
	 * @return 0 once the thread has been signalled
	 */
	MMINLINE intptr_t notify()
	{
		return omrthread_monitor_notify(_monitor);
	};

	/**
	 * Signal all threads waiting on the lock.
	 * If any threads are waiting on this lock, all of them are signaled
	 * that the lock is now free.  A thread is considered to be waiting
	 * on the lock if it is currently blocked while executing #wait.
	 * If no threads are waiting this method does nothing
	 * 
	 * @return 0 once the threads have been signalled
	 */
	MMINLINE intptr_t notifyAll()
	{
		return omrthread_monitor_notify_all(_monitor);
	};

	/**
	 * Discards a lock object.
	 * A lock must be discarded to free the resources associated with it.
	 * 
	 * @note A lock must not be destroyed if threads are waiting on it, or 
	 * if it is currently owned.
	 * 
	 * @return 0 on success -1 on failure
	 */
	MMINLINE intptr_t tearDown()
	{
		return omrthread_monitor_destroy(_monitor);
	};
};

#endif /* LOCK_HPP_ */
