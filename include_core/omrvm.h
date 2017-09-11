/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#ifndef OMR_VM_API_H_
#define OMR_VM_API_H_

#include "omr.h"

#ifdef __cplusplus
extern "C" {
#endif

omr_error_t OMR_Initialize(void *languageVM, OMR_VM **vmSlot);

/* Warning: It is possible for OMR_Shutdown to be called from a different thread than OMR_Initialize, for example:
 * initial thread -> OMR_Initialize() -> new Thread() -> (in new thread) fork() -> (in child) OMR_Shutdown().
 *
 * The current thread might not be attached to any OMR components. In particular, it might not be omrthread_attach()ed.
 */
omr_error_t OMR_Shutdown(OMR_VM *omrVM);

omr_error_t OMR_Initialize_VM(OMR_VM **omrVMSlot, OMR_VMThread **omrVMThreadSlot, void *languageVM, void *languageVMThread);

omr_error_t OMR_Shutdown_VM(OMR_VM *omrVM, OMR_VMThread *omrVMThread);

/**
 * @brief Attach the current thread to OMR.
 *
 * The thread must be cleaned up with a matching call to OMR_Thread_Free().
 *
 * A thread may be OMR_Thread_Init()ed multiple times. However, each call to OMR_Thread_Init() must
 * be balanced by a call to OMR_Thread_Free().
 *
 * @post The current thread is omrthread_attach()ed.
 *
 * @param[in,out] omrVM The OMR VM.
 * @param[in,out] language_vm_thread The current thread's language VM thread.
 * @param[out] threadSlot Location to store the new OMR_VMThread. This should be &(language_vm_thread->_omrVMThread).
 * @param[in] threadName The new thread's name.
 * @return an OMR error code
 */
omr_error_t OMR_Thread_Init(OMR_VM *omrVM, void *language_vm_thread, OMR_VMThread **threadSlot, const char *threadName);

/**
 * @brief Detach a thread from OMR.
 *
 * Cleans up a thread that was initialized using OMR_Thread_Init().
 *
 * The thread need not be the current thread. It is recommended to invoke OMR_Thread_Free() from the terminating thread itself.
 * However, if the thread is _already dead_, OMR_Thread_Free() may be invoked by another thread to clean it up.
 *
 * A thread may be OMR_Thread_Free()ed multiple times, if it was OMR_Thread_Init()ed a corresponding number of times.
 *
 * @post If detaching the current thread, perform omrthread_detach() corresponding to the omrthread_attach()
 * in OMR_Thread_Init().
 *
 * @param[in,out] omrVMThread An attached OMR_VMThread. omrVMThread is freed by this function.
 * @return an OMR error code
 */
omr_error_t OMR_Thread_Free(OMR_VMThread *omrVMThread);

/**
 * @brief Helper for OMR_Thread_Init(). Use instead of OMR_Thread_Init() when certain that the current thread is not attached to OMR.
 */
omr_error_t OMR_Thread_FirstInit(OMR_VM *omrVM, omrthread_t self, void *language_vm_thread, OMR_VMThread **threadSlot, const char *threadName);

/**
 * @brief Helper for OMR_Thread_Free(). Use instead of OMR_Thread_Free() when certain that omrVMThread is only attached to OMR once.
 */
omr_error_t OMR_Thread_LastFree(OMR_VMThread *omrVMThread);


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* OMR_VM_API_H_ */
