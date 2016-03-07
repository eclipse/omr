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

#if !defined(LIGHTWEIGHT_NONREENTRANT_READER_WRITER_LOCK_HPP_)
#define LIGHTWEIGHT_NONREENTRANT_READER_WRITER_LOCK_HPP_

#include "omrcomp.h"

#include "modronbase.h"
#include "BaseNonVirtual.hpp"

#define LWRW_OK				0
#define LWRW_FAILED_INIT 	-1
#define LWRW_READER_MODE 						((uint32_t)1)			/**< bit0 of lock-word is lock mode, :0=Reader Mode, :1=Writer Mode */
#define LWRW_WRITER_MODE 						((uint32_t)0)
#define LWRW_INCREMENTAL_BASE_READERS			((uint32_t)2)			/**< bit15-bit1 is shared reader count, max reader count is 32767, can be extended to bit16-bit30 in order supporting more readers, need to match the change of LWRW_INCREMENTAL_BASE_WAITINGWRITERS, LWRW_MASK_WAITINGWRITERS and LWRW_MASK_READERS */
#define LWRW_INCREMENTAL_BASE_WAITINGWRITERS	((uint32_t)(1<<16))		/**< bit31-bit16 is waiting writer count,  */
#define LWRW_MASK_WAITINGWRITERS				(LWRW_INCREMENTAL_BASE_WAITINGWRITERS-1)
#define LWRW_MASK_READERS						(LWRW_MASK_WAITINGWRITERS^((uint32_t)0xFFFFFFFF))
#define LWRW_MASK_MODE							(LWRW_READER_MODE^((uint32_t)0xFFFFFFFF))

/**
 * Lightweight NonReentrant Readers-writer lock is simplified version rwlock,
 * using atomic access to lockword for improving the performance.
 * it is writer priority lock(write locks have priority over reads, no priority order between waiting writers)
 * and support multiple concurrent writers case.
 *
 * The lock is not re-entrant, can be extended to support re-entrant in future
 * it would not provide realtime locking.
 */
class MM_LightweightNonReentrantReaderWriterLock : public MM_BaseNonVirtual {
private:
	uintptr_t _spinCount;

#if defined(J9MODRON_USE_CUSTOM_READERWRITERLOCK)
	volatile uint32_t _status;
#else /* defined(J9MODRON_USE_CUSTOM_READERWRITERLOCK) */
	omrthread_rwmutex_t _rwmutex;
#endif /* defined(J9MODRON_USE_CUSTOM_READERWRITERLOCK) */
protected:
public:
	MM_LightweightNonReentrantReaderWriterLock() :
		MM_BaseNonVirtual()
		,_spinCount(1)
#if defined(J9MODRON_USE_CUSTOM_READERWRITERLOCK)
		,_status(LWRW_READER_MODE)
#else /* defined(J9MODRON_USE_CUSTOM_READERWRITERLOCK) */
		,_rwmutex(NULL)
#endif /* defined(J9MODRON_USE_CUSTOM_READERWRITERLOCK) */

	{
		_typeId = __FUNCTION__;
	};

	/**
	 * Initialize a new lock object.
	 * A lock must be initialized before it may be used.
	 *
	 * @parm[in] env the environment for the current thread
	 * @param[in] spinCount loop count between re-try the atomic
	 */
	intptr_t initialize(uintptr_t spinCount);

	/**
	 * Discards a lock object.
	 * A lock must be discarded to free the resources associated with it.
	 *
	 *
	 * @note A lock must not be destroyed if threads are waiting on it, or
	 * if it is currently owned.
	 */
	intptr_t tearDown();
	/**
	 * Enter lock as a reader.
	 *
	 * A thread can not re-enter it owns multiple times.
	 *
	 * @return 0 on success
	 * @note Creates a load/store barrier.
	 */

	intptr_t enterRead();
	/**
	 * Exit the lock as a reader
	 * the current thread has to be the owner of the reader lock
	 *
	 * @return 0 on success
	 * @note Creates a load barrier.
	 */
	intptr_t exitRead();

	/**
	 * Enter lock as a writer.
	 *
	 * A thread can not re-enter it owns multiple times.
	 *
	 * @return 0 on success
	 */

	intptr_t enterWrite();

	/**
	 * Exit the lock as a writer
	 * the current thread has to be the owner of the writer lock
	 *
	 * @return 0 on success
	 * @note Creates a store barrier.
	 */
	intptr_t exitWrite();
};

#endif /* LIGHTWEIGHT_NONREENTRANT_READER_WRITER_LOCK_HPP_ */
