/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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

#include "omrfilehelpers.h"


/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
 */
int32_t
findError(int32_t errorCode)
{
	switch (errorCode) {
	case ERROR_FILENAME_EXCED_RANGE:
		return OMRPORT_ERROR_FILE_NAMETOOLONG;
	case ERROR_ACCESS_DENIED:
		return OMRPORT_ERROR_FILE_NOPERMISSION;
	case ERROR_FILE_NOT_FOUND:
		return OMRPORT_ERROR_FILE_NOENT;
	case ERROR_DISK_FULL:
		return OMRPORT_ERROR_FILE_DISKFULL;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:
		return OMRPORT_ERROR_FILE_EXIST;
	case ERROR_NOT_ENOUGH_MEMORY:
		return OMRPORT_ERROR_FILE_SYSTEMFULL;
	case ERROR_LOCK_VIOLATION:
		return OMRPORT_ERROR_FILE_LOCK_NOREADWRITE;
	case ERROR_NOT_READY:
		return OMRPORT_ERROR_FILE_IO;
	case ERROR_OPERATION_ABORTED:
		return OMRPORT_ERROR_FILE_OPERATION_ABORTED;
	case ERROR_NOT_ENOUGH_QUOTA:
		return OMRPORT_ERROR_FILE_NOT_ENOUGH_QUOTA;
	case ERROR_INSUFFICIENT_BUFFER:
		return OMRPORT_ERROR_FILE_INSUFFICIENT_BUFFER;
	case ERROR_HANDLE_EOF:
		return OMRPORT_ERROR_FILE_HANDLE_EOF;
	case ERROR_BROKEN_PIPE:
		return OMRPORT_ERROR_FILE_BROKEN_PIPE;
	case ERROR_MORE_DATA:
		return OMRPORT_ERROR_FILE_MORE_DATA;
	case ERROR_INVALID_PARAMETER:
		return OMRPORT_ERROR_FILE_INVALID_PARAMETER;
	case ERROR_IO_PENDING:
		return OMRPORT_ERROR_FILE_IO_PENDING;
	case ERROR_INVALID_HANDLE:
		return OMRPORT_ERROR_FILE_BADF;
	default:
		return OMRPORT_ERROR_FILE_OPFAILED;
	}
}

/**
 * @internal Gets the overlappedhandle event from TLS.
 * If it is not stored in TLS yet, it creates one and stores it in TLS.
 *
 * @param [in]   portLibrary            The port library
 *
 * @return 								OverlappedHandle
 */
HANDLE
omrfile_get_overlapped_handle_helper(struct OMRPortLibrary *portLibrary)
{
	omrthread_t thisThread = omrthread_self();
	HANDLE overlappedHandle = omrthread_tls_get(thisThread, tlsKeyOverlappedHandle);
	if (NULL == overlappedHandle) {
		overlappedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (NULL != overlappedHandle) {
			omrthread_tls_set(thisThread, tlsKeyOverlappedHandle, overlappedHandle);
		}
	}
	return overlappedHandle;
}


/**
 * This function will acquire a lock of the requested type on the given file, starting at offset bytes
 * from the start of the file and continuing for length bytes
 *
 * @param [in]   portLibrary            The port library
 * @param [in]   fd                              The file descriptor/handle of the file to be locked
 * @param [in]   lockFlags              Flags indicating the type of lock required and whether the call should block
 * @args                                               OMRPORT_FILE_READ_LOCK
 * @args                                               OMRPORT_FILE_WRITE_LOCK
 * @args                                               OMRPORT_FILE_WAIT_FOR_LOCK
 * @args                                               OMRPORT_FILE_NOWAIT_FOR_LOCK
 * @param [in]   offest                       Offset from start of file to start of locked region
 * @param [in]   length                      Number of bytes to be locked
 * @param [in] 	 async						True, if the file opened asynchronously, false otherwise
 *
 * @return                                              0 on success, -1 on failure
 */
int32_t
omrfile_lock_bytes_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length, BOOLEAN async)
{
	HANDLE hFile = (HANDLE)fd;
	DWORD dwFlags = 0;
	DWORD nNumberOfBytesToLockLow = (DWORD)(length & 0xFFFFFFFF);
	DWORD nNumberOfBytesToLockHigh = (DWORD)(length >> 32);
	OVERLAPPED overlapped;
	BOOL success;
	int32_t lastError = 0;
	const char *errMsg;
	char errBuf[512];
	DWORD bytes = 0;

	Trc_PRT_file_lock_bytes_win32_entered(fd, lockFlags, offset, length);

	if (!(lockFlags & OMRPORT_FILE_READ_LOCK) && !(lockFlags & OMRPORT_FILE_WRITE_LOCK)) {
		Trc_PRT_file_lock_bytes_win32_failed_noReadWrite();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_FILE_LOCK_INVALID_FLAG,
				 NULL);
		portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errMsg, lockFlags);
		lastError = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_FILE_LOCK_NOREADWRITE, errBuf);
		Trc_PRT_file_lock_bytes_win32_exiting_with_error(lastError);
		return -1;
	}
	if (!(lockFlags & OMRPORT_FILE_WAIT_FOR_LOCK) && !(lockFlags & OMRPORT_FILE_NOWAIT_FOR_LOCK)) {
		Trc_PRT_file_lock_bytes_win32_failed_noWaitNoWait();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_FILE_LOCK_INVALID_FLAG,
				 NULL);
		portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errMsg, lockFlags);
		lastError = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_FILE_LOCK_NOWAITNOWAIT, errBuf);
		Trc_PRT_file_lock_bytes_win32_exiting_with_error(lastError);
		return -1;
	}
	if (lockFlags & OMRPORT_FILE_WRITE_LOCK) {
		dwFlags |= LOCKFILE_EXCLUSIVE_LOCK;
	}
	if (lockFlags & OMRPORT_FILE_NOWAIT_FOR_LOCK) {
		dwFlags |= LOCKFILE_FAIL_IMMEDIATELY;
	}

	memset(&overlapped, '\0', sizeof(OVERLAPPED));
	if (TRUE == async) {
		HANDLE overlappedHandle = omrfile_get_overlapped_handle_helper(portLibrary);
		if (NULL == overlappedHandle) {
			goto error;
		}
		overlapped.hEvent = overlappedHandle;
	}
	overlapped.Offset = (DWORD)(offset & 0xFFFFFFFF);
	overlapped.OffsetHigh = (DWORD)(offset >> 32);

	Trc_PRT_file_lock_bytes_win32_callingLockFileEx(dwFlags, overlapped.Offset, overlapped.OffsetHigh, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);

	/*
	 * Regarding to asychronous files, this function returns right away without waiting its execution to finish.
	 * If the returned result is FALSE and error is ERROR_IO_PENDING, use GetOverlappedResult to wait the execution finishes.
	 *
	 */
	success = LockFileEx(hFile, dwFlags, 0, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, &overlapped);
	if ((TRUE == async) && (FALSE == success) && (ERROR_IO_PENDING == GetLastError())) {
		success = GetOverlappedResult(hFile, &overlapped, &bytes, TRUE);
	}
	if (!success) {
		goto error;
	}

	Trc_PRT_file_lock_bytes_win32_exiting();
	return 0;

error:
	lastError = portLibrary->error_set_last_error(portLibrary, GetLastError(), OMRPORT_ERROR_FILE_LOCK_BADLOCK);
	Trc_PRT_file_lock_bytes_win32_exiting_with_error(lastError);
	return -1;

}


/**
 * This function will release the lock on the given file, starting at offset bytes
 * from the start of the file and continuing for length bytes
 *
 * @param [in]   portLibrary            The port library
 * @param [in]   fd                              The file descriptor/handle of the file to be locked
 * @param [in]   offest                       Offset from start of file to start of locked region
 * @param [in]   length                      Number of bytes to be unlocked
 * @param [in] 	 async						True, if the file opened asynchronously, false otherwise
 *
 * @return                                              0 on success, -1 on failure
 */
int32_t
omrfile_unlock_bytes_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length, BOOLEAN async)
{
	HANDLE hFile = (HANDLE)fd;
	DWORD nNumberOfBytesToLockLow = (DWORD)(length & 0xFFFFFFFF);
	DWORD nNumberOfBytesToLockHigh = (DWORD)(length >> 32);
	OVERLAPPED overlapped;
	BOOL success;
	int32_t lastError = 0;
	DWORD bytes = 0;

	Trc_PRT_file_unlock_bytes_win32_entered(fd, offset, length);

	memset(&overlapped, '\0', sizeof(OVERLAPPED));
	if (TRUE == async) {
		HANDLE overlappedHandle = omrfile_get_overlapped_handle_helper(portLibrary);
		if (NULL == overlappedHandle) {
			goto error;
		}
		overlapped.hEvent = overlappedHandle;
	}
	overlapped.Offset = (DWORD)(offset & 0xFFFFFFFF);
	overlapped.OffsetHigh = (DWORD)(offset >> 32);

	Trc_PRT_file_unlock_bytes_win32_callingUnlockFileEx(overlapped.Offset, overlapped.OffsetHigh, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);

	/*
	 * Regarding to asychronous files, this function returns right away without waiting its execution to finish.
	 * If the returned result is FALSE and error is ERROR_IO_PENDING, use GetOverlappedResult to wait the execution finishes.
	 *
	 */
	success = UnlockFileEx(hFile, 0, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, &overlapped);
	if ((TRUE == async) && (FALSE == success) && (ERROR_IO_PENDING == GetLastError())) {
		success = GetOverlappedResult(hFile, &overlapped, &bytes, TRUE);
	}

	if (!success) {
		goto error;
	}
	Trc_PRT_file_unlock_bytes_win32_exiting();
	return 0;

error:
	lastError = portLibrary->error_set_last_error(portLibrary, GetLastError(), OMRPORT_ERROR_FILE_UNLOCK_BADUNLOCK);
	Trc_PRT_file_unlock_bytes_win32_exiting_with_error(lastError);
	return -1;
}
