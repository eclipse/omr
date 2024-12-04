/*******************************************************************************
 * Copyright IBM Corp. and others 2013
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef omr_h
#define omr_h

/*
 * @ddr_namespace: default
 */

#include "omrcfg.h"

#include "omrport.h"

#define OMRPORT_ACCESS_FROM_OMRRUNTIME(omrRuntime) OMRPortLibrary *privateOmrPortLibrary = (omrRuntime)->_portLibrary
#define OMRPORT_ACCESS_FROM_OMRVM(omrVM) OMRPORT_ACCESS_FROM_OMRRUNTIME((omrVM)->_runtime)
#define OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread) OMRPORT_ACCESS_FROM_OMRVM((omrVMThread)->_vm)

#if defined(J9ZOS390)
#include "edcwccwi.h"
/* Convert function pointer to XPLINK calling convention */
#define OMR_COMPATIBLE_FUNCTION_POINTER(fp) ((void*)__bldxfd(fp))
#else /* J9ZOS390 */
#define OMR_COMPATIBLE_FUNCTION_POINTER(fp) ((void*)(fp))
#endif /* J9ZOS390 */

#if !(defined(OMR_GC_COMPRESSED_POINTERS) || defined(OMR_GC_FULL_POINTERS))
#error One or both of OMR_GC_COMPRESSED_POINTERS and OMR_GC_FULL_POINTERS must be defined
#endif /* !(defined(OMR_GC_COMPRESSED_POINTERS) || defined(OMR_GC_FULL_POINTERS)) */

#ifdef __cplusplus
extern "C" {
#endif

#define OMR_OS_STACK_SIZE (256 * 1024)

typedef enum {
	OMR_ERROR_NONE = 0,
	OMR_ERROR_OUT_OF_NATIVE_MEMORY,
	OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD,
	OMR_ERROR_MAXIMUM_VM_COUNT_EXCEEDED,
	OMR_ERROR_MAXIMUM_THREAD_COUNT_EXCEEDED,
	OMR_THREAD_STILL_ATTACHED,
	OMR_VM_STILL_ATTACHED,
	OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR,
	OMR_ERROR_INTERNAL,
	OMR_ERROR_ILLEGAL_ARGUMENT,
	OMR_ERROR_NOT_AVAILABLE,
	OMR_THREAD_NOT_ATTACHED,
	OMR_ERROR_FILE_UNAVAILABLE,
	OMR_ERROR_RETRY
} omr_error_t;

struct OMR_Agent;
struct OMR_RuntimeConfiguration;
struct OMR_Runtime;
struct OMR_SysInfo;
struct OMR_TI;
struct OMRTraceEngine;
struct OMR_VMConfiguration;
struct OMR_VM;
struct OMR_VMThread;
struct UtInterface;
struct UtThreadData;
struct OMR_TraceThread;

typedef struct OMR_RuntimeConfiguration {
	uintptr_t _maximum_vm_count; /* 0 for unlimited */
} OMR_RuntimeConfiguration;

typedef struct OMR_Runtime {
	uintptr_t _initialized;
	OMRPortLibrary *_portLibrary;
	struct OMR_VM *_vmList;
	omrthread_monitor_t _vmListMutex;
	struct OMR_VM *_rootVM;
	struct OMR_RuntimeConfiguration _configuration;
	uintptr_t _vmCount;
} OMR_Runtime;

typedef struct OMR_VMConfiguration {
	uintptr_t _maximum_thread_count; /* 0 for unlimited */
} OMR_VMConfiguration;

typedef struct OMR_ExclusiveVMAccessStats {
	U_64 startTime;
	U_64 endTime;
	U_64 totalResponseTime;
	struct OMR_VMThread *requester;
	struct OMR_VMThread *lastResponder;
	UDATA haltedThreads;
} OMR_ExclusiveVMAccessStats;

typedef struct OMR_VM {
	struct OMR_Runtime *_runtime;
	void *_language_vm;
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	uintptr_t _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	struct OMR_VM *_linkNext;
	struct OMR_VM *_linkPrevious;
	struct OMR_VMThread *_vmThreadList;
	omrthread_monitor_t _vmThreadListMutex;
	omrthread_tls_key_t _vmThreadKey;
	uintptr_t _arrayletLeafSize;
	uintptr_t _arrayletLeafLogSize;
	uintptr_t _compressedPointersShift;
	uintptr_t _objectAlignmentInBytes;
	uintptr_t _objectAlignmentShift;
	void *_gcOmrVMExtensions;
	struct OMR_VMConfiguration _configuration;
	uintptr_t _languageThreadCount;
	uintptr_t _internalThreadCount;
	struct OMR_ExclusiveVMAccessStats exclusiveVMAccessStats;
	uintptr_t gcPolicy;
	struct OMR_SysInfo *sysInfo;
	struct OMR_SizeClasses *_sizeClasses;
#if defined(OMR_THR_FORK_SUPPORT)
	uintptr_t forkGeneration;
	uintptr_t parentPID;
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#if defined(OMR_RAS_TDF_TRACE)
	struct UtInterface *utIntf;
	struct OMR_Agent *_hcAgent;
	omrthread_monitor_t _omrTIAccessMutex;
	struct OMRTraceEngine *_trcEngine;
	void *_methodDictionary;
#endif /* OMR_RAS_TDF_TRACE */
#if defined(OMR_GC_REALTIME)
	omrthread_monitor_t _gcCycleOnMonitor;
	uintptr_t _gcCycleOn;
#endif /* defined(OMR_GC_REALTIME) */
} OMR_VM;

typedef struct OMR_VMThread {
	struct OMR_VM *_vm;
	uint32_t _sampleStackBackoff;
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	uint32_t _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	void *_language_vmthread;
	omrthread_t _os_thread;
	struct OMR_VMThread *_linkNext;
	struct OMR_VMThread *_linkPrevious;
	uintptr_t _internal;
	void *_gcOmrVMThreadExtensions;

	uintptr_t vmState;
	uintptr_t exclusiveCount;

	uint8_t *threadName;
	BOOLEAN threadNameIsStatic; /**< threadName is managed externally; Don't free it. */
	omrthread_monitor_t threadNameMutex; /**< Hold this mutex to read or modify threadName. */

#if defined(OMR_RAS_TDF_TRACE)
	union {
		struct UtThreadData *uteThread; /* used by JVM */
		struct OMR_TraceThread *omrTraceThread; /* used by OMR */
	} _trace;
#endif /* OMR_RAS_TDF_TRACE */

	/* todo: dagar these are temporarily duplicated and should be removed from J9VMThread */
	void *lowTenureAddress;
	void *highTenureAddress;

	void *heapBaseForBarrierRange0;
	uintptr_t heapSizeForBarrierRange0;

	void *memorySpace;

	int32_t _attachCount;

	void *_savedObject1; /**< holds new object allocation until object can be attached to reference graph (see MM_AllocationDescription::save/restoreObjects()) */
	void *_savedObject2; /**< holds new object allocation until object can be attached to reference graph (see MM_AllocationDescription::save/restoreObjects()) */
} OMR_VMThread;

#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
/* Mixed mode - necessarily 64-bit */
#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
/* Mixed mode - static */
#define OMR_COMPRESS_OBJECT_REFERENCES(dynamicValue) (0 != OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
#else /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
/* Mixed mode - dynamic */
#define OMR_COMPRESS_OBJECT_REFERENCES(dynamicValue) (dynamicValue)
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#define OMRVM_REFERENCE_SHIFT(omrVM) (OMRVM_COMPRESS_OBJECT_REFERENCES(omrVM) ? 2 : 3)
#define OMRVMTHREAD_REFERENCE_SHIFT(omrVMThread) (OMRVMTHREAD_COMPRESS_OBJECT_REFERENCES(omrVMThread) ? 2 : 3)
#else /* OMR_GC_FULL_POINTERS */
/* Compressed only - necessarily 64-bit */
#define OMRVM_REFERENCE_SHIFT(omrVM) 2
#define OMRVMTHREAD_REFERENCE_SHIFT(omrVMThread) 2
#define OMR_COMPRESS_OBJECT_REFERENCES(dynamicValue) TRUE
#endif /* OMR_GC_FULL_POINTERS */
#else /* OMR_GC_COMPRESSED_POINTERS */
/* Full only - could be 32 or 64-bit */
#define OMR_COMPRESS_OBJECT_REFERENCES(dynamicValue) FALSE
#define OMRVM_REFERENCE_SHIFT(omrVM) OMR_LOG_POINTER_SIZE
#define OMRVMTHREAD_REFERENCE_SHIFT(omrVMThread) OMR_LOG_POINTER_SIZE
#endif /* OMR_GC_COMPRESSED_POINTERS */

#define OMRVM_COMPRESS_OBJECT_REFERENCES(omrVM) OMR_COMPRESS_OBJECT_REFERENCES(0 != (omrVM)->_compressObjectReferences)
#define OMRVMTHREAD_COMPRESS_OBJECT_REFERENCES(omrVMThread) OMR_COMPRESS_OBJECT_REFERENCES(0 != (omrVMThread)->_compressObjectReferences)

/**
 * Perform basic structural initialization of the OMR runtime
 * (allocating monitors, etc).
 *
 * @param[in] *runtime the runtime to initialize
 *
 * @return an OMR error code
 */
omr_error_t omr_initialize_runtime(OMR_Runtime *runtime);

/**
 * Perform final destruction of the OMR runtime.
 * All VMs be detached before calling.
 *
 * @param[in] *runtime the runtime to destroy
 *
 * @return an OMR error code
 */
omr_error_t omr_destroy_runtime(OMR_Runtime *runtime);

/**
 * Attach an OMR VM to the runtime.
 *
 * @param[in] *vm the VM to attach
 *
 * @return an OMR error code
 */
omr_error_t omr_attach_vm_to_runtime(OMR_VM *vm);

/**
 * Detach an OMR VM from the runtime.
 * All language threads must be detached before calling.
 *
 * @param[in] vm the VM to detach
 *
 * @return an OMR error code
 */
omr_error_t omr_detach_vm_from_runtime(OMR_VM *vm);

/**
 * Attach an OMR VMThread to the VM.
 *
 * Used by JVM and OMR tests, but not used by OMR runtimes.
 *
 * @param[in,out] vmthread The vmthread to attach. NOT necessarily the current thread.
 *                         vmthread->_os_thread must be initialized.
 *
 * @return an OMR error code
 */
omr_error_t omr_attach_vmthread_to_vm(OMR_VMThread *vmthread);

/**
 * Detach a OMR VMThread from the VM.
 *
 * Used by JVM and OMR tests, but not used by OMR runtimes.
 *
 * @param[in,out] vmthread The vmthread to detach. NOT necessarily the current thread.
 *
 * @return an OMR error code
 */
omr_error_t omr_detach_vmthread_from_vm(OMR_VMThread *vmthread);

/**
 * Initialize an OMR VMThread.
 * This should be done before attaching the thread to the OMR VM.
 * The caller thread must be attached to omrthread.
 *
 * @param[in,out] vmthread a new vmthread
 * @return an OMR error code
 */
omr_error_t omr_vmthread_init(OMR_VMThread *vmthread);

/**
 * Destroy an OMR VMThread. Free associated data structures.
 * This should be done after detaching the thread from the OMR VM.
 * The caller thread must be attached to omrthread.
 *
 * Does not free the vmthread.
 *
 * @param[in,out] vmthread the vmthread to cleanup
 */
void omr_vmthread_destroy(OMR_VMThread *vmthread);

/**
 * @brief Attach the current thread to an OMR VM.
 *
 * @pre The current thread must not be attached to the OMR VM.
 * @pre The current thread must be omrthread_attach()ed.
 *
 * Allocates and initializes a new OMR_VMThread for the current thread.
 * The thread should be detached using omr_vmthread_lastDetach().
 *
 * Not currently used by JVM.
 *
 * @param[in]  vm The OMR VM.
 * @param[out] vmThread A new OMR_VMThread for the current thread.
 * @return an OMR error code
 */
omr_error_t omr_vmthread_firstAttach(OMR_VM *vm, OMR_VMThread **vmThread);

/**
 * @brief Detach a thread from its OMR VM.
 *
 * @pre The thread being detached must not have any unbalanced re-attaches.
 * @pre The current thread must be omrthread_attach()ed.
 *
 * Detaches and destroys the OMR_VMThread.
 * The thread should have been initialized using omr_vmthread_firstAttach().
 *
 * This function might be used to detach another thread that is already dead.
 *
 * Not currently used by JVM.
 *
 * @param[in,out] vmThread The thread to detach. If the thread is already dead, it won't be the current thread. It will be freed.
 * @return an OMR error code
 */
omr_error_t omr_vmthread_lastDetach(OMR_VMThread *vmThread);

/**
 * @brief Re-attach a thread that is already attached to the OMR VM.
 *
 * This increments an attach count instead of allocating a new OMR_VMThread.
 * Re-attaches must be paired with an equal number of omr_vmthread_redetach()es.
 *
 * @param[in,out] currentThread The current OMR_VMThread.
 * @param[in] threadName A new name for the thread.
 */
void omr_vmthread_reattach(OMR_VMThread *currentThread, const char *threadName);

/**
 * @brief Detach a thread that has been re-attached multiple times.
 *
 * This decrements an attach count.
 * Re-detaches must be paired with an equal number of omr_vmthread_reattach()es.
 *
 * @param[in,out] omrVMThread An OMR_VMThread. It must have been re-attached at least once.
 */
void omr_vmthread_redetach(OMR_VMThread *omrVMThread);

/**
 * Get the current OMR_VMThread, if the current thread is attached.
 *
 * This function doesn't prevent another thread from maliciously freeing OMR_VMThreads
 * that are still in use.
 *
 * @param[in] vm The VM
 * @return A non-NULL OMR_VMThread if the current thread is already attached, NULL otherwise.
 */
OMR_VMThread *omr_vmthread_getCurrent(OMR_VM *vm);

/*
 * C wrappers for OMR_Agent API
 */
/**
 * @see OMR_Agent::createAgent
 */
struct OMR_Agent *omr_agent_create(OMR_VM *vm, char const *arg);

/**
 * @see OMR_Agent::destroyAgent
 */
void omr_agent_destroy(struct OMR_Agent *agent);

/**
 * @see OMR_Agent::openLibrary
 */
omr_error_t omr_agent_openLibrary(struct OMR_Agent *agent);

/**
 * @see OMR_Agent::callOnLoad
 */
omr_error_t omr_agent_callOnLoad(struct OMR_Agent *agent);

/**
 * @see OMR_Agent::callOnUnload
 */
omr_error_t omr_agent_callOnUnload(struct OMR_Agent *agent);

/**
 * Access the TI function table.
 *
 * @return the TI function table
 */
struct OMR_TI const *omr_agent_getTI(void);

#if defined(OMR_THR_FORK_SUPPORT)

/**
 * To be called directly after a fork in the child process, this function will reset the OMR_VM,
 * including cleaning up the _vmThreadList of threads that do not exist after a fork.
 *
 * @param[in] vm The OMR vm.
 */
void omr_vm_postForkChild(OMR_VM *vm);

/**
 * To be called directly after a fork in the parent process, this function releases the
 * _vmThreadListMutex.
 *
 * @param[in] vm The OMR vm.
 */
void omr_vm_postForkParent(OMR_VM *vm);

/**
 * To be called directly before a fork, this function will hold the _vmThreadListMutex to
 * ensure that the _vmThreadList is in a consistent state during a fork.
 *
 * @param[in] vm The OMR vm.
 */
void omr_vm_preFork(OMR_VM *vm);

#endif /* defined(OMR_THR_FORK_SUPPORT) */

/*
 * LANGUAGE VM GLUE
 * The following functions must be implemented by the language VM.
 */
/**
 * @brief Bind the current thread to a language VM.
 *
 * As a side-effect, the current thread will also be bound to the OMR VM.
 * A thread can bind itself multiple times.
 * Binds must be paired with an equal number of Unbinds.
 *
 * @param[in] omrVM the OMR vm
 * @param[in] threadName An optional name for the thread. May be NULL.
 *   It is the responsibility of the caller to ensure this string remains valid for the lifetime of the thread.
 * @param[out] omrVMThread the current OMR VMThread
 * @return an OMR error code
 */
omr_error_t OMR_Glue_BindCurrentThread(OMR_VM *omrVM, const char *threadName, OMR_VMThread **omrVMThread);

/**
 * @brief Unbind the current thread from its language VM.
 *
 * As a side-effect, the current thread will also be unbound from the OMR VM.
 * Unbinds must be paired with an equal number of binds.
 * When the bind count of a thread reaches 0, the OMR VMThread
 * is freed, and can no longer be used.
 *
 * @param[in,out] omrVMThread the current OMR VMThread
 * @return an OMR error code
 */
omr_error_t OMR_Glue_UnbindCurrentThread(OMR_VMThread *omrVMThread);

/**
 * @brief Allocate and initialize a new language thread.
 *
 * Allocate a new language thread, perform language-specific initialization,
 * and attach it to the language VM.
 *
 * The new thread is probably not the current thread. Don't perform initialization
 * that requires the new thread to be currently executing, such as setting TLS values.
 *
 * Don't invoke OMR_Thread_Init(), or attempt to allocate an OMR_VMThread.
 *
 * @param[in] languageVM The language VM to attach the new thread to.
 * @param[out] languageThread A new language thread.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_AllocLanguageThread(void *languageVM, void **languageThread);

/**
 * @brief Cleanup and free a language thread.
 *
 * Perform the reverse of OMR_Glue_AllocLanguageThread().
 *
 * The thread being destroyed is probably not the current thread.
 *
 * Don't invoke OMR_Thread_Free().
 *
 * @param[in,out] languageThread The thread to be destroyed.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_FreeLanguageThread(void *languageThread);

/**
 * @brief Link an OMR VMThread to its corresponding language thread.
 *
 * @param[in,out] languageThread The language thread to be modified.
 * @param[in] omrVMThread The OMR_VMThread that corresponds to languageThread.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_LinkLanguageThreadToOMRThread(void *languageThread, OMR_VMThread *omrVMThread);

#if defined(OMR_OS_WINDOWS)
/**
 * @brief Get a platform-dependent token that can be used to locate the VM directory.
 *
 * The token is used by the utility function detectVMDirectory().
 * Although this utility is currently only implemented for Windows, this functionality
 * is also useful for other platforms.
 *
 * For Windows, the token is the name of a module that resides in the VM directory.
 * For AIX/Linux, the token would be the address of a function that resides in a
 * library in the VM directory.
 * For z/OS, no token is necessary because the VM directory is found by searching
 * the LIBPATH.
 *
 * @param[out] token A token.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_GetVMDirectoryToken(void **token);
#endif /* defined(OMR_OS_WINDOWS) */

char *OMR_Glue_GetThreadNameForUnnamedThread(OMR_VMThread *vmThread);

/**
 * Get the number of method properties. This is the number of properties per method
 * inserted into the method dictionary. See OMR_Glue_GetMethodDictionaryPropertyNames().
 *
 * @return Number of method properties
 */
int OMR_Glue_GetMethodDictionaryPropertyNum(void);

/**
 * Get the method property names. This should be a constant array of strings identifying
 * the method properties common to all methods to be inserted into the method dictionary
 * for profiling, such as method name, file name, line number, class name, or other properties.
 *
 * @return Method property names
 */
const char * const *OMR_Glue_GetMethodDictionaryPropertyNames(void);

#ifdef __cplusplus
/*
 * Not all compilers retain debugging information for anonymous enum types.
 * This macro works around that behavior by declaring a static member
 * (that will not be defined). It must be applied selectively because
 * not all compilers generate debugging information for static members
 * (__xlC__ is defined by xlC_r, __ibmxl_version__ is defined by xlclang++).
 */
#if (defined(__xlC__) && ((__xlC__ >> 8) >= 16)) \
 || (defined(__ibmxl_version__) && (__ibmxl_version__ >= 16)) \
 || defined(__open_xl__)
#define ddr_constant(name, value) static enum { name = value } ddr_ref_ ## name
#else /* defined(__ibmxl_version__) && (__ibmxl_version__ >= 16) */
#define ddr_constant(name, value)        enum { name = value }
#endif /* defined(__ibmxl_version__) && (__ibmxl_version__ >= 16) */

} /* extern "C" */
#endif /* __cplusplus */

#endif /* omr_h */
