/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 *   Multiple authors (IBM Corp.) - z/TPF platform initial port to OMR environment
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Memory map
 *
 * This module provides memory mapping facilities that allow a user to map files
 * into the virtual address space of the process.  There are various options that
 * can be used when mapping a file into memory, such as copy on write.  Not all 
 * of these options are available on all platforms, @ref omrmmap_capabilities
 * provides the list of supported options.  Note also that on some platforms 
 * memory mapping facilites do not exist at all. On these platforms the API will
 * still be available, but will simply read the file into allocated memory.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "omrport.h"
#include "omrportasserts.h"
#include "portnls.h"
#include "ut_omrport.h"
#include "protect_helpers.h"
#include "omrportpriv.h"


/**
 * Map a part of file into memory. 
 *
 * @param [in]  portLibrary       The port library
 * @param [in]  file                       The file descriptor/handle of the already open file to be mapped
 * @param [in]  offset                  The file offset of the part to be mapped
 * @param [in]  size                     The number of bytes to be mapped, if zero, the whole file is mapped
 * @param [in]  mappingName      The name of the file mapping object to be created/opened.  This will be used as the basis of the name (invalid 
 *                                                         characters being converted to '_') of the file mapping object on Windows
 *                                                         so that it can be shared between processes.  If a named object is not required, this parameter can be
 *                                                         specified as NULL
 * @param [in]   flags                  Flags relating to the mapping:
 * @args                                         OMRPORT_MMAP_FLAG_READ                     read only map
 * @args                                         OMRPORT_MMAP_FLAG_WRITE                   read/write map
 * @args                                         OMRPORT_MMAP_FLAG_COPYONWRITE copy on write map
 * @args                                         OMRPORT_MMAP_FLAG_SHARED              share memory mapping with other processes
 * @args                                         OMRPORT_MMAP_FLAG_PRIVATE              private memory mapping, do not share with other processes (implied by J9PORT_MMAP_FLAG_COPYONWRITE)
 * @param [in]  categoryCode     Memory allocation category code
 *
 * @return                       A J9MmapHandle struct or NULL is an error has occurred
 */
J9MmapHandle *
omrmmap_map_file(struct OMRPortLibrary *portLibrary, IDATA file, U_64 offset, UDATA size, const char *mappingName, U_32 flags, U_32 categoryCode)
{
  return NULL;
}
/**
 * UnMap previously mapped memory.
 *
 * @param[in] portLibrary The port library
 *
 * @param[in] handle - A pointer to a J9MmapHandle structure returned by omrmmap_map_file.
 */
void
omrmmap_unmap_file(struct OMRPortLibrary *portLibrary, J9MmapHandle *handle)
{

}
/**
 * Synchronise updates to memory mapped file region with file on disk.  The call may wait for the file write
 * to complete or this may be scheduled for a later time and the function return immediately, depending on
 * the flags setting
 *
 * @param [in]  portLibrary         The port library
 * @param [in]  start                      Pointer to the start of the memory mapped area to be synchronised
 * @param [in]  length                  Length of the memory mapped area to be synchronised
 * @param [in]  flags                    Flags controlling the behaviour of the function:
 * @args                                           J9PORT_MMAP_SYNC_WAIT   Synchronous update required, function will not
 *                                                                     return until file updated.  Note that to achieve this on Windows requires the
 *                                                                     file to be opened with the FILE_FLAG_WRITE_THROUGH flag
 * @args                                           J9PORT_MMAP_SYNC_ASYNC Asynchronous update required, function returns
 *                                                                    immediately, file will be updated later
 * @args                                           J9PORT_MMAP_SYNC_INVALIDATE Requests that other mappings of the same
 *                                                                    file be invalidated, so that they can be updated with the values just written
 *
* @return                                          0 on success, -1 on failure.  Errors will be reported using the usual port library mechanism
 */
IDATA
omrmmap_msync(struct OMRPortLibrary *portLibrary, void *start, UDATA length, U_32 flags)
{
  return 0;
}
/**
 * PortLibrary shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrmmap_startup
 * should be destroyed here.
 *
 */
void
omrmmap_shutdown(struct OMRPortLibrary *portLibrary)
{
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the memory mapping operations may be created here.  All resources created here should be destroyed
 * in @ref omrmmap_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_MMAP
 *
 * @note Most implementations will simply return success.
 */
I_32
omrmmap_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}
/**
 * Check the capabilities available for J9MMAP at runtime for the current platform.
 *
 *
 * @param[in] portLibrary The port library
 *
 * @return a bit map containing the capabilites supported by the j9mmap sub component of the port library.
 * Possible bit values:
 *   J9PORT_MMAP_CAPABILITY_COPYONWRITE - if not present, platform is not capable of "copy on write" memory mapping.
 *
 */
I_32
omrmmap_capabilities(struct OMRPortLibrary *portLibrary)
{
  return 0;
}

IDATA
omrmmap_protect(struct OMRPortLibrary *portLibrary, void* address, UDATA length, UDATA flags)
{
	return protect_memory(portLibrary, address, length, flags);
}

UDATA 
omrmmap_get_region_granularity(struct OMRPortLibrary *portLibrary, void *address)
{
	return protect_region_granularity(portLibrary, address);
}


