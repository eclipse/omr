/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#if defined(LINUX)
#include <sys/vfs.h>
#elif defined(OSX)
#include <sys/param.h>
#include <sys/mount.h>
#endif /* defined(LINUX) */
#include <string.h>

#if defined(WIN32)
#include <direct.h>
#define J9FILE_UNC_EXTENDED_LENGTH_PREFIX (L"\\\\?\\")
#else /* defined(WIN32) */
#include <sys/types.h>
#include <sys/statvfs.h>
#include <dirent.h>
#endif /* defined(WIN32)*/

#include "Port.hpp"


#if defined(WIN32)
RCType
Port::omrfile_findfirst(const char *path, char **resultbuf, intptr_t *handle)
{
	WIN32_FIND_DATAW findFileData;
	char *newPath = NULL;
	HANDLE result = INVALID_HANDLE_VALUE;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE];
	wchar_t *unicodePath = NULL;

	int newPathLen = snprintf(NULL, 0, "%s/*.*", path) + 1;
	newPath = (char *)Port::omrmem_malloc(newPathLen);
	if (NULL == newPath) {
		goto failed;
	}
	snprintf(newPath, newPathLen, "%s/*.*", path);

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(newPath, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		goto failed;
	}
	Port::omrmem_free((void **)&newPath);

	result = FindFirstFileW(unicodePath, &findFileData);
	if (INVALID_HANDLE_VALUE == result) {
		goto failed;
	}

	if (RC_OK != convertToUTF8(findFileData.cFileName, resultbuf)) {
		goto failed;
	}
	*handle = (intptr_t) result;

	if (unicodePath != unicodeBuffer) {
		Port::omrmem_free((void **)&unicodePath);
	}
	return RC_OK;
failed:
	Port::omrmem_free((void **)&newPath);
	if (unicodePath != unicodeBuffer) {
		Port::omrmem_free((void **)&unicodePath);
	}
	return RC_FAILED;
}

RCType
Port::omrfile_findclose(intptr_t findhandle)
{
	RCType rc = RC_FAILED;
	if (0 != FindClose((HANDLE) findhandle)) {
		rc = RC_OK;
	}
	return rc;
}

RCType
Port::omrfile_findnext(intptr_t findhandle, char **resultbuf)
{
	WIN32_FIND_DATAW findFileData;
	if (!FindNextFileW((HANDLE) findhandle, &findFileData)) {
		goto failed;
	}

	if (RC_OK != convertToUTF8(findFileData.cFileName, resultbuf)) {
		goto failed;
	}
	return RC_OK;
failed:
	return RC_FAILED;
}

wchar_t *
Port::file_get_unicode_path(const char *utf8Path, wchar_t *unicodeBuffer, unsigned int unicodeBufferSize)
{
	wchar_t *unicodePath = NULL;
	wchar_t *pathName = NULL;
	size_t pathLen;

	unicodePath = convertFromUTF8(utf8Path, unicodeBuffer, unicodeBufferSize);

	if (NULL == unicodePath) {
		return NULL;
	}

	pathLen = wcslen(unicodePath);

	/* If the file name is longer than (the system-defined) EsMaxPath characters we need to prepend it with \\?\  */
	if (pathLen * sizeof(wchar_t) <= EsMaxPath) {
		pathName = unicodePath;
	} else {
		DWORD getFullPathNameRc;
		DWORD fullPathNameLengthInWcharts;
		wchar_t *fullPathNameBuffer = NULL;
		size_t fullPathNameBufferSizeBytes;

		/*
		 * We need to prepend the path with "\\?\", but this format is only supported if the path is fully qualified.
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

		fullPathNameBufferSizeBytes = fullPathNameLengthInWcharts * sizeof(wchar_t) + wcslen(J9FILE_UNC_EXTENDED_LENGTH_PREFIX) * sizeof(wchar_t) + sizeof(wchar_t) /* null terminator */ ;

		fullPathNameBuffer = (wchar_t *)Port::omrmem_malloc(fullPathNameBufferSizeBytes);
		if (NULL == fullPathNameBuffer) {
			pathName = NULL;
			goto cleanup;
		}

		getFullPathNameRc = GetFullPathNameW(unicodePath, fullPathNameLengthInWcharts , fullPathNameBuffer, NULL);
		if (0 == getFullPathNameRc) {
			pathName = NULL;
			goto cleanup;
		}

		/*  now prepend the fullPathNameBuffer with "\\?\" */
		memmove(fullPathNameBuffer + wcslen(J9FILE_UNC_EXTENDED_LENGTH_PREFIX), fullPathNameBuffer, fullPathNameLengthInWcharts * sizeof(wchar_t));
		wcsncpy(fullPathNameBuffer, J9FILE_UNC_EXTENDED_LENGTH_PREFIX, wcslen(J9FILE_UNC_EXTENDED_LENGTH_PREFIX));

		pathName = fullPathNameBuffer;

cleanup:
		if (unicodeBuffer != unicodePath) {
			Port::omrmem_free((void **)&unicodePath);
		}
	}
	return pathName;
}

wchar_t *
Port::convertFromUTF8(const char *string, wchar_t *unicodeBuffer, unsigned int unicodeBufferSize)
{
	wchar_t *unicodeString = NULL;
	size_t length;

	if (NULL == string) {
		return NULL;
	}
	length = strlen(string);

	if (length < unicodeBufferSize) {
		unicodeString = unicodeBuffer;
	} else {
		unicodeString = (wchar_t *)Port::omrmem_malloc((length + 1) * 2);
		if (NULL == unicodeString) {
			return NULL;
		}
	}

	if (0 == MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, string, -1, unicodeString, (int)length + 1)) {
		if (unicodeString != unicodeBuffer) {
			Port::omrmem_free((void **)&unicodeString);
		}
		return NULL;
	}

	return unicodeString;
}

RCType
Port::convertToUTF8(const wchar_t *unicodeString, char **utf8Buffer)
{
	int sizeNeeded = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeString, -1, NULL, 0, NULL, NULL);
	if (0 == sizeNeeded) {
		goto failed;
	}
	*utf8Buffer = (char *)Port::omrmem_malloc(sizeNeeded);
	if (NULL == *utf8Buffer) {
		goto failed;
	}

	if (0 == WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeString, -1, *utf8Buffer, sizeNeeded, NULL, NULL)) {
		goto failed;
	}
	return RC_OK;
failed:
	Port::omrmem_free((void **)utf8Buffer);
	return RC_FAILED;
}

FILE *
Port::fopen(const char *path, const char *mode)
{
	wchar_t unicodePathBuffer[UNICODE_BUFFER_SIZE];
	wchar_t unicodeModeBuffer[UNICODE_BUFFER_SIZE];
	wchar_t *unicodePath = NULL;
	wchar_t *unicodeMode = NULL;
	FILE *result = NULL;

	unicodePath = file_get_unicode_path(path, unicodePathBuffer, UNICODE_BUFFER_SIZE);
	unicodeMode = convertFromUTF8(mode, unicodeModeBuffer, UNICODE_BUFFER_SIZE);

	result = _wfopen(unicodePath, unicodeMode);

	if (unicodePath != unicodePathBuffer) {
		Port::omrmem_free((void **)&unicodePath);
	}
	if (unicodeMode != unicodeModeBuffer) {
		Port::omrmem_free((void **)&unicodeMode);
	}
	return result;
}

RCType
Port::omrfile_stat(const char *path, unsigned int flags, struct J9FileStat *buf)
{
	DWORD result;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE];
	wchar_t *unicodePath;

	if (NULL == path || NULL == buf) {
		return RC_FAILED;
	}

	memset(buf, 0, sizeof(*buf));

	/* Convert the filename from UTF8 to Unicode */
	unicodePath = file_get_unicode_path(path, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePath) {
		return RC_FAILED;
	}

	result = GetFileAttributesW(unicodePath);
	if (unicodeBuffer != unicodePath) {
		Port::omrmem_free((void **)&unicodePath);
	}

	if (INVALID_FILE_ATTRIBUTES == result) {
		return RC_FAILED;
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
			return RC_FAILED;
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

	return RC_OK;
}

RCType
Port::stat(const char *path, dirstat *statbuf)
{
	RCType rc = RC_FAILED;
	wchar_t unicodePathBuffer[UNICODE_BUFFER_SIZE];
	wchar_t *unicodePath = NULL;

	unicodePath = Port::file_get_unicode_path(path, unicodePathBuffer, UNICODE_BUFFER_SIZE);
	if (0 == _wstat(unicodePath, statbuf)) {
		rc = RC_OK;
	}

	if (unicodePath != unicodePathBuffer) {
		Port::omrmem_free((void **)&unicodePath);
	}

	return rc;
}

char *
Port::omrfile_getcwd()
{
	char *result = NULL;
	int bufSize = EsMaxPath;
	wchar_t *unicodeBuffer = NULL;

	while (true) {
		unicodeBuffer = (wchar_t *)Port::omrmem_malloc(bufSize * sizeof(wchar_t));
		if (NULL == unicodeBuffer) {
			goto done;
		}
		if (NULL != _wgetcwd(unicodeBuffer, bufSize)) {
			char *resultBuffer = NULL;
			if (RC_OK != convertToUTF8(unicodeBuffer, &resultBuffer)) {
				goto done;
			}
			result = resultBuffer;
			break;
		} else if (ERANGE == errno) {
			Port::omrmem_free((void **)&unicodeBuffer);
			bufSize = bufSize * 2;
		} else {
			Port::omrmem_free((void **)&unicodeBuffer);
			break;
		}
	}
done:
	Port::omrmem_free((void **)&unicodeBuffer);
	return result;
}

RCType
Port::omrfile_move(const char *pathExist, const char *pathNew)
{
	wchar_t unicodeBufferExist[UNICODE_BUFFER_SIZE];
	wchar_t *unicodePathExist = NULL;
	wchar_t unicodeBufferNew[UNICODE_BUFFER_SIZE];
	wchar_t *unicodePathNew = NULL;
	BOOL result;

	/* Convert the filenames from UTF8 to Unicode */
	unicodePathExist = file_get_unicode_path(pathExist, unicodeBufferExist, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePathExist) {
		return RC_FAILED;
	}
	unicodePathNew = file_get_unicode_path(pathNew, unicodeBufferNew, UNICODE_BUFFER_SIZE);
	if (NULL == unicodePathNew) {
		if (unicodeBufferExist != unicodePathExist) {
			Port::omrmem_free((void **)&unicodePathExist);
		}
		return RC_FAILED;
	}

	result = MoveFileW(unicodePathExist, unicodePathNew);
	if (unicodeBufferExist != unicodePathExist) {
		Port::omrmem_free((void **)&unicodePathExist);
	}
	if (unicodeBufferNew != unicodePathNew) {
		Port::omrmem_free((void **)&unicodePathNew);
	}

	if (!result) {
		return RC_FAILED;
	}
	return RC_OK;
}

char *
Port::omrfile_realpath(const char *utf8Path)
{
	wchar_t *unicodePath = NULL;
	wchar_t *pathName = NULL;
	size_t pathLen = 0;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE];
	char *resolved_path = NULL;

	DWORD getFullPathNameRc = 0;
	DWORD fullPathNameLengthInWcharts = 0;
	wchar_t *fullPathNameBuffer = NULL;
	size_t fullPathNameBufferSizeBytes = 0;

	unicodePath = convertFromUTF8(utf8Path, unicodeBuffer, UNICODE_BUFFER_SIZE);

	if (NULL == unicodePath) {
		return NULL;
	}

	pathLen = wcslen(unicodePath);

	/*
	 * We need to prepend the path with "\\?\", but this format is only supported if the path is fully qualified.
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

	fullPathNameBufferSizeBytes = fullPathNameLengthInWcharts * sizeof(wchar_t) + wcslen(J9FILE_UNC_EXTENDED_LENGTH_PREFIX) * sizeof(wchar_t) + sizeof(wchar_t) /* null terminator */ ;

	fullPathNameBuffer = (wchar_t *)Port::omrmem_malloc(fullPathNameBufferSizeBytes);
	if (NULL == fullPathNameBuffer) {
		pathName = NULL;
		goto cleanup;
	}

	getFullPathNameRc = GetFullPathNameW(unicodePath, fullPathNameLengthInWcharts , fullPathNameBuffer, NULL);
	if (0 == getFullPathNameRc) {
		pathName = NULL;
		goto cleanup;
	}
	/* If the file name is longer than (the system-defined) EsMaxPath characters we need to prepend it with \\?\  */
	if (pathLen * sizeof(wchar_t) > EsMaxPath) {
		/*  now prepend the fullPathNameBuffer with "\\?\" */
		memmove(fullPathNameBuffer + wcslen(J9FILE_UNC_EXTENDED_LENGTH_PREFIX), fullPathNameBuffer, fullPathNameLengthInWcharts * sizeof(wchar_t));
		wcsncpy(fullPathNameBuffer, J9FILE_UNC_EXTENDED_LENGTH_PREFIX, wcslen(J9FILE_UNC_EXTENDED_LENGTH_PREFIX));
	}

	pathName = fullPathNameBuffer;

	if (RC_OK != convertToUTF8(pathName, &resolved_path)) {
		goto cleanup;
	}
cleanup:
	if (unicodeBuffer != unicodePath) {
		Port::omrmem_free((void **)&unicodePath);
	}

	return resolved_path;
}
#else /* defined(WIN32) */

FILE *
Port::fopen(const char *path, const char *mode)
{
	return ::fopen(path, mode);
}

RCType
Port::omrfile_findfirst(const char *path, char **resultbuf, intptr_t *handle)
{
	RCType rc = RC_FAILED;
#if defined(AIXPPC)
	DIR64 *dirp = NULL;
#else /* defined(AIXPPC) */
	DIR *dirp = NULL;
#endif /* defined(AIXPPC) */

#if defined(AIXPPC)
	struct dirent64 *entry;
#else /* defined(AIXPPC) */
	struct dirent *entry;
#endif /* defined(AIXPPC) */

#if defined(AIXPPC)
	dirp = opendir64(path);
#else /* defined(AIXPPC) */
	dirp = opendir(path);
#endif /* defined(AIXPPC) */
	if (NULL == dirp) {
		return RC_FAILED;
	}
#if defined(AIXPPC)
	entry = readdir64(dirp);
#else /* defined(AIXPPC) */
	entry = readdir(dirp);
#endif /* defined(AIXPPC) */
	if (NULL == entry) {
#if defined(AIXPPC)
		closedir64(dirp);
#else /* defined(AIXPPC) */
		closedir(dirp);
#endif /* defined(AIXPPC) */
		return RC_FAILED;
	}

	*resultbuf = strdup(entry->d_name);
	if (NULL != *resultbuf) {
		*handle = (intptr_t)dirp;
		rc = RC_OK;
	}
	return rc;
}

RCType
Port::omrfile_findnext(intptr_t findhandle, char **resultbuf)
{
	RCType rc = RC_FAILED;
#if defined(AIXPPC)
	struct dirent64 *entry;
#else /* defined(AIXPPC) */
	struct dirent *entry;
#endif /* defined(AIXPPC) */

#if defined(AIXPPC)
	entry = readdir64((DIR64 *)findhandle);
#else /* defined(AIXPPC) */
	entry = readdir((DIR *)findhandle);
#endif /* defined(AIXPPC) */
	if (entry == NULL) {
		return RC_FAILED;
	}
	*resultbuf = strdup(entry->d_name);
	if (NULL != *resultbuf) {
		rc = RC_OK;
	}
	return rc;
}

RCType
Port::omrfile_findclose(intptr_t findhandle)
{
	RCType rc = RC_FAILED;
#if defined(AIXPPC)
	if (0 == closedir64((DIR64 *)findhandle)) {
#else /* defined(AIXPPC) */
	if (0 == closedir((DIR *)findhandle)) {
#endif /* defined(AIXPPC) */
		rc = RC_OK;
	}
	return rc;
}

RCType
Port::stat(const char *path, dirstat *statbuf)
{
	RCType rc = RC_FAILED;
	if (0 == ::stat(path, statbuf)) {
		rc = RC_OK;
	}
	return rc;
}

char *
Port::omrfile_getcwd()
{
	int bufSize = EsMaxPath;
	char *buffer = NULL;
	while (true) {
		buffer = (char *)Port::omrmem_malloc(bufSize);
		if (NULL == buffer) {
			break;
		}
		if (NULL != getcwd(buffer, bufSize)) {
			break;
		} else if (ERANGE == errno) {
			Port::omrmem_free((void **)&buffer);
			bufSize = bufSize * 2;
		} else {
			Port::omrmem_free((void **)&buffer);
			break;
		}
	}
	return buffer;
}

RCType
Port::omrfile_move(const char *pathExist, const char *pathNew)
{
	RCType rc = RC_FAILED;

	if (0 == rename(pathExist, pathNew)) {
		rc = RC_OK;
	}
	return rc;
}

RCType
Port::omrfile_stat(const char *path, unsigned int flags, struct J9FileStat *buf)
{
	struct stat statbuf;
#if defined(LINUX) || defined(OSX)
	struct statfs statfsbuf;
#elif defined(AIXPPC)
	struct statvfs statvfsbuf;
#endif /* defined(LINUX) || defined(OSX) */

	memset(buf, 0, sizeof(*buf));

	/* Neutrino does not handle NULL for stat */
	if (stat(path, &statbuf)) {
		return RC_FAILED;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		buf->isDir = 1;
	} else {
		buf->isFile = 1;
	}

	if (statbuf.st_mode & S_IWUSR) {
		buf->perm.isUserWriteable = 1;
	}
	if (statbuf.st_mode & S_IRUSR) {
		buf->perm.isUserReadable = 1;
	}
	if (statbuf.st_mode & S_IWGRP) {
		buf->perm.isGroupWriteable = 1;
	}
	if (statbuf.st_mode & S_IRGRP) {
		buf->perm.isGroupReadable = 1;
	}
	if (statbuf.st_mode & S_IWOTH) {
		buf->perm.isOtherWriteable = 1;
	}
	if (statbuf.st_mode & S_IROTH) {
		buf->perm.isOtherReadable = 1;
	}

	buf->ownerUid = statbuf.st_uid;
	buf->ownerGid = statbuf.st_gid;

#if defined(LINUX) || defined(OSX)
	if (statfs(path, &statfsbuf)) {
		return RC_FAILED;
	}
	switch (statfsbuf.f_type) {
	/* Detect remote filesystem types */
	case 0x6969: /* NFS_SUPER_MAGIC */
	case 0x517B: /* SMB_SUPER_MAGIC */
	case 0xFF534D42: /* CIFS_MAGIC_NUMBER */
		buf->isRemote = 1;
		break;
	default:
		buf->isFixed = 1;
		break;
	}
#elif defined(AIXPPC) && !defined(J9OS_I5)
	if (statvfs(path, &statvfsbuf)) {
		return RC_FAILED;
	}
	/* Detect NFS filesystem */
	if (NULL != strstr(statvfsbuf.f_basetype, "nfs")) {
		buf->isRemote = 1;
	} else {
		buf->isFixed = 1;
	}
#else /* defined(LINUX) || defined(OSX) */
	/* Assume we have a fixed file unless the platform can be more specific */
	buf->isFixed = 1;
#endif /* defined(LINUX) || defined(OSX) */

	return RC_OK;
}

char *
Port::omrfile_realpath(const char *path)
{
	char *result = NULL;
	char *buffer = (char *) Port::omrmem_malloc(EsMaxPath);
	if (NULL != buffer) {
		result = realpath(path, buffer);
	}
	return result;
}
#endif /* #if defined(WIN32) */

int
Port::strncasecmp(const char *s1, const char *s2, size_t n)
{
#if defined(WIN32)
	return ::_strnicmp(s1, s2, n);
#elif defined(J9ZOS390)
	return ::strncasecmp(s1, s2, (int) n);
#else /* defined(WIN32) */
	return ::strncasecmp(s1, s2, n);
#endif /* defined(WIN32) */
}
