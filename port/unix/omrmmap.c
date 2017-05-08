/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if defined(AIXPPC)
#include <sys/shm.h>
#include <sys/vminfo.h>
#endif/*AIXPPC*/

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
 * @param [in]  categoryCode     Memory allocation category code
 *
 * @return                       A J9MmapHandle struct or NULL is an error has occurred
 */
J9MmapHandle *
omrmmap_map_file(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags, uint32_t categoryCode)
{
	int mmapProt = 0;
	int mmapFlags = 0;
	void *pointer = NULL;
	int rwCount = 0;
	int spCount = 0;
	char const *errMsg;
	J9MmapHandle *returnVal;
	OMRMemCategory *category = omrmem_get_category(portLibrary, categoryCode);

	Trc_PRT_mmap_map_file_unix_entered(file, offset, size, mappingName, flags);

	/* Determine prot and flags values for mmap */
	/* One, and only one, read/write option can be specified and zero or one shared/private option */
	if (flags & OMRPORT_MMAP_FLAG_READ) {
		mmapProt = PROT_READ;
		mmapFlags = MAP_SHARED;
		rwCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_WRITE) {
		mmapProt = PROT_READ | PROT_WRITE;
		mmapFlags = MAP_SHARED;
		rwCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_COPYONWRITE) {
		mmapProt = PROT_READ | PROT_WRITE;
		mmapFlags = MAP_PRIVATE;
		rwCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_SHARED) {
		mmapFlags = MAP_SHARED;
		spCount++;
	}
	if (flags & OMRPORT_MMAP_FLAG_PRIVATE) {
		mmapFlags = MAP_PRIVATE;
		spCount++;
	}
	if (1 != rwCount) {
		Trc_PRT_mmap_map_file_unix_invalidFlags();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_MMAP_INVALID_MEMORY_PROTECTION,
				 NULL);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_MMAP_MAP_FILE_INVALIDFLAGS, errMsg);
		return NULL;
	}
	if (spCount > 1) {
		Trc_PRT_mmap_map_file_unix_invalidFlags();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_MMAP_INVALID_FLAG,
				 NULL);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_MMAP_MAP_FILE_INVALIDFLAGS, errMsg);
		return NULL;
	}
	Trc_PRT_mmap_map_file_unix_flagsSet(mmapProt, mmapFlags);

	if (0 == size) {
		struct stat buf;

		memset(&buf, 0, sizeof(struct stat));
		if (-1 == fstat(file - FD_BIAS, &buf)) {
			Trc_PRT_mmap_map_file_unix_filestatfailed();
			portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_MMAP_MAP_FILE_STATFAILED);
			return NULL;
		}
		size = buf.st_size;
	}

	if (!(returnVal = (J9MmapHandle *)portLibrary->mem_allocate_memory(portLibrary, sizeof(J9MmapHandle), OMR_GET_CALLSITE(), categoryCode))) {
		Trc_PRT_mmap_map_file_cannotallocatehandle();
		return NULL;
	}

	/* Call mmap */
	pointer = mmap(0, size, mmapProt, mmapFlags, file, offset);
	if (pointer == MAP_FAILED) {
		portLibrary->mem_free_memory(portLibrary, returnVal);
		Trc_PRT_mmap_map_file_unix_badMmap(errno);
		portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_MMAP_MAP_FILE_MAPPINGFAILED);
		return NULL;
	}

	returnVal->category = category;
	omrmem_categories_increment_counters(category, size);

	returnVal->pointer = pointer;
	returnVal->size = size;

	/* Completed, return */
	Trc_PRT_mmap_map_file_unix_exiting(pointer, returnVal);
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
	intptr_t rc = 0;

	Trc_PRT_mmap_unmap_file_unix_entering(handle);

	if (handle != NULL) {
		Trc_PRT_mmap_unmap_file_unix_values(handle->pointer, handle->size);
		rc = munmap(handle->pointer, handle->size);
		omrmem_categories_decrement_counters(handle->category, handle->size);
		portLibrary->mem_free_memory(portLibrary, handle);
	}

	Trc_PRT_mmap_unmap_file_unix_exiting(rc);
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
	int msyncFlags = 0;
	int rc = 0;

	Trc_PRT_mmap_msync_unix_entered(start, length, flags);

	/* Validate flags and translate to msyncFlags */
	if ((flags & OMRPORT_MMAP_SYNC_WAIT) && (flags & OMRPORT_MMAP_SYNC_ASYNC)) {
		portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_MMAP_MSYNC_INVALIDFLAGS);
		Trc_PRT_mmap_msync_unix_invalidFlags();
		return -1;
	}
	if (flags & OMRPORT_MMAP_SYNC_WAIT) {
		msyncFlags |= MS_SYNC;
	}
	if (flags & OMRPORT_MMAP_SYNC_ASYNC) {
		msyncFlags |= MS_ASYNC;
	}
	if (flags & OMRPORT_MMAP_SYNC_INVALIDATE) {
		msyncFlags |= MS_INVALIDATE;
	}
	Trc_PRT_mmap_msync_unix_flagsSet(msyncFlags);

	/* Call msync */
	rc = msync(start, length, msyncFlags);
	if (rc == -1) {
		Trc_PRT_mmap_msync_unix_badMsync(errno);
		portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_MMAP_MSYNC_FAILED);
		return -1;
	}

	Trc_PRT_mmap_msync_unix_exiting();
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
			| OMRPORT_MMAP_CAPABILITY_PROTECT
			/* If JSE platforms include WRITE and MSYNC - ZOS included, but currently has own omrmmap.c */
#if ((defined(LINUX) && defined(J9X86)) \
  || (defined(LINUXPPC)) \
  || (defined(LINUX) && defined(S390)) \
  || (defined(LINUX) && defined(J9HAMMER)) \
  || (defined(LINUX) && defined(ARM)) \
  || (defined(AIXPPC)) \
  || (defined(OSX)))
			| OMRPORT_MMAP_CAPABILITY_WRITE
			| OMRPORT_MMAP_CAPABILITY_MSYNC
#endif
		   );
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

/**
 * Disclaim entire pages only: don't release partial pages at the start and end.
 * This is to prevent reloading pages used by other callers and to satisfy madvise's constraints.
 */
void
omrmmap_dont_need(struct OMRPortLibrary *portLibrary, const void *startAddress, size_t length)
{
	size_t pageSize = portLibrary->mmap_get_region_granularity(portLibrary, (void *)startAddress);

	Trc_PRT_mmap_dont_need(pageSize, startAddress, length);

	if (pageSize > 0 && length >= pageSize) {
		uintptr_t endAddress = (uintptr_t) startAddress + length;
		uintptr_t roundedStart = ROUND_UP_TO_POWEROF2((uintptr_t) startAddress, pageSize);
		size_t roundedLength = ROUND_DOWN_TO_POWEROF2(endAddress - roundedStart, pageSize);
		if (roundedLength >= pageSize) {

			Trc_PRT_mmap_dont_need_oscall(roundedStart, roundedLength);

#if defined(LINUX) || defined(OSX)
			if (-1 == madvise((void *)roundedStart, roundedLength, MADV_DONTNEED)) {
				Trc_PRT_mmap_dont_need_madvise_failed((void *)roundedStart, roundedLength, errno);
			}
#elif defined(AIXPPC)
			/* madvise is not supported on AIX.  disclaim64() fails on virtual memory mapped to a file */
			if (-1 == disclaim64((void *)roundedStart, roundedLength, DISCLAIM_ZEROMEM)) {
				Trc_PRT_mmap_dont_need_disclaim64_failed((void *)roundedStart, roundedLength, errno);
			}
#endif /* defined(AIXPPC) */
		}
	}
}
