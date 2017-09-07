/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "omrportptb.h"


/**
 * @internal
 * @brief  Per Thread Buffer Support
 *
 * Free all memory associated with a per thread buffer, including any memory it may
 * have allocated.
 *
 * @param[in] portLibrary The port library.
 * @param[in] ptBuffers pointer to the PortlibPTBuffers struct that contains the buffers
 */
void
omrport_free_ptBuffer(struct OMRPortLibrary *portLibrary, PortlibPTBuffers_t ptBuffer)
{
	if (NULL != ptBuffer) {
		if (NULL != ptBuffer->errorMessageBuffer) {
			portLibrary->mem_free_memory(portLibrary, ptBuffer->errorMessageBuffer);
			ptBuffer->errorMessageBufferSize = 0;
		}
		if (NULL != ptBuffer->reportedMessageBuffer) {
			portLibrary->mem_free_memory(portLibrary, ptBuffer->reportedMessageBuffer);
			ptBuffer->reportedMessageBufferSize = 0;
		}

		portLibrary->mem_free_memory(portLibrary, ptBuffer);
	}
}

