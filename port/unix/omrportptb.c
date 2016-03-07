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
#if defined(J9VM_PROVIDE_ICONV)
	int i;
#endif /* J9VM_PROVIDE_ICONV  */

	if (NULL != ptBuffer) {
		if (NULL != ptBuffer->errorMessageBuffer) {
			portLibrary->mem_free_memory(portLibrary, ptBuffer->errorMessageBuffer);
			ptBuffer->errorMessageBufferSize = 0;
		}
		if (NULL != ptBuffer->reportedMessageBuffer) {
			portLibrary->mem_free_memory(portLibrary, ptBuffer->reportedMessageBuffer);
			ptBuffer->reportedMessageBufferSize = 0;
		}

#if defined(J9VM_PROVIDE_ICONV)
		for (i = 0; i < UNCACHED_ICONV_DESCRIPTOR; i++) {
			if (J9VM_INVALID_ICONV_DESCRIPTOR != ptBuffer->converterCache[i]) {
				iconv_close(ptBuffer->converterCache[i]);
			}
		}
#endif /* J9VM_PROVIDE_ICONV */

		portLibrary->mem_free_memory(portLibrary, ptBuffer);
	}
}

