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

#ifndef thread_internal_h
#define thread_internal_h

/**
* @file thread_internal.h
* @brief Internal prototypes used within the THREAD module.
*
* This file contains implementation-private function prototypes and
* type definitions for the THREAD module.
*
*/

#include "omrcfg.h"
#include "omrcomp.h"
#include "thread_api.h"
#include "thrtypes.h"
#if defined(OMR_THR_FORK_SUPPORT)
#include "omrpool.h"
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- omrthread.c ---------------- */

/**
* @brief
* @param lib
* @return void
*/
void
omrthread_init(omrthread_library_t lib);

/**
* @brief
* @param void
* @return void
*/
void
omrthread_shutdown(void);


/* ---------------- omrthreadjlm.c ---------------- */

#if defined(OMR_THR_JLM)

/**
 * @brief
 * @param thread
 * @return intptr_t
 */
intptr_t
jlm_thread_init(omrthread_t thread);

/**
 * @brief
 * @param lib
 * @param thread
 * @return void
 */
void
jlm_thread_free(omrthread_library_t lib, omrthread_t thread);

/**
 * @brief
 * @param lib
 * @param monitor
 * @return intptr_t
 */
intptr_t
jlm_monitor_init(omrthread_library_t lib, omrthread_monitor_t monitor);

/**
 * @brief
 * @param lib
 * @param monitor
 * @return void
 */
void
jlm_monitor_free(omrthread_library_t lib, omrthread_monitor_t monitor);

/**
 * @brief
 * @param lib
 * @param monitor
 * @return void
 */
void
jlm_monitor_clear(omrthread_library_t lib, omrthread_monitor_t monitor);

#endif /* OMR_THR_JLM */

/* ---------------- omrthreadtls.c ---------------- */

/**
 * @brief
 * @param thread
 * @return void
 */
void
omrthread_tls_finalize(omrthread_t thread);

/**
 * Run finalizers on any non-NULL TLS values for the current thread without locks.
 * To be used by omrthread fork handler functions while the lock is already held.
 *
 * @param[in] omrthread_t thread
 */
void
omrthread_tls_finalizeNoLock(omrthread_t thread);

/* ---------------- thrprof.c ---------------- */

/**
* @brief
* @param thread
* @return void
*/
void
paint_stack(omrthread_t thread);

/**
 * @brief Return a monotonically increasing high resolution clock in nanoseconds.
 * @return uint64_t
 */
uint64_t
omrthread_get_hires_clock(void);

/* ------------- omrthreadnuma.c ------------ */
void
omrthread_numa_init(omrthread_library_t threadLibrary);

void
omrthread_numa_shutdown(omrthread_library_t lib);

intptr_t
omrthread_numa_set_node_affinity_nolock(omrthread_t thread, const uintptr_t *nodeList, uintptr_t nodeCount, uint32_t flags);

#if defined(OMR_PORT_NUMA_SUPPORT)
void
omrthread_add_node_number_to_affinity_cache(omrthread_t thread, uintptr_t nodeNumber);

BOOLEAN
omrthread_does_affinity_cache_contain_node(omrthread_t thread, uintptr_t nodeNumber);

enum {J9THREAD_MAX_NUMA_NODE = 1024};
#endif /* defined(OMR_PORT_NUMA_SUPPORT) */


/* ------------- omrthreadmem.c ------------ */

void
omrthread_mem_init(omrthread_library_t threadLibrary);

/**
 * Thin-wrapper for malloc that increments the OMRMEM_CATEGORY_THREADS memory
 * counter.
 * @param [in] threadLibrary Thread library
 * @param [in] size Size of memory block to allocate
 * @param [in] memoryCategory Memory category
 * @return Pointer to allocated memory, or NULL if the allocation failed.
 */
void *
omrthread_allocate_memory(omrthread_library_t threadLibrary, uintptr_t size, uint32_t memoryCategory);

/**
 * Thin-wrapper for free that decrements the OMRMEM_CATEGORY_THREADS memory
 *  counter.
 * @param [in] threadLibrary Thread library
 * @param [in] ptr Pointer to free.
 */
void
omrthread_free_memory(omrthread_library_t threadLibrary, void *ptr);

/**
 * Updates memory category with an allocation
 *
 * @param [in] category Category to be updated
 * @param [in] size Size of memory block allocated
 */
void
increment_memory_counter(OMRMemCategory *category, uintptr_t size);

/**
 * Updates memory category with a free.
 *
 * @param [in] category Category to be updated
 * @param [in] size Size of memory block freed
 */
void
decrement_memory_counter(OMRMemCategory *category, uintptr_t size);

/**
 * @brief
 * @param unused
 * @param size
 * @param callSite
 * @param memoryCategory
 * @return void*
 */
void *
omrthread_mallocWrapper(void *unused, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit);

/**
 * @brief
 * @param unused
 * @param ptr
 * @return void
 */
void
omrthread_freeWrapper(void *unused, void *ptr, uint32_t type);

/* ------------- omrthreadpriority.c ------------ */
/**
 * Returns the OS priority from the priority_map for the
 * specified omrthread priority
 * @param omrthreadPriority[in]  The omrthread priority to map to the os priority
 * @return The priority value from the underlying global os priority_map
 */
int
omrthread_get_mapped_priority(omrthread_prio_t omrthreadPriority);

/* ------------- priority.c ------------ */

#if !defined(WIN32)
/**
 * @brief
 * @return intptr_t
 */
intptr_t
initialize_priority_map(void);
#endif /* !defined(WIN32) */

/**
 * @brief
 * @param thread
 * @return void
 */
void
initialize_thread_priority(omrthread_t thread);

/**
 * @brief
 * @return intptr_t
 */
intptr_t
set_priority_spread(void);

#if defined(OMR_THR_FORK_SUPPORT)
/**
 * @param [in] omrthread_rwmutex_t rwmutex to reset
 * @param [in] self Self thread
 */
void
omrthread_rwmutex_reset(omrthread_rwmutex_t rwmutex, omrthread_t self);

/**
 * @param omrthread_library_t Thread library
 * @return J9Pool*
 */
J9Pool *
omrthread_rwmutex_init_pool(omrthread_library_t library);
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#ifdef __cplusplus
}
#endif

#endif /* thread_internal_h */


