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

#include <sys/stat.h>
#include <errno.h>

#include "omrport.h"
#include "omrportasserts.h"
#include "omrvmem.h"
#include "ut_omrport.h"
#include "omrportpriv.h"

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

	/* ensure that allocated memory is 8 byte aligned, just in case it matters */
	allocPointer = portLibrary->mem_allocate_memory(portLibrary, size + 8, OMR_GET_CALLSITE(), category);
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

	if (!(returnVal = (J9MmapHandle *)portLibrary->mem_allocate_memory(portLibrary, sizeof(J9MmapHandle), OMR_GET_CALLSITE(), category))) {
		Trc_PRT_mmap_map_file_cannotallocatehandle();
		portLibrary->mem_free_memory(portLibrary, allocPointer);
		return NULL;
	}
	returnVal->pointer = mappedMemory;
	returnVal->allocPointer = allocPointer;
	returnVal->size = size;

	Trc_PRT_mmap_map_file_default_exit();
	return returnVal;
}

void
omrmmap_unmap_file(struct OMRPortLibrary *portLibrary, J9MmapHandle *handle)
{
	if (handle != NULL) {
		portLibrary->mem_free_memory(portLibrary, handle->allocPointer);
		portLibrary->mem_free_memory(portLibrary, handle);
	}
}
intptr_t
omrmmap_msync(struct OMRPortLibrary *portLibrary, void *start, uintptr_t length, uint32_t flags)
{
	Trc_PRT_mmap_msync_default_entered(start, length, flags);
	return -1;
}
int32_t
omrmmap_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}
void
omrmmap_shutdown(struct OMRPortLibrary *portLibrary)
{
}
int32_t
omrmmap_capabilities(struct OMRPortLibrary *portLibrary)
{
	return OMRPORT_MMAP_CAPABILITY_COPYONWRITE | OMRPORT_MMAP_CAPABILITY_READ;
}

intptr_t
omrmmap_protect(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags)
{
	return OMRPORT_PAGE_PROTECT_NOT_SUPPORTED;
}

uintptr_t
omrmmap_get_region_granularity(struct OMRPortLibrary *portLibrary, void *address)
{
	return 0;
}

#pragma linkage (PGSERRM,OS)
#pragma map(Pgser_Release,"PGSERRM")
#if defined(OMR_ENV_DATA64)
#pragma linkage(j9discard_data,OS_NOSTACK)
int j9discard_data(void *address, int numFrames);
#endif /*OMR_ENV_DATA64 */

intptr_t Pgser_Release(void *address, int byteAmount);
void
omrmmap_dont_need(struct OMRPortLibrary *portLibrary, const void *startAddress, size_t length)
{
	uintptr_t pageSize = portLibrary->mmap_get_region_granularity(portLibrary, (void *)startAddress);

	Trc_PRT_mmap_dont_need(pageSize, startAddress, length);

	if (pageSize > 0 && length >= pageSize) {
		uintptr_t const *endAddress = startAddress + length;
		uintptr_t const *roundedStart = (uintptr_t const *) ROUND_UP_TO_POWEROF2((uintptr_t) startAddress, pageSize);
		size_t roundedLength = ROUND_DOWN_TO_POWEROF2(endAddress - roundedStart, pageSize);
		if (roundedLength >= pageSize) {

			Trc_PRT_mmap_dont_need_oscall(roundedStart, roundedLength);

#if defined(OMR_ENV_DATA64)
			if (j9discard_data((void *)roundedStart, roundedLength >> ZOS_REAL_FRAME_SIZE_SHIFT) != 0) {
				Trc_PRT_mmap_dont_need_j9discard_data_failed((void *)roundedStart, roundedLength);
			}
#else
			if (Pgser_Release((void *)roundedStart, roundedLength) != 0) {
				Trc_PRT_mmap_dont_need_Pgser_Release_failed((void *)roundedStart, roundedLength);
			}
#endif /* defined(OMR_ENV_DATA64) */
		}
	}
}




