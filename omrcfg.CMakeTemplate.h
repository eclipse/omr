/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

/**
 * The OMR build configuration. A public header. Controls the APIs of all other headers.
 * Include at the top of every OMR source and header.
 */

#if !defined(OMRCFG_H_)
#define OMRCFG_H_

#cmakedefine OMR_GC
#cmakedefine OMR_JIT
#cmakedefine OMR_JITBUILDER
#cmakedefine OMR_TEST_COMPILER
#cmakedefine OMR_PORT
#cmakedefine OMR_THREAD
#cmakedefine OMR_OMRSIG
#cmakedefine OMR_EXAMPLE

#cmakedefine OMR_GC_ALLOCATION_TAX
#cmakedefine OMR_GC_OBJECT_ALLOCATION_NOTIFY
#cmakedefine OMR_GC_BATCH_CLEAR_TLH
#cmakedefine OMR_GC_COMBINATION_SPEC
#cmakedefine OMR_GC_CONCURRENT_SWEEP
#cmakedefine OMR_GC_DEBUG_ASSERTS
#cmakedefine OMR_GC_HEAP_CARD_TABLE
#cmakedefine OMR_GC_IDLE_HEAP_MANAGER
#cmakedefine OMR_GC_LARGE_OBJECT_AREA
#cmakedefine OMR_GC_LEAF_BITS
#cmakedefine OMR_GC_MINIMUM_OBJECT_SIZE
#cmakedefine OMR_GC_MODRON_COMPACTION
#cmakedefine OMR_GC_MODRON_CONCURRENT_MARK
#cmakedefine OMR_GC_MODRON_SCAVENGER
#cmakedefine OMR_GC_CONCURRENT_SCAVENGER
#cmakedefine OMR_GC_MODRON_STANDARD
#cmakedefine OMR_GC_NON_ZERO_TLH
#cmakedefine OMR_GC_SEGREGATED_HEAP
#cmakedefine OMR_GC_THREAD_LOCAL_HEAP

/**
 * Tells if a platform has support for Semaphores.
 * ifRemoved: platform cannot use semaphore operations.
 */
#cmakedefine OMR_INTERP_HAS_SEMAPHORES

/**
 * Use TDF based trace engine
 * ifRemoved: No tracepoints
 */
#cmakedefine OMR_RAS_TDF_TRACE

/**
 * Support options to adapt spinning as controlled by options provided to the vm.
 * Requires flag: OMR_THR_JLM_HOLD_TIMES
 * ifRemoved: No adaptive spinning supported
 */
#cmakedefine OMR_THR_ADAPTIVE_SPIN

/**
 * Enable support for lock monitor to profile locking behaviour - counts only
 * ifRemoved: Lock monitor cannot be enabled
 */
#cmakedefine OMR_THR_JLM

/**
 * Enable support for lock monitor to profile locking behaviour - counts + timings only
 * ifRemoved: Lock monitor cannot be enabled
 */
#cmakedefine OMR_THR_JLM_HOLD_TIMES

/**
 * object header does not contain the monitor word
 * ifRemoved: object header will contain the monitor word
 */
#cmakedefine OMR_THR_LOCK_NURSERY

/**
 * Output lots of thread statistics (Pentium only)
 * ifRemoved: Turn off in production VMs
 */
#cmakedefine OMR_THR_TRACING 1

#cmakedefine OMRTHREAD_LIB_AIX 1
#cmakedefine OMRTHREAD_LIB_UNIX 1
#cmakedefine OMRTHREAD_LIB_WIN32 1
#cmakedefine OMRTHREAD_LIB_ZOS 1

/**
 * This spec targets PPC processors.
 */
#cmakedefine OMR_ARCH_POWER

/**
 * This spec targets ARM processors.
 */
#cmakedefine OMR_ARCH_ARM

/**
 * This spec targets x86 processors.
 */
#cmakedefine OMR_ARCH_X86

/**
 * This spec targets S390 processors.
 */
#cmakedefine OMR_ARCH_S390

#cmakedefine OMR_ENV_DATA64
#cmakedefine OMR_ENV_GCC
#cmakedefine OMR_ENV_LITTLE_ENDIAN
#cmakedefine OMR_GC_ARRAYLETS
#cmakedefine OMR_GC_COMPRESSED_POINTERS
#cmakedefine OMR_GC_HYBRID_ARRAYLETS
#cmakedefine OMR_GC_OBJECT_MAP
#cmakedefine OMR_GC_REALTIME
#cmakedefine OMR_GC_STACCATO
#cmakedefine OMR_GC_TLH_PREFETCH_FTA
#cmakedefine OMR_GC_VLHGC

/**
 * Flag to indicate that on 64-bit platforms, the monitor slot and class slot in object headers are U32 rather than UDATA.
 */
#cmakedefine OMR_INTERP_COMPRESSED_OBJECT_HEADER

/**
 * Flag to indicate that on 64-bit platforms, the monitor slot in object headers is a U32 rather than a UDATA.
 * ifRemoved: The monitor slot is a UDATA.
 */
#cmakedefine OMR_INTERP_SMALL_MONITOR_SLOT

/**
 * Add support for CUDA
 */
#cmakedefine OMR_OPT_CUDA

/**
 * Enabling this flag implies the platform's vmem implementation is able to allocate memory top down.
 * ifRemoved: Implies the platform's vmem implementation is not able to allocate memory top down
 */
#cmakedefine OMR_PORT_ALLOCATE_TOP_DOWN

/**
 * Enables async signal handler thread in the port library.
 * ifRemoved: Async signal handler thread is disabled.
 */
#cmakedefine OMR_PORT_ASYNC_HANDLER

/**
 * The platform is able to attempt to reserve virtual memory via a call to omrvmem_reserve_memory at the address specified by the user.
 * ifRemoved: The platform is not able to reserve virtual memory at a specific address
 */
#cmakedefine OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS

/**
 * This platform is able to associate memory with a specific node, as is relevant when the system has a Non Uniform Memory Access configuration.
 * ifRemoved: This platform is not able to associate memory with a specific node.
 */
#cmakedefine OMR_PORT_NUMA_SUPPORT

/**
 * If set, omrsig_protect will include support for registering a handler using CEEHDLR.
 * Pass OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR into omrsig_set_options() before the first call
 * to omrsig_protect() to turn on the use of CEEHDLR at runtime.
 * ifRemoved: omrsig_protect will not include support for CEEHDLR.
 */
#cmakedefine OMR_PORT_ZOS_CEEHDLRSUPPORT

/**
 * Enable the j9thread library to be used after a fork().
 * ifRemoved: The j9thread library may be corrupt and unusable after a fork().
 */
#cmakedefine OMR_THR_FORK_SUPPORT

/**
 * Attempt to enter raw monitors using user-space spinlocks before reverting to an OS synchronization object
 * ifRemoved: All raw monitor enters will proceed directly to an OS synchronization object
 */
#cmakedefine OMR_THR_THREE_TIER_LOCKING

/**
 * Enable support for custom spin options.
 * Requires flag: OMR_THR_THREE_TIER_LOCKING.
 * ifRemoved: Custom spin options cannot be used.
 */
#cmakedefine OMR_THR_CUSTOM_SPIN_OPTIONS

/**
 * Allows a user to select the thread notify policy: signal or broadcast.
 * ifRemoved: User will not be able to select the thread notify policy, and broadcast policy will always be used.
 */
#cmakedefine OMR_NOTIFY_POLICY_CONTROL

/**
 * This flag enables new synchronization prototypes which  
 * -- prevent large number of threads to spin at a time on a monitor 
 * -- avoid the thundering herd problem on wake up
 *
 * The prototypes provide the following features
 * -- track number of threads spinning on a monitor
 * -- wake threads only when no thread is spinning on a monitor
 * -- limit/control number of threads that are woken up per monitor
 * -- limit/control number of threads that are allowed to spin per monitor
 */
#cmakedefine OMR_THR_SPIN_WAKE_CONTROL

/**
 * This enables the option to select different algorithms for yielding.
 */
#cmakedefine OMR_THR_YIELD_ALG

#endif /* !defined(OMRCFG_H_) */
