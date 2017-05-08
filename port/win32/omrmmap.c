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


#include <windows.h>

#include "omrport.h"
#include "omrportpriv.h"
#include "portnls.h"
#include "ut_omrport.h"
#include "protect_helpers.h"



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
 * @args                                         OMRPORT_MMAP_FLAG_PRIVATE              private memory mapping, do not share with other processes (implied by OMRPORT_MMAP_FLAG_COPYONWRITE)
 *
 * @return                       A J9MmapHandle struct or NULL is an error has occurred
 */
J9MmapHandle *
omrmmap_map_file(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags, uint32_t categoryCode)
{
	DWORD lastError = 0;
	HANDLE mapping;
	void *pointer = NULL;
	DWORD flProtect = 0;
	DWORD dwFileOffsetHigh = 0;
	DWORD dwFileOffsetLow = 0;
	char *lpName = NULL;
	int lpNameSize = 0;
	DWORD dwDesiredAccess = 0;
	char *p;
	int rwCount = 0;
	int spCount = 0;
	const char *errMsg;
	char errBuf[512];
	J9MmapHandle *returnVal;
	OMRMemCategory *category;

	Trc_PRT_mmap_map_file_win32_entered(file, offset, size, mappingName, flags);

	category = omrmem_get_category(portLibrary, categoryCode);

	/* Check parameters */
	if ((HANDLE)file == INVALID_HANDLE_VALUE) {
		Trc_PRT_mmap_map_file_win32_invalidFile();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_MMAP_INVALID_FILE_HANDLE,
				 NULL);
		portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errMsg, file);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_MMAP_MAP_FILE_INVALIDFILE, errBuf);
		return NULL;
	}

	/* Determine flag settings for system calls */
	/* One, and only one, read/write option can be specified and zero or one shared/private option */
	if (flags & OMRPORT_MMAP_FLAG_READ) {
		flProtect = PAGE_READONLY;
		dwDesiredAccess = FILE_MAP_READ;
		rwCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_WRITE) {
		flProtect = PAGE_READWRITE;
		dwDesiredAccess = FILE_MAP_WRITE;
		rwCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_COPYONWRITE) {
		flProtect = PAGE_WRITECOPY;
		dwDesiredAccess = FILE_MAP_COPY;
		rwCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_SHARED) {
		spCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_PRIVATE) {
		dwDesiredAccess = FILE_MAP_COPY;
		spCount++;
	}

	if (1 != rwCount) {
		Trc_PRT_mmap_map_file_win32_invalidFlags();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_MMAP_INVALID_MEMORY_PROTECTION,
				 NULL);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_MMAP_MAP_FILE_INVALIDFLAGS, errMsg);
		return NULL;
	}
	if (spCount > 1) {
		Trc_PRT_mmap_map_file_win32_invalidFlags();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_MMAP_INVALID_FLAG,
				 NULL);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_MMAP_MAP_FILE_INVALIDFLAGS, errMsg);
		return NULL;
	}
	Trc_PRT_mmap_map_file_win32_flagsSet(flProtect, dwDesiredAccess);

	/* Modify mappingName to create valid object mapping object name (change invalid chars to '_' */
	if (mappingName != NULL) {
		lpNameSize = (int)strlen(mappingName) + 1;
		lpName = portLibrary->mem_allocate_memory(portLibrary, lpNameSize, OMR_GET_CALLSITE(), categoryCode);
		if (lpName == NULL) {
			errMsg = portLibrary->nls_lookup_message(portLibrary,
					 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
					 J9NLS_PORT_FILE_MEMORY_ALLOCATE_FAILURE,
					 NULL);
			portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_MMAP_MAP_FILE_MALLOCFAILED, errMsg);
			return NULL;
		}
		portLibrary->str_printf(portLibrary, lpName, lpNameSize, "%s", mappingName);
		for (p = lpName; *p != '\0'; p++) {
			if (*p == '\\' || *p == ':' || *p == ' ') {
				*p = '_';
			}
		}
	}
	Trc_PRT_mmap_map_file_win32_mappingName(mappingName, lpName);

	if (!(returnVal = (J9MmapHandle *)portLibrary->mem_allocate_memory(portLibrary, sizeof(J9MmapHandle), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY))) {
		if (lpName != NULL) {
			portLibrary->mem_free_memory(portLibrary, lpName);
		}
		Trc_PRT_mmap_map_file_cannotallocatehandle();
		return NULL;
	}

	/* Call CreateFileMapping */
	mapping = (HANDLE)CreateFileMapping((HANDLE)file, NULL, flProtect, 0, 0, (LPCTSTR)lpName);
	if (lpName != NULL) {
		portLibrary->mem_free_memory(portLibrary, lpName);
		lpName = NULL;
	}
	if (mapping == NULL) {
		portLibrary->mem_free_memory(portLibrary, returnVal);
		lastError = GetLastError();
		Trc_PRT_mmap_map_file_win32_badCreateFileMapping(lastError);
		portLibrary->error_set_last_error(portLibrary, lastError, OMRPORT_ERROR_MMAP_MAP_FILE_CREATEMAPPINGOBJECTFAILED);
		return NULL;
	}

	/* Call MapViewOfFile */
	dwFileOffsetHigh = (DWORD)(offset >> 32);
	dwFileOffsetLow = (DWORD)(offset & 0xFFFFFFFF);
	Trc_PRT_mmap_map_file_win32_callingMapViewOfFile(mapping, dwFileOffsetHigh, dwFileOffsetLow);
	pointer = MapViewOfFile(mapping, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, size);
	CloseHandle(mapping);
	if (pointer == NULL) {
		portLibrary->mem_free_memory(portLibrary, returnVal);
		lastError = GetLastError();
		Trc_PRT_mmap_map_file_win32_badMapViewOfFile(lastError);
		portLibrary->error_set_last_error(portLibrary, lastError, OMRPORT_ERROR_MMAP_MAP_FILE_MAPPINGFAILED);
		return NULL;
	}
	returnVal->pointer = pointer;
	returnVal->size = size;
	returnVal->category = category;

	omrmem_categories_increment_counters(category, size);

	/* Completed, return */
	Trc_PRT_mmap_map_file_win32_exiting(pointer, returnVal);
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
		UnmapViewOfFile(handle->pointer);
		omrmem_categories_decrement_counters(handle->category, handle->size);
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
	BOOL success;
	DWORD lastError = 0;

	Trc_PRT_mmap_msync_win32_entered(start, length, flags);

	success = FlushViewOfFile(start, length);
	if (!success) {
		lastError = GetLastError();
		Trc_PRT_mmap_msync_win32_badFlushViewOfFile(lastError);
		portLibrary->error_set_last_error(portLibrary, lastError, OMRPORT_ERROR_MMAP_MSYNC_FAILED);
		return -1;
	}

	Trc_PRT_mmap_msync_win32_exiting();
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
 * Check the capabilities available for J9MMAP at runtime for the current platform.
 *
 *
 * @param[in] portLibrary The port library
 *
 * @return a bit map containing the capabilites supported by the omrmmap sub component of the port library.
 * Possible bit values:
 *   OMRPORT_MMAP_CAPABILITY_COPYONWRITE - if not present, platform is not capable of "copy on write" memory mapping.
 *
 */
int32_t
omrmmap_capabilities(struct OMRPortLibrary *portLibrary)
{
	return (OMRPORT_MMAP_CAPABILITY_COPYONWRITE
			| OMRPORT_MMAP_CAPABILITY_READ
			| OMRPORT_MMAP_CAPABILITY_WRITE
			| OMRPORT_MMAP_CAPABILITY_PROTECT
			| OMRPORT_MMAP_CAPABILITY_MSYNC);
}

intptr_t
omrmmap_protect(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags)
{
	return protect_memory(portLibrary, address, length, flags);
}

uintptr_t
omrmmap_get_region_granularity(struct OMRPortLibrary *portLibrary, void *address)
{
	return protect_region_granularity(portLibrary, address);
}

void
omrmmap_dont_need(struct OMRPortLibrary *portLibrary, const void *startAddress, size_t length)
{
	size_t pageSize = portLibrary->mmap_get_region_granularity(portLibrary, (void *)startAddress);

	Trc_PRT_mmap_dont_need(pageSize, startAddress, length);

	if (pageSize > 0 && length >= pageSize) {
		uintptr_t endAddress = (uintptr_t) startAddress + length;
		uintptr_t roundedStart = ROUND_UP_TO_POWEROF2((uintptr_t) startAddress, pageSize);
		size_t roundedLength = ROUND_DOWN_TO_POWEROF2(endAddress - roundedStart, pageSize);
		Trc_PRT_mmap_dont_need_oscall(roundedStart, roundedLength);
		if (roundedLength >= pageSize) {
			if (!VirtualUnlock((LPVOID) roundedStart, roundedLength)) {
				DWORD lastError = GetLastError();
				if (ERROR_NOT_LOCKED != lastError) {
					Trc_PRT_mmap_dont_need_virtual_unlock_failed((void *)roundedStart, roundedLength, lastError);
				}
			}
		}
	}
}
