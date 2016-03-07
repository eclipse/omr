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
 * @brief CPU Control.
 *
 * Functions setting CPU attributes.
 */
#include <stdlib.h>
#include "omrport.h"

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the exit operations may be created here.  All resources created here should be destroyed
 * in @ref omrcpu_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_CPU
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrcpu_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrcpu_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
omrcpu_shutdown(struct OMRPortLibrary *portLibrary)
{
}

/**
 * @brief CPU Control operations.
 *
 * Flush the instruction cache to memory.
 *
 * @param[in] portLibrary The port library
 * @param[in] memoryPointer The base address of memory to flush.
 * @param[in] byteAmount Number of bytes to flush.
 */
void
omrcpu_flush_icache(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount)
{
	/* no-op by default */
}

/**
 * Get the cpu cache line size for supported platforms.
 *
 * @param[in] portLibrary The port library
 * @param[out] lineSize The cache line size
 * @return Returns the cpu cache line size if supported. Returns
 * OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM otherwise.
 *
 * @note Most implementations will return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM.
 */
int32_t
omrcpu_get_cache_line_size(struct OMRPortLibrary *portLibrary, int32_t *lineSize)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}
