/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Per-thread buffer (PTB) for socket information.
 *
 * Per thread buffers (PTB) are used to store information that is not shareable among the threads.
 * For example when an OS system call fails the error code associated with that error is
 * relevant to the thread that called the OS function; it has no meaning to any other thread.
 *
 * This file contains the functions supported by the port library for creating, accessing and
 * destroying per thread buffers. @see omrsock.h for details on the per thread buffer structure.
 */

#include "omrsockptb.h"

/**
 * @internal
 * @brief Omrsock Per Thread Buffer (PTB) Support
 *
 * Get a per thread buffer. 
 * 
 * OMRSocketPTB is the per thread buffer and omrsock_ptb_t is a pointer to OMRSocketPTB. Both are defined
 * in @ref omrsockptb.h. 
 *
 * @param[in] portLibrary The port library.
 *
 * @return a pointer to the per thread buffer on success, otherwise return NULL.
 */
omrsock_ptb_t
omrsock_ptb_get(struct OMRPortLibrary *portLibrary)
{
	omrthread_t self = omrthread_self();
	omrsock_ptb_t ptBuffer = (omrsock_ptb_t)omrthread_tls_get(self, portLibrary->portGlobals->socketTlsKey);

	if (NULL == ptBuffer) {
		ptBuffer = (omrsock_ptb_t)portLibrary->mem_allocate_memory(portLibrary, sizeof(OMRSocketPTB), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != ptBuffer) {
			if (0 == omrthread_tls_set(self, portLibrary->portGlobals->socketTlsKey, ptBuffer)) {
				memset(ptBuffer, 0, sizeof(OMRSocketPTB));
				ptBuffer->portLibrary = portLibrary;
			} else {
				portLibrary->mem_free_memory(portLibrary, ptBuffer);
				ptBuffer = NULL;
			}
		}
	}
	return ptBuffer;
}

/**
 * @internal
 * @brief Omrsock Per Thread Buffer (PTB) Support
 * 
 * Free the current thread's OMRSocketPTB. This function is the finalizer function referred
 * in @ref omrsock_ptb_init. This function is invoked when a thread is detached or terminated
 * if the thread's TLS entry for socketTlsKey is non-NULL.
 *
 * @param[in] socketPTBVoidP The current thread's J9SocketPTB.
 *
 * @return void return type.
 */
static void J9THREAD_PROC
omrsock_ptb_free(void *socketPTBVoidP)
{
	omrsock_ptb_t ptBuffer = (omrsock_ptb_t)socketPTBVoidP;
	struct OMRPortLibrary *portLibrary = ptBuffer->portLibrary;

	/* Free the ptBuffer */
	if (NULL != ptBuffer->addrInfoHints.addrInfo) {
		portLibrary->mem_free_memory(portLibrary, ptBuffer->addrInfoHints.addrInfo);
	}

	portLibrary->mem_free_memory(portLibrary, ptBuffer);
}

/**
 * @internal
 * @brief Omrsock Per Thread Buffer (PTB) Support
 * 
 * Initialize omrsockptb, to use omrsockptb related functions.
 * 
 * This function is called during startup of omrsock. All resources created here should be 
 * destroyed in @ref omrsock_ptb_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0, if no errors occurred, otherwise return an error.
 */
int32_t
omrsock_ptb_init(struct OMRPortLibrary *portLibrary)
{
	if(0 != omrthread_tls_alloc_with_finalizer(&(portLibrary->portGlobals->socketTlsKey), omrsock_ptb_free)){
		return OMRPORT_ERROR_SOCK_PTB_FAILED;
	}
	return 0;
}

/**
 * @internal
 * @brief Omrsock Per Thread Buffer (PTB) Support
 * 
 * Shutdown omrsockptb, shutdown omrsockptb related functions.
 *
 * This function is called during shutdown of omrsock. Any resources that were created by 
 * @ref omrsock_ptb_init should be destroyed here.
 *
 * @param[in] OMRPortLibrary The port library.
 * 
 * @return 0, if no errors occurred, otherwise return an error.
 */
int32_t 
omrsock_ptb_shutdown(struct OMRPortLibrary *portLibrary)
{
	if ((NULL != portLibrary->portGlobals)  && (0 != portLibrary->portGlobals->socketTlsKey)) {
		/* Release the TLS key */
		if (0 != omrthread_tls_free(portLibrary->portGlobals->socketTlsKey)) {
			return OMRPORT_ERROR_SOCK_PTB_FAILED;
		}
		portLibrary->portGlobals->socketTlsKey = 0;
	}
	return 0;
}
