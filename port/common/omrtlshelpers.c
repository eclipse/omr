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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Per Tthread Buffer Support
 *
 * Per thread buffers are used to store information that is not sharable among the threads.
 * For example when an OS system call fails the error code associated with that error is
 * relevant to the thread that called the OS function; it has no meaning to any other thread.
 *
 * This file contains the functions supported by the port library for creating, accessing and
 * destroying per thread buffers.@see omrportptb.h for details on the per thread buffer structure.
 *
 * Only the function @omrport_tls_free is available via the port library function table.  The rest of
 * the functions are helpers for the port library only.
 */
#include <string.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrthread.h"
#include "omrportptb.h"
#include "omriconvhelpers.h"


/**
 * @internal
 * @brief Per Thread Buffer Support
 *
 * Get a per thread buffer.
 *
 * @param[in] portLibrary The port library.
 *
 * @return The per thread buffer on success, NULL on failure.
 */
void *
omrport_tls_get(struct OMRPortLibrary *portLibrary)
{
	PortlibPTBuffers_t ptBuffers;

	ptBuffers = omrthread_tls_get(omrthread_self(), portLibrary->portGlobals->tls_key);
	if (NULL == ptBuffers) {
		MUTEX_ENTER(portLibrary->portGlobals->tls_mutex);

		ptBuffers = portLibrary->mem_allocate_memory(portLibrary, sizeof(PortlibPTBuffers_struct), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != ptBuffers) {
			if (0 == omrthread_tls_set(omrthread_self(), portLibrary->portGlobals->tls_key, ptBuffers)) {
#if defined(J9VM_PROVIDE_ICONV)
				int i;
				memset(ptBuffers, 0, sizeof(PortlibPTBuffers_struct));
				for (i = 0; i < UNCACHED_ICONV_DESCRIPTOR; i++) {
					ptBuffers->converterCache[i] = J9VM_INVALID_ICONV_DESCRIPTOR;
				}
#else
				memset(ptBuffers, 0, sizeof(PortlibPTBuffers_struct));
#endif /* J9VM_PROVIDE_ICONV */
				ptBuffers->next = portLibrary->portGlobals->buffer_list;
				if (portLibrary->portGlobals->buffer_list) {
					((PortlibPTBuffers_t)portLibrary->portGlobals->buffer_list)->previous = ptBuffers;
				}
				portLibrary->portGlobals->buffer_list = ptBuffers;
			} else {
				portLibrary->mem_free_memory(portLibrary, ptBuffers);
				ptBuffers = NULL;
			}
		}

		MUTEX_EXIT(portLibrary->portGlobals->tls_mutex);
	}
	return ptBuffers;
}
/**
 * @brief Per Thread Buffer Support
 *
 * Free the per thread buffers.
 *
 * @param[in] portLibrary The port library.
 */
void
omrport_tls_free(struct OMRPortLibrary *portLibrary)
{
	PortlibPTBuffers_t ptBuffers;

	MUTEX_ENTER(portLibrary->portGlobals->tls_mutex);
	ptBuffers = omrthread_tls_get(omrthread_self(), portLibrary->portGlobals->tls_key);
	if (ptBuffers) {
		omrthread_tls_set(omrthread_self(), portLibrary->portGlobals->tls_key, NULL);

		/* Unlink */
		if (ptBuffers->next) {
			ptBuffers->next->previous = ptBuffers->previous;
		}
		if (portLibrary->portGlobals->buffer_list == ptBuffers) {
			portLibrary->portGlobals->buffer_list = ptBuffers->next;
		} else {
			if (ptBuffers->previous) {
				ptBuffers->previous->next = ptBuffers->next;
			}
		}

		omrport_free_ptBuffer(portLibrary, ptBuffers);
	}
	MUTEX_EXIT(portLibrary->portGlobals->tls_mutex);
}
/**
 * @internal
 * @brief PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the portl library thread local storage operations may be created here.  All resources created here should be destroyed
 * in @ref omrport_tls_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_TLS
 * \arg OMRPORT_ERROR_STARTUP_TLS_ALLOC
 * \arg OMRPORT_ERROR_STARTUP_TLS_MUTEX
 */
int32_t
omrport_tls_startup(struct OMRPortLibrary *portLibrary)
{
	if (omrthread_tls_alloc(&portLibrary->portGlobals->tls_key)) {
		return OMRPORT_ERROR_STARTUP_TLS_ALLOC;
	}

	if (!MUTEX_INIT(portLibrary->portGlobals->tls_mutex)) {
		return OMRPORT_ERROR_STARTUP_TLS_MUTEX;
	}

	return 0;
}
/**
 * @internal
 * @brief PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by
 * @ref omrport_tls_startup should be destroyed here.
 *
 * @param[in] OMRPortLibrary The port library
 */
void
omrport_tls_shutdown(struct OMRPortLibrary *portLibrary)
{
	if (NULL != portLibrary->portGlobals) {
		PortlibPTBuffers_t ptBuffers, next;

		/* Free all remaining buffer sets */
		MUTEX_ENTER(portLibrary->portGlobals->tls_mutex);
		ptBuffers = portLibrary->portGlobals->buffer_list;
		while (NULL != ptBuffers) {
			next = ptBuffers->next;
			omrport_free_ptBuffer(portLibrary, ptBuffers);
			ptBuffers = next;
		}
		portLibrary->portGlobals->buffer_list = NULL;
		MUTEX_EXIT(portLibrary->portGlobals->tls_mutex);

		/* Now dispose of the tls_key and the mutex */
		omrthread_tls_free(portLibrary->portGlobals->tls_key);
		MUTEX_DESTROY(portLibrary->portGlobals->tls_mutex);
	}
}
/**
 * @internal
 * @brief Per Thread Buffer Support
 *
 * Get the per thread buffer for a thread. If the buffer has not been allocated do not allocate a new
 * one, the function @ref omrport_tls_get is used for that purpose. This function is not exported outside
 * the port library as most applications will want a per thread buffer created to store their data.  This
 * function is used to access existing data in the per thread buffers.
 *
 * @param[in] portLibrary The port library.
 *
 * @return The per thread buffer, may be NULL.
 */
void *
omrport_tls_peek(struct OMRPortLibrary *portLibrary)
{
	return omrthread_tls_get(omrthread_self(), portLibrary->portGlobals->tls_key);
}

