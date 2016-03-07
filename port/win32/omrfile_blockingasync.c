/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
 * @brief file
 */


#include <windows.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"
#include "omrfilehelpers.h"

omrthread_tls_key_t tlsKeyOverlappedHandle = 0;

int32_t
omrfile_blockingasync_close(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	return omrfile_close(portLibrary, fd);
}


intptr_t
omrfile_blockingasync_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	flags |= EsOpenAsynchronous;
	return omrfile_open(portLibrary, path, flags, mode);
}


int32_t
omrfile_blockingasync_lock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length)
{
	return omrfile_lock_bytes_helper(portLibrary, fd, lockFlags, offset, length, TRUE);
}


int32_t
omrfile_blockingasync_unlock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length)
{
	return omrfile_unlock_bytes_helper(portLibrary, fd, offset, length, TRUE);
}


intptr_t
omrfile_blockingasync_read(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	DWORD bytesRead = 0;
	HANDLE fileHandle = NULL;
	OVERLAPPED overlapped;
	BOOL result = FALSE;
	DWORD highOffset = 0;
	int32_t errorCode = 0;
	HANDLE overlappedHandle = NULL;

	Trc_PRT_file_blockingasync_read_Entry(fd, buf, nbytes);

	fileHandle = (HANDLE)fd;

	if (0 == nbytes) {
		Trc_PRT_file_blockingasync_read_Exit(0);
		return 0;
	}

	overlappedHandle = omrfile_get_overlapped_handle_helper(portLibrary);
	if (NULL == overlappedHandle) {
		goto error;
	}

	memset(&overlapped, '\0', sizeof(OVERLAPPED));
	overlapped.hEvent = overlappedHandle;
	overlapped.Offset = SetFilePointer(fileHandle, 0, &highOffset, FILE_CURRENT);
	overlapped.OffsetHigh = highOffset;

	/* ReadFile limits the buffer to DWORD size and returns the number of bytes read,
	 * 	so the (DWORD)nbytes cast is functionally correct, though less than ideal.
	 * If the caller wants to read more than 2^32 bytes they will have to make more than one call,
	 * 	but the logic of the calling code should already account for this.
	 */

	result = ReadFile(fileHandle, buf, (DWORD)nbytes, &bytesRead, &overlapped);

	if (FALSE == result) {
		if (ERROR_IO_PENDING != GetLastError()) {
			goto error;
		}
		result = GetOverlappedResult(fileHandle, &overlapped,  &bytesRead, TRUE);
		if (FALSE == result) {
			goto error;
		}
	} /* else {
   		ReadFile completed synchronously. Nothing to do here.
   	} */

	if (0 == bytesRead) {
		errorCode = portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_FILE_READ_NO_BYTES_READ);
		Trc_PRT_file_blockingasync_read_Exit(errorCode);
		return -1;
	}

	/* Update the file pointer */
	if (-1 == omrfile_seek(portLibrary, fd, (int64_t)bytesRead, EsSeekCur)) {
		goto error;
	}

	Trc_PRT_file_blockingasync_read_Exit(bytesRead);
	return bytesRead;

error:
	errorCode = portLibrary->error_set_last_error(portLibrary, GetLastError(), findError(GetLastError()));
	Trc_PRT_file_blockingasync_read_Exit(errorCode);
	return -1;

}


intptr_t
omrfile_blockingasync_write(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	DWORD bytesWritten;
	intptr_t toWrite = 0;
	intptr_t offset = 0;
	HANDLE fileHandle = NULL;
	OVERLAPPED overlapped;
	BOOL result = FALSE;
	DWORD highOffset = 0;
	int32_t errorCode = 0;
	int64_t filePtr = 0;
	HANDLE overlappedHandle = NULL;

	Trc_PRT_file_blockingasync_write_Entry(fd, buf, nbytes);

	fileHandle = (HANDLE)fd;

	overlappedHandle = omrfile_get_overlapped_handle_helper(portLibrary);
	if (NULL == overlappedHandle) {
		goto error;
	}

	memset(&overlapped, '\0', sizeof(OVERLAPPED));
	overlapped.hEvent = overlappedHandle;
	overlapped.Offset = SetFilePointer(fileHandle, 0, &highOffset, FILE_CURRENT);
	overlapped.OffsetHigh = highOffset;
	filePtr = (DWORDLONG)overlapped.Offset | ((DWORDLONG)overlapped.OffsetHigh << 32);

	toWrite = nbytes;
	while (nbytes > 0) {
		if (toWrite > nbytes) {
			toWrite = nbytes;
		}
		/* The (DWORD)nbytes downcasts can make toWrite smaller than nbytes, however, the while loop
		 *  ensures that nbytes of data is still written */
		result = WriteFile(fileHandle, (char *)buf + offset, (DWORD)toWrite, &bytesWritten, &overlapped);

		if (FALSE == result) {
			if (ERROR_IO_PENDING == GetLastError()) {
				result = GetOverlappedResult(fileHandle, &overlapped,  &bytesWritten, TRUE);
			}

			if (FALSE == result) {
				if (ERROR_NOT_ENOUGH_MEMORY == GetLastError()) {
					/*[PR 94924] Use 48K chunks to get around out of memory problem */
					if (toWrite > (48 * 1024)) {
						toWrite = 48 * 1024;
					} else {
						toWrite /= 2;
					}
					/* If we can't write 128 bytes, just return */
					if (toWrite >= 128) {
						continue;
					}
				}
				goto error;
			}
		} /*else {
			 WriteFile completed synchronously
		} */
		offset += bytesWritten;
		nbytes -= bytesWritten;
		filePtr += bytesWritten;
		overlapped.Offset = (DWORD)(filePtr & 0xFFFFFFFF);
		overlapped.OffsetHigh = (DWORD)(filePtr >> 32);
		/* Update the file pointer */
		if (-1 == omrfile_seek(portLibrary, fd, filePtr, EsSeekSet)) {
			goto error;
		}
	}

	Trc_PRT_file_blockingasync_write_Exit(offset);

	return offset;

error:
	errorCode = portLibrary->error_set_last_error(portLibrary, GetLastError(), findError(GetLastError()));
	Trc_PRT_file_blockingasync_write_Exit(errorCode);
	return errorCode;
}


int32_t
omrfile_blockingasync_set_length(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength)
{
	return omrfile_set_length(portLibrary, fd, newLength);
}


int64_t
omrfile_blockingasync_flength(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	return omrfile_flength(portLibrary, fd);
}


static void J9THREAD_PROC
tls_blockingasync_finalizer(void *entry)
{
	HANDLE overlappedHandle = omrthread_tls_get(omrthread_self(), tlsKeyOverlappedHandle);
	if (NULL != overlappedHandle) {
		CloseHandle(overlappedHandle);
	}
}


int32_t
omrfile_blockingasync_startup(struct OMRPortLibrary *portLibrary)
{
	int32_t lastError = 0;

	Trc_PRT_file_blockingasync_startup_Entry();
	if (omrthread_tls_alloc_with_finalizer(&tlsKeyOverlappedHandle, tls_blockingasync_finalizer)) {
		lastError = portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_FILE_FAILED_TO_ALLOCATE_TLS);
		Trc_PRT_file_blockingasync_startup_alloc_tls_failure(lastError);
		return lastError;
	}
	Trc_PRT_file_blockingasync_startup_Exit();
	return 0;
}


void
omrfile_blockingasync_shutdown(struct OMRPortLibrary *portLibrary)
{
	Trc_PRT_file_blockingasync_shutdown_Entry();
	omrthread_tls_free(tlsKeyOverlappedHandle);
	Trc_PRT_file_blockingasync_shutdown_Exit();
}

