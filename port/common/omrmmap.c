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

#include "omrport.h"
#include "ut_omrport.h"




/**
 * Map a part of file into memory.
 *
 * @param [in]  portLibrary		The port library
 * @param [in]  file			The file descriptor/handle of the already open file to be mapped
 * @param [in]  offset			The file offset of the part to be mapped
 * @param [in]  size			The number of bytes to be mapped, if zero, the whole file is mapped
 * @param [in]  mappingName		The name of the file mapping object to be created/opened.  This will be used as the basis of the name (invalid
 *                              characters being converted to '_') of the file mapping object on Windows
 *                              so that it can be shared between processes.  If a named object is not required, this parameter can be
 *                              specified as NULL
 * @param [in]  flags			Flags relating to the mapping:
 * @args                        	OMRPORT_MMAP_FLAG_READ           read only map
 * @args                            OMRPORT_MMAP_FLAG_WRITE          read/write map
 * @args                            OMRPORT_MMAP_FLAG_COPYONWRITE 	copy on write map
 * @args                            OMRPORT_MMAP_FLAG_SHARED         share memory mapping with other processes
 * @args                            OMRPORT_MMAP_FLAG_PRIVATE        private memory mapping, do not share with other processes (implied by OMRPORT_MMAP_FLAG_COPYONWRITE)
 * @param [in]  category        Memory allocation category code
 *
 * @return                      A J9MmapHandle struct or NULL is an error has occurred
 */
J9MmapHandle *
omrmmap_map_file(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags, uint32_t category)
{
	/* default implementation will allocate memory and read the file into it */
	uintptr_t numBytesRead;
	intptr_t rc;
	void *mappedMemory;
	void *allocPointer;
	J9MmapHandle *returnVal;

	Trc_PRT_mmap_map_file_default_entered(file, size);

	if (file == -1) {
		Trc_PRT_mmap_map_file_default_badfile();
		return NULL;
	}

	/* ensure that allocated memory is 8 byte aligned, just in case it matters */
	allocPointer = portLibrary->mem_allocate_memory(portLibrary, size + 8, OMR_GET_CALLSITE() , OMRMEM_CATEGORY_PORT_LIBRARY);
	Trc_PRT_mmap_map_file_default_allocPointer(allocPointer, size + 8);

	if (allocPointer == NULL) {
		Trc_PRT_mmap_map_file_default_badallocPointer();
		portLibrary->file_close(portLibrary, file);
		return NULL;
	}

	if ((uintptr_t)allocPointer % 8) {
		mappedMemory = (void *)((uintptr_t)allocPointer + 8 - ((uintptr_t)allocPointer % 8));
	} else {
		mappedMemory = allocPointer;
	}
	Trc_PRT_mmap_map_file_default_mappedMemory(mappedMemory);

	numBytesRead = 0;
	rc = 0;
	while (size - numBytesRead > 0) {
		rc = portLibrary->file_read(portLibrary, file, (void *)(((uintptr_t)mappedMemory) + numBytesRead), size - numBytesRead);
		if (rc == -1) {
			/* failed to completely read the file */
			Trc_PRT_mmap_map_file_default_badread();
			portLibrary->mem_free_memory(portLibrary, allocPointer);
			return NULL;
		}
		numBytesRead += rc;
		Trc_PRT_mmap_map_file_default_readingFile(numBytesRead);
	}

	if (!(returnVal = (J9MmapHandle *)portLibrary->mem_allocate_memory(portLibrary, sizeof(J9MmapHandle), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY))) {
		Trc_PRT_mmap_map_file_cannotallocatehandle();
		portLibrary->mem_free_memory(portLibrary, allocPointer);
		return NULL;
	}
	returnVal->pointer = mappedMemory;
	returnVal->size = size;

	Trc_PRT_mmap_map_file_default_exit();
	return returnVal;
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
	if (handle != NULL) {
		portLibrary->mem_free_memory(portLibrary, handle->pointer);
		portLibrary->mem_free_memory(portLibrary, handle);
	}
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
 * @args                                           OMRPORT_MMAP_SYNC_WAIT   Synchronous update required, function will not
 *                                                                     return until file updated.  Note that to achieve this on Windows requires the
 *                                                                     file to be opened with the FILE_FLAG_WRITE_THROUGH flag
 * @args                                           OMRPORT_MMAP_SYNC_ASYNC Asynchronous update required, function returns
 *                                                                    immediately, file will be updated later
 * @args                                           OMRPORT_MMAP_SYNC_INVALIDATE Requests that other mappings of the same
 *                                                                    file be invalidated, so that they can be updated with the values just written
 *
* @return                                          0 on success, -1 on failure.  Errors will be reported using the usual port library mechanism
 */
intptr_t
omrmmap_msync(struct OMRPortLibrary *portLibrary, void *start, uintptr_t length, uint32_t flags)
{
	Trc_PRT_mmap_msync_default_entered(start, length, flags);
	return -1;
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
 * \arg OMRPORT_ERROR_STARTUP_MMAP
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrmmap_startup(struct OMRPortLibrary *portLibrary)
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
 * Check the capabilities available for J9MMAP at runtime for the current platform
 *
 * @param [in]  portLibrary         The port library
 *
 * @return                                          The capabilities available on this platform
 * @args                                            OMRPORT_MMAP_CAPABILITY_READ
 * @args                                            OMRPORT_MMAP_CAPABILITY_WRITE
 * @args                                            OMRPORT_MMAP_CAPABILITY_COPYONWRITE
 * @args                                            OMRPORT_MMAP_CAPABILITY_MSYNC
 * @args											OMRPORT_MMAP_CAPABILITY_PROTECT
 */

int32_t
omrmmap_capabilities(struct OMRPortLibrary *portLibrary)
{
	return OMRPORT_MMAP_CAPABILITY_COPYONWRITE | OMRPORT_MMAP_CAPABILITY_READ;
}

/**
 * Sets the protection as specified by flags for the memory pages containing all or part of the interval address->(address+len).
 * The size of the memory page for a specified memory region can be requested via @ref omrmmap_get_region_granularity
 *
 * The memory region must have been aquired using omrmmap_file
 *
 * This call has no effect on the protection of other processes.
 *
 * The specified protection will overwrite any preexisting protection. This means that if the page had previously been marked
 * OMRPORT_PAGE_PROTECT_READ and omrmmap_protect is called with OMRPORT_PAGE_PROTECT_WRITE, it will no longer be readable.
 *
 * @param[in] portLibrary the Port Library.
 * @param[in] address Pointer to the shared memory region.
 * @param[in] length The size of memory in bytes spanning the region in which we want to set protection
 * @param[in] flags The specified protection to apply to the pages in the specified interval
 * \arg OMRPORT_PAGE_PROTECT_NONE accessing the memory will cause a segmentation violation
 * \arg OMRPORT_PAGE_PROTECT_READ reading the memory is allowed
 * \arg OMRPORT_PAGE_PROTECT_WRITE writing to the memory is allowed
 * \arg OMRPORT_PAGE_PROTECT_EXEC executing code in the memory is allowed
 *
 * @return 0 on success, non-zero on failure.
 * 	\arg OMRPORT_PAGE_PROTECT_NOT_SUPPORTED the functionality is not supported on this platform
 */
intptr_t
omrmmap_protect(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags)
{
	return OMRPORT_PAGE_PROTECT_NOT_SUPPORTED;
}

/**
 * Returns the minumum granularity in bytes on which permissions can be set for a memory region containing the address provided.
 *
 * @param[in] address  an address that lies within the region of memory the caller is interested in.
 *
 * @return 0 on error, the minimum size of region on which we can control permissions size on success.
 */
uintptr_t
omrmmap_get_region_granularity(struct OMRPortLibrary *portLibrary, void *address)
{
	return 0;
}

/**
 * Advise operating system to free resources in the given range.
 * @note The start address is rounded up to the nearest page boundary and the length is rounded down to a page boundary.
 * @param startAddress start address of the data to disclaim
 * @param length number of bytes to disclaim
 */
void
omrmmap_dont_need(struct OMRPortLibrary *portLibrary, const void *startAddress, size_t length)
{
	return;
}

