/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef omrportptb_h
#define omrportptb_h

/*
 * @ddr_namespace: map_to_type=J9PortptbConstants
 */

/**
 * @file
 * @ingroup Port
 * @brief Per Thread Buffers
 *
 * Per thread buffers are used to store information that is not sharable among the threads.
 * For example when an OS system call fails the error code associated with that error is
 * relevant to the thread that called the OS function; it has no meaning to any other thread.
 *
 * This file contains the structure of the per thread buffers.  @see omrtlshelpers.c for details on accessing
 * these buffers..
 */

#include "omrport.h"

#include "omriconvhelpers.h"

#define J9ERROR_DEFAULT_BUFFER_SIZE 256 /**< default customized error message size if we need to create one */
#define J9ERROR_MAXIMUM_BUFFER_SIZE 0xFFFFFFFF /**< maximum customized error message size if we need to create one */

/**
 * @typedef
 * @brief The per thread buffer
 * Storage for data related to the threads state.
 */
typedef struct PortlibPTBuffers_struct {
	struct PortlibPTBuffers_struct *next; /**< Next per thread buffer */
	struct PortlibPTBuffers_struct *previous; /**< Previous per thread buffer */

	int32_t platformErrorCode; /**< error code as reported by the OS */
	int32_t portableErrorCode; /**< error code translated to portable format by application */
	char *errorMessageBuffer; /**< last saved error message, either customized or from OS */
	uintptr_t errorMessageBufferSize; /**< error message buffer size */

	int32_t reportedErrorCode; /**< last reported error code */
	char *reportedMessageBuffer; /**< last reported error message, either customized or from OS */
	uintptr_t reportedMessageBufferSize; /**< reported message buffer size */

#if defined(J9VM_PROVIDE_ICONV)
	iconv_t converterCache[UNCACHED_ICONV_DESCRIPTOR]; /**< Everything in J9IconvName before UNCACHED_ICONV_DESCRIPTOR is cached */
#endif /* J9VM_PROVIDE_ICONV */
} PortlibPTBuffers_struct;

/**
 * @typedef
 * @brief The per thread buffer
 * Storage for data related to the threads state.
 */
typedef struct PortlibPTBuffers_struct *PortlibPTBuffers_t;

void omrport_free_ptBuffer(struct OMRPortLibrary *portLibrary, PortlibPTBuffers_t ptBuffer);


#endif     /* omrportptb_h */


