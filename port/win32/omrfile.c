/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief file
 */


#include <windows.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportptb.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"
#include "omrfilehelpers.h"

static wchar_t *file_get_unicode_path(OMRPortLibrary *portLibrary, const char *utf8Path, wchar_t *unicodeBuffer, uintptr_t unicodeBufferSize);

#define J9FILE_UNC_EXTENDED_LENGTH_PREFIX (L"\\\\?\\")
#define J9FILE_UNC_EXTENDED_LENGTH_PREFIX_NETWORK (L"\\\\?\\UNC")

/* Convert file descriptor to windows file handle.
 * OMRPORT_TTY_IN, OMRPORT_TTY_OUT and OMRPORT_TTY_ERR are handled as a special case because they don't
 * match UNIX standard stream file descriptors.
 * @param[in] fd returned by port library functions
 * @return windows file handle.
 */
static HANDLE
toHandle(OMRPortLibrary *portLibrary, intptr_t fd)
{
	switch (fd) {
	case OMRPORT_TTY_IN:
		return PPG_tty_consoleInputHd;
	case OMRPORT_TTY_OUT:
		return PPG_tty_consoleOutputHd;
	case OMRPORT_TTY_ERR:
		return PPG_tty_consoleErrorHd;
	default:
		return (HANDLE) fd;
	}
}

/* Convert windows file handle to a file descriptor.
 * OMRPORT_TTY_IN, OMRPORT_TTY_OUT and OMRPORT_TTY_ERR are handled as a special case because they don't
 * match UNIX standard stream file descriptors.
 * @param[in] fd returned by port library functions
 * @return windows file handle.
 */
static intptr_t
fromHandle(OMRPortLibrary *portLibrary, HANDLE handle)
{
	if (handle == PPG_tty_consoleInputHd) {
		return OMRPORT_TTY_IN;
	}
	if (handle == PPG_tty_consoleOutputHd) {
		return OMRPORT_TTY_OUT;
	}
	if (handle == PPG_tty_consoleErrorHd) {
		return OMRPORT_TTY_ERR;
	}

	return (intptr_t) handle;
}


/*
 * Uses the standard port_convertFromUTF8 and if the resulting unicode path is longer than the Windows defined MAX_PATH,
 * the path is converted to an absolute path and prepended with \\?\ or \\?\UNC if it is a networked path (eg. \\server\share\..)
 *
 * @note	Relative paths are not thread-safe as the current directory is a process global that may change.
 *
 * @param[in] 	portLibrary			the portLibrary
 * @param[in] 	utf8Path			the string to be converted
 * @param[out]	unicodeBuffer		buffer to hold the unicode representation of @ref utf8Path
 * @param[in]	unicodeBufferSize	size of @ref unicodeBuffer.
 *
 * @return 		NULL on failure
 * 			 	otherwise the converted string.
 * 					If the return value is not the same as @ref unicodeBuffer, file_get_unicode_path had to allocate memory to hold the
 * 					unicode path - the caller is responsible for freeing this memory by calling @ref omrmem_free_memory on the returned value.
 */
static wchar_t *
file_get_unicode_path(OMRPortLibrary *portLibrary, const char *utf8Path, wchar_t *unicodeBuffer, uintptr_t unicodeBufferSize)
{
	wchar_t *unicodePath;
	wchar_t *pathName;
	size_t pathLen;

	unicodePath = port_convertFromUTF8(portLibrary, utf8Path, unicodeBuffer, unicodeBufferSize);

	if (NULL == unicodePath) {
		return NULL;
	}

	pathLen = wcslen(unicodePath);

	/* If the file name is longer than (the system-defined) MAX_PATH characters we need to prepend it with \\?\  */
	if (pathLen * sizeof(wchar_t) <= MAX_PATH) {
		pathName = unicodePath;
	} else {

		DWORD getFullPathNameRc;
		DWORD fullPathNameLengthInWcharts;
		wchar_t *fullPathNameBuffer = NULL;
		uintptr_t fullPathNameBufferSizeBytes;
		wchar_t *pathPrefix = NULL;
		size_t lengthOfPathPrefix = 0;
		size_t sizeOfPathPrefix = 0;
		UDATA startOfPathInBuffer = 0;

		if (('\\' == utf8Path[0]) && ('\\' == utf8Path[1])) {
			pathPrefix = J9FILE_UNC_EXTENDED_LENGTH_PREFIX_NETWORK;
			lengthOfPathPrefix = wcslen(pathPrefix);
			/*
			 * We do not have to account for a null terminator as we are replacing one of the initial "\" in the file path
			 * eg. \\server\share -> \\?\UNC\server\share
			 * */
			sizeOfPathPrefix = lengthOfPathPrefix * sizeof(wchar_t);
			startOfPathInBuffer = (UDATA)(lengthOfPathPrefix - 1);
		} else {
			pathPrefix = J9FILE_UNC_EXTENDED_LENGTH_PREFIX;
			lengthOfPathPrefix = wcslen(pathPrefix);
			sizeOfPathPrefix = lengthOfPathPrefix * sizeof(wchar_t) + sizeof(wchar_t) /* null terminator */ ;
			startOfPathInBuffer = (UDATA)lengthOfPathPrefix;
		}

		/*
		 * We need to prepend the path with "\\?\" or "\\?\UNC", but this format is only supported if the path is fully qualified.
		 *
		 * The only way to get the fully qualified path is using GetFullPathNameW. The MSDN docs for GetFullPathNameW state
		 * that it should not be used in multi-threaded applications, but the docs are fairly clear that this is because the conversion from a
		 * relative path to a fully qualified path depends on the current working directory (CWD), which is a process global that may change.
		 *
		 * However, we can't simply limit the use of GetFullPathNameW to non-absolute paths, because all occurrences of "." and "..",
		 * need to be removed. This includes the occurrences that don't simply prepend the path, in which case the CWD is not relevant and
		 * where we assume the call to GetFullPathNameW is thread-safe.
		 *
		 */

		/* find out how big a buffer we need */
		fullPathNameLengthInWcharts = GetFullPathNameW(unicodePath, 0 /* zero means give us back the size we need */, NULL, NULL);
		if (0 == fullPathNameLengthInWcharts) {
			pathName = NULL;
			goto cleanup;
		}

		fullPathNameBufferSizeBytes = fullPathNameLengthInWcharts * sizeof(wchar_t) + sizeOfPathPrefix;

		fullPathNameBuffer = portLibrary->mem_allocate_memory(portLibrary, fullPathNameBufferSizeBytes, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == fullPathNameBuffer) {
			pathName = NULL;
			goto cleanup;
		}

		getFullPathNameRc = GetFullPathNameW(unicodePath, fullPathNameLengthInWcharts , fullPathNameBuffer, NULL);
		if (0 == getFullPathNameRc) {
			pathName = NULL;
			goto cleanup;
		}

		/*  now prepend the fullPathNameBuffer with "\\?\UNC" or "\\?\" */
		memmove((void *)(fullPathNameBuffer + startOfPathInBuffer), fullPathNameBuffer, fullPathNameLengthInWcharts * sizeof(wchar_t));
		wcsncpy(fullPathNameBuffer, pathPrefix, lengthOfPathPrefix);

		pathName = fullPathNameBuffer;

cleanup:
		if (unicodeBuffer != unicodePath) {
			portLibrary->mem_free_memory(portLibrary, unicodePath);
		}

	}

	return pathName;
}

int32_t
omrfile_attr(struct OMRPortLibrary *portLibrary, const char *path)
{
	DWORD result;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;

	Trc_PRT_file_attr_Entry(path);

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return -1;
	}

	result = GetFileAttributesW(unicodePath);
	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}

	if (result == INVALID_FILE_ATTRIBUTES) {
		int32_t setError;
		result = GetLastError();
		setError = portLibrary->error_set_last_error(portLibrary, result, findError(result));
		Trc_PRT_file_attr_ExitFail(setError);
		return setError;
	}

	if (result & FILE_ATTRIBUTE_DIRECTORY) {
		Trc_PRT_file_attr_ExitDir(EsIsDir);
		return EsIsDir;
	}

	/* otherwise assume it's a normal file */
	Trc_PRT_file_attr_ExitFile(EsIsFile);
	return EsIsFile;
}

#define J9_FILE_MODE_READABLE 0444
#define J9_FILE_MODE_WRITABLE 0222
int32_t
omrfile_chmod(struct OMRPortLibrary *portLibrary, const char *path, int32_t mode)
{
	int32_t attrs;
	int32_t result;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	int32_t actualMode = J9_FILE_MODE_READABLE;

	Trc_PRT_file_chmod_Entry(path, mode);
	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		Trc_PRT_file_chmod_Exit(-1);
		return -1;
	}

	attrs = GetFileAttributesW(unicodePath);
	if (attrs == INVALID_FILE_ATTRIBUTES) {
		int32_t setError;
		result = GetLastError();
		setError = portLibrary->error_set_last_error(portLibrary, result, findError(result));
		Trc_PRT_file_chmod_getAttributesFailed(setError);
		if (unicodeBuffer != unicodePath) {
			portLibrary->mem_free_memory(portLibrary, unicodePath);
		}
		Trc_PRT_file_chmod_Exit(-1);
		return -1;
	}
	if (J9_FILE_MODE_WRITABLE == (J9_FILE_MODE_WRITABLE & mode)) {
		/* If all of the write-enable bits in mode are set, then turn off READONLY */
		attrs = attrs & ~FILE_ATTRIBUTE_READONLY;
		actualMode |= J9_FILE_MODE_WRITABLE;
	} else {
		attrs = attrs | FILE_ATTRIBUTE_READONLY;
	}
	result = SetFileAttributesW(unicodePath, attrs);
	if (0 == result) {
		int32_t setError;
		result = GetLastError();
		setError = portLibrary->error_set_last_error(portLibrary, result, findError(result));
		Trc_PRT_file_chmod_setAttributesFailed(setError);
		actualMode = -1;
	}
	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}
	Trc_PRT_file_chmod_Exit(actualMode);
	return actualMode;
}
#undef J9_FILE_MODE_READABLE
#undef J9_FILE_MODE_WRITABLE

intptr_t
omrfile_chown(struct OMRPortLibrary *portLibrary, const char *path, uintptr_t owner, uintptr_t group)
{
	return 0;
}

int32_t
omrfile_close(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	HANDLE handle = toHandle(portLibrary, fd);

	Trc_PRT_file_close_Entry(fd);

	if ((intptr_t) INVALID_HANDLE_VALUE == fd) {
		/* RTC 100203 Windows 10 does not report an error when closing an invalid handle */
		portLibrary->error_set_last_error(portLibrary, 0, OMRPORT_ERROR_FILE_INVALID_PARAMETER);
		Trc_PRT_file_close_invalidFileHandle();
		return -1;
	} else if (TRUE == CloseHandle(handle)) {
		Trc_PRT_file_close_Exit(0);
		return 0;
	} else {
		int32_t error = GetLastError();
		error = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		Trc_PRT_file_close_ExitFail(error);
		return -1;
	}
}

const char *
omrfile_error_message(struct OMRPortLibrary *portLibrary)
{
	return portLibrary->error_last_error_message(portLibrary);
}

void
omrfile_findclose(struct OMRPortLibrary *portLibrary, uintptr_t findhandle)
{
	int32_t error = 0;

	Trc_PRT_file_findclose_Entry(findhandle);
	if (0 == FindClose((HANDLE)findhandle)) {
		error = GetLastError();
		error = portLibrary->error_set_last_error(portLibrary, error, findError(error));
	}
	Trc_PRT_file_findclose_Exit(error);
}

uintptr_t
omrfile_findfirst(struct OMRPortLibrary *portLibrary, const char *path, char *resultbuf)
{
	WIN32_FIND_DATAW findFileData;
	char newPath[EsMaxPath + 1];
	HANDLE result;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;

	Trc_PRT_file_findfirst_Entry2(path, resultbuf);

	strcpy(newPath, path);
	strcat(newPath, "*");

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, newPath, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		Trc_PRT_file_findfirst_ExitFail(-1);
		return (uintptr_t)-1;
	}

	result = FindFirstFileW(unicodePath, &findFileData);
	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}
	if (result == INVALID_HANDLE_VALUE) {
		int32_t error = GetLastError();
		error = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		Trc_PRT_file_findfirst_ExitFail(error);
		return (uintptr_t)-1;
	}

	/* Assume a successful conversion. The data may be truncated to fit. */
	port_convertToUTF8(portLibrary, findFileData.cFileName, resultbuf, EsMaxPath);
	Trc_PRT_file_findfirst_Exit((uintptr_t)result);
	return (uintptr_t) result;
}

int32_t
omrfile_findnext(struct OMRPortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf)
{
	WIN32_FIND_DATAW findFileData;
	Trc_PRT_file_findnext_Entry2(findhandle, resultbuf);
	if (!FindNextFileW((HANDLE)findhandle, &findFileData)) {
		int32_t error = GetLastError();
		error = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		Trc_PRT_file_findnext_ExitFail(error);
		return -1;
	}

	/* Assume a successful conversion. The data may be truncated to fit. */
	port_convertToUTF8(portLibrary, findFileData.cFileName, resultbuf, EsMaxPath);
	Trc_PRT_file_findnext_Exit(0);
	return 0;
}

int64_t
omrfile_lastmod(struct OMRPortLibrary *portLibrary, const char *path)
{
	int64_t result = -1;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;

	Trc_PRT_file_lastmod_Entry(path);

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL != unicodePath) {
		WIN32_FILE_ATTRIBUTE_DATA myStat;
		if (0 == GetFileAttributesExW(unicodePath, GetFileExInfoStandard, &myStat)) {
			int32_t error = GetLastError();
			result = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		} else {
			/*
			 * Search MSDN for 'Converting a time_t Value to a File Time' for following implementation.
			 */
			result = ((int64_t) myStat.ftLastWriteTime.dwHighDateTime << 32) | (int64_t) myStat.ftLastWriteTime.dwLowDateTime;
			result = (result - 116444736000000000) / 10000;
		}

		if (unicodeBuffer != unicodePath) {
			portLibrary->mem_free_memory(portLibrary, unicodePath);
		}
	}

	Trc_PRT_file_lastmod_Exit(result);
	return result;
}

int64_t
omrfile_length(struct OMRPortLibrary *portLibrary, const char *path)
{
	int64_t result = -1;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;

	Trc_PRT_file_length_Entry(path);

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL != unicodePath) {
		WIN32_FILE_ATTRIBUTE_DATA myStat;
		if (0 == GetFileAttributesExW(unicodePath, GetFileExInfoStandard, &myStat)) {
			int32_t error = GetLastError();
			result = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		} else {
			result = ((int64_t)myStat.nFileSizeHigh) << 32;
			result += (int64_t)myStat.nFileSizeLow;
		}

		if (unicodeBuffer != unicodePath) {
			portLibrary->mem_free_memory(portLibrary, unicodePath);
		}
	}

	Trc_PRT_file_length_Exit(result);
	return result;
}

int64_t
omrfile_flength(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	int32_t result = 0;
	LARGE_INTEGER length;

	Trc_PRT_file_flength_Entry(fd);

	result = GetFileSizeEx((HANDLE)fd, &length);
	if (0 == result) {
		int32_t error = GetLastError();
		result = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		Trc_PRT_file_flength_ExitFail(result);
	}

	Trc_PRT_file_flength_Exit(length.QuadPart);
	return length.QuadPart;
}

int32_t
omrfile_mkdir(struct OMRPortLibrary *portLibrary, const char *path)
{
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	int32_t result = 0;

	Trc_PRT_file_mkdir_entry2(path);
	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		result = -1;
	} else {

		result = CreateDirectoryW(unicodePath, 0);
		if (unicodeBuffer != unicodePath) {
			portLibrary->mem_free_memory(portLibrary, unicodePath);
		}
		if (0 == result) {
			int32_t error = GetLastError();
			result = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		} else {
			result = 0;
		}
	}
	Trc_PRT_file_mkdir_exit2(result);
	return result;
}

int32_t
omrfile_move(struct OMRPortLibrary *portLibrary, const char *pathExist, const char *pathNew)
{
	wchar_t unicodeBufferExist[UNICODE_BUFFER_SIZE], *unicodePathExist;
	wchar_t unicodeBufferNew[UNICODE_BUFFER_SIZE], *unicodePathNew;
	BOOL result;

	/* Convert the filenames from UTF8 to Unicode */
	unicodePathExist = file_get_unicode_path(portLibrary, pathExist, unicodeBufferExist, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePathExist) {
		return -1;
	}
	unicodePathNew = file_get_unicode_path(portLibrary, pathNew, unicodeBufferNew, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePathNew) {
		if (unicodeBufferExist != unicodePathExist) {
			portLibrary->mem_free_memory(portLibrary, unicodePathExist);
		}
		return -1;
	}

	result = MoveFileW(unicodePathExist, unicodePathNew);
	if (unicodeBufferExist != unicodePathExist) {
		portLibrary->mem_free_memory(portLibrary, unicodePathExist);
	}
	if (unicodeBufferNew != unicodePathNew) {
		portLibrary->mem_free_memory(portLibrary, unicodePathNew);
	}

	if (!result) {
		int32_t error = GetLastError();
		portLibrary->error_set_last_error(portLibrary, error, findError(error));
		return -1;
	}
	return 0;
}

intptr_t
omrfile_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	DWORD	accessMode, shareMode, createMode, flagsAndAttributes;
	HANDLE	aHandle;
	int32_t error = 0;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	SECURITY_ATTRIBUTES sAttrib;

	Trc_PRT_file_open_Entry(path, flags, mode);

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return -1;
	}

	accessMode = 0;
	if (flags & EsOpenRead) {
		accessMode |= GENERIC_READ;
	}
	if (flags & EsOpenWrite) {
		accessMode |= GENERIC_WRITE;
	}

	shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

	/* this flag allows files to be deleted/renamed while they are still open, which more in line with unix semantics */
	if (OMR_ARE_ALL_BITS_SET(flags, EsOpenShareDelete)) {
		shareMode = shareMode | FILE_SHARE_DELETE;
	}

	if ((flags & EsOpenCreate) == EsOpenCreate) {
		createMode = OPEN_ALWAYS;
	} else if ((flags & EsOpenCreateNew) == EsOpenCreateNew) {
		createMode = CREATE_NEW;
	} else if ((flags & EsOpenCreateAlways) == EsOpenCreateAlways) {
		createMode = CREATE_ALWAYS;
	} else {
		createMode = OPEN_EXISTING;
	}

	flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	if (flags & EsOpenSync) {
		flagsAndAttributes |= FILE_FLAG_WRITE_THROUGH;
	}

	if (flags & EsOpenAsynchronous) {
		flagsAndAttributes |= FILE_FLAG_OVERLAPPED;
	}

	if (flags & EsOpenForInherit) {
		ZeroMemory(&sAttrib, sizeof(sAttrib));
		sAttrib.bInheritHandle = 1;
		sAttrib.nLength = sizeof(sAttrib);
		aHandle = CreateFileW(unicodePath, accessMode, shareMode, &sAttrib, createMode, flagsAndAttributes, NULL);
	} else {
		aHandle = CreateFileW(unicodePath, accessMode, shareMode, NULL, createMode, flagsAndAttributes, NULL);
	}

	if (INVALID_HANDLE_VALUE == aHandle) {
		error = GetLastError();
		portLibrary->error_set_last_error(portLibrary, error, findError(error));
		if (unicodeBuffer != unicodePath) {
			portLibrary->mem_free_memory(portLibrary, unicodePath);
		}
		Trc_PRT_file_open_Exception2(path, error, findError(error));
		Trc_PRT_file_open_Exit(-1);
		portLibrary->error_set_last_error(portLibrary, error, findError(error));
		return -1;
	}

	if ((FILE_TYPE_DISK == GetFileType(aHandle)) && ((flags & EsOpenTruncate) == EsOpenTruncate)) {
		if (0 == CloseHandle(aHandle)) {
			error = GetLastError();
			portLibrary->error_set_last_error(portLibrary, error, findError(error)); /* continue */
		}

		aHandle = CreateFileW(unicodePath, accessMode, shareMode, NULL, TRUNCATE_EXISTING, flagsAndAttributes, NULL);
		if (INVALID_HANDLE_VALUE == aHandle) {
			error = GetLastError();
			portLibrary->error_set_last_error(portLibrary, error, findError(error));
			if (unicodeBuffer != unicodePath) {
				portLibrary->mem_free_memory(portLibrary, unicodePath);
			}
			Trc_PRT_file_open_Exception3(path, error, findError(error));
			Trc_PRT_file_open_Exit(-1);
			portLibrary->error_set_last_error(portLibrary, error, findError(error));
			return -1;
		}
	}

	if (flags & EsOpenAppend) {
		portLibrary->file_seek(portLibrary, (intptr_t)aHandle, 0, EsSeekEnd);
	}

	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}

	Trc_PRT_file_open_Exit(aHandle);
	return ((intptr_t)aHandle);
}

intptr_t
omrfile_read(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	DWORD bytesRead;
	HANDLE handle;
	int32_t errorCode = 0;

	Trc_PRT_file_read_Entry(fd, buf, nbytes);

	if (OMRPORT_TTY_IN == fd) {
		handle = PPG_tty_consoleInputHd;
	} else {
		handle = (HANDLE)fd;
	}

	if (0 == nbytes) {
		Trc_PRT_file_read_Exit(0);
		return 0;
	}

	/* ReadFile limits the buffer to DWORD size and returns the number of bytes read,
	 * 	so the (DWORD)nbytes cast is functionally correct, though less than ideal.
	 * If the caller wants to read more than 2^32 bytes they will have to make more than one call,
	 * 	but the logic of the calling code should already account for this.
	 */
	if (FALSE == ReadFile(handle, buf, (DWORD)nbytes, &bytesRead, NULL)) {
		int32_t error = GetLastError();
		errorCode = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		Trc_PRT_file_read_Exit(errorCode);
		return -1;
	}
	if (0 == bytesRead) {
		errorCode = portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_FILE_READ_NO_BYTES_READ);
		Trc_PRT_file_read_Exit(errorCode);
		return -1;
	}

	Trc_PRT_file_read_Exit(bytesRead);

	return bytesRead;
}

int64_t
omrfile_seek(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t offset, int32_t whence)
{
	DWORD moveMethod;
	LARGE_INTEGER liOffset;
	int64_t result;
	int32_t error;

	Trc_PRT_file_seek_Entry(fd, offset, whence);
	liOffset.QuadPart = offset;

	if ((whence < EsSeekSet) || (whence > EsSeekEnd)) {
		Trc_PRT_file_seek_Exit(-1);
		return -1;
	}
	if (whence == EsSeekSet) {
		moveMethod = FILE_BEGIN;
	}
	if (whence == EsSeekEnd)	{
		moveMethod = FILE_END;
	}
	if (whence == EsSeekCur) {
		moveMethod = FILE_CURRENT;
	}
	liOffset.LowPart = SetFilePointer((HANDLE)fd, liOffset.LowPart, &liOffset.HighPart, moveMethod);
	if (INVALID_SET_FILE_POINTER == liOffset.LowPart) {
		error = GetLastError();
		if (error != NO_ERROR) {
			portLibrary->error_set_last_error(portLibrary, error, findError(error));
			Trc_PRT_file_seek_Exit(-1);
			return -1;
		}
	}

	result = (int64_t)liOffset.QuadPart;

	Trc_PRT_file_seek_Exit(result);

	return result;
}

void
omrfile_shutdown(struct OMRPortLibrary *portLibrary)
{
}

int32_t
omrfile_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

int32_t
omrfile_sync(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	HANDLE handle = toHandle(portLibrary, fd);

	Trc_PRT_file_sync_Entry(fd);

	/*
	 * According to MSDN:
	 * 1) If hFile is a handle to the server end of a named pipe, the function does not return until the client has read all buffered data from the pipe.
	 * 2) The function fails if hFile is a handle to the console output. That is because the console output is not buffered. The function returns FALSE, and GetLastError returns ERROR_INVALID_HANDLE.
	 *
	 * This behaviour caused JAZZ 44899 defect: Test_SimpleTenantLimitXmt would freeze while waiting for process to terminate while child
	 * was waiting for parent to read from STDIN and STDOUT streams of a child.
	 *
	 * Manual testing showed that even if we called System.out.print('X') and System.err.print('Y') a parent process would still get receive 'X' and 'Y'
	 * (with and without flushing) after child process in terminated.
	 *
	 * This check does not modify original behaviour of the port library because we never actually flushed STDIN and STDOUT handles but
	 * some arbitrary handles 1 & 2.
	 */
	if (((HANDLE)OMRPORT_TTY_OUT == (HANDLE)fd) || ((HANDLE)OMRPORT_TTY_ERR == (HANDLE)fd)) {
		Trc_PRT_file_sync_Exit(0);
		return 0;
	}

	if (FlushFileBuffers(handle)) {
		Trc_PRT_file_sync_Exit(0);
		return 0;
	} else {
		int32_t error = GetLastError();
		error = portLibrary->error_set_last_error(portLibrary, error, findError(error));
		Trc_PRT_file_sync_Exit(error);
		return -1;
	}
}

int32_t
omrfile_unlink(struct OMRPortLibrary *portLibrary, const char *path)
{
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	BOOL result;

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return -1;
	}

	/*PR 93036 - should be able to delete read-only files, so we set the file attribute back to normal*/
	if (0 == SetFileAttributesW(unicodePath, FILE_ATTRIBUTE_NORMAL)) {
		int32_t error = GetLastError();
		portLibrary->error_set_last_error(portLibrary, error, findError(error));	 /* continue */
	}

	result = DeleteFileW(unicodePath);
	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}

	if (!result) {
		int32_t error = GetLastError();
		portLibrary->error_set_last_error(portLibrary, error, findError(error));
		return -1;
	}
	return 0;
}

int32_t
omrfile_unlinkdir(struct OMRPortLibrary *portLibrary, const char *path)
{
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	BOOL result;

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return -1;
	}

	/*PR 93036 - should be able to delete read-only dirs, so we set the file attribute back to normal*/
	if (0 == SetFileAttributesW(unicodePath, FILE_ATTRIBUTE_NORMAL)) {
		int32_t error = GetLastError();
		portLibrary->error_set_last_error(portLibrary, error, findError(error));	 /* continue */
	}

	result = RemoveDirectoryW(unicodePath);
	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}

	if (!result) {
		int32_t error = GetLastError();
		portLibrary->error_set_last_error(portLibrary, error, findError(error));
		return -1;
	}
	return 0;
}

void
omrfile_vprintf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, va_list args)
{
	char localBuffer[256];
	char *writeBuffer = NULL;
	uintptr_t bufferSize = 0;
	uintptr_t stringSize = 0;

	/* str_vprintf(.., NULL, ..) result is size of buffer required including the null terminator */
	bufferSize = portLibrary->str_vprintf(portLibrary, NULL, 0, format, args);

	/* use local buffer if possible, allocate a buffer from system memory if local buffer not large enough */
	if (sizeof(localBuffer) >= bufferSize) {
		writeBuffer = localBuffer;
	} else {
		writeBuffer = portLibrary->mem_allocate_memory(portLibrary, bufferSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	}

	/* format and write out the buffer (truncate into local buffer as last resort) without null terminator */
	if (NULL != writeBuffer) {
		stringSize = portLibrary->str_vprintf(portLibrary, writeBuffer, bufferSize, format, args);
		portLibrary->file_write_text(portLibrary, fd, writeBuffer, stringSize);
		/* dispose of buffer if not on local */
		if (writeBuffer != localBuffer) {
			portLibrary->mem_free_memory(portLibrary, writeBuffer);
		}
	} else {
		portLibrary->nls_printf(portLibrary, J9NLS_ERROR, J9NLS_PORT_FILE_MEMORY_ALLOCATE_FAILURE);
		stringSize = portLibrary->str_vprintf(portLibrary, localBuffer, sizeof(localBuffer), format, args);
		portLibrary->file_write_text(portLibrary, fd, localBuffer, stringSize);
	}
}

intptr_t
omrfile_write(struct OMRPortLibrary *portLibrary, intptr_t fd, const void *buf, intptr_t nbytes)
{
	DWORD	nCharsWritten;
	intptr_t toWrite, offset = 0;
	int32_t errorCode;
	HANDLE handle;

	Trc_PRT_file_write_Entry(fd, buf, nbytes);

	if (OMRPORT_TTY_OUT == fd) {
		handle = PPG_tty_consoleOutputHd;
	} else if (OMRPORT_TTY_ERR == fd) {
		handle = PPG_tty_consoleErrorHd;
	} else {
		handle = (HANDLE)fd;
	}

	toWrite = nbytes;
	while (nbytes > 0) {
		if (toWrite > nbytes) {
			toWrite = nbytes;
		}
		/* The (DWORD)nbytes downcasts can make toWrite smaller than nbytes, however, the while loop
		 *  ensures that nbytes of data is still written */
		if (FALSE == WriteFile(handle, (char *)buf + offset, (DWORD)toWrite, &nCharsWritten, NULL)) {
			errorCode = GetLastError();
			if (ERROR_NOT_ENOUGH_MEMORY == errorCode) {
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
			errorCode = portLibrary->error_set_last_error(portLibrary, errorCode, findError(errorCode));
			Trc_PRT_file_write_Exit(errorCode);
			return errorCode;
		}
		offset += nCharsWritten;
		nbytes -= nCharsWritten;
	}

	Trc_PRT_file_write_Exit(offset);

	return offset;
}

void
omrfile_printf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, ...)
{
	va_list args;

	Trc_PRT_file_printf_Entry(fd, format);

	va_start(args, format);
	portLibrary->file_vprintf(portLibrary, fd, format, args);
	va_end(args);

	Trc_PRT_file_printf_Exit();
}

int32_t
omrfile_set_length(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength)
{
	int32_t lastError = 0;
	int64_t currentFilePtr = 0;

	Trc_PRT_file_setlength_Entry(fd, newLength);

	/* Save current file pointer location */
	currentFilePtr = omrfile_seek(portLibrary, fd, 0, EsSeekCur);

	if ((-1 != currentFilePtr) && (-1 != omrfile_seek(portLibrary, fd, newLength, FILE_BEGIN))) {
		if (0 == SetEndOfFile((HANDLE)fd)) {
			lastError = GetLastError();
			lastError =  portLibrary->error_set_last_error(portLibrary, lastError, findError(lastError));
			Trc_PRT_file_setlength_Exit(lastError);
			return lastError;
		}

		/* Put pointer back to where it started */
		if (-1 != omrfile_seek(portLibrary, fd, currentFilePtr, EsSeekSet)) {
			Trc_PRT_file_setlength_Exit(0);
			return 0;
		}
	}

	/* omrfile_seek already set the last error */
	lastError = portLibrary->error_last_error_number(portLibrary);
	Trc_PRT_file_setlength_Exit(lastError);
	return lastError;
}

int32_t
omrfile_lock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length)
{
	return omrfile_lock_bytes_helper(portLibrary, fd, lockFlags, offset, length, FALSE);
}


int32_t
omrfile_unlock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length)
{
	return omrfile_unlock_bytes_helper(portLibrary, fd, offset, length, FALSE);
}

int32_t
omrfile_fstat(struct OMRPortLibrary *portLibrary, intptr_t fd, struct J9FileStat *buf)
{
	/* TODO: Use GetFileInformationByHandleEx() to get file stats */
	return -1;
}

int32_t
omrfile_stat(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStat *buf)
{
	DWORD result;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;

	if (NULL == path || NULL == buf) {
		return -1;
	}

	memset(buf, 0, sizeof(*buf));

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return -1;
	}

	result = GetFileAttributesW(unicodePath);
	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}

	if (result == 0xFFFFFFFF) {
		result = GetLastError();
		return portLibrary->error_set_last_error(portLibrary, result, findError(result));
	}

	if (result & FILE_ATTRIBUTE_DIRECTORY) {
		buf->isDir = 1;
	} else {
		/* otherwise assume it's a normal file */
		buf->isFile = 1;
	}

	if (result & FILE_ATTRIBUTE_READONLY) {
		buf->perm.isUserWriteable  = 0;
		buf->perm.isGroupWriteable = 0;
		buf->perm.isOtherWriteable = 0;

	} else {
		buf->perm.isUserWriteable  = 1;
		buf->perm.isGroupWriteable = 1;
		buf->perm.isOtherWriteable = 1;
	}
	buf->perm.isUserReadable  = 1;
	buf->perm.isGroupReadable = 1;
	buf->perm.isOtherReadable = 1;

	{
		wchar_t driveBuffer[UNICODE_BUFFER_SIZE];

		result = GetFullPathNameW(unicodePath, UNICODE_BUFFER_SIZE, driveBuffer, NULL);
		if (result == 0 || result > UNICODE_BUFFER_SIZE) {
			result = GetLastError();
			return portLibrary->error_set_last_error(portLibrary, result, findError(result));
		}
		driveBuffer[3] = '\0'; /* Chop off everything after the initial X:\ */
		switch (GetDriveTypeW(driveBuffer)) {
		case DRIVE_REMOVABLE:
			buf->isRemovable = 1;
			break;
		case DRIVE_REMOTE:
			buf->isRemote = 1;
			break;
		default:
			buf->isFixed = 1;
			break;
		}
	}

	return 0;
}

int32_t
omrfile_stat_filesystem(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStatFilesystem *buf)
{
	DWORD result;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodePath;
	wchar_t driveBuffer[UNICODE_BUFFER_SIZE];
	if (NULL == path || NULL == buf) {
		return -1;
	}

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(portLibrary, path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return -1;
	}

	result = GetFullPathNameW(unicodePath, UNICODE_BUFFER_SIZE, driveBuffer, NULL);

	if (unicodeBuffer != unicodePath) {
		portLibrary->mem_free_memory(portLibrary, unicodePath);
	}

	if (result == 0 || result > UNICODE_BUFFER_SIZE) {
		result = GetLastError();
		return portLibrary->error_set_last_error(portLibrary, result, findError(result));
	}
	driveBuffer[3] = '\0'; /* Chop off everything after the initial X:\ */

	result = GetDiskFreeSpaceExW(driveBuffer,
								 (PULARGE_INTEGER)&buf->freeSizeBytes,
								 (PULARGE_INTEGER)&buf->totalSizeBytes,
								 NULL);
	if (0 == result) {
		result = GetLastError();
		return portLibrary->error_set_last_error(portLibrary, result, findError(result));
	}
	return 0;
}

intptr_t
omrfile_convert_native_fd_to_omrfile_fd(struct OMRPortLibrary *portLibrary, intptr_t nativeFD)
{
	return fromHandle(portLibrary, (HANDLE) nativeFD);
}

intptr_t
omrfile_convert_omrfile_fd_to_native_fd(struct OMRPortLibrary *portLibrary, intptr_t omrfileFD)
{
	return (intptr_t) toHandle(portLibrary, omrfileFD);
}
