/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include "omrcfg.h"

/** This header is included from outside the thread library directory and does not have
	access to vpath information encoded in the makefile.  Do a compile-time test of a
	J9THREAD_LIB constant defined in j9cfg.h.

	Please keep the tests in alphabetic order.
 */

#if OMRTHREAD_LIB_AIX
#include "./unix/thrdsup.h"
#elif OMRTHREAD_LIB_UNIX
#include "./unix/thrdsup.h"
#elif OMRTHREAD_LIB_WIN32
#include "./win/thrdsup.h"
#elif OMRTHREAD_LIB_ZOS
#include "./unix/thrdsup.h"
#else
#error "A OMRTHREAD_LIB_ constant should be defined in omrcfg.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform-specific thread creation.
 * @param[in] self Parent thread. Must be non-NULL.
 * @param[out] handle Location where this function should return the new thread's handle. Must be non-NULL.
 * @param[in] attr Creation attributes. Must be non-NULL. This function will not modify attr.
 * @param[in] entrypoint New thread's main function. Must be non-NULL.
 * @param[in] entryarg Data for main function.
 * @returns success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 * @retval J9THREAD_ERR_OS_ERRNO_SET bit flag indicating that an os_errno is available.
 */
intptr_t
osthread_create(struct J9Thread *self, OSTHREAD *handle, const omrthread_attr_t attr, 
		WRAPPER_FUNC entrypoint, WRAPPER_ARG entryarg);

/**
 * Platform-specific thread join.
 * @param[in] self The current thread.
 * @param[in] threadToJoin The thread to join.
 * @returns success or error code
 * @retval J9THREAD_SUCCESS Success
 * @retval J9THREAD_TIMED_OUT The OS join function returned a timeout status.
 * @retval J9THREAD_ERR The OS join function returned an error code.
 * @retval J9THREAD_ERR|J9THREAD_ERR_OS_ERRNO_SET The O/S join function returned an error code,
 *         and an os_errno is also available.
 */
intptr_t
osthread_join(omrthread_t self, omrthread_t threadToJoin);

#ifdef __cplusplus
}
#endif
