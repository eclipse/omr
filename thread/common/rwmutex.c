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

#include <stdio.h>
#include <stdlib.h>
#include "threaddef.h"
#include "thread_internal.h"

#undef  ASSERT
#define ASSERT(x) /**/

typedef struct RWMutex {
	omrthread_monitor_t syncMon;
	intptr_t status;
	omrthread_t writer;
} RWMutex;

#define ASSERT_RWMUTEX(m)\
    ASSERT((m));\
    ASSERT((m)->syncMon);

#define RWMUTEX_STATUS_IDLE(m)     ((m)->status == 0)
#define RWMUTEX_STATUS_READING(m)  ((m)->status > 0)
#define RWMUTEX_STATUS_WRITING(m)  ((m)->status < 0)

/**
 * Acquire and initialize a new read/write mutex from the threading library.
 *
 * @param[out] handle pointer to a omrthread_rwmutex_t to be set to point to the new mutex
 * @param[in] flags initial flag values for the mutex
 * @return J9THREAD_RWMUTEX_OK on success
 *
 * @see omrthread_rwmutex_destroy
 */
intptr_t
omrthread_rwmutex_init(omrthread_rwmutex_t *handle, uintptr_t flags, const char *name)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	intptr_t ret = J9THREAD_RWMUTEX_OK;
	RWMutex *mutex = NULL;

#if defined(OMR_THR_FORK_SUPPORT)
	ASSERT(0 != lib->rwmutexPool);
	GLOBAL_LOCK_SIMPLE(lib);
	mutex = (RWMutex *)pool_newElement(lib->rwmutexPool);
	GLOBAL_UNLOCK_SIMPLE(lib);
#else /* defined(OMR_THR_FORK_SUPPORT) */
	mutex = (RWMutex *)omrthread_allocate_memory(lib, sizeof(RWMutex), OMRMEM_CATEGORY_THREADS);
#endif /* defined(OMR_THR_FORK_SUPPORT) */
	if (NULL == mutex) {
		ret = J9THREAD_RWMUTEX_FAIL;
	} else {
		omrthread_monitor_init_with_name(&mutex->syncMon, 0, (char *)name);
		mutex->status = 0;
		mutex->writer = 0;

		ASSERT(handle);
		*handle = mutex;
	}

	return ret;
}

/**
 * Destroy a read/write mutex.
 *
 * Destroying a mutex frees the internal resources associated
 * with it.
 *
 * @note A mutex must NOT be destroyed if it is owned
 * by any threads for either read or write access.
 *
 * @param[in] mutex a mutex to be destroyed
 * @return J9THREAD_RWMUTEX_OK on success
 *
 * @see omrthread_rwmutex_init
 */
intptr_t
omrthread_rwmutex_destroy(omrthread_rwmutex_t mutex)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	ASSERT(mutex);
	ASSERT(mutex->syncMon);
	ASSERT(0 == mutex->status);
	ASSERT(0 == mutex->writer);
	omrthread_monitor_destroy(mutex->syncMon);
#if defined(OMR_THR_FORK_SUPPORT)
	ASSERT(0 != lib->rwmutexPool);
	GLOBAL_LOCK_SIMPLE(lib);
	pool_removeElement(lib->rwmutexPool, mutex);
	GLOBAL_UNLOCK_SIMPLE(lib);
#else /* defined(OMR_THR_FORK_SUPPORT) */
	omrthread_free_memory(lib, mutex);
#endif /* defined(OMR_THR_FORK_SUPPORT) */
	return J9THREAD_RWMUTEX_OK;
}

/**
 * Enter a read/write mutex as a reader.
 *
 * A thread may re-enter a mutex it owns multiple times, but
 * must exit the same number of times as a reader
 * using omrthread_rwmutex_exit_read.
 *
 * A thread with writer access can enter a monitor
 * with reader access, but must exit the mutex in the
 * opposite order.
 *
 * e.g. The following is acceptable
 * omrthread_rwmutex_enter_write(mutex);
 * omrthread_rwmutex_enter_read(mutex);
 * omrthread_rwmutex_exit_read(mutex);
 * omrthread_rwmutex_exit_write(mutex);
 *
 * However, a thread with read access MUST NOT
 * ask for write access on the same mutex.
 *
 * @param[in] mutex a mutex to be entered for read access
 * @return J9THREAD_RWMUTEX_OK on success
 *
 * @see omrthread_rwmutex_exit_read
 */
intptr_t
omrthread_rwmutex_enter_read(omrthread_rwmutex_t mutex)
{
	ASSERT_RWMUTEX(mutex);
	if (mutex->writer == omrthread_self()) {
		return J9THREAD_RWMUTEX_OK;
	}

	omrthread_monitor_enter(mutex->syncMon);

	while (mutex->status < 0) {
		omrthread_monitor_wait(mutex->syncMon);
	}
	mutex->status++;

	omrthread_monitor_exit(mutex->syncMon);
	return J9THREAD_RWMUTEX_OK;
}

/**
 * Exit a read/write mutex as a reader.
 *
 * @param[in] mutex a mutex to be exited
 * @return J9THREAD_RWMUTEX_OK on success
 *
 * @see omrthread_rwmutex_enter_read
 *
 */
intptr_t
omrthread_rwmutex_exit_read(omrthread_rwmutex_t mutex)
{
	ASSERT_RWMUTEX(mutex);
	if (mutex->writer == omrthread_self()) {
		return J9THREAD_RWMUTEX_OK;
	}

	omrthread_monitor_enter(mutex->syncMon);

	mutex->status--;
	if (0 == mutex->status) {
		omrthread_monitor_notify(mutex->syncMon);
	}

	omrthread_monitor_exit(mutex->syncMon);

	return J9THREAD_RWMUTEX_OK;
}

/**
 * Enter a read/write mutex as a writer.
 *
 * A thread may re-enter a mutex it owns multiple times, but
 * must exit the same number of times as a writer
 * using omrthread_rwmutex_exit_write.
 *
 * A thread with writer access can enter a monitor
 * with reader access, but must exit the mutex in the
 * opposite order.
 *
 * e.g. The following is acceptable
 * omrthread_rwmutex_enter_write(mutex);
 * omrthread_rwmutex_enter_read(mutex);
 * omrthread_rwmutex_exit_read(mutex);
 * omrthread_rwmutex_exit_write(mutex);
 *
 * However, a thread with read access MUST NOT
 * ask for write access on the same mutex.
 *
 * @param[in] mutex a mutex to be entered for write access
 * @return J9THREAD_RWMUTEX_OK on success
 *
 * @see omrthread_rwmutex_exit_write
 */
intptr_t
omrthread_rwmutex_enter_write(omrthread_rwmutex_t mutex)
{
	omrthread_t self = omrthread_self();
	ASSERT_RWMUTEX(mutex);

	/* recursive? */
	if (mutex->writer == self) {
		mutex->status--;
		return J9THREAD_RWMUTEX_OK;
	}

	omrthread_monitor_enter(mutex->syncMon);

	while (mutex->status != 0) {
		omrthread_monitor_wait(mutex->syncMon);
	}
	mutex->status--;
	mutex->writer = self;

	ASSERT(RWMUTEX_STATUS_WRITING(mutex));

	omrthread_monitor_exit(mutex->syncMon);

	return J9THREAD_RWMUTEX_OK;
}

/**
 * This method allows you to try to enter the mutex for write and instead
 * of blocking return an error code if you would have blocked
 *
 * @param mutex the mutex to try and enter
 * @return success or failure
 * @retval J9THREAD_RWMUTEX_WOULDBLOCK if the thread would have blocked
 * @retval J9THREAD_RWMUTEX_OK one the thread has entered for write
 *
 * @see omrthread_rwmutex_exit_write
 */
intptr_t
omrthread_rwmutex_try_enter_write(omrthread_rwmutex_t mutex)
{
	omrthread_t self = omrthread_self();
	ASSERT_RWMUTEX(mutex);

	/* recursive? */
	if (mutex->writer == self) {
		mutex->status--;
		return J9THREAD_RWMUTEX_OK;
	}

	omrthread_monitor_enter(mutex->syncMon);
	if (mutex->status != 0) {
		/* must get out */
		omrthread_monitor_exit(mutex->syncMon);
		return J9THREAD_RWMUTEX_WOULDBLOCK;
	}
	mutex->status--;
	mutex->writer = self;

	ASSERT(RWMUTEX_STATUS_WRITING(mutex));

	omrthread_monitor_exit(mutex->syncMon);

	return J9THREAD_RWMUTEX_OK;
}

/**
 * Exit a read/write mutex as a writer.
 *
 * @param[in] mutex a mutex to be exited
 * @return J9THREAD_RWMUTEX_OK on success
 *
 * @see omrthread_rwmutex_enter_write
 * @see omrthread_rwmutex_try_enter_write
 *
 */
intptr_t
omrthread_rwmutex_exit_write(omrthread_rwmutex_t mutex)
{
	ASSERT_RWMUTEX(mutex);
	ASSERT(mutex->writer == omrthread_self());
	ASSERT(RWMUTEX_STATUS_WRITING(mutex));
	omrthread_monitor_enter(mutex->syncMon);

	mutex->status++;
	if (0 == mutex->status) {
		mutex->writer = NULL;
		omrthread_monitor_notify_all(mutex->syncMon);
	}

	omrthread_monitor_exit(mutex->syncMon);
	return J9THREAD_RWMUTEX_OK;
}

/**
 * check if the mutex is in writing state.
 * for performance reason, the method would not use syncMon and
 * would return true if either RWMUTEX_STATUS_WRITING(mutex) or mutex->writer != NULL is true.
 * it is not 100% reliable for generic case. it is only for detecting logic error purpose.
 *
 * @param mutex a mutex to be tested
 * @return BOOLEAN return true if the mutex is in the writing state.
 */
BOOLEAN
omrthread_rwmutex_is_writelocked(omrthread_rwmutex_t mutex)
{
	return (RWMUTEX_STATUS_WRITING(mutex) || (0 != mutex->writer));
}

#if defined(OMR_THR_FORK_SUPPORT)
/**
 * @param [in] rwmutex to reset
 * @param [in] self thread
 * @return void
 */
void
omrthread_rwmutex_reset(omrthread_rwmutex_t rwmutex, omrthread_t self)
{
	if (RWMUTEX_STATUS_READING(rwmutex)) {
		fprintf(stderr, "ERROR: found read-locked rwmutex during post-fork reset!\n");
		abort();
	}
	if (rwmutex->writer != self) {
		/* If another thread was writing or reading and the current thread is not blocked,
		 * reset it. If current thread is writer, it stays writer. The syncMon is reset
		 * by postForkResetMonitors().
		 */
		rwmutex->writer = NULL;
		rwmutex->status = 0;
	}
}

J9Pool *
omrthread_rwmutex_init_pool(omrthread_library_t library)
{
	return pool_new(sizeof(RWMutex), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_THREADS, omrthread_mallocWrapper, omrthread_freeWrapper, library);
}

#endif /* defined(OMR_THR_FORK_SUPPORT) */

